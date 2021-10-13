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

// PPP Compression Control Protocol (CCP) Option Handling


//  Include Files

#include "windows.h"
#include "cclib.h"
#include "memory.h"
#include "cxport.h"

// VJ Compression Include Files

#include "ndis.h"
#include "tcpip.h"
#include "vjcomp.h"

// PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "ipcp.h"
#include "ccp.h"
#include "ncp.h"
#include "mac.h"
#include "ip_intf.h"
#include "ras.h"
#include "raserror.h"

#include "cclib.h"


//
//	Debug pretty print routine for CCP option data
//
void
ccpMSPPCOptionToStringCb(
	IN		                    PVOID       context,
	IN	OUT	                    POptionInfo pInfo,
	__in_bcount(cbOptData)		PBYTE       pOptData,
	IN		                    DWORD       cbOptData,
	IN  OUT                     PSTR       *ppBuffer)
{
	PSTR   pBuffer = *ppBuffer;
	DWORD  bits;

	PREFAST_ASSERT(cbOptData == 4);

	*pBuffer = '\0';

	BYTES_TO_DWORD(bits, pOptData);

	if (bits & MCCP_COMPRESSION)
		strcat(pBuffer, "Compression,");
	if (bits & MSTYPE_ENCRYPTION_40F)
		strcat(pBuffer, "40-bit-encryption,");
	if (bits & MSTYPE_ENCRYPTION_56)
		strcat(pBuffer, "56-bit-encryption,");
	if (bits & MSTYPE_ENCRYPTION_128)
		strcat(pBuffer, "128-bit-encryption,");
	if (bits & MSTYPE_HISTORYLESS)
		strcat(pBuffer, "Stateless,");
	if (bits & MSTYPE_ENCRYPTION_40)
		strcat(pBuffer, "40-bit-encryption(obsolete),");

	pBuffer += strlen(pBuffer);

	bits &= ~(MCCP_COMPRESSION|MSTYPE_ENCRYPTION_40F|MSTYPE_ENCRYPTION_56|MSTYPE_ENCRYPTION_128|MSTYPE_ENCRYPTION_40|MSTYPE_HISTORYLESS);
	if (bits)
		pBuffer += sprintf(pBuffer, "Unknown(%x)", bits);
	*ppBuffer = pBuffer;
}

void
ccpResetPeerOptionValuesCb(
	IN		PVOID  context)
//
//	This function is called whenever a new configure-request is
//	received from the peer prior to processing the options therein
//	contained. This function must reset all options to their default
//	values, such that any not explicity contained within the configure
//	request will use the default setting.
//
{
	PCCPContext pContext = (PCCPContext)context;

	DEBUGMSG(ZONE_CCP, (L"PPP: CCP Reset Peer options to 0 (received new CR from peer)\n"));
	pContext->peer.SupportedBits = 0;
}

void
ccpOptionValueReset(
	PCCPContext pContext)
//
//	Initialize the option states at the beginning of an
//	CCP negotiation. Note that the peer states are initialized
//  when we receive a CR by the function ccpResetPeerOptionValuesCb.
//
{
	pppSession_t    *pSession = pContext->session;
	RASPENTRY		*pRasEntry = &pSession->rasEntry;

	DEBUGMSG( ZONE_CCP, ( TEXT( "PPP: ccpOptionValueReset\n" )));

	pContext->local.SupportedBits = 0;

	if (pRasEntry->dwfOptions & RASEO_SwCompression)
		pContext->local.SupportedBits |= MCCP_COMPRESSION;

	if (pContext->bmEncryptionTypesSupported)
	{
		//
		//	If data encryption is required, then add encryption bits to those we put in our config-request.
		//  NOTE: RASEO_RequireDataEncryption is ignored unless RASEO_RequireMsEncryptedPw is also set.
		//
		if (NEED_ENCRYPTION(pRasEntry))
		{
		    pContext->local.SupportedBits |= pContext->bmEncryptionTypesSupported;	
		}
	}
}


/////////////	MSPPC option negotiation callbacks //////////////

