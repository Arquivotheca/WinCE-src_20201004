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

class CFilterFactory :
    public CComObjectRootEx<CComMultiThreadModel>,
    public IClassFactory
{
public:
    CFilterFactory();
    ~CFilterFactory();

    BEGIN_COM_MAP( CFilterFactory )
        COM_INTERFACE_ENTRY( IClassFactory )
    END_COM_MAP()


    static CUnknown* WINAPI CreateInstance( LPUNKNOWN pUnkOuter, HRESULT* phr );
    PFCREATEINSTANCE m_pfnFilterCreateInstance;

    STDMETHOD( CreateInstance )( LPUNKNOWN pUnkOuter, REFIID riid, void **ppvObj );
    STDMETHOD( LockServer )( BOOL flock );
};
