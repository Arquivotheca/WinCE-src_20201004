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
// -----------------------------------------------------------------------------
//
//      THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//      ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//      THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//      PARTICULAR PURPOSE.
//
// -----------------------------------------------------------------------------
#include "wavemain.h"

BOOL DeviceContext::IsSupportedFormat(LPWAVEFORMATEX lpFormat)
{
    if (lpFormat->wFormatTag != WAVE_FORMAT_PCM)
        return FALSE;

    if (  (lpFormat->nChannels!=1) && (lpFormat->nChannels!=2) )
        return FALSE;

    if (  (lpFormat->wBitsPerSample!=8) && (lpFormat->wBitsPerSample!=16) )
        return FALSE;

    if (lpFormat->nSamplesPerSec < 100 || lpFormat->nSamplesPerSec > 192000)
        return FALSE;

    return TRUE;
}

// We also support MIDI on output
BOOL OutputDeviceContext::IsSupportedFormat(LPWAVEFORMATEX lpFormat)
{
    if (lpFormat->wFormatTag == WAVE_FORMAT_MIDI)
    {
        return TRUE;
    }

    return DeviceContext::IsSupportedFormat(lpFormat);
}

// Assumes lock is taken
DWORD DeviceContext::NewStream(StreamContext *pStreamContext)
{
    DWORD Res = MMSYSERR_NOERROR;
    DWORD Err = ERROR_SUCCESS;

    DEBUGMSG(ZONE_FUNCTION, (L"A2DP: +DeviceContext::NewStream 0x%08X\n", pStreamContext));

    if(g_pHWContext->IsAutoConnectEnabled())
    {
        //TODO: change not to try to open if already opened
        if(IsListEmpty(&m_StreamList) || !g_pHWContext->IsReadyToWrite())
        {

            if(g_pHWContext->IsOpening())//this does nothing since never in opening
                                        //state outside locks
            {
                return MMSYSERR_ERROR;
            }
            g_pHWContext->Opening();
            Err = g_pHWContext->OpenA2dpConnection();
            if(ERROR_SUCCESS == Err)
            {
                Err = g_pHWContext->StartA2dpStream();
                if(ERROR_SUCCESS == Err)
                    {
                    g_pHWContext->Streaming();
                    }
            }

            if(ERROR_SUCCESS != Err)
            {
                g_pHWContext->Closed();
                Res = MMSYSERR_ERROR;
            }        
        }
    }
    else
    {
    // 2 valid possiblities here
    // - the user used message WODM_OPEN_CLOSE_A2DP to open the driver already
    // - the user pushed button in headphones and did a remote open
    
        if(!g_pHWContext->HasA2dpBeenOpened())
        {
            //fail since autoconnect disabled
            Err = ERROR_SERVICE_NOT_ACTIVE;
            Res = MMSYSERR_ERROR;
            //switch out if necessary
            g_pHWContext->MakeNonPreferredDriver();
        }
        else
        {
            Err = g_pHWContext->StartA2dpStream();
            if(ERROR_SUCCESS == Err)
            {
                g_pHWContext->Streaming();
            }
            else
            {
            g_pHWContext->Closed();
            Res = MMSYSERR_ERROR;
            }
            
        } 
    }
    
    if(ERROR_SUCCESS == Err)
    {
        InsertTailList(&m_StreamList,&pStreamContext->m_Link);
    }

    DEBUGMSG(ZONE_FUNCTION, (L"A2DP: -DeviceContext::NewStream 0x%08X\n", pStreamContext));
    
    return Res;
}

void DeviceContext::FlushOutputAndSetSuspend()
{
    SVSUTIL_ASSERT(g_pHWContext->ThisThreadOwnsLock());
    if ( g_pHWContext->IsReadyToWrite() ) {
        if(!StreamsRunning())
        {
            g_pHWContext->SetSuspendRequest();
        }

        // We need to do this on every close since the other stream could
        // currently be opened but not streaming and we need to flush
        // the remaining data of the closing stream.
        g_pHWContext->StartBTTransferThread();
    }
}

// Assumes lock is taken
void DeviceContext::DeleteStream(StreamContext *pStreamContext)
{
    RemoveEntryList(&pStreamContext->m_Link);
    FlushOutputAndSetSuspend();
}

