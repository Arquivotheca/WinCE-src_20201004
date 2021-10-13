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

#include "main.h"
#include "tuxmain.h"

/*
    this file contains the individual tests.  For each test you add to the FTE
    in drvdetector.h, add the appropriate function to this file.

    Full documentation on the input parameters can be found in
    tux.h, or on http://badmojo/tux.htm

    If you have additional questions, please email mojotool
*/

/*
    Global declarations:
    The following items exist here to enable the project header
    file to be included by multiple source files without
    causing multiple definition errors at link time
*/
CKato *g_pKato = NULL;
SPS_SHELL_INFO *g_pShellInfo = NULL;
extern "C" {
    __declspec(dllexport) CKato* GetKato(void){return g_pKato;}
}

HINSTANCE g_hInstDll = NULL;

#define ARRAYSIZE(x) (sizeof(x)/sizeof(*x))
#define DEBUG_INDENT            11

// debugging
#ifdef UNDER_CE
VOID Debug(LPCTSTR szFormat, ...)
{
    TCHAR szBuffer[1024] = MODULE_NAME;
    UINT tmpLen= 0;
    StringCchLength( szBuffer, 1024, &tmpLen );

    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsnwprintf_s(szBuffer + tmpLen, 1024 - tmpLen, ARRAYSIZE(szBuffer) - DEBUG_INDENT, szFormat, pArgs);
    va_end(pArgs);

    OutputDebugString(szBuffer);
}
#endif

/// --------------------------------------------------------------------
// Tux testproc function table
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    // note kernel driver is ALWAYS loaded in TCB
    _T("SDMemory Detection Logic"),         0,      0,       0,     NULL,
    _T( "SD Peripheral Detector"),              1,  0,      1000,  DetectSD,
    _T( "SDHC Peripheral Detector"),            1,  0,      1100,  DetectSDHC,
    _T( "MMC Peripheral Detector"),                 1,  0,      1200,  DetectMMC,
    _T( "MMCHC Peripheral Detector"),           1,  0,      1300,  DetectMMCHC,

    _T("SDIO Detection Logic"),             0,      0,       0,     NULL,
    _T( "SDIO Peripheral Detector"),            1,  0,      2000,  DetectSDIO,
    NULL,   0,  0,  0,  NULL
};

/*
    There's rarely much need to modify the remaining two functions
    (DllMain and ShellProc) unless you need to debug some very
    strange behavior, or if you are doing other customizations.
*/

BOOL WINAPI
DllMain(
    HANDLE hInstance,
    ULONG dwReason,
    LPVOID /*lpReserved*/ )
{

    switch( dwReason )
    {
        case DLL_PROCESS_ATTACH:
            Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);
            g_hInstDll = (HINSTANCE)hInstance;
            return TRUE;

        case DLL_PROCESS_DETACH:
            Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
            return TRUE;
    }
    return FALSE;
}

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

            // handle command line parsing

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

        // Message: SPM_BEGIN_GROUP
        case SPM_BEGIN_GROUP:
            Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP:DrvDetector"));

            return SPR_HANDLED;

        // Message: SPM_END_GROUP
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP:DrvDetector"));

            return SPR_HANDLED;

        // Message: SPM_BEGIN_TEST
        case SPM_BEGIN_TEST:
            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
                                _T("BEGIN TEST: %s, Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);

            return SPR_HANDLED;

        // Message: SPM_END_TEST
        case SPM_END_TEST:
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
            g_pKato->Log(LOG_EXCEPTION, _T("%s: Exception Thrown!"), MODULE_NAME);
            return SPR_HANDLED;

        default:
            Debug(_T("ShellProc received message it is not handling: 0x%X\r\n"), uMsg);
            return SPR_NOT_HANDLED;
    }
}
/*
    GetTestResult
    Checks the Kato object to see if failures, aborts, or skips were logged
        and returns the result accordingly
    Return value:
        TESTPROCAPI value of either TPR_FAIL, TPR_ABORT, TPR_SKIP, or TPR_PASS.
*/
TESTPROCAPI GetTestResult(void)
{
    // Check to see if we had any LOG_EXCEPTIONs or LOG_FAILs and, if so,
    // return TPR_FAIL
    for(int i = 0; i <= LOG_FAIL; i++)    {
        if(g_pKato->GetVerbosityCount(i))      {
            return TPR_FAIL;
        }
    }
    // Check to see if we had any LOG_ABORTs and, if so,
    // return TPR_ABORT
    for( ; i <= LOG_ABORT; i++)   {
        if(g_pKato->GetVerbosityCount(i))  {
            return TPR_ABORT;
        }
    }
    // Check to see if we had LOG_SKIPs or LOG_NOT_IMPLEMENTEDs and, if so,
    // return TPR_SKIP
    for( ; i <= LOG_NOT_IMPLEMENTED; i++)  {
        if (g_pKato->GetVerbosityCount(i))  {
            return TPR_SKIP;
        }
    }
    // If we got here, we only had LOG_PASSs, LOG_DETAILs, and LOG_COMMENTs
    // return TPR_PASS
    return TPR_PASS;
}
