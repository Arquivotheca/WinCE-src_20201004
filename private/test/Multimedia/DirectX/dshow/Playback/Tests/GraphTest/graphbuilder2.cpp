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
//  IGraphBuilder2 tests
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "TuxGraphTest.h"
#include "logging.h"
#include "StringConversions.h"
#include "MMTestParser.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"
#include "VideoStampFilterGuids.h"

typedef  HRESULT ( __stdcall *pFuncpfv)();

TESTPROCAPI 
IGraphBuilder2_AddFilterByCLSID_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IGraphBuilder2  *pGraph = NULL;
    FilterInfo  tmpFilterInfo;

    // Handle tux messages
    if ( SPR_HANDLED == HandleTuxMessages(uMsg, tpParam) )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    // Test Graph
    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // From the config, determine the media files to be used
    PlaybackMedia *pMedia = NULL;
    pMedia = pTestDesc->GetMedia( 0 );
    if ( !pMedia )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the media object"), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return TPR_FAIL;
    }

    testGraph.SetMedia( pMedia );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        return TPR_FAIL;
    }

    // Enable the verification required
    bool bGraphVerification = false;
    VerificationList verificationList = pTestDesc->GetVerificationList();
    VerificationList::iterator iterator = verificationList.begin();
    while ( iterator != verificationList.end() )
    {
        VerificationType type = (VerificationType)iterator->first;
        if ( testGraph.IsCapable(type) ) 
        {
            hr = testGraph.EnableVerification( type, iterator->second );
            if (FAILED_F(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to enable verification %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
                break;
            }
            bGraphVerification = true;
        }
        else 
        {
            LOG(TEXT("%S: WARNING %d@%S - unrecognized verification requested %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
        }
        iterator++;
    }

    pGraph = (IGraphBuilder2*)(testGraph.GetGraph());
    if ( !pGraph )
        goto cleanup;

    // Add filters manually if custom configurations need to be done.
    for ( DWORD i = 0; i < pTestDesc->GetNumOfFilters(); i++ )
    {
        WCHAR   wszFilterName[MAX_PATH] = {0};
        hr = FilterInfoMapper::GetFilterInfo( pTestDesc->GetFilterName(i), &tmpFilterInfo );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to get the filter info for %s (0x%x)"), 
                            __FUNCTION__, __LINE__, __FILE__, pTestDesc->GetFilterName(i), hr );
            retval = TPR_FAIL;
            goto cleanup;
        }

        hr = pGraph->AddFilterByCLSID( tmpFilterInfo.guid, wszFilterName );
        // If it failed as expected, then it is a valid case. reset HR.
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to add %s (0x%x)" ), 
                            __FUNCTION__, __LINE__, __FILE__, pTestDesc->GetFilterName(i), hr );
            retval = TPR_FAIL;
            goto cleanup;
        }
    }

    // Signal the start of verification
    if ( bGraphVerification )
    {
        hr = testGraph.StartVerification();
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr );
        }
    }

    // Build the graph
    hr = testGraph.BuildGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else 
    {
        LOG( TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl() );
    }

    // verify the filters with the remote graph matches what we set in the test config.
    hr = testGraph.GetVerificationResults();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - verification failed. (0x%08x)"), __FUNCTION__, __LINE__, __FILE__, hr );
        retval = TPR_FAIL;
    }
    else 
    {
        LOG( TEXT("%S: verification succeeded"), __FUNCTION__ );
    }

cleanup:
    if ( pGraph )
        pGraph->Release();

    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    
    pTestDesc->Reset();

    return retval;    
}

