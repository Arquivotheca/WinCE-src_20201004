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
//    kcalls.h - headers for all KCalls
//
#ifndef _NK_KCALLS_H_
#define _NK_KCALLS_H_

#include "kerncmn.h"
#include "spinlock.h"

typedef struct _sttd_t {
    PTHREAD pThNeedRun;
    PTHREAD pThNeedBoost;
} sttd_t;

// resume a thread
DWORD SCHL_ThreadResume (PTHREAD pth);

// suspend a thread
DWORD SCHL_ThreadSuspend (PTHREAD pth);

// on api return, or exception return, check to see if we're late suspended
void SCHL_SuspendSelfIfNeeded (void);

// current thread yield
void SCHL_ThreadYield (void);

// Set thread affinity
void SCHL_SetThreadAffinity (PTHREAD pth, DWORD dwAffinity);

// change base priority of a thread
BOOL SCHL_SetThreadBasePrio (PTHREAD pth, DWORD nPriority);

// set thread context
BOOL SCHL_DoThreadSetContext (PTHREAD pth, const CONTEXT *lpContext);

// get thread context
BOOL SCHL_DoThreadGetContext (PTHREAD pth, CONTEXT *lpContext);

// terminate a thread
BOOL SCHL_SetThreadToDie (PTHREAD pth, DWORD dwExitCode, sttd_t *psttd);

// change thread quantum
BOOL SCHL_SetThreadQuantum (PTHREAD pth, DWORD dwQuantum);

// get kernel time of current running thread
DWORD SCHL_GetCurThreadKTime (void);

// get user time of current running thread
DWORD SCHL_GetCurThreadUTime (void);

// SCHL_PreAbandonCrit - thread is giving up ownership of all of it's owned CS/mutex
PMUTEX SCHL_PreAbandonMutex (PTHREAD pth);

// SCHL_SwitchOwnerToKernel - thread exiting path, make kernel owner of a thread 
void SCHL_SwitchOwnerToKernel (PTHREAD pth);

// SCHL_FinalRemoveThread - final stage of thread/process cleanup.
void SCHL_FinalRemoveThread (PHDATA phdThrd, PSTUBTHREAD pStubThread, BOOL fIsMainThread);

// NextThread - part I of scheduler. Set interrupt event on interrupt, 
//              and make sleeping thread runnable when sleep time expired.
void NextThread (void) ;

// KCNextThread - part 2 of scheduler, find the highest prioity runnable thread
void KCNextThread (void);

// SCHL_BlockCurThread: block current running thread and reschedule.
void SCHL_BlockCurThread (void);

// SCHL_MakeRunIfNeeded - make a thread runnable if needed
void SCHL_MakeRunIfNeeded (PTHREAD pth);

// SCHL_DequeueFlatProxy - remove a proxy from a flat list (not a 2-d priority queue)
BOOL SCHL_DequeueFlatProxy (PPROXY pprox);

// SCHL_DequeuePrioProxy - remove a proxy form a 2-d priority queue
BOOL SCHL_DequeuePrioProxy (PPROXY pprox);

// SCHL_RegisterMonitorThread - register current thread as monitor thread
void SCHL_RegisterMonitorThread (LPDWORD pdwSpinnerThreadId, DWORD lMaxDelay);

//
// Mutex/CS related functions
//

// SCHL_PostUnlinkCritMut - final step of releasing the ownership of a CS/Mutex
void SCHL_PostUnlinkCritMut (PMUTEX pMutex, LPCRITICAL_SECTION lpcs);

// SCHL_DoReprioCritMut - re-prio a CS in the owner thread's 'owned object list'
void SCHL_DoReprioCritMut (PMUTEX pMutex);

// SCHL_PreReleaseMutex - step 1 of leaving a CS
//          remove the CS from the thread's "owned object list"
BOOL SCHL_PreReleaseMutex (PMUTEX pMutex, LPCRITICAL_SECTION lpcs);

// SCHL_BoostMutexOwnerPriority - boost mutex owner priority.
void SCHL_BoostMutexOwnerPriority (PTHREAD pCurTh, PMUTEX pMutex);

// SCHL_MutexFinalBoost - final step of priority inheritance handling of CS/fastlock.
//                  (boost the priority of the new owner)
void SCHL_MutexFinalBoost (PMUTEX pMutex, LPCRITICAL_SECTION lpcs);

// SCHL_LinkMutexOwner - link mutex to owner thread and handle priority inheritance
void SCHL_LinkMutexOwner (PTHREAD pCurTh, PMUTEX pMutex);

// SCHL_UnlinkMutexOwner - unlink mutex from its owner thread
// NOTE: This function should only be called when a mutex object is to be destroyed.
void SCHL_UnlinkMutexOwner (PMUTEX pMutex);

