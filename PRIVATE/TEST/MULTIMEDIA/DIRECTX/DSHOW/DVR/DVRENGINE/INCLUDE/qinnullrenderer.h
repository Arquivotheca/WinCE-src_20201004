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

#pragma once

// Macros to handle error checking.
#define _CHR(e) {HRESULT _hr=(e); if (FAILED(_hr)) {DebugBreak(); return _hr;}}
#define _CHR_NOASSERT(e) {HRESULT _hr=(e); if (FAILED(_hr)) {return _hr;}}
#define _CP(e)  {void *p=(e); if (!p) {DebugBreak(); return E_POINTER;}}

// {6D634A79-873A-4fc4-90A5-F0F1BAECA39D}
DEFINE_GUID(IID_INullFilter, 
0x6d634a79, 0x873a, 0x4fc4, 0x90, 0xa5, 0xf0, 0xf1, 0xba, 0xec, 0xa3, 0x9d);


// We define the interface the app can use to program us
MIDL_INTERFACE("6D634A79-873A-4fc4-90A5-F0F1BAECA39D")
INullFilter : public IUnknown
{
    public:
        virtual HRESULT DisableIKSPropertySet() = 0;
        virtual HRESULT EnableIKSPropertySet() = 0;
};


// ##########################################

class CNullFilter : public CBaseRenderer, public IKsPropertySet, public INullFilter
{
public:
    DECLARE_IUNKNOWN;

    CNullFilter(LPUNKNOWN pUnk, HRESULT *phr);
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    HRESULT CheckMediaType(const CMediaType *pMT);
    HRESULT Receive(IMediaSample *pSample);
    HRESULT DoRenderSample(IMediaSample *pMediaSample);

    HRESULT EndFlush();
    STDMETHODIMP Stop();
    STDMETHODIMP Run(REFERENCE_TIME rt);

    STDMETHODIMP Set(REFGUID guidPropSet, DWORD PropID, LPVOID pInstanceData, 
                     DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData) ;
    STDMETHODIMP Get(REFGUID guidPropSet, DWORD PropID, LPVOID pInstanceData, 
                     DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData,
                     DWORD *pcbReturned) ;
    STDMETHODIMP QuerySupported(REFGUID guidPropSet, DWORD PropID, DWORD *pTypeSupport) ;

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

    AMOVIESETUP_FILTER *GetSetupData();

    HRESULT DisableIKSPropertySet();
    HRESULT EnableIKSPropertySet();

private:
    CMediaType m_MT;                // Connected Media type

    BOOL m_bValidRenderedPosition;
    BOOL m_bValidDiscontinuityPosition;
    BOOL m_bDiscontinuity;
    BOOL m_bEnableIKSPropertySet;
    REFERENCE_TIME m_rtRenderedPosition;
    REFERENCE_TIME m_rtDiscontinuityPosition;

    HRESULT GetRenderedPosition (REFERENCE_TIME *pCurrentPos);
    HRESULT GetDiscontinuityPosition (REFERENCE_TIME *pDiscontinuityPos);
};

