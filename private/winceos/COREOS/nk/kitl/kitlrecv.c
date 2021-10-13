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
#include <windows.h>
#include <kitl.h>
#include <kitlprot.h>
#include "kernel.h"
#include "kitlp.h"

static void HandleACK (KITL_CLIENT *pClient, PKITL_HDR pMsg, WORD wMsgLen)
{
    BOOL fInSysCall = InSysCall ();
    
    // toss ACK if we can't make API call (in kcall), and the client use API call to synchronize
    if (fInSysCall && (pClient->State & KITL_USE_SYSCALLS)) {
        KITL_DEBUGMSG(ZONE_RECV, ("KITL(%u): Toss ack for msg %u, Tx window: %u,%u due to syscall state\n",
                                 pClient->ServiceId, pMsg->SeqNum, pClient->AckExpected,pClient->TxSeqNum));
    } else {

        BOOL fTxFull = (SEQ_DELTA(pClient->AckExpected, pClient->TxSeqNum) >= pClient->WindowSize-1)
                    && (KITLGlobalState & KITL_ST_INT_ENABLED);
        
        DEBUGCHK (!fInSysCall || !fTxFull);
        
        KITL_DEBUGMSG(ZONE_RECV,("KITL(%u): Received ack for msg %u, Tx window: %u,%u\n",
                                 pClient->ServiceId, pMsg->SeqNum, pClient->AckExpected,pClient->TxSeqNum));

        // ACKs acknowledge all data up to the ACK sequence #
        while (SEQ_BETWEEN(pClient->AckExpected, pMsg->SeqNum, pClient->TxSeqNum)) {        
            // Stop retransmission timer if we're not in syscall
            if (!fInSysCall) {
                TimerStop (pClient, (UCHAR)(pClient->AckExpected % pClient->WindowSize));
            }
            SEQ_INC (pClient->AckExpected);
        }

        if (fTxFull) {
            DEBUGCHK (!fInSysCall);
            KITLSetEvent (pClient->phdTxFullEvt);
        }
    }
}

static BOOL HandleNACK (KITL_CLIENT *pClient, PKITL_HDR pMsg, WORD wMsgLen)
{
    BOOL fRet = FALSE;
    KITL_DEBUGMSG (ZONE_RECV,("KITL(%u): Received NACK for msg %u, Tx window: %u,%u\n",
                        pClient->ServiceId, pMsg->SeqNum, pClient->AckExpected,pClient->TxSeqNum)); 
    
    if (SEQ_BETWEEN (pClient->AckExpected, pMsg->SeqNum, pClient->TxSeqNum)) {
        UCHAR Index = pMsg->SeqNum % pClient->WindowSize;
        
        if (pClient->TxFrameLen[Index]) {
            fRet = RetransmitFrame (pClient, Index);
        } else {
            KITL_DEBUGMSG (ZONE_WARNING,("!KITL(%u): NACK in window, but TxFrameLen empty!\n", pClient->ServiceId));
        }
    } else {
        KITL_DEBUGMSG (ZONE_WARNING,("!KITL(%u): Received NACK outside of TX window: Seq: %u, Window: %u,%u\n",
                        pClient->ServiceId, pMsg->SeqNum,pClient->AckExpected,pClient->TxSeqNum));
    }

    return fRet;
}


/* SendAckNack
 *
 *   Send an ACK or NACK frame to peer:
 *      ACK:   Acknowledge receipt of all frames up to SeqNum
 *      NACK:  Request retransmission of frame specified by SeqNum
 */
#pragma warning(push)
#pragma warning(disable:4995)
// Ideally we want to declare a max KITL pack on stack. However, as KITL can be running on kernel 
// stack where the size is very limited (<6k), we don't want to grow the stack unless nessary.
// the use of _alloca here is to reduce stack usage.

