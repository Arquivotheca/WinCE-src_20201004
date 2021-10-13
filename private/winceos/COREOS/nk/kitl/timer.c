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
    timer.c
    
Abstract:  
    Routines for retransmission timer thread used in Ethernet debug
    protocol.  When a frame is sent, a timer is inserted on the list of
    active timers.  This list is checked periodically, and when timers
    expire, the corresponding frame is retransmitted.

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

// How often the retransmit thread will wake up to check for retransmissions
#define RETRANSMIT_CHECK_INTERVAL_MS  500
#define DHCP_RENEW_SCALE    500 // 1000 ms/sec and renew at 1/2 of the lease time
#define DHCP_RETRY_TIMEOUT      10000
#define NUM_DHCP_RETRIES        4

typedef struct _timerent
{
    struct _timerent *pNext;
    union {
        KITL_CLIENT *pClient;
        PFN_KITLTIMERCB pfnCB;
    };
    DWORD  Index;
    DWORD  dwExpireTime;
    DWORD  dwExpireInt;
    
} TIMERENT;

// for DEBUGMSG and CESH, we don't want to call AllocMem to allocate timer
// entry, or they will stop working in low memory condition.
// KDBG always poll, so we don't need to pre-allocate for it.
 // Added one additional node to account for possible "transition" nodes where 
 // in timerthread a node is not in the free list or timerlist but is in transition for
 // a brief duration when a call could come into TimerStart leading to no free
 // slots avaialble.
TIMERENT preallocent[KITL_MAX_WINDOW_SIZE*2 + 1];
#define preallocentLen (sizeof(preallocent) / sizeof(preallocent[0]))

// We borrow kernel fixed size memory allocation routine
ERRFALSE(sizeof(TIMERENT) <= sizeof(PROXY));


static TIMERENT *TimerList;
static TIMERENT *pFreeList;
extern CRITICAL_SECTION TimerListCS;
extern KITLTRANSPORT Kitl;
static void TimerListInsert(TIMERENT *pTimerEnt);

//
// we'll setup one buffer slot for timer callback since the OEM might want to set timer 
// callback before timer thread is started
static TIMERENT entbuf;

void
DumpTimerList()
{
    TIMERENT *pTimerEnt;
    for (pTimerEnt=TimerList; pTimerEnt; pTimerEnt=pTimerEnt->pNext) {
        KITLOutputDebugString("TimerEnt: slot %u, expire: %u\n",pTimerEnt->Index,pTimerEnt->dwExpireTime);
    }
}

void InitTimerList (void)
{
    int i;
    pFreeList = preallocent;

    for (i = 0; i < preallocentLen - 1; i ++) {
        preallocent[i].pNext = &preallocent[i+1];
    }
    preallocent[i].pNext = NULL;
}

void FreeTimerEntry (TIMERENT *pTimerEnt)
{
    DEBUGCHK (OwnCS (&TimerListCS));

    if (!(pTimerEnt->dwExpireInt & 0x80000000)) {
        DEBUGCHK (pTimerEnt->pClient);
        switch (pTimerEnt->pClient->ServiceId) {
        case KITL_SVC_DBGMSG:
        case KITL_SVC_PPSH:
            pTimerEnt->pNext = pFreeList;
            pFreeList = pTimerEnt;
            return;
        default:
            break;
        }
    }
    FreeMem (pTimerEnt,HEAP_TIMERENTRY);
}

/* TimerCallback
 *
 *   Function to send any timer notifications based on desktop ack packet. 
 */
DWORD
TimerCallback(DWORD* pdwCurInterval)
{
    // Check to see if any timers expired
    DWORD dwTimerInt = 0;
    PFN_KITLTIMERCB pfn = entbuf.pfnCB;
    TIMERENT *pTimerEnt = TimerList;
    TIMERENT* pTimerPrev = NULL;
    
    if (NULL != (HANDLE)TimerListCS.OwnerThread || !g_pKData->dwInDebugger) {
        // timerlist is locked or we are not in a debug break state; ignore the request
        return 0;
    }

    while (pTimerEnt) {

        if (pTimerEnt->dwExpireInt & 0x80000000) { // timer callback
            dwTimerInt = (pTimerEnt->dwExpireInt & 0x7fffffff) / 1000; // expire time in seconds         

            if (dwTimerInt <= *pdwCurInterval) { // timer expired
                if (!pTimerPrev) {
                    TimerList = pTimerEnt->pNext;
                } else {
                    pTimerPrev->pNext = pTimerEnt->pNext;
                }
                
                // call the callback function
                pTimerEnt->pfnCB ((LPVOID) pTimerEnt->Index);
                FreeMem (pTimerEnt,HEAP_TIMERENTRY);

                *pdwCurInterval = 0; // reset the desktop timer count
                return 1;
            }
        }
        pTimerPrev = pTimerEnt;
        pTimerEnt = pTimerEnt->pNext;
    }

    // if we didn't find any timer notification expirations, see if there are any pending
    // timer callback notifications
    if (pfn && entbuf.dwExpireInt & 0x80000000) {
        dwTimerInt = (entbuf.dwExpireInt & 0x7fffffff) / 1000; // expire time in seconds         
        if (dwTimerInt <= *pdwCurInterval) { // timer expired
            // call the callback function
            entbuf.pfnCB = NULL;
            (*pfn) ((LPVOID) entbuf.Index);
            *pdwCurInterval = 0; // reset the desktop timer count            
            return 1;
        }
    }

    return 0;
}

