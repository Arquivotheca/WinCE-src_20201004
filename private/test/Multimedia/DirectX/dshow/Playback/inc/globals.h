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
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: globals.h
//          Declares all global variables and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES global
//          variables, including the function table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <tux.h>
#include <kato.h>
#include <string>
#include "Media.h"

using namespace std;
typedef basic_string<TCHAR> tstring;
#define countof(x)  (sizeof(x)/sizeof(*(x)))
#define FAILED_F(status) (((HRESULT)(status) < 0) || ((HRESULT)(status) == S_FALSE))
#define SUCCEEDED_F(status) (((HRESULT)(status) >= 0) && ((HRESULT)(status) != S_FALSE))
#define TEST_MAX_PATH (MAX_PATH+20)       // For invalid long file name test

////////////////////////////////////////////////////////////////////////////////
// Global macros
#ifdef __GLOBALS_CPP__
#define GLOBAL
#define INIT(x) = x
#else // __GLOBALS_CPP__
#define GLOBAL  extern
#define INIT(x)
#endif // __GLOBALS_CPP__


////////////////////////////////////////////////////////////////////////////////
// Global function prototypes
void            Debug(LPCTSTR, ...);
SHELLPROCAPI    ShellProc(UINT, SPPARAM);

////////////////////////////////////////////////////////////////////////////////
// Globals

// Global CKato logging object. Set while processing SPM_LOAD_DLL message.
GLOBAL CKato            *g_pKato INIT( NULL ) ;

// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO   *g_pShellInfo ;

// Add more globals of your own here. There are two macros available for this:
//  GLOBAL  Precede each declaration/definition with this macro.
//  INIT    Use this macro to initialize globals, instead of typing "= ..."
//
// For example, to declare two DWORDs, one uninitialized and the other
// initialized to 0x80000000, you could enter the following code:
//
//  GLOBAL DWORD        g_dwUninit,
//                      g_dwInit INIT(0x80000000);
////////////////////////////////////////////////////////////////////////////////
GLOBAL HINSTANCE globalInst ;

#endif // __GLOBALS_H__
