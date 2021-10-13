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
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

// The purpose of this file is to hold stuff common to testproc.cpp
// and tuxmain.cpp

//
// Common headers
//

#include <windows.h>
#include <tchar.h>
#include <tux.h>
#include <katoex.h>

//
// TBD: Add more common headers if needed
//

//
// Common global externs
//

extern CKato            *g_pKato;
extern SPS_SHELL_INFO   *g_pShellInfo;

//
// TBD: Add more global variable extern declarations if needed
//		Some globals come from tuxmain.cpp (typically command line options)
//		Some globals come from testproc.cpp (typically test specific globals)
//

extern TCHAR g_SampleStringOption[];
extern DWORD g_SampleIntegerOption;
extern BOOL  g_SampleBooleanOption;

//
// Common #defines
//

#define MODULE_NAME     _T("perf_ndis")

#ifdef UNDER_CE

#define Debug NKDbgPrintfW

#ifndef STARTF_USESIZE
#define STARTF_USESIZE     0x00000002
#endif

#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#ifndef _vsntprintf
#define _vsntprintf(d,c,f,a) wvsprintf(d,f,a)
#endif

#endif // UNDER_CE

// NT ASSERT macro
#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#define Debug _tprintf
#endif

#define TESTID_BASE	1000

//
// TBD: Add more common #defines if needed 
//

//
// Common function prototype declarations
//

void LOG(LPWSTR szFmt, ...);
void LogKato(DWORD dwVerbosity, LPCTSTR sz);

//
// TBD: Add more common function prototype declarations if needed 
//

int SetOptions(INT argc, LPTSTR argv[]);
int SendPerf();
int ndisd();
void PrintUsage();

#endif // __GLOBALS_H__