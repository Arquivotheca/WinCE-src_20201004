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

#include "logging.h"
#include "globals.h"
#include "StringConversions.h"
#include "MMTestParser.h"
#include "TestDescParser.h"
#include "GraphTestDescParser.h"

const char* const STR_PositionList = "PositionList";
const char* const STR_PreloadFilter = "FilterList";

//For xBVT Tests
const char* const STR_isXBVT = "isXBVT";

//Do you want to monitor resource during multiple graph test?
const char* const STR_ResMonitor = "ResMonitor";
//For Interface Test
//For IBasicAudio test
const char* const STR_Tolerance = "Tolerance";
//For IAMNetShowExProps test 
const char* const STR_Protocol = "Protocol";
const char* const STR_BandWidth = "BandWidth";
const char* const STR_CodecCount = "CodecCount";

//For IAMDroppedFrames test
const char* const STR_FrameCount = "FrameCount";
const char* const STR_DroppedFrameRate = "DroppedFrameRate";

//For IAMNetworkStatus test
const char* const STR_IsBroadcast = "IsBroadcast";

//For IAMExtendedSeeking test
const char* const STR_MarkerCount = "MarkerCount";
const char* const STR_SeekCapabilities = "SeekCapabilities";
const char* const STR_MarkerInfo = "MarkerInfo";
const char* const STR_MarkerNum = "MarkerNum";
const char* const STR_MarkerName = "MarkerName";
const char* const STR_MarkerTime = "MarkerTime";

//For IAMNetShowConfig test
const char* const STR_NetShowConfig = "NetShowConfig";

HRESULT 
ParsePlaybackTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    if ( !hElement || !ppTestDesc ) return E_FAIL;

    PlaybackTestDesc* pPlaybackTestDesc = new PlaybackTestDesc();
    if ( !pPlaybackTestDesc )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to allocate PlaybackTestDesc object when parsing."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        hr = E_OUTOFMEMORY;
        return hr;
    }

    hXmlChild = hElement->GetFirstChildElement();
    if ( hXmlChild == 0 )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to find first test desc element."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return E_FAIL;
    }

    PCSTR pElemName = 0x0;
    while ( SUCCEEDED(hr) && hXmlChild )
    {
        // First parse the common test configuration data: test id, media, verification list, ...
        hr = ParseCommonTestInfo( hXmlChild, pPlaybackTestDesc );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the common test data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            break;
        }

        pElemName = hXmlChild->GetName();
        if ( !pElemName ) 
        {
            hr = E_FAIL;
            break;
        }
        if ( !strcmp(STR_PositionList, pElemName) )
        {
            hr = ParseStartStopPosition(hXmlChild, pPlaybackTestDesc->start, pPlaybackTestDesc->stop );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the position list."), 
                            __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }
        if ( !strcmp( STR_State, pElemName ) )
        {
            hr = ParseState( hXmlChild, &pPlaybackTestDesc->state );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the state."), 
                            __FUNCTION__, __LINE__, __FILE__);
                break;
            }
        }
        if ( !strcmp( STR_isXBVT, pElemName ) )
        {
            pPlaybackTestDesc->XBvt = true;
        }
		
		if ( !strcmp( STR_ResMonitor, pElemName ) )
        {
            pPlaybackTestDesc->resMonitor = true;
        }

        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();
    }

    if ( SUCCEEDED(hr) )
        *ppTestDesc = pPlaybackTestDesc;
    else
        delete pPlaybackTestDesc;

    return hr;
}

HRESULT 
ParseStateChangeTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    if ( !hElement || !ppTestDesc ) return E_FAIL;

    StateChangeTestDesc* pStateChangeTestDesc = new StateChangeTestDesc();
    if ( !pStateChangeTestDesc )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to allocate StateChangeTestDesc object when parsing."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        hr = E_OUTOFMEMORY;
        return hr;
    }

    hXmlChild = hElement->GetFirstChildElement();
    if ( hXmlChild == 0 )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to find the first test desc element."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return E_FAIL;
    }

    PCSTR pElemName = 0x0;
    while ( SUCCEEDED(hr) && hXmlChild )
    {
        // First parse the common test configuration data: test id, media, verification list, ...
        hr = ParseCommonTestInfo( hXmlChild, pStateChangeTestDesc );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the common test data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            break;
        }

        pElemName = hXmlChild->GetName();
        if ( !pElemName ) 
        {
            hr = E_FAIL;
            break;
        }
        if ( !strcmp(STR_StateChangeSequence, pElemName) )
        {
            hr = ParseStateChangeSequenceString( hXmlChild->GetValue(), &pStateChangeTestDesc->m_StateChange);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the state change sequence."), 
                            __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }
        if ( !strcmp( STR_isXBVT, pElemName ) )
        {
            pStateChangeTestDesc->XBvt = true;
        }
		//Monitor resource on multiple graph testing
		if ( !strcmp( STR_ResMonitor, pElemName ) )
        {
            pStateChangeTestDesc->resMonitor = true;
        }

        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();
    }

    if ( SUCCEEDED(hr) )
        *ppTestDesc = pStateChangeTestDesc;
    else
        delete pStateChangeTestDesc;

    return hr;
}

