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
/*++


Module Name:

    wmtp.cpp

Abstract:

    Thread pool functions and structures


--*/

#pragma once

#include <windows.h>
#include <dlist.h>
#include <kerncmn.h>
#include <celog.h>
#include <coredll.h>

// thread pool count defaults
#define TP_THRD_DFLT_MIN        0x01        // default pool minimum if not set by user
#define TP_THRD_DFLT_MAX        0x04        // default pool maximum if not set by user        
#define TP_THRD_MAX_LIMIT       0x100       // max limit for a custom thread pool

// thread pool states
#define TP_STATE_ACTIVE         0x01        // pool main thread created
#define TP_STATE_DYING          0x02        // pool is being shut down

// item type definitions
#define TP_TYPE_USER_WORK       0x01        // item for QueueUserWorkItem
#define TP_TYPE_SIMPLE          0x02        // item for TrySubmitThreadpoolCallback
#define TP_TYPE_TIMER           0x04        // item for CreateThreadpoolTimer
#define TP_TYPE_WORK            0x08        // item for CreateThreadpoolWork
#define TP_TYPE_WAIT            0x10        // item for CreateThreadpoolWait
#define TP_TYPE_REGWAIT         0x20        // item for RegisterWaitForSingleObject
#define TP_TYPE_REGWAIT_ONCE    0x40        // item for RegisterWaitForSingleObject with WT_EXECUTEONLYONCE flag
#define TP_TYPE_TIMERQ_TIMER    0x80        // item for CreateTimerQueueTimer

// timer definitions
#define TP_ITEM_STATE_ACTIVATED    0x01     // item is activated
#define TP_ITEM_STATE_DELETE       0x02     // item should be deleted  
#define TP_ITEM_DUE_TIME_VALID     0x04     // set when SetThreadpoolTimer called while there is any outstanding callback

#define MAX_THREADPOOL_WAIT_TIME    0x7fff0000

class CThreadPoolWaitGroup;

// thread pool item struct
typedef struct _TP_ITEM
{
    DLIST       Link;                       // link to other timer nodes: must be first in struct
    PTP_POOL    pPool;                      // the threadpool this item belongs
    WORD        wState;                     // item state    
    WORD        wType;                      // item type
    PVOID       hModule;                    // RaceDll
    PVOID       Context;                    // callback conntext
    HANDLE      DoneEvent;                  // event to signal callback completion
    DWORD       dwCallbackInProcess;        // # of callback in progress
    DWORD       dwCallbackRemaining;        // # of callback waiting to be done
    DWORD       dwDueTime;                  // time (in ticks) when timer expire
    DWORD       dwPeriod;                   // periodic interval (0 if one shot)
    HANDLE      hObject;                    // wait object
    DWORD       dwWaitResult;               // wait result for wait callback
    HANDLE      hCompletionEvent;           // external completion event
    DWORD       dwCookie;                   // clean up cookie
    union {
        FARPROC                 Callback;   // callback function ptr
        LPTHREAD_START_ROUTINE  UserWorkCallback;
        PTP_SIMPLE_CALLBACK     SimpleCallback;
        PTP_TIMER_CALLBACK      TimerCallback;
        PTP_WORK_CALLBACK       WorkCallback;
        PTP_WAIT_CALLBACK       WaitCallback;
        WAITORTIMERCALLBACK     WaitOrTimerCallback;
    };
}TP_ITEM, *PTP_ITEM;

struct _TP_CALLBACK_INSTANCE {
    PTP_ITEM                pTpItem;        // item for the callback instance
    LPCRITICAL_SECTION      pcs;            // CS to release after callback
    HANDLE                  hMutex;         // mutex to release after callback
    HANDLE                  hEvent;         // event to signal after callback
    HANDLE                  hSemaphore;     // semaphore to release after callback
    DWORD                   lSemRelCnt;     // semaphore release count
    HMODULE                 hModule;        // library to free after callback
};

