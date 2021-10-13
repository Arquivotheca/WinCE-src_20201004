// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------

#include "globals.h"

#define __FILE_NAME__   TEXT("APITEST.CPP")

// --------------------------------------------------------------------
TESTPROCAPI 
DeviceCapsTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------    
{

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet = TRUE;
    TPDC_SAMPLE_RATE tsr = {0};
    TPDC_CALIBRATION_POINT_COUNT tcpc = {0};

    PostMessage( TEXT("Please Wait...") );

    //
    // make sure the TouchPanelGetDeviceCaps function was loaded
    //
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

// --------------------------------------------------------------------
TESTPROCAPI 
SetSampleRateTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------    
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet = TRUE;
    TPDC_SAMPLE_RATE tsr = {0};

    PostMessage( TEXT("Please Wait...") );    

    //
    // make sure the TouchPanelGetDeviceCaps function was loaded
    //
    if( NULL == g_pfnTouchPanelGetDeviceCaps )
    {
        DETAIL("Driver does not support TouchPanelGetDeviceCaps");
        return TPR_SKIP;
    }

    //
    // make sure the TouchPanelSetMode function was loaded
    //
    if( NULL == g_pfnTouchPanelSetMode )
    {
        DETAIL("Driver does not support TouchPanelSetMode");
        return TPR_SKIP;
    }    

    //
    // set the sample rate Low
    //
    DETAIL("Calling TouchPanelSetMode( TPSM_SAMPLERATE_LOW_ID, NULL )");
    if( !(*g_pfnTouchPanelSetMode)( TPSM_SAMPLERATE_LOW_ID, NULL ) )
    {
        FAIL("TouchPanelSetMode( TPSM_SAMPLERATE_LOW_ID )");
        fRet = FALSE;
        goto Exit;
    }

    //
    // verify that either the sample rate was set Low or the sample rate is fixed
    //
    DETAIL("Calling TouchPanelGetDeviceCaps( TPDC_SAMPLE_RATE_ID )");
    if( !(*g_pfnTouchPanelGetDeviceCaps)( TPDC_SAMPLE_RATE_ID, &tsr ) )
    {
        FAIL("TouchPanelGetDeviceCaps( TPSM_SAMPLE_RATE_ID )");
        fRet = FALSE;
        goto Exit;
    }
    DETAIL("Comparing sample rate settings");
    if( (0 != tsr.CurrentSampleRateSetting) &&
        (tsr.SamplesPerSecondLow != tsr.SamplesPerSecondHigh) )
    {
        FAIL("TouchPanelSetMode( TPSM_SAMPLE_RATE_LOW_ID ) did not set the sample rate to Low");
        fRet = FALSE;
        goto Exit;
    }

    //
    // set the sample rate High
    //
    DETAIL("Calling TouchPanelSetMode( TPSM_SAMPLERATE_HIGH_ID, NULL )");
    if( !(*g_pfnTouchPanelSetMode)( TPSM_SAMPLERATE_HIGH_ID, NULL ) )
    {
        FAIL("TouchPanelSetMode( TPSM_SAMPLERATE_HIGH_ID )");
        fRet = FALSE;
        goto Exit;
    }

    //
    // verify that either the sample rate was set High or the sample rate is fixed
    //
    DETAIL("Calling TouchPanelGetDeviceCaps( TPDC_SAMPLE_RATE_ID )");
    if( !(*g_pfnTouchPanelGetDeviceCaps)( TPDC_SAMPLE_RATE_ID, &tsr ) )
    {
        FAIL("TouchPanelGetDeviceCaps( TPSM_SAMPLE_RATE_ID )");
        fRet = FALSE;
        goto Exit;
    }
    DETAIL("Comparing sample rate settings");
    if( (1 != tsr.CurrentSampleRateSetting)  &&
        (tsr.SamplesPerSecondLow != tsr.SamplesPerSecondHigh) )
    {
        FAIL("TouchPanelSetMode( TPSM_SAMPLE_RATE_HIGH_ID ) did not set the sample rate to High");
        fRet = FALSE;
        goto Exit;
    }        

Exit:
    ClearMessage( );
    return fRet ? TPR_PASS : TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
SetPriorityTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------    
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet = TRUE;
    TPDC_SAMPLE_RATE tsr = {0};

    PostMessage( TEXT("Please Wait...") );    
    
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
        goto Exit;
    }

    //
    // set thread priority Normal
    //
    DETAIL("Calling TouchPanelSetMode( TPSM_PRIORITY_NORMAL_ID )");
    if( !(*g_pfnTouchPanelSetMode)( TPSM_PRIORITY_NORMAL_ID, NULL ) )
    {
        FAIL("TouchPanelSetMode( TPSM_PRIORITY_NORMAL_ID )");
        fRet = FALSE;
        goto Exit;
    }   

