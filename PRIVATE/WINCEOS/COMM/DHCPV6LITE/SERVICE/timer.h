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

//
// Convert from seconds to "long duration timer ticks".
//

#define SECONDS_TO_LONG_TICKS(seconds) ((seconds)/60)

//
// Convert from seconds to "short duration timer ticks".
//

#define SECONDS_TO_SHORT_TICKS(seconds) (seconds)

#define MILLISECONDS_IN_SECONDS         1000
#define SEC_TO_MS                       1000
#define NANO100_TO_MS                   10000
#define NANO100_TO_SEC                  10000000

#define DHCPV6_TIMER_INFINITE_INTERVAL   0x7fffffff


typedef struct _DHCPV6_TIMER_MODULE {
    DHCPV6_RW_LOCK RWLock;
    BOOL bDeInitializing;

    HANDLE hTimerQueue;
} DHCPV6_TIMER_MODULE, * PDHCPV6_TIMER_MODULE;


DWORD
InitDhcpV6TimerModule(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule
    );

DWORD
DeInitDhcpV6TimerModule(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule
    );

DWORD
DhcpV6TimerSetTimer(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule,
    PDHCPV6_ADAPT pDhcpV6Adapt,
    WAITORTIMERCALLBACK CallbackRoutine,
    PVOID pvCallbackContext,
    DWORD dwDueTime
    );

DWORD
DhcpV6TimerCancel(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule,
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
DhcpV6TimerFireNow(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule,
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
DhcpV6TimerDelete(
    PDHCPV6_TIMER_MODULE pDhcpV6TimerModule,
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

