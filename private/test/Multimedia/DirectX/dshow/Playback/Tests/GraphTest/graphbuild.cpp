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
//  Build Graph Test functions
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
#include "utility.h"

static TESTPROCAPI PerformRenderTest( TestGraph *pTestGraph );
static TESTPROCAPI PerformSetLogFileTest( TestGraph *pTestGraph );

TESTPROCAPI EmptyGraphQueryInterfaceTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the BuildGraph test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    TestGraph testGraph;

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // Build the empty graph
    hr = testGraph.BuildEmptyGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built an empty graph"), __FUNCTION__);
    }

    if (retval == TPR_PASS)
    {
        // Query the essential interfaces
        hr = testGraph.AcquireInterfaces();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to query common interfaces (0x%x)."), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: successfully queried interfaces"), __FUNCTION__);
        }
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI AddSourceFilterTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    // From the config, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }


    TestGraph testGraph;

    if ( pTestDesc->RemoteGB() )
    {
        // AddSourceFilter is not supported.
        retval = TPR_SKIP;
        goto cleanup;
    }

    // Set the media for the graph
    hr = testGraph.SetMedia(pMedia);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to set media %s with the test graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }

    // Add a source filter for the set media file
    hr = testGraph.AddSourceFilter();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to add source filter for supported media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully added source filter for supported media %s"), __FUNCTION__, pMedia->GetUrl());
    }

