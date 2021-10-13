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

// PPP Link Control Protocol (LCP)

//  Include Files

#include "windows.h"
#include "cclib.h"
#include "memory.h"
#include "cxport.h"
#include "crypt.h"

#include "ndis.h"
#include "ndiswan.h"

// PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "mac.h"
#include "lcp.h"
#include "auth.h"
#include "ras.h"
#include "ncp.h"
#include "pppserver.h"

void lcpIdleDisconnectTimerStop(IN  PLCPContext pContext);
void lcpIdleDisconnectTimerStart(IN  PLCPContext pContext, IN  DWORD DurationMs);
void lcpSetIdleDisconnectMs(IN  PVOID   context, IN  DWORD   dwIdleDisconnectMs);

void
pppLcpCloseCompleteCallback(
    PLCPContext c_p)
//
//  This function is called when the LCP FSM transitions to the closed state.
//  It completes any pending close requests.
//
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +pppLcpCloseCompleteCallback\n" )));

    pppExecuteCompleteCallbacks(&c_p->pPendingCloseCompleteList);

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: -pppLcpCloseCompleteCallback\n" )));
}

void
pppLcp_GetLinkInfoCompleteCallback(
    IN  PLCPContext             pContext,
    IN  NDIS_WAN_GET_LINK_INFO *pInfo,
    IN  NDIS_STATUS             Status
    )
//
//  Called after the MAC layer indicates that it is up
//  and we retrieve MAC settings.
//
{
    pppSession_t      *pSession = (pppSession_t *)(pContext->session);
    OptionRequireLevel orlLocalMRU,
                       orlLocalAuth,
                       orlPFC,
                       orlACFC;
    USHORT             DefaultMTU;

    DefaultMTU = PPP_DEFAULT_MTU;
    if (_tcscmp(pSession->rasEntry.szDeviceType, RASDT_Vpn) == 0)
        DefaultMTU = PPP_DEFAULT_VPN_MTU;

    pContext->linkMaxSendPPPInfoFieldSize = DefaultMTU;
    pContext->linkMaxRecvPPPInfoFieldSize = DefaultMTU;

    if (Status == NDIS_STATUS_SUCCESS)
    {
        //
        // The MaxSend/RecvFrameSizes returned by the miniport must be reduced
        // by 2 bytes to account for the PPP protocol field in order to compute
        // the maximum PPP info field size.
        //
        pContext->linkMaxSendPPPInfoFieldSize = (USHORT)pInfo->MaxSendFrameSize - 2;
        pContext->linkMaxRecvPPPInfoFieldSize = (USHORT)pInfo->MaxRecvFrameSize - 2;
    }

    lcpOptionValueReset(pContext);

    // If our desired MaxRecvFrameSize is not the default MRU (1500),
    // then we want to negotiate this with the peer.  That is, we
    // want to tell our peer what size packets we can handle.

    orlLocalMRU = ORL_Allowed;
    if (pContext->local.MRU != PPP_DEFAULT_MTU)
        orlLocalMRU = ORL_Wanted;

    orlPFC = ORL_Unsupported;
    if (pContext->FramingBits & PPP_COMPRESS_PROTOCOL_FIELD)
        orlPFC = ORL_Wanted;

    orlACFC = ORL_Unsupported;
    if (pContext->FramingBits & PPP_COMPRESS_ADDRESS_CONTROL)
        orlACFC = ORL_Wanted;

    PppFsmOptionsSetORLs(pContext->pFsm,
                LCP_OPT_MRU,           orlLocalMRU,  ORL_Allowed,
                LCP_OPT_PFC,           orlPFC,       orlPFC,
                LCP_OPT_ACFC,          orlACFC,      orlACFC,
                -1);

    if (pSession->bIsServer)
    {
        orlLocalAuth = ORL_Allowed;
        if (PPPServerGetSessionAuthenticationRequired(pSession))
            orlLocalAuth = ORL_Required;

        PppFsmOptionsSetORLs(pContext->pFsm,
                LCP_OPT_AUTH_PROTOCOL, orlLocalAuth,  ORL_Unsupported,
                -1);
    }

    if (pContext->bMACLayerUp)
        PppFsmLowerLayerUp(pContext->pFsm);
}

void
pppLcp_LowerLayerUp(
    IN  PVOID   context)
