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


#pragma once
#include "globals.h"

// TestProc prototypes
TESTPROCAPI    CachedMemoryPerfTest   ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI    UnCachedMemoryPerfTest ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI    DDrawMemoryPerfTest    ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI    OverallMemoryPerfTest  ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Tux function table for Memory Performance tests
extern __declspec(selectany) FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    TEXT(   "Cached Memory Performance Test"        ), 1,   0,           1, CachedMemoryPerfTest,  
    TEXT(   "UnCached Memory Performance Test"      ), 1,   0,           2, UnCachedMemoryPerfTest,  
    TEXT(   "DDrawSurf Memory Performance Test"     ), 1,   0,           3, DDrawMemoryPerfTest,  
    TEXT(   "Overall Memory Performance Test"       ), 1,   0,           4, OverallMemoryPerfTest,  
    NULL,                                              0,   0,           0, NULL  // marks end of list
};



