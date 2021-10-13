//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "av_upnp.h"

using namespace av_upnp;


/////////////////////////////////////////////////////////////////////////////
// ModeratedEventSupport

ModeratedEventSupport::ModeratedEventSupport(DWORD nMaxEventRate)
    : m_nMaxEventRate(nMaxEventRate)
{
    m_bTimerInitialized = (SUCCESS_AV == details::TimedEventCallerSingleton::instance().RegisterCallee(m_nMaxEventRate, this));
    
    // this can fail only in OOM - in this case we won't be reporting moderated events
    assert(m_bTimerInitialized);
}


ModeratedEventSupport::~ModeratedEventSupport()
{
    assert(m_punkSubscriber == NULL);
    
    if(m_bTimerInitialized)
        details::TimedEventCallerSingleton::instance().UnregisterCallee(m_nMaxEventRate, this);
}


void ModeratedEventSupport::AddRefModeratedEvent()
{
    if(m_bTimerInitialized)
        details::TimedEventCallerSingleton::instance().AddRef(m_nMaxEventRate);
}


void ModeratedEventSupport::ReleaseModeratedEvent()
{
    if(m_bTimerInitialized)
        details::TimedEventCallerSingleton::instance().Release(m_nMaxEventRate);
}


//
// IUPnPEventSource
//

STDMETHODIMP ModeratedEventSupport::Advise(/*[in]*/ IUPnPEventSink* punkSubscriber)
{
    ce::gate<ce::critical_section> gate(m_csPunkSubscriber);
    
    if(!punkSubscriber)
        return E_POINTER;

    return punkSubscriber->QueryInterface(IID_IUPnPEventSink, reinterpret_cast<void**>(&m_punkSubscriber));
}


STDMETHODIMP ModeratedEventSupport::Unadvise(/*[in]*/ IUPnPEventSink* punkSubscriber)
{
    ce::gate<ce::critical_section> gate(m_csPunkSubscriber);
    
    if(m_punkSubscriber)
        m_punkSubscriber.Release();

    return S_OK;
}
