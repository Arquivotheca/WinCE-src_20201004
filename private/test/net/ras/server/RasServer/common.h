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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef _COMMON_H_
#define _COMMON_H_

#include <windows.h>
#include <ras.h>
#include <raserror.h>

void
RasPrint(
    TCHAR *pFormat, 
    ...);

DWORD GetLineDevices(
    OUT LPRASDEVINFOW *ppRasDevInfo, 
    OUT PDWORD pcDevices,
    IN OUT PDWORD pdwTargetedLine
    );

DWORD RemoveAllLines(
    );

VOID CleanupServer(
    );

#define L2TP_LINE_NAME _T("L2TP")
#define PPTP_LINE_NAME _T("RAS VPN")

#endif
