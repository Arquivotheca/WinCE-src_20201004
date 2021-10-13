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

#include "av_upnp.h"
#include "av_upnp_ctrl_internal.h"

using namespace av_upnp;
using namespace av_upnp::details;

/////////////////////////////////////////////////////////////////////////////
// ContentDirectoryCtrl

//
// Init
//
bool ContentDirectoryCtrl::Init(/* [in] */ IUPnPDevice* pDevice)
{
    HRESULT hr;
    
    CComPtr<IUPnPServices>  pServices;
    CComPtr<IUPnPService>   pContentDirectory;
    
    if(!pDevice)
    {
        DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("pDevice is NULL")));
        return false;
    }
    
    // Get collection of UPnP services
    if(FAILED(hr = pDevice->get_Services(&pServices)))
    {
        DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("get_Serivces failed 0x%08x"), hr));
        return false;
    }
    
    // Get ContentDirectory service
    if(!GetService(pServices, L"urn:upnp-org:serviceId:ContentDirectory", L"urn:schemas-upnp-org:service:ContentDirectory:1", &pContentDirectory))
        return false;
    
    if(!ServiceCtrl<IContentDirectory>::Init(pContentDirectory, pDevice))
        return false;

    m_proxyContentDirectory.init(pContentDirectory);
    
    return true;
}


///////////////////////
// IContentDirectory
///////////////////////


//
// GetSearchCapabilities
//
DWORD ContentDirectoryCtrl::GetSearchCapabilities(
		    /* [in, out] */ wstring* pstrSearchCaps)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"GetSearchCapabilities");
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pstrSearchCaps))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// GetSortCapabilities
//
DWORD ContentDirectoryCtrl::GetSortCapabilities(
		    /* [in, out] */ wstring* pstrSortCaps)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"GetSortCapabilities");
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pstrSortCaps))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// GetSystemUpdateID
//
DWORD ContentDirectoryCtrl::GetSystemUpdateID(
		    /* [in, out] */ unsigned long* pId)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"GetSystemUpdateID");
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pId))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// BrowseMetadata
//
DWORD ContentDirectoryCtrl::BrowseMetadata(
		    /* [in] */ LPCWSTR pszObjectID,
		    /* [in] */ LPCWSTR pszFilter,
		    /* [in, out] */ wstring* pstrResult,
		    /* [in, out] */ unsigned long* pUpdateID)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"Browse", 
                                      pszObjectID,          // ObjectID
                                      L"BrowseMetadata",    // BrowseFlag
                                      pszFilter,            // Filter
                                      (unsigned long)0,     // StartingIndex
                                      (unsigned long)0,     // RequestedCount
                                      L"");                 // SortCriteria (unused)
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pstrResult,     // Result
                                            NULL, NULL,     // NumberReturned, TotalMatches (unused)
                                            pUpdateID))     // UpdateID
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// BrowseChildren
//
DWORD ContentDirectoryCtrl::BrowseChildren(
		    /* [in] */ LPCWSTR pszObjectID,
		    /* [in] */ LPCWSTR pszFilter,
		    /* [in] */ unsigned long StartingIndex,
		    /* [in] */ unsigned long RequestedCount,
		    /* [in] */ LPCWSTR pszSortCriteria,
		    /* [in, out] */ wstring* pstrResult,
		    /* [in, out] */ unsigned long* pNumberReturned,
		    /* [in, out] */ unsigned long* pTotalMatches,
		    /* [in, out] */ unsigned long* pUpdateID)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"Browse", 
                                      pszObjectID,              // ObjectID
                                      L"BrowseDirectChildren",  // BrowseFlag
                                      pszFilter,                // Filter
                                      StartingIndex,            // StartingIndex
                                      RequestedCount,           // RequestedCount
                                      pszSortCriteria);         // SortCriteria
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pstrResult,         // Result
                                            pNumberReturned,    // NumberReturned
                                            pTotalMatches,      // TotalMatches
                                            pUpdateID))         // UpdateID
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// Search
//
DWORD ContentDirectoryCtrl::Search(
		    /* [in] */ LPCWSTR pszContainerID,
		    /* [in] */ LPCWSTR pszSearchCriteria,
		    /* [in] */ LPCWSTR pszFilter,
		    /* [in] */ unsigned long StartingIndex,
		    /* [in] */ unsigned long RequestedCount,
		    /* [in] */ LPCWSTR pszSortCriteria,
		    /* [in, out] */ wstring* pstrResult,
		    /* [in, out] */ unsigned long* pNumberReturned,
		    /* [in, out] */ unsigned long* pTotalMatches,
		    /* [in, out] */ unsigned long* pUpdateID)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"Search", 
                                      pszContainerID,
                                      pszSearchCriteria,
                                      pszFilter,
                                      StartingIndex,
                                      RequestedCount,
                                      pszSortCriteria);
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pstrResult,         // Result
                                            pNumberReturned,    // NumberReturned
                                            pTotalMatches,      // TotalMatches
                                            pUpdateID))         // UpdateID
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// CreateObject
//
DWORD ContentDirectoryCtrl::CreateObject(
		    /* [in] */ LPCWSTR pszContainerID,
		    /* [in] */ LPCWSTR pszElements,
		    /* [in, out] */ wstring* pstrObjectID,
		    /* [in, out] */ wstring* pstrResult)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"CreateObject",
                                      pszContainerID,
                                      pszElements);
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pstrObjectID,
                                            pstrResult))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// DestroyObject
