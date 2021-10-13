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

static WCHAR g_pwchSoftwareDeviceFilename0[MAX_PATH]; // Unused in case of D3DMADAPTER_DEFAULT ( pwchSoftwareDeviceFilename
static WCHAR g_pwchSoftwareDeviceFilename1[MAX_PATH]; // will be set to NULL instead of pointing to this storage)

static WCHAR  g_pwchImageComparitorDir0[MAX_PATH] = D3DQA_DEFAULT_COMPARITOR_PATH;
static WCHAR  g_pwchImageComparitorDir1[MAX_PATH] = D3DQA_DEFAULT_COMPARITOR_PATH;

static WCHAR g_pwchTestDependenciesDir0[MAX_PATH] = D3DQA_DEFAULT_DEPENDENCIES_PATH;
static WCHAR g_pwchTestDependenciesDir1[MAX_PATH] = D3DQA_DEFAULT_DEPENDENCIES_PATH;

//
// Storage, and data, for window argument string fields:
//
//     * WINDOW_ARGS.lpClassName
//     * WINDOW_ARGS.lpWindowName
//
static WCHAR g_lpClassName[] = L"D3DMQA";
static WCHAR g_lpWindowName[] = L"D3DMQA";
