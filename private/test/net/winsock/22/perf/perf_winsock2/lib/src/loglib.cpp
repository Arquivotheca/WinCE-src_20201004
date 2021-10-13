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
// loglib.cpp

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include "loglib.h"
#include <strsafe.h>
#ifdef SUPPORT_KATO
#include "katoex.h"
#endif

#define FILE_MODE_APPEND_ONLY L"a"

const int MAX_CACHE_ENTRIES = 1024;
const int MAX_CACHE_ENTRY_SIZE = 256;
TCHAR lpszDebugCache[MAX_CACHE_ENTRIES * MAX_CACHE_ENTRY_SIZE];
DWORD dwCurrentCacheCounter = 0;

extern HWND g_hwndEdit;
extern HWND g_hMainWnd;

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
    _tcsncpy_s(g_szModuleName, MAX_PATH, szModuleName, MAX_PATH-1);
    g_szModuleName[MAX_PATH-1] = _T('\0');
    return TRUE;
}

BOOL FileInitialize(TCHAR *szFileName)
{
    return (0 != _tfopen_s(&g_FP, szFileName, FILE_MODE_APPEND_ONLY));
}

#ifdef SUPPORT_KATO
BOOL LogInitialize(TCHAR* szModuleName, LogLevel level, CKato* pKato) {
    LogInitialize(szModuleName, level);
    g_pKato = pKato;
    return TRUE;
}
#endif

//
// Divider between tests in the output file
//
void FileAddNewTestBreak(TCHAR *szText)
{
    TCHAR szOut[256];
    StringCchPrintf(szOut, 256, TEXT("\r\n*************\r\n%s: %s\r\n*************\r\n"), g_szModuleName, szText);
    if (g_FP)
    {
       _ftprintf(g_FP, szOut);
    }
    SendMessage(g_hwndEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)szOut);  
}

void Log(LogLevel level, TCHAR* szFmt, ...)
{
    if (level < g_CurrentOutputLevel) return;
    TCHAR NewLine[]=TEXT("\r\n");
    const int iMaxBufLen = 256;
    TCHAR szOut[iMaxBufLen];
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
        _vsntprintf_s(szOut, iMaxBufLen, szFmt, va);
        szOut[iMaxBufLen-1] = _T('\0');

        SendMessage(g_hwndEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)szOut); 
        SendMessage(g_hwndEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)NewLine); 

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
        _vsntprintf_s(szOut, iMaxBufLen, szFmt, va);
        szOut[iMaxBufLen-1] = _T('\0');
#ifdef UNDER_CE        
        RETAILMSG(1, (TEXT("%s: %s"), g_szModuleName, szOut));
#endif
        SendMessage(g_hwndEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)szOut); 
        SendMessage(g_hwndEdit, EM_REPLACESEL, (WPARAM)FALSE, (LPARAM)NewLine); 
#ifndef UNDER_CE
        _vtprintf(szFmt, va);
#endif
        va_end(va);
#ifndef UNDER_CE
        _tprintf(TEXT("\n"));
#endif
    }
}

