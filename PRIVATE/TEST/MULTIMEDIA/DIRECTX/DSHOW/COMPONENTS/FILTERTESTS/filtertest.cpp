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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include <windows.h>
#include "FilterTest.h"
#include "TestGraph.h"
#include "TestInterfaces.h"
#include "DummySource.h"
#include "DummySink.h"
#include "common.h"

// BUGBUG:  should we verify the return values of the query direct calls?

HRESULT GetPinArray(IBaseFilter* pFilter, IPin*** pppPins, int *pNumPins, PIN_DIRECTION pinDir)
{
    IEnumPins* pEnumPins = NULL;
    HRESULT hr = S_OK;
    int nPins = 0;

    if (FAILED(hr = pFilter->EnumPins(&pEnumPins))) {
       LOG(TEXT("GetPinArray: Failed to enumerate the pins."));
       return hr;
    }
    
    if (FAILED(hr = pEnumPins->Reset())) {
        LOG(TEXT("GetPinArray: Failed to reset the enumerated pins."));
        pEnumPins->Release();
        return hr;
    }
        
    PIN_DIRECTION dir;
    IPin *pPin = NULL;
    ULONG cPins = 0;

    while ((hr = pEnumPins->Next(1, &pPin, 0)) == S_OK) {
        pPin->QueryDirection(&dir);
        if (dir == pinDir)
            nPins++;
        pPin->Release();
    }
    
    // Next returns S_FALSE which is a success at the end
    if (pppPins && nPins && SUCCEEDED(hr)) {
        hr = S_OK;
        IPin** ppPins = (IPin**) new PPIN[nPins];
        if (!ppPins)
        {
            LOG(TEXT("GetPinArray: Failed to allocate the PPin array."));
            hr = E_OUTOFMEMORY;
        }
        else {
            *pppPins = ppPins;
            
            pEnumPins->Reset();
            int i = 0;
            while ((hr = pEnumPins->Next(1, &pPin, 0)) == S_OK) {
                pPin->QueryDirection(&dir);
                if (dir == pinDir) {
                    ppPins[i] = pPin;
                    i++;
                }
                else pPin->Release();
            }
            hr = S_OK;
        }
    }
    
    if (pEnumPins)
        pEnumPins->Release();

    *pNumPins = (SUCCEEDED(hr)) ? nPins : 0;

    return hr;
}

TESTPROCAPI FilterCreate( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;

    // Create an empty test graph
    LOG(TEXT("FilterCreate: creating test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();    
    if (!pTestGraph) {
        LOG(TEXT("FilterCreate: No pTestGraph given."));
        return TPR_FAIL;
    }

    // Ask for a filter - results in the filter being created and addreffed
    LOG(TEXT("FilterCreate: calling GetFilter."));
    IBaseFilter* pFilter = pTestGraph->GetFilter();
    if (!pFilter) {
        LOG(TEXT("FilterCreate: GetFilter failed."));
        retval = TPR_FAIL;
    }
    
    // Release reference on the filter created.
    LOG(TEXT("FilterCreate: releasing filter."));
    if (pFilter)
        pFilter->Release();
    
    // Release the filter - results in the created filter being released
    LOG(TEXT("FilterCreate: deleting test graph."));
    delete pTestGraph;
    return retval;
}

TESTPROCAPI PinEnumerate( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    
    // Create an empty test graph
    LOG(TEXT("PinEnumerate: creating the test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("PinEnumerate: No pTestGraph given."));
        return TPR_FAIL;
    }

    int nInputPins = 0;
    int nOutputPins = 0;

    // Ask for a filter - results in the filter being created and addreffed
    LOG(TEXT("PinEnumerate: querying the test graph for the filter under test."));
    IBaseFilter* pFilter = pTestGraph->GetFilter();
    if (!pFilter) {
        LOG(TEXT("PinEnumerate: GetFilter failed."));
        retval = TPR_FAIL;
    }
    else {

        //Need to add filters to graph, some filters will not expose a pin unless they have been initialized
        HRESULT hr = pTestGraph->AddFiltersToGraph();
        if(FAILED(hr)) {
                LOG(TEXT("PinEnumerate: There was a problem in AddFiltersToGraph() hr: 0x%0xd."), hr);
                retval = TPR_FAIL;
        }
        else {
        
            // The enumerator for the pins
            IEnumPins *pEnum = NULL;

            // Get the enumerator
            hr = pFilter->EnumPins(&pEnum);
            if (FAILED(hr) || !pEnum) {
                LOG(TEXT("PinEnumerate: IBaseFilter::EnumPins failed."));
                retval = TPR_FAIL;
            }
            else {
                // Reset the enumerator
                hr = pEnum->Reset();
                if (FAILED(hr))
                {
                    LOG(TEXT("PinEnumerate: IEnumPins::Reset failed."));
                    retval = TPR_FAIL;
                }
                else {
                    // Traverse the enumerator, noting the direction and number of the pins
                    IPin* pPin;
                    PIN_DIRECTION pinDir;

                    LOG(TEXT("PinEnumerate: enumerating pins..."));
                    while(pEnum->Next(1, &pPin, 0) == S_OK)
                    {
                        if (!pPin || FAILED(pPin->QueryDirection(&pinDir))) {
                            LOG(TEXT("PinEnumerate: QueryDirection failed, or no pin enumerated."));
                            retval = TPR_FAIL;
                        }
                        else {
                            if (pinDir == PINDIR_INPUT)
                            {
                                LOG(TEXT("PinEnumerate: enumerated input pin."));
                                nInputPins++;
                            }
                            else
                            {
                                LOG(TEXT("PinEnumerate: enumerated output pin."));
                                nOutputPins++;
                            }
                        }
                        if (pPin)
                            pPin->Release();
                    }
                }
            }        
            if (pEnum)
                pEnum->Release();
        }
    }
    
    if (retval == TPR_PASS)
    {
        int nActualPins = 0;

        nActualPins = pTestGraph->GetNumFilterInputPins();
        // Now verify if enumeration was right - the graph might return -1 if it was not sure the exact number of pins
        if (nActualPins == -1)
        {
            LOG(TEXT("PinEnumerate: test graph does not specify input pin count - skipping verification."));
        }
        else {
            LOG(TEXT("PinEnumerate: test graph specifies %d input pins."), nActualPins);
            if (nActualPins != nInputPins)
            {
                LOG(TEXT("PinEnumerate: input pin count from test graph doesn't match enumerated pin count."));
                retval = TPR_FAIL;
            }
        }

        nActualPins = pTestGraph->GetNumFilterOutputPins();
        if (nActualPins == -1)
        {
            LOG(TEXT("PinEnumerate: test graph does not specify output pin count - skipping verification."));
        } 
        else {
            LOG(TEXT("PinEnumerate: test graph specifies %d output pins."), nActualPins);
            if (nOutputPins != nActualPins) {
                LOG(TEXT("PinEnumerate: output pin count from test graph doesn't match enumerated pin count."));
                retval = TPR_FAIL;
            }
        }
    }

    // Release the refcount on the filter
    if (pFilter)
        pFilter->Release();

    // Ask the test graph to release its resources
    delete pTestGraph;

    return retval;
}

TESTPROCAPI QueryFilterInfo(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    
    // Create an empty test graph
    LOG(TEXT("QueryFilterInfo: creating the test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
            LOG(TEXT("QueryFilterInfo: No pTestGraph given."));
            return TPR_FAIL;
    }
    
    // Ask for a filter - results in the filter being created and addreffed
    LOG(TEXT("QueryFilterInfo: querying the test graph for the filter under test."));
    IBaseFilter* pFilter = pTestGraph->GetFilter();
    if (!pFilter) {
        LOG(TEXT("QueryFilterInfo: GetFilter failed"));
        retval = TPR_FAIL;
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("QueryFilterInfo: adding filters to graph."));
        HRESULT hr = pTestGraph->AddFiltersToGraph();
        if (FAILED(hr)) {
            LOG(TEXT("QueryFilterInfo: Failed to add filters to the graph."));
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("QueryFilterInfo: getting the IGraphBuilder interface."));
            IGraphBuilder* pGraphBuilder = pTestGraph->GetFilterGraphBuilder();
            if (pGraphBuilder) {
                IFilterGraph* pFilterGraph = NULL;
                LOG(TEXT("QueryFilterInfo: querying for the IFilterGraph interface."));
                pGraphBuilder->QueryInterface(IID_IFilterGraph, (void**)&pFilterGraph);
                if (pFilterGraph) {
                    FILTER_INFO info;
                    if (FAILED(pFilter->QueryFilterInfo(&info)) || (info.pGraph != pFilterGraph)) {
                        LOG(TEXT("QueryFilterInfo: Unable to query filter info, or info incorrect."));
                        retval = TPR_FAIL;
                    }
                    else {
                        LOG(TEXT("QueryFilterInfo: filter info seems correct."));
                    }
                    // Release the ref counted graph object returned in the info object
                    if (info.pGraph)
                        info.pGraph->Release();
                    pFilterGraph->Release();
                }
                else {
                    LOG(TEXT("QueryFilterInfo: failed to retrieve the filter graph interface."));
                    retval = TPR_FAIL;
                }
                pGraphBuilder->Release();
            }
            else {
                LOG(TEXT("QueryFilterInfo: failed to retrieve the IGraphBuilder interface."));
                retval = TPR_FAIL;
            }

        }
    }

    if (pFilter)
        pFilter->Release();

    delete pTestGraph;

    return retval;
}

TESTPROCAPI QueryPinAcceptTest(IPin* pPin)
{
    HRESULT hr = NOERROR;
    DWORD retval = TPR_PASS;

    // Enumerator for the media types
    IEnumMediaTypes *pEnumMediaTypes = NULL;

    hr = pPin->EnumMediaTypes(&pEnumMediaTypes);
    if (FAILED(hr)) {
        LOG(TEXT("QueryPinAcceptTest: Failed to enumerate the media types."));
        retval = TPR_FAIL;
    }
    else {
        hr = pEnumMediaTypes->Reset();    
        if (FAILED(hr)) {
            LOG(TEXT("QueryPinAcceptTest: Failed to reset the enumerated media types."));
            retval = TPR_FAIL;
        }
        else {
            ULONG nMediaTypes = 0;
            while(1) {
                AM_MEDIA_TYPE *pMediaType = NULL;
                // Query the enumerator for the media types it accepts
                hr = pEnumMediaTypes->Next(1, &pMediaType, &nMediaTypes);
                if (SUCCEEDED(hr) && pMediaType && (nMediaTypes == 1)) {
                    // Query the pin if it accepts the media type: it should, otherwise call out error
                    hr = pPin->QueryAccept(pMediaType);
                    DeleteMediaType(pMediaType);
                    pMediaType = NULL;
                    if (FAILED(hr) || (hr == S_FALSE)) {
                        LOG(TEXT("QueryPinAcceptTest: QueryAccept failed for the given media type."));
                        retval = TPR_FAIL;
                        break;
                    }
                }
                else {
                    // Next returns S_FALSE (which is a success) when it goes to the end
                    if (hr != S_FALSE) 
                        retval = TPR_FAIL;
                    break;
                }
            }
        }
        pEnumMediaTypes->Release();
    }
    return retval;
}

TESTPROCAPI QueryAcceptTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE_TPM_EXECUTE();
    HRESULT hr = NOERROR;
    DWORD retval = TPR_PASS;
    
    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("QueryAcceptTest: No pTestGraph given."));
        return TPR_FAIL;
    }

    // Ask for a filter - results in the filter being created and addreffed
    IBaseFilter* pFilter = pTestGraph->GetFilter();
    // Enumerator for the pins
    IEnumPins *pEnumPins = NULL;
    if (!pFilter) {
        LOG(TEXT("QueryAcceptTest: GetFilter failed"));
        retval = TPR_FAIL;
    }
    else {
        hr = pFilter->EnumPins(&pEnumPins);
        if (FAILED(hr) || !pEnumPins) {
            LOG(TEXT("QueryAcceptTest: Failed to enumerate the pins, or pin enumerated is invalid."));
            retval = TPR_FAIL;
        }
        else {
            ULONG nPins = 0;
            while(1) {
                IPin* pPin = NULL;
                // Query the enumerator for pins
                hr = pEnumPins->Next(1, &pPin, &nPins);
                if (SUCCEEDED(hr) && (nPins == 1)) {
                    if (QueryPinAcceptTest(pPin) != TPR_PASS) {
                        LOG(TEXT("QueryAcceptTest: QueryPinAcceptTestFailed."));
                        retval = TPR_FAIL;
                    }
                    if (pPin)
                        pPin->Release();
                }
                else {
                    // Next returns S_FALSE (which is a success) when it goes to the end
                    if (hr != S_FALSE)
                        retval = TPR_FAIL;
                    if (pPin)
                        pPin->Release();
                    break;
                }
            }
        }
    }

    if (pEnumPins)
        pEnumPins->Release();

    if (pFilter)
        pFilter->Release();

    if (pTestGraph)
        delete pTestGraph;

    return retval;
}

