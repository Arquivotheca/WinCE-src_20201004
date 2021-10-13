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

#include "stdafx.h"
#include "RenderMp2Ts.h"
#include <dvdmedia.h> // for MPEG2VIDEOINFO
#include <wmcodectypes.h> // for MEDIASUBTYPE_WVC1
#include <mtype.h>
#include <auto_xxx.hxx>
#include <hash_map.hxx>

using namespace RenderMpeg2TransportStream;

namespace
{

const wchar_t wszMpeg1Video[] = L"MPEG-1 Video";
const wchar_t wszMpeg2Video[] = L"MPEG-2 Video";
const wchar_t wszH264Video [] = L"H.264 Video";
const wchar_t wszVc1Video  [] = L"VC-1 Video";
const wchar_t wszMpeg1Audio[] = L"MPEG-1 Audio";
const wchar_t wszMpeg2Audio[] = L"MPEG-2 Audio";
const wchar_t wszAacAudio  [] = L"AAC Audio";
const wchar_t wszAc3Audio  [] = L"AC-3 Audio";
const wchar_t wszPsi       [] = L"PSI";

class ElementaryStreamInfo;

typedef ce::smart_ptr< ElementaryStreamInfo > shared_EsInfo;
typedef std::pair< shared_EsInfo, shared_EsInfo > AvInfo;
typedef std::pair< CComPtr< IPin >, CComPtr< IPin >> AvPins;

// helper functions make it easier to access std::pair-derived classes
bool HasAudio( const AvInfo& av ) { return( NULL != av.first ); }
bool HasVideo( const AvInfo& av ) { return( NULL != av.second ); }
bool NoAv( const AvInfo& av ) { return( !HasAudio( av ) && !HasVideo( av )); }

bool HasAudio( const AvPins& av ) { return( NULL != av.first ); }
bool HasVideo( const AvPins& av ) { return( NULL != av.second ); }

shared_EsInfo& AudioPtr( AvInfo& av ) { return av.first; }
shared_EsInfo& VideoPtr( AvInfo& av ) { return av.second; }

CComPtr< IPin >& AudioPtr( AvPins& av ) { return av.first; }
CComPtr< IPin >& VideoPtr( AvPins& av ) { return av.second; }

ElementaryStreamInfo& Audio( AvInfo& av ) { ASSERT( NULL != av.first ); return *( av.first ); }
ElementaryStreamInfo& Video( AvInfo& av ) { ASSERT( NULL != av.second ); return *( av.second ); }

// can return NULL if filter argument isn't joined to a graph
CComPtr< IGraphBuilder > GetGraphFromFilter( IBaseFilter& rFilter )
{
    FILTER_INFO info;
    HRESULT hr = rFilter.QueryFilterInfo( &info );
    ASSERT( SUCCEEDED( hr ));

    CComPtr< IFilterGraph > spGraph;
    CComPtr< IGraphBuilder > spBuilder;

    spGraph.Attach( info.pGraph ); // QueryFilterInfo already called AddRef; don't AddRef here
    if( spGraph )
    {
        hr = spGraph.QueryInterface( &spBuilder );
        ASSERT( SUCCEEDED( hr ));
    }

    return spBuilder;
}

// HRESULT indicates an error. (S_FALSE==hr && !rspFound) indicates the requested filter isn't in the graph
HRESULT FindFilterByClsid( IFilterGraph& rGraph, const CLSID& rClsid, CComPtr< IBaseFilter >& rspFound )
{
    ASSERT( !rspFound );

    CComPtr< IEnumFilters > spEnum;
    HRESULT hr = rGraph.EnumFilters( &spEnum );
    if( FAILED( hr ))
    {
        return hr;
    }

    while( true )
    {
        CComPtr< IBaseFilter > spFilter;

        hr = spEnum->Next( 1, &spFilter, NULL );
        if( FAILED( hr ) || ( S_FALSE == hr ))
        {
            return hr;
        }

        CLSID classId = GUID_NULL;
        hr = spFilter->GetClassID( &classId );
        ASSERT( SUCCEEDED( hr ));

        if(rClsid == classId)
        {
            rspFound = spFilter;
            return S_OK;
        }
    }
}

bool IsConnectedInputPin( IPin& rPin )
{
    PIN_DIRECTION dir;
    HRESULT hr = rPin.QueryDirection( &dir );
    ASSERT( SUCCEEDED( hr ));

    if( PINDIR_INPUT != dir )
    {
        return false;
    }

    CComPtr< IPin > spConnectedPin;
    hr = rPin.ConnectedTo( &spConnectedPin );
    ASSERT( VFW_E_NOT_CONNECTED == hr || S_OK == hr );

    return( SUCCEEDED( hr ));
}

bool IsOutputPin( IPin& rPin )
{
    PIN_DIRECTION dir;
    HRESULT hr = rPin.QueryDirection( &dir );
    ASSERT( SUCCEEDED( hr ));

    return( PINDIR_OUTPUT == dir );
}

HRESULT GetNextGraphEvent( IMediaEvent& rHandler, long& eventCode )
{
    LONG_PTR param1 = 0, param2 = 0;
    const long MaxWaitMs = 5000;
    
    HRESULT hr = rHandler.GetEvent( &eventCode, &param1, &param2, MaxWaitMs );
    if( FAILED( hr ))
    {
        ASSERT( E_ABORT == hr ); // failed on timeout
        return hr;
    }
    
    hr = rHandler.FreeEventParams( eventCode, param1, param2 );
    ASSERT( SUCCEEDED( hr ));

    return S_OK;
}

HRESULT GetFirstInputPinFromFilter( IBaseFilter& rFilter, CComPtr< IPin >& rspPin )
{
    ASSERT( !rspPin );

    CComPtr< IEnumPins > spEnum;
    HRESULT hr = rFilter.EnumPins( &spEnum );
    if( FAILED( hr ))
    {
        return hr;
    }

    while( true )
    {
        CComPtr< IPin > spPin;
        hr = spEnum->Next( 1, &spPin, NULL );
        if( FAILED( hr ))
        {
            return hr;
        }

        if( S_FALSE == hr )
        {
            return S_OK;
        }

        ASSERT( S_OK == hr );

        PIN_DIRECTION dir;
        HRESULT hr = spPin->QueryDirection( &dir );
        ASSERT( SUCCEEDED( hr ));
        
        if( PINDIR_INPUT == dir )
        {
            rspPin = spPin;
            return S_OK;
        }
    }
}

class PsiEventsCollector
{
public:
    PsiEventsCollector( ) :
        m_patChanged( false ),
        m_pmtChanged( false )
    { }

