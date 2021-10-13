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

#define GTC_VALID_FLAGS     (STACKSNAP_FAIL_IF_INCOMPLETE | STACKSNAP_EXTENDED_INFO | STACKSNAP_INPROC_ONLY | STACKSNAP_RETURN_FRAMES_ON_ERROR)

#define MAX_SKIP            0x10000     // skipping 64K frames? I don't think so
#define MAX_NUM_FRAME       0x10000     // max frame 64K

DWORD g_dwCoProcPool;

static ULONG EXTGetCallStack (PTHREAD pth, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip)
{
    DWORD  dwFrameSize = ((STACKSNAP_EXTENDED_INFO & dwFlags)? sizeof (CallSnapshotEx) : sizeof (CallSnapshot));
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
        ) {                                   
        dwErr = ERROR_INVALID_PARAMETER;
    } else if ((dwTotalSize > sizeof (_local))
                && !(pFrameCopy = VMAlloc (g_pprcNK, NULL, dwTotalSize, MEM_COMMIT, PAGE_READWRITE))) {
        dwErr = ERROR_NOT_ENOUGH_MEMORY;
    } else if ((dwRtn = THRDGetCallStack (pth, dwMaxFrames, pFrameCopy, dwFlags, dwSkip+1))
                && !CeSafeCopyMemory (lpFrames, pFrameCopy, dwRtn * dwFrameSize)) {
        dwErr = ERROR_INVALID_PARAMETER;
        dwRtn = 0;
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
};

static const ULONGLONG thrdSigs[] = {
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
};

ERRFALSE (sizeof(ThrdExtMthds) == sizeof(ThrdIntMthds));
ERRFALSE ((sizeof(ThrdExtMthds) / sizeof(ThrdExtMthds[0])) == (sizeof(thrdSigs) / sizeof(thrdSigs[0])));