// SCHL_ReleaseMutex - final stop to release a CS/mutex
//          give the CS to new owner or set owner to 0 if there is no one waiting
BOOL SCHL_ReleaseMutex (PMUTEX pMutex, LPCRITICAL_SECTION lpcs, PTHREAD *ppTh);

// SCHL_YieldToNewMutexOwner - upon release of mutex, yield to the new owner if it's of 
//          priority equal to or higher than the running thread.
void SCHL_YieldToNewMutexOwner (PTHREAD pNewOwner);

// SCHL_WakeOneThreadFlat - wake up a thread that is blocked on thread/process/manual-event
//                      (object linked on a flat-queue)
PTWO_D_NODE SCHL_WakeOneThreadFlat (PTWO_D_NODE *ppQHead, PTHREAD *ppTh);

// SCHL_EventModMan - set/pulse a manual reset event
DWORD SCHL_EventModMan (PEVENT pEvt, PTWO_D_NODE *ppStubQHead, DWORD action);

// SCHL_EventModAuto - set/pulse a auto-reset event
BOOL SCHL_EventModAuto (PEVENT lpe, DWORD action, PTHREAD *ppTh);

// SCHL_EventModIntr -- set/pulse a interrupt event
PTHREAD SCHL_EventModIntr (PEVENT lpe, DWORD type);

// SCHL_ResetProcEvent -- reset a process event if the process hasn't exited yet
void SCHL_ResetProcEvent (PEVENT lpe);

// SCHL_AdjustPrioDown -- adjust the priority of current running thread while set/pulse 
//                   manual-reset events.
void SCHL_AdjustPrioDown (void);

// SamAdd - increment Semaphore count by lReleaseCount,
//          returns previous count (-1 if exceed max)
LONG SCHL_SemAdd (PSEMAPHORE lpsem, LONG lReleaseCount);

// relase all threads, up to the current semaphore count, blocked on the semaphore
BOOL SCHL_SemPop (PSEMAPHORE lpsem, LPLONG pRemain, PTHREAD *ppTh);

// unblock the next thread blocked on the fastlock
PTHREAD SCHL_UnblockNextThread (PFAST_LOCK pFastLock);

// begin releasing a write lock
void SCHL_PreReleaseWriteLock (PFAST_LOCK pFastLock);

// try acquire write lock, block if can't grab it outright
void SCHL_WaitForWriteLock (PFAST_LOCK pFastLock);

// try acquire read lock, block if can't grab it outright
void SCHL_WaitForReadLock (PFAST_LOCK pFastLock);

// SCHL_PutThreadToSleep - put a thread into the sleep queue
BOOL SCHL_PutThreadToSleep (SLEEPSTRUCT *pSleeper);

// power on/off CPUs
BOOL SCHL_PowerOnCPU (DWORD dwProcessor);
BOOL SCHL_PowerOffCPU (DWORD dwProcessor, DWORD dwHint);


typedef BOOL (* PFN_WAITFUNC) (DWORD dwUserData);
// SCHL_WaitOneMore - process one synchronization object in Wait funciton
BOOL SCHL_WaitOneMore (PWAITSTRUCT pws);

#define SIGNAL_STATE_NOT_SIGNALED       0
#define SIGNAL_STATE_SIGNALED           1
#define SIGNAL_STATE_BAD_WAIT           2

// SCHL_CSWaitPart1 - part I of grabbing a CS.
DWORD SCHL_CSWaitPart1 (PMUTEX *ppCritMut, PPROXY pProx, PMUTEX pMutex, LPCRITICAL_SECTION lpcs);

// SCHL_CSWaitPart2 - part 2 of grabbing a CS.
void SCHL_CSWaitPart2 (PMUTEX pMutex, LPCRITICAL_SECTION lpcs);

// common Exception handler
BOOL KC_CommonHandleException (PTHREAD pth, DWORD arg1, DWORD arg2, DWORD arg3);

void SendIPIAndReleaseSchedulerLock (void);

#ifdef ARM
void MDSpin (void);
void MDSignal (void);
#else
#define MDSpin()
#define MDSignal()
#endif

#define AcquireSchedulerLock(kcallid)      { AcquireSpinLock (&g_schedLock); KCALLPROFON (kcallid); }
#define ReleaseSchedulerLock(kcallid)      { KCALLPROFOFF (kcallid); SendIPIAndReleaseSchedulerLock (); }
#define OwnSchedulerLock()                 (GetPCB ()->dwCpuId == g_schedLock.owner_cpu)

FORCEINLINE BOOL PriorityLowerThanNode (PCTWO_D_NODE pNode, DWORD dwPrio)
{
    return pNode && (pNode->prio < dwPrio);
}

extern DWORD dwLastKCTime;

#endif // _NK_KCALLS_H_
