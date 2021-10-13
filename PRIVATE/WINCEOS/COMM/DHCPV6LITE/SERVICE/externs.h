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

    externs.h

Abstract:

    Holds externs for global variables.

Author:

    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/


#ifdef __cplusplus
extern "C" {
#endif

extern SERVICE_STATUS_HANDLE gDhcpV6SvcStatusHandle;

extern BOOL gbDHCPV6RPCServerUp;

extern HANDLE ghServiceStopEvent;

extern CRITICAL_SECTION gcServerListenSection;

extern DWORD gdwServersListening;

extern BOOL gbServerListenSection;

extern CRITICAL_SECTION gcDHCPV6Section;

extern BOOL gbDHCPV6Section;
extern BOOL gbDHCPV6PDEnabled;

extern DHCPV6_RW_LOCK gDhcpV6IniInterfaceTblRWLock;
extern PDHCPV6_RW_LOCK gpDhcpV6IniInterfaceTblRWLock;
extern PINI_INTERFACE_HANDLE gpIniInterfaceHandle;

extern PSECURITY_DESCRIPTOR gpDHCPV6SD;

extern WSADATA gWSAData;

extern LPWSADATA gpWSAData;

extern BOOL gbAdapterInit;
extern HANDLE  ghAddrChangeThread;
extern DHCPV6_RW_LOCK gAdapterRWLock;
extern PDHCPV6_RW_LOCK gpAdapterRWLock;
extern LIST_ENTRY AdapterList;

extern BOOL gbAdapterPendingDeleteInit;
extern DHCPV6_RW_LOCK gAdapterPendingDeleteRWLock;
extern PDHCPV6_RW_LOCK gpAdapterPendingDeleteRWLock;
extern LIST_ENTRY AdapterPendingDeleteList;

extern SOCKADDR_IN6 MCastSockAddr;

extern BOOL gbTimerModuleInit;
extern DHCPV6_TIMER_MODULE gDhcpV6TimerModule;
extern PDHCPV6_TIMER_MODULE gpDhcpV6TimerModule;

extern BOOL gbReplyModuleInit;
extern DHCPV6_REPLY_MODULE gDhcpV6ReplyModule;
extern PDHCPV6_REPLY_MODULE gpDhcpV6ReplyModule;

extern BOOL gbEventModule;
extern DHCPV6_EVENT_MODULE gDhcpV6EventModule;
extern PDHCPV6_EVENT_MODULE gpDhcpV6EventModule;

#ifdef __cplusplus
}
#endif

