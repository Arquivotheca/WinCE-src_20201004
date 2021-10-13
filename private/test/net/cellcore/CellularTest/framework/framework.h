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

#ifndef _Framework_H
#define _Framework_H

#include <windows.h>

#define TESTMODE_USER1      1
#define TESTMODE_USER2      2

extern DWORD g_dwTestMode;

// Ini file parsing routines
BOOL ReadStaticTestData(VOID);

VOID PrintConfiguration(VOID);

// Test entry point, called from UI worker thread
BOOL TestEntryPoint(VOID);

// Test entry point, called from UI worker thread
BOOL TuxEntryPoint(VOID);

BOOL TestInit();
BOOL TestDeinit();

// Test sequence lookup and execution
int    GetNumTests(VOID);
VOID   DeriveSuiteName(DWORD dwToken, TCHAR **pSuiteName);
VOID   DeriveTestName(DWORD dwToken, TCHAR **pTestName);
BOOL   ExecuteTest(DWORD dwToken);
BOOL   ExecuteInit(DWORD dwToken);
BOOL   ExecuteDeInit(DWORD dwToken);
BOOL   ExecuteSummary(DWORD dwToken);
DWORD  DeriveRandomToken(VOID);
BOOL   SetTestWeight(DWORD dwToken, DWORD weight);
BOOL   GetTestWeight(DWORD dwToken, DWORD *pdwWeight);
BOOL   BuildWeightedLookupTable(VOID);
BOOL SetTestTarget(DWORD dwToken, DWORD percentage);
BOOL SetTestNumFailAllowed(DWORD dwToken, DWORD dwNum);
void IncrementTestPass(DWORD dwToken);
void IncrementTestFail(DWORD dwToken);

BOOL TestSequenceResult (BOOL fPrint = FALSE);

const LPWSTR TOOLNAME = L"Cellular Test";

#endif
