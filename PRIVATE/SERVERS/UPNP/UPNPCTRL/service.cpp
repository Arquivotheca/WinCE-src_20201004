//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pch.h"
#pragma hdrstop

#include "Service.h"
#include "com_macros.h"
#include "HttpRequest.h"
#include "safe_array.hxx"

// ServiceImplWrapper::ServiceImplWrapper
Service::ServiceImplWrapper::ServiceImplWrapper(
							LPCWSTR pwszUniqueDeviceName,
							LPCWSTR pwszServiceType, 
							LPCWSTR pwszDescriptionURL,
							LPCWSTR pwszControlURL, 
							LPCWSTR pwszEventsURL,
							UINT    nLifeTime,
							ce::string* pstrBaseURL)
    : m_bInitiated(false),
      m_pstrBaseURL(pstrBaseURL),
	  m_ServiceImpl(pwszUniqueDeviceName,
					pwszServiceType, 
					pwszDescriptionURL,
					pwszControlURL, 
					pwszEventsURL,
					nLifeTime)
{
}


// InitServiceImpl
void Service::ServiceImplWrapper::Init()
{
	m_ServiceImpl.Init(*m_pstrBaseURL);
	
	m_bInitiated = true;
}


///////////////////////////////////
// IUPnPService methods

// QueryStateVariable
STDMETHODIMP Service::QueryStateVariable(
	/*[in]*/ BSTR bstrVariableName,
	/*[out, retval]*/ VARIANT *pValue)
{
	HRESULT hr;

	switch(hr = m_pServiceImpl->QueryStateVariable(bstrVariableName, pValue))
	{
		case DISP_E_UNKNOWNNAME:
        case DISP_E_MEMBERNOTFOUND: return UPNP_E_INVALID_VARIABLE;
		
		case HRESULT_FROM_WIN32(ERROR_INTERNET_TIMEOUT):
									return UPNP_E_DEVICE_TIMEOUT;

		default:					return hr;
	}
}


// InvokeAction
STDMETHODIMP Service::InvokeAction(
	/*[in]*/ BSTR bstrActionName,
	/*[in]*/ VARIANT vInActionArgs,
	/*[in, out]*/ VARIANT * pvOutActionArgs,
	/*[out, retval]*/ VARIANT *pvRetVal)
{
	HRESULT hr;

	switch(hr = InvokeActionImpl(bstrActionName, vInActionArgs, pvOutActionArgs, pvRetVal))
	{
		case DISP_E_UNKNOWNNAME:
        case DISP_E_MEMBERNOTFOUND: return UPNP_E_INVALID_ACTION;
		
		case DISP_E_TYPEMISMATCH:
        case DISP_E_PARAMNOTFOUND:  return UPNP_E_INVALID_ARGUMENTS;
        
        case HRESULT_FROM_WIN32(ERROR_INTERNET_TIMEOUT):
									return UPNP_E_DEVICE_TIMEOUT;

		default:					return hr;
	}
}


// InvokeActionImpl
HRESULT Service::InvokeActionImpl(
	/*[in]*/ BSTR bstrActionName,
	/*[in]*/ VARIANT vInActionArgs,
	/*[in, out]*/ VARIANT * pvOutActionArgs,
	/*[out, retval]*/ VARIANT *pvRetVal)
{
	DISPID			dispid;
	int				nOutArgs, nInArgs, i, ubound;
	unsigned int	position;
	HRESULT			hr;
	UINT			uArgErr;

	if(FAILED(hr = m_pServiceImpl->GetIDsOfNames(&bstrActionName, 1, &dispid)))
		return hr;

	ce::safe_array<ce::variant, VT_VARIANT> arrayInArguments;
	ce::safe_array<ce::variant, VT_VARIANT> arrayOutArguments;

	VARIANT *pvarInArgs;

    // dereference [in] arguments variant
	for (pvarInArgs = &vInActionArgs; pvarInArgs->vt == (VT_VARIANT | VT_BYREF); pvarInArgs = pvarInArgs->pvarVal);
	
	// attach to [in] arguments array
	if(pvarInArgs->vt == (VT_ARRAY | VT_VARIANT))
	{
		arrayInArguments.attach(pvarInArgs->parray);
	}
	else
		if(pvarInArgs->vt == (VT_BYREF | VT_ARRAY | VT_VARIANT))
		{
			CHECK_POINTER(pvarInArgs->pparray);
			
			arrayInArguments.attach(*pvarInArgs->pparray);
		}
		else
			return DISP_E_TYPEMISMATCH;

	nOutArgs = m_pServiceImpl->GetActionOutArgumentsCount(dispid);

	if(nOutArgs)
	{
		CHECK_POINTER(pvOutActionArgs);
			
		// create [out] arguments safe array
		arrayOutArguments.create(nOutArgs);
	}

	nInArgs = arrayInArguments.size();

	DISPPARAMS		DispParams;
	ce::variant*	rgvarg;

	DispParams.cArgs = nInArgs + nOutArgs;
	DispParams.cNamedArgs = 0;
	DispParams.rgdispidNamedArgs = NULL;

	rgvarg = new ce::variant[DispParams.cArgs];
	
	DispParams.rgvarg = &rgvarg[0];

	// argument in DISPPARAM are in reverse order
	position = DispParams.cArgs - 1;
	
	for(i = arrayInArguments.lbound(), ubound = arrayInArguments.ubound(); i <= ubound; ++i, --position)
	{
		Assert(position >= 0 && position < DispParams.cArgs);

		rgvarg[position] = arrayInArguments[i];
	}

	if(nOutArgs)
	{
		// lock array so that it is safe to get pointers to elements - safe_array will unlock in dtor
		arrayOutArguments.lock();

		for(i = arrayOutArguments.lbound(), ubound = arrayOutArguments.ubound(); i <= ubound; ++i, --position)
		{
			Assert(position >= 0 && position < DispParams.cArgs);

			rgvarg[position].vt = VT_VARIANT | VT_BYREF;
			rgvarg[position].pvarVal = &arrayOutArguments[i];
		}
	}
		
	Assert(position == -1);

	hr = m_pServiceImpl->Invoke(dispid, DISPATCH_METHOD, &DispParams, pvRetVal, NULL, &uArgErr);

	delete[] rgvarg;

	if(nOutArgs)
	{
		arrayOutArguments.unlock();

		if(SUCCEEDED(hr))
		{
			VariantClear(pvOutActionArgs);
			
			pvOutActionArgs->vt = VT_ARRAY | VT_VARIANT;
			pvOutActionArgs->parray = arrayOutArguments.detach();
		}
	}

	return hr;
}


