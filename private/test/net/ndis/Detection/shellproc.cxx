//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "ShellProc.h"
#include <cmnprint.h>
#include <logwrap.h>

typedef void (*PLOG_FUNCTION)(DWORD dwLevel, LPCTSTR sz);
//------------------------------------------------------------------------------

INT ProcessBeginGroup(LPFUNCTION_TABLE_ENTRY pFTE);
INT ProcessEndGroup(LPFUNCTION_TABLE_ENTRY pFTE);
INT ProcessBeginScript(PLOG_FUNCTION pLogFce);
INT ProcessEndScript();
INT ProcessBeginTest(LPSPS_BEGIN_TEST pBT);
INT ProcessEndTest(LPSPS_END_TEST pET);
INT ProcessException(LPSPS_EXCEPTION pException);
void LogKato(DWORD dwVerbosity, LPCTSTR sz);
void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]);

extern LPCTSTR g_szTestGroupName;
BOOL g_fNetLogInitted = FALSE;
BOOL ParseCommands(INT argc, LPTSTR argv[]);

//------------------------------------------------------------------------------

// Global shell info structure
SPS_SHELL_INFO* g_pShellInfo = NULL;

// Global logging object
CKato* g_pKato = NULL;
BOOL g_fNetlogInitted = FALSE;
BOOL g_bVerboseEnabled = FALSE;
// DLL name:
TCHAR *g_pDllName = TEXT("NdisDetetionTests");

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

// Global flag for UNICODE
BOOL g_bUnicode = TRUE;

// ----------------------------------------------------------------------------
//
// Forwards debug and error messages from CmnPrint to Kato.
//
static void
ForwardDebug(const TCHAR *pMessage)
{
    if (g_fNetlogInitted)
    {
        NetLogDebug(TEXT("%s"), pMessage);
    }
    else
    if (g_pKato)
    {
        g_pKato->Log(LOG_COMMENT, TEXT("%s"), pMessage);
    }
    else
    {
        DEBUGMSG(TRUE, (TEXT("%s"), pMessage));
    }
}

static void
ForwardWarning(const TCHAR *pMessage)
{
    if (g_fNetlogInitted)
    {
        NetLogWarning(TEXT("%s"), pMessage);
    }
    else
    if (g_pKato)
    {
        g_pKato->Log(LOG_DETAIL, TEXT("%s"), pMessage);
    }
    else
    {
        RETAILMSG(TRUE, (TEXT("%s"), pMessage));
    }
}

static void
ForwardError(const TCHAR *pMessage)
{
    if (g_fNetlogInitted)
    {
        NetLogError(TEXT("%s"), pMessage);
    }
    else
    if (g_pKato)
    {
        g_pKato->Log(LOG_FAIL, TEXT("%s"), pMessage);
    }
    else
    {
        RETAILMSG(TRUE, (TEXT("%s"), pMessage));
    }
}

static void
ForwardVerbose(const TCHAR *pMessage)
{
    if (g_bVerboseEnabled)
    	{
	    if (g_fNetlogInitted)
	    {
	        NetLogMessage(TEXT("%s"), pMessage);
	    }
	    else
	    if (g_pKato)
	    {
	        g_pKato->Log(LOG_DETAIL, TEXT("%s"), pMessage);
	    }
	    else
	    {
	        RETAILMSG(TRUE, (TEXT("%s"), pMessage));
	    }
    	}
}

#ifndef _MINITUX

//------------------------------------------------------------------------------
// Windows CE specific code
//------------------------------------------------------------------------------

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
   return TRUE;
}
 
//------------------------------------------------------------------------------
// ShellProc()
//------------------------------------------------------------------------------

