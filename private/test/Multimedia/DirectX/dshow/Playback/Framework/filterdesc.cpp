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
#include "FilterDesc.h"
#include "StringConversions.h"
#include <uuids.h>
#ifdef UNDER_CE
#include "op.h"
#endif

#ifdef UNDER_CE
#include <fslguids.h>
#include <qnetwork.h>
// Makes definitions of the following GUID declarations
#include <initguid.h>
#else
#include <initguid.h>
#include <qnetwork.h>
#endif

#include "videostampfilterguids.h"

// {2eeb4adf-4578-4d10-bca7-bb955f56320a}
DEFINE_GUID(CLSID_CWMADecMediaObject,  0x2eeb4adf, 0x4578, 0x4d10, 0xbc, 0xa7, 0xbb, 0x95, 0x5f, 0x56, 0x32, 0x0a);

// {cba9e78b-49a3-49ea-93d4-6bcba8c4de07}
DEFINE_GUID(CLSID_MP43Dec,
0xcba9e78b, 0x49a3, 0x49ea, 0x93, 0xd4, 0x6b, 0xcb, 0xa8, 0xc4, 0xde, 0x07);

// {6b928210-84e7-4930-9b33-1eb6f02b526e}
DEFINE_GUID(CLSID_MP13Dec,
0x6b928210, 0x84e7, 0x4930, 0x9b, 0x33, 0x1e, 0xb6, 0xf0, 0x2b, 0x52, 0x6e);

// Need to create a CLSID for MP13DecDMO
// {05E1D164-9331-437a-90EC-154086F9BCC1}
DEFINE_GUID(CLSID_MP13DecDMO, 
0x5e1d164, 0x9331, 0x437a, 0x90, 0xec, 0x15, 0x40, 0x86, 0xf9, 0xbc, 0xc1);


// {86A495AC-64CE-42DE-A13A-321ACC0F02DB} 
DEFINE_GUID(CLSID_MP13Dec2,
0x86a495ac, 0x64ce, 0x42de, 0xa1, 0x3a, 0x32, 0x1a, 0xcc, 0x0f, 0x02, 0xdb);

// {70E10528-6383-4166-A93F-42C15ABDF9C2} 
DEFINE_GUID(CLSID_MainConceptMPEG2Splitter,
0x70E10528, 0x6383, 0x4166, 0xA9, 0x3F, 0x42, 0xC1, 0x5A, 0xBD, 0xF9, 0xC2);

// {70E10538-6383-4166-A93F-42C15ABDF9C2} 
DEFINE_GUID(CLSID_MainConceptMPEG2VideoDecoder,
0x70E10538, 0x6383, 0x4166, 0xA9, 0x3F, 0x42, 0xC1, 0x5A, 0xBD, 0xF9, 0xC2);

// {70E10548-6383-4166-A93F-42C15ABDF9C2} 
DEFINE_GUID(CLSID_MainConceptMPEG2AudioDecoder,
0x70E10548, 0x6383, 0x4166, 0xA9, 0x3F, 0x42, 0xC1, 0x5A, 0xBD, 0xF9, 0xC2);

#ifndef UNDER_CE
// Undefined on the desktop but defined on CE

// {82d353df-90bd-4382-8bc2-3f6192b76e34}
DEFINE_GUID(CLSID_WMVDec,
0x82d353df, 0x90bd, 0x4382, 0x8b, 0xc2, 0x3f, 0x61, 0x92, 0xb7, 0x6e, 0x34);

// {D51BD5A1-7548-11cf-A520-0080C77EF58A}
DEFINE_GUID(CLSID_WAVEParser,
0xd51bd5a1, 0x7548, 0x11cf, 0xa5, 0x20, 0x0, 0x80, 0xc7, 0x7e, 0xf5, 0x8a);
#endif

// {CFD28198-115C-4606-BD64-27A7E0051D2A}
DEFINE_GUID(CLSID_MPEGDest,
0xCFD28198, 0x115C, 0x4606, 0xBD, 0x64, 0x27, 0xA7, 0xE0, 0x05, 0x1D, 0x2A);

// {731B8592-4001-46D4-B1A5-33EC792B4501}
DEFINE_GUID(CLSID_ElecardMPEG2Demux,
0x731B8592, 0x4001, 0x46D4, 0xB1, 0xA5, 0x33, 0xEC, 0x79, 0x2B, 0x45, 0x01);

// {BC4EB321-771F-4E9F-AF67-37C631ECA106}
DEFINE_GUID(CLSID_ElecardMPEG2Decoder,
0xBC4EB321, 0x771F, 0x4E9F, 0xAF, 0x67, 0x37, 0xC6, 0x31, 0xEC, 0xA1, 0x06);

// {00098205-76CC-497E-98A1-6EF10D0BF26C}
DEFINE_GUID(CLSID_ElecardMPEG2Encoder,
0x00098205, 0x76CC, 0x497E, 0x98, 0xA1, 0x6E, 0xF1, 0x0D, 0x0b, 0xF2, 0x6C);