TESTPROCAPI GraphConnectDisconnect(CTestGraph* pTestGraph, ITestSource* pTestSource, ITestSink* pTestSink)
{
    DWORD retval = TPR_PASS;
    HRESULT hrConnect = NOERROR;
    HRESULT hrDisconnect = NOERROR;

    if (!pTestGraph) {
        LOG(TEXT("GraphConnectDisconnect: No pTestGraph given."));
        return TPR_FAIL;
    }

    // If this filter supports both input and output connections, try both connection orders -
    // but failure of a connection would mean failure of the test only if that connection order was supported. 
    // Similarly, success of a connection would mean test success only if that connection order was supported.
    
    ConnectionOrder connorder = pTestGraph->GetConnectionOrder();

    // Try InputFirst
    if (pTestSource) {
        LOG(TEXT("GraphConnectDisconnect: connecting source."));
        hrConnect = pTestGraph->ConnectSource();
        if ((FAILED(hrConnect) && (connorder == InputFirst)) || 
            (SUCCEEDED(hrConnect) && (connorder == OutputFirst))) {
            LOG(TEXT("GraphConnectDisconnect: Failed to connect the source when InputFirst, or succeeded in connecting the source when OutputFirst."));
            retval = TPR_FAIL;
        }
    }
    
    // Now connect the sink - we are still doing the InputFirst first. 
    // Now if the source filter did not exist then this would amount to just connecting the sink.
    if ((retval == TPR_PASS) && pTestSink) {
        LOG(TEXT("GraphConnectDisconnect: connecting sink."));
		hrConnect = pTestGraph->ConnectSink();
        // Disconnect starting with the sink
        if (FAILED(hrConnect)) {
            LOG(TEXT("GraphConnectDisconnect: Failed to connect the sink."));
            retval = TPR_FAIL;
        }
        hrDisconnect = pTestGraph->DisconnectSink();
        if ((retval == TPR_PASS) && FAILED(hrDisconnect)) {
            LOG(TEXT("GraphConnectDisconnect: Failed to disconnect the sink."));
            retval = TPR_FAIL;
        }
    }

    // Now disconnect the source
    if (pTestSource) {
        hrDisconnect = pTestGraph->DisconnectSource();
        if ((retval == TPR_PASS) && FAILED(hrDisconnect)) {
            LOG(TEXT("GraphConnectDisconnect: Failed to disconnect the source."));
            retval = TPR_FAIL;
        }
    }

    // Now try the other way - OutputFirst. If either TestSource or TestSink did not exist, we are done with the test
    if (pTestSource && pTestSink) {
        LOG(TEXT("GraphConnectDisconnect: reversing the order of connection - sink first."));
        hrConnect = pTestGraph->ConnectSink();
        if ((FAILED(hrConnect) && (connorder == OutputFirst)) || 
            (SUCCEEDED(hrConnect) && (connorder == InputFirst))) {
            LOG(TEXT("GraphConnectDisconnect: failed to connect the sink when output first, or succeeded when input first."));
            retval = TPR_FAIL;
        }
        else {
            if (FAILED(hrConnect))
                LOG(TEXT("GraphConnectDisconnect: connect sink was meant to fail and it did."));
        }
    
        // Now connect the source only if connecting the sink succeeded and it was meant to
        if ((retval == TPR_PASS) && SUCCEEDED(hrConnect)) {
            LOG(TEXT("GraphConnectDisconnect: connecting source now."));
            hrConnect = pTestGraph->ConnectSource();
            // Disconnect starting with the source
            if (FAILED(hrConnect)) {
                LOG(TEXT("GraphConnectDisconnect: failed to connect the source."));
                retval = TPR_FAIL;
            }
            LOG(TEXT("GraphConnectDisconnect: disconnecting source."));
            hrDisconnect = pTestGraph->DisconnectSource();
            if ((retval == TPR_PASS) && FAILED(hrDisconnect)) {
                LOG(TEXT("GraphConnectDisconnect: failed to disconnect the source."));
                retval = TPR_FAIL;
            }
        }

        // Now disconnect the sink
        LOG(TEXT("GraphConnectDisconnect: disconnecting sink."));
        hrDisconnect = pTestGraph->DisconnectSink();
        if ((retval == TPR_PASS) && FAILED(hrDisconnect)) {
            LOG(TEXT("GraphConnectDisconnect: failed to disconnect the sink."));
            retval = TPR_FAIL;
        }
    }
    return retval;
}

