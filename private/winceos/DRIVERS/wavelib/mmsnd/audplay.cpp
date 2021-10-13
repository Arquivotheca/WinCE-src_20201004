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
#include "audplayi.h"
#include "filestream.h"
#include "debug.h"

#define DEFAULT_NUM_BUFFERS 4
#define PLAYSOUND_MIN_BUFFERS 2
#define PLAYSOUND_MAX_BUFFERS 1000

#define DEFAULT_MS_PER_BUFFER 20
#define PLAYSOUND_MIN_MS_PER_BUFFER 1
#define PLAYSOUND_MAX_MS_PER_BUFFER 1000

CAudPlay::CAudPlay(DWORD numBuffers, DWORD msPerBuffer) :
    m_pIStream(NULL),
    m_pIAudNotify(NULL),
    m_hWave(INVALID_WAVEOUT_DEVICE),
    m_pWFX(NULL),
    m_BuffersQueued(0),
    m_bRunning(FALSE),
    m_bReset(FALSE),
    m_pWaveDataBuffer(NULL),
    m_cNumBuffers(numBuffers),
    m_msPerBuffer(msPerBuffer),
    m_pWaveHdr(NULL)
{
    // Check for bad or no configuration, use defaults if so
    if(m_cNumBuffers < PLAYSOUND_MIN_BUFFERS || m_cNumBuffers > PLAYSOUND_MAX_BUFFERS)
    {
        m_cNumBuffers = DEFAULT_NUM_BUFFERS;
    }

    if(m_msPerBuffer < PLAYSOUND_MIN_MS_PER_BUFFER || m_msPerBuffer > PLAYSOUND_MAX_MS_PER_BUFFER)
    {
        m_msPerBuffer = DEFAULT_MS_PER_BUFFER;
    }
}

CAudPlay::~CAudPlay()
{
    if (m_hWave!=INVALID_WAVEOUT_DEVICE)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay destructor called but wave device wasn't closed")));
    }
}

// Prepare all the headers for use with waveOutWrite
HRESULT CAudPlay::PrepareHeaders()
{
    HRESULT hr;
    MMRESULT mmRet;

    for (UINT i=0;i<m_cNumBuffers;i++)
    {
        m_pWaveHdr[i].lpData = (LPSTR)&m_pWaveDataBuffer[i*m_cbBufSize];
        m_pWaveHdr[i].dwBufferLength = m_cbBufSize;

        mmRet = waveOutPrepareHeader(m_hWave, m_pWaveHdr+i, sizeof(WAVEHDR));
        if (MMSYSERR_NOERROR != mmRet)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::PrepareHeaders failed!")));
            UnprepareHeaders();
            hr = E_FAIL;
            goto Exit;
        }
    }
    hr = S_OK;

Exit:
    return hr;

}

// Unprepare all the headers when we're ready to close the wave device
HRESULT CAudPlay::UnprepareHeaders()
{
    MMRESULT mmRet;

    for (UINT i=0;i<m_cNumBuffers;i++)
    {
        if (m_pWaveHdr[i].dwFlags & WHDR_PREPARED)
        {
            mmRet = waveOutUnprepareHeader(m_hWave, m_pWaveHdr+i, sizeof(WAVEHDR));
            if (mmRet != MMSYSERR_NOERROR)
            {
                // What to do... just ignore it (the waveapi will clean up anyway once the device is closed
                DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::UnprepareHeaders failed!")));
            }
        }
    }
    memset(m_pWaveHdr, 0, m_cNumBuffers * sizeof(WAVEHDR));


    return S_OK;
}