// {CF2521A7-4029-4CC1-8C6E-F82BD82BB343}
DEFINE_GUID(CLSID_ElecardFileWriter,
0xCF2521A7, 0x4029, 0x4CC1, 0x8C, 0x6E, 0xF8, 0x2B, 0xD8, 0x2B, 0xB3, 0x43);

// {D76E2820-1563-11CF-AC98-00AA004C0FA9}
DEFINE_GUID(CLSID_AVIEncoder,
0xD76E2820, 0x1563, 0x11CF, 0xAC, 0x98, 0x00, 0xAA, 0x00, 0x4C, 0x0F, 0xA9);

// {22B8142-0946-11CF-BCB1-444553540000}
DEFINE_GUID(CLSID_Tee,
0x22b8142, 0x946, 0x11cf, 0xbc, 0xb1, 0x44, 0x45, 0x53, 0x54, 0x0, 0x0);

// d6655b8e-ebee-45d2-9a7a-477ef624b609
DEFINE_GUID(CLSID_MP4Mux,
0xd6655b8e, 0xebee, 0x45d2, 0x9a, 0x7a, 0x47, 0x7e, 0xf6, 0x24, 0xb6, 0x09);

//caa414f0-c7c3-11d4-a914-0080ad91bd94
DEFINE_GUID(CLSID_SigmaDDecoderFilter,
0xcaa414f0, 0xc7c3, 0x11d4, 0xa9, 0x14, 0x00, 0x80, 0xad, 0x91, 0xbd, 0x94);

