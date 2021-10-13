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
#include <list.hxx>
#include "commontypes.hpp"


namespace DvrPsMux
{

template <class T>
T
ExtractBits(
    T source,
    const int nMsb,
    const int nLsb)
{
    ASSERT(nMsb - nLsb >= 0);
    ASSERT(nMsb >= 0 && nLsb >=0);
    ASSERT(nMsb < sizeof(T)*8 && nLsb < sizeof(T)*8);

    if (nMsb == sizeof(T)*8 - 1)
        {
        return source >> nLsb;
        }
    else
        {
        T mask = (1 << (nMsb+1)) - 1;
        return (source & mask) >> nLsb;
        }
}


//
// --------------- Parser_t ---------------
//

// This class and its derivatives are templated so you can
// easily pull the parser classes out of the mux and drop them
// into stand-alone unit-test apps.
template<
    class     Prototype,    // function prototype for Release callback
    class     ArgType,      // type of Release callback parameter
    Prototype FnRelease,    // callback pointer
    ArgType   ArgNullVal    // "null" value for callback param --
    >                       // -- used to conditionalize callback
class Parser_t
{
protected:
    struct Buffer_t
    {
        BYTE*   pData;
        long    Size;
        ArgType ReleaseArg;
    };

    struct Marker_t
    {
        Buffer_t* pBuffer;
        long Position;

        Marker_t()
        {
            Reset();
        }

        void
        Reset()
        {
            pBuffer = NULL;
            Position = 0;
        }
    };

    typedef ce::list< Buffer_t, ce::free_list<2> > List_t;

    typedef Parser_t<
        Prototype,
        ArgType,
        FnRelease,
        ArgNullVal
        > BaseParser_t;

    List_t   m_BufferList;
    Marker_t m_Parse;
    Marker_t m_Write;
    __int64  m_Timestamp;
    bool     m_NeedsTimestamp;

    virtual
    bool
    DataAvailableAt(
        long Offset,
        long Length = 0
        ) const
    {
        ASSERT(Offset >= 0);
        ASSERT(m_Parse.pBuffer == &m_BufferList.back());

        long Limit = m_Parse.Position + Offset + Length;
        return (Limit <= m_Parse.pBuffer->Size);
    }

    virtual
    BYTE
    GetByteAt(
        long Offset
        )
    {
        ASSERT(Offset >= 0);
        ASSERT(m_Parse.pBuffer == &m_BufferList.back());

        BYTE* pData = m_Parse.pBuffer->pData;
        return pData[m_Parse.Position + Offset];
    }

    long
    GetRemainingLength()
    {
        ASSERT(m_Parse.pBuffer == &m_BufferList.back());
        ASSERT(m_Parse.Position <= m_Parse.pBuffer->Size);

        return (m_Parse.pBuffer->Size - m_Parse.Position);
    }

    void
    SetParsePosition(
        long Pos
        )
    {
        ASSERT(m_Parse.pBuffer == &m_BufferList.back());

        if (Pos > m_Parse.pBuffer->Size)
            {
            Pos = m_Parse.pBuffer->Size;
            }

        m_Parse.Position = Pos;

        // If the write and parse markers are in the same buffer
        // then the write marker should always come before the
        // parser marker (or they should be equivalent).
        // If the write and parse markers are in different
        // buffers then the write marker should be in the head
        // and the parse marker should be in the tail (we treat
        // this list like a queue).
        ASSERT( ( m_Write.pBuffer == m_Parse.pBuffer &&
                  m_Write.Position <= m_Parse.Position ) ||
                ( m_BufferList.size() > 1 ) );
    }

    void
    AdvanceParsePosition(
        long Length
        )
    {
        SetParsePosition(m_Parse.Position + Length);
    }

    void
    SetWritePosition(
        long Pos
        )
    {
        ASSERT(m_Write.pBuffer == &m_BufferList.front());
        ASSERT(m_Parse.pBuffer == &m_BufferList.back());

        m_Write.Position = Pos;

        // If the write and parse markers are in the same buffer
        // then the write marker should always come before the
        // parser marker (or they should be equivalent).
        // If the write and parse markers are in different
        // buffers then the write marker should be in the head
        // and the parse marker should be in the tail (we treat
        // this list like a queue).
        ASSERT( ( m_Write.pBuffer == m_Parse.pBuffer &&
                  m_Write.Position <= m_Parse.Position ) ||
                ( m_BufferList.size() > 1 ) );
    }

    void
    AdvanceWritePosition(
        long Length
        )
    {
        SetWritePosition(m_Write.Position + Length);
    }

    void
    ReleaseAndPopFrontBuffer()
    {
        Buffer_t* pBuffer = &m_BufferList.front();
        
        if (pBuffer->ReleaseArg != ArgNullVal)
            {
            (*FnRelease)(pBuffer->ReleaseArg);
            }

        pBuffer->pData = NULL;
        pBuffer->Size = 0;
        pBuffer->ReleaseArg = ArgNullVal;
        pBuffer = NULL;

        m_BufferList.pop_front();
    }

public:
    virtual
    void
    Reset()
    {
        m_Timestamp = -1;
        m_NeedsTimestamp = true;

        while (m_BufferList.size() > 0)
            {
            ReleaseAndPopFrontBuffer();
            }

        m_Parse.Reset();
        m_Write.Reset();
    }

    Parser_t()
    {
        Reset();
    }

    virtual
    ~Parser_t()
    {
        Reset();
    }

    virtual
    HRESULT
    SetBuffer(
        BYTE* pData,
        long Size,
        ArgType ReleaseArg,
        __int64 Timestamp,
        bool Discontinuity
        )
    {
        if (!pData)
            {
            ASSERT(false);
            return E_POINTER;
            }

        if (!Size)
            {
            ASSERT(false);
            return E_INVALIDARG;
            }

        Buffer_t Buffer = { pData, Size, ReleaseArg };
        if ( !m_BufferList.push_back(Buffer) )
            {
            return E_OUTOFMEMORY;
            }

        m_Parse.pBuffer = &m_BufferList.back();

        if (!m_Write.pBuffer)
            {
            m_Write.pBuffer = m_Parse.pBuffer;
            SetWritePosition(0);
            }

        SetParsePosition(0);

        if (Timestamp >= 0)
            {
            m_Timestamp = Timestamp;
            }

        // how to handle discontinuity is determined by the
        // derived class

        return S_OK;
    }

