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
#include "av_upnp_ctrl_internal.h"

using namespace av_upnp;
using namespace av_upnp::details;

/////////////////////////////////////////////////////////////////////////////
// ConnectionManagerCtrl


//
// Init
//
bool ConnectionManagerCtrl::Init(/* [in] */ IUPnPDevice* pDevice)
{
    HRESULT hr;
    
    CComPtr<IUPnPServices>  pServices;
    CComPtr<IUPnPService>   pConnectionManager, pAVTransport, pRenderingControl;
    
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
    
    // Get ConnectionManager service
    if(!GetService(pServices, L"urn:upnp-org:serviceId:ConnectionManager", L"urn:schemas-upnp-org:service:ConnectionManager:1", &pConnectionManager))
    {
        return false;
    }
    
    ce::auto_bstr bstrServiceID;
    
    pConnectionManager->get_Id(&bstrServiceID);
    
    m_strServiceID = bstrServiceID;
    
    if(!ServiceCtrl<IConnectionManager>::Init(pConnectionManager, pDevice))
    {
        return false;
    }
        
    m_proxyConnectionManager.init(pConnectionManager);
    
    // Get [optional] AVTransport service
    if(GetService(pServices, L"urn:upnp-org:serviceId:AVTransport", L"urn:schemas-upnp-org:service:AVTransport:1", &pAVTransport))
    {
        m_AVTransport.Init(pAVTransport);
    }
#ifdef DEBUG
    else
    {
        ce::auto_bstr bstrName;
        
        pDevice->get_FriendlyName(&bstrName);
        
        DEBUGMSG(ZONE_AV_TRACE, (AV_TEXT("Device %s doesn't implement optional service AVTransport"), static_cast<BSTR>(bstrName)));
    }
#endif
    
    // Get [optional] RenderingControl service
    if(GetService(pServices, L"urn:upnp-org:serviceId:RenderingControl", L"urn:schemas-upnp-org:service:RenderingControl:1", &pRenderingControl))
    {
        m_RenderingControl.Init(pRenderingControl);
    }
#ifdef DEBUG
    else
    {
        ce::auto_bstr bstrName;
        
        pDevice->get_FriendlyName(&bstrName);
        
        DEBUGMSG(ZONE_AV_TRACE, (AV_TEXT("Device %s doesn't implement optional service RenderingControl"), static_cast<BSTR>(bstrName)));
    }
#endif
    
    return true;
}


//
// GetProtocolInfo
//
DWORD ConnectionManagerCtrl::GetProtocolInfo(
            /* [in, out] */ wstring* pstrSourceProtocolInfo,
            /* [in, out] */ wstring* pstrSinkProtocolInfo)
{
    HRESULT hr;
    ce::upnp_proxy_context context;
    
    hr = m_proxyConnectionManager.call(context, L"GetProtocolInfo");
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyConnectionManager.get_results(context,
                                             pstrSourceProtocolInfo, 
                                             pstrSinkProtocolInfo))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
        
    return SUCCESS_AV;
}


//
// PrepareForConnection
//
DWORD ConnectionManagerCtrl::PrepareForConnection(
            /* [in] */ LPCWSTR pszRemoteProtocolInfo,
            /* [in] */ LPCWSTR pszPeerConnectionManager,
            /* [in] */ long PeerConnectionID,
            /* [in] */ DIRECTION Direction,
            /* [in, out] */ long* pConnectionID,
            /* [in, out] */ IAVTransport** ppAVTransport,
            /* [in, out] */ IRenderingControl** ppRenderingControl)
{
    HRESULT hr;
    ce::upnp_proxy_context context;
    
    hr = m_proxyConnectionManager.call(context,
                                       L"PrepareForConnection",
                                       pszRemoteProtocolInfo,
                                       pszPeerConnectionManager,
                                       PeerConnectionID,
                                       Direction == INPUT ? details::Direction::Input : details::Direction::Output);
    
    if(hr == UPNP_E_INVALID_ACTION)
    {
        DEBUGMSG(ZONE_AV_WARN, (AV_TEXT("PrepareForConnection not implemented. Using IDs = 0")));
        
        *pConnectionID = 0;
        
        m_AVTransport.GetInstance(0, ppAVTransport);
        m_RenderingControl.GetInstance(0, ppRenderingControl);
    }
    else
    {
        if(FAILED(hr))
        {
            return AVErrorFromUPnPError(hr);
        }
    
        long RcsID, AVTransportID;
        
        if(!m_proxyConnectionManager.get_results(context,
                                                 pConnectionID, 
                                                &AVTransportID,
                                                &RcsID))
        {
            return ERROR_AV_INVALID_OUT_ARGUMENTS;
        }
        
        m_AVTransport.GetInstance(AVTransportID, ppAVTransport);
        m_RenderingControl.GetInstance(RcsID, ppRenderingControl);
    }
    
    return SUCCESS_AV;
}



