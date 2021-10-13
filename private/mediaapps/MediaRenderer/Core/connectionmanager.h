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
// ConnectionManager.h : Declaration of CConnectionManager

#pragma once

//
// CM_NO_PREPARE_FOR_CONNECTION macro is used within this sample to show how to implement a MediaRenderer
// without optional PrepareForConnection and ConnectionComplete actions. 
// When CM_NO_PREPARE_FOR_CONNECTION is defined then PrepareForConnection and ConnectionComplete 
// actions must be removed for ConnectionManager service description (MediaRendererConnectionManagerService.xml)
//
#define CM_NO_PREPARE_FOR_CONNECTION


/////////////////////////////////////////////////////////////////////////////
// CConnectionManager
class CConnectionManager :     public av::IConnectionManagerImpl
{
public:
    DWORD SetConnection(ce::smart_ptr<IConnection> pConnection);

    // IConnectionManagerImpl
protected:
    virtual DWORD CreateConnection(
        /* [in] */ LPCWSTR pszRemoteProtocolInfo,
        /* [in] */ av::DIRECTION Direction,
        /* [in, out] */ long ConnectionID,
        /* [in, out] */ av::IAVTransport** ppAVTransport,
        /* [in, out] */ av::IRenderingControl** ppRenderingControl);

    virtual DWORD EndConnection(
        /* [in] */ long ConnectionID);

    virtual DWORD GetCurrentConnectionInfo(
        /* [in] */ long ConnectionID,
        /* [in, out] */ av::ConnectionInfo* pConnectionInfo);

#ifdef CM_NO_PREPARE_FOR_CONNECTION
    virtual DWORD GetFirstConnectionID(
        /* [in, out] */ long* pConnectionID);

    virtual DWORD GetNextConnectionID(
        /* [in, out] */ long* pConnectionID);
#endif

    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult);

protected:
    ce::smart_ptr<IConnection>  m_pConnection;
    typedef ce::hash_map<long, ce::smart_ptr<IConnection> >        ConnMap; // map from ConnectionID -> CConnection*
    ConnMap m_mapConn;
};