// This is a statically constructed list of filter names, their local id and the type
// Update both the count and table together
int FilterInfoMapper::nFilters = MAX_FILTERS;
FilterInfo* FilterInfoMapper::map[MAX_FILTERS] = 
{
    new FilterInfo(STR_WMAAudioDecoderDMO,        WMADEC,        WMAAudioDecoderDMO, WMAAudioDecoderDMO_TYPE, CLSID_CWMADecMediaObject),
    new FilterInfo(STR_WMVVideoDecoderDMO,        WMVDEC,        WMVVideoDecoderDMO, WMVVideoDecoderDMO_TYPE, CLSID_WMVDec),
    new FilterInfo(STR_WMVVideoMPEG4DecoderDMO,    WMVDEC,        WMVVideoDecoderDMO, WMVVideoDecoderDMO_TYPE, CLSID_WMVDec),
    new FilterInfo(STR_MP43VideoDecoderDMO,        MP43DEC,    MP43VideoDecoderDMO, MP43VideoDecoderDMO_TYPE, CLSID_MP43Dec),

    new FilterInfo(STR_MPEGVideoCodec,            MPGDEC,        MPEGVideoDecoder, MPEGVideoDecoder_TYPE, CLSID_CMpegVideoCodec),
    new FilterInfo(STR_MPEGVideoDecoder,            MPGDEC,        MPEGVideoDecoder, MPEGVideoDecoder_TYPE, CLSID_CMpegVideoCodec),

    new FilterInfo(STR_MPEGLayer3AudioDecoder,    MP3DEC,        MPEGAudioDecoder, MPEGAudioDecoder_TYPE, CLSID_CMpegAudioCodec),
    new FilterInfo(STR_MPEG1Layer3AudioDecoderDMO, MP13DECDMO,    MP13AudioDecoderDMO, MP13AudioDecoder_TYPE, CLSID_MP13DecDMO),
    new FilterInfo(STR_MPEG1Layer3AudioDecoder, MP13DEC,    MP13AudioDecoder, MP13AudioDecoder_TYPE, CLSID_MP13Dec),
    new FilterInfo(STR_MPEGAudioDecoder,        MP3DEC,        MPEGAudioDecoder, MPEGAudioDecoder_TYPE, CLSID_CMpegAudioCodec),
    new FilterInfo(STR_MPEGAudioCodec,            MP3DEC,        MPEGAudioDecoder, MPEGAudioDecoder_TYPE, CLSID_CMpegAudioCodec),
    new FilterInfo(STR_MainConceptMPEG2AudioDecoder,    MP2ADEC,    MPEG2AudioDecoder, MPEG2AudioDecoder_TYPE, CLSID_MainConceptMPEG2AudioDecoder),
    new FilterInfo(STR_MainConceptMPEG2VideoDecoder,    MP2VDEC,    MPEG2VideoDecoder, MPEG2VideoDecoder_TYPE, CLSID_MainConceptMPEG2VideoDecoder),

    new FilterInfo(STR_ElecardMPEG2VideoDecoder,    MP2VDEC,    MPEG2VideoDecoder, MPEG2VideoDecoder_TYPE, CLSID_ElecardMPEG2Decoder),
    new FilterInfo(STR_ElecardMPEG2VideoEncoder,    MP2VENC,    MPEG2VideoEncoder, MPEG2VideoEncoder_TYPE, CLSID_ElecardMPEG2Encoder),

    new FilterInfo(STR_AVIEncoder,                AVIENC,    AVIEncoder,      AVIEncoder_TYPE, CLSID_AVIEncoder),
    new FilterInfo(STR_AVIDecoder,                AVIDEC,    AVIDecoder,      AVIDecoder_TYPE, CLSID_AVIDec),

    new FilterInfo(STR_WaveParser,                WAVPAR,        WaveParser, WaveParser_TYPE, CLSID_WAVEParser),
    new FilterInfo(STR_ACMWrapper,                ACMWRAP,    ACMWrapper, ACEWrapper_TYPE, CLSID_ACMWrapper),
    
    new FilterInfo(STR_AVISplitter,                AVISPLIT,    AVISplitter,      AVISplitter_TYPE, CLSID_AviSplitter),
    new FilterInfo(STR_MPEG1StreamSplitter,        MPGSPLIT,    MPEG1StreamSplitter, MPEG1StreamSplitter_TYPE, CLSID_MPEG1Splitter),
    new FilterInfo(STR_MPEG2Demux,                MPEG2DMUX,    MPEG2Demux, MPEG2Demux_TYPE, CLSID_MPEG2Demultiplexer),
    new FilterInfo(STR_ElecardMPEG2Demux,                MPEG2DMUX,    ElecardMPEG2Demux, MPEG2Demux_TYPE, CLSID_ElecardMPEG2Demux),
    new FilterInfo(STR_MainConceptMPEG2Splitter,MPEG2DMUX,    MPEG2Demux, MPEG2Demux_TYPE, CLSID_MainConceptMPEG2Splitter),
    new FilterInfo(STR_InfiniteTee,    INFTEE,    InfiniteTee, InfiniteTee_TYPE, CLSID_Tee),
    new FilterInfo(STR_MPEG4Demux,    MP4MUX,    MPEG4Demux, MPEG4Demux_TYPE, CLSID_MP4Mux),

    new FilterInfo(STR_DefaultAudioRenderer,    AUDREND,    DefaultAudioRenderer, DefaultAudioRenderer_TYPE, CLSID_AudioRender),
    new FilterInfo(STR_DefaultDSoundDev,        AUDREND,    DefaultDSoundDev, DefaultDSoundDev_TYPE, CLSID_DSoundRender),
    new FilterInfo(STR_VideoMixingRenderer,        VMRREND,    VideoMixingRenderer, VideoMixingRenderer_TYPE, CLSID_VideoMixingRenderer),
#ifdef UNDER_CE
    new FilterInfo(STR_ScriptCmdRenderer,       SCMDREND,   ScriptCmdRenderer, ScriptCmdRenderer_TYPE, CLSID_ScriptCommandRenderer),
    new FilterInfo(STR_ASFICMHandler,            ICMDISP,    ASFICMHandler, ASFICMHandler_TYPE, CLSID_ASFICMHandler),
    new FilterInfo(STR_ASFACMHandler,            ACMDISP,    ASFACMHandler, ASFACMHandler_TYPE, CLSID_ASFACMHandler),
    new FilterInfo(STR_WMSourceFilter,            NSSOURCE,    WMSourceFilter, WMSourceFilter_TYPE, CLSID_NetShowSource),
    new FilterInfo(STR_MIDIParser,            MIDIPSR,    MIDIParser, MIDIParser_TYPE, CLSID_MIDIParser),
    new FilterInfo(STR_MIDISynthesizer,            MIDISYN,    MIDISynthesizer, MIDISynthesizer_TYPE, CLSID_MIDISynth),    
#endif
    new FilterInfo(STR_AsyncReader,                ASYNCRDR,    AsyncReader,    AsyncReader_TYPE, CLSID_AsyncReader),
    new FilterInfo(STR_UrlReader,                URLRDR,        UrlReader,    UrlReader_TYPE, CLSID_URLReader),
    new FilterInfo(STR_ColorConverter,            CLRCONV,    ColorConverter,    ColorConverter_TYPE, CLSID_Colour),
    new FilterInfo(STR_AVIMux,                    AVIMUX,        AVIMuxFilter,    AVIMuxFilter_TYPE,    CLSID_AviDest),
    new FilterInfo(STR_MPEGMux,                    MPEGMUX,        MPEGMuxFilter,    MPEGMuxFilter_TYPE,    CLSID_MPEGDest),
#ifndef UNDER_CE
    new FilterInfo(STR_WMAsfWriter,                    WMASFWRITER,        WMAsfWriterFilter,    WMAsfWriterFilter_TYPE,    CLSID_WMAsfWriter),
    new FilterInfo(STR_WMAsfReader,                    WMASFREADER,        WMAsfReaderFilter,     WMAsfReaderFilter_TYPE,    CLSID_WMAsfReader),
#else
    new FilterInfo(STR_WMAsfWriter,                    WMASFWRITER,        WMAsfWriterFilter,    WMAsfWriterFilter_TYPE,    CLSID_ASFWriter),
#endif
    new FilterInfo(STR_VideoStampWriter,                    VSWRITE,        VideoStampWriterFilter,    VideoStampWriter_TYPE,    CLSID_VideoStampWriterFilter),
    new FilterInfo(STR_VideoStampReader,                    VSREAD,        VideoStampReaderFilter,    VideoStampReader_TYPE,    CLSID_VideoStampReaderFilter),
    new FilterInfo(STR_OVMixer,                    OVMIXER,        OverlayMixer,    OverlayMixer_TYPE,    CLSID_OverlayMixer),
#ifdef UNDER_CE
    new FilterInfo(STR_OVMixer,                    OVMIXER,        OverlayMixer2,    OverlayMixer_TYPE,    CLSID_OverlayMixer2),
#endif

    new FilterInfo(STR_FileWriter,                FILEWR,        FileWriterFilter,    FileWriterFilter_TYPE,    CLSID_FileWriter),
    new FilterInfo(STR_ElecardFileWriter,                FILEWR,        ElecardFileWriterFilter,    FileWriterFilter_TYPE,    CLSID_ElecardFileWriter),
    new FilterInfo(STR_SigmaDDecoderFilter,                SIGMAD,        SigmaDDecoderFilter,    SigmaDDecoderFilter_TYPE,    CLSID_SigmaDDecoderFilter),

#ifdef UNDER_CE
    new FilterInfo(STR_BufferingStreamingFilter,    BUFFSTRM,        BufferingStreamingFilter,    BufferingStreamingFilter_TYPE,    CLSID_BufferingStreamFilter),
    new FilterInfo(STR_iMX27VPUDecoderFilter,       MX27VPU,        iMX27DecoderFilter,         iMX27VPUDecoderFilter_TYPE,         CLSID_FSLMX27VPUDEC),
#endif 

};