    void AddEvent( long event )
    {
        if( EC_PAT_CHANGED == event )
        {
            ASSERT( !m_patChanged );
            m_patChanged = true;
        }
        else if( EC_PMT_CHANGED == event )
        {
            ASSERT( !m_pmtChanged );
            m_pmtChanged = true;
        }
    }

    bool IsComplete( )
    {
        return( m_patChanged && m_pmtChanged );
    }

private:
    // disable copy ctor and assignment
    PsiEventsCollector( const PsiEventsCollector& );
    PsiEventsCollector& operator=( const PsiEventsCollector& );

    bool m_patChanged;
    bool m_pmtChanged;
};

class ElementaryStreamInfo
{
public:
    ElementaryStreamInfo( ) :
        pid( 0 ),
        streamName( NULL )
    { }

    ElementaryStreamInfo(
        unsigned long pidValue,
        const CMediaType& mtValue,
        const wchar_t* nameValue
        ) : pid( pidValue ), mediaType( mtValue ), streamName( nameValue )
    {
    }

    unsigned long pid;
    CMediaType mediaType;
    const wchar_t* streamName;
};

class AvRenderReport
{
public:
    AvRenderReport( const AvPins& rPins ) :
        m_isAudioRendered(false),
        m_isVideoRendered(false),
        m_hasAudioPin( HasAudio( rPins )),
        m_hasVideoPin( HasVideo( rPins ))
    { }

    void RenderedAudio() { m_isAudioRendered = true; }
    void RenderedVideo() { m_isVideoRendered = true; }

    HRESULT GetResult()
    {
        ASSERT( IsValidCombo_() );

        if( !m_isAudioRendered && !m_isVideoRendered )
        {
            return VFW_E_CANNOT_RENDER;
        }

        if( m_hasAudioPin && !m_isAudioRendered )
        {
            return VFW_S_AUDIO_NOT_RENDERED;
        }

        if( m_hasVideoPin && !m_isVideoRendered )
        {
            return VFW_S_VIDEO_NOT_RENDERED;
        }

        // render succeeded for available pins (A, V, or both)
        return S_OK;
    }

private:
    // disable default ctor, copy ctor and assignment
    AvRenderReport( );
    AvRenderReport( const AvRenderReport& );
    AvRenderReport& operator=( const AvRenderReport& );

    #if defined(DEBUG)
    bool IsValidCombo_()
    {
        if( !m_hasAudioPin && !m_hasVideoPin )    { return false; }
        if( !m_hasAudioPin && m_isAudioRendered ) { return false; }
        if( !m_hasVideoPin && m_isVideoRendered ) { return false; }
        return true;
    }
    #endif

