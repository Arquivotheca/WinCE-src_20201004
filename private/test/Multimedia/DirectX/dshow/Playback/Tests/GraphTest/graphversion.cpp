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
//  IGraphVersion interface test
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

TESTPROCAPI 
IGraphVersionTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    PlaybackMedia      *pMedia = NULL;

    long lVersion, lTmpVersion;
    unsigned long  ulNumFetched;
    IGraphVersion    *pGraphVersion = NULL;
    IGraphBuilder    *pFilterGraph = NULL;
    IEnumFilters    *pEnumFilters = NULL;
    IBaseFilter        *pTestFilter  = NULL;

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    for ( int i = 0; i < pTestDesc->GetNumMedia(); i++ )
    {
        // Reset the testGraph
        testGraph.Reset();
        if ( pFilterGraph )
        {
            pFilterGraph->Release();
            pFilterGraph = NULL;
        }
        if ( pGraphVersion )
        {
            pGraphVersion->Release();
            pGraphVersion = NULL;
        }
        if ( pTestFilter )
        {
            pTestFilter->Release();
            pTestFilter = NULL;
        }
        if ( pEnumFilters )
        {
            pEnumFilters->Release();
            pEnumFilters = NULL;
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

        // Create an empty graph
        hr = testGraph.BuildEmptyGraph();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Create an empty graph failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
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

        // Get IGraphVersion Interface
        hr = pFilterGraph->QueryInterface( IID_IGraphVersion,  (void**)&pGraphVersion );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - QueryInterface w/ IID_IGraphVersion failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( !pGraphVersion )
        {
            LOG( TEXT("%S: ERROR %d@%S - NULL IGraphVersion pointer returned."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // QueryVersion with NULL pointer input without rendering a file
        if ( E_POINTER != pGraphVersion->QueryVersion(NULL) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IGraphVersion::QueryVersion w/ NULL didn't return E_POINTER. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // QueryVersion with valid parameter
        hr = pGraphVersion->QueryVersion( &lVersion );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IGraphVersion::QueryVersion failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Since we didn't render file, the graphversion should be 0(?1).
        if ( 0 != lVersion )
        {
            LOG( TEXT("%S: ERROR %d@%S - Before rendering a file, the version number should be zero not %d."), 
                        __FUNCTION__, __LINE__, __FILE__, lVersion );
            retval = TPR_FAIL;
            continue;
        }

        // safe release the interface
        pGraphVersion->Release();
        pGraphVersion = NULL;

        testGraph.SetMedia(pMedia);

        // Now render the file so the filter graph is really created
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

        // Get IGraphVersion Interface
        hr = pFilterGraph->QueryInterface( IID_IGraphVersion,  (void**)&pGraphVersion );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - QueryInterface w/ IID_IGraphVersion failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( !pGraphVersion )
        {
            LOG( TEXT("%S: ERROR %d@%S - NULL IGraphVersion pointer returned."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // QueryVersion with NULL pointer input after rendering a file
        if ( E_POINTER != pGraphVersion->QueryVersion(NULL) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IGraphVersion::QueryVersion w/ NULL didn't return E_POINTER. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // QueryVersion with valid parameter
        hr = pGraphVersion->QueryVersion( &lTmpVersion );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IGraphVersion::QueryVersion failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // This time the graph version should increment,but not necessarily by 1
        if ( lTmpVersion <= lVersion )
        {
            LOG( TEXT("%S: ERROR %d@%S - The graph version %d is less than previous one %d. "), 
                        __FUNCTION__, __LINE__, __FILE__, lTmpVersion, lVersion );
            retval = TPR_FAIL;
            continue;
        }
        
        lVersion = lTmpVersion; // Update the current version

        // This test will change the filter graph and check the graph version
        // Get IEnumFilters interface so that we can add/remove filters
        hr = pFilterGraph->EnumFilters( &pEnumFilters );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - EnumFilters failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }
        if ( !pEnumFilters )
        {
            LOG( TEXT("%S: ERROR %d@%S - NULL IEnumFilters returned."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            retval = TPR_FAIL;
            continue;
        }

        // Fetch one filter in the graph
        hr = pEnumFilters->Reset();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        hr = pEnumFilters->Next( 1, &pTestFilter, &ulNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Next() failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Remove this filter from graph
        hr = pFilterGraph->RemoveFilter( pTestFilter );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - RemoveFilter failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

         // Now fetch the graph version again
        hr = pGraphVersion->QueryVersion( &lTmpVersion );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - QueryVersion failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // The graph version should increment by 1
        lVersion++;
        if ( lVersion != lTmpVersion )
        {
            LOG( TEXT("%S: ERROR %d@%S - incorrect version after removing filter. (before: %d, after: %d)"), 
                        __FUNCTION__, __LINE__, __FILE__, lVersion - 1, lTmpVersion );
            retval = TPR_FAIL;
            continue;
        }

        // Now add the filter back into the graph
        hr = pFilterGraph->AddFilter( pTestFilter, NULL );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - AddFilter failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // Do this to prevent previous bug (wrong version number) from propagating
        lVersion = lTmpVersion;

        // Now fetch the graph version again
        hr = pGraphVersion->QueryVersion( &lTmpVersion );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - QueryVersion failed. (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            continue;
        }

        // The graph version should increment by 1
        lVersion++;
        if ( lVersion != lTmpVersion )
        {
            LOG( TEXT("%S: ERROR %d@%S - incorrect version after removing filter. (before: %d, after: %d)"), 
                        __FUNCTION__, __LINE__, __FILE__, lVersion - 1, lTmpVersion );
            retval = TPR_FAIL;
            continue;
        }
    }    // end of media list

    // release pointers
    testGraph.Reset();

	// Readjust return value if HRESULT matches/differs from expected in some instances
	retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;    
}