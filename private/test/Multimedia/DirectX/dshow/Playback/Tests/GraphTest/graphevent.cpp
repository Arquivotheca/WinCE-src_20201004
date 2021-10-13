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
//  Perform IMediaEvent interface tests.
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"
#include "utility.h"

HRESULT NullParameterTest( IMediaEvent *pEvent, bool bRendered );
HRESULT WaitForCompletionTest( TestGraph *pGraph );
HRESULT GetEventTest( TestGraph *pGraph );
HRESULT DefaultRepaintEventHandlingTest( TestGraph *pGraph, bool *pbRepaint );
HRESULT DefaultCompleteEventHandlingTest( TestGraph *pGraph );

// Windows Embedded CE 6.0 Event Notification Codes
// Exclude EC_COMPLETE and EC_REPAINT which have default handling.
long EventCodes[] = { EC_ACTIVATE, EC_BUFFER_FULL, EC_BUFFERING_DATA,
                      EC_CAP_FILE_COMPLETED, EC_CAP_FILE_WRITE_ERROR, EC_CAP_SAMPLE_PROCESSED,
                      EC_CLOCK_CHANGED, /* EC_COMPLETE, */ EC_DRM_LEVEL, EC_END_OF_SEGMENT,
                      EC_ERROR_STILLPLAYING, EC_ERRORABORT, EC_FULLSCREEN_LOST, EC_NEED_RESTART,
                      EC_NOTIFY_WINDOW, EC_OLE_EVENT, EC_OPENING_FILE, EC_PLAY_NEXT,
                      EC_PLAY_PREVIOUS, EC_PALETTE_CHANGED, EC_QUALITY_CHANGE, /* EC_REPAINT, */
                      EC_SEGMENT_STARTED, EC_SHUTTING_DOWN, EC_STARVATION, EC_STREAM_CONTROL_STARTED,
                      EC_STREAM_CONTROL_STOPPED, EC_STREAM_ERROR_STILLPLAYING,
                      EC_STREAM_ERROR_STOPPED, EC_TIME, EC_USERABORT, /* EC_VIDEO_SIZE_AR_CHANGED, */
                      EC_VIDEO_SIZE_CHANGED, EC_WINDOW_DESTROYED };

long EventCodeSize = sizeof(EventCodes)/sizeof(long);


int
CancelRestoreDefaultHandling( bool bCancelDefaultHandling, IMediaEvent *pMediaEvent )
{
        HRESULT hr = S_OK;
    if ( !pMediaEvent )
    {
        LOG( TEXT("%S: ERROR %d@%S - NULL IMediaEvent interface returned."),
                    __FUNCTION__, __LINE__, __FILE__ );
        return -1;
    }

    if ( bCancelDefaultHandling )
    {
        for ( int i = 0; i < EventCodeSize; i++ )
        {
                        hr = pMediaEvent->CancelDefaultHandling( EventCodes[i] );
            if ( SUCCEEDED(hr)  )
            {
                LOG( TEXT("%S: WARNING %d@%S - No default handling for the event 0x%x, should not succeed."),
                            __FUNCTION__, __LINE__, __FILE__, EventCodes[i] );
            }
        }
    }
    else
    {
        for ( int i = 0; i < EventCodeSize; i++ )
        {
                        hr = pMediaEvent->RestoreDefaultHandling( EventCodes[i] );
            if ( SUCCEEDED(hr) )
            {
                LOG( TEXT("%S: WARNING %d@%S - No default handling for the event 0x%x, should not succeeded."),
                            __FUNCTION__, __LINE__, __FILE__, EventCodes[i] );
            }
        }
    }
    return 0;
}

