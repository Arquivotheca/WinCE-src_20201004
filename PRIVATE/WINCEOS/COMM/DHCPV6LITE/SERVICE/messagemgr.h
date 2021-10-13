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

    messagemgr.h

Abstract:

    Message manager for DhcpV6 windows.

Author:

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

