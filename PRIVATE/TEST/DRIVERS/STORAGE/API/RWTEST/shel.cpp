//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
-------------------------------------------------------------------------------

Module Name:
 
    shel.cpp

Abstract:

    mtrwshel.cpp contains the shell processing functions for running as a
    tux .DLL. 

Functions:

    BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved)
    SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam)

Notes:

-------------------------------------------------------------------------------
*/

#include "main.h"
#include "globals.h"

// ----------------------------------------------------------------------------
// Function Prototypes and Table
// ----------------------------------------------------------------------------

TESTPROCAPI TestProc( UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY );
VOID ParseCommandLine( VOID );


// ----------------------------------------------------------------------------
// Dll Entry
// ----------------------------------------------------------------------------

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {

    return TRUE;
}


// ----------------------------------------------------------------------------
// Shell Processing Functions
// ----------------------------------------------------------------------------

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {
    
    switch (uMsg) {
        
        //------------------------------------------------------------------------
        // Message: SPM_LOAD_DLL
        //
        // Sent once to the DLL immediately after it is loaded. The spParam 
        // parameter will contain a pointer to a SPS_LOAD_DLL structure.    The DLL
        // should set the fUnicode member of this structre to TRUE if the DLL is
        // built with the UNICODE flag set.  By setting this flag, Tux will ensure
        // that all strings passed to your DLL will be in UNICODE format, and all
        // strings within your function table will be processed by Tux as UNICODE.
        // The DLL may return SPR_FAIL to prevent the DLL from continuing to load.
        //------------------------------------------------------------------------
        
        case SPM_LOAD_DLL: {
            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();
        
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));
        
            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif
        
            return SPR_HANDLED;
        }
        
        //------------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        //
        // Sent once to the DLL immediately before it is unloaded.
        //------------------------------------------------------------------------
        
        case SPM_UNLOAD_DLL: {
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));
        
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
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));
        
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
            // Display our Dlls command line if we have one.
            if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) {
                g_pKato->Log( LOG_DETAIL, TEXT("Command Line Arguments: %s"),
                    g_pShellInfo->szDllCmdLine );
            }

            return SPR_HANDLED;
        }
        
        //------------------------------------------------------------------------
        // Message: SPM_REGISTER
        //
        // This is the only ShellProc() message that a DLL is required to handle
        // (except for SPM_LOAD_DLL if you are UNICODE).  This message is sent
        // once to the DLL immediately after the SPM_SHELL_INFO message to query
        // the DLL for it’s function table.  The spParam will contain a pointer to
        // a SPS_REGISTER structure.    The DLL should store its function table in
        // the lpFunctionTable member of the SPS_REGISTER structure.  The DLL may
        // return SPR_FAIL to prevent the DLL from continuing to load.
        //------------------------------------------------------------------------
        
        case SPM_REGISTER: {
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_REGISTER, ...) called"));
            
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
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
            return SPR_HANDLED;
        }
        
        //------------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        // Sent to the DLL when the script has stopped.  This message is sent when
        // the script reaches its end, or because the user pressed stopped prior
        // to the end of the script.    This message is sent to all Tux DLLs,
        // including loaded Tux DLLs that are not in the script.
        //------------------------------------------------------------------------
        
        case SPM_STOP_SCRIPT: {
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
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
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP"));
            return SPR_HANDLED;
        }
    
        //------------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        // Sent to the DLL after a group of tests from that DLL has completed
        // running.  This gives the DLL a time to cleanup after it has been run.
        // This message does not mean that the DLL will not be called again to run
        // tests; it just means that the next test to run belongs to a different
        // DLL. SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL to track when it
        // is active and when it is not active.
        //------------------------------------------------------------------------
        
        case SPM_END_GROUP: {
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_END_GROUP, ...) called"));
            g_pKato->EndLevel(TEXT("END GROUP"));
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
        // test to execute. If the ShellProc function returns SPR_SKIP, then the
        // test case will not execute.
        //------------------------------------------------------------------------
        
        case SPM_BEGIN_TEST: {
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));
            
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
        // Sent to the DLL after a single test executes from the DLL.   This gives
        // the DLL a time perform any common action that occurs at the completion
        // of each test, such as exiting the current logging level.  The spParam
        // parameter will contain a pointer to a SPS_END_TEST structure, which
        // contains the function table entry and some other useful information for
        // the test that just completed.
        //------------------------------------------------------------------------
        
        case SPM_END_TEST: {
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_END_TEST, ...) called"));
            
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
            g_pKato->Log( LOG_COMMENT, TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
            return SPR_HANDLED;
        }
    } // End switch
    
    return SPR_NOT_HANDLED;
}
