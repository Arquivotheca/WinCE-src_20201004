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

    tpwait.cpp

Abstract:

    Thread pool wait functions and structures


--*/

#include "cetp.hpp"

BOOL EnDeleteItem (
    PDLIST pItem,
    LPVOID pENumData
    );

/*++

    Routine Description:
        Wait group thread which:
        - Wait on a group of waits objects

    Arguments:
        None

    Return Value:     

    --*/
void CThreadPoolWaitGroup::WaitGroupThread ()
{
    HANDLE hWaitHandles[MAXIMUM_WAIT_OBJECTS] = { m_hEvtWaitChanged };
    DWORD  dwWaitResult = WAIT_OBJECT_0;       // anything but WAIT_FAILED
    DWORD  dwTimeout;

    for ( ; ; ) {
        DWORD nHandles = CollectAndSignalWaitHandles (&hWaitHandles[1], 
                                                      MAXIMUM_WAIT_OBJECTS-1,
                                                      &dwTimeout,
                                                      (WAIT_FAILED == dwWaitResult));

        dwWaitResult = WaitForMultipleObjects (nHandles+1, &hWaitHandles[0], FALSE, dwTimeout);

        if (m_fShutdown) {
            break;
        }

        if (   (dwWaitResult > WAIT_OBJECT_0)
            && (dwWaitResult < MAXIMUM_WAIT_OBJECTS)) {
            // object signaled
            SignalWait (hWaitHandles[dwWaitResult - WAIT_OBJECT_0]);
        }

    }

    // when we reach here, the thread pool is shutting down and no other threads will be accesing this list.
    // i.e. no need to acquire lock.
    DestroyDList (&m_dlWait, EnDeleteItem, m_pPool);

    SetEvent (m_hEvtThreadExited);
}    

/*++

    Routine Description:
        Destructor (protected to disable calls to this from external)

    Arguments:

    Return Value:        

    --*/
CThreadPoolWaitGroup::~CThreadPoolWaitGroup (void) 
{    
    ASSERT (IsDListEmpty (&m_dlWait));
    ASSERT (IsDListEmpty (&m_link));

    if (m_hEvtWaitChanged) {
        CloseHandle (m_hEvtWaitChanged);
    }
    if (m_hEvtThreadExited) {
        CloseHandle (m_hEvtThreadExited);
    }
}
    
    
/*++

    Routine Description:
        Constructor

    Arguments:

    Return Value:        

    --*/
CThreadPoolWaitGroup::CThreadPoolWaitGroup ()
{
    m_pPool             = NULL;
    m_nWaits            = 0;
    m_fShutdown         = FALSE;
    m_dwWaitThreadId    = 0;
    m_hEvtWaitChanged   = NULL;
    m_hEvtThreadExited  = NULL;

    InitDList (&m_dlWait);
    InitDList (&m_link);
    

    // Initialize will fail if any of the above failed
}

/*++

    Routine Description:
        Initialize wait group
        
    Arguments:
        pointer to thread pool where this wait group is created

    Return Value:
        initialization succeeded or not.

    --*/
BOOL CThreadPoolWaitGroup::Initialize (PThreadPool pPool)
{
    ASSERT (pPool);
    ASSERT (!m_dwWaitThreadId);

    m_hEvtWaitChanged = CreateEvent (NULL, FALSE, FALSE, NULL);
    m_hEvtThreadExited = CreateEvent (NULL, FALSE, FALSE, NULL);

    if (m_hEvtWaitChanged && m_hEvtThreadExited) {

        m_pPool = pPool;

        HANDLE hThread = CreateThread(NULL, 0, WaitGroupThreadStub, this, THRD_NO_BASE_FUNCTION, &m_dwWaitThreadId);
        if (hThread) {
            CloseHandle (hThread);
        }

    }
    
    return 0 != m_dwWaitThreadId;
}    

/*++

    Routine Description:
        Destory the wait group

    Arguments:    

    Return Value: 

    --*/
