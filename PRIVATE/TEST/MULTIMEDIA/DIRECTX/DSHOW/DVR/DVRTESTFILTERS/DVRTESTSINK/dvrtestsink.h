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
#ifndef DVRTESTSINK_H
#define DVRTESTSINK_H

#include <atlbase.h>

class CDVRTestSinkFilter;

class CVideoInputPin : public CBaseInputPin, public IKsPropertySet
{

public:
    DECLARE_IUNKNOWN;

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

    CVideoInputPin(    CDVRTestSinkFilter *pDump,
                    LPUNKNOWN pUnk,
                    CCritSec *pLock,
                    HRESULT *phr);

    // Do something with this media sample
    STDMETHODIMP Receive(IMediaSample *pSample);

    HRESULT CheckMediaType(const CMediaType *);
    STDMETHODIMP EndFlush(void);
    void SetDiscontinuity();

    HRESULT GetRenderedPosition (REFERENCE_TIME *pCurrentPos);
    HRESULT GetDiscontinuityPosition (REFERENCE_TIME *pDiscontinuityPos);


    STDMETHODIMP Set(REFGUID guidPropSet, DWORD PropID, LPVOID pInstanceData, 
                     DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData) ;
    STDMETHODIMP Get(REFGUID guidPropSet, DWORD PropID, LPVOID pInstanceData, 
                     DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData,
                     DWORD *pcbReturned) ;
    STDMETHODIMP QuerySupported(REFGUID guidPropSet, DWORD PropID, DWORD *pTypeSupport) ;

private:
    CDVRTestSinkFilter * const m_pOwnerFilter;        // Main renderer object
    CCritSec * const m_Lock;    // Sample critical section

    BOOL m_bValidRenderedPosition;
    BOOL m_bValidDiscontinuityPosition;
    BOOL m_bDiscontinuity;
    REFERENCE_TIME m_rtRenderedPosition;
    REFERENCE_TIME m_rtDiscontinuityPosition;
};

class CAudioInputPin : public CBaseInputPin
{

public:

    CAudioInputPin(    CDVRTestSinkFilter *pDump,
                    LPUNKNOWN pUnk,
                    CCritSec *pLock,
                    HRESULT *phr);

    // Do something with this media sample
    STDMETHODIMP Receive(IMediaSample *pSample);

    HRESULT CheckMediaType(const CMediaType *);

private:
    CDVRTestSinkFilter * const m_pOwnerFilter;        // Main renderer object
    CCritSec * const m_Lock;    // Sample critical section
};

class CDVRTestSinkFilter : public CBaseFilter
{
    friend class CVideoInputPin;

public:
    DECLARE_IUNKNOWN;

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

    CDVRTestSinkFilter(LPUNKNOWN pUnk, HRESULT *phr);
    ~CDVRTestSinkFilter();
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    // Pin accessors
    CBasePin *GetPin(int n);
    int GetPinCount();
    STDMETHODIMP Stop();
    STDMETHODIMP Run(REFERENCE_TIME rt);

    AMOVIESETUP_FILTER *GetSetupData();

private:
    CCritSec m_Lock;                    // Main renderer critical section
    CVideoInputPin *m_pVideoInputPin;
    CAudioInputPin *m_pAudioInputPin;
};
#endif

