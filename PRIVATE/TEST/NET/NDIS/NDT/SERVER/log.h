//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __LOG_H
#define __LOG_H

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Log verbosity flags
//------------------------------------------------------------------------------

#ifndef __KATOEX_H__
#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14
#endif

//------------------------------------------------------------------------------

#define LOG_WARNING            3
#define LOG_MAX_VERBOSITY     15
#define LOG_DEFAULT           10

void LogErr(LPCTSTR format, ...);
void LogWrn(LPCTSTR format, ...);
void LogMsg(LPCTSTR format, ...);
void LogDbg(LPCTSTR format, ...);
void LogVbs(LPCTSTR format, ...);
void LogSetLevel(DWORD dwLogLevel);

extern DWORD g_dwLogLevel;
extern LPTSTR g_szLogPrefix;

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
