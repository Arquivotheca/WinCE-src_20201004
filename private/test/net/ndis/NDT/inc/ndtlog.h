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
#ifndef __NDT_LOG_H
#define __NDT_LOG_H

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Log verbosity flags
//------------------------------------------------------------------------------

#define NDT_LOG_EXCEPTION           0
#define NDT_LOG_FAIL                2
#define NDT_LOG_WARNING             3
#define NDT_LOG_ABORT               4
#define NDT_LOG_SKIP                6
#define NDT_LOG_NOT_IMPLEMENTED     8
#define NDT_LOG_PASS                10
#define NDT_LOG_DETAIL              12
#define NDT_LOG_COMMENT             14
#define NDT_LOG_MAX_VERBOSITY       15

#define NDT_LOG_DEFAULT             13

#define NDT_LOG_MASK                0x000F
#define NDT_LOG_PACKETS             0x8000

//------------------------------------------------------------------------------

typedef void (*PLOG_FUNCTION)(DWORD dwLevel, LPCTSTR sz);

void NDTLogStartup(LPCTSTR szTestName, DWORD dwLogLevel, PLOG_FUNCTION pLogFce);
void NDTLogCleanup();
void NDTLogSetWATTOutput(BOOL bWATTOutput);
void NDTLogSetLevel(DWORD dwLogLevel);
DWORD NDTLogGetLevel();

void NDTLogErr(LPCTSTR szFormat, ...);
void NDTLogWrn(LPCTSTR szFormat, ...);
void NDTLogMsg(LPCTSTR szFormat, ...);
void NDTLogDbg(LPCTSTR szFormat, ...);
void NDTLogVbs(LPCTSTR szFormat, ...);

DWORD NDTLogGetErrorCount(void);
DWORD NDTLogGetWarningCount(void);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
