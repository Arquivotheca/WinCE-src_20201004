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
//
// PDD_Performace.cpp:  PDD performance test functions for the Touch PQD driver
//
// Test Functions: TouchPddPerformanceTest()
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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#include "globals.h"

static BOOL PddPerformanceTest( DWORD );

#define __FILE_NAME__   TEXT( "PDD_Performance.CPP" )  //This filename for kato logs


/*
 *    TouchPddPerformance100Test:  Performance test for a sample size of 100
 */
TESTPROCAPI TouchPddPerformance100Test( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )

{    

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwResult;


    if ( InitTouchWindow( TEXT( "PDD Performance Test - 100 Samples" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        dwResult = TPR_ABORT;
    }
    else
        if( !CalibrateScreen( FALSE ) )
        {
            FAIL("Calibration failed, aborting test");
            dwResult = TPR_ABORT;
        }
        else
        {
            dwResult = PddPerformanceTest( 100 ) == TRUE ? TPR_PASS : TPR_FAIL;
        }
    


    DeinitTouchWindow( TEXT( "PDD Performance Test - 100 Samples" ), dwResult );


    return dwResult;
} 

/*
 *    TouchPddPerformance100Test:  Performance test for a sample size of 300
 */
TESTPROCAPI TouchPddPerformance300Test( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )

{    

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwResult;;


    if ( InitTouchWindow( TEXT( "PDD Performance Test - 300 Samples" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        dwResult = TPR_ABORT;
    }
    else
        if( !CalibrateScreen( FALSE ) )
        {
            FAIL("Calibration failed, aborting test");
            dwResult = TPR_ABORT;
        }
        else
            dwResult = PddPerformanceTest( 300 ) == TRUE ? TPR_PASS : TPR_FAIL;
    


    DeinitTouchWindow( TEXT( "PDD Performance Test - 300 Samples" ), dwResult );


    return dwResult;
} 

/*
 *    TouchPddPerformance500Test:  Performance test for a sample size of 500
 */
TESTPROCAPI TouchPddPerformance500Test( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )

{    

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwResult;;


    if ( InitTouchWindow( TEXT( "PDD Performance Test - 500 Samples" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        dwResult = TPR_ABORT;
    }
    else
        if( !CalibrateScreen( FALSE ) )
        {
            FAIL("Calibration failed, aborting test");
            dwResult = TPR_ABORT;
        }
        else
            dwResult = PddPerformanceTest( 500 ) == TRUE ? TPR_PASS : TPR_FAIL;
    


    DeinitTouchWindow( TEXT( "PDD Performance Test - 500 Samples" ), dwResult );


    return dwResult;
} 



/*
 * PddPerformanceTest: Actual performance test routine.  
 *
 * ARGUMENTS:
 *    dwSampleSize:  Number of samples to collect 
 *
 * RETURNS:
 *    TRUE:    If successful
 *    FALSE:    If otherwise
 */
BOOL PddPerformanceTest( DWORD dwSampleSize )
{    
#define PERF_TEST_TIME  150000


    BOOL fRet               = FALSE;
    DWORD dwTime            = 0;
    TOUCH_POINT_DATA tpd    = {0};
    POINT point             = {0};

    int nStatus;

    DWORD dwSampleRate;

    PDD_PERF_DATA PddPerfData;


    while ( TRUE )
    {

        PostTitle  ( TEXT( "Testing sample size of %d" ),  dwSampleSize );
        PostMessage( TEXT( "Draw on the screen until screen clears.\r\nDo not lift the pen." ) );  


        SetDrawPen( TRUE );

        fRet = FALSE;
        dwTime = GetTickCount();

        nStatus = FALSE;
        g_touchPointQueue.Clear();
        do
        {

            if( g_touchPointQueue.Dequeue( &tpd ) )
            {

                fRet = TRUE;
                point.x = tpd.x;
                point.y = tpd.y;
    
                if( PEN_WENT_DOWN & tpd.dwTouchEventFlags )
                {
                    DoPddPerformTest( dwSampleSize );

    
                    SetLineFrom( point );
                }
                else if( TouchSampleDown( tpd.flags ) )
                {
                    DrawLineTo( point );
                }
            }

            if ( fRet == TRUE )
                if ( ( nStatus = GetPddPerformTestStatus( &PddPerfData ) ) != TRUE )
                        break;
        }
        while( PERF_TEST_TIME > ( GetTickCount() - dwTime ) || nStatus == TRUE );

        ClearDrawing( );
        SetDrawPen( FALSE );

        if ( nStatus != EOF )
            break;

        PostMessage( TEXT( "Pen was not held down long enough" ) );
        Sleep( 1000 );
        WaitForClick( USER_WAIT_TIME );

    }

    if ( fRet == FALSE )
    {
        PostMessage( TEXT( "Test has timed out due to inactivity" ) );
        FAIL( "Test has timed out due to inactivity" );
        Sleep( 1000 );
        return FALSE;
    }

    ClearMessage( );

    
    if ( PddPerfData.dwEndTime != PddPerfData.dwStartTime )
    {
        dwSampleRate = ( DWORD )( ( PddPerfData.dwSamples * 1000 ) / ( PddPerfData.dwEndTime - PddPerfData.dwStartTime ) );
        
        ClearDrawing( );

        PostMessage( TEXT( "Sample Rate detected at %d per second" ), 
                          dwSampleRate );  
                
        Sleep( 1000 ); 

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
        
        fRet = WaitForInput( USER_WAIT_TIME, FALSE );
    }


    return fRet;
} 