SHELLPROCAPI ShellProc(UINT uiMessage, SPPARAM spParam)
{
   INT rc = SPR_NOT_HANDLED;
   DWORD options_flag    = NETLOG_KATO_RQ;
   DWORD level           = LOG_DETAIL;
   DWORD use_multithread = FALSE;
	
   switch (uiMessage) {

   case SPM_LOAD_DLL:
      // If we are UNICODE, then tell Tux this by setting the following flag.
      if (g_bUnicode) ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
      // Get/create a global logging object
      g_pKato = (CKato*)KatoGetDefaultObject();
	// Initialize logwrapper and CmnPrint.
	g_fNetlogInitted = NetLogInitWrapper(g_pDllName, level, options_flag, use_multithread);
#ifndef NETLOG_WORKS_PROPERLY
	g_fNetlogInitted = false;
#endif

      CmnPrint_Initialize();
      CmnPrint_RegisterPrintFunction(PT_LOG,     ForwardDebug,   FALSE);
      CmnPrint_RegisterPrintFunction(PT_WARN1,   ForwardWarning, FALSE);
      CmnPrint_RegisterPrintFunction(PT_WARN2,   ForwardWarning, FALSE);
      CmnPrint_RegisterPrintFunction(PT_FAIL,    ForwardError,   FALSE);
      CmnPrint_RegisterPrintFunction(PT_VERBOSE, ForwardVerbose,  FALSE);

      // Initialize our global critical section
      InitializeCriticalSection(&g_csProcess);
      rc = SPR_HANDLED;
      break;

   case SPM_UNLOAD_DLL:
      // This is a good place to destroy our global critical section.
      DeleteCriticalSection(&g_csProcess);
	// Uninitialize logwrapper
       NetLogCleanupWrapper();

      rc = SPR_HANDLED;
      break;

   case SPM_SHELL_INFO:
      // Store a pointer to our shell info for later use.
      g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
      // This should be there because we can cancel test only there
      rc = ProcessBeginScript(LogKato);
      break;

   case SPM_REGISTER:
      ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
      rc = g_bUnicode ? SPR_HANDLED | SPF_UNICODE : SPR_HANDLED;
      break;

   case SPM_START_SCRIPT:
      rc = SPR_HANDLED;
      break;

   case SPM_STOP_SCRIPT:
      rc = ProcessEndScript();
      break;

   case SPM_BEGIN_GROUP:
      rc = ProcessBeginGroup((LPFUNCTION_TABLE_ENTRY)spParam);
      break;

   case SPM_END_GROUP:
      rc = ProcessEndGroup((LPFUNCTION_TABLE_ENTRY)spParam);
      break;

   case SPM_BEGIN_TEST:
      rc = ProcessBeginTest((LPSPS_BEGIN_TEST)spParam);
      break;

    case SPM_END_TEST:
      rc = ProcessEndTest((LPSPS_END_TEST)spParam);
      break;

   case SPM_EXCEPTION:
      rc = ProcessException((LPSPS_EXCEPTION)spParam);
      break;
   }

   return rc;
}

#else

//------------------------------------------------------------------------------
// ExceptionFilter()
//------------------------------------------------------------------------------

LONG ExceptionFilter(
   DWORD dwCode, EXCEPTION_POINTERS *pEP, LPFUNCTION_TABLE_ENTRY lpFTE, 
   UINT uMsg
)
{
   SPS_EXCEPTION spsException;

   // Create exception structure
   ZeroMemory(&spsException, sizeof(spsException));
   spsException.lpFTE = lpFTE;
   spsException.dwExceptionCode = dwCode;
   spsException.lpExceptionPointers = pEP;
   spsException.dwExceptionFilter = EXCEPTION_EXECUTE_HANDLER;
   spsException.uMsg = uMsg;

   // Notify the test DLL of the exception and allow it to modify the exception.
   ProcessException(&spsException);

   // Return the appropriate value to the __except() clause
   return spsException.dwExceptionFilter;
}

//------------------------------------------------------------------------------
// WinMain()
//------------------------------------------------------------------------------

