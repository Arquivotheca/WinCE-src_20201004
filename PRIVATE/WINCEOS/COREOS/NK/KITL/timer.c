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

// We borrow kernel fixed size memory allocation routine
ERRFALSE(sizeof(TIMERENT) <= sizeof(THRDDBG));

static TIMERENT *TimerList = NULL;
extern CRITICAL_SECTION TimerListCS;

static void TimerListInsert(TIMERENT *pTimerEnt, BOOL fUseSysCalls);

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

/* TimerThread
 *
 *   Thread that handles retransmissions.  Periodically check the list
 *   to see if any timers have expired.  If so, retransmit the associated
 *   frame, and re-insert back in the list. 
 */
DWORD
TimerThread(LPVOID pUnused)
{
    BOOL fResched;

    KITL_DEBUGMSG(ZONE_INIT,("KITL Timer thread started, (hTh: 0x%X, pTh: 0x%X)\n",
                             hCurThread,pCurThread));

    pCurThread->bDbgCnt = 1;   // no entry messages
    while (1) {
        DWORD CurTime = GetTickCount();

        if (entbuf.pfnCB) {
            TimerStart (entbuf.pClient, entbuf.Index, entbuf.dwExpireInt, TRUE);
            entbuf.pfnCB = NULL;
        }
        // Check to see if any timers expired
        EnterCriticalSection(&TimerListCS);        
        while (TimerList && ((int) (TimerList->dwExpireTime - CurTime) <= 0)) {
            TIMERENT *pTimerEnt = TimerList;
            
            // Expired timer, remove from timer list and retransmit frame (first release 
            // CS to avoid potential deadlock, since RetransmitFrame gets client CS).
            TimerList = pTimerEnt->pNext;
            LeaveCriticalSection(&TimerListCS);
            if (!(pTimerEnt->dwExpireInt & 0x80000000))
                fResched = RetransmitFrame(pTimerEnt->pClient, (UCHAR) pTimerEnt->Index, TRUE);
            else {
                fResched = FALSE;
                pTimerEnt->pfnCB ((LPVOID) pTimerEnt->Index);
            }
            EnterCriticalSection(&TimerListCS);
            if (fResched) {
                // Put back in list, increase expiry time so that we get
                // timeouts at 1s, 2s, 4s, and 8s (max)
                if (pTimerEnt->dwExpireInt < KITL_RETRANSMIT_INTERVAL_MAX)
                    pTimerEnt->dwExpireInt <<= 1;
                TimerListInsert(pTimerEnt,TRUE);
            }
            else
                FreeMem(pTimerEnt,HEAP_THREADDBG);
        }
        LeaveCriticalSection(&TimerListCS);
        SC_Sleep(RETRANSMIT_CHECK_INTERVAL_MS);
    }
    return 0;
}

static void
TimerListInsert(TIMERENT *pTimerEnt, BOOL fUseSysCalls)
{
    TIMERENT *pTmp;
    if (!(KITLGlobalState & KITL_ST_TIMER_INIT))
        return;
    if (fUseSysCalls)
        EnterCriticalSection(&TimerListCS);
    else if (TimerListCS.OwnerThread) {
        KITL_DEBUGMSG(ZONE_WARNING,("!TimerListInsert: No syscalls and CS owned\n"));
        FreeMem(pTimerEnt,HEAP_THREADDBG);
        return;
    }
    pTimerEnt->dwExpireTime = GetTickCount() + (pTimerEnt->dwExpireInt & 0x7fffffff);
        
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
    if (KITLDebugZone & ZONE_TIMER)
        DumpTimerList();
    if (fUseSysCalls)
        LeaveCriticalSection(&TimerListCS);
}

BOOL
TimerStart(KITL_CLIENT *pClient, DWORD Index, DWORD dwMSecs, BOOL fUseSysCalls)
{
    TIMERENT *pTimerEnt;
    if (!(KITLGlobalState & KITL_ST_TIMER_INIT))
        return FALSE;
    
    // See note below about not calling AllocMem in !fUseSysCalls state.  Potentially,
    // I could deal with this better - possibly pre allocate some buffers. But for
    // now, there isn't any reason we need to start timers when in this state (since
    // we poll for acks when in no syscall state).
    if (!fUseSysCalls)
        return FALSE;

    pTimerEnt = (TIMERENT *) AllocMem(HEAP_THREADDBG);
    if (!pTimerEnt) {
        KITLOutputDebugString("KITL Error allocating timer memory\n");
        return FALSE;
    }
    pTimerEnt->pClient      = pClient;
    pTimerEnt->Index        = Index;
    pTimerEnt->dwExpireInt  = dwMSecs;
    
    TimerListInsert(pTimerEnt,fUseSysCalls);
    return TRUE;
}

