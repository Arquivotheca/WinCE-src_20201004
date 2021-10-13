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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//      syncobj.c - header for synchronization objects

#ifndef _NK_SYNC_OBJ_H_
#define _NK_SYNC_OBJ_H_

#include "kerncmn.h"
#include "schedule.h"

// nodes in message queue (variable size)
struct _MSGNODE {
    DWORD       dwFlags;            // message flags
    DWORD       dwDataSize;         // data size contained in message
    PHDATA      phdTok;             // sender's token
    PMSGNODE    pNext;              // next node in queue
};

// main structure for message queue

struct _MSGQUEUE {

    PHDATA      phdReader;          // ptr to HDATA of the readers
    PHDATA      phdWriter;          // ptr to HDATA of the writers

    DWORD       dwFlags;            // behavior of message queue 
    DWORD       cbMaxMessage;       // max byte per message 
    DWORD       dwMaxNumMsg;        // max # of messages allowed in queue 
    DWORD       dwCurNumMsg;        // current # of messages in queue 
    DWORD       dwMaxQueueMessages; // high water mark of data nodes 

    PMSGNODE    pAlert;             // alert message address 
    PMSGNODE    pHead;              // queue head 
    PMSGNODE    pTail;              // queue tail 
    PMSGNODE    pFreeList;          // queue free list head 

    CRITICAL_SECTION csLock;        // lock of the msg queue
};

// if this structure changes, must change AllocName
struct _EVENT {
    DWORD       dwData;             /* data associated with the event (CE extention) */
    PMSGQUEUE   pMsgQ;              /* ptr to msg queue if this event is a message queue */
    PPROXY      pProxList;
    PPROXY      pProxHash[PRIORITY_LEVELS_HASHSIZE];
    BYTE        _pad;
    BYTE        state;              /* TRUE: signalled, FALSE: unsignalled */
    BYTE        manualreset;        /* TRUE: manual reset, FALSE: autoreset */
    BYTE        bMaxPrio;
    PHDATA      phdIntr;            /* Ptr to HData of an interrupt event */
    PWatchDog   pWD;                /* ptr to watch dog if this is a watchdog event */
};

#define HT_MANUALEVENT      0xff    // special handle type for identifying manual reset events in proxy code
#define MSGQ_WRITER         0x2     // bit field in manualreset, to indicate this is a msgq writer
                                    // NOTE: message queue event is always Manual-Reset, thus we can use
                                    //       bis in the manualreset field to store information.

#define PROCESS_EVENT       0x4     // bit field in manualreset, indicate this is a process event
#define PROCESS_EXITED      0x8    // bit field in manualreset, indicate this is an event for exited process


struct _STUBEVENT {
    DWORD  dwPad;
    DWORD  dwPad2;
    PPROXY pProxList;       /* Root of stub event blocked list */
};

#define MUTEX_MAXLOCKCNT        0x7fff
#define CRIT_SIGNATURE          0x8000

struct _CRIT {
    PCRIT  pPrevOwned;      /* Prev crit/mutex (for prio inversion) */
    PCRIT  pNextOwned;      /* Next crit/mutex section owned (for prio inversion) */
    PPROXY pProxList;
    PPROXY pProxHash[PRIORITY_LEVELS_HASHSIZE];
    PCRIT  pUpOwned;
    PCRIT  pDownOwned;
    BYTE   bListed;         /* Is this on someone's owner list */
    BYTE   bListedPrio;
    WORD   wCritSig;
    LPCRITICAL_SECTION lpcs;/* Pointer to critical_section structure */
};

struct _MUTEX {
    PMUTEX pPrevOwned;      /* Prev crit/mutex owned (for prio inversion) */
    PMUTEX pNextOwned;      /* Next crit/mutex owned (for prio inversion) */
    PPROXY pProxList;
    PPROXY pProxHash[PRIORITY_LEVELS_HASHSIZE];
    PMUTEX pUpOwned;
    PMUTEX pDownOwned;
    BYTE   bListed;
    BYTE   bListedPrio;
    WORD   LockCount;       /* current lock count */
    PTHREAD pOwner;         /* owner thread */
};

struct _SEMAPHORE {
    LONG   lCount;            /* current count */
    LONG   lMaxCount;         /* Maximum count */
    PPROXY pProxList;
    PPROXY pProxHash[PRIORITY_LEVELS_HASHSIZE];
    LONG   lPending;          /* Pending count */
};

// check if current thread owns a CS
#define OwnCS(lpcs)    ((lpcs)->OwnerThread == (HANDLE) dwCurThId)

#ifdef IN_KERNEL

ERRFALSE(offsetof(CRIT, wCritSig) == offsetof(MUTEX, LockCount));

// How to tell a mutex from a crit
#define ISMUTEX(P) (((PCRIT)(P))->wCritSig != CRIT_SIGNATURE)


//------------------------------------------------------------------------------
//
// EVENT related functions
//

// create an event
HANDLE NKCreateEvent (LPSECURITY_ATTRIBUTES lpsa, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName);

// open an existing named event
HANDLE NKOpenEvent (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpEventName);

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

#define NKSetEvent(pprc, hEvt)      NKEventModify (pprc, hEvt, EVENT_SET)
#define NKPulseEvent(pprc, hEvt)    NKEventModify (pprc, hEvt, EVENT_PULSE)
#define NKResetEvent(pprc, hEvt)    NKEventModify (pprc, hEvt, EVENT_RESET)



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

// DoLeaveMutex - worker function to release a mutex
void DoLeaveMutex (PMUTEX lpm);

//  release a mutex
BOOL MUTXRelease (PMUTEX lpm);

// closehandle a mutex
BOOL MUTXCloseHandle (PMUTEX lpm);

//------------------------------------------------------------------------------
//
// critical section related functions
//

// CRITEnter - EnterCriticalSection
void CRITEnter (PCRIT pCrit, LPCRITICAL_SECTION lpcs);

// CRITLeave - LeaveCriticalSection
void CRITLeave (PCRIT pCrit, LPCRITICAL_SECTION lpcs);

// CRITCreate - create a critical section (InitializeCriticalSection)
PCRIT CRITCreate (LPCRITICAL_SECTION lpcs);

// CRITDelete - delete a critical section
BOOL CRITDelete (PCRIT pCrit);

// Win32 exports
void Win32CRITEnter (PCRIT pCrit, LPCRITICAL_SECTION lpcs);
HANDLE Win32CRITCreate (LPCRITICAL_SECTION lpcs);

#endif

#endif  // _NK_SYNC_OBJ_H_
