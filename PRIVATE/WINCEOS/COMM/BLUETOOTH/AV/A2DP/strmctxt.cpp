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

// The following define the maximum attenuations for the SW volume controls in devctxt.cpp
// e.g. 100 => range is from 0dB to -100dB
#ifndef STREAM_ATTEN_MAX
#define STREAM_ATTEN_MAX       100
#endif

#ifndef DEVICE_ATTEN_MAX
#define DEVICE_ATTEN_MAX        35
#endif

#ifndef CLASS_ATTEN_MAX
#define CLASS_ATTEN_MAX         35
#endif

DWORD StreamContext::Open(DeviceContext *pDeviceContext, LPWAVEOPENDESC lpWOD, DWORD dwFlags)
{
    m_RefCount = 1;
    m_pDeviceContext = pDeviceContext;
    m_pfnCallback = (DRVCALLBACK *)lpWOD->dwCallback;
    m_dwInstance  = lpWOD->dwInstance;
    m_hWave       = lpWOD->hWave;
    m_dwFlags     = dwFlags;
    m_bRunning    = FALSE;
    m_bForceSpeaker = FALSE;

    // If it's a PCMWAVEFORMAT struct, it's smaller than a WAVEFORMATEX struct (it doesn't have the cbSize field),
    // so don't copy too much or we risk a fault if the structure is located on the end of a page.
    // All other non-PCM wave formats share the WAVEFORMATEX base structure
    // Note: I don't keep around anything after the cbSize of the WAVEFORMATEX struct so that I don't need to
    // worry about allocating additional space. If we need to keep this info around in the future, we can either
    // allocate it dynamically here, or keep the information in any derived format-specific classes.
    DWORD dwSize;
    WAVEFORMATEX *pwfx = lpWOD->lpFormat;
    if (pwfx->wFormatTag == WAVE_FORMAT_PCM)
    {
        dwSize = sizeof(PCMWAVEFORMAT);
        m_WaveFormat.cbSize = 0;
    }
    else
    {
        dwSize = sizeof(WAVEFORMATEX);
    }

    memcpy(&m_WaveFormat,pwfx,dwSize);

    m_lpWaveHdrHead    = NULL;
    m_lpWaveHdrTail    = NULL;
    m_lpWaveHdrCurrent = NULL;
    m_lpCurrData       = NULL;
    m_lpCurrDataEnd    = NULL;
    m_dwByteCount      = 0;
    m_dwLoopCount = 0;

    m_SecondaryGainClass=0;
    SetGain(pDeviceContext->GetDefaultStreamGain()); // Set gain to default value

    // DEBUGMSG(1, (TEXT("Opening stream 0x%x\r\n"),this));

    // Add stream to list. This will start playback.
    DWORD Res = pDeviceContext->NewStream(this);
    if(MMSYSERR_NOERROR == Res)
    {
    DoCallbackStreamOpened();
    }
    return Res;
}

DWORD StreamContext::Close()
{
    if (StillPlaying())
    {
        return WAVERR_STILLPLAYING;
    }

    // Be sure to turn off speaker if we turned it on.
    ForceSpeaker(FALSE);

    // DEBUGMSG(1, (TEXT("Closing stream 0x%x\r\n"),this));
    DoCallbackStreamClosed();

    return MMSYSERR_NOERROR;
}

// Assumes lock is taken
LONG StreamContext::AddRef()
{
    LONG RefCount = ++m_RefCount;
//    DEBUGMSG(1, (TEXT("AddRef stream 0x%x, RefCount=%d\r\n"),this,RefCount));
    return RefCount;
}

// Assumes lock is taken
LONG StreamContext::Release()
{
    LONG RefCount = --m_RefCount;

//    DEBUGMSG(1, (TEXT("Releasing stream 0x%x, RefCount=%d\r\n"),this,RefCount));
    if (RefCount==0)
    {
        // DEBUGMSG(1, (TEXT("Deleting stream 0x%x\r\n"),this));
        // Only remove stream from list when all refcounts are gone.
        m_pDeviceContext->DeleteStream(this);
        delete this;
    }
    return RefCount;
}

