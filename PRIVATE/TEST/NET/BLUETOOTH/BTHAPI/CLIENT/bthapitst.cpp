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

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  @doc Bluetooth BthXxx API Test Client

  @test bthapisvr | Ji Li (jil) | Bluetooth BthXxx API Test Client |

	This program depends on L2CAPAPI

  @tcindex BTHAPITST

*/
/*

Revision History:
	V0.1   16-May-2003	Original version

-------------------------------------------------------------*/

#include <windows.h>
#include <winbase.h>
#include <katoex.h>
#include <tux.h>
#include <tchar.h>
#include <cmdline.h>
#include <bt_api.h>
#include "logwrap.h"

#define _FILENAM  "BTHAPITST"

extern FUNCTION_TABLE_ENTRY g_lpFTE[];

void Usage();
int GetBA (WCHAR **, BT_ADDR *);

// Server Bluetooth address
BT_ADDR g_ServerAddress;
BOOL g_fHasServer = FALSE;

//--------------------- Tux & Kato Variable Initilization ---------------------

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato			*g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO	*g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION	g_csProcess;

//-------------------------- Global & extern variables ------------------------


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
   DWORD options_flag    = NETLOG_DEBUG_OP;
   DWORD level           = LOG_PASS;
   DWORD use_multithread = FALSE;

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

		 // Initialize logwrapper
	     NetLogInitWrapper(TEXT(_FILENAM), level, options_flag, use_multithread);

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

		 // Uninitialize logwrapper
		 NetLogCleanupWrapper();

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

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_REGISTER
      //
      // This is the only ShellProc() message that a DLL is required to handle
      // (except for SPM_LOAD_DLL if you are UNICODE).  This message is sent
      // once to the DLL immediately after the SPM_SHELL_INFO message to query
      // the DLL for it's function table.  The spParam will contain a pointer to
      // a SPS_REGISTER structure.  The DLL should store its function table in
      // the lpFunctionTable member of the SPS_REGISTER structure.  The DLL may
      // return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_REGISTER: {
	 	 ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
		
		 #define MAX_COMMAND_LINE_ELEM	32

		 TCHAR szCommandLine[MAX_PATH];
		 int argc = MAX_COMMAND_LINE_ELEM;
		 TCHAR *argv[MAX_COMMAND_LINE_ELEM];

		 memset(&g_ServerAddress, 0, sizeof(g_ServerAddress));
		 g_fHasServer = FALSE;

		 if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine)
		 {
			 _tcscpy(szCommandLine, g_pShellInfo->szDllCmdLine);
			 CommandLineToArgs(szCommandLine, &argc, argv);

			 // StrictOptionsOnly(TRUE);

			 if((WasOption(argc, argv, TEXT("?")) >= 0) || (WasOption(argc, argv, TEXT("help")) >= 0))
			 {
				Usage();
				return SPR_FAIL;
			 }

			 TCHAR szOptionArray [][5] = {TEXT("s")};
			 int numberOfOptions = 1;

			 int i, woRetVal;
			 LPTSTR lpszGetOptStr = NULL;

			 for (i = 0; i < numberOfOptions; i++)
			 {
				 woRetVal = WasOption(argc, argv, szOptionArray[i]);

				 if (woRetVal < -1)
				 {
					 // Something went wrong while parsing command line
					 QAError(TEXT("Error parsing command line for option -%s\n"), szOptionArray[i]);
					 Usage();
					 return SPR_FAIL;
				 }
				 else if (woRetVal == -1)
				 {
					 // Missing option
					 if (i == 0) // -s option
					 {
						 // No server name means to skip connection tests.
						 g_fHasServer = FALSE;
					 }
				 }
				 else
				 {
					 if (i == 0) // -s option
					 {
						 GetOption(argc, argv, szOptionArray[i], (TCHAR **)&lpszGetOptStr);
						 if ((lpszGetOptStr == NULL) || !GetBA(&lpszGetOptStr, &g_ServerAddress))
						 {
							 // Something went wrong while parsing command line
							 QAError(TEXT("Error parsing command line for option -%s\n"), szOptionArray[i]);
							 Usage();
							 return SPR_FAIL;
						 }
						 g_fHasServer = TRUE;
						 QAMessage(TEXT("Server address is 0x%04x%08x"), GET_NAP(g_ServerAddress), GET_SAP(g_ServerAddress));
					 }
				 }
			 }
		 }
	 
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
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: BTW.DLL"));

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
         g_pKato->EndLevel(TEXT("END GROUP: BTW.DLL"));

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

		 // Tests need to be run sequencially.
		 EnterCriticalSection(&g_csProcess);

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

		 // Tests need to run sequentially.
		 LeaveCriticalSection(&g_csProcess);

         g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                           pET->lpFTE->lpDescription,
                           pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                           pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                           pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                           pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
         
	     Sleep(500); // Give hardware sometime

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

/**************************************************************
**** internal functions
***************************************************************/

void Usage()
{
	QAMessage(TEXT("Bluetooth BthXxx API Test Client"));
	QAMessage(TEXT("--------------------------------"));
	QAMessage(TEXT(""));
	QAMessage(TEXT("Usage:"));
	QAMessage(TEXT("tux -o -d bthapitst.dll -c \"-s <Address>\""));
	QAMessage(TEXT("<Address> : Server Bluetooth Address"));
	QAMessage(TEXT(""));
	QAMessage(TEXT("Example:"));
	QAMessage(TEXT("tux -o -d bthapitst.dll -c \"-s 123456789012\""));

	return;
}

int GetBA (WCHAR **pp, BT_ADDR *pba) {
	while (**pp == ' ')
		++*pp;

	for (int i = 0 ; i < 4 ; ++i, ++*pp) {
		if (! iswxdigit (**pp))
			return FALSE;

		int c = **pp;
		if (c >= 'a')
			c = c - 'a' + 0xa;
		else if (c >= 'A')
			c = c - 'A' + 0xa;
		else c = c - '0';

		if ((c < 0) || (c > 16))
			return FALSE;

		*pba = *pba * 16 + c;
	}

	for (i = 0 ; i < 8 ; ++i, ++*pp) {
		if (! iswxdigit (**pp))
			return FALSE;

		int c = **pp;
		if (c >= 'a')
			c = c - 'a' + 0xa;
		else if (c >= 'A')
			c = c - 'A' + 0xa;
		else c = c - '0';

		if ((c < 0) || (c > 16))
			return FALSE;

		*pba = *pba * 16 + c;
	}

	if ((**pp != ' ') && (**pp != '\0'))
		return FALSE;

	return TRUE;
}

// END OF FILE