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
//    thread.c - implementations of Thread APIs
//
#include <windows.h>
#include <kernel.h>
#include <dbgrapi.h>

#ifdef x86
#pragma optimize ("y", off)
#endif

ERRFALSE ((offsetof(THREAD, ftCreate) & 7) == 0);

#define GTC_VALID_FLAGS     (STACKSNAP_FAIL_IF_INCOMPLETE | STACKSNAP_EXTENDED_INFO | STACKSNAP_INPROC_ONLY | STACKSNAP_RETURN_FRAMES_ON_ERROR | STACKSNAP_NEW_VM | STACKSNAP_GET_MANAGED_FRAMES)

#define MAX_SKIP            0x10000     // skipping 64K frames? I don't think so
#define MAX_NUM_FRAME       0x10000     // max frame 64K

DWORD g_dwCoProcPool;
#ifndef SHIP_BUILD
BOOL g_fCheckForTlsLeaks;
#endif

extern void RemovePslServer (DWORD dwProcessId);

//
// GetThreadCallStack call from user mode
//
static ULONG EXTGetCallStack (PTHREAD pth, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip)
{
    DWORD  dwFrameSize = (STACKSNAP_NEW_VM & dwFlags)? sizeof (CallSnapshot3) : ((STACKSNAP_EXTENDED_INFO & dwFlags)? sizeof (CallSnapshotEx) : sizeof (CallSnapshot));
    DWORD  dwTotalSize = dwMaxFrames * dwFrameSize;
    DWORD  _local[512];         // 2K local copy
    LPVOID pFrameCopy  = _local;
    DWORD  dwErr = 0;
    DWORD  dwRtn = 0;
    
    // validate arguments
    if ((dwFlags & ~GTC_VALID_FLAGS)                        // invalid flag
        || (dwSkip > MAX_SKIP)                              // too many skip
        || !dwMaxFrames                                     // dwMaxFrames = 0
        || (dwMaxFrames > MAX_NUM_FRAME)                    // dwMaxFrames too big
        || !IsValidUsrPtr (lpFrames, dwTotalSize, TRUE)     // buffer not valid
        || ((dwFlags & STACKSNAP_EXTENDED_INFO) && (dwFlags & STACKSNAP_NEW_VM)) // invalid; only one of the extended flags should be present
        ) {                                   
        dwErr = ERROR_INVALID_PARAMETER;

    } else if ((dwTotalSize > sizeof (_local))
               && (NULL == (pFrameCopy = VMAlloc (g_pprcNK, NULL, dwTotalSize, MEM_COMMIT, PAGE_READWRITE)))) {
        dwErr = ERROR_NOT_ENOUGH_MEMORY;

    } else {

        dwRtn = THRDGetCallStack (pth, dwMaxFrames, pFrameCopy, dwFlags, (pth == pCurThread) ? dwSkip+1 :  dwSkip);
        if (dwRtn && !CeSafeCopyMemory (lpFrames, pFrameCopy, dwRtn * dwFrameSize)) {
            dwErr = ERROR_INVALID_PARAMETER;
            dwRtn = 0;
        }
    }

    if (pFrameCopy && (_local != pFrameCopy)) {
        VMFreeAndRelease (g_pprcNK, pFrameCopy, dwTotalSize);
    }
    
    if (dwErr) {
        SetLastError (dwErr);
    }

    return dwRtn;
}

#ifdef x86
#pragma optimize ("y", on)
#endif


static BOOL THRDPreClose (PTHREAD pth, PTHREAD pthExited)
{
    // if pth is not NULL, handle is closed due to failure in thread creation, do nothing in this case.
    // otherwise - close the special "thread id" handle
    DEBUGCHK (!pth || (pth == pthExited));

    if (!pth) {
        PHDATA phd = HNDLZapHandle (g_pprcNK, (HANDLE)pthExited->dwId);
        DEBUGCHK (phd);
        // unlock twice, HNDLZapHandle locked the handle to get to phd
        DoUnlockHDATA (phd, -2*LOCKCNT_INCR);
    }
    return TRUE;
}


/*

Note about the API set: All APIs should either return 0 on error or return
no value at-all. This will allow us to return gracefully to the user when an
argument to an API doesn't pass the PSL checks. Otherwise we will end
up throwing exceptions on invalid arguments to API calls.

*/

/* 
    Note: internal table refers to some external entry
    points to force both internal and external clients
    to go through policy check.
*/

static const PFNVOID ThrdIntMthds[] = {
    (PFNVOID)THRDCloseHandle,
    (PFNVOID)THRDPreClose,
    (PFNVOID)THRDSuspend,       // 2 - SuspendThread
    (PFNVOID)THRDResume,        // 3 - ResumeThread
    (PFNVOID)THRDSetPrio,       // 4 - SetThreadPriority
    (PFNVOID)THRDGetPrio,       // 5 - GetThreadPriority
    (PFNVOID)THRDGetCallStack,  // 6 - GetThreadCallStack
    (PFNVOID)THRDGetContext,    // 7 - GetThreadContext
    (PFNVOID)THRDSetContext,    // 8 - SetThreadContext
    (PFNVOID)THRDTerminate,     // 9 - TerminateThread
    (PFNVOID)THRDGetPrio256,    // 10 - CeGetThreadPriority
    (PFNVOID)THRDSetPrio256,    // 11 - CeSetThreadPriority
    (PFNVOID)THRDGetQuantum,    // 12 - CeGetThreadQuantum
    (PFNVOID)THRDSetQuantum,    // 13 - CeSetThreadQuantum
    (PFNVOID)THRDGetID,         // 14 - GetThreadId
    (PFNVOID)THRDGetProcID,     // 15 - GetProcessIdOfThread
    (PFNVOID)THRDGetAffinity,   // 16 - CeGetThreadAffinity
    (PFNVOID)THRDSetAffinity,   // 17 - CeSetThreadAffinity
    (PFNVOID)NotSupported,      // 18 - Unsupported
};

static const PFNVOID ThrdExtMthds[] = {
    (PFNVOID)THRDCloseHandle,
    (PFNVOID)THRDPreClose,
    (PFNVOID)THRDSuspend,       // 2 - SuspendThread
    (PFNVOID)THRDResume,        // 3 - ResumeThread
    (PFNVOID)THRDSetPrio,       // 4 - SetThreadPriority
    (PFNVOID)THRDGetPrio,       // 5 - GetThreadPriority
    (PFNVOID)EXTGetCallStack,   // 6 - GetThreadCallStack
    (PFNVOID)THRDGetContext,    // 7 - GetThreadContext
    (PFNVOID)THRDSetContext,    // 8 - SetThreadContext
    (PFNVOID)THRDTerminate,     // 9 - TerminateThread
    (PFNVOID)THRDGetPrio256,    // 10 - CeGetThreadPriority
    (PFNVOID)THRDSetPrio256,    // 11 - CeSetThreadPriority
    (PFNVOID)THRDGetQuantum,    // 12 - CeGetThreadQuantum
    (PFNVOID)THRDSetQuantum,    // 13 - CeSetThreadQuantum
    (PFNVOID)THRDGetID,         // 14 - GetThreadId
    (PFNVOID)THRDGetProcID,     // 15 - GetProcessIdOfThread
    (PFNVOID)THRDGetAffinity,   // 16 - CeGetThreadAffinity
    (PFNVOID)THRDSetAffinity,   // 17 - CeSetThreadAffinity
    (PFNVOID)NotSupported,      // 18 - Unsupported    
};

static const ULONGLONG ThrdSigs[] = {
    0,                          // CloseHandle, never call direct
    0,                          // preclose, not required
    FNSIG2 (DW, IO_PDW),        // 2 - SuspendThread
    FNSIG2 (DW, IO_PDW),        // 3 - ResumeThread
    FNSIG2 (DW, DW),            // 4 - SetThreadPriority
    FNSIG2 (DW, IO_PDW),        // 5 - GetThreadPriority
    FNSIG5 (DW, DW, DW, DW, DW),// 6 - GetThreadCallStack - NOTE: GetThreadCallStack validate arguments in proc
    FNSIG2 (DW, IO_PDW),        // 7 - GetThreadContext
    FNSIG2 (DW, IO_PDW),        // 8 - SetThreadContext
    FNSIG3 (DW, DW, DW),        // 9 - TerminateThread
    FNSIG2 (DW, IO_PDW),        // 10 - CeGetThreadPriority
    FNSIG2 (DW, DW),            // 11 - CeSetThreadPriority
    FNSIG2 (DW, IO_PDW),        // 12 - CeGetThreadQuantum
    FNSIG2 (DW, DW),            // 13 - CeSetThreadQuantum
    FNSIG1 (DW),                // 14 - GetThreadId
    FNSIG1 (DW),                // 15 - GetProcessIdOfThread
    FNSIG2 (DW, IO_PDW),        // 16 - CeGetThreadAffinity
    FNSIG2 (DW, DW),            // 17 - CeSetThreadAffinity
    FNSIG3 (DW, O_PTR, DW),     // 18 - Unsupported
};

