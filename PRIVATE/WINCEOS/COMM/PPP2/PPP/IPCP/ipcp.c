//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

// PPP Internet Protocol Control Protocol (IPCP)

//  Include Files

#include "windows.h"
#include "cclib.h"
#include "memory.h"
#include "cxport.h"

// VJ Compression Include Files

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

#include "pppserver.h"

//
//	Activesync 3.1 seems to send frames that are larger than the MRU.
//
#define ACTIVESYNC31_EXTRA_BYTES		64
#define MAX_PROTOCOL_FIELD_LENGTH		2

void
pppIpcp_LowerLayerUp(
	IN	PVOID	context)
//
//	This function will be called when the auth layer is up
//
{
	HANDLE hThread;
	DWORD  dwThreadId;

	PIPCPContext	pContext = (PIPCPContext)context;

	if (pContext->session->bIsServer)
	{
		// Update our name server addresses prior to starting
		// IPCP negotiation
		hThread = CreateThread(NULL, 0, PPPServerGetNameServerAddresses, NULL, 0, &dwThreadId);
		if (hThread)
			CloseHandle(hThread);
	}

	PppFsmLowerLayerUp(pContext->pFsm);
}

void
pppIpcp_LowerLayerDown(
	IN	PVOID	context)
//
//	This function will be called when the auth layer is down
//
{
	PIPCPContext	pContext = (PIPCPContext)context;

	PppFsmLowerLayerDown(pContext->pFsm);
}


static DWORD
ipcpSendPacket(
	PVOID context,
	USHORT ProtocolWord,
	PBYTE	pData,
	DWORD	cbData)
{
	PIPCPContext	pContext = (PIPCPContext)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->session);
	DWORD			dwResult;

	dwResult = pppSendData(pSession, ProtocolWord, pData, cbData);

	return dwResult;
}


static  DWORD
ipcpUp(
	PVOID context)
//
//	This is called when the FSM enters the Opened state
//
{
	PIPCPContext	pContext = (PIPCPContext)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->session);
	USHORT          MRU;
	DWORD           cbMRU,
		            cbMaxRxBuf;

    if (pContext->local.VJCompressionEnabled || pContext->peer.VJCompressionEnabled )
    {
		//
		// Allocate a new VJ decompression buffer if the MRU size has changed.
		//
		cbMRU = sizeof(MRU);
		pppLcp_QueryParameter(pSession->lcpCntxt, TRUE, LCP_OPT_MRU, &MRU, &cbMRU);
		cbMaxRxBuf = MAX_HDR + MAX_PROTOCOL_FIELD_LENGTH + MRU + ACTIVESYNC31_EXTRA_BYTES;
		if (cbMaxRxBuf != pContext->cbVJRxBuf)
		{
			DEBUGMSG(ZONE_IPCP, (TEXT("PPP: Allocating VJ RXBUF %d bytes\n"), cbMaxRxBuf));
			pppFree(pSession, pContext->pVJRxBuf, TEXT("VJ RXBUF"));
			pContext->cbVJRxBuf = cbMaxRxBuf;
			pContext->pVJRxBuf = pppAlloc(pSession, cbMaxRxBuf, TEXT("VJ RXBUF"));
			if (pContext->pVJRxBuf == NULL)
			{
				// Disable decompresssion
				pContext->local.VJCompressionEnabled = FALSE;
				pContext->cbVJRxBuf = 0;
			}
		}

        sl_compress_init(
			&pContext->vjcomp,
			pContext->local.MaxSlotId + 1,
			pContext->peer.MaxSlotId + 1,
			pContext->local.CompSlotId,
			pContext->peer.CompSlotId && pContext->VJEnableSlotIdCompressionTx); // only enable if both we AND peer want it
    }

	// Bring up the link

	DEBUGMSG( ZONE_IPCP, ( TEXT( "PPP: IPCP - bringing up the interface\n" )));

	// Link will  be registered with TCP/IP after DHCP completes.
	PppDhcpStart(pSession);

	return NO_ERROR;
}

static  DWORD
ipcpDown(
	PVOID context)
