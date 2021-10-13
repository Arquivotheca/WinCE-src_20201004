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

#ifndef FILTER_DESC_H
#define FILTER_DESC_H

#include <dshow.h>
#include <tchar.h>
#include <stdlib.h>
#include <vector>
#include <string>

// MAX_FILTERS is the maximum number of filters which the test can map.  This value 
// needs to be incremented if the number of filters listed below in 'enum FilterId'
// grows beyond 64
enum Constants {
    MAX_FILTERS = 64,
    FILTER_NAME_LENGTH = 256
};


typedef std::basic_string<_TCHAR> tstring;

// Filter classes - that we are aware of and understand
// These may need to be extended as needed or as the WinCE DShow code evolves
#define    Splitter        1
#define SourceFilter    2
#define Multiplexer        4
#define Decoder            8
#define Encoder            16
#define TransformFilter        32
#define Renderer        64
#define Dispatcher        128
#define WriterFilter    256
#define BufferingFilter 512

// Sub classes
#define    VideoDecoder        1
#define    AudioDecoder        2
#define    VideoEncoder        4
#define    AudioEncoder        8
#define    AudioRenderer        16
#define VideoRenderer        32
#define    VideoTransform        64
#define    AudioTransform        128
#define VideoDispatcher        256
#define AudioDispatcher        512
#define MediaObject            1024

    
// Macros to extract the class, subclass
#define Class(x) ((x >> 16) & 0xff)
#define SubClass(x) (x & 0xffff)
#define ToSubClass(x) (x & 0xffff)
#define ToClass(x) ((x & 0xff) << 16)

#define FilterType(x,y) (ToClass(x) | ToSubClass(y))

// Macros to determine the type of filter
#define IsVideoCodec(x) (SubClass(x) & VideoDecoder) || (SubClass(x) & VideoEncoder)
#define IsAudioCodec(x) (SubClass(x) & AudioDecoder) || (SubClass(x) & AudioEncoder)
#define IsEncoder(x) ((SubClass(x) & VideoEncoder) || (SubClass(x) & AudioEncoder))
#define IsSplitter(x) (Class(x) & Splitter)
#define IsSourceFilter(x) (Class(x) & SourceFilter)
#define IsVideoDecoder(x) (SubClass(x) & VideoDecoder)
#define IsAudioDecoder(x) (SubClass(x) & AudioDecoder)



// Specific filters that we know about
enum FilterId {
    // Dispatcher Filter
    ASFICMHandler,
    ASFACMHandler,

    // Source filters
    WMSourceFilter,
    AsyncReader,
    UrlReader,
    WMAsfReaderFilter,

    //Buffering filters
    BufferingStreamingFilter,

    // Splitter/Parser filters
    AVISplitter,
    MPEG1StreamSplitter,
    MPEG2Demux,
    ElecardMPEG2Demux,
    WaveParser,
    InfiniteTee,
    MPEG4Demux,

    // Decoders
    ACMWrapper,
    WMVVideoDecoderDMO,
    MP43VideoDecoderDMO,
    WMAAudioDecoderDMO,
    MPEGVideoDecoder,
    
    MPEG1VideoDecoder,
    MPEG2VideoDecoder,
    MPEGAudioDecoder,
    MPEG2AudioDecoder,
    MP13AudioDecoderDMO,
    MP13AudioDecoder,
    AVIDecoder,
    SigmaDDecoderFilter,
    iMX27DecoderFilter,

    MPEG2VideoEncoder,
    AVIEncoder,
    
    // Renderers
    DefaultAudioRenderer,
    DefaultDSoundDev,
    DefaultVideoRenderer,
    VideoMixingRenderer,
        ScriptCmdRenderer,

    // Video Transforms
    ColorConverter,
    VideoStampWriterFilter,
    VideoStampReaderFilter,

    // Mux
    AVIMuxFilter,
    MPEGMuxFilter,
    WMAsfWriterFilter,

    // Overlay Mixer
    OverlayMixer,
    OverlayMixer2,        // only support VIDEOINFO2

