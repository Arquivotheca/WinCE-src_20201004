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
//  Video Renderer Graph Tests
//
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "VMRTestDesc.h"
#include "TestGraph.h"
#include "VMRTest.h"
#include "utility.h"
#include "TestWindow.h"

#define MIN_TIME_BETWEEN_WINDOW_OPERATION 10000
#define MAX_TIME_BETWEEN_WINDOW_OPERATION 10000
#define NUM_RENDER_MODE_SWITCHES 10


HRESULT PrepVMRTest( TestGraph* pTestGraph, VMRTestDesc* pTestDesc )
{
	HRESULT hr = S_OK;
	IVMRFilterConfig *pConfig = 0x0;

    if ( pTestDesc->IsUsingVR() ) return E_INVALIDARG;

	LOG(TEXT("%S: - adding the video mixing renderer to the graph"), __FUNCTION__);
    hr =pTestGraph->AddFilterByCLSID( CLSID_VideoMixingRenderer, L"VideoMixingRenderer" ); 
	if ( FAILED(hr) )
	{
		LOG(TEXT("%S: ERROR %d@%S - failed to add the video mixing renderer (0x%08x)"), 
					__FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;
	}

    hr = pTestGraph->FindInterfaceOnGraph( IID_IVMRFilterConfig, (void **)&pConfig );
	if ( FAILED(hr) || !pConfig )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to find IVMRFilterConfig (0x%08x)" ), 
						__FUNCTION__, __LINE__, __FILE__, hr );
		goto cleanup;
	}

	hr = pConfig->SetRenderingMode( VMRMode_Windowed );
	if ( FAILED(hr) )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to set the windowed mode. (0x%08x)" ), 
						__FUNCTION__, __LINE__, __FILE__, hr );
		goto cleanup;
	}

	hr = pConfig->SetRenderingPrefs( RenderPrefs_AllowOverlays | RenderPrefs_AllowOffscreen );
	if ( FAILED(hr) )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to set the rendering prefs. (0x%08x)" ), 
						__FUNCTION__, __LINE__, __FILE__, hr );
		goto cleanup;
	}

    LOG(TEXT("%S: INFO %d@%S - The default AP is created and the VMR is initialized"), 
                __FUNCTION__, __LINE__, __FILE__);

cleanup:
	if ( pConfig ) pConfig->Release();

    return hr;
}