    virtual
    HRESULT
    Process()
    {
        if ( 0 == m_BufferList.size() )
            {
            ASSERT(false);
            return E_UNEXPECTED;
            }

        if ( m_NeedsTimestamp && (m_Timestamp < 0) )
            {
            return S_FALSE;
            }

        return S_OK;
    }
};


//
// ---------- Mpeg2VideoParser_t ----------
//

template<
    class Prototype,
    class ArgType,
    Prototype FnRelease,
    ArgType ArgNullVal
    >
class Mpeg2VideoParser_t :
    public Parser_t<Prototype, ArgType, FnRelease, ArgNullVal>
{
private:
    struct PictureInfoEntryEx_t
        {
        long TemporalReference;
        long DisplayPeriod;
        Buffer_t* pBuffer;
        PictureInfoEntry_t Info;
        };

    enum StartCodeValues_e
        {
        picture_start_code   = 0x00,
        sequence_header_code = 0xB3,
        extension_start_code = 0xB5,
        group_start_code     = 0xB8,
        };

    enum Offsets_e
        {
        start_code_prefix   = 0,
        start_code_value    = 3,
        extension_start_code_identifier = 4,
        temporal_reference  = 4,
        picture_coding_type = 5,
        picture_structure   = 6,
        frame_rate_code     = 7,
        closed_gop          = 7
        };

    enum RestorePoint_e
        {
        RpNone,
        RpStartCodeValue,
        RpTemporalReference1,
        RpTemporalReference2,
        RpPictureCodingType,
        RpFrameRateCode,
        RpExtensionStartCodeIdentifier,
        RpPictureStructure,
        RpClosedGop
        };

    enum FrameRate_e
        {
        Rate_23_967 = 1,
        Rate_24,
        Rate_25,
        Rate_29_97,
        Rate_30,
        Rate_50,
        Rate_59_94,
        Rate_60
        };

    enum PictureStructure_e
        {
        TopField = 1,
        BottomField,
        FramePicture
        };

    enum PictureState_e
        {
        NoPicture,
        DetectedPicture,
        ParsedField,
        ParsedFrame,
        CompletedFrame
        };

    enum ParseOrder_e
        {
        SequenceHeader,
        GopHeader,
        PictureHeader,
        PictureCodingExtensionHeader
        };

    const long StartCodeSize;
    const long PictureCodingExtensionId;
    const bool m_ExpectIframeTimestamps;

    RestorePoint_e       m_RestorePoint;
    FrameRate_e          m_FrameRate;
    PictureState_e       m_IPictureState;
    ParseOrder_e         m_ParseOrder;

    // The startcode marker guards a split start code by holding
    // onto the previous buffer (where the start code begins)
    // until we can complete the start code in the next buffer.
    // This marker is set when we detect a split, and cleared
    // immediately after we parse the start code value.
    // 
    // The sync marker guards a split in the access unit headers
    // by holding onto the previous buffer (where the first AU
    // header was detected) until we detect the picture start
    // code. Access unit headers include everything between a
    // sequence_header_code (or group_start_code if there's no
    // sequence header for the AU, or just the
    // picture_start_code if there's no sequence or GOP header)
    // and the picture_start_code. We use this to determine the
    // proper access unit boundary (see ISO/IEC 13818-1 2.1.1)
    // so we can output each I-frame into it's own sync-point
    // output buffer. The sync marker is set when it detects
    // the first start code in an access unit, and it is cleared
    // when the I-picture is detected. It can also be cleared
    // if the parser determines the access unit is not an
    // I-picture.
    //
    // The picture marker guards a split in the picture data
    // we need to generate a "picture info entry". These entries
    // are placed in a queue and passed to the parser manager
    // so it has timestamp and offset information about each
    // picture in the bitstream. The marker guards a split by
    // holding the previous buffer (where the picture header
    // began) until we parse the picture_coding_extension
    // header. This marker is set when we detect a
    // picture_start_code and released when we append the
    // picture info entry to the queue.
    Marker_t             m_StartCode;
    Marker_t             m_Sync;
    Marker_t             m_Picture;

    long                 m_LastStartCodeOffset;
    DWORD                m_AccumCode;
    long                 m_ReferencePictureCount;
    long                 m_DependentPictureCount;
    short                m_TemporalReference;
    bool                 m_FoundFirstSequence;
    bool                 m_FoundFirstGop;
    bool                 m_HadStartCodeMarker;
    PictureInfoQueue_t   m_PictureInfoQueue;
    PictureInfoEntryEx_t m_PrevPicture;
    PictureInfoEntryEx_t m_CurPicture;
    IVideoParserManager* m_pManager;

    __int64 m_LastIPictureTimestamp;

    bool
    DataAvailableAt(
        long Offset,
        long Length = 0
        ) const
    {
        Offset -= m_LastStartCodeOffset;
        ASSERT(Offset >= 0);

        return BaseParser_t::DataAvailableAt(Offset, Length);
    }

    BYTE
    GetByteAt(
        long Offset
        )
    {
        Offset -= m_LastStartCodeOffset;
        ASSERT(Offset >= 0);

        return BaseParser_t::GetByteAt(Offset);
    }

    void
    AdvanceParsePosition(
        long Length
        )
    {
        Length -= m_LastStartCodeOffset;
        ASSERT(Length >= 0);

        BaseParser_t::AdvanceParsePosition(Length);
    }

    HRESULT
    AdvanceToStartCode()
    {
        BYTE* pData = m_Parse.pBuffer->pData;
        long Pos = m_Parse.Position;
        ASSERT(pData && m_BufferList.back().pData == pData);

        BYTE *p = &pData[Pos];
        BYTE *q = &pData[m_Parse.pBuffer->Size];
        DWORD *q32 = (DWORD *)((LONG)q & ~0x3);
        DWORD AccumCode = m_AccumCode;

        while (p < q)
            {
            AccumCode <<= 8;
            AccumCode |= 0xff000000 | *p;
            if (AccumCode == 0xff000001)
                {
                int i = Pos + (p - &pData[Pos]);
                long Offset = (i >= 2) ? 0 : 2 - i;
                SetParsePosition (i - (2 - Offset));
                m_LastStartCodeOffset = Offset;
                m_AccumCode = AccumCode;

                if (m_StartCode.pBuffer && !Offset)
                    {
                    // False alarm--we set the startcode marker 
                    // on a previous buffer because we thought
                    // the first bytes in the bitstream might
                    // contain a start code. They didn't, so we
                    // can write out and release any held
                    // buffers.
                    m_StartCode.Reset();
                    HRESULT hr = OnBufferEnd();
                    if (FAILED(hr))
                        {
                        return hr;
                        }
                    }

                return S_OK;
                }
            ++p;
            if ( (p < q)  &&
                 (*p > 1) &&
                 ( (BYTE *)((LONG)p & ~0x3) == p ) )
                {
                // now search for a 0 in DWORDS
                DWORD *p32 = (DWORD *)p;
                while (p32 < q32)
                    {
                    DWORD val = *p32;

                    DWORD hasZeroByte = (DWORD)(
                        (val - 0x01010101) &
                        ~val &
                        0x80808080
                        );
                    if (hasZeroByte)
                        {
                        break;
                        }
                    ++p32;
                    }
                p = (BYTE *)p32;
                // let's point to the 0 byte now
                while (*p != 0  &&  p < q)
                    {
                    ++p;
                    }
                AccumCode = 0xffffffff;
                }
            }

        SetParsePosition(m_Parse.pBuffer->Size);
        m_AccumCode = AccumCode;

        if (m_StartCode.pBuffer)
            {
            // False alarm--we set the startcode marker 
            // on a previous buffer because we thought
            // the bitstream might contain a start code.
            // It didn't, so we can write out and release
            // any held buffers.
            m_StartCode.Reset();
            HRESULT hr = OnBufferEnd();
            if (FAILED(hr))
                {
                return hr;
                }
            }

        long NumZeros = 0;
        for (int i = 0; i < 4; i++)
            {
            if ((AccumCode & 0xff) != 0)
                {
                break;
                }
            ++NumZeros;
            AccumCode >>= 8;
            }

        NumZeros = min(NumZeros, 2);

        if (NumZeros > m_Parse.pBuffer->Size)
            {
            NumZeros = m_Parse.pBuffer->Size;
            }

        if (NumZeros)
            {
            // This could be one of the start codes we care
            // about. Set the startcode marker so we can
            // preserve the offset and hold the buffer until we
            // know more.
            m_StartCode = m_Parse;
            m_StartCode.Position -= NumZeros;
            CheckMarkers();
            }

        return S_FALSE;
    }

    bool
    SetRestorePoint(
        RestorePoint_e RestorePoint
        )
    {
        ASSERT(RpNone != RestorePoint);
        m_RestorePoint = RestorePoint;
        m_LastStartCodeOffset = GetRemainingLength();
        SetParsePosition(m_Parse.pBuffer->Size);
        return true;
    }

    static
    long
    GetPicturePeriod(
        FrameRate_e FrameRate,
        bool IsField
        )
    {
        // Returns the period of one frame in 100-nanosecond
        // units. We calculate the value using:
        //  (1 / frame_rate_value) * ( 1 / (10^-9 / 100) )
        long Rate;

        switch (FrameRate)
            {
            case Rate_23_967:
                Rate = 417083;
                break;
            case Rate_24:
                Rate = 416666;
                break;
            case Rate_25:
                Rate = 400000;
                break;
            case Rate_29_97:
                Rate = 333666;
                break;
            case Rate_30:
                Rate = 333333;
                break;
            case Rate_50:
                Rate = 200000;
                break;
            case Rate_59_94:
                Rate = 166833;
                break;
            case Rate_60:
                Rate = 166666;
                break;
            default:
                ASSERT(false);
                Rate = 0;
            }

        if (IsField)
            {
            Rate >>= 1;
            }

        return Rate;
    }

    void
    CalculateFirstTimestamps()
    {
        // TODO: terrestrial scenario --> We get a single picture,
        // TODO: then lose the signal for a little while, then pick
        // TODO: it up again a little later.  Because we haven't
        // TODO: figured out the first DTS, we won't detect the
        // TODO: discontinuity and the DTS will be WAY off.

        ASSERT( BidirectionallyPredictiveCoded !=
                m_PrevPicture.Info.CodingType );
        ASSERT( m_ReferencePictureCount >= 0 );
        ASSERT( BidirectionallyPredictiveCoded !=
                m_CurPicture.Info.CodingType ||
                m_DependentPictureCount >= 0 );
        ASSERT( m_PrevPicture.Info.Pts > 0 );

        // PTS is already set, calculate the DTS using
        // information from the 2nd (aka current) picture
        if ( BidirectionallyPredictiveCoded !=
             m_CurPicture.Info.CodingType )
            {
            // 2nd picture is I or P
            m_PrevPicture.Info.DtsOffset =
                -m_PrevPicture.DisplayPeriod;
            return;
            }

        // 2nd picture is B
        long DisplayOrderDelta =
            m_PrevPicture.TemporalReference -
            m_CurPicture.TemporalReference;

        ASSERT(DisplayOrderDelta != 0);

        if (DisplayOrderDelta < 0)
            {
            DisplayOrderDelta += 1024;
            }

        m_PrevPicture.Info.DtsOffset =
            m_PrevPicture.DisplayPeriod *
            -(DisplayOrderDelta + 1);
    }

    HRESULT
    CalculateTimestamps()
    {
        ASSERT( m_CurPicture.Info.OnlyNeedsTimestamp() );

        // The temporal_refence field in MPEG-2 Video is
        // designed to reset at every GOP or rollover at 1024.
        // We use reference and dependent counters to detect the
        // rollover: our counter associated with the current
        // picture type (I/P == reference, B == dependent)
        // should always equal the temporal_reference for that
        // picture. If it doesn't, it has rolled over (or reset
        // on a new GOP). These counters help us unfold the
        // temporal reference value so we can use it in our
        // PTS/DTS calculations.
        if (m_DependentPictureCount < 0)
            {
            if ( BidirectionallyPredictiveCoded ==
                    m_CurPicture.Info.CodingType )
                {
                m_DependentPictureCount =
                    m_CurPicture.TemporalReference;
                }
            }
        else
            {
            m_DependentPictureCount++;
            }

        if (m_ReferencePictureCount < 0)
            {
            if ( BidirectionallyPredictiveCoded !=
                    m_CurPicture.Info.CodingType )
                {
                m_ReferencePictureCount =
                    m_CurPicture.TemporalReference;
                }
            }
        else
            {
            m_ReferencePictureCount++;
            }

        if ( m_PrevPicture.Info.Offset < 0 )
            {
            ASSERT(m_PrevPicture.Info.CodingType == None);

            // This is the first picture in the stream following
            // a discontinuity, so there is no previous picture.
            // Just exit this method early, let the parser
            // append a partial picture info structure to the
            // queue, then we'll fix it up when we get another
            // picture.
            return S_FALSE;
            }

        if ( m_PrevPicture.Info.OnlyNeedsTimestamp() )
            {
            // The previous picture info structure is only
            // partially complete, which means it's the first
            // picture in the stream following a discontinuity.
            // Now that we have information about the 2nd
            // picture in the stream, we can complete the
            // timestamp information for the first.
            CalculateFirstTimestamps();

            bool Succeeded =
                m_pManager->AmmendTailEntry(m_PrevPicture.Info);
            if (!Succeeded)
                {
                return E_FAIL;
                }
            }

        ASSERT( m_PrevPicture.Info.IsComplete() );

        // use information about the current picture and the
        // last picture to calculate the precise presentation
        // (PTS) and decoding (DTS) timestamps.

        // PTS
        long DisplayOrderDelta;
        if ( BidirectionallyPredictiveCoded ==
             m_CurPicture.Info.CodingType )
            {
            DisplayOrderDelta =
                m_DependentPictureCount -
                m_PrevPicture.TemporalReference;
            }
        else
            {
            DisplayOrderDelta =
                m_ReferencePictureCount -
                m_PrevPicture.TemporalReference;
            }

        ASSERT(DisplayOrderDelta != 0);

        __int64 CalculatedPts = -1; //m_CurPicture.Info.Pts

        if (DisplayOrderDelta > 0)
            {
            // in case the frame rate changes, we need to break
            // this calculation into two parts: (1) add the last
            // picture's period to its timestamp, then (2) add a
            // multiple of this sequence's picture period to the
            // timestamp if necessary
            CalculatedPts =
                m_PrevPicture.Info.Pts +
                m_PrevPicture.DisplayPeriod;

            if (DisplayOrderDelta > 1)
                {
                CalculatedPts +=
                    (DisplayOrderDelta - 1) *
                    m_PrevPicture.DisplayPeriod;
                }
            }
        else // DisplayOrderDelta < 0
            {
            CalculatedPts =
                m_PrevPicture.Info.Pts +
                (DisplayOrderDelta * m_PrevPicture.DisplayPeriod);
            }

#if 0
        if (m_CurPicture.Info.Pts >= 0)
            {
            static __int64 MaxDelta = 0;
            __int64 Delta =
                (CalculatedPts > m_CurPicture.Info.Pts)
                    ? (CalculatedPts - m_CurPicture.Info.Pts)
                    : (m_CurPicture.Info.Pts - CalculatedPts);

            if (Delta > MaxDelta)
                {
                MaxDelta = Delta;

                RETAILMSG(1,
                    (L"### Max PTS delta: %I64i\r\n", MaxDelta)
                    );
                }
            }
#endif

        // only set Pts value if it wasn't available on the
        // media sample
        if (m_CurPicture.Info.Pts < 0)
            {
            if ( m_ExpectIframeTimestamps &&
                 IntraCoded == m_CurPicture.Info.CodingType )
                {
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Mpeg2VideoParser::"
                      L"CalculateTimestamps --> I-frame is "
                      L"missing timestamp!\r\n" )
                    );

                ASSERT(false);
                return E_UNEXPECTED;
                }

            m_CurPicture.Info.Pts = CalculatedPts;
            }
        else
            {
            // The DVR Engine always expects I-frame timestamps
            // to increase (even across channel changes, because
            // it was developed against an analog encoder chip).
            // So if we detect that I-frame timestamps go
            // backwards due to problems in the upstream filter,
            // we should at least reset the parser here to
            // hopefully prevent the DVR Engine from sending
            // the EC_ERROR_STREAM_STOPPED event and stopping
            // the graph altogther.
            if ( IntraCoded == m_CurPicture.Info.CodingType &&
                 m_CurPicture.Info.Pts <= m_LastIPictureTimestamp )
                {
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Mpeg2VideoParser::"
                      L"CalculateTimestamps --> This I-frame "
                      L"timestamp is less than the previous "
                      L"I-frame timestamp! (delta=%i)\r\n",
                      m_CurPicture.Info.Pts - m_LastIPictureTimestamp )
                    );

