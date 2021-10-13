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
#include <windows.h>

#include "HttpChannel.h"

CookieJar<HttpChannel, AUTHOR_HTTPCHANNEL> g_CJHttpChannel;

static const DWORD s_dwTransactionTimeout = 30 * 1000;    // 30 seconds

HttpChannel::HttpChannel(DWORD dwCookie)
    : m_pszUrl(NULL)
    , m_pszUrlPath(NULL)
    , m_pszServer(NULL)
    , m_pszUserName(NULL)
    , m_pszPassword(NULL)
    , m_hInternet(NULL)
    , m_hConnection(NULL)
    , m_dwChannelCookie(dwCookie)
    , m_hr(S_OK)
{
}
HttpChannel::~HttpChannel(void)
{
    // close tcp connection
    CloseConnection();

    // Free the saved strings
    FreeStrings();
}


HINTERNET HttpChannel::AttachRequest(LPCSTR pszUrl)
{
    // Check parameter
    if (pszUrl == NULL)
    {
        return NULL;
    }

    // if a new URL is supplied, process it
    if (m_pszUrl == NULL || strcmp(pszUrl, m_pszUrl))
    {
        if (!m_pszUrl)
        {
            CloseConnection();
        }
        if (FAILED(ParseURL(pszUrl)))
        {
            goto errorExit;
        }
    }

    // if necessary, re-establish the connection
    if (m_hConnection == NULL)
    {
        if (FAILED(OpenConnection()))
        {
            goto errorExit;
        }
    }

    return m_hConnection;

errorExit:
    // Error exit - restart the timer and exit the gate
    return NULL;
}

void HttpChannel::DetachRequest()
{
    CloseConnection();
}



HRESULT HttpChannel::OpenConnection(void)
{
    // Only needed if not already done
    if (m_hConnection)
    {
        return S_OK;
    }

    // Make the 'socket'
    m_hInternet = InternetOpenA(*HttpBase::Identify(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, INTERNET_FLAG_ASYNC);
    if(m_hInternet == NULL)
    {
        m_hr = HRESULT_FROM_WIN32(GetLastError());
        goto Cleanup;
    }

    // exercise the socket options
    DWORD dwTimeout = s_dwTransactionTimeout;
    if (!InternetSetOption(m_hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout))
    ||  !InternetSetOption(m_hInternet, INTERNET_OPTION_SEND_TIMEOUT, &dwTimeout, sizeof(dwTimeout))
    ||  !InternetSetOption(m_hInternet, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout))
    ||  !InternetSetOption(m_hInternet, INTERNET_OPTION_DATA_SEND_TIMEOUT, &dwTimeout, sizeof(dwTimeout))
    ||  !InternetSetOption(m_hInternet, INTERNET_OPTION_CONNECT_TIMEOUT , &dwTimeout, sizeof(dwTimeout)))
    {
        m_hr = HRESULT_FROM_WIN32(GetLastError());
        goto Cleanup;
    }

    // open the connection 
    m_hConnection = InternetConnectA(m_hInternet, m_pszServer, m_nPort, 
                                 m_pszUserName, m_pszPassword, 
                                 INTERNET_SERVICE_HTTP, 0, reinterpret_cast<DWORD_PTR>(this));
    if(m_hConnection == NULL)
    {
        m_hr = HRESULT_FROM_WIN32(GetLastError());
        goto Cleanup;
    }

    // Full speed ahead
    m_hr = S_OK;

Cleanup:
    if (FAILED(m_hr))
    {
        CloseConnection();
    }
    return m_hr;
}

void HttpChannel::CloseConnection(void)
{
    // Close handles as appropriate
    if (m_hConnection)
    {
        DWORD dw = InternetCloseHandle(m_hConnection);
        m_hConnection = NULL;
    }
    if (m_hInternet)
    {
        DWORD dw = InternetCloseHandle(m_hInternet);
        m_hInternet = NULL;
    }
}


