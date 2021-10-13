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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#pragma once

#include <tux.h>

TESTPROCAPI TuxTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

   //|       Description                                            |   Depth of Item   |    Data to pass to    |  Unique ID for  |     Pointer to      |
   //|       of Test Case                                           | in Tree Hierarchy |  TestProc at runtime  |    test case    |      TestProc       |
   //+--------------------------------------------------------------+-------------------+-----------------------+-----------------+---------------------+
        { _T("Performance Tests                                   "),                  0,                      0,                0,                 NULL},
        { _T("Sphere Test using DrawPrimitive"                     ),                  1,                      0,              100,              TuxTest},
        { _T("Sphere Test using ProcessVertices (no rasterization)"),                  1,                      0,              101,              TuxTest},
        { _T("Sphere Test using DrawPrimitive (no transformations)"),                  1,                      0,              102,              TuxTest},
	{ NULL,                                                                        0,                      0,                0,                 NULL}
};

//  Processes messages from the TUX shell.
SHELLPROCAPI    ShellProc(UINT, SPPARAM);

// Shell info structure (contains instance handle, window handle, DLL handle, command line args, etc.)
SPS_SHELL_INFO   *g_pShellInfo;


