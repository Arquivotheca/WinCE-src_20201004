//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <StdAfx.h>
#include "Messages.h"
#include "NDTLog.h"

//------------------------------------------------------------------------------

#define BUFFER_SIZE           2048
#define MILLISEC_PER_HOUR     (1000*60*60)
#define MILLISEC_PER_MINUTE   (1000*60)

//------------------------------------------------------------------------------

typedef struct {
   BOOL   m_bStartup;
   BOOL   m_bWATTOutput;
   DWORD  m_dwLogLevel;
   DWORD  m_dwStartTick;
   TCHAR  m_szBuffer[BUFFER_SIZE];
   LPTSTR m_szBufferTail;
   DWORD  m_cbBufferTail;
   LONG   m_nErrorCount;
   LONG   m_nWarningCount;
   PLOG_FUNCTION m_pLogFce;
   CRITICAL_SECTION m_cs;
} NDT_LOG_INFO;

//------------------------------------------------------------------------------

static NDT_LOG_INFO g_NDTLogInfo = {
   FALSE, FALSE, NDT_LOG_DEFAULT, 0, _T(""), NULL, 0, 0, 0, NULL
};

//------------------------------------------------------------------------------

static LPCTSTR g_NDTLogPrefix[] = {
   _T("ERROR: "), _T("ERROR: "), _T("ERROR: "), _T("Wrn: "), 
   _T("Wrn: "), _T("Wrn: "), _T("Wrn: "), _T("Wrn: "), 
   _T("Wrn: "), _T("Wrn: "), _T("Msg: "), _T("Msg: "), 
   _T("Dbg: "), _T("Dbg: "), _T("Vbs: "), _T("Vbs: ")
};

//------------------------------------------------------------------------------

void NDTLogStartup(LPCTSTR szTestName, DWORD dwLogLevel, PLOG_FUNCTION pLogFce)
{
   DWORD nLength = 0;

   ASSERT(!g_NDTLogInfo.m_bStartup);

   InitializeCriticalSection(&g_NDTLogInfo.m_cs);
   EnterCriticalSection(&g_NDTLogInfo.m_cs);
   
   g_NDTLogInfo.m_bStartup = TRUE;
   g_NDTLogInfo.m_bWATTOutput = FALSE;
   g_NDTLogInfo.m_dwLogLevel = dwLogLevel;
   g_NDTLogInfo.m_dwStartTick = GetTickCount();
   g_NDTLogInfo.m_pLogFce = pLogFce;
   
   lstrcpy(g_NDTLogInfo.m_szBuffer, _T("00000000: "));
   lstrcpy(g_NDTLogInfo.m_szBuffer + 10, szTestName);
   lstrcat(g_NDTLogInfo.m_szBuffer, _T(" "));

   nLength = lstrlen(g_NDTLogInfo.m_szBuffer);
   g_NDTLogInfo.m_szBufferTail = g_NDTLogInfo.m_szBuffer + nLength;
   g_NDTLogInfo.m_cbBufferTail = BUFFER_SIZE - nLength - 3;

   LeaveCriticalSection(&g_NDTLogInfo.m_cs);
}

//------------------------------------------------------------------------------

void NDTLogCleanup()
{
   DWORD dwTime, dw;
   UINT uiHours, uiMinutes, uiSeconds;
   TCHAR szBuffer[41] = _T("");

   ASSERT(g_NDTLogInfo.m_bStartup);
   
   EnterCriticalSection(&g_NDTLogInfo.m_cs);

   if (g_NDTLogInfo.m_bWATTOutput) {
      dw = dwTime = GetTickCount() - g_NDTLogInfo.m_dwStartTick;
      uiHours = dw / MILLISEC_PER_HOUR;
      dw -= uiHours * MILLISEC_PER_HOUR;
      uiMinutes = dw / MILLISEC_PER_MINUTE;
      dw -= uiMinutes * MILLISEC_PER_MINUTE;
      uiSeconds = dw / 1000;

      NDTLog(NDT_INF_TEST_TIME, uiHours, uiMinutes, uiSeconds, dwTime);
      NDTLog(
         NDT_INF_TEST_RESULT, 
         NDTLogGetErrorCount() ? _T("FAILED") : _T("PASSED"), 
         NDTLogGetErrorCount(), NDTLogGetWarningCount()
      );

      _stprintf(szBuffer, _T("@@@@@@%d\r\n"), NDTLogGetErrorCount());
      OutputDebugString(szBuffer);
   }

   g_NDTLogInfo.m_bStartup = FALSE;
   LeaveCriticalSection(&g_NDTLogInfo.m_cs);        
   DeleteCriticalSection(&g_NDTLogInfo.m_cs);
}

//------------------------------------------------------------------------------

void NDTLogV(DWORD dwGroup, LPCTSTR szFormat, va_list pArgs)
{
   LONG nCount = 0;

   ASSERT(g_NDTLogInfo.m_bStartup);

   EnterCriticalSection(&g_NDTLogInfo.m_cs);

   if (dwGroup > g_NDTLogInfo.m_dwLogLevel) goto cleanUp;

   if (dwGroup <= NDT_LOG_FAIL) g_NDTLogInfo.m_nErrorCount++;
   else if (dwGroup <= NDT_LOG_WARNING) g_NDTLogInfo.m_nWarningCount++;

   wsprintf(g_NDTLogInfo.m_szBuffer, _T("%08x"), GetCurrentThreadId());
   g_NDTLogInfo.m_szBuffer[8] = _T(':');

   nCount = lstrlen(g_NDTLogPrefix[dwGroup]);
   lstrcpy(g_NDTLogInfo.m_szBufferTail, g_NDTLogPrefix[dwGroup]);

   wvsprintf(g_NDTLogInfo.m_szBufferTail + nCount, szFormat, pArgs);
   if (g_NDTLogInfo.m_pLogFce == NULL) {
      OutputDebugString(g_NDTLogInfo.m_szBuffer);
   } else {
      g_NDTLogInfo.m_pLogFce(dwGroup, g_NDTLogInfo.m_szBuffer);
   }

cleanUp:
   LeaveCriticalSection(&g_NDTLogInfo.m_cs);
}