                ASSERT(false);
                return E_UNEXPECTED;
                }

            // We can detect a discontinuity here if the actual
            // and calculated timestamps are too far apart. How
            // far apart should they be? Well, the upstream
            // filter is already flagging discontinuities for
            // the beginning of the stream and for channel
            // changes. We're looking for discontinuities that
            // occur due to gaps in the input stream (e.g. you
            // lose your terrestrial digital signal for a brief
            // moment). The shortest discontinuity then would be
            // one picture. If the picture we missed is the
            // first in a new sequence, then the shortest
            // possible picture period is for a 60fps field
            // picture. So we'll go with that value, minus a
            // small error factor.
            long DiscontinuityDelta =
                GetPicturePeriod(Rate_60, true);
            DiscontinuityDelta -= 10000; // 1ms error factor

            __int64 PtsDelta = 0;
            bool IsEarly = false;

            if (CalculatedPts > m_CurPicture.Info.Pts)
                {
                PtsDelta = CalculatedPts - m_CurPicture.Info.Pts;
                IsEarly = true;
                }
            else
                {
                PtsDelta = m_CurPicture.Info.Pts - CalculatedPts;
                }

            if (PtsDelta > DiscontinuityDelta)
                {
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Mpeg2VideoParser::"
                      L"CalculateTimestamps --> Detected video "
                      L"decoding discontinuity (ts real=%i ms, "
                      L"calculated=%i ms, delta=%s%i ms)\r\n",
                      static_cast<long>(m_CurPicture.Info.Pts/10000),
                      static_cast<long>(CalculatedPts/10000),
                      IsEarly ? L"-" : L"",
                      static_cast<long>(PtsDelta/10000) )
                    );
                }
            }

        if (IntraCoded == m_CurPicture.Info.CodingType)
            {
            m_LastIPictureTimestamp = m_CurPicture.Info.Pts;
            }

        // DTS
        if ( BidirectionallyPredictiveCoded ==
             m_CurPicture.Info.CodingType )
            {
            m_CurPicture.Info.DtsOffset = 0;
            }
        else // current picture is I or P
            {
            if ( BidirectionallyPredictiveCoded ==
                 m_PrevPicture.Info.CodingType )
                {
                m_CurPicture.Info.DtsOffset = static_cast<long>(
                    ( m_PrevPicture.Info.Pts +
                      m_PrevPicture.DisplayPeriod ) -
                    m_CurPicture.Info.Pts
                    );
                ASSERT(m_CurPicture.Info.DtsOffset < 0);
                }
            else // previous picture is I or P
                {
                m_CurPicture.Info.DtsOffset = 0;
                }
            }

        // detect temporal_reference rollover and reset counters
        // as needed
        if ( BidirectionallyPredictiveCoded ==
             m_CurPicture.Info.CodingType )
            {
            if ( m_DependentPictureCount !=
                 m_CurPicture.TemporalReference )
                {
                m_DependentPictureCount =
                    m_CurPicture.TemporalReference;
                m_ReferencePictureCount = -1;
                }
            }
        else // reference pictures (I & P)
            {
            if ( m_ReferencePictureCount !=
                 m_CurPicture.TemporalReference )
                {
                m_ReferencePictureCount =
                    m_CurPicture.TemporalReference;
                m_DependentPictureCount = -1;
                }
            }

        return S_OK;
    }

    void
    CheckMarkers()
    {
#ifdef DEBUG
        // If there's a sync marker it should always point into
        // the same buffer as the write marker.
        if (m_Sync.pBuffer)
            {
            ASSERT( m_Write.pBuffer->pData ==
                    m_Sync.pBuffer->pData );
            ASSERT(m_Sync.Position >= m_Write.Position);
            }

        // If there's a picture marker it should always point
        // into the same buffer as the write marker.
        if (m_Picture.pBuffer)
            {
            ASSERT( m_Write.pBuffer->pData ==
                    m_Picture.pBuffer->pData );
            ASSERT(m_Picture.Position >= m_Write.Position);
            }

        // Same goes for the startcode marker unless there's a
        // sync or picture marker before it.
        if ( m_StartCode.pBuffer &&
             !m_Sync.pBuffer &&
             !m_Picture.pBuffer )
            {
            ASSERT( m_Write.pBuffer->pData ==
                    m_StartCode.pBuffer->pData );
            ASSERT(m_StartCode.Position >= m_Write.Position);
            }

        // The sync and picture markers are mutually exclusive.
        if (m_Sync.pBuffer)
            {
            ASSERT(!m_Picture.pBuffer);
            }
        else if (m_Picture.pBuffer)
            {
            ASSERT(!m_Sync.pBuffer);
            }

        // If either the sync or picture marker is set, it
        // should come before the startcode marker
        if (m_Sync.pBuffer && m_StartCode.pBuffer)
            {
            if (m_Sync.pBuffer == m_StartCode.pBuffer)
                {
                ASSERT(m_Sync.Position <= m_StartCode.Position);
                }
            else
                {
                Buffer_t* pSync = NULL;
                Buffer_t* pStartCode = NULL;
                List_t::iterator it = m_BufferList.begin();

                for (; pSync != m_Sync.pBuffer; ++it)
                    {
                    pSync = &(*it);
                    }
                ASSERT( pSync == m_Sync.pBuffer );

                for (; pStartCode != m_StartCode.pBuffer; ++it)
                    {
                    pStartCode = &(*it);
                    }
                ASSERT( pStartCode == m_StartCode.pBuffer );
                }
            }

        if (m_Picture.pBuffer && m_StartCode.pBuffer)
            {
            if (m_Picture.pBuffer == m_StartCode.pBuffer)
                {
                ASSERT(
                    m_Picture.Position <= m_StartCode.Position
                    );
                }
            else
                {
                Buffer_t* pPicture = NULL;
                Buffer_t* pStartCode = NULL;
                List_t::iterator it = m_BufferList.begin();

                for (; pPicture != m_Picture.pBuffer; ++it)
                    {
                    pPicture = &(*it);
                    }
                ASSERT( pPicture == m_Picture.pBuffer );

                for (; pStartCode != m_StartCode.pBuffer; ++it)
                    {
                    pStartCode = &(*it);
                    }
                ASSERT( pStartCode == m_StartCode.pBuffer );
                }
            }

        if (m_BufferList.size() == 1)
            {
            return;
            }

        // There can be more than one buffer in the list IF:
        // (1) the sync marker is set and the first picture
        //     following the sync marker position is NOT in the
        //     same buffer as the sync marker, or
        // (2) the picture marker is set and the picture and
        //     picture coding extension headers aren't entirely
        //     contained in the same buffer, or
        // (3) the startcode marker is set and the buffer ends
        //     before we determine whether we've really located
        //     a picture or not.
        //
        // In all of these cases the headers for this video
        // access unit (sequence, GOP, picture, picture ext
        // headers) are split across two or more buffers (okay
        // the buffers would have to be REALLY small for the
        // headers to span more than two, so two is realistic),
        // so we need to hold onto multiple buffers until we
        // find what we're looking for. The parse marker will
        // not be in the same buffer as the sync, picture, and
        // write markers. The startcode marker can point to the
        // parser marker buffer in the case where a sync or
        // picture marker has to hang around for longer than
        // one buffer (e.g. a sequence header with a quantizer
        // matrix might span 3 or more buffers, if the buffers
        // are relatively small).
        if (m_Sync.pBuffer)
            {
            ASSERT( m_Sync.pBuffer->pData !=
                    m_Parse.pBuffer->pData );
            }

        if (m_Picture.pBuffer)
            {
            ASSERT( m_Picture.pBuffer->pData !=
                    m_Parse.pBuffer->pData );
            }

        if ( m_StartCode.pBuffer &&
             !m_Sync.pBuffer     &&
             !m_Picture.pBuffer )
            {
            ASSERT( m_StartCode.pBuffer->pData !=
                    m_Parse.pBuffer->pData );
            }
#endif
    }

    long
    PrepareNextBufferWrite()
    {
        // We've finished writing the held buffer, so we
        // can release it now and go back to the normal
        // case: just one buffer in the list.
        ASSERT(m_Write.Position == m_Write.pBuffer->Size);
        ASSERT(&m_BufferList.front() == m_Write.pBuffer);

        ReleaseAndPopFrontBuffer();

        m_Write.pBuffer = &m_BufferList.front();
        SetWritePosition(0);

        long WriteSize = 0;
        if (m_Write.pBuffer != m_Parse.pBuffer)
            {
            // Keep writing out queued input buffers
            // until we get to the one we're currently
            // parsing.
            WriteSize = 
                m_Write.pBuffer->Size - m_Write.Position;
            ASSERT(WriteSize >= 0);
            }

        return WriteSize;
    }

    HRESULT
    OnBufferEnd()
    {
        AutoLogReturn_t LogReturn(
            L"DvrPsMux::Mpeg2VideoParser_t::OnBufferEnd --> "
            L"EXITED on line %i\r\n"
            );

        HRESULT hr = S_OK;
        CheckMarkers();

        // If the sync, picture or startcode markers are set,
        // then we just want to write up to the first marker.
        // The current buffer will hang around until we gather
        // more info from the next buffer, so we can write
        // everything AFTER the first marker later.
        bool OnlyWriteToMarker =
            ( m_Sync.pBuffer    ||
              m_Picture.pBuffer ||
              m_StartCode.pBuffer );

        long MaxSize = -1;

        // NOTE if the startcode marker and another marker (sync
        // or picture) exist at the same time, then the latter
        // marker will always be first, so it should also be the
        // first marker in this <if..then..else> block.
        if (m_Sync.pBuffer)
            {
            MaxSize = m_Sync.Position - m_Write.Position;
            }
        else if (m_Picture.pBuffer)
            {
            MaxSize = m_Picture.Position - m_Write.Position;
            }
        else if (m_StartCode.pBuffer)
            {
            MaxSize = m_StartCode.Position - m_Write.Position;
            }
        else
            {
            if (m_BufferList.size() > 1)
                {
                // this happens when we set a startcode marker
                // in one buffer because of a partial start
                // code, and then in the next buffer we learn
                // that the start code is real, but it isn't one
                // we care about
                MaxSize =
                    m_Write.pBuffer->Size - m_Write.Position;
                }
            else
                {
                MaxSize = m_Parse.Position - m_Write.Position;
                }
            }

        if (MaxSize < 0)
            {
            ASSERT(false);
            return LogReturn.Return(__LINE__, E_FAIL);
            }

        if (!MaxSize)
            {
            // This could happen, for example, if we set the
            // sync point in a buffer and parsed all the way
            // through the next buffer without finding the next
            // picture. At the end of the second buffer we'd
            // hit this code.
            ASSERT( m_Sync.pBuffer    ||
                    m_Picture.pBuffer ||
                    m_StartCode.pBuffer );
            return S_OK;
            }

        IVideoParserWriter* pWriter = NULL;
        ASSERT(m_pManager);
        hr = m_pManager->GetWriter(&pWriter);
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, hr);
            }

        ASSERT(S_FALSE != hr && pWriter);

        while (MaxSize)
            {
            long BytesWritten = 0;

            hr = pWriter->Write(
                m_Write.pBuffer->pData,
                m_Write.Position,
                MaxSize,
                &m_PictureInfoQueue,
                &BytesWritten
                );
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            MaxSize -= BytesWritten;
            AdvanceWritePosition(BytesWritten);

            if (MaxSize)
                {
                DEBUGMSG(
                    ZONE_INFO_MAX,
                    ( L"DvrPsMux::Mpeg2VideoParser_t::"
                      L"OnBufferEnd  --> completed video "
                      L"pack\r\n" )
                    );

                pWriter = NULL;
                hr = m_pManager->GetWriter(&pWriter);
                if (FAILED(hr))
                    {
                    return LogReturn.Return(__LINE__, hr);
                    }

                ASSERT(S_FALSE != hr && pWriter);
                continue;
                }

            if (!OnlyWriteToMarker && m_BufferList.size() > 1)
                {
                ASSERT(!m_Sync.pBuffer);
                MaxSize = PrepareNextBufferWrite();
                if (!MaxSize)
                    {
                    break;
                    }
                }
            }

        return S_OK;
    }

    HRESULT
    OnIFrameBegin()
    {
        AutoLogReturn_t LogReturn(
            L"DvrPsMux::Mpeg2VideoParser_t::OnIFrameBegin "
            L"--> EXITED on line %i\r\n"
            );

        HRESULT hr = S_OK;

        CheckMarkers();
        ASSERT(m_Sync.pBuffer);
        ASSERT(!m_StartCode.pBuffer);
        ASSERT(!m_Picture.pBuffer);
        ASSERT(DetectedPicture == m_IPictureState);

        long MaxSize = m_Sync.Position - m_Write.Position;

        if (MaxSize < 0)
            {
            ASSERT(false);
            return LogReturn.Return(__LINE__, E_FAIL);
            }

        if (!MaxSize)
            {
            // Even if there isn't any data from THIS INPUT
            // buffer to write, there may be data from previous
            // writes sitting in the output buffer. So we may
            // need to flush.
            hr = m_pManager->Flush();
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            // Because we've detected an I-picture, it is now
            // safe to clear the sync marker.
            m_Sync.Reset();

            return S_OK;
            }

        IVideoParserWriter* pWriter = NULL;
        ASSERT(m_pManager);
        hr = m_pManager->GetWriter(&pWriter);
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, hr);
            }

        ASSERT(S_FALSE != hr && pWriter);

        while (MaxSize)
            {
            long BytesWritten = 0;

            hr = pWriter->Write(
                m_Write.pBuffer->pData,
                m_Write.Position,
                MaxSize,
                &m_PictureInfoQueue,
                &BytesWritten
                );
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            MaxSize -= BytesWritten;
            AdvanceWritePosition(BytesWritten);

            if (MaxSize)
                {
                DEBUGMSG(
                    ZONE_INFO_MAX,
                    ( L"DvrPsMux::Mpeg2VideoParser_t::"
                      L"OnIFrameBegin --> completed video "
                      L"pack\r\n" )
                    );

                pWriter = NULL;
                hr = m_pManager->GetWriter(&pWriter);
                if (FAILED(hr))
                    {
                    return LogReturn.Return(__LINE__, hr);
                    }

                ASSERT(S_FALSE != hr && pWriter);
                continue;
                }

            ASSERT( !m_pManager->IsOutputBufferEmpty() );
            hr = m_pManager->Flush();
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            m_Sync.Reset();
            }

        return S_OK;
    }

    HRESULT
    OnIFrameEnd()
    {
        AutoLogReturn_t LogReturn(
            L"DvrPsMux::Mpeg2VideoParser_t::OnIFrameEnd "
            L"--> EXITED on line %i\r\n"
            );

        HRESULT hr = S_OK;

        CheckMarkers();
        ASSERT(!m_Sync.pBuffer);
        ASSERT(!m_StartCode.pBuffer);
        ASSERT(!m_Picture.pBuffer);
        ASSERT(ParsedFrame == m_IPictureState);

        m_IPictureState = CompletedFrame;

        // The only case where we'll have multiple buffers is if
        // the start code which caused the parser to invoke
        // OnIFrameEnd() was split across buffers, i.e. the
        // startcode marker was set. That marker has been
        // cleared, but the associated OnBufferEnd() call wrote
        // all the data up to the split start code, so there
        // isn't anything more for us to write here.
        long MaxSize = (m_BufferList.size() > 1)
                ? 0
                : m_Parse.Position - m_Write.Position;

        if (MaxSize < 0)
            {
            ASSERT(false);
            return LogReturn.Return(__LINE__, E_FAIL);
            }

        if (!MaxSize)
            {
            // Even if there isn't any data from THIS INPUT
            // buffer to write, there may be data from previous
            // writes sitting in the output buffer. So we may
            // need to flush.
            hr = m_pManager->Flush();
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            m_IPictureState = NoPicture;
            return S_OK;
            }

        IVideoParserWriter* pWriter = NULL;
        ASSERT(m_pManager);
        hr = m_pManager->GetWriter(&pWriter);
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, hr);
            }

        ASSERT(S_FALSE != hr && pWriter);

        while (MaxSize)
            {
            long BytesWritten = 0;

            hr = pWriter->Write(
                m_Write.pBuffer->pData,
                m_Write.Position,
                MaxSize,
                &m_PictureInfoQueue,
                &BytesWritten
                );
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            MaxSize -= BytesWritten;
            AdvanceWritePosition(BytesWritten);

            if (MaxSize)
                {
                DEBUGMSG(
                    ZONE_INFO_MAX,
                    ( L"DvrPsMux::Mpeg2VideoParser_t::"
                      L"OnIFrameEnd --> completed video "
                      L"pack\r\n" )
                    );

                pWriter = NULL;
                hr = m_pManager->GetWriter(&pWriter);
                if (FAILED(hr))
                    {
                    return LogReturn.Return(__LINE__, hr);
                    }

                ASSERT(S_FALSE != hr && pWriter);
                continue;
                }

            ASSERT( !m_pManager->IsOutputBufferEmpty() );

            // finished parsing an I-frame; flush the current
            // output buffer downstream
            hr = m_pManager->Flush();
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            if (m_BufferList.size() > 1)
                {
                MaxSize = PrepareNextBufferWrite();
                if (!MaxSize)
                    {
                    break;
                    }
                }
            }

        m_IPictureState = NoPicture;
        return S_OK;
    }

    bool
    ExpectIframeTimestamps()
    {
        long  Value = 0;
        DWORD Size = sizeof(long);
        DWORD Type = 0;
        HKEY  hKey = NULL;

        long ReturnCode = RegOpenKeyEx(
            HKEY_LOCAL_MACHINE,
            L"Software\\Microsoft\\Dvr\\DvrPsMux",
            0,
            0,
            &hKey);
        if (ReturnCode == ERROR_SUCCESS && hKey != NULL)
            {
            ReturnCode = RegQueryValueEx(
                hKey,
                L"ExpectIframeTimestamps",
                NULL,
                &Type,
                reinterpret_cast<BYTE*>(&Value),
                &Size
                );
            ASSERT( ERROR_SUCCESS == ReturnCode ||
                    ERROR_FILE_NOT_FOUND == ReturnCode );

            RegCloseKey(hKey);

            if ( ERROR_SUCCESS != ReturnCode ||
                 ( REG_DWORD != Type ||
                   sizeof(DWORD) != Size ) )
                {
                Value = 0;
                }
            }

        return (0 != Value);
    }

