//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++



Module Name:

    mrswlock.c

Abstract:

    This module contains multiple readers / single writer implementation.



    FrancisD

Environment

    User Level: Win32

Revision History:


--*/


#include "precomp.h"


DWORD
InitializeRWLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    )
{
    DWORD dwError = 0;
    SECURITY_ATTRIBUTES SecurityAttributes;


    memset(pDhcpV6RWLock, 0, sizeof(DHCPV6_RW_LOCK));

    __try {
        InitializeCriticalSection(&(pDhcpV6RWLock->csExclusive));
        pDhcpV6RWLock->bInitExclusive = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = GetExceptionCode();
        BAIL_ON_WIN32_ERROR(dwError);
    }

    __try {
        InitializeCriticalSection(&(pDhcpV6RWLock->csShared));
        pDhcpV6RWLock->bInitShared = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
         dwError = GetExceptionCode();
         BAIL_ON_WIN32_ERROR(dwError);
    }

    memset(&SecurityAttributes, 0, sizeof(SECURITY_ATTRIBUTES));

    SecurityAttributes.nLength = sizeof(SecurityAttributes);
    SecurityAttributes.lpSecurityDescriptor = NULL;
    SecurityAttributes.bInheritHandle = TRUE;

    pDhcpV6RWLock->hReadDone = CreateEvent(
                                  &SecurityAttributes,
                                  TRUE,
                                  FALSE,
                                  NULL
                                  );
    if (!pDhcpV6RWLock->hReadDone) {
        dwError = GetLastError();
        BAIL_ON_WIN32_ERROR(dwError);
    }

    SetEvent(pDhcpV6RWLock->hReadDone);

    return (dwError);

error:

    DestroyRWLock(pDhcpV6RWLock);

    return (dwError);
}


VOID
DestroyRWLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    )
{
    if (pDhcpV6RWLock->hReadDone) {
        CloseHandle(pDhcpV6RWLock->hReadDone);
    }

    if (pDhcpV6RWLock->bInitShared == TRUE) {
        DeleteCriticalSection(&(pDhcpV6RWLock->csShared));
        pDhcpV6RWLock->bInitShared = FALSE;
    }

    if (pDhcpV6RWLock->bInitExclusive == TRUE) {
        DeleteCriticalSection(&(pDhcpV6RWLock->csExclusive));
        pDhcpV6RWLock->bInitExclusive = FALSE;
    }

    memset(pDhcpV6RWLock, 0, sizeof(DHCPV6_RW_LOCK));

    return;
}


VOID
AcquireSharedLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    )
{
    //
    // Claim the exclusive critical section. This call blocks if there's
    // an active writer or if there's a writer waiting for active readers
    // to complete.
    //

    EnterCriticalSection(&(pDhcpV6RWLock->csExclusive));

    //
    // Claim access to the reader count. If this blocks, it's only for a
    // brief moment while other reader threads go through to increment or
    // decrement the reader count.
    //

    EnterCriticalSection(&(pDhcpV6RWLock->csShared));

    //
    // Increment the reader count. If this is the first reader then reset
    // the read done event so that the next writer blocks.
    //

    if ((pDhcpV6RWLock->lReaders)++ == 0) {
        ResetEvent(pDhcpV6RWLock->hReadDone);
    }

    //
    // Release access to the reader count.
    //

    LeaveCriticalSection(&(pDhcpV6RWLock->csShared));

    //
    // Release access to the exclusive critical section. This enables
    // other readers  to come through and the next write to wait for
    // active readers to complete which in turn prevents new readers
    // from entering.
    //

    LeaveCriticalSection(&(pDhcpV6RWLock->csExclusive));

    return;
}


VOID
AcquireExclusiveLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    )
{
    DWORD dwStatus = 0;


    //
    // Claim the exclusive critical section. This not only prevents other
    // threads from claiming the write lock, but also prevents any new
    // threads from claiming the read lock.
    //

    EnterCriticalSection(&(pDhcpV6RWLock->csExclusive));

    pDhcpV6RWLock->dwCurExclusiveOwnerThreadId = GetCurrentThreadId();

    //
    // Wait for the active readers to release their read locks.
    //

    dwStatus = WaitForSingleObject(pDhcpV6RWLock->hReadDone, INFINITE);

    ASSERT(dwStatus == WAIT_OBJECT_0);

    return;
}


VOID
ReleaseSharedLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    )
{
    //
    // Claim access to the reader count. If this blocks, it's only for a
    // brief moment while other reader threads go through to increment or
    // decrement the reader count.
    //

    EnterCriticalSection(&(pDhcpV6RWLock->csShared));

    //
    // Decrement the reader count. If this is the last reader, set read
    // done event, which allows the first waiting writer to proceed.
    //

    if (--(pDhcpV6RWLock->lReaders) == 0) {
        SetEvent(pDhcpV6RWLock->hReadDone);
    }

    //
    // Release access to the reader count.
    //

    LeaveCriticalSection(&(pDhcpV6RWLock->csShared));

    return;
}


VOID
ReleaseExclusiveLock(
    PDHCPV6_RW_LOCK pDhcpV6RWLock
    )
{
    //
    // Make the exclusive critical section available to one other writer
    // or to the first reader.
    //

    pDhcpV6RWLock->dwCurExclusiveOwnerThreadId = 0;

    LeaveCriticalSection(&(pDhcpV6RWLock->csExclusive));

    return;
}