//
// Performs the Media event interface tests for the media file specified.
// Return value:
// TPR_PASS if the test passed
// TPR_FAIL if the test failed
// or possibly other special conditions.
//
TESTPROCAPI
IMediaEventTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia( 0 );
    if ( !pMedia )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve media object"),
                    __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    LONG    lEventCode = 0;
    bool    bRepaint = 0;

    // Instantiate the test graph
    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);

    // Build the graph, don't render the file at this point, but need obtain required
    // interfaces.
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to build an empty graph. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.AcquireInterfaces();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to acquire the required interfaces. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    IMediaEvent *pMediaEvent = NULL;
    hr = testGraph.GetMediaEvent( &pMediaEvent );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to acquire IMediaEvent interface. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    if ( !pMediaEvent )
    {
        LOG( TEXT("%S: ERROR %d@%S - NULL IMediaEvent interface returned."),
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Null parameter tests on IMediaEvent before rendering
    hr = NullParameterTest( pMediaEvent, false );
    if ( FAILED(hr) ) retval = TPR_FAIL;

    // Render the file, which is done inside BuildGraph
    hr = testGraph.BuildGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to render media %s (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else {
        LOG( TEXT("%S: successfully render %s"), __FUNCTION__, pMedia->GetUrl() );
    }

    // Null parameter tests on IMediaEvent after rendering
    hr = NullParameterTest( pMediaEvent, true );
    if ( FAILED(hr) ) retval = TPR_FAIL;

    if ( !pTestDesc->RemoteGB() )
    {
        // This test renders a file and then calls CancelDefaultHandling and RestoreDefaultHandling
        // for the events which doesn't have default handlings.
        CancelRestoreDefaultHandling( true, pMediaEvent );
        CancelRestoreDefaultHandling( false, pMediaEvent );

        // This test renders a file and then calls RestoreDefaultHandling for the events which has default handling but we haven't done
        // CanceDefaultHandling.
        if ( E_INVALIDARG != pMediaEvent->RestoreDefaultHandling(EC_COMPLETE) )
        {
            LOG( TEXT("The default handler for EC_COMPLETE has not been cancelled yet.") );
        }
        if ( E_INVALIDARG != pMediaEvent->RestoreDefaultHandling(EC_REPAINT) )
        {
            LOG( TEXT("The default handler for EC_REPAINT has not been cancelled yet.") );
        }


        // This test renders a file and calls CancelDefaultHandling and RestoreDefaultHandling for the events which
        // has default handling
        hr = pMediaEvent->CancelDefaultHandling( EC_COMPLETE );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to cancel default handling for EC_COMPLETE. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
        }
        hr = pMediaEvent->CancelDefaultHandling( EC_REPAINT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to cancel default handling for EC_REPAINT. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
        }
        hr = pMediaEvent->RestoreDefaultHandling( EC_COMPLETE );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to restore default handling for EC_COMPLETE. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
        }
        hr = pMediaEvent->RestoreDefaultHandling( EC_REPAINT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to restore default handling for EC_REPAINT. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
        }
    }

    // perfore the following test only if media file has video in it.
    IMediaSeeking *pMediaSeeking = NULL;
    hr = testGraph.GetMediaSeeking( &pMediaSeeking );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaSeeking interface. (0x%08x)"), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    if ( !pMediaSeeking )
    {
        LOG( TEXT("NULL IMediaSeeking interface returned.") );
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( pMedia->m_bIsVideo && !pTestDesc->RemoteGB())
    {
        // rewind the file
        hr = testGraph.SetPosition( 0 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to rewind file using IMediaSeeking's SetPosition. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }

        // This test will render a video file and will not call CancelDefaultHandling on EC_REPAINT,
        // run the file and will change the destination position to generate the WM_PAINT message,
        // since we have default handling on EC_REPAINT, application will not receive
        // EC_REPAINT event.
        hr = DefaultRepaintEventHandlingTest( &testGraph, &bRepaint );
        if ( SUCCEEDED(hr) )
        {
            if ( false != bRepaint)
            {
                // should not get repaint event since the default event handler consumes it.
                retval = TPR_FAIL;
                goto cleanup;
            }
        }
        else
        {
            retval = TPR_FAIL;
            goto cleanup;
        }

        // reset the position
        hr = testGraph.SetPosition( 0 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to reset position to 0 using IMediaSeeking's SetPosition. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }

        // This test will render a video file and CancelDefaultHandling on EC_REPAINT, run the file
        // and will change the destination position to generate the WM_PAINT message, which we lead
        // to generating EC_REPAINT to application.
        hr = pMediaEvent->CancelDefaultHandling( EC_REPAINT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to cancel default handling for EC_REPAINT. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }
        hr = DefaultRepaintEventHandlingTest( &testGraph, &bRepaint );
        if ( SUCCEEDED(hr) )
        {
            // The DefaultRepaintEventHandlingTest() does not guarantee
            // EC_REPAINT message to be generated
            // BUGBUG: if ( bRepaint != true );
        }

        hr = pMediaEvent->RestoreDefaultHandling( EC_REPAINT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to restore default handling for EC_REPAINT. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    else
    {
        LOG( TEXT("IMediaEvent::Cancel/RestoreDefaultHandling (EC_REPAINT event) cannot be performed on this file, Skipping the test") );
    }

    // reset the position
    hr = testGraph.SetPosition( 0 );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to reset position to 0 using IMediaSeeking's SetPosition. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( !pTestDesc->RemoteGB() )
    {
        // This test will render a file which has two streams (Audio and Video) and CancelDefaultHandling
        // on EC_COMPLETE, run the file. should receive EC_COMPLETE for both Audio and video stream
        hr = pMediaEvent->CancelDefaultHandling( EC_COMPLETE );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to cancel default handling for EC_COMPLETE. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }

        hr = DefaultCompleteEventHandlingTest( &testGraph );
        if ( FAILED(hr) )
        {    // log generated inside the function
            // just save the result and continue the tests
            retval = TPR_FAIL;
        }

        hr = pMediaEvent->RestoreDefaultHandling( EC_COMPLETE );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to restore default handling for EC_COMPLETE. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    // reset the position
    hr = testGraph.SetPosition( 0 );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to reset position to 0 using IMediaSeeking's SetPosition. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // WaitForCompletion test.
    LOG( TEXT("WaitForCompletion test: Playing media file using WaitForCompletion") );
    hr = WaitForCompletionTest( &testGraph );
    if ( FAILED(hr) )
    {    // log generated inside the function.
        // save the result and continue the test
        retval = TPR_FAIL;
    }

    // Rewind the file.
    // reset the position
    hr = testGraph.SetPosition( 0 );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to reset position to 0 using IMediaSeeking's SetPosition. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    LOG( TEXT("Playing media file using GetEventHandle,WaitForMultipleObjects,GetEvent") );
    // Now perform the GetEvent and GetEventHandle test.
    hr = GetEventTest( &testGraph );
    if ( FAILED(hr) )
    {    // log generated inside the function.
        // save the result and continue the test
        retval = TPR_FAIL;
    }

cleanup:
    // Reset the testGraph and test desc
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    pTestDesc->Reset();

    return retval;
}


////////////////////////////////////////////////////////////////////////////////
// NullParameterTest()
// Performs the IMediaEvent interface tests with null parameters.
//
// Return value:
// TPR_PASS if the test passed,
// TPR_FAIL if the test fails, or possibly other special conditions.
//
HRESULT NullParameterTest( IMediaEvent *pMediaEvent, bool bRendered )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    LONG    lEventCode = 0;
    LONG    lParam1=0; LONG lParam2 = 0;

    if ( !pMediaEvent )
    {
        LOG( TEXT(" NULL IMediaEvent pointer passed." ) );
        return TPR_FAIL;
    }

    // Call GetEventHandle with NULL parameter without rendering a file gives exception!!!
    // The test is expected to throw an exception, so we wrap it in a try/except block.
    __try
    {
        hr = pMediaEvent->GetEventHandle( NULL );
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IMediaEvent GetEventHandle with NULL should fail. (%s)"),
                        __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
            retval = TPR_FAIL;
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        // As expected
        LOG( TEXT("%S: WARN %d@%S - IMediaEvent GetEventHandle with NULL throws an exception. (%s)"),
                        __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
    }

    // Call GetEvent with NULL parameters (all permuatations) without rendering a file,
    // it should return E_POINTER.
    if ( E_POINTER != pMediaEvent->GetEvent( NULL, &lParam1, &lParam2, 0 ) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEvent with NULL parameter didn't return E_POINTER. (%s)"),
                    __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
        retval = TPR_FAIL;
    }
    if ( E_POINTER != pMediaEvent->GetEvent( NULL, NULL, &lParam2, 0 ) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEvent with NULL parameter didn't return E_POINTER. (%s)"),
                    __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
        retval = TPR_FAIL;
    }
    if ( E_POINTER != pMediaEvent->GetEvent( NULL, &lParam1, NULL, 0 ) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEvent with NULL parameter didn't return E_POINTER. (%s)"),
                    __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
        retval = TPR_FAIL;
    }
    if ( E_POINTER != pMediaEvent->GetEvent( NULL, NULL, NULL, 0 ) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEvent with NULL parameter didn't return E_POINTER. (%s)"),
                    __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
        retval = TPR_FAIL;
    }
    if ( E_POINTER != pMediaEvent->GetEvent( &lEventCode, NULL, &lParam2, 0 ) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEvent with NULL parameter didn't return E_POINTER. (%s)"),
                    __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
        retval = TPR_FAIL;
    }
    if ( E_POINTER != pMediaEvent->GetEvent( &lEventCode, &lParam1, NULL, 0 ) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEvent with NULL parameter didn't return E_POINTER. (%s)"),
                    __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
        retval = TPR_FAIL;
    }
    if ( E_POINTER != pMediaEvent->GetEvent( &lEventCode, NULL, NULL, 0 ) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEvent with NULL parameter didn't return E_POINTER. (%s)"),
                    __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
        retval = TPR_FAIL;
    }

    // Calling the WaitForCompletion with NULL parameter without rendering a file
    // gives an exception.  The test is expected to throw an exception, so we wrap
    // it in a try/exception block
    __try
    {
        hr = pMediaEvent->WaitForCompletion( 0, NULL );
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IMediaEvent WaitForCompletion with NULL should fail. (%s)"),
                        __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
            hr = E_FAIL;
            goto cleanup;
        }
        else
            hr = S_OK;
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        // as expected
        LOG( TEXT("%S: WARN %d@%S - IMediaEvent WaitForCompletion with NULL throws an exception. (%s)"),
                        __FUNCTION__, __LINE__, __FILE__, bRendered ? TEXT("after render") : TEXT("before render") );
    }

cleanup:

    return hr;
}

////////////////////////////////////////////////////////////////////////////////
// WaitForCompletionTest()
// Performs the WaitForCompletion method test for each media file.
//
// Return value:
// TPR_PASS if the test passed,
// TPR_FAIL if the test fails, or possibly other special conditions.
//
HRESULT
WaitForCompletionTest( TestGraph *pGraph )
{
    HRESULT hr = S_OK;
    IMediaEvent *pMediaEvent = NULL;

    long EvCode=0;
    bool bWaitForCompTestFailed = false;
    UINT uNumber = 0;
    PlaybackMedia *pMedia = NULL;

    if ( !pGraph )
    {
        LOG( TEXT(" NULL TestGraph pointer passed." ) );
        hr = E_FAIL;
        goto cleanup;
    }
    pMedia = pGraph->GetCurrentMedia();
    if ( !pMedia )
    {
        LOG( TEXT("failed to retrieve media object") );
        hr = E_FAIL;
        goto cleanup;
    }
    
    hr = pGraph->GetMediaEvent( &pMediaEvent );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaEvent interface. (0x%08x)"), hr );
        goto cleanup;
    }

    // Duration of the media in millisecond
    DWORD dwDuration = (DWORD)(pMedia->duration / 10000);

    // This test renders a file and then calls the function with valid parameters.
    hr = pMediaEvent->WaitForCompletion(0, &EvCode);
    if ( VFW_E_WRONG_STATE != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - WaitForCompletion w/ timeout 0 didn't return VFW_E_WRONG_STATE before running the graph. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        hr = E_FAIL;
        goto cleanup;
    }

    // This test renders a file ,play and wait for x millisecond where x < duration
    // of the file.
    hr = pGraph->SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to Running for media %s. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        goto cleanup;
    }

    // Wait for 0 millisecond.
    if ( E_ABORT != pMediaEvent->WaitForCompletion(0,&EvCode) )
    {
        LOG( TEXT("%S: ERROR %d@%S - WaitForCompletion with timeout 0 didn't return E_ABORT. "),
                    __FUNCTION__, __LINE__, __FILE__ );
        hr = E_FAIL;
        goto cleanup;
    }


    // Now wait for x milliseconds where x < duration of the media play.
    DWORD dwStartTime, dwTotalTime;
    DWORD dwTmpDur = 0; // Cumulative duration.

    // Perform the WaitForCompletion till half of the media play duration.
    while( dwTmpDur < dwDuration / 6 )
    {
        // BUGBUG: configurable is better.
        // Generate a random wait time
        rand_s(&uNumber);
        DWORD dwWaitTime = uNumber % (dwDuration / 8 + 50);

        // Boost thread priority to disallow CE scheduling as cause for tolerance failure
        SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );

        dwStartTime = GetTickCount();
        hr = pMediaEvent->WaitForCompletion( dwWaitTime, &EvCode );
        dwTotalTime = GetTickCount() - dwStartTime;

        // Return to normal thread priority
        SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );

        if (E_ABORT != hr)
        {
            LOG( TEXT("Duration: %d,  Wait time passed: %d, Actual wait time: %d"),
                        dwDuration, dwWaitTime, dwTotalTime );
            LOG( TEXT("WaitForCompletion returned 0x%x, Event Code: 0x%x"), hr, EvCode);
            hr = E_FAIL;
            goto cleanup;
        }

        // The wait time should always be larger than passed wait time
        if ( dwTotalTime < dwWaitTime )
        {
            LOG( TEXT(" The wait time should always be larger than passed wait time.") );
            hr = E_FAIL;
            goto cleanup;
        }

        // Toterance of 6 TICKS
        if ( !ToleranceCheckAbs( dwTotalTime, dwWaitTime, max(25 , TICKS_TO_MSEC(6)) ) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IMediaEvent::WaitForCompletion, Wait time passed = %d, Actual wait time(TickCount) = %d, Difference = %d, Tolerance = %d"),
                            __FUNCTION__, __LINE__, __FILE__,
                            dwWaitTime,dwTotalTime, dwWaitTime-dwTotalTime, max(25 , TICKS_TO_MSEC(6)));
            bWaitForCompTestFailed = true;      // Test has failed.
            hr = E_FAIL;
            goto cleanup;
        }
        dwTmpDur += dwTotalTime;
    }


    hr = pGraph->SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to Stopped for media %s(0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        goto cleanup;
    }

    //This test renders a file, play and Wait for INFINITE with valid parameters.
    hr = pGraph->SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to Running for media %s(0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        goto cleanup;
    }

    // Wait for INFINITE.
    hr = pMediaEvent->WaitForCompletion( (long)(2 * pMedia->duration / 10000), &EvCode );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - WaitForCompletion for %d ms failed. (0x%x)" ),
                __FUNCTION__, __LINE__, __FILE__, 2 * pMedia->duration / 10000, hr );
        goto cleanup;
    }
    if ( EC_COMPLETE != EvCode )
    {
        LOG( TEXT("Event code returned is not equal to EC_COMPLETE.") );
        DisplayECEvent( EvCode );
        hr = E_FAIL;
        goto cleanup;
    }


