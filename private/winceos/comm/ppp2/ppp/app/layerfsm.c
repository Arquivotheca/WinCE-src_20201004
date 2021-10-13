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


//
//  PPP Protocol Layer Finite State Machine Implementation Module
//

#include "windows.h"
#include "types.h"

#include "protocol.h"
#include "ppp.h"
#include "raserror.h"

#include "layerfsm.h"
#include "debug.h"

#define MALLOC(size)    LocalAlloc(LPTR, (size))
#define FREE(ptr)       LocalFree(ptr)

void PppFsmStartTimer(PPppFsm   pFsm,   ULONG   timeInMilliseconds);
void PppFsmNewState(IN  OUT PPppFsm pFsm, IN        PppFsmState newState);

#ifdef DEBUG
PSTR g_szStateName[] =
{
    "Initial",
    "Starting",
    "Closed",
    "Stopped",
    "Closing",
    "Stopping",
    "Request-Sent",
    "Ack-Received",
    "Ack-Sent",
    "Opened"
};
#endif

void
PppFsmActionTLU(
    IN  OUT PPppFsm pFsm)
//
//  This-Layer-Up
//
//  This action indicates to the upper layers that the automaton is
//  entering the Opened state.
//
{
    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmActionTLU %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));
    pFsm->pDescriptor->ThisLayerUpCb(pFsm->CbContext);
}

void
PppFsmActionTLD(
    IN  OUT PPppFsm pFsm)
//
//  This-Layer-Down
//
//  This action indicates to the upper layers that the automaton is
//  leaving the Opened state.
//
{
    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmActionTLD %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));
    pFsm->pDescriptor->ThisLayerDownCb(pFsm->CbContext);
}

void
PppFsmActionTLS(
    IN  OUT PPppFsm pFsm)
//
//  This-Layer-Started
//
//  This action indicates to the lower layers that the automaton is
//  entering the Starting state, and the lower layer is needed for the
//  link. The lower layer SHOULD respond with an Up event when the lower
//  layer is available.
//
{
    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmActionTLS %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));
    pFsm->pDescriptor->ThisLayerStartedCb(pFsm->CbContext);
}

void
PppFsmActionTLF(
    IN  OUT PPppFsm pFsm)
//
//  This-Layer-Finished
//
//  This action indicates to the lower layers that the automaton is
//  entering the Initial, Closed, or Stopped states, and the lower
//  layer is no longer needed for the link. The lower layer SHOULD
//  respond with a Down event when the lower layer has terminated.
//
{
    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmActionTLF %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));
    pFsm->pDescriptor->ThisLayerFinishedCb(pFsm->CbContext);
}

#ifdef DEBUG
PSTR
PacketCodeName(
    IN      PPppFsm pFsm,
    IN      BYTE    code)
{
    PPppFsmExtensionMessageDescriptor pMsgDescriptor;

    switch(code)
    {
    case PPP_CONFIGURE_REQUEST:   return "CR";
    case PPP_CONFIGURE_ACK:       return "ACK";
    case PPP_CONFIGURE_NAK:       return "NAK";
    case PPP_CONFIGURE_REJ:       return "REJ";
    case PPP_TERMINATE_REQUEST:   return "TERM-REQ";
    case PPP_TERMINATE_ACK:       return "TERM-ACK";
    case PPP_CODE_REJECT:         return "CODE-REJ";
    default:
        pMsgDescriptor = pFsm->pDescriptor->pExtensionMessageDescriptors;
        if (pMsgDescriptor)
        {
            while (TRUE)
            {
                if (pMsgDescriptor->MessageRxCb == NULL)
                {
                    // Didn't find a matching handler
                    pMsgDescriptor = NULL;
                    break;
                }
                if (pMsgDescriptor->code == code)
                    break;
                pMsgDescriptor++;
            }
        }

        if (pMsgDescriptor)
            return pMsgDescriptor->szName;
        else
            return "???";
    }
}

void
PppFsmDebugShowPacket(
    IN      PPppFsm pFsm,
    IN      PSTR    szTxOrRx,
    IN      PBYTE   pPacket,
    IN      DWORD   cbPacket)
{
    char     buffer[512];
    BYTE    type = pPacket[0];

    StringCchPrintfA(buffer, _countof(buffer), "%s %s %-3s ID=%u", szTxOrRx, pFsm->pDescriptor->szProtocolName, PacketCodeName(pFsm, type), pPacket[1]);

    if (PPP_CONFIGURE_REQUEST <= type && type <= PPP_CONFIGURE_REJ)
        OptionDebugAppendOptionList(&pFsm->optionContext, &buffer[0], pPacket + 4, cbPacket - 4);

    DEBUGMSG(1, (TEXT("PPP: %hs\n"), &buffer[0]));
}