void CThreadPoolWaitGroup::Destroy (void)
{
    // this is only called when by main thread of thread pool when the thread pool is closed.
    // no lock should be held.
    ASSERT (!IsLockHeld ());
    if (m_dwWaitThreadId) {

        m_fShutdown = TRUE;
        SetEvent (m_hEvtWaitChanged);    // signal wait thread to exit

        // wait for the wait thread to exit
        // NOTE: DO NOT WAIT FOR THE THREAD HANDLE. otherwise
        //       we can deadlock if CloseThreadPool is called in DLL_PROCESS_DETACH.
        WaitForSingleObject (m_hEvtThreadExited, INFINITE);
    }
    ASSERT (IsDListEmpty (&m_dlWait));
    delete this;
}

/*++

    Routine Description:
        Activate the "wait". i.e. start waiting on an object with a timeout.

    Arguments:
        pwa - "wait" to activate.
        h   - handle to wait
        dwWaitTime - timeout value in milliseconds

    Return Value: 
        none.

    --*/    
void CThreadPoolWaitGroup::ActivateWait (
    PTP_ITEM pTpItem,
    HANDLE    h,
    DWORD     dwWaitTime
    )
{
    HANDLE hEvt = NULL;

    Lock ();

    pTpItem->hObject = h;

    // store wait time in PeriodicDelayInTicks field, the wait will be activated after callback complete 
    pTpItem->dwPeriod = dwWaitTime;

    // only process the activation if we're not in the middle of callback.
    // the activation will be handled after callback is done.
    if (!m_pPool->IsCallbackPending (pTpItem)) {
        pTpItem->wState |= TP_ITEM_STATE_ACTIVATED;
        hEvt = ReactivateWait (pTpItem);
    }

    Unlock ();

    if (hEvt) {
        SetEvent (hEvt);
    }
}

HANDLE CThreadPoolWaitGroup::ReactivateWait (PTP_ITEM pTpItem)
{
    ASSERT (IsLockHeld ());
    ASSERT (pTpItem->hObject);

    // It's unclear in MSDN document if we should use the old due time + period, like 
    // period timer, in RegisterWaitForSingleObject. Here we use the current time + period
    // for wait.
    pTpItem->dwDueTime = pTpItem->dwPeriod + GetTickCount ();
    RemoveDList (&pTpItem->Link);
    AddToDListTail (&m_dlWait, &pTpItem->Link);

    return m_hEvtWaitChanged;
}

/*++

    Routine Description:
        deactive a wait operation. i.e. remove the wait object from the wait list if possible.
        
    Arguments:
        pTpItem - "wait" to deactivate
        hCompeletionEvent - event to signal when done.
        fCancelPending - cancel outstanding callback (expired, not yet executed)
        fDelete - Optionally delete the wait node

    Return Value:
        non-zero if the "wait" is deactivated (i.e. the wait object is removed from the wait list).

    --*/
BOOL CThreadPoolWaitGroup::DeactivateWait (
    PTP_ITEM   pTpItem,
    HANDLE     hCompletionEvent,
    BOOL       fCancelPending,
    BOOL       fDelete
    )
{
    BOOL fDeactivated = pTpItem     // pTpItem is NULL when TpAlloc failed. We need to release the wait we reserved.
                      ? m_pPool->DequeueItem (pTpItem, hCompletionEvent, fCancelPending, fDelete)
                      : TRUE;

    if (fDeactivated && fDelete) {

        Lock ();
        ReleaseWait ();
        Unlock ();
    }
    
    return fDeactivated;
}

typedef struct {
    PHANDLE pHandles;
    DWORD   cMaxHandles;
    DWORD   cHandles;
    DWORD   dwTimeout;
    DWORD   dwCurrTime;
    BOOL    fCheckBadObject;
    BOOL    fNewThreadNeeded;
} EN_COLLECT_STRUCT, *PEN_COLLECT_STRUCT;

