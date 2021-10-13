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
//  IFileSourceFilter
//
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"
#include "utility.h"
#include "StringConversions.h"

HRESULT GetMediaType( PlaybackInterfaceTestDesc *pTestDesc, PlaybackMedia* pMedia, AM_MEDIA_TYPE &mt)
{
    CheckPointer( pTestDesc, E_POINTER);
    HRESULT hr = S_OK;
    TestGraph testGraph;
    IPin* pPin = NULL;

    // skip the test since the test graph couldn't get the list of filters in the graph
    // when rendering is happened in the secure chamber.
    if ( pTestDesc->RemoteGB() )
        return S_OK;

    //set Meida
    hr = testGraph.SetMedia(pMedia);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to set media for testGraph %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        goto cleanup;
    }
    else 
    {
        LOG(TEXT("%S: successfully set media for %s"), __FUNCTION__, pMedia->GetUrl());
    }

    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        goto cleanup;
    }
    else 
    {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }
    
    FilterDesc* pFilterDesc = NULL;    
    pFilterDesc = testGraph.GetFilterDesc(SourceFilter, 0);    
    if(!pFilterDesc)
    {
        LOG(TEXT("%S: ERROR %d@%S - Failed to get FilterDesc. "), __FUNCTION__, __LINE__, __FILE__);
        hr = E_FAIL;
        goto cleanup;
    }
    
    pPin = pFilterDesc->GetPin(0, PINDIR_OUTPUT);
    if (!pPin)
    {
        LOG(TEXT("%S: ERROR %d@%S - Failed to get pin. "), __FUNCTION__, __LINE__, __FILE__);
        hr = E_FAIL;
        goto cleanup;
    }
    else
    {
        // Get the input pin's media type
        hr = pPin->ConnectionMediaType(&mt);
        PrintMediaType(&mt);
    }


cleanup:    

    if(pPin)
    {
        pPin->Release();
        pPin = NULL;
    }
    
    // Reset the testGraph
    testGraph.Reset();

    return hr;
}