// thread state definitions
#define TP_THRD_FLAGS_IDLE      0x01        // thread is idle
#define TP_THRD_FLAGS_SUSP      0x02        // thread is suspended
#define TP_THRD_FLAGS_EXIT      0x04        // thread should exit once this flag is set    

// thread struct
struct _TP_THRD
{
    DLIST       Link;                       // link to other thread nodes: must be first in struct
    PTP_POOL    Pool;                       // owner pool
    PTP_ITEM    pTpItem;                    // thread pool item
    LONG        Flags;                      // pool thread state   
    HTHREAD     ThreadHandle;               // handle to the thread (for resume/suspend)
#ifndef SHIP_BUILD    
    FARPROC     LastCallback;               // last callback called on this thread
    DWORD       LastTickCount;              // when was last callback made on this thread
#endif    
};

typedef struct _TP_THRD TP_THRD, *PTP_THRD;

void LogTPCommon(WORD wID, PTP_ITEM pTpItem);
DWORD GetWaitTimeInMilliseconds (PFILETIME pllftDue);

FORCEINLINE LONGLONG MillisecondToFileTime (DWORD dwMilliseconds)
{
    return (LONGLONG) dwMilliseconds * 10000;
}

FORCEINLINE DWORD FileTimeToMillisecond (LONGLONG l64FileTime)
{
    ASSERT (l64FileTime >= 0);
    return (DWORD) ((l64FileTime+9999) / 10000);
}

FORCEINLINE BOOL ValidateTpFlags (DWORD dwFlags)
{
    BOOL fRet = !(dwFlags & ~WT_VALID_FLAGS);

    if (fRet) {
        DEBUGMSG (DBGTP && (dwFlags & ~WT_CE_HONORED_FLAGS), 
                  (L"CE only honored the threadpool flags 0x%x, the rest are ignored\r\n", WT_CE_HONORED_FLAGS));
    } else {
        SetLastError (ERROR_INVALID_PARAMETER);
        DEBUGMSG (DBGTP, 
                  (L"Invalid Flag 0x%x passed to thread pool function\r\n", dwFlags));
    }
    return fRet;
}

//
// disable "unreference local variable" warning, as this is intended.
//
#pragma warning(push)
#pragma warning(disable:4189)
FORCEINLINE void FaultIfBadPointer (PCVOID ptr)
{
    BYTE p = *(volatile BYTE *)ptr;
}
#pragma warning(pop)

FORCEINLINE DWORD TpCeLogZone()
{
    return CELZONE_RESCHEDULE;
}

FORCEINLINE BOOL IsLoggingEnabled()
{
    return IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL, TpCeLogZone());
}

FORCEINLINE static void LogTPCallbackStart(PTP_ITEM pTpItem)
{
    LogTPCommon(CELID_CETP_TPCBSTART, pTpItem);
}

FORCEINLINE void LogTPCallbackStop()
{
    if (IsLoggingEnabled())
    {
        CeLogData(
            TRUE,
            CELID_CETP_TPCBSTOP,
            NULL,
            0,
            0,
            TpCeLogZone(),
            0,
            FALSE);
    }
}

FORCEINLINE void LogTPAllocItem(PTP_ITEM pTpItem)
{
    LogTPCommon(CELID_CETP_TPALLOC, pTpItem);
}

FORCEINLINE void LogTPDeleteItem(PTP_ITEM pTpItem)
{
    LogTPCommon(CELID_CETP_TPDELETE, pTpItem);
}

//
// thread pool class
//
class CThreadPool {
private:
    friend class CThreadPoolWaitGroup;
    
    LONG    m_lState;                   // pool state: Active or not
    
    LONG    m_MaxThrdCnt;               // max threads in the pool
    LONG    m_MinThrdCnt;               // min threads in the pool
    LONG    m_CurThrdCnt;               // count of active threads in the pool
    LONG    m_nCallbackInProgress;      // # of callback in progress
    
    DLIST   m_dlActiveThrds;            // list of all pool threads (idle threads = head of list, busy threads = tail of list)

