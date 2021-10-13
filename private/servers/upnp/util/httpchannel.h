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

#include <auto_xxx.hxx>
#include <string.hxx>
#include <pch.h>
#include "HttpBase.h"
#include "CookieJar.h"




// HttpChannel
class HttpChannel
{
public:
    //  ctor dtor
    HttpChannel(DWORD dwCookie = 0);
    ~HttpChannel(void);

    // Request usage of the channel for an HTTP exchange
    HINTERNET AttachRequest(LPCSTR pszUrl);
    void      DetachRequest(void);

    // utilities
    LPCSTR  GetUrl(void);
    LPCSTR  GetUrlPath(void);


private:
    HRESULT ParseURL(LPCSTR pszUrl);
    void    FreeStrings(void);
    HRESULT OpenConnection(void);
    void    CloseConnection(void);

private:
    HINTERNET        m_hInternet;
    HINTERNET        m_hConnection;
    char *           m_pszUrl;
    char *           m_pszUrlPath;
    char *           m_pszServer;
    char *           m_pszUserName;
    char *           m_pszPassword;
    WORD             m_nPort;
    HRESULT          m_hr;
    DWORD            m_dwChannelCookie;
};

#define AUTHOR_HTTPCHANNEL 2

extern CookieJar<HttpChannel,AUTHOR_HTTPCHANNEL> g_CJHttpChannel;
