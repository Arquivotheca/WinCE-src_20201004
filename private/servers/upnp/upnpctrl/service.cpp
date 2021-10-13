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
#include "pch.h"
#pragma hdrstop

#include "Service.h"
#include "com_macros.h"
#include "HttpRequest.h"
#include "safe_array.hxx"
#include "messages.h"

Service::Service()
    :
    m_pServiceImpl(NULL),
    m_bstrUniqueDeviceName(NULL),
    m_bstrType(NULL),
    m_bstrId(NULL),
    m_bstrDescriptionURL(NULL),
    m_bstrControlURL(NULL),
    m_bstrEventsURL(NULL),
    m_nLifeTime(0),
    m_bInit(FALSE),
    m_ppszBaseURL(NULL)
{
}

Service::~Service()
{
    if (m_pServiceImpl)
    {
        delete m_pServiceImpl;
    }
}

STDMETHODIMP Service::Init(
    LPCWSTR pwszUniqueDeviceName,
    LPCWSTR pwszType,
    LPCWSTR pwszId,
    LPCWSTR pwszDescriptionURL,
    LPCWSTR pwszControlURL,
    LPCWSTR pwszEventsURL,
    UINT    nLifeTime,
    ce::string *ppszBaseURL)
{
    HRESULT hr = S_OK;
    
    ChkBool(pwszUniqueDeviceName, E_POINTER);
    ChkBool(pwszType, E_POINTER);
    ChkBool(pwszId, E_POINTER);
    ChkBool(pwszDescriptionURL, E_POINTER);
    ChkBool(pwszControlURL, E_POINTER);
    ChkBool(pwszEventsURL, E_POINTER);
    
    m_bstrUniqueDeviceName = SysAllocString(pwszUniqueDeviceName);
    ChkBool(m_bstrUniqueDeviceName, E_OUTOFMEMORY);
    
    m_bstrType = SysAllocString(pwszType);
    ChkBool(m_bstrType, E_OUTOFMEMORY);
    
    m_bstrId = SysAllocString(pwszId);
    ChkBool(m_bstrId, E_OUTOFMEMORY);
    
    m_bstrDescriptionURL = SysAllocString(pwszDescriptionURL);
    ChkBool(m_bstrDescriptionURL, E_OUTOFMEMORY);
    
    m_bstrControlURL = SysAllocString(pwszControlURL);
    ChkBool(m_bstrControlURL, E_OUTOFMEMORY);
    
    m_bstrEventsURL = SysAllocString(pwszEventsURL);
    ChkBool(m_bstrEventsURL, E_OUTOFMEMORY);

    m_nLifeTime = nLifeTime;
    
    // Note that the value of this will change later.
    m_ppszBaseURL = ppszBaseURL;
    
    hr = S_OK;

Cleanup:        
    return hr;
}

HRESULT Service::SetBaseURL()
{
    if (!m_ppszBaseURL)
    {
        return E_UNEXPECTED;
    }
    return m_pszBaseURL.assign(*m_ppszBaseURL) ? S_OK : E_OUTOFMEMORY;
}

HRESULT Service::EnsureServiceImpl()
{
    HRESULT hr = S_OK;
    
    // If we already have the impl, we are done.
    ChkBool(!m_pServiceImpl, S_OK);
    
    // Create the ServiceImpl
    m_pServiceImpl = new ServiceImpl();
    ChkBool(m_pServiceImpl, E_OUTOFMEMORY);

    // Initialize it (note that m_pszBaseURL has *changed* since the Service::Init was called)
    // TODO : remove the reference in the Service::Init method
    Chk(m_pServiceImpl->Init(
        m_bstrUniqueDeviceName,
        m_bstrType,
        m_bstrDescriptionURL,
        m_bstrControlURL,
        m_bstrEventsURL,
        m_nLifeTime,
        &m_pszBaseURL));
            
Cleanup:
    if (FAILED(hr))
    {
        if (m_pServiceImpl)
        {
            delete m_pServiceImpl;
            m_pServiceImpl = NULL;
        }
    }
    
    return hr;    
}

///////////////////////////////////
// IUPnPService methods