    bool m_isAudioRendered;
    bool m_isVideoRendered;
    bool m_hasAudioPin;
    bool m_hasVideoPin;
};

class PinsQuery
{
public:
    PinsQuery( IBaseFilter* pFilter ) :
        m_spFilter( pFilter ),
        m_isEnumComplete( false ),
        m_hasNoOutputPins( true ),
        m_hasConnectedInputPin( false )
    {
        ASSERT( m_spFilter );
    }

    // HRESULT indicates an error. S_OK means filter has no input pins, otherwise S_FALSE
    HRESULT FilterHasNoOutputPins( )
    {
        HRESULT hr = EnumPins_( );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        return( m_hasNoOutputPins ? S_OK : S_FALSE );
    }

    // HRESULT indicates an error. S_OK means filter has a connected input pin, otherwise S_FALSE
    HRESULT FilterHasConnectedInputPin( )
    {
        HRESULT hr = EnumPins_( );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        return( m_hasConnectedInputPin ? S_OK : S_FALSE );
    }

private:
    // disable default ctor, copy ctor and assignment
    PinsQuery( );
    PinsQuery( const PinsQuery& );
    PinsQuery& operator=( const PinsQuery& );

    HRESULT EnumPins_( )
    {
        if( m_isEnumComplete )
        {
            return S_OK;
        }
    
        CComPtr< IEnumPins > spEnum;
        HRESULT hr = m_spFilter->EnumPins( &spEnum );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        while( true )
        {
            CComPtr< IPin > spPin;
            hr = spEnum->Next( 1, &spPin, NULL );
    
            if( FAILED( hr ))
            {
                return hr;
            }
    
            if( S_FALSE == hr )
            {
                break;
            }
    
            ASSERT( S_OK == hr );
    
            if( IsConnectedInputPin( *spPin ))
            {
                m_hasConnectedInputPin = true;
            }
    
            if( IsOutputPin( *spPin ))
            {
                m_hasNoOutputPins = false;
            }
        }
    
        m_isEnumComplete = true;
    
        return S_OK;
    }

    CComPtr< IBaseFilter > m_spFilter;
    bool m_isEnumComplete;
    bool m_hasNoOutputPins;
    bool m_hasConnectedInputPin;
};

class StreamInfo
{
public:
    static bool Mpeg1Video( ElementaryStreamInfo& info )
    {
        BYTE* pbFormat = info.mediaType.AllocFormatBuffer( sizeof( MPEG1VIDEOINFO ));
        if ( !pbFormat )
        {
            return false;
        }

        info.mediaType.SetType( &MEDIATYPE_Video );
        info.mediaType.SetSubtype( &MEDIASUBTYPE_MPEG1Video );
        info.mediaType.SetFormatType( &FORMAT_MPEGVideo );

        ZeroMemory( pbFormat, sizeof( MPEG1VIDEOINFO ));
        MPEG1VIDEOINFO& videoInfo = *( reinterpret_cast< MPEG1VIDEOINFO* >( pbFormat ));
        videoInfo.hdr.bmiHeader.biCompression = info.mediaType.subtype.Data1;
        videoInfo.hdr.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );

        info.streamName = wszMpeg1Video;

        return true;
    }

    static bool Mpeg2Video( ElementaryStreamInfo& info )
    {
        BYTE* pbFormat = info.mediaType.AllocFormatBuffer( sizeof( MPEG2VIDEOINFO ));
        if ( !pbFormat )
        {
            return false;
        }

        info.mediaType.SetType( &MEDIATYPE_Video );
        info.mediaType.SetSubtype( &MEDIASUBTYPE_MPEG2_VIDEO );
        info.mediaType.SetFormatType( &FORMAT_MPEG2Video );

        ZeroMemory( pbFormat, sizeof( MPEG2VIDEOINFO ));
        MPEG2VIDEOINFO& videoInfo = *( reinterpret_cast< MPEG2VIDEOINFO* >( pbFormat ));
        videoInfo.hdr.bmiHeader.biCompression = info.mediaType.subtype.Data1;
        videoInfo.hdr.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );

        info.streamName = wszMpeg2Video;

