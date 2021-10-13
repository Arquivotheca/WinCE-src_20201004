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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <tchar.h>

#ifndef DVRLOGGER_H
#define DVRLOGGER_H

void Debug(LPCTSTR szFormat, ...);

void LogInfo(int iType, UINT32 LineNumber, LPCTSTR szFunctionName, LPCTSTR szFormat, va_list argList);
void LogError(UINT32 uiLineNumber, const TCHAR * pszFunctionName, const TCHAR *fmt, ...);
void LogWarning(UINT32 uiLineNumber, const TCHAR * pszFunctionName, const TCHAR *fmt, ...);
void LogText(UINT32 uiLineNumber, const TCHAR * pszFunctionName, const TCHAR *fmt, ...);
void LogText(const TCHAR *fmt, ...);
void LogDetail(UINT32 uiLineNumber, const TCHAR * pszFunctionName, const TCHAR *fmt, ...);

#endif

