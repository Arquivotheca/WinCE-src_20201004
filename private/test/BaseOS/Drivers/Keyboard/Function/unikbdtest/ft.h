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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module: ft.h
//          Declares the TUX function table and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES the function
//          table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////


#pragma once



// test procedures
TESTPROCAPI Getkbdlayout      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Activatekbdlayout      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetKbdLayoutList      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetKbdTest      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI MapVKeyTest      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

////////////////////////////////////////////////////////////////////////////////
// TUX Function table
//  To create the function table and function prototypes, two macros are
//  available:
//
//      FTH(level, description)
//          (Function Table Header) Used for entries that don't have functions,
//          entered only as headers (or comments) into the function table.
//
//      FTE(level, description, code, param, function)
//          (Function Table Entry) Used for all functions. DON'T use this macro
//          if the "function" field is NULL. In that case, use the FTH macro.
//
//  You must not use the TEXT or _T macros here. This is done by the FTH and FTE
//  macros.
//
//  In addition, the table must be enclosed by the BEGIN_FTE and END_FTE macros.

FUNCTION_TABLE_ENTRY g_lpFTE[] = {

    _T("Unified Keyboard Drivers API Test"), 			0,      0,       0,  	NULL,
    _T( "Get Keyboad Layout Test"),     				1,      0,       1001,  Getkbdlayout,
    _T( "ActivateKeyboardLayout Test"),     				1,      0,       1002,  Activatekbdlayout,    
    _T( "GetKeyboardLayoutList Test"),      				1,      0,       1003,  GetKbdLayoutList,
    _T( "GetKeyboardType Test"),     					1,      0,       1004,  GetKbdTest,
    _T( "Virtual Key to XT Scancode Test (MapVirtualKey)"),	1,      0,       1005,  MapVKeyTest,
    NULL,   0,  0,  0,  NULL
};

////////////////////////////////////////////////////////////////////////////////





