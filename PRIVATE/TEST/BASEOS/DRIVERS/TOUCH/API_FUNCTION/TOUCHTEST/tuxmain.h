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

#ifndef __TUXDEMO_H__
#define __TUXDEMO_H__

#include "globals.h"
     
// TestProc prototypes

// API TestProcs from APITEST.CPP
TESTPROCAPI DeviceCapsTest               ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI SetSampleRateTest             ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI SetPriorityTest                 ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrationPointTest     ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrationAbortTest        ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrationTest            ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI InitCursorTest                 ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrateAPointTest          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI PowerHandlerTest           ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Function Tests from FUNCTIONTEST.CPP
TESTPROCAPI PenUpDownTest                ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI PointTest                      ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI DrawingTest                      ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

//PDD Performance Tests from PDD_Performance.cpp
TESTPROCAPI TouchPddPerformance100Test( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TouchPddPerformance300Test( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TouchPddPerformance500Test( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );


#define BASE 8000
#define PERF_BASE 9000

// Tux function table
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
        
        TEXT("API Tests"                             ), 0,   0,              0, NULL,
     TEXT(     "Query Device Caps Test"          ), 1,     0,           BASE+  1, DeviceCapsTest,  
     TEXT(     "Set Sample Rate Test"               ), 1,     0,           BASE+  2, SetSampleRateTest,
     TEXT(     "Set Priority Test"                    ), 1,     0,           BASE+  3, SetPriorityTest,                
     TEXT(     "Read Calibration Point Test"     ), 1,     0,              BASE+  4, CalibrationPointTest,
        TEXT(     "Calibration Abort Test"          ), 1,     0,           BASE+  5, CalibrationAbortTest,
        TEXT(     "Calibration Test"                    ), 1,     0,           BASE+  6, CalibrationTest,   
       TEXT(     "Initialize Cursor Test"          ), 1,     0,           BASE+  7, InitCursorTest,  
       TEXT(     "Power Handler Test"               ), 1,   0,           BASE+  9, PowerHandlerTest,
       
        TEXT("Function Tests"                          ), 0,   0,              0, NULL,   
        TEXT(     "Point Test"                         ), 1,     0,           BASE+ 10, PointTest,
     TEXT(     "Drawing Test"                         ), 1,   0,           BASE+ 11, DrawingTest,     
    TEXT(     "Pen Up / Down Test"               ), 1,   0,       BASE+ 12, PenUpDownTest, 

      TEXT("Performance Tests"                         ), 0,     0,                      0, NULL,
     TEXT(     "PDD Performance Test - 100 Samples" ), 1,  0,               PERF_BASE+ 1, TouchPddPerformance100Test,     
     TEXT(     "PDD Performance Test - 300 Samples" ), 1,  0,               PERF_BASE+ 2, TouchPddPerformance300Test,     
     TEXT(     "PDD Performance Test - 500 Samples" ), 1,  0,               PERF_BASE+ 3, TouchPddPerformance500Test,     

        NULL,                                      0,   0,              0, NULL  // marks end of list
};

#endif // __TUXDEMO_H__
