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
*   @module ccp.c | PPP Compression Control Protocol (CCP)
*/

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

#include "cclib.h"

//
//	Activesync 3.1 seems to send frames that are larger than the MRU.
//
#define ACTIVESYNC31_EXTRA_BYTES		64

static DWORD
ccpSendPacket(
	PVOID context,
	USHORT ProtocolWord,
	PBYTE	pData,
	DWORD	cbData)
{
	PCCPContext  	pContext = (PCCPContext)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->session);
	DWORD			dwResult;

	dwResult = pppSendData(pSession, ProtocolWord, pData, cbData);

	return dwResult;
}

DWORD
ccpSessionLock(
	PVOID context)
//
//	This is called by the FSM when it needs to lock the session
//	because a timer has expired.
//
{
	PCCPContext	pContext = (PCCPContext)context;

	pppLock(pContext->session);

	return NO_ERROR;
}

DWORD
ccpSessionUnlock(
	PVOID context)
//
//	This is called by the FSM when it needs to unlock the session
//	after a prior call to lock it due to timer expiry.
//
{
	PCCPContext	pContext = (PCCPContext)context;

	pppUnLock(pContext->session);

	return NO_ERROR;
}

static  DWORD
ccpStarted(
	PVOID context)
{
	PCCPContext	pContext = (PCCPContext)context;

    DEBUGMSG( ZONE_IPCP, ( TEXT( "PPP: IPCP STARTED\n" )));

	ccpOptionValueReset(pContext);

	return NO_ERROR;
}

static  DWORD
ccpFinished(
	PVOID context)
{
	PCCPContext	     pContext = (PCCPContext)context;
	pppSession_t    *pSession = (pppSession_t *)(pContext->session);
	RASPENTRY	    *pRasEntry = &pSession->rasEntry;

	if (NEED_ENCRYPTION(pRasEntry))
	{
		// If encryption is required, when CCP goes down we take the link down
		pppLcp_Close(pSession->lcpCntxt, NULL, NULL);
	}

	return NO_ERROR;
}

DWORD
ccpUp(
	IN	PVOID	context )
//
//	Called when CCP enters the Opened state
//
{
	PCCPContext	    pContext = (PCCPContext)context;
	DWORD			dwResult = NO_ERROR;

	ASSERT(pContext->prxEncryptionInfo == NULL);
	ASSERT(pContext->ptxEncryptionInfo == NULL);

	DEBUGMSG(ZONE_CCP, (L"PPP: CCP Up\n"));

	// Initialize the compression contexts

    initsendcontext( &pContext->mppcSndCntxt );   
    initrecvcontext( &pContext->mppcRcvCntxt );
	pContext->LastRC4Reset = 0;
	pContext->rxCoherency  = 0x0000;
	pContext->bRxOutOfSync = FALSE;
	pContext->txCoherency  = 0x0000;
	pContext->txFlush = TRUE; // Force resync on first packet
	pContext->lastDecryptionKeyIndex = 0;

	//
	//	Allocate transmit and receive encryption contexts as
	//  appropriate.
	//

	if (pContext->local.SupportedBits & pContext->bmEncryptionTypesSupported)
	{
		pContext->prxEncryptionInfo = EncryptionInfoNew(pContext, pContext->local.SupportedBits, 0);

		if (pContext->prxEncryptionInfo == NULL)
		{
			dwResult = ERROR_OUTOFMEMORY;
		}
	}

	if (pContext->peer.SupportedBits & pContext->bmEncryptionTypesSupported)
	{
		pContext->ptxEncryptionInfo = EncryptionInfoNew(pContext, pContext->peer.SupportedBits, CRYPTO_IS_SEND);

		if (pContext->ptxEncryptionInfo == NULL)
		{
			dwResult = ERROR_OUTOFMEMORY;
		}
	}

	// Indicate that we are connected if we were waiting on encryption
	pppNcp_IndicateConnected(pContext->session->ncpCntxt);

	return dwResult;
}

DWORD
ccpDown(
	IN	PVOID	context )
//
//	Called when CCP leaves the Opened state
//
{
	PCCPContext	    pContext = (PCCPContext)context;
	DWORD			dwResult = NO_ERROR;

	DEBUGMSG(ZONE_CCP, (L"PPP: CCP Down\n"));

    //
	//	Release any encryption contexts
	//
	EncryptionInfoDelete(pContext, pContext->prxEncryptionInfo);
	EncryptionInfoDelete(pContext, pContext->ptxEncryptionInfo);
	pContext->prxEncryptionInfo = NULL;
	pContext->ptxEncryptionInfo = NULL;

	ccpOptionValueReset(pContext);

	return dwResult;
}

