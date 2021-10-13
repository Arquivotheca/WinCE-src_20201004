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
#include "HttpRequest.h"

#ifdef UTIL_HTTPLITE
#define _WININET_  // don't allow dlna.h to pull in wininet stuff in the httplite case
#endif

#include "dlna.h"



#ifndef Chk
#define Chk(val) { hr = (val); if (FAILED(hr)) goto Cleanup; }
#endif

#ifndef ChkBool
#define ChkBool(val, x) { if (!(val)) { hr = x; goto Cleanup; } }
#endif

CookieJar<HttpRequest, AUTHOR_HTTPREQUEST> g_CJHttpRequest;
static const DWORD s_dwTransactionTimeout = 30 * 1000;    // 30 seconds


// HttpRequest
HttpRequest::HttpRequest(DWORD dwCookie)
    : m_dwChannelCookie(0)
    , m_dwRequestCookie(dwCookie)
    , m_hReplyEvent(NULL)
    , m_pChannel(NULL)
    , m_hConnect(NULL)
    , m_hr(S_OK)
    , m_dwHttpStatus(0)
    , m_hRequest(NULL)
{
}

// ~HttpRequest
HttpRequest::~HttpRequest(void)
{
    Uninit();
}

// Attach
HRESULT HttpRequest::Init(DWORD dwChannelCookie, LPCSTR pszUrl)
{
    HRESULT hr = S_OK;

    // check argument
    ChkBool (pszUrl != NULL, E_POINTER)
    ChkBool (dwChannelCookie != 0, E_INVALIDARG)
    ChkBool (m_dwChannelCookie == 0, E_UNEXPECTED)

    // Acquire channel
    Chk(g_CJHttpChannel.Acquire(dwChannelCookie, m_pChannel));
    m_dwChannelCookie = dwChannelCookie;

    // Take the channel
    m_hReplyEvent = CreateEvent(NULL, false, false, NULL);
    ChkBool(m_hReplyEvent != NULL, HRESULT_FROM_WIN32(GetLastError()));

    // Set the URL
    m_hConnect = m_pChannel->AttachRequest(pszUrl);
    ChkBool(m_hConnect != NULL, E_FAIL);

Cleanup:
    if (FAILED(hr))
    {
        Uninit();
    }
    return m_hr = hr;
}

// uninit
void HttpRequest::Uninit()
{
    // Close my internet handle
    if (m_hRequest)
    {
        InternetCloseHandle(m_hRequest);
        m_hRequest = NULL;
    }

    // free up the event
    if (m_hReplyEvent)
    {
        CloseHandle(m_hReplyEvent);
        m_hReplyEvent = NULL;
    }

    // drop the channel
    if (m_pChannel != NULL)
    {
        m_pChannel->DetachRequest();
        g_CJHttpChannel.Release(m_dwChannelCookie, m_pChannel);
        m_pChannel = NULL;
    }
}

// SendMessage - shortcut for SOAP
HRESULT HttpRequest::SendMessage(LPCSTR pszAction, LPCWSTR pwszMessage)
{
    HRESULT hr = S_OK;

    ChkBool(pszAction != NULL && pwszMessage != NULL, E_POINTER);

    // try POST request 
    Chk(Open("POST", c_szHttpVersion));
    Chk(AddHeader("SOAPACTION", pszAction));
    Chk(AddHeader("CONTENT-TYPE", "text/xml; charset=\"utf-8\""));
    Chk(Write(pwszMessage, CP_UTF8));
    Chk(Send());

Cleanup:
    return m_hr = hr;
}


