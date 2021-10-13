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
// -----------------------------------------------------------------------------
//
//      THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//      ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//      THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//      PARTICULAR PURPOSE.
//
// -----------------------------------------------------------------------------

#include "wavemain.h"

void CMidiStream::GainChange()
{
    PLIST_ENTRY pListEntry;
    CMidiNote *pCNote;
    pListEntry = m_NoteList.Flink;
    while (pListEntry != &m_NoteList)
    {
        // Get a pointer to the stream context
        pCNote = CONTAINING_RECORD(pListEntry,CMidiNote,m_Link);
        pCNote->GainChange();
        pListEntry = pListEntry->Flink;
    }
}

DWORD CMidiStream::MapNoteGain(DWORD NoteGain, DWORD Channel)
{
    DWORD TotalGain = NoteGain & 0xFFFF;
    DWORD StreamGain = m_dwGain;
    if (Channel==1)
    {
        StreamGain >>= 16;
    }
    StreamGain &= 0xFFFF;

    TotalGain *= StreamGain; // Calc. aggregate gain
    TotalGain += 0xFFFF;   // Force to round up
    TotalGain >>= 16;

    if (Channel==1)
    {
        TotalGain <<= 16;
    }

    return MapGain(TotalGain,Channel);
}

DWORD CMidiStream::Open(DeviceContext *pDeviceContext, LPWAVEOPENDESC lpWOD, DWORD dwFlags)
{
    HRESULT Result;
    LPWAVEFORMAT_MIDI pwfxmidi = (LPWAVEFORMAT_MIDI) lpWOD->lpFormat;

    if (pwfxmidi->wfx.cbSize!=WAVEFORMAT_MIDI_EXTRASIZE)
    {
        return E_FAIL;
    }

    m_USecPerQuarterNote  = pwfxmidi->USecPerQuarterNote;
    m_TicksPerQuarterNote = pwfxmidi->TicksPerQuarterNote;

    UpdateTempo();

    m_DeltaSampleCount=0;

    // Add all notes to free list
    InitializeListHead(&m_NoteList);
    InitializeListHead(&m_FreeList);
    for (int i=0;i<NUMNOTES;i++)
    {
        InsertTailList(&m_FreeList,&m_MidiNote[i].m_Link);
    }

    Result = StreamContext::Open(pDeviceContext, lpWOD, dwFlags);

    if (Result==MMSYSERR_NOERROR)
    {
        // Note: Output streams should be initialized in the run state.
        Run();
    }

    return Result;
}

DWORD CMidiStream::Reset()
{
    DWORD dwResult = StreamContext::Reset();
    if (dwResult==MMSYSERR_NOERROR)
    {
        AllNotesOff(0);

        // Note: Output streams should be reset to the run state.
        Run();
    }
    return dwResult;
}

DWORD CMidiStream::Close()
{
    DWORD dwResult = StreamContext::Close();
    if (dwResult==MMSYSERR_NOERROR)
    {
        AllNotesOff(0);
    }
    return dwResult;
}

HRESULT CMidiStream::UpdateTempo()
{
    if (m_USecPerQuarterNote==0)
    {
        m_USecPerQuarterNote = 500000; // If not specified, assume 500000usec = 1/2 sec per quarter note
    }

    if (m_TicksPerQuarterNote==0)
    {
        m_TicksPerQuarterNote = 96;      // If not specified, assume 96 ticks/quarter note
    }

    UINT64 Num = SAMPLE_RATE;
    Num *= m_USecPerQuarterNote;
    UINT64 Den = 1000000;
    Den *= m_TicksPerQuarterNote;
    UINT64 SamplesPerTick = Num/Den;
    m_SamplesPerTick = (UINT32)SamplesPerTick;
    return S_OK;
}

// Return the delta # of samples until the next midi event
// or 0 if no midi events are left in the queue
UINT32 CMidiStream::ProcessMidiStream()
{
    WAVEFORMAT_MIDI_MESSAGE *pMsg;
    WAVEFORMAT_MIDI_MESSAGE *pMsgEnd;
    UINT32 ThisMidiEventDelta;

    // Process all midi messages up to and including the current sample
    pMsg    = (WAVEFORMAT_MIDI_MESSAGE *)m_lpCurrData;
    pMsgEnd = (WAVEFORMAT_MIDI_MESSAGE *)m_lpCurrDataEnd;

    for (;;)
    {
        if (pMsg>=pMsgEnd)
        {
            pMsg = (WAVEFORMAT_MIDI_MESSAGE *)GetNextBuffer();
            if (!pMsg)
            {
                // DEBUGMSG(1, (TEXT("CMidiStream::ProcessMidiStream no more events\r\n")));
                return 0;
            }
            pMsgEnd = (WAVEFORMAT_MIDI_MESSAGE *)m_lpCurrDataEnd;
        }

        _try
        {
            ThisMidiEventDelta = DeltaTicksToSamples(pMsg->DeltaTicks);
            if (ThisMidiEventDelta > m_DeltaSampleCount)
            {
                m_lpCurrData = (PBYTE)pMsg;
                INT32 Delta = ThisMidiEventDelta-m_DeltaSampleCount;
                // DEBUGMSG(1, (TEXT("CMidiStream::ProcessMidiStream next event @delta %d\r\n"),Delta));
                return Delta;
            }

            // DEBUGMSG(1, (TEXT("CMidiStream::ProcessMidiStream sending midi message 0x%x\r\n"),pMsg->MidiMsg));
            InternalMidiMessage(pMsg->MidiMsg);
            m_DeltaSampleCount=0;
            pMsg++;
        }
        _except (EXCEPTION_EXECUTE_HANDLER)
        {
            RETAILMSG(1, (TEXT("EXCEPTION IN IST for midi stream 0x%x, buffer 0x%x!!!!\r\n"), this, m_lpCurrData));
            pMsg = pMsgEnd; // Pretend we finished reading the application buffer
        }
    }
}

