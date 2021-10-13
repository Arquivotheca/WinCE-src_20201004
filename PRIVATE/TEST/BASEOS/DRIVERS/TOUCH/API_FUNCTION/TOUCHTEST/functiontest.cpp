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

#include "globals.h"

#define __FILE_NAME__   TEXT("FUNCTIONTEST.CPP")

// --------------------------------------------------------------------
TESTPROCAPI 
PenUpDownTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------    
{
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    TOUCH_POINT_DATA tpd        = {0};
    TOUCH_POINT_DATA tpdPrev    = {0};
    DWORD dwTime                = 0;
    BOOL fRet                   = FALSE;
    BOOL fPenDown               = FALSE;        

    if ( InitTouchWindow( TEXT( "Pen Up / Down Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        return TPR_ABORT;
    }

    PostMessage( TEXT("Touch the pen to the Touch Panel") );
    g_touchPointQueue.Clear(); 
    dwTime = GetTickCount();
    do 
    {
        switch( fPenDown )
        {   
        case FALSE: // wait for a pen down
            
            if( g_touchPointQueue.Dequeue( &tpd ) )
            {
                if( PEN_WENT_DOWN & tpd.dwTouchEventFlags )
                {
                    PostMessage( TEXT("Lift the pen from the Touch Panel") );
                    fPenDown = TRUE;       
                    dwTime = GetTickCount(); // reset the time
                }                    
            }
            break;

        case TRUE: // wait for a pen up

            if( g_touchPointQueue.Dequeue( &tpd ) )
            {
                if( PEN_WENT_UP & tpd.dwTouchEventFlags )
                {
                    fRet = TRUE;
                    goto Exit;
                }
            }
            break;

        default:
            ASSERT(!"Default case never reached");
            break;
        }                    
    } 
    while ( (GetTickCount() - dwTime) < TEST_TIME );      
    
Exit:
    ClearMessage( );   

    DeinitTouchWindow( TEXT( "Pen Up / Down Test" ), fRet ? TPR_PASS : TPR_FAIL );

    return fRet ? TPR_PASS : TPR_FAIL;
}    


// --------------------------------------------------------------------
TESTPROCAPI 
PointTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )

#define TOUCH_REPEATS       10    
#define POINT_TEST_TIMEOUT  20000
// --------------------------------------------------------------------   
{    
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    BOOL fRet               = TRUE;
    BOOL fDone              = FALSE;
    DWORD dwTouchCount      = 0;
    INT nTime               = 0;
    TOUCH_POINT_DATA tpd    = {0};


    if ( InitTouchWindow( TEXT( "Point Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        return TPR_ABORT;
    }

    if( !CalibrateScreen( FALSE ) )
    {
        fRet = FALSE;
        FAIL("Calibration failed, aborting test");
        DeinitTouchWindow( TEXT( "Point Test" ), TPR_ABORT );
        return TPR_ABORT;
    }
    
    PostTitle( TEXT("Touch the screen in %d different locations"), TOUCH_REPEATS );
    nTime = GetTickCount();
    do
    {
        if( g_touchPointQueue.Dequeue( &tpd ) )
        {
            if( PEN_WENT_DOWN & tpd.dwTouchEventFlags )
            {
                SetCrosshair( tpd.x, tpd.y, TRUE );
                dwTouchCount++;
                PostMessage( TEXT("%d more times"), TOUCH_REPEATS - dwTouchCount );
                nTime = GetTickCount(); // reset the timer
            }                       
        }
    }
    while( (TOUCH_REPEATS > dwTouchCount) &&
           (POINT_TEST_TIMEOUT > (GetTickCount() - nTime)) );

    HideCrosshair( );
    ClearMessage( );
    
    if( dwTouchCount != TOUCH_REPEATS )
    { 
        fRet = FALSE;
        goto Exit;
    }
    
    PostTitle( TEXT("Did the crosshair appear at the correct points?") );
    fRet = WaitForInput( USER_WAIT_TIME, FALSE );

Exit:
    DeinitTouchWindow( TEXT( "Point Test" ), fRet ? TPR_PASS : TPR_FAIL );

    return fRet ? TPR_PASS : TPR_FAIL;
} 

// --------------------------------------------------------------------
TESTPROCAPI 
DrawingTest( 
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )

#define DRAWING_TEST_TIME  15
// --------------------------------------------------------------------   
{    
    DWORD dwResult; 

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    if ( InitTouchWindow( TEXT( "Drawing Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        return TPR_ABORT;
    }


    switch ( DrawOnScreen( DRAWING_TEST_TIME ) )
    {
        case TRUE:
            dwResult = TPR_PASS;
            break;

        case FALSE:
            dwResult = TPR_FAIL;
            break;

        default:
            dwResult = TPR_ABORT;
    }

    DeinitTouchWindow( TEXT( "Drawing Test" ), dwResult );
    return dwResult;
} 

/*
 * DrawOnScreen:  Draw on screen tests
 *
 * ARGUEMNTS:
 *    unDrawTestTime:  Length in seconds to allow test to run
 */
BOOL DrawOnScreen( unsigned int unDrawTestTime )
{
    BOOL fRet               = FALSE;
    DWORD dwTime            = 0;
    TOUCH_POINT_DATA tpd    = {0};
    POINT point             = {0};

   if( !CalibrateScreen( FALSE ) )
    {
        FAIL("Calibration failed, aborting test");
        return EOF;
    } 

    PostTitle( TEXT("Draw on the screen,Test times out after a while.") );  

    SetDrawPen( TRUE );
    dwTime = GetTickCount();
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
            else if( TouchSampleDown( tpd.flags ) )
            {
                DrawLineTo( point );
            }
        }
    }
    while( unDrawTestTime*1000 > (GetTickCount() - dwTime) );
    SetDrawPen( FALSE );
    
    ClearDrawing( );
    ClearMessage( );

    //
    // if any drawing was done, prompt the user for verification
    //
    if( fRet )
    {
        PostTitle( TEXT("Did drawing work correctly?") );
        fRet = WaitForInput( USER_WAIT_TIME, FALSE );
    }

    return fRet;
}