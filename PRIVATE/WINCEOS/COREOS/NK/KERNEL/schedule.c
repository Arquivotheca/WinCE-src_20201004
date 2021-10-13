//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include <kitlpriv.h>
#include <psapi.h>

BOOL PageOutForced, bLastIdle;

DWORD dwDefaultThreadQuantum;

typedef void (* LogThreadCreate_t)(DWORD, DWORD);
typedef void (* LogThreadDelete_t)(DWORD, DWORD);
typedef void (* LogProcessCreate_t)(DWORD);
typedef void (* LogProcessDelete_t)(DWORD);
typedef void (* LogThreadSwitch_t)(DWORD, DWORD);

LogThreadCreate_t pLogThreadCreate;
LogThreadDelete_t pLogThreadDelete;
LogProcessCreate_t pLogProcessCreate;
LogProcessDelete_t pLogProcessDelete;
LogThreadSwitch_t pLogThreadSwitch;

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

CRITICAL_SECTION csDbg, DbgApiCS, NameCS, CompCS, LLcs, ModListcs, WriterCS, MapCS, PagerCS;
CRITICAL_SECTION MapNameCS, PhysCS, ppfscs, ODScs, RFBcs, VAcs, rtccs, PageOutCS;
CRITICAL_SECTION DirtyPageCS;

#ifdef DEBUG
CRITICAL_SECTION EdbgODScs, TimerListCS;
#endif
BOOL fKITLcsInitialized;
CRITICAL_SECTION KITLcs;

THREADTIME *TTList;

#if defined(x86)
extern PTHREAD g_CurFPUOwner;
#endif

RunList_t RunList;
PTHREAD SleepList;
HANDLE hEventList;
HANDLE hMutexList;
HANDLE hSemList;
DWORD dwReschedTime;

DWORD currmaxprio = MAX_PRIORITY_LEVELS - 1;
DWORD CurrTime;
DWORD dwPrevReschedTime;
PTHREAD pOOMThread;

SYSTEMTIME CurAlarmTime;
DWORD dwNKAlarmResolutionMSec = 10 * 1000;      // 10 seconds
#define MIN_NKALARMRESOLUTION_MSEC      1000    // 1 second
#define MAX_NKALARMRESOLUTION_MSEC      60000   // 60 seconds

HANDLE hAlarmEvent, hAlarmThreadWakeup; 
PTHREAD pCleanupThread;

PROCESS ProcArray[MAX_PROCESSES];
PROCESS *kdProcArray = ProcArray;
struct KDataStruct *kdpKData = &KData;

HANDLE hCoreDll;
void (*TBFf)(LPVOID, ulong);
void (*MTBFf)(LPVOID, ulong, ulong, ulong, ulong);
void (*CTBFf)(LPVOID, ulong, ulong, ulong, ulong);
BOOL (*KSystemTimeToFileTime)(LPSYSTEMTIME, LPFILETIME);
LONG (*KCompareFileTime)(LPFILETIME, LPFILETIME);
BOOL (*KLocalFileTimeToFileTime)(const FILETIME *, LPFILETIME);
BOOL (*KFileTimeToSystemTime)(const FILETIME *, LPSYSTEMTIME);
void (*pPSLNotify)(DWORD, DWORD, DWORD);
void (*pSignalStarted)(DWORD);
BOOL (*pGetHeapSnapshot)(THSNAP *pSnap, BOOL bMainOnly, LPVOID *pDataPtr, HANDLE hProc);

LPVOID pExitThread;
LPVOID pExcpExitThread;
LPDWORD pIsExiting;
extern Name *pDebugger;
extern BOOL bDbgOnFirstChance;

void DoPageOut(void);
extern DWORD PageOutNeeded;
BYTE GetHighPos(DWORD);

// idle thread for cleaning dirty pages
void CleanDirtyPagesThread(LPVOID pv);

// for save/restore CoProcRegister
DWORD cbNKCoProcRegSize;
DWORD fNKSaveCoProcReg;
void (*pOEMSaveCoProcRegister) (LPBYTE pArea);
void (*pOEMRestoreCoProcRegister) (LPBYTE pArea);
void (*pOEMInitCoProcRegisterSavedArea) (LPBYTE pArea);

static BYTE W32PrioMap[MAX_WIN32_PRIORITY_LEVELS] = {
    MAX_PRIORITY_LEVELS-8,
    MAX_PRIORITY_LEVELS-7,
    MAX_PRIORITY_LEVELS-6,
    MAX_PRIORITY_LEVELS-5,
    MAX_PRIORITY_LEVELS-4,
    MAX_PRIORITY_LEVELS-3,
    MAX_PRIORITY_LEVELS-2,
    MAX_PRIORITY_LEVELS-1,
};

void (*pfnOEMMapW32Priority) (int nPrios, LPBYTE pPrioMap);

typedef struct RTs {
    PTHREAD pHelper;    // must be first
    DWORD dwBase, dwLen;// if we're freeing the stack
    DWORD dwOrigBase;   // if we are on a fiber stack, we need to free the original
    PPROCESS pProc;     // if we're freeing the proc
    LPTHRDDBG pThrdDbg; // if we're freeing a debug structure
    HANDLE hThread;     // if we're freeing a handle / threadtime
    PTHREAD pThread;    // if we're freeing a thread structure
    CLEANEVENT *lpce1;
    CLEANEVENT *lpce2;
    CLEANEVENT *lpce3;
    LPDWORD pdwDying;
} RTs;

const PFNVOID ThrdMthds[] = {
    (PFNVOID)SC_ThreadCloseHandle,
    (PFNVOID)0,
    (PFNVOID)UB_ThreadSuspend,
    (PFNVOID)SC_ThreadResume,
    (PFNVOID)SC_ThreadSetPrio,
    (PFNVOID)SC_ThreadGetPrio,
    (PFNVOID)SC_ThreadGetCode,
    (PFNVOID)SC_ThreadGetContext,
    (PFNVOID)SC_ThreadSetContext,
    (PFNVOID)SC_ThreadTerminate,
    (PFNVOID)SC_CeGetThreadPriority,
    (PFNVOID)SC_CeSetThreadPriority,
    (PFNVOID)SC_CeGetThreadQuantum,
    (PFNVOID)SC_CeSetThreadQuantum,
};

BOOL SC_ProcGetModInfo (HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpmodinfo, DWORD cb);
BOOL SC_ProcSetVer (HANDLE hProcess, DWORD dwVersion);

const PFNVOID ProcMthds[] = {
    (PFNVOID)SC_ProcCloseHandle,
    (PFNVOID)0,
    (PFNVOID)SC_ProcTerminate,
    (PFNVOID)SC_ProcGetCode,
    (PFNVOID)0,
    (PFNVOID)SC_ProcFlushICache,
    (PFNVOID)SC_ProcReadMemory,
    (PFNVOID)SC_ProcWriteMemory,
    (PFNVOID)SC_ProcDebug,
    (PFNVOID)SC_ProcGetModInfo,
    (PFNVOID)SC_ProcSetVer,
};

const PFNVOID EvntMthds[] = {
    (PFNVOID)SC_EventCloseHandle,
    (PFNVOID)0,
    (PFNVOID)SC_EventModify,
    (PFNVOID)SC_EventAddAccess,
    (PFNVOID)SC_EventGetData,
    (PFNVOID)SC_EventSetData,
};

const PFNVOID MutxMthds[] = {
    (PFNVOID)SC_MutexCloseHandle,
    (PFNVOID)0,
    (PFNVOID)SC_ReleaseMutex,
};

const PFNVOID SemMthds[] = {
    (PFNVOID)SC_SemCloseHandle,
    (PFNVOID)0,
    (PFNVOID)SC_ReleaseSemaphore,
};

const CINFO cinfThread = {
    "THRD",
    DISPATCH_KERNEL_PSL,
    SH_CURTHREAD,
    sizeof(ThrdMthds)/sizeof(ThrdMthds[0]),
    ThrdMthds
};

const CINFO cinfProc = {
    "PROC",
    DISPATCH_KERNEL_PSL,
    SH_CURPROC,
    sizeof(ProcMthds)/sizeof(ProcMthds[0]),
    ProcMthds
};

const CINFO cinfEvent = {
    "EVNT",
    DISPATCH_KERNEL_PSL,
    HT_EVENT,
    sizeof(EvntMthds)/sizeof(EvntMthds[0]),
    EvntMthds
};

const CINFO cinfMutex = {
    "MUTX",
    DISPATCH_KERNEL_PSL,
    HT_MUTEX,
    sizeof(MutxMthds)/sizeof(MutxMthds[0]),
    MutxMthds
};

const CINFO cinfSem = {
    "SEMP",
    DISPATCH_KERNEL_PSL,
    HT_SEMAPHORE,
    sizeof(SemMthds)/sizeof(SemMthds[0]),
    SemMthds
};

ERRFALSE(offsetof(EVENT, pProxList) == offsetof(PROCESS, pProxList));
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


#define MAX_SECURE_STACK    100         // max # of secure stack
LONG    g_CntSecureStk;

extern DWORD   ROMDllLoadBase;     // this is the low water mark for DLL loaded into per-process's slot
extern DWORD   SharedDllBase;      // base of dlls loaded in slot 1


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
GCFT(
    LPFILETIME lpFileTime
    ) 
{
    SYSTEMTIME st;
    OEMGetRealTime(&st);
    KSystemTimeToFileTime(&st,lpFileTime);
    KLocalFileTimeToFileTime(lpFileTime,lpFileTime);
}

void AddHandleToList (HANDLE *phList, HANDLE hObj, DWORD dwType)
{
    // since all the objects have the same 'look' for the first 4 fields,
    // we'll just use event to represent all.
    LPEVENT lpe;
    DEBUGCHK (hObj);
    DEBUGCHK (((HT_EVENT == dwType) && (&hEventList == phList))
        || ((HT_MUTEX == dwType) && (&hMutexList == phList))
        || ((HT_SEMAPHORE == dwType) && (&hSemList == phList)));
    lpe = (LPEVENT) GetObjectPtrByType (hObj, dwType);
    DEBUGCHK (lpe);
    lpe->hPrev = 0;

    EnterCriticalSection (&NameCS);

    if (lpe->hNext = *phList) {
        LPEVENT lpeNext = (LPEVENT) GetObjectPtrByType (lpe->hNext, dwType);
        DEBUGCHK (lpeNext);
        lpeNext->hPrev = hObj;
    }

    *phList = hObj;
    
    LeaveCriticalSection (&NameCS);
}

