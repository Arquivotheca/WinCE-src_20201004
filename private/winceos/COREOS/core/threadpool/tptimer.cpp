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

    tptimer.cpp

Abstract:

    Thread pool timer functions


--*/

#include "cetp.hpp"


static LONG g_lTimerQSeqNum;

/*++

Routine Description:

    This function allocates a new timer.  The timer's callback routine
    will be called by a worker thread each time the timer expires.

Arguments:

    Callback - The callback implementing the timer, called by a worker
               thread when the timer expires.

    Context - Contextual information for the callback.

    CallbackEnviron - Describes the environment in which the callback
                      should be called.

Return Value:

    Timer item

--*/
extern "C"
PTP_TIMER
WINAPI
CreateThreadpoolTimer(
    __in        PTP_TIMER_CALLBACK   pCallback,
    __inout_opt PVOID                pContext,
    __in_opt    PTP_CALLBACK_ENVIRON pCallbackEnviron
    )
{
    // fault if pCallback is bad
    FaultIfBadPointer (pCallback);
    
    PTP_TIMER   pti   = NULL;
    PThreadPool pPool = GetThreadpoolFromEnv (pCallbackEnviron);

    if (pPool && pPool->Activate ()) {
        pti = (PTP_TIMER) pPool->TpAllocItem ((FARPROC)pCallback, pContext, (PTP_POOL)pPool, GetRaceDllFromEnv (pCallbackEnviron), TP_TYPE_TIMER);
    }

    DEBUGMSG (DBGTP, (L"CETP Create Timer Pool: 0x%8.8lx, Timer: 0x%8.8lx\r\n", pPool, pti));    
    
    return pti;
}

/*++

Routine Description:

    This function causes the timer's callback to be called by a
    worker thread after the specified timeout has expired.

    This function allocates no memory; barring corruption, it will
    succeed.

Arguments:

    Timer - The timer to set.

    DueTime - Supplies the absolute or relative time at which the
              timer should expire.  If positive, it indicates the time
              since 1/1/1600, measured in 100 nanosecond units; if
              negative, it indicates the negative of the amount of
              time to wait relative to the current time, again
              specified in 100 nanosecond units.

              If DueTime is NULL, the timer will cease to queue new
              callbacks (but callbacks already queued will still
              occur).

    Period - Specifies an optional period for the timer, in
             milliseconds.

    WindowLength - Optionally specifies the amount of time between when the
                   timer may expire and when it must expire--this avoids
                   waking threads unnecessarily. Not supported.

Return Value:

    None.

--*/
extern "C"
VOID
WINAPI
SetThreadpoolTimer(
    __inout  PTP_TIMER pti,
    __in_opt PFILETIME pftDueTime,
    __in     DWORD     msPeriod,
    __in_opt DWORD     msWindowLength
    )
{
    PTP_ITEM    pTpItem = (PTP_ITEM) pti;
    PThreadPool pPool   = (PThreadPool)pTpItem->pPool;
    
    UNREFERENCED_PARAMETER (msWindowLength);

    // if the assertion is hit, you're calling the API with a PTP_TIMER that is already closed.
    ASSERT (pPool && (0xcccccccc != (DWORD) pPool));

    if (pPool) {              
        if (!pftDueTime) {
            // cancel timer (infinite timeout --> never fire
            pPool->DequeueItem (pTpItem, NULL, FALSE, FALSE);            
            DEBUGMSG (DBGTP, (L"CETP Dequeue Timer Pool: 0x%8.8lx, Timer: 0x%8.8lx\r\n", pPool, pTpItem));                                

        } else {

            // update existing timer
            pPool->EnqueueItem (pTpItem, 
                                GetWaitTimeInMilliseconds (pftDueTime),
                                msPeriod);

            DEBUGMSG (DBGTP, (L"CETP Set Timer Pool: 0x%8.8lx, Timer: 0x%8.8lx, msDelay: 0x%8.8lx msPeriod: 0x%8.8lx\r\n", 
                                pPool, pTpItem, GetWaitTimeInMilliseconds (pftDueTime), msPeriod));
        }
    }
    
}

