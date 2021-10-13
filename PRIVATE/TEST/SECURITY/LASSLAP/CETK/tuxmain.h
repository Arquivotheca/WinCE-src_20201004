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

#include "globals.h"

// Test function prototypes (TestProc's)
TESTPROCAPI ListLapsTest             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ListAEsTest             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI InitDeinitTest             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CreateEnDiagTest             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI VerifyUserTest             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Tux testproc function table
FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    _T("LAP Test"                      ),      0,      0,                               0,  NULL,
    _T("  List Laps"                   ),      1,      0,                               0,  NULL,
    _T("    List All LAPs"             ),      2,      LSTL_ALL,                LSTL +  1,  ListLapsTest,
    _T("  List AEs"                    ),      1,      0,                               0,  NULL,
    _T("    List All AEs"              ),      2,      LSTA_ALL,                LSTA +  1,  ListAEsTest,
    _T("  Init/Deinit"                 ),      1,      0,                               0,  NULL,
    _T("    Init/Deinit"               ),      2,      INI_INI,                 INI  +  1,  InitDeinitTest,
    _T("  CreateEnrollmentDialog"      ),      1,      0,                               0,  NULL,
    _T("    CreateEnrollmentDialog"    ),      2,      CED_CED,                 CED  +  1,  CreateEnDiagTest,
    _T("  VerifyUser"                  ),      1,      0,                               0,  NULL,
    _T("    VerifyUser"                ),      2,      VUS_VUS,                 VUS  +  1,  VerifyUserTest,

    
    NULL,   0,  0,  0,  NULL
};

// Internal functions

#endif // __TUXMAIN_H__
