//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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

BOOL LogInitialize(TCHAR* szModuleName, LogLevel level);
#ifdef SUPPORT_KATO
BOOL LogInitialize(TCHAR* szModuleName, LogLevel level, CKato* pKato);
#endif

void Log(LogLevel level, TCHAR* szFmt, ...);

#endif // _LOGLIB_H_
