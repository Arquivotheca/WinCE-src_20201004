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
//  IFilterGraph, IGraphBuilder, IGraphBuilder interface tests
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


//Test IGraphBuilder
TESTPROCAPI IGraphBuilderTestAbort(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IGraphBuilder *pIGraphBuilder = NULL;

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

    //get graph builder pointer
    pIGraphBuilder = testGraph.GetGraph();
    if(!pIGraphBuilder)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to get  IGraphBuilder pointer for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval =  TPR_FAIL;
        goto cleanup;
    }

    //set the graph in running state and do interface test
    hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS, NULL);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to change state to State_Running, (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( !pTestDesc->RemoteGB() )
    {
        hr = pIGraphBuilder->Abort();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to abort during the playback. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    //set the graph in paused state and do interface test
    hr = testGraph.SetState(State_Paused, TestGraph::SYNCHRONOUS, NULL);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to change state to State_Paused, (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( !pTestDesc->RemoteGB() )
    {
        hr = pIGraphBuilder->Abort();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to abort during the playback. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    //set the graph in stopped state and do interface test
    hr = testGraph.SetState(State_Stopped, TestGraph::SYNCHRONOUS, NULL);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to change state to State_Stopped, (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( !pTestDesc->RemoteGB() )
    {
        hr = pIGraphBuilder->Abort();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to abort during the playback. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    //set the graph in stopped state and do interface test
    hr = testGraph.StopWhenReady();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to call StopWhenReady(), (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( !pTestDesc->RemoteGB() )
    {
        hr = pIGraphBuilder->Abort();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to abort during the playback. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

cleanup:

    if(pIGraphBuilder)
    {
        pIGraphBuilder->Release();
        pIGraphBuilder = NULL;
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the test
    pTestDesc->Reset();

    return retval;
}


//
// Test IGraphBuilder and IFilterGraph2
//
TESTPROCAPI
IGraphBuilder_IFilterGraph2_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    UINT uNumber = 0;
    // Handle tux messages
    if ( SPR_HANDLED == HandleTuxMessages(uMsg, tpParam) )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc* pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    // Test Graph
    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // From the config, determine the media files to be used
    PlaybackMedia *pMedia = NULL;

    IGraphBuilder *pFilterGraph = NULL;
    IFilterGraph2 *pFilterGraph2 = NULL;
    IEnumFilters  *pEnumFilters = NULL;
    IBaseFilter      *pFiltersInGraph[MAX_NUM_FILTERS];
    IEnumPins      *pEnumPins = NULL;
    IPin          *pPinsInFilter[MAX_NUM_PINS];

    IBaseFilter   *pTestFilter = NULL, *pTmpFilter = NULL;
    IPin          *pTestPin1 = NULL, *pTestPin2 = NULL, *pTmpPin = NULL;
    AM_MEDIA_TYPE  Connection_MT;

    unsigned long ulNumFetched, ulNumFiltersInGraph;
    unsigned long ulCount = 0;
    bool    bFound;
    WCHAR   wszFilterName[]=L"TestFilter";
    PIN_DIRECTION pd1, pd2;

    for ( int i = 0; i < MAX_NUM_FILTERS; i++ )
        pFiltersInGraph[i] = NULL;

    for ( int i = 0; i < MAX_NUM_PINS; i++ )
        pPinsInFilter[i] = NULL;

    for ( int i = 0; i < pTestDesc->GetNumMedia(); i++ )
    {
        // Reset the testGraph
        testGraph.Reset();
        if ( pFilterGraph )
        {
            pFilterGraph->Release();
            pFilterGraph = NULL;
        }
        if ( pFilterGraph2 )
        {
            pFilterGraph2->Release();
            pFilterGraph2 = NULL;
        }
        if ( pEnumFilters )
        {
            pEnumFilters->Release();
            pEnumFilters = NULL;
        }
        if ( pEnumPins )
        {
            pEnumPins->Release();
            pEnumPins = NULL;
        }
        if ( pTestFilter )
        {
            pTestFilter->Release();
            pTestFilter = NULL;
        }
        if ( pTmpFilter )
        {
            pTmpFilter->Release();
            pTmpFilter = NULL;
        }
        if ( pTestPin1 )
        {
            pTestPin1->Release();
            pTestPin1 = NULL;
        }
        if ( pTestPin2 )
        {
            pTestPin2->Release();
            pTestPin2 = NULL;
        }
        if ( pTmpPin )
        {
            pTmpPin->Release();
            pTmpPin = NULL;
        }
        for ( int j = 0; j < MAX_NUM_FILTERS; j++ )
        {
            if ( pFiltersInGraph[j] ) pFiltersInGraph[j]->Release();
            pFiltersInGraph[j] = NULL;
        }
        for ( int j = 0; j < MAX_NUM_PINS; j++ )
        {
            if ( pPinsInFilter[j] ) pPinsInFilter[j]->Release();
            pPinsInFilter[j] = NULL;
        }

        pMedia = pTestDesc->GetMedia( i );
        if ( !pMedia )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the %d media object"),
                        __FUNCTION__, __LINE__, __FILE__, i + 1 );
            retval = TPR_FAIL;
            if ( i < pTestDesc->GetNumMedia() - 1 ) // or it is the last one.
                continue;
            else
                break;
        }

        testGraph.SetMedia(pMedia);

        // Build the graph
        hr = testGraph.BuildGraph();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to build graph with the video renderer for %s (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
            retval = TPR_FAIL;
            continue;
        }
        else {
            LOG( TEXT("%S: successfully built graph with the video renderer for %s"),
                        __FUNCTION__, pMedia->GetUrl() );
        }

        // Start the tests
        pFilterGraph = testGraph.GetGraph();
        if ( !pFilterGraph )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to retrieve graph builder interface for %s. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
            retval = TPR_FAIL;
            continue;
        }

        //
        // Test EnumFilters methods
        //

        // NULL pointer input
        hr = pFilterGraph->EnumFilters( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - EnumFilters w/ NULL didn't return E_POINTER. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Get IEnumFilters Interface from IFilterGraph
        hr = pFilterGraph->EnumFilters( &pEnumFilters );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to get IEnumFilters interface for %s. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( NULL == pEnumFilters )
        {
            LOG( TEXT("%S: ERROR %d@%S - NULL IEnumFilters pointer returned."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // Enumerate filters
        hr = pEnumFilters->Next( MAX_NUM_FILTERS, &(pFiltersInGraph[0]), &ulNumFiltersInGraph );
        if ( S_FALSE != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() w/ %d as the number to fetch should return S_FALSE. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_FILTERS, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( 0 == ulNumFiltersInGraph )
        {
            LOG( TEXT("%S: ERROR %d@%S - No filters in the graph for media %s."),
                        __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl() );
            retval = TPR_FAIL;
            continue;
        }

        //
        // Test Disconnect, Reconnect and ConnectDirect method
        //

        // Pick up a random filter as the test filter
        rand_s(&uNumber);
        i = uNumber % ulNumFiltersInGraph;
        pTestFilter = pFiltersInGraph[i];
        if ( !pTestFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - NULL filter pointer returned from Next()."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // Print out the name of the filter
        FILTER_INFO Info;
        hr = pTestFilter->QueryFilterInfo( &Info );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - QueryFilterInfo failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        LOG( TEXT("The %dth filter has been chosen for testing. Filter name: %s"),
                                i, Info.achName );
        Info.pGraph->Release();

        // Get all the pins in the filter
        hr = pTestFilter->EnumPins( &pEnumPins );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to get IEnumPins interface for filter. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( NULL == pEnumPins )
        {
            LOG( TEXT("%S: ERROR %d@%S - NULL IEnumPins pointer returned."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        hr = pEnumPins->Next( MAX_NUM_PINS, &(pPinsInFilter[0]), &ulNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() w/ %d as the number to fetch failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_PINS, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( 0 == ulNumFetched )
        {    // There should be at least one pin in a filter
            LOG( TEXT("%S: ERROR %d@%S - The filter should have at least one pin."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }


        // Select a pin which is connected with another pin
        bFound = false; ulCount = 1;
        rand_s(&uNumber);
        for ( i = uNumber % ulNumFetched; ulCount <= ulNumFetched; ulCount++ )
        {
            i = (i+1) % ulNumFetched;
            pTestPin1 = pPinsInFilter[i];
            if ( !pTestPin1 )
            {
                LOG( TEXT("%S: ERROR %d@%S - The Pin retrieved (%d) by Next() is NULL."),
                            __FUNCTION__, __LINE__, __FILE__, i );
                retval = TPR_FAIL;
                break;
            }
            // Get the pin connected with the test pin 1
            hr = pTestPin1->ConnectedTo(&pTestPin2);
            if ( SUCCEEDED(hr) )
            {   // there is a pin connected to the test pin 1
                if ( !pTestPin2 )
                {
                    LOG( TEXT("%S: ERROR %d@%S - ConnectedTo return NULL for the pin (0x%08x)."),
                                __FUNCTION__, __LINE__, __FILE__, (LONG)pTestPin1 );
                    retval = TPR_FAIL;
                    break;
                }
                bFound = true;
                break;          // stop finding
            }
            else
            {
                // if not connected, it should be NULL
                if ( pTestPin2 )
                {
                    LOG( TEXT("%S: ERROR %d@%S - NULL should be returned for Pin (0x%08x) is not connected."),
                                __FUNCTION__, __LINE__, __FILE__, (LONG)pTestPin1 );
                    retval = TPR_FAIL;
                }
                pTestPin2 = NULL;
            }
            pTestPin1 = NULL;   // release
        }

        // There shouldn't be a filter who's not connected with any others or
        // something wrong happened in the for loop
        if ( !bFound )
        {
            retval = TPR_FAIL;
            continue;
        }

        // Now we find a suitable test pin pair: pTestPin1 and pTestPin2
        if ( !pTestPin1 || !pTestPin2 )
        {
            LOG( TEXT("%S: ERROR %d@%S - Can't find the test pin pair (0x%08x, 0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pTestPin1, (LONG)pTestPin2 );
            retval = TPR_FAIL;
            continue;
        }

        LOG( TEXT(" Test Pin Pair (Pin 1: 0x%08x, Pin 2: 0x%08x)"), (LONG)pTestPin1, (LONG)pTestPin2 );
        // Verify that they are connected
        pTmpPin = NULL;
        pTestPin2->ConnectedTo( &pTmpPin );
        if ( pTmpPin != pTestPin1 )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 2 ConnectedTo returned 0x%0x, not pin 1."),
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pTmpPin );
            retval = TPR_FAIL;
            continue;
        }

        // Store the Media Type of the connection
        hr = pTestPin1->ConnectionMediaType( &Connection_MT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 1 ConnectionMediaType failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }


        pTmpPin = NULL;  //release

        // Test Reconnect method
        // NULL pointer input to Reconnect
        hr = pFilterGraph->Reconnect( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - Reconnect w/ NULL didn't return E_POINTER. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Valid input to Reconnect
        // Should be able to Reconnect both pins
        hr = pFilterGraph->Reconnect( pTestPin1 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Reconnect for PIN 1 failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        Sleep(100);  // wait a while for the Reconnect thread to finish

        // After Reconnect, the test pin should be connected
        hr = pTestPin1->ConnectedTo( &pTmpPin );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - ConnectedTo for PIN 1 failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        // with the exactly the same pin it used to connect to
        if ( pTmpPin != pTestPin2 )
        {
            LOG( TEXT("%S: ERROR %d@%S - The Pin PIN 1 connected is not PIN 2."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // Media Type might have been renegociated, store it
        hr = pTestPin1->ConnectionMediaType( &Connection_MT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 1 ConnectionMediaType failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Clean up
        pTmpPin = NULL;

        // Test Disconnect method
        if ( pTestPin2 )  //  Do this only when the pin is connected
        {
            // NULL pointer input to Disconnect
            hr = pFilterGraph->Disconnect( NULL );
            if ( E_POINTER != hr )
            {
                LOG( TEXT("%S: ERROR %d@%S - Filter graph Disconnect w/ NULL didn't return E_POINTER. (0x%08x)."),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }

            // Valid input to Disconnect
            // It's necessary to disconnect both of the pins, since disconnecting one does not completely
            // break the connection
            hr = pFilterGraph->Disconnect( pTestPin1 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Disconnect with Pin 1 failed. (0x%08x)."),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
            hr = pFilterGraph->Disconnect( pTestPin2 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Disconnect with Pin 2 failed. (0x%08x)."),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }

            // Verify it's disconnected
            pTestPin1->ConnectedTo( &pTmpPin );
            if ( NULL != pTmpPin )
            {
                LOG( TEXT("%S: ERROR %d@%S - Pin 1 is not disconnected after calling Disconnect."),
                            __FUNCTION__, __LINE__, __FILE__ );
                retval = TPR_FAIL;
                continue;
            }

            // Disconnect for the second time should result in a successful no-op
            hr = pFilterGraph->Disconnect( pTestPin1 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Disconnect a disconnected PIN should succeed. (0x%08x)."),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
            // Verify it's disconnected
            pTestPin2->ConnectedTo( &pTmpPin );
            if ( NULL != pTmpPin )
            {
                LOG( TEXT("%S: ERROR %d@%S - Pin 2 is not disconnected after calling Disconnect."),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
        }

        // Test ConnectDirect method
        // NULL pointer input to ConnectDirect
        hr = pFilterGraph->ConnectDirect( NULL, NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - ConnectDirect with NULLs didn't return E_POINTER. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pFilterGraph->ConnectDirect( pTestPin1, NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - ConnectDirect with NULLs didn't return E_POINTER. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pFilterGraph->ConnectDirect( NULL, pTestPin2, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - ConnectDirect with NULLs didn't return E_POINTER. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Check if the Media Type is accepted by the pins
        hr = pTestPin1->QueryAccept( &Connection_MT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 1 QueryAccept failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pTestPin2->QueryAccept( &Connection_MT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 2 QueryAccept failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Check which pin is input pin, which is output pin
        hr = pTestPin1->QueryDirection( &pd1 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 1 QueryDirection failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pTestPin2->QueryDirection( &pd2 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 2 QueryDirection failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pd1 == pd2 )
        {
            LOG( TEXT("%S: ERROR %d@%S - Directions for Pin 1 and Pin 2 should not be same, should be input and output."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // Valid input to ConnectDirect
        if ( PINDIR_INPUT == pd1 )
        {
            hr = pFilterGraph->ConnectDirect( pTestPin2, pTestPin1, &Connection_MT );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - ConnectDirect failed. (0x%08x)."),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
        }
        else
        {
            hr = pFilterGraph->ConnectDirect( pTestPin1, pTestPin2, &Connection_MT );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - ConnectDirect failed. (0x%08x)."),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
        }

        // Verify that they are now connected
        pTestPin2->ConnectedTo( &pTmpPin );
        if ( pTmpPin != pTestPin1 )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 2 is not connected with Pin 1."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        pTmpPin = NULL;  // release

        // Break the connection again
        hr = pFilterGraph->Disconnect( pTestPin1 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Disconnect w/ Pin 1 failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        hr = pFilterGraph->Disconnect( pTestPin2 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Disconnect w/ Pin 2 failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Try NULL parameter to ConnectDirect() as Media Type. It should be ok.
        if ( PINDIR_INPUT == pd1 )
        {
            hr = pFilterGraph->ConnectDirect( pTestPin2, pTestPin1, NULL );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - ConnectDirect w/ NULL media type failed. (0x%08x)."),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
        }
        else
        {
            hr = pFilterGraph->ConnectDirect( pTestPin1, pTestPin2, NULL );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - ConnectDirect w/ NULL media type failed. (0x%08x)."),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
        }

        // Verify that they are now connected
        pTestPin2->ConnectedTo( &pTmpPin );
        if ( pTmpPin != pTestPin1 )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 2 is not connected to Pin 1."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        pTmpPin = NULL;  // release


        // Test IFilterGraph2::ReconnectEx method
        // Get IFilterGraph2 Interface
        hr = pFilterGraph->QueryInterface( IID_IFilterGraph2,  (void**)&pFilterGraph2 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - QueryInterface w/ IID_IFilterGraph2 failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Make sure the test pin is connected
        pTmpPin = NULL;
        pTestPin1->ConnectedTo( &pTmpPin );
        if ( pTmpPin != pTestPin2 )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 1 is not connected to Pin 2."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }
        pTmpPin = NULL;  //release

        // Store the Media Type of the connection
        hr = pTestPin1->ConnectionMediaType( &Connection_MT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 1 ConnectionMediaType failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // NULL pointer input to ReconnectEx
        hr = pFilterGraph2->ReconnectEx( NULL,NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - ReconnectEx w/ NULL didn't return E_POINTER. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Valid input to ReconnectEx
        // Should be able to reconnect both pins
        hr = pFilterGraph2->ReconnectEx( pTestPin1, NULL );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - ReconnectEx w/ valid input failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Verify that they are now connected
        pTestPin2->ConnectedTo( &pTmpPin );
        if ( pTmpPin != pTestPin1 )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 2 is not connected to Pin 1 after ReconnectEx w/ Pin 1"),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }
        if ( pTmpPin ) pTmpPin->Release();
        pTmpPin = NULL;  // release

        // Specify Media Type this time
        hr = pFilterGraph2->ReconnectEx( pTestPin1, &Connection_MT );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - ReconnectEx w/ non-NULL media type failed. (0x%08x)."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Verify that they are now connected
        pTestPin2->ConnectedTo( &pTmpPin );
        if ( pTmpPin != pTestPin1 )
        {
            LOG( TEXT("%S: ERROR %d@%S - Pin 2 is not connected to Pin 1 after ReconnectEx w/ Pin 1"),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }
        if ( pTmpPin ) pTmpPin->Release();
        pTmpPin = NULL;  // release


        // Test RemoveFilter method
        // Pick up a random filter as the test filter
        pTestFilter = NULL;  // release interface
        rand_s(&uNumber);
        int idx = uNumber%ulNumFiltersInGraph;
        pTestFilter = pFiltersInGraph[ idx ];
        if ( !pTestFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - NULL filter pointer returned from enumerated filter array. index: %d"),
                        __FUNCTION__, __LINE__, __FILE__, idx );
            retval = TPR_FAIL;
            continue;
        }

        // NULL pointer input
        hr = pFilterGraph->RemoveFilter( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - RemoveFilter w/ NULL didn't return E_POINTER. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Valid input
        hr = pFilterGraph->RemoveFilter( pTestFilter );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - RemoveFilter failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // The filter should already be moved from the graph. Others should stay.
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        ulCount = 0;
        bool bNullFilter = false;
        while( SUCCEEDED( pEnumFilters->Next( 1, &pTmpFilter, &ulNumFetched ) ) )
        {
            if ( 0 == ulNumFetched )
                break;
            else
            {
                if ( !pTmpFilter )
                {
                    LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() returned a NULL filter pointer"),
                                __FUNCTION__, __LINE__, __FILE__, hr );
                    retval = TPR_FAIL;
                    bNullFilter = true;
                    break;
                }
                ulCount++;
               // The removed filter should not be in the graph any more
                if ( pTmpFilter == pTestFilter )
                {
                    LOG( TEXT("%S: ERROR %d@%S - The removed filter shound not be in the filter anymore."),
                                __FUNCTION__, __LINE__, __FILE__ );
                    retval = TPR_FAIL;
                    break;
                }

                bFound = false;
                // We should find other filters in the graph
                for ( i = 0; i <= (int)ulNumFiltersInGraph; i++ )
                {
                    if ( pTmpFilter == pFiltersInGraph[i] )
                    {
                        bFound = true;
                        break;
                    }
                }
                if ( !bFound ) break;  // error if the filter is not found
            }
            // release the interface before going to the next one
            if ( pTmpFilter ) pTmpFilter->Release();
            pTmpFilter = NULL;
        }

        if ( bNullFilter ) continue;    // try next media file.
        if ( !bFound )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() returned a filter pointer not in previous list."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // When enumerate beyond the end, pTmpFilter should be NULL
        if ( pTmpFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - When enumerate beyond the end, pTmpFilter should be NULL."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // There should be exactly one less fiter in the graph
        if ( ulNumFiltersInGraph - 1 != ulCount )
        {
            LOG( TEXT("%S: ERROR %d@%S - After removing a filter, the size %d is not orig size %d minus 1."),
                        __FUNCTION__, __LINE__, __FILE__, ulCount, ulNumFiltersInGraph );
            retval = TPR_FAIL;
            continue;
        }


        // Test AddFilter method
        // NULL pointer input
        hr = pFilterGraph->AddFilter( NULL,NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - AddFilter w/ NULL didn't return E_POINTER. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pFilterGraph->AddFilter( NULL, wszFilterName );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - AddFilter w/ NULL didn't return E_POINTER. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Valid input
        hr = pFilterGraph->AddFilter( pTestFilter,wszFilterName );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - AddFilter w/ valid Filter failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Now we should be able to find it in the graph
        // We use two methods to find it: Enumerate and FindFilterByName
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        bFound = false;
        while( SUCCEEDED( pEnumFilters->Next( 1, &pTmpFilter, &ulNumFetched ) ) )
        {
            if ( 0 == ulNumFetched )
                break;
            else
            {
                if ( !pTmpFilter )
                {
                    LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() returned a NULL filter pointer"),
                                __FUNCTION__, __LINE__, __FILE__, hr );
                    retval = TPR_FAIL;
                    break;
                }
                if( pTmpFilter == pTestFilter )
                {
                    bFound = true;
                    break;
                }
            }
            // release the interface before going to the next one
            if ( pTmpFilter ) pTmpFilter->Release();
            pTmpFilter = NULL;
        }
        // The filter should have been found in the graph
        if ( !bFound )
        {
            retval = TPR_FAIL;
            continue;        // go to the next media file.
        }

        pTmpFilter = NULL;
        ulCount = 0;
        // Now all the filter shoudl be able to found in the graph
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        bNullFilter = false;
        while( SUCCEEDED( pEnumFilters->Next( 1, &pTmpFilter, &ulNumFetched ) ) )
        {
            if ( 0 == ulNumFetched )
                break;
            else
            {
                if ( !pTmpFilter )
                {
                    LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() returned a NULL filter pointer"),
                                __FUNCTION__, __LINE__, __FILE__, hr );
                    retval = TPR_FAIL;
                    bNullFilter = true;
                    break;
                }
                ulCount++;
                bFound = false;
                // We should find all filters in the graph
                for ( i = 0; i <= (int)ulNumFiltersInGraph; i++ )
                {
                    if ( pTmpFilter == pFiltersInGraph[i] )
                    {
                        bFound = true;
                        break;
                    }
                }
                if ( !bFound ) break;  // error if the filter is not found
            }
            // release the interface before going to the next one
            if ( pTmpFilter ) pTmpFilter->Release();
            pTmpFilter = NULL;
        }

        if ( bNullFilter ) continue;    // try next media file.
        if ( !bFound )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() returned a filter pointer not in previous list."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // When enumerate beyond the end, pTmpFilter should be NULL
        if ( pTmpFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - When enumerate beyond the end, pTmpFilter should be NULL."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // There should be exactly one less fiter in the graph
        if ( ulNumFiltersInGraph != ulCount )
        {
            LOG( TEXT("%S: ERROR %d@%S - After adding the filter, the size %d does not match the orig size."),
                        __FUNCTION__, __LINE__, __FILE__, ulCount, ulNumFiltersInGraph );
            retval = TPR_FAIL;
            continue;
        }


        // Test FindFilterByName method
        pTmpFilter = NULL;

        // NULL pointer input
        hr = pFilterGraph->FindFilterByName( NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName w/ NULLs didn't return E_POINTER. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pFilterGraph->FindFilterByName( wszFilterName, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName w/ NULLs didn't return E_POINTER. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pFilterGraph->FindFilterByName( NULL, &pTmpFilter );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName w/ NULLs didn't return E_POINTER. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        if ( pTmpFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName w/ NULL name returned a non-null filter."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            pTmpFilter->Release();
            pTmpFilter = NULL;
        }

        // Invalid filtername
        hr = pFilterGraph->FindFilterByName( L"", &pTmpFilter );
        // A null string should make the function return error, but not crash the system
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName w/ invalid filter name should fail."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }
        if ( pTmpFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName w/ NULL name returned a non-null filter."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            pTmpFilter->Release();
            pTmpFilter = NULL;
        }

        // Look for a non-existing filtername
        hr = pFilterGraph->FindFilterByName( L"NONEXIST", &pTmpFilter );
        LOG( TEXT("When looking for non-existing filter, FindFilterByName returns 0x%08x"), hr );

        // I expect it to return E_NOT_FOUND. It is not documented though.
        if ( VFW_E_NOT_FOUND != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName w/ non-existing filter name didn't return VFW_E_NOT_FOUND. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        if ( pTmpFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName w/ NULL name returned a non-null filter."),
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            pTmpFilter->Release();
            pTmpFilter = NULL;
        }

        // Look for an existing filter
        hr = pFilterGraph->FindFilterByName( wszFilterName, &pTmpFilter );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pTmpFilter != pTestFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - FindFilterByName return a different filter 0x%08x from test filter 0x%08x."),
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pTmpFilter, (LONG)pTestFilter );
            retval = TPR_FAIL;
            continue;
        }


        // Test SetDefaultSyncSource method
        hr = pFilterGraph->SetDefaultSyncSource();
        // This method is really implementation dependent, so we just make sure
        // it does not return E_POINTER or E_INVALIDARG.
        // Actually these two values are valid in the desktop document, but since
        // the function takes no parameter, I don't think they should appear in any case.
        if ( (E_POINTER == hr) || (E_INVALIDARG == hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - SetDefaultSyncSource return 0x%08x."),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
        }
    }

    // release pointers
    for ( int j = 0; j < MAX_NUM_FILTERS; j++ )
    {
        if ( pFiltersInGraph[j] )
            pFiltersInGraph[j]->Release();
        pFiltersInGraph[j] = NULL;
    }

    // Reset the testGraph
    if ( pFilterGraph )
        pFilterGraph->Release();

    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the test
    pTestDesc->Reset();

    return retval;
}