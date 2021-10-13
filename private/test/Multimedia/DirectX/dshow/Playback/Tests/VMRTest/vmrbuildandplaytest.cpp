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
//  Video Mixing Renderer Interface Test:
//  1. Blending with video streams with native sizes
//  2. Blending with multiple streams and resize/reposition one or more video
//     rectangles.
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <atlbase.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "VMRTestDesc.h"
#include "TestGraph.h"
#include "VMRTest.h"
#include "VideoUtil.h"
#include "BaseCustomAP.h"


//
// Build a VMR graph with multiple video streams
//
TESTPROCAPI 
VMRGraphBuildTest( UINT uMsg, 
                   TPPARAM tpParam, 
                   LPFUNCTION_TABLE_ENTRY lpFTE )
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;
    IVMRFilterConfig		*pVMRConfig = NULL;
    PlaybackMedia *pMedia = NULL;
	TCHAR tmp[128] = {0};

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

    // Add the VMR manually if custom configurations need to be done.
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
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface ...") );

    // Set the rendering mode
    DWORD dwMode = pTestDesc->GetRenderingMode();
    if ( VMRMode_Windowless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode(VMRMode_Windowless);
        CHECKHR( hr, TEXT("Setting windowless mode ...") );
    }
    else if ( VMRMode_Renderless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Renderless );
        CHECKHR( hr, TEXT("Setting renderless mode ...") );
    }
    else if ( VMRMode_Windowed == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowed );
        CHECKHR( hr, TEXT("Setting windowed mode ...") );
    }
    else
    {
        LOG( TEXT("%S: ERROR %d@%S - Invalid rendering mode %08x specified." ), 
                        __FUNCTION__, __LINE__, __FILE__, dwMode );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp,
                     128,
                     TEXT("SetNumberofStreams with %d"),
                     pTestDesc->GetNumberOfStreams() );
    CHECKHR( hr, tmp );

	// Connect to VMR
	for ( DWORD i = 0; i < dwStreams; i++ )
	{
		if ( VMR_MAX_INPUT_STREAMS == dwStreams )
            pMedia = pTestDesc->GetMedia( 0 );
		else
            pMedia = pTestDesc->GetMedia( i );

        if ( !pMedia )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the media object"), 
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            goto cleanup;
        }

        testGraph.SetMedia(pMedia);
        if(FAILED(hr = testGraph.BuildGraph()))
        {
            LOG( TEXT("Rendering media file: %s failed. (%08x)"), pMedia->GetUrl(), hr );
            retval = TPR_FAIL;

            // If we are just trying to save the filter graph with the '/GRF' command (instead of running the test for real)
            // then BuildGraph() will fail to try and short-circuit the test. In this case,
            // we want to ALWAYS ensure that we've tried to render every stream.
            continue;
        }
        else
            LOG( TEXT("Rendering media file: %s succeeded"), pMedia->GetUrl() );
    }
    
    // At some point trying to render the streams failed, so bail out.
    if(retval == TPR_FAIL)
        goto cleanup;

cleanup:
	if ( pVMRConfig )
		pVMRConfig->Release();

	// Reset the testGraph
	testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

	// Reset the test
    pTestDesc->Reset();

    return retval;
}

