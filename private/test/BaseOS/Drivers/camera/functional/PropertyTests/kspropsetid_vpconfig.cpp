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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: WMFunctional_Tests_No_Media.cpp
//          Contains Camera Driver Property tests
//          CSPROPERTYSETID_VPCONFIG
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include "logging.h"
#include "Globals.h"
#include "CameraDriverTest.h"

////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_NUMCONNECTINFO
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_NUMCONNECTINFO
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VPCONFIG_NUMCONNECTINFO.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_NUMCONNECTINFO( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    DWORD        dwNumConnectInfo= 0 ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_NUMCONNECTINFO ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &dwNumConnectInfo, 
                                            sizeof( DWORD ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO : TestDeviceIOControl failed. Unable to get CSPROPERTY_VPCONFIG_NUMCONNECTINFO  " ) ) ;
        goto done ;
    }

    if ( 1 == dwNumConnectInfo )
    {
        DETAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO : Videoport is in use" ) ) ;
    }
    else
    {
        DETAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO : Videoport is not in use or no videoport available" ) ) ;
    }

done:    
    Log( TEXT( "Test_VPCONFIG_NUMCONNECTINFO : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_NUMCONNECTINFO2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_NUMCONNECTINFO2
//
//  Assertion:        
//
//  Description:    1:     Test CSPROPERTY_VPCONFIG_NUMCONNECTINFO. 
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_NUMCONNECTINFO2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO2: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    // Let us try to set the number of pin types to 3
    DWORD        dwNumConnectInfo= 3 ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_NUMCONNECTINFO ;
    csProp.Flags    = CSPROPERTY_TYPE_SET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &dwNumConnectInfo, 
                                            sizeof( DWORD ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO2 : Should not have been able to set CSPROPERTY_VPCONFIG_NUMCONNECTINFO " ) ) ;
        goto done ;
    }


done:    
    Log( TEXT( "Test_VPCONFIG_NUMCONNECTINFO2 : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_NUMCONNECTINFO3
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_NUMCONNECTINFO3
//
//  Assertion:        
//
//  Description:    3: Test CSPROPERTY_VPCONFIG_NUMCONNECTINFO. Verify that it handles invalid output buffers correctly.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_NUMCONNECTINFO3( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_NUMCONNECTINFO ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            NULL, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 :  Querying for CSPROPERTY_VPCONFIG_NUMCONNECTINFO should have failed incase of NULL output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            NULL, 
                                            0, 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 :  Querying for CSPROPERTY_VPCONFIG_NUMCONNECTINFO should have failed incase of NULL output buffer  " ) ) ;
    }

    BOOL bTestVar = FALSE ;
    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &bTestVar, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 :  Querying for CSPROPERTY_VPCONFIG_NUMCONNECTINFO should have failed incase of BOOL output buffer  " ) ) ;
    }


    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &bTestVar, 
                                            sizeof( BOOL ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 :  Querying for CSPROPERTY_VPCONFIG_NUMCONNECTINFO should have failed incase of BOOL output buffer  " ) ) ;
    }

    WORD wTestVar = 0 ;
    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &wTestVar, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 :  Querying for CSPROPERTY_VPCONFIG_NUMCONNECTINFO should have failed incase of WORD output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &wTestVar, 
                                            sizeof( WORD ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 :  Querying for CSPROPERTY_VPCONFIG_NUMCONNECTINFO should have failed incase of WORD output buffer  " ) ) ;
    }

    ULONG64 ul64TestVar = 0 ;
    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &ul64TestVar, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 :  Querying for CSPROPERTY_VPCONFIG_NUMCONNECTINFO should have failed incase of WORD output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &ul64TestVar, 
                                            sizeof( ULONG64 ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 :  Querying for CSPROPERTY_VPCONFIG_NUMCONNECTINFO should have failed incase of WORD output buffer  " ) ) ;
    }

done:    
    Log( TEXT( "Test_VPCONFIG_NUMCONNECTINFO3 : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_SURFACEPARAMS
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_SURFACEPARAMS
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VPCONFIG_SURFACEPARAMS. This is a WRITE-only property
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_SURFACEPARAMS( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_SURFACEPARAMS: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_SURFACEPARAMS : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    CSVPSURFACEPARAMS    pCSVPSurfaceParams;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_SURFACEPARAMS ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &pCSVPSurfaceParams, 
                                            sizeof( pCSVPSurfaceParams ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_SURFACEPARAMS : TestDeviceIOControl failed. Get request CSPROPERTY_VPCONFIG_SURFACEPARAMS should be disallowed. This is a Write-Only property " ) ) ;
        goto done ;
    }


done:    
    Log( TEXT( "Test_VPCONFIG_SURFACEPARAMS : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_SURFACEPARAMS2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_SURFACEPARAMS2
//
//  Assertion:        
//
//  Description:    1:     Test CSPROPERTY_VPCONFIG_SURFACEPARAMS for writability
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_SURFACEPARAMS2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_SURFACEPARAMS2: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_SURFACEPARAMS2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    // Let us try to set the number of pin types to 3
    CSVPSURFACEPARAMS    pCSVPSurfaceParams;
    pCSVPSurfaceParams.dwPitch    = 300 ;
    pCSVPSurfaceParams.dwXOrigin    = 10 ;
    pCSVPSurfaceParams.dwYOrigin    = 20 ;
    DWORD        dwBytesReturned = 0 ;

    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_SURFACEPARAMS ;
    csProp.Flags    = CSPROPERTY_TYPE_SET ;

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &pCSVPSurfaceParams, 
                                            sizeof( CSVPSURFACEPARAMS ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_SURFACEPARAMS2 : Should not have been able to set CSPROPERTY_VPCONFIG_SURFACEPARAMS " ) ) ;
        goto done ;
    }

done:    
    Log( TEXT( "Test_VPCONFIG_SURFACEPARAMS2 : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_SCALEFACTOR
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_SCALEFACTOR
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VPCONFIG_SCALEFACTOR. This is a WRITE-only property
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_SCALEFACTOR( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_SCALEFACTOR: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_SCALEFACTOR : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    CS_AMVPSIZE csVPSurfaceParams;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_SCALEFACTOR ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csVPSurfaceParams, 
                                            sizeof( csVPSurfaceParams ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_SCALEFACTOR : TestDeviceIOControl failed. Get request CSPROPERTY_VPCONFIG_SCALEFACTOR should be disallowed. This is a Write-Only property " ) ) ;
        goto done ;
    }


done:    
    Log( TEXT( "Test_VPCONFIG_SCALEFACTOR : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_SCALEFACTOR2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_SCALEFACTOR2
//
//  Assertion:        
//
//  Description:    1:     Test CSPROPERTY_VPCONFIG_SCALEFACTOR for writability
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_SCALEFACTOR2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_SCALEFACTOR2: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_SCALEFACTOR2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    // Let us try to set the number of pin types to 3
    CS_AMVPSIZE csVPSurfaceParams;
    DWORD        dwBytesReturned = 0 ;

    csVPSurfaceParams.dwHeight    = 40 ;
    csVPSurfaceParams.dwWidth        = 80 ;
    
    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_SCALEFACTOR ;
    csProp.Flags    = CSPROPERTY_TYPE_SET ;

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &pCSVPSurfaceParams, 
                                            sizeof( CSVPSURFACEPARAMS ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_SCALEFACTOR2 : Should not have been able to set CSPROPERTY_VPCONFIG_SCALEFACTOR " ) ) ;
        goto done ;
    }

done:    
    Log( TEXT( "Test_VPCONFIG_SCALEFACTOR2 : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_DECIMATIONCAPABILITY
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_DECIMATIONCAPABILITY
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_DECIMATIONCAPABILITY( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    BOOL        isCapable = TRUE ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &isCapable, 
                                            sizeof( isCapable ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY : TestDeviceIOControl failed. Unable to get CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY  " ) ) ;
        goto done ;
    }

    if ( TRUE == isCapable )
    {
        DETAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY : The video image can be reduced in size." ) ) ;
    }
    else
    {
        DETAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY : The image can still be resized, but would also be clipped to the rectangle instead of scaled." ) ) ;
    }

done:    
    Log( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_DECIMATIONCAPABILITY2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_DECIMATIONCAPABILITY2
//
//  Assertion:        
//
//  Description:    1:     Test CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY. 
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_DECIMATIONCAPABILITY2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY2: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    // Let us try to set the number of pin types to 3
    BOOL        bIsCapable = TRUE ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY ;
    csProp.Flags    = CSPROPERTY_TYPE_SET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &bIsCapable, 
                                            sizeof( bIsCapable ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY2 : Should not have been able to set CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY " ) ) ;
        goto done ;
    }


done:    
    Log( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY2 : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VPCONFIG_DECIMATIONCAPABILITY3
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VPCONFIG_DECIMATIONCAPABILITY3
//
//  Assertion:        
//
//  Description:    3: Test CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY. Verify that it handles invalid output buffers correctly.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VPCONFIG_DECIMATIONCAPABILITY3( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = CSPROPERTYSETID_VPCONFIG ;
    csProp.Id        = CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            NULL, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 :  Querying for CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY should have failed incase of NULL output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            NULL, 
                                            0, 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 :  Querying for CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY should have failed incase of NULL output buffer  " ) ) ;
    }

    BOOL bTestVar = FALSE ;
    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &bTestVar, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 :  Querying for CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY should have failed incase of BOOL output buffer  " ) ) ;
    }


    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &bTestVar, 
                                            sizeof( BOOL ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 :  Querying for CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY should have failed incase of BOOL output buffer  " ) ) ;
    }

    WORD wTestVar = 0 ;
    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &wTestVar, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 :  Querying for CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY should have failed incase of WORD output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &wTestVar, 
                                            sizeof( WORD ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 :  Querying for CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY should have failed incase of WORD output buffer  " ) ) ;
    }

    ULONG64 ul64TestVar = 0 ;
    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &ul64TestVar, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 :  Querying for CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY should have failed incase of WORD output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &ul64TestVar, 
                                            sizeof( ULONG64 ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 :  Querying for CSPROPERTY_VPCONFIG_DECIMATIONCAPABILITY should have failed incase of WORD output buffer  " ) ) ;
    }

done:    
    Log( TEXT( "Test_VPCONFIG_DECIMATIONCAPABILITY3 : Test Completed. " ) ) ;
    return GetTestResult();
}