static void
SendAckNack(BOOL IsAck, KITL_CLIENT *pClient, UCHAR SeqNum)
{
    UCHAR *AckNackBuf = SendFmtBuf;
    KITL_HDR *pHdr;

    if (!InSysCall ()) {
        AckNackBuf = _alloca (Kitl.FrmHdrSize // for transport header
            + sizeof(KITL_HDR)                // for protocol header
            + Kitl.FrmTlrSize);               // for transport tailer

        if (!AckNackBuf) {
            KITLOutputDebugString ("!SendAckNack: Stack overflow\r\n");
            return;
        }
    }
    pHdr = (KITL_HDR *)(AckNackBuf + Kitl.FrmHdrSize);
    pHdr->Id       = KITL_ID;
    pHdr->Service  = pClient->ServiceId;    // must use id of the other side
    pHdr->Flags    = (IsAck? KITL_FL_ACK:KITL_FL_NACK)|KITL_FL_FROM_DEV;
    pHdr->Cmd      = KITL_CMD_SVC_DATA;
    pHdr->SeqNum   = SeqNum;

    if (KITLDebugZone & ZONE_FRAMEDUMP)
        KITLFrameDump(">>SendAckNack ", pHdr, sizeof(KITL_HDR));
    if (!KitlSendFrame (AckNackBuf, sizeof(KITL_HDR)))
        KITLOutputDebugString("!KITL: Error in KitlSendFrame for ACK\n");
}
#pragma warning(pop)


static BOOL HandleLostACK (KITL_CLIENT *pClient, PKITL_HDR pMsg, WORD wMsgLen)
{
    UCHAR uLastACK = (UCHAR) (pClient->RxWinEnd - pClient->WindowSize - 1);
    
    KITL_DEBUGMSG (ZONE_RECV, ("KITL(%u): Received msg outside window: Seq:%u, Win:%u,%u\n",
                        pClient->ServiceId, pMsg->SeqNum, pClient->RxSeqNum,pClient->RxWinEnd));
    
    // Special case to handle lost ACKs - if an ack is dropped, our Rx window will have
    // advanced beyond the seq # of the retransmitted frame.  Since ACKs ack all messages
    // up to the ack #, we only need to check the last frame. 
    if (pMsg->SeqNum == uLastACK) {
        KITL_DEBUGMSG(ZONE_RECV,("KITL(%u): Lost ACK (seq: %u, win: %u,%u)\n",
                        pClient->ServiceId, pMsg->SeqNum,uLastACK,pClient->RxWinEnd));
        SendAckNack (TRUE, pClient, uLastACK);
    }
    return TRUE;
}
static BOOL HandleNewDataPacket (KITL_CLIENT *pClient, PKITL_HDR pMsg, WORD wMsgLen)
{
    // If we're in non-preemptible mode, only handle KD packets. Toss any other
    // packets and wait for retry.
    BOOL fRet = !InSysCall () || (KITL_SVC_KDBG == pClient->ServiceId);

    if (fRet) {
        UCHAR   RxBufOffset = pMsg->SeqNum % pClient->WindowSize;
        
        KITL_DEBUGMSG(ZONE_RECV,("KITL(%u): Received frame Seq: %u, len: %u, putting in slot %u\n",

        pClient->ServiceId, pMsg->SeqNum, wMsgLen, RxBufOffset));
        // If frames were dropped, send NACK (only allow one outstanding NACK)
        if (pMsg->SeqNum != pClient->RxSeqNum) {
            KITL_DEBUGMSG(ZONE_RECV,("!KITL(%u): Dropped frame (seq: %u, win: %u,%u)\n",
                        pClient->ServiceId, pMsg->SeqNum,pClient->RxSeqNum, pClient->RxWinEnd));
        
            if (!(pClient->State & KITL_NACK_SENT)) {
                SendAckNack (FALSE, pClient, pClient->RxSeqNum);
                pClient->State |= KITL_NACK_SENT;           
            }
        } else {
            pClient->State &= ~KITL_NACK_SENT;
        }
        
        // Copy data to receive buffer, unblock anyone waiting, and close receive window
        wMsgLen -= sizeof(KITL_HDR);
        if (!wMsgLen) {
            KITL_DEBUGMSG(ZONE_WARNING,("!KITL: Received data message with 0 length!\n"));
        }

        memcpy (pClient->pRxBufferPool + RxBufOffset * KITL_MTU, KITLDATA(pMsg), wMsgLen);
        
        pClient->RxFrameLen[RxBufOffset] = wMsgLen;
        
        if (!InSysCall ()) {
            KITLSetEvent (pClient->phdRecvEvt);
        }
        
        // Close receive window
        while (     pClient->RxFrameLen[pClient->RxSeqNum % pClient->WindowSize]
                &&  (SEQ_DELTA(pClient->RxSeqNum, pClient->RxWinEnd) >= 1)) {
            KITL_DEBUGMSG(ZONE_RECV,("Rx win: %u,%u\n",pClient->RxSeqNum, pClient->RxWinEnd));
            SEQ_INC(pClient->RxSeqNum);
        }
    }
    return fRet;
}

