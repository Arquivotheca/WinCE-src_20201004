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
//  IEnumPins Interface Tests
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


//
// Applications use IBaseFilter::EnumPins to retrieve pins on a filter.
//
TESTPROCAPI
IEnumPinsTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    // Add define for MAX_NUM_FILTERS and MAX_NUM_PINS.

    PlaybackMedia *pMedia = NULL;

    IGraphBuilder    *pFilterGraph = NULL;
    IEnumFilters    *pEnumFilters = NULL;
    IEnumPins        *pEnumPins = NULL;
    IBaseFilter        *pBaseFilter = NULL;
    IPin            *pPin = NULL;
    IBaseFilter        *pFiltersInGraph[MAX_NUM_FILTERS] = {0};
    IPin            *pPinsInFilter[MAX_NUM_PINS] = {0};
    unsigned long ulNumToFetch, ulNumFetched, ulTmpNumFetched;

    for ( int i = 0; i < pTestDesc->GetNumMedia(); i++ )
    {
        // Reset the testGraph
        testGraph.Reset();
        if ( pFilterGraph )
        {
            pFilterGraph->Release();
            pFilterGraph = NULL;
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

        for ( int j = 0; j < MAX_NUM_FILTERS; j++ )
        {
            if ( pFiltersInGraph[j] )
                pFiltersInGraph[j]->Release();
            pFiltersInGraph[j] = NULL;
        }
        for ( int j = 0; j < MAX_NUM_PINS; j++ )
        {
            if ( pPinsInFilter[j] )
                pPinsInFilter[j]->Release();
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

        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // The call should succeed, but since ulNumFetched does not match ulNumToFetch,
        // S_FALSE should be returned
        hr = pEnumFilters->Next( MAX_NUM_FILTERS, &(pFiltersInGraph[0]), &ulNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() w/ %d as the number to fetch failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_FILTERS, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Randomly choose a filter to run EnumPins test
        rand_s(&uNumber);
        pBaseFilter = pFiltersInGraph[ uNumber % ulNumFetched ];


        // Get IEnumPins Interface from the filter
        hr = pBaseFilter->EnumPins( &pEnumPins );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to get IEnumPins interface for filter (0x%08x). (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pBaseFilter, hr );
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

        // This test passes NULL as the parameter to ::Next()
        hr = pEnumPins->Next( 1, NULL, &ulNumFetched );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() w/ NULL didn't return E_POINTER. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumPins->Next( 1, NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() w/ NULL didn't return E_POINTER. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
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

        ulNumToFetch= ULONG_MAX; //0xFFFFFFFF 
        // The call should succeed, but since ulNumFetched does not match ulNumToFetch,
        // S_FALSE should be returned
        hr = pEnumPins->Next( ulNumToFetch, &(pPinsInFilter[0]), &ulNumFetched );
        if ( S_FALSE != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() w/ 0xFFFFFFFF as the number to fetch should return S_FALSE. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( ulNumFetched >= ulNumToFetch )
        {
            LOG( TEXT("%S: ERROR %d@%S - Feteched (%d) larger than number to fetch (%d)"),
                        __FUNCTION__, __LINE__, __FILE__, ulNumFetched, ulNumToFetch );
            retval = TPR_FAIL;
            continue;
        }
        if ( 1 > ulNumFetched )
        {
            LOG( TEXT("%S: ERROR %d@%S - Each filter should have at least one pin, not %d here."),
                        __FUNCTION__, __LINE__, __FILE__, ulNumFetched );
            retval = TPR_FAIL;
            continue;
        }

        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // This test enumerates one pin each time, compares it with the pointers
        // stored in pPinsInFilter[]
        unsigned long ulCount = 0;
        bool bWhileTestFailed = false;
        while( SUCCEEDED( pEnumPins->Next(1, &pPin, &ulTmpNumFetched) ) && ulTmpNumFetched )
        {
            if ( 1 < ulTmpNumFetched )
            {
                LOG( TEXT("%S: ERROR %d@%S - Next() fetched more than 1 pin as specified. (%d)"),
                            __FUNCTION__, __LINE__, __FILE__, ulTmpNumFetched );
                retval = TPR_FAIL;
                bWhileTestFailed = true;
                if ( pPin ) pPin->Release();
                break;
            }

            // Compare the two results
            if ( pPin != pPinsInFilter[ulCount] )
            {
                LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
            }
            ulCount++;
            //we need to release this interface before getting the Next one
            if ( pPin ) pPin->Release();
            pPin = NULL;
        }

        if ( bWhileTestFailed ) continue;

        // Two enumerating methods should yeild same number of filters
        if ( ulCount != ulNumFetched )
        {
            LOG( TEXT("%S: ERROR %d@%S - The number of pins enumerated (%d) doesn't match the previous one (%d)."),
                        __FUNCTION__, __LINE__, __FILE__, ulCount, ulNumFetched );
            retval = TPR_FAIL;
            continue;
        }

        // This test passes 0 to Skip(). Subsequent Next() should not be affected
        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        // Skip 0 before we call a Next()
        hr = pEnumPins->Skip( 0 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Skip w/ 0 failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumPins->Next( 1, &pPin, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pPin != pPinsInFilter[0] )
        {
            LOG( TEXT("%S: ERROR %d@%S - IPin pointers mismatch. (0x%08x, 0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pPin, (LONG)pPinsInFilter[0] );
            retval = TPR_FAIL;
            continue;
        }
        if ( pPin ) pPin->Release();
        pPin = NULL; //we need to release this interface before getting the Next one


        // This test passes 0 to Skip(). Subsequent Next() should not be affected
        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        // This test passes 1 to Skip()
        if ( ulNumFetched >= 2 ) // test only when there are enough filters in graph
        {
            hr = pEnumPins->Skip( 1 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Skip w/ 1 failed. (0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }

            hr = pEnumPins->Next( 1, &pPin, &ulTmpNumFetched );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
            if ( pPin != pPinsInFilter[1] ) // [0] should be skipped
            {
                LOG( TEXT("%S: ERROR %d@%S - IPin mismatch. (0x%08x, 0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, (LONG)pPin, (LONG)pPinsInFilter[1] );
                retval = TPR_FAIL;
                continue;
            }
            if ( pPin ) pPin->Release();
            pPin = NULL;
        }

        // This test passes a very large number to Skip
        // Subsequent calls to Next() should fetch nothing
        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        hr = pEnumPins->Skip( MAX_NUM_PINS );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Skip w/ %d failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_PINS, hr );
            retval = TPR_FAIL;
        }

        hr = pEnumPins->Next( 1, &pPin, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        // Number of filters fetched should be 0
        if ( 0 != ulTmpNumFetched )
        {
            LOG( TEXT("%S: ERROR %d@%S - We skipped %d filters and number of fetched pins (%d) not equal to zero."),
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_PINS, ulTmpNumFetched );
            retval = TPR_FAIL;
        }
        // Filter pointer should be NULL
        if ( NULL != pPin )
        {
            LOG( TEXT("%S: ERROR %d@%S - NULL IPin should be returned since we skipped %d filters."),
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_PINS );
            retval = TPR_FAIL;
        }

        // This test calls Reset() before calling Next()
        // The Next() should always start from the first filter
        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        hr = pEnumPins->Next( 1, &pPin, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pPin != pPinsInFilter[0] )
        {
            LOG( TEXT("%S: ERROR %d@%S - IPin pointers mismatch. (0x%08x, 0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pPin, (LONG)pPinsInFilter[0] );
            retval = TPR_FAIL;
            continue;
        }
        if ( pPin ) pPin->Release();
        pPin = NULL;

        // This test calls Reset() several times between two calls to Next()
        // They should have no effect on the Next()
        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumPins->Next( 1, &pPin, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pPin != pPinsInFilter[0] )
        {
            LOG( TEXT("%S: ERROR %d@%S - IPin pointers mismatch. (0x%08x, 0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pPin, (LONG)pPinsInFilter[0] );
            retval = TPR_FAIL;
            continue;
        }
        if ( pPin ) pPin->Release();
        pPin = NULL;

        // This test repeated calls Next() until there's no more filters to fetch
        // Then a call to Reset() should reset the sequence to the beginning
        do  // Enumerate till the end
        {
            hr = pEnumPins->Next( 1, &pPin, &ulTmpNumFetched );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                break;
            }
            if ( pPin )
            {
                pPin->Release();
                pPin=NULL;
            }

        } while ( ulTmpNumFetched );    // so when approaching the end of filter list, it set number to fetch as zero.
        // Do a reset
        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        // Now Next() will start from the beginning again
        hr = pEnumPins->Next( 1, &pPin, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pPin != pPinsInFilter[0] )
        {
            LOG( TEXT("%S: ERROR %d@%S - IPin pointers mismatch. (0x%08x, 0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pPin, (LONG)pPinsInFilter[0] );
            retval = TPR_FAIL;
            continue;
        }
        if ( pPin ) pPin->Release();
        pPin = NULL;

    }    // end of media file list.

    // release interface pointers
    for ( int j = 0; j < MAX_NUM_FILTERS; j++ )
    {
        if ( pFiltersInGraph[j] )
            pFiltersInGraph[j]->Release();
        pFiltersInGraph[j] = NULL;
    }
    for ( int j = 0; j < MAX_NUM_PINS; j++ )
    {
        if ( pPinsInFilter[j] )
            pPinsInFilter[j]->Release();
        pPinsInFilter[j] = NULL;
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


//
// Applications use IBaseFilter::EnumPins to retrieve pins on a filter.
//
TESTPROCAPI
IEnumPinsCloneTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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

    IGraphBuilder    *pFilterGraph = NULL;
    IEnumFilters    *pEnumFilters = NULL;
    IEnumPins        *pEnumPins = NULL;
    IBaseFilter        *pBaseFilter = NULL;
    IPin            *pPin = NULL;
    IBaseFilter        *pFiltersInGraph[MAX_NUM_FILTERS] = {0};
    IPin            *pPinsInFilter[MAX_NUM_PINS]= {0};
    IEnumPins        *pCloneEnumPins = NULL;
    IPin            *pClonePin = NULL;
    unsigned long ulCloneTmpNumFetched;
    unsigned long ulNumFetched, ulTmpNumFetched;

    for ( int i = 0; i < pTestDesc->GetNumMedia(); i++ )
    {
        // Reset the testGraph
        testGraph.Reset();
        if ( pFilterGraph )
        {
            pFilterGraph->Release();
            pFilterGraph = NULL;
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

        for ( int j = 0; j < MAX_NUM_FILTERS; j++ )
        {
            if ( pFiltersInGraph[j] )
                pFiltersInGraph[j]->Release();
            pFiltersInGraph[j] = NULL;
        }
        for ( int j = 0; j < MAX_NUM_PINS; j++ )
        {
            if ( pPinsInFilter[j] )
                pPinsInFilter[j]->Release();
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

        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // The call should succeed, but since ulNumFetched does not match ulNumToFetch,
        // S_FALSE should be returned
        hr = pEnumFilters->Next( MAX_NUM_FILTERS, &(pFiltersInGraph[0]), &ulNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() w/ %d as the number to fetch failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_FILTERS, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Randomly choose a filter to run EnumPins test
        rand_s(&uNumber);
        pBaseFilter = pFiltersInGraph[ uNumber % ulNumFetched ];


        // Get IEnumPins Interface from the filter
        hr = pBaseFilter->EnumPins( &pEnumPins );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Failed to get IEnumPins interface for filter (0x%08x). (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pBaseFilter, hr );
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

        // This test passes NULL to Clone()
        hr = pEnumPins->Clone( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Clone() w/ NULL should return E_POINTER. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // This test clone a new enumerator, we compare it with the original enumerator
        // by comparing the result of each identical subsequent call to Next()
        hr = pEnumPins->Clone( &pCloneEnumPins );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Clone() failed. (0x%x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // reset both enumerator
        hr = pEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pCloneEnumPins->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Cloned IEnumPins::Reset() failed. (0x%08x)"),
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        do  // compare the the two enumeration sequence
        {   // fetch one filter each time
            if ( pPin ) pPin->Release();
            if ( pClonePin ) pClonePin->Release();

            hr = pEnumPins->Next( 1, &pPin, &ulTmpNumFetched );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IEnumPins::Next() failed. (0x%x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
            hr = pCloneEnumPins->Next( 1, &pClonePin, &ulCloneTmpNumFetched );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Cloned IEnumPins::Next() failed. (0x%x)"),
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }

            if( (ulTmpNumFetched != ulCloneTmpNumFetched) ||
                (pPin != pClonePin) )
            {   // error, quit
                LOG( TEXT("%S: ERROR %d@%S - Cloned IEnumPins returned (%d, 0x%08x), orig returned (%d, 0x%08x)"),
                            __FUNCTION__, __LINE__, __FILE__, ulCloneTmpNumFetched, (LONG)pClonePin,
                            ulTmpNumFetched, (LONG)pPin );
                retval = TPR_FAIL;
                break;
            }

            if ( 0 != ulTmpNumFetched )
            {   // haven't reached the end yet. release pointers and ready to fetch again
                pPin = NULL;
                pClonePin = NULL;
            }
            // otherwise, we've reached the end. no need to release pointers this time

        } while ( ulTmpNumFetched ); // do for every filter fetched

    }    // end of media list

    // release interface pointers
    for ( int j = 0; j < MAX_NUM_FILTERS; j++ )
    {
        if ( pFiltersInGraph[j] )
            pFiltersInGraph[j]->Release();
        pFiltersInGraph[j] = NULL;
    }
    for ( int j = 0; j < MAX_NUM_PINS; j++ )
    {
        if ( pPinsInFilter[j] )
            pPinsInFilter[j]->Release();
        pPinsInFilter[j] = NULL;
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