DWORD
ccpBuildMSPPCOptionCb(
	IN		PVOID  context,
	IN	OUT	POptionInfo pInfo,
	OUT		PBYTE  pOptData,
	IN  OUT	PDWORD pcbOptData)
//
//	Called to fill in the SupportedBits field for the MPPC option
//	that is part of a configure-request we will send to the peer.
//
{
	PCCPContext  pContext = (PCCPContext)context;
	DWORD	     dwResult = NO_ERROR;

    PREFAST_ASSERT(*pcbOptData >= 4);

	// Cannot do compression without a scratch Rx buffer
	if (pContext->ScratchRxBuf == NULL)
		pContext->local.SupportedBits &= ~MCCP_COMPRESSION;

	DWORD_TO_BYTES(pOptData, pContext->local.SupportedBits);
	*pcbOptData = 4;

	return dwResult;
}

DWORD
ccpAckMSPPCOptionCb(
	IN		PVOID  context,
	IN	OUT	POptionInfo pInfo,
	IN		PBYTE  pOptData,
	IN		DWORD  cbOptData)
//
//	Called when the peer ACKs the compression option
//	that we sent in a CR.
//
{
	PCCPContext  pContext = (PCCPContext)context;
	DWORD	     dwResult = NO_ERROR;
	DWORD		 bits;

	//
	// The generic option handling code should have rejected any illegal sized option.
	//
	PREFAST_ASSERT(cbOptData == 4);

	BYTES_TO_DWORD(bits, pOptData);

	if ((bits & (MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128)) ==
				(MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128))
	{
		//
		// The ACK should contain only one encryption type
		//
		DEBUGMSG(ZONE_WARN, (TEXT("PPP: CCP-WARNING: Peer ACKed with BOTH 128 and 40 bit encryption\n")));
	
		// Let's just try 128 bit encryption
		pContext->local.SupportedBits = bits & ~MSTYPE_ENCRYPTION_40F;

		//
		// This will make the FSM treat the ACK was a NAK to generate a new CR.
		//
		dwResult = ERROR_UNRECOGNIZED_RESPONSE;
	}
	else
	{
		//
		// Nothing to do here. When CCP enters the opened state we will act on
		// the negotiated bit settings in pContext->local.SupportedBits.
		//
		ASSERT(bits == pContext->local.SupportedBits);
	}

	return dwResult;
}

DWORD
ccpNakMSPPCOptionCb(
	IN		PVOID  context,
	IN OUT	struct OptionInfo *pInfo,
	IN		PBYTE  pOptData,
	IN		DWORD  cbOptData)
//
//	Called when the peer NAKs a compression option
//	that we sent in a CR.
//
{
	PCCPContext  pContext = (PCCPContext)context;
	pppSession_t *pSession = pContext->session;
	RASPENTRY	 *pRasEntry = &pSession->rasEntry;
	DWORD	     dwResult = NO_ERROR;
    DWORD		 dwPeerWantedBits,
		         dwCompressionWantedBits,
				 dwEncryptionWantedBits;

	//
	// The generic option handling code should have rejected any illegal sized option.
	//
	PREFAST_ASSERT(cbOptData == 4);

	BYTES_TO_DWORD(dwPeerWantedBits, pOptData);
	dwCompressionWantedBits = dwPeerWantedBits & MCCP_COMPRESSION;
	dwEncryptionWantedBits  = dwPeerWantedBits & (MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128);

	//
	// If the peer wants compression then allow it unless compression is disabled
	// in our rasEntry.
	//
	if (dwCompressionWantedBits
	&&  (!(pRasEntry->dwfOptions & RASEO_SwCompression)))
	{
		DEBUGMSG(ZONE_NCP, (TEXT("CCP: Peer wants data compression but RASEO_SwCompression is off\n")));
		dwCompressionWantedBits = 0;
	}

	//
	// If we require data encryption, then we can't agree with a peer's
	// suggestion to not do encryption.
	//
	if (NEED_ENCRYPTION(pRasEntry)
	&&  ((dwEncryptionWantedBits & pContext->bmEncryptionTypesSupported) == 0))
	{
		// Modify the peer requested bits to pretend that an encryption type was requested.
		// This will give the peer another chance to accept encryption.

		DEBUGMSG(ZONE_WARN, (TEXT("PPP: CCP-WARNING: Peer NAKed with no encrypt but we need encrypt\n")));
		dwEncryptionWantedBits = pContext->bmEncryptionTypesSupported;
	}
	else if ((dwEncryptionWantedBits & (MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128)) ==
									   (MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128))
	{
		//
		// The NAK should contain only one encryption type
		//
		DEBUGMSG(ZONE_WARN, (TEXT("PPP: CCP-WARNING: Peer NAKed with both 128 and 40 bit encryption\n")));

		// Let's just try 128 bit encryption
		dwEncryptionWantedBits &= ~MSTYPE_ENCRYPTION_40F;
	}

	pContext->local.SupportedBits = dwCompressionWantedBits | dwEncryptionWantedBits;

	return dwResult;
}

