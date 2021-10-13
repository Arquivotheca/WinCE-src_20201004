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

#include "psmux.hpp"

using namespace DvrPsMux;


template<typename T>
inline
BYTE
MakeByte(T Value)
{
    ASSERT( Value == (Value & 0xFF) );
    return static_cast<BYTE>(Value);
}

void
CloseQueue(
    InputBufferQueue_t* pQueue
    )
{
    if (!pQueue)
        {
        return;
        }

    // Block further writes to the queue
    pQueue->Close();

    // Release all the remaining samples in the queue
    while (pQueue->Size() > 0)
        {
        SampleEntry_t& Entry = pQueue->Front();

        if (Entry.pSample)
            {
            Entry.pSample->Release();
            }

        pQueue->PopFront();
        }
}

typedef ce::auto_xxx<
    InputBufferQueue_t*,
    void (*)(InputBufferQueue_t*),
    CloseQueue,
    NULL
    > AutoCloseQueue_t;


//
// ----------------- Mux_t ----------------
//

DWORD
__cdecl
Mux_t::
MuxThreadProc(
  void* pArg
  )
{
    AutoLogReturn_t LogReturn(
        L"DvrPsMux::Mux_t::MuxThreadProc --> "
        L"EXITED on line %i\r\n"
        );

    Mux_t* pMux = reinterpret_cast<Mux_t*>(pArg);
    if (!pMux)
        {
        ASSERT(false);
        return LogReturn.Return(__LINE__, 1);
        }

    HKEY hKey;
    long ReturnCode = RegOpenKeyEx(
        HKEY_CLASSES_ROOT,
        L"CLSID\\{E9FA2A46-8CD8-4A5C-AE52-B82378703A69}",
        0,
        0,
        &hKey);
    if (ReturnCode == ERROR_SUCCESS && hKey != NULL)
        {
        // dwThreadPriority shouldn't needed to be initialized,
        // but just to be safe...
        DWORD dwThreadPriority = 248 + THREAD_PRIORITY_NORMAL;
        DWORD dwSize = sizeof(dwThreadPriority);
        DWORD dwType;
        ReturnCode = RegQueryValueEx(
            hKey,
            L"ThreadPriority",
            0,
            &dwType,
            reinterpret_cast<BYTE*>(&dwThreadPriority),
            &dwSize);

        if ( ERROR_SUCCESS == ReturnCode &&
             REG_DWORD == dwType &&
             dwThreadPriority < 256 )
            {
            CeSetThreadPriority(
                GetCurrentThread(),
                dwThreadPriority
                );
            }

        RegCloseKey(hKey);
        }

    ReturnCode = pMux->MuxWorker();

    if (ReturnCode != 0)
        {
        RETAILMSG(
            1,
            ( L"DvrPsMux::Mux_t::MuxThreadProc --> MuxWorker "
              L"thread returned %i, sending "
              L"EC_STREAM_ERROR_STOPPED event\r\n",
              ReturnCode )
            );

        HRESULT hr = pMux->m_pFilter->NotifyEvent(
            EC_STREAM_ERROR_STOPPED,
            E_FAIL,
            0
            );

        RETAILMSG(
            FAILED(hr),
            ( L"DvrPsMux::Mux_t::MuxThreadProc --> NotifyEvent "
              L"FAILED! (%#x)\r\n",
              hr )
            );
        }

    return ReturnCode;
}

DWORD
Mux_t::
MuxWorker()
{
    AutoLogReturn_t LogReturn(
        L"DvrPsMux::Mux_t::MuxWorker --> "
        L"EXITED on line %i\r\n"
        );

    // If we exit this thread prematurely, this object ensures
    // that we'll signal back to the main thread that something
    // is wrong by shutting down the queue. Future appends from
    // the input threads will fail. Of course this object will
    // also close the queue in the case where we exit correctly,
    // but in that case the graph is stopped so it doesn't
    // matter.
    AutoCloseQueue_t CloseQueue(&m_InputQueue);

    HANDLE Exit = m_ExitWorker;
    DWORD WaitResult = WaitForSingleObject(Exit, 0);

    // Note: This thread must not block--we need to always be
    // able to detect the ExitWorker event.
    while (WAIT_TIMEOUT == WaitResult)
        {
        SampleEntry_t* pEntry =
            m_InputQueue.WaitFront(Exit);
        if (!pEntry)
            {
            return LogReturn.Return(__LINE__, 0);
            }

        if ( Mpeg2Video != pEntry->Type &&
             m_AudioType != pEntry->Type )
            {
            // we don't support dynamically renegotiating the
            // audio type
            if (m_pAudioParser)
                {
                ASSERT(false);
                return LogReturn.Return(__LINE__, 1);
                }

            bool Succeeded = SetAudioType(pEntry->Type);
            ASSERT(Succeeded);
            }

        if (Mpeg2Video != pEntry->Type && !m_pAudioParser)
            {
            IAudioParserManager* pManager =
                dynamic_cast<IAudioParserManager*>(this);

            if (!pManager)
                {
                ASSERT(false);
                return LogReturn.Return(__LINE__, 1);
                }

            if (Mpeg2Audio == m_AudioType)
                {
                Mpeg2AudioSampleParser_t* pParser =
                    new Mpeg2AudioSampleParser_t;

                if (!pParser)
                    {
                    return LogReturn.Return(__LINE__, 1);
                    }

                bool Succeeded =
                    pParser->SetParserManager(pManager);
                ASSERT(Succeeded);

                m_pAudioParser = pParser;
                }
            else if (Ac3Audio == m_AudioType)
                {
                 Ac3SampleParser_t* pParser =
                    new Ac3SampleParser_t;

                if (!pParser)
                    {
                    return LogReturn.Return(__LINE__, 1);
                    }

                bool Succeeded =
                    pParser->SetParserManager(pManager);
                ASSERT(Succeeded);

                m_pAudioParser = pParser;
                }
            }
        else if ( Mpeg2Video == pEntry->Type &&
                  !m_pVideoParser )
            {
            Mpeg2VideoSampleParser_t* pParser =
                new Mpeg2VideoSampleParser_t;

            if (!pParser)
                {
                return LogReturn.Return(__LINE__, 1);
                }

            bool Succeeded = pParser->SetParserManager(
                dynamic_cast<IVideoParserManager*>(this)
                );
            ASSERT(Succeeded);

            m_pVideoParser = pParser;
            }

#if 0
        DWORD Tick = GetTickCount();

        if (Mpeg2Video == pEntry->Type)
            {
            static DWORD s_LastVideoTick = Tick;
            static DWORD s_MaxVideoLatency = 0;

            DWORD Latency = Tick - s_LastVideoTick;
            RETAILMSG(
                0,
                ( L"@@@ Video Latency = %u\r\n", Latency )
                );

            if (Latency > s_MaxVideoLatency)
                {
                s_MaxVideoLatency = Latency;
                RETAILMSG(
                    1,
                    ( L"@@@ Max Video Latency = %u\r\n",
                        s_MaxVideoLatency )
                    );
                }

            s_LastVideoTick = Tick;
            }
        else
            {
            static DWORD s_LastAudioTick = Tick;
            static DWORD s_MaxAudioLatency = 0;

            DWORD Latency = Tick - s_LastAudioTick;
            RETAILMSG(
                0,
                ( L"@@@ Audio Latency = %u\r\n", Latency )
                );

            if (Latency > s_MaxAudioLatency)
                {
                s_MaxAudioLatency = Latency;
                RETAILMSG(
                    1,
                    ( L"@@@ Max Audio Latency = %u\r\n",
                        s_MaxAudioLatency )
                    );
                }

            s_LastAudioTick = Tick;
            }

        pEntry->pSample->Release();
        m_InputQueue.PopFront();

        RETAILMSG(
            1,
            ( L"@@@ MuxWorker: got %s sample\r\n",
                Mpeg2Video == pEntry->Type
                ? L"video"
                : L"audio" )
            );

        continue;
#endif

        IMediaSample* pSample = pEntry->pSample;
        if (!pSample)
            {
            ASSERT(false);
            return LogReturn.Return(__LINE__, 1);
            }

        BYTE* pBuffer;
        HRESULT hr = pSample->GetPointer(&pBuffer);
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, 1);
            }

        long Length = pSample->GetActualDataLength();
        ASSERT(Length);

        bool IsDiscontinuity = false;
        hr = pSample->IsDiscontinuity();
        if (S_OK == hr)
            {
            IsDiscontinuity = true;
            hr = Discontinuity(pEntry->Type);
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, 1);
                }
            }

        REFERENCE_TIME StartTime;
        REFERENCE_TIME EndTime;
        hr = pSample->GetTime(&StartTime, &EndTime);
        if (FAILED(hr))
            {
            ASSERT(VFW_E_SAMPLE_TIME_NOT_SET == hr);
            if (IsDiscontinuity)
                {
                // first sample on a discontinuity must have
                // a timestamp
                return LogReturn.Return(__LINE__, 1);
                }

            StartTime = -1;
            }

        SampleParser_t* pParser = (Mpeg2Video == pEntry->Type)
                ? m_pVideoParser
                : m_pAudioParser;

        hr = pParser->SetBuffer(
            pBuffer,
            Length,
            pSample,
            StartTime,
            IsDiscontinuity
            );
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, 1);
            }

        m_InputQueue.PopFront();

        hr = pParser->Process();
        if (FAILED(hr))
            {
            return LogReturn.Return(__LINE__, 1);
            }
        else if (S_FALSE == hr)
            {
            // parser requested a reset (probably encountered
            // bitstream corruption)
            pParser->Reset();

            // Discontinuity call ifdef'd out because, for
            // example, a reset in the audio parser will cause
            // a discontinuity to be set on the program stream,
            // which will eventually cause BOTH the audio and
            // video decoders to reset...so an audio parser
            // reset will result in a video glitch--we don't
            // want that to happen.
            #if 0
            hr = Discontinuity(pEntry->Type);
            if (FAILED(hr))
                {
                return LogReturn.Return(__LINE__, 1);
                }
            #endif

            RETAILMSG(
                1,
                ( L"DvrPsMux::Mux_t::MuxWorker --> reset %s "
                  L"parser\r\n",
                  Mpeg2Video == pEntry->Type
                    ? L"video" : L"audio" )
                );
            }

        WaitResult = WaitForSingleObject(m_ExitWorker, 0);
        }

    return LogReturn.Return(__LINE__, 0);
}