public:
    void
    Reset()
    {
        BaseParser_t::Reset();

        m_RestorePoint = RpNone;
        m_IPictureState = NoPicture;
        m_ParseOrder = PictureCodingExtensionHeader;
        m_Sync.Reset();
        m_Picture.Reset();
        m_StartCode.Reset();
        m_LastStartCodeOffset = 0;
        m_AccumCode = 0xffffffff;
        m_ReferencePictureCount = -1;
        m_DependentPictureCount = -1;
        m_TemporalReference = 0;
        m_FoundFirstSequence = false;
        m_FoundFirstGop = false;
        m_HadStartCodeMarker = false;
        m_PrevPicture.Info.Reset();
        m_CurPicture.Info.Reset();
        m_PictureInfoQueue.clear();
    }

    Mpeg2VideoParser_t() :
        StartCodeSize(4),
        PictureCodingExtensionId(8),
        m_FrameRate(Rate_29_97),
        m_pManager(NULL),
        m_LastIPictureTimestamp(-1),
        m_ExpectIframeTimestamps(ExpectIframeTimestamps())
    {
        Reset();
    }

    bool
    SetParserManager(
        IVideoParserManager* pManager
        )
    {
        if (!pManager)
            {
            return false;
            }

        m_pManager = pManager;
        return true;
    }

    HRESULT
    SetBuffer(
        BYTE* pData,
        long Size,
        ArgType ReleaseArg,
        __int64 Timestamp,
        bool Discontinuity
        )
    {
        if (Discontinuity)
            {
            Reset();
            }

        HRESULT hr = BaseParser_t::SetBuffer(
            pData,
            Size,
            ReleaseArg,
            Timestamp,
            Discontinuity
            );
        if (FAILED(hr))
            {
            return hr;
            }

        return S_OK;
    }

    HRESULT
    Process()
    {
        AutoLogReturn_t LogReturn(
            L"DvrPsMux::Mpeg2VideoParser_t::Process --> EXITED "
            L"on line %i\r\n"
            );

        HRESULT hr = BaseParser_t::Process();
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, hr);
            }
        else if (S_FALSE == hr)
            {
            RETAILMSG(
                1,
                ( L"DvrPsMux::Mpeg2VideoParser_t::Process --> "
                  L"Waiting for the first TIMESTAMPED media "
                  L"sample following a parser reset, "
                  L"discarding current media sample\r\n" )
                );

            ASSERT( 1 == m_BufferList.size() );
            ReleaseAndPopFrontBuffer();
            return S_OK;
            }

        if (!m_pManager)
            {
            ASSERT(false);
            return LogReturn.Return(__LINE__, E_UNEXPECTED);
            }

        if (RpNone != m_RestorePoint)
            {
            RestorePoint_e RestorePoint = m_RestorePoint;
            m_RestorePoint = RpNone;

            switch (RestorePoint)
                {
                case RpStartCodeValue:
                    goto Restore_StartCodeValue;
                case RpTemporalReference1:
                    goto Restore_TemporalReference1;
                case RpTemporalReference2:
                    goto Restore_TemporalReference2;
                case RpPictureCodingType:
                    goto Restore_PictureCodingType;
                case RpFrameRateCode:
                    goto Restore_FrameRateCode;
                case RpExtensionStartCodeIdentifier:
                    goto Restore_ExtensionStartCodeIdentifier;
                case RpPictureStructure:
                    goto Restore_PictureStructure;
                case RpClosedGop:
                    goto Restore_ClosedGop;
                default:
                    ASSERT(RpNone == RestorePoint);
                    // do nothing
                    break;
                }
            }

        // if m_LastStartCodeOffset is set, we should've taken
        // one of the RestorePoint paths above
        ASSERT(!m_LastStartCodeOffset);

        HRESULT AdvanceResult = AdvanceToStartCode();

        while (S_OK == AdvanceResult)
            {
            if ( !DataAvailableAt(start_code_value, 1) )
                {
                // This could be one of the start codes we care
                // about. Set the startcode marker so we can
                // preserve the offset and hold the buffer until
                // we know more.
                if (!m_StartCode.pBuffer)
                    {
                    m_StartCode = m_Parse;
                    CheckMarkers();
                    }
                
                bool Succeeded = SetRestorePoint(RpStartCodeValue);
                if (!Succeeded)
                    {
                    return LogReturn.Return(__LINE__, E_FAIL);
                    }
                break;
                }

    Restore_StartCodeValue:

            if ( ( !m_FoundFirstSequence ) &&
                 ( sequence_header_code !=
                    GetByteAt(start_code_value) ) )
                {
                // we want to start the program stream with a
                // proper sequence (because we need to know the
                // frame rate) so skip orphaned pictures until
                // we get a sequence header
                AdvanceParsePosition(StartCodeSize);
                m_LastStartCodeOffset = 0;

                if (m_StartCode.pBuffer)
                    {
                    // We held onto the previous buffer thinking
                    // it might have the first one or two bytes
                    // of a sequence start code. It didn't, so
                    // so now we can release that buffer
                    ASSERT( m_BufferList.size() > 1 );
                    ASSERT( &m_BufferList.front() ==
                            m_StartCode.pBuffer );

                    while (m_BufferList.size() > 1)
                        {
                        ReleaseAndPopFrontBuffer();
                        }

                    m_StartCode.Reset();
                    m_Write = m_Parse;
                    }

                AdvanceResult = AdvanceToStartCode();
                continue;
                }

            switch ( GetByteAt(start_code_value) )
                {
                case picture_start_code:
                    {
                    // validate parser order
                    if (m_ParseOrder == PictureHeader)
                        {
                        RETAILMSG(
                            1,
                            ( L"DvrPsMux::Mpeg2VideoParser_t:Process "
                              L"--> detected video bitstream corruption "
                              L"(consecutive picture start codes)\r\n" )
                            );
                        return S_FALSE;
                        }

                    if (!m_FoundFirstGop)
                        {
                        // GOP headers are optional, so stop looking
                        // if we don't find one by the time we get
                        // to a picture header
                        ASSERT(m_FoundFirstSequence);
                        m_FoundFirstGop = true;
                        }

                    // scope PictureStart
                        {
                        Marker_t PictureStart = m_Parse;
                        m_HadStartCodeMarker = false;

                        if (m_StartCode.pBuffer)
                            {
                            PictureStart = m_StartCode;
                            m_HadStartCodeMarker = true;
                            m_StartCode.Reset();
                            }

                        ASSERT(PictureStart.pBuffer);
                        ASSERT(PictureStart.Position >= 0);

                        if (ParsedFrame == m_IPictureState)
                            {
                            hr = OnIFrameEnd();
                            if (FAILED(hr))
                                {
                                return LogReturn.Return(__LINE__, hr);
                                }
                            }

                        m_PrevPicture = m_CurPicture;

                        m_CurPicture.Info.Reset();
                        m_CurPicture.Info.Offset =
                            PictureStart.Position;
                        m_CurPicture.pBuffer =
                            PictureStart.pBuffer;
                        }

                    // the picture marker needs to be mutually
                    // exclusive with the sync marker
                    if (!m_Sync.pBuffer)
                        {
                        m_Picture.pBuffer =
                            m_CurPicture.pBuffer;
                        m_Picture.Position =
                            m_CurPicture.Info.Offset;
                        CheckMarkers();
                        }

                    // get temporal_reference (describes display
                    // order; helps us determine timestamps)
                    // NOTE this field is more than one byte in
                    // length, so it can be split across
                    // buffers. We'll handle that problem by
                    // reading it one byte at a time and OR-ing
                    // the result
                    if ( !DataAvailableAt(temporal_reference, 1) )
                        {
                        bool Succeeded =
                            SetRestorePoint(RpTemporalReference1);
                        if (!Succeeded)
                            {
                            return LogReturn.Return(__LINE__, E_FAIL);
                            }
                        break;
                        }

    Restore_TemporalReference1:

                    BYTE TemporalReference1 = GetByteAt(
                        temporal_reference
                        );

                    m_TemporalReference =
                        static_cast<short>(TemporalReference1) << 2;

                    if ( !DataAvailableAt(temporal_reference+1, 1) )
                        {
                        bool Succeeded =
                            SetRestorePoint(RpTemporalReference2);
                        if (!Succeeded)
                            {
                            return LogReturn.Return(__LINE__, E_FAIL);
                            }
                        break;
                        }

    Restore_TemporalReference2:

                    BYTE TemporalReference2 = ExtractBits(
                        GetByteAt(temporal_reference + 1),
                        7, 6
                        );

                    m_TemporalReference |= TemporalReference2;
                    m_CurPicture.TemporalReference =
                        m_TemporalReference;

                    // get picture_coding_type (detect I,P,B frames)
                    if ( !DataAvailableAt(picture_coding_type, 1) )
                        {
                        bool Succeeded =
                            SetRestorePoint(RpPictureCodingType);
                        if (!Succeeded)
                            {
                            return LogReturn.Return(__LINE__, E_FAIL);
                            }
                        break;
                        }

    Restore_PictureCodingType:

                    BYTE CodingType = ExtractBits(
                        GetByteAt(picture_coding_type),
                        5, 3
                        );

                    m_CurPicture.Info.CodingType =
                        static_cast<PictureCodingType_e>(CodingType);

                    if ( (m_CurPicture.Info.CodingType <
                            IntraCoded) ||
                         (m_CurPicture.Info.CodingType >
                            BidirectionallyPredictiveCoded) )
                        {
                        ASSERT(false);
                        RETAILMSG(
                            1,
                            ( L"DvrPsMux::Mpeg2VideoParser_t::"
                              L"Process --> detected bitstream "
                              L"corruption (picture_header::"
                              L"picture_coding_type)\r\n" )
                            );
                        return S_FALSE;
                        }

                    // If an I-frame is coded in two fields,
                    // they can be two I-pictures, or an
                    // I-picture followed by a P-picture
                    ASSERT(
                        NoPicture == m_IPictureState ||
                        BidirectionallyPredictiveCoded != CodingType
                        );
                    if ( ParsedField == m_IPictureState)
                        {

                        ASSERT( BidirectionallyPredictiveCoded
                                != CodingType );

                        // we're in the middle of an interlaced
                        // I-frame, so we just need to wait
                        AdvanceParsePosition(
                            picture_coding_type + 1
                            );
                        break;
                        }

                    ASSERT(NoPicture == m_IPictureState);

                    if (IntraCoded == CodingType)
                        {
                        m_IPictureState = DetectedPicture;

                        // we're at the beginning of an I-frame,
                        // so we need to finish the current
                        // media sample and begin a new sync-
                        // point media sample
                        if (!m_Sync.pBuffer)
                            {
                            // the picture and sync markers are
                            // mutually exclusive
                            m_Sync = m_Picture;
                            m_Picture.Reset();
                            CheckMarkers();
                            }

                        hr = OnIFrameBegin();
                        if (FAILED(hr))
                            {
                            return LogReturn.Return(__LINE__, hr);
                            }
                        }
                    else
                        {
                        if (m_Sync.pBuffer)
                            {
                            // The sync marker is NOT pointing
                            // to the beginning of an I-picture
                            // access unit--false alarm. So we
                            // can clear the sync marker. Also,
                            // if we held the previous buffer,
                            // we can release it here. NOTE this
                            // also handles the case where both
                            // the sync AND startcode markers
                            // were set.
                            ASSERT( m_BufferList.size() == 1 ||
                                    m_Sync.Position == m_Write.Position );

                            m_Sync.Reset();

                            if (m_BufferList.size() > 1)
                                {
                                hr = OnBufferEnd();
                                if (FAILED(hr))
                                    {
                                    return LogReturn.Return(__LINE__, hr);
                                    }
                                }
                            }
                        else if (m_HadStartCodeMarker)
                            {
                            // Handle the case where just the
                            // startcode marker was set (not
                            // sync). We need to write and
                            // release the previous buffer.
                            ASSERT(m_BufferList.size() > 1);
                            hr = OnBufferEnd();
                            if (FAILED(hr))
                                {
                                return LogReturn.Return(__LINE__, hr);
                                }
                            }
                        }

                    DEBUGMSG(
                        ZONE_INFO_MAX,
                        ( L"DvrPsMux::Mpeg2VideoParser_t::"
                          L"Process --> detected %s-picture\r\n",
                          (IntraCoded == CodingType) ? L"I"
                            : (PredictiveCoded == CodingType) ? L"P"
                                : L"B" )
                        );

                    // set the picture marker to hold the buffer
                    // associated with the picture byte offset
                    // until we send the picture info to the mux
                    if (!m_Picture.pBuffer)
                        {
                        while (m_CurPicture.pBuffer != m_Write.pBuffer)
                            {
                            // do this to maintain the rule that the
                            // picture marker always points into the
                            // write marker buffer when there are
                            // multiple buffers
                            ASSERT(m_BufferList.size() > 1);

                            m_Picture.pBuffer = m_Write.pBuffer;
                            m_Picture.Position = m_Write.pBuffer->Size;

                            hr = OnBufferEnd();
                            if (FAILED(hr))
                                {
                                return LogReturn.Return(__LINE__, hr);
                                }

                            ReleaseAndPopFrontBuffer();
                            m_Write.pBuffer = &m_BufferList.front();
                            SetWritePosition(0);
                            }

                        m_Picture.pBuffer = m_CurPicture.pBuffer;
                        m_Picture.Position = m_CurPicture.Info.Offset;
                        CheckMarkers();
                        }

                    // we wait to set the timestamp until we're
                    // sure we're going to parse all the way
                    // through this picture (in the I-picture
                    // case, we may stop parsing as soon as we
                    // detect it above, flush the sample, then
                    // start parsing again from the start code--
                    // if we reset the timestamp to -1 in the
                    // first parse attempt, then we wouldn't
                    // have it for the second)
                    m_CurPicture.Info.Pts = m_Timestamp;
                    if (m_Timestamp >= 0)
                        {
                        m_Timestamp = -1;
                        }

                    m_HadStartCodeMarker = false;
                    m_ParseOrder = PictureHeader;

                    AdvanceParsePosition(
                        picture_coding_type + 1
                        );
                    }
                    break;

                case sequence_header_code:
                    {
                    // validate parser order
                    if ( m_ParseOrder !=
                         PictureCodingExtensionHeader )
                        {
                        RETAILMSG(
                            1,
                            ( L"DvrPsMux::Mpeg2VideoParser_t:Process "
                              L"--> detected video bitstream corruption "
                              L"(sequence header not preceded by picture "
                              L"coding extension header)\r\n" )
                            );
                        return S_FALSE;
                        }

                    if (!m_FoundFirstSequence)
                        {
                        // We can't just advance the parse marker
                        // because then the offset for this picture
                        // would be wrong in the timestamp entry;
                        // instead we need to rebase the write
                        // marker (and consequently the parse
                        // marker if there's only one buffer in
                        // the list) relative to the parse position.
                        long Pos = m_StartCode.pBuffer
                            ? m_StartCode.Position
                            : m_Parse.Position;
                        
                        m_Write.pBuffer->pData += Pos;
                        m_Write.pBuffer->Size -= Pos;

                        SetWritePosition(0);
                        SetParsePosition(0);

                        m_FoundFirstSequence = true;
                        m_NeedsTimestamp = false;
                        m_pManager->Notify(FirstVideoSequence);
                        }

                    bool HadStartCodeMarker = false;
                    if (m_StartCode.pBuffer)
                        {
                        HadStartCodeMarker = true;
                        m_StartCode.Reset();
                        }
                        
                    if (ParsedFrame == m_IPictureState)
                        {
                        hr = OnIFrameEnd();
                        if (FAILED(hr))
                            {
                            return LogReturn.Return(__LINE__, hr);
                            }
                        }

                    // if this (repeat) sequence header precedes
                    // an I-frame, then the new sync-point media
                    // sample should start with this sequence
                    // header
                    ASSERT(!m_Sync.pBuffer);
                    m_Sync = HadStartCodeMarker
                        ? m_Write
                        : m_Parse;
                    CheckMarkers();

                    // get frame_rate_code (to interpolate PTS)
                    if ( !DataAvailableAt(frame_rate_code, 1) )
                        {
                        bool Succeeded =
                            SetRestorePoint(RpFrameRateCode);
                        if (!Succeeded)
                            {
                            return LogReturn.Return(__LINE__, E_FAIL);
                            }
                        break;
                        }

    Restore_FrameRateCode:

                    BYTE FrameRate = ExtractBits(
                        GetByteAt(frame_rate_code),
                        3, 0
                        );

                    m_FrameRate =
                        static_cast<FrameRate_e>(FrameRate);
                    if ( m_FrameRate < Rate_23_967 ||
                         m_FrameRate > Rate_60 )
                        {
                        ASSERT(false);
                        RETAILMSG(
                            1,
                            ( L"DvrPsMux::Mpeg2VideoParser_t::"
                              L"Process --> detected bitstream "
                              L"corruption (sequence_header::"
                              L"frame_rate_code)\r\n" )
                            );
                        return S_FALSE;
                        }

                    m_ParseOrder = SequenceHeader;

                    AdvanceParsePosition(frame_rate_code + 1);
                    }
                    break;

                case extension_start_code:
                    {
                    if (m_StartCode.pBuffer)
                        {
                        // we don't need to remember the offset
                        // of this start code
                        m_StartCode.Reset();
                        hr = OnBufferEnd();
                        if (FAILED(hr))
                            {
                            return LogReturn.Return(__LINE__, hr);
                            }
                        }

                    // get picture_structure (to detect fields vs.
                    // frame, to know what goes in a sync-point
                    // media sample)
                    if ( !DataAvailableAt(
                            extension_start_code_identifier,
                            1
                            ) )
                        {
                        bool Succeeded = SetRestorePoint(
                            RpExtensionStartCodeIdentifier
                            );
                        if (!Succeeded)
                            {
                            return LogReturn.Return(__LINE__, E_FAIL);
                            }
                        break;
                        }

    Restore_ExtensionStartCodeIdentifier:

                    BYTE ExtensionId = ExtractBits(
                        GetByteAt(extension_start_code_identifier),
                        7, 4
                        );
                    if (PictureCodingExtensionId != ExtensionId)
                        {
                        // we only care about the picture coding
                        // extension header
                        AdvanceParsePosition(
                            extension_start_code_identifier + 1
                            );
                        break;
                        }

                    // validate parser order
                    if (m_ParseOrder != PictureHeader)
                        {
                        RETAILMSG(
                            1,
                            ( L"DvrPsMux::Mpeg2VideoParser_t:Process "
                              L"--> detected video bitstream corruption "
                              L"(picture coding extension header not "
                              L"preceded by picture header)\r\n" )
                            );
                        return S_FALSE;
                        }

                    if ( !DataAvailableAt(picture_structure, 1) )
                        {
                        bool Succeeded =
                            SetRestorePoint(RpPictureStructure);
                        if (!Succeeded)
                            {
                            return LogReturn.Return(__LINE__, E_FAIL);
                            }
                        break;
                        }

    Restore_PictureStructure:

                    BYTE PictureStructure = ExtractBits(
                        GetByteAt(picture_structure),
                        1, 0
                        );

                    if (FramePicture == PictureStructure)
                        {
                        m_CurPicture.DisplayPeriod =
                            GetPicturePeriod(m_FrameRate, false);

                        ASSERT( NoPicture == m_IPictureState ||
                                DetectedPicture == m_IPictureState );
                        if (DetectedPicture == m_IPictureState)
                            {
                            // entering an I-frame
                            m_IPictureState = ParsedFrame;
                            }
                        }
                    else
                        {
                        if ( TopField != PictureStructure &&
                             BottomField != PictureStructure )
                            {
                            ASSERT(false);
                            RETAILMSG(
                                1,
                                ( L"DvrPsMux::Mpeg2VideoParser_t::"
                                  L"Process --> detected bitstream "
                                  L"corruption (picture_coding_extension::"
                                  L"picture_structure)\r\n" )
                                );
                            return S_FALSE;
                            }

                        m_CurPicture.DisplayPeriod =
                            GetPicturePeriod(m_FrameRate, true);

                        ASSERT( NoPicture == m_IPictureState ||
                                DetectedPicture == m_IPictureState ||
                                ParsedField == m_IPictureState );

                        if (DetectedPicture == m_IPictureState)
                            {
                            // entering first field in an I-frame
                            m_IPictureState = ParsedField;
                            }
                        else if (ParsedField == m_IPictureState)
                            {
                            // entering second field in an I-frame
                            m_IPictureState = ParsedFrame;
                            }
                        }

                    // We have officially parsed out all the
                    // information we care about from this
                    // picture. Now we can calculate the
                    // timestamps before committing the picture
                    // info to the queue. And we can clear the
                    // picture marker.
                    hr = CalculateTimestamps();
                    if (E_UNEXPECTED == hr)
                        {
                        // timestamps from the upstream filter
                        // are wrong; reset the parser
                        RETAILMSG(
                            1,
                            ( L"DvrPsMux::Mpeg2VideoParser_t:Process "
                              L"--> I-frame timestamps from the "
                              L"upstream filter are incorrect\r\n" )
                            );
                        return S_FALSE;
                        }
                    else if (FAILED(hr))
                        {
                        return LogReturn.Return(__LINE__, E_FAIL);
                        }

                    bool Succeeded = m_PictureInfoQueue.push_back(
                        m_CurPicture.Info
                        );
                    if (!Succeeded)
                        {
                        ASSERT(false);
                        return
                            LogReturn.Return(__LINE__, E_FAIL);
                        }

                    m_Picture.Reset();
                    if (m_BufferList.size() > 1)
                        {
                        hr = OnBufferEnd();
                        if (FAILED(hr))
                            {
                            return LogReturn.Return(__LINE__, hr);
                            }
                        }

                    if (m_PictureInfoQueue.size() > 100)
                    {
                        DEBUGMSG(
                            ZONE_INFO_MAX && ZONE_REFCOUNT,
                            ( L"DvrPsMux::Mpeg2VideoParser_t::"
                              L"Process --> Picture Info Queue "
                              L"size = %i\r\n",
                              m_PictureInfoQueue.size() )
                            );
                    }

                    m_ParseOrder = PictureCodingExtensionHeader;

                    AdvanceParsePosition(picture_structure + 1);
                    }
                    break;

                case group_start_code:
                    {
                    // validate parser order
                    if ( m_ParseOrder != SequenceHeader  &&
                         m_ParseOrder != PictureCodingExtensionHeader )
                        {
                        RETAILMSG(
                            1,
                            ( L"DvrPsMux::Mpeg2VideoParser_t:Process "
                              L"--> detected video bitstream corruption "
                              L"(GOP header not preceded by sequence "
                              L"header)\r\n" )
                            );
                        return S_FALSE;
                        }

                    bool HadStartCodeMarker = false;
                    if (m_StartCode.pBuffer)
                        {
                        HadStartCodeMarker = true;
                        m_StartCode.Reset();
                        }

                    if (ParsedFrame == m_IPictureState)
                        {
                        hr = OnIFrameEnd();
                        if (FAILED(hr))
                            {
                            return LogReturn.Return(__LINE__, hr);
                            }
                        }

                    // unless a sequence header precedes this
                    // I-frame, the new sync-point media sample
                    // should begin with this GOP header
                    if (!m_Sync.pBuffer)
                        {
                        m_Sync = HadStartCodeMarker
                            ? m_Write
                            : m_Parse;
                        CheckMarkers();
                        }

                    if (m_FoundFirstSequence && !m_FoundFirstGop)
                        {
                        if ( !DataAvailableAt(closed_gop, 1) )
                            {
                            bool Succeeded =
                                SetRestorePoint(RpClosedGop);
                            if (!Succeeded)
                                {
                                return LogReturn.Return(__LINE__, E_FAIL);
                                }
                            break;
                            }

    Restore_ClosedGop:
                        ASSERT( !m_FoundFirstGop &&
                                 m_FoundFirstSequence );

                        m_FoundFirstGop = true;

                        BYTE GopFields = GetByteAt(closed_gop);
                        bool IsClosedGop = !!ExtractBits(GopFields, 6, 6);
                        bool IsBrokenLink = !!ExtractBits(GopFields, 5, 5);

                        if (!IsClosedGop && !IsBrokenLink)
                            {
                            // set broken_link flag so decoder will
                            // ignore any B-pictures following the
                            // 1st I-picture following this GOP header
                            // see ISO/IEC 13818-2 section 6.3.8)
                            BYTE* pData = m_Parse.pBuffer->pData;
                            long Offset = closed_gop - m_LastStartCodeOffset;
                            pData[m_Parse.Position + Offset] =
                                GopFields | 0x20;

                            RETAILMSG(
                                1,
                                ( L"DvrPsMux::Mpeg2VideoParser_t:Process "
                                  L"--> Set broken_link flag in GOP "
                                  L"header (GOP byte[7]=%#x)\r\n",
                                  GetByteAt(closed_gop) )
                                );
                            }
                        }

                    m_ParseOrder = GopHeader;

                    AdvanceParsePosition(StartCodeSize);
                    }
                    break;

                default:
                    // a start code we don't care about--ignore
                    if (m_StartCode.pBuffer)
                        {
                        m_StartCode.Reset();
                        hr = OnBufferEnd();
                        if (FAILED(hr))
                            {
                            return LogReturn.Return(__LINE__, hr);
                            }
                        }

                    AdvanceParsePosition(StartCodeSize);
                    break;
                }

            if (RpNone != m_RestorePoint)
                {
                break;
                }

            ASSERT(!m_StartCode.pBuffer);

            // We've successfully parsed all the interesting
            // data associated with the current start code, so
            // now we can reset the start code offset.
            m_LastStartCodeOffset = 0;

            AdvanceResult = AdvanceToStartCode();
            }

        if (FAILED(AdvanceResult))
            {
            return LogReturn.Return(__LINE__, AdvanceResult);
            }

        // reached the end of the buffer; write it and get a new
        // one
        if (m_FoundFirstSequence)
            {
            hr = OnBufferEnd();
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }
            }

        if ( !m_Sync.pBuffer &&
             !m_Picture.pBuffer &&
             !m_StartCode.pBuffer )
            {
            // If there's a sync, picture or startcode marker,
            // it means we need to hold onto this buffer until
            // we collect more info from the next buffer.
            // Otherwise it's safe to release this buffer now.
            ASSERT(m_Write.pBuffer == m_Parse.pBuffer);
            ASSERT(&m_BufferList.front() == m_Write.pBuffer);

            ReleaseAndPopFrontBuffer();
            m_Write.Reset();
            }
        else
            {
            DEBUGMSG(
                ZONE_INFO_MAX,
                ( L"DvrPsMux::Mpeg2VideoParser_t::Process --> "
                  L"%s marker set, holding buffer (%i)\r\n",
                  m_Sync.pBuffer ? L"SYNC" :
                  m_Picture.pBuffer ? L"PICTURE" : L"STARTCODE",
                  m_BufferList.size() )
                );

            //ASSERT(m_BufferList.size() == 1);
            }

        DEBUGMSG(
            ZONE_INFO_MAX && m_FoundFirstSequence,
            ( L"DvrPsMux::Mpeg2VideoParser_t::Process --> "
              L"processed valid video sample\r\n" )
            );

        return S_OK;
    }

};