DWORD StreamContext::QueueBuffer(LPWAVEHDR lpWaveHdr)
{
    if (!(lpWaveHdr->dwFlags & WHDR_PREPARED))
    {
        return WAVERR_UNPREPARED;
    }

    lpWaveHdr->dwFlags |= WHDR_INQUEUE;
    lpWaveHdr->dwFlags &= ~WHDR_DONE;
    lpWaveHdr->lpNext=NULL;
    lpWaveHdr->dwBytesRecorded=0;

    if (!m_lpWaveHdrHead)
    {
        m_lpWaveHdrHead = lpWaveHdr;
    }
    else
    {
        m_lpWaveHdrTail->lpNext=lpWaveHdr;
    }

    m_lpWaveHdrTail=lpWaveHdr;

    // Note: Even if head & tail are valid, current may be NULL if we're in the middle of
    // a loop and ran out of data. So, we need to check specifically against current to
    // decide if we need to initialize it.
    if (!m_lpWaveHdrCurrent)
    {
        m_lpWaveHdrCurrent = lpWaveHdr;
        m_lpCurrData    = (PBYTE)lpWaveHdr->lpData;
        m_lpCurrDataEnd = (PBYTE)lpWaveHdr->lpData + lpWaveHdr->dwBufferLength;
        if (lpWaveHdr->dwFlags & WHDR_BEGINLOOP)    // if this is the start of a loop block
        {
            m_dwLoopCount = lpWaveHdr->dwLoops;     // save # of loops
        }
    }

    if (m_bRunning)
    {
        m_pDeviceContext->StreamReadyToRender(this);
    }

    return MMSYSERR_NOERROR;
}


// Note: I've found that when we return used buffers, the wave manager may
// call back into the wave driver in the same thread context to close the stream when
// we return the last buffer.
// If it wasn't the last buffer, the close call will return MMSYSERR_STILLPLAYING.
// However, if it was the last buffer, the close will proceed, and the
// stream may be deleted out from under us. Note that a Lock won't help us here,
// since we're in the same thread which already owns the lock.
// The solution to this is the AddRef/Release use on the stream context, which keeps it
// around if we're acessing it, even if it's closed.

// Assumes lock is taken
PBYTE StreamContext::GetNextBuffer()
{
    LPWAVEHDR lpOldHdr;
    LPWAVEHDR lpNewHdr;
    LPSTR pNewBuf=NULL;

    // Get a pointer to the current buffer which is now done being processed
    lpOldHdr=m_lpWaveHdrCurrent;

    if (!lpOldHdr)
    {
        return NULL;
    }

    // Are we in a loop
    // Note: a loopcount of 1 means we're not really in a loop
    if (m_dwLoopCount>1)
    {
        // We're in a loop!
        if (lpOldHdr->dwFlags & WHDR_ENDLOOP)
        {
           // In loop, last buffer
            // If dwLoopCount was set to INFINITE, loop forever
            // (Note: this is not explicitly in the wave driver API spec)
            if (m_dwLoopCount!=INFINITE)
            {
           m_dwLoopCount--;                    // decrement loop count
            }
           lpNewHdr=m_lpWaveHdrHead;           // go back to start of loop
        }
        else
        {
           // In loop, intermediate buffer
           lpNewHdr=lpOldHdr->lpNext;          // just go to next buffer in loop block
        }

        lpOldHdr=NULL;
    }
    else
    {
        // Not in a loop; return old buffer and get new buffer
        lpNewHdr=lpOldHdr->lpNext;

        m_lpWaveHdrHead = lpNewHdr;           // reset list head
        if (!lpNewHdr)
        {
            m_lpWaveHdrTail=NULL;             // no new buffer, reset tail to NULL
        }
        else if (lpNewHdr->dwFlags & WHDR_BEGINLOOP)    // if new buffer is start of a loop block
        {
            m_dwLoopCount=lpNewHdr->dwLoops;  // save # of loops
        }
    }

    m_lpWaveHdrCurrent=lpNewHdr;              // save current buffer pointer

    if (lpNewHdr)
    {
        m_lpCurrData    = (PBYTE)lpNewHdr->lpData;  // reinitialize data pointer
        m_lpCurrDataEnd = m_lpCurrData + lpNewHdr->dwBufferLength;
    }
    else
    {
        m_lpCurrData  = NULL;
        m_lpCurrDataEnd = NULL;
    }

    // Return the old buffer
    // This may cause the stream to be destroyed, so make sure that any calls to this function
    // are within an AddRef/Release block
    if (lpOldHdr)
    {
        ReturnBuffer(lpOldHdr);
    }

    return m_lpCurrData;
}


