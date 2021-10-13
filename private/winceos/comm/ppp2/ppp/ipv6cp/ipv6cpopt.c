//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//


//  Include Files

#include "windows.h"
#include "cclib.h"
#include "memory.h"
#include "cxport.h"

#include "ndis.h"
#include "ndiswan.h"
#include "tcpip.h"
#include "vjcomp.h"

// PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "ncp.h"
#include "ccp.h"
#include "mac.h"
#include "ip_intf.h"
#include "ras.h"
#include "util.h"

#include "layerfsm.h"
#include "ipv6cp.h"

#include <fipsrandom.h>

static const BYTE g_IFIDZero[IPV6_IFID_LENGTH] = {0};

#define UNIVERSAL_LOCAL_BITNUM	1

//
// Utility macros for treating a byte array as an array of bits
//
#define BITARRAY_SET_BIT(array, bit) (array)[(bit) / 8] |=  (1 << ((bit) % 8)) 
#define BITARRAY_CLR_BIT(array, bit) (array)[(bit) / 8] &= ~(1 << ((bit) % 8))
#define BITARRAY_INV_BIT(array, bit) (array)[(bit) / 8] ^=  (1 << ((bit) % 8))

void
EUIToIFID(
	IN	PBYTE	pEUI,
	IN	DWORD	cbEUI,
	OUT	PBYTE	pIFID)
//
//	Convert a 48 or 64 bit IEEE EUI into an IPV6 IFID using the algorithm
//	specified in RFC2472.
//
{
	if (cbEUI == 8)
	{
		// The only transformation from an EUI-64 is to invert the "u" bit.
		memcpy(pIFID, pEUI, 8);
		BITARRAY_INV_BIT(pIFID, UNIVERSAL_LOCAL_BITNUM);
	}
	else if (cbEUI == 6)
	{
		//
		// EUI-48 is handled by inserting 0xFFFE into the middle to convert
		// it to an EUI-64.
		//
		pIFID[0] = pEUI[0];
		pIFID[1] = pEUI[1];
		pIFID[2] = pEUI[2];
		pIFID[3] = 0xFF;
		pIFID[4] = 0xFE;
		pIFID[5] = pEUI[3];
		pIFID[6] = pEUI[4];
		pIFID[7] = pEUI[5];
		BITARRAY_INV_BIT(pIFID, UNIVERSAL_LOCAL_BITNUM);
	}
	else
	{
		ASSERT(FALSE);
	}
}

BOOL
IFIDEqual(
	IN	const	PBYTE	pIFID1,
	IN	const	PBYTE	pIFID2)
//
//	Compare 2 IPV6 Interface Identifiers.
//	Return TRUE if they are equal, FALSE otherwise.
//
{
	return memcmp(pIFID1, pIFID2, IPV6_IFID_LENGTH) == 0;
}

BOOL
IFIDIsZero(
	IN	const	PBYTE	pIFID1)
//
//	Return TRUE if the IFID is all 0.
//
{
	return IFIDEqual(pIFID1, (const PBYTE)&g_IFIDZero[0]);
}

void
ipv6cpGetUniqueIFID(
	PIPV6Context pContext,
	OUT	PBYTE	 pIFID,
	IN	PBYTE	 pExistingLinkIFID OPTIONAL)