// This test ensures there is atleast one way of connecting the source, filter and sink
TESTPROCAPI GraphConnectDisconnect( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;

    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("GraphConnectDisconnect: No pTestGraph given."));
        return TPR_FAIL;
    }

    ITestSource* pTestSource = pTestGraph->GetTestSource();
    ITestSink* pTestSink = pTestGraph->GetTestSink();

    if (!pTestSource && !pTestSink) {
        LOG(TEXT("GraphConnectDisconnect: No test source or sink given."));
        retval = TPR_FAIL;
    }
    else {
        // Allow all supported media types to be used in the connect
        if (pTestSource)
            pTestSource->SelectMediaTypeCombination(-1);

        if (pTestSink)
            pTestSink->SelectMediaTypeCombination(-1);


        // Run the non validated connect disconnect test, GraphConnectDisconnect outputs the appropriate
        // failure information.
        retval = GraphConnectDisconnect(pTestGraph, pTestSource, pTestSink);

        if (pTestSource)
            pTestSource->Release();

        if (pTestSink)
            pTestSink->Release();
    }

    delete pTestGraph;
    
    return retval;
}

// This test checks that each supported combination of source and sink type either succeed or fail
TESTPROCAPI ValidatedGraphConnectDisconnect( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    HRESULT hr = NOERROR;

    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("ValidatedGraphConnectDisconnect: No pTestGraph given."));
        return TPR_FAIL;
    }

    // The key to this test is that 
    // - the test graph should be able to validate the media types used to connect
    // - the plain GraphConnectDisconnect just ensures that there exists atleast one way to connect

    ITestSource* pTestSource = pTestGraph->GetTestSource();
    ITestSink* pTestSink = pTestGraph->GetTestSink();

    if (!pTestSource && !pTestSink) {
        LOG(TEXT("ValidatedGraphConnectDisconnect: No test source or sink given."));
        retval = TPR_FAIL;
    }

    MediaTypeCombination mtc;
    memset(&mtc, 0, sizeof(mtc));

    // Get the number of source outputs and sink inputs
    int nSourceOutputs = 0;
    IBaseFilter* pSourceFilter = pTestGraph->GetSourceFilter();
    if (pSourceFilter) {
        if(FAILED(GetPinArray(pSourceFilter, NULL, &nSourceOutputs, PINDIR_OUTPUT))) {
            LOG(TEXT("ValidatedGraphConnectDisconnect: Unable to retrieve the source pin array."));
            retval = TPR_FAIL;
        }
        pSourceFilter->Release();
    }

    int nSinkInputs = 0;
    IBaseFilter* pSinkFilter = pTestGraph->GetSinkFilter();
    if (pSinkFilter) {
        if(FAILED(GetPinArray(pSinkFilter, NULL, &nSinkInputs, PINDIR_INPUT))) {
            LOG(TEXT("ValidatedGraphConnectDisconnect: Unable to retrieve the sink pin array."));
            retval = TPR_FAIL;
        }
        pSinkFilter->Release();
    }

    // Allocate an array of media types
    AM_MEDIA_TYPE* pSelectedSourceType = NULL;
    if (nSourceOutputs) {
        pSelectedSourceType = new AM_MEDIA_TYPE[nSourceOutputs];
        if(pSelectedSourceType) {
            memset(pSelectedSourceType, 0, sizeof(AM_MEDIA_TYPE)*nSourceOutputs);
        }
        else {
            LOG(TEXT("ValidatedGraphConnectDisconnect: Unable to allocate source media types array."));
            retval = TPR_FAIL;
        }
    }
    
    AM_MEDIA_TYPE* pSelectedSinkType = NULL;
    if (nSinkInputs) {
        pSelectedSinkType = new AM_MEDIA_TYPE[nSinkInputs];
        if(pSelectedSinkType) {
            memset(pSelectedSinkType, 0, sizeof(AM_MEDIA_TYPE)*nSinkInputs);
        }
        else {
            LOG(TEXT("ValidatedGraphConnectDisconnect: Unable to allocate sink media types array."));
            retval = TPR_FAIL;
        }
    }

    // This is a 2-way loop over the source combinations and sink combinations
    // At the core of the loop is the test for connection validation
    int nCombinationsTried = 0;
    int nSourceCombinations = pTestSource ? pTestSource->GetNumMediaTypeCombinations() : 1;
    int nSinkCombinations = pTestSink ? pTestSink->GetNumMediaTypeCombinations() : 1;
    for(int iSourceCombination = 0; iSourceCombination < nSourceCombinations; iSourceCombination++)
    {
        for(int iSinkCombination = 0; iSinkCombination < nSinkCombinations; iSinkCombination++)
        {
            if (pTestSource)
                pTestSource->GetMediaTypeCombination(iSourceCombination, &pSelectedSourceType);
            if (pTestSink)
                pTestSink->GetMediaTypeCombination(iSinkCombination, &pSelectedSinkType);

            // Check with the graph if the media combination works ok
            ConnectionValidation validation = pTestGraph->ValidateMediaTypeCombination(&pSelectedSourceType, &pSelectedSinkType);

            // Now free  the format blocks before we proceed, we dont need them
            for(int i = 0; i < nSourceOutputs; i++)
                FreeMediaType(pSelectedSourceType[i]);
            for(int i = 0; i < nSinkInputs; i++)
                FreeMediaType(pSelectedSinkType[i]);

            // If the validation was unknown then just proceed
            if (validation == Unknown)
                continue;

            // Run the connect graph test with this enumerated combination
            if (pTestSource)
                pTestSource->SelectMediaTypeCombination(iSourceCombination);
            if (pTestSink)
                pTestSink->SelectMediaTypeCombination(iSinkCombination);

            // Now run the connect,disconnect test
            DWORD ret = GraphConnectDisconnect(pTestGraph, pTestSource, pTestSink);
            if ((ret == TPR_FAIL) && (validation == Supported) ||
                (ret == TPR_PASS) && (validation == Unsupported)) {
                LOG(TEXT("ValidatedGraphConnectDisconnect: GraphConnectDisconnect succeeded on an unspported combination or failed on a supported combination."));
                retval = TPR_FAIL;
            }
            if (ret != TPR_SKIP)
                nCombinationsTried++;
        }
    }

    // Format blocks should already be deleted
    if (pSelectedSourceType)
        delete [] pSelectedSourceType;
    
    if (pSelectedSinkType)
        delete [] pSelectedSinkType;

    if (pTestSource)
        pTestSource->Release();
    
    if (pTestSink)
        pTestSink->Release();

    delete pTestGraph;
    
    // If we did not try even a single combination, skip the test
    if ((retval == TPR_PASS) && !nCombinationsTried)
        return TPR_SKIP;

    return retval;
}


