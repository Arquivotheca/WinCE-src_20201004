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

#undef __FILE_NAME__
#define __FILE_NAME__   _T("TUXMAIN.CPP")

#define Debug LOG

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

extern DWORD g_Suspend ;

#define cmdAllowSuspend _T("s")
#define BASE 100


// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
// Tux testproc function table
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
  _T("USB Function Bus Ioctl tests"),  0,  0,  0,  NULL,
        _T( "Unload/Reload USB Function bus driver"),  1,  0,    BASE +  1,  TestUnloadReloadUfnBusDrv,
        _T( "Enumerate USB Function client driver"),  1, 0,    BASE +  2,  TestEnumClient,
        _T( "Get/Set USB Function current client driver"),  1, 0,    BASE +  3,  TestGetSetCurrentClient,
        _T( "Get/Set USB Function default client driver"),  1, 0,    BASE +  4,  TestGetSetDefaultClient,
        _T( "Additional IOCTL Test"),  1, 0, BASE + 5,  TestIoctlAdditional,
        _T( "Enumerate All Clients"),  1, 0,    BASE +  6,  TestEnumChangeClient,
  _T("SuspendResume Tests"), 0, 0, 0, NULL ,
        _T( "Test Current and Default Client settings post SuspendResume"), 1, 0, BASE+100,    TestCurrentAndDefaultClientSuspendResume,
  NULL,                                           0,  0,                  0,          NULL
};
// --------------------------------------------------------------------

// --------------------------------------------------------------------
BOOL WINAPI
DllMain(
    HANDLE hInstance,
    ULONG dwReason,
    LPVOID lpReserved )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);

    switch( dwReason )
      {
      case DLL_PROCESS_ATTACH:
            Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), __FILE_NAME__);
            return TRUE;

      case DLL_PROCESS_DETACH:
            Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), __FILE_NAME__);
            return TRUE;

      }
    return FALSE;
}

#ifdef __cplusplus
}
#endif
// end extern "C"

////////////////////////////////////////////////////////////////////////////////
// InitializeCmdLine
// Process Command Line.
//
// Parameters:
//  pszCmdLine         Command Line String .
//
// Return value:
// Void

void InitializeCmdLine(LPCTSTR pszCmdLine)
{
	UNREFERENCED_PARAMETER(pszCmdLine);
    CClParse cmdLine(g_pShellInfo->szDllCmdLine);

    if(cmdLine.GetOpt(cmdAllowSuspend))
    {
        Debug(TEXT("Suspend enabled for USB FN BVT tests."));
        g_Suspend= SUSPEND_RESUME_ENABLED;
    }
    else
    {
        Debug(TEXT("Suspend disabled for USB FN BVT test."));
        g_Suspend = SUSPEND_RESUME_DISABLED;

    }

}
// --------------------------------------------------------------------
SHELLPROCAPI
ShellProc(
    UINT uMsg,
    SPPARAM spParam )
// --------------------------------------------------------------------
{
    LPSPS_BEGIN_TEST pBT = {0};
    LPSPS_END_TEST pET = {0};

    switch (uMsg) {

        // --------------------------------------------------------------------
        // Message: SPM_LOAD_DLL
        //
        case SPM_LOAD_DLL:
            Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called"));

            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // turn on kato debugging
            KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();

            // Initialize our global critical section.
            InitializeCriticalSection(&g_csProcess);

            //setup test registry
            SetupUfnTestReg();
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL:
            Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));

            // This is a good place to delete our global critical section.
            DeleteCriticalSection(&g_csProcess);

	     //Clean up the USB FN BVT Registry
           CleanUfnTest();

         return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_SHELL_INFO
        //
        case SPM_SHELL_INFO:
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called"));

            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                InitializeCmdLine(g_pShellInfo->szDllCmdLine);
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL,
                    _T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
            }

        return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_REGISTER
        //
        case SPM_REGISTER:
            Debug(_T("ShellProc(SPM_REGISTER, ...) called"));

            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif

        // --------------------------------------------------------------------
        // Message: SPM_START_SCRIPT
        //
        case SPM_START_SCRIPT:
            Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called"));
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:

            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
            Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called"));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: USBFnBvt.DLL"));

            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called"));
            g_pKato->EndLevel(_T("END GROUP: USBFnBvt.DLL"));

            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_TEST
        //
        case SPM_BEGIN_TEST:
            Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called"));

            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
                                _T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);

            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_TEST
        //
        case SPM_END_TEST:
            Debug(_T("ShellProc(SPM_END_TEST, ...) called"));

            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(_T("END TEST: \"%s\", %s, Time=%u.%03u"),
                              pET->lpFTE->lpDescription,
                              pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                              pET->dwResult == TPR_PASS ? _T("PASSED") :
                              pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                              pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_EXCEPTION
        //
        case SPM_EXCEPTION:
            Debug(_T("ShellProc(SPM_EXCEPTION, ...) called"));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;
        //------------------------------------------------------------------------
      // Message: SPM_SETUP_GROUP
      //
      // Sent to the DLL whenever a new group of tests is about to run.
      // A 'group' is defined as a series of tests that are listed after a
      // FUNCTION_TABLE_ENTRY with lpTestProc = NULL.
      //
      // The DLL may return SPR_FAIL to cause all tests in this group to be
      // skipped.
      //------------------------------------------------------------------------

      case SPM_SETUP_GROUP: {
         //LPSPS_SETUP_GROUP pSG = (LPSPS_SETUP_GROUP) spParam;
         Debug(TEXT("ShellProc(SPM_SETUP_GROUP, ...) called"));

         // SPR_FAIL is the only return code checked for.
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_TEARDOWN_GROUP
      //
      // Sent to the DLL whenever a group of tests has finished running. See
      // the handler for SPM_SETUP_GROUP for more information about this grouping.
      //
      // The DLL's return value is ignored.
      //------------------------------------------------------------------------

      case SPM_TEARDOWN_GROUP: {
         LPSPS_TEARDOWN_GROUP pTG = (LPSPS_TEARDOWN_GROUP) spParam;
         Debug(TEXT("ShellProc(SPM_TEARDOWN_GROUP, ...) called"));
          Debug(_T("Results:"));
          Debug(_T("Passed:  %u"), pTG->dwGroupTestsPassed);
          Debug(_T("Failed:  %u"), pTG->dwGroupTestsFailed);
          Debug(_T("Skipped: %u"), pTG->dwGroupTestsSkipped);
          Debug(_T("Aborted: %u"), pTG->dwGroupTestsAborted);
          Debug(_T("Time:    %u"), pTG->dwGroupExecutionTime);

         // The return value is ignored.
         return SPR_HANDLED;
      }

        default:
            Debug(_T("ShellProc received bad message: 0x%X"), uMsg);
            ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
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
void FAILLOG(LPWSTR szFmt, ...)
// --------------------------------------------------------------------
{
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFmt);
        g_pKato->LogV( LOG_FAIL, szFmt, va);
        va_end(va);
    }
}