//------------------------------------------------------------------------------

void NDTLog(DWORD dwId, ...)
{
   va_list pArgs;
   DWORD dwGroup = (dwId & 0x0F000000) >> 24;
   LPCTSTR* aszFormatTable = g_NDTLogMessageTables[(dwId & 0x00FF0000) >> 16];

   va_start(pArgs, dwId);
   NDTLogV(dwGroup, aszFormatTable[dwId & 0x0000FFFF], pArgs);
   va_end(pArgs); 
}

//------------------------------------------------------------------------------
//
//  @func void | NDTLogErr |  Output an error message.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the error
//  @parmvar           The parameters that are needed by the format string
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized
//  using the function <f NetLogInitWrapper> and not after the function 
//  <f NetLogCleanupWrapper> has been called.
//
//  Output an error message and increment the error count. Concatenate 
//  the user message to the string "ERROR: ".
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
void NDTLogErr(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   NDTLogV(NDT_LOG_FAIL, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------
//
//  @func void | NDTLogWarning |  Output an warning message.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the warning
//  @parmvar           The parameters that are needed by the format string
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized
//  using the function <f NetLogInitWrapper> and not after the function
//  <f NetLogCleanupWrapper> has been called.
//
//  Output a warning message and increment the warning count. At the NetLog
//  level of NDT_LOG_WARNING. Concatenate the user message to 
//  the string "Wrn: ".
//
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
void NDTLogWrn(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   NDTLogV(NDT_LOG_WARNING, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------
//
//  @func void | NDTLogMsg |  Output a message to the user.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the message
//  @parmvar           The parameters that are needed by the format string
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized
//  using the function <f NetLogInitWrapper> and not after the function
//  <f NetLogCleanupWrapper> has been called.
//
//  Output a message. At the NetLog level of NDT_LOG_PASS. Concatenate the user 
//  message to the string "Msg: ".
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
void NDTLogMsg(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   NDTLogV(NDT_LOG_PASS, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------
//
//  @func void | NDTLogDebug |  Output a debug message.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the message
//  @parmvar           The parameters that are needed by the format string
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized
//  using the function <f NetLogInitWrapper> and not after the function
//  <f NetLogCleanupWrapper> has been called.
//
//  Output a debug message.  Set at the NetLog level of NDT_LOG_DETAIL, which
//  is higher than default and is not normally outputed. Concatenate the user 
//  message to the string "Dbg: ".
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
void NDTLogDbg(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   NDTLogV(NDT_LOG_DETAIL, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------
//
//  @func void | NDTLogVerbose |  Output a error message.
//
//  @parm  IN LPCSTR   | szFormat | The format string that describes the message
//  @parmvar           The parameters that are needed by the format string
//
//  @comm:(LIBRARY)
//
//  This function requires that the logging engine has already been initalized
//  using the function <f NetLogInitWrapper> and not after the function
//  <f NetLogCleanupWrapper> has been called.
//
//  Output a verbose message.  Set at the NetLog level of NDT_LOG_COMMENT, 
//  which is higher than default and is not normally output. Concatenate 
//  the user message to the string "Vbs: ".
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
void NDTLogVbs(LPCTSTR szFormat, ...)
{
   va_list pArgs;
   
   va_start(pArgs, szFormat);
   NDTLogV(NDT_LOG_COMMENT, szFormat, pArgs);
   va_end(pArgs);
}

//------------------------------------------------------------------------------
//
//  @func DWORD | NDTLogGetErrorCount |  Get the current number of errors
//
//  @rdesc Returns the total number of errors currently counted
//
//  @comm:(LIBRARY)
//
//  This function can be called at any time and is not dependent on the logging
//  tool being initialized.  It will return the value associated with the most
//  recent run of the logging wrapper.
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
DWORD NDTLogGetErrorCount(void)
{
   return g_NDTLogInfo.m_nErrorCount;
}

//------------------------------------------------------------------------------
//
//  @func DWORD | NDTLogGetWarningCount |  Get the current number of warnings
//
//  @rdesc Returns the total number of warnings currently counted
//
//  @comm:(LIBRARY)
//
//  This function can be called at any time and is not dependent on the logging
//  tool being initialized.  It will return the value associated with the most
//  recent run of the logging wrapper.
//
//  @comm:(INTERNAL)
//  KNOWN ISSUES<nl>
//
DWORD NDTLogGetWarningCount(void)
{
   return g_NDTLogInfo.m_nWarningCount;
}

//------------------------------------------------------------------------------

void NDTLogSetWATTOutput(BOOL bWATTOutput)
{
   g_NDTLogInfo.m_bWATTOutput = bWATTOutput;
}
//------------------------------------------------------------------------------

void NDTLogSetLevel(DWORD dwLogLevel)
{
   g_NDTLogInfo.m_dwLogLevel = dwLogLevel;
}

//------------------------------------------------------------------------------

DWORD NDTLogGetLevel()
{
   return g_NDTLogInfo.m_dwLogLevel;
}

//------------------------------------------------------------------------------