    DLIST   m_dlPendingWork;            // list of work pending (not expired)
    DLIST   m_dlExpiredWork;            // list of expired work
    DLIST   m_dlInactive;               // inactive list (including items in callback)
    
    DLIST   m_dlWaitGroup;              // list of wait groups

    DWORD   m_cbStackCommit;            // min. amount of stack memory commited per thread
    DWORD   m_cbStackReserve;           // max. amount of stack memory to commit per thread
    
    DWORD   m_dwDriverThreadId;         // driver thread id    
    HANDLE  m_hEvtStateChanged;         // signal when there is new work item
    HANDLE  m_hEvtThrdExited;           // signaled whenever a pool thread exited

    LONG    m_lRefCnt;                  // total number of reference to the pool object

    CRITICAL_SECTION m_PoolCs;          // cs guarding all the list updates

protected:

    //
    // IncrementCallbackInProgress - increment # of callback in progresss
    //
    void IncrementCallbackInProgress (PTP_ITEM pTpItem)
    {
        ASSERT (IsPoolCsOwned ());

        // increment callback in progress. 
        pTpItem->dwCallbackInProcess ++;
    }

    //
    // DecrementCallbackInProgress - decrement # of callback in progress
    //
    BOOL DecrementCallbackInProgress (PTP_ITEM pTpItem)
    {
        ASSERT (IsPoolCsOwned ());
        ASSERT (pTpItem->dwCallbackInProcess);
        --pTpItem->dwCallbackInProcess;
        return !IsCallbackPending (pTpItem);
    }

    //
    // IncrementCallbackRemaining - increment the # of callback remained to be done.
    //
    void IncrementCallbackRemaining (PTP_ITEM pTpItem)
    {
        ASSERT (IsPoolCsOwned ());

        if (!pTpItem->dwCallbackRemaining ++) {
            // 1st time incrementing
            RemoveDList (&pTpItem->Link);
            AddToDListTail (&m_dlExpiredWork, &pTpItem->Link);

            if (!pTpItem->dwCallbackInProcess) {
                ResetEvent (pTpItem->DoneEvent);
            }
        }
    }

    //
    // DecrementCallbackRemaining - decrement the # of callback remained to be done.
    //
    void DecrementCallbackRemaining (PTP_ITEM pTpItem)
    {
        ASSERT (pTpItem->dwCallbackRemaining);
        RemoveDList (&pTpItem->Link);
        if (! --pTpItem->dwCallbackRemaining) {
            AddToDListTail (&m_dlInactive, &pTpItem->Link);
        } else {
            AddToDListTail (&m_dlExpiredWork, &pTpItem->Link);
        }
    }

    BOOL IsCallbackPending (PTP_ITEM pTpItem)
    {
        ASSERT (IsPoolCsOwned ());
        return pTpItem->dwCallbackInProcess || pTpItem->dwCallbackRemaining;
    }

    void Lock ()
    {
        EnterCriticalSection (&m_PoolCs);
    }

    void Unlock ()
    {
        LeaveCriticalSection (&m_PoolCs);
    }

    void IncRef ()
    {
        ASSERT (m_lRefCnt);
        InterlockedIncrement (&m_lRefCnt);
    }

    void DecRef ()
    {
        ASSERT (m_lRefCnt);
        if (InterlockedDecrement (&m_lRefCnt) == 0) {
            delete this;
        }
    }

    /*++

        Routine Description:
            Does the calling thread own the pool cs?

        Arguments:
            None

        Return Value:
            TRUE/FALSE

        --*/
    BOOL IsPoolCsOwned (void)
    {
        return OWN_CS (&m_PoolCs);
    }

    /*++

        Routine Description:
            Get an idle thread if available?

        Arguments:
            None

        Return Value:
            ptr to PTP_THRD, NULL if failed

        --*/
    PTP_THRD GetIdleThread (void)
    {
        PTP_THRD pThrd = (PTP_THRD)m_dlActiveThrds.pFwd;
        return (IsDListEmpty (&m_dlActiveThrds) || !IsPoolThrdIdle (pThrd))
             ? NULL
             : pThrd;
    }
    

