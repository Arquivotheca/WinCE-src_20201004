//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <tchar.h>
#include "Log.h"
#include "ndp_lib.h"

#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#endif

//------------------------------------------------------------------------------

#define BUFFER_SIZE           2048

//------------------------------------------------------------------------------

typedef struct {
   BOOL   m_fInitialized;
   DWORD  m_logLevel;
   TCHAR  m_szBuffer[BUFFER_SIZE];
   LPTSTR m_szBufferTail;
   DWORD  m_bufferTail;
   LONG   m_errorCount;
   LONG   m_warningCount;
   PLOG_FUNCTION m_pLogFce;
   CRITICAL_SECTION m_cs;
} LOG_INFO;

//------------------------------------------------------------------------------

static LOG_INFO g_logInfo = {
   FALSE, LOG_PASS, _T(""), NULL, 0, 0, 0, NULL
};

//------------------------------------------------------------------------------

static LPCTSTR g_LogPrefix[] = {
   _T("ERROR: "), _T("ERROR: "), _T("ERROR: "), _T("Wrn: "), 
   _T("Wrn: "),   _T("Wrn: "),   _T("Wrn: "),   _T("Wrn: "), 
   _T("Wrn: "),   _T("Wrn: "),   _T("Msg: "),   _T("Msg: "), 
   _T("Dbg: "),   _T("Dbg: "),   _T("Vbs: "),   _T("Vbs: ")
};

//------------------------------------------------------------------------------

void LogStartup(LPCTSTR szTestName, DWORD logLevel, PLOG_FUNCTION pLogFce)
{
   DWORD length = 0;

   ASSERT(!g_logInfo.m_fInitialized);

   InitializeCriticalSection(&g_logInfo.m_cs);
   EnterCriticalSection(&g_logInfo.m_cs);
   
   g_logInfo.m_fInitialized = TRUE;
   g_logInfo.m_logLevel = logLevel;
   g_logInfo.m_pLogFce = pLogFce;
   
   lstrcpy(g_logInfo.m_szBuffer, _T("00000000: "));
   StringCchCopy(g_logInfo.m_szBuffer, BUFFER_SIZE, szTestName);
   StringCchCat(g_logInfo.m_szBuffer, BUFFER_SIZE, _T(" "));

   length = _tcslen(g_logInfo.m_szBuffer);
   g_logInfo.m_szBufferTail = g_logInfo.m_szBuffer + length;
   g_logInfo.m_bufferTail = BUFFER_SIZE-length;

   LeaveCriticalSection(&g_logInfo.m_cs);
}

//------------------------------------------------------------------------------

void LogCleanup()
{
   ASSERT(g_logInfo.m_fInitialized);
   EnterCriticalSection(&g_logInfo.m_cs);
   g_logInfo.m_fInitialized = FALSE;
   LeaveCriticalSection(&g_logInfo.m_cs);        
   DeleteCriticalSection(&g_logInfo.m_cs);
}

//------------------------------------------------------------------------------

void LogV(DWORD logLevel, LPCTSTR szFormat, va_list pArgs)
{
   LONG count = 0;

   if (!g_logInfo.m_fInitialized)
	   LogStartup(_T("Test"), LOG_PASS, NULL);

   ASSERT(g_logInfo.m_fInitialized);
   EnterCriticalSection(&g_logInfo.m_cs);

   ASSERT(g_logInfo.m_fInitialized);
   if (logLevel > g_logInfo.m_logLevel) goto cleanUp;
   if (logLevel <= LOG_FAIL) g_logInfo.m_errorCount++;
   else if (logLevel <= LOG_WARNING) g_logInfo.m_warningCount++;

   count = _tcslen(g_LogPrefix[logLevel]);
   StringCchCopy(
      g_logInfo.m_szBufferTail, g_logInfo.m_bufferTail, g_LogPrefix[logLevel]
   );
   StringCchVPrintf(
      g_logInfo.m_szBufferTail + count, g_logInfo.m_bufferTail, szFormat, pArgs
   );

   if (g_logInfo.m_pLogFce == NULL) {
      OutputDebugString(g_logInfo.m_szBuffer);
   } else {
#ifndef UNDER_CE
      _tprintf(g_logInfo.m_szBuffer);
	  _tprintf(_T("\n"));
#else
      g_logInfo.m_pLogFce(logLevel, g_logInfo.m_szBuffer);
#endif
   }

cleanUp:
   LeaveCriticalSection(&g_logInfo.m_cs);
}

//------------------------------------------------------------------------------

void Log(DWORD logLevel, LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   LogV(logLevel, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

void LogErr(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   LogV(LOG_FAIL, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

void LogWrn(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   LogV(LOG_WARNING, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

void LogMsg(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   LogV(LOG_PASS, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

void LogDbg(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   LogV(LOG_DETAIL, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

void LogVbs(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   LogV(LOG_COMMENT, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------

DWORD LogGetErrorCount(void)
{
   return g_logInfo.m_errorCount;
}

//------------------------------------------------------------------------------

DWORD LogGetWarningCount(void)
{
   return g_logInfo.m_warningCount;
}

//------------------------------------------------------------------------------

void LogSetLevel(DWORD dwLogLevel)
{
   g_logInfo.m_logLevel = dwLogLevel;
}

//------------------------------------------------------------------------------

DWORD LogGetLevel()
{
   return g_logInfo.m_logLevel;
}

//------------------------------------------------------------------------------
