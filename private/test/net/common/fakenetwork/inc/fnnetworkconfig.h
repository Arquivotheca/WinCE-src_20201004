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
#include "FnFakeNetwork.h"
#include "CmNet.h"
#include <psl_marshaler.hxx>


//class used to make modifications or get information from FnService - the FakeNetwork service
class FnNetworkConfig
{
private:
    ce::smart_ptr<ce::psl_proxy<>> m_pProxy;
    ce::smart_ptr<FnCmConnection> _GetConnection( WORD dwService, const ce::wstring & strName );

public:
    FnNetworkConfig();
    //Call FnService to create a fakenetwork
    BOOL CreateNetwork( ce::smart_ptr<FnFakeNetwork> fnNetwork );
    ce::smart_ptr<FnFakeNetwork> GetNetwork();
    
    //Call down in to FnService to connect/disconnect an adapter that was 
    //  already configured using CreateNetwork
    BOOL ConnectAdapter( const ce::wstring & strAdapterName );
    BOOL DisconnectAdapter( const ce::wstring & strAdapterName );
    BOOL IsAdapterConnected( const ce::wstring & strAdapterName );

    //Call down in to FnService to connect/disconnect an adapter based on the
    //  connection name that was already configured using CreateNetwork
    BOOL ConnectConnection( const ce::wstring & strConnectionName );
    BOOL DisconnectConnection( const ce::wstring & strConnectionName );

    //Set a CM Requirement so that Infrastructure connections are ignored when
    //  connection manager iterates over connections using the passed in session
    BOOL SetInfrastructureRequirement( CM_SESSION_HANDLE cmSession );

    //Set a CM prefrence so that connections are returned in the expected order when
    //  connection manager iterates over connections using the passed in session
    BOOL OrderConnections( CM_SESSION_HANDLE cmSession );

    //Get the connection information for connection that is already configured 
    //  either by adapter name or connection name. Most applications that
    //  configured the network won't need to call this as they already have the 
    //  connection configuration. This is primarly for use by the FakeCsp.
    ce::smart_ptr<FnCmConnection> GetConnectionByConnectionName( const ce::wstring & strConnectionName );
    ce::smart_ptr<FnCmConnection> GetConnectionByAdapterName( const ce::wstring & strAdapterName );


    // unload Network
    BOOL UnloadNetwork();
    //DEBUG USE ONLY - DO NOT USE
    BOOL ServiceInitialize( );
    BOOL ServiceDeinitialize( );
};