//
//	As per RFC2472 Section 4.1, try to obtain a unique interface identifier
//	to be used for the PPP connection.
//
//	1. If an EUI-48 or EUI-64 is available anywhere on the node, it is used
//	   to generate the tentative IFID.
//  2. Otherwise, a different source of uniqueness should be used such as
//	   a machine serial number.
//  3. Otherwise, a random number should be generated.
//	4. Otherwise, zero can be used which requests the peer to generate the
//     Unique IFID.
//
{
	BOOL	bFoundIFID = FALSE;
	DWORD   method = pContext->NextIFIDMethod;

	ASSERT(method <= METHOD_IFID_RANDOM);

	while (!bFoundIFID)
	{
		if ((method < METHOD_IFID_RANDOM)
		&&  (pContext->dwOptionFlags & (1 << method)))
		{
			// This method is disabled, proceed to next
			method++;
			continue;
		}

		if (method == METHOD_IFID_REGISTRY
		&&  pContext->bRegistryIFIDPresent)
		{
			//  Try the fixed registry setting
			memcpy(pIFID, &pContext->RegistryIFID[0], IPV6_IFID_LENGTH);
		}
		if (method == METHOD_IFID_PERSISTENT_RANDOM
		&&  pContext->bRegistryRandomIFIDPresent)
		{
			//  Use the random number generated in an earlier negotiation
			memcpy(pIFID, &pContext->RegistryRandomIFID[0], IPV6_IFID_LENGTH);
		}
		else if (method == METHOD_IFID_EUI
		     &&  pContext->cbEUI)
		{
			//	Try to produce a unique IFID on the link from an EUI-48 or 64 on the node
			EUIToIFID(&pContext->EUI[0], pContext->cbEUI, pIFID);
		}
		else if (method == METHOD_IFID_DEVICE_ID
		     &&  pContext->cbDeviceID)
		{
			//  Try to produce a unique IFID on the link from Device ID info
			ASSERT(pContext->cbDeviceID == IPV6_IFID_LENGTH);
			memcpy(pIFID, &pContext->DeviceID[0], IPV6_IFID_LENGTH);
			BITARRAY_CLR_BIT(pIFID, UNIVERSAL_LOCAL_BITNUM);
		}
		else if (method == METHOD_IFID_RANDOM)
		{
			// Generate a random number for the IFID
			FipsGenRandom(IPV6_IFID_LENGTH, pIFID);

			// The "u" bit of the IFID MUST be set to 0.
			BITARRAY_CLR_BIT(pIFID, UNIVERSAL_LOCAL_BITNUM);
		}
		else
		{
			// Current method not available, on to the next
			method++;
			continue;
		}

		if ((pExistingLinkIFID && IFIDEqual(pIFID, pExistingLinkIFID))
		||  IFIDIsZero(pIFID))
		{
			// The EUI we just generated is identical to another one
			// on the link, or else is 0, so cannot be used.
			// Try the next method (or just keep doing random numbers)
			if (method < METHOD_IFID_RANDOM)
				method++;
		}
		else
		{
			bFoundIFID = TRUE;
		}
	}

	if (method == METHOD_IFID_RANDOM
	&&  pExistingLinkIFID == NULL
	&&  !(pContext->dwOptionFlags & OPTION_DISABLE_PERSISTENT_RANDOM)
	&&  !pContext->bRegistryIFIDPresent)
	{
		// Save the newly generated random IFID in the registry so that it will
		// be used on subsequent connections.
		(void)WriteRegistryValues(HKEY_LOCAL_MACHINE, TEXT("Comm\\ppp\\Parms"),
										RAS_VALUENAME_IPV6_RANDOM_IFID, REG_BINARY, 0, pIFID, IPV6_IFID_LENGTH,
										NULL);
	}

	//
	// To prevent looping, each method (other than random) can only be used once
	// per negotiation.
	//
	if (method < METHOD_IFID_RANDOM)
		method++;
	pContext->NextIFIDMethod = method;
}

void
ipv6cpOptionValueReset(
	PIPV6Context pContext)
//
//	Initialize the option states at the beginning of an
//	IPV6CP negotiation.
//
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT( "pppIpv6cp_OptValueInit\n" )));

	// We want to negotiate a local interface-identifier

	pContext->NextIFIDMethod = 0;

	// Set the initial value for the local interface-identifier
	ipv6cpGetUniqueIFID(pContext, &pContext->LocalInterfaceIdentifier[0], NULL);

	// We allow the peer to negotiate its interface-identifier
	// Set it to 0 prior to the peer assigning it a value.
	memset(pContext->PeerInterfaceIdentifier, 0, 8);

}

DWORD
ipv6cpRequestIFIDCb(
	IN	OUT	PVOID context,
	IN	OUT	POptionInfo pInfo,
		OUT	PBYTE		pCode,
	IN	OUT	PBYTE		*ppOptData,
	IN	OUT	PDWORD		pcbOptData)
//
//	Called when we receive an IPV6CP configure request containing
//	the Interface-Identifier option.  The peer is telling us what
//	its Interface-Identifier (the lower 64 bits of the 128 bit IPv6
//	address) is.
//
{
	PIPV6Context pContext = (PIPV6Context)context;
	DWORD	     dwResult = NO_ERROR;
	PBYTE		 ifidPeerRequested = *ppOptData;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "ipv6cpRequestInterfaceIdentifierCB\n" )));

	// Length checking of the option should have been done by Option module.
	ASSERT(!ifidPeerRequested || *pcbOptData == IPV6_IFID_LENGTH);

    if (ifidPeerRequested == NULL)
	{
		//
		// The peer did not send this option in a configure request, and
		// we require or want him to send it.  We need to specify the value
		// to NAK with.  We will NAK with a zero IFID to suggest that the peer
		// send this in its next configure-request.
		//
		*ppOptData = (PBYTE)&g_IFIDZero[0];
		*pcbOptData = IPV6_IFID_LENGTH;
	}
	else // Peer sent a config-req with an interface identfier value
	{
		if (IFIDIsZero(ifidPeerRequested))
		{
			//
			//	The Interface-Identifier is 0, the peer is requesting that
			//	we generate a unique Interface-Identifier and NAK with that value.
			//

			if (IFIDIsZero(pContext->LocalInterfaceIdentifier) == 0)
			{
				//
				//	RFC 2472 Section 4.1
				//	"If the two Interface-Identifiers are equal to zero, the Interface-
				//	Identifiers negotiation MUST be terminated by transmitting the Configure-
				//	Reject with the Interface-Identifier value set to 0. In this case
				//	a unique Interface-Identifier can not be negotiated."
				//
				*pCode = PPP_CONFIGURE_REJ;
			}
			else
			{
				//
				// RFC 2472 Section 4.1
				// "If the two Interface-Identifiers are different but the received
				// Identifier is zero, a Configure-Nak is sent with a non-zero Interface-
				// Identifier value suggested for use by the remote peer. Such a suggested
				// Interface-Identifier MUST be different from the Interface-Identifier of
				// the last configure request sent to the peer."
				//
				// Generate a Interface-Identifier for the peer to use.
				//
				ipv6cpGetUniqueIFID(pContext, pContext->IFIDSuggestedToPeer, pContext->LocalInterfaceIdentifier);
				*ppOptData = pContext->IFIDSuggestedToPeer;
				*pCode = PPP_CONFIGURE_NAK;
			}
		}
		else // Peer wants to use a non-zero identifier
		{
			if (IFIDEqual(ifidPeerRequested, pContext->LocalInterfaceIdentifier))
			{
				//
				//	RFC 2472 Section 4.1
				//  "If the two Interface-Identifiers are equal and are not zero,
				//  a Configure-Nak MUST be sent specifying a different non-zero
				//  Interface-Identifier value."
				//
				ipv6cpGetUniqueIFID(pContext, pContext->IFIDSuggestedToPeer, pContext->LocalInterfaceIdentifier);
				*ppOptData = pContext->IFIDSuggestedToPeer;
				*pCode = PPP_CONFIGURE_NAK;
			}
			else
			{
				//
				//	RFC 2472 Section 4.1
				//  "If the two Interface-Identifiers are different and the received
				//  Interface-Identifier is not zero, the Interface-Identifier MUST
				//  be acknowledged."
				//
				memcpy(pContext->PeerInterfaceIdentifier, ifidPeerRequested, IPV6_IFID_LENGTH);
				*pCode = PPP_CONFIGURE_ACK;
			}
		}
	}

	return dwResult;
}

