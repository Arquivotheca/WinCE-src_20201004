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

#define __FILE_NAME__   TEXT("APITEST.CPP")

/* ----------------------------------------------------------------------------
 * DeviceCapsTest:
 * Test IOCTL_TOUCH_SET_SAMPLE_RATE, IOCTL_TOUCH_GET_CALIBRATION_POINTS
 * Query current sample rate and number of calibration points needed
 * to perform the calibration of touch screen
-----------------------------------------------------------------------------*/
TESTPROCAPI 
DeviceCapsTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }


    BOOL fRet = TRUE;
    TPDC_SAMPLE_RATE tsr = {0};
    TPDC_CALIBRATION_POINT_COUNT tcpc = {0};

    //PostMessage( TEXT("Please Wait...") );

    // make sure the TouchPanelGetDeviceCaps function was loaded
    if( NULL == g_pfnTouchPanelGetDeviceCaps )
    {
        DETAIL("Driver does not support TouchPanelGetDeviceCaps");
        return TPR_SKIP;
    }

    //
    // Query sample rate
    //
    DETAIL("Calling TouchPanelGetDeviceCaps( TPDC_SAMPLE_RATE_ID )");
    if( !(*g_pfnTouchPanelGetDeviceCaps)( TPDC_SAMPLE_RATE_ID, &tsr ) )
    {
        fRet = FALSE;
        FAIL("TouchPanelGetDeviceCaps( TPDC_SAMPLE_RATE_ID )");
        goto Exit;
    }
    g_pKato->Log( LOG_DETAIL,
        TEXT("SamplesPerSecondLow: %d"), tsr.SamplesPerSecondLow );
    g_pKato->Log( LOG_DETAIL,
        TEXT("SamplesPerSecondHigh: %d"), tsr.SamplesPerSecondHigh );
    g_pKato->Log( LOG_DETAIL,
        TEXT("CurrentSampleRateSetting: %s"), 
        0 == tsr.CurrentSampleRateSetting ? TEXT("Low") : TEXT("High") );        

    //
    // Query calibration point count
    //
    DETAIL("Calling TouchPanelGetDeviceCaps( TPDC_CALIBRATION_POINT_COUNT_ID )");
    if( !(*g_pfnTouchPanelGetDeviceCaps)( TPDC_CALIBRATION_POINT_COUNT_ID, &tcpc ) )
    {
        fRet = FALSE;
        FAIL("TouchPanelGetDeviceCaps( TPDC_CALIBRATION_POINT_COUNT_ID )");
        goto Exit;
    }
    g_pKato->Log( LOG_DETAIL,
        TEXT("flags: %u"), tcpc.flags );
    g_pKato->Log( LOG_DETAIL,
        TEXT("cCalibrationPoints: %d"), tcpc.cCalibrationPoints ); 

Exit:
    ClearMessage( );
    return fRet ? TPR_PASS : TPR_FAIL;
}

