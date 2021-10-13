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

    tpwork.cpp

Abstract:

    Thread pool work functions and wait group implementations


--*/

#include "cetp.hpp"


/*++

Routine Description:

    This function allocates a new work object.

Arguments:

    Callback - A worker thread calls this callback each time you call 
                    SubmitThreadpoolWork to post the work object.

    Context - Contextual information for the callback.

    CallbackEnviron - Describes the environment in which the callback
                      should be called.

Return Value:

    Work object

--*/
extern "C"
PTP_WORK 
WINAPI 
CreateThreadpoolWork(
    __in        PTP_WORK_CALLBACK       pCallback,
    __inout_opt PVOID                   pContext,
    __in_opt    PTP_CALLBACK_ENVIRON    pCallbackEnviron
    )
{
    // fault if pCallback is bad
    FaultIfBadPointer (pCallback);

    PTP_WORK    pwk   = NULL;
    PThreadPool pPool = GetThreadpoolFromEnv (pCallbackEnviron);

    if (pPool && pPool->Activate ()) {
        pwk = (PTP_WORK) pPool->TpAllocItem ((FARPROC)pCallback, pContext, (PTP_POOL)pPool, GetRaceDllFromEnv (pCallbackEnviron), TP_TYPE_WORK);
    }

    DEBUGMSG (DBGTP, (L"CETP Create Work Pool: 0x%8.8lx, Work: 0x%8.8lx\r\n", pPool, pwk));    
    return pwk;
}

/*++

Routine Description:

    Posts a work object to the thread pool. A worker thread calls the 
    work object's callback function.

Arguments:

    pwk - A TP_WORK structure that defines the work object.

Return Value:

--*/
extern "C"
VOID 
WINAPI 
SubmitThreadpoolWork(
    __inout  PTP_WORK pwk
    )
{
    PTP_ITEM  pTpItem = (PTP_ITEM)pwk;
    PThreadPool pPool = (PThreadPool)pTpItem->pPool;

    // if the assertion is hit, you're calling the API with a PTP_WORK that is already closed.
    ASSERT (pPool && (0xcccccccc != (DWORD) pPool));

    if (pPool) {              
        pPool->EnqueueItem (pTpItem, 0, 0);
    }
}

/*++

Routine Description:

    help function to queue a simple callback (TrySubmitThreadpoolCallback or QueueUserWorkItem).

Arguments:

    pPool   - the thread pool

    hModule - RaceDll
    
    Callback - A worker thread calls this callback.

    Context - Contextual information for the callback.

    CallbackType - callback type. must be TP_TYPE_USER_WORK or TP_TYPE_SIMPLE.

Return Value:

    TRUE - success
    FALSE - failure

--*/
static BOOL 
DoSubmitSimpleWork (
    CThreadPool        *pPool,
    LPVOID              hModule,
    FARPROC             pCallback,
    LPVOID              pContext,
    WORD                wType
    )
{
    // fault if pCallback is bad
    FaultIfBadPointer (pCallback);

    PTP_ITEM pTpItem = NULL;

    // must be coming from TrySubmitThreadpoolCallback or QueueUserWorkItem
    ASSERT ((TP_TYPE_SIMPLE == wType) || (TP_TYPE_USER_WORK == wType));

    if (pPool && pPool->Activate ()) {
        pTpItem = pPool->TpAllocItem ((FARPROC)pCallback, pContext, (PTP_POOL)pPool, hModule, wType);

        if (pTpItem && !pPool->EnqueueItem (pTpItem, 0, 0)) {
            // can only happen if thread pool is shutting down. 
            pPool->DequeueItem (pTpItem, NULL, TRUE, TRUE);
            SetLastError (ERROR_INVALID_PARAMETER);
            pTpItem = NULL;
        }
    }


    DEBUGMSG ((DBGTP || !pTpItem), (L"CETP Try Submit Work pTpItem: 0x%8.8lx, Callback: 0x%8.8lx, Context: 0x%8.8lx\r\n", pTpItem, pCallback, pContext));
    return NULL != pTpItem;
}


