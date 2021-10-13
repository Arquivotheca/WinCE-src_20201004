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
CRITICAL_SECTION PhysCS, ODScs, RFBcs, rtccs, PageOutCS, WDcs, PslServerCS;

PHDATA phdAlarmThreadWakeup;
extern PTHREAD pPageOutThread;

#define THREAD_PWR_GUARD_PRIORITY       1
PHDATA phdEvtPwrHndlr;

FARPROC pExitThread;
FARPROC pUsrExcpExitThread;
FARPROC pKrnExcpExitThread;
FARPROC pCaptureDumpFileOnDevice;
FARPROC pKCaptureDumpFileOnDevice;

ERRFALSE (offsetof(MUTEX, proxyqueue)       == offsetof(EVENT, proxyqueue));
ERRFALSE (offsetof(SEMAPHORE, proxyqueue)   == offsetof(EVENT, proxyqueue));

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

    // set pageout thread priority
    if (currmaxprio < GET_CPRIO(pPageOutThread))
        SCHL_SetThreadBasePrio (pPageOutThread, currmaxprio);

    DEBUGMSG(ZONE_ENTRY,(L"NKSetLowestScheduledPriority exit: %8.8lx\r\n",retval));
    return retval;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
GoToUserTime()
{
    PPCB  ppcb;
    PTHREAD pCurTh;
    DWORD dwCurrentTime;

    PcbIncOwnSpinlockCount ();
    ppcb   = GetPCB ();
    pCurTh = ppcb->pCurThd;
    SET_TIMEUSER(pCurTh);
    dwCurrentTime = GETCURRTICK ();
    pCurTh->dwKTime += dwCurrentTime - ppcb->dwPrevTimeModeTime;
    ppcb->dwPrevTimeModeTime = dwCurrentTime;
    PcbDecOwnSpinlockCount ();

    if (ppcb->bResched) {
        KCall ((PKFN) ReturnFalse);
    }
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (dwCurrentTime & 0x1f);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
GoToKernTime()
{
    DWORD dwCurrentTime;
    PPCB  ppcb;
    PTHREAD pCurTh;

    PcbIncOwnSpinlockCount ();
    ppcb   = GetPCB ();
    pCurTh = ppcb->pCurThd;
    SET_TIMEKERN(pCurTh);
    dwCurrentTime = GETCURRTICK ();
    pCurTh->dwUTime += dwCurrentTime - ppcb->dwPrevTimeModeTime;
    ppcb->dwPrevTimeModeTime = dwCurrentTime;
    PcbDecOwnSpinlockCount ();

    if (ppcb->bResched) {
        KCall ((PKFN) ReturnFalse);
    }
    randdw2 = ((randdw2<<5) | (randdw2>>27)) ^ (dwCurrentTime & 0x1f);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
ThreadSleep (
    DWORD time
    )
{
    PTHREAD pCurTh = pCurThread;
    SLEEPSTRUCT sleeper;
    DEBUGCHK (!(time & 0x80000000));
    sleeper.pth = NULL;
    sleeper.wDirection = 0;
    pCurTh->dwWakeupTime = OEMGetTickCount () + time;

    while (SCHL_PutThreadToSleep (&sleeper) || GET_NEEDSLEEP (pCurTh))
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
        SCHL_BlockCurThread ();
    else if (!cMilliseconds)
        SCHL_ThreadYield ();
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
    PTHREAD pCurTh = pCurThread;
    DEBUGMSG (ZONE_ENTRY, (L"NKSleepTillTick entry\r\n"));
    SET_USERBLOCK (pCurTh);
    ThreadSleep (1);
    CLEAR_USERBLOCK (pCurTh);
    DEBUGMSG (ZONE_ENTRY, (L"NKSleepTillTick exit\r\n"));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
UB_Sleep(
    DWORD cMilliseconds
    )
{
    PTHREAD pCurTh = pCurThread;
    SET_USERBLOCK (pCurTh);
    NKSleep(cMilliseconds);
    CLEAR_USERBLOCK (pCurTh);
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
    NULL,       // 2   &g_LoaderPool.cs       CSARRAY_LOADERPOOL
    NULL,       // 3   &g_FilePool.cs         CSARRAY_FILEPOOL
    &rtccs,     // 4
    &NameCS,    // 5
    &CompCS,    // 6
    &MapCS,     // 7
    &WDcs,      // 8
    NULL,       // 9   &FlushCS                 CSARRAY_FLUSH
    &PagerCS,   // 10
    &RFBcs,     // 11
    &ModListcs, // 12
    &PageOutCS, // 13  
    &PslServerCS, // 14
    NULL,       // 15  &g_pprcNK->csLoader      CSARRAY_NKLOADER
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


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void DequeueProxy (PPROXY pProx)
{
    if (pProx->pQDown) {
        switch (pProx->bType) {
        case SH_CURTHREAD:
        case HT_MANUALEVENT:
            SCHL_DequeueFlatProxy (pProx);
            break;
        case HT_MUTEX:
            {
                PMUTEX pMutex = (PMUTEX) pProx->pObject;
                if (SCHL_DequeuePrioProxy (pProx) && (pMutex->pOwner != pCurThread)) {
                    SCHL_DoReprioCritMut (pMutex);
                }
            }
            break;
        case HT_EVENT:
        case HT_SEMAPHORE:
            SCHL_DequeuePrioProxy (pProx);
            break;
        default:
            DEBUGCHK(0);
        }
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
    PPROXY pHeadProx;
    DWORD  idx;

    PREFAST_DEBUGCHK (cObjects && (cObjects <= MAXIMUM_WAIT_OBJECTS));
    if (NUM_STACK_PROXY >= cObjects) {
        pHeadProx = pStkProx;
        for (idx = 0; idx < cObjects - 1; idx ++) {
            pStkProx[idx].pThLinkNext = &pStkProx[idx+1];
        }
        pStkProx[idx].pThLinkNext = NULL;
    } else {
        PPROXY pCurrProx;
        pHeadProx = NULL;
        for (idx = 0; idx < cObjects; idx ++) {
            pCurrProx = (PPROXY) AllocMem (HEAP_PROXY);
            if (!pCurrProx) {
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

static void InitWaitStruct (PTHREAD pth, PWAITSTRUCT pws, DWORD cObjects, PHDATA *phds, DWORD dwTimeout)
{
    pws->cObjects  = cObjects;
    pws->phds      = phds;
    pws->dwTimeout = dwTimeout;
    pws->fEnqueue  = dwTimeout || (pws->cObjects > 1);
    pws->pfnWait   = NULL;
    pws->pProxyEnqueued = NULL;
    if (dwTimeout && (INFINITE != dwTimeout)) {
        if (dwTimeout < MAX_TIMEOUT) {
            pws->dwTimeout ++;
        } else {
            pws->dwTimeout = MAX_TIMEOUT;
        }
        pth->dwWakeupTime         = OEMGetTickCount () + pws->dwTimeout;
        pws->sleeper.pth          = NULL;
        pws->sleeper.wDirection   = 0;
    }
}

static void InitProxyList (PTHREAD pth, PPROXY pProx, PWAITSTRUCT pws)
{
    PHDATA  *phds = pws->phds;
    DWORD   idx;

    // initialize wait struct
    pws->pProxyPending  = pProx;

    // initialize all proxies .
    for (idx = 0; pProx; pProx = pProx->pThLinkNext, idx ++) {
        DEBUGCHK (idx < pws->cObjects);
    
        pProx->wCount  = pth->wCount;
        pProx->pObject = phds[idx]->pvObj;
        pProx->bType   = phds[idx]->pci->type;
        pProx->pTh     = pth;
        pProx->prio    = (BYTE)GET_CPRIO(pth);
        pProx->dwRetVal = WAIT_OBJECT_0 + idx;
    }


    // setup thread state
    // Changing Wait State here is safe without holding spinlock, for we're not in any queue, and
    // there is no way any other thread/interrupt can wake us up.
    pth->bWaitState = WAITSTATE_PROCESSING;
    

}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static DWORD DoWaitWithWaitStruct (
    PWAITSTRUCT pws
    )
{
    DWORD   dwWaitResult    = WAIT_FAILED;
    PTHREAD pCurTh          = pCurThread;
    DWORD   cObjects        = pws->cObjects;
    PPROXY  pHeadProx, pCurrProx;
    PROXY   _proxys[NUM_STACK_PROXY];

    PREFAST_DEBUGCHK (cObjects && (cObjects <= MAXIMUM_WAIT_OBJECTS));

    DEBUGCHK (!pCurTh->lpProxy);

    CELOG_WaitForMultipleObjects (cObjects, pws->phds, pws->dwTimeout);

    // allocate proxy if needed
    pHeadProx = AllocateProxy (cObjects, _proxys);
    if (pHeadProx) {

        // initialize proxy list and wait struct
        InitProxyList (pCurTh, pHeadProx, pws);

        // iterate through the object one by one until signaled or block
        do {
            pws->pMutex = NULL;

            while (SCHL_WaitOneMore (pws)) {
                
                if (pws->pMutex) {
                    SCHL_BoostMutexOwnerPriority (pCurTh, pws->pMutex);
                    pws->pMutex = NULL;
                }
            }

        } while (GET_NEEDSLEEP (pCurTh));

        DEBUGCHK (!pCurTh->lpProxy);

        // get the wait result
        dwWaitResult = pCurTh->dwWaitResult;

        // remove proxy from all waiting queue
        for (pCurrProx = pws->pProxyEnqueued; pCurrProx; pCurrProx = pCurrProx->pThLinkNext) {
            DequeueProxy (pCurrProx);
        }

        if ((dwWaitResult - WAIT_OBJECT_0) < cObjects) {
            // the wait is successful
            // special handling for mutex - ownership assignment and priority boosting
            PHDATA phd = pws->phds[dwWaitResult - WAIT_OBJECT_0];

            if (&cinfMutex == phd->pci) {
                SCHL_LinkMutexOwner (pCurTh, (PMUTEX) phd->pvObj);
            }
        }

        // free memory if proxies are not on stack (cObjects > 4)
        if (_proxys != pHeadProx) {
            FreeProxyList (pws->pProxyEnqueued);
            FreeProxyList (pws->pProxyPending);
        }
    }

    if (WAIT_FAILED == dwWaitResult) {
        KSetLastError (pCurTh, ERROR_INVALID_PARAMETER);
    }
        
    return dwWaitResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD DoWaitForObjects (
    DWORD  cObjects,
    PHDATA *phds,
    DWORD  dwTimeout
    )
{
    WAITSTRUCT ws;
    DWORD      dwWaitResult;
    
    DEBUGMSG (ZONE_ENTRY,(L"DoWaitForObjects entry: %8.8lx %8.8lx %8.8lx\r\n",
                            cObjects, phds, dwTimeout));

    InitWaitStruct (pCurThread, &ws, cObjects, phds, dwTimeout);

    dwWaitResult = DoWaitWithWaitStruct (&ws);

    DEBUGMSG (ZONE_ENTRY,(L"DoWaitForObjects exit: %8.8lx\r\n", dwWaitResult));
    return dwWaitResult;
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
BOOL NKWaitForMultipleObjects (
    DWORD cObjects,
    CONST HANDLE *lphObjects,
    BOOL fWaitAll,
    DWORD dwTimeout,
    LPDWORD lpRetVal
)
{
    DWORD    dwWaitResult = WAIT_FAILED;
    PTHREAD  pCurTh = pCurThread;
    DWORD    dwErr  = 0;

    if (fWaitAll || !cObjects || (cObjects > MAXIMUM_WAIT_OBJECTS)) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {
        PPROCESS pprc = pCurTh->pprcActv;
        PHDATA phds[MAXIMUM_WAIT_OBJECTS];
        DWORD  loop = 0;

        // lock the handles we're waiting for
        __try {
            // accessing lphObjects[i], user pointer, need to try-except
            for (loop = 0; loop < cObjects; loop ++) {
                phds[loop] = LockWaitableObject (lphObjects[loop], pprc);
                if (!phds[loop]) {
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
            WAITSTRUCT ws;
            InitWaitStruct (pCurTh, &ws, cObjects, phds, dwTimeout);
            SET_USERBLOCK (pCurTh);
            dwWaitResult = DoWaitWithWaitStruct (&ws);
            CLEAR_USERBLOCK (pCurTh);
        }

        // unlock the handle we locked
        for (loop = 0; loop < cObjects; loop ++) {
            UnlockHandleData (phds[loop]);
        }
    }

    *lpRetVal = dwWaitResult;

    if (dwErr) {
        KSetLastError (pCurTh, dwErr);
    }
    return !dwErr;
}


//------------------------------------------------------------------------------
// Wait for single object (h is a handle in kernel)
//
DWORD NKWaitForKernelObject (HANDLE h, DWORD dwTimeout)
{
    PHDATA phd = LockWaitableObject (h, g_pprcNK);
    DWORD  dwRet = WAIT_FAILED;

    if (phd) {

        dwRet = DoWaitForObjects (1, &phd, dwTimeout);

        UnlockHandleData (phd);

    } else {
        KSetLastError (pCurThread, ERROR_INVALID_HANDLE);
    }
    return dwRet;
}

//
// NKWaitForAPIReady - waiting for an API set to be ready
//
DWORD NKWaitForAPIReady (DWORD dwAPISet, DWORD dwTimeout)
{
    DWORD dwRet = WAIT_FAILED;

    if (dwAPISet < NUM_API_SETS) {
        
        if (dwTimeout && !SystemAPISets[dwAPISet]) {
            PTHREAD    pth = pCurThread;
            WAITSTRUCT ws;

            InitWaitStruct (pth, &ws, 1, &g_phdAPIRegEvt, dwTimeout);
            ws.pfnWait    = IsSystemAPISetReady;
            ws.dwUserData = dwAPISet;

            do {
                SET_USERBLOCK (pth);
                dwRet = DoWaitWithWaitStruct (&ws);
                CLEAR_USERBLOCK (pth);
            } while ((WAIT_OBJECT_0 == dwRet) && !SystemAPISets[dwAPISet]);
        }

        dwRet = SystemAPISets[dwAPISet]? WAIT_OBJECT_0 : WAIT_TIMEOUT;       
    }

    if (WAIT_OBJECT_0 != dwRet) {
        SetLastError (dwRet);
    }

    return dwRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD NKGetCPUInfo (DWORD dwProcessor, DWORD dwInfoType)
{
    DWORD dwRet = 0;
    DEBUGMSG (ZONE_ENTRY, (L"NKGetCPUInfo %8.8lx %8.8lx\r\n", dwProcessor, dwInfoType));
    if ((DWORD) (dwProcessor - 1) < g_pKData->nCpus) {
        PPCB ppcb = g_ppcbs[dwProcessor-1];
        switch (dwInfoType) {
        case CPUINFO_IDLE_TIME:
            if (g_pNKGlobal->dwIdleConv) {
                dwRet = (DWORD)(ppcb->liIdleTime.QuadPart/g_pNKGlobal->dwIdleConv);
            }
            
            break;
        case CPUINFO_POWER_STATE:
            dwRet = ppcb->dwCpuState;
            switch (dwRet) {
            case CE_PROCESSOR_STATE_POWERED_ON:
            case CE_PROCESSOR_STATE_POWERED_OFF:
                break;
            default:
                dwRet = CE_PROCESSOR_STATE_IN_TRANSITION;
            }
            break;
        default:
            DEBUGCHK (0);
        }
    } else {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG (ZONE_ENTRY, (L"NKGetCPUInfo returns %8.8lx\r\n", dwRet));
    return dwRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL  NKCPUPowerFunc (DWORD dwProcessor, BOOL fOnOff, DWORD dwHint)
{
    DWORD dwErr = ((dwProcessor > 1) && (dwProcessor <= g_pKData->nCpus))? 0 : ERROR_INVALID_PARAMETER;

    if (!dwErr) {
        // It looks strange that we're using PhysCS to guard against multi-thread trying to power off/on CPUs at the same time.
        // There is actually a reason for that - during powering off/on, we need to maintain cache state, therefore we need to make
        // sure that no more dirty pages are added into the memory pool (FreePhysPage not allowed).
        // 
        EnterCriticalSection (&PhysCS);
        if (fOnOff) {
            // power on
            if (CE_PROCESSOR_STATE_POWERED_OFF != g_ppcbs[dwProcessor-1]->dwCpuState) {
                dwErr = ERROR_NOT_READY;
            } else if (!SCHL_PowerOnCPU (dwProcessor)) {
                dwErr = ERROR_NOT_SUPPORTED;
            }
        } else {
            // power off
            if (CE_PROCESSOR_STATE_POWERED_ON != g_ppcbs[dwProcessor-1]->dwCpuState) {
                dwErr = ERROR_NOT_READY;
            } else if (!SCHL_PowerOffCPU (dwProcessor, dwHint)) {
                dwErr = ERROR_NOT_SUPPORTED;
            }
        }
        LeaveCriticalSection (&PhysCS);
    }
    if (dwErr) {
        NKSetLastError (dwErr);
    }
    
    return !dwErr;
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

        // we should've never hit this, or we're leaking CS
        DEBUGCHK (0);
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


PHDATA NKCreateAndLockEvent (BOOL fManualReset, BOOL fInitState)
{
    HANDLE hEvt   = NKCreateEvent (NULL, fManualReset, fInitState, NULL);
    PHDATA phdEvt = NULL;

    if (hEvt) {
        PPROCESS pprc = pActvProc;
        phdEvt = LockHandleData (hEvt, pprc);
        HNDLCloseHandle (pprc, hEvt);
    }
    return phdEvt;
}

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
    KInfoTable[KINX_PENDEVENTS] = (DWORD) &PendEvents1((PPCB) g_pKData);

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

    // initialize the OAL's interlocked functions table
    g_pOemGlobal->pfnInitInterlockedFunc();

    // Give the OEM a final chance to do a more full-featured init before any
    // apps are started
    KernelIoctl (IOCTL_HAL_POSTINIT, NULL, 0, NULL, 0, NULL);

    InitMsgQueue ();
    InitWatchDog ();

    // create the power handler event and guard thread

    StartAllCPUs ();
    g_pprcNK->bState = PROCESS_STATE_NORMAL; // OpenProcess (nk.exe) will succeed from this point on

    // we don't want to waste a thread here (create a separate for cleaning dirty pages).
    // Instead, RunApps thread will become "CleanDirtyPage" thread once filesys started
    hTh = CreateKernelThread (RunApps,0,THREAD_RT_PRIORITY_NORMAL,0);
    HNDLCloseHandle (g_pprcNK, hTh);

#define ONE_DAY     86400000

    for ( ; ; ) {
        DoWaitForObjects (1, &phdAlarmThreadWakeup, ONE_DAY);
        NKRefreshKernelAlarm ();
    }
}

#if defined(x86)
// Re-Enable optimization
#pragma optimize("",on)
#endif

