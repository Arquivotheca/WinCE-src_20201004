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
//  Perform IReferenceClock interface tests.
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


DWORD ADVISE_PAST_TIME_LATENCY_TOLERANCE_DEFAULT    = 100;
DWORD ADVISE_TIME_LATENCY_TOLERANCE_DEFAULT            = 200;


TESTPROCAPI
IReferenceClockTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    if ( SPR_HANDLED == HandleTuxMessages( uMsg, tpParam ) )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc *)lpFTE->dwUserData;
    if(!pTestDesc)
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to retrieve test config object" ), __FUNCTION__, __LINE__, __FILE__ );
        return TPR_FAIL;
    }
    // From the test desc, determine the media file to be used
    PlaybackMedia *pMedia = pTestDesc->GetMedia( 0 );
    if ( !pMedia )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to retrieve media object" ), __FUNCTION__, __LINE__, __FILE__ );
        return TPR_FAIL;
    }

    IMediaFilter    *pIMediaFilter= NULL;
    IReferenceClock *pIReferenceClock = NULL;
    IGraphBuilder   *pIGraphBuilder = NULL;

    // Instantiate the test graph
    TestGraph    testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);

    // Build the graph
    hr = testGraph.BuildGraph();
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to build graph for media %s (0x%x)" ), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT( "%S: successfully built the graph for %s" ), __FUNCTION__, pMedia->GetUrl() );
    }

    // get the duration
    LONGLONG duration = 0;
    LONGLONG actualDuration = 0;
    LONGLONG expectedDuration = 0;

    hr = testGraph.GetDuration( &duration );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to get the duration (0x%x)" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // ??
    // in order for IMediaFilter to get IReferenceClock interface. The graph has to be run first.
    //
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS);
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to set graph state." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }
    // Then stop the graph
    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS);
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to set graph state." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    pIGraphBuilder = testGraph.GetGraph();
    if ( !pIGraphBuilder )
    {
        LOG( TEXT( "%S: ERROR %d@%S - NULL IGraphBuilder pointer returned" ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pIGraphBuilder->QueryInterface( IID_IMediaFilter, (void **)&pIMediaFilter );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - QueryInterface for IID_IMediaFilter failed! 0x%08x" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Get IMediaFilter interface pointer
    hr = pIMediaFilter->GetSyncSource( &pIReferenceClock );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - Unable to retrieve IReferenceClock interface! 0x%08x" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    if ( NULL == pIReferenceClock )
    {
        LOG( TEXT( "%S: ERROR %d@%S - NULL IReferenceClock returned!" ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Test ADvisePeriodic() with NULL parameters
    HANDLE hSemaphore;
    // create semaphore with count 5
    hSemaphore = CreateSemaphore( NULL, 0, 5, NULL );
    hr = pIReferenceClock->AdvisePeriodic( 0, 0, (HSEMAPHORE)hSemaphore, NULL );
    CloseHandle( hSemaphore );
    if ( E_POINTER != hr )
    {
        LOG( TEXT( "%S: ERROR %d@%S - AdvisePeriodic w/ NULL should return E_POINTER." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pIReferenceClock->AdvisePeriodic( 0, 0, NULL, NULL );
    if ( E_POINTER != hr )
    {
        LOG( TEXT( "%S: ERROR %d@%S - AdvisePeriodic w/ NULL should return E_POINTER." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Test AdviseTime() with Event
    HANDLE hEvent;
    hEvent = CreateEvent( NULL, false, false, NULL ); // Auto Reset Event
    hr = pIReferenceClock->AdviseTime( 0, 0, (HEVENT)hEvent, NULL );
    CloseHandle( hEvent );
    if ( E_POINTER != hr )
    {
        LOG( TEXT( "%S: ERROR %d@%S - AdviseTime w/ NULL should return E_POINTER." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pIReferenceClock->AdviseTime( 0, 0, NULL, NULL );
    if ( E_POINTER != hr )
    {
        LOG( TEXT( "%S: ERROR %d@%S - AdviseTime w/ NULL should return E_POINTER." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Test AdviseTime() with Mutex (sync object other than event)
    DWORD dwAdvise = 0;
    REFERENCE_TIME rtBaseTime = 0;    // LONGLONG type

    // Pass NULL to GetTime() should fail.
    hr = pIReferenceClock->GetTime( NULL );
    if ( E_POINTER != hr )
    {
        LOG( TEXT( "%S: ERROR %d@%S - IReferenceClock GetTime() w/ NULL should return E_POINTER. (0x%08x)" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // call Unadvise() without calling AdviseTime() first
    hr = pIReferenceClock->Unadvise( dwAdvise );
    if ( hr == S_OK )
    {
        LOG( TEXT( "%S: ERROR %d@%S - Unadvise without calling AdviseTime should fail." ), __FUNCTION__, __LINE__, __FILE__ );
    }

    // call Unadvise() extra time should fail.
    hEvent = CreateEvent( NULL, false, false, NULL ); // Auto reset event
    hr = pIReferenceClock->GetTime( &rtBaseTime );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S: IReferenceClock GetTime failed. (0x%08x)" ),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pIReferenceClock->AdviseTime( rtBaseTime, 0, (HEVENT)hEvent, &dwAdvise );
    CloseHandle( hEvent );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S: AdviseTime w/ valid parameters failed. (0x%08x)" ),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pIReferenceClock->Unadvise( dwAdvise );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S: IReferenceClock::Unadvise failed. (0x%08x)" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    // extra Unadvise should fail
    //hr returning S_FALSE is expected and not a fail case.
    hr = pIReferenceClock->Unadvise( dwAdvise );
    if ( S_OK == hr )
    {
        LOG( TEXT( "%S: ERROR %d@%S - Extra IReferenceClock::Unadvise call should fail." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // call AdviseTime() with some future advise time and then call Unadvise()
    // WaitForMultipleObjects on the event should not get any event.
    hEvent = CreateEvent( NULL, false, false, NULL ); // auto reset event
    hr = pIReferenceClock->GetTime( &rtBaseTime );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - GetTime failed. (0x%08x)" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pIReferenceClock->AdviseTime( rtBaseTime, MSEC_TO_TICKS( 100 ), (HEVENT)hEvent, &dwAdvise );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - AdviseTime failed. (0x%08x)" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pIReferenceClock->Unadvise( dwAdvise );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - Unadvise after calling AdviseTime failed. (0x%08x)" ),
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( WAIT_TIMEOUT != WaitForMultipleObjects( 1, (const HANDLE *)&hEvent, FALSE, 250 ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - WaitForMultipleObject failed." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }
    CloseHandle( hEvent );

cleanup:

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the testGraph
    testGraph.Reset();
    pTestDesc->Reset();

    // Release interfaces not used any more
    if ( pIMediaFilter )
        pIMediaFilter->Release();
    if ( pIGraphBuilder )
        pIGraphBuilder->Release();
    if ( pIReferenceClock )
        pIReferenceClock->Release();

    return retval;
}

TESTPROCAPI
IReferenceClock_GetTime_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    UINT uNumber = 0;
    bool bGetTimeTestFailed = false;
    if ( SPR_HANDLED == HandleTuxMessages( uMsg, tpParam ) )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc *)lpFTE->dwUserData;
    if(!pTestDesc)
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to retrieve test config object" ), __FUNCTION__, __LINE__, __FILE__ );
        return TPR_FAIL;
    }

    // From the test desc, determine the media file to be used
    PlaybackMedia *pMedia = pTestDesc->GetMedia( 0 );
    if ( !pMedia )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to retrieve media object" ), __FUNCTION__, __LINE__, __FILE__ );
        return TPR_FAIL;
    }

    IMediaFilter    *pIMediaFilter= NULL;
    IReferenceClock *pIReferenceClock = NULL;
    IGraphBuilder   *pIGraphBuilder = NULL;

    // Instantiate the test graph
    TestGraph    testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);

    // Build the graph
    hr = testGraph.BuildGraph();
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to build graph for media %s (0x%x)" ), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        retval = TPR_FAIL; 
        goto cleanup;
    }
    else
    {
        LOG( TEXT( "%S: successfully built the graph for %s" ), __FUNCTION__, pMedia->GetUrl() );
    }

    // ??
    // in order for IMediaFilter to get IReferenceClock interface. The graph has to be run first.
    //
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS);
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to set graph to Running state." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }
    // Then stop the graph
    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS);
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to set graph to Stopped state." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Set up the test
    
    rand_s(&uNumber);
    int count = (unsigned int)((double)uNumber/((double)UINT_MAX+1)*20 + 20); // random loop count.
    REFERENCE_TIME rtCurrentTime, rtPrevCurrentTime, rtElapsedTime;
    DWORD dwStartTime, dwEndTime, dwElapsedTime, dwDriftTolerance;
    REFERENCE_TIME rtDuration;

    pIGraphBuilder = testGraph.GetGraph();
    if ( !pIGraphBuilder )
    {
        LOG( TEXT( "%S: ERROR %d@%S - NULL IGraphBuilder pointer returned." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pIGraphBuilder->QueryInterface( IID_IMediaFilter, (void **)&pIMediaFilter );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - QueryInterface for IID_IMediaFilter failed! 0x%08x" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Get IMediaFilter interface pointer
    hr = pIMediaFilter->GetSyncSource( &pIReferenceClock );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - Unable to retrieve IReferenceClock interface! 0x%08x" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    if ( NULL == pIReferenceClock )
    {
        LOG( TEXT( "%S: ERROR %d@%S - NULL IReferenceClock returned!" ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.GetDuration( &rtDuration );
    if ( FAILED(hr) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to get duration for media %s!" ), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl() );
        retval = TPR_FAIL;
        goto cleanup;
    }


    dwDriftTolerance = (DWORD)( TICKS_TO_MSEC(rtDuration) / 4.0 );  // drift tolerance: 1/3 of duration in ms

    // set filter state
    hr = testGraph.SetState( pTestDesc->state, TestGraph::SYNCHRONOUS);
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to set graph state to %s." ),
                    __FUNCTION__, __LINE__, __FILE__, GetStateName( pTestDesc->state) );
        retval = TPR_FAIL;
        goto cleanup;
    }

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST );
    hr = pIReferenceClock->GetTime( &rtPrevCurrentTime );
    if ( FAILED(hr) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to get the current time." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    dwStartTime = GetTickCount();

    while ( count-- )
    {
        // get current time
        hr = pIReferenceClock->GetTime( &rtCurrentTime );
        dwEndTime = GetTickCount();

        // The current time should always be greater than previous current time
        if ( rtCurrentTime < rtPrevCurrentTime )
        {
            LOG( TEXT( "%S: ERROR %d@%S - Current time is less than the previous current time!" ), __FUNCTION__, __LINE__, __FILE__ );
            continue;
        }
        // compare the clock values with respect to the values returned by GetTickCount
        dwElapsedTime = dwEndTime - dwStartTime;
        rtElapsedTime = rtCurrentTime - rtPrevCurrentTime;

        if ( !ToleranceCheckAbs( dwElapsedTime, TICKS_TO_MSEC( rtElapsedTime ), dwDriftTolerance ) )
        {
            LOG( TEXT( "%S: ERROR %d@%S - Too much difference between GetTime and GetTickCount!" ), __FUNCTION__, __LINE__, __FILE__ );
            LOG( TEXT( "dwStartTime: %d ms    dwEndTime: %d ms, rtPrevCurrentTime: %d ms    rtCurrentTime: %d ms" ), dwStartTime, dwEndTime,
                    TICKS_TO_MSEC(rtPrevCurrentTime) ,  TICKS_TO_MSEC(rtCurrentTime ) );
            LOG( TEXT( "GetTime: %d ms    GetTickCount: %d ms" ), dwElapsedTime, TICKS_TO_MSEC( rtElapsedTime ) );

            hr = pIReferenceClock->GetTime( &rtCurrentTime );
            if ( FAILED(hr) )
            {
                LOG( TEXT( "%S: ERROR %d@%S - GetTime failed!" ), __FUNCTION__, __LINE__, __FILE__ );
                retval = TPR_FAIL;
                goto cleanup;
            }
            dwEndTime = GetTickCount();
            bGetTimeTestFailed = true;
        }

        // random sleep
        rand_s(&uNumber);
        Sleep( uNumber%25 );
        // Mark the current value as previous value.
        rtPrevCurrentTime = rtCurrentTime;
        dwStartTime = dwEndTime;
    }

cleanup:
    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );
    
    // Reset the testGraph
    testGraph.Reset();

    if ( bGetTimeTestFailed )
        retval = TPR_FAIL;

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Release interfaces not used any more
    if ( pIMediaFilter )
        pIMediaFilter->Release();
    if ( pIGraphBuilder )
        pIGraphBuilder->Release();
    if ( pIReferenceClock )
        pIReferenceClock->Release();

    return retval;
}


TESTPROCAPI
IReferenceClock_AdviseTime_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    UINT uNumber = 0;
    if ( SPR_HANDLED == HandleTuxMessages( uMsg, tpParam ) )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc *)lpFTE->dwUserData;
    if(!pTestDesc)
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to retrieve test config object" ), __FUNCTION__, __LINE__, __FILE__ );
        return TPR_FAIL;
    }

    // From the test desc, determine the media file to be used
    PlaybackMedia *pMedia = pTestDesc->GetMedia( 0 );
    if ( !pMedia )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to retrieve media object" ), __FUNCTION__, __LINE__, __FILE__ );
        return TPR_FAIL;
    }

    IMediaFilter    *pIMediaFilter= NULL;
    IReferenceClock *pIReferenceClock = NULL;
    IGraphBuilder   *pIGraphBuilder = NULL;

    // Instantiate the test graph
    TestGraph    testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);

    // Build the graph
    hr = testGraph.BuildGraph();
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to build graph for media %s (0x%x)" ), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT( "%S: successfully built the graph for %s" ), __FUNCTION__, pMedia->GetUrl() );
    }

    // ??
    // in order for IMediaFilter to get IReferenceClock interface. The graph has to be run first.
    //
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS);
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to set graph state." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }
    // Then stop the graph
    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS);
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to set graph state." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Set up the test
    bool bAdviseTimeTestFailed = false;
    bool bPastTime = false;        // whether to perform test with old ("past") time.
    rand_s(&uNumber);
    int count = (unsigned int)((double)uNumber/((double)UINT_MAX+1)*10 + 10);
    DWORD dwAdvise;
    REFERENCE_TIME rtBaseTime = 0, rtBaseOffsetTime = 0, rtOffsetTime = 0;
    DWORD dwStartTime = 0, dwTotalTime = 0;

    pIGraphBuilder = testGraph.GetGraph();
    if ( !pIGraphBuilder )
    {
        LOG( TEXT( "%S: ERROR %d@%S - NULL IGraphBuilder pointer returned." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pIGraphBuilder->QueryInterface( IID_IMediaFilter, (void **)&pIMediaFilter );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - QueryInterface for IID_IMediaFilter failed! 0x%08x" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Get IMediaFilter interface pointer
    hr = pIMediaFilter->GetSyncSource( &pIReferenceClock );
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - Unable to retrieve IReferenceClock interface! 0x%08x" ), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    if ( NULL == pIReferenceClock )
    {
        LOG( TEXT( "%S: ERROR %d@%S - NULL IReferenceClock returned!" ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.SetState( pTestDesc->state, TestGraph::SYNCHRONOUS);
    if ( FAILED( hr ) )
    {
        LOG( TEXT( "%S: ERROR %d@%S - failed to set graph state." ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // wait for filter state transition to finish.
    Sleep( 500 );

    // Create auto reset event
    HANDLE hEvent = CreateEvent( NULL, false, false, NULL );
    if ( NULL == hEvent )
    {
        LOG( TEXT( "%S: ERROR %d@%S - Create an auto reset event failed!" ), __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST );

    while ( count-- )
    {
        rand_s(&uNumber);
        // randomly calculate offset time.  This will generate +ve and -ve offset values.
        rtOffsetTime = MSEC_TO_TICKS((double)uNumber/((double)UINT_MAX+1)*250 - 100 );
        // Get the current base time
        hr = pIReferenceClock->GetTime( &rtBaseTime );
        
        if ( FAILED( hr ) )
        {
            LOG( TEXT( "%S: ERROR %d@%S - GetTime failed!" ), __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            goto cleanup;
        }
        rand_s(&uNumber);
        if ((unsigned int)(double)uNumber/((double)UINT_MAX+1)*2 )
        {
            rand_s(&uNumber);
            rtBaseOffsetTime = -MSEC_TO_TICKS( (unsigned int)(double)uNumber/((double)UINT_MAX+1.0)*100 + 150 );
            bPastTime = true;
        }
        else
        {
            rtBaseOffsetTime = 0;
            bPastTime = false;
        }

        // special case
        if ( 0 > rtBaseTime + rtBaseOffsetTime + rtOffsetTime )
        {
            // For all negative values, AdviseTime should fail.
            hr = pIReferenceClock->AdviseTime( rtBaseTime + rtBaseOffsetTime,
                                               rtOffsetTime,
                                               (HEVENT)hEvent,
                                               &dwAdvise );
            if ( E_INVALIDARG != hr )
            {
                LOG( TEXT( "%S: ERROR %d@%S - AdviseTime should fail, not succeed."), __FUNCTION__, __LINE__, __FILE__ );
                retval = TPR_FAIL;
                goto cleanup;
            }
            continue;
        }

        // call AdviseTime
        hr = pIReferenceClock->AdviseTime( rtBaseTime + rtBaseOffsetTime,
                                            rtOffsetTime,
                                            (HEVENT)hEvent,
                                            &dwAdvise );
        if ( FAILED(hr) )
        {
            LOG( TEXT( "%S: ERROR %d@%S - AdviseTime failed." ), __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            goto cleanup;
        }

        // wait for event
        dwStartTime = GetTickCount();
        hr = WaitForMultipleObjects( 1, (const HANDLE *)&hEvent, FALSE, 10000 );
        if ( WAIT_FAILED == hr )
        {
            LOG( TEXT( "%S: ERROR %d@%S - WaitForMultipleObjects failed!" ), __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            goto cleanup;
        }
        dwTotalTime = GetTickCount() - dwStartTime;

        // Unadvise on the cookie
        hr = pIReferenceClock->Unadvise( dwAdvise );
        if ( FAILED(hr) )
        {
            LOG( TEXT( "%S: ERROR %d@%S - Unadvise the cookie failed." ), __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            goto cleanup;
        }

        LOG( TEXT( "Base: %I64d    BaseOffset: %I64d    Offset: %I64d    WaitTime: %d" ), rtBaseTime, rtBaseOffsetTime, rtOffsetTime, dwTotalTime );

        if ( bPastTime )
        {
            if ( ADVISE_PAST_TIME_LATENCY_TOLERANCE_DEFAULT < dwTotalTime )
            {
                LOG( TEXT( "%S: ERROR %d@%S AdviseTime latency exceeds the tolerance." ), __FUNCTION__, __LINE__, __FILE__ );
                retval = TPR_FAIL;
                goto cleanup;
            }
        }
        else
        {
            // The base time is present time.
            if ( 0 < rtOffsetTime )
            {
                if ( !ToleranceCheckAbs( dwTotalTime, TICKS_TO_MSEC( rtOffsetTime ), ADVISE_TIME_LATENCY_TOLERANCE_DEFAULT ) )
                {
                    LOG( TEXT( "IReferenceClock::AdviseTime: Total Tick Count = %d, Advise offset time = %d, Difference = %d, Tolerance = %d"),
                            dwTotalTime, TICKS_TO_MSEC( rtOffsetTime ), dwTotalTime - TICKS_TO_MSEC( rtOffsetTime ), ADVISE_TIME_LATENCY_TOLERANCE_DEFAULT );
                    bAdviseTimeTestFailed = true;
                }
            }
            else
            {
                // when offset value is negative, AdviseTime should return immediately
                if ( ADVISE_PAST_TIME_LATENCY_TOLERANCE_DEFAULT < dwTotalTime )
                {
                }
            }
        }
    } // end while( count-- )

    if ( hEvent )
        CloseHandle( hEvent );

cleanup:

    SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the testGraph
    testGraph.Reset();
    pTestDesc->Reset();

    // Release interfaces not used any more
    if ( pIMediaFilter )
        pIMediaFilter->Release();
    if ( pIGraphBuilder )
        pIGraphBuilder->Release();
    if ( pIReferenceClock )
        pIReferenceClock->Release();

    return retval;
}
