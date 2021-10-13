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
    {
        details::TimedEventCallerSingleton::instance().UnregisterCallee(m_nMaxEventRate, this);
    }
}


void ModeratedEventSupport::AddRefModeratedEvent()
{
    if(m_bTimerInitialized)
    {
        details::TimedEventCallerSingleton::instance().AddRef(m_nMaxEventRate);
    }
}


void ModeratedEventSupport::ReleaseModeratedEvent()
{
    if(m_bTimerInitialized)
    {
        details::TimedEventCallerSingleton::instance().Release(m_nMaxEventRate);
    }
}


//
// IUPnPEventSource
//

STDMETHODIMP ModeratedEventSupport::Advise(/*[in]*/ IUPnPEventSink* punkSubscriber)
{
    ce::gate<ce::critical_section> gate(m_csPunkSubscriber);
    
    if(!punkSubscriber)
    {
        return E_POINTER;
    }

    return punkSubscriber->QueryInterface(IID_IUPnPEventSink, reinterpret_cast<void**>(&m_punkSubscriber));
}


STDMETHODIMP ModeratedEventSupport::Unadvise(/*[in]*/ IUPnPEventSink* punkSubscriber)
{
    ce::gate<ce::critical_section> gate(m_csPunkSubscriber);
    
    if(m_punkSubscriber)
    {
        m_punkSubscriber.Release();
    }

    return S_OK;
}