HRESULT HttpChannel::ParseURL(LPCSTR pszUrl)
{
    char pszServer[INTERNET_MAX_HOST_NAME_LENGTH];
    char pszUrlPath[INTERNET_MAX_URL_LENGTH];
    char pszUserName[INTERNET_MAX_USER_NAME_LENGTH];
    char pszPassword[INTERNET_MAX_PASSWORD_LENGTH];

    // Check args
    if (pszUrl == NULL)
    {
        assert(pszUrl != NULL);
        return m_hr = E_POINTER;
    }

    // Prepare for the URL parser
    URL_COMPONENTSA urlComp = {0};
    urlComp.dwStructSize = sizeof(URL_COMPONENTS);
    urlComp.lpszHostName = pszServer;
    urlComp.dwHostNameLength = INTERNET_MAX_HOST_NAME_LENGTH;
    urlComp.lpszUrlPath = pszUrlPath;
    urlComp.dwUrlPathLength = INTERNET_MAX_URL_LENGTH;
    urlComp.lpszUserName = pszUserName;
    urlComp.dwUserNameLength = INTERNET_MAX_USER_NAME_LENGTH;
    urlComp.lpszPassword = pszPassword;
    urlComp.dwPasswordLength = INTERNET_MAX_PASSWORD_LENGTH;

    // Parse the URL
    if(!InternetCrackUrlA(pszUrl, 0, ICU_DECODE, &urlComp))
    {
        return m_hr = E_INVALIDARG;
    }
    if(urlComp.nScheme != INTERNET_SCHEME_HTTP)
    {
        return m_hr = ERROR_INTERNET_UNRECOGNIZED_SCHEME;
    }

    // Free up the old strings
    FreeStrings();

    // Save all the parsed out pieces of information
    int slen = strnlen(pszUrl, INTERNET_MAX_URL_LENGTH);
    m_pszUrl = (char *)malloc(slen+1);
    if (m_pszUrl == NULL)
    {
        return m_hr = E_OUTOFMEMORY;
    }
    memcpy(m_pszUrl, pszUrl, slen);
    m_pszUrl[slen] = 0;

    slen = strnlen(pszUrlPath, INTERNET_MAX_URL_LENGTH);
    m_pszUrlPath = (char *)malloc(slen+1);
    if (m_pszUrlPath == NULL)
    {
        return m_hr = E_OUTOFMEMORY;
    }
    memcpy(m_pszUrlPath, pszUrlPath, slen);
    m_pszUrlPath[slen] = 0;

    slen = strnlen(pszServer, INTERNET_MAX_HOST_NAME_LENGTH);
    m_pszServer = (char *)malloc(slen+1);
    if (m_pszServer == NULL)
    {
        return m_hr = E_OUTOFMEMORY;
    }
    memcpy(m_pszServer, pszServer, slen);
    m_pszServer[slen] = 0;

    slen = strnlen(pszUserName, INTERNET_MAX_USER_NAME_LENGTH);
    m_pszUserName = (char *)malloc(slen+1);
    if (m_pszUserName == NULL)
    {
        return m_hr = E_OUTOFMEMORY;
    }
    memcpy(m_pszUserName, pszUserName, slen);
    m_pszUserName[slen] = 0;

    slen = strnlen(pszPassword, INTERNET_MAX_PASSWORD_LENGTH);
    m_pszPassword = (char *)malloc(slen+1);
    if (m_pszPassword == NULL)
    {
        return m_hr = E_OUTOFMEMORY;
    }
    memcpy(m_pszPassword, pszPassword, slen);
    m_pszPassword[slen] = 0;

    m_nPort = urlComp.nPort;

    return S_OK;
}

void HttpChannel::FreeStrings(void)
{
    // Free up the URL
    if (m_pszUrl)
    {
        free(m_pszUrl);
        m_pszUrl = NULL;
    }

    // Free up the URLPath
    if (m_pszUrlPath)
    {
        free(m_pszUrlPath);
        m_pszUrlPath = NULL;
    }

    // Free up the Server name
    if (m_pszServer)
    {
        free(m_pszServer);
        m_pszServer = NULL;
    }

    // Free up the UserName
    if (m_pszUserName)
    {
        free(m_pszUserName);
        m_pszUserName = NULL;
    }

    // Free up the Password
    if (m_pszPassword)
    {
        free(m_pszPassword);
        m_pszPassword = NULL;
    }
}

LPCSTR HttpChannel::GetUrl(void) 
{
    return m_pszUrl; 
}

LPCSTR HttpChannel::GetUrlPath(void) 
{
    return m_pszUrlPath; 
}

