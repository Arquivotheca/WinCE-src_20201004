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
#include <litexml.h>
#include "StorageAddr.h"

class FnFakeIpAddress
{
private:
    //member 
    int m_iFamily;
    ce::wstring m_strAddress;
    int m_iSubnetLength;
    VOID FnFakeIpAddress::_ReverseString( ce::wstring & string, int start, int end );

public:
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );

    void PrintDetails( int iDepth );
    enum AddressPrintDetails
    {
        APDAddress,
        APDAddressAndSubnet,
        APDrDNS 
    };
    ce::wstring ToString(AddressPrintDetails details = APDAddressAndSubnet);
public:
    void SetIPAddress( int iFamily, ce::wstring strAddress ) { m_iFamily = iFamily; m_strAddress = strAddress; }
    void SetSubnetLength( int iSubnetLength ) { m_iSubnetLength = iSubnetLength; }

    BOOL ExportStorageAddr( StorageADDR & addr );

    ce::wstring GetAddress( ) { return m_strAddress; }
    int GetAddressFamily( ) { return m_iFamily; }
};