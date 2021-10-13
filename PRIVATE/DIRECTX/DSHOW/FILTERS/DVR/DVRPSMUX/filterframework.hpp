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

#include <streams.h>
#include "queue.hpp"
#include "commontypes.hpp"
#include "outputpin.hpp"
#include "psmux.hpp"

#define AUDIO_INPUT_PIN_NAME L"ES Audio In"
#define VIDEO_INPUT_PIN_NAME L"ES Video In"
#define FILTER_NAME          L"DVR MPEG-2 PS Mux"

// {E9FA2A46-8CD8-4a5c-AE52-B82378703A69}
DEFINE_GUID(CLSID_MPEG2PSMuxFilter,
    0xe9fa2a46,0x8cd8,0x4a5c,0xae,0x52,0xb8,0x23,0x78,0x70,0x3a,0x69
    );


namespace DvrPsMux
{

//
// --------------- Filter_t ---------------
//

class InputPin_t;

class Filter_t :
    public CBaseFilter,
    public IPropertyBag
{
    CCritSec       m_Lock;
    InputPin_t*    m_pAudioInput;
    InputPin_t*    m_pVideoInput;
    OutputPin_t*   m_pOutput;
    Mux_t*         m_pMux;

public:
    DECLARE_IUNKNOWN

    STDMETHOD(NonDelegatingQueryInterface)(
        REFIID riid,
        void ** ppv
        );

    static
    CUnknown*
    CreateInstance(
        IUnknown* pUnk,
        HRESULT*  pHr
        );

    Filter_t(
        wchar_t*  pName,
        IUnknown* pUnk,
        HRESULT*  pHr
        );

    ~Filter_t();

    CBasePin*
    GetPin(
        int Index
        );

    int
    GetPinCount();

    AMOVIESETUP_FILTER*
    GetSetupData();

    STDMETHODIMP
    Run(
        REFERENCE_TIME tStart
        );

    STDMETHODIMP
    Stop();

    // IPropertyBag decls
    STDMETHOD(Read)(
        LPCOLESTR Name,
        VARIANT* pValue,
        IErrorLog* pErrorLog
        );

    STDMETHOD(Write)(
        LPCOLESTR Name,
        VARIANT* pValue
        );

    void NormalizePTS(
        IMediaSample *pSample,
        BOOL bVideo
        );
};


//
// -------------- InputPin_t --------------
//

class InputPin_t :
    public CBaseInputPin
{
    const AMOVIESETUP_PIN*     m_pPinInfo;
    StreamType_e               m_StreamType;
    IMuxInput*                 m_pMuxInput;
    Filter_t*                  m_pFilter;

public:
    InputPin_t(
        wchar_t*               pClassName,
        Filter_t*              pFilter,
        CCritSec*              pLock,
        HRESULT*               pHr,
        wchar_t*               pPinName,
        const AMOVIESETUP_PIN* pPinInfo,
        IMuxInput*             pMuxInput
        ) :
            CBaseInputPin(
                pClassName,
                pFilter,
                pLock,
                pHr,
                pPinName
                ),
            m_pPinInfo(pPinInfo),
            m_pMuxInput(pMuxInput),
            m_pFilter(pFilter)
    {
        ASSERT(m_pPinInfo && pMuxInput && m_pFilter);
    }

    ~InputPin_t()
    {
    }

    HRESULT
    CheckMediaType(
        const CMediaType* pMediaType
        );

    HRESULT
    GetMediaType(
        int         Position,
        CMediaType* pMediaType
        );

    HRESULT
    CompleteConnect(
        IPin *pReceivePin
        );

    STDMETHODIMP
    Receive(
        IMediaSample* pSample
        );

    STDMETHODIMP
    ReceiveCanBlock()
    {
        return S_FALSE;
    }

    STDMETHODIMP
    NewSegment(
        __int64 StartTime,
        __int64 StopTime,
        double Rate
        );
};

}

