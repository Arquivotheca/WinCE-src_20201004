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
#ifndef __reg_HLP__
#define __reg_HLP__

#include <bldver.h>
#include <windbase.h>
#include "tuxmain.h"

#define CHECK_ALLOC(X)      { \
    if (NULL == X) \
        GENERIC_FAIL(LocalAlloc); \
}

#define DUMP_LOCN   {TRACE(TEXT("File %s, line %d\r\n"), _T(__FILE__), __LINE__);}

#define GENERIC_FAIL(X) { \
    TRACE(_T("%s error in %s line %u 0x%x "), _T(#X), _T(__FILE__), __LINE__, GetLastError()); \
    TRACE(_T("\n"));    \
    goto ErrorReturn; \
}

#define FREE(X) { \
    if (X) \
    { \
        if (LocalFree(X)) \
            TRACE(_T(">>>WARNING: LocalFree Failed. 0x%x\n"), GetLastError()); \
         X=NULL; \
    } \
}

#define CLOSE_HANDLE(X)     { \
    if (INVALID_HANDLE_VALUE!=X) \
    { \
        if (!CloseHandle(X)) \
        { \
            TRACE(TEXT("CloseHandle failed 0x%x\n"), GetLastError()); \
            ASSERT(0); \
        } \
        else \
            X=INVALID_HANDLE_VALUE; \
    } \
}

#define REG_FAIL(X, Y) { \
    TRACE(TEXT("%s error in %s line %u 0x%x "), TEXT(#X), TEXT(__FILE__), __LINE__, Y); \
    TRACE(TEXT("\n")); \
    goto ErrorReturn; \
}

#define REG_CLOSE_KEY(X)     { \
    if (0!=X) \
    { \
        if (ERROR_SUCCESS != RegCloseKey(X)) \
        { \
            TRACE(TEXT("RegCloseKey failed \n")); \
            ASSERT(0); \
        } \
        else \
            X = 0; \
    } \
}

#define PERF_FLAG_READABLE  0x0001  //  used by Hlp_GenStringData
#define PERF_FLAG_ALPHA     0x0002
#define PERF_FLAG_ALPHA_NUM 0x0003
#define PERF_FLAG_NUMERIC       0x0004

static DWORD   rg_RegTypes[] ={REG_NONE,
                                REG_SZ,
                                REG_EXPAND_SZ,
                                REG_BINARY,
                                REG_DWORD,
                                REG_DWORD_LITTLE_ENDIAN,
                                REG_DWORD_BIG_ENDIAN,
                                REG_MULTI_SZ };

#define PERF_NUM_REG_TYPES   (sizeof(rg_RegTypes)/sizeof(DWORD))

#define TST_FLAG_READABLE       0x0001  //  used by Hlp_GenStringData
#define TST_FLAG_ALPHA      0x0002
#define TST_FLAG_ALPHA_NUM  0x0003
#define TST_FLAG_NUMERIC        0x0004

#define HLP_FILL_RANDOM     1
#define HLP_FILL_SEQUENTIAL 2
#define HLP_FILL_DONT_CARE  0

//MultiThread Test Stuff
#define REG_CACHE_DEFAULT_CACHE_SIZE 400
#define REG_CACHE_DEFAULT_KEYSIZE 10
#define REG_CACHE_DEFAULT_VALUE_OVERHEAD 10
#define MAX_REGTEST_THREADS 8
#define MAX_REGVALUES 20
#define MAX_REGKEYS 20
#define REG_VALUE_OVERHEAD 5
#define DEFAULT_NUM_TEST_ITERATIONS 4

#define MT_MAX_SLEEP_TIME 60000 //in milliseconds, per test
#define MULTITHREAD_REG_TEST_PATH L"\\TESTMultiThreadRegTEST\\"

static TCHAR g_szMultiThreadRegTestPath[MAX_PATH] = L"\\TESTMultiThreadRegTEST\\";

//struct used in multithread tests, passed to individual threads
struct THREAD_PARAMS{
    UINT nTestRange;
    UINT nIterations;
    UINT nThreadIndex;
    UINT nSizeOfValues;
    UINT nPerfMarker;

};

static TCHAR g_szarrRegValue[MAX_PATH] = {L"RegTestValue00"};

static TCHAR g_szarrRegKeys[MAX_REGKEYS][MAX_PATH] = {
        L"RegTestKey00",
        L"RegTestKey01",
        L"RegTestKey02",
        L"RegTestKey03",
        L"RegTestKey04",
        L"RegTestKey05",
        L"RegTestKey06",
        L"RegTestKey07",
        L"RegTestKey08",
        L"RegTestKey09",
        L"RegTestKey10",
        L"RegTestKey11",
        L"RegTestKey12",
        L"RegTestKey13",
        L"RegTestKey14",
        L"RegTestKey15",
        L"RegTestKey16",
        L"RegTestKey17",
        L"RegTestKey18",
        L"RegTestKey19"
};

static TCHAR g_szarrRegKeys_FullPath[MAX_REGKEYS][MAX_PATH] = {
        L"\\TESTMultiThreadRegTEST\\RegTestKey00",
        L"\\TESTMultiThreadRegTEST\\RegTestKey01",
        L"\\TESTMultiThreadRegTEST\\RegTestKey02",
        L"\\TESTMultiThreadRegTEST\\RegTestKey03",
        L"\\TESTMultiThreadRegTEST\\RegTestKey04",
        L"\\TESTMultiThreadRegTEST\\RegTestKey05",
        L"\\TESTMultiThreadRegTEST\\RegTestKey06",
        L"\\TESTMultiThreadRegTEST\\RegTestKey07",
        L"\\TESTMultiThreadRegTEST\\RegTestKey08",
        L"\\TESTMultiThreadRegTEST\\RegTestKey09",
        L"\\TESTMultiThreadRegTEST\\RegTestKey10",
        L"\\TESTMultiThreadRegTEST\\RegTestKey11",
        L"\\TESTMultiThreadRegTEST\\RegTestKey12",
        L"\\TESTMultiThreadRegTEST\\RegTestKey13",
        L"\\TESTMultiThreadRegTEST\\RegTestKey14",
        L"\\TESTMultiThreadRegTEST\\RegTestKey15",
        L"\\TESTMultiThreadRegTEST\\RegTestKey16",
        L"\\TESTMultiThreadRegTEST\\RegTestKey17",
        L"\\TESTMultiThreadRegTEST\\RegTestKey18",
        L"\\TESTMultiThreadRegTEST\\RegTestKey19"
};



///////////////////////////////////////////////////////////////////////////////
//
//  FUNCTION PROTOTYPES
//
///////////////////////////////////////////////////////////////////////////////
LPTSTR Hlp_GenStringData(__out_ecount(cChars) LPTSTR pszString, DWORD cChars, DWORD dwFlags);
BOOL Hlp_GenRandomValData(DWORD *pdwType, __out_ecount(*pcbData) PBYTE pbData, DWORD *pcbData);
TCHAR* Hlp_HKeyToTLA(HKEY hKey, __out_ecount(cBuffer) TCHAR *pszBuffer, DWORD cBuffer);
BOOL Hlp_FillBuffer(__out_ecount(cbBuffer) PBYTE pbBuffer, DWORD cbBuffer, DWORD dwFlags);

///////////////////////////////////////////////////////////////////////////////
//
//  GLOBAL VARIABLES
//
///////////////////////////////////////////////////////////////////////////////


#endif
