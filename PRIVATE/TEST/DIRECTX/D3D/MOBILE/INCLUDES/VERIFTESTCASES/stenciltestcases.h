//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// To be included from ..\VerifTestCases.h only

#pragma once

//
// Test function prototype
//
TESTPROCAPI StencilTest(LPVERIFTESTCASEARGS pTestCaseArgs);

//
// Test Ordinal Extents
//
#define D3DMQA_STENCILTEST_COUNT                        146
#define D3DMQA_STENCILTEST_BASE                         900
#define D3DMQA_STENCILTEST_MAX                          (D3DMQA_STENCILTEST_BASE+D3DMQA_STENCILTEST_COUNT-1)