BOOL CThreadPoolWaitGroup::EnCollectWait (PDLIST pItem, LPVOID pEnumData)
{
    PTP_ITEM pTpItem        = (PTP_ITEM) pItem;
    PEN_COLLECT_STRUCT pecs = (PEN_COLLECT_STRUCT) pEnumData;

    ASSERT (pTpItem->hObject);

    DWORD dwWaitResult = pecs->fCheckBadObject
                       ? WaitForSingleObject (pTpItem->hObject, 0)
                       : WAIT_TIMEOUT;

    BOOL  fSignaled    = (WAIT_TIMEOUT != dwWaitResult);
    DWORD dwWaitTime   = pTpItem->dwPeriod;
    DWORD dwTimeLeft   = pTpItem->dwDueTime - pecs->dwCurrTime;
    
    if (!fSignaled && (INFINITE != dwWaitTime)) {

        fSignaled = ((int) dwTimeLeft <= 0);
            
    }

    if (fSignaled) {
        if (((CThreadPoolWaitGroup *)pTpItem->pPool)->ProcessSignaledWaitItem (pTpItem, dwWaitResult)) {
            pecs->fNewThreadNeeded = TRUE;
        }

    } else {        
        // not signaled and not timed out yet. add wait objects
        pecs->pHandles[pecs->cHandles] = pTpItem->hObject;
        pecs->cHandles ++;

        ASSERT (pecs->cHandles <= pecs->cMaxHandles);

        if ((INFINITE != dwWaitTime) 
            && (pecs->dwTimeout > dwTimeLeft)) {
            pecs->dwTimeout = dwTimeLeft;
        }
    }

    return FALSE;   // keep enumerating
}


DWORD CThreadPoolWaitGroup::CollectAndSignalWaitHandles (PHANDLE pHandles, DWORD cMaxHandles, LPDWORD pdwTimeout, BOOL fCheckBadWait)
{
    EN_COLLECT_STRUCT ecs = { pHandles, cMaxHandles, 0, INFINITE, GetTickCount (), fCheckBadWait, FALSE };

    Lock ();
    EnumerateDList (&m_dlWait, EnCollectWait, &ecs);
    Unlock ();

    if (ecs.fNewThreadNeeded) {
        m_pPool->CreateNewThreads ();
    }

    *pdwTimeout = ecs.dwTimeout;
    return ecs.cHandles;
}

BOOL EnFindWait (PDLIST pItem, LPVOID pEnumData)
{
    return ((PTP_ITEM) pItem)->hObject == pEnumData;
}

void CThreadPoolWaitGroup::SignalWait (HANDLE hObject)
{
    PTP_ITEM pTpItem;
    BOOL     fNewThreadNeeded = FALSE;
    Lock ();

    pTpItem = (PTP_ITEM) EnumerateDList (&m_dlWait, EnFindWait, hObject);

    if (pTpItem) {
        fNewThreadNeeded = ProcessSignaledWaitItem (pTpItem, WAIT_OBJECT_0);
    }
    
    Unlock ();
    if (fNewThreadNeeded) {
        m_pPool->NewCallbackThrd (NULL);
    }
}


BOOL CThreadPoolWaitGroup::ProcessSignaledWaitItem (PTP_ITEM pTpItem, DWORD dwWaitResult)
{
    BOOL fCreateNewThread = FALSE;
    ASSERT (IsLockHeld ());
    ASSERT (!m_pPool->IsCallbackPending (pTpItem));
    
    if (WAIT_FAILED != dwWaitResult) {
        // unless is of type REGWAIT_CALLBACK, it's always 1-shot
        if (TP_TYPE_REGWAIT != pTpItem->wType) {
            pTpItem->hObject = NULL;
        }
        
        pTpItem->dwWaitResult = dwWaitResult;

        m_pPool->IncrementCallbackRemaining (pTpItem);

        fCreateNewThread = m_pPool->ProcessExpiredItems ();

    } else {
        // assertion for debugging purpose
        // If the assertion is hit, the object handle passed to SetThreadPoolWait/RegisterWaitForSingleObject
        // is closed. Or an invalid ihandle is passed to them.
        ASSERT (0);

        // the only thing we can do is deactivate it.
        m_pPool->DequeueItem (pTpItem, NULL, TRUE, FALSE);
    }
    return fCreateNewThread;
}

//
// exported APIs
//



