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
    lckmgr.cpp

Abstract:
    Lock Manager API.

Revision History:

--*/

#include <lockmgr.h>
#include "lockmgrdbg.hpp"
#include "lockmgrapi.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL
FSDMGR_InstallFileLock(
    PACQUIREFILELOCKSTATE pAcquireFileLockState,
    PRELEASEFILELOCKSTATE pReleaseFileLockState,
    DWORD dwHandle,
    DWORD dwFlags,
    DWORD dwReserved,
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh,
    LPOVERLAPPED lpOverlapped
    )
{
    BOOL f;                        // function result
    PFILELOCKSTATE pFileLockState; // file lock state fetched from FSD
    BOOL fWait = FALSE;            // whether we have to wait to install lock
    LOCKRESULT lrResult;           // LXX_Lock result
    DWORD dwResult;                // WaitForMultipleObject result

lock:;

    // acquire file lock state from FSD

    f = (*pAcquireFileLockState)(dwHandle, &pFileLockState);
    if (!f) {
        DEBUGMSG(1, (_T("FSDMGR_InstallFileLock: Failed to acquire file lock state from FSD\r\n")));
        goto exit;
    }

    // if we were waiting, decrement wait count

    if (fWait) {
        pFileLockState->cQueue -= 1;
        fWait = FALSE;
    }

    // is the file scheduled to be closed?

    if (pFileLockState->fTerminal) {

        // if no other threads are waiting for a lock, then the FSD's thread
        // may be waiting for the last application thread to exit; signal the
        // file lock state's event to wake the FSD's thread up

        if (0 == pFileLockState->cQueue) {
            // SetEvent instead of PulseEvent to avoid race condition
            if (!SetEvent(pFileLockState->hevUnlock)) {
                DEBUGMSG(1, (_T("FSDMGR_InstallFileLock: Failed to failed to set unlock event\r\n")));
            }
        }
        f = FALSE;
        goto release;
    }

    // ensure that the file was opened with GENERIC_READ and/or GENERIC_WRITE

    f = (pFileLockState->dwAccess & (GENERIC_READ|GENERIC_WRITE)) ? TRUE : FALSE;
    if (!f) {
        goto release;
    }

    // if a lock container does not exist, create

    if (NULL == pFileLockState->pvLockContainer) {
        pFileLockState->pvLockContainer = LXX_CreateLockContainer();
        if (NULL == pFileLockState->pvLockContainer) {
            DEBUGMSG(1, (_T("FSDMGR_InstallFileLock: Failed to create lock container\r\n")));
            f = FALSE;
            goto release;
        }
    }

    // install the lock

    lrResult = LXX_Lock(pFileLockState->pvLockContainer, dwHandle, dwFlags, dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);

    switch (lrResult) {

        // error occurred installing lock

        case LR_ERROR:
            f = FALSE;
            goto release;

        // specified lock conflicts with existing lock; we're going to have to
        // wait for the lock to be removed

        case LR_CONFLICT:

            // if the event is null, then we're the first to wait; we're
            // responsible for creating the event

            if (NULL == pFileLockState->hevUnlock) {
                pFileLockState->hevUnlock = CreateEvent(
                    NULL,  // lpEventAttributes (not supported)
                    TRUE,  // bManualReset; when we use PulseEvent, this releases all waiting threads resets to nonsignaled
                    FALSE, // bInitialState (nonsignaled)
                    NULL   // lpName
                    );
                if (NULL == pFileLockState->hevUnlock) {
                    DEBUGMSG(1, (_T("FSDMGR_InstallFileLock: Failed to create unlock event\n")));
                    f = FALSE;
                    goto release;
                }
            }

            pFileLockState->cQueue += 1; // bump the wait count
            fWait = TRUE;                // we'll need to decrement the wait count on re-try

            // release file lock state

            f = (*pReleaseFileLockState)(dwHandle, &pFileLockState);
            if (!f) {
                DEBUGMSG(1, (_T("FSDMGR_InstallFileLock: Failed to release file lock state\r\n")));
                goto exit;
            }

            // wait for unlock

            dwResult = WaitForSingleObject(pFileLockState->hevUnlock, INFINITE);
            if (WAIT_OBJECT_0 != dwResult) {
                DEBUGMSG(1, (_T("FSDMGR_InstallFileLock: Failed to wait for unlock event\r\n")));
                f = FALSE;
                goto exit;
            }

            // try to install the lock, again

            goto lock;
    }

release:;

    // release file lock state

    if (!(*pReleaseFileLockState)(dwHandle, &pFileLockState)) {
        DEBUGMSG(1, (_T("FSDMGR_InstallFileLock: Failed to release file lock stateFSD\r\n")));
        return FALSE;
    }

exit:;

    return f;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL
FSDMGR_RemoveFileLock(
    PACQUIREFILELOCKSTATE pAcquireFileLockState,
    PRELEASEFILELOCKSTATE pReleaseFileLockState,
    DWORD dwHandle,
    DWORD dwReserved,
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh,
    LPOVERLAPPED lpOverlapped
    )
{
    BOOL f;                        // function result
    PFILELOCKSTATE pFileLockState; // file lock state fetched from FSD

    // acquire file lock state from FSD

    f = (*pAcquireFileLockState)(dwHandle, &pFileLockState);
    if (!f) {
        DEBUGMSG(1, (_T("FSDMGR_RemoveFileLock: Failed to acquire file lock state from FSD\r\n")));
        goto exit;
    }

    // if a lock container does not exist, then fail (there isn't a lock to remove)

    if (NULL == pFileLockState->pvLockContainer) {
        f = FALSE;
        goto release;
    }

    // remove lock

    f = LXX_Unlock(pFileLockState->pvLockContainer, dwHandle, dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
    if (!f) {
        goto release;
    }

    // if threads are waiting, then signal the event, as we may have just removed a
    // lock that was preventing a thread from installing its own lock; otherwise,
    // if the lock container is empty, delete/deallocate it

    if (pFileLockState->cQueue > 0) {
        if (!PulseEvent(pFileLockState->hevUnlock)) {
            DEBUGMSG(1, (_T("FSDMGR_RemoveFileLock: Failed to remove; failed to pulse unlock event\r\n")));
            f = FALSE;
            goto release;
        }
    }
    else if (LXX_IsLockContainerEmpty(pFileLockState->pvLockContainer)) {
        LXX_DestroyLockContainer(pFileLockState->pvLockContainer);
        pFileLockState->pvLockContainer = NULL;
    }

release:;

    // release file lock state

    if(!(*pReleaseFileLockState)(dwHandle, &pFileLockState)) {
        DEBUGMSG(1, (_T("FSDMGR_RemoveFileLock: Failed to release file lock state\r\n")));
        return FALSE;
    }

exit:;

    return f;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL
FSDMGR_RemoveFileLockEx(
    PACQUIREFILELOCKSTATE pAcquireFileLockState,
    PRELEASEFILELOCKSTATE pReleaseFileLockState,
    DWORD dwHandle
    )
{
    BOOL f;                        // function result
    PFILELOCKSTATE pFileLockState; // file lock state fetched from FSD

    // acquire file lock state from FSD

    f = (*pAcquireFileLockState)(dwHandle, &pFileLockState);
    if (!f) {
        DEBUGMSG(1, (_T("FSDMGR_RemoveFileLockEx: Failed to acquire file lock state from FSD\r\n")));
        goto exit;
    }

    // if a lock container does not exist, then fail (there aren't locks to remove)

    if (NULL == pFileLockState->pvLockContainer) {
        f = TRUE;
        goto release;
    }

    // unlock all locks owned by specified handle

    f = LXX_UnlockLocksOwnedByHandle(pFileLockState->pvLockContainer, dwHandle);
    if (!f) {
        DEBUGMSG(1, (_T("FSDMGR_RemoveFileLockEx: Failed to close locks owned by handle(%u)\r\n"), dwHandle));
        goto release;
    }

    // if threads are waiting, then signal the event, as we may have just removed a
    // lock that was preventing a thread from installing its own lock; otherwise,
    // if the lock container is empty, delete/deallocate it

    if (pFileLockState->cQueue > 0) {
        if (!PulseEvent(pFileLockState->hevUnlock)) {
            DEBUGMSG(1, (_T("FSDMGR_RemoveFileLockEx: Failed to close file; failed to pulse unlock event\r\n")));
            f = FALSE;
            goto release;
        }
    }
    else if (LXX_IsLockContainerEmpty(pFileLockState->pvLockContainer)) {
        LXX_DestroyLockContainer(pFileLockState->pvLockContainer);
        pFileLockState->pvLockContainer = NULL;
    }

release:;

    // release file lock state

    if (!(*pReleaseFileLockState)(dwHandle, &pFileLockState)) {
        DEBUGMSG(1, (_T("FSDMGR_RemoveFileLockEx: Failed to release file lock state\r\n")));
        return FALSE;
    }

exit:;

    return f;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL
FSDMGR_TestFileLock(
    PACQUIREFILELOCKSTATE pAcquireFileLockState,
    PRELEASEFILELOCKSTATE pReleaseFileLockState,
    DWORD dwHandle,
    BOOL fRead,
    DWORD cbReadWrite
    )
{
    BOOL f;                        // function result
    PFILELOCKSTATE pFileLockState; // file lock state fetched from FSD

    // acquire file lock state from FSD

    f = (*pAcquireFileLockState)(dwHandle, &pFileLockState);
    if (!f) {
        DEBUGMSG(1, (_T("FSDMGR_TestFileLock: Failed to acquire file lock state from FSD\r\n")));
        goto exit;
    }

    // if lock container does not exist, file does not have locks on it

    if (NULL == pFileLockState->pvLockContainer) {
        f = TRUE;
        goto release;
    }

    // authorize access

    f = LXX_AuthorizeAccess(pFileLockState->pvLockContainer, dwHandle, fRead, pFileLockState->dwPosLow, pFileLockState->dwPosHigh, cbReadWrite);
    if (!f) {
        DEBUGMSG(1, (_T("FSDMGR_TestFileLock: Failed to access file; access denied\r\n")));
        goto release;
    }

release:;

    // release file lock state

    if (!(*pReleaseFileLockState)(dwHandle, &pFileLockState)) {
        DEBUGMSG(1, (_T("FSDMGR_TestFileLock: Failed to release file lock state\r\n")));
        return FALSE;
    }

exit:;

    return f;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

BOOL
FSDMGR_TestFileLockEx(
    PACQUIREFILELOCKSTATE pAcquireFileLockState,
    PRELEASEFILELOCKSTATE pReleaseFileLockState,
    DWORD dwHandle,
    BOOL fRead,
    DWORD cbReadWrite,
    DWORD dwOffsetLow,
    DWORD dwOffsetHigh
    )
{
    BOOL f;                        // function result
    PFILELOCKSTATE pFileLockState; // file lock state fetched from FSD

    // acquire file lock state from FSD

    f = (*pAcquireFileLockState)(dwHandle, &pFileLockState);
    if (!f) {
        DEBUGMSG(1, (_T("FSDMGR_TestFileLockEx: Failed to acquire file lock state from FSD\r\n")));
        goto exit;
    }

    // if lock container does not exist, file does not have locks on it

    if (NULL == pFileLockState->pvLockContainer) {
        f = TRUE;
        goto release;
    }

    // authorize access; override position with supplied offset

    f = LXX_AuthorizeAccess(pFileLockState->pvLockContainer, dwHandle, fRead, dwOffsetLow, dwOffsetHigh, cbReadWrite);
    if (!f) {
        goto release;
    }

release:;

    // release file lock state

    if (!(*pReleaseFileLockState)(dwHandle, &pFileLockState)) {
        DEBUGMSG(1, (_T("FSDMGR_TestFileLockEx: Failed to release file lock state\r\n")));
        return FALSE;
    }

exit:;

    return f;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

VOID
FSDMGR_EmptyLockContainer(
    PFILELOCKSTATE pFileLockState
    )
{
    if (NULL == pFileLockState) {
        return;
    }
    if (NULL == pFileLockState->pvLockContainer) {
        return;
    }

    EnterCriticalSection(pFileLockState->lpcs);

    if (pFileLockState->cQueue > 0) {

        // mark the lock container as terminal; i.e., scheduled for destruction

        pFileLockState->fTerminal = TRUE;

        // signal all waiting threads and wait for the last thread to exit;
        // the waiting threads will wake up, see that the file lock state is
        // terminal, and exit; when the last thread has exited, it will signal
        // the unlock event, wake us up, and let us destroy the lock container

        // signal all waiting threads

        if (!PulseEvent(pFileLockState->hevUnlock)) {
            DEBUGMSG(1, (_T("FSDMGR_EmptyLockContainer: Failed to empty lock container; failed to pulse unlock event\r\n")));
            goto exit;
        }

        // leave the critical section to allow the waiting threads to determine
        // that the lock container is terminal

        LeaveCriticalSection(pFileLockState->lpcs);

        // wait for the last thread to exit

        if (WAIT_OBJECT_0 != WaitForSingleObject(pFileLockState->hevUnlock, INFINITE)) {
            DEBUGMSG(1, (_T("FSDMGR_EmptyLockContainer: Failed to empty lock container; failed to wait on unlock event\r\n")));
        }

        EnterCriticalSection(pFileLockState->lpcs);
    }

exit:;

    LXX_DestroyLockContainer(pFileLockState->pvLockContainer);
    pFileLockState->pvLockContainer = NULL;
    LeaveCriticalSection(pFileLockState->lpcs);
}

#ifdef __cplusplus
}
#endif

