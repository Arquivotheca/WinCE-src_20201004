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
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    thread.h - internal kernel thread header
//
#ifndef _NK_THREAD_H_
#define _NK_THREAD_H_

#include "kerncmn.h"
#include "apicall.h"
#include "syncobj.h"
#include "handle.h"


/* Thread data structures */
struct _THREAD {
    DLIST       thLink;         /* 00: DList, link all threads in a process */
    PPROXY      pProxList;      /* 08: list of proxies to threads blocked on this thread */
    WORD        wInfo;          /* 0C: various info about thread, see above */
    BYTE        bSuspendCnt;    /* 0E: thread suspend count */
    BYTE        bWaitState;     /* 0F: state of waiting loop */
    PPROCESS    pprcActv;       /* 10: pointer to active (current) process */
    PPROCESS    pprcOwner;      /* 14: pointer to owner process */
    PPROCESS    pprcVM;         /* 18: pointer to process whose VM is used, NULL if no specific VM required (should only for kernel threads) */
    PCALLSTACK  pcstkTop;       /* 1C: current api call info */
    DWORD       dwOrigBase;     /* 20: Original stack base */
    DWORD       dwOrigStkSize;  /* 24: Size of the original thread stack */
    LPDWORD     tlsPtr;         /* 28: tls pointer */
    LPDWORD     tlsSecure;      /* 2c: TLS for secure stack */
    LPDWORD     tlsNonSecure;   /* 30: TLS for non-secure stack */
    PPROXY      lpProxy;        /* 34: first proxy this thread is blocked on */
    DWORD       dwLastError;    /* 38: last error */
    DWORD       dwId;           /* 3C: Thread Id (handle of thread, in NK's handle table */
    BYTE        bBPrio;         /* 40: base priority */
    BYTE        bCPrio;         /* 41: curr priority */
    WORD        wCount;         /* 42: nonce for blocking lists */
    LPBYTE      pCoProcSaveArea;/* 44: co-proc save area */
    PCONTEXT    pSavedCtx;      /* 48: pointer to saved context structure */
    PSTKLIST    pStkForExit;    /* 4c: stklist to link secure stack onto NK's stack list */
    FILETIME    ftCreate;       // 50: creation time - MUST BE 8-byt aligned
    FILETIME    ftExit;         // 58: exit time - MUST BE 8-BYTE aligned
    CPUCONTEXT  ctx;            /* 60: thread's cpu context information */
    PTHREAD     pNextSleepRun;  /* ??: next sleeping thread, if sleeping, else next on runq if runnable */
    PTHREAD     pPrevSleepRun;  /* ??: back pointer if sleeping or runnable */
    CLEANEVENT *lpce;           /* ??: cleanevent for unqueueing blocking lists */
    DWORD       dwStartAddr;    /* ??: thread PC at creation, used to get thread name */
    PTHREAD     pUpRun;         /* ??: up run pointer (circulaar) */
    PTHREAD     pDownRun;       /* ??: down run pointer (circular) */
    PTHREAD     pUpSleep;       /* ??: up sleep pointer (null terminated) */
    PTHREAD     pDownSleep;     /* ??: down sleep pointer (null terminated) */
    PCRIT       pOwnedList;     /* ??: list of crits and mutexes for priority inversion */
    PCRIT       pOwnedHash[PRIORITY_LEVELS_HASHSIZE];
    DWORD       dwWakeupTime;   /* ??: sleep count, also pending sleepcnt on waitmult */
    DWORD       dwQuantum;      /* ??: thread quantum */
    DWORD       dwQuantLeft;    /* ??: quantum left */
    PPROXY      lpCritProxy;    /* ??: proxy from last critical section block, in case stolen back */
    PPROXY      lpPendProxy;    /* ??: pending proxies for queueing */
    DWORD       dwPendReturn;   /* ??: return value from pended wait */
    DWORD       dwPendTime;     /* ??: timeout value of wait operation */
    PTHREAD     pCrabPth;
    WORD        wCrabCount;
    WORD        wCrabDir;
    DWORD       dwPendWakeup;   /* ??: pending timeout */
    WORD        wCount2;        /* ??: nonce for SleepList */
    BYTE        bPendSusp;      /* ??: pending suspend count */
    BYTE        bDbgCnt;        /* ??: recurse level in debug message */
    PCRIT       pLastCrit;      /* ??: Last crit taken, cleared by nextthread */
    HANDLE      hTok;           /* ??: thread token */
    HANDLE      hMQDebuggerRead;    /* ??: Message queue to read debug events */
    DWORD       dwDbgFlag;      /* ??: used to set if debuggee needs to be terminated on debugger thread exit */
    HANDLE      hDbgEvent;      /* ??: Event signaled by debugger on processing of a debug event from this thread */   
    DWORD       dwUTime;        // ??: user time
    DWORD       dwKTime;        // ??: kernel time
    DWORD       dwExitCode;     // ??: exit code
#ifdef x86
    LPVOID      lpFPEmul;       // Floating point emulation handler
#endif
};  /* Thread */