const CINFO cinfThread = {
    "THRD",
    DISPATCH_KERNEL_PSL,
    SH_CURTHREAD,
    sizeof(ThrdExtMthds)/sizeof(ThrdExtMthds[0]),
    ThrdExtMthds,
    ThrdIntMthds,
    thrdSigs,
    0,
    0,
    0,
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
    PCRIT               pCrit;
    LPCRITICAL_SECTION  lpcs;

    DEBUGCHK (pth == pCurThread);
    
    LockLoader (pproc);
    RemoveDList (&pth->thLink);
    // NOTE: RemoveThreadFromProc doesn't decrement thread count to avoid race in NKExitThread.
    //       It will be decremented before calling FinalRemoveThread to ensure thread counts 
    //       matches # of threads in a process.
    UnlockLoader (pproc);

    // release all held sync objects owned by current thread and set the thread state to burried.
    // NOTE: (1) Thread state set to BURIED after the 1st call to GetOwnedSyncObject.
    //       (2) Due to CS fast-path, the code can access user mode address. Therefore the code
    //           must be called with correct VM.
    while (pCrit = (PCRIT)KCall ((PKFN)GetOwnedSyncObject)) {
        if (ISMUTEX(pCrit))
            DoLeaveMutex((PMUTEX)pCrit);
        else {
            // if we except here, the memory pointed by lpcs had already being decommitted.
            // most likely due to freeing a library while there is a thread blocking on a
            // CS declared in the library.
            __try {
                lpcs = pCrit->lpcs;
                lpcs->LockCount = 1;
                // DO NOT USE LeaveCriticalSection here. The CS can be a user CS, and LeaveCriticalSection
                // will assume it's a kernel CS, and treat the lpcs->hCrit as a pointer...
                lpcs->OwnerThread = (HANDLE) ((DWORD)lpcs->OwnerThread | 0x1);
                CRITLeave (pCrit, lpcs);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }

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
    // clear pTh contents in KCall, in case pTh is reused and its previous session is still referred by sleeper_t or synobj handling.
    KCall ((PKFN)ClearThreadStruct, pTh);

    HNDLSetUserInfo (hTh, (DWORD) pTh, g_pprcNK);
    pTh->dwExitCode     = STILL_ACTIVE;
    pTh->pprcActv       = pTh->pprcOwner = pProc;
    pTh->dwId           = (DWORD) hTh & ~1;
    pTh->bWaitState     = WAITSTATE_SIGNALLED;
    pTh->dwQuantLeft    = pTh->dwQuantum = g_pOemGlobal->dwDefaultThreadQuantum;
    // record thread creation time
    SET_BPRIO(pTh,prioAndFlags);
    SET_CPRIO(pTh,prioAndFlags);
    pTh->hTok = pProc->hTok;
    InitDList (&pTh->thLink);
    if (g_pprcNK == pProc) {
#ifdef x86        
        pTh->lpFPEmul = g_pKCoredllFPEmul;
#endif
        SET_TIMEKERN (pTh);
    } else {
    #ifdef x86
        pTh->lpFPEmul = g_pCoredllFPEmul;
    #endif
        pTh->pprcVM = pProc;
    }
}

//------------------------------------------------------------------------------
// THRDCreate - main function to create a thread. thread always created suspended
//------------------------------------------------------------------------------
PTHREAD THRDCreate(
    PPROCESS pprc,          // which process
    LPBYTE  lpStack,        // stack
    DWORD   cbStack,        // stack size
    LPBYTE  pSecureStk,     // secure stack
    LPVOID  lpStart,        // entry point
    LPVOID  param,          // parameter to thread function
    ulong   mode,           // kernel/user mode
    DWORD   prioAndFlags    // priority and flag
    ) 
{
    PTHREAD     pth         = NULL;
    LPBYTE      pCoProcSaveArea = NULL;
    PSTKLIST    pStkList;
    HANDLE      hth = NULL;
    
    DEBUGMSG(ZONE_SCHEDULE,(TEXT("+ THRDCreate: (%8.8lx, %8.8lx, %8.8lx, %8.8lx %8.8lx, %8.8lx, %8.8lx, %8.8lx)\r\n"),
            pprc, pprc->dwId,lpStack, cbStack, pSecureStk, lpStart, param, prioAndFlags));

    // call OEM to initlized co-proc save area for debug registers if required
    if (g_pOemGlobal->cbCoProcRegSize) {
        pCoProcSaveArea = (LPBYTE) AllocMem (g_dwCoProcPool);
        if (!pCoProcSaveArea) {
            DEBUGMSG (1, (L"THRDCreate failed: unable to allocate co-proc save area\r\n"));
            return NULL;
        }
        g_pOemGlobal->pfnInitCoProcRegs (pCoProcSaveArea);
    }


    if ((pStkList = (PSTKLIST) AllocMem (HEAP_STKLIST))                     // allocate the stklist (16 bytes) used for thread exit
        && (pth = AllocMem (HEAP_THREAD))                                   // thread structure available?
        && (hth = HNDLCreateHandle (&cinfThread, pth, g_pprcNK))) {         // thread handle available?

        DEBUGMSG (ZONE_SCHEDULE, (L"THRDCreate: pth = %8.8lx, hth = %8.8lx\r\n", pth, hth));

        // initialize thread structure
        InitThreadStruct (pth, hth, pprc, prioAndFlags);
        pth->pCoProcSaveArea = pCoProcSaveArea;
        pth->pStkForExit = pStkList;

        NKGetTimeAsFileTime (&pth->ftCreate, TM_SYSTEMTIME);

        // Save off the thread's program counter for getting its name later.
        pth->dwStartAddr = (DWORD) lpStart;
        // save the original stack base/bound
        pth->dwOrigBase = (DWORD) lpStack;
        pth->dwOrigStkSize = cbStack;

        // initialize TLS info
        pth->tlsPtr = pth->tlsNonSecure = TLSPTR (lpStack, cbStack);
        pth->tlsSecure = pSecureStk? TLSPTR(pSecureStk, KRN_STACK_SIZE) : pth->tlsPtr;

        pth->bSuspendCnt = 1;       // suspended the new thread first
        SET_RUNSTATE(pth,RUNSTATE_BLOCKED);
        
        // perform machine dependent thread initialization
        if (prioAndFlags & THRD_NO_BASE_FUNCITON)
            MDSetupThread(pth, (LPVOID)lpStart, param, mode, (ulong)param);
        else
            MDSetupThread(pth, (LPVOID) (IsKModeAddr((DWORD)lpStart)? KTBFf : TBFf),
                            lpStart, mode, (ulong)param);

        DEBUGCHK(!((pth->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));

        // AddThreadToProc failed if the process is exiting
        if (AddThreadToProc (pprc, pth)) {
            CELOG_ThreadCreate(pth, pprc, param);

            DEBUGMSG (ZONE_SCHEDULE, (L"THRDCreate returns %8.8lx, id = %8.8lx\r\n", pth, pth->dwId));
            return pth;
        }

        // thread creation failed. Close the "thread-id" handle.
        HNDLCloseHandle (g_pprcNK, hth);
        pth = NULL;         // HNDLCloseHandle will free the thread memory.  Set to NULL so we don't free twice
    }

    // Thread creation failed, free memory
    DEBUGMSG (ZONE_SCHEDULE, (L"THRDCreate Failed\r\n"));

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
    DWORD retval;

    DEBUGCHK(lpRetVal);
    DEBUGMSG(ZONE_ENTRY,(L"THRDSuspend entry: %8.8lx\r\n",pth));
    if (pth == pCurThread) {
        SET_USERBLOCK (pth);
    }
    retval = ((DWORD (*) (PKFN, ...)) KCall) ((PKFN)ThreadSuspend, pth, TRUE);
    if (pth == pCurThread) {
        CLEAR_USERBLOCK (pth);
    }
    if ((int) retval < 0) {
        KSetLastError (pCurThread, (0xffffffff == retval)? ERROR_SIGNAL_REFUSED : ERROR_INVALID_PARAMETER);
        retval = 0xffffffff;
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
    retval = KCall ((PKFN)ThreadResume, pth);
    DEBUGMSG(ZONE_ENTRY,(L"THRDResume exit: %8.8lx\r\n",retval));

    if (lpRetVal)
        *lpRetVal = retval;
        
    return TRUE;
}

//------------------------------------------------------------------------------
// THRDSetPrio - Set thread priority
//------------------------------------------------------------------------------
BOOL THRDSetPrio (PTHREAD pth, DWORD prio)
{
    BOOL fRet = TRUE;
    
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetPrio entry: %8.8lx %8.8lx\r\n",pth,prio));

    if ((prio >= MAX_WIN32_PRIORITY_LEVELS) 
        || !KCall ((PKFN)SetThreadBasePrio, pth, W32PrioMap[prio])) {
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    }

    DEBUGMSG(ZONE_ENTRY,(L"THRDSetPrio exit: %8.8lx\r\n", fRet));
    return fRet;
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
    if (CeSafeCopyMemory (&ctx, lpContext, sizeof (CONTEXT))) {

        // only allow debugger, or threads with system token to set thread context
        if ((TOKEN_SYSTEM != pCurThread->hTok)
            && (pth->pprcOwner->pDbgrThrd != pCurThread)) {
            dwErr = ERROR_ACCESS_DENIED;
            
        } else if (KCall ((PKFN)DoThreadSetContext, pth, &ctx)) {
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
    BOOL    fRet;
    CONTEXT ctx;

    // Privilege check (information disclosure)

    // make a local copy to make sure the the memory is valid
    __try {
        ctx = *lpContext;
        fRet = (BOOL) KCall ((PKFN)DoThreadGetContext, pth, &ctx);
        *lpContext = ctx;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fRet = FALSE;
    }

    if (!fRet) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }

    return fRet;
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
    
        CONTEXT ctx = {0}; //Initialize stack variable so we won't fault in KCall
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

        } else {

            DWORD dwThrdPrio = GET_CPRIO(pth);

            // other threads, get the threads context
            ctx.ContextFlags = CONTEXT_FULL;

            // boost the priority of current thread to be higher than the thread we want to get callstack
            // NOTE: there is still possibility that the thread will run if its priority got inverted by higher priority
            //       threads. Well, unless we always set priority to 0, it doesn't seems to be a way to avoid it.
            //       Since this is a debug api and we can tolerate some error to exist, all we do is trying our best to
            //       avoid race condition.
            if (bPrio >= dwThrdPrio) {
                KCall ((PKFN)SetThreadBasePrio, pCurThread, dwThrdPrio? dwThrdPrio-1 : dwThrdPrio);
            }
                
            // obtain the context of the thread
            KCall ((PKFN)DoThreadGetContext, pth, &ctx);
            
        }

        dwCnt = NKGetThreadCallStack (pth, dwMaxFrames, lpFrames, dwFlags, dwSkip, &ctx);

        // restore priority if we changed it.
        if (GET_BPRIO(pCurThread) != bPrio) {
            KCall ((PKFN)SetThreadBasePrio, pCurThread, bPrio);
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
PTHREAD KillOneThread (PTHREAD pth, DWORD dwExitCode) 
{
    sttd_t sttd;
    
    sttd.pThNeedRun = 0;
    sttd.pThNeedBoost = 0;
    pth = (PTHREAD)  KCall ((PKFN)SetThreadToDie, pth, dwExitCode, &sttd);
    if (sttd.pThNeedBoost)
        KCall ((PKFN)SetThreadBasePrio, sttd.pThNeedBoost, GET_CPRIO(pCurThread));
    if (sttd.pThNeedRun)
        KCall ((PKFN)MakeRunIfNeeded, sttd.pThNeedRun);
    
    return pth;
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
    
        KillOneThread (pth, dwExitCode);

        if (fWait
            && (pth->dwId != dwCurThId)
            && (NKWaitForKernelObject ((HANDLE) pth->dwId, 3000) == WAIT_TIMEOUT)) {
            DEBUGMSG (1, (L"THRDTerminate: Unable to terminate thread %8.8lx\r\n", pth->dwId));
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
// Set thread priority (abs priority value)
//------------------------------------------------------------------------------
BOOL THRDSetPrio256 (PTHREAD pth, DWORD prio) 
{
    BOOL fRet = FALSE;
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetPrio256 entry: %8.8lx %8.8lx\r\n", pth, prio));

    if (prio < (MAX_CE_PRIORITY_LEVELS - MAX_WIN32_PRIORITY_LEVELS)) {
        TRUSTED_API (L"THRDSetPrio256", FALSE);
    }

    if (prio < MAX_CE_PRIORITY_LEVELS) {
        fRet = KCall ((PKFN)SetThreadBasePrio, pth, prio);
    }

    if (!fRet) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetPrio256 exit: %8.8lx\r\n",fRet));
    return fRet;
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
// set thread quantum
//------------------------------------------------------------------------------
BOOL THRDSetQuantum (PTHREAD pth, DWORD dwQuantum)
{
    BOOL fRet = FALSE;

    DEBUGMSG(ZONE_ENTRY,(L"THRDSetQuantum entry: %8.8lx %8.8lx\r\n", pth, dwQuantum));

    TRUSTED_API (L"THRDSetQuantum", FALSE);

    if (!(dwQuantum & 0x80000000)) {
        fRet = KCall ((PKFN) SetThreadQuantum, pth, dwQuantum);
    }
    if (fRet) {
        CELOG_ThreadSetQuantum(pth, dwQuantum);
    } else {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG(ZONE_ENTRY,(L"THRDSetQuantum exit: %8.8lx\r\n", fRet));
    return fRet;
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
    if (phd = LockHandleData (hth, pActvProc)) {
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
                DWORD dwUTime, dwKTime;

                *lpCreationTime = pth->ftCreate;
                *lpExitTime = pth->ftExit;
                
                if ((HANDLE) dwCurThId == hth) {
                    dwUTime = KCall ((PKFN) GetCurThreadUTime);
                    dwKTime = KCall ((PKFN) GetCurThreadKTime);
                } else {
                    dwUTime = pth->dwUTime;
                    dwKTime = pth->dwKTime;
                }

                *(__int64*)lpUserTime = dwUTime;
                *(__int64*)lpUserTime *= 10000;
                *(__int64*)lpKernelTime = dwKTime;
                *(__int64*)lpKernelTime *= 10000;

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
        if (pth->pCoProcSaveArea) {
            FreeMem (pth->pCoProcSaveArea, g_dwCoProcPool);
        }
        if (pth->pStkForExit) {
            FreeMem (pth->pStkForExit, HEAP_STKLIST);
        }
        HNDLCloseHandle (g_pprcNK, (HANDLE) pth->dwId);
    
    }
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
    LPBYTE  lpStack = NULL, pSecureStk = NULL;
    DWORD   dwErr = 0;
    DEBUGMSG(ZONE_ENTRY,(L"NKCreateThread entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        lpsa,cbStack,lpStartAddr,lpvThreadParm,fdwCreate,lpIDThread));

    if (((pprc != g_pprcNK) && (g_pNKGlobal->dwMaxThreadPerProc < pprc->wThrdCnt))
        || (PageFreeCount < g_cMinPageThrdCreate)) {
        dwErr = ERROR_NOT_ENOUGH_MEMORY;

    } else if ((fdwCreate & ~(CREATE_SUSPENDED|STACK_SIZE_PARAM_IS_A_RESERVATION))
                || ((int) cbStack < 0)
                || !lpStartAddr) {
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else {
        DWORD mode = (pprc == g_pprcNK)? TH_KMODE : TH_UMODE;
        
        // use process'es default stacksize unless specified
        if (!(fdwCreate & STACK_SIZE_PARAM_IS_A_RESERVATION) || !cbStack) {
            cbStack = pprc->e32.e32_stackmax;
        }
        // round the stack size up to the next 64k
        cbStack = (cbStack + VM_BLOCK_OFST_MASK) & ~VM_BLOCK_OFST_MASK;

        // allocate stacks and create thread
        if (!(lpStack = VMCreateStack (pprc, cbStack))                      // create user stack
            || ((pprc != g_pprcNK)
                && !(pSecureStk = VMCreateStack (g_pprcNK, KRN_STACK_SIZE)))    // creat kernel stack if necessary
            || !(pth = THRDCreate (pprc, lpStack, cbStack, pSecureStk, (LPVOID)lpStartAddr, lpvThreadParm, mode, THREAD_RT_PRIORITY_NORMAL))    // create thread
            || (dwErr = HNDLDuplicate (g_pprcNK, (HANDLE) pth->dwId, pprc, &hth))) { // duplicate handle
            dwErr = ERROR_NOT_ENOUGH_MEMORY;

        } else if (lpIDThread) {
            __try {
                *lpIDThread = pth->dwId;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
        }

        // add debug support to the thread
        if (!dwErr && pprc->pDbgrThrd) {
            DbgrInitThread(pth);
        }
    }

    if (dwErr) {

        CleanupThreadStruct (pth);

        if (hth) {
            HNDLCloseHandle (pprc, hth);
        }
        if (lpStack) {
            VMFreeAndRelease (pprc, lpStack, cbStack);
        }
        if (pSecureStk) {
            VMFreeAndRelease (g_pprcNK, pSecureStk, KRN_STACK_SIZE);
        }
        hth = NULL;
        KSetLastError (pCurThread, dwErr);
        
    } else {

        // notify PSL of thread creation
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
//------------------------------------------------------------------------------
HANDLE NKOpenThread (DWORD fdwAccess, BOOL fInherit, DWORD IDThread)
{
    HANDLE hRet = NULL;
    PHDATA phd = LockHandleData ((HANDLE) IDThread, g_pprcNK);

    
    if (!fInherit && GetThreadPtr (phd)) {
        HNDLDuplicate (g_pprcNK, (HANDLE) IDThread, pActvProc, &hRet);
    }

    UnlockHandleData (phd);

    if (!hRet) {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return hRet;
}


//------------------------------------------------------------------------------
// CreateKernelThread - create a kernel thread
//------------------------------------------------------------------------------
HANDLE CreateKernelThread (LPTHREAD_START_ROUTINE lpStartAddr, LPVOID lpvThreadParm, WORD prio, DWORD dwFlags) 
{
    LPVOID lpStack = VMCreateStack (g_pprcNK, KRN_STACK_SIZE);
    HANDLE  hTh    = NULL;
    PTHREAD pth    = NULL;

    if (lpStack
        && (pth = THRDCreate (g_pprcNK, lpStack, KRN_STACK_SIZE, NULL, (LPVOID)lpStartAddr, lpvThreadParm, TH_KMODE, prio))
        && !HNDLDuplicate (g_pprcNK, (HANDLE) pth->dwId, g_pprcNK, &hTh)) {

        // resume the thread if not create suspended
        if (!(dwFlags & CREATE_SUSPENDED)) {
            THRDResume (pth, NULL);
        }
        DEBUGMSG (ZONE_SCHEDULE, (L"CreateKernelThread returns %8.8lx\r\n", hTh));
    } else {

        DEBUGMSG (ZONE_SCHEDULE, (L"CreateKernelThread failed pth = %8.8lx, lpStack = %8.8lx, hTh = %8.8lx\r\n",
            pth, lpStack, hTh));
        CleanupThreadStruct (pth);

        if (lpStack) {
            VMFreeAndRelease (g_pprcNK, lpStack, KRN_STACK_SIZE);
        }

    }
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
    PPROCESS pprc = pActvProc;

    DEBUGCHK (pprc == pCurThread->pprcOwner);

    // set the "dead" bit, such that it won't be terminated again.
    SET_DEAD (pCurThread);

    if (IsMainThread (pCurThread)) {

        if (pprc == pCurThread->pprcOwner) {
            DWORD i;

            pprc->bState = PROCESS_STATE_START_EXITING;
            LockLoader (pprc);

            DEBUGCHK (!IsDListEmpty (&pprc->thrdList));
            
            // kill all the secondary threads
            EnumerateDList (&pprc->thrdList, EnumKillThread, NULL);
            
            UnlockLoader (pprc);

            // wait for all the secondary terminated, the test below
            for (i = 1; !IsAll2ndThrdExited (pprc); i ++) {
                if (i == 2) {
                    NKPSLNotify (DLL_PROCESS_EXITING, dwActvProcId, dwCurThId);
                }
                NKSleep (i * 250);
            }

        } else {
            // ExitThread called on a main thread while inside a PSL call???
            DEBUGCHK (0);
        }
    }      
}


//------------------------------------------------------------------------------
// NKExitThread - current thread exiting
//------------------------------------------------------------------------------
void NKExitThread (DWORD dwExitCode) 
{
    HANDLE   hWake;
    HANDLE   hTh = (HANDLE) dwCurThId;
    PTHREAD  pth = pCurThread;
    PPROCESS pprcOwner = pth->pprcOwner;
    BOOL     fIsMainThread = IsMainThread (pth);
    LPBYTE   pCoProcSaveArea = pth->pCoProcSaveArea;
    PHDATA   phdThrd;
    DWORD    dwReason;
    BOOL     fNotify = TRUE;
    
    DEBUGMSG (ZONE_SCHEDULE,(L"NKExitThread entry: pCurThread = %8.8lx, dwExitCode = %8.8lx\r\n", pth, dwExitCode));

    DEBUGCHK (pActvProc == pth->pprcOwner);

    if (pprcOwner == g_pprcNK) {
        DllThreadNotify (DLL_THREAD_DETACH);
    }

    if (GET_DYING(pth))
        dwExitCode = pth->dwExitCode;
    else
        pth->dwExitCode = dwExitCode;
    
    SET_DEAD(pth);
    DEBUGCHK(!GET_BURIED(pth));

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
    // Notify PSL of process/thread exit
    //
    if (fNotify) {
        NKPSLNotify (dwReason, dwActvProcId, dwCurThId);
    }

    //
    // inform the app debugger of process/thread detach
    //
    NKDebugNotify(dwReason, dwExitCode);

    // remove any app debugger support from this thread;
    // this is a no-op if current thread is not a debugger
    DbgrDeInitDebuggerThread (pth);

    //
    // free excess cached kernel stacks
    //
    VMFreeExcessStacks (MAX_STK_CACHE);

    //
    // clean up delayed free threads
    // 
    CleanDelayFreeThread ();
    
#if defined(x86) || defined(SH4) || defined(MIPS_HAS_FPU)
    if (pth == g_CurFPUOwner)
        g_CurFPUOwner = NULL;
#endif
#if defined(SH3)
    if (pth == g_CurDSPOwner)
        g_CurDSPOwner = NULL;
#endif
    if (pth == pOOMThread)
        pOOMThread = 0;

    DEBUGCHK (!pth->lpce);
    DEBUGCHK(!pth->lpProxy);
    while (pth->pProxList) {
        hWake = 0;
        KCall((PKFN)WakeOneThreadFlat,pth,&hWake);
        if (hWake)
            KCall((PKFN)MakeRunIfNeeded,hWake);
    }

    // get exit time before freeing DLLs because we need to call into coredll
    if (*(__int64 *)&pth->ftCreate)
        NKGetTimeAsFileTime (&pth->ftExit, TM_SYSTEMTIME);

    // clean up token list
    CleanupTokenList (NULL);

    if (fIsMainThread) {
        PEVENT  pEvt = GetEventPtr (pprcOwner->phdProcEvt);

        DEBUGCHK (pEvt);

        // process terminating --> remove debugger support from this process
        // do it before we unload all implicit libs as FreeAll... holds the process
        // loader critical section and we will get into deadlock if we detach after
        // that call.
        DbgrDeInitDebuggeeProcess (pprcOwner);
        
        // free all libraries loaded
        FreeAllProcLibraries (pprcOwner);

        // unmap all views
        MAPUnmapAllViews (pprcOwner);
        
        // close all handles in the process
        HNDLCloseAllHandles (pprcOwner);

        // freeing locked page list
        VMFreeLockPageList (pprcOwner);
        
        (*HDModUnload) ((DWORD) pprcOwner);

        // Decommit each executable section that uses the page pool, returning
        // memory to the pool.  The reservation and non-pool pages will be
        // cleaned up in VMDelete.
        DecommitExe (pprcOwner, &pprcOwner->oe, &pprcOwner->e32, pprcOwner->o32_ptr, TRUE);

        // close the executable
        CloseExe (&pprcOwner->oe);

        // close the process token
        if (TOKEN_SYSTEM != pprcOwner->hTok) {
            HNDLCloseHandle (g_pprcNK, pprcOwner->hTok);
        }
        pth->hTok = pprcOwner->hTok = TOKEN_SYSTEM;
        
        // free the pStdNames
        if (pprcOwner->pStdNames[0])     FreeName (pprcOwner->pStdNames[0]);
        if (pprcOwner->pStdNames[1])     FreeName (pprcOwner->pStdNames[1]);
        if (pprcOwner->pStdNames[2])     FreeName (pprcOwner->pStdNames[2]);

        // free the vm allocation list (not the VM, they'll be freed when we destroy vm)
        VMFreeAllocList (pprcOwner);

        // wakeup all threads waiting on the process
        pEvt->manualreset |= PROCESS_EXITED;
        VERIFY (!ForceEventModify (pEvt, EVENT_SET));  // signal the event (ForceEventModify return error code)

    }

    // freeup all callstack structure
    CleanupAllCallstacks (pth);
    
    DEBUGMSG (ZONE_SCHEDULE, (L"%s Thread %8.8lx of process '%s' exiting\r\n", fIsMainThread? L"Main" : L"Secondary", pth->dwId, pprcOwner->lpszProcName));

    CELOG_ThreadTerminate(pth);

    // celog must be done before we close the thread handle as it migh cause paging and use CS.
    CELOG_ThreadDelete(pth);

    if (fIsMainThread) {

        PHDATA phdProc = LockHandleData ((HANDLE) pprcOwner->dwId, g_pprcNK);
        // main thread: 
        DEBUGCHK (pprcOwner != g_pprcNK);

        DEBUGMSG (ZONE_SCHEDULE, (L"Removing Process %d from process list\r\n", pprcOwner->dwId));
        // remove the process from the process list
        LockModuleList ();
        PagePoolDeleteProcess (pprcOwner);
        RemoveDList (&pprcOwner->prclist);
        UnlockModuleList ();

        // remove the thread from process's thread list
        RemoveThreadFromProc (pprcOwner, pth);

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

            pth->tlsNonSecure = 0;       // can't reference it anymore
            DEBUGMSG (ZONE_SCHEDULE, (L"Freeing non-secure stack at %8.8lx, size %8.8lx\n", pth->dwOrigBase, pth->dwOrigStkSize));

            VMFreeStack (pprcOwner, pth->dwOrigBase, pth->dwOrigStkSize);
        }

    }

    // get the hdata of thread, handle will be closed in FinalRemoveThread
    phdThrd = LockHandleData (hTh, g_pprcNK);
    DEBUGCHK (phdThrd);

    // clear the co-proc save area, we should never have to touch co-proc again
    pth->pCoProcSaveArea = NULL;
    if (pCoProcSaveArea) {
        FreeMem (pCoProcSaveArea, g_dwCoProcPool);
    }

    // decrement # of threads of the process
    if (pprcOwner) {
        InterlockedDecrement ((PLONG) &pprcOwner->wThrdCnt);
    }
    
    DEBUGMSG (ZONE_SCHEDULE, (L"Entering Final stage of thread removal, thread id = %8.8lx\r\n", pth->dwId));
    KCall ((PKFN) FinalRemoveThread, phdThrd);
    DEBUGCHK(0);
}


//------------------------------------------------------------------------------
// THRDInit - initialize thread handling (called at system startup)
//------------------------------------------------------------------------------
void THRDInit (void) 
{
    LPBYTE      pStack;

    DEBUGLOG (1, g_pprcNK);

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
    pCurThread = AllocMem (HEAP_THREAD);
    DEBUGCHK (pCurThread);

    dwCurThId = (DWORD) HNDLCreateHandle (&cinfThread, pCurThread, g_pprcNK) & ~1;
    DEBUGCHK (dwCurThId);

    InitThreadStruct (pCurThread, (HANDLE) dwCurThId, g_pprcNK, THREAD_RT_PRIORITY_ABOVE_NORMAL);

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

    AddToDListHead (&g_pprcNK->thrdList, &pCurThread->thLink);
    g_pprcNK->wThrdCnt ++;


#ifdef SHx
    SetCPUGlobals();
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_ALL);
#endif


    if (!OpenExecutable (NULL, TEXT("NK.EXE"), &g_pprcNK->oe, TOKEN_SYSTEM, NULL, 0)) {
        LoadE32 (&g_pprcNK->oe, &g_pprcNK->e32, 0, 0, 0);
        g_pprcNK->BasePtr = (LPVOID)g_pprcNK->e32.e32_vbase;
        UpdateKmodVSize(&g_pprcNK->oe, &g_pprcNK->e32);
    }
    
    // create/setup stack
    pStack = VMCreateStack (g_pprcNK, KRN_STACK_SIZE);
    pCurThread->dwOrigBase = (DWORD) pStack;
    pCurThread->dwOrigStkSize = KRN_STACK_SIZE;
    pCurThread->tlsSecure = pCurThread->tlsNonSecure = pCurThread->tlsPtr = TLSPTR (pStack, KRN_STACK_SIZE);
    pCurThread->hTok = TOKEN_SYSTEM;

    // Save off the thread's program counter for getting its name later.
    pCurThread->dwStartAddr = (DWORD) SystemStartupFunc;

    MDSetupThread (pCurThread, (LPVOID)SystemStartupFunc, 0, TH_KMODE, 0);

    CELOG_ThreadCreate(pCurThread, g_pprcNK, NULL);

    MakeRun(pCurThread);
    DEBUGMSG(ZONE_SCHEDULE,(TEXT("Scheduler: Created master thread %8.8lx\r\n"),pCurThread));

}
