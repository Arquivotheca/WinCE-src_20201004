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
//  IAMDroppedFrames test
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

TESTPROCAPI IAMDroppedFramesTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IAMDroppedFrames *pIAMDroppedFrames = NULL;
    bool b_IsPass = false;

    long averageSize = 0;
    const long lSize = 32;
    long lArray[lSize] = {0};
    long lNumCopied = 0;
    long lDropped = 0;
    long lNotDropped = 0;
    
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
    hr = testGraph.FindInterfaceOnGraph( IID_IAMDroppedFrames, (void **)&pIAMDroppedFrames );
    if ( FAILED(hr) || !pIAMDroppedFrames )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IAMDroppedFrames from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS, NULL);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetState failed with 0x%08x! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

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

    //test GetAverageFrameSize()
    hr =  pIAMDroppedFrames->GetAverageFrameSize(&averageSize);
    if ( FAILED(hr) )
    {
        if(hr == E_NOTIMPL)
        {
            LOG( TEXT("%S: Not implemented function: IAMDroppedFrames::GetAverageFrameSize().\n"), __FUNCTION__);
            retval = TPR_SKIP;
        }
        else
        {
            LOG( TEXT("%S: ERROR %d@%S - GetAverageFrameSize failed with hr 0x%x ! \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    else
    {
        LOG( TEXT("%S: successfully get AverageFrameSize: %d\n"), __FUNCTION__, averageSize);
        b_IsPass = true;
    }
   
    //test GetDroppedInfo()
    hr =  pIAMDroppedFrames->GetDroppedInfo(lSize, lArray, &lNumCopied);
    if ( FAILED(hr) )
    {
        if(hr == E_NOTIMPL)
        {
            LOG( TEXT("%S: Not implemented function: IAMDroppedFrames::GetDroppedInfo().\n"), __FUNCTION__);
            retval = TPR_SKIP;
        }
        else
        {
            LOG( TEXT("%S: ERROR %d@%S - GetDroppedInfo failed with hr 0x%x ! \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    else
    {
        LOG( TEXT("%S: successfully get Dropped frame Info.\n"), __FUNCTION__);
        b_IsPass = true;
    }

    //test GetNumDropped()
    hr =  pIAMDroppedFrames->GetNumDropped(&lDropped);
    if ( FAILED(hr) )
    {
        if(hr == E_NOTIMPL)
        {
            LOG( TEXT("%S: Not implemented function: IAMDroppedFrames::GetNumDropped().\n"), __FUNCTION__);
            retval = TPR_SKIP;
        }
        else
        {
            LOG( TEXT("%S: ERROR %d@%S - GetNumDropped failed with hr 0x%x ! \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    else
    {
        LOG( TEXT("%S: successfully get number of dropped frames: %d\n"), __FUNCTION__, lDropped);
        //verify content
        if((pTestDesc->totalFrameCount != 0) && (pTestDesc->droppedFrameRate != 0))
        {
            long totalFrame = pTestDesc->totalFrameCount;
            long droppedRate = pTestDesc->droppedFrameRate;
            double actualDroppedRate = (double)lDropped/totalFrame; 
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
                b_IsPass = true;
            } 
        }
    }    

    //test GetNumNotDropped()
    hr =  pIAMDroppedFrames->GetNumNotDropped(&lNotDropped);
    if ( FAILED(hr) )
    {
        if(hr == E_NOTIMPL)
        {
            LOG( TEXT("%S: Not implemented function: IAMDroppedFrames::GetNumNotDropped().\n"), __FUNCTION__);
            retval = TPR_SKIP;
        }
        else
        {
            LOG( TEXT("%S: ERROR %d@%S - GetNumNotDropped failed with hr 0x%x ! \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    else
    {
        LOG( TEXT("%S: successfully get number of not dropped frames: %d\n"), __FUNCTION__, lNotDropped);
        b_IsPass = true;
    }

cleanup:
    if(pIAMDroppedFrames)
    {
        pIAMDroppedFrames->Release();
        pIAMDroppedFrames = NULL;
    }
    
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances 
    retval = pTestDesc->AdjustTestResult(hr, retval);
  
    // if some funtions in this interface are not implemented, and some funtions was called successfully, the case pass.
    if((retval == TPR_SKIP) && b_IsPass)
    {
        retval = TPR_PASS;
    }
    // Reset the test
    pTestDesc->Reset();

    return retval;
    
}