#define THREAD_CONTEXT_OFFSET   0x60

ERRFALSE(THREAD_CONTEXT_OFFSET == offsetof(THREAD, ctx));


#define RUNSTATE_RUNNING 0  // must be 0
#define RUNSTATE_RUNNABLE 1
#define RUNSTATE_BLOCKED 2
#define RUNSTATE_NEEDSRUN 3 // on way to being runnable

#define WAITSTATE_SIGNALLED 0
#define WAITSTATE_PROCESSING 1
#define WAITSTATE_BLOCKED 2

#define TIMEMODE_USER 0
#define TIMEMODE_KERNEL 1

#define RUNSTATE_SHIFT       0  // 2 bits
#define DYING_SHIFT          2  // 1 bit
#define DEAD_SHIFT           3  // 1 bit
#define BURIED_SHIFT         4  // 1 bit
#define SLEEPING_SHIFT       5  // 1 bit
#define TIMEMODE_SHIFT       6  // 1 bit
#define NEEDDBG_SHIFT        7  // 1 bit
#define STACKFAULT_SHIFT     8  // 1 bit
#define DEBUGBLK_SHIFT       9  // 1 bit
#define NOPRIOCALC_SHIFT    10  // 1 bit
#define DEBUGWAIT_SHIFT     11  // 1 bit
#define USERBLOCK_SHIFT     12  // 1 bit
#ifdef DEBUG
#define DEBUG_LOOPCNT_SHIFT 13 // 1 bit - only in debug
#endif
#define NEEDSLEEP_SHIFT     14  // 1 bit
#define PROFILE_SHIFT       15  // 1 bit, must be 15!  Used by assembly code!

#define GET_BPRIO(T)        ((WORD)((T)->bBPrio))                           /* base priority */
#define GET_CPRIO(T)        ((WORD)((T)->bCPrio))                           /* current priority */
#define SET_BPRIO(T,S)      ((T)->bBPrio = (BYTE)(S))
#define SET_CPRIO(T,S)      ((T)->bCPrio = (BYTE)(S))

#define GET_TIMEMODE(T)     (((T)->wInfo >> TIMEMODE_SHIFT)&0x1)        /* What timemode are we in */
#define GET_RUNSTATE(T)     (((T)->wInfo >> RUNSTATE_SHIFT)&0x3)        /* Is thread runnable */
#define GET_BURIED(T)       (((T)->wInfo >> BURIED_SHIFT)&0x1)          /* Is thread 6 feet under */
#define GET_SLEEPING(T)     (((T)->wInfo >> SLEEPING_SHIFT)&0x1)        /* Is thread sleeping */
#define GET_DEBUGBLK(T)     (((T)->wInfo >> DEBUGBLK_SHIFT)&0x1)        /* Has DebugActive suspended thread */
#define GET_DYING(T)        (((T)->wInfo >> DYING_SHIFT)&0x1)           /* Has been set to die */
#define TEST_STACKFAULT(T)  ((T)->wInfo & (1<<STACKFAULT_SHIFT))
#define GET_DEAD(T)         (((T)->wInfo >> DEAD_SHIFT)&0x1)
#define GET_NEEDDBG(T)      (((T)->wInfo >> NEEDDBG_SHIFT)&0x1)
#define GET_PROFILE(T)      (((T)->wInfo >> PROFILE_SHIFT)&0x1)         /* montecarlo profiling */
#define GET_NOPRIOCALC(T)   (((T)->wInfo >> NOPRIOCALC_SHIFT)&0x1)
#define GET_DEBUGWAIT(T)    (((T)->wInfo >> DEBUGWAIT_SHIFT)&0x1)
#define GET_USERBLOCK(T)    (((T)->wInfo >> USERBLOCK_SHIFT)&0x1)       /* did thread voluntarily block? */
#define GET_NEEDSLEEP(T)    (((T)->wInfo >> NEEDSLEEP_SHIFT)&0x1)       /* should the thread put back to sleepq? */