// QueryStateVariable
STDMETHODIMP Service::QueryStateVariable(
    /*[in]*/ BSTR bstrVariableName,
    /*[out, retval]*/ VARIANT *pValue)
{
    HRESULT hr;
    
    ChkBool(bstrVariableName, E_POINTER);
    ChkBool(pValue, E_POINTER);
    
    Chk(EnsureServiceImpl());
    ChkBool(m_pServiceImpl, E_UNEXPECTED);
    
    hr = m_pServiceImpl->QueryStateVariable(bstrVariableName, pValue);

    switch(hr)
    {
        case DISP_E_UNKNOWNNAME:
        case DISP_E_MEMBERNOTFOUND:
            hr = UPNP_E_INVALID_VARIABLE;
        break;

        case HRESULT_FROM_WIN32(ERROR_INTERNET_TIMEOUT):
            hr = UPNP_E_DEVICE_TIMEOUT;
        break;

        default:
        break;
    }
    
Cleanup:
    return hr;
}

// InvokeAction
STDMETHODIMP Service::InvokeAction(
    /*[in]*/ BSTR bstrActionName,
    /*[in]*/ VARIANT vInActionArgs,
    /*[in, out]*/ VARIANT * pvOutActionArgs,
    /*[out, retval]*/ VARIANT *pvRetVal)
{
    HRESULT hr = S_OK;
    
    ChkBool(bstrActionName, E_POINTER);
    
    hr = InvokeActionImpl(bstrActionName, vInActionArgs, pvOutActionArgs, pvRetVal);

    switch(hr)
    {
        case DISP_E_UNKNOWNNAME:
        case DISP_E_MEMBERNOTFOUND:
            hr = UPNP_E_INVALID_ACTION;
        break;

        case DISP_E_TYPEMISMATCH:
        case DISP_E_PARAMNOTFOUND:
            hr = UPNP_E_INVALID_ARGUMENTS;
        break;

        case HRESULT_FROM_WIN32(ERROR_INTERNET_TIMEOUT):
            hr = UPNP_E_DEVICE_TIMEOUT;
        break;

        default:
            if(XML_ERROR_MASK == (hr & XML_ERROR_MASK))
            {
                hr = UPNP_E_PARSE_ERROR;
            }
        break;
    }

Cleanup:
    return hr;
}


// InvokeActionImpl
HRESULT Service::InvokeActionImpl(
    /*[in]*/ BSTR bstrActionName,
    /*[in]*/ VARIANT vInActionArgs,
    /*[in, out]*/ VARIANT *pvOutActionArgs,
    /*[out, retval]*/ VARIANT *pvRetVal)
{
    HRESULT hr = S_OK;
    DISPID dispid;
    int nOutArgs, nInArgs, i, ubound;
    unsigned int position;
    UINT uArgErr;
    VARIANT *pvarInArgs = NULL;
    DISPPARAMS DispParams;
    ce::variant *rgvarg = NULL;
    ce::safe_array<ce::variant, VT_VARIANT> arrayInArguments;
    ce::safe_array<ce::variant, VT_VARIANT> arrayOutArguments;
    
    ChkBool(bstrActionName, E_POINTER);
    ChkBool(pvOutActionArgs, E_POINTER);
    // ChkBool(pvRetVal, E_POINTER); - not used here

    Chk(EnsureServiceImpl());
    ChkBool(m_pServiceImpl, E_UNEXPECTED);
    
    Chk(m_pServiceImpl->GetIDsOfNames(&bstrActionName, 1, &dispid));

    // dereference [in] arguments variant
    for (pvarInArgs = &vInActionArgs; pvarInArgs->vt == (VT_VARIANT | VT_BYREF); pvarInArgs = pvarInArgs->pvarVal)
    {
        // All this does is find the first non-(VT_VARIANT|VT_BYREF) in the vInActionArgs VARIANT
    }

    // attach to [in] arguments array
    if(pvarInArgs->vt == (VT_ARRAY | VT_VARIANT))
    {
        arrayInArguments.attach(pvarInArgs->parray);
    }
    else
    {
        if(pvarInArgs->vt == (VT_BYREF | VT_ARRAY | VT_VARIANT))
        {
            ChkBool(pvarInArgs->pparray, E_POINTER);

            arrayInArguments.attach(*pvarInArgs->pparray);
        }
        else
        {
            Chk(DISP_E_TYPEMISMATCH);
        }
    }

    nOutArgs = m_pServiceImpl->GetActionOutArgumentsCount(dispid);

    if(nOutArgs)
    {
        ChkBool(pvOutActionArgs, E_POINTER);

        // create [out] arguments safe array
        arrayOutArguments.create(nOutArgs);
    }

    nInArgs = arrayInArguments.size();

    DispParams.cArgs = nInArgs + nOutArgs;
    DispParams.cNamedArgs = 0;
    DispParams.rgdispidNamedArgs = NULL;

    rgvarg = new ce::variant[DispParams.cArgs];
    ChkBool(rgvarg, E_OUTOFMEMORY);

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

Cleanup:
    if(rgvarg)
    {
        delete[] rgvarg;
    }

    return hr;
}

