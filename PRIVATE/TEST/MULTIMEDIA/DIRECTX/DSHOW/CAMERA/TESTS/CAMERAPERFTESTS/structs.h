//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#pragma once

class CParameter
{
    public:
        CParameter();
        ~CParameter();

        WCHAR*  wzName;
        WCHAR*  wzRecipient;
        DWORD   dwCount;
        double  *pValues;
};


class CParameterList
{
    public:
        CParameterList();
        ~CParameterList();

        DWORD dwParameterCount;
        CParameter *pParameters;

        HRESULT CreateList( DWORD dwCount );
};

