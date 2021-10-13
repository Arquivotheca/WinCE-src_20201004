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
//      syncobj.c - header for synchronization objects

#ifndef _NK_SYNC_OBJ_H_
#define _NK_SYNC_OBJ_H_

#include "kerncmn.h"
#include "schedule.h"

// nodes in message queue (variable size)
struct _MSGNODE {
    DWORD       dwFlags;                // message flags
    DWORD       dwDataSize;             // data size contained in message
    PHDATA      phdTok;                 // sender's token
    PMSGNODE    pNext;                  // next node in queue
};

// main structure for message queue

struct _MSGQUEUE {

    PHDATA      phdReader;              // ptr to HDATA of the readers
    PHDATA      phdWriter;              // ptr to HDATA of the writers

    DWORD       dwFlags;                // behavior of message queue 
    DWORD       cbMaxMessage;           // max byte per message 
    DWORD       dwMaxNumMsg;            // max # of messages allowed in queue 
    DWORD       dwCurNumMsg;            // current # of messages in queue 
    DWORD       dwMaxQueueMessages;     // high water mark of data nodes 

    PMSGNODE    pAlert;                 // alert message address 
    PMSGNODE    pHead;                  // queue head 
    PMSGNODE    pTail;                  // queue tail 
    PMSGNODE    pFreeList;              // queue free list head 

    CRITICAL_SECTION csLock;            // lock of the msg queue
};

// if this structure changes, must change AllocName
struct _EVENT {
    PMSGQUEUE   pMsgQ;                  // ptr to msg queue if this event is a message queue 
    PWatchDog   pWD;                    // ptr to watch dog if this is a watchdog event 
    DWORD       dwData;                 // data associated with the event (CE extention) 
    PHDATA      phdIntr;                // Ptr to HData of if interrupt event 
    BYTE        _pad;
    BYTE        state;                  // TRUE: signalled, FALSE: unsignalled
    BYTE        manualreset;            // 0: auto-reset, non-zero:manual-reset, see below for addition bits
    BYTE        bMaxPrio;
    TWO_D_QUEUE proxyqueue;             // 2-D priority queue
};

#define HT_MANUALEVENT          0xff    // special handle type for identifying manual reset events in proxy code

#define MSGQ_WRITER             0x2     // bit field in manualreset, to indicate this is a msgq writer
                                        // NOTE: message queue event is always Manual-Reset, thus we can use
                                        //       bis in the manualreset field to store information.
#define PROCESS_EVENT           0x4     // bit field in manualreset, indicate this is a process event
#define PROCESS_EXITED          0x8     // bit field in manualreset, indicate this is an event for exited process


//
// Mutex or CS
//
struct _MUTEX {
    // NOTE: The 1st 5 DWORD MUST LOOK EXACTLY LIKE TWO_D_NODE
    PMUTEX      pPrevOwned;             // Previous node in 2-D queue
    PMUTEX      pNextOwned;             // Next node in 2-D queue
    PMUTEX      pUpOwned;               // Node above in 2-D queue
    PMUTEX      pDownOwned;             // Node below in 2-D queue
    BYTE        bListedPrio;            // priority
    BYTE        bListed;                // if the mutex is in a thread's "owned object list"
    WORD        LockCount;              // current lock count, or CRIT_SIGNATURE for CS
    TWO_D_QUEUE proxyqueue;             // 2-D priority queue
    PTHREAD     pOwner;                 // owner of the sync object
    LPCRITICAL_SECTION lpcs;            // ptr to CS, NULL if mutex
                                        // Theoretically we don't need this field at all, for kernel don't access this field other
                                        // than validation purposes. However, existence of this field makes it easier to find out
                                        // which CS we're blocking on while debugging.
};

#define MUTEX_OF_QNODE(qn)      ((PMUTEX) (qn))
#define QNODE_OF_MUTEX(mtx)     ((PTWO_D_NODE) (mtx))


#define MUTEX_MAXLOCKCNT        0xffff

#define CRIT_STATE_NOT_OWNED    0       // not in any thread's "owned object list"
#define CRIT_STATE_OWNED        1       // in a thread's "owned object list"
#define CRIT_STATE_LEAVING      2       // owner thread of the CS is giving up ownership

