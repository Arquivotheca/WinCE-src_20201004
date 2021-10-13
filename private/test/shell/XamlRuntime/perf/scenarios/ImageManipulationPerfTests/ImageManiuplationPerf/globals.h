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
//  TestTux TUX DLL
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

#include "ft.h"

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

//#define countof(x)  (sizeof(x)/sizeof(*(x)))

////////////////////////////////////////////////////////////////////////////////
// Global function prototypes

void            Debug(LPCTSTR, ...);
SHELLPROCAPI    ShellProc(UINT, SPPARAM);

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

////////////////////////////////////////////////////////////////////////////////
// Globals

// Global CKato logging object. Set while processing SPM_LOAD_DLL message.
GLOBAL CKato                *g_pKato INIT(NULL);

// Global Xamlruntime Application handle and Visual Host handle
GLOBAL IXRApplication       *g_pTestApplication INIT(NULL);
GLOBAL IXRVisualHost        *g_pActiveHost INIT(NULL);
GLOBAL IXRScrollViewer      *g_pScroller INIT(NULL);

// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO       *g_pShellInfo;

GLOBAL HINSTANCE            g_hInstance INIT(NULL);

GLOBAL int                  g_Result INIT(TPR_FAIL);
GLOBAL int                  g_OptionRepeats INIT(3);
GLOBAL int                  g_OptionSeconds INIT(30);
GLOBAL bool                 g_bUseBaml INIT(false);
GLOBAL WCHAR                g_szPathRoot[256] INIT({0});
GLOBAL WCHAR                g_szDataFilePath[256] INIT({0});
GLOBAL HMODULE              g_ResourceModule INIT (NULL);
GLOBAL int                  g_ScrnHeight INIT(0);
GLOBAL int                  g_ScrnWidth INIT(0);
GLOBAL int                  g_dwImageLoadRet INIT(TPR_PASS);
GLOBAL bool                 g_bIsGraph INIT(false);
GLOBAL int                  g_DoneRepeats INIT(0);
GLOBAL bool                 g_bRunningUnderCTK INIT(false);
////////////////////////////////////////////////////////////////////////////////

#endif // __GLOBALS_H__
