//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// ConnectionManager.cpp : Implementation of CConnectionManager


#include "stdafx.h"


DWORD CConnectionManager::SetConnection(ce::smart_ptr<IConnection> pConnection)
{
    if (pConnection == NULL)
    {
        assert(pConnection);
        return av::ERROR_AV_POINTER;
    }
    m_pConnection = pConnection;
    return av::SUCCESS_AV;
}

//
// Changes if Connection Manager does not implement PrepareForConnection and ConnectionComplete
//

DWORD CConnectionManager::CreateConnection(
        /* [in] */ LPCWSTR pszRemoteProtocolInfo,
        /* [in] */ av::DIRECTION Direction,
        /* [in, out] */ long ConnectionID,
        /* [in, out] */ av::IAVTransport** ppAVTransport,
        /* [in, out] */ av::IRenderingControl** ppRenderingControl)
{
    return av::ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD CConnectionManager::EndConnection(
        /* [in] */ long ConnectionID)
{
    return av::ERROR_AV_UPNP_INVALID_ACTION;
}


// Override IConnectionManager::GetCurrentConnectionInfo to report ConnectionID 0's info
DWORD CConnectionManager::GetCurrentConnectionInfo(
        /* [in] */ long ConnectionID,
        /* [in, out] */ av::ConnectionInfo* pConnectionInfo)
{
    LPWSTR pTemp = NULL;

    if(!pConnectionInfo)
    {
        return av::ERROR_AV_POINTER;
    }

    if(0 != ConnectionID)
    {
        return av::ERROR_AV_UPNP_CM_INVALID_CONNECTION_REFERENCE;
    }

    pConnectionInfo->strRemoteProtocolInfo      = NULL;      // NULL, or the protocol information if it is known
    pConnectionInfo->strPeerConnectionManager   = NULL;
    pConnectionInfo->nPeerConnectionID          = -1;
    pConnectionInfo->Direction                  = av::INPUT; // For this sample direction is always INPUT
    pConnectionInfo->strStatus                  = L"OK";     // Really, one would inspect and report the actual status

    // If there were global instances of AVTransport and/or RenderingControl
    // (that had been registered with AVTransportServiceImpl/
    // RenderingControlServiceImpl respectively), one would give the pointers to
    // these instances here and AddRef them before returning
    pConnectionInfo->pAVTransport               = NULL;
    pConnectionInfo->pRenderingControl          = NULL;
    m_pConnection->GetProtocolInfo(&pTemp);
    if (!pConnectionInfo->strRemoteProtocolInfo.assign(pTemp))
    {
        return av::ERROR_AV_OOM;
    }
    return av::SUCCESS_AV;

}


//
// Override IConnectionManager::Get[First|Next]ConnectionID() to report the existance of ConnectionID 0

DWORD CConnectionManager::GetFirstConnectionID(
        /* [in, out] */ long* pConnectionID)
{
    return av::ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD CConnectionManager::GetNextConnectionID(
        /* [in, out] */ long* pConnectionID)
{
    return av::ERROR_AV_UPNP_INVALID_ACTION;
}



// InvokeVendorAction
DWORD CConnectionManager::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    return av::ERROR_AV_UPNP_INVALID_ACTION;
}
