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
#pragma once

#include <windows.h>
#include <kato.h>

// Pointer to C++ interface for Kato (all Tux-based tests should
// initialize this in ShellProc)
extern CKato *g_pKato;
BOOL IsKatoInit();

//
// Logging levels
//
#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

// Maximum allowable length for OutputDebugString text
#define MAX_DEBUG_OUT 256
#define countof(x)  (sizeof(x)/sizeof(*(x)))

void Debug(LPCTSTR szFormat, ...);
