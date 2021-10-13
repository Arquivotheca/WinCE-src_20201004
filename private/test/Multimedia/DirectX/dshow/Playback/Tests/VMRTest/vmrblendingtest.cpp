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
//  Video Mixing Renderer Blending Test:
//  1. Blending with video streams with native sizes
//  2. Blending with multiple streams and resize/reposition one or more video
//     rectangles.
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <atlbase.h>
#include <tux.h>
#include "resource.h"
#include "StringConversions.h"
#include "logging.h"
#include "TuxGraphTest.h"
#include "VMRTestDesc.h"
#include "TestGraph.h"
#include "VMRTest.h"
#include "VideoUtil.h"
#include "BaseCustomAP.h"

extern SPS_SHELL_INFO   *g_pShellInfo;

// 
// Alpha blend a static bitmap on a video stream.
//
TESTPROCAPI 
VMRBlendStaticBitmapTest( UINT uMsg, 
                          TPPARAM tpParam, 
                          LPFUNCTION_TABLE_ENTRY lpFTE )
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;
	IMediaEvent	  *pEvent = 0x0;
    IVMRFilterConfig		*pVMRConfig = NULL;
    IVMRMixerBitmap		    *pVMRMixerBitmap = NULL;
    IVMRWindowlessControl	*pVMRWindowlessControl = NULL;
    // for set up alpha bitmap
    // CComPtr<IDirectDrawSurface> pDDS;
    IDirectDrawSurface *pDDS = NULL;
    HDC hdcBmp = NULL;
    HBITMAP hbm, hbmOld;
    BITMAP bm;
    HDC hdcDDSurf;
    VMRALPHABITMAP AlphaBitmap;

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
	LOG( TEXT("%S: - adding the video mixing renderer to the graph"), __FUNCTION__ );
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

    // Set the rendering mode
    DWORD dwMode = pTestDesc->GetRenderingMode();
    if ( VMRMode_Windowless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
        CHECKHR( hr, TEXT("Setting windowless mode ...") );

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
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    if ( dwStreams > 1 )
    {
        LOG( TEXT("%S: ERROR %d@%S - Should only have one stream. %d" ), 
                        __FUNCTION__, __LINE__, __FILE__, dwStreams );
        retval = TPR_SKIP;
        goto cleanup;
    }
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), dwStreams );
    CHECKHR( hr, tmp );

    testGraph.SetMedia(pTestDesc->GetMedia(0));

    // Connect to VMR
    hr = testGraph.BuildGraph();
    StringCchPrintf( tmp, 128, TEXT("Rendering media file: %s"), pTestDesc->GetMedia(0)->GetUrl() );
    CHECKHR( hr, tmp );

    // Set owner
    LONG lWidth, lHeight, lARWidth, lARHeight;
    if ( VMRMode_Windowless == dwMode )
    {
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
        hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
        CHECKHR( hr, TEXT("SetVideoPosition ...") );
    }

    // Set the predefined bitmap
	hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerBitmap, (void **)&pVMRMixerBitmap );
    CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

    // Load the multi-image bitmap to alpha blend from the resource file
    hbm = LoadBitmap( g_pShellInfo->hLib, MAKEINTRESOURCE(IDB_TESTALPHABITMAP) );
    if ( !hbm )
    {
        LOG( TEXT("%S: ERROR %d@%S - Can't load the test alpha bitmap" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    GetObject(hbm, sizeof(bm), &bm);
    hdcBmp = CreateCompatibleDC( NULL );
    hbmOld = (HBITMAP)SelectObject( hdcBmp, hbm );

    // set up the bitmap in either the device context or ddraw surface.
    if ( !(pTestDesc->m_AlphaBitmap.dwFlags & VMRBITMAP_HDC) )
    {
        pDDS = CreateDDrawSurface( bm.bmWidth, bm.bmHeight );
        if ( !pDDS )
            hr = E_FAIL;
        CHECKHR( hr, TEXT("Create Direct Draw surface...") );

        pDDS->GetDC( &hdcDDSurf );            
        if ( !BitBlt(hdcDDSurf, 0, 0, bm.bmWidth, bm.bmHeight, hdcBmp, 0, 0, SRCCOPY) )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("BitBlt the test bitmap to the test DDraw surface...") );
        }
        pDDS->ReleaseDC( hdcDDSurf );
    }

    if ( pTestDesc->m_AlphaBitmap.dwFlags & VMRBITMAP_HDC )
        pTestDesc->m_AlphaBitmap.hdc = hdcBmp;

    pTestDesc->m_AlphaBitmap.pDDS = pDDS;
    AlphaBitmap = pTestDesc->m_AlphaBitmap;

    hr = pVMRMixerBitmap->SetAlphaBitmap( &AlphaBitmap );
    CHECKHR( hr, TEXT("SetAlphaBitmap...") );

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

    // Check the returned event code to make sure the operation completed .
    // LONG evCode = 0;
    // hr = testGraph.WaitForCompletion( 2*TICKS_TO_MSEC(llMaxDuration), &evCode );
    // CHECKHR( hr, TEXT("WaitForCompletion ...") );

    // BUGBUG: even for compatibility mode, we have to provide a message pump?
    // BUGBUG: it seems if the VMR is picked up by the intelligent connect such as RenderFile, then
    // BUGBUG: the test does not need to provide a message pump.
    DWORD   dwStart = GetTickCount();
    DWORD   dwStop = min( VMR_TEST_DURATION, 2*TICKS_TO_MSEC(llMaxDuration) );
    MSG  msg;
    long evCode;
    LONG_PTR param1, param2;
	BOOL bExit = FALSE;

    while ( GetTickCount() - dwStart < dwStop )
    {
        while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

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

        // TODO: dynamically change the bitmap (rSrc, rDest, fAlpha, clrSrcKey, but not hdc/pDDS)
        InitAlphaParams( &AlphaBitmap, 
                         pTestDesc->m_AlphaBitmap.dwFlags, 
                         pTestDesc->m_AlphaBitmap.hdc, 
                         pTestDesc->m_AlphaBitmap.pDDS, 
                         &pTestDesc->m_AlphaBitmap.rSrc, 
                         &pTestDesc->m_AlphaBitmap.rDest, 
                         0.5f,
                         pTestDesc->m_AlphaBitmap.clrSrcKey );

        hr = pVMRMixerBitmap->UpdateAlphaBitmapParameters( &AlphaBitmap );
    }

cleanup:
    LOG( TEXT("Stop the graph ...") );
    testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

    if ( pVMRWindowlessControl ) 
        pVMRWindowlessControl->Release();

    if ( pVMRMixerBitmap ) 
        pVMRMixerBitmap->Release();

    if ( pVMRConfig )
        pVMRConfig->Release();

	// Reset the testGraph
	testGraph.Reset();

    // Clean up GDI resources
    DeleteObject( SelectObject(hdcBmp, hbmOld) );
    DeleteDC( hdcBmp );

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the test
    pTestDesc->Reset();

    if ( hwndApp )
        CloseAppWindow(hwndApp);

    return retval;
}