#endif

DWORD
PppFsmSendPacket(
    IN      PPppFsm pFsm,
    IN      PBYTE   pPacket,
    IN      DWORD   cbPacket)
{
    DWORD   dwResult;

#ifdef DEBUG
    if (ZONE_TRACE)
        PppFsmDebugShowPacket(pFsm, "TX", pPacket, cbPacket);
#endif
    dwResult = pFsm->pDescriptor->SendPacketCb(pFsm->CbContext, pFsm->pDescriptor->ProtocolWord, pPacket, cbPacket);

    return dwResult;
}

DWORD
PppFsmActionSCR(
    IN  OUT PPppFsm pFsm)
//
//  Called to send a configure-request packet.
//
{
    DWORD dwResult,
          cbCR;
    
    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmActionSCR %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));
    cbCR = pFsm->pDescriptor->cbMaxTxPacket;
    dwResult = OptionBuildConfigureRequest(&pFsm->optionContext, pFsm->idTxCR, pFsm->pRequestBuffer, &cbCR);

    //
    // If we were unable to build the configure request, for example because the
    // peer rejected a required option, then just return the error.
    // If we built it successfully, send it and start the timer.
    //
    if (dwResult == NO_ERROR)
    {
        if (pFsm->dwRestartCount > 0)
            pFsm->dwRestartCount--;

        pFsm->cbRequestBuffer = cbCR;
        pFsm->bmActions |= ACTION_SEND_REQUEST;

        PppFsmStartTimer(pFsm, pFsm->dwRestartTimerMs);
    }

    return dwResult;
}

void
PppFsmActionSTR(
    IN  OUT PPppFsm pFsm)
//
//  Called to send a terminate-request packet.
//
{
    PBYTE pFrame = pFsm->pRequestBuffer;

    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmActionSTR %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));
    
    if (pFsm->dwRestartCount > 0)
        pFsm->dwRestartCount--;

    pFrame[0] = PPP_TERMINATE_REQUEST;
    pFrame[1] = ++pFsm->idTxTR;
    pFrame[2] = 0;
    pFrame[3] = 4;

    pFsm->cbRequestBuffer = 4;
    pFsm->bmActions |= ACTION_SEND_REQUEST;

    PppFsmStartTimer(pFsm, pFsm->dwRestartTimerMs);
}

void
PppFsmActionSTA(
    IN  OUT PPppFsm pFsm,
    IN      BYTE    idTR)
{
    PBYTE pFrame = &pFsm->abResponsePacket[0];

    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmActionSTA %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));
    
    pFrame[0] = PPP_TERMINATE_ACK;
    pFrame[1] = idTR;
    pFrame[2] = 0;
    pFrame[3] = 4;

    pFsm->cbResponsePacket = 4;
    pFsm->bmActions |= ACTION_SEND_RESPONSE;
}

void
PppFsmLockSession(
    PPppFsm pFsm)
{
    pFsm->pDescriptor->SessionLockCb(pFsm->CbContext);
}

void
PppFsmUnlockSession(
    PPppFsm pFsm)
{
    pFsm->pDescriptor->SessionUnlockCb(pFsm->CbContext);
}

void
PppFsmTimerExpireCb(
    CTETimer *pTimer,
    PVOID     CbContext)
//
//  Called when the timer expires, e.g. due to no response
//  received for a terminate-request or a configure-request.
//
{
    PPppFsm pFsm = (PPppFsm)CbContext;
    PppFsmState curState,
                newState;

    PppFsmLockSession(pFsm);

    //
    //  Ignore the timer if we have stopped it.
    //  That is, the timer thread could have fired just as/after we
    //  stopped it, and we can't prevent it from running,
    //  so we just have to ignore it.
    //
    if (pFsm->bTimerRunning)
    {
        curState = pFsm->state,
        newState = pFsm->state;

        pFsm->bTimerRunning = FALSE;

        DEBUGMSG(ZONE_TRACE, (TEXT("PPP: EV %hs TO%c\n"),
            pFsm->pDescriptor->szProtocolName, pFsm->dwRestartCount ? '+' : '-'));

        switch(curState)
        {
        case PFS_Closing:
        case PFS_Stopping:
            if (pFsm->dwRestartCount > 0)
            {
                PppFsmActionSTR(pFsm);
            }
            else
            {
                newState = curState == PFS_Closing ? PFS_Closed : PFS_Stopped;
            }
            break;

        case PFS_Request_Sent:
        case PFS_Ack_Received:
        case PFS_Ack_Sent:
            if (pFsm->dwRestartCount > 0
            &&  PppFsmActionSCR(pFsm) == NO_ERROR)
            {
                if (curState == PFS_Ack_Received)
                    newState =  PFS_Request_Sent;
            }
            else
            {
                newState = PFS_Stopped;
            }
            break;
        }

        PppFsmNewState(pFsm, newState);
    }

    PppFsmUnlockSession(pFsm);
}