TESTPROCAPI SourceConnectDisconnect( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    HRESULT hr = NOERROR;

    // Create an empty test graph
    LOG(TEXT("SourceConnectDisconnect: creating the test graph."));
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("SourceConnectDisconnect: No pTestGraph given."));
        return TPR_FAIL;
    }

    // If we dont have input pins or the wrong conenction order, then skip
    LOG(TEXT("SourceConnectDisconnect: checking for input pins."));
    if (pTestGraph->GetNumFilterInputPins() == 0)
    {
        LOG(TEXT("SourceConnectDisconnect: no input pins on the filter - skipping"));
        retval = TPR_SKIP;
    }
    else if (pTestGraph->GetConnectionOrder() == OutputFirst)
    {
        LOG(TEXT("SourceConnectDisconnect: the test graph specifies outputs should be connected first. Connecting the source will fail - skipping"));
        retval = TPR_SKIP;
    }
    else {
        // Connect and disconnect
        LOG(TEXT("SourceConnectDisconnect: requesting the test graph to connect the source filter."));
        hr = pTestGraph->ConnectSource();
        if (FAILED(hr)) {
            LOG(TEXT("SourceConnectDisconnect: ConnectSource failed."));
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("SourceConnectDisconnect: ConnectSource succeeded."));
            LOG(TEXT("SourceConnectDisconnect: requesting the test graph to disconnect any connected source pins."));
            hr = pTestGraph->DisconnectSource();
            if (FAILED(hr)) {
                LOG(TEXT("SourceConnectDisconnect: DisconnectSource failed."));
                retval = TPR_FAIL;
            }
            else {
                LOG(TEXT("SourceConnectDisconnect: DisconnectSource succeeded."));
            }
        }
    }
    delete pTestGraph;

    return retval;
}

TESTPROCAPI SinkConnectDisconnect( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    HRESULT hr = NOERROR;

    LOG(TEXT("SinkConnectDisconnect: creating test graph."));
    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("SinkConnectDisconnect: No pTestGraph given."));
        return TPR_FAIL;
    }

    // If we dont have output pins or if we have the wrong connection order, then skip
    LOG(TEXT("SinkConnectDisconnect: checking for output pins."));
    if (pTestGraph->GetNumFilterOutputPins() == 0)
    {
        LOG(TEXT("SinkConnectDisconnect: no output pins on the filter - skipping"));
        retval = TPR_SKIP;
    }
    else if (pTestGraph->GetConnectionOrder() == InputFirst)
    {
        LOG(TEXT("SinkConnectDisconnect: the test graph specifies that inputs should be connected first. Connecting the sink will fail - skipping"));
        retval = TPR_SKIP;
    }
    else {
        // Connect and disconnect
        LOG(TEXT("SinkConnectDisconnect: requesting the test graph to connect the sink filter."));
        // Connect and disconnect
        hr = pTestGraph->ConnectSink();
        if (FAILED(hr)) {
            LOG(TEXT("SinkConnectDisconnect: ConnectSink failed."));
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("SinkConnectDisconnect: ConnectSink succeeded."));
            LOG(TEXT("SinkConnectDisconnect: requesting the test graph to disconnect any connected sink pins."));
            hr = pTestGraph->DisconnectSink();
            if (FAILED(hr)) {
                LOG(TEXT("SinkConnectDisconnect: DisconnectSink failed."));
                retval = TPR_FAIL;
            }
            else {
                LOG(TEXT("SinkConnectDisconnect: DisconnectSink succeeded."));
            }
        }
    }

    delete pTestGraph;

    return retval;
}

void ReleasePinArray(IPin **ppPinArray, int nPins)
{
    if (!nPins || !ppPinArray)
        return;
    for(int i = 0; i < nPins; i++)
    {
        ppPinArray[i]->Release();
    }
    delete ppPinArray;
}

HRESULT ConnectDirect(IGraphBuilder* pGraph, int nOutputPins, IPin** ppOutputPinArray, IPin** ppInputPinArray)
{
    HRESULT hr = NOERROR;
    for(int i = 0; i < nOutputPins; i++)
    {
        hr = pGraph->ConnectDirect(ppOutputPinArray[i], ppInputPinArray[i], NULL);
        if (FAILED(hr))
            return hr;
    }
    return S_OK;
}

HRESULT Disconnect(IGraphBuilder* pGraph, int nOutputPins, IPin** ppOutputPinArray)
{
    HRESULT hr = NOERROR;
    for(int i = 0; i < nOutputPins; i++) {
        IPin *pConnectedPin = NULL;
        ppOutputPinArray[i]->ConnectedTo(&pConnectedPin);
        if (pConnectedPin) {
            HRESULT hrPin = NOERROR;
            hrPin = pGraph->Disconnect(ppOutputPinArray[i]);
            if (SUCCEEDED(hr) && FAILED(hrPin))
                hr = hrPin;
            hrPin = pGraph->Disconnect(pConnectedPin);
            if (SUCCEEDED(hr) && FAILED(hrPin))
                hr = hrPin;
            pConnectedPin->Release();
        }
    }
    return hr;
}

