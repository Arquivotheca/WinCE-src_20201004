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
// IContentDirectoryImpl

//
// IContentDirectory
//

DWORD IContentDirectoryImpl::GetSearchCapabilities(
        /* [in, out] */ wstring* pstrSearchCaps)
{
    if(!pstrSearchCaps)
    {
        return ERROR_AV_POINTER;
    }


    // Set search capabilities string to the emptry string, no capabilities:
    // "ContentDirectory:1 Service Template Version 1.01", p18, s2.5.18
    pstrSearchCaps->clear();


    return SUCCESS_AV;
}


DWORD IContentDirectoryImpl::GetSortCapabilities(
        /* [in, out] */ wstring* pstrSortCaps)
{
    if(!pstrSortCaps)
    {
        return ERROR_AV_POINTER;
    }


    // Set sort capabilities string to the emptry string, no capabilities:
    // "ContentDirectory:1 Service Template Version 1.01", p18, s2.5.19
    pstrSortCaps->clear();


    return SUCCESS_AV;
}


DWORD IContentDirectoryImpl::Search(
        /* [in] */ LPCWSTR pszContainerID,
        /* [in] */ LPCWSTR pszSearchCriteria,
        /* [in] */ LPCWSTR pszFilter,
        /* [in] */ unsigned long StartingIndex,
        /* [in] */ unsigned long RequestedCount,
        /* [in] */ LPCWSTR pszSortCriteria,
        /* [in, out] */ wstring* pstrResult,
        /* [in, out] */ unsigned long *pNumberReturned,
        /* [in, out] */ unsigned long *pTotalMatches,
        /* [in, out] */ unsigned long *pUpdateID)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IContentDirectoryImpl::CreateObject(
        /* [in] */ LPCWSTR pszContainerID,
        /* [in] */ LPCWSTR pszElements,
        /* [in, out] */ wstring* pstrObjectID,
        /* [in, out] */ wstring* pstrResult)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IContentDirectoryImpl::DestroyObject(
        /* [in] */ LPCWSTR pszObjectID)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IContentDirectoryImpl::UpdateObject(
        /* [in] */ LPCWSTR pszObjectID,
        /* [in] */ LPCWSTR pszCurrentTagValue,
        /* [in] */ LPCWSTR pszNewTagValue)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IContentDirectoryImpl::ImportResource(
        /* [in] */ LPCWSTR pszSourceURI,
        /* [in] */ LPCWSTR pszDestinationURI,
        /* [in, out] */ unsigned long *pTransferID)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IContentDirectoryImpl::ExportResource(
        /* [in] */ LPCWSTR pszSourceURI,
        /* [in] */ LPCWSTR pszDestinationURI,
        /* [in, out] */ unsigned long *pTransferID)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IContentDirectoryImpl::StopTransferResource(
        /* [in] */ unsigned long TransferID)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IContentDirectoryImpl::GetTransferProgress(
        /* [in] */ unsigned long TransferID,
        /* [in, out] */ wstring* pstrTransferStatus,
        /* [in, out] */ wstring* pstrTransferLength,
        /* [in, out] */ wstring* pstrTransferTotal)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IContentDirectoryImpl::DeleteResource(
        /* [in] */ LPCWSTR pszResourceURI)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IContentDirectoryImpl::CreateReference(
        /* [in] */ LPCWSTR pszContainerID,
        /* [in] */ LPCWSTR pszObjectID,
        /* [in, out] */ wstring* pstrNewID)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


// InvokeVendorAction
DWORD IContentDirectoryImpl::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}
