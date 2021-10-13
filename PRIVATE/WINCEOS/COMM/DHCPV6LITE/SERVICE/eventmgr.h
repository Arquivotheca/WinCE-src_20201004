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

    eventmgr.h

Abstract:

    Event manager for DHCPv6 client.

Author:

    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/


#define DHCPV6_MAX_EVENT_BUCKETS        10

typedef struct _DHCPV6_EVENT_BUCKET {
    DHCPV6_RW_LOCK RWLock;
    BOOL bWorkItemEnabled;
    LIST_ENTRY leEventHead;
} DHCPV6_EVENT_BUCKET, * PDHCPV6_EVENT_BUCKET;

typedef struct _DHCPV6_EVENT_MODULE {
    DHCPV6_RW_LOCK RWLock;
    BOOL bDeInitializing;
    DHCPV6_EVENT_BUCKET EventBucket[DHCPV6_MAX_EVENT_BUCKETS];
} DHCPV6_EVENT_MODULE, * PDHCPV6_EVENT_MODULE;

typedef VOID
(*PDHCPV6_EVENT_FUNCTION) (
    PVOID pvContext1,
    PVOID pvContext2
    );


DWORD
InitDhcpV6EventMgr(
    PDHCPV6_EVENT_MODULE pDhcpV6EventModule
    );

DWORD
DeInitDhcpV6EventMgr(
    PDHCPV6_EVENT_MODULE pDhcpV6EventModule
    );

DWORD
DhcpV6EventAddEvent(
    PDHCPV6_EVENT_MODULE pDhcpV6EventModule,
    ULONG uHashIndex,
    PDHCPV6_EVENT_FUNCTION pDhcpV6EventFn,
    PVOID pvContext1,
    PVOID pvContext2
    );

