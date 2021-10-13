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
#include <stdexcept>

class getlast_error : public std::runtime_error
{
public:
    DWORD lastErr;
    std::string lastMsg;
    getlast_error();
    getlast_error(LPCSTR str);
};

class hresult_error : public std::runtime_error
{
public:
    HRESULT hr;
    hresult_error(HRESULT _hr) :
        std::runtime_error("hresult_error"),
        hr(_hr) {}
    hresult_error(LPSTR msg, HRESULT _hr) :
        std::runtime_error(msg),
        hr(_hr) {}
};