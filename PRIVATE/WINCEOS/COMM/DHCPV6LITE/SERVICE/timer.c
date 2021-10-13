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

    timer.c

Abstract:

    Timer related routines for DHCPv6 windows APIs.



    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

#include "dhcpv6p.h"
#include "wcetim.h"
//#include "precomp.h"
//#include "timer.tmh"


DWORD
InitDhcpV6TimerModule(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule
    )
{
    DWORD dwError = 0;


    DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_TRACE, ("Begin Initializing Timer"));

    memset(pDhcpV6TimerModule, 0, sizeof(DHCPV6_TIMER_MODULE));

    dwError = InitializeRWLock(&pDhcpV6TimerModule->RWLock);
    BAIL_ON_WIN32_ERROR(dwError);

    pDhcpV6TimerModule->hTimerQueue = CreateTimerQueue();
    if(pDhcpV6TimerModule->hTimerQueue == NULL) {
        dwError = GetLastError();
        DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_FATAL, ("FAILED CreateTimerQueue with Error: %!status!", dwError));
        ASSERT(0);
        BAIL_ON_WIN32_ERROR(dwError);
    }

error:

    DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Timer"));

    return dwError;
}


DWORD
DeInitDhcpV6TimerModule(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule
    )
{
    DWORD dwError = 0;
    BOOL bDeleted = FALSE;


    DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_TRACE, ("Begin DeInitializing Timer"));

    AcquireExclusiveLock(&pDhcpV6TimerModule->RWLock);
    if (pDhcpV6TimerModule->bDeInitializing) {
        ReleaseExclusiveLock(&pDhcpV6TimerModule->RWLock);

        dwError = ERROR_DELETE_PENDING;
        BAIL_ON_WIN32_ERROR(dwError);
    }
    pDhcpV6TimerModule->bDeInitializing = TRUE;
    ReleaseExclusiveLock(&pDhcpV6TimerModule->RWLock);

    bDeleted = DeleteTimerQueueEx(pDhcpV6TimerModule->hTimerQueue, INVALID_HANDLE_VALUE);
    if (bDeleted == FALSE) {
        dwError = GetLastError();
        DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_FATAL, ("FAILED DeleteTimerQueueEx with Error: %!status!", dwError));
        ASSERT(0);
        BAIL_ON_WIN32_ERROR(dwError);
    }

    DestroyRWLock(&pDhcpV6TimerModule->RWLock);

error:

    DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_TRACE, ("End DeInitializing Timer with Error: %!status!", dwError));

    return dwError;
}


//
// Add timer to timer queue
// Lock: Adapter: Exclusive
//
DWORD
DhcpV6TimerSetTimer(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule,
    PDHCPV6_ADAPT pDhcpV6Adapt,
    WAITORTIMERCALLBACK CallbackRoutine,
    PVOID pvCallbackContext,
    DWORD dwDueTime
    )
{
    DWORD dwError = 0;


    DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_TRACE, ("Set Timer on Adapt: %d with RefCount: %d, due in: %d", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount, dwDueTime));

    if (pDhcpV6Adapt->bTimerCreated == FALSE) {

        pDhcpV6Adapt->bTimerCreated = CreateTimerQueueTimer(
                                        &pDhcpV6Adapt->hEventTimer,
                                        pDhcpV6TimerModule->hTimerQueue,
                                        CallbackRoutine,
                                        pvCallbackContext,
                                        dwDueTime,
                                        DHCPV6_TIMER_INFINITE_INTERVAL,
                                        0
                                        );
        if (pDhcpV6Adapt->bTimerCreated == FALSE) {
            dwError = GetLastError();
            DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Create Timer on Adapt: %d with Error: %!status!", pDhcpV6Adapt->dwIPv6IfIndex, dwError));
            ASSERT(0);
            BAIL_ON_WIN32_ERROR(dwError);
        }

    } else {
        BOOL bTimerChanged = FALSE;

        if (pDhcpV6Adapt->bEventTimerQueued &&
            ! pDhcpV6Adapt->bEventTimerCancelled) {
            dwError = ERROR_ALREADY_EXISTS;
            DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_WARN, ("WARN Timer Already Queued on Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));
            BAIL_ON_WIN32_ERROR(dwError);
        }

        bTimerChanged = ChangeTimerQueueTimer(
                            pDhcpV6TimerModule->hTimerQueue,
                            pDhcpV6Adapt->hEventTimer,
                            dwDueTime, // Will not immediately fire when set to 0
                            DHCPV6_TIMER_INFINITE_INTERVAL
                            );
        if (!bTimerChanged) {
            dwError = GetLastError();
            DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Change Timer on Adapt: %d with Error: %!status!", pDhcpV6Adapt->dwIPv6IfIndex, dwError));
            ASSERT(0);
            BAIL_ON_WIN32_ERROR(dwError);
        }
    }

    pDhcpV6Adapt->bEventTimerQueued = TRUE;
    pDhcpV6Adapt->bEventTimerCancelled = FALSE;

error:

    DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_TRACE, ("End Set Timer on with Error: %!status!", dwError));

    return dwError;
}


