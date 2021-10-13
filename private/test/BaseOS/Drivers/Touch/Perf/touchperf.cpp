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

#include "..\globals.h"
#include "..\winmain.h"

static BOOL PddPerformanceTest( DWORD );
int GetTouchPanelType();
TESTPROCAPI TouchPddPerformanceTest( UINT uMsg, 
                                     TPPARAM tpParam, 
                                     LPFUNCTION_TABLE_ENTRY lpFTE );


#define DEFAULT_SAMPLE_SIZE 800

CKato *g_pKato = NULL;
SPS_SHELL_INFO *g_pShellInfo;
DWORD dwSampleSize = 0;

// --------------------------------------------------------------------
// TUX Function table
extern "C" {
    FUNCTION_TABLE_ENTRY g_lpFTE[] = {
        TEXT("Touch Driver Performance Tests"                    ), 0,  0,              0, NULL,
        TEXT(   "PDD Performance Test" ), 1,  0,  PERF_BASE+ 1, TouchPddPerformanceTest,   
        {NULL, 0, 0, 0, NULL}
    };
}

//-----------------------------------------------------------------------------
//  TouchPddPerformanceTest  
//  The test collect 800 samples, measure the time it takes to collect the 
//  the samples and reports sample rate to user.
//  User can change the sample size by using the -t command option
// ---------------------------------------------------------------------------- 
TESTPROCAPI TouchPddPerformanceTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )

