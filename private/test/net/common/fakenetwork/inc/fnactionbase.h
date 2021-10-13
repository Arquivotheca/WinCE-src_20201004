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
//
#pragma once

#include <windows.h>
#include <string.hxx>
#include <litexml.h>
#include <auto_xxx.hxx>
#include "smart_ptr_cast.hxx"

class FnActionBase
{
public:
    enum FnActionType
    {
        AT_SocketAccept,
        AT_SocketClose,
        AT_HttpRequest,
        AT_HttpResponse,
        AT_SSLNegotiation,
        AT_Unknown
    };

    virtual ~FnActionBase() {};
    // import/export methods   
    virtual BOOL ExportXml( litexml::XmlElement_t & pElement ) = 0;
    virtual BOOL ImportXml( const litexml::XmlBaseElement_t & pElement ) = 0;

    virtual void PrintDetails( int iDepth ) = 0;

    virtual FnActionType GetActionType() = 0;

    static ce::wstring ActionTypeToString( FnActionType ActionType );
    static FnActionType StringToActionType(const ce::wstring & strActionType );
    static ce::smart_ptr<FnActionBase> CreateAction( FnActionType ActionType );
    static ce::smart_ptr<FnActionBase> CreateAction( const ce::wstring & strActionType );

};

