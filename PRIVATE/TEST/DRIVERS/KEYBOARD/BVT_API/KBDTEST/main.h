/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     main.h  

Abstract:
	 
	 Protoypes for keyboard test procedures

--*/


// ***************** Driver BVTs *****************
//Reporting
TESTPROCAPI Instructions_T             (UINT, TPPARAM , LPFUNCTION_TABLE_ENTRY);

// Keyboard
TESTPROCAPI ManualDepress_T            (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI AutoCheck_T                (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI KeyChording_T              (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI EditText_T                 (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RepeatRateKeyDelay_T       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI AsyncKeyState_T            (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);




// ***************** Test Registration Function Table *****************
extern LPFUNCTION_TABLE_ENTRY g_lpNewFTE;
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {

#define BVT_BASE       10


TEXT("Driver BVTs"),                   0, 0, 0,			  NULL,

   TEXT("Instructions"),               1, 0, BVT_BASE,	  Instructions_T,
	
   TEXT("Keyboard"),                   1, 0, 0,			  NULL,
      TEXT("Manual Key Press"),        2, 0, BVT_BASE+40, ManualDepress_T,
      TEXT("Key Sequence Check"),      2, 0, BVT_BASE+41, AutoCheck_T,
      TEXT("Key Chording")           , 2, 0, BVT_BASE+42, KeyChording_T,
      TEXT("Text Editing"),            2, 0, BVT_BASE+43, EditText_T,
      TEXT("Repeat Rate & Key Delay"), 2, 0, BVT_BASE+44, RepeatRateKeyDelay_T,
	  TEXT("Async Key Test"),		   2, 0, BVT_BASE+45, AsyncKeyState_T,	
   NULL,                               0, 0, 0,			  NULL 
};