cleanup:

    hr = pGraph->SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to stopped for media %s(0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
    }

    return hr;
}


////////////////////////////////////////////////////////////////////////////////
// GetEventTest()
// Performs the GetEventHandle and GetEvent Test.
//
// Return value:
// TPR_PASS if the test passed,
// TPR_FAIL if the test fails, or possibly other special conditions.
//
HRESULT
GetEventTest( TestGraph *pGraph )
{
    bool bGetEventTestFailed = false;
    long hEvent = 0;
    long lEventCode=0;
    long lParam1 = 0;
    long lParam2 = 0;
    DWORD dwTimeOutTickCount = 0;
    HRESULT hr = S_OK;
    UINT uNumber = 0;

    if ( !pGraph )
    {
        LOG( TEXT(" NULL TestGraph pointer passed." ) );
        return TPR_FAIL;
    }
    PlaybackMedia *pMedia = pGraph->GetCurrentMedia();
    if ( !pMedia )
    {
        LOG( TEXT("failed to retrieve media object") );
        hr = E_FAIL;
        goto cleanup;
    }
    IMediaEvent *pMediaEvent = NULL;
    hr = pGraph->GetMediaEvent( &pMediaEvent );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaEvent interface. (0x%08x)"), hr );
        goto cleanup;
    }

    // This test renders a file and then calls the function with valid parameters (zero timeout),
    // but before any events should be waiting.
    hr = pMediaEvent->GetEvent( &lEventCode, &lParam1, &lParam2, 0 );
    // EC_BANDWIDTHCHANGE is ok here for streaming media
    // BUGBUG: for streaming: if ( E_ABORT != hr && EC_BANDWIDTHCHANGE != hr )
    if ( E_ABORT != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaEvent::GetEvent returns 0x%x when passed 0 timeout"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        DisplayECEvent( lEventCode );
        hr = E_FAIL;
        goto cleanup;
    }

    // This test renders a file and then calls the function with valid parameter (x millisecond),
    // but before any events should be waiting.
    // BUGBUG: random number is not a good idea.
    rand_s(&uNumber);
    int count = uNumber % 20 + 5;
    DWORD dwStartTickCount=0;DWORD dwTotalCount=0;
    while ( count-- )
    {
        rand_s(&uNumber);
        DWORD dwWaitTime = uNumber % 300 + 150;

        // Boost thread priority to disallow CE scheduling as cause for tolerance failure
        SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );

        dwStartTickCount = GetTickCount();
        hr = pMediaEvent->GetEvent( &lEventCode, &lParam1, &lParam2, dwWaitTime );
        if ( E_ABORT != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetEvent didn't return E_ABORT within %d ms before any events. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, dwWaitTime, hr );
            hr = E_FAIL;

            // Return to normal thread priority
            SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );
            goto cleanup;
        }
        dwTotalCount = GetTickCount() - dwStartTickCount;

        // Return to normal thread priority
        SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );

        // The wait time should always be larger than passed wait time
        if ( dwTotalCount < dwWaitTime )
        {
            LOG( TEXT( "The wait time should always be larger than passed wait time." ) );
            hr = E_FAIL;
        }

        // Tolerance of 20 tick count or 25ms which ever is larger
        if ( !ToleranceCheckAbs( dwTotalCount, dwWaitTime, max(25 , TICKS_TO_MSEC(20)) ) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IMediaEvent::GetEvent, FAILED: Wait time passed = %d, Actual wait time(TickCount) = %d, Difference = %d, Tolerance = %d"),
                            __FUNCTION__, __LINE__, __FILE__,
                            dwWaitTime, dwTotalCount, dwWaitTime-dwTotalCount, max(25 , TICKS_TO_MSEC(20)) );
            bGetEventTestFailed = true;
            hr = E_FAIL;
        }
    }

    // This test renders a file  and then calls the function with a valid parameter.
    // Call WaitForMultipleObjects with this handle and play the file till EC_COMPLETE event is catched.
    // This test renders and runs file and then calls the function with valid parameters (zero timeout),
    // at times where events should be waiting.
    LONGLONG llDuration = 0;
    hr = pGraph->GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get duration. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    dwTimeOutTickCount = (DWORD)(GetTickCount() + llDuration / 10000 + 10000);

    // Run the filter graph
    hr = pGraph->SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to Running for media %s(0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        goto cleanup;
    }

    // Get the event handle.
    hr = pMediaEvent->GetEventHandle( &hEvent );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEventHandle failed(0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    while( dwTimeOutTickCount > GetTickCount() )
    {
        // Use while here because we may have events other than EC_COMPLETE

        LOG( TEXT("Waiting for media event...") );

        // Wait on the event handle.. This will also check the validity of event handle
        // returned by GetEventHandle.
        hr = WaitForMultipleObjects( 1,
                                     (const HANDLE *)&hEvent,
                                     FALSE,
                                     DWORD(2 * llDuration / 10000) );
        if ( WAIT_TIMEOUT == hr )
        {
            LOG( TEXT( "WaitForMultipleObjects timed out." ) );
            hr = E_FAIL;
            goto cleanup;
        }

        // Get the event.
        hr = pMediaEvent->GetEvent( &lEventCode, &lParam1, &lParam2, 0 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetEvent failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }

        DisplayECEvent( lEventCode );

        // We have received the event. Break only if we get EC_COMPLETE event.
        if ( EC_COMPLETE == lEventCode )
        {
            // free event parameter
            hr = FreeEventParams( pMediaEvent, lEventCode, lParam1, lParam2 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Free event paras failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }
            break;
        }

        // Free the media event resources.
        // looks like EC_OLE_EVENTs are only supposed to have lParam1.
        hr = FreeEventParams( pMediaEvent, lEventCode, lParam1, lParam2 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Free event paras failed. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }
    }    // end of while

cleanup:

    hr = pGraph->SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to stopped for media %s(0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
    }
    return hr;
}


