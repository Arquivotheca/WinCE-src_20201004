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

#include "tuxmain.h"
#include <strsafe.h>

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

BOOL g_bEnablePMtest  = FALSE;

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] =
{
    TEXT( "Group: Backlight API Tests"               ), 0, 0,           0, NULL,
    TEXT( "Get Backlight Brightness"                 ), 1, 0, BASE +    1, GetBklBrightness,
    TEXT( "Get Backlight Brightness Capabilities"    ), 1, 0, BASE +    2, GetBklBrightnessCap,
    TEXT( "Get Backlight Settings"                   ), 1, 0, BASE +    3, GetBklSettings,
    TEXT( "Set Backlight Settings"                   ), 1, 0, BASE +    4, SetBklSettings,
    TEXT( "Force Backlight Update"                   ), 1, 0, BASE +    5, ForceBklUpdate,
    TEXT( "Power Manager Backlight Operations"       ), 1, 0, BASE +    6, PwrMgrBklOps,
    // marks end of list
    NULL,                                               0, 0,           0, NULL
};

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(dwReason);
    UNREFERENCED_PARAMETER(lpReserved);
    return TRUE;
}
#endif

//******************************************************************************

VOID Debug(LPCTSTR szFormat, ...) {
    TCHAR szBuffer[1024] = TEXT("BKLAPITEST: ");

    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(szBuffer + 12, _countof(szBuffer) - 12, _countof(szBuffer) - 11, szFormat, pArgs);
    va_end(pArgs);

    _tcscat_s(szBuffer, _countof(szBuffer), TEXT("\r\n"));

    OutputDebugString(szBuffer);
}

//******************************************************************************
//******************************************************************************
//
// This should be remove by M4 - temporary add registry to enable device manager ACL.
//
//******************************************************************************
//******************************************************************************
BOOL ModifySecurityRegistry(BOOL bEnableSecurity)
{
    RegManipulate   RegMani(HKEY_LOCAL_MACHINE);
    //check if this key already exists, if so, return
    if(RegMani.IsAKeyValidate(DEVLOAD_DRIVERS_KEY) == FALSE){
        NKMSG(L"Key %s does not exist!", DEVLOAD_DRIVERS_KEY);
        return FALSE;
    }
    BOOL bRet = TRUE;

// These are generic flags sitting in HKLM\Drivers - this will be remove afterward when the
// second check in in place

    DWORD dwCurrentFlag = bEnableSecurity ? 1 : 0;

    bRet = RegMani.SetAValue(DEVLOAD_DRIVERS_KEY,
                                        L"EnforceDevMgrSecurity",
                                        REG_DWORD,
                                        sizeof(DWORD),
                                        (PBYTE)&dwCurrentFlag);

    if(bRet == FALSE){
        NKMSG(L"Can not set EnforcedDevMgrSecurity on %s!", DEVLOAD_DRIVERS_KEY);
        return FALSE;
    }

    bRet = RegMani.SetAValue(DEVLOAD_DRIVERS_KEY,
                                        L"udevicesecurity",
                                        REG_DWORD,
                                        sizeof(DWORD),
                                        (PBYTE)&dwCurrentFlag);

    if(bRet == FALSE){
        NKMSG(L"Can not set udevicesecurity on %s!", DEVLOAD_DRIVERS_KEY);
        return FALSE;
    }

    return bRet;
}

BOOL Init()
{
    BOOL fRet = TRUE;
    
    
    // Add Registry
	Debug(_T("Add Security Registry\r\n"));
	if (!ModifySecurityRegistry(TRUE))
		return FALSE;

    // Initialize peHlpLib
#if 0
        Debug(_T("Initialize peHlpLib\r\n"));
        if (!InitPEHlpLib())
            return FALSE;
#endif
    
    return fRet;
}
BOOL Deinit()
{
    BOOL fRet = TRUE;

	Debug(_T("Remove Security Registry\r\n"));
	if (!ModifySecurityRegistry(FALSE))
            return FALSE;

#if 0
        Debug(_T("Deinitialize peHlpLib"));
        if (!DeinitPEHlpLib())
            return FALSE;
#endif


    return fRet;
}

void Usage()
{
    Debug(TEXT(""));
    Debug(TEXT("Usage: tux -o -d BacklightSecTest"));
    Debug(TEXT(""));
}


BOOL ProcessCmdLine(LPCTSTR szCmdLine )
{
    CClParse cmdLine(szCmdLine);
    


    // Get the input chamber type
    if(cmdLine.GetOpt(_T("p"))) {
        g_bEnablePMtest = TRUE;
    }
    if(cmdLine.GetOpt(_T("?"))) {
        Debug(_T("~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~"));
        Debug(_T( "usage: s tux -o -d BacklightSecTest tux_parameters -c\"DLL_parameters\"" ) );
        Debug(_T( "tux_parameters:  please refer to s tux -?" ) );
        Debug(_T( "DLL_parameters: [-p] [-?]" ) );
        Debug(_T( "\t-p\tEnable power management tests. (default=disabled)"));
        Debug(_T( "\t-?\tThis help message" ) );
        Debug(_T("~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~"));
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
         Debug(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

         // If we are UNICODE, then tell Tux this by setting the following flag.
         #ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
         #endif

         // Get/Create our global logging object.
         g_pKato = (CKato*)KatoGetDefaultObject();

         // Initialize our global critical section.
         InitializeCriticalSection(&g_csProcess);
         Usage();
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_UNLOAD_DLL
      //
      // Sent once to the DLL immediately before it is unloaded.
      //------------------------------------------------------------------------

      case SPM_UNLOAD_DLL: {
         Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));

         // Remove Security Registry
         if (!Deinit())
             return SPR_NOT_HANDLED;

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

         // Display our Dlls command line if we have one.
         if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) {
#ifdef UNDER_CE
            Debug(TEXT("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
#else
            MessageBox(g_pShellInfo->hWnd, g_pShellInfo->szDllCmdLine,
                       TEXT("BACKLIGHTAPITEST.DLL Command Line Arguments"), MB_OK);
#endif UNDER_CE
         }

        // handle command line parsing
        if(!ProcessCmdLine(g_pShellInfo->szDllCmdLine)) {
            return SPR_FAIL;
        }

        // Add Security Registry
        if (!Init())
           return SPR_NOT_HANDLED;
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
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: BACKLIGHTAPITEST.DLL"));
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
         g_pKato->EndLevel(TEXT("END GROUP: BACKLIGHTAPITEST.DLL"));
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
         const LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
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
         const LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;
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