// ServiceTypeIdentifier
STDMETHODIMP Service::get_ServiceTypeIdentifier(/*[out, retval]*/ BSTR *pVal)
{
    HRESULT hr = S_OK;
    BSTR bstrVal = NULL;
    
    ChkBool(pVal, E_POINTER);
    ChkBool(m_bstrType, E_UNEXPECTED);
    
    bstrVal = SysAllocString(m_bstrType);
    
    ChkBool(bstrVal, E_OUTOFMEMORY);

    *pVal = bstrVal;
    
Cleanup:
    return hr;
}

// Id
STDMETHODIMP Service::get_Id(/*[out, retval]*/ BSTR *pVal)
{
    HRESULT hr = S_OK;
    BSTR bstrVal = NULL;
    
    ChkBool(pVal, E_POINTER);
    ChkBool(m_bstrId, E_UNEXPECTED);

    bstrVal = SysAllocString(m_bstrId);
    
    ChkBool(bstrVal, E_OUTOFMEMORY);
    
    *pVal = bstrVal;
    
Cleanup:
    return hr;
}

// LastTransportStatus
STDMETHODIMP Service::get_LastTransportStatus(/*[out, retval]*/ long* plValue)
{
    HRESULT hr = S_OK;
    
    ChkBool(plValue, E_POINTER);
    
    Chk(EnsureServiceImpl());
    ChkBool(m_pServiceImpl, E_UNEXPECTED);

    *plValue = m_pServiceImpl->GetLastTransportStatus();

Cleanup:
    return hr;
}

// AddCallback
STDMETHODIMP Service::AddCallback(/*[in]*/ IUnknown* pUnkCallback, /*[out]*/ DWORD *pdwCookie)
{
    HRESULT hr = S_OK;
    
    ChkBool(pUnkCallback, E_POINTER);
    ChkBool(pdwCookie, E_POINTER);
    
    Chk(EnsureServiceImpl());
    ChkBool(m_pServiceImpl, E_UNEXPECTED);
    
    Chk(m_pServiceImpl->AddCallback(this, pUnkCallback, pdwCookie));
    
Cleanup:
    return hr;
}

// RemoveCallback
STDMETHODIMP Service::RemoveCallback(/*[in]*/ DWORD dwCookie)
{
    HRESULT hr = S_OK;
    
    Chk(EnsureServiceImpl());
    ChkBool(m_pServiceImpl, E_UNEXPECTED);
    
    Chk(m_pServiceImpl->RemoveCallback(dwCookie));
    
Cleanup:
    return hr;
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
    HRESULT hr = S_OK;
    
    Chk(EnsureServiceImpl());
    ChkBool(m_pServiceImpl, E_UNEXPECTED);

    hr = UPnPServiceDispatchImpl::GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);

    if(DISP_E_UNKNOWNNAME == hr)
    {
        hr = m_pServiceImpl->GetIDsOfNames(rgszNames, cNames, rgDispId);
    }

Cleanup:
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
    HRESULT hr = S_OK;
    
    Chk(EnsureServiceImpl());
    ChkBool(m_pServiceImpl, E_UNEXPECTED);
    
    hr = UPnPServiceDispatchImpl::Invoke(dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    if(DISP_E_MEMBERNOTFOUND == hr)
    {
        hr = m_pServiceImpl->Invoke(dispIdMember, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    }

Cleanup:
    return hr;
}