bool
Mux_t::
OnNewAccessUnit(
    PictureInfoEntry_t& Entry
    )
{
    // associate the picture info with a particular output
    // buffer so we have a context for the offset
    ASSERT(!Entry.Reserved);
    Entry.Reserved = m_OutputEntry.Index;

    bool Succeeded = m_PictureInfoQueue.push_back(Entry);
    ASSERT(Succeeded);

    if (m_PictureInfoQueue.size() > 100)
    {
        DEBUGMSG(
            LOG_ERROR, 
            ( L"DvrPsMux::Mux_t::OnNewAccessUnit %s--> Picture "
              L"Info Queue size = %i\r\n",
              Succeeded ? L"" : L"FAILED ",
              m_PictureInfoQueue.size() )
            );
    }

    return Succeeded;
}

bool
Mux_t::
OnNewAccessUnit(
    AudioTimestampEntry_t& Entry
    )
{
    // associate the audio timestamp entry with a particular
    // output buffer so we have a context for the offset
    ASSERT(!Entry.Reserved);
    Entry.Reserved = m_OutputEntry.Index;

    bool Succeeded = m_AudioPtsQueue.push_back(Entry);
    ASSERT(Succeeded);

    if (m_AudioPtsQueue.size() > 100)
    {
        DEBUGMSG(
            ZONE_INFO_MAX && ZONE_REFCOUNT,
            ( L"DvrPsMux::Mux_t::OnNewAccessUnit %s--> Audio "
              L"Timestamp Queue size = %i\r\n",
              Succeeded ? L"" : L"FAILED ",
              m_AudioPtsQueue.size() )
            );
    }

    return Succeeded;
}

bool
Mux_t::
AmmendTailEntry(
    PictureInfoEntry_t& Entry
    )
{
    AutoLogReturn_t LogReturn(
        L"DvrPsMux::Mux_t::AmmendTailEntry --> EXITED on "
        L"line %i\r\n"
        );

    if (m_PictureInfoQueue.size() == 0)
        {
        ASSERT(false);
        return LogReturn.Return(__LINE__, false);
        }

    PictureInfoEntry_t& Last = m_PictureInfoQueue.back();

    ASSERT(
        ( Last.Offset >= 0 ) &&
        ( Last.Reserved <= m_OutputEntry.Index ) &&
        ( !Last.IsComplete() )
        );

    ASSERT( Entry.IsComplete() );

    Last.CodingType = Entry.CodingType;
    Last.DtsOffset  = Entry.DtsOffset;
    Last.Pts        = Entry.Pts;

    // now that we've received the picture information we were
    // waiting for, we can deliver the buffer associated with
    // this picture
    if (m_OutputBufferQueue.size() > 0)
        {
        if ( !DeliverSample() )
            {
            return LogReturn.Return(__LINE__, false);
            }
        }

    return true;
}

HRESULT
Mux_t::
OnWriterFinished()
{
    AutoLogReturn_t LogReturn(
        L"DvrPsMux::Mux_t::OnWriterFinished --> "
        L"EXITED on line %i\r\n"
        );

    if (!m_OutputEntry.pSample || m_pNextPack)
        {
        // if there's no sample buffer, or there is a buffer
        // and it's still writeable (not full and flush not
        // requested), then exit--we aren't ready to complete
        // this buffer yet
        return S_OK;
        }

    // queue this output sample and move on
    IMediaSample* pSample = m_OutputEntry.pSample;
    ASSERT(pSample);

    BYTE* pBuffer = NULL;
    HRESULT hr = pSample->GetPointer(&pBuffer);
    if (FAILED(hr))
        {
        return LogReturn.Return(__LINE__, hr);
        }

    // In the case where we complete a sample early in order to
    // start a new I-frame sample, the last video pack may be
    // empty. This happens when the I-picture naturally begins
    // on a pack boundary. If there's already an audio pack
    // being written AFTER the empty video pack, then we just
    // need to pad out the video pack so we don't leave a hole.
    // If the empty video pack is last, we can discard it to
    // minimize waste.
    long LastOffset = max(
        m_AudioWriter.m_PackOffset,
        m_VideoWriter.m_PackOffset
        );

    if ( (m_VideoWriter.m_Pos == m_VideoWriter.m_HeaderSize) &&
         (LastOffset == m_VideoWriter.m_PackOffset) )
        {
        LastOffset = m_AudioWriter.m_PackOffset;
        }

    long BufferLength = LastOffset + f_PackSize;
    ASSERT( BufferLength <= pSample->GetSize() );
    ASSERT( BufferLength % f_PackSize == 0 );

    hr = pSample->SetActualDataLength(BufferLength);
    if (FAILED(hr))
        {
        return LogReturn.Return(__LINE__, hr);
        }

    hr = pSample->SetDiscontinuity(
        m_Discontinuity ? TRUE : FALSE
        );
    if (FAILED(hr))
        {
        return LogReturn.Return(__LINE__, hr);
        }

    if ( m_VideoWriter.IsPackSet() &&
         m_VideoWriter.m_Pos > m_VideoWriter.m_HeaderSize )
        {
        ASSERT(m_VideoWriter.m_Pos <= f_PackSize);

        PackInfo_t* pInfo =
            reinterpret_cast<PackInfo_t*>(m_VideoWriter.m_pPack);
        pInfo->LengthHi =
            MakeByte((m_VideoWriter.m_Pos >> 8) & 0xFF);
        pInfo->LengthLo =
            MakeByte(m_VideoWriter.m_Pos & 0xFF);
        }

    if ( m_AudioWriter.IsPackSet() &&
         m_AudioWriter.m_Pos > m_AudioWriter.m_HeaderSize )
        {
        ASSERT(m_AudioWriter.m_Pos <= f_PackSize);

        PackInfo_t* pInfo =
            reinterpret_cast<PackInfo_t*>(m_AudioWriter.m_pPack);
        pInfo->LengthHi =
            MakeByte((m_AudioWriter.m_Pos >> 8) & 0xFF);
        pInfo->LengthLo =
            MakeByte(m_AudioWriter.m_Pos & 0xFF);
        }

    if ( m_Segment.IsComplete() )
        {
        ASSERT( !m_SetSegment.IsComplete() );
        m_OutputEntry.Segment = m_Segment;
        }

    __int64 Start = -1;
    __int64 End = -1;
    if (m_pOutputPin->SinkIsFileWriter())
        {
        // Start is the byte offset in the stream for the first
        // byte in this buffer, End is the length of the buffer
        Start = m_StreamPosition;
        End = m_StreamPosition + BufferLength;
        }

    hr = pSample->SetTime(
        (Start >= 0) ? &Start : NULL,
        (End >= 0) ? &End : NULL
        );
    if (FAILED(hr))
        {
        return LogReturn.Return(__LINE__, hr);
        }

    hr = pSample->SetMediaTime(NULL, NULL);
    if (FAILED(hr))
        {
        return LogReturn.Return(__LINE__, hr);
        }

    bool Succeeded =
        m_OutputBufferQueue.push_back(m_OutputEntry);
    if (!Succeeded)
        {
        return LogReturn.Return(__LINE__, E_FAIL);
        }

    m_OutputEntry.pSample = NULL;
    m_Discontinuity = false;
    m_Flush = false;
    m_StreamPosition += BufferLength;
    m_Segment.Reset();

    if (m_SetDiscontinuity)
        {
        m_SetDiscontinuity = false;
        ASSERT( m_SetDiscontinuityType == Mpeg2Video ||
                m_SetDiscontinuityType == m_AudioType );
        OnDiscontinuity(m_SetDiscontinuityType);
        }

    if ( m_SetSegment.IsComplete() )
        {
        ASSERT( !m_Segment.IsComplete() );
        m_Segment = m_SetSegment;
        m_SetSegment.Reset();
        }

    // we've queued the old buffer, so get a new one ready
    hr = GetBuffer();
    if (FAILED(hr))
        {
        return LogReturn.Return(__LINE__, hr);
        }

    PictureInfoEntry_t& Tail = m_PictureInfoQueue.back();
    if ( m_PictureInfoQueue.size() > 0 && !Tail.IsComplete() )
        {
        // we'll wait to deliver the sample until the picture
        // info is ammended
        return S_OK;
        }

    while (m_OutputBufferQueue.size() > 0)
        {
        if ( !DeliverSample() )
            {
            return LogReturn.Return(__LINE__, E_FAIL);
            }
        }

    return S_OK;
}

