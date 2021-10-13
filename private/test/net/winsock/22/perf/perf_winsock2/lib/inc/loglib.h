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
// loglib.h

#ifndef _LOGLIB_H_
#define _LOGLIB_H_

#include <tchar.h>

#ifdef SUPPORT_KATO
#include <kato.h>
#endif

typedef enum LogLevel
{
    DEBUG_MSG     = 0,
    WARNING_MSG   = 1,
    ERROR_MSG     = 2,
    REQUIRED_MSG  = 2
};

BOOL FileInitialize(TCHAR *szFileName);
void FileAddNewTestBreak(TCHAR *szText);
BOOL LogInitialize(TCHAR* szModuleName, LogLevel level);
#ifdef SUPPORT_KATO
BOOL LogInitialize(TCHAR* szModuleName, LogLevel level, CKato* pKato);
#endif

void Log(LogLevel level, TCHAR* szFmt, ...);

#endif // _LOGLIB_H_
