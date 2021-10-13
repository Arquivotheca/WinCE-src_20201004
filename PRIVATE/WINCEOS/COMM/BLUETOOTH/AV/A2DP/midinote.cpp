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

// x is the desired frequency
#define FREQRATIO(x) (double(x)/double(SAMPLE_RATE))

#define PITCH(x)    ( (UINT32 ) ( FREQRATIO(x) * double(1i64 << 32)) )

#define EQUAL       (1.059463094359)
#ifdef EUROPE
    #define  A      (442.0)
#else
    #define  A      (440.0)
#endif
#define ASHARP      (A * EQUAL)
#define B           (ASHARP * EQUAL)
#define C           (B * EQUAL / 2.0)
#define CSHARP      (C * EQUAL)
#define DNOTE       (CSHARP * EQUAL)
#define DSHARP      (DNOTE * EQUAL)
#define E           (DSHARP * EQUAL)
#define F           (E * EQUAL)
#define FSHARP      (F * EQUAL)
#define G           (FSHARP * EQUAL)
#define GSHARP      (G * EQUAL)

// pitch values, from middle c to octave above
const UINT32 CMidiNote::PitchTable[ 12 ] =
{
    PITCH(C),
    PITCH(CSHARP),
    PITCH(DNOTE),
    PITCH(DSHARP),
    PITCH(E),
    PITCH(F),
    PITCH(FSHARP),
    PITCH(G),
    PITCH(GSHARP),
    PITCH(A),
    PITCH(ASHARP),
    PITCH(B)
};

const INT16 CMidiNote::SineTable[0x101] =
{
0,
804,
1607,
2410,
3211,
4011,
4807,
5601,
6392,
7179,
7961,
8739,
9511,
10278,
11038,
11792,
12539,
13278,
14009,
14732,
15446,
16150,
16845,
17530,
18204,
18867,
19519,
20159,
20787,
21402,
22004,
22594,
23169,
23731,
24278,
24811,
25329,
25831,
26318,
26789,
27244,
27683,
28105,
28510,
28897,
29268,
29621,
29955,
30272,
30571,
30851,
31113,
31356,
31580,
31785,
31970,
32137,
32284,
32412,
32520,
32609,
32678,
32727,
32757,
32767,
32757,
32727,
32678,
32609,
32520,
32412,
32284,
32137,
31970,
31785,
31580,
31356,
31113,
30851,
30571,
30272,
29955,
29621,
29268,
28897,
28510,
28105,
27683,
27244,
26789,
26318,
25831,
25329,
24811,
24278,
23731,
23169,
22594,
22004,
21402,
20787,
20159,
19519,
18867,
18204,
17530,
16845,
16150,
15446,
14732,
14009,
13278,
12539,
11792,
11038,
10278,
9511,
8739,
7961,
7179,
6392,
5601,
4807,
4011,
3211,
2410,
1607,
804,
0,
-804,
-1607,
-2410,
-3211,
-4011,
-4807,
-5601,
-6392,
-7179,
-7961,
-8739,
-9511,
-10278,
-11038,
-11792,
-12539,
-13278,
-14009,
-14732,
-15446,
-16150,
-16845,
-17530,
-18204,
-18867,
-19519,
-20159,
-20787,
-21402,
-22004,
-22594,
-23169,
-23731,
-24278,
-24811,
-25329,
-25831,
-26318,
-26789,
-27244,
-27683,
-28105,
-28510,
-28897,
-29268,
-29621,
-29955,
-30272,
-30571,
-30851,
-31113,
-31356,
-31580,
-31785,
-31970,
-32137,
-32284,
-32412,
-32520,
-32609,
-32678,
-32727,
-32757,
-32767,
-32757,
-32727,
-32678,
-32609,
-32520,
-32412,
-32284,
-32137,
-31970,
-31785,
-31580,
-31356,
-31113,
-30851,
-30571,
-30272,
-29955,
-29621,
-29268,
-28897,
-28510,
-28105,
-27683,
-27244,
-26789,
-26318,
-25831,
-25329,
-24811,
-24278,
-23731,
-23169,
-22594,
-22004,
-21402,
-20787,
-20159,
-19519,
-18867,
-18204,
-17530,
-16845,
-16150,
-15446,
-14732,
-14009,
-13278,
-12539,
-11792,
-11038,
-10278,
-9511,
-8739,
-7961,
-7179,
-6392,
-5601,
-4807,
-4011,
-3211,
-2410,
-1607,
-804,
0   // Extra sample here saves 1 instruction in interpolation case
};