bool
Mux_t::
DeliverSample()
{
    AutoLogReturn_t LogReturn(
        L"DvrPsMux::Mux_t::DeliverSample --> "
        L"EXITED on line %i\r\n"
        );

    if (m_OutputBufferQueue.size() == 0)
        {
        ASSERT(false);
        return LogReturn.Return(__LINE__, false);
        }

    OutputBufferEntry_t& OutputEntry =
        m_OutputBufferQueue.front();

    IMediaSample* pSample = OutputEntry.pSample;
    ASSERT(pSample);

    BYTE* pBuffer = NULL;
    HRESULT hr = pSample->GetPointer(&pBuffer);
    if (FAILED(hr))
        {
        return LogReturn.Return(__LINE__, false);
        }

    long Length = pSample->GetActualDataLength();
    ASSERT( Length && (0 == Length % f_PackSize) );

    if ( !CompleteBuffer(pBuffer, OutputEntry.Index, Length) )
        {
        return LogReturn.Return(__LINE__, false);
        }

    hr = pSample->SetSyncPoint(
        m_SyncPoint ? TRUE : FALSE
        );
    if (FAILED(hr))
        {
        return LogReturn.Return(__LINE__, false);
        }

    m_SyncPoint = false;

    if ( OutputEntry.Segment.IsComplete() )
        {
        // send NewSegment notification prior to delivering the
        // first sample of the new segment
        hr = m_pOutputPin->DeliverNewSegment(
            OutputEntry.Segment.StartTime,
            OutputEntry.Segment.StopTime,
            OutputEntry.Segment.Rate
            );
        // we're only making our best effort to deliver the new
        // segment notification; don't propogate failure
        ASSERT(SUCCEEDED(hr));

        DEBUGMSG(
            ZONE_INFO_MAX,
            ( L"DvrPsMux::Mux_t::DeliverSample --> NewSegment("
              L"%i)\r\n",
              static_cast<long>(
                OutputEntry.Segment.StartTime/10000
                ) )
            );
        }

    DEBUGMSG(
        ZONE_INFO_MAX,
        ( L"DvrPsMux::Mux_t::DeliverSample --> delivering "
          L"output buffer\r\n" )
        );

    m_OutputBufferQueue.pop_front();

    hr = m_pOutputPin->Deliver(pSample);
    pSample->Release();

    if (FAILED(hr))
        {
        return LogReturn.Return(__LINE__, false);
        }

    return true;
}

bool
Mux_t::
CompleteBuffer(
    BYTE*         pBuffer,
    unsigned long Index,
    long          Length
    )
{
    if (!pBuffer)
        {
        return false;
        }

#ifdef DEBUG
    m_CompletedFirstVideoPack = false;
#endif

    // TODO: if we have more than one packet in a pack, then we should code the packet offset into bytes 2-5
    // TODO: of the pack (0x00000000 otherwise), detect it, then create the PES header in the middle of the pack
    for (long i = 0; i < Length; i += f_PackSize)
        {
        long Offset = i;

        // We've coded some information into the empty space
        // where the headers will go. We need to pull it out now
        // before we overwrite it.
        PackInfo_t* pInfo =
            reinterpret_cast<PackInfo_t*>(pBuffer + Offset);

        bool NeedsSystemHeader =
            (0x01 == pInfo->NeedsSystemHeader);
        bool NeedsPesExtension =
            (0x01 == pInfo->NeedsPstdBufferFields);
        StreamType_e StreamType =
            static_cast<StreamType_e>(pBuffer[Offset + 2]);
        long PackLength =
            (static_cast<long>(pInfo->LengthHi) << 8) |
            pInfo->LengthLo;

        if (PackLength < 0)
            {
            ASSERT(false);
            return false;
            }

        if (StreamType < Mpeg2Video || StreamType > Ac3Audio)
            {
            ASSERT(false);
            return false;
            }

        DEBUGMSG(
            ZONE_INFO_MAX,
            ( L"DvrPsMux::Mux_t::CompleteBuffer --> %s "
              L"pack\r\n",
              Mpeg2Video == StreamType ? L"VIDEO" : L"AUDIO" )
            );

        bool Succeeded = CodePackHeader(pBuffer + Offset);
        if (!Succeeded)
            {
            ASSERT(false);
            return false;
            }

        Offset += f_PackHeaderSize;

        if (NeedsSystemHeader)
            {
            Succeeded =
                CodeSystemHeader(pBuffer + Offset);
            ASSERT(Succeeded);
            Offset += f_SystemHeaderSize;
            }

        Succeeded = CodePesHeader(
            StreamType,
            pBuffer,
            Index,
            Offset,
            NeedsPesExtension
            );
        ASSERT(Succeeded);
        Offset += f_PesHeaderSize;

#ifdef DEBUG
        if (Mpeg2Video == StreamType)
            {
            m_CompletedFirstVideoPack = true;
            }
#endif

        if (PackLength < f_PackSize)
            {
            // Pad partial packs via one of two methods:
            // 1. If seven or fewer bytes are required, use
            //    memmove to right-justify the payload, and add
            //    stuffing bytes to the PES header.
            // 2. If 8 or more bytes are required, add a padding
            //    packet to the end of the pack.
            long PadLength = f_PackSize - PackLength;
            ASSERT(PadLength >= 0);

            if (PadLength > 0 && PadLength < 8)
                {
                DEBUGMSG(
                    ZONE_INFO_MAX,
                    ( L"DvrPsMux::Mux_t::CompleteBuffer --> "
                      L"shifting PES payload to accommodate %i "
                      L"stuffing bytes\r\n",
                      PadLength )
                    );

                // shift the payload right by <PadLength> bytes
                memmove(
                    pBuffer + Offset + PadLength,
                    pBuffer + Offset,
                    PackLength - (Offset - i)
                    );

                memset(pBuffer + Offset, 0xFF, PadLength);

                Succeeded = UpdatePesHeaderDataLength(
                    pBuffer + Offset - f_PesHeaderSize,
                    PadLength
                    );
                ASSERT(Succeeded);
                }
            else if (PadLength >= 8)
                {
                Succeeded = CodePaddingPacket(
                    pBuffer + i + PackLength,
                    PadLength
                    );
                ASSERT(Succeeded);

                Succeeded = UpdatePesPacketLength(
                    pBuffer + Offset - f_PesHeaderSize,
                    -PadLength
                    );
                ASSERT(Succeeded);
                }
            }
         }

    DEBUGMSG(
        ZONE_INFO_MAX,
        ( L"DvrPsMux::Mux_t::CompleteBuffer --> DONE\r\n" )
        );

    return true;
}

