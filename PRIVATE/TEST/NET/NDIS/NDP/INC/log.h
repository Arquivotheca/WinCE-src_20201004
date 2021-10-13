//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __NDT_LOG_H
#define __NDT_LOG_H

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------


#define LOG_EXCEPTION            0
#define LOG_FAIL                 2
#define LOG_WARNING              3
#define LOG_ABORT                4
#define LOG_SKIP                 6
#define LOG_NOT_IMPLEMENTED      8
#define LOG_PASS                 10
#define LOG_DETAIL               12
#define LOG_COMMENT              14
#define LOG_MAX_VERBOSITY        15

//------------------------------------------------------------------------------

typedef void (*PLOG_FUNCTION)(DWORD logLevel, LPCTSTR sz);

void LogStartup(LPCTSTR szTestName, DWORD logLevel, PLOG_FUNCTION pLogFce);
void LogCleanup();

void LogSetLevel(DWORD logLevel);
DWORD LogGetLevel();

void LogErr(LPCTSTR szFormat, ...);
void LogWrn(LPCTSTR szFormat, ...);
void LogMsg(LPCTSTR szFormat, ...);
void LogDbg(LPCTSTR szFormat, ...);
void LogVbs(LPCTSTR szFormat, ...);

void Log(DWORD logLevel, LPCTSTR szFormat, ...);

DWORD LogGetErrorCount(void);
DWORD LogGetWarningCount(void);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
