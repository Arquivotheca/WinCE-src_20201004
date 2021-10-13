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
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __GLOBALS_CPP__
#define GLOBAL
#define INIT(x) = x
#else // __GLOBALS_CPP__
#define GLOBAL  extern
#define INIT(x)
#endif // __GLOBALS_CPP__

// --------------------------------------------------------------------
// Module Name
// --------------------------------------------------------------------

#define MODULE_NAME _T("fsperflog")

////////////////////////////////////////////////////////////////////////////////
// Global macros
#define DEFAULT_TEST_FILE_NAME _T("perf.file")
#define DEFAULT_TEST_PATH _T("\\Storage Card")
#define DEFAULT_STORAGE_PROFILE _T("PCMCIA")
#define DEFAULT_STORAGE_TYPE _T("Unknown")
#define DEFAULT_DIRECTORY_NAME _T("\\a")
#define DEFAULT_STORAGE_DESCRIPTION _T("Unknown")
#define DEFAULT_TEST_DIRECTORY _T("testdir")
#define DEFAULT_SRC_DIRECTORY _T("testsrc")
#define DEFAULT_DST_DIRECTORY _T("testdst")
#define DEFAULT_TEST_FIND_FILE _T("jpg")
#define DEFAULT_NUM_TEST_ITERATIONS 4
#define DEFAULT_DATA_SIZE 512
#define DEFAULT_FILE_SIZE 2097152
#define DEFAULT_SEARCH_FILE_SIZE 10000
#define DEFAULT_FILE_SET_COUNT 100
#define DEFAULT_FILE_SET_SIZE 1000000
#define DEFAULT_WRITE_SIZE 32768
#define DEFAULT_SYS_MON_INTERVAL 10
#define NUM_TEST_SEEKS 1000
#define MAX_IO_DATA_SIZE 1048576
#define MAX_FILE_SIZE 1024*1024*128
#define DEFAULT_BURST_WRITE_PAUSE_MS 100

#define FLAG_WRITETHROUGH          _T("wt")
#define FLAG_NO_BUFFERING          _T("nb")
#define FLAG_TIME_FILE_FLUSH       _T("flush")
#define FLAG_STORAGE_DESCRIPTOR    _T("desc")
#define FLAG_STORAGE_PROFILE       _T("p")
#define FLAG_PATH                  _T("r")
#define FLAG_LOG_CPU               _T("cpu")
#define FLAG_USE_ROOT_FILE_SYSTEM  _T("root")
#define FLAG_IO_SIZE               _T("iosize")
#define FLAG_FILE_SIZE             _T("filesize")
#define FLAG_BURST_WRITE_PAUSE     _T("burstw")
#define FLAG_USE_TUX_RANDOM_SEED   _T("rand")


#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)
#define VALID_POINTER(X)    (X != NULL && 0xcccccccc != (int) X)
#define CHECK_FREE(X) if (VALID_POINTER(X)) free(X)
#define CHECK_DELETE(X) if (VALID_POINTER(X)) delete X
#define CHECK_LOCAL_FREE(X) if (VALID_POINTER(X)) LocalFree(X)
#define CHECK_CLOSE_HANDLE(X) if (VALID_HANDLE(X)) CloseHandle(X)
#define CHECK_FIND_CLOSE(X) if (VALID_HANDLE(X)) FindClose(X)

#ifndef CE_VOLUME_FLAG_64BIT_FILES_SUPPORTED
#define CE_VOLUME_FLAG_64BIT_FILES_SUPPORTED 0
#endif

// Macro to start perf logging
#define START_PERF() \
    pCPerflog->StartLog(); \
    if (g_fLogCPUData) { \
    pCPerflog->StartSystemMonitor(g_dwSysMonitorInterval); }

// Macro to end perf logging
#define END_PERF() \
    pCPerflog->EndLog();

// Macro to close the handle and end the perf logging
#define CLOSE_FILE_END_PERF(x) \
    if (g_fTimeFlush) { \
    FlushFileBuffers(x); \
    CloseHandle(x); \
    pCPerflog->EndLog(); \
    } else { \
    pCPerflog->EndLog(); \
    CloseHandle(x); }

