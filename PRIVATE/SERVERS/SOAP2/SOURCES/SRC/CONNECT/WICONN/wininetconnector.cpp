//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      WinInetConnector.cpp
//
// Contents:
//
//      CWinInetConnector class implementation
//
//-----------------------------------------------------------------------------

#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
// static member initialization
////////////////////////////////////////////////////////////////////////////////////////////////////

const WCHAR CWinInetConnector::s_szSoapAction[]    = L"SOAPAction: ";
const WCHAR CWinInetConnector::s_szNewLine[]       = L"\r\n";
const WCHAR CWinInetConnector::s_Quote             = '"';

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Error Map
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define WEE(wi, hr) { HRESULT_FROM_WIN32(wi), hr , __LINE__ },
#else
#define WEE(wi, hr) { HRESULT_FROM_WIN32(wi), hr },
#endif

static HResultMapEntry g_WinInetHResultMapEntries[] = 
{
    WEE(ERROR_INTERNET_OUT_OF_HANDLES,                    CONN_E_HTTP_OUTOFMEMORY)
    WEE(ERROR_INTERNET_TIMEOUT,                           CONN_E_HTTP_TIMEOUT)
    WEE(ERROR_INTERNET_EXTENDED_ERROR,                    CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INTERNAL_ERROR,                    CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INVALID_URL,                       CONN_E_HTTP_BAD_URL)
    WEE(ERROR_INTERNET_UNRECOGNIZED_SCHEME,               CONN_E_HTTP_BAD_URL)
    WEE(ERROR_INTERNET_NAME_NOT_RESOLVED,                 CONN_E_HTTP_HOST_NOT_FOUND)
    WEE(ERROR_INTERNET_PROTOCOL_NOT_FOUND,                CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INVALID_OPTION,                    CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_BAD_OPTION_LENGTH,                 CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_OPTION_NOT_SETTABLE,               CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_SHUTDOWN,                          CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INCORRECT_USER_NAME,               CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INCORRECT_PASSWORD,                CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_LOGIN_FAILURE,                     CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INVALID_OPERATION,                 CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_OPERATION_CANCELLED,               CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INCORRECT_HANDLE_TYPE,             CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INCORRECT_HANDLE_STATE,            CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_NOT_PROXY_REQUEST,                 CONN_E_HTTP_CANNOT_USE_PROXY)
    WEE(ERROR_INTERNET_REGISTRY_VALUE_NOT_FOUND,          CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_BAD_REGISTRY_PARAMETER,            CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_NO_DIRECT_ACCESS,                  CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_NO_CONTEXT,                        CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_NO_CALLBACK,                       CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_REQUEST_PENDING,                   CONN_E_HTTP_OVERLOADED)
    WEE(ERROR_INTERNET_INCORRECT_FORMAT,                  CONN_E_HTTP_BAD_REQUEST)
    WEE(ERROR_INTERNET_ITEM_NOT_FOUND,                    CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_CANNOT_CONNECT,                    CONN_E_HTTP_CONNECT_FAILED)
    WEE(ERROR_INTERNET_CONNECTION_ABORTED,                CONN_E_HTTP_CONNECT_FAILED)
    WEE(ERROR_INTERNET_CONNECTION_RESET,                  CONN_E_HTTP_CONNECT_FAILED)
    WEE(ERROR_INTERNET_FORCE_RETRY,                       CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INVALID_PROXY_REQUEST,             CONN_E_HTTP_BAD_REQUEST)
    WEE(ERROR_INTERNET_NEED_UI,                           CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_HANDLE_EXISTS,                     CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_SEC_CERT_DATE_INVALID,             CONN_E_HTTP_BAD_CERTIFICATE)
    WEE(ERROR_INTERNET_SEC_CERT_CN_INVALID,               CONN_E_HTTP_BAD_CERTIFICATE)
    WEE(ERROR_INTERNET_HTTP_TO_HTTPS_ON_REDIR,            CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_HTTPS_TO_HTTP_ON_REDIR,            CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_MIXED_SECURITY,                    CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_CHG_POST_IS_NON_SECURE,            CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_POST_IS_NON_SECURE,                CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED,           CONN_E_HTTP_BAD_CERTIFICATE)
    WEE(ERROR_INTERNET_INVALID_CA,                        CONN_E_HTTP_BAD_CERT_AUTHORITY)
    WEE(ERROR_INTERNET_CLIENT_AUTH_NOT_SETUP,             CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_ASYNC_THREAD_FAILED,               CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_REDIRECT_SCHEME_CHANGE,            CONN_E_HTTP_UNSPECIFIED)
#ifndef UNDER_CE
    WEE(ERROR_INTERNET_DIALOG_PENDING,                    CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_RETRY_DIALOG,                      CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_HTTPS_HTTP_SUBMIT_REDIR,           CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_INSERT_CDROM,                      CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_FORTEZZA_LOGIN_NEEDED,             CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_SEC_CERT_ERRORS,                   CONN_E_HTTP_BAD_CERTIFICATE)
    WEE(ERROR_INTERNET_SEC_CERT_NO_REV,                   CONN_E_HTTP_BAD_CERTIFICATE)
    WEE(ERROR_INTERNET_SEC_CERT_REV_FAILED,               CONN_E_HTTP_BAD_CERTIFICATE)
#endif 

    WEE(ERROR_HTTP_HEADER_NOT_FOUND,                      CONN_E_HTTP_HOST_NOT_FOUND)
    WEE(ERROR_HTTP_DOWNLEVEL_SERVER,                      CONN_E_HTTP_BAD_RESPONSE)
    WEE(ERROR_HTTP_INVALID_SERVER_RESPONSE,               CONN_E_HTTP_BAD_RESPONSE)
    WEE(ERROR_HTTP_INVALID_HEADER,                        CONN_E_HTTP_PARSE_RESPONSE)
    WEE(ERROR_HTTP_INVALID_QUERY_REQUEST,                 CONN_E_HTTP_BAD_REQUEST)
    WEE(ERROR_HTTP_HEADER_ALREADY_EXISTS,                 CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_HTTP_REDIRECT_FAILED,                       CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_SECURITY_CHANNEL_ERROR,            CONN_E_HTTP_SSL_ERROR)
    WEE(ERROR_INTERNET_UNABLE_TO_CACHE_FILE,              CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_TCPIP_NOT_INSTALLED,               CONN_E_HTTP_CONNECT_FAILED)

    WEE(ERROR_HTTP_NOT_REDIRECTED,                        CONN_E_HTTP_UNSPECIFIED)
#ifndef UNDER_CE
    WEE(ERROR_HTTP_COOKIE_NEEDS_CONFIRMATION,             CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_HTTP_COOKIE_DECLINED,                       CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_DISCONNECTED,                      CONN_E_HTTP_CONNECT_FAILED)
    WEE(ERROR_INTERNET_SERVER_UNREACHABLE,                CONN_E_HTTP_CONNECT_FAILED)
    WEE(ERROR_INTERNET_PROXY_SERVER_UNREACHABLE,          CONN_E_HTTP_CONNECT_FAILED)
    WEE(ERROR_INTERNET_BAD_AUTO_PROXY_SCRIPT,             CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_UNABLE_TO_DOWNLOAD_SCRIPT,         CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_HTTP_REDIRECT_NEEDS_CONFIRMATION,           CONN_E_HTTP_UNSPECIFIED)
    WEE(ERROR_INTERNET_SEC_INVALID_CERT,                  CONN_E_HTTP_BAD_CERTIFICATE)
    WEE(ERROR_INTERNET_SEC_CERT_REVOKED,                  CONN_E_HTTP_BAD_CERTIFICATE)
    WEE(ERROR_INTERNET_FAILED_DUETOSECURITYCHECK,         CONN_E_ACCESS_DENIED)
    WEE(ERROR_INTERNET_NOT_INITIALIZED,                   CONN_E_HTTP_CONNECT_FAILED)
    WEE(ERROR_INTERNET_NEED_MSN_SSPI_PKG,                 CONN_E_HTTP_UNSPECIFIED)

    WEE(ERROR_INTERNET_LOGIN_FAILURE_DISPLAY_ENTITY_BODY, CONN_E_HTTP_UNSPECIFIED)
#else
    WEE(0,                                                S_OK)
#endif 
};

