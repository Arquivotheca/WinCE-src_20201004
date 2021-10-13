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

static void PollAck (KITL_CLIENT *pClient, LPBYTE pPollBuf)
{
    // poll ACK
    UCHAR Seq;
    do {
        if (HandleRecvInterrupt (pPollBuf)) {
            // retransmit
            for (Seq = pClient->AckExpected; Seq != pClient->TxSeqNum; SEQ_INC (Seq))
                RetransmitFrame (pClient, (UCHAR) (Seq % pClient->WindowSize));
        }
    } while ((char) (pClient->TxSeqNum - pClient->AckExpected) > 0);
}

static LONG g_kitlSeqNum;

static void ProduceKITLPacket (KITL_CLIENT *pClient, LPCVOID pUserData, DWORD dwUserDataLen)
{
    UCHAR *pFrame, *pData;
    DWORD dwPacketIndex;
    PKITL_HDR pHdr;

    DEBUGCHK (InSysCall () || OwnCS (&pClient->ClientCS));

    // Format frame into Tx buffer (leave space for transport headers)
    dwPacketIndex   = pClient->TxSeqNum % pClient->WindowSize;
    pFrame          = pClient->pTxBufferPool + (dwPacketIndex * KITL_MTU);
    pHdr            = (PKITL_HDR) (pFrame + Kitl.FrmHdrSize);
    pHdr->Id        = KITL_ID;
    pHdr->Service   = pClient->ServiceId;
    pHdr->Flags     = KITL_FL_FROM_DEV;
    pHdr->Cmd       = KITL_CMD_SVC_DATA;
    pHdr->SeqNum    = pClient->TxSeqNum;
    SEQ_INC(pClient->TxSeqNum);

    pData = (UCHAR *)KITLDATA(pHdr);

    if (KITL_SVC_DBGMSG == pClient->ServiceId) {
        // Copy user data into buffer
        KITL_DBGMSG_INFO *pDbgInfo = (KITL_DBGMSG_INFO *)pData;
        // Prepend a header so that application on the other side can get timing
        // and thread info.
        pDbgInfo->dwLen      = sizeof(KITL_DBGMSG_INFO);
        pDbgInfo->dwThreadId = dwCurThId;
        pDbgInfo->dwProcId   = dwActvProcId+PcbGetCurCpu()-1;
        pDbgInfo->dwTimeStamp = CurMSec; // InterlockedIncrement (&g_kitlSeqNum);
        NKUnicodeToAscii ((PCHAR) (pDbgInfo+1), (LPCWSTR) pUserData, KITL_MAX_DATA_SIZE-sizeof(KITL_DBGMSG_INFO));
    } else {
        // Copy user data into buffer
        memcpy (pData, pUserData, dwUserDataLen);
    }
    pClient->TxFrameLen[dwPacketIndex] = (USHORT)(dwUserDataLen + sizeof(KITL_HDR));

    if (KITLDebugZone & ZONE_FRAMEDUMP) {
        KITLFrameDump(">>KITLSend", pHdr, pClient->TxFrameLen[dwPacketIndex]);
    }

    
}

static void SendStringToSerial (LPWSTR pszStr)
{
    // debug message in syscall, send to serial if available
    if (!(KITLDebugZone & KITL_ZONE_NOSERIAL)) {
        AcquireKitlSpinLock ();        
        KITLOutputDebugString ("%d PID:%x TID:%x ",
            InterlockedIncrement (&g_kitlSeqNum), dwActvProcId+PcbGetCurCpu()-1, dwCurThId);
        OEMWriteDebugString (pszStr);
        ReleaseKitlSpinLock ();        
    }
}

BOOL
DoKITLSend(
    UCHAR Id,               // @parm [IN] - KITL client id (returned from <f KITLRegisterClient>) and type of data
    UCHAR *pUserData,       // @parm [IN] - Data to send.
    DWORD dwUserDataLen )   // @parm [IN] - Data length (up to KITL_MAX_DATA_SIZE).
{
    KITL_CLIENT *pClient    = KITLClients[Id];
    BOOL        fRet        = TRUE;
    PTHREAD     pCurTh      = pCurThread;
    UCHAR       *pFrame;
    DWORD       dwPacketIndex;
    BYTE        PollBuf[KITL_MTU];

    // Check for re-entrancy -- e.g. if kernel debug zones are turned on
    if ((KITL_SVC_DBGMSG == Id) && pCurTh && (pCurTh->bDbgCnt > 1)) {
        // debug message re-entrant
        SendStringToSerial ((LPWSTR) pUserData);

    } else {
        EnterCriticalSection (&pClient->ClientCS);

        KITL_DEBUGMSG(ZONE_SEND,("+KITLSend(%u), Len: %u, Seq: %u, AckExpected: %u\n",
                             Id, dwUserDataLen,pClient->TxSeqNum, pClient->AckExpected));
        // If Tx window full, block
#ifdef DEBUG
        if (SEQ_DELTA(pClient->AckExpected, pClient->TxSeqNum) >= pClient->WindowSize) {
            KITL_DEBUGMSG(ZONE_SEND,("KITLSend(%u): Tx window full (AckExpected: %u, Seq: %u)\n",
                                        Id,pClient->AckExpected,pClient->TxSeqNum));
        }
#endif
        while (fRet && SEQ_DELTA(pClient->AckExpected, pClient->TxSeqNum) >= pClient->WindowSize) {

            // reset queue full event
            KITLResetEvent (pClient->phdTxFullEvt);

            LeaveCriticalSection (&pClient->ClientCS);

            if (KITLGlobalState & KITL_ST_INT_ENABLED) {
                // interrupt enabled
                VERIFY (DoWaitForObjects (1, &pClient->phdTxFullEvt, INFINITE) == WAIT_OBJECT_0);

                fRet = ((pClient->State & KITL_CLIENT_REGISTERED) != 0);

            } else {
                // pooling mode, there are at least 16 threads in the middle of sending. Give them 
                // chances to run.
                NKSleep (1);   
            }
            EnterCriticalSection (&pClient->ClientCS);
        }

        if (fRet) {
            dwPacketIndex   = pClient->TxSeqNum % pClient->WindowSize;
            pFrame          = pClient->pTxBufferPool + (dwPacketIndex * KITL_MTU);

            // Prepare KITL Packet
            ProduceKITLPacket (pClient, pUserData, dwUserDataLen);

            fRet = KitlSendFrame(pFrame, pClient->TxFrameLen[dwPacketIndex]);
            if (!fRet) {
                KITLOutputDebugString("!KITLSend: Error in KitlSendFrame\n");
                // Restore state
                pClient->TxSeqNum = SEQ_SUB1(pClient->TxSeqNum);
                pClient->TxFrameLen[dwPacketIndex] = 0;
            } else if (KITLGlobalState & KITL_ST_INT_ENABLED) {
                // interrupt mode, Start retransmit timer
                
                // TODO: Make sure the timer system will hold a
                // reference count to the client.
                TimerStart (pClient, dwPacketIndex, KITL_RETRANSMIT_INTERVAL_MS);

            } else {
                LeaveCriticalSection (&pClient->ClientCS);

                // polling mode, poll ACK
                PollAck (pClient, PollBuf);

                EnterCriticalSection (&pClient->ClientCS);
            }
        }
        LeaveCriticalSection (&pClient->ClientCS);

        KITL_DEBUGMSG(ZONE_SEND,("-Send (%u): Ret %u\n", Id, fRet));
    }

    return fRet;
}

