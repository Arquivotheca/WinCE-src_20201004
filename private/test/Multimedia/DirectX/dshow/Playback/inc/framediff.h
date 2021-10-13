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
////////////////////////////////////////////////////////////////////////////////
//
//  Frame diff
//
//
////////////////////////////////////////////////////////////////////////////////

#ifndef FRAME_DIFF_H
#define FRAME_DIFF_H

#define 	NBITMAPS  12

class FrameDiff
{
public:
    FrameDiff();
    ~FrameDiff();
    HRESULT Init(AM_MEDIA_TYPE* pMediaType);
    void Reset();
    HRESULT Compare(IMediaSample* pMediaSample, int iFrameNum, int nSamples);
    HRESULT SaveDiff(BYTE* pTargetBuffer, BITMAPINFO *pTargetBm, int iBmpIndex, int iFrameNumber);

private:
    AM_MEDIA_TYPE m_mt;
    GUID m_BitmapSubType;
    HBITMAP m_hBitmap[NBITMAPS];
    BITMAPINFO m_SrcBmi[NBITMAPS];
    BYTE* m_pSrcBits[NBITMAPS];
    int m_SrcY;
};


#endif