/*++

Routine Description:

    Requests that a thread pool worker thread call the specified 
    callback function.

    The work node is deleted after the callback is made; user 
    has no way of cancelling a work item scheduled with this
    function call.

Arguments:

    Callback - A worker thread calls this callback.

    Context - Contextual information for the callback.

    CallbackEnviron - Describes the environment in which the callback
                      should be called.

Return Value:

    TRUE - success
    FALSE - failure

--*/
extern "C"
BOOL 
WINAPI 
TrySubmitThreadpoolCallback(
    __in         PTP_SIMPLE_CALLBACK  pCallback,
    __inout_opt  PVOID                pContext,
    __in_opt     PTP_CALLBACK_ENVIRON pCallbackEnviron
    )
{
    return DoSubmitSimpleWork (GetThreadpoolFromEnv (pCallbackEnviron),
                               GetRaceDllFromEnv (pCallbackEnviron),
                               (FARPROC) pCallback,
                               pContext,
                               TP_TYPE_SIMPLE);
}

/*++

Routine Description:

    Waits for outstanding work callbacks to complete and optionally 
    cancels pending callbacks that have not yet started to execute.

    This function allocates no memory; barring corruption, it will
    succeed.

Arguments:

    Work object - The work object to wait for.

    CancelPendingCallbacks - If TRUE, cancels queued callbacks which
                             have not yet started to execute.

Return Value:

    None.

--*/
extern "C"
VOID 
WINAPI 
WaitForThreadpoolWorkCallbacks(
    __inout  PTP_WORK pwk,
    __in     BOOL fCancelPendingCallbacks
    )
{
    PTP_ITEM  pTpItem = (PTP_ITEM)pwk;
    PThreadPool pPool = (PThreadPool)pTpItem->pPool;

    // if the assertion is hit, you're calling the API with a PTP_WORK that is already closed.
    ASSERT (pPool && (0xcccccccc != (DWORD) pPool));

    if (pPool
        && !pPool->DequeueItem(pTpItem, NULL, fCancelPendingCallbacks, FALSE)) {
        WaitForSingleObject (pTpItem->DoneEvent, INFINITE);
    }
}

/*++

Routine Description:

    This function deletes the specified work object.  If there are outstanding 
    callbacks, the object will be asynchronously freed after all callbacks 
    have completed; otherwise, the object will be freed immediately.

    This function allocates no memory; barring corruption, it will
    succeed.

Arguments:

    Work - Work object to release.

Return Value:

    None.

--*/
extern "C"
VOID
WINAPI
CloseThreadpoolWork(
    __inout PTP_WORK pwk
    )
{
    PTP_ITEM  pTpItem = (PTP_ITEM)pwk;
    PThreadPool pPool = (PThreadPool)pTpItem->pPool;
    
    // if the assertion is hit, you're calling the API with a PTP_WORK that is already closed.
    ASSERT (pPool && (0xcccccccc != (DWORD) pPool));

    if (pPool) {
        pPool->DequeueItem (pTpItem, NULL, TRUE, TRUE);
    }
    
    DEBUGMSG (DBGTP, (L"CETP Close Work Object: 0x%8.8lx\r\n", pTpItem));    
}



WINBASEAPI
BOOL
WINAPI
QueueUserWorkItem(
    __in     LPTHREAD_START_ROUTINE Function,
    __in_opt PVOID Context,
    __in     ULONG Flags
    )
{
    BOOL fRet = FALSE;
    if (!Function) {
        SetLastError (ERROR_INVALID_PARAMETER);
    } else {
        fRet = ValidateTpFlags (Flags)
            && DoSubmitSimpleWork (GetPerProcessThreadPool (), NULL, (FARPROC) Function, Context, TP_TYPE_USER_WORK);
    }
    return fRet;
}



