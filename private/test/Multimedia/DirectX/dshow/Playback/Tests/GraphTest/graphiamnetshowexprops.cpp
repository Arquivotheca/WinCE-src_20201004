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
//  IAMNetShowExProps
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
#include <qnetwork.h>

TESTPROCAPI IAMNetShowExPropsTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    
    IAMNetShowExProps *pIAMNetShowExProps = NULL;;
    long protocol = 0;
    long bandWidth = 0;
    long codecCount = 0;    
    BSTR errorCorrection = NULL;
    BSTR sourceLink = NULL;
    BSTR codecDescription = NULL;
    BSTR FAR  codecURL = NULL;
    DATE creationDate = 0;
    VARIANT_BOOL codecInstalled;
    long  CodecNum=0;

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

    // FindInterfacesOnGraph can be used within either normal or secure chamber.
    hr = testGraph.FindInterfaceOnGraph( IID_IAMNetShowExProps, (void **)&pIAMNetShowExProps );
	if ( FAILED(hr) || !pIAMNetShowExProps )
	{
		LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IAMNetShowExProps from the graph" ), 
						__FUNCTION__, __LINE__, __FILE__ );
		retval = TPR_FAIL;
		goto cleanup;
	}
    
    // test get_SourceProtocol()
    hr = pIAMNetShowExProps->get_SourceProtocol(&protocol);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get Source Protocol failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get Protocol: %d\n"), __FUNCTION__, protocol);
        // verify the content
        if(pTestDesc->protocol != 0)
        {
            if(protocol != pTestDesc->protocol)
            {
                LOG( TEXT("%S: ERROR %d@%S - Expected protocol: %d deos not match actual protocol: %d .\n"), 
                        __FUNCTION__, __LINE__, __FILE__, pTestDesc->protocol, protocol);
                retval = TPR_FAIL;
                goto cleanup;
            }
        }
    }
   
    // test get_Bandwidth
    hr = pIAMNetShowExProps->get_Bandwidth(&bandWidth);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_Bandwidth failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get bandWidth: %d\n"), __FUNCTION__, bandWidth);
        
    }

    // test get_CodecCount
    hr = pIAMNetShowExProps->get_CodecCount(&codecCount);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_CodecCount failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get codecCount: %d\n"), __FUNCTION__, codecCount);
        // verify the content
        if(pTestDesc->codecCount != 0)
        {
            if(codecCount != pTestDesc->codecCount)
            {
                LOG( TEXT("%S: ERROR %d@%S - Expected codecCount: %d deos not match actual codecCount: %d .\n"), 
                        __FUNCTION__, __LINE__, __FILE__, pTestDesc->codecCount, codecCount);
                retval = TPR_FAIL;
                goto cleanup;
            }
        }
    }

    // test get_CreationDate
    hr = pIAMNetShowExProps->get_CreationDate(&creationDate);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_CreationDate failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get creationDate: %f\n"), __FUNCTION__, creationDate);
        
    }

    // test get_ErrorCorrection
    hr = pIAMNetShowExProps->get_ErrorCorrection(&errorCorrection);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_ErrorCorrection failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get errorCorrection: %s\n"), __FUNCTION__, errorCorrection);
        
    }

    // test get_SourceLink
    hr = pIAMNetShowExProps->get_SourceLink(&sourceLink);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - get_SourceLink failed, error code is 0x%x ! \n"), 
                __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG( TEXT("%S: successfully get sourceLink: %s\n"), __FUNCTION__, sourceLink);
        // verify the content
        if(wcscmp((LPCWSTR)sourceLink, (LPCWSTR)(pMedia->GetUrl())))
        {
            LOG( TEXT("%S: ERROR %d@%S - Expected sourceLink: %s deos not match actual sourceLink: %s .\n"), 
                    __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), sourceLink);
            retval = TPR_FAIL;
            goto cleanup;
        }
    }
    
    do
    {
        LOG( TEXT("Codec Number: %d\n"), CodecNum);
        // test GetCodecDescription
        hr = pIAMNetShowExProps->GetCodecDescription(CodecNum, &codecDescription);
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetCodecDescription failed, error code is 0x%x ! \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            LOG( TEXT("%S: successfully get codecDescription: %s\n"), __FUNCTION__, codecDescription);
            
        }

        // test GetCodecInstalled
        hr = pIAMNetShowExProps->GetCodecInstalled(CodecNum, &codecInstalled);
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetCodecInstalled failed, error code is 0x%x ! \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            LOG( TEXT("%S: successfully get codecInstalled: %d\n"), __FUNCTION__, codecInstalled);
            
        }

        // test GetCodecURL
        hr = pIAMNetShowExProps->GetCodecURL(CodecNum, &codecURL);
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - GetCodecURL failed, error code is 0x%x ! \n"), 
                    __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            LOG( TEXT("%S: successfully get codecURL: %s\n"), __FUNCTION__, codecURL);
            
        }
        CodecNum++;
    }while(CodecNum < codecCount);

cleanup:
    
    if(pIAMNetShowExProps)
    {
        pIAMNetShowExProps->Release();
        pIAMNetShowExProps = NULL;
    }

    if(errorCorrection)
    {
        SysFreeString(errorCorrection);
    }

    if(sourceLink)
    {
        SysFreeString(sourceLink);
    }

    if(codecDescription)
    {
        SysFreeString(codecDescription);
    }

    if(codecURL)
    {
        SysFreeString(codecURL);
    }
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
    
}