FilterInfoMapper::FilterInfoMapper()
{
}

FilterInfoMapper::~FilterInfoMapper()
{
    for(int i = 0; i < FilterInfoMapper::nFilters; i++)
    {
        if (FilterInfoMapper::map[i])
        {
            delete FilterInfoMapper::map[i];
            FilterInfoMapper::map[i] = NULL;
        }
    }
}

// Instantiate the object so the desctructor will be called and resources can be freed
FilterInfoMapper g_FilterInfoMapper;

// Given a filter name, get the filter id and type
HRESULT FilterInfoMapper::GetFilterInfo(const TCHAR* szName, FilterInfo* pFilterInfo)
{
    HRESULT hr = S_OK;
    if (!pFilterInfo)
        return E_INVALIDARG;

    if ( !szName )
        return E_INVALIDARG;
    
    int i = 0;
    for(; i < nFilters; i++)
    {
        // BUGBUG: visit it later for the case when one filter's name is a substring of another's
        if (map[i] && map[i]->szName && _tcsstr(szName, map[i]->szName) ) // !_tcscmp(map[i]->szName, szName)) 
        {
            hr = pFilterInfo->Init(map[i]->szName, map[i]->szWellKnownName, map[i]->id, map[i]->type, map[i]->guid);
            if (FAILED(hr))
                return hr;
            break;
        }
    }

    // return S_FALSE if our map does not have the information about this filter.
    return (i < nFilters) ? S_OK : S_FALSE;
}

// Given a filter id, get the filter info
HRESULT FilterInfoMapper::GetFilterInfo(FilterId id, FilterInfo* pFilterInfo)
{
    HRESULT hr = S_OK;
    int i = 0;
    
    if (id == UnknownFilter)
        return E_FAIL;

    if (!pFilterInfo)
        return E_INVALIDARG;

    for(; i < nFilters; i++)
    {
        if (map[i] && map[i]->szName && _tcscmp(map[i]->szName, _T("")) && (map[i]->id == id)) 
        {
            hr = pFilterInfo->Init(map[i]->szName, map[i]->szWellKnownName, map[i]->id, map[i]->type, map[i]->guid);
            if (FAILED(hr))
                return hr;
            break;
        }
    }

    // return S_FALSE if our map does not have the information about this filter.
    return (i < nFilters) ? S_OK : S_FALSE;
}


FilterInfo::FilterInfo()
{
    szName = NULL;
    szWellKnownName = NULL;
}

FilterInfo::FilterInfo(TCHAR* szName, TCHAR* szWellKnownName, FilterId id, DWORD type, GUID guid)
{
    this->szName = NULL;
    this->szWellKnownName = NULL;
    Init(szName, szWellKnownName, id, type, guid);
}

FilterInfo::~FilterInfo()
{
    if (szName)
        delete [] szName;
    if (szWellKnownName)
        delete [] szWellKnownName;
}