// lock is not taken when calling into the dma functions, need to lock
//only used if things have gone totaly wrong
void DeviceContext::ResetDeviceContext(void)
{
    PLIST_ENTRY pListEntry;
    StreamContext *pStreamContext;
    if(!IsListEmpty(&m_StreamList)){
    for (pListEntry = m_StreamList.Flink;
        pListEntry != &m_StreamList;
        )
        {
        pStreamContext = CONTAINING_RECORD(pListEntry,StreamContext,m_Link);
        pListEntry = pListEntry->Flink;
        if (MMSYSERR_NOERROR == pStreamContext->Reset())
            {
            // After reset we should set this back to running
            pStreamContext->Run();
            }        
        }
    }
}


BOOL DeviceContext::StreamsRunning(void)
{
    PLIST_ENTRY pListEntry;
    StreamContext *pStreamContext;
    if(!IsListEmpty(&m_StreamList)){
    for (pListEntry = m_StreamList.Flink;
        pListEntry != &m_StreamList;
        )
        {
        pStreamContext = CONTAINING_RECORD(pListEntry,StreamContext,m_Link);
        pListEntry = pListEntry->Flink;
        if (pStreamContext->IsRunning())
            {
            return TRUE;
            }
        }
    }
    return FALSE;

}

BOOL DeviceContext::StreamsStillPlaying(void)
{
    PLIST_ENTRY pListEntry;
    StreamContext *pStreamContext;
    if(!IsListEmpty(&m_StreamList)){
    for (pListEntry = m_StreamList.Flink;
        pListEntry != &m_StreamList;
        )
        {
        pStreamContext = CONTAINING_RECORD(pListEntry,StreamContext,m_Link);
        pListEntry = pListEntry->Flink;
        if (pStreamContext->StillPlaying())
            {
            return TRUE;
            }
        }
    }
    return FALSE;

}
// Returns # of samples of output buffer filled
// Assumes that g_pHWContext->Lock already held.
PBYTE DeviceContext::TransferBuffer(PBYTE pBuffer, PBYTE pBufferEnd, DWORD *pNumStreams)
{
    PLIST_ENTRY pListEntry;
    StreamContext *pStreamContext;
    PBYTE pBufferLastThis;
    PBYTE pBufferLast=pBuffer;
    DWORD NumStreams=0;
    BOOL StopRendering = TRUE;

    pListEntry = m_StreamList.Flink;
    while (pListEntry != &m_StreamList)
    {
        // Get a pointer to the stream context
        pStreamContext = CONTAINING_RECORD(pListEntry,StreamContext,m_Link);

        // Note: The stream context may be closed and removed from the list inside
        // of Render, and the context may be freed as soon as we call Release.
        // Therefore we need to grab the next Flink first in case the
        // entry disappears out from under us.
        pListEntry = pListEntry->Flink;

        // Render buffers
        pStreamContext->AddRef();
        pBufferLastThis = pStreamContext->Render(pBuffer, pBufferEnd, pBufferLast);
        pStreamContext->Release();
        if (pBufferLastThis>pBuffer)
        {
            NumStreams++;
        }
        if (pBufferLast < pBufferLastThis)
        {
            pBufferLast = pBufferLastThis;
        }
    }

    if (pNumStreams)
    {
        *pNumStreams=NumStreams;
    }
    return pBufferLast;
}

void DeviceContext::RecalcAllGains()
{
    PLIST_ENTRY pListEntry;
    StreamContext *pStreamContext;

    for (pListEntry = m_StreamList.Flink;
        pListEntry != &m_StreamList;
        pListEntry = pListEntry->Flink)
    {
        pStreamContext = CONTAINING_RECORD(pListEntry,StreamContext,m_Link);
        pStreamContext->GainChange();
    }
    return;
}

void OutputDeviceContext::StreamReadyToRender(StreamContext *pStreamContext)
{
    g_pHWContext->StartRenderingThread();
    return;
}

#ifdef INPUT_ON
void InputDeviceContext::StreamReadyToRender(StreamContext *pStreamContext)
{
    g_pHWContext->StartInputDMA();
    return;
}
#endif
DWORD OutputDeviceContext::GetDevCaps(LPVOID pCaps, DWORD dwSize)
{
    static const WAVEOUTCAPS wc =
    {
        MM_MICROSOFT,
        24,
        0x0001,
        TEXT("Bluetooth Advanced Audio Output"),
        WAVE_FORMAT_1M08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_4M08 |
        WAVE_FORMAT_1S08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_4S08 |
        WAVE_FORMAT_1M16 | WAVE_FORMAT_2M16 | WAVE_FORMAT_4M16 |
        WAVE_FORMAT_1S16 | WAVE_FORMAT_2S16 | WAVE_FORMAT_4S16,
        OUT_CHANNELS,
        0,
        WAVECAPS_VOLUME | WAVECAPS_PLAYBACKRATE
#if !defined(MONO_GAIN)
        | WAVECAPS_LRVOLUME
#endif
    };

    if (dwSize>sizeof(wc))
    {
        dwSize=sizeof(wc);
    }

    memcpy( pCaps, &wc, dwSize);
    return MMSYSERR_NOERROR;
}
#ifdef INPUT_ON

