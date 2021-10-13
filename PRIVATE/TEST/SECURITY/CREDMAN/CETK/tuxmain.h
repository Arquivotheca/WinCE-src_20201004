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
TESTPROCAPI ListCredProvPrimitiveTest             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ListCredProvVirtualTest             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

TESTPROCAPI CreateReadUpdateDeleteTest             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Tux testproc function table
FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    _T("Credentials Manager Test"                      ),      0,      0,                               0,  NULL,

   _T(" Credentials Providers Tests"                 ),      1,      0,                               0,  NULL,
   _T(" List Credentials Providers for Primitive Types"               ),      2,      PR_ALL,                 PROVR  +  1,  ListCredProvPrimitiveTest,   
   _T(" List Credentials Providers for Virtual Types"               ),      2,      PR_ALL,                 PROVR  +  2,  ListCredProvVirtualTest,   

    _T("  Create, Read, Update and Delete Credentials Tests"                   ),      1,      0,                               0,  NULL,
    _T(" Create, Read, Update and Delete Credentials Test"             ),      2,      CD_ALL,                CANDD +  1,  CreateReadUpdateDeleteTest,

    NULL,   0,  0,  0,  NULL
};

// Internal functions

#endif // __TUXMAIN_H__