//
// ---------- Mpeg2AudioParser_t ----------
//

const long
s_FramePeriods_Mpeg2Layer3[] =
{
    261224, 240000, 360000  // 22.05, 24, 16 kHz
};

const long
s_FramePeriods_Mpeg2Layer2[] =
{
    522449, 480000, 720000  // 22.05, 24, 16 kHz
};

const long
s_FramePeriods_Mpeg2Layer1[] =
{
    174150, 160000, 240000  // 22.05, 24, 16 kHz
};

const long
s_FramePeriods_Mpeg1Layer3[] =
{
    261224, 240000, 360000  // 44.1, 48, 32 kHz
};

const long
s_FramePeriods_Mpeg1Layer2[] =
{
    261224, 240000, 360000  // 44.1, 48, 32 kHz
};

const long
s_FramePeriods_Mpeg1Layer1[] =
{
    87075, 80000, 120000    // 44.1, 48, 32 kHz
};


const short
s_FrameSizes_Mpeg2Layer3_22_05_kHz[] =
{
      0,  26,  52,  78, 104, 130, 156, 182,
    208, 261, 313, 365, 417, 470, 522
};

const short
s_FrameSizes_Mpeg2Layer3_24_kHz[] =
{
      0,  24,  48,  72,  96, 120, 144, 168,
    192, 240, 288, 336, 384, 432, 480
};

