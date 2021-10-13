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
#include <windows.h>
#include <commdlg.h>
#include <streams.h>
#include <stdio.h>
#include <dvdmedia.h>
#include "DVRTestSink.h"

#ifndef AM_RATE_QueryFullFrameRate
#define AM_RATE_QueryFullFrameRate 6
#endif

CVideoInputPin::CVideoInputPin(    CDVRTestSinkFilter *pOwnerFilter,
                                LPUNKNOWN pUnk,
                                CCritSec *pLock,
                                HRESULT *phr)
    :    CBaseInputPin(    NAME("CVideoInputPin"),
                        pOwnerFilter,
                        pLock,
                        phr,
                        L"Video"),
        m_pOwnerFilter(pOwnerFilter),
        m_Lock(pLock),
        m_bValidRenderedPosition(FALSE),
        m_bValidDiscontinuityPosition(FALSE),
        m_bDiscontinuity(FALSE),
        m_rtRenderedPosition(0),
        m_rtDiscontinuityPosition(0)
{
}

STDMETHODIMP CVideoInputPin::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
    CheckPointer(ppv, E_POINTER);

    if (riid == IID_IKsPropertySet)
    {
        return GetInterface((IKsPropertySet *)this, ppv);
    }

    return CBaseInputPin::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CVideoInputPin::Receive(IMediaSample *pMediaSample)
{
    if(!pMediaSample)
        return E_FAIL;

    REFERENCE_TIME rtStart, rtEnd;

    if(SUCCEEDED(pMediaSample->GetTime (&rtStart, &rtEnd)))
    {
        m_rtRenderedPosition = rtStart;
        m_bValidRenderedPosition = TRUE;
    }
    else
    {
        m_bValidRenderedPosition = FALSE;
    }

    if(m_bDiscontinuity)
    {
        if(m_bValidRenderedPosition)
        {
            m_rtDiscontinuityPosition = rtStart;
            m_bValidDiscontinuityPosition = TRUE;
        }
        else
        {
            m_bValidDiscontinuityPosition = FALSE;
        }
        m_bDiscontinuity = FALSE;
    }

    // the DVR nav filter releases the samples after they're sent
    //pMediaSample->Release();
    return S_OK;
}

HRESULT CVideoInputPin::CheckMediaType(const CMediaType *pMT)
{

    if (pMT->majortype == MEDIATYPE_DVD_ENCRYPTED_PACK &&
        pMT->subtype == MEDIASUBTYPE_MPEG2_VIDEO)
    {
        return S_OK ;
    }

    return E_FAIL;
}

STDMETHODIMP CVideoInputPin::EndFlush(void)
{
    m_bDiscontinuity = TRUE;
    return CBaseInputPin::EndFlush();
}

void CVideoInputPin::SetDiscontinuity()
{
    m_bDiscontinuity = TRUE;
}

HRESULT CVideoInputPin::GetRenderedPosition (REFERENCE_TIME *pCurrentPos)
{
    HRESULT hr = VFW_E_MEDIA_TIME_NOT_SET;
    if (!pCurrentPos)
    {
        return E_INVALIDARG;
    }

    if (m_bValidRenderedPosition)
    {
        *pCurrentPos = m_rtRenderedPosition;
        hr = S_OK;
    }

    return hr;
}

HRESULT CVideoInputPin::GetDiscontinuityPosition (REFERENCE_TIME *pDiscontinuityPos)
{
    HRESULT hr = VFW_E_MEDIA_TIME_NOT_SET;
    if (!pDiscontinuityPos)
    {
        return E_INVALIDARG;
    }

    if (m_bValidDiscontinuityPosition)
    {
        *pDiscontinuityPos = m_rtDiscontinuityPosition;
        hr = S_OK;
    }

    return hr;
}

STDMETHODIMP
CVideoInputPin::Set(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceLength, LPVOID pPropData, DWORD cbPropData)
{
    HRESULT hr = E_PROP_ID_UNSUPPORTED;

    if (guidPropSet == AM_KSPROPSETID_TSRateChange)
    {
        if(dwPropID == AM_RATE_SimpleRateChange)
        {
            hr = S_OK;
            OutputDebugString(TEXT("Received a request to set a simple rate change, ignored"));
        }
        else OutputDebugString(TEXT("Received a request to set an unsupported property"));
    }
    else OutputDebugString(TEXT("Received a request to set an unsupported property set"));

    return hr;
}

