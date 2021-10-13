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


#include <windows.h>
#include <tux.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"
#include "utility.h"

HRESULT TestInterfaceMethods(IAMMediaContent  *pInterface,PlaybackMedia *pPlayMedia)
{
    HRESULT hr = S_OK;
    BSTR value = NULL;
    BOOL bAllNotimpl = true;
     //whether all interfact not implemented yet. 

    if(pInterface == NULL || pPlayMedia == NULL)
    {
        LOG(TEXT("NULL printer to IAMMediaContent or PlaybackMedia"));
        return E_FAIL;
    }
    
    //begin to test 13 methods of IAMMediaContent;
    //skip verification if no metadata in XML.

    hr=pInterface->get_AuthorName(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_AuthorName not successful - return: (0x%x)"),hr);
        // if return E_NOTIMPL or VFW_E_NOT_FOUND, continure test; if return other code, test failed.
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }
    }
    else
    {
        LOG(TEXT("Method get_AutherName(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;
        //have related element in XML, then compare the data from Interface Method.
        if(pPlayMedia->bAutherName)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szAutherName);
            if(!wcscmp(pPlayMedia->szAutherName, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
         //skip verification if not meta data in xml 
    }

    hr=pInterface->get_Title(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_Title not successful - return: (0x%x)"),hr);
        // if return E_NOTIMPL or VFW_E_NOT_FOUND, continure test; if return other code, test failed.
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }
    }
    else
    {
        LOG(TEXT("Method get_Title(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        //have related element in XML, then compare the data from Interface Method.
        if(pPlayMedia->bTitle)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szTitle);
            if(!wcscmp(pPlayMedia->szTitle, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
          //skip verification if not meta data in xml 
    }
    
    hr=pInterface->get_Rating(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_Rating not successful - return: (0x%x)"),hr);
        // if return E_NOTIMPL or VFW_E_NOT_FOUND, continure test; if return other code, test failed.
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }

    }
    else
    {
        LOG(TEXT("Method get_Rating(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;
        //have related element in XML, then compare the data from Interface Method.
        if(pPlayMedia->bRating)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szRating);
            if(!wcscmp(pPlayMedia->szRating, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
          //skip verification if not meta data in xml 
    }

    hr=pInterface->get_Description(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_Description not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }
    }
    else
    {
        LOG(TEXT("Method get_Description(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bDescription)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szDescription);
            if(!wcscmp(pPlayMedia->szDescription, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
         //skip verification if not meta data in xml 
    }

    hr=pInterface->get_Copyright(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_Copyright not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }
    }
    else
    {
        LOG(TEXT("Method get_Copyright(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bCopyright)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szCopyright);
            if(!wcscmp(pPlayMedia->szCopyright, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
          //skip verification if not meta data in xml 
    }

     hr=pInterface->get_BaseURL(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_BaseURL not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }
    }
    else
    {
        LOG(TEXT("Method get_BaseURL(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bBaseURL)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szBaseURL);
            if(!wcscmp(pPlayMedia->szBaseURL, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
         //skip verification if not meta data in xml 
    }

    hr=pInterface->get_LogoURL(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_LogoURL not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }
    }
    else
    {
        LOG(TEXT("Method get_LogoURL(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bLogoURL)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szLogoURL);
            if(!wcscmp(pPlayMedia->szLogoURL, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
         //skip verification if not meta data in xml 
    }

    hr=pInterface->get_LogoIconURL(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_LogoIconURL not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }

    }
    else
    {
        LOG(TEXT("Method get_LogoIconURL(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bLogoIconURL)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szLogoIconURL);
            if(!wcscmp(pPlayMedia->szLogoIconURL, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
          //skip verification if not meta data in xml 
    }

    hr=pInterface->get_WatermarkURL(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_WatermarkURL not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }

    }
    else
    {
        LOG(TEXT("Method get_WatermarkURL(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bWatermarkURL)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szWatermarkURL);
            if(!wcscmp(pPlayMedia->szWatermarkURL, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
         //skip verification if not meta data in xml 
    }


    hr=pInterface->get_MoreInfoURL(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_MoreInfoURL not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }
    }
    else
    {
        LOG(TEXT("Method get_MoreInfoURL(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bMoreInfoURL)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szMoreInfoURL);
            if(!wcscmp(pPlayMedia->szMoreInfoURL, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
         //skip verification if not meta data in xml 
    }

    hr=pInterface->get_MoreInfoBannerImage(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_MoreInfoBannerImage not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }
    }
    else
    {
        LOG(TEXT("Method get_MoreInfoBannerImage(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bMoreInfoBannerImage)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szMoreInfoBannerImage);
            if(!wcscmp(pPlayMedia->szMoreInfoBannerImage, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
         //skip verification if not meta data in xml 
    }
    
    hr=pInterface->get_MoreInfoBannerURL(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_MoreInfoBannerURL not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }

    }
    else
    {
        LOG(TEXT("Method get_MoreInfoBannerURL(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bMoreInfoBannerURL)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szMoreInfoBannerURL);
            if(!wcscmp(pPlayMedia->szMoreInfoBannerURL, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
         //skip verification if not meta data in xml 
    }

    hr=pInterface->get_MoreInfoText(&value);
    if(FAILED(hr))
    {
        LOG(TEXT("get_MoreInfoText not successful - return: (0x%x)"),hr);
        if( !( VFW_E_NOT_FOUND == hr || E_NOTIMPL == hr )) 
        {
            LOG(TEXT("Test failed as it return other than S_OK,E_NOTIMPL and VFW_E_NOT_FOUND"));
            return E_FAIL;
        }

    }
    else
    {
        LOG(TEXT("Method get_MoreInfoText(): Supported(0x%08x),   value=%s,"),hr,value);
        bAllNotimpl = false;

        if(pPlayMedia->bMoreInfoText)
        {
            LOG(TEXT("Meta data from XML file: %s"),pPlayMedia->szMoreInfoText);
            if(!wcscmp(pPlayMedia->szMoreInfoText, value))
                LOG(TEXT("verification meta data succeed!"));
            else
            {
                LOG(TEXT("verification meta data failed. Test failed!"));
                return E_FAIL;
            }
        }
         //Free the string allocated previously by pInterface->get_method() 
         SysFreeString(value);
         //skip verification if not meta data in xml 
    }
    //if all function not impleted,return TPR_SKIP.Test has passed if can 
    //execute here.
    if (bAllNotimpl) 
    {
    LOG(TEXT("All Methods return E_NOTIMPL, test skipped."));
    hr =  S_FALSE;
    }else
    hr = S_OK;
    
    return hr;
}

//Test IAMMediaContent
TESTPROCAPI IAMMediaContentTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS; 
    HRESULT hr = S_OK;
    IAMMediaContent *pIAMMediaContent = NULL;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

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
    else 
    {
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
    else 
    {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }
    
    // FindInterfacesOnGraph can be used within either normal or secure chamber.
    hr = testGraph.FindInterfaceOnGraph( IID_IAMMediaContent, (void **)&pIAMMediaContent );
    if ( FAILED(hr) || !pIAMMediaContent )
    {
        LOG( TEXT("%S: WARNING %d@%S - failed to retrieve IAMMediaContent from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_SKIP;
        goto cleanup;
    }

    LOG(TEXT("%S: Support IAMMediaContent for %s"), __FUNCTION__,pMedia->GetUrl());
    //do interface IAMMediaContent test.
    
    //set the graph in stopped state and do interface test
    hr = testGraph.SetState(State_Stopped, TestGraph::SYNCHRONOUS, NULL);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to change state to State_Stopped (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else
    {
        LOG(TEXT("%S: change state to State_Stopped, do Interface test now."), __FUNCTION__);
        hr = TestInterfaceMethods(pIAMMediaContent,pMedia);
        switch(hr)
        {
            case S_OK:
            {
                retval = TPR_PASS;
                break;
            }
            case S_FALSE:
            {
                retval = TPR_SKIP;
                break;
            }
            default:
                retval = TPR_FAIL;
                break;            
        }    
    }
cleanup:
    SAFE_RELEASE(pIAMMediaContent);
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
}


