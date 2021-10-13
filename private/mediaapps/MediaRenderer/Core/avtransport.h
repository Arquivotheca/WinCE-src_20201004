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
// AVTransport.h : Declaration of CAVTransport

#pragma once


static const LPCWSTR c_vpszChangedStateVars[]  = 
{
    av::AVTransportState::CurrentTrackURI,
    av::AVTransportState::CurrentURI,
    av::AVTransportState::CurrentURIMetaData,
    av::AVTransportState::CurrentTrackMetaData,
    av::AVTransportState::CurrentTrackDuration,
    av::AVTransportState::MediaDuration, 
    0
};


static const  LPCWSTR c_vnNumericStateVariables[] = 
{
    av::AVTransportState::CurrentTrack,
    av::AVTransportState::NrTracks,
    0
};


static const LPCWSTR c_vpszStateVariables[] =
{
    // TransportInfo
    av::AVTransportState::TransportState,
    av::AVTransportState::TransportStatus,
    av::AVTransportState::PlaySpeed,
    av::AVTransportState::CurrentTransportActions,

    // PositionInfo
    av::AVTransportState::CurrentTrackDuration,
    av::AVTransportState::CurrentTrackMetaData,
    av::AVTransportState::CurrentTrackURI,

    // DeviceCapabilities
    av::AVTransportState::PossiblePlayMedia,
    av::AVTransportState::PossibleRecMedia,
    av::AVTransportState::PossibleRecQualityModes,

    // TransportSettings
    av::AVTransportState::PlayMode,
    av::AVTransportState::RecQualityMode,

    // MediaInfo
    av::AVTransportState::MediaDuration,
    av::AVTransportState::CurrentURI,
    av::AVTransportState::CurrentURIMetaData,
    av::AVTransportState::NextURI,
    av::AVTransportState::NextURIMetaData,
    av::AVTransportState::PlaybackMedia,
    av::AVTransportState::RecordMedia,
    av::AVTransportState::WriteStatus,
    0
};


static const LPCWSTR c_vNnumericStateVariables[] =
{
    av::AVTransportState::CurrentTrack,
    av::AVTransportState::NrTracks,
    0
};


// This helps identify media types by DLNA.ORG_PN
typedef struct 
{
    LPCWSTR     DlnaName;
    LPCWSTR     FileExtension;
    EMediaClass MediaClass;
} MediaProfileElement;


static MediaProfileElement MediaProfileTable[] =
{
    // Audio
    {L"MP3",     L"mp3", MediaClass_Audio},
    {L"LPCM",    L"pcm", MediaClass_Audio},
    {L"WAVPCM",  L"wav", MediaClass_Audio},
    {L"WMA",     L"wma", MediaClass_Audio},
    {L"AAC",     L"m4a", MediaClass_Audio},

    // Video
    {L"MPEG_PS", L"m2p", MediaClass_Video},
    {L"MPEG_TS", L"m2t", MediaClass_Video},
    {L"WMV",     L"wmv", MediaClass_Video},
    {L"MPEG4_P2",L"mp4", MediaClass_Video},
    {L"AVC_",    L"mp4", MediaClass_Video},

    // Image
    {L"JPEG",    L"jpg", MediaClass_Image},

    // End of table
    {NULL,        NULL,  MediaClass_Unknown}
};


// This helps identify media types by file extension
typedef struct 
{
    LPCWSTR     FileExtension;
    EMediaClass MediaClass;
} FileExtensionElement;

static FileExtensionElement FileExtensionTable[] =
{
    // Audio
    {L"pcm",  MediaClass_Audio},
    {L"wav",  MediaClass_Audio},
    {L"mp3",  MediaClass_Audio},
    {L"m4a",  MediaClass_Audio},
    {L"wma",  MediaClass_Audio},

    // Video
    {L"mpg",  MediaClass_Video},
    {L"mpeg", MediaClass_Video},
    {L"m2p",  MediaClass_Video},
    {L"ts",   MediaClass_Video},
    {L"tts",  MediaClass_Video},
    {L"m2t",  MediaClass_Video},
    {L"mp4",  MediaClass_Video},
    {L"m4v",  MediaClass_Video},
    {L"wmv",  MediaClass_Video},

    // Image
    {L"jpg", MediaClass_Image},

    // End of table
    {NULL,  MediaClass_Unknown}
};



class CAVTransport : public av::IAVTransportImpl, public ITransportCallback
{
public:
    CAVTransport(ce::smart_ptr<IConnection> pConnection);
    virtual ~CAVTransport();

    virtual void OnFinalRelease();
    bool Init();
    void Uninit();

    virtual DWORD Advise(  av::IEventSink* pSubscriber);
    virtual DWORD Unadvise( av::IEventSink* pSubscriber);

