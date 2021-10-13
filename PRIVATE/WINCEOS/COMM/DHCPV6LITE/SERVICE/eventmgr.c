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
/*++



Module Name:

    eventmgr.c

Abstract:

    Event manager for DHCPv6 client.

Author:

    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

#include "dhcpv6p.h"
//#include "precomp.h"
//#include "eventmgr.tmh"


#define DHCPV6_HASH_FN(x)    x % DHCPV6_MAX_EVENT_BUCKETS

typedef struct _DHCPV6_EVENT {
    LIST_ENTRY Link;
    PDHCPV6_EVENT_FUNCTION pDhcpV6EventFn;
    PVOID pvContext1;
    PVOID pvContext2;
} DHCPV6_EVENT, * PDHCPV6_EVENT;


DWORD __inline
IniInitDhcpDhcpV6EventBucket(
    PDHCPV6_EVENT_BUCKET pDhcpV6EventBucket
    )
{
    DWORD dwError = 0;


    memset(pDhcpV6EventBucket, 0, sizeof(DHCPV6_EVENT_BUCKET));

    dwError = InitializeRWLock(&pDhcpV6EventBucket->RWLock);
    BAIL_ON_WIN32_ERROR(dwError);

    InitializeListHead(&pDhcpV6EventBucket->leEventHead);

error:
    return dwError;
}


DWORD
InitDhcpV6EventMgr(
    PDHCPV6_EVENT_MODULE pDhcpV6EventModule
    )
{
    DWORD dwError = 0;
    ULONG uIndex = 0;


    DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_TRACE, ("Begin Initializing Event"));

    memset(pDhcpV6EventModule, 0, sizeof(DHCPV6_EVENT_MODULE));

    dwError = InitializeRWLock(&pDhcpV6EventModule->RWLock);
    BAIL_ON_WIN32_ERROR(dwError);

    for(uIndex = 0; uIndex < DHCPV6_MAX_EVENT_BUCKETS; uIndex++) {
        dwError = IniInitDhcpDhcpV6EventBucket(&pDhcpV6EventModule->EventBucket[uIndex]);
        BAIL_ON_WIN32_ERROR(dwError);
    }

error:

    DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Event with Error: %!status!", dwError));

    return dwError;
}


DWORD __inline
IniDeInitDhcpV6EventBucket(
    PDHCPV6_EVENT_BUCKET pDhcpV6EventBucket
    )
{
    DWORD dwError = 0;


    AcquireSharedLock(&pDhcpV6EventBucket->RWLock);
    while (!IsListEmpty(&pDhcpV6EventBucket->leEventHead)) {
        DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_WARN, ("WARN: DeInitializing Event: Waiting for Event Bucket: %p to Complete", pDhcpV6EventBucket));
        ReleaseSharedLock(&pDhcpV6EventBucket->RWLock);
        Sleep(2000);
        AcquireSharedLock(&pDhcpV6EventBucket->RWLock);
    }

    while (pDhcpV6EventBucket->bWorkItemEnabled) {
        DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_WARN, ("WARN: DeInitializing Event: Waiting for Event WorkItem: %p to Complete", pDhcpV6EventBucket));
        ReleaseSharedLock(&pDhcpV6EventBucket->RWLock);
        Sleep(2000);
        AcquireSharedLock(&pDhcpV6EventBucket->RWLock);
    }
    ReleaseSharedLock(&pDhcpV6EventBucket->RWLock);

    DestroyRWLock(&pDhcpV6EventBucket->RWLock);

    return dwError;
}


DWORD
DeInitDhcpV6EventMgr(
    PDHCPV6_EVENT_MODULE pDhcpV6EventModule
    )
{
    DWORD dwError = 0;
    ULONG uIndex = 0;


    DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_TRACE, ("Begin DeInitializing Event"));

    AcquireExclusiveLock(&pDhcpV6EventModule->RWLock);
    if (pDhcpV6EventModule->bDeInitializing) {
        ReleaseExclusiveLock(&pDhcpV6EventModule->RWLock);
        dwError = ERROR_DELETE_PENDING;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    pDhcpV6EventModule->bDeInitializing = TRUE;
    ReleaseExclusiveLock(&pDhcpV6EventModule->RWLock);

    for(uIndex = 0; uIndex < DHCPV6_MAX_EVENT_BUCKETS; uIndex++) {
        dwError = IniDeInitDhcpV6EventBucket(&pDhcpV6EventModule->EventBucket[uIndex]);
        BAIL_ON_WIN32_ERROR(dwError);
    }

error:
    DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_TRACE, ("End DeInitializing Event with Error: %!status!", dwError));

    return dwError;
}


DWORD WINAPI
IniDhcpV6BucketWorker(
    LPVOID lpParameter
    )
{
    DWORD dwError = 0;
    PDHCPV6_EVENT_BUCKET pDhcpV6EventBucket = (PDHCPV6_EVENT_BUCKET)lpParameter;


    DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_TRACE, ("Begin Bucket Worker for Bucket: %p", pDhcpV6EventBucket));

    AcquireExclusiveLock(&pDhcpV6EventBucket->RWLock);

    ASSERT(pDhcpV6EventBucket->bWorkItemEnabled == TRUE);

    while(!IsListEmpty(&pDhcpV6EventBucket->leEventHead)) {
        PLIST_ENTRY pTempListEntry = NULL;
        PDHCPV6_EVENT pTempDhcpV6Event = NULL;

        pTempListEntry = RemoveHeadList(
                            &pDhcpV6EventBucket->leEventHead
                            );

        ReleaseExclusiveLock(&pDhcpV6EventBucket->RWLock);

        pTempDhcpV6Event = CONTAINING_RECORD(pTempListEntry, DHCPV6_EVENT, Link);

        pTempDhcpV6Event->pDhcpV6EventFn(pTempDhcpV6Event->pvContext1, pTempDhcpV6Event->pvContext2);
        FreeDHCPV6Mem(pTempDhcpV6Event);

        AcquireExclusiveLock(&pDhcpV6EventBucket->RWLock);
    }

    pDhcpV6EventBucket->bWorkItemEnabled = FALSE;

    ReleaseExclusiveLock(&pDhcpV6EventBucket->RWLock);

    DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_TRACE, ("End Bucket Worker for Bucket: %p", pDhcpV6EventBucket));

    return dwError;
}

DWORD
IniDhcpV6XXBucketWorker(
CTEEvent *pEvent, VOID *pVoid) {
    DWORD   Status;

    Status = IniDhcpV6BucketWorker(pVoid);
    MemFree(pEvent);
    return Status;
}



DWORD __inline
IniDhcpV6EventAddEventToBucket(
    PDHCPV6_EVENT_BUCKET pDhcpV6EventBucket,
    PDHCPV6_EVENT_FUNCTION pDhcpV6EventFn,
    PVOID pvContext1,
    PVOID pvContext2
    )
{
    DWORD dwError = 0;
    PDHCPV6_EVENT pDhcpV6Event = NULL;
    ULONG uLength = 0;


    uLength = sizeof(DHCPV6_EVENT_BUCKET);

    pDhcpV6Event = AllocDHCPV6Mem(uLength);
    if (!pDhcpV6Event) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    memset(pDhcpV6Event, 0, uLength);

    pDhcpV6Event->pDhcpV6EventFn = pDhcpV6EventFn;
    pDhcpV6Event->pvContext1 = pvContext1;
    pDhcpV6Event->pvContext2 = pvContext2;

    AcquireExclusiveLock(&pDhcpV6EventBucket->RWLock);

    InsertTailList(&pDhcpV6EventBucket->leEventHead, &pDhcpV6Event->Link);

    if (pDhcpV6EventBucket->bWorkItemEnabled == FALSE) {
        BOOL bQueued = FALSE;

        bQueued = QueueUserWorkItem(
                        IniDhcpV6XXBucketWorker,
                        pDhcpV6EventBucket,
                        WT_EXECUTEDEFAULT
                        );
        if (!bQueued) {
            dwError = GetLastError();
            DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_ERROR, ("FAILED: Queue Work Item for Bucket: %p", pDhcpV6EventBucket));
            BAIL_ON_LOCK_ERROR(dwError);
        }

        pDhcpV6EventBucket->bWorkItemEnabled = TRUE;
    }

lock:
    ReleaseExclusiveLock(&pDhcpV6EventBucket->RWLock);

error:

    return dwError;
}


DWORD
DhcpV6EventAddEvent(
    PDHCPV6_EVENT_MODULE pDhcpV6EventModule,
    DWORD dwHashIndex,
    PDHCPV6_EVENT_FUNCTION pDhcpV6EventFn,
    PVOID pvContext1,
    PVOID pvContext2
    )
{
    DWORD dwError = 0;
    ULONG uBucketID = 0;


    DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_TRACE, ("Begin Add Event To Bucket HashIndex: %d", dwHashIndex));

    AcquireSharedLock(&pDhcpV6EventModule->RWLock);
    if (pDhcpV6EventModule->bDeInitializing) {
        dwError = ERROR_DELETE_PENDING;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    uBucketID = DHCPV6_HASH_FN(dwHashIndex);

    DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_TRACE, ("Add Event To Bucket ID: %d", uBucketID));

    dwError = IniDhcpV6EventAddEventToBucket(
                    &pDhcpV6EventModule->EventBucket[uBucketID],
                    pDhcpV6EventFn,
                    pvContext1,
                    pvContext2
                    );
    BAIL_ON_LOCK_ERROR(dwError);

lock:
    ReleaseSharedLock(&pDhcpV6EventModule->RWLock);

    DhcpV6Trace(DHCPV6_EVENT, DHCPV6_LOG_LEVEL_TRACE, ("End Add Event To Bucket Index: %d with Error: %!status!", dwHashIndex, dwError));

    return dwError;
}