//
//  This function will be called when the MAC layer is up
//
{
    PLCPContext     pContext = (PLCPContext)context;
    pppSession_t    *pSession = (pppSession_t *)(pContext->session);
    WCHAR           wszRegKey[MAX_PATH + 1];

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +pppLcp_LowerLayerUp\n" )));

    pContext->bMACLayerUp = TRUE;

    //
    //  Get Mac parameters
    //

    pppMac_GetFramingInfo(pSession->macCntxt, &pContext->FramingBits, &pContext->DesiredACCM);

    //
    //  Override the global miniport value with the per-line
    //  registry setting value if present.
    //
    PREFAST_ASSERT(wcslen(pContext->session->rasEntry.szDeviceName) <= RAS_MaxDeviceName);
    StringCchPrintfW(wszRegKey, _countof(wszRegKey), L"Comm\\ppp\\Parms\\Line\\%s", pSession->rasEntry.szDeviceName);
    (void) ReadRegistryValues(HKEY_LOCAL_MACHINE, wszRegKey,
                                    L"ACCM",    REG_DWORD, 0, &pContext->DesiredACCM, sizeof(DWORD),
                                    NULL);

    pppMac_GetLinkInfo(pSession->macCntxt, pppLcp_GetLinkInfoCompleteCallback, pContext);

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: -pppLcp_LowerLayerUp\n" )));
}

void
pppLcp_LowerLayerDown(
    IN  PVOID   context)