Exit:
    ClearMessage( );
    return fRet ? TPR_PASS : TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
PowerHandlerTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )

#define POWER_TEST_TIME 10000
#define POWER_UP_TIME   2000
// --------------------------------------------------------------------    
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet               = FALSE;
    DWORD dwStartTime       = 0;
    TOUCH_POINT_DATA tpd    = {0};
    
    //
    // make sure the TouchPanelPowerHandler function was loaded
    //
    if( NULL == g_pfnTouchPanelPowerHandler )
    {
        DETAIL("Driver does not support TouchPanelPowerHandler");
        return TPR_SKIP;
    }    

    //
    // power Off touch panel
    //
    (*g_pfnTouchPanelPowerHandler)( TRUE );

    g_touchPointQueue.Clear();
    
    PostTitle( TEXT("Touch Panel Powered Off\r\n(input will be ignored)") );
    PostMessage( TEXT("Touch the Screen") );

    dwStartTime = GetTickCount();
    do
    {
        if( g_touchPointQueue.Dequeue( &tpd ) )
        {
            FAIL("Touch data received after TouchPanelPowerHandler( TRUE )");
            goto Exit;
        }
    }
    while( POWER_TEST_TIME > (GetTickCount() - dwStartTime) );

    //
    // Add a false delay so the user knows what is going on
    //
    PostTitle( TEXT("Powering On...") );
    PostMessage( TEXT("Do not touch the screen") );
    Sleep( POWER_UP_TIME );

    //
    // power On touch panel
    //
    (*g_pfnTouchPanelPowerHandler)( FALSE );    

    PostTitle( TEXT("Touch Panel Powered On") );
    PostMessage( TEXT("Touch the Screen") );    
    
    dwStartTime = GetTickCount();
    do
    {
        if( g_touchPointQueue.Dequeue( &tpd ) )
        {
            fRet = TRUE;
            goto Exit;
        }

    }
    while( POWER_TEST_TIME > (GetTickCount() - dwStartTime) );

Exit:
    ClearMessage( );
    return fRet ? TPR_PASS : TPR_FAIL;
}

// --------------------------------------------------------------------
TESTPROCAPI 
CalibrationPointTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------   
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    //
    // make sure the TouchPanelReadCalibrationPoint function was loaded
    //
    if( NULL == g_pfnTouchPanelReadCalibrationPoint )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationPoint");
        return TPR_SKIP;
    }  

    //
    // make sure the TouchPanelReadCalibrationAbort function was loaded
    //
    if( NULL == g_pfnTouchPanelReadCalibrationAbort )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationAbort");
        return TPR_SKIP;
    }  
    
    HANDLE hThread          = INVALID_HANDLE_VALUE;
    BOOL fRet               = TRUE;
    ABORT_THREAD_DATA atd   = { CALIBRATION_TIMEOUT, FALSE };
    INT32 x, y;    

    DETAIL("Creating a calibration abort thread");
    hThread = CreateThread( NULL, NULL, CalibrationAbortThreadProc, &atd, 0, NULL );
    if(INVALID_HANDLE_VALUE == hThread || NULL == hThread)
    {
        FAIL("Unable to create calibration abort thread");
        fRet = FALSE;
        goto Exit;
    }

    PostMessage( TEXT("Firmly touch the screen and release") );

    DETAIL("Calling TouchPanelReadCalibrationPoint()");
    if( !(*g_pfnTouchPanelReadCalibrationPoint)( &x, &y ) )
    {
        fRet = FALSE;
        FAIL("TouchPanelReadCalibrationPoint");
        goto Exit;
    }
   