// If we are in the no syscall state, we can't call AllocMem or FreeMem, 
// because they do KCalls, which will reenable interrupts, causing scheduling
// to resume if we are called in through the debugger.  So, just chain up blocks
// of memory to free and free the memory when we get called again in a normal state.
static TIMERENT *pTimerEntsToFree;

BOOL
TimerStop(KITL_CLIENT *pClient, DWORD Index, BOOL fUseSysCalls)
{
    TIMERENT *pTmp,**ppTmp;

    if (!(KITLGlobalState & KITL_ST_TIMER_INIT))
        return FALSE;

    // If we can't use sys calls, and someone owns the CS, just bail. Since RetransmitFrame
    // checks the sequence number, we'll dequeue the timer the next time it expires anyway.
    if (fUseSysCalls)
        EnterCriticalSection(&TimerListCS);
    else if (TimerListCS.OwnerThread)
        return FALSE;
    
    // Remove timer from list
    for (ppTmp = &TimerList;
         *ppTmp && !(((*ppTmp)->pClient == pClient) && ((*ppTmp)->Index == Index));
         ppTmp = &(*ppTmp)->pNext)
        ;
    if (*ppTmp) {
        pTmp = *ppTmp;
        KITL_DEBUGMSG(ZONE_TIMER,("Timer: Removing entry, slot: %u, expire: %u\n",pTmp->Index,pTmp->dwExpireTime));
        *ppTmp = pTmp->pNext;
        // Put timer entry on list to be freed the next time we are called with
        // syscalls enabled.
        pTmp->pNext = pTimerEntsToFree;
        pTimerEntsToFree = pTmp;
    }
    else
        KITL_DEBUGMSG(ZONE_TIMER,("!TimerStop: Timer not started for slot %u\n",Index));
    if (KITLDebugZone & ZONE_TIMER)
        DumpTimerList();
    if (fUseSysCalls) {
        while (pTimerEntsToFree)
        {
            pTmp = pTimerEntsToFree;
            pTimerEntsToFree = pTmp->pNext;
            FreeMem(pTmp, HEAP_THREADDBG);
        }
        LeaveCriticalSection(&TimerListCS);
    }
    return TRUE;
}

/*
 * Stop all timers for a given client.
 */
void
CancelTimersForClient(KITL_CLIENT *pClient)
{
    UCHAR i;

    KITL_DEBUGMSG(ZONE_TIMER,("+CancelTimersForClient(%s)\n",pClient->ServiceName));    
    for (i = 0; i < pClient->WindowSize; i ++)
        TimerStop(pClient,i,(pClient->State & KITL_USE_SYSCALLS) != 0);
}

BOOL KitlSetTimerCallback (int nSecs, PFN_KITLTIMERCB pfnCB, LPVOID lpParam)
{
    if (!(KITLGlobalState & KITL_ST_TIMER_INIT) || InSysCall ()) {
        // can't insert into timer when we're in system call. Just save the info
        // and Timer thread will pick it up once it starts running
        if (!entbuf.pfnCB) {
            entbuf.pfnCB = pfnCB;
            entbuf.dwExpireInt = (nSecs * 1000) | 0x80000000;   // set MSB to indicate callback
            entbuf.Index = (DWORD) lpParam;
            return TRUE;
        }
    }

    return TimerStart ((KITL_CLIENT *) pfnCB, (DWORD) lpParam, (DWORD) (nSecs * 1000) | 0x80000000, TRUE);
}

BOOL KitlStopTimerCallback (PFN_KITLTIMERCB pfnCB, LPVOID lpParam)
{
    if (pfnCB == entbuf.pfnCB) {
        entbuf.pfnCB = NULL;
        return TRUE;
    }
    return TimerStop ((KITL_CLIENT *) pfnCB, (DWORD) lpParam, !InSysCall ());
}