/* TimerThread
 *
 *   Thread that handles retransmissions.  Periodically check the list
 *   to see if any timers have expired.  If so, retransmit the associated
 *   frame, and re-insert back in the list. 
 */
void
TimerThread(LPVOID pUnused)
{
    BOOL fResched;
    UCHAR *pData = NULL;
    KITL_HDR *pHdr = NULL;
    
    pCurThread->bDbgCnt = 1;   // no entry messages - must be set before any debug message

    KITL_DEBUGMSG(ZONE_INIT,("KITL Timer thread started, (hTh: 0x%X, pTh: 0x%X)\n",
                             dwCurThId, pCurThread));

    dwTimerThId = dwCurThId;
    KITLGlobalState |= KITL_ST_TIMER_INIT;
    for(;;) {
        DWORD CurTime = OEMGetTickCount();

        if (entbuf.pfnCB) {
            TimerStart (entbuf.pClient, entbuf.Index, entbuf.dwExpireInt);
            entbuf.pfnCB = NULL;
        }
        // Check to see if any timers expired
        //KITLOutputDebugString ("Timer Thread checking, TimerListCS Owner = %x\r\n", TimerListCS.OwnerThread);
        EnterCriticalSection(&TimerListCS);        
        PREFAST_SUPPRESS (22019, "integer underflow below is by design. Negative distance mean timer expired");
        while (TimerList && ((int) (TimerList->dwExpireTime - CurTime) <= 0)) {
            TIMERENT *pTimerEnt = TimerList;
            
            // Expired timer, remove from timer list and retransmit frame (first release 
            // CS to avoid potential deadlock, since RetransmitFrame gets client CS).
            TimerList = pTimerEnt->pNext;
            LeaveCriticalSection(&TimerListCS);
            if (!(pTimerEnt->dwExpireInt & 0x80000000)) {
                //KITLOutputDebugString ("Timer Thread retransmitting, %x\r\n", pTimerEnt->Index);
                fResched = RetransmitFrame(pTimerEnt->pClient, (UCHAR) pTimerEnt->Index);
                //KITLOutputDebugString ("Timer Thread Done retransmitting, fResched = %x\r\n", fResched);
            } else {
                fResched = FALSE;
                pTimerEnt->pfnCB ((LPVOID) pTimerEnt->Index);
            }
            EnterCriticalSection(&TimerListCS);
            if (fResched) {
                // find out if this particular kitl packet is already acked
                pData = pTimerEnt->pClient->pTxBufferPool + (pTimerEnt->Index * KITL_MTU);                
                pHdr = (KITL_HDR *)(pData + Kitl.FrmHdrSize);
    
                if (SEQ_BETWEEN(pTimerEnt->pClient->AckExpected, pHdr->SeqNum, pTimerEnt->pClient->TxSeqNum)) {
                    // Put back in list, increase expiry time so that we get
                    // timeouts at 1s, 2s, 4s, and 8s (max)
                    if (pTimerEnt->dwExpireInt < KITL_RETRANSMIT_INTERVAL_MAX)
                        pTimerEnt->dwExpireInt <<= 1;
                    TimerListInsert (pTimerEnt);
                } else {
                    // this particular packet is acked; remove it from the list
                    FreeTimerEntry (pTimerEnt);
                }
            }
            else {
                FreeTimerEntry (pTimerEnt);
            }
        }
        LeaveCriticalSection(&TimerListCS);
        NKSleep (RETRANSMIT_CHECK_INTERVAL_MS);
    }
}

static void TimerListInsert (TIMERENT *pTimerEnt)
{
    TIMERENT *pTmp;

    DEBUGCHK (KITLGlobalState & KITL_ST_TIMER_INIT);

    EnterCriticalSection(&TimerListCS);

    pTimerEnt->dwExpireTime = OEMGetTickCount() + (pTimerEnt->dwExpireInt & 0x7fffffff);
        
    // Insert into timer list, sorted by time remaining (if equal, put at end of equal
    // entries, to keep retransmission sequence the same)
    if (((pTmp = TimerList) == NULL) || ((int) (pTmp->dwExpireTime - pTimerEnt->dwExpireTime) > 0)) {
        pTimerEnt->pNext = TimerList;
        TimerList = pTimerEnt;
    }
    else {
        while (pTmp->pNext && ((int) (pTmp->pNext->dwExpireTime - pTimerEnt->dwExpireTime) <= 0))
            pTmp = pTmp->pNext;
        pTimerEnt->pNext = pTmp->pNext;
        pTmp->pNext = pTimerEnt;
    }
    KITL_DEBUGMSG(ZONE_TIMER,("Timer: Inserted entry 0x%X, slot: %u, expire: %u\n",pTimerEnt,pTimerEnt->Index,pTimerEnt->dwExpireTime));

#ifdef DEBUG    
    if (KITLDebugZone & ZONE_TIMER) {
        DumpTimerList();
    }
#endif

    LeaveCriticalSection(&TimerListCS);
}

