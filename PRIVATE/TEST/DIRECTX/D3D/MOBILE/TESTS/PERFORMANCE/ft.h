//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#include <tux.h>

TESTPROCAPI TuxTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

   //|       Description                                            |   Depth of Item   |    Data to pass to    |  Unique ID for  |     Pointer to      |
   //|       of Test Case                                           | in Tree Hierarchy |  TestProc at runtime  |    test case    |      TestProc       |
   //+--------------------------------------------------------------+-------------------+-----------------------+-----------------+---------------------+
        { _T("Performance Tests                                   "),                  0,                      0,                0,                 NULL},
        { _T("Sphere Test using DrawPrimitive                     "),                  1,                      0,              100,              TuxTest},
        { _T("Sphere Test using ProcessVertices (no rasterization)"),                  1,                      0,              101,              TuxTest},
        { _T("Sphere Test using DrawPrimitive (no transformations)"),                  1,                      0,              102,              TuxTest},
	{ NULL,                                                                        0,                      0,                0,                 NULL}
};

//  Processes messages from the TUX shell.
SHELLPROCAPI    ShellProc(UINT, SPPARAM);

// Shell info structure (contains instance handle, window handle, DLL handle, command line args, etc.)
SPS_SHELL_INFO   *g_pShellInfo;


