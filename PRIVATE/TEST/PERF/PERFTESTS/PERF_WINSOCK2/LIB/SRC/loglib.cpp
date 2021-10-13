//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// loglib.cpp

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include "loglib.h"
#ifdef SUPPORT_KATO
#include "katoex.h"
#endif

// Internal variables
#ifdef SUPPORT_KATO
static CKato* g_pKato = NULL;
#else
static VOID* g_pKato = NULL;
#endif

FILE* g_FP = NULL;
LogLevel g_CurrentOutputLevel = DEBUG_MSG;
TCHAR g_szModuleName[MAX_PATH] = TEXT("LOG");

BOOL LogInitialize(TCHAR* szModuleName, LogLevel level) {
    g_CurrentOutputLevel = level;
    _tcsncpy(g_szModuleName, szModuleName, MAX_PATH-1);
    g_szModuleName[MAX_PATH-1] = _T('\0');
    return TRUE;
}

#ifdef SUPPORT_KATO
BOOL LogInitialize(TCHAR* szModuleName, LogLevel level, CKato* pKato) {
    LogInitialize(szModuleName, level);
    g_pKato = pKato;
    return TRUE;
}
#endif

void Log(LogLevel level, TCHAR* szFmt, ...)
{
    if (level < g_CurrentOutputLevel) return;

    va_list va;

    if (g_FP)
    {
        _ftprintf(g_FP, TEXT("%s: "), g_szModuleName);
        va_start(va, szFmt);
        _vftprintf(g_FP, szFmt, va);
        va_end(va);
        _ftprintf(g_FP, TEXT("\n"));
    }
#ifdef SUPPORT_KATO
    else if (g_pKato)
    {
        va_start(va, szFmt);
        g_pKato->LogV(LOG_FAIL, szFmt, va);
        va_end(va);
    }
#endif
    else
    {
#ifndef UNDER_CE
        _tprintf(TEXT("%s: "), g_szModuleName);
#endif
         va_start(va, szFmt);
#ifdef UNDER_CE
        const int iMaxBufLen = 256;
        TCHAR szOut[iMaxBufLen];
        _vsntprintf(szOut, iMaxBufLen, szFmt, va);
        // Force null-termination on the string (_vsntprintf may not take care of this)
        szOut[iMaxBufLen-1] = _T('\0');
        RETAILMSG(1, (TEXT("%s: %s"), g_szModuleName, szOut));
#else
        _vtprintf(szFmt, va);
#endif
        va_end(va);
#ifndef UNDER_CE
        _tprintf(TEXT("\n"));
#endif
    }
}

