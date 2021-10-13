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
//          CSPROPSETID_Stream
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include "logging.h"
#include "Globals.h"
#include "CameraDriverTest.h"
#define DEFINE_CAMERA_TEST_PROPERTIES
#include "CameraSpecs.h"

////////////////////////////////////////////////////////////////////////////////
// Test_IO_Preview
//
// Parameters:
//  uMsg            Message code.
//  tpParam            Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_IO_Preview
//
//  Assertion:        
//
//  Description:    1: Test by creating a Preview Pin.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IO_Preview(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    CAMERADRIVERTEST camTest;
    RECT rc;
    int nVideoFormat = 0;
    CS_DATARANGE_VIDEO csDRV;
    int nTestFormat, NumSupportedFormats;
    int nPreDelayFrameCount, nPostDelayFrameCount;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    memset(&csDRV, 0, sizeof(csDRV));

    if(camTest.DetermineCameraAvailability())
    {
        if(camTest.InitializeDriver())
        {
            if(camTest.AvailablePinInstance(STREAM_PREVIEW))
            {
                if(FALSE == camTest.CreateStream(STREAM_PREVIEW, NULL, rc))
                {
                    FAIL(TEXT("Test_IO_Preview : Creating the stream failed "));
                    goto done;
                }

                NumSupportedFormats = camTest.GetNumSupportedFormats(STREAM_PREVIEW);
                if(0 == NumSupportedFormats)
                {
                    FAIL(TEXT("Test_IO_Preview : No supported formats on the pin "));
                    goto done;
                }

                for(nTestFormat = 0; nTestFormat < NumSupportedFormats; nTestFormat++)
                {
                    Log(TEXT("Test_IO_Preview : Testing format %d of %d"), nTestFormat + 1, NumSupportedFormats);

                    if(FALSE == camTest.GetFormatInfo(STREAM_PREVIEW, nTestFormat, &csDRV))
                    {
                        FAIL(TEXT("Test_IO_Preview : No supported formats on the pin "));
                        goto done;
                    }

                    if(0 == camTest.SelectVideoFormat(STREAM_PREVIEW, nTestFormat))
                    {
                        FAIL(TEXT("Test_IO_Preview : No supported formats on the pin "));
                        goto done;
                    }

                    if(FALSE == camTest.SetupStream(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : Creating the stream failed "));
                        goto done;
                    }

                    Log(TEXT("Test_IO_Preview : Testing a new running preview"));

                    if(FALSE == camTest.SetState(STREAM_PREVIEW, CSSTATE_RUN))
                    {
                        FAIL(TEXT("Test_IO_Preview : SetState run failed"));
                        goto done;
                    }

                    if(CSSTATE_RUN != camTest.GetState(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : The preview pin state is not run after setting it to be run."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : Succeeded in triggering a capture event on the preview stream.. Only still pin should allow this."));
                        goto done;
                    }

                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    if(nPostDelayFrameCount <= nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Preview : no frames processed when pin was running."));
                    }

                    Log(TEXT("Test_IO_Preview : Testing a paused preview"));

                    if(FALSE == camTest.SetState(STREAM_PREVIEW, CSSTATE_PAUSE))
                    {
                        FAIL(TEXT("Test_IO_Preview : SetState pause failed"));
                        goto done;
                    }

                    if(CSSTATE_PAUSE != camTest.GetState(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : The preview pin state is not paused after setting it to be paused."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : Succeeded in triggering a capture event on the preview stream."));
                        goto done;
                    }

                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    if(nPostDelayFrameCount > nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Preview : frames processed during the pause test"));
                    }

                    Log(TEXT("Test_IO_Preview: Testing the preview pin restarted from the paused state"));

                    if(FALSE == camTest.SetState(STREAM_PREVIEW, CSSTATE_RUN))
                    {
                        FAIL(TEXT("Test_IO_Preview : SetState run failed"));
                        goto done;
                    }

                    if(CSSTATE_RUN != camTest.GetState(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : The preview pin state is not run after setting it to be run."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : Succeeded in triggering a capture event on the preview stream."));
                        goto done;
                    }

                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    if(nPostDelayFrameCount <= nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Preview : no frames processed when pin was running."));
                    }

                    Log(TEXT("Test_IO_Preview : Testing a stopped preview to verify no processing"));

                    if(FALSE == camTest.SetState(STREAM_PREVIEW, CSSTATE_STOP))
                    {
                        FAIL(TEXT("Test_IO_Preview : SetState stop failed"));
                        goto done;
                    }

                    if(CSSTATE_STOP != camTest.GetState(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : The preview pin state is not stopped after setting it to be stopped."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : Succeeded in triggering a capture event on the preview stream."));
                        goto done;
                    }

                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    if(nPostDelayFrameCount > nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Preview : frames processed during the pause test"));
                    }

                    Log(TEXT("Test_IO_Preview: Testing a restarted preview going through the stopped state"));

                    if(FALSE == camTest.SetState(STREAM_PREVIEW, CSSTATE_PAUSE))
                    {
                        FAIL(TEXT("Test_IO_Preview : SetState run failed"));
                        goto done;
                    }

                    if(FALSE == camTest.SetState(STREAM_PREVIEW, CSSTATE_RUN))
                    {
                        FAIL(TEXT("Test_IO_Preview : SetState run failed"));
                        goto done;
                    }

                    if(CSSTATE_RUN != camTest.GetState(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : The preview pin state is not run after setting it to be run."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : Succeeded in triggering a capture event on the preview stream."));
                        goto done;
                    }

                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_PREVIEW);

                    if(nPostDelayFrameCount <= nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Preview : frames not processed during the run portion of the test"));
                    }

                    if(FALSE == camTest.SetState(STREAM_PREVIEW, CSSTATE_PAUSE))
                    {
                        FAIL(TEXT("Test_IO_Preview : SetState pause failed"));
                        goto done;
                    }

                    if(FALSE == camTest.SetState(STREAM_PREVIEW, CSSTATE_STOP))
                    {
                        FAIL(TEXT("Test_IO_Preview : SetState stop failed"));
                        goto done;
                    }

                    if(CSSTATE_STOP != camTest.GetState(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : The preview pin state is not stopped after setting it to be stopped."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : Succeeded in triggering a capture event on the preview stream."));
                        goto done;
                    }

                    if(FALSE == camTest.CleanupStream(STREAM_PREVIEW))
                    {
                        FAIL(TEXT("Test_IO_Preview : Creating the stream failed "));
                        goto done;
                    }
                }
            }
            else SKIP(TEXT("Test_IO_Preview : Preview pin unavailable"));
        }
        else FAIL(TEXT("Test_IO_Preview : Camera driver initialization failed."));
    }
    else SKIP(TEXT("Test_IO_Preview : Camera driver unavailable."));

