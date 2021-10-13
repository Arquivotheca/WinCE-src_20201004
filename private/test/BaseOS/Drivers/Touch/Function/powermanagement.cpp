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
// PDD_Performace.cpp:  Power Management test functions for the Touch PQD driver
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

#include <pwrtstutil.h> // for power state simulation
#include <pkfuncs.h>
#include "..\globals.h"
#include "..\winmain.h"


HANDLE g_RTCAlarm         = NULL;
BOOL   g_bRTCWakeupSource = FALSE;

/*
 * TouchGetPowerCapabilitiesTest:  
 * Verifies Touch Panel responds to Power Capabailities query
 * Verifies power state D0 and D4 are supported
 */
TESTPROCAPI TouchGetPowerCapabilitiesTest( 
            UINT                   uMsg, 
            TPPARAM                tpParam, 
            LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    
    HANDLE hnd;
    int    nError;

    POWER_CAPABILITIES PowerCapabilities;
    DWORD BytesReturned;
    INT    TP_Status = TPR_FAIL;

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }


    if ( ( hnd = CreateFile(  TEXT( "TCH1:" ), 
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL          ) ) == INVALID_HANDLE_VALUE )
    {
        nError = GetLastError();
        FAIL( "Error getting handle to Touch Panel" );
        g_pKato->Log( LOG_DETAIL, "Return code = %d", nError );

        goto Exit;

    }

    if ( DeviceIoControl( hnd,
                          IOCTL_POWER_CAPABILITIES,
                          NULL,
                          0,
                          &PowerCapabilities,
                          sizeof( POWER_CAPABILITIES ),
                          &BytesReturned,
                          NULL                 )  == 0 )
    {
        nError = GetLastError();
        FAIL( "Error getting Power capabilities for the Touch Panel" );
        g_pKato->Log( LOG_DETAIL, TEXT( "Return code = %d" ), nError );

        goto Exit;

    }

    if ( PowerCapabilities.Power[ 0 ] < 0 )
    {
        FAIL( "Power state D0 is reported as not supported" );
    }
    else
    {
        if ( PowerCapabilities.Power[ 4] < 0 )
        {
            FAIL( "Power state D4 is reported as not supported" );
        }
        else
        {
            TP_Status = TPR_PASS;
        }
    }

Exit:

    return TP_Status;

}


/*
 * TouchDevicePowerStateTest:  
 * Verifies touch panel power state can be set to D4 and back to D0, 
 * Verifies touch functionality is off when device is in D4 power state
 * Verifies touch functionality is on when device is in D0 power state
 */
TESTPROCAPI TouchDevicePowerStateTest( 
            UINT                   uMsg, 
            TPPARAM                tpParam, 
            LPFUNCTION_TABLE_ENTRY lpFTE )
{
    UNREFERENCED_PARAMETER(lpFTE);
    UNREFERENCED_PARAMETER(tpParam);
    
    INT    TP_Status = TPR_FAIL;

    DWORD dwError;
    
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

    if ( InitTouchWindow( TEXT( "Device Power State Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        return TPR_ABORT;
    }

    if( !CalibrateScreen( FALSE ) )
    {
        FAIL("Calibration failed, aborting test");
        goto Exit;
    } 

    if ( DrawOnScreen( SHORT_DRAWING_TEST_TIME, true ) == FALSE )
    {
        FAIL( "First Draw on Screen test failed" );
        goto Exit;
    }

    ClearTitle();
    PostMessage( TEXT( "Device in D4 power state. \nExpect drawing test to fail" ) );
    Sleep( 3000 );

    if ( ( dwError = SetDevicePower( TEXT( "TCH1:" ),
                                                      POWER_NAME,
                                                      D4   ) ) != ERROR_SUCCESS )
    {
        FAIL( "Failed to set device power state to D4" );
        g_pKato->Log( LOG_DETAIL, TEXT( "Return code = %d" ), dwError );
        goto Exit;
    }

    ClearMessage( );

    DrawOnScreen( SHORT_DRAWING_TEST_TIME, false );
   
    //need to restore power state to D0 here so that user can click yes or no.
    if ( ( dwError = SetDevicePower( TEXT( "TCH1:" ),
                                     POWER_NAME,
                                     D0   ) ) != ERROR_SUCCESS )
    {
        FAIL( "Failed to set device power state to D0" );
        g_pKato->Log( LOG_DETAIL, TEXT( "Return code = %d" ), dwError );
        goto Exit;
    }

    PostTitle( TEXT("Drawing failed?") );
    if ( FALSE == WaitForInput( USER_WAIT_TIME) )
    {
        FAIL( "User indicated second drawing test passed - Expected to fail" );
        goto Exit;
    }


    ClearTitle();
    PostMessage( TEXT( "Device in D0 power state. \nExpect drawing test to succeed" ) );
    Sleep( 3000 );

    ClearMessage();
    if ( DrawOnScreen( SHORT_DRAWING_TEST_TIME, true ) == FALSE )
    {
        FAIL( "Third Draw on Screen test failed" );
    }
    else
    {
        TP_Status = TPR_PASS;
    }

Exit:

    DeinitTouchWindow( TEXT( "Device Power State Test" ), TP_Status );

    return TP_Status;

}

