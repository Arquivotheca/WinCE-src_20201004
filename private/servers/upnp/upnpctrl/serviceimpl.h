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
#ifndef __SERVICE_IMPL__
#define __SERVICE_IMPL__

#include "ConnectionPoint.h"
#include "HttpChannel.h"
#include "Action.h"
#include "StateVar.h"
#include "vector.hxx"
#include "string.hxx"
#include "sax.h"
#include "upnp.h"

// ServiceImpl
class ServiceImpl : ce::SAXContentHandler,
                    ConnectionPoint::ICallback
{
public:
    ServiceImpl();
    ~ServiceImpl();

    HRESULT Init(LPCWSTR pwszUniqueDeviceName,
                 LPCWSTR pwszServiceType, 
                 LPCWSTR pwszDescriptionURL, 
                 LPCWSTR pwszControlURL, 
                 LPCWSTR pwszEventsURL,
                 UINT    nLifeTime,
                 ce::string *pszBaseURL);
    void Uninit();
    
    HRESULT Invoke(DISPID dispIdMember, 
                   WORD wFlags, 
                   DISPPARAMS FAR *pDispParams, 
                   VARIANT FAR *pVarResult, 
                   EXCEPINFO FAR *pExcepInfo, 
                   unsigned int FAR *puArgErr);

    HRESULT QueryStateVariable(LPCWSTR pwszVariableName, VARIANT *pValue);

    HRESULT GetIDsOfNames(OLECHAR FAR *FAR *rgszNames,
                          unsigned int cNames, 
                          DISPID FAR *rgDispId);

    HRESULT AddCallback(IUPnPService* pUPnPService, IUnknown *punkCallback, DWORD* pdwCookie);
    
    HRESULT RemoveCallback(DWORD dwCookie);

    long GetLastTransportStatus()
        {return m_lLastTransportStatus; }

    int GetActionOutArgumentsCount(DISPID dispidAction);

private:
    enum {query_state_var_dispid = 999,
          base_dispid = 1000};

// ConnectionPoint::ICallback
private:    
    virtual void StateVariableChanged(LPCWSTR pwszName, LPCWSTR pwszValue);
    virtual void ServiceInstanceDied(LPCWSTR pszUSN);
    virtual void AliveNotification(LPCWSTR pszUSN, LPCWSTR pszLocation, LPCWSTR pszAL, DWORD dwLifeTime);

// ISAXContentHandler
private:
    virtual HRESULT STDMETHODCALLTYPE startElement(
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName,
        /* [in] */ ISAXAttributes __RPC_FAR *pAttributes);
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName);
    
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars);

protected:
    class callback
    {
    public:
        enum type {dispatch, upnp_service_callback, invalid};

        callback()
            : m_pUnk(NULL),
              m_type(invalid)
        {}

        ~callback()
        {
            if(m_pUnk)
            {
                m_pUnk->Release();
            }
        }

        callback(const callback& c)
        {
            m_type = c.m_type;
            if(m_pUnk = c.m_pUnk)
            {
                m_pUnk->AddRef();
            }
        }

        type m_type;

        union
        {
            IDispatch*             m_pDispatch;
            IUPnPServiceCallback*  m_pUPnPServiceCallback;
            IUnknown*              m_pUnk;
        };
    };
    
    ce::vector<Action>             m_Actions;
    ce::vector<StateVar>           m_StateVars;
    ce::wstring                    m_strUniqueServiceName;
    ce::string                     m_strControlURL;
    ce::string                     m_strEventsURL;
    ce::string                     m_strDescriptionURL;
    ce::wstring                    m_strType;
    DWORD                          m_dwSubscriptionCookie;
    ce::list<callback>             m_listCallback;
    ce::critical_section           m_csListCallback;
    IUPnPService*                  m_pUPnPService;
    bool                           m_bSubscribed;
    bool                           m_bServiceInstanceDied;
    DWORD                          m_dwRequestCookie;
    DWORD                          m_dwChannelCookie;

    Action *                       m_pActionQueryStateVar;
    // fake StatVar object to support QueryStateVariable as action
    StateVar *                     m_pStateVarName; 

    // members used during parsing
    ce::wstring                    m_strActionName;
    ce::wstring                    m_strArgumentDirection;
    ce::wstring                    m_strArgumentName;
    ce::wstring                    m_strStateVarName;
    ce::wstring                    m_strStateVarType;
    ce::wstring                    m_strSpecVersionMajor;
    ce::wstring                    m_strSpecVersionMinor;
    bool                           m_bParsedRootElement;
    bool                           m_bRetval;
    bool                           m_bSendEvents;
    Action*                        m_pCurrentAction;
    long                           m_lLastTransportStatus;
};

#endif // __SERVICE_IMPL__
