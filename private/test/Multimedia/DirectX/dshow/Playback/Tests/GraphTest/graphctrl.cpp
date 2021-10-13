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
//  Perform IMediaControl interface tests.
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


HRESULT RandomRSPTest( TestGraph *pGraph );
HRESULT GetStateTest( TestGraph *pGraph, int filterState );
HRESULT StopWhenReadyTest( TestGraph *pGraph );
HRESULT VerifyFilterState( TestGraph *pGraph, int filterState );
HRESULT IMediaControlManualTest( TestGraph *pGraph );
UINT uNumber = 0;
//
// Runs MediaControl test on media file (AVI, MPeg, Wav etc)
// Return value:
// TPR_PASS if the test passed
// TPR_FAIL if the test failed
// or possibly other special conditions.
//

TESTPROCAPI
IMediaControlTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    // get IMediaControl interface pointer
    IMediaControl *pMediaControl = NULL;
    hr = testGraph.GetMediaControl( &pMediaControl );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaControl interface. (0x%08x)"), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    OAFilterState state  = State_Stopped;
    hr = pMediaControl->Run();
    if ( E_NOINTERFACE != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl::Run() didn't return E_NOINTERFACE before rendering file. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pMediaControl->Pause();
    if ( E_NOINTERFACE != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl::Pause() didn't return E_NOINTERFACE before rendering file. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // This test will call Stop without rendering the file.
    hr = pMediaControl->Stop();
    if ( S_OK == hr )
    {
        LOG( TEXT("%S: WARNING %d@%S - IMediaControl::Stop()should fail before rendering file. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
    }

    // This test will call GetState with null parameters (Without rendering the file).
    hr = pMediaControl->GetState( 0, NULL );
    if ( E_POINTER != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl->GetState(0, NULL) didn't return E_POINTER before rendering file. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    //This test will call GetState with valid parameters (Without rendering the file)
    hr = pMediaControl->GetState(0, &state);
    if ( S_OK == hr )
    {
        LOG( TEXT("%S: WARNING %d@%S - IMediaControl->GetState() should fail before rendering file. (0x%08x)"),
            __FUNCTION__, __LINE__, __FILE__, hr );
    }

    // This test will call StopWhenReady before rendering a media file.
    if ( SUCCEEDED( pMediaControl->StopWhenReady() ) )
    {
        LOG( TEXT("%S: WARNING %d@%S - IMediaControl->StopWhenReady() should fail before rendering file. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
    }

    // Render the file, which is done inside BuildGraph
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to render media %s (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else {
        LOG( TEXT("%S: successfully render %s"), __FUNCTION__, pMedia->GetUrl() );
    }

    // This test will call GetState with null parameters (After rendering the file).
    hr = pMediaControl->GetState( 0, NULL );
    if ( E_POINTER != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl->GetState(0, NULL) didn't return E_POINTER after rendering file. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
        hr = S_OK;

    // This test will render a video media file and Changes the media position and calls
    // StopWhenReady. New frame should be displayed (Corresponding to the new position)
    // each time.
    if ( pMedia->m_bIsVideo )
    {
        hr = StopWhenReadyTest( &testGraph );
        if ( FAILED(hr) )
        {
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    // This test will call Run/Pause/Stop after rendering the file. In addition this test will call
    // Stop, Pause randomely and check the the filter state using GetState(). In the GetState test a random
    // wait time is passed. If return value is  VFW_S_STATE_INTERMEDIATE then measure the wait time of GetState.
    hr = RandomRSPTest( &testGraph );

cleanup:
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    pTestDesc->Reset();

    return retval;
}


////////////////////////////////////////////////////////////////////////////////
// RandomRSPTest
// Performs test on Stop/Pause/Run and GetState by randomely calling them
//
// Return value:
// TPR_PASS if the media file Runs, TPR_FAIL if something goes wrong,
//
HRESULT RandomRSPTest( TestGraph *pGraph )
{
    HRESULT hr = S_OK;
    PlaybackMedia *pMedia = NULL;

    if ( !pGraph )
    {
        LOG( TEXT("%S: ERROR %d@%S - NULL TestGraph pointer!"),
                    __FUNCTION__, __LINE__, __FILE__);
        return E_POINTER;
    }
    // get IMediaControl interface pointer
    IMediaControl *pMediaControl = NULL;
    hr = pGraph->GetMediaControl( &pMediaControl );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaControl interface. (0x%08x)"), hr );
        goto cleanup;
    }

    pMedia = pGraph->GetCurrentMedia();
    if ( !pMedia )
    {
        LOG( TEXT("Failed to get Media object.") );
        hr = E_FAIL;
        goto cleanup;
    }

    OAFilterState state  = State_Stopped; // Initialize the filter state as Stopped sate.
    int filter_state = 2;   // Running state.
    rand_s(&uNumber);
    int nCount = uNumber % 20 + 30; // Loop count varies from 30 to 50.

    // Run filter graph
    hr = pGraph->SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to change state to running for media %s(0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        goto cleanup;
    }

    hr = pMediaControl->GetState( 0, &state );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl failed to get current state. (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    if ( State_Running != state )
    {
        LOG( TEXT("%S: ERROR %d@%S - After running the graph, the state (%s) is not (%s)."),
                    __FUNCTION__, __LINE__, __FILE__,
                GetStateName( (FILTER_STATE)state ), GetStateName(State_Running) );
        hr = E_FAIL;
        goto cleanup;
    }

    // Call Run Stop and Pause randomely.
    int rand_num;
    while ( nCount-- )
    {
        rand_s(&uNumber);
        rand_num = uNumber % 3;

        // Make sure the rand generated filter state is not equal to last time generated filter state.
        filter_state = ( (rand_num == filter_state) ? (filter_state + 1) % 3 : rand_num );
        switch( filter_state )
        {
            case 0: // Stop
            {    // Stop filter graph
                rand_s(&uNumber);
                if ( uNumber%2 )
                {
                    hr = pMediaControl->Stop();
                    if ( FAILED(hr) )
                    {
                        LOG( TEXT("%S: ERROR %d@%S - IMediaControl failed to stop. (0x%x)"),
                                __FUNCTION__, __LINE__, __FILE__, hr );
                        goto cleanup;
                    }
                }
                else
                {
                    hr = pMediaControl->StopWhenReady();
                    if ( FAILED(hr) )
                    {
                        LOG( TEXT("%S: ERROR %d@%S - IMediaControl StopWhenReady failed. (0x%x)"),
                                __FUNCTION__, __LINE__, __FILE__, hr );
                        goto cleanup;
                    }
                }
            }
            break;
            case 1: // Pause
            {    // Pause filter graph
                hr = pMediaControl->Pause();
                if ( FAILED(hr) )
                {
                    LOG( TEXT("%S: ERROR %d@%S - IMediaControl failed to pause. (0x%x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                    goto cleanup;
                }
            }
            break;
            case 2: // Run
            {    // Run filter graph
                hr = pMediaControl->Run();
                if ( FAILED(hr) )
                {
                    LOG( TEXT("%S: ERROR %d@%S - IMediaControl failed to run. (0x%x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                    goto cleanup;
                }
            }
            break;
        default:
            // should not reach here.
            LOG( TEXT("%S: ERROR %d@%S - Invalid Filter Graph state."), __FUNCTION__, __LINE__, __FILE__ );
            break;
        }

        // Perform either GetState or VerifyFilterState for stop, pause, run
        rand_s(&uNumber);
        if ( uNumber%2 )
            hr = GetStateTest( pGraph, filter_state );
        else
            hr = VerifyFilterState( pGraph, filter_state );
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
// GetStateTest
// Performs GetState method test with different wait time.
//
// Return value:
// TPR_PASS if the media file Runs, TPR_FAIL if something goes wrong,
//
HRESULT GetStateTest( TestGraph *pGraph, int filterState )
{
    OAFilterState state; // Filter state
    bool  bGetStateFailed = false;
    DWORD dwStartTime = 0;
    DWORD dwTotalTime = 0;
    rand_s(&uNumber);
    DWORD dwWaitTime = uNumber % 100 + 250;  // Wait time ranges from 250 to 350 millisec.

    HRESULT hr = S_OK;

    if ( !pGraph )
    {
        LOG(TEXT("%S: ERROR %d@%S - NULL TestGraph pointer!"),
                __FUNCTION__, __LINE__, __FILE__);
        return E_POINTER;
    }
    // get IMediaControl interface pointer
    IMediaControl *pMediaControl = NULL;
    hr = pGraph->GetMediaControl( &pMediaControl );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaControl interface. (0x%08x)"), hr );
        goto cleanup;
    }

    LOG( TEXT("PerformGetStateTest: filter_state=%d  Calling pMediaControl->GetState()"), filterState);

    // avoid thread being block for too long. See#18724
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );

    dwStartTime = GetTickCount();
    hr = pMediaControl->GetState( dwWaitTime, &state );
    dwTotalTime = GetTickCount() - dwStartTime;

    // set priority back to normal. see #18724.
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );

    if ( VFW_S_STATE_INTERMEDIATE == hr )
    {
        // The wait time should always be larger than passed wait time
        LOG(TEXT("IMediaControl::GetState() returns INTERMEDIATE_STATE, wait-time passed= %d, actual wait-time (TickCount) = %d"),
                dwWaitTime, dwTotalTime);

        if ( dwTotalTime < dwWaitTime )
        {
            LOG( TEXT(" The wait time should always be larger than passed wait time." ) );
        }

        // Filter graph is in INTERMEDIATE state. In this case we will be able to measure the wait time.
        if ( !ToleranceCheckAbs( dwTotalTime, dwWaitTime, TICKS_TO_MSEC(6) ) )
        {
            LOG(TEXT("IMediaControl::GetState() returns INTERMEDIATE_STATE, wait time doesn't match, wait-time passed= %d, actual wait-time (TickCount) = %d, Tolerance = %d"),
                       dwWaitTime, dwTotalTime, max(25 , TICKS_TO_MSEC(6)) );
            bGetStateFailed = true;
        }
    }
    else
    {
        LOG(TEXT("%S: INFO %d@%S - pMediaControl->GetState() returned hr=0x%x"),
                        __FUNCTION__, __LINE__, __FILE__, hr );

        // State transition is complete.
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IMediaControl GetState failed. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }

        if ( filterState != int(state) )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetState returned %s, does not match passed state %s."),
                    __FUNCTION__, __LINE__, __FILE__, GetStateName( (FILTER_STATE)state ), GetStateName( (FILTER_STATE)filterState) );
            hr = E_FAIL;
            goto cleanup;
        }
    }

cleanup:
    return hr;
}

////////////////////////////////////////////////////////////////////////////////
// StopWhenReadyTest
// Performs StopWhenReady method test
//
// Return value:
// TPR_PASS if the media file Runs, TPR_FAIL if something goes wrong,
//
HRESULT StopWhenReadyTest( TestGraph *pGraph )
{
    rand_s(&uNumber);
    int count = uNumber % 20 + 10; // Count ranges from 10 to 30
    int nStartMultiplier, nStopMultiplier, nTemp;
    LONGLONG llStartPos, llStopPos, llDuration;
    DWORD dwCap = AM_SEEKING_CanSeekAbsolute; // Seeking Cap flag.

    HRESULT hr = S_OK;

    if ( !pGraph )
    {
        LOG(TEXT("%S: ERROR %d@%S - NULL TestGraph pointer!"),
                __FUNCTION__, __LINE__, __FILE__);
        return E_POINTER;
    }
    // get IMediaControl interface pointer
    IMediaControl *pMediaControl = NULL;
    hr = pGraph->GetMediaControl( &pMediaControl );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaControl interface. (0x%08x)"), hr );
        goto cleanup;
    }
    // get IMediaSeeking interface pointer
    IMediaSeeking *pMediaSeeking = NULL;
    hr = pGraph->GetMediaSeeking( &pMediaSeeking );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaSeeking interface. (0x%08x)"), hr );
        goto cleanup;
    }

    // Get the duration of the media
    hr = pMediaSeeking->GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
        LOG(TEXT("%S: ERROR %d@%S - IMediaSeeking failed to get duration! (0x%08x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    while( count-- )
    {
        // Generate random Start and Stop time.
        rand_s(&uNumber);
        nStartMultiplier = (uNumber%100);
        rand_s(&uNumber);
        nStopMultiplier  = (uNumber%100);

        //Make them well ordered
        if( nStartMultiplier>nStopMultiplier )
        {    //Swap 'em
            nTemp = nStartMultiplier;
            nStartMultiplier = nStopMultiplier;
            nStopMultiplier = nTemp;
        }
        //Positions are in percentages so convert
        llStartPos = (llDuration * nStartMultiplier) / 100;
        llStopPos  = (llDuration * nStopMultiplier) / 100;
        if ( S_OK == pMediaSeeking->CheckCapabilities(&dwCap) )
        {
            hr = pMediaSeeking->SetPositions( &llStartPos,
                                              AM_SEEKING_AbsolutePositioning,
                                              &llStopPos,
                                              AM_SEEKING_AbsolutePositioning);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IMediaSeeking SetPositions failed. start: %d, stop: %d (0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, llStartPos, llStopPos, hr );
                goto cleanup;
            }

            // StopWhenReady should ensure that new image is repainted after every position change.
            hr = pMediaControl->StopWhenReady();
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IMediaControl StopWhenReady failed! (0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            // Wait so that we can see the new image.
            rand_s(&uNumber);
            Sleep( uNumber%200 + 200 );
        }
    }    // end of while.

    // Set positions to default.
    if ( S_OK == pMediaSeeking->CheckCapabilities(&dwCap) )
    {
        llStartPos = 0;
        llStopPos = llDuration;
        hr = pMediaSeeking->SetPositions( &llStartPos,
                                          AM_SEEKING_AbsolutePositioning,
                                          &llStopPos,
                                          AM_SEEKING_AbsolutePositioning);
        if ( FAILED(hr) )
        {
            LOG(TEXT("%S: ERROR %d@%S - IMediaSeeking set default pos failed. start: %d, stop: %d (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, llStartPos, llStopPos, hr );
            goto cleanup;
        }
    }

cleanup:
    return hr;
}

////////////////////////////////////////////////////////////////////////////////
// VerifyFilterState
// Verifies filter state through MediaSeeking. If the filter is in Stop or Paused state
// the current position should never move. In run state the current positions should be
// greater than the previous current position.
//
// Return value:
// TPR_PASS if the media file Runs, TPR_FAIL if something goes wrong,
//
HRESULT VerifyFilterState( TestGraph *pGraph, int filterState )
{
    int count = 10;
    LONGLONG llLastPosition, llCurrentPosition, llDuration;
    OAFilterState fs;

    HRESULT hr = S_OK;

    if ( !pGraph )
    {
        LOG(TEXT("%S: ERROR %d@%S - NULL TestGraph pointer!"),
                __FUNCTION__, __LINE__, __FILE__);
        return E_POINTER;
    }
    // get IMediaControl interface pointer
    IMediaControl *pMediaControl = NULL;
    hr = pGraph->GetMediaControl( &pMediaControl );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaControl interface. (0x%08x)"), hr );
        goto cleanup;
    }
    // get IMediaSeeking interface pointer
    IMediaSeeking *pMediaSeeking = NULL;
    hr = pGraph->GetMediaSeeking( &pMediaSeeking );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaSeeking interface. (0x%08x)"), hr );
        goto cleanup;
    }

    LOG(TEXT("VerifyFilterState: filter_state=%d  Calling pMediaSeeking->GetDuration()"),
                    filterState);

    hr = pMediaSeeking->GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
        LOG(TEXT("%S: ERROR %d@%S - IMediaSeeking GetDuration failed! (0x%08x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    switch(filterState)
    {
        case 0: // Stop
        {
            LOG(TEXT("VerifyFilterState: (FilterState STOP) Calling pMS->GetState(2000,&fs)"));

            //make sure the filter has stopped (2 sec time out)
            hr = pMediaControl->GetState( 2000,&fs );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - IMediaControl GetState failed! (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            hr = pMediaSeeking->GetCurrentPosition( &llLastPosition );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - IMediaControl GetCurrentPosition failed! (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            while ( (count--) && (llLastPosition < llDuration) )
            {
                hr = pMediaSeeking->GetCurrentPosition( &llCurrentPosition );
                if ( FAILED(hr) )
                {
                    LOG(TEXT("%S: ERROR %d@%S - IMediaControl GetState failed! (0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                    goto cleanup;
                }
                Sleep(5);
                // Since filter is in stop state. Current positions should be equal
                // to previous current positions..
                if ( llCurrentPosition != llLastPosition )
                {
                    LOG( TEXT(" Stop State: Current positions should be equal to previous current position.") );
                    hr = E_FAIL;
                    goto cleanup;
                }
                llLastPosition = llCurrentPosition;
            }

            break;
        }
        case 1: // Pause
        {
            LOG(TEXT("VerifyFilterState: (FilterState PAUSE) Calling pMS->GetState(1000,&fs)"));

            // Make sure the filters are paused; 1s timeout
            hr = pMediaControl->GetState( 1000, &fs );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - IMediaControl GetState failed! (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            if ( State_Paused != fs )
            {
                LOG( TEXT( "The state get (%s) is not %s." ),
                        GetStateName( (FILTER_STATE)fs ), GetStateName( (FILTER_STATE)State_Paused ) );
                hr = E_FAIL;
                goto cleanup;
            }

            hr = pMediaSeeking->GetCurrentPosition( &llLastPosition );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - IMediaSeeking GetCurrentPosition failed! (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            while ( (count--) && (llLastPosition < llDuration) )
            {
                hr = pMediaSeeking->GetCurrentPosition( &llCurrentPosition );
                if ( FAILED(hr) )
                {
                    LOG(TEXT("%S: ERROR %d@%S - IMediaSeeking GetCurrentPosition failed! (0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                    goto cleanup;
                }

                Sleep(5);
                // Since filter is in pause state. Current positions should be equal to
                // previous current positions.
                if ( llCurrentPosition != llLastPosition )
                {
                    LOG( TEXT(" Pause State: Current positions should be equal to previous current position.") );
                    hr = E_FAIL;
                    goto cleanup;
                }
                llLastPosition = llCurrentPosition;
            }
            break;
        }
        case 2: // Run
        {
            LOG(TEXT("VerifyFilterState: (FilterState RUN) Calling pMediaSeeking->GetCurPos()"));

            hr = pMediaSeeking->GetCurrentPosition( &llLastPosition );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - IMediaSeeking GetCurrentPosition failed! (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            hr = pMediaSeeking->GetCurrentPosition( &llCurrentPosition );
            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - IMediaSeeking GetCurrentPosition failed! (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
                goto cleanup;
            }

            while ( (count--) && (llCurrentPosition < llDuration) )
            {
                Sleep(100);
                hr = pMediaSeeking->GetCurrentPosition( &llCurrentPosition );
                if ( FAILED(hr) )
                {
                    LOG(TEXT("%S: ERROR %d@%S - IMediaSeeking GetCurrentPosition failed! (0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                    goto cleanup;
                }

                // Since filter is in run state. Current positions should always be greater than
                // previous positions.
                if ( llCurrentPosition < llLastPosition )
                {
                    LOG( TEXT(" Run State: Current positions should always be greater than previous position.") );
                    hr = E_FAIL;
                    goto cleanup;
                }

                llLastPosition = llCurrentPosition;
            }
            break;
        }
        default:
            ASSERT(FALSE);
    }

cleanup:
    return hr;
}

////////////////////////////////////////////////////////////////////////////////
// IMediaControlManualTest
//  Runs MediaControl (Manual) test on media file (AVI, MPeg, Wav etc)
//
// Return value:
//  TPR_PASS if the media file Runs, TPR_FAIL if something goes wrong,
//
// BUGBUG: this is only used in manual test 41005.
HRESULT IMediaControlManualTest( TestGraph *pGraph )
{
    OAFilterState state  = State_Stopped; // Initialize the filter state as Stopped sate.
    int filter_state = 2;
    DWORD dwTmpDur = 0; // Accumulated duration.
    
    HRESULT hr = S_OK;
    PlaybackMedia *pMedia = NULL;

    if ( !pGraph )
    {
        LOG(TEXT("%S: ERROR %d@%S - NULL TestGraph pointer!"),
                __FUNCTION__, __LINE__, __FILE__);
        return E_POINTER;
    }
    // get IMediaControl interface pointer
    IMediaControl *pMediaControl = NULL;
    hr = pGraph->GetMediaControl( &pMediaControl );
    if ( FAILED(hr) )
    {
        LOG( TEXT("Failed to acquire IMediaControl interface. (0x%08x)"), hr );
        goto cleanup;
    }
    // get IMediaSeeking interface pointer
    pMedia = pGraph->GetCurrentMedia();
    if ( !pMedia )
    {
        LOG( TEXT("Failed to get Media object.") );
        hr = E_POINTER;
        goto cleanup;
    }

    // Duration of the media in millisec.
    DWORD dwDuration = (DWORD)(pMedia->duration / 10000);

    pGraph->SetMedia(pMedia);

    // Render the file, which is done inside BuildGraph
    hr = pGraph->BuildGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to render media %s (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        goto cleanup;
    }
    else {
        LOG(TEXT("%S: successfully render %s"), __FUNCTION__, pMedia->GetUrl() );
    }

    // Run the filter graph.
    hr = pMediaControl->Run();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl Run failed. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;
    }
    hr = pMediaControl->GetState( 0, &state );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl GetState failed. (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;
    }

    if ( State_Running != state )
    {
        LOG( TEXT("The state return (%s) is not (%s)"),
            GetStateName( (FILTER_STATE)state ), GetStateName( State_Running ) );
        hr = E_FAIL;
        goto cleanup;
    }

    // Call Run Stop and Pause randomely.
    while (dwTmpDur < dwDuration)
    {
        rand_s(&uNumber);
        int rand_num = uNumber % 3;
        rand_s(&uNumber);
        DWORD sleep_time = uNumber % (dwDuration / 6 + 200);
        // Make sure when we reach near the end of file, adjust the sleep value.
        if (sleep_time > (dwDuration - dwTmpDur))
            sleep_time = dwDuration - dwTmpDur;

        // Make sure the rand generated filter state is not equal to last time generated filter state.
        filter_state = ((rand_num == filter_state) ? (filter_state + 1) % 3 : rand_num);
        switch(filter_state)
        {
            case 0: // Stop
            {
                 // Stop filter graph
                hr = pMediaControl->Stop();
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - IMediaControl Stop failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr);
                    goto cleanup;
                }
                hr = pMediaControl->GetState( 0, &state );
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - IMediaControl GetState failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr);
                    goto cleanup;
                }

                if ( State_Stopped != state )
                {
                    LOG(TEXT("The state return (%s) is not (%s)"),
                        GetStateName( (FILTER_STATE)state ), GetStateName( State_Stopped ) );
                    hr = E_FAIL;
                    goto cleanup;
                }
            }
            break;
            case 1: // Pause
            {
                // Pause filter graph
                hr = pMediaControl->Pause();
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - IMediaControl Pause failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr);
                    goto cleanup;
                }
                hr = pMediaControl->GetState( 0, &state );
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - IMediaControl GetState failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr);
                    goto cleanup;
                }

                if ( State_Paused != state )
                {
                    LOG(TEXT("The state return (%s) is not (%s)"),
                        GetStateName( (FILTER_STATE)state ), GetStateName( State_Paused) );
                    hr = E_FAIL;
                    goto cleanup;
                }
                LOG(TEXT("Media will Pause for %d millsec"), sleep_time/3);
                // Sleep
                Sleep(sleep_time/3); // Millisecond

            }
            break;
            case 2: // Run
            {
                // Run filter graph
                hr = pMediaControl->Run();
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - IMediaControl Run failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr);
                    goto cleanup;
                }
                hr = pMediaControl->GetState( 0, &state );
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - IMediaControl GetState failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr);
                    goto cleanup;
                }

                if ( State_Running != state )
                {
                    LOG(TEXT("The state return (%s) is not (%s)"),
                        GetStateName( (FILTER_STATE)state ), GetStateName( State_Running ) );
                    hr = E_FAIL;
                    goto cleanup;
                }
                LOG(TEXT("Media will Run for %d millsec"), sleep_time);
                // Sleep
                Sleep(sleep_time);
                dwTmpDur += sleep_time;
            }
            break;
        default:
            ASSERT(FALSE);
        }
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