// Open
HRESULT HttpRequest::Open(LPCSTR pszVerb, LPCSTR pszVersion/* = NULL*/)
{
    HRESULT hr = S_OK;

    ChkBool(pszVerb != NULL && pszVersion != NULL, E_POINTER);
    ChkBool(m_pChannel != NULL, E_FAIL);
    ChkBool(m_hRequest == NULL, E_UNEXPECTED);

#ifdef UTIL_HTTPLITE
    m_hRequest = HttpOpenRequestA(m_hConnect, pszVerb, m_pChannel->GetUrlPath(), pszVersion, NULL, NULL, 
                                  INTERNET_FLAG_NO_CACHE_WRITE, 
                                  (DWORD_PTR)m_dwRequestCookie);
    ChkBool(m_hRequest != NULL, HRESULT_FROM_WIN32(GetLastError()));
    INTERNET_STATUS_CALLBACK callback = InternetSetStatusCallback(m_hRequest, &Callback);
    ChkBool(callback != INTERNET_INVALID_STATUS_CALLBACK, E_FAIL);
#else
    m_hRequest = HttpOpenRequestA(m_hConnect, pszVerb, m_pChannel->GetUrlPath(), pszVersion, NULL, NULL, 
                                  INTERNET_FLAG_RESYNCHRONIZE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 
                                  (DWORD_PTR)m_dwRequestCookie);
    ChkBool(m_hRequest != NULL, HRESULT_FROM_WIN32(GetLastError()));
    INTERNET_STATUS_CALLBACK callback = InternetSetStatusCallbackA(m_hRequest, &Callback);
    ChkBool(callback != INTERNET_INVALID_STATUS_CALLBACK, E_FAIL);
#endif

Cleanup:
    return m_hr = hr;
}


// AddHeader
HRESULT HttpRequest::AddHeader(LPCSTR pszHeaderName, int nHeaderValue, DWORD dwModifiers/* = HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD*/)
{
    HRESULT hr = S_OK;
    char pszHeaderValue[11];

    ChkBool(pszHeaderName != NULL, E_POINTER);
    _itoa_s(nHeaderValue, pszHeaderValue, _countof(pszHeaderValue), 10);
    Chk(AddHeader(pszHeaderName, pszHeaderValue, dwModifiers));

Cleanup:
    return m_hr = hr;
}


// AddHeader
HRESULT HttpRequest::AddHeader(LPCSTR pszHeaderName, LPCSTR pszHeaderValue, DWORD dwModifiers/* = HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD*/)
{
    HRESULT hr = S_OK;
    ce::string strHeader;

    ChkBool (pszHeaderName != NULL && pszHeaderValue != NULL, E_POINTER);
    ChkBool (m_pChannel != NULL, E_FAIL);

    strHeader.reserve(strlen(pszHeaderName) + strlen(pszHeaderValue) + 3);
    strHeader = pszHeaderName;
    strHeader += ": ";
    strHeader += pszHeaderValue;

    if (!HttpAddRequestHeadersA(m_hRequest, strHeader, -1, dwModifiers))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

Cleanup:
    return m_hr = hr;
}


// Write
HRESULT HttpRequest::Write(LPCSTR pszData)
{
    HRESULT hr = S_OK;
    ChkBool (pszData != NULL, E_POINTER);
    ChkBool (m_pChannel != NULL, E_FAIL);
    m_strMessage += pszData;
Cleanup:
    return m_hr = hr;
}


// Write
HRESULT HttpRequest::Write(LPCWSTR pwszData, UINT CodePage/* = CP_UTF8*/)
{
    HRESULT hr = S_OK;
    int nSizeCurrent = 0;
    int nSizeAdd = 0;

    ChkBool(pwszData != NULL, E_POINTER);
    ChkBool (m_pChannel != NULL, E_FAIL);

    nSizeCurrent = m_strMessage.size();
    nSizeAdd = WideCharToMultiByte(CodePage, 0, pwszData, -1, NULL, 0, NULL, NULL);
    if(!nSizeAdd)
    {
        nSizeAdd = WideCharToMultiByte(CP_ACP, 0, pwszData, -1, NULL, 0, NULL, NULL);
        ChkBool(nSizeAdd != 0, E_FAIL);
        CodePage = CP_ACP;
    }
    m_strMessage.reserve(nSizeCurrent + nSizeAdd);
    
    // regardless of allocation failure or integer overflow in reserve call above,
    // WideCharToMultiByte will only write up to actual capacity of the buffer
    PREFAST_SUPPRESS(12009, "");
    WideCharToMultiByte(CodePage, 0, pwszData, -1, m_strMessage.get_buffer() + nSizeCurrent, m_strMessage.capacity() - nSizeCurrent, NULL, NULL);

Cleanup:
    return m_hr = hr;
}


