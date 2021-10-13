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
// Check to see if bot _TUX_SUITE and _STRESS_SUITE are defined 
#if defined(_STRESS_SUITE) && defined(_TUX_SUITE)
#error _TUX_SUITE and _STRESS_SUITE can not be set at the same time
#endif //_STRESS_SUITE

#include <windows.h>
#include <tchar.h>
#ifndef UNDER_CE
#include <strsafe.h>
#endif

#ifdef _STRESS_SUITE
// Include STRESS specific logging macros
#include "logging_stress.h"
#else

#ifndef __MAIN_DEFAULT_H__
#define __MAIN_DEFAULT_H__

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
// logging macros
#define CHECKHR( hr,msg )    \
    if ( FAILED(hr) )    \
    {                    \
        LOG( msg );        \
        LOG( TEXT("%S: ERROR %d@%S - error code: 0x%08x "),    \
                    __FUNCTION__, __LINE__, __FILE__, hr);    \
        goto cleanup;    \
    }                    \
    else                \
    {                    \
        LOG( msg );        \
        LOG( TEXT("Succeeded.") );        \
    }    \

#define FAIL(x)     DebugDetailed( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, x )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)  DebugDetailed( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, x, GetLastError() )

#define ABORT(x)     DebugDetailed( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, x )

// same as ABORT, but also logs GetLastError value
#define ERRABORT(x)  DebugDetailed( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(#x), GetLastError() )

#define WARN(x)     DebugDetailed( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, x )

// same as WARN, but also logs GetLastError value
#define ERRWARN(x)  DebugDetailed( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, x, GetLastError() )

// log's an error, but doesn't log a failure
#define ERR(x)      DebugDetailed( LOG_DETAIL, \
                        TEXT("ERROR in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(#x) )

#define DETAIL(x)   DebugDetailed( LOG_DETAIL, x )

#define PASS(x)     DebugDetailed( LOG_PASS, TEXT(x) )

#define COMMENT(x)  DebugDetailed( LOG_COMMENT, TEXT(x) )

#define SKIP(x)     DebugDetailed( LOG_SKIP, x )

#define UNIMPL(x)   DebugDetailed( LOG_NOT_IMPLEMENTED, TEXT(x) )

#define LOG Debug
void Debug(LPCTSTR szFormat, ...) ;
void DebugDetailed(DWORD verbosity, LPCTSTR szFormat, ...) ;

#endif //_MAIN_DEFAULT_H_


#endif     //_STRESS_SUTE

#define LOG_ENTRY() LOG(TEXT("%s: entry \n"), __FUNCTION__)
#define LOG_EXITHR(hr) LOG(TEXT("%s: exiting with 0x%x \n"), __FUNCTION__, hr)
#define LOG_EXIT LOG(TEXT("%s: exit \n"), __FUNCTION__)

