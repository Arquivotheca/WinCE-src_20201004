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
