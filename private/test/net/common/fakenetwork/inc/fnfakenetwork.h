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

#include "FnFakeAdapter.h"
#include "FnFakeDnsServer.h"
#include "FnEchoServer.h"
#include "FnActionServer.h"
#include "FnFakeDHCPServer.h"

class FnFakeNetwork
{
public:
    class FnAdapterBlockPair
    {
    public:
        FnAdapterBlockPair( const ce::wstring & one, const ce::wstring & two ) : strAdapter1( one ), strAdapter2 ( two ) {};
        ce::wstring strAdapter1;
        ce::wstring strAdapter2;
    };
private:
    //member variables
    ce::smart_ptr<FnFakeDnsServer> m_fnDnsServer;
    ce::smart_ptr<FnFakeDHCPServer> m_fnDHCPServer;
    ce::list<ce::smart_ptr<FnEchoServer>> m_lstSocketServers;
    ce::list<ce::smart_ptr<FnActionServer>> m_lstActionServers;
    ce::list<ce::smart_ptr<FnFakeAdapter>> m_lstAdapters;
    ce::list<ce::smart_ptr<FnAdapterBlockPair>> m_lstBlockAdapters;
    ce::wstring m_strNetworkName;
    BOOL m_bDisableRealAdapters;

    //import/export helper functions
    BOOL _ExportServerSettings( litexml::XmlElement_t & pElement );
    BOOL _ImportServerSettings( const litexml::XmlBaseElement_t & pElement );

    BOOL _ExportSocketServerSettings( litexml::XmlElement_t & pElement );
    BOOL _ImportSocketServerSettings( const litexml::XmlBaseElement_t & pElement );

    BOOL _ExportActionServerSettings( litexml::XmlElement_t & pElement );
    BOOL _ImportActionServerSettings( const litexml::XmlBaseElement_t & pElement );

    BOOL _ExportServerAdapters( litexml::XmlElement_t & pElement );
    BOOL _ImportServerAdapters( const litexml::XmlBaseElement_t & pElement );

    BOOL _ExportBlockedPairs( litexml::XmlElement_t & pElement );
    BOOL _ImportBlockedPairs( const litexml::XmlBaseElement_t & pElement );
    
public:
    //constructors
    FnFakeNetwork( ce::wstring strNetworkName = L"TestNetwork" );

     
    //methods
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );
    void PrintDetails( int iDepth );
    BOOL AddAdapter( ce::smart_ptr<FnFakeAdapter> fnAdapter );
    BOOL AddEchoServer( ce::smart_ptr<FnEchoServer> fnSocketServer );
    BOOL AddActionServer( ce::smart_ptr<FnActionServer> fnActionServer );

    //Congfigure adapters so that all traffic between the two adapters is dropped
    BOOL BlockRoute( ce::smart_ptr<FnFakeAdapter> fnAdapter1, ce::smart_ptr<FnFakeAdapter> fnAdapter2 );

    //Disable real adapters
    void SetDisableRealAdapters( BOOL bDisable ) { m_bDisableRealAdapters = bDisable; }

    BOOL GetDisableRealAdapters( ) { return m_bDisableRealAdapters; }

    //iterators
    typedef ce::list<ce::smart_ptr<FnFakeAdapter>>::iterator AdapterIterator;
    AdapterIterator beginAdapters()
        {return m_lstAdapters.begin(); }
    AdapterIterator endAdapters()
        {return m_lstAdapters.end(); }

    typedef ce::list<ce::smart_ptr<FnEchoServer>>::iterator EchoServerIterator;
    EchoServerIterator beginEchoServers()
        {return m_lstSocketServers.begin(); }
    EchoServerIterator endEchoServers()
        {return m_lstSocketServers.end(); }

    typedef ce::list<ce::smart_ptr<FnActionServer>>::iterator ActionServerIterator;
    ActionServerIterator beginActionServers()
        {return m_lstActionServers.begin(); }
    ActionServerIterator endActionServers()
        {return m_lstActionServers.end(); }

    typedef ce::list<ce::smart_ptr<FnAdapterBlockPair>>::iterator BlockedPairIterator;
    BlockedPairIterator beginBlockedPairs()
        {return m_lstBlockAdapters.begin(); }
    BlockedPairIterator endBlockedPairs()
        {return m_lstBlockAdapters.end(); }


    //properties accessors
    void SetDnsServer( ce::smart_ptr<FnFakeDnsServer> fnDnsServer ) { m_fnDnsServer = fnDnsServer; }
    ce::smart_ptr<FnFakeDnsServer> GetDnsServer( ) { return m_fnDnsServer; }
    void SetDHCPServer( ce::smart_ptr<FnFakeDHCPServer> fnDHCPServer ) { m_fnDHCPServer = fnDHCPServer; }
    ce::smart_ptr<FnFakeDHCPServer> GetDHCPServer( ) { return m_fnDHCPServer; }
    ce::wstring GetNetworkName( ) { return m_strNetworkName; }
};