/* ----------------------------------------------------------------------------
 * SetSampleRateTest:
 * Test IOCTL_TOUCH_SET_SAMPLE_RATE
 * Sets sample rate low and sample rate high
-----------------------------------------------------------------------------*/
TESTPROCAPI 
SetSampleRateTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet = TRUE;
    TPDC_SAMPLE_RATE tsr = {0};

    // make sure the TouchPanelGetDeviceCaps function was loaded
    if( NULL == g_pfnTouchPanelGetDeviceCaps )
    {
        DETAIL("Driver does not support TouchPanelGetDeviceCaps");
        return TPR_SKIP;
    }
  
    // make sure the TouchPanelSetMode function was loaded
    if( NULL == g_pfnTouchPanelSetMode )
    {
        DETAIL("Driver does not support TouchPanelSetMode");
        return TPR_SKIP;
    }    
    
    // set the sample rate Low
    DETAIL("Calling TouchPanelSetMode( TPSM_SAMPLERATE_LOW_ID, NULL )");
    if( !(*g_pfnTouchPanelSetMode)( TPSM_SAMPLERATE_LOW_ID, NULL ) )
    {
        FAIL("TouchPanelSetMode( TPSM_SAMPLERATE_LOW_ID )");
        fRet = FALSE;
     }
    
    // verify that either the sample rate was set Low or the sample rate is fixed
    DETAIL("Calling TouchPanelGetDeviceCaps( TPDC_SAMPLE_RATE_ID )");
    if( !(*g_pfnTouchPanelGetDeviceCaps)( TPDC_SAMPLE_RATE_ID, &tsr ) )
    {
        FAIL("TouchPanelGetDeviceCaps( TPSM_SAMPLE_RATE_ID )");
        fRet = FALSE;
     }
    DETAIL("Comparing sample rate settings");
    if( (0 != tsr.CurrentSampleRateSetting) &&
        (tsr.SamplesPerSecondLow != tsr.SamplesPerSecondHigh) )
    {
        FAIL("TouchPanelSetMode( TPSM_SAMPLE_RATE_LOW_ID ) did not set the sample rate to Low");
        fRet = FALSE;
     }
    
    // set the sample rate High
    DETAIL("Calling TouchPanelSetMode( TPSM_SAMPLERATE_HIGH_ID, NULL )");
    if( !(*g_pfnTouchPanelSetMode)( TPSM_SAMPLERATE_HIGH_ID, NULL ) )
    {
        FAIL("TouchPanelSetMode( TPSM_SAMPLERATE_HIGH_ID )");
        fRet = FALSE;
    }

    // verify that either the sample rate was set High or the sample rate is fixed
    DETAIL("Calling TouchPanelGetDeviceCaps( TPDC_SAMPLE_RATE_ID )");
    if( !(*g_pfnTouchPanelGetDeviceCaps)( TPDC_SAMPLE_RATE_ID, &tsr ) )
    {
        FAIL("TouchPanelGetDeviceCaps( TPSM_SAMPLE_RATE_ID )");
        fRet = FALSE;
    }
    DETAIL("Comparing sample rate settings");
    if( (1 != tsr.CurrentSampleRateSetting)  &&
        (tsr.SamplesPerSecondLow != tsr.SamplesPerSecondHigh) )
    {
        FAIL("TouchPanelSetMode( TPSM_SAMPLE_RATE_HIGH_ID ) did not set the sample rate to High");
        fRet = FALSE;
    }        

    ClearMessage( );
    return fRet ? TPR_PASS : TPR_FAIL;
}

/* ----------------------------------------------------------------------------
 * SetPriorityTest:
 * Tests TouchPanelSetMode API
 * Sets IST priority to high and normal
-----------------------------------------------------------------------------*/
TESTPROCAPI 
SetPriorityTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet = TRUE;
    TPDC_SAMPLE_RATE tsr = {0};

    //PostMessage( TEXT("Please Wait...") );    
    
    //
    // make sure the TouchPanelSetMode function was loaded
    //
    if( NULL == g_pfnTouchPanelSetMode )
    {
        DETAIL("Driver does not support TouchPanelSetMode");
        return TPR_SKIP;
    }    

    //
    // set thread priority High
    //
    DETAIL("Calling TouchPanelSetMode( TPSM_PRIORITY_HIGH_ID )");
    if( !(*g_pfnTouchPanelSetMode)( TPSM_PRIORITY_HIGH_ID, NULL ) )
    {
        FAIL("TouchPanelSetMode( TPSM_PRIORITY_HIGH_ID )");
        fRet = FALSE;
     }

    //
    // set thread priority Normal
    //
    DETAIL("Calling TouchPanelSetMode( TPSM_PRIORITY_NORMAL_ID )");
    if( !(*g_pfnTouchPanelSetMode)( TPSM_PRIORITY_NORMAL_ID, NULL ) )
    {
        FAIL("TouchPanelSetMode( TPSM_PRIORITY_NORMAL_ID )");
        fRet = FALSE;
    }   

    ClearMessage( );
    return fRet ? TPR_PASS : TPR_FAIL;
}


/* ----------------------------------------------------------------------------
 * CalibrationPointTest:
 * Tests TouchPanelReadCalibration API
 -----------------------------------------------------------------------------*/
