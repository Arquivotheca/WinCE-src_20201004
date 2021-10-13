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
//  IEnumRegFilter interface test
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

#define MAX_NUM_REGFILTERS 400

TESTPROCAPI 
IEnumRegFiltersTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
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
    if ( pTestDesc->RemoteGB() )
        return TPR_SKIP;

    IFilterMapper       *pFilterMapper = NULL;
    IEnumRegFilters     *pEnumRegFilters = NULL;
    IEnumRegFilters     *pCloneEnumRegFilters = NULL;
    unsigned long ulNumFetched, ulTmpNumFetched, ulNumToFetch = 1000;
    DWORD   dwMerit = MERIT_DO_NOT_USE; // Lowest possible merit value

    REGFILTER * pTmpFilter = NULL;
    REGFILTER * pTmpFilter2 = NULL;
    REGFILTER * pAllFilters[MAX_NUM_REGFILTERS];
    REGFILTER * pTmpAllFilters[MAX_NUM_REGFILTERS];

    for ( int i = 0; i < MAX_NUM_REGFILTERS; i++ ) 
    {
        pAllFilters[i] = NULL; 
        pTmpAllFilters[i] = NULL;
    }

    // Initialize COM lib
    hr = CoInitializeEx ( 0, COINIT_MULTITHREADED );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - CoInitializeEx w/ COINIT_MULTITHREADED failed. 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Create the Filter Mapper
    hr = CoCreateInstance( CLSID_FilterMapper, 
                           NULL, 
                           CLSCTX_INPROC_SERVER, 
                           IID_IFilterMapper, 
                           (void **)&pFilterMapper );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - CoCreateInstance w/ CLSID_FilterMapper failed. 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( !pFilterMapper )
    {
        LOG( TEXT("%S: ERROR %d@%S - CoCreateInstance w/ CLSID_FilterMapper returned NULL pointer. "), 
            __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Get the IEnumRegFilters interface
    hr = pFilterMapper->EnumMatchingFilters( &pEnumRegFilters,  // enumerator returned
                                             dwMerit,           // enumerate only filters w/ at least this merit
                                             false,             // TRUE if there must be a least one input pin
                                             GUID_NULL,         // input major type. NULL if don't care
                                             GUID_NULL,         // input sub type, NULL if don't care
                                             false,             // bRender, if the input must be rendered by this filter
                                             false,             // bOutputNeeded, must be at least one output pin
                                             GUID_NULL,         // output major type, NULL if don't care
                                             GUID_NULL          // outptu sub type, NULL if don't care
                                             );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - EnumMatchingFilters failed. 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( !pEnumRegFilters )
    {
        LOG( TEXT("%S: ERROR %d@%S - EnumMatchingFilters returned a NULL enumerator."), 
            __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pEnumRegFilters->Reset();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Reset() failed. 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Tests Next() method
    // This test passes NULL as the parameter to ::Next()
    hr = pEnumRegFilters->Next( 1, NULL, &ulNumFetched );
    if ( E_POINTER != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - Next() w/ NULL didn't return E_POINTER. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pEnumRegFilters->Next( 1, NULL, NULL );
    if ( E_POINTER != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - Next() w/ NULL didn't return E_POINTER. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    
    // This test passes a very big number as the parameter of no. of filters to fetch
    hr = pEnumRegFilters->Reset();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Reset() failed. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pEnumRegFilters->Next( ulNumToFetch, pAllFilters, &ulNumFetched );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Next() failed. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    if ( ulNumFetched >= ulNumToFetch )
    {
        LOG( TEXT("%S: ERROR %d@%S - Number of filters fetched (%d) is larger than number of filters to fetch (%d)."), 
            __FUNCTION__, __LINE__, __FILE__, ulNumFetched, ulNumToFetch );
        retval = TPR_FAIL;
        goto cleanup;
    }
               
    // This test enumerates one filter each time, compares it with the pointers
    // stored in pAllFilters[]
    hr = pEnumRegFilters->Reset();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Reset() failed. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pEnumRegFilters->Next( ulNumToFetch, pTmpAllFilters, &ulTmpNumFetched );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Next() failed. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    
    if ( ulTmpNumFetched != ulNumFetched )
    {
        LOG( TEXT("%S: ERROR %d@%S - The number of filters fetched %d is different from previous one %d."), 
            __FUNCTION__, __LINE__, __FILE__, ulTmpNumFetched, ulNumFetched );
        retval = TPR_FAIL;
        goto cleanup;
    }
    
    // Check if the two arrays of REGFILTER structures are identical
    for ( DWORD i = 0; i < ulTmpNumFetched; i++ )
    {        
        if( !IsEqualGUID( pTmpAllFilters[i]->Clsid, pAllFilters[i]->Clsid ) ||
            wcscmp( pTmpAllFilters[i]->Name, pAllFilters[i]->Name ) )
        {
            LOG( TEXT("%S: ERROR %d@%S - Filter %d in two arrays are different."), 
                __FUNCTION__, __LINE__, __FILE__, i );
            retval = TPR_FAIL;
            break;
        }
    }

    hr = pEnumRegFilters->Reset();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Reset() failed. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    unsigned long ulCount = 0;
    while( SUCCEEDED(pEnumRegFilters->Next( 1, &pTmpFilter, &ulTmpNumFetched )) )
    {
        if ( ulTmpNumFetched > 1 )
        {
            LOG( TEXT("%S: ERROR %d@%S - The number of Filter fetched %d is invalid after specifying 1 in Next()."), 
                __FUNCTION__, __LINE__, __FILE__, ulTmpNumFetched );
            retval = TPR_FAIL;
            goto cleanup;
        }
        if ( 1 == ulTmpNumFetched )
        {   
            // Compare the two results
            if ( !IsEqualGUID(pTmpFilter->Clsid, pAllFilters[ulCount]->Clsid) ||
                  wcscmp(pTmpFilter->Name, pAllFilters[ulCount]->Name) )
            {
                LOG( TEXT("%S: ERROR %d@%S - Next() returned a filter which does not match the one in the array with index %d."), 
                    __FUNCTION__, __LINE__, __FILE__, ulCount );
                retval = TPR_FAIL;
                goto cleanup;
            }
            ulCount++;
            CoTaskMemFree( pTmpFilter ); //we need to release this interface before getting the Next one
            pTmpFilter = NULL;
        }
        else
        {
            break;//if Next didn't fetch a BaseFilter then we're done
        }
    }
    // Two enumerating methods should yeild same number of filters
    if ( ulCount != ulNumFetched )
    {
        LOG( TEXT("%S: ERROR %d@%S - The number of filters fetched %d is different from previous one %d."), 
            __FUNCTION__, __LINE__, __FILE__, ulCount, ulNumFetched );
        retval = TPR_FAIL;
        goto cleanup;
    }


    // Test Skip() method
    hr = pEnumRegFilters->Reset();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumFilters::Reset failed. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    
    // Currently Skip() is not implemented 
    hr = pEnumRegFilters->Skip( ULONG_MAX );
    if ( E_NOTIMPL != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - Skip w/ -1 didn't return E_NOTIMPL. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pEnumRegFilters->Skip( 0 );
    if ( E_NOTIMPL != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - Skip w/ 0 didn't return E_NOTIMPL. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pEnumRegFilters->Skip( 1 );
    if ( E_NOTIMPL != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - Skip w/ 1 didn't return E_NOTIMPL. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    hr = pEnumRegFilters->Skip( MAX_NUM_REGFILTERS );
    if ( E_NOTIMPL != hr )
    {
        LOG( TEXT("%S: ERROR %d@%S - Skip w/ %d didn't return E_NOTIMPL. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, MAX_NUM_REGFILTERS, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }


    // Test Reset() method
    // This test calls Reset() before calling Next()
    // The Next() should always start from the first filter
    hr = pEnumRegFilters->Reset();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Reset() failed. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pEnumRegFilters->Next( 1, &pTmpFilter, &ulTmpNumFetched );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Next() w/ 1 failed. (0x%08x)"), 
        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    
    hr = pEnumRegFilters->Reset();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Reset() failed. (0x%08x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = pEnumRegFilters->Next( 1, &pTmpFilter2, &ulTmpNumFetched );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Next() w/ 1 failed. (0x%08x)"), 
        __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    

    // Check if the REGFILTER structures are identical
    if ( !IsEqualGUID(pTmpFilter2->Clsid, pTmpFilter->Clsid) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Filter information mismatch "), 
        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
    }
    if ( 0 != wcscmp(pTmpFilter2->Name, pTmpFilter->Name) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Filter information mismatch "), 
        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
    }

    if ( !IsEqualGUID(pTmpFilter->Clsid, pAllFilters[0]->Clsid) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Filter information mismatch"), 
        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
    }
    if ( 0 != wcscmp(pTmpFilter->Name, pAllFilters[0]->Name) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Filter information mismatch"), 
        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
    }

    CoTaskMemFree( pTmpFilter ); pTmpFilter = NULL;
    CoTaskMemFree( pTmpFilter2 ); pTmpFilter2 = NULL;

    // This test calls Reset() several times between two calls to Next()
    // They should have no effect on the Next()
    pEnumRegFilters->Reset();
    pEnumRegFilters->Reset();
    pEnumRegFilters->Reset();
    hr = pEnumRegFilters->Next( 1, &pTmpFilter, &ulTmpNumFetched );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Next() failed. (0x%08x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    if ( !IsEqualGUID(pTmpFilter->Clsid, pAllFilters[0]->Clsid) ||
            wcscmp(pTmpFilter->Name, pAllFilters[0]->Name) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Filter information mismatch."), 
                __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    CoTaskMemFree(pTmpFilter); pTmpFilter = NULL;
    
    // This test repeated calls Next() until there's no more filters to fetch
    // Then a call to Reset() should reset the sequence to the beginning
    do  // Enumerate till the end
    {   
        hr = pEnumRegFilters->Next( 1, &pTmpFilter, &ulTmpNumFetched );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Next() failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
            retval = TPR_FAIL;
            goto cleanup;
        }
        CoTaskMemFree( pTmpFilter ); pTmpFilter = NULL;
    }while ( ulTmpNumFetched );

    // Do a reset
    hr = pEnumRegFilters->Reset();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilter::Reset failed. (0x%08x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    // Now Next() will start from the beginning again
    hr = pEnumRegFilters->Next( 1, &pTmpFilter, &ulTmpNumFetched );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Next() failed. (0x%08x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
        goto cleanup;
    }

    if ( !IsEqualGUID(pTmpFilter->Clsid, pAllFilters[0]->Clsid) ||
            wcscmp(pTmpFilter->Name, pAllFilters[0]->Name) )
    {
        LOG( TEXT("%S: ERROR %d@%S - Filter information mismatch."), 
                __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }
    CoTaskMemFree(pTmpFilter); pTmpFilter = NULL;

    // Test Clone() method
    // Clone() is currently not implemented
    if ( E_NOTIMPL != pEnumRegFilters->Clone(NULL) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Clone should return E_NOTIMPL."), 
                __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }
    if ( E_NOTIMPL != pEnumRegFilters->Clone(&pCloneEnumRegFilters) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IEnumRegFilters::Clone should return E_NOTIMPL."), 
                __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

cleanup:

    // release pointers
    if ( pTmpFilter ) CoTaskMemFree( pTmpFilter );
    
    for ( int i = 0; i < MAX_NUM_REGFILTERS; i++ )
    {
        if ( pAllFilters[i] ) CoTaskMemFree( pAllFilters[i] );
        if ( pAllFilters[i] ) CoTaskMemFree( pTmpAllFilters[i] );
    }

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Uninitialize the COM library
    CoUninitialize();
    return retval;  
}