#define SET_RUNSTATE(T,S)   ((T)->wInfo = (WORD)(((T)->wInfo & ~(3<<RUNSTATE_SHIFT)) | ((S)<<RUNSTATE_SHIFT)))
#define SET_BURIED(T)       ((T)->wInfo |= (1<<BURIED_SHIFT))
#define SET_SLEEPING(T)     ((T)->wInfo |= (1<<SLEEPING_SHIFT))
#define CLEAR_SLEEPING(T)   ((T)->wInfo &= ~(1<<SLEEPING_SHIFT))
#define SET_DEBUGBLK(T)     ((T)->wInfo |= (1<<DEBUGBLK_SHIFT))
#define CLEAR_DEBUGBLK(T)   ((T)->wInfo &= ~(1<<DEBUGBLK_SHIFT))
#define SET_DYING(T)        ((T)->wInfo |= (1<<DYING_SHIFT))
#define SET_STACKFAULT(T)   ((T)->wInfo |= (1<<STACKFAULT_SHIFT))
#define CLEAR_STACKFAULT(T) ((T)->wInfo &= ~(1<<STACKFAULT_SHIFT))
#define SET_DEAD(T)         ((T)->wInfo |= (1<<DEAD_SHIFT))
#define CLEAR_DEAD(T)       ((T)->wInfo &= ~(1<<DEAD_SHIFT))
#define SET_TIMEUSER(T)     ((T)->wInfo &= ~(1<<TIMEMODE_SHIFT))
#define SET_TIMEKERN(T)     ((T)->wInfo |= (1<<TIMEMODE_SHIFT))
#define SET_NEEDDBG(T)      ((T)->wInfo |= (1<<NEEDDBG_SHIFT))
#define CLEAR_NEEDDBG(T)    ((T)->wInfo &= ~(1<<NEEDDBG_SHIFT))
#define SET_PROFILE(T)      ((T)->wInfo |= (1<<PROFILE_SHIFT))
#define CLEAR_PROFILE(T)    ((T)->wInfo &= ~(1<<PROFILE_SHIFT))
#define SET_NOPRIOCALC(T)   ((T)->wInfo |= (1<<NOPRIOCALC_SHIFT))
#define CLEAR_NOPRIOCALC(T) ((T)->wInfo &= ~(1<<NOPRIOCALC_SHIFT))
#define SET_DEBUGWAIT(T)    ((T)->wInfo |= (1<<DEBUGWAIT_SHIFT))
#define CLEAR_DEBUGWAIT(T)  ((T)->wInfo &= ~(1<<DEBUGWAIT_SHIFT))
#define SET_USERBLOCK(T)    ((T)->wInfo |= (1<<USERBLOCK_SHIFT))
#define CLEAR_USERBLOCK(T)  ((T)->wInfo &= ~(1<<USERBLOCK_SHIFT))
#define SET_NEEDSLEEP(T)    ((T)->wInfo |= (1<<NEEDSLEEP_SHIFT))
#define CLEAR_NEEDSLEEP(T)  ((T)->wInfo &= ~(1<<NEEDSLEEP_SHIFT))

// get/set last error
#define KSetLastError(pth, err) (pth->dwLastError = err)
#define KGetLastError(pth) (pth->dwLastError)

// Macros to access stack base/bound
#define KSTKBOUND(pth)      ((pth)->tlsPtr[PRETLS_STACKBOUND])
#define KCURFIBER(pth)      ((pth)->tlsNonSecure? (pth)->tlsNonSecure[PRETLS_CURFIBER] : 0)

#define KTHRDINFO(pth)      ((pth)->tlsSecure[PRETLS_THRDINFO])


#define MIN_STACK_SIZE VM_PAGE_SIZE
#define KRN_STACK_SIZE VM_BLOCK_SIZE    // the size of kernel thread stacks

// MIN_STACK_RESERVE is the minimum amount of stack space for the debugger and/or SEH code to run.
// When there is less than this amount of stack left and another stack page is committed,
// a stack overflow exception will be raised.
#define MIN_STACK_RESERVE   (VM_PAGE_SIZE*2)
#define MAX_STACK_RESERVE   (1024*1024)


