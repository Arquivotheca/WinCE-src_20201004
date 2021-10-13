// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 1996-2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------
#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include "tuxmain.h"

#ifndef UNDER_CE
#include <stdio.h>
#endif

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

//******************************************************************************
//***** Windows CE specific code
//******************************************************************************
#ifdef UNDER_CE

#ifndef STARTF_USESIZE
#define STARTF_USESIZE     0x00000002
#endif

#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#ifndef _vsntprintf
#define _vsntprintf(d,c,f,a) wvsprintf(d,f,a)
#endif

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
   return TRUE;
}
#endif
void ChangeCommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]) ;  
void OutputInstructions(void);
//******************************************************************************
VOID Debug(LPCTSTR szFormat, ...) {
   TCHAR szBuffer[1024] = TEXT("TUXDEMO: ");

   va_list pArgs; 
   va_start(pArgs, szFormat);
   _vsntprintf(szBuffer + 9, countof(szBuffer) - 11, szFormat, pArgs);
   va_end(pArgs);

   _tcscat(szBuffer, TEXT("\r\n"));

   OutputDebugString(szBuffer);
}

//******************************************************************************
//***** ShellProc()
//******************************************************************************

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {

   switch (uMsg) {

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
         Debug(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

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
         Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));

         // This is a good place to delete our global critical section.
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
		  Debug(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));
         
		  // Store a pointer to our shell info for later use.
		  g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
				  
		  int argc = MAX_COMMAND_LINE_ELEM, i, iOption = NO_OPTION;
		  TCHAR *argv[MAX_COMMAND_LINE_ELEM], szCommandLine[MAX_COMMAND_LINE_LEN];
		  if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) 
		  {
			// Parse the command line for the server name, port, username, password and domain
			_tcscpy(szCommandLine, g_pShellInfo->szDllCmdLine);
			ChangeCommandLineToArgs(szCommandLine, &argc, argv);
				  
			_tcscpy(szPhoneNumber, TEXT("#"));   /* initialize telephone number to default */

			for (i = 0; i < argc; ++i)
			{
				if ((_tcsnicmp(argv[i], TEXT("-h"), 2) == 0) ||
				  (_tcsnicmp(argv[i], TEXT("/h"), 2) == 0) ||
				  (_tcsnicmp(argv[i], TEXT("-?"), 2) == 0) ||
				  (_tcsnicmp(argv[i], TEXT("/?"), 2) == 0) ||
				  (_tcsnicmp(argv[i], TEXT("?"), 1) == 0))	
				{
					OutputInstructions();
					return SPR_FAIL;
				}

				else if ((_tcsnicmp(argv[i], TEXT("-l"), 2) == 0) ||
 				  (_tcsnicmp(argv[i], TEXT("/l"), 2) == 0))
					bListAllDevices = TRUE;

				else if ((_tcsnicmp(argv[i], TEXT("-n"), 2) == 0) ||
 				  (_tcsnicmp(argv[i], TEXT("/n"), 2) == 0))
				{
					if (_tcslen(argv[i]) > 2)
						dwNumCalls = _tcstoul(argv[i] + 2, NULL, 10);
					if (!dwNumCalls)                    /* always make at least one call */
						dwNumCalls++;
				}

				else if ((_tcsnicmp(argv[i], TEXT("-td"), 3) == 0) ||
 				  (_tcsnicmp(argv[i], TEXT("/td"), 3) == 0))
				{
					if (_tcslen(argv[i]) > 3)
						dwDialingTimeout = _tcstoul(argv[i] + 3, NULL, 10) * 1000;
				}

				else if ((_tcsnicmp(argv[i], TEXT("-tc"), 3) == 0) ||
 				  (_tcsnicmp(argv[i], TEXT("/tc"), 3) == 0))
				{
					if (_tcslen(argv[i]) > 3)
						dwConnectionDelay = _tcstoul(argv[i] + 3, NULL, 10) * 1000;
				}

				else if ((_tcsnicmp(argv[i], TEXT("-th"), 3) == 0) ||
 				  (_tcsnicmp(argv[i], TEXT("/th"), 3) == 0))
				{
					if (_tcslen(argv[i]) > 3)
						dwHangupTimeout = _tcstoul(argv[i] + 3, NULL, 10) * 1000;
				}

				else if ((_tcsnicmp(argv[i], TEXT("-tw"), 3) == 0) ||
 				  (_tcsnicmp(argv[i], TEXT("/tw"), 3) == 0))
				{
					if (_tcslen(argv[i]) > 3)
						dwDeallocateDelay = _tcstoul(argv[i] + 3, NULL, 10) * 1000;
				}

				else if ((_tcsnicmp(argv[i], TEXT("-p"), 2) == 0) ||
				  (_tcsnicmp(argv[i], TEXT("/p"), 2) == 0))
				{
					if (_tcslen(argv[i]) > 2)
					{
						if (*(argv[i] + 2) == TEXT(' '))      /* skip over leading space */
							_tcscpy(szPhoneNumber, argv[i] + 3);
						else
							_tcscpy(szPhoneNumber, argv[i] + 2);
					}
				}

				else if ((_tcsnicmp(argv[i], TEXT("-d"), 2) == 0) ||
 				  (_tcsnicmp(argv[i], TEXT("/d"), 2) == 0))
				{
					if (_tcslen(argv[i]) > 2)
						dwDeviceID = _tcstoul(argv[i] + 2, NULL, 10);
				}

				else if ((_tcsnicmp(argv[i], TEXT("-r"), 2) == 0) ||
 				  (_tcsnicmp(argv[i], TEXT("/r"), 2) == 0))
				{
					if (_tcslen(argv[i]) > 2)
						dwBaudrate = _tcstoul(argv[i] + 2, NULL, 10);
				}

				else if ((_tcsnicmp(argv[i], TEXT("-e"), 2) == 0) ||
 				  (_tcsnicmp(argv[i], TEXT("/e"), 2) == 0))
					bEditDevConfig = TRUE;
			} //for
		  } //if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) 

         return SPR_HANDLED;
      } //case SPM_SHELL_INFO

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
         Debug(TEXT("ShellProc(SPM_REGISTER, ...) called"));
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
         Debug(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
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
         Debug(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
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
         Debug(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TUXDEMO.DLL"));
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
         Debug(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
         g_pKato->EndLevel(TEXT("END GROUP: TUXDEMO.DLL"));
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
         Debug(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));

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
         Debug(TEXT("ShellProc(SPM_END_TEST, ...) called"));

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
         Debug(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
         g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
         return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;
}

void OutputInstructions(void)
{
    g_pKato->Log(LOG_COMMENT, TEXT("\n\t") TEXT(_FILENAM) TEXT(_FILEVER) TEXT(_TESTDESCR)); 
    g_pKato->Log(LOG_COMMENT, TEXT("Usage: ") TEXT(_FILENAM) TEXT("[-h][-n<xx>][-p<xx>][-d<xx>]"));
    g_pKato->Log(LOG_COMMENT, TEXT("   [-td<xx>][-tc<xx>][-th<xx>][-tw<xx>][-r<xx>][-e]"));
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-h] - print help (this screen)"));
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-n<number of calls>] - set number of calls to be made (default = 1)"));
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-p<phone>] - set phone number (default = # = NULL)"));
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-d<device>] - set device number (default = 0)"));
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-td<seconds>] - set dialing timeout (default = %lu seconds)"), dwDialingTimeout / 1000);
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-tc<seconds>] - set connection delay (default = %lu seconds)"), dwConnectionDelay / 1000);
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-th<seconds>] - set hangup timeout (default = %lu seconds)"), dwHangupTimeout / 1000);
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-tw] - set delay before deallocating call (default = %lu seconds)"), dwDeallocateDelay / 1000);
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-r<baudrate>] - set baudrate (default = 19200)"));
	g_pKato->Log(LOG_COMMENT, TEXT("\t[-e] - edit device configuration and current location"));
	g_pKato->Log(LOG_COMMENT, TEXT("Command line example (from PPSH/CESH): s tapiclient -v14 -n3 -d3 -e"));
}
void ChangeCommandLineToArgs(TCHAR szCommandLine[], int *argc, LPTSTR argv[]) 
{
    // Parse command line into individual arguments - arg/argv style.
	int i, iArgc = 0;
    BOOL fInQuotes;
	TCHAR *p = szCommandLine;

    for (i = 0; i < *argc; i++)
    {
        // Clear our quote flag
        fInQuotes = FALSE;

        // Move past and zero out any leading whitespace
        while( *p && _istspace(*p) )
            *(p++) = TEXT('\0');

        // If the next character is a quote, move over it and remember that we saw it
        if (*p == TEXT('\"'))
        {
            *(p++) = TEXT('\0');
            fInQuotes = TRUE;
        }

        // Point the current argument to our current location
        argv[i] = p;

        // If this argument contains some text, then update our argument count
        if (*argv[i])
            iArgc = i + 1;

        // Move over valid text of this argument.
        while (*p)
        {
            if (fInQuotes)
            {
                // If in quotes, we only break out when we see another quote.
                if (*p == TEXT('\"'))
                {
                    *(p++) = TEXT('\0');
                    break;
                }
				
			}
			else
			{
				// If we are not in quotes and we see a quote, replace it with a space
				// and set "fInQuotes" to TRUE
				if (*p == TEXT('\"'))
				{
					*(p++) = TEXT(' ');
					fInQuotes = TRUE;
				}   // If we are not in quotes and we see whitespace, then we break out
				else if (_istspace(*p))
				{
					*(p++) = TEXT('\0');
					break;
				}
            }
            // Move to the next character
            p++;
        }
    }

    *argc = iArgc;
}
