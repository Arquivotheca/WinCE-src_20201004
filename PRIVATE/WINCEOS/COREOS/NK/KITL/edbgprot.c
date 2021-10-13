//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    KITLprot.c
    
Abstract:  
    Routines for kernel debug services over Ethernet (KITL).  This file contains
    the core functions to implement the KITL protocol over UDP.
    
Functions:


Notes: 

--*/
#include <windows.h>
#include <types.h>
#include <nkintr.h>
#include <kitl.h>
#include <kitlprot.h>
#include "kernel.h"
#include "kitlp.h"

DWORD SC_WaitForMultiple(DWORD cObjects, CONST HANDLE *lphObjects, BOOL fWaitAll, DWORD dwTimeout);

// time to retransmit if using ticks
#define MIN_POLL_TIME   1000
#define MAX_POLL_TIME   8000

// time to retransmit if using iterations
#define MIN_POLL_ITER   10000
#define MAX_POLL_ITER   1600000

static UCHAR PollRecvBuf[KITL_MTU];

static BOOL ProcessRecvFrame(UCHAR *pFrame, WORD wMsgLen, BOOL fUseSysCalls, PFN_TRANSMIT pfnTransmit, LPVOID pData);
static void KITLDecodeFrame(char *LeadingText, KITL_HDR *pHdr, WORD wLen);
static BOOL ProcessAdminMsg(KITL_HDR *pHdr, WORD wMsgLen, BOOL fUseSysCalls, PFN_TRANSMIT pfnTransmit, LPVOID pData);

extern CRITICAL_SECTION KITLODScs;
extern CRITICAL_SECTION KITLKCallcs;
extern KITLTRANSPORT Kitl;
extern BOOL IsDesktopDbgrExist (void);
extern BOOL KernelIoctl (DWORD , LPVOID , DWORD , LPVOID , DWORD , LPDWORD );


extern DWORD g_dwKeys[];
extern CHAR* g_keyNames[];


/* SetClientEvent
 *
 *   Adjust proc permissions to match client and set event
 */
void SetClientEvent(KITL_CLIENT *pClient, HANDLE hEvent)
{
    DWORD OldProcPerms;
    if (pClient->ProcPerms) {
        SWITCHKEY (OldProcPerms, pClient->ProcPerms);
    }
    if (hEvent && !SC_EventModify(hEvent, EVENT_SET)) {
        KITL_DEBUGMSG(ZONE_ERROR,("!SetClientEvent:Error %u setting event %X..\n",GetLastError(),hEvent));
    }
    if (pClient->ProcPerms) {
        SETCURKEY (OldProcPerms);
    }
}

/* ResetClientState
 *
 *   Reset all state associated with client struct.
 *   Must hold client CS while calling this function.
 */
void
ResetClientState(KITL_CLIENT *pClient)
{
    // When system is coming up, we can't make any system calls. 
    UCHAR UseSysCalls = (pClient->State & KITL_USE_SYSCALLS);
    
    // Cancel any timers
    CancelTimersForClient(pClient);
    
    // Reset our state
    pClient->TxSeqNum = 0;
    pClient->AckExpected = 0;
    pClient->RxSeqNum = 0;
    pClient->RxWinEnd = pClient->WindowSize;
    memset(&pClient->TxFrameLen,0,sizeof(pClient->TxFrameLen));
    memset(&pClient->RxFrameLen,0,sizeof(pClient->RxFrameLen));
    pClient->NextRxIndex = 0;
    pClient->State &= ~KITL_SYNCED;

    // Unblock any threads which might be stuck
    if (UseSysCalls) {
        SetClientEvent(pClient,pClient->evTxFull);
        // For now, let recv threads keep blocking. We might want to return an error
        // here, investigate later.
//        SetEvent(pClient->evRecv);
    }
}

static BOOL KITLPollData(BOOL fUseSysCalls, PFN_TRANSMIT pfnTransmit, LPVOID pData)
{
    LPBYTE pRecvBuf = PollRecvBuf;
    
    if (fUseSysCalls && (KITLGlobalState & KITL_ST_MULTITHREADED)
        && !(pRecvBuf = _alloca(KITL_MTU))) {
        KITLOutputDebugString("!KITLPollData: STACK OVERFLOW!\r\n");
        return FALSE;
    }
    HandleRecvInterrupt(pRecvBuf, fUseSysCalls, pfnTransmit, pData);
    return TRUE;
}


static BOOL CheckCfg (KITL_CLIENT *pClient)
{
    return !(pClient->State & KITL_WAIT_CFG);
}

static BOOL RetransmitCfg (KITL_CLIENT *pClient, BOOL fUseSysCalls)
{
    return SendConfig (pClient, FALSE);
}

/* ExchangeConfig
 *
 *   If we have peer address (e.g. for default services, we get this info from eshell at boot),
 *   send config message to peer, indicating the service ID and window size to use.  Otherwise,
 *   do passive connect (desktop app will get device address from eshell, then send us a config
 *   message).
 */
BOOL ExchangeConfig(KITL_CLIENT *pClient)
{
    // When system is coming up, we can't make any system calls. 
    BOOL fUseSysCalls = (pClient->State & KITL_USE_SYSCALLS)? TRUE : FALSE; 

    if (!(pClient->State & KITL_WAIT_CFG))
        return 0 != (pClient->State & KITL_CLIENT_REGISTERED);
    
    KITL_DEBUGMSG(ZONE_INIT,("Waiting for service '%s' to connect..., fUseSysCalls = %d\n", pClient->ServiceName, fUseSysCalls));

    if (fUseSysCalls && (KITLGlobalState & KITL_ST_INT_ENABLED)) { 
        DWORD Timeout = MIN_POLL_TIME;
    
        while (pClient->State & KITL_WAIT_CFG) {

            if (!SendConfig (pClient, FALSE)) {
                return FALSE;
            }

            switch (SC_WaitForMultiple (1, &pClient->evCfg, 0, Timeout)) {
            case WAIT_OBJECT_0:
                return (0 != (pClient->State & KITL_CLIENT_REGISTERED));
                
            case WAIT_TIMEOUT:
                if (Timeout < MAX_POLL_TIME)
                    Timeout <<= 1;
                break;
                
            default:
                KITLOutputDebugString("!ExchangeConfig: WaitForMultipleObjects failed\n");
                return FALSE;
            }
            
        }
        
    } else {

        if (!SendConfig (pClient, FALSE))
            return FALSE;
            
        KITLPollResponse (fUseSysCalls, CheckCfg, RetransmitCfg, pClient);

    }
    return 0 != (pClient->State & KITL_CLIENT_REGISTERED);
}