void
PppFsmStopTimer(
    PPppFsm pFsm)
{
    if (pFsm->bTimerRunning)
    {
        CTEStopTimer(&pFsm->timer);
        pFsm->bTimerRunning = FALSE;
    }
}

void
PppFsmStartTimer(
    PPppFsm pFsm,
    ULONG   timeInMilliseconds)
//
//  Per IETF STD51, Section 4.1:
//    "Only the Send-Configure-Request, Send-Terminate-Request, and Zero-Restart-Count actions
//     start or restart the Restart timer."
//
{
    PppFsmStopTimer(pFsm);
    CTEStartTimer(&pFsm->timer, timeInMilliseconds, PppFsmTimerExpireCb, (PVOID)pFsm);
    pFsm->bTimerRunning = TRUE;
}

PPppFsm
PppFsmNew(
    IN  PPppFsmDescriptor           pDescriptor,
    IN  OPTION_RESET_PEER_OPT_CB    CbResetPeerOptions,
    IN  PVOID                       CbContext)
//
//  Allocate and initialize a new PppFsm structure.
//
{
    PPppFsm pFsm;
    DWORD   dwRestartTimerSecs = 0;

    if (pDescriptor->cbMaxTxPacket > 65536)
    {
        DEBUGMSG(ZONE_ERROR, (L"PPP: ERROR - cbMaxTxPacket configured too large (%u)\n", pDescriptor->cbMaxTxPacket));
        pFsm = NULL;
    }
    else
        pFsm = (PPppFsm)MALLOC(sizeof(PppFsm) + pDescriptor->cbMaxTxPacket);

    if (pFsm)
    {
        pFsm->pDescriptor = pDescriptor;
        pFsm->CbContext = CbContext;

        pFsm->state = PFS_Initial;

        pFsm->bTimerRunning = FALSE;
        CTEInitTimer(&pFsm->timer);

        pFsm->dwRestartTimerMs = DEFAULT_RESTART_TIMER_MS;
        pFsm->dwMaxTerminate   = DEFAULT_MAX_TERMINATE;
        pFsm->dwMaxConfigure   = DEFAULT_MAX_CONFIGURE;

        pFsm->pRequestBuffer = (PBYTE)(pFsm + 1);
        pFsm->idTxCR = 0xFF;
        OptionContextInitialize(&pFsm->optionContext, pDescriptor->szProtocolName, CbResetPeerOptions, CbContext);

        //
        //  Read in optional registry overrides for default settings
        //
        (void) ReadRegistryValues(HKEY_LOCAL_MACHINE, TEXT("Comm\\ppp\\Parms"),
                                    RAS_VALUENAME_MAXCONFIGURE,         REG_DWORD, 0, &pFsm->dwMaxConfigure,             sizeof(DWORD),
                                    RAS_VALUENAME_MAXTERMINATE,         REG_DWORD, 0, &pFsm->dwMaxTerminate,             sizeof(DWORD),
                                    RAS_VALUENAME_MAXFAILURE,           REG_DWORD, 0, &pFsm->optionContext.dwMaxFailure, sizeof(DWORD),
                                    RAS_VALUENAME_RESTARTTIMER,         REG_DWORD, 0, &dwRestartTimerSecs,               sizeof(DWORD),
                                    NULL);

        if (dwRestartTimerSecs)
            pFsm->dwRestartTimerMs = dwRestartTimerSecs * 1000;
    }
    return pFsm;
}

void
PppFsmDelete(
    PPppFsm pFsm)
//
//  Free a PppFsm previously created by PppFsmNew.
//
{
    if (pFsm)
    {
        PppFsmStopTimer(pFsm);

        OptionContextCleanup(&pFsm->optionContext);

        FREE(pFsm);
    }
}

DWORD
PppFsmOptionsAdd(
    PPppFsm pFsm,
    ...)
