//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
