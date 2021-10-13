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
//
//  Module: main.h
//          Header for all files in the project.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MAIN_TUX_H__
#define __MAIN_TUX_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>

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

extern CKato            *g_pKato ;

////////////////////////////////////////////////////////////////////////////////
// kato logging macros

#define FAIL(x, ...)     g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: ") ## x, \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__ )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x, ...)  g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: ") ## x ## TEXT("; error %u"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, GetLastError() )

// same as FAIL, but also logs hresult value
#define HRFAIL(hr, x, ...)  g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: ") ## x ## TEXT("; error 0x%08x"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, hr )


#define ABORT(x, ...)     g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: ") ## x, \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__ )

// same as ABORT, but also logs GetLastError value
#define ERRABORT(x, ...)  g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: ") ## x ## TEXT("; error %u"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, GetLastError() )


// same as ABORT, but also logs GetLastError value
#define HRABORT(hr, x, ...)  g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: ") ## x ## TEXT("; error 0x%08x"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, hr)


#define WARN(x, ...)     g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d ") ## x, \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__ )

// same as ABORT, but also logs GetLastError value
#define ERRWARN(x, ...)  g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: ") ## x ## TEXT("; error %u"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, GetLastError() )

// same as ABORT, but also logs GetLastError value
#define HRWARN(hr, x, ...)  g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: ") ## x ## TEXT("; error 0x%08x"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, hr)


// log's an error, but doesn't log a failure
#define ERR(x, ...)      g_pKato->Log( LOG_DETAIL, \
                        TEXT("ERROR in %s at line %d: ") ## x, \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__ )

#define DETAIL(x, ...)   g_pKato->Log( LOG_DETAIL, x, __VA_ARGS__ )

#define PASS(x, ...)     g_pKato->Log( LOG_PASS, x, __VA_ARGS__ )

#define COMMENT(x, ...)  g_pKato->Log( LOG_COMMENT, x, __VA_ARGS__ )

#define SKIP(x, ...)     g_pKato->Log( LOG_SKIP, x, __VA_ARGS__ )

#define UNIMPL(x, ...)   g_pKato->Log( LOG_NOT_IMPLEMENTED, x, __VA_ARGS__ )

TESTPROCAPI GetTestResult(void);

void Log(LPWSTR szFormat, ...) ;
void Debug(LPCTSTR szFormat, ...) ;


#endif // __MAIN_TUX_H__