PppFsmExtensionMessageDescriptor ccpExtensionMessageDescriptor[] =
{
	{CCP_RESET_REQ, "Reset-Request", ccpRxResetRequest},
	{CCP_RESET_ACK, "Reset-Ack",     ccpRxResetAck},
	{0,             NULL,            NULL}
};

//
//	cbMaxTxPacket derivation for IPCP:
//		4 bytes for standard header (type, id, 2 octet len)
//		6 bytes for MSPPC option (2 bytes header + 4 bytes SupportedBits)
//
static PppFsmDescriptor ccpFsmData =
{
	"CCP",             // szProtocolName
	PPP_PROTOCOL_CCP,  // ProtocolWord
	32,				   // cbMaxTxPacket
	ccpUp,
	ccpDown,
	ccpStarted,
	ccpFinished,
	ccpSendPacket,
	ccpSessionLock,
	ccpSessionUnlock,
	&ccpExtensionMessageDescriptor[0]	// Extension message handlers for CCP
};

void
CcpProtocolRejected(
	IN	PVOID	context)
//
//  Called when we receive a protocol reject packet from
//  the peer in which they are rejecting the CCP (or CPKT) protocol.
//
{
	PCCPContext	    pContext = (PCCPContext)context;

	//
	// Ok for peer to reject CCP, they don't want to do
	// compression or encryption. If encryption is mandatory,
	// then closing the CCP FSM will terminate the link.
	//
	PppFsmProtocolRejected( pContext->pFsm );
}

PROTOCOL_DESCRIPTOR pppCCPProtocolDescriptor =
{
	CcpProcessRxPacket,
	CcpProtocolRejected,
	CcpOpen,
	CcpClose,
	CcpRenegotiate
};

PROTOCOL_DESCRIPTOR pppCPKTProtocolDescriptor =
{
	CpktProcessRxPacket,
	CcpProtocolRejected
};


DWORD
pppCcp_InstanceCreate(
	IN	PVOID session,
	OUT	PVOID *ReturnedContext)