done:    
    camTest.Cleanup();

    Log(TEXT("Test_IO_Preview : Test Completed. "));
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_IO_Capture
//
// Parameters:
//  uMsg            Message code.
//  tpParam            Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_IO_Capture
//
//  Assertion:        
//
//  Description:    1: Test by creating a capture Pin.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IO_Capture(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    CAMERADRIVERTEST camTest;
    RECT rc;
    int nVideoFormat = 0;
    CS_DATARANGE_VIDEO csDRV;
    int nTestFormat, NumSupportedFormats;
    int nPreDelayFrameCount, nPostDelayFrameCount;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    memset(&csDRV, 0, sizeof(csDRV));

    if(camTest.DetermineCameraAvailability())
    {
        if(camTest.InitializeDriver())
        {
            if(camTest.AvailablePinInstance(STREAM_CAPTURE))
            {
                if(FALSE == camTest.CreateStream(STREAM_CAPTURE, NULL, rc))
                {
                    FAIL(TEXT("Test_IO_Capture : Creating the stream failed "));
                    goto done;
                }

                NumSupportedFormats = camTest.GetNumSupportedFormats(STREAM_CAPTURE);
                if(0 == NumSupportedFormats)
                {
                    FAIL(TEXT("Test_IO_Capture : No supported formats on the pin "));
                    goto done;
                }

                for(nTestFormat = 0; nTestFormat < NumSupportedFormats; nTestFormat++)
                {
                    Log(TEXT("Test_IO_Capture : Testing format %d of %d"), nTestFormat + 1, NumSupportedFormats);

                    if(FALSE == camTest.GetFormatInfo(STREAM_CAPTURE, nTestFormat, &csDRV))
                    {
                        FAIL(TEXT("Test_IO_Capture : No supported formats on the pin "));
                        goto done;
                    }

                    if(0 == camTest.SelectVideoFormat(STREAM_CAPTURE, nTestFormat))
                    {
                        FAIL(TEXT("Test_IO_Capture : No supported formats on the pin "));
                        goto done;
                    }

                    if(FALSE == camTest.SetupStream(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : Creating the stream failed "));
                        goto done;
                    }

                    Log(TEXT("Test_IO_Capture : Testing a new running capture"));

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_RUN))
                    {
                        FAIL(TEXT("Test_IO_Capture : SetState run failed"));
                        goto done;
                    }

                    if(CSSTATE_RUN != camTest.GetState(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : The capture pin state is not run after setting it to be run."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : Succeeded triggering a capture event on the capture stream. Only still pin should allow this."));
                        goto done;
                    }

                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    if(nPostDelayFrameCount <= nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Capture : no frames processed when pin was running."));
                    }

                    Log(TEXT("Test_IO_Capture : Testing a paused capture"));

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_PAUSE))
                    {
                        FAIL(TEXT("Test_IO_Capture : SetState pause failed"));
                        goto done;
                    }

                    if(CSSTATE_PAUSE != camTest.GetState(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : The capture pin state is not paused after setting it to be paused."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : Succeeded triggering a capture event on the capture stream. Only still pin should allow this."));
                        goto done;
                    }

                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    if(nPostDelayFrameCount > nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Capture : frames processed during the pause test"));
                    }

                    Log(TEXT("Test_IO_Capture: Testing the capture pin restarted from the paused state"));

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_RUN))
                    {
                        FAIL(TEXT("Test_IO_Capture : SetState run failed"));
                        goto done;
                    }

                    if(CSSTATE_RUN != camTest.GetState(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : The capture pin state is not run after setting it to be run."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : Succeeded triggering a capture event on the capture stream. Only still pin should allow this."));
                        goto done;
                    }

                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    if(nPostDelayFrameCount <= nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Capture : no frames processed when pin was running."));
                    }

                    Log(TEXT("Test_IO_Capture : Testing a stopped capture to verify no processing"));

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_STOP))
                    {
                        FAIL(TEXT("Test_IO_Capture : SetState stop failed"));
                        goto done;
                    }

                    if(CSSTATE_STOP != camTest.GetState(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : The capture pin state is not stopped after setting it to be stopped."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : Succeeded triggering a capture event on the capture stream. Only still pin should allow this."));
                        goto done;
                    }

                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    if(nPostDelayFrameCount > nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Capture : frames processed during the pause test"));
                    }

                    Log(TEXT("Test_IO_Capture: Testing a restarted capture going through the stopped state"));

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_PAUSE))
                    {
                        FAIL(TEXT("Test_IO_Capture : SetState pause failed"));
                        goto done;
                    }

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_RUN))
                    {
                        FAIL(TEXT("Test_IO_Capture : SetState run failed"));
                        goto done;
                    }

                    if(CSSTATE_RUN != camTest.GetState(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : The capture pin state is not run after setting it to be run."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : Succeeded triggering a capture event on the capture stream. This should only succeed on still pin."));
                        goto done;
                    }

                    // Give some time to flush out any frames that were queued up before stopping.
                    Sleep(500);
                    nPreDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    Sleep(5000);

                    nPostDelayFrameCount = camTest.GetNumberOfFramesProcessed(STREAM_CAPTURE);

                    if(nPostDelayFrameCount <= nPreDelayFrameCount)
                    {
                        FAIL(TEXT("Test_IO_Capture : frames not processed during the run portion of the test"));
                    }

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_PAUSE))
                    {
                        FAIL(TEXT("Test_IO_Capture : SetState pause failed"));
                        goto done;
                    }

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_STOP))
                    {
                        FAIL(TEXT("Test_IO_Capture : SetState stop failed"));
                        goto done;
                    }

                    if(CSSTATE_STOP != camTest.GetState(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : The capture pin state is not stopped after setting it to be stopped."));
                        goto done;
                    }

                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : Succeeded triggering a capture event on the capture stream. This should only succeed on still pin."));
                        goto done;
                    }

                    if(FALSE == camTest.CleanupStream(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_Capture : Creating the stream failed "));
                        goto done;
                    }
                }
            }
            else SKIP(TEXT("Test_IO_Capture : Capture pin unavailable"));
        }
        else FAIL(TEXT("Test_IO_Capture : Camera driver initialization failed."));
    }
    else SKIP(TEXT("Test_IO_Capture : Camera driver unavailable."));