/*++

Routine Description:

    This function allocates a new wait object.

Arguments:

    Callback - The callback function to call when the wait completes or times out.

    Context - Contextual information for the callback.

    CallbackEnviron - Describes the environment in which the callback
                      should be called.

Return Value:

    wait object

--*/
extern "C"
PTP_WAIT 
WINAPI 
CreateThreadpoolWait (
    __in        PTP_WAIT_CALLBACK       pCallback,
    __inout_opt PVOID                   pContext,
    __in_opt    PTP_CALLBACK_ENVIRON    pCallbackEnviron
    )
{
    // fault if pCallback is bad
    FaultIfBadPointer (pCallback);

    PTP_WAIT    pWait = NULL;
    PThreadPool pPool = GetThreadpoolFromEnv (pCallbackEnviron);

    if (pPool && pPool->Activate ()) {
        CThreadPoolWaitGroup *pWaitGroup = pPool->ReserveWaitInWaitGroup ();
        if (pWaitGroup) {
            pWait = (PTP_WAIT)pPool->TpAllocItem ((FARPROC)pCallback,
                                                     pContext,
                                                     (PTP_POOL)pWaitGroup,
                                                     GetRaceDllFromEnv (pCallbackEnviron),
                                                     TP_TYPE_WAIT);
            if (!pWait) {
                // release the wait we reserved
                pWaitGroup->DeactivateWait (NULL, NULL, TRUE, TRUE);
            }
        }
    }

    DEBUGMSG (DBGTP, (L"CETP Create Wait Pool: 0x%8.8lx, Wait: 0x%8.8lx\r\n", pPool, pWait));    
    return pWait;
}


/*++

Routine Description:

    Sets the wait object. 
    A worker thread calls the wait object's callback function after the handle 
    becomes signaled or after the specified timeout expires.
    
    This function allocates no memory; barring corruption, it will
    succeed.

Arguments:

    pwa - The wait object to activate.

    h - The handle to wait on. 
        If the parameter is NULL, it'll cease to queue new callbacks,
        but the callback already queued will still occur.

    pftTimeout - Supplies the absolute or relative time at which the
              wait should timeout.  If positive, it indicates the time
              since 1/1/1600, measured in 100 nanosecond units; if
              negative, it indicates the negative of the amount of
              time to wait relative to the current time, again
              specified in 100 nanosecond units.

              If it points to 0, the wait times out immediately.
              If pftTimeout is NULL, the wait never timeout.

Return Value:

    None.

--*/
extern "C"
VOID
WINAPI
SetThreadpoolWait(
    _Inout_   PTP_WAIT pwa,
    _In_opt_  HANDLE h,
    _In_opt_  PFILETIME pftTimeout
    )
{
    PTP_ITEM pTpItem = (PTP_ITEM) pwa;
    CThreadPoolWaitGroup *pWaitGroup = (CThreadPoolWaitGroup *) pTpItem->pPool;
    
    // if the assertion is hit, you're calling the API with a PTP_WAIT that is already closed.
    ASSERT (pWaitGroup && (0xcccccccc != (DWORD) pWaitGroup));

    if (pWaitGroup) {
        if (h) {
            // reactivate the wait with new wait object/timeout
            pWaitGroup->ActivateWait (pTpItem, h, pftTimeout);
            DEBUGMSG (DBGTP, (L"CETP Set Wait WaitGroup: 0x%8.8lx, Wait: 0x%8.8lx, timeout: 0x%x\r\n", 
                             pWaitGroup, pwa, GetWaitTimeInMilliseconds (pftTimeout)));
        } else {
            // cancel pending wait
            pWaitGroup->DeactivateWait (pTpItem, NULL, FALSE, FALSE);
        }
    }
    
}

/*++

Routine Description:

    Waits for outstanding wait callbacks to complete and optionally 
    cancels pending callbacks that have not yet started to execute.
    
    This function allocates no memory; barring corruption, it will
    succeed.

Arguments:

    pwa - The wait object returned from CreateThreadpoolWait.

    fCancelPendingCallbacks - whether to cancel queued callbacks that have
          not yet started to execute.

Return Value:

    None.

--*/
extern "C"
VOID
WINAPI
WaitForThreadpoolWaitCallbacks(
    _Inout_   PTP_WAIT pwa,
    _In_      BOOL     fCancelPendingCallbacks
    )
{
    PTP_ITEM pTpItem = (PTP_ITEM) pwa;
    CThreadPoolWaitGroup *pWaitGroup = (CThreadPoolWaitGroup *) pTpItem->pPool;

    // if the assertion is hit, you're calling the API with a PTP_WAIT that is already closed.
    ASSERT (pWaitGroup && (0xcccccccc != (DWORD) pWaitGroup));

    if (pWaitGroup
        && !pWaitGroup->DeactivateWait (pTpItem, NULL, fCancelPendingCallbacks, FALSE)) {
        WaitForSingleObject (pTpItem->DoneEvent, INFINITE);
    }
}

