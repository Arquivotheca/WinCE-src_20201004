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

class FnFakeDHCPServer
{
private:
    //internal member variables
    ce::list<ce::smart_ptr<FnFakeIpAddress>> m_lstServerAddresses;
	ce::smart_ptr<FnFakeIpAddress> m_AddressPoolStartingAddress;

public:
	//import/export methods
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );

	void PrintDetails( int iDepth );

	BOOL AddServerAddress( ce::smart_ptr<FnFakeIpAddress> fnAddress );

	BOOL SetAddressPoolStartingAddress( ce::smart_ptr<FnFakeIpAddress> fnAddress );

	ce::smart_ptr<FnFakeIpAddress> GetAddressPoolStartingAddress();

	//iterators
    //ip addresses
    typedef ce::list<ce::smart_ptr<FnFakeIpAddress>>::iterator AddressIterator;
    AddressIterator beginAddresses()
        {return m_lstServerAddresses.begin(); }
    AddressIterator endAddresses()
        {return m_lstServerAddresses.end(); }
};