{    

    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwResult;


    if ( InitTouchWindow( TEXT( "Touch Driver Performance Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        dwResult = TPR_ABORT;
    }
    else
    {
        if( !CalibrateScreen( FALSE ) )
        {
            FAIL("Calibration failed, aborting test");
            dwResult = TPR_ABORT;
        }
        else
        {
            if (dwSampleSize != 0)
            {
                dwResult = PddPerformanceTest( dwSampleSize ) == TRUE ? TPR_PASS : TPR_FAIL;
            }
            else
            {
                dwResult = PddPerformanceTest( DEFAULT_SAMPLE_SIZE ) == TRUE ? TPR_PASS : TPR_FAIL;
            }
        }
    }
   
    ClearTitle();
    PostMessage( TEXT( "Please reboot device after\ntest completes and follow\nthe manual test running steps") ); 
    Sleep(8000);

    DeinitTouchWindow( TEXT( "Touch Driver Performance Test" ), dwResult );
    return dwResult;
} 

//-----------------------------------------------------------------------------
// PddPerformanceTest: Actual performance test routine.  
//
// ARGUMENTS:
//  dwSampleSize:  Number of samples to collect 
//
// RETURNS:
//  TRUE:   If successful. Detected sample rate is larger than 100
//  FALSE:  If otherwise. Detected sample rate is lower than 100
//-----------------------------------------------------------------------------
BOOL PddPerformanceTest( DWORD dwSampleSize )
{    
#define PERF_TEST_TIME  10000

    BOOL fRet               = FALSE;
    DWORD dwTime            = 0;
    TOUCH_POINT_DATA tpd    = {0};
    POINT point             = {0};
    DWORD dwSampleRate = 0;
    int nStatus;

    
    fRet = FALSE;
    dwTime = GetTickCount();
    nStatus = FALSE;
    PDD_PERF_DATA PddPerfData = {0};
    int touchPanelType =  GetTouchPanelType();
    
    g_touchPointQueue.Clear();

    PostTitle  ( TEXT( "Testing sample size of %d" ),  dwSampleSize );
    PostMessage( TEXT( "Vigorously draw circles on the\nscreen until screen clears." ) );  

    SetDrawPen( TRUE );

    DoPddPerformTest( dwSampleSize );
    do
    {
        if( g_touchPointQueue.Dequeue( &tpd ) )
        {

            fRet = TRUE;
            point.x = tpd.x;
            point.y = tpd.y;

            if( PEN_WENT_DOWN & tpd.dwTouchEventFlags )
            {
                SetLineFrom( point );
            }
            else if(  PEN_MOVE & tpd.dwTouchEventFlags )
            {
                DrawLineTo( point );
            }
        }

        if ( fRet == TRUE )
            if ( ( nStatus = GetPddPerformTestStatus( &PddPerfData ) ) != TRUE )
                    break;  //has collected enough samples
    }
    while( PERF_TEST_TIME >= ( GetTickCount() - dwTime ));

    ClearDrawing( );
    SetDrawPen( FALSE );

    if ( fRet == FALSE )
    {
        PostMessage( TEXT( "Test has timed out due to inactivity" ) );
        FAIL( "Test has timed out due to inactivity" );
        Sleep( 5000 );
        return FALSE;
    }

    ClearMessage( );

    
    if ( PddPerfData.dwEndTime != PddPerfData.dwStartTime )
    {
        if (nStatus != TRUE)
        {
            dwSampleRate = ( DWORD )( ( PddPerfData.dwSamples * 1000 ) / ( PddPerfData.dwEndTime - PddPerfData.dwStartTime ) );
        }
        else
        {
            dwSampleRate = ( DWORD )( ( PddPerfData.dwSamples * 1000 ) / ( PERF_TEST_TIME  ) );
        }

        ClearDrawing( );

        if ((touchPanelType == RESISTIVE) && (dwSampleRate < 100))
        {
             PostMessage( TEXT( "Sample rate detected at %d\n per second.Low Sample Rate" ), 
                          dwSampleRate ); 
        }
        else if ((touchPanelType == CAPACITIVE) && (dwSampleRate < 60))
        {
             PostMessage( TEXT( "Sample rate detected at %d\n per second.Low Sample Rate" ), 
                          dwSampleRate ); 
        }
        else if (dwSampleRate > 200)
        {
             PostMessage( TEXT( "Sample rate detected at %d\n per second.High Sample Rate" ), 
                          dwSampleRate ); 
        }
        else 
        {
            PostMessage( TEXT( "Sample rate detected at %d\n per second.Normal Sample Rate" ), 
                          dwSampleRate ); 
        }
                
        Sleep( 6000 ); 

        g_pKato->Log( LOG_DETAIL, 
                      TEXT( "Sample Rate detected at %d per second for sample size %d" ), 
                      dwSampleRate,
                      dwSampleSize  );
        
    }
    else
    {
        FAIL( "Start time equals End Time!" );
        return FALSE;
    }

    if ( fRet )
    {
        PostTitle( TEXT("Did drawing work correctly?") );
        
        fRet = WaitForInput( USER_WAIT_TIME );
    }
    if (touchPanelType == RESISTIVE)
    {
        if (fRet && (dwSampleRate < 100)) 
        {
            // consider test failed if sample rate is lower than 100 on Resistive touch device
            FAIL("Detected sample rate is lower than 100 on Resistive touch device");
            fRet = FALSE;
        }
    }
    else
    {
        if (fRet && (dwSampleRate < 60)) 
        {
            // consider test failed if sample rate is lower than 60 on Capacitive touch device
            FAIL("Detected sample rate is lower than 60 on Capacitive touch device");
            fRet = FALSE;
        }
   }

    return fRet;
} 


//----------------------------------------------------------------------
// Request users to enter the type of touch controller, Capacitive or Resistive.
// Since there is no API available for test to query the type of touch controller, getting
// the information from user is the only way to go.
//----------------------------------------------------------------------
int GetTouchPanelType()
{
    int touchPanelType =  CAPACITIVE;
    DWORD dwStartTime       = GetTickCount();
    BOOL fPenDown           = FALSE;
    TOUCH_POINT_DATA tpd    = {0};
    DWORD displayWidth = GetSystemMetrics(SM_CXSCREEN);

    Sleep( INPUT_DELAY_MS ); 
    
    PostMessage( TEXT("CAPACITIVE                RESISTIVE") );  
    PostTitle  ( TEXT( "Tap on the type of touch panel" ));

    g_touchPointQueue.Clear();
    do
    {
        if( !fPenDown )
        {
            // wait for pen to go down
            if( g_touchPointQueue.Dequeue( &tpd ) )
            {
                if( PEN_WENT_DOWN & tpd.dwTouchEventFlags )
                {                    
                    fPenDown = TRUE;
                    // check the coordinates to see if this
                    // is an affirmative or a negative
                    if( tpd.x  < (INT)(displayWidth / 2) )
                    {
                        // touch was on the left half
                        touchPanelType= CAPACITIVE;
                    } 
                    else
                    {
                        // touch was on the right half
                         touchPanelType= RESISTIVE;
                    }                    
                }
            }
        }

        else
        {
            // wait for pen to come back up
            if( g_touchPointQueue.Dequeue( &tpd ) )
            {
                if( PEN_WENT_UP & tpd.dwTouchEventFlags ) 
                {
                    goto Exit;
                }
            }
        }
    }
    while( (GetTickCount() - dwStartTime) < USER_WAIT_TIME);

    if( !fPenDown )
        DETAIL( "User prompt timed out waiting for user input" );
Exit:
    ClearMessage();
    return touchPanelType;
}

//-----------------------------------------------------------------------------
// ProcessCmdLine: Processes the command line given to the library  
//
// ARGUMENTS:
//  szCmdLine:  Command Line
//
// RETURNS:  void
//-----------------------------------------------------------------------------
static
void
ProcessCmdLine( 
    LPCTSTR szCmdLine )
{
    const int len = 11;
    TCHAR szSampleSize[ len]; //Maximum of DWORD has 10 digits
    // parse command line
    int option = 0;
    LPCTSTR szOpt = _T("t:v:");

    NKDbgPrintfW(_T("touchPerf: Usage: tux -o -n -d touchperf [-c\" [-v drivername] [-t SampleSize]\"]\r\n"));
    NKDbgPrintfW(_T("By default the test collect 800 touch samples. User can change the sample size\r\n"));
    while(EOF != (option = WinMainGetOpt(szCmdLine, szOpt)))
    {
        switch(option)
        {
        case L'v':
            wcscpy_s(g_szTouchDriver, MAX_TOUCHDRV_NAMELEN, optArg);
            NKDbgPrintfW(_T("Driver Name: %s\r\n"), g_szTouchDriver);
            break;
        case L't':
            wcscpy_s( szSampleSize, len, optArg);
            dwSampleSize = ( DWORD ) ( _ttol( szSampleSize) ); 
            break;
        default:
            // bad parameters
            NKDbgPrintfW(_T("touchtest: Unknown option\r\n"), option);
            return;
        }
    }
}

// --------------------------------------------------------------------
SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam) 
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
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: TOUCHPERF.DLL"));                      
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
           g_pKato->EndLevel(TEXT("END GROUP: TOUCHPERF.DLL"));         
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
                             _T("New Touch driver is not detected. Skip all test cases in the test suite"));
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

