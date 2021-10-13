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
using namespace av_upnp::details;


/////////////////////////////////////////////////////////////////////////////
// IUPnPEventSinkSupport

IUPnPEventSinkSupport::IUPnPEventSinkSupport()
    : m_punkSubscriber(NULL)
{
}


IUPnPEventSinkSupport::~IUPnPEventSinkSupport()
{
    assert(m_punkSubscriber == NULL);
}



/////////////////////////////////////////////////////////////////////////////
// IEventSinkSupport

// OnStateChanged
DWORD IEventSinkSupport::OnStateChanged(
                            /*[in]*/ LPCWSTR pszStateVariableName,
                            /*[in*/  LPCWSTR)
{
    if(!pszStateVariableName) // don't check for NULL pszValue, not sued; see ignore comment below
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section> csPunkSubscriberLock(m_csPunkSubscriber);
    
    if(m_punkSubscriber)
    {
        // We ignore the new value, pszValue, for ConnectionManager and ContentDirectory (upnp will retrieve the value through get_)

        ce::gate<ce::critical_section> csMapDISPIDsLock(m_csMapDISPIDs);
        
        const DISPIDMap::const_iterator it = m_mapDISPIDs.find(pszStateVariableName);
        
        if(it == m_mapDISPIDs.end())
        {
            return ERROR_AV_INVALID_STATEVAR;
        }
            
        DISPID dispid = it->second;

        HRESULT hr = m_punkSubscriber->OnStateChanged(1, &dispid);
        
        if(FAILED(hr))
        {
            return ERROR_AV_UPNP_ACTION_FAILED; // return general error, specifics not known
        }
    }

    return SUCCESS_AV;
}
    

// OnStateChanged
DWORD IEventSinkSupport::OnStateChanged(
                            /*[in]*/ LPCWSTR pszStateVariableName,
                            /*[in*/  long)
{
    // We ignore the new value, nValue, for ConnectionManager and ContentDirectory (upnp will retrieve the value through get_)
    return OnStateChanged(pszStateVariableName, (LPCWSTR)NULL);
}


// OnStateChanged
DWORD IEventSinkSupport::OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszChannel,
        /*[in]*/  long nValue)
{
    // This overload of OnStateChanged is only for RenderingControl use,
    // therefore we error (override this method to change this behavior)
    assert(false);
    
    return ERROR_AV_UPNP_ACTION_FAILED;
}