//
//  Add options to the list.
//  Varargs parameters should be in groups of
//      POptionDescriptor, OptionRequireLevel, OptionRequireLevel
//  terminated by a NULL POptionDescriptor
//
{
    DWORD   dwResult = NO_ERROR;
    POptionDescriptor       pDescriptor;
    OptionRequireLevel      orlLocal;
    OptionRequireLevel      orlPeer;
    va_list                 arglist;

    va_start(arglist, pFsm);

    do
    {
        pDescriptor = va_arg(arglist, POptionDescriptor);
        if (pDescriptor == NULL)
            break;

        orlLocal = va_arg(arglist, OptionRequireLevel);
        orlPeer  = va_arg(arglist, OptionRequireLevel);
        dwResult = OptionInfoAdd(&pFsm->optionContext, pDescriptor, orlLocal, orlPeer);
    } while (dwResult == NO_ERROR);

    va_end(arglist);

    return dwResult;
}

DWORD
PppFsmOptionsSetORLs(
    PPppFsm pFsm,
    ...)
//
//  Set the option require list for a set of options
//  Varargs parameters should be in groups of
//      Option Type, OptionRequireLevel, OptionRequireLevel
//  terminated by a -1 Option Type
//
{
    DWORD                   dwResult = NO_ERROR;
    int                     optionType;
    OptionRequireLevel      orlLocal;
    OptionRequireLevel      orlPeer;
    va_list                 arglist;

    va_start(arglist, pFsm);

    while (TRUE)
    {
        optionType = va_arg(arglist, int);
        if (optionType == -1)
            break;

        orlLocal = va_arg(arglist, OptionRequireLevel);
        orlPeer  = va_arg(arglist, OptionRequireLevel);
        dwResult = OptionSetORL(&pFsm->optionContext, (BYTE)optionType, orlLocal, orlPeer);
    }

    va_end(arglist);

    return dwResult;
}

void
PppFsmActionIRC(
    IN  OUT PPppFsm pFsm,
    IN      DWORD   dwMaxTries)
//
//  Initialize the restart count.
//
{
    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmActionIRC %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));
    pFsm->dwRestartCount = dwMaxTries;
}

PppFsmActionZRC(
    IN  OUT PPppFsm pFsm)
//
//  Zero the restart count.
//
//  From IETF STD51:
//      "This action enables the FSA to pause before proceeding
//      to the desired final state, allowing traffic to be processed by the
//      peer. In addition to zeroing the restart counter, the implementation
//      MUST set the timeout period to an appropriate value."
//
//  When the timer expires, it will be a TO- event because the Restart count is 0.
//  This will cause the transition from the Stopping state to the Stopped state.
//
{
    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmActionZRC %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));
    pFsm->dwRestartCount = 0;
    PppFsmStartTimer(pFsm, STOP_DELAY_TIME_MS);
}

void
PppFsmNewState(
    IN  OUT PPppFsm pFsm,
    IN      PppFsmState newState)
{
    PppFsmState curState = pFsm->state;
    void      (*pDeferredCb)(PPppFsm pFsm) = NULL;
    DWORD       bmActions;

    ASSERT(PFS_Initial <= newState && newState <= PFS_Opened);
    ASSERT(PFS_Initial <= pFsm->state && pFsm->state <= PFS_Opened);
    
    if (newState != pFsm->state)
    {
        //
        // Per IETF STD51, Section 4.1:
        //   "The Restart timer is stopped when transitioning from any state
        //    where the timer is running to a state where the timer is not running."
        //
        if (newState <= PFS_Stopped || newState == PFS_Opened)
        {
            // The timer is not active in these states.
            PppFsmStopTimer(pFsm);
        }

        DEBUGMSG(ZONE_TRACE, (TEXT("PPP:    %hs State %hs --> %hs\n"),
            pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state], g_szStateName[newState]));

        if (newState == PFS_Starting
        &&  (curState == PFS_Initial || curState == PFS_Stopped))
        {
            pDeferredCb = PppFsmActionTLS;
        }
        else if (newState == PFS_Opened)
        {
            // Transitioning to the Open state - this layer up
            pDeferredCb = PppFsmActionTLU;
        }
        else if ((newState == PFS_Initial || newState == PFS_Closed || newState == PFS_Stopped)
             &&  (curState != PFS_Initial && curState != PFS_Closed && curState != PFS_Stopped))
        {
            //
            // Transitioning from a state requiring the service of lower layers
            // to a state which does not.  Inform the lower layers they are no
            // longer needed.
            //
            pDeferredCb = PppFsmActionTLF;
        }

        pFsm->state = newState;
    }

    // Process deferred send request and response actions
    bmActions = pFsm->bmActions;
    pFsm->bmActions = 0;
    if (bmActions & ACTION_SEND_REQUEST)
        PppFsmSendPacket(pFsm, pFsm->pRequestBuffer, pFsm->cbRequestBuffer);
    if (bmActions & ACTION_SEND_RESPONSE)
        PppFsmSendPacket(pFsm, pFsm->abResponsePacket, pFsm->cbResponsePacket);

    if (pDeferredCb)
        pDeferredCb(pFsm);

}