    /*++

        Routine Description:
            Is the thread pool active (shutdown is not called)?

        Arguments:
            None

        Return Value:
            TRUE/FALSE

        --*/
    BOOL IsPoolActive (void)
    {
        return (m_lState & TP_STATE_ACTIVE)? TRUE : FALSE;
    }

    /*++

        Routine Description:
            Is the pool thread cnt above trim threshold?
            Currently the trim threshold is set to twice
            the minimum thread count.

        Arguments:
            None

        Return Value:
            if above trim limit

        --*/
    BOOL IsPoolAboveTrim (void)
    {
        return (m_CurThrdCnt > 2*m_MinThrdCnt);
    }    

    /*++

        Routine Description:
            Is the thread idle?

        Arguments:
            None

        Return Value:
            non-zero if thread is idle

        --*/
    BOOL IsPoolThrdIdle (PTP_THRD pThrd)
    {
        return pThrd->Flags & TP_THRD_FLAGS_IDLE;
    }        

    /*++

        Routine Description:
            Resume an idle thread to pickup the 1st callback to execute.
            There MUST BE at least one callback ready to be executed. 

        Arguments:
            Thread to resume

        Return Value:

        --*/
    void ResumeThrd (
        PTP_THRD pThrd
        );

    /*++

        Routine Description:
            Schedule any timers in the expired
            queue.

        Arguments: 
            None
            
        Return Value:  
            if more threads are needed.
            
        --*/
    BOOL ProcessExpiredItems ();

    /*++

        Routine Description:
            Process an expired item.

        Arguments:
            Worker thread executing this timer/work
            Timer/work node

        Return Value:        

        --*/
    void ProcessItem (PTP_THRD pThrd, PTP_ITEM pTpItem);

    //
    // post callback handling 
    //
    HANDLE PostCallbackProcess (PTP_ITEM pTpItem);
    
    /*++

        Routine Description:
            Scan and process all expired work items.
            If possible schedule the work items on any
            available idle threads immediately; if not
            create additional pool threads to process
            work.

        Arguments:
            ptr to dword to receive new timeout
            
        Return Value:        
            need more threads or not

        --*/
    BOOL ExpireItems (LPDWORD pdwTimeout);

    /*++

        Routine Description:
            Insert a thread pool item in pending work list
            at the right slot (sorted by delay).
            the due time must be set before calling this function.

        Arguments: 
            thread pool item

        Return Value:  
            TRUE - if timer is inserted at head of list
            FALSE - otherwise
            
        --*/
    BOOL InsertItem (PTP_ITEM pTpItem);
    
    /*++

        Routine Description:
            Scan and retire all idle threads beyond
            the minimum # of threads required
            in the pool.

        Arguments:            
            list to add the thread too
        Return Value:                   
            
        --*/
    void RetireExcessIdleThreads ();
   
    /*++

        Routine Description:
            Thread proc used by all threads in the pool 
            waiting for work.

        Arguments:
            Thread node
            
        Return Value:      

        --*/
    void CallbackThread (PTP_THRD pThrd);
    
    static DWORD WINAPI CallbackThreadStub (LPVOID lpParam)
    {
        PTP_THRD pThrd = (PTP_THRD) lpParam;
        class CThreadPool* pPool = (class CThreadPool*)pThrd->Pool;

        if (pPool) {
            pPool->CallbackThread(pThrd);
            ASSERT (IsDListEmpty (&pThrd->Link));
            SetEvent (pPool->m_hEvtThrdExited);
            pPool->DecRef ();
        }
        
        CloseHandle (pThrd->ThreadHandle);
        LocalFree (pThrd);

        ExitThread(0); // don't use return as this thread is created with no base
        // keep compiler happy, never reached
        return 0;
    }