void RemoveHandleFromList (HANDLE *phList, HANDLE hObj, DWORD dwType)
{
    // since all the objects have the same 'look' for the first 4 fields,
    // we'll just use event to represent all.
    LPEVENT lpe;
    DEBUGCHK (phList && *phList && hObj);
    DEBUGCHK (((HT_EVENT == dwType) && (&hEventList == phList))
        || ((HT_MUTEX == dwType) && (&hMutexList == phList))
        || ((HT_SEMAPHORE == dwType) && (&hSemList == phList)));
    lpe = (LPEVENT) GetObjectPtrByType (hObj, dwType);
    DEBUGCHK (lpe);
    
    EnterCriticalSection (&NameCS);
    if (*phList == hObj) {
        // 1st element in the list, update the list
        *phList = lpe->hNext;
    } else {
        LPEVENT lpePrev = (LPEVENT) GetObjectPtrByType (lpe->hPrev, dwType);
        DEBUGCHK (lpePrev);
        lpePrev->hNext = lpe->hNext;
    }
    if (lpe->hNext) {
        LPEVENT lpeNext = (LPEVENT) GetObjectPtrByType (lpe->hNext, dwType);
        DEBUGCHK (lpeNext);
        lpeNext->hPrev = lpe->hPrev;
    }            
    LeaveCriticalSection (&NameCS);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoLinkCritMut(
    LPCRIT lpcrit,
    PTHREAD pth,
    BYTE prio
    ) 
{
    LPCRIT lpcrit2, lpcrit3;
    BYTE prio2;
    prio2 = prio/PRIORITY_LEVELS_HASHSCALE;
    DEBUGCHK(prio2 < PRIORITY_LEVELS_HASHSIZE);
    DEBUGCHK(IsKernelVa (lpcrit));
    if (!pth->pOwnedList || (prio < pth->pOwnedList->bListedPrio)) {
        // either pth doesn't own any critical section or lpcrit has higher priority
        // than the ones in its 'owned list'. Make lpcrit the head of its 'owned list' 
        // and update its hash table
        lpcrit->pPrevOwned = 0;
        if (lpcrit->pNextOwned = pth->pOwnedList) {
            lpcrit->pNextOwned->pPrevOwned = lpcrit;
        }
        lpcrit->pUpOwned = lpcrit->pDownOwned = pth->pOwnedHash[prio2] = pth->pOwnedList = lpcrit;
    } else {
        // pth owns a CS that has a higer priority thread blocked on it, find the right
        // place to enqueue lpcrit.
        if (!(lpcrit2 = pth->pOwnedHash[prio2])) {
            // find the preceeding non-zero hash table entry
            // bounded by PRIORITY_LEVELS_HASHSIZE
            pth->pOwnedHash[prio2] = lpcrit;
            while (!(lpcrit2 = pth->pOwnedHash[--prio2]))
                ;
        }
            
        if (prio < lpcrit2->bListedPrio) {
            // insert in front of lpcrit2 and replace the hash table entry
            DEBUGCHK (prio/PRIORITY_LEVELS_HASHSCALE == prio2);
            lpcrit->pPrevOwned = lpcrit2->pPrevOwned;
            lpcrit->pNextOwned = lpcrit2;
            lpcrit->pUpOwned = lpcrit->pDownOwned = pth->pOwnedHash[prio2] = lpcrit->pPrevOwned->pNextOwned = lpcrit2->pPrevOwned = lpcrit;
            
        } else {    

            // find the appropriate place to insert pth
            // bounded by MAX_PRIORITY_HASHSCALE
            while ((lpcrit3 = lpcrit2->pNextOwned) && (prio >= lpcrit3->bListedPrio))
                lpcrit2 = lpcrit3;

            DEBUGCHK (!lpcrit3 || (prio < lpcrit3->bListedPrio));
            DEBUGCHK (lpcrit2->bListedPrio <= prio);

            if (prio == lpcrit2->bListedPrio) {
                // insert at the end of the veritcal list of lpcrit2
                lpcrit->pUpOwned = lpcrit2->pUpOwned;
                lpcrit->pUpOwned->pDownOwned = lpcrit2->pUpOwned = lpcrit;
                lpcrit->pDownOwned = lpcrit2;
                lpcrit->pPrevOwned = lpcrit->pNextOwned = 0;
            } else {
                // insert between lpcrit2 and lpcrit3
                if (lpcrit->pNextOwned = lpcrit3)
                    lpcrit3->pPrevOwned = lpcrit;
                lpcrit->pPrevOwned = lpcrit2;
                lpcrit->pUpOwned = lpcrit->pDownOwned = lpcrit2->pNextOwned = lpcrit;
            }
        }
    }
    lpcrit->bListed = 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
LinkCritMut(
    LPCRIT lpcrit,
    PTHREAD pth,
    BOOL bIsCrit
    ) 
{ 
    BYTE prio;
    DEBUGCHK(IsKernelVa (lpcrit));
    DEBUGCHK(lpcrit->bListed != 1);
    if (!bIsCrit || (lpcrit->lpcs->needtrap && !GET_BURIED(pth))) {
        prio = lpcrit->bListedPrio = (lpcrit->pProxList ? lpcrit->pProxList->prio : MAX_PRIORITY_LEVELS-1);
        DoLinkCritMut(lpcrit,pth,prio);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
LaterLinkCritMut(
    LPCRIT lpcrit,
    BOOL bIsCrit
    ) 
{ 
    BYTE prio;
    KCALLPROFON(50);
    if (!lpcrit->bListed && (!bIsCrit || (lpcrit->lpcs->needtrap && !GET_BURIED(pCurThread)))) {
        prio = lpcrit->bListedPrio = (lpcrit->pProxList ? lpcrit->pProxList->prio : MAX_PRIORITY_LEVELS-1);
        DoLinkCritMut(lpcrit,pCurThread,prio);
    }
    KCALLPROFOFF(50);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
LaterLinkMutOwner(
    LPMUTEX lpm
    ) 
{ 
    BYTE prio;
    KCALLPROFON(55);
    if (!lpm->bListed) {
        prio = lpm->bListedPrio = (lpm->pProxList ? lpm->pProxList->prio : MAX_PRIORITY_LEVELS-1);
        DoLinkCritMut((LPCRIT)lpm,lpm->pOwner,prio);
    }
    KCALLPROFOFF(55);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
UnlinkCritMut(
    LPCRIT lpcrit,
    PTHREAD pth
    ) 
{ 
    LPCRIT pDown, pNext;
    WORD prio;
    if (lpcrit->bListed == 1) {
        prio = lpcrit->bListedPrio/PRIORITY_LEVELS_HASHSCALE;
        DEBUGCHK(prio < PRIORITY_LEVELS_HASHSIZE);
        pDown = lpcrit->pDownOwned;
        pNext = lpcrit->pNextOwned;
        if (pth->pOwnedHash[prio] == lpcrit) {
            pth->pOwnedHash[prio] = ((pDown != lpcrit) ? pDown :
                (pNext && (pNext->bListedPrio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
        }
        if (pDown == lpcrit) {
            if (!lpcrit->pPrevOwned) {
                DEBUGCHK(pth->pOwnedList == lpcrit);
                if (pth->pOwnedList = pNext)
                    pNext->pPrevOwned = 0;
            } else {
                if (lpcrit->pPrevOwned->pNextOwned = pNext)
                    pNext->pPrevOwned = lpcrit->pPrevOwned;
            }
        } else {
            pDown->pUpOwned = lpcrit->pUpOwned;
            lpcrit->pUpOwned->pDownOwned = pDown;
            if (lpcrit->pPrevOwned) {
                lpcrit->pPrevOwned->pNextOwned = pDown;
                pDown->pPrevOwned = lpcrit->pPrevOwned;
                goto FinishDequeue;
            } else if (lpcrit == pth->pOwnedList) {
                pth->pOwnedList = pDown;
                DEBUGCHK(!pDown->pPrevOwned);
    FinishDequeue:            
                if (pNext) {
                    pNext->pPrevOwned = pDown;
                    pDown->pNextOwned = pNext;
                }
            }
        }
        lpcrit->bListed = 0;
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PreUnlinkCritMut(
    LPCRIT lpcrit
    ) 
{
    KCALLPROFON(51);
    UnlinkCritMut(lpcrit,pCurThread);
    SET_NOPRIOCALC(pCurThread);
    KCALLPROFOFF(51);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PostUnlinkCritMut(void) 
{
    WORD prio, prio2;
    KCALLPROFON(52);
    CLEAR_NOPRIOCALC(pCurThread);
    prio = GET_BPRIO(pCurThread);
    if (pCurThread->pOwnedList && (prio > (prio2 = pCurThread->pOwnedList->bListedPrio)))
        prio = prio2;
    if (prio != GET_CPRIO(pCurThread)) {
        SET_CPRIO(pCurThread,prio);
        if (IsCeLogEnabled()) {
            CELOG_SystemInvert(pCurThread->hTh, prio);
        }
        if (RunList.pRunnable && (prio > GET_CPRIO(RunList.pRunnable)))
            SetReschedule();
    }
    KCALLPROFOFF(52);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ReprioCritMut(
    LPCRIT lpcrit,
    PTHREAD pth
    ) 
{
    BYTE prio;
    if (lpcrit->bListed == 1) {
        UnlinkCritMut(lpcrit,pth);
        prio = lpcrit->bListedPrio = (lpcrit->pProxList ? lpcrit->pProxList->prio : MAX_PRIORITY_LEVELS-1);
        DoLinkCritMut(lpcrit,pth,prio);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
MakeRun(
    PTHREAD pth
    ) 
{
    DWORD prio, prio2;
    PTHREAD pth2, pth3;
    if (!pth->bSuspendCnt) {
        SET_RUNSTATE(pth,RUNSTATE_RUNNABLE);
        prio = GET_CPRIO(pth);
        prio2 = prio/PRIORITY_LEVELS_HASHSCALE;
        if (!(pth2 = RunList.pRunnable) || (prio < GET_CPRIO(pth2))) {
            // either there is no runnable or we're the highest priority thread
            // make pth the head of runable and update the hash table
            pth->pPrevSleepRun = 0;
            if (pth->pNextSleepRun = pth2) {
                pth2->pPrevSleepRun = pth;
            }
            pth->pUpRun = pth->pDownRun = RunList.pHashThread[prio2] = pth;
            RunList.pRunnable = pth;

            // see if we need to reschedule
            if (!RunList.pth || (prio < GET_CPRIO(RunList.pth)))
                SetReschedule();

        } else {
            // there are higher priority thread than pth, find the right
            // place to enqueue it.

            if (!(pth2 = RunList.pHashThread[prio2])) {
                // find the preceeding non-zero hash table entry
                // bounded by PRIORITY_LEVELS_HASHSIZE
                RunList.pHashThread[prio2] = pth;
                while (!(pth2 = RunList.pHashThread[-- prio2]))
                    ;

            }
            
            if (prio < GET_CPRIO(pth2)) {
                // insert into runlist and replace the hash table entry
                DEBUGCHK (prio/PRIORITY_LEVELS_HASHSCALE == prio2);
                pth->pPrevSleepRun = pth2->pPrevSleepRun;
                pth->pNextSleepRun = pth2;
                pth->pUpRun = pth->pDownRun = RunList.pHashThread[prio2] = pth->pPrevSleepRun->pNextSleepRun = pth2->pPrevSleepRun = pth;

            } else {
                
                // find the appropriate place to insert pth
                // bounded by MAX_PRIORITY_HASHSCALE
                while ((pth3 = pth2->pNextSleepRun) && (prio >= GET_CPRIO(pth3)))
                    pth2 = pth3;
                DEBUGCHK (!pth3 || (prio < GET_CPRIO(pth3)));
                DEBUGCHK (GET_CPRIO (pth2) <= prio);
                if (prio == GET_CPRIO(pth2)) {
                    pth->pUpRun = pth2->pUpRun;
                    pth->pUpRun->pDownRun = pth2->pUpRun = pth;
                    pth->pDownRun = pth2;
                    pth->pPrevSleepRun = pth->pNextSleepRun = 0;
                } else {
                    // insert between pth2 and pth3
                    if (pth->pNextSleepRun = pth3) {
                        pth3->pPrevSleepRun = pth;
                    }
                    pth->pPrevSleepRun = pth2;
                    pth2->pNextSleepRun = pth->pUpRun = pth->pDownRun = pth;
                }
            }
        }
        
    } else {
        DEBUGCHK(!((pth->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
        SET_RUNSTATE(pth,RUNSTATE_BLOCKED);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
RunqDequeue(
    PTHREAD pth,
    DWORD cprio
    ) 
{
    PTHREAD pDown, pNext;
    DWORD prio = cprio/PRIORITY_LEVELS_HASHSCALE;

    DEBUGCHK (!GET_SLEEPING (pth));
    DEBUGCHK (!pth->pUpSleep);
    // check if there is a hanging tail of the thread...
    if (pDown = pth->pDownSleep) {
        DEBUGCHK (!GET_NEEDSLEEP(pDown) && GET_SLEEPING (pDown));
#ifdef DEBUG
        if (cprio > GET_CPRIO (pDown)) {
            DEBUGMSG (1, (L"pth->bprio = %d, pth->cprio = %d, pDown->bprio = %d, pDown->cprio = %d\r\n",
                pth->bBPrio, cprio, pDown->bBPrio, GET_CPRIO(pDown)));
            if (pth->pOwnedList) {
                DEBUGMSG (1, (L"pth->pOwnedList->bListedPrio = %dlx\r\n", pth->pOwnedList->bListedPrio));
            }
            if (pDown->pOwnedList) {
                DEBUGMSG (1, (L"pDown->pOwnedList->bListedPrio = %dlx\r\n", pDown->pOwnedList->bListedPrio));
            }
            DEBUGMSG (1, (L"pth->wInfo = %8.8lx, pDown->wInfo = %8.8lx\r\n", pth->wInfo, pDown->wInfo));
            DEBUGCHK (0);
        }
#endif
        DEBUGCHK (!pDown->bSuspendCnt);
        pDown->wCount ++;
        pDown->wCount2 ++;
        CLEAR_SLEEPING (pDown);
        pDown->pUpSleep = pth->pDownSleep = 0;
        // don't worry about it if NEEDRUN flag is set
        if (GET_RUNSTATE(pDown) != RUNSTATE_NEEDSRUN) {

            // setup proxy for cleanup if not pure sleeping 
            DEBUGCHK(GET_RUNSTATE(pDown) == RUNSTATE_BLOCKED);
            if (pDown->lpce) { // not set if purely sleeping
                pDown->lpce->base = pDown->lpProxy;
                pDown->lpce->size = (DWORD)pDown->lpPendProxy;
                pDown->lpProxy = 0;
            } else if (pDown->lpProxy) {
                // must be an interrupt event - special case it!
                LPPROXY pprox = pDown->lpProxy;
                LPEVENT lpe = (LPEVENT)pprox->pObject;
                DEBUGCHK(pprox->bType == HT_MANUALEVENT);
                DEBUGCHK(pprox->pQDown == pprox);
                DEBUGCHK(pprox->pQPrev == (LPPROXY)lpe);
                DEBUGCHK(pprox->dwRetVal == WAIT_OBJECT_0);
                lpe->pProxList = 0;
                pprox->pQDown = 0;
                pDown->lpProxy = 0;
            }

            // replace pth's slot if of same priority
            if (cprio == GET_CPRIO (pDown)) {
                if (pth == pth->pDownRun) {
                    // only node in this priority
                    pDown->pUpRun = pDown->pDownRun = pDown;
                } else {
                    // fixup the links
                    if (pDown->pUpRun = pth->pUpRun)
                        pDown->pUpRun->pDownRun = pDown;
                    if (pDown->pDownRun = pth->pDownRun)
                        pDown->pDownRun->pUpRun = pDown;
                }
                // fix up next node
                if (pDown->pNextSleepRun = pth->pNextSleepRun)
                    pDown->pNextSleepRun->pPrevSleepRun = pDown;

                // fix up prev node, update pRunnable if necessary
                if (pDown->pPrevSleepRun = pth->pPrevSleepRun) {
                    pDown->pPrevSleepRun->pNextSleepRun = pDown;
                } else if (RunList.pRunnable == pth) {
                    RunList.pRunnable = pDown;
                }
                // update hash table if necessary
                if (RunList.pHashThread[prio] == pth)
                    RunList.pHashThread[prio] = pDown;
                SET_RUNSTATE (pDown, RUNSTATE_RUNNABLE);
                return;
            }
            // not of the same priority, just call MakeRun
            // might want to save an instruction or two by
            // handling the logic here (don't have to check
            // suspend/pRunnable, etc.
            MakeRun (pDown);
        } 
    }

    // remove pth from the run queue
    pDown = pth->pDownRun;
    pNext = pth->pNextSleepRun;
    if (RunList.pHashThread[prio] == pth) {
        RunList.pHashThread[prio] = ((pDown != pth) ? pDown :
            (pNext && (GET_CPRIO(pNext)/PRIORITY_LEVELS_HASHSCALE == (WORD)prio)) ? pNext : 0);
    }
    if (pDown == pth) {
        if (!pth->pPrevSleepRun) {
            DEBUGCHK(RunList.pRunnable == pth);
            if (RunList.pRunnable = pNext)
                pNext->pPrevSleepRun = 0;
        } else {
            if (pth->pPrevSleepRun->pNextSleepRun = pNext)
                pNext->pPrevSleepRun = pth->pPrevSleepRun;
        }
    } else {
        pDown->pUpRun = pth->pUpRun;
        pth->pUpRun->pDownRun = pDown;
        if (pth->pPrevSleepRun) {
            pth->pPrevSleepRun->pNextSleepRun = pDown;
            pDown->pPrevSleepRun = pth->pPrevSleepRun;
            goto FinishDequeue;
        } else if (pth == RunList.pRunnable) {
            RunList.pRunnable = pDown;
            DEBUGCHK(!pDown->pPrevSleepRun);
FinishDequeue:            
            if (pNext) {
                pNext->pPrevSleepRun = pDown;
                pDown->pNextSleepRun = pNext;
            }
        }
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SleepqDequeue(
    PTHREAD pth
    ) 
{
    PTHREAD pth2;
    DEBUGCHK(pth && GET_SLEEPING(pth));
    pth->wCount2++;

    if (pth2 = pth->pUpSleep) {
        DEBUGCHK (pth != SleepList);
        DEBUGCHK (!pth->pNextSleepRun && !pth->pPrevSleepRun);
        if (pth2->pDownSleep = pth->pDownSleep) {
            pth2->pDownSleep->pUpSleep = pth2;
            pth->pDownSleep = 0;
        }
        pth->pUpSleep = 0;
    } else if (pth2 = pth->pDownSleep) {
        DEBUGCHK (!pth2->pNextSleepRun && !pth2->pPrevSleepRun);
        if (pth2->pNextSleepRun = pth->pNextSleepRun) {
            pth2->pNextSleepRun->pPrevSleepRun = pth2;
        }
        if (pth2->pPrevSleepRun = pth->pPrevSleepRun) {
            pth2->pPrevSleepRun->pNextSleepRun = pth2;
        } else {
            DEBUGCHK (pth == SleepList);
            SleepList = pth2;
        }
        pth2->pUpSleep = pth->pDownSleep = 0;

    } else if (pth2 = pth->pPrevSleepRun) {
        if (pth2->pNextSleepRun = pth->pNextSleepRun) {
            pth2->pNextSleepRun->pPrevSleepRun = pth2;
        }
    } else {
        DEBUGCHK (pth == SleepList);
        // update SleepList and dwReschedTime
        dwReschedTime = dwPrevReschedTime
            + ((RunList.pth && RunList.pth->dwQuantum)? RunList.pth->dwQuantLeft : 0x7fffffff);
        if (SleepList = pth->pNextSleepRun) {
            SleepList->pPrevSleepRun = 0;
            if ((int) (dwReschedTime - SleepList->dwWakeupTime) > 0) {
                dwReschedTime = SleepList->dwWakeupTime;
            }
        }
    }

    CLEAR_SLEEPING(pth);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
MakeRunIfNeeded(
    HANDLE hth
    ) 
{
    PTHREAD pth;
    KCALLPROFON(39);
    if ((pth = HandleToThread(hth)) && (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN)) {
        if (GET_SLEEPING(pth)) {
            SleepqDequeue(pth);
            pth->wCount ++;
        }
        MakeRun(pth);
    }
    KCALLPROFOFF(39);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
DequeueFlatProxy(
    LPPROXY pprox
    ) 
{
    LPPROXY pDown;
    LPEVENT pObject;
    KCALLPROFON(54);
    DEBUGCHK((pprox->bType == SH_CURPROC) || (pprox->bType == SH_CURTHREAD) || (pprox->bType == HT_MANUALEVENT));
    if (pDown = pprox->pQDown) { // not already dequeued
        if (pprox->pQPrev) { // we're first
            pObject = ((LPEVENT)pprox->pQPrev);
            DEBUGCHK(pObject->pProxList == pprox);
            if (pDown == pprox) { // we're alone
                pObject->pProxList = 0;
            } else {
                pDown->pQUp = pprox->pQUp;
                pprox->pQUp->pQDown = pObject->pProxList = pDown;
                pDown->pQPrev = (LPPROXY)pObject;
            }
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = pDown;
        }
        pprox->pQDown = 0;
    }
    KCALLPROFOFF(54);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
DequeuePrioProxy(
    LPPROXY pprox
    ) 
{
    LPCRIT lpcrit;
    LPPROXY pDown, pNext;
    WORD prio;
    BOOL bRet;
    KCALLPROFON(31);
    DEBUGCHK((pprox->bType == HT_EVENT) || (pprox->bType == HT_CRITSEC) || (pprox->bType == HT_MUTEX) || (pprox->bType == HT_SEMAPHORE));
    bRet = FALSE;
    if (pDown = pprox->pQDown) { // not already dequeued
        lpcrit = (LPCRIT)pprox->pObject;
        prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
        pDown = pprox->pQDown;
        pNext = pprox->pQNext;
        if (lpcrit->pProxHash[prio] == pprox) {
            lpcrit->pProxHash[prio] = ((pDown != pprox) ? pDown :
                (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
        }
        if (pDown == pprox) {
            if (!pprox->pQPrev) {
                DEBUGCHK(lpcrit->pProxList == pprox);
                if (lpcrit->pProxList = pNext)
                    pNext->pQPrev = 0;
                bRet = TRUE;
            } else {
                if (pprox->pQPrev->pQNext = pNext)
                    pNext->pQPrev = pprox->pQPrev;
            }
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = pDown;
            if (pprox->pQPrev) {
                pprox->pQPrev->pQNext = pDown;
                pDown->pQPrev = pprox->pQPrev;
                goto FinishDequeue;
            } else if (pprox == lpcrit->pProxList) {
                lpcrit->pProxList = pDown;
                DEBUGCHK(!pDown->pQPrev);
FinishDequeue:            
                if (pNext) {
                    pNext->pQPrev = pDown;
                    pDown->pQNext = pNext;
                }
            }
        }
        pprox->pQDown = 0;
    }
    KCALLPROFOFF(31);
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoReprioCrit(
    LPCRIT lpcrit
    ) 
{
    HANDLE hth;
    PTHREAD pth;
    KCALLPROFON(4);
    if ((hth = lpcrit->lpcs->OwnerThread) && (pth = HandleToThread((HANDLE)((DWORD)hth & ~1))))
        ReprioCritMut(lpcrit,pth);
    KCALLPROFOFF(4);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoReprioMutex(
    LPMUTEX lpm
    ) 
{
    KCALLPROFON(29);
    if (lpm->pOwner)
        ReprioCritMut((LPCRIT)lpm,lpm->pOwner);
    KCALLPROFOFF(29);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DequeueProxy(
    LPPROXY pProx
    ) 
{
    switch (pProx->bType) {
        case SH_CURPROC:
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



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
BoostCPrio(
    PTHREAD pth,
    DWORD prio
    ) 
{
    DWORD oldcprio;
    oldcprio = GET_CPRIO(pth);
    SET_CPRIO(pth,prio);
    if (GET_RUNSTATE(pth) == RUNSTATE_RUNNABLE) {
        RunqDequeue(pth,oldcprio);
        MakeRun(pth);
    } else if (GET_SLEEPING (pth) && pth->pUpSleep && (GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN)) {
        SleepqDequeue (pth);
        SET_NEEDSLEEP (pth);
        if (pth->lpce) {
            pth->bWaitState = WAITSTATE_PROCESSING; // re-process the wait
        }
        MakeRun (pth);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PostBoostMut(
    LPMUTEX lpm
    ) 
{
    PTHREAD pth;
    WORD prio;
    KCALLPROFON(56);
    if (pCurThread->bWaitState == WAITSTATE_PROCESSING) {
        pth = lpm->pOwner;
        DEBUGCHK(pth);
        prio = GET_CPRIO(pCurThread);
        if (prio < GET_CPRIO(pth)) {
            BoostCPrio(pth,prio);
            if (IsCeLogEnabled()) {
                CELOG_SystemInvert(pth->hTh, prio);
            }
        }
        if (!GET_NOPRIOCALC(pth))
            LaterLinkMutOwner(lpm);
        ReprioCritMut((LPCRIT)lpm,pth);
    }
    KCALLPROFOFF(56);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PostBoostCrit1(
    LPCRIT pcrit
    ) 
{
    PTHREAD pth;
    KCALLPROFON(57);
    if (pCurThread->bWaitState == WAITSTATE_PROCESSING) {
        pth = HandleToThread((HANDLE)((DWORD)pcrit->lpcs->OwnerThread & ~1));
        DEBUGCHK(pth);
        ReprioCritMut(pcrit,pth);
    }
    KCALLPROFOFF(57);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PostBoostCrit2(
    LPCRIT pcrit
    ) 
{
    PTHREAD pth;
    BYTE prio;
    KCALLPROFON(60);
    if (pCurThread->bWaitState == WAITSTATE_PROCESSING) {
        pth = HandleToThread((HANDLE)((DWORD)pcrit->lpcs->OwnerThread & ~1));
        DEBUGCHK(pth);
        if (pcrit->pProxList && ((prio = pcrit->pProxList->prio) < GET_CPRIO(pth))) {
            BoostCPrio(pth,prio);
            if (IsCeLogEnabled()) {
                CELOG_SystemInvert(pth->hTh, prio);
            }
        }
    }
    KCALLPROFOFF(60);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
CritFinalBoost(
    LPCRITICAL_SECTION lpcs
    ) 
{
    LPCRIT pcrit;
    DWORD prio;
    KCALLPROFON(59);
    DEBUGCHK(lpcs->OwnerThread == hCurThread);
    pcrit = (LPCRIT)lpcs->hCrit;
    if (!pcrit->bListed && pcrit->pProxList)
        LinkCritMut(pcrit,pCurThread,1);
    if (pcrit->pProxList && ((prio = pcrit->pProxList->prio) < GET_CPRIO(pCurThread))) {
        SET_CPRIO(pCurThread,prio);
        if (IsCeLogEnabled()) {
            CELOG_SystemInvert(pCurThread->hTh, prio);
        }
    }
    KCALLPROFOFF(59);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID 
AddToProcRunnable(
    PPROCESS pproc,
    PTHREAD pth
    ) 
{
    KCALLPROFON(24);
    pth->pNextInProc = pproc->pTh;
    pth->pPrevInProc = 0;
    DEBUGCHK(!pproc->pTh->pPrevInProc);
    pproc->pTh->pPrevInProc = pth;
    pproc->pTh = pth;
    MakeRun(pth);
    KCALLPROFOFF(24);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
WakeOneThreadInterruptDelayed(
    LPEVENT lpe
    ) 
{
    PTHREAD pth;
    LPPROXY pprox;
    HANDLE hRet;
    pprox = lpe->pProxList;
    DEBUGCHK(pprox->pObject == (LPBYTE)lpe);
    DEBUGCHK(pprox->bType == HT_MANUALEVENT);
    DEBUGCHK(pprox->pQDown == pprox);
    DEBUGCHK(pprox->pQPrev == (LPPROXY)lpe);
    DEBUGCHK(pprox->dwRetVal == WAIT_OBJECT_0);
    lpe->pProxList = 0;
    pth = pprox->pTh;
    DEBUGCHK(!pth->lpce);
    pth->wCount++;
    DEBUGCHK(pth->lpProxy == pprox);
    pth->lpProxy = 0;
    if (pth->bWaitState == WAITSTATE_BLOCKED) {
        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
        pth->retValue = WAIT_OBJECT_0;
        hRet = pth->hTh;
        SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
    } else {
        DEBUGCHK(!GET_SLEEPING(pth));
        DEBUGCHK(pth->bWaitState == WAITSTATE_PROCESSING);
        pth->dwPendReturn = WAIT_OBJECT_0;
        pth->bWaitState = WAITSTATE_SIGNALLED;
        hRet = 0;
    }
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
WakeOneThreadFlat(
    LPEVENT pObject,
    HANDLE *phTh
    ) 
{
    PTHREAD pth;
    LPPROXY pprox, pDown;
    KCALLPROFON(41);
    if (pprox = pObject->pProxList) {
        DEBUGCHK((pprox->bType == SH_CURPROC) || (pprox->bType == SH_CURTHREAD) || (pprox->bType == HT_MANUALEVENT));
        DEBUGCHK(pprox->pQPrev = (LPPROXY)pObject);
        pDown = pprox->pQDown;
        DEBUGCHK(pDown);
        if (pDown == pprox) { // we're alone
            pObject->pProxList = 0;
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = pObject->pProxList = pDown;
            pDown->pQPrev = (LPPROXY)pObject;
        }
        pprox->pQDown = 0;
        pth = pprox->pTh;
        DEBUGCHK(pth);
        if (pth->wCount == pprox->wCount) {
            DEBUGCHK(pth->lpce);
            pth->lpce->base = pth->lpProxy;
            pth->lpce->size = (DWORD)pth->lpPendProxy;
            pth->lpProxy = 0;
            pth->wCount++;
            if (pth->bWaitState == WAITSTATE_BLOCKED) {
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                pth->retValue = pprox->dwRetVal;
                SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                *phTh = pth->hTh;
            } else {
                DEBUGCHK(!GET_SLEEPING(pth));
                DEBUGCHK(pth->bWaitState == WAITSTATE_PROCESSING);
                pth->dwPendReturn = pprox->dwRetVal;
                pth->bWaitState = WAITSTATE_SIGNALLED;
            }
        }
    }
    KCALLPROFOFF(41);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
EventModMan(
    LPEVENT lpe,
    LPSTUBEVENT lpse,
    DWORD action
    ) 
{
    DWORD prio;
    KCALLPROFON(15);
    prio = lpe->bMaxPrio;
    lpe->bMaxPrio = THREAD_RT_PRIORITY_IDLE;
    if (lpse->pProxList = lpe->pProxList) {
        lpse->pProxList->pQPrev = (LPPROXY)lpse;
        DEBUGCHK(lpse->pProxList->pQDown);
        lpe->pProxList = 0;
    }
    lpe->state = (action == EVENT_SET);
    KCALLPROFOFF(15);
    return prio;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
EventModAuto(
    LPEVENT lpe,
    DWORD action,
    HANDLE *phTh
    ) 
{
    BOOL bRet;
    PTHREAD pth;
    LPPROXY pprox, pDown, pNext;
    BYTE prio;
    KCALLPROFON(16);
    bRet = 0;

    if (!(pprox = lpe->pProxList)) {
        lpe->state = (action == EVENT_SET);
    } else {
        pDown = pprox->pQDown;
        if (lpe->onequeue) {
            DEBUGCHK(pprox->pQPrev = (LPPROXY)lpe);
            if (pDown == pprox) { // we're alone
                lpe->pProxList = 0;
            } else {
                pDown->pQUp = pprox->pQUp;
                pprox->pQUp->pQDown = lpe->pProxList = pDown;
                pDown->pQPrev = (LPPROXY)lpe;
            }
        } else {
            prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
            pNext = pprox->pQNext;
            if (lpe->pProxHash[prio] == pprox) {
                lpe->pProxHash[prio] = ((pDown != pprox) ? pDown :
                    (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
            }
            if (pDown == pprox) {
                if (lpe->pProxList = pNext)
                    pNext->pQPrev = 0;
            } else {
                pDown->pQUp = pprox->pQUp;
                pprox->pQUp->pQDown = pDown;
                lpe->pProxList = pDown;
                DEBUGCHK(!pDown->pQPrev);
                if (pNext) {
                    pNext->pQPrev = pDown;
                    pDown->pQNext = pNext;
                }
            }
        }
        pprox->pQDown = 0;
        pth = pprox->pTh;
        DEBUGCHK(pth);
        if (pth->wCount != pprox->wCount)
            bRet = 1;
        else {
            DEBUGCHK(pth->lpce);
            pth->lpce->base = pth->lpProxy;
            pth->lpce->size = (DWORD)pth->lpPendProxy;
            pth->lpProxy = 0;
            pth->wCount++;
            if (pth->bWaitState == WAITSTATE_BLOCKED) {
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                pth->retValue = pprox->dwRetVal;
                SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                *phTh = pth->hTh;
            } else {
                DEBUGCHK(!GET_SLEEPING(pth));
                DEBUGCHK(pth->bWaitState == WAITSTATE_PROCESSING);
                pth->dwPendReturn = pprox->dwRetVal;
                pth->bWaitState = WAITSTATE_SIGNALLED;
            }
            lpe->state = 0;
        }
    }
    KCALLPROFOFF(16);
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoFreeMutex(
    LPMUTEX lpm
    ) 
{
    KCALLPROFON(34);
    UnlinkCritMut((LPCRIT)lpm,lpm->pOwner);
    KCALLPROFOFF(34);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoFreeCrit(
    LPCRIT lpcrit
    ) 
{
    PTHREAD pth;
    KCALLPROFON(45);
    if (lpcrit->bListed == 1) {
        pth = HandleToThread(lpcrit->lpcs->OwnerThread);
        DEBUGCHK(pth);
        UnlinkCritMut(lpcrit,pth);
    }
    KCALLPROFOFF(45);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_EventAddAccess(
    HANDLE hEvent
    ) 
{
    BOOL retval;
    TRUSTED_API (L"SC_EventAddAccess", FALSE);
    
    DEBUGMSG(ZONE_ENTRY,(L"SC_EventAddAccess entry: %8.8lx\r\n",hEvent));
    retval = IncRef(hEvent,pCurProc);
    DEBUGMSG(ZONE_ENTRY,(L"SC_EventAddAccess exit: %d\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
EventModIntr(
    LPEVENT lpe,
    DWORD type
    ) 
{
    HANDLE hRet;
    KCALLPROFON(42);
    if (!lpe->pProxList) {
        lpe->state = (type == EVENT_SET);
        hRet = 0;
    } else {
        lpe->state = 0;
        hRet = WakeOneThreadInterruptDelayed(lpe);
    }
    DEBUGCHK(!lpe->manualreset);
    KCALLPROFOFF(42);
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
AdjustPrioDown() 
{
    DWORD dwPrio, dwPrio2;
    KCALLPROFON(66);
    dwPrio = GET_BPRIO(pCurThread);
    if (pCurThread->pOwnedList && ((dwPrio2 = pCurThread->pOwnedList->bListedPrio) < dwPrio))
        dwPrio = dwPrio2;
    SET_CPRIO(pCurThread,dwPrio);
    if (RunList.pRunnable && (dwPrio > GET_CPRIO(RunList.pRunnable)))
        SetReschedule();
    KCALLPROFOFF(66);
}


BOOL SC_EventSetData (HANDLE hEvent, DWORD dwData)
{
    LPEVENT lpe;
    TRUSTED_API (L"SC_EventSetData", FALSE);
    
    DEBUGMSG(ZONE_ENTRY,(L"SC_EventSetData entry: %8.8lx %8.8lx\r\n",hEvent,dwData));
    if (!(lpe = HandleToEventPerm(hEvent))) {
        KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
        DEBUGMSG(ZONE_ENTRY,(L"SC_EventSetData exit: 0\r\n"));
        return FALSE;
    }

    lpe->dwData = dwData;
    DEBUGMSG(ZONE_ENTRY,(L"SC_EventSetData exit: 1\r\n"));
    return TRUE;
}

DWORD SC_EventGetData (HANDLE hEvent)
{
    LPEVENT lpe;
    TRUSTED_API (L"SC_EventGetData", FALSE);
    DEBUGMSG(ZONE_ENTRY,(L"SC_EventGetData entry: %8.8lx\r\n",hEvent));
    if (!(lpe = HandleToEventPerm(hEvent))) {
        KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
        DEBUGMSG(ZONE_ENTRY,(L"SC_EventGetData exit: -1\r\n"));
        return 0;
    }

    if (!lpe->dwData) {
        KSetLastError (pCurThread, 0);  // no error
    }

    DEBUGMSG(ZONE_ENTRY,(L"SC_EventGetData exit: %8.8lx\r\n", lpe->dwData));
    return lpe->dwData;

}

/* When a thread tries to set/reset/pulse an event */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_EventModify(
    HANDLE hEvent,
    DWORD type
    ) 
{
    LPEVENT lpe;
    HANDLE hWake;
    LPSTUBEVENT lpse;
    DEBUGMSG(ZONE_ENTRY,(L"SC_EventModify entry: %8.8lx %8.8lx\r\n",hEvent,type));
    if (!(lpe = HandleToEventPerm(hEvent))) {
        KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
        return FALSE;
    }
    
    if (IsCeLogEnabled()) {
        CELOG_Event(hEvent, type);
    }

    switch (type) {
    case EVENT_PULSE:
    case EVENT_SET:
        if (lpe->pIntrProxy) {
            DEBUGCHK(lpe->onequeue);
            if (hWake = (HANDLE)KCall((PKFN)EventModIntr,lpe,type))
                KCall((PKFN)MakeRunIfNeeded,hWake);
        } else if (lpe->manualreset) {
            DWORD dwOldPrio, dwNewPrio;
            DEBUGCHK(lpe->onequeue);
            // *lpse can't be stack-based since other threads won't have access and might dequeue/requeue
            if (!(lpse = AllocMem(HEAP_STUBEVENT))) {
                DEBUGMSG(ZONE_ENTRY,(L"SC_EventModify exit: %8.8lx\r\n",FALSE));
                KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
                return FALSE;
            }
            dwOldPrio = GET_BPRIO(pCurThread);
            dwNewPrio = KCall((PKFN)EventModMan,lpe,lpse,type);
            if (lpse->pProxList) {
                SET_NOPRIOCALC(pCurThread);
                if (dwNewPrio < dwOldPrio)
                    SET_CPRIO(pCurThread,dwNewPrio);
                while (lpse->pProxList) {
                    hWake = 0;
                    KCall((PKFN)WakeOneThreadFlat,lpse,&hWake);
                    if (hWake)
                        KCall((PKFN)MakeRunIfNeeded,hWake);
                }
                CLEAR_NOPRIOCALC(pCurThread);
                if (dwNewPrio < dwOldPrio)
                    KCall((PKFN)AdjustPrioDown);
            }
            FreeMem(lpse,HEAP_STUBEVENT);
        } else {
            hWake = 0;
            while (KCall((PKFN)EventModAuto,lpe,type,&hWake))
                ;
            if (hWake)
                KCall((PKFN)MakeRunIfNeeded,hWake);
        }
        break;
    case EVENT_RESET:
        lpe->state = 0;
        break;
    default:
        DEBUGCHK(0);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_EventModify exit: %8.8lx\r\n",TRUE));
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
IsValidIntrEvent(
    HANDLE hEvent
    ) 
{
    EVENT *lpe;
    if (!(lpe = HandleToEvent(hEvent)) || lpe->manualreset || lpe->pProxList)
        return FALSE;
    return TRUE;
}

typedef DWORD (* PFN_InitSyncObj) (HANDLE hObj, LPVOID pObj, DWORD dwParam1, DWORD dwParam2, LPName pName);
typedef void (* PFN_DeInitSyncObj) (HANDLE hObj, LPVOID pObj);
typedef LPName (* PFN_GetName) (LPVOID pObj);

//
// structure to define object specific methods of kernel sync-objects (mutex, event, and semaphore)
// NOTE: name comparision is object specific because it's not at the same offset
//       for individual objects. We should probabaly look into changing the structures
//       so they're at the same offset and then we don't need a compare-name method.
//
typedef struct _SYNCOBJ_METHOD {
    PFN_InitSyncObj     pfnInit;
    PFN_DeInitSyncObj   pfnDeInit;
    PFN_GetName         pfnGetName;
    const CINFO         *pci;
} SYNCOBJ_METHOD, *PSYNCOBJ_METHOD;

//------------------------------------------------------------------------------
// initialize a mutex
//------------------------------------------------------------------------------
static DWORD InitMutex (HANDLE hObj, LPVOID pObj, DWORD bInitialOwner, DWORD unused, LPName pName)
{
    LPMUTEX pMutex = (LPMUTEX) pObj;
    pMutex->name = pName;
    if (bInitialOwner) {
        pMutex->LockCount = 1;
        pMutex->pOwner = pCurThread;
        KCall ((PKFN)LaterLinkCritMut, pMutex, 0);
    }
    if (IsCeLogEnabled()) {
        CELOG_MutexCreate(hObj, pMutex);
    }
    return 0;   // no error
}

//------------------------------------------------------------------------------
// initialize an event
//------------------------------------------------------------------------------
static DWORD InitEvent (HANDLE hObj, LPVOID pObj, DWORD fManualReset, DWORD fInitState, LPName pName)
{
    LPEVENT pEvent = (LPEVENT) pObj;
    pEvent->name = pName;
    pEvent->state = (BYTE) fInitState;
    pEvent->manualreset = pEvent->onequeue = (BYTE) fManualReset;
    pEvent->bMaxPrio = THREAD_RT_PRIORITY_IDLE;
    if (IsCeLogEnabled()) {
        CELOG_EventCreate(hObj, fManualReset, fInitState, pName? pName->name : NULL, TRUE);
    }
    return 0;   // no error
}

//------------------------------------------------------------------------------
// initialize a semaphore
//------------------------------------------------------------------------------
static DWORD InitSemaphore (HANDLE hObj, LPVOID pObj, DWORD lInitCount, DWORD lMaxCount, LPName pName)
{
    LPSEMAPHORE pSem = (LPSEMAPHORE) pObj;
    if (((LONG) lInitCount < 0) || ((LONG) lMaxCount < 0) || (lInitCount > lMaxCount))
        return ERROR_INVALID_PARAMETER;
    pSem->name = pName;
    pSem->lMaxCount = (LONG) lMaxCount;
    pSem->lCount = (LONG) lInitCount;
    
    if (IsCeLogEnabled()) {
        CELOG_SemaphoreCreate(hObj, (LONG) lInitCount, (LONG) lMaxCount, pName? pName->name : NULL);
    }

    return 0;   // no error
}

//------------------------------------------------------------------------------
// delete a mutex
//------------------------------------------------------------------------------
static void DeInitMutex (HANDLE hObj, LPVOID pObj)
{
    LPMUTEX pMutex = (LPMUTEX) pObj;
    BOOL fOtherWaiting = FALSE;

    if (IsCeLogEnabled ()) {
        CELOG_MutexDelete (hObj);
    }

    while (pMutex->pProxList) {
        if (KCall ((PKFN)DequeuePrioProxy, pMutex->pProxList)) {
            KCall ((PKFN)DoReprioMutex, pMutex);
            fOtherWaiting = TRUE;
        }
    }

    KCall ((PKFN)DoFreeMutex, pMutex);

    if (pMutex->name)
        FreeName (pMutex->name);
    DEBUGCHK (!fOtherWaiting);
}

//------------------------------------------------------------------------------
// delete an event
//------------------------------------------------------------------------------
static void DeInitEvent (HANDLE hObj, LPVOID pObj)
{
    LPEVENT pEvent = (LPEVENT) pObj;

    if (IsCeLogEnabled ()) {
        CELOG_EventDelete (hObj);
    }

    if (pEvent->onequeue) {
        while (pEvent->pProxList)
            KCall ((PKFN)DequeueFlatProxy, pEvent->pProxList);
    } else {
        while (pEvent->pProxList)
            KCall ((PKFN)DequeuePrioProxy, pEvent->pProxList);
    }

    if (pEvent->name)
        FreeName (pEvent->name);
    if (pEvent->pIntrProxy)
        FreeIntrFromEvent (pEvent);
    
}

//------------------------------------------------------------------------------
// delete a semaphore
//------------------------------------------------------------------------------
static void DeInitSemaphore (HANDLE hObj, LPVOID pObj)
{
    LPSEMAPHORE pSem = (LPSEMAPHORE) pObj;
    
    if (IsCeLogEnabled ()) {
        CELOG_SemaphoreDelete (hObj);
    }

    while (pSem->pProxList)
        KCall ((PKFN)DequeuePrioProxy, pSem->pProxList);

    if (pSem->name)
        FreeName (pSem->name);
}

//------------------------------------------------------------------------------
// get name of a mutex
//------------------------------------------------------------------------------
static LPName MutexGetName (LPVOID pObj)
{
    return ((LPMUTEX) pObj)->name;
}

//------------------------------------------------------------------------------
// get name of a event
//------------------------------------------------------------------------------
static LPName EventGetName (LPVOID pObj)
{
    return ((LPEVENT) pObj)->name;
}

//------------------------------------------------------------------------------
// get name of a semaphore
//------------------------------------------------------------------------------
static LPName SemGetName (LPVOID pObj)
{
    return ((LPSEMAPHORE) pObj)->name;
}

//
// Event methods
//
static SYNCOBJ_METHOD eventMethod = { 
    InitEvent,
    DeInitEvent,
    EventGetName,
    &cinfEvent,
    };

//
// Mutex methods
//
static SYNCOBJ_METHOD mutexMethod = {
    InitMutex,
    DeInitMutex,
    MutexGetName,
    &cinfMutex,
    };

//
// Semaphore methods
//
static SYNCOBJ_METHOD semMethod   = {
    InitSemaphore,
    DeInitSemaphore,
    SemGetName,
    &cinfSem,
    };

//------------------------------------------------------------------------------
// common function to create/open a sync-object
//------------------------------------------------------------------------------
static HANDLE OpenOrCreateSyncObject (
    LPSECURITY_ATTRIBUTES   lpsa,
    DWORD                   dwParam1, 
    DWORD                   dwParam2, 
    LPCWSTR                 pszName,
    HANDLE                  *phHandleList, 
    PSYNCOBJ_METHOD         pObjMethod,
    BOOL                    fCreate)
{
    int     len = 0;
    DWORD   dwErr = 0;
    HANDLE  hObj = NULL;
    LPVOID  pObj;
    DWORD   dwSavedLastErr = KGetLastError (pCurThread);

    // error if open existing without a name
    if (!fCreate && !pszName) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (pszName) {
        LPName pObjName;
        // try to find it in the list if name is supplied
        len = strlenW(pszName) + 1;
        if ((MAX_PATH < len) || !LockPages(pszName, len *= sizeof(WCHAR), 0, LOCKFLAG_READ)) {
            KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
            return NULL;
        }
        EnterCriticalSection(&NameCS);
        for (hObj = *phHandleList; hObj; hObj = ((LPEVENT) pObj)->hNext) {
            pObj = GetObjectPtrByType (hObj, pObjMethod->pci->type);
            DEBUGCHK (pObj);
            pObjName = pObjMethod->pfnGetName (pObj);
            if (pObjName && !strcmpW (pObjName->name, pszName)) {
                // object already exist, increment ref-count and done
                IncRef (hObj, pCurProc);
                dwErr = ERROR_ALREADY_EXISTS;
                break;
            }
        }

        // error if open existing but not found
        if (!fCreate && !hObj) {
            dwErr = ERROR_FILE_NOT_FOUND;
        } else if (!dwErr) {
            dwSavedLastErr = 0;
        }
    }

    if (!dwErr) {
        // create a new object
        LPName pName = NULL;
        if ((pObj = AllocMem (HEAP_SYNC_OBJ))
            && (hObj = AllocHandle (pObjMethod->pci, pObj, pCurProc))
            && (!len || (pName = AllocName (len)))) {

            // clear memory
            memset (pObj, 0, HEAP_SIZE_SYNC_OBJ);

            // copy name if exist
            if (len) {
                memcpy (pName->name, pszName, len);
            }

            // call the object specific init function
            if (!(dwErr = pObjMethod->pfnInit (hObj, pObj, dwParam1, dwParam2, pName))) {
                
                // add the new object to the handle list
                AddHandleToList (phHandleList, hObj, pObjMethod->pci->type);
            }
            
        } else {
        
            // Out of memory
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
        }

        if (dwErr) {

            // error, free the memory we already allocated
            if (pName) {
                FreeName (pName);
            }
            
            if (hObj) {
                FreeHandle (hObj);
            }
            
            if (pObj) {
                FreeMem (pObj, HEAP_SYNC_OBJ);
            }

            hObj = NULL;
        }
    }

    if (pszName) {
        // release the NameCS and unlock the page we locked for name
        LeaveCriticalSection (&NameCS);
        UnlockPages (pszName, len);
    }

    // set/restore last error (might be changed by UnlockPages
    KSetLastError (pCurThread, dwErr? dwErr : dwSavedLastErr);

    return hObj;
    
}

//------------------------------------------------------------------------------
// common function to close a sync-object handle
//------------------------------------------------------------------------------
static BOOL CloseSyncObjectHandle (HANDLE hObj, HANDLE *phHandleList, PSYNCOBJ_METHOD pSyncMethod)
{
    LPVOID pObj = GetObjectPtrByType (hObj, pSyncMethod->pci->type);

    // need a local variable to keep the named information because the object will be deleted.
    BOOL   fNamed = (NULL != pSyncMethod->pfnGetName (pObj));
    
    DEBUGCHK (pObj);

    // we need to grab the NameCS if the object is named. Otherwise, we might
    // run into race condition where another thread is in the middle of Creating
    // the object.
    if (fNamed) {
        EnterCriticalSection (&NameCS);
    }

    if (DecRef (hObj, pCurProc, FALSE)) {
        
        RemoveHandleFromList (phHandleList, hObj, pSyncMethod->pci->type);

        pSyncMethod->pfnDeInit (hObj, pObj);
        
        FreeMem (pObj, HEAP_SYNC_OBJ);
        FreeHandle (hObj);
    }
    
    if (fNamed) {
        LeaveCriticalSection (&NameCS);
    }
    return TRUE;
}

//------------------------------------------------------------------------------
// create an event
//------------------------------------------------------------------------------
HANDLE 
SC_CreateEvent(
    LPSECURITY_ATTRIBUTES lpsa,
    BOOL fManReset,
    BOOL fInitState,
    LPCWSTR lpEventName
    ) 
{
    HANDLE hEvent;

    DEBUGMSG(ZONE_ENTRY, (L"SC_CreateEvent entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                          lpsa, fManReset, fInitState, lpEventName));
    
    hEvent = OpenOrCreateSyncObject (lpsa, fManReset, fInitState, lpEventName, &hEventList, &eventMethod, TRUE);
    
    
    DEBUGMSG(ZONE_ENTRY, (L"SC_CreateEvent exit: %8.8lx\r\n", hEvent));

    return hEvent;
}


//------------------------------------------------------------------------------
// open an existing named event
//------------------------------------------------------------------------------
HANDLE 
SC_OpenEvent(
    DWORD   dwDesiredAccess,
    BOOL    fInheritHandle,
    LPCWSTR lpEventName
    ) 
{
    HANDLE hEvent = NULL;

    DEBUGMSG(ZONE_ENTRY, (L"SC_OpenEvent entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                          dwDesiredAccess, fInheritHandle, lpEventName));
    
    // Validate args
    if ((EVENT_ALL_ACCESS != dwDesiredAccess) || fInheritHandle) {

        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);

    } else {
    
        hEvent = OpenOrCreateSyncObject (NULL, FALSE, FALSE, lpEventName, &hEventList, &eventMethod, FALSE);

        // open event does not go though EventInit, so we need to do Celog here
        if (hEvent && IsCeLogEnabled()) {
            CELOG_EventCreate(NULL, FALSE, FALSE, lpEventName, FALSE);
        }
    }
    
    DEBUGMSG(ZONE_ENTRY, (L"SC_OpenEvent exit: %8.8lx\r\n", hEvent));

    return hEvent;
}

//------------------------------------------------------------------------------
// create a semaphore
//------------------------------------------------------------------------------
HANDLE 
SC_CreateSemaphore(
    LPSECURITY_ATTRIBUTES lpsa,
    LONG lInitialCount,
    LONG lMaximumCount,
    LPCWSTR lpName
    ) 
{
    HANDLE hSem;

    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateSemaphore entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",lpsa,lInitialCount,lMaximumCount,lpName));

    hSem = OpenOrCreateSyncObject (lpsa, (DWORD) lInitialCount, (DWORD) lMaximumCount, lpName, &hSemList, &semMethod, TRUE);

    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateSemaphore exit: %8.8lx\r\n",hSem));
    return hSem;
}

//------------------------------------------------------------------------------
//  create a mutex
//------------------------------------------------------------------------------
HANDLE 
SC_CreateMutex(
    LPSECURITY_ATTRIBUTES lpsa,
    BOOL bInitialOwner,
    LPCTSTR lpName
    ) 
{
    HANDLE hMutex;
    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateMutex entry: %8.8lx %8.8lx %8.8lx\r\n",lpsa,bInitialOwner,lpName));


    hMutex = OpenOrCreateSyncObject (lpsa, bInitialOwner, 0, lpName, &hMutexList, &mutexMethod, TRUE);

    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateMutex exit: %8.8lx\r\n",hMutex));
    return hMutex;
}


//------------------------------------------------------------------------------
// closehandle an event
//------------------------------------------------------------------------------
BOOL 
SC_EventCloseHandle(
    HANDLE hEvent
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_EventCloseHandle entry: %8.8lx\r\n",hEvent));
    if (IsCeLogEnabled ()) {
        CELOG_EventCloseHandle (hEvent);
    }

    CloseSyncObjectHandle (hEvent, &hEventList, &eventMethod);

    DEBUGMSG(ZONE_ENTRY,(L"SC_EventCloseHandle exit: %8.8lx\r\n",TRUE));
    return TRUE;
}

//------------------------------------------------------------------------------
// closehandle a semaphore
//------------------------------------------------------------------------------
BOOL 
SC_SemCloseHandle(
    HANDLE hSem
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_SemCloseHandle entry: %8.8lx\r\n",hSem));
    if (IsCeLogEnabled ()) {
        CELOG_SemaphoreCloseHandle (hSem);
    }

    CloseSyncObjectHandle (hSem, &hSemList, &semMethod);

    DEBUGMSG(ZONE_ENTRY,(L"SC_SemCloseHandle exit: %8.8lx\r\n",TRUE));
    return TRUE;
}

//------------------------------------------------------------------------------
// closehandle a mutex
//------------------------------------------------------------------------------
BOOL 
SC_MutexCloseHandle(
    HANDLE hMutex
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_MutexCloseHandle entry: %8.8lx\r\n",hMutex));
    if (IsCeLogEnabled ()) {
        CELOG_MutexCloseHandle (hMutex);
    }

    CloseSyncObjectHandle (hMutex, &hMutexList, &mutexMethod);

    DEBUGMSG(ZONE_ENTRY,(L"SC_MutexCloseHandle exit: %8.8lx\r\n",TRUE));
    return TRUE;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_SetLowestScheduledPriority(
    DWORD maxprio
    ) 
{
    DWORD retval;
    TRUSTED_API (L"SC_SetLowestScheduledPriority", currmaxprio);
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetLowestScheduledPriority entry: %8.8lx\r\n",maxprio));
    if (maxprio > MAX_WIN32_PRIORITY_LEVELS - 1)
        maxprio = MAX_WIN32_PRIORITY_LEVELS - 1;
    retval = currmaxprio;
    if (retval < MAX_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS)
        retval = MAX_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS;
    retval -= (MAX_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS);        
    currmaxprio = maxprio + (MAX_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS);
    pOOMThread = pCurThread;
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetLowestScheduledPriority exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
NextThread(void) 
{
    PTHREAD pth;
    LPEVENT lpe;
    LPPROXY pprox;
    DWORD pend, intr;
    KCALLPROFON(10);
    /* Handle interrupts */
    do {
        INTERRUPTS_OFF();
        pend = PendEvents;
        PendEvents = 0;
        INTERRUPTS_ON();
        while (pend) {
            intr = GetHighPos(pend);
            pend &= ~(1<<intr);
            if (lpe = IntrEvents[intr]) {
                if (!lpe->pProxList)
                    lpe->state = 1;
                else {
                    DEBUGCHK(!lpe->state);
                    pprox = lpe->pProxList;
                    DEBUGCHK(pprox->pObject == (LPBYTE)lpe);
                    DEBUGCHK(pprox->bType == HT_MANUALEVENT);
                    DEBUGCHK(pprox->pQDown == pprox);
                    DEBUGCHK(pprox->pQPrev == (LPPROXY)lpe);
                    DEBUGCHK(pprox->dwRetVal == WAIT_OBJECT_0);
                    lpe->pProxList = 0;
                    pth = pprox->pTh;
                    DEBUGCHK(!pth->lpce);
                    pth->wCount++;
                    DEBUGCHK(pth->lpProxy == pprox);
                    DEBUGCHK(pth->lpProxy == lpe->pIntrProxy);
                    pth->lpProxy = 0;
                    if (pth->bWaitState == WAITSTATE_BLOCKED) {
                        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                        pth->retValue = WAIT_OBJECT_0;
                        if (GET_SLEEPING(pth)) {
                            SleepqDequeue(pth);
                        }
                        MakeRun(pth);
                    } else {
                        DEBUGCHK(!GET_SLEEPING(pth));
                        DEBUGCHK(pth->bWaitState == WAITSTATE_PROCESSING);
                        pth->dwPendReturn = WAIT_OBJECT_0;
                        pth->bWaitState = WAITSTATE_SIGNALLED;
                    }
                }
            }
        }
    } while (PendEvents);

///////////////////////////////////////////////////////////
// We can not return here or the real-time SleepQueue
// will not work as expected. i.e. if timer tick and 
// other interrupts come at the same time, we'll lose 
// the timer tick and the thread at the beginning of
// the SleepList will not be waked up until the next
// tick.

    /* Handle timeouts */
    if ((int) (CurMSec - dwReschedTime) >= 0) {
        SetReschedule ();
    }
    if ((pth = SleepList) && ((int) (CurMSec - pth->dwWakeupTime) >= 0)) {

        DEBUGCHK((int) (CurMSec - dwReschedTime) >= 0);
        DEBUGCHK(GET_SLEEPING(pth));
        // We only update CurMSec on timer interrupt or in/out of Idle.
        // Sinc the timer ISR should return SYSINTR_RESCHED in this case, and we 
        // always reschedule after Idle, there is no need to update dwReschedTime
        // It'll be updated in KCNextThread
        CLEAR_SLEEPING(pth);
        pth->wCount++;
        pth->wCount2 ++;

        if (SleepList = pth->pNextSleepRun) {
            SleepList->pPrevSleepRun = 0;
        }
        if (GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN) {
            DEBUGCHK(GET_RUNSTATE(pth) == RUNSTATE_BLOCKED);
            if (pth->lpce) { // not set if purely sleeping
                pth->lpce->base = pth->lpProxy;
                pth->lpce->size = (DWORD)pth->lpPendProxy;
                pth->lpProxy = 0;
            } else if (pth->lpProxy) {
                // must be an interrupt event - special case it!
                pprox = pth->lpProxy;
                lpe = (LPEVENT)pprox->pObject;
                DEBUGCHK(pprox->bType == HT_MANUALEVENT);
                DEBUGCHK(pprox->pQDown == pprox);
                DEBUGCHK(pprox->pQPrev == (LPPROXY)lpe);
                DEBUGCHK(pprox->dwRetVal == WAIT_OBJECT_0);
                lpe->pProxList = 0;
                pprox->pQDown = 0;
                pth->lpProxy = 0;
            }
            MakeRun(pth);
        } 
    }

    KCALLPROFOFF(10);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
GoToUserTime() 
{
    INTERRUPTS_OFF();
    SET_TIMEUSER(pCurThread);
    pCurThread->dwKernTime += CurMSec - CurrTime;
    CurrTime = CurMSec;
    INTERRUPTS_ON();
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (CurMSec & 0x1f);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
GoToKernTime() 
{
    INTERRUPTS_OFF();
    SET_TIMEKERN(pCurThread);
    pCurThread->dwUserTime += CurMSec - CurrTime;
    CurrTime = CurMSec;
    INTERRUPTS_ON();
    randdw2 = ((randdw2<<5) | (randdw2>>27)) ^ (CurMSec & 0x1f);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
KCNextThread(void) 
{
    DWORD prio, prio2, NewTime, dwTmp;
    BOOL bQuantExpired;
    PTHREAD pth, pth2, pth3, pOwner;
    LPCRIT pCrit;
    LPCRITICAL_SECTION lpcs;
    KCALLPROFON(44);

    NewTime = CurMSec;

    if (pth = RunList.pth) {
        // calculate quantum left
        if (bQuantExpired = (0 != pth->dwQuantum)) {
            if ((int) (pth->dwQuantLeft -= NewTime - dwPrevReschedTime) <= 0) {
                pth->dwQuantLeft = pth->dwQuantum;
            } else {
                bQuantExpired = FALSE;
            }
        }

        // if there is any other runable thread, check to see if we need to context switch
        if (pth2 = RunList.pRunnable) {
            if (((prio = GET_CPRIO (pth)) == (prio2 = GET_CPRIO (pth2))) && bQuantExpired) {
                // Current thread quantum expired and next runnable thread have same priority, same run list.  
                // Use runnable thread pointer (pth2, first on list) to place current thread (pth) 
                // last on list.
                pth->pUpRun = pth2->pUpRun;
                pth->pUpRun->pDownRun = pth2->pUpRun = pth;
                pth->pDownRun = pth2;
                pth->pPrevSleepRun = pth->pNextSleepRun = 0;

                SET_RUNSTATE(pth,RUNSTATE_RUNNABLE);
                RunList.pth = 0;

            } else if (prio > prio2) {
                // there is a runable thread of higher priority, enqueue the current thread
                prio2 = prio/PRIORITY_LEVELS_HASHSCALE;
                if (!(pth2 = RunList.pHashThread[prio2])) {
                    // find the preceeding non-zero hash table entry
                    // bounded by PRIORITY_LEVELS_HASHSIZE
                    RunList.pHashThread[prio2] = pth;
                    while (!(pth2 = RunList.pHashThread[-- prio2]))
                        ;
                }
                
                if (prio < GET_CPRIO(pth2)) {
                    // insert into runlist and replace the hash table entry
                    DEBUGCHK (prio/PRIORITY_LEVELS_HASHSCALE == prio2);
                    pth->pPrevSleepRun = pth2->pPrevSleepRun;
                    pth->pNextSleepRun = pth2;
                    pth->pUpRun = pth->pDownRun = RunList.pHashThread[prio2] = pth->pPrevSleepRun->pNextSleepRun = pth2->pPrevSleepRun = pth;

                } else {
                    
                    // find the appropriate place to insert current thread
                    // bounded by MAX_PRIORITY_HASHSCALE
                    while ((pth3 = pth2->pNextSleepRun) && (prio >= GET_CPRIO(pth3)))
                        pth2 = pth3;
                    DEBUGCHK (!pth3 || (prio < GET_CPRIO(pth3)));
                    DEBUGCHK (GET_CPRIO (pth2) <= prio);
                    DEBUGCHK (pth);

                    if (prio == GET_CPRIO(pth2)) {
                        // Run list for current thread priority exists, make current thread first if time
                        // left in quantum or if quantum is 0 (run to completion).
                        if (bQuantExpired) {
                            pth->pPrevSleepRun = pth->pNextSleepRun = 0;
                        } else {
                            // become the head of the list
                            if (pth->pNextSleepRun = pth3)
                                pth3->pPrevSleepRun = pth;
                            if (pth->pPrevSleepRun = pth2->pPrevSleepRun)
                                pth2->pPrevSleepRun->pNextSleepRun = pth;
                            pth2->pNextSleepRun = pth2->pPrevSleepRun = 0;
                            // Update hash table entry if replacing first thread on list.
                            if (pth2 == RunList.pHashThread[prio2]) {
                                DEBUGCHK (prio/PRIORITY_LEVELS_HASHSCALE == prio2);
                                RunList.pHashThread[prio2] = pth;
                            }
                        }

                        // make pth "before" pth2
                        pth->pUpRun = pth2->pUpRun;
                        pth->pUpRun->pDownRun = pth2->pUpRun = pth;
                        pth->pDownRun = pth2;

                    } else {
                        // insert between pth2 and pth3
                        if (pth->pNextSleepRun = pth3) {
                            pth3->pPrevSleepRun = pth;
                        }
                        pth->pPrevSleepRun = pth2;
                        pth2->pNextSleepRun = pth->pUpRun = pth->pDownRun = pth;
                    }
                }
                SET_RUNSTATE(pth,RUNSTATE_RUNNABLE);
                RunList.pth = 0;
            }
        }
    } else if (!bLastIdle) {
        // pCurThread yeild, receive full quantum again
        pCurThread->dwQuantLeft = pCurThread->dwQuantum;
    }

    if (!RunList.pth && (pth = RunList.pRunnable)) {
        // context switch occurs -- check "max runable priority" (set by OOM)
        pOwner = 0;
        if ((prio = GET_CPRIO(pth)) > currmaxprio) {
            pOwner = pOOMThread;
            pth = 0;
            while (pOwner && !pth && !PendEvents) {
                switch (GET_RUNSTATE(pOwner)) {
                case RUNSTATE_NEEDSRUN:
                    if (GET_SLEEPING(pOwner)) {
                        SleepqDequeue(pOwner);
                        pOwner->wCount ++;
                    }
                    MakeRun(pOwner);
                    DEBUGCHK (GET_RUNSTATE (pOwner) == RUNSTATE_RUNNABLE);
                    // fall thru
                case RUNSTATE_RUNNABLE:
                    pth = pOwner;
                    prio = GET_CPRIO(pOwner);
                    break;

                case RUNSTATE_BLOCKED:
                    if (!GET_SLEEPING(pOwner) && pOwner->lpProxy && pOwner->lpProxy->pQDown
                        && !pOwner->lpProxy->pThLinkNext && (pOwner->lpProxy->bType == HT_CRITSEC)) {
                        pCrit = (LPCRIT)pOwner->lpProxy->pObject;
                        DEBUGCHK(pCrit);
                        SWITCHKEY(dwTmp, 0xffffffff);
                        lpcs = pCrit->lpcs;
                        DEBUGCHK(lpcs);
                        DEBUGCHK(lpcs->OwnerThread);
                        pOwner = HandleToThread((HANDLE)((DWORD)lpcs->OwnerThread & ~1));
                        SETCURKEY(dwTmp);
                        DEBUGCHK(pOwner);
                        break;
                    }
                    pOwner = 0;
                    break;

                default:
                    DEBUGCHK (0);    // the thread can't be running!! something wrong
                }
            }
        }

        // check if there is a thread to run
        if (RunList.pth = pth) {
            SET_RUNSTATE(pth,RUNSTATE_RUNNING);
            RunqDequeue(pth,prio);
            pth->hLastCrit = 0;
            pth->lpCritProxy = 0;

            if (!GET_NOPRIOCALC(pth)) {
                // recalculate priority based on sync object pth owns
                prio = GET_BPRIO(pth);
                if (pth->pOwnedList && ((prio2 = pth->pOwnedList->bListedPrio) < prio))
                    prio = prio2;

                // make sure we don't reschedule forever in OOM condition
                if (pOwner && (prio > currmaxprio)) {
                    prio = currmaxprio;
                }

                if (prio > GET_CPRIO(pth)) {
                    SET_CPRIO(pth,prio);
                    if (IsCeLogEnabled()) {
                        CELOG_SystemInvert(pth->hTh, prio);
                    }
                    if (RunList.pRunnable && (prio > GET_CPRIO(RunList.pRunnable)))
                        SetReschedule();
                }
            }
        }
    }

    randdw2 = ((randdw2<<5) | (randdw2>>27)) ^ ((NewTime>>5) & 0x1f);
    
    if (!bLastIdle) {
        if (GET_TIMEMODE(pCurThread) == TIMEMODE_USER)
            pCurThread->dwUserTime += NewTime - dwPrevReschedTime;
        else
            pCurThread->dwKernTime += NewTime - dwPrevReschedTime;

        // save the debug register for the thread being swap out.
        // NOTE: we use the pSwapStack field to save the debug registers
        //       since a thread will not use the swap stack unless it's dying,
        //       we don't have worry about the contents being destroyed.
        //       The cleanup thread doesn't have a swap stack but it makes
        //       perfect sense that OEM didn't see them.
        if (fNKSaveCoProcReg && cbNKCoProcRegSize && pCurThread->pSwapStack) {
            DEBUGCHK (pOEMSaveCoProcRegister);
            pOEMSaveCoProcRegister (pCurThread->pSwapStack);
        }
    }
    CurrTime = dwPrevReschedTime = NewTime;
    bLastIdle = !(pth = RunList.pth);
    dwReschedTime = SleepList? SleepList->dwWakeupTime : (NewTime + 0x7fffffff);

    if (!bLastIdle) {
        // update dwReschedTime
        if (pth->dwQuantum && ((int) ((NewTime += pth->dwQuantLeft) - dwReschedTime) < 0)) {
            dwReschedTime = NewTime;
        }

        // restore debug register using pSwapStack (see note above)
        if (fNKSaveCoProcReg && cbNKCoProcRegSize && pth->pSwapStack) {
            DEBUGCHK (pOEMRestoreCoProcRegister);
            pOEMRestoreCoProcRegister (pth->pSwapStack);
        }

        if (IsCeLogKernelEnabled()) {
            CELOG_ThreadSwitch(pth);
        }
        if (pLogThreadSwitch)
            pLogThreadSwitch((DWORD)pth,(DWORD)pth->pOwnerProc);
    } else {

        if (IsCeLogKernelEnabled()) {
            CELOG_ThreadSwitch(0);
        }
        if (pLogThreadSwitch)
            pLogThreadSwitch(0,0);
    }
//#ifndef SHIP_BUILD
    if (pCurThread != pth)
        KInfoTable[KINX_DEBUGGER]++;
//#endif
#ifdef SH4
    if (pth) {
        if (g_CurFPUOwner != pth)
            pth->ctx.Psr |= 0x8000;
        else
            pth->ctx.Psr &= ~0x8000;
    }
#endif
#ifdef SH3
    if (pth) {
        if (g_CurDSPOwner != pth)
            pth->ctx.Psr &= ~0x1000;
        else
            pth->ctx.Psr |= 0x1000;
    }
#endif
    KCALLPROFOFF(44);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPTHRDDBG 
AllocDbg(
    HANDLE hProc
    ) 
{
    LPTHRDDBG pdbg;
    if (!(pdbg = (LPTHRDDBG)AllocMem(HEAP_THREADDBG)))
        return 0;
    if (!(pdbg->hEvent = CreateEvent(0,0,0,0))) {
        FreeMem(pdbg,HEAP_THREADDBG);
        return 0;
    }
    if (!(pdbg->hBlockEvent = CreateEvent(0,0,0,0))) {
        CloseHandle(pdbg->hEvent);
        FreeMem(pdbg,HEAP_THREADDBG);
        return 0;
    }
    if (hProc) {
        SC_SetHandleOwner(pdbg->hEvent,hProc);
        SC_SetHandleOwner(pdbg->hBlockEvent,hProc);
    }
    pdbg->hFirstThrd = 0;
    pdbg->psavedctx = 0;
    pdbg->dbginfo.dwDebugEventCode = 0;
    pdbg->bDispatched = 0;
    return pdbg;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void InitThreadStruct(
    PTHREAD pTh,
    HANDLE hTh,
    PPROCESS pProc,
    HANDLE hProc,
    WORD prio
    ) 
{
    SetUserInfo(hTh,STILL_ACTIVE);
    pTh->pProc = pProc;
    pTh->pOwnerProc = pProc;
    pTh->pProxList = 0;
    pTh->pThrdDbg = 0;
    pTh->aky = pProc->aky;
    AddAccess(&pTh->aky,ProcArray[0].aky);
    pTh->hTh = hTh;
    pTh->pcstkTop = 0;
    pTh->bSuspendCnt = pTh->bPendSusp = 0;
    pTh->wInfo = 0;
    pTh->bWaitState = WAITSTATE_SIGNALLED;
    pTh->lpCritProxy = 0;
    pTh->lpProxy = 0;
    pTh->lpce = 0;
    pTh->dwLastError = 0;
    pTh->dwUserTime = 0;
    pTh->dwKernTime = 0;
    pTh->dwQuantLeft = pTh->dwQuantum = dwDefaultThreadQuantum;
    SET_BPRIO(pTh,prio);
    SET_CPRIO(pTh,prio);
    pTh->pOwnedList = 0;
    memset(pTh->pOwnedHash,0,sizeof(pTh->pOwnedHash));
    pTh->pSwapStack = 0;
    pTh->pUpSleep = pTh->pDownSleep = 0;
    pTh->tlsSecure = pTh->tlsNonSecure = 0;
    pTh->bDbgCnt = 0;
}


//------------------------------------------------------------------------------
// ThreadSuspend: suspend a thread. If fLateSuspend is true, we'll mark the thread
//                structure and the thread would suspend itself when exiting PSL.
//------------------------------------------------------------------------------
DWORD ThreadSuspend (PTHREAD pth, BOOL fLateSuspend) 
{
    DWORD retval = 0xfffffffd;
    ACCESSKEY ulOldKey;
    KCALLPROFON(5);
    SWITCHKEY(ulOldKey,0xffffffff);

    // validate parameter (suspending kernel threads or a thread holding LLcs is not allowed)
    if (!pth || (pth->pOwnerProc == &ProcArray[0]) || (LLcs.OwnerThread == pth->hTh)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        retval = 0xffffffff;

    // okay to suspend current running thread as long as it's not in the middle of dying
    } else if (pth == pCurThread) {
    
        if (GET_DYING(pCurThread) && !GET_DEAD(pCurThread)) {
            KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
            retval = 0xfffffffe;
        }
        
    } else {

        // Only kernel would ever call ThreadSuspend with fLateSuspend == False. Currently
        // the only case is for stack scavanging, where we know it'll resume the thread quickly.
        // We'll suspend the thread as long as it's not inside kernel or it's blocked.
        if (!fLateSuspend) {

            if ((RUNSTATE_BLOCKED != GET_RUNSTATE(pth)) && (GetThreadIP (pth) & 0x80000000)) {
                KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
                retval = 0xfffffffe;
            }

        // fLateSuspend == TRUE, check where we are, late suspend if in PSL
        } else if ((GetThreadIP (pth) & 0x80000000) || (pth->pOwnerProc != pth->pProc)) {
        
            if (pth->bPendSusp != MAX_SUSPEND_COUNT) {
                retval = pth->bPendSusp;
                pth->bPendSusp ++;
            } else {
                KSetLastError (pCurThread,ERROR_SIGNAL_REFUSED);
                retval = 0xffffffff;
            }
        }
    }

    // okay to suspend on the spot?
    if (0xfffffffd == retval) {

        retval = pth->bSuspendCnt;
        if (pth->bSuspendCnt) {
            if (pth->bSuspendCnt != MAX_SUSPEND_COUNT)
                pth->bSuspendCnt++;
            else {
                KSetLastError(pCurThread,ERROR_SIGNAL_REFUSED);
                retval = 0xffffffff;
            }
        } else {
            pth->bSuspendCnt = 1;
            switch (GET_RUNSTATE(pth)) {
                case RUNSTATE_RUNNING:
                    DEBUGCHK(RunList.pth == pth);
                    RunList.pth = 0;
                    SetReschedule();
                    break;
                case RUNSTATE_RUNNABLE:
                    RunqDequeue(pth, GET_CPRIO(pth));
                    break;
//                case RUNSTATE_NEEDSRUN:
                default:
                    if (GET_SLEEPING(pth)) {
                        SleepqDequeue(pth);
                        if (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN) {
                            pth->wCount ++;
                        } else {
                            //DEBUGCHK (0);   // we should always be late suspended if we're sleeping
                            SET_NEEDSLEEP (pth);
                            if (pth->lpce) {
                                pth->bWaitState = WAITSTATE_PROCESSING; // re-process the wait
                            }
                        }
                    }
                    break;
            }
//            DEBUGCHK(!((pth->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
            SET_RUNSTATE(pth,RUNSTATE_BLOCKED);
        }
    }
    SETCURKEY(ulOldKey);
    KCALLPROFOFF(5);
    return retval;
}

/* When a thread gets SuspendThread() */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_ThreadSuspend(
    HANDLE hTh
    ) 
{
    DWORD retval;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadSuspend entry: %8.8lx\r\n",hTh));
    if (hTh == GetCurrentThread())
        hTh = hCurThread;
    if (IsCeLogEnabled()) {
        CELOG_ThreadSuspend(hTh);
    }
    retval = KCall((PKFN)ThreadSuspend,HandleToThread(hTh), TRUE);
    if (retval == 0xfffffffe)
        retval = 0xffffffff;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadSuspend exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
UB_ThreadSuspend(
    HANDLE hTh
    ) 
{
    DWORD retval;
    if ((hTh == (HANDLE)GetCurrentThread()) || (hTh == hCurThread))
        SET_USERBLOCK(pCurThread);
    retval = SC_ThreadSuspend(hTh);
    if ((hTh == (HANDLE)GetCurrentThread()) || (hTh == hCurThread))
        CLEAR_USERBLOCK(pCurThread);
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
ThreadResume(
    PTHREAD pth
    ) 
{
    DWORD retval = 0;
    KCALLPROFON(47);
    if (pth) {
        if (retval = pth->bSuspendCnt) {
            if (!--pth->bSuspendCnt && (GET_RUNSTATE(pth) == RUNSTATE_BLOCKED) && (!pth->lpProxy || GET_NEEDSLEEP(pth))) {
                DEBUGCHK (!GET_SLEEPING(pth));
                MakeRun(pth);
                if (IsCeLogEnabled()) {
                    CELOG_ThreadResume(pth->hTh);
                }
            }
        } else if (retval = pth->bPendSusp) {
            // if the suspend is pending, just decrement the count and return
            pth->bPendSusp --;
            if (IsCeLogEnabled()) {
                CELOG_ThreadResume(pth->hTh);
            }
        }
    }
    KCALLPROFOFF(47);
    return retval;
}


/* When a thread gets ResumeThread() */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_ThreadResume(
    HANDLE hTh
    ) 
{
    DWORD retval;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadResume entry: %8.8lx\r\n",hTh));
    if (hTh == GetCurrentThread())
        hTh = hCurThread;
    retval = KCall((PKFN)ThreadResume,HandleToThread(hTh));
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadResume exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ThreadYield(void) 
{
    KCALLPROFON(35);
    DEBUGCHK(GET_RUNSTATE(pCurThread) == RUNSTATE_RUNNING);
    DEBUGCHK(RunList.pth == pCurThread);
    RunList.pth = 0;
    SetReschedule();
    MakeRun(pCurThread);
    KCALLPROFOFF(35);
}

typedef struct sleeper_t {
    PTHREAD pth;
    WORD wCount2;
    WORD wDirection;
    DWORD dwWakeupTime;
} sleeper_t;


// PutThreadToSleep
// Returns:
//        0    thread is put to sleep, or no need to sleep
//        1    continue search


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SleepOneMore (
    sleeper_t *pSleeper
    ) 
{
    PTHREAD pth, pth2;
    int nDiff;

    if (pth = pSleeper->pth) {
        if (pth->wCount2 != pSleeper->wCount2) {
            // restart if pth changes its position in the sleepq, or removed from sleepq
            pSleeper->pth = 0;
            pSleeper->wDirection = 0;
            return 1;
        }
#if 0    // FreeMem now save/restore the count and we should never see this again...
        if (pth->wCount2 == 0xabab && (DWORD) pth->pProc == 0xabababab) {
            DEBUGCHK(0);
            DEBUGMSG(1, (TEXT("PutThreadToSleep : restarting...\r\n")));
            pSleeper->pth = 0;
            pSleeper->wDirection = 0;
            return 1;
        }
#endif
    }

    if (pSleeper->wDirection) {
        // veritcal
        DEBUGCHK (pth && GET_SLEEPING(pth));
        DEBUGCHK (pSleeper->dwWakeupTime == pth->dwWakeupTime);
        DEBUGCHK((GET_RUNSTATE(pth) == RUNSTATE_BLOCKED) || (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN));

        if (GET_CPRIO (pCurThread) < GET_CPRIO (pth)) {
            // rare case when we're priority inverted while traversing down, restart
            pSleeper->pth = 0;
            pSleeper->wDirection = 0;
            return 1;
        }

        if ((pth2 = pth->pDownSleep) && (GET_CPRIO (pCurThread) >= GET_CPRIO (pth2))) {
            // continue vertical search
            pSleeper->pth = pth2;
            pSleeper->wCount2 = pth2->wCount2;
            return 1;
        }
        // insert between pth and pth2 vertically
        pCurThread->pNextSleepRun = pCurThread->pPrevSleepRun = 0;
        pCurThread->pUpSleep = pth;
        pCurThread->pDownSleep = pth2;
        pth->pDownSleep = pCurThread;
        if (pth2) {
            pth2->pUpSleep = pCurThread;
        }

    } else {
        // horizontal
        pth2 = pth? pth->pNextSleepRun : SleepList;

        if (pCurThread->pNextSleepRun = pth2) {
            nDiff = pth2->dwWakeupTime - pSleeper->dwWakeupTime;
            if (0 > nDiff) {
                // continue search horizontally
                pSleeper->pth = pth2;
                pSleeper->wCount2 = pth2->wCount2;
                return 1;
            }
            if (0 == nDiff) {
                if (GET_CPRIO (pCurThread) >= GET_CPRIO (pth2)) {
                    // start vertical search
                    pSleeper->wDirection = 1;
                    pSleeper->pth = pth2;
                    pSleeper->wCount2 = pth2->wCount2;
                    return 1;
                }
                // replace pth2 as head of vertical list
                pth2->wCount2 ++;
                pCurThread->pDownSleep = pth2;
                pCurThread->pNextSleepRun = pth2->pNextSleepRun;
                pth2->pUpSleep = pCurThread;
                pth2->pNextSleepRun = pth2->pPrevSleepRun = 0;
                if (pth2 = pCurThread->pNextSleepRun)
                    pth2->pPrevSleepRun = pCurThread;
            } else {
                // insert between pth and pth2
                pth2->pPrevSleepRun = pCurThread;
            }
        }

        // common part
        if (pCurThread->pPrevSleepRun = pth) {
            DEBUGCHK((GET_RUNSTATE(pth) == RUNSTATE_BLOCKED) || (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN));
            pth->pNextSleepRun = pCurThread;
        } else {
            // update SleepList
            SleepList = pCurThread;
            // We MUST do this update or NextThread might not pull us out of the queue
            dwReschedTime = pSleeper->dwWakeupTime;
        }
    }

    pCurThread->dwWakeupTime = pSleeper->dwWakeupTime;
    pSleeper->pth = 0;
    pSleeper->wDirection = 0;
    RunList.pth = 0;
    SetReschedule();

    DEBUGCHK(!((pCurThread->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
    SET_RUNSTATE(pCurThread,RUNSTATE_BLOCKED);
    SET_SLEEPING(pCurThread);
    return 0;

}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
PutThreadToSleep (
    sleeper_t *pSleeper
    ) 
{
    int nRet;
    KCALLPROFON(2);
    DEBUGCHK(!GET_SLEEPING(pCurThread));
    DEBUGCHK(!pCurThread->pUpSleep && !pCurThread->pDownSleep);

    CLEAR_NEEDSLEEP (pCurThread);

    if (GET_DYING(pCurThread) && !GET_DEAD(pCurThread) && GET_USERBLOCK(pCurThread)) {
        // thread dying
        KCALLPROFOFF(2);
        return 0;
    }

    if ((int) (pSleeper->dwWakeupTime - CurMSec) <= 0) {
        // time to wakeup already
        KCALLPROFOFF(2);
        return 0;
    }

    nRet = SleepOneMore (pSleeper);

    KCALLPROFOFF(2);
    return nRet;
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
    sleeper.dwWakeupTime = SC_GetTickCount () + time;
    
    while (KCall ((PKFN)PutThreadToSleep, &sleeper) || GET_NEEDSLEEP (pCurThread))
        ;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_Sleep(
    DWORD cMilliseconds
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_Sleep entry: %8.8lx\r\n",cMilliseconds));
    
    if (IsCeLogEnabled()) {
        CELOG_Sleep(cMilliseconds);
    }

    if (cMilliseconds == INFINITE)
        KCall((PKFN)ThreadSuspend,pCurThread, TRUE);
    else if (!cMilliseconds)
        KCall((PKFN)ThreadYield);
    else {
        if (cMilliseconds < 0xfffffffe)
            cMilliseconds ++;    // add 1 millisecond

        // handle cases when someone want to sleep *very* long
        if (cMilliseconds & 0x80000000) {
            DWORD dwToWakeup = CurMSec + 0x7fffffff;
            ThreadSleep (0x7fffffff);
            if ((int) (cMilliseconds -= 0x7fffffff + (CurMSec - dwToWakeup)) <= 0) {
                DEBUGMSG(ZONE_ENTRY,(L"SC_Sleep exit 1\r\n"));
                return;
            }
        }
        ThreadSleep (cMilliseconds);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_Sleep exit\r\n"));
}

void UB_SleepTillTick (void)
{
    DEBUGMSG (ZONE_ENTRY, (L"SC_SleepTillTick entry\r\n"));
    SET_USERBLOCK (pCurThread);
    ThreadSleep (1);
    CLEAR_USERBLOCK (pCurThread);
    DEBUGMSG (ZONE_ENTRY, (L"SC_SleepTillTick exit\r\n"));   
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
UB_Sleep(
    DWORD cMilliseconds
    ) 
{
    SET_USERBLOCK(pCurThread);
    SC_Sleep(cMilliseconds);
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

/* Creates a thread in a process */
void MDInitSecureStack (LPBYTE lpStack);

static LPBYTE CreateSecureStack (void)
{
    LPBYTE lpStack;
    DEBUGCHK (KERN_TRUST_FULL != pCurProc->bTrustLevel);
    if (MAX_SECURE_STACK <= InterlockedIncrement (&g_CntSecureStk)) {
        InterlockedDecrement (&g_CntSecureStk);
    } else if (lpStack = VirtualAlloc ((LPVOID)ProcArray[0].dwVMBase, CNP_STACK_SIZE, MEM_RESERVE|MEM_AUTO_COMMIT, PAGE_NOACCESS)) {
        if (VirtualAlloc(lpStack+CNP_STACK_SIZE-MIN_STACK_SIZE, MIN_STACK_SIZE, MEM_COMMIT, PAGE_READWRITE)) {
            MDInitSecureStack (lpStack);
            return lpStack;
        }
        VirtualFree (lpStack, 0, MEM_RELEASE);
    }
    return NULL;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
DoCreateThread(
    LPVOID lpStack,
    DWORD cbStack,
    LPVOID lpStart,
    LPVOID param,
    DWORD flags,
    LPDWORD lpthid,
    ulong mode,
    WORD prio
    ) 
{
    PTHREAD pth = 0, pdbgr;
    HANDLE hth;
    LPBYTE pSwapStack = 0, pSecureStk = 0;
    LPTHRDDBG pDbgInfo;
    DEBUGMSG(ZONE_SCHEDULE,(TEXT("Scheduler: DoCreateThread(%8.8lx, %8.8lx, %8.8lx, %8.8lx, %8.8lx, %8.8lx, %8.8lx %8.8lx)\r\n"),
            hCurThread,hCurProc,cbStack,lpStart,param,flags,lpthid,prio));

    if (!TBFf) {
        KSetLastError(pCurThread,ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    if (((flags & 0x80000000) || (pSwapStack = GetHelperStack()))       // swap stack available if needed?
        && ((KERN_TRUST_FULL == pCurProc->bTrustLevel) || (pSecureStk = CreateSecureStack ()))  // secure stack available if needed?
        && (pth = AllocMem (HEAP_THREAD))                                 // thread structure available?
        && (hth = AllocHandle (&cinfThread, pth, pCurProc))             // thread handle available?
        && (!pCurProc->hDbgrThrd || (pDbgInfo = AllocDbg(hCurProc)))) {   // debug info available if needed?   

        // zero'd out the area for debug registers if required
        if (cbNKCoProcRegSize && pSwapStack && pOEMInitCoProcRegisterSavedArea) {
            pOEMInitCoProcRegisterSavedArea (pSwapStack);
        }

        // increment the ref count of the thread handle
        IncRef (hth,pCurProc);

        // initialize thread structure
        InitThreadStruct (pth, hth, pCurProc, hCurProc, prio);
        pth->pSwapStack = pSwapStack;

        // use per-process VM address for stack
        if (!((ulong)lpStack >> VA_SECTION)) {
            lpStack = (LPVOID) ((ulong) lpStack + pCurProc->dwVMBase);
        }

        // record thread creation time
        GCFT(&pth->ftCreate);

        // perform machine dependent thread initialization
        if (flags & 0x80000000) {
            SET_DYING(pth);
            SET_DEAD(pth);
            MDCreateThread(pth, lpStack, cbStack, lpStart, param, mode, (ulong)param);
        } else if (flags & 0x40000000)
            MDCreateThread(pth, lpStack, cbStack, (LPVOID)lpStart, param, mode, (ulong)param);
        else
            MDCreateThread(pth, lpStack, cbStack, (LPVOID)TBFf, lpStart, mode, (ulong)param);
        ZeroTLS(pth);
        
        // save the original stack base/bound
        pth->dwOrigBase = KSTKBASE (pth);
        pth->dwOrigStkSize = cbStack;

        // put the default stack size on TLS so it's accessable without entering kernel.
        KPROCSTKSIZE(pth) = pCurProc->e32.e32_stackmax;

        // initialize secure stack info
        pth->tlsNonSecure = pth->tlsPtr;
        if (pSecureStk) {
            pth->tlsSecure = (LPDWORD) (pSecureStk + ((DWORD)pth->tlsPtr & 0xffff));
            pth->tlsSecure[PRETLS_STACKBASE] = (DWORD) pSecureStk;
            pth->tlsSecure[PRETLS_STACKBOUND] = (DWORD) pth->tlsSecure & ~(PAGE_SIZE-1);
            pth->tlsSecure[PRETLS_PROCSTKSIZE] = KPROCSTKSIZE (pth);
            pth->tlsPtr = pth->tlsNonSecure;
        } else {
            pth->tlsSecure = pth->tlsPtr;
        }


        pth->bSuspendCnt = 1;       // suspended the new thread first

        // debugger exist? init debug info
        // ??? WHY WE NEED THE DbgApiCS for Soooo long?
        EnterCriticalSection (&DbgApiCS);
        if (pCurProc->hDbgrThrd) {
            pth->pThrdDbg = pDbgInfo;
            pdbgr = HandleToThread (pCurProc->hDbgrThrd);
            DEBUGCHK(pdbgr);
            pth->pThrdDbg->hNextThrd = pdbgr->pThrdDbg->hFirstThrd;
            pdbgr->pThrdDbg->hFirstThrd = hth;
            IncRef(pth->hTh,pdbgr->pOwnerProc);
        }
        DEBUGCHK(!((pth->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
        
        SET_RUNSTATE(pth,RUNSTATE_BLOCKED);

        // Save off the thread's program counter for getting its name later.
        pth->dwStartAddr = (DWORD) lpStart;
       
        if (IsCeLogKernelEnabled()) {
            CELOG_ThreadCreate (pth, pth->pOwnerProc->hProc, NULL, (flags & CREATE_SUSPENDED));
        }
        if (pLogThreadCreate)
            pLogThreadCreate ((DWORD)pth, (DWORD)pCurProc);
        KCall ((PKFN)AddToProcRunnable, pCurProc, pth);
        LeaveCriticalSection (&DbgApiCS);

        if (!(flags & CREATE_SUSPENDED)) {
            KCall((PKFN)ThreadResume,pth);
        }
        if (lpthid)
            *lpthid = (DWORD)hth;
        
        return hth;
    }

    // Thread creation failed, free memory
    KSetLastError(pCurThread,ERROR_NOT_ENOUGH_MEMORY);
    
    if (pSwapStack)
        FreeHelperStack(pSwapStack);

    if (pSecureStk) {
        VirtualFree (pSecureStk, CNP_STACK_SIZE, MEM_DECOMMIT);
        VirtualFree (pSecureStk, 0, MEM_RELEASE);
        InterlockedDecrement (&g_CntSecureStk);
    }

    if (pth) {
        FreeMem (pth,HEAP_THREAD);
        if (hth) {
            FreeHandle (hth);
        }
    }
    return 0;
    
}

// Creates a thread



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
SC_CreateThread(
    LPSECURITY_ATTRIBUTES lpsa,
    DWORD cbStack,
    LPTHREAD_START_ROUTINE lpStartAddr,
    LPVOID lpvThreadParm,
    DWORD fdwCreate,
    LPDWORD lpIDThread
    ) 
{
    HANDLE retval;
    LPVOID lpStack;
    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateThread entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        lpsa,cbStack,lpStartAddr,lpvThreadParm,fdwCreate,lpIDThread));
    // verify parameter
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel) 
        && lpIDThread
        && !SC_MapPtrWithSize (lpIDThread, sizeof (DWORD), hCurProc)) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (PageFreeCount < (48*1024/PAGE_SIZE)) {
        KSetLastError(pCurThread,ERROR_NOT_ENOUGH_MEMORY);
        DEBUGMSG(ZONE_ENTRY,(L"SC_CreateThread exit: %8.8lx\r\n",0));
        return 0;
    }
    if (fdwCreate & ~(CREATE_SUSPENDED|STACK_SIZE_PARAM_IS_A_RESERVATION)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        DEBUGMSG(ZONE_ENTRY,(L"SC_CreateThread exit: %8.8lx\r\n",0));
        return 0;
    }
    // use process'es default stacksize unless specified
    if (!(fdwCreate & STACK_SIZE_PARAM_IS_A_RESERVATION)) {
        cbStack = pCurProc->e32.e32_stackmax;
    } else if ((int) cbStack <= 0) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        DEBUGMSG(ZONE_ENTRY,(L"SC_CreateThread exit: invalid stack size\r\n"));
        return 0;
    }
    // round the stack size up to the next 64k
    cbStack = (cbStack + 0xffff) & 0xffff0000;

    // allocate the stack
    lpStack = VirtualAlloc((LPVOID)pCurProc->dwVMBase,cbStack,MEM_RESERVE|MEM_AUTO_COMMIT,PAGE_NOACCESS);
    if (!lpStack) {
        KSetLastError(pCurThread,ERROR_NOT_ENOUGH_MEMORY);
        DEBUGMSG(ZONE_ENTRY,(L"SC_CreateThread exit: %8.8lx\r\n",0));
        return 0;
    }
    if (!VirtualAlloc((LPVOID)((DWORD)lpStack+cbStack-MIN_STACK_SIZE),MIN_STACK_SIZE, MEM_COMMIT,PAGE_READWRITE)) {
        VirtualFree(lpStack,0,MEM_RELEASE);
        KSetLastError(pCurThread,ERROR_NOT_ENOUGH_MEMORY);
        DEBUGMSG(ZONE_ENTRY,(L"SC_CreateThread exit: %8.8lx\r\n",0));
        return 0;
    }
    retval = DoCreateThread(lpStack, cbStack, (LPVOID)lpStartAddr, lpvThreadParm, fdwCreate, lpIDThread, TH_UMODE,THREAD_RT_PRIORITY_NORMAL);
    if (!retval) {
        VirtualFree (lpStack, cbStack, MEM_DECOMMIT);
        VirtualFree (lpStack, 0, MEM_RELEASE);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateThread exit: %8.8lx\r\n",retval));
    return retval;
}

// Creates a kernel thread



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
CreateKernelThread(
    LPTHREAD_START_ROUTINE lpStartAddr,
    LPVOID lpvThreadParm,
    WORD prio,
    DWORD dwFlags
    ) 
{
    LPVOID lpStack;
    CALLBACKINFO cbi;
    HANDLE hTh;
    if (!(lpStack = VirtualAlloc((LPVOID)ProcArray[0].dwVMBase,KRN_STACK_SIZE,MEM_RESERVE|MEM_AUTO_COMMIT,PAGE_NOACCESS)))
        return 0;
    if (!VirtualAlloc((LPVOID)((DWORD)lpStack+KRN_STACK_SIZE-MIN_STACK_SIZE),MIN_STACK_SIZE,MEM_COMMIT,PAGE_READWRITE)) {
        VirtualFree(lpStack,0,MEM_RELEASE);
        return 0;
    }
    if (pCurProc == &ProcArray[0]) {
        hTh = DoCreateThread(lpStack, KRN_STACK_SIZE, (LPVOID)lpStartAddr, lpvThreadParm, dwFlags, 0, TH_KMODE, prio);
    } else {
        cbi.hProc = ProcArray[0].hProc;
        cbi.pfn = (FARPROC)DoCreateThread;
        cbi.pvArg0 = lpStack;
        hTh = (HANDLE)PerformCallBack(&cbi,KRN_STACK_SIZE,(LPVOID)lpStartAddr, lpvThreadParm, dwFlags, 0, TH_KMODE, prio);
    }
    return hTh;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SetThreadBasePrio(
    HANDLE hth,
    DWORD nPriority
    ) 
{
    BOOL bRet;
    PTHREAD pth;
    DWORD oldb, oldc, prio;
    KCALLPROFON(28);
    bRet = FALSE;
    if (pth = HandleToThread(hth)) {
        oldb = GET_BPRIO(pth);
        if (oldb != nPriority) {
            oldc = GET_CPRIO(pth);
            SET_BPRIO(pth,(WORD)nPriority);

            // update process priority watermark
            if (nPriority < pth->pOwnerProc->bPrio)
                pth->pOwnerProc->bPrio = (BYTE) nPriority;
                
            if (pCurThread == pth) {
                // thread is running, can't be in sleep queue.
                if (pCurThread->pOwnedList && ((prio = pCurThread->pOwnedList->bListedPrio) < nPriority))
                    nPriority = prio;
                if (nPriority != GET_CPRIO(pCurThread)) {
                    SET_CPRIO(pCurThread,nPriority);
                    if (RunList.pRunnable && (nPriority > GET_CPRIO(RunList.pRunnable)))
                        SetReschedule();
                }
            } else {
                if (nPriority < oldc) {
                    SET_CPRIO(pth,nPriority);
                    if (GET_RUNSTATE(pth) == RUNSTATE_RUNNABLE) {
                        RunqDequeue(pth,oldc);
                        MakeRun(pth);
                    } else if (GET_SLEEPING (pth) && pth->pUpSleep && (GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN)) {
                        SleepqDequeue (pth);
                        SET_NEEDSLEEP (pth);
                        if (pth->lpce) {
                            pth->bWaitState = WAITSTATE_PROCESSING; // re-process the wait
                        }
                        MakeRun (pth);
                    }
                }
            }
        }
        if (IsCeLogEnabled()) {
            CELOG_ThreadSetPriority(hth, nPriority);
        }
        bRet = TRUE;
    }
    KCALLPROFOFF(28);
    return bRet;
}

typedef struct sttd_t {
    HANDLE hThNeedRun;
    HANDLE hThNeedBoost;
} sttd_t;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
SetThreadToDie(
    HANDLE hTh,
    DWORD dwExitCode,
    sttd_t *psttd
    ) 
{
    PTHREAD pth;
    HANDLE hRet;
    DWORD prio;
    LPPROXY pprox;
    KCALLPROFON(17);
    if (!hTh)
        hRet = pCurProc->pTh->hTh;
    else if (!(pth = HandleToThread(hTh)))
        hRet = 0;
    else {
        hRet = pth->pNextInProc ? pth->pNextInProc->hTh : 0;
        if (!GET_DYING(pth) && !GET_DEAD(pth)) {
            SET_DYING(pth);
            pth->tlsPtr[TLSSLOT_KERNEL] |= TLSKERN_TRYINGTODIE;
            SetUserInfo(hTh,dwExitCode);
            psttd->hThNeedBoost = hTh;
            pth->bPendSusp = 0;     // clear pending suspend bit, no longer need to suspend since we're dying
            switch (GET_RUNSTATE(pth)) {
                case RUNSTATE_RUNNABLE:
                    if ((pth->pOwnerProc == pth->pProc) && (LLcs.OwnerThread != hTh) && !(GetThreadIP(pth) & 0x80000000)) {
                        if (pth->bWaitState == WAITSTATE_PROCESSING) {
                            DEBUGCHK (0);
                            if (pth->lpce) {
                                pth->lpce->base = pth->lpProxy;
                                pth->lpce->size = (DWORD)pth->lpPendProxy;
                            } else if (pprox = pth->lpProxy) {
                                // must be an interrupt event - special case it!
                                DEBUGCHK(pprox->bType == HT_MANUALEVENT);
                                DEBUGCHK(((LPEVENT)pprox->pObject)->pProxList == pprox);
                                ((LPEVENT)pprox->pObject)->pProxList = 0;
                                pprox->pQDown = 0;
                            }
                            pth->lpProxy = 0;
                            pth->wCount++;
                            pth->bWaitState = WAITSTATE_SIGNALLED;
                        } else if (!pth->pcstkTop) {
                            prio = GET_CPRIO(pth);
                            RunqDequeue(pth,prio);
                            SetThreadIP(pth, pExitThread);
#ifdef THUMBSUPPORT
                            if ((DWORD)pExitThread & 1)
                                pth->ctx.Psr |= THUMB_STATE;
                            else
                                pth->ctx.Psr &= ~THUMB_STATE;
#endif
                            SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                            psttd->hThNeedRun = hTh;
                        }
                    }
                    break;
                case RUNSTATE_NEEDSRUN:
                    if ((pth->pOwnerProc == pth->pProc) && (LLcs.OwnerThread != hTh) && !(GetThreadIP(pth) & 0x80000000)) {
                        pth->bSuspendCnt = (hTh == hCurrScav ? 1 : 0);
                        SetThreadIP(pth, pExitThread);
#ifdef THUMBSUPPORT
                        if ((DWORD)pExitThread & 1)
                            pth->ctx.Psr |= THUMB_STATE;
                        else
                            pth->ctx.Psr &= ~THUMB_STATE;
#endif
                        psttd->hThNeedRun = hTh;
                    }
                    break;
                case RUNSTATE_BLOCKED:
                    if ((pth->pOwnerProc == pth->pProc) && (LLcs.OwnerThread != hTh) &&
                        (GET_USERBLOCK(pth) || !(GetThreadIP(pth) & 0x80000000))) {
                        if (!GET_USERBLOCK(pth)) {
                            SetThreadIP(pth, pExitThread);
#ifdef THUMBSUPPORT
                            if ((DWORD)pExitThread & 1)
                                pth->ctx.Psr |= THUMB_STATE;
                            else
                                pth->ctx.Psr &= ~THUMB_STATE;
#endif
                        } else if (pprox = pth->lpProxy) {
                            if (pth->lpce) {
                                pth->lpce->base = pprox;
                                pth->lpce->size = (DWORD)pth->lpPendProxy;
                            } else {
                                // must be an interrupt event - special case it!
                                DEBUGCHK(pprox->bType == HT_MANUALEVENT);
                                DEBUGCHK(((LPEVENT)pprox->pObject)->pProxList == pprox);
                                ((LPEVENT)pprox->pObject)->pProxList = 0;
                                pprox->pQDown = 0;
                            }
                            pth->lpProxy = 0;
                            pth->bWaitState = WAITSTATE_SIGNALLED;
                        }
                        pth->bSuspendCnt = (hTh == hCurrScav ? 1 : 0);
                        pth->wCount++;
                        SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                        psttd->hThNeedRun = hTh;
                    }
                    break;
                case RUNSTATE_RUNNING:
                    break;
                default:
                    DEBUGCHK(0);
                    break;
            }
        }
    }
    KCALLPROFOFF(17);
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
KillOneThread(
    HANDLE hTh,
    DWORD dwRetVal
    ) 
{
    sttd_t sttd;
    HANDLE hRet;
    ACCESSKEY ulOldKey;
    sttd.hThNeedRun = 0;
    sttd.hThNeedBoost = 0;
    SWITCHKEY(ulOldKey,0xffffffff);
    hRet = (HANDLE)KCall((PKFN)SetThreadToDie,hTh,dwRetVal,&sttd);
    SETCURKEY(ulOldKey);
    if (sttd.hThNeedBoost)
        KCall((PKFN)SetThreadBasePrio,sttd.hThNeedBoost,GET_CPRIO(pCurThread));
    if (sttd.hThNeedRun)
        KCall((PKFN)MakeRunIfNeeded,sttd.hThNeedRun);
    return hRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ProcTerminate(
    HANDLE hProc,
    DWORD dwRetVal
    ) 
{
    ACCESSKEY ulOldKey;
    PPROCESS pProc;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcTerminate entry: %8.8lx %8.8lx\r\n",hProc,dwRetVal));
    if (IsCeLogKernelEnabled()) {
        CELOG_ProcessTerminate(hProc);
    }
    if (hProc == GetCurrentProcess())
        hProc = hCurProc;
    if (!(pProc = HandleToProc(hProc))) {
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
        return FALSE;
    }
    SWITCHKEY(ulOldKey,0xffffffff);
    if (*(LPDWORD)MapPtrProc(pIsExiting,pProc)) {
        SETCURKEY(ulOldKey);
        KSetLastError(pCurThread,ERROR_ACCESS_DENIED);
        DEBUGMSG(ZONE_ENTRY,(L"SC_ProcTerminate exit 1: %8.8lx\r\n",FALSE));
        return FALSE;
    }
    KillOneThread(pProc->pMainTh->hTh,dwRetVal);

    SETCURKEY(ulOldKey);
    pPSLNotify(DLL_PROCESS_EXITING,(DWORD)hProc,(DWORD)NULL);
    SWITCHKEY(ulOldKey,0xffffffff);

    if ((hProc != hCurProc) && (WaitForMultipleObjects(1,&hProc,FALSE,3000) != WAIT_TIMEOUT)) {
        SETCURKEY(ulOldKey);
        DEBUGMSG(ZONE_ENTRY,(L"SC_ProcTerminate exit 2: %8.8lx\r\n",TRUE));
        return TRUE;
    }
    SETCURKEY(ulOldKey);
    KSetLastError(pCurThread,ERROR_ACCESS_DENIED);
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcTerminate exit 3: %8.8lx\r\n",FALSE));
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_TerminateSelf(void) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_TerminateSelf entry\r\n"));
    SC_ProcTerminate(hCurProc,0);
    DEBUGMSG(ZONE_ENTRY,(L"SC_TerminateSelf exit\r\n"));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ThreadTerminate(
    HANDLE hThread,
    DWORD dwExitcode
    ) 
{
    PTHREAD pTh;
    if (IsCeLogKernelEnabled()) {
        CELOG_ThreadTerminate(hThread);
    }
    if (hThread == GetCurrentThread())
        hThread = hCurThread;
    if (pTh = HandleToThread(hThread)) {
        if (!pTh->pNextInProc)
            return SC_ProcTerminate(pTh->pOwnerProc->hProc, dwExitcode);
        KillOneThread(hThread,dwExitcode);
        if ((hThread != hCurThread) && (WaitForMultipleObjects(1,&hThread,FALSE,3000) != WAIT_TIMEOUT))
            return TRUE;
    }
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}

/* Returns TRUE until only the main thread is around */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_OtherThreadsRunning(void) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_OtherThreadsRunning entry\r\n"));
    DEBUGCHK(pCurProc == pCurThread->pOwnerProc);
    if ((pCurProc->pTh != pCurThread) || pCurProc->dwDyingThreads) {
        DEBUGCHK(!(pCurProc->dwDyingThreads & 0x80000000));
        DEBUGMSG(ZONE_ENTRY,(L"SC_OtherThreadsRunning exit: %8.8lx\r\n",TRUE));
        return TRUE;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_OtherThreadsRunning exit: %8.8lx\r\n",FALSE));
    return FALSE;
}

// Final clean-up for a thread, and the process if it's the main thread


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
RemoveThread(
    RTs *pRTs
    ) 
{
    PTHREAD pth;
    KCALLPROFON(43);
    DEBUGCHK(!pCurThread->pOwnedList);
    DEBUGCHK(RunList.pth == pCurThread);
    RunList.pth = 0;
    SetReschedule();
    if (pCurThread->pNextInProc) {
        if (!(pCurThread->pNextInProc->pPrevInProc = pCurThread->pPrevInProc)) {
            DEBUGCHK(pCurThread->pOwnerProc->pTh == pCurThread);
            pCurThread->pOwnerProc->pTh = pCurThread->pNextInProc;
        } else
            pCurThread->pPrevInProc->pNextInProc = pCurThread->pNextInProc;
    } else {
        DEBUGCHK (!pCurThread->pPrevInProc);
        pCurThread->pOwnerProc->pTh = pCurThread->pOwnerProc->pMainTh = 0;
    }
    pRTs->pThrdDbg = pCurThread->pThrdDbg;
    pRTs->hThread = hCurThread;
#ifdef DEBUG
    SetObjectPtr(hCurThread, (PVOID)0xabababcd);
#else
    SetObjectPtr(hCurThread, 0);
#endif
    pth = pRTs->pHelper;
    DEBUGCHK(pth->bSuspendCnt == 1);
    pth->bSuspendCnt = 0;
    MakeRun (pth);
    
    KCALLPROFOFF(43);
}





//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_KillAllOtherThreads(void) 
{
    HANDLE hTh;
    DEBUGMSG(ZONE_ENTRY,(L"SC_KillAllOtherThreads entry\r\n"));
    hTh = pCurProc->pTh->hTh;
    while (hTh != hCurThread)
        hTh = KillOneThread(hTh,0);
    DEBUGMSG(ZONE_ENTRY,(L"SC_KillAllOtherThreads exit\r\n"));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_IsPrimaryThread(void) 
{
    BOOL retval;
    retval = (pCurThread->pNextInProc ? FALSE : TRUE);
    DEBUGMSG(ZONE_ENTRY,(L"SC_IsPrimaryThread exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
GetCurThreadKTime(void) 
{
    DWORD retval;
    KCALLPROFON(48);
    retval = pCurThread->dwKernTime;
    if (GET_TIMEMODE(pCurThread))
        retval += (CurMSec - CurrTime);
    KCALLPROFOFF(48);
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
GetCurThreadUTime(void) 
{
    DWORD retval;
    KCALLPROFON(65);
    retval = pCurThread->dwUserTime;
    if (!GET_TIMEMODE(pCurThread))
        retval += (CurMSec - CurrTime);
    KCALLPROFOFF(65);
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_GetThreadTimes(
    HANDLE hThread,
    LPFILETIME lpCreationTime,
    LPFILETIME lpExitTime,
    LPFILETIME lpKernelTime,
    LPFILETIME lpUserTime
    ) 
{
    BOOL retval;
    DWORD dwKTime, dwUTime;
    PTHREAD pTh;
    THREADTIME *ptt;
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
        && (!SC_MapPtrWithSize (lpCreationTime, sizeof (FILETIME), hCurProc)
            || !SC_MapPtrWithSize (lpExitTime, sizeof (FILETIME), hCurProc)
            || !SC_MapPtrWithSize (lpKernelTime, sizeof (FILETIME), hCurProc)
            || !SC_MapPtrWithSize (lpUserTime, sizeof (FILETIME), hCurProc))) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!(pTh = HandleToThreadPerm(hThread))) {
        LockPages(&lpCreationTime,sizeof(LPFILETIME),0,LOCKFLAG_WRITE);
        LockPages(&lpExitTime,sizeof(LPFILETIME),0,LOCKFLAG_WRITE);
        LockPages(&lpKernelTime,sizeof(LPFILETIME),0,LOCKFLAG_WRITE);
        LockPages(&lpUserTime,sizeof(LPFILETIME),0,LOCKFLAG_WRITE);
        EnterCriticalSection(&NameCS);
        for (ptt = TTList; ptt; ptt = ptt->pnext) {
            if (ptt->hTh == hThread) {
                __try {
                    *lpCreationTime = ptt->CreationTime;
                    *lpExitTime = ptt->ExitTime;
                    *lpKernelTime = ptt->KernelTime;
                    *lpUserTime = ptt->UserTime;
                    retval = TRUE;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    retval = FALSE;
                }
                break;
            }
        }
        LeaveCriticalSection(&NameCS);
        UnlockPages(&lpCreationTime,sizeof(LPFILETIME));
        UnlockPages(&lpExitTime,sizeof(LPFILETIME));
        UnlockPages(&lpKernelTime,sizeof(LPFILETIME));
        UnlockPages(&lpUserTime,sizeof(LPFILETIME));
        if (!ptt) {
            KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
            retval = FALSE;
        }
    } else {
        __try {
            dwUTime = (pTh != pCurThread) ? pTh->dwUserTime : KCall((PKFN)GetCurThreadUTime);
            dwKTime = (pTh != pCurThread) ? pTh->dwKernTime : KCall((PKFN)GetCurThreadKTime);
            *lpCreationTime = pTh->ftCreate;
            *(__int64 *)lpExitTime = 0;
            *(__int64 *)lpUserTime = dwUTime;
            *(__int64 *)lpUserTime *= 10000;
            *(__int64 *)lpKernelTime = dwKTime;
            *(__int64 *)lpKernelTime *= 10000;
            retval = TRUE;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            retval = FALSE;
        }
    }
    return retval;
}


LPCRITICAL_SECTION csarray[] = {
    &ppfscs,
    &ODScs,
    &PhysCS,
    &VAcs,
    &CompCS,
    &MapCS,
    &PagerCS,
    &NameCS,
    &WriterCS,
    &ModListcs,
    &RFBcs,
    &MapNameCS,
    &DbgApiCS,
    &LLcs,
    &PageOutCS,
    &rtccs,
    &DirtyPageCS,
};

#define NUM_CSARRAY (sizeof(csarray) / sizeof(csarray[0]))

BOOL CanTakeCS (LPCRITICAL_SECTION lpcs)
{
    if (lpcs->OwnerThread != hCurThread) {
        int i;
        for (i = 0; i < NUM_CSARRAY; i ++) {
            if (lpcs == csarray[i]) {
                // no CS order violation, okay to take it
                break;
            }
            if (csarray[i]->OwnerThread == hCurThread) {
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
    DWORD loop;
    // anyone can call the debugger, and it may cause demand loads... but then they can't call the debugger
    if (InDebugger || (lpcs == &csDbg) || (lpcs == &EdbgODScs) || (lpcs == &TimerListCS))
        return;
    // We expect ethdbg's per-client critical sections to be unknown
    for (loop = 0; loop < NUM_CSARRAY; loop++)
        if (csarray[loop] == lpcs)
            break;
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
        // since we don't allow paging for PPFS anymore, it's alright if anyone holding ppfs and acqure other CS
        if (csarray[loop]->OwnerThread == hCurThread) {
            DEBUGMSG(ZONE_SCHEDULE,(L"CHECKTAKECRITSEC: Violation of critical section ordering at index %d, lpcs %8.8lx\r\n",loop,lpcs));
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
void 
Monitor1(void) 
{
    while (1) {
        Sleep(30000);
        NKDbgPrintfW(L"\r\n\r\n");
        NKDbgPrintfW(L" ODScs -> %8.8lx\r\n",ODScs.OwnerThread);
        NKDbgPrintfW(L" CompCS -> %8.8lx\r\n",CompCS.OwnerThread);
        NKDbgPrintfW(L" PhysCS -> %8.8lx\r\n",PhysCS.OwnerThread);
        NKDbgPrintfW(L" VAcs -> %8.8lx\r\n",VAcs.OwnerThread);
        NKDbgPrintfW(L" LLcs -> %8.8lx\r\n",LLcs.OwnerThread);
        NKDbgPrintfW(L" ModListcs -> %8.8lx\r\n",ModListcs.OwnerThread);
        NKDbgPrintfW(L" ppfscs -> %8.8lx\r\n",ppfscs.OwnerThread);
        NKDbgPrintfW(L" RFBcs -> %8.8lx\r\n",RFBcs.OwnerThread);
        NKDbgPrintfW(L" MapCS -> %8.8lx\r\n",MapCS.OwnerThread);
        NKDbgPrintfW(L" NameCS -> %8.8lx\r\n",NameCS.OwnerThread);
        NKDbgPrintfW(L" DbgApiCS -> %8.8lx\r\n",DbgApiCS.OwnerThread);
        NKDbgPrintfW(L" PagerCS -> %8.8lx\r\n",PagerCS.OwnerThread);
        NKDbgPrintfW(L" WriterCS -> %8.8lx\r\n",WriterCS.OwnerThread);
        NKDbgPrintfW(L" rtccs -> %8.8lx\r\n",rtccs.OwnerThread);
        NKDbgPrintfW(L" MapNameCS -> %8.8lx\r\n",MapNameCS.OwnerThread);
        NKDbgPrintfW(L" DirtyPageCS -> %8.8lx\r\n", DirtyPageCS.OwnerThread);
        NKDbgPrintfW(L"\r\n\r\n");
    }
}

#endif



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void SC_ForcePageout(void) 
{
    ACCESSKEY ulOldKey;
    TRUSTED_API_VOID (L"SC_ForcePageout");

    SWITCHKEY(ulOldKey,0xffffffff); // for pageout
    EnterPhysCS();
    ScavengeStacks(100000);     // Reclaim all extra stack pages.
    LeaveCriticalSection(&PhysCS);
    PageOutForced = 1;
    DoPageOut();
    PageOutForced = 0;
    SETCURKEY(ulOldKey);
}

LPCRIT lpCritList;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FreeCrit(
    LPCRIT pcrit
    ) 
{
    while (pcrit->pProxList)
        KCall((PKFN)DequeuePrioProxy,pcrit->pProxList);
    KCall((PKFN)DoFreeCrit,pcrit);
    FreeMem(pcrit,HEAP_CRIT);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
SC_CreateCrit(
    LPCRITICAL_SECTION lpcs
    ) 
{
    LPCRIT pcrit, pTrav;
    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateCrit entry: %8.8lx\r\n",lpcs));
    // verify parameter
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel) 
        && !SC_MapPtrWithSize ((LPVOID) ((DWORD)lpcs & ~1), sizeof (CRITICAL_SECTION), hCurProc)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return 0;
    }
    if ((DWORD)lpcs & 1) {
        lpcs = MapPtr((LPCRITICAL_SECTION)((DWORD)lpcs & ~1));
#ifdef DEBUG
        // debugchk only if the CS is held by a thread that is still alive
        if (lpcs->OwnerThread) {
            PTHREAD pth = HandleToThread(lpcs->OwnerThread);
            if (pth && !GET_DYING(pth) && !GET_DEAD(pth)) {
                DEBUGCHK (0);
            }
        }
#endif
        if (!(pcrit = (LPCRIT) lpcs->hCrit)) {
            DEBUGMSG (1, (L"Deleting an uninitialized critical section, ignored!\n"));
            return 0;
        }
        if (!IsValidKPtr(pcrit) || (pcrit->lpcs != lpcs)) {
            DEBUGCHK (0);
            return 0;
        }

        EnterCriticalSection(&NameCS);
        if (lpCritList) {
            for (pTrav = (LPCRIT)((DWORD)&lpCritList-offsetof(CRIT,pNext)); pTrav->pNext; pTrav = pTrav->pNext)
                if (pTrav->pNext == pcrit) {
                    pTrav->pNext = pTrav->pNext->pNext;
                    FreeCrit(pcrit);
                    break;
                }
        }
        LeaveCriticalSection(&NameCS);
    } else {
        lpcs = MapPtr(lpcs);
        if (!(pcrit = AllocMem(HEAP_CRIT))) {
            DEBUGMSG(ZONE_ENTRY,(L"SC_CreateCrit exit: %8.8lx\r\n",0));
            return 0;
        }
        memset(pcrit->pProxHash,0,sizeof(pcrit->pProxHash));
        pcrit->pProxList = 0;
        pcrit->lpcs = lpcs;
        pcrit->bListed = 0;
        pcrit->iOwnerProc = pCurProc->procnum;
        EnterCriticalSection(&NameCS);
        pcrit->pNext = lpCritList;
        lpCritList = pcrit;
        LeaveCriticalSection(&NameCS);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateCrit exit: %8.8lx\r\n",pcrit));
    return pcrit;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
CloseAllCrits(void) 
{
    LPCRIT pTrav, pCrit;
    EnterCriticalSection(&NameCS);
    while (lpCritList && (lpCritList->iOwnerProc == pCurProc->procnum)) {
        pCrit = lpCritList;
        lpCritList = pCrit->pNext;
        FreeCrit(pCrit);
    }
    if (pTrav = lpCritList) {
        while (pCrit = pTrav->pNext) {
            if (pCrit->iOwnerProc == pCurProc->procnum) {
                pTrav->pNext = pCrit->pNext;
                FreeCrit(pCrit);
            } else
                pTrav = pCrit;
        }
    }
    LeaveCriticalSection(&NameCS);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
WaitConfig(
    LPPROXY pProx,
    CLEANEVENT *lpce
    ) 
{
    KCALLPROFON(1);
    DEBUGCHK(RunList.pth == pCurThread);
    DEBUGCHK(!pCurThread->lpProxy);
    pCurThread->lpPendProxy = pProx;
    pCurThread->lpce = lpce;
    pCurThread->bWaitState = WAITSTATE_PROCESSING;
    pCurThread->pCrabPth = 0;
    pCurThread->wCrabDir = 0;
    DEBUGCHK(RunList.pth == pCurThread);
    KCALLPROFOFF(1);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PrioEnqueue(
    LPEVENT pevent,
    DWORD prio,
    LPPROXY pProx
    ) 
{
    DWORD prio2;
    LPPROXY pProx2, pProx3;
    prio2 = prio/PRIORITY_LEVELS_HASHSCALE;
    if (!pevent->pProxList) {
        DEBUGCHK(!pProx->pQPrev && !pProx->pQNext);
        pProx->pQUp = pProx->pQDown = pevent->pProxHash[prio2] = pevent->pProxList = pProx;
    } else if (prio < pevent->pProxList->prio) {
        DEBUGCHK(!pProx->pQPrev);
        pProx->pQUp = pProx->pQDown = pevent->pProxHash[prio2] = pevent->pProxList->pQPrev = pProx;
        pProx->pQNext = pevent->pProxList;
        pevent->pProxList = pProx;
    } else if (pProx2 = pevent->pProxHash[prio2]) {
        if (prio < pProx2->prio) {
            pProx->pQPrev = pProx2->pQPrev;
            pProx->pQNext = pProx2;
            pProx->pQUp = pProx->pQDown = pevent->pProxHash[prio2] = pProx->pQPrev->pQNext = pProx2->pQPrev = pProx;
        } else {
FinishEnqueue:
            // bounded by MAX_PRIORITY_HASHSCALE
            while ((pProx3 = pProx2->pQNext) && (prio > pProx3->prio))
                pProx2 = pProx3;
            if (prio == pProx2->prio) {
                pProx->pQUp = pProx2->pQUp;
                pProx->pQUp->pQDown = pProx2->pQUp = pProx;
                pProx->pQDown = pProx2;
                DEBUGCHK(!pProx->pQPrev && !pProx->pQNext);
            } else if (!pProx3) {
                DEBUGCHK(!pProx->pQNext);
                pProx->pQPrev = pProx2;
                pProx->pQUp = pProx->pQDown = pProx2->pQNext = pProx;
            } else {
                 if (prio == pProx3->prio) {
                    pProx->pQUp = pProx3->pQUp;
                    pProx->pQUp->pQDown = pProx3->pQUp = pProx;
                    pProx->pQDown = pProx3;
                    DEBUGCHK(!pProx->pQPrev && !pProx->pQNext);
                 } else {
                    pProx->pQUp = pProx->pQDown = pProx;
                    pProx->pQPrev = pProx2;
                    pProx2->pQNext = pProx3->pQPrev = pProx;
                    pProx->pQNext = pProx3;
                 }
            }
        }
    } else {
        pevent->pProxHash[prio2] = pProx;
        // bounded by PRIORITY_LEVELS_HASHSIZE
        while (!(pProx2 = pevent->pProxHash[--prio2]))
            ;
        goto FinishEnqueue;
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FlatEnqueue(
    PTHREAD pth,
    LPPROXY pProx
    ) 
{
    LPPROXY pProx2;
    if (!(pProx2 = pth->pProxList)) {
        pProx->pQUp = pProx->pQDown = pProx;
        pProx->pQPrev = (LPPROXY)pth;
        pth->pProxList = pProx;
    } else {
        DEBUGCHK(!pProx->pQPrev);
        pProx->pQUp = pProx2->pQUp;
        pProx->pQDown = pProx2;
        pProx2->pQUp = pProx->pQUp->pQDown = pProx;
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
WaitOneMore(
    LPDWORD pdwContinue,
    LPCRIT *ppCritMut
    ) 
{
    LPPROXY pProx;
    DWORD retval;
    BOOL noenqueue;
    KCALLPROFON(18);
    CLEAR_NEEDSLEEP (pCurThread);
    noenqueue = !pCurThread->lpce && !pCurThread->dwPendTime;
    DEBUGCHK(RunList.pth == pCurThread);
    if (pCurThread->bWaitState == WAITSTATE_SIGNALLED) {
        *pdwContinue = 0;
        retval = pCurThread->dwPendReturn;
    } else if (GET_DYING(pCurThread) && !GET_DEAD(pCurThread) && GET_USERBLOCK(pCurThread)) {
        *pdwContinue = 0;
        KSetLastError(pCurThread,ERROR_OPERATION_ABORTED);
        retval = WAIT_FAILED;
        goto exitfunc;
    } else if (pProx = pCurThread->lpPendProxy) {
        DEBUGCHK(pCurThread->bWaitState == WAITSTATE_PROCESSING);
        DEBUGCHK(pCurThread == pProx->pTh);
        DEBUGCHK(pProx->pTh == pCurThread);
        DEBUGCHK(pProx->wCount == pCurThread->wCount);
        pProx->pObject = GetObjectPtr(pProx->pObject);
        switch (pProx->bType) {
            case SH_CURTHREAD: {
                PTHREAD pthWait;
                if (!(pthWait = (PTHREAD)pProx->pObject) || GET_DEAD(pthWait))
                    goto waitok;
                if (!noenqueue)
                    FlatEnqueue(pthWait,pProx);
                break;
            }
            case SH_CURPROC: {
                PPROCESS pprocWait;
                if (!(pprocWait = (PPROCESS)pProx->pObject))
                    goto waitok;
                if (!noenqueue)
                    FlatEnqueue((PTHREAD)pprocWait,pProx);
                break;
            }
            case HT_EVENT: {
                LPEVENT pevent;
                BYTE prio;
                if (!(pevent = (LPEVENT)pProx->pObject))
                    goto waitbad;
                if (pevent->state) {    // event is already signalled
                    DEBUGCHK(!pevent->pIntrProxy || !pevent->manualreset);
                    pevent->state = pevent->manualreset;
                    goto waitok;
                }
                if (!noenqueue) {
                    prio = pProx->prio;
                    if (!pevent->onequeue) {
                        PrioEnqueue(pevent,prio,pProx);
                    } else {
                        pProx->bType = HT_MANUALEVENT;
                        if (pevent->pIntrProxy) {
                            // interrupt event
                            if (pevent->pProxList) {
                                // more than one threads waiting on the same interrupt event
                                DEBUGCHK (0);
                                goto waitbad;
                            }
                            pProx->pQUp = pProx->pQDown = pProx;
                            pProx->pQPrev = (LPPROXY) pevent;
                            pevent->pProxList = pProx;
                        } else {
                            FlatEnqueue((PTHREAD)pevent,pProx);
                            if (prio < pevent->bMaxPrio)
                                pevent->bMaxPrio = prio;
                        }
                    }
                }
                break;
            }
            case HT_SEMAPHORE: {
                LPSEMAPHORE psem;
                BYTE prio;
                if (!(psem = (LPSEMAPHORE)pProx->pObject))
                    goto waitbad;
                if (psem->lCount) {
                    psem->lCount--;
                    goto waitok;
                }
                if (!noenqueue) {
                    prio = pProx->prio;
                    PrioEnqueue((LPEVENT)psem,prio,pProx);
                }
                break;
            }
            case HT_MUTEX: {
                LPMUTEX pmutex;
                BYTE prio;
                if (!(pmutex = (LPMUTEX)pProx->pObject))
                    goto waitbad;
                if (!pmutex->pOwner) {
                    pmutex->LockCount = 1;
                    pmutex->pOwner = pCurThread;
                    *ppCritMut = (LPCRIT)pmutex;
                    goto waitok;
                }
                if (pmutex->pOwner == pCurThread) {
                    pmutex->LockCount++;
                    goto waitok;
                }
                if (!noenqueue) {
                    prio = pProx->prio;
                    if (!pmutex->pProxList || (prio < pmutex->pProxList->prio))
                        *ppCritMut = (LPCRIT)pmutex;
                    PrioEnqueue((LPEVENT)pmutex,prio,pProx);
                }
                break;
            }
            default:
                goto waitbad;
        }
        if (!noenqueue) {
            pCurThread->lpPendProxy = pProx->pThLinkNext;
            pProx->pThLinkNext = pCurThread->lpProxy;
            pCurThread->lpProxy = pProx;
            retval = 0; // doesn't matter, we're continuing
        } else {
            retval = WAIT_TIMEOUT;
            goto exitfunc;
        }
    } else if (pCurThread->dwPendTime == INFINITE) {
        *pdwContinue = 0;
        pCurThread->bWaitState = WAITSTATE_BLOCKED;
        DEBUGCHK(!((pCurThread->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
        SET_RUNSTATE(pCurThread,RUNSTATE_BLOCKED);
        RunList.pth = 0;
        SetReschedule();
        retval = 0; // doesn't matter, only an event can wake us
    } else {
        if (!pCurThread->dwPendTime || (((int) (CurMSec - pCurThread->dwPendWakeup)) >= 0)) {
            DEBUGCHK(pCurThread->lpce);
            retval = WAIT_TIMEOUT;
            goto exitfunc;
        }
        if (SleepOneMore ((sleeper_t *) &pCurThread->pCrabPth)) {
            KCALLPROFOFF(18);
            return 0;    // doesn't matter, we're continueing
        }
        DEBUGCHK(!pCurThread->lpPendProxy);
        pCurThread->bWaitState = WAITSTATE_BLOCKED;
        *pdwContinue = 0;
        retval = WAIT_TIMEOUT;
    }
    KCALLPROFOFF(18);
    return retval;
waitbad:
    KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
    retval = WAIT_FAILED;
    goto exitfunc;
waitok:
    retval = pProx->dwRetVal;
exitfunc:
    if (pCurThread->lpce) {
        pCurThread->lpce->base = pCurThread->lpProxy;
        pCurThread->lpce->size = (DWORD)pCurThread->lpPendProxy;
    }
    pCurThread->lpProxy = 0;
    pCurThread->wCount++;
    pCurThread->bWaitState = WAITSTATE_SIGNALLED;
    *pdwContinue = 0;
    KCALLPROFOFF(18);
    return retval;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_WaitForMultiple(
    DWORD cObjects,
    CONST HANDLE *lphObjects,
    BOOL fWaitAll,
    DWORD dwTimeout
    ) 
{
    DWORD loop, retval;
    DWORD dwContinue;
    LPPROXY pHeadProx, pCurrProx;
    CLEANEVENT *lpce;
    PCALLSTACK pcstk;
    LPEVENT lpe;
    LPCRIT pCritMut;
    cObjects &= ~0x80000000;
    DEBUGMSG(ZONE_ENTRY,(L"SC_WaitForMultiple entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        cObjects,lphObjects,fWaitAll,dwTimeout));
    if (IsCeLogEnabled()) {
        CELOG_WaitForMultipleObjects(cObjects, lphObjects, fWaitAll, dwTimeout);
    }
    if (fWaitAll || !cObjects || (cObjects > MAXIMUM_WAIT_OBJECTS)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        DEBUGMSG(ZONE_ENTRY,(L"SC_WaitForMultiple exit: %8.8lx\r\n",WAIT_FAILED));
        return WAIT_FAILED;
    }
    if ((GetHandleType(lphObjects[0]) == HT_EVENT) && (lpe = HandleToEvent(lphObjects[0])) && lpe->pIntrProxy) {
        lpce = 0;
        DEBUGCHK(cObjects == 1);
        pHeadProx = lpe->pIntrProxy;
        if ((pcstk = pCurThread->pcstkTop) && ((DWORD)pcstk->pprcLast < 0x10000)) {
            memcpy(&pCurThread->IntrStk,pcstk,sizeof(CALLSTACK));
            pCurThread->pcstkTop = &pCurThread->IntrStk;
            if (IsValidKPtr(pcstk))
                FreeMem(pcstk,HEAP_CALLSTACK);
        }
    } else if (!(lpce = AllocMem(HEAP_CLEANEVENT)) || !(pHeadProx = (LPPROXY)AllocMem(HEAP_PROXY))) {
        if (lpce)
            FreeMem(lpce,HEAP_CLEANEVENT);
        KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
        DEBUGMSG(ZONE_ENTRY,(L"SC_WaitForMultiple exit: %8.8lx\r\n",WAIT_FAILED));
        return WAIT_FAILED;
    }
    pHeadProx->pQNext = pHeadProx->pQPrev = 0;
    pHeadProx->wCount = pCurThread->wCount;
    pHeadProx->pObject = lphObjects[0];
    pHeadProx->bType = GetHandleType(lphObjects[0]);
    pHeadProx->pTh = pCurThread;
    pHeadProx->prio = (BYTE)GET_CPRIO(pCurThread);
    pHeadProx->dwRetVal = WAIT_OBJECT_0;
    pCurrProx = pHeadProx;
    for (loop = 1; loop < cObjects; loop++) {
        if (!(pCurrProx->pThLinkNext = (LPPROXY)AllocMem(HEAP_PROXY))) {
            for (pCurrProx = pHeadProx; pCurrProx; pCurrProx = pHeadProx) {
                pHeadProx = pCurrProx->pThLinkNext;
                FreeMem(pCurrProx,HEAP_PROXY);
            }
            if (lpce)
                FreeMem(lpce,HEAP_CLEANEVENT);
            KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
            DEBUGMSG(ZONE_ENTRY,(L"SC_WaitForMultiple exit: %8.8lx\r\n",WAIT_FAILED));
            return WAIT_FAILED;
        }
        pCurrProx = pCurrProx->pThLinkNext;
        pCurrProx->pQNext = pCurrProx->pQPrev = 0;
        pCurrProx->wCount = pCurThread->wCount;
        pCurrProx->pObject = lphObjects[loop];
        pCurrProx->bType = GetHandleType(lphObjects[loop]);
        DEBUGCHK((pCurrProx->bType != HT_EVENT) || !HandleToEvent(pCurrProx->pObject)->pIntrProxy);
        pCurrProx->pTh = pCurThread;
        pCurrProx->prio = (BYTE)GET_CPRIO(pCurThread);
        pCurrProx->dwRetVal = WAIT_OBJECT_0 + loop;
    }
    pCurrProx->pThLinkNext = 0;
    // max timeout is 0x7fffffff now
    if (dwTimeout) {
        if (dwTimeout < 0x7fffffff) {
            dwTimeout ++;    // add 1 msec
        } else if (dwTimeout != INFINITE) {
            dwTimeout = 0x7fffffff;
        }
    }
    pCurThread->dwPendTime = dwTimeout;
    pCurThread->dwPendWakeup = SC_GetTickCount () + dwTimeout;
    KCall((PKFN)WaitConfig,pHeadProx,lpce);
    dwContinue = 2;
    do {
        pCritMut = 0;
        retval = KCall((PKFN)WaitOneMore,&dwContinue,&pCritMut);
        if (GET_NEEDSLEEP (pCurThread)) {
            // priority changed while sleeping, just retry with current timeout settings
            dwContinue = 2;
        } else if (pCritMut) {
            if (!dwContinue)
                KCall((PKFN)LaterLinkCritMut,pCritMut,0);
            else
                KCall((PKFN)PostBoostMut,pCritMut);
        }
    } while (dwContinue);
    DEBUGCHK(lpce == pCurThread->lpce);
    if (lpce) {
        pCurThread->lpce = 0;
        pHeadProx = (LPPROXY)lpce->base;
        while (pHeadProx) {
            pCurrProx = pHeadProx->pThLinkNext;
            DequeueProxy(pHeadProx);
            FreeMem(pHeadProx,HEAP_PROXY);
            pHeadProx = pCurrProx;
        }
        pHeadProx = (LPPROXY)lpce->size;
        while (pHeadProx) {
            pCurrProx = pHeadProx->pThLinkNext;
            FreeMem(pHeadProx,HEAP_PROXY);
            pHeadProx = pCurrProx;
        }
        FreeMem(lpce,HEAP_CLEANEVENT);
    }
    DEBUGCHK(!pCurThread->lpce);
    DEBUGMSG(ZONE_ENTRY,(L"SC_WaitForMultiple exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
UB_WaitForMultiple(
    DWORD cObjects,
    CONST HANDLE *lphObjects,
    BOOL fWaitAll,
    DWORD dwTimeout
    ) 
{
    DWORD dwRet;
    SET_USERBLOCK(pCurThread);
    dwRet = SC_WaitForMultiple(cObjects,lphObjects,fWaitAll,dwTimeout);
    CLEAR_USERBLOCK(pCurThread);
    return dwRet;
}

/* Set thread priority */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ThreadSetPrio(
    HANDLE hTh,
    DWORD prio
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadSetPrio entry: %8.8lx %8.8lx\r\n",hTh,prio));
    if (hTh == GetCurrentThread())
        hTh = hCurThread;
    if (!HandleToThreadPerm(hTh)) {
        KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
        DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadSetPrio exit: %8.8lx\r\n",FALSE));
        return FALSE;
    }
    if (prio >= MAX_WIN32_PRIORITY_LEVELS) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadSetPrio exit: %8.8lx\r\n",FALSE));
        return FALSE;
    }

    if (!KCall((PKFN)SetThreadBasePrio, hTh, W32PrioMap[prio])) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadSetPrio exit: %8.8lx\r\n",FALSE));
        return FALSE;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadSetPrio exit: %8.8lx\r\n",TRUE));
    return TRUE;
}

/* Get thread priority */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SC_ThreadGetPrio(
    HANDLE hTh
    ) 
{
    PTHREAD pth;
    int retval;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadGetPrio entry: %8.8lx\r\n",hTh));
    if (!(pth = HandleToThreadPerm(hTh))) {
        retval = THREAD_PRIORITY_ERROR_RETURN;
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
    } else {
        retval = GET_BPRIO(pth);

        // if the priority hasn't been mapped by OEM, return TIME_CRITICAL for any thread
        // in RT range. Otherwise return THREAD_PRIORITY_ERROR_RETURN if there's no mapping
        // for the priority

        if (MAX_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS == W32PrioMap[0]) {
            // mapping not changed        
            if (retval < MAX_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS)
                retval = MAX_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS;
            retval -= (MAX_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS);        
        } else {
            int i;
            for (i= 0; i < MAX_WIN32_PRIORITY_LEVELS; i ++) {
                if (retval == W32PrioMap[i]) {
                    retval = i;
                    break;
                }
            }
            if (MAX_WIN32_PRIORITY_LEVELS == i) {
                retval = THREAD_PRIORITY_ERROR_RETURN;
                KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
            }
        }
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadGetPrio exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
SC_CeGetThreadPriority(
    HANDLE hTh
    ) 
{
    PTHREAD pth;
    int retval;
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetThreadPriority entry: %8.8lx\r\n",hTh));
    if (pth = HandleToThread(hTh))
        retval = GET_BPRIO(pth);
    else {
        retval = THREAD_PRIORITY_ERROR_RETURN;
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetThreadPriority exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_CeSetThreadPriority(
    HANDLE hTh,
    DWORD prio
    ) 
{
    TRUSTED_API (L"SC_CeSetThreadPriority", FALSE);
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeSetThreadPriority entry: %8.8lx %8.8lx\r\n",hTh,prio));
    if (hTh == GetCurrentThread())
        hTh = hCurThread;
    if (prio >= MAX_PRIORITY_LEVELS) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        DEBUGMSG(ZONE_ENTRY,(L"SC_CeSetThreadPriority exit: %8.8lx\r\n",FALSE));
        return FALSE;
    }
    if (!KCall((PKFN)SetThreadBasePrio,hTh,prio)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        DEBUGMSG(ZONE_ENTRY,(L"SC_CeSetThreadPriority exit: %8.8lx\r\n",FALSE));
        return FALSE;
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeSetThreadPriority exit: %8.8lx\r\n",TRUE));
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
SC_CeGetThreadQuantum(
    HANDLE hTh
    ) 
{
    PTHREAD pth;
    DWORD retval;
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetThreadQuantum entry: %8.8lx\r\n",hTh));
    if (pth = HandleToThread(hTh))
        retval = pth->dwQuantum;
    else {
        retval = (DWORD)-1;
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetThreadQuantum exit: %8.8lx\r\n",retval));
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_CeSetThreadQuantum(
    HANDLE hTh,
    DWORD dwTime
    ) 
{
    PTHREAD pth;
    TRUSTED_API (L"SC_CeSetThreadQuantum", FALSE);
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeSetThreadQuantum entry: %8.8lx %8.8lx\r\n",hTh,dwTime));
    if ((pth = HandleToThread(hTh)) && !(dwTime & 0x80000000)) {
        pth->dwQuantum = dwTime;
        DEBUGMSG(ZONE_ENTRY,(L"SC_CeSetThreadQuantum exit: %8.8lx\r\n",TRUE));
        if (IsCeLogEnabled()) {
            CELOG_ThreadSetQuantum(pth->hTh, dwTime);
        }
        return TRUE;
    }
    KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
    DEBUGMSG(ZONE_ENTRY,(L"SC_CeSetThreadQuantum exit: %8.8lx\r\n",FALSE));
    return FALSE;
}

#define CSWAIT_TAKEN 0
#define CSWAIT_PEND 1
#define CSWAIT_ABORT 2



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
CSWaitPart1(
    LPCRIT *ppCritMut,
    LPPROXY pProx,
    LPCRIT pcrit
    ) 
{
    BOOL retval;
    LPCRITICAL_SECTION lpcs;
    PTHREAD pOwner;
    BYTE prio;
    KCALLPROFON(19);
    DEBUGCHK(RunList.pth == pCurThread);
    DEBUGCHK(GET_RUNSTATE(pCurThread) != RUNSTATE_BLOCKED);
    if (pCurThread->bWaitState == WAITSTATE_SIGNALLED) {
        retval = CSWAIT_TAKEN;
        DEBUGCHK(RunList.pth == pCurThread);
    } else if (GET_DYING(pCurThread) && !GET_DEAD(pCurThread) && GET_USERBLOCK(pCurThread) &&
        (pCurThread->pOwnerProc == pCurProc)) {
        retval = CSWAIT_ABORT;
        DEBUGCHK(RunList.pth == pCurThread);
        goto exitcs1;
    } else {
        DEBUGCHK(RunList.pth == pCurThread);
        DEBUGCHK(pProx == pCurThread->lpPendProxy);
        DEBUGCHK(pCurThread->bWaitState == WAITSTATE_PROCESSING);
        DEBUGCHK(pCurThread == pProx->pTh);
        DEBUGCHK(pProx->pTh == pCurThread);
        DEBUGCHK(pProx->wCount == pCurThread->wCount);
        DEBUGCHK(pProx->bType == HT_CRITSEC);
        DEBUGCHK(pcrit = (LPCRIT)pProx->pObject);
        lpcs = pcrit->lpcs;
        if (!lpcs->OwnerThread || (!lpcs->needtrap && ((DWORD)lpcs->OwnerThread & 1)) ||
            !(pOwner = HandleToThread((HANDLE)((DWORD)lpcs->OwnerThread & ~1))) ||
            (!lpcs->needtrap && (pcrit->bListed != 1) && GET_BURIED(pOwner) && !IsKernelVa (lpcs))) {
            DEBUGCHK(RunList.pth == pCurThread);
            lpcs->OwnerThread = hCurThread;
            lpcs->LockCount = 1;
            DEBUGCHK(!pcrit->pProxList);
            DEBUGCHK(pcrit->bListed != 1);
            retval = CSWAIT_TAKEN;
            DEBUGCHK(RunList.pth == pCurThread);
            goto exitcs1;
        } else if ((pOwner->hLastCrit == lpcs->hCrit) && (GET_CPRIO(pOwner) > GET_CPRIO(pCurThread))) {
            DEBUGCHK(RunList.pth == pCurThread);
            DEBUGCHK(lpcs->LockCount == 1);
            DEBUGCHK(pOwner->lpCritProxy);
            DEBUGCHK(!pOwner->lpProxy);
            DEBUGCHK(pOwner->wCount == (unsigned short)(pOwner->lpCritProxy->wCount + 1));
            lpcs->OwnerThread = hCurThread;
            DEBUGCHK(pcrit->bListed != 1);
            if (pcrit->pProxList) {
                *ppCritMut = pcrit;
                lpcs->needtrap = 1;
            } else
                DEBUGCHK(!lpcs->needtrap);
            retval = CSWAIT_TAKEN;
            DEBUGCHK(RunList.pth == pCurThread);
            goto exitcs1;
        } else {
            DEBUGCHK(RunList.pth == pCurThread);
            prio = pProx->prio;
            DEBUGCHK(!pcrit->pProxList || lpcs->needtrap);
            if (!pcrit->pProxList || (prio < pcrit->pProxList->prio))
                *ppCritMut = pcrit;
            PrioEnqueue((LPEVENT)pcrit,prio,pProx);
            lpcs->needtrap = 1;
            DEBUGCHK(RunList.pth == pCurThread);
            // get here if blocking
            DEBUGCHK(!pProx->pThLinkNext);
            pCurThread->lpPendProxy = 0;
            DEBUGCHK(!pCurThread->lpProxy);
            pProx->pThLinkNext = 0;
            pCurThread->lpProxy = pProx;
            retval = CSWAIT_PEND;
            DEBUGCHK(RunList.pth == pCurThread);
        }
    }
    DEBUGCHK(RunList.pth == pCurThread);
    DEBUGCHK(GET_RUNSTATE(pCurThread) != RUNSTATE_BLOCKED);
    KCALLPROFOFF(19);
    return retval;
exitcs1:
    DEBUGCHK(pCurThread->lpce);
    DEBUGCHK(!pCurThread->lpProxy);
    DEBUGCHK(pCurThread->lpPendProxy);
    pCurThread->lpce->base = 0;
    pCurThread->lpce->size = (DWORD)pCurThread->lpPendProxy;
    pCurThread->wCount++;
    pCurThread->bWaitState = WAITSTATE_SIGNALLED;
    KCALLPROFOFF(19);
    DEBUGCHK(RunList.pth == pCurThread);
    DEBUGCHK(GET_RUNSTATE(pCurThread) != RUNSTATE_BLOCKED);
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
CSWaitPart2(void) 
{
    KCALLPROFON(58);
    if (pCurThread->bWaitState != WAITSTATE_SIGNALLED) {
        if (GET_DYING(pCurThread) && !GET_DEAD(pCurThread) && GET_USERBLOCK(pCurThread) && (pCurThread->pOwnerProc == pCurProc)) {
            DEBUGCHK(pCurThread->lpce);
            DEBUGCHK(pCurThread->lpProxy);
            pCurThread->lpce->base = pCurThread->lpProxy;
            DEBUGCHK(!pCurThread->lpPendProxy);
            pCurThread->lpce->size = 0;
            pCurThread->lpProxy = 0;
            pCurThread->wCount++;
            pCurThread->bWaitState = WAITSTATE_SIGNALLED;
        } else {
            LPCRIT pCrit;
            PTHREAD pth;
            pCrit = (LPCRIT)pCurThread->lpProxy->pObject;
            if (!pCrit->bListed) {
                pth = HandleToThread((HANDLE)((DWORD)pCrit->lpcs->OwnerThread & ~1));
                DEBUGCHK(pth);
                pth->hLastCrit = 0;
                LinkCritMut(pCrit,pth,1);
            }
            DEBUGCHK(pCrit->lpcs->needtrap);
            pCurThread->bWaitState = WAITSTATE_BLOCKED;
            DEBUGCHK(!((pCurThread->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
            SET_RUNSTATE(pCurThread,RUNSTATE_BLOCKED);
            RunList.pth = 0;
            SetReschedule();
        }
    }
    KCALLPROFOFF(58);
}

/* When a thread tries to get a Critical Section */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_TakeCritSec(
    LPCRITICAL_SECTION lpcs
    ) 
{
    BOOL bRetry;
    LPPROXY pProx;
    LPCRIT pCritMut;
    LPCLEANEVENT lpce;
    bRetry = 1;
    if (!IsValidKPtr(lpcs->hCrit) || (((LPCRIT) (lpcs->hCrit))->lpcs != MapPtr(lpcs))) {
        DEBUGCHK (0);
        return;
    }
    do {
        pProx = AllocMem(HEAP_PROXY);
        lpce = AllocMem(HEAP_CLEANEVENT);
        if (!pProx || !lpce) {
            if (pProx)
                FreeMem(pProx,HEAP_PROXY);
            SC_Sleep (0);   // yield before retry
            continue;
        }
        DEBUGCHK(pProx && lpce && lpcs->hCrit);
        pProx->pQNext = pProx->pQPrev = pProx->pQDown = 0;
        pProx->wCount = pCurThread->wCount;
        pProx->pObject = (LPBYTE)lpcs->hCrit;
        pProx->bType = HT_CRITSEC;
        pProx->pTh = pCurThread;
        pProx->prio = (BYTE)GET_CPRIO(pCurThread);
        pProx->dwRetVal = WAIT_OBJECT_0;
        pProx->pThLinkNext = 0;
#ifdef DEBUG
        DEBUGCHK(!((pCurThread->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
        pCurThread->wInfo |= (1 << DEBUG_LOOPCNT_SHIFT);
#endif
        pCurThread->dwPendTime = INFINITE;    // required??
        KCall((PKFN)WaitConfig,pProx,lpce);
        pCritMut = 0;
        switch (KCall((PKFN)CSWaitPart1,&pCritMut,pProx,lpcs->hCrit)) {
            case CSWAIT_TAKEN: 
                DEBUGCHK(!pCritMut || (pCritMut == (LPCRIT)lpcs->hCrit));
                if (pCritMut && (hCurThread == lpcs->OwnerThread))
                    KCall((PKFN)LaterLinkCritMut,pCritMut,1);
                break;
            case CSWAIT_PEND:
                DEBUGCHK(!pCritMut || (pCritMut == (LPCRIT)lpcs->hCrit));
                if (pCritMut) {
                    KCall((PKFN)PostBoostCrit1,pCritMut);
                    KCall((PKFN)PostBoostCrit2,pCritMut);
                }
                if (IsCeLogEnabled()) {
                    CELOG_CSEnter(lpcs->hCrit, lpcs->OwnerThread);
                }
#ifdef DEBUG
                pCurThread->wInfo &= ~(1 << DEBUG_LOOPCNT_SHIFT);
#endif    
                KCall((PKFN)CSWaitPart2);
                break;
            case CSWAIT_ABORT:
                bRetry = 0;
                break;
        }
#ifdef DEBUG
        pCurThread->wInfo &= ~(1 << DEBUG_LOOPCNT_SHIFT);
#endif    
        DEBUGCHK(!pCurThread->lpProxy);
        DEBUGCHK(pCurThread->lpce == lpce);
        DEBUGCHK(((lpce->base == pProx) && !lpce->size) || (!lpce->base && (lpce->size == (DWORD)pProx)));
        if (pProx->pQDown)
            if (KCall((PKFN)DequeuePrioProxy,pProx))
                KCall((PKFN)DoReprioCrit,lpcs->hCrit);
        DEBUGCHK(!pProx->pQDown);
        pCurThread->lpce = 0;
        FreeMem(pProx,HEAP_PROXY);
        FreeMem(lpce,HEAP_CLEANEVENT);
    } while ((lpcs->OwnerThread != hCurThread) && bRetry);
    if (lpcs->OwnerThread == hCurThread)
        KCall((PKFN)CritFinalBoost,lpcs);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
UB_TakeCritSec(
    LPCRITICAL_SECTION lpcs
    ) 
{
    SET_USERBLOCK(pCurThread);
    SC_TakeCritSec(lpcs);
    CLEAR_USERBLOCK(pCurThread);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
PreLeaveCrit(
    LPCRITICAL_SECTION lpcs
    ) 
{
    BOOL bRet;
    KCALLPROFON(53);
    if (((DWORD)lpcs->OwnerThread != ((DWORD)hCurThread | 1)))
        bRet = FALSE;
    else {
        lpcs->OwnerThread = hCurThread;
        bRet = TRUE;
        SET_NOPRIOCALC(pCurThread);
        UnlinkCritMut((LPCRIT)lpcs->hCrit,pCurThread);
        ((LPCRIT)lpcs->hCrit)->bListed = 2;
    }
    KCALLPROFOFF(53);
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
LeaveCrit(
    LPCRITICAL_SECTION lpcs,
    HANDLE *phTh
    ) 
{
    LPCRIT pcrit;
    PTHREAD pNewOwner;
    LPPROXY pprox, pDown, pNext;
    WORD prio;
    BOOL bRet;
    KCALLPROFON(20);
    bRet = FALSE;
    DEBUGCHK(lpcs->OwnerThread == hCurThread);
    pcrit = (LPCRIT)(lpcs->hCrit);
    DEBUGCHK(lpcs->LockCount == 1);
    if (!(pprox = pcrit->pProxList)) {
        DEBUGCHK(pcrit->bListed != 1);
        lpcs->OwnerThread = 0;
        lpcs->needtrap = 0;
    } else {
        // dequeue proxy
        prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
        pDown = pprox->pQDown;
        pNext = pprox->pQNext;
        DEBUGCHK(pcrit->pProxHash[prio] == pprox);
        pcrit->pProxHash[prio] = ((pDown != pprox) ? pDown :
            (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
        if (pDown == pprox) {
            if (pcrit->pProxList = pNext)
                pNext->pQPrev = 0;
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = pcrit->pProxList = pDown;
            DEBUGCHK(!pDown->pQPrev);
            if (pNext) {
                pNext->pQPrev = pDown;
                pDown->pQNext = pNext;
            }
        }
        pprox->pQDown = 0;
        // wake up new owner
        pNewOwner = pprox->pTh;
        DEBUGCHK(pNewOwner);
        if (pNewOwner->wCount != pprox->wCount)
            bRet = 1;
        else {
            DEBUGCHK(!GET_SLEEPING(pNewOwner));
            DEBUGCHK(pNewOwner->lpce);
            DEBUGCHK(pNewOwner->lpProxy == pprox);
            DEBUGCHK(!pNewOwner->lpPendProxy);
            pNewOwner->lpce->base = pprox;
            pNewOwner->lpce->size = 0;
            pNewOwner->wCount++;
            pNewOwner->lpProxy = 0;
            pNewOwner->lpCritProxy = pprox; // since we can steal it back, don't lose proxy
            if (pNewOwner->bWaitState == WAITSTATE_BLOCKED) {
                DEBUGCHK(GET_RUNSTATE(pNewOwner) != RUNSTATE_RUNNABLE);
                DEBUGCHK(GET_RUNSTATE(pNewOwner) != RUNSTATE_RUNNING);
                DEBUGCHK(GET_RUNSTATE(pNewOwner) != RUNSTATE_NEEDSRUN);
                SET_RUNSTATE(pNewOwner,RUNSTATE_NEEDSRUN);
                *phTh = pNewOwner->hTh;
            } else {
                DEBUGCHK(GET_RUNSTATE(pNewOwner) != RUNSTATE_BLOCKED);
                pNewOwner->bWaitState = WAITSTATE_SIGNALLED;
            }
            if (!pcrit->pProxList)
                lpcs->needtrap = 0;
            else
                DEBUGCHK(lpcs->needtrap);
            lpcs->LockCount = 1;
            lpcs->OwnerThread = pNewOwner->hTh;
            DEBUGCHK(!pNewOwner->hLastCrit);
            pNewOwner->hLastCrit = lpcs->hCrit;
            if (IsCeLogEnabled()) {
                CELOG_CSLeave(lpcs->hCrit, pNewOwner->hTh);
            }
      }
    }
    if (!bRet) {
        DEBUGCHK(pcrit->bListed != 1);
        pcrit->bListed = 0;
    }
    KCALLPROFOFF(20);
    return bRet;
}

/* When a thread releases a Critical Section */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_LeaveCritSec(
    LPCRITICAL_SECTION lpcs
    ) 
{
    HANDLE hth;
    if (!IsValidKPtr(lpcs->hCrit) || (((LPCRIT) (lpcs->hCrit))->lpcs != MapPtr (lpcs))) {
        DEBUGCHK (0);
        return;
    }
    if (KCall((PKFN)PreLeaveCrit,lpcs)) {
        hth = 0;
        while (KCall((PKFN)LeaveCrit,lpcs,&hth))
            ;
        if (hth)
            KCall((PKFN)MakeRunIfNeeded,hth);
        KCall((PKFN)PostUnlinkCritMut);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PuntCritSec(
    CRITICAL_SECTION *pcs
    ) 
{
    if (pcs->OwnerThread == hCurThread) {
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
    PuntCritSec(&VAcs);
    PuntCritSec(&ppfscs);
    PuntCritSec(&ODScs);
    PuntCritSec(&RFBcs);
    PuntCritSec(&MapCS);
    PuntCritSec(&MapNameCS);
    PuntCritSec(&WriterCS);
    PuntCritSec(&NameCS);
    PuntCritSec(&ModListcs);
    PuntCritSec(&LLcs);
    PuntCritSec(&DbgApiCS);
    PuntCritSec(&csDbg);
    PuntCritSec(&DirtyPageCS);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
LeaveMutex(
    LPMUTEX lpm,
    HANDLE *phTh
    ) 
{
    BOOL bRet;
    PTHREAD pth;
    LPPROXY pprox, pDown, pNext;
    WORD prio;
    KCALLPROFON(21);
    bRet = 0;
    DEBUGCHK(pCurThread == lpm->pOwner);
    if (!(pprox = lpm->pProxList)) {
        lpm->pOwner = 0;
    } else {
        // dequeue proxy
        prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
        pDown = pprox->pQDown;
        pNext = pprox->pQNext;
        DEBUGCHK(lpm->pProxHash[prio] == pprox);
        lpm->pProxHash[prio] = ((pDown != pprox) ? pDown :
            (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
        if (pDown == pprox) {
            if (lpm->pProxList = pNext)
                pNext->pQPrev = 0;
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = lpm->pProxList = pDown;
            DEBUGCHK(!pDown->pQPrev);
            if (pNext) {
                pNext->pQPrev = pDown;
                pDown->pQNext = pNext;
            }
        }
        pprox->pQDown = 0;
        // wake up new owner
        pth = pprox->pTh;
        DEBUGCHK(pth);
        if (pth->wCount != pprox->wCount)
            bRet = 1;
        else {
            DEBUGCHK(pth->lpProxy);
            DEBUGCHK(pth->lpce);
            pth->lpce->base = pth->lpProxy;
            pth->lpce->size = (DWORD)pth->lpPendProxy;
            pth->wCount++;
            pth->lpProxy = 0;
            if (pth->bWaitState == WAITSTATE_BLOCKED) {
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                pth->retValue = pprox->dwRetVal;
                SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                *phTh = pth->hTh;
            } else {
                DEBUGCHK(!GET_SLEEPING(pth));
                pth->dwPendReturn = pprox->dwRetVal;
                pth->bWaitState = WAITSTATE_SIGNALLED;
            }
            lpm->LockCount = 1;
            lpm->pOwner = pth;
            LinkCritMut((LPCRIT)lpm,pth,0);
        }
    }
    KCALLPROFOFF(21);
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoLeaveMutex(
    LPMUTEX lpm
    ) 
{
    HANDLE hth;
    KCall((PKFN)PreUnlinkCritMut,lpm);
    hth = 0;
    while (KCall((PKFN)LeaveMutex,lpm,&hth))
        ;
    if (hth)
        KCall((PKFN)MakeRunIfNeeded,hth);
    KCall((PKFN)PostUnlinkCritMut);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ReleaseMutex(
    HANDLE hMutex
    ) 
{
    LPMUTEX lpm;
    if (IsCeLogEnabled()) {
        CELOG_MutexRelease(hMutex);
    }
    if (!(lpm = HandleToMutexPerm(hMutex))) {
        KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (lpm->pOwner != pCurThread) {
        SetLastError(ERROR_NOT_OWNER);
        return FALSE;
    }
    if (lpm->LockCount != 1)
        lpm->LockCount--;
    else
        DoLeaveMutex(lpm);
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LONG 
SemAdd(
    LPSEMAPHORE lpsem,
    LONG lReleaseCount
    ) 
{
    LONG prev;
    KCALLPROFON(3);
    if ((lReleaseCount <= 0) ||
        (lpsem->lCount + lpsem->lPending + lReleaseCount > lpsem->lMaxCount) ||
        (lpsem->lCount + lpsem->lPending + lReleaseCount < lpsem->lCount + lpsem->lPending)) {
        KCALLPROFOFF(3);
        return -1;
    }
    prev = lpsem->lCount + lpsem->lPending;
    lpsem->lPending += lReleaseCount;
    KCALLPROFOFF(3);
    return prev;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SemPop(
    LPSEMAPHORE lpsem,
    LPLONG pRemain,
    HANDLE *phTh
    ) 
{
    PTHREAD pth;
    LPPROXY pprox, pDown, pNext;
    WORD prio;
    BOOL bRet;
    KCALLPROFON(37);
    bRet = 0;
    if (*pRemain) {
        DEBUGCHK(*pRemain <= lpsem->lPending);
        if (!(pprox = lpsem->pProxList)) {
            lpsem->lCount += *pRemain;
            lpsem->lPending -= *pRemain;
        } else {
            bRet = 1;
            // dequeue proxy
            prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
            pDown = pprox->pQDown;
            pNext = pprox->pQNext;
            DEBUGCHK(lpsem->pProxHash[prio] == pprox);
            lpsem->pProxHash[prio] = ((pDown != pprox) ? pDown :
                (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
            if (pDown == pprox) {
                if (lpsem->pProxList = pNext)
                    pNext->pQPrev = 0;
            } else {
                pDown->pQUp = pprox->pQUp;
                pprox->pQUp->pQDown = lpsem->pProxList = pDown;
                DEBUGCHK(!pDown->pQPrev);
                if (pNext) {
                    pNext->pQPrev = pDown;
                    pDown->pQNext = pNext;
                }
            }
            pprox->pQDown = 0;
            // wake up thread        
            pth = pprox->pTh;
            DEBUGCHK(pth);
            if (pth->wCount == pprox->wCount) {
                DEBUGCHK(pth->lpProxy);
                if (pth->lpce) {
                    pth->lpce->base = pth->lpProxy;
                    pth->lpce->size = (DWORD)pth->lpPendProxy;
                }
                pth->wCount++;
                pth->lpProxy = 0;
                if (pth->bWaitState == WAITSTATE_BLOCKED) {
                    DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                    DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                    DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                    pth->retValue = pprox->dwRetVal;
                    SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                    *phTh = pth->hTh;
                } else {
                    DEBUGCHK(!GET_SLEEPING(pth));
                    pth->dwPendReturn = pprox->dwRetVal;
                    pth->bWaitState = WAITSTATE_SIGNALLED;
                }
                lpsem->lPending--;
                (*pRemain)--;
            }
        }
    }
    KCALLPROFOFF(37);
    return bRet;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ReleaseSemaphore(
    HANDLE hSem,
    LONG lReleaseCount,
    LPLONG lpPreviousCount
    ) 
{
    LONG prev;
    HANDLE hth;
    LPSEMAPHORE pSem;
    BOOL bRet = FALSE;
    if (!(pSem = HandleToSem(hSem)) || ((prev = KCall((PKFN)SemAdd,pSem,lReleaseCount)) == -1)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        if (IsCeLogEnabled()) {
            CELOG_SemaphoreRelease(hSem, lReleaseCount, (LONG)-1);
        }
    } else {
        if (lpPreviousCount)
            *lpPreviousCount = prev;
        hth = 0;
        while (KCall((PKFN)SemPop,pSem,&lReleaseCount,&hth)) {
            if (hth) {
                KCall((PKFN)MakeRunIfNeeded,hth);
                hth = 0;
            }
        }
        bRet = TRUE;
        if (IsCeLogEnabled()) {
            CELOG_SemaphoreRelease(hSem, lReleaseCount, prev);
        }
    }
    return bRet;
}

PHDATA ZapHandle(HANDLE h);
extern PHDATA PhdPrevReturn, PhdNext;
extern LPBYTE pFreeKStacks;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
KillSpecialThread(void) 
{
    PHDATA phd;
    LPBYTE pStk;
    KCALLPROFON(36);
    DEBUGCHK(RunList.pth == pCurThread);
    RunList.pth = 0;
    SetReschedule();
    DEBUGCHK(pCurThread->pNextInProc);
    if (!pCurThread->pPrevInProc) {
        DEBUGCHK(pCurProc->pTh == pCurThread);
        pCurProc->pTh = pCurThread->pNextInProc;
        pCurProc->pTh->pPrevInProc = 0;
    } else {
        pCurThread->pPrevInProc->pNextInProc = pCurThread->pNextInProc;
        pCurThread->pNextInProc->pPrevInProc = pCurThread->pPrevInProc;
    }
    DEBUGCHK(!pCurThread->lpce);
    DEBUGCHK(!pCurThread->lpProxy);
    DEBUGCHK(!pCurThread->pProxList);
    //DEBUGCHK(!pCurThread->pOwnedList);
    DEBUGCHK(!pCurThread->pcstkTop);
    DEBUGCHK(!pCurThread->pSwapStack);
    pStk = (LPBYTE) KSTKBASE(pCurThread);
    DEBUGCHK(IsValidKPtr(pStk));
    *(LPBYTE *)pStk = pFreeKStacks;
    pFreeKStacks = pStk;
    phd = (PHDATA)(((ulong)hCurThread & HANDLE_ADDRESS_MASK) + KData.handleBase);
    DEBUGCHK(phd->ref.count == 2);
    DEBUGCHK(((DWORD)phd->hValue & 0x1fffffff) == (((ulong)phd & HANDLE_ADDRESS_MASK) | 2));
    // not really needed, just helpful to avoid stale handle issues
    // phd->hValue = (HANDLE)((DWORD)phd->hValue+0x20000000);
    phd->linkage.fwd->back = phd->linkage.back;
    phd->linkage.back->fwd = phd->linkage.fwd;
    PhdPrevReturn = 0; 
    PhdNext = 0;
    FreeMem(phd, HEAP_HDATA);
    FreeMem(pCurThread,HEAP_THREAD);
    KCALLPROFOFF(36);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
FinishRemoveThread(
    RTs *pRT
    ) 
{
    RTs RTinfo;
    DWORD retval;
    LPTHREADTIME p1,p2;
    HANDLE hWake;
    SETCURKEY((DWORD)-1);
    DEBUGCHK (IsKernelVa (&RTinfo));
    memcpy(&RTinfo,pRT,sizeof(RTs));
    if (RTinfo.dwBase) {
        retval = VirtualFree((LPVOID)RTinfo.dwBase,RTinfo.dwLen,MEM_DECOMMIT);
        DEBUGCHK(retval);
        retval = VirtualFree((LPVOID)RTinfo.dwBase,0,MEM_RELEASE);
        DEBUGCHK(retval);
#if 0
        // we can't free the stack of the original thread since it might be the "fiber"
        // running on another thread. The original stack will only be freed when
        // (1) the original thread using original stack exit
        // (2) process exit.
        // 
        if (RTinfo.dwOrigBase != RTinfo.dwBase) {
            // we're on a fiber stack, free the original stack too
            retval = VirtualFree((LPVOID)RTinfo.dwOrigBase,RTinfo.dwLen,MEM_DECOMMIT);
            DEBUGCHK(retval);
            retval = VirtualFree((LPVOID)RTinfo.dwOrigBase,0,MEM_RELEASE);
            DEBUGCHK(retval);
        }
#endif        
    }
    if (RTinfo.hThread) {
        if (DecRef(RTinfo.hThread,RTinfo.pThread->pOwnerProc,0)) {
            if (IsCeLogEnabled()) {
                CELOG_ThreadDelete(RTinfo.hThread);
            }
            FreeHandle(RTinfo.hThread);
            EnterCriticalSection(&NameCS);
            p1 = NULL;
            p2 = TTList;
            while (p2 && p2->hTh != RTinfo.hThread) {
                p1 = p2;
                p2 = p2->pnext;
            }
            if (p2) {
                if (p1)
                    p1->pnext = p2->pnext;
                else
                    TTList = p2->pnext;
                FreeMem(p2,HEAP_THREADTIME);
            }
            LeaveCriticalSection(&NameCS);
        }
    }
    if (RTinfo.pThread)
        FreeMem(RTinfo.pThread,HEAP_THREAD);
    if (RTinfo.pThrdDbg)
        FreeMem(RTinfo.pThrdDbg,HEAP_THREADDBG);
    if (RTinfo.pProc) {

        
        EnterCriticalSection(&PageOutCS);
        DEBUGCHK(RTinfo.pProc != pCurProc);
        if (IsCeLogKernelEnabled()) {
            CELOG_ProcessTerminate(RTinfo.pProc->hProc);
        }
        if (DecRef(RTinfo.pProc->hProc,RTinfo.pProc,TRUE)) {
            if (IsCeLogEnabled()) {
                CELOG_ProcessDelete(RTinfo.pProc->hProc);
            }
            FreeHandle(RTinfo.pProc->hProc);
        }
        while (RTinfo.pProc->pProxList) {
            hWake = 0;
            KCall((PKFN)WakeOneThreadFlat,RTinfo.pProc,&hWake);
            if (hWake)
                KCall((PKFN)MakeRunIfNeeded,hWake);
        }
        if (RTinfo.pProc->pStdNames[0])
            FreeName(RTinfo.pProc->pStdNames[0]);
        if (RTinfo.pProc->pStdNames[1])
            FreeName(RTinfo.pProc->pStdNames[1]);
        if (RTinfo.pProc->pStdNames[2])
            FreeName(RTinfo.pProc->pStdNames[2]);
        if (pLogProcessDelete)
            pLogProcessDelete((DWORD)RTinfo.pProc);
        retval = RTinfo.pProc->dwVMBase;
        RTinfo.pProc->dwVMBase = 0;
        DeleteSection((LPVOID)retval);
        LeaveCriticalSection(&PageOutCS);
    } else {
        DEBUGCHK(*RTinfo.pdwDying);
        InterlockedDecrement(RTinfo.pdwDying);
    }
    KCall((PKFN)KillSpecialThread);
    DEBUGCHK(0);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
BlockWithHelper(
    FARPROC pFunc,
    FARPROC pHelpFunc,
    LPVOID param
    ) 
{
    CALLBACKINFO cbi;
    HANDLE hTh;
    if (pCurProc == &ProcArray[0]) {
        hTh = DoCreateThread(pCurThread->pSwapStack,HELPER_STACK_SIZE,pHelpFunc,param,CREATE_SUSPENDED|0x80000000,0,TH_KMODE,GET_CPRIO(pCurThread));
    } else {
        cbi.hProc = ProcArray[0].hProc;
        cbi.pfn = (FARPROC)DoCreateThread;
        cbi.pvArg0 = pCurThread->pSwapStack;
        hTh = (HANDLE)PerformCallBack(&cbi,HELPER_STACK_SIZE,pHelpFunc,param,CREATE_SUSPENDED|0x80000000,0,TH_KMODE,GET_CPRIO(pCurThread));
    }
    DEBUGCHK(hTh);
    *(PTHREAD *)param = HandleToThread(hTh);
    KCall((PKFN)pFunc,param);
    DEBUGCHK(0);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
BlockWithHelperAlloc(
    FARPROC pFunc,
    FARPROC pHelpFunc,
    LPVOID param
    ) 
{
    CALLBACKINFO cbi;
    HANDLE hTh;
    while (!(cbi.pvArg0 = (LPBYTE)GetHelperStack()))
        Sleep(500); // nothing we can do, so we'll just poll
    if (pCurProc == &ProcArray[0]) {
        hTh = DoCreateThread(cbi.pvArg0,HELPER_STACK_SIZE,pHelpFunc,param,CREATE_SUSPENDED|0x80000000,0,TH_KMODE,GET_CPRIO(pCurThread));
    } else {
        cbi.hProc = ProcArray[0].hProc;
        cbi.pfn = (FARPROC)DoCreateThread;
        hTh = (HANDLE)PerformCallBack(&cbi,HELPER_STACK_SIZE,pHelpFunc,param,CREATE_SUSPENDED|0x80000000,0,TH_KMODE,GET_CPRIO(pCurThread));
    }
    DEBUGCHK(hTh);
    *(PTHREAD *)param = HandleToThread(hTh);
    KCall((PKFN)pFunc,param);
    DEBUGCHK(0);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPCRIT 
DoLeaveSomething() 
{
    KCALLPROFON(67);
    if (pCurThread->pOwnedList)
        return pCurThread->pOwnedList;
    SET_BURIED(pCurThread);
    KCALLPROFOFF(67);
    return 0;
}

/* Terminate current thread routine */



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_NKTerminateThread(
    DWORD dwExitCode
    ) 
{
    int loop;
    LPCRIT pCrit;
    PCALLSTACK pcstk, pcnextstk;
    LPCLEANEVENT lpce;
    PTHREAD pdbg;
    HANDLE hClose, hWake;
    LPCRITICAL_SECTION lpcs;
    LPTHREADTIME lptt;
    LPPROXY pprox, nextprox;
    RTs RT;
    FILETIME ftExit;
    
    DEBUGMSG(ZONE_ENTRY,(L"SC_NKTerminateThread entry: %8.8lx\r\n",dwExitCode));
    SETCURKEY(0xffffffff);
    if (GET_DYING(pCurThread))
        dwExitCode = GetUserInfo(hCurThread);
    SET_DEAD(pCurThread);
    DEBUGCHK(!GET_BURIED(pCurThread));
    SetUserInfo(hCurThread,dwExitCode);
    if (!pCurThread->pNextInProc) {
        SetUserInfo(pCurThread->pOwnerProc->hProc,dwExitCode);
        CloseMappedFileHandles();
    }
#if defined(x86) || defined(SH4) || defined(MIPS_HAS_FPU)
    if (pCurThread == g_CurFPUOwner)
        g_CurFPUOwner = NULL;
#endif
#if defined(SH3)
    if (pCurThread == g_CurDSPOwner)
        g_CurDSPOwner = NULL;
#endif
    if (pCurThread == pOOMThread)
        pOOMThread = 0;
    EnterCriticalSection(&DbgApiCS);
    if (pCurThread->pThrdDbg && pCurThread->pThrdDbg->hFirstThrd) {
        for (loop = 0; loop < MAX_PROCESSES; loop++)
            if (ProcArray[loop].hDbgrThrd == hCurThread)
                ProcArray[loop].hDbgrThrd = 0;
        while (pCurThread->pThrdDbg->hFirstThrd) {
            pdbg = HandleToThread(pCurThread->pThrdDbg->hFirstThrd);
            DEBUGCHK (pdbg && pdbg->pThrdDbg);
            pCurThread->pThrdDbg->hFirstThrd = pdbg->pThrdDbg->hNextThrd;
            hClose = pdbg->pThrdDbg->hEvent;
            pdbg->pThrdDbg->hEvent = 0;
            SetHandleOwner(hClose,hCurProc);
            CloseHandle(hClose);
            hClose = pdbg->pThrdDbg->hBlockEvent;
            pdbg->pThrdDbg->hBlockEvent = 0;
            SetHandleOwner(hClose,hCurProc);
            SetEvent(hClose);
            CloseHandle(hClose);
        }
    }
    LeaveCriticalSection(&DbgApiCS);
    if (pCurThread->lpce) {
        lpce = pCurThread->lpce;
        pCurThread->lpce = 0;
        pprox = (LPPROXY)lpce->base;
        while (pprox) {
            nextprox = pprox->pThLinkNext;
            DequeueProxy(pprox);
            FreeMem(pprox,HEAP_PROXY);
            pprox = nextprox;
        }
        pprox = (LPPROXY)lpce->size;
        while (pprox) {
            nextprox = pprox->pThLinkNext;
            FreeMem(pprox,HEAP_PROXY);
            pprox = nextprox;
        }
        FreeMem(lpce,HEAP_CLEANEVENT);
    }
    DEBUGCHK(!pCurThread->lpProxy);
    while (pCurThread->pProxList) {
        hWake = 0;
        KCall((PKFN)WakeOneThreadFlat,pCurThread,&hWake);
        if (hWake)
            KCall((PKFN)MakeRunIfNeeded,hWake);
    }

    // get exit time before freeing DLLs because we need to call into coredll
    if (*(__int64 *)&pCurThread->ftCreate)
        GCFT (&ftExit);

    // free library before freeing callstack (we might need to call out to filesys to close a handle)
    if (!pCurThread->pNextInProc) {
        FreeAllProcLibraries(pCurThread->pOwnerProc);
    }

    for (pcstk = (PCALLSTACK)((DWORD)pCurThread->pcstkTop & ~1); pcstk; pcstk = pcnextstk) {
        pcnextstk = (PCALLSTACK)((DWORD)pcstk->pcstkNext & ~1);
        // Don't free on-stack ones
        if (IsValidKPtr(pcstk))
            FreeMem(pcstk,HEAP_CALLSTACK);
    }
    pCurThread->pcstkTop = 0;
    while (pCrit = (LPCRIT)KCall((PKFN)DoLeaveSomething)) {
        if (ISMUTEX(pCrit))
            DoLeaveMutex((LPMUTEX)pCrit);
        else {
            lpcs = pCrit->lpcs;
            lpcs->LockCount = 1;
            LeaveCriticalSection(lpcs);
        }
    }
    if (IsCeLogKernelEnabled()) {
        CELOG_ThreadTerminate(pCurThread->hTh);
    }
    if (pLogThreadDelete)
        pLogThreadDelete((DWORD)pCurThread,(DWORD)pCurThread->pOwnerProc);
    if (*(__int64 *)&pCurThread->ftCreate && (lptt = AllocMem(HEAP_THREADTIME))) {
        lptt->hTh = hCurThread;
        lptt->CreationTime = pCurThread->ftCreate;
        lptt->ExitTime = ftExit;
        *(__int64 *)&lptt->KernelTime = KCall((PKFN)GetCurThreadKTime);
        *(__int64 *)&lptt->KernelTime *= 10000;
        *(__int64 *)&lptt->UserTime = pCurThread->dwUserTime;
        *(__int64 *)&lptt->UserTime *= 10000;
        EnterCriticalSection(&NameCS);
        lptt->pnext = TTList;
        TTList = lptt;
        LeaveCriticalSection(&NameCS);
    }
    if (pCurThread->pNextInProc) {
        if (pCurThread->tlsSecure != pCurThread->tlsNonSecure) {
            // since we're going to be in the kernel from now on, the non-secure stack can be freed
            DWORD dwBase, dwSize;
            DEBUGCHK (pCurThread->tlsPtr == pCurThread->tlsSecure);
            dwBase = pCurThread->tlsNonSecure[PRETLS_STACKBASE];
            dwSize = KCURFIBER(pCurThread)? KPROCSTKSIZE(pCurThread) : pCurThread->dwOrigStkSize;
            pCurThread->tlsNonSecure = 0;       // can't reference it anymore
            DEBUGMSG (1, (L"Freeing non-secure stack at %8.8lx, size %8.8lx\n", dwBase, dwSize));
            if (!VirtualFree ((LPVOID)dwBase, dwSize, MEM_DECOMMIT))
                DEBUGCHK(0);
            if (!VirtualFree ((LPVOID)dwBase, 0, MEM_RELEASE))
                DEBUGCHK(0);

            // we're not freeing it here, but we will in FinishRemoveThread.
            InterlockedDecrement (&g_CntSecureStk);
            RT.dwLen = CNP_STACK_SIZE;  // secure stack is fix sized.
        } else {
            RT.dwLen = KCURFIBER(pCurThread)? KPROCSTKSIZE(pCurThread) : pCurThread->dwOrigStkSize;
        }
        RT.dwBase = KSTKBASE(pCurThread);
        RT.pProc = 0;
        RT.dwOrigBase = pCurThread->dwOrigBase;
    } else {
        if (IsSecureVa (KSTKBASE(pCurThread))) {
            RT.dwBase = KSTKBASE(pCurThread);
            RT.dwLen = CNP_STACK_SIZE;
        } else {
            RT.dwBase = 0;
            RT.dwLen = 0;
        }
        RT.pProc = pCurThread->pOwnerProc;
        RT.dwOrigBase = RT.dwBase;
        
        // free all pages it own in the paging pool
        FreeAllPagesFromQ (&RT.pProc->pgqueue);
        
    }
    RT.pThrdDbg = 0;
    RT.hThread = 0;
    RT.pThread = pCurThread;
    RT.pdwDying = &pCurThread->pOwnerProc->dwDyingThreads;
    InterlockedIncrement(RT.pdwDying);
    DEBUGCHK(*RT.pdwDying);

    DEBUGMSG (ZONE_ENTRY, (L"RT.dwBase = %8.8lx, RT.dwOrigBase = %8.8lx\r\n", RT.dwBase, RT.dwOrigBase));

    BlockWithHelper((FARPROC)RemoveThread,(FARPROC)FinishRemoveThread,(LPVOID)&RT);
    DEBUGCHK(0);
}

#ifdef KCALL_PROFILE



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
ProfileThreadSetContext(
    HANDLE hTh,
    const CONTEXT *lpContext
    ) 
{
    BOOL retval;
    KCALLPROFON(23);
    retval = DoThreadSetContext(hTh,lpContext);
    KCALLPROFOFF(23);
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
ProfileThreadGetContext(
    HANDLE hTh,
    LPCONTEXT lpContext
    ) 
{
    BOOL retval;
    KCALLPROFON(22);
    retval = DoThreadGetContext(hTh,lpContext);
    KCALLPROFOFF(22);
    return retval;
}

#define DoThreadSetContext ProfileThreadSetContext
#define DoThreadGetContext ProfileThreadGetContext

#endif



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ThreadSetContext(
    HANDLE hTh,
    const CONTEXT *lpContext
    ) 
{
    BOOL b;
    // use the SC_ version to check trustlevel
    if (!SC_LockPages((LPVOID)lpContext,sizeof(CONTEXT),0,LOCKFLAG_WRITE)) {
        return FALSE;
    }
    b = (BOOL)KCall((PKFN)DoThreadSetContext,hTh,lpContext);
    UnlockPages((LPVOID)lpContext,sizeof(CONTEXT));
    return b;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ThreadGetContext(
    HANDLE hTh,
    LPCONTEXT lpContext
    ) 
{
    BOOL b;
    // use the SC_ version to check trustlevel
    if (!SC_LockPages((LPVOID)lpContext,sizeof(CONTEXT),0,LOCKFLAG_WRITE)) {
        return FALSE;
    }
    b = (BOOL)KCall((PKFN)DoThreadGetContext,hTh,lpContext);
    UnlockPages((LPVOID)lpContext,sizeof(CONTEXT));
    return b;
}

#undef ExitProcess
#undef CreateThread

const char Hexmap[17] = "0123456789abcdef";

#define MAX_CALLSTACK   20
CallSnapshot cstks[MAX_CALLSTACK];
ULONG        nCstks;

DWORD JITGetCallStack (HANDLE hThrd, ULONG dwMaxFrames, CallSnapshot lpFrames[])
{
    // hThrd is ignored for current implementation. We'll make use of it when
    // we have the implementation for getting call stack for arbitrary threads.

    if (dwMaxFrames > nCstks) {
        dwMaxFrames = nCstks;
    }

    memcpy (lpFrames, cstks, dwMaxFrames * sizeof (CallSnapshot));
    return dwMaxFrames;
        
}


static void DbgGetExcpStack (PCONTEXT pctx)
{
	// we need use a local copy of the context to pass to NKGetCallStack since it'll overwrite the context while walking the stack.
    CONTEXT ctx = *pctx;
    extern ULONG NKGetCallStackSnapshot (ULONG dwMaxFrames, CallSnapshot lpFrames[], DWORD dwFlags, DWORD dwSkip, PCONTEXT pCtx);
    nCstks = NKGetCallStackSnapshot (MAX_CALLSTACK, cstks, 0, 0, &ctx);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
UserDbgTrap(
    EXCEPTION_RECORD *er,
    PCONTEXT pctx,
    BOOL bSecond
    ) 
{

    // get callstack of current running thread if second chance. Don't do it if it's stack overflow or we might fault recursively
    if ((bSecond || bDbgOnFirstChance)
        && (STATUS_BREAKPOINT != er->ExceptionCode)
        && (STATUS_STACK_OVERFLOW != er->ExceptionCode)) {
        DbgGetExcpStack (pctx);
    }


    // only launch JIT debugger on second chance exception
    if ((bSecond || bDbgOnFirstChance) && (er->ExceptionCode != STATUS_BREAKPOINT) && pDebugger && (!pCurThread->pThrdDbg || !pCurThread->pThrdDbg->hEvent)) {
        PROCESS_INFORMATION pi;
        DWORD ExitCode;
        WCHAR cmdline[11];
        cmdline[0] = '0';
        cmdline[1] = 'x';
        cmdline[2] = (WCHAR)Hexmap[((((DWORD)hCurProc)>>28)&0xf)];
        cmdline[3] = (WCHAR)Hexmap[((((DWORD)hCurProc)>>24)&0xf)];
        cmdline[4] = (WCHAR)Hexmap[((((DWORD)hCurProc)>>20)&0xf)];
        cmdline[5] = (WCHAR)Hexmap[((((DWORD)hCurProc)>>16)&0xf)];
        cmdline[6] = (WCHAR)Hexmap[((((DWORD)hCurProc)>>12)&0xf)];
        cmdline[7] = (WCHAR)Hexmap[((((DWORD)hCurProc)>> 8)&0xf)];
        cmdline[8] = (WCHAR)Hexmap[((((DWORD)hCurProc)>> 4)&0xf)];
        cmdline[9] = (WCHAR)Hexmap[((((DWORD)hCurProc)>> 0)&0xf)];
        cmdline[10] = 0;
        if (CreateProcess(pDebugger->name,cmdline,0,0,0,0,0,0,0,&pi)) {
            SET_NEEDDBG(pCurThread);
            CloseHandle(pi.hThread);
            while (SC_ProcGetCode(pi.hProcess,&ExitCode) && (ExitCode == STILL_ACTIVE) && (!pCurThread->pThrdDbg || !pCurThread->pThrdDbg->hEvent))
                Sleep(250);
            CloseHandle(pi.hProcess);
            while (pCurThread->bPendSusp)
                Sleep (250);
            CLEAR_NEEDDBG(pCurThread);
        }
    }
    if (pCurThread->pThrdDbg && ProcStarted(pCurProc) && pCurThread->pThrdDbg->hEvent) {
        DWORD dwPrio = SC_CeGetThreadPriority (pCurProc->hDbgrThrd);
        pCurThread->pThrdDbg->psavedctx = pctx;
        pCurThread->pThrdDbg->dbginfo.dwDebugEventCode = EXCEPTION_DEBUG_EVENT;
        pCurThread->pThrdDbg->dbginfo.dwProcessId = (DWORD)hCurProc;
        pCurThread->pThrdDbg->dbginfo.dwThreadId = (DWORD)hCurThread;
        pCurThread->pThrdDbg->dbginfo.u.Exception.dwFirstChance = !bSecond;
        pCurThread->pThrdDbg->dbginfo.u.Exception.ExceptionRecord = *er;

        // boost the priority of the debugger thread
        if (dwPrio >= pCurProc->bPrio) {
            // can't call SC_CeSetThreadPriority since the debugee might not be trusted
            KCall ((PKFN)SetThreadBasePrio, pCurProc->hDbgrThrd, pCurProc->bPrio? pCurProc->bPrio-1 : 0);
        }
        
        SetEvent(pCurThread->pThrdDbg->hEvent);
        SC_WaitForMultiple(1,&pCurThread->pThrdDbg->hBlockEvent,FALSE,INFINITE);

        // restore the priority of the debugger thread
        //  -- can't call SC_CeSetThreadPriority since the debugee might not be trusted
        KCall ((PKFN)SetThreadBasePrio, pCurProc->hDbgrThrd, dwPrio);
        
        pCurThread->pThrdDbg->psavedctx = 0;
        if (pCurThread->pThrdDbg->dbginfo.dwDebugEventCode == DBG_CONTINUE)
            return TRUE;
    }
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoDebugAttach(
    PPROCESS pproc
    ) 
{
    PTHREAD pTrav, pth;
    PMODULE pMod;
    DEBUGCHK(pproc);
    pth = pproc->pMainTh;
    DEBUGCHK(pth);
    pth->pThrdDbg->dbginfo.dwProcessId = (DWORD)pproc->hProc;
    pth->pThrdDbg->dbginfo.dwThreadId = (DWORD)pth->hTh;
    pth->pThrdDbg->dbginfo.dwDebugEventCode = CREATE_PROCESS_DEBUG_EVENT;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.hFile = NULL;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.hProcess = pproc->hProc;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.hThread = pth->hTh;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.lpStartAddress = (LPVOID)0;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.lpBaseOfImage = (LPVOID)pproc->dwVMBase;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.dwDebugInfoFileOffset = 0;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.nDebugInfoSize = 0;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.lpThreadLocalBase = pth->tlsPtr;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.lpImageName = pproc->lpszProcName;
    pth->pThrdDbg->dbginfo.u.CreateProcessInfo.fUnicode = 1;
    SetEvent(pth->pThrdDbg->hEvent);
    SC_WaitForMultiple(1,&pth->pThrdDbg->hBlockEvent,FALSE,INFINITE);
    for (pTrav = pproc->pTh; pTrav->pNextInProc; pTrav = pTrav->pNextInProc) {
        pTrav->pThrdDbg->dbginfo.dwProcessId = (DWORD)pproc->hProc;
        pTrav->pThrdDbg->dbginfo.dwThreadId = (DWORD)pTrav->hTh;
        pTrav->pThrdDbg->dbginfo.dwDebugEventCode = CREATE_THREAD_DEBUG_EVENT;
        pTrav->pThrdDbg->dbginfo.u.CreateThread.hThread = pTrav->hTh;
        pTrav->pThrdDbg->dbginfo.u.CreateThread.lpThreadLocalBase = pTrav->tlsPtr;
        pTrav->pThrdDbg->dbginfo.u.CreateThread.lpStartAddress = NULL;
        SetEvent(pTrav->pThrdDbg->hEvent);
        SC_WaitForMultiple(1,&pTrav->pThrdDbg->hBlockEvent,FALSE,INFINITE);
    }
    EnterCriticalSection(&ModListcs);
    pMod = pModList;
    while (pMod) {
        if (HasModRefProcPtr(pMod,pproc)) {
            LeaveCriticalSection(&ModListcs);
            pth->pThrdDbg->dbginfo.dwProcessId = (DWORD)pproc->hProc;
            pth->pThrdDbg->dbginfo.dwThreadId = (DWORD)pth->hTh;
            pth->pThrdDbg->dbginfo.dwDebugEventCode = LOAD_DLL_DEBUG_EVENT;
            pth->pThrdDbg->dbginfo.u.LoadDll.hFile = NULL;
            pth->pThrdDbg->dbginfo.u.LoadDll.lpBaseOfDll = (LPVOID)ZeroPtr(pMod->BasePtr);
            pth->pThrdDbg->dbginfo.u.LoadDll.dwDebugInfoFileOffset = 0;
            pth->pThrdDbg->dbginfo.u.LoadDll.nDebugInfoSize = 0;
            pth->pThrdDbg->dbginfo.u.LoadDll.lpImageName = pMod->lpszModName;
            pth->pThrdDbg->dbginfo.u.LoadDll.fUnicode = 1;
            SetEvent(pth->pThrdDbg->hEvent);
            SC_WaitForMultiple(1,&pth->pThrdDbg->hBlockEvent,FALSE,INFINITE);
            EnterCriticalSection(&ModListcs);
        }
        pMod = pMod->pMod;
    }
    LeaveCriticalSection(&ModListcs);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
WakeIfDebugWait(
    HANDLE hThrd
    ) 
{
    PTHREAD pth;
    KCALLPROFON(14);
    if ((pth = HandleToThread(hThrd)) && GET_DEBUGWAIT(pth) && (GET_RUNSTATE(pth) == RUNSTATE_BLOCKED)) {
        if (pth->lpProxy) {
            DEBUGCHK(pth->lpce);
            pth->lpce->base = pth->lpProxy;
            pth->lpce->size = (DWORD)pth->lpPendProxy;
            pth->lpProxy = 0;
            pth->bWaitState = WAITSTATE_SIGNALLED;
        }
        pth->wCount++;
        pth->retValue = WAIT_FAILED;
        SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
    } else
        hThrd = 0;
    KCALLPROFOFF(14);
    return hThrd;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_DebugNotify(
    DWORD dwFlags,
    DWORD data
    ) 
{
    PMODULE pMod;
    HANDLE hth;
    PTHREAD pth;
    ACCESSKEY ulOldKey;

    if (pCurThread->pThrdDbg && ProcStarted(pCurProc) && pCurThread->pThrdDbg->hEvent) {
        pCurThread->pThrdDbg->dbginfo.dwProcessId = (DWORD)hCurProc;
        pCurThread->pThrdDbg->dbginfo.dwThreadId = (DWORD)hCurThread;
        switch (dwFlags) {
            case DLL_PROCESS_ATTACH:
                pCurThread->pThrdDbg->dbginfo.dwDebugEventCode = CREATE_PROCESS_DEBUG_EVENT;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.hFile = NULL;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.hProcess = hCurProc;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.hThread = hCurThread;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.lpStartAddress = (LPTHREAD_START_ROUTINE)data;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.lpBaseOfImage = (LPVOID)pCurProc->dwVMBase;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.dwDebugInfoFileOffset = 0;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.nDebugInfoSize = 0;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.lpThreadLocalBase = pCurThread->tlsPtr;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.lpImageName = pCurProc->lpszProcName;
                pCurThread->pThrdDbg->dbginfo.u.CreateProcessInfo.fUnicode = 1;
                break;
            case DLL_PROCESS_DETACH:
                pCurThread->pThrdDbg->dbginfo.dwDebugEventCode = EXIT_PROCESS_DEBUG_EVENT;
                pCurThread->pThrdDbg->dbginfo.u.ExitProcess.dwExitCode = GET_DYING(pCurThread) ? GetUserInfo(hCurThread) : data;
                break;
            case DLL_THREAD_ATTACH:
                pCurThread->pThrdDbg->dbginfo.dwDebugEventCode = CREATE_THREAD_DEBUG_EVENT;
                pCurThread->pThrdDbg->dbginfo.u.CreateThread.hThread = hCurThread;
                pCurThread->pThrdDbg->dbginfo.u.CreateThread.lpThreadLocalBase = pCurThread->tlsPtr;
                pCurThread->pThrdDbg->dbginfo.u.CreateThread.lpStartAddress = (LPTHREAD_START_ROUTINE)data;
                break;
            case DLL_THREAD_DETACH:
                pCurThread->pThrdDbg->dbginfo.dwDebugEventCode = EXIT_THREAD_DEBUG_EVENT;
                pCurThread->pThrdDbg->dbginfo.u.ExitThread.dwExitCode = GET_DYING(pCurThread) ? GetUserInfo(hCurThread) : data;
                break;
        }
        SWITCHKEY(ulOldKey,0xffffffff);
        if (hth = (HANDLE)KCall((PKFN)WakeIfDebugWait,pCurProc->hDbgrThrd))
            KCall((PKFN)MakeRunIfNeeded,hth);
        SETCURKEY(ulOldKey);
        SetEvent(pCurThread->pThrdDbg->hEvent);
        SC_WaitForMultiple(1,&pCurThread->pThrdDbg->hBlockEvent,FALSE,INFINITE);
        if ((dwFlags == DLL_THREAD_DETACH) && (pth = HandleToThread(pCurThread->pOwnerProc->hDbgrThrd)))
            DecRef(hCurThread,pth->pOwnerProc,0);
        if (dwFlags == DLL_PROCESS_ATTACH) {
            EnterCriticalSection(&ModListcs);
            pMod = pModList;
            while (pMod) {
                if (HasModRefProcPtr(pMod,pCurProc)) {
                    LeaveCriticalSection(&ModListcs);
                    pCurThread->pThrdDbg->dbginfo.dwProcessId = (DWORD)hCurProc;
                    pCurThread->pThrdDbg->dbginfo.dwThreadId = (DWORD)hCurThread;
                    pCurThread->pThrdDbg->dbginfo.dwDebugEventCode = LOAD_DLL_DEBUG_EVENT;
                    pCurThread->pThrdDbg->dbginfo.u.LoadDll.hFile = NULL;
                    pCurThread->pThrdDbg->dbginfo.u.LoadDll.lpBaseOfDll = (LPVOID)ZeroPtr(pMod->BasePtr);
                    pCurThread->pThrdDbg->dbginfo.u.LoadDll.dwDebugInfoFileOffset = 0;
                    pCurThread->pThrdDbg->dbginfo.u.LoadDll.nDebugInfoSize = 0;
                    pCurThread->pThrdDbg->dbginfo.u.LoadDll.lpImageName = pMod->lpszModName;
                    pCurThread->pThrdDbg->dbginfo.u.LoadDll.fUnicode = 1;
                    SetEvent(pCurThread->pThrdDbg->hEvent);
                    SC_WaitForMultiple(1,&pCurThread->pThrdDbg->hBlockEvent,FALSE,INFINITE);
                    EnterCriticalSection(&ModListcs);
                }
                pMod = pMod->pMod;
            }
            LeaveCriticalSection(&ModListcs);
        }
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_WaitForDebugEvent(
    LPDEBUG_EVENT lpDebugEvent,
    DWORD dwMilliseconds
    ) 
{
    HANDLE hWaits[MAXIMUM_WAIT_OBJECTS];
    HANDLE hThrds[MAXIMUM_WAIT_OBJECTS];
    DWORD ret;
    DWORD count;
    HANDLE hth;
    PTHREAD pth;

    TRUSTED_API (L"SC_WaitForDebugEvent", FALSE);
    do {
        count = 0;
        EnterCriticalSection(&DbgApiCS);
        if (pCurThread->pThrdDbg) {
            hth = pCurThread->pThrdDbg->hFirstThrd;
            while (hth && (count < MAXIMUM_WAIT_OBJECTS)) {
                pth = HandleToThread(hth);
                if (!(pth && pth->pThrdDbg)) {
                    // this could happen when a thread is in the middle of CreateNewProc.
                    // fail the call with GLE == ERROR_NOT_READY.
                    LeaveCriticalSection(&DbgApiCS);
                    KSetLastError (pCurThread, ERROR_NOT_READY);
                    return FALSE;
                }
                hThrds[count] = hth;
                hWaits[count++] = pth->pThrdDbg->hEvent;
                hth = pth->pThrdDbg->hNextThrd;
            }
        }
        LeaveCriticalSection(&DbgApiCS);
        if (!count) {
            KSetLastError (pCurThread, ERROR_OUTOFMEMORY);
            return FALSE;
        }
        SET_USERBLOCK(pCurThread);
        SET_DEBUGWAIT(pCurThread);
        ret = SC_WaitForMultiple(count,hWaits,FALSE,dwMilliseconds);
        CLEAR_DEBUGWAIT(pCurThread);
        CLEAR_USERBLOCK(pCurThread);
        if (ret == WAIT_TIMEOUT) {
            KSetLastError (pCurThread, ret);
            return FALSE;
        }
        if (ret == WAIT_FAILED)
            lpDebugEvent->dwDebugEventCode = 0;
        else {
            pth = HandleToThread(hThrds[ret-WAIT_OBJECT_0]);
            EnterCriticalSection(&DbgApiCS);
            if (!pth || !pth->pThrdDbg || !pth->pThrdDbg->hEvent)
                lpDebugEvent->dwDebugEventCode = 0;
            else {
                *lpDebugEvent = pth->pThrdDbg->dbginfo;
                pth->pThrdDbg->bDispatched = 1;
            }
            LeaveCriticalSection(&DbgApiCS);
        }
    } while (!lpDebugEvent->dwDebugEventCode);
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ContinueDebugEvent(
    DWORD dwProcessId,
    DWORD dwThreadId,
    DWORD dwContinueStatus
    ) 
{
    PTHREAD pth;
    ACCESSKEY ulOldKey;
    TRUSTED_API (L"SC_ContinueDebugEvent", FALSE);
    pth = HandleToThread((HANDLE)dwThreadId);
    if (!pth || !pth->pThrdDbg || !pth->pThrdDbg->hEvent || !pth->pThrdDbg->bDispatched) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    pth->pThrdDbg->dbginfo.dwDebugEventCode = dwContinueStatus;
    pth->pThrdDbg->bDispatched = 0;
    SWITCHKEY(ulOldKey,0xffffffff);
    SetEvent(pth->pThrdDbg->hBlockEvent);
    SETCURKEY(ulOldKey);
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
StopProc(
    HANDLE hProc,
    LPTHRDDBG pdbg
    ) 
{
    DWORD retval, res;
    PPROCESS pproc;
    PTHREAD pth;
    KCALLPROFON(27);
    if (!(pproc = HandleToProc(hProc))) {
        KCALLPROFOFF(27);
        return 2;
    }
    pth = pproc->pMainTh;
    res = ThreadSuspend (pth, TRUE);
    if (res == 0xfffffffe) {
        KCALLPROFOFF(27);
        return 0;
    }
    if (res == 0xffffffff) {
        KCALLPROFOFF(27);
        return 2;
    }
    SET_DEBUGBLK(pth);
    IncRef(pth->hTh,pCurProc);
    if (!pth->pThrdDbg) {
        pth->pThrdDbg = pdbg;
        retval = 1;
    } else {
        DEBUGCHK(!pth->pThrdDbg->hEvent);
        pth->pThrdDbg->hEvent = pdbg->hEvent;
        pth->pThrdDbg->hBlockEvent = pdbg->hBlockEvent;
        retval = 3;
    }
    pth->pThrdDbg->hNextThrd = pCurThread->pThrdDbg->hFirstThrd;
    pCurThread->pThrdDbg->hFirstThrd = pth->hTh;
    KCALLPROFOFF(27);
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
StopProc2(
    PPROCESS pproc,
    LPTHRDDBG pdbg,
    PTHREAD *ppth
    ) 
{
    DWORD retval;
    PTHREAD pth;
    KCALLPROFON(25);
    pth = *ppth;
    retval = 2;
    if (!GET_DEBUGBLK(pth)) {
        if (ThreadSuspend(pth, TRUE) == 0xfffffffe) {
            KCALLPROFOFF(25);
            return 0;
        }
        SET_DEBUGBLK(pth);
        IncRef(pth->hTh,pCurProc);
        if (!pth->pThrdDbg) {
            pth->pThrdDbg = pdbg;
            retval = 1;
        } else {
            DEBUGCHK(!pth->pThrdDbg->hEvent);
            pth->pThrdDbg->hEvent = pdbg->hEvent;
            pth->pThrdDbg->hBlockEvent = pdbg->hBlockEvent;
            retval = 3;
        }
        pth->pThrdDbg->hNextThrd = pCurThread->pThrdDbg->hFirstThrd;
        pCurThread->pThrdDbg->hFirstThrd = pth->hTh;
    }
    *ppth = pth->pNextInProc;
    KCALLPROFOFF(25);
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DAPProc(
    PPROCESS pproc
    ) 
{
    PTHREAD pth, pth2;
    SETCURKEY(0xffffffff);
    DoDebugAttach(pproc);
    for (pth = pproc->pTh; pth; pth = pth2) {
        pth2 = pth->pNextInProc;
        CLEAR_DEBUGBLK(pth);        
        KCall((PKFN)ThreadResume,pth);
    }
    SET_DYING(pCurThread);
    SET_DEAD(pCurThread);
    NKTerminateThread(0);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ProcDebug(
    DWORD dwProcessId
    ) 
{
    HANDLE hth;
    PTHREAD pth;
    DWORD res;
    PPROCESS pproc;
    LPTHRDDBG pdbg;
    ACCESSKEY ulOldKey;

    TRUSTED_API (L"SC_ProcDebug", FALSE);

    pproc = HandleToProc((HANDLE)dwProcessId);
    if (!pproc || pproc->DbgActive)
        return FALSE;

    // testing fNoDebug and setting DbgActive need to be protected by LLcs to
    // prevent race condition with the 'ChkDebug' function
    EnterCriticalSection (&LLcs);
    if (pproc->fNoDebug) {
        ERRORMSG(1,(L"SC_ProcDebug failed because the process can't be debugged\r\n"));
        KSetLastError(pCurThread,ERROR_ACCESS_DENIED);
        LeaveCriticalSection (&LLcs);
        return FALSE;
    }
    pproc->DbgActive = (BYTE)MAX_PROCESSES + (BYTE)pCurProc->procnum;
    LeaveCriticalSection (&LLcs);
    
    if (!pCurThread->pThrdDbg) {
        if (!(pdbg = (LPTHRDDBG)AllocMem(HEAP_THREADDBG))) {
            pproc->DbgActive = 0;
            return FALSE;
        }
        pdbg->hFirstThrd = 0;
        pdbg->hEvent = 0;
        pdbg->psavedctx = 0;
        pdbg->bDispatched = 0;
        pdbg->dbginfo.dwDebugEventCode = 0;
        if (InterlockedTestExchange((LPDWORD)&pCurThread->pThrdDbg,0,(DWORD)pdbg))
            FreeMem(pdbg,HEAP_THREADDBG);
    }
    if (!(pdbg = AllocDbg((HANDLE)dwProcessId))) {
        pproc->DbgActive = 0;
        return FALSE;
    }
    SC_CeSetThreadPriority (pCurThread, pproc->bPrio? pproc->bPrio - 1 : 0);
    EnterCriticalSection(&DbgApiCS);
    while (!(res = KCall((PKFN)StopProc,dwProcessId,pdbg)))
        Sleep(10);
    if (res == 2) {
        pproc->DbgActive = 0;
        LeaveCriticalSection(&DbgApiCS);
        return FALSE;
    }
    if (res == 3)
        FreeMem(pdbg,HEAP_THREADDBG);
    pth = pproc->pTh;
    res = 1;
    while (pth && pth->pNextInProc) {
        if (res && (res != 2) && !(pdbg = AllocDbg((HANDLE)dwProcessId))) {
            
            pproc->DbgActive = 0;
            LeaveCriticalSection(&DbgApiCS);
            return FALSE;
        }
        if (!(res = KCall((PKFN)StopProc2,pproc,pdbg,&pth))) {
            LeaveCriticalSection(&DbgApiCS);
            Sleep(10);
            pth = pproc->pTh;
            EnterCriticalSection(&DbgApiCS);
        }
        if (res == 3)
            FreeMem(pdbg,HEAP_THREADDBG);
    }
    LeaveCriticalSection(&DbgApiCS);
    if (res == 2) {
       SWITCHKEY(ulOldKey,0xffffffff);
        SetHandleOwner(pdbg->hEvent,hCurProc);
        SetHandleOwner(pdbg->hBlockEvent,hCurProc);
        SETCURKEY(ulOldKey);
        CloseHandle(pdbg->hEvent);
        CloseHandle(pdbg->hBlockEvent);
        FreeMem(pdbg,HEAP_THREADDBG);
    }
    pproc->hDbgrThrd = hCurThread;
    pproc->bChainDebug = 0;
    hth = CreateKernelThread((LPTHREAD_START_ROUTINE)DAPProc,pproc,(WORD) (pproc->bPrio? pproc->bPrio-1 : 0), 0x40000000);
    if (!hth) {
        
        pproc->DbgActive = 0;
        pproc->hDbgrThrd = 0;
        return FALSE;
    }
    CloseHandle(hth);
    return TRUE;
}

extern BOOL CheckLastRef(HANDLE hTh);



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ThreadCloseHandle(
    HANDLE hTh
    ) 
{
    LPTHREADTIME p1,p2;
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadCloseHandle entry: %8.8lx\r\n",hTh));
    if (IsCeLogEnabled()) {
        CELOG_ThreadCloseHandle(hTh);
    }
    if (!KCall((PKFN)CheckLastRef,hTh)) {
        KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
        DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadCloseHandle exit: %8.8lx\r\n", FALSE));
        return FALSE;
    }
    if (DecRef(hTh,pCurProc,0)) {
        DEBUGCHK(!HandleToThread(hTh));
        FreeHandle(hTh);
        EnterCriticalSection(&NameCS);
        p1 = NULL;
        p2 = TTList;
        while (p2 && p2->hTh != hTh) {
            p1 = p2;
            p2 = p2->pnext;
        }
        if (p2) {
            if (p1)
                p1->pnext = p2->pnext;
            else
                TTList = p2->pnext;
            FreeMem(p2,HEAP_THREADTIME);
        }
        LeaveCriticalSection(&NameCS);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_ThreadCloseHandle exit: %8.8lx\r\n", TRUE));
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_ProcCloseHandle(
    HANDLE hProc
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcCloseHandle entry: %8.8lx\r\n",hProc));
    if (IsCeLogEnabled()) {
        CELOG_ProcessCloseHandle(hProc);
    }
    if (DecRef(hProc,pCurProc,0)) {
#ifdef DEBUG
        PPROCESS pprc;
        DEBUGCHK(!(pprc = HandleToProc(hProc)) || !pprc->pMainTh);
#endif
        if (IsCeLogEnabled()) {
            CELOG_ProcessDelete(hProc);
        }
        FreeHandle(hProc);
    }
    DEBUGMSG(ZONE_ENTRY,(L"SC_ProcCloseHandle exit: %8.8lx\r\n", TRUE));
    return TRUE;
}

/* Creates a new process */

#define VALID_CREATE_PROCESS_FLAGS (CREATE_NEW_CONSOLE|DEBUG_ONLY_THIS_PROCESS|DEBUG_PROCESS|CREATE_SUSPENDED|INHERIT_CALLER_PRIORITY|0x80000000)

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_CreateProc(
    LPCWSTR lpszImageName,
    LPCWSTR lpszCommandLine,
    LPSECURITY_ATTRIBUTES lpsaProcess,
    LPSECURITY_ATTRIBUTES lpsaThread,
    BOOL fInheritHandles,
    DWORD fdwCreate,
    LPVOID lpvEnvironment,
    LPWSTR lpszCurDir,
    LPSTARTUPINFO lpsiStartInfo,
    LPPROCESS_INFORMATION lppiProcInfo
    ) 
{
    LPTHRDDBG pdbg;
    ProcStartInfo psi;
    PROCESS_INFORMATION procinfo;
    LPBYTE lpStack = 0;
    LPBYTE pSwapStack = 0;
    LPVOID lpvSection = 0;
    PPROCESS pNewproc = 0;
    PTHREAD pNewth = 0;
    HANDLE hNewproc = 0, hNewth = 0;
    int loop;
    int length;
    LPWSTR uptr, procname;
    extern void KUnicodeToAscii(LPCHAR chptr, LPCWSTR wchptr, int maxlen);
    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateProc entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        lpszImageName, lpszCommandLine, lpsaProcess, lpsaThread, fInheritHandles, fdwCreate, lpvEnvironment,
        lpszCurDir, lpsiStartInfo, lppiProcInfo));

    // verify parameter
    if ((KERN_TRUST_FULL != pCurProc->bTrustLevel) 
        && lppiProcInfo
        && !SC_MapPtrWithSize (lppiProcInfo, sizeof (PROCESS_INFORMATION), hCurProc)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    psi.lpszImageName = MapPtr((LPWSTR)lpszImageName);
    psi.lpszCmdLine = MapPtr((LPWSTR)(lpszCommandLine ? lpszCommandLine : TEXT("")));
    psi.lppi = MapPtr(lppiProcInfo ? lppiProcInfo : &procinfo);
    psi.lppi->hProcess = 0;
    psi.he = 0;
    if (!lpszImageName || fInheritHandles || (fdwCreate & ~VALID_CREATE_PROCESS_FLAGS) ||
        lpvEnvironment || lpszCurDir || (strlenW(lpszImageName) >= MAX_PATH)) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        goto exitcpw;
    }
    if (pCurProc->bTrustLevel != KERN_TRUST_FULL)
        fdwCreate &= ~(DEBUG_ONLY_THIS_PROCESS|DEBUG_PROCESS);
    psi.fdwCreate = fdwCreate;
    if (!MTBFf || 
        !(lpStack = VirtualAlloc((LPVOID)ProcArray[0].dwVMBase,CNP_STACK_SIZE,MEM_RESERVE|MEM_AUTO_COMMIT,PAGE_NOACCESS)) ||
        !VirtualAlloc((LPVOID)((DWORD)lpStack+CNP_STACK_SIZE-MIN_STACK_SIZE),MIN_STACK_SIZE,MEM_COMMIT,PAGE_READWRITE) ||
        !(psi.he = CreateEvent(0,1,0,0)) ||
        !(lpvSection = CreateSection(0, TRUE))) {
        KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
        goto exitcpw;
    }
    MDInitSecureStack(lpStack);
    loop = ((DWORD)lpvSection>>VA_SECTION)-1;
    pNewproc = &ProcArray[loop];
    pNewproc->pStdNames[0] = pNewproc->pStdNames[1] = pNewproc->pStdNames[2] = 0;
    pNewth = (PTHREAD)AllocMem(HEAP_THREAD);
    if (!pNewth) {
        KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
        goto exitcpw;
    }
    if (!(hNewth = AllocHandle(&cinfThread,pNewth,pCurProc))) {
        KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
        goto exitcpw;
    }
    if (!(hNewproc = AllocHandle(&cinfProc,pNewproc,pCurProc))) {
        KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
        goto exitcpw;
    }
    if (!(pSwapStack = GetHelperStack()))
        goto exitcpw;
    SetUserInfo(hNewproc,STILL_ACTIVE);
    pNewproc->procnum = loop;
    pNewproc->pcmdline = &L"";
    pNewproc->lpszProcName = &L"";
    pNewproc->DbgActive = 0;
    pNewproc->hProc = hNewproc;
    pNewproc->dwVMBase = (DWORD)lpvSection;
    pNewproc->pTh = pNewproc->pMainTh = pNewth;
    pNewproc->aky = 1 << pNewproc->procnum;
    pNewproc->bChainDebug = 0;
    pNewproc->bTrustLevel = KERN_TRUST_FULL;
    if (fdwCreate & (DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS)) {
        EnterCriticalSection(&DbgApiCS);
        pNewproc->hDbgrThrd = hCurThread;
        if (!(fdwCreate & DEBUG_ONLY_THIS_PROCESS))
            pNewproc->bChainDebug = 1;
        if (!pCurThread->pThrdDbg) {
            if (!(pdbg = (LPTHRDDBG)AllocMem(HEAP_THREADDBG))) {
                LeaveCriticalSection(&DbgApiCS);
                KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
                goto exitcpw;
            }
            pdbg->hFirstThrd = 0;
            pdbg->hEvent = 0;
            pdbg->psavedctx = 0;
            pdbg->bDispatched = 0;
            pdbg->dbginfo.dwDebugEventCode = 0;
            if (InterlockedTestExchange((LPDWORD)&pCurThread->pThrdDbg,0,(DWORD)pdbg))
                FreeMem(pdbg,HEAP_THREADDBG);
        }
        LeaveCriticalSection(&DbgApiCS);
    } else if (pCurProc->hDbgrThrd && pCurProc->bChainDebug) {
        pNewproc->hDbgrThrd = pCurProc->hDbgrThrd;
        pNewproc->bChainDebug = 1;
    }
    else
        pNewproc->hDbgrThrd = 0;
    pNewproc->tlsLowUsed = TLSSLOT_RESERVE;
    pNewproc->tlsHighUsed = 0;
    pNewproc->pfnEH = 0;
    pNewproc->pExtPdata = 0;
    pNewproc->bPrio = THREAD_RT_PRIORITY_NORMAL;
    pNewproc->fNoDebug = FALSE;
    pNewproc->ZonePtr = 0;
    pNewproc->pProxList = 0;
    pNewproc->dwDyingThreads = 0;
    pNewproc->pmodResource = 0;
    pNewproc->oe.filetype = 0;
    pNewproc->pgqueue.idxHead = INVALID_PG_INDEX;
    if (!(fdwCreate & CREATE_NEW_CONSOLE)) {
        for (loop = 0; loop < 3; loop++) {
            if (!pCurProc->pStdNames[loop])
                pNewproc->pStdNames[loop] = 0;
            else {
                if (!(pNewproc->pStdNames[loop] = AllocName((strlenW(pCurProc->pStdNames[loop]->name)+1)*2))) {
                    KSetLastError(pCurThread,ERROR_OUTOFMEMORY);
                    goto exitcpw;
                }
                kstrcpyW(pNewproc->pStdNames[loop]->name,pCurProc->pStdNames[loop]->name);
            }
        }
    }
    uptr = procname = psi.lpszImageName;
    while (*uptr)
        if (*uptr++ == (WCHAR)'\\')
            procname = uptr;
    length = (uptr - procname + 1)*sizeof(WCHAR);
    uptr = MapPtr((LPBYTE)((DWORD)lpStack + CNP_STACK_SIZE - ((length+7) & ~7)));
    memcpy(uptr, procname, length);
    pNewproc->lpszProcName = uptr;

	InitThreadStruct(pNewth,hNewth,pNewproc,hNewproc,(WORD) ((fdwCreate & INHERIT_CALLER_PRIORITY)? GET_BPRIO(pCurThread) : THREAD_RT_PRIORITY_NORMAL));

    // initialize the area for CoProc registers if required
    if (cbNKCoProcRegSize && pSwapStack && pOEMInitCoProcRegisterSavedArea) {
        pOEMInitCoProcRegisterSavedArea (pSwapStack);
    }
    pNewth->pSwapStack = pSwapStack;
    pNewth->pNextInProc = pNewth->pPrevInProc = 0;
    AddAccess(&pNewth->aky,pCurThread->aky);
    GCFT(&pNewth->ftCreate);
    MDCreateThread(pNewth,lpStack,CNP_STACK_SIZE,(LPVOID)CreateNewProc,0,TH_KMODE,(ulong)&psi);
    pNewth->dwOrigBase = (DWORD) lpStack;
    pNewth->dwOrigStkSize = CNP_STACK_SIZE;
    pNewth->tlsSecure = pNewth->tlsNonSecure = pNewth->tlsPtr;
    ZeroTLS(pNewth);
    IncRef(hNewproc,pNewproc);
    IncRef(hNewth,pNewproc);
    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateProc switching to loader on thread %8.8lx\r\n",pNewth));
    
    // Save off the thread's program counter for getting its name later.
    pNewth->dwStartAddr = (DWORD) CreateNewProc;
   
    // Null out the tocptr because it hasn't been set yet. (It will be fixed when
    // the thread runs through CreateNewProc, running OpenExe in the process.)
    // This is necessary because we look into the TOC for the process' name 
    // when getting the name of the primary thread of a process.
    pNewth->pOwnerProc->oe.tocptr = 0;
    pNewth->pOwnerProc->oe.filetype = 0;
   
    // The process name was overwritten when we ran ZeroTLS on the thread!
    // Set the process name now because we will need it when we are logging the
    // thread creation.  The process struct will be corrected when our new thread
    // begins running through CreateNewProc.
    pNewproc->lpszProcName = procname;
   
    if (IsCeLogKernelEnabled()) {
        CELOG_ProcessCreate(hNewproc, procname, (DWORD)lpvSection);
        CELOG_ThreadCreate(pNewth, hNewproc, procname, 0);
    }
    if (pLogProcessCreate)
        pLogProcessCreate((DWORD)pNewproc);
    if (pLogThreadCreate)
        pLogThreadCreate((DWORD)pNewth,(DWORD)pNewproc);
    
    SET_RUNSTATE(pNewth,RUNSTATE_NEEDSRUN);
    KCall((PKFN)MakeRunIfNeeded,hNewth);
    WaitForMultipleObjects(1,&psi.he,FALSE,INFINITE);
    if (!psi.lppi->hProcess) {
        CloseHandle(hNewproc);
        CloseHandle(hNewth);
        KSetLastError(pCurThread,psi.lppi->dwThreadId);
    } else if ((psi.lppi == MapPtr(&procinfo) && !(fdwCreate & (DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS)))) {
        CloseHandle(hNewproc);
        CloseHandle(hNewth);
    }
    goto done;
exitcpw:
    if (pNewproc) {
        for (loop = 0; loop < 3; loop++)
            if (pNewproc->pStdNames[loop])
                FreeName(pNewproc->pStdNames[loop]);
    }
    if (pNewth)
        FreeMem(pNewth,HEAP_THREAD);
    if (hNewth)
        FreeHandle(hNewth);
    if (hNewproc)
        FreeHandle(hNewproc);
    if (lpvSection)
        DeleteSection(lpvSection);
    if (psi.he) {
        CloseHandle(psi.he);
        psi.he = NULL;
    }
    if (lpStack) {
        VirtualFree(lpStack,CNP_STACK_SIZE,MEM_DECOMMIT);
        VirtualFree(lpStack,0,MEM_RELEASE);
    }
    if (pSwapStack)
        FreeHelperStack(pSwapStack);
done:
    if (psi.he)
        CloseHandle(psi.he);
    DEBUGMSG(ZONE_ENTRY,(L"SC_CreateProc exit: %8.8lx\r\n", (psi.lppi->hProcess ? TRUE : FALSE)));
    return (psi.lppi->hProcess ? TRUE : FALSE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
sub64_64_64(
    const FILETIME *lpnum1,
    LPFILETIME lpnum2,
    LPFILETIME lpres
    ) 
{
    LARGE_INTEGER li1, li2;
    li1.LowPart  = lpnum1->dwLowDateTime;
    li1.HighPart = lpnum1->dwHighDateTime;
    li2.LowPart  = lpnum2->dwLowDateTime;
    li2.HighPart = lpnum2->dwHighDateTime;

    li1.QuadPart -= li2.QuadPart;
    lpres->dwHighDateTime = li1.HighPart;
    lpres->dwLowDateTime = li1.LowPart;
}

void UpdateAndSignalAlarm (void)
{
    DEBUGCHK (rtccs.OwnerThread == hCurThread);
    if (hAlarmEvent) {
        SYSTEMTIME st;
        FILETIME ft,ft2,ft3;
        OEMGetRealTime(&st);
        KSystemTimeToFileTime(&st,&ft);
        KSystemTimeToFileTime(&CurAlarmTime,&ft2);
        if (dwNKAlarmResolutionMSec < MIN_NKALARMRESOLUTION_MSEC)
            dwNKAlarmResolutionMSec = MIN_NKALARMRESOLUTION_MSEC;
        else if (dwNKAlarmResolutionMSec > MAX_NKALARMRESOLUTION_MSEC)
            dwNKAlarmResolutionMSec = MAX_NKALARMRESOLUTION_MSEC;
        ft3.dwLowDateTime = dwNKAlarmResolutionMSec * 10000;        // convert to 100 ns units
        ft3.dwHighDateTime = 0;
        sub64_64_64(&ft2,&ft3,&ft3);
        if (KCompareFileTime(&ft,&ft3) >= 0) {
            SetEvent(hAlarmEvent);
            hAlarmEvent = NULL;
        } else
            OEMSetAlarmTime(&CurAlarmTime);
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SystemStartupFunc(
    ulong param
    ) 
{
    DWORD first = 0xffffffff, last = 0;
    ROMChain_t *pROM;
    TOCentry *tocptr;
    e32_rom  *eptr;
    int     loop;
    HANDLE hTh;
#if x86
    LPVOID pEmul;
#endif
    KernelInit2();

    SharedDllBase = MODULE_SECTION_END; // the first address of ROM DLL code

    for (pROM = ROMChain; pROM; pROM = pROM->pNext) {

        // find the base address of ROM DLL's code
        tocptr = (TOCentry *)(pROM->pTOC+1);  // toc entries follow the header
        for (loop = 0; loop < (int)pROM->pTOC->nummods; loop++, tocptr ++) {
            eptr = (e32_rom *)(tocptr->ulE32Offset);
            DEBUGMSG (ZONE_LOADER1, (L"eptr = %8.8lx, vbase = %8.8lx\n", eptr, eptr->e32_vbase));
            if (IsModCodeAddr (eptr->e32_vbase) && (SharedDllBase > eptr->e32_vbase)) {
                SharedDllBase = eptr->e32_vbase;
            }
        }

        // find first and last of ROM DLL's RW data
        if (pROM->pTOC->dllfirst != pROM->pTOC->dlllast) {
            if (first > pROM->pTOC->dllfirst)
                first = pROM->pTOC->dllfirst;
            if (last < pROM->pTOC->dlllast)
                last = pROM->pTOC->dlllast;
        }
    }

    first &= 0xffff0000;    // 64K alignment

    DEBUGMSG (1, (L"first = %8.8lx, last = %8.8lx, SharedDllBase = %8.8lx\n", first, last, SharedDllBase));

    if (MODULE_SECTION_END != SharedDllBase) {

        // reserve the code area of the modules
        SharedDllBase = (DWORD) VirtualAlloc ((LPVOID) SharedDllBase, MODULE_SECTION_END - SharedDllBase, MEM_RESERVE|MEM_IMAGE, PAGE_NOACCESS);
        DEBUGCHK (SharedDllBase);
        
    }

    ROMDllLoadBase = DllLoadBase = first;

    if ((first != last) 
        && !VirtualAlloc ((LPVOID) MapPtrProc (first, ProcArray), last - first, MEM_RESERVE|MEM_IMAGE, PAGE_NOACCESS)) {
        RETAILMSG (1, (L"Failed to Reserve ROM DLL space!, SPIN FOREVER\r\n"));
        INTERRUPTS_OFF ();
        while (1)
            ;
    }
    hCoreDll = LoadOneLibraryW(L"coredll.dll",LLIB_NO_PAGING,0);
    pExitThread = GetProcAddressA(hCoreDll,(LPCSTR)6); // ExitThread
    pExcpExitThread = GetProcAddressA(hCoreDll, (LPCSTR) 1474); // ThreadExceptionExit
    pPSLNotify = (void (*)(DWORD, DWORD, DWORD))GetProcAddressA(hCoreDll,(LPCSTR)7); // PSLNotify
    pSignalStarted = (void (*)(DWORD))GetProcAddressA(hCoreDll,(LPCSTR)639); // SignalStarted
    pIsExiting = GetProcAddressA(hCoreDll,(LPCSTR)159); // IsExiting
    DEBUGMSG (ZONE_LOADER2, (L"pExitThread = %8.8lx\n", pExitThread));
    DEBUGCHK(pIsExiting == (LPDWORD)ZeroPtr(pIsExiting));
    MTBFf = (void (*)(LPVOID, ulong, ulong, ulong, ulong))GetProcAddressA(hCoreDll,(LPCSTR)14); // MainThreadBaseFunc
    CTBFf = (void (*)(LPVOID, ulong, ulong, ulong, ulong))GetProcAddressA(hCoreDll,(LPCSTR)1240); // ComThreadBaseFunc
    TBFf = (void (*)(LPVOID, ulong))GetProcAddressA(hCoreDll,(LPCSTR)13); // ThreadBaseFunc
 
#if x86
    if (!(pEmul = GetProcAddressA(hCoreDll,(LPCSTR)"NPXNPHandler"))) {
        DEBUGMSG(1,(L"Did not find emulation code for x86... using floating point hardware.\r\n"));
        dwFPType = 1;
        KCall((PKFN)InitializeFPU);
    } else {
        DEBUGMSG(1,(L"Found emulation code for x86... using software for emulation of floating point.\r\n"));
        dwFPType = 2;
        KCall((PKFN)InitializeEmx87);
        KCall((PKFN)InitNPXHPHandler,pEmul);
    }
#endif
    KSystemTimeToFileTime = (BOOL (*)(LPSYSTEMTIME, LPFILETIME))GetProcAddressA(hCoreDll,(LPCSTR)19); // SystemTimeToFileTime
    KLocalFileTimeToFileTime = (BOOL (*)(const FILETIME *, LPFILETIME))GetProcAddressA(hCoreDll,(LPCSTR)22); // LocalFileTimeToFileTime
    KFileTimeToSystemTime = (BOOL (*)(const FILETIME *, LPSYSTEMTIME))GetProcAddressA(hCoreDll,(LPCSTR)20); // FileTimeToSystemTime
    KCompareFileTime = (LONG (*)(LPFILETIME, LPFILETIME))GetProcAddressA(hCoreDll,(LPCSTR)18); // CompareFileTime
    pGetHeapSnapshot = (BOOL (*)(THSNAP *, BOOL, LPVOID *, HANDLE))GetProcAddressA(hCoreDll,(LPCSTR)52); // GetHeapSnapshot

    // do this now, so that we continue running after we've created the new thread
#ifdef START_KERNEL_MONITOR_THREAD
    CreateKernelThread((LPTHREAD_START_ROUTINE)Monitor1,0,THREAD_RT_PRIORITY_ABOVE_NORMAL,0);
#endif
    
    // If platform supports debug Ethernet, register ISR now.
    if (pKITLInitializeInterrupt)
        pKITLInitializeInterrupt();

    // initialize KTILcs
    InitializeCriticalSection (&KITLcs);
    fKITLcsInitialized = TRUE;
    SETCURKEY(0xffffffff);
    pCleanupThread = pCurThread;
    hAlarmThreadWakeup = CreateEvent(0,0,0,0);
    DEBUGCHK(hAlarmThreadWakeup);
    InitializeCriticalSection(&rtccs);
    IntrEvents[SYSINTR_RTC_ALARM-SYSINTR_DEVICES] = HandleToEvent(hAlarmThreadWakeup);
    IntrEvents[SYSINTR_RTC_ALARM-SYSINTR_DEVICES]->pIntrProxy = AllocMem(HEAP_PROXY);
    DEBUGCHK(IntrEvents[SYSINTR_RTC_ALARM-SYSINTR_DEVICES]->pIntrProxy);
    IntrEvents[SYSINTR_RTC_ALARM-SYSINTR_DEVICES]->onequeue = 1;
    
    // Give the OEM a final chance to do a more full-featured init before any
    // apps are started
    OEMIoControl(IOCTL_HAL_POSTINIT, NULL, 0, NULL, 0, NULL);
    
    hTh = CreateKernelThread((LPTHREAD_START_ROUTINE)CleanDirtyPagesThread,0,THREAD_RT_PRIORITY_NORMAL,0);
    CloseHandle(hTh);
    CreateKernelThread((LPTHREAD_START_ROUTINE)RunApps,0,THREAD_RT_PRIORITY_NORMAL,0);

    while (1) {
        KCall((PKFN)SetThreadBasePrio, hCurThread, THREAD_RT_PRIORITY_IDLE);
        WaitForMultipleObjects(1,&hAlarmThreadWakeup,0,INFINITE);
        if (InterlockedTestExchange(&PageOutNeeded,1,2) == 1) {
            // low memory, grab DirtyPageCS to give CleanDirtyPagesThread a chance to
            // clean up all un-initialized free memory and add them to free page list
            EnterCriticalSection(&DirtyPageCS);
            LeaveCriticalSection(&DirtyPageCS);
            do {
                DoPageOut();
                pPSLNotify(DLL_MEMORY_LOW,(DWORD)NULL,(DWORD)NULL);
            } while (InterlockedTestExchange(&PageOutNeeded,1,2) == 1) ;
        }

        EnterCriticalSection(&rtccs);
        UpdateAndSignalAlarm ();
        LeaveCriticalSection(&rtccs);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_SetKernelAlarm(
    HANDLE hAlarm,
    LPSYSTEMTIME lpst
    ) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetKernelAlarm entry: %8.8lx %8.8lx\r\n",hAlarm,lpst));
    EnterCriticalSection(&rtccs);
    hAlarmEvent = hAlarm;
    memcpy(&CurAlarmTime,lpst,sizeof(SYSTEMTIME));
    UpdateAndSignalAlarm ();
    LeaveCriticalSection(&rtccs);
    DEBUGMSG(ZONE_ENTRY,(L"SC_SetKernelAlarm exit\r\n"));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_RefreshKernelAlarm(void) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SC_RefreshKernelAlarm entry\r\n"));
    EnterCriticalSection(&rtccs);
    UpdateAndSignalAlarm ();
    LeaveCriticalSection(&rtccs);
    DEBUGMSG(ZONE_ENTRY,(L"SC_RefreshKernelAlarm exit\r\n"));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ProcInit(void) 
{
    // map W32 thread priority if OEM choose to
    if (pfnOEMMapW32Priority) {
        BYTE prioMap[MAX_WIN32_PRIORITY_LEVELS];
        int  i;
        memcpy (prioMap, W32PrioMap, sizeof (prioMap));
        pfnOEMMapW32Priority (MAX_WIN32_PRIORITY_LEVELS, prioMap);
        // validate the the priority is mono-increase
        for (i = 0; i < MAX_WIN32_PRIORITY_LEVELS-1; i ++) {
            if (prioMap[i] >= prioMap[i+1])
                break;
        }

        DEBUGMSG ((MAX_WIN32_PRIORITY_LEVELS-1) != i, (L"ProcInit: Invalid priority map provided by OEM, Ignored!\r\n"));
        if ((MAX_WIN32_PRIORITY_LEVELS-1) == i) {
            memcpy (W32PrioMap, prioMap, sizeof (prioMap));
        }
    }
    
    if (!dwDefaultThreadQuantum)
        dwDefaultThreadQuantum = 100;
#ifdef DEBUG
    CurMSec = dwPrevReschedTime = (DWORD) -200000;      // ~3 minutes before wrap
#endif
    hCurThread = (HANDLE)2;            // so that EnterCriticalSection doesn't put 0 for owner
    ProcArray[0].aky = 1;    // so that the AllocHandles do the right thing for permissions
    pCurProc = &ProcArray[0];
    hCurProc = AllocHandle(&cinfProc,pCurProc,pCurProc);
    DEBUGCHK(hCurProc);
    pCurThread = AllocMem(HEAP_THREAD);
    DEBUGCHK(pCurThread);
    hCurThread = AllocHandle(&cinfThread,pCurThread,pCurProc);
    DEBUGCHK(hCurThread);
    SetUserInfo(hCurProc,STILL_ACTIVE);
    pCurProc->procnum = 0;
    pCurProc->pcmdline = &L"";
    pCurProc->DbgActive = 0;
    pCurProc->hProc = hCurProc;
    pCurProc->dwVMBase = SECURE_VMBASE;
    pCurProc->pTh = pCurProc->pMainTh = pCurThread;
    pCurProc->aky = 1 << pCurProc->procnum;
    pCurProc->hDbgrThrd = 0;
    pCurProc->lpszProcName = L"NK.EXE";
    pCurProc->tlsLowUsed = TLSSLOT_RESERVE;
    pCurProc->tlsHighUsed = 0;
    pCurProc->pfnEH = 0;
    pCurProc->pExtPdata = 0;
    pCurProc->bPrio = THREAD_RT_PRIORITY_NORMAL;
    pCurProc->pStdNames[0] = pCurProc->pStdNames[1] = pCurProc->pStdNames[2] = 0;
    pCurProc->dwDyingThreads = 0;
    pCurProc->pmodResource = 0;
    pCurProc->bTrustLevel = KERN_TRUST_FULL;
#ifdef DEBUG
    pCurProc->ZonePtr = &dpCurSettings;
#else
    pCurProc->ZonePtr = 0;
#endif
    pCurProc->pProxList = 0;
    pCurProc->o32_ptr = 0;
    pCurProc->e32.e32_stackmax = KRN_STACK_SIZE;
    InitThreadStruct(pCurThread,hCurThread,pCurProc,hCurProc,THREAD_RT_PRIORITY_ABOVE_NORMAL);
    SETCURKEY(GETCURKEY()); // for CPUs that cache the access key outside the thread structure
    pCurThread->pNextInProc = pCurThread->pPrevInProc = 0;
    *(__int64 *)&pCurThread->ftCreate = 0;
}

extern void UpdateKmodVSize (openexe_t *oeptr, e32_lite *eptr);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SchedInit(void) 
{
    LPBYTE pStack;
    bAllKMode = (pTOC->ulKernelFlags & KFLAG_NOTALLKMODE) ? 0 : 1;
    InitNKSection();
    SetCPUASID(pCurThread);
#ifdef SHx
    SetCPUGlobals();
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_ALL);
#endif

    // make cbNKCoProcRegSize multiple of 4
    cbNKCoProcRegSize = (cbNKCoProcRegSize + 3) & ~3;
    // check the debug register related values.
    if ((cbNKCoProcRegSize > MAX_COPROCREGSIZE) 
        || !pOEMSaveCoProcRegister 
        || !pOEMRestoreCoProcRegister
        || !pOEMInitCoProcRegisterSavedArea) {
        cbNKCoProcRegSize = fNKSaveCoProcReg = 0;
    }
    DEBUGMSG(ZONE_SCHEDULE,(TEXT("cbNKCoProcRegSize = %d, pSave = %8.8lx, pRestore = %8.8lx\r\n"),
            cbNKCoProcRegSize, pOEMSaveCoProcRegister, pOEMRestoreCoProcRegister));

    if (OpenExe(L"nk.exe",&pCurProc->oe,0,0)) {
        LoadE32(&pCurProc->oe,&pCurProc->e32,0,0,0,1,&pCurProc->bTrustLevel);
        pCurProc->BasePtr = (LPVOID)pCurProc->e32.e32_vbase;
        UpdateKmodVSize (&pCurProc->oe, &pCurProc->e32);
    }
    pStack = VirtualAlloc((LPVOID)pCurProc->dwVMBase,pCurProc->e32.e32_stackmax,MEM_RESERVE|MEM_AUTO_COMMIT,PAGE_NOACCESS);
    // need to call DoVirtualAlloc directly to bypass stack check
    DoVirtualAlloc(pStack+pCurProc->e32.e32_stackmax-PAGE_SIZE,PAGE_SIZE,MEM_COMMIT,PAGE_READWRITE, MEM_NOSTKCHK, 0);
    MDCreateThread(pCurThread, pStack, pCurProc->e32.e32_stackmax, (LPVOID)SystemStartupFunc, 0, TH_KMODE, 0);
    pCurThread->dwOrigBase = (DWORD) pStack;
    pCurThread->dwOrigStkSize = CNP_STACK_SIZE;
    ZeroTLS(pCurThread);
    pCurThread->dwUserTime = 0;
    pCurThread->dwKernTime = 0;
    pCurThread->tlsSecure = pCurThread->tlsNonSecure = pCurThread->tlsPtr;
    MakeRun(pCurThread);
    DEBUGMSG(ZONE_SCHEDULE,(TEXT("Scheduler: Created master thread %8.8lx\r\n"),pCurThread));

   // Save off the thread's program counter for getting its name later.
   pCurThread->dwStartAddr = (DWORD) SystemStartupFunc;
    
   if (IsCeLogKernelEnabled()) {
       CELOG_ThreadCreate(pCurThread, pCurThread->pOwnerProc->hProc, NULL, 0);
   }
}

typedef void PAGEFN();

extern PAGEFN PageOutProc, PageOutMod, PageOutFile;

PAGEFN * const PageOutFunc[3] = {PageOutProc,PageOutMod,PageOutFile};



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
DoPageOut(void) 
{
    static int state;
    extern DWORD cbNKPagingPoolSize;
    int tries = 0;
    EnterCriticalSection(&PageOutCS);
    if (cbNKPagingPoolSize) {
        // Using paging pool, no reason to page out in low memory since it won't help.
        // Memory mapped file isn't using paging pool now, so we'll still page it out,
        // but in the future we'll make the modificaiton for the memory mapped files to
        // use paging pool and the function will be a no-op when using paging pool
        PageOutFile ();
    } else {
        while ((tries++ < 3) && (PageOutNeeded || PageOutForced)) {
            if (PageOutFunc[state]) {
                (PageOutFunc[state])();
            }
            state = (state+1)%3;
        }
    }
    LeaveCriticalSection(&PageOutCS);
}

