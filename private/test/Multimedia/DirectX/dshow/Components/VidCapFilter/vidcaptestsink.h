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
// File: VidCapTestSink.h
//
//------------------------------------------------------------------------------

#ifndef _VIDCAP_TEST_SINK_H
#define _VIDCAP_TEST_SINK_H

#include <streams.h>

#include "VidCapTestUids.h"
#include "TestInterfaces.h"
#include "vidcapverifier.h"

class IVidCapTestSink : public ITestSink
{
public:
    virtual CVidCapVerifier *GetVidCapVerifier() = 0;
};


// the filter class (defined below)
class CVidCapTestSink;
extern const AMOVIESETUP_FILTER VidCapTestSinkDesc;

#define MAX_VID_SINK_PINS    3

// the output pin class
class CVidCapTestInputPin
  : public CBaseInputPin
{
public:
    // constructor and destructor
    CVidCapTestInputPin(HRESULT * phr, CVidCapTestSink* pFilter, CCritSec* pLock, int index);

    virtual ~CVidCapTestInputPin();

    // --- CUnknown ---
    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID, void**);

    // --- CBaseInputPin methods ---
    virtual STDMETHODIMP BeginFlush(void);
    virtual STDMETHODIMP EndFlush(void);

    // return the types we prefer - this will return the known
    // file type
    virtual HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);

    // can we support this type?
    virtual HRESULT CheckMediaType(const CMediaType* pType);

    // IMemInputPin methods
    virtual STDMETHODIMP Receive(IMediaSample *pSample);

    // CBasePin methods
    virtual STDMETHODIMP EndOfStream();

private:
    CVidCapTestSink* m_pTestSink;
    int m_Index;
    bool m_bFirstSampleDelivered;
};

// the test sink needs to expose an IAMFilterMiscFlag to indicate that it's a renderer to
// the filtergraph manager as neither the vid cap filter or the vid cap test sink expose the
// IMediaSeeking interface.
class CVidCapTestSink : public IVidCapTestSink, public CBaseFilter,  public IAMFilterMiscFlags
{
    friend CVidCapTestInputPin;
public:
    CVidCapTestSink(LPUNKNOWN pUnk, HRESULT *phr, int nInputs = 3);

    virtual ~CVidCapTestSink();

    static CUnknown * WINAPI CreateInstance(LPUNKNOWN, HRESULT *);

    DECLARE_IUNKNOWN

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

    // CBaseFilter pure virtual methods
    virtual int GetPinCount();
    virtual CBasePin *GetPin(int n);

    // ITestSink implementation
    virtual HRESULT SetVerifier(IVerifier* pVerifier);
    virtual IVerifier* GetVerifier();
    virtual int GetNumMediaTypeCombinations();
    virtual HRESULT SelectMediaTypeCombination(int iPosition);
    virtual HRESULT GetMediaTypeCombination(int iPosition, AM_MEDIA_TYPE** ppMediaType);
    virtual int GetSelectedMediaTypeCombination();
    virtual bool SetInvalidConnectionParameters();
    virtual bool SetInvalidAllocatorParameters();

    virtual CVidCapVerifier *GetVidCapVerifier();

    //IAMFilterMiscFlags
    STDMETHOD_(ULONG, GetMiscFlags )(void);

private:
    CCritSec m_Lock;
    int m_nInputPins;
    CVidCapTestInputPin *m_pInputPin[MAX_VID_SINK_PINS];
    CVidCapVerifier* m_pVerifier;
    // Currently unused
    int m_SelectedMediaType[MAX_VID_SINK_PINS];
    int m_SelectedMediaTypeCombination;
};

#endif //_VIDCAP_TEST_SINK_H