int WINAPI WinMain(
   HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd
)
{
   UINT rc = TPR_PASS;
   LPFUNCTION_TABLE_ENTRY pFTE = NULL;
   UINT uiDepth = 0;
   LPFUNCTION_TABLE_ENTRY apFTE[32];
   SPS_BEGIN_TEST spsBeginTest;
   SPS_END_TEST spsEndTest;
   TPS_EXECUTE tpsExecute;
   DWORD dwStartTime = 0;
   DWORD dwStopTime = 0;

   // Be a good citizen
   memset(apFTE, 0, sizeof apFTE);

   // Create a shell info
   g_pShellInfo = (LPSPS_SHELL_INFO)LocalAlloc(LPTR, sizeof(SPS_SHELL_INFO));
   g_pShellInfo->hInstance = hInstance;
   g_pShellInfo->hWnd = NULL;
   g_pShellInfo->hLib = NULL;
   g_pShellInfo->hevmTerminate = NULL;
   g_pShellInfo->fUsingServer = FALSE;
   g_pShellInfo->szDllCmdLine = lpCmdLine;

   // Call begin group processing
   rc = ProcessBeginScript(NULL);
   if (rc == SPR_FAIL) goto cleanUp;

   // Walk through test table
   pFTE = g_lpFTE;
   while (pFTE->lpDescription != NULL) {

      // When we end with a group
      while (pFTE->uDepth < uiDepth) {
         ASSERT(uiDepth > 0);
         uiDepth--;
         ProcessEndGroup(apFTE[uiDepth]);
      }

      // Is it a group or a test?
      if (pFTE->lpTestProc == NULL) {

         ProcessBeginGroup(pFTE);
         apFTE[uiDepth] = pFTE;
         uiDepth++;

      } else {

         // Call begin test processing
         spsBeginTest.lpFTE = pFTE;
         spsBeginTest.dwRandomSeed = Random();
         spsBeginTest.dwThreadCount = 0;

         ProcessBeginTest(&spsBeginTest);


         // Call test itself
         tpsExecute.dwRandomSeed = spsBeginTest.dwRandomSeed;
         tpsExecute.dwThreadCount = spsBeginTest.dwThreadCount;
         tpsExecute.dwThreadNumber = 1;

         dwStartTime = GetTickCount();

         __try {
            rc = pFTE->lpTestProc(TPM_EXECUTE, (TPPARAM)&tpsExecute, pFTE);
         } __except (ExceptionFilter(
            GetExceptionCode(), GetExceptionInformation(), pFTE, TPM_EXECUTE
         )) { }

         dwStopTime = GetTickCount();

         // Call end test processing
         spsEndTest.lpFTE = spsBeginTest.lpFTE;
         spsEndTest.dwResult = (DWORD)rc;
         spsEndTest.dwExecutionTime = dwStopTime - dwStartTime;
         spsEndTest.dwRandomSeed = spsBeginTest.dwRandomSeed;
         spsEndTest.dwThreadCount = spsBeginTest.dwThreadCount;

         ProcessEndTest(&spsEndTest);

      }

      // Move to next test
      pFTE++;
   }

   // Call end group processing
   rc = ProcessEndScript();
   if (rc == SPR_FAIL) goto cleanUp;

cleanUp:
   if (g_pShellInfo != NULL) {
      LocalFree((HLOCAL)g_pShellInfo);
   }
   return 0;
}

#endif // MINITUX

//------------------------------------------------------------------------------
// ProcessBeginScript()
//------------------------------------------------------------------------------

INT ProcessBeginScript(PLOG_FUNCTION pLogFce)
{
   INT rc = SPR_FAIL;
   TCHAR szCmdLine[512] = _T("");
   LPTSTR argv[64];
   int argc = 64;
   BOOL bNoWatt = FALSE;
   LPTSTR szOption = NULL;
   BOOL bOk = FALSE;

   // Make a local copy of command line
   if (_tcslen(g_pShellInfo->szDllCmdLine) > 511) goto cleanUp;
   StringCchCopy(szCmdLine, _countof(szCmdLine), g_pShellInfo->szDllCmdLine);

   // Divide command line to token
   CommandLineToArgs(szCmdLine, &argc, argv);

   // Let parse test specific options
   bOk = ParseCommands(argc, argv);
   if (!bOk) goto cleanUp;

   // When we are there it's success
   rc = SPR_HANDLED;

cleanUp:
   return rc;
}