cleanup:
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI AddUnsupportedSourceFilterTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

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

    // Add a source filter for the given media file
    hr = testGraph.AddSourceFilter();
    if (SUCCEEDED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - added source filter for unsupported media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: failed to add source filter for unsupported media %s as expected"), __FUNCTION__, pMedia->GetUrl());
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI BuildGraphTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

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

    // Enable the verification required
    bool bGraphBuildLatency = false;
    DWORD latencyTolerance = 0;
    bool bGraphVerification = false;
    VerificationList verificationList = pTestDesc->GetVerificationList();
    VerificationList::iterator iterator = verificationList.begin();
    while (iterator != verificationList.end())
    {
        VerificationType type = (VerificationType)iterator->first;
        if (type == BuildGraphLatency) {
            bGraphBuildLatency = true;
            latencyTolerance = *(DWORD*)iterator->second;
        }
        else if (testGraph.IsCapable(type)) {
            hr = testGraph.EnableVerification(type, iterator->second);
            if (FAILED_F(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to enable verification %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
                break;
            }
            bGraphVerification = true;
        }
        else {
            LOG(TEXT("%S: WARNING %d@%S - unrecognized verification requested %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
        }
        iterator++;
    }

    // Instantiate timer for measurement
    Timer timer;
    LONGLONG latency;

    // Start the timer
    timer.Start();

    // Build the graph
     //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
       // LOG(TEXT("and also failed in trying to build transport streaming Graph using MPEG-2 Demux"));
        retval = TPR_FAIL;
        goto cleanup;
    }

    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }

    // Measure the latency
    latency = timer.Stop();

    // Verification of build latency
    if (bGraphBuildLatency)
    {
        LOG(TEXT("%S: graph build latency: %d msec, tolerance: %d msec"), __FUNCTION__, TICKS_TO_MSEC(latency), latencyTolerance);
        if (TICKS_TO_MSEC(latency) > latencyTolerance)
        {
            LOG(TEXT("%S: ERROR %d@%S - actual build latency %d msec > accepted: %d msec."), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(latency), latencyTolerance);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: - actual build latency %d msec <= accepted: %d msec."), __FUNCTION__, TICKS_TO_MSEC(latency), latencyTolerance);
        }
    }

    // Check the results of the other verifications
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
cleanup:
    // Reset the testGraph
    testGraph.Reset();

    // Reset the test
    pTestDesc->Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI BuildGraphMultipleTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    // From the config, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    for(int i = 0; i < GRAPH_BUILD_REPETITIONS; i++)
    {
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

        // Build the graph
     //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
        hr = testGraph.BuildGraph();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to build graph for %s (0x%x) using RenderFile()"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            LOG(TEXT("and also failed in trying to build transport streaming Graph using MPEG-2 Demux"));
            retval = TPR_FAIL;

            // Break out of the loop if we failed
            break;

        }

        else {
            LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
        }


        // Reset the testGraph
        testGraph.Reset();
    }

    if (retval == TPR_PASS)
        LOG(TEXT("%S: successfully rendered %s %d times."), __FUNCTION__, pMedia->GetUrl(), i);

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI BuildGraphQueryInterfaceTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

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

    // Build the graph
     //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for %s (0x%x) using RenderFile()"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        LOG(TEXT("and also failed in building transport streaming Graph using MPEG-2 Demux"));

        retval = TPR_FAIL;

    }


    else {
        LOG(TEXT("%S: successfully rendered %s"), __FUNCTION__, pMedia->GetUrl());
    }

    if (retval == TPR_PASS)
    {
        // Query the essential interfaces
        hr = testGraph.AcquireInterfaces();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to render supported media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: successfully rendered %s"), __FUNCTION__, pMedia->GetUrl());
        }
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI BuildGraphUnsupportedMediaTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

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

    // Build the graph
      //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();

    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        LOG(TEXT("%S: ERROR %d@%S - rendered unsupported media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: failed to render %s as expected."), __FUNCTION__, pMedia->GetUrl());
    }


    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI BuildMultipleGraphTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    int nMedia = pTestDesc->GetNumMedia();

    TestGraph* pTestGraphs = new TestGraph[nMedia];

    for(int i = 0; i < nMedia; i++)
    {
        TestGraph* pTestGraph = &pTestGraphs[i];
        if ( pTestDesc->RemoteGB() )
            pTestGraph->UseRemoteGB( TRUE );

        // Get the media file to be used
        PlaybackMedia* pMedia = pTestDesc->GetMedia(i);
        if (!pMedia)
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }

        if (retval == TPR_PASS)
        {
            // Set the media for the graph
            hr = pTestGraph->SetMedia(pMedia);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to set media %s with the test graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
                retval = TPR_FAIL;
            }
        }

        if (retval == TPR_PASS)
        {
              //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
            hr = pTestGraph->BuildGraph();
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to render supported media %s (0x%x) with %dth graph"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr, i);
                LOG(TEXT("Also Failed in building graph for MPEG2 Transport Stream"));
                retval = TPR_FAIL;
            }
        }
    }

    if (retval == TPR_PASS)
        LOG(TEXT("%S: successfully rendered multiple (%d) media."), __FUNCTION__, nMedia);

    for(int i = 0; i < nMedia; i++)
    {
        TestGraph* pTestGraph = &pTestGraphs[i];
        // Reset the testGraph
        pTestGraph->Reset();
    }

    // release the resource
    delete [] pTestGraphs;

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI RenderPinTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

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

    // Add a source filter for the given media file
    hr = testGraph.AddSourceFilter();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to add source filter for supported media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully added source filter for supported media %s"), __FUNCTION__, pMedia->GetUrl());
    }

    if (retval == TPR_PASS)
    {
        // Render the unconnected pins of the graph
        hr = testGraph.RenderUnconnectedPins();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to render pins of the source filter for supported media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: successfully rendered pins of the source filter for supported media %s"), __FUNCTION__, pMedia->GetUrl());
        }
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI ConnectSourceFilterToRendererTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

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

    // Build the empty graph
    hr = testGraph.BuildEmptyGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built an empty graph"), __FUNCTION__);
    }

    TCHAR szNewFilterName[FILTER_NAME_LENGTH];
    int nFilters = 0;

    if (retval == TPR_PASS)
    {
        // Add the source and renderer filters to the graph - we only support 1 renderer filter at the moment
        // The protocol is that the first filter is the source filter and the following filter is a renderer filter
        // And that the first pin of the source is to be connected to the first pin of the renderer
        nFilters = pTestDesc->filterList.size();
        //don't try to access something that doesn't exist
        if(nFilters > 0) {
            FilterId id;
            GraphTest::m_pTestConfig->GetFilterDescFromName( pTestDesc->filterList[0], id );
            if ( UnknownFilter != id )
            {
                hr = testGraph.AddFilter( id, szNewFilterName, countof(szNewFilterName));
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to add source filter %d (0x%x) to the graph"), __FUNCTION__, __LINE__, __FILE__, id, hr);
                    retval = TPR_FAIL;
                }
            }
            else
            {
                LOG(TEXT("%S: ERROR %d@%S - Unknown source Filter ID."), __FUNCTION__, __LINE__, __FILE__ );
                retval = TPR_FAIL;
            }
        }
        else
        {
            LOG(TEXT("%S: ERROR %d@%S - user did not specify source filter to add to the graph"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
    }

    if (retval == TPR_PASS)
    {
        // Added the source filter
        LOG(TEXT("%S: added source filter %s to the graph"), __FUNCTION__, szNewFilterName);

        // Add the renderer filter
        if(nFilters >1) {
            FilterId id;
            GraphTest::m_pTestConfig->GetFilterDescFromName( pTestDesc->filterList[1], id );
            if ( UnknownFilter != id )
            {
                hr = testGraph.AddFilter( id, szNewFilterName, countof(szNewFilterName));
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to add renderer filter %d (0x%x) to the graph"), __FUNCTION__, __LINE__, __FILE__, id, hr);
                    retval = TPR_FAIL;
                }
            }
            else
            {
                LOG(TEXT("%S: ERROR %d@%S - Unknown renderer Filter ID."), __FUNCTION__, __LINE__, __FILE__ );
                retval = TPR_FAIL;
            }
        }
        else
        {
            LOG(TEXT("%S: ERROR %d@%S - user did not specify renderer to add to the graph"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }

    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: added renderer filter %s to the graph"), __FUNCTION__, szNewFilterName);

        // Connect the source filter to the renderer intelligently
        hr = testGraph.Connect(pTestDesc->filterList[0], 0, pTestDesc->filterList[1], 0);
        if (FAILED_F(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to connect source filter to renderer for supported media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: successfully rendered pins of the source filter for supported media %s"), __FUNCTION__, pMedia->GetUrl());
        }
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI BuildGraphPreLoadFilterTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

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

    // Build the empty graph
    hr = testGraph.BuildEmptyGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built an empty graph"), __FUNCTION__);
    }

    // Get the filters to be preloaded
    if (retval == TPR_PASS)
    {
        // Add the specified filters to the graph
        int nFilters = pTestDesc->filterList.size();
        for(int i = 0; i < nFilters; i++)
        {
            TCHAR szNewFilterName[FILTER_NAME_LENGTH];
            FilterId id;
            GraphTest::m_pTestConfig->GetFilterDescFromName( pTestDesc->filterList[i], id );
            if ( UnknownFilter != id )
            {
                hr = testGraph.AddFilter( id, szNewFilterName, countof(szNewFilterName));
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to add specified filter %d (0x%x) to the graph"), __FUNCTION__, __LINE__, __FILE__, id, hr);
                    retval = TPR_FAIL;
                    break;
                }
            }
            else
            {
                LOG(TEXT("%S: ERROR %d@%S - Unknown Filter ID."), __FUNCTION__, __LINE__, __FILE__ );
                retval = TPR_FAIL;
            }
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: successfully added the specified filters to the graph"), __FUNCTION__);
        // Build the graph - this should utilize the filters that we preloaded
        hr = testGraph.BuildGraph();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to render supported media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            LOG(TEXT("Also Failed in building graph for MPEG2 Transport Stream"));
            retval = TPR_FAIL;
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: successfully built the graph for the media %s"), __FUNCTION__, pMedia->GetUrl());

        // Now check that the filters that we added are connected in the graph
        int nFilters = pTestDesc->filterList.size();
        for(int i = 0; i < nFilters; i++)
        {
            StringList filterNameList = pTestDesc->filterList;
            hr = testGraph.AreFiltersInGraph(filterNameList);
            if (S_OK == hr)
            {
                LOG(TEXT("%S: verified that pre-loaded filters are in the graph"), __FUNCTION__);
            }
            else if ((S_FALSE == hr) || FAILED(hr))
            {
                LOG(TEXT("%S: failed to verify that the pre-loaded filters are in the graph - failing test."), __FUNCTION__);
                retval = TPR_FAIL;
            }
        }
    }

    // Reset the testGraph
    testGraph.Reset();

    // Reset the test desc
    pTestDesc->Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}



////////////////////////////////////////////////////////////////////////////////
// GraphBuilderIFTest
//  Performs IGraphBuilder interface test on single media file
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
//
TESTPROCAPI
IGraphBuilderTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

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

    // Build the empty graph
    hr = testGraph.BuildEmptyGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built an empty graph"), __FUNCTION__);
    }

    // Before
    retval = PerformRenderTest( &testGraph );

    // Render the file, which is done inside BuildGraph
    hr = testGraph.BuildGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to render media %s (0x%x)"),
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else {
        LOG( TEXT("%S: successfully render %s"), __FUNCTION__, pMedia->GetUrl() );
    }

    // SetLogFile test cases
    if ( !pTestDesc->RemoteGB() )
        retval = PerformSetLogFileTest( &testGraph );

cleanup:

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the test desc
    pTestDesc->Reset();

    return retval;
}

////////////////////////////////////////////////////////////////////////////////
// PerformRenderTest
//  Performs various test on RenderFile method.
//
// Return value:
//  trPASS if the test passed, trFAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI PerformRenderTest( TestGraph *pTestGraph )
{
    // Some random value..
    WCHAR lpPlayList[10] = L"xyz";
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    IGraphBuilder *pGraphBuilder = NULL;

    if ( !pTestGraph )
    {
        LOG( TEXT(" NULL TestGraph pointer passed." ) );
        return TPR_FAIL;
    }

    // Since GetGraph() addref for the pointer, we need to release it after using it.
    pGraphBuilder = pTestGraph->GetGraph();
    if ( !pGraphBuilder )
    {
        LOG( TEXT("failed to retrieve graph builder pointer") );
        return TPR_FAIL;
    }

    PlaybackMedia *pMedia = pTestGraph->GetCurrentMedia();
    if ( !pMedia )
    {
        LOG( TEXT("failed to retrieve the media") );
        return TPR_FAIL;
    }

    // This test passes NULL as the first parameter.
    hr = pGraphBuilder->RenderFile(NULL, NULL);
    if ( hr !=E_POINTER )
    {
        LOG( TEXT("%S: ERROR %d@%S - IGraphBuilder::RenderFile should have returned E_POINTER when we passed NULL as first parameters, result returned 0x%0x"),

                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

    //This test passes valid first parameter and non-NULL (valid pointer) as the second parameter.
    WCHAR wszUrl[MAX_PATH];
    TCharToWChar( pMedia->GetUrl(), wszUrl, countof(wszUrl) );
    hr = pGraphBuilder->RenderFile( wszUrl, lpPlayList );
    // BUGBUG: E_NOTIMPL should be returned???
    if ( E_NOTIMPL != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - RenderFile with Non-NULL as 2nd parameter didn't return E_NOTIMPL. (0x%08x)"),
                    __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

    if ( pGraphBuilder )
        pGraphBuilder->Release();

    return retval;
}

////////////////////////////////////////////////////////////////////////////////
// PerformSetLogFileTest
//  Performs various test on IGraphBuilder::SetLogFile method.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI PerformSetLogFileTest( TestGraph *pTestGraph )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    UINT uNumber = 0;
    IGraphBuilder *pGraphBuilder = NULL;
    IMediaEvent *pMediaEvent = NULL;

    if ( !pTestGraph )
    {
        LOG( TEXT(" NULL TestGraph pointer passed." ) );
        return TPR_FAIL;
    }

    // Since GetGraph() addref for the pointer, we need to release it after using it.
    pGraphBuilder = pTestGraph->GetGraph();
    if ( !pGraphBuilder )
    {
        LOG( TEXT("failed to retrieve graph builder pointer") );
        return TPR_FAIL;
    }
    hr = pTestGraph->GetMediaEvent( &pMediaEvent );
      if ( FAILED(hr) || !pMediaEvent )
    {
        LOG( TEXT("failed to retrieve media event pointer") );
        return TPR_FAIL;
    }

    // This test pass NULL as the parameter.
    // THis test is under #ifdef TEST_NT_DSHOW_BUGS and nerver got hit in old test framework dshow1/3
    // So make it WARNING here but not jump to make other tests be run.
    if ( S_OK == pGraphBuilder->SetLogFile(NULL) )
    {
        LOG( TEXT("%S: WARNING %d@%S - IGraphBuilder::SetLogFile with NULL should fail. This is under #ifdef TEST_NT_DSHOW_BUGS in old test framework"),
                    __FUNCTION__, __LINE__, __FILE__ );
        goto cleanup;
    }

    // This test passes a valid handle (Not a valid file handle..
    long evHandle;

    // Get the event handle and pass it to the SetLogFile method.
    hr = pMediaEvent->GetEventHandle( &evHandle );
    hr = pGraphBuilder->SetLogFile( (HANDLE)evHandle );
    if ( SUCCEEDED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - SetLogFile with a valid handle, but not a file handle, should fail."),
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
    }
    //BuildGraph should not except
    try
    {
        hr = pTestGraph->BuildGraph();
        if(FAILED(hr))
        {
            LOG(TEXT("%S: WARNING %d@%S - BuildGraph should pass, and no log file should be genereated, but it failed with hr 0x%0x."),
                      __FUNCTION__, __LINE__, __FILE__ );
        }
    }
    catch(...)
    {
        LOG( TEXT("%S: ERROR %d@%S - BuildGraph should fail gracefully and expect, which invalid file handle in SetLogFile."),
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
    }

    // BUGBUG: should be configurable.
    rand_s(&uNumber);
    int count = uNumber % 50 + 10;
    HANDLE fileHandle;

    // Repeatedly call the SetLogFile with valid handle.
    while ( count-- )
    {
        // This test passes a vaild file handle.
        fileHandle = CreateFile( TEXT("myfile"),
                                 GENERIC_WRITE,
                                 (DWORD)0,
                                 NULL,
                                 CREATE_ALWAYS,
                                 (DWORD)0,
                                 NULL );

        if ( INVALID_HANDLE_VALUE == fileHandle)
        {
            LOG( TEXT("%S: ERROR %d@%S - CreateFile returned invalid handle."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            goto cleanup;
        }
        hr = pGraphBuilder->SetLogFile( fileHandle );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetLogFile with a valid handle failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            CloseHandle(fileHandle);
            goto cleanup;
        }

        // Before closing the file handle, call SetLogFile with a NULL file handle. This will
        // ensure that the component does not attempt to use the file handle after you have closed it
        hr = pGraphBuilder->SetLogFile(NULL);
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IGraphBuilder::SetLogFile with NULL failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            CloseHandle( fileHandle );
            goto cleanup;
        }

        CloseHandle( fileHandle );
    }

cleanup:

    if ( pGraphBuilder )
        pGraphBuilder->Release();

    return retval;
}



TESTPROCAPI ManualGraphBuildTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    return TPR_SKIP;
}

#if 0
TESTPROCAPI ManualGraphBuildTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    // Handle tux messages
    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    // From the config, determine the media file to be used
    Media* pMedia = pTestDesc->GetMedia(0);
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

    // Construct the requested graph
    if (retval == TPR_PASS)
    {
        GraphDesc* pGraph = &pTestDesc->graphDesc;

        if (pGraph)
        {
            // Construct the graph
            int nConnections = pGraph->GetNumConnections();
            for(int i = 0; i < nConnections; i++)
            {
                ConnectionDesc* pConnection = pGraph->GetConnection(i);
                FilterDesc* pFromFilter = pConnection->GetFromFilter();
                FilterDesc* pToFilter = pConnection->GetToFilter();
                int iPinFrom = pConnection->GetFromPin();
                int iPinTo = pConnection->GetToPin();
                TCHAR szNewFilterName[FILTER_NAME_LENGTH];

                // Add the "from" filter
                hr = testGraph.AddFilter(pFromFilter->GetId(), szNewFilterName, sizeof(szNewFilterName));
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to add specified filter %s (0x%x) to the graph"), __FUNCTION__, __LINE__, __FILE__, pFromFilter->GetName(), hr);
                    retval = TPR_FAIL;
                    break;
                }

                // Add the "to" filter
                hr = testGraph.AddFilter(pToFilter->GetId(), szNewFilterName, sizeof(szNewFilterName));
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to add specified filter %s (0x%x) to the graph"), __FUNCTION__, __LINE__, __FILE__, pToFilter->GetName(), hr);
                    retval = TPR_FAIL;
                    break;
                }

                // Connect the two filters
                hr = testGraph.Connect(pFromFilter->GetName(), iPinFrom, pToFilter->GetName(), iPinTo);
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to connect filter %s to filter %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pFromFilter->GetName(), pToFilter->GetName(), hr);
                    retval = TPR_FAIL;
                    break;
                }
            }
        }
        else {
            LOG(TEXT("%S: ERROR %d@%S - unable to retrieve graph for manual construction"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: successfully constructed the graph manually"), __FUNCTION__);
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}
#endif
