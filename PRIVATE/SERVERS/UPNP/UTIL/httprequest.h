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
#ifndef __HTTP_REQUEST__
#define __HTTP_REQUEST__

#ifdef UTIL_HTTPLITE
#    include "dubinet.h"
#else
#    include "wininet.h"
#endif

#include "auto_xxx.hxx"
#include "string.hxx"

#include "assert.h"

// async_internet_handle
class async_internet_handle
{
public:
    async_internet_handle()
        : m_handle(NULL)
    {
        m_hEventClosed = CreateEvent(NULL, true, false, NULL);
        m_hEventComplete = CreateEvent(NULL, false, false, NULL);
    }

    ~async_internet_handle()
        {close(); }

    operator HINTERNET()
        {return m_handle; }

    async_internet_handle& operator=(const HINTERNET& handle)
    {
        close();

        if(m_handle = handle)
        {
            ResetEvent(m_hEventClosed);

#ifdef UTIL_HTTPLITE
            INTERNET_STATUS_CALLBACK callback = InternetSetStatusCallback(m_handle, &Callback);
#else
            INTERNET_STATUS_CALLBACK callback = InternetSetStatusCallbackA(m_handle, &Callback);
#endif

            if(callback == INTERNET_INVALID_STATUS_CALLBACK)
            {
                InternetCloseHandle(m_handle);
                m_handle = NULL;
            }
        }
        
        return *this;
    }

    void close()
    {
        if(m_handle)
        {
            InternetCloseHandle(m_handle);
            WaitForSingleObject(m_hEventClosed, INFINITE);
            assert(m_handle == NULL);
        }
    }

    DWORD wait(int nTimeout);

private:
    static void CALLBACK Callback(HINTERNET hInternet,
                                  DWORD_PTR dwContext,
                                  DWORD dwInternetStatus,
                                  LPVOID lpvStatusInformation,
                                  DWORD dwStatusInformationLength);
private:
    HINTERNET       m_handle;
    ce::auto_handle m_hEventClosed;
    ce::auto_handle m_hEventComplete;
    DWORD           m_dwError;
};


// HttpRequest
class HttpRequest
{
public:
    HttpRequest(DWORD dwTimeout = 30 * 1000);

    static bool Initialize(LPCSTR pszAgent, ...);
    static void Uninitialize();
    
    bool Open(LPCSTR pszVerb, LPCSTR pszUrl, LPCSTR pwszVersion = NULL);
    bool AddHeader(LPCSTR pszHeaderName, LPCSTR pszHeaderValue, DWORD dwModifiers = HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);
    bool AddHeader(LPCSTR pszHeaderName, int nHeaderValue, DWORD dwModifiers = HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD);
    void Write(LPCWSTR pwszData, UINT CodePage = CP_UTF8);
    void Write(LPCSTR pszData);
    bool Send();
    bool Read(LPVOID pBuffer, DWORD cbBytesToRead, DWORD* pcbBytesRead);
    DWORD GetStatus();
    bool GetHeader(LPCSTR pszHeaderName, LPSTR pszBuffer, LPDWORD lpdwBufferLength);

    DWORD GetError() const
        {return m_dwError; }

    HRESULT GetHresult() const
        {return HRESULT_FROM_WIN32(m_dwError); }

private:
    static ce::string*      m_pstrAgentName;
    ce::auto_hinternet      m_hInternet;
    async_internet_handle   m_hConnect;
    async_internet_handle   m_hRequest;
    ce::string              m_strMessage;
    DWORD                   m_dwTimeout;
    DWORD                   m_dwError;
};

#ifndef HTTP_STATUS_PRECOND_FAILED
#    define HTTP_STATUS_PRECOND_FAILED      412 // precondition given in request failed
#endif

#endif // __HTTP_REQUEST__