DWORD
PppFsmOpen(
    PPppFsm pFsm)
{
    DWORD   dwResult = NO_ERROR;
    PppFsmState newState = pFsm->state;

    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmOpen %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));

    switch(pFsm->state)
    {
    case PFS_Initial:
        newState = PFS_Starting;
        break;

    case PFS_Closed:
        OptionResetNegotiation(&pFsm->optionContext);
        PppFsmActionIRC(pFsm, pFsm->dwMaxConfigure);
        pFsm->idTxCR++;
        dwResult = PppFsmActionSCR(pFsm);
        if (dwResult == NO_ERROR)
            newState = PFS_Request_Sent;
        break;

    case PFS_Closing:
        newState = PFS_Stopping;
        break;
    }

    PppFsmNewState(pFsm, newState);

    return dwResult;
}

DWORD
PppFsmClose(
    PPppFsm pFsm)
//
//  Called to request that the finite state machine
//  take steps to enter the closed state.
//
{
    DWORD       dwResult = NO_ERROR;
    PppFsmState newState = pFsm->state;

    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmClose %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));

    switch(pFsm->state)
    {
    case PFS_Initial:
        break;

    case PFS_Starting:
        newState = PFS_Initial;
        break;

    case PFS_Closed:
    case PFS_Closing:
        break;

    case PFS_Stopped:
        newState = PFS_Closed;
        break;

    case PFS_Stopping:
        newState = PFS_Closing;
        break;

    case PFS_Opened:
        PppFsmActionTLD(pFsm);
        // FALLTHROUGH
    case PFS_Request_Sent:
    case PFS_Ack_Received:
    case PFS_Ack_Sent:
        PppFsmActionIRC(pFsm, pFsm->dwMaxTerminate);
        PppFsmActionSTR(pFsm);
        newState = PFS_Closing;
        break;
    }

    PppFsmNewState(pFsm, newState);

    return dwResult;
}

DWORD
PppFsmRenegotiate(
    PPppFsm pFsm)
//
//  Called to request that the FSM renegotiate options with the peer.
//  This takes the layer down if it is currently Opened, then proceeds
//  through the whole negotiation process to get it back to the Opened state.
//
{
    DWORD   dwResult = NO_ERROR;
    PppFsmState newState = pFsm->state;

    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmRenegotiate %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));

    switch(pFsm->state)
    {
    case PFS_Initial:
    case PFS_Closed:
    case PFS_Closing:
    case PFS_Starting:
        break;

    case PFS_Opened:
        PppFsmActionTLD(pFsm);
        // FALLTHROUGH
    case PFS_Stopped:
    case PFS_Stopping:
    case PFS_Request_Sent:
    case PFS_Ack_Received:
    case PFS_Ack_Sent:
        PppFsmActionIRC(pFsm, pFsm->dwMaxConfigure);
        OptionResetNegotiation(&pFsm->optionContext);
        pFsm->idTxCR++;
        dwResult = PppFsmActionSCR(pFsm);
        if (dwResult == NO_ERROR)
            newState = PFS_Request_Sent;
        break;
    }

    PppFsmNewState(pFsm, newState);

    return dwResult;
}

DWORD
PppFsmLowerLayerUp(
    PPppFsm pFsm)
//
//  Called to indicate that the layer below this one has
//  come up, we are able to send/receive frames.
//
{
    DWORD       dwResult = NO_ERROR;
    PppFsmState newState = pFsm->state;

    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmLowerLayerUp %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));

    switch(pFsm->state)
    {
    case PFS_Initial:
        newState = PFS_Closed;
        break;

    case PFS_Starting:
        OptionResetNegotiation(&pFsm->optionContext);
        PppFsmActionIRC(pFsm, pFsm->dwMaxConfigure);
        pFsm->idTxCR++;
        dwResult = PppFsmActionSCR(pFsm);
        if (dwResult == NO_ERROR)
            newState = PFS_Request_Sent;
        else
            newState = PFS_Stopped;
        break;
    }

    PppFsmNewState(pFsm, newState);

    return dwResult;
}

DWORD
PppFsmLowerLayerDown(
    PPppFsm pFsm)
