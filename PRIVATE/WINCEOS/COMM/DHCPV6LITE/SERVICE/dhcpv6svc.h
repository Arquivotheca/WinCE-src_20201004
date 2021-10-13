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

    dhcpv6svc.h

Abstract:

    This module contains all of the code prototypes
    to drive the DhcpV6 Service.



    FrancisD

Environment

    User Level: Windows

Revision History:


--*/


#ifdef __cplusplus
extern "C" {
#endif


VOID
WINAPI
DhcpV6ServiceMain(
    DWORD dwArgc,
    LPTSTR * lpszArgv
    );

DWORD
DhcpV6SvcUpdateStatus(
    );

DWORD
DhcpV6SvcControlHandler(
    DWORD dwOpCode,
    DWORD dwEventType,
    PVOID pEventData,
    PVOID pContext
    );

VOID
DhcpV6SvcShutdown(
    DWORD dwErrorCode
    );

VOID
DhcpV6Shutdown(
    DWORD dwErrorCode
    );

#ifdef __cplusplus
}
#endif