// 
// play back multiple video streams using one single VMR in mixing mode.
//
TESTPROCAPI 
VMRBlendVideoStreamTest( UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE )
{
	DWORD retval = TPR_PASS;
	HRESULT hr = S_OK;
	IMediaEvent	  *pEvent = 0x0;
    IVMRFilterConfig		*pVMRConfig = NULL;
    IVMRMixerControl		*pVMRMixerControl = NULL;
    IVMRWindowlessControl	*pVMRWindowlessControl = NULL;
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
	LOG( TEXT("%S: - adding the video mixing renderer to the graph"), __FUNCTION__ );
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
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}

    // Set the rendering mode
    DWORD dwMode = pTestDesc->GetRenderingMode();
    if ( VMRMode_Windowless == dwMode )
    {
        hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
        CHECKHR( hr, TEXT("Setting windowless mode ...") );

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
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
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
        hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
        CHECKHR( hr, TEXT("SetVideoPosition ...") );
    }

	// Set alpha, z-order and output rectangles
	hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
    CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

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

    // Check the returned event code to make sure the operation completed .
    // LONG evCode = 0;
    // hr = testGraph.WaitForCompletion( 2*TICKS_TO_MSEC(llMaxDuration), &evCode );
    // CHECKHR( hr, TEXT("WaitForCompletion ...") );

    // BUGBUG: even for compatibility mode, we have to provide a message pump?
    // BUGBUG: it seems if the VMR is picked up by the intelligent connect such as RenderFile, then
    // BUGBUG: the test does not need to provide a message pump.
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

