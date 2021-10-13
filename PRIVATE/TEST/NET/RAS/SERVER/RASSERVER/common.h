//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