//------------------------------------------------------------------------------
// ProcessEndScript()
//------------------------------------------------------------------------------

INT ProcessEndScript()
{
   return SPR_HANDLED;
}

//------------------------------------------------------------------------------
// ProcessBeginGroup()
//------------------------------------------------------------------------------

INT ProcessBeginGroup(LPFUNCTION_TABLE_ENTRY pFTE)
{
#ifndef _MINITUX   
   g_pKato->BeginLevel(0, _T("BEGIN GROUP: ndt_BasicTests.dll"));
#endif
   return SPR_HANDLED;
}

//------------------------------------------------------------------------------
// ProcessEndGroup()
//------------------------------------------------------------------------------

INT ProcessEndGroup(LPFUNCTION_TABLE_ENTRY pFTE)
{
#ifndef _MINITUX   
   g_pKato->EndLevel(_T("END GROUP: ndt_BasicTests.dll"));
#endif
   return SPR_HANDLED;
}

//------------------------------------------------------------------------------
// ProcessBeginTest()
//------------------------------------------------------------------------------

INT ProcessBeginTest(LPSPS_BEGIN_TEST pBT)
{
#ifndef _MINITUX   
   // Start our logging level.
   g_pKato->BeginLevel(
      pBT->lpFTE->dwUniqueID, _T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
      pBT->lpFTE->lpDescription, pBT->dwThreadCount, pBT->dwRandomSeed
   );
#endif
   return SPR_HANDLED;
}

//------------------------------------------------------------------------------
// ProcessEndTest()
//------------------------------------------------------------------------------

INT ProcessEndTest(LPSPS_END_TEST pET)
{  
#ifndef _MINITUX   
   LPTSTR szResult = NULL;
   switch (pET->dwResult) {
   case TPR_SKIP:
      szResult = _T("SKIPPED");
      break;
   case TPR_PASS:
      szResult = _T("PASSED");
      break;
   case TPR_FAIL:
      szResult = _T("FAILED");
      break;
   default:
      szResult = _T("ABORTED");
      break;
   }
   g_pKato->EndLevel(
      _T("END TEST: \"%s\", %s, Time=%u.%03u"), pET->lpFTE->lpDescription,
      szResult, pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000
   );
#endif
   return SPR_HANDLED;
}

//------------------------------------------------------------------------------
// ProcessException()
//------------------------------------------------------------------------------

INT ProcessException(LPSPS_EXCEPTION pException)
{
   g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
   return SPR_HANDLED;
}

//------------------------------------------------------------------------------

void LogKato(DWORD dwVerbosity, LPCTSTR sz)
{
#ifndef _MINITUX   
   g_pKato->Log(dwVerbosity, sz);
#endif
}

//------------------------------------------------------------------------------

void CommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]) 
{
   int i, nArgc = 0;
   BOOL bInQuotes, bEndFound = FALSE;
   TCHAR *p = szCommandLine;

   if (szCommandLine == NULL || argc == NULL || argv == NULL) return;

   for (i = 0; i < *argc; i++) {
        
      // Clear our quote flag
      bInQuotes = FALSE;

      // Move past and zero out any leading whitespace
      while (*p && _istspace(*p)) *(p++) = _T('\0');

      // If the next character is a quote, move over it and remember it
      if (*p == _T('\"')) {
        *(p++) = _T('\0');
        bInQuotes = TRUE;
      }

      // Point the current argument to our current location
      argv[i] = p;

      // If this argument contains some text, then update our argument count
      if (*p != _T('\0')) nArgc = i + 1;

      // Move over valid text of this argument.
      while (*p != _T('\0')) {
         if (_istspace(*p) || (bInQuotes && (*p == _T('\"')))) {
            *(p++) = _T('\0');
            break;
         }
         p++;
      }
      
      // If reach end of string break;
      if (*p == _T('\0')) break;
   }
   *argc = nArgc;
}

//------------------------------------------------------------------------------