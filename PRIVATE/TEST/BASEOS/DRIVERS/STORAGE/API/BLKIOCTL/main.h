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
//  BLKIOCTL TUX DLL
//
//  Module: main.h
//          Header for all files in the project.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MAIN_H__
#define __MAIN_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>
#include <diskio.h>
#include <storemgr.h>
#include <clparse.h>
#include <devutils.h>

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

#define SECTOR_SIZE 512

// kato logging macros
#define FAIL(x)     g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, x )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)  g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, x, GetLastError() )

#define ABORT(x)     g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, x )

// same as ABORT, but also logs GetLastError value
#define ERRABORT(x)  g_pKato->Log( LOG_ABORT, \
                        TEXT("ABORT in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, x, GetLastError() )

#define WARN(x)     g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, x )

// same as WARN, but also logs GetLastError value
#define ERRWARN(x)  g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, x, GetLastError() )

// log's an error, but doesn't log a failure
#define ERR(x)      g_pKato->Log( LOG_DETAIL, \
                        TEXT("ERROR in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, x )

#define DETAIL(x)   g_pKato->Log( LOG_DETAIL, x )

#define COMMENT(x)  g_pKato->Log( LOG_COMMENT, x )

#define SKIP(x)     g_pKato->Log( LOG_SKIP, x )

#define UNIMPL(x)   g_pKato->Log( LOG_NOT_IMPLEMENTED, x )

#define LOG_WARN    (g_fFailOnWarn ? LOG_FAIL : LOG_DETAIL)
            
TESTPROCAPI GetTestResult(void);

void LOG(LPWSTR szFmt, ...);

HANDLE GetDiskHandle(LPWSTR pszDisk, DISK_INFO *pDiskInfo);

#endif // __MAIN_H__