HRESULT 
ParseBuildGraphTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    if ( !hElement || !ppTestDesc ) return E_FAIL;

    BuildGraphTestDesc* pBuildGraphTestDesc = new BuildGraphTestDesc();
    if ( !pBuildGraphTestDesc )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to allocate TestDesc object when parsing."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        hr = E_OUTOFMEMORY;
        return hr;
    }

    *ppTestDesc = pBuildGraphTestDesc;

    hXmlChild = hElement->GetFirstChildElement();
    // It's possible that there is no child elements for buid graph tests.
    if ( !hXmlChild )
        return S_OK;

    PCSTR pElemName = 0x0;
    while ( SUCCEEDED(hr) && hXmlChild )
    {
        // First parse the common test configuration data: test id, media, verification list, ...
        hr = ParseCommonTestInfo( hXmlChild, pBuildGraphTestDesc );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the common test data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            break;
        }

        pElemName = hXmlChild->GetName();
        if ( !pElemName )
        {
            hr = E_FAIL;
            break;
        }
        if ( !strcmp( "GUID", pElemName) )
        {
            GUID tempGUID;
            hr = ParseGUID( hXmlChild, &tempGUID );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the GUID."), 
                            __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pBuildGraphTestDesc->SetTestGuid( &tempGUID );
        }
        if ( !strcmp( "FilterInterfaces", pElemName) )
        {
            DWORD dwFilterCount = 0;
            hr = ParseDWORD( hXmlChild, &dwFilterCount );
            if ( FAILED(hr) )
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the number of filter interfaces."), 
                            __FUNCTION__, __LINE__, __FILE__ );
            else
                pBuildGraphTestDesc->SetNumOfFilters( dwFilterCount );
        }
        if ( !strcmp( STR_PreloadFilter, pElemName ) )
        {
            hr = ParseStringList( hXmlChild, &pBuildGraphTestDesc->filterList, ':');
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the filter list."), 
                            __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
        }

        if ( !strcmp( "Filters", pElemName ) )
        {
            hr = ParseStringList( hXmlChild, &pBuildGraphTestDesc->filterList, ':');
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the filter list."), 
                            __FUNCTION__, __LINE__, __FILE__ );
                break;
            }
            pBuildGraphTestDesc->SetNumOfFilters( pBuildGraphTestDesc->filterList.size() );
        }

        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();
    }

    if ( FAILED(hr) )
    {
        delete pBuildGraphTestDesc;
        *ppTestDesc = NULL;
    }

    return hr;
}