// FinishClose gets called in response to WOM_CLOSE, which means that the wave API is done sending us callbacks
// This is where we do our final cleanup of objects that were allocate for use with the wave API.
HRESULT CAudPlay::FinishClose()
{
    // Free the waveformat header
    if (m_pWFX)
    {
        LocalFree(m_pWFX);
        m_pWFX=NULL;
    }

    // Free the data buffer
    if (m_pWaveDataBuffer)
    {
        LocalFree(m_pWaveDataBuffer);
        m_pWaveDataBuffer=NULL;
    }

    // Free the waveHdrs
    if (m_pWaveHdr)
    {
        LocalFree(m_pWaveHdr);
        m_pWaveHdr=NULL;    
    }

    // Release our refs on everything
    if (m_pIAudNotify)
    {
        m_pIAudNotify->Release();
        m_pIAudNotify=NULL;
    }

    if (m_pIStream)
    {
        m_pIStream->Release();
        m_pIStream=NULL;
    }

    // Release our ref on ourself
    Release();

    return S_OK;
}

BOOL CAudPlay::WaveOutInvalid()
{
    if (m_hWave == INVALID_WAVEOUT_DEVICE)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("**** CAudPlay wave device not open! ****")));
        return TRUE;
    }
    return FALSE;
}


// Close the wave device and free all the buffers
// The client application must call this API before releasing CAudPlay
HRESULT CAudPlay::Close()
{

    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    HRESULT hr;
    MMRESULT mmRet;

    hr = Reset();
    if (FAILED(hr))
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Close: Reset failed!")));
        // Ignore
    }

    hr = UnprepareHeaders();
    if (FAILED(hr))
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Close: UnprepareHeaders failed!")));
        // Ignore
    }

    HWAVEOUT hWave = m_hWave;
    m_hWave = INVALID_WAVEOUT_DEVICE;

    mmRet = waveOutClose(hWave);
    if (mmRet != MMSYSERR_NOERROR)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Close: waveOutClose failed!")));
        // Ignore
    }

    return S_OK;
}

// Open the wave device and get ready to play audio
HRESULT CAudPlay::Open(IStream *pIStream, IAudNotify *pIAudNotify)
{
    HRESULT hr;

    if (m_hWave != INVALID_WAVEOUT_DEVICE)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: device was already opened!")));
        return E_FAIL;
    }

    if ( (!pIStream) || (!pIAudNotify) )
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: pIStream or pIAudNotify were NULL!")));
        return E_FAIL;
    }

    // Allocate waveHdrs
    m_pWaveHdr = (LPWAVEHDR)LocalAlloc(LMEM_FIXED, sizeof(WAVEHDR) * m_cNumBuffers);
    if(!m_pWaveHdr)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: unable to allocate m_pWaveHdr!")));
        goto Exit;
    }

    // Zero waveHdrs
    memset(m_pWaveHdr, 0, m_cNumBuffers * sizeof(WAVEHDR));

    // AddRef everything... they will be released during the WOM_CLOSE callback
    m_pIStream = pIStream;
    m_pIAudNotify = pIAudNotify;

    m_pIAudNotify->AddRef();
    m_pIStream->AddRef();

    AddRef();

    // Load info about wave file
    m_cbDataPos = 0;
    hr = QueryWaveFileInfo(pIStream,
                           &m_pWFX,
                           &m_cbFileSize,
                           &m_cbDataOffset,
                           &m_cbDataLength
                           );
    if (FAILED(hr))
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: Invalid wave file!")));
        goto Exit;
    }

    if (0==m_pWFX->nAvgBytesPerSec)
    {
        hr = E_FAIL;
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: Invalid wave file nAvgBytesPerSec value of 0!")));
        goto Exit;
    }

    // Allocate buffers
    m_cbBufSize = m_pWFX->nAvgBytesPerSec /(1000 / m_msPerBuffer);

    // pad up to a multiple of nBlockAlign
    ULONG nBlockAlign = m_pWFX->nBlockAlign;
    if (0==nBlockAlign)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: Invalid wave file nBlockAlign value!")));
        hr = E_FAIL;
        goto Exit;
    }

    ULONG cbOver = m_cbBufSize % nBlockAlign;
    if (cbOver)
    {
        m_cbBufSize += (nBlockAlign - cbOver);
    }

    // make sure we don't go too small
    if (m_cbBufSize < nBlockAlign)
    {
        m_cbBufSize = nBlockAlign;
    }

    m_pWaveDataBuffer = (PBYTE)LocalAlloc(LMEM_FIXED, m_cbBufSize * m_cNumBuffers);
    if (!m_pWaveDataBuffer)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: unable to allocate m_pWaveDataBuffer!")));
        hr = E_FAIL;
        goto Exit;
    }

    // Open wave device
    MMRESULT mmRet;
    mmRet = waveOutOpen(&m_hWave, WAVE_MAPPER, m_pWFX, (DWORD)CAudPlay::s_WaveCallback, (DWORD)this, CALLBACK_FUNCTION);
    if (MMSYSERR_NOERROR != mmRet)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: waveOutOpen failed!")));
        // Undo everything we've done up to this point
        hr = E_FAIL;
        goto Exit;
    }

    // After this point we have enough stuff opened that if we fail, we can just call Close

    // Pause wave device (waveOutOpen leaves it in the running state
    mmRet = waveOutPause(m_hWave);
    m_bRunning = FALSE;
    m_bReset = TRUE;    // Reset denotes that we're not sending buffers yet. Preroll will clear it.

    hr = PrepareHeaders();
    if (FAILED(hr))
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: PrepareHeaders failed!")));
        goto Exit;
    }

    hr = Preroll();
    if (FAILED(hr))
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Open: Preroll failed!")));
        goto Exit;
    }

