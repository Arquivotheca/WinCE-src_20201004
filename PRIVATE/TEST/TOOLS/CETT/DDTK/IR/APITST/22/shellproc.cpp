//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include <winsock2.h>
#include <oscfg.h>
#include <katoex.h>
#include <tux.h>
#undef AF_IRDA
#include <af_irda.h>
#include "irsup.h"
#include "shellproc.h"
#include "irsrv.h"


//--------------------- Tux & Kato Variable Initilization ---------------------

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato			*g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO	*g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION	g_csProcess;

//--------------------- Global Variable Initilization -------------------------

int		g_af = AF_IRDA;
char*	g_szServerName = IRTEST_SERVERNAME;
char	g_bufServerName[64];

//-------------------------- Windows CE specific code -------------------------

#ifdef UNDER_CE

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) 
{
   return TRUE;
}
#endif
 
/*-----------------------------------------------------------------------------
  Function		: ShellProc()
  Description	:
  Parameters	:
  Returns		:
  Comments		:
------------------------------------------------------------------------------*/

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) 
{

   switch (uMsg) 
   {
	  //------------------------------------------------------------------------
      // Message: SPM_LOAD_DLL
      //
      // Sent once to the DLL immediately after it is loaded.  The spParam 
      // parameter will contain a pointer to a SPS_LOAD_DLL structure.  The DLL
      // should set the fUnicode member of this structre to TRUE if the DLL is
      // built with the UNICODE flag set.  By setting this flag, Tux will ensure
      // that all strings passed to your DLL will be in UNICODE format, and all
      // strings within your function table will be processed by Tux as UNICODE.
      // The DLL may return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_LOAD_DLL: {
         // If we are UNICODE, then tell Tux this by setting the following flag.
         #ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
         #endif

         // Get/Create our global logging object.
         g_pKato = (CKato*)KatoGetDefaultObject();

         // Initialize our global critical section.
         InitializeCriticalSection(&g_csProcess);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_UNLOAD_DLL
      //
      // Sent once to the DLL immediately before it is unloaded.
      //------------------------------------------------------------------------

      case SPM_UNLOAD_DLL: {
         // This is a good place to destroy our global critical section.
         DeleteCriticalSection(&g_csProcess);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_SHELL_INFO
      //
      // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
      // some useful information about its parent shell and environment.  The
      // spParam parameter will contain a pointer to a SPS_SHELL_INFO structure.
      // The pointer to the structure may be stored for later use as it will
      // remain valid for the life of this Tux Dll.  The DLL may return SPR_FAIL
      // to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_SHELL_INFO: {
         // Store a pointer to our shell info for later use.
         g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

         // Display our Dlls command line if we have one.
         if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) 
		 {
			 g_af = AF_INET;
			 g_szServerName = &g_bufServerName[0];

			 const TCHAR *psrc = g_pShellInfo->szDllCmdLine;
			 char  *pdst = g_szServerName;

			 while (1)
			 {
				 *pdst = (char)*psrc;
				 if (*pdst == '\0')
					 break;
				 pdst++, psrc++;
			 }
         }

		 IRSockInit();

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_REGISTER
      //
      // This is the only ShellProc() message that a DLL is required to handle
      // (except for SPM_LOAD_DLL if you are UNICODE).  This message is sent
      // once to the DLL immediately after the SPM_SHELL_INFO message to query
      // the DLL for it’s function table.  The spParam will contain a pointer to
      // a SPS_REGISTER structure.  The DLL should store its function table in
      // the lpFunctionTable member of the SPS_REGISTER structure.  The DLL may
      // return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_REGISTER: {
         ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
         #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
         #else
            return SPR_HANDLED;
         #endif
      }

      //------------------------------------------------------------------------
      // Message: SPM_START_SCRIPT
      //
      // Sent to the DLL immediately before a script is started.  It is sent to
      // all Tux DLLs, including loaded Tux DLLs that are not in the script.
      // All DLLs will receive this message before the first TestProc() in the
      // script is called.
      //------------------------------------------------------------------------

      case SPM_START_SCRIPT: {
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_STOP_SCRIPT
      //
      // Sent to the DLL when the script has stopped.  This message is sent when
      // the script reaches its end, or because the user pressed stopped prior
      // to the end of the script.  This message is sent to all Tux DLLs,
      // including loaded Tux DLLs that are not in the script.
      //------------------------------------------------------------------------

      case SPM_STOP_SCRIPT: {
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_GROUP
      //
      // Sent to the DLL before a group of tests from that DLL is about to be
      // executed.  This gives the DLL a time to initialize or allocate data for
      // the tests to follow.  Only the DLL that is next to run receives this
      // message.  The prior DLL, if any, will first receive a SPM_END_GROUP
      // message.  For global initialization and de-initialization, the DLL
      // should probably use SPM_START_SCRIPT and SPM_STOP_SCRIPT, or even
      // SPM_LOAD_DLL and SPM_UNLOAD_DLL.
      //------------------------------------------------------------------------

      case SPM_BEGIN_GROUP: {
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: IRIASTST.DLL"));
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_GROUP
      //
      // Sent to the DLL after a group of tests from that DLL has completed
      // running.  This gives the DLL a time to cleanup after it has been run.
      // This message does not mean that the DLL will not be called again to run
      // tests; it just means that the next test to run belongs to a different
      // DLL.  SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL to track when it
      // is active and when it is not active.
      //------------------------------------------------------------------------

      case SPM_END_GROUP: {
         g_pKato->EndLevel(TEXT("END GROUP: IRIASTST.DLL"));
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_TEST
      //
      // Sent to the DLL immediately before a test executes.  This gives the DLL
      // a chance to perform any common action that occurs at the beginning of
      // each test, such as entering a new logging level.  The spParam parameter
      // will contain a pointer to a SPS_BEGIN_TEST structure, which contains
      // the function table entry and some other useful information for the next
      // test to execute.  If the ShellProc function returns SPR_SKIP, then the
      // test case will not execute.
      //------------------------------------------------------------------------

      case SPM_BEGIN_TEST: {
         // Start our logging level.
         LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
         g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                             TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                             pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                             pBT->dwRandomSeed);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_TEST
      //
      // Sent to the DLL after a single test executes from the DLL.  This gives
      // the DLL a time perform any common action that occurs at the completion
      // of each test, such as exiting the current logging level.  The spParam
      // parameter will contain a pointer to a SPS_END_TEST structure, which
      // contains the function table entry and some other useful information for
      // the test that just completed.
      //------------------------------------------------------------------------

      case SPM_END_TEST: {
         // End our logging level.
         LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;
         g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                           pET->lpFTE->lpDescription,
                           pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                           pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                           pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                           pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_EXCEPTION
      //
      // Sent to the DLL whenever code execution in the DLL causes and exception
      // fault.  By default, Tux traps all exceptions that occur while executing
      // code inside a Tux DLL.
      //------------------------------------------------------------------------

      case SPM_EXCEPTION: {
         g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
         return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;
}


// END OF FILE