    FileWriterFilter,
    ElecardFileWriterFilter,

    // MIDI Filter
    MIDIParser,
    MIDISynthesizer,

    UnknownFilter
};

// Names of the filters
// Dispatcher filters
#define STR_ASFICMHandler            _T("ASF ICM Handler")
#define STR_ASFACMHandler            _T("ASF ACM Handler")

// Source filters
#define STR_WMSourceFilter            _T("WM Source Filter")
#define STR_AsyncReader                _T("Async Reader")
#define STR_UrlReader                _T("Url Reader")
#define STR_WMAsfReader            _T("WM ASF Reader Filter")

#define STR_BufferingStreamingFilter    _T("Buffering Streaming Filter") 
// Splitter/Parser filters
#define STR_MPEG1StreamSplitter        _T("MPEG-I Stream Splitter")
#define STR_AVISplitter                _T("AVI Splitter")
#define STR_MPEG2Demux                _T("MPEG-2 Stream Demultiplexer")
#define STR_ElecardMPEG2Demux                _T("Moonlight-Elecard MPEG2 Demultiplexer")
#define STR_MainConceptMPEG2Splitter _T("MainConcept (AdobeEncore) MPEG Splitter")
#define STR_InfiniteTee             _T("Infinite Tee")
#define STR_MPEG4Demux              _T("MPEG-4 Demux Filter")

// Transform filters
#define STR_ACMWrapper                _T("ACM Wrapper")
#define STR_ColorConverter            _T("Color Space Converter")
#define STR_VideoStampWriter        _T("Video Stamp Writer")
#define STR_VideoStampReader        _T("Video Stamp Reader")

// Decoder filters
#define STR_WaveParser                    _T("Wave Parser")
#define STR_WMAAudioDecoderDMO            _T("WMAudio Decoder DMO")
#define STR_WMVVideoDecoderDMO            _T("WMVideo Decoder DMO")
#define STR_WMVVideoMPEG4DecoderDMO        _T("WMVideo & MPEG4 Decoder DMO")
#define STR_MP43VideoDecoderDMO            _T("Mpeg43 Decoder DMO")
#define STR_MPEGVideoCodec                _T("MPEG Video Codec")
#define STR_MPEGVideoDecoder            _T("MPEG Video Decoder")
#define STR_MPEG1Layer3AudioDecoderDMO         _T("MPEG-1 Layer 3 Decoder DMO")
#define STR_SigmaDDecoderFilter            _T("Sigma Designs Decoder Filter")
#define STR_iMX27VPUDecoderFilter           _T("MX27VPUDEC Filter")

// Add MPEG-1 Layer 3 decoder filter
#define STR_MPEG1Layer3AudioDecoder        _T("MPEG-1 Layer-3 Decoder")
#define STR_MPEGLayer3AudioDecoder        _T("MPEG Layer-3 Decoder")
#define STR_MPEGAudioDecoder            _T("MPEG Audio Decoder")
#define STR_MPEGAudioCodec                _T("MPEG Audio Codec")
#define STR_AVIDecoder                    _T("AVI Decompressor")
#define STR_MainConceptMPEG2VideoDecoder _T("MainConcept (AdobeEncore) MPEG Video Decoder")
#define STR_MainConceptMPEG2AudioDecoder _T("MainConcept (AdobeEncore) MPEG Audio Decoder")

#define STR_ElecardMPEG2VideoDecoder _T("Elecard MPEG-2 Video Decoder")
#define STR_ElecardMPEG2VideoEncoder _T("Elecard Mpeg Video Encoder")
#define STR_AVIEncoder _T("AVI Video Encoder")

// Renderer filters
#define STR_DefaultAudioRenderer        _T("Audio Renderer")
#define STR_DefaultDSoundDev            _T("Default DirectSound Device")
#define STR_DefaultVideoRenderer        _T("Video Renderer")
#define STR_VideoMixingRenderer            _T("VideoMixingRenderer")
#define STR_ScriptCmdRenderer           _T("Internal Script Command Renderer")

