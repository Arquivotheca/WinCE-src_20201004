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

    globals.c

Abstract:

    Holds global variable declarations.

Author:

    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/


#include "precomp.h"

SERVICE_STATUS_HANDLE gDhcpV6SvcStatusHandle = NULL;

BOOL gbDHCPV6RPCServerUp = FALSE;

HANDLE ghServiceStopEvent = INVALID_HANDLE_VALUE;

CRITICAL_SECTION gcServerListenSection;

DWORD gdwServersListening = 0;

BOOL gbServerListenSection = FALSE;

CRITICAL_SECTION gcDHCPV6Section;

BOOL gbDHCPV6Section = FALSE;
BOOL gbDHCPV6PDEnabled;

//
// RPC Interface Table
//
DHCPV6_RW_LOCK gDhcpV6IniInterfaceTblRWLock;
PDHCPV6_RW_LOCK gpDhcpV6IniInterfaceTblRWLock = &gDhcpV6IniInterfaceTblRWLock;
PINI_INTERFACE_HANDLE gpIniInterfaceHandle = NULL;


PSECURITY_DESCRIPTOR gpDHCPV6SD = NULL;

WSADATA gWSAData;

LPWSADATA gpWSAData = &gWSAData;


//
// List of all bound adapters.
//
BOOL gbAdapterInit;
HANDLE  ghAddrChangeThread;
DHCPV6_RW_LOCK gAdapterRWLock;
PDHCPV6_RW_LOCK gpAdapterRWLock = &gAdapterRWLock;
LIST_ENTRY AdapterList;

//
// List of all pending delete adapters.
//
BOOL gbAdapterPendingDeleteInit;
DHCPV6_RW_LOCK gAdapterPendingDeleteRWLock;
PDHCPV6_RW_LOCK gpAdapterPendingDeleteRWLock = &gAdapterPendingDeleteRWLock;
LIST_ENTRY AdapterPendingDeleteList;

//
// Timer Module
//
BOOL gbTimerModuleInit;
DHCPV6_TIMER_MODULE gDhcpV6TimerModule;
PDHCPV6_TIMER_MODULE gpDhcpV6TimerModule = &gDhcpV6TimerModule;

SOCKADDR_IN6 MCastSockAddr;

//
// Reply Module
//
BOOL gbReplyModuleInit;
DHCPV6_REPLY_MODULE gDhcpV6ReplyModule;
PDHCPV6_REPLY_MODULE gpDhcpV6ReplyModule = &gDhcpV6ReplyModule;

//
// Event Module
//
BOOL gbEventModule;
DHCPV6_EVENT_MODULE gDhcpV6EventModule;
PDHCPV6_EVENT_MODULE gpDhcpV6EventModule = &gDhcpV6EventModule;