//
// Cancel Adapters timer event
// Lock: Adapter: Exclusive
//
DWORD
DhcpV6TimerCancel(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule,
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    BOOL bTimerChanged = FALSE;


    DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_TRACE, ("Cancel Timer on Adapt: %d with RefCount: %d", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount));

    if (pDhcpV6Adapt->bTimerCreated == FALSE) {
        return dwError;
    }

    if (pDhcpV6Adapt->bEventTimerQueued == TRUE){
        pDhcpV6Adapt->bEventTimerCancelled = TRUE;
        bTimerChanged = ChangeTimerQueueTimer(
                            pDhcpV6TimerModule->hTimerQueue,
                            pDhcpV6Adapt->hEventTimer,
                            100, // Will not immediately fire when set to 0
                            DHCPV6_TIMER_INFINITE_INTERVAL
                            );
        if (!bTimerChanged) {
            dwError = GetLastError();
            DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Change Timer on Adapt: %d with Error: %!status!", pDhcpV6Adapt->dwIPv6IfIndex, dwError));
            ASSERT(0);
            BAIL_ON_WIN32_ERROR(dwError);
        }
    }

error:

    return dwError;
}


//
// Fire Adapters timer event NOW
// Lock: Adapter: Exclusive
//
DWORD
DhcpV6TimerFireNow(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule,
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    BOOL bTimerChanged = FALSE;


    DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_TRACE, ("Fire Timer on Adapt: %d with RefCount: %d", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount));

    if (pDhcpV6Adapt->bTimerCreated == FALSE) {
        return dwError;
    }

    if (pDhcpV6Adapt->bEventTimerQueued == TRUE){
        bTimerChanged = ChangeTimerQueueTimer(
                            pDhcpV6TimerModule->hTimerQueue,
                            pDhcpV6Adapt->hEventTimer,
                            100, // Will not immediately fire when set to 0
                            DHCPV6_TIMER_INFINITE_INTERVAL
                            );
        if (!bTimerChanged) {
            dwError = GetLastError();
            DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Fire Timer on Adapt: %d with Error: %!status!", pDhcpV6Adapt->dwIPv6IfIndex, dwError));
            ASSERT(0);
            BAIL_ON_WIN32_ERROR(dwError);
        }
    }

error:

    return dwError;
}

DWORD
DhcpV6TimerDelete(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule,
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    BOOL bTimerDeleted = FALSE;


    DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_TRACE, ("Delete Timer on Adapt: %d with RefCount: %d", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount));

    if (pDhcpV6Adapt->bTimerCreated == TRUE) {
        pDhcpV6Adapt->bEventTimerCancelled = TRUE;
        bTimerDeleted = DeleteTimerQueueTimer(
                            pDhcpV6TimerModule->hTimerQueue,
                            pDhcpV6Adapt->hEventTimer,
                            NULL
                            );
        if (!bTimerDeleted) {
            dwError = GetLastError();
            if (dwError != ERROR_IO_PENDING) {
                DhcpV6Trace(DHCPV6_TIMER, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Delete Timer on Adapt: %d with Error: %!status!", pDhcpV6Adapt->dwIPv6IfIndex, dwError));
                ASSERT(0);
                BAIL_ON_WIN32_ERROR(dwError);
            }
            dwError = 0;
        }

        pDhcpV6Adapt->bTimerCreated = FALSE;
    }

error:

    return dwError;
}