    virtual DWORD SetAVTransportURI( LPCWSTR pszCurrentURI, LPCWSTR pszCurrentURIMetaData );
    virtual DWORD Stop();
    virtual DWORD Play( LPCWSTR pszSpeed );
    virtual DWORD Pause();
    virtual DWORD Seek( LPCWSTR pszUnit, LPCWSTR pszTarget );
    virtual DWORD Next();
    virtual DWORD Previous();
    virtual DWORD GetMediaInfo( av::MediaInfo* pMediaInfo );
    virtual DWORD GetTransportInfo( av::TransportInfo* pTransportInfo );
    virtual DWORD GetPositionInfo( av::PositionInfo* pPositionInfo );
    virtual DWORD GetDeviceCapabilities( av::DeviceCapabilities* pDeviceCapabilities );
    virtual DWORD GetTransportSettings( av::TransportSettings* pTransportSettings );
    virtual DWORD GetCurrentTransportActions( av::TransportActions* pActions );
    virtual DWORD SetPlayMode( LPCWSTR pszPlayMode );
    virtual DWORD SetCurrentTrack( long nTrack );
    virtual DWORD InvokeVendorAction( LPCWSTR pszActionName, DISPPARAMS* pdispparams, VARIANT* pvarResult );
    
protected:
    void    SetTransportState(LPCWSTR pszState, LPCWSTR pszStatus);
    void    ClearAVTransport(void);

    DWORD   SetMetadata(LPCWSTR pszCurrentURI, LPCWSTR pszCurrentURIMetaData);
    void    ClearMetadata(void);

    HRESULT ProcessURISuffix(void);
    HRESULT ProcessMetaData(void);
    HRESULT ProcessMetaDataNotify(void);
    HRESULT ProcessMetaDataURI(void);

    // basic string processing routines
    HRESULT ParseMetaItemXML(LPCWSTR pSource, LPCWSTR pItem, 
                             ce::wstring *pAttr, ce::wstring *pXML, LPCWSTR *ppNext);
    HRESULT ParseMetaItemXML(LPCWSTR pItem, ce::wstring *pResult);
    HRESULT ParseMetaItemAttr(LPCWSTR pSource, LPCWSTR pItem, ce::wstring *pResult);
    HRESULT ParseMetaItemAttr(LPCWSTR pItem, ce::wstring *pResult);
    HRESULT ParseMetaItemSubAttr(LPCWSTR pSource, LPCWSTR pItem, ce::wstring *pResult);
    HRESULT ParseMetaItemSubAttr(LPCWSTR pItem, ce::wstring *pResult);
    // Simple data fetch routines
    HRESULT GetMetaDataTitle(void);
    HRESULT GetMetaDataDate(void);
    HRESULT GetMetaDataFlags(void);
    HRESULT GetMetaDataPN(void);
    HRESULT GetMetaDataOp(void);
    HRESULT GetMetaDataRate(void);
    HRESULT GetMetaDataChannels(void);
    HRESULT GetMetaDataResolution(void);
    // Complex data fetch and analyse routines
    HRESULT GetMetaDataProtocolInfo( void );
    HRESULT GetMetaDataTrackSize(void);
    HRESULT GetMetaDataDuration(void);
    HRESULT ProcessMetaDataResource(void);
    HRESULT ProcessMetaDataFileExtension(void);
    HRESULT ProcessMetaDataSeekability();
    HRESULT ProcessMetaDataTrackDuration(void);
    HRESULT ProcessMetaDataFlags(void);

    HRESULT EventNotify(ETransportCallbackEvent event);
    inline  bool IsInState(LPCWSTR state);
    inline  bool IsNotInState(LPCWSTR state);
    int     ReadFlagsBit_V1_5(DWORD bitnum);

protected:
    ce::smart_ptr<IConnection>  m_pConnection;
    CComPtr<IMediaMetadata>     m_pIMM;
    av::IEventSink*             m_pSubscriber;
    bool                        m_bNewURI;
    bool                        m_bExit;
    EMediaClass                 m_MediaClass;
    bool                        m_bHasURIExtension;
    BOOL                        m_fCanSeek;
    bool                        m_bIsContainer;
    bool                        m_bLimitedRange;
    long long                   m_llDuration;

    av::TransportInfo           m_TransportInfo;
    av::PositionInfo            m_PositionInfo;
    av::DeviceCapabilities      m_DeviceCapabilities;
    av::TransportSettings       m_TransportSettings;
    av::TransportActions        m_TransportActionBits;
    av::MediaInfo               m_MediaInfo;
    ce::wstring                 m_NewURI;
    ce::wstring                 m_TransportActionString;
    ce::wstring                 m_URIFileExtension;
    ce::wstring                 m_MetaDataFileExtension; 
    ce::wstring                 m_MetaDataForNotify; 
    ce::wstring                 m_MetaDataResource;
    ce::wstring                 m_MetaDataTitle;
    ce::wstring                 m_MetaDataProtocolInfo; 
    ce::wstring                 m_MetaDataDate;
    ce::wstring                 m_MetaDataOrgFlags;
    ce::wstring                 m_MetaDataOrgPN;
    ce::wstring                 m_MetaDataOrgOp;
    ce::wstring                 m_MetaDataTrackSize;
    ce::wstring                 m_MetaDataRate;
    ce::wstring                 m_MetaDataChannels;
    ce::wstring                 m_MediaDurationMs;
    ce::wstring                 m_MetaDataResolution;
};
 