// If we are in the no syscall state, we can't call AllocMem or FreeMem, 
// because they do KCalls, which will reenable interrupts, causing scheduling
// to resume if we are called in through the debugger.  So, just chain up blocks
// of memory to free and free the memory when we get called again in a normal state.
static TIMERENT *pTimerEntsToFree;

void
TimerStart(KITL_CLIENT *pClient, DWORD Index, DWORD dwMSecs)
{
    if (KITLGlobalState & KITL_ST_TIMER_INIT) {

        TIMERENT *pTimerEnt = NULL;
        
        DEBUGCHK (!InSysCall ());
        
        // dwMSecs < 0 iff for timer callback
        if (!(dwMSecs & 0x80000000)) {

            switch (pClient->ServiceId) {
            case KITL_SVC_DBGMSG:
            case KITL_SVC_PPSH:
                EnterCriticalSection (&TimerListCS);
                DEBUGCHK (pFreeList);
                pTimerEnt = pFreeList;
                pFreeList = pFreeList->pNext;
                LeaveCriticalSection (&TimerListCS);
                break;
            default:
                break;
            }
        }

        if(pTimerEnt == NULL) {
            pTimerEnt = AllocMem(HEAP_TIMERENTRY);
            if(pTimerEnt == NULL) {
                KITLOutputDebugString("KITL Error allocating timer memory\n");
                return;
            }
        }

        pTimerEnt->pClient      = pClient;
        pTimerEnt->Index        = Index;
        pTimerEnt->dwExpireInt  = dwMSecs;
        
        TimerListInsert (pTimerEnt);
    }
}

void
TimerStop(KITL_CLIENT *pClient, DWORD Index)
{
    if (KITLGlobalState & KITL_ST_TIMER_INIT) {

        TIMERENT *pTmp,**ppTmp;
        DEBUGCHK (!InSysCall ());

        EnterCriticalSection(&TimerListCS);
        
        // Remove timer from list
        for (ppTmp = &TimerList;
             *ppTmp && !(((*ppTmp)->pClient == pClient) && ((*ppTmp)->Index == Index));
             ppTmp = &(*ppTmp)->pNext)
            ;
        if (*ppTmp) {
            pTmp = *ppTmp;
            KITL_DEBUGMSG(ZONE_TIMER,("Timer: Removing entry, slot: %u, expire: %u\n",pTmp->Index,pTmp->dwExpireTime));
            *ppTmp = pTmp->pNext;
            FreeTimerEntry (pTmp);
            
        } else {
            KITL_DEBUGMSG(ZONE_TIMER,("!TimerStop: Timer not started for slot %u\n",Index));
        }

#ifdef DEBUG        
        if (KITLDebugZone & ZONE_TIMER) {
            DumpTimerList();
        }
#endif
        
        LeaveCriticalSection(&TimerListCS);
    }

}

/*
 * Stop all timers for a given client.
 */
void
CancelTimersForClient(KITL_CLIENT *pClient)
{
    UCHAR i;

    DEBUGCHK (!InSysCall ());
    
    KITL_DEBUGMSG(ZONE_TIMER,("+CancelTimersForClient(%s)\n",pClient->ServiceName));    
    for (i = 0; i < pClient->WindowSize; i ++)
        TimerStop(pClient,i);
}

BOOL KitlSetTimerCallback (int nSecs, PFN_KITLTIMERCB pfnCB, LPVOID lpParam)
{
    if (!(KITLGlobalState & KITL_ST_TIMER_INIT) || InSysCall () || g_pKData->dwInDebugger) {
        // can't insert into timer when we're in system call. Just save the info
        // and Timer thread will pick it up once it starts running
        if (!entbuf.pfnCB) {
            entbuf.pfnCB = pfnCB;
            entbuf.dwExpireInt = (nSecs * 1000) | 0x80000000;   // set MSB to indicate callback
            entbuf.Index = (DWORD) lpParam;
        }
    } else {
#pragma warning(suppress:4054)
        TimerStart ((KITL_CLIENT *) pfnCB, (DWORD) lpParam, (DWORD) (nSecs * 1000) | 0x80000000);
    }
    return TRUE;
}

BOOL KitlStopTimerCallback (PFN_KITLTIMERCB pfnCB, LPVOID lpParam)
{
    if (pfnCB == entbuf.pfnCB) {
        entbuf.pfnCB = NULL;
        return TRUE;
    } else if (!InSysCall ()) {
#pragma warning(suppress:4054)
        TimerStop ((KITL_CLIENT *) pfnCB, (DWORD) lpParam);
        return TRUE;
    }
    return FALSE;
}