DWORD StreamContext::BreakLoop()
{
    AddRef();

    if (m_dwLoopCount>0)
    {
        m_dwLoopCount = 0;

        LPWAVEHDR lpHdr;
        while (m_lpWaveHdrHead!=m_lpWaveHdrCurrent)
        {
            lpHdr = m_lpWaveHdrHead;
            m_lpWaveHdrHead = lpHdr->lpNext;
            if (m_lpWaveHdrHead==NULL)
            {
                m_lpWaveHdrTail=NULL;
            }
            ReturnBuffer(lpHdr);
        }
    }

    Release();

    return MMSYSERR_NOERROR;
}

// GainMap in 16.16 format
// Sample code to generate GainMap using VC++

/*

#include "stdafx.h"
#include "math.h"

const int NumEntries = 200;
const double fdBMax = -0.5;
const double fdBMin = -100.0;

int _tmain(int argc, _TCHAR* argv[])
{
    const double fNumEntries = ((double)(NumEntries-1));
    for (int i=0;i<NumEntries;i++)
    {
        double fVol = fdBMax - ( (fdBMax - fdBMin) * ( ((double)(i)) / fNumEntries) );
        double fMulVal = pow(10.0,fVol/20);
        unsigned long MulVal = (unsigned long)(fMulVal * (double)0x10000);
        printf("0x%04x, // %d: %2.2f dB\n",MulVal,i,fVol);
    }
    return 0;
}

*/


