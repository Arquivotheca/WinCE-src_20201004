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

#include "XRCommon.h"
#include <windows.h>

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __GLOBALS_CPP__
#define GLOBAL
#define INIT(x) = x
#else // __GLOBALS_CPP__
#define GLOBAL  extern
#define INIT(x)
#endif // __GLOBALS_CPP__

#define FIRSTFRAME                  L"First Frame"
#define XR_PERF_SESSION_HEADER      L"XR\\"
#define CEPERF_SESSION_FOOTER       L"\\API\\Render\\FirstPaint"
#define PERFSCENARIO_SESSION_FOOTER L"\\API"
#define STR_SIZE                    100

#define CHK_CMDLN_HR(Func, FuncName)                                                                       \
    if(FAILED(hr = Func))                                                                                  \
    {                                                                                                      \
        g_pKato->Log(LOG_COMMENT, L"   ***XamlRuntime Error***   %s failed with HRESULT: 0x%x", FuncName, hr); \
        return false;                                                                                      \
    }

////////////////////////////////////////////////////////////////////////////////
// Global function prototypes

void            Debug(LPCTSTR, ...);
SHELLPROCAPI    ShellProc(UINT, SPPARAM);

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

////////////////////////////////////////////////////////////////////////////////
// Globals

// Global module instance
GLOBAL HMODULE           g_hInstance INIT(NULL);

// Global CKato logging object. Set while processing SPM_LOAD_DLL message.
GLOBAL CKato             *g_pKato INIT(NULL);


// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO    *g_pShellInfo;

// App under test's name
GLOBAL LPCWSTR           g_szAppName INIT(NULL);
// Path to where App exists

GLOBAL LPCWSTR           g_szAppPath INIT(new WCHAR[MAX_PATH]);

// the window name, for finding the window's HWND
GLOBAL LPCWSTR           g_szWindowName INIT(NULL);


// Time in seconds to sleep while the app begins.  Increase this for longer load times.  
GLOBAL int               g_SleepSeconds INIT(6);

// CePerf Session Name
GLOBAL LPWSTR g_lpszCePerfSessionName        INIT(new WCHAR[STR_SIZE]);

// Perf Scenario Namespace
GLOBAL LPWSTR g_lpszPerfScenarioNamespace    INIT(new WCHAR[STR_SIZE]);

// Perf Scenario Session Name
GLOBAL LPWSTR g_lpszPerfScenarioSessionName  INIT(new WCHAR[STR_SIZE]);

// executable name (including path) under test.
GLOBAL LPWSTR g_lpszExeName                  INIT(new WCHAR[MAX_PATH]);

// process Name
GLOBAL LPWSTR g_lpszProcessName              INIT(new WCHAR[STR_SIZE]);

// class name
GLOBAL bool g_bCloseWindow                   INIT(false);

// Parameters to pass into CreateProcess
GLOBAL LPWSTR g_lpszParams                   INIT(new WCHAR[STR_SIZE]);

//Tells the test to save results under /release or the CTK working directory
GLOBAL bool  g_bRunningUnderCTK INIT(false);


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
////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

#endif // __GLOBALS_H__