HRESULT ConnectDisconnectFilters(CTestGraph* pTestGraph,  ConnectionOrder connorder, int nInputs, int nOutputs, IBaseFilter* pSource, IBaseFilter* pFilter, IBaseFilter* pSink)
{
    HRESULT hr = NOERROR;
    HRESULT hrPin = NOERROR;

    if (!pFilter)
        return E_POINTER;

    int nFilterInputPins, nFilterOutputPins, nSourceOutputPins, nSinkInputPins;

    nFilterInputPins = nFilterOutputPins = nSourceOutputPins = nSinkInputPins = 0;
    IPin **ppFilterInputPinArray = NULL;
    IPin **ppFilterOutputPinArray = NULL;
    IPin **ppSourceOutputPinArray = NULL;
    IPin **ppSinkInputPinArray = NULL;

    IGraphBuilder* pGraph = NULL;
    
    LOG(TEXT("ConnectDisconnectFilters: creating filter graph manager."));

    pGraph = pTestGraph->GetFilterGraphBuilder();

    if (pSource) {
        hr = pGraph->AddFilter(pSource, NULL);

        if(FAILED(hr)) {
            LOG(TEXT("ConnectDisconnectFilters: Failed to add the source filter."));
        }
    }

    if (SUCCEEDED(hr) && pSink) {
        hr = pGraph->AddFilter(pSink, NULL);

        if(FAILED(hr)) {
            LOG(TEXT("ConnectDisconnectFilters: Failed to add the sink filter."));
        }
    }

    if (SUCCEEDED(hr)) {
        hr = pGraph->AddFilter(pFilter, NULL);

        if(FAILED(hr)) {
            LOG(TEXT("ConnectDisconnectFilters: Failed to add the transform filter."));
        }
        //Initialize filter if need be
        hr = pTestGraph->InitializeFilter();
        if (FAILED(hr))
            LOG(TEXT("AddFiltersToGraph: IGraphBuilder::InitializeFilter failed for filter under test."));
    }

    if (SUCCEEDED(hr)) {
        if (nInputs) {
            hrPin = GetPinArray(pFilter, &ppFilterInputPinArray, &nFilterInputPins, PINDIR_INPUT);
            if (FAILED(hrPin)) {
                LOG(TEXT("ConnectDisconnectFilters: GetPinArray for the transform input pins failed."));
                hr = hrPin;
            }
        }

        if (SUCCEEDED(hr) && nOutputs) {
            hrPin = GetPinArray(pFilter, &ppFilterOutputPinArray, &nFilterOutputPins, PINDIR_OUTPUT);
            if (FAILED(hrPin)) {
                LOG(TEXT("ConnectDisconnectFilters: GetPinArray for the transform output pins failed."));
                hr = hrPin;
            }
        }

        if ((nInputs != nFilterInputPins) || (nOutputs != nFilterOutputPins)) {
            LOG(TEXT("ConnectDisconnectFilters: Input or output pin count doesn't match the expected value."));
            hr = E_UNEXPECTED;
        }

        if (SUCCEEDED(hr) && pSource && nInputs) {
            hrPin = GetPinArray(pSource, &ppSourceOutputPinArray, &nSourceOutputPins, PINDIR_OUTPUT);
            if (FAILED(hrPin)) {
                LOG(TEXT("ConnectDisconnectFilters: Failed to retrieve the source output pins."));
                hr = hrPin;
            }
        }

        if (SUCCEEDED(hr) && pSink && nOutputs) {
            hrPin = GetPinArray(pSink, &ppSinkInputPinArray, &nSinkInputPins, PINDIR_INPUT);
            if (FAILED(hrPin)) {
                LOG(TEXT("ConnectDisconnectFilters: Failed to retrieve the sink input pins."));
                hr = hrPin;
            }
        }

        if ((nSinkInputPins != nFilterOutputPins) || (nSourceOutputPins != nFilterInputPins)) {
            LOG(TEXT("ConnectDisconnectFilters: unexpected mismatch of input(%d,%d))/output pin (%d,%d) counts."), nSinkInputPins, nFilterOutputPins, nSourceOutputPins, nFilterInputPins);
            hr = E_UNEXPECTED;
        }

        if (SUCCEEDED(hr)) {
            // If we have succeeded so far, on to the business of connecting
            if ((connorder == EitherFirst) || (connorder == InputFirst)) {
                // Connect the source first
                if (ppSourceOutputPinArray && ppFilterInputPinArray)
                    hr = ConnectDirect(pGraph, nSourceOutputPins, ppSourceOutputPinArray, ppFilterInputPinArray);
                if (SUCCEEDED(hr) && ppFilterOutputPinArray && ppSinkInputPinArray)
                    hr = ConnectDirect(pGraph, nFilterOutputPins, ppFilterOutputPinArray, ppSinkInputPinArray);        
            }
            else {
                // Reverse order of connection
                if (ppFilterOutputPinArray && ppSinkInputPinArray)
                    hr = ConnectDirect(pGraph, nFilterOutputPins, ppFilterOutputPinArray, ppSinkInputPinArray);        
                if (SUCCEEDED(hr) && ppSourceOutputPinArray && ppFilterInputPinArray)
                    hr = ConnectDirect(pGraph, nSourceOutputPins, ppSourceOutputPinArray, ppFilterInputPinArray);
            }

            if(FAILED(hr)) {
                LOG(TEXT("ConnectDisconnectFilters: ConnectDirect failed for the graph given."));
            }
            
            // Now onto disconnecting - the order shouldn't matter
            HRESULT hrDisconnect = NOERROR;
            if (ppFilterOutputPinArray && ppSinkInputPinArray) {
                hrDisconnect = Disconnect(pGraph, nFilterOutputPins, ppFilterOutputPinArray);
                if (SUCCEEDED(hr) && FAILED(hrDisconnect)) {
                    LOG(TEXT("ConnectDisconnectFilters: Filter output disconnection failed when expected to succeed."));
                    hr = hrDisconnect;
                }
            }
            if (ppSourceOutputPinArray && ppFilterInputPinArray) {
                hrDisconnect = Disconnect(pGraph, nSourceOutputPins, ppSourceOutputPinArray);
                if (SUCCEEDED(hr) && FAILED(hrDisconnect)) {
                    LOG(TEXT("ConnectDisconnectFilters: Source output disconnection failed when expected to succeed."));
                    hr = hrDisconnect;
                }
            }
        }

    LOG(TEXT("ConnectDisconnectFilters: releasing pins."));
        if (nSourceOutputPins)
            ReleasePinArray(ppSourceOutputPinArray, nSourceOutputPins);
        if (nSinkInputPins)
            ReleasePinArray(ppSinkInputPinArray, nSinkInputPins);
        if (nFilterInputPins)
            ReleasePinArray(ppFilterInputPinArray, nFilterInputPins);
        if (nFilterOutputPins)
            ReleasePinArray(ppFilterOutputPinArray, nFilterOutputPins);
    }

    LOG(TEXT("ConnectDisconnectFilters: removing filters from the graph."));
    if (pSource)
        pGraph->RemoveFilter(pSource);
    
    if (pSink)
        pGraph->RemoveFilter(pSink);
    
    if (pFilter)
        pGraph->RemoveFilter(pFilter);
    
    LOG(TEXT("ConnectDisconnectFilters: releasing the FGM."));
    if (pGraph)
        pGraph->Release();

    return hr;
}

TESTPROCAPI MediaCombinationTest(CTestGraph* pTestGraph,  ConnectionOrder connorder, IBaseFilter* pFilter, MediaTypeCombination* pCombination, bool bValid, bool bWildCard)
{
    HRESULT hr = NOERROR;
    DWORD retval = TPR_PASS;

     // Use the dummy source and sink for this test
    CDummySource* pSource = NULL;
    CDummySink* pSink = NULL;

    if (!pCombination || !pFilter) {
        LOG(TEXT("MediaCombinationTest: No filter, or media combination given."));
        return TPR_FAIL;
    }


    if (pCombination->nInputs)
    {
        pSource = pTestGraph->GetMediaCombTestSource(&hr, pCombination->nInputs);
        if (!pSource) {
            LOG(TEXT("MediaCombinationTest: Unable to allocate the dummy source."));
            return TPR_FAIL;
        }
        pSource->AddRef();
        // Iterate over pins
        for(int i = 0; i < pCombination->nInputs; i++) {
            // For this pin, there is only type supported.
            pSource->SetSupportedTypes(i, &pCombination->mtInput[i], 1, bWildCard);
        }
    }
    
    if (pCombination->nOutputs)
    {
        pSink = pTestGraph->GetMediaCombTestSink(&hr, pCombination->nOutputs);            
        if (!pSink) {
            if (pSource)
                delete pSource;
            LOG(TEXT("MediaCombinationTest: Unable to allocate the dummy sink."));
            return TPR_FAIL;
        }
        pSink->AddRef();
        // Iterate over pins
        for(int i = 0; i < pCombination->nOutputs; i++) {
            // For this pin, there is only type supported.
            pSink->SetSupportedTypes(i, &pCombination->mtOutput[i], 1, bWildCard);
        }
    }

    LOG(TEXT("MediaCombinationTest: Combination is expected to %s"), bValid?TEXT("Succeed"):TEXT("Fail"));
    
    // Now we have a source and sink with the appropriate media combinations
    // Conenct and disconnect them
    hr = ConnectDisconnectFilters(pTestGraph, connorder, pCombination->nInputs, pCombination->nOutputs, pSource, pFilter, pSink);

    if (SUCCEEDED(hr))
    {
        if (bValid)
            LOG(TEXT("MediaCombinationTest: ConnectDisconnectFilters succeeded."));
        else {
            LOG(TEXT("MediaCombinationTest: ConnectDisconnectFilters succeeded when expected to fail."));
            retval = TPR_FAIL;
        }
    }
    else {
        if (bValid) {
            LOG(TEXT("MediaCombinationTest: ConnectDisconnectFilters failed when expected to succeed."));
            retval = TPR_FAIL;
        }
        else
            LOG(TEXT("MediaCombinationTest: ConnectDisconnectFilters failed and it was expected to fail - so PASS."));
    }

    if (pSource)
        delete pSource;

    if (pSink)
        delete pSink;
    return retval;
}

