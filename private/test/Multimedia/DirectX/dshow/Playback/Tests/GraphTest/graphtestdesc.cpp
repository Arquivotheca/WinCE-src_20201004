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

#include "GraphTestDesc.h"

// BuildGraphTestDesc methods
BuildGraphTestDesc::BuildGraphTestDesc() : TestDesc()
{
    m_dwNumOfFilters = 0;
    m_Guid = GUID_NULL;
}

BuildGraphTestDesc::~BuildGraphTestDesc()
{
    EmptyStringList(filterList);
}

void 
BuildGraphTestDesc::SetTestGuid( GUID *pGuid )
{
    if ( pGuid )
        m_Guid = *pGuid;
}

TCHAR *
BuildGraphTestDesc::GetFilterName( DWORD dwIndex )
{
    if ( dwIndex >= filterList.size() )
        return NULL;
    return filterList[dwIndex];
}

// PlaybackTestDesc methods
PlaybackTestDesc::PlaybackTestDesc() :
    TestDesc(),
    start(0),
    stop(0),
    durationTolerance(0)
{
    XBvt = false;
    resMonitor = false;//monitor resource during multiple graph test
    state = State_Running;
}

PlaybackTestDesc::~PlaybackTestDesc()
{
}


// StateChangeTestDesc methods
StateChangeTestDesc::StateChangeTestDesc()
{
    XBvt = false;
}

StateChangeTestDesc::~StateChangeTestDesc()
{
}

//PlaybackInterfaceTestDesc
PlaybackInterfaceTestDesc::PlaybackInterfaceTestDesc()
{
    //For IBasicAudio test
    tolerance = 0;
    
    //For IAMNetShowExProps test
    protocol = 0;
    bandWidth = 0;
    codecCount = 0;    

    //For IAMDroppedFrames test
    totalFrameCount = 0;
    droppedFrameRate = 0;
    
    //For IAMNetworkStatus test
    isBroadcast = 0;
   
    //For IAMExtendedSeeking test    
    markerCount = 0;
    exCapabilities = 0;
    //For IAMNetShowConfig test
    pszHost = NULL;
    testPort = 0;
    bufferTime = 0;
    
}

PlaybackInterfaceTestDesc::~PlaybackInterfaceTestDesc()
{
    if(pszHost)
    SysFreeString(pszHost);

}

void PlaybackInterfaceTestDesc::clearMem()
{
    if( mediaMarkerInfo.size() > 0)
    { 
        for(DWORD i = 0; i<mediaMarkerInfo.size(); i++ )
        {
            if(mediaMarkerInfo.at(i)->markerName)
            {
                delete mediaMarkerInfo.at(i)->markerName;
            }
        }
        mediaMarkerInfo.clear();
    }
}

// AudioControlTestDesc methods
AudioControlTestDesc::AudioControlTestDesc()
{
    UseBT = false;
    ExtraRend = false;
    SelectRend = 0;
    Prompt = false;
    Input = false;
}

AudioControlTestDesc::~AudioControlTestDesc()
{
}