BOOL SendConfig(KITL_CLIENT *pClient, BOOL fIsResp)
{
    UCHAR *FmtBuf;
    KITL_HDR *pHdr;
    KITL_SVC_CONFIG_DATA *pConfigData;

    FmtBuf = (UCHAR *) _alloca (Kitl.FrmHdrSize   // for transport header
        + KITL_DATA_OFFSET                        // for protocol header
        + sizeof(KITL_SVC_CONFIG_DATA)            // for data
        + Kitl.FrmTlrSize);                       // for transport tailer
    if (!FmtBuf) {
        KITLOutputDebugString ("!SendConfig: Stack overflow\r\n");
        return FALSE;
    }
    pHdr = (KITL_HDR *)(FmtBuf + Kitl.FrmHdrSize);   // pointer to the  protocol header
    pConfigData = (KITL_SVC_CONFIG_DATA *) KITLDATA(pHdr);   // pointer to the data

    // Format protocol header
    pHdr->Id = KITL_ID;
    pHdr->Service = KITL_SVC_ADMIN;
    pHdr->Flags = KITL_FL_FROM_DEV;
    if (fIsResp)
        pHdr->Flags |= KITL_FL_ADMIN_RESP;
    pHdr->SeqNum = 0;
    pHdr->Cmd = KITL_CMD_SVC_CONFIG;

    // Format service config message
    pConfigData->ProtocolVersion = CURRENT_KITL_VERSION;
    strcpy(pConfigData->ServiceName,pClient->ServiceName);
    pConfigData->ServiceId = pClient->ServiceId;
    pConfigData->WindowSize = pClient->WindowSize;
    pConfigData->Flags = pClient->CfgFlags;
    
    if (KITLDebugZone & ZONE_FRAMEDUMP)
        KITLDecodeFrame(">>SendConfig",pHdr,KITL_DATA_OFFSET + sizeof(KITL_SVC_CONFIG_DATA));
    if (!KitlSendFrame (FmtBuf, KITL_DATA_OFFSET + sizeof(KITL_SVC_CONFIG_DATA))) {
        KITLOutputDebugString("!SendConfig: Error in KITLSendUDP\n");
        return FALSE;
    }
    return TRUE;
}