static void ProcessClientMsg (PKITL_HDR pMsg, WORD wMsgLen)
{
    UCHAR           ClientIdx   = pMsg->Service;
    KITL_CLIENT     *pClient    = KITLClients[ClientIdx];
    
    // Service Id is index into KITLClients array
    if (   (ClientIdx >= MAX_KITL_CLIENTS)
        || !pClient 
        || !(pClient->State & KITL_CLIENT_REGISTERED)
        || (ClientIdx != pClient->ServiceId)) {
        KITL_DEBUGMSG(ZONE_WARNING,("!ProcessClientMsg: Invalid ServiceId: %u\n",pMsg->Service));
        
    } else if ((ClientIdx == KITL_SVC_KDBG) && !InSysCall ()) {
        KITL_DEBUGMSG(ZONE_WARNING,("ProcessClientMsg: KD packet received while not in syscall - tossed (seq = %d)\n",pMsg->SeqNum));
        
    } else if (KITLTryAcquireClient (pClient)) {

        UCHAR   RxBufOffset = pMsg->SeqNum % pClient->WindowSize;
        BOOL    fUseSysCall = UseSysCall (pClient);

        if (fUseSysCall) {
            EnterCriticalSection (&pClient->ClientCS);
        } 
        
        // we've being in sync with the desktop
        pClient->State |= KITL_SYNCED;
        
        // Put flags and seq # to LEDs
        KITL_DEBUGLED(LED_PEM_SEQ, ((DWORD) pMsg->Flags << 24) | pMsg->SeqNum);
        
        
        if (pMsg->Flags & KITL_FL_ACK) {
            // handle ACK packet
            HandleACK (pClient, pMsg, wMsgLen);
            
        } else if (pMsg->Flags & KITL_FL_NACK) {
            // handle NACK packet
            HandleNACK (pClient, pMsg, wMsgLen);
        
        } else if (!SEQ_BETWEEN(pClient->RxSeqNum, pMsg->SeqNum, pClient->RxWinEnd)) {
            // handle lost ACK
            HandleLostACK (pClient, pMsg, wMsgLen);
        
        } else if (!pClient->RxFrameLen[RxBufOffset]) {
            // new data packet
            HandleNewDataPacket (pClient, pMsg, wMsgLen);
        
        } else {
            // duplicate
            KITL_DEBUGMSG (ZONE_RECV,("KITL(%u): Received duplicate (Seq:%u), slot %u already full. Win: %u,%u\n",
                            pClient->ServiceId, pMsg->SeqNum, RxBufOffset, pClient->RxSeqNum, pClient->RxWinEnd));
        }
    
        if (fUseSysCall) {
            LeaveCriticalSection (&pClient->ClientCS);
        }

        KITLReleaseClient(pClient);
    } else {
        KITL_DEBUGMSG(ZONE_WARNING, ("ProcessClientMsg: Received message on a closing stream(%d).  Tossing. (seq = %d)\n",
                                     pClient->ServiceId, pMsg->SeqNum));
    }
    
}