const WORD GainMap[] =
{
    0xf1ad, // 0: -0.50 dB
    0xe429, // 1: -1.00 dB
    0xd765, // 2: -1.50 dB
    0xcb59, // 3: -2.00 dB
    0xbff9, // 4: -2.50 dB
    0xb53b, // 5: -3.00 dB
    0xab18, // 6: -3.50 dB
    0xa186, // 7: -4.00 dB
    0x987d, // 8: -4.50 dB
    0x8ff5, // 9: -5.00 dB
    0x87e8, // 10: -5.50 dB
    0x804d, // 11: -6.00 dB
    0x7920, // 12: -6.50 dB
    0x7259, // 13: -7.00 dB
    0x6bf4, // 14: -7.50 dB
    0x65ea, // 15: -8.00 dB
    0x6036, // 16: -8.50 dB
    0x5ad5, // 17: -9.00 dB
    0x55c0, // 18: -9.50 dB
    0x50f4, // 19: -10.00 dB
    0x4c6d, // 20: -10.50 dB
    0x4826, // 21: -11.00 dB
    0x441d, // 22: -11.50 dB
    0x404d, // 23: -12.00 dB
    0x3cb5, // 24: -12.50 dB
    0x394f, // 25: -13.00 dB
    0x361a, // 26: -13.50 dB
    0x3314, // 27: -14.00 dB
    0x3038, // 28: -14.50 dB
    0x2d86, // 29: -15.00 dB
    0x2afa, // 30: -15.50 dB
    0x2892, // 31: -16.00 dB
    0x264d, // 32: -16.50 dB
    0x2429, // 33: -17.00 dB
    0x2223, // 34: -17.50 dB
    0x203a, // 35: -18.00 dB
    0x1e6c, // 36: -18.50 dB
    0x1cb9, // 37: -19.00 dB
    0x1b1d, // 38: -19.50 dB
    0x1999, // 39: -20.00 dB
    0x182a, // 40: -20.50 dB
    0x16d0, // 41: -21.00 dB
    0x158a, // 42: -21.50 dB
    0x1455, // 43: -22.00 dB
    0x1332, // 44: -22.50 dB
    0x121f, // 45: -23.00 dB
    0x111c, // 46: -23.50 dB
    0x1027, // 47: -24.00 dB
    0x0f3f, // 48: -24.50 dB
    0x0e65, // 49: -25.00 dB
    0x0d97, // 50: -25.50 dB
    0x0cd4, // 51: -26.00 dB
    0x0c1c, // 52: -26.50 dB
    0x0b6f, // 53: -27.00 dB
    0x0acb, // 54: -27.50 dB
    0x0a31, // 55: -28.00 dB
    0x099f, // 56: -28.50 dB
    0x0915, // 57: -29.00 dB
    0x0893, // 58: -29.50 dB
    0x0818, // 59: -30.00 dB
    0x07a4, // 60: -30.50 dB
    0x0737, // 61: -31.00 dB
    0x06cf, // 62: -31.50 dB
    0x066e, // 63: -32.00 dB
    0x0612, // 64: -32.50 dB
    0x05bb, // 65: -33.00 dB
    0x0569, // 66: -33.50 dB
    0x051b, // 67: -34.00 dB
    0x04d2, // 68: -34.50 dB
    0x048d, // 69: -35.00 dB
    0x044c, // 70: -35.50 dB
    0x040e, // 71: -36.00 dB
    0x03d4, // 72: -36.50 dB
    0x039d, // 73: -37.00 dB
    0x0369, // 74: -37.50 dB
    0x0339, // 75: -38.00 dB
    0x030a, // 76: -38.50 dB
    0x02df, // 77: -39.00 dB
    0x02b6, // 78: -39.50 dB
    0x028f, // 79: -40.00 dB
    0x026a, // 80: -40.50 dB
    0x0248, // 81: -41.00 dB
    0x0227, // 82: -41.50 dB
    0x0208, // 83: -42.00 dB
    0x01eb, // 84: -42.50 dB
    0x01cf, // 85: -43.00 dB
    0x01b6, // 86: -43.50 dB
    0x019d, // 87: -44.00 dB
    0x0186, // 88: -44.50 dB
    0x0170, // 89: -45.00 dB
    0x015b, // 90: -45.50 dB
    0x0148, // 91: -46.00 dB
    0x0136, // 92: -46.50 dB
    0x0124, // 93: -47.00 dB
    0x0114, // 94: -47.50 dB
    0x0104, // 95: -48.00 dB
    0x00f6, // 96: -48.50 dB
    0x00e8, // 97: -49.00 dB
    0x00db, // 98: -49.50 dB
    0x00cf, // 99: -50.00 dB
    0x00c3, // 100: -50.50 dB
    0x00b8, // 101: -51.00 dB
    0x00ae, // 102: -51.50 dB
    0x00a4, // 103: -52.00 dB
    0x009b, // 104: -52.50 dB
    0x0092, // 105: -53.00 dB
    0x008a, // 106: -53.50 dB
    0x0082, // 107: -54.00 dB
    0x007b, // 108: -54.50 dB
    0x0074, // 109: -55.00 dB
    0x006e, // 110: -55.50 dB
    0x0067, // 111: -56.00 dB
    0x0062, // 112: -56.50 dB
    0x005c, // 113: -57.00 dB
    0x0057, // 114: -57.50 dB
    0x0052, // 115: -58.00 dB
    0x004d, // 116: -58.50 dB
    0x0049, // 117: -59.00 dB
    0x0045, // 118: -59.50 dB
    0x0041, // 119: -60.00 dB
    0x003d, // 120: -60.50 dB
    0x003a, // 121: -61.00 dB
    0x0037, // 122: -61.50 dB
    0x0034, // 123: -62.00 dB
    0x0031, // 124: -62.50 dB
    0x002e, // 125: -63.00 dB
    0x002b, // 126: -63.50 dB
    0x0029, // 127: -64.00 dB
    0x0027, // 128: -64.50 dB
    0x0024, // 129: -65.00 dB
    0x0022, // 130: -65.50 dB
    0x0020, // 131: -66.00 dB
    0x001f, // 132: -66.50 dB
    0x001d, // 133: -67.00 dB
    0x001b, // 134: -67.50 dB
    0x001a, // 135: -68.00 dB
    0x0018, // 136: -68.50 dB
    0x0017, // 137: -69.00 dB
    0x0015, // 138: -69.50 dB
    0x0014, // 139: -70.00 dB
    0x0013, // 140: -70.50 dB
    0x0012, // 141: -71.00 dB
    0x0011, // 142: -71.50 dB
    0x0010, // 143: -72.00 dB
    0x000f, // 144: -72.50 dB
    0x000e, // 145: -73.00 dB
    0x000d, // 146: -73.50 dB
    0x000d, // 147: -74.00 dB
    0x000c, // 148: -74.50 dB
    0x000b, // 149: -75.00 dB
    0x000b, // 150: -75.50 dB
    0x000a, // 151: -76.00 dB
    0x0009, // 152: -76.50 dB
    0x0009, // 153: -77.00 dB
    0x0008, // 154: -77.50 dB
    0x0008, // 155: -78.00 dB
    0x0007, // 156: -78.50 dB
    0x0007, // 157: -79.00 dB
    0x0006, // 158: -79.50 dB
    0x0006, // 159: -80.00 dB
    0x0006, // 160: -80.50 dB
    0x0005, // 161: -81.00 dB
    0x0005, // 162: -81.50 dB
    0x0005, // 163: -82.00 dB
    0x0004, // 164: -82.50 dB
    0x0004, // 165: -83.00 dB
    0x0004, // 166: -83.50 dB
    0x0004, // 167: -84.00 dB
    0x0003, // 168: -84.50 dB
    0x0003, // 169: -85.00 dB
    0x0003, // 170: -85.50 dB
    0x0003, // 171: -86.00 dB
    0x0003, // 172: -86.50 dB
    0x0002, // 173: -87.00 dB
    0x0002, // 174: -87.50 dB
    0x0002, // 175: -88.00 dB
    0x0002, // 176: -88.50 dB
    0x0002, // 177: -89.00 dB
    0x0002, // 178: -89.50 dB
    0x0002, // 179: -90.00 dB
    0x0001, // 180: -90.50 dB
    0x0001, // 181: -91.00 dB
    0x0001, // 182: -91.50 dB
    0x0001, // 183: -92.00 dB
    0x0001, // 184: -92.50 dB
    0x0001, // 185: -93.00 dB
    0x0001, // 186: -93.50 dB
    0x0001, // 187: -94.00 dB
    0x0001, // 188: -94.50 dB
    0x0001, // 189: -95.00 dB
    0x0001, // 190: -95.50 dB
    0x0001, // 191: -96.00 dB
    0x0000, // 192: -96.50 dB
    0x0000, // 193: -97.00 dB
    0x0000, // 194: -97.50 dB
    0x0000, // 195: -98.00 dB
    0x0000, // 196: -98.50 dB
    0x0000, // 197: -99.00 dB
    0x0000, // 198: -99.50 dB
    0x0000, // 199: -100.00 dB
};

