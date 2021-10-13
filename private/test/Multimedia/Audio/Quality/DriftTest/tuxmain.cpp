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
#include "TuxMain.h"

// shell and log vars
HINSTANCE globalInst;
SPS_SHELL_INFO g_spsShellInfo;
CKato * g_pDebugLogKato = NULL;

void SetKato(CKato * pKato)
{
    g_pDebugLogKato = pKato;
    return;
}

TESTPROCAPI HandleTuxMessages(UINT uMsg, TPPARAM tpParam)
{
    DWORD retval = SPR_NOT_HANDLED;
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        retval = SPR_HANDLED;
    }
    return retval;
}

////////////////////////////////////////////////////////////////////////////////
// Debug
//  Printf-like debug logging. Uses either OutputDebugString or a Kato logger
//   object. By default, OutputDebugString is called. If SetKato is called with
//   a pointer to a Kato logger, DebugOut will use that Kato logger object.
//
// Parameters:
//  szFormat        Formatting string (as in printf).
//  ...             Additional arguments.
//
// Return value:
//  None.

void Debug(LPCTSTR szFormat, ...)
{
    va_list pArgs;
    va_start(pArgs, szFormat);
    
    if(g_pDebugLogKato) {
        g_pDebugLogKato->LogV( LOG_DETAIL, szFormat, pArgs);
    }
    else {
        static const int iBUFSIZE = 1024;
        static  TCHAR   szHeader[] = TEXT("");
        TCHAR   szBuffer[iBUFSIZE];
        
        errno_t  Error = _tcscpy_s (szBuffer,countof(szBuffer),szHeader);
        int nChars = _vstprintf_s(
        szBuffer + countof(szHeader) - 1,countof(szBuffer)-countof(szHeader),
        szFormat,
        pArgs);
        Error = _tcscat_s(szBuffer,countof(szBuffer),TEXT("\r\n"));

        OutputDebugString(szBuffer);        
    }
    va_end(pArgs);

}

// dummy main
BOOL    WINAPI
DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
{
    return 1;
}

//******************************************************************************
//***** ShellProc
//******************************************************************************

SHELLPROCAPI
ShellProc(UINT uMsg, SPPARAM spParam)
{
    SPS_BEGIN_TEST *pBeginTestInfo;
    
    switch (uMsg)
    {

        case SPM_REGISTER:
            ((LPSPS_REGISTER) spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
#else // UNICODE
            return SPR_HANDLED;
#endif // UNICODE

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
            g_pDebugLogKato = (CKato *) KatoGetDefaultObject();

#ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif

        case SPM_UNLOAD_DLL:
        case SPM_START_SCRIPT:
        case SPM_STOP_SCRIPT:
            return SPR_HANDLED;

        case SPM_SHELL_INFO:
            // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
            // some useful information about its parent shell and environment. The
            // spParam parameter will contain a pointer to a SPS_SHELL_INFO
            // structure. The pointer to the structure may be stored for later use
            // as it will remain valid for the life of this Tux Dll. The DLL may
            // return SPR_FAIL to prevent the DLL from continuing to load.
            g_spsShellInfo = *(LPSPS_SHELL_INFO) spParam;
            globalInst = g_spsShellInfo.hLib;

            // if the process command line fails, then that means the user requested the command
            // line options, so exit the dll.
            if( !ProcessCommandLine(g_spsShellInfo.szDllCmdLine) )
            {
                Debug(TEXT("invalid command line, failing the test suite so it won't continue."));
                return SPR_FAIL;
            }
            return SPR_HANDLED;

        case SPM_BEGIN_GROUP:
            // Sent to the DLL before a group of tests from that DLL is about to
            // be executed. This gives the DLL a time to initialize or allocate
            // data for the tests to follow. Only the DLL that is next to run
            // receives this message. The prior DLL, if any, will first receive
            // a SPM_END_GROUP message. For global initialization and
            // de-initialization, the DLL should probably use SPM_START_SCRIPT
            // and SPM_STOP_SCRIPT, or even SPM_LOAD_DLL and SPM_UNLOAD_DLL.
            return SPR_HANDLED;

        case SPM_END_GROUP:
            // Sent to the DLL after a group of tests from that DLL has completed
            // running. This gives the DLL a time to cleanup after it has been
            // run. This message does not mean that the DLL will not be called
            // again; it just means that the next test to run belongs to a
            // different DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
            // to track when it is active and when it is not active.
            return SPR_HANDLED;

        case SPM_BEGIN_TEST:
            // Sent to the DLL immediately before a test executes. This gives
            // the DLL a chance to perform any common action that occurs at the
            // beginning of each test, such as entering a new logging level.
            // The spParam parameter will contain a pointer to a SPS_BEGIN_TEST
            // structure, which contains the function table entry and some other
            // useful information for the next test to execute. If the ShellProc
            // function returns SPR_SKIP, then the test case will not execute.    
            if(spParam!=NULL)
            {
                pBeginTestInfo = reinterpret_cast<SPS_BEGIN_TEST *>(spParam);
                srand(pBeginTestInfo->dwRandomSeed);
                g_pDebugLogKato->BeginLevel(((LPSPS_BEGIN_TEST) spParam)->lpFTE->dwUniqueID, TEXT("BEGIN TEST: <%s>, Thds=%u,"),
                                 ((LPSPS_BEGIN_TEST) spParam)->lpFTE->lpDescription, ((LPSPS_BEGIN_TEST) spParam)->dwThreadCount);
            }
            else return SPR_FAIL;

            return SPR_HANDLED;

        case SPM_END_TEST:
            // Sent to the DLL after a single test executes from the DLL.
            // This gives the DLL a time perform any common action that occurs at
            // the completion of each test, such as exiting the current logging
            // level. The spParam parameter will contain a pointer to a
            // SPS_END_TEST structure, which contains the function table entry and
            // some other useful information for the test that just completed. If
            // the ShellProc returned SPR_SKIP for a given test case, then the
            // ShellProc() will not receive a SPM_END_TEST for that given test case.
            g_pDebugLogKato->EndLevel(TEXT("END TEST: <%s>"), ((LPSPS_END_TEST) spParam)->lpFTE->lpDescription);
            return SPR_HANDLED;

        case SPM_EXCEPTION:
            // Sent to the DLL whenever code execution in the DLL causes and
            // exception fault. TUX traps all exceptions that occur while
            // executing code inside a test DLL.
            g_pDebugLogKato->Log(0, TEXT("Exception occurred!"));
            return SPR_HANDLED;
    }
    return SPR_NOT_HANDLED;
}