//
// Basic rules for thread access are:
// a) To operate on a thread, current thread should have
//     the right open privilege set. For example to get
//     thread callstack, calling thread should be able open
//     the target thread with THREAD_GET_CONTEXT
//     privilege.
// b) Additionally there are cases where setting a thread
//     changes the real-time behavior of the system.
//     For example: setting thread priority, affinity,
//     quantum etc. In this case in addition to having a
//     valid thread hanlde of the target thread, the
//     calling thread should have scheduler privilege
//     set.
//
// Definitions taken from sdk\inc\winnt.h
//
static const DWORD ThrdAccessMask[] = {
    0,                          // 0 - CloseHandle
    0,                          // 1 - PreCloseHandle
    THREAD_SUSPEND_RESUME,      // 2 - SuspendThread
    THREAD_SUSPEND_RESUME,      // 3 - ResumeThread
    THREAD_SET_INFORMATION,     // 4 - SetThreadPriority
    THREAD_QUERY_INFORMATION,   // 5 - GetThreadPriority
    THREAD_GET_CONTEXT,         // 6 - GetThreadCallStack
    THREAD_GET_CONTEXT,         // 7 - GetThreadContext
    THREAD_SET_CONTEXT,         // 8 - SetThreadContext
    THREAD_TERMINATE,           // 9 - TerminateThread
    THREAD_QUERY_INFORMATION,   // 10 - CeGetThreadPriority
    THREAD_SET_INFORMATION,     // 11 - CeSetThreadPriority
    THREAD_QUERY_INFORMATION,   // 12 - CeGetThreadQuantum
    THREAD_SET_INFORMATION,     // 13 - CeSetThreadQuantum
    THREAD_QUERY_INFORMATION,   // 14 - GetThreadId
    THREAD_QUERY_INFORMATION,   // 15 - GetProcessIdOfThread
    THREAD_QUERY_INFORMATION,   // 16 - CeGetThreadAffinity
    THREAD_SET_INFORMATION,     // 17 - CeSetThreadAffinity
    THREAD_QUERY_INFORMATION,   // 18 - Unsupported    
};

ERRFALSE (sizeof(ThrdExtMthds) == sizeof(ThrdIntMthds));
ERRFALSE (sizeof(ThrdExtMthds) == sizeof(ThrdAccessMask));
ERRFALSE ((sizeof(ThrdExtMthds) / sizeof(ThrdExtMthds[0])) == (sizeof(ThrdSigs) / sizeof(ThrdSigs[0])));

const CINFO cinfThread = {
    { 'T', 'H', 'R', 'D' },
    DISPATCH_KERNEL_PSL,
    SH_CURTHREAD,
    sizeof(ThrdExtMthds)/sizeof(ThrdExtMthds[0]),
    ThrdExtMthds,
    ThrdIntMthds,
    ThrdSigs,
    0,
    0,
    0,
    ThrdAccessMask,
};


BYTE W32PrioMap[MAX_WIN32_PRIORITY_LEVELS] = {
    MAX_CE_PRIORITY_LEVELS-8,
    MAX_CE_PRIORITY_LEVELS-7,
    MAX_CE_PRIORITY_LEVELS-6,
    MAX_CE_PRIORITY_LEVELS-5,
    MAX_CE_PRIORITY_LEVELS-4,
    MAX_CE_PRIORITY_LEVELS-3,
    MAX_CE_PRIORITY_LEVELS-2,
    MAX_CE_PRIORITY_LEVELS-1,
};

static long g_cMinPageThrdCreate = 20;

// for save/restore CoProcRegister

extern PTHREAD pOOMThread;


__inline LPBYTE CreateSecureStack (void)
{
    return VMCreateStack (g_pprcNK, KRN_STACK_SIZE);
}

//------------------------------------------------------------------------------
// add a thread to a process
//------------------------------------------------------------------------------
static BOOL AddThreadToProc (PPROCESS pprc, PTHREAD pth) 
{
    BOOL fRet = FALSE;
    LockLoader (pprc);
    if (!IsProcessExiting (pprc)) {
        AddToDListHead (&pprc->thrdList, &pth->thLink);
        InterlockedIncrement ((PLONG) &pprc->wThrdCnt);
        fRet = TRUE;
    }
    UnlockLoader (pprc);
    return fRet;
}


//------------------------------------------------------------------------------
// remove a thread from a process
//------------------------------------------------------------------------------
static VOID RemoveThreadFromProc (PPROCESS pproc, PTHREAD pth) 
{
    DEBUGCHK (pth == pCurThread);
    
    LockLoader (pproc);

    pproc->dwKrnTime += pth->dwKTime;
    pproc->dwUsrTime += pth->dwUTime;
    
    RemoveDList (&pth->thLink);
    // NOTE: RemoveThreadFromProc doesn't decrement thread count to avoid race in NKExitThread.
    //       It will be decremented before calling SCHL_FinalRemoveThread to ensure thread counts 
    //       matches # of threads in a process.
    UnlockLoader (pproc);

    //
    // switch the thread owner to kernel
    //
    SCHL_SwitchOwnerToKernel (pth);

    // switch VM and active process to kernel
    SwitchVM (g_pprcNK);
    SwitchActiveProcess (g_pprcNK);

}

//------------------------------------------------------------------------------
// InitThreadStruct - initialize a thread structure
//------------------------------------------------------------------------------
static void InitThreadStruct(
    PTHREAD     pTh,        // pointer to thread
    HANDLE      hTh,        // handle to the thread in NK's handle table (thread ID)
    PPROCESS    pProc,      // which process
    DWORD       prioAndFlags        // priority of the thread
    ) 
{
    // NOTE: DO NOT CALL memset on thread structure. wCount and wCount2 should
    //       never be reset.
    HNDLSetUserInfo (hTh, (DWORD) pTh, g_pprcNK);
    pTh->dwExitCode     = STILL_ACTIVE;
    pTh->pprcActv       = pTh->pprcOwner = pProc;
    pTh->bWaitState     = WAITSTATE_SIGNALLED;
    pTh->dwQuantLeft    = pTh->dwQuantum = g_pOemGlobal->dwDefaultThreadQuantum;
    pTh->dwAffinity     = pProc->dwAffinity;
    pTh->wInfo          = 0;

    if (pTh->dwAffinity) {
        pTh->pRunQ = &g_ppcbs[pTh->dwAffinity-1]->affinityQ;
    } else if (pProc->pStubThread) {
        pTh->pRunQ = &pProc->pStubThread->ProcReadyQ;
    } else {
        pTh->pRunQ = &RunList;
    }
    
    // record thread creation time
    SET_BPRIO(pTh,prioAndFlags);
    SET_CPRIO(pTh,prioAndFlags);
    pTh->hTok = pProc->hTok;
    
    InitDList (&pTh->thLink);
    if (g_pprcNK == pProc) {
#ifdef x86        
        pTh->lpFPEmul = g_pKCoredllFPEmul;
#endif        
        pTh->pprcVM = NULL;
        SET_TIMEKERN (pTh);
    } else {
    #ifdef x86
        pTh->lpFPEmul = g_pCoredllFPEmul;
    #endif    
        pTh->pprcVM = pProc;
    }
}

