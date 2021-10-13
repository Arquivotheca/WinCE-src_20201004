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

#include "..\globals.h"
    
// TestProc prototypes

// API TestProcs from APITEST.CPP
TESTPROCAPI DeviceCapsTest          ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI SetSampleRateTest       ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI SetPriorityTest           ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );  //pre-CE7 test
TESTPROCAPI CalibrationPointTest    ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrationAbortTest    ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrationTest         ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI NullParamTest           ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI InitCursorTest            ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );  //pre-CE7 test
TESTPROCAPI CalibrateAPointTest       ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );  //pre-CE7 test

// Function Tests from FUNCTIONTEST.CPP
TESTPROCAPI PenUpDownTest           ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI PointTest               ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI DrawingTest             ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrationEntriesTest  ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Power Management Tests defined in PowerManagement.cpp
TESTPROCAPI TouchGetPowerCapabilitiesTest( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TouchSuspendPowerStateTest   ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI TouchDevicePowerStateTest    ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Fault injection tests from faultinjection.cpp
TESTPROCAPI TouchDriverUnloadedTest ( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

BOOL IsDriverSupported();

// Tux function table for CE7 touch driver tests
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    TEXT("API Tests"                        ), 0,   0,                  1, NULL,
    TEXT(   "Query Device Caps Test"        ), 1,   0,           API_BASE+  1, DeviceCapsTest,  
    TEXT(   "Set Sample Rate Test"          ), 1,   0,           API_BASE+  2, SetSampleRateTest,
    TEXT(   "Read Calibration Point Test"   ), 1,   0,           API_BASE+  4, CalibrationPointTest,
    TEXT(   "Calibration Abort Test"        ), 1,   0,           API_BASE+  5, CalibrationAbortTest,
    TEXT(   "Calibration Test"              ), 1,   0,           API_BASE+  6, CalibrationTest,
    TEXT(   "Null Parameter Test"           ), 1,   0,           API_BASE+  7, NullParamTest,

    TEXT("Function Tests"                   ), 0,   0,                  2, NULL,   
    TEXT(   "Pen Up / Down Test"            ), 1,   0,           FUNC_BASE+ 1, PenUpDownTest,  
    TEXT(   "Point Test"                    ), 1,   0,           FUNC_BASE+ 2, PointTest,
    TEXT(   "Drawing Test"                  ), 1,   0,           FUNC_BASE+ 3, DrawingTest,
    TEXT(   "Default Calibration Entries"   ), 1,   0,           FUNC_BASE+ 4, CalibrationEntriesTest,

    TEXT("Power Management Tests"           ), 0,   0,                  3, NULL,   
    TEXT(   "Power Capabilities Test"       ), 1,   0,           PM_BASE+ 1, TouchGetPowerCapabilitiesTest, 
    TEXT(   "Device Power State Test"       ), 1,   0,           PM_BASE+ 2, TouchDevicePowerStateTest,

    TEXT("Fault Injection Tests"            ), 0,   0,                  4, NULL,
    TEXT(   "Touch Driver Unloaded Test"    ), 1,   0,           FI_BASE+ 1, TouchDriverUnloadedTest,   

    NULL,                                      0,   0,              0, NULL  // marks end of list
};

// Tux function table for pre-CE7 touch driver tests
static FUNCTION_TABLE_ENTRY g_lpFTE1[] = {

    TEXT("API Tests"                        ), 0,   0,                  0, NULL,
    TEXT(   "Query Device Caps Test"        ), 1,   0,           FUNC_BASE+  1, DeviceCapsTest,  
    TEXT(   "Set Sample Rate Test"          ), 1,   0,           FUNC_BASE+  2, SetSampleRateTest,
    TEXT(   "Set Priority Test"             ), 1,   0,           FUNC_BASE+  3, SetPriorityTest,  
    TEXT(   "Read Calibration Point Test"   ), 1,   0,           FUNC_BASE+  4, CalibrationPointTest,
    TEXT(   "Calibration Abort Test"        ), 1,   0,           FUNC_BASE+  5, CalibrationAbortTest,
    TEXT(   "Calibration Test"              ), 1,   0,           FUNC_BASE+  6, CalibrationTest,
    TEXT(   "Initialize Cursor Test"        ), 1,   0,           FUNC_BASE+  7, InitCursorTest,  
    TEXT(   "Calibrate A Point Test"        ), 1,   0,           FUNC_BASE+  8, CalibrateAPointTest,  
    
    TEXT("Function Tests"                   ), 0,   0,                  0, NULL,   
    TEXT(   "Pen Up / Down Test"            ), 1,   0,           FUNC_BASE+  9, PenUpDownTest,  
    TEXT(   "Point Test"                    ), 1,   0,           FUNC_BASE+ 10, PointTest,
    TEXT(   "Drawing Test"                  ), 1,   0,           FUNC_BASE+ 11, DrawingTest,
    NULL,                                      0,   0,              0, NULL  // marks end of list
};
#endif // __TUXDEMO_H__