BOOL HandleRecvInterrupt (LPBYTE pRecvBuf)
{
    PKITL_HDR pHdr;
    WORD      wMsgLen = KITL_MTU;
    BOOL      fRetransmit = FALSE;

    for(;;) {
        pHdr = KitlRecvFrame(pRecvBuf, &wMsgLen);
        if(pHdr == NULL) {
            break;
        }
        if (KITL_SVC_ADMIN != pHdr->Service) {
            ProcessClientMsg (pHdr, wMsgLen);
            
        } else if (ProcessAdminPacket (pHdr, wMsgLen)) {
            // retransmit requested from desktop
            fRetransmit = TRUE;
            break;
        }
        wMsgLen = KITL_MTU;
    }

    // potentially reset / debug break if one is pending
    HandleResetOrDebugBreak ();
    
    return fRetransmit;
}

static void ConsumeKITLPacket (KITL_CLIENT *pClient, LPVOID pRecvBuf, LPDWORD pdwLen)
{
    // Copy received data into buffer
    DWORD dwBytesToCopy = min(pClient->RxFrameLen[pClient->NextRxIndex], *pdwLen);
    
    DEBUGCHK (InSysCall () || OwnCS (&pClient->ClientCS));

    memcpy(pRecvBuf, pClient->pRxBufferPool+(pClient->NextRxIndex*KITL_MTU),dwBytesToCopy);
    *pdwLen = dwBytesToCopy;
    pClient->RxFrameLen[pClient->NextRxIndex] = 0;
    pClient->NextRxIndex = (pClient->NextRxIndex+1) % pClient->WindowSize;
    
    // Send ack if required
    if (!(pClient->CfgFlags & KITL_CFGFL_NOACKS))
        SendAckNack (TRUE, pClient, (UCHAR)(pClient->RxWinEnd - pClient->WindowSize));
    
    // Advance top of Rx window
    SEQ_INC(pClient->RxWinEnd);
}



__declspec(noinline) // Too much KStack is used if this is inlined
static BOOL
DoKITLRecv(
    UCHAR       Id,
    UCHAR       *pRecvBuf, // @parm [OUT]- Buffer to receive data
    DWORD       *pdwLen,   // @parm [IN] - Buffer size. [OUT] - Bytes of data received.
    DWORD       Timeout)   // @parm [IN] - Timeout value (0 == don't block, INFINITE == wait forever)
{
    BOOL        dwErr    = 0;
    KITL_CLIENT *pClient = KITLClients[Id];
    BYTE        PollBuf[KITL_MTU];

    DWORD dwStartTime = CurMSec;

#if 1
    if (Timeout != INFINITE)
        Timeout *= 1000;
#endif

    EnterCriticalSection (&pClient->ClientCS);

    KITL_DEBUGMSG(ZONE_RECV,("+KITLRecv(%u): Waiting for message (expect Seq %u). User bufsize: %u\n",
                             pClient->ServiceId, pClient->RxSeqNum, *pdwLen));

    while (!pClient->RxFrameLen[pClient->NextRxIndex]) {

        LeaveCriticalSection (&pClient->ClientCS);

        if (KITLGlobalState & KITL_ST_INT_ENABLED) {
            // interrupt mode, block on receive event
            if (WAIT_TIMEOUT == DoWaitForObjects (1, &pClient->phdRecvEvt, Timeout)) {
                // interrupt mode, block on receive event
                dwErr = ERROR_TIMEOUT;
                break;
            }

        } else {
            // polling mode, poll for data

            // check for timeout
            if (   (Timeout != INFINITE)
                && (!Timeout || ((int) (CurMSec - dwStartTime) >= (int) Timeout))) {
                dwErr = ERROR_TIMEOUT;
                break;
            }

            HandleRecvInterrupt (PollBuf);
        }

        // fail if client de-registered while receiving
        if (!(pClient->State & KITL_CLIENT_REGISTERED)) {
            dwErr = ERROR_BROKEN_PIPE;
            break;
        }

        EnterCriticalSection (&pClient->ClientCS);   
    }

    if (dwErr) {
        KITL_DEBUGMSG (ZONE_RECV,("-KITLRecv(%u): Failed, dwErr = 0x%\n", Id, dwErr));
        NKSetLastError (dwErr);
        
    } else {
        // Copy received data into buffer
        KITL_DEBUGMSG (ZONE_RECV,("-KITLRecv(%u): Read %u bytes from slot %u, copying %u\n",
                pClient->ServiceId, pClient->RxFrameLen[pClient->NextRxIndex], pClient->NextRxIndex, 
                min(pClient->RxFrameLen[pClient->NextRxIndex], *pdwLen)));
        ConsumeKITLPacket (pClient, pRecvBuf, pdwLen);
        LeaveCriticalSection (&pClient->ClientCS);
    }
    
    return !dwErr;
}