// Channel 0 is the left channel, which is the low 16-bits of volume data
DWORD StreamContext::MapGain(DWORD StreamGain, DWORD Channel)
{
    // Get correct stream gain based on channel
    if (Channel==1)
    {
        StreamGain >>= 16;
    }
    StreamGain &= 0xFFFF;

    // Get Device gain
    DWORD DeviceGain;
    if (m_SecondaryGainClass >= SECONDARYDEVICEGAINCLASSMAX)
    {
        DeviceGain = 0xFFFF;
    }
    else
    {
        // Apply device gain
        DeviceGain = m_pDeviceContext->GetGain();
        if (Channel==1)
        {
            DeviceGain >>= 16;
        }
        DeviceGain &= 0xFFFF;
    }

    // Get Secondary gain
    DWORD SecondaryGain;
    SecondaryGain = m_pDeviceContext->GetSecondaryGainLimit(m_SecondaryGainClass);
    SecondaryGain &= 0xFFFF; // For now, only use lowest 16 bits for both channels

    DWORD fGainMultiplier;

    // Special handling- if any gain is totally 0, mute the output
    if ((StreamGain==0) || (DeviceGain==0) || (SecondaryGain==0))
    {
        fGainMultiplier = 0;
    }
    else
    {
        // Now calculate attenuation of each in dB using appropriate ranges

        // Stream volume is normalized to the range from 0 to -100 dB
        // Device and secondary gain are normalized from 0 to -35 dB
        // These can be modified in hwctxt.h

        DWORD dBAttenStream, dBAttenDevice, dBAttenSecondary, dBAttenTotal;

        dBAttenStream    = ((0xFFFF - StreamGain)    * STREAM_ATTEN_MAX);
        dBAttenDevice    = ((0xFFFF - DeviceGain)    * DEVICE_ATTEN_MAX );
        dBAttenSecondary = ((0xFFFF - SecondaryGain) * CLASS_ATTEN_MAX );

        // Add together
        dBAttenTotal = dBAttenStream + dBAttenDevice + dBAttenSecondary;

        // Multiply result by 2 for .5 dB steps in the table
        dBAttenTotal *= 2;

        // Round up to account for rounding errors in lower 16 bits
        dBAttenTotal += 0x8000;

        // Now shift back to the lowest 16 bits to get an index into the table
        dBAttenTotal >>= 16;

        // dBAttenTotal should range from 0 to something like 340 (if all terms were close to 0)

        // Special case 0 as totally muted. The table starts at -.5dB, rather than 0dB, since
        // 0dB would take more than the 16-bits we allowed per entry.
        if (dBAttenTotal==0)
        {
            fGainMultiplier = 0x10000;
        }
        else if (dBAttenTotal>200)
        {
            fGainMultiplier = 0;
        }
        else
        {
            fGainMultiplier = (DWORD)GainMap[dBAttenTotal-1];
        }
    }

    return fGainMultiplier;
}

