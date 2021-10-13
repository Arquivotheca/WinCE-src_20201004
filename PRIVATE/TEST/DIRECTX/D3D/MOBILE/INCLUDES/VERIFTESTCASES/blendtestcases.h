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
TESTPROCAPI BlendTest(LPVERIFTESTCASEARGS pTestCaseArgs);

//
// Test Ordinal Extents
//
#define D3DMQA_BLENDTEST_COUNT                        605
#define D3DMQA_BLENDTEST_BASE                         200
#define D3DMQA_BLENDTEST_MAX                         (D3DMQA_BLENDTEST_BASE+D3DMQA_BLENDTEST_COUNT-1)
