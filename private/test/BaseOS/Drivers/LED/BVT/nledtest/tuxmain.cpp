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
#include "ceddk.h"
#include "regmani.h"        // for CRegMani
#include <assert.h>
#include <clparse.h>        // For Command Parser
#include "globals.h"        //For Memory, CPU macros

#ifndef UNDER_CE
#include <stdio.h>
#endif



// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

// Global variable for MessageBox Input Message
TCHAR g_szInputMsg[100] = TEXT("Input Required");

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

BOOL g_fInteractive    = FALSE;
BOOL g_fVibrator       = FALSE;
UINT g_nMinNumOfNLEDs  = DEFAULT_MIN_NUM_OF_NLEDS;
BOOL g_fServiceTests   = FALSE;
BOOL g_fGeneralTests   = FALSE;
BOOL g_fPQDTests       = FALSE;
BOOL g_fAutomaticTests = FALSE;

//Defining the default values for Max Memory, CPU Utilization and Time Delay
UINT g_nMaxMemoryUtilization = DEFAULT_MAX_MEMORY_UTILIZATION;
UINT g_nMaxCpuUtilization    = DEFAULT_MAX_CPU_UTILIZATION;
UINT g_nMaxTimeDelay         = DEFAULT_MAX_TIME_DELAY;

//Declaring the global variables for storing the Event and Thread handles
UINT g_nThreadCount                      = 0;
HANDLE g_hNledDeviceHandle               = NULL;
BOOL g_fMULTIPLENLEDSETFAILED            = FALSE;
HANDLE g_hThreadHandle[MAX_THREAD_COUNT] = {NULL};

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(dwReason);
    UNREFERENCED_PARAMETER(lpReserved);
   return TRUE;
}
#endif

//******************************************************************************

VOID Debug(LPCTSTR szFormat, ...) {
   TCHAR szBuffer[1024] = TEXT("NLEDTEST: ");

   va_list pArgs;
   va_start(pArgs, szFormat);
   StringCchVPrintf(szBuffer + 9, _countof(szBuffer) - 11, szFormat, pArgs);
   va_end(pArgs);

   wcscat_s(szBuffer, TEXT("\r\n"));

   OutputDebugString(szBuffer);
}

// --------------------------------------------------------------------
void LOG(LPWSTR szFmt, ...)
// --------------------------------------------------------------------
{
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFmt);
        g_pKato->LogV( LOG_DETAIL, szFmt, va);
        va_end(va);
    }
}

// --------------------------------------------------------------------
// This should be remove by M4 - temporary add registry to enable device manager ACL.
// --------------------------------------------------------------------
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
    // Add Registry
        Debug(_T("Add Security Registry\r\n"));
        if (!ModifySecurityRegistry(TRUE))
            return FALSE;
        return TRUE;
}

BOOL Deinit()
{
        // Remove the Registry changes
        Debug(_T("Remove Security Registry changes\r\n"));
        if (!ModifySecurityRegistry(FALSE))
            return FALSE;

    return TRUE;
}

void Usage()
{
    Debug(TEXT(""));
    Debug(TEXT("NLEDTEST: Usage: tux -o -d nledtest"));
    Debug(TEXT(""));
}

//******************************************************************************
BOOL ProcessCommandLine(LPCTSTR szDLLCmdLine)
{
    CClParse cmdLine((TCHAR*)szDLLCmdLine);
    DWORD minnumofled = 0;
    DWORD dwTempVal = 0;

    if(cmdLine.GetOpt(_T("?")))
    {
        Debug( TEXT( "usage:  s tux -o -d nledtest tux_parameters -c \"dll_parameters\"\n" ) );
        Debug( TEXT( "Tux parameters:  please refer to s tux -?\n" ) );
        Debug( TEXT( "DLL parameters: [-i] [-n #] [-v] [-ser] [-gen] [-auto] [-pqd] [-cpt #] [-mt #] [-td #][-?]\n" ) );
        Debug( TEXT( "\t-i\tTurn on interactive mode. (default=FALSE)"));
        Debug( TEXT( "\t-n\tMinimum number of NLEDs required (default=0)"));
        Debug( TEXT( "\t-v\tVibrator required. (default=no)"));
        Debug( TEXT( "\t-ser\tTurn on Nled Service tests"));
        Debug( TEXT( "\t-gen\tTurn on Nled general tests"));
        Debug( TEXT( "\t-auto\tTurn on Nled Automatic Tests"));
        Debug( TEXT( "\t-pqd\tTurn on Nled PQD tests"));
        Debug( TEXT( "\t-cpt\tMaximum allowed CPU utilization value"));
        Debug( TEXT( "\t-mt\tMaximum allowed Memory utilization value"));
        Debug( TEXT( "\t-td\tMaximum allowed time delay value for Nled Service to trigger a Event"));
        Debug( TEXT( "\t-?\tThis help message\n" ) );
        return FALSE;
    }

    g_fInteractive   = cmdLine.GetOpt(_T("i"));
    g_nMinNumOfNLEDs = (cmdLine.GetOptVal(_T("n"), &minnumofled))? (UINT) minnumofled:DEFAULT_MIN_NUM_OF_NLEDS;
    g_fVibrator      = cmdLine.GetOpt(_T("v"));
    g_fServiceTests  = cmdLine.GetOpt(_T("ser"));
    g_fGeneralTests  = cmdLine.GetOpt(_T("gen"));
    g_fAutomaticTests= cmdLine.GetOpt(_T("auto"));
    g_fPQDTests      = cmdLine.GetOpt(_T("pqd"));
    g_nMaxCpuUtilization = (cmdLine.GetOptVal(_T("cpt"), &dwTempVal))? (UINT) dwTempVal:DEFAULT_MAX_CPU_UTILIZATION;
    g_nMaxMemoryUtilization = (cmdLine.GetOptVal(_T("mt"), &dwTempVal))? (UINT) dwTempVal:DEFAULT_MAX_MEMORY_UTILIZATION;
    g_nMaxTimeDelay = (cmdLine.GetOptVal(_T("td"), &dwTempVal))? (UINT) dwTempVal:DEFAULT_MAX_TIME_DELAY;

    //If none of the options is specified, by default all the tests are enabled
    //except the Interactive tests
    if(!g_fServiceTests   &&
       !g_fGeneralTests   &&
       !g_fAutomaticTests &&
       !g_fPQDTests)
    {
        g_fServiceTests   = TRUE;
        g_fGeneralTests   = TRUE;
        g_fPQDTests       = TRUE;
        g_fAutomaticTests = TRUE;
    }

    return TRUE;
} // ProcessCommandLine

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
        // Remove Security Registry
        if (!Deinit())
           return SPR_NOT_HANDLED;
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

         // Display our Dlls command line if we have one.
         if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) {
#ifdef UNDER_CE
            Debug(TEXT("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
#else
            MessageBox(g_pShellInfo->hWnd, g_pShellInfo->szDllCmdLine,
                       TEXT("NLEDTEST.DLL Command Line Arguments"), MB_OK);
#endif UNDER_CE
         }

        if( !ProcessCommandLine( (TCHAR*)g_pShellInfo->szDllCmdLine ) )
            return SPR_FAIL;

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
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: NLEDTEST.DLL"));
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
         g_pKato->EndLevel(TEXT("END GROUP: NLEDTEST.DLL"));
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