/*++

Routine Description:

    This function indicates whether or not a TP_TIMER is currently set.

Arguments:

    Timer - The timer to check.

Return Value:

    TRUE - The timer is set.

    FALSE - The timer is not set.

--*/
extern "C"
BOOL
WINAPI
IsThreadpoolTimerSet(
    __inout PTP_TIMER pti
    )    
{
    PTP_ITEM pTpItem = (PTP_ITEM) pti;
    return (pTpItem->wState & TP_ITEM_STATE_ACTIVATED)
        && (pTpItem->wType == TP_TYPE_TIMER);
}

/*++

Routine Description:

    This function returns when the timer has no more outstanding
    callbacks, and optionally cancels pending callbacks (caused by
    timer expirations) which have not yet started to execute.

    This function allocates no memory; barring corruption, it will
    succeed.

Arguments:

    Timer - The timer to wait for.

    CancelPendingCallbacks - If TRUE, cancels queued callbacks which
                             have not yet started to execute.

Return Value:

    None.

--*/
extern "C"
VOID
WINAPI
WaitForThreadpoolTimerCallbacks(
    __inout PTP_TIMER pti,
    __in    BOOL      fCancelPendingCallbacks
    )
{
    PTP_ITEM    pTpItem = (PTP_ITEM) pti;
    PThreadPool pPool   = (PThreadPool)pTpItem->pPool;

    // if the assertion is hit, you're calling the API with a PTP_TIMER that is already closed.
    ASSERT (pPool && (0xcccccccc != (DWORD) pPool));

    if (pPool
        && !pPool->DequeueItem(pTpItem, NULL, fCancelPendingCallbacks, FALSE)) {
        WaitForSingleObject (pTpItem->DoneEvent, INFINITE);
    }
}

/*++

Routine Description:

    This function deletes the specified timer.  If there are outstanding 
    callbacks, the timer will be asynchronously freed after all callbacks 
    have completed; otherwise, the timer will be freed immediately.

    This function allocates no memory; barring corruption, it will
    succeed.

Arguments:

    Timer - The timer to release.

Return Value:

    None.

--*/
extern "C"
VOID
WINAPI
CloseThreadpoolTimer(
    __inout PTP_TIMER pti
    )
{
    PTP_ITEM    pTpItem = (PTP_ITEM) pti;
    PThreadPool pPool   = (PThreadPool)pTpItem->pPool;
    
    // if the assertion is hit, you're calling the API with a PTP_TIMER that is already closed.
    ASSERT (pPool && (0xcccccccc != (DWORD) pPool));

    if (pPool) {
        pPool->DequeueItem (pTpItem, NULL, TRUE, TRUE);
    }
    
    DEBUGMSG (DBGTP, (L"CETP Close Timer Timer: 0x%8.8lx\r\n", pti));    
}

typedef struct {
    HANDLE hEvtCleanup;     // internal event to handle clean up
    HANDLE hEvtComplete;    // external event passed in in DeleteTimerQueueEx
    DLIST  dlInProgress;    // used for DeleteTimerQueueEx to keep the callback in progress items
    PTP_POOL pPool;     // thread pool to keep the timer
    PTP_WORK pwk;           // thread pool work to handle cleanup
} TIMERQ, *PTIMERQ;

FORCEINLINE BOOL ValidateTimerQueueTimer (PTP_ITEM pti, PTIMERQ pTimerQ)
{
    PTP_POOL    pPool = pTimerQ? pTimerQ->pPool : (PTP_POOL) GetPerProcessThreadPool ();
    BOOL        fRet  = pti
                     && (pPool == pti->pPool)
                     && ((DWORD) pTimerQ == pti->dwCookie);

    if (!fRet) {
        SetLastError (ERROR_INVALID_PARAMETER);
    }

    return fRet;
}