TESTPROCAPI VMR_Run_Pause_OccludingWindow_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    VMRTestDesc* pTestDesc = (VMRTestDesc*)lpFTE->dwUserData;

    // From the config, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );
    
    // Set the media for the graph
    hr = testGraph.SetMedia(pMedia);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to set media %s with the test graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }

    // Build an empty graph
    if (retval == TPR_PASS)
    {
        hr = testGraph.BuildEmptyGraph();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    // Add the VR and configure it according to the test desc
    if (retval == TPR_PASS)
    {
        hr = PrepVMRTest(&testGraph, pTestDesc);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to build graph with the video renderer for %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            retval = TPR_FAIL;
        }
    }

    // Enable the verification required - just before setting the state of the graph
    bool bGraphVerification = false;
    if (retval == TPR_PASS)
    {
        VerificationList verificationList = pTestDesc->GetVerificationList();
        VerificationList::iterator iterator = verificationList.begin();
        while (iterator != verificationList.end())
        {
            VerificationType type = (VerificationType)iterator->first;
            if (testGraph.IsCapable(type)) {
                hr = testGraph.EnableVerification(type, iterator->second);
                if (FAILED_F(hr))
                {
                    // BUGBUG: if it's not implemented, don't treat it as a failure.
                    if ( E_NOTIMPL == hr ) 
                    {
                        LOG( TEXT("%S: WARN %d@%S - Verification (%S) is not implemented!"), 
                                    __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type) );
                    }
                    else
                    {
                        LOG( TEXT("%S: ERROR %d@%S - failed to enable verification %S)"), 
                                    __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type) );
                        retval = TPR_FAIL;
                    }
                    break;
                }
                bGraphVerification = true;
            }
            else {
                LOG(TEXT("%S: WARNING %d@%S - unrecognized verification requested %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
            }
            iterator++;
        }
    }

    // Build the graph
    if (retval == TPR_PASS)
    {
        hr = testGraph.BuildGraph();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to build graph with the video renderer for %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: successfully built graph with the video renderer for %s"), __FUNCTION__, pMedia->GetUrl());
        }
    }

    // BUGBUG: Set the source and dest rectangles if any
    long videoWidth = 0;
    long videoHeight = 0;
    if (retval == TPR_PASS)
    {
        hr = testGraph.GetVideoWidth(&videoWidth);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get video width (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }

        hr = testGraph.GetVideoHeight(&videoHeight);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get video height (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }

        if (SUCCEEDED(hr))
        {
            if ( pTestDesc->IsSrcRectPresent() )
            {
                RECT *pSrcRect = pTestDesc->GetSrcDestRect();
                hr = testGraph.SetSourceRect(pSrcRect->left*videoWidth/100,
                                             pSrcRect->top*videoHeight/100,
                                            ( pSrcRect->right - pSrcRect->left)*videoWidth/100,
                                            ( pSrcRect->bottom - pSrcRect->top)*videoHeight/100);
            }

            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to set source image rectangle(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
            }
            else if ( pTestDesc->IsDestRectPresent() )
            {
                RECT *pDestRect = pTestDesc->GetSrcDestRect( false );
                hr = testGraph.SetDestinationRect(pDestRect->left*videoWidth/100,
                                            pDestRect->top*videoHeight/100,
                                            ( pDestRect->right - pDestRect->left)*videoWidth/100,
                                            ( pDestRect->bottom - pDestRect->top)*videoHeight/100);
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to set destination image rectangle(0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                    retval = TPR_FAIL;
                }
            }
        }
    }

    // Get the screen resolution
    DWORD screenWidth = 0;
    DWORD screenHeight = 0;
    if (retval == TPR_PASS)
    {
        hr = GetScreenResolution(&screenWidth, &screenHeight);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get screen resolution (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        }
    }

    // Get the duration
    LONGLONG duration = 0;
    if (retval == TPR_PASS)
    {
        hr = testGraph.GetDuration(&duration);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: WARNING %d@%S - failed to get duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        }        
    }

    // Set the graph to run
    if (retval == TPR_PASS)
    {
        hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to run graph"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
    }

    // BUGBUG: sleep for some time
    Sleep( 5000 );

    // Set the graph to paused
    if (retval == TPR_PASS)
    {
        hr = testGraph.SetState(State_Paused, TestGraph::SYNCHRONOUS);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to pause graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
    }

    // Create the occluding window object)
    TestWindow testWindow;
    if (retval == TPR_PASS)
    {
        hr = testWindow.Init();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - creating occluding window failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    // Signal the start of verification
    if ((retval == TPR_PASS) && bGraphVerification)
    {
        hr = testGraph.StartVerification();
        if (FAILED_F(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    // The actual test
    if (retval == TPR_PASS)
    {
        // The rect list is a list of percentages reflecting the position and size of the occluding window
        WndRectList *pWndRectList = pTestDesc->GetTestWinPositions();

        // The position list is a list of percentages reflecting the position of the video window
        WndPosList *pWndPosList = pTestDesc->GetVWPositions();

        for(int i = 0; (i < pWndRectList->size()) && (retval == TPR_PASS); i++)
        {
            // Set the window position requested after converting to actual position
            // Only set if index is in range
            if (i < pWndPosList->size())
            {
                WndPos wndPos = (*pWndPosList)[i];
                wndPos.x = wndPos.x*screenWidth/100;
                wndPos.y = wndPos.y*screenHeight/100;
                
                // Move the window to the new position
                LOG(TEXT("%S: moving video window to (X:%d, Y:%d)"), __FUNCTION__, wndPos.x, wndPos.y);

                hr = testGraph.SetVideoWindowPosition(wndPos.x, wndPos.y, videoWidth, videoHeight);
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - setting video window position failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                }

                // Reset the verification, so we don't count the repaints caused during the move
                if (retval == TPR_PASS)
                    testGraph.ResetVerification();
            }

            // BUGBUG: sleep some time
            Sleep(5000);

            // Get the window rectangle requested - converting to actual position from percentages
            WndRect wndRect = (*pWndRectList)[i];
            wndRect.top = wndRect.top*screenHeight/100;
            wndRect.bottom = wndRect.bottom*screenHeight/100;
            wndRect.left = wndRect.left*screenWidth/100;
            wndRect.right = wndRect.right*screenWidth/100;
        
            // Set the occluding window to cover the requested area
            hr = testWindow.SetClientRect(&wndRect);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to set the occluding window position and size (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
            }
            else {
                testWindow.ShowWindow();
            }

            // Sleep for some time
            Sleep(5000);

            // Now minimize the occluding window - this should generate a repaint
            hr = testWindow.Minimize();
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to minimize the occluding window (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
            }

            LOG(TEXT("%S: minimized test window"), __FUNCTION__);

            // Sleep some more time
            Sleep( 5000 );

            // Check the results of the verification
            if (retval == TPR_PASS)
            {
                if (bGraphVerification && (retval == TPR_PASS))
                {
                    hr = testGraph.GetVerificationResults();
                    if (FAILED_F(hr))
                    {
                        LOG(TEXT("%S: ERROR %d@%S - verification failed"), __FUNCTION__, __LINE__, __FILE__);
                        retval = TPR_FAIL;
                    }
                    else {
                        LOG(TEXT("%S: verification succeeded"), __FUNCTION__);
                    }

                    // Reset the verification
                    if (retval == TPR_PASS)
                        testGraph.ResetVerification();
                }
            }

        }
    }

    // Stop playback
    if (retval == TPR_PASS)
    {
        hr = testGraph.SetState(State_Stopped, TestGraph::SYNCHRONOUS);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to stop graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

	// Readjust return value if HRESULT matches/differs from expected in some instances
	retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the test window
    testWindow.Reset();

    // Reset the testGraph
    testGraph.Reset();

    // Reset the test
    pTestDesc->Reset();

    return retval;
}


TESTPROCAPI 
VMR_Run_UntilKilled_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    VMRTestDesc* pTestDesc = (VMRTestDesc*)lpFTE->dwUserData;

    // From the config, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );
    
    // Set the media for the graph
    hr = testGraph.SetMedia(pMedia);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to set media %s with the test graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }

    // Build an empty graph
    if (retval == TPR_PASS)
    {
        hr = testGraph.BuildEmptyGraph();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    // Add the VR and configure it according to the test desc
    if (retval == TPR_PASS)
    {
        hr = PrepVMRTest(&testGraph, pTestDesc);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to build graph with the video renderer for %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            retval = TPR_FAIL;
        }
    }

    // Enable the verification required - just before setting the state of the graph
    bool bGraphVerification = false;
    if (retval == TPR_PASS)
    {
        VerificationList verificationList = pTestDesc->GetVerificationList();
        VerificationList::iterator iterator = verificationList.begin();
        while (iterator != verificationList.end())
        {
            VerificationType type = (VerificationType)iterator->first;
            if (testGraph.IsCapable(type)) {
                hr = testGraph.EnableVerification(type, iterator->second);
                if (FAILED_F(hr))
                {
                    // BUGBUG: if it's not implemented, don't treat it as a failure.
                    if ( E_NOTIMPL == hr ) 
                    {
                        LOG( TEXT("%S: WARN %d@%S - Verification (%S) is not implemented!"), 
                                    __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type) );
                    }
                    else
                    {
                        LOG( TEXT("%S: ERROR %d@%S - failed to enable verification %S)"), 
                                    __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type) );
                        retval = TPR_FAIL;
                    }
                    break;
                }
                bGraphVerification = true;
            }
            else {
                LOG(TEXT("%S: WARNING %d@%S - unrecognized verification requested %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
            }
            iterator++;
        }
    }

    // Build the graph
    if (retval == TPR_PASS)
    {
        hr = testGraph.BuildGraph();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to build graph with the video renderer for %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: successfully built graph with the video renderer for %s"), __FUNCTION__, pMedia->GetUrl());
        }
    }

    // BUGBUG: Set the source and dest rectangles if any
    long videoWidth = 0;
    long videoHeight = 0;
    if (retval == TPR_PASS)
    {
        hr = testGraph.GetVideoWidth(&videoWidth);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get video width (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }

        hr = testGraph.GetVideoHeight(&videoHeight);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get video height (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    // Get the screen resolution
    DWORD screenWidth = 0;
    DWORD screenHeight = 0;
    if (retval == TPR_PASS)
    {
        hr = GetScreenResolution(&screenWidth, &screenHeight);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get screen resolution (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        }
    }

    // Get the duration
    LONGLONG duration = 0;
    if (retval == TPR_PASS)
    {
        hr = testGraph.GetDuration(&duration);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: WARNING %d@%S - failed to get duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        }        
    }

    // Set the graph to run
    if (retval == TPR_PASS)
    {
        hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to run graph"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
    }


    // Signal the start of verification
    if ((retval == TPR_PASS) && bGraphVerification)
    {
        hr = testGraph.StartVerification();
        if (FAILED_F(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    // The actual test
    if (retval == TPR_PASS)
    {
        // The position list is a list of percentages reflecting the position of the video window
        WndPosList *pWndPosList = pTestDesc->GetVWPositions();

        for(int i = 0; (i < pWndPosList->size()) && (retval == TPR_PASS); i++)
        {
            // Set the window position requested after converting to actual position
            // Only set if index is in range
            if (i < pWndPosList->size())
            {
                WndPos wndPos = (*pWndPosList)[i];
                wndPos.x = wndPos.x*screenWidth/100;
                wndPos.y = wndPos.y*screenHeight/100;
                
                // Move the window to the new position
                LOG(TEXT("%S: moving video window to (X:%d, Y:%d)"), __FUNCTION__, wndPos.x, wndPos.y);

                hr = testGraph.SetVideoWindowPosition(wndPos.x, wndPos.y, videoWidth, videoHeight);
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - setting video window position failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                }

                // Reset the verification, so we don't count the repaints caused during the move
                if (retval == TPR_PASS)
                    testGraph.ResetVerification();
            }

            // Check the results of the verification
            if (retval == TPR_PASS)
            {
                if (bGraphVerification && (retval == TPR_PASS))
                {
                    hr = testGraph.GetVerificationResults();
                    if (FAILED_F(hr))
                    {
                        LOG(TEXT("%S: ERROR %d@%S - verification failed"), __FUNCTION__, __LINE__, __FILE__);
                        retval = TPR_FAIL;
                    }
                    else {
                        LOG(TEXT("%S: verification succeeded"), __FUNCTION__);
                    }

                    // Reset the verification
                    if (retval == TPR_PASS)
                        testGraph.ResetVerification();
                }
            }

        }
    }

    while ( 1 )
    {
        MSG  msg;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if ( msg.message == WM_QUIT )
            break;

        Sleep(10);
    }

	// Readjust return value if HRESULT matches/differs from expected in some instances
	retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the testGraph
    testGraph.Reset();

    // Reset the test
    pTestDesc->Reset();

    return retval;
}
