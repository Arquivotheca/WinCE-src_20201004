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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#include "tuxmain.h"
#include "..\globals.h"
#include "..\winmain.h"

#define __FILE_NAME__   TEXT("TUXMAIN.CPP")

#ifndef UNDER_CE
#include <stdio.h>
#endif

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

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

BOOL WINAPI 
DllMain(
    HANDLE hInstance, 
    ULONG dwReason, 
    LPVOID lpReserved ) 
{
    return TRUE;
}

#endif // UNDER_CE

static void ProcessCmdLine( LPCTSTR );

// --------------------------------------------------------------------
SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam) 
// --------------------------------------------------------------------
{

    switch (uMsg) {

        case SPM_LOAD_DLL: 
        {
            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject(); 
            return SPR_HANDLED;
        }

        case SPM_UNLOAD_DLL: 
        {               
            return SPR_HANDLED;
        }

        case SPM_SHELL_INFO: 
        {        
         
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine )
            {
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL, 
                    _T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);

                ProcessCmdLine(g_pShellInfo->szDllCmdLine);
            } 

            // get the global pointer to our HINSTANCE
            g_hInstance = g_pShellInfo->hLib; 
             
            return SPR_HANDLED;
        }

        case SPM_REGISTER: 
        {
            if (TouchDriverVersion() == CE7)
            {
                ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            }
            else 
            {
                ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE1;
            }
            
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif
        }

        case SPM_START_SCRIPT: 
        {           
            return SPR_HANDLED;
        }

        case SPM_STOP_SCRIPT: 
        {
            return SPR_HANDLED;
        }

        case SPM_BEGIN_GROUP: 
        {
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TOUCHFUNC.DLL"));                      

            // load the touch panel driver and initialize
            if( !LoadTouchDriver() )
            {
                FAIL("Failed to load touch driver");
                return SPR_FAIL;
            }
            return SPR_HANDLED;
        }

        case SPM_END_GROUP: 
        {
            UnloadTouchDriver( );

            g_pKato->EndLevel(TEXT("END GROUP: TOUCHFUNC.DLL"));            

            return SPR_HANDLED;
        }

        case SPM_BEGIN_TEST: 
        {
            // Start our logging level.
            LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);
            
            if (TouchDriverVersion() != NO_TOUCH)
            {
               return SPR_HANDLED;
            }
            else  // touch driver is not detected in device
            {
                g_pKato->Log( LOG_DETAIL, 
                             _T("Touch driver is not supported. Skip all test cases in the test suite"));
                return SPR_FAIL;
            }
        }
        case SPM_END_TEST: 
        {
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

        case SPM_EXCEPTION: 
        {
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
            return SPR_HANDLED;
        }
    }

    return SPR_NOT_HANDLED;

}

/*
//
//  Prints the english meaning of the sample flags as defined in tchddi.h
//  to the Kato log as LOG_DETAILs.
//
// --------------------------------------------------------------------  
void
PrintSampleFlags(
    DWORD dwFlags )

{
    if( dwFlags & TouchSampleValidFlag )
        g_pKato->Log( LOG_DETAIL, TEXT("TouchSampleValidFlag") );
    if( dwFlags & TouchSampleDownFlag )
        g_pKato->Log( LOG_DETAIL, TEXT("TouchSampleDownFlag") );
    if( dwFlags & TouchSampleIsCalibratedFlag )
        g_pKato->Log( LOG_DETAIL, TEXT("TouchSampleIsCalibratedFlag") );
    if( dwFlags & TouchSamplePreviousDownFlag )
        g_pKato->Log( LOG_DETAIL, TEXT("TouchSamplePreviousDownFlag") );
    if( dwFlags & TouchSampleIgnore )
        g_pKato->Log( LOG_DETAIL, TEXT("TouchSampleIgnore") );
    if( dwFlags & TouchSampleMouse )
        g_pKato->Log( LOG_DETAIL, TEXT("TouchSampleMouse") );                
}
*/



static
void
ProcessCmdLine( 
    LPCTSTR szCmdLine )
{
#define nTIMESTRLEN 10

    // parse command line
    int option = 0;
    LPCTSTR szOpt = _T("t:av:");

    NKDbgPrintfW(_T("touchfunc: Usage: tux -o -n -d touchfunc [-c\" [-v drivername]\"]\r\n"));
    while(EOF != (option = WinMainGetOpt(szCmdLine, szOpt)))
    {
        switch(option)
        {
        case L'v':
            wcsncpy_s(g_szTouchDriver, _countof(g_szTouchDriver), optArg, MAX_TOUCHDRV_NAMELEN);
            NKDbgPrintfW(_T("Driver Name: %s\r\n"), g_szTouchDriver);
            break;
        
        default:
            // bad parameters
            NKDbgPrintfW(_T("touchtest: Unknown option\r\n"), option);
            return;
        }
    }
}

