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
//  Video Mixing Renderer Misc Test:
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <atlbase.h>
#include <tux.h>
#include "StringConversions.h"
#include "logging.h"
#include "TuxGraphTest.h"
#include "VMRTestDesc.h"
#include "TestGraph.h"
#include "VMRTest.h"
#include "VideoUtil.h"
#include "BaseCustomAP.h"

static const DWORD WAIT_FOR_STATE = 2000;
// 
// play back multiple video streams using one single VMR in mixing mode.
//
TESTPROCAPI 
WLDeletePlaybackWnd( UINT uMsg, 
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
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IVMRFilterConfig from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}

    // Set the rendering mode
    hr = pVMRConfig->SetRenderingMode( VMRMode_Windowless );
    CHECKHR( hr, TEXT("Setting windowless mode ...") );

    hr = testGraph.FindInterfaceOnGraph( IID_IVMRWindowlessControl, (void **)&pVMRWindowlessControl );
    CHECKHR( hr, TEXT("Query IVMRWindowlessControl interface...") );

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    if ( dwStreams > 1 )
    {
        hr = pVMRConfig->SetNumberOfStreams( dwStreams );
        StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
        CHECKHR( hr, tmp );
    }

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

    // Set alpha, z-order and output rectangles for mixing tests
    if ( dwStreams > 1 )
    {
        hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
        CHECKHR( hr, TEXT("Query IVMRMixerControl interface...") );

        if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
        {
            LOG( TEXT("SetStreamParameters failed.") );
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    hr = testGraph.GetMediaEvent( &pEvent );
    CHECKHR( hr, TEXT("Query IMediaEvent interface ...") );

    // Get the max duration of the input vide streams
    // BUGBUG: it seems it just returns the durtion for the last renderer media file.
    LONGLONG llMaxDuration = 0;
    hr = testGraph.GetDuration( &llMaxDuration );
    CHECKHR( hr, TEXT("GetDuration ...") );

    // Run the clip
    if ( pTestDesc->IsTestStatePresent() )
    {
        hr = testGraph.SetState( pTestDesc->GetTestState(), TestGraph::SYNCHRONOUS );
        StringCchPrintf( tmp, 128, TEXT("Set the graph state: %d"), pTestDesc->GetTestState() );
        CHECKHR( hr, tmp );
    }

    Sleep( WAIT_FOR_STATE );

    // Delete the window handle
    try
    {
        if ( !DestroyWindow( hwndApp ) )
        {
            LOG( TEXT("Destroy the window failed.") );
            retval = TPR_FAIL;
        }
        else
            LOG( TEXT("Destroy the window after setting state %d for %d seconds."), pTestDesc->GetTestState(), WAIT_FOR_STATE/1000 );
    }
    catch( ... )
    {
        LOG( TEXT("Destroying the window after setting state %d for %d seconds caused an exception."), 
                pTestDesc->GetTestState(), WAIT_FOR_STATE/1000 );
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

    if ( FAILED(hr) )
        retval = TPR_FAIL;

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

	// Reset the test
    pTestDesc->Reset();

    if ( IsWindow( hwndApp ) )
        CloseAppWindow(hwndApp);

    return retval;
}
