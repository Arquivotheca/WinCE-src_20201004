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
/*
 *              NK Kernel scheduler code
 *
 *
 * Module Name:
 *
 *              schedule.c
 *
 * Abstract:
 *
 *              This file implements the scheduler for the NK kernel
 *
 *
 */

/* @doc EXTERNAL KERNEL */
/* @topic Kernel Entrypoints | Kernel Entrypoints */

#include "kernel.h"
#ifdef ARM
#include "nkarm.h"
#endif
#include <psapi.h>
#include "ksysdbg.h"
#include <watchdog.h>

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif


/* General design:
    There are many queues in the system, for all categories of processes.  All
    processes are on exactly one queue, with the exception of the currently
    running process (if any) which is on the NULL queue.  Runnable processes are
    on one of the run queues (there are several, one for each priority).  Examples
    of other queues are for sleeping, blocked on a critical section (one per
    critical section), etc.  Preemption grabs the current thread (if any) and puts
    it at the end of the queue for that thread's priority, and then grabs the
    highest priority thread (which should, by definition, always be at the same
    priority as the preempted thread, else the other thread would have been
    running [in the case of a higher prio], or we would have continued running
    the preempted thread [in the case of a lower prio].  Kernel routines take
    threads off of queues and put them on other queues.  NextThread finds the
    next thread to run, or returns 0 if there are no runnable threads.  We preempt
    with a fixed timeslice (probably 10ms).
*/

CRITICAL_SECTION csDbg, NameCS, CompCS, ModListcs, MapCS, PagerCS;
CRITICAL_SECTION PhysCS, ODScs, RFBcs, rtccs, PageOutCS, WDcs;


//
// Check to see if we own any kernel critical section.
// NOTE: the check here may not be strict enough for we'll not be able to detect
//       cs fast-path. In order to detect CS fast path, we have to enumerate
//       though all the kernel create CS, and check owner thread, which is impractical
//       at.
//
BOOL OwnAnyKernelCs (void)
{
    PPROCESS pprc = pActvProc;
    return OwnCS (&pprc->csHndl)
        || OwnCS (&pprc->csVM)
        || OwnCS (&g_pprcNK->csHndl)
        || OwnCS (&g_pprcNK->csVM)
        || OwnCS (&MapCS)
        || OwnCS (&csDbg)
        || OwnCS (&NameCS)
        || OwnCS (&ODScs)
        || OwnCS (&CompCS)
        || OwnCS (&PhysCS)
        || OwnCS (&RFBcs)
        || OwnCS (&WDcs)
        || OwnCS (&PagerCS)
        || OwnCS (&PageOutCS);
}

HANDLE  hAlarmThreadWakeup;
PTHREAD pCleanupThread;

#define THREAD_PWR_GUARD_PRIORITY       1
HANDLE hEvtPwrHndlr;
HANDLE hEvtDirtyPage;
PEVENT pEvtDirtyPage;

LPVOID pExitThread;
LPVOID pUsrExcpExitThread;
LPVOID pKrnExcpExitThread;
LPVOID pCaptureDumpFileOnDevice;
LPVOID pKCaptureDumpFileOnDevice;


ERRFALSE(offsetof(EVENT, pProxList) == offsetof(THREAD, pProxList));
ERRFALSE(offsetof(EVENT, pProxList) == offsetof(STUBEVENT, pProxList));

ERRFALSE(offsetof(CRIT, pProxList) == offsetof(SEMAPHORE, pProxList));
ERRFALSE(offsetof(CRIT, pProxList) == offsetof(MUTEX, pProxList));
ERRFALSE(offsetof(CRIT, pProxList) == offsetof(EVENT, pProxList));
ERRFALSE(offsetof(CRIT, pProxHash) == offsetof(SEMAPHORE, pProxHash));
ERRFALSE(offsetof(CRIT, pProxHash) == offsetof(MUTEX, pProxHash));
ERRFALSE(offsetof(CRIT, pProxHash) == offsetof(EVENT, pProxHash));