DWORD InputDeviceContext::GetDevCaps(LPVOID pCaps, DWORD dwSize)
{
    static const WAVEINCAPS wc =
    {
        MM_MICROSOFT,
        23,
        0x0001,
        TEXT("Audio Input"),
        WAVE_FORMAT_1M08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_4M08 |
        WAVE_FORMAT_1S08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_4S08 |
        WAVE_FORMAT_1M16 | WAVE_FORMAT_2M16 | WAVE_FORMAT_4M16 |
        WAVE_FORMAT_1S16 | WAVE_FORMAT_2S16 | WAVE_FORMAT_4S16,
        OUT_CHANNELS,
        0
    };

    if (dwSize>sizeof(wc))
    {
        dwSize=sizeof(wc);
    }

    memcpy( pCaps, &wc, dwSize);
    return MMSYSERR_NOERROR;
}
#endif
DWORD OutputDeviceContext::GetExtDevCaps(LPVOID pCaps, DWORD dwSize)
{
    static const WAVEOUTEXTCAPS wec =
    {
        0x0000FFFF,                         // max number of hw-mixed streams
        0x0000FFFF,                         // available HW streams
        SAMPLE_RATE,                        // preferred sample rate for software mixer (0 indicates no preference)
        g_pHWContext->GetAudioDmaPageSize(),// preferred buffer size for software mixer (0 indicates no preference)
        0,                                  // preferred number of buffers for software mixer (0 indicates no preference)
        SAMPLE_RATE,                        // minimum sample rate for a hw-mixed stream
        SAMPLE_RATE                         // maximum sample rate for a hw-mixed stream
    };

    if (dwSize>sizeof(wec))
    {
        dwSize=sizeof(wec);
    }

    memcpy( pCaps, &wec, dwSize);
    return MMSYSERR_NOERROR;
}
#ifdef INPUT_ON

DWORD InputDeviceContext::GetExtDevCaps(LPVOID pCaps, DWORD dwSize)
{
    return MMSYSERR_NOTSUPPORTED;
}



StreamContext *InputDeviceContext::CreateStream(LPWAVEOPENDESC lpWOD)
{
    return new InputStreamContext;
}
#endif
StreamContext *OutputDeviceContext::CreateStream(LPWAVEOPENDESC lpWOD)
{
    LPWAVEFORMATEX lpFormat=lpWOD->lpFormat;
    if (lpWOD->lpFormat->wFormatTag == WAVE_FORMAT_MIDI)
    {
        return new CMidiStream;
    }

    if (lpFormat->nChannels==1)
    {
        if (lpFormat->wBitsPerSample==8)
        {
            return new OutputStreamContextM8;
        }
        else
        {
            return new OutputStreamContextM16;
        }
    }
    else
    {
        if (lpFormat->wBitsPerSample==8)
        {
            return new OutputStreamContextS8;
        }
        else
        {
            return new OutputStreamContextS16;
        }
    }
}

DWORD DeviceContext::OpenStream(LPWAVEOPENDESC lpWOD, DWORD dwFlags, StreamContext **ppStreamContext)
{
    DWORD Result;
    StreamContext *pStreamContext;

    if (lpWOD->lpFormat==NULL)
    {
        return WAVERR_BADFORMAT;
    }

    if (!IsSupportedFormat(lpWOD->lpFormat))
    {
        return WAVERR_BADFORMAT;
    }

    // Query format support only - don't actually open device?
    if (dwFlags & WAVE_FORMAT_QUERY)
    {
        return MMSYSERR_NOERROR;
    }

    if(!g_pHWContext->HasBeenInitialized())
        {
        SetLastError(ERROR_NOT_READY);
        return MMSYSERR_NOTENABLED;
        }

        
    pStreamContext = CreateStream(lpWOD);
    if (!pStreamContext)
    {
        return MMSYSERR_NOMEM;
    }

    Result = pStreamContext->Open(this,lpWOD,dwFlags);
    if (MMSYSERR_ERROR == Result)
    {
        delete pStreamContext;
        return Result;
    }

    *ppStreamContext=pStreamContext;
    return MMSYSERR_NOERROR;
}