//
//  Called to indicate that the layer below this one has
//  gone down, we are unable to send/receive frames any more.
//
{
    DWORD   dwResult = NO_ERROR;

    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmLowerLayerDown %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));

    switch(pFsm->state)
    {
    case PFS_Initial:
    case PFS_Starting:
        break;

    case PFS_Closing:
    case PFS_Closed:
        PppFsmNewState(pFsm, PFS_Initial);
        break;

    case PFS_Opened:
        PppFsmActionTLD(pFsm);
        // FALLTHROUGH
    case PFS_Stopped:
    case PFS_Stopping:
    case PFS_Request_Sent:
    case PFS_Ack_Received:
    case PFS_Ack_Sent:
        PppFsmNewState(pFsm, PFS_Starting);
        break;

    default:
        ASSERT(FALSE);
        break;
    }

    return dwResult;
}

DWORD
PppFsmProtocolRejected(
    PPppFsm pFsm)
//
//  Called to handle the rejection of our protocol
//  by the peer.
//
{
    DWORD   dwResult = NO_ERROR;

    DEBUGMSG(ZONE_TRACE, (L"PPP: PppFsmProtocolRejected %hs State=%hs\n", pFsm->pDescriptor->szProtocolName, g_szStateName[pFsm->state]));

    switch(pFsm->state)
    {
    case PFS_Initial:
    case PFS_Starting:
        break;

    case PFS_Closed:
    case PFS_Closing:
        PppFsmNewState(pFsm, PFS_Closed);
        break;

    case PFS_Stopped:
    case PFS_Stopping:
    case PFS_Request_Sent:
    case PFS_Ack_Received:
    case PFS_Ack_Sent:
        PppFsmNewState(pFsm, PFS_Stopped);
        break;

    case PFS_Opened:
        PppFsmActionTLD(pFsm);
        PppFsmActionIRC(pFsm, pFsm->dwMaxTerminate);
        PppFsmActionSTR(pFsm);
        PppFsmNewState(pFsm, PFS_Stopping);
        break;
    }

    return dwResult;
}

void
PppFsmActionSendCodeReject(
    IN  OUT PPppFsm      pFsm,
    IN      PBYTE        pPacket,
    IN      DWORD        cbPacket)
//
//  Send a code-reject packet containing the packet
//  received from the peer with an unrecognized code.
//
{
    //
    // Per RFC1661 5.6
    //  "The Rejected-Packet MUST be truncated to comply with
    //   the peer's established MRU."
    //
    // Currently, for simplicity, assume default MRU of 1500.
    //
    if (cbPacket > (PPP_DEFAULT_MTU - PPP_PACKET_HEADER_SIZE))
    {
        // Truncate the rejected packet
        cbPacket = PPP_DEFAULT_MTU - PPP_PACKET_HEADER_SIZE;
    }

    pFsm->cbResponsePacket = PPP_PACKET_HEADER_SIZE + cbPacket;
    ASSERT(pFsm->cbResponsePacket <= PPP_DEFAULT_MTU);

    PPP_SET_PACKET_HEADER(pFsm->abResponsePacket, PPP_CODE_REJECT, pFsm->idNextCodeReject, pFsm->cbResponsePacket);
    pFsm->idNextCodeReject++;
    memcpy(&pFsm->abResponsePacket[PPP_PACKET_HEADER_SIZE], pPacket, cbPacket);

    pFsm->bmActions |= ACTION_SEND_RESPONSE;
}

#define MAX_RESPONSE_PACKET_SIZE PPP_DEFAULT_MTU

DWORD
PppFsmProcessRxConfigureRequest(
    IN  OUT PPppFsm      pFsm,
    IN      PBYTE        pPacket,
    IN      DWORD        cbPacket,
        OUT PBYTE        pResponsePacket,
    IN  OUT PDWORD       pcbResponsePacket)
{
    DWORD dwResult;

    //
    // Construct a response (ACK/NAK/REJ) to the configure-request
    //
    dwResult = OptionProcessRxMessage(
                    &pFsm->optionContext,
                    pPacket,
                    cbPacket,
                    pResponsePacket,
                    pcbResponsePacket);

    ASSERT ((dwResult != NO_ERROR) 
         || (*pcbResponsePacket >= PPP_PACKET_HEADER_SIZE && PPP_CONFIGURE_ACK <= *pResponsePacket && *pResponsePacket <= PPP_CONFIGURE_REJ));

    return dwResult;
}

DWORD
PppFsmProcessRxConfigureResponse(
    IN  OUT PPppFsm      pFsm,
    IN      PBYTE        pPacket,
    IN      DWORD        cbPacket)
{
    DWORD dwResult;

    dwResult = OptionProcessRxMessage(
                    &pFsm->optionContext,
                    pPacket,
                    cbPacket,
                    NULL,
                    NULL);

    return dwResult;
}

void
PppFsmProcessRxPacket(
    IN  OUT PPppFsm pFsm,
    IN      PBYTE   pPacket,
    IN      DWORD   cbPacket)