////////////////////////////////////////////////////////////////////////////////
// DefaultRepaintEventHandlingTest()
// Ensure EC_REPAINT event is received when resize video image during playback of
// a media file.
//
// Return value:
// TPR_PASS if the test passed,
// TPR_FAIL if the test fails, or possibly other special conditions.
//
HRESULT
DefaultRepaintEventHandlingTest( TestGraph *pGraph, bool *pbRepaint )
{
    bool bTestFailed = true;
    long lEventCode=0;
    long lParam1 = 0;
    long lParam2 = 0;
    long top =0;long left=0;long width=0;long height=0;
    LONGLONG llDuration = 0;
    DWORD dwTimeOutTickCount = 0;

    HRESULT hr = S_OK;
    IMediaEvent *pMediaEvent = NULL;
    PlaybackMedia *pMedia = NULL;

    if ( !pGraph )
    {
        LOG( TEXT(" NULL TestGraph pointer passed." ) );
        hr = E_FAIL;
        goto cleanup;
    }
    pMedia = pGraph->GetCurrentMedia();
    if ( !pMedia )
    {
        LOG( TEXT("failed to retrieve media object") );
        hr = E_FAIL;
        goto cleanup;
    }
    
    hr = pGraph->GetMediaEvent( &pMediaEvent );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaEvent interface. (0x%08x)"), hr );
        goto cleanup;
    }

    IBasicVideo *pBasicVideo = NULL;
    hr = pGraph->GetBasicVideo( &pBasicVideo );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IBasicVideo interface. (0x%08x)"), hr );
        goto cleanup;
    }

    LOG( TEXT("Perform DefaultRepaintEventHandlingTest...") );

    *pbRepaint = false; // Initialize;

    hr = pGraph->GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get duration. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }
    hr = pGraph->SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to running. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }
    hr = pBasicVideo->GetDestinationPosition( &top, &left, &width, &height );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IBasicVideo failed to get destination position. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    dwTimeOutTickCount = (DWORD)(GetTickCount() + llDuration / 10000 + 10000);
    LOG( TEXT("  Duration of the media: %d ms"), llDuration / 10000 );
    LOG( TEXT("  Set to wait up to %d seconds\r\n"), (dwTimeOutTickCount - GetTickCount()) / 1000 );

    LOG( TEXT("%S: Playing file till EC_COMPLETE or EC_REPAINT event..."), __FUNCTION__ );

    // Play the file till end or till we get EC_REPAINT event.
    while( dwTimeOutTickCount > GetTickCount() )
    {
        // Get the event.
        hr = pMediaEvent->GetEvent( &lEventCode, &lParam1, &lParam2, 0 );
        if ( SUCCEEDED(hr) )
            DisplayECEvent(lEventCode);

        if ( (E_ABORT != hr) && (EC_COMPLETE == lEventCode) )
        {
            // Free the media event resources.
            hr = FreeEventParams( pMediaEvent, lEventCode, lParam1, lParam2 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("FreeEventParams failed. (0x%08x)"), hr );
                goto cleanup;
            }
            bTestFailed = false;
            break;
        }
        else if ( (E_ABORT != hr) && (EC_REPAINT == lEventCode) )
        {
            // We have got the repaint event!!!
            hr = FreeEventParams( pMediaEvent, lEventCode, lParam1, lParam2 );
            *pbRepaint = true;
            bTestFailed = false;
            if ( FAILED(hr) )
            {
                LOG( TEXT("FreeEventParams failed. (0x%08x)"), hr );
                goto cleanup;
            }
            break;
        }
        else if ( E_ABORT != hr )
        {
            hr = FreeEventParams( pMediaEvent, lEventCode, lParam1, lParam2 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("FreeEventParams failed. (0x%08x)"), hr );
                goto cleanup;
            }
        }

        // Allow DShow core to catch up
        Sleep( 1 );

        // This is required to generate the WM_PAINT message. This is a minimum and
        // valid destination position.
        hr = pBasicVideo->SetDestinationPosition( 0, 0, width/2, height/2 );
        if( E_NOINTERFACE == hr )
        {
            LOG( TEXT("IMediaEvent::Cancel/RestoreDefaultHandling (EC_REPAINT event) cannot be performed on video file. IBasicVideo interface not supported. Skipping the test") );
            hr = E_FAIL;
            goto cleanup;
        }
        else if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IBasicVideo SetDestinationPosition w/ (0,0,%d,%d) failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, width/2, height/2, hr );
            goto cleanup;
        }
    }

    LOG( TEXT("%S: Left while() loop..."), __FUNCTION__ );

    // Set the destination points to default value.
    hr = pBasicVideo->SetDefaultDestinationPosition();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IBasicVideo failed to set default dest position. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }
    // Fail we if couldn't receive EC_COMPLETE or EC_REPAINT and we broke with the timeout.
    if ( bTestFailed )
    {
        LOG( TEXT(" Test failed: we couldn't receive EC_COMPLETE or EC_REPAINT and we broke with the timeout.") );
        hr = E_FAIL;
    }

