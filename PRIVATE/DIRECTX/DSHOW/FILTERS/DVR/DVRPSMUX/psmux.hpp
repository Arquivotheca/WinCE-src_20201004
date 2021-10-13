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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#pragma once

#include <windows.h>
#include <wxdebug.h>
#include <auto_xxx.hxx>
#include "queue.hpp"
#include "commontypes.hpp"
#include "outputpin.hpp"
#include "parsers.hpp"


namespace DvrPsMux
{

// byte 0: whether or not we need to code a system
//         header in this pack
// byte 1: whether or not we need to code the
//         P-STD_buffer_scale and P-STD_buffer_size in
//         this pack's PES header
// byte 2: the stream type for this pack
// bytes 3 & 4: The length of pack data (including headers)
struct PackInfo_t
{
    BYTE NeedsSystemHeader;
    BYTE NeedsPstdBufferFields;
    BYTE StreamType;
    BYTE LengthHi;
    BYTE LengthLo;
};

struct SegmentInfo_t
{
    __int64 StartTime;
    __int64 StopTime;
    double  Rate;

    bool
    IsComplete()
    {
        return (
            StartTime >= 0 &&
            StopTime == 0x7FFFFFFFFFFFFFFF &&
            Rate == 1.0
            );
    }

    void
    Reset()
    {
        StartTime = -1;
        StopTime = -1;
        Rate = 0.0;
    }

    SegmentInfo_t()
    {
        Reset();
    }
};

struct OutputBufferEntry_t
{
    unsigned long Index;
    IMediaSample* pSample;
    SegmentInfo_t Segment;

    OutputBufferEntry_t() :
        pSample(NULL)
    {
    }
};

// allocators
typedef
    ce::free_list< 32, ce::fixed_block_allocator<4> >
    Fixed4FreeList32_t;


// queues
typedef
    ce::list<PictureInfoEntry_t, Fixed8FreeList1024_t> 
    MuxPictureInfoQueue_t;

typedef
    ce::list<AudioTimestampEntry_t, Fixed8FreeList1024_t>
    MuxAudioTimestampQueue_t;

typedef
    ce::list<OutputBufferEntry_t, Fixed4FreeList32_t>
    OutputBufferQueue_t;

//
// ------------- PackWriter_t -------------
//

class Mux_t;

class PackWriter_t :
    public IVideoParserWriter,
    public IAudioParserWriter
{
    BYTE*  m_pPack;
    long   m_PackOffset;
    long   m_HeaderSize;
    long   m_Pos;
    Mux_t* m_pMux;

    friend class Mux_t;

    bool
    IsComplete()
    {
        ASSERT(m_Pos <= f_PackSize);
        return (m_Pos >= f_PackSize);
    }

    bool
    IsPackSet()
    {
        ASSERT(m_Pos <= f_PackSize);

        return (
            m_pMux &&
            m_pPack &&
            m_HeaderSize &&
            m_Pos > m_HeaderSize
            );
    }

    long
    GetRemainingLength()
    {
        return (f_PackSize - m_Pos);
    }

    void
    Reset()
    {
        m_pPack = NULL;
        m_PackOffset = 0;
        m_HeaderSize = 0;
        ASSERT(m_Pos <= f_PackSize);
        m_Pos = f_PackSize;
    }

    PackWriter_t() :
        m_pPack(NULL),
        m_PackOffset(0),
        m_HeaderSize(0),
        m_Pos(f_PackSize),
        m_pMux(NULL)
    {
    }

    void
    SetMux(
        Mux_t* pMux
        )
    {
        ASSERT(pMux);
        m_pMux = pMux;
    }

    void
    SetPack(
        BYTE* pPack,
        long  Offset,
        long  Pos
        )
    {
        ASSERT(pPack && m_pMux);
        ASSERT(0 == (Offset % f_PackSize) );
        m_pPack = pPack;
        m_PackOffset = Offset;
        m_HeaderSize = Pos;
        m_Pos = Pos;

        DEBUGMSG(
            0/*ZONE_INFO_MAX*/,
            ( L"DvrPsMux::PackWriter_t::SetPack --> pack "
              L"(%#x), offset (%#x)\r\n",
              pPack, Offset )
            );
    }

public:

    HRESULT
    Write(
        BYTE* pSrc,
        long  Offset,
        long  Size,
        PictureInfoQueue_t* pQueue,
        long* pBytesWritten
        )
    {
        return WriteHelper<
            PictureInfoEntry_t,
            PictureInfoQueue_t
            >(pSrc, Offset, Size, pQueue, pBytesWritten);
    }

    HRESULT
    Write(
        BYTE* pSrc,
        long  Offset,
        long  Size,
        AudioTimestampQueue_t* pQueue,
        long* pBytesWritten
        )
    {
        return WriteHelper<
            AudioTimestampEntry_t,
            AudioTimestampQueue_t
            >(pSrc, Offset, Size, pQueue, pBytesWritten);
    }

    template<class Entry, class Queue>
    HRESULT
    WriteHelper(
        BYTE* pSrc,
        long  Offset,
        long  Size,
        Queue* pQueue,
        long* pBytesWritten
        );
};


//
// ----------------- Mux_t ----------------
//

struct SampleEntry_t
{
    StreamType_e Type;
    IMediaSample* pSample;
};

interface IMuxInput
{
    virtual
    bool
    Append(
        SampleEntry_t& Entry
        ) = 0;