PBYTE CMidiStream::Render(PBYTE pBuffer, PBYTE pBufferEnd, PBYTE pBufferLast)
{

    // DEBUGMSG(1, (TEXT("Entering CMidiStream::Render, pBuffer=0x%x, current delta = %d\r\n"), pBuffer, m_DeltaSampleCount));

    // If we're not running, or we don't have any buffers queued and the note list is empty,
    // just return
    if ( (!m_bRunning) || (!StillPlaying() && IsListEmpty(&m_NoteList)) )
    {
        // DEBUGMSG(1, (TEXT("CMidiStream::Render nothing to do\r\n")));
        return pBuffer;
    }

    while (pBuffer<pBufferEnd)
    {
        // Process pending midi messages and get relative sample # of next midi event
        UINT32 NextMidiEvent;
        NextMidiEvent = ProcessMidiStream();

        PBYTE pBufferEndEvent;  // Where to stop on this pass

        // If NextMidiEvent returns 0, it means there are no more midi messages left in the queue.
        if (NextMidiEvent==0)
        {
            // Just process the rest of this buffer
            pBufferEndEvent=pBufferEnd;
        }
        // NextMidiEvent is non-zero, and represents the delta sample value of the next midi event
        else
        {
            // Convert to be a pointer in this buffer
            pBufferEndEvent = pBuffer + (NextMidiEvent * (sizeof(HWSAMPLE) * OUT_CHANNELS));

            // If the next event occurs after this buffer, just finish processing this buffer
            if (pBufferEndEvent>pBufferEnd)
            {
                pBufferEndEvent=pBufferEnd;
            }
        }

        // Update the delta for the samples we're about to process
        m_DeltaSampleCount += ((pBufferEndEvent-pBuffer)/(sizeof(HWSAMPLE) * OUT_CHANNELS));

        // Process existing notes
        PLIST_ENTRY pListEntry;
        pListEntry = m_NoteList.Flink;
        while (pListEntry != &m_NoteList)
        {
            CMidiNote *pCNote;

            // Get a pointer to the stream context
            pCNote = CONTAINING_RECORD(pListEntry,CMidiNote,m_Link);

            // Get next list entry, since Render may cause note to go away
            pListEntry = pListEntry->Flink;

            PBYTE pBufferLastThis;
            pBufferLastThis = pCNote->Render(pBuffer, pBufferEndEvent, pBufferLast);
            if (pBufferLast < pBufferLastThis)
            {
                pBufferLast = pBufferLastThis;
            }
        }

        pBuffer = pBufferEndEvent;
    }

    // We need to make sure we clear any unwritten section of the buffer to make sure the DMA controller doesn't stop
    StreamContext::ClearBuffer(pBufferLast,pBufferEnd);
    pBufferLast=pBufferEnd;

    // DEBUGMSG(1, (TEXT("CMidiStream::Render returning, pBufferLast=0x%x, pBufferEnd=0x%x\r\n"),pBufferLast,pBufferEnd));
    return pBufferLast;
}

DWORD CMidiStream::MidiMessage(UINT32 dwMessage)
{
    HRESULT Result;

    Result = InternalMidiMessage(dwMessage);

    // If we're running, and the notelist has notes to render, make sure DMA is enabled
    if ( (m_bRunning) && (m_NoteList.Flink != &m_NoteList) )
    {
        m_pDeviceContext->StreamReadyToRender(this);
    }

    return (Result==S_OK) ? MMSYSERR_NOERROR : MMSYSERR_ERROR;
}

// Assumes lock is taken, and we're already positioned at the correct point in the stream
HRESULT CMidiStream::InternalMidiMessage(UINT32 dwData)
{
    UINT32 OpCode = dwData & 0xF0000000;
    switch (OpCode)
    {
    case 0:
        return MidiData(dwData);
    case MIDI_MESSAGE_UPDATETEMPO:
        m_USecPerQuarterNote  = (dwData & 0xFFFFFF);
        return UpdateTempo();
    case MIDI_MESSAGE_FREQGENON:
    case MIDI_MESSAGE_FREQGENOFF:
        {
            UINT32 dwNote = ((dwData) & 0xffff);
            UINT32 dwVelocity = ((dwData >> 16) & 0x7f) ;
            if ((OpCode==MIDI_MESSAGE_FREQGENON)  && (dwVelocity>0))
            {
                return NoteOn(dwNote, dwVelocity, FREQCHANNEL);
            }
            else
            {
                return NoteOff(dwNote, dwVelocity, FREQCHANNEL);
            }
        }
    }
    return E_NOTIMPL;
}

