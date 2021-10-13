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
#include "perf_regEx.h"
#include <clparse.h>
#include <perfhlp.h>

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;
TCHAR  g_szPerfLogOutputFileName[MAX_PATH];
// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

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
            Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);
            return TRUE;

        case DLL_PROCESS_DETACH:
            Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
            return TRUE;
    }
    return FALSE;
}

#ifdef __cplusplus
} // end extern "C"
#endif

// --------------------------------------------------------------------
void LOG(LPCTSTR szFmt, ...)
// --------------------------------------------------------------------
{
#ifdef DEBUG
    va_list va;

    if(g_pKato)
    {
        va_start(va, szFmt);
        g_pKato->LogV( LOG_DETAIL, szFmt, va);
        va_end(va);
    }
#endif
}



// --------------------------------------------------------------------
void TRACE(LPCTSTR szFmt, ...)
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
// Prints out usage
// --------------------------------------------------------------------
void Usage()
{
    Debug(_T("Usage : %s -c\"[-interval <pool interval val>|-cachedkeys <noofkeys>]\""), MODULE_NAME);
    Debug(_T("      -interval <pool interval val> : interval value, e.g. 50, 100"));
    Debug(_T("      -cachedkeys <no of keys> : No of Keys, e.g. 10,20"));
}



// --------------------------------------------------------------------
// Initialize perf logging functionality
// --------------------------------------------------------------------
BOOL InitPerfLog()
{
    BOOL fRet = FALSE;
    if (!CPerfScenario::Initialize(MODULE_NAME, true))
    {
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}
BOOL ParseCommandLine()
{
    CClParse* pCmdLine = NULL;
    BOOL fRet = FALSE;

    // Read in the commandline
    if (g_pShellInfo->szDllCmdLine)
    {
        pCmdLine = new CClParse(g_pShellInfo->szDllCmdLine);
        if (!pCmdLine)
        {
            ERRFAIL("ParseCommandLine : Out of memory!");
            goto done;
        }
    }
    else
    {
        ERRFAIL("ParseCommandLine : error, commandline is NULL");
        goto done;
    }

    if (!pCmdLine->GetOptVal(TEXT("interval"), &g_dwPoolIntervalMs))
    {
        // Set default values
        g_dwPoolIntervalMs = PERF_DEF_CPU_POOL_INTERVAL_MS;
    }

    g_pKato->Log(LOG_DETAIL, TEXT("pool interval: %d ms"), g_dwPoolIntervalMs);
    fRet = TRUE;
done:
    // Clean Up
    CHECK_DELETE(pCmdLine);

    return fRet;
}

// --------------------------------------------------------------------
// Initializes global test parameters
// --------------------------------------------------------------------
BOOL Initialize()
{
    BOOL fRet = FALSE;

    //construct the logname
    VERIFY(SUCCEEDED(StringCchCopy(g_szPerfLogOutputFileName, MAX_PATH, DEFAULT_OUTPUT_LOG_FILE_NAME)));

    // Parse the command line
    if (!(ParseCommandLine() && InitPerfLog() ))
    {
        goto done;
    }

    fRet = TRUE;
    done:
        return fRet;

}


// --------------------------------------------------------------------
// Cleanup and housekeeping
// --------------------------------------------------------------------
void Shutdown()
{
    // Dump the log file
    CPerfScenario::DumpLogToFile(g_szPerfLogOutputFileName);
    return;
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

        // Message: SPM_LOAD_DLL
        //
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
        //
        case SPM_UNLOAD_DLL:
            Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));
            Shutdown();
            return SPR_HANDLED;

        // Message: SPM_SHELL_INFO
        //
        case SPM_SHELL_INFO:
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));

            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

            // Print out the usage
            Usage();
            // Break if the initialization fails
                if (!Initialize())
                {
                    return SPR_FAIL;
                }

            // handle command line parsing

        return SPR_HANDLED;

        // Message: SPM_REGISTER
        //
        case SPM_REGISTER:
            Debug(_T("ShellProc(SPM_REGISTER, ...) called\r\n"));

            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif

        // Message: SPM_START_SCRIPT
        //
        case SPM_START_SCRIPT:
            Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));

            return SPR_HANDLED;

        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:

            return SPR_HANDLED;

        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
            Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: FSDTEST.DLL"));

            return SPR_HANDLED;

        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP: TUXDEMO.DLL"));

            return SPR_HANDLED;

        // Message: SPM_BEGIN_TEST
        //
        case SPM_BEGIN_TEST:
            Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called\r\n"));

            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
                                _T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);

            return SPR_HANDLED;

        // Message: SPM_END_TEST
        //
        case SPM_END_TEST:
            Debug(_T("ShellProc(SPM_END_TEST, ...) called\r\n"));

            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(_T("END TEST: \"%s\", %s, Time=%u.%03u"),
                              pET->lpFTE->lpDescription,
                              pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                              pET->dwResult == TPR_PASS ? _T("PASSED") :
                              pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                              pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

            return SPR_HANDLED;

        // Message: SPM_EXCEPTION
        //
        case SPM_EXCEPTION:
            Debug(_T("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;

        default:
            Debug(_T("ShellProc received bad message: 0x%X\r\n"), uMsg);

            return SPR_NOT_HANDLED;
    }
}
