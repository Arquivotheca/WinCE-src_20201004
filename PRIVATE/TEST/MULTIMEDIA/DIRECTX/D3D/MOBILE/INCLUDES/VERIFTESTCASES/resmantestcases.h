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
// To be included from ..\VerifTestCases.h only

#pragma once

//
// Test function prototype
//
TESTPROCAPI ResManTest(LPVERIFTESTCASEARGS pTestCaseArgs);

//
// Test Ordinal Extents
//
#define D3DMQA_RESMANTEST_COUNT                          3
#define D3DMQA_RESMANTEST_BASE                        1100
#define D3DMQA_RESMANTEST_MAX                         (D3DMQA_RESMANTEST_BASE+D3DMQA_RESMANTEST_COUNT-1)
