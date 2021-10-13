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
// Useful bit / byte twiddling stuff

#ifndef __DVD_BITOPS_H__
#define __DVD_BITOPS_H__

inline DWORD ByteSwap(DWORD dwX)
{
    return _lrotl(((dwX & 0xFF00FF00) >> 8) | ((dwX & 0x00FF00FF) << 8), 16);
}

inline DWORD ByteSwap(const BYTE* pData)
{
    DWORD dw = (pData[0] << 24) |
		(pData[1] << 16) |
		(pData[2] << 8) |
		(pData[3]);
    return dw;
}

inline WORD ByteSwapWord(const BYTE* pData)
{
    return static_cast<WORD>((pData[0] << 8) | pData[1]);
}

inline DWORD BCDByte(BYTE bData)
{
    return (((bData & 0xf0) >> 4) * 10) + (bData & 0x0f);
}

inline WORD StartCoord(const BYTE Data[])
{
    WORD w = ByteSwapWord(&Data[0]);
    return static_cast<WORD>((w & 0x3FF0) >> 4);
}

inline WORD EndCoord(const BYTE Data[])
{
    WORD w = ByteSwapWord(&Data[0]);
    return static_cast<WORD>(w & 0x03FF);
}

// true iff w is an odd number
inline BOOL IsOdd(unsigned n)
{
    return ((n & 0x00000001) == 1);
}

// Convert from PTS to RefTime.
inline REFERENCE_TIME Convert90KHz(DWORD pts)
{
    REFERENCE_TIME rt = pts;
    rt = pts * UNITS / 90000;
    return rt;
};

inline BYTE ToBCD(BYTE bData)
{
    ASSERT(bData < 100);
    return (BYTE)((bData / 10) << 4 | (bData % 10));
}

inline long SMPTEToSeconds(DWORD dwSMPTE)
{
    int hours, minutes, seconds;
    hours = BCDByte((BYTE)(dwSMPTE >> 24));
    minutes = BCDByte((BYTE)(dwSMPTE >> 16)) + 60 * hours;
    seconds = BCDByte((BYTE)(dwSMPTE >> 8)) + (60 * minutes);
    return seconds;
}

inline DWORD SMPTEFromSeconds(long seconds)
{
    int hours, minutes;

    minutes = seconds / 60;
    seconds = seconds % 60;
    hours = minutes / 60;
    minutes = minutes % 60;

    DWORD dwSMPTE = ToBCD((BYTE)hours) << 24 | ToBCD((BYTE)minutes) << 16 | ToBCD((BYTE)seconds) << 8;
    return (dwSMPTE);
}

inline REFERENCE_TIME ConvertFromSMPTE(DWORD dwSMPTE)
{
    REFERENCE_TIME rt = SMPTEToSeconds(dwSMPTE) * UNITS;

    // check frame type 25 fps or 30 fps (non-drop)
    int frames = BCDByte((BYTE)(dwSMPTE & 0x3f));
    frames *= UNITS;
    switch (dwSMPTE & 0xc0) {
    case 0x40:
	rt += frames / 25;
	break;
    case 0xc0:
	rt += frames / 30;
	break;

    case 0:
	ASSERT(frames == 0);
	break;

    default:
	rt = -1;
	break;
    }
    return rt;
}

inline DWORD ConvertToSMPTE(REFERENCE_TIME rt, BOOL b25fps)
{
    DWORD dwSMPTE = SMPTEFromSeconds( (int) (rt/UNITS));

    if (b25fps) {
	dwSMPTE |= 0x40 | ((rt%UNITS) / (UNITS/25));
    } else {
	dwSMPTE |= 0xc0 | ((rt%UNITS) / (UNITS/30));
    }
    return dwSMPTE;
}

#endif __DVD_BITOPS_H__
