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
#include "tux.h"
#include "kato.h"

extern CKato * g_pKato;
#define countof(x)  (sizeof(x)/sizeof(*(x)))

void Debug(LPCTSTR szFormat, ...)
{
    static  TCHAR   szHeader[] = TEXT("DSHOW API: ");
    TCHAR   szBuffer[1024];

    va_list pArgs;
    va_start(pArgs, szFormat);
    lstrcpy(szBuffer, szHeader);
    wvsprintf(
        szBuffer + countof(szHeader) - 1,
        szFormat,
        pArgs);
    va_end(pArgs);

    _tcscat(szBuffer, TEXT("\r\n"));

    OutputDebugString(szBuffer);
}

void
LogInfo(int iType, UINT32 LineNumber, LPCTSTR szFunctionName, LPCTSTR szFormat, va_list argList)
{
    TCHAR szAdjustedFormat[1024] = {NULL};
    TCHAR   szBuffer[1024] = {NULL};

    if(LineNumber > 0 && szFunctionName != NULL)
        _stprintf(szAdjustedFormat, TEXT("%s - Line %d - "), szFunctionName, LineNumber);
    _tcsncat(szAdjustedFormat, szFormat, 1024 - _tcslen(szAdjustedFormat));

    if(FAILED(StringCbVPrintf(szBuffer, countof(szBuffer) - 1, szAdjustedFormat, argList)))
       OutputDebugString(TEXT("StringCbVPrintf failed"));

    switch (iType)
    {
        case TPR_FAIL:
            if(g_pKato)
                g_pKato->Log(TPR_FAIL, TEXT("***FAIL: %s"), szBuffer);
            else
                Debug(TEXT("***FAIL: %s"), szBuffer);
            break;
        case TPR_PASS:
            if(g_pKato)
                g_pKato->Log(TPR_PASS, szBuffer);
            else
                Debug(szBuffer);
            break;
        case TPR_ABORT:
            if(g_pKato)
                g_pKato->Log(TPR_ABORT, TEXT("***ABORT: %s"), szBuffer);
            else
                Debug(TEXT("***ABORT: %s"), szBuffer);
            break;
        case TPR_SKIP:
            if(g_pKato)
            g_pKato->Log(TPR_SKIP, TEXT("***SKIP: %s"), szBuffer);
            else
                Debug(TEXT("***SKIP: %s"), szBuffer);
            break;
    }
}

void LogError(UINT32 uiLineNumber, const TCHAR * pszFunctionName, const TCHAR *fmt, ...)
{
    va_list argList;
    va_start(argList, fmt);
    LogInfo(TPR_FAIL, uiLineNumber, pszFunctionName, fmt, argList);
    va_end(argList);
}

void LogWarning(UINT32 uiLineNumber, const TCHAR * pszFunctionName, const TCHAR *fmt, ...)
{
    va_list argList;
    va_start(argList, fmt);
    LogInfo(TPR_PASS, uiLineNumber, pszFunctionName, fmt, argList);
    va_end(argList);
}


void LogText(UINT32 uiLineNumber, const TCHAR * pszFunctionName, const TCHAR *fmt, ...)
{
    va_list argList;
    va_start(argList, fmt);
    LogInfo(TPR_PASS, uiLineNumber, pszFunctionName, fmt, argList);
    va_end(argList);
}

void LogText(const TCHAR *fmt, ...)
{
    va_list argList;
    va_start(argList, fmt);
    LogInfo(TPR_PASS, 0, NULL, fmt, argList);
    va_end(argList);
}


void LogDetail(UINT32 uiLineNumber, const TCHAR * pszFunctionName, const TCHAR *fmt, ...)
{
    va_list argList;
    va_start(argList, fmt);
    LogInfo(TPR_PASS, uiLineNumber, pszFunctionName, fmt, argList);
    va_end(argList);
}