ERRFALSE(offsetof(CRIT, bListed) == offsetof(MUTEX, bListed));
ERRFALSE(offsetof(CRIT, bListedPrio) == offsetof(MUTEX, bListedPrio));
ERRFALSE(offsetof(CRIT, pPrevOwned) == offsetof(MUTEX, pPrevOwned));
ERRFALSE(offsetof(CRIT, pNextOwned) == offsetof(MUTEX, pNextOwned));
ERRFALSE(offsetof(CRIT, pUpOwned) == offsetof(MUTEX, pUpOwned));
ERRFALSE(offsetof(CRIT, pDownOwned) == offsetof(MUTEX, pDownOwned));


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
NKSetLowestScheduledPriority(
    DWORD maxprio
    )
{
    DWORD retval;
    extern DWORD currmaxprio;
    extern PTHREAD pOOMThread;
    TRUSTED_API (L"NKSetLowestScheduledPriority", currmaxprio);
    DEBUGMSG(ZONE_ENTRY, (L"NKSetLowestScheduledPriority entry: %8.8lx\r\n",maxprio));
    if (maxprio > MAX_WIN32_PRIORITY_LEVELS - 1)
        maxprio = MAX_WIN32_PRIORITY_LEVELS - 1;
    retval = currmaxprio;
    if (retval < MAX_CE_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS)
        retval = MAX_CE_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS;
    retval -= (MAX_CE_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS);
    currmaxprio = maxprio + (MAX_CE_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS);
    pOOMThread = pCurThread;

    // Make sure the paging pool trim threads still get to run
    SetPagingPoolPrio (currmaxprio);

    DEBUGMSG(ZONE_ENTRY,(L"NKSetLowestScheduledPriority exit: %8.8lx\r\n",retval));
    return retval;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
GoToUserTime()
{
    DWORD newTime;
    INTERRUPTS_OFF();
    newTime = GETCURRTICK ();
    SET_TIMEUSER(pCurThread);
    pCurThread->dwKTime += newTime - CurrTime;
    CurrTime = newTime;
    INTERRUPTS_ON();
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (newTime & 0x1f);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
GoToKernTime()
{
    DWORD newTime;
    INTERRUPTS_OFF();
    newTime = GETCURRTICK ();
    SET_TIMEKERN(pCurThread);
    pCurThread->dwUTime += newTime - CurrTime;
    CurrTime = newTime;
    INTERRUPTS_ON();
    randdw2 = ((randdw2<<5) | (randdw2>>27)) ^ (newTime & 0x1f);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
UB_ThreadSuspend(
    PTHREAD pth
    )
{
    DWORD retval = 0xFFFFFFFF;
    SET_USERBLOCK(pCurThread);
    THRDSuspend (pth, &retval);
    CLEAR_USERBLOCK(pCurThread);
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
ThreadSleep (
    DWORD time
    )
{
    sleeper_t sleeper;
    DEBUGCHK (!(time & 0x80000000));
    sleeper.pth = 0;
    sleeper.wDirection = 0;
    sleeper.dwWakeupTime = OEMGetTickCount () + time;

    while (KCall ((PKFN)PutThreadToSleep, &sleeper) || GET_NEEDSLEEP (pCurThread))
        ;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
NKSleep(
    DWORD cMilliseconds
    )
{
    DEBUGMSG(ZONE_ENTRY,(L"NKSleep entry: %8.8lx\r\n",cMilliseconds));

    CELOG_Sleep(cMilliseconds);

    if (cMilliseconds == INFINITE)
        KCall((PKFN)ThreadSuspend,pCurThread, TRUE);
    else if (!cMilliseconds)
        KCall((PKFN)ThreadYield);
    else {

        if (cMilliseconds < 0xfffffffe)
            cMilliseconds ++;    // add 1 millisecond

        // handle cases when someone want to sleep *very* long
        while (cMilliseconds > MAX_TIMEOUT) {
            DWORD dwToWakeup = GETCURRTICK() + MAX_TIMEOUT;
            ThreadSleep (MAX_TIMEOUT);
            if ((int) (cMilliseconds -= MAX_TIMEOUT + (GETCURRTICK() - dwToWakeup)) <= 0) {
                DEBUGMSG(ZONE_ENTRY,(L"SC_Sleep exit 1\r\n"));
                return;
            }
        }
        ThreadSleep (cMilliseconds);
    }
    DEBUGMSG(ZONE_ENTRY,(L"NKSleep exit\r\n"));
}

void NKSleepTillTick (void)
{
    DEBUGMSG (ZONE_ENTRY, (L"NKSleepTillTick entry\r\n"));
    SET_USERBLOCK (pCurThread);
    ThreadSleep (1);
    CLEAR_USERBLOCK (pCurThread);
    DEBUGMSG (ZONE_ENTRY, (L"NKSleepTillTick exit\r\n"));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
UB_Sleep(
    DWORD cMilliseconds
    )
{
    SET_USERBLOCK(pCurThread);
    NKSleep(cMilliseconds);
    CLEAR_USERBLOCK(pCurThread);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
ZeroTLS(
    PTHREAD pth
    )
{
    memset(pth->tlsPtr,0,4*TLS_MINIMUM_AVAILABLE);
}


LPCRITICAL_SECTION csarray[] = {
    &ODScs,     // 0
    &PhysCS,    // 1
    NULL,       // 2   &g_pLoaderPool->cs       CSARRAY_LOADERPOOL
    NULL,       // 3   &g_pFilePool->cs         CSARRAY_FILEPOOL
    &rtccs,     // 4
    &NameCS,    // 5
    &CompCS,    // 6
    &MapCS,     // 7
    NULL,       // 8   &FlushCS                 CSARRAY_FLUSH
    &PagerCS,   // 9
    &ModListcs, // 10
    &RFBcs,     // 11
    &PageOutCS, // 12  
    NULL,       // 13  &g_pprcNK->csLoader      CSARRAY_NKLOADER
    &WDcs,      // 14
};

#define NUM_CSARRAY (sizeof(csarray) / sizeof(csarray[0]))

BOOL CanTakeCS (LPCRITICAL_SECTION lpcs)
{
    if (!OwnCS (lpcs)) {
        int i;
        for (i = 0; i < NUM_CSARRAY; i ++) {
            if (lpcs == csarray[i]) {
                // no CS order violation, okay to take it
                break;
            }
            if (csarray[i] && OwnCS (csarray[i])) {
                // CS order violation
                return FALSE;
            }
        }
    }

    return TRUE;
}

#ifdef DEBUG

#define MAX_UNKCNT 20
LPCRITICAL_SECTION csUnknown[MAX_UNKCNT];
DWORD dwUnkCnt;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
CheckTakeCritSec(
    LPCRITICAL_SECTION lpcs
    )
{
    DWORD loop, CSIndex;
    // anyone can call the debugger, and it may cause demand loads... but then they can't call the debugger
    if (InDebugger || (lpcs == &csDbg))
        return;
    // We expect ethdbg's per-client critical sections to be unknown
    CSIndex = NUM_CSARRAY;
    for (loop = 0; loop < NUM_CSARRAY; loop++)
        if (csarray[loop] == lpcs) {
            CSIndex = loop;
            break;
        }
    if (loop == NUM_CSARRAY) {
        for (loop = 0; loop < dwUnkCnt; loop++)
            if (csUnknown[loop] == lpcs)
                return;
        if (dwUnkCnt == MAX_UNKCNT) {
            DEBUGMSG(ZONE_SCHEDULE,(L"CHECKTAKECRITSEC: Unknown tracking space exceeded!\r\n"));
            return;
        }
        csUnknown[dwUnkCnt++] = lpcs;
        return;
    }
    for (loop = 0; loop < NUM_CSARRAY; loop++) {
        if (csarray[loop] == lpcs)
            return;

        if (csarray[loop] && OwnCS (csarray[loop])) {
            DEBUGMSG(1, (L"CHECKTAKECRITSEC: Violation of critical section ordering, holding CS %8.8lx (%u) while taking CS %8.8lx (%u)\r\n",
                         csarray[loop],loop,lpcs,CSIndex));
            DEBUGCHK(0);
            return;
        }
    }
    DEBUGMSG(1,(L"CHECKTAKECRITSEC: Bug in CheckTakeCritSec!\r\n"));
}

#endif

#ifdef START_KERNEL_MONITOR_THREAD



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD WINAPI
Monitor1(LPVOID pUnused)
{
    while (1) {
        Sleep(30000);
        NKDbgPrintfW(L"\r\n\r\n");
        NKDbgPrintfW(L" ODScs -> %8.8lx\r\n",ODScs.OwnerThread);
        NKDbgPrintfW(L" CompCS -> %8.8lx\r\n",CompCS.OwnerThread);
        NKDbgPrintfW(L" PhysCS -> %8.8lx\r\n",PhysCS.OwnerThread);
        NKDbgPrintfW(L" LLcs -> %8.8lx\r\n",LLcs.OwnerThread);
        NKDbgPrintfW(L" ModListcs -> %8.8lx\r\n",ModListcs.OwnerThread);
        NKDbgPrintfW(L" RFBcs -> %8.8lx\r\n",RFBcs.OwnerThread);
        NKDbgPrintfW(L" MapCS -> %8.8lx\r\n",MapCS.OwnerThread);
        NKDbgPrintfW(L" NameCS -> %8.8lx\r\n",NameCS.OwnerThread);
        NKDbgPrintfW(L" PagerCS -> %8.8lx\r\n",PagerCS.OwnerThread);
        NKDbgPrintfW(L" rtccs -> %8.8lx\r\n",rtccs.OwnerThread);
        if (csarray[CSARRAY_NKLOADER])
            NKDbgPrintfW(L" g_pprcNK->csLoader -> %8.8lx\r\n", csarray[CSARRAY_NKLOADER]->OwnerThread);
        if (csarray[CSARRAY_LOADERPOOL])
            NKDbgPrintfW(L" g_pLoaderPool->cs -> %8.8lx\r\n", csarray[CSARRAY_LOADERPOOL]->OwnerThread);
        if (csarray[CSARRAY_FILEPOOL])
            NKDbgPrintfW(L" g_pFilePool->cs -> %8.8lx\r\n", csarray[CSARRAY_FILEPOOL]->OwnerThread);
        if (csarray[CSARRAY_FLUSH])
            NKDbgPrintfW(L" FlushCS -> %8.8lx\r\n", csarray[CSARRAY_FLUSH]->OwnerThread);
        NKDbgPrintfW(L"\r\n\r\n");
    }

    return 0;
}

#endif



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
DequeueProxy(
    PPROXY pProx
    )
{
    switch (pProx->bType) {
        case SH_CURTHREAD:
        case HT_MANUALEVENT:
            KCall((PKFN)DequeueFlatProxy,pProx);
            break;
        case HT_MUTEX:
            if (KCall((PKFN)DequeuePrioProxy,pProx))
                KCall((PKFN)DoReprioMutex,pProx->pObject);
            break;
        case HT_CRITSEC:
            if (KCall((PKFN)DequeuePrioProxy,pProx))
                KCall((PKFN)DoReprioCrit,pProx->pObject);
            break;
        case HT_EVENT:
        case HT_SEMAPHORE:
            KCall((PKFN)DequeuePrioProxy,pProx);
            break;
        default:
            DEBUGCHK(0);
    }
}

#define NUM_STACK_PROXY     4

void FreeProxyList (PPROXY pHeadProx)
{
    PPROXY pCurrProx;
    while (pHeadProx) {
        pCurrProx = pHeadProx;
        pHeadProx = pHeadProx->pThLinkNext;
        FreeMem (pCurrProx, HEAP_PROXY);
    }
}

static PPROXY AllocateProxy (DWORD cObjects, PPROXY pStkProx)
{
    PPROXY pHeadProx = NULL, pCurrProx;
    DWORD  idx;

    PREFAST_DEBUGCHK (cObjects && (cObjects <= MAXIMUM_WAIT_OBJECTS));
    if (NUM_STACK_PROXY >= cObjects) {
        pHeadProx = pStkProx;
        for (idx = 0; idx < cObjects - 1; idx ++) {
            pStkProx[idx].pThLinkNext = &pStkProx[idx+1];
        }
        pStkProx[idx].pThLinkNext = NULL;
    } else {
        for (idx = 0; idx < cObjects; idx ++) {

            if (!(pCurrProx = (PPROXY) AllocMem (HEAP_PROXY))) {
                break;
            }
            pCurrProx->pThLinkNext = pHeadProx;
            pHeadProx = pCurrProx;
        }

        if (idx < cObjects) {
            // failed allocating memory
            FreeProxyList (pHeadProx);
            pHeadProx = NULL;
            KSetLastError (pCurThread,ERROR_OUTOFMEMORY);
        }
    }

    return pHeadProx;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD DoWaitForObjects (
    DWORD  cObjects,
    PHDATA *phds,
    DWORD  dwTimeout
    )
{
    DWORD  retval = WAIT_OBJECT_0;
    PPROXY pHeadProx;
    PROXY  _proxys[NUM_STACK_PROXY];

    DEBUGMSG(ZONE_ENTRY,(L"DoWaitForObjects entry: %8.8lx %8.8lx %8.8lx\r\n",
        cObjects,phds,dwTimeout));

    PREFAST_DEBUGCHK (cObjects && (cObjects <= MAXIMUM_WAIT_OBJECTS));

    CELOG_WaitForMultipleObjects(cObjects, phds, dwTimeout);

    if (!(pHeadProx = AllocateProxy (cObjects, _proxys))) {
        retval = WAIT_FAILED;

    } else {

        PTHREAD pth = pCurThread;
        DWORD   idx;
        DWORD   dwContinue;
        PPROXY  pCurrProx;
        PEVENT  lpe;
        PCRIT   pCritMut;
        CLEANEVENT _ce = {0};
        CLEANEVENT *lpce = &_ce;

        // initialize all proxies .
        for (pCurrProx = pHeadProx, idx = 0; pCurrProx; pCurrProx = pCurrProx->pThLinkNext, idx ++) {
            DEBUGCHK (idx < cObjects);

            pCurrProx->pQNext  = pCurrProx->pQPrev = 0;
            pCurrProx->wCount  = pth->wCount;
            pCurrProx->pObject = phds[idx]->pvObj;
            pCurrProx->bType   = phds[idx]->pci->type;
            pCurrProx->pTh     = pth;
            pCurrProx->prio    = (BYTE)GET_CPRIO(pth);
            pCurrProx->dwRetVal = WAIT_OBJECT_0 + idx;

            if ((lpe = GetEventPtr (phds[idx])) && lpe->phdIntr) {
                // interrupt event special case
                if (1 != cObjects) {
                    DEBUGMSG (1, (L"ERROR: Interrupt Event can only be waited with WaitForSingleObject!\r\n"));
                    DEBUGCHK (0);
                    if (_proxys != pHeadProx) {
                        FreeProxyList (pHeadProx);
                    }
                    KSetLastError (pth, ERROR_INVALID_PARAMETER);
                    retval = WAIT_FAILED;
                } else {
                    // indicate interrupt event
                    lpce = NULL;
                }
                break;
            }
        }

        if (WAIT_FAILED != retval) {

            if (dwTimeout + 1 > 1) {  // the test fail only for 0xffffffff and 0
                dwTimeout = (dwTimeout < MAX_TIMEOUT)? (dwTimeout+1) : MAX_TIMEOUT;
            }

            pth->dwPendTime = dwTimeout;
            pth->dwPendWakeup = OEMGetTickCount () + dwTimeout;
            KCall ((PKFN)WaitConfig, pHeadProx, lpce);
            dwContinue = 2;
            do {
                pCritMut = 0;
                retval = KCall((PKFN)WaitOneMore, &dwContinue, &pCritMut);
                DEBUGMSG ((WAIT_FAILED == retval) && !lpce,
                    (L"ERROR: More then one thread waiting on the same interrupt Event!\r\n"));
                // more than one thread waiting on a single interrupt event if we hit the debugchk
                DEBUGCHK ((WAIT_FAILED != retval) || lpce);

                if (GET_NEEDSLEEP (pth)) {
                    // priority changed while sleeping, just retry with current timeout settings
                    dwContinue = 2;
                } else if (pCritMut) {
                    if (!dwContinue)
                        KCall((PKFN)LaterLinkCritMut,pCritMut,0);
                    else
                        KCall((PKFN)PostBoostMut,pCritMut);
                }
            } while (dwContinue);
            DEBUGCHK(lpce == pth->lpce);
            if (lpce) {
                pth->lpce = 0;

                // remove proxy from all waiting queue
                for (pCurrProx = (PPROXY)lpce->base; pCurrProx; pCurrProx = pCurrProx->pThLinkNext) {
                    DequeueProxy(pCurrProx);
                }

                // free memory if proxies are not on stack (cObjects > 4)
                if (_proxys != pHeadProx) {
                    FreeProxyList ((PPROXY)lpce->base);
                    FreeProxyList ((PPROXY)lpce->size);
                }
            }
        }
    }
    DEBUGMSG(ZONE_ENTRY,(L"DoWaitForObjects exit: %8.8lx\r\n",retval));
    return retval;
}

PHDATA LockWaitableObject (HANDLE h, PPROCESS pprc)
{
    PHDATA phd = LockHandleParam (h, pprc);
    if (phd) {
        switch (phd->pci->type) {
        case SH_CURPROC:
            {
                // a process handle, change it to the "process event"
                PHDATA phdEvt = ((PPROCESS) phd->pvObj)->phdProcEvt;
                LockHDATA (phdEvt);     // lock "process event"
                UnlockHandleData (phd); // unlock the process
                phd = phdEvt;           // return the process event
            }
            break;
        case SH_CURTHREAD:
        case HT_EVENT:
        case HT_MUTEX:
        case HT_SEMAPHORE:
            // sync object, valid
            break;
        default:
            // handles that are not waitable
            UnlockHandleData (phd);
            phd = NULL;
            break;
        }
    }

    return phd;
}

//------------------------------------------------------------------------------
// Wait for multiple objects, handles are from process pprc
//------------------------------------------------------------------------------
DWORD NKWaitForMultipleObjects (
    DWORD cObjects,
    CONST HANDLE *lphObjects,
    BOOL fWaitAll,
    DWORD dwTimeout
)
{
    DWORD    dwRet = WAIT_FAILED;
    PPROCESS pprc = pActvProc;
    DWORD    dwErr = 0;

    if (fWaitAll || !cObjects || (cObjects > MAXIMUM_WAIT_OBJECTS)) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {
        PHDATA phds[MAXIMUM_WAIT_OBJECTS];
        DWORD  loop;

        // lock the handles we're waiting for
        __try {
            // accessing lphObjects[i], user pointer, need to try-except
            for (loop = 0; loop < cObjects; loop ++) {
                if (!(phds[loop] = LockWaitableObject (lphObjects[loop], pprc))) {
                    dwErr = ERROR_INVALID_HANDLE;
                    break;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = ERROR_INVALID_PARAMETER;
        }

        if (dwErr) {
            cObjects = loop;    // so we can unlock handles we locked so far

        } else {
            dwRet = DoWaitForObjects (cObjects, phds, dwTimeout);
        }

        // unlock the handle we locked
        for (loop = 0; loop < cObjects; loop ++) {
            UnlockHandleData (phds[loop]);
        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }
    return dwRet;
}


static DWORD DoWaitForSingleObject (PPROCESS pprc, HANDLE h, DWORD dwTimeout)
{
    PHDATA phd = LockWaitableObject (h, pprc);
    DWORD  dwRet = WAIT_FAILED;

    if (phd) {

        dwRet = DoWaitForObjects (1, &phd, dwTimeout);

        UnlockHandleData (phd);

    } else {
        KSetLastError (pCurThread, ERROR_INVALID_HANDLE);
    }
    return dwRet;
}


//------------------------------------------------------------------------------
// Wait for single object (h is a handle in pprc)
//------------------------------------------------------------------------------
DWORD NKWaitForSingleObject (HANDLE h, DWORD dwTimeout)
{
    return DoWaitForSingleObject (pActvProc, h, dwTimeout);
}

//
// Wait for single object (h is a handle in kernel)
//
DWORD NKWaitForKernelObject (HANDLE h, DWORD dwTimeout)
{
    return DoWaitForSingleObject (g_pprcNK, h, dwTimeout);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
UB_WaitForMultipleObjects(
    DWORD cObjects,
    CONST HANDLE *lphObjects,
    BOOL fWaitAll,
    DWORD dwTimeout,
    LPDWORD lpRetVal
    ) 
{
    DEBUGCHK(lpRetVal);
    SET_USERBLOCK(pCurThread);
    *lpRetVal = NKWaitForMultipleObjects (cObjects, lphObjects, fWaitAll, dwTimeout);    
    CLEAR_USERBLOCK(pCurThread);
    return TRUE;
}


/* When a thread releases a Critical Section */


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
PuntCritSec(
    CRITICAL_SECTION *pcs
    )
{
    if (OwnCS (pcs)) {
        ERRORMSG(1,(L"Abandoning CS %8.8lx in PuntCritSec\r\n",pcs));
        pcs->LockCount = 1;
        LeaveCriticalSection(pcs);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
SurrenderCritSecs(void)
{
    PuntCritSec(&rtccs);
    PuntCritSec(&CompCS);
    PuntCritSec(&PagerCS);
    PuntCritSec(&PhysCS);
    PuntCritSec(&ODScs);
    PuntCritSec(&RFBcs);
    PuntCritSec(&MapCS);
    PuntCritSec(&NameCS);
    PuntCritSec(&ModListcs);
    PuntCritSec(&csDbg);
    if (csarray[CSARRAY_NKLOADER])
        PuntCritSec(csarray[CSARRAY_NKLOADER]);
    if (csarray[CSARRAY_LOADERPOOL])
        PuntCritSec(csarray[CSARRAY_LOADERPOOL]);
    if (csarray[CSARRAY_FILEPOOL])
        PuntCritSec(csarray[CSARRAY_FILEPOOL]);
    if (csarray[CSARRAY_FLUSH])
        PuntCritSec(csarray[CSARRAY_FLUSH]);
}


DWORD WINAPI PowerHandlerGuardThrd (LPVOID pUnused)
{
    DEBUGMSG (ZONE_SCHEDULE, (L"PowerHandler Guard Thread started\r\n"));

    DEBUGCHK (GET_BPRIO(pCurThread) == THREAD_PWR_GUARD_PRIORITY);
    while (1) {
        NKWaitForSingleObject (hEvtPwrHndlr, INFINITE);
        if (PowerOffFlag) {
            NKDbgPrintfW (L"ERROR: Power Handler function yield to low priority thread.\r\n");
            DebugBreak ();
        }
        // go back to the wait again
    }
    return 0;
}

DWORD dwNKAlarmThrdPrio = MAX_CE_PRIORITY_LEVELS-1;

#if defined(x86)
// Turn off FPO optimization for base functions so that debugger can correctly unwind retail x86 call stacks
#pragma optimize("y",off)
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
SystemStartupFunc(
    ulong param
    )
{
    HANDLE hTh;

    // record PendEvent address for SetInterruptEvent
    KInfoTable[KINX_PENDEVENTS] = (DWORD) &PendEvents1;

    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);

    KernelInit2();

    // adjust alarm resolution if it it's not in bound
    if (g_pOemGlobal->dwAlarmResolution < MIN_NKALARMRESOLUTION_MSEC)
        g_pOemGlobal->dwAlarmResolution = MIN_NKALARMRESOLUTION_MSEC;
    else if (g_pOemGlobal->dwAlarmResolution > MAX_NKALARMRESOLUTION_MSEC)
        g_pOemGlobal->dwAlarmResolution = MAX_NKALARMRESOLUTION_MSEC;
    
    VERIFY (LoaderInit ());
    
    // initialize the compiler /GS cookie - this must happen before other threads
    // start running
    __security_init_cookie();

    PagePoolInit ();

    // This can only be done after the loader initialization
    LoggerInit();           // Initialization for CeLog, profiler, code-coverage, etc.
    SysDebugInit ();        // initialize System Debugger (HW Debug stub, Kernel dump capture, SW Kernel Debug stub)

    // do this now, so that we continue running after we've created the new thread
#ifdef START_KERNEL_MONITOR_THREAD
    hTh = CreateKernelThread(Monitor1,0,THREAD_RT_PRIORITY_ABOVE_NORMAL,0);
    HNDLCloseHandle (g_pprcNK, hTh);
#endif

    pCleanupThread = pCurThread;
    hAlarmThreadWakeup = NKCreateEvent(0,0,0,0);
    DEBUGCHK(hAlarmThreadWakeup);
    InitializeCriticalSection(&rtccs);
    IntrEvents[SYSINTR_RTC_ALARM-SYSINTR_DEVICES] = LockIntrEvt (hAlarmThreadWakeup);
    DEBUGCHK(IntrEvents[SYSINTR_RTC_ALARM-SYSINTR_DEVICES]->phdIntr);

    // Give the OEM a final chance to do a more full-featured init before any
    // apps are started
    KernelIoctl (IOCTL_HAL_POSTINIT, NULL, 0, NULL, 0, NULL);

    InitMsgQueue ();
    InitWatchDog ();

    // create the power handler event and guard thread
    hEvtPwrHndlr = NKCreateEvent (NULL, FALSE, FALSE, NULL);
    DEBUGCHK (hEvtPwrHndlr);
    hTh = CreateKernelThread (PowerHandlerGuardThrd, NULL, THREAD_PWR_GUARD_PRIORITY, 0);
    HNDLCloseHandle (g_pprcNK, hTh);

    // dirty page event, initially set
    hEvtDirtyPage = NKCreateEvent (NULL, FALSE, TRUE, NULL);    
    DEBUGCHK (hEvtDirtyPage);
    pEvtDirtyPage = GetEventPtr(LockHandleData(hEvtDirtyPage,g_pprcNK));
    DEBUGCHK (pEvtDirtyPage);

    // we don't want to waste a thread here (create a separate for cleaning dirty pages).
    // Instead, RunApps thread will become "CleanDirtyPage" thread once filesys started
    hTh = CreateKernelThread (RunApps,0,THREAD_RT_PRIORITY_NORMAL,0);
    HNDLCloseHandle (g_pprcNK, hTh);

#define ONE_DAY     86400000

    while (1) {
        KCall((PKFN)SetThreadBasePrio, pCurThread, dwNKAlarmThrdPrio);
        NKWaitForSingleObject (hAlarmThreadWakeup, ONE_DAY);
        NKRefreshKernelAlarm ();
        PageOutIfNeeded();
    }
}

#if defined(x86)
// Re-Enable optimization
#pragma optimize("",on)
#endif

