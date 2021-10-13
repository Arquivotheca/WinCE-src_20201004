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

#define __FILE_NAME__   TEXT("DEPTHTEST.CPP")

// --------------------------------------------------------------------
TESTPROCAPI 
NullParamTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------   
{

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet               = TRUE;
    HANDLE hThread          = INVALID_HANDLE_VALUE;
    ABORT_THREAD_DATA atd   = { CALIBRATION_TIMEOUT, FALSE };

    //
    // make sure the TouchPanelReadCalibrationPoint function was loaded
    //
    if( NULL == g_pfnTouchPanelReadCalibrationPoint )
    {
        DETAIL("Driver does not support TouchPanelReadCalibrationPoint");
    }    

    //
    // check TouchPanelReadCalibrationPoint using NULL parameters
    //
    else   
    {
        PostTitle( TEXT("Firmly touch the screen and release") );
        //
        // create an abort thread to kill it if necessary
        //
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

    //
    // make sure the TouchPanelSetCalibration function was loaded
    //
    if( NULL == g_pfnTouchPanelSetCalibration )
    {
        DETAIL("Driver does not support TouchPanelSetCalibration");
    }   

    //
    // check TouchPanelSetCalibration using NULL parameters
    //
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

    //
    // make sure the TouchPanelGetDeviceCaps function was loaded
    //
    if( NULL == g_pfnTouchPanelGetDeviceCaps )
    {
        DETAIL("Driver does not support TouchPanelGetDeviceCaps");
    }   

    //
    // check TouchPanelGetDeviceCaps using NULL parameters
    //
    else
    {
        __try
        {
            // 
            // the function should not succeed with NULL params, it should also
            // not throw an exeption -- test it with each possible query type
            //
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

    //
    // make sure the TouchPanelCalibrateAPoint function was loaded
    //
    if( NULL == g_pfnTouchPanelCalibrateAPoint )
    {
        DETAIL("Driver does not support TouchPanelCalibrateAPoint");
    }   

    //
    // check TouchPanelCalibrateAPoint using NULL parameters
    //
    else
    {
        __try
        {
            // 
            // the function should not throw an exception
            //
            (*g_pfnTouchPanelCalibrateAPoint)( 0, 0, NULL, NULL );            
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            FAIL("TouchPanelCalibrateAPoint threw an exception when passed NULL parameter");
            fRet = FALSE;
        }
    }

    // TODO: check TouchPanelInitializeCursor
   
    return fRet ? TPR_PASS : TPR_FAIL;
}
