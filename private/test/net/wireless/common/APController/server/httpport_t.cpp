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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the HttpPort_t class.
//
// ----------------------------------------------------------------------------

#include "HttpPort_t.hpp"

#include <assert.h>
#include <tchar.h>
#include <strsafe.h>
#include <wininet.h>
#include <winsock.h>

#include <auto_xxx.hxx>

// Define this if you want LOTS of debug output:
#define EXTRA_DEBUG 1
//#define EXTRA_EXTRA_DEBUG 1

using namespace ce::qa;


/* ============================= HttpRequest =============================== */

// ----------------------------------------------------------------------------
//
// Stores information about the request to be performed.
//
namespace ce {
namespace qa {
class HttpRequest_t
{
private:

    // Web-page name:
    ce::wstring m_PageName;

    // Web-page URL:
    WCHAR *m_pPageURL;

    // Canonized POST parameters:
    char *m_pPostParams;
    DWORD m_PostParamsLength;

    // True if parameters are being POSTed:
    bool m_fPostParams;

    // Canonized parameter buffers:
    ce::wstring m_GetParams;
    ce::string m_PostParams;

public:

    // Page-load constructor:
    HttpRequest_t(
        const WCHAR *pPageName)   // page to be loaded
        : m_PageName(pPageName),
          m_fPostParams(false),
          m_pPageURL(&m_PageName[0]),
          m_pPostParams(NULL),
          m_PostParamsLength(0)
    { }

    // Page-update constructor:
    HttpRequest_t(
        const WCHAR *pMethod,     // update method (GET or POST)
        const WCHAR *pAction)     // action to be performed
        : m_PageName(pAction),
          m_fPostParams(_wcsicmp(pMethod, L"POST") == 0),
          m_pPageURL(&m_PageName[0]),
          m_pPostParams(NULL),
          m_PostParamsLength(0)
    { }

    // Destructor:
   ~HttpRequest_t(void)
    { }

    // Canonizes the web-page parameters (if any):
    DWORD
    Canonize(
        const ValueMap &Params);

    // Gets the name of the web-page:
    const WCHAR *
    GetPageName(void) const
    {
        return m_PageName;
    }

    // Gets the GET or POST method:
    const WCHAR *
    GetConnectMethod(void) const
    {
        return m_fPostParams? L"POST" : L"GET";
    }

    // Gets the page-name URL:
    const WCHAR *
    GetPageURL(void) const
    {
        return m_pPageURL;
    }

    // Gets the encoded POST information (if any):
    const char *
    GetPostInfo(void) const
    {
        return m_pPostParams;
    }
    DWORD
    GetPostInfoLength(void) const
    {
        return m_PostParamsLength;
    }
};
};
};