const short
s_FrameSizes_Mpeg2Layer3_16_kHz[] =
{
      0,  36,  72, 108, 144, 180, 216, 252,
    288, 360, 432, 504, 576, 648, 720
};

const short
s_FrameSizes_Mpeg2Layer2_22_05_kHz[] =
{
      0,  52, 104, 156, 208, 261, 313, 365,
    417, 522, 626, 731, 835, 940, 1044
};

const short
s_FrameSizes_Mpeg2Layer2_24_kHz[] =
{
      0,  48,  96, 144, 192, 240, 288, 336,
    384, 480, 576, 672, 768, 864, 960
};

const short
s_FrameSizes_Mpeg2Layer2_16_kHz[] =
{
      0,  72, 144,  216,  288,  360,  432, 504,
    576, 720, 864, 1008, 1152, 1296, 1440
};

const short
s_FrameSizes_Mpeg2Layer1_22_05_kHz[] =
{
      0,  69, 104, 121, 139, 174, 208, 243,
    278, 313, 348, 383, 417, 487, 557
};

const short
s_FrameSizes_Mpeg2Layer1_24_kHz[] =
{
      0,  64,  96, 112, 128, 160, 192, 224,
    256, 288, 320, 352, 384, 448, 512
};

const short
s_FrameSizes_Mpeg2Layer1_16_kHz[] =
{
      0,  96, 144, 168, 192, 240, 288, 336,
    384, 432, 480, 528, 576, 672, 768
};

const short
s_FrameSizes_Mpeg1Layer3_44_1_kHz[] =
{
      0, 104, 130, 156, 182, 208,  261, 313,
    365, 417, 522, 626, 731, 835, 1044
};

const short
s_FrameSizes_Mpeg1Layer3_48_kHz[] =
{
      0,  96, 120, 144, 168, 192, 240, 288,
    336, 384, 480, 576, 672, 768, 960
};

const short
s_FrameSizes_Mpeg1Layer3_32_kHz[] =
{
      0, 144, 180, 216,  252,  288,  360, 432,
    504, 576, 720, 864, 1008, 1152, 1440
};

const short
s_FrameSizes_Mpeg1Layer2_44_1_kHz[] =
{
      0, 104, 156, 182, 208,  261,  313, 365,
    417, 522, 626, 731, 835, 1044, 1253
};

const short
s_FrameSizes_Mpeg1Layer2_48_kHz[] =
{
      0,  96, 144, 168, 192, 240,  288, 336,
    384, 480, 576, 672, 768, 960, 1152
};

const short
s_FrameSizes_Mpeg1Layer2_32_kHz[] =
{
      0, 144, 216,  252,  288,  360,  432, 504,
    576, 720, 864, 1008, 1152, 1440, 1728
};

const short
s_FrameSizes_Mpeg1Layer1_44_1_kHz[] =
{
      0,  34,  69, 104, 139, 174, 208, 243,
    278, 313, 348, 383, 417, 452, 487
};

const short
s_FrameSizes_Mpeg1Layer1_48_kHz[] =
{
      0,  32,  64,  96, 128, 160, 192, 224,
    256, 228, 320, 352, 384, 416, 448
};

const short
s_FrameSizes_Mpeg1Layer1_32_kHz[] =
{
      0,  48,  96, 144, 192, 240, 288, 336,
    384, 432, 480, 528, 576, 624, 672
};


template<
    class Prototype,
    class ArgType,
    Prototype FnRelease,
    ArgType ArgNullVal
    >
class Mpeg2AudioParser_t :
    public Parser_t<Prototype, ArgType, FnRelease, ArgNullVal>
{
    enum Offsets_e
        {
        ID = 1,
        bitrate_index = 2
        };

    enum AudioId_e
        {
        Mpeg2,
        Mpeg1
        };

    enum Layer_e
        {
        Layer_I = 3,
        Layer_II = 2,
        Layer_III = 1
        };

    enum SamplingFrequency_e
        {
        Rate_22_05 = 0,
        Rate_24 = 1,
        Rate_16 = 2,
        Rate_44_1 = 0,
        Rate_48 = 1,
        Rate_32 = 2
        };

    enum RestorePoint_e
        {
        RpNone,
        RpId,
        RpBitrateIndex
        };

    RestorePoint_e m_RestorePoint;
    AudioId_e      m_Id;
    Layer_e        m_Layer;
    long           m_LastPeriod;
    long           m_FrameLength;
    long           m_Adjust;
    bool           m_FoundFirstFrame;
    bool           m_FoundSyncByte;
    bool           m_FirstWrite;
    bool           m_LostSync;
    AudioTimestampQueue_t m_PtsQueue;
    AudioTimestampEntry_t m_TimestampEntry;
    IAudioParserManager*  m_pManager;

    bool
    DataAvailableAt(
        long Offset,
        long Length = 0
        ) const
    {
        Offset -= m_Adjust;
        ASSERT(Offset >= 0);
        return BaseParser_t::DataAvailableAt(Offset, Length);
    }

    BYTE
    GetByteAt(
        long Offset
        )
    {
        Offset -= m_Adjust;
        ASSERT(Offset >= 0);
        return BaseParser_t::GetByteAt(Offset);
    }

    bool
    ScanForSyncword()
    {
        long BufferSize = m_Parse.pBuffer->Size;
        BYTE* pData = m_Parse.pBuffer->pData;

        for (int i = m_Parse.Position; i < BufferSize; i++)
            {
            if (0xFF == pData[i])
                {
                m_FoundSyncByte = true;
                }
            else if ( 0xF0 == (pData[i] & 0xF0) &&
                      m_FoundSyncByte )
                {
                m_Adjust = 1;
                if (i > 0)
                    {
                    // If we found last byte of the syncword at
                    // position 0 in the buffer it means the
                    // syncword is split across buffers. If we
                    // find it later in the buffer than the
                    // syncword was not split and we don't need
                    // to make any adjustments.
                    m_Adjust = 0;
                    m_FoundSyncByte = false;
                    }

                SetParsePosition( i - (1 - m_Adjust) );

                if (!m_FoundFirstFrame)
                    {
                    // we can't just advance the parse marker
                    // because then the offset for this frame
                    // would be wrong in the timestamp entry;
                    // instead we need to update the buffer
                    // pointer and rebase the parse and write
                    // position markers, relative the the buffer
                    // pointer
                    m_FoundFirstFrame = true;
                    m_Parse.pBuffer->pData += m_Parse.Position;
                    m_Parse.pBuffer->Size -= m_Parse.Position;
                    SetWritePosition(0);
                    SetParsePosition(0);
                    }

                return true;
                }
            else
                {
                m_FoundSyncByte = false;
                }
            }

        return false;
    }


    bool
    AdvanceToSyncword()
    {
        if (m_BufferList.size() != 1)
            {
            ASSERT(false);
            return false;
            }

        // If the bitrate is known, then we can calculate the
        // size of an MPEG-2 audio frame. So we don't need to
        // scan for the next syncword; we can just advance the
        // position marker based on these fields. However the
        // first buffer(s) in the bitstream may not start on a
        // clean syncword boundary, so we do have to scan for
        // the first syncword. We also need to scan if the audio
        // frame reports no bitrate ("free" mode, a.k.a. VBR,
        // variable bitrate)
        if (!m_FrameLength)
            {
            return ScanForSyncword();
            }

        if ( !DataAvailableAt(m_FrameLength, 1) )
            {
            m_FrameLength -= GetRemainingLength();
            SetParsePosition(m_Parse.pBuffer->Size);
            return false;
            }

        AdvanceParsePosition(m_FrameLength);
        m_FrameLength = 0;

        BYTE* pData = m_Parse.pBuffer->pData;
        long Pos = m_Parse.Position;

        if ( pData[Pos] != 0xFF ||
             ( GetRemainingLength() > 1 &&
               (pData[Pos + 1] & 0xF0) != 0xF0 ) )
            {
            ASSERT(false);
            m_LostSync = true;
            }

        return true;
    }

    bool
    SetRestorePoint(
        RestorePoint_e RestorePoint
        )
    {
        m_RestorePoint = RestorePoint;
        m_Adjust = GetRemainingLength();
        SetParsePosition(m_Parse.pBuffer->Size);
        return true;
    }

    long
    GetFramePeriod(
        AudioId_e Id,
        Layer_e Layer,
        BYTE SamplingFrequency
        )
    {
        // Returns the period of one frame in 100-nanosecond
        // units. We calculate the value using:
        //  ( 1 / (<sampling frequency> / <samples per frame>) )
        //      * ( 10^9 / 100 )
        // ...where <samples per frame> depends on the layer
        // (see ISO/IEC 13818-3 section 2.4.2.1)

        if (SamplingFrequency < 0 || SamplingFrequency > 2)
            {
            ASSERT(false);
            return 0;
            }

        const long* pPeriods = NULL;

        if (Mpeg2 == Id)
            {
            if (Layer_III == Layer)
                {
                pPeriods = s_FramePeriods_Mpeg2Layer3;
                }
            else if (Layer_II == Layer)
                {
                pPeriods = s_FramePeriods_Mpeg2Layer2;
                }
            else if (Layer_I == Layer)
                {
                pPeriods = s_FramePeriods_Mpeg2Layer1;
                }
            else
                {
                ASSERT(false);
                return 0;
                }
            }
        else if (Mpeg1 == Id)
            {
            if (Layer_III == Layer)
                {
                pPeriods = s_FramePeriods_Mpeg1Layer3;
                }
            else if (Layer_II == Layer)
                {
                pPeriods = s_FramePeriods_Mpeg1Layer2;
                }
            else if (Layer_I == Layer)
                {
                pPeriods = s_FramePeriods_Mpeg1Layer1;
                }
            else
                {
                ASSERT(false);
                return 0;
                }
            }
        else
            {
            ASSERT(false);
            return 0;
            }

        return pPeriods[SamplingFrequency];
    }

    long
    GetFrameSize(
        AudioId_e Id,
        Layer_e Layer,
        BYTE SamplingFrequency,
        BYTE BitrateIndex,
        bool FrameHasPadding
        )
    {
        if ( (SamplingFrequency < 0 || SamplingFrequency > 2) ||
             (BitrateIndex < 0 || BitrateIndex > 14) )
            {
            ASSERT(false);
            return 0;
            }

        const short* pSizes = NULL;

        if (Mpeg2 == Id)
            {
            if (Layer_III == Layer)
                {
                if (Rate_22_05 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg2Layer3_22_05_kHz;
                    }
                else if (Rate_24 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg2Layer3_24_kHz;
                    }
                else if (Rate_16 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg2Layer3_16_kHz;
                    }
                else
                    {
                    ASSERT(false);
                    return 0;
                    }
                }
            else if (Layer_II == Layer)
                {
                if (Rate_22_05 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg2Layer2_22_05_kHz;
                    }
                else if (Rate_24 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg2Layer2_24_kHz;
                    }
                else if (Rate_16 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg2Layer2_16_kHz;
                    }
                else
                    {
                    ASSERT(false);
                    return 0;
                    }
                }
            else if (Layer_I == Layer)
                {
                if (Rate_22_05 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg2Layer1_22_05_kHz;
                    }
                else if (Rate_24 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg2Layer1_24_kHz;
                    }
                else if (Rate_16 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg2Layer1_16_kHz;
                    }
                else
                    {
                    ASSERT(false);
                    return 0;
                    }
                }
            else
                {
                ASSERT(false);
                return 0;
                }
            }
        else if (Mpeg1 == Id)
            {
            if (Layer_III == Layer)
                {
                if (Rate_44_1 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg1Layer3_44_1_kHz;
                    }
                else if (Rate_48 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg1Layer3_48_kHz;
                    }
                else if (Rate_32 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg1Layer3_32_kHz;
                    }
                else
                    {
                    ASSERT(false);
                    return 0;
                    }
                }
            else if (Layer_II == Layer)
                {
                if (Rate_44_1 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg1Layer2_44_1_kHz;
                    }
                else if (Rate_48 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg1Layer2_48_kHz;
                    }
                else if (Rate_32 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg1Layer2_32_kHz;
                    }
                else
                    {
                    ASSERT(false);
                    return 0;
                    }
                }
            else if (Layer_I == Layer)
                {
                if (Rate_44_1 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg1Layer1_44_1_kHz;
                    }
                else if (Rate_48 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg1Layer1_48_kHz;
                    }
                else if (Rate_32 == SamplingFrequency)
                    {
                    pSizes = s_FrameSizes_Mpeg1Layer1_32_kHz;
                    }
                else
                    {
                    ASSERT(false);
                    return 0;
                    }
                }
            else
                {
                ASSERT(false);
                return 0;
                }
            }
        else
            {
            ASSERT(false);
            return 0;
            }


        long FrameSize = pSizes[BitrateIndex];

        if (FrameSize && FrameHasPadding)
            {
            FrameSize += ( Layer_I == m_Layer ? 4 : 1 );
            }

        return (FrameSize - m_Adjust);
    }

    HRESULT
    Write()
    {
        AutoLogReturn_t LogReturn(
            L"DvrPsMux::Mpeg2AudioParser_t::Write --> EXITED "
            L"on line %i\r\n"
            );

        IAudioParserWriter* pWriter = NULL;

        if (m_FirstWrite)
            {
            // This is our first opportunity to mux audio into
            // the output stream, so we need to make sure we
            // begin on an audio frame boundary.
            if (m_PtsQueue.size() == 0)
                {
                return S_FALSE;
                }

            // Try to get a writer. If GetWriter() returns
            // S_FALSE it means the mux won't let us write yet.
            ASSERT(m_pManager);
            HRESULT hr = m_pManager->GetWriter(&pWriter);
            if (S_FALSE == hr)
                {
                // discard any timestamp data in the queue
                m_PtsQueue.clear();
                return S_FALSE;
                }
            else if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            // Find the frame boundary to begin writing from
            AudioTimestampEntry_t& Entry = m_PtsQueue.front();
            ASSERT(m_Parse.Position >= Entry.Offset);
            ASSERT(m_Write.Position <= Entry.Offset);
            SetWritePosition(Entry.Offset);

            m_FirstWrite = false;
            ASSERT(pWriter);
            }
        else
            {
            // Don't reserve a pack (i.e. get a writer) until we
            // know we have data to write.
            ASSERT(m_pManager);
            HRESULT hr = m_pManager->GetWriter(&pWriter);
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            ASSERT(S_FALSE != hr && pWriter);
            }

        HRESULT hr = S_OK;
        long BytesWritten = 0;

        while (m_FoundSyncByte)
            {
            // this will only happen if the first audio frame in
            // the bitstream does not begin on a buffer
            // boundary; after that we won't need to scan for
            // syncwords
            BYTE SyncByte = 0x0B;
            hr = pWriter->Write(
                &SyncByte,
                0,
                1,
                &m_PtsQueue,
                &BytesWritten
                );
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            AdvanceWritePosition(BytesWritten);

            if (BytesWritten < 1)
                {
                DEBUGMSG(
                    ZONE_INFO_MAX,
                    ( L"DvrPsMux::Mpeg2AudioParser_t::Write "
                      L"--> completed audio pack\r\n" )
                    );

                pWriter = NULL;
                hr = m_pManager->GetWriter(&pWriter);
                if (FAILED(hr))
                    {
                    return LogReturn.Return(__LINE__, hr);
                    }

                ASSERT(S_FALSE != hr && pWriter);
                continue;
                }

            m_FoundSyncByte = false;
            }

        ASSERT(m_Parse.pBuffer == m_Write.pBuffer);

        long WriteSize = m_Parse.Position - m_Write.Position;

        if (WriteSize < 0)
            {
            ASSERT(false);
            return LogReturn.Return(__LINE__, E_FAIL);
            }

        while (WriteSize)
            {
            hr = pWriter->Write(
                m_Write.pBuffer->pData,
                m_Write.Position,
                WriteSize,
                &m_PtsQueue,
                &BytesWritten
                );
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            WriteSize -= BytesWritten;
            AdvanceWritePosition(BytesWritten);

            if (!WriteSize)
                {
                break;
                }

            DEBUGMSG(
                ZONE_INFO_MAX,
                ( L"DvrPsMux::Mpeg2AudioParser_t::Write --> "
                  L"completed audio pack\r\n" )
                );

            pWriter = NULL;
            hr = m_pManager->GetWriter(&pWriter);
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            ASSERT(S_FALSE != hr && pWriter);
            }

        return S_OK;
    }

public:
    void
    Reset()
    {
        BaseParser_t::Reset();

        m_RestorePoint = RpNone;
        m_LastPeriod = 0;
        m_FrameLength = 0;
        m_Adjust = 0;
        m_FoundFirstFrame = false;
        m_FoundSyncByte = false;
        m_FirstWrite = true;
        m_LostSync = false;
        m_TimestampEntry.Reset();
        m_PtsQueue.clear();
    }

    Mpeg2AudioParser_t() :
        m_Id(Mpeg1),
        m_Layer(Layer_II),
        m_pManager(NULL)
    {
        Reset();
    }

    bool
    SetParserManager(
        IAudioParserManager* pManager
        )
    {
        if (!pManager)
            {
            return false;
            }

        m_pManager = pManager;
        return true;
    }

    HRESULT
    SetBuffer(
        BYTE* pData,
        long Size,
        ArgType ReleaseArg,
        __int64 Timestamp,
        bool Discontinuity
        )
    {
        if (Discontinuity)
            {
            Reset();
            }

        HRESULT hr = BaseParser_t::SetBuffer(
            pData,
            Size,
            ReleaseArg,
            Timestamp,
            Discontinuity
            );
        if (FAILED(hr))
            {
            return hr;
            }

        if (Timestamp >= 0)
            {
            m_LastPeriod = 0;
            }

        return S_OK;
    }

    HRESULT
    Process()
    {
        AutoLogReturn_t LogReturn(
            L"DvrPsMux::Mpeg2AudioParser_t::Process --> EXITED "
            L"on line %i\r\n"
            );

        HRESULT hr = BaseParser_t::Process();
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, hr);
            }
        else if (S_FALSE == hr)
            {
            RETAILMSG(
                1,
                ( L"DvrPsMux::Mpeg2AudioParser_t::Process --> "
                  L"Waiting for the first TIMESTAMPED media "
                  L"sample following a parser reset, "
                  L"discarding current media sample\r\n" )
                );

            ASSERT( 1 == m_BufferList.size() );
            ReleaseAndPopFrontBuffer();
            return S_OK;
            }

        if (!m_pManager)
            {
            ASSERT(false);
            return LogReturn.Return(__LINE__, E_UNEXPECTED);
            }

        if (RpNone != m_RestorePoint)
            {
            RestorePoint_e RestorePoint = m_RestorePoint;
            m_RestorePoint = RpNone;

            switch (RestorePoint)
                {
                case RpId:
                    goto Restore_Id;
                case RpBitrateIndex:
                    goto Restore_BitrateIndex;
                default:
                    ASSERT(RpNone == RestorePoint);
                    // do nothing
                    break;
                }
            }

        while ( AdvanceToSyncword() )
            {
            if (m_LostSync)
                {
                // We didn't find a syncword where we expected
                // it
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Mpeg2AudioParser_t::Process "
                      L"--> detected bitsream corruption (syncword)\r\n" )
                    );

                // write out all the data up to this point, then
                // reset the parser so we can scan for a
                // syncword
                hr = Write();
                if (FAILED(hr))
                    {
                    return LogReturn.Return(__LINE__, hr);
                    }

                return S_FALSE;
                }

            if (m_NeedsTimestamp)
                {
                m_NeedsTimestamp = false;
                }

            m_Timestamp += m_LastPeriod;

            m_TimestampEntry.Offset = m_Parse.Position;
            m_TimestampEntry.Timestamp = m_Timestamp;
            bool Succeeded =
                m_PtsQueue.push_back(m_TimestampEntry);
            if (!Succeeded)
                {
                return LogReturn.Return(__LINE__, E_FAIL);
                }

            if (m_PtsQueue.size() > 100)
            {
                DEBUGMSG(
                    ZONE_INFO_MAX && ZONE_REFCOUNT,
                    ( L"DvrPsMux::Mpeg2AudioParser_t::Process --> "
                      L"Audio Timestamp Queue size = %i\r\n",
                      m_PtsQueue.size() )
                    );
            }

            if ( !DataAvailableAt(ID, 1) )
                {
                SetRestorePoint(RpId);
                break;
                }

    Restore_Id:

            BYTE Fields = GetByteAt(ID);

            BYTE Id = ExtractBits(Fields, 3, 3);
            m_Id = static_cast<AudioId_e>(Id);
            if ( m_Id != Mpeg1 &&
                 m_Id != Mpeg2 )
                {
                ASSERT(false);
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Mpeg2AudioParser_t::Process "
                      L"--> detected bitstream corruption (ID)\r\n" )
                    );

                // Reset parser, force syncword scan
                return S_FALSE;
                }

            BYTE Layer = ExtractBits(Fields, 2, 1);
            m_Layer = static_cast<Layer_e>(Layer);
            if ( m_Layer != Layer_I &&
                 m_Layer != Layer_II &&
                 m_Layer != Layer_III )
                {
                ASSERT(false);
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Mpeg2AudioParser_t::Process "
                      L"--> detected bitstream corruption (layer)\r\n" )
                    );

                // Reset parser, force syncword scan
                return S_FALSE;
                }

            if ( !DataAvailableAt(bitrate_index, 1) )
                {
                SetRestorePoint(RpBitrateIndex);
                break;
                }

    Restore_BitrateIndex:

            Fields = GetByteAt(bitrate_index);

            BYTE BitrateIndex = ExtractBits(Fields, 7, 4);
            if ( BitrateIndex < 0 ||
                 BitrateIndex > 14 )
                {
                ASSERT(false);
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Mpeg2AudioParser_t::Process "
                      L"--> detected bitstream corruption "
                      L"(bitrate_index)\r\n" )
                    );

                // Reset parser, force syncword scan
                return S_FALSE;
                }

            BYTE SampleRateCode = ExtractBits(Fields, 3, 2);
            if ( SampleRateCode < 0 ||
                 SampleRateCode > 2 )
                {
                ASSERT(false);
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Mpeg2AudioParser_t::Process "
                      L"--> detected bitstream corruption "
                      L"(sampling_frequency)\r\n" )
                    );

                // Reset parser, force syncword scan
                return S_FALSE;
                }

            BYTE PaddingBit = ExtractBits(Fields, 1, 1);

            m_LastPeriod =
                GetFramePeriod(m_Id, m_Layer, SampleRateCode);
            ASSERT(m_LastPeriod);

            m_FrameLength = GetFrameSize(
                m_Id,
                m_Layer,
                SampleRateCode,
                BitrateIndex,
                1 == PaddingBit
                );

            if (!m_FrameLength)
                {
                // variable bitrate; revert to syncword scanning
                AdvanceParsePosition(1);
                }

            // We've successfully parsed all the interesting
            // data associated with the current audio frame, so
            // now we can reset the syncword offset.
            m_Adjust = 0;
            }

        // we've reached the end of the input buffer, so write
        // out as much data as we can
        hr = Write();
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, hr);
            }

        ASSERT( (S_FALSE == hr && !m_Write.Position) ||
                (m_Write.Position == m_Write.pBuffer->Size) );
        ASSERT(m_Write.pBuffer == m_Parse.pBuffer);

        // we're finished parsing this buffer; release it and
        // remove it from the queue
        ASSERT(&m_BufferList.front() == m_Write.pBuffer);

        ReleaseAndPopFrontBuffer();
        m_Write.Reset();

        DEBUGMSG(
            ZONE_INFO_MAX,
            ( L"DvrPsMux::Mpeg2AudioParser_t::Process --> "
              L"processed valid audio sample\r\n" )
            );

        return S_OK;
    }
};


