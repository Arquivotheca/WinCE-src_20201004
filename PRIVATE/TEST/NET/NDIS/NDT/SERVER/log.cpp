//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "Log.h"

//------------------------------------------------------------------------------

DWORD  g_dwLogLevel = LOG_DEFAULT;
LPTSTR g_szLogPrefix = _T("");

//------------------------------------------------------------------------------

void OutputMessage(LPTSTR szHead, LPCTSTR szFormat, va_list pArgs);

//------------------------------------------------------------------------------

extern "C" void LogErr(LPCTSTR format, ...)
{
   va_list pArgs;
   if (g_dwLogLevel < LOG_FAIL) return;
   
   va_start(pArgs, format);
   OutputMessage(_T("ERROR: "), format, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

extern "C" void LogWrn(LPCTSTR format, ...)
{
   va_list pArgs;
   if (g_dwLogLevel < LOG_WARNING) return;

   va_start(pArgs, format);
   OutputMessage(_T("Wrn: "), format, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

extern "C" void LogMsg(LPCTSTR format, ...)
{
   va_list pArgs;
   if (g_dwLogLevel < LOG_PASS) return;

   va_start(pArgs, format);
   OutputMessage(_T("Msg: "), format, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

extern "C" void LogDbg(LPCTSTR format, ...)
{
   va_list pArgs;
   if (g_dwLogLevel < LOG_DETAIL) return;

   va_start(pArgs, format);
   OutputMessage(_T("Dbg: "), format, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

extern "C" void LogVbs(LPCTSTR format, ...)
{
   va_list pArgs;
   if (g_dwLogLevel < LOG_COMMENT) return;
   
   va_start(pArgs, format);
   OutputMessage(_T("Vbs: "), format, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

void OutputMessage(LPTSTR szHead, LPCTSTR szFormat, va_list pArgs)
{
   INT nCount;
   TCHAR szBuffer[1024] = _T("");

   _tcscpy(szBuffer, g_szLogPrefix);
   nCount = _tcslen(g_szLogPrefix);
   
   _stprintf(szBuffer + nCount, _T(" %08x: "), GetCurrentThreadId());
   nCount += 11; 

   _tcscpy(szBuffer + nCount, szHead);
   nCount += _tcslen(szHead);

   nCount += _vsntprintf(
      szBuffer + nCount, sizeof(szBuffer) - nCount - 3, szFormat, pArgs
   );
   if (szBuffer[nCount - 1] != _T('\n')) {
      szBuffer[nCount++] = _T('\r');
      szBuffer[nCount++] = _T('\n');
      szBuffer[nCount++] = _T('\0');
   }

   OutputDebugString(szBuffer);
}

//------------------------------------------------------------------------------