FORCEINLINE void FreeTimerQ (PTIMERQ pTimerQ)
{
    if (pTimerQ->hEvtCleanup) {
        CloseHandle (pTimerQ->hEvtCleanup);
    }
    if (pTimerQ->pwk) {
        CloseThreadpoolWork (pTimerQ->pwk);
    }
    pTimerQ->pPool = NULL;
    LocalFree (pTimerQ);
}

static void WaitForCompletion (
    PTP_CALLBACK_INSTANCE Instance,
    PVOID                 Context,
    PTP_WORK              Work
    )
{
    PTIMERQ pTimerQ = (PTIMERQ) Context;
    ASSERT (pTimerQ->pwk == Work);

    while (!IsDListEmpty (&pTimerQ->dlInProgress)) {
        WaitForSingleObject (pTimerQ->hEvtCleanup, INFINITE);
    }
    SetEvent (pTimerQ->hEvtComplete);
    FreeTimerQ (pTimerQ);
}

WINBASEAPI
HANDLE
WINAPI
CreateTimerQueue(
    VOID
    )
{
    PTIMERQ  pTimerQ = (PTIMERQ) LocalAlloc (LMEM_FIXED, sizeof (TIMERQ));
    if (pTimerQ) {
        pTimerQ->hEvtCleanup = CreateEvent (NULL, FALSE, FALSE, NULL);
        pTimerQ->pwk         = CreateThreadpoolWork (WaitForCompletion,
                                                     pTimerQ,
                                                     NULL);
        if (!pTimerQ->hEvtCleanup || !pTimerQ->pwk) {
            FreeTimerQ (pTimerQ);
            pTimerQ = NULL;
        } else {
            InitDList (&pTimerQ->dlInProgress);
            pTimerQ->pPool        = (PTP_POOL) GetPerProcessThreadPool ();
            pTimerQ->hEvtComplete = NULL;
        }
    }
    // timer queue is just an event
    return pTimerQ;
}

WINBASEAPI
BOOL
WINAPI
CreateTimerQueueTimer(
    _Out_ PHANDLE phNewTimer,
    _In_opt_ HANDLE TimerQueue,
    _In_ WAITORTIMERCALLBACK Callback,
    _In_opt_ PVOID Parameter,
    _In_ DWORD DueTime,
    _In_ DWORD Period,
    _In_ ULONG Flags
    )
{
    // fault if pCallback is bad
    FaultIfBadPointer (Callback);

    PTP_ITEM pti = NULL;
    if (ValidateTpFlags (Flags)) {

        if ((Flags & WT_EXECUTEONLYONCE) && Period) {
            // Period must be 0 if execute only once.
            SetLastError (ERROR_INVALID_PARAMETER);
            
        } else {
            PTIMERQ    pTimerQ = (PTIMERQ) TimerQueue;
            PThreadPool  pPool = pTimerQ? (PThreadPool) pTimerQ->pPool : GetPerProcessThreadPool ();
            if (pPool && pPool->Activate ()) {
                pti = pPool->TpAllocItem ((FARPROC)Callback,
                                           Parameter,
                                           (PTP_POOL)pPool,
                                           NULL,
                                           TP_TYPE_TIMERQ_TIMER,
                                           (DWORD) TimerQueue);

                if (pti && (INFINITE != DueTime)) {

                    if (DueTime > MAX_THREADPOOL_WAIT_TIME) {
                        DueTime = MAX_THREADPOOL_WAIT_TIME;
                    }
                    pPool->EnqueueItem (pti, 
                                        DueTime,
                                        Period);
                    *phNewTimer = pti;
                }
            } else {
                SetLastError (ERROR_INVALID_PARAMETER);
            }
        }
    }
    
    return NULL != pti;
}