// Mux Filters
#define STR_AVIMux                        _T("AVI Multiplexer")
#define STR_MPEGMux                        _T("Elecard Multiplexer")
#define STR_WMAsfWriter                        _T("WMASF writer")

// Overlay Mixer
#define STR_OVMixer                        _T("Overlay Mixer")
#define STR_OVMixer2                    _T("Overlay Mixer 2")

#define STR_FileWriter                    _T("File Writer Filter")
#define STR_ElecardFileWriter            _T("ElecardSinkFilter")

// MIDI Filters
#define STR_MIDIParser                          _T("MIDI Parser")
#define STR_MIDISynthesizer                 _T("MIDI Synthesizer")


// Classes of the filters
// Dispatcher filters
#define ASFICMHandler_TYPE            (ToClass(Dispatcher) | ToSubClass(VideoDispatcher))
#define ASFACMHandler_TYPE            (ToClass(Dispatcher) | ToSubClass(AudioDispatcher))

// Source filters
#define WMSourceFilter_TYPE            (ToClass(Splitter) | ToClass(SourceFilter))
#define WMAsfReaderFilter_TYPE            (ToClass(Splitter) | ToClass(SourceFilter))
#define AsyncReader_TYPE            (ToClass(SourceFilter))
#define UrlReader_TYPE                (ToClass(SourceFilter))
#define MIDIParser_TYPE             (ToClass(SourceFilter))
#define ColorFilter_TYPE            (ToClass(TransformFilter))
#define VideoStampWriter_TYPE            (ToClass(TransformFilter))
#define VideoStampReader_TYPE            (ToClass(TransformFilter))

//Buffering filters
#define BufferingStreamingFilter_TYPE    (ToClass(BufferingFilter))

// Splitter filters
#define MPEG1StreamSplitter_TYPE    (ToClass(Splitter))
#define AVISplitter_TYPE (ToClass(Splitter))
#define MPEG2Demux_TYPE                (ToClass(Splitter))
#define InfiniteTee_TYPE                (ToClass(Splitter))
#define MPEG4Demux_TYPE              (ToClass(Splitter))

// Renderer filters
#define DefaultAudioRenderer_TYPE        (ToClass(Renderer) | ToSubClass(AudioRenderer))
#define DefaultDSoundDev_TYPE            (ToClass(Renderer) | ToSubClass(AudioRenderer))
#define VideoMixingRenderer_TYPE        (ToClass(Renderer) | ToSubClass(VideoMixingRenderer))
#define ScriptCmdRenderer_TYPE          ToClass(Renderer)

#define OverlayMixer_TYPE            (ToClass(Renderer) | ToSubClass(VideoRenderer))

// Decoder filters
#define WaveParser_TYPE                (ToClass(Decoder) | ToSubClass(AudioDecoder))
#define ACEWrapper_TYPE                (ToClass(Decoder) | ToSubClass(AudioDecoder))
#define WMAAudioDecoderDMO_TYPE        (ToClass(Decoder) | ToSubClass(AudioDecoder) | ToSubClass(MediaObject))
#define WMVVideoDecoderDMO_TYPE        (ToClass(Decoder) | ToSubClass(VideoDecoder) | ToSubClass(MediaObject))
#define MP43VideoDecoderDMO_TYPE    (ToClass(Decoder) | ToSubClass(VideoDecoder) | ToSubClass(MediaObject))
#define MPEGVideoDecoder_TYPE        (ToClass(Decoder) | ToSubClass(VideoDecoder))
#define MPEGAudioDecoder_TYPE        (ToClass(Decoder) | ToSubClass(AudioDecoder))
#define MP13AudioDecoder_TYPE       (ToClass(Decoder) | ToSubClass(AudioDecoder))
#define AVIDecoder_TYPE                (ToClass(Decoder) | ToSubClass(VideoDecoder))
#define MPEG2VideoDecoder_TYPE        (ToClass(Decoder) | ToSubClass(VideoDecoder))
#define MPEG2AudioDecoder_TYPE        (ToClass(Decoder) | ToSubClass(AudioDecoder))
#define MIDISynthesizer_TYPE                (ToClass(Decoder) | ToSubClass(AudioDecoder))
#define SigmaDDecoderFilter_TYPE      (ToClass(Decoder) | ToSubClass(AudioDecoder) | ToClass(Renderer))
#define iMX27VPUDecoderFilter_TYPE      (ToClass(Decoder) | ToSubClass(VideoDecoder))

