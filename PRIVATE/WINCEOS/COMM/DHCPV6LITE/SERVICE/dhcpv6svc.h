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

    dhcpv6svc.h

Abstract:

    This module contains all of the code prototypes
    to drive the DhcpV6 Service.

Author:

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

