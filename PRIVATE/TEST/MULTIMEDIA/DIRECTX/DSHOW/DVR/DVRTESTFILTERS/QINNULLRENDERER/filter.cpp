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
#include <initguid.h>
#include <atlbase.h>
#include "QinNullRenderer.h"
#include <dvdmedia.h>

// {155BEAF8-7FB3-4db5-885D-540E395EDDD3}
DEFINE_GUID(CLSID_QinNullRenderer, 
0x155BEAF8, 0x7FB3, 0x4db5, 0x88, 0x5D, 0x54, 0x0E, 0x39, 0x5E, 0xDD, 0xD3);

// a higher than Thread_priority_time_critical priority because we need to release the media samples
// as regularly as possible.
static DWORD s_dwVideoThreadPriority = 240;

// Setup data
const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
    &MEDIATYPE_NULL,            // Major type
    &MEDIASUBTYPE_NULL          // Minor type
};

const AMOVIESETUP_PIN sudPins =
{
    L"Null",                     // Pin string name
    FALSE,                      // Is it rendered
    FALSE,                      // Is it an output
    FALSE,                      // Allowed none
    FALSE,                      // Likewise many
    &CLSID_NULL,                // Connects to filter
    NULL,                        // Connects to pin
    1,                          // Number of types
    &sudPinTypes                // Pin information
};

const AMOVIESETUP_FILTER sudNULL =
{
    &CLSID_QinNullRenderer,     // Filter CLSID
    L"Qin Null Renderer",        // String name
    MERIT_DO_NOT_USE,           // Filter merit
    1,                          // Number pins
    &sudPins                    // Pin details
};


//
//  Object creation stuff
//
CFactoryTemplate g_Templates[]= {
    L"Qin Null Renderer", &CLSID_QinNullRenderer, CNullFilter::CreateInstance, NULL, &sudNULL
};
int g_cTemplates = 1;

//  CNullFilter constructor.  Create the filter and pins.
CNullFilter::CNullFilter(LPUNKNOWN pUnk, HRESULT *phr) :
    CBaseRenderer(CLSID_QinNullRenderer, NAME("CNullFilter"), pUnk, phr)
{
    m_bValidRenderedPosition = FALSE;
    m_bValidDiscontinuityPosition = FALSE;
    m_bDiscontinuity = TRUE;
    m_bEnableIKSPropertySet = TRUE;
}

// Provide the way for COM to create the NULL filter
CUnknown * WINAPI CNullFilter::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    HRESULT hr;
    if (!phr)
        phr = &hr;
    
    CNullFilter *pNewObject = new CNullFilter(punk, phr);
    if (pNewObject == NULL)
        *phr = E_OUTOFMEMORY;

    return pNewObject;
}

STDMETHODIMP CNullFilter::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
    CheckPointer(ppv, E_POINTER);

    if(riid == IID_INullFilter)
    {
        return GetInterface((INullFilter *)this, ppv);
    }
    else if(m_bEnableIKSPropertySet && riid == IID_IKsPropertySet)
    {
        return GetInterface((IKsPropertySet *)this, ppv);
    }

    return CBaseRenderer::NonDelegatingQueryInterface(riid, ppv);
}

// Make sure the supplied type an ASF stream.
HRESULT CNullFilter::CheckMediaType(const CMediaType *pMT)
{
    m_MT.majortype = pMT->majortype;
    m_MT.subtype = pMT->subtype;
    m_MT.formattype = pMT->formattype;
    return S_OK;    // Null is cool because all types are OK for it.
}