#define IsOwnerLeaving(pMutex)       ((pMutex)->bListed >= CRIT_STATE_LEAVING)
// check if current thread owns a CS
#define OwnCS(lpcs)    ((lpcs)->OwnerThread == (HANDLE) dwCurThId)


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Kernel fast (Reader/writer) lock.
//
// IMPORTANT NOTE:
//      The lock is implemented with one thing in mind - fast. There is no nesting support whatsoever.
//      i.e. it'll deadlock if you call AcquireReadLock/AcquireWriteLock on the same lock with
//      any combination. USE WITH CARE.
// 
#define RWL_XBIT    0x80000000  // exclusive access
#define RWL_WBIT    0x40000000  // wait bit - indicate someone is waiting on this lock

#define RWL_CNTMAX  0x0fffffff  // intentionally not using the full 30-bits, to make sure no overflow

#define HT_READLOCK  0xf0       // pseudo handle type for reader lock
#define HT_WRITELOCK 0xf1       // pseudo handle type for reader lock

struct _FAST_LOCK {
    // NOTE: The 1st 5 DWORD MUST LOOK EXACTLY LIKE TWO_D_NODE
    PFAST_LOCK  pPrevOwned;             // Previous node in 2-D queue
    PFAST_LOCK  pNextOwned;             // Next node in 2-D queue
    PFAST_LOCK  pUpOwned;               // Node above in 2-D queue
    PFAST_LOCK  pDownOwned;             // Node below in 2-D queue
    BYTE        bListedPrio;            // priority
    BYTE        bListed;                // is the lock owned by a writer
    WORD        _unused;                // 
    TWO_D_QUEUE proxyqueue;             // 2-D priority queue
    PTHREAD     pOwner;                 // owner of the sync object
    LONG        lLock;                  // lLock value
    DWORD       dwContention;           // contention count
};

ERRFALSE (offsetof(FAST_LOCK, pPrevOwned) == offsetof(MUTEX, pPrevOwned));
ERRFALSE (offsetof(FAST_LOCK, pNextOwned) == offsetof(MUTEX, pNextOwned));
ERRFALSE (offsetof(FAST_LOCK, pUpOwned) == offsetof(MUTEX, pUpOwned));
ERRFALSE (offsetof(FAST_LOCK, pDownOwned) == offsetof(MUTEX, pDownOwned));
ERRFALSE (offsetof(FAST_LOCK, bListedPrio) == offsetof(MUTEX, bListedPrio));
ERRFALSE (offsetof(FAST_LOCK, bListed) == offsetof(MUTEX, bListed));
ERRFALSE (offsetof(FAST_LOCK, proxyqueue) == offsetof(MUTEX, proxyqueue));
ERRFALSE (offsetof(FAST_LOCK, pOwner) == offsetof(MUTEX, pOwner));


struct _SEMAPHORE {
    LONG   lCount;                      // current count
    LONG   lMaxCount;                   // Maximum count
    LONG   lPending;                    // Pending count
    LONG   _unused[2];                  // padding to keep offset the same
    TWO_D_QUEUE proxyqueue;             // 2-D priority queue
};



#ifdef IN_KERNEL

// How to tell a mutex from a crit
#define ISMUTEX(pMutex)                 (NULL == (pMutex)->lpcs)

//
// THREAD AND MANUAL EVENT ONLY. Signal all threads blocked on the THREAD/MANUAL-EVENT.
//
void SignalAllBlockers (PTWO_D_NODE *ppQHead);


//------------------------------------------------------------------------------
//
// EVENT related functions
//

// create an event
HANDLE NKCreateEvent (LPSECURITY_ATTRIBUTES lpsa, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName);
HANDLE NKCreateMsgQEvent (LPSECURITY_ATTRIBUTES lpsa, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName);
HANDLE NKCreateWdogEvent (LPSECURITY_ATTRIBUTES lpsa, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName);

// open an existing named event
HANDLE NKOpenEvent (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpEventName);
HANDLE NKOpenMsgQEvent (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpEventName);
HANDLE NKOpenWdogEvent (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpEventName);

// EVNTSetData - set datea field of an event
BOOL EVNTSetData (PEVENT lpe, DWORD dwData);

// EVNTGetData - get datea field of an event
DWORD EVNTGetData (PEVENT lpe);

// EVNTModify - set/pulse/reset an event
BOOL EVNTModify (PEVENT lpe, DWORD type);

// ForceEventModify - set/pulse/reset an event, bypass WAIT_ONLY checking, return error code
DWORD ForceEventModify (PEVENT lpe, DWORD type);

// EVNTResumeMainThread - BC workaround, call ResumeThread on process event
BOOL EVNTResumeMainThread (PEVENT lpe, LPDWORD pdwRetVal);

// closehandle an event
BOOL EVNTCloseHandle (PEVENT lpe);

// speical handling for interrupt events
PEVENT LockIntrEvt (HANDLE hIntrEvt);
BOOL UnlockIntrEvt (PEVENT pIntrEvt);