TESTPROCAPI ValidMediaCombination( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    HRESULT hr = NOERROR;

    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("ValidMediaCombination: No pTestGraph given."));
        return TPR_FAIL;
    }

    IBaseFilter* pFilter = pTestGraph->GetFilter();
    if (!pFilter) {
        LOG(TEXT("ValidMediaCombination: Unable to retrieve the filter."));
        delete pTestGraph;
        return TPR_FAIL;
    }

    // Get the media type combinations supported
    int nCombinations = pTestGraph->GetNumMediaTypeCombinations();

    if (!nCombinations) {
        LOG(TEXT("ValidMediaCombination: No combinations given."));
        retval = TPR_SKIP;
    }

    LOG(TEXT("ValidMediaCombination: Testing %d combinations"), nCombinations);

    // We will not use the graph for this test.
    bool valid = false;
    ConnectionOrder connorder = pTestGraph->GetConnectionOrder();

    for(int i = 0; i < nCombinations; i++)
    {
        LOG(TEXT("ValidMediaCombination: Testing combination %d"), i);
        // This pointer is a private pointer from within the test graph - dont delete it!
        MediaTypeCombination *pCombination = pTestGraph->GetMediaTypeCombination(i, valid);

    LOG(TEXT("ValidMediaCombination: got combination: %d inputs, %d outputs"), pCombination->nInputs, pCombination->nOutputs);
        if (pCombination) {
            // Currently only doing the non-wildcard test - we could also allow wildcard matches for the media types.
            if ((connorder == InputFirst) || (connorder == EitherFirst))
            {
                if (MediaCombinationTest(pTestGraph, InputFirst, pFilter, pCombination, valid, false) != TPR_PASS) {
                    LOG(TEXT("ValidMediaCombination: InputFirst MediaCombinationTest failed."));
                    retval = TPR_FAIL;
                    break;
                }
            }
            else {
                if (MediaCombinationTest(pTestGraph, OutputFirst, pFilter, pCombination, valid, false) != TPR_PASS) {
                    LOG(TEXT("ValidMediaCombination: OutputFirst MediaCombinationTest failed."));
                    retval = TPR_FAIL;
                    break;
                }
            }
        }
    }

    LOG(TEXT("ValidMediaCombination: releasing filter."));
    pFilter->Release();

    LOG(TEXT("ValidMediaCombination: deleting test graph."));
    delete pTestGraph;

    return retval;
}


HRESULT SetState(IMediaControl* pMediaControl, FILTER_STATE toState)
{
    FILTER_STATE state;
    HRESULT hr;
    switch(toState)
    {
    case State_Stopped:
        hr = pMediaControl->Stop();
        break;
    case State_Running:
        hr = pMediaControl->Run();
        break;
    case State_Paused:
        hr = pMediaControl->Pause();
        break;
    }
    
    // Wait for the state change to be completed even if hr is S_OK, 8 secs seems reasonable
    hr = pMediaControl->GetState(8000, (OAFilterState*)&state);
    if (((state == toState) && (hr == S_OK)) || 
        ((state == State_Paused) && (hr == VFW_S_CANT_CUE)))
	{
        return S_OK;
    }
	else if (hr == VFW_S_STATE_INTERMEDIATE)
	{
		return S_FALSE;
	}
    else {
        LOG(TEXT("SetState: GetState failed."));
        return E_FAIL;
    }
}

TESTPROCAPI StateTest(IMediaControl* pFilterGraph, FILTER_STATE state)
{
    HRESULT hr = NOERROR;

    hr = SetState(pFilterGraph, state);
    if (FAILED(hr)) {
        LOG(TEXT("StateTest: Failed to set the state."));
        return TPR_FAIL;
    }

    return TPR_PASS;
}

TESTPROCAPI StopTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    HRESULT hr = NOERROR;

    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("StopTest: No pTestGraph given."));
        return TPR_FAIL;
    }

    // Connect in the order specified
    ConnectionOrder connorder = pTestGraph->GetConnectionOrder();
    connorder = (connorder == EitherFirst) ? InputFirst : connorder;
    hr = pTestGraph->ConnectGraph(connorder);
    if (FAILED(hr)) {
        LOG(TEXT("StopTest: Failed to connect the graph."));
        retval = TPR_FAIL;
    }
    else {
        IMediaControl* pMediaControl = pTestGraph->GetMediaControlGraph();
        if (!pMediaControl) {
            LOG(TEXT("StopTest: Unable to retrieve the media control."));
            retval = TPR_FAIL;
        }
        else {
            if (StateTest(pMediaControl, State_Stopped) != TPR_PASS) {
                LOG(TEXT("StopTest: Unable to set state to stopped."));
                retval = TPR_FAIL;
            }
        }
        if (pMediaControl)
            pMediaControl->Release();
    }

    hr = pTestGraph->DisconnectGraph();
    if (FAILED(hr))
        retval = TPR_FAIL;

    delete pTestGraph;

    return retval;
}

TESTPROCAPI PauseTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    HRESULT hr = NOERROR;

    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("PauseTest: No pTestGraph given."));
        return TPR_FAIL;
    }

    // Connect 
    LOG(TEXT("PauseTest: connecting graph."));
    hr = pTestGraph->ConnectGraph();
    if (FAILED(hr)) {
        LOG(TEXT("PauseTest: Unable to connect graph."));
        retval = TPR_FAIL;
    }
    else {
        IMediaControl* pMediaControl = pTestGraph->GetMediaControlGraph();
        if (!pMediaControl) {
            LOG(TEXT("PauseTest: Unable to retrieve the media control."));
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("PauseTest: pausing graph."));
            if (StateTest(pMediaControl, State_Paused) != TPR_PASS) {
                LOG(TEXT("PauseTest: Unable to pause graph."));
                retval = TPR_FAIL;
            }

            if (StateTest(pMediaControl, State_Stopped) != TPR_PASS) {
                LOG(TEXT("PauseTest: Unable to stop graph."));
                retval = TPR_FAIL;
            }
        }
        if (pMediaControl)
            pMediaControl->Release();
    }

    hr = pTestGraph->DisconnectGraph();
    if (FAILED(hr)) {
        LOG(TEXT("PauseTest: Unable to disconnect graph."));
        retval = TPR_FAIL;
    }

    delete pTestGraph;

    return retval;
}

TESTPROCAPI RunTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    HRESULT hr = NOERROR;

    LOG(TEXT("RunTest: no streaming."));
    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("RunTest: No pTestGraph given."));
        return TPR_FAIL;
    }

    // Connect 
    hr = pTestGraph->ConnectGraph();
    if (FAILED(hr)) {
        LOG(TEXT("RunTest: Unable to connect graph."));
        retval = TPR_FAIL;
    }
    else {
    LOG(TEXT("RunTest: Connected graph."));
        IMediaControl* pMediaControl = pTestGraph->GetMediaControlGraph();
        if (!pMediaControl) {
            LOG(TEXT("RunTest: Unable to retrieve the media control."));
            retval = TPR_FAIL;
        }
        else {
             LOG(TEXT("RunTest: Running state test."));
            if (StateTest(pMediaControl, State_Running) != TPR_PASS) {
                LOG(TEXT("RunTest: Failed to change the graph state to running."));
                retval = TPR_FAIL;
            }

            if (StateTest(pMediaControl, State_Stopped) != TPR_PASS) {
                LOG(TEXT("RunTest: Failed to change the graph state to stopped."));
                retval = TPR_FAIL;
            }
        }

    LOG(TEXT("RunTest: releasing IMediaControl."));
        if (pMediaControl)
            pMediaControl->Release();
    }

    hr = pTestGraph->DisconnectGraph();
    if (FAILED(hr)) {
        LOG(TEXT("RunTest: Failed to disconnect graph."));
        retval = TPR_FAIL;
    }

    delete pTestGraph;

    return retval;
}


