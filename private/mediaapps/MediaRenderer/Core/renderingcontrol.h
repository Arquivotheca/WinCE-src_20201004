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
// RenderingControl.h : Declaration of CRenderingControl

#pragma once



/////////////////////////////////////////////////////////////////////////////
// CRenderingControl
class CRenderingControl :     public av::IRenderingControlImpl
{
public:
    CRenderingControl(ce::smart_ptr<IConnection> pConnection);
    virtual ~CRenderingControl();

// ServiceInstanceLifetime
public:
    void OnFinalRelease();

// IEventSource
public:
    DWORD Advise(  /*[in]*/ av::IEventSink* pSubscriber);
    DWORD Unadvise(/*[in]*/ av::IEventSink* pSubscriber);

// IRenderingControlImpl
public:
    virtual DWORD ListPresets(
        /* [in, out] */ av::wstring* pstrPresetNameList);

    virtual DWORD SelectPreset(
        /* [in] */ LPCWSTR pszPresetName);

    virtual DWORD GetMute(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ bool* pMute);

    virtual DWORD SetMute(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ bool Mute);

    virtual DWORD GetVolume(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ unsigned short* pVolume);

    virtual DWORD SetVolume(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ unsigned short Volume);
        
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult);

protected:
    DWORD InitListPresets();
    DWORD InitChannels();
    
protected:    
    av::IEventSink*             m_pSubscriber;
    ce::smart_ptr<IConnection>  m_pConnection;

    struct RenderingControlPreset
    {
    };

    typedef ce::hash_map<av::wstring, RenderingControlPreset> ListPresetMap; // map from ListPreset name -> preset object
    ListPresetMap m_mapListPresets;


    class ChannelAudioData
    {
    public:
        ChannelAudioData() 
            : nVolume(0) 
        {}
        
        unsigned short nVolume;
    };

    typedef ce::hash_map<av::wstring, ChannelAudioData>       VolumeMap;     // map from channel -> data for the given channel
    VolumeMap m_mapChannel;
};

