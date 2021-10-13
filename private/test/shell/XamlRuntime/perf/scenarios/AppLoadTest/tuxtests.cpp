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
//  TestTux TUX DLL
//
//  Module: TestTuxTest.cpp
//          Contains the shell processing function.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "globals.h"
#include "ft.h"

//******************************************************************************
//***** Global Variables
//******************************************************************************

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
GLOBAL SPS_SHELL_INFO *g_pShellInfo;

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
    g_hInstance = (HINSTANCE)hInstance;
    
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// Debug
//  Printf-like wrapping around OutputDebugString.
//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...             Additional arguments.
//
// Return value:
//  None.

void Debug(LPCTSTR szFormat, ...)
{
    static  TCHAR   szHeader[] = TEXT("TestTux: ");
    TCHAR   szBuffer[1024];

    va_list pArgs;
    va_start(pArgs, szFormat);
    wcscpy_s(szBuffer, szHeader);
    StringCchVPrintf(
        szBuffer + countof(szHeader) - 1,
        1024 - countof(szHeader) + 1,
        szFormat,
        pArgs);
    va_end(pArgs);

    wcscat_s(szBuffer, 1024, TEXT("\r\n"));

    OutputDebugString(szBuffer);
}

BOOL ParseCmdLine(LPCTSTR szDllCmdLine)
{
    WCHAR   szSeperators[]      = TEXT(" ,;");
    const   WCHAR*  szToken;
    WCHAR*  pszNextToken = NULL;
    WCHAR   szCmdLine[256];
    WCHAR   szTempPath[MAX_PATH];
    bool bNewParam              = true;
    bool bProcessNameSet        = false;
    bool bPathSet               = false;
    HRESULT hr                  = E_FAIL;

    //initialize wchar strings to 0;
    *g_lpszParams = 0;
    *g_lpszExeName = 0;

    if ( !szDllCmdLine || !*szDllCmdLine )
        return FALSE;

    // Validate length
    if (_tcslen(szDllCmdLine) > 255)
        return FALSE;

    StringCchPrintfW( szCmdLine, 255, L"%s", szDllCmdLine );

    szToken = wcstok_s( szCmdLine, szSeperators, &pszNextToken );

    while ( szToken )
    {
        if( wcsncmp( szToken, L"processname=", 12 ) == 0 )
        {
            // set up exe name
            CHK_CMDLN_HR(StringCchPrintfW(g_lpszExeName, MAX_PATH, L"%s.exe", szToken + 12), L"StrCchPrintfW - processname and .exe extension");

            // set up ceperf session string
            CHK_CMDLN_HR(StringCchPrintfW(g_lpszCePerfSessionName, STR_SIZE, L"%s%s%s", XR_PERF_SESSION_HEADER, szToken + 12, CEPERF_SESSION_FOOTER), L"StrCchPrintfW - CePerf session header, processname and footer");

            // set up perfscenario Session Name and Namespace
            CHK_CMDLN_HR(StringCchPrintfW(g_lpszPerfScenarioSessionName, STR_SIZE, L"%s%s%s", XR_PERF_SESSION_HEADER, szToken + 12, PERFSCENARIO_SESSION_FOOTER), L"StrCchPrintfW - (SessionName) PerfScenario session header, processname and footer");
            CHK_CMDLN_HR(StringCchPrintfW(g_lpszPerfScenarioNamespace, STR_SIZE, L"%s%s %s", XR_PERF_SESSION_HEADER, szToken + 12, FIRSTFRAME), L"StringCchPrintfW - (NameSpace) PerfScenario session header, processname and first frame");

            CHK_CMDLN_HR(StringCchCopy(g_lpszProcessName, STR_SIZE, szToken + 12), L"StrCchCopy - arg Processname");

            bProcessNameSet = true;
        }
        else if( wcsncmp( szToken, L"apppath=", 8 ) == 0 )
        {
            CHK_CMDLN_HR(StringCchCopy(szTempPath, MAX_PATH, szToken + 8), L"StrCchCopy - Arg application path");
            bPathSet = true;
        }
        else if (wcsncmp( szToken, L"waittime=", 9 ) == 0 )
        {
            g_SleepSeconds =  _wtoi( szToken + 9);
        }
        else if (wcsncmp( szToken, L"param=", 6) == 0 )
        {
            CHK_CMDLN_HR(StringCchCat(g_lpszParams, STR_SIZE, szToken + 6), L"StrCchCat - arg param");
            CHK_CMDLN_HR(StringCchCat(g_lpszParams, STR_SIZE, L" "), L"StringCchCat - space on arg param");
        }
        else if (wcsncmp( szToken, L"closewindow", 11) == 0)
        {
            g_bCloseWindow = true;
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

    if(bProcessNameSet && bPathSet)
    {
        CHK_CMDLN_HR(StringCchCat(szTempPath, MAX_PATH, g_lpszExeName), L"StringCchCat");
        CHK_CMDLN_HR(StringCchCopy(g_lpszExeName, MAX_PATH, szTempPath), "StrCchCopy - arg apppath");
    }

    if(!bProcessNameSet)
    {
        g_pKato->Log(LOG_EXCEPTION, L"No Process Name was set (use -c \"processname=...\")!");
        return FALSE;
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
    LPSPS_BEGIN_TEST    pBT;
    LPSPS_END_TEST      pET;

    switch (uMsg)
    {
    case SPM_LOAD_DLL:
        // Sent once to the DLL immediately after it is loaded. The spParam
        // parameter will contain a pointer to a SPS_LOAD_DLL structure. The
        // DLL should set the fUnicode member of this structure to TRUE if the
        // DLL is built with the UNICODE flag set. If you set this flag, Tux
        // will ensure that all strings passed to your DLL will be in UNICODE
        // format, and all strings within your function table will be processed
        // by Tux as UNICODE. The DLL may return SPR_FAIL to prevent the DLL
        // from continuing to load.
        Debug(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

        // If we are UNICODE, then tell Tux this by setting the following flag.
#ifdef UNICODE
        ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif // UNICODE
        g_pKato = (CKato*)KatoGetDefaultObject();

        // Initalize COM
        CoInitializeEx(NULL,COINIT_MULTITHREADED);
                
        break;

    case SPM_UNLOAD_DLL:
        // Sent once to the DLL immediately before it is unloaded.
        Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));

        // Uninitialize COM
        CoUninitialize();
    
        break;

    case SPM_STABILITY_INIT:
    
        Debug(TEXT("ShellProc(SPM_STABILITY_INIT, ...) called"));

        break;

    case SPM_SHELL_INFO:
        // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
        // some useful information about its parent shell and environment. The
        // spParam parameter will contain a pointer to a SPS_SHELL_INFO
        // structure. The pointer to the structure may be stored for later use
        // as it will remain valid for the life of this Tux Dll. The DLL may
        // return SPR_FAIL to prevent the DLL from continuing to load.
        Debug(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

        // Store a pointer to our shell info for later use.
        g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

        // Parse -c tux command line passed in by user.
        if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) {
            if( ParseCmdLine( g_pShellInfo->szDllCmdLine ) == FALSE )
            {
                return( SPR_FAIL );
            }
        }

        break;

    case SPM_REGISTER:
        // This is the only ShellProc() message that a DLL is required to
        // handle (except for SPM_LOAD_DLL if you are UNICODE). This message is
        // sent once to the DLL immediately after the SPM_SHELL_INFO message to
        // query the DLL for its function table. The spParam will contain a
        // pointer to a SPS_REGISTER structure. The DLL should store its
        // function table in the lpFunctionTable member of the SPS_REGISTER
        // structure. The DLL may return SPR_FAIL to prevent the DLL from
        // continuing to load.
        Debug(TEXT("ShellProc(SPM_REGISTER, ...) called"));
        ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
        return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
        return SPR_HANDLED;
#endif // UNICODE

    case SPM_START_SCRIPT:
        // Sent to the DLL immediately before a script is started. It is sent
        // to all Tux DLLs, including loaded Tux DLLs that are not in the
        // script. All DLLs will receive this message before the first
        // TestProc() in the script is called.
        Debug(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
        break;

    case SPM_STOP_SCRIPT:
        // Sent to the DLL when the script has stopped. This message is sent
        // when the script reaches its end, or because the user pressed
        // stopped prior to the end of the script. This message is sent to
        // all Tux DLLs, including loaded Tux DLLs that are not in the script.
        Debug(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
        break;

    case SPM_BEGIN_GROUP:
        // Sent to the DLL before a group of tests from that DLL is about to
        // be executed. This gives the DLL a time to initialize or allocate
        // data for the tests to follow. Only the DLL that is next to run
        // receives this message. The prior DLL, if any, will first receive
        // a SPM_END_GROUP message. For global initialization and
        // de-initialization, the DLL should probably use SPM_START_SCRIPT
        // and SPM_STOP_SCRIPT, or even SPM_LOAD_DLL and SPM_UNLOAD_DLL.
        Debug(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
        g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: Xamlruntime Application Load"));
        
        break;

    case SPM_END_GROUP:
        // Sent to the DLL after a group of tests from that DLL has completed
        // running. This gives the DLL a time to cleanup after it has been
        // run. This message does not mean that the DLL will not be called
        // again; it just means that the next test to run belongs to a
        // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
        // to track when it is active and when it is not active.
        Debug(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
        

        g_pKato->EndLevel(TEXT("END GROUP: Xamlruntime Application Load"));
        break;

    case SPM_BEGIN_TEST:
        // Sent to the DLL immediately before a test executes. This gives
        // the DLL a chance to perform any common action that occurs at the
        // beginning of each test, such as entering a new logging level.
        // The spParam parameter will contain a pointer to a SPS_BEGIN_TEST
        // structure, which contains the function table entry and some other
        // useful information for the next test to execute. If the ShellProc
        // function returns SPR_SKIP, then the test case will not execute.
        Debug(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));
        // Start our logging level.
        pBT = (LPSPS_BEGIN_TEST)spParam;
        g_pKato->BeginLevel(
            pBT->lpFTE->dwUniqueID,
            TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
            pBT->lpFTE->lpDescription,
            pBT->dwThreadCount,
            pBT->dwRandomSeed);

        break;

    case SPM_END_TEST:
        // Sent to the DLL after a single test executes from the DLL.
        // This gives the DLL a time perform any common action that occurs at
        // the completion of each test, such as exiting the current logging
        // level. The spParam parameter will contain a pointer to a
        // SPS_END_TEST structure, which contains the function table entry and
        // some other useful information for the test that just completed. If
        // the ShellProc returned SPR_SKIP for a given test case, then the
        // ShellProc() will not receive a SPM_END_TEST for that given test case.
        Debug(TEXT("ShellProc(SPM_END_TEST, ...) called"));

        // End our logging level.
        pET = (LPSPS_END_TEST)spParam;
        g_pKato->EndLevel(
            TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
            pET->lpFTE->lpDescription,
            pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
            pET->dwResult == TPR_PASS ? TEXT("PASSED") :
            pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);


        break;

    case SPM_EXCEPTION:
        // Sent to the DLL whenever code execution in the DLL causes and
        // exception fault. TUX traps all exceptions that occur while
        // executing code inside a test DLL.
        Debug(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
        g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
        break;

    default:
        // Any messages that we haven't processed must, by default, cause us
        // to return SPR_NOT_HANDLED. This preserves compatibility with future
        // versions of the TUX shell protocol, even if new messages are added.
        return SPR_NOT_HANDLED;
    }

    return SPR_HANDLED;
}

////////////////////////////////////////////////////////////////////////////////