// ServiceTypeIdentifier
STDMETHODIMP Service::get_ServiceTypeIdentifier(/*[out, retval]*/ BSTR *pVal)
{
	CHECK_OUT_POINTER(pVal);

	*pVal = SysAllocString(m_strType);

	if(pVal)
		return S_OK;
	else
		return E_OUTOFMEMORY;
}


// AddCallback
STDMETHODIMP Service::AddCallback(/*[in]*/ IUnknown *punkCallback)
{
	return m_pServiceImpl->AddCallback(this, punkCallback, NULL);
}


// Id
STDMETHODIMP Service::get_Id(/*[out, retval]*/ BSTR *pVal)
{
	CHECK_OUT_POINTER(pVal);

	*pVal = SysAllocString(m_strId);

	if(pVal)
		return S_OK;
	else
		return E_OUTOFMEMORY;
}


// LastTransportStatus
STDMETHODIMP Service::get_LastTransportStatus(/*[out, retval]*/ long* plValue)
{
	CHECK_POINTER(plValue);

	*plValue = m_pServiceImpl->GetLastTransportStatus();
	
	return S_OK;
}


///////////////////////////////////
// IUPnPServiceCallbackPrivate


// AddTransientCallback
STDMETHODIMP Service::AddTransientCallback(/*[in]*/ IUnknown* pUnkCallback, /*[out]*/ DWORD *pdwCookie)
{
	return m_pServiceImpl->AddCallback(this, pUnkCallback, pdwCookie);
}


// RemoveTransientCallback
STDMETHODIMP Service::RemoveTransientCallback(/*[in]*/ DWORD dwCookie)
{
	return m_pServiceImpl->RemoveCallback(dwCookie);
}


///////////////////////////////////
// IDispatch

// GetIDsOfNames
STDMETHODIMP Service::GetIDsOfNames( 
        /* [in] */ REFIID riid,
        /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
        /* [in] */ UINT cNames,
        /* [in] */ LCID lcid,
        /* [size_is][out] */ DISPID __RPC_FAR *rgDispId)
{
	HRESULT hr;
    
    hr = UPnPServiceDispatchImpl::GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);
    
   	if(DISP_E_UNKNOWNNAME == hr)
		hr = m_pServiceImpl->GetIDsOfNames(rgszNames, cNames, rgDispId);
    
    return hr;
}


// Invoke
STDMETHODIMP Service::Invoke( 
        /* [in] */ DISPID dispIdMember,
        /* [in] */ REFIID riid,
        /* [in] */ LCID lcid,
        /* [in] */ WORD wFlags,
        /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
        /* [out] */ VARIANT __RPC_FAR *pVarResult,
        /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
        /* [out] */ UINT __RPC_FAR *puArgErr)
{
    HRESULT hr;
    
    hr = UPnPServiceDispatchImpl::Invoke(dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    
	if(DISP_E_MEMBERNOTFOUND == hr)
		hr = m_pServiceImpl->Invoke(dispIdMember, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    
    return hr;
}

