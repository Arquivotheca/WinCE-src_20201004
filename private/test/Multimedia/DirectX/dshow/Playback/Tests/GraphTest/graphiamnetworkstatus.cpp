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
//  IAMNetworkStatus
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

TESTPROCAPI IAMNetworkStatusTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IAMNetworkStatus *pIAMNetworkStatus = NULL;

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
    hr = testGraph.FindInterfaceOnGraph( IID_IAMNetworkStatus, (void **)&pIAMNetworkStatus );
    if ( FAILED(hr) || !pIAMNetworkStatus )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IAMNetworkStatus from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    VARIANT_BOOL IsBroadcast;
    long BufferingCount;
    long BufferingProgress;      
    long LostPackets;
    long ReceivedPackets;
    long ReceptionQuality;
    long RecoveredPackets;

    // Get the duration in the current time format
    LONGLONG duration = 0;
    hr = testGraph.GetDuration( &duration );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to get the duration (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        return TPR_FAIL;
        goto cleanup;
    }

    //test get_IsBroadcast
    hr =  pIAMNetworkStatus->get_IsBroadcast(&IsBroadcast);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_IsBroadcast failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get IsBroadcast: %d\n"), __FUNCTION__, IsBroadcast);
        if(IsBroadcast)
        {
            LOG( TEXT("%S: current clip is a broadcast clip\n"), __FUNCTION__);
        }
        else
        {
            LOG( TEXT("%S: current clip is not a broadcast clip\n"), __FUNCTION__);
        }
        
        //verify content
        if(IsBroadcast != VARIANT_BOOL(pTestDesc->isBroadcast))
        {
            LOG( TEXT("%S: ERROR %d@%S - expected isBroadcast value:%d does not match actual value:%d.  \n"), 
                __FUNCTION__, __LINE__, __FILE__, VARIANT_BOOL(pTestDesc->isBroadcast), (IsBroadcast));
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            LOG( TEXT("%S: expected isBroadcast value matches actual value.\n"), __FUNCTION__);
        }
        
    }
    
    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS, NULL);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetState failed with hr! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    //test get_BufferingCount
    hr =  pIAMNetworkStatus->get_BufferingCount(&BufferingCount);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_BufferingCount failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get BufferingCount: %d\n"), __FUNCTION__, BufferingCount);
        //verify content
        if(BufferingCount < 0)
        {
            LOG( TEXT("%S: ERROR %d@%S - Wrong value: BufferingCount=%d. \n"), 
                __FUNCTION__, __LINE__, __FILE__, BufferingCount);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    
    //test get_BufferingProgress
    long BufferingProgress1 = 0;
    long BufferingProgress2 = 0;
    hr =  pIAMNetworkStatus->get_BufferingProgress(&BufferingProgress1);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_BufferingProgress failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {        
        LOG( TEXT("%S: successfully get BufferingProgress: %d\n"), __FUNCTION__, BufferingProgress1);
        //verify content
        if(BufferingProgress1 < 0 || BufferingProgress1 > 100)
        {
            LOG( TEXT("%S: ERROR %d@%S - Wrong value: BufferingProgress=%d. \n"), 
                __FUNCTION__, __LINE__, __FILE__, BufferingProgress1);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    Sleep((DWORD)(duration/(10000*2)));

    hr =  pIAMNetworkStatus->get_BufferingProgress(&BufferingProgress2);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_BufferingProgress failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {        
        LOG( TEXT("%S: successfully get BufferingProgress: %d\n"), __FUNCTION__, BufferingProgress2);
        //verify content
        if(BufferingProgress2 < 0 || BufferingProgress2 > 100)
        {
            LOG( TEXT("%S: ERROR %d@%S - Wrong value: BufferingProgress=%d. \n"), 
                __FUNCTION__, __LINE__, __FILE__, BufferingProgress2);
            retval = TPR_FAIL;
            goto cleanup;
        }
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
    
    // test get_BufferingCount
    hr =  pIAMNetworkStatus->get_BufferingCount(&BufferingCount);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_BufferingCount failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get BufferingCount: %d\n"), __FUNCTION__, BufferingCount);
        //verify content
        if(BufferingCount < 0)
        {
            LOG( TEXT("%S: ERROR %d@%S - Wrong value: BufferingCount=%d. \n"), 
                __FUNCTION__, __LINE__, __FILE__, BufferingCount);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    
    // test get_BufferingProgress
   hr =  pIAMNetworkStatus->get_BufferingProgress(&BufferingProgress);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_BufferingProgress failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {        
        LOG( TEXT("%S: successfully get BufferingProgress: %d\n"), __FUNCTION__, BufferingProgress);
        //verify content
        if(BufferingProgress < 0 || BufferingProgress>100)
        {
            LOG( TEXT("%S: ERROR %d@%S - Wrong value: BufferingProgress=%d. \n"), 
                __FUNCTION__, __LINE__, __FILE__, BufferingProgress);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    // test get_LostPackets
    hr =  pIAMNetworkStatus->get_LostPackets(&LostPackets);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_LostPackets failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {        
        LOG( TEXT("%S: successfully get LostPackets: %d\n"), __FUNCTION__, LostPackets);
        //verify content
        if(LostPackets < 0)
        {
            LOG( TEXT("%S: ERROR %d@%S - Wrong value: LostPackets=%d. \n"), 
                __FUNCTION__, __LINE__, __FILE__, LostPackets);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    
    // test get_ReceivedPackets
    hr =  pIAMNetworkStatus->get_ReceivedPackets(&ReceivedPackets);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_ReceivedPackets failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get ReceivedPackets: %d\n"), __FUNCTION__, ReceivedPackets);
        //verify content
        if(ReceivedPackets < 0)
        {
            LOG( TEXT("%S: ERROR %d@%S - Wrong value: ReceivedPackets=%d. \n"), 
                __FUNCTION__, __LINE__, __FILE__, ReceivedPackets);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    // test get_ReceptionQuality
    hr =  pIAMNetworkStatus->get_ReceptionQuality(&ReceptionQuality);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_ReceptionQuality failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {     
        LOG( TEXT("%S: successfully get Reception Quality: %d\n"), __FUNCTION__, ReceptionQuality);
        //verify content
        if(ReceptionQuality < 0 || ReceptionQuality > 100)
        {
            LOG( TEXT("%S: ERROR %d@%S - Wrong value: ReceptionQuality=%d. \n"), 
                __FUNCTION__, __LINE__, __FILE__, ReceptionQuality);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    
    // test get_RecoveredPackets 
    hr =  pIAMNetworkStatus->get_RecoveredPackets(&RecoveredPackets);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_RecoveredPackets failed with hr 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get RecoveredPackets: %d\n"), __FUNCTION__, RecoveredPackets);
        //verify content
        if(RecoveredPackets < 0)
        {
            LOG( TEXT("%S: ERROR %d@%S - Wrong value: RecoveredPackets=%d. \n"), 
                __FUNCTION__, __LINE__, __FILE__, RecoveredPackets);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    


cleanup:
    if(pIAMNetworkStatus)
    {
        pIAMNetworkStatus->Release();
        pIAMNetworkStatus = NULL;
    }
    
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
    
}



