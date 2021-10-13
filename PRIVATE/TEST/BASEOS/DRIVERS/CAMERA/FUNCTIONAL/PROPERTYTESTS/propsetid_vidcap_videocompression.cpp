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
//          CSPROPSETID_VIDCAP_VIDEOPROCAMP
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include "logging.h"
#include "Globals.h"
#include "CameraDriverTest.h"
#include "CameraSpecs.h"

void
Test_VideoCompression(GUID guidPropertySet, ULONG ulProperty)
{
    CAMERAPROPERTYTEST camTest ;
    DWORD dwBytesReturned = 0 ;
    BOOL supported = FALSE;
    CS_DATARANGE_VIDEO csDataRangeVideo ;
    CSPROPERTY_VIDEOCOMPRESSION_GETINFO_S csVideoCompressionInfo ;
    CSPROPERTY_VIDEOCOMPRESSION_S csVideoCompression ;
    RECT rc ;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    if(FALSE == camTest.DetermineCameraAvailability())
    {
        FAIL( TEXT( "Camera: Failed to determine camera availability." ) ) ;
        return;
    }

    if ( FALSE == camTest.InitializeDriver( ) )
    {
        FAIL( TEXT( "Camera : InitializeDriver failed " ) ) ;
        return;
    }

    camTest.PrepareTestEnvironment(     guidPropertySet,  
                                    ulProperty,
                                    VT_I4 ) ;

    if ( TRUE == camTest.IsPropertySetSupported( ) )
    {
        Log ( TEXT( "Camera: PROPSETID_VIDCAP_VIDEOCOMPRESSION is a supported property set" ) ) ;
        supported = TRUE;
    }

    if ( TRUE == camTest.FetchAccessFlags( ) && !supported )
    {
        FAIL( TEXT( "Camera: PROPSETID_VIDCAP_VIDEOCOMPRESSION succeeded in fetching the access flags, though property is not supported" ) ) ;
    }

    if ( FALSE == camTest.FetchAccessFlags( ) && supported )
    {
        FAIL( TEXT( "Camera: PROPSETID_VIDCAP_VIDEOCOMPRESSION failed fetching the access flags, though property is supported" ) ) ;
    }

    if ( FALSE == camTest.FetchAccessFlags( ) )
    {
        SKIP( TEXT( "Camera: PROPSETID_VIDCAP_VIDEOCOMPRESSION skip unsuported property" ) ) ;
        return;
    }

    // loop through all possible stream
    for ( SHORT nStream = 0 ; nStream < MAX_STREAMS ; ++nStream )
    {
        if ( FALSE == camTest.AvailablePinInstance(nStream) )
        {
            continue; // no instance available, try next one
        }

        // setup stream for test
        if ( FALSE == camTest.CreateStream(nStream, NULL, rc) )
        {
            FAIL( TEXT( "Camera: Creating the stream failed " ) ) ;
            return;
        }

        if ( FALSE == camTest.GetFormatInfo(nStream, 0, &csDataRangeVideo) )
        {
            FAIL ( TEXT( "Camera: No supported formats on the pin " ) ) ;
            return;
        }

        if ( FALSE == camTest.SelectVideoFormat(nStream, 0) )
        {
            FAIL ( TEXT( "Camera: No supported formats on the pin " ) ) ;
            return;
        }

        if ( FALSE == camTest.SetupStream(nStream) )
        {
            FAIL ( TEXT( "Camera: Creating the stream failed " ) ) ;
            return;
        }

        if ( FALSE == camTest.SetState(nStream, CSSTATE_RUN) )
        {
            FAIL ( TEXT( "Camera: SetState run failed" ) ) ;
            return;
        }

        // get compression info first
        csVideoCompressionInfo.Property.Set    = PROPSETID_VIDCAP_VIDEOCOMPRESSION ;
        csVideoCompressionInfo.Property.Id     = CSPROPERTY_VIDEOCOMPRESSION_GETINFO ;
        csVideoCompressionInfo.Property.Flags  = CSPROPERTY_TYPE_GET ;
        csVideoCompressionInfo.StreamIndex     = nStream ;
        if ( FALSE == camTest.TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                        &csVideoCompressionInfo, 
                                        sizeof( csVideoCompressionInfo ), 
                                        &csVideoCompressionInfo, 
                                        sizeof( csVideoCompressionInfo ), 
                                        &dwBytesReturned,
                                        NULL ) )
        {
            FAIL ( TEXT ( "Camera: can't get CSPROPERTY_VIDEOCOMPRESSION_GETINFO property." ) ) ;
            return;
        }

        // more test for no CSPROPERTY_VIDEOCOMPRESSION_GETINFO
        if ( ulProperty != CSPROPERTY_VIDEOCOMPRESSION_GETINFO )
        {
            csVideoCompression.StreamIndex = nStream ;

            if ( camTest.IsReadAllowed( ) )
            {
                // read current property value
                if ( FALSE == camTest.GetCurrentPropertyValue (&csVideoCompression, 
                                                        sizeof(csVideoCompression), 
                                                        &csVideoCompression, 
                                                        sizeof(csVideoCompression) ) )
                {
                    FAIL ( TEXT( "Camera: Fail to read property value" ) ) ;
                    return;
                }

                // compare with default value
                switch ( ulProperty )
                {
                case CSPROPERTY_VIDEOCOMPRESSION_KEYFRAME_RATE:
                    if ( csVideoCompression.Value != csVideoCompressionInfo.DefaultKeyFrameRate )
                    {
                        FAIL( TEXT( "Camera : The default value is incorrect." ) ) ;
                        return;
                    }
                    break;
                case CSPROPERTY_VIDEOCOMPRESSION_PFRAMES_PER_KEYFRAME:
                    if ( csVideoCompression.Value != csVideoCompressionInfo.DefaultPFrameRate )
                    {
                        FAIL( TEXT( "Camera : The default value is incorrect." ) ) ;
                        return;
                    }
                    break;
                case CSPROPERTY_VIDEOCOMPRESSION_QUALITY:
                    if ( csVideoCompression.Value != csVideoCompressionInfo.DefaultQuality )
                    {
                        FAIL( TEXT( "Camera : The default value is incorrect." ) ) ;
                        return;
                    }
                    break;
                }
            }

            // check write ability consistence
            static const LONG lCapabilityFlag[] = 
            {
                0,                              // CSPROPERTY_VIDEOCOMPRESSION_GETINFO
                CS_CompressionCaps_CanKeyFrame, // CSPROPERTY_VIDEOCOMPRESSION_KEYFRAME_RATE
                CS_CompressionCaps_CanBFrame,   // CSPROPERTY_VIDEOCOMPRESSION_PFRAMES_PER_KEYFRAME
                CS_CompressionCaps_CanQuality,  // CSPROPERTY_VIDEOCOMPRESSION_QUALITY
                0,                              // CSPROPERTY_VIDEOCOMPRESSION_OVERRIDE_KEYFRAME
                0,                              // CSPROPERTY_VIDEOCOMPRESSION_OVERRIDE_FRAME_SIZE
                CS_CompressionCaps_CanWindow    // CSPROPERTY_VIDEOCOMPRESSION_WINDOWSIZE

            };
            if ( camTest.IsWriteAllowed( ) != ((lCapabilityFlag[ulProperty]&csVideoCompressionInfo.Capabilities) != 0) )
            {
                FAIL( TEXT( "Camera : The property write ability is inconsistent." ) ) ;
            }

            // try to set property value
            if ( camTest.IsWriteAllowed( ) )
            {
                if ( FALSE == camTest.SetPropertyValue (&csVideoCompression, 
                                                    sizeof(csVideoCompression), 
                                                    &csVideoCompression, 
                                                    sizeof(csVideoCompression) ) )
                {
                    FAIL ( TEXT( "Camera: Fail to write property value" ) );
                    return;
                }
            }
        }

        if ( FALSE == camTest.CleanupStream(nStream) )
        {
            FAIL ( TEXT( "Camera: Cleanup the stream failed ") );
            return;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VIDEOCOMPRESSION_GETINFO
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VIDEOCOMPRESSION_GETINFO
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOCOMPRESSION_GETINFO. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VIDEOCOMPRESSION_GETINFO( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    Test_VideoCompression(PROPSETID_VIDCAP_VIDEOCOMPRESSION,
                                CSPROPERTY_VIDEOCOMPRESSION_GETINFO);

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VIDEOCOMPRESSION_KEYFRAME_RATE
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VIDEOCOMPRESSION_KEYFRAME_RATE
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOCOMPRESSION_KEYFRAME_RATE. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VIDEOCOMPRESSION_KEYFRAME_RATE( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    Test_VideoCompression(PROPSETID_VIDCAP_VIDEOCOMPRESSION,
                                CSPROPERTY_VIDEOCOMPRESSION_KEYFRAME_RATE);

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VIDEOCOMPRESSION_PFRAMES_PER_KEYFRAME
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VIDEOCOMPRESSION_PFRAMES_PER_KEYFRAME
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOCOMPRESSION_PFRAMES_PER_KEYFRAME. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VIDEOCOMPRESSION_PFRAMES_PER_KEYFRAME( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    Test_VideoCompression(PROPSETID_VIDCAP_VIDEOCOMPRESSION,
                                CSPROPERTY_VIDEOCOMPRESSION_PFRAMES_PER_KEYFRAME);

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VIDEOCOMPRESSION_QUALITY
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VIDEOCOMPRESSION_QUALITY
//
//  Assertion:        
//
//  Description:    1: Test CSPROPERTY_VIDEOCOMPRESSION_QUALITY. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VIDEOCOMPRESSION_QUALITY( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    Test_VideoCompression(PROPSETID_VIDCAP_VIDEOCOMPRESSION,
                                CSPROPERTY_VIDEOCOMPRESSION_QUALITY);

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_CSProperty_VIDEOCOMPRESSION_WINDOWSIZE
//
// Parameters:
//  uMsg            Message code.
//  tpParam        Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_CSProperty_VIDEOCOMPRESSION_WINDOWSIZE
//
//  Assertion:        
//
//  Description:    1: Test Test_CSProperty_VIDEOCOMPRESSION_WINDOWSIZE. Try to get the current and default value
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_CSProperty_VIDEOCOMPRESSION_WINDOWSIZE( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if ( uMsg != TPM_EXECUTE ) 
    {
        return TPR_NOT_HANDLED;
    }

    Test_VideoCompression(PROPSETID_VIDCAP_VIDEOCOMPRESSION,
                                CSPROPERTY_VIDEOCOMPRESSION_WINDOWSIZE);

    Log( TEXT( "Camera : Test Completed. " ) ) ;
    return GetTestResult();
}