BOOL THRDCloseHandle (PTHREAD pth, PTHREAD pthExited);
BOOL THRDSuspend (PTHREAD pth, LPDWORD lpRetVal);
BOOL THRDResume (PTHREAD pth, LPDWORD lpRetVal);
BOOL THRDSetPrio (PTHREAD pth, DWORD prio);
BOOL  THRDGetPrio (PTHREAD pth, LPDWORD lpRetVal);
BOOL THRDGetContext (PTHREAD pth, LPCONTEXT lpContext);
BOOL THRDSetContext (PTHREAD pth, const CONTEXT *lpContext);
BOOL THRDTerminate (PTHREAD pth, DWORD dwExitCode, BOOL fWait);
BOOL THRDGetPrio256 (PTHREAD pth, LPDWORD lpRetVal);
BOOL THRDSetPrio256 (PTHREAD pth, DWORD nPriority);
BOOL THRDGetQuantum (PTHREAD pth, LPDWORD lpRetVal);
BOOL THRDSetQuantum (PTHREAD pth, DWORD dwTime);
ULONG THRDGetCallStack (PTHREAD pth, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip);
DWORD THRDGetID (PTHREAD pth);
DWORD THRDGetProcID (PTHREAD pth);

// validate pth inside a kcall
__inline BOOL KC_IsValidThread (PTHREAD pth)
{
    return (pth && !GET_BURIED (pth) && (KC_PthFromId (pth->dwId) == pth));
}

// KC_IsThrdInPSL - check if a thread is in the middle of a PSL call. If calling from non-KCall context,
//                  pth must be pCurThread
__inline BOOL KC_IsThrdInPSL (PTHREAD pth)
{
    return pth->pprcOwner != pth->pprcActv;
}

//------------------------------------------------------------------------------
// THRDCreate - main function to create a thread. thread always created suspended
//------------------------------------------------------------------------------
PTHREAD THRDCreate(
    PPROCESS pprc,          // which process
    LPBYTE  lpStack,        // stack
    DWORD   cbStack,        // stack size
    LPBYTE  pSecureStk,     // secure stack, NULL for kernel threads
    LPVOID  lpStart,        // entry point
    LPVOID  param,          // parameter to thread function
    ulong   mode,           // kernel/user mode
    DWORD   prioAndFlags    // priority and flag
    );

// flags in prioAndFlags field.
// NOTE: MUST NOT use the lowest order byte, which is the priority
#define THRD_FINAL_CLEANUP      0x80000000  // special thread to do final cleanup
#define THRD_NO_BASE_FUNCITON   0x40000000  // don't use ThreadBaseFunc as entry 

BOOL THRDGetCode (HANDLE hth, LPDWORD dwExit);
BOOL THRDGetTimes (HANDLE hth, LPFILETIME lpCreationTime, LPFILETIME lpExitTime, LPFILETIME lpKernelTime, LPFILETIME lpUserTime);

// THRDInit - initialize thread handling (called at system startup)
void THRDInit (void) ;

// globals
extern BYTE W32PrioMap[];

#define THREAD_RT_PRIORITY_ABOVE_NORMAL    W32PrioMap[THREAD_PRIORITY_ABOVE_NORMAL]
#define THREAD_RT_PRIORITY_NORMAL          W32PrioMap[THREAD_PRIORITY_NORMAL]
#define THREAD_RT_PRIORITY_IDLE            W32PrioMap[THREAD_PRIORITY_IDLE]


// kill a specific thread
PTHREAD KillOneThread (PTHREAD pth, DWORD dwExitCode);

// machine dependent part of setting up main/secondary thread
void MDSetupThread (PTHREAD pTh, LPVOID lpBase, LPVOID lpStart, BOOL kMode, ulong param);


//------------------------------------------------------------------------------
//
// WIN32 APIs
//

// NKCreateThread - the CreateThread API
HANDLE NKCreateThread (
    LPSECURITY_ATTRIBUTES lpsa,
    DWORD cbStack,
    LPTHREAD_START_ROUTINE lpStartAddr,
    LPVOID lpvThreadParm,
    DWORD fdwCreate,
    LPDWORD lpIDThread
    );

// NKOpenThread - given thread id, get the handle to the thread.
HANDLE NKOpenThread (DWORD fdwAccess, BOOL fInherit, DWORD IDThread);

void NKExitThread (DWORD dwExitCode);
void NKPrepareThreadExit (DWORD dwExitCode);

#endif  // _NK_THREAD_H_

