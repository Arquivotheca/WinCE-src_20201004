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
#include "FnActionList.h"

class FnActionServer
{
private:
    //member variables
    ce::list<ce::smart_ptr<FnFakeIpAddress>> m_lstServerAddresses;
    ce::list<ce::smart_ptr<FnActionList>> m_lstActionLists;
    USHORT m_usPort;

    BOOL _ExportActionListsXml( litexml::XmlElement_t & pElement );
    
public:
    //constructors
    FnActionServer( ) {};
    
    // import/export methods   
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );

    void PrintDetails( int iDepth );
 
    //propety accessors
    void SetPort( USHORT usPort ) { m_usPort = usPort; }
    USHORT GetPort() { return m_usPort; }

    BOOL AddServerAddress( ce::smart_ptr<FnFakeIpAddress> fnAddress );
    BOOL AddActionList( ce::smart_ptr<FnActionList> fnActionList );

    

    //iterators
    //ip addresses
    typedef ce::list<ce::smart_ptr<FnFakeIpAddress>>::iterator AddressIterator;
    AddressIterator beginAddresses()
        {return m_lstServerAddresses.begin(); }
    AddressIterator endAddresses()
        {return m_lstServerAddresses.end(); }

    //action list
    typedef ce::list<ce::smart_ptr<FnActionList>>::iterator ActionListIterator;
    ActionListIterator beginActionsLists()
        {return m_lstActionLists.begin(); }
    ActionListIterator endActionsLists()
        {return m_lstActionLists.end(); }

};