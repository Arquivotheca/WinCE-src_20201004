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
////////////////////////////////////////////////////////////////////////////////
//
//  Frame number recognizer
//
//
////////////////////////////////////////////////////////////////////////////////

#ifndef FRAME_RECOGNIZER_H
#define FRAME_RECOGNIZER_H

#include <windows.h>
#include <dshow.h>

enum Instrumentation {
	NDIGITS = 6,
	NBITMAPS = 4
};


class VideoFrameRecognizer
{
public:
	VideoFrameRecognizer();
	~VideoFrameRecognizer();
	HRESULT Init(AM_MEDIA_TYPE* pMediaType);
	void Reset();
	HRESULT Recognize(IMediaSample* pSample, DWORD* pFrameNumber);

private:
	AM_MEDIA_TYPE m_mt;
	GUID m_BitmapSubType;
	HBITMAP m_hBitmap[NBITMAPS];
	BITMAPINFO m_SrcBmi[NBITMAPS];
	BITMAPINFO* m_pConvBmi[NBITMAPS];
	BYTE* m_pSrcBits[NBITMAPS];
	BYTE* m_pConvBits[NBITMAPS];
	int m_SrcY;
};


class VideoFrameInstrumenter
{
public:
	VideoFrameInstrumenter();
	~VideoFrameInstrumenter();
	HRESULT Init(AM_MEDIA_TYPE* pMediaType);
	HRESULT Instrument(IMediaSample* pSample, DWORD pFrameNumber);

private:
	AM_MEDIA_TYPE m_mt;
	GUID m_BitmapSubType;
	HBITMAP m_hBitmap[NBITMAPS];
	BITMAPINFO m_SrcBmi[NBITMAPS];
	BYTE* m_pSrcBits[NBITMAPS];
	int m_BltY;
};

#endif
