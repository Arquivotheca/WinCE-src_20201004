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
//  Graph Playback Test functions
//
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

//Test IQualProp
TESTPROCAPI IQualPropTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IGraphBuilder *pIGraphBuilder = NULL;
    IQualProp *pIQualProp = NULL;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

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
    hr = testGraph.FindInterfaceOnGraph( IID_IQualProp, (void **)&pIQualProp );
	if ( FAILED(hr) || !pIQualProp )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IQualProp from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}

    //run the filter graph
    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS, NULL);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to change state to State_Running, (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }
    else 
    {
        LOG(TEXT("%S: change state to State_Running"), __FUNCTION__);
    }

    //wait for completion
    LONG evCode = 0;
    hr = testGraph.WaitForCompletion(INFINITE, &evCode);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - WaitForCompletion failed with event code (0x%x). (0x%x)"), 
                            __FUNCTION__, __LINE__, __FILE__, evCode, hr );
        retval = TPR_FAIL;        
        goto cleanup;
    }
    if ( evCode != EC_COMPLETE )
    {
        LOG( TEXT("%S: ERROR %d@%S - The event code returned (0x%08x)is not EC_COMPLETE."), 
                            __FUNCTION__, __LINE__, __FILE__, evCode );
        retval = TPR_FAIL;
        goto cleanup;
    }

    int DevSyncOffset;
    int AvgFrameRate;
    int Jitter;
    int AvgSyncOffset;
    int FramesDrawn;
    int FramesDroppedInRenderer;

    //begin to test the methods of interface
    hr = pIQualProp->get_DevSyncOffset(&DevSyncOffset);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get DevSyncOffset failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get DevSyncOffset: %d\n"), __FUNCTION__, DevSyncOffset);
        // verify the content
        /*if(DevSyncOffset != pMedia->DevSyncOffset)
        {
            LOG( TEXT("%S: ERROR %d@%S - Expected DevSyncOffset: %d deos not match actual DevSyncOffset: %d .\n"), 
                    __FUNCTION__, __LINE__, __FILE__, pMedia->DevSyncOffset, DevSyncOffset);
            retval = TPR_FAIL;
            goto cleanup;
        }*/
    }

    hr = pIQualProp->get_AvgFrameRate(&AvgFrameRate);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get AvgFrameRate failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get AvgFrameRate: %d\n"), __FUNCTION__, AvgFrameRate);
        // verify the content
        
    }

    hr = pIQualProp->get_Jitter(&Jitter);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get Jitter failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get Jitter: %d\n"), __FUNCTION__, Jitter);
        // verify the content
        
    }

    hr = pIQualProp->get_AvgSyncOffset(&AvgSyncOffset);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get AvgSyncOffset failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get AvgSyncOffset: %d\n"), __FUNCTION__, AvgSyncOffset);
        // verify the content
        
    }

    hr = pIQualProp->get_FramesDrawn(&FramesDrawn);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get FramesDrawn failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get FramesDrawn: %d\n"), __FUNCTION__, FramesDrawn);
        // verify the content
        
    }

    hr = pIQualProp->get_FramesDroppedInRenderer(&FramesDroppedInRenderer);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get FramesDroppedInRenderer failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get FramesDroppedInRenderer: %d\n"), __FUNCTION__, FramesDroppedInRenderer);
        //verify content
        if((pTestDesc->totalFrameCount != 0) && (pTestDesc->droppedFrameRate!=0))
        {
            long totalFrame = pTestDesc->totalFrameCount;
            long droppedRate = pTestDesc->droppedFrameRate;
            double actualDroppedRate = (double)FramesDroppedInRenderer/totalFrame; 
            double expectedDroppedRate = (double)droppedRate/100; 
            if(actualDroppedRate > expectedDroppedRate)
            {
                LOG( TEXT("%S: ERROR %d@%S - the actual dropped fram rate: %f is larger than expected dropped rate: %f . \n"), 
                        __FUNCTION__, __LINE__, __FILE__, actualDroppedRate, expectedDroppedRate);
                retval = TPR_FAIL;
                goto cleanup;
            }
            else
            {
                LOG( TEXT("%S: the actual dropped fram rate is acceptable: %f.\n"), __FUNCTION__, actualDroppedRate);
            }        
        }
    }

    
cleanup:    
    if(pIQualProp)
    {
        pIQualProp->Release();
        pIQualProp = NULL;
    }
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
}





