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

#include <windows.h>
#include "assert.h"

#ifdef UTIL_HTTPLITE
#    include "dubinet.h"
#else
#    include "wininet.h"
#endif

#include "auto_xxx.hxx"
#include "string.hxx"




// Shared constants
static const DWORD dwHttpDefaultTimeout = 30 * 1000;    // 30 seconds
extern const CHAR c_szHttpVersion[];
extern const CHAR c_szHttpTimeout[];


// HttpBase
class HttpBase
{
public:
    HttpBase();
    static bool Initialize(LPCSTR pszAgent, ...);
    static ce::string *Identify(void);
    static void Uninitialize(void);

private:
    static ce::string*      m_pstrAgentName;
};