done:    
    camTest.Cleanup();

    Log(TEXT("Test_IO_Capture : Test Completed. "));
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_IO_DroppedFrames
//
// Parameters:
//  uMsg            Message code.
//  tpParam            Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_IO_DroppedFrames
//
//  Assertion:        
//
//  Description:    1: Test by creating a capture Pin and then retrieving the dropped frames count.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IO_DroppedFrames(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE) 
    {
        return TPR_NOT_HANDLED;
    }

    CAMERADRIVERTEST    camTest;
    RECT rc;
    int nVideoFormat = 0;
    CSPROPERTY csProp;
    CSPROPERTY_DROPPEDFRAMES_CURRENT_S csPropDroppedFrames;
    DWORD dwBytesReturned;
    DWORD dwStartTickCount;
    DWORD dwCurrentTickCount;
    CS_DATARANGE_VIDEO csDRV;
    int nTestFormat, NumSupportedFormats;
    DWORD dwTestTimeout = 2000;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    if(camTest.DetermineCameraAvailability())
    {
        if(camTest.InitializeDriver())
        {
            if(camTest.AvailablePinInstance(STREAM_CAPTURE))
            {
                if(FALSE == camTest.CreateStream(STREAM_CAPTURE, NULL, rc))
                {
                    FAIL(TEXT("Test_IO_DroppedFrames : InitializeDriver failed "));
                    goto done;
                }

                // tell the framework to simulate an artificial delay of 2/10th of a second
                if(FALSE == camTest.SetArtificialDelay(STREAM_CAPTURE, 200))
                {
                    FAIL(TEXT("Test_IO_DroppedFrames : SetArtificialDelay failed "));
                    goto done;
                }

                NumSupportedFormats = camTest.GetNumSupportedFormats(STREAM_CAPTURE);
                if(0 == NumSupportedFormats)
                {
                    FAIL(TEXT("Test_IO_DroppedFrames : No supported formats on the pin "));
                    goto done;
                }

                for(nTestFormat = 0; nTestFormat < NumSupportedFormats; nTestFormat++)
                {
                    Log(TEXT("Test_IO_DroppedFrames : Testing format %d of %d"), nTestFormat + 1, NumSupportedFormats);

                    memset(&csDRV, 0, sizeof(csDRV));

                    // default to testing entry 0.
                    if(0 == camTest.GetFormatInfo(STREAM_CAPTURE, nTestFormat, &csDRV))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : No supported formats on the pin "));
                        goto done;
                    }

                    if(0 == camTest.SelectVideoFormat(STREAM_CAPTURE, nTestFormat))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : No supported formats on the pin "));
                        goto done;
                    }

                    if(FALSE == camTest.SetupStream(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : Creating the stream failed "));
                        goto done;
                    }

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_RUN))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : SetState failed"));
                        goto done;
                    }

                    if(CSSTATE_RUN != camTest.GetState(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : The capture pin state is not run after setting it to be run."));
                        goto done;
                    }

                    dwStartTickCount = GetTickCount();

                    // waste some cpu time to get a frame drop
                    do
                    {
                        dwCurrentTickCount = GetTickCount();

                        // if we happen to hit the tick count rollover, then reset the start tick count to the
                        // current tick count to start the timing over.  The worst case is we do 2*dwTestTimeout,
                        // which means the test runs a little longer but no harm is done.
                        if(dwCurrentTickCount < dwStartTickCount)
                            dwStartTickCount = dwCurrentTickCount;
                    }while((dwCurrentTickCount - dwStartTickCount) < dwTestTimeout);

                    memset(&csProp, 0, sizeof(csProp));
                    memset(&csPropDroppedFrames, 0, sizeof(csPropDroppedFrames));
                    dwBytesReturned = 0;

                    csProp.Set = PROPSETID_VIDCAP_DROPPEDFRAMES;
                    csProp.Id = CSPROPERTY_DROPPEDFRAMES_CURRENT;
                    csProp.Flags = CSPROPERTY_TYPE_GET;

                    if(FALSE == camTest.TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                                            &csProp, 
                                                            sizeof(CSPROPERTY), 
                                                            &csPropDroppedFrames, 
                                                            sizeof(csPropDroppedFrames), 
                                                            &dwBytesReturned,
                                                            NULL))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : TestDeviceIOControl failed. Unable to retrieve dropped frame count."));
                        goto done;
                    }

                    if(csPropDroppedFrames.PictureNumber == 0)
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : Recieved a 0 frame count."));
                    }

                    if(csPropDroppedFrames.DropCount == 0)
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : Recieved a 0 frame drop count when expected some frames dropped"));
                    }

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_PAUSE))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : SetState failed"));
                        goto done;
                    }


                    if(CSSTATE_PAUSE != camTest.GetState(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : The capture pin state is not paused after setting it to be paused."));
                        goto done;
                    }

                    if(FALSE == camTest.TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                                            &csProp, 
                                                            sizeof(CSPROPERTY), 
                                                            &csPropDroppedFrames, 
                                                            sizeof(csPropDroppedFrames), 
                                                            &dwBytesReturned,
                                                            NULL))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : TestDeviceIOControl failed. Unable to retrieve dropped frame count."));
                        goto done;
                    }

                    if(csPropDroppedFrames.PictureNumber == 0)
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : Recieved a 0 frame count after pause."));
                    }

                    if(csPropDroppedFrames.DropCount == 0)
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : Recieved a 0 frame drop count when expected some frames dropped after pause."));
                    }

                    if(FALSE == camTest.SetState(STREAM_CAPTURE, CSSTATE_STOP))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : SetState failed"));
                        goto done;
                    }

                    if(CSSTATE_STOP != camTest.GetState(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : The capture pin state is not stopped after setting it to be stopped."));
                        goto done;
                    }

                    if(FALSE == camTest.TestDeviceIOControl (IOCTL_CS_PROPERTY, 
                                                            &csProp, 
                                                            sizeof(CSPROPERTY), 
                                                            &csPropDroppedFrames, 
                                                            sizeof(csPropDroppedFrames), 
                                                            &dwBytesReturned,
                                                            NULL))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : TestDeviceIOControl failed. Unable to retrieve dropped frame count."));
                        goto done;
                    }

                    if(csPropDroppedFrames.PictureNumber != 0)
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : Recieved a non-zero picture count on a stopped camera driver."));
                        Log(TEXT("Test_IO_DroppedFrames : Returned %d frames, expect 0"), csPropDroppedFrames.PictureNumber);
                    }

                    if(csPropDroppedFrames.DropCount != 0)
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : Recieved a non-zero frame drop count on a stopped camera driver."));
                        Log(TEXT("Test_IO_DroppedFrames : Returned %d frames dropped, expect 0"), csPropDroppedFrames.DropCount);
                    }

                    if(FALSE == camTest.CleanupStream(STREAM_CAPTURE))
                    {
                        FAIL(TEXT("Test_IO_DroppedFrames : Creating the stream failed "));
                        goto done;
                    }
                }
            }
            else SKIP(TEXT("Test_IO_DroppedFrames : Capture pin unavailable"));
        }
        else FAIL(TEXT("Test_IO_DroppedFrames : Camera driver initialization failed."));
    }
    else SKIP(TEXT("Test_IO_DroppedFrames : Camera driver unavailable."));