//
//  Process a received packet for the protocol managed by this Fsm.
//  The protocol type field must have been stripped from the packet already.
//
//  For state machine table, see RFC 1661 section 4.1
//
{
    PppFsmState curState,
                newState;
    BYTE        code,
                id;
    DWORD       dwResult;
    DWORD       length;
    PPppFsmExtensionMessageDescriptor pMsgDescriptor;

    do
    {
        //
        //  All packets have the form:
        //      <Code> <Id> <Length:2 octets> <Data: Length-4 octets>
        //
        if (cbPacket < PPP_PACKET_HEADER_SIZE)
        {
            DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING RX %hs short packet, %u bytes\n"), 
                pFsm->pDescriptor->szProtocolName, cbPacket));
            break;
        }

        code = pPacket[0];
        id = pPacket[1];
        length = (pPacket[2] << 8) | pPacket[3];
        if (length < PPP_PACKET_HEADER_SIZE || cbPacket < length)
        {
            DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING RX %hs length %u invalid for cbPacket %u\n"), 
                pFsm->pDescriptor->szProtocolName, length, cbPacket));
            break;
        }

        curState = pFsm->state;
        newState = curState;

#ifdef DEBUG
        if (ZONE_TRACE)
            PppFsmDebugShowPacket(pFsm, "RX", pPacket, cbPacket);