HRESULT FilterInfo::Init(TCHAR* szName, TCHAR* szWellKnownName, FilterId id, DWORD type, GUID guid)
{
    HRESULT hr = S_OK;
    if (this->szName)
        delete this->szName;
    if (szName)
    {
        this->szName = new TCHAR[_tcslen(szName) + 1];
        if (!this->szName)
            return E_OUTOFMEMORY;
        _tcscpy_s(this->szName, _tcslen(szName) + 1, szName );
    }
    else this->szName = NULL;
    

    if (this->szWellKnownName)
        delete this->szWellKnownName;
    if (szWellKnownName)
    {
        this->szWellKnownName = new TCHAR[_tcslen(szWellKnownName) + 1];
        if (!this->szWellKnownName)
            return E_OUTOFMEMORY;

        _tcscpy_s( this->szWellKnownName, _tcslen(szWellKnownName) + 1, szWellKnownName );
    }
    else this->szWellKnownName = NULL;

    this->id = id;    
    this->guid = guid;
    this->type = type;
    return hr;
}

FilterDesc::FilterDesc()
{
    Init();
}

FilterDesc::FilterDesc(FilterId id)
{
    Init();
    m_id = id;
}

FilterDesc::FilterDesc(IBaseFilter* pBaseFilter, TCHAR* szUrl, HRESULT* phr)
{
    HRESULT hr = S_OK;

    if (!szUrl || !pBaseFilter)
    {
        *phr = E_INVALIDARG;
        return;
    }

    // Init the member variables
    Init();

    // Determine the type/name/guid of the filter
    if (pBaseFilter)
    {
        pBaseFilter->AddRef();
        m_pFilter = pBaseFilter;

        // Copy the url - we need this to identify source filters
        m_szCurrUrl[countof(m_szCurrUrl) - 1] = TEXT('\0');
        _tcscpy_s(m_szCurrUrl, MAX_PATH, szUrl );

        // Get the filter info - this gets the filter m_id and type
        hr = GetFilterInfo();
        if ( hr == S_FALSE )
            LOG(TEXT("%S: WARNING - failed to get the filter info from our map. Could be an unknown filter?\n"), __FUNCTION__);
        else if ( FAILED(hr) )
            LOG(TEXT("%S: ERROR - failed to get the filter info.\n"), __FUNCTION__);

    }

    *phr = hr;
}

FilterDesc::~FilterDesc()
{
    if (m_pFilter)
        m_pFilter->Release();
    if (m_pGraph)
        m_pGraph->Release();
}

void FilterDesc::Init()
{
    memset(this, 0, sizeof(FilterDesc));
    m_id = UnknownFilter;
    m_guid = GUID_NULL;
}

FilterId FilterDesc::GetId()
{
    return m_id;
}

DWORD FilterDesc::GetType()
{
    return m_type;
}

HRESULT FilterDesc::AddToGraph(IGraphBuilder* pGraph)
{
    HRESULT hr = S_OK;
    WCHAR wszFilterName[MAX_PATH];
    
    // If already added return error
    if (m_pGraph || !m_pFilter)
        return E_FAIL;

    // Add to the graph
    hr = pGraph->AddFilter(m_pFilter, TCharToWChar(m_szFilterName, wszFilterName, MAX_PATH));
    if (SUCCEEDED(hr))
    {
        m_pGraph = pGraph;
        pGraph->AddRef();
    }
    
    // BUGBUG: Get the name

    return hr;
}

const TCHAR* FilterDesc::GetName()
{
    // BUGBUG: is this what we need to return
    return m_szWellKnownName;
}

void FilterDesc::SetName(const TCHAR* szFilterName)
{
    _tcscpy_s( m_szFilterName, MAX_PATH, szFilterName );
}

IPin* FilterDesc::GetPin(int i, PIN_DIRECTION dir)
{
    HRESULT hr = S_OK;
    int nPins = 0;
    IPin** ppPinArray = NULL;
    IPin* pPin = NULL;

    if (i < 0)
        goto cleanup;

    // Get the pin array
    if (FAILED(hr = GetPinArray(&ppPinArray, &nPins, dir)))
        goto cleanup;

    if (i >= nPins)
        goto cleanup;

    // Get the desired pin
    pPin = ppPinArray[i];
    if (pPin)
        pPin->AddRef();

cleanup: 
    if(FAILED(hr))
    {
        if(pPin)
        {
            pPin->Release();
            pPin = NULL;
        }
    }

    // Delete the pin array
    DeletePinArray(ppPinArray, nPins);

    return pPin;
}

HRESULT FilterDesc::GetPinIndex(IPin* pPin, int *pIndex)
{
    HRESULT hr = S_OK;

    if (!pPin || !pIndex)
        return E_POINTER;

    PIN_DIRECTION pindir;
    pPin->QueryDirection(&pindir);

    int nPins = 0;
    IPin** ppPinArray = NULL;

    // Get the pin array
    hr = GetPinArray(&ppPinArray, &nPins, pindir);
    if (FAILED(hr))
        return hr;

    // Get the desired pin
    int i = 0;
    for(; i < nPins; i++)
    {
        if (pPin == ppPinArray[i])
            break;
    }

    // Delete the pin array
    DeletePinArray(ppPinArray, nPins);

    if (i < nPins)
    {
        *pIndex = i;
        return S_OK;
    }

    return E_FAIL;
}

