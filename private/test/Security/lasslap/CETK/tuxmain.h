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
