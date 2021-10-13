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

//for IAMPlayListItem and IAMPlayList interface
#include "playlist.h"

HRESULT TestMediaListItemMethods(IAMPlayListItem*pInterface,PlaybackMedia *pPlayMedia, int mSourceIndex)
{
    HRESULT hr = S_OK;
    
    DWORD dwValue = NULL;
    DWORD dwSourceCount = NULL;
    BSTR bstrValue = NULL;
    BOOL bDoVerification = FALSE;
    ASXEntrySource *pEntrySource = NULL;
     
    //check for wheather do verification
    //the Entry datas in test XML should follow the sequence in media content.
    if( (int)(pPlayMedia->EntrySources.size()) > mSourceIndex)
    {
        // if there is verification data in test xml. 
        pEntrySource =pPlayMedia->EntrySources.at(mSourceIndex);

        if(pEntrySource->ASXEntrySourceCount == pEntrySource->ASXEntrySourceURLs.size())
        {
            LOG(TEXT("_Will do data verification for this entry"));
            bDoVerification = TRUE;
        }
        else
        {
            LOG(TEXT("_SourceCount != SourceURL count. Check test XML! Skip verification"));
        }
    }
    else
    {
        LOG(TEXT("_Skip verificaton for this entry."));
    }
    
    hr = pInterface->GetSourceCount(&dwSourceCount);
    if(FAILED(hr))
    {
        LOG(TEXT("IAMPlayListItem::GetSourceCount not successful - return: (0x%x)"),hr);
        return hr;
    }
    else
    {
        LOG(TEXT("IAMPlayListItem::GetSourceCount succeed value=%d"),dwSourceCount);
        if(bDoVerification)
        {
            if(pPlayMedia->EntrySources.at(mSourceIndex)->ASXEntrySourceCount == dwSourceCount)
            {
                LOG(TEXT("Verification GetSourceCount succeed"));
            }
            else
            {
                LOG(TEXT("Verification GetSourceCount failed, test failed"));
                return E_FAIL;
            }
        }
    }

    hr = pInterface->GetFlags(&dwValue);
    if(FAILED(hr))
    {
        LOG(TEXT("IAMPlayListItem::GetFlags not successful - return: (0x%x)"),hr);
        return hr;

    }
    else
    {
        LOG(TEXT("IAMPlayListItem::GetFlags succeed, value=%d"),dwValue);
        if(bDoVerification)
        {
            if(pPlayMedia->EntrySources.at(mSourceIndex)->ASXEntryFlags == dwValue)
            {
                LOG(TEXT("Verification GetFlags succeed."));
            }
            else
            {
                LOG(TEXT("Verification GetFlags failed, test failed"));
                return E_FAIL;
            }
        }
    }

    //retrieve all SourceURL of this entry.
    //if not verification data from test XML, skip verification
    for(DWORD mIndex = 0;mIndex < dwSourceCount;mIndex ++)
    {
        hr=pInterface->GetSourceURL(mIndex,&bstrValue);
        
        if(FAILED(hr))
        {
            LOG(TEXT("IAMPlayListItem::GetSourceURL not successful - return: (0x%x)"),hr);
            return hr;
        }
        else
        {
            LOG(TEXT("IAMPlayListItem::GetSourceURL succeed for %d soucerURL , value=%s,"),mIndex,bstrValue);
            
            if(bDoVerification)
            {
                if(!wcscmp(pEntrySource->ASXEntrySourceURLs.at(mIndex), bstrValue))
                {
                    LOG(TEXT("verification entry's %d SourceURL succeed."),mIndex);
                    SysFreeString(bstrValue);
                }
                else
                {
                    LOG(TEXT("verification SourceURL failed, test failed! Data from test XML:%s"),pEntrySource->ASXEntrySourceURLs.at(mIndex));
                    return E_FAIL;
                }
            }
            else
            {
                SysFreeString(bstrValue);
            }
        }
    }

    //extra test for invalid value.(exceed 1)
    hr=pInterface->GetSourceURL(dwSourceCount,&bstrValue);
    if(FAILED(hr))
    {
        LOG(TEXT("Test for invalid sourceURL failed as expect."));
        hr = S_OK;
       // reset hr here. 
    }
    else
    {
        LOG(TEXT("Test for invalid sourceURL succeed without expectation."));
        return E_FAIL;
    }
    
    return hr;
}

