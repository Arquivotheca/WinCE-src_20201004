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
#ifndef VIDEOSTAMPREADER_H
#define VIDEOSTAMPREADER_H

#include "videostamp.h"

typedef HRESULT VSREADERCALLBACK(IMediaSample * pms, void *pData);

MIDL_INTERFACE("E6522D7F-76D9-4c63-8160-F9BFF37F59E5")
IVideoStampReaderFilter : public IUnknown
{
    public:
    virtual HRESULT SetTemplateBitmap(TCHAR *tszTemplate) = 0;
    virtual HRESULT SetTemplateBitmap(TCHAR *tszTemplate, RECT *rcFrameID, RECT *rcTimestamp) = 0;
    virtual HRESULT SetFont(LOGFONT *FontIn);
    virtual HRESULT GetPresentationTime(REFERENCE_TIME *pMediaTime) = 0;
    virtual HRESULT GetFrameID(int *nFrameID) = 0;
    virtual HRESULT GetDescription(TCHAR *tszDescription, UINT nLength) = 0;
    virtual HRESULT GetBitmap(BYTE *pBits, BITMAPINFO *pbmi, int stride, int X, int Y, int width, int height, GUID MediaSubtype) = 0;
    virtual int GetDiscontinuityCount() = 0;
    virtual HRESULT IsContinuous(BOOL *bContinuous) = 0;
    virtual HRESULT SetCallback(VSREADERCALLBACK *pCallback, void *pData) = 0;
    virtual void SetScale(float ScaleIn);
    virtual void SetRotation(DWORD RotationIn);
};

class CVideoStampReaderOutPin;
class CVideoStampReaderFilter;

class CVideoStampReaderOutPin : public CTransInPlaceOutputPin
{

public:

    CVideoStampReaderOutPin( CTransInPlaceFilter * pFilter, HRESULT * pHr ) 
        : CTransInPlaceOutputPin( TEXT("VideoStampReaderOutputPin\0"), pFilter, pHr, L"Output\0" )
    {
    }

    ~CVideoStampReaderOutPin( )
    {
    }

    HRESULT DecideAllocator(IMemInputPin *pPin, IMemAllocator **ppAlloc);
};

class CVideoStampReaderFilter : public CTransInPlaceFilter,
                       public IVideoStampReaderFilter
{
    friend class CVideoStampReaderOutPin;

protected:
    CCritSec m_Lock;
    CVideoStampReader VideoStampReader;
    TCHAR m_tszTemplateBitmap[MAX_PATH];
    VSREADERCALLBACK *m_pUserCallback;
    void *m_pUserData;

public:
    CVideoStampReaderFilter( IUnknown * pOuter, HRESULT * pHr, BOOL ModifiesData );
    HRESULT Transform(IMediaSample * pms);
    HRESULT CheckInputType(const CMediaType *mtIn);
    HRESULT SetMediaType(PIN_DIRECTION direction, const CMediaType *pmt);

    static CUnknown *WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
    DECLARE_IUNKNOWN;

#ifdef UNDER_CE
    virtual LPAMOVIESETUP_FILTER GetSetupData();
#endif

    HRESULT SetTemplateBitmap(TCHAR *tszTemplate);
    HRESULT SetTemplateBitmap(TCHAR *tszTemplate, RECT *rcFrameID, RECT *rcTimestamp);
    HRESULT SetFont(LOGFONT *FontIn);
    HRESULT GetPresentationTime(REFERENCE_TIME *pMediaTime);
    HRESULT GetFrameID(int *nFrameID);
    HRESULT GetDescription(TCHAR *tszDescription, UINT nLength);
    HRESULT GetBitmap(BYTE *pBits, BITMAPINFO *pbmi, int stride, int X, int Y, int width, int height, GUID MediaSubtype);
    int GetDiscontinuityCount();
    HRESULT IsContinuous(BOOL *bContinuous);
    HRESULT SetCallback(VSREADERCALLBACK *pCallback, void *pData);
    void SetScale(float ScaleIn){m_Scaling = ScaleIn;}
    void SetRotation(DWORD RotationIn){m_Rotation = RotationIn;}

private:
    float m_Scaling;
    DWORD m_Rotation;
    LOGFONT m_Font;
    BOOL m_bSelectFont;

    RECT m_rcFrameIDRect;
    RECT m_rcTimestampRect;
    BOOL m_bRectSelected;
};

#endif