#endif
        switch(code)
        {
        case PPP_CONFIGURE_REQUEST:
            switch(curState)
            {
            case PFS_Closed:
                PppFsmActionSTA(pFsm, id);
                break;

            case PFS_Stopped:
            case PFS_Request_Sent:
            case PFS_Ack_Sent:
            case PFS_Ack_Received:
            case PFS_Opened:
                pFsm->cbResponsePacket = sizeof(pFsm->abResponsePacket);
                dwResult = PppFsmProcessRxConfigureRequest(pFsm, pPacket, cbPacket, &pFsm->abResponsePacket[0], &pFsm->cbResponsePacket);
                if (dwResult == NO_ERROR)
                {
                    newState = pFsm->abResponsePacket[0] == PPP_CONFIGURE_ACK ? PFS_Ack_Sent : PFS_Request_Sent;

                    if (curState == PFS_Stopped || curState == PFS_Opened)
                    {
                        if (curState == PFS_Opened)
                            PppFsmActionTLD(pFsm);

                        OptionResetNegotiation(&pFsm->optionContext);
                        PppFsmActionIRC(pFsm, pFsm->dwMaxConfigure);
                        pFsm->idTxCR++;
                        if (PppFsmActionSCR(pFsm) != NO_ERROR)
                            newState = PFS_Stopped;
                    }
                    else if (curState == PFS_Ack_Received)
                    {
                        newState = newState == PFS_Ack_Sent ? PFS_Opened : PFS_Ack_Received;
                    }

                    //
                    // Note that the response must be sent AFTER the config-request sent above
                    // in order to prevent infinite renegotiations from occuring.
                    //
                    pFsm->bmActions |= ACTION_SEND_RESPONSE;
                }
                else if (dwResult == ERROR_PPP_NOT_CONVERGING)
                {
                    // Further negotiation is probably futile, give up and shut down the link
                    newState = PFS_Stopped;
                }
                break;
            }
            break;

        case PPP_CONFIGURE_ACK:
        case PPP_CONFIGURE_NAK:
        case PPP_CONFIGURE_REJ:
            //
            // Ignore the response if its ID does not match the most
            // recent config-request we sent.
            //
            if (id != pFsm->idTxCR)
            {
                // Stale response, ignore it
                DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - RX Stale response id=%u, expecting %u\n"), id, pFsm->idTxCR));
                break;
            }

            switch(curState)
            {
            case PFS_Closed:
            case PFS_Stopped:
                PppFsmActionSTA(pFsm, id);
                break;

            case PFS_Opened:
                PppFsmActionTLD(pFsm);
                // FALLTHROUGH
            case PFS_Ack_Received:
                //
                // We are in a state in which we are not expecting a response.
                //
                DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - Unexpected response received\n")));
                pFsm->idTxCR++;
                if (PppFsmActionSCR(pFsm) == NO_ERROR)
                    newState = PFS_Request_Sent;
                else
                {
                    PppFsmActionSTR(pFsm);
                    newState = PFS_Stopping;
                }
                break;

            case PFS_Request_Sent:
            case PFS_Ack_Sent:
                //
                // All improperly formed responses must be ignored, and not
                // result in a state transition.
                //
                if (code == PPP_CONFIGURE_ACK)
                {
                    //
                    //  An ACK packet must be identical to the CR we issued,
                    //  except for the 1st byte (which is ACK(2) instead of CR(1)
                    //
                    DWORD len = (pPacket[2] << 8) | pPacket[3];

					if ((DWORD)(len -1) > (DWORD)((pPacket[2] << 8) | pPacket[3])) { // no overflow
					    dwResult = ERROR_INVALID_MESSAGE;
                        break;
						}

                    if (pFsm->cbRequestBuffer != len ||
                        memcmp(pFsm->pRequestBuffer + 1, pPacket + 1, len - 1))
                    {
                        // Invalid ACK - ignore
                        DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - ACK received with nonmatching option list\n")));
                        dwResult = ERROR_INVALID_MESSAGE;
                        break;
                    }
                }
                dwResult = PppFsmProcessRxConfigureResponse(pFsm, pPacket, cbPacket);
                if (dwResult == ERROR_UNRECOGNIZED_RESPONSE && code == PPP_CONFIGURE_ACK)
                {
                    //
                    // Treat the malformed ACK packet as a NAK.
                    // This is specifically designed to deal with a misbehaving peer
                    // that sends a MPPE ACK with more than 1 encryption type enabled.
                    // The CCP code will adjust the next CR sent.
                    //
                    dwResult = NO_ERROR;
                    code = PPP_CONFIGURE_NAK;
                }
                if (dwResult == NO_ERROR)
                {
                    // Valid response
                    PppFsmActionIRC(pFsm, pFsm->dwMaxConfigure);
                    if (code == PPP_CONFIGURE_ACK)
                        newState = curState == PFS_Request_Sent ? PFS_Ack_Received : PFS_Opened;
                    else
                    {
                        pFsm->idTxCR++;
                        if (PppFsmActionSCR(pFsm) != NO_ERROR)
                            newState = PFS_Stopped;
                    }
                }
                break;
            }
            break;

        case PPP_TERMINATE_REQUEST:
            switch(curState)
            {
            case PFS_Closed:
            case PFS_Closing:
            case PFS_Stopped:
            case PFS_Stopping:
            case PFS_Request_Sent:
                PppFsmActionSTA(pFsm, id);
                break;

            case PFS_Ack_Received:
            case PFS_Ack_Sent:
                PppFsmActionSTA(pFsm, id);
                newState = PFS_Request_Sent;
                break;

            case PFS_Opened:
                PppFsmActionTLD(pFsm);
                PppFsmActionZRC(pFsm);
                PppFsmActionSTA(pFsm, id);
                newState = PFS_Stopping;
                break;
            }
            break;

        case PPP_TERMINATE_ACK:
            switch (pFsm->state)
            {
            case PFS_Closing:
                newState = PFS_Closed;
                break;

            case PFS_Stopping:
                newState = PFS_Stopped;
                break;

            case PFS_Ack_Received:
                // This is per RFC1661, but seems wierd that
                // we enter the Request_Sent state without
                // sending a new request...
                newState = PFS_Request_Sent;
                break;

            case PFS_Opened:
                PppFsmActionTLD(pFsm);
                pFsm->idTxCR++;
                if (PppFsmActionSCR(pFsm) == NO_ERROR)
                    newState = PFS_Request_Sent;
                else
                    newState = PFS_Stopped;
                break;
            }
            break;

        case PPP_CODE_REJECT:
            //
            // We must be running a newer version of the protocol,
            // such that the peer does not understand the code.
            //
            // If the rejected code is for a required message type
            // like PPP_CONFIGURE_REQUEST, this is catastrophic
            // and we need to take down the link.
            //
            // Regardless, log the bad message the peer doesn't like,
            // stop sending that message type.
            //

            break;

        default:
            //
            // Search the list of protocol specific code handlers
            //
            pMsgDescriptor = pFsm->pDescriptor->pExtensionMessageDescriptors;
            if (pMsgDescriptor)
            {
                while (TRUE)
                {
                    if (pMsgDescriptor->MessageRxCb == NULL)
                    {
                        // Didn't find a matching handler
                        pMsgDescriptor = NULL;
                        break;
                    }
                    if (pMsgDescriptor->code == code)
                    {
                        break;
                    }
                    pMsgDescriptor++;
                }
            }

            if (pMsgDescriptor)
            {
                //
                // Invoke the extension message handler
                //
                pMsgDescriptor->MessageRxCb(pFsm->CbContext, code, id, pPacket + 4, length - 4);
            }
            else
            {
                //
                // Unrecognized code - RUC
                PppFsmActionSendCodeReject(pFsm, pPacket, cbPacket);
            }
            break;
        }

        PppFsmNewState(pFsm, newState);

    } while (FALSE); // end do
}