// 
// play back multiple video streams using one single VMR in mixing mode.
//
TESTPROCAPI 
VMRPlaybackTest( UINT uMsg, 
                 TPPARAM tpParam, 
                 LPFUNCTION_TABLE_ENTRY lpFTE )
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;
	IMediaEvent	  *pEvent = 0x0;
    IVMRFilterConfig		*pVMRConfig = NULL;
    IVMRMixerControl		*pVMRMixerControl = NULL;
    IVMRWindowlessControl	*pVMRWindowlessControl = NULL;
    PlaybackMedia *pMedia = NULL;
    HWND    hwndApp = NULL;
    TCHAR tmp[128] = {0};

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

    // Add the VMR manually if custom configurations need to be done.
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
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface...") );

    // Set the rendering mode
    DWORD dwMode = pTestDesc->GetRenderingMode();
    if ( VMRMode_Windowless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
        CHECKHR( hr, TEXT("Setting windowless mode ...") );

        // Set windowless parameters
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );
    }
    else if ( VMRMode_Windowed == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowed );
        CHECKHR( hr, TEXT("Setting windowed mode ...") );
    }
    else 
    {
        LOG( TEXT("%S: ERROR %d@%S - Invalid rendering mode %08x for the test.  For renderless mode, use VMRBlendUseCustomAPTest" ), 
                        __FUNCTION__, __LINE__, __FILE__, dwMode );
        retval = TPR_SKIP;
        goto cleanup;
    }

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp,
                     128,
                     TEXT("SetNumberofStreams with %d"), 
                     pTestDesc->GetNumberOfStreams() );
    CHECKHR( hr, tmp );

	// Connect to VMR
	for ( DWORD i = 0; i < dwStreams; i++ )
	{
		LOG( TEXT("Rendering media file: %s"), pTestDesc->GetMedia(i)->GetUrl() );
        testGraph.SetMedia(pTestDesc->GetMedia(i));
        if(FAILED(hr = testGraph.BuildGraph()))
        {
            LOG( TEXT("%S: ERROR %d@%S - Build graph failed. 0x%08x"),__FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;

			// VMR tests do not use verifier framework, instead, custom AP is used. don't need to create GRF
			// break out of the loop
			break;
        }
        else
            LOG( TEXT("Succeeded.") ); 
    }
    
    // At some point trying to render the streams failed, so bail out.
    if(retval == TPR_FAIL)
        goto cleanup;

    // Set owner
    if ( VMRMode_Windowless == dwMode )
    {
        LONG lWidth, lHeight, lARWidth, lARHeight;
        // BUGBUG: it seems GetNativeVideoSize just simply returns the size for the last connected video stream
        hr = pVMRWindowlessControl->GetNativeVideoSize( &lWidth, &lHeight, &lARWidth, &lARHeight );
        LOG( TEXT("GetNativeVideoSize returned (%d, %d, %d, %d)"), lWidth, lHeight, lARWidth, lARHeight );
        RECT rcVideoWindow;
        hwndApp = OpenAppWindow( 0, 0, lWidth, lHeight, &rcVideoWindow, pVMRWindowlessControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }

        hr = pVMRWindowlessControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );
        // hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
        // CHECKHR( hr, TEXT("SetVideoPosition ...") );

        // Set source and dest rectangle
        RECT *pSrcRect = pTestDesc->GetSrcDestRect();
        RECT *pDestRect = pTestDesc->GetSrcDestRect( false );
        SetRect( pSrcRect, 
                 pSrcRect->left * lWidth/100,
                 pSrcRect->top * lHeight/100,
                 pSrcRect->right * lWidth / 100,
                 pSrcRect->bottom * lHeight / 100 );
        SetRect( pDestRect, 
                 pDestRect->left * lWidth/100,
                 pDestRect->top * lHeight/100,
                 pDestRect->right * lWidth / 100,
                 pDestRect->bottom * lHeight / 100 );

        if ( 0 == pDestRect->left && 0 == pDestRect->top && 0 == pDestRect->right && 0 == pDestRect->bottom )
            pDestRect = NULL;
        if ( 0 == pSrcRect->left && 0 == pSrcRect->top && 0 == pSrcRect->right && 0 == pSrcRect->bottom )
            pSrcRect = NULL;
        if ( pSrcRect || pDestRect )
        {
            hr = pVMRWindowlessControl->SetVideoPosition( pSrcRect, pDestRect );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to set source image rectangle(0x%x)" ), 
                            __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
                goto cleanup;
            }
        }
        else
        {
            hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
            CHECKHR( hr, TEXT("SetVideoPosition ...") );
        }
    }

	// Set alpha, z-order and output rectangles
    hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
    CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

    // Set the parameters for the streams.
    if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
    {
        LOG( TEXT("SetStreamParameters failed.") );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    // Get the max duration of the input vide streams
    // BUGBUG: it seems it just returns the durtion for the last renderer media file.
    LONGLONG llMaxDuration = 0;
    hr = testGraph.GetDuration( &llMaxDuration );
    CHECKHR( hr, TEXT("GetDuration ...") );

    // Run the clip
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, TEXT("Run the graph ...") );

    // If we create the VMR in the main thread, we need to provide a message pump
    DWORD   dwStart = GetTickCount();
    DWORD   dwStop = min( VMR_TEST_DURATION, 2*TICKS_TO_MSEC(llMaxDuration) );
	BOOL    bExit = FALSE;
    while ( GetTickCount() - dwStart < dwStop )
    {
        MSG  msg;
        long evCode;
        LONG_PTR param1;
        LONG_PTR param2;

        while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

		// We need to clean the event queue, otherwise EC_COMPLETE 
        HRESULT hrEvent = pEvent->GetEvent(&evCode, &param1,  &param2, 30);
        if (SUCCEEDED(hrEvent))
        {
            switch (evCode)
            {
                case EC_COMPLETE:
                    LOG ( TEXT("Received EC_COMPLETE!\n") );
					bExit = TRUE;
                    break;
            }
            pEvent->FreeEventParams(evCode, param1, param2);
        }
		if ( bExit )
			break;
    }

    // Verify the stream parameters
    if ( TRUE != VerifyStreamParameters( pVMRMixerControl, pTestDesc ) )
    {
        LOG( TEXT("Verify stream parameters failed.") );
        retval = TPR_FAIL;
    }

cleanup:
    LOG( TEXT("Stop the graph ...") );
    testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

    if ( pVMRWindowlessControl ) 
        pVMRWindowlessControl->Release();

    if ( pVMRMixerControl ) 
        pVMRMixerControl->Release();

	if ( pVMRConfig )
		pVMRConfig->Release();
	
	// Reset the testGraph
	testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

	// Reset the test
    pTestDesc->Reset();

    if ( hwndApp )
        CloseAppWindow(hwndApp);

    return retval;
}