        return true;
    }

    static bool H264Video( ElementaryStreamInfo& info )
    {
        BYTE* pbFormat = info.mediaType.AllocFormatBuffer( sizeof( VIDEOINFOHEADER ));
        if ( !pbFormat )
        {
            return false;
        }

        info.mediaType.SetType( &MEDIATYPE_Video );
        info.mediaType.SetSubtype( &MEDIASUBTYPE_H264 );
        info.mediaType.SetFormatType( &FORMAT_VideoInfo );

        ZeroMemory( pbFormat, sizeof( VIDEOINFOHEADER ));
        VIDEOINFOHEADER& videoInfo = *( reinterpret_cast< VIDEOINFOHEADER* >( pbFormat ));
        videoInfo.bmiHeader.biCompression = info.mediaType.subtype.Data1;
        videoInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );

        info.streamName = wszH264Video;

        return true;
    }

    static bool Vc1Video( ElementaryStreamInfo& info )
    {
        BYTE* pbFormat = info.mediaType.AllocFormatBuffer( sizeof( VIDEOINFOHEADER ) + 1 );
        if ( !pbFormat )
        {
            return false;
        }

        info.mediaType.SetType( &MEDIATYPE_Video );
        info.mediaType.SetSubtype( &MEDIASUBTYPE_WVC1 );
        info.mediaType.SetFormatType( &FORMAT_VideoInfo );

        ZeroMemory( pbFormat, sizeof( VIDEOINFOHEADER ) + 1 );
        VIDEOINFOHEADER& videoInfo = *( reinterpret_cast< VIDEOINFOHEADER* >( pbFormat ));
        videoInfo.bmiHeader.biCompression = info.mediaType.subtype.Data1;
        videoInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );

        // Set the VC-1 binding byte
        pbFormat[ sizeof( VIDEOINFOHEADER ) ] = 0x01;

        info.streamName = wszVc1Video;

        return true;
    }

    static bool Mpeg1Audio( ElementaryStreamInfo& info )
    {
        BYTE* pbFormat = info.mediaType.AllocFormatBuffer( sizeof( MPEG1WAVEFORMAT ));
        if ( !pbFormat )
        {
            return false;
        }

        info.mediaType.SetType( &MEDIATYPE_Audio );
        info.mediaType.SetSubtype( &MEDIASUBTYPE_MPEG1AudioPayload );
        info.mediaType.SetFormatType( &FORMAT_WaveFormatEx );

        ZeroMemory( pbFormat, sizeof( MPEG1WAVEFORMAT ));
        MPEG1WAVEFORMAT& waveFormat = *( reinterpret_cast< MPEG1WAVEFORMAT* >( pbFormat ));
        waveFormat.wfx.wFormatTag = WAVE_FORMAT_MPEG;
        waveFormat.wfx.cbSize = sizeof( MPEG1WAVEFORMAT ) - sizeof( WAVEFORMATEX );

        info.streamName = wszMpeg1Audio;

        return true;
    }

    static bool Mpeg2Audio( ElementaryStreamInfo& info )
    {
        BYTE* pbFormat = info.mediaType.AllocFormatBuffer( sizeof( WAVEFORMATEX ));
        if ( !pbFormat )
        {
            return false;
        }

        info.mediaType.SetType( &MEDIATYPE_Audio );
        info.mediaType.SetSubtype( &MEDIASUBTYPE_MPEG2_AUDIO );
        info.mediaType.SetFormatType( &FORMAT_WaveFormatEx );

        ZeroMemory( pbFormat, sizeof( WAVEFORMATEX ));
        WAVEFORMATEX& waveFormat = *( reinterpret_cast< WAVEFORMATEX* >( pbFormat ));
        waveFormat.wFormatTag = WAVE_FORMAT_UNKNOWN;

        info.streamName = wszMpeg2Audio;

        return true;
    }

    static bool AacAudio( ElementaryStreamInfo& info )
    {
        BYTE* pbFormat = info.mediaType.AllocFormatBuffer( sizeof( WAVEFORMATEX ));
        if ( !pbFormat )
        {
            return false;
        }

        info.mediaType.SetType( &MEDIATYPE_Audio );
        info.mediaType.SetSubtype( &MEDIASUBTYPE_MPEG_ADTS_AAC );
        info.mediaType.SetFormatType( &FORMAT_WaveFormatEx );

        ZeroMemory( pbFormat, sizeof( WAVEFORMATEX ));
        WAVEFORMATEX& waveFormat = *( reinterpret_cast< WAVEFORMATEX* >( pbFormat ));
        waveFormat.wFormatTag = WAVE_FORMAT_MPEG_ADTS_AAC;

        info.streamName = wszAacAudio;

        return true;
    }

    static bool Ac3Audio( ElementaryStreamInfo& info )
    {
        BYTE* pbFormat = info.mediaType.AllocFormatBuffer( sizeof( WAVEFORMATEX ));
        if ( !pbFormat )
        {
            return false;
        }

        info.mediaType.SetType( &MEDIATYPE_Audio );
        info.mediaType.SetSubtype( &MEDIASUBTYPE_DOLBY_AC3 );
        info.mediaType.SetFormatType( &FORMAT_WaveFormatEx );

        ZeroMemory( pbFormat, sizeof( WAVEFORMATEX ));
        WAVEFORMATEX& waveFormat = *( reinterpret_cast< WAVEFORMATEX* >( pbFormat ));
        waveFormat.wFormatTag = WAVE_FORMAT_UNKNOWN;

        info.streamName = wszAc3Audio;

        return true;
    }
};

