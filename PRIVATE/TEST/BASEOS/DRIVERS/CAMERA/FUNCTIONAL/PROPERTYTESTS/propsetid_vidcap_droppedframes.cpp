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
//          PROPSETID_VIDCAP_DROPPEDFRAMES
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include "logging.h"
#include "Globals.h"
#include "CameraDriverTest.h"

////////////////////////////////////////////////////////////////////////////////
// Test_DROPPEDFRAMES_CURRENT
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_DROPPEDFRAMES_CURRENT
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_DROPPEDFRAMES_CURRENT.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_DROPPEDFRAMES_CURRENT( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    CSPROPERTY_DROPPEDFRAMES_CURRENT_S csPropDroppedFrames ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = PROPSETID_VIDCAP_DROPPEDFRAMES ;
    csProp.Id        = CSPROPERTY_DROPPEDFRAMES_CURRENT ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames, 
                                            sizeof( csPropDroppedFrames ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT : TestDeviceIOControl succeeded. Able to recieved dropped frame count while in the stopped state." ) ) ;
        goto done ;
    }

done:    
    Log( TEXT( "Test_DROPPEDFRAMES_CURRENT : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_DROPPEDFRAMES_CURRENT2
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_DROPPEDFRAMES_CURRENT2
//
//  Assertion:        
//
//  Description:    2:     Test CSPROPERTY_DROPPEDFRAMES_CURRENT. Verify that a client can not set 
//                    this property
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_DROPPEDFRAMES_CURRENT2( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT2: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT2 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    CSPROPERTY_DROPPEDFRAMES_CURRENT_S csPropDroppedFrames ;
    DWORD        dwBytesReturned = 0 ;

    csPropDroppedFrames.AverageFrameSize     = 3 ;
    csPropDroppedFrames.DropCount        = 1 ;
    csPropDroppedFrames.PictureNumber        = 4 ;
    csPropDroppedFrames.Property.Set        = PROPSETID_VIDCAP_DROPPEDFRAMES ;
    csPropDroppedFrames.Property.Id        = CSPROPERTY_DROPPEDFRAMES_CURRENT ;
    csPropDroppedFrames.Property.Flags        = CSPROPERTY_TYPE_SET ;

    
    csProp.Set    = PROPSETID_VIDCAP_DROPPEDFRAMES ;
    csProp.Id        = CSPROPERTY_DROPPEDFRAMES_CURRENT ;
    csProp.Flags    = CSPROPERTY_TYPE_SET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames, 
                                            sizeof( csPropDroppedFrames ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT2 : Should not have been able to set CSPROPERTY_DROPPEDFRAMES_CURRENT " ) ) ;
        goto done ;
    }

done:    
    Log( TEXT( "Test_DROPPEDFRAMES_CURRENT2 : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_DROPPEDFRAMES_CURRENT3
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_DROPPEDFRAMES_CURRENT3
//
//  Assertion:        
//
//  Description:    3: Test CSPROPERTY_DROPPEDFRAMES_CURRENT. Verify that it handles invalid output buffers correctly.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_DROPPEDFRAMES_CURRENT3( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    CSPROPERTY    csProp ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = PROPSETID_VIDCAP_DROPPEDFRAMES ;
    csProp.Id        = CSPROPERTY_DROPPEDFRAMES_CURRENT ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            NULL, 
                                            sizeof( ULONG ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of NULL output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            NULL, 
                                            0, 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of NULL output buffer  " ) ) ;
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
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of BOOL output buffer  " ) ) ;
    }


    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &bTestVar, 
                                            sizeof( BOOL ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of BOOL output buffer  " ) ) ;
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
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of WORD output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &wTestVar, 
                                            sizeof( WORD ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of WORD output buffer  " ) ) ;
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
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of WORD output buffer  " ) ) ;
    }

    if ( TRUE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &ul64TestVar, 
                                            sizeof( ULONG64 ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT3 :  Querying for CSPROPERTY_PIN_CTYPES should have failed incase of WORD output buffer  " ) ) ;
    }

done:    
    Log( TEXT( "Test_DROPPEDFRAMES_CURRENT3 : Test Completed. " ) ) ;
    return GetTestResult();
}


////////////////////////////////////////////////////////////////////////////////
// Test_DROPPEDFRAMES_CURRENT4
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_DROPPEDFRAMES_CURRENT4
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_DROPPEDFRAMES_CURRENT.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_DROPPEDFRAMES_CURRENT4( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERAPROPERTYTEST camTest ;
    CS_DATARANGE_VIDEO csDataRangeVideo;
    RECT rc;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4: Failed to determine camera availability." ) ) ;
        goto done ;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : InitializeDriver failed " ) ) ;
        goto done ;
    }

    if(FALSE == camTest.CreateStream(STREAM_CAPTURE, NULL, rc))
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : Creating the stream failed "));
        goto done ;
    }

    if(FALSE == camTest.GetFormatInfo(STREAM_CAPTURE, 0, &csDataRangeVideo))
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : No supported formats on the pin "));
        goto done ;
    }

    if(FALSE == camTest.SelectVideoFormat(STREAM_CAPTURE, 0))
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : No supported formats on the pin "));
        goto done ;
    }

    CSPROPERTY    csProp ;
    CSPROPERTY_DROPPEDFRAMES_CURRENT_S csPropDroppedFrames ;
    CSPROPERTY_DROPPEDFRAMES_CURRENT_S csPropDroppedFrames2 ;
    DWORD        dwBytesReturned = 0 ;
    csProp.Set    = PROPSETID_VIDCAP_DROPPEDFRAMES ;
    csProp.Id        = CSPROPERTY_DROPPEDFRAMES_CURRENT ;
    csProp.Flags    = CSPROPERTY_TYPE_GET ;

    // Test frame&drop count is zero in stop state
    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames, 
                                            sizeof( csPropDroppedFrames ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : TestDeviceIOControl fail to recieved dropped frame count in stop state." ) ) ;
        goto done ;
    }

    if(csPropDroppedFrames.PictureNumber != 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The frame count should be zero in stop state."));
        goto done ;
    }

    if(csPropDroppedFrames.DropCount != 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The drop count should be zero in stop state."));
        goto done ;
    }

    // Test frame&drop count doesn't change in pause state
    if(FALSE == camTest.SetupStream(STREAM_CAPTURE))
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : Creating the stream failed "));
        goto done ;
    }

    Sleep(DWORD(csDataRangeVideo.VideoInfoHeader.AvgTimePerFrame/1000)); // wait 10 frames

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames, 
                                            sizeof( csPropDroppedFrames ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : TestDeviceIOControl fail to recieved dropped frame count in pause state." ) ) ;
        goto done ;
    }

    if(csPropDroppedFrames.PictureNumber != 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The frame count shouldn't change in pause state."));
        goto done ;
    }

    if(csPropDroppedFrames.DropCount != 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The drop count shouldn't change in pause state."));
        goto done ;
    }

    // Test frame&drop count increase in run state
    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_RUN))
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : SetState run failed"));
        goto done ;
    }

    Sleep(DWORD(csDataRangeVideo.VideoInfoHeader.AvgTimePerFrame/1000)); // wait 10 frames

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames, 
                                            sizeof( csPropDroppedFrames ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : TestDeviceIOControl fail to recieved dropped frame count in run state." ) ) ;
        goto done ;
    }

    if(csPropDroppedFrames.PictureNumber == 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The frame count should change in run state."));
        goto done ;
    }

    if(csPropDroppedFrames.PictureNumber < csPropDroppedFrames.DropCount)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The drop count should be less than or equal to frame count."));
        goto done ;
    }

    // Try again
    Sleep(DWORD(csDataRangeVideo.VideoInfoHeader.AvgTimePerFrame/1000)); // wait 10 frames

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames2, 
                                            sizeof( csPropDroppedFrames2 ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : TestDeviceIOControl fail to recieved dropped frame count in run state." ) ) ;
        goto done ;
    }

    if(csPropDroppedFrames2.PictureNumber - csPropDroppedFrames.PictureNumber <= 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The frame count should increase in run state."));
        goto done ;
    }

    if(csPropDroppedFrames2.DropCount - csPropDroppedFrames.DropCount < 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The drop count shouldn't decrease."));
        goto done ;
    }

    if(csPropDroppedFrames.PictureNumber < csPropDroppedFrames.DropCount)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The drop count should be less than or equal to frame count."));
        goto done ;
    }

    // Test frame&drop count doesn't change in pause state again
    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_PAUSE))
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : SetState pause failed"));
        goto done ;
    }

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames, 
                                            sizeof( csPropDroppedFrames ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : TestDeviceIOControl fail to recieved dropped frame count in pause state." ) ) ;
        goto done ;
    }

    Sleep(DWORD(csDataRangeVideo.VideoInfoHeader.AvgTimePerFrame/1000)); // wait 10 frames

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames2, 
                                            sizeof( csPropDroppedFrames2 ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : TestDeviceIOControl fail to recieved dropped frame count in pause state." ) ) ;
        goto done ;
    }

    if(csPropDroppedFrames.PictureNumber != csPropDroppedFrames2.PictureNumber)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The frame count shouldn't change in pause state."));
        goto done ;
    }

    if(csPropDroppedFrames.DropCount != csPropDroppedFrames2.DropCount)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The drop count shouldn't change in pause state."));
        goto done ;
    }

    // Test frame&drop count increase in run state again
    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_RUN))
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : SetState run failed"));
        goto done ;
    }

    Sleep(DWORD(csDataRangeVideo.VideoInfoHeader.AvgTimePerFrame/1000)); // wait 10 frames

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames2, 
                                            sizeof( csPropDroppedFrames2 ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : TestDeviceIOControl fail to recieved dropped frame count in run state." ) ) ;
        goto done ;
    }

    if(csPropDroppedFrames2.PictureNumber - csPropDroppedFrames.PictureNumber <= 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The frame count should increase in run state."));
        goto done ;
    }

    if(csPropDroppedFrames2.DropCount - csPropDroppedFrames.DropCount < 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The drop count shouldn't decrease."));
        goto done ;
    }

    // Test frame&drop count reset to zero in stop state
    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_STOP))
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : SetState stop failed"));
        goto done ;
    }

    if ( FALSE == camTest.TestDeviceIOControl (     IOCTL_CS_PROPERTY, 
                                            &csProp, 
                                            sizeof( CSPROPERTY ), 
                                            &csPropDroppedFrames, 
                                            sizeof( csPropDroppedFrames ), 
                                            &dwBytesReturned,
                                            NULL ) )
    {
        FAIL( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : TestDeviceIOControl fail to recieved dropped frame count in stop state." ) ) ;
        goto done ;
    }

    if(csPropDroppedFrames.PictureNumber != 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The frame count should be zero in stop state."));
        goto done ;
    }

    if(csPropDroppedFrames.DropCount != 0)
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : The drop count should be zero in stop state."));
        goto done ;
    }

    if(FALSE == camTest.CleanupStream(STREAM_CAPTURE))
    {
        FAIL(TEXT("Test_DROPPEDFRAMES_CURRENT4 : Cleanup the stream failed "));
        goto done ;
    }

done:    
    Log( TEXT( "Test_DROPPEDFRAMES_CURRENT4 : Test Completed. " ) ) ;
    return GetTestResult();
}