    virtual
    HRESULT
    NewSegment(
        __int64 StartTime,
        __int64 StopTime,
        double  Rate
        ) = 0;
};

typedef
    SafeQueue_t<SampleEntry_t, Fixed8FreeList1024_t>
    InputBufferQueue_t;

inline
void
ReleaseSample(
    IMediaSample* pSample
    )
{
    if (pSample)
        {
        pSample->Release();
        }
}

class Filter_t;

class Mux_t :
    public IVideoParserManager,
    public IAudioParserManager,
    public IMuxInput
{
    enum TimestampType_e
        {
        PTS,
        DTS
        };

    typedef Parser_t<
        void (*)(IMediaSample*),
        IMediaSample*,
        ReleaseSample,
        NULL
        > SampleParser_t;

    typedef Mpeg2VideoParser_t<
        void (*)(IMediaSample*),
        IMediaSample*,
        ReleaseSample,
        NULL
        > Mpeg2VideoSampleParser_t;

    typedef Mpeg2AudioParser_t<
        void (*)(IMediaSample*),
        IMediaSample*,
        ReleaseSample,
        NULL
        > Mpeg2AudioSampleParser_t;

    typedef Ac3Parser_t<
        void (*)(IMediaSample*),
        IMediaSample*,
        ReleaseSample,
        NULL
        > Ac3SampleParser_t;

    PackWriter_t          m_VideoWriter;
    PackWriter_t          m_AudioWriter;
    SampleParser_t*       m_pVideoParser;
    SampleParser_t*       m_pAudioParser;
    HANDLE                m_MuxThread;
    HANDLE                m_ExitWorker;
    CBaseFilter*          m_pFilter;
    OutputPin_t*          m_pOutputPin;
    BYTE*                 m_pNextPack;
    long                  m_NextPackOffset;
    long                  m_ScrAdvance;
    bool                  m_FoundVideo;
    bool                  m_FoundAudio;
    bool                  m_Flush;
    bool                  m_SetDiscontinuity;
    bool                  m_Discontinuity;
    bool                  m_SyncPoint;
    bool                  m_AllowAudio;
    long                  m_RateBound;
    long                  m_ProgramMuxRate;
    long                  m_VideoStdBufferScale;
    long                  m_VideoStdBufferSize;
    long                  m_VideoStdBufferBoundScale;
    long                  m_VideoStdBufferSizeBound;
    long                  m_AudioStdBufferScale;
    long                  m_AudioStdBufferSize;
    long                  m_AudioStdBufferBoundScale;
    long                  m_AudioStdBufferSizeBound;
    StreamType_e          m_AudioType;
    StreamType_e          m_SetDiscontinuityType;
    __int64               m_StreamPosition;
    __int64               m_LastCodedVideoPts;
    __int64               m_Scr27Mhz;
    __int64               m_LastVideoDTS;
    SegmentInfo_t         m_Segment;    // Segment info to send before this sample
    SegmentInfo_t         m_SetSegment; // Segment info to send before next sample
    SegmentInfo_t         m_NewSegment; // Unassigned segment info
    InputBufferQueue_t       m_InputQueue;
    MuxPictureInfoQueue_t    m_PictureInfoQueue;
    MuxAudioTimestampQueue_t m_AudioPtsQueue;
    OutputBufferQueue_t      m_OutputBufferQueue;
    OutputBufferEntry_t      m_OutputEntry;

    const long       PtsCodingThreshold;
    const wchar_t*   RateBound;
    const wchar_t*   ProgramMuxRate;
    const wchar_t*   VideoStdBufferScale;
    const wchar_t*   VideoStdBufferSize;
    const wchar_t*   VideoStdBufferBoundScale;
    const wchar_t*   VideoStdBufferSizeBound;
    const wchar_t*   AudioStdBufferScale;
    const wchar_t*   AudioStdBufferSize;
    const wchar_t*   AudioStdBufferBoundScale;
    const wchar_t*   AudioStdBufferSizeBound;

#ifdef DEBUG
    bool m_CompletedFirstVideoPack;
#endif

    friend class PackWriter_t;

    static
    DWORD
    __cdecl
    MuxThreadProc(
      void* pArg
      );

    DWORD
    MuxWorker();

    bool
    OnNewAccessUnit(
        PictureInfoEntry_t& pEntry
        );

    bool
    OnNewAccessUnit(
        AudioTimestampEntry_t& Entry
        );

    bool
    AmmendTailEntry(
        PictureInfoEntry_t& Entry
        );

    bool
    AmmendTailEntry(
        AudioTimestampEntry_t &Entry
        )
    {
        // no need to ammend the tail for audio
        ASSERT(false);
        return false;
    }

    HRESULT
    OnWriterFinished();

    bool
    DeliverSample();

    bool
    CompleteBuffer(
        BYTE*         pBuffer,
        unsigned long Index,
        long          Length
        );

    bool
    CodePackHeader(
        BYTE* pStart
        );

    bool
    CodeSystemHeader(
        BYTE* pStart
        );

    bool
    CodePesHeader(
        StreamType_e StreamType,
        BYTE* pBuffer,
        unsigned long BufferIndex,
        long HeaderOffset,
        bool CodeExtension
        );

    bool
    UpdatePesPacketLength(
        BYTE* pPesHeaderStart,
        long PacketLengthAdjust
        );

    bool
    UpdatePesHeaderDataLength(
        BYTE* pPesHeaderStart,
        long DataLengthAdjust
        );

    bool
    CodePaddingPacket(
        BYTE* pStart,
        long Length
        );

    bool
    CrossedPtsCodingThreshold(
        __int64 Timestamp
        )
    {
        return Timestamp >
            (m_LastCodedVideoPts + PtsCodingThreshold);
    }

    bool
    CodeTimestamp(
        BYTE* pTimestampLoc,
        __int64 Timestamp,
        TimestampType_e PtsDts
        );

    void
    OnDiscontinuity(
        StreamType_e StreamType
        );

    HRESULT
    GetBuffer();

public:
    Mux_t(
        CBaseFilter *pFilter,
        OutputPin_t* pOut
        ) :
        PtsCodingThreshold(6000000),
        RateBound(L"rate_bound"),
        ProgramMuxRate(L"program_mux_rate"),
        VideoStdBufferScale(L"Video P-STD_buffer_scale"),
        VideoStdBufferSize(L"Video P-STD_buffer_size"),
        VideoStdBufferBoundScale(L"Video P-STD_buffer_bound_scale"),
        VideoStdBufferSizeBound(L"Video P-STD_buffer_size_bound"),
        AudioStdBufferScale(L"Audio P-STD_buffer_scale"),
        AudioStdBufferSize(L"Audio P-STD_buffer_size"),
        AudioStdBufferBoundScale(L"Audio P-STD_buffer_bound_scale"),
        AudioStdBufferSizeBound(L"Audio P-STD_buffer_size_bound"),
        m_pVideoParser(NULL),
        m_pAudioParser(NULL),
        m_ExitWorker( CreateEvent(NULL, TRUE, FALSE, NULL) ),
        m_MuxThread(NULL),
        m_pFilter(pFilter),
        m_pOutputPin(pOut),
        m_pNextPack(NULL),
        m_NextPackOffset(0),
        m_ScrAdvance(-1),
        m_FoundVideo(false),
        m_FoundAudio(false),
        m_Flush(false),
        m_SetDiscontinuity(false),
        m_Discontinuity(false),
        m_SyncPoint(false),
        m_AllowAudio(false),
        m_RateBound(25200),             // DVD default
        m_ProgramMuxRate(25200),        // DVD default
        m_VideoStdBufferScale(1),       // DVD default
        m_VideoStdBufferSize(232),      // DVD default
        m_VideoStdBufferBoundScale(1),  // DVD default
        m_VideoStdBufferSizeBound(232), // DVD default
        m_AudioStdBufferScale(1),       // DVD default (AC-3)
        m_AudioStdBufferSize(58),       // DVD default (AC-3)
        m_AudioStdBufferBoundScale(1),  // DVD default (AC-3)
        m_AudioStdBufferSizeBound(58),  // DVD default (AC-3)
        m_AudioType(Ac3Audio),
        m_SetDiscontinuityType(Mpeg2Video),
        m_StreamPosition(0),
        m_LastCodedVideoPts(0),
        m_Scr27Mhz(-1),
        m_LastVideoDTS(-1)
    {
        ASSERT(pOut);
        m_VideoWriter.SetMux(this);
        m_AudioWriter.SetMux(this);
    }

    ~Mux_t()
    {
        if (m_MuxThread)
            {
            BOOL Succeeded = SetEvent(m_ExitWorker);
            ASSERT(Succeeded);
            DWORD Wait = WaitForSingleObject(m_MuxThread, 5000);
            ASSERT(WAIT_OBJECT_0 == Wait);
            CloseHandle(m_MuxThread);
            }

        if(m_pVideoParser)
            {
            delete m_pVideoParser;
            m_pVideoParser = NULL;
            }

        if(m_pAudioParser)
            {
            delete m_pAudioParser;
            m_pAudioParser = NULL;
            }

        CloseHandle(m_ExitWorker);
    }

    HRESULT
    GetWriter(
        IVideoParserWriter** ppWriter
        );

    HRESULT
    GetWriter(
        IAudioParserWriter** ppWriter
        );

    HRESULT
    GetWriterHelper(
        StreamType_e StreamType,
        PackWriter_t** ppWriter
        );

    HRESULT
    Flush();

    bool
    IsOutputBufferEmpty();

    void
    Notify(
        ParserManagerEvent_e Event
        );

    bool
    Append(
        SampleEntry_t& Entry
        );

    HRESULT
    NewSegment(
        __int64 StartTime,
        __int64 StopTime,
        double  Rate
        );

    HRESULT
    Discontinuity(
        StreamType_e StreamType
        );

    bool
    SetAudioType(
        StreamType_e StreamType
        );

    bool
    OnStart(
        const __int64& StartTime
        );

    HRESULT
    OnStop();

    HRESULT
    GetProperty(
        LPCOLESTR Name,
        VARIANT* pValue
        );

    HRESULT
    SetProperty(
        LPCOLESTR Name,
        VARIANT* pValue
        );

};

}

