// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------

#include "tuxmain.h"
#include "globals.h"

#define __FILE_NAME__   TEXT("TUXMAIN.CPP")

#ifndef UNDER_CE
#include <stdio.h>
#endif

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// global touch point queue class instance
MiniQueue<TOUCH_POINT_DATA, 100> g_touchPointQueue;

// --------------------------------------------------------------------
// Windows CE specific code

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
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;

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
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TOUCHTEST.DLL"));                      

            //
            // load the touch panel driver and initialize
            //
            if( !LoadTouchDriver() )
            {
                FAIL("Failed to load touch driver");
                return SPR_FAIL;
            }

            if( !Initialize() )
            {
                FAIL("Failed to initialize");
                return SPR_FAIL;
            }

            return SPR_HANDLED;
        }

        case SPM_END_GROUP: 
        {
            g_pKato->EndLevel(TEXT("END GROUP: TOUCHTEST.DLL"));

            PostTitle(TEXT("All Tests Complete"));

            //
            // unload the touch driver and clean up
            //
            Deinitialize( );
            UnloadTouchDriver( );

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

            // prompt for user intervention
            PostTitle(
                TEXT("%s"),
                pBT->lpFTE->lpDescription );            
            WaitForClick( USER_WAIT_TIME );
         
            return SPR_HANDLED;
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
            
            // prompt for user intervention
            PostTitle(
                TEXT("%s : %s"),
                pET->lpFTE->lpDescription,
                pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED") );            
            WaitForClick( USER_WAIT_TIME );
             
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

// --------------------------------------------------------------------
void
PrintSampleFlags(
    DWORD dwFlags )
//
//  Prints the english meaning of the sample flags as defined in tchddi.h
//  to the Kato log as LOG_DETAILs.
//
// --------------------------------------------------------------------  
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

// --------------------------------------------------------------------
VOID 
SystemErrorMessage( 
    DWORD dwMessageId )
//
//  Prints the English meaning of a system error code.
//
//  Typical usage:
//
//      SystemErrorMessage( GetLastError() )
//
// --------------------------------------------------------------------
{
    LPTSTR msgBuffer;       // string returned from system
    DWORD cBytes;           // length of returned string

    cBytes = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        dwMessageId,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (TCHAR *)&msgBuffer,
        500,
        NULL );
    if( msgBuffer ) {
        msgBuffer[ cBytes ] = TEXT('\0');
        g_pKato->Log(LOG_DETAIL, TEXT( "SYSTEM ERROR %d: %s"), 
            dwMessageId, msgBuffer );
        LocalFree( msgBuffer );
    }
    else {
        g_pKato->Log(LOG_DETAIL, TEXT( "FormatMessage error: %d"), 
            GetLastError());
    }
}


const DWORD MAX_OPT_ARG = 256;
const TCHAR END_OF_STRING = (TCHAR)NULL;
const TCHAR BAD_ARG = _T('?');
const TCHAR ARG_FLAG = _T('-');
const TCHAR OPT_ARG = _T(':');

TCHAR optArg[MAX_OPT_ARG];

// --------------------------------------------------------------------
INT
WinMainGetOpt(
    LPCTSTR szCmdLine,
    LPCTSTR szOptions )
// --------------------------------------------------------------------    
{
    static LPCTSTR pCurPos = szCmdLine;   
    LPCTSTR pCurOpt = szOptions;
    LPTSTR pOptArg = optArg;
    UINT cOptArg = 0;
    INT option = 0;
    TCHAR quoteChar = TCHAR(' ');

    // end of string reached, or NULL string
    if( NULL == pCurPos || END_OF_STRING == *pCurPos )
    {
        return EOF;
    }

    // eat leading white space
    while( _T(' ') == *pCurPos )
        pCurPos++;

    // trailing white space
    if( END_OF_STRING == *pCurPos )
    {
       return EOF;
    }

    // found an option
    if( ARG_FLAG != *pCurPos )
    {
        return BAD_ARG;
    }
    
    pCurPos++;

    // found - at end of string
    if( END_OF_STRING == *pCurPos )
    {
        return BAD_ARG;
    }

    // search all options requested
    for( pCurOpt = szOptions; *pCurOpt != END_OF_STRING; pCurOpt++ )
    {
        // found matching option
        if( *pCurOpt == *pCurPos )
        {
            option = (int)*pCurPos;
            
            pCurOpt++;
            pCurPos++;
            if( OPT_ARG == *pCurOpt )
            {
                // next argument is the arg value
                // look for something between quotes or whitespace                
                while( _T(' ') == *pCurPos )
                    pCurPos++;

                if( END_OF_STRING == *pCurPos )
                {
                    return BAD_ARG;
                }

                if( _T('\"') == *pCurPos )
                {
                    // arg value is between quotes
                    quoteChar = *pCurPos;
                    pCurPos++;
                }
                
                else if( ARG_FLAG == *pCurPos )
                {
                    return BAD_ARG;
                }

                else                
                {
                    // arg end at next whitespace
                    quoteChar = _T(' ');
                }
                
                pOptArg = optArg;
                cOptArg = 0;
                
                // TODO: handle embedded, escaped quotes
                while( quoteChar != *pCurPos && END_OF_STRING != *pCurPos  && cOptArg < MAX_OPT_ARG )
                {
                    *pOptArg++ = *pCurPos++;      
                    cOptArg++;
                }

                // missing final quote
                if( _T(' ') != quoteChar && quoteChar != *pCurPos )
                {
                    return BAD_ARG;
                }
                
                // append NULL char to output string
                *pOptArg = END_OF_STRING;

                // if there was no argument value, fail
                if( 0 == cOptArg )
                {
                    return BAD_ARG;
                }   
            }  

            return option;            
        }
    }
    pCurPos++;
    // did not find a macthing option in list
    return BAD_ARG;
}

static
void
ProcessCmdLine( 
    LPCTSTR szCmdLine )
{
    // parse command line
    int option = 0;
    LPCTSTR szOpt = _T("v:");

    NKDbgPrintfW(_T("touchtest: Usage: tux -o -d touchtest [-v drivername]\r\n"));
    
    while(EOF != (option = WinMainGetOpt(szCmdLine, szOpt)))
    {
        switch(option)
        {
        case L'v':
            wcsncpy(g_szTouchDriver, optArg, MAX_TOUCHDRV_NAMELEN);
            NKDbgPrintfW(_T("Driver Name: %s\r\n"), g_szTouchDriver);
            break;

        default:
            // bad parameters
            NKDbgPrintfW(_T("touchtest: Unknown option\r\n"), option);
            return;
        }
    }
}
