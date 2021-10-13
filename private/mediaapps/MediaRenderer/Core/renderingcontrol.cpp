//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// RenderingControl.cpp : Implementation of CRenderingControl

#include "stdafx.h"



// Strings of state variable names
LPCWSTR
    pszNListPresets = L"PresetNameList",
    pszNMute        = L"Mute",
    pszNVolume      = L"Volume";


/////////////////////////////////////////////////////////////////////////////
// CRenderingControl

CRenderingControl::CRenderingControl(ce::smart_ptr<IConnection> pConnection)
    : m_pSubscriber(NULL),
      m_pConnection(pConnection)
{
    assert(pConnection);

    InitListPresets();
    InitChannels();
}


CRenderingControl::~CRenderingControl()
{
}



//
// ServiceInstanceLifetime
//
void CRenderingControl::OnFinalRelease()
{
    delete this;
}



//
// IEventSource
//

DWORD CRenderingControl::Advise(  /*[in]*/ av::IEventSink *pSubscriber)
{
    assert(!m_pSubscriber);
    
    m_pSubscriber = pSubscriber;

    if(m_pSubscriber)
    {
        // Call OnStateChange for each non-volume-related state variable

        LPCWSTR vpszStateVariables[] =
        {
            pszNListPresets,
            0
        };


        av::wstring strListPresets;
        const DWORD retLP = ListPresets(&strListPresets);
        if(av::SUCCESS_AV != retLP)
            return retLP;

        LPCWSTR vpszValues[] =
        {
            strListPresets
        };

        const DWORD retMSC = MultiOnStateChanged(m_pSubscriber, vpszStateVariables, vpszValues);
        if(av::SUCCESS_AV != retMSC)
            return retMSC;


        // Call OnStateChange for each volume-related state variable

        DWORD retOSC;
        const VolumeMap::const_iterator itChannelsEnd = m_mapChannel.end();
        for(VolumeMap::const_iterator itChannels = m_mapChannel.begin(); itChannelsEnd != itChannels; ++itChannels)
        {
            retOSC = pSubscriber->OnStateChanged(pszNMute, itChannels->first, false);
            if(av::SUCCESS_AV != retOSC)
                return retOSC;

            retOSC = pSubscriber->OnStateChanged(pszNVolume, itChannels->first, itChannels->second.nVolume);
            if(av::SUCCESS_AV != retOSC)
                return retOSC;
        }
    }

    return av::SUCCESS_AV;
}


DWORD CRenderingControl::Unadvise(/*[in]*/ av::IEventSink *pSubscriber)
{
    if(m_pSubscriber)
    {
        //m_pSubscriber->Release(); // TODO: does IEventSink need a ::Release()?
        m_pSubscriber = NULL;
    }

    return av::SUCCESS_AV;
}



//
// IRenderingControlImpl
//

DWORD CRenderingControl::ListPresets(/* [in, out] */ av::wstring* pstrPresetNameList)
{
    if(!pstrPresetNameList)
        return av::ERROR_AV_POINTER;

    bool fFirstEntry = true;

    for(ListPresetMap::const_iterator itEnd = m_mapListPresets.end(), it = m_mapListPresets.begin(); itEnd != it; ++it)
    {
        if(!fFirstEntry)
        {
            if(!pstrPresetNameList->append(av::AVDCPListDelimiter))
                return av::ERROR_AV_OOM;
        }
        else
        {
            fFirstEntry = false;
        }

        if(!pstrPresetNameList->append(it->first))
            return av::ERROR_AV_OOM;
    }

    return av::SUCCESS_AV;
}


DWORD CRenderingControl::SelectPreset(/* [in] */ LPCWSTR pszPresetName)
{
    if(!pszPresetName)
        return av::ERROR_AV_POINTER;

    const ListPresetMap::const_iterator itPreset = m_mapListPresets.find(pszPresetName);
    
    if(itPreset == m_mapListPresets.end())
        return av::ERROR_AV_UPNP_RC_INVALID_PRESET_NAME;

    //
    // TO DO:
    //
    // set state as given in itPreset->second
    //

    return av::SUCCESS_AV;
}


DWORD CRenderingControl::GetMute(
                            /* [in] */ LPCWSTR pszChannel,
                            /* [in, out] */ bool* pMute)
{
    if(!pMute || !pszChannel)
        return av::ERROR_AV_POINTER;

    const VolumeMap::const_iterator it = m_mapChannel.find(pszChannel);
    
    if(it == m_mapChannel.end())
        return av::ERROR_AV_UPNP_ACTION_FAILED; // invalid channel for this device
        
    unsigned short Volume = MAX_VOLUME_RANGE;
    
    m_pConnection->GetVolume(&Volume);
        
    *pMute = (Volume == MIN_VOLUME_RANGE);

    return av::SUCCESS_AV;
}