static BOOL
DoKITLRecvInSysCall (
    UCHAR           Id,
    UCHAR           *pRecvBuf, // @parm [OUT]- Buffer to receive data
    DWORD           *pdwLen)   // @parm [IN] - Buffer size. [OUT] - Bytes of data received.
{
    KITL_CLIENT *pClient    = KITLClients[Id];
    
    // only kernel debugger should ever do this.
    DEBUGCHK (KITL_SVC_KDBG == pClient->ServiceId);
    DEBUGCHK (pClient);
    DEBUGCHK (KITLGlobalState & KITL_ST_DESKTOP_CONNECTED);
    DEBUGCHK (!KITLTransportCS.OwnerThread);    // transport should be guarded by spinlock when debugger present

    AcquireKitlSpinLock ();

    while (!pClient->RxFrameLen[pClient->NextRxIndex]) {
        HandleRecvInterrupt (PollRecvBuf);
    }

    // Copy received data into buffer
    ConsumeKITLPacket (pClient, pRecvBuf, pdwLen);
    
    ReleaseKitlSpinLock ();

    return TRUE;
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
 *        Added code to set the last error which callers can use to determine the
 *        failure condition.
 *        ERROR_TIMEOUT: timeout failure
 *        ERROR_INVALID_PARAMETER: invalid id or client
 *        ERROR_BROKEN_PIPE: stream is disconnected while in recv call
 *        Any other failure
 *  @xref  <f KITLRegisterClient> <f KITLSend> 
 */ 
BOOL
KITLRecv(
    UCHAR Id,        // @parm [IN] - KITL client id (returned from <f KITLRegisterClient>)
    UCHAR *pRecvBuf, // @parm [OUT]- Buffer to receive data
    DWORD *pdwLen,   // @parm [IN] - Buffer size. [OUT] - Bytes of data received.
    DWORD Timeout)   // @parm [IN] - Timeout value (0 == don't block, INFINITE == wait forever)
{
    BOOL fRet = KITLValidateArgAndAcquireClient(Id, 0);
    if (fRet) {
        if (KITL_SVC_KDBG == Id) {
            // KD - always do it with spinlock
            fRet = DoKITLRecvInSysCall (Id, pRecvBuf, pdwLen);
        } else if(!InSysCall()) {
            fRet = DoKITLRecv (Id, pRecvBuf, pdwLen, Timeout);
        } else {
            KITL_DEBUGMSG (ZONE_ERROR, ("!ERROR: Calling KITLRecv while in syscall\r\n"));
            fRet = FALSE;
        }
        KITLReleaseClient(KITLClients[Id]);
        
        HandleResetOrDebugBreak ();
    } else {
        NKSetLastError(ERROR_INVALID_INDEX);
    }
    return fRet;
}

