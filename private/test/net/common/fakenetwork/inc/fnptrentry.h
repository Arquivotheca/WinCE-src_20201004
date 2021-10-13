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
#include <auto_xxx.hxx>
#include "FnFakeIPAddress.h"
#include "StorageAddr.h"

// TODO: INherit from DNSEntry?
class FnPTREntry
{
private:
    //member variables
    ce::smart_ptr<FnFakeIpAddress> m_fnClientAddr;
    ce::smart_ptr<FnFakeIpAddress> m_fnServerAddr;
    ce::wstring m_strHost;
    ce::wstring m_fnResult;
    DWORD m_dwTTL;

    DWORD m_dwResponseDelay;

    //class helper functions
    BOOL _GetAddr( StorageADDR & addr, ce::smart_ptr<FnFakeIpAddress> & m_fnAddr );

public:
    //constructors
    FnPTREntry();
    FnPTREntry( ce::smart_ptr<FnFakeIpAddress> fnClientAddr, ce::smart_ptr<FnFakeIpAddress> fnServerAddr, const ce::wstring & strHost, const ce::wstring & fnResult, DWORD dwTTL ) :
        m_fnClientAddr( fnClientAddr ), m_fnServerAddr( fnServerAddr ), m_strHost( strHost ), m_fnResult( fnResult ), m_dwTTL ( dwTTL ), m_dwResponseDelay( 0 ) {};

    //import/export functions
    BOOL ExportXml( litexml::XmlElement_t & pElement );
    BOOL ImportXml( const litexml::XmlBaseElement_t & pElement );

    void PrintDetails( int iDepth );

    //member variable accessor functions
    BOOL GetClientAddr( StorageADDR & addr );
    BOOL GetServerAddr( StorageADDR & addr );

    //result
    ce::wstring GetResult( ) { return m_fnResult; }
    void SetResult( const ce::wstring & fnResult ) { m_fnResult = fnResult; }

    //Host
    ce::wstring GetHost( ) { return m_strHost; }
    void SetHost( const ce::wstring & strHost ) { m_strHost = strHost; }
    //TTL
    DWORD GetTTL( ) { return m_dwTTL; }
    void SetTTL( DWORD dwTTL ) { m_dwTTL = dwTTL; }

    //delay
    DWORD GetResponseDelay( ) { return m_dwResponseDelay; }
    void SetResponseDelay( DWORD dwResponseDelay ) { m_dwResponseDelay = dwResponseDelay; }

};