// Macro to stop perf and clean up the logger object
#define CHECK_END_PERF(x) \
    if (VALID_POINTER(x)) { \
    x->StopSystemMonitor(); \
    x->EndLog(); \
    CHECK_DELETE(x);\
    x = NULL;}

// Macro to clean up the test file
#define CHECK_DELETE_FILE(x) \
    if (VALID_POINTER(g_pCFSUtils)) { \
    g_pCFSUtils->DeleteFile(x); }

#define CHECK_CLEAN_DIRECTORY(x,y) \
    if (VALID_POINTER(x)) { \
    x->CleanDirectory(y, TRUE); }

////////////////////////////////////////////////////////////////////////////////
// Global function prototypes
void            Debug(LPCTSTR, ...);
class CPerfHelper;
LPTSTR CreateMarkerName(__out_ecount(cbLength) LPTSTR pszPerfMarkerName,
                        DWORD cbLength,
                        LPCTSTR pszTestName);
LPTSTR CreateMarkerParams(__out_ecount(cbLength) LPTSTR pszPerfMarkerParams,
                          DWORD cbLength,
                          DWORD dwDataSize,
                          DWORD dwFullSize,
                          LPCTSTR pszUnits);
LPTSTR CreateMarkerParamsForBurstWrite(__out_ecount(cbLength) LPTSTR pszPerfMarkerParams,
                          DWORD cbLength,
                          DWORD dwDataSize,
                          DWORD dwFullSize,
                          LPCTSTR pszUnits);
CPerfHelper * MakeNewPerflogger(LPCTSTR pszPerfMarkerName,
                                LPCTSTR pszPerfMarkerParams,
                                PerfDataType dataType,
                                DWORD dwDataSize = 0,
                                DWORD dwFullSize = 0,
                                float flScalingFactor = 1);
SHELLPROCAPI    ShellProc(UINT, SPPARAM);
BOOL            InitFileSys();
BOOL            InitPerfLog();

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

#include "ft.h"
#include "fsutils.h"

////////////////////////////////////////////////////////////////////////////////
// Globals

// Global CKato logging object. Set while processing SPM_LOAD_DLL message.
GLOBAL CKato            *g_pKato INIT(NULL);

// Global shell info structure. Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO   *g_pShellInfo;

GLOBAL OSVERSIONINFO    g_osVerInfo INIT({0});
GLOBAL TCHAR            g_szTestPath[MAX_PATH] INIT(DEFAULT_TEST_PATH); 
GLOBAL TCHAR            g_szTestFileName[MAX_PATH] INIT(DEFAULT_TEST_FILE_NAME);
GLOBAL TCHAR            g_szProfile[PROFILENAMESIZE] INIT(DEFAULT_STORAGE_PROFILE);
GLOBAL TCHAR            g_szStorageDescriptor[MAX_PATH] INIT(DEFAULT_STORAGE_DESCRIPTION);
GLOBAL BOOL             g_fProfileSpecified INIT(FALSE);
GLOBAL BOOL             g_fPathSpecified INIT(FALSE);
GLOBAL BOOL             g_fLogCPUData INIT(FALSE);
GLOBAL BOOL             g_fTimeFlush INIT(FALSE);
GLOBAL BOOL             g_fUseRootFileSystem INIT(FALSE);
GLOBAL BOOL             g_fFileSysInitPerformed INIT(FALSE);
GLOBAL BOOL             g_fDoBurstWrite INIT(FALSE);
GLOBAL BOOL             g_fUseTuxRandomSeed INIT(FALSE);
GLOBAL DWORD            g_dwFileCreateFlags INIT(FILE_ATTRIBUTE_NORMAL);
GLOBAL DWORD            g_dwNumTestIterations INIT(DEFAULT_NUM_TEST_ITERATIONS);
GLOBAL DWORD            g_dwDataSize INIT(DEFAULT_DATA_SIZE);
GLOBAL DWORD            g_dwFileSize INIT(DEFAULT_FILE_SIZE);
GLOBAL DWORD            g_dwSysMonitorInterval INIT(DEFAULT_SYS_MON_INTERVAL);
GLOBAL DWORD            g_dwBurstWritePauseMs INIT(DEFAULT_BURST_WRITE_PAUSE_MS);
GLOBAL CFSUtils *       g_pCFSUtils INIT(NULL);
#endif // __GLOBALS_H__