    /*++

        Routine Description:
            thread pool driver thread which:
            - delegates work 
            - shuts down pool threads
            - creates/trims callback threads if needed

        Arguments:
            None

        Return Value:     

        --*/
    void DriverThread ();

    static DWORD WINAPI DriverThreadStub (LPVOID lpParam)
    {
        class CThreadPool* pTpPool = (class CThreadPool*) lpParam;
        pTpPool->DriverThread();
        pTpPool->DecRef ();

        ExitThread(0); // don't use return as this thread is created with no base                
        // keep compiler happy, never reached
        return 0;
    }

    /*++

        Routine Description:
            Create a new callback thread in the thread pool.

        Arguments:
            ptr to receive if more threads are needed
        Return Value:
            TRUE - thread created
            FALSE - thread not created

        --*/
    BOOL NewCallbackThrd (PBOOL pfNeedMoreThreads);

    void CreateNewThreads ();

    /*++

        Routine Description:
            Destructor (protected to disable calls to this from external)

        Arguments:

        Return Value:        

        --*/
    ~CThreadPool (void); 


    static BOOL EnExpireItem (PDLIST pdl, LPVOID pParam);

    //
    // dList enumeration function
    // - destroy a wait group
    //
    static BOOL EnDestroyWaitGroup (PDLIST pdl, LPVOID);

    //
    // dList enumeration function
    // - try to reserve a wait spot in a wait group
    //
    static BOOL EnReserveWait (PDLIST pdl, LPVOID);
    
public:
    
    /*++

        Routine Description:
            Constructor

        Arguments:

        Return Value:        

        --*/
    CThreadPool (void);

    /*++

        Routine Description:
            Is the thread pool initialized?

        Arguments:
            None

        Return Value:
            TRUE - pool constructor was successful
            FALSE - pool failed to initialize in the constructor

        --*/
    BOOL IsInitialized (void)
    {
        return (m_hEvtStateChanged && m_hEvtThrdExited && m_PoolCs.hCrit);
    }    

    /*++

        Routine Description:
            Mark the pool state as inactive and signal driver 
            thread to exit. driver thread will start the shutdown 
            process for all the pool threads.

            Shutdown of the pool is always done in an 
            asynchronous mode where all the worker threads
            are signaled for shutdown and driver thread waits
            for these threads to shut down before deleting the
            pool object itself.

        Arguments:    

        Return Value: 

        --*/
    void Shutdown (void);

    /*++

        Routine Description:
            Activate the thread pool. Create driver thread
            if this is the first time the pool is activated.

        Arguments:
            None

        Return Value:        
            TRUE - pool is activated
            FALSE - pool failed to activate or shutting down

        --*/
    BOOL Activate (void);
        
    /*++

        Routine Description:
            Set the pool thread stack max and commit bytes

        Arguments:
            max stack memory to reserve per thread
            min stack memory to commit per thread

        Return Value:            

        --*/
    void SetStackInfo (DWORD cbReserve, DWORD cbCommit);

    /*++

        Routine Description:
            Get the pool thread stack max and commit bytes

        Arguments:
            [out] stack reserve and commit values

        Return Value:            

        --*/
    void GetStackInfo (PDWORD pcbReserve, PDWORD pcbCommit);

    /*++

        Routine Description:
            Set the minimum number of threads and if needed
            also create driver thread in the pool.

            If the pool was not active before this call, we will
            try to create the driver thread when this is called.

        Arguments:
            Count of min threads

        Return Value:
            TRUE - pool is activated with given min threads
            FALSE - pool failed to activate with given min threads

        --*/
    BOOL SetThrdMin (LONG cThrdMin);

    /*++

        Routine Description:
            Set the maximum number of threads

        Arguments:
            Count of max threads

        Return Value:            

        --*/
    void SetThrdMax (LONG cThrdMost);
    
    /*++

        Routine Description:
            Add work item to the sorted list to expire after
            a given delay.

            msDelay=0 is the most common usage of this
            function. optimize that case.

        Arguments:
            Item to add
            Delay after which to expire this timer
            If periodic, period interval to reschedule this timer

        Return Value: 
            TRUE - timer is queued
            FALSE - timer is not queued

        --*/    
    BOOL EnqueueItem (
        PTP_ITEM pTpItem,
        DWORD msDelay,
        DWORD msPeriod
        );