bool
Mux_t::
CodePackHeader(
    BYTE* pStart
    )
{
    if (!pStart)
        {
        return false;
        }

    // pack_start_code
    pStart[0] = 0x00;
    pStart[1] = 0x00;
    pStart[2] = 0x01;
    pStart[3] = 0xBA;

    // SCR
    if (m_Scr27Mhz < 0)
        {
        m_Scr27Mhz = 0;
        }
    else
        {
        if (m_ScrAdvance < 0)
            {
            double TicksPerByte =
                27000000 / (m_ProgramMuxRate * 50);
            m_ScrAdvance =
                static_cast<long>(TicksPerByte * f_PackSize);
            }

        if (m_LastVideoDTS > 0)
            {
            // Adjust the SCR so that it doesn't exceed the
            // video DTS. The distance between the SCR and the
            // corresponding DTS also shouldn't exceed 1 second,
            // so we'll pick the half-way value, 500ms.
            __int64 AdjustedScr =
                (((m_LastVideoDTS - (500 * 10000)) * 27) / 10);
            m_Scr27Mhz = max(AdjustedScr, m_Scr27Mhz);
            }
        else
            {
            m_Scr27Mhz += m_ScrAdvance;
            }

        m_LastVideoDTS = -1;
        }

    // code up the SCR base and extension
    __int64 ScrBase = m_Scr27Mhz / 300;
    short ScrExtension = static_cast<short>(m_Scr27Mhz % 300);

    pStart[4] = MakeByte(
        0x44 // '01b' @ b[7..6] | marker @ b2
        | ( (ScrBase & 0x0001C0000000) >> 27 ) // b[5..3]
        | ( (ScrBase & 0x000030000000) >> 28 ) // b[1..0]
        );

    pStart[5] = MakeByte( (ScrBase & 0x0FF00000) >> 20 );

    pStart[6] = MakeByte(
        0x4 // marker @ b2
        | ( (ScrBase & 0x000F8000) >> 12 ) // b[7..3]
        | ( (ScrBase & 0x6000) >> 13 )     // b[1..0]
        );

    pStart[7] = MakeByte( (ScrBase & 0x1FE0) >> 5 );

    pStart[8] = MakeByte(
        0x4 // marker @ b2
        | ( (ScrBase & 0x001F) << 2 )      // b[7..3]
        | ( (ScrExtension & 0x0180) >> 7 ) // b[1..0]
        );

    pStart[9] = MakeByte(
        0x1 // marker @ b0
        | ( (ScrExtension & 0x007F) << 1 ) // b[7..1]
        );

    // program_mux_rate
    long MuxRate = m_ProgramMuxRate;
    pStart[10] = MakeByte( (MuxRate & 0x003FC000) >> 14 );
    pStart[11] = MakeByte( (MuxRate & 0x00003FC0) >> 6 );
    pStart[12] = MakeByte(
        0x3 // markers @ b[1..0]
        | ( (MuxRate & 0x003F) << 2 ) // b[7..2]
        );

    // reserved='1 1111b', pack_stuffing_length=0
    pStart[13] = 0xF8;

    return true;
}

bool
Mux_t::
CodeSystemHeader(
    BYTE* pStart
    )
{
    if (!pStart)
        {
        return false;
        }

    // system_header_start_code
    pStart[0] = 0x00;
    pStart[1] = 0x00;
    pStart[2] = 0x01;
    pStart[3] = 0xBB;

    // header_length
    pStart[4] = MakeByte(
        ( (f_SystemHeaderSize - 6) & 0xFF00 ) >> 8
        );
    pStart[5] = MakeByte(
        (f_SystemHeaderSize - 6) & 0x00FF
        );

    // rate_bound
    long RateBound = m_RateBound;
    pStart[6] = MakeByte(
        0x80 // marker @ b7
        | ( (RateBound & 0x003F8000) >> 15 ) // b[6..0]
        );

    pStart[7] = MakeByte(
        (RateBound & 0x7F80) >> 7            // b[7..0]
        );
    pStart[8] = MakeByte(
        0x1  // marker @ b0
        | ( (RateBound & 0x007F) << 1 )      // b[7..1]
        );

    // audio_bound=1, fixed_flag=0, CSPS_flag=0
    pStart[9] = 4;

    // system_audio_lock_flag=1, system_video_lock_flag=1,
    // video_bound=1
    pStart[10] = 0xE1;

    // packet_rate_restriction_flag=0, reserved='111 1111b'
    pStart[11] = 0x7F;

    // stream_id (video)
    pStart[12] = 0xB9;  // all video streams

    // buffer_bound_scale, buffer_bound_size
    long VideoScale = m_VideoStdBufferBoundScale;
    long VideoSize = m_VideoStdBufferSizeBound;

    pStart[13] = MakeByte(
        0xC0 // '11b' @ b[7..6]
        | ( (VideoScale & 0x1) << 5 )   // b5
        | ( (VideoSize & 0x1F00) >> 8 ) // b[4..0]
        );
    pStart[14] = MakeByte(VideoSize & 0x00FF);

    // stream_id (audio)
    // buffer_bound_scale, buffer_bound_size
    if (Mpeg2Audio == m_AudioType)
        {
        pStart[15] = 0xB8; // all MPEG-2 audio streams
        }
    else if (Ac3Audio == m_AudioType)
        {
        pStart[15] = 0xBD; // private_stream_1
        }
    else
        {
        ASSERT(false);
        return false;
        }

    long AudioScale = m_AudioStdBufferBoundScale;
    long AudioSize = m_AudioStdBufferSizeBound;

    pStart[16] = MakeByte(
        0xC0 // '11b' @ b[7..6]
        | ( (AudioScale & 0x1) << 5 )   // b5
        | ( (AudioSize & 0x1F00) >> 8 ) // b[4..0]
        );
    pStart[17] = MakeByte(AudioSize & 0x00FF);

    return true;
}