TESTPROCAPI 
CalibrationPointTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    // make sure the TouchPanelReadCalibrationPoint function was loaded
    if( NULL == g_pfnTouchPanelReadCalibrationPoint )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationPoint");
        return TPR_SKIP;
    }  

    // make sure the TouchPanelReadCalibrationAbort function was loaded
    if( NULL == g_pfnTouchPanelReadCalibrationAbort )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationAbort");
        return TPR_SKIP;
    }  
    
    HANDLE hThread          = NULL;
    BOOL fRet               = TRUE;
    ABORT_THREAD_DATA atd   = { CALIBRATION_TIMEOUT, FALSE };
    INT32 x, y;    

    if ( InitTouchWindow( TEXT( "Read Calibration Point Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        return TPR_ABORT;
    }

    DETAIL("Creating a calibration abort thread");
    hThread = CreateThread( NULL, NULL, CalibrationAbortThreadProc, &atd, 0, NULL );
    if(NULL == hThread)
    {
        FAIL("Unable to create calibration abort thread");
        fRet = FALSE;
        goto Exit;
    }

    PostMessage( TEXT("Firmly touch screen and release") );

    DETAIL("Calling TouchPanelReadCalibrationPoint()");
    if( !(*g_pfnTouchPanelReadCalibrationPoint)( &x, &y ) )
    {
        fRet = FALSE;
        FAIL("TouchPanelReadCalibrationPoint failed");
        goto Exit;
    }
   
Exit:
    atd.fTerminate = TRUE;
    ClearMessage( );
    WaitForSingleObject( hThread, INFINITE );

    DeinitTouchWindow( TEXT( "Read Calibration Point Test" ), fRet ? TPR_PASS : TPR_FAIL );

    return fRet ? TPR_PASS : TPR_FAIL;    
}

/* ----------------------------------------------------------------------------
 * CalibrationAbortTest:
 * Tests TouchPanelReadCalibrationAbort API
 -----------------------------------------------------------------------------*/
TESTPROCAPI
CalibrationAbortTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    HANDLE hThread          = NULL;
    BOOL fRet               = TRUE;
    ABORT_THREAD_DATA atd   = { CALIBRATION_TIMEOUT / 4, FALSE };
    INT32 x, y;

    // make sure the TouchPanelReadCalibrationPoint function was loaded
    if( NULL == g_pfnTouchPanelReadCalibrationPoint )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationPoint");
        return TPR_SKIP;
    }  

    // make sure the TouchPanelReadCalibrationAbort function was loaded
    if( NULL == g_pfnTouchPanelReadCalibrationAbort )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationAbort");
        return TPR_SKIP;
    }  

    if ( InitTouchWindow( TEXT( "Calibration Abort Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        return TPR_ABORT;
    }

    PostMessage(TEXT("Do not touch the screen"));
    DETAIL("Creating a calibration abort thread");
    hThread = CreateThread( NULL, NULL, CalibrationAbortThreadProc, &atd, 0, NULL );
    if(NULL == hThread)
    {
        FAIL("Unable to create calibration abort thread");
        fRet = FALSE;
        goto Exit;
    }

    DETAIL("Calling TouchPanelReadCalibrationPoint() (expecting failure)");
    if( (*g_pfnTouchPanelReadCalibrationPoint)( &x, &y ) )
    {
        // this call will not succeed if the user doesn't touch the panel
        // and the abort thread calls TouchPanelReadCalibrationAbort
        fRet = FALSE;
        atd.fTerminate = TRUE;
        FAIL("TouchPanelReadCalibrationAbort");
        goto Exit;
    }
   
Exit:
    ClearMessage( );
    WaitForSingleObject( hThread, INFINITE );

    DeinitTouchWindow( TEXT( "Calibration Abort Test" ), fRet ? TPR_PASS : TPR_FAIL );


    return fRet ? TPR_PASS : TPR_FAIL;    
}

/* ----------------------------------------------------------------------------
 * CalibrationTest:
 * Tests TouchPanelSetCalibration API
 -----------------------------------------------------------------------------*/
TESTPROCAPI 
CalibrationTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD dwResult;

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }
    
    // make sure the TouchPanelReadCalibrationPoint function was loaded
    if( NULL == g_pfnTouchPanelReadCalibrationPoint )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationPoint");
        return TPR_SKIP;
    }  

    // make sure the TouchPanelReadCalibrationAbort function was loaded
    if( NULL == g_pfnTouchPanelReadCalibrationAbort )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationAbort");
        return TPR_SKIP;
    }  

    // make sure the TouchPanelGetDeviceCaps function was loaded
    if( NULL == g_pfnTouchPanelGetDeviceCaps )
    {
        DETAIL("Driver does not support TouchPanelGetDeviceCaps");
        return TPR_SKIP;
    }

    // make sure the TouchPanelSetCalibration function was loaded
    if( NULL == g_pfnTouchPanelSetCalibration )
    {
        DETAIL("Driver does not support TouchPanelSetCalibration");
        return TPR_SKIP;
    }  

    if ( InitTouchWindow( TEXT( "Calibration Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        return TPR_ABORT;
    }
    
    // simply call the calibration routine and force calibration
    dwResult = CalibrateScreen( TRUE ) ? TPR_PASS : TPR_FAIL;
    
    DeinitTouchWindow( TEXT( "Calibration Test" ), dwResult );

    return dwResult;
}

//TODO: Check with Dev to see does it make sense to run the NullParamTest

/* ----------------------------------------------------------------------------
 * NullParamTest:
 * Pass null parameters to API
 -----------------------------------------------------------------------------*/
TESTPROCAPI 
NullParamTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet               = TRUE;
    HANDLE hThread          = INVALID_HANDLE_VALUE;
    ABORT_THREAD_DATA atd   = { CALIBRATION_TIMEOUT, FALSE };

    // make sure the TouchPanelReadCalibrationPoint function was loaded
    if( NULL == g_pfnTouchPanelReadCalibrationPoint )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationPoint");
    }    

    // check TouchPanelReadCalibrationPoint using NULL parameters
    else   
    {
        PostTitle( TEXT("Firmly touch screen and release") );
        
        // create an abort thread to kill it if necessary
        hThread = CreateThread( NULL, NULL, CalibrationAbortThreadProc, &atd, 0, NULL );
        __try
        {
            // This test messes up the system
            if( (*g_pfnTouchPanelReadCalibrationPoint)( NULL, NULL ) )
            {                
                // what should the return value be? guessing FALSE
                // FALSE will also occur if the abort thread kills it... 
                FAIL("TouchPanelReadCalibrationPoint succeeded with NULL parameters");
                fRet = FALSE;
            }
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            FAIL("TouchPanelReadCalibrationPoint threw an exception when passed NULL parameters");
            fRet = FALSE;        
        }
        // kill the thread
        atd.fTerminate = TRUE;
        WaitForSingleObject( hThread, INFINITE );
    }

    // make sure the TouchPanelSetCalibration function was loaded
    if( NULL == g_pfnTouchPanelSetCalibration )
    {
        DETAIL("Driver does not support TouchPanelSetCalibration");
    }   
    
    // check TouchPanelSetCalibration using NULL parameters
    else
    {
        __try
        {
            // 
            // the function should not succeed with NULL params, it should also
            // not throw an exeption
            //
            if( (*g_pfnTouchPanelSetCalibration)( 5, NULL, NULL, NULL, NULL ) )
            {
                FAIL("TouchPanelSetCalibration succeeded with NULL parameters");                
                fRet = FALSE;
            }
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            FAIL("TouchPanelSetCalibration threw an exception when passed NULL parameters");
            fRet = FALSE;
        }
    }

    // make sure the TouchPanelGetDeviceCaps function was loaded
    if( NULL == g_pfnTouchPanelGetDeviceCaps )
    {
        DETAIL("Driver does not support TouchPanelGetDeviceCaps");
    }   

    // check TouchPanelGetDeviceCaps using NULL parameters
    else
    {
        __try
        {
            // the function should not succeed with NULL params, it should also
            // not throw an exeption -- test it with each possible query type
            if( (*g_pfnTouchPanelGetDeviceCaps)( TPDC_CALIBRATION_POINT_COUNT_ID, NULL ) )
            {
                FAIL("TouchPanelGetDeviceCaps( TPDC_CALIBRATION_POINT_COUNT_ID ) succeeded with NULL parameters");                
                fRet = FALSE;
            }

            if( (*g_pfnTouchPanelGetDeviceCaps)( TPDC_CALIBRATION_POINT_ID, NULL ) )
            {
                FAIL("TouchPanelGetDeviceCaps( TPDC_CALIBRATION_POINT_ID ) succeeded with NULL parameters");                
                fRet = FALSE;
            }

            if( (*g_pfnTouchPanelGetDeviceCaps)( TPDC_SAMPLE_RATE_ID, NULL ) )
            {
                FAIL("TouchPanelGetDeviceCaps( TPDC_SAMPLE_RATE_ID ) succeeded with NULL parameters");                
                fRet = FALSE;
            }
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            FAIL("TouchPanelGetDeviceCaps threw an exception when passed NULL parameter");
            fRet = FALSE;
        }
    }

    // make sure the TouchPanelCalibrateAPoint function was loaded
    if( NULL == g_pfnTouchPanelCalibrateAPoint )
    {
        DETAIL("Driver does not support TouchPanelCalibrateAPoint");
    }   

    // check TouchPanelCalibrateAPoint using NULL parameters
    else
    {
        __try
        {
            // the function should not throw an exception
            (*g_pfnTouchPanelCalibrateAPoint)( 0, 0, NULL, NULL );            
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            FAIL("TouchPanelCalibrateAPoint threw an exception when passed NULL parameter");
            fRet = FALSE;
        }
    }
   
    return fRet ? TPR_PASS : TPR_FAIL;
}