interface PsiSource
{
    virtual HRESULT ConnectTo( IBaseFilter& rPsiFilter, IPin& rPsiPin ) = 0;
    virtual HRESULT DisconnectFrom( IBaseFilter& rPsiFilter ) = 0;
};

interface PsiData
{
    virtual HRESULT FindFirstProgramWithAv( AvInfo& rInfo ) = 0;
};

class PsiParser : public PsiData
{
public:
    PsiParser( ) :
        m_spStreamInfoMap( InitStreamInfoMap_( ))
    { }

    HRESULT ConnectTo( PsiSource& rPsiSource )
    {
        HRESULT hr = CreatePsiFilter_( );
        if( FAILED( hr ))
        {
            return hr;
        }

        CComPtr< IPin > spInputPin;
        hr = GetFirstInputPinFromFilter( *m_spFilter, spInputPin );
        if( FAILED( hr ))
        {
            return hr;
        }

        if ( !spInputPin )
        {
            // the PSI Parser filter should always have an input pin by default
            ASSERT( false );
            return E_FAIL;
        }
    
        hr = rPsiSource.ConnectTo( *m_spFilter, *spInputPin );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        return S_OK;
    }

    HRESULT DisconnectFrom( PsiSource& rPsiSource )
    {
        ASSERT( m_spFilter );
        return rPsiSource.DisconnectFrom( *m_spFilter );
    }

    //
    // S_FALSE indicates that the graph terminated early or reached the end
    // of the stream without collecting enough PSI information
    HRESULT RunGraphToCollectData( )
    {
        ASSERT( m_spFilter );
        CComPtr< IGraphBuilder > spGraph( GetGraphFromFilter( *m_spFilter ));
        ASSERT( spGraph );
    
        CComPtr< IMediaControl > pControl;
        HRESULT hr = spGraph.QueryInterface( &pControl );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        CComPtr< IMediaEvent > spEvents;
        hr = spGraph.QueryInterface( &spEvents );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        hr = pControl->Run();
        if (FAILED(hr))
        {
            return hr;
        }
    
        PsiEventsCollector collector;
    
        while (true)
        {
            long eventCode = 0;
            hr = GetNextGraphEvent( *spEvents, eventCode );
            if( FAILED( hr ))
            {
                pControl->Stop(); // best effort
                return hr;
            }
    
            if( EC_COMPLETE == eventCode || EC_ERRORABORT == eventCode )
            {
                break;
            }
    
            collector.AddEvent( eventCode );
            if( collector.IsComplete( ))
            {
                break;
            }
        }
    
        hr = pControl->Stop();
        if( FAILED( hr ))
        {
            return hr;
        }
    
        return( collector.IsComplete( ) ? S_OK : S_FALSE );
    }

    //
    // PsiData interface
    //

    HRESULT FindFirstProgramWithAv( AvInfo& rInfo )
    {
        ASSERT( NoAv( rInfo ));
        ASSERT( m_spFilter );

        CComPtr< IMpeg2PsiParser > spPsi;
        HRESULT hr = m_spFilter.QueryInterface( &spPsi );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        int programCount = 0;
        hr = spPsi->GetCountOfPrograms( &programCount );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        for( int i = 0; i < programCount; ++i )
        {
            WORD programNumber = 0;
            hr = spPsi->GetRecordProgramNumber( i, &programNumber );
            if( FAILED( hr ))
            {
                return hr;
            }

            AvInfo info; // use fresh local var so we don't accumulate A/V info from multiple programs
            hr = FindAvInProgram_( *spPsi, programNumber, info );
            if( FAILED( hr ))
            {
                return hr;
            }
    
            if( HasAudio( info ) || HasVideo( info ))
            {
                rInfo = info;
                return S_OK;
            }
        }
    
        // no audio or video found in this program
        return S_OK;
    }

protected:
    HRESULT CreatePsiFilter_( )
    {
        ASSERT( !m_spFilter );
        return m_spFilter.CoCreateInstance( CLSID_PsiParser, NULL, CLSCTX_INPROC_SERVER );
    }

private:
    // disable copy ctor and assignment
    PsiParser( const PsiParser& );
    PsiParser& operator=( const PsiParser& );