bool
Mux_t::
CodePesHeader(
    StreamType_e StreamType,
    BYTE* pBuffer,
    unsigned long BufferIndex,
    long HeaderOffset,
    bool CodeExtension
    )
{
    // TODO: we should code the MPEG_program_end_code before a discontinuity
    if (!pBuffer)
        {
        return false;
        }

    BYTE* pStart = pBuffer + HeaderOffset;
    long PackRelativeOffset = (HeaderOffset % f_PackSize);

    if ( (f_PackSize - PackRelativeOffset) < f_PesHeaderSize )
        {
        return false;
        }

    // packet_start_code_prefix
    pStart[0] = 0x00;
    pStart[1] = 0x00;
    pStart[2] = 0x01;

    // stream_id
    if (Mpeg2Video == StreamType)
        {
        pStart[3] = 0xE0; // MPEG-2 video stream
        }
    else if (Mpeg2Audio == StreamType)
        {
        // TODO: we don't have extension streams!
        pStart[3] = 0xC0; // MPEG-2 main audio stream
        }
    else if (Ac3Audio == StreamType)
        {
        pStart[3] = 0xBD; // private_stream_1 (AC-3)
        }
    else
        {
        ASSERT(false);
        return false;
        }

    // PES_packet_length
    // TODO: this needs to change when we support multiple MPEG-2 audio packets in a single pack
    pStart[4] = MakeByte(
        ( (f_PackSize - PackRelativeOffset - 6) & 0xFF00 ) >> 8
        );
    pStart[5] = MakeByte(
        (f_PackSize - PackRelativeOffset - 6) & 0x00FF
        );

    // '10b', PES_scrambling_control=0, PES_priority=0,
    // data_alignment_indicator=0, copyright=0,
    // original_or_copy=1
    pStart[6] = 0x81;

    // figure out if we need to code PTS/DTS, then code:
    // PTS_DTS_flags, ESCR_flag=0, ES_rate_flag=0,
    // DSM_trick_mode_flag=0, additional_copy_iinfo_flag=0,
    // PES_CRC_flag=0, PES_extension_flag
    long PacketEnd =
        HeaderOffset - PackRelativeOffset + f_PackSize;

    long NextFieldPos = 0;
    long AudioFrameCount = 0;
    long AudioFrameOffset = 0;

    if (Mpeg2Video == StreamType)
        {
        PictureInfoEntry_t* pEntry =
            &m_PictureInfoQueue.front();

        ASSERT( m_PictureInfoQueue.size() == 0 ||
                pEntry->Reserved > BufferIndex ||
                ( pEntry->Reserved == BufferIndex &&
                  pEntry->Offset > HeaderOffset ) );

        while ( m_PictureInfoQueue.size() > 0 &&
                ( pEntry->Reserved < BufferIndex ||
                  ( pEntry->Reserved == BufferIndex &&
                    pEntry->Offset <= HeaderOffset ) ) )
            {
            // last resort: if we failed to pop an entry from
            // the queue at the right time, discard it now so
            // it doesn't plug the queue
            RETAILMSG(
                1,
                ( L"DvrPsMux::Mux_t::CodePesHeader --> "
                  L"discarding stale picture info entry!\r\n" )
                );
            m_PictureInfoQueue.pop_front();
            pEntry = &m_PictureInfoQueue.front();
            }

        if ( m_PictureInfoQueue.size() > 0 &&
             pEntry->Reserved == BufferIndex &&
             pEntry->Offset < PacketEnd )
            {
            ASSERT( pEntry->IsComplete() );

            // all I-frames should start in a new sample
            ASSERT( IntraCoded != pEntry->CodingType ||
                    !m_CompletedFirstVideoPack );

            if (IntraCoded == pEntry->CodingType)
                {
                m_SyncPoint = true;
                }

            // code the PTS if (1) it's an I-frame, or (2)
            // we've exceeded the PTS coding threshold
            if ( IntraCoded == pEntry->CodingType ||
                 CrossedPtsCodingThreshold(pEntry->Pts) )
                {
                // TODO: should I support low_delay sequences in this Mux filter? (see 13818-1, p. 34, 5th para. + NOTE)
                if ( /*( m_IsLowDelay ) ||*/
                     ( BidirectionallyPredictiveCoded ==
                       pEntry->CodingType ) )
                    {
                    pStart[7] = 0x80 | (CodeExtension ? 1 : 0);

                    // PTS, no DTS
                    bool Succeeded = CodeTimestamp(
                        pStart + 9,
                        pEntry->Pts,
                        PTS
                        );
                    ASSERT(Succeeded);

                    NextFieldPos = 14;
                    }
                else
                    {
                    pStart[7] = 0xC0 | (CodeExtension ? 1 : 0);

                    // PTS + DTS
                    bool Succeeded = CodeTimestamp(
                        pStart + 9,
                        pEntry->Pts,
                        PTS
                        );
                    ASSERT(Succeeded);

                    Succeeded = CodeTimestamp(
                        pStart + 14,
                        pEntry->Pts + pEntry->DtsOffset,
                        DTS
                        );
                    ASSERT(Succeeded);

                    m_LastVideoDTS =
                        pEntry->Pts + pEntry->DtsOffset;

                    NextFieldPos = 19;
                    }

                DEBUGMSG(
                    ZONE_INFO_MAX,
                    ( L"DvrPsMux::Mux_t:CodePesHeader --> "
                      L"coded video %s-frame timestamp: %i "
                      L"ms\r\n",
                      IntraCoded == pEntry->CodingType
                        ? L"I"
                        : PredictiveCoded == pEntry->CodingType
                          ? L"P" : L"B",
                      static_cast<long>(pEntry->Pts/10000) )
                    );

                m_LastCodedVideoPts = pEntry->Pts;
                }
            else
                {
                // don't code the PTS/DTS
                pStart[7] = CodeExtension ? 1 : 0;
                NextFieldPos = 9;
                }

            m_PictureInfoQueue.pop_front();

            // If there's more than one picture coded into
            // this pack/packet, then there will be more
            // than one entry in the queue.
            pEntry = &m_PictureInfoQueue.front();

            ASSERT( m_PictureInfoQueue.size() == 0 ||
                    pEntry->Reserved > BufferIndex ||
                    pEntry->Offset > HeaderOffset );

            while ( m_PictureInfoQueue.size() > 0 &&
                    pEntry->Reserved == BufferIndex &&
                    pEntry->Offset < PacketEnd &&
                    pEntry->IsComplete() )
                {
                m_PictureInfoQueue.pop_front();
                pEntry = &m_PictureInfoQueue.front();
                }
            }
        else
            {
            // no picture in this packet; don't code the PTS/DTS
            pStart[7] = CodeExtension ? 1 : 0;
            NextFieldPos = 9;
            }
        }
    else // audio
        {
        // note that since we're coding the PTS on every audio
        // pack, we don't need to worry about crossing the
        // 700ms PTS threshold for audio
        AudioTimestampEntry_t* pEntry =
            &m_AudioPtsQueue.front();

        ASSERT( m_AudioPtsQueue.size() == 0 ||
                pEntry->Reserved > BufferIndex ||
                ( pEntry->Reserved == BufferIndex &&
                  pEntry->Offset > HeaderOffset ) );

        while ( m_AudioPtsQueue.size() > 0 &&
                ( pEntry->Reserved < BufferIndex ||
                  ( pEntry->Reserved == BufferIndex &&
                    pEntry->Offset <= HeaderOffset ) ) )
            {
            // last resort: if we failed to pop an entry from
            // the queue at the right time, discard it now so
            // it doesn't plug the queue
            RETAILMSG(
                1,
                ( L"DvrPsMux::Mux_t::CodePesHeader --> "
                  L"discarding stale audio timestamp "
                  L"entry!\r\n" )
                );
            m_AudioPtsQueue.pop_front();
            pEntry = &m_AudioPtsQueue.front();
            }

        if ( m_AudioPtsQueue.size() > 0 &&
             ( pEntry->Reserved == BufferIndex &&
               pEntry->Offset < PacketEnd ) )
            {
            // code this PTS
            pStart[7] = 0x80 | (CodeExtension ? 1 : 0);

            bool Succeeded = CodeTimestamp(
                pStart + 9,
                pEntry->Timestamp,
                PTS
                );
            ASSERT(Succeeded);

            DEBUGMSG(
                ZONE_INFO_MAX,
                ( L"DvrPsMux::Mux_t:CodePesHeader --> coded "
                  L"audio timestamp: %i ms\r\n",
                  static_cast<long>(pEntry->Timestamp/10000) )
                );

            NextFieldPos = 14;
            AudioFrameCount++;
            AudioFrameOffset = pEntry->Offset;

            m_AudioPtsQueue.pop_front();

            // If there's more than one frame coded into this
            // pack, then there will be more than one timestamp
            // in the queue.  Since the PTS in the packet header
            // is only associated with the first frame in the
            // pack, we can discard the subsequent queue entries
            pEntry = &m_AudioPtsQueue.front();

            ASSERT( m_AudioPtsQueue.size() == 0    ||
                    pEntry->Reserved > BufferIndex ||
                    pEntry->Offset > HeaderOffset );

            while ( m_AudioPtsQueue.size() > 0 &&
                    pEntry->Reserved == BufferIndex &&
                    pEntry->Offset < PacketEnd )
                {
                AudioFrameCount++;
                m_AudioPtsQueue.pop_front();
                pEntry = &m_AudioPtsQueue.front();
                }
            }
        else
            {
            // don't code the PTS
            pStart[7] = CodeExtension ? 1 : 0;
            NextFieldPos = 9;
            }
        }

    // PES_header_data_length
    pStart[8] = f_PesHeaderSize - 9;

    ASSERT(NextFieldPos > 8);

    if (CodeExtension)
        {
        // PES_private_data_flag=0, pack_header_field_flag=0,
        // program_packet_sequence_counter_flag=0,
        // P-STD_buffer_flag=1, reserved=111b,
        // PES_extension_flag_2=0
        pStart[NextFieldPos] = 0x1E;

        // P-STD_buffer_scale, P-STD_buffer_size
        long BufferScale = 0;
        long BufferSize = 0;
        if (Mpeg2Video == StreamType)
            {
            BufferScale = m_VideoStdBufferScale;
            BufferSize = m_VideoStdBufferSize;
            }
        else // audio
            {
            BufferScale = m_AudioStdBufferScale;
            BufferSize = m_AudioStdBufferSize;
            }

        pStart[NextFieldPos+1] = MakeByte(
            0x40 // '01b' @ b[7..6]
            | ( (BufferScale & 1) << 5 )    // b5
            | ( (BufferSize & 0x1F00) >> 8 ) // b[4..0]
            );

        pStart[NextFieldPos+2] = MakeByte(BufferSize & 0xFF);

        NextFieldPos += 3;
        }

    // stuffing_byte(s)
    long StuffingLength = f_PesHeaderSize - NextFieldPos;
    if (StuffingLength > 0)
        {
        memset(pStart + NextFieldPos, 0xFF, StuffingLength);
        }

    // AC-3-specific private data area
    if (Ac3Audio == StreamType)
        {
        NextFieldPos += StuffingLength;

        // sub_stream_id
        pStart[NextFieldPos] = 0x80;

        // number_of_frame_headers
        pStart[NextFieldPos+1] = MakeByte(
            AudioFrameCount & 0xFF
            );

        // first_access_unit_pointer
        if (!AudioFrameOffset)
            {
            pStart[NextFieldPos+2] = 0;
            pStart[NextFieldPos+3] = 0;
            }
        else
            {
            long Rbn = AudioFrameOffset
                        - HeaderOffset
                        - f_PesHeaderSize
                        - 4;
            if (Rbn < 0)
                {
                ASSERT(false);
                Rbn = 0;
                }

            ASSERT( Rbn == (Rbn & 0xFFFF) );
            pStart[NextFieldPos+2] = MakeByte(
                (Rbn & 0xFF00) >> 8
                );
            pStart[NextFieldPos+3] = MakeByte(Rbn & 0x00FF);
            }
        }

    return true;
}

