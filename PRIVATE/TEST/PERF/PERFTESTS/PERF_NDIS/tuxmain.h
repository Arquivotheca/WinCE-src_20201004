//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#ifndef __TUXMAIN_H__
#define __TUXMAIN_H__

#include <cmdline.h>
#include "globals.h"

// Test function prototypes (TestProc's)
TESTPROCAPI	Perf_ndis (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

//	typedef struct _FUNCTION_TABLE_ENTRY {
//		LPCTSTR  lpDescription; // description of test
//		UINT     uDepth;        // depth of item in tree hierarchy
//		DWORD    dwUserData;    // user defined data that will be passed to TestProc at runtime
//		DWORD    dwUniqueID;    // uniquely identifies the test - used in loading/saving scripts
//		TESTPROC lpTestProc;    // pointer to TestProc function to be called for this test
//	} FUNCTION_TABLE_ENTRY, *LPFUNCTION_TABLE_ENTRY;

// Tux testproc function table

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    _T("Sample Tux Test"   ),      0,      0,                        0,  NULL,
	_T("  Test Set 1"        ),    1,      0,                        0,  NULL,
    _T("    Test 1"),              2,      0,          TESTID_BASE+  1,  Perf_ndis,
    NULL,   0,  0,  0,  NULL
};

// Internal functions
VOID ShowUsage();

#endif // __TUXMAIN_H__
