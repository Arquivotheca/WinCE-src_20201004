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
#include "VMRTest.h"
#include "VideoUtil.h"

// 
// play back multiple video streams using one single VMR in renderless mode using a custom
// allocator presenter.
//
TESTPROCAPI 
VMRVideoFrameStepTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = NULL;
    IBaseFilter   *pAR = NULL;
    IMediaEvent *pEvent = NULL;
    IVMRFilterConfig    *pVMRConfig = NULL;
    IVMRMixerControl    *pVMRMixerControl = NULL;
    IVMRWindowlessControl   *pVMRWindowlessControl = NULL;
    IVideoFrameStep   *pVideoFrameStep = NULL;
    HWND    hwndApp = NULL;
    IGraphBuilder *pGraphBuilder = NULL;    
    TCHAR tmp[128] = {0};
    CBaseCustomAP   *pCustomAP = NULL;
    VMRStreamData   streamData;
    DWORD dwPassCount = 0;
    DWORD dwOrigUpstreamRotationPreferred = 0xffffffff;
    DWORD dwOrigUpstreamScalingPreferred = 0xffffffff;
    DWORD dwPrefs;
    DWORD dwSleepTime = 0;
    bool bUpstreamRotateEnabled = false;
    bool bUpstreamScaleEnabled = false;    
    DWORD dwNSteps = 0;

    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    VMRTestDesc* pTestDesc = (VMRTestDesc*)lpFTE->dwUserData;
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
    pCustomAP = new CBaseCustomAP( &hr );
    if ( !pCustomAP )
       hr = E_OUTOFMEMORY;

    CHECKHR( hr, TEXT("Creating the custom allocator presenter ...") );

    pCustomAP->AddRef();

    //Disable WMV decoder frame dropping if we want to save all frames at PresentImage in CustomAP
    hr = DisableWMVDecoderFrameDrop();
    CHECKHR( hr, TEXT("DisableWMVDecoderFrameDrop ...") );

  //Enable upstream rotate
    hr = EnableUpstreamRotate(true, &dwOrigUpstreamRotationPreferred);
    if(FAILED(hr))
    {
        //if failed and error is not registry not existing
        if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
            LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream rotate (0x%x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }else{
            //that key doesn't exist
            LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream rotate: the registry not exist (0x%x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        }
    }else{
            LOG( TEXT("Enable upstream rotate: Succeeded!!"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
            bUpstreamRotateEnabled = true;
    }

    //Enable upstream scale
    hr = EnableUpstreamScale(true, &dwOrigUpstreamScalingPreferred);
    if(FAILED(hr))
    {
        //if failed and error is not registry not existing
        if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
            LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream scale (0x%x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }else{
            //that key doesn't exist
            LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream scale: the registry not exist (0x%x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        }
    }else{
            LOG( TEXT("Enable upstream scale: Succeeded!!"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
            bUpstreamScaleEnabled = true;
        }

    //Turn on image save mode on custom AP
    if (pTestDesc->m_vmrTestMode == IMG_SAVE_MODE ||pTestDesc->m_vmrTestMode == IMG_VERIFICATION_MODE)
    {
        pCustomAP->SaveImage( TRUE );
        //set where to save images
        pCustomAP->SetImagePath( pTestDesc->szDestImgPath );
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

    //Get VMR and AR interfaces
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
        testGraph.SetMedia(pTestDesc->GetMedia(i));
        hr = testGraph.BuildGraph();
        CHECKHR( hr, TEXT("Rendering the file ...") );
    }

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
    }

    LONG lWidth, lHeight, lARWidth, lARHeight;
    hr = pVMRWindowlessControl->GetNativeVideoSize( &lWidth, &lHeight, &lARWidth, &lARHeight );

    LOG( TEXT("GetNativeVideoSize returned (%d, %d, %d, %d)"), lWidth, lHeight, lARWidth, lARHeight );

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

    //Get Audio Renderer Filter
    pAR = testGraph.GetFilter( DefaultAudioRenderer );
    if ( !pAR )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the AR from the graph" ), 
            __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

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

    //Only VMR CanStep
    hr = pVideoFrameStep->CanStep( 0, pAR );
    if ( SUCCEEDED(hr) )
    {
        LOG(TEXT("Audio Render should not support IVideoFrameStep!!"));
        retval = TPR_FAIL;
        goto cleanup;
    } else
        LOG(TEXT("Audio Render does not support IVideoFrameStep!!"));

    //Some CanStep interface tests
    hr = pVideoFrameStep->CanStep( pTestDesc->m_lStepSize, NULL );
    StringCchPrintf( tmp, 128, TEXT("Check if CanStep() supports stepping %d frames"), pTestDesc->m_lStepSize);
    CHECKHR( hr, tmp );

    hr = pVideoFrameStep->CanStep( pTestDesc->m_lFirstStepSize, NULL );
    StringCchPrintf( tmp, 128, TEXT("Check if CanStep() supports stepping %d frames"), pTestDesc->m_lFirstStepSize);
    CHECKHR( hr, tmp );

    //We could run or pause graph first before we do frame stepping
    if(pTestDesc->m_bRunFirst)
    {
        hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
        CHECKHR(hr,TEXT("Run graph first for 2 seconds..." ));
        Sleep(2000);
    }else if (pTestDesc->m_bPauseFirst)
    {
        hr = testGraph.SetState(State_Paused, TestGraph::SYNCHRONOUS);
        CHECKHR(hr,TEXT("Pause graph first for 2 seconds..." ));
        Sleep(2000);
    }

    //First Step
    hr = pVideoFrameStep->Step(pTestDesc->m_lFirstStepSize, NULL);
    StringCchPrintf( tmp, 128, TEXT("First Step: %d"), pTestDesc->m_lFirstStepSize);
    CHECKHR( hr, tmp);                       
    dwNSteps++;

    if(pTestDesc->m_lFirstStepSize >= 1000)
    {
        LOG(TEXT("Sleep 3 seconds!!"));
        Sleep(3000);
        if(pTestDesc->m_bCancelStep)
        {
            hr = pVideoFrameStep->CancelStep();
            StringCchPrintf( tmp, 128, TEXT("Cancel Step operation of %d steps"), pTestDesc->m_lFirstStepSize);
            CHECKHR( hr, tmp); 

            hr = pVideoFrameStep->Step(pTestDesc->m_lStepSize, NULL);
            StringCchPrintf( tmp, 128, TEXT("Step: %d"), ++dwNSteps);
            CHECKHR( hr, tmp);                       
        }
    }
    
    DWORD dwStartStep = GetTickCount();
    BOOL done = FALSE;
    LONGLONG llDuration = 0; 
    hr = testGraph.GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
	    LOG( TEXT("Fail to the get duration of the clip. 0x%08x"), hr );
	    goto cleanup;
    }
    else
	    llDuration /= 10000; // convert to ms
    
    while (!done)
    {
        MSG  msg;
        long evCode = 0;
        LONG_PTR param1 = 0;
        LONG_PTR param2 = 0;

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
                        if(pTestDesc->m_bRunInMiddle)
                        {
                            hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
                            CHECKHR(hr,TEXT("Run graph in the middle of stepping for 2 seconds ..." ));
                            Sleep(2000);
                        }else if (pTestDesc->m_bPauseInMiddle)
                        {
                            hr = testGraph.SetState(State_Paused, TestGraph::SYNCHRONOUS);
                            CHECKHR(hr,TEXT("Pause graph in the middle of stepping for 2 seconds ..." ));
                            Sleep(2000);
                        }
                        //Just step through
                        Sleep(300);
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

        if ( GetTickCount() - dwStartStep > 3 * llDuration )
        {
	        LOG( TEXT("ERROR: No EC_STEP_COMPLETE for more than %d ms!!"), llDuration );
	        hr = E_FAIL;
	        goto cleanup;
        }
    }

   
    //Playback is done and stop the graph
    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

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

    if ( pCustomAP )
        pCustomAP->Deactivate();

    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if ( pVMRWindowlessControl ) 
        pVMRWindowlessControl->Release();

    if ( pVMRMixerControl ) 
        pVMRMixerControl->Release();

    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMR ) 
        pVMR->Release();

    if ( pAR ) 
        pAR->Release();

    if ( pVideoFrameStep) 
        pVideoFrameStep->Release();
    
    if( pGraphBuilder)
        pGraphBuilder->Release();

    // Reset the testGraph
    testGraph.Reset();

    if ( pCustomAP )
        pCustomAP->Release();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

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


// 
// pretty much the same as the above test, but make the test applicable for both local and 
// remote scenarios.
//
TESTPROCAPI 
VMRVideoFrameStepNoCustomAPTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IGraphBuilder         *pGB = NULL;
    IAMAudioRendererStats *pARStats = NULL;
    IVMRFilterConfig      *pVMRConfig = NULL;
    IVMRMixerControl      *pVMRMixerControl = NULL;
    IVideoFrameStep       *pVideoFrameStep = NULL;
    IMediaEvent           *pEvent = NULL;
    TCHAR tmp[128] = {0};
    DWORD dwPassCount = 0;
    DWORD dwOrigUpstreamRotationPreferred = 0xffffffff;
    DWORD dwOrigUpstreamScalingPreferred = 0xffffffff;
    DWORD dwPrefs;
    DWORD dwSleepTime = 0;
    bool bUpstreamRotateEnabled = false;
    bool bUpstreamScaleEnabled = false;    
    DWORD dwNSteps = 0;

    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    VMRTestDesc* pTestDesc = (VMRTestDesc*)lpFTE->dwUserData;
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

    //Disable WMV decoder frame dropping if we want to save all frames at PresentImage in CustomAP
    hr = DisableWMVDecoderFrameDrop();
    CHECKHR( hr, TEXT("DisableWMVDecoderFrameDrop ...") );

    //Enable upstream rotate
    hr = EnableUpstreamRotate(true, &dwOrigUpstreamRotationPreferred);
    if(FAILED(hr))
    {
        //if failed and error is not registry not existing
        if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
            LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream rotate (0x%x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }else{
            //that key doesn't exist
            LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream rotate: the registry not exist (0x%x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        }
    }else{
            LOG( TEXT("Enable upstream rotate: Succeeded!!"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
            bUpstreamRotateEnabled = true;
    }

    //Enable upstream scale
    hr = EnableUpstreamScale(true, &dwOrigUpstreamScalingPreferred);
    if(FAILED(hr))
    {
        //if failed and error is not registry not existing
        if(hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            {
            LOG( TEXT("%S: ERROR %d@%S - failed to Enable upstream scale (0x%x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }else{
            //that key doesn't exist
            LOG( TEXT("%S: WARN %d@%S - failed to Enable upstream scale: the registry not exist (0x%x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        }
    }else{
            LOG( TEXT("Enable upstream scale: Succeeded!!"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
            bUpstreamScaleEnabled = true;
        }

    // Add the VMR manually
    LOG( TEXT("%S: - adding the first video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph.AddFilterByCLSID( CLSID_VideoMixingRenderer, L"VideoMixingRenderer" );
	if ( FAILED(hr) )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
						__FUNCTION__, __LINE__, __FILE__, hr );
		retval = TPR_FAIL;
		goto cleanup;
	}

    hr = testGraph.FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pVMRConfig );
	if ( !pVMRConfig )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IVMRFilterConfig from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}


    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    if ( pTestDesc->MixingMode() )
    {
        hr = pVMRConfig->SetNumberOfStreams( dwStreams );
        StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
        CHECKHR( hr, tmp );
    }

    // Connect to VMR
    for ( DWORD i = 0; i < dwStreams; i++ )
    {
        testGraph.SetMedia( pTestDesc->GetMedia(i) );
        hr = testGraph.BuildGraph();
        CHECKHR( hr, TEXT("Rendering the file ...") );
    }

    //Set render pref surface
    hr = pVMRConfig->GetRenderingPrefs(&dwPrefs);
    StringCchPrintf( tmp, 128, TEXT("GetRenderingPrefs ..."));
    CHECKHR( hr, tmp );

    dwPrefs &= ~(RenderPrefs_ForceOverlays | RenderPrefs_ForceOffscreen);
    if ( pTestDesc->m_dwRenderingPrefsSurface & RenderPrefs_ForceOverlays )
    {
        dwPrefs |= RenderPrefs_ForceOverlays;
        hr = pVMRConfig->SetRenderingPrefs(dwPrefs);
        StringCchPrintf( tmp, 128, TEXT("SetRenderingPrefs ..."));
        CHECKHR(hr, tmp);
    }
    else if ( pTestDesc->m_dwRenderingPrefsSurface & RenderPrefs_ForceOffscreen )
    {
        dwPrefs |= RenderPrefs_ForceOffscreen;
        hr = pVMRConfig->SetRenderingPrefs(dwPrefs);
        StringCchPrintf( tmp, 128, TEXT("SetRenderingPrefs ..."));
        CHECKHR(hr, tmp);
    }

    if ( pTestDesc->MixingMode() )
    {
        // Set alpha, z-order and output rectangles
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        // Set stream parameters
        if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
        {
            LOG( TEXT("Set stream parameters failed.") );
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
        
    //Get Audio Renderer Filter
    hr = testGraph.FindInterfaceOnGraph( IID_IAMAudioRendererStats, (void **)&pARStats );
	if ( FAILED(hr) )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IAMAudioRendererStats from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}

    pGB = (IGraphBuilder *)testGraph.GetGraph();
    if ( !pGB )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the filter graph manager!" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}

    hr = pGB->QueryInterface( IID_IVideoFrameStep, (void **)&pVideoFrameStep);
    CHECKHR(hr, TEXT("Query IVideoFrameStep ..."));

    //Only VMR CanStep
    hr = pVideoFrameStep->CanStep( 0, (IBaseFilter *)pARStats );
    if ( SUCCEEDED(hr) )
    {
        LOG(TEXT("Audio Render should not support IVideoFrameStep!!"));
        retval = TPR_FAIL;
        goto cleanup;
    } else
        LOG(TEXT("Audio Render does not support IVideoFrameStep!!"));

    //Some CanStep interface tests
    hr = pVideoFrameStep->CanStep( pTestDesc->m_lStepSize, NULL );
    StringCchPrintf( tmp, 128, TEXT("Check if CanStep() supports stepping %d frames"), pTestDesc->m_lStepSize);
    CHECKHR( hr, tmp );

    hr = pVideoFrameStep->CanStep( pTestDesc->m_lFirstStepSize, NULL );
    StringCchPrintf( tmp, 128, TEXT("Check if CanStep() supports stepping %d frames"), pTestDesc->m_lFirstStepSize);
    CHECKHR( hr, tmp );

    //We could run or pause graph first before we do frame stepping
    if(pTestDesc->m_bRunFirst)
    {
        hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
        CHECKHR(hr,TEXT("Run graph first for 2 seconds..." ));
        Sleep(2000);
    }else if (pTestDesc->m_bPauseFirst)
    {
        hr = testGraph.SetState(State_Paused, TestGraph::SYNCHRONOUS);
        CHECKHR(hr,TEXT("Pause graph first for 2 seconds..." ));
        Sleep(2000);
    }

    //First Step
    hr = pVideoFrameStep->Step(pTestDesc->m_lFirstStepSize, NULL);
    StringCchPrintf( tmp, 128, TEXT("First Step: %d"), pTestDesc->m_lFirstStepSize);
    CHECKHR( hr, tmp);                       
    dwNSteps++;

    if(pTestDesc->m_lFirstStepSize >= 1000)
    {
        LOG(TEXT("Sleep 3 seconds!!"));
        Sleep(3000);
        if(pTestDesc->m_bCancelStep)
        {
            hr = pVideoFrameStep->CancelStep();
            StringCchPrintf( tmp, 128, TEXT("Cancel Step operation of %d steps"), pTestDesc->m_lFirstStepSize);
            CHECKHR( hr, tmp); 

            hr = pVideoFrameStep->Step(pTestDesc->m_lStepSize, NULL);
            StringCchPrintf( tmp, 128, TEXT("Step: %d"), ++dwNSteps);
            CHECKHR( hr, tmp);                       
        }
    }
    
    
    hr = testGraph.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    DWORD dwStartStep = GetTickCount();
    BOOL done = FALSE;
    HRESULT hrEvent = S_OK;
    LONGLONG llDuration = 0; 
    hr = testGraph.GetDuration( &llDuration );
    if ( FAILED(hr) )
    {
	    LOG( TEXT("Fail to the get duration of the clip. 0x%08x"), hr );
	    goto cleanup;
    }
    else
	    llDuration /= 10000; // convert to ms

    while (!done)
    {
        long evCode = 0;
        LONG_PTR param1 = 0;
        LONG_PTR param2 = 0;
   
        hrEvent = pEvent->GetEvent(&evCode, &param1,  &param2, 500);		
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
                        if(pTestDesc->m_bRunInMiddle)
                        {
                            hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
                            CHECKHR(hr,TEXT("Run graph in the middle of stepping for 2 seconds ..." ));
                            Sleep(2000);
                        }else if (pTestDesc->m_bPauseInMiddle)
                        {
                            hr = testGraph.SetState(State_Paused, TestGraph::SYNCHRONOUS);
                            CHECKHR(hr,TEXT("Pause graph in the middle of stepping for 2 seconds ..." ));
                            Sleep(2000);
                        }
                        //Just step through
                        Sleep(300);
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

        if ( GetTickCount() - dwStartStep > 3 * llDuration )
        {
	        LOG( TEXT("ERROR: No EC_STEP_COMPLETE for more than %d ms!!"), llDuration );
	        hr = E_FAIL;
	        goto cleanup;
        }
    }

cleanup:
    //Any failure from test running should fail the case
    if ( FAILED(hr) )
        retval = TPR_FAIL;

    hr = testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    if ( FAILED(hr) ) {
        LOG(TEXT("%S: ERROR %d@%S - failed to stop the graph. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

    if ( pVMRMixerControl ) 
        pVMRMixerControl->Release();

    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pARStats ) 
        pARStats->Release();

    if ( pVideoFrameStep) 
        pVideoFrameStep->Release();
    
    if ( pGB )
        pGB->Release();

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

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