// encoder filters
#define MPEG2VideoEncoder_TYPE        (ToClass(Encoder) | ToSubClass(VideoEncoder))
#define AVIEncoder_TYPE                (ToClass(Encoder) | ToSubClass(VideoEncoder))

// Color Converters
#define ColorConverter_TYPE            (ToClass(TransformFilter) | ToSubClass(VideoTransform))

// Multiplexer
#define AVIMuxFilter_TYPE            (ToClass(Multiplexer))
#define MPEGMuxFilter_TYPE            (ToClass(Multiplexer))
#define WMAsfWriterFilter_TYPE            (ToClass(Multiplexer) | ToClass(WriterFilter))

#define FileWriterFilter_TYPE        (ToClass(WriterFilter))

#define ICMDISP TEXT("ICMDISP")
#define ACMDISP TEXT("ACMDISP")
#define NSSOURCE TEXT("NSSOURCE")
#define ASYNCRDR TEXT("ASYNCRDR")
#define WMASFREADER TEXT("WMASFREADER")
#define URLRDR TEXT("URLRDR")
#define WMADEC TEXT("WMADEC")
#define WMVDEC TEXT("WMVDEC")
#define MPGDEC TEXT("MPGDEC")
#define MP43DEC TEXT("MP43DEC")
#define MP3DEC TEXT("MP3DEC")
#define MP13DECDMO TEXT("MP13DECDMO")
#define MP13DEC TEXT("MP13DEC")
#define MPGSPLIT TEXT("MPGSPLIT")
#define WAVPAR TEXT("WAVPAR")
#define ACMWRAP TEXT("ACMWRAP")
#define AUDREND TEXT("AUDREND")
#define VIDREND TEXT("VIDREND")
#define VMRREND TEXT("VMRREND")
#define SCMDREND TEXT("SCMDREND")
#define CLRCONV TEXT("CLRCONV")
#define VSWRITE TEXT("VSWRITE")
#define VSREAD TEXT("VSREAD")
#define AVISPLIT TEXT("AVISPLIT")
#define AVIDEC TEXT("AVIDEC")
#define AVIENC TEXT("AVIENC")
#define AVIMUX TEXT("AVIMUX")
#define WMASFWRITER TEXT("WMASFWRITER")
#define MPEGMUX TEXT("MPEGMUX")
#define MPEG2DMUX TEXT("MPEG2DMUX")
#define INFTEE TEXT("INFTEE")
#define FILEWR TEXT("FILEWR")
#define MP2VDEC TEXT("MP2VDEC")
#define MP2ADEC TEXT("MP2ADEC")
#define MP2VENC TEXT("MP2VENC")
#define MP2AENC TEXT("MP2AENC")
#define OVMIXER TEXT("OVMIXER")
#define MIDIPSR TEXT("MIDIPSR")
#define MIDISYN TEXT("MIDISYN")
#define MP4MUX TEXT("MP4MUX")
#define SIGMAD TEXT("SIGMAD")
#define BUFFSTRM TEXT("BUFFSTRM")
#define MX27VPU TEXT("MX27VPU")

class FilterInfo
{
public:
    // the name of the filter in the graph
    TCHAR* szName;

    // the well known name such as "AVIMUX"
    TCHAR* szWellKnownName;

    // The id of the filter
    FilterId id;

    // The type
    DWORD type;

    // GUID
    GUID guid;

public:
    FilterInfo();
    FilterInfo(TCHAR* szName, TCHAR* szWellKnownName, FilterId id, DWORD type, GUID guid);
    ~FilterInfo();
    HRESULT Init(TCHAR* szName, TCHAR* szWellKnownName, FilterId id, DWORD type, GUID guid);
};

