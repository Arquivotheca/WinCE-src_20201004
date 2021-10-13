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

////////////////////////////////////////////////////////////////////////////////
/**********************************  Headers  *********************************/

#include "tuxmain.h"
#include "pwrioctls.h"

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;


// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;


// Tux testproc function table
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    _T("Power test"),                      0,  0,     0,  NULL,
    _T("TST_GetPwrManageableDevices"),               1,  0,  1001,  TST_GetPwrManageableDevices,
    _T("TST_GetDevPwrLevels"),                  1,  0,  1002,  TST_GetDevPwrLevels,
    _T("TST_SetDevPwrLevels"),                  1,  0,  1003,  TST_SetDevPwrLevels,
    _T("TST_PwrLevelsForUserGivenDev"), 1,  0,  1004,  TST_PwrLevelsForUserGivenDev,
    NULL,   0,  0,  0,  NULL
};

////////////////////////////////////////////////////////////////////////////////
/****************************************************************************/

/*
There's rarely much need to modify the remaining two functions
(DllMain and ShellProc) unless you need to debug some very
strange behavior, or if you are doing other customizations.
*/

BOOL WINAPI
DllMain(
        HANDLE hInstance,
        ULONG dwReason,
        LPVOID lpReserved )
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);

    switch( dwReason )
    {
    case DLL_PROCESS_ATTACH:
        Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);
        return TRUE;

    case DLL_PROCESS_DETACH:
        Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
        return TRUE;
    }
    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////
// ShellProc
//  Processes messages from the TUX shell.
//
// Parameters:
//  uMsg            Message code.
//  spParam         Additional message-dependent data.
//
// Return value:
//  Depends on the message.
////////////////////////////////////////////////////////////////////////////////

SHELLPROCAPI
ShellProc(
          UINT uMsg,
          SPPARAM spParam )
{
    LPSPS_BEGIN_TEST pBT = {0};
    LPSPS_END_TEST pET = {0};

    switch (uMsg) {

        // Message: SPM_LOAD_DLL
        case SPM_LOAD_DLL:
            Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));

            // If we are UNICODE, then tell Tux this by setting the following flag.
#ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif

            // turn on kato debugging
            KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();

            return SPR_HANDLED;

            // Message: SPM_UNLOAD_DLL
        case SPM_UNLOAD_DLL:
            Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));
            return SPR_HANDLED;

            // Message: SPM_SHELL_INFO
        case SPM_SHELL_INFO:
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));

            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

            return SPR_HANDLED;

            // Message: SPM_REGISTER
        case SPM_REGISTER:
            Debug(_T("ShellProc(SPM_REGISTER, ...) called\r\n"));
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
#else
            return SPR_HANDLED;
#endif

            // Message: SPM_START_SCRIPT
        case SPM_START_SCRIPT:
            Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));
            return SPR_HANDLED;

            // Message: SPM_STOP_SCRIPT
        case SPM_STOP_SCRIPT:
            return SPR_HANDLED;

            // Message: SPM_BEGIN_DLL
        case SPM_BEGIN_DLL: {
            Debug(_T("ShellProc(SPM_BEGIN_DLL, ...) called\r\n"));
            g_pKato->BeginLevel(0, TEXT("BEGIN DLL: %s"), MODULE_NAME);
            return SPR_HANDLED;
                            }

                            // Message: SPM_END_DLL
        case SPM_END_DLL: {
            Debug(_T("ShellProc(SPM_END_DLL, ...) called\r\n"));
            g_pKato->EndLevel(TEXT("END DLL: %s"), MODULE_NAME);
            return SPR_HANDLED;
                          }

                          // Message: SPM_BEGIN_TEST
        case SPM_BEGIN_TEST:
            Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called\r\n"));

            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
                _T("BEGIN TEST: %s, Threads=%u, Seed=%u"),
                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                pBT->dwRandomSeed);
            return SPR_HANDLED;

            // Message: SPM_END_TEST
        case SPM_END_TEST:
            Debug(_T("ShellProc(SPM_END_TEST, ...) called\r\n"));

            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(_T("END TEST: %s, %s, Time=%u.%03u"),
                pET->lpFTE->lpDescription,
                pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                pET->dwResult == TPR_PASS ? _T("PASSED") :
                pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
            return SPR_HANDLED;

            // Message: SPM_EXCEPTION
        case SPM_EXCEPTION:
            Debug(_T("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;

            // Message: SPM_SETUP_GROUP
        case SPM_SETUP_GROUP: {
            Debug(TEXT("ShellProc(SPM_SETUP_GROUP, ...) called"));
            return SPR_HANDLED;
                              }

                              // Message: SPM_TEARDOWN_GROUP
        case SPM_TEARDOWN_GROUP: {
            Debug(TEXT("ShellProc(SPM_TEARDOWN_GROUP, ...) called"));
            return SPR_HANDLED;
                                 }

        default:
            Debug(_T("ShellProc received bad message: 0x%X\r\n"), uMsg);
            ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
}
