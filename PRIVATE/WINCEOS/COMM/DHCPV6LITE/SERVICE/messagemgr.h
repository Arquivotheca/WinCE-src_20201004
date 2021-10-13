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

    messagemgr.h

Abstract:

    Message manager for DhcpV6 windows.



    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

void SetInitialTimeout(PDHCPV6_ADAPT pDhcpV6Adapt, ULONG Time, ULONG MaxTime,
    ULONG MaxReXmits);

DWORD
InitDHCPV6MessageMgr(
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
DHCPV6MessageMgrSolicitMessage(
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
DHCPV6MessageMgrRequestMessage(
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
DHCPV6MessageMgrInfoRequest(
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
DHCPV6MessageMgrRenewMessage(
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
DHCPV6MessageMgrRebindMessage(
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

DWORD
DHCPV6MessageMgrPerformRefresh(
    PDHCPV6_ADAPT pDhcpV6Adapt
    );

VOID
CALLBACK
DHCPV6MessageMgrTimerCallbackRoutine(
    PVOID pvContext,
    BOOLEAN bWaitTimeout
    );

VOID
DHCPV6MessageMgrRequestCallback(
    PVOID pvContext1,
    PVOID pvContext2
    );

#ifdef UNDER_CE

VOID
DHCPV6MessageMgrSolicitCallback(
    PVOID pvContext1,
    PVOID pvContext2
    );

VOID
DHCPV6MessageMgrTCallback(
    PVOID pvContext1,
    PVOID pvContext2
    );

VOID
DHCPV6MessageMgrInfoReqCallback(
    PVOID pvContext1,
    PVOID pvContext2
    );

#endif