Exit:
    atd.fTerminate = TRUE;
    ClearMessage( );
    WaitForSingleObject( hThread, INFINITE );
    return fRet ? TPR_PASS : TPR_FAIL;    
}

// --------------------------------------------------------------------
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

    HANDLE hThread          = INVALID_HANDLE_VALUE;
    BOOL fRet               = TRUE;
    ABORT_THREAD_DATA atd   = { CALIBRATION_TIMEOUT / 4, FALSE };
    INT32 x, y;

    //
    // make sure the TouchPanelReadCalibrationPoint function was loaded
    //
    if( NULL == g_pfnTouchPanelReadCalibrationPoint )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationPoint");
        return TPR_SKIP;
    }  

    //
    // make sure the TouchPanelReadCalibrationAbort function was loaded
    //
    if( NULL == g_pfnTouchPanelReadCalibrationAbort )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationAbort");
        return TPR_SKIP;
    }  

    PostMessage(TEXT("Do not touch the screen"));
    DETAIL("Creating a calibration abort thread");
    hThread = CreateThread( NULL, NULL, CalibrationAbortThreadProc, &atd, 0, NULL );
    if(INVALID_HANDLE_VALUE == hThread || NULL == hThread)
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
    
    return fRet ? TPR_PASS : TPR_FAIL;    
}

// --------------------------------------------------------------------
TESTPROCAPI 
CalibrationTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------   
{

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }
    
    //
    // make sure the TouchPanelReadCalibrationPoint function was loaded
    //
    if( NULL == g_pfnTouchPanelReadCalibrationPoint )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationPoint");
        return TPR_SKIP;
    }  

    //
    // make sure the TouchPanelReadCalibrationAbort function was loaded
    //
    if( NULL == g_pfnTouchPanelReadCalibrationAbort )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationAbort");
        return TPR_SKIP;
    }  

    //
    // make sure the TouchPanelGetDeviceCaps function was loaded
    //
    if( NULL == g_pfnTouchPanelGetDeviceCaps )
    {
        DETAIL("Driver does not support TouchPanelGetDeviceCaps");
        return TPR_SKIP;
    }

    //
    // make sure the TouchPanelSetCalibration function was loaded
    //
    if( NULL == g_pfnTouchPanelSetCalibration )
    {
        DETAIL("Driver does not support TouchPanelSetCalibration");
        return TPR_SKIP;
    }  

    
    //
    // simply call the calibration routine and force calibration
    //
    return CalibrateScreen( TRUE ) ? TPR_PASS : TPR_FAIL;
}

// --------------------------------------------------------------------
VOID
TouchCursorCallback(
    INT32 X,
    INT32 Y )
// --------------------------------------------------------------------    
{
    // do nothing
}


// --------------------------------------------------------------------
TESTPROCAPI 
InitCursorTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------   
{    
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet = TRUE;    

    PostMessage( TEXT("Please Wait...") );
   
    //
    // make sure the TouchPanelInitializeCursor function was loaded
    //
    if( NULL == g_pfnTouchPanelInitializeCursor )
    {
        DETAIL("Driver does not support TouchPanelInitializeCursor");
        return TPR_SKIP;
    }    

    DETAIL("Calling TouchPanelInitializeCursor()");
    if( !(*g_pfnTouchPanelInitializeCursor)(
            (PFN_DISP_DRIVER_MOVE_CURSOR)TouchCursorCallback) )
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
//
//  Incomplete
// --------------------------------------------------------------------   
{    
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet       = TRUE;   
    INT32 uncalXY   = 0;
    INT32 calX      = 0;
    INT32 calY      = 0;   

    PostMessage( TEXT("Please Wait...") );
    
    //
    // make sure the TouchPanelCalibrateAPoint function was loaded
    //
    if( NULL == g_pfnTouchPanelCalibrateAPoint )
    {
        DETAIL("Driver does not support TouchPanelCalibrateAPoint");
        return TPR_SKIP;
    } 

    //
    // make sure the screen is calibrated properly to start
    //
    if( !CalibrateScreen( FALSE ) )
    {
        fRet = FALSE;
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
        fRet = FALSE;
    }
    
    ClearMessage( );
    return fRet ? TPR_PASS : TPR_FAIL;
}   
