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
//  IAudioRendererWaveout
//
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "TuxGraphTest.h"
#include "logging.h"
#include "StringConversions.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"

#include <audiosys.h>

TESTPROCAPI IAudioRendererWaveOutTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IAudioRendererWaveOut *pIAudioRendererWaveOut = NULL;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
    {
        return SPR_HANDLED;
    }
    else if (TPM_EXECUTE != uMsg)
    {
        return TPR_NOT_HANDLED;
    }

    // Get the test config object from the test parameter
    PlaybackInterfaceTestDesc *pTestDesc = (PlaybackInterfaceTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    // Instantiate the test graph
    TestGraph testGraph;

    if ( pTestDesc->RemoteGB() )
    {
        testGraph.UseRemoteGB( TRUE );
    }

    //set Meida
    hr = testGraph.SetMedia(pMedia);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to set media for testGraph %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else {
        LOG(TEXT("%S: successfully set media for %s"), __FUNCTION__, pMedia->GetUrl());
    }
     
    // Build the graph
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }
    
    // FindInterfacesOnGraph can be used within either normal or secure chamber.
    hr = testGraph.FindInterfaceOnGraph( IID_IAudioRendererWaveOut, (void **)&pIAudioRendererWaveOut );
	if ( FAILED(hr) || !pIAudioRendererWaveOut )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IAudioRendererWaveOut from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}

    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS, NULL);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetState failed with hr 0x%x!\n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    UINT uMessage = MM_WOM_FORCESPEAKER;
    DWORD dw1 = 0;
    DWORD dw2 = 0;
    hr = pIAudioRendererWaveOut->SendMessage(uMessage, (DWORD)TRUE, 0);
    if ( (S_OK != hr) && ((HRESULT)MMSYSERR_NODRIVER != hr) && (MMSYSERR_NOTSUPPORTED != hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - TestSendMessage: SendMessage failed unexpectedly, hr = 0x%08x"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Random input
    for (int i = 0; i < 1000; ++i)
    {
        errno_t errno;
        
        if (0 != (errno = rand_s(&uMessage)) ||
            0 != (errno = rand_s((UINT *)&dw1)) ||
            0 != (errno = rand_s((UINT *)&dw2)))
        {
            LOG(TEXT("TestSendMessage: rand_s failed, errno = %d"), errno);
            retval = TPR_FAIL;
            goto cleanup;
        }

        hr = pIAudioRendererWaveOut->SendMessage(uMessage, dw1, dw2);

        if ( (S_OK != hr) && ((HRESULT)MMSYSERR_NODRIVER != hr) && (MMSYSERR_NOTSUPPORTED != hr) )
        {
            LOG(TEXT("TestSendMessage: SendMessage failed unexpectedly, uMsg = %u, dw1 = %lu, dw2 = %lu, hr = 0x%08x"),
                         uMessage, dw1, dw2, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
            hr = S_OK;  // reset the results otherwise the adjusttestresult will report false negative result.
        
    }

cleanup:
    if(pIAudioRendererWaveOut)
    {
        pIAudioRendererWaveOut->Release();
        pIAudioRendererWaveOut = NULL;
    }
	
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;    
}