DWORD CRenderingControl::SetMute(
                            /* [in] */ LPCWSTR pszChannel,
                            /* [in] */ bool Mute)
{
    if(!pszChannel)
        return av::ERROR_AV_POINTER;

    const VolumeMap::iterator it = m_mapChannel.find(pszChannel);
    
    if(it == m_mapChannel.end())
        return av::ERROR_AV_UPNP_ACTION_FAILED; // invalid channel for this device

    if(Mute)
        m_pConnection->SetVolume(MIN_VOLUME_RANGE);
    else
        m_pConnection->SetVolume(it->second.nVolume);
    
    if(m_pSubscriber)
    {        
        m_pSubscriber->OnStateChanged(pszNMute, pszChannel, static_cast<int>(Mute));
    }
    return av::SUCCESS_AV;
}


DWORD CRenderingControl::GetVolume(
                            /* [in] */ LPCWSTR pszChannel,
                            /* [in, out] */ unsigned short* pVolume)
{
    if(!pVolume || !pszChannel)
        return av::ERROR_AV_POINTER;

    const VolumeMap::const_iterator it = m_mapChannel.find(pszChannel);
    
    if(it == m_mapChannel.end())
        return av::ERROR_AV_UPNP_ACTION_FAILED; // invalid channel for this device

    m_pConnection->GetVolume(pVolume);
        
    return av::SUCCESS_AV;
}


DWORD CRenderingControl::SetVolume(
                            /* [in] */ LPCWSTR pszChannel,
                            /* [in] */ unsigned short Volume)
{
    if(!pszChannel)
        return av::ERROR_AV_POINTER;

    const VolumeMap::iterator it = m_mapChannel.find(pszChannel);
    
    if(it == m_mapChannel.end())
        return av::ERROR_AV_UPNP_ACTION_FAILED; // invalid channel for this device

    if( FAILED( m_pConnection->SetVolume( Volume )))
        return av::ERROR_AV_UPNP_ACTION_FAILED;

    it->second.nVolume = Volume;

    wchar_t strVolume[24];
    swprintf(strVolume,L"%d",Volume);
    if(m_pSubscriber)
    {        
        m_pSubscriber->OnStateChanged(pszNVolume, pszChannel, Volume);
    }
    return av::SUCCESS_AV;
}



// InvokeVendorAction
DWORD CRenderingControl::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    if(0 == wcscmp(L"X_VendorExtensionExample", pszActionName))
    {
        ce::variant varInput;
        UINT        uArgErr;
        
        if(pdispparams->cArgs != 3)
            return av::ERROR_AV_UPNP_ACTION_FAILED;
        
        // ignore the first argument (InstanceID)
        
        // get second [in] argument
        if(FAILED(DispGetParam(pdispparams, 1, VT_BSTR, &varInput, &uArgErr)))
            return av::ERROR_AV_UPNP_ACTION_FAILED;
        
        // named arguments are not supported
        assert(pdispparams->rgdispidNamedArgs == NULL);
        assert(0 == pdispparams->cNamedArgs);
        
        // verify type of [out] argument
        if(pdispparams->rgvarg[0].vt != (VT_BSTR | VT_BYREF))
            return av::ERROR_AV_UPNP_ACTION_FAILED;
        
        assert(*pdispparams->rgvarg[0].pbstrVal == NULL);
            
        av::wstring strResult;
        
        strResult = L"Input was \"";
        strResult += varInput.bstrVal;
        strResult += L"\"";
        
        // set result to [out] argument
        *pdispparams->rgvarg[0].pbstrVal = SysAllocString(strResult);
        
        return av::SUCCESS_AV;
    }
    
    return av::ERROR_AV_UPNP_INVALID_ACTION;
}


//
// Private
//

DWORD CRenderingControl::InitListPresets()
{
    RenderingControlPreset dummyRCPreset;

    if((m_mapListPresets.insert(L"FactoryDefaults",      dummyRCPreset) == m_mapListPresets.end()) ||
       (m_mapListPresets.insert(L"InstallationDefaults", dummyRCPreset) == m_mapListPresets.end()))
       return av::ERROR_AV_OOM;

    return av::SUCCESS_AV;
}


DWORD CRenderingControl::InitChannels()
{
    VolumeMap::iterator it;

    if(m_mapChannel.end() == (it = m_mapChannel.insert(L"Master", ChannelAudioData())))
       return av::ERROR_AV_OOM;

    m_pConnection->GetVolume(&it->second.nVolume);
        
    return av::SUCCESS_AV;
}