HRESULT TestIFileSourceFilter( TestGraph *pGraph, PlaybackInterfaceTestDesc *pTestDesc, PlaybackMedia* pMedia, AM_MEDIA_TYPE* mediaType)
{
    CheckPointer( pGraph, E_POINTER );
    CheckPointer( pTestDesc, E_POINTER );
    CheckPointer( pMedia, E_POINTER );
    HRESULT hr;
    LPOLESTR fileName = NULL;
    IFileSourceFilter *pIFileSourceFilter = NULL;
    GUID Filter_CLSID = GUID_NULL;
    
    AM_MEDIA_TYPE  mt;
    memset(&mt, 0, sizeof(AM_MEDIA_TYPE));    
    
    //get media type 
    AM_MEDIA_TYPE  mt_Verification;
    memset(&mt_Verification, 0, sizeof(AM_MEDIA_TYPE));    

    hr = pGraph->BuildEmptyGraph();
    if ( FAILED(hr) )
    {
        LOG(TEXT("%S: ERROR %d@%S - Failed to create the FGM. (0x%x) "), __FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;
    }

    hr = GetMediaType( pTestDesc, pMedia, mt_Verification);
    if(FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - Failed to get Meida Type for %s. (0x%x) "), __FUNCTION__, __LINE__, __FILE__,
            pMedia->GetUrl(), hr);
        goto cleanup;
    }

    //set filter CLSID
	if ( _tcsstr( pMedia->GetUrl(), TEXT(".wmv") ) || _tcsstr( pMedia->GetUrl(), TEXT(".wma") ) )
	{
		Filter_CLSID = CLSID_NetShowSource;
	}
	else
		Filter_CLSID = _tcsnicmp(TEXT("http"), pMedia->GetUrl(),4) ? CLSID_AsyncReader:CLSID_URLReader;

    // Add filter to graph
    hr = pGraph->AddFilterByCLSID(Filter_CLSID,L"New Source Filter");
    if(FAILED(hr)) 
    {
        LOG(TEXT("%S: ERROR %d@%S - Failed to add filter to garph. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;
    }
    

    //query IFileSourceFilter interface
    hr = pGraph->FindInterfaceOnGraph(IID_IFileSourceFilter,(void**)&pIFileSourceFilter);
    if(FAILED(hr)) 
    {
        LOG(TEXT("%S: ERROR %d@%S - Failed to query IFileSourceFilter interface. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        goto cleanup;        
    }

    //Test Load(), invalid input parameter. 
    hr = pIFileSourceFilter->Load(NULL, mediaType);                
    if(SUCCEEDED(hr)) 
    {
        LOG(TEXT("%S: ERROR %d@%S -when input NULL file name,  expected returned value shoud be failed, but actually success.. (0x%x)"), 
            __FUNCTION__, __LINE__, __FILE__, hr);
        hr = E_FAIL;
        goto cleanup;
    }
    
    //Test Load(), normal case 
    WCHAR wszUrl[MAX_PATH];
    TCharToWChar(pMedia->GetUrl(), wszUrl, countof(wszUrl));
    
    hr = pIFileSourceFilter->Load((LPOLESTR)wszUrl, mediaType);                
    if(FAILED(hr)) 
    {
        LOG(TEXT("%S: ERROR %d@%S - Failed while trying to Load media %s. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, 
            pMedia->GetUrl(), hr);
        goto cleanup;
    }
    
    //Test GetCurFile(), normal case
    hr = pIFileSourceFilter->GetCurFile(&fileName, &mt);
    if(FAILED(hr)) 
    {
        LOG(TEXT("%S: ERROR %d@%S - Failed while trying to get file name and media type with media:%s. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, 
            pMedia->GetUrl(), hr);
        goto cleanup;
    }
    else 
    {
        int result = wcscmp((LPCWSTR)fileName,(LPCWSTR)pMedia->GetUrl());
        if(result != 0)
        {
            LOG(TEXT("%S: ERROR %d@%S - The file name: %s get from IFileSouirceFilter does not match actual media:%s. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, 
                fileName, pMedia->GetUrl(), hr);
            hr = E_FAIL;
            goto cleanup;
        }
        
        // If we couldn't get the media type through enumerating types on the pin of the source filter.
        if ( !mt_Verification.majortype.Data1 )
            goto cleanup;

        LOG(TEXT("The media majortype and subtype that get from IFileSouirceFilter are:"));
        PrintMediaType(&mt);
        LOG(TEXT("The actual media majortype and subtype are:"));
        PrintMediaType(&mt_Verification);
        if( !IsEqualGUID(mt.majortype, mt_Verification.majortype) && 
			!IsEqualGUID(mt.majortype, GUID_NULL) && 
			!IsEqualGUID(mt_Verification.majortype, GUID_NULL) )
        {
            LOG(TEXT("%S: ERROR %d@%S - The media majortype get from IFileSouirceFilter does not match actual majortype. (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr);
            hr = E_FAIL;
            goto cleanup;
        }
        else if(!IsEqualGUID(mt.subtype, mt_Verification.subtype) && 
			    !IsEqualGUID(mt.subtype, GUID_NULL) && 
				!IsEqualGUID(mt_Verification.subtype, GUID_NULL) )
        {                    
            LOG(TEXT("%S: ERROR %d@%S - The media subtype get from IFileSouirceFilter does not match actual subtype. (0x%x)"),
                __FUNCTION__, __LINE__, __FILE__, hr);
            hr = E_FAIL;
            goto cleanup;
        }            
       
        //Test GetCurFile(), Meida Type is NULL
        hr = pIFileSourceFilter->GetCurFile(&fileName, NULL);
        if(FAILED(hr)) 
        {
            LOG(TEXT("%S: ERROR %d@%S - Failed while trying to get file name  with media:%s. (0x%x)"), __FUNCTION__, __LINE__, __FILE__, 
                pMedia->GetUrl(), hr);
            goto cleanup;
        }

        //Test GetCurFile(), invalid parameter
        hr = pIFileSourceFilter->GetCurFile(NULL, &mt);
        if(SUCCEEDED(hr)) 
        {
            LOG(TEXT("%S: ERROR %d@%S -when input NULL file name,  expected returned value shoud be failed, but actually success.. (0x%x)"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
            hr = E_FAIL;
            goto cleanup;
        } 
        hr = S_OK;
    }
    
cleanup:
    //release fileName
    if(fileName)
    {
        CoTaskMemFree(fileName);
    }
    
    //release the memory for the format block in the media type (if any). 
    if (mt.cbFormat != 0)
    {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL)
    {
        // Unecessary because pUnk should not be used, but safest.
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }

    //release the memory for the format block in the verification media type (if any). 
    if (mt_Verification.cbFormat != 0)
    {
        CoTaskMemFree((PVOID)mt_Verification.pbFormat);
        mt_Verification.cbFormat = 0;
        mt_Verification.pbFormat = NULL;
    }
    if (mt_Verification.pUnk != NULL)
    {
        // Unecessary because pUnk should not be used, but safest.
        mt_Verification.pUnk->Release();
        mt_Verification.pUnk = NULL;
    }

    //release pIFileSourceFilter, pIBaseFilter, pIUnkonwn
    if(pIFileSourceFilter)
    {
        pIFileSourceFilter->Release();
        pIFileSourceFilter = NULL;
    }

    return hr;
}


//Test IFileSourceFilter
TESTPROCAPI IFileSourceFilterTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

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

    //set media type
    AM_MEDIA_TYPE  mt;
    memset(&mt, 0, sizeof(AM_MEDIA_TYPE));    
    
    hr = GetMediaType( pTestDesc, pMedia, mt);
    if(FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - Failed to get Meida Type for %s. (0x%x) "), __FUNCTION__, __LINE__, __FILE__,
            pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
        goto cleanup;
    }

    hr = TestIFileSourceFilter( &testGraph, pTestDesc, pMedia, NULL );
    if( FAILED(hr) )
    {
        retval = TPR_FAIL;
        goto cleanup;
    }

    // In non DRM case
    if ( mt.majortype.Data1 )
    {
        hr = TestIFileSourceFilter( &testGraph, pTestDesc, pMedia, &mt );
        if( FAILED(hr) )
        {
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    
cleanup:
     //release the memory for the format block in the media type (if any). 
    if (mt.cbFormat != 0)
    {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL)
    {
        // Unecessary because pUnk should not be used, but safest.
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }

    // Reset the testGraph
    testGraph.Reset();
    
    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();
    return retval;
}