// Send
HRESULT HttpRequest::Send()
{
    HRESULT hr = S_OK;
    LPCSTR  pszMessage = NULL;
    DWORD   dwLength = 0;
    DWORD   dwHttpStatus = 0;
    DWORD cb = sizeof(dwHttpStatus);

    ChkBool(m_pChannel != NULL, E_FAIL);

    // Check message
    dwLength = m_strMessage.size();
    if (dwLength != 0)
    {
        pszMessage = m_strMessage;
    }

    // Send the request and wait for a reply
    if(!HttpSendRequestA(m_hRequest, NULL, 0, const_cast<LPSTR>(pszMessage), dwLength))
    {
        // Either failed or is pending
        DWORD dwError = GetLastError();
        ChkBool(dwError == ERROR_IO_PENDING, HRESULT_FROM_WIN32(dwError));
        Chk(WaitForResult());
    }

    // Get and evaluate the status
    if (!HttpQueryInfo(m_hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwHttpStatus, &cb, NULL))
    {
        ChkBool(0, HRESULT_FROM_WIN32(GetLastError()));
    }
    ASSERT(cb == sizeof(dwHttpStatus));

    // Record the result
    m_dwHttpStatus = dwHttpStatus;
    hr = IsHttpStatusOK(dwHttpStatus) ? S_OK : S_FALSE;

Cleanup:
    m_strMessage.resize(0);
    return m_hr = hr;
}


// Read
HRESULT HttpRequest::Read(LPVOID pBuffer, DWORD cbBytesToRead, DWORD* pcbBytesRead)
{
    HRESULT hr = S_OK;
    DWORD   dwError = ERROR_SUCCESS;

    ChkBool(pBuffer != NULL && pcbBytesRead != NULL, E_POINTER);
    ChkBool(m_pChannel != NULL, E_FAIL);

    if(!InternetReadFile(m_hRequest, pBuffer, cbBytesToRead, pcbBytesRead))
    {
        Chk(WaitForResult());
    }

Cleanup:
    return m_hr = hr;
}

// Get error in hresult form
HRESULT HttpRequest::GetHresult()
{
    return m_hr;
}

// GetStatus
DWORD HttpRequest::GetHttpStatus()
{
    return m_dwHttpStatus;
}


// GetHeader
HRESULT HttpRequest::GetHeader(LPCSTR pszHeaderName, LPSTR pszBuffer, LPDWORD lpdwBufferLength)
{
    DWORD nLen = strlen(pszHeaderName) + 1;

    // remove pre-existing error: insufficient buffer
    if (m_hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
    {
        m_hr = S_OK;
    }

    if(nLen > *lpdwBufferLength)
    {
        *lpdwBufferLength = nLen;
        return m_hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }
    
    memcpy(pszBuffer, pszHeaderName, nLen);
    
    if (!HttpQueryInfoA(m_hRequest, HTTP_QUERY_CUSTOM, pszBuffer, lpdwBufferLength, NULL))
    {
        return m_hr = HRESULT_FROM_WIN32(GetLastError());
    }

    return m_hr;
}


// Waiting for callback
HRESULT HttpRequest::WaitForResult()
{
    HRESULT hr = S_OK;
    DWORD   dwError = ERROR_SUCCESS;
    DWORD   dwWaitResult;

    // Must be inited
    ChkBool(m_pChannel != NULL, E_FAIL);
    ChkBool(m_hReplyEvent != NULL, E_UNEXPECTED);

    // Must release the lock while waiting
    dwWaitResult = WaitForSingleObject(m_hReplyEvent, s_dwTransactionTimeout);
    if(dwWaitResult == WAIT_TIMEOUT)
    {
        Chk(HRESULT_FROM_WIN32(ERROR_INTERNET_TIMEOUT));
    }

Cleanup:
    return m_hr = hr;
}


// Callback Routine - a static member
void CALLBACK HttpRequest::Callback(HINTERNET hInternet,
                                    DWORD_PTR dwContext,
                                    DWORD dwInternetStatus,
                                    LPVOID lpvStatusInformation,
                                    DWORD dwStatusInformationLength)
{
    if (dwInternetStatus == INTERNET_STATUS_REQUEST_COMPLETE)
    {
        HttpRequest* pThis = NULL;
        DWORD dwCookie = (DWORD)dwContext;
        if (SUCCEEDED(g_CJHttpRequest.CallbackAcquire(dwCookie, pThis)) && pThis != NULL)
        {
            INTERNET_ASYNC_RESULT* pAsyncResult = reinterpret_cast<INTERNET_ASYNC_RESULT*>(lpvStatusInformation);
            pThis->m_hr = HRESULT_FROM_WIN32(pAsyncResult->dwError);
            SetEvent(pThis->m_hReplyEvent);
        }
        g_CJHttpRequest.CallbackRelease(dwCookie, pThis);
    }
}

