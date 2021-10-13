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
#include <wchar.h>

//
// Data to be passed to test case
//
typedef struct _TEST_CASE_ARGS {
	UINT uiAdapterOrdinal;
	PWCHAR pwchSoftwareDeviceFilename;
	PWCHAR pwchImageComparitorDir;
	PWCHAR pwchTestDependenciesDir;
	FLOAT fTestTolerance;
} TEST_CASE_ARGS, *LPTEST_CASE_ARGS;

//
// Data to be passed to the test case; for use with the windowing class.
//
typedef struct _WINDOW_ARGS {
	UINT uiWindowWidth;
	UINT uiWindowHeight;
	DWORD uiWindowStyle;
	LPCTSTR lpClassName;
	LPCTSTR lpWindowName;
	BOOL bPParmsWindowed;
} WINDOW_ARGS, *LPWINDOW_ARGS;


//
// Instructions about calling the test cases and what to
// do with the results.
//
typedef struct _CALLER_INSTRUCTIONS {

	BOOL           bTestValid[2];       // Which ordinals should cause test execution?

	TEST_CASE_ARGS TestCaseArgs[2];     // Valid only for ordinals indicated by bTestValid

	BOOL           bKeepInputFrames[2]; // Valid for all ordinals, regardless of bTestValid.
	                                    // Test should not delete frames that it did not generate.
	BOOL           bKeepOutputFrames;
	BOOL           bCompare;
} CALLER_INSTRUCTIONS, *LPCALLER_INSTRUCTIONS;

//
// Default strings
//
#define D3DQA_D3DMREF_FILENAME L"D3DMREF.DLL"
#define D3DQA_DEFAULT_COMPARITOR_PATH L"\\Release\\"
#define D3DQA_DEFAULT_DEPENDENCIES_PATH L"\\Release\\"


//
// Storage for both instances' test case argument string fields:
//
//      * TEST_CASE_ARGS.pwchSoftwareDeviceFilename
//      * TEST_CASE_ARGS.pwchImageLoadDir
//      * TEST_CASE_ARGS.pwchImageSaveDir
//

#define D3DQA_FILELENGTH (MAX_PATH)

__declspec(selectany) WCHAR g_pwchSoftwareDeviceFilename0[D3DQA_FILELENGTH]; // Unused in case of D3DMADAPTER_DEFAULT ( pwchSoftwareDeviceFilename
__declspec(selectany) WCHAR g_pwchSoftwareDeviceFilename1[D3DQA_FILELENGTH]; // will be set to NULL instead of pointing to this storage)

__declspec(selectany) WCHAR  g_pwchImageComparitorDir0[D3DQA_FILELENGTH] = D3DQA_DEFAULT_COMPARITOR_PATH;
__declspec(selectany) WCHAR  g_pwchImageComparitorDir1[D3DQA_FILELENGTH] = D3DQA_DEFAULT_COMPARITOR_PATH;

__declspec(selectany) WCHAR g_pwchTestDependenciesDir0[D3DQA_FILELENGTH] = D3DQA_DEFAULT_DEPENDENCIES_PATH;
__declspec(selectany) WCHAR g_pwchTestDependenciesDir1[D3DQA_FILELENGTH] = D3DQA_DEFAULT_DEPENDENCIES_PATH;

//
// Storage, and data, for window argument string fields:
//
//     * WINDOW_ARGS.lpClassName
//     * WINDOW_ARGS.lpWindowName
//
__declspec(selectany) WCHAR g_lpClassName[]  = L"D3DMQA";
__declspec(selectany) WCHAR g_lpWindowName[] = L"D3DMQA";