// ----------------------------------------------------------------------------
//
// Canonizes the specified web-page parameter value.
//
static DWORD
CanonizeParam(
    ValueMapCIter Param,
    ce::wstring  *pCanonized)
{
    DWORD result;

    if (0 == Param->second.length())
    {
        pCanonized->clear();
    }
    else
    {
        // InternetCanonicalize strips leading and trailing spaces.
        // To avoid that, we manually strip them then put them back
        // on after the canonization finishes.
        int head = 0, tail = 0; DWORD reserved = 1;
        ce::wstring localBuffer(Param->second);
        WCHAR *pEnd, *pStart = localBuffer.get_buffer();
        while (iswspace(pStart[0]))
        {
            pStart++;
            head++;
            reserved += 3;
        }
        for (pEnd = pStart ; pEnd[0] ; pEnd++)
            ;
        while (--pEnd > pStart && iswspace(pEnd[0]))
        {
            pEnd[0] = L'\0';
            tail++;
            reserved += 3;
        }

        for (DWORD bufferChars = pCanonized->capacity() ;;)
        {
            if (bufferChars <= reserved)
            {
                result = ERROR_INSUFFICIENT_BUFFER;
            }
            else
            {
                DWORD canonChars = bufferChars - reserved;
                if (InternetCanonicalizeUrl(pStart,
                                           &(*pCanonized)[head*3],
                                           &canonChars, 0))
                {
                    pStart = &(*pCanonized)[0];
                    while (head-- > 0)
                    {
                       *(pStart++) = L'%';
                       *(pStart++) = L'2';
                       *(pStart++) = L'0';
                         canonChars += 3;
                    }
                    pEnd = &(*pCanonized)[(size_t)canonChars];
                    while (tail-- > 0)
                    {
                       *(pEnd++) = L'%';
                       *(pEnd++) = L'2';
                       *(pEnd++) = L'0';
                         canonChars += 3;
                    }
                    pEnd[0] = L'\0';
                    pCanonized->resize(canonChars);
                    break;
                }
                result = GetLastError();
            }

            if (ERROR_INSUFFICIENT_BUFFER == result && 1024 > bufferChars)
            {
                bufferChars = (bufferChars * 3) / 2;
                if (pCanonized->reserve(bufferChars))
                    continue;
                result = ERROR_OUTOFMEMORY;
            }

            LogError(TEXT("Error canonizing request data: %s"),
                     Win32ErrorText(result));
            return result;
        }
    }
#if defined(EXTRA_DEBUG)
    LogDebug(TEXT("[AC]  param \"%hs\" = \"%ls\""),
            &(Param->first)[0], &(*pCanonized)[0]);
#endif
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Canonizes the web-page parameters (if any).
//
DWORD
HttpRequest_t::
Canonize(
    const ValueMap &Params)
{
    HRESULT hr;
    DWORD result;
    m_pPageURL = &m_PageName[0];
    m_pPostParams = NULL;
    m_PostParamsLength = 0;

    if (0 == Params.size())
    {
        return ERROR_SUCCESS;
    }

    // Canonize GET parameters.
    if (!m_fPostParams)
    {
        if (!m_GetParams.assign(m_PageName))
        {
            LogError(TEXT("Error GETing request data: out of memory"));
            return ERROR_OUTOFMEMORY;
        }

        const WCHAR *separator = wcschr(m_PageName, L'?')? L"&" : L"?";

        ce::wstring nameBuff, valuBuff;
        for (ValueMapCIter it = Params.begin() ; it != Params.end() ; ++it)
        {
            result = CanonizeParam(it, &valuBuff);
            if (ERROR_SUCCESS != result)
                return result;

            hr = WiFUtils::ConvertString(&nameBuff, it->first, "GET request data",
                                                    it->first.length(), CP_UTF8);
            if (FAILED(hr))
                return HRESULT_CODE(hr);
            
            if (!m_GetParams.append(separator)
             || !m_GetParams.append(nameBuff)
             || !m_GetParams.append(L"=")
             || !m_GetParams.append(valuBuff))
            {
                LogError(TEXT("Error GETing request data: out of memory"));
                return ERROR_OUTOFMEMORY;
            }

            separator = L"&";
        }

        m_pPageURL = &m_GetParams[0];
    }

    // Canonize POST parameters.
    else
    {
        const char *separator = "";

        ce::wstring wcValuBuff;
        ce::string  mbValuBuff;
        for (ValueMapCIter it = Params.begin() ; it != Params.end() ; ++it)
        {
            result = CanonizeParam(it, &wcValuBuff);
            if (ERROR_SUCCESS != result)
                return result;

            hr = WiFUtils::ConvertString(&mbValuBuff, wcValuBuff, "POST request data",
                                                      wcValuBuff.length(), CP_UTF8);
            if (FAILED(hr))
                return HRESULT_CODE(hr);

            if (!m_PostParams.append(separator)
             || !m_PostParams.append(it->first)
             || !m_PostParams.append("=")
             || !m_PostParams.append(mbValuBuff))
            {
                LogError(TEXT("Error POSTing request data: out of memory"));
                return ERROR_OUTOFMEMORY;
            }

            separator = "&";
        }

        m_pPostParams = &m_PostParams[0];
        m_PostParamsLength = m_PostParams.length();
    }

    return ERROR_SUCCESS;
}

/* ============================== HttpHandle =============================== */

// ----------------------------------------------------------------------------
//
// Provides a contention-controlled interface to a web-server.
//
namespace ce {
namespace qa {
class HttpHandle_t
{
private:

    // Server host-name, user and password:
    ce::tstring m_ServerHost;
    ce::tstring m_UserName;
    ce::tstring m_Password;

    // Internet and device handles:
    HINTERNET m_OpenHandle;
    HINTERNET m_ConnectHandle;

    // Number connected HttpPorts:
    int m_NumberConnected;

    // Cookies:
    ValueMap m_Cookies;

    // Synchronization object:
    ce::critical_section m_Locker;

    // Pointer to next handle in list:
    HttpHandle_t *m_Next;

    // Construction is done by AttachHandle:
    HttpHandle_t(const WCHAR *pServerHost);

public:

    // Destructor:
   ~HttpHandle_t(void);

    // Attaches or releases the web-server connection:
    static HttpHandle_t *
    AttachHandle(const TCHAR *pServerHost);
    static void
    DetachHandle(const HttpHandle_t *pHandle);

    // Gets the web-server name or address:
    const TCHAR *
    GetServerHost(void) const
    {
        return m_ServerHost;
    }

    // Gets or sets the user-name:
    const TCHAR *
    GetUserName(void) const
    {
        return m_UserName;
    }
    void
    SetUserName(const TCHAR *Value)
    {
        m_UserName = Value;
    }

    // Gets or sets the admin password:
    const TCHAR *
    GetPassword(void) const
    {
        return m_Password;
    }
    void
    SetPassword(const TCHAR *Value)
    {
        m_Password = Value;
    }

    // Retrieves an object which can be locked to prevent other threads
    // from using the port:
    // Callers should lock this object before performing any I/O operations.
    ce::critical_section &
    GetLocker(void)
    {
        return m_Locker;
    }

    // Connects to the device's web-server to access the specified page:
    DWORD
    Connect(
        const HttpRequest_t &Request,
        HINTERNET           *pResource);

    // Closes the existing connection to force a reconnection next time:
    void
    Disconnect(void);

    // Loads the specified web-page:
    DWORD
    SendPageRequest(
        const HttpRequest_t &Request,
        long                 SleepMillis, // wait time before reading response
        ce::string          *pResponse);  // response from device


    // Gets or sets the specified cookie:
    const WCHAR *
    GetCookie(const char *pName) const;
    DWORD
    SetCookie(const char *pName, const ce::wstring &Value);

private:
    // Sends the specified request to the specified resource.
    DWORD
    SendRequest(
        HINTERNET            Resource,    // connected HTTP resource
        const HttpRequest_t &Request,     // page to be sent
        DWORD               *pStatus);    // HTTP request status code
};
};
};

// ----------------------------------------------------------------------------
//
// Constructor.
//
HttpHandle_t::
HttpHandle_t(
    const TCHAR *pServerHost)
    : m_ServerHost(pServerHost),
      m_OpenHandle(NULL),
      m_ConnectHandle(NULL),
      m_NumberConnected(1),
      m_Next(NULL)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
HttpHandle_t::
~HttpHandle_t(void)
{
    Disconnect();
}

// ----------------------------------------------------------------------------
//
// Existing handles:
//

static HttpHandle_t        *HttpHandles = NULL;
static ce::critical_section HttpHandlesLocker;


// ----------------------------------------------------------------------------
//
// Attaches the web-server handle.
//
HttpHandle_t *
HttpHandle_t::
AttachHandle(
    const TCHAR *pServerHost)
{
    ce::gate<ce::critical_section> locker(HttpHandlesLocker);

    HttpHandle_t *hand = HttpHandles;
    for (;;)
    {
        if (NULL == hand)
        {
            hand = new HttpHandle_t(pServerHost);
            if (NULL == hand)
            {
                LogError(TEXT("[AC] Can't allocate HttpHandle for %s"),
                         pServerHost);
            }
            else
            {
                hand->m_Next = HttpHandles;
                HttpHandles = hand;
            }
            break;
        }
        if (_tcscmp(pServerHost, hand->GetServerHost()) == 0)
        {
            hand->m_NumberConnected++;
            break;
        }
        hand = hand->m_Next;
    }

    return hand;
}

// ----------------------------------------------------------------------------
//
// Releases the web-server handle.
//
void
HttpHandle_t::
DetachHandle(const HttpHandle_t *pHandle)
{
    ce::gate<ce::critical_section> locker(HttpHandlesLocker);

    HttpHandle_t **parent = &HttpHandles;
    for (;;)
    {
        HttpHandle_t *hand = *parent;
        if (NULL == hand)
        {
            assert(!"Tried to detach unknown HttpHandle");
            break;
        }
        if (hand == pHandle)
        {
            if (--hand->m_NumberConnected <= 0)
            {
               *parent = hand->m_Next;
                delete hand;
            }
            break;
        }
        parent = &(hand->m_Next);
    }
}

// ----------------------------------------------------------------------------
//
// Connects to the device's web-server to access the specified page.
//
DWORD
HttpHandle_t::
Connect(
    const HttpRequest_t &Request,
    HINTERNET           *pResource)
{
    DWORD result;
    LogDebug(TEXT("[AC] Connect to %s for page %ls"),
             GetServerHost(), Request.GetPageName());

    // Open the internet.
    if (NULL == m_OpenHandle)
    {
        static const TCHAR *IEUserAgent =
             TEXT("Mozilla/4.0 (compatible; MSIE 6.0b; Windows NT 5.1; .NET)");
        m_OpenHandle = InternetOpen(IEUserAgent,
                                    INTERNET_OPEN_TYPE_DIRECT,
                                    NULL,    // proxy name
                                    NULL,    // proxy bypass
                                    0);      // flags
        if (NULL == m_OpenHandle)
        {
            result = GetLastError();
            LogError(TEXT("Can't open HTTP session: %s"),
                     Win32ErrorText(result));
            return result;
        }
        LogDebug(TEXT("[AC] Opened internet handle 0x%X"),
                (unsigned)m_OpenHandle);
    }

    // Connect to the network.
    if (NULL == m_ConnectHandle)
    {
        m_ConnectHandle = InternetConnect(m_OpenHandle,
                                          GetServerHost(),
                                          INTERNET_DEFAULT_HTTP_PORT,
                                          GetUserName(),
                                          GetPassword(),
                                          INTERNET_SERVICE_HTTP,
                                          0,     // flags
                                          NULL); // context
        if (NULL == m_ConnectHandle)
        {
            Disconnect();
            result = GetLastError();
            LogError(TEXT("Can't connect to \"%s\": %s"),
                     GetServerHost(), Win32ErrorText(result));
            return result;
        }
        LogDebug(TEXT("[AC] Connected to server at %s"), GetServerHost());
    }

    // Connect to the page.
    ce::tstring referer;
    referer.assign(TEXT("http://"));
    referer.append(GetServerHost());
    
   *pResource = HttpOpenRequest(m_ConnectHandle,
                                Request.GetConnectMethod(),
                                Request.GetPageURL(),
                                NULL,            // protocol version - HTTP/1.1
                                referer,
                                NULL,            // accept only "text/*"
                                INTERNET_FLAG_KEEP_CONNECTION
                              | INTERNET_FLAG_NO_CACHE_WRITE
                              | INTERNET_FLAG_NO_UI
                              | INTERNET_FLAG_PRAGMA_NOCACHE,
                                NULL);        // context
    if (!(*pResource))
    {
        Disconnect();
        result = GetLastError();
        LogError(TEXT("Can't open request \"%ls\" at %s: %s"),
                 Request.GetPageName(), GetServerHost(), 
                 Win32ErrorText(result));
        return result;
    }

    LogDebug(TEXT("[AC] Connected to URL \"%ls\""), Request.GetPageURL());
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Disconnects the current connection to cause a reconnection next time.
//
void
HttpHandle_t::
Disconnect(void)
{
    if (m_ConnectHandle)
    {
        LogDebug(TEXT("[AC] Disconnecting server %s"), 
                 GetServerHost());
        InternetCloseHandle(m_ConnectHandle);
        m_ConnectHandle = NULL;
    }
    if (m_OpenHandle)
    {
        LogDebug(TEXT("[AC] Closing internet handle 0x%X"), 
                (unsigned)m_OpenHandle);
        InternetCloseHandle(m_OpenHandle);
        m_OpenHandle = NULL;
    }
}

#if defined(EXTRA_EXTRA_DEBUG)
// ----------------------------------------------------------------------------
//
// Logs the specified web-page contents.
//
static void
LogWebPage(const TCHAR *pFormat, ...)
{
    va_list va;
    va_start(va, pFormat);
    WiFUtils::LogMessageV(PT_LOG, true, pFormat, va);
    va_end(va);
}
#endif

// ----------------------------------------------------------------------------
//
// Translate an HTTP status code to text.
//

static const WiFUtils::EnumMap s_HttpStatusCodes[] =
{
#ifdef           HTTP_STATUS_OK
    EnumMapEntry(HTTP_STATUS_OK,              TEXT("request completed")),
#endif
#ifdef           HTTP_STATUS_CREATED
    EnumMapEntry(HTTP_STATUS_CREATED,         TEXT("object created, reason = new URI")),
#endif
#ifdef           HTTP_STATUS_ACCEPTED
    EnumMapEntry(HTTP_STATUS_ACCEPTED,        TEXT("async completion (TBS)")),
#endif
#ifdef           HTTP_STATUS_PARTIAL
    EnumMapEntry(HTTP_STATUS_PARTIAL,         TEXT("partial completion")),
#endif
#ifdef           HTTP_STATUS_NO_CONTENT
    EnumMapEntry(HTTP_STATUS_NO_CONTENT,      TEXT("no info to return")),
#endif
#ifdef           HTTP_STATUS_AMBIGUOUS
    EnumMapEntry(HTTP_STATUS_AMBIGUOUS,       TEXT("server couldn't decide what to return")),
#endif
#ifdef           HTTP_STATUS_MOVED
    EnumMapEntry(HTTP_STATUS_MOVED,           TEXT("object permanently moved")),
#endif
#ifdef           HTTP_STATUS_REDIRECT
    EnumMapEntry(HTTP_STATUS_REDIRECT,        TEXT("object temporarily moved")),
#endif
#ifdef           HTTP_STATUS_REDIRECT_METHOD
    EnumMapEntry(HTTP_STATUS_REDIRECT_METHOD, TEXT("redirection w/ new access method")),
#endif
#ifdef           HTTP_STATUS_NOT_MODIFIED
    EnumMapEntry(HTTP_STATUS_NOT_MODIFIED,    TEXT("if-modified-since was not modified")),
#endif
#ifdef           HTTP_STATUS_BAD_REQUEST
    EnumMapEntry(HTTP_STATUS_BAD_REQUEST,     TEXT("invalid syntax")),
#endif
#ifdef           HTTP_STATUS_DENIED
    EnumMapEntry(HTTP_STATUS_DENIED,          TEXT("access denied")),
#endif
#ifdef           HTTP_STATUS_PAYMENT_REQ
    EnumMapEntry(HTTP_STATUS_PAYMENT_REQ,     TEXT("payment required")),
#endif
#ifdef           HTTP_STATUS_FORBIDDEN
    EnumMapEntry(HTTP_STATUS_FORBIDDEN,       TEXT("request forbidden")),
#endif
#ifdef           HTTP_STATUS_NOT_FOUND
    EnumMapEntry(HTTP_STATUS_NOT_FOUND,       TEXT("object not found")),
#endif
#ifdef           HTTP_STATUS_BAD_METHOD
    EnumMapEntry(HTTP_STATUS_BAD_METHOD,      TEXT("method is not allowed")),
#endif
#ifdef           HTTP_STATUS_NONE_ACCEPTABLE
    EnumMapEntry(HTTP_STATUS_NONE_ACCEPTABLE, TEXT("no response acceptable to client found")),
#endif
#ifdef           HTTP_STATUS_PROXY_AUTH_REQ
    EnumMapEntry(HTTP_STATUS_PROXY_AUTH_REQ,  TEXT("proxy authentication required")),
#endif
#ifdef           HTTP_STATUS_REQUEST_TIMEOUT
    EnumMapEntry(HTTP_STATUS_REQUEST_TIMEOUT, TEXT("server timed out waiting for request")),
#endif
#ifdef           HTTP_STATUS_CONFLICT
    EnumMapEntry(HTTP_STATUS_CONFLICT,        TEXT("user should resubmit with more info")),
#endif
#ifdef           HTTP_STATUS_GONE
    EnumMapEntry(HTTP_STATUS_GONE,            TEXT("the resource is no longer available")),
#endif
#ifdef           HTTP_STATUS_AUTH_REFUSED
    EnumMapEntry(HTTP_STATUS_AUTH_REFUSED,    TEXT("couldn't authorize client")),
#endif
#ifdef           HTTP_STATUS_SERVER_ERROR
    EnumMapEntry(HTTP_STATUS_SERVER_ERROR,    TEXT("internal server error")),
#endif
#ifdef           HTTP_STATUS_NOT_SUPPORTED
    EnumMapEntry(HTTP_STATUS_NOT_SUPPORTED,   TEXT("required not supported")),
#endif
#ifdef           HTTP_STATUS_BAD_GATEWAY
    EnumMapEntry(HTTP_STATUS_BAD_GATEWAY,     TEXT("error response received from gateway")),
#endif
#ifdef           HTTP_STATUS_SERVICE_UNAVAIL
    EnumMapEntry(HTTP_STATUS_SERVICE_UNAVAIL, TEXT("temporarily overloaded")),
#endif
#ifdef           HTTP_STATUS_GATEWAY_TIMEOUT
    EnumMapEntry(HTTP_STATUS_GATEWAY_TIMEOUT, TEXT("timed out waiting for gateway")),
#endif
};

static const TCHAR *
HttpStatusCode2Desc(
    DWORD StatusCode)
{
    return WiFUtils::EnumMapCode2Desc(s_HttpStatusCodes,
                              COUNTOF(s_HttpStatusCodes),
                                      StatusCode,
                                      TEXT("<unknown status code>"));
}

// ----------------------------------------------------------------------------
//
// Loads the specified web-page.
//
DWORD
HttpHandle_t::
SendPageRequest(
    const HttpRequest_t &Request,
    long                 SleepMillis, // wait time before reading response
    ce::string          *pResponse)   // response from device
{
    HRESULT hr;
    DWORD result, status;
    ce::auto_hinternet resourceHandle;
    LogDebug(TEXT("[AC] SendPageRequest(%ls) to %s"),
             Request.GetPageName(), GetServerHost());

    // Connect to the server.
    result = Connect(Request, &resourceHandle);
    if (ERROR_SUCCESS != result)
        return result;

    // Insert the cookies (if any).
    if (!m_Cookies.empty())
    {
        ce::tstring url(m_ServerHost); bool schemeAdded = false;
        for (ValueMapIter it = m_Cookies.begin() ; it != m_Cookies.end() ; ++it)
        {
            ce::tstring name;
            hr = WiFUtils::ConvertString(&name, it->first, "cookie name",
                                                it->first.length(), CP_UTF8);
            if (FAILED(hr))
                return HRESULT_CODE(hr);

            ce::tstring value;
            hr = WiFUtils::ConvertString(&value, it->second, "cookie value",
                                                 it->second.length(), CP_UTF8);
            if (FAILED(hr))
                return HRESULT_CODE(hr);

            while (!InternetSetCookie(url, name, value))
            {
                result = GetLastError();
                if (!schemeAdded && ERROR_INTERNET_UNRECOGNIZED_SCHEME == result)
                {
                    schemeAdded = true;
                    url.assign(TEXT("http://"));
                    url.append(m_ServerHost);
                    continue;
                }
                LogError(TEXT("Error sending cookie \"%s=%s\" to %s: %s"),
                         &name[0], &(it->second)[0], url,
                         Win32ErrorText(result));
                return result;
            }
        }
    }

    // Send the request.
    bool authenticated = false;
    for (;;)
    {
        result = SendRequest(resourceHandle, Request, &status);
        if (ERROR_SUCCESS != result)
            return result;
        if (HTTP_STATUS_OK == status)
            break;

        // Handle an authentication failure.
        if (!authenticated && status == HTTP_STATUS_DENIED)
        {
            LogDebug(TEXT("[AC] Resending user \"%s\" and passwd \"%s\""),
                     GetUserName(), GetPassword());

            InternetSetOption(resourceHandle, INTERNET_OPTION_USERNAME,
                      (void *)GetUserName(), m_UserName.length()+1);
            InternetSetOption(resourceHandle, INTERNET_OPTION_PASSWORD,
                      (void *)GetPassword(), m_Password.length()+1);

            authenticated = true;
        }
        else
        {
            LogError(TEXT("Got error from \"%ls\" at %s: status=%d (%s)"),
                     Request.GetPageURL(), GetServerHost(),
                     status, HttpStatusCode2Desc(status));
            return ERROR_OPEN_FAILED;
        }
    }

    // If necessary, wait a while before reading the response.
    if (0L < SleepMillis)
    {
        LogDebug(TEXT("[AC] Pausing %g seconds for device to respond"),
                (double)SleepMillis / 1000.0);
        Sleep(SleepMillis);
    }

    // Until we've read all of the response...
    pResponse->clear();
    ce::string readBuffer;
    for (;;)
    {
        // Repeat this in case the read returns INSUFFICIENT_BUFFER.
        DWORD bytesReceived;
        for (;;)
        {
            // Determine the size of the remaining response.
            DWORD bytesAvailable;
            if (!InternetQueryDataAvailable(resourceHandle,
                                            &bytesAvailable,
                                            0,      // flags
                                            NULL))  // context
            {
                result = GetLastError();
                LogError(TEXT("Can't get response-size from \"%ls\" at %s: %s"),
                         Request.GetPageName(), GetServerHost(),
                         Win32ErrorText(result));
                return result;
            }
            if (0 == bytesAvailable)
            {
                bytesReceived = 0;
                break;
            }

#if defined(EXTRA_EXTRA_DEBUG)
            LogDebug(TEXT("[AC] Said %u response bytes are available"),
                     bytesAvailable);
#endif

            // Allocate a read-buffer.
            if (!readBuffer.reserve(bytesAvailable + 1))
            {
                LogError(TEXT("Can't allocate %u bytes for read buffer"),
                         bytesAvailable + 1);
                return ERROR_OUTOFMEMORY;
            }

            // Read the response (if any).
            if (InternetReadFile(resourceHandle,
                                 readBuffer.get_buffer(),
                                 readBuffer.capacity() - 1,
                                &bytesReceived))
            {
                break;
            }

            result = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER != result)
            {
                LogError(TEXT("Can't read from \"%ls\" at %s: %s"),
                         Request.GetPageName(), GetServerHost(),
                         Win32ErrorText(result));
                return result;
            }
        }
#if defined(EXTRA_EXTRA_DEBUG)
        LogDebug(TEXT("[AC] Received %u response bytes"), bytesReceived);
#endif
        if (0 == bytesReceived)
        {
            break;
        }

        // Accumulate the output.
        if (!pResponse->append(readBuffer, bytesReceived))
        {
            LogError(TEXT("Can't allocate %u bytes for response buffer"),
                     pResponse->length() + bytesReceived);
            return ERROR_OUTOFMEMORY;
        }
    }

#if defined(EXTRA_EXTRA_DEBUG)
    LogDebug(TEXT("[AC] Response (length=%d):"), pResponse->length());
    LogWebPage(L"%hs", &(*pResponse)[0]);
#elif defined(EXTRA_DEBUG)
    LogDebug(TEXT("[AC] Response length = %d"),
             pResponse->length());
#endif

    return result;
}

// ----------------------------------------------------------------------------
//
// Sends the specified request to the specified resource.
//
DWORD
HttpHandle_t::
SendRequest(
    HINTERNET            Resource,    // connected HTTP resource
    const HttpRequest_t &Request,     // page to be sent
    DWORD               *pStatus)     // HTTP request status code
{
    DWORD result;
    LogDebug(TEXT("[AC] Sending request to %s"), GetServerHost());

    // Send the request.
    if (!HttpSendRequest(Resource,
                         NULL,   // additional headers
                         0,      // chars in headers
                 (void *)Request.GetPostInfo(),
                         Request.GetPostInfoLength()))
    {
        result = GetLastError();
        LogError(TEXT("Can't send request \"%ls\" to %s: %s"),
                 Request.GetPageName(), GetServerHost(), 
                 Win32ErrorText(result));
        return result;
    }

    // Get the status-response.
    DWORD statusSize = sizeof(*pStatus);
    if (!HttpQueryInfo(Resource,
                       HTTP_QUERY_STATUS_CODE
                     | HTTP_QUERY_FLAG_NUMBER,
                       pStatus,
                      &statusSize, NULL))
    {
        result = GetLastError();
        LogError(TEXT("Can't get status code from \"%ls\" at %s: %s"),
                 Request.GetPageName(), GetServerHost(), 
                 Win32ErrorText(result));
        return result;
    }

    LogDebug(TEXT("[AC]   send succeeded w/ status=%d (%s)"),
            *pStatus,
             HttpStatusCode2Desc(*pStatus));
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the specified cookie.
//
const WCHAR *
HttpHandle_t::
GetCookie(const char *pName) const
{
    ValueMapCIter it = const_cast<HttpHandle_t *>(this)->m_Cookies.find(pName);
    return (it == m_Cookies.end())? NULL : &(it->second)[0];
}

DWORD
HttpHandle_t::
SetCookie(const char *pName, const ce::wstring &Value)
{
    ValueMapIter it = m_Cookies.insert(ce::string(pName), ce::wstring(Value));
    return (it == m_Cookies.end())? ERROR_OUTOFMEMORY : ERROR_SUCCESS;
}

/* =============================== HttpPort ================================ */

// ----------------------------------------------------------------------------
//
// Constructor.
//
HttpPort_t::
HttpPort_t(const TCHAR *pServerHost)
    : m_pHandle(HttpHandle_t::AttachHandle(pServerHost))
{
    assert(IsValid());
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
HttpPort_t::
~HttpPort_t(void)
{
    if (NULL != m_pHandle)
    {
        HttpHandle_t::DetachHandle(m_pHandle);
        m_pHandle = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Returns true if the port handle is valid.
//
bool
HttpPort_t::
IsValid(void) const
{
    return NULL != m_pHandle;
}

// ----------------------------------------------------------------------------
//
// Gets the web-server name or address.
//
const TCHAR *
HttpPort_t::
GetServerHost(void) const
{
    return (NULL == m_pHandle)? TEXT("") : m_pHandle->GetServerHost();
}

// ----------------------------------------------------------------------------
//
// Gets or sets the user-name.
//
const TCHAR *
HttpPort_t::
GetUserName(void) const
{
    return (NULL == m_pHandle)? TEXT("") : m_pHandle->GetUserName();
}
void
HttpPort_t::
SetUserName(const TCHAR *Value)
{
    if (NULL != m_pHandle && 0 != _tcscmp(Value, m_pHandle->GetUserName()))
    {
        m_pHandle->Disconnect();
        m_pHandle->SetUserName(Value);
    }
}

// ----------------------------------------------------------------------------
//
// Gets or sets the admin password.
//
const TCHAR *
HttpPort_t::
GetPassword(void) const
{
    return (NULL == m_pHandle)? TEXT("") : m_pHandle->GetPassword();
}
void
HttpPort_t::
SetPassword(const TCHAR *Value)
{
    if (NULL != m_pHandle && 0 != _tcscmp(Value, m_pHandle->GetPassword()))
    {
        m_pHandle->Disconnect();
        m_pHandle->SetPassword(Value);
    }
}

// ----------------------------------------------------------------------------
//
// Retrieves an object which can be locked to prevent other threads
// from using the port.
//
ce::critical_section &
HttpPort_t::
GetLocker(void) const
{
    return (NULL == m_pHandle)? HttpHandlesLocker : m_pHandle->GetLocker();
}

// ----------------------------------------------------------------------------
//
// Disconnects the current connection to cause a reconnection next time.
//
void
HttpPort_t::
Disconnect(void)
{
    if (NULL != m_pHandle)
    {
        m_pHandle->Disconnect();
    }
}

// ----------------------------------------------------------------------------
//
// Loads the specified web-page.
//
DWORD
HttpPort_t::
SendPageRequest(
    const WCHAR *pPageName,   // page to be loaded
    long         SleepMillis, // wait time before reading response
    ce::string  *pResponse)   // response from device
{
    if (NULL == m_pHandle)
        return ERROR_OUTOFMEMORY;

    return m_pHandle->SendPageRequest(pPageName, SleepMillis, pResponse);
}

// ----------------------------------------------------------------------------
//
// Sends a configuration-update command for the specified web-page.
//
DWORD
HttpPort_t::
SendUpdateRequest(
    const WCHAR    *pPageName,   // page being updated
    const WCHAR    *pMethod,     // update method (GET or POST)
    const WCHAR    *pAction,     // action to be performed
    const ValueMap &Parameters,  // modified form parameters
    long            SleepMillis, // wait time before reading response
    ce::string     *pResponse)   // response from device
{
    if (NULL == m_pHandle)
        return ERROR_OUTOFMEMORY;

    // Canonize the web-page paramters.
    HttpRequest_t request(pMethod, pAction);
    DWORD result = request.Canonize(Parameters);
    if (ERROR_SUCCESS != result)
        return result;

    // Perform the action and get the response.
    return m_pHandle->SendPageRequest(request, SleepMillis, pResponse);
}

// ----------------------------------------------------------------------------
//
// Sends a configuration-update command for the specified web-page
// and checks the response.
//
DWORD
HttpPort_t::
SendUpdateRequest(
    const WCHAR    *pPageName,   // page being updated
    const WCHAR    *pMethod,     // update method (GET or POST)
    const WCHAR    *pAction,     // action to be performed
    const ValueMap &Parameters,  // modified form parameters
    long            SleepMillis, // wait time before reading response
    ce::string     *pResponse,   // response from device
    const char     *pExpected)   // expected response
{
    // Perform the action and get the response.
    DWORD result = SendUpdateRequest(pPageName, pMethod, pAction, Parameters,
                                     SleepMillis, pResponse);
    if (ERROR_SUCCESS != result)
        return result;

    // Check the response.
    const char *pCursor = pResponse->get_buffer();
    if (strstr(pCursor, pExpected))
        return ERROR_SUCCESS;

    // Describe the error.
    char errorBuffer[1024];
    enum { printSize = sizeof(errorBuffer) - 100 };
    const char *pBody = NULL;
    while (NULL != (pCursor = strstr(pCursor, "<body")))
    {
        pBody = pCursor;
        pCursor += 5;
    }
    if (NULL == pBody)
    {
        strncpy_s(errorBuffer, _countof(errorBuffer),"no response from device", printSize);
    }
    else
    {
        const char *pWebPage = &(*pResponse)[0];
        int errorSize = &pWebPage[pResponse->length()] - pBody;
        if (errorSize <= 0)
        {
            pBody = pWebPage;
            errorSize = pResponse->length();
        }
        if (printSize >= errorSize)
        {
            strncpy_s(errorBuffer, _countof(errorBuffer),pBody, errorSize);
            errorBuffer[errorSize] = '\0';
        }
        else
        {
            strncpy_s(errorBuffer, _countof(errorBuffer),pBody, printSize);
            HRESULT hr = StringCchPrintfA(&errorBuffer[printSize],
                                          sizeof(errorBuffer) - printSize,
                                          " ... (%d more)",
                                          errorSize - printSize);
            if (FAILED(hr))
            {
                errorBuffer[printSize] = '\0';
            }
        }
    }

    errorBuffer[sizeof(errorBuffer) - 1] = '\0';
    LogError(TEXT("Page %s update failed:\n%hs"), pPageName, errorBuffer);

    LogWarn(TEXT("Page %s update failed:\n%hs"), pPageName, errorBuffer);
    LogWarn(TEXT("Form Information:"));
    LogWarn(TEXT("  method = %s"), pMethod);
    LogWarn(TEXT("  action = %s"), pAction);
    LogWarn(TEXT("  parameters:"));
    for (ValueMapCIter it = Parameters.begin() ; it != Parameters.end() ; ++it)
    {
        LogWarn(TEXT("    %hs = \"%ls\""), &it->first[0], &it->second[0]);
    }

    return ERROR_INVALID_PARAMETER;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the specified cookie.
//
const WCHAR *
HttpPort_t::
GetCookie(const char *pName) const
{
    return (NULL == m_pHandle)? NULL : m_pHandle->GetCookie(pName);
}

DWORD
HttpPort_t::
SetCookie(const char *pName, const char *pValue)
{
    if (NULL == m_pHandle)
        return ERROR_OUTOFMEMORY;

    ce::wstring value;
    HRESULT hr = WiFUtils::ConvertString(&value, pValue,
                                         "cookie value", -1, CP_UTF8);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    return m_pHandle->SetCookie(pName, value);
}

DWORD
HttpPort_t::
SetCookie(const char *pName, const WCHAR *pValue)
{
    return (NULL == m_pHandle)? ERROR_OUTOFMEMORY
                              : m_pHandle->SetCookie(pName, pValue);
}

// ----------------------------------------------------------------------------