//
// The help function to create filter instances if the filter information 
// is listed in our internal map.
//
HRESULT FilterDesc::CreateFilterInstance()
{
    HRESULT hr = S_OK;

    // Map the filter id to the GUID
    FilterInfo filterInfo;
    
    if (m_id == UnknownFilter)
        return E_FAIL;
    
    hr = g_FilterInfoMapper.GetFilterInfo(m_id, &filterInfo);
    if (FAILED_F(hr))
    {
        // Can't find matching filter for this filter name
        return E_FAIL;
    }
    
    // Create the filter
    m_id = filterInfo.id;
    m_type = filterInfo.type;
    m_guid = filterInfo.guid;
    // BUGBUG: do a query info to get the filter name
    _tcscpy_s( m_szFilterName, MAX_PATH, filterInfo.szName );
    _tcscpy_s( m_szWellKnownName, MAX_FILTER_NAME, filterInfo.szWellKnownName );

    // With the guid - CoCreate the filter
    hr = CoCreateInstance(m_guid, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**) &m_pFilter);

    return hr;
}

IBaseFilter* FilterDesc::GetFilterInstance()
{
    // Return the filter pointer
    if (m_pFilter)
        m_pFilter->AddRef();
    return m_pFilter;
}

// The filter that we are trying to identify is a source filter
HRESULT FilterDesc::GetSourceFilterInfo()
{
    HRESULT hr = S_OK;

    FilterId sourceId = UnknownFilter;
    DWORD sourceType = 0;
    TCHAR* szSourceFilter = NULL;
    TCHAR* szWellKnownName = NULL;

    // Check all the default source filters that we know of
    
    // First, check if this is the WM Source Filter
    // BUGBUG: Query the filter for the following interfaces: IFileSourceFilter
    IUnknown* pInterface = NULL;
    hr = CheckInterface(m_pFilter, IID_IFileSourceFilter);
    if (SUCCEEDED(hr))
    {
        // Now check for IAMMediaContent
        hr = CheckInterface(m_pFilter, IID_IAMMediaContent);
        if (SUCCEEDED(hr))
        {
            sourceId = WMSourceFilter;
            sourceType = WMSourceFilter_TYPE;
            szSourceFilter =  STR_WMSourceFilter;
            szWellKnownName = NSSOURCE;
        }
    }

    // If still unknown, check for the URL File Source Filter
    if (sourceId == UnknownFilter)
    {
        // Query for IFileSourceFilter and IAMOpenProgress
        hr = CheckInterface(m_pFilter, IID_IAMOpenProgress);
        if (SUCCEEDED(hr))
        {
            hr = CheckInterface(m_pFilter, IID_IFileSourceFilter);
            if (SUCCEEDED(hr))
            {
                sourceId = UrlReader;
                sourceType = UrlReader_TYPE;
                szSourceFilter =  STR_UrlReader;
                szWellKnownName = URLRDR;
            }
        }
    }

    // If still unknown, check for the Async reader interface
    if (sourceId == UnknownFilter)
    {
        // Query for IFileSourceFilter
        hr = CheckInterface(m_pFilter, IID_IFileSourceFilter);
        if (SUCCEEDED(hr))
        {
            sourceId = AsyncReader;
            sourceType = AsyncReader_TYPE;
            szSourceFilter = STR_AsyncReader;
            szWellKnownName = ASYNCRDR;
        }
    }

    if (sourceId != UnknownFilter)
    {
        m_id = sourceId;
        m_type = sourceType;
        _tcscpy_s(m_szWellKnownName, MAX_FILTER_NAME, szWellKnownName );
        _tcscpy_s(m_szFilterName, MAX_PATH, szSourceFilter);
    }

    return hr;
}

/*
 * Once a filter has been created and added to a graph - there seems to be no way of retrieiving the actual type of the filter
 * - except by querying the filter for its name
 * So this method is making the assumption that filters return something that is (mostly) unique, non-null and doesn't change over time.
 */
