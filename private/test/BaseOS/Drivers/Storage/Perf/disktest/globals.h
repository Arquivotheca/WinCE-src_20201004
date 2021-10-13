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
//  DISKTEST_PERF TUX DLL
//
//  Module: globals.h
//          Declares all global variables and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES global
//          variables, including the function table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// Global macros
#define countof(x)  (sizeof(x)/sizeof(*(x)))
#define INVALID_HANDLE(X)   (INVALID_HANDLE_VALUE == X || NULL == X)
#define VALID_HANDLE(X)     (INVALID_HANDLE_VALUE != X && NULL != X)

#define MAX_DISK            10 // maximum # of disks to check (e.g. DSK1, DSK2)
#define MAX_BUFFER_SIZE     (1024 * 1024) // 1MB buffer size
#define READ_LOCATIONS      1000 // tests will hit every 1/READ_LOCATIONS position on disk
#define DEF_NUM_ITER        3
#define DEF_MAX_SECTORS     16384
#define DEF_NUM_SECTORS     128
#define lptStrUnknownManufacturer TEXT("Unknown Manufacturer")

// logging types
enum { LOG_DBG, LOG_CSV, LOG_PERFLOG, LOG_BTS};


////////////////////////////////////////////////////////////////////////////////
// Global function prototypes

void            Debug(LPCTSTR, ...);
SHELLPROCAPI    ShellProc(UINT, SPPARAM);

////////////////////////////////////////////////////////////////////////////////
// TUX Function table

#include "ft.h"

////////////////////////////////////////////////////////////////////////////////
// Globals

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
GLOBAL CKato *g_pKato;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
GLOBAL CRITICAL_SECTION g_csProcess;

// default logger
GLOBAL PerfLogStub      *g_pPerfLog;
GLOBAL DWORD            g_logMethod;

// filename for .CSV files
GLOBAL TCHAR g_csvFilename[MAX_PATH];

GLOBAL DWORD            g_dwMaxSectorsPerOp;
GLOBAL DWORD            g_dwPerfIterations;

GLOBAL DWORD            g_sectorList[1024];
GLOBAL DWORD            g_sectorListCounts;

#endif // __GLOBALS_H__

