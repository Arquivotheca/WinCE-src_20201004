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
#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>
#include <clparse.h>
#include <storemgr.h>
#include <disk_common.h>
#include <FlashMdd.h>
#include <FlashPdd.h>
#include <intsafe.h>
#include "storehlp.h"

// --------------------------------------------------------------------
// Log verbosities
// --------------------------------------------------------------------

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

// --------------------------------------------------------------------
// Kato logging macros
// --------------------------------------------------------------------

#define FAIL(x)     g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)  g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

#define ABORT(x)     g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as ABORT, but also logs GetLastError value
#define ERRABORT(x)  g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

#define WARN(x)     g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as WARN, but also logs GetLastError value
#define ERRWARN(x)  g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

// log's an error, but doesn't log a failure
#define ERR(x)      g_pKato->Log( LOG_DETAIL, \
                        TEXT("ERROR in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

#define DETAIL(x)   g_pKato->Log( LOG_DETAIL, TEXT(x) )

#define COMMENT(x)  g_pKato->Log( LOG_COMMENT, TEXT(x) )

#define SKIP(x)     g_pKato->Log( LOG_SKIP, TEXT(x) )

#define UNIMPL(x)   g_pKato->Log( LOG_NOT_IMPLEMENTED, TEXT(x) )

TESTPROCAPI GetTestResult(void);

void LOG(LPWSTR szFmt, ...);

#endif // __MAIN_H__