DWORD
ccpRequestMSPPCOptionCb(
	IN	OUT	PVOID context,
	IN	OUT	POptionInfo pInfo,
		OUT	PBYTE		pCode,
	IN	OUT	PBYTE		*ppOptData,
	IN	OUT	PDWORD		pcbOptData)
//
//	Called when we receive a CCP configure request containing MPPC option,
//  or, when a CCP configure request from the peer fails to contain an option
//  that we need or want, in which case *ppOptData will be NULL.
//
{
	PCCPContext  pContext = (PCCPContext)context;
	pppSession_t *pSession = pContext->session;
	RASPENTRY    *pRasEntry = &pSession->rasEntry;
	DWORD	     dwResult = NO_ERROR;
	PBYTE		 pOptData = *ppOptData;
	DWORD		 cbOptData= *pcbOptData;
	DWORD        bmRequestBits,
		         bmResponseBits;

	bmRequestBits = 0;
	bmResponseBits = 0;
	if (pOptData == NULL)
	{
		//
		//	The peer did not send a compression option in its config-request,
		//  and we want to tell the peer the types that we want or require.
		//
		if (pRasEntry->dwfOptions & RASEO_SwCompression)
			bmResponseBits |= MCCP_COMPRESSION;

		if (pContext->bmEncryptionTypesSupported
		&&  NEED_ENCRYPTION(pRasEntry))
		{
		    bmResponseBits |= pContext->bmEncryptionTypesSupported;	
		}

		ASSERT(bmResponseBits != 0);
	}
	else
	{
		//
		// The generic option handling code should have rejected any illegal sized option.
		//
		ASSERT(cbOptData == 4);
		BYTES_TO_DWORD(bmRequestBits, pOptData);

		//
		//	If SW compression is allowed, then accept it if requested.
		//
		if(bmRequestBits & MCCP_COMPRESSION)
		{
			if (pRasEntry->dwfOptions & RASEO_SwCompression)
			{
				DEBUGMSG( ZONE_NCP | ZONE_FUNCTION, (TEXT("CCP: accept COMPRESSION REQUESTED BY PEER\n")));

				bmResponseBits |= MCCP_COMPRESSION;
			}
			else
			{
				DEBUGMSG( ZONE_NCP | ZONE_FUNCTION, (TEXT("CCP: deny COMPRESSION REQUESTED BY PEER\n")));

				// Just leave bmResponseBits unchanged
			}
		}

		// From IETF Draft on MPPE, section 2.1 Option Negotiation:
		//    "... the negotiation initiator SHOULD request all of the options
		//	   it supports.  The responder should NAK with a single encryption
		//	   option."
		//
		// Possible cases:
		//		1. Peer has set multiple encryption types (S and L)
		//			Response: NAK with strongest matching type that we support (S, L, OR none)
		//		2. Peer has set a single encryption type (S, L)
		//			Response: ACK requested type if supported, otherwise NAK with none
		//		3. Peer has requested no encryption
		//			Response: ACK no encryption
		//

		bmResponseBits |= (bmRequestBits & pContext->bmEncryptionTypesSupported);

		//
		//	If the peer offered both types of encryption, and we support both
		//	types, modify the response to only suggest a single (strongest) type.
		//
		if ((bmResponseBits & (MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128)) == 
			(MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128))
		{
			// Keep the 128 bit encryption, turn off the 40 bit.
			bmResponseBits &= ~MSTYPE_ENCRYPTION_40F;
		}

		//
		//	If RASEO_RequireDataEncryption has been set in the local RAS
		//	options, and our current response is for no encryption, then
		//	respond instead with the encryption types we support.
		//
		if (NEED_ENCRYPTION(pRasEntry)
		&&   ((bmResponseBits & (MSTYPE_ENCRYPTION_40F | MSTYPE_ENCRYPTION_128)) == 0) )
		{
			DEBUGMSG( ZONE_NCP, (TEXT( "CCP:RX MUST INSIST UPON ENCRYPTION!\n")));
			ASSERT(pContext->bmEncryptionTypesSupported != 0);
			bmResponseBits |= pContext->bmEncryptionTypesSupported;
		}

		//
		//	No support for stateless mode
		//
		if (bmRequestBits & MSTYPE_HISTORYLESS)
		{
			// No action required, just don't add it to response bits
		}
	}

	if( bmRequestBits == bmResponseBits )
    {
        // Packet OK - ACK it
		DEBUGMSG(ZONE_CCP, (L"PPP: CCP Acking RequestBits %x\n", bmRequestBits));
		pContext->peer.SupportedBits = bmResponseBits;
		*pCode = PPP_CONFIGURE_ACK;
	}
    else
    {
        // Packet NOT OK - NAK it with corrections

        DEBUGMSG( ZONE_CCP, (TEXT( "PPP: CCP NAK bits=%x, suggest=%x\n"), bmRequestBits, bmResponseBits) );

		DWORD_TO_BYTES(&pContext->abResponseBits[0], bmResponseBits);

		*pCode = PPP_CONFIGURE_NAK;
		*ppOptData = &pContext->abResponseBits[0];
		*pcbOptData = 4;
    }

	return dwResult;
}

