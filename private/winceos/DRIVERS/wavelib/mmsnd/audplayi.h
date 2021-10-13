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
#pragma once

#include <wavelib.h>
#include <audplay.h>
#include <unknown.h>

#define INVALID_WAVEOUT_DEVICE ((HWAVEOUT)-1)

class CAudPlay : public _simpleunknown<IAudPlay>
{
public:
    CAudPlay(DWORD numBuffers, DWORD msPerBuffer);
    virtual ~CAudPlay();

    HRESULT Open(IStream *pIStream, IAudNotify *pIAudNotify);
    HRESULT Close();

    HRESULT Length(ULONG *pcbLength, ULONG *pcbBlockAlign);
    HRESULT Duration(ULONG *pTimeMs);
    HRESULT Start();
    HRESULT Stop();
    HRESULT Seek(ULONG Pos);
    HRESULT SetVolume(DWORD Volume);
    HRESULT Preroll();
    HRESULT Reset();

    HRESULT FinishClose();

private:

    HRESULT PrepareHeaders();
    HRESULT UnprepareHeaders();
    HRESULT PlaybackDone();
    HRESULT FillAndQueueBuffer(WAVEHDR *pWaveHdr);
    HRESULT BufferDone(WAVEHDR *pWaveHdr);

    BOOL WaveOutInvalid();

    static void s_WaveCallback( HWAVEOUT hwo,
                                UINT uMsg,
                                DWORD dwInstance,
                                DWORD dwParam1,
                                DWORD dwParam2);

    IStream *m_pIStream;        // Stream to read data from
    IAudNotify *m_pIAudNotify;  // Function to notify when done

    ULONG m_cbFileSize;         // Total size of file
    ULONG m_cbDataOffset;       // Offset into file where audio data begins
    ULONG m_cbDataLength;       // Length of file audio data
    ULONG m_cbDataPos;          // Current position

    PWAVEFORMATEX m_pWFX;                  // Wave Format Header from file

    UINT m_cNumBuffers;                    // Number of buffers to allocate
    PBYTE m_pWaveDataBuffer;               // Pointer to buffer from which wave buffers are allocated
    UINT m_msPerBuffer;                    // Size of each audio buffer in ms
    ULONG m_cbBufSize;                     // Size of each audio buffer in bytes
    LPWAVEHDR m_pWaveHdr;                  // Wave headers
    LONG m_BuffersQueued;                  // Number of buffers queued in the wave driver

    HWAVEOUT m_hWave;                      // Handle of open wave device

    BOOL m_bRunning;                       // Status of Stop/Start
    BOOL m_bReset;                         // Resetting the stream, stop feeding buffers down
};


HRESULT QueryWaveFileInfo(IStream *pIStream,                // Stream to read from
                          WAVEFORMATEX **ppWFX,             // Returned waveformat structure. Remember to call delete on it when done.
                          ULONG *pcbFileSize,               // Returned size of total file
                          ULONG *pcbFileDataOffset,         // Returned offset in file where audio data starts
                          ULONG *pcbFileDataLength          // Returned length of audio data
                          );