HRESULT CMidiStream::MidiData(UINT32 dwData)
{
    HRESULT Result=E_NOTIMPL;
    UINT32 dwChannel;
    UINT32 dwNote;
    UINT32 dwVelocity;

    if (dwData & 0x80)
    {
        m_RunningStatus = dwData&0xFF;      // status byte...
    }
    else
    {
        dwData = (dwData<<8) | m_RunningStatus;
    }

    dwChannel  = (dwData & 0x0f) ;
    dwNote     = ((dwData >> 8) & 0x7f) ;
    dwVelocity = ((dwData >> 16) & 0x7f) ;

    switch (dwData & 0xf0)
    {
    case 0x90:  // Note on
        if (dwVelocity!=0)
        {
            Result = NoteOn(dwNote, dwVelocity, dwChannel);
            break;
        }
        // If dwVelocity is 0, this is really a note off message, so fall through

    case 0x80:  // Note off
        Result = NoteOff(dwNote, dwVelocity, dwChannel);
        break;

    case 0xB0:  // Control change
        {
            switch (dwNote)
            {
            case 123:   // turns all notes off
                {
                    Result = AllNotesOff(0);
                    break;
                }
            }
            break;
        }
    }

    return Result;
}

CMidiNote *CMidiStream::FindNote(UINT32 dwNote, UINT32 dwChannel)
{
    PLIST_ENTRY pListEntry;
    CMidiNote *pCNote;
    pListEntry = m_NoteList.Flink;
    while (pListEntry != &m_NoteList)
    {
        // Get a pointer to the stream context
        pCNote = CONTAINING_RECORD(pListEntry,CMidiNote,m_Link);

        if (pCNote->NoteVal()==dwNote && pCNote->NoteChannel()==dwChannel)
        {
            return pCNote;
        }

        pListEntry = pListEntry->Flink;
    }
    return NULL;
}

// Assumes lock is taken, and we're already positioned at the correct point in the stream
HRESULT CMidiStream::NoteOn(UINT32 dwNote, UINT32 dwVelocity, UINT32 dwChannel)
{
    CMidiNote *pCNote=NULL;

    PLIST_ENTRY pListEntry;

    // First try to find the same note already being played
    pCNote = FindNote(dwNote, dwChannel);
    if (pCNote)
    {
        // If so, just set its velocity to the new velocity
        // This allows us to change volume while a note is being
        // played without any chance of glitching
        pCNote->SetVelocity(dwVelocity);
    }
    else
    {
        // Try to allocate a note from the free list
        pListEntry = m_FreeList.Flink;
        if (pListEntry != &m_FreeList)
        {
            pCNote = CONTAINING_RECORD(pListEntry,CMidiNote,m_Link);

            // If we got a note from the free list, do an AddRef on this stream context
            AddRef();
        }
        else
        {
            // Note: if we every support multiple instruments, here we should try to steal the oldest
            // note with the same channel before just trying to steal the oldest note.

            // Steal the oldest note (which is the note at the head of the note list)
            // Note: This should _never_ fail, since there must be a note on one of the lists!
            pListEntry = m_NoteList.Flink;
            pCNote = CONTAINING_RECORD(pListEntry,CMidiNote,m_Link);
        }

        pCNote->NoteOn(this,dwNote,dwVelocity,dwChannel);
    }

    // Move the note from whichever list it was on to the note list at the end.
    // This ensures that if we reused an existing note, its age gets reset.
    NoteMoveToNoteList(pCNote);

    return S_OK;
}

// Assumes lock is taken, and we're already positioned at the correct point in the stream
HRESULT CMidiStream::NoteOff(UINT32 dwNote, UINT32 dwVelocity, UINT32 dwChannel)
{
    CMidiNote *pCNote = FindNote(dwNote, dwChannel);
    if (pCNote)
    {
        pCNote->NoteOff(dwVelocity);
    }

    return S_OK;
}

HRESULT CMidiStream::AllNotesOff(UINT32 dwVelocity)
{
    PLIST_ENTRY pListEntry;
    CMidiNote *pCNote;
    pListEntry = m_NoteList.Flink;
    while (pListEntry != &m_NoteList)
    {
        // Get the note
        pCNote = CONTAINING_RECORD(pListEntry,CMidiNote,m_Link);

        // Get the next link, since NoteOff may remove the note from the queue depeding on the implementation
        pListEntry = pListEntry->Flink;

        // Turn the note off
        pCNote->NoteOff(dwVelocity);
    }
    return S_OK;
}

