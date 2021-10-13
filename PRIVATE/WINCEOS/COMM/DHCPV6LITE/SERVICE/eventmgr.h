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

    eventmgr.h

Abstract:

    Event manager for DHCPv6 client.



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

