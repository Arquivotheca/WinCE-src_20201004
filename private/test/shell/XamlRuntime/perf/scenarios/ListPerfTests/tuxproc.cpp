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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Copyright (c) 2002 Microsoft Corporation

Module Name:

    TuxProc.h

Abstract:


Environment:

    Win32 CE 


-------------------------------------------------------------------*/
 

//******************************************************************************
//***** Includes
//******************************************************************************
#include "PerfListTests.h"
#include "globals.h"
#include "tuxproc.h"

//******************************************************************************
//***** Global Variables
//******************************************************************************

////////////////////////////////////////////////////////////////////////////////
// DllMain
//  Main entry point of the DLL. Called when the DLL is loaded or unloaded.
//
// Parameters:
//  hInstance       Module instance of the DLL.
//  dwReason        Reason for the function call.
//  lpReserved      Reserved for future use.
//
// Return value:
//  TRUE if successful, FALSE to indicate an error condition.

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{ 
    // Any initialization code goes here.
    g_hInstance  = (HINSTANCE)hInstance;        
    return TRUE;
}

BOOL ParseCmdLine(LPCTSTR szDllCmdLine)
{
    WCHAR   szSeperators[] = TEXT(" ,;");
    const   WCHAR*  szToken;
    WCHAR*  pszNextToken = NULL;
    WCHAR   szCmdLine[256];

    if ( !szDllCmdLine || !*szDllCmdLine )
        return FALSE;

    // Validate length
    if (_tcslen(szDllCmdLine) > 255)
        return FALSE;

    StringCchPrintfW( szCmdLine, 255, L"%s", szDllCmdLine );
    //_tcsncpy( szCmdLine, szDllCmdLine, 255 );

    szToken = wcstok_s( szCmdLine, szSeperators, &pszNextToken );


    while ( szToken )
    {
        if( wcsncmp( szToken, L"seconds=", 8 ) == 0 )
        {
            g_OptionSeconds = _wtoi( szToken + 8 );
        }
        else if( wcsncmp( szToken, L"repeats=", 8 ) == 0 )
        {
            g_OptionRepeats = _wtoi( szToken + 8 );
        }
        else if( wcsncmp( szToken, L"UseBaml", 7 ) == 0 )
        {
            g_UseBaml = true;
        }
        else if( wcsnicmp( szToken, L"Manual", 6 ) == 0 )
        {
            g_bManualMode = true;
        }
        else if( wcsncmp( szToken, L"xml=", 4 ) == 0 )
        {
            if (!LoadDataFile( (wchar_t*)szToken+4 ) ) // load the xml data file once 
                return FALSE;
        }
        else if( wcsncmp( szToken, L"CTK=", 4 ) == 0 )
        {
            if( *(szToken+4) == '1' )
            {
                g_bRunningUnderCTK = true;
            }
        }

        szToken = wcstok_s( NULL, szSeperators, &pszNextToken);
    }

    return TRUE;
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

         // If we are UNICODE, then tell Tux this by setting the following flag.
         
         #ifdef UNICODE
            ((LPSPS_LOAD_DLL) spParam)->fUnicode = TRUE;
         #endif

         // Get/Create our global logging object.
         g_pKato =  (CKato*) KatoGetDefaultObject();

        return SPR_HANDLED;

      }

      //------------------------------------------------------------------------
      // Message: SPM_UNLOAD_DLL
      //
      // Sent once to the DLL immediately before it is unloaded.
      //------------------------------------------------------------------------

      case SPM_UNLOAD_DLL: {
         TRACE(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));

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
         TRACE(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));
         
         // Store a pointer to our shell info for later use.
         g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

         if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) {
             if( ParseCmdLine( g_pShellInfo->szDllCmdLine ) == FALSE )
             {
                 return( SPR_FAIL );
             }
         }
   
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
         TRACE(TEXT("ShellProc(SPM_REGISTER, ...) called"));
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
         TRACE(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
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
         TRACE(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
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
         TRACE(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP:"));

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
         TRACE(TEXT("ShellProc(SPM_END_GROUP, ...) called"));

         g_pKato->EndLevel(TEXT("END GROUP:"));

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
         

         TRACE(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));

         // Start our logging level.
         const SPS_BEGIN_TEST* pBT = (const SPS_BEGIN_TEST*)spParam;
         g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                             TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                             pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                             pBT->dwRandomSeed);
        
         if( !XamlRuntimeInitialize() )
        {
            g_pKato->Log(LOG_COMMENT, L"XamlRuntimeInitialize() returned false.");
            return SPR_FAIL;
        }

        if (FAILED(GetXRApplicationInstance( &g_pTestApplication )))
        {
            g_pKato->Log(LOG_COMMENT, L"GetXRApplicationInstance() failed");
            return SPR_FAIL;
        }

        if (g_pTestApplication == NULL)
        {
            g_pKato->Log(LOG_COMMENT, L"The test application is NULL.");
            return SPR_FAIL;
        }

        if (g_UseBaml)
        {
            // Add resource module
            if (FAILED(g_pTestApplication->AddResourceModule(g_hInstance)))
            {
                g_pKato->Log(LOG_COMMENT, TEXT("AddResourceModule failed"));
                return SPR_FAIL;
            }
        }

        if (!SetTestCaseData((SPS_BEGIN_TEST *)spParam))  //look for test case data in the xml file and set global element
            return SPR_SKIP;

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
         TRACE(TEXT("ShellProc(SPM_END_TEST, ...) called"));

         // End our logging level.
         const SPS_END_TEST* pET = (const SPS_END_TEST*)spParam;
         g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                           pET->lpFTE->lpDescription,
                           pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                           pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                           pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                           pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);


         CleanTestCaseData();

        if (g_UseBaml)
        {
            if (FAILED(g_pTestApplication->RemoveResourceModule(g_hInstance)))
            {
                g_pKato->Log(LOG_COMMENT, TEXT("RemoveResourceModule failed"));
                return SPR_FAIL;
            }
        }

        SAFE_RELEASE(g_pTestApplication);

        // Uninitialize application
        if (!XamlRuntimeUninitialize())
        {
            g_pKato->Log(LOG_COMMENT, L"XamlRuntimeUninitialize() returned false.");
        }

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
         TRACE(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
         g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
         return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;
}