    /*++

        Routine Description:
            This function removes a scheduled thread run from the queue.
            If the scheduled run has already expired, then the work node
            is not removed. This only removes pending work item.

        Arguments:
            pTpItem - item to remove
            hCompletionEvent - event to signal when complete
            fCancelPending - cancel expired but yet to started callback
            fDelete - delete the timer node

        Return Value:
            TRUE - timer is de-queued
            FALSE - timer is not de-queued (either expired or not found)

        --*/
    BOOL DequeueItem (
        PTP_ITEM pTpItem,
        HANDLE hCompletionEvent,
        BOOL fCancelPending,
        BOOL fDelete
        );

    // 
    // Delete all items of the same cookie
    //  
    void DeleteAllItems (
        DWORD dwCookie,
        PDLIST pdlInProgress,       // DLIST header to receive the list of callback in progress
        HANDLE hCleanupEvent        // internal event to be signaled for cleanup (i.e. whenever an inprogress callback completed)
        );

    /*++

        Routine Description:
            This function finds a wait group that can take addition
            "wait" and reserve it. Create a new wait group if it can't 
            find any.

        Arguments:
            none

        Return Value:
            the wait group found/created. NULL if failed.        
    --*/
    CThreadPoolWaitGroup *ReserveWaitInWaitGroup ();

    //
    // create a thead pool item
    //
    PTP_ITEM TpAllocItem (FARPROC pfn, PVOID pv, PTP_POOL pPool, PVOID pMod, WORD wType, DWORD dwCookie = 0);

    //
    // delete a thead pool item
    //
    void TpDeleteItem (LPVOID pti);


};

typedef class CThreadPool *PThreadPool;

PThreadPool
GetPerProcessThreadPool (
    void
    );

FORCEINLINE PVOID GetRaceDllFromEnv (PTP_CALLBACK_ENVIRON pCallbackEnviron)
{
    return pCallbackEnviron ? pCallbackEnviron->RaceDll : NULL;
}

FORCEINLINE PThreadPool GetThreadpoolFromEnv (PTP_CALLBACK_ENVIRON pCallbackEnviron)
{
    return (pCallbackEnviron && pCallbackEnviron->Pool) ? (PThreadPool)pCallbackEnviron->Pool : GetPerProcessThreadPool(); // use passed in pool via env block or global thread pool
}


class CThreadPoolWaitGroup {
private:
    friend class CThreadPool;
    
    DLIST   m_link;                     // link to thread groups, MUST BE FIRST.
    DLIST   m_dlWait;                   // list of "wait"
    
    DWORD   m_dwWaitThreadId;           // wait thread id
    HANDLE  m_hEvtWaitChanged;          // signal when there is work to be processed by wait thread
    HANDLE  m_hEvtThreadExited;         // signaled when wait thread exited.

    LONG    m_nWaits;                   // # of "Wait" in this wait-group
    BOOL    m_fShutdown;                // the wait group is shutting down


    PThreadPool m_pPool;              // the thread pool the wait group belongs to.

protected:

    void Lock ()
    {
        m_pPool->Lock ();
    }

    void Unlock ()
    {
        m_pPool->Unlock ();
    }

    BOOL IsLockHeld ()
    {
        return m_pPool->IsPoolCsOwned ();
    }

    DWORD CollectAndSignalWaitHandles (PHANDLE pHandles, DWORD cMaxHandles, LPDWORD pdwTimeout, BOOL fCheckBadWait);

    void SignalWait (HANDLE hObject);

    /*++

        Routine Description:
            Wait group thread which:
            - Wait on a group of waits items

        Arguments:
            None

        Return Value:     

        --*/
    void WaitGroupThread ();

