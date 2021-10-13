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
// faultinjection.cpp:  "Fault injection" test functions for the Touch PQD driver
//
// Test Functions: 
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

#include <windows.h>
#include <devload.h>
#include <ceddk.h>
#include <excpt.h>

#include "..\globals.h"
#include "..\winmain.h"

static BOOL DeactivateTouchDriver( void );
static BOOL ReactivateTouchDriver( void );

DEVMGR_DEVICE_INFORMATION g_ddiTouchDriver;
DEVMGR_DEVICE_INFORMATION g_ddiParent;
HANDLE                    g_hndParent;


//-----------------------------------------------------------------------------
// TouchDriverUnloadedTest:
// Verifies if touch panels functionality is lost when the driver is unloaded
//-----------------------------------------------------------------------------
TESTPROCAPI TouchDriverUnloadedTest( 
            UINT                   uMsg, 
            TPPARAM                tpParam, 
            LPFUNCTION_TABLE_ENTRY lpFTE )
{
    INT    TP_Status = TPR_FAIL;

    BOOL fRet               = FALSE;
    DWORD dwTime            = 0;
    TOUCH_POINT_DATA tpd    = {0};
    POINT point             = {0};
        
    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }
    if ( InitTouchWindow( TEXT( "Touch Driver Unloaded Test" ) ) == FALSE )
    {
        FAIL( "Error creating test window" );
        return TPR_ABORT;
    }

    if( !CalibrateScreen( FALSE ) )
    {
        FAIL("Calibration failed, aborting test");
        goto Exit;
    } 
    PostMessage( TEXT( "Deactivating touch driver ...\nExpect drawing test to fail" ) );

    Sleep(3000);

    //Deactivate touch driver and begin drawing test
    if ( DeactivateTouchDriver() == FALSE )
    {
        FAIL( "Error unloading touch driver" );
        goto Exit;
    }

    ClearMessage( );

    DrawOnScreen( SHORT_DRAWING_TEST_TIME, false );

    if ( ReactivateTouchDriver() == FALSE )
    {
        FAIL( "Unable to reload touch driver" );
        goto Exit;
    }
    
    PostTitle( TEXT("Drawing failed?") );
    fRet = WaitForInput( USER_WAIT_TIME);
  
    
    ClearDrawing( );
    ClearMessage( );

    if ( fRet == FALSE )
    {
            FAIL( "Drawing test has PASSED when expecting draw operation to be non-functional" );
            goto Exit;
    }
    else
        TP_Status = TPR_PASS;
    
Exit:
    DeinitTouchWindow( TEXT( "Touch Driver Unloaded Test" ), TP_Status );
    return TP_Status;
}


//-----------------------------------------------------------------------------
// DeactivateTouchDriver:  
// Unloads touch driver by finding the parent bus driver and call its 
// IOCTL_BUS_DEACTIVATE_CHILD service 
// RETURNS:
//  TRUE:   If successful
//  FALSE:  If otherwsie
//-----------------------------------------------------------------------------
BOOL DeactivateTouchDriver( void )
{
    HANDLE hndFindDrvr;
    int    nError;
    BOOL   bResult;
    DWORD  dwBytes;

    TCHAR szBusName[ MAX_PATH ];

    //Get device driver info for touch driver
    memset( &g_ddiTouchDriver, 0, sizeof( DEVMGR_DEVICE_INFORMATION ) );

    g_ddiTouchDriver.dwSize = sizeof( DEVMGR_DEVICE_INFORMATION );

    if ( ( hndFindDrvr = FindFirstDevice( DeviceSearchByLegacyName,
                                          L"TCH?:",
                                          &g_ddiTouchDriver ) ) == INVALID_HANDLE_VALUE )
    {
        nError = GetLastError();
        FAIL( "Could not get device information for touch driver" );
        g_pKato->Log( LOG_DETAIL, "Error code = %d", nError );
        return FALSE;
    }

    FindClose( hndFindDrvr );
   
    //Get info on parent bus driver
    if ( g_ddiTouchDriver.hParentDevice == NULL )
    {
        FAIL( "No parent bus driver - Unable to unload touch driver" );
        return FALSE;
    }

    memset( &g_ddiParent, 0, sizeof( DEVMGR_DEVICE_INFORMATION ) );

    g_ddiParent.dwSize = sizeof( DEVMGR_DEVICE_INFORMATION );

    if( GetDeviceInformationByDeviceHandle( g_ddiTouchDriver.hParentDevice, 
                                           &g_ddiParent )              == FALSE )
    {
        nError = GetLastError();
        FAIL( "Could not get device information on parent device for touch driver" );
        g_pKato->Log( LOG_DETAIL, "Error code = %d", nError );
        return FALSE;
    }

    //Open handleto parent bus driver using the device name in the $bus namespace
    if ( ( g_hndParent = CreateFile( g_ddiParent.szBusName, 
                                     0, 
                                     0, 
                                     NULL, 
                                     OPEN_EXISTING, 
                                     0, 
                                     NULL ) ) == NULL )
    {
        nError = GetLastError();
        FAIL( "Could not get the file handle for parent device" );
        g_pKato->Log( LOG_DETAIL, "Error code = %d", nError );
        return FALSE;

    }

    //Deactivate touch driver using name from bus namespace
    wcscpy_s( szBusName, _countof(szBusName), &( g_ddiTouchDriver.szBusName[ 5 ] ) ); //Cut off "$bus\"
    dwBytes = ( wcslen( szBusName ) + 1 ) * sizeof( TCHAR );  

    if ( ( bResult = DeviceIoControl( g_hndParent, 
                                      IOCTL_BUS_DEACTIVATE_CHILD, 
                                      ( LPVOID ) szBusName, 
                                      dwBytes, 
                                      NULL, 
                                      0, 
                                      NULL, 
                                      NULL               ) ) == FALSE )
    {
        nError = GetLastError();
        FAIL( "Could not deactivate touch driver" );
        g_pKato->Log( LOG_DETAIL, "Error code = %d", nError );

        CloseHandle( g_hndParent );

        return FALSE;

    }

    DETAIL( "Touch driver successfully deactivated" );
    return TRUE;
}

//-----------------------------------------------------------------------------
// RectivateTouchDriver:   
// Reloads touch driver by using the parent bus driver and call to its 
// IOCTL_BUS_ACTIVATE_CHILD service 
// RETURNS:
//  TRUE:   If successful
//  FALSE:  If otherwsie
//-----------------------------------------------------------------------------
BOOL ReactivateTouchDriver( void )
{
    int    nError;
    BOOL   bResult = TRUE;
    DWORD  dwBytes;

    TCHAR szBusName[ MAX_PATH ];

    wcscpy_s( szBusName, _countof(szBusName), &( g_ddiTouchDriver.szBusName[ 5 ] ) ); //Cut off "$bus\"
    dwBytes = ( wcslen( szBusName ) + 1 ) * sizeof( TCHAR );  

    if ( ( bResult = DeviceIoControl( g_hndParent, 
                                      IOCTL_BUS_ACTIVATE_CHILD, 
                                      ( LPVOID ) szBusName, 
                                      dwBytes, 
                                      NULL, 
                                      0, 
                                      NULL, 
                                      NULL               ) ) == FALSE )
    {
        nError = GetLastError();
        FAIL( "Could not reactivate touch driver" );
        g_pKato->Log( LOG_DETAIL, "Error code = %d", nError );
    }
    else
        DETAIL( "Touch driver successfully reactivated" );

    CloseHandle( g_hndParent );

    return bResult;

}
