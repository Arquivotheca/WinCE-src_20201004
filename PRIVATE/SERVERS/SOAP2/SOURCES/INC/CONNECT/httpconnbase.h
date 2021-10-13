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
//      HttpConnBase.h
//
// Contents:
//
//      CHttpConnBase - base class for all http connectors (property implementations)
//
//-----------------------------------------------------------------------------

#ifndef __HTTPCONNBASE_H_INCLUDED__
#define __HTTPCONNBASE_H_INCLUDED__

#ifdef UNDER_CE
#include <wincrypt.h>
#endif

class CHttpConnBase : public CSoapConnector
{
protected:

    //
    // Properties
    //

    DWORD       m_dwTimeout;
    BSTR        m_bstrAuthUser;
    BSTR        m_bstrAuthPassword;
    BSTR        m_bstrProxyUser;
    BSTR        m_bstrProxyPassword;
    BSTR        m_bstrProxyServer;
    DWORD       m_dwProxyPort;
    bool        m_bUseSSL;
    bool        m_bUseProxy;
    BSTR        m_bstrSSLClientCertificateName;
    BSTR        m_bstrSoapAction;
    BSTR        m_bstrHTTPCharset;

    CUrl        m_Url;

    //
    // Enums
    //

private:
    enum HttpConnectorState
    {
        Reconnect,
        Disconnected,
        Connected,
        Message,
        Sent,
        Error,
    };

protected:
    enum
    {
#ifdef _DEBUG
        DefaultRequestTimeout = INFINITE
#else
        DefaultRequestTimeout = 30000
#endif
    };

    //
    // Implementation members
    //

protected:
    CComPointer<IWSDLPort>  m_WsdlPort;
    HttpConnectorState      m_State;

protected:
    CHttpConnBase();
    ~CHttpConnBase();

public:
    STDMETHOD(get_Property)(BSTR pPropertyName, VARIANT *pPropertyValue);
    STDMETHOD(put_Property)(BSTR pPropertyName, VARIANT PropertyValue);

    STDMETHOD(ConnectWSDL)(IWSDLPort *pPort);
    STDMETHOD(Reset)();
    STDMETHOD(BeginMessageWSDL)(IWSDLOperation *pOperation);
    STDMETHOD(EndMessage)();

protected:
    HRESULT ReadPortSettings(IWSDLPort *pPort);
    HRESULT GetSoapAction(IWSDLOperation *pOperation);

    HRESULT Disconnect(void);

    STDMETHOD(EstablishConnection)() = 0;
    STDMETHOD(ShutdownConnection)() = 0;
    STDMETHOD(SendRequest)() = 0;
    STDMETHOD(ReceiveResponse)();
    STDMETHOD(AbortMessage)();
    STDMETHOD(EmptyBuffers)();
    STDMETHOD(ClearError)();

#ifdef _DEBUG
    STDMETHOD_(bool, IsConnected)() = 0;
#endif

private:
    HRESULT get_EndPointURL(VARIANT *pVal);
    HRESULT put_EndPointURL(VARIANT *pVal);
    HRESULT get_Timeout(VARIANT *pVal);
    HRESULT put_Timeout(VARIANT *pVal);
    HRESULT get_AuthUser(VARIANT *pVal);
    HRESULT put_AuthUser(VARIANT *pVal);
    HRESULT get_AuthPassword(VARIANT *pVal);
    HRESULT put_AuthPassword(VARIANT *pVal);
    HRESULT get_ProxyUser(VARIANT *pVal);
    HRESULT put_ProxyUser(VARIANT *pVal);
    HRESULT get_ProxyPassword(VARIANT *pVal);
    HRESULT put_ProxyPassword(VARIANT *pVal);
    HRESULT get_ProxyServer(VARIANT *pVal);
    HRESULT put_ProxyServer(VARIANT *pVal);
    HRESULT get_ProxyPort(VARIANT *pVal);
    HRESULT put_ProxyPort(VARIANT *pVal);
    HRESULT get_UseProxy(VARIANT *pVal);
    HRESULT put_UseProxy(VARIANT *pVal);
    HRESULT get_UseSSL(VARIANT *pVal);
    HRESULT put_UseSSL(VARIANT *pVal);
    HRESULT get_SSLClientCertificateName(VARIANT *pVal);
    HRESULT put_SSLClientCertificateName(VARIANT *pVal);
    HRESULT get_SoapAction(VARIANT *pVal);
    HRESULT put_SoapAction(VARIANT *pVal);
    HRESULT get_HTTPCharset(VARIANT *pVal);
    HRESULT put_HTTPCharset(VARIANT *pVal);

protected:
    HRESULT PutEndPointURL(BSTR bstrUrl);
    HRESULT GetEndPointURL(BSTR *pbstrUrl);
    void ScheduleReconnect();

    HRESULT GetProxyURL(BSTR *pbstrUrl);

    DECLARE_PROPERTY_MAP(CHttpConnBase);

protected:
    static HRESULT FindCertificate(BSTR bstrCertName, PCCERT_CONTEXT *pCertContext);
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT CHttpConnBase::GetEndPointURL(BSTR *pbstrUrl)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT CHttpConnBase::GetEndPointURL(BSTR *pbstrUrl)
{
    return m_Url.get_Url(pbstrUrl);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline void CHttpConnBase::ScheduleReconnect()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CHttpConnBase::ScheduleReconnect()
{
    m_State = Reconnect;
}


#endif //__HTTPCONNBASE_H_INCLUDED__