BOOL DoKITLSendInSysCall (UCHAR Id, UCHAR *pUserData, DWORD dwUserDataLen)
{
    KITL_CLIENT *pClient = KITLClients[Id];
    DWORD       dwErr    = 0;
    PUCHAR      pFrame;
    DWORD       dwPacketIndex;

    DEBUGCHK ((KITL_SVC_KDBG == Id) || !g_pprcNK->csVM.hCrit);
    DEBUGCHK (pClient);
    DEBUGCHK (KITLGlobalState & KITL_ST_DESKTOP_CONNECTED);
    DEBUGCHK (!KITLTransportCS.OwnerThread && !pClient->ClientCS.OwnerThread);
    DEBUGCHK (SEQ_DELTA(pClient->AckExpected, pClient->TxSeqNum) < pClient->WindowSize);

    AcquireKitlSpinLock ();        

    // Format frame into Tx buffer (leave space for transport headers)
    dwPacketIndex   = pClient->TxSeqNum % pClient->WindowSize;
    pFrame          = pClient->pTxBufferPool + (dwPacketIndex * KITL_MTU);
    ProduceKITLPacket (pClient, pUserData, dwUserDataLen);
    
    if (KitlSendFrame (pFrame, pClient->TxFrameLen[dwPacketIndex])) {
        // poll ACK
        PollAck (pClient, PollRecvBuf);

    } else {
        // Restore state
        dwErr = KITLERR_TRANSPORT_FAILED;
        pClient->TxSeqNum = SEQ_SUB1(pClient->TxSeqNum);
        pClient->TxFrameLen[dwPacketIndex] = 0;
    }

    ReleaseKitlSpinLock ();

    if (dwErr) {
        KITL_DEBUGMSG (ZONE_ERROR, ("!DoKITLSendInSysCall - KITL Transport failed.\n"));
    }

    return !dwErr;
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
BOOL
KITLSend(
    UCHAR Id,               // @parm [IN] - KITL client id (returned from <f KITLRegisterClient>)
    UCHAR *pUserData,       // @parm [IN] - Data to send.
    DWORD dwUserDataLen )   // @parm [IN] - Data length (up to KITL_MAX_DATA_SIZE).
{
    BOOL fRet;

    if (KITL_SVC_DBGMSG == Id) {
        // adjust data length
        dwUserDataLen = NKwcslen ((LPCWSTR)pUserData) + 1 + sizeof (KITL_DBGMSG_INFO);
    }

    fRet = KITLValidateArgAndAcquireClient(Id, dwUserDataLen);
    if (fRet) {
        if ((KITL_SVC_KDBG == Id) || !g_pprcNK->csVM.hCrit) {
            // system intialization or KD - use spinlock
            fRet = DoKITLSendInSysCall (Id, pUserData, dwUserDataLen);
        } else if (!InSysCall ()) {
            fRet = DoKITLSend (Id, pUserData, dwUserDataLen);
        } else if (KITL_SVC_DBGMSG == Id) {
            // debug message in syscall, send to serial if available
            SendStringToSerial ((LPWSTR) pUserData);
            fRet = TRUE;
        } else {
            KITL_DEBUGMSG (ZONE_ERROR, ("!ERROR: Calling KITLSend while in syscall\r\n"));
            fRet = FALSE;
        }
        KITLReleaseClient(KITLClients[Id]);
        
        HandleResetOrDebugBreak ();
    } else {
        NKSetLastError(ERROR_INVALID_INDEX);
    }
    return fRet;
}

void KITLWriteDebugString (LPCWSTR str)
{
    KITLSend (KITL_SVC_DBGMSG, (LPBYTE) str, 0);
}