//
DWORD ContentDirectoryCtrl::DestroyObject(
		    /* [in] */ LPCWSTR pszObjectID)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"DestroyObject",
                                      pszObjectID);
           
    return AVErrorFromUPnPError(hr);
}


//
// UpdateObject
//
DWORD ContentDirectoryCtrl::UpdateObject(
		    /* [in] */ LPCWSTR pszObjectID,
		    /* [in] */ LPCWSTR pszCurrentTagValue,
		    /* [in] */ LPCWSTR pszNewTagValue)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"UpdateObject",
                                      pszObjectID,
                                      pszCurrentTagValue,
                                      pszNewTagValue);
           
    return AVErrorFromUPnPError(hr);
}


//
// ImportResource
//
DWORD ContentDirectoryCtrl::ImportResource(
		    /* [in] */ LPCWSTR pszSourceURI,
		    /* [in] */ LPCWSTR pszDestinationURI,
		    /* [in, out] */ unsigned long* pTransferID)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"ImportResource",
                                      pszSourceURI,
                                      pszDestinationURI);
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pTransferID))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// ExportResource
//
DWORD ContentDirectoryCtrl::ExportResource(
		    /* [in] */ LPCWSTR pszSourceURI,
		    /* [in] */ LPCWSTR pszDestinationURI,
		    /* [in, out] */ unsigned long* pTransferID)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"ExportResource",
                                      pszSourceURI,
                                      pszDestinationURI);
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pTransferID))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// StopTransferResource
//
DWORD ContentDirectoryCtrl::StopTransferResource(
		    /* [in] */ unsigned long TransferID)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"StopTransferResource",
                                      TransferID);
           
    return AVErrorFromUPnPError(hr);
}


//
// GetTransferProgress
//
DWORD ContentDirectoryCtrl::GetTransferProgress(
		    /* [in] */ unsigned long TransferID,
		    /* [in, out] */ wstring* pstrTransferStatus,
		    /* [in, out] */ wstring* pstrTransferLength,
		    /* [in, out] */ wstring* pstrTransferTotal)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"GetTransferProgress",
                                      TransferID);
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pstrTransferStatus,
                                            pstrTransferLength,
                                            pstrTransferTotal))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// DeleteResource
//
DWORD ContentDirectoryCtrl::DeleteResource(
		    /* [in] */ LPCWSTR pszResourceURI)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"DeleteResource",
                                      pszResourceURI);
           
    return AVErrorFromUPnPError(hr);
}


//
// CreateReference
//
DWORD ContentDirectoryCtrl::CreateReference(
		    /* [in] */ LPCWSTR pszContainerID,
		    /* [in] */ LPCWSTR pszObjectID,
		    /* [in, out] */ wstring* pstrNewID)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.call(L"CreateReference",
                                      pszContainerID,
                                      pszObjectID);
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyContentDirectory.get_results(pstrNewID))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    return SUCCESS_AV;
}


//
// InvokeVendorAction
//
DWORD ContentDirectoryCtrl::InvokeVendorAction(
            /* [in] */ LPCWSTR pszActionName,
            /* [in] */ DISPPARAMS* pdispparams, 
            /* [in, out] */ VARIANT* pvarResult)
{
    HRESULT hr;
    
    hr = m_proxyContentDirectory.invoke(pszActionName, pdispparams, pvarResult);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    return SUCCESS_AV;
}
