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

#include <windows.h>
#include <tux.h>
#include "TuxGraphTest.h"
#include "logging.h"
#include "StringConversions.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"

//header for IAMOpenProgress
#include <Op.h>
#define LOOP_TIMEOUT 100

TESTPROCAPI IAMOpenProgressTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    int i = 0;
    LONG evCode = 0;
    IMediaEvent* pEvent = NULL;
    IAMOpenProgress *pOpenProgress = NULL;
    LONGLONG llTotal = 0, llCurrent = 0,llLastT = 0,llLastC = 0;

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
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    // From the config, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object\n"), __FUNCTION__, __LINE__, __FILE__);
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
    hr = testGraph.FindInterfaceOnGraph( IID_IAMOpenProgress, (void **)&pOpenProgress );
    if ( FAILED(hr) || !pOpenProgress )
    {
        LOG( TEXT("%S: WARNING %d@%S - failed to retrieve IAMOpenProgress from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_SKIP;
        goto cleanup;
    }

    hr = testGraph.GetMediaEvent( &pEvent );
    if ( FAILED(hr) || !pEvent )
    {
        LOG( TEXT("%S: ERROR %d@%S - Failed to acquire IMediaEvent interface. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS, NULL);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetState failed with hr! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pOpenProgress->QueryProgress(&llTotal, &llCurrent);
    if (FAILED(hr)) 
    {
        LOG(TEXT("IAMOpenProgress:Unable to callQueryProgress with hresult 0x%0x."), hr);
        retval = TPR_SKIP;
        goto cleanup;
    }
    else
    {
        llLastC = llCurrent;
        llLastT = llTotal;
        LOG(TEXT("first :Total=%lld,Current=%lld"),llTotal,llCurrent);
    }
    
    do
    {
        hr = pEvent->WaitForCompletion(0x2FF, &evCode);
        if(SUCCEEDED(hr)) 
        {
            if(EC_ERRORABORT==evCode)
            {
                LOG(_T("IAMOpenProgressTest:We got back EC_ERRORABORT from WaitForCompletion.Test failed."));
                retval = TPR_FAIL;
                goto cleanup;
            }
        }
        else if (hr == VFW_E_WRONG_STATE) 
        {
            LOG(_T("IAMOpenProgressTest:We got back VFW_E_WRONG_STATE from WaitForCompletion.Test failed."));
            retval = TPR_FAIL;
            goto cleanup;
        }

        hr = pOpenProgress->QueryProgress(&llTotal, &llCurrent);
        if (FAILED(hr)) 
        {
            LOG(TEXT("IAMOpenProgress:Unable to QueryProgress with hresult 0x%0x."), hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
            LOG(TEXT("Total=%lld,Current=%lld"),llTotal,llCurrent);
        
        if( llTotal != llLastT )
        {
            LOG(TEXT("QueryProgress test failed, it return different Total bites for single media content."));
            retval = TPR_FAIL;
            goto cleanup;
        }    

        if ( evCode == EC_COMPLETE )
        {
            LOG( TEXT("Has reached the end of this media content."));
            if(llTotal == llCurrent)
            {
                LOG(TEXT("Verification for Total and EndOfContent succeed. EndOfcontent:=%lld =Total:%lld"),
                    llCurrent,llTotal);
                         break;
            }
            else
            {
                retval = TPR_FAIL;
                LOG(TEXT("QueryProgress return value not as expect,Total=%lld, not equal ot EndOfContent%lld"),
                    llTotal,llCurrent);
                break;
            }
        }

        if(llLastC > llCurrent || llCurrent > llLastT)
        {
            LOG(TEXT("QueryProgress return value not as expect,Current(%lld) should greater than LastCurrent(%lld),and should <Total(%lld)"),
                                llCurrent,llLastC,llLastT);
            retval = TPR_FAIL;
            goto cleanup;
        }
        llLastC= llCurrent;
        i++;
    }while(i < LOOP_TIMEOUT);        
    
     //if we timed out then there is a problem
    if(i==LOOP_TIMEOUT) 
    {
        LOG(TEXT("IAMOpenProgressTest failed, no event goes back during LOOT_TIMEOUT."));
        retval = TPR_FAIL;
         goto cleanup;
    }

    if(evCode != EC_COMPLETE) 
    {
        LOG(TEXT("IAMOpenProgress:The file streaming didn't finish as expected.evCode: 0x%0x."),evCode);
        retval = TPR_FAIL;
        goto cleanup;
    }

    //stop  this graph.
    hr = testGraph.SetState(State_Stopped, TestGraph::SYNCHRONOUS, NULL);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetState failed with hr! \n"), 
                                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    LOG(TEXT("__Test for QueryProgress complete. Have tested %d times during play"),i);    
    
    //restart to test AbortOperation
    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS, NULL);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetState failed with hr! \n"), 
                        __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    Sleep(10);

    hr = pOpenProgress->AbortOperation();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - AbortOperation failed with hr! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG(TEXT("IAMOpenProgressTest::AbortOperation call succeed."));
    }
cleanup:
    SAFE_RELEASE(pOpenProgress);    
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
}
