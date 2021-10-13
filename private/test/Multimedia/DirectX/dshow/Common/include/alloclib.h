//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/******************************Module*Header*******************************\
* Module Name: AllocLib.h
*
*
*
*
* Created: Fri 03/10/2000
* Author:  Stephen Estrop [StEstrop]
*
\**************************************************************************/


#ifndef __INC_ALLOCLIB_H__
#define __INC_ALLOCLIB_H__

#ifndef __ZEROSTRUCT_DEFINED
#define __ZEROSTRUCT_DEFINED
template <typename T>
__inline void ZeroStruct(T& t)
{
    ZeroMemory(&t, sizeof(t));
}
#endif

#ifndef __INITDDSTRUCT_DEFINED
#define __INITDDSTRUCT_DEFINED
template <typename T>
__inline void INITDDSTRUCT(T& dd)
{
    ZeroStruct(dd);
    dd.dwSize = sizeof(dd);
}
#endif

#if 0
template< typename T >
__inline CopyStruct(T& pDest, const T& pSrc)
{
    CopyMemory( &pDest, &pSrc, sizeof(pSrc));
}
#endif

BOOL
IsSubStreamFormat(
    DWORD Format
    );

const DWORD*
GetBitMasks(
    const BITMAPINFOHEADER *pHeader
    );

LPBITMAPINFOHEADER
GetbmiHeader(
    const AM_MEDIA_TYPE *pMediaType
    );

void
FixupMediaType(
    AM_MEDIA_TYPE* pmt
    );

LPRECT
GetTargetRectFromMediaType(
    const AM_MEDIA_TYPE *pMediaType
    );

struct TargetScale
{
    float fX;
    float fY;
};

void
GetTargetScaleFromMediaType(
    const AM_MEDIA_TYPE *pMediaType,
    TargetScale* pScale
    );

LPRECT
GetSourceRectFromMediaType(
    const AM_MEDIA_TYPE *pMediaType
    );

HRESULT
ConvertSurfaceDescToMediaType(
    const LPDDSURFACEDESC pSurfaceDesc,
    const AM_MEDIA_TYPE* pTemplateMediaType,
    AM_MEDIA_TYPE** ppMediaType
    );

HRESULT
GetImageAspectRatio(
    const AM_MEDIA_TYPE* pMediaType,
    long* lpARWidth,
    long* lpARHeight
    );


bool
EqualSizeRect(
    const RECT* lpRc1,
    const RECT* lpRc2
    );


bool
ContainedRect(
    const RECT* lpRc1,
    const RECT* lpRc2
    );

void
LetterBoxDstRect(
    LPRECT lprcLBDst,
    const RECT& rcSrc,
    const RECT& rcDst,
    LPRECT lprcBdrTL,
    LPRECT lprcBdrBR
    );


void
AspectRatioCorrectSize(
    LPSIZE lpSizeImage,
    const SIZE& sizeAr,
    const SIZE& sizeOrig,
    BOOL ScaleXorY = FALSE
    );

enum {
    TXTR_POWER2     = 0x01,
    TXTR_AGPYUVMEM  = 0x02,
    TXTR_AGPRGBMEM  = 0x04,
    TXTR_SRCKEY     = 0x08
};

HRESULT
GetTextureCaps(
    LPDIRECTDRAW pDD,
    DWORD* ptc
    );

DWORD
DDColorMatch(
    IDirectDrawSurface *pdds,
    COLORREF rgb,
    HRESULT& hr
    );

HRESULT
VMRCopyFourCC(
    LPDIRECTDRAWSURFACE lpDst,
    LPDIRECTDRAWSURFACE lpSrc
    );


HRESULT
GetInterlaceFlagsFromMediaType(
    const AM_MEDIA_TYPE *pMediaType,
    DWORD *pdwInterlaceFlags
    );

BOOL
NeedToFlipOddEven(
    DWORD dwInterlaceFlags,
    DWORD dwTypeSpecificFlags,
    DWORD *pdwFlipFlag,
    BOOL bUsingOverlays
    );

BOOL
IsSingleFieldPerSample(
    DWORD dwFlags
    );

REFERENCE_TIME
GetAvgTimePerFrame(
    const AM_MEDIA_TYPE *pMT
    );

#ifdef DEBUG
void __inline DumpDDSAddress(const TCHAR *szText, LPDIRECTDRAWSURFACE lpDDS)
{
    DDSURFACEDESC ddSurfaceDesc;
    INITDDSTRUCT(ddSurfaceDesc);

    HRESULT hr;

#ifdef UNDER_CE
    hr = lpDDS->Lock(NULL, &ddSurfaceDesc, DDLOCK_WAITNOTBUSY, (HANDLE)NULL);
#else
    hr = lpDDS->Lock(NULL, &ddSurfaceDesc, DDLOCK_NOSYSLOCK | DDLOCK_WAIT, (HANDLE)NULL);
#endif
    if (hr != DD_OK) {
        DbgLog((LOG_TRACE, 0, TEXT("Lock failed hr = %#X"), hr));
    }

    hr = lpDDS->Unlock(NULL);
    if (hr != DD_OK) {
        DbgLog((LOG_TRACE, 0, TEXT("Unlock failed hr = %#X"), hr));
    }

    DbgLog((LOG_TRACE, 0, TEXT("%s%p"), szText, ddSurfaceDesc.lpSurface));
}
#else
#define DumpDDSAddress( _x_, _y_ )
#endif


#endif
