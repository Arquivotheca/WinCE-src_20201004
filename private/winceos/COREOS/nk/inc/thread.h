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


// Thread data structures
struct _THREAD {
    DLIST       thLink;         // 00: DList, link all threads in a process
    PPROCESS    pprcOwner;      // 08: pointer to owner process 
    WORD        wInfo;          // 0C: various info about thread, see above 
    BYTE        bSuspendCnt;    // 0E: thread suspend count 
    BYTE        bWaitState;     // 0F: state of waiting loop
    PPROCESS    pprcActv;       // 10: pointer to active (current) process 
    PPROCESS    pprcVM;         // 14: pointer to process whose VM is used, NULL if no specific VM required (should only for kernel threads) 
    PCALLSTACK  pcstkTop;       // 18: current api call info 
    LPDWORD     tlsPtr;         // 1c: tls pointer 
    LPDWORD     tlsSecure;      // 20: TLS for secure stack 
    LPDWORD     tlsNonSecure;   // 24: TLS for non-secure stack 
    DWORD       dwId;           // 28: Thread Id (handle of thread, in NK's handle table 
    PPROXY      lpProxy;        // 2C: first proxy this thread is blocked on 
    DWORD       dwStartAddr;    // 30: thread PC at creation, used to get thread name 

    // NOTE: The next 5 DWORD MUST LOOK EXACLY THE SAME AS TWO_D_NODE
    LPVOID      pPrevSleepRun;  // 34: Previous node in 2-D queue (PTWO_D_NODE if runnable, or PTHREAD if sleeping)
    LPVOID      pNextSleepRun;  // 38: Next node in 2-D queue (PTWO_D_NODE if runnable, or PTHREAD if sleeping)
    PTWO_D_NODE pUpRun;         // 3c: Node above in 2-D queue
    PTWO_D_NODE pDownRun;       // 40: Node below in 2-D queue
    BYTE        bCPrio;         // 44: curr priority - MUST BE RIGHT AFTER the 4 pointers above 
    BYTE        bBPrio;         // 45: base priority 
    WORD        wCount;         // 46: nonce for blocking lists 

    PTWO_D_QUEUE pRunQ;         // 48: which run queue should the thread be on.
    DWORD       dwSeqNum;       // 4c: sequence number

    FILETIME    ftCreate;       // 50: creation time - MUST BE 8-byt aligned
    FILETIME    ftExit;         // 58: exit time - MUST BE 8-BYTE aligned
    CPUCONTEXT  ctx;            // 60: thread's cpu context information

    TWO_D_QUEUE ownedlist;      // ??: CS/Mutex owned by this thread


    // fields for 2-d sleep queeu
    PTHREAD     pUpSleep;       // ??: up sleep pointer (null terminated) 
    PTHREAD     pDownSleep;     // ??: down sleep pointer (null terminated) 

    PTWO_D_NODE pBlockers;      // ??: list of proxies to threads blocked on this thread 
    DWORD       dwOrigBase;     // ??: Original stack base
    DWORD       dwOrigStkSize;  // ??: Size of the original thread stack
    DWORD       dwLastError;    // ??: last error
    LPBYTE      pCoProcSaveArea;// ??: co-proc save area
    PCONTEXT    pSavedCtx;      // ??: pointer to saved context structure
    PSTKLIST    pStkForExit;    // ??: stklist to link secure stack onto NK's stack list

    DWORD       dwWakeupTime;   // ??: sleep count, also pending sleepcnt on waitmult
    DWORD       dwQuantum;      // ??: thread quantum
    DWORD       dwQuantLeft;    // ??: quantum left
    DWORD       dwWaitResult;   // ??: return value from pended wait
    WORD        wCount2;        // ??: nonce for SleepList
    BYTE        bPendSusp;      // ??: pending suspend count
    BYTE        bDbgCnt;        // ??: recurse level in debug message
    HANDLE      hTok;           // ??: thread token
    HANDLE      hMQDebuggerRead;// ??: Message queue to read debug events
    DWORD       dwDbgFlag;      // ??: used to set if debuggee needs to be terminated on debugger thread exit
    HANDLE      hDbgEvent;      // ??: Event signaled by debugger on processing of a debug event from this thread
    DWORD       dwUTime;        // ??: user time
    DWORD       dwKTime;        // ??: kernel time
    DWORD       dwExitCode;     // ??: exit code
    DWORD       dwAffinity;     // ??: thread affinity
    DWORD       dwTimeWhenRunnable; // ??: CurMSec when thread became runnable
    DWORD       dwTimeWhenBlocked;  // ??: CurMSec when thread became blocked
#ifdef x86
    FARPROC     lpFPEmul;       // Floating point emulation handler
#endif    
};  // Thread

#define THREAD_CONTEXT_OFFSET   0x60

ERRFALSE(THREAD_CONTEXT_OFFSET == offsetof(THREAD, ctx));

// this is a stub thread structure used to handle per-process ready queue (for BC, only one
// thread of a process can be running at any given time)
struct _STUBTHREAD {
    BYTE _pad1[offsetof(THREAD, wInfo)];
    WORD        wInfo;          // 0C: various info about thread, see above
    BYTE _pad2[offsetof(THREAD, pPrevSleepRun) - offsetof(THREAD, bSuspendCnt)];

