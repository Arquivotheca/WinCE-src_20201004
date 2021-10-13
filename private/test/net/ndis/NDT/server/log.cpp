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

   StringCchCopy(szBuffer, _countof(szBuffer), g_szLogPrefix);
   nCount = _tcslen(g_szLogPrefix);
   
   StringCchPrintf(szBuffer + nCount, _countof(szBuffer)-nCount, _T(" %08x: "), GetCurrentThreadId());
   nCount += 11; 

   StringCchCopy(szBuffer + nCount, _countof(szBuffer)-nCount, szHead);
   nCount += _tcslen(szHead);

   nCount += _vsntprintf_s(
      szBuffer + nCount, sizeof(szBuffer) - (nCount + 3)*sizeof(TCHAR), _TRUNCATE, szFormat, pArgs
   );
   if (szBuffer[nCount - 1] != _T('\n')) {
      szBuffer[nCount++] = _T('\r');
      szBuffer[nCount++] = _T('\n');
      szBuffer[nCount++] = _T('\0');
   }

   OutputDebugString(szBuffer);
}

//------------------------------------------------------------------------------
