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
//  Video Mixing Renderer Rotation Scaling  Test:
//  1. Single stream rotation scaling test
//  2. Mixing mode rotation scaling test
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <atlbase.h>
#include <tux.h>
#include "StringConversions.h"
#include "logging.h"
#include "utility.h"
#include "TuxGraphTest.h"
#include "BaseCustomAP.h"
#include "RotationScalingTest.h"

////////////////////////////////////////////////////////////////////////////////
// Rotation_Scaling_Manual_Test :
//    Plays single or mixing streams in secure chamber and visually inspects the
//  image on the screen.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI
Rotation_Scaling_Manual_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IVMRFilterConfig    *pVMRConfig = NULL;
    IVMRMixerControl    *pVMRMixerControl = NULL;
    IVMRMixerControl2    *pVMRMixerControl2 = NULL;
    IVMRWindowlessControl *pVMRWindowlessControl = NULL;
    HWND    hwndApp = NULL;
    TCHAR tmp[128] = {0};
    RotateConfigList rotateConfigList;
    WndScaleList scaleConfigList;
    VMRStreamData   streamData;
    bool bRotateWindows = false;
    DWORD dwRotation = 0;
    BOOL bMixingMode = false;
    LONG lWidth, lHeight;

    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    RotationScalingTestDesc* pTestDesc = (RotationScalingTestDesc*)lpFTE->dwUserData;

    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    if ( pTestDesc->MixingMode() )
        testGraph.UseVMR( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"),
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Add the VMR manually
    WCHAR   wszFilterName[MAX_PATH] = L"VideoMixingRenderer";
    hr = testGraph.AddFilterByCLSID( CLSID_VideoMixingRenderer, wszFilterName );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ),
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Retrieve IVMRFilterConfig interface
    hr = testGraph.FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface using remote graph builder") );

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    if ( dwStreams > 1 )
    {
        hr = pVMRConfig->SetNumberOfStreams( dwStreams );
        StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
        CHECKHR( hr, tmp );
        bMixingMode = true;
    }

    // Set windowless mode
    hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
    CHECKHR( hr, TEXT("SetRenderingMode as windowless ..."));

    PlaybackMedia *pMedia = NULL;
    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        pMedia = pTestDesc->GetMedia( i );
        if ( !pMedia )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the media object"),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            goto cleanup;
        }

        testGraph.SetMedia( pMedia );
        hr = testGraph.BuildGraph();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Rendering media file: %s failed. (0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, pTestDesc->GetMedia(i)->GetUrl(), hr );
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    // Retrieve IVMRWindowlessControl interface
    hr = testGraph.FindInterfaceOnGraph( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
    CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface using remote graph builder") );

    //Mixing Mode
    if ( dwStreams > 1 )
    {
        // Retrieve IVMRMixerControl interface
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface using remote graph builder") );

        if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
        {
            LOG( TEXT("Set stream parameters failed.") );
            retval = TPR_FAIL;
            goto cleanup;
        }

        // Retrieve IVMRMixerControl2 interface
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl2, (void **)&pVMRMixerControl2 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("Query IVMRMixerControl2 interface using remote graph builder failed! Use IVMRWindowlessControl instead") );
            hr = pVMRWindowlessControl->GetNativeVideoSize( &lWidth, &lHeight, NULL, NULL );
            LOG( TEXT("GetNativeVideoSize returned (%d, %d)"), lWidth, lHeight);
        }
        else
        {
            //always put the video with max native size on pin 0
            hr = pVMRMixerControl2->GetStreamNativeVideoSize(0, &lWidth, &lHeight);
            LOG( TEXT("GetStreamNativeVideoSize returned stream %d: (%d, %d)"), 0, lWidth, lHeight );
        }
    }else{
        //not in mixing mode
        hr = pVMRWindowlessControl->GetNativeVideoSize( &lWidth, &lHeight, NULL, NULL );
        LOG( TEXT("GetNativeVideoSize returned (%d, %d)"), lWidth, lHeight);
    }

    RECT rcVideoWindow;
    hwndApp = OpenAppWindow( 0, 0, lWidth, lHeight, &rcVideoWindow, pVMRWindowlessControl );

    if ( !hwndApp )
    {
        hr = E_FAIL;
        CHECKHR( hr, TEXT("Create an application window...") );
    }

    hr = pVMRWindowlessControl->SetVideoClippingWindow( hwndApp );
    CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

    hr = pVMRWindowlessControl->SetVideoPosition( NULL, &rcVideoWindow );
    CHECKHR( hr, TEXT("SetVideoPosition ..."));

    // Get the max duration of the input vide streams
    // BUGBUG: it seems it just returns the duration for the last rendered media file.
    LONGLONG llMaxDuration = 0;
    hr = testGraph.GetDuration( &llMaxDuration );
    CHECKHR( hr, TEXT("GetDuration ...") );

    // Run the clip
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, TEXT("Run the graph ...") );

    // set rotation and scaling parameters
    scaleConfigList = pTestDesc->dynamicScaleList;
    rotateConfigList = pTestDesc->dynamicRotateConfigList;

    hr = VMRRotate(&testGraph, rotateConfigList[0], &bRotateWindows, &dwRotation);
    if ( FAILED(hr) )
    {
        LOG(TEXT("%S: WARN %d@%S - The graphic mode is not supported. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
    }
    else
        CHECKHR(hr,TEXT("Dynamic Rotate video ..." ));

    hr = VMRScale(&testGraph, (IBaseFilter *)pVMRConfig, scaleConfigList[0], bRotateWindows, bMixingMode);
    CHECKHR(hr,TEXT("Dynamic Scale video ..." ));

    // center the window after it's rotated and scaled.
    hr = VMRUpdateWindow( (IBaseFilter *)pVMRConfig, hwndApp, bRotateWindows );
    CHECKHR(hr,TEXT("Stopped VMR Update window ..." ));

    // For windowless display mode, we need to provide a message pump
    DWORD   dwStart = GetTickCount();
    DWORD   dwStop = min( 5000, TICKS_TO_MSEC(llMaxDuration) );
    while ( GetTickCount() - dwStart < dwStop )
    {
        MSG  msg;
        while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(5);
    }

cleanup:
    //Any failure from test running should fail the case
    if ( FAILED(hr) )
        retval = TPR_FAIL;

    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
        retval = TPR_FAIL;

    hr = RotateUI (AM_ROTATION_ANGLE_0);
    if ( FAILED(hr) ) {
        LOG(TEXT("%S: ERROR %d@%S - failed to Rotate UI back. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if ( pVMRMixerControl )
        pVMRMixerControl->Release();

    if ( pVMRWindowlessControl )
        pVMRWindowlessControl->Release();

    if ( pVMRMixerControl2 )
        pVMRMixerControl2->Release();

    if ( pVMRConfig )
        pVMRConfig->Release();

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    // Ask the tester if playback was ok
    int query = MessageBox( NULL,
                            TEXT("Did the test complete succesfully with right rotation and scale?"),
                            TEXT("Manual Playback Test"),
                            MB_YESNO | MB_SETFOREGROUND
                            );
    if ( query == IDYES )
        LOG( TEXT("%S: playback completed successfully."), __FUNCTION__ );
    else if ( query == IDNO )
    {
        LOG( TEXT("%S: playback was not ok."), __FUNCTION__ );
        retval = TPR_FAIL;
    }
    else
    {
        LOG( TEXT("%S: unknown response or MessageBox error."), __FUNCTION__ );
        retval = TPR_ABORT;
    }

    return retval;
}

////////////////////////////////////////////////////////////////////////////////
// Rotation_Scaling_Test_VMR :
//    Plays back multiple video streams using one single VMR in renderless mode
// using a custom allocator presenter.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI
Rotation_Scaling_Test_VMR( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IMediaEvent *pEvent = 0x0;
    IVMRFilterConfig    *pVMRConfig = NULL;
    IVMRMixerControl    *pVMRMixerControl = NULL;
    IVMRMixerControl2    *pVMRMixerControl2 = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;
    HWND    hwndApp = NULL;
    TCHAR tmp[128] = {0};
    RotateConfigList rotateConfigList;
    WndScaleList scaleConfigList;
    CVMRRSCustomAP   *pCustomAP = NULL;
    VMRStreamData   streamData;
    DWORD dwPassCount = 0;
    UINT uSize = 0, uRSize = 0, uSSize = 0, uMax = 0;
    int iScale = 0, iRotate = 0;
    bool bRotateWindows = false;
    bool bUpstreamRotateEnabled = false;
    bool bUpstreamScaleEnabled = false;
    DWORD dwRotation = 0;
    DWORD dwOrigUpstreamRotationPreferred = 0xffffffff;
    DWORD dwOrigUpstreamScalingPreferred = 0xffffffff;
    DWORD dwPrefs;
    DWORD dwSleepTime = 0;
    LONG lWidth, lHeight;

    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    RotationScalingTestDesc* pTestDesc = (RotationScalingTestDesc*)lpFTE->dwUserData;

    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    if ( pTestDesc->MixingMode() )
        testGraph.UseVMR( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"),
            __FUNCTION__, __LINE__, __FILE__, hr );
    retval = TPR_FAIL;
    goto cleanup;
    }

    //initialize custom AP
    pCustomAP = new CVMRRSCustomAP( &hr );
    if ( !pCustomAP )
       hr = E_OUTOFMEMORY;

    CHECKHR( hr, TEXT("Creating the custom allocator presenter ...") );

    pCustomAP->AddRef();

    //Disable WMV decoder frame dropping if we want to save all frames at PresentImage in CustomAP
    hr = DisableWMVDecoderFrameDrop();
    CHECKHR( hr, TEXT("DisableWMVDecoderFrameDrop ...") );

      //Enable upstream rotate
    if(pTestDesc->bPrefUpstreamRotate)
    {
        hr = EnableUpstreamRotate(pTestDesc->bPrefUpstreamRotate, &dwOrigUpstreamRotationPreferred);
        if(FAILED(hr))
        {
            //if failed and error is not registry not existing
            if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream rotate (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }
            else
            {
                //that key doesn't exist
                LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream rotate: the registry not exist (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
            }
        }
        else
        {
                LOG( TEXT("Enable upstream rotate: Succeeded!!"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                bUpstreamRotateEnabled = true;
        }
    }

      //Enable upstream scale
    if(pTestDesc->bPrefUpstreamScale)
    {
        hr = EnableUpstreamScale(pTestDesc->bPrefUpstreamScale, &dwOrigUpstreamScalingPreferred);
        if(FAILED(hr))
        {
            //if failed and error is not registry not existing
            if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream scale (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }
            else
            {
                //that key doesn't exist
                LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream scale: the registry not exist (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
            }
        }
        else
        {
            LOG( TEXT("Enable upstream scale: Succeeded!!"),
                __FUNCTION__, __LINE__, __FILE__, hr );
            bUpstreamScaleEnabled = true;
        }
    }

    //Turn on image save mode on custom AP
    if (pTestDesc->m_vmrTestMode == IMG_SAVE_MODE ||pTestDesc->m_vmrTestMode == IMG_VERIFICATION_MODE)
    {
        pCustomAP->SaveImage( TRUE );
        //set where to save images
        pCustomAP->SetImagePath( pTestDesc->szDestImgPath );
    }
    else if (pTestDesc->m_vmrTestMode == IMG_STM_VERIFICATION_MODE )
    {
        //turn on verification thread
        pCustomAP->VerifyStream( TRUE );
        //set where to find source images
        pCustomAP->SetImagePath( pTestDesc->szSrcImgPath );
    }
    else if (pTestDesc->m_vmrTestMode == IMG_CAPTURE_MODE )
    {
        //turn on verification thread
        pCustomAP->CaptureImage( TRUE );
        //set where to find source images
        pCustomAP->SetImagePath( pTestDesc->szSrcImgPath );
        pCustomAP->SetWndHandle(hwndApp);
    }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the first video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilter( VideoMixingRenderer );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ),
            __FUNCTION__, __LINE__, __FILE__, hr );
    retval = TPR_FAIL;
    goto cleanup;
    }

    pVMR = testGraph.GetFilter( VideoMixingRenderer );

    if ( !pVMR )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ),
            __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pVMR->QueryInterface( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface") );

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    if ( pTestDesc->MixingMode() )
    {
        hr = pVMRConfig->SetNumberOfStreams( dwStreams );
        StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
        CHECKHR( hr, tmp );
    }

    streamData.vmrId = DW_COMPOSITED_ID;
    // The rendering mode will be set to renderless inside
    hr = pCustomAP->Activate(pVMR, NULL, &streamData );
    CHECKHR( hr, TEXT("Activate the custom allocator presenter.") );

   //Set render pref surface
    hr = pVMRConfig->GetRenderingPrefs(&dwPrefs);
    StringCchPrintf( tmp, 128, TEXT("GetRenderingPrefs ..."));
    CHECKHR( hr, tmp );

    dwPrefs &= ~(RenderPrefs_ForceOverlays | RenderPrefs_ForceOffscreen);

    if (pTestDesc->m_dwRenderingPrefsSurface & RenderPrefs_ForceOverlays)
    {
        dwPrefs |= RenderPrefs_ForceOverlays;
        hr = pVMRConfig->SetRenderingPrefs(dwPrefs);
        StringCchPrintf( tmp, 128, TEXT("SetRenderingPrefs ..."));
        CHECKHR(hr, tmp);
    }
    else if (pTestDesc->m_dwRenderingPrefsSurface & RenderPrefs_ForceOffscreen)
    {
        dwPrefs |= RenderPrefs_ForceOffscreen;
        hr = pVMRConfig->SetRenderingPrefs(dwPrefs);
        StringCchPrintf( tmp, 128, TEXT("SetRenderingPrefs ..."));
        CHECKHR(hr, tmp);
    }

    // Set windowless parameters
    hr = pVMR->QueryInterface( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
    CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

    hr = pVMRWindowlessControl->SetBorderColor( RGB(0,0,0) );
    CHECKHR( hr, TEXT("SetBorderColor as black...") );

    // Connect to VMR
    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        LOG( TEXT("Rendering the file ..."));
        testGraph.SetMedia(pTestDesc->GetMedia(i));
        if(FAILED(hr = testGraph.BuildGraph()))
        {
            LOG( TEXT("%S: ERROR %d@%S - error code: 0x%08x "),
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;

            // If we are just trying to save the filter graph with the '/GRF' command (instead of running the test for real)
            // then BuildGraph() will fail to try and short-circuit the test. In this case,
            // we want to ALWAYS ensure that we've tried to render every stream.
            continue;
        }
        else
            LOG( TEXT("Succeeded.") );
    }

    // At some point trying to render the streams failed, so bail out.
    if(retval == TPR_FAIL)
        goto cleanup;

    if ( pTestDesc->MixingMode() )
    {
        // Set alpha, z-order and output rectangles
        hr = pVMR->QueryInterface( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        // Set stream parameters
        if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
        {
            LOG( TEXT("Set stream parameters failed.") );
            retval = TPR_FAIL;
            goto cleanup;
        }

        // Retrieve IVMRMixerControl2 interface
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl2, (void **)&pVMRMixerControl2 );
        CHECKHR( hr, TEXT("Query IVMRMixerControl2 interface using remote graph builder") );

        //always put the video with max native size on pin 0
        hr = pVMRMixerControl2->GetStreamNativeVideoSize(0, &lWidth, &lHeight);

        LOG( TEXT("GetStreamNativeVideoSize returned stream %d: (%d, %d)"), 0, lWidth, lHeight );

    }else{
        hr = pVMRWindowlessControl->GetNativeVideoSize( &lWidth, &lHeight, NULL, NULL );
        LOG( TEXT("GetNativeVideoSize returned (%d, %d)"), lWidth, lHeight);
    }


    RECT rcVideoWindow;
    RECT rcVideoPosition;
    hwndApp = OpenAppWindow( 0, 0, lWidth + pTestDesc->m_dwWindowSizeChange, lHeight + pTestDesc->m_dwWindowSizeChange, &rcVideoWindow, pVMRWindowlessControl );

    if ( !hwndApp )
    {
        hr = E_FAIL;
        CHECKHR( hr, TEXT("Create an application window...") );
    }

    if (pTestDesc->m_vmrTestMode == IMG_CAPTURE_MODE )
    {
        //If in capture mode, send in the wnd handle
        pCustomAP->SetWndHandle(hwndApp);
    }

    hr = pVMRWindowlessControl->SetVideoClippingWindow( hwndApp );
    CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

    //Center the image in case window szie is bigger than video size
    rcVideoPosition.top = abs(((rcVideoWindow.bottom - rcVideoWindow.top) - lHeight))/2;
    rcVideoPosition.left = abs(((rcVideoWindow.right - rcVideoWindow.left) - lWidth))/2;
    rcVideoPosition.bottom = abs(rcVideoWindow.bottom - ((rcVideoWindow.bottom - rcVideoWindow.top) - lHeight)/2);
    rcVideoPosition.right = abs(rcVideoWindow.right - ((rcVideoWindow.right - rcVideoWindow.left) - lWidth)/2);

    hr = pVMRWindowlessControl->SetVideoPosition( NULL, &rcVideoPosition );
    CHECKHR( hr, TEXT("SetVideoPosition ..."));

    pCustomAP->SetCompositedSize( abs(lWidth), abs(lHeight) );

    hr = testGraph.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    // Get the max duration of the input video streams
    LONGLONG llMaxDuration = 0;
    hr = testGraph.GetDuration( &llMaxDuration );

    scaleConfigList = pTestDesc->multiScaleList;
    rotateConfigList = pTestDesc->multiRotateConfigList;
    iRotate = 0;
    iScale = 0;

    //Get number of rotate operations
    uRSize = rotateConfigList.size();
    uSSize = scaleConfigList.size();

    while(iRotate < (int)uRSize || iScale < (int)uSSize)
    {
        //Stopped Rotate
        if( iRotate < (int)uRSize)
        {
            hr = VMRRotate(&testGraph, rotateConfigList[iRotate], &bRotateWindows, &dwRotation);
            if (hr == DISP_CHANGE_BADMODE)
            {
                //The specified graphics mode is not supported
                retval = TPR_SKIP;
                goto cleanup;
            }
            CHECKHR(hr,TEXT("Stopped Rotate video ..." ));
            iRotate++;
            pCustomAP->SetRotation(dwRotation);
        }

        //Stopped Scale window
        if( iScale < (int)uSSize)
        {
            hr = VMRScale(&testGraph, pVMR, scaleConfigList[iScale], bRotateWindows, pTestDesc->MixingMode() );
            if (hr == S_FALSE)
            {
                //Codec doesn't support scaling
                retval = TPR_SKIP;
                goto cleanup;
            }
            CHECKHR(hr,TEXT("Stopped Scale video ..." ));
            iScale++;
        }

        //Center the Window and rotate it if necessary
        if(pTestDesc->m_bUpdateWindow)
        {
            hr = VMRUpdateWindow(pVMR, hwndApp, bRotateWindows);
            CHECKHR(hr,TEXT("Stopped VMR Update window ..." ));
        }
    }

      // Set the graph as run for short time
    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
    CHECKHR(hr,TEXT("Run graph for 1 second ..." ));
    Sleep(1000);

    // The paused test
    // Set the graph as pause
    hr = testGraph.SetState(State_Paused, TestGraph::SYNCHRONOUS);
    CHECKHR(hr,TEXT("Pause graph ..." ));

    scaleConfigList = pTestDesc->pausedScaleList;
    rotateConfigList = pTestDesc->pausedRotateConfigList;
    iRotate = 0;
    iScale = 0;

    //Get number of rotate operations
    uRSize = rotateConfigList.size();
    uSSize = scaleConfigList.size();

    while(iRotate < (int)uRSize || iScale < (int)uSSize)
    {
        //Paused Rotate
        if( iRotate < (int)uRSize)
        {
            hr = VMRRotate(&testGraph, rotateConfigList[iRotate], &bRotateWindows, &dwRotation);
            CHECKHR(hr,TEXT("Paused Rotate video ..." ));
            iRotate++;
            pCustomAP->SetRotation(dwRotation);
        }

        //Paused Scale window
        if( iScale < (int)uSSize)
        {
            hr = VMRScale(&testGraph, pVMR, scaleConfigList[iScale], bRotateWindows, pTestDesc->MixingMode() );
            CHECKHR(hr,TEXT("Paused Scale video ..." ));
            iScale++;
        }

        //Center the Window and rotate it if necessary
        if(pTestDesc->m_bUpdateWindow)
        {
            hr = VMRUpdateWindow(pVMR, hwndApp, bRotateWindows);
            CHECKHR(hr,TEXT("Paused VMR Update window ..." ));
        }
    }

    // The running test
    // Set the graph as running
    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
    CHECKHR(hr,TEXT("Run graph ..." ));

    scaleConfigList = pTestDesc->dynamicScaleList;
    rotateConfigList = pTestDesc->dynamicRotateConfigList;
    iRotate = 0;
    iScale = 0;

    //Get number of rotate operations
    uRSize = rotateConfigList.size();
    uSSize = scaleConfigList.size();
    uMax = max(uRSize, uSSize);

   // Sleep time
    if(uMax > 0)
        dwSleepTime = TICKS_TO_MSEC(llMaxDuration)/uMax;

    while(iRotate < (int)uRSize || iScale < (int)uSSize)
    {
        //Dynamic Rotate
        if( iRotate < (int)uRSize)
        {
            hr = VMRRotate(&testGraph, rotateConfigList[iRotate], &bRotateWindows, &dwRotation);
            CHECKHR(hr,TEXT("Dynamic Rotate video ..." ));
            iRotate++;
            pCustomAP->SetRotation(dwRotation);
        }

        //Dynamic Scale window
        if( iScale < (int)uSSize)
        {
            hr = VMRScale(&testGraph, pVMR, scaleConfigList[iScale], bRotateWindows, pTestDesc->MixingMode());
            CHECKHR(hr,TEXT("Dynamic Scale video ..." ));
            iScale++;
        }

        //Center the Window and rotate it if necessary
        if(pTestDesc->m_bUpdateWindow)
        {
            hr = VMRUpdateWindow(pVMR, hwndApp, bRotateWindows);
            CHECKHR(hr,TEXT("Dynamic VMR Update window ..." ));
        }
        Sleep(dwSleepTime);

        // Check the results of the remaining verification
    }

    //No dynamic operation
    if(uMax == 0)
    {
        DWORD   dwStart = GetTickCount();
        DWORD   dwStop = TICKS_TO_MSEC(llMaxDuration);
        while ( GetTickCount() - dwStart < dwStop )
        {
            MSG  msg;
            while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            // give a chance to others...
            Sleep(1);
        }
    }

    //Playback is done and stop the graph
    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

    //If we check is using overlays
    if (pTestDesc->bCheckUsingOverlay)
    {
        if(!pCustomAP->IsUsingOverlay())
            retval = TPR_FAIL;
    }

    // If in image stream comparision verification mode, check verification result
    if(pTestDesc->m_vmrTestMode == IMG_STM_VERIFICATION_MODE)
    {
        dwPassCount = pCustomAP->GetMatchCount();
        if(dwPassCount >= pTestDesc->m_dwPassRate)
            retval = TPR_FAIL;
    }

    // If in images comparision mode, compare images
    if(pTestDesc->m_vmrTestMode == IMG_VERIFICATION_MODE)
    {
        dwPassCount = pCustomAP->CompareSavedImages(pTestDesc->szSrcImgPath);
        LOG( TEXT("Image Comparision: %d images match and test configure pass rate is %d!"), dwPassCount,  pTestDesc->m_dwPassRate);

        if(dwPassCount < pTestDesc->m_dwPassRate)
        {
            LOG( TEXT("Verification Failed!!"));
            retval = TPR_FAIL;
        }else{
            LOG( TEXT("Verification Succeeded!"));
        }
    }

cleanup:
    //Any failure from test running should fail the case. Test should skip if rotation not supported.
    if (hr == DISP_CHANGE_BADMODE)
        retval = TPR_SKIP;
    else if ( FAILED(hr) )
        retval = TPR_FAIL;

    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
        retval = TPR_FAIL;

    hr = RotateUI (AM_ROTATION_ANGLE_0);
    if ( FAILED(hr) ) {
        LOG(TEXT("%S: ERROR %d@%S - failed to Rotate UI back. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    if ( pCustomAP )
        pCustomAP->Deactivate();

    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if ( pVMRWindowlessControl )
        pVMRWindowlessControl->Release();

    if ( pVMRMixerControl2 )
        pVMRMixerControl2->Release();

    if ( pVMRMixerControl )
        pVMRMixerControl->Release();

    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMR )
        pVMR->Release();

    // Reset the testGraph
    testGraph.Reset();

    if ( pCustomAP )
        pCustomAP->Release();

    // Reset the test
    pTestDesc->Reset();


    hr = EnableWMVDecoderFrameDrop();
    if(FAILED(hr))
    {
        LOG( TEXT("WARN! Failed to enable WMV decoder drop frame (0x%x) !!"), hr);
    }

    //Restore upstream rotate
    if(bUpstreamRotateEnabled)
    {
        hr = RestoreOrigUpstreamRotate(dwOrigUpstreamRotationPreferred);
        if(FAILED(hr))
        {
            LOG( TEXT("WARN! Failed to restore original upstream rotate configure (0x%x) !!"), hr);
        }
    }

    //Restore upstream scale
    if(bUpstreamScaleEnabled)
    {
        hr = RestoreOrigUpstreamScale(dwOrigUpstreamScalingPreferred);
        if(FAILED(hr))
        {
            LOG( TEXT("WARN! Failed to restore original upstream scale configure (0x%x) !!"), hr);
        }
    }

    return retval;
}

////////////////////////////////////////////////////////////////////////////////
// Rotation_Scaling_FrameStep_Test :
//    Rotation scaling test with frame stepping
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI
Rotation_Scaling_FrameStep_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IMediaEvent *pEvent = 0x0;
    IVMRFilterConfig    *pVMRConfig = NULL;
    IVMRMixerControl    *pVMRMixerControl = NULL;
    IVMRMixerControl2    *pVMRMixerControl2 = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;
    HWND    hwndApp = NULL;
    IVideoFrameStep   *pVideoFrameStep = NULL;
    IGraphBuilder *pGraphBuilder = NULL;
    TCHAR tmp[128] = {0};
    RotateConfigList rotateConfigList;
    WndScaleList scaleConfigList;
    CVMRRSCustomAP   *pCustomAP = NULL;
    VMRStreamData   streamData;
    DWORD dwPassCount = 0;
    UINT uSize = 0, uRSize = 0, uSSize = 0, uMax = 0;
    int iScale = 0, iRotate = 0;
    bool bRotateWindows = false;
    bool bUpstreamRotateEnabled = false;
    bool bUpstreamScaleEnabled = false;
    DWORD dwRotation = 0;
    DWORD dwOrigUpstreamRotationPreferred = 0xffffffff;
    DWORD dwOrigUpstreamScalingPreferred = 0xffffffff;
    DWORD dwPrefs;
    DWORD dwSleepTime = 0;
    DWORD dwNSteps = 0;
    LONG lWidth, lHeight;

    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    RotationScalingTestDesc* pTestDesc = (RotationScalingTestDesc*)lpFTE->dwUserData;

    TestGraph testGraph;

    if ( pTestDesc->MixingMode() )
        testGraph.UseVMR( TRUE );

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"),
            __FUNCTION__, __LINE__, __FILE__, hr );
    retval = TPR_FAIL;
    goto cleanup;
    }

    //initialize custom AP
    pCustomAP = new CVMRRSCustomAP( &hr );
    if ( !pCustomAP )
       hr = E_OUTOFMEMORY;

    CHECKHR( hr, TEXT("Creating the custom allocator presenter ...") );

    pCustomAP->AddRef();

    //Disable WMV decoder frame dropping if we want to save all frames at PresentImage in CustomAP
    hr = DisableWMVDecoderFrameDrop();
    CHECKHR( hr, TEXT("DisableWMVDecoderFrameDrop ...") );

      //Enable upstream rotate
    if(pTestDesc->bPrefUpstreamRotate)
    {
        hr = EnableUpstreamRotate(pTestDesc->bPrefUpstreamRotate, &dwOrigUpstreamRotationPreferred);
        if(FAILED(hr))
        {
            //if failed and error is not registry not existing
            if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream rotate (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }
            else
            {
                //that key doesn't exist
                LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream rotate: the registry not exist (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
            }
        }
        else
        {
                LOG( TEXT("Enable upstream rotate: Succeeded!!"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                bUpstreamRotateEnabled = true;
        }
    }

      //Enable upstream scale
    if(pTestDesc->bPrefUpstreamScale)
    {
        hr = EnableUpstreamScale(pTestDesc->bPrefUpstreamScale, &dwOrigUpstreamScalingPreferred);
        if(FAILED(hr))
        {
            //if failed and error is not registry not existing
            if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream scale (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }
            else
            {
                //that key doesn't exist
                LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream scale: the registry not exist (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
            }
        }
        else
        {
                LOG( TEXT("Enable upstream scale: Succeeded!!"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                bUpstreamScaleEnabled = true;
        }
    }

    //Turn on image save mode on custom AP
    if (pTestDesc->m_vmrTestMode == IMG_SAVE_MODE ||pTestDesc->m_vmrTestMode == IMG_VERIFICATION_MODE)
    {
        pCustomAP->SaveImage( TRUE );
        //set where to save images
        pCustomAP->SetImagePath( pTestDesc->szDestImgPath );
    }
    else if (pTestDesc->m_vmrTestMode == IMG_STM_VERIFICATION_MODE )
    {
        //turn on verification thread
        pCustomAP->VerifyStream( TRUE );
        //set where to find source images
        pCustomAP->SetImagePath( pTestDesc->szSrcImgPath );
    }
    else if (pTestDesc->m_vmrTestMode == IMG_CAPTURE_MODE )
    {
        //turn on verification thread
        pCustomAP->CaptureImage( TRUE );
        //set where to find source images
        pCustomAP->SetImagePath( pTestDesc->szSrcImgPath );
        pCustomAP->SetWndHandle(hwndApp);
    }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the first video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilter( VideoMixingRenderer );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ),
            __FUNCTION__, __LINE__, __FILE__, hr );
    retval = TPR_FAIL;
    goto cleanup;
    }

    pVMR = testGraph.GetFilter( VideoMixingRenderer );

    if ( !pVMR )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ),
            __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pVMR->QueryInterface( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface") );

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    if ( pTestDesc->MixingMode() )
    {
        hr = pVMRConfig->SetNumberOfStreams( dwStreams );
        StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
        CHECKHR( hr, tmp );
    }

    streamData.vmrId = DW_COMPOSITED_ID;
    // The rendering mode will be set to renderless inside
    hr = pCustomAP->Activate(pVMR, NULL, &streamData );
    CHECKHR( hr, TEXT("Activate the custom allocator presenter.") );

   //Set render pref surface
    hr = pVMRConfig->GetRenderingPrefs(&dwPrefs);
    StringCchPrintf( tmp, 128, TEXT("GetRenderingPrefs ..."));
    CHECKHR( hr, tmp );

    dwPrefs &= ~(RenderPrefs_ForceOverlays | RenderPrefs_ForceOffscreen);

    if (pTestDesc->m_dwRenderingPrefsSurface & RenderPrefs_ForceOverlays)
    {
        dwPrefs |= RenderPrefs_ForceOverlays;
        hr = pVMRConfig->SetRenderingPrefs(dwPrefs);
        StringCchPrintf( tmp, 128, TEXT("SetRenderingPrefs ..."));
        CHECKHR(hr, tmp);
    }
    else if (pTestDesc->m_dwRenderingPrefsSurface & RenderPrefs_ForceOffscreen)
    {
        dwPrefs |= RenderPrefs_ForceOffscreen;
        hr = pVMRConfig->SetRenderingPrefs(dwPrefs);
        StringCchPrintf( tmp, 128, TEXT("SetRenderingPrefs ..."));
        CHECKHR(hr, tmp);
    }

    // Set windowless parameters
    hr = pVMR->QueryInterface( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
    CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

    hr = pVMRWindowlessControl->SetBorderColor( RGB(0,0,0) );
    CHECKHR( hr, TEXT("SetBorderColor as black...") );

    // Connect to VMR
    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        LOG( TEXT("Rendering the file ..."));
        testGraph.SetMedia(pTestDesc->GetMedia(i));
        if(FAILED(hr = testGraph.BuildGraph()))
        {
            LOG( TEXT("%S: ERROR %d@%S - error code: 0x%08x "),
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;

            // If we are just trying to save the filter graph with the '/GRF' command (instead of running the test for real)
            // then BuildGraph() will fail to try and short-circuit the test. In this case,
            // we want to ALWAYS ensure that we've tried to render every stream.
            continue;
        }
        else
            LOG( TEXT("Succeeded.") );
    }

    // At some point trying to render the streams failed, so bail out.
    if(retval == TPR_FAIL)
        goto cleanup;

    if ( pTestDesc->MixingMode() )
    {
        // Set alpha, z-order and output rectangles
        hr = pVMR->QueryInterface( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        // Set stream parameters
        if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
        {
            LOG( TEXT("Set stream parameters failed.") );
            retval = TPR_FAIL;
            goto cleanup;
        }
           // Retrieve IVMRMixerControl2 interface
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl2, (void **)&pVMRMixerControl2 );
        CHECKHR( hr, TEXT("Query IVMRMixerControl2 interface using remote graph builder") );

        //always put the video with max native size on pin 0
        hr = pVMRMixerControl2->GetStreamNativeVideoSize(0, &lWidth, &lHeight);

        LOG( TEXT("GetStreamNativeVideoSize returned stream %d: (%d, %d)"), 0, lWidth, lHeight );

    }
    else
    {
        hr = pVMRWindowlessControl->GetNativeVideoSize( &lWidth, &lHeight, NULL, NULL );
        LOG( TEXT("GetNativeVideoSize returned (%d, %d)"), lWidth, lHeight);
    }

    RECT rcVideoWindow;
    hwndApp = OpenAppWindow( 0, 0, lWidth, lHeight, &rcVideoWindow, pVMRWindowlessControl );

    if ( !hwndApp )
    {
        hr = E_FAIL;
        CHECKHR( hr, TEXT("Create an application window...") );
    }

    if (pTestDesc->m_vmrTestMode == IMG_CAPTURE_MODE )
    {
        //If in capture mode, send in the wnd handle
        pCustomAP->SetWndHandle(hwndApp);
    }

    hr = pVMRWindowlessControl->SetVideoClippingWindow( hwndApp );
    CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

    hr = pVMRWindowlessControl->SetVideoPosition( NULL, &rcVideoWindow );
    CHECKHR( hr, TEXT("SetVideoPosition ..."));

    pCustomAP->SetCompositedSize( abs(lWidth), abs(lHeight) );

    hr = testGraph.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    //Get IVideoFrameStep Interface
    pGraphBuilder = testGraph.GetGraph();
    if ( !pGraphBuilder )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve graph builder interface. (0x%x)"),
            __FUNCTION__, __LINE__, __FILE__,  hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pGraphBuilder->QueryInterface(&pVideoFrameStep);
    CHECKHR(hr, TEXT("Query IVideoFrameStep ..."));

    //Some CanStep interface tests
    hr = pVideoFrameStep->CanStep( pTestDesc->m_lStepSize, NULL );
    StringCchPrintf( tmp, 128, TEXT("Check if CanStep() supports stepping %d frames"), pTestDesc->m_lStepSize);
    CHECKHR( hr, tmp );

    scaleConfigList = pTestDesc->multiScaleList;
    rotateConfigList = pTestDesc->multiRotateConfigList;
    iRotate = 0;
    iScale = 0;

    //Get number of rotate operations
    uRSize = rotateConfigList.size();
    uSSize = scaleConfigList.size();

    while(iRotate < (int)uRSize || iScale < (int)uSSize)
    {
        //Stopped Rotate
        if( iRotate < (int)uRSize)
        {
            hr = VMRRotate(&testGraph, rotateConfigList[iRotate], &bRotateWindows, &dwRotation);
            if (hr == DISP_CHANGE_BADMODE)
            {
                //The specified graphics mode is not supported
                retval = TPR_SKIP;
                goto cleanup;
            }   
            CHECKHR(hr,TEXT("Stopped Rotate video ..." ));
            iRotate++;
            pCustomAP->SetRotation(dwRotation);
        }

        //Stopped Scale window
        if( iScale < (int)uSSize)
        {
            hr = VMRScale(&testGraph, pVMR, scaleConfigList[iScale], bRotateWindows, pTestDesc->MixingMode());
            CHECKHR(hr,TEXT("Stopped Scale video ..." ));
            iScale++;
        }

        //Center the Window and rotate it if necessary
        hr = VMRUpdateWindow(pVMR, hwndApp, bRotateWindows);
        CHECKHR(hr,TEXT("Stopped VMR Update window ..." ));
    }

    //First Step
    hr = pVideoFrameStep->Step(pTestDesc->m_lStepSize, NULL);
    StringCchPrintf( tmp, 128, TEXT("First Step: %d"), pTestDesc->m_lStepSize);
    CHECKHR( hr, tmp);
    dwNSteps++;

    Sleep(1000);

    scaleConfigList = pTestDesc->dynamicScaleList;
    rotateConfigList = pTestDesc->dynamicRotateConfigList;
    iRotate = 0;
    iScale = 0;

    //Get number of rotate operations
    uRSize = rotateConfigList.size();
    uSSize = scaleConfigList.size();
    uMax = max(uRSize, uSSize);
    DWORD dwStartStep = GetTickCount();
    BOOL done = FALSE;

    while (!done)
    {
        MSG  msg;
        long evCode;
        LONG_PTR param1;
        LONG_PTR param2;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // give a chance to others...
            Sleep(1);
        }

        HRESULT hrEvent = pEvent->GetEvent(&evCode, &param1,  &param2, 500);
        if (SUCCEEDED(hrEvent))
        {
            switch (evCode)
            {
                case EC_COMPLETE:
                    LOG ( TEXT("Received EC_COMPLETE!\n") );
                    done = TRUE;
                    break;

                case EC_STEP_COMPLETE:
                    LOG(TEXT("Received EC_STEP_COMPLETE - took %d ms\n"), GetTickCount() - dwStartStep);
                    //We don't want to step through whole clip, test configre how many steps we want to go
                    if (dwNSteps >= pTestDesc->m_dwTestSteps)
                    {
                        done = TRUE;
                    }
                    else
                    {
                        //We could Run or Pause graph in the middle of stepping
                       if(iRotate < (int)uRSize || iScale < (int)uSSize)
                        {
                            //Dynamic Rotate
                            if( iRotate < (int)uRSize)
                            {
                                hr = VMRRotate(&testGraph, rotateConfigList[iRotate], &bRotateWindows, &dwRotation);
                                if (hr == DISP_CHANGE_BADMODE)
                                {
                                    //The specified graphics mode is not supported
                                    retval = TPR_SKIP;
                                    goto cleanup;
                                }
                                CHECKHR(hr,TEXT("Dynamic Rotate video ..." ));
                                iRotate++;
                                pCustomAP->SetRotation(dwRotation);
                            }

                            //Dynamic Scale window
                            if( iScale < (int)uSSize)
                            {
                                hr = VMRScale(&testGraph, pVMR, scaleConfigList[iScale], bRotateWindows, pTestDesc->MixingMode());
                                CHECKHR(hr,TEXT("Dynamic Scale video ..." ));
                                iScale++;
                            }

                            //Center the Window and rotate it if necessary
                            hr = VMRUpdateWindow(pVMR, hwndApp, bRotateWindows);
                            CHECKHR(hr,TEXT("Dynamic VMR Update window ..." ));

                            Sleep(2000);
                        }

                        //Just step through
                        ++dwNSteps;
                        dwStartStep = GetTickCount();
                        hr = pVideoFrameStep->Step(pTestDesc->m_lStepSize, NULL);
                        StringCchPrintf( tmp, 128, TEXT("Step: %d"), dwNSteps);
                        CHECKHR( hr, tmp);
                    }
                    break;
            }
            pEvent->FreeEventParams(evCode, param1, param2);
        }
    }

    //Playback is done and stop the graph
    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

    //If we check is using overlays
    if (pTestDesc->bCheckUsingOverlay)
    {
        if(!pCustomAP->IsUsingOverlay())
            retval = TPR_FAIL;
    }

    // If in image stream comparision verification mode, check verification result
    if(pTestDesc->m_vmrTestMode == IMG_STM_VERIFICATION_MODE)
    {
        dwPassCount = pCustomAP->GetMatchCount();
        if(dwPassCount >= pTestDesc->m_dwPassRate)
            retval = TPR_FAIL;
    }

    // If in images comparision mode, compare images
    if(pTestDesc->m_vmrTestMode == IMG_VERIFICATION_MODE)
    {
        dwPassCount = pCustomAP->CompareSavedImages(pTestDesc->szSrcImgPath);
        LOG( TEXT("Image Comparision: %d images match and test configure pass rate is %d!"), dwPassCount,  pTestDesc->m_dwPassRate);

        if(dwPassCount < pTestDesc->m_dwPassRate)
        {
            LOG( TEXT("Verification Failed!!"));
            retval = TPR_FAIL;
        }
        else
        {
            LOG( TEXT("Verification Succeeded!"));
        }
    }

cleanup:
    //Any failure from test running should fail the case. Test should skip if the specified graphics mode is not supported.
    if (hr == DISP_CHANGE_BADMODE)
        retval = TPR_SKIP;
    else if ( FAILED(hr) )
        retval = TPR_FAIL;

    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
        retval = TPR_FAIL;

    hr = RotateUI (AM_ROTATION_ANGLE_0);
    if ( FAILED(hr) )
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to Rotate UI back. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    if ( pCustomAP )
        pCustomAP->Deactivate();

    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if ( pVMRWindowlessControl )
        pVMRWindowlessControl->Release();

    if ( pVMRMixerControl )
        pVMRMixerControl->Release();

    if ( pVMRMixerControl2 )
        pVMRMixerControl2->Release();

    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMR )
        pVMR->Release();

    if ( pVideoFrameStep)
        pVideoFrameStep->Release();

    if( pGraphBuilder)
        pGraphBuilder->Release();

    // Reset the testGraph
    testGraph.Reset();

    if ( pCustomAP )
        pCustomAP->Release();

    // Reset the test
    pTestDesc->Reset();


    hr = EnableWMVDecoderFrameDrop();
    if(FAILED(hr))
    {
        LOG( TEXT("WARN! Failed to enable WMV decoder drop frame (0x%x) !!"), hr);
    }

    //Restore upstream rotate
    if(bUpstreamRotateEnabled)
    {
        hr = RestoreOrigUpstreamRotate(dwOrigUpstreamRotationPreferred);
        if(FAILED(hr))
        {
            LOG( TEXT("WARN! Failed to restore original upstream rotate configure (0x%x) !!"), hr);
        }
    }

    //Restore upstream scale
    if(bUpstreamScaleEnabled)
    {
        hr = RestoreOrigUpstreamScale(dwOrigUpstreamScalingPreferred);
        if(FAILED(hr))
        {
            LOG( TEXT("WARN! Failed to restore original upstream scale configure (0x%x) !!"), hr);
        }
    }

    return retval;
}

////////////////////////////////////////////////////////////////////////////////
// Rotation_Scaling_Stress_Test :
//    Rotation scaling stress test
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI
Rotation_Scaling_Stress_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IMediaEvent *pEvent = 0x0;
    IVMRFilterConfig    *pVMRConfig = NULL;
    IVMRMixerControl    *pVMRMixerControl = NULL;
    IVMRMixerControl2    *pVMRMixerControl2 = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;
    HWND    hwndApp = NULL;
    TCHAR tmp[128] = {0};
    CVMRRSCustomAP   *pCustomAP = NULL;
    VMRStreamData   streamData;
    bool bRotateWindows = false;
    bool bUpstreamRotateEnabled = false;
    bool bUpstreamScaleEnabled = false;
    DWORD dwRotation = 0;
    DWORD dwOrigUpstreamRotationPreferred = 0xffffffff;
    DWORD dwOrigUpstreamScalingPreferred = 0xffffffff;
    DWORD dwPrefs;
    DWORD dwSleepTime = 0;
    DWORD dwNSteps = 1;
    LONG lWidth, lHeight;

    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    RotationScalingTestDesc* pTestDesc = (RotationScalingTestDesc*)lpFTE->dwUserData;

    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    if ( pTestDesc->MixingMode() )
        testGraph.UseVMR( TRUE );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"),
            __FUNCTION__, __LINE__, __FILE__, hr );
    retval = TPR_FAIL;
    goto cleanup;
    }

    //initialize custom AP
    pCustomAP = new CVMRRSCustomAP( &hr );
    if ( !pCustomAP )
       hr = E_OUTOFMEMORY;

    CHECKHR( hr, TEXT("Creating the custom allocator presenter ...") );

    pCustomAP->AddRef();

    //Disable WMV decoder frame dropping if we want to save all frames at PresentImage in CustomAP
    hr = DisableWMVDecoderFrameDrop();
    CHECKHR( hr, TEXT("DisableWMVDecoderFrameDrop ...") );

      //Enable upstream rotate
    if(pTestDesc->bPrefUpstreamRotate)
    {
        hr = EnableUpstreamRotate(pTestDesc->bPrefUpstreamRotate, &dwOrigUpstreamRotationPreferred);
        if(FAILED(hr))
        {
            //if failed and error is not registry not existing
            if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                {
                LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream rotate (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }
            else
            {
                //that key doesn't exist
                LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream rotate: the registry not exist (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
            }
        }
        else
        {
                LOG( TEXT("Enable upstream rotate: Succeeded!!"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                bUpstreamRotateEnabled = true;
        }
    }

      //Enable upstream scale
    if(pTestDesc->bPrefUpstreamScale)
    {
        hr = EnableUpstreamScale(pTestDesc->bPrefUpstreamScale, &dwOrigUpstreamScalingPreferred);
        if(FAILED(hr))
        {
            //if failed and error is not registry not existing
            if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
                {
                LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream scale (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                goto cleanup;
            }
            else
            {
                //that key doesn't exist
                LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream scale: the registry not exist (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr );
            }
        }
        else
        {
                LOG( TEXT("Enable upstream scale: Succeeded!!"),
                __FUNCTION__, __LINE__, __FILE__, hr );
                bUpstreamScaleEnabled = true;
        }
    }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the first video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilter( VideoMixingRenderer );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ),
            __FUNCTION__, __LINE__, __FILE__, hr );
    retval = TPR_FAIL;
    goto cleanup;
    }

    pVMR = testGraph.GetFilter( VideoMixingRenderer );

    if ( !pVMR )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ),
            __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pVMR->QueryInterface( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface") );

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    if ( pTestDesc->MixingMode() )
    {
        hr = pVMRConfig->SetNumberOfStreams( dwStreams );
        StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
        CHECKHR( hr, tmp );
    }

    streamData.vmrId = DW_COMPOSITED_ID;
    // The rendering mode will be set to renderless inside
    hr = pCustomAP->Activate(pVMR, NULL, &streamData );
    CHECKHR( hr, TEXT("Activate the custom allocator presenter.") );

   //Set render pref surface
    hr = pVMRConfig->GetRenderingPrefs(&dwPrefs);
    StringCchPrintf( tmp, 128, TEXT("GetRenderingPrefs ..."));
    CHECKHR( hr, tmp );

    dwPrefs &= ~(RenderPrefs_ForceOverlays | RenderPrefs_ForceOffscreen);

    if (pTestDesc->m_dwRenderingPrefsSurface & RenderPrefs_ForceOverlays)
    {
        dwPrefs |= RenderPrefs_ForceOverlays;
        hr = pVMRConfig->SetRenderingPrefs(dwPrefs);
        StringCchPrintf( tmp, 128, TEXT("SetRenderingPrefs ..."));
        CHECKHR(hr, tmp);
    }
    else if (pTestDesc->m_dwRenderingPrefsSurface & RenderPrefs_ForceOffscreen)
    {
        dwPrefs |= RenderPrefs_ForceOffscreen;
        hr = pVMRConfig->SetRenderingPrefs(dwPrefs);
        StringCchPrintf( tmp, 128, TEXT("SetRenderingPrefs ..."));
        CHECKHR(hr, tmp);
    }

    // Set windowless parameters
    hr = pVMR->QueryInterface( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
    CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

    hr = pVMRWindowlessControl->SetBorderColor( RGB(0,0,0) );
    CHECKHR( hr, TEXT("SetBorderColor as black...") );

    // Connect to VMR
    for ( DWORD i = 0; i < dwStreams; i++ )
 {
        LOG( TEXT("Rendering the file ..."));
        testGraph.SetMedia(pTestDesc->GetMedia(i));
        if(FAILED(hr = testGraph.BuildGraph()))
        {
            LOG( TEXT("%S: ERROR %d@%S - error code: 0x%08x "),
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;

            // If we are just trying to save the filter graph with the '/GRF' command (instead of running the test for real)
            // then BuildGraph() will fail to try and short-circuit the test. In this case,
            // we want to ALWAYS ensure that we've tried to render every stream.
            continue;
        }
        else
            LOG( TEXT("Succeeded.") );
    }

    // At some point trying to render the streams failed, so bail out.
    if(retval == TPR_FAIL)
        goto cleanup;

    if ( pTestDesc->MixingMode() )
    {
        // Set alpha, z-order and output rectangles
        hr = pVMR->QueryInterface( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        // Set stream parameters
        if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
        {
            LOG( TEXT("Set stream parameters failed.") );
            retval = TPR_FAIL;
            goto cleanup;
        }
        // Retrieve IVMRMixerControl2 interface
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl2, (void **)&pVMRMixerControl2 );
        CHECKHR( hr, TEXT("Query IVMRMixerControl2 interface using remote graph builder") );

        //always put the video with max native size on pin 0
        hr = pVMRMixerControl2->GetStreamNativeVideoSize(0, &lWidth, &lHeight);

        LOG( TEXT("GetStreamNativeVideoSize returned stream %d: (%d, %d)"), 0, lWidth, lHeight );

    }
    else
    {
        hr = pVMRWindowlessControl->GetNativeVideoSize( &lWidth, &lHeight, NULL, NULL );
        LOG( TEXT("GetNativeVideoSize returned (%d, %d)"), lWidth, lHeight);
    }


    RECT rcVideoWindow;
    hwndApp = OpenAppWindow( 0, 0, lWidth, lHeight, &rcVideoWindow, pVMRWindowlessControl );

    if ( !hwndApp )
    {
        hr = E_FAIL;
        CHECKHR( hr, TEXT("Create an application window...") );
    }

    hr = pVMRWindowlessControl->SetVideoClippingWindow( hwndApp );
    CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );

    hr = pVMRWindowlessControl->SetVideoPosition( NULL, &rcVideoWindow );
    CHECKHR( hr, TEXT("SetVideoPosition ..."));

    pCustomAP->SetCompositedSize( abs(lWidth), abs(lHeight) );

    hr = testGraph.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    LONGLONG llMaxDuration = 0;
    hr = testGraph.GetDuration( &llMaxDuration );
    CHECKHR( hr, TEXT("GetDuration ...") );

    //Run the graph
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, tmp );

    //Get number of rotate operations
    BOOL done = FALSE;
    AM_ROTATION_ANGLE angle = AM_ROTATION_ANGLE_0;
    DWORD   dwStart = GetTickCount();
    DWORD   dwStop = 2*TICKS_TO_MSEC(llMaxDuration);

    //Keep rotating if we not are at the end of stream or exceeds 2x clip duration
    while (!done && ( GetTickCount() - dwStart < dwStop ))
    {
        MSG  msg;
        long evCode;
        LONG_PTR param1;
        LONG_PTR param2;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            // give a chance to others...
            Sleep(1);
        }

        // empty the event queue before rotating again

        HRESULT hrEvent = pEvent->GetEvent(&evCode, &param1,  &param2, 500);
        while (SUCCEEDED(hrEvent))
        {
            switch (evCode)
            {
                case EC_COMPLETE:
                    LOG ( TEXT("Received EC_COMPLETE!\n") );
                    done = TRUE;
                    break;
                case EC_VIDEOFRAMEREADY:
                    LOG ( TEXT("Received EC_VIDEOFRAMEREADY!\n") );
                    break;
            }
            pEvent->FreeEventParams(evCode, param1, param2);

            hrEvent = pEvent->GetEvent(&evCode, &param1,  &param2, 0);
        }

        // let the video run for 3 seconds, and then rotate
        Sleep(3000);
        switch (dwNSteps % 4)
        {
            case 0:
                angle = AM_ROTATION_ANGLE_0;
                bRotateWindows = false;
                break;
            case 1:
                angle = AM_ROTATION_ANGLE_90;
                bRotateWindows = true;
                break;
            case 2:
                angle = AM_ROTATION_ANGLE_180;
                bRotateWindows = false;
                break;
            case 3:
                angle = AM_ROTATION_ANGLE_270;
                bRotateWindows = true;
                break;
        }

        hr = RotateUI (angle);
        CHECKHR(hr,TEXT("Rotate UI ..." ));

        //Center the Window and rotate it if necessary
        hr = VMRUpdateWindow(pVMR, hwndApp, bRotateWindows);
        CHECKHR(hr,TEXT("Stopped VMR Update window ..." ));
        dwNSteps++;

    }

    //Playback is done and stop the graph
    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

cleanup:
    //Any failure from test running should fail the case
    if ( FAILED(hr) )
        retval = TPR_FAIL;

    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) )
        retval = TPR_FAIL;

    hr = RotateUI (AM_ROTATION_ANGLE_0);
    if ( FAILED(hr) ) {
        LOG(TEXT("%S: ERROR %d@%S - failed to Rotate UI back. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    if ( pCustomAP )
        pCustomAP->Deactivate();

    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if ( pVMRWindowlessControl )
        pVMRWindowlessControl->Release();

    if ( pVMRMixerControl )
        pVMRMixerControl->Release();

    if ( pVMRMixerControl2 )
        pVMRMixerControl2->Release();

    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMR )
        pVMR->Release();

    // Reset the testGraph
    testGraph.Reset();

    if ( pCustomAP )
        pCustomAP->Release();

    // Reset the test
    pTestDesc->Reset();


    hr = EnableWMVDecoderFrameDrop();
    if(FAILED(hr))
    {
        LOG( TEXT("WARN! Failed to enable WMV decoder drop frame (0x%x) !!"), hr);
    }

    //Restore upstream rotate
    if(bUpstreamRotateEnabled)
    {
        hr = RestoreOrigUpstreamRotate(dwOrigUpstreamRotationPreferred);
        if(FAILED(hr))
        {
            LOG( TEXT("WARN! Failed to restore original upstream rotate configure (0x%x) !!"), hr);
        }
    }

    //Restore upstream scale
    if(bUpstreamScaleEnabled)
    {
        hr = RestoreOrigUpstreamScale(dwOrigUpstreamScalingPreferred);
        if(FAILED(hr))
        {
            LOG( TEXT("WARN! Failed to restore original upstream scale configure (0x%x) !!"), hr);
        }
    }

    return retval;
}

