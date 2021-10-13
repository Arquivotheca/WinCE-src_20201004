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

//
// Implementations of inline functions and templated classes in av_upnp.h
//

#ifndef __AV_UPNP_IMPL_H
#define __AV_UPNP_IMPL_H

#include <variant.h>

namespace av_upnp {

/////////////////////////////////////////////////////////////////////////////
// VirtualServiceSupport


//
// VirtualServiceSupport
//
template <typename T>
VirtualServiceSupport<T>::VirtualServiceSupport(DISPID dispidLastChange, long nFirstInstanceID/* = 0 */)
    : ModeratedEventSupport(2),
      m_nLastCreatedID(nFirstInstanceID - 1),
      m_bGetLastChangeUpdatesOnly(false)
{
    assert(nFirstInstanceID >= 0); // the first created ID should be zero or above
    
    m_dispidLastChange[0] = dispidLastChange;
}


//
// ~VirtualServiceSupport
//
template <typename T>
VirtualServiceSupport<T>::~VirtualServiceSupport()
{
    assert(m_punkSubscriber == NULL);

    // Unregister any remaining service instances
    ce::gate<ce::critical_section> csInstancesLock(m_csMapInstances);

    while(m_mapInstances.size() > 0)
        UnregisterInstance(m_mapInstances.begin()->first);
}


//
// RegisterInstance
// 
template <typename T>
DWORD VirtualServiceSupport<T>::RegisterInstance(
                            /* [in] */  T* pIServiceInstance,
                            /* [in, out] */ long* pInstanceID)
{
    DWORD dw;

    assert(pIServiceInstance);
    
    ce::gate<ce::critical_section> csInstancesLock(m_csMapInstances);

    if(SUCCESS_AV != (dw = CreateUniqueID(pInstanceID)))
        return dw;
        
    return RegisterInstance(pIServiceInstance, *pInstanceID);
}    
    

//
// RegisterInstance
//
template <typename T>
DWORD VirtualServiceSupport<T>::RegisterInstance(
                            /* [in] */  T* pIServiceInstance,
                            /* [in] */  long InstanceID)
{
    DWORD dw;
    
    if(!pIServiceInstance)
        return ERROR_AV_POINTER;

    if(InstanceID < 0)
        return ERROR_AV_INVALID_INSTANCE;

    ce::gate<ce::critical_section> gate(m_csMapInstances);

    if(m_mapInstances.end() != m_mapInstances.find(InstanceID))
        return ERROR_AV_INVALID_INSTANCE;
        
    InstanceMap::iterator it = m_mapInstances.insert(InstanceID, pIServiceInstance);
    
    if(m_mapInstances.end() == it)
        return ERROR_AV_OOM;

    if(SUCCESS_AV != (dw = pIServiceInstance->Advise(&it->second.virtualEventSink)))
    {
        m_mapInstances.erase(it);
        return dw;
    }

    it->second.pService->AddRef();
    AddRefModeratedEvent();

    m_nLastCreatedID = InstanceID;
    
    assert(m_nLastCreatedID >= 0);

    return SUCCESS_AV;
}


//
// UnregisterInstance
//
template <typename T>
DWORD VirtualServiceSupport<T>::UnregisterInstance(
                            /* [in] */  long  InstanceID)
{
    ReleaseModeratedEvent();
    
    ce::gate<ce::critical_section> gate(m_csMapInstances);

    const InstanceMap::iterator it = m_mapInstances.find(InstanceID);
    
    if(m_mapInstances.end() == it)
        return (DWORD)ERROR_AV_INVALID_INSTANCE;

    it->second.pService->Unadvise(&it->second.virtualEventSink);

    it->second.pService->Release();

    m_mapInstances.erase(it);

    return SUCCESS_AV;
}


//
// DefaultInstanceExists
//
template <typename T>
bool VirtualServiceSupport<T>::DefaultInstanceExists()
{
    const long DefaultInstanceID = 0;
    ce::gate<ce::critical_section> gate(m_csMapInstances);
    return(m_mapInstances.end() != m_mapInstances.find(DefaultInstanceID));
}            


//
// GetLastChange
//
template <typename T>
DWORD VirtualServiceSupport<T>::GetLastChange(wstring* pLastChange)
{
    assert(pLastChange);
    assert(pLastChange->empty());

    // Start the root <Event> element with appropriate namespace for the service
    if(!pLastChange->append(L"<Event xmlns=\"") ||
       !pLastChange->append(details::get_event_namespace<T>()) ||
       !pLastChange->append(L"\">"))
    {
        DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OOM when creating last change event")));
        return ERROR_AV_OOM;
    }
    
    m_csMapInstances.assert_locked();

    // Loop through each service instance, writing LastChange data for each that has changed, or all if the caller wanted all state variables
    for(InstanceMap::iterator it = m_mapInstances.begin(), itEnd = m_mapInstances.end(); itEnd != it; ++it)
    {
        if(!m_bGetLastChangeUpdatesOnly || it->second.virtualEventSink.HasChanged())
        {
            wchar_t pszValue[33];
            
            _itow(it->first, pszValue, 10);

            // Start <InstanceID> element for this instance
            if(!pLastChange->append(L"<InstanceID val=\"") ||
               !pLastChange->append(pszValue) ||
               !pLastChange->append(L"\">"))
            {
                return ERROR_AV_OOM;
            }
            
            // Have the service instance add its state variables 
            if(SUCCESS_AV != it->second.virtualEventSink.GetLastChange(m_bGetLastChangeUpdatesOnly, pLastChange))
                return ERROR_AV_UPNP_ACTION_FAILED;
            
            // End <InstanceID> element
            if(!pLastChange->append(L"</InstanceID>"))
                return ERROR_AV_OOM;
        }
    }
    
    // End <Event> element
    if(!pLastChange->append(L"</Event>"))
        return ERROR_AV_OOM;

    return SUCCESS_AV;
}


//
// CreateUniqueID
// Caller must hold m_csMapInstances
//
template <typename T>
DWORD VirtualServiceSupport<T>::CreateUniqueID(long* pInstanceID)
{
    if(!pInstanceID)
        return ERROR_AV_POINTER;

    assert(m_nLastCreatedID + 1 >= 0);
    
    // used to detect if we've stepped through all instance ids
    const long firstID = m_nLastCreatedID; 
    
    for(long n = firstID + 1; n != firstID; ++n)
    {
        if(n < 0)
        {
            // don't allow for negative IDs
            n = 0;
            continue;
        }
            
        if(m_mapInstances.end() == m_mapInstances.find(n))
        {
            *pInstanceID = n;
            return SUCCESS_AV;
        }
    }
    
    return ERROR_AV_UPNP_ACTION_FAILED;
}


//
// TimedEventCall
//
template <typename T>
void VirtualServiceSupport<T>::TimedEventCall()
{
    ce::gate<ce::critical_section> gateSubscriberLock(m_csPunkSubscriber);
    
    if(!m_punkSubscriber)
        return;

    ce::gate<ce::critical_section> gateInstancesLock(m_csMapInstances);

    // Generate LastChange event if any virtual service instance has unreported changes
    for(InstanceMap::iterator it = m_mapInstances.begin(), itEnd = m_mapInstances.end(); itEnd != it; ++it)
    {
        if(it->second.virtualEventSink.HasChanged())
        {
            //
            // Generate event for LastChange state variable. The even will contain only variables
            // that have changed since last event (m_bGetLastChangeUpdatesOnly = true).
            // Holding m_csGetLastChange critical section prevents other thread from calling
            // GetLastChange until we complete this event.
            //
            ce::gate<ce::critical_section> gate(m_csGetLastChange);
            
            m_bGetLastChangeUpdatesOnly = true;

            //
            // OnStateChanged will call back into the service's get_LastChange which in turn calls
            // GetLastChange to get value of the LastChange state variable. 
            //
            m_punkSubscriber->OnStateChanged(1, m_dispidLastChange);

            m_bGetLastChangeUpdatesOnly = false;
            
            break;
        }
    }
}



//
// GetLastChange
//
template <typename T>
HRESULT VirtualServiceSupport<T>::GetLastChange(BSTR *pbstrLastChange)
{
    // m_csMapInstances critical section is needed by GetLastChange(wstring*) 
    // and must be entered before entering m_csGetLastChange to avoid deadlock
    ce::gate<ce::critical_section> csInstancesLock(m_csMapInstances); 
    
    ce::gate<ce::critical_section> gateGetLastChange(m_csGetLastChange);

    // static buffer used to build LastChange value (for efficiency)
    static details::wstring_buffer LastChangeBuffer;
    
    if(!pbstrLastChange)
        return m_ErrReport.ReportError(ERROR_AV_POINTER);

    // reset the global buffer so that we can use append to build LastChange value
    LastChangeBuffer.ResetBuffer();

    // Build LastChange value
    const DWORD dw = GetLastChange(&LastChangeBuffer.strBuffer);
    
    if(SUCCESS_AV != dw)
        return m_ErrReport.ReportError(dw);

    // Allocate BSTR string for LastChange
    if(!(*pbstrLastChange = SysAllocString(LastChangeBuffer.strBuffer)))
        return m_ErrReport.ReportError(ERROR_AV_OOM);

    return S_OK;
}


//
// InvokeVendorAction
//
template <typename T>
DWORD VirtualServiceSupport<T>::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    unsigned    uArgErr;
    ce::variant varInstanceID;
    
    // get first argument (InstanceID)
    if(FAILED(DispGetParam(pdispparams, 0, VT_UI4, &varInstanceID, &uArgErr)))
        return (DWORD)ERROR_AV_UPNP_ACTION_FAILED;
    
    ce::gate<ce::critical_section> csMapInstances(m_csMapInstances);

    // find service instance based on ID
    const InstanceMap::iterator it = m_mapInstances.find(varInstanceID.ulVal);
    
    if(it == m_mapInstances.end())
        return (DWORD)ERROR_AV_INVALID_INSTANCE;
    
    return it->second.pService->InvokeVendorAction(pszActionName, pdispparams, pvarResult);
}        

}

#endif // __AV_UPNP_IMPL_H
