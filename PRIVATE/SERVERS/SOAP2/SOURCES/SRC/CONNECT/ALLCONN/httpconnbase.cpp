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
//      HttpConnBase.cpp
//
// Contents:
//
//      CHttpConnBase class implementation - property set/get code
//
//-----------------------------------------------------------------------------

#include "Headers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Property Map
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_PROPERTY_MAP(CHttpConnBase)
    ADD_PROPERTY(CHttpConnBase, VT_BSTR, s_EndPointURL,                EndPointURL)
    ADD_PROPERTY(CHttpConnBase, VT_I4,   s_Timeout,                    Timeout)
    ADD_PROPERTY(CHttpConnBase, VT_BSTR, s_AuthUser,                   AuthUser)
    ADD_PROPERTY(CHttpConnBase, VT_BSTR, s_AuthPassword,               AuthPassword)
    ADD_PROPERTY(CHttpConnBase, VT_BSTR, s_ProxyUser,                  ProxyUser)
    ADD_PROPERTY(CHttpConnBase, VT_BSTR, s_ProxyPassword,              ProxyPassword)
    ADD_PROPERTY(CHttpConnBase, VT_BSTR, s_ProxyServer,                ProxyServer)
    ADD_PROPERTY(CHttpConnBase, VT_I4,   s_ProxyPort,                  ProxyPort)
    ADD_PROPERTY(CHttpConnBase, VT_BOOL, s_UseProxy,                   UseProxy)
    ADD_PROPERTY(CHttpConnBase, VT_BOOL, s_UseSSL,                     UseSSL)
    ADD_PROPERTY(CHttpConnBase, VT_BSTR, s_SSLClientCertificateName,   SSLClientCertificateName)
    ADD_PROPERTY(CHttpConnBase, VT_BSTR, s_SoapAction,                 SoapAction)
    ADD_PROPERTY(CHttpConnBase, VT_BSTR, s_HTTPCharset,                HTTPCharset)