DWORD StreamContext::GetPos(PMMTIME pmmt)
{
    switch (pmmt->wType)
    {

    case TIME_SAMPLES:
        pmmt->u.sample = (m_dwByteCount * 8) /
                         (m_WaveFormat.nChannels * m_WaveFormat.wBitsPerSample);
        break;

    case TIME_MS:
        if (m_WaveFormat.nAvgBytesPerSec != 0)
        {
            pmmt->u.ms = (m_dwByteCount * 1000) / m_WaveFormat.nAvgBytesPerSec;
            break;
        }
        // If we don't know avg bytes per sec, fall through to TIME_BYTES

    default:
        // Anything else, return TIME_BYTES instead.
        pmmt->wType = TIME_BYTES;

        // Fall through to TIME_BYTES
    case TIME_BYTES:
        pmmt->u.cb = m_dwByteCount;
    }

    return MMSYSERR_NOERROR;
}

DWORD WaveStreamContext::Open(DeviceContext *pDeviceContext, LPWAVEOPENDESC lpWOD, DWORD dwFlags)
{
    DWORD Result;
    Result = StreamContext::Open(pDeviceContext,lpWOD,dwFlags);
    if (MMSYSERR_NOERROR != Result)
    {
        return Result;
    }

    if (m_WaveFormat.wBitsPerSample == 8)
    {
        if (m_WaveFormat.nChannels == 1)
        {
            m_SampleType = PCM_TYPE_M8;
            m_SampleSize = 1;
        }
        else
        {
            m_SampleType = PCM_TYPE_S8;
            m_SampleSize = 2;
        }
    }
    else
    {
        if (m_WaveFormat.nChannels == 1)
        {
            m_SampleType = PCM_TYPE_M16;
            m_SampleSize = 2;
        }
        else
        {
            m_SampleType = PCM_TYPE_S16;
            m_SampleSize = 4;
        }
    }

    SetRate(0x10000);

    int i;
    for (i=0;i<OUT_CHANNELS;i++)
    {
        m_PrevSamp[i] = 0;
        m_CurrSamp[i] = 0;
    }
    m_CurrT    = DELTA_OVERFLOW;   // Initializing to this ensures we get the 1st sample.

    return MMSYSERR_NOERROR;
}

DWORD WaveStreamContext::GetRate(DWORD *pdwMultiplier)
{
    *pdwMultiplier = m_dwMultiplier;
    return MMSYSERR_NOERROR;
}

DWORD StreamContext::Run()
{
    m_bRunning=TRUE;
    if (m_lpCurrData)
    {
        m_pDeviceContext->StreamReadyToRender(this);
    }

    return MMSYSERR_NOERROR;
}

DWORD StreamContext::Stop()
{
    m_bRunning=FALSE;
    return MMSYSERR_NOERROR;
}

DWORD StreamContext::Reset()
{
    AddRef();

    // Stop stream for now.
    Stop();

    m_lpWaveHdrCurrent  = NULL;
    m_lpCurrData       = NULL;
    m_lpCurrDataEnd    = NULL;
    m_dwByteCount      = 0;
    m_dwLoopCount      = 0;

    LPWAVEHDR lpHdr;
    while (m_lpWaveHdrHead)
    {
        lpHdr = m_lpWaveHdrHead;
        m_lpWaveHdrHead = lpHdr->lpNext;
        if (m_lpWaveHdrHead==NULL)
        {
            m_lpWaveHdrTail=NULL;
        }
        ReturnBuffer(lpHdr);
    }

    Release();

    return MMSYSERR_NOERROR;
}

//------------------------------------------------------------------------------
//
//  Function: ForceSpeaker
//  
//

DWORD 
StreamContext::ForceSpeaker (BOOL bForceSpeaker)
{
    // Normalize to 0 or 1
    bForceSpeaker = (bForceSpeaker!=0);
    if (bForceSpeaker==m_bForceSpeaker)
    {
        return MMSYSERR_NOERROR;
    }
    m_bForceSpeaker = bForceSpeaker;
    return g_pHWContext->ForceSpeaker(bForceSpeaker);
}