//
//	This is called when the FSM leaves the Opened state
//
{
	PIPCPContext	pContext = (PIPCPContext)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->session);

	PppDhcpStop(pSession);

    // Take the TCP/IP interface down
    LinkDownIndication(pSession->context, pSession);

	return NO_ERROR;
}

static  DWORD
ipcpStarted(
	PVOID context)
{
	PIPCPContext	pContext = (PIPCPContext)context;

    DEBUGMSG( ZONE_IPCP, ( TEXT( "PPP: IPCP STARTED\n" )));

	ipcpOptionValueReset(pContext);

	return NO_ERROR;
}

static  DWORD
ipcpFinished(
	PVOID context)
{
	PIPCPContext	pContext = (PIPCPContext)context;

	return NO_ERROR;
}

DWORD
ipcpSessionLock(
	PVOID context)
//
//	This is called by the FSM when it needs to lock the session
//	because a timer has expired.
//
{
	PIPCPContext	pContext = (PIPCPContext)context;

	pppLock(pContext->session);

	return NO_ERROR;
}

DWORD
ipcpSessionUnlock(
	PVOID context)
//
//	This is called by the FSM when it needs to unlock the session
//	after a prior call to lock it due to timer expiry.
//
{
	PIPCPContext	pContext = (PIPCPContext)context;

	pppUnLock(pContext->session);

	return NO_ERROR;
}

//
//	cbMaxTxPacket derivation for IPCP:
//		4 bytes for standard header (type, id, 2 octet len)
//		6 bytes for IP-compression option (2 header + 4 option data)
//		6 bytes for IP-address option
//		24 bytes for 4 name server IP address options
//
static PppFsmDescriptor ipcpFsmData =
{
	"IPCP",             // szProtocolName
	PPP_PROTOCOL_IPCP,  // ProtocolWord
	64,					// cbMaxTxPacket
	ipcpUp,
	ipcpDown,
	ipcpStarted,
	ipcpFinished,
	ipcpSendPacket,
	ipcpSessionLock,
	ipcpSessionUnlock,
	NULL				// No extension message types for IPCP
};

DWORD
IpcpOpen(
	IN	PVOID	context )
//
//	Called to open IPCP layer.
//
{
	PIPCPContext	pContext = (PIPCPContext)context;

	return PppFsmOpen(pContext->pFsm);
}

DWORD
IpcpClose(
	IN	PVOID	context )
//
//	Called to initiate the closure of the IPCP layer.
//
{
	PIPCPContext	pContext = (PIPCPContext)context;
			
	return PppFsmClose(pContext->pFsm);
}

DWORD
IpcpRenegotiate(
	IN	PVOID	context )
//
//	Called to renegotiate IPCP layer parameters with the peer.
//
{
	PIPCPContext	pContext = (PIPCPContext)context;
			
	return PppFsmRenegotiate(pContext->pFsm);
}

void
pppIpcp_Rejected(
	IN	PVOID	context )
//
//	Called when the peer rejects the IPCP protocol.
//
{
	PIPCPContext	pContext = (PIPCPContext)context;
			
	PppFsmProtocolRejected(pContext->pFsm);
}

PROTOCOL_DESCRIPTOR pppIpcpProtocolDescriptor =
{
	pppIpcpRcvData,
	pppIpcp_Rejected,
	IpcpOpen,
	IpcpClose,
	IpcpRenegotiate
};

PROTOCOL_DESCRIPTOR pppIPProtocolDescriptor =
{
	PppIPV4ReceiveIP,
	NULL
};

PROTOCOL_DESCRIPTOR pppVJIPProtocolDescriptor =
{
	PppIPV4ReceiveVJIP,
	NULL
};

PROTOCOL_DESCRIPTOR pppVJUIPProtocolDescriptor =
{
	PppIPV4ReceiveVJUIP,
	NULL
};

DWORD
pppIpcp_InstanceCreate(
	IN	PVOID session,
	OUT	PVOID *ReturnedContext)