void CMidiNote::GainChange()
{
    for (int i=0; i<2; i++)
    {
#if defined(MONO_GAIN)
        m_fxpGain[i] = m_pMidiStream->MapNoteGain(m_dwGain,0);
#else
        m_fxpGain[i] = m_pMidiStream->MapNoteGain(m_dwGain,i);
#endif
    }
}

HRESULT CMidiNote::NoteOn(CMidiStream *pMidiStream, UINT32 Note, UINT32 Velocity, UINT32 Channel)
{
    // Save params
    m_pMidiStream = pMidiStream;
    m_Note     = Note;
    m_Channel  = Channel;
    SetVelocity(Velocity);

    // Init pitch
    m_Index = 0;
    if (Channel==FREQCHANNEL)
    {
        m_IndexDelta = (Note * INVSAMPLERATE);
    }
    else
    {
        m_IndexDelta = PitchTable[ Note % 12 ] ;

        // Adjust octave
        int Octave = ((int)(Note/12)) - 5;
        if (Octave>0)
        {
            m_IndexDelta <<= Octave;
        }
        else if (Octave<0)
        {
            m_IndexDelta >>= -Octave;
        }
    }

    return S_OK;
}

HRESULT CMidiNote::NoteOff(UINT32 Velocity)
{
    // Calculate the number of samples left before we cross a 0 boundary at the middle or end of the table
    DWORD SamplesLeft;
    if (m_IndexDelta)
        {
        SamplesLeft = ( ((0-m_Index)&0x7FFFFFFF) /m_IndexDelta) + 1;
        }
    else
        {
        SamplesLeft=0;
        }
    m_dwBytesLeft = SamplesLeft * sizeof(HWSAMPLE) * OUT_CHANNELS;
    DEBUGMSG(1, (TEXT("CMidiNote::NoteOff, m_Index = 0x%x, m_IndexDelta = 0x%x, m_dwBytesLeft = %d\r\n"),m_Index,m_IndexDelta,m_dwBytesLeft));
    // m_pMidiStream->NoteDone(this);
    return S_OK;
}

PBYTE CMidiNote::Render(PBYTE pBuffer, PBYTE pBufferEnd, PBYTE pBufferLast)
{
    DWORD BytesLeft = m_dwBytesLeft;

    // Handle common case first
    if (BytesLeft==(DWORD)-1)
    {
        // Call real inner loop
        return Render2(pBuffer,pBufferEnd,pBufferLast);
    }

    DEBUGMSG(1, (TEXT("CMidiNote::Render, Note in release, m_dwBytesLeft = %d\r\n"),BytesLeft));

    DWORD BytesThisBuf = (pBufferEnd-pBuffer);
    if (BytesLeft > BytesThisBuf)
    {
        // If we can't end during this buffer, just remember where we were
        BytesLeft-=BytesThisBuf;
    }
    else
    {
        // Ok, we end during this buffer. Update pBufferEnd to force the renderer to stop on a 0 crossing.
        pBufferEnd = pBuffer + BytesLeft;
        BytesLeft=0;
    }

    m_dwBytesLeft = BytesLeft;

    // Call real inner loop
    pBufferLast = Render2(pBuffer,pBufferEnd,pBufferLast);

    if (BytesLeft==0)
    {
        // Time to end the note.
        DEBUGMSG(1, (TEXT("CMidiNote::Render, Last index after note done = 0x%x\r\n"),m_Index));
        m_pMidiStream->NoteDone(this);
    }

    return pBufferLast;
}

