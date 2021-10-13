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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#pragma once
#include "Exceptions.h"

struct coinit_error : public getlast_error
{
    HRESULT hr;
    coinit_error(HRESULT _hr, LPSTR msg) :
        getlast_error(msg),
        hr(_hr) {}
};

struct AutoCoInit
{
    AutoCoInit()
    {
        HRESULT hr;
        if (FAILED(hr = ::CoInitialize(NULL)))
            throw coinit_error(hr, "CoInitialize");
    }

    AutoCoInit(DWORD dwCoInit)
    {
        HRESULT hr;
        if (FAILED(hr = ::CoInitializeEx(NULL, dwCoInit)))
            throw coinit_error(hr, "CoInitializeEx");
    }

    ~AutoCoInit()
    {
        ::CoUninitialize();
    }
};