//
//	Called during session creation to allocate and initialize the IPCP protocol
//	context.
//
{
	pppSession_t    *pSession = (pppSession_t *)session;
	PIPCPContext    pContext;
	DWORD			dwResult = NO_ERROR;

    DEBUGMSG( ZONE_IPCP | ZONE_FUNCTION, (TEXT( "pppIpcp_InstanceCreate\r\n" )));

	do
	{
		pContext = (PIPCPContext)pppAlloc(pSession,  sizeof(*pContext), TEXT( "IPCP CONTEXT" ) );
		if( pContext == NULL )
		{
			DEBUGMSG( ZONE_ERROR, (TEXT( "PPP: ERROR: NO MEMORY for IPCP context\r\n" )) );
			dwResult = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		// Register IPCP context and protocol decriptor with session

		dwResult = PPPSessionRegisterProtocol(pSession, PPP_PROTOCOL_IPCP, &pppIpcpProtocolDescriptor, pContext);
		if (dwResult != NO_ERROR)
			break;

		dwResult = PPPSessionRegisterProtocol(pSession, PPP_PROTOCOL_IP, &pppIPProtocolDescriptor, pContext);
		if (dwResult != NO_ERROR)
			break;

		dwResult = PPPSessionRegisterProtocol(pSession, PPP_PROTOCOL_COMPRESSED_TCP, &pppVJIPProtocolDescriptor, pContext);
		if (dwResult != NO_ERROR)
			break;

		dwResult = PPPSessionRegisterProtocol(pSession, PPP_PROTOCOL_UNCOMPRESSED_TCP, &pppVJUIPProtocolDescriptor, pContext);
		if (dwResult != NO_ERROR)
			break;

		// Initialize context

		pContext->session = session;

		// Create Fsm

		pContext->pFsm = PppFsmNew(&ipcpFsmData, ipcpResetPeerOptionValuesCb, pContext);
		if (pContext->pFsm == NULL)
		{
			dwResult = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		// Default max VJ slot ID is 15
		pContext->VJMaxSlotIdTx = MAX_STATES - 1;
		pContext->VJMaxSlotIdRx = MAX_STATES - 1;

		 // Disable slot ID compression because we don't get link errors
		pContext->VJEnableSlotIdCompressionTx = FALSE;
		pContext->VJEnableSlotIdCompressionRx = FALSE;

		// Read optional overrides from registry
        ReadRegistryValues(HKEY_LOCAL_MACHINE, TEXT("Comm\\PPP\\Parms"),
						L"VJMaxSlotIdTx",               REG_DWORD, 0,  &pContext->VJMaxSlotIdTx,                 sizeof(DWORD),
						L"VJMaxSlotIdRx",               REG_DWORD, 0,  &pContext->VJMaxSlotIdRx,                 sizeof(DWORD),
						L"VJEnableSlotIdCompressionTx", REG_DWORD, 0,  &pContext->VJEnableSlotIdCompressionTx,   sizeof(DWORD),
						L"VJEnableSlotIdCompressionRx", REG_DWORD, 0,  &pContext->VJEnableSlotIdCompressionRx,   sizeof(DWORD),
						NULL);

		// Configure option values
		ipcpOptionInit(pContext);

		// Put the FSM into the opened state, ready to go when the lower layer comes up

		PppFsmOpen(pContext->pFsm);

	} while (FALSE);

	if (dwResult != NO_ERROR)
	{
		pppFree(pSession, pContext, TEXT( "IPCP CONTEXT" ));
		pContext = NULL;
	}

	*ReturnedContext = pContext;
    return dwResult;
}

void
pppIpcp_InstanceDelete(
	IN	PVOID	context )
//
//	Called during session deletion to free the IPCP protocol context
//	created by pppIpcp_InstanceCreate.
//
{
	PIPCPContext	pContext = (PIPCPContext)context;

    DEBUGMSG( ZONE_IPCP | ZONE_FUNCTION, (TEXT("pppIpcp_InstanceDelete\r\n" )));

	if (context)
	{
		PppFsmClose(pContext->pFsm);
		PppFsmDelete(pContext->pFsm);
		pppFree(pContext->session, pContext->pVJRxBuf, TEXT("VJ RXBUF"));
		pppFree(pContext->session, pContext, TEXT( "IPCP CONTEXT" ) );
	}
}
