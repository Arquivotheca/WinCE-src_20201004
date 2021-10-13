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
//          PROPSETID_VIDCAP_VIDEOCONTROL
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include "logging.h"
#include "Globals.h"
#include "CameraDriverTest.h"
#include "CameraSpecs.h"

////////////////////////////////////////////////////////////////////////////////
// Test_VIDEOCONTROL_CAPS
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VIDEOCONTROL_CAPS
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOCONTROL_CAPS.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VIDEOCONTROL_CAPS( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS : InitializeDriver failed " ) ) ;
        goto done ;
    }

    DWORD    dwBytesReturned = 0 ;
    CSPROPERTY_VIDEOCONTROL_CAPS_S csPropVidControlCaps ;
    CSPROPERTY_VIDEOCONTROL_CAPS_S csProp ;

    memset(&csProp, 0, sizeof(csProp));

    csProp.Property.Set        = PROPSETID_VIDCAP_VIDEOCONTROL ;
    csProp.Property.Id        = CSPROPERTY_VIDEOCONTROL_CAPS ;
    csProp.Property.Flags    = CSPROPERTY_TYPE_GET ;

    if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                                        &csProp, 
                                                                        sizeof( csProp ), 
                                                                        &csPropVidControlCaps, 
                                                                        sizeof( csPropVidControlCaps ), 
                                                                        &dwBytesReturned,
                                                                        NULL ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS : TestDeviceIOControl failed. Unable to get CSPROPERTY_VIDEOCONTROL_CAPS  " ) ) ;
        goto done ;
    }


    csProp.StreamIndex = -1;
    if ( TRUE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                                        &csProp, 
                                                                        sizeof( csProp ), 
                                                                        &csPropVidControlCaps, 
                                                                        sizeof( csPropVidControlCaps ), 
                                                                        &dwBytesReturned,
                                                                        NULL ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS : TestDeviceIOControl succeeded with an invalid index." ) ) ;
        goto done ;
    }

