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
/*****************************************************************************
* 
*
*   @doc
*   @module ipv6cp.c | PPP Internet Protocol Version 6 Control Protocol (IPV6CP)
*
*/

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
#include "auth.h"
#include "ipcp.h"
#include "ncp.h"
#include "ccp.h"
#include "mac.h"
#include "ip_intf.h"
#include "ras.h"
#include "util.h"

#include "layerfsm.h"
#include "ipv6cp.h"

extern BOOL DumpIPv6 (BOOL Direction, PBYTE Buffer, DWORD Len);

static  DWORD
ipv6cpUp(
	PVOID context)
//
//	This is called when the FSM enters the Opened state
//
{
	PIPV6Context	pContext = (PIPV6Context)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->pSession);

    DEBUGMSG( ZONE_IPV6CP, ( TEXT( "PPP: IPV6CP UP\n" )));

	// Indicate connected to RAS (unless still waiting on encryption)
	pppNcp_IndicateConnected(pSession->ncpCntxt);

	return NO_ERROR;
}

static  DWORD
ipv6cpDown(
	PVOID context)
//
//	This is called when the FSM leaves the Opened state
//
{
 	PIPV6Context	pContext = (PIPV6Context)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->pSession);

    DEBUGMSG( ZONE_IPV6CP, ( TEXT( "PPP: IPV6CP DOWN\n" )));

	PPPVEMIPV6InterfaceDown(pSession);

	pContext->NextIFIDMethod = 0;

	return NO_ERROR;
}

static  DWORD
ipv6cpStarted(
	PVOID context)
//
//	This is called when the FSM enters the Opened state
//
{
	PIPV6Context	pContext = (PIPV6Context)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->pSession);

    DEBUGMSG( ZONE_IPV6CP, ( TEXT( "PPP: IPV6CP STARTED\n" )));

	ipv6cpOptionValueReset(pContext);

	return NO_ERROR;
}

static  DWORD
ipv6cpFinished(
	PVOID context)
{
	PIPV6Context	pContext = (PIPV6Context)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->pSession);

	if (FALSE == pSession->HangUpReq)
	{
		// Indicate to NCP that IPV6CP is not connected, allowing NCP to reevaulate
		// connection status.
		pppNcp_IndicateConnected(pSession->ncpCntxt);
	}

	return NO_ERROR;
}

static DWORD
ipv6cpSendPacket(
	PVOID context,
	USHORT ProtocolWord,
	PBYTE	pData,
	DWORD	cbData)
{
	PIPV6Context	pContext = (PIPV6Context)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->pSession);
	DWORD			dwResult;

	dwResult = pppSendData(pSession, ProtocolWord, pData, cbData);

	return dwResult;
}

void
ipv6cpResetPeerOptionsToDefaultSettings(
	PVOID context)
//
//	This function is called whenever a new configure-request is
//	received from the peer prior to processing the options therein
//	contained. This function must reset all options to their default
//	values, such that any not explicity contained within the configure
//	request will use the default setting.
//
{
	PIPV6Context	pContext = (PIPV6Context)context;

	// Set Peer IFID to 0 prior to the peer assigning it a value.
	memset(pContext->PeerInterfaceIdentifier, 0, 8);
}

DWORD
ipv6cpSessionLock(
	PVOID context)
//
//	This is called by the FSM when it needs to lock the session
//	because a timer has expired.
//
{
	PIPV6Context	pContext = (PIPV6Context)context;

	pppLock(pContext->pSession);

	return NO_ERROR;
}

DWORD
ipv6cpSessionUnlock(
	PVOID context)
//
//	This is called by the FSM when it needs to unlock the session
//	after a prior call to lock it due to timer expiry.
//
{
	PIPV6Context	pContext = (PIPV6Context)context;

	pppUnLock(pContext->pSession);

	return NO_ERROR;
}

static PppFsmDescriptor ipv6cpFsmData =
{
	"IPV6CP",             // szProtocolName
	PPP_PROTOCOL_IPV6CP,  // ProtocolWord
	32,					  // cbMaxTxPacket
	ipv6cpUp,
	ipv6cpDown,
	ipv6cpStarted,
	ipv6cpFinished,
	ipv6cpSendPacket,
	ipv6cpSessionLock,
	ipv6cpSessionUnlock,
	NULL				  // No extension message types for IPCP
};


PROTOCOL_DESCRIPTOR pppIpv6cpProtocolDescriptor =
{
	pppIpv6cpRcvData,
	pppIpv6cp_Rejected,
	Ipv6cpOpen,
	Ipv6cpClose,
	Ipv6cpRenegotiate
};

PROTOCOL_DESCRIPTOR pppIPV6ProtocolDescriptor =
{
	pppIpv6ReceiveIPV6,
	NULL
};

/*****************************************************************************
* 
*   @func   DWORD | pppIpv6cp_InstanceCreate | Creates IPV6CP instance.
*
*   @parm   void * | SessionContext | Session context pointer.
*   @parm   void ** | ReturnedContext | Returned context pointer.
*
*   @rdesc  Returns error code, return of 0 indicates success.
*   
*   @comm   This function creates an instance of the IPV6CP layer and returns
*           a pointer to it. This pointer is used for all subsequent calls 
*           to this context.
*/