bool
Mux_t::
UpdatePesPacketLength(
    BYTE* pPesHeaderStart,
    long PacketLengthAdjust
    )
{
    if (!pPesHeaderStart)
        {
        return false;
        }

    ASSERT( 0x00 == pPesHeaderStart[0] &&
            0x00 == pPesHeaderStart[1] &&
            0x01 == pPesHeaderStart[2] &&
            0xBC <= pPesHeaderStart[3] );

    long PacketLength =
        static_cast<long>(pPesHeaderStart[4] << 8);
    PacketLength |= pPesHeaderStart[5];

    PacketLength += PacketLengthAdjust;
    ASSERT( PacketLength == (PacketLength & 0xFFFF) );

    pPesHeaderStart[4] = MakeByte(
        ( PacketLength & 0xFF00 ) >> 8
        );
    pPesHeaderStart[5] = MakeByte(PacketLength & 0x00FF);

    return true;
}

bool
Mux_t::
UpdatePesHeaderDataLength(
    BYTE* pPesHeaderStart,
    long DataLengthAdjust
    )
{
    if (!pPesHeaderStart)
        {
        return false;
        }

    ASSERT( 0x00 == pPesHeaderStart[0] &&
            0x00 == pPesHeaderStart[1] &&
            0x01 == pPesHeaderStart[2] &&
            0xBC <= pPesHeaderStart[3] );

    ASSERT(
        static_cast<long>(pPesHeaderStart[8]) + DataLengthAdjust
        < 256
        );

    pPesHeaderStart[8] += MakeByte(DataLengthAdjust);

    return true;
}

bool
Mux_t::
CodePaddingPacket(
    BYTE* pStart,
    long Length
    )
{
    if (!pStart || Length < 8)
        {
        ASSERT(false);
        return false;
        }

    // packet_start_code_prefix
    pStart[0] = 0x00;
    pStart[1] = 0x00;
    pStart[2] = 0x01;

    // stream_id
    pStart[3] = 0xBE; // padding_stream

    // PES_packet_length
    long PacketLength = Length - 6;
    ASSERT( PacketLength == (PacketLength & 0xFFFF) );

    pStart[4] = MakeByte( (PacketLength & 0xFF00) >> 8 );
    pStart[5] = MakeByte(PacketLength & 0xFF);

    memset(pStart + 6, 0xFF, PacketLength);

    return true;
}

bool
Mux_t::
CodeTimestamp(
    BYTE* pTimestampLoc,
    __int64 Timestamp,
    TimestampType_e PtsDts
    )
{
    if (!pTimestampLoc)
        {
        ASSERT(false);
        return false;
        }

    if (PtsDts == PTS)
        {
        pTimestampLoc[0] = 0x31; // '0011b' @ b[7..4], marker @ b0
        }
    else // DTS
        {
        pTimestampLoc[0] = 0x11; // '0001b' ! b[7..4], marker @ b0
        }

    // convert from DShow's 10MHz clock reference to MPEG's
    // 90kHz clock reference and code the 33-bit timestamp
    Timestamp = (Timestamp * 9) / 1000;

    pTimestampLoc[0] |= MakeByte(
        (Timestamp & 0x1C0000000) >> 29      // b[3..1]
        );
    pTimestampLoc[1]  = MakeByte(
        (Timestamp & 0x3FC00000) >> 22       // b[7..0]
        );
    pTimestampLoc[2]  = MakeByte(
        0x1 // marker @ b0
        | ( (Timestamp & 0x003F8000) >> 14 ) // b[7..1]
        );
    pTimestampLoc[3]  = MakeByte(
        (Timestamp & 0x00007F80) >> 7        // b[7..0]
        );
    pTimestampLoc[4]  = MakeByte(
        0x1 // marker @ b0
        | (Timestamp & 0x3F)                 // b[7..1]
        );

    return true;
}

void
Mux_t::
OnDiscontinuity(
    StreamType_e StreamType
    )
{
    // try to deliver samples from the "old" stream
    if (Mpeg2Video == StreamType)
        {
        PictureInfoEntry_t& Tail = m_PictureInfoQueue.back();
        if ( m_PictureInfoQueue.size() > 0 &&
             !Tail.IsComplete() )
            {
            // we have incomplete info about the last picture
            // in the stream--discard
            while (m_OutputBufferQueue.size() > 0)
                {
                OutputBufferEntry_t& Entry =
                    m_OutputBufferQueue.front();

                IMediaSample* pSample = Entry.pSample;
                if (pSample)
                    {
                    pSample->Release();
                    }

                m_OutputBufferQueue.pop_front();
                }
            }

        // if there are valid, complete buffers in the queue,
        // deliver them
        while (m_OutputBufferQueue.size() > 0)
            {
            // DeliverSample can fail, but we're just making a
            // best effort at delivery
            bool Succeeded = DeliverSample();
            ASSERT(Succeeded);
            }

        // flush the picture info queue
        m_PictureInfoQueue.clear();
        }
    else
        {
        // flush the audio timestamp queue
        m_AudioPtsQueue.clear();
        }

    // now reset state vars
    m_Discontinuity = true;

    if (Mpeg2Video == StreamType)
        {
        m_LastCodedVideoPts = 0;
        }
}

HRESULT
Mux_t::
GetBuffer()
{
    IMediaSample* pSample = NULL;
    HRESULT hr = m_pOutputPin->GetDeliveryBuffer(
        &pSample,
        NULL, NULL, 0
        );
    if (FAILED(hr))
        {
        return hr;
        }

    hr = pSample->GetPointer(&m_pNextPack);
    if (FAILED(hr))
        {
        return hr;
        }

    long Size = pSample->GetSize();
    if (0 != Size % f_PackSize)
        {
        ASSERT(false);
        return E_FAIL;
        }

    m_NextPackOffset = 0;

    m_FoundVideo = false;
    m_FoundAudio = false;

    m_VideoWriter.Reset();
    m_AudioWriter.Reset();

    m_OutputEntry.Index++;
    m_OutputEntry.pSample = pSample;
    m_OutputEntry.Segment.Reset();

    return S_OK;
}

HRESULT
Mux_t::
GetWriter(
    IVideoParserWriter** ppWriter
    )
{
    if (!ppWriter)
        {
        ASSERT(false);
        return E_POINTER;
        }

    *ppWriter = NULL;

    PackWriter_t* pWriter = NULL;
    HRESULT hr = GetWriterHelper(Mpeg2Video, &pWriter);
    if (S_OK == hr)
        {
        *ppWriter = dynamic_cast<IVideoParserWriter*>(pWriter);
        ASSERT(*ppWriter);
        }

    return hr;
}

HRESULT
Mux_t::
GetWriter(
    IAudioParserWriter** ppWriter
    )
{
    if (!ppWriter)
        {
        ASSERT(false);
        return E_POINTER;
        }

    *ppWriter = NULL;

    PackWriter_t* pWriter = NULL;
    HRESULT hr = GetWriterHelper(m_AudioType, &pWriter);
    if (S_OK == hr)
        {
        *ppWriter = dynamic_cast<IAudioParserWriter*>(pWriter);
        ASSERT(*ppWriter);
        }

    return hr;
}

