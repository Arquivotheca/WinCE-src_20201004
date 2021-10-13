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