cleanup:

    hr = pGraph->SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to stopped for media %s(0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
    }
    return hr;
}


////////////////////////////////////////////////////////////////////////////////
// DefaultCompleteEventHandlingTest()
// Ensure the EC_COMPLETE received after playing back a media file.
//
// Return value:
// TPR_PASS if the test passed, tr
// TPR_FAIL if the test fails, or possibly other special conditions.
//
HRESULT
DefaultCompleteEventHandlingTest( TestGraph *pGraph )
{
    long lEventCode=0;
    long lParam1 = 0;
    long lParam2 = 0;
    DWORD dwTimeOutTickCount = 0;
    DWORD dwGotFirstECCompleteTickCount = 0;
    int nOfECComplete = 0; // Number of EC_COMPLETE events received.
    HRESULT hr = S_OK;

    if ( !pGraph )
    {
        LOG( TEXT(" NULL TestGraph pointer passed." ) );
        return TPR_FAIL;
    }
    PlaybackMedia *pMedia = pGraph->GetCurrentMedia();
    if ( !pMedia )
    {
        LOG( TEXT("failed to retrieve media object") );
        hr = E_FAIL;
        goto cleanup;
    }
    IMediaEvent *pMediaEvent = NULL;
    hr = pGraph->GetMediaEvent( &pMediaEvent );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaEvent interface. (0x%08x)"), hr );
        goto cleanup;
    }

    LONGLONG llDuration;
    hr = pGraph->GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaSeeking failed to get duration. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    // Play the media file.
    hr = pGraph->SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to Running for media %s(0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        goto cleanup;
    }

    dwTimeOutTickCount = (DWORD)( GetTickCount() +llDuration/10000 + 10000 );

    LOG( TEXT("  Duration of the media: %d ms"), llDuration/10000 );
    LOG( TEXT("  Set to wait up to %d seconds\r\n"), (dwTimeOutTickCount - GetTickCount()) / 1000 );

    LOG( TEXT("%S: Playing file till end (EC_COMPLETE event)..."), __FUNCTION__ );

    // Play the files till end .
    while( dwTimeOutTickCount > GetTickCount() )
    {
        // Get the event.
        hr = pMediaEvent->GetEvent( &lEventCode, &lParam1, &lParam2, 0 );

        if ( SUCCEEDED(hr) )
            DisplayECEvent( lEventCode );

        if ( (E_ABORT != hr) && (EC_COMPLETE == lEventCode) )
        {
            // Free the media event resources.
            hr = FreeEventParams( pMediaEvent, lEventCode, lParam1, lParam2 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("FreeEventParams failed. (0x%08x)"), hr );
                goto cleanup;
            }

            nOfECComplete++;
            dwGotFirstECCompleteTickCount = GetTickCount();
            if( nOfECComplete == 2 )
            {
                // Got both EC_COMPLETE event!!!
                LOG( TEXT("Got Two EC_COMPLETE events [audio and video stream]") );
                break;
            }

        }
        else if ( E_ABORT != hr )
        {
            hr = FreeEventParams( pMediaEvent, lEventCode, lParam1, lParam2 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("FreeEventParams failed. (0x%08x)"), hr );
                goto cleanup;
            }
        }

        // If we have already got one EC_COMPLETE, then give enough time (5 Sec) to receive
        // the second EC_COMPLETE. If we don't receive second EC_COMPLETE in next one second.
        // then assume that it has only one stream (either audio or video).
        if ( (nOfECComplete) && ( GetTickCount() - dwGotFirstECCompleteTickCount > 5000 ) )
        {
            LOG( TEXT("Got only one EC_COMPLETE event, probably media file has only one stream (audio or video)") );
            goto cleanup;
        }

        // Allow DShow core to catch up
        Sleep(1);
    }

    // We should have received atleast one EC_COMPLETE.
    if ( nOfECComplete == 0 )
    {
        LOG( TEXT(" We should have received at least one EC_COMPLETE.") );
        hr = E_FAIL;
    }

cleanup:

    hr = pGraph->SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to stopped for media %s(0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
    }
    return hr;
}