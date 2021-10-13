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

#include "FnFakeIPAddress.h"
#include "FnDnsEntry.h"
#include "FnPTREntry.h"

class FnFakeDnsServer
{
private:
    //internal member variables
    ce::list<ce::smart_ptr<FnFakeIpAddress>> m_lstServerAddresses;
    ce::list<ce::smart_ptr<FnDnsEntry>> m_lstDnsEntries;
    ce::list<ce::smart_ptr<FnPTREntry>> m_lstPTREntries;

    //helper methods
    BOOL _ExportDnsEntries( litexml::XmlElement_t & pElement );
    BOOL _ImportDnsEntries( const litexml::XmlBaseElement_t & pElement );
    BOOL _ExportPTREntries( litexml::XmlElement_t & pElement );
    BOOL _ImportPTREntries( const litexml::XmlBaseElement_t & pElement );
public:

    //import/export methods
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );

    void PrintDetails( int iDepth );

    BOOL AddServerAddress( ce::smart_ptr<FnFakeIpAddress> fnAddress );
    BOOL AddDNSEntry( ce::smart_ptr<FnFakeIpAddress> fnClientAddr, ce::smart_ptr<FnFakeIpAddress> fnServerAddr, const ce::wstring & strHost, ce::smart_ptr<FnFakeIpAddress> fnResultAddr, DWORD dwTTL );
    BOOL AddDNSEntry( ce::smart_ptr<FnDnsEntry> fnDnsEntry);

    BOOL AddPTREntry( ce::smart_ptr<FnFakeIpAddress> fnClientAddr, ce::smart_ptr<FnFakeIpAddress> fnServerAddr, const ce::wstring & strHost, const ce::wstring & strResult, DWORD dwTTL );
    BOOL AddPTREntry( ce::smart_ptr<FnPTREntry> fnPTREntry);

    //iterators
    //ip addresses
    typedef ce::list<ce::smart_ptr<FnFakeIpAddress>>::iterator AddressIterator;
    AddressIterator beginAddresses()
        {return m_lstServerAddresses.begin(); }
    AddressIterator endAddresses()
        {return m_lstServerAddresses.end(); }

    //dns entry
    typedef ce::list<ce::smart_ptr<FnDnsEntry>>::iterator DnsEntryIterator;
    DnsEntryIterator beginDnsEntries()
        {return m_lstDnsEntries.begin(); }
    DnsEntryIterator endDnsEntries()
        {return m_lstDnsEntries.end(); }

    //dns entry
    typedef ce::list<ce::smart_ptr<FnPTREntry>>::iterator PTREntryIterator;
    PTREntryIterator beginPTREntries()
        {return m_lstPTREntries.begin(); }
    PTREntryIterator endPTREntries()
        {return m_lstPTREntries.end(); }

};