HRESULT FilterDesc::GetFilterInfo()
{
    FILTER_INFO info;
    HRESULT hr = NOERROR;

    // Get the filter info
    hr = m_pFilter->QueryFilterInfo(&info);
    if (FAILED(hr)) {
        LOG(TEXT("%S: ERROR - failed to get filter info.\n"), __FUNCTION__);
        return hr;
    }
    else {
        TCHAR szFilterName[FILTER_NAME_LENGTH];
        WCharToTChar(info.achName, szFilterName, countof(szFilterName));
        LOG(TEXT("%S: retrieved filter info - %s \n"), __FUNCTION__, szFilterName);
    }

    // Query  for the IFileSourceFilter interface
    hr = CheckInterface(m_pFilter, IID_IFileSourceFilter);
    if (hr == S_OK)
    {
        // First check if the name of the filter is the same as the current url
        // BUGBUG: doesn't seem to work on CE - if (!_tcscmp(info.achName, m_szCurrUrl)) {

        // We have a source filter
        hr = GetSourceFilterInfo();
        if (FAILED(hr))
            LOG(TEXT("%S: ERROR - failed to get the id of source filter for %s.\n"), __FUNCTION__, m_szCurrUrl);
    }
    else {
        // If not a source filter, try to get the id and type from the filter mapper
        FilterInfo finfo;
        TCHAR szFilterName[FILTER_NAME_LENGTH];
        WCharToTChar(info.achName, szFilterName, countof(szFilterName));
        hr = g_FilterInfoMapper.GetFilterInfo(szFilterName, &finfo);
        if ( S_FALSE == hr )
        {
            LOG(TEXT("%S: WARNING - Unkown filter: %s.\n"), __FUNCTION__, szFilterName );
        }
        else if ( FAILED(hr) )
        {
            LOG(TEXT("%S: ERROR - failed to get the id of filter %s.\n"), __FUNCTION__, info.achName);
        }
        else 
        {
            // The filter name as exported by the filter
            _tcscpy_s(m_szFilterName, MAX_PATH, szFilterName);
            // The id
            m_id = finfo.id;
            // The type
            m_type = finfo.type;
            // The GUID
            m_guid = finfo.guid;
            // Our name for the filter
            _tcscpy_s(m_szWellKnownName, MAX_FILTER_NAME, finfo.szWellKnownName);
        }
    }

    // Release the ref count on the filter graph
    info.pGraph->Release();

    return hr;
}

// This helper method allocates a pin array and gets the pins in the specified direction
HRESULT FilterDesc::GetPinArray(IPin*** pppPins, int *pNumPins, PIN_DIRECTION pinDir)
{
    IEnumPins* pEnumPins = NULL;
    HRESULT hr = S_OK;
    int nPins = 0;

    if (FAILED(hr = m_pFilter->EnumPins(&pEnumPins)))
    {
        LOG(TEXT("GetPinArray: EnumPins failed.\n"));
        return hr;
    }
    
    if (FAILED(hr = pEnumPins->Reset())) 
    {
        LOG(TEXT("GetPinArray: EnumPins::Reset failed.\n"));
        pEnumPins->Release();
        return hr;
    }
        
    PIN_DIRECTION dir;
    IPin *pPin = NULL;
    ULONG cPins = 0;
    while ((hr = pEnumPins->Next(1, &pPin, 0)) == S_OK) {
        pPin->QueryDirection(&dir);
        if (dir == pinDir)
            nPins++;
        pPin->Release();
    }
    
    // Next returns S_FALSE which is a success when it goes the end
    if (nPins && SUCCEEDED(hr) && pppPins) {
        hr = S_OK;
        IPin** ppPins = (IPin**) new PPIN[nPins];
        if (!ppPins)
        {
            LOG(TEXT("GetPinArray: couldn't allocate pin array.\n"));
            hr = E_OUTOFMEMORY;
        }
        else {
            *pppPins = ppPins;
            
            pEnumPins->Reset();
            int i = 0;
            while ((hr = pEnumPins->Next(1, &pPin, 0)) == S_OK) {
                pPin->QueryDirection(&dir);
                if (dir == pinDir) {
                    ppPins[i] = pPin;
                    i++;
                } else pPin->Release();
            }
            hr = S_OK;
        }
    }
    
    if (pEnumPins)
        pEnumPins->Release();

    *pNumPins = (SUCCEEDED(hr)) ? nPins : 0;

    return hr;
}

HRESULT FilterDesc::GetConnectedFilterArray(IBaseFilter*** pppFilters, int *pNumFilters, PIN_DIRECTION pinDir)
{
    HRESULT hr = S_OK;
    int nPins = 0;
    IPin** ppPins = NULL;
    IPin** ppConnectedPin = NULL;
    IBaseFilter** ppConnectedFilter = NULL;

    // Get the pin array for the given filter in the given direction
    hr = GetPinArray(&ppPins, &nPins, pinDir);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR - failed to get pin array.\n"), __FUNCTION__);
        goto cleanup;
    }


    // Allocate the pin array
    ppConnectedPin = new IPin*[nPins];
    if (!ppConnectedPin)
        goto cleanup;

    memset(ppConnectedPin, 0, sizeof(IPin*)*nPins);
    for(int i = 0; i < nPins; i++)
    {
        hr = ppPins[i]->ConnectedTo(&ppConnectedPin[i]);
        if (hr == VFW_E_NOT_CONNECTED)
        {
            ppConnectedPin[i] = NULL;
        }
        else if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR - failed to get %dth connected pin.\n"), __FUNCTION__, i);
            goto cleanup;
        }
    }

    // Allocate the connected filter array
    ppConnectedFilter = new IBaseFilter*[nPins];
    if (!ppConnectedFilter)
        goto cleanup;

    memset(ppConnectedFilter, 0, sizeof(IBaseFilter*)*nPins);
    
    // Once we have the connected pins, we can do a QueryPinInfo and get the IBaseFilter*
    for(int i = 0; i < nPins; i++)
    {
        // Query the pin info
        PIN_INFO pinInfo;
        if (ppConnectedPin[i])
        {
            hr = ppConnectedPin[i]->QueryPinInfo(&pinInfo);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR - failed to get PIN_INFO of the %dth connected pin.\n"), __FUNCTION__, i);
                goto cleanup;
            }

            // Once we have the pin info - set the connected filter
            ppConnectedFilter[i] = pinInfo.pFilter;
        }
        else {
            ppConnectedFilter[i] = NULL;
        }
    }

    // Now we have the connected filters in the direction specified
    if (pppFilters)
        *pppFilters = ppConnectedFilter;
    *pNumFilters = nPins;

