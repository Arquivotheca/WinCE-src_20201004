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
#include <audplay.h>

// FileHeader
typedef struct
{
   DWORD   dwRiff;     // Type of file header.
   DWORD   cbSize;     // Size of file header.
   DWORD   dwWave;     // Type of wave.
} RIFF_FILEHEADER, *PRIFF_FILEHEADER;

//  ChunkHeader
typedef struct
{
   DWORD   dwCKID;        // Type Identification for current chunk header.
   DWORD   cbSize;        // Size of current chunk header.
} RIFF_CHUNKHEADER, *PRIFF_CHUNKHEADER;
typedef RIFF_CHUNKHEADER UNALIGNED* PURIFF_CHUNKHEADER;

//  Chunk Types
#define RIFF_FILE       mmioFOURCC('R','I','F','F')
#define RIFF_WAVE       mmioFOURCC('W','A','V','E')
#define RIFF_FORMAT     mmioFOURCC('f','m','t',' ')
#define RIFF_CHANNEL    mmioFOURCC('d','a','t','a')

#ifndef mmioFOURCC
/* This macro is machine byte-sex and word-sex dependent!! */
/* The characters are BYTES, so compatible with ANSI, not at all with UNICODE */
#define mmioFOURCC( ch0, ch1, ch2, ch3 )                                \
                ( (DWORD)(BYTE)(ch0) | ( (DWORD)(BYTE)(ch1) << 8 ) |    \
                ( (DWORD)(BYTE)(ch2) << 16 ) | ( (DWORD)(BYTE)(ch3) << 24 ) )
#endif

// Load wave file, return waveformat header, and position file pointer on first byte of data
static HRESULT ScanForChunk (IStream *pIStream, ULONG dwFourCC, PULONG pcbSize, ULONG & cbCurPos, ULONG cbSourceSize)
{
    HRESULT hr;

    //
    // scan until we find the 'fmt' chunk
    //
    while (cbCurPos < cbSourceSize)
    {
        ULONG cbRead;
        RIFF_CHUNKHEADER Chunk;
        hr = pIStream->Read(&Chunk, sizeof(RIFF_CHUNKHEADER), &cbRead);
        if (FAILED(hr))
        {
            return hr;
        }
        if (cbRead != sizeof(RIFF_CHUNKHEADER))
        {
            return E_FAIL; // requested amount of data was not read, may be EOF
        }

        cbCurPos += sizeof(RIFF_CHUNKHEADER);

        if (Chunk.dwCKID == dwFourCC )
        {
            *pcbSize = Chunk.cbSize;
            return S_OK;
        }

        LARGE_INTEGER FileOffset;
        FileOffset.QuadPart = Chunk.cbSize;
        hr = pIStream->Seek(FileOffset, STREAM_SEEK_CUR, NULL);
        if (FAILED(hr))
        {
            return hr;
        }
        cbCurPos += Chunk.cbSize;
    }

    return E_FAIL; // fell off the end of the source without finding the desired FOURCCC
}

static HRESULT ReadChunk (IStream *pIStream, PVOID * ppvData, ULONG cbSize, ULONG & cbCurPos, ULONG cbSourceSize)
{
    HRESULT hr;

    PVOID pData;

    pData = LocalAlloc(LMEM_FIXED, cbSize);
    if (pData == NULL)
    {
        return E_FAIL;
    }

    // slurp the format structure into place
    ULONG cbRead;
    hr = pIStream->Read(pData, cbSize, &cbRead);
    if (FAILED(hr))
    {
        return hr;
    }

    cbCurPos += cbSize;
    *ppvData = pData;

    return S_OK;
}

// Returns information about the wave file
HRESULT QueryWaveFileInfo(IStream *pIStream,                // Stream to read from
                          WAVEFORMATEX **ppWFX,    // Returned waveformat structure. Remember to call delete on it when done.
                          ULONG *pcbFileSize,               // Returned size of total file
                          ULONG *pcbFileDataOffset,         // Returned offset in file where audio data starts
                          ULONG *pcbFileDataLength          // Returned length of audio data
                          )
{
    HRESULT hr = E_FAIL;

    LPWAVEFORMATEX pWFX = NULL;
    ULONG cbFileSize;
    ULONG cbFileDataOffset;
    ULONG cbFileDataLength;

    LARGE_INTEGER FileOffset;
    FileOffset.QuadPart = 0;
    hr = pIStream->Seek(FileOffset, STREAM_SEEK_SET, NULL);

    // the first few bytes are the file header
    RIFF_FILEHEADER Header;
    ULONG cbRead;
    hr = pIStream->Read(&Header, sizeof(RIFF_FILEHEADER), &cbRead);
    if (FAILED(hr))
    {
        goto Exit;
    }

    // check that it's a valid RIFF file and a valid WAVE form.
    if (Header.dwRiff != RIFF_FILE || Header.dwWave != RIFF_WAVE )
    {
        hr = E_FAIL;
        goto Exit;
    }

    cbFileSize = Header.cbSize;
    ULONG cbCurPos   = sizeof(RIFF_FILEHEADER);

    ULONG cbFmtSize;
    hr = ScanForChunk(pIStream, RIFF_FORMAT, &cbFmtSize, cbCurPos, cbFileSize);
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = ReadChunk(pIStream, (PVOID *) &pWFX, cbFmtSize, cbCurPos, cbFileSize);
    if (FAILED(hr))
    {
        goto Exit;
    }

    // sanity check the format size
    if (cbFmtSize < sizeof(WAVEFORMAT))
    {
        hr = E_FAIL;
        goto Exit;
    }

    // make sure that extended formats have enough data to be processed properly
    // for non-pcm formats, the cbSize field is the number of extra bytes.
    // make sure the the cbSize field is there and that the extra bytes are really there.
    if (    pWFX->wFormatTag != WAVE_FORMAT_PCM
            &&  ( (cbFmtSize < sizeof(WAVEFORMATEX)) || (cbFmtSize < sizeof(WAVEFORMATEX) + pWFX->cbSize) )
       )
    {
        hr = E_FAIL;
        goto Exit;
    }

    // scan until we find the 'data' chunk
    hr = ScanForChunk(pIStream, RIFF_CHANNEL, &cbFileDataLength, cbCurPos, cbFileSize);
    if (FAILED(hr))
    {
        goto Exit;
    }

    cbFileDataOffset = cbCurPos;
    hr = S_OK;

Exit:
    if (FAILED(hr))
    {
        LocalFree(pWFX);
    }
    else
    {
        *ppWFX              = pWFX;
        *pcbFileSize        = cbFileSize;
        *pcbFileDataOffset  = cbFileDataOffset;
        *pcbFileDataLength  = cbFileDataLength;
    }
    return hr;
}

