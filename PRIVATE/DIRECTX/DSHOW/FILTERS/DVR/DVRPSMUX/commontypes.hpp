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
#include <block_allocator.hxx>
#include <list.hxx>

namespace DvrPsMux
{

const long f_PackSize = 2048;
const long f_PackHeaderSize = 14;
const long f_SystemHeaderSize = 18;

// The PES header size includes room for PTS, DTS,
// P-STD_buffer_scale and P-STD_buffer_size, plus AC-3-specific
// fields. If we don't code all these fields into a particular
// packet header, then we'll use stuffing bytes to pad it out.
const long f_PesHeaderSize = 22;

enum PictureCodingType_e
{
    None,
    // these values match the values in the picture_coding_type
    // field
    IntraCoded = 1,
    PredictiveCoded,
    BidirectionallyPredictiveCoded
};

enum StreamType_e
{
    Mpeg2Video,
    Mpeg2Audio,
    Ac3Audio
};

struct PictureInfoEntry_t
{
    long                Offset;
    unsigned long       Reserved;
    PictureCodingType_e CodingType;
    long                DtsOffset;
    __int64             Pts;

    PictureInfoEntry_t() :
        Offset(-1),
        Reserved(0),
        CodingType(None),
        DtsOffset(-1),
        Pts(-1)
    {
    }

    void
    Reset()
    {
        Offset = -1;
        Reserved = 0;
        CodingType = None;
        DtsOffset = -1;
        Pts = -1;
    }

    bool
    IsComplete()
    {
        return (
            (Offset >= 0)        &&
            (CodingType != None) &&
            (DtsOffset != -1)    &&
            (Pts >= 0)
            );
    }

    bool
    OnlyNeedsTimestamp()
    {
        return (
            (Offset >= 0)        &&
            (CodingType != None) &&
            (DtsOffset == -1)
            );
    }
};

struct AudioTimestampEntry_t
{
    long Offset;
    unsigned long Reserved;
    __int64 Timestamp;

    AudioTimestampEntry_t() :
        Offset(0),
        Reserved(0),
        Timestamp(0)
    {
    }

    void
    Reset()
    {
        Offset = 0;
        Reserved = 0;
        Timestamp = 0;
    }
};

// allocators
typedef
    ce::free_list< 1024, ce::fixed_block_allocator<8> >
    Fixed8FreeList1024_t;

// queues
typedef
    ce::list<PictureInfoEntry_t, Fixed8FreeList1024_t> 
    PictureInfoQueue_t;

typedef
    ce::list<AudioTimestampEntry_t, Fixed8FreeList1024_t>
    AudioTimestampQueue_t;

template<class Entry, class Queue>
interface IParserWriter
{
    virtual
    HRESULT
    Write(
        BYTE* pSrc,
        long  Offset,
        long  Size,
        Queue* pQueue,
        long* pBytesWritten
        ) = 0;
};

enum ParserManagerEvent_e
{
    FirstVideoSequence
};

template<class Entry, class Queue>
interface IParserManager
{
    virtual
    HRESULT
    GetWriter(
        IParserWriter<Entry, Queue>** ppWriter
        ) = 0;

    virtual
    HRESULT
    Flush() = 0;

    virtual
    bool
    IsOutputBufferEmpty() = 0;

    virtual
    bool
    AmmendTailEntry(
        Entry& Entry
        ) = 0;

    virtual
    void
    Notify(
        ParserManagerEvent_e Event
        ) = 0;
};

typedef
    IParserWriter<PictureInfoEntry_t, PictureInfoQueue_t>
    IVideoParserWriter;

typedef
    IParserWriter<AudioTimestampEntry_t, AudioTimestampQueue_t>
    IAudioParserWriter;

typedef
    IParserManager<PictureInfoEntry_t, PictureInfoQueue_t>
    IVideoParserManager;

typedef
    IParserManager<AudioTimestampEntry_t, AudioTimestampQueue_t>
    IAudioParserManager;

#if !defined(SHIP_BUILD)
class AutoLogReturn_t
{
    long m_LineNumber;
    const wchar_t* m_pFormatString;

    AutoLogReturn_t() :
        m_LineNumber(-1),
        m_pFormatString(L" ")
    {
    }

public:
    AutoLogReturn_t(
        wchar_t* pFormatString
        ) :
            m_LineNumber(-1),
            m_pFormatString(pFormatString)
    {
        ASSERT(m_pFormatString);
    }

    ~AutoLogReturn_t()
    {
        // this assert will fire if the user doesn't set the
        // line number before returning
        if (m_LineNumber >= 0)
            {
            DisplayMessage();
            }
    }

    void
    SetLineNumber(
        long LineNumber
        )
    {
        ASSERT(m_LineNumber < 0 && LineNumber >= 0);
        m_LineNumber = LineNumber;
    }

    void
    DisplayMessage()
    {
        RETAILMSG(
            1,
            ( m_pFormatString,
              m_LineNumber )
            );
    }

    template <typename T>
    T
    Return(
        long LineNumber,
        T ReturnValue
        )
    {
        SetLineNumber(LineNumber);
        return ReturnValue;
    }
};
#else
class AutoLogReturn_t
{
    AutoLogReturn_t()
    {
    }

public:
    AutoLogReturn_t(
        wchar_t* pFormatString
        )
    {
    }

    void
    SetLineNumber(
        long LineNumber
        )
    {
    }

    void
    DisplayMessage()
    {
    }

    template <typename T>
    T
    Return(
        long LineNumber,
        T ReturnValue
        )
    {
        return ReturnValue;
    }
};
#endif

}

