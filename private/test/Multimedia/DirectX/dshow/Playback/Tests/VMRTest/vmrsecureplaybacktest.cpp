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
//  VMR tests in normal and secure chamber rendering mix of non drm and drm files
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <atlbase.h>
#include <tux.h>
#include "TuxGraphTest.h"
#include "logging.h"
#include "StringConversions.h"
#include "TestGraph.h"
#include "VMRTest.h"
#include "VideoUtil.h"
#include "VMRTestDesc.h"

// 
// play back multiple video streams using one single VMR in mixing mode.
//
TESTPROCAPI 
RestrictedGB_VMR_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IVMRMixerControl        *pVMRMixerControl = NULL;
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
    WCHAR   wszFilterName[MAX_PATH] = L"VideoMixingRenderer";
    // StringCchCopyW( wszFilterName, MAX_PATH, L"VideoMixingRenderer" );
    // Remote graph version of AddFilter
	hr = testGraph.AddFilterByCLSID( CLSID_VideoMixingRenderer, wszFilterName );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%x)" ), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

	hr = testGraph.FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pVMRConfig );
    CHECKHR( hr, TEXT("Query IVMRFilterConfig interface ...") );

    // Use windowed mode.  Now the VMR will create its own message pump once connected.

    // Load the mixer
    DWORD dwStreams = pTestDesc->GetNumberOfStreams();
    hr = pVMRConfig->SetNumberOfStreams( dwStreams );
    StringCchPrintf( tmp, 128, TEXT("SetNumberofStreams with %d"), pTestDesc->GetNumberOfStreams() );
    CHECKHR( hr, tmp );

    // Retrieve IVMRMixerControl interface
	hr = testGraph.FindInterfaceOnGraph( IID_IVMRMixerControl, (void **)&pVMRMixerControl );
    CHECKHR( hr, TEXT("Query IVMRMixerControl interface using remote graph builder") );

    // Some mixing scenarios:
    // 1. With CLSID_FilterGraph, attempt to play the mixing of DRM and non-DRM in the normal chamber will return E_NOTIMPL (?)
    // 2. With CLSID_FilterGraph, play the mix of DRM and non-DRM should work in the secure chamber and have full funtionality.
    // 3. With CLSID_RestrictedFilterGraph, mix of DRM and non-DRM can be played in both normal and secure chamber 
    //    with limited functionality.
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
            if ( hr == pTestDesc->TestExpectedHR() )
                hr = S_OK;
            else
            {
                LOG( TEXT("%S: ERROR %d@%S - Rendering media file: %s failed. (0x%08x)"), 
                                __FUNCTION__, __LINE__, __FILE__, pTestDesc->GetMedia(i)->GetUrl(), hr );
                retval = TPR_FAIL;
                goto cleanup;
            }
        }
    }

    // Set alpha, z-order and output rectangles
    if ( TRUE != SetStreamParameters( pVMRMixerControl, pTestDesc ) )
    {
        LOG( TEXT("SetStreamParameters failed.") );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Run the clip
    hr = testGraph.SetState( State_Running, TestGraph::SYNCHRONOUS );
    CHECKHR( hr, TEXT("Run the graph ...") );

    // Check the returned event code to make sure the operation completed .
    LONG evCode = 0;
	hr = testGraph.WaitForCompletion( INFINITE, &evCode );
    CHECKHR( hr, TEXT("WaitForCompletion ...") );
    // Sleep( 10000 );

    // Verify the stream parameters
    if ( TRUE != VerifyStreamParameters( pVMRMixerControl, pTestDesc ) )
    {
        LOG( TEXT("Verify stream parameters failed.") );
        retval = TPR_FAIL;
    }

cleanup:
    LOG( TEXT("Stop the graph ...") );
    testGraph.SetState( State_Stopped, TestGraph::SYNCHRONOUS );

    if ( pVMRMixerControl ) 
        pVMRMixerControl->Release();

    if ( pVMRConfig )
        pVMRConfig->Release();

    if ( FAILED(hr) )
        retval = TPR_FAIL;

	// Readjust return value if HRESULT matches/differs from expected in some instances
	retval = pTestDesc->AdjustTestResult(hr, retval);
    
    // Reset the testGraph
    testGraph.Reset();

    // Reset the test
    pTestDesc->Reset();

    return retval;
}