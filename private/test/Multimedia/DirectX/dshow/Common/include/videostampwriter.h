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
#ifndef VIDEOSTAMPWRITER_H
#define VIDEOSTAMPWRITER_H

#include "videostamp.h"

MIDL_INTERFACE("66AAFF88-380E-4a78-A932-E1A10A8B500F")
IVideoStampWriterFilter : public IUnknown
{
    public:
    virtual HRESULT SetBitmap(BYTE *pBits, int nWidth, int nHeight, int nStride, int X, int Y, GUID MediaSubtype, BITMAPINFO *pbmi, int nDisplayFrameCount) = 0;
    virtual HRESULT SetDescription(TCHAR *tszDescription) = 0;
    virtual HRESULT CreateTemplate(TCHAR *tszOCRTemplate) = 0;
    virtual HRESULT CreateTemplateFromLogfont(TCHAR *tszOCRTemplate, LOGFONT *lfFont, RECT *rcFrameID, RECT *rcTimestamp) = 0;

    virtual void SetRotation(DWORD RotationIn);
    virtual HRESULT SetFont(LOGFONT *FontIn);
};

class CVideoStampWriterFilter : public CTransInPlaceFilter, public IVideoStampWriterFilter
{
private:
    CCritSec m_Lock;
    CVideoStampWriter VideoStampWriter;

public:
    CVideoStampWriterFilter( IUnknown * pOuter, HRESULT * pHr, BOOL ModifiesData );
    HRESULT Transform(IMediaSample * pms);
    HRESULT CheckInputType(const CMediaType *mtIn);
    HRESULT SetMediaType(PIN_DIRECTION direction, const CMediaType *pmt);

    static CUnknown *WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
    DECLARE_IUNKNOWN;

#ifdef UNDER_CE
    virtual LPAMOVIESETUP_FILTER GetSetupData();
#endif

    HRESULT SetBitmap(BYTE *pBits, int nWidth, int nHeight, int nStride, int X, int Y, GUID MediaSubtype, BITMAPINFO *pbmi, int nDisplayFrameCount);
    HRESULT SetDescription(TCHAR *tszDescription);
    HRESULT CreateTemplate(TCHAR *tszOCRTemplate);
    HRESULT CreateTemplateFromLogfont(TCHAR *tszOCRTemplate, LOGFONT *lfFont, RECT *rcFrameID, RECT *rcTimestamp);
    void SetRotation(DWORD RotationIn){m_Rotation = RotationIn;}
    HRESULT SetFont(LOGFONT *FontIn);

private:
    DWORD m_Rotation;
    LOGFONT m_Font;
    BOOL m_bSelectFont;
};

#endif