Exit:
    if (FAILED(hr))
    {
        if (m_hWave!=INVALID_WAVEOUT_DEVICE)
        {
            Close();
        }
        else
        {
            FinishClose();
        }
    }

    return hr;
}

// Set the volume of the wave device
HRESULT CAudPlay::SetVolume(DWORD Volume)
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    waveOutSetVolume(m_hWave,Volume);
    return S_OK;
}

// Returns length of data portion of file and required alignment
HRESULT CAudPlay::Length(ULONG *pcbLength, ULONG *pcbBlockAlign)
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    *pcbLength = m_cbDataLength;
    *pcbBlockAlign = m_pWFX->nBlockAlign;
    return S_OK;
}

// Returns the expected duration of playback
HRESULT CAudPlay::Duration(ULONG *pTimeMs)
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

     unsigned __int64 TimeMs64;
     TimeMs64 = m_cbDataLength;
     TimeMs64 *= 1000;
     TimeMs64 /= m_pWFX->nAvgBytesPerSec;
    *pTimeMs = (ULONG)TimeMs64;
    return S_OK;
}

// Seek to a new position in the wave stream.
HRESULT CAudPlay::Seek(ULONG Pos)
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    HRESULT hr;

    // TODO: Ensure seek is aligned to next block boundary
    LARGE_INTEGER FileOffset;
    m_cbDataPos = m_cbDataOffset + Pos;
    FileOffset.QuadPart = m_cbDataPos;
    hr = m_pIStream->Seek(FileOffset, STREAM_SEEK_SET, NULL);

    return hr;
}

// Start (unpause) playback
HRESULT CAudPlay::Start()
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    if (!m_bRunning)
    {
        m_bRunning=TRUE;
        waveOutRestart(m_hWave);
    }
    return S_OK;
}

// Stop (pause) playback
HRESULT CAudPlay::Stop()
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    if (m_bRunning)
    {
        m_bRunning=FALSE;
        waveOutPause(m_hWave);
    }
    return S_OK;
}

// PlaybackDone is called on EOF condition after all buffers have been returned from the wave device.
// Once we hit this, the device is essentially set to the reset condition.
// The notification function can close/release CAudPlay, or it can call Seek/Preroll/Start to rebegin playback.
HRESULT CAudPlay::PlaybackDone()
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    HRESULT hr;

    Stop();
    m_bReset = TRUE;
    hr = m_pIAudNotify->Done();

    return S_OK;
}

// Reset puts the device in a "reset" state where it is stopped and all buffers are returned from the waveapi.
// Once in the reset state, to restart playback the caller must call Preroll and then Start.
HRESULT CAudPlay::Reset()
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    m_bReset = TRUE;
    Stop();

    // Only call waveOutReset if there are still some queued buffers
    // waveOutReset is the only wave API that requires some synchronization with the
    // returned buffer queue, so it helps to avoid some subtle race conditions
    // in the waveOutReset path if we can avoid calling it.
    if (m_BuffersQueued!=0)
    {
        waveOutReset(m_hWave);
        waveOutPause(m_hWave);
    }

    return S_OK;
}