/* ---------------------------------------------------------------------------
* Tests TouchPanelInitializeCursor function
-----------------------------------------------------------------------------*/ 
TESTPROCAPI 
InitCursorTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{    
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet = TRUE;    

    if( NULL == g_pfnTouchPanelInitializeCursor )
    {
        DETAIL("Driver does not support TouchPanelInitializeCursor");
        return TPR_SKIP;
    }    

    DETAIL("Calling TouchPanelInitializeCursor()");
    if( !(*g_pfnTouchPanelInitializeCursor)(
            (PFN_DISP_DRIVER_MOVE_CURSOR)TouchPanelCallback) )
    {
        FAIL("TouchPanelInitialzieCursor");
        fRet = FALSE;
        goto Exit;
    }
    
Exit:       
    ClearMessage( );
    return fRet ? TPR_PASS : TPR_FAIL;
}    


// --------------------------------------------------------------------
TESTPROCAPI 
CalibrateAPointTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
{    
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwResult = TPR_PASS;
    INT32 uncalXY   = 0;
    INT32 calX      = 0;
    INT32 calY      = 0;   

    if( NULL == g_pfnTouchPanelCalibrateAPoint )
    {
        DETAIL("Driver does not support TouchPanelCalibrateAPoint");
        return TPR_SKIP;
    } 

    // make sure the screen is calibrated properly to start
    
    if ( InitTouchWindow( TEXT( "Calibration Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        return TPR_ABORT;
    }
    if( !CalibrateScreen( FALSE ) )
    {
        FAIL("Calibration failed, aborting test");
        return TPR_ABORT;
    }
    
    __try
    {
        // loop through some numbers to calibrate
        for( uncalXY = -5000; uncalXY < 5000; uncalXY += 100 )
        {        
            (*g_pfnTouchPanelCalibrateAPoint)( uncalXY, uncalXY, &calX, &calY );      
        }
    }

    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        FAIL("TouchPanelCalibratAPoint caused exception");
        dwResult = TPR_FAIL;
    }
    
    DeinitTouchWindow( TEXT( "Calibrate A Point Test" ), dwResult );
    return dwResult;
    
}   
