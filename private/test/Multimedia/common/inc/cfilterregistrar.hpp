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

class CFilterRegistrar
{
public:

    CFilterRegistrar();
    ~CFilterRegistrar();

    HRESULT RegisterFilter( WCHAR *wzExtension, PFCREATEINSTANCE pFilterCreationFunction );
    HRESULT UnregisterFilter();

private:
    HRESULT RegisterFormatEntries();
    HRESULT RegisterFilterEntries();
    HRESULT CoRegisterFilter( PFCREATEINSTANCE pFilterCreationFunction );

    HRESULT UnregisterFormatEntries();
    HRESULT UnregisterFilterEntries();
    HRESULT CoUnregisterFilter();

    void    *m_pFilterCreationFunction;
    CLSID   m_FilterCLSID;
    DWORD   m_dwRegisterCookie;
    WCHAR   *m_wzExtension;
};