//
//  This function will be called when the auth layer is down
//
{
    PLCPContext pContext = (PLCPContext)context;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +pppLcp_LowerLayerDown\n" )));

    pContext->bMACLayerUp = FALSE;

    PppFsmLowerLayerDown(pContext->pFsm);

    if( pContext->pFsm->state == PFS_Initial )
    {
        // Complete any pending close requests
        pppLcpCloseCompleteCallback(pContext);
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: -pppLcp_LowerLayerDown\n" )));
}

static DWORD
lcpSendPacket(
    PVOID context,
    USHORT ProtocolWord,
    PBYTE   pData,
    DWORD   cbData)
{
    PLCPContext pContext = (PLCPContext)context;
    pppSession_t    *pSession = (pppSession_t *)(pContext->session);
    DWORD           dwResult;

    dwResult = pppSendData(pSession, ProtocolWord, pData, cbData);

    return dwResult;
}

static void
lcpConfigureMACLayer(
    PLCPContext pContext )
//
//  Called to configure the MAC layer with either:
//    1. parameters negotiated by LCP (if LCP is Opened), OR
//    2. default LCP settings (prior to LCP entering the Opened state)
//
{
    pppSession_t           *pSession = (pppSession_t *)(pContext->session);
    NDIS_WAN_SET_LINK_INFO  LinkInfo;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +lcpConfigureMACLayer\n" )));

    memset ((char *)&LinkInfo, 0, sizeof(LinkInfo));

    if (pContext->pFsm->state == PFS_Opened)
    {
        // Update MAC Settings with negotiated values
        //
        LinkInfo.MaxSendFrameSize = pContext->peer.MRU;
        LinkInfo.SendACCM = pContext->peer.ACCM;
        LinkInfo.SendFramingBits = PPP_FRAMING;
        if( pContext->peer.bPFC)
            LinkInfo.SendFramingBits |= PPP_COMPRESS_PROTOCOL_FIELD;
        if( pContext->peer.bACFC)
            LinkInfo.SendFramingBits |= PPP_COMPRESS_ADDRESS_CONTROL;

        LinkInfo.MaxRecvFrameSize = pContext->local.MRU;
        LinkInfo.RecvACCM = pContext->local.ACCM;
        LinkInfo.RecvFramingBits = PPP_FRAMING;
        if( pContext->local.bPFC)
            LinkInfo.RecvFramingBits |= PPP_COMPRESS_PROTOCOL_FIELD;
        if( pContext->local.bACFC)
            LinkInfo.RecvFramingBits |= PPP_COMPRESS_ADDRESS_CONTROL;
    }
    else
    {
        //
        // Prior to entering the Opened state, LCP default settings
        // are in effect (regardless of what our current in-negotiation
        // context values may be at the moment).
        //
        LinkInfo.MaxSendFrameSize = PPP_DEFAULT_MTU;
        if (pContext->linkMaxSendPPPInfoFieldSize < PPP_DEFAULT_MTU)
            LinkInfo.MaxSendFrameSize = pContext->linkMaxSendPPPInfoFieldSize;

        LinkInfo.SendACCM = LCP_DEFAULT_ACCM;
        LinkInfo.SendFramingBits = PPP_FRAMING;

        LinkInfo.MaxRecvFrameSize = pContext->linkMaxRecvPPPInfoFieldSize;
        LinkInfo.RecvACCM = LCP_DEFAULT_ACCM;
        LinkInfo.RecvFramingBits = PPP_FRAMING;
    }

    // Set MAC Layer

    pppMac_SetLink(pSession->macCntxt, &LinkInfo );
}

static  DWORD
lcpUp(
    PVOID context)
//
//  This is called when the FSM enters the Opened state
//
{
    PLCPContext pContext = (PLCPContext)context;
    pppSession_t    *pSession = (pppSession_t *)(pContext->session);

    pSession->RxBufferSize = pContext->local.MRU;
    lcpConfigureMACLayer( pContext );

    // Indicate to Session Manager and upper layer that we are UP.
    // RAS state AllDevicesConnected - doesn't seem quite right 
    // but is best fit.

    pppChangeOfState( pSession, RASCS_AllDevicesConnected, 0 );

    // Inform higher layer (AUTH) that LCP is up
    AuthLowerLayerUp(pSession->authCntxt);

    // Start the Idle Disconnect Timer as appropriate
    lcpSetIdleDisconnectMs(pContext, pContext->dwIdleDisconnectMs);

    return NO_ERROR;
}

static  DWORD
lcpDown(
    PVOID context)
//
//  This is called when the FSM leaves the Opened state
//
{
    PLCPContext      pContext = (PLCPContext)context;
    pppSession_t    *pSession = (pppSession_t *)(pContext->session);
    PPPP_RX_BUFFER   pRxBuffer;

    lcpIdleDisconnectTimerStop(pContext);

    lcpOptionValueReset(pContext);

    // Set MAC layer settings to defaults
    lcpConfigureMACLayer( pContext );

    // Indicate to upper (auth) layer we are down
    AuthLowerLayerDown(pSession->authCntxt);

    // Free RxBuffers.
    while (TRUE)
    {
        pRxBuffer = pSession->RxBufferList;
        if (pRxBuffer == NULL)
            break;
        pSession->RxBufferList = pRxBuffer->Next;
        pRxBuffer->Next = NULL;
        pppFree(pSession, pRxBuffer);
    }

    return NO_ERROR;
}

static  DWORD
lcpStarted(
    PVOID context)
{
    PLCPContext pContext = (PLCPContext)context;

    DEBUGMSG( ZONE_LCP, ( TEXT( "PPP: LCP STARTED\n" )));

    return NO_ERROR;
}

static  DWORD
lcpFinished(
    PVOID context)
//
//  Called when the FSM enters the Closed or Stopped state from a higher state,
//  or enters the Initial state from the Starting state.
//
{
    PLCPContext pContext = (PLCPContext)context;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +lcpFinished\n" )));

    //
    // Take the MAC layer down
    // Note that this will result in the RASCS_Disconnected state being
    // entered when LINECALLSTATE_DISCONNECTED  is indicated by the miniport.
    //
    pppMac_CallClose(pContext->session->macCntxt, NULL, NULL);

    if( pContext->pFsm->state == PFS_Initial )
    {
        // Complete any pending close requests
        pppLcpCloseCompleteCallback(pContext);
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: -lcpFinished\n" )));

    return NO_ERROR;
}

DWORD
lcpSessionLock(
    PVOID context)
//
//  This is called by the FSM when it needs to lock the session
//  because a timer has expired.
//
{
    PLCPContext pContext = (PLCPContext)context;

    pppLock(pContext->session);

    return NO_ERROR;
}

DWORD
lcpSessionUnlock(
    PVOID context)
//
//  This is called by the FSM when it needs to unlock the session
//  after a prior call to lock it due to timer expiry.
//
{
    PLCPContext pContext = (PLCPContext)context;

    pppUnLock(pContext->session);

    return NO_ERROR;
}

void
LcpProtocolRejected(
    IN  PVOID   context)
//
//  Called when we receive a protocol reject packet from
//  the peer in which they are rejecting the LCP protocol.
//
{
    PLCPContext     pContext = (PLCPContext)context;

    //
    // A protocol reject of an LCP packet is a major error, can't do
    // anything without LCP!
    //
    DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR - LCP protocol REJECTED\n")));
    PppFsmProtocolRejected( pContext->pFsm );
}

void
lcpRxProtocolReject(
    IN  PVOID   context,
    IN  BYTE    code,
    IN  BYTE    id,
    IN  PBYTE   pData,
    IN  DWORD   cbData)
//
// Called when we receive an LCP_PROTOCOL_REJECT packet from
// the peer because they didn't like the protocol type in a
// packet we sent.
// pData will point to the packet that we sent that the peer
// doesn't like.
//
{
    PLCPContext     pContext = (PLCPContext)context;
    pppMsg_t        msg;

    // At this point the LCP header has been removed from the received 
    // packet.  Therefore this packet is our packet from the protocol 
    // to be terminated. Decode the packet again to determine the enclosed
    // protocol and change the option header to P_PROT_REJ. Route the packet
    // with the changed Code back to the protocol to terminate it.

    msg.data = pData;
    msg.len  = cbData;

    PPPRxProtocolReject(pContext->session, &msg);
}

void
lcpRxEchoRequest(
    IN  PVOID   context,
    IN  BYTE    code,
    IN  BYTE    id,
    IN  PBYTE   pData,
    IN  DWORD   cbData)
{
    PLCPContext     pContext = (PLCPContext)context;
    PBYTE           pPacket = pData - PPP_PACKET_HEADER_SIZE;

    if (cbData < 4)
    {
        // 4 byte magic number field is required in an echo-request or echo-reply
        DEBUGMSG( ZONE_LCP, (TEXT( "PPP: LCP ECHO REQUEST missing magic number, cbData=%u\n"), cbData));
    }
    else
    {
        // Reception of a Magic-Number equal to the negotiated local magic number
        // indicates a looped back link.

        // Reception of a Magic-Number other than the negotiated local magic number,
        // the peer's negotiated magic number, or zero if the peer didn't negotiate one,
        // indicates a link which has been (mis)configured for communications with
        // a different peer.

        // Change packet code to echo REPLY

        pPacket[0] = LCP_ECHO_REPLY;             

        // Put in our magic number

        memcpy(pData, &pContext->local.MagicNumber, sizeof( DWORD ) );

        // Send the REPLY

        pppSendData(pContext->session, PPP_PROTOCOL_LCP, pPacket, cbData + PPP_PACKET_HEADER_SIZE);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Echo Request Management for dead link detection
//
//  1. If no packets of any kind are received from the peer for half a disconnect timeout interval 
//     (default 10/2 = 5 seconds), then we send an LCP echo request.
//  2. We keep sending LCP echo requests (at default 1 second intervals) until either we receive a packet
//     (echo reply or something else) or the disconnect timeout interval is reached.
//  3. If no packets of any kind are received from the peer for a full disconnect timeout interval,
//     then we terminate the link.
//
//  By default dead link detection is only enabled for PPPoE connections. The specific device types
//  for which it is enabled can be specified by setting the registry value:
//     [HKEY_LOCAL_MACHINE\Comm\Ppp\Parms]
//          "LcpIdleDeviceTypes"=multi_sz:"vpn","direct","pppoe"
//
void
lcpRxEchoReply(
    IN  PVOID   context,
    IN  BYTE    code,
    IN  BYTE    id,
    IN  PBYTE   pData,
    IN  DWORD   cbData)
{
    PLCPContext     pContext = (PLCPContext)context;

    //
    // Let's not be picky about the ID matching that of the most recent request.
    // The peer is there and sending us messages, so that's good.
    //
    // The session will have set the bRxData flag already to indicate that we
    // have received data and thus the peer is alive, we don't need to do anything.
    //
}

void
lcpEchoRequestSend(
    PLCPContext     pContext)
//
//  Build and send an LCP Echo Request packet.
//
{
    BYTE echoRequestPacket[8];

    //
    // Echo request format:
    //    Code   = LCP_ECHO_REQUEST (9)
    //    Id     = sequence number incremented for each echo request transmission
    //    Length = 0x0008
    //    Magic Number (4 bytes)
    //
    pContext->EchoRequestId++;
    PPP_SET_PACKET_HEADER(echoRequestPacket, LCP_ECHO_REQUEST, (BYTE)pContext->EchoRequestId, sizeof(echoRequestPacket));
    DWORD_TO_BYTES(&echoRequestPacket[4], pContext->local.MagicNumber);

    DEBUGMSG(ZONE_TRACE, (L"PPP: TX LCP Echo-Request ID=%u", pContext->EchoRequestId));

    pppSendData(pContext->session, PPP_PROTOCOL_LCP, echoRequestPacket, 8);
}

void lcpIdleDisconnectTimerCb(CTETimer *timer, PVOID     context);

void
lcpIdleDisconnectTimerStop(
    IN  PLCPContext     pContext)
{
    CTEStopTimer(&pContext->IdleDisconnectTimer);
}

void
lcpIdleDisconnectTimerStart(
    IN  PLCPContext     pContext,
    IN  DWORD           DurationMs)
{
    if (DurationMs && pContext->pFsm->state == PFS_Opened)
    {
        CTEStartTimer(&pContext->IdleDisconnectTimer, DurationMs, lcpIdleDisconnectTimerCb, (PVOID)pContext);
    }
}

void
lcpIdleDisconnectTimerCb(
    CTETimer *timer,
    PVOID     context)
//
//  This function is called periodically to check to see if we are receiving traffic from the
//  peer. We take action based upon how much time has elapsed since the last traffic received:
//
//  Elapsed Time                            Action
//  ------------                            ------
//  0 to dwIdleDisconnectMs/2               Restart timer for dwIdleDisconnectMs/2
//  dwIdleDisconnect/2 to dwIdleDisconnect  Send Echo Request, start timer for dwEchoRequestIntervalMs
//  > dwIdleDisconnect                     Terminate link
//
{
    PLCPContext     pContext = (PLCPContext)context;
    BOOL            bTerminateConnection = FALSE;
    DWORD           CurrentTime = GetTickCount();
    DWORD           TimeSinceLastRxData;

    pppLock(pContext->session);

    if (pContext->session->bRxData)
    {
        pContext->session->bRxData = FALSE;
        pContext->LastRxDataTime = CurrentTime;

        // Line is receiving, restart timer
        lcpIdleDisconnectTimerStart(pContext, pContext->dwIdleDisconnectMs / 2);
    }
    else
    {
        // Didn't receive anything lately
        
        TimeSinceLastRxData = CurrentTime - pContext->LastRxDataTime;

        if (TimeSinceLastRxData <= pContext->dwIdleDisconnectMs)
        {
            // We haven't been idle for too long, send echo request
            lcpEchoRequestSend(pContext);
            lcpIdleDisconnectTimerStart(pContext, pContext->dwEchoRequestIntervalMs);
        }
        else
        {
            // Idle for too long, disconnect
            DEBUGMSG(ZONE_WARN, (L"PPP: WARNING - No RX data from peer for %u ms, terminating connection\n", pContext->dwIdleDisconnectMs));
            pContext->session->bDisconnectDueToUnresponsivePeer = TRUE;
            pppLcp_Close(pContext, NULL, NULL);
        }
    }

    pppUnLock(pContext->session);

}

void
lcpSetIdleDisconnectMs(
    IN  PVOID   context,
    IN  DWORD   dwIdleDisconnectMs)
{
    PLCPContext     pContext = (PLCPContext)context;

    lcpIdleDisconnectTimerStop(pContext);

    pContext->dwIdleDisconnectMs = dwIdleDisconnectMs;

    pContext->LastRxDataTime = GetTickCount();
    lcpIdleDisconnectTimerStart(pContext, pContext->dwIdleDisconnectMs / 2);
}

void
lcpRxDiscardRequest(
    IN  PVOID   context,
    IN  BYTE    code,
    IN  BYTE    id,
    IN  PBYTE   pData,
    IN  DWORD   cbData)
{
}

PppFsmExtensionMessageDescriptor lcpExtensionMessageDescriptor[] =
{
    {LCP_PROTOCOL_REJECT, "Protocol-Reject", lcpRxProtocolReject},
    {LCP_ECHO_REQUEST,    "Echo-Request",    lcpRxEchoRequest},
    {LCP_ECHO_REPLY,      "Echo-Reply",      lcpRxEchoReply},
    {LCP_DISCARD_REQUEST, "Discard-Request", lcpRxDiscardRequest},
    {0,                    NULL,             NULL}
};

//
//  cbMaxTxPacket derivation for LCP:
//      6 bytes standard PPP packet header
//      4 bytes MRU option
//      6 bytes ACCM option
//      5 bytes auth option
//      6 bytes magic number option
//      2 bytes pfc option
//      2 bytes acfc option
//
static PppFsmDescriptor lcpFsmData =
{
    "LCP",             // szProtocolName
    PPP_PROTOCOL_LCP,  // ProtocolWord
    64,                 // cbMaxTxPacket
    lcpUp,
    lcpDown,
    lcpStarted,
    lcpFinished,
    lcpSendPacket,
    lcpSessionLock,
    lcpSessionUnlock,
    &lcpExtensionMessageDescriptor[0]   // Extension message handlers for LCP
};

void
pppLcp_Open(
    IN  PLCPContext pContext)
//
//  Get the LCP FSM into the opened state, ready to establish connections when the
//  MAC layer is up.
//
{
    PppFsmOpen(pContext->pFsm);
}

void
pppLcp_Close(
    IN  PLCPContext pContext,
    IN  void        (*pLcpCloseCompleteCallback)(PVOID),
    IN  PVOID       pCallbackData)
//
//  Get LCP into the initial state, i.e. closed and MAC layer down.
//  When PFS_Initial is achieved, invoke the pLcpCloseCompleteCallback function.
//
{
    DWORD dwResult;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +pppLcp_Close\n" )));

    // Signal the state machine to
    // get to the initial state.  The state machine will
    // call pppLcpCloseCompleteCallback when that state
    // is achieved, and that function will call the
    // pLcpCloseCompleteCallback handler.
    //

    DEBUGMSG(ZONE_LCP, (TEXT( "PPP: PppFsmClose LCP\n" )));
    PppFsmClose(pContext->pFsm);

    if( pContext->pFsm->state == PFS_Initial )
    {
        // All done
        if (pLcpCloseCompleteCallback)
            pLcpCloseCompleteCallback(pCallbackData);
    }
    else 
    {   
        // Can be in one of the following states:
        //   PFS_Closed  (MAC layer is still up)
        //   PFS_Closing (MAC layer is up, waiting for terminate-ack)
        //
        // Allocate and add the close request to the queue of
        // pending close requests.
        //
        dwResult = pppInsertCompleteCallbackRequest(&pContext->pPendingCloseCompleteList, pLcpCloseCompleteCallback, pCallbackData);
        if (ERROR_SUCCESS != dwResult)
        {
            // Unable to allocate callback structure. Perform callback immediately as if LCP was down (no graceful LCP shutdown).
            if (pLcpCloseCompleteCallback)
                pLcpCloseCompleteCallback(pCallbackData);
        }
        else
        {
            if (pContext->pFsm->state == PFS_Closed)
            {
                // Need to terminate MAC layer to get to Initial state
                pppMac_CallClose(pContext->session->macCntxt, NULL, NULL);

                // When lcpLowerLayerDown is called, we will enter the PFS_Initial state and
                // process pPendingCloseCompleteList
            }
            else // PFS_Closing state
            {
                // lcpFinished will be called when the state machine transitions from:
                //    PFS_Closing to PFS_Initial (on a Down indication) or
                //    PFS_Closing to PFS_Closed (on receive-terminate-ack)
            }
        }
    }
    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: -pppLcp_Close\n" )));
}

void
pppLcp_Rejected(
    IN  PVOID   context )
//
//  Called when the peer rejects the LCP protocol.
//  The link will be terminated.
//
{
    PLCPContext pContext = (PLCPContext)context;
            
    PppFsmProtocolRejected(pContext->pFsm);
}

static BOOLEAN
StringIsInMultiSz(
    IN  PWSTR   sz,
    IN  PWSTR   multisz)
//
//  Return TRUE if sz is one of the strings in multisz.
//
{
    BOOLEAN bFound = FALSE;

    while (*multisz != L'\0')
    {
        if (wcsicmp(sz, multisz) == 0)
        {
            bFound = TRUE;
            break;
        }
        multisz += wcslen(multisz) + 1;
    }
    return bFound;
}

DWORD
LcpOpen(
    IN  PVOID   context)
//
//  Called when RasIoControl is invoked to cause us to open LCP.
//
{
    PLCPContext     pContext = (PLCPContext)context;

    DEBUGMSG(ZONE_LCP, (TEXT("PPP: Open LCP\n")));
    return PppFsmOpen(pContext->pFsm);
}

DWORD
LcpClose(
    IN  PVOID   context)
//
//  Called when RasIoControl is invoked to cause us to open LCP.
//
{
    PLCPContext     pContext = (PLCPContext)context;

    DEBUGMSG(ZONE_LCP, (TEXT("PPP: Close LCP\n")));
    return PppFsmClose(pContext->pFsm);
}

DWORD
LcpRenegotiate(
    IN  PVOID   context)
//
//  Called when RasIoControl is invoked to cause us to renegotiate
//  our LCP parameters, typically after we have modified one of
//  the settings.
//
{
    PLCPContext     pContext = (PLCPContext)context;

    DEBUGMSG(ZONE_LCP, (TEXT("PPP: Renegotiate LCP\n")));
    return PppFsmRenegotiate(pContext->pFsm);
}

DWORD
LcpGetParameter(
    IN      PVOID                     context,
    IN  OUT PRASCNTL_LAYER_PARAMETER  pParm,
    IN  OUT PDWORD                    pcbParm)
//
// Called by RasIoControl to get a particular LCP parameter.
//
{
    PLCPContext     pContext = (PLCPContext)context;
    DWORD           dwResult = ERROR_SUCCESS;
    lcpOptValue_t  *pValues = &pContext->local;

    pParm->dwValueType = RASCNTL_LAYER_PARAMETER_TYPE_DWORD;
    pParm->dwValueSize = sizeof(DWORD);
    switch (pParm->dwParameterId)
    {
    case LCP_OPT_MRU:
        pParm->dwValue = pValues->MRU;
        break;

    case LCP_OPT_ACCM:
        pParm->dwValue = pValues->ACCM;
        break;

    case LCP_OPT_AUTH_PROTOCOL:
        pParm->dwValue = pValues->Auth.Protocol;
        break;

    case LCP_OPT_MAGIC_NUMBER:
        pParm->dwValue = pValues->MagicNumber;
        break;

    case LCP_OPT_PFC:
        pParm->dwValue = pValues->bPFC;
        break;

    case LCP_OPT_ACFC:
        pParm->dwValue = pValues->bACFC;
        break;

    default:
        dwResult = ERROR_INVALID_PARAMETER;
        break;
    }

    return dwResult;
}

DWORD
LcpSetParameter(
    IN      PVOID                     context,
    IN      PRASCNTL_LAYER_PARAMETER  pParm,
    IN      DWORD                     cbParm)
//
// Called by RasIoControl to set a particular LCP parameter.
//
{
    PLCPContext     pContext = (PLCPContext)context;
    DWORD           dwResult = ERROR_SUCCESS;
    lcpOptValue_t  *pValues = &pContext->local;

    switch (pParm->dwParameterId)
    {
    case LCP_OPT_MRU:
        pValues->MRU = (USHORT)pParm->dwValue;;
        break;

    case LCP_OPT_ACCM:
        pContext->DesiredACCM = pParm->dwValue;
        break;

    case LCP_OPT_AUTH_PROTOCOL:
        pValues->Auth.Protocol = (USHORT)pParm->dwValue;
        break;

    case LCP_OPT_MAGIC_NUMBER:
        pValues->MagicNumber = pParm->dwValue;
        break;

    case LCP_OPT_PFC:
        pValues->bPFC = pParm->dwValue;
        break;

    case LCP_OPT_ACFC:
        pValues->bACFC = pParm->dwValue;
        break;

    default:
        dwResult = ERROR_INVALID_PARAMETER;
        break;
    }

    return dwResult;
}

PROTOCOL_DESCRIPTOR pppLcpProtocolDescriptor =
{
    pppLcpRcvData,
    LcpProtocolRejected,
    LcpOpen,
    LcpClose,
    LcpRenegotiate,
    LcpGetParameter,
    LcpSetParameter
};

DWORD
pppLcp_InstanceCreate(
    IN  PVOID session,
    OUT PVOID *ReturnedContext)
//
//  Called during session creation to allocate and initialize the IPCP protocol
//  context.
//
{
    pppSession_t    *pSession = (pppSession_t *)session;
    RASPENTRY       *pRasEntry = &pSession->rasEntry;
    PLCPContext     pContext;
    DWORD           dwResult = NO_ERROR;
    WCHAR           wmszIdleDeviceTypes[MAX_PATH];
    
    DEBUGMSG( ZONE_LCP | ZONE_FUNCTION, (TEXT( "pppLcp_InstanceCreate\r\n" )));
    ASSERT( session );

    do
    {
        pContext = (PLCPContext)pppAlloc(pSession,  sizeof(*pContext));
        if( pContext == NULL )
        {
            DEBUGMSG( ZONE_LCP, (TEXT( "PPP: ERROR: NO MEMORY for IPCP context\r\n" )) );
            dwResult = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        // Register LCP context and protocol decriptor with session

        dwResult = PPPSessionRegisterProtocol(pSession, PPP_PROTOCOL_LCP, &pppLcpProtocolDescriptor, pContext);
        if (dwResult != NO_ERROR)
            break;

        // Initialize context

        pContext->session = session;

        pContext->pPendingCloseCompleteList = NULL;

        CTEInitTimer(&pContext->IdleDisconnectTimer);
        pContext->dwIdleDisconnectMs = DEFAULT_IDLE_DISCONNECT_MS;
        pContext->dwEchoRequestIntervalMs = DEFAULT_ECHO_REQUEST_INTERVAL_MS;
        memset(wmszIdleDeviceTypes, 0, sizeof(wmszIdleDeviceTypes));
        wcscpy(wmszIdleDeviceTypes, RASDT_PPPoE);

        (void)ReadRegistryValues(HKEY_LOCAL_MACHINE, TEXT("Comm\\ppp\\Parms"),
                                        RAS_VALUENAME_LCP_IDLE_DISCONNECT_MS, REG_DWORD, 0,  &pContext->dwIdleDisconnectMs, sizeof(DWORD),
                                        RAS_VALUENAME_LCP_ECHO_REQUEST_INTERVAL_MS, REG_DWORD, 0,  &pContext->dwEchoRequestIntervalMs, sizeof(DWORD),
                                        RAS_VALUENAME_LCP_IDLE_DEVICE_TYPES, REG_MULTI_SZ, 0, &wmszIdleDeviceTypes[0], sizeof(wmszIdleDeviceTypes),
                                        NULL);

        if (!StringIsInMultiSz(pRasEntry->szDeviceType, wmszIdleDeviceTypes))
        {
            // Disable Idle Disconnect detection for this connection
            pContext->dwIdleDisconnectMs = 0;
        }

        // Create Fsm

        pContext->pFsm = PppFsmNew(&lcpFsmData, lcpResetPeerOptionValuesCb, pContext);
        if (pContext->pFsm == NULL)
        {
            dwResult = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        // Configure option values

        lcpOptionInit(pContext);

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

void
pppLcp_InstanceDelete(
    IN  PVOID   context )
//
//  Called during session deletion to free the IPCP protocol context
//  created by pppLcp_InstanceCreate.
//
{
    PLCPContext pContext = (PLCPContext)context;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+pppLcp_InstanceDelete\r\n" )));

    if (context)
    {
        DEBUGMSG(ZONE_LCP, (TEXT( "PPP: Close LCP\n" )));

        lcpIdleDisconnectTimerStop(pContext);
        PppFsmDelete(pContext->pFsm);
        pppFree( pContext->session, pContext);
    }
    DEBUGMSG(ZONE_FUNCTION, (TEXT("-pppLcp_InstanceDelete\r\n" )));
}

DWORD
pppLcp_QueryParameter(
    IN      PVOID   context,
    IN      BOOL    bLocal,
    IN      DWORD   type,
        OUT PVOID   pValue,
    IN  OUT PDWORD  pcbValue)
//
//  Query the value of a parameter negotiated by LCP
//
{
    PLCPContext    pContext = (PLCPContext)context;
    DWORD          dwResult = NO_ERROR;
    lcpOptValue_t *pValues = bLocal ? &pContext->local : &pContext->peer;
    PVOID          pCurValue;
    DWORD          dwSize;

    switch(type)
    {
    case LCP_OPT_MRU:
        dwSize = sizeof(pValues->MRU);
        pCurValue = &pValues->MRU;
        break;

    case LCP_OPT_AUTH_PROTOCOL:
        dwSize = sizeof(lcpAuthValue_t);
        pCurValue = &pValues->Auth;
        break;

    default:
        dwResult = ERROR_INVALID_PARAMETER;
        break;
    }

    if (dwResult == NO_ERROR)
    {
        if (*pcbValue < dwSize)
            dwResult = ERROR_INSUFFICIENT_BUFFER;
        else
            memcpy(pValue, pCurValue, dwSize);

        *pcbValue = dwSize;
    }

    return dwResult;
}