//
// ------------- Ac3Parser_t --------------
//

const short
FrameSize_Ac3_48kHz[19] =
{
     64,  80,  96, 112, 128, 160, 192,  224,  256,
    320, 384, 448, 512, 640, 768, 896, 1024, 1152, 1280
};

const short
FrameSize_Ac3_44_1kHz[38] =
{
     69,  70,   87,   88,  104,  105,  121,  122, 139, 140,
    174, 175,  208,  209,  243,  244,  278,  279, 348, 349,
    417, 418,  487,  488,  557,  558,  696,  697, 835, 836,
    975, 976, 1114, 1115, 1253, 1254, 1393, 1394
};

const short
FrameSize_Ac3_32kHz[19] =
{
     96, 120, 144,  168, 192,  240,  288,  336,  384,
    480, 576, 672,  768, 960, 1152, 1344, 1536, 1728, 1920
};

template<
    class Prototype,
    class ArgType,
    Prototype FnRelease,
    ArgType ArgNullVal
    >
class Ac3Parser_t :
    public Parser_t<Prototype, ArgType, FnRelease, ArgNullVal>
{
    enum Offsets_e
        {
        fscod = 4,
        frmsizecod = 4
        };

    enum SampleRate_e
        {
        Rate_48,
        Rate_44_1,
        Rate_32
        };

    enum RestorePoint_e
        {
        RpNone,
        RpSampleRateCode
        };

    RestorePoint_e m_RestorePoint;
    SampleRate_e   m_SampleRate;
    long           m_LastPeriod;
    long           m_FrameLength;
    long           m_Adjust;
    bool           m_FoundFirstFrame;
    bool           m_FoundSyncByte;
    bool           m_FirstWrite;
    bool           m_LostSync;
    AudioTimestampQueue_t m_PtsQueue;
    AudioTimestampEntry_t m_TimestampEntry;
    IAudioParserManager*  m_pManager;

    #if !defined(SHIP_BUILD)
    DWORD m_BufferNum;
    DWORD m_LastBufferNum;
    long m_LastOffset;
    __int64 m_LastTimestamp;
    #endif

    bool
    DataAvailableAt(
        long Offset,
        long Length = 0
        ) const
    {
        Offset -= m_Adjust;
        ASSERT(Offset >= 0);
        return BaseParser_t::DataAvailableAt(Offset, Length);
    }

    BYTE
    GetByteAt(
        long Offset
        )
    {
        Offset -= m_Adjust;
        ASSERT(Offset >= 0);
        return BaseParser_t::GetByteAt(Offset);
    }

    bool
    ScanForSyncword()
    {
        long BufferSize = m_Parse.pBuffer->Size;
        BYTE* pData = m_Parse.pBuffer->pData;

        for (int i = m_Parse.Position; i < BufferSize; i++)
            {
            if (0x0B == pData[i])
                {
                m_FoundSyncByte = true;
                }
            else if (0x77 == pData[i] && m_FoundSyncByte)
                {
                m_Adjust = 1;
                if (i > 0)
                    {
                    // If we found the '0x77' byte at position 0
                    // in the buffer it means the syncword is
                    // split across buffers. If we find it later
                    // in the buffer than the syncword was not
                    // split and we don't need to make any
                    // adjustments.
                    m_Adjust = 0;
                    m_FoundSyncByte = false;
                    }

                SetParsePosition( i - (1 - m_Adjust) );

                if (!m_FoundFirstFrame)
                    {
                    // we can't just advance the parse marker
                    // because then the offset for this frame
                    // would be wrong in the timestamp entry;
                    // instead we need to update the buffer
                    // pointer and rebase the parse and write
                    // markers, relative to the buffer pointer
                    m_FoundFirstFrame = true;
                    m_Parse.pBuffer->pData += m_Parse.Position;
                    m_Parse.pBuffer->Size -= m_Parse.Position;
                    SetWritePosition(0);
                    SetParsePosition(0);
                    }

                return true;
                }
            else
                {
                m_FoundSyncByte = false;
                }
            }

        return false;
    }

    bool
    AdvanceToSyncword()
    {
        if (m_BufferList.size() != 1)
            {
            ASSERT(false);
            return false;
            }

        // AC-3 frames always have fields to describe their
        // size. So we don't need to scan for the next syncword;
        // we can just advance the position marker based on
        // these fields. However the first buffer(s) in the
        // bitstream may not start on a clean syncword boundary,
        // so we do have to scan for the first syncword.
        if (!m_FrameLength)
            {
            return ScanForSyncword();
            }

        if ( !DataAvailableAt(m_FrameLength, 1) )
            {
            m_FrameLength -= GetRemainingLength();
            SetParsePosition(m_Parse.pBuffer->Size);
            return false;
            }

        AdvanceParsePosition(m_FrameLength);
        m_FrameLength = 0;

        BYTE* pData = m_Parse.pBuffer->pData;
        long Pos = m_Parse.Position;

        if ( pData[Pos] != 0x0B ||
             ( GetRemainingLength() > 1 &&
               pData[Pos + 1] != 0x77 ) )
            {
            ASSERT(false);
            m_LostSync = true;
            }

        return true;
    }

    bool
    SetRestorePoint(
        RestorePoint_e RestorePoint
        )
    {
        m_RestorePoint = RestorePoint;
        m_Adjust = GetRemainingLength();
        SetParsePosition(m_Parse.pBuffer->Size);
        return true;
    }

    static
    long
    GetFramePeriod(
        SampleRate_e SampleRate
        )
    {
        // Returns the period of one frame in 100-nanosecond
        // units. We calculate the value using:
        //  ( 1 / (<sampling frequency> / <samples per frame>) )
        //      * ( 10^9 / 100 )
        // ...where <samples per frame> is 1536 (see AC-3 spec,
        // section A2)
        long Rate;

        switch (SampleRate)
            {
            case Rate_48:
                Rate = 320000;
                break;
            case Rate_44_1:
                Rate = 348299;
                break;
            case Rate_32:
                Rate = 480000;
                break;
            default:
                ASSERT(false);
                Rate = 0;
            }

        return Rate;
    }

    long
    GetFrameSize(
        SampleRate_e SampleRate,
        long FrameSizeCode
        )
    {
        short FrameWords = 0;

        switch (SampleRate)
            {
            case Rate_48:
                FrameSizeCode >>= 1;
                FrameWords = FrameSize_Ac3_48kHz[FrameSizeCode];
                break;
            case Rate_44_1:
                FrameWords = FrameSize_Ac3_44_1kHz[FrameSizeCode];
                break;
            case Rate_32:
                FrameSizeCode >>= 1;
                FrameWords = FrameSize_Ac3_32kHz[FrameSizeCode];
                break;
            default:
                ASSERT(false);
                return 0;
            }

        return ( (static_cast<long>(FrameWords) * 2) - m_Adjust );
    }

    HRESULT
    Write()
    {
        AutoLogReturn_t LogReturn(
            L"DvrPsMux::Ac3Parser_t::Write --> EXITED on "
            L"line %i\r\n"
            );

        IAudioParserWriter* pWriter = NULL;

        if (m_FirstWrite)
            {
            // This is our first opportunity to mux audio into
            // the output stream, so we need to make sure we
            // begin on an audio frame boundary.
            if (m_PtsQueue.size() == 0)
                {
                return S_FALSE;
                }

            // Try to get a writer. If GetWriter() returns
            // S_FALSE it means the mux won't let us write yet.
            ASSERT(m_pManager);
            HRESULT hr = m_pManager->GetWriter(&pWriter);
            if (S_FALSE == hr)
                {
                // discard any timestamp data in the queue
                m_PtsQueue.clear();
                return S_FALSE;
                }
            else if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            // Find the frame boundary to begin writing from
            AudioTimestampEntry_t& Entry = m_PtsQueue.front();
            ASSERT(m_Parse.Position >= Entry.Offset);
            ASSERT(m_Write.Position <= Entry.Offset);
            SetWritePosition(Entry.Offset);

            m_FirstWrite = false;
            ASSERT(pWriter);
            }
        else
            {
            ASSERT(m_pManager);
            HRESULT hr = m_pManager->GetWriter(&pWriter);
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            ASSERT(S_FALSE != hr && pWriter);
            }

        HRESULT hr = S_OK;
        long BytesWritten = 0;

        while (m_FoundSyncByte)
            {
            // this will only happen if the first audio frame in
            // the bitstream does not begin on a buffer
            // boundary; after that we won't need to scan for
            // syncwords
            BYTE SyncByte = 0x0B;
            hr = pWriter->Write(
                &SyncByte,
                0,
                1,
                &m_PtsQueue,
                &BytesWritten
                );
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            AdvanceWritePosition(BytesWritten);

            if (BytesWritten < 1)
                {
                DEBUGMSG(
                    ZONE_INFO_MAX,
                    ( L"DvrPsMux::Ac3Parser_t::Write --> "
                      L"completed audio pack\r\n" )
                    );

                pWriter = NULL;
                hr = m_pManager->GetWriter(&pWriter);
                if (FAILED(hr))
                    {
                    return LogReturn.Return(__LINE__, hr);
                    }

                ASSERT(S_FALSE != hr && pWriter);
                continue;
                }

            m_FoundSyncByte = false;
            }

        ASSERT(m_Parse.pBuffer == m_Write.pBuffer);

        long WriteSize = m_Parse.Position - m_Write.Position;

        if (WriteSize < 0)
            {
            ASSERT(false);
            return LogReturn.Return(__LINE__, E_FAIL);
            }

        while (WriteSize)
            {
            hr = pWriter->Write(
                m_Write.pBuffer->pData,
                m_Write.Position,
                WriteSize,
                &m_PtsQueue,
                &BytesWritten
                );
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            WriteSize -= BytesWritten;
            AdvanceWritePosition(BytesWritten);

            if (!WriteSize)
                {
                break;
                }

            DEBUGMSG(
                ZONE_INFO_MAX,
                ( L"DvrPsMux::Ac3Parser_t::Write --> completed "
                  L"audio pack\r\n" )
                );

            pWriter = NULL;
            hr = m_pManager->GetWriter(&pWriter);
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, hr);
                }

            ASSERT(S_FALSE != hr && pWriter);
            }

        return S_OK;
    }

