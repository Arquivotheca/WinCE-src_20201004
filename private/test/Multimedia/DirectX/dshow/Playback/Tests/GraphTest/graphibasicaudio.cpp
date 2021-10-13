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
//  IBasicAudio
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

#define TESTVALUENUM 6
#define MAXVOLUME 0
#define MINVOLUME -10000
#define MAXBALANCE 10000
#define MINBALANCE -10000

//Test IBasicAudio
TESTPROCAPI IBasicAudioTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IBasicAudio *pIBasicAudio = NULL;
    UINT uNumber = 0;  
    rand_s(&uNumber);
    long randVolume = -abs(uNumber%MINVOLUME);
    rand_s(&uNumber);
    long randBalance = -abs(uNumber%MINBALANCE) + MAXBALANCE;

    long volumeTestArray[TESTVALUENUM] = {MINVOLUME, (MINVOLUME + MAXVOLUME)/2, MAXVOLUME, MINVOLUME - 1, MAXVOLUME+ 1, randVolume};
    long balanceTestArray[TESTVALUENUM] = {MINBALANCE, (MINBALANCE + MAXBALANCE)/2, MAXBALANCE, MINBALANCE - 1, MAXBALANCE + 1, randBalance};

    
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

    hr = testGraph.GetBasicAudio( &pIBasicAudio );
    if ( FAILED(hr) || !pIBasicAudio )
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to get IBasicAudio (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    long volume_in = 0;
    long balance_in = 0;
    long volume_out = 0;
    long balance_out = 0;

    for(int i = 0; i<TESTVALUENUM; i++) //besides Volume and Balance Test array, add random value test
    {
        //set the graph in paused state and do interface test
        hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS, NULL);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to change state to State_Running, (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
        else 
        {
            LOG(TEXT("%S: change state to State_Running"), __FUNCTION__);
            //set the volume and balance value
            hr = pIBasicAudio->put_Volume(volume_in);
            if(FAILED(hr))
            {
                if((volume_in>=MINVOLUME) && (volume_in<=MAXVOLUME))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to set volume value: %d (0x%x)"), __FUNCTION__, __LINE__, __FILE__, volume_in, hr);
                    retval =  TPR_FAIL;
                    goto cleanup;
                }
                else
                {
                    LOG( TEXT("%S: input volume is out of range, volume = %d."), __FUNCTION__, volume_in);
                }
            }
            else
            {
                if((volume_in<MINVOLUME) || (volume_in>MAXVOLUME))
                {
                    LOG(TEXT("%S: ERROR %d@%S - expected result doesn't match actual result: input volume is out of range, volume = %d, but the result is success: (0x%x)."), 
                        __FUNCTION__, __LINE__, __FILE__, volume_in, hr);
                    retval =  TPR_FAIL;
                    goto cleanup;
                }
                else
                {
                    LOG( TEXT("%S: succeeded to set volume: %d."), __FUNCTION__, volume_in);
                }
            }
            
            hr = pIBasicAudio->put_Balance(balance_in);
            if(FAILED(hr))
            {
                if((balance_in>=MINBALANCE) && (balance_in<=MAXBALANCE))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to set balance value: %d (0x%x)"), __FUNCTION__, __LINE__, __FILE__, balance_in, hr);
                    retval =  TPR_FAIL;
                    goto cleanup;
                }
                else
                {
                    LOG( TEXT("%S: input balance is out of range, balance = %d."), __FUNCTION__, balance_in);
                }
            }
            else
            {
                if((balance_in<MINBALANCE) || (balance_in>MAXBALANCE))
                {
                    LOG(TEXT("%S: ERROR %d@%S - expected result doesn't match actual result: input balance is out of range, balance = %d, but the result is success: (0x%x)."), 
                        __FUNCTION__, __LINE__, __FILE__, balance_in, hr);
                    retval =  TPR_FAIL;
                    goto cleanup;
                }
                else
                {
                    LOG( TEXT("%S: succeeded to set balance: %d."), __FUNCTION__, balance_in);
                }
            }            

            //get the volume and balance value, and verify them.            
            if( (volume_in>=MINVOLUME) && (volume_in<=MAXVOLUME))
            {   
                hr = pIBasicAudio->get_Volume(&volume_out);        
                if(FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to get volume value: %d (0x%x)"), __FUNCTION__, __LINE__, __FILE__, volume_in, hr);
                    retval =  TPR_FAIL;
                    goto cleanup;
                }                
                else if(abs(volume_out - volume_in) > pTestDesc->tolerance)
                {
                    LOG( TEXT("%S: ERROR %d@%S - the difference between expected volume: %d  and the volume we got: %d is beyond the tolerance: %d."), 
                                        __FUNCTION__, __LINE__, __FILE__, volume_in, volume_out, pTestDesc->tolerance);
                    retval =  TPR_FAIL;
                    goto cleanup;
                }
                else
                {
                    LOG( TEXT("%S: the tolerance between expected volume: %d and the volume we got: %d is within  the tolerance: %d."), 
                                        __FUNCTION__, volume_in, volume_out, pTestDesc->tolerance);
                }
            }
            
            if( (balance_in>=MINBALANCE) && (balance_in<=MAXBALANCE))
            {  
                hr = pIBasicAudio->get_Balance(&balance_out);
                if(FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to get balance value: %d (0x%x)"), __FUNCTION__, __LINE__, __FILE__, balance_in, hr);
                    retval =  TPR_FAIL;
                    goto cleanup;
                }            
                else if(abs(balance_out - balance_in) > pTestDesc->tolerance)
                {
                    LOG( TEXT("%S: ERROR %d@%S - the difference between expected balance: %d  and the balance we got: %d is beyond the tolerance: %d."), 
                                        __FUNCTION__, __LINE__, __FILE__, balance_in, balance_out, pTestDesc->tolerance);
                    retval =  TPR_FAIL;
                    goto cleanup;
                }
                else
                {
                    LOG( TEXT("%S: the tolerance between expected balance: %d and the balance we got: %d is within  the tolerance: %d."), 
                                        __FUNCTION__, balance_in, balance_out, pTestDesc->tolerance);
                }
            }
        }       
    }
    // reset the hr for adjusttestresults.
    hr = S_OK;

cleanup:

    // Reset the testGraph
    testGraph.Reset();
    
    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
}