HRESULT
Mux_t::
GetWriterHelper(
    StreamType_e StreamType,
    PackWriter_t** ppWriter
    )
{
    if (!ppWriter)
        {
        ASSERT(false);
        return E_POINTER;
        }

    *ppWriter = NULL;

    if ( !m_AllowAudio && (Mpeg2Video != StreamType) )
        {
        return S_FALSE;
        }

    PackWriter_t* pWriter = (Mpeg2Video == StreamType)
        ? &m_VideoWriter
        : &m_AudioWriter;

    if ( pWriter->IsComplete() )
        {
        HRESULT hr = S_OK;

        if (!m_OutputEntry.pSample)
            {
            // first GetWriter call; we need a new output buffer
            hr = GetBuffer();
            if (FAILED(hr))
                {
                return hr;
                }
            }
        else if (!m_pNextPack)
            {
            hr = OnWriterFinished();
            if (FAILED(hr))
                {
                return hr;
                }
            }

        PREFAST_ASSERT(m_OutputEntry.pSample);
        long HeaderLength = f_PackHeaderSize + f_PesHeaderSize;

        if (Ac3Audio == StreamType)
            {
            HeaderLength += 4; // AC-3-specific data area
            }

        BYTE* pBuffer;
        IMediaSample* pSample = m_OutputEntry.pSample;

        hr = pSample->GetPointer(&pBuffer);
        if (FAILED(hr))
            {
            ASSERT(false);
            return hr;
            }

        // We need to code some information into the empty space
        // where the headers will go--we do this so
        // OnWriterFinished() knows how to code the headers for
        // each pack.
        PackInfo_t* pInfo =
            reinterpret_cast<PackInfo_t*>(m_pNextPack);

        if (1 == m_OutputEntry.Index)
            {
            if (pBuffer == m_pNextPack)
                {
                HeaderLength += f_SystemHeaderSize;
                pInfo->NeedsSystemHeader = 0x01;
                }

            if ( !m_FoundVideo && (Mpeg2Video == StreamType) )
                {
                pInfo->NeedsPstdBufferFields = 0x01;
                }
            else if ( !m_FoundAudio && (Mpeg2Video != StreamType) )
                {
                pInfo->NeedsPstdBufferFields = 0x01;
                }
            else
                {
                pInfo->NeedsPstdBufferFields = 0x00;
                }
            }
        else
            {
            pInfo->NeedsSystemHeader = 0x00;
            pInfo->NeedsPstdBufferFields = 0x00;
            }

        pInfo->StreamType = StreamType;
        pInfo->LengthHi = MakeByte((f_PackSize >> 8) & 0xFF);
        pInfo->LengthLo = MakeByte(f_PackSize & 0xFF);

        // For EVERY buffer (not just the first) keep track of
        // whether there is audio/video in the buffer yet, so
        // we know how to handle a discontinuity.
        if ( !m_FoundVideo && (Mpeg2Video == StreamType) )
            {
            m_FoundVideo = true;
            }
        else if ( !m_FoundAudio && (Mpeg2Video != StreamType) )
            {
            m_FoundAudio = true;
            }

        // here's the finale: we assign a writer to this pack
        // so parsers can write data into the buffer
        pWriter->SetPack(
            m_pNextPack,
            m_NextPackOffset,
            HeaderLength
            );

        // now prepare m_pNextPack for the next GetWriter() call
        m_pNextPack += f_PackSize;
        m_NextPackOffset += f_PackSize;

        DEBUGMSG(
            0/*ZONE_INFO_MAX*/,
            ( L"DvrPsMux::Mux_t::GetWriter --> next pack "
              L"(%#x), EOB=%#x\r\n",
              m_pNextPack, pBuffer + pSample->GetSize() )
            );

        if (m_pNextPack >= pBuffer + pSample->GetSize())
            {
            ASSERT(m_pNextPack == pBuffer + pSample->GetSize());
            m_pNextPack = NULL;
            }
        }

    *ppWriter = pWriter;
    return S_OK;
}


HRESULT
Mux_t::
Flush()
{
    ASSERT(!m_Flush);

    if (m_OutputEntry.pSample)
        {
        // if there's something to flush, then flush it
        m_Flush = true;
        m_pNextPack = NULL;
        return OnWriterFinished();
        }

    return S_OK;
}

bool
Mux_t::
IsOutputBufferEmpty()
{
    return (m_NextPackOffset == 0);
}

void
Mux_t::
Notify(
    ParserManagerEvent_e Event
    )
{
    if (FirstVideoSequence == Event)
        {
        m_AllowAudio = true;
        }
}

bool
Mux_t::
Append(
    SampleEntry_t& Entry
    )
{
    bool Succeeded = m_InputQueue.PushBack(Entry);

    if (m_InputQueue.Size() > 100)
    {
        DEBUGMSG(
            ZONE_INFO_MAX && ZONE_REFCOUNT,
            ( L"DvrPsMux::Mux_t::Append %s--> Input Queue size = "
              L"%i\r\n",
              Succeeded ? L"" : L"FAILED ",
              m_InputQueue.Size() )
            );
    }

    return Succeeded;
}

HRESULT
Mux_t::
NewSegment(
    __int64 StartTime,
    __int64 StopTime,
    double  Rate
    )
{
    if ( StartTime < 0 ||
         StopTime <= StartTime ||
         Rate <= 0 )
        {
        ASSERT(false);
        return E_INVALIDARG;
        }

    ASSERT( StopTime == 0x7FFFFFFFFFFFFFFF &&
            Rate == 1.0 );
    ASSERT( !m_NewSegment.IsComplete() );

    m_NewSegment.StartTime = StartTime;
    m_NewSegment.StopTime = 0x7FFFFFFFFFFFFFFF;
    m_NewSegment.Rate = 1.0;

    ASSERT( m_NewSegment.IsComplete() );
    return S_OK;
}

HRESULT
Mux_t::
Discontinuity(
    StreamType_e StreamType
    )
{
    // m_SetDiscontinuity is in effect until the previous sample
    // gets flushed; it means "Set the discontinuity flag on the
    // NEXT sample."
    // m_Discontinuity is set after the previous sample is
    // flushed or if there is no previous sample; it means "Set
    // the discontinuity flag on THIS sample."

    if (Mpeg2Video == StreamType)
        {
        if (m_FoundVideo)
            {
            m_SetDiscontinuity = true;
            m_SetDiscontinuityType = StreamType;
            if ( m_NewSegment.IsComplete() )
                {
                m_SetSegment = m_NewSegment;
                m_NewSegment.Reset();
                }
            return Flush();
            }
        else
            {
            if ( m_NewSegment.IsComplete() )
                {
                m_Segment = m_NewSegment;
                }

            OnDiscontinuity(StreamType);
            }
        }
    else // audio stream type
        {
        if (m_FoundAudio)
            {
            m_SetDiscontinuity = true;
            m_SetDiscontinuityType = StreamType;
            if ( m_NewSegment.IsComplete() )
                {
                m_SetSegment = m_NewSegment;
                m_NewSegment.Reset();
                }
            return Flush();
            }
        else
            {
            if ( m_NewSegment.IsComplete() )
                {
                m_Segment = m_NewSegment;
                }

            OnDiscontinuity(StreamType);
            }
        }

    m_NewSegment.Reset();

    return S_OK;
}

bool
Mux_t::
SetAudioType(
    StreamType_e StreamType
    )
{
    if (StreamType != Ac3Audio && StreamType != Mpeg2Audio)
        {
        return false;
        }

    m_AudioType = StreamType;

    if (Mpeg2Audio == StreamType)
        {
        // The DVD-Video spec defines these values for MPEG-2
        // Audio (see Table 5.2.4-3 in the DVD-Video spec)
        m_AudioStdBufferScale = 0;
        m_AudioStdBufferSize = 32;
        m_AudioStdBufferBoundScale = 0;
        m_AudioStdBufferSizeBound = 32;
        }
    else
        {
        // The DVD-Video spec defines these values for AC-3
        // Audio (see Table 5.2.4-2 in the DVD-Video spec)
        m_AudioStdBufferScale = 1;
        m_AudioStdBufferSize = 58;
        m_AudioStdBufferBoundScale = 1;
        m_AudioStdBufferSizeBound = 58;
        }

    return true;
}

bool
Mux_t::
OnStart(
    const __int64& StartTime
    )
{
    m_Scr27Mhz = -1;
    m_ScrAdvance = -1;

    m_OutputEntry.Index = 0;
    m_OutputEntry.Segment.Reset();
    m_OutputEntry.pSample = NULL;

    m_InputQueue.Open();

    if (!m_MuxThread)
        {
        m_MuxThread = CreateThread(
            NULL, 0,
            MuxThreadProc,
            this,
            0, NULL
            );
        }

    if (!m_MuxThread)
        {
        return false;
        }

    return true;
}