//
//	Called during session creation to allocate and initialize the IPCP protocol
//	context.
//
{
	pppSession_t    *pSession = (pppSession_t *)session;
	RASPENTRY		*pRasEntry = &pSession->rasEntry;
	PCCPContext     pContext;
	DWORD			dwResult = NO_ERROR;

    DEBUGMSG( ZONE_CCP | ZONE_FUNCTION, (TEXT( "pppCcp_InstanceCreate\r\n" )));
    ASSERT( session );

	do
	{
		pContext = (PCCPContext)pppAlloc(pSession,  sizeof(*pContext));
		if( pContext == NULL )
		{
			DEBUGMSG( ZONE_CCP, (TEXT( "PPP: ERROR: NO MEMORY for CCP context\r\n" )) );
			dwResult = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		// Register CCP context and protocol decriptor with session

		dwResult = PPPSessionRegisterProtocol(pSession, PPP_PROTOCOL_CCP, &pppCCPProtocolDescriptor, pContext);
		if (dwResult != NO_ERROR)
			break;

		dwResult = PPPSessionRegisterProtocol(pSession, PPP_PROTOCOL_COMPRESSION, &pppCPKTProtocolDescriptor, pContext);
		if (dwResult != NO_ERROR)
			break;

		// Initialize context

		pContext->session = session;

		// Determine what kind of encryption support we have on the local machine
		// By default, this will be 128 bit and 40 bit RC4, but a registry setting can selectively disable these.

		pContext->bmEncryptionTypesSupported = pSession->bmCryptTypes & (MSTYPE_ENCRYPTION_128 | MSTYPE_ENCRYPTION_40F);

		//
		// If we require encryption, but have no Crypto support for it, then
		// we are hosed.
		//
		if (NEED_ENCRYPTION(pRasEntry)
		&&  pContext->bmEncryptionTypesSupported == 0)
		{
			DEBUGMSG( ZONE_NCP | ZONE_ERROR, (TEXT( "CCP: ERROR - Encryption required but no crypto support present\n")));
			dwResult = ERROR_INVALID_PARAMETER;
			break;
		}

		// Create Fsm

		pContext->pFsm = PppFsmNew(&ccpFsmData, ccpResetPeerOptionValuesCb, pContext);
		if (pContext->pFsm == NULL)
		{
			dwResult = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		// Configure option values

		ccpOptionInit(pContext);

		ccpOptionValueReset(pContext);

		if (pContext->local.SupportedBits == 0)
		{
			// We don't want any CCP stuff, so don't open the FSM.
			// If the peer sends us a CCP message, we will open the FSM then.
			DEBUGMSG(ZONE_CCP, (TEXT("PPP: Skipping CCP Config-Request, no encryption or compression wanted\n")));
		}
		else
		{
			// Put the FSM into the opened state, ready to go when the lower layer comes up
			PppFsmOpen(pContext->pFsm);
		}

	} while (FALSE);

	if (dwResult != NO_ERROR)
	{
		pppFree(pSession, pContext);
		pContext = NULL;
	}

	*ReturnedContext = pContext;
    return dwResult;
}

void
pppCcp_InstanceDelete(
	IN	PVOID	context )
//
//	Called during session deletion to free the CCP protocol context
//	created by pppCcp_InstanceCreate.
//
{
	PCCPContext	pContext = (PCCPContext)context;

    DEBUGMSG( ZONE_CCP | ZONE_FUNCTION, (TEXT("pppCcp_InstanceDelete\r\n" )));

	if (context)
	{
		PppFsmDelete(pContext->pFsm);

		pppFree(pContext->session, pContext->ScratchRxBuf);
		pContext->ScratchRxBuf = NULL;
		pContext->cbScratchRxBuf = 0;

		// Free tx, rx encryption info
		EncryptionInfoDelete(pContext, pContext->ptxEncryptionInfo);
		EncryptionInfoDelete(pContext, pContext->prxEncryptionInfo);

		pppFree( pContext->session, pContext);
	}
}

void
pppCcp_LowerLayerUp(
	IN	PVOID	context)
//
//	This function will be called when the auth layer is up
//
{
	PCCPContext	pContext = (PCCPContext)context;
	USHORT          MRU;
	DWORD           cbMRU,
		            cbMaxRxBuf;
	
	//
	// Allocate scratch Rx buffer (after freeing current, if any)
	//
	pppFree(pContext->session, pContext->ScratchRxBuf);
	pContext->ScratchRxBuf = NULL;
	pContext->cbScratchRxBuf = 0;

	cbMRU = sizeof(MRU);
	pppLcp_QueryParameter(pContext->session->lcpCntxt, TRUE, LCP_OPT_MRU, &MRU, &cbMRU);
	cbMaxRxBuf = MRU + ACTIVESYNC31_EXTRA_BYTES;
	pContext->ScratchRxBuf = pppAlloc(pContext->session, cbMaxRxBuf);
	if (pContext->ScratchRxBuf)
		pContext->cbScratchRxBuf = cbMaxRxBuf;

	// if we fail to allocate ScratchRxBuf then we will not allow rx compression during
	// negotiations.

	PppFsmLowerLayerUp(pContext->pFsm);
}

void
pppCcp_LowerLayerDown(
	IN	PVOID	context)
//
//	This function will be called when the auth layer is down
//
{
	PCCPContext	pContext = (PCCPContext)context;

	PppFsmLowerLayerDown(pContext->pFsm);
}

DWORD
CcpOpen(
	IN	PVOID	context )
//
//	Called to open the CCP layer.
//
{
	PCCPContext	pContext = (PCCPContext)context;
			
	return PppFsmOpen(pContext->pFsm);
}

DWORD
CcpClose(
	IN	PVOID	context )
//
//	Called to initiate the closure of the CCP layer.
//
{
	PCCPContext	pContext = (PCCPContext)context;
			
	return PppFsmClose(pContext->pFsm);
}

DWORD
CcpRenegotiate(
	IN	PVOID	context )
//
//	Called to renegotiate CCP layer settings with the peer.
//
{
	PCCPContext	pContext = (PCCPContext)context;
			
	return PppFsmRenegotiate(pContext->pFsm);
}

void
pppCcp_Rejected(
	IN	PVOID	context )
//
//	Called when the peer rejects the CCP protocol..
//
{
	PCCPContext	pContext = (PCCPContext)context;
			
	PppFsmProtocolRejected(pContext->pFsm);
}