WINBASEAPI
BOOL
WINAPI
DeleteTimerQueueTimer(
    _In_opt_ HANDLE TimerQueue,
    _In_     HANDLE Timer,
    _In_opt_ HANDLE CompletionEvent
    )
{
    BOOL     fDequeued = FALSE;
    PTP_ITEM pti       = (PTP_ITEM) Timer;
    PTIMERQ  pTimerQ   = (PTIMERQ) TimerQueue;

    if (ValidateTimerQueueTimer (pti, pTimerQ)) {
        PThreadPool pPool = (PThreadPool) pti->pPool;

        HANDLE hDoneEvent = pti->DoneEvent;       // save DoneEvent for later use

        fDequeued = pPool->DequeueItem (pti, CompletionEvent, TRUE, TRUE);
        
        pti = NULL;     // no dereference of pti from here as memory can be freed. 
                        // set it to null to make sure we don't deref it.
                            
        if (fDequeued) {
            // no callback in progress. done.
            if (CompletionEvent && (INVALID_HANDLE_VALUE != CompletionEvent)) {
                SetEvent (CompletionEvent);
            }
        } else {
            // in callback, wait if it's a blocking call
            if (INVALID_HANDLE_VALUE == CompletionEvent) {
                // blocking
                WaitForSingleObject (hDoneEvent, INFINITE);
                fDequeued = TRUE;
            } else {
                // non-blocking, fail with ERROR_IO_PENDING
                SetLastError (ERROR_IO_PENDING);
            }
        }
    }
    
    return fDequeued;
}

WINBASEAPI
BOOL
WINAPI
ChangeTimerQueueTimer(
    _In_opt_  HANDLE TimerQueue,
    _Inout_   HANDLE Timer,
    _In_      ULONG DueTime,
    _In_      ULONG Period
    )
{
    PTP_ITEM pti     = (PTP_ITEM) Timer;
    PTIMERQ  pTimerQ = (PTIMERQ) TimerQueue;
    BOOL     fRet    = FALSE;

    if (ValidateTimerQueueTimer (pti, pTimerQ)) {

        PThreadPool pPool = (PThreadPool) pti->pPool;
    
        if (INFINITE == DueTime) {
            // deactivate the timer as it'll never fire again
            pPool->DequeueItem (pti, NULL, FALSE, FALSE);
            fRet = TRUE;    // or should we fail the call???
            
        } else {
            // update timer to the new DueTime/Period
            if (DueTime > MAX_THREADPOOL_WAIT_TIME) {
                DueTime = MAX_THREADPOOL_WAIT_TIME;
            }
            fRet = pPool->EnqueueItem (pti, 
                                       DueTime,
                                       Period);
        }
    }
    return fRet;
}


WINBASEAPI
BOOL
WINAPI
DeleteTimerQueueEx(
    _In_ HANDLE TimerQueue,
    _In_opt_ HANDLE CompletionEvent
    )
{
    PTIMERQ     pTimerQ = (PTIMERQ) TimerQueue;
    PThreadPool pPool   = pTimerQ? (PThreadPool) pTimerQ->pPool : (PThreadPool) -1;
    BOOL        fRet    = FALSE;

    if (GetPerProcessThreadPool () == pPool) {
        pTimerQ->pPool = NULL;
        pPool->DeleteAllItems ((DWORD) TimerQueue, &pTimerQ->dlInProgress, pTimerQ->hEvtCleanup);

        if (INVALID_HANDLE_VALUE == CompletionEvent) {
            // blocking call, wait until all callback completed
            while (!IsDListEmpty (&pTimerQ->dlInProgress)) {
                WaitForSingleObject (pTimerQ->hEvtCleanup, INFINITE);
            }
            CompletionEvent = NULL;
        }
        
        fRet = IsDListEmpty (&pTimerQ->dlInProgress);

        if (fRet) {
            if (CompletionEvent) {
                SetEvent (CompletionEvent);
                CompletionEvent = NULL;
            }
            
        } else {
            SetLastError (ERROR_IO_PENDING);

            if (CompletionEvent) {
                // async, queue a thread to wait for callback to finish.
                // so we can signal completion event
                pTimerQ->hEvtComplete = CompletionEvent;
                SubmitThreadpoolWork (pTimerQ->pwk);
            }
        }
        if (!CompletionEvent) {
            // no async wait, free the timer queue
            FreeTimerQ (pTimerQ);
        }
        

    } else {
        SetLastError (ERROR_INVALID_PARAMETER);
    }
    
    return fRet;
}



