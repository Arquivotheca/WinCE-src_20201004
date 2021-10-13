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

#include <list.hxx>
#include <auto_xxx.hxx>
#include <string.hxx>
#include <litexml.h>

#include "FnFakeIpAddress.h"
#include "FnCmConnection.h"

class FnFakeAdapter
{
private:
    //member variables
    ce::list<ce::smart_ptr<FnFakeIpAddress>> m_lstAddresses;
    ce::list<ce::smart_ptr<FnFakeIpAddress>> m_lstGateways;
    ce::list<ce::smart_ptr<FnFakeIpAddress>> m_lstDnsServers;
    ce::list<ce::smart_ptr<FnCmConnection>> m_lstConnections;
    ce::wstring m_strAdapterName;

    //helper functions
    BOOL _ImportConnections( const litexml::XmlBaseElement_t & pElement );
    BOOL _ExportConnections( litexml::XmlElement_t & pElement );

public:
    //constructors
    FnFakeAdapter( ce::wstring strAdapterName) : m_strAdapterName( strAdapterName ) {};


    //methods
    BOOL AddAddress( ce::smart_ptr<FnFakeIpAddress> fnAddress );
    BOOL AddGateway( ce::smart_ptr<FnFakeIpAddress> fnAddress );
    BOOL AddDnsServer( ce::smart_ptr<FnFakeIpAddress> fnAddress );
    
    BOOL AddConnection( ce::smart_ptr<FnCmConnection> fnConnection );
    ce::smart_ptr<FnCmConnection> AddConnection( const ce::wstring & strConnectionName, FnCmConnection::FnCmConnectionType ConnType = FnCmConnection::NormalConnection, DWORD dwConnectionOrder = 0, CM_CONNECTION_CONNECT_BEHAVIOR dwConnectBehavior = CMCB_ON_DEMAND );

    //import/export methods
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );


    void PrintDetails( int iDepth );

    //property accessors
    ce::wstring GetName( ) { return m_strAdapterName; }


    
    //iterators
    typedef ce::list<ce::smart_ptr<FnFakeIpAddress>>::iterator AddressIterator;
    //ip addresses
    AddressIterator beginAddresses()
        {return m_lstAddresses.begin(); }
    AddressIterator endAddresses()
        {return m_lstAddresses.end(); }
    //gateways
    AddressIterator beginGateways()
        {return m_lstGateways.begin(); }
    AddressIterator endGateways()
        {return m_lstGateways.end(); }
    //dns servers
    AddressIterator beginDnsServers()
        {return m_lstDnsServers.begin(); }
    AddressIterator endDnsServers()
        {return m_lstDnsServers.end(); }

    //Connections
    typedef ce::list<ce::smart_ptr<FnCmConnection>>::iterator ConnectionIterator;
    ConnectionIterator beginConnections()
        {return m_lstConnections.begin(); }
    ConnectionIterator endConnections()
        {return m_lstConnections.end(); }




};