TESTPROCAPI StreamingVerification(CTestGraph* pTestGraph, StreamingMode mode, int nUnits, StreamingFlags flags, TCHAR* datasource)
{
    HRESULT hr = NOERROR;
    DWORD retval = TPR_PASS;
    
    // Get the test source and sink if any
    ITestSource* pTestSource = pTestGraph->GetTestSource();
    ITestSink* pTestSink = pTestGraph->GetTestSink();
    IVerifier* pVerifier = NULL;

    // If we had a test source or sink get the verifier
    if (pTestSource)
        pVerifier = pTestSource->GetVerifier();
    else if (pTestSink)
        pVerifier = pTestSink->GetVerifier();

    // If we had a verifier, set the streaming to begin.
    if (pVerifier)
    {
        pVerifier->Reset();
        pVerifier->StreamingBegin();
    }

    // If we had a test source then set the streaming flags. If we did not then there is no control over the source filter under test.
    // Use only the timeout.
    if (pTestSource)
    {
        if (datasource)
            pTestSource->SetDataSource(datasource);
        LOG(TEXT("StreamingVerification: streaming mode %s nUnits %d flags %s."), StreamingModeName(mode), nUnits, StreamingFlagName(flags));
        pTestSource->SetStreamingMode(mode, nUnits, flags);
    }
    else {
        // We only support continous mode and UntilEndOfStream if there's no source available
        if (!((mode == Continuous) && (flags == UntilEndOfStream)) && 
            !((mode == Continuous) && (flags == ForSpecifiedTime))) {
            LOG(TEXT("StreamingVerification: Only Continuous and UntilEndOfStream or ForSpecifiedTime modes supported."));
            return TPR_SKIP;
        }
    }

    // Get the media control interface and run the test graph.
    IMediaControl* pMediaControl = pTestGraph->GetMediaControlGraph();
    if (!pMediaControl) {
        LOG(TEXT("StreamingVerification: Unable to retrieve the media control."));
        return TPR_FAIL;
    }
    else {
        LOG(TEXT("StreamingVerification: Starting the graph running."));
        hr = pMediaControl->Run();
        if (FAILED(hr)) {
            LOG(TEXT("StreamingVerification: Unable to run the graph."));
            retval = TPR_FAIL;
        }
    }

    // Get the media event object
    IMediaEvent   *pEvent = pTestGraph->GetMediaEventGraph();
    if (!pEvent) {
        	LOG(TEXT("StreamingVerification: Unable to retrieve the media event."));
    }	

    // If there is a test source, start the streaming. If this was a source filter test, then streaming would have already started.
    if ((retval == TPR_PASS) && pTestSource) {
        LOG(TEXT("StreamingVerification: Beginning streaming test source."));
        hr = pTestSource->BeginStreaming();
        if (FAILED(hr)) {
            LOG(TEXT("StreamingVerification: BeginStreaming failed."));
            retval = TPR_FAIL;
        }
    }

    if (retval == TPR_PASS) {
        if (pTestSource) {
            // If we have a test source, run test until the source stops streaming
            while (pTestSource->IsStreaming())
                Sleep(10);
        }
        else if(flags == UntilEndOfStream) {
            LOG(TEXT("StreamingVerification: Streaming with no test source."));
		    if(pEvent) {
		     	// Wait for completion.
			   	long evCode;
				int i =0;
				do{
					hr = pEvent->WaitForCompletion(0xFFFFFF, &evCode);
					if(hr == S_OK) {
						if(EC_ERRORABORT==evCode){
							LOG(TEXT("StreamingVerification: We got back EC_ERRORABORT from WaitForCompletion. Something is wrong."));
							retval = TPR_FAIL;
						}
						break;
					}
					else if (hr == VFW_E_WRONG_STATE) {
						LOG(TEXT("StreamingVerification: We got back VFW_E_WRONG_STATE from WaitForCompletion. Something is wrong."));
						retval = TPR_FAIL;
						break;
					}
					i++;
				} while(i < nUnits);
        	}
			else
            	Sleep(nUnits);
        }
        else if(flags == ForSpecifiedTime)
        {
            Sleep(nUnits);
        }

        // If we had a test source end the streaming. If this was a source filter then streaming will end when we stop the graph.
        if (pTestSource) {
            LOG(TEXT("StreamingVerification: Completing test source streaming."));
            hr = pTestSource->EndStreaming();
            if (FAILED(hr)) {
                LOG(TEXT("StreamingVerification: EndStreaming failed."));
                retval = TPR_FAIL;
            }
        }
    }

    // Before stopping the graph, wait awhile
    // If we had a test source, we stopped streaming. 
    // If we had a capture filter, then Pause should pause capture
    // BUGBUG: If something else that continues to stream, then we don't deal with it very well
    LOG(TEXT("StreamingVerification: waiting for filter to drain."));
    // if we're testing on a filter that doesn't send an end of stream, then
    // don't wait for the end of stream.
    if(flags != ForSpecifiedTime)
    {
        if (pTestSource && pVerifier && pVerifier->GotSourceEndOfStream(-1))
        {
            // If we got an end of stream at source, then wait till we get an EOS at sink
            int nSleeps = 0;
            while (!pVerifier->GotSinkEndOfStream(-1)) {
                Sleep(2000);
                nSleeps++;
            }
        }
        else {
            // BUGBUG: By default: wait 2 secs - if this does not work for a filter, the filter's got to write it's own tests
            Sleep(2000);
        }
    }

    // Pause the graph (after the sink has been flushed
    if (pMediaControl) {
        pMediaControl->Pause();
    }

    // Stop the graph
    if (pMediaControl) {
        LOG(TEXT("StreamingVerification: Stopping and releasing the graph."));
        pMediaControl->Stop();
        pMediaControl->Release();
    }
	
    if(pEvent)    
    	pEvent->Release();

    // Now check that the verifier was happy
    if ((retval == TPR_PASS) && pVerifier) {

        LOG(TEXT("StreamingVerification: Ending the verifier stream."));
        // End the streaming and ask the verifier for the result.
        hr = pVerifier->StreamingEnd();

        if (FAILED(hr) || (hr == S_FALSE)) {
            LOG(TEXT("StreamingVerification: Verifier StreamingEnd failed."));
            retval = TPR_FAIL;
        }

        if(pVerifier->GetSamplesFailed(-1) > 0) {
            LOG(TEXT("StreamingVerification: %d Samples that failed verification."), pVerifier->GetSamplesFailed());
            retval = TPR_FAIL;
        }

        LOG(TEXT("StreamingVerification: Retrieving the verifier results."));
        // if we're not streaming for a specified time and then stopping (the source doesn't send an end of stream), then 
        // An EOS should have been sent when the test source was told to stop streaming. Check if one was sent on any pin.
        if (mode != ForSpecifiedTime && pTestSource && pVerifier->GotSourceEndOfStream(-1))
        {
            // So an EOS was sent, did we get it on the sink on any pin
            if (!pVerifier->GotSinkEndOfStream(-1)) {
                LOG(TEXT("StreamingVerification: No end of stream recieved on the sink."));
                retval = TPR_FAIL;
            }
        }
    }

    LOG(TEXT("StreamingVerification: Test completed."));

    if (pTestSource)
        pTestSource->Release();
    if (pTestSink)
        pTestSink->Release();

    return retval;
}