    static DWORD WINAPI WaitGroupThreadStub (LPVOID lpParam)
    {
        CThreadPoolWaitGroup *pGroup = (CThreadPoolWaitGroup *) lpParam;

        pGroup->WaitGroupThread();

        ExitThread(0); // don't use return as this thread is created with no base                
        // keep compiler happy, never reached
        return 0;
    }

    /*++

        Routine Description:
            Destructor (protected to disable calls to this from external)

        Arguments:

        Return Value:        

        --*/
    ~CThreadPoolWaitGroup (void);
    
    
    /*++

        Routine Description:
            Constructor

        Arguments:

        Return Value:        

        --*/
    CThreadPoolWaitGroup ();

    /*++

        Routine Description:
            Initialize wait group
            
        Arguments:
            pointer to thread pool where this wait group is created

        Return Value:
            initialization succeeded or not.

        --*/
    BOOL Initialize (PThreadPool pPool);

    /*++

        Routine Description:
            Destroy the wait group.

        Arguments:    

        Return Value: 

        --*/
    void Destroy (void);


    BOOL ReserveWait (void)
    {
        ASSERT (IsLockHeld ());

        BOOL fRet = (m_nWaits < (MAXIMUM_WAIT_OBJECTS - 2));

        if (fRet) {
            m_nWaits ++;
        }
        
        return fRet;
    }

    BOOL ReleaseWait (void)
    {
        ASSERT (IsLockHeld ());
        -- m_nWaits;

        if (!m_nWaits                               // no more wait in the wait group
            && (m_link.pBack != m_link.pFwd)) {     // not the only wait group
            // about to destroy the wait group
            RemoveDList (&m_link);              
            InitDList (&m_link);

            // there shouldn't be any pending wait
            ASSERT (IsDListEmpty (&m_dlWait));
            
            m_fShutdown = TRUE;
            SetEvent (m_hEvtWaitChanged);
        }
        return m_fShutdown;
    }

    HANDLE ReactivateWait (
        PTP_ITEM pTpItem
        );

    BOOL ProcessSignaledWaitItem (PTP_ITEM pTpItem, DWORD dwWaitResult);

    static BOOL EnCollectWait (PDLIST pItem, LPVOID pEnumData);

public:
    /*++

        Routine Description:
            Activate the "wait". i.e. start waiting on a handle with a timeout.

        Arguments:
            pTpItem - "wait" to activate.
            h      - handle to wait
            dwTimeout - Timeout value in milliseconds.

        Return Value: 
            none.

        --*/    
    void ActivateWait (
        PTP_ITEM   pTpItem,
        HANDLE     h,
        DWORD      dwTimeout
        );

    /*++

        Routine Description:
            Activate the "wait". i.e. start waiting on a handle with a timeout.

        Arguments:
            pTpItem - "wait" to activate.
            h      - handle to wait
            pftTimeout - A pointer to a FILETIME structure that specifies the absolute 
                  or relative time at which the wait operation should time out. 
                  If this parameter points to a positive value, it indicates the absolute 
                  time since January 1, 1601 (UTC), in 100-nanosecond intervals. If this parameter
                  points to a negative value, it indicates the amount of time to wait
                  relative to the current time.

        Return Value: 
            none.

        --*/    


    FORCEINLINE void ActivateWait (
        PTP_ITEM   pTpItem,
        HANDLE     h,
        PFILETIME  pftTimeout
        )
    {
        return ActivateWait (pTpItem, h, GetWaitTimeInMilliseconds (pftTimeout));
    }

    /*++

        Routine Description:
            deactive a wait operation. i.e. remove the wait handle from the wait list.
            
        Arguments:
            pTpItem - "wait" to deactivate
            hCompletionEvent - event to signal upon completion
            fCancelPending - cancel outstanding callback (expired, not yet executed)
            fDelete - Optionally delete the wait node

        --*/
    BOOL DeactivateWait (
        PTP_ITEM pTpItem,
        HANDLE     hCompletionEvent,
        BOOL       fCancelPending,
        BOOL       fDelete
        );


};

