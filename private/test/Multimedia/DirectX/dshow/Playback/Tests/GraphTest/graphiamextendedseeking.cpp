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
//  IAMExtendSeeking tests
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
#include <qnetwork.h>

TESTPROCAPI IAMExtendedSeekingTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IAMExtendedSeeking *pIAMExtendedSeeking = NULL;
    
    long markerNum = 1; //marker number start from 1
    long markerCount = 0;
    BSTR markerName = NULL;
    double markerTime = 0.0;
    long exCapabilities = 0;
    long currentMarker = 0;
    double playbackSpeed =0.0;

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
    hr = testGraph.FindInterfaceOnGraph( IID_IAMExtendedSeeking, (void **)&pIAMExtendedSeeking );
	if ( FAILED(hr) || !pIAMExtendedSeeking )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IAMExtendedSeeking from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}

    //test get_MarkerCount function
    hr = pIAMExtendedSeeking->get_MarkerCount(&markerCount);
    if ( FAILED(hr) && pTestDesc->markerCount )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_MarkerCount failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        // verify the content
        if(markerCount != pTestDesc->markerCount)
        {
            LOG( TEXT("%S: ERROR %d@%S - verify markerCount failed ! \n Expected: %d != Got: %d .\n"), 
                    __FUNCTION__, __LINE__, __FILE__, pTestDesc->markerCount, markerCount);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            LOG( TEXT("%S: successfully get Marker Count: %d\n"), __FUNCTION__, markerCount);
        }
    }

    //test get_ExSeekCapabilities function
    hr = pIAMExtendedSeeking->get_ExSeekCapabilities(&exCapabilities);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_ExSeekCapabilities failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        // verify the content
        /*if(exCapabilities != pTestDesc->exCapabilities)
        {
            LOG( TEXT("%S: ERROR %d@%S - Expected value of exCapabilities: %d does not match actual value: %d .\n"), 
                    __FUNCTION__, __LINE__, __FILE__, pTestDesc->exCapabilities, exCapabilities);
            retval = TPR_FAIL;
            goto cleanup;
        }*/
        hr = S_OK;
        LOG( TEXT("%S: successfully get SeekCapabilities %d\n"), __FUNCTION__, exCapabilities);
        
    }

    //test get_PlaybackSpeed function
    hr = pIAMExtendedSeeking->get_PlaybackSpeed(&playbackSpeed);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_PlaybackSpeed failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get playbackSpeed %f\n"), __FUNCTION__, playbackSpeed);
    }

    //test get_ExSeekCapabilities function
    double newSpeed = 2*playbackSpeed;
    hr = pIAMExtendedSeeking->put_PlaybackSpeed(newSpeed);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - put_PlaybackSpeed failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }       
    else
    {
        LOG( TEXT("%S: successfully put playbackSpeed: %f\n"), __FUNCTION__, newSpeed);
    }

    //test get_PlaybackSpeed function
    hr = pIAMExtendedSeeking->get_PlaybackSpeed(&playbackSpeed);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_PlaybackSpeed failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        if(playbackSpeed != newSpeed)
        {
            LOG( TEXT("%S: ERROR %d@%S - Expected value of playbackSpeed: %f does not match actual value: %d .\n"), 
                    __FUNCTION__, __LINE__, __FILE__, newSpeed, playbackSpeed);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            LOG( TEXT("%S: successfully get playbackSpeed %f\n"), __FUNCTION__, playbackSpeed);
        }
    }

    do
    {
        //test get_CurrentMarker function
        hr = pIAMExtendedSeeking->get_CurrentMarker(&currentMarker);
        if ( FAILED(hr) )
        {
            if(hr == E_NOTIMPL)
            {
                LOG( TEXT("%S: Not implemented function: pIAMExtendedSeeking::get_CurrentMarker().\n"), __FUNCTION__);
            }
            else if ( !pTestDesc->markerCount )
            {
                LOG( TEXT("%S: No markers in the stream!\n"), __FUNCTION__);
                hr = S_OK;
                break;
            }
            else
            {
                LOG( TEXT("%S: ERROR %d@%S - get_CurrentMarker failed with hr 0x%x ! \n"), 
                        __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
                goto cleanup;
            }
        }
        else
        {
            /*// verify the content
            if(currentMarker != markerNum)
            {
                LOG( TEXT("%S: ERROR %d@%S - Expected value of currentMarker: %d does not match actual value: %d .\n"), 
                        __FUNCTION__, __LINE__, __FILE__, markerNum, currentMarker);
                retval = TPR_FAIL;
                goto cleanup;
            }*/
            
            LOG( TEXT("%S: successfully get current Marker %d\n"), __FUNCTION__, currentMarker);
            
        }

        int index =0;
        for(int i =0;i<markerCount;i++)
        {
            if(markerNum == pTestDesc->mediaMarkerInfo.at(i)->markerNum)
            {
                index =i;
            }
        }
        
        //test GetMarkerName function
        hr = pIAMExtendedSeeking->GetMarkerName(markerNum, &markerName);
        if ( FAILED(hr) && pTestDesc->markerCount )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMarkerName failed with hr 0x%x ! \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            // verify the content
            if(pTestDesc->mediaMarkerInfo.at(index)->markerName)
            {
                if(strcmp((char*)pTestDesc->mediaMarkerInfo.at(index)->markerName, (char*)markerName) != 0)
                {
                    LOG( TEXT("%S: ERROR %d@%S - Expected value of markerName: %s does not match actual value: %s .\n"), 
                            __FUNCTION__, __LINE__, __FILE__, pTestDesc->mediaMarkerInfo.at(index)->markerName, markerName);
                    retval = TPR_FAIL;
                    goto cleanup;
                }
                else
                {
                    LOG( TEXT("%S: successfully get Marker %d Name: %s\n"), __FUNCTION__, markerNum, markerName);
                }
            }
        }

        //test GetMarkerTime function
        hr = pIAMExtendedSeeking->GetMarkerTime(markerNum, &markerTime);
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetMarkerTime failed with hr 0x%x ! \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            // verify the content
            if(markerTime != pTestDesc->mediaMarkerInfo.at(index)->markerTime)
            {
                LOG( TEXT("%S: ERROR %d@%S - Expected value of markerTime: %f does not match actual value: %f .\n"), 
                        __FUNCTION__, __LINE__, __FILE__, pTestDesc->mediaMarkerInfo.at(index)->markerTime, markerTime);
                retval = TPR_FAIL;
                goto cleanup;
            }
            else
            {
                LOG( TEXT("%S: successfully get Marker %d Time: %f\n"), __FUNCTION__, markerNum, markerTime);
            }
        }
    
        markerNum++;
    }
    while(markerNum <= markerCount);
    


cleanup:

    if(pIAMExtendedSeeking)
    {
        pIAMExtendedSeeking->Release();
        pIAMExtendedSeeking = NULL;
    }
    
    if(markerName)
    {
        SysFreeString(markerName);
    }
    
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->clearMem();
    pTestDesc->Reset();

    return retval;
    
}