    HRESULT FindAvInProgram_( IMpeg2PsiParser& rPsi, const int programNumber, AvInfo& rInfo )
    {
        ASSERT( NoAv( rInfo ));

        WORD streamCount = 0;
        HRESULT hr = rPsi.GetCountOfElementaryStreams(programNumber, &streamCount);
        if( FAILED( hr ))
        {
            return hr;
        }
    
        for( int i = 0; i < streamCount; ++i )
        {
            BYTE streamType;
            WORD pid;
            
            hr = rPsi.GetRecordStreamType( programNumber, i, &streamType );
            if( FAILED( hr ))
            {
                return hr;
            }
            
            hr = rPsi.GetRecordElementaryPid( programNumber, i, &pid );
            if( FAILED( hr ))
            {
                return hr;
            }
    
            if( ( IsAudioType_( streamType ) && !HasAudio( rInfo )) ||
                ( IsVideoType_( streamType ) && !HasVideo( rInfo )) )
            {
                SetStreamInfo_( streamType, pid, rInfo );
            }
    
            if( HasAudio( rInfo ) && HasVideo( rInfo ))
            {
                break; // we have enough, exit the loop early
            }
        }
    
        return S_OK;
    }

    HRESULT SetStreamInfo_( BYTE streamType, WORD pid, AvInfo& info )
    {
        ASSERT( ( IsAudioType_( streamType ) && !HasAudio( info )) ||
                ( IsVideoType_( streamType ) && !HasVideo( info )) );

        if( !m_spStreamInfoMap )
        {
            return E_OUTOFMEMORY;
        }

        shared_EsInfo spEsInfo( new ElementaryStreamInfo );
        if( !spEsInfo )
        {
            return E_OUTOFMEMORY;
        }

        spEsInfo->pid = pid;

        StreamInfoFn fn = FindStreamInfoFn_( streamType );
        ASSERT( fn ); // the IsAudioType_/IsVideoType_ checks above should guarantee this type is registered

        bool success = ( *fn )( *spEsInfo );
        if ( !success )
        {
            return E_OUTOFMEMORY;
        }

        if( IsAudioType_( streamType ))
        {
            AudioPtr( info ) = spEsInfo;
        }
        else
        {
            ASSERT( IsVideoType_( streamType ));
            VideoPtr( info ) = spEsInfo;
        }
    
        return S_OK;
    }

    // create a const lookup table to retrieve functions that will build
    // media types and other stream information given a stream type

    enum AvType { Audio, Video, None };

    typedef bool ( *StreamInfoFn )( ElementaryStreamInfo& );
    typedef std::pair< AvType, StreamInfoFn > stream_info_pair;
    typedef ce::hash_map< BYTE, stream_info_pair > stream_info_map;

    const stream_info_map* InitStreamInfoMap_( )
    {
        // # buckets must be power of 2...luckily we have exactly 8 conversion functions
        const size_t cBuckets = 8;

        ce::auto_ptr< stream_info_map > spMap( new stream_info_map( cBuckets ));
        if( !spMap )
        {
            return NULL;
        }

        #define EXIT_IF_FAILED( exp ) { if( spMap->end( ) == ( exp )) { return NULL; } }

        EXIT_IF_FAILED( spMap->insert( 0x01, std::make_pair( Video, &StreamInfo::Mpeg1Video )));
        EXIT_IF_FAILED( spMap->insert( 0x02, std::make_pair( Video, &StreamInfo::Mpeg2Video )));
        EXIT_IF_FAILED( spMap->insert( 0x1B, std::make_pair( Video, &StreamInfo::H264Video  )));
        EXIT_IF_FAILED( spMap->insert( 0xEA, std::make_pair( Video, &StreamInfo::Vc1Video   )));
        EXIT_IF_FAILED( spMap->insert( 0x03, std::make_pair( Audio, &StreamInfo::Mpeg1Audio )));
        EXIT_IF_FAILED( spMap->insert( 0x04, std::make_pair( Audio, &StreamInfo::Mpeg2Audio )));
        EXIT_IF_FAILED( spMap->insert( 0x0F, std::make_pair( Audio, &StreamInfo::AacAudio   )));
        EXIT_IF_FAILED( spMap->insert( 0x81, std::make_pair( Audio, &StreamInfo::Ac3Audio   )));

        #undef EXIT_IF_FAILED

        return spMap.release( );
    }