//------------------------------------------------------------------------------
// SetupThread - allocate and setup a thread structure.
//------------------------------------------------------------------------------
static PTHREAD SetupThread (
    PPROCESS pprc,          // which process
    LPBYTE  lpStack,        // stack
    DWORD   cbStack,        // stack size
    LPBYTE  pSecureStk,     // secure stack
    FARPROC lpStart,        // entry point
    LPVOID  param,          // parameter to thread function
    ulong   mode,           // kernel/user mode
    DWORD   prioAndFlags    // priority and flag
    ) 
{
    PTHREAD     pth         = NULL;
    LPBYTE      pCoProcSaveArea = NULL;
    PSTKLIST    pStkList;
    HANDLE      hth = NULL;
    
    DEBUGMSG(ZONE_SCHEDULE,(TEXT("+ SetupThread: (%8.8lx, %8.8lx, %8.8lx, %8.8lx %8.8lx, %8.8lx, %8.8lx, %8.8lx)\r\n"),
            pprc, pprc->dwId,lpStack, cbStack, pSecureStk, lpStart, param, prioAndFlags));

    // call OEM to initlized co-proc save area for debug registers if required
    if (g_pOemGlobal->cbCoProcRegSize) {
        pCoProcSaveArea = (LPBYTE) AllocMem (g_dwCoProcPool);
        if (!pCoProcSaveArea) {
            DEBUGMSG (ZONE_ERROR, (L"ERROR: SetupThread - unable to allocate co-proc save area\r\n"));
            return NULL;
        }
        g_pOemGlobal->pfnInitCoProcRegs (pCoProcSaveArea);
    }

    pStkList = (PSTKLIST) AllocMem (HEAP_STKLIST);                    // allocate the stklist (16 bytes) used for thread exit
    if (pStkList
        && (NULL != (pth = AllocMem (HEAP_THREAD)))) {                               // thread structure available?

        // NOTE: DO NOT CALL memset on thread structure. wCount and wCount2 should
        //       never be reset.
        // memset (pth, 0, sizeof(THREAD));
        pth->dwId = 0;      // force thread to be available only after the thread structure is initialized later
        hth = HNDLCreateHandle (&cinfThread, pth, g_pprcNK, THREAD_ALL_ACCESS);
        if (hth) {         // thread handle available?

            DEBUGMSG (ZONE_SCHEDULE, (L"SetupThread: pth = %8.8lx, hth = %8.8lx\r\n", pth, hth));

            // initialize thread structure
            InitThreadStruct (pth, hth, pprc, prioAndFlags);
            pth->pCoProcSaveArea = pCoProcSaveArea;
            pth->pStkForExit = pStkList;

            pth->pcstkTop   = NULL;
            pth->lpProxy    = NULL;
            pth->pUpSleep   = pth->pDownSleep = NULL;
            pth->pBlockers  = NULL;
            pth->pSavedCtx  = NULL;
            pth->dwLastError = pth->dwDbgFlag = pth->dwUTime = pth->dwKTime = 0;
            pth->bPendSusp  = pth->bDbgCnt = 0;
            pth->hMQDebuggerRead = pth->hDbgEvent = NULL;

            memset (&pth->ftExit, 0, sizeof (FILETIME));
            memset (&pth->ownedlist, 0, sizeof (pth->ownedlist));

            NKGetTimeAsFileTime (&pth->ftCreate, TM_SYSTEMTIME);

            // Save off the thread's program counter for getting its name later.
            pth->dwStartAddr = (DWORD) lpStart;
            // save the original stack base/bound
            pth->dwOrigBase = (DWORD) lpStack;
            pth->dwOrigStkSize = cbStack;

            // initialize TLS info
            pth->tlsPtr = pth->tlsNonSecure = TLSPTR (lpStack, cbStack);
            pth->tlsSecure = (pSecureStk != lpStack)? TLSPTR(pSecureStk, KRN_STACK_SIZE) : pth->tlsPtr;

            pth->bSuspendCnt = 1;       // suspended the new thread first
            SET_RUNSTATE(pth,RUNSTATE_BLOCKED);
            
            // perform machine dependent thread initialization
            if (prioAndFlags & THRD_NO_BASE_FUNCITON)
                MDSetupThread(pth, lpStart, (FARPROC) (LONG) param, mode, (ulong)param);
            else
                MDSetupThread(pth, IsKModeAddr ((DWORD)lpStart)? KTBFf : TBFf,
                                lpStart, mode, (ulong)param);

            // AddThreadToProc failed if the process is exiting
            pth->dwId = (DWORD) hth & ~1;
            if (AddThreadToProc (pprc, pth)) {
                CELOG_ThreadCreate(pth, pprc, param);

                DEBUGMSG (ZONE_SCHEDULE, (L"SetupThread returns %8.8lx, id = %8.8lx\r\n", pth, pth->dwId));
                return pth;
            }

            // thread creation failed. Close the "thread-id" handle.
            HNDLCloseHandle (g_pprcNK, hth);
            pth = NULL;         // HNDLCloseHandle will free the thread memory.  Set to NULL so we don't free twice
        }
    }

    // Thread creation failed, free memory
    DEBUGMSG (ZONE_SCHEDULE, (L"SetupThread Failed\r\n"));

    if (pCoProcSaveArea) {
        FreeMem (pCoProcSaveArea, g_dwCoProcPool);
    }

    if (pStkList)
        FreeMem (pStkList, HEAP_STKLIST);

    if (pth) {
        FreeMem (pth, HEAP_THREAD);
    }
    return NULL;
    
}


//------------------------------------------------------------------------------
// closing a thread handle
//------------------------------------------------------------------------------
BOOL THRDCloseHandle (PTHREAD pth, PTHREAD pthExited) 
{
    
    DEBUGCHK (!pth || (pth == pthExited));
    FreeMem (pthExited, HEAP_THREAD);

    return TRUE;
}


//------------------------------------------------------------------------------
// suspend a thread
//------------------------------------------------------------------------------
BOOL THRDSuspend (PTHREAD pth, LPDWORD lpRetVal)
{
    PTHREAD pCurTh = pCurThread;
    DWORD   retval;

    DEBUGCHK(lpRetVal);
    DEBUGMSG(ZONE_ENTRY,(L"THRDSuspend entry: %8.8lx\r\n",pth));
    if (pth == pCurTh) {
        SET_USERBLOCK (pth);
    }
    retval = SCHL_ThreadSuspend (pth);
    if (pth == pCurTh) {
        CLEAR_USERBLOCK (pth);
    }
    if ((int) retval < 0) {
        KSetLastError (pCurTh, (0xffffffff == retval)? ERROR_SIGNAL_REFUSED : ERROR_INVALID_PARAMETER);
        retval = 0xffffffff;
        // if the following debugchk fires, then a kernel thread is being suspended;
        // generally not a good idea as resources might be held blocking other
        // kernel mode components.
        DEBUGCHK (0);

    }
    
    DEBUGMSG(ZONE_ENTRY,(L"THRDSuspend exit: %8.8lx\r\n",retval));

    *lpRetVal = retval;
    return TRUE;
}


//------------------------------------------------------------------------------
// resume a thread 
//------------------------------------------------------------------------------
BOOL THRDResume (PTHREAD pth, LPDWORD lpRetVal)
{
    DWORD retval;
    DEBUGMSG(ZONE_ENTRY,(L"THRDResume entry: %8.8lx\r\n",pth));
    retval = SCHL_ThreadResume (pth);
    DEBUGMSG(ZONE_ENTRY,(L"THRDResume exit: %8.8lx\r\n",retval));

    if (lpRetVal)
        *lpRetVal = retval;
        
    return TRUE;
}