done:    
    camTest.Cleanup();

    Log(TEXT("Test_IO_DroppedFrames : Test Completed. "));
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// Test_IO_Still
//
// Parameters:
//  uMsg            Message code.
//  tpParam            Additional message-dependent data.
//  lpFTE            Function table entry that generated this call.
//
//  Test:            Test_IO_Still
//
//  Assertion:        
//
//  Description:    1: Test by creating a still Pin.
//                  
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Test_IO_Still(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    CAMERADRIVERTEST camTest;
    RECT rc;
    int nVideoFormat = 0;
    CS_DATARANGE_VIDEO csDRV;
    int nTestFormat, NumSupportedFormats;

    SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);

    memset(&csDRV, 0, sizeof(csDRV));

    if(camTest.DetermineCameraAvailability())
    {
        if(camTest.InitializeDriver())
        {
            if(camTest.AvailablePinInstance(STREAM_STILL))
            {
                if(FALSE == camTest.CreateStream(STREAM_STILL, NULL, rc))
                {
                    FAIL(TEXT("Test_IO_Still : Creating the stream failed "));
                    goto done;
                }

                NumSupportedFormats = camTest.GetNumSupportedFormats(STREAM_STILL);
                if(0 == NumSupportedFormats)
                {
                    FAIL(TEXT("Test_IO_Still : No supported formats on the pin "));
                    goto done;
                }

                for(nTestFormat = 0; nTestFormat < NumSupportedFormats; nTestFormat++)
                {
                    Log(TEXT("Test_IO_Still : Testing format %d of %d"), nTestFormat + 1, NumSupportedFormats);

                    if(FALSE == camTest.GetFormatInfo(STREAM_STILL, nTestFormat, &csDRV))
                    {
                        FAIL(TEXT("Test_IO_Still : No supported formats on the pin "));
                        goto done;
                    }

                    if(0 == camTest.SelectVideoFormat(STREAM_STILL, nTestFormat))
                    {
                        FAIL(TEXT("Test_IO_Still : No supported formats on the pin "));
                        goto done;
                    }

                    if(FALSE == camTest.SetupStream(STREAM_STILL))
                    {
                        FAIL(TEXT("Test_IO_Still : Creating the stream failed "));
                        goto done;
                    }

                    if(FALSE == camTest.SetState(STREAM_STILL, CSSTATE_RUN))
                    {
                        FAIL(TEXT("Test_IO_Still : SetState run failed on the still stream."));
                        Log(TEXT("Setting Still to Run should succeed but it should not change the state of the stream"));
                        goto done;
                    }
                    else Log(TEXT("Test_IO_Still : SetState failed as expected."));

                    if(CSSTATE_RUN == camTest.GetState(STREAM_STILL))
                    {
                        FAIL(TEXT("Test_IO_Still : SetState changed the still state when not expected to."));
                        goto done;
                    }

                    if(FALSE == camTest.SetState(STREAM_STILL, CSSTATE_PAUSE))
                    {
                        FAIL(TEXT("Test_IO_Still : SetState pause failed on the preview stream."));
                        goto done;
                    }

                    if(CSSTATE_PAUSE != camTest.GetState(STREAM_STILL))
                    {
                        FAIL(TEXT("Test_IO_Still : The preview stream state was something other than paused."));
                        goto done;
                    }

                    for(int i = 0; i < 25; i++)
                    {
                        Log(TEXT("Capturing still image %d"), i);
                        Sleep(100);
                        if(FALSE == camTest.TriggerCaptureEvent(STREAM_STILL))
                        {
                            FAIL(TEXT("Test_IO_Still : Failed triggering a capture event on the still stream."));
                            goto done;
                        }
                    }

                    // 1000 tries, 1/10 second per try, so we're trying for 10 seconds.
                    // This gives the driver a chance to finish all of the pending still captures
                    // we did 25 triggers above (i was 0 to 24), so wait for 25 samples.
                    int Tries = 1000;
                    while(camTest.GetNumberOfFramesProcessed(STREAM_STILL) <= 25 && Tries-- > 0) Sleep(100);

                    if(FALSE == camTest.SetState(STREAM_STILL, CSSTATE_STOP))
                    {
                        FAIL(TEXT("Test_IO_Still : SetState stop failed on the still stream."));
                        goto done;
                    }

                    if(CSSTATE_STOP != camTest.GetState(STREAM_STILL))
                    {
                        FAIL(TEXT("Test_IO_Still : Still stream state isn't stopped after being set to stopped."));
                        goto done;
                    }

                    Log(TEXT("Capturing still image while stopped: should fail"));
                    if(TRUE == camTest.TriggerCaptureEvent(STREAM_STILL))
                    {
                        FAIL(TEXT("Test_IO_Still : Succeeded triggering a capture event on still stream that was stopped"));
                        goto done;
                    }

                    if(FALSE == camTest.CleanupStream(STREAM_STILL))
                    {
                        FAIL(TEXT("Test_IO_Still : Creating the stream failed "));
                        goto done;
                    }
                }
            }
            else SKIP(TEXT("Test_IO_Still : Still pin unavailable"));
        }
        else FAIL(TEXT("Test_IO_Still : Camera driver initialization failed."));
    }
    else SKIP(TEXT("Test_IO_Still : Camera driver unavailable."));

done:    
    camTest.Cleanup();

    Log(TEXT("Test_IO_Still : Test Completed. "));
    return GetTestResult();
}
