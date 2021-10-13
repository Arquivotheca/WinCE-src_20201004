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
//    intrapi.h - interrupt related API header
//
#ifndef _NK_INTRAPI_H_
#define _NK_INTRAPI_H_

#include "kerncmn.h"

//
// INTRInitialize - initialize interrupt, associate an interrupt event with a SYSINTR
//
BOOL INTRInitialize (DWORD idInt, HANDLE hEvent, LPVOID pvData, DWORD cbData);

//
// INTRDisable - disable a SYSINTR
//
void INTRDisable (DWORD idInt);

//
// INTRDone - done processing an interrupt (re-enable a SYSINTR)
//
void INTRDone (DWORD idInt);

//
// INTRMask - mask a SYSINTR
//
void INTRMask (DWORD idInt, BOOL fDisable);

BOOL HookInterrupt (int hwIntNumber, FARPROC pfnHandler);
BOOL UnhookInterrupt (int hwInterruptNumber, FARPROC pfnHandler);
DWORD NKCallIntChain (BYTE bIRQ);

#endif