/*++

Routine Description:

    Release the specified wait object.
    
Arguments:

    pwa - The wait object returned from CreateThreadpoolWait.

Return Value:

    None.

--*/
extern "C"
VOID
WINAPI
CloseThreadpoolWait(
    _Inout_   PTP_WAIT pwa
    )
{
    PTP_ITEM pTpItem = (PTP_ITEM) pwa;
    CThreadPoolWaitGroup *pWaitGroup = (CThreadPoolWaitGroup *) pTpItem->pPool;

    // if the assertion is hit, you're calling the API with a PTP_WAIT that is already closed.
    ASSERT (pWaitGroup && (0xcccccccc != (DWORD) pWaitGroup));

    if (pWaitGroup) {
        pWaitGroup->DeactivateWait (pTpItem, NULL, TRUE, TRUE);
    }
}


extern "C"
BOOL
WINAPI
RegisterWaitForSingleObject(
    _Out_       PHANDLE phNewWaitObject,
    _In_        HANDLE hObject,
    _In_        WAITORTIMERCALLBACK Callback,
    _In_opt_    PVOID Context,
    _In_        ULONG dwMilliseconds,
    _In_        ULONG dwFlags
    )
{
    // fault if pCallback is bad
    FaultIfBadPointer (Callback);

    PTP_ITEM pWait = NULL;

    if (ValidateTpFlags (dwFlags)) {
        PThreadPool pPool = GetPerProcessThreadPool ();

        if (pPool && pPool->Activate ()) {
            CThreadPoolWaitGroup *pWaitGroup = pPool->ReserveWaitInWaitGroup ();
            if (pWaitGroup) {
                pWait = pPool->TpAllocItem ((FARPROC)Callback,
                                               Context, 
                                               (PTP_POOL)pWaitGroup,
                                               NULL,
                                               (dwFlags & WT_EXECUTEONLYONCE)? TP_TYPE_REGWAIT_ONCE : TP_TYPE_REGWAIT);

                if (pWait) {
                    pWaitGroup->ActivateWait ((PTP_ITEM) pWait, hObject, dwMilliseconds);
                    *phNewWaitObject = (HANDLE) pWait;
                } else {
                    // release the wait we reserved.
                    pWaitGroup->DeactivateWait (NULL, NULL, TRUE, TRUE);
                }
            }
        }
    }
    return NULL != pWait;
    
}

extern "C"
BOOL
WINAPI
UnregisterWaitEx(
    _In_ HANDLE WaitHandle,
    _In_opt_ HANDLE CompletionEvent
    )
{
    BOOL fDeactivated = FALSE;

    if (WaitHandle) {
        PTP_ITEM              pTpItem    = (PTP_ITEM) WaitHandle;
        CThreadPoolWaitGroup *pWaitGroup = (CThreadPoolWaitGroup *) pTpItem->pPool;

        // if the assertion is hit, you're calling the API with a PTP_WAIT that is already closed.
        ASSERT (pWaitGroup && (0xcccccccc != (DWORD) pWaitGroup));

        if (pWaitGroup) {
            HANDLE hDoneEvent   = pTpItem->DoneEvent;       // save DoneEvent for later use
            
            fDeactivated = pWaitGroup->DeactivateWait (pTpItem, CompletionEvent, FALSE, TRUE);
            
            pTpItem = NULL;     // no dereference of pTpItem from here as memory can be freed. 
                                // set it to null to make sure we don't deref it.
                                
            if (fDeactivated) {
                // no callback in progress. done.
                if (CompletionEvent && (INVALID_HANDLE_VALUE != CompletionEvent)) {
                    SetEvent (CompletionEvent);
                }
            } else {
                // in callback, wait if it's a blocking call
                if (INVALID_HANDLE_VALUE == CompletionEvent) {
                    // blocking
                    WaitForSingleObject (hDoneEvent, INFINITE);
                    fDeactivated = TRUE;
                } else {
                    // non-blocking, fail with ERROR_IO_PENDING
                    SetLastError (ERROR_IO_PENDING);
                }
            }
        }
    } else {
        SetLastError (ERROR_INVALID_PARAMETER);
    }

    return fDeactivated;
}