HRESULT 
FindInterfaceOnGraphTest( IGraphBuilder2 *pGraph, GUID *pTestGuid, DWORD dwNumOfInstances )
{
    CheckPointer( pGraph, E_POINTER );
    CheckPointer( pTestGuid, E_POINTER );

    HRESULT hr = S_OK;
    WCHAR   wszInterfaceName[MAX_PATH] = {0};
    IUnknown *pFilter = NULL;
    IUnknown *pTestInterface = NULL;
    IEnumFilterInterfaces *pInterfaces = NULL;

    if(FAILED(hr = pGraph->FindInterfacesOnGraph( *pTestGuid, &pInterfaces )))
    {
        LOG( TEXT("%S: INFO %d@%S - FindInterfaceOnGraph failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    if(FAILED(hr = pInterfaces->Reset()))
    {
        LOG( TEXT("%S: INFO %d@%S - IEnumFilterInterfaces::Reset() failed. (0x%08x)"), 
                    __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    // Enumerate all filters which expose this interface
    DWORD dwFilters = 0;
    do 
    {
        if(SUCCEEDED(hr = pInterfaces->Next( &pFilter, MAX_PATH, wszInterfaceName )))
        {        
            if(FAILED(hr = pFilter->QueryInterface( *pTestGuid, (void **)&pTestInterface )))
            {
                LOG( TEXT("%S: INFO %d@%S - QueryInterface failed. (0x%08x)"), 
                            __FUNCTION__, __LINE__, __FILE__, hr );
                break;
            }
            dwFilters++;
            if ( pTestInterface )
            {
                pTestInterface->Release();
                pTestInterface = NULL;
            }
            if ( pFilter )
            {
                pFilter->Release();
                pFilter = NULL;
            }
        }
    } while ( SUCCEEDED(hr) );
    
    // If the loop exits because of the end of enumerator, reset the hr result.
    if ( dwFilters )
        hr = S_OK;

    if ( dwFilters != dwNumOfInstances )
    {
        LOG( TEXT("%S: ERROR %d@%S - The number of interfaces found (%d) does not match the specified (%d)"), 
                    __FUNCTION__, __LINE__, __FILE__, dwFilters, dwNumOfInstances );
        hr = E_FAIL;
    }

cleanup:
    if ( pTestInterface )
        pTestInterface->Release();

    if ( pInterfaces )
        pInterfaces->Release();
    if ( pFilter )
        pFilter->Release();

    return hr;
}

TESTPROCAPI 
IGraphBuilder2_FindInterfacesOnGraph_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IGraphBuilder2 *pGraph = NULL;
    IVMRFilterConfig        *pVMRConfig = NULL;
    IEnumFilterInterfaces   *pEnumFilterInterfaces = NULL;

    // Handle tux messages
    if ( SPR_HANDLED == HandleTuxMessages(uMsg, tpParam) )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    // Test Graph
    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // From the config, determine the media files to be used
    PlaybackMedia *pMedia = NULL;

    pMedia = pTestDesc->GetMedia( 0 );
    if ( !pMedia )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the media object"), 
                    __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    testGraph.SetMedia( pMedia );

    // Build the graph
    hr = testGraph.BuildGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        retval = TPR_FAIL;
        goto cleanup;
    }
    else 
    {
        LOG( TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl() );
    }

    pGraph = (IGraphBuilder2*)(testGraph.GetGraph());
    if ( !pGraph )
        goto cleanup;

    // An interface can be exposed by multiple filters.
    hr = FindInterfaceOnGraphTest( pGraph, pTestDesc->GetTestGuid(), pTestDesc->GetNumOfFilters() );
    if ( FAILED(hr) )
    {
        retval = TPR_FAIL;
        goto cleanup;
    }
       
cleanup:
    if ( pGraph )
        pGraph->Release();
    if ( pEnumFilterInterfaces )
        pEnumFilterInterfaces->Release();

    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    
    pTestDesc->Reset();

    return retval;    
}

TESTPROCAPI 
IGraphBuilder2_AddUnsignedFilterToGraph_Test( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    IGraphBuilder2  *pGraph = NULL;
    FilterInfo  tmpFilterInfo;
    TestGraph testGraph;

    // Handle tux messages
    if ( SPR_HANDLED == HandleTuxMessages(uMsg, tpParam) )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    BuildGraphTestDesc* pTestDesc = (BuildGraphTestDesc*)lpFTE->dwUserData;

    //Register video stamp reader filter
    HINSTANCE hInst = LoadLibrary(TEXT("VideoStampReader.dll"));
    pFuncpfv pfv = NULL;

    if( hInst == NULL)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        LOG( TEXT("%S: ERROR %d@%S - failed to load VideoStampReader DLL!"), 
                    __FUNCTION__, __LINE__, __FILE__ );
        goto cleanup;

    }

    pfv = (pFuncpfv)GetProcAddress(hInst, TEXT("DllRegisterServer"));
    if(pfv == NULL )
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        LOG( TEXT("%S: ERROR %d@%S - failed to GetProcAddress of DllRegisterServer!"), 
                    __FUNCTION__, __LINE__, __FILE__ );
        FreeLibrary(hInst);
        goto cleanup;
    }
    
    // Test Graph
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    // From the config, determine the media files to be used
    PlaybackMedia *pMedia = NULL;
    pMedia = pTestDesc->GetMedia( 0 );
    if ( !pMedia )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve the media object"), 
                    __FUNCTION__, __LINE__, __FILE__ );
        hr = E_FAIL;
        goto cleanup;
    }

    testGraph.SetMedia( pMedia );

    // Build an empty graph
    hr = testGraph.BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build an empty graph (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, hr );
        goto cleanup;
    }

    pGraph = (IGraphBuilder2*)(testGraph.GetGraph());
    if ( !pGraph )
        goto cleanup;

    // Add filter manually 
    WCHAR   wszFilterName[MAX_PATH] = {0};

    hr = pGraph->AddFilterByCLSID( CLSID_VideoStampReaderFilter, wszFilterName );
    // If using remote GB, it should fail, otherwise, it should succeed
    if(pTestDesc->RemoteGB() )
    {
        if ( SUCCEEDED(hr) )
        {
            LOG( TEXT("Remote GB, adding unsigned filter should fail but it succeeded!!" ));
            hr = E_FAIL;
            goto cleanup;
        }else
            LOG( TEXT("Remote GB, adding unsigned filter should fail and it failed!!" ));
    }else{
        if ( FAILED(hr) )
        {
            LOG( TEXT("Normal GB, adding unsigned filter should succeed but it failed!!" ));
            hr = E_FAIL;
            goto cleanup;
        }else
            LOG( TEXT("Normal GB, adding unsigned filter should succeed and it succeeded!!" ));
    }

    // Build the graph
    hr = testGraph.BuildGraph();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), 
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr );
        goto cleanup;
    }
    else 
    {
        LOG( TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl() );
    }

cleanup:
    if(FAILED(hr))
        retval = TPR_FAIL;

    (*pfv)();
    if( hInst )
        FreeLibrary(hInst);

    if ( pGraph )
        pGraph->Release();

    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    
    pTestDesc->Reset();

    return retval;    
}


