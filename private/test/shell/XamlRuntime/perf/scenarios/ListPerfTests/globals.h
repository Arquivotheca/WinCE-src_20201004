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
#include "EventHandler.h"

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

// Global Xamlruntime Application handle
GLOBAL IXRApplication        *g_pTestApplication INIT(NULL);

// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO        *g_pShellInfo;

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
GLOBAL HINSTANCE        g_hInstance INIT(NULL);

GLOBAL DWORD g_dwRet INIT(TPR_FAIL);
GLOBAL DWORD g_dwImageRet INIT(TPR_PASS);

GLOBAL DWORD g_OptionSeconds INIT(15);
GLOBAL DWORD g_OptionRepeats INIT(1);
GLOBAL bool  g_UseBaml INIT(false);
GLOBAL bool  g_bRunningUnderCTK INIT(false);

GLOBAL int g_Reps INIT(0);
GLOBAL IXRListBox* g_pListBox INIT(NULL);
GLOBAL IXRVisualHost* g_pHost INIT(NULL);
GLOBAL int g_CurrentIndex INIT(49);
GLOBAL bool g_bGoingUp INIT(false);
GLOBAL bool g_bFirstRep INIT(true);

GLOBAL IXRScrollViewer* g_pScroller INIT(NULL);
GLOBAL EventHandler g_eventHandler;
GLOBAL int g_TimerID INIT(0);

GLOBAL bool g_bExitTest INIT(false);
static CRITICAL_SECTION g_cs;

GLOBAL bool g_bManualMode INIT(false);

#endif // __GLOBALS_H__
