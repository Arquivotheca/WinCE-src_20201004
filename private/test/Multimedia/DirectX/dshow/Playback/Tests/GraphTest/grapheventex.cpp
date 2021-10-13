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
//  Perform IMediaEventEx interface tests.
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "TestWindow.h"
#include "GraphTest.h"
#include "utility.h"

// Test Message ID.
#define WM_DSHOWMESSAGE     WM_USER + 1


LRESULT CALLBACK TestWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

////////////////////////////////////////////////////////////////////////////////
// GraphMediaEventExTest
// Performs the Media event test for media file.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//
TESTPROCAPI 
IMediaEventExTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IMediaEventEx *pMediaEventEx = NULL;
    HANDLE    hEvent = NULL;

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

    long lEvHandle = 0;
    long lEvCode = 0;
    long lParam1 = 0;
    long lParam2 = 0;
    long lNotifyFlags = 0;

    // Create the notify window
    TestWindow testWindow;

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
    if ( FAILED(hr) || !pMediaEvent )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to acquire IMediaEvent interface. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    
    hr = testGraph.GetMediaEventEx( &pMediaEventEx );
    if ( FAILED(hr) || !pMediaEventEx )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to acquire IMediaEventEx interface. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Set our own window procedure
    testWindow.SetWindProc( TestWindowProc );    

    hr = testWindow.Init();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - creating occluding window failed (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

    testWindow.ShowWindow();

    // before rendering file
    hr = pMediaEventEx->GetNotifyFlags( NULL );
    if ( SUCCEEDED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetNotifyFlags w/ NULL should fail."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // This test calls the function with valid parameter without first rendering a file
    // Flag correspond to turn notification on.
    hr = pMediaEventEx->SetNotifyFlags( 0 ); 
    if ( S_OK == hr )
    {
        LOG( TEXT("%S: WARN %d@%S - SetNotifyFlags w/ 0 should fail."), 
                    __FUNCTION__, __LINE__, __FILE__ );
    }

    // This test call the funtion with a non NULL invalid HWND.
    // BUGBUG: it seems GetEventHandle can't tell the difference.
#if 0
    hr = pMediaEvent->GetEventHandle( &lEvHandle );
    if ( SUCCEEDED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEventHandle should fail. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
#endif

    // This function succeeds even if we pass non NULL invalid handle.
    lEvHandle = (long)INVALID_HANDLE_VALUE;
    hr = pMediaEventEx->SetNotifyWindow( lEvHandle, WM_DSHOWMESSAGE, 0);
    if ( SUCCEEDED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyWindow w/ Non-NULL invalid handle should fail."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // This test calls the function with a valid HWND (without rendering a file).
    hr = pMediaEventEx->SetNotifyWindow((OAHWND)testWindow.GetWinHandle(), WM_DSHOWMESSAGE,0);
    if ( S_OK == hr )
    {
        LOG( TEXT("%S: WARN %d@%S - SetNotifyWindow w/ valid HWND without rendering a file shoudl fail."), 
                    __FUNCTION__, __LINE__, __FILE__ );
    }

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

    // This test randers a file and then calls the function with a NULL
    hr = pMediaEventEx->GetNotifyFlags( NULL );
    if ( SUCCEEDED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetNotifyFlags w/ NULL should fail."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // This test renders a file and calls the function with valid parameter.
    hr = pMediaEventEx->GetNotifyFlags( &lNotifyFlags );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetNotifyFlags failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Fail if the value is other than 0.
    if ( lNotifyFlags != 0 )
    {
        LOG( TEXT("%S: ERROR %d@%S - Event notifications should be enabled by default."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // This test renders a file and calls the function with invalid value.
    hr = pMediaEventEx->SetNotifyFlags( 3 );
    if ( E_INVALIDARG != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyFlags w/ 3 didn't return E_INVALIDARG.(0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // This test renders a file , SetNotifyFlags to 1, and GetEventHandle.
    // AM_MEDIAEVENT_NONOTIFY = 0x1
    hr = pMediaEventEx->SetNotifyFlags( AM_MEDIAEVENT_NONOTIFY );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyFlags w/ AM_MEDIAEVENT_NONOTIFY failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // If event notifications are disabled, the handle returned by the IMediaEvent::GetEventHandle 
    // method is signaled at the end of each stream—that is, whenever the Filter Graph Manager 
    // receives an EC_COMPLETE event.
    hr = pMediaEvent->GetEventHandle(&lEvHandle);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEventHandle should return the manual-reset event that reflects the state of the event queue."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

     //This test renders a file, SetNotifyFlags to 0, GetEventHandle, play and sleep
     //until file is done playing.
    hr = pMediaEventEx->SetNotifyFlags( 0 );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyFlags w/ Zero (enable notify) failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pMediaEvent->GetEventHandle(&lEvHandle);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEventHandle failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Get the duration and rate
    LONGLONG llDuration;
    double rate;
    hr = testGraph.GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get duration. (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = testGraph.GetRate( &rate );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get playback rate. (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to run the graph. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // wait for the completion of the playback.
    hr = testGraph.WaitForCompletion( NearlyInfinite(llDuration, rate), &lEvCode );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - WaitForCompletion failed with event code (0x%08x). (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, lEvCode, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    if ( lEvCode != EC_COMPLETE )
    {
        LOG( TEXT("%S: ERROR %d@%S - The event code returned (0x%08x)is not EC_COMPLETE."), 
                    __FUNCTION__, __LINE__, __FILE__, lEvCode );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to stop the graph. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Rewind the media file.
    hr = testGraph.SetPosition(0);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaSeeking SetPosition w/ 0 failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // This test renders a file, GetEventHandle, SetNotifyFlags to 1, and play, sleep until 
    // the file is done playing.
    hr = pMediaEvent->GetEventHandle(&lEvHandle);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEventHandle failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pMediaEventEx->SetNotifyFlags( AM_MEDIAEVENT_NONOTIFY );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyFlags w/ AM_MEDIAEVENT_NONOTIFY failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to run the graph. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Try Getting  the event. Here the waiting time out period is > duration of the media play. 
    // It should return E_ABORT.
    hr = testGraph.GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to run the graph. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    
    // convert duration to ms
    llDuration /= 10000;
    hr = pMediaEvent->GetEvent(&lEvCode, &lParam1, &lParam2, (LONG)(llDuration + 2000));
    if ( E_ABORT != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - GetEvent didn't return E_ABORT after we set AM_MEDIAEVENT_NONOTIFY. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to stop the graph. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pMediaEventEx->SetNotifyFlags( 0 );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyFlags w/ zero (enable notification) failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Create an Auto Reset event.
    
    hEvent = CreateEvent( NULL,false,false,NULL );
    hr = testGraph.SetPosition(0);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaSeeking SetPosition w/ 0 failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pMediaEventEx->SetNotifyFlags( AM_MEDIAEVENT_NONOTIFY );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyFlags w/ AM_MEDIAEVENT_NONOTIFY failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // The third parameter is event handle which will will be set when wndproc receive the DSHOW messages.
    hr = pMediaEventEx->SetNotifyWindow((OAHWND)testWindow.GetWinHandle(), WM_DSHOWMESSAGE,(long)hEvent );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyWindow failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: INFO %d@%S - SetNotifyWindow with HWND: 0x%08x, Event: 0x%08x"), 
                    __FUNCTION__, __LINE__, __FILE__, (long)testWindow.GetWinHandle(), (long)hEvent );
    }

    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to run the graph. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Pump messages for the window
    DWORD   dwStart = GetTickCount();
    while ( GetTickCount() - dwStart < llDuration + 2000 )
    {
        MSG  msg;
        if ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(100);
    }
 
    hr = WaitForSingleObject( hEvent, 0 );
    if ( WAIT_TIMEOUT != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - WaitForSingleObject with event (0x%08x) didn't timeout after disabling event notification. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hEvent, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to stop the graph. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pMediaEventEx->SetNotifyWindow(NULL, WM_DSHOWMESSAGE,0);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyWindow w/ NULL failed (stop receiving messages). (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.SetPosition(0);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaSeeking SetPosition w/ 0 failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pMediaEventEx->SetNotifyFlags(0);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyFlags w/ 0 (enable notification) failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // The third parameter is event handle which will will be set when wndproc receive the DSHOW messages.
    hr = pMediaEventEx->SetNotifyWindow((OAHWND)testWindow.GetWinHandle(), WM_DSHOWMESSAGE,(long)hEvent);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyWindow failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: INFO %d@%S - SetNotifyWindow w/ HWND: 0x%08x, Event: 0x%08x"), 
                    __FUNCTION__, __LINE__, __FILE__, (long)testWindow.GetWinHandle(), (long)hEvent );
    }

    // Get the duration and rate
    hr = testGraph.GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get duration. (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = testGraph.GetRate( &rate );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get playback rate. (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to run the graph. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Pump messages for the window
    dwStart = GetTickCount();
    while ( GetTickCount() - dwStart < (DWORD)NearlyInfinite(llDuration, rate) )
    {
        MSG  msg;
        if ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(100);
    }

    hr = WaitForSingleObject( hEvent, 0 );
    if ( WAIT_OBJECT_0 != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - WaitForSingleObject with event (0x%08x) failed after enabling notification."), 
                    __FUNCTION__, __LINE__, __FILE__, (long)hEvent );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to stop the graph. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pMediaEventEx->SetNotifyWindow(NULL, WM_DSHOWMESSAGE,0);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetNotifyWindow w/ NULL (stop receiving messages) failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

cleanup:
    testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if (hEvent)
        CloseHandle( hEvent );
    if ( pMediaEventEx )
        pMediaEventEx->SetNotifyFlags(0);

    // Reset the testGraph and test desc
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    
    pTestDesc->Reset();
    testWindow.Reset();

    return retval;
}


////////////////////////////////////////////////////////////////////////////////
// TestWindowProc
// Window message handling routine.
//
// Return value:
//  The return value is the result of the message processing and depends on the message
LRESULT CALLBACK TestWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch ( uMsg )
    {
        case WM_DSHOWMESSAGE: // Dshow events/Messages
            // Fire an event.
            SetEvent( (HANDLE)lParam );
            break;
        default:
            return DefWindowProc( hwnd, uMsg, wParam, (LPARAM)lParam );
    }
    return 0;
}
