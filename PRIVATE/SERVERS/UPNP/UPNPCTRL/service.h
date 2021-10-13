//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __SERVICE__
#define __SERVICE__

#include <combook.h>

#include "DispatchImpl.hxx"
#include "ServiceImpl.h"
#include "string.hxx"
#include "upnp.h"

typedef ce::DispatchImpl<IUPnPService, &IID_IUPnPService> UPnPServiceDispatchImpl;

class Service : public UPnPServiceDispatchImpl,
				public IUPnPServiceCallbackPrivate
{
public:
    Service(LPCWSTR pwszUniqueDeviceName,
			LPCWSTR pwszType,
            LPCWSTR pwszId,
            LPCWSTR pwszDescriptionURL,
            LPCWSTR pwszControlURL,
            LPCWSTR pwszEventsURL,
			UINT	nLifeTime,
			ce::string* pstrBaseURL)
		: m_strType(pwszType),
		  m_strId(pwszId),
		  m_pServiceImpl(pwszUniqueDeviceName, pwszType, pwszDescriptionURL, pwszControlURL, pwszEventsURL, nLifeTime, pstrBaseURL)
	{};

    ~Service()
    {
        // First unadvise service impl from connection point 
        // so that no events happen when service is being destroyed.
        // This will wait for any event in progress to be processed
        m_pServiceImpl->UnadviseConnectionPoint();
    }
    
    // implement GetInterfaceTable
    BEGIN_INTERFACE_TABLE(Service)
        IMPLEMENTS_INTERFACE(IDispatch)
		IMPLEMENTS_INTERFACE(IUPnPService)
		IMPLEMENTS_INTERFACE(IUPnPServiceCallbackPrivate)
    END_INTERFACE_TABLE()

    // implement QueryInterface/AddRef/Release
    IMPLEMENT_UNKNOWN(Service)

    // IDispatch methods
public:
    // GetIDsOfNames
    STDMETHOD(GetIDsOfNames)( 
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);

    // Invoke
    STDMETHOD(Invoke) ( 
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);

public:    
    // IUPnPService methods
    STDMETHOD(QueryStateVariable)		(/*[in]*/ BSTR bstrVariableName,
										 /*[out, retval]*/ VARIANT *pValue);
    STDMETHOD(InvokeAction)				(/*[in]*/ BSTR bstrActionName,
										 /*[in]*/ VARIANT vInActionArgs,
										 /*[in, out]*/ VARIANT * pvOutActionArgs,
										 /*[out, retval]*/ VARIANT *pvRetVal);
    STDMETHOD(get_ServiceTypeIdentifier)(/*[out, retval]*/ BSTR *pVal);
    STDMETHOD(AddCallback)				(/*[in]*/ IUnknown *pUnkCallback);
    STDMETHOD(get_Id)					(/*[out, retval]*/ BSTR *pbstrId);
    STDMETHOD(get_LastTransportStatus)	(/*[out, retval]*/ long * plValue);

public:
	// IUPnPServiceCallbackPrivate
	STDMETHOD(AddTransientCallback)		(/*[in]*/ IUnknown * pUnkCallback, /*[out]*/ DWORD *pdwCookie);
    STDMETHOD(RemoveTransientCallback)	(/*[in]*/ DWORD dwCookie);

protected:
    HRESULT InvokeActionImpl			(/*[in]*/ BSTR bstrActionName,
										 /*[in]*/ VARIANT vInActionArgs,
										 /*[in, out]*/ VARIANT * pvOutActionArgs,
										 /*[out, retval]*/ VARIANT *pvRetVal);

protected:
    class ServiceImplWrapper
    {
    public:
        ServiceImplWrapper(LPCWSTR pwszUniqueDeviceName,
						   LPCWSTR pwszServiceType,
						   LPCWSTR pwszDescriptionURL, 
						   LPCWSTR pwszControlURL, 
						   LPCWSTR pwszEventsURL,
						   UINT    nLifeTime,
						   ce::string* pstrBaseURL);

        ServiceImpl* operator->()
        {
            if(!m_bInitiated)
                Init();
            
            return &m_ServiceImpl;
        }
		
		void Init();

    protected:
        bool        m_bInitiated;
        ServiceImpl m_ServiceImpl;
        ce::string* m_pstrBaseURL;
    };

protected:
	ce::wstring         m_strType;
    ce::wstring         m_strId;
    ServiceImplWrapper	m_pServiceImpl;
};

#endif // __SERVICE__