HRESULT
Mux_t::
OnStop()
{
    m_InputQueue.Close();

    // Release all the remaining samples in the output queue.
    while (m_OutputBufferQueue.size() > 0)
        {
        OutputBufferEntry_t& OutputEntry =
            m_OutputBufferQueue.front();

        if (OutputEntry.pSample)
            {
            OutputEntry.pSample->Release();
            }

        m_OutputBufferQueue.pop_front();
        }

    // If we were in the middle of writing to a buffer, release
    // that as well.
    if (m_OutputEntry.pSample)
        {
        m_OutputEntry.pSample->Release();
        m_OutputEntry.pSample = NULL;
        }

    // flush the video/audio info queues
    m_PictureInfoQueue.clear();
    m_AudioPtsQueue.clear();

    // wait for the mux worker thread to shut down so we can
    // clear the thread handle
    //
    // the thread should exit because we closed the queue, but
    // we'll also set the abort event just in case
    BOOL Succeeded = SetEvent(m_ExitWorker);
    ASSERT(Succeeded);

    DWORD Wait = WaitForSingleObject(m_MuxThread, 10000);
    if (WAIT_OBJECT_0 != Wait)
        {
        return E_FAIL;
        }

    CloseHandle(m_MuxThread);
    m_MuxThread = NULL;

    Succeeded = ResetEvent(m_ExitWorker);
    ASSERT(Succeeded);

    return S_OK;
}

HRESULT
Mux_t::
GetProperty(
    LPCOLESTR Name,
    VARIANT* pValue
    )
{
    if (!Name || !pValue)
        {
        ASSERT(false);
        return E_POINTER;
        }

    if ( 0 == wcscmp(Name, RateBound) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_RateBound;
        }
    else if ( 0 == wcscmp(Name, ProgramMuxRate) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_ProgramMuxRate;
        }
    else if ( 0 == wcscmp(Name, VideoStdBufferScale) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_VideoStdBufferScale;
        }
    else if ( 0 == wcscmp(Name, VideoStdBufferSize) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_VideoStdBufferSize;
        }
    else if ( 0 == wcscmp(Name, VideoStdBufferBoundScale) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_VideoStdBufferBoundScale;
        }
    else if ( 0 == wcscmp(Name, VideoStdBufferSizeBound) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_VideoStdBufferSizeBound;
        }
    else if ( 0 == wcscmp(Name, AudioStdBufferScale) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_AudioStdBufferScale;
        }
    else if ( 0 == wcscmp(Name, AudioStdBufferSize) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_AudioStdBufferSize;
        }
    else if ( 0 == wcscmp(Name, AudioStdBufferBoundScale) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_AudioStdBufferBoundScale;
        }
    else if ( 0 == wcscmp(Name, AudioStdBufferSizeBound) )
        {
        VariantInit(pValue);
        pValue->vt = VT_I4;
        pValue->lVal = m_AudioStdBufferSizeBound;
        }
    else
        {
        return E_FAIL;
        }

    return S_OK;
}

HRESULT
Mux_t::
SetProperty(
    LPCOLESTR Name,
    VARIANT* pValue
    )
{
    if (!Name || !pValue)
        {
        ASSERT(false);
        return E_POINTER;
        }

    if ( (0 == wcscmp(Name, RateBound) ) &&
         (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 0x400000)
            {
            return E_INVALIDARG;
            }

        m_RateBound = pValue->lVal;
        }
    else if ( (0 == wcscmp(Name, ProgramMuxRate) ) &&
         (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 0x400000)
            {
            return E_INVALIDARG;
            }

        m_ProgramMuxRate = pValue->lVal;
        }
    else if ( (0 == wcscmp(Name, VideoStdBufferScale) ) &&
              (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 1)
            {
            return E_INVALIDARG;
            }

        m_VideoStdBufferScale = pValue->lVal;
        }
    else if ( (0 == wcscmp(Name, VideoStdBufferSize) ) &&
              (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 0x2000)
            {
            return E_INVALIDARG;
            }

        m_VideoStdBufferSize = pValue->lVal;
        }
    else if ( (0 == wcscmp(Name, VideoStdBufferBoundScale) ) &&
              (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 1)
            {
            return E_INVALIDARG;
            }

        m_VideoStdBufferBoundScale = pValue->lVal;
        }
    else if ( (0 == wcscmp(Name, VideoStdBufferSizeBound) ) &&
              (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 0x2000)
            {
            return E_INVALIDARG;
            }

        m_VideoStdBufferSizeBound = pValue->lVal;
        }
    else if ( (0 == wcscmp(Name, AudioStdBufferScale) ) &&
              (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 1)
            {
            return E_INVALIDARG;
            }

        m_AudioStdBufferScale = pValue->lVal;
        }
    else if ( (0 == wcscmp(Name, AudioStdBufferSize) ) &&
              (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 0x2000)
            {
            return E_INVALIDARG;
            }

        m_AudioStdBufferSize = pValue->lVal;
        }
    else if ( (0 == wcscmp(Name, AudioStdBufferBoundScale) ) &&
              (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 1)
            {
            return E_INVALIDARG;
            }

        m_AudioStdBufferBoundScale = pValue->lVal;
        }
    else if ( (0 == wcscmp(Name, AudioStdBufferSizeBound) ) &&
              (pValue->vt == VT_I4) )
        {
        if (pValue->lVal < 0 || pValue->lVal > 0x2000)
            {
            return E_INVALIDARG;
            }

        m_AudioStdBufferSizeBound = pValue->lVal;
        }
    else
        {
        return E_FAIL;
        }

    return S_OK;
}


//
// ------------- PackWriter_t -------------
//

template<class Entry, class Queue>
HRESULT
PackWriter_t::
WriteHelper(
    BYTE* pSrc,
    long  Offset,
    long  Size,
    Queue* pQueue,
    long* pBytesWritten
    )
{
    if (!pSrc || !pQueue || !pBytesWritten)
        {
        ASSERT(false);
        return E_POINTER;
        }

    if (!Size)
        {
        *pBytesWritten = 0;
        return S_FALSE;
        }

    ASSERT(m_pMux);

    long Length = min( Size, GetRemainingLength() );
    ASSERT(m_Pos + Length <= f_PackSize);
    memcpy(m_pPack + m_Pos, pSrc + Offset, Length);

    while (true)
        {
        if (pQueue->size() == 0)
            {
            // maybe this chunk we're writing doesn't contain
            // the start codes for any access units, thus no
            // timestamps
            break;
            }

        Entry& Ent = pQueue->front();

        ASSERT(Ent.Offset >= Offset);
        if (Ent.Offset < Offset + Length)
            {
            // This entry is part of the block we just copied,
            // so we need to send this to the mux. First convert
            // the byte offset to be relative to the output
            // buffer, then send it to the mux.
            Ent.Offset =
                (m_PackOffset + m_Pos) + (Ent.Offset - Offset);

            if (!m_pMux->OnNewAccessUnit(Ent))
                {
                return E_FAIL;
                }

            pQueue->pop_front();
            }
        else
            {
            RETAILMSG(
                0,
                ( L"DvrPsMux::PackWriter_t::WriterHelper --> "
                  L"head timestamp queue entry offset=%#x, "
                  L"max=%#x\r\n",
                  Ent.Offset,
                  Offset + Length )
                );
            break;
            }
        }

    m_Pos += Length;
    *pBytesWritten = Length;

    if ( IsComplete() )
        {
        // this pack is filled; let's see if the buffer is
        // also filled (OnWriterFinished will return immediately
        // if there are still empty packs in the buffer)
        HRESULT hr = m_pMux->OnWriterFinished();
        if (FAILED(hr))
            {
            return hr;
            }
        }

    return (Length < Size) ? S_FALSE : S_OK;
}

// explicit instantiations
template
HRESULT
PackWriter_t::
WriteHelper<PictureInfoEntry_t, PictureInfoQueue_t>(
    BYTE* pSrc,
    long  Offset,
    long  Size,
    PictureInfoQueue_t* pQueue,
    long* pBytesWritten
    );

template
HRESULT
PackWriter_t::
WriteHelper<AudioTimestampEntry_t, AudioTimestampQueue_t>(
    BYTE* pSrc,
    long  Offset,
    long  Size,
    AudioTimestampQueue_t* pQueue,
    long* pBytesWritten
    );