// Inner loop of the note renderer.
PBYTE CMidiNote::Render2(PBYTE pBuffer, PBYTE pBufferEnd, PBYTE pBufferLast)
{
    // Cache values so compiler won't worry about aliasing
    UINT32 Index            = m_Index;
    UINT32 IndexDelta       = m_IndexDelta;
    const INT16 * pSineTable = SineTable;
    LONG fxpGain[2];
    fxpGain[0] = m_fxpGain[0];
    fxpGain[1] = m_fxpGain[1];

    while (pBuffer < pBufferEnd)
    {
        // Index is in 8.24 format, where the top 8 bits index into the sine table and
        // the lower 24 bits represent the fraction of where we sit between two adjacent
        // samples, which we can use if we're doing linear interpolation
        // I chose 8.24 format so that wrap around at the top of the table happens
        // automatically without the need to do any ANDing.

        // Get an index into the sine table and look up the sample.
        UINT32 TableIndex = Index>>24;
        INT32 OutSamp0 = pSineTable[TableIndex];

#if MIDI_OPTIMIZE_LINEAR_INTERPOLATE
        // If we're doing linear interpolation, get the next sample also. Note that I don't
        // need to worry about wrap around at the top of the table because the sine table has
        // an extra value tacked onto the end to handle this special case.
        INT32 NextSamp = pSineTable[TableIndex+1];

        // Now do the interpolation, adjusting the index to be in 24.8 format and throwing away
        // the integer part (e.g. interpolate 256 points between samples).
        OutSamp0 += ( (NextSamp - OutSamp0) * ((Index>>16)&0x00FF) ) >> 8;
#endif

        // Increment the index to move to the next sample
        // and keep within the valid range
        Index += IndexDelta;

#if (OUT_CHANNELS==2)
        INT32 OutSamp1;
        OutSamp1=OutSamp0;

        // Volume!
        OutSamp0 = (OutSamp0 * fxpGain[0]) >> VOLSHIFT;
        OutSamp1 = (OutSamp1 * fxpGain[1]) >> VOLSHIFT;

        if (pBuffer < pBufferLast)
        {
            OutSamp0 += ((HWSAMPLE *)pBuffer)[0];
            OutSamp1 += ((HWSAMPLE *)pBuffer)[1];

#if USE_MIX_SATURATE
            // Handle saturation
            if (OutSamp0>AUDIO_SAMPLE_MAX)
            {
                OutSamp0=AUDIO_SAMPLE_MAX;
            }
            else if (OutSamp0<AUDIO_SAMPLE_MIN)
            {
                OutSamp0=AUDIO_SAMPLE_MIN;
            }
            if (OutSamp1>AUDIO_SAMPLE_MAX)
            {
                OutSamp1=AUDIO_SAMPLE_MAX;
            }
            else if (OutSamp1<AUDIO_SAMPLE_MIN)
            {
                OutSamp1=AUDIO_SAMPLE_MIN;
            }
#endif
        }
        ((HWSAMPLE *)pBuffer)[0] = (HWSAMPLE)OutSamp0;
        ((HWSAMPLE *)pBuffer)[1] = (HWSAMPLE)OutSamp1;
        pBuffer += 2*sizeof(HWSAMPLE);
#else
        // Volume!
        OutSamp0 = (OutSamp0 * fxpGain[0]) >> VOLSHIFT;

        if (pBuffer<pBufferLast)
        {
            // Store/sum to the output buffer
            OutSamp0 += *(HWSAMPLE *)pBuffer;

#if USE_MIX_SATURATE
            // Handle saturation
            if (OutSamp0>AUDIO_SAMPLE_MAX)
            {
                OutSamp0=AUDIO_SAMPLE_MAX;
            }
            else if (OutSamp0<AUDIO_SAMPLE_MIN)
            {
                OutSamp0=AUDIO_SAMPLE_MIN;
            }
#endif
        }
        ((HWSAMPLE *)pBuffer)[0] = (HWSAMPLE)OutSamp0;
        pBuffer+=sizeof(HWSAMPLE);
#endif
    }

    // Save cached settings that might have changed in the inner loop
    m_Index = Index;

    return pBuffer;
}