done:    
    Log( TEXT( "Test_VIDEOCONTROL_CAPS : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VIDEOCONTROL_CAPS2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VIDEOCONTROL_CAPS2
//
//  Assertion:        
//
//  Description:    1:     Test CSPROPERTY_VIDEOCONTROL_CAPS. 
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VIDEOCONTROL_CAPS2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS2: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    // Let us try to set the number of pin types to 3
    DWORD        dwNumConnectInfo= 3 ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = PROPSETID_VIDCAP_VIDEOCONTROL ;
    csProp.Id        = CSPROPERTY_VIDEOCONTROL_CAPS ;
    csProp.Flags    = CSPROPERTY_TYPE_SET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &dwNumConnectInfo, 
                                            sizeof( DWORD ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS2 : Should not have been able to set CSPROPERTY_VIDEOCONTROL_CAPS " ) ) ;
        goto done ;
    }


done:    
    Log( TEXT( "Test_VIDEOCONTROL_CAPS2 : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VIDEOCONTROL_CAPS3
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VIDEOCONTROL_CAPS3
//
//  Assertion:        
//
//  Description:    3: Test CSPROPERTY_VIDEOCONTROL_CAPS. Verify that it handles invalid output buffers correctly.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VIDEOCONTROL_CAPS3( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = PROPSETID_VIDCAP_VIDEOCONTROL ;
    csProp.Id        = CSPROPERTY_VIDEOCONTROL_CAPS ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            NULL, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3 :  Querying for CSPROPERTY_VIDEOCONTROL_CAPS should have failed incase of NULL output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            NULL, 
                                            0, 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3 :  Querying for CSPROPERTY_VIDEOCONTROL_CAPS should have failed incase of NULL output buffer  " ) ) ;
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
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3 :  Querying for CSPROPERTY_VIDEOCONTROL_CAPS should have failed incase of BOOL output buffer  " ) ) ;
    }


    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &bTestVar, 
                                            sizeof( BOOL ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3 :  Querying for CSPROPERTY_VIDEOCONTROL_CAPS should have failed incase of BOOL output buffer  " ) ) ;
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
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3 :  Querying for CSPROPERTY_VIDEOCONTROL_CAPS should have failed incase of WORD output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &wTestVar, 
                                            sizeof( WORD ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3 :  Querying for CSPROPERTY_VIDEOCONTROL_CAPS should have failed incase of WORD output buffer  " ) ) ;
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
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3 :  Querying for CSPROPERTY_VIDEOCONTROL_CAPS should have failed incase of WORD output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &ul64TestVar, 
                                            sizeof( ULONG64 ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_CAPS3 :  Querying for CSPROPERTY_VIDEOCONTROL_CAPS should have failed incase of WORD output buffer  " ) ) ;
    }

done:    
    Log( TEXT( "Test_VIDEOCONTROL_CAPS3 : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VIDEOCONTROL_ACTUAL_FRAME_RATE
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VIDEOCONTROL_ACTUAL_FRAME_RATE
//
//  Assertion:        
//
//  Description:    4: Test Test_VIDEOCONTROL_ACTUAL_FRAME_RATE. Verify that it handles invalid output buffers correctly.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VIDEOCONTROL_ACTUAL_FRAME_RATE( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;
    RECT rc ;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE : InitializeDriver failed " ) ) ;
        goto done ;
    }

    // loop through all possible stream
    for ( SHORT nStream = 0 ; nStream < MAX_STREAMS ; ++nStream )
    {
        DWORD dwBytesReturned = 0 ;
        CSPROPERTY_VIDEOCONTROL_ACTUAL_FRAME_RATE_S csProp ;
        CS_DATARANGE_VIDEO csDataRangeVideo ;

        if ( FALSE == camTest.AvailablePinInstance(nStream) )
        {
            continue; // no instance available, try next one
        }

        // setup stream for test
        if ( FALSE == camTest.CreateStream(nStream, NULL, rc) )
        {
            FAIL( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE: Creating the stream failed " ) ) ;
            goto done ;
        }

        if ( FALSE == camTest.GetFormatInfo(nStream, 0, &csDataRangeVideo) )
        {
            FAIL ( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE: No supported formats on the pin " ) ) ;
            goto done ;
        }

        if ( FALSE == camTest.SelectVideoFormat(nStream, 0) )
        {
            FAIL ( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE: No supported formats on the pin " ) ) ;
            goto done ;
        }

        if ( FALSE == camTest.SetupStream(nStream) )
        {
            FAIL ( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE: Creating the stream failed " ) ) ;
            goto done ;
        }

        if ( FALSE == camTest.SetState(nStream, CSSTATE_RUN) )
        {
            FAIL ( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE: SetState run failed" ) ) ;
            goto done ;
        }

        // try to get actual frame rate
        csProp.Property.Set    = PROPSETID_VIDCAP_VIDEOCONTROL ;
        csProp.Property.Id     = CSPROPERTY_VIDEOCONTROL_ACTUAL_FRAME_RATE ;
        csProp.Property.Flags  = CSPROPERTY_TYPE_GET ;
        csProp.StreamIndex     = nStream ;
        csProp.RangeIndex      = 0 ;
        csProp.Dimensions.cx   = csDataRangeVideo.VideoInfoHeader.bmiHeader.biWidth ;
        csProp.Dimensions.cy   = csDataRangeVideo.VideoInfoHeader.bmiHeader.biHeight ;

        if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                    &csProp, 
                                                    sizeof( csProp ), 
                                                    &csProp, 
                                                    sizeof( csProp ), 
                                                    &dwBytesReturned,
                                                    NULL ) )
        {
            SKIP ( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE : Unable to get CSPROPERTY_VIDEOCONTROL_ACTUAL_FRAME_RATE  " ) ) ;
        }

        if ( FALSE == camTest.CleanupStream(nStream) )
        {
            FAIL ( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE: Cleanup the stream failed ") );
            goto done ;
        }
    }

done:    
    Log( TEXT( "Test_VIDEOCONTROL_ACTUAL_FRAME_RATE : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_VIDEOCONTROL_FRAME_RATES
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_VIDEOCONTROL_FRAME_RATES
//
//  Assertion:        
//
//  Description:    5: Test Test_VIDEOCONTROL_FRAME_RATES. Verify that it handles invalid output buffers correctly.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_VIDEOCONTROL_FRAME_RATES( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;
    RECT rc ;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_FRAME_RATES: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_VIDEOCONTROL_FRAME_RATES : InitializeDriver failed " ) ) ;
        goto done ;
    }

    // loop through all possible stream
    for ( SHORT nStream = 0 ; nStream < MAX_STREAMS ; ++nStream )
    {
        DWORD dwBytesReturned = 0 ;
        CSPROPERTY_VIDEOCONTROL_FRAME_RATES_S csProp ;
        CS_DATARANGE_VIDEO csDataRangeVideo ;
        PCSMULTIPLE_ITEM pMultipleItem ;

        if ( FALSE == camTest.AvailablePinInstance(nStream) )
        {
            continue; // no instance available, try next one
        }

        // setup stream for test
        if ( FALSE == camTest.CreateStream(nStream, NULL, rc) )
        {
            FAIL( TEXT( "Test_VIDEOCONTROL_FRAME_RATES: Creating the stream failed " ) ) ;
            goto done ;
        }

        if ( FALSE == camTest.GetFormatInfo(nStream, 0, &csDataRangeVideo) )
        {
            FAIL ( TEXT( "Test_VIDEOCONTROL_FRAME_RATES: No supported formats on the pin " ) ) ;
            goto done ;
        }

        // try to get frame rate
        csProp.Property.Set    = PROPSETID_VIDCAP_VIDEOCONTROL ;
        csProp.Property.Id     = CSPROPERTY_VIDEOCONTROL_FRAME_RATES ;
        csProp.Property.Flags  = CSPROPERTY_TYPE_GET ;
        csProp.StreamIndex     = nStream ;
        csProp.RangeIndex      = 0 ;
        csProp.Dimensions.cx   = csDataRangeVideo.VideoInfoHeader.bmiHeader.biWidth ;
        csProp.Dimensions.cy   = csDataRangeVideo.VideoInfoHeader.bmiHeader.biHeight ;

        if ( TRUE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                    &csProp, 
                                                    sizeof( csProp ), 
                                                    NULL, 
                                                    0, 
                                                    &dwBytesReturned,
                                                    NULL ) )
        {
            FAIL ( TEXT( "Test_VIDEOCONTROL_FRAME_RATES : should fail with ERROR_MORE_DATA " ) ) ;
            goto done ;
        }

        if ( ERROR_MORE_DATA == GetLastError( ) )
        {
            pMultipleItem = reinterpret_cast<PCSMULTIPLE_ITEM>(new BYTE[dwBytesReturned]);
            if ( NULL == pMultipleItem )
            {
                FAIL ( TEXT( "Test_VIDEOCONTROL_FRAME_RATES : OOM") );
                goto done ;
            }

            if ( FALSE == camTest.TestDeviceIOControl ( IOCTL_CS_PROPERTY, 
                                                        &csProp, 
                                                        sizeof( csProp ), 
                                                        pMultipleItem, 
                                                        dwBytesReturned, 
                                                        &dwBytesReturned,
                                                        NULL ) )
            {
                FAIL ( TEXT( "Test_VIDEOCONTROL_FRAME_RATES : unable to get CSPROPERTY_VIDEOCONTROL_FRAME_RATES " ) ) ;
                goto done ;
            }
            delete[] pMultipleItem;
        }
        else
        {
            SKIP ( TEXT( "Test_VIDEOCONTROL_FRAME_RATES : unsuport CSPROPERTY_VIDEOCONTROL_FRAME_RATES " ) );
        }

        if ( FALSE == camTest.CleanupStream(nStream) )
        {
            FAIL ( TEXT( "Test_VIDEOCONTROL_FRAME_RATES: Cleanup the stream failed ") );
            goto done ;
        }
    }

done:    
    Log( TEXT( "Test_VIDEOCONTROL_FRAME_RATES : Test Completed. " ) ) ;
    return GetTestResult();
}
