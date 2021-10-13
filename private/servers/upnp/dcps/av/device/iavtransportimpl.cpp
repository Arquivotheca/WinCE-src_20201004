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
// IAVTransportImpl

//
// IAVTransport
//

DWORD IAVTransportImpl::SetNextAVTransportURI(
        /*[in]*/ LPCWSTR pszNextURI,
        /*[in]*/ LPCWSTR pszNextURIMetaData)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IAVTransportImpl::Pause()
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IAVTransportImpl::Record()
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IAVTransportImpl::SetPlayMode(/*[in]*/ LPCWSTR pszNewPlayMode)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IAVTransportImpl::SetRecordQualityMode(
        /*[in]*/ LPCWSTR pszNewRecordQualityMode)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IAVTransportImpl::GetCurrentTransportActions(
        /*[in, out]*/ TransportActions* pActions)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}



DWORD IAVTransportImpl::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    // If toolkit user has not overloaded this function, return invalid
    return ERROR_AV_UPNP_INVALID_ACTION;
}