HRESULT 
ParseInterfaceTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    if ( !hElement || !ppTestDesc ) return E_FAIL;

    PlaybackInterfaceTestDesc* pPlaybackInterfaceTestDesc = new PlaybackInterfaceTestDesc();
    if ( !pPlaybackInterfaceTestDesc )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to allocate PlaybackTestDesc object when parsing."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        hr = E_OUTOFMEMORY;
        return hr;
    }

    hXmlChild = hElement->GetFirstChildElement();
    if ( hXmlChild == 0 )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to find first test desc element."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return E_FAIL;
    }

    PCSTR pElemName = 0x0;
    while ( SUCCEEDED(hr) && hXmlChild )
    {
        // First parse the common test configuration data: test id, media, verification list, ...
        hr = ParseCommonTestInfo( hXmlChild, pPlaybackInterfaceTestDesc );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the common test data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            break;
        }

        pElemName = hXmlChild->GetName();
        if ( !pElemName ) 
        {
            hr = E_FAIL;
            break;
        }
        
        //For IBasicAudio test
        if ( !strcmp( STR_Tolerance, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(pPlaybackInterfaceTestDesc->tolerance) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse tolerance for IBasicAudio test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }

        //For IAMNetShowExProps test 
        if ( !strcmp( STR_Protocol, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(pPlaybackInterfaceTestDesc->protocol) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse protocol for IAMNetShowExProps test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }

        if ( !strcmp( STR_BandWidth, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(pPlaybackInterfaceTestDesc->bandWidth) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse bandwidth for IAMNetShowExProps test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }

        if ( !strcmp( STR_CodecCount, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(pPlaybackInterfaceTestDesc->codecCount) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse codecCount for IAMNetShowExProps test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }

        //For IAMDroppedFrames test
        if ( !strcmp( STR_FrameCount, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(pPlaybackInterfaceTestDesc->totalFrameCount) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse totalFrameCount for IAMNetShowExProps test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }
        
        if ( !strcmp( STR_DroppedFrameRate, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(pPlaybackInterfaceTestDesc->droppedFrameRate) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse droppedFrameRate for IAMNetShowExProps test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }

        //For IAMNetworkStatus test
        if ( !strcmp( STR_IsBroadcast, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(pPlaybackInterfaceTestDesc->isBroadcast) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse isBroadcast for IAMNetShowExProps test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }

        //For IAMExtendedSeeking test        
        if ( !strcmp( STR_SeekCapabilities, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(pPlaybackInterfaceTestDesc->exCapabilities) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse exCapabilities for IAMExtendedSeeking test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }

        if ( !strcmp( STR_MarkerCount, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(pPlaybackInterfaceTestDesc->markerCount) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse markerCount for IAMExtendedSeeking test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }

        if ( !strcmp( STR_MarkerInfo, pElemName ) )
        {
            hr = ParseMarkerInfo( hXmlChild, pPlaybackInterfaceTestDesc);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse markerCount for IAMExtendedSeeking test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
        }

        if ( !strcmp(STR_NetShowConfig, pElemName) )
    {
            char szString[128];
            TCHAR tszString[64];
            int i;
            int pos = 0;

            // Parse the Proxy Host.
            ParseStringListString( hXmlChild->GetValue(), 
                          STR_STATE_CHANGE_SEQ_DELIMITER, 
                           szString, 
                           countof(szString), 
                           pos );
            if ( pos == -1 )
            return E_FAIL;

            CharToTChar( szString, tszString, countof(tszString) );
            pPlaybackInterfaceTestDesc->pszHost = SysAllocString(tszString);

            //Parse the Proxy prot.
            ParseStringListString( hXmlChild->GetValue(), 
                    STR_STATE_CHANGE_SEQ_DELIMITER, 
                    szString, 
                    countof(szString), 
                     pos );
            if ( pos == -1 )
            return E_FAIL;     

            CharToTChar( szString, tszString, countof(tszString) );
            i = (INT32)_ttoi( tszString );
            pPlaybackInterfaceTestDesc->testPort = long(i);


            // Parse the Buffertime.
            ParseStringListString( hXmlChild->GetValue(), 
                    STR_STATE_CHANGE_SEQ_DELIMITER, 
                    szString, 
                    countof(szString), 
                    pos );
            if ( pos != -1 )
            return E_FAIL;     

            CharToTChar( szString, tszString, countof(tszString) );
            i = (INT32)_ttoi( tszString );
            pPlaybackInterfaceTestDesc->bufferTime= long(i);
        }

        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();
    }

    if ( SUCCEEDED(hr) )
        *ppTestDesc = pPlaybackInterfaceTestDesc;
    else
        delete pPlaybackInterfaceTestDesc;

    return hr;
}

HRESULT 
ParseDOUBLE( CXMLElement *pEleFloat, double* pDouble )
{
    HRESULT hr = S_OK;

    if ( !pEleFloat || !pDouble )
        return E_POINTER;

    const char* szFloat = NULL;
    szFloat = pEleFloat->GetValue();
    if ( !szFloat )
    {
        LOG( TEXT("ParseDOUBLE: did not find parseable double.") );
        hr = E_FAIL;
    }
    else 
    {
        TCHAR tszFloat[64];
        CharToTChar( szFloat, tszFloat, countof(tszFloat) );
        *pDouble = atof( szFloat );
        // if the input can not be coverted to a value, it returns 0.
        // we need distinguish it from value 0.
        if ( 0 != strcmp( "0", szFloat ) && 0 == *pDouble )
            return E_FAIL;
    }

    return hr;
}

HRESULT
ParseMarkerInfo( CXMLElement *hElement, PlaybackInterfaceTestDesc* pPlaybackInterfaceTestDesc )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    if ( !hElement || !pPlaybackInterfaceTestDesc ) return E_FAIL;

    MarkerInfo *tmpMarker = new MarkerInfo;
    tmpMarker->markerName = NULL;
    tmpMarker->markerNum = 0;
    tmpMarker->markerTime = 0.0;

    //if parse failed, delete this new object before leave this function.
    BOOL bHaveParam = FALSE;

    hXmlChild = hElement->GetFirstChildElement();
    if ( hXmlChild == 0 )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to find first test desc element."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return E_FAIL;
    }

    PCSTR pElemName = 0x0;

    while ( SUCCEEDED(hr) && hXmlChild )
    {
        pElemName = hXmlChild->GetName();
        if ( !pElemName ) 
        {
            hr = E_FAIL;
            break;
        }
         if ( !strcmp( STR_MarkerNum, pElemName ) )
        {
            hr = ParseINT32( hXmlChild, (INT32*)&(tmpMarker->markerNum) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse markerCount for IAMExtendedSeeking test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
            bHaveParam = TRUE;
        }
        if ( !strcmp( STR_MarkerName, pElemName ) )
        {
            TCHAR szMarkerName[MAX_PATH];;
            hr = ParseString( hXmlChild, szMarkerName, countof(szMarkerName) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse markerName for IAMExtendedSeeking test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
            {
                tmpMarker->markerName = new WCHAR[countof(szMarkerName)];
                TCharToWChar(szMarkerName, tmpMarker->markerName, countof(szMarkerName));
            }
            bHaveParam = TRUE;
        }

        if ( !strcmp( STR_MarkerTime, pElemName ) )
        {
            hr = ParseDOUBLE( hXmlChild, (double*)&(tmpMarker->markerTime) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse markerTime for IAMExtendedSeeking test."), 
                    __FUNCTION__, __LINE__, __FILE__ );
            }
            bHaveParam = TRUE;
        }
            
        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();
        }

    if(bHaveParam)
    {
        pPlaybackInterfaceTestDesc->mediaMarkerInfo.push_back(tmpMarker);
    }    
    else
        {
        delete tmpMarker;
           LOG( TEXT("%S: ERROR %d@%S - Empty or Error ASXMDEntry for GraphMediaContetnEx tests."), 
                                __FUNCTION__, __LINE__, __FILE__ );
        }
        
    return hr;
}

HRESULT 
ParseAudioControlTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    if ( !hElement || !ppTestDesc ) return E_FAIL;

    AudioControlTestDesc* pAudioControlTestDesc = new AudioControlTestDesc();
    if ( !pAudioControlTestDesc )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to allocate TestDesc object when parsing."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        hr = E_OUTOFMEMORY;
        return hr;
    }

    *ppTestDesc = pAudioControlTestDesc;

    hXmlChild = hElement->GetFirstChildElement();
    // It's possible that there is no child elements for buid graph tests.
    if ( !hXmlChild )
        return S_OK;

    PCSTR pElemName = 0x0;
    while ( SUCCEEDED(hr) && hXmlChild )
    {
        // First parse the common test configuration data: test id, media, verification list, ...
        hr = ParseCommonTestInfo( hXmlChild, pAudioControlTestDesc );
        if ( FAILED(hr) )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the common test data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            break;
        }

        pElemName = hXmlChild->GetName();
        if ( !pElemName )
        {
            hr = E_FAIL;
            break;
        }

        if ( !strcmp( "UseBT", pElemName) )
        {
            pAudioControlTestDesc->SetUseBT();
        }
        if ( !strcmp( "ExtraRend", pElemName) )
        {
            pAudioControlTestDesc->SetExtraRend();
        }
        if ( !strcmp( "SelectRend", pElemName) )
        {
            DWORD dwSelectRend = 0;
            hr = ParseDWORD( hXmlChild, &dwSelectRend );
            if ( FAILED(hr) )
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the renderer that must be selected."), 
                            __FUNCTION__, __LINE__, __FILE__ );
            else
                pAudioControlTestDesc->SetSelectRend( dwSelectRend );
        }
        if ( !strcmp( "Prompt", pElemName) )
        {
            pAudioControlTestDesc->SetPrompt();
        }
        if ( !strcmp( "Input", pElemName) )
        {
            pAudioControlTestDesc->SetInput();
        }

        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();
    }

    if ( FAILED(hr) )
    {
        delete pAudioControlTestDesc;
        *ppTestDesc = NULL;
    }

    return hr;
}
