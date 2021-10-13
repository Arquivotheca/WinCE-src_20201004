//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include <string.hxx>
#include <litexml.h>
#include <cmnet.h>

class FnCmConnection
{
public:
    enum FnCmConnectionType
    {
        // Normal connections show up to CM as a standard connection.
        NormalConnection,
        // Infrastructure connections are treated specially by the FakeCSP. If you use 
        //  FnNetworkConfig::SetInfrastructureRequirement on a CM session the connections 
        //  will not show up when you iterate connections in CM. They will
        //  also be automatically connected when the FakeNetwork is configured.
        InfrastructureConnection
    };
private:
    //member fields 
    ce::wstring m_strConnectionName;
    ce::wstring m_strAdapterName;
    FnCmConnectionType m_ConnType;
    BOOL m_bConnected;
    DWORD m_dwConnectionOrder;
    CM_CONNECTION_CONNECT_BEHAVIOR m_cmConnectBehavior;

public:
    //by default create normal connections.
    FnCmConnection( const ce::wstring & strConnectionName, FnCmConnectionType ConnType = NormalConnection, DWORD dwConnectionOrder = 0, CM_CONNECTION_CONNECT_BEHAVIOR dwConnectBehavior = CMCB_ON_DEMAND ) : m_strConnectionName ( strConnectionName ), m_ConnType ( ConnType ), m_dwConnectionOrder(dwConnectionOrder), m_bConnected( FALSE ), m_strAdapterName(L""), m_cmConnectBehavior( dwConnectBehavior ){};
    FnCmConnection( ) { FnCmConnection(L""); }

    //used to send the configuration information to the FakeNetwork service
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );

    void PrintDetails( int iDepth );
    
public:
    //property accessors
    ce::wstring GetName() { return m_strConnectionName; }
    FnCmConnectionType GetFnConnectionType() { return m_ConnType; }
    BOOL GetConnectedState() { return m_bConnected; }
    void SetConnectedState( BOOL bConnected ) { m_bConnected = bConnected; }

    void SetAdapterName( const ce::wstring strAdapterName ) { m_strAdapterName = strAdapterName; }
    ce::wstring GetAdapterName() { return m_strAdapterName; }

    void SetConnectionOrder( DWORD dwConnectionOrder ) { m_dwConnectionOrder = dwConnectionOrder; }
    DWORD GetConnectionOrder( ) { return m_dwConnectionOrder; }

    void SetConnectBehavior( CM_CONNECTION_CONNECT_BEHAVIOR dwConnectBehavior ) { m_cmConnectBehavior = dwConnectBehavior; }
    CM_CONNECTION_CONNECT_BEHAVIOR GetConnectBehavior( ) { return m_cmConnectBehavior; }

};

