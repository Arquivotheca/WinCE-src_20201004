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

#define MODULE_NAME     _T("FSDTST")

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <storemgr.h>

// --------------------------------------------------------------------
// Windows CE specific code

#ifdef UNDER_CE

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
#define HFILE FILE*
#endif

#define DEF_FILESYS_DLL         _T("EXFAT.DLL")
#define ALT_FILESYS_DLL         _T("FATFSD.DLL")
#define DEF_STORAGE_PROFILE     _T("PCMCIA")
#define DEF_STORAGE_PATH        _T("")
#define DEF_QUICK_FILL_FLAG     TRUE
#define DEF_FORMAT_FLAG         TRUE
#define DEF_FILE_COUNT          100
#define DEF_THREADS_PER_VOL     10
#define DEF_CLUSTER_SIZE        4096
#define DEF_ROOT_DIR_ENTRIES    256

//
// global macros
//
#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)

// kato logging macros
#define FAIL(x)     g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x)  g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

#define SUCCESS(x)  g_pKato->Log( LOG_PASS, \
                        TEXT("SUCCESS: %s"), TEXT(x) )

#define WARN(x)     g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        TEXT(__FILE__), __LINE__, TEXT(x) )

#define DETAIL(x)   g_pKato->Log( LOG_DETAIL, TEXT(x) )

// same as ERRFAIL, but doesn't log it as a failure
#define ERR(x)  g_pKato->Log( LOG_DETAIL, \
                        TEXT("ERROR in %s at line %d: %s; error %u"), \
                        TEXT(__FILE__), __LINE__, TEXT(x), GetLastError() )

void LOG(LPCWSTR szFmt, ...);

typedef struct _TEST_OPTIONS
{
    BOOL fQuickFill;
    BOOL fFormat;
    UINT nFileCount;
    UINT nThreadsPerVol;
    UINT nMaxRootDirs;
    UINT nClusterSize;
    TCHAR szPath[MAX_PATH];
    TCHAR szProfile[PROFILENAMESIZE];
    TCHAR szFilesystem[FILESYSNAMESIZE];
    TCHAR szFilesystemAlt[FILESYSNAMESIZE];
} TEST_OPTIONS, *PTEST_OPTIONS;

// externs
extern CKato            *g_pKato;

extern TEST_OPTIONS     g_testOptions;


extern BOOL g_fZorch;
extern BOOL g_fNonDestructive;

#define NON_DESTRUCTIVE_TEST_COUNT 27
extern const DWORD gc_dwNonDestructiveTestIds[NON_DESTRUCTIVE_TEST_COUNT];
// prototype for helper function to determine if a test is non destructive
BOOL IsWithinTestRange(CKato * g_pKato, DWORD dwTestId);

#endif // __TUXMAIN_H__