cleanup:
    // Clean up the filter's pins
    if (NULL != ppPins)
        DeletePinArray(ppPins, nPins);

    // Clean up the filter's connected pins
    if (NULL != ppConnectedPin)
    {
        for(int i = 0; i < nPins; i++)
        {
            if (NULL != ppConnectedPin[i])
                ppConnectedPin[i]->Release();
        }
        
        delete [] ppConnectedPin;
    }

    // Clean up the filter's connected filters if we failed - otherwise it is returned
    if (FAILED(hr))
    {
        if (ppConnectedFilter)
        {
            for(int i = 0; i < nPins; i++)
            {
                if (ppConnectedFilter[i])
                    ppConnectedFilter[i]->Release();
            }
            
            delete [] ppConnectedFilter;
        }
    }
    
    return hr;
}

HRESULT FilterDesc::SaveToXml(HANDLE hXmlFile, int depth)
{
    HRESULT hr = NOERROR;
    tstring xml;
    tstring prefix;
    TCHAR str[2*MAX_PATH];

    for(int i = 0; i < depth; i++)
        prefix += _T("\t");
     
    // Output the filter name and type
    xml = prefix;
    xml += _T("<Filter>\r\n");
    
    prefix += _T("\t");
    _stprintf_s(str, _countof(str), _T("<Name>%s</Name>\r\n"), m_szWellKnownName);
    xml += prefix;
    xml += str;

    // Output type
    tstring typestr = TypeToString(m_type);
    _stprintf_s(str, _countof(str), _T("<Type>%s</Type>\r\n"), typestr.c_str());
    xml += prefix;
    xml += str;

    // Close the XML elements
    prefix.erase(prefix.end() - 1);
    xml += prefix;
    xml += _T("</Filter>\n");

    // Write the xml string to the file
    DWORD nBytesWritten = 0;
    WriteFile(hXmlFile, (const void*)xml.c_str(), xml.length()*sizeof(TCHAR), &nBytesWritten, NULL);

    return S_OK;
}

HRESULT FilterDesc::CheckInterface(IBaseFilter* pFilter, const IID& riid)
{
    IUnknown* pInterface = NULL;
    HRESULT hr = pFilter->QueryInterface(riid, (void**)&pInterface);
    if (SUCCEEDED(hr) && pInterface)
    {
        pInterface->Release();
        pInterface = NULL;
    }
    return hr;

}

void FilterDesc::DeletePinArray(IPin** ppPins, int nPins)
{
    if (ppPins)
    {
        for(int i = 0; i < nPins; i++)
        {
            if (ppPins[i])
                ppPins[i]->Release();
        }
        delete ppPins;
    }
}

const tstring FilterDesc::TypeToString(DWORD type)
{
    tstring typestr = _T("");
    if (Class(type) & Splitter)
        typestr += _T("Splitter");
    if (Class(type) & SourceFilter)
        typestr += _T("SourceFilter");
    if (Class(type) & Multiplexer)
        typestr += _T("Multiplexer");

#if 0
    if (Class(type) & Decoder)
        typestr += _T("Decoder:");
    if (Class(type) & Encoder)
        typestr += _T("Encoder:");
    if (Class(type) & Renderer)
        typestr += _T("Renderer:");
#endif

    if (SubClass(type) & VideoDecoder)
        typestr += _T("VideoDecoder");
    if (SubClass(type) & AudioDecoder)
        typestr += _T("AudioDecoder");
    if (SubClass(type) & VideoEncoder)
        typestr += _T("VideoEncoder");
    if (SubClass(type) & AudioEncoder)
        typestr += _T("AudioEncoder");
    if (SubClass(type) & VideoRenderer)
        typestr += _T("VideoRenderer");

    return typestr;
}

bool FilterDesc::IsSameFilter(FilterDesc* pFilter)
{
    return false;
}

bool FilterDesc::IsSameFilter(TCHAR* szWellKnownFilterName)
{
    return (!_tcsncmp(m_szWellKnownName, szWellKnownFilterName, countof(m_szWellKnownName)));
}

int FilterDesc::GetInputPinCount()
{
    int nInputPins = 0;
    GetPinArray(NULL, &nInputPins, PINDIR_INPUT);
    return nInputPins;
}

int FilterDesc::GetOutputPinCount()
{
    int nOutputPins = 0;
    GetPinArray(NULL, &nOutputPins, PINDIR_OUTPUT);
    return nOutputPins;
}
