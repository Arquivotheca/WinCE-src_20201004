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
//------------------------------------------------------------------------------
// File: ImageTestSink.h
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef IMAGETESTSINK_H
#define IMAGETESTSINK_H

#include <streams.h>

#include "TestInterfaces.h"

DEFINE_GUID(CLSID_ImageTestSink, 
0x27175db, 0x4da3, 0x451e, 0x87, 0xd4, 0x98, 0xdb, 0x82, 0x31, 0xb6, 0x36);

// the filter class (defined below)
extern const AMOVIESETUP_FILTER ImageTestSinkDesc;

class CImageTestSink : public ITestSink, CBaseRenderer
{
public:
    CImageTestSink(LPUNKNOWN pUnk, HRESULT *phr);

    virtual ~CImageTestSink();

    static CUnknown * WINAPI CreateInstance(LPUNKNOWN, HRESULT *);

    DECLARE_IUNKNOWN

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);
    
    // CBaseFilter pure virtual methods
    virtual int GetPinCount();
    virtual CBasePin *GetPin(int n);
    virtual HRESULT DoRenderSample(IMediaSample *pMediaSample);

    virtual HRESULT BeginFlush(void);
    virtual HRESULT EndFlush(void);

    virtual HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
    virtual HRESULT CheckMediaType(const CMediaType* pType);
    virtual HRESULT Receive(IMediaSample *pSample);

    // CBasePin methods
    virtual HRESULT EndOfStream();

    // ITestSink implementation
#ifdef UNDER_CE
    virtual LPAMOVIESETUP_FILTER GetSetupData();
#endif

    virtual HRESULT SetVerifier(IVerifier* pVerifier);
    virtual IVerifier* GetVerifier();
    virtual int GetNumMediaTypeCombinations();
    virtual HRESULT SelectMediaTypeCombination(int iPosition);
    virtual HRESULT GetMediaTypeCombination(int iPosition, AM_MEDIA_TYPE** ppMediaType);
    virtual int GetSelectedMediaTypeCombination();
    virtual bool SetInvalidConnectionParameters();
    virtual bool SetInvalidAllocatorParameters();

private:
    CCritSec m_Lock;
    IVerifier* m_pVerifier;
    int m_SelectedMediaType;
};

#endif //_IMAGE_TEST_SINK_H