BOOL KITLPollResponse (BOOL fUseSysCalls, PFN_CHECK pfnCheck, PFN_TRANSMIT pfnTransmit, LPVOID pData)
{
    DWORD dwLoopCnt = 0, dwLoopMax = MIN_POLL_ITER;
    DWORD dwStartTime = CurMSec;
    int   nTimeMax = MIN_POLL_TIME;  // start with 1 sec
    BOOL  fUseIter = FALSE, fUseTick = FALSE;

    while (!pfnCheck (pData)) {
        //
        // if we've already connected with desktop, use the desktop
        // "Retransmit" package to determine if we need to retransmit
        //
        if (!(KITLGlobalState & KITL_ST_DESKTOP_CONNECTED)) {
            if (fUseTick) {
                if ((int) (CurMSec - dwStartTime) > nTimeMax) {
                    // retransmit
                    if (!pfnTransmit (pData, fUseSysCalls))
                        return FALSE;
                    dwStartTime = CurMSec;
                    if (nTimeMax < MAX_POLL_TIME)
                        nTimeMax <<= 1;
                }
            } else if (fUseIter || (dwStartTime == CurMSec)) {
                // if time isn't moving for a while, we'll
                // use iteration.
                if (dwLoopCnt ++ > dwLoopMax) {
                    if (!pfnTransmit (pData, fUseSysCalls))
                        return FALSE;
                    if (dwLoopMax < MAX_POLL_ITER)
                        dwLoopMax <<= 1;
                    dwLoopCnt = 0;
                    fUseIter = TRUE;
                }
            } else {
                // time is moving, just use tick from here
                fUseTick = TRUE;
            }
        }
        if (!KITLPollData(fUseSysCalls, pfnTransmit, pData)) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL ChkAck (KITL_CLIENT *pClient)
{
    return pClient->AckExpected == pClient->TxSeqNum;
}

static BOOL TranAck (KITL_CLIENT *pClient, BOOL fUseSysCalls)
{
    UCHAR Seq;
    for (Seq = pClient->AckExpected; Seq != pClient->TxSeqNum; SEQ_INC (Seq))
        RetransmitFrame (pClient, (UCHAR) (Seq % pClient->WindowSize), fUseSysCalls);
    return TRUE;
}

/*  @doc   KITL
 *  @func  BOOL | KITLSend | Send a message over debug Ethernet interface.
 *  @rdesc Return TRUE if successful, FALSE if error occurrs.
 *  @comm  Send a message to peer KITL client.  Returns immediately unless the transmit
 *         window is full, or if we're in polling mode, in which case this function will 
 *         block.  Once sent, the message will be retried by the KITL subsystem until 
 *         it is acknowledged by other side.
 *  @xref  <f KITLRegisterClient> <f KITLRecv>
 */
static HANDLE hDbgMsgThread;
extern HANDLE hIntrThread;
extern HANDLE hTimerThread;
BOOL
KITLSend(
    UCHAR Id,               // @parm [IN] - KITL client id (returned from <f KITLRegisterClient>)
    UCHAR *pUserData,       // @parm [IN] - Data to send.
    DWORD dwUserDataLen )   // @parm [IN] - Data length (up to KITL_MAX_DATA_SIZE).
{
    KITL_CLIENT *pClient = KITLClients[Id];
    PKITL_HDR pHdr;
    UCHAR *pFrame, *pData;
    BOOL fRet = TRUE;
    DWORD dwPacketIndex;
    UCHAR PrevSyscallState;
    BOOL  fRestoreClientState = FALSE, fRestoreGlobalState = FALSE;
    BOOL  fUseSysCalls;

    if (!IS_VALID_ID(Id))
        return FALSE;

    // Check that client is registered
    if (!pClient || (pClient->ServiceId != Id))
        return FALSE;

    // establish connection with desktop
    if (!(pClient->State & KITL_CLIENT_REGISTERED) && !ExchangeConfig (pClient))
        return FALSE;

    if (dwUserDataLen > KITL_MAX_DATA_SIZE) {
        KITL_DEBUGMSG(ZONE_WARNING,("!KITLSend: Buffer too large\n"));
        return FALSE;
    }

    // check if the data overlapped with buffer pool, reject if it does
    if ((pUserData < pClient->pTxBufferPool + pClient->WindowSize * KITL_MTU)
        && (pUserData + dwUserDataLen > pClient->pTxBufferPool)) {
        KITLOutputDebugString ("!KITLSend: Data to be send overlapped with buffer pools\r\n");
        return FALSE;
    }

    // Check for re-entrancy -- e.g. if kernel debug zones are turned on
    if (pClient->ServiceId == KITL_SVC_DBGMSG ) {
        if (((hCurThread == hDbgMsgThread) || (hCurThread == hIntrThread) || (hCurThread == hTimerThread))
            && (KITLGlobalState & KITL_ST_MULTITHREADED)) {
            if (!(KITLDebugZone & KITL_ZONE_NOSERIAL)) {
                KITL_DBGMSG_INFO *pDbgMsg = (KITL_DBGMSG_INFO *)pUserData;
                KITLOutputDebugString("!KITLSend(DBGMSG): Reentrant call, hTh:%X\n",hCurThread);
                KITLOutputDebugString("%s\n",pUserData + pDbgMsg->dwLen);
            }
            return TRUE;
        }
        else
            hDbgMsgThread = hCurThread;
    }

    // Determine whether we can make system calls or not. There are three cases where we cannot:
    //     o  During system init, until KITLInitializeInterrupt is called
    //     o  When we are called from within a system call
    //     o  When we are called from the debugger
    if (KITLGlobalState & KITL_ST_MULTITHREADED) {
        // If we're in a sys call, can't block on anything (but won't be preempted)
        if (InSysCall()) {
            KITL_DEBUGMSG(ZONE_WARNING,("KITLSend(%u): InSysCall()\n",Id));
            fUseSysCalls = FALSE;
            fRestoreClientState = TRUE;
            PrevSyscallState = pClient->State & KITL_USE_SYSCALLS;
            pClient->State &= ~KITL_USE_SYSCALLS;
                    
            KITLGlobalState &= ~KITL_ST_MULTITHREADED;
            fRestoreGlobalState = TRUE;

            // The protocol state machine can't handle us stepping in at some arbitrary point
            // and mucking with state, so if someone owns the CS, just bail and print to the
            // debug serial port.
            if (pClient->ClientCS.OwnerThread || KITLKCallcs.OwnerThread) {
                KITL_DEBUGMSG(ZONE_WARNING,("!KITLSend(%u): In syscall with ClientCS owned (KCallcs = 0x%x)",Id, KITLKCallcs.OwnerThread));
                if ((pClient->ServiceId == KITL_SVC_DBGMSG) && !(KITLDebugZone & KITL_ZONE_NOSERIAL)) {
                    KITL_DBGMSG_INFO *pDbgMsg = (KITL_DBGMSG_INFO *)pUserData;
                    KITLOutputDebugString("%s",pUserData + pDbgMsg->dwLen);
                }
                fRet = FALSE;
                goto KITLSend_exit;
            }
        }
        else if (pClient->State & KITL_USE_SYSCALLS) {
            // Normal case
            fUseSysCalls = TRUE;
        }
        else {
            // This client cannot use system calls, and guarantees that scheduling is disabled while
            // it is called into the KITL functions. Currently, this only applies to KDBG client.
            if (Id != KITL_SVC_KDBG) {
                KITL_DEBUGMSG(ZONE_WARNING,("!KITLSend(%u): KITL_USE_SYSCALLS for client turned off\n",Id)); 
            }

            fUseSysCalls = FALSE;
            KITLGlobalState &= ~KITL_ST_MULTITHREADED;
            fRestoreGlobalState = TRUE;
        }
    }
    else {
        // Single threaded and non preemptible
        fUseSysCalls = FALSE;
    }

    if (fUseSysCalls)        
        EnterCriticalSection(&pClient->ClientCS);

    KITL_DEBUGMSG(ZONE_SEND,("+KITLSend(%u), Len: %u, Seq: %u, AckExpected: %u, SC:%u\n",
                             Id, dwUserDataLen,pClient->TxSeqNum, pClient->AckExpected,fUseSysCalls));
    // If Tx window full, block
#ifdef DEBUG
    if (SEQ_DELTA(pClient->AckExpected, pClient->TxSeqNum) >= pClient->WindowSize) {
        KITL_DEBUGMSG(ZONE_WARNING,("KITLSend(%u): Tx window full (AckExpected: %u, Seq: %u) SC:%u\n",
                                    Id,pClient->AckExpected,pClient->TxSeqNum,fUseSysCalls));
    }
#endif
    while ( SEQ_DELTA(pClient->AckExpected, pClient->TxSeqNum) >= pClient->WindowSize) {
        if (fUseSysCalls) {
            SC_EventModify(pClient->evTxFull, EVENT_RESET);
            LeaveCriticalSection(&pClient->ClientCS);
            // Some platforms don't have interrupts
            if (KITLGlobalState & KITL_ST_INT_ENABLED) {
        WaitAgain:
                if (SC_WaitForMultiple (1,&pClient->evTxFull,0,KITL_TIMEOUT_INTERVAL_MS) != WAIT_OBJECT_0) {
                    KITLOutputDebugString("0x%x: KITLSend(%u): Timed out waiting for ack (AckExpected: %u, TxSeq: %u)\n", hCurThread, Id,
                                          pClient->AckExpected,pClient->TxSeqNum);
                    // For default services, don't give up
                    if (IS_DFLT_SVC(Id))
                        goto WaitAgain;
                    fRet = FALSE;
                    goto KITLSend_exit;
                }
                if (!(pClient->State & KITL_CLIENT_REGISTERED)) {
                    // client de-registered, return FALSE
                    KITL_DEBUGMSG(ZONE_WARNING,("KITLSend(%u): Client de-registered while sending\n", Id));
                    fRet = FALSE;
                    goto KITLSend_exit;
                }
            }
            else {
                if (!KITLPollData(TRUE, NULL, NULL)) {
                    fRet = FALSE;
                    goto KITLSend_exit;
                }
                // Give retransmit thread chance to run
                if (SEQ_DELTA(pClient->AckExpected, pClient->TxSeqNum) >= pClient->WindowSize)
                    SC_Sleep(1);
            }
            EnterCriticalSection(&pClient->ClientCS);
        } else {
            // we're InSysCall or in KD, Poll until window closes
            KITLPollResponse (FALSE, ChkAck, TranAck, pClient);
        }
    }
    
    // Format frame into Tx buffer (leave space for transport headers)
    dwPacketIndex = pClient->TxSeqNum % pClient->WindowSize;
    pFrame = pClient->pTxBufferPool + (dwPacketIndex * KITL_MTU);
    pHdr = (PKITL_HDR) (pFrame + Kitl.FrmHdrSize);
    pHdr->Id = KITL_ID;
    pHdr->Service = pClient->ServiceId;
    pHdr->Flags = KITL_FL_FROM_DEV;
    pHdr->Cmd = KITL_CMD_SVC_DATA;
    pHdr->SeqNum = pClient->TxSeqNum;
    SEQ_INC(pClient->TxSeqNum);

    pData = (UCHAR *)pHdr + KITL_DATA_OFFSET;
    
    // Copy user data into buffer
    memcpy(pData, pUserData, dwUserDataLen);

    pClient->TxFrameLen[dwPacketIndex] = (USHORT)(dwUserDataLen + KITL_DATA_OFFSET);

    if (KITLDebugZone & ZONE_FRAMEDUMP)
        KITLDecodeFrame(">>KITLSend",pHdr,pClient->TxFrameLen[dwPacketIndex]);
    
    if (!(fRet = KitlSendFrame (pFrame, pClient->TxFrameLen[dwPacketIndex])))
        KITLOutputDebugString("!KITLSend: Error in KITLSendUDP\n");
    else {
        if (!fUseSysCalls)
            fRet = KITLPollResponse (FALSE, ChkAck, TranAck, pClient);
        else {
            // Start retransmit timer
            TimerStart(pClient,(UCHAR)dwPacketIndex,KITL_RETRANSMIT_INTERVAL_MS,TRUE);
            // If we're in stop and wait mode, wait for ack here. Also need to do this
            // if our interrupt isn't enabled.
            if ((pClient->CfgFlags & KITL_CFGFL_STOP_AND_WAIT) ||
                !(KITLGlobalState & KITL_ST_INT_ENABLED)) {
                SC_EventModify(pClient->evTxFull, EVENT_RESET);
                LeaveCriticalSection(&pClient->ClientCS);

                if (KITLGlobalState & KITL_ST_INT_ENABLED) {
                    while (SC_WaitForMultiple (1,&pClient->evTxFull, 0, KITL_TIMEOUT_INTERVAL_MS) == WAIT_TIMEOUT) {
                        KITL_DEBUGMSG(ZONE_WARNING,("KITLSend: Id: %u Timed out waiting for ack (AckExpected: %u, TxSeq: %u)\n",Id,
                                                    pClient->AckExpected,pClient->TxSeqNum));
                    }
                    if (!(pClient->State & KITL_CLIENT_REGISTERED)) {
                        // client de-registered, return FALSE
                        KITL_DEBUGMSG(ZONE_WARNING,("KITLSend(%u): Client de-registered while sending\n", Id));
                        fRet = FALSE;
                        goto KITLSend_exit;
                    }
                    
                }
                else {
                    fRet = KITLPollResponse (TRUE, ChkAck, TranAck, pClient);
                }
                EnterCriticalSection(&pClient->ClientCS);
            }
            if (!fRet) 
                TimerStop(pClient,(UCHAR)dwPacketIndex,TRUE);
        }
    }
    if (!fRet) {
        // Restore state
        pClient->TxSeqNum = SEQ_SUB1(pClient->TxSeqNum);
        pClient->TxFrameLen[dwPacketIndex] = 0;
    }

    if (fUseSysCalls)
        LeaveCriticalSection(&pClient->ClientCS);
KITLSend_exit:

    if (pClient->ServiceId == KITL_SVC_DBGMSG)
        hDbgMsgThread = 0;
    
    if (fRestoreClientState)
        pClient->State |= PrevSyscallState;
    
    if (fRestoreGlobalState)
        KITLGlobalState |= KITL_ST_MULTITHREADED;
    
    KITL_DEBUGMSG(ZONE_SEND,("-Send (%u): Ret %u\n",Id,fRet));
    return fRet;
}

/* RetransmitFrame
 *
 *   Retransmit specified frame in Tx buffer, either due to a timeout waiting
 *   for an ack, or a NACK received from the peer.
 *
 * Return Value:
 *   Return TRUE if the timer should be rescheduled, FALSE if not.  We only return
 *   FALSE if the retransmitted frame does not fall in the Tx window.  This can
 *   happen if we get called in a sys call, and so cannot touch the timer structs
 *   (since can't get the CS).
 */
BOOL
RetransmitFrame(KITL_CLIENT *pClient, UCHAR Index, BOOL fUseSysCalls)
{
    UCHAR *pData = pClient->pTxBufferPool + (Index * KITL_MTU);
    DWORD OldProcPerms;
    BOOL  fRet = TRUE;
    KITL_HDR *pHdr;

    pHdr = (KITL_HDR *)(pData + Kitl.FrmHdrSize);

    if (fUseSysCalls && (pClient->State & KITL_USE_SYSCALLS))
        EnterCriticalSection(&pClient->ClientCS);
    
    // Set permissions for accessing client buffer, if necessary
    if (pClient->ProcPerms) {
        SWITCHKEY (OldProcPerms, pClient->ProcPerms);
    }

    if (!SEQ_BETWEEN(pClient->AckExpected, pHdr->SeqNum, pClient->TxSeqNum)) {
        KITL_DEBUGMSG(ZONE_WARNING,("!RetransmitFrame: Frame seq %u not in Tx window (%u,%u)\n",
                                    pHdr->SeqNum,pClient->AckExpected,pClient->TxSeqNum));
        fRet = FALSE; // Don't reschedule timer
    } else {
        if (KITLDebugZone & ZONE_FRAMEDUMP)
            KITLDecodeFrame(">>Retransmit",(KITL_HDR *)(pData+Kitl.FrmHdrSize),pClient->TxFrameLen[Index]);
        KITL_DEBUGMSG(ZONE_RETRANSMIT,("RetransmitFrame(Id:%u), Seq: %u, Len: %u, Window (%u,%u)\n",pClient->ServiceId,
                                       pHdr->SeqNum,pClient->TxFrameLen[Index],pClient->AckExpected,pClient->TxSeqNum));
        if (!KitlSendRawData (pData, (USHORT) (pClient->TxFrameLen[Index] + Kitl.FrmHdrSize + Kitl.FrmTlrSize))) {
            KITLOutputDebugString("!RetransmitFrame: Error in KITLSendUDP\n");
            // Go ahead and return TRUE so frame will be tried again.
        }
    }
    if (pClient->ProcPerms) {
        SETCURKEY (OldProcPerms);
    }
    if (fUseSysCalls && (pClient->State & KITL_USE_SYSCALLS))
        LeaveCriticalSection(&pClient->ClientCS);
    return fRet;
}



/* SendAckNack
 *
 *   Send an ACK or NACK frame to peer:
 *      ACK:   Acknowledge receipt of all frames up to SeqNum
 *      NACK:  Request retransmission of frame specified by SeqNum
 */
static void
SendAckNack(BOOL IsAck, KITL_CLIENT *pClient, UCHAR SeqNum)
{
    UCHAR *AckNackBuf;
    KITL_HDR *pHdr;

    AckNackBuf = _alloca (Kitl.FrmHdrSize // for transport header
        + KITL_DATA_OFFSET                // for protocol header
        + Kitl.FrmTlrSize);               // for transport tailer

    if (!AckNackBuf) {
        KITLOutputDebugString ("!SendAckNack: Stack overflow\r\n");
        return;
    }
    pHdr = (KITL_HDR *)(AckNackBuf + Kitl.FrmHdrSize);
    pHdr->Id       = KITL_ID;
    pHdr->Service  = pClient->ServiceId;    // must use id of the other side
    pHdr->Flags    = (IsAck? KITL_FL_ACK:KITL_FL_NACK)|KITL_FL_FROM_DEV;
    pHdr->Cmd      = KITL_CMD_SVC_DATA;
    pHdr->SeqNum   = SeqNum;

    if (KITLDebugZone & ZONE_FRAMEDUMP)
        KITLDecodeFrame(">>SendAckNack ",pHdr,KITL_DATA_OFFSET);
    if (!KitlSendFrame (AckNackBuf, KITL_DATA_OFFSET))
        KITLOutputDebugString("!KITL: Error in KITLSendUDP for ACK\n");
}


/* 
 * @func  BOOL | KITLRecv | Receive message over debug Ethernet interface.
 * @rdesc Return TRUE if successful, FALSE if error occurrs.
 * @comm  Receive message from peer KITL client.  If no received data
 *        is available for the specified service, block until data is ready, or
 *        timeout occurs.
 *        Note: If the caller does not provide a big enough buffer to hold the data, 
 *        the buffer is filled, and remaining data is tossed.  Don't have to worry 
 *        about this by providing a buffer of KITL_MAX_DATA_SIZE bytes.
 *  @xref  <f KITLRegisterClient> <f KITLSend> 
 */ 
BOOL
KITLRecv(
    UCHAR Id,        // @parm [IN] - KITL client id (returned from <f KITLRegisterClient>)
    UCHAR *pRecvBuf, // @parm [OUT]- Buffer to receive data
    DWORD *pdwLen,   // @parm [IN] - Buffer size. [OUT] - Bytes of data received.
    DWORD Timeout)   // @parm [IN] - Timeout value (0 == don't block, INFINITE == wait forever)
{
    KITL_CLIENT *pClient = KITLClients[Id];
    DWORD dwStartTime;
    DWORD dwBytesToCopy;
    UCHAR PrevSyscallState;
    BOOL  fRestoreClientState = FALSE, fRestoreGlobalState = FALSE;
    BOOL  fUseSysCalls;
    BOOL  fRet = TRUE;
    
    if (!IS_VALID_ID(Id))
        return FALSE;

    // Check that client is registered
    if (!pClient || (pClient->ServiceId != Id)) {
        KITLOutputDebugString ("\r\n!KITLRecv: Invalid client ID %u!!!!!\r\n",Id);
        return FALSE;
    }

    // establish connection with desktop
    if (!(pClient->State & KITL_CLIENT_REGISTERED) && !ExchangeConfig (pClient)) {
        KITL_DEBUGMSG (ZONE_WARNING, ("\r\n!KITLRecv: Client (%d) not registered!\r\n", Id));
        return FALSE;
    }

    // check if the data overlapped with buffer pool, reject if it does
    if ((pRecvBuf < pClient->pRxBufferPool + pClient->WindowSize * KITL_MTU)
        && (pRecvBuf + *pdwLen > pClient->pRxBufferPool)) {
        KITLOutputDebugString ("!KITLRecv: Receive buffer overlapped with buffer pools\r\n");
        return FALSE;
    }

    // Determine whether we can make system calls or not. There are three cases where we cannot:
    //     o  During system init, until KITLInitializeInterrupt is called
    //     o  When we are called from within a system call (this case actually should never happen,
    //        since the only service called in a syscall is debug messages, which shouldn't call recv).
    //     o  When we are called from the debugger
    if (KITLGlobalState & KITL_ST_MULTITHREADED) {
        // If we're in a sys call, can't block on anything (but won't be preempted)
        if (InSysCall()) {
            KITL_DEBUGMSG(ZONE_WARNING,("KITLRecv(%u): InSysCall()\n",Id));
            fUseSysCalls = FALSE;
            fRestoreClientState = TRUE;
            PrevSyscallState = pClient->State & KITL_USE_SYSCALLS;
            pClient->State &= ~KITL_USE_SYSCALLS;
                    
            KITLGlobalState &= ~KITL_ST_MULTITHREADED;
            fRestoreGlobalState = TRUE;

            // The protocol state machine can't handle us stepping in at some arbitrary point
            // and mucking with state, so if someone owns the CS, just bail and print to the
            // debug serial port.
            if (pClient->ClientCS.OwnerThread) {
                KITL_DEBUGMSG(ZONE_WARNING,("!KITLRecv(%u): In syscall with ClientCS owned",Id));
                fRet = FALSE;
                goto KITLRecv_exit;
            }
        }
        else if (pClient->State & KITL_USE_SYSCALLS) {
            fUseSysCalls = TRUE;
        }
        else {
            // This client cannot use system calls, and guarantees that scheduling is disabled while
            // it is called into the KITL functions. Currently, this only applies to KDBG client.
            if (Id != KITL_SVC_KDBG) {
                KITL_DEBUGMSG(ZONE_WARNING,("!KITLRecv(%u): SYSCALLS for client turned off\n",Id)); 
            }
            fUseSysCalls = FALSE;
            KITLGlobalState &= ~KITL_ST_MULTITHREADED;
            fRestoreGlobalState = TRUE;
        }
    }
    else {
        // Single threaded and non preemptible
        fUseSysCalls = FALSE;
    }

    dwStartTime = CurMSec;
    if (Timeout != INFINITE)
        Timeout *= 1000;

    if (fUseSysCalls)
        EnterCriticalSection(&pClient->ClientCS);
    
    KITL_DEBUGMSG(ZONE_RECV,("+KITLRecv(%u): Waiting for message (expect Seq %u). User bufsize: %u, SC:%u\n",
                             Id,pClient->RxSeqNum,*pdwLen,fUseSysCalls));
                             
    while (pClient->RxFrameLen[pClient->NextRxIndex] == 0) {
        if (fUseSysCalls) {
            LeaveCriticalSection (&pClient->ClientCS);
        }
        
        if (!fUseSysCalls || !(KITLGlobalState & KITL_ST_INT_ENABLED)) {
            // polling mode or debugger
            if (!KITLPollData(fUseSysCalls, NULL, NULL)) {
                fRet = FALSE;
                goto KITLRecv_exit;
            }
            
            if ((pClient->RxFrameLen[pClient->NextRxIndex] == 0)
                && (Timeout != INFINITE)
                && (!Timeout  || ((int) (CurMSec - dwStartTime) >= (int) Timeout))) {
                if (Timeout)
                    KITL_DEBUGMSG(ZONE_WARNING,("KITL(%u): Timed out polling for recv data\n",Id));

                fRet = FALSE;
                goto KITLRecv_exit;
            }
            
        } else if (SC_WaitForMultiple (1, &pClient->evRecv, 0, (Timeout == INFINITE)? INFINITE : Timeout) != WAIT_OBJECT_0) {
            KITL_DEBUGMSG(ZONE_WARNING,("KITL(%u): Timed out waiting for recv data\n",Id));
            fRet = FALSE;
            goto KITLRecv_exit;
        }
        
        if (!(pClient->State & KITL_CLIENT_REGISTERED)) {
            // client de-registered, return an error
            KITL_DEBUGMSG(ZONE_WARNING,("KITL(%u): Client de-registered while waiting to receive data\n",Id));
            fRet = FALSE;
            goto KITLRecv_exit;
        }
        if (fUseSysCalls) {
            EnterCriticalSection(&pClient->ClientCS);   
        }
    }

    // Copy received data into buffer
    dwBytesToCopy = min(pClient->RxFrameLen[pClient->NextRxIndex], *pdwLen);
    KITL_DEBUGMSG(ZONE_RECV,("-KITLRecv(%u): Read %u bytes from slot %u, copying %u\n",
                             Id,pClient->RxFrameLen[pClient->NextRxIndex],pClient->NextRxIndex,dwBytesToCopy));
    memcpy(pRecvBuf, pClient->pRxBufferPool+(pClient->NextRxIndex*KITL_MTU),dwBytesToCopy);
    *pdwLen = dwBytesToCopy;
    pClient->RxFrameLen[pClient->NextRxIndex] = 0;
    pClient->NextRxIndex = (pClient->NextRxIndex+1) % pClient->WindowSize;

    // Send ack if required
    if (!(pClient->CfgFlags & KITL_CFGFL_NOACKS))
        SendAckNack(TRUE,pClient, (UCHAR)(pClient->RxWinEnd - pClient->WindowSize));
    
    // Advance top of Rx window
    SEQ_INC(pClient->RxWinEnd);
    if (fUseSysCalls)
        LeaveCriticalSection(&pClient->ClientCS);

KITLRecv_exit:
    if (fRestoreClientState)
        pClient->State |= PrevSyscallState;
    if (fRestoreGlobalState)
        KITLGlobalState |= KITL_ST_MULTITHREADED;
    if (fRet == FALSE) {
        KITL_DEBUGMSG(ZONE_WARNING,("\nKITL(%u): EdbgRecv returning FALSE!!!!!!\n",Id));
    }
    return fRet;
}


/* HandleRecvInterrupt
 *
 *   Called to process a receive interrupt.  Is also called in polling mode, to
 *   check for and handle received frames.  Buffer must be able to hold an Ethernet
 *   KITL_MTU (1500 bytes) of data.
 */
void HandleRecvInterrupt(UCHAR *pRecvBuf, BOOL fUseSysCalls, PFN_TRANSMIT pfnTransmit, LPVOID pData)
{
    WORD wLen = KITL_MTU;
    BOOL fFrameRecvd;

    // Receive data into buffer
    do {
        if (!fUseSysCalls)
            fFrameRecvd = Kitl.pfnRecv (pRecvBuf, &wLen);
        else if (IsDesktopDbgrExist ())
            fFrameRecvd = KCall((PKFN) Kitl.pfnRecv, pRecvBuf, &wLen);
        else {
            EnterCriticalSection (&KITLKCallcs);
            fFrameRecvd = Kitl.pfnRecv (pRecvBuf, &wLen);
            LeaveCriticalSection (&KITLKCallcs);
        }
        if (fFrameRecvd) {
            ProcessRecvFrame (pRecvBuf,wLen,fUseSysCalls, pfnTransmit, pData);
            wLen = KITL_MTU;
        }
    } while (fFrameRecvd);
}

// Frame received from the wire, with all protocol headers attached
static BOOL ProcessRecvFrame(UCHAR *pFrame, WORD wMsgLen, BOOL fUseSysCalls, PFN_TRANSMIT pfnTransmit, LPVOID pData)
{
    KITL_HDR *pMsg;
    KITL_CLIENT *pClient = NULL;
    BOOL fRet = TRUE;
    UCHAR RxBufOffset;
    WORD  wDataLen;
    UCHAR ClientIdx;
    // let the transport layer decode the frame
    if (!(pMsg = (KITL_HDR *) Kitl.pfnDecode (pFrame, &wMsgLen))) {
        KITL_DEBUGMSG(ZONE_RECV, ("ProcessRecvFrame: Received Unhandled frame\n"));
        return FALSE;
    }

    // is it a valid KITL message?
    if (pMsg->Id != KITL_ID) {
        KITL_DEBUGMSG(ZONE_WARNING,("KITL: Got unrecognized Id: %X\r\n",pMsg->Id));
        return FALSE;
    }
    
    // Validate length
    if (wMsgLen < KITL_DATA_OFFSET) {
        KITL_DEBUGMSG(ZONE_WARNING,("KITL: Invalid length %u\n",wMsgLen));
        return FALSE;
    }
    if (KITLDebugZone & ZONE_FRAMEDUMP)
        KITLDecodeFrame("<<KITLRecv",pMsg, wMsgLen);
    
    // Check for administrative messages
    if (pMsg->Service == KITL_SVC_ADMIN)
        return ProcessAdminMsg(pMsg, wMsgLen, fUseSysCalls, pfnTransmit, pData);

    // Service Id is index into KITLClients array
    ClientIdx = pMsg->Service;
    if (ClientIdx >= MAX_KITL_CLIENTS) {
        KITL_DEBUGMSG(ZONE_WARNING,("!ProcessKITLMsg: Invalid ServiceId: %u\n",pMsg->Service));
        return FALSE;
    }

    pClient = KITLClients[ClientIdx];
    
    // Until we complete registering, only handle administrative messages
    if (!pClient || !(pClient->State & KITL_CLIENT_REGISTERED)) {
        KITL_DEBUGMSG(ZONE_WARNING,("!ProcessKITLMsg: Client %u not registered\n",ClientIdx));
        return FALSE;
    }
    if (pMsg->Service != pClient->ServiceId) {
        KITL_DEBUGMSG(ZONE_WARNING,("!ProcessKITLMsg: Mismatch in service Id for Client %u (Got %u, expect %u)\n",
                                    ClientIdx,pMsg->Service,pClient->ServiceId));
        return FALSE;
    }

    if (pClient->State & KITL_USE_SYSCALLS) {
        if (fUseSysCalls)  
            EnterCriticalSection(&pClient->ClientCS);
        else if (pClient->ClientCS.OwnerThread) {
            // We can't get the client CS, and it is owned - just toss frame
            KITL_DEBUGMSG(ZONE_WARNING,("!KITL(%u) tossing msg %u (Can't get CS)\n",ClientIdx, pMsg->SeqNum));
            return FALSE;
        }
    }

    // we've being in sync with the desktop
    pClient->State |= KITL_SYNCED;
    
    // Put flags and seq # to LEDs
    KITL_DEBUGLED(LED_PEM_SEQ, ((DWORD) pMsg->Flags << 24) | pMsg->SeqNum);
    
    // OK, valid message, see if it's an ACK
    if (pMsg->Flags & KITL_FL_ACK) {
        KITL_DEBUGMSG(ZONE_RECV,("KITL(%u): Received ack for msg %u, Tx window: %u,%u\n",
                                 ClientIdx,pMsg->SeqNum, pClient->AckExpected,pClient->TxSeqNum));
        // ACKs acknowledge all data up to the ACK sequence #
        while (SEQ_BETWEEN(pClient->AckExpected, pMsg->SeqNum, pClient->TxSeqNum)) {        
            if ((pClient->State & KITL_USE_SYSCALLS) &&
                ((pClient->CfgFlags & KITL_CFGFL_STOP_AND_WAIT) ||
                 (SEQ_DELTA(pClient->AckExpected, pClient->TxSeqNum) >= pClient->WindowSize-1) ||
                 !(KITLGlobalState & KITL_ST_INT_ENABLED))) {
                if (fUseSysCalls)
                    SetClientEvent(pClient,pClient->evTxFull);
                else {
                    // Can't process message at this time...
                    KITL_DEBUGMSG(ZONE_WARNING,("!KITL(%u): Tossing ACK %u (Can't set event)\n",
                                                ClientIdx, pMsg->SeqNum));
                    return FALSE;
                }
            }
            // Stop retransmission timer. 
            TimerStop(pClient, (UCHAR)(pClient->AckExpected % pClient->WindowSize),fUseSysCalls);
            SEQ_INC(pClient->AckExpected);
        }
        goto ProcessKITLMsg_exit;
    }

    // Handle NACKs - retransmit requested frame if it is in our Tx window
    if (pMsg->Flags & KITL_FL_NACK) {
        KITL_DEBUGMSG(ZONE_WARNING,("KITL(%u): Received NACK for msg %u, Tx window: %u,%u\n",
                                    ClientIdx,pMsg->SeqNum, pClient->AckExpected,pClient->TxSeqNum)); 
        if (SEQ_BETWEEN(pClient->AckExpected, pMsg->SeqNum, pClient->TxSeqNum)) {
            UCHAR Index = pMsg->SeqNum % pClient->WindowSize;
            if (pClient->TxFrameLen[Index]) {
                // Restart retransmission timer (note we can't start timers if syscalls
                // are disabled, but this shouldn't screw us up, we'll just potentially
                // retransmit an extra frame if the timer fires before we get the ACK)
                if (fUseSysCalls)
                    TimerStop(pClient,Index,fUseSysCalls);
                RetransmitFrame(pClient, Index, fUseSysCalls);
                if (fUseSysCalls)
                    TimerStart(pClient,Index,KITL_RETRANSMIT_INTERVAL_MS,fUseSysCalls);
            }
            else
                KITL_DEBUGMSG(ZONE_WARNING,("!KITL(%u): NACK in window, but TxFrameLen empty!\n",ClientIdx));
        }
        else
            KITL_DEBUGMSG(ZONE_WARNING,("!KITL(%u): Received NACK outside of TX window: Seq: %u, Window: %u,%u\n",
                                        ClientIdx,pMsg->SeqNum,pClient->AckExpected,pClient->TxSeqNum));
        goto ProcessKITLMsg_exit;
    }
    
    // Data frame.  Place in appropriate slot in Rx buffer pool. Note that we defer acking
    // received frames until they are read from the buffer, in KITLRecv.
    RxBufOffset = pMsg->SeqNum % pClient->WindowSize;

    if (! SEQ_BETWEEN(pClient->RxSeqNum, pMsg->SeqNum, pClient->RxWinEnd)) {
        UCHAR uLastACK = (UCHAR) (pClient->RxWinEnd - pClient->WindowSize - 1);

        KITL_DEBUGMSG (ZONE_WARNING, ("KITL(%u): Received msg outside window: Seq:%u, Win:%u,%u\n",
                              ClientIdx,pMsg->SeqNum,pClient->RxSeqNum,pClient->RxWinEnd));

        // Special case to handle lost ACKs - if an ack is dropped, our Rx window will have
        // advanced beyond the seq # of the retransmitted frame.  Since ACKs ack all messages
        // up to the ack #, we only need to check the last frame. 
        if (pMsg->SeqNum == uLastACK) {
            KITL_DEBUGMSG(ZONE_WARNING,("KITL(%u): Lost ACK (seq: %u, win: %u,%u)\n",ClientIdx,
                                        pMsg->SeqNum,uLastACK,pClient->RxWinEnd));
            SendAckNack (TRUE, pClient, uLastACK);
        }
    } else if (pClient->RxFrameLen[RxBufOffset] != 0) {
        // If all our buffers are full, toss frame (will be acked when data is read in KITLRecv)
        KITL_DEBUGMSG(ZONE_WARNING,("KITL(%u): Received duplicate (Seq:%u), slot %u already full. Win: %u,%u\n",
                                    ClientIdx,pMsg->SeqNum,RxBufOffset,pClient->RxSeqNum,pClient->RxWinEnd));
    } else {
        DWORD OldProcPerms;

        // If we're in non-preemptible mode, can't set the receive event, so just toss message
        // and wait for retry.
        if (!fUseSysCalls && (pClient->State & KITL_USE_SYSCALLS)) {
            KITL_DEBUGMSG(ZONE_WARNING,("KITL(%u): Tossing frame %u (Can't signal Rx event)\n",
                                        ClientIdx,pMsg->SeqNum));
            return FALSE;
        }

        KITL_DEBUGMSG(ZONE_RECV,("KITL(%u): Received frame Seq: %u, len: %u, putting in slot %u\n",
                                 ClientIdx, pMsg->SeqNum, wMsgLen, RxBufOffset));
        // If frames were dropped, send NACK (only allow one outstanding NACK)
        if (pMsg->SeqNum != pClient->RxSeqNum) {
            KITL_DEBUGMSG(ZONE_WARNING,("!KITL(%u): Dropped frame (seq: %u, win: %u,%u)\n",
                                        ClientIdx,pMsg->SeqNum,pClient->RxSeqNum, pClient->RxWinEnd));

            if (!(pClient->State & KITL_NACK_SENT)) {
                SendAckNack(FALSE, pClient, pClient->RxSeqNum);
                pClient->State |= KITL_NACK_SENT;           
            }
        }
        else
            pClient->State &= ~KITL_NACK_SENT;
        
        // Copy data to receive buffer, unblock anyone waiting, and close receive window
        wDataLen = wMsgLen - (WORD)KITL_DATA_OFFSET;
        if (wDataLen == 0)
            KITL_DEBUGMSG(ZONE_WARNING,("!KITL: Received data message with 0 length!\n"));
        if (pClient->ProcPerms) {
            // acquire permission of pClient and add it to current thread
            ACCESSKEY aKey = GETCURKEY() | pClient->ProcPerms;
            SWITCHKEY (OldProcPerms, aKey);
        }
        memcpy(pClient->pRxBufferPool + RxBufOffset*KITL_MTU,KITLDATA(pMsg), wDataLen);
        if (pClient->ProcPerms) {
            SETCURKEY (OldProcPerms);            
        }
        pClient->RxFrameLen[RxBufOffset] = wDataLen;

        if (pClient->State & KITL_USE_SYSCALLS)
            // If we get here, we know that fUseSysCalls is TRUE
            SetClientEvent(pClient,pClient->evRecv);
        
        // Close receive window
        while (pClient->RxFrameLen[pClient->RxSeqNum % pClient->WindowSize] &&
               (SEQ_DELTA(pClient->RxSeqNum, pClient->RxWinEnd) >= 1)) {
            KITL_DEBUGMSG(ZONE_RECV,("Rx win: %u,%u, usesyscalls: %u\n",pClient->RxSeqNum, pClient->RxWinEnd, fUseSysCalls));
            SEQ_INC(pClient->RxSeqNum);
        }
    }
    
ProcessKITLMsg_exit:

    if (fUseSysCalls && (pClient->State & KITL_USE_SYSCALLS))
        LeaveCriticalSection(&pClient->ClientCS);
    
    return fRet;
}

static BOOL ProcessAdminMsg(KITL_HDR *pHdr, WORD wMsgLen, BOOL fUseSysCalls, PFN_TRANSMIT pfnTransmit, LPVOID pData)
{
    KITL_CLIENT *pClient = NULL;    
    
    switch (pHdr->Cmd)
    {
        case KITL_CMD_SVC_CONFIG:
        {
            KITL_SVC_CONFIG_DATA *pCfg = (KITL_SVC_CONFIG_DATA *) KITLDATA (pHdr);
            int i, iStart;
            
            if (wMsgLen != (KITL_DATA_OFFSET + sizeof(KITL_SVC_CONFIG_DATA))) {
                KITL_DEBUGMSG(ZONE_WARNING,("!ProcessAdminMsg: Invalid legnth for CONFIG msg: %u\n",wMsgLen));
                return FALSE;
            }

            // Find client struct
            if ((i = ChkDfltSvc (pCfg->ServiceName)) < 0)
                i = HASH(pCfg->ServiceName[0]);

            iStart = i;

            while (KITLClients[i]) {
                // For multi instanced services, skip clients that are already registered
                if (!strcmp(KITLClients[i]->ServiceName,pCfg->ServiceName) && 
                    (!(KITLClients[i]->State & KITL_CLIENT_REGISTERED) || !(KITLClients[i]->CfgFlags & KITL_CFGFL_MULTIINST))) {
                    pClient = KITLClients[i];
                    break;
                }
                if (i < NUM_DFLT_KITL_SERVICES)
                    // no dups for default services
                    break;

                if (MAX_KITL_CLIENTS == ++ i)
                    i = NUM_DFLT_KITL_SERVICES;

                if (iStart == i)
                    break;  // couldn't find a client
            }

            if (!pClient || !(pClient->State & (KITL_CLIENT_REGISTERING|KITL_CLIENT_REGISTERED))) {
                KITL_DEBUGMSG(ZONE_WARNING,("!Received config for unrecognized service %s\n",
                                            pCfg->ServiceName));
                return TRUE;
            }

            if (fUseSysCalls)
                EnterCriticalSection(&pClient->ClientCS);

            // Send config to peer, unless this was a response to our cmd
            if (!(pHdr->Flags & KITL_FL_ADMIN_RESP)) {
                // ack this config message
                SendConfig(pClient,TRUE);

                // Stop any pending transfers, reset sequence #s, etc

                // WARNING - can cause lost transmit data if the other side doesn't get
                // our config, and retries the config command.
                if (pClient->State & KITL_SYNCED) {
                    ResetClientState(pClient);
                }
            }

            //
            // we got the response from desktop, connecting the client
            //
            KITL_DEBUGMSG(ZONE_INIT, ("ProcessAdminMsg: Receive Config message for service %s\n", pClient->ServiceName));
            pClient->State &= ~(KITL_WAIT_CFG|KITL_CLIENT_REGISTERING);
            pClient->State |= KITL_CLIENT_REGISTERED;
            // Set our event in case anyone is waiting for config info
            if (fUseSysCalls)
                SetClientEvent(pClient,pClient->evCfg);

            
            if (fUseSysCalls)            
                LeaveCriticalSection(&pClient->ClientCS);
            break;
        }
        case KITL_CMD_RESET:
            {
                KITL_RESET_DATA *pResetData =  (KITL_RESET_DATA *) KITLDATA (pHdr);

                KITLOutputDebugString("KITL: Got RESET command\n");

                // Set for clean boot if requested
                if (pResetData->Flags & KITL_RESET_CLEAN) 
                    SC_SetCleanRebootFlag();
                
                // This function never returns
                KernelIoctl(IOCTL_HAL_REBOOT, NULL,0,NULL,0,NULL);
                KITLOutputDebugString("KITL: IOCTL_HAL_REBOOT not supported on this platform\n");
                break;
            }

        case KITL_CMD_DEBUGBREAK:
            if (fUseSysCalls && IsDesktopDbgrExist ())
                DebugBreak ();
            break;

        case KITL_CMD_TRAN_CONFIG:
            {
                int i;
                
                PKITL_HOST_TRANSCFG pCfg = (PKITL_HOST_TRANSCFG) KITLDATA(pHdr);
                wMsgLen -= KITL_DATA_OFFSET;
                if (pCfg->dwLen != wMsgLen) {
                    KITLOutputDebugString ("!Host config message size mismatch %d, %d\r\n", pCfg->dwLen, wMsgLen);
                    return FALSE;
                }
                wMsgLen -= sizeof (KITL_HOST_TRANSCFG);
                if (!Kitl.pfnSetHostCfg ((LPBYTE) (pCfg+1), wMsgLen))
                    return FALSE;
                Kitl.dwBootFlags = pCfg->dwFlags;

                if (pCfg->dwKeySig == HOST_TRANSCFG_KEYSIG) {
                    for (i = 0; i < HOST_TRANSCFG_NUM_REGKEYS; i++) {
                        g_dwKeys[i] = pCfg->dwKeys[i];
                        KITL_DEBUGMSG (ZONE_INIT, (" KeyIndex %d = %d \n", i, g_dwKeys[i]));
                    }
                }    
                KITLGlobalState |= KITL_ST_DESKTOP_CONNECTED;
            }
            break;

        // in case we're polling (pfnTransmit && pData only set to non-null if we're polling)
        // we'll use desktop as our timer (desktop sends a retransmit packet to us every 2 seconds).
        case KITL_CMD_RETRASMIT:
            if (pfnTransmit && pData) {
                // KITLOutputDebugString ("Retrasmitting packets....\n");
                pfnTransmit (pData, fUseSysCalls);
            }
            break;
        default:
            KITL_DEBUGMSG(ZONE_WARNING,("!ProcessAdminMsg: Unhandled command 0x%X\n",pHdr->Cmd));
            return FALSE;
    }
    return TRUE;
}

static void
KITLDecodeFrame(char *LeadingText, KITL_HDR *pHdr, WORD wLen)
{
    // Watch for reentrancy - we may be calling in to do a debug message from within
    // EnterCriticalSection() for example.
    BOOL fUseSysCalls = (KITLGlobalState & KITL_ST_MULTITHREADED) && !InSysCall() && (KITLODScs.OwnerThread != hCurThread);
    LARGE_INTEGER liPerfCnt;

    if (fUseSysCalls) {
        DWORD ms;
        EnterCriticalSection(&KITLODScs);
        SC_QueryPerformanceCounter(&liPerfCnt);
        ms = liPerfCnt.LowPart/1000;
        // Print out ms count
        KITLOutputDebugString("(%u.%u) ",ms/1000,ms%1000);
    }
    if (LeadingText)
        KITLOutputDebugString(LeadingText);

    switch (pHdr->Service)
    {
        case KITL_SVC_DBGMSG:
            KITLOutputDebugString(" DBGMSG ");
            break;
        case KITL_SVC_PPSH:
            KITLOutputDebugString(" PPSH ");
            break;
        case KITL_SVC_KDBG:
            KITLOutputDebugString(" KDBG ");
            break;
        case KITL_SVC_ADMIN:
            KITLOutputDebugString(" ADMIN ");
            
            switch (pHdr->Cmd)
            {
                case KITL_CMD_BOOTME:
                    KITLOutputDebugString("BOOTME ");
                    break;
                case KITL_CMD_JUMPIMG:
                    KITLOutputDebugString("JUMPIMG ");
                    break;
                case KITL_CMD_RESET:
                    KITLOutputDebugString("RESET ");
                    break;
                case KITL_CMD_SVC_CONFIG:
                    KITLOutputDebugString("CFG");
                    if (pHdr->Flags & KITL_FL_FROM_DEV) {
                        KITL_SVC_CONFIG_DATA *pCfg = (KITL_SVC_CONFIG_DATA *)((UCHAR *)pHdr+KITL_DATA_OFFSET);
                        KITLOutputDebugString("DEV %s, ProtoVer:%u, Service: %s, Id: %u, Window: %u, Fl:0x%X",
                                              (pHdr->Flags & KITL_FL_ADMIN_RESP)? "RESP":"CMD",
                                              pCfg->ProtocolVersion,pCfg->ServiceName,pCfg->ServiceId,
                                              pCfg->WindowSize,pCfg->Flags);
                    }
                    else {
                        KITL_SVC_CONFIG_DATA *pCfg = (KITL_SVC_CONFIG_DATA *)((UCHAR *)pHdr+KITL_DATA_OFFSET);
                        KITLOutputDebugString("DESK %s, ProtoVer:%u, Service: %s, Id: %u",
                                              (pHdr->Flags & KITL_FL_ADMIN_RESP)? "RESP":"CMD",
                                              pCfg->ProtocolVersion,pCfg->ServiceName, pCfg->ServiceId);
                    }
                    break;
                case KITL_CMD_RETRASMIT:
                    KITLOutputDebugString("RETRANSMIT ");
                    break;
                default:
                    KITLOutputDebugString("Unrecognized cmd: 0x%X ",pHdr->Cmd);
                    break;
            }
            break;
            
        default:
            KITLOutputDebugString(" Svc:0x%X",pHdr->Service);
            break;
    }

    if (pHdr->Service != KITL_SVC_ADMIN) {
        KITLOutputDebugString("  Seq:%u(0x%X), Flags:0x%X ",pHdr->SeqNum,pHdr->SeqNum,pHdr->Flags);
        if (pHdr->Flags) {
            KITLOutputDebugString("(");
            if (pHdr->Flags & KITL_FL_ACK)
                KITLOutputDebugString("ACK ");
            if (pHdr->Flags & KITL_FL_NACK)
                KITLOutputDebugString("NACK ");
            if (pHdr->Flags & KITL_FL_FROM_DEV)
                KITLOutputDebugString("DEV ");
            KITLOutputDebugString(")");
        }
    }
    KITLOutputDebugString(", DataLen: %u\n",wLen - KITL_DATA_OFFSET);
    if (fUseSysCalls)
        LeaveCriticalSection(&KITLODScs);    
}


//
// KitlSendRawData:
//      calling the transport Send Function, with the necessary protection
//
BOOL KitlSendRawData (LPBYTE pbData, WORD wLength)
{
    BOOL fRet;
    if (!(KITLGlobalState & KITL_ST_MULTITHREADED) || InSysCall())
        fRet = Kitl.pfnSend (pbData, wLength);
    else if (IsDesktopDbgrExist ())
        fRet = KCall((PKFN) Kitl.pfnSend, pbData, wLength);
    else {
        EnterCriticalSection (&KITLKCallcs);
        fRet = Kitl.pfnSend (pbData, wLength);
        LeaveCriticalSection (&KITLKCallcs);
    }
    return fRet;
}

// NOTE: pFrame is pointing to the start of the frame, cbData is ONLY the size of the data
//       NOT the size of the frame
//          ----------------
//          | Frame Header |
//          |--------------|
//          |     Data     |        // of size cbData
//          |--------------|
//          | Frame Tailer |
//          |--------------|
BOOL KitlSendFrame (LPBYTE pbFrame, WORD cbData)
{
    if (!Kitl.pfnEncode (pbFrame, cbData)) {
        KITLOutputDebugString ("!KitlSendFrame: transport failed to encode the data frame\r\n");
        return FALSE;
    }

    return KitlSendRawData (pbFrame, (USHORT) (cbData + Kitl.FrmHdrSize + Kitl.FrmTlrSize));
}