DWORD
ipv6cpBuildIFIDOptionCb(
	IN		PVOID  context,
	IN	OUT	POptionInfo pInfo,
	OUT		__out_ecount(IPV6_IFID_LENGTH) PBYTE  pOptData,
	IN  OUT	PDWORD pcbOptData
	)
//
//	Called to fill in the data for an Interface-Identifier option
//	that is part of a configure-request we will send to the peer.
//
{
	PIPV6Context pContext = (PIPV6Context)context;
	DWORD	     dwResult = NO_ERROR;

    PREFAST_ASSERT(*pcbOptData >= IPV6_IFID_LENGTH);

	memcpy(pOptData, pContext->LocalInterfaceIdentifier, IPV6_IFID_LENGTH);
	*pcbOptData = IPV6_IFID_LENGTH;

	return dwResult;
}

DWORD
ipv6cpAckIFIDOptionCb(
	IN		PVOID  context,
	IN	OUT	POptionInfo pInfo,
	IN		PBYTE  pOptData,
	IN		DWORD  cbOptData
	)
//
//	Called when the peer ACKs the Interface-Identifier option
//	that we sent in a CR.  Since we both now agree on the IFID,
//	we can commit to using it.
//
{
	PIPV6Context pContext = (PIPV6Context)context;
	DWORD	     dwResult = NO_ERROR;

	return dwResult;
}

DWORD
ipv6cpNakIFIDOptionCb(
	IN		PVOID  context,
	IN OUT	struct OptionInfo *pInfo,
	IN		__out_ecount(IPV6_IFID_LENGTH) PBYTE  pOptData,
	IN		DWORD  cbOptData
	)
//
//	Called when the peer sends us a configure-nak for an Interface-Identifer option
//	to suggest an Interface-Identifier that we should use.
//
{
	PIPV6Context pContext = (PIPV6Context)context;
	DWORD		 dwResult = NO_ERROR;

	// Length should have been checked by the generic Option code already.
	PREFAST_ASSERT(cbOptData == IPV6_IFID_LENGTH);

	if (!IFIDIsZero(pOptData))
	{
		// Use the IFID suggested by the peer
		memcpy(&pContext->LocalInterfaceIdentifier[0], pOptData, IPV6_IFID_LENGTH);
	}
	else
	{
		DEBUGMSG(ZONE_WARN, (TEXT("PPP: IPv6CP - WARNING - Peer NAK IFID option with IFID=0\n")));
	}

	return dwResult;
}

OptionDescriptor ipv6cpIFIDOptionDescriptor =
{
	IPV6CP_OPT_INTERFACE_IDENTIFIER,	// type == Interface-Identifier
	IPV6_IFID_LENGTH,					// fixedLength
	"IFID",								// name
	ipv6cpBuildIFIDOptionCb,
	ipv6cpAckIFIDOptionCb,
	ipv6cpNakIFIDOptionCb,
	NULL,								// no special reject handling
	ipv6cpRequestIFIDCb,
	NULL								// use default debug hex output
};

DWORD
ipv6cpOptionInit(
	PIPV6Context pContext)
{
	DWORD dwResult;

	dwResult = PppFsmOptionsAdd(pContext->pFsm,
							&ipv6cpIFIDOptionDescriptor,ORL_Wanted, ORL_Wanted,
							NULL);

	return dwResult;
}