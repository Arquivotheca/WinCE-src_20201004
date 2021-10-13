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
//  IEnumFilters Interface Tests
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
// The IEnumFilters enumerates the filters in a filter graph.
//
TESTPROCAPI 
IEnumFiltersTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

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
    IEnumFilters  *pEnumFilters = NULL;
    IBaseFilter      *pFiltersInGraph[MAX_NUM_FILTERS];
    IBaseFilter      *pBaseFilter = NULL;

    unsigned long ulNumFetched, ulTmpNumFetched, ulNumToFetch = 1000;

    for ( int i = 0; i < MAX_NUM_FILTERS; i++ )
        pFiltersInGraph[i] = NULL;

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

        for ( int j = 0; j < MAX_NUM_FILTERS; j++ )
        {
            if ( pFiltersInGraph[j] ) pFiltersInGraph[j]->Release();
            pFiltersInGraph[j] = NULL;
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

        // This test passes NULL as the parameter to ::Next()
        hr = pEnumFilters->Next( 1, NULL, &ulNumFetched );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() w/ NULL didn't return E_POINTER. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumFilters->Next( 1, NULL, NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() w/ NULL didn't return E_POINTER. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        
        ulNumToFetch= ULONG_MAX; //0xFFFFFFFF 
        // The call should succeed, but since ulNumFetched does not match ulNumToFetch,
        // S_FALSE should be returned
        hr = pEnumFilters->Next( ulNumToFetch, &(pFiltersInGraph[0]), &ulNumFetched );
        if ( S_FALSE != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() w/ 0xFFFFFFFF as the number to fetch should return S_FALSE. (0x%x)"), 
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

        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // This test enumerates one filter each time, compares it with the pointers
        // stored in pFiltersInGraph[]
        unsigned long ulCount = 0;
        bool bWhileTestFailed = false;
        while( SUCCEEDED( pEnumFilters->Next(1, &pBaseFilter, &ulTmpNumFetched) ) )
        {
            if ( 1 < ulTmpNumFetched )
            {
                LOG( TEXT("%S: ERROR %d@%S - Next() fetched more than 1 filter as specified. (%d)"), 
                            __FUNCTION__, __LINE__, __FILE__, ulTmpNumFetched );
                retval = TPR_FAIL;
                bWhileTestFailed = true;
                if ( pBaseFilter ) pBaseFilter->Release();
                break;
            }

            if ( 1 == ulTmpNumFetched )
            {   // Compare the two results
                if ( pBaseFilter != pFiltersInGraph[ulCount] ) 
                {
                    LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                                __FUNCTION__, __LINE__, __FILE__, hr );
                    retval = TPR_FAIL;
                    bWhileTestFailed = true;
                    if ( pBaseFilter ) pBaseFilter->Release();
                    break;
                }
                ulCount++;
                //we need to release this interface before getting the Next one
                if ( pBaseFilter ) pBaseFilter->Release();
            }
            else
            {
                // BUGBUG: how do we know it's done when get here. will Next() set ulTmpNumFetched as NULL or something else??
                break;//if Next didn't fetch a BaseFilter then we're done
            }
            pBaseFilter = NULL;             
        }

        if ( bWhileTestFailed ) continue;

        // Two enumerating methods should yeild same number of filters
        if ( ulCount != ulNumFetched )
        {
            LOG( TEXT("%S: ERROR %d@%S - The number of filter enumerated (%d) doesn't match the previous one (%d)."), 
                        __FUNCTION__, __LINE__, __FILE__, ulCount, ulNumFetched );
            retval = TPR_FAIL;
            continue;
        }

        // This test passes -1 to Skip()
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumFilters->Skip( ULONG_MAX );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Skip w/ -1 failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
        }

        // Since -1 is the the largest number as an unsigned long, after 
        // calling a Skip(-1) there should be no more filter to enumerate
        hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( 0 != ulTmpNumFetched )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() should return 0, not %d fetched filters, since we skipped all filters"), 
                        __FUNCTION__, __LINE__, __FILE__, ulTmpNumFetched );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - Next() should return NULL IBaseFilter pointer since we skipped all filters."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // This test passes 0 to Skip(). Subsequent Next() should not be affected
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        // Skip 0 before we call a Next()
        hr = pEnumFilters->Skip( 0 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Skip w/ 0 failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter != pFiltersInGraph[0] )
        {
            LOG( TEXT("%S: ERROR %d@%S - IBaseFilter pointers mismatch. (0x%08x, 0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pBaseFilter, (LONG)pFiltersInGraph[0] );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter ) pBaseFilter->Release();
        pBaseFilter = NULL; //we need to release this interface before getting the Next one
        
        // Skip 0 after we call a Next, the next Next() should not be affected
        hr = pEnumFilters->Skip( 0 );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Skip w/ 0 failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
        }

        hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter != pFiltersInGraph[1] )
        {
            LOG( TEXT("%S: ERROR %d@%S - IBaseFilter pointers mismatch. (0x%08x, 0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pBaseFilter, (LONG)pFiltersInGraph[0] );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter ) pBaseFilter->Release();
        pBaseFilter = NULL;

        // This test passes 1 to Skip()
        if ( ulNumFetched >= 4 ) // test only when there are enough filters in graph
        {
            hr = pEnumFilters->Skip( 1 );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Skip w/ 1 failed. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }

            hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
            if ( pBaseFilter != pFiltersInGraph[3] ) // [2] should be skipped
            {
                LOG( TEXT("%S: ERROR %d@%S - IBaseFilters mismatch. (0x%08x, 0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, (LONG)pBaseFilter, (LONG)pFiltersInGraph[3] );
                retval = TPR_FAIL;
                continue;
            }
            if ( pBaseFilter ) pBaseFilter->Release();
            pBaseFilter = NULL;
        }

        // This test passes a very large number to Skip
        // Subsequent calls to Next() should fetch nothing
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        
        hr = pEnumFilters->Skip( MAX_NUM_FILTERS );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Skip w/ %d failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_FILTERS, hr );
            retval = TPR_FAIL;
        }

        hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        // Number of filters fetched should be 0
        if ( 0 != ulTmpNumFetched )
        {
            LOG( TEXT("%S: ERROR %d@%S - We skipped %d filters and number of fetched filter (%d) not equal to zero."), 
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_FILTERS, ulTmpNumFetched );
            retval = TPR_FAIL;
            continue;
        }
        // Filter pointer should be NULL
        if ( NULL != pBaseFilter )
        {
            LOG( TEXT("%S: ERROR %d@%S - NULL IBaseFilter should be returned since we skipped %d filters."), 
                        __FUNCTION__, __LINE__, __FILE__, MAX_NUM_FILTERS );
            retval = TPR_FAIL;
            continue;
        }

        // This test calls Reset() before calling Next()
        // The Next() should always start from the first filter
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter != pFiltersInGraph[0] )
        {
            LOG( TEXT("%S: ERROR %d@%S - IBaseFilter pointers mismatch. (0x%08x, 0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pBaseFilter, (LONG)pFiltersInGraph[0] );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter ) pBaseFilter->Release();
        pBaseFilter = NULL;

        // This test calls Reset() several times between two calls to Next()
        // They should have no effect on the Next()
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter != pFiltersInGraph[0] )
        {
            LOG( TEXT("%S: ERROR %d@%S - IBaseFilter pointers mismatch. (0x%08x, 0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pBaseFilter, (LONG)pFiltersInGraph[0] );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter ) pBaseFilter->Release();
        pBaseFilter = NULL;
        
        // This test repeated calls Next() until there's no more filters to fetch
        // Then a call to Reset() should reset the sequence to the beginning
        do  // Enumerate till the end
        {  
            hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                break;
            }
            if ( pBaseFilter ) pBaseFilter->Release();
            pBaseFilter=NULL;
        } while ( ulTmpNumFetched );    // so when approaching the end of filter list, it set number to fetch as zero.

        // Do a reset
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        // Now Next() will start from the beginning again
        hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter != pFiltersInGraph[0] )
        {
            LOG( TEXT("%S: ERROR %d@%S - IBaseFilter pointers mismatch. (0x%08x, 0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, (LONG)pBaseFilter, (LONG)pFiltersInGraph[0] );
            retval = TPR_FAIL;
            continue;
        }
        if ( pBaseFilter ) pBaseFilter->Release();
        pBaseFilter = NULL;

     }    // end of media files    

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


//
// Test the Clone function of IEnumFilters.
//
TESTPROCAPI 
IEnumFiltersCloneTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

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
    IEnumFilters  *pEnumFilters = NULL;
    IBaseFilter      *pBaseFilter = NULL;

    IEnumFilters  *pCloneEnumFilters = NULL;
    IBaseFilter   *pCloneBaseFilter = NULL;

    unsigned long ulTmpNumFetched, ulNumToFetch = 1000;
    unsigned long ulCloneTmpNumFetched;

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
        if ( pCloneEnumFilters )
        {
            pCloneEnumFilters->Release();
            pCloneEnumFilters = NULL;
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

        // This test passes NULL to Clone()
        hr = pEnumFilters->Clone( NULL );
        if ( E_POINTER != hr )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Clone() w/ NULL should return E_POINTER. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // This test clone a new enumerator, we compare it with the original enumerator
        // by comparing the result of each identical subsequent call to Next()
        hr = pEnumFilters->Clone( &pCloneEnumFilters );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Clone() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // reset both enumerator
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        hr = pCloneEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Cloned IEnumFilters::Reset() failed. (0x%08x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        do  // compare the the two enumeration sequence
        {   // fetch one filter each time
            if ( pBaseFilter ) pBaseFilter->Release();
            if ( pCloneBaseFilter ) pCloneBaseFilter->Release();

            hr = pEnumFilters->Next( 1, &pBaseFilter, &ulTmpNumFetched );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }
            hr = pCloneEnumFilters->Next( 1, &pCloneBaseFilter, &ulCloneTmpNumFetched );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Cloned IEnumFilters::Next() failed. (0x%x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                retval = TPR_FAIL;
                continue;
            }

            if( (ulTmpNumFetched != ulCloneTmpNumFetched) ||
                (pBaseFilter != pCloneBaseFilter) )
            {   // error, quit
                LOG( TEXT("%S: ERROR %d@%S - Cloned IEnumFilters returned (%d, 0x%08x), orig returned (%d, 0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, ulCloneTmpNumFetched, (LONG)pCloneBaseFilter,
                            ulTmpNumFetched, (LONG)pBaseFilter );
                retval = TPR_FAIL;
                break;
            }
        
            if ( 0 != ulTmpNumFetched )
            {   // haven't reached the end yet. release pointers and ready to fetch again
                pBaseFilter = NULL;
                pCloneBaseFilter = NULL;
            }
            // otherwise, we've reached the end. no need to release pointers this time
                                    
        } while ( ulTmpNumFetched ); // do for every filter fetched

    }    // end of media files.

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