    stream_info_pair FindStreamInfo_( BYTE streamType )
    {
        ASSERT( m_spStreamInfoMap );
        stream_info_map::const_iterator it = m_spStreamInfoMap->find( streamType );
        if( m_spStreamInfoMap->end( ) == it )
        {
            return stream_info_pair( None, NULL );
        }

        return (*it).second;
    }

    StreamInfoFn FindStreamInfoFn_( BYTE streamType )
    {
        stream_info_pair& info = FindStreamInfo_( streamType );
        return info.second;
    }

    AvType FindStreamType_( BYTE streamType )
    {
        stream_info_pair& info = FindStreamInfo_( streamType );
        return info.first;
    }

    bool IsAudioType_( BYTE streamType )
    {
        return( Audio == FindStreamType_( streamType ));
    }

    bool IsVideoType_( BYTE streamType )
    {
        return( Video == FindStreamType_( streamType ));
    }

    const ce::auto_ptr< const stream_info_map > m_spStreamInfoMap;
    CComPtr< IBaseFilter > m_spFilter;
};

class Mpeg2Demux : public PsiSource
{
public:
    Mpeg2Demux( IBaseFilter* pFilter ) :
        m_spFilter( pFilter )
    {
        ASSERT( m_spFilter );
    }

    HRESULT ConfigureOutputPins( PsiData& rPsi, AvPins& rPins )
    {
        AvInfo avInfo;
        HRESULT hr = rPsi.FindFirstProgramWithAv( avInfo );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        if( NoAv( avInfo ))
        {
            return VFW_E_CANNOT_RENDER;
        }

        AvPins avPins;
        if( HasAudio( avInfo ))
        {
            hr = CreateOutputPin_( MEDIA_ELEMENTARY_STREAM, Audio( avInfo ), &AudioPtr( avPins ));
            if( FAILED( hr ))
            {
                return hr;
            }
        }
    
        if( HasVideo( avInfo ))
        {
            hr = CreateOutputPin_( MEDIA_ELEMENTARY_STREAM, Video( avInfo ), &VideoPtr( avPins ));
            if( FAILED( hr ))
            {
                return hr;
            }
        }

        rPins = avPins;
        return S_OK;
    }

    HRESULT FinishRendering( AvPins& rPins )
    {
        ASSERT( m_spFilter );
        CComPtr< IGraphBuilder > spGraph( GetGraphFromFilter( *m_spFilter ));
        ASSERT( spGraph );

        HRESULT hr = S_OK;
        AvRenderReport report( rPins );

        if( HasAudio( rPins ))
        {
            hr = spGraph->Render( AudioPtr( rPins ));
            if( SUCCEEDED( hr ))
            {
                report.RenderedAudio();
            }
        }

        if( HasVideo( rPins ))
        {
            hr = spGraph->Render( VideoPtr( rPins ));
            if( SUCCEEDED( hr ))
            {
                report.RenderedVideo();
            }
        }

        return report.GetResult();
    }

    // PsiSource impl
    HRESULT ConnectTo( IBaseFilter& rPsiFilter, IPin& rPsiPin )
    {
        CComPtr< IPin > spPin;
        HRESULT hr = CreatePsiOutputPin_( spPin );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        ASSERT( m_spFilter );
        CComPtr< IGraphBuilder > spGraph( GetGraphFromFilter( *m_spFilter ));
        ASSERT( spGraph );

        hr = spGraph->AddFilter( &rPsiFilter, NULL );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        hr = spGraph->ConnectDirect( spPin, &rPsiPin, NULL );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        return S_OK;
    }

    HRESULT DisconnectFrom( IBaseFilter& rPsiFilter )
    {
        ASSERT( m_spFilter );

        CComPtr< IMpeg2Demultiplexer > spMpeg2;
        HRESULT hr = m_spFilter.QueryInterface( &spMpeg2 );
        ASSERT( SUCCEEDED( hr ));

        ce::wstring pinName( wszPsi );

        hr = spMpeg2->DeleteOutputPin( pinName.get_buffer( ));
        if( FAILED( hr ))
        {
            return hr;
        }
        
        ASSERT( m_spFilter );
        CComPtr< IGraphBuilder > spGraph( GetGraphFromFilter( *m_spFilter ));
        ASSERT( spGraph );
    
        hr = spGraph->RemoveFilter( &rPsiFilter );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        return S_OK;
    }

private:
    // disable default ctor, copy ctor and assignment
    Mpeg2Demux( );
    Mpeg2Demux( const Mpeg2Demux& );
    Mpeg2Demux& operator=( const Mpeg2Demux& );

