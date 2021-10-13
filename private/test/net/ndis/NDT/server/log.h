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