// This class helps maintain a map from filter names to their id, type and guid
class FilterInfoMapper
{
public:
    FilterInfoMapper();
    ~FilterInfoMapper();

    static FilterInfo *map[MAX_FILTERS];
    static int nFilters;

    // From the name determine the filter's info
    static HRESULT GetFilterInfo(const TCHAR* name, FilterInfo* pFilterInfo);
    static HRESULT GetFilterInfo(FilterId id, FilterInfo* pFilterInfo);
};


// FilterDesc encapsulates information about filters and contains
class FilterDesc
{
public:
    static void DeletePinArray(IPin** ppPins, int nPins);
    static const tstring TypeToString(DWORD type);

    FilterDesc();

    FilterDesc(FilterId id);

    // Constructor: If the filter has been already added to the graph and it's name/type have to be figured out
    FilterDesc(IBaseFilter* pBaseFilter, TCHAR* szUrl, HRESULT* phr);

    ~FilterDesc();

    // Create the filter - the filter is not added to any graph
    HRESULT CreateFilterInstance();

    // Get the filter instance
    IBaseFilter* GetFilterInstance();

    // Set the numeric count for this filter - this only changes the filter name
    void SetCount(int count);

    // Save the filter info to XML
    HRESULT SaveToXml(HANDLE hFile, int depth);
    
    // Get the specific filter id
    FilterId GetId();

    // Get the type of the filter
    DWORD GetType();

    // Get the name of the filter
    const TCHAR* GetName();

    // Set the name 
    void SetName(const TCHAR* szFilterName);

    // Get the number of input pins
    int GetInputPinCount();

    // Get the number of output pins
    int GetOutputPinCount();
    
    // Get a pin given index and direction
    IPin* GetPin(int i, PIN_DIRECTION dir);

    // Get the pin index given a pointer to the pin
    HRESULT GetPinIndex(IPin* pPin, int* pIndex);

    // Get the pin array or pin count for a specific direction
    HRESULT GetPinArray(IPin*** pppPins, int *pNumPins, PIN_DIRECTION pinDir);

    // Get the connect filter array or filter count for a specific direction
    HRESULT GetConnectedFilterArray(IBaseFilter*** pppFilters, int *pNumFilters, PIN_DIRECTION pinDir);

    // Is the specified filter same as this one?
    bool IsSameFilter(FilterDesc* pFilter);
    bool IsSameFilter(TCHAR* szFilterName);

    HRESULT AddToGraph(IGraphBuilder* pGraph);

private:
    HRESULT CheckInterface(IBaseFilter* pFilter, const IID& riid);
    HRESULT GetSourceFilterInfo();
    HRESULT GetFilterInfo();
    void Init();

public:
    // The current url - we need this for source filters
    TCHAR m_szCurrUrl[MAX_PATH];

    // The name of the filter
    TCHAR m_szFilterName[MAX_PATH];

    // The well known name
    TCHAR m_szWellKnownName[MAX_FILTER_NAME];

    // The id of the filter
    FilterId m_id;

    // The filter type
    DWORD m_type;

    // The filter guid
    GUID m_guid;
    
    // The pointer to the filter
    IBaseFilter* m_pFilter;

    IGraphBuilder* m_pGraph;
};


typedef std::vector<FilterDesc*> FilterDescList;

#endif

#if 0
// FilterList is a list of filters
class FilterDescList
{
private:
    vector<FilterDesc*> m_FilterArray;

public:
    FilterDescList();
    ~FilterDescList();

    // Get the number of filters in the list
    int GetNumFilters();

    // Get filter given name
    FilterDesc* GetFilter(const TCHAR* szFilterName);

    // Get filter i
    FilterDesc* GetFilter(int i);


    // Add a filter
    HRESULT AddFilter(FilterDesc* pFilter);

    // Get the number of instances of this specific filter
    int GetNumFilterInstances(TCHAR* szFilterName);
};
#endif