static HResultMap g_WinInetHResultMap =
{
    countof(g_WinInetHResultMapEntries),
    g_WinInetHResultMapEntries,
    CONN_E_HTTP_UNSPECIFIED
#ifdef _DEBUG
    , __FILE__
#endif
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  Interface Map
////////////////////////////////////////////////////////////////////////////////////////////////////

TYPEINFOIDSFORCLASS(CHttpConnBase, ISoapConnector, MSSOAPLib)

BEGIN_INTERFACE_MAP(CWinInetConnector)
    ADD_IUNKNOWN(CWinInetConnector, ISoapConnector)
    ADD_INTERFACE(CWinInetConnector, ISoapConnector)
    ADD_INTERFACE(CWinInetConnector, IDispatch)
END_INTERFACE_MAP(CWinInetConnector)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWinInetConnector::CWinInetConnector
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CWinInetConnector::CWinInetConnector()
: m_hInternet(0),
#ifndef UNDER_CE
  m_hConnect(0),
  m_hRequest(0),
#endif 
  m_pRequest(0),
  m_pResponse(0)
{
#ifdef UNDER_CE
    m_hConnect = 0;
    m_hRequest = 0;
#endif /* UNDER_CE */


#ifdef _DEBUG
    m_dwTimeout = 0x1FFFFFFF;       // is int, not DWORD in WinInet
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWinInetConnector::~CWinInetConnector
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CWinInetConnector::~CWinInetConnector()
{
    ::ReleaseInterface(m_pRequest);
    ::ReleaseInterface(m_pResponse);

    CWinInetConnector::ShutdownConnection();
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetConnector::get_InputStream(IStream **pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetConnector::get_InputStream(IStream **pVal)
{
    if (! pVal)
        return E_INVALIDARG;

    HRESULT hr = S_OK;

    *pVal = 0;

    if (! m_pRequest)
    {
        m_pRequest = new CSoapOwnedObject<CWinInetRequestStream>(INITIAL_REFERENCE);

        if (! m_pRequest)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }

        hr = m_pRequest->set_Owner(this);
    }

    m_pRequest->AddRef();
    *pVal = m_pRequest;

Cleanup:
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetConnector::get_OutputStream(IStream **pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetConnector::get_OutputStream(IStream **pVal)
{
    if (! pVal)
        return E_INVALIDARG;

    HRESULT hr = S_OK;

    *pVal = 0;

    if (! m_pResponse)
    {
        m_pResponse = new CSoapOwnedObject<CWinInetResponseStream>(INITIAL_REFERENCE);

        if (! m_pResponse)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }

        hr = m_pResponse->set_Owner(this);
    }

    m_pResponse->AddRef();
    *pVal = m_pResponse;

Cleanup:
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetConnector::EstablishConnection()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetConnector::EstablishConnection()
{
    HINTERNET           hInternet   = 0;
#ifndef UNDER_CE
    HINTERNET           hConnect    = 0;
#endif 
    HRESULT             hr          = S_OK;
    HINTERNET           hTemp = NULL;

    BSTR                pProxyUrl   = 0;
#ifndef UNDER_CE    
    INTERNET_PROXY_INFO proxy;
#endif 

#ifdef DEBUG
    DWORD dbg_dwError;
    WCHAR dbg_pszError[1000];
    DWORD dbg_bufLen = 1000;
#endif
    
    if (!m_Url.HostName())
    {
        hr = E_INVALIDARG;
        goto Cleanup;
    }

    CHK(GetProxyURL(&pProxyUrl));

    if (m_bUseProxy)
    {
#ifndef UNDER_CE
        if (pProxyUrl)
            hInternet = ::InternetOpenW(0, INTERNET_OPEN_TYPE_PROXY, pProxyUrl, 0, 0);
        else
            hInternet = ::InternetOpenW(0, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0);
#else
        if (pProxyUrl)
            hInternet = ::InternetOpenW(L"", INTERNET_OPEN_TYPE_PROXY, pProxyUrl, 0, INTERNET_FLAG_ASYNC);
        else
            hInternet = ::InternetOpenW(L"", INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, INTERNET_FLAG_ASYNC);
#endif 
    }
    else
    {
#ifndef UNDER_CE
        hInternet = ::InternetOpenW(0, INTERNET_OPEN_TYPE_DIRECT, 0, 0, 0);
#else
        hInternet = ::InternetOpenW(L"", INTERNET_OPEN_TYPE_DIRECT, 0, 0, INTERNET_FLAG_ASYNC);
#endif 
    }
    CHK_BOOL(hInternet != 0, MAP_WININET_ERROR());
    TRACE(("InternetOpen ==> hInternet == (0x%08X)\n", hInternet));

#ifndef UNDER_CE
    hConnect = ::InternetConnectW(hInternet, m_Url.HostName(), m_Url.Port(), 0, 0, INTERNET_SERVICE_HTTP, 0, 0);
    CHK_BOOL(hConnect, MAP_WININET_ERROR());
    TRACE(("InternetConnect ==> hConnect == (0x%08X)\n", hConnect));
#else
    hTemp = ::InternetConnectW(hInternet, m_Url.HostName(), m_Url.Port(), 0, 0, INTERNET_SERVICE_HTTP, 0, reinterpret_cast<DWORD>(&m_hConnect)); 
#if DEBUG

    if(NULL == hTemp && InternetGetLastResponseInfo(&dbg_dwError, dbg_pszError, &dbg_bufLen))
    {
        OutputDebugString(L"ERROR!! -- InternetConnectW FAILED!");
        OutputDebugString(dbg_pszError);
    }
#endif
    CHK_BOOL(hTemp, MAP_WININET_ERROR());
    m_hConnect = hTemp;
    TRACE(("InternetConnect ==> hConnect == (0x%08X)\n", m_hConnect));
#endif 

    m_hInternet = hInternet; hInternet = 0;
#ifndef UNDER_CE
    m_hConnect  = hConnect;  hConnect  = 0;
#endif 

    ASSERT(m_hRequest == 0);
    m_hRequest  = 0;

Cleanup:
    if (hInternet)
    {
        ::InternetCloseHandle(hInternet);
    }
#ifndef UNDER_CE
    if (hConnect)
    {
        ::InternetCloseHandle(hInternet);
    }
    ::SysFreeString(pProxyUrl);
#endif 
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetConnector::ShutdownConnection()
//
//  parameters:
//          
//  description:
//          Shuts the connection down
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetConnector::ShutdownConnection()
{
    if (m_hRequest)
    {
#ifndef UNDER_CE // dont call close handle!  when we set to 0 the auto template magic will fix it
        ::InternetCloseHandle(m_hRequest);
#endif 
        m_hRequest = 0;
    }

    if (m_hConnect)
    {
#ifndef HTTP_LITE
        ::InternetSetOption(m_hConnect, INTERNET_OPTION_END_BROWSER_SESSION, 0, 0);
#endif

#ifndef UNDER_CE // dont call close handle!  when we set to 0 the auto template magic will fix it
        ::InternetCloseHandle(m_hConnect);
#endif 
        m_hConnect = 0;
    }

    if (m_hInternet)
    {
        ::InternetCloseHandle(m_hInternet);
        m_hInternet = 0;
    }

    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetConnector::SendRequest()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetConnector::SendRequest()
{
    if (! m_pRequest)
    {
        return E_INVALIDARG;
    }

    HRESULT         hr           = S_OK;
    DWORD           dwFlags      = 0;
    LPSTR           pProxyUser   = 0;
    LPSTR           pProxyPwd    = 0;
    LPSTR           pUserName    = 0;
    LPSTR           pPassword    = 0;
    PCCERT_CONTEXT  pCertContext = 0;

#ifndef UNDER_CE
    dwFlags    = (m_Url.Scheme() == INTERNET_SCHEME_HTTPS || m_bUseSSL) ?
                     INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION:
                     INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION;
#else                    
#ifndef HTTP_LITE
    dwFlags    = (m_Url.Scheme() == INTERNET_SCHEME_HTTPS || m_bUseSSL) ?
                     INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION:
                     INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION;
#else
    dwFlags    = (m_Url.Scheme() == INTERNET_SCHEME_HTTPS || m_bUseSSL) ?
                     INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE :
                     INTERNET_FLAG_NO_CACHE_WRITE;
#endif
#endif

    
#ifndef UNDER_CE
    m_hRequest = ::HttpOpenRequest(m_hConnect, L"POST", m_Url.ExtraPath(), L"HTTP/1.0", 0, 0, dwFlags, 0);
    CHK_BOOL(m_hRequest, MAP_WININET_ERROR());
#else
    HINTERNET hTemp;
    hTemp = ::HttpOpenRequest(m_hConnect, L"POST", m_Url.ExtraPath(), L"HTTP/1.0", 0, 0, dwFlags, reinterpret_cast<DWORD>(&m_hRequest));
    
#ifdef DEBUG
        if(hTemp == NULL)
        {
            DWORD dwError = GetLastError();
            WCHAR msg[100];
            swprintf(msg, L"HttpOpenRequestA failed! error:%x\n", dwError);
            OutputDebugString(msg);
        }
#endif
    CHK_BOOL(hTemp, MAP_WININET_ERROR());

    m_hRequest = hTemp;
#endif

    
    CHK_BOOL(::InternetSetOption(m_hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &m_dwTimeout, sizeof(m_dwTimeout)) == TRUE,
          MAP_WININET_ERROR());

    __try
    {
        LPWSTR pTheUserName = m_bstrAuthUser     ? m_bstrAuthUser     : m_Url.UserName();
        LPWSTR pThePassword = m_bstrAuthPassword ? m_bstrAuthPassword : m_Url.Password();

        if (pTheUserName)        {  CONVERT_WIDECHAR_TO_MULTIBYTE(pTheUserName, pUserName);        }
        if (pThePassword)        {  CONVERT_WIDECHAR_TO_MULTIBYTE(pThePassword, pPassword);        }
        if (m_bstrProxyUser)     {  CONVERT_WIDECHAR_TO_MULTIBYTE(m_bstrProxyUser, pProxyUser);    }
        if (m_bstrProxyPassword) {  CONVERT_WIDECHAR_TO_MULTIBYTE(m_bstrProxyPassword, pProxyPwd); }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    if (pUserName)
    {
        CHK_BOOL(::InternetSetOptionA(m_hRequest, INTERNET_OPTION_USERNAME, pUserName, strlen(pUserName)) == TRUE,
              MAP_WININET_ERROR());
    }
    if (pPassword)
    {
        CHK_BOOL(::InternetSetOptionA(m_hRequest, INTERNET_OPTION_PASSWORD, pPassword, strlen(pPassword)) == TRUE,
              MAP_WININET_ERROR());
    }
#ifndef HTTP_LITE    
    if (pProxyUser)
    {
        CHK_BOOL(::InternetSetOptionA(m_hRequest, INTERNET_OPTION_PROXY_USERNAME, pProxyUser, strlen(pProxyUser)) == TRUE,
              MAP_WININET_ERROR());
    }
    if (pProxyPwd)
    {
        CHK_BOOL(::InternetSetOptionA(m_hRequest, INTERNET_OPTION_PROXY_PASSWORD, pProxyPwd, strlen(pProxyPwd)) == TRUE,
              MAP_WININET_ERROR());
    }
#endif

    TRACE(("HttpOpenRequest ==> hRequest == (0x%08X)\n", m_hRequest));

    CHK(AddHeaders());

    hr = m_pRequest->Send(m_hRequest);

#ifndef HTTP_LITE
    if (hr == HRESULT_FROM_WIN32(ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED))
    {
        CHK(FindCertificate(m_bstrSSLClientCertificateName, &pCertContext));
        if (pCertContext)
        {
            CHK_BOOL(::InternetSetOption(m_hRequest, INTERNET_OPTION_CLIENT_CERT_CONTEXT, const_cast<PCERT_CONTEXT>(pCertContext), sizeof(CERT_CONTEXT)) == TRUE,
                  MAP_WININET_ERROR());
            CHK(m_pRequest->Send(m_hRequest));
        }
        else
        {
            hr = MAP_WININET_ERROR_(HRESULT_FROM_WIN32(ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED));
            goto Cleanup;
        }
    }
#endif

    CHK(m_pRequest->EmptyStream());

Cleanup:
    if (pCertContext)
    {
        ::CertFreeCertificateContext(pCertContext);
    }

    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetConnector::ReceiveResponse()
//
//  parameters:
//          
//  description:
//          Receives the response and checks HTTP status and content-type
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetConnector::ReceiveResponse()
{
    HRESULT hr = S_OK;
    DWORD dwStatus = 0;
    DWORD dwSize   = sizeof(DWORD);

    if (::HttpQueryInfo(m_hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwStatus, &dwSize, 0) != TRUE)
    {
        hr = CONN_E_CONNECTION_ERROR;
        goto Cleanup;
    }

    TRACE(("Http Status == %i\n", dwStatus));

    hr = ::HttpStatusToHresult(dwStatus);
    if (SUCCEEDED(hr) || hr == CONN_E_SERVER_ERROR)
    {
        char contentType[1024];
        dwSize = sizeof(contentType);

        if (::HttpQueryInfoA(m_hRequest, HTTP_QUERY_CONTENT_TYPE, contentType, &dwSize, 0) != TRUE)
        {
            hr = CONN_E_BAD_CONTENT;
            goto Cleanup;
        }
        CHK(::HttpContentTypeToHresult(contentType));

        hr = S_OK;
    }

    TRACE(("Http response status (in connector) == %i\n", dwStatus));

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetConnector::EmptyBuffers()
//
//  parameters:
//          
//  description:
//          Empties the buffers maintained by the connector
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetConnector::EmptyBuffers()
{
    if (m_pRequest)
    {
        m_pRequest->EmptyStream();
    }
    if (m_hRequest)
    {
#ifndef UNDER_CE // dont call close handle!  when we set to 0 the auto template magic will fix it
        ::InternetCloseHandle(m_hRequest);
#endif 
        m_hRequest = 0;
    }

    return S_OK;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWinInetConnector::AddHeaders()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWinInetConnector::AddHeaders()
{
    HRESULT hr    = S_OK;
    BSTR    escSA = 0;
#ifdef UNDER_CE
    WCHAR wcContentType[1024];
    WCHAR *pwcContentType = wcContentType;       
#endif


    CHK(EscapeUrl(m_bstrSoapAction, &escSA));

    if (escSA)
    {
        WCHAR *soapAction = 0;
        BYTE *pSA         = 0;

        size_t saSize     = wcslen(escSA) * sizeof(WCHAR);
        size_t sa         = saSize + char_array_size(s_szSoapAction) + char_array_size(s_szNewLine) +
                            sizeof(WCHAR) + sizeof(WCHAR) + sizeof(WCHAR);
#ifndef CE_NO_EXCEPTIONS
        try
#else
        __try
#endif 
        {
            soapAction = reinterpret_cast<WCHAR *>(_alloca(sa));
        }
#ifndef CE_NO_EXCEPTIONS
        catch (...)
#else
        __except(1)
#endif 
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }

        pSA = reinterpret_cast<BYTE *>(soapAction);

        memcpy(pSA, s_szSoapAction, char_array_size(s_szSoapAction));
        pSA += char_array_size(s_szSoapAction);
        memcpy(pSA, &s_Quote, sizeof(s_Quote));
        pSA += sizeof(s_Quote);
        memcpy(pSA, escSA, saSize);
        pSA += saSize;
        memcpy(pSA, &s_Quote, sizeof(s_Quote));
        pSA += sizeof(s_Quote);
        memcpy(pSA, s_szNewLine, sizeof(s_szNewLine));      // copies trailing zero

        CHK_BOOL(::HttpAddRequestHeaders(m_hRequest, soapAction, -1, HTTP_ADDREQ_FLAG_ADD) == TRUE, MAP_WININET_ERROR());
    }

#ifdef UNDER_CE
    if(0!= m_bstrHTTPCharset && ::SysStringByteLen(m_bstrHTTPCharset) > 0) {
        const WCHAR cwcContentType[] = L"Content-Type: text/xml; charset=\"%s\"\r\n";
        
        
        // get the byte size of the charset
        UINT uiSize = ::SysStringLen(m_bstrHTTPCharset) + wcslen(cwcContentType) + 1;
        
        if(uiSize >= sizeof(wcContentType)/sizeof(wcContentType[0])) {
            pwcContentType = new WCHAR[uiSize + 1];
        }
        
        swprintf(pwcContentType, cwcContentType, m_bstrHTTPCharset);
        CHK_BOOL(::HttpAddRequestHeaders(m_hRequest, pwcContentType, -1, HTTP_ADDREQ_FLAG_ADD) == TRUE, MAP_WININET_ERROR());       
    } else {
        CHK_BOOL(::HttpAddRequestHeaders(m_hRequest, L"Content-Type: text/xml\r\n", -1, HTTP_ADDREQ_FLAG_ADD) == TRUE, MAP_WININET_ERROR());
    }
#else 
    CHK_BOOL(::HttpAddRequestHeaders(m_hRequest, L"Content-Type: text/xml\r\n", -1, HTTP_ADDREQ_FLAG_ADD) == TRUE, MAP_WININET_ERROR());
#endif
    CHK_BOOL(::HttpAddRequestHeaders(m_hRequest, L"User-Agent: SOAP Sdk\r\n", -1, HTTP_ADDREQ_FLAG_ADD|HTTP_ADDREQ_FLAG_REPLACE) == TRUE, MAP_WININET_ERROR());

Cleanup:
#ifdef UNDER_CE
     if(pwcContentType != wcContentType) {
            delete [] pwcContentType;
     }
#endif

    ::SysFreeString(escSA);
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWinInetConnector::WinInetErrorToHResult(HRESULT hr)
//
//  parameters:
//          
//  description:
//          Translates HRESULT into HRESULT based on WinInet's HRESULT map
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWinInetConnector::WinInetErrorToHResult(HRESULT hr)
{
    return ::HResultToHResult(&g_WinInetHResultMap, hr);
}


#ifdef _DEBUG
////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP_(bool) CWinInetConnector::IsConnected()
//
//  parameters:
//          
//  description:
//          Debug function - is connector truly connected?
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP_(bool) CWinInetConnector::IsConnected()
{
    return m_hInternet && m_hConnect;
}
#endif
