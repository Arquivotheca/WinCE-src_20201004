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
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    dbgrapi.h - headers for debugger related functions
//
#ifndef _NK_DBGRAPI_H_
#define _NK_DBGRAPI_H_

#include "kerncmn.h"

BOOL DbgrInitProcess(PPROCESS pprc, DWORD flags);
BOOL DbgrInitThread(PTHREAD pth);
BOOL DbgrDeInitDebuggeeProcess(PPROCESS pprc);
BOOL DbgrDeInitDebuggerThread(PTHREAD pth);

void DbgrNotifyDllLoad (PTHREAD pth, PPROCESS pproc, PMODULE pMod);
void DbgrNotifyDllUnload (PTHREAD pth, PPROCESS pproc, PMODULE pMod);

#endif _NK_DBGRAPI_H_

