/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997-1999  Microsoft Corporation.  All Rights Reserved.

Module Name:

     main.h  

Abstract:
	 prototypes for driver tests

--*/


// ***************** Driver BVTs *****************

// mouse
TESTPROCAPI Tap_T                      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CornerTap_T                (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);                          
TESTPROCAPI Drag_T                     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY); 
TESTPROCAPI DoubleClickTest_T          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY); 
TESTPROCAPI WheelTest_T          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY); 

// ***************** Test Registration Function Table *****************
extern LPFUNCTION_TABLE_ENTRY g_lpNewFTE;
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {


#define BVT_BASE       10
#define ERROR_BASE     100
#define STRESS_BASE    1000
#define OTHER_BASE     10000

   TEXT("Mouse and Touch Driver BVT"), 0, 0, 0,    NULL,
      TEXT("Simple Left Click"),            1, 0, BVT_BASE+30, Tap_T,
      TEXT("Corner Left Click"),            1, 0, BVT_BASE+31, CornerTap_T,
      TEXT("Double Left Click"),            1, 0, BVT_BASE+32, DoubleClickTest_T,
      TEXT("Left Drag"),                    1, 0, BVT_BASE+33, Drag_T,

	TEXT("Simple Right Click"),            1, 0, BVT_BASE+35, Tap_T,
      TEXT("Corner Right Click"),            1, 0, BVT_BASE+36,  CornerTap_T,
      TEXT("Right Drag"),                    1, 0, BVT_BASE+38, Drag_T,

	TEXT("Simple Middle Click"),            1, 0, BVT_BASE+39, Tap_T,
      TEXT("Corner Middle Click"),            1, 0, BVT_BASE+40, CornerTap_T,
      TEXT("Middle Drag"),                    1, 0, BVT_BASE+42, Drag_T,
      
      
      
      TEXT("Wheel "),                    1, 0, BVT_BASE+34, WheelTest_T,
            
   NULL,                               0, 0, 0,			  NULL 
};
