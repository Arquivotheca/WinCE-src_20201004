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

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <clparse.h>        // For Command Parser
#include "..\bootStress.h"

BOOL           gBootStressSetUp = FALSE;
DWORD        dwRunTime = 100;

CKato *g_pKato = NULL;
SPS_SHELL_INFO *g_pShellInfo;

TESTPROCAPI RebootStressTest( UINT uMsg, TPPARAM tpParam, 
                                                       LPFUNCTION_TABLE_ENTRY lpFTE );


// ----------------------------------------------------------------------------
// TUX Function table
//-----------------------------------------------------------------------------
extern "C" {
        FUNCTION_TABLE_ENTRY g_lpFTE[] = {
        TEXT("Bootloader stress Test"                    ), 0,  0,              0, NULL,
        TEXT("Bootloader stress Test" ), 1,  0,  1, RebootStressTest,   
        {NULL, 0, 0, 0, NULL}
    };
}

//-----------------------------------------------------------------------------
//Set up test
//This function creates three files in your release directory
// BootStress.txt:  contains number of desired reboot times and total boot time for each reboot
// BootStressTotalRan.txt: contains number of times the device has been rebooted
// BootStressLastTick.txt:  contains last reboot time stamp. The time stamp is used for calculating
//                                    total boot time.
//-----------------------------------------------------------------------------
static BOOL BootStressSetUp( )
{
    BOOL fOk = FALSE;
    errno_t err;

        
    FILE* pBootTestFile = NULL;
    err = _wfopen_s( &pBootTestFile, FILE_BOOTTEST, L"w+" );
    if( (err != 0) ||(pBootTestFile == NULL ))
    {
        goto cleanUp;
    }
    fwprintf_s( pBootTestFile, L"%ld", dwRunTime);
    fclose( pBootTestFile );
    pBootTestFile = NULL;

    FILE* pTimesRunFile = NULL;
    err = _wfopen_s( &pTimesRunFile, FILE_TOTAL_RAN , L"w+" );
    if ((err != 0) || (pTimesRunFile == NULL))
    {
        goto cleanUp;
    }
    fwprintf_s( pTimesRunFile, L"%ld", 0);
    fclose( pTimesRunFile );
    pTimesRunFile = NULL;
    
    FILETIME ftCurTime;
    GetCurrentFT( &ftCurTime );
    ULARGE_INTEGER ulCurTime;
    ulCurTime.LowPart  = ftCurTime.dwLowDateTime;
    ulCurTime.HighPart  = ftCurTime.dwHighDateTime;
   

    FILE* pLastTickFile = NULL;
    err = _wfopen_s( &pLastTickFile, FILE_LASTTICK, L"w+" );
    if ((err != 0) ||(pLastTickFile == NULL))
    {
        goto cleanUp;
    }
    fwprintf_s( pLastTickFile, L"%lu", ulCurTime.HighPart );
    fwprintf_s( pLastTickFile, L" %lu", ulCurTime.LowPart );
    fclose( pLastTickFile );
    pLastTickFile = NULL;
          
    gBootStressSetUp = TRUE;
    fOk = TRUE;
    return fOk;
    
cleanUp:
    NKDbgPrintfW(L"BootStress.exe: Open files failed.\r\n");
    return fOk;
}

//-----------------------------------------------------------------------------
//     Reboot the platform multiple times
//-----------------------------------------------------------------------------
TESTPROCAPI RebootStressTest( UINT uMsg, 
                                                          TPPARAM tpParam, 
                                                          LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    UNREFERENCED_PARAMETER(uMsg);
    
    if (!gBootStressSetUp)
    {
        g_pKato->Log( LOG_DETAIL, 
                                TEXT( "Setup test. Reboot device %d times" ), 
                                 dwRunTime);
        BootStressSetUp();
    }
    else
    {
         g_pKato->Log( LOG_DETAIL, TEXT( "Reboot" ));
         reboot();
    }
    return TPR_PASS;
}

//----------------------------------------------------------------------
static void ProcessCmdLine( LPCTSTR szCmdLine )
{
    const int len = 10;
    TCHAR szRunTime[ len + 1 ]; //Maximum of DWORD has 10 digits
    // parse command line

    CClParse cmdLine(szCmdLine);
    
    if(cmdLine.GetOptString(_T("t"),szRunTime, len))
    {
        dwRunTime = ( DWORD ) ( _ttol( szRunTime) ); 
               
     }
}
// --------------------------------------------------------------------
SHELLPROCAPI ShellProc( UINT uMsg, SPPARAM spParam) 
{
    switch (uMsg) 
   {
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
        
            return SPR_HANDLED;
        }

        case SPM_REGISTER: 
        {
            g_pKato->Log(LOG_DETAIL, _T("BootStress: Usage: tux -o -d downloadstress [-c\" [-t RebootIterations]\"]\r\n"));
            g_pKato->Log(LOG_DETAIL, _T("By default the test reboots the device 100 times. The -t parameter allows the user to specify the reboot count.\r\n"));
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif
        }

        case SPM_START_SCRIPT: 
        case SPM_STOP_SCRIPT: 
        case SPM_BEGIN_GROUP: 
        case SPM_END_GROUP:
        case SPM_BEGIN_TEST: 
        case SPM_END_TEST: 
        {           
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



