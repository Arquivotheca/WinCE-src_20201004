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

bool CanDoVerify(long lIndex,PlaybackMedia *pMedia)
{
    long size = pMedia->EntryList.size();
    bool bhr = TRUE;

    if (pMedia == NULL)
    {
        LOG(TEXT("NULL printer to PlaybackMedia on %s, Line:%d"),__FUNCTION__, __LINE__);
        return FALSE;
    }

    if(lIndex >= size)
    {
        LOG(TEXT("Entry Index for this test exceed ASXMDEntry have in test XML. Check if Media content met with test XML."));
        LOG(TEXT("And check if missed  some ASXMDEntry in test XML."));
        return FALSE ;
    }
    
    ASXEntryData *tmpdata = pMedia->EntryList.at(lIndex);
    if(tmpdata->ASXEntryIndex != (DWORD)lIndex)
    {
        LOG(TEXT("ASXMDEntry's EntryIndex sequence did meet media content. "));
        return FALSE;
    }
    else{
        LOG(TEXT("Will do verificaton for  entry %d."),lIndex);
    }
    
return bhr;
}


HRESULT TestInterfaceMethods(IAMMediaContentEx  *pInterface,PlaybackMedia *pPlayMedia)
{
    ASXEntryData *tmpdata = NULL;

    HRESULT hr = S_OK;
    BSTR ParaValue = NULL;
    BSTR ParaName = NULL;


    long LEntryCount = 0;
    //in asx, entry index start start from 0
    long LEntryIndex = 0;
    //in asx, first param has index of 1
    long LParamIndex = 1;

    // if not verification data in test xml, skip verificatioln.    
    BOOL bDoVerificaton = FALSE;

    if(pInterface == NULL || pPlayMedia == NULL)
    {
        LOG(TEXT("NULL printer to IAMMediaContent or PlaybackMedia"));
        return E_FAIL;
    }

    hr=pInterface->get_PlaylistCount(&LEntryCount);
    if(FAILED(hr))
    {
        LOG(TEXT("get_PlaylistCount not successful. Test failed - return: (0x%x)"),hr);
        return hr;
    }
    else
    {
        LOG(TEXT("get_PlaylistCount succeed, value= %d"),LEntryCount);
        if(pPlayMedia->bItemCount)
        {
            if((DWORD)LEntryCount == pPlayMedia->nItemCount)
                LOG(TEXT("Verification Playlistcount data succeed!"));
            else
            {
                LOG(TEXT("Verification Playlistcount data failed. Test failed!"));
                return E_FAIL;
            }
        }
        else
            LOG(TEXT("Skip verification for PlaylistCount."));
    }


    //retrieve all ASX param data from root to last Entry.
    //skip verification if not data in test XML files.
    while(LEntryIndex <= LEntryCount)
    {
        // for each entry, test from first param.
        LParamIndex = 1;
        //if test XML has verification data for this entry
        bDoVerificaton = CanDoVerify(LEntryIndex,pPlayMedia);
        if(bDoVerificaton)
            tmpdata = pPlayMedia->EntryList.at(LEntryIndex);
        else
            LOG(TEXT("skip verification for this entry as no valid data find in test XML"));

        while(S_OK == pInterface->get_MediaParameterName(LEntryIndex,LParamIndex,&ParaName) )
        {
            //for each param under this entry
            LOG(TEXT("Get ASX's  %d Entry's %d ParamName succeed, value = %s. "),LEntryIndex,LParamIndex,ParaName);

            hr=pInterface->get_MediaParameter(LEntryIndex,ParaName,&ParaValue);
            if(FAILED(hr))
            {
                LOG(TEXT("get_MediaParameter not successful for ASX's %d Entry with %s. - return: (0x%x)"),LEntryIndex,ParaName,hr);
                goto cleanup;
            }
            else
            {
                LOG(TEXT("Get ASX's  %d Entry's %d MediaParameter succeed, value = %s. "),LEntryIndex,LParamIndex,ParaValue);
                if(bDoVerificaton)
                {
                    if(!wcscmp(tmpdata->ASXEntryParamNames.at(LParamIndex -1), ParaName) &&
                        !wcscmp(tmpdata->ASXEntryParamNameValues.at(LParamIndex -1), ParaValue))
                    {
                        LOG(TEXT("verification ParamName and it's value succeed for %d's param of %d's entry"),LParamIndex,LEntryIndex);
                    }
                    else{
                        LOG(TEXT("Verification ParamName and it's value  failed. Test Failed. Name from test XML: %s ,value from test XML %s"),
                                    tmpdata->ASXEntryParamNames.at(LParamIndex-1),tmpdata->ASXEntryParamNameValues.at(LParamIndex-1));
                        hr = E_FAIL;
                        goto cleanup;
                    }
                }
                SysFreeString(ParaName);
                SysFreeString(ParaValue);
            }
            LParamIndex++;
        }

        if(bDoVerificaton)
        {
            if((DWORD)(LParamIndex-1) == tmpdata->ASXEntryParamCount)
            {
                LOG(TEXT("__All params of  entry %d has been retrieved and tested"),LEntryIndex);
            }
            else{
                LOG(TEXT("Param count is different from execution and test XML. Test Failed"));
                return E_FAIL;
            }
        }
        else
        {
            LOG(TEXT("get_MediaParameterName return other than S_OK. may be meet the end of this entry. try next entry."));
        }
        LEntryIndex++;
    }

cleanup:

    if(ParaName != NULL)
        SysFreeString(ParaName);
    if(ParaValue != NULL)
        SysFreeString(ParaName);

    return hr;
}


//Test IAMMediaContentEX
TESTPROCAPI IAMMediaContentExTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS; 
    HRESULT hr = S_OK;
    IAMMediaContentEx *pIAMMediaContentEx = NULL;

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
    hr = testGraph.FindInterfaceOnGraph( IID_IAMMediaContentEx, (void **)&pIAMMediaContentEx );
    if ( FAILED(hr) || !pIAMMediaContentEx )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to retrieve IAMMediaContentEx from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_FAIL;
        goto cleanup;
    }

    LOG(TEXT("%S: Support IAMMediaContentEx for %s"), __FUNCTION__,pMedia->GetUrl());
    //do interface IAMMediaContentEx test.

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
        hr = TestInterfaceMethods(pIAMMediaContentEx,pMedia);
        if(FAILED(hr))
            retval = TPR_FAIL;
    }
cleanup:
    SAFE_RELEASE(pIAMMediaContentEx);

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
}