DWORD
pppIpv6cp_InstanceCreate(
	IN	PVOID	session,
	OUT	PVOID	*ReturnedContext)
{
	pppSession_t    *pSession = (pppSession_t *)session;
	PIPV6Context	pContext;
	DWORD			dwResult = NO_ERROR;
	DWORD           nRead;
	DWORD           cbRegistryIFID;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "pppIpv6cp_InstanceCreate\r\n" )));

	do
	{
		pContext = (PIPV6Context)pppAlloc(pSession, sizeof(*pContext));
		if (pContext == NULL)
		{
			dwResult = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		// Register IPV6CP context and protocol decriptor with session

		dwResult = PPPSessionRegisterProtocol(pSession, PPP_PROTOCOL_IPV6CP, &pppIpv6cpProtocolDescriptor, pContext);
		if (dwResult != NO_ERROR)
			break;

		dwResult = PPPSessionRegisterProtocol(pSession, PPP_PROTOCOL_IPV6, &pppIPV6ProtocolDescriptor, pContext);
		if (dwResult != NO_ERROR)
			break;

		// Initialize context

		pContext->pSession = session;

		// Create Fsm

		pContext->pFsm = PppFsmNew(&ipv6cpFsmData, ipv6cpResetPeerOptionsToDefaultSettings, pContext);
		if (pContext->pFsm == NULL)
		{
			dwResult = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		nRead = ReadRegistryValues(HKEY_LOCAL_MACHINE, TEXT("Comm\\ppp\\Parms"),
										RAS_VALUENAME_IPV6_FLAGS, REG_DWORD, 0,  &pContext->dwOptionFlags,   sizeof(DWORD),
										NULL);
		//
		// Read in registry IFID if present
		//
		cbRegistryIFID = IPV6_IFID_LENGTH;
		nRead = ReadRegistryValues(HKEY_LOCAL_MACHINE, TEXT("Comm\\ppp\\Parms"),
									RAS_VALUENAME_IPV6_IFID,  REG_BINARY, 0, &pContext->RegistryIFID[0], &cbRegistryIFID,
									NULL);
		pContext->bRegistryIFIDPresent = nRead == 1 && cbRegistryIFID == IPV6_IFID_LENGTH;

		cbRegistryIFID = IPV6_IFID_LENGTH;
		nRead = ReadRegistryValues(HKEY_LOCAL_MACHINE, TEXT("Comm\\ppp\\Parms"),
									RAS_VALUENAME_IPV6_RANDOM_IFID,  REG_BINARY, 0, &pContext->RegistryRandomIFID[0], &cbRegistryIFID,
									NULL);
		pContext->bRegistryRandomIFIDPresent = nRead == 1 && cbRegistryIFID == IPV6_IFID_LENGTH;

		// Get the EUI of an adapter during initialization, rather than during
		// option negotiation, to avoid problems with deadlock due to calls to
		// the IP Helpers calling back into NDIS miniports.
		if (utilGetEUI(0, &pContext->EUI[0], &pContext->cbEUI) == FALSE)
			pContext->cbEUI = 0;

		// Get the machine unique ID if available
		pContext->cbDeviceID = sizeof(pContext->DeviceID);
		utilGetDeviceIDHash(&pContext->DeviceID[0], &pContext->cbDeviceID);

		// Configure option values

		ipv6cpOptionInit(pContext);

		// Put the FSM into the opened state, ready to go when the lower layer comes up

		PppFsmOpen(pContext->pFsm);

	} while (FALSE);

	if (dwResult != NO_ERROR)
	{
		pppFree(pSession, pContext);
		pContext = NULL;
	}

	*ReturnedContext = pContext;
    return dwResult;
}

DWORD
Ipv6cpOpen(
	IN	PVOID	context )
//
//	Called to open the IPV6CP layer.
//
{
	PIPV6Context	pContext = (PIPV6Context)context;
			
	return PppFsmOpen(pContext->pFsm);
}

DWORD
Ipv6cpClose(
	IN	PVOID	context )
//
//	Called to initiate the closure of the IPV6CP layer.
//
{
	PIPV6Context	pContext = (PIPV6Context)context;
			
	return PppFsmClose(pContext->pFsm);
}

DWORD
Ipv6cpRenegotiate(
	IN	PVOID	context )
//
//	Called to renegotiate IPV6CP layer parameters with the peer.
//
{
	PIPV6Context	pContext = (PIPV6Context)context;

	return PppFsmRenegotiate(pContext->pFsm);
}

void
pppIpv6cp_Rejected(
	IN	PVOID	context )
//
//	Called when the peer rejects the IPV6CP protocol
//
{
	PIPV6Context	pContext = (PIPV6Context)context;
			
	PppFsmProtocolRejected(pContext->pFsm);
}
/*****************************************************************************
* 
*   @func   void | pppIpv6cp_InstanceDelete | Deletes ipv6cp instance.
*
*   @parm   void * | context | Instance context pointer.
*
*   @comm   This function deletes the passed in context.
*/

void
pppIpv6cp_InstanceDelete(
	IN	PVOID	context )
{
	PIPV6Context	pContext = (PIPV6Context)context;

    DEBUGMSG( ZONE_IPV6CP | ZONE_FUNCTION, (TEXT("pppIpv6cp_InstanceDelete\r\n" )));

	if (context)
	{
		PppFsmDelete(pContext->pFsm);
		pppFree( pContext->pSession, pContext);
	}
}

void
pppIpv6cp_LowerLayerUp(
	IN	PVOID	context)
//
//	This function will be called when the auth layer is up
//
{
	PIPV6Context	pContext = (PIPV6Context)context;

	PppFsmLowerLayerUp(pContext->pFsm);
}

void
pppIpv6cp_LowerLayerDown(
	IN	PVOID	context)
//
//	This function will be called when the auth layer is down
//
{
	PIPV6Context	pContext = (PIPV6Context)context;

	PppFsmLowerLayerDown(pContext->pFsm);
}

void
pppIpv6cpRcvData(
	PVOID       context,
	pppMsg_t    *pMsg )
//
//  Process received IPV6CP Control Messages
//
{
	PIPV6Context	pContext = (PIPV6Context)context;

	ASSERT(PPP_PROTOCOL_IPV6CP == pMsg->ProtocolType);

	// If the layer below IPV6CP (usually auth) has not indicated that it is up yet,
	// then receiving a IPV6CP packet means that we probably lost an
	// authentication success packet from the peer. So, for some
	// authentication protocols the reception of a network layer
	// packet (e.g. CCP, IPV6CP) is implicit authentication success.
	//
	if (pContext->pFsm->state <= PFS_Starting)
	{
		AuthRxImpliedSuccessPacket(pContext->pSession->authCntxt);
	}

	PppFsmProcessRxPacket(pContext->pFsm, pMsg->data, pMsg->len);
}

void
pppIpv6ReceiveIPV6(
	IN  PIPV6Context pContext,
	IN  pppMsg_t    *pMsg )
//
//	Process a received IPv6 packet.
//  This function is called when we receive a PPP packet with the protocol type 0x0057
//
{
	pppSession_t	*pSession = pContext->pSession;

#ifdef DEBUG
	if (ZONE_IPHEADER)
		DumpIPv6(FALSE, pMsg->data, pMsg->len);
#endif

	if (pContext->pFsm->state == PFS_Opened)
    {
		PPPSessionUpdateRxPacketStats(pSession, pMsg);
		// Send data up to IPV6
		PPPVEMIPvXIndicateReceivePacket(pSession, pMsg, EthPacketTypeIPv6);
    }
}

NDIS_STATUS
pppIpv6cpSendData(
	IN	PVOID         context,
	IN	PNDIS_WAN_PACKET  pWanPacket)
//
//	This function is called to send an IPv6 packet.
//
{
	PIPV6Context	pContext = (PIPV6Context)context;
	pppSession_t	*pSession = pContext->pSession;
	ncpCntxt_t     *ncp_p = (ncpCntxt_t *)pSession->ncpCntxt;
	USHORT			wProtocol;
	void			*pMac = pSession->macCntxt;
	NDIS_STATUS		Status = NDIS_STATUS_FAILURE;
	DWORD			dwFlatLen;

	DEBUGMSG(ZONE_FUNCTION, (L"PPP: pppIpv6cpSendData\n"));
    ASSERT( pWanPacket );

	do
	{
		// Protocol State MUST be OPEN to accept messages

		if (pContext->pFsm->state != PFS_Opened)
		{
			DEBUGMSG(ZONE_IPV6CP, (TEXT("PPP: IPv6 unable to send packet in state %u\n"), pContext->pFsm->state));
			break;
		}

		dwFlatLen = pWanPacket->CurrentLength;	// Save for later
		pSession->Stats.BytesSent += pWanPacket->CurrentLength;
		pSession->Stats.FramesSent++;

		wProtocol = PPP_PROTOCOL_IPV6;

#ifdef DEBUG
		if (dpCurSettings.ulZoneMask & 0x80000000)
			DumpIPv6(TRUE, pWanPacket->CurrentBuffer, pWanPacket->CurrentLength);
#endif
		// CCP Datagram Compression
		//
		// If CCP is enabled pass the packet to the CCP protocol for compression. 
		// If this option has been negotiated the pppData structure will be 
		// suitably modified to pass to PPP for transmission.
        
		if( ncp_p->protocol[NCP_CCP].enabled )
		{
			// Pass in a pointer to the current Wan Packet.
			// The compression code might replace our packet if it can compress
			// the data
			if (!pppCcp_Compress( pSession, &pWanPacket, &wProtocol))
			{
				// Encryption is required and CCP is not open, return with error.
				break;
			}
		}

		// If either compression enabled then keep track of compression stats.
		pSession->Stats.BytesTransmittedUncompressed += dwFlatLen;
		pSession->Stats.BytesTransmittedCompressed += pWanPacket->CurrentLength;
		
		// Send down to the miniport.
		Status = pppMacSndData (pMac, wProtocol, pWanPacket);
	} while (FALSE);

    return Status;
}
