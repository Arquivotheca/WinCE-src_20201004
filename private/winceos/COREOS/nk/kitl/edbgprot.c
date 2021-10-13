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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    edbgprot.c
    
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

extern CRITICAL_SECTION KITLODScs;

SPINLOCK g_kitlLock;

BOOL IsDesktopDbgrExist (void);

#include "..\..\..\..\..\public\common\oak\dbgpub\dbgbrk.c" // For DoDebugBreak()

DWORD g_KitlInterval; // KITL time based on desktop re-transmit packet
extern DWORD TimerCallback(DWORD*);

/* ResetClientState
 *
 *   Reset all state associated with client struct.
 *   Must hold client CS while calling this function.
 */
void
ResetClientState(KITL_CLIENT *pClient)
{
    BOOL const fInSysCall = InSysCall ();

    if (!fInSysCall) {
        // If not in a syscall, then cancel any timers for this kitl stream.
        CancelTimersForClient(pClient);
    }
    
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
    if (!fInSysCall) {
        KITLSetEvent (pClient->phdTxFullEvt);
    }
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
RetransmitFrame(KITL_CLIENT *pClient, UCHAR Index)
{
    UCHAR *pData = pClient->pTxBufferPool + (Index * KITL_MTU);
    BOOL  fRet = TRUE;
    BOOL  fUseSysCalls = UseSysCall (pClient);
    KITL_HDR *pHdr;

    pHdr = (KITL_HDR *)(pData + Kitl.FrmHdrSize);

    if (KITLTryAcquireClient (pClient)) {
        //KITLOutputDebugString ("RetransmitFrame: %x %x %x\r\n", fUseSysCalls, pClient->ClientCS.OwnerThread, dwCurThId);
        if (fUseSysCalls)
            EnterCriticalSection (&pClient->ClientCS);

        if (!SEQ_BETWEEN(pClient->AckExpected, pHdr->SeqNum, pClient->TxSeqNum)) {
            KITL_DEBUGMSG(ZONE_WARNING,("!RetransmitFrame: Frame seq %u not in Tx window (%u,%u)\n",
                                        pHdr->SeqNum, pClient->AckExpected, pClient->TxSeqNum));
            fRet = FALSE; // Don't reschedule timer
        } else {
            if (KITLDebugZone & ZONE_FRAMEDUMP)
                KITLFrameDump(">>Retransmit", pHdr, pClient->TxFrameLen[Index]);

            KITL_DEBUGMSG(ZONE_RETRANSMIT,("RetransmitFrame(Id:%u), Seq: %u, Len: %u, Window (%u,%u)\n",pClient->ServiceId,
                                           pHdr->SeqNum, pClient->TxFrameLen[Index], pClient->AckExpected, pClient->TxSeqNum));
            if (!KitlSendRawData (pData, (USHORT) (pClient->TxFrameLen[Index] + Kitl.FrmHdrSize + Kitl.FrmTlrSize))) {
                KITLOutputDebugString("!RetransmitFrame: Error in KitlSendRawData\n");
                // Go ahead and return TRUE so frame will be tried again.
            }
        }

        if (fUseSysCalls)
            LeaveCriticalSection (&pClient->ClientCS);

        KITLReleaseClient(pClient);
    } else {
        // pClient is no longer valid (shutting down).  Make sure this
        // frame never gets rescheduled for a retransmit.
        fRet = FALSE;
    }
    return fRet;
}


BOOL KITLValidateArgAndAcquireClient (UCHAR Id, DWORD dwUserDataLen)
{
    KITL_CLIENT *pClient = KITLClients[Id];
    BOOL fRegistered;
    if (!IS_VALID_ID(Id) ||
          pClient == NULL ||
          Id != pClient->ServiceId ||
          dwUserDataLen > KITL_MAX_DATA_SIZE ||
          !KITLTryAcquireClient(pClient)) {
        return FALSE;
    }

    fRegistered = (pClient->State & KITL_CLIENT_REGISTERED) || ExchangeConfig(pClient);
    if (!fRegistered) {
        KITLReleaseClient(pClient);
    }
    return fRegistered;
}
   

void KITLFrameDump(char *LeadingText, KITL_HDR *pHdr, WORD wLen)
{
    // Watch for reentrancy - we may be calling in to do a debug message from within
    // EnterCriticalSection() for example.
    BOOL fUseSysCalls = (KITLGlobalState & KITL_ST_MULTITHREADED) && !InSysCall();

    if (fUseSysCalls) {
//        LARGE_INTEGER liPerfCnt;
//        DWORD ms;
        EnterCriticalSection (&KITLODScs);
//        NKQueryPerformanceCounter(&liPerfCnt);
//        ms = liPerfCnt.LowPart/1000;
        // Print out ms count
//        KITLOutputDebugString("(%u.%u) ",ms/1000,ms%1000);
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
                    {
                        KITL_SVC_CONFIG_DATA *pCfg = (KITL_SVC_CONFIG_DATA *)KITLDATA(pHdr);

                        const SVCINSTID siiInstanceId = IS_SVC_MULTIINST (pCfg->Flags)
                            ? pCfg->Multi.ServiceInstanceId
                            : SVC_INST_ID_INVALID;

                        const LPCSTR szServiceName = IS_SVC_MULTIINST (pCfg->Flags)
                            ? pCfg->Multi.ServiceName
                            : pCfg->Single.ServiceName;

                        if (pHdr->Flags & KITL_FL_FROM_DEV) {
                            KITLOutputDebugString("DEV %s, ProtoVer:%u, Service: ('%s',%u), Id: %u, Window: %u, Fl:0x%X",
                                                  (pHdr->Flags & KITL_FL_ADMIN_RESP)? "RESP":"CMD",
                                                  pCfg->ProtocolVersion,szServiceName,siiInstanceId,pCfg->ServiceId,
                                                  pCfg->WindowSize,pCfg->Flags);
                        }
                        else {
                            KITLOutputDebugString("DESK %s, ProtoVer:%u, Service: ('%s',%u), Id: %u",
                                                  (pHdr->Flags & KITL_FL_ADMIN_RESP)? "RESP":"CMD",
                                                  pCfg->ProtocolVersion,szServiceName,siiInstanceId, pCfg->ServiceId);
                        }
                    }
                    break;
                case KITL_CMD_RETRASMIT:
                    KITLOutputDebugString("RETRANSMIT ");
                    break;
                case KITL_CMD_TRAN_CONFIG:
                {
                    PKITL_HOST_TRANSCFG pCfg = (PKITL_HOST_TRANSCFG) KITLDATA(pHdr);
                    KITLOutputDebugString ("TRANSPORT CONFIG, flag = 0x%x", pCfg->dwFlags);
                    break;
                }
                case KITL_CMD_GET_CAPABILITIES:
                    KITLOutputDebugString ("KITL_CMD_GET_CAPABILITIES");
                    if (pHdr->Flags & KITL_FL_FROM_DEV)
                    {
                        const PKITL_CAPABILITIES_DATA pCapabilitiesData = (PKITL_CAPABILITIES_DATA) KITLDATA(pHdr);
                        KITLOutputDebugString (", Capabilities: 0x%x", pCapabilitiesData->kcSupported);
                    }
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
    KITLOutputDebugString(", DataLen: %u\n",wLen - sizeof(KITL_HDR));
    if (fUseSysCalls)
        LeaveCriticalSection (&KITLODScs);    
}