// MSGQRead - read from a message queue
BOOL MSGQRead (PEVENT lpe, LPVOID lpBuffer, DWORD cbSize, 
               LPDWORD lpNumberOfBytesRead, DWORD dwTimeout, LPDWORD pdwFlags, PHANDLE phTok);

// MSGQWrite - write to a message queue
BOOL MSGQWrite (PEVENT lpe, LPCVOID lpBuffer, DWORD cbDataSize, DWORD dwTimeout, DWORD dwFlags);

// MSGQGetInfo - get information from a message queue
BOOL MSGQGetInfo (PEVENT lpe, PMSGQUEUEINFO lpInfo);

// MSGQClose - called when a message queue is closed
BOOL MSGQClose (PEVENT lpe);

// MSGQPreClose - called when all handle to the message queue are closed (can still be locked)
BOOL MSGQPreClose (PEVENT lpe);

// InitMsgQueue - initialize kernel message queue support
BOOL InitMsgQueue(void);

// open a message queue
HANDLE NKOpenMsgQueueEx (PPROCESS pprc, HANDLE hMsgQ, HANDLE hDstPrc, PMSGQUEUEOPTIONS lpOptions);

// create a message queue
HANDLE NKCreateMsgQueue (LPCWSTR pQName, PMSGQUEUEOPTIONS lpOptions);

// check if a named event is signaled
BOOL NKIsNamedEventSignaled (LPCWSTR pszName, DWORD dwFlags);

// functions to set an event in kernel (need to know which handle table to use to decode the event handle)
// NKEventModify - set/reset/pulse an event, with handle
BOOL NKEventModify (PPROCESS pprc, HANDLE hEvent, DWORD type);



//------------------------------------------------------------------------------
//
// Semaphore related funcitons
//

// create a semaphore
HANDLE NKCreateSemaphore (LPSECURITY_ATTRIBUTES lpsa, LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName);

// open a semaphore
HANDLE NKOpenSemaphore (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpName);

//  release a semaphore
BOOL SEMRelease (PSEMAPHORE pSem, LONG lReleaseCount, LPLONG lpPreviousCount);

// closehandle a semaphore
BOOL SEMCloseHandle (PSEMAPHORE pSem);

//------------------------------------------------------------------------------
//
// mutex related functions
//

//  create a mutex
HANDLE NKCreateMutex (LPSECURITY_ATTRIBUTES lpsa, BOOL bInitialOwner, LPCTSTR lpName);

// open a mutex
HANDLE NKOpenMutex (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpName);

// GiveupMutex - give up ownership of a mutex.  Returns ID of new owner thread.
DWORD GiveupMutex (PMUTEX pMutex, LPCRITICAL_SECTION lpcs);

//  release a mutex
BOOL MUTXRelease (PMUTEX pMutex);

// closehandle a mutex
BOOL MUTXCloseHandle (PMUTEX pMutex);

//------------------------------------------------------------------------------
//
// critical section related functions
//

// CRITEnter - EnterCriticalSection
void CRITEnter (PMUTEX pCrit, LPCRITICAL_SECTION lpcs);

// CRITLeave - LeaveCriticalSection
void CRITLeave (PMUTEX pCrit, LPCRITICAL_SECTION lpcs);

// CRITCreate - create a critical section (InitializeCriticalSection)
PMUTEX CRITCreate (LPCRITICAL_SECTION lpcs);

// CRITDelete - delete a critical section
BOOL CRITDelete (PMUTEX pCrit);

// Win32 exports
void Win32CRITEnter (PMUTEX pCrit, LPCRITICAL_SECTION lpcs);
void Win32CRITLeave (PMUTEX pCrit, LPCRITICAL_SECTION lpcs);
HANDLE EXTCRITCreate (LPCRITICAL_SECTION lpcs);

//------------------------------------------------------------------------------
//
// kernel fast (reader/writer) lock related functions
//

// initilize a fast lock
BOOL InitializeFastLock (PFAST_LOCK lpFastLock);

// delete a fast lock
BOOL DeleteFastLock (PFAST_LOCK pFastLock);

// acquire fast lock for read access
void AcquireReadLock (PFAST_LOCK pFastLock);

// release fast lock for read access
void ReleaseReadLock (PFAST_LOCK pFastLock);

// acquire fast lock for write access
void AcquireWriteLock (PFAST_LOCK pFastLock);

// release fast lock for write access
void ReleaseWriteLock (PFAST_LOCK pFastLock);


#endif

#endif  // _NK_SYNC_OBJ_H_
