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
//  Module: globals.h
//          Declares all global variables and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES global
//          variables, including the function table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __GLOBALS_CPP__
#define GLOBAL
#define INIT(x) = x
#else // __GLOBALS_CPP__
#define GLOBAL  extern
#define INIT(x)
#endif // __GLOBALS_CPP__

////////////////////////////////////////////////////////////////////////////////
// Global macros

#define countof(x)  (sizeof(x)/sizeof(*(x)))

// Maximum # of slots available in preload reg key. This #define is taken from \public\common\oak\inc\pwinuser.h
#define MAX_HKL_PRELOAD_COUNT   20
// Maximum # of Virtual Keys . This #define is taken from \public\common\sdk\inc\keybd.h
#define COUNT_VKEYS 256
#define PRELOAD_PATH TEXT("Keyboard Layout\\Preload\\")
#define LAYOUT_PATH TEXT ("System\\CurrentControlSet\\Control\\Layouts\\")

#define KBD_TYPE         0
#define KBD_SUBTYPE   1
#define KBD_FUNCKEY   2

////////////////////////////////////////////////////////////////////////////////
// Global function prototypes

void            Debug(LPCTSTR, ...);
SHELLPROCAPI    ShellProc(UINT, SPPARAM);
extern TESTPROCAPI getCode(void);
extern BOOL ProcessCmdLine(LPCTSTR CmdLine);
extern BOOL InitList();
extern BOOL InitInputLang();

#include "inputlang.h"

////////////////////////////////////////////////////////////////////////////////
// Globals

// Global CKato logging object. Set while processing SPM_LOAD_DLL message.
GLOBAL CKato            *g_pKato INIT(NULL);

// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO   *g_pShellInfo;
//global structure for input language info 
GLOBAL INPUT_LANGUAGE InputLangInfo;


//Global HKL to store the input locale lists from the registry at the start of the test
GLOBAL HKL g_rghInputLocale[MAX_HKL_PRELOAD_COUNT] INIT({NULL});

//total number of locale's 
GLOBAL UINT g_cInputLocale INIT(0);

//default locale 
GLOBAL HKL g_hDefaultInputLocale INIT(NULL);

//////////////////////////////////////////////