HRESULT TestMediaListMethods(IAMPlayList  *pInterface,PlaybackMedia *pPlayMedia)
{

        HRESULT hr = S_OK;
        
        DWORD dwRepeatCount = NULL;
        DWORD dwRepeatStart = NULL;
        DWORD dwRepeatEnd = NULL;
        DWORD dwValue = NULL;
    
        LOG(TEXT("!!Begin test of IAMPlayList"));
        
        hr = pInterface->GetRepeatInfo(&dwRepeatCount,&dwRepeatStart,&dwRepeatEnd);
        if(FAILED(hr))
        {
            LOG(TEXT("IAMPlayList::GetRepeatInfo not successful - return: (0x%x)"),hr);
            return hr;
        }
        else
        {
            if(pPlayMedia->bRepeatCount && pPlayMedia->bRepeatEnd && pPlayMedia->bRepeatStart)
            {
                if(pPlayMedia->nRepeatCount == dwRepeatCount && pPlayMedia->nRepeatStart 
                    == dwRepeatStart  && pPlayMedia->nRepeatEnd == dwRepeatEnd )
                {
                    LOG(TEXT("Verification for RepeatCount(%d),RepeatStart(%d),RepeatEnd(%d) succeed. "),
                                dwRepeatCount,dwRepeatStart,dwRepeatEnd);
                }
                else
                {
                    LOG(TEXT("Verification Repeat info failed, test failed"));
                    return E_FAIL;
                }
            }
            else
            {
                LOG(TEXT("Verification skipped."));
            }                    
        }
            
        hr = pInterface->GetItemCount(&dwValue);
        if(FAILED(hr))
        {
            LOG(TEXT("IAMPlayList::GetItemCount not successful - return: (0x%x)"),hr);
            return hr;

        }
        else
        {
            LOG(TEXT("IAMPlayList::GetItemCount succeed, value=%d"),dwValue);
            if(pPlayMedia->bItemCount)
            {
                if(pPlayMedia->nItemCount == dwValue)
                {
                    LOG(TEXT("Verification ItemCount succeed"));
                }
                else
                {
                    LOG(TEXT("Verification ItemCount failed, test failed"));
                    return E_FAIL;
                }
            }
            else
            {
                LOG(TEXT("Verification skipped."));
            }
        }

    return hr;
}


//Test both IAMMediaList and IAMPlayListItem
TESTPROCAPI IAMPlayListTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS; 
    HRESULT hr = S_OK;
    IAMPlayListItem *pIAMPlayListItem = NULL;
    IAMPlayList  *pIAMPlayList = NULL;

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

    DWORD dwEntryCount = NULL;

    // FindInterfacesOnGraph can be used within either normal or secure chamber.
    hr = testGraph.FindInterfaceOnGraph( IID_IAMPlayList, (void **)&pIAMPlayList );
    if ( FAILED(hr) || !pIAMPlayList )
    {
        LOG( TEXT("%S: WARNING %d@%S - failed to retrieve IAMPlayList from the graph" ), 
                        __FUNCTION__, __LINE__, __FILE__ );
        retval = TPR_SKIP;
        goto cleanup;
    }

    LOG(TEXT("%S: Support IAMPlayList for %s"), __FUNCTION__,pMedia->GetUrl());

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
        hr = TestMediaListMethods(pIAMPlayList,pMedia);
        if (FAILED(hr))
            goto cleanup;
    }


    hr = pIAMPlayList->GetItemCount(&dwEntryCount);
    if(FAILED(hr))
    {
        LOG(TEXT("IAMPlayListItem::GetSourceCount not successful - return: (0x%x)"),hr);
        return TPR_FAIL;
    }

    //trying to ge playlistitem, then test IAMPlayListItem methods
    for(DWORD mEntryIndex = 0; mEntryIndex <dwEntryCount; mEntryIndex++)
    {
        hr = pIAMPlayList->GetItem(mEntryIndex, &pIAMPlayListItem);
        if(FAILED(hr))
        {
            LOG(TEXT("pIAMPlayList::GetItem not successful - return: (0x%x)"),hr);
            retval = TPR_FAIL;
            goto cleanup;
        }
        else
        {
            LOG(TEXT("IAMPlayList::GetItem  for %d Entry Succeed. Begin to test MediaListItem"),(mEntryIndex +1));
            hr = TestMediaListItemMethods(pIAMPlayListItem,pMedia,mEntryIndex);
            if (FAILED(hr))
                goto cleanup;
        }
    }
    
cleanup:
    SAFE_RELEASE(pIAMPlayListItem);
    SAFE_RELEASE(pIAMPlayList);

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    // Reset the test
    pTestDesc->Reset();

    return retval;
}

