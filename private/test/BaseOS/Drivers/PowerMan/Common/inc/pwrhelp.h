//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include <windows.h>
#include <tux.h>
#include <katoex.h>

extern CKato    *g_pKato;

extern void LOG(LPCWSTR szFmt, ...);

extern void LOGERR(LPCWSTR szFmt, ...);

// kato logging macros
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