END_PROPERTY_MAP(CHttpConnBase)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CHttpConnBase::CHttpConnBase()
//
//  parameters:
//
//  description:
//          Constructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CHttpConnBase::CHttpConnBase()
: m_dwTimeout(DefaultRequestTimeout),
  m_bstrAuthUser(0),
  m_bstrAuthPassword(0),
  m_bstrProxyUser(0),
  m_bstrProxyPassword(0),
  m_bstrProxyServer(0),
  m_dwProxyPort(0),
  m_bUseProxy(false),
  m_bUseSSL(false),
  m_bstrSSLClientCertificateName(0),
  m_bstrSoapAction(0),
  m_bstrHTTPCharset(0),
  m_State(Disconnected)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CHttpConnBase::~CHttpConnBase()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CHttpConnBase::~CHttpConnBase()
{
    ::SysFreeString(m_bstrAuthUser);
    ::SysFreeString(m_bstrAuthPassword);
    ::SysFreeString(m_bstrProxyUser);
    ::SysFreeString(m_bstrProxyPassword);
    ::SysFreeString(m_bstrProxyServer);
    ::SysFreeString(m_bstrSSLClientCertificateName);
    ::SysFreeString(m_bstrSoapAction);
    ::SysFreeString(m_bstrHTTPCharset);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CHttpConnBase::get_Property(BSTR bstrPropertyName, VARIANT *pPropertyValue)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::get_Property(BSTR pPropertyName, VARIANT *pPropertyValue)
{
    return GET_PROPERTY(pPropertyName, pPropertyValue);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CHttpConnBase::put_Property(BSTR bstrPropertyName, VARIANT PropertyValue)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::put_Property(BSTR pPropertyName, VARIANT PropertyValue)
{
    return PUT_PROPERTY(pPropertyName, &PropertyValue);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnBase::ConnectWSDL(IWSDLPort *pPort)
//
//  parameters:
//          
//  description:
//          Connects the Http connector (reads the settings from IWSDLPort)
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::ConnectWSDL(IWSDLPort *pPort)
{
    HRESULT hr = S_OK;

    switch (m_State)
    {
    case Reconnect:
        Disconnect();
        ASSERT(m_State == Disconnected);
    case Connected:
    case Disconnected:
        break;
    case Message:
        return E_INVALIDARG;
    case Sent:
        break;
    default:
        ASSERT(FALSE);
    }

    m_WsdlPort = pPort;

    CHK(ReadPortSettings(pPort));

    hr = S_OK;

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnBase::Reset()
//
//  parameters:
//          
//  description:
//          Resets the connector
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::Reset()
{
    switch (m_State)
    {
    case Message:
    case Sent:
        if (SUCCEEDED(AbortMessage()))
        {
            m_State = Connected;
            break;
        }
        else if (SUCCEEDED(ShutdownConnection()))
        {
            m_State = Disconnected;
            break;
        }

        m_State = Error;

    case Error:
        ClearError();
        break;

    case Reconnect:
    case Connected:
    case Disconnected:
        break;
    }

    ClearError();
    EmptyBuffers();

    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnBase::BeginMessageWSDL(IWSDLOperation *pOperation)
//
//  parameters:
//          
//  description:
// 
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::BeginMessageWSDL(IWSDLOperation *pOperation)
{
    HRESULT hr = S_OK;

    switch (m_State)
    {
    case Reconnect:
        Disconnect();
        ASSERT(m_State == Disconnected);
        
        // fall through
    case Disconnected:
        CHK(EstablishConnection());
        m_State = Connected;

        // fall through
    case Connected:
    #if !defined UNDER_CE && defined _DEBUG
        ASSERT(IsConnected());
    #endif
        break;

    case Message:
        hr = E_INVALIDARG;
        goto Cleanup;

    case Sent:
        break;
    }

    if (pOperation)
    {
        CHK(GetSoapAction(pOperation));
    }

    m_State = Message;
    hr      = S_OK;

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnBase::EndMessage()
//
//  parameters:
//          
//  description:
//          Finishes construction of message and sends it to the endpoint
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::EndMessage()
{
    if (m_State != Message)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;

    CHK(SendRequest());

    m_State = Sent;

    CHK(ReceiveResponse());

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::ReadPortSetting(IWSDLPort *pPort)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::ReadPortSettings(IWSDLPort *pPort)
{
    HRESULT hr      = S_OK;
    BSTR    address = 0;

    if (pPort != 0)
    {
        CAutoBSTR bindStyle;
        CAutoBSTR transport;

        CHK(pPort->get_bindStyle(& bindStyle));
        CHK(pPort->get_transport(& transport));
        CHK(pPort->get_address(& address));

        CHK(PutEndPointURL(address));
    }

Cleanup:
    ::SysFreeString(address);
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::GetSoapAction(IWSDLOperation *pOperation)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::GetSoapAction(IWSDLOperation *pOperation)
{
    HRESULT hr = S_OK;
    BSTR    bstrSoapAction = 0;

    if (! pOperation)
    {
        goto Cleanup;
    }

    CHK(pOperation->get_soapAction(&bstrSoapAction));
    CHK(::FreeAndStoreBSTR(m_bstrSoapAction, bstrSoapAction));

    bstrSoapAction = 0;

    hr = S_OK;

Cleanup:
    ::SysFreeString(bstrSoapAction);
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::Disconnect()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::Disconnect()
{
    ShutdownConnection();
    ClearError();
    EmptyBuffers();

    m_State = Disconnected;

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnBase::ReceiveResponse()
//
//  parameters:
//          
//  description:
//          Default response reception code
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::ReceiveResponse()
{
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnBase::AbortMessage()
//
//  parameters:
//          
//  description:
//          Attempt to abort the sent message
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::AbortMessage()
{
    return E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnBase::EmptyBuffers()
//
//  parameters:
//          
//  description:
//          Empty input and output message buffers
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::EmptyBuffers()
{
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CHttpConnBase::ClearError()
//
//  parameters:
//          
//  description:
//          Clears any errors cached in the connector and message buffers
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CHttpConnBase::ClearError()
{
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_EndPointURL(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_EndPointURL(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    HRESULT hr = S_OK;

    CHK(GetEndPointURL(&V_BSTR(pVal)));
    V_VT(pVal) = VT_BSTR;

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_EndPointURL(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_EndPointURL(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BSTR);
    ScheduleReconnect();
    return PutEndPointURL(V_BSTR(pVal));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_Timeout(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_Timeout(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal) = VT_I4;
    V_I4(pVal) = m_dwTimeout;
    
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_Timeout(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_Timeout(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_I4);

    ScheduleReconnect();
    if (V_I4(pVal) < 0)
    {
        return(E_INVALIDARG);
    }
    m_dwTimeout = V_I4(pVal);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_AuthUser(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_AuthUser(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BSTR;
    V_BSTR(pVal) = ::SysAllocString(m_bstrAuthUser);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_AuthUser(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_AuthUser(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BSTR);

    ScheduleReconnect();
    return ::AtomicFreeAndCopyBSTR(m_bstrAuthUser, V_BSTR(pVal));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_AuthPassword(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_AuthPassword(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BSTR;
    V_BSTR(pVal) = ::SysAllocString(m_bstrAuthPassword);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_AuthPassword(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_AuthPassword(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BSTR);

    ScheduleReconnect();
    return ::AtomicFreeAndCopyBSTR(m_bstrAuthPassword, V_BSTR(pVal));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_ProxyUser(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_ProxyUser(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BSTR;
    V_BSTR(pVal) = ::SysAllocString(m_bstrProxyUser);

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_ProxyUser(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_ProxyUser(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BSTR);

    ScheduleReconnect();
    return ::AtomicFreeAndCopyBSTR(m_bstrProxyUser, V_BSTR(pVal));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_ProxyPassword(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_ProxyPassword(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BSTR;
    V_BSTR(pVal) = ::SysAllocString(m_bstrProxyPassword);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_ProxyPassword(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_ProxyPassword(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BSTR);

    ScheduleReconnect();
    return ::AtomicFreeAndCopyBSTR(m_bstrProxyPassword, V_BSTR(pVal));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_ProxyServer(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_ProxyServer(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BSTR;
    V_BSTR(pVal) = ::SysAllocString(m_bstrProxyServer);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_ProxyServer(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_ProxyServer(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BSTR);

    HRESULT hr = S_OK;

    ScheduleReconnect();
    hr = ::AtomicFreeAndCopyBSTR(m_bstrProxyServer, V_BSTR(pVal));

    if (SUCCEEDED(hr) && V_BSTR(pVal) != 0 && *(V_BSTR(pVal)) != 0)
    {
        m_bUseProxy = true;
    }

    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_ProxyPort(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_ProxyPort(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)  = VT_I4;
    V_I4(pVal) = m_dwProxyPort;

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_ProxyPort(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_ProxyPort(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_I4);

    if (V_I4(pVal) < 0)
    {
        return(E_INVALIDARG);
    }
    ScheduleReconnect();
    m_dwProxyPort = V_I4(pVal);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_UseProxy(VARIANT *pVal)
//
//  parameters:
//
//  description:
//        
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_UseProxy(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BOOL;
    V_BOOL(pVal) = m_bUseProxy ? VARIANT_TRUE : VARIANT_FALSE;

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_UseProxy(VARIANT *pVal)
//
//  parameters:
//
//  description:
//        
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_UseProxy(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BOOL);

    ScheduleReconnect();
    m_bUseProxy = !! V_BOOL(pVal);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_UseSSL(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_UseSSL(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BOOL;
    V_BOOL(pVal) = m_bUseSSL ? VARIANT_TRUE : VARIANT_FALSE;

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_UseSSL(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_UseSSL(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BOOL);

    ScheduleReconnect();
    m_bUseSSL = !! V_BOOL(pVal);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_SSLClientCertificateName(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_SSLClientCertificateName(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BSTR;
    V_BSTR(pVal) = ::SysAllocString(m_bstrSSLClientCertificateName);

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_SSLClientCertificateName(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_SSLClientCertificateName(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BSTR);
    ScheduleReconnect();
    return ::AtomicFreeAndCopyBSTR(m_bstrSSLClientCertificateName, V_BSTR(pVal));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_SoapAction(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_SoapAction(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BSTR;
    V_BSTR(pVal) = ::SysAllocString(m_bstrSoapAction);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_SoapAction(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_SoapAction(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BSTR);
    return ::AtomicFreeAndCopyBSTR(m_bstrSoapAction, V_BSTR(pVal));
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::get_HTTPCharset(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::get_HTTPCharset(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_EMPTY);

    V_VT(pVal)   = VT_BSTR;
    V_BSTR(pVal) = ::SysAllocString(m_bstrHTTPCharset);

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::put_HTTPCharset(VARIANT *pVal)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::put_HTTPCharset(VARIANT *pVal)
{
    ASSERT(pVal != 0 && V_VT(pVal) == VT_BSTR);
    return ::AtomicFreeAndCopyBSTR(m_bstrHTTPCharset, V_BSTR(pVal));
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::PutEndPointURL(BSTR bstrUrl)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT CHttpConnBase::PutEndPointURL(BSTR bstrUrl)
{
    HRESULT hr = m_Url.put_Url(bstrUrl);

    BEGIN_INLINE_ERROR_MAP()
        PASS_THROUGH(E_OUTOFMEMORY)
    END_INLINE_ERROR_MAP(hr, CONN_E_HTTP_BAD_URL)
    
    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static HRESULT ParseCertificateName(BSTR bstrCertName, BSTR *pLocation, BSTR *pStoreName, BSTR *pCertificate)
//
//  parameters:
//          
//  description:
//          Parses certificate name into pieces [ CURRENT_USER | LOCAL_MACHINE \ [ store-name \ ] ] cert-name
//          Default store is CURRENT_USER\MY
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
static HRESULT ParseCertificateName(BSTR bstrCertName, BSTR *pLocation, BSTR *pStoreName, BSTR *pCertificate)
{
    ASSERT(bstrCertName && *bstrCertName);

    HRESULT hr              = S_OK;
    LPWSTR  pCertName       = bstrCertName;
    DWORD   dwLocation      = 0;
    DWORD   dwStoreName     = 0;
    DWORD   dwCertificate   = 0;
    BSTR    bstrLocation    = 0;
    BSTR    bstrStoreName   = 0;
    BSTR    bstrCertificate = 0;
    LPCWSTR pszLocation     = 0;
    LPCWSTR pszStoreName    = 0;
    LPCWSTR pszCertificate  = 0;

    while (*pCertName ++);   // Go to the end

    if (pCertName > bstrCertName + 1)
    {
        pCertName --; 
    }
    else    // Nothing to parse
    {
        goto Cleanup;
    }

    // Find certificate Name

    while (-- pCertName >= bstrCertName && *pCertName != L'\\')
    {
        dwCertificate ++;
    }

    pszCertificate = pCertName + 1;

    if (pCertName < bstrCertName)
    {
        goto Finished;
    }

    while (-- pCertName >= bstrCertName && *pCertName != L'\\')
    {
        dwStoreName ++;
    }

    pszStoreName = pCertName + 1;

    if (pCertName < bstrCertName)
    {
        goto Finished;
    }

    while (-- pCertName >= bstrCertName && *pCertName != L'\\')
    {
        dwLocation ++;
    }

    if (pCertName >= bstrCertName)
    {
        hr = CONN_E_BAD_CERTIFICATE_NAME;
        goto Cleanup;
    }

    pszLocation = pCertName + 1;

    ASSERT(pszLocation == bstrCertName);

Finished:

    ASSERT(pszCertificate);
    if (pszCertificate)
    {
        CHK_MEM(bstrCertificate = ::SysAllocStringLen(pszCertificate, dwCertificate));
    }
    else
    {
        hr = CONN_E_BAD_CERTIFICATE_NAME;
        goto Cleanup;
    }

    if (pszStoreName)
    {
        CHK_MEM(bstrStoreName = ::SysAllocStringLen(pszStoreName, dwStoreName));
    }
    else
    {
        CHK_MEM(bstrStoreName = ::SysAllocStringLen(L"MY", 2));
    }

    if (pszLocation)
    {
        CHK_MEM(bstrLocation = ::SysAllocStringLen(pszLocation, dwLocation));
    }
    else
    {
        CHK_MEM(bstrLocation = ::SysAllocStringLen(L"CURRENT_USER", 12));
    }

    *pCertificate   = bstrCertificate;
    bstrCertificate = 0;
    *pStoreName     = bstrStoreName;
    bstrStoreName   = 0;
    *pLocation      = bstrLocation;
    bstrLocation    = 0;

    hr = S_OK;

Cleanup:
    ::SysFreeString(bstrCertificate);
    ::SysFreeString(bstrStoreName);
    ::SysFreeString(bstrLocation);

    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::GetProxyURL(BSTR *pbstrUrl)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::GetProxyURL(BSTR *pbstrUrl)
{
    HRESULT hr   = S_OK;
    LPWSTR  pUrl = 0;

    CHK_ARG(pbstrUrl != 0);

    *pbstrUrl = 0;

    if (m_dwProxyPort == 0)
    {
        pUrl = m_bstrProxyServer;
    }
    else
    {
        if (m_bstrProxyServer)
        {
            int length = ::SysStringByteLen(m_bstrProxyServer);
#ifndef UNDER_CE         
            try
#else   
            __try
#endif
            {
                pUrl = reinterpret_cast<LPWSTR>(_alloca(length + 40));
            }
#ifndef UNDER_CE
            catch (...)
#else
            __except(1)
#endif
            {
                hr = E_OUTOFMEMORY;
                goto Cleanup;
            }

            swprintf(pUrl, L"%s:%i", m_bstrProxyServer, m_dwProxyPort);
        }
    }

    if (pUrl != 0)
    {
        *pbstrUrl = ::SysAllocString(pUrl);
    }

    hr = S_OK;

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: static bool FindCommonName(WCHAR *subject, WCHAR *&name)
//
//  parameters:
//
//  description:
//        Identifies position of the Common Name (CN) part in the certificate name
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
static bool FindCommonName(WCHAR *subject, WCHAR *&name)
{
    ASSERT(subject != 0);
    ASSERT(name == 0);

    enum
    {
        Initial,
        Name,
        Value,
        QValue,
        QQValue,
        NValue,
    } state = Initial;

    WCHAR *pName     = 0;
    DWORD dwNameSize = 0;

    for (; *subject != _T('\0'); subject ++)
    {
        switch (state)
        {
        case Initial:
            if (! iswspace(*subject))
            {
                state      = Name;
                pName      = subject;
                dwNameSize = 0;
            }
            break;
        case Name:
            if (*subject == _T('='))
            {
                dwNameSize = subject - pName;
                state      = Value;
            }
            break;
        case Value:
            if (*subject == _T('"'))
            {
                state = QValue;
                if (*(subject + 1) != _T('\0'))
                {
                    name = subject + 1;
                }
            }
            else
            {
                state = NValue;
                name  = subject;
            }
            break;
        case QValue:
            if (*subject == _T('"'))
            {
                state = QQValue;
            }
            break;
        case QQValue:
            if (*subject == _T('"'))
            {
                state = QValue;
            }
            else if (*subject == _T(','))
                goto Found;
            break;
        case NValue:
            if (*subject == _T(','))
            {
                goto Found;
            }
            break;
        default:
            ASSERT(FALSE);
            break;

        Found:
            *subject = _T('\0');
            if (pName != 0 && dwNameSize == 2 && wcsncmp(pName, _T("CN"), 2) == 0)
            {
                return true;
            }
            else
            {
                state   = Initial;
                name    = 0;
            }
        }
    }

    if (state == NValue)
    {
        *subject = _T('\0');
        if (pName != 0 && wcsncmp(pName, _T("CN"), dwNameSize) == 0)
        {
            return true;
        }
    }

    name   = 0;

    return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CHttpConnBase::FindCertificate(BSTR bstrCertName, PCERT_CONTEXT *pCertContext)
//
//  parameters:
//          
//  description:
//          Finds certificate specified by bstrCertName
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CHttpConnBase::FindCertificate(BSTR bstrCertName, PCCERT_CONTEXT *pCertContext)
{
    ASSERT(pCertContext != 0);

    HRESULT hr = S_OK;
    BSTR    bstrLocation     = 0;
    BSTR    bstrStoreName    = 0;
    BSTR    bstrCertificate  = 0;

    PCCERT_CONTEXT pContext   = 0;
    HCERTSTORE     hCertStore = 0;
    DWORD          dwFlags    = 0;

    if (bstrCertName != 0 && *bstrCertName != 0)
    {
        CHK(::ParseCertificateName(bstrCertName, &bstrLocation, &bstrStoreName, &bstrCertificate));

        if (wcscmp(bstrLocation, L"CURRENT_USER") == 0)
        {
            dwFlags = CERT_SYSTEM_STORE_CURRENT_USER;
        }
        else if (wcscmp(bstrLocation, L"LOCAL_MACHINE") == 0)
        {
            dwFlags = CERT_SYSTEM_STORE_LOCAL_MACHINE;
        }
        else
        {
            hr = CONN_E_BAD_CERTIFICATE_NAME;
            goto Cleanup;
        }

        hCertStore = ::CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, dwFlags, bstrStoreName);
        CHK_BOOL(hCertStore, HRESULT_FROM_WIN32(::GetLastError()));

        for (;;)
        {
            pContext = ::CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING, 0, CERT_FIND_ANY, 0, pContext);

            if (! pContext)
            {
                pContext = ::CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR, bstrCertificate, 0);
                CHK_BOOL(pContext, HRESULT_FROM_WIN32(::GetLastError()));
                break;
            }
#ifndef UNDER_CE
            WCHAR subject[1024] = { 0 };
            WCHAR *name  = 0;

            CHK_BOOL(::CertNameToStrW(X509_ASN_ENCODING, &pContext->pCertInfo->Subject, CERT_X500_NAME_STR, subject, sizeof(subject)), HRESULT_FROM_WIN32(::GetLastError()));
#else
            #define DEF_SZ_SUB_SIZE 1024
            WCHAR subject[DEF_SZ_SUB_SIZE] = { 0 };
            WCHAR *name  = 0;

            CHK_BOOL(::CertNameToStrW(X509_ASN_ENCODING, &pContext->pCertInfo->Subject, CERT_X500_NAME_STR, subject, DEF_SZ_SUB_SIZE), HRESULT_FROM_WIN32(::GetLastError()));

#endif

            if (! FindCommonName(subject, name))
                continue;

            if (_wcsicmp(name, bstrCertificate) == 0)
                break;
        }

    }

    *pCertContext = pContext;
    pContext      = 0;

Cleanup:

    if (hCertStore)
    {
        ::CertCloseStore(hCertStore, 0);
    }

    if (pContext)
    {
        ::CertFreeCertificateContext(pContext);
    }

    ::SysFreeString(bstrCertificate);
    ::SysFreeString(bstrStoreName);
    ::SysFreeString(bstrLocation);

    return hr;
}