//
// ConnectionComplete
//
DWORD ConnectionManagerCtrl::ConnectionComplete(
    /* [in] */ long ConnectionID)
{
    HRESULT hr;
    ce::upnp_proxy_context context;
    
    hr = m_proxyConnectionManager.call(context, L"ConnectionComplete", ConnectionID);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    return SUCCESS_AV;
}


//
// GetFirstConnectionID
//
DWORD ConnectionManagerCtrl::GetFirstConnectionID(
    /* [in, out] */ long* pConnectionID)
{
    HRESULT hr;
    ce::upnp_proxy_context context;
    
    hr = m_proxyConnectionManager.call(context, L"GetCurrentConnectionIDs");
           
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    if(!m_proxyConnectionManager.get_results(context, &m_strConnectionIDs))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    if(m_strConnectionIDs.empty())
    {
        return ERROR_AV_NO_MORE_ITEMS;
    }
    
    // append extra ',' to make parsing easier
    m_strConnectionIDs += L",";
    
    return GetNextConnectionID(pConnectionID);
}


//
// GetNextConnectionID
//
DWORD ConnectionManagerCtrl::GetNextConnectionID(
    /* [in, out] */ long* pConnectionID)
{
    if(!pConnectionID)
    {
        return ERROR_AV_POINTER;
    }
        
    if(m_strConnectionIDs.empty())
    {
        return ERROR_AV_NO_MORE_ITEMS;
    }
    
    *pConnectionID = _wtoi(m_strConnectionIDs);
    
    m_strConnectionIDs.erase(0, m_strConnectionIDs.find(L",") + 1);
    
    return SUCCESS_AV;
}


//
// GetCurrentConnectionInfo
//
DWORD ConnectionManagerCtrl::GetCurrentConnectionInfo(
            /* [in] */ long ConnectionID,
            /* [in, out] */ ConnectionInfo* pConnectionInfo)
{
    if(!pConnectionInfo)
    {
        return ERROR_AV_POINTER;
    }
        
    HRESULT hr;
    ce::upnp_proxy_context context;
    
    hr = m_proxyConnectionManager.call(context, L"GetCurrentConnectionInfo", ConnectionID);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }                              
    
    ce::wstring strDirection;
    long        RcsID, AVTransportID;
    
    if(!m_proxyConnectionManager.get_results(context,
                                             &RcsID,
                                             &AVTransportID,
                                             &pConnectionInfo->strRemoteProtocolInfo,
                                             &pConnectionInfo->strPeerConnectionManager,
                                             &pConnectionInfo->nPeerConnectionID,
                                             &strDirection,
                                             &pConnectionInfo->strStatus))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    // Direction
    if(Direction::Input == strDirection)
    {
        pConnectionInfo->Direction = INPUT;
    }
    else
    {
        if(Direction::Output == strDirection)
        {
            pConnectionInfo->Direction = OUTPUT;
        }
        else
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("GetCurrentConnectionInfo returned invalid Direction: %s"), static_cast<LPCWSTR>(strDirection)));
            return ERROR_AV_INVALID_OUT_ARGUMENTS;
        }
    }
    
    m_AVTransport.GetInstance(AVTransportID, &pConnectionInfo->pAVTransport);
    m_RenderingControl.GetInstance(RcsID, &pConnectionInfo->pRenderingControl);
    
    return SUCCESS_AV;
}


//
// InvokeVendorAction
//
DWORD ConnectionManagerCtrl::InvokeVendorAction(
            /* [in] */ LPCWSTR pszActionName,
            /* [in] */ DISPPARAMS* pdispparams, 
            /* [in, out] */ VARIANT* pvarResult)
{
    HRESULT hr;
    
    hr = m_proxyConnectionManager.invoke(pszActionName, pdispparams, pvarResult);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    return SUCCESS_AV;
}
