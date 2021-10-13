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
#ifndef __TUXMAIN_H__
#define __TUXMAIN_H__

#include <windows.h>
#include <tchar.h>
#include <tux.h>
#include <katoex.h>
#include <perfloggerapi.h>
#include <perfhlp.h>

#define MODULE_NAME     _T("PERF_REGEX")

// --------------------------------------------------------------------
// Windows CE specific

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

#define DEFAULT_OUTPUT_LOG_FILE_NAME _T("\\Release\\perf_regex.xml")
#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)
#define VALID_POINTER(X)    (X != NULL && 0xcccccccc != (int) X)
#define CHECK_FREE(X) if (VALID_POINTER(X)) free(X)
#define CHECK_DELETE(X) if (VALID_POINTER(X)) delete X
#define CHECK_LOCAL_FREE(X) if (VALID_POINTER(X)) LocalFree(X)
#define CHECK_CLOSE_HANDLE(X) if (VALID_HANDLE(X)) CloseHandle(X)
#define CHECK_FIND_CLOSE(X) if (VALID_HANDLE(X)) FindClose(X)

// Macro to stop perf and clean up the logger object
#define CHECK_END_PERF(x) \
    if (VALID_POINTER(x)) { \
    x->StopSystemMonitor(); \
    x->EndLog(); \
    CHECK_DELETE(x); \
    x=NULL;}
	
// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)  g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

void LOG(LPCTSTR szFmt, ...);
void TRACE(LPCTSTR szFmt, ...);

// externs
extern CKato            *g_pKato;
extern SPS_SHELL_INFO   *g_pShellInfo;

#endif // __TUXMAIN_H__
