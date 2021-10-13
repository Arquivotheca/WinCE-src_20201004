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
//
//  DDBVT TUX DLL
//  Copyright (c) 1996-1998, Microsoft Corporation
//
//  Module: ddbvt.cpp
//          Contains the shell processing function.
//
//  Revision History:
//      09/12/96    lblanco     Created with the TUX Wizard.
//      08/03/99    a-shende    Uncommented LogResult function
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "hqamisc.h"
#include "heaptest.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// Local types

typedef HANDLE  (WINAPI *pfnHTStartSession)(HANDLE, DWORD, DWORD, DWORD);
typedef BOOL    (WINAPI *pfnHTStopSession)(HANDLE);

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
    if(dwReason == DLL_PROCESS_ATTACH)
    {
        // Save our module instance handle
        g_hInstance = (HINSTANCE)hInstance;

        // Tell DXQA to log to Kato server, but don't overwrite Tux's settings
        DontSetKatoServer();
        UseKato(true);
    }

    return TRUE;
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

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)
{
    static HINSTANCE            s_hHeapTest = NULL;
    static pfnHTStartSession    s_HTStartSession = NULL;
    static pfnHTStopSession     s_HTStopSession = NULL;
    static HANDLE               s_hHTSession = NULL;

    switch (uMsg)
    {
    case SPM_LOAD_DLL:
        {
            // Load the Heap Test DLL
            s_hHeapTest = LoadLibrary(TEXT("\\heaptest.dll"));
            if(s_hHeapTest)
            {
                // Get the two functions we need
                s_HTStartSession = (pfnHTStartSession)GetProcAddress(
                    s_hHeapTest,
                    XTEXT("HeapTestStartSession"));
                s_HTStopSession = (pfnHTStopSession)GetProcAddress(
                    s_hHeapTest,
                    XTEXT("HeapTestStopSession"));

                // Start the background thread. Check memory every 5 seconds
                if(s_HTStartSession)
                {
                    s_hHTSession = s_HTStartSession(
                        NULL,
                        100,
                        THREAD_PRIORITY_NORMAL,
                        HTF_CALL_HEAPVALIDATE | HTF_BREAK_ON_CORRUPTION);
                }
            }
            SPS_LOAD_DLL *pspsLoadDllInfo = (SPS_LOAD_DLL*)spParam;
            pspsLoadDllInfo->fUnicode = true;
        }
        break;

    case SPM_UNLOAD_DLL:
        if(s_hHTSession && s_HTStopSession)
        {
            s_HTStopSession(s_hHTSession);
            FreeLibrary(s_hHeapTest);

            // Clear our variables
            s_hHeapTest = NULL;
            s_HTStartSession = NULL;
            s_HTStopSession = NULL;
            s_hHTSession = NULL;
        }
        break;

    case SPM_SHELL_INFO:
        g_pspsShellInfo = (LPSPS_SHELL_INFO)spParam;
        ProcessCommandLine(g_pspsShellInfo->szDllCmdLine);
        break;

    case SPM_REGISTER:
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
        return SPR_HANDLED | SPF_UNICODE;
#else
        return SPR_HANDLED;
#endif

    case SPM_START_SCRIPT:
        // Seed the random number generator - TUX handles this for us.
        // srand(GetTickCount());
        break;

    case SPM_STOP_SCRIPT:
        break;

    case SPM_BEGIN_GROUP:
        DebugBeginLevel(0, TEXT("BEGIN GROUP: DDInts.DLL"));
        break;

    case SPM_END_GROUP:
        FinishDirectDraw();
        DebugEndLevel(TEXT("END GROUP: DDInts.DLL"));
        break;

    case SPM_BEGIN_TEST:
        DebugBeginLevel(
            ((LPSPS_BEGIN_TEST)spParam)->lpFTE->dwUniqueID,
            TEXT("BEGIN TEST: \"%s\""),
            ((LPSPS_BEGIN_TEST)spParam)->lpFTE->lpDescription);
        break;

    case SPM_END_TEST:
        DebugEndLevel(TEXT("END TEST: \"%s\""),
            ((LPSPS_END_TEST)spParam)->lpFTE->lpDescription);
        break;

    case SPM_EXCEPTION:
        Debug(
            LOG_EXCEPTION,
            FAILURE_HEADER TEXT("An exception has occurred during the")
            TEXT(" execution of this test"));
        break;

    default:
        return SPR_NOT_HANDLED;
    }

    return SPR_HANDLED;
}

void ProcessCommandLine(LPCTSTR tszCommandLine)
{
    // The only parameter we currently accept is /DisplayIndex,
    // and it should be used as "/DisplayIndex <int>"
    const TCHAR * tszDisplayIndex = TEXT("/DisplayIndex");

    if (NULL != tszCommandLine)
    {
        if (_tcsstr(tszCommandLine, tszDisplayIndex))
        {
            if (!swscanf_s(tszCommandLine + _tcslen(tszDisplayIndex), TEXT(" %i"), &g_iDisplayIndex))
            {
                g_iDisplayIndex = -1;
            }
            if (g_iDisplayIndex < -1 || g_iDisplayIndex > 100)
            {
                g_iDisplayIndex = -1;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// LogResult
//  Logs an appropriate error (or success) message, depending on the returned
//  and expected error codes. Also decides whether the test as a whole
//  succeeded, failed, or should be skipped as a result.
//
// Parameters:
//  hrActual        The code returned by the operation whose result we want to
//                  log.
//  sz              Description of the operation.
//  hrExpected      The code we expected to get.
//
// Return value:
//  TPR_PASS, TPR_FAIL or TPR_SKIP, indicating the appropriate action to take.

int LogResult(HRESULT hrActual, const TCHAR *sz, HRESULT hrExpected)
{
    int nTPR;

    if(hrActual == hrExpected)
    {
        Debug(
            LOG_PASS,
            TEXT("PASS: %s; ret = %s - %s"),
            sz,
            GetErrorName(hrActual),
            GetErrorDescription(hrActual));
        nTPR = TPR_PASS;
    }
    else
    {
        if (hrActual == DDERR_NOALPHAHW || hrActual == DDERR_NOBLTHW
            || hrActual == DDERR_NOCOLORCONVHW
            || hrActual == DDERR_NOCOLORKEYHW
            || hrActual == DDERR_NOFLIPHW
            || hrActual == DDERR_NOOVERLAYHW
            || hrActual == DDERR_NOPALETTEHW || hrActual == DDERR_NORASTEROPHW
            || hrActual == DDERR_NOSTRETCHHW
            || hrActual == DDERR_NOVSYNCHW
            || hrActual == DDERR_NOZOVERLAYHW
            || hrActual == DDERR_INVALIDCAPS)
        {
            Debug(
                LOG_SKIP,
                TEXT("%s; ret = %s - %s"),
                sz,
                GetErrorName(hrActual),
                GetErrorDescription(hrActual));
            nTPR = TPR_SKIP;
        }
        else
        {
            Debug(
                LOG_FAIL,
                FAILURE_HEADER TEXT("%s; ret = %s - %s"),
                sz,
                GetErrorName(hrActual),
                GetErrorDescription(hrActual));
            nTPR = TPR_FAIL;
        }
    }

    return nTPR;
}

////////////////////////////////////////////////////////////////////////////////