    HRESULT CreateOutputPin_(
        const MEDIA_SAMPLE_CONTENT contentType,
        ElementaryStreamInfo& rEsInfo,
        IPin** ppPin = NULL
        )
    {
        ASSERT( m_spFilter );

        CComPtr< IMpeg2Demultiplexer > spMpeg2;
        HRESULT hr = m_spFilter.QueryInterface( &spMpeg2 );
        ASSERT( SUCCEEDED( hr ));

        ce::wstring pinName( rEsInfo.streamName );
    
        CComPtr< IPin > spPin;
        hr = spMpeg2->CreateOutputPin( &rEsInfo.mediaType, pinName.get_buffer( ), &spPin );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        CComPtr< IMPEG2PIDMap > spMap;
        hr = spPin.QueryInterface( &spMap );
        if( FAILED( hr ))
        {
            return hr;
        }
    
        hr = spMap->MapPID( 1, &rEsInfo.pid, contentType );
        if ( FAILED( hr ))
        {
            return hr;
        }
    
        if( ppPin )
        {
            *ppPin = spPin.Detach( );
        }
    
        return S_OK;
    }

    HRESULT CreatePsiOutputPin_( CComPtr< IPin >& rspPin )
    {
        ASSERT( !rspPin );

        CMediaType mediaType;
        mediaType.SetType( &MEDIATYPE_MPEG2PSI );
    
        ElementaryStreamInfo info( 0x00, mediaType, wszPsi );
    
        HRESULT hr = CreateOutputPin_( MEDIA_MPEG2_PSI, info, &rspPin );
        if( FAILED( hr ))
        {
            ASSERT( !rspPin );
            return hr;
        }
    
        return S_OK;
    }

    CComPtr< IBaseFilter > m_spFilter;
};

}   // unnamed namespace

Graph::Graph( )
{
}

HRESULT Graph::RenderFile( IGraphBuilder* pGraph, const wchar_t* wszFile )
{
    if( !pGraph )
    {
        return E_POINTER;
    }

    ASSERT( !m_spGraph );
    m_spGraph = pGraph;

    HRESULT hr = m_spGraph->RenderFile( wszFile, NULL );
    if( FAILED( hr ))
    {
        return hr;
    }

    CComPtr< IBaseFilter > spMpeg2Demux;
    hr = FindMpeg2TransportStreamDemuxOnGraph_( spMpeg2Demux );
    if( FAILED( hr ))
    {
        return hr;
    }

    if( !spMpeg2Demux )
    {
        // not a transport stream; we're done
        return S_OK;
    }

    PsiParser psi;
    Mpeg2Demux demux( spMpeg2Demux );

    hr = psi.ConnectTo( demux );
    if( FAILED( hr ))
    {
        return hr;
    }

    hr = psi.RunGraphToCollectData( );
    if( FAILED( hr ))
    {
        return hr;
    }

    if( S_FALSE == hr )
    {
        // didn't collect enough data from the PSI
        return VFW_E_CANNOT_RENDER;
    }

    AvPins avPins;
    hr = demux.ConfigureOutputPins( psi, avPins );
    if( FAILED( hr ))
    {
        return hr;
    }

    hr = psi.DisconnectFrom( demux );
    if( FAILED( hr ))
    {
        return hr;
    }

    return demux.FinishRendering( avPins );
}

HRESULT Graph::GetMpeg2DemuxFilter_( CComPtr< IBaseFilter >& rspMpeg2Demux )
{
    return FindFilterByClsid( *m_spGraph, CLSID_MPEG2Demultiplexer, rspMpeg2Demux );
}

// HRESULT indicates an error. (S_FALSE==hr && !rspFound) indicates the demux
//   isn't in the graph, or it isn't in Transport Stream mode
HRESULT Graph::FindMpeg2TransportStreamDemuxOnGraph_( CComPtr< IBaseFilter >& rspFound )
{
    ASSERT( !rspFound );

    CComPtr< IBaseFilter > spMpeg2Demux;
    HRESULT hr = GetMpeg2DemuxFilter_( spMpeg2Demux );
    if( FAILED( hr ))
    {
        return hr;
    }

    if( spMpeg2Demux )
    {
        PinsQuery query( spMpeg2Demux );

        hr = query.FilterHasConnectedInputPin( );
        if( S_OK == hr )
        {
            hr = query.FilterHasNoOutputPins( );
            if( S_OK == hr )
            {
                // found the MPEG-2 Transport Stream demux
                rspFound = spMpeg2Demux;
                return S_OK;
            }
        }

        if( FAILED( hr ))
        {
            return hr;
        }
    }

    // demux not found, or it's not TS
    return S_FALSE;
}

