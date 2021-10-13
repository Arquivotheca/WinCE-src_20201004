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
#include <pch.h>

#include "HttpBase.h"
#include "HttpChannel.h"
#include "CookieJar.h"




// HttpRequest
class HttpRequest
{
public:
    //  ctor dtor
    HttpRequest(DWORD dwCookie = 0);
    ~HttpRequest(void);

    // set up
    HRESULT Init(DWORD dwChannelCookie, LPCSTR pszUrl);
    void    Uninit();

    // build and send request
    HRESULT Open(LPCSTR pszVerb, LPCSTR pwszVersion = c_szHttpVersion);
    HRESULT AddHeader(LPCSTR pszHeaderName, LPCSTR pszHeaderValue, DWORD dwModifiers = HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);
    HRESULT AddHeader(LPCSTR pszHeaderName, int nHeaderValue, DWORD dwModifiers = HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);
    HRESULT Write(LPCWSTR pwszData, UINT CodePage = CP_UTF8);
    HRESULT Write(LPCSTR pszData);
    HRESULT Send();

    // read and parse reply
    HRESULT Read(LPVOID pBuffer, DWORD cbBytesToRead, DWORD* pcbBytesRead);
    HRESULT GetHeader(LPCSTR pszHeaderName, LPSTR pszBuffer, LPDWORD lpdwBufferLength);

    // SOAP shortcut
    HRESULT SendMessage(LPCSTR pszAction, LPCWSTR pwszMessage);

    // status
    DWORD   GetHttpStatus();
    HRESULT GetHresult();

private:
    HRESULT WaitForResult();
    static void CALLBACK  Callback(HINTERNET hInternet,
                                   DWORD_PTR dwContext,
                                   DWORD dwInternetStatus,
                                   LPVOID lpvStatusInformation,
                                   DWORD dwStatusInformationLength);


private:
    DWORD           m_dwChannelCookie;
    DWORD           m_dwRequestCookie;
    HANDLE          m_hReplyEvent;
    HttpChannel*    m_pChannel;
    HINTERNET       m_hConnect;
    HRESULT         m_hr;
    DWORD           m_dwHttpStatus;
    ce::string      m_strMessage;
    HINTERNET       m_hRequest;
};



#define AUTHOR_HTTPREQUEST 3
extern CookieJar<HttpRequest, AUTHOR_HTTPREQUEST> g_CJHttpRequest;

