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
#ifndef __UTIL_H__
#define __UTIL_H__

#include "globals.h"

//******************************************************************************
//
// Debugging Stuff
//
#define FAIL(x)     g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        __FILE_NAME__, __LINE__, TEXT(x) )
                        
#define SUCCESS(x)  g_pKato->Log( LOG_PASS, \
                        TEXT("SUCCESS: %s"), TEXT(x) )

#define WARN(x)     g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        __FILE_NAME__, __LINE__, TEXT(x) )

#define DETAIL(x)   g_pKato->Log( LOG_DETAIL, TEXT(x) )

void Debug( LPCTSTR, ...);

//******************************************************************************
//
// Helper functions
//
#define countof(a) (sizeof(a)/sizeof(*(a)))
#define VALID_HANDLE(x)     ((INVALID_HANDLE_VALUE != x) && \
                             (NULL != x))
#define INVALID_HANDLE(x)   ((INVALID_HANDLE_VALUE == x) || \
                             (NULL == x))

//
// strings for CD/DVD Rom devices attached to the system
//
#ifdef UNDER_CE
#define CD_DRIVE_STRING     TEXT("\\CDROM Drive")
#else
#define CD_DRIVE_STRING     TEXT("D:")
#endif

#define SEARCH_STRING       TEXT("\\*")

#define TEST_DIR_NAME       TEXT("test_directory")
#define TEST_FILE_NAME      TEXT("test_file.tmp")

#define TEST_FILE_EXT       TEXT("tst")

void SystemErrorMessage( DWORD );

BOOL IsTestFileName( LPCTSTR );

#endif // __UTIL_H__