public:
    void
    Reset()
    {
        BaseParser_t::Reset();

        m_RestorePoint = RpNone;
        m_LastPeriod = 0;
        m_FrameLength = 0;
        m_Adjust = 0;
        m_FoundFirstFrame = false;
        m_FoundSyncByte = false;
        m_FirstWrite = true;
        m_LostSync = false;
        m_TimestampEntry.Reset();
        m_PtsQueue.clear();
    }

    Ac3Parser_t() :
        m_SampleRate(Rate_44_1),
        m_pManager(NULL)
        #if !defined(SHIP_BUILD)
        ,m_BufferNum((DWORD)-1),
        m_LastBufferNum(-2),
        m_LastOffset(-1),
        m_LastTimestamp(0)
        #endif
    {
        Reset();
    }

    bool
    SetParserManager(
        IAudioParserManager* pManager
        )
    {
        if (!pManager)
            {
            return false;
            }

        m_pManager = pManager;
        return true;
    }

    HRESULT
    SetBuffer(
        BYTE* pData,
        long Size,
        ArgType ReleaseArg,
        __int64 Timestamp,
        bool Discontinuity
        )
    {
        if (Discontinuity)
            {
            Reset();
            }

        ASSERT( 0 == m_BufferList.size() );

        HRESULT hr = BaseParser_t::SetBuffer(
            pData,
            Size,
            ReleaseArg,
            Timestamp,
            Discontinuity
            );
        if (FAILED(hr))
            {
            return hr;
            }

        if (Timestamp >= 0)
            {
            m_LastPeriod = 0;
            }

        #if !defined(SHIP_BUILD)
        m_BufferNum++;
        #endif

        return S_OK;
    }

    HRESULT
    Process()
    {
        AutoLogReturn_t LogReturn(
            L"DvrPsMux::Ac3Parser_t::Process --> EXITED on "
            L"line %i\r\n"
            );

        HRESULT hr = BaseParser_t::Process();
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, hr);
            }
        else if (S_FALSE == hr)
            {
            RETAILMSG(
                1,
                ( L"DvrPsMux::Ac3Parser_t::Process --> "
                  L"Waiting for the first TIMESTAMPED media "
                  L"sample following a parser reset, "
                  L"discarding current media sample\r\n" )
                );

            ASSERT( 1 == m_BufferList.size() );
            ReleaseAndPopFrontBuffer();
            return S_OK;
            }

        if (!m_pManager)
            {
            ASSERT(false);
            return LogReturn.Return(__LINE__, E_UNEXPECTED);
            }

        if (RpNone != m_RestorePoint)
            {
            RestorePoint_e RestorePoint = m_RestorePoint;
            m_RestorePoint = RpNone;

            switch (RestorePoint)
                {
                case RpSampleRateCode:
                    goto Restore_SampleRateCode;
                default:
                    ASSERT(RpNone == RestorePoint);
                    // do nothing
                    break;
                }
            }

        while ( AdvanceToSyncword() )
            {
            if (m_LostSync)
                {
                // We didn't find a syncword where we expected
                // it
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Ac3Parser_t::Process --> "
                      L"detected bitstream corruption (syncword)\r\n" )
                    );

                // write out all the data up to this point, then
                // reset the parser so we can scan for a
                // syncword
                hr = Write();
                if (FAILED(hr))
                    {
                    return LogReturn.Return(__LINE__, hr);
                    }

                return S_FALSE;
                }

            if (m_NeedsTimestamp)
                {
                m_NeedsTimestamp = false;
                }

            m_Timestamp += m_LastPeriod;

            #if !defined(SHIP_BUILD)
            RETAILMSG(
                m_Timestamp < m_LastTimestamp,
                ( L"DvrPsMux::Ac3Parser_t::Process --> Current "
                  L"timestamp is BEFORE previous timestamp!! "
                  L"(prev=%i, cur=%i\r\n",
                  static_cast<long>(m_LastTimestamp/10000),
                  static_cast<long>(m_Timestamp/10000) )
                );

            m_LastTimestamp = m_Timestamp;
            #endif

            m_TimestampEntry.Offset = m_Parse.Position;
            m_TimestampEntry.Timestamp = m_Timestamp;
            bool Succeeded =
                m_PtsQueue.push_back(m_TimestampEntry);
            if (!Succeeded)
                {
                return LogReturn.Return(__LINE__, E_FAIL);
                }

            #if !defined(SHIP_BUILD)
            if ( m_LastBufferNum == m_BufferNum &&
                 m_LastOffset == m_TimestampEntry.Offset )
                {
                // processed the same frame twice!!
                ASSERT(false);
                RETAILMSG(
                    1,
                    ( L"### Processed same AC-3 frame twice!!! "
                      L"(buf#%i, addr=%#x, offset=%i)\r\n",
                      m_BufferNum,
                      m_Parse.pBuffer->pData,
                      m_TimestampEntry.Offset )
                    );
                }

            m_LastBufferNum = m_BufferNum;
            m_LastOffset = m_TimestampEntry.Offset;
            #endif

            if (m_PtsQueue.size() > 100)
            {
                DEBUGMSG(
                    ZONE_INFO_MAX && ZONE_REFCOUNT,
                    ( L"DvrPsMux::Ac3Parser_t::Process --> "
                      L"Audio Timestamp Queue size = %i\r\n",
                      m_PtsQueue.size() )
                    );
            }

            if ( !DataAvailableAt(fscod, 1) )
                {
                SetRestorePoint(RpSampleRateCode);
                break;
                }

    Restore_SampleRateCode:

            BYTE SampleRateCode = ExtractBits(
                GetByteAt(fscod),
                7, 6
                );

            m_SampleRate = static_cast<SampleRate_e>(
                SampleRateCode
                );
            if ( m_SampleRate != Rate_48 &&
                 m_SampleRate != Rate_44_1 &&
                 m_SampleRate != Rate_32 )
                {
                ASSERT(false);
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Ac3Parser_t::Process --> "
                      L"detected bitstream corruption (fscod)\r\n" )
                    );

                // Reset parser, force syncword scan
                return S_FALSE;
                }

            m_LastPeriod = GetFramePeriod(m_SampleRate);

            BYTE FrameSizeCode = ExtractBits(
                GetByteAt(frmsizecod),
                5, 0
                );
            if ( FrameSizeCode < 0 &&
                 FrameSizeCode > 37 )
                {
                ASSERT(false);
                RETAILMSG(
                    1,
                    ( L"DvrPsMux::Ac3Parser_t::Process --> "
                      L"detected bitstream corruption (frmsizecod)\r\n" )
                    );

                // Reset parser, force syncword scan
                return S_FALSE;
                }

            m_FrameLength = GetFrameSize(
                m_SampleRate,
                FrameSizeCode
                );

            if (!m_FrameLength)
                {
                RETAILMSG(
                    1,
                    ( L"### AC-3 frame length is 0!!!\r\n" )
                    );
                RETAILMSG(
                    1,
                    ( L"Byte sequence:\r\n"
                      L"  syncword: %2x %2x\r\n"
                      L"  fscod:    %2x\r\n"
                      L"  frmszcod: %2x\r\n",
                      GetByteAt(0), GetByteAt(1),
                      SampleRateCode,
                      FrameSizeCode )
                    );

                // Reset parser, force syncword scan
                return S_FALSE;
                }

            // We've successfully parsed all the interesting
            // data associated with the current audio frame, so
            // now we can reset the syncword offset.
            m_Adjust = 0;
            }

        // we've reached the end of the input buffer, so write
        // out as much data as we can
        hr = Write();
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, hr);
            }

        ASSERT( (S_FALSE == hr && !m_Write.Position) ||
                (m_Write.Position == m_Write.pBuffer->Size) );
        ASSERT(m_Write.pBuffer == m_Parse.pBuffer);

        // we're finished parsing this buffer; release it and
        // remove it from the queue
        ASSERT(&m_BufferList.front() == m_Write.pBuffer);

        ReleaseAndPopFrontBuffer();
        m_Write.Reset();

        DEBUGMSG(
            ZONE_INFO_MAX,
            ( L"DvrPsMux::Ac3Parser_t::Process --> processed "
              L"valid audio sample\r\n" )
            );

        return S_OK;
    }
};

}

