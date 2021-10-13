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

////////////////////////////////////////////////////////////////////////////////
// kato logging macros

#define FAIL(x)      OutputDebugString( (TCHAR *)x ) ;

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)  OutputDebugString( (TCHAR *)x ) ;
#define ABORT(x)     OutputDebugString( (TCHAR *)x ) ;

// same as ABORT, but also logs GetLastError value
#define ERRABORT(x) OutputDebugString( (TCHAR *)x ) ;
#define WARN(x)      OutputDebugString( (TCHAR *)x ) ;
// same as WARN, but also logs GetLastError value
#define ERRWARN(x)   OutputDebugString( (TCHAR *)x ) ;
// log's an error, but doesn't log a failure
#define ERR(x)       OutputDebugString( (TCHAR *)x ) ;
#define DETAIL(x)    OutputDebugString( (TCHAR *)x ) ;

#define PASS(x)      OutputDebugString( (TCHAR *)x ) ;

#define COMMENT(x)  OutputDebugString( (TCHAR *)x ) ;

#define SKIP(x)      OutputDebugString( (TCHAR *)x ) ;

#define UNIMPL(x)    OutputDebugString( (TCHAR *)x ) ;


////////////////////////////////////////////////////////////////////////////////
// Log
//  Printf-like wrapping around g_pKato->Log(LOG_DETAIL, ...)
//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...             Additional arguments.
//
// Return value:
//  None.

void Log(LPWSTR szFormat, ...) ;

#endif // __MAIN_TUX_H__