OptionDescriptor ccpCompressionOptionDescriptor =
{
	CCP_OPT_MSC_CCP,	            // type == MS Compression/Encryption
	4,								// fixedLength = 4
	"MSC",                          // name
	ccpBuildMSPPCOptionCb,
	ccpAckMSPPCOptionCb,
	ccpNakMSPPCOptionCb,
	NULL,							// no special reject handling
	ccpRequestMSPPCOptionCb,
	ccpMSPPCOptionToStringCb		
};

DWORD
ccpOptionInit(
	PCCPContext pContext)
//
//	This function is called once during session initialization
//	to build the list of options that are supported.
//
{
	pppSession_t       *pSession = pContext->session;
	RASPENTRY		   *pRasEntry = &pSession->rasEntry;
	DWORD              dwResult;
	OptionRequireLevel orlMPPCELocal,
		               orlMPPCEPeer;

	if (pContext->bmEncryptionTypesSupported
	&&  NEED_ENCRYPTION(pRasEntry))
	{
		// Encryption is required
		orlMPPCELocal = ORL_Required;
		orlMPPCEPeer  = ORL_Required;
	}
	else if (pRasEntry->dwfOptions & RASEO_SwCompression)
	{
		// Compression is wanted
		orlMPPCELocal = ORL_Wanted;
		orlMPPCEPeer  = ORL_Wanted;
	}
	else if (pContext->bmEncryptionTypesSupported)
	{
		// Allow encryption if the peer requests it

		orlMPPCEPeer  = ORL_Allowed;

		//
		// Some servers appear to have a bug that they behave badly if we
		// send them an empty CCP configure-request option list.
		// So, we always send our MPPCE option even when we don't ourselves
		// want any encryption/compression. This allows the server to see
		// an MSCCP option with a value of 0x00000000 which they understand.
		//
		orlMPPCELocal = ORL_Wanted;
	}
	else
	{
		// Encryption isn't supported, Compression is not allowed
		orlMPPCELocal = ORL_Unsupported;
		orlMPPCEPeer  = ORL_Unsupported;
	}

	dwResult = PppFsmOptionsAdd(pContext->pFsm,
							&ccpCompressionOptionDescriptor,    orlMPPCELocal, orlMPPCEPeer,
							NULL);

	return dwResult;
}
