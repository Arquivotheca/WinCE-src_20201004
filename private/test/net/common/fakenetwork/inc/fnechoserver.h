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

class FnEchoServer
{
public:
    enum ServerType 
    {
        ServerType_TCP,
        ServerType_UDP
    };

private:
    //member variables
    ce::list<ce::smart_ptr<FnFakeIpAddress>> m_lstServerAddresses;
    USHORT m_usPort;
    ServerType m_ServerType;
    DWORD m_dwMaxDatagramSize;

public:
    //constructors, getsockoption is not supported on CE to get max datagram size,
    //if it is zero by default , UDP echo server recv and send fail.
    FnEchoServer( ) : m_ServerType( ServerType_TCP ), m_dwMaxDatagramSize( 1024 ) {};
    
    // import/export methods   
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );

    void PrintDetails( int iDepth );
 
    //propety accessors
    void SetPort( USHORT usPort ) { m_usPort = usPort; }
    USHORT GetPort() { return m_usPort; }

    void SetServerType( ServerType type ) { m_ServerType = type; }
    ServerType GetServerType() { return m_ServerType; }

    void SetMaxDatagramSize( DWORD dwSize )  { m_dwMaxDatagramSize = dwSize; }
    DWORD GetMaxDatagramSize( ) { return m_dwMaxDatagramSize; }

    BOOL AddServerAddress( ce::smart_ptr<FnFakeIpAddress> fnAddress );

    

    //iterators
    //ip addresses
    typedef ce::list<ce::smart_ptr<FnFakeIpAddress>>::iterator AddressIterator;
    AddressIterator beginAddresses()
        {return m_lstServerAddresses.begin(); }
    AddressIterator endAddresses()
        {return m_lstServerAddresses.end(); }

};