// Preroll queues up all the buffers into the wave driver in preparation to begin playback
HRESULT CAudPlay::Preroll()
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    HRESULT hr = S_OK;

    if (!m_bReset)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("CAudPlay::Preroll: device not in reset state!")));
        return E_FAIL;
    }

    m_bReset = FALSE;
    for (UINT i=0;i<m_cNumBuffers;i++)
    {
        hr = FillAndQueueBuffer(m_pWaveHdr+i);
    }

    return hr;
}

// FillAndQueueBuffer reads as much data from the stream as is available/will fit in the buffer
// Returns:
//  E_FAIL on error
//  S_OK on success
//  S_FALSE if there is no data available
HRESULT CAudPlay::FillAndQueueBuffer(WAVEHDR *pWaveHdr)
{
    if (WaveOutInvalid())
    {
        return E_FAIL;
    }

    // Calculate how much wave data we can safely read from the wave file
    // Note that the wave data typically will not extend to the end of the file,
    // so we can't just rely on the filesize/EOF indicator to read safely.
    HRESULT hr;
    ULONG cbRead = m_cbBufSize;
    ULONG cbFree = m_cbDataLength - m_cbDataPos;
    if (cbRead > cbFree)
    {
        cbRead = cbFree;
    }

    // Read from the file
    hr = m_pIStream->Read(pWaveHdr->lpData, cbRead, &cbRead);
    if (SUCCEEDED(hr))
    {
        if (cbRead==0)
        {
            // No data to be read
            hr = S_FALSE;
        }
        else
        {
            // Remember our position and queue the buffer back to the wave driver
            m_cbDataPos += cbRead;
            pWaveHdr->dwBufferLength = cbRead;
            waveOutWrite(m_hWave,pWaveHdr, sizeof(WAVEHDR));
            InterlockedIncrement(&m_BuffersQueued);
        }
    }

    return hr;
}

// BufferDone is called when the wave driver returns a buffer that is done being played
HRESULT CAudPlay::BufferDone(WAVEHDR *pWaveHdr)
{
    ULONG BuffersQueued = InterlockedDecrement(&m_BuffersQueued);

    // Refill/queue buffers back to the wave driver if we're not in the reset state
    if (!m_bReset)
    {
        HRESULT hr = FillAndQueueBuffer(pWaveHdr);

        // Are we done?
        if ( (hr == S_FALSE) && ( m_BuffersQueued==0 ) )
        {
            PlaybackDone();
        }
    }

    return S_OK;
}

// This is the callback specified in waveOutOpen.
void CAudPlay::s_WaveCallback( HWAVEOUT hwo,
                               UINT uMsg,
                               DWORD dwInstance,
                               DWORD dwParam1,
                               DWORD dwParam2)
{
    CAudPlay *pThis = (CAudPlay *)dwInstance;

    switch (uMsg)
    {
        case WOM_DONE:
            pThis->BufferDone((WAVEHDR *)dwParam1);
            break;

        case WOM_CLOSE:
            pThis->FinishClose();
            break;
    }

    return;
}

// CreateAudPlay returns a pIAudPlay interface pointer.
HRESULT CreateAudPlay(DWORD numBuffers, DWORD msPerBuffer, IAudPlay **ppIAudPlay)
{
    HRESULT hr = S_OK;
    CAudPlay *pAudPlay = NULL;

    if (NULL == ppIAudPlay)
    {
        hr = E_POINTER;
        goto Error;
    }

    *ppIAudPlay = NULL;

    pAudPlay = new CAudPlay(numBuffers, msPerBuffer);
    if (NULL == pAudPlay)
    {
        hr = E_OUTOFMEMORY;
        goto Error;
    }

    *ppIAudPlay = pAudPlay;
    (*ppIAudPlay)->AddRef();

Error:

    if (pAudPlay != NULL)
    {
        pAudPlay->Release();
    }

    return hr;
}