TESTPROCAPI StreamingRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    HRESULT hr = NOERROR;

    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("StreamingRun: No pTestGraph given."));
        return TPR_FAIL;
    }

    // Connect 
    hr = pTestGraph->ConnectGraph();
    if (FAILED(hr)) {
        LOG(TEXT("StreamingRun: Failed to ConnectGraph."));
        retval = TPR_FAIL;
    }
    else {
        int nStreamingModes = pTestGraph->GetNumStreamingModes();
        for(int i = 0; i < nStreamingModes; i++)
        {
            StreamingMode mode;
            StreamingFlags flags;
            TCHAR* datasource = NULL;
            int nUnits = 0;
            pTestGraph->GetStreamingMode(i, mode, nUnits, flags, &datasource);
            if (StreamingVerification(pTestGraph, mode, nUnits, flags, datasource) != TPR_PASS) {
                LOG(TEXT("StreamingRun: StreamingVerification failed."));
                retval = TPR_FAIL;
            }
        }
    }

    hr = pTestGraph->DisconnectGraph();
    if (FAILED(hr)) {
        LOG(TEXT("StreamingRun: Failed to disconnect the graph."));
        retval = TPR_FAIL;
    }

    LOG(TEXT("StreamingRun: deleting test graph."));
    delete pTestGraph;

    return retval;
}

TESTPROCAPI FlushTest(ITestSource* pTestSource, IVerifier* pVerifier)
{
    HRESULT hr = NOERROR;
    DWORD retval = TPR_PASS;

    // Begin the flush - did the sink get a begin flush
    hr = pTestSource->BeginFlushTest();
    if (FAILED(hr)) {
        retval = (hr == E_NOTIMPL) ? TPR_SKIP : TPR_FAIL;
        LOG(TEXT("FlushTest: BeginFlushTest failed or unsupported on test source."));
    }
    else {
        // Now check that the source actually sent a BeginFlush
        if (pVerifier && pVerifier->GotSourceBeginFlush()) {
            if (!pVerifier->GotSinkBeginFlush()) {
                LOG(TEXT("FlushTest: Verifier failed to get a BeginFlush."));
                retval = TPR_FAIL;
            }
        }
        else {
            // For some reason the test sink did not get a BeginFlush
            LOG(TEXT("FlushTest: sent BeginFlush but Verifier was not signaled."));
            retval = TPR_FAIL;
        }

        // We have to end the flush
        hr = pTestSource->EndFlushTest();
        if (FAILED(hr)) {
            LOG(TEXT("FlushTest: EndFlushTest failed."));
            retval = TPR_FAIL;
        }
        else {
            // Now check that the source actually sent a BeginFlush
            if (pVerifier && pVerifier->GotSourceEndFlush()) {
                if (!pVerifier->GotSinkEndFlush()) {
                    LOG(TEXT("FlushTest: Verifier failed to get a EndFlush."));
                    retval = TPR_FAIL;
                }
            }
            else {
                // For some reason the test sink did not get a EndFlush
                LOG(TEXT("FlushTest: sent EndFlush but Verifier was not signaled."));
                retval = TPR_FAIL;
            }
        }
    }
    return retval;
}

FILTER_STATE GetNextState(FILTER_STATE state)
{
    if (state == State_Stopped)
        return State_Paused;
    else if (state == State_Paused)
        return State_Running;
    else if (state == State_Running)
        return State_Stopped;
    return State_Stopped;
}

TESTPROCAPI FlushTest(CTestGraph* pTestGraph)
{
    HRESULT hr = NOERROR;
    DWORD retval = TPR_PASS;
    ITestSource* pTestSource = NULL;
    ITestSink* pTestSink = NULL;

    // Get the test source and sink interfaces - the graph sets a verifier
    if (!(pTestSource = pTestGraph->GetTestSource())) {
        LOG(TEXT("FlushTest: No test source, flush test not applicable, skipping."));
        return TPR_SKIP;
    }
    if ( !(pTestSink = pTestGraph->GetTestSink())) {
        LOG(TEXT("FlushTest: No test sink, flush test not applicable, skipping."));
        pTestSource->Release();
        return TPR_SKIP;
    }

    // Get the verifier if any
    IVerifier* pVerifier = pTestSource->GetVerifier();

    if (!pVerifier) {
        pTestSource->Release();
        pTestSink->Release();

        LOG(TEXT("FlushTest: No Verifier given, skipping."));
        return TPR_SKIP;
    }

    // Get media filter interface
    IMediaControl *pFilterGraph = pTestGraph->GetMediaControlGraph();
    if (!pFilterGraph) {
        LOG(TEXT("FlushTest: Unable to retrieve the media control."));
        retval = TPR_FAIL;
    }
    else {
        FILTER_STATE state = State_Stopped;
        int i = 0;
        bool flushok = false;
        // Iterate over states
        while ((i < 6) && (retval == TPR_PASS))
        {
            if (FAILED(SetState(pFilterGraph, state))) {
                LOG(TEXT("FlushTest: SetState failed."));
                retval = TPR_FAIL;
            }
            else {
                // Now check our state and if not stopped start streaming
                if (state != State_Stopped) {
                    StreamingMode mode;
                    StreamingFlags flags;
                    TCHAR* datasource = NULL;
                    int nUnits = 0;
                    // Get the first streaming mode - this is a hint to test graph developers to put shorter streaming modes
                    // at the front
                    if (!pTestGraph->GetStreamingMode(0, mode, nUnits, flags, &datasource))
                    {
                        // There are no streaming modes available, doh!
                        LOG(TEXT("FlushTest: There are no streaming modes available, skipping."));
                        retval = TPR_SKIP;
                    }
                    else {
                        // If we had a verifier, set the streaming to begin.
                        if (pVerifier)
                        {
                            pVerifier->Reset();
                            pVerifier->StreamingBegin();
                        }

                        // Start streaming - note that we have not notified the verifiers that we are doing a streaming test
                        pTestSource->SetStreamingMode(mode, nUnits, flags);
                        pTestSource->BeginStreaming();
                    }
                }

                // Run the actual test after sleeping a bit - there is an hidden assumption that the streaming mode is longer than this period
                Sleep(100);

                // Did the flush work ok
                if (FlushTest(pTestSource, pVerifier) != TPR_PASS) {
                    LOG(TEXT("FlushTest: FlushTest failed."));
                    retval = TPR_FAIL;
                }

                // End streaming
                if (state != State_Stopped)
                    pTestSource->EndStreaming();

                if (pVerifier)
                    pVerifier->StreamingEnd();
            }
            // Change state
            state = GetNextState(state);
            i++;
        }
    }

    // Set to Stopped before releasing which disconnects the graph
    if (FAILED(SetState(pFilterGraph, State_Stopped))) {
        LOG(TEXT("FlushTest: SetState to Stop failed."));
        retval = TPR_FAIL;
    }

        
    // No need to release the verifier
    if (pFilterGraph)
        pFilterGraph->Release();
    if (pTestSource)
        pTestSource->Release();
    if (pTestSink)
        pTestSink->Release();
    return retval;
}

TESTPROCAPI FlushTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE_TPM_EXECUTE();

    DWORD retval = TPR_PASS;
    HRESULT hr = NOERROR;

    // Create an empty test graph
    CTestGraph* pTestGraph = ((TestGraphFactory)lpFTE->dwUserData)();
    if (!pTestGraph) {
        LOG(TEXT("FlushTest: No pTestGraph given."));
        return TPR_FAIL;
    }

    hr = pTestGraph->ConnectGraph();
    if (FAILED(hr)) {
        LOG(TEXT("FlushTest: Failed to connect the graph."));
        retval = TPR_FAIL;
    }
    else retval = FlushTest(pTestGraph);

    hr = pTestGraph->DisconnectGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("FlushTest: Failed to disconnect the graph."));
        retval = TPR_FAIL;
    }

    delete pTestGraph;
    return retval;
}


TESTPROCAPI NewSegmentTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    HANDLE_TPM_EXECUTE();

    // Until I figure how to do this one, skip.
    return TPR_SKIP;
}
