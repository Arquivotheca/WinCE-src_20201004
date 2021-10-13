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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------

#include "Backlight_detector.h"
#include <strsafe.h>
#ifndef UNDER_CE
    #include <stdio.h>
#endif

CKato *g_pKato = NULL;
SPS_SHELL_INFO *g_pShellInfo;

VOID Debug(LPCTSTR szFormat, ...) {
    TCHAR szBuffer[1024] = DEBUG_LABEL_TEXT;
    DWORD szBufferLen = _tcsclen(DEBUG_LABEL_TEXT);
    DWORD countOf = _countof(szBuffer);

    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(szBuffer + szBufferLen, countOf - szBufferLen, countOf - DEBUG_LABEL_INDENT, szFormat, pArgs);
    va_end(pArgs);

    _tcscat_s(szBuffer, countOf, _T("\r\n"));

    OutputDebugString(szBuffer);
}

//*****************************************************************************
//
//  Parameters: standard Tux arguments, INT for # of BKL's
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Description: This helper function detects if there is one or multiple BKL drivers
//               depending on parameters passed in.
//
//*****************************************************************************
TESTPROCAPI DetectBKLDriver( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE,
                             INT numBacklights )
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE ) {
        return TPR_NOT_HANDLED;
    }

    DWORD dwReturnValue = TPR_FAIL;
    LPCTSTR pszName[] = {GUIDSTRING_BKL0, GUIDSTRING_BKL1};
    HANDLE hFindDrvr;
    DEVMGR_DEVICE_INFORMATION ddiBKLDriver;
    INT bklCount, bklFound;
    DWORD dwErr;
    union {
        BYTE rgbGuidBuffer[sizeof(GUID) + 4]; // +4 since scanf writes longs
        GUID guidBus;
    } u = { 0 };

    memset(&ddiBKLDriver, 0, sizeof(DEVMGR_DEVICE_INFORMATION));
    ddiBKLDriver.dwSize = sizeof(DEVMGR_DEVICE_INFORMATION);
    LPGUID pguidBus = &u.guidBus;

    bklFound = 0;
    for(bklCount = 0; bklCount < GUIDSTRINGS; bklCount++) {
        // First and foremost backlight to be detected.
        dwErr = swscanf_s(pszName[bklCount], SVSUTIL_GUID_FORMAT, SVSUTIL_PGUID_ELEMENTS(&pguidBus));

        // Check if GUID is parsed correctly.
        if (dwErr == 0 || dwErr == EOF) {
            hFindDrvr = INVALID_HANDLE_VALUE;
        }
        else {
            hFindDrvr = FindFirstDevice(DeviceSearchByGuid, pguidBus, &ddiBKLDriver);
        }

        if (hFindDrvr != INVALID_HANDLE_VALUE) {
            // Close the handle to driver.
            FindClose(hFindDrvr);
            bklFound++;
        }
    }

    Debug(_T("Found %d backlight driver(s)."), bklFound);

    if(0 == bklFound) {
        FAIL("No backlight drivers found.");
    }
    else if(bklFound > 0 && 1 == numBacklights) {
        SUCCESS("Found a backlight driver.");
        dwReturnValue = TPR_PASS;
    }
    else if(bklFound > 1 && numBacklights > 1) {
        SUCCESS("Found multiple backlight drivers.");
        dwReturnValue = TPR_PASS;
    }
    else {
        FAIL("Could not find multiple backlight drivers.");
    }

    return dwReturnValue;
}

//*****************************************************************************
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Description: This detects if there is a single backlight driver depending
//               on parameters passed in.
//
//*****************************************************************************
TESTPROCAPI SingleBKLDetectorTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    return DetectBKLDriver( uMsg, tpParam, lpFTE, 1 );
}

//*****************************************************************************
//
//  Parameters: standard Tux arguments
//
//  Return Value: TPR_PASS, TPR_FAIL
//
//  Description: This detects if there are multiple backlight drivers depending
//               on parameters passed in.
//
//*****************************************************************************
TESTPROCAPI MultiBKLDetectorTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    return DetectBKLDriver( uMsg, tpParam, lpFTE, 2 );
}

BOOL ProcessCmdLine(LPCTSTR szCmdLine )
{
    CClParse cmdLine(szCmdLine);

    if(cmdLine.GetOpt(_T("?"))) {
        Debug(_T("~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*"));
        Debug(_T("usage: s tux -o -d Backlight_detector tux_parameters -c\"DLL_parameters\""));
        Debug(_T("tux_parameters:  please refer to s tux -?"));
        Debug(_T("Test description:"));
        Debug(_T("\t-x1001\tDetect Single backlight driver."));
        Debug(_T("\t-x1002\tDetect Multiple backlight driver."));
        Debug(_T("DLL_parameters:"));
        Debug(_T("\t-?\tThis help message"));
        Debug(_T("~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*"));
        return FALSE;
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
         Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called"));

         // We are UNICODE. Tell Tux this by setting the following flag.
         ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;

         // Get/Create our global logging object.
         g_pKato = (CKato*)KatoGetDefaultObject();

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_UNLOAD_DLL
      //
      // Sent once to the DLL immediately before it is unloaded.
      //------------------------------------------------------------------------

      case SPM_UNLOAD_DLL: {
         Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));

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
         Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called"));

         // Store a pointer to our shell info for later use.
         g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

         // Display our Dlls command line if we have one.
         if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) {
#ifdef UNDER_CE
            Debug(_T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
#else
            MessageBox(g_pShellInfo->hWnd, g_pShellInfo->szDllCmdLine,
                       _T("BACKLIGHT_DETECTOR.DLL Command Line Arguments"), MB_OK);
#endif UNDER_CE
         }

        // handle command line parsing
        if(!ProcessCmdLine(g_pShellInfo->szDllCmdLine)) {
            return SPR_FAIL;
        }

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
         Debug(_T("ShellProc(SPM_REGISTER, ...) called"));
         ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
          return SPR_HANDLED | SPF_UNICODE;
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
         Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called"));
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
         Debug(_T("ShellProc(SPM_STOP_SCRIPT, ...) called"));
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
         Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called"));
         g_pKato->BeginLevel(0, _T("BEGIN GROUP: BACKLIGHT_DETECTOR.DLL"));
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
         Debug(_T("ShellProc(SPM_END_GROUP, ...) called"));
         g_pKato->EndLevel(_T("END GROUP: BACKLIGHT_DETECTOR.DLL"));
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
         Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called"));

         // Start our logging level.
         LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
         g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
                             _T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
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
         Debug(_T("ShellProc(SPM_END_TEST, ...) called"));

         // End our logging level.
         LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;
         g_pKato->EndLevel(_T("END TEST: \"%s\", %s, Time=%u.%03u"),
                           pET->lpFTE->lpDescription,
                           pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                           pET->dwResult == TPR_PASS ? _T("PASSED") :
                           pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
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
         Debug(_T("ShellProc(SPM_EXCEPTION, ...) called"));
         g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
         return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;

}
