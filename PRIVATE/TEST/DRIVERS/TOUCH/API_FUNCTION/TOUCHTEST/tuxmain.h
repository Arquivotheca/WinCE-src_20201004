// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------

#ifndef __TUXDEMO_H__
#define __TUXDEMO_H__

#include "globals.h"
	
// TestProc prototypes

// API TestProcs from APITEST.CPP
TESTPROCAPI DeviceCapsTest     		( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI SetSampleRateTest	   	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI SetPriorityTest	       	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrationPointTest	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrationAbortTest   	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrationTest       	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI InitCursorTest	       	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI CalibrateAPointTest		( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI PowerHandlerTest      	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Function Tests from FUNCTIONTEST.CPP
TESTPROCAPI PenUpDownTest      		( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI PointTest		       	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
TESTPROCAPI DrawingTest		       	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

// Depth Tests from DEPTHTEST.CPP
TESTPROCAPI NullParamTest	       	( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );

#define BASE 8000

// Tux function table
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
	   
   	TEXT("API Tests"					    ), 0,   0,              0, NULL,
	TEXT(	"Query Device Caps Test"		), 1,	0,		 BASE+  1, DeviceCapsTest,  
	TEXT(	"Set Sample Rate Test"			), 1,	0,		 BASE+  2, SetSampleRateTest,
	TEXT(	"Set Priority Test"				), 1,	0,		 BASE+  3, SetPriorityTest,   	   	
	TEXT(	"Read Calibration Point Test"	), 1,	0,	   	 BASE+  4, CalibrationPointTest,
   	TEXT(	"Calibration Abort Test"		), 1,	0,		 BASE+  5, CalibrationAbortTest,
   	TEXT(	"Calibration Test"				), 1,	0,		 BASE+  6, CalibrationTest,   
  	TEXT(	"Initialize Cursor Test"		), 1,	0,		 BASE+  7, InitCursorTest,  
  	TEXT(	"Calibrate A Point Test"		), 1,	0,		 BASE+  8, CalibrateAPointTest,    	
//  	TEXT(	"Power Handler Test"			), 1,   0,		 BASE+  9, PowerHandlerTest,
  	
   	TEXT("Function Tests"			      	), 0,   0,              0, NULL,   
   	TEXT(	"Pen Up / Down Test"			), 1,   0,       BASE+  9, PenUpDownTest, 	
   	TEXT(	"Point Test"					), 1,	0,		 BASE+ 10, PointTest,
	TEXT(	"Drawing Test"					), 1,   0,		 BASE+ 11, DrawingTest,

// depth tests are incomplete -- sometimes cause CESH to lose connection
#if 0
 	TEXT("Depth Tests"						), 0,	0,		       	0, NULL,
	TEXT(	"Null Parameter Test"			), 1,   0,		 BASE+ 20, NullParamTest,	
#endif   	

   	NULL,                                      0,   0,              0, NULL  // marks end of list
};

#endif // __TUXDEMO_H__