//
// Callback to verify the configuration parameters 
//
HRESULT 
VerifyStreamComposition( IDirectDrawSurface *pSurface, void *userData )
{
    HRESULT hr = S_OK;
    CheckPointer( userData, E_POINTER );

    return hr;
}

// 
// play back multiple video streams using one single VMR in renderless mode using a custom
// allocator presenter.  Since the custom ap has to be set before the filter is connected, testing
// it in the DRM chamber is very difficult.  Therefore, image verification will be done through
// manula tests.
//
TESTPROCAPI 
VMRBlendUseCustomAPTest( UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR = 0x0;
    IMediaEvent      *pEvent = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRMixerControl        *pVMRMixerControl = NULL;
    IVMRWindowlessControl    *pVMRWindowlessControl = NULL;
    HWND    hwndApp = NULL;
    TCHAR tmp[128] = {0};
    CVMRE2ECustomAP    *pCustomAP = NULL;
    VMRStreamData    streamData;

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

    pCustomAP = new CVMRE2ECustomAP( &hr );
    if ( !pCustomAP )
       hr = E_OUTOFMEMORY;
    
    CHECKHR( hr, TEXT("Creating the custom allocator presenter ...") );

    pCustomAP->AddRef();

    // Set up the custom AP based on the config
    if ( pTestDesc->m_vmrTestMode == IMG_SAVE_MODE ||pTestDesc->m_vmrTestMode == IMG_VERIFICATION_MODE )
    {
        pCustomAP->SaveImage( TRUE );
        //set where to save images
        pCustomAP->SetImagePath( pTestDesc->szDestImgPath );
    }
    else if ( pTestDesc->m_vmrTestMode == IMG_STM_VERIFICATION_MODE )
    {
        //turn on verification thread
        pCustomAP->VerifyStream( TRUE );
        //set where to find source images
        pCustomAP->SetImagePath( pTestDesc->szSrcImgPath );
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
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
    CHECKHR( hr, tmp );

    streamData.vmrId = DW_COMPOSITED_ID;
    // The rendering mode will be set to renderless inside
    hr = pCustomAP->Activate(pVMR, NULL, &streamData );
    CHECKHR( hr, TEXT("Activate the custom allocator presenter.") );

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

    hr = testGraph.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    // Get the max duration of the input vide streams
    LONGLONG llMaxDuration = 0;
    hr = testGraph.GetDuration( &llMaxDuration );

    // Run the clip
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, TEXT("Run the graph ...") );

    // Check the returned event code to make sure the operation completed .
    //LONG evCode = 0;
    //hr = testGraph.WaitForCompletion( 2*TICKS_TO_MSEC(llMaxDuration), &evCode );
    //CHECKHR( hr, TEXT("WaitForCompletion ...") );

    DWORD   dwStart = GetTickCount();
    DWORD   dwStop = min( VMR_TEST_DURATION, 2*TICKS_TO_MSEC(llMaxDuration) );
    while ( GetTickCount() - dwStart < dwStop )
    {
        MSG  msg;
        while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // If in image stream comparision verification mode, check verification result
    DWORD dwPassCount = 0;
    if( pTestDesc->m_vmrTestMode == IMG_STM_VERIFICATION_MODE )
    {
        dwPassCount = pCustomAP->GetMatchCount();
        if( dwPassCount >= pTestDesc->m_dwPassRate )
            retval = TPR_FAIL;
    }

    // If in images comparision mode, compare images
    if( pTestDesc->m_vmrTestMode == IMG_VERIFICATION_MODE )
    {
        dwPassCount = pCustomAP->CompareSavedImages( pTestDesc->szSrcImgPath );
        LOG( TEXT("Image Comparision: %d images match and test configure pass rate is %d!"), dwPassCount,  pTestDesc->m_dwPassRate );

        if( dwPassCount < pTestDesc->m_dwPassRate )
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

    testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

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


    // Disconnect the VMR before releasing custom AP
    /*
    HRESULT hrTmp = testGraph.DisconnectFilter( TEXT("VideoMixingRenderer") );
    if ( FAILED(hrTmp) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Disconnecting the VMR failed.(0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hrTmp );
    }
    */

    if ( pVMR ) 
        pVMR->Release();
    
    // Reset the testGraph
    testGraph.Reset();

    if ( pCustomAP )
        pCustomAP->Release();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
	// Reset the test
    pTestDesc->Reset();

    return retval;
}


// 
// play back multiple video streams using multiple VMR in renderless mode using one single custom
// allocator presenter.
//
TESTPROCAPI 
VMRBlendUseCustomAPMultiGraphTest( UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBaseFilter   *pVMR1 = 0x0;
    IBaseFilter   *pVMR2 = 0x0;
    IBaseFilter   *pVMRComp = 0x0;
    IMediaEvent      *pEvent = 0x0;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRMixerControl        *pVMRMixerControl = NULL;
    IVMRWindowlessControl    *pVMRWindowlessControl = NULL;
    IVideoWindow            *pVideoWindow = NULL;
    IBasicVideo            *pBasicVideo = NULL;
    HWND    hwndApp = NULL;
    TCHAR tmp[128] = {0};
    CVMRE2ECustomAP    *pCustomAP = NULL;
    VMRStreamData    streamData[3];

    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    VMRTestDesc* pTestDesc = (VMRTestDesc*)lpFTE->dwUserData;

    TestGraph testGraph1;
    TestGraph testGraph2;
    TestGraph testGraphComp;
    if ( pTestDesc->RemoteGB() )
    {
        testGraph1.UseRemoteGB( TRUE );
        testGraph2.UseRemoteGB( TRUE );
        testGraphComp.UseRemoteGB( TRUE );
    }

    if ( pTestDesc->MixingMode() )
        testGraphComp.UseVMR( TRUE );

    // Build an empty graph
    hr = testGraph1.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = testGraph2.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = testGraphComp.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    pCustomAP = new CVMRE2ECustomAP( &hr );
    if ( !pCustomAP )
       hr = E_OUTOFMEMORY;

    CHECKHR( hr, TEXT("Creating the custom allocator presenter ...") );

    pCustomAP->AddRef();

    // enable the blending verificaiton
    pCustomAP->VerifyStream( TRUE );
    pCustomAP->RuntimeVerify( TRUE );

    // Add the VMR manually
    // ZeroMemory( streamData, 3 * sizeof( VMRStreamData ) );
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();

    // Add the third VMR to the graph
    LOG( TEXT("%S: - adding the first video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraphComp.AddFilter( VideoMixingRenderer );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    pVMRComp = testGraphComp.GetFilter( VideoMixingRenderer );

    if ( !pVMRComp )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pVMRComp->QueryInterface( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface") );

    // Load the mixer
    dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
    CHECKHR( hr, tmp );

    streamData[2].vmrId = DW_COMPOSITED_ID;
    hr = pCustomAP->Activate(pVMRComp, NULL, &streamData[2] );
    CHECKHR( hr, TEXT("Activate the custom allocator presenter.") );

     // Set the rendering mode
    DWORD dwMode = pTestDesc->GetRenderingMode();
    if ( VMRMode_Windowless == dwMode || VMRMode_Renderless == dwMode )
    {
        // Set windowless parameters
        hr = pVMRComp->QueryInterface( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
        CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

        hr = pVMRWindowlessControl->SetBorderColor(RGB(0,0,0));
        CHECKHR( hr, TEXT("SetBorderColor as black...") );
    }
    else 
    {
        hr = pVMRComp->QueryInterface( IID_IVideoWindow, (void **)&pVideoWindow );
        CHECKHR( hr, TEXT("Query IVideoWindow interface ...") );

        hr = pVMRComp->QueryInterface( IID_IBasicVideo, (void **)&pBasicVideo );
        CHECKHR( hr, TEXT("Query IVideoWindow interface ...") );
    }

    // Connect to VMR
    for ( DWORD i = 0; i < dwStreams; i++ )
    {
   		LOG( TEXT("Rendering media file: %s"), pTestDesc->GetMedia(i)->GetUrl() );
        testGraphComp.SetMedia(pTestDesc->GetMedia(i));
        if(FAILED(hr = testGraphComp.BuildGraph()))
        {
            LOG( TEXT("%S: ERROR %d@%S - Build graph failed. 0x%08x"),__FUNCTION__, __LINE__, __FILE__, hr);
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

    LONG lWidth = 0, lHeight = 0, lARWidth = 0, lARHeight = 0;
    if ( VMRMode_Windowless == dwMode || VMRMode_Renderless == dwMode )
    {
        hr = pVMRWindowlessControl->GetNativeVideoSize( &lWidth, &lHeight, &lARWidth, &lARHeight );
        RECT rcVideoWindow;
        hwndApp = OpenAppWindow( 0, 0, lWidth, lHeight, &rcVideoWindow, pVMRWindowlessControl );
        if ( !hwndApp )
        {
            hr = E_FAIL;
            CHECKHR( hr, TEXT("Create an application window...") );
        }

        hr = pVMRWindowlessControl->SetVideoClippingWindow(hwndApp);
        CHECKHR( hr, TEXT("SetVideoClippingWindow ...") );
        hr = pVMRWindowlessControl->SetVideoPosition(NULL, &rcVideoWindow);
        CHECKHR( hr, TEXT("SetVideoPosition ...") );
    }
    else if ( VMRMode_Windowed == dwMode )
    {
        if ( hwndApp ) 
        {
            hr = pVideoWindow->put_Owner((OAHWND)hwndApp);
            CHECKHR( hr, TEXT("Put owner of the video window...") );
        }
        hr = pBasicVideo->GetVideoSize( &lWidth, &lHeight );
        CHECKHR( hr, TEXT("Retrieving the video size...") );
    }

    // Set the composited video size for the ap
    pCustomAP->SetCompositedSize( abs(lWidth), abs(lHeight) );

    // Set alpha, z-order and output rectangles
    hr = pVMRComp->QueryInterface( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
    CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

    // Set stream parameters
    if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
    {
        LOG( TEXT("Set stream parameters failed.") );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraphComp.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    // Add the first VMR into the graph
    LOG( TEXT("%S: - adding the first video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph1.AddFilter( VideoMixingRenderer );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    pVMR1 = testGraph1.GetFilter( VideoMixingRenderer );

    if ( !pVMR1 )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    streamData[0].vmrId = DW_STREAM1_ID;
    streamData[0].fAlpha = pTestDesc->m_StreamInfo[0].fAlpha;
    streamData[0].iZorder = pTestDesc->m_StreamInfo[0].iZOrder;
    streamData[0].nrRect.left = pTestDesc->m_StreamInfo[0].nrOutput.left;
    streamData[0].nrRect.right = pTestDesc->m_StreamInfo[0].nrOutput.right;
    streamData[0].nrRect.bottom = pTestDesc->m_StreamInfo[0].nrOutput.bottom;
    streamData[0].nrRect.top = pTestDesc->m_StreamInfo[0].nrOutput.top;

    // -- Activate will set the VMR into the renderless mode
    hr = pCustomAP->Activate(pVMR1, NULL, &streamData[0] );
    LOG( TEXT("Activate the custom allocator presenter.") );

    testGraph1.SetMedia(pTestDesc->GetMedia(0));

    hr = testGraph1.BuildGraph();
    CHECKHR( hr, TEXT("Rendering the file ...") );


    // Add the second VMR to the graph
    LOG( TEXT("%S: - adding the second video mixing renderer to the graph"), __FUNCTION__ );
    hr = testGraph2.AddFilter( VideoMixingRenderer );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    pVMR2 = testGraph2.GetFilter( VideoMixingRenderer );

    if ( !pVMR2 )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the VMR from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    streamData[1].vmrId = DW_STREAM2_ID;
    streamData[1].fAlpha = pTestDesc->m_StreamInfo[1].fAlpha;
    streamData[1].iZorder = pTestDesc->m_StreamInfo[1].iZOrder;
    streamData[1].nrRect.left = pTestDesc->m_StreamInfo[1].nrOutput.left;
    streamData[1].nrRect.right = pTestDesc->m_StreamInfo[1].nrOutput.right;
    streamData[1].nrRect.bottom = pTestDesc->m_StreamInfo[1].nrOutput.bottom;
    streamData[1].nrRect.top = pTestDesc->m_StreamInfo[1].nrOutput.top;

    hr = pCustomAP->Activate(pVMR2, NULL, &streamData[1] );
    LOG( TEXT("Activate the custom allocator presenter.") );

    testGraph2.SetMedia(pTestDesc->GetMedia(1));

    hr = testGraph2.BuildGraph();
    CHECKHR( hr, TEXT("Rendering the file ...") );


    // Get the max duration of the input vide streams
    LONGLONG llMaxDuration = 0, llTmp;
    hr = testGraph1.GetDuration( &llTmp );
    llMaxDuration = llTmp;
    hr = testGraph2.GetDuration( &llTmp );
    if ( llTmp > llMaxDuration ) llMaxDuration = llTmp;
    CHECKHR( hr, TEXT("GetDuration ...") );

    // Run the clip
    hr = testGraph1.SetState( State_Running, TestGraph::SYNCHRONOUS );
    hr = testGraph2.SetState( State_Running, TestGraph::SYNCHRONOUS );
    hr = testGraphComp.SetState( State_Running, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, TEXT("Run the graph ...") );


    // Check the returned event code to make sure the operation completed .
    //LONG evCode = 0;
    //hr = testGraph.WaitForCompletion( 2*TICKS_TO_MSEC(llMaxDuration), &evCode );
    //CHECKHR( hr, TEXT("WaitForCompletion ...") );

    DWORD   dwStart = GetTickCount();
    DWORD   dwStop = min( VMR_TEST_DURATION, 2*TICKS_TO_MSEC(llMaxDuration) );
    while ( GetTickCount() - dwStart < dwStop )
    {
        MSG  msg;

        while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

cleanup:

    testGraph1.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    testGraph2.SetState( State_Stopped, TestGraph::SYNCHRONOUS );
    testGraphComp.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

    if ( pCustomAP )
        pCustomAP->Deactivate();

    if ( hwndApp )
        CloseAppWindow(hwndApp);

    if ( pVideoWindow )
        pVideoWindow->Release();

    if ( pBasicVideo )
        pBasicVideo->Release();

    if ( pVMRWindowlessControl ) 
        pVMRWindowlessControl->Release();

    if ( pVMRMixerControl ) 
        pVMRMixerControl->Release();

    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( pVMR1 ) 
        pVMR1->Release();
    if ( pVMR2 ) 
        pVMR2->Release();
    if ( pVMRComp )
        pVMRComp->Release();
    
    // Reset the testGraph
    testGraph1.Reset();
    testGraph2.Reset();
    testGraphComp.Reset();

    if ( pCustomAP )
        pCustomAP->Release();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
}


// 
// play back multiple video streams using dshow interfaces directly.
//
TESTPROCAPI 
VMRSimpleMixingTest( UINT uMsg, 
                     TPPARAM tpParam, 
                     LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IUnknown  *pUnk = NULL;
    IFilterGraph *pFG = NULL;
    IGraphBuilder *pGB = NULL;
    IBaseFilter   *pVMR = 0x0;
    IMediaControl      *pMC = 0x0;
    IMediaEvent      *pME = 0x0;
    IMediaSeeking      *pMS = 0x0;
    IVMRFilterConfig* pConfig = NULL;
    IVMRMixerControl        *pVMRMixerControl = NULL;
    TCHAR tmp[128] = {0};

    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    VMRTestDesc* pTestDesc = (VMRTestDesc*)lpFTE->dwUserData;

    // Initialize COM
    hr = CoInitializeEx( NULL, COINIT_MULTITHREADED );
    CHECKHR( hr, TEXT("Initializing COM...") );

    hr = CoCreateInstance( CLSID_FilterGraph,
                           NULL, CLSCTX_INPROC,
                           IID_IUnknown, (LPVOID *)&pUnk );
    CHECKHR( hr, TEXT("Create FGM...") );

    hr = pUnk->QueryInterface( IID_IFilterGraph, (LPVOID *)&pFG );
    CHECKHR( hr, TEXT("Query IFilterGraph interface ...") );

    hr = CoCreateInstance( CLSID_VideoMixingRenderer,
                           NULL, CLSCTX_INPROC,
                           IID_IBaseFilter,
                           (LPVOID *)&pVMR );
    CHECKHR( hr, TEXT("Create VMR ...") );

    hr = pFG->AddFilter( pVMR, L"Video Mixing Renderer" );
    CHECKHR( hr, TEXT("Adding VMR into the graph ...") );

    hr = pVMR->QueryInterface( IID_IVMRFilterConfig, (LPVOID *)&pConfig );
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface ...") );
    
    hr = pConfig->SetRenderingMode( VMRMode_Windowed );
    CHECKHR( hr, TEXT("Setting rendering mode: windowed ...") );

    hr = pConfig->SetNumberOfStreams( 2 );
    CHECKHR( hr, TEXT("SetNumberOfStreams w/ 2 ...") );

    hr = pVMR->QueryInterface( IID_IVMRMixerControl, (LPVOID *)&pVMRMixerControl );
    CHECKHR( hr, TEXT("Query IVMRMixerControl interface ...") );

    hr = pUnk->QueryInterface( IID_IGraphBuilder, (LPVOID *)&pGB );
    CHECKHR( hr, TEXT("Query IGraphBuilder interface ...") );

    WCHAR wszUrl[MAX_PATH];
    TCharToWChar( pTestDesc->GetMedia( 0 )->GetUrl(), wszUrl, countof(wszUrl) );
    hr = pGB->RenderFile( wszUrl, NULL );
    StringCchPrintf( tmp, 128, TEXT("Rendering %s ..."), pTestDesc->GetMedia( 0 )->GetUrl() );
    CHECKHR( hr, tmp );
   
    ZeroMemory( wszUrl, MAX_PATH );
    TCharToWChar( pTestDesc->GetMedia( 1 )->GetUrl(), wszUrl, countof(wszUrl) );
    hr = pGB->RenderFile( wszUrl, NULL );
    StringCchPrintf( tmp, 128, TEXT("Rendering %s ..."), pTestDesc->GetMedia( 1 )->GetUrl() );
    CHECKHR( hr, tmp );

    // set up the mixing and alpha parameters
    if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
    {
        LOG( TEXT("SetStreamParameters failed.") );
        hr = E_FAIL;
        goto cleanup;
    }

    hr = pUnk->QueryInterface( IID_IMediaControl, (LPVOID *)&pMC );
    CHECKHR( hr, TEXT("Query IMediaControl interface ...") );

    hr = pUnk->QueryInterface( IID_IMediaEvent, (LPVOID *)&pME );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    hr = pUnk->QueryInterface( IID_IMediaSeeking, (LPVOID *)&pMS );
    CHECKHR( hr, TEXT("Query IMediaSeeking ...") );

    // Get the max duration of the input vide streams
    LONGLONG llMaxDuration = 0;
    hr = pMS->GetDuration( &llMaxDuration );
    CHECKHR( hr, TEXT("Get Duration of the playback ...") );

    // Run the clip
    hr = pMC->Run();
    CHECKHR( hr, TEXT("Run the graph ...") );

    DWORD   dwStart = GetTickCount();
    DWORD   dwStop = min( VMR_TEST_DURATION, 2*TICKS_TO_MSEC(llMaxDuration) );
    while ( GetTickCount() - dwStart < dwStop )
    {
        MSG  msg;
        while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    hr = pMC->Stop();
    CHECKHR( hr, TEXT("Stop the graph ...") );

cleanup:

    SAFERELEASE( pUnk );
    SAFERELEASE( pFG );
    SAFERELEASE( pGB );
    SAFERELEASE( pVMR );
    SAFERELEASE( pMC );
    SAFERELEASE( pME );
    SAFERELEASE( pMS );
    SAFERELEASE( pConfig );
    SAFERELEASE( pVMRMixerControl );

    if ( FAILED(hr) )
        retval = TPR_FAIL;

	// Readjust return value if HRESULT matches/differs from expected in some instances
	retval = pTestDesc->AdjustTestResult(hr, retval);

    CoUninitialize();

    return retval;
}