    // NOTE: The next 5 DWORD MUST LOOK EXACLY THE SAME AS TWO_D_NODE
    LPVOID      pPrevSleepRun;  // 34: Previous node in 2-D queue (PTWO_D_NODE if runnable, or PTHREAD if sleeping)
    LPVOID      pNextSleepRun;  // 38: Next node in 2-D queue (PTWO_D_NODE if runnable, or PTHREAD if sleeping)
    PTWO_D_NODE pUpRun;         // 3c: Node above in 2-D queue
    PTWO_D_NODE pDownRun;       // 40: Node below in 2-D queue
    BYTE        bCPrio;         // 44: curr priority - MUST BE RIGHT AFTER the 4 pointers above 
    BYTE        bBPrio;         // 45: base priority 
    WORD        wCount;         // 46: nonce for blocking lists 

    PTWO_D_QUEUE pRunQ;         // 48: which run queue should the thread be on.
    DWORD       dwSeqNum;       // 4c: sequence number
    TWO_D_QUEUE ProcReadyQ;     // 50: per-process process ready queue
};

ERRFALSE (offsetof (STUBTHREAD, pRunQ) == offsetof (THREAD, pRunQ));
ERRFALSE (offsetof (STUBTHREAD, pPrevSleepRun) == offsetof (THREAD, pPrevSleepRun));
ERRFALSE (offsetof (STUBTHREAD, dwSeqNum) == offsetof (THREAD, dwSeqNum));
ERRFALSE (offsetof (STUBTHREAD, wInfo) == offsetof (THREAD, wInfo));

#define THREAD_OF_QNODE(n)      ((PTHREAD) ((DWORD) (n) - offsetof (THREAD, pPrevSleepRun)))
#define QNODE_OF_THREAD(pth)    ((PTWO_D_NODE) &(pth)->pPrevSleepRun)

#define IsStubThread(pth)       (!(pth)->pprcActv)
#define GetStubThread(pth)      ((pth)->pprcOwner->pStubThread)

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
//#define FAULTONEXIT_SHIFT    7  // 1 bit
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

#define GET_BPRIO(T)        ((T)->bBPrio)                           /* base priority */
#define GET_CPRIO(T)        ((T)->bCPrio)                           /* current priority */
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

#define MIN_STACK_SIZE VM_PAGE_SIZE
#define KRN_STACK_SIZE VM_BLOCK_SIZE    // the size of kernel thread stacks

// MIN_STACK_RESERVE is the minimum amount of stack space for the debugger and/or SEH code to run.
// When there is less than this amount of stack left and another stack page is committed,
// a stack overflow exception will be raised.
#define MIN_STACK_RESERVE   (VM_PAGE_SIZE*2)
#define MAX_STACK_RESERVE   (1024*1024)

// get thread pointer checks if thread is initialized before the call or not
__inline PTHREAD GetThreadPtr (PHDATA phd)
{
    PTHREAD pth = (PTHREAD) GetObjPtrByType(phd, SH_CURTHREAD);
    return (pth && pth->dwId) ? pth : NULL;
}

//
// function that can only be called inside KCall
//
// NOTE: The returned pointer is only valid inside the same KCall
//
__inline PTHREAD KC_PthFromId (DWORD dwThId)
{
    return GetThreadPtr (h2pHDATA ((HANDLE) dwThId, g_phndtblNK));
}

BOOL THRDCloseHandle (PTHREAD pth, PTHREAD pthExited);
BOOL THRDSuspend (PTHREAD pth, LPDWORD lpRetVal);
BOOL THRDResume (PTHREAD pth, LPDWORD lpRetVal);
BOOL THRDSetPrio (PTHREAD pth, DWORD prio);
BOOL THRDGetPrio (PTHREAD pth, LPDWORD lpRetVal);
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
BOOL THRDSetAffinity (PTHREAD pth, DWORD dwAffinity);
BOOL THRDGetAffinity (PTHREAD pth, LPDWORD pdwAffinity);

// validate pth inside a kcall
__inline BOOL KC_IsValidThread (PCTHREAD pth)
{
    return (pth && !GET_BURIED (pth) && (KC_PthFromId (pth->dwId) == pth));
}

//------------------------------------------------------------------------------
// THRDCreate - main function to create a thread. thread always created suspended, return locked pHDATA
//------------------------------------------------------------------------------
PHDATA THRDCreate (PPROCESS pprc, FARPROC pfnStart, LPVOID pParam, DWORD cbStack, DWORD mode, DWORD prioAndFlags);

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
void KillOneThread (PTHREAD pth, DWORD dwExitCode);

// machine dependent part of setting up main/secondary thread
void MDSetupThread (PTHREAD pTh, FARPROC lpBase, FARPROC lpStart, BOOL kMode, ulong param);


//------------------------------------------------------------------------------
//
// WIN32 APIs
//

// NKCreateThread - the CreateThread API (internal)
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