//------------------------------------------------------------------------------
// THRDSetPrio - Set thread priority - internal entry point
//------------------------------------------------------------------------------
BOOL THRDSetPrio (PTHREAD pth, DWORD prio)
{
    DWORD dwErr = 0;
    
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetPrio entry: %8.8lx %8.8lx\r\n",pth,prio));
    
    if (!pth || (prio >= MAX_WIN32_PRIORITY_LEVELS)) {
        dwErr = ERROR_INVALID_PARAMETER;

        } else if (!SCHL_SetThreadBasePrio (pth, W32PrioMap[prio])) {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    
    if (dwErr) {
        KSetLastError(pCurThread, dwErr);
    }

    DEBUGMSG(ZONE_ENTRY,(L"THRDSetPrio exit: %8.8lx\r\n", dwErr));
    return !dwErr;
}

//------------------------------------------------------------------------------
// get thread priority
//------------------------------------------------------------------------------
BOOL THRDGetPrio (PTHREAD pth, LPDWORD lpRetVal)
{
    DWORD retval = pth? GET_BPRIO (pth) : THREAD_PRIORITY_ERROR_RETURN;
    
    DEBUGMSG(ZONE_ENTRY,(L"THRDGetPrio entry: %8.8lx\r\n", pth));

    DEBUGCHK(lpRetVal);

    if (pth) {

        // if the priority hasn't been mapped by OEM, return TIME_CRITICAL for any thread
        // in RT range. Otherwise return THREAD_PRIORITY_ERROR_RETURN if there's no mapping
        // for the priority

        if (MAX_CE_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS == W32PrioMap[0]) {
            // mapping not changed        
            if (retval < MAX_CE_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS)
                retval = MAX_CE_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS;
            retval -= (MAX_CE_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS);        
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

    *lpRetVal = retval;
    DEBUGMSG(ZONE_ENTRY,(L"THRDGetPrio exit: %8.8lx\r\n", retval));
    return TRUE;
}

//------------------------------------------------------------------------------
// change affinity of a thread - internal entry point
//------------------------------------------------------------------------------
BOOL THRDSetAffinity (PTHREAD pth, DWORD dwAffinity)
{
    DWORD   dwErr  = 0;
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetAffinity entry: %8.8lx %8.8lx\r\n", pth, dwAffinity));

    if (dwAffinity > g_pKData->nCpus) {
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else if (g_pKData->nCpus > 1){

        if (pth->pprcOwner->e32.e32_cevermajor < FIRST_SMP_SUPPORT_VERSION) {
            // cannot change thread affinity if EXE is built before 7.
            dwErr = ERROR_ACCESS_DENIED;

        } else if (dwAffinity && (CE_PROCESSOR_STATE_POWERED_ON != g_ppcbs[dwAffinity-1]->dwCpuState)) {
            dwErr = ERROR_NOT_READY;

        } else {        
            SCHL_SetThreadAffinity (pth, dwAffinity);
        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }
    
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetAffinity exit: dwErr = %8.8lx\r\n", dwErr));
    return !dwErr;
}

//------------------------------------------------------------------------------
// get affinity of a thread
//------------------------------------------------------------------------------
DWORD DoGetAffinity (DWORD dwBaseAffinity)
{
    return ((dwBaseAffinity > 1)
        && (CE_PROCESSOR_STATE_POWERED_ON != g_ppcbs[dwBaseAffinity-1]->dwCpuState)) 
    ? 1                     // affinity is set to a CPU that is powered off, change to 1
    : dwBaseAffinity;
}


BOOL THRDGetAffinity (PTHREAD pth, LPDWORD pdwAffinity)
{
    *pdwAffinity = DoGetAffinity (pth->dwAffinity);
    return TRUE;
}


//------------------------------------------------------------------------------
// set thread context
//
// We should really think about depreciate this API (SetThreadContext), as it 
// doesn't make any sense for the OS, and it's very dangerous.
//
//------------------------------------------------------------------------------
BOOL THRDSetContext (PTHREAD pth, const CONTEXT * lpContext)
{
    DWORD   dwErr = ERROR_INVALID_PARAMETER;
    CONTEXT ctx;

    // make a local copy to make sure the the memory is valid
    if (pth && CeSafeCopyMemory (&ctx, lpContext, sizeof (CONTEXT))) {

        if (SCHL_DoThreadSetContext (pth, &ctx)) {
            dwErr = 0;
        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return !dwErr;
}

//------------------------------------------------------------------------------
// get thread context
//------------------------------------------------------------------------------
BOOL THRDGetContext (PTHREAD pth, LPCONTEXT lpContext)
{
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    PTHREAD pthCurrent = pCurThread;
    CONTEXT ctx;

    // make a local copy to make sure the the memory is valid
    __try {
        ctx = *lpContext;
        if (SCHL_DoThreadGetContext (pth, &ctx)) {
            *lpContext = ctx;
            dwErr = 0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // dwErr already set
    }

    if (dwErr) {
        KSetLastError (pthCurrent, dwErr);
    }

    return !dwErr;
}


#ifdef x86
    #define SKIP_FRAMES 0
#else
    #define SKIP_FRAMES 2
#endif

#ifdef x86
// x86 - disable frame pointer omission
#pragma optimize ("y", off)
#endif


//------------------------------------------------------------------------------
// THRDGetCallStack - get callstack of a thread
//------------------------------------------------------------------------------
ULONG THRDGetCallStack (PTHREAD pth, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip)
{
    DWORD dwCnt = 0;

    if (!pth) {
        // thread already exited
        KSetLastError (pCurThread, ERROR_INVALID_HANDLE);
        
    } else {
    
        CONTEXT ctx;
        DWORD   bPrio;
    
        bPrio = GET_BPRIO (pCurThread);
        
        DEBUGCHK (IsKModeAddr ((DWORD) lpFrames));

        if (pth == pCurThread) {

            // current thread, get current context

#ifdef x86
            // for x86, we only need EBP to get the callstack
            __asm {
                mov  ctx.Ebp, ebp
            }
            ctx.Eip = (DWORD)THRDGetCallStack + 0x3;
            
#else
            // get the current context
            RtlCaptureContext(&ctx);

            // skip the 1st frame since it's a leaf entry
            CONTEXT_TO_PROGRAM_COUNTER(&ctx) = (DWORD) ctx.IntRa - INST_SIZE;

#endif
            dwSkip += SKIP_FRAMES;
            ctx.ContextFlags = CONTEXT_SEH;

        } else {

            DWORD dwThrdPrio = GET_CPRIO(pth);

            // other threads, get the threads context
            ctx.ContextFlags = CONTEXT_SEH;

            // boost the priority of current thread to be higher than the thread we want to get callstack
            // NOTE: there is still possibility that the thread will run if its priority got inverted by higher priority
            //       threads. Well, unless we always set priority to 0, it doesn't seems to be a way to avoid it.
            //       Since this is a debug api and we can tolerate some error to exist, all we do is trying our best to
            //       avoid race condition.
            if (bPrio >= dwThrdPrio) {
                SCHL_SetThreadBasePrio (pCurThread, dwThrdPrio? dwThrdPrio-1 : dwThrdPrio);
            }
                
            // obtain the context of the thread
            SCHL_DoThreadGetContext (pth, &ctx);
            
        }

        dwCnt = NKGetThreadCallStack (pth, dwMaxFrames, lpFrames, dwFlags, dwSkip, &ctx);

        // restore priority if we changed it.
        if (GET_BPRIO(pCurThread) != bPrio) {
            SCHL_SetThreadBasePrio (pCurThread, bPrio);
        }
    }
    return dwCnt;
}
#ifdef x86
#pragma optimize ("y", on)
#endif



//------------------------------------------------------------------------------
// worker function to kill one thread
//------------------------------------------------------------------------------
void KillOneThread (PTHREAD pth, DWORD dwExitCode) 
{
    sttd_t sttd;
    
    sttd.pThNeedRun = 0;
    sttd.pThNeedBoost = 0;
    SCHL_SetThreadToDie (pth, dwExitCode, &sttd);
    if (sttd.pThNeedBoost)
        SCHL_SetThreadBasePrio (sttd.pThNeedBoost, GET_CPRIO(pCurThread));
    if (sttd.pThNeedRun)
        SCHL_MakeRunIfNeeded (sttd.pThNeedRun);
    
}

//------------------------------------------------------------------------------
// terminate a thread
//------------------------------------------------------------------------------
BOOL THRDTerminate (PTHREAD pth, DWORD dwExitCode, BOOL fWait)
{
    BOOL fRet = FALSE;
    if (!pth) {
        KSetLastError (pCurThread, ERROR_INVALID_HANDLE);
        
    } else if (IsMainThread (pth)) {
        fRet = PROCTerminate (pth->pprcOwner, dwExitCode);
        
    } else {
    
        DEBUGCHK (pth->pprcOwner != g_pprcNK);
        KillOneThread (pth, dwExitCode);

        if (fWait
            && (pth->dwId != dwCurThId)
            && (NKWaitForKernelObject ((HANDLE) pth->dwId, 3000) == WAIT_TIMEOUT)) {
            DEBUGMSG (ZONE_ERROR, (L"ERROR: THRDTerminate - Unable to terminate thread %8.8lx\r\n", pth->dwId));
            KSetLastError (pCurThread, ERROR_ACCESS_DENIED);
        } else {
            fRet = TRUE;
        }
    }
    return fRet;
}


//------------------------------------------------------------------------------
// get thread priority (abs priority value)
//------------------------------------------------------------------------------
BOOL THRDGetPrio256 (PTHREAD pth, LPDWORD lpRetVal)
{
    BOOL fRet = (NULL != pth);

    if (!fRet) {
        KSetLastError (pCurThread, ERROR_INVALID_HANDLE);
        
    } else {
        
        DEBUGCHK(lpRetVal);

        DEBUGMSG(ZONE_ENTRY,(L"THRDGetPrio256 entry: %8.8lx\r\n", pth));
        *lpRetVal = GET_BPRIO(pth);
        DEBUGMSG(ZONE_ENTRY,(L"THRDGetPrio256 exit: %8.8lx\r\n", *lpRetVal));

    }
    return fRet;
}

//------------------------------------------------------------------------------
// Set thread priority (abs priority value) - internal entry point
//------------------------------------------------------------------------------
BOOL THRDSetPrio256 (PTHREAD pth, DWORD prio) 
{
    DWORD dwErr =  0;
    
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetPrio256 entry: %8.8lx %8.8lx\r\n", pth, prio));

    if (!pth || (prio >= MAX_CE_PRIORITY_LEVELS)) {
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else if (!SCHL_SetThreadBasePrio (pth, prio)) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }
    
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetPrio256 exit: %8.8lx\r\n",dwErr));
    return !dwErr;
}

//------------------------------------------------------------------------------
// get thread quantum
//------------------------------------------------------------------------------
BOOL THRDGetQuantum (PTHREAD pth, LPDWORD lpRetVal)
{
    BOOL fRet = (NULL != pth);

    if (!fRet) {
        KSetLastError (pCurThread, ERROR_INVALID_HANDLE);
        
    } else {
        DEBUGCHK(lpRetVal);
        
        DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetThreadQuantum entry: %8.8lx\r\n", pth));
        *lpRetVal = pth->dwQuantum;
        DEBUGMSG(ZONE_ENTRY,(L"SC_CeGetThreadQuantum exit: %8.8lx\r\n", *lpRetVal));
    }
    return fRet;
}

//------------------------------------------------------------------------------
// set thread quantum - internal entry point
//------------------------------------------------------------------------------
BOOL THRDSetQuantum (PTHREAD pth, DWORD dwQuantum)
{
    DWORD dwErr = 0;
    
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetQuantum entry: %8.8lx %8.8lx\r\n", pth, dwQuantum));

    if (!pth || (dwQuantum & 0x80000000)) {
        dwErr = ERROR_INVALID_HANDLE;
                
    } else if (!SCHL_SetThreadQuantum (pth, dwQuantum)) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (!dwErr) {
        CELOG_ThreadSetQuantum(pth, dwQuantum);
    } else {
        KSetLastError (pCurThread, dwErr);
    }
    
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetQuantum exit: %8.8lx\r\n", dwErr));
    
    return !dwErr;
}

//
// THRDGetID - return thread id of a thread
//
DWORD THRDGetID (PTHREAD pth)
{
    return pth? pth->dwId : 0;
}

//
// THRDGetProcID - return owner process id of a thread
//
DWORD THRDGetProcID (PTHREAD pth)
{
    return pth? pth->pprcOwner->dwId : 0;
}


//
// The following 'thread API' are not considerd 'handle-based' for the thread object might have
// already been destroyed, while the handle is still valid.
//


//------------------------------------------------------------------------------
// get thread exit code
//------------------------------------------------------------------------------
BOOL THRDGetCode (HANDLE hth, LPDWORD pdwExit)
{
    PHDATA  phd;
    DWORD   dwErr = ERROR_INVALID_HANDLE;
    DEBUGMSG(ZONE_ENTRY,(L"THRDGetCode entry: %8.8lx %8.8lx\r\n", hth, pdwExit));
    phd = LockHandleData (hth, pActvProc);
    if (phd) {
        __try {
            if (&cinfThread == phd->pci) {
                PTHREAD pth = (PTHREAD) phd->dwData;
                *pdwExit = pth->dwExitCode;
                dwErr = 0;
            } else if ((&cinfEvent == phd->pci) 
                && (((PEVENT) phd->pvObj)->manualreset & PROCESS_EVENT)) {
                // the pseudo handle returned by CreateProcess
                *pdwExit = phd->dwData;
                dwErr = 0;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = ERROR_INVALID_PARAMETER;
        }
        UnlockHandleData (phd);
    }

    KSetLastError (pCurThread, dwErr);

    DEBUGMSG(ZONE_ENTRY,(L"THRDGetCode exit: dwErr = %8.8lx\r\n", dwErr));
    return !dwErr;
    
}

//------------------------------------------------------------------------------
// get thread time statisitics
//------------------------------------------------------------------------------
BOOL THRDGetTimes (HANDLE hth, LPFILETIME lpCreationTime, LPFILETIME lpExitTime, LPFILETIME lpKernelTime, LPFILETIME lpUserTime)
{
    PHDATA  phd;
    DWORD   dwErr = ERROR_INVALID_HANDLE;
    DEBUGMSG(ZONE_ENTRY,(L"THRDGetTimes entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                        hth, lpCreationTime, lpExitTime, lpKernelTime, lpUserTime));
    phd = (GetCurrentThread () == hth)
        ? LockHandleData ((HANDLE) dwCurThId, g_pprcNK)
        : LockHandleData (hth, pActvProc);
    if (phd) {

        if (&cinfThread == phd->pci) {
            PTHREAD pth = (PTHREAD) phd->dwData;

            __try {
                ULONGLONG utime, ktime;

                *lpCreationTime = pth->ftCreate;
                *lpExitTime     = pth->ftExit;
                
                if ((HANDLE) dwCurThId == hth) {
                    utime = SCHL_GetCurThreadUTime ();
                    ktime = SCHL_GetCurThreadKTime ();
                } else {
                    utime = pth->dwUTime;
                    ktime = pth->dwKTime;
                }

                utime *= 10000;
                ktime *= 10000;

                *lpUserTime   = *(LPFILETIME)&utime;
                *lpKernelTime = *(LPFILETIME)&ktime;

                dwErr = 0;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
        }
        UnlockHandleData (phd);
    }

    KSetLastError (pCurThread, dwErr);

    DEBUGMSG(ZONE_ENTRY,(L"THRDGetTimes exit: dwErr = %8.8lx\r\n", dwErr));
    return !dwErr;
}


static void CleanupThreadStruct (PTHREAD pth)
{
    //
    // clean up thread when it failed to finish creation
    //
    if (pth) {
        PPROCESS pprc = pth->pprcOwner;

        DEBUGCHK (pth->tlsNonSecure && pth->tlsSecure);

        if (pth->tlsNonSecure == pth->tlsSecure) {
            // k-mode thread
            VMFreeStack (g_pprcNK, pth->dwOrigBase, pth->dwOrigStkSize);
        } else {
            // user mode thread
            VMFreeAndRelease (pprc, (LPVOID) pth->dwOrigBase, pth->dwOrigStkSize);
            VMFreeStack (g_pprcNK, pth->tlsSecure[PRETLS_STACKBASE], pth->tlsSecure[PRETLS_STACKSIZE]);
        }
        
        if (pth->pCoProcSaveArea) {
            FreeMem (pth->pCoProcSaveArea, g_dwCoProcPool);
        }
        if (pth->pStkForExit) {
            FreeMem (pth->pStkForExit, HEAP_STKLIST);
        }

        LockLoader (pprc);
        RemoveDList (&pth->thLink);
        InterlockedDecrement ((PLONG) &pprc->wThrdCnt);
        UnlockLoader (pprc);
        
        HNDLCloseHandle (g_pprcNK, (HANDLE) pth->dwId);    
    }
}

//------------------------------------------------------------------------------
// THRDCreate - main function to create a thread. thread always created suspended, return locked pHDATA
//------------------------------------------------------------------------------
PHDATA THRDCreate (PPROCESS pprc, FARPROC pfnStart, LPVOID pParam, DWORD cbStack, DWORD mode, DWORD prioAndFlags)
{
    LPVOID pUsrStack = NULL;
    LPVOID pKrnStack = NULL;
    PHDATA phdThrd   = NULL;

    // stack must be multiple of 64k
    DEBUGCHK (!(cbStack & VM_BLOCK_OFST_MASK));

    // kernel thread must be in kernel mode
    DEBUGCHK ((TH_KMODE == mode) || (pprc != g_pprcNK));

    // if creating user mode thread, the process must be the current VM (or stack access can fault)
    DEBUGCHK ((TH_KMODE == mode) || (pprc == pVMProc));

    // only case where we create a kernel mode thread in a process other than kernel is during process creation.
    DEBUGCHK (  (TH_UMODE == mode)
             || (pprc == g_pprcNK)
             || ((FARPROC) CreateNewProc == pfnStart));

    // create thread stack(s)
    if (TH_UMODE == mode) {
        pKrnStack = VMCreateStack (g_pprcNK, KRN_STACK_SIZE);   // create kernel stack
        pUsrStack = VMCreateStack (pprc, cbStack);              // create user stack
    } else {
        pKrnStack = VMCreateStack (g_pprcNK, cbStack);
        pUsrStack = pKrnStack;
    }
    
    if (pUsrStack && pKrnStack) {

        PTHREAD pth = SetupThread (pprc, 
                                  pUsrStack,
                                  cbStack,
                                  pKrnStack,
                                  pfnStart,
                                  pParam,
                                  mode,
                                  prioAndFlags);
        if (pth) {
            phdThrd = LockHandleData ((HANDLE) pth->dwId, g_pprcNK);
            DEBUGCHK (GetThreadPtr (phdThrd) == pth);
        }
    }

    if (!phdThrd) {
        // free user mode stack, if created
        if (TH_UMODE == mode) {
            if (pUsrStack) {
                VMFreeAndRelease (pprc, pUsrStack, cbStack);
            }
            // continue to free kernel stack, which is always fixed sized.
            cbStack = KRN_STACK_SIZE;
        }

        // free kernel mode stack, if created
        if (pKrnStack) {
            VMFreeStack (g_pprcNK, (DWORD) pKrnStack, cbStack);
        }
    }
    return phdThrd;
}


//------------------------------------------------------------------------------
// NKCreateThread - the CreateThread API
//------------------------------------------------------------------------------
HANDLE NKCreateThread (
    LPSECURITY_ATTRIBUTES lpsa,
    DWORD cbStack,
    LPTHREAD_START_ROUTINE lpStartAddr,
    LPVOID lpvThreadParm,
    DWORD fdwCreate,
    LPDWORD lpIDThread
    ) 
{
    PPROCESS pprc = pActvProc;
    HANDLE  hth = NULL;
    PTHREAD pth = NULL;
    DWORD   dwErr = 0;
    DEBUGMSG(ZONE_ENTRY,(L"NKCreateThread entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        lpsa,cbStack,lpStartAddr,lpvThreadParm,fdwCreate,lpIDThread));

    if (((pprc != g_pprcNK) && (g_pNKGlobal->dwMaxThreadPerProc < pprc->wThrdCnt))
        || (PageFreeCount_Pool < g_cMinPageThrdCreate)) {
        dwErr = ERROR_NOT_ENOUGH_MEMORY;

    } else if ((fdwCreate & ~(CREATE_SUSPENDED|STACK_SIZE_PARAM_IS_A_RESERVATION))
                || ((int) cbStack < 0)
                || !lpStartAddr) {
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else {
        DWORD mode = (pprc == g_pprcNK)? TH_KMODE : TH_UMODE;
        PHDATA phdThrd;
        
        // use process'es default stacksize unless specified
        if (!(fdwCreate & STACK_SIZE_PARAM_IS_A_RESERVATION) || !cbStack) {
            cbStack = pprc->e32.e32_stackmax;
        }
        // round the stack size up to the next 64k
        cbStack = (cbStack + VM_BLOCK_OFST_MASK) & ~VM_BLOCK_OFST_MASK;

        phdThrd = THRDCreate (pprc, (FARPROC) lpStartAddr, lpvThreadParm, cbStack, mode, THREAD_RT_PRIORITY_NORMAL);

        if (phdThrd) {
            pth = GetThreadPtr (phdThrd);
            // It's possible that the above GetThreadPtr return NULL (we actullay hit this in stress test)
            // 1) preempted right after THRDCreate
            // 2) a high prio thread terminate the process, which terminates all threads.
            // 3) the thread just created fully exited due to termination.
            if (pth) {
                hth = HNDLDupWithHDATA (pprc, phdThrd, &dwErr);
            }
        }

        UnlockHandleData (phdThrd);

        if (!hth) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;

        } else if (lpIDThread && !CeSafeCopyMemory (lpIDThread, &pth->dwId, sizeof (DWORD))) {
            dwErr = ERROR_INVALID_PARAMETER;
            
        } else if (pprc->pDbgrThrd) {
            DbgrInitThread(pth);
        }
        
    }

    if (dwErr) {

        CleanupThreadStruct (pth);

        if (hth) {
            HNDLCloseHandle (pprc, hth);
        }
        
        hth = NULL;
        KSetLastError (pCurThread, dwErr);
        
    } else {

        NKPSLNotify (DLL_THREAD_ATTACH, pprc->dwId, pth->dwId);

        // resume thread if not create-suspended
        if (!(fdwCreate & CREATE_SUSPENDED)) {
            THRDResume (pth, NULL);
        }
    }

    DEBUGMSG(ZONE_ENTRY,(L"NKCreateThread exit: dwErr = %8.8lx\r\n", dwErr));
    return hth;
}

//------------------------------------------------------------------------------
// NKOpenThread - given thread id, get the handle to the thread.
//
// Note: currently supports only 0 or full access modes.
//------------------------------------------------------------------------------
HANDLE NKOpenThread (DWORD dwAccessRequested, BOOL fInherit, DWORD dwThreadId)
{
    DWORD dwErr = 0;
    HANDLE hRet = NULL;
    PPROCESS pprcCurrent = pActvProc;
    PHDATA phdTarget = LockHandleData ((HANDLE) dwThreadId, g_pprcNK); // thread we want to open
    PTHREAD pthTarget = GetThreadPtr (phdTarget);

    if (fInherit 
        || !pthTarget 
        || (dwAccessRequested && (dwAccessRequested != THREAD_ALL_ACCESS))) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        dwAccessRequested = THREAD_ALL_ACCESS;

        if (phdTarget->dwAccessMask == dwAccessRequested) {
            // original handle has the same access mask as the callers request; just duplicate the handle
            dwErr = HNDLDuplicate (g_pprcNK, (HANDLE) dwThreadId, pprcCurrent, &hRet);

        } else {
            dwErr = ERROR_INVALID_ACCESS;
            DEBUGCHK (0); // policy is granting other than full access! not supported currently.
        }
    }

    UnlockHandleData (phdTarget);
    
    if (dwErr) {
        DEBUGCHK (!hRet);
        SetLastError(dwErr);
    }
    
    return hRet;
}

//------------------------------------------------------------------------------
// CreateKernelThread - create a kernel thread
//------------------------------------------------------------------------------
HANDLE CreateKernelThread (LPTHREAD_START_ROUTINE lpStartAddr, LPVOID lpvThreadParm, WORD prio, DWORD dwFlags) 
{
    PHDATA phdThrd = THRDCreate (g_pprcNK, (FARPROC) lpStartAddr, lpvThreadParm, KRN_STACK_SIZE, TH_KMODE, prio);
    HANDLE hTh     = NULL;

    if (phdThrd) {

        DWORD dwErr = 0;
        PTHREAD pth = GetThreadPtr (phdThrd);

        DEBUGCHK (pth);
        hTh = HNDLDupWithHDATA (g_pprcNK, phdThrd, &dwErr);

        if (!hTh) {
            CleanupThreadStruct (pth);

        } else if (!(dwFlags & CREATE_SUSPENDED)) {
            THRDResume (pth, NULL);
        }

        UnlockHandleData (phdThrd);
    }

    DEBUGMSG (ZONE_SCHEDULE, (L"CreateKernelThread returns %8.8lx\r\n", hTh));

    return hTh;
}


BOOL EnumKillThread (PDLIST pItem, LPVOID pEnumData)
{
    PTHREAD  pth  = (PTHREAD) pItem;
    if (IsMainThread (pth)) {
        return TRUE;
    }
    KillOneThread (pth, 0);
    return FALSE;
}

void NKPrepareThreadExit (DWORD dwExitCode)
{
    PTHREAD pth = pCurThread;

    if (pth->pcstkTop && pth->pcstkTop->pcstkNext) {
        RETAILMSG (1, (L"NKExitThread: Calling ExitThread (%8.8lx) in callback/PSL context, treat it as TerminateThread\r\n", pth->dwId));
        // PSL call chain not empty
        KillOneThread (pth, dwExitCode);

    } else {
        PPROCESS pprc = pActvProc;

        DEBUGCHK (pprc == pth->pprcOwner);

        if (IsMainThread (pth)) {

            DWORD i;

            pprc->bState = PROCESS_STATE_START_EXITING;
            LockLoader (pprc);

            DEBUGCHK (!IsDListEmpty (&pprc->thrdList));
            
            // kill all the secondary threads
            EnumerateDList (&pprc->thrdList, EnumKillThread, NULL);
            
            UnlockLoader (pprc);

            // wait for all the secondary terminated, and any in-progress notfication
            // for process exiting to finish.
            // NOTE: as we've already changed the process to start-exiting, there will not 
            //       be any  DLL_PROCESS_EXITING notifcation except for the in-flight one
            //       beyond this point.
            for (i = 1; !IsAll2ndThrdExited (pprc) || pprc->fNotifyExiting; i ++) {
                if (i == 2) {
                    NKPSLNotify (DLL_PROCESS_EXITING, pprc->dwId, pth->dwId);
                }
                NKSleep (i * 250);
            }

        }      
    }
}


//------------------------------------------------------------------------------
// NKExitThread - current thread exiting
//------------------------------------------------------------------------------
void NKExitThread (DWORD dwExitCode) 
{
    HANDLE   hTh                    = (HANDLE) dwCurThId;
    PTHREAD  pth                    = pCurThread;
    PPROCESS pprcOwner              = pth->pprcOwner;
    BOOL     fIsMainThread          = IsMainThread (pth);
    LPBYTE   pCoProcSaveArea        = pth->pCoProcSaveArea;
    PSTUBTHREAD pStubThread         = NULL;
    PHDATA   phdThrd                = LockHandleData (hTh, g_pprcNK);
    BOOL     fNotify                = TRUE;
    PMUTEX   pMutex;
    DWORD    dwReason;
    
    DEBUGMSG (ZONE_SCHEDULE,(L"NKExitThread entry: pCurThread = %8.8lx, dwExitCode = %8.8lx\r\n", pth, dwExitCode));

    DEBUGCHK (pActvProc == pth->pprcOwner);

    if (pth->pcstkTop && pth->pcstkTop->pcstkNext) {
        RETAILMSG (1, (L"NKExitThread: Calling ExitThread (%8.8lx) in callback/PSL context, treat it as TerminateThread\r\n", pth->dwId));
        // PSL call chain not empty
        KillOneThread (pth, dwExitCode);
        return;
    }

    DEBUGCHK (!(pth->tlsSecure[TLSSLOT_KERNEL] & TLSKERN_THRDEXITING));
    pth->tlsSecure[TLSSLOT_KERNEL] |= TLSKERN_THRDEXITING;

    if (GET_DYING(pth))
        dwExitCode = pth->dwExitCode;
    else
        pth->dwExitCode = dwExitCode;
    
    SET_DEAD(pth);
    DEBUGCHK(!GET_BURIED(pth));
    
    if ((pprcOwner == g_pprcNK) 
        || (pth->tlsSecure[TLSSLOT_KERNEL] & TLSKERN_TLSCALLINPSL)) {

        // kernel mode thread or user mode thread on which
        // TLS call was made while in kernel mode context.
        PPROCESS CurrentProcess = SwitchActiveProcess (g_pprcNK);        
        DllThreadNotify (DLL_THREAD_DETACH);
        SwitchActiveProcess (CurrentProcess);
    }

    if (fIsMainThread) {
        fNotify = !IsProcessStartingUp (pprcOwner);
        pprcOwner->bState = PROCESS_STATE_START_EXITING;
        WaitForCallerLeave (pprcOwner);

        // All 2nd threads should've already exited unless someone calls NKExitThread directly (malicious).
        // In this case, we'll simply kill all other threads and wait for them to exit. DLL Notification won't
        // be processed.
        NKPrepareThreadExit (dwExitCode);

        pprcOwner->phdProcEvt->dwData = dwExitCode; // set process exit code
        
        dwReason = DLL_PROCESS_DETACH;
        
    } else {
        //
        // Free excess stacks if we've cached too many of them
        // 
        dwReason = DLL_THREAD_DETACH;
    }

    //
    // inform the app debugger of process/thread detach
    //
    NKDebugNotify(dwReason, dwExitCode);

    // remove any app debugger support from this thread;
    // this is a no-op if current thread is not a debugger
    DbgrDeInitDebuggerThread (pth);

    // get exit time - ftExit needs to be valid before the waiting thread is signaled
    NKGetTimeAsFileTime (&pth->ftExit, TM_SYSTEMTIME);
    if (!pth->ftExit.dwLowDateTime)
        pth->ftExit.dwLowDateTime = 1; // has to be non-zero to denote exiting thread
    if (IsMainThread (pth)) {
        pprcOwner->ftExit = pth->ftExit;
    }

    // Synchronize this thread exit with another thread 
    // potentially in the middle of wait on this thread.
    // This has to be done after ftExit for this thread
    // is set.
    AcquireSchedulerLock (0);
    ReleaseSchedulerLock (0);

    // At this point any calls to WaitForSingleObject on
    // this thread will return immediately with the right
    // exit code already set.

    // signal all threads waiting for us to exit.
    SignalAllBlockers (&pth->pBlockers);

    //
    // free excess cached kernel stacks
    //
    VMFreeExcessStacks (MAX_STK_CACHE);

    //
    // clean up delayed free threads
    // 
    CleanDelayFreeThread ();
    
    if (pth == pOOMThread)
        pOOMThread = 0;

    // we cannot be blocking...
    DEBUGCHK(!pth->lpProxy);

    //
    // Notify PSL of process/thread exit
    //
    if (fNotify) {
        NKPSLNotify (dwReason, dwActvProcId, dwCurThId);
    }    

#ifndef SHIP_BUILD
    // check for non-zero tls values
    if (g_fCheckForTlsLeaks) {
        int i;
        for (i = TLSSLOT_NUMRES; i < TLS_MINIMUM_AVAILABLE; ++i) {
            if (IsKModeAddr(pth->tlsSecure[i])) {
                NKDbgPrintfW (L"Potential memory leak (tls item not freed on thread exit). tid: 0x%8.8lx, slot: %d, value: 0x%8.8lx\r\n", pth, i, pth->tlsSecure[i]);
                DebugBreak();
                break;
            }
        }
    }
#endif

    if (fIsMainThread) {
        PEVENT  pEvt = GetEventPtr (pprcOwner->phdProcEvt);

        DEBUGCHK (pEvt);

        // remove the process from the PSL servers list if this is a PSL server
        if (pprcOwner->pfnTlsCleanup) {
            RemovePslServer (pprcOwner->dwId);
        }

        // process terminating --> remove debugger support from this process
        // do it before we unload all implicit libs as FreeAll... holds the process
        // loader critical section and we will get into deadlock if we detach after
        // that call.
        DbgrDeInitDebuggeeProcess (pprcOwner);
        
        // unmap all views
        MAPUnmapAllViews (pprcOwner);
        
        // close all handles in the process
        HNDLCloseAllHandles (pprcOwner);

        // free all libraries loaded
        FreeAllProcLibraries (pprcOwner);

        // freeing locked page list
        VMFreeLockPageList (pprcOwner);
        
        (*HDModUnload) ((DWORD) pprcOwner, TRUE);

        // Decommit each executable section that uses the page pool, returning
        // memory to the pool.  The reservation and non-pool pages will be
        // cleaned up in VMDelete.
        DecommitExe (pprcOwner, &pprcOwner->oe, &pprcOwner->e32, pprcOwner->o32_ptr);

        // close the executable
        CloseExe (&pprcOwner->oe);

        // free the pStdNames
        if (pprcOwner->pStdNames[0])     FreeName (pprcOwner->pStdNames[0]);
        if (pprcOwner->pStdNames[1])     FreeName (pprcOwner->pStdNames[1]);
        if (pprcOwner->pStdNames[2])     FreeName (pprcOwner->pStdNames[2]);

        // free the vm allocation list (not the VM, they'll be freed when we destroy vm)
        VMFreeAllocList (pprcOwner);

        // wakeup all threads waiting on the process
        pEvt->manualreset |= PROCESS_EXITED;
        VERIFY (!ForceEventModify (pEvt, EVENT_SET));  // signal the event (ForceEventModify return error code)

        // pprcOwner object can be destroyed, save pStubThread before the object got deleted
        pStubThread = pprcOwner->pStubThread;

    }

    // freeup all callstack structure
    CleanupAllCallstacks (pth);
    
    DEBUGMSG (ZONE_SCHEDULE, (L"%s Thread %8.8lx of process '%s' exiting\r\n", fIsMainThread? L"Main" : L"Secondary", pth->dwId, pprcOwner->lpszProcName));

    CELOG_ThreadTerminate(pth);
    g_pOemGlobal->pfnNotifyThreadExit (pth->dwId, dwExitCode);

    // clear the co-proc save area, we should never have to touch co-proc again
    pth->pCoProcSaveArea = NULL;
    if (pCoProcSaveArea) {
        FreeMem (pCoProcSaveArea, g_dwCoProcPool);
    }

    // celog must be done before we close the thread handle as it migh cause paging and use CS.
    CELOG_ThreadDelete(pth);

    if (fIsMainThread) {

        HANDLE hProcQuery = pprcOwner->hProcQuery;
        PHDATA phdProc = LockHandleData ((HANDLE) pprcOwner->dwId, g_pprcNK);
        
        // main thread: 
        DEBUGCHK (pprcOwner != g_pprcNK);

        // reduce the handle cnt for the query handle
        if (hProcQuery) {
            pprcOwner->hProcQuery = NULL;
            HNDLCloseHandle (g_pprcNK, hProcQuery);
        }        

        DEBUGMSG (ZONE_SCHEDULE, (L"Removing Process %d from process list\r\n", pprcOwner->dwId));
        // remove the process from the process list
        LockModuleList ();
        PagePoolDeleteProcess (pprcOwner);
        RemoveDList (&pprcOwner->prclist);
        UnlockModuleList ();

        // remove the thread from process's thread list
        RemoveThreadFromProc (pprcOwner, pth);

        // notify other CPU of process removal
        SendIPI (IPI_INVALIDATE_VM, (DWORD) pprcOwner);

        DEBUGMSG (ZONE_SCHEDULE, (L"Destroying Handle Table of process %8.8lx\r\n", pprcOwner->dwId));
        // destroy the handle table
        HNDLDeleteHandleTable (pprcOwner);

        CELOG_ProcessTerminate(pprcOwner);

        DEBUGMSG (ZONE_SCHEDULE, (L"Decrement handle count of Process, id %8.8lx\r\n", pprcOwner->dwId));

        // The refcnt of the "process id" handle has at least a handle and a lock at this point. 
        // here we'll remove the "handle count" which can in-turn trigger pre-close if there is no open handle, 
        // and destroy the "process id" handle. If there is any existing opened handle, the "process id" handle
        // will not be destroyed. We cannot delete the VM of the process yet since an open handle to a process
        // can lead to VM access. We need to keep the process VM till the last handle to the process is closed.
        DEBUGCHK (phdProc->dwRefCnt > HNDLCNT_INCR);
        DoUnlockHDATA (phdProc, -HNDLCNT_INCR);

        pprcOwner = NULL;   // can't access pprcOwner from here on, for the process object can be destroyed.

        DEBUGMSG (ZONE_MEMORY, (L"", ShowKernelHeapInfo ()));

    } else {

        // remove the thread from process's thread list
        RemoveThreadFromProc (pprcOwner, pth);

        // since we're going to be in the kernel from now on, the non-secure stack can be freed
        if (pth->tlsSecure != pth->tlsNonSecure) {

            DEBUGCHK (pth->tlsPtr == pth->tlsSecure);

            pth->tlsNonSecure = NULL;       // can't reference it anymore
            DEBUGMSG (ZONE_SCHEDULE, (L"Freeing non-secure stack at %8.8lx, size %8.8lx\n", pth->dwOrigBase, pth->dwOrigStkSize));

            VMFreeStack (pprcOwner, pth->dwOrigBase, pth->dwOrigStkSize);
        }

    }

    DEBUGMSG (ZONE_SCHEDULE, (L"Entering Final stage of thread removal, thread id = %8.8lx\r\n", pth->dwId));

    // release all held sync objects owned by current thread and set the thread state to burried.
    // NOTE: we do not change the fast-path CS owner from user mode here (lpcs). Whichever thread 
    //       waken up will update the  owner field, or the next trap into kernel will take care of it.
    while (NULL != (pMutex = SCHL_PreAbandonMutex (pth))) {
        GiveupMutex (pMutex, IsKModeAddr ((DWORD) pMutex->lpcs)? pMutex->lpcs : 0);
        DEBUGCHK (!IsKernelVa (pMutex->lpcs));
    }

    // decrement # of threads of the process
    if (pprcOwner) {
        InterlockedDecrement ((PLONG) &pprcOwner->wThrdCnt);
    }
    
    KCall ((PKFN) SCHL_FinalRemoveThread, phdThrd, pStubThread, fIsMainThread);
    DEBUGCHK(0);
}

THREAD dummyThread;

//------------------------------------------------------------------------------
// THRDInit - initialize thread handling (called at system startup)
//------------------------------------------------------------------------------
void THRDInit (void) 
{
    LPBYTE      pStack;
    PTHREAD     pth = &dummyThread;
    PTWO_D_NODE pthNode;
    PPCB        ppcb = GetPCB ();
    DWORD       dummy;

    DEBUGLOG (ZONE_SCHEDULE, g_pprcNK);

    pth->pprcActv = pth->pprcOwner = pth->pprcVM = g_pprcNK;
    pth->dwId = DUMMY_THREAD_ID;

    // don't allow thread create one memory drop below 1% available
    if (g_cMinPageThrdCreate < PageFreeCount / 100) {
        g_cMinPageThrdCreate = PageFreeCount / 100;
    }
    
    // map W32 thread priority if OEM choose to
    if (g_pOemGlobal->pfnMapW32Priority) {
        BYTE prioMap[MAX_WIN32_PRIORITY_LEVELS];
        int  i;
        memcpy (prioMap, W32PrioMap, sizeof (prioMap));
        g_pOemGlobal->pfnMapW32Priority (MAX_WIN32_PRIORITY_LEVELS, prioMap);
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

    // allocate memory for the 1st thread
    pth = AllocMem (HEAP_THREAD);
    DEBUGCHK (pth);
    memset (pth, 0, sizeof(THREAD));
    ppcb->CurThId = (DWORD) HNDLCreateHandle (&cinfThread, pth, g_pprcNK, THREAD_ALL_ACCESS) & ~1;
    ppcb->pCurThd = pth;

    DEBUGCHK (ppcb->CurThId);

    InitThreadStruct (pth, (HANDLE) ppcb->CurThId, g_pprcNK, THREAD_RT_PRIORITY_ABOVE_NORMAL);
    pth->dwId = ppcb->CurThId;
    ppcb->OwnerProcId = pth->pprcOwner->dwId;

    if (g_pOemGlobal->cbCoProcRegSize) {

        DEBUGCHK (g_pOemGlobal->pfnInitCoProcRegs);
        DEBUGCHK (g_pOemGlobal->pfnSaveCoProcRegs);
        DEBUGCHK (g_pOemGlobal->pfnRestoreCoProcRegs);

        // check the debug register related values.
        if (g_pOemGlobal->cbCoProcRegSize > MAX_COPROCREGSIZE) {
            g_pOemGlobal->cbCoProcRegSize = g_pOemGlobal->fSaveCoProcReg = 0;
        } else {
            PNAME pTmp = AllocName (g_pOemGlobal->cbCoProcRegSize);
            DEBUGCHK (pTmp);
            g_dwCoProcPool = pTmp->wPool;
            FreeName (pTmp);
        }
    } else {
        g_pOemGlobal->fSaveCoProcReg = FALSE;
    }
    DEBUGMSG (ZONE_SCHEDULE,(TEXT("cbCoProcRegSize = %d\r\n"), g_pOemGlobal->cbCoProcRegSize));

    AddToDListHead (&g_pprcNK->thrdList, &pth->thLink);
    g_pprcNK->wThrdCnt ++;

#ifdef SHx
    SetCPUGlobals();
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_ALL);
#endif


    if (!OpenExecutable (NULL, TEXT("NK.EXE"), &g_pprcNK->oe, 0)) {
        LoadE32 (&g_pprcNK->oe, &g_pprcNK->e32, &dummy, &dummy, 0);
        g_pprcNK->BasePtr = (LPVOID)g_pprcNK->e32.e32_vbase;
    }
    
    // create/setup stack
    pStack = VMCreateStack (g_pprcNK, KRN_STACK_SIZE);
    pth->dwOrigBase = (DWORD) pStack;
    pth->dwOrigStkSize = KRN_STACK_SIZE;
    pth->tlsSecure = pth->tlsNonSecure = pth->tlsPtr = TLSPTR (pStack, KRN_STACK_SIZE);

    // Save off the thread's program counter for getting its name later.
    pth->dwStartAddr = (DWORD) SystemStartupFunc;

    MDSetupThread (pth, (FARPROC)SystemStartupFunc, 0, TH_KMODE, 0);

    CELOG_ThreadCreate(pth, g_pprcNK, NULL);

    // make pth the head of runable and update the hash table
    pthNode = QNODE_OF_THREAD (pth);

    RunList.pHead = pthNode->pQUp = pthNode->pQDown = RunList.Hash[THREAD_RT_PRIORITY_ABOVE_NORMAL/PRIORITY_LEVELS_HASHSCALE] = pthNode;
    g_pNKGlobal->dwCntRunnableThreads = 1;
    SetReschedule(ppcb);

    DEBUGMSG(ZONE_SCHEDULE,(TEXT("Scheduler: Created master thread %8.8lx\r\n"),pth));

}