HRESULT CNullFilter::Receive(IMediaSample *pSample)
{
    ASSERT(pSample);

    DWORD dwCurrentPriority = CeGetThreadPriority(GetCurrentThread());

    if (dwCurrentPriority != s_dwVideoThreadPriority)
        CeSetThreadPriority(GetCurrentThread(), s_dwVideoThreadPriority);

    // It may return VFW_E_SAMPLE_REJECTED code to say don't bother
    
    HRESULT hr = PrepareReceive(pSample);
    ASSERT(m_bInReceive == SUCCEEDED(hr));
    if (FAILED(hr)) {
        if (hr == VFW_E_SAMPLE_REJECTED) {
            return NOERROR;
        }
        return hr;
    }
    
    // We realize the palette in "PrepareRender()" so we have to give away the
    // filter lock here.
    if (m_State == State_Paused) {
        Ready();  // we have a sample and we're going to WaitForRenderTime, so we set evComplete now.

        // If the state is paused, we have to wait for the appropriate time for displaying the sample; 
        // it will be scheduled when StartStreaming happens.
        hr = WaitForRenderTime();
        if (FAILED(hr)) {
            m_bInReceive = FALSE;
            return NOERROR;
        }
        // do the same work for the first sample as in the normal case below
        PrepareRender();
        // no need to use InterlockedExchange
        m_bInReceive = FALSE;
        {
            // We must hold both these locks
            CAutoLock cRendererLock(&m_InterfaceLock);
            if (m_State == State_Stopped)
                return NOERROR;
            m_bInReceive = TRUE;
            CAutoLock cSampleLock(&m_RendererLock);
            OnReceiveFirstSample(pSample);
            m_bInReceive = FALSE;
            ClearPendingSample();
            SendEndOfStream();
            CancelNotification();
        }

        // Processed first sample
        return NOERROR;
    }
    // Having set an advise link with the clock we sit and wait. We may be
    // awoken by the clock firing or by a state change. The rendering call
    // will lock the critical section and check we can still render the data
    
    hr = WaitForRenderTime();
    if (FAILED(hr)) {
        m_bInReceive = FALSE;
        return NOERROR;
    }
    
    PrepareRender();
    
    //  Set this here and poll it until we work out the locking correctly
    //  It can't be right that the streaming stuff grabs the interface
    //  lock - after all we want to be able to wait for this stuff
    //  to complete
    m_bInReceive = FALSE;
    
    // We must hold both these locks
    CAutoLock cRendererLock(&m_InterfaceLock);
    
    // since we gave away the filter wide lock, the sate of the filter could
    // have chnaged to Stopped
    if (m_State == State_Stopped)
        return NOERROR;
    
    CAutoLock cSampleLock(&m_RendererLock);
    
    // Deal with this sample
    
    Render(m_pMediaSample);
    ClearPendingSample();
    SendEndOfStream();
    CancelNotification();
    return NOERROR;
}

HRESULT CNullFilter::DoRenderSample(IMediaSample *pMediaSample)
{
    REFERENCE_TIME rtStart, rtEnd;

    // if the IKsPropertySet interface is disabled, then we can just return.
    if(!m_bEnableIKSPropertySet)
        return S_OK;

    if(SUCCEEDED(pMediaSample->GetTime (&rtStart, &rtEnd)))
    {
        m_rtRenderedPosition = rtStart;
        m_bValidRenderedPosition = TRUE;

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
    }

    return S_OK;
}

HRESULT CNullFilter::EndFlush()
{
    m_bDiscontinuity = TRUE;
    return CBaseRenderer::EndFlush();
}

STDMETHODIMP CNullFilter::Stop()
{
    m_bDiscontinuity = TRUE;
    return CBaseRenderer::Stop();
}

STDMETHODIMP CNullFilter::Run(REFERENCE_TIME rt)
{
    m_bDiscontinuity = TRUE;
    return CBaseRenderer::Run(rt);
}







HRESULT CNullFilter::GetRenderedPosition (REFERENCE_TIME *pCurrentPos)
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

HRESULT CNullFilter::GetDiscontinuityPosition (REFERENCE_TIME *pDiscontinuityPos)
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
CNullFilter::Set(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceLength, LPVOID pPropData, DWORD cbPropData)
{
    return E_NOTIMPL;
}


STDMETHODIMP
CNullFilter::Get(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceLength, LPVOID pPropData, DWORD cbPropData, DWORD *pcbReturned)
{
    HRESULT hr = E_PROP_SET_UNSUPPORTED;

    if (guidPropSet == AM_KSPROPSETID_RendererPosition)    
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
            else
            {
                hr = E_PROP_ID_UNSUPPORTED;
            }
        }
    }
    return hr;
}


STDMETHODIMP
CNullFilter::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, ULONG *pTypeSupport)
{
    if (guidPropSet != AM_KSPROPSETID_RendererPosition)
    {
        return E_PROP_SET_UNSUPPORTED ;
    }
    
    if (dwPropID != AM_PROPERTY_CurrentPosition  &&
        dwPropID != AM_PROPERTY_DiscontinuityPosition)
    {
        return E_PROP_ID_UNSUPPORTED ;
    }
    
    if (pTypeSupport)
    {
        *pTypeSupport = KSPROPERTY_SUPPORT_GET ;
    }

    return S_OK;
}

// AMovieDllRegisterServer will call back into this to get pin info.
AMOVIESETUP_FILTER *
CNullFilter::GetSetupData()
{
    return (AMOVIESETUP_FILTER *)&sudNULL;
}

HRESULT
CNullFilter::DisableIKSPropertySet()
{
    m_bEnableIKSPropertySet = FALSE;
    return S_OK;
}

HRESULT
CNullFilter::EnableIKSPropertySet()
{
    m_bEnableIKSPropertySet = TRUE;
    return S_OK;
}

////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

//
// DllRegisterSever
//
// Handle the registration of this filter
//
STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2( TRUE );
} // DllRegisterServer


//
// DllUnregisterServer
//
STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2( FALSE );
} // DllUnregisterServer


//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                    DWORD  dwReason, 
                    LPVOID lpReserved)
{
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