STDMETHODIMP
CVideoInputPin::Get(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceLength, LPVOID pPropData, DWORD cbPropData, DWORD *pcbReturned)
{
    HRESULT hr = E_PROP_ID_UNSUPPORTED;

    if (guidPropSet == AM_KSPROPSETID_TSRateChange)
    {
        if(dwPropID == AM_RATE_MaxFullDataRate)
        {
            hr = S_OK;
        }
        else if(dwPropID == AM_RATE_QueryFullFrameRate)
        {
            hr = S_OK;
        }
    }
    else if (guidPropSet == AM_KSPROPSETID_RendererPosition)    
    {
        if (!pPropData || (cbPropData < sizeof(LONGLONG)))
        {
            hr = E_INVALIDARG;
        }
        else
        {
            if (dwPropID == AM_PROPERTY_CurrentPosition)
            {
                LONGLONG *pProp =  (LONGLONG *)pPropData;
                if (pcbReturned)
                {
                    *pcbReturned = sizeof(LONGLONG);
                }
                hr = GetRenderedPosition(pProp);

            }
            else if (dwPropID == AM_PROPERTY_DiscontinuityPosition)
            {
                LONGLONG *pProp =  (LONGLONG *)pPropData;
                if (pcbReturned)
                {
                    *pcbReturned = sizeof(LONGLONG);
                }
                hr = GetDiscontinuityPosition(pProp);
            }
        }
    }

    return hr;
}

STDMETHODIMP
CVideoInputPin::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, ULONG *pTypeSupport)
{
    HRESULT hr = E_PROP_ID_UNSUPPORTED;
    ULONG TypeSupport = 0;

    if (guidPropSet == AM_KSPROPSETID_TSRateChange)
    {
        if(dwPropID == AM_RATE_MaxFullDataRate ||
            dwPropID == AM_RATE_QueryFullFrameRate)
        {
            TypeSupport = KSPROPERTY_SUPPORT_GET ;
            hr = S_OK;
        }
        else if(dwPropID == AM_RATE_SimpleRateChange)
        {
            TypeSupport = KSPROPERTY_SUPPORT_SET ;
            hr = S_OK;
        }
    }
    else if (guidPropSet == AM_KSPROPSETID_RendererPosition)
    {
        if(dwPropID == AM_PROPERTY_CurrentPosition ||
            dwPropID == AM_PROPERTY_DiscontinuityPosition)
        {
            TypeSupport = KSPROPERTY_SUPPORT_GET ;
            hr = S_OK;
        }
    }

    if(SUCCEEDED(hr) && pTypeSupport)
        *pTypeSupport = TypeSupport;

    return hr;
}

CAudioInputPin::CAudioInputPin(    CDVRTestSinkFilter *pOwnerFilter,
                                LPUNKNOWN pUnk,
                                CCritSec *pLock,
                                HRESULT *phr)
    :    CBaseInputPin(    NAME("CAudioInputPin"),
                        pOwnerFilter,
                        pLock,
                        phr,
                        L"Audio"),
        m_pOwnerFilter(pOwnerFilter),
        m_Lock(pLock)
{
}

STDMETHODIMP CAudioInputPin::Receive(IMediaSample *pMediaSample)
{
    if(!pMediaSample)
        return E_FAIL;

    // the DVR nav filter releases the samples after they're sent
    //pMediaSample->Release();

    return S_OK;
}

// Make sure the supplied type an ASF stream.
HRESULT CAudioInputPin::CheckMediaType(const CMediaType *pMT)
{
    if (pMT->majortype == MEDIATYPE_DVD_ENCRYPTED_PACK &&
            (pMT->subtype == MEDIASUBTYPE_DOLBY_AC3 || 
            pMT->subtype == MEDIASUBTYPE_MPEG2_AUDIO ||
            pMT->subtype == MEDIASUBTYPE_DVD_LPCM_AUDIO))
    {
        return S_OK ;
    }

    return E_FAIL;
}

