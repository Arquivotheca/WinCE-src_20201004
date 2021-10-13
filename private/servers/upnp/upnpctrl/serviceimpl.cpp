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

#include "ssdpapi.h"
#include "upnp.h"
#include "url_verifier.h"
#include "upnp_config.h"

#include "ServiceImpl.h"
#include "com_macros.h"
#include "HttpChannel.h"
#include "HttpRequest.h"
#include "dlna.h"

extern url_verifier* g_pURL_Verifier;

// ServiceImpl
ServiceImpl::ServiceImpl()
    :
    m_dwSubscriptionCookie(0),
    m_pUPnPService(NULL),
    m_bServiceInstanceDied(false),
    m_bParsedRootElement(false),
    m_bSubscribed(false),
    m_pActionQueryStateVar(NULL),
    m_pStateVarName(NULL),
    m_bRetval(false),
    m_bSendEvents(false),
    m_pCurrentAction(NULL),
    m_dwChannelCookie(0),
    m_lLastTransportStatus(0)
{
}

// ~ServiceImpl
ServiceImpl::~ServiceImpl()
{
    Uninit();
}

void
ServiceImpl::Uninit()
{
    if (m_dwSubscriptionCookie && g_pConnectionPoint != NULL)
    {
        g_pConnectionPoint->unadvise(m_dwSubscriptionCookie);
        m_dwSubscriptionCookie = 0;
    }
    
    if (m_dwChannelCookie)
    {
        g_CJHttpChannel.Free(m_dwChannelCookie);
        m_dwChannelCookie = 0;
    }
    
    if (m_pActionQueryStateVar)
    {
        delete m_pActionQueryStateVar;
        m_pActionQueryStateVar = NULL;
    }
    
    if (m_pStateVarName)
    {
        delete m_pStateVarName;
        m_pStateVarName = NULL;
    }
}

// Init
HRESULT ServiceImpl::Init(
    LPCWSTR pwszUniqueDeviceName,
    LPCWSTR pwszServiceType, 
    LPCWSTR pwszDescriptionURL,
    LPCWSTR pwszControlURL, 
    LPCWSTR pwszEventsURL,
    UINT    nLifeTime,
    ce::string *pszBaseURL)
{
    HRESULT hr = S_OK;
    HttpRequest *pRequest = NULL;
    DWORD dwRequestCookie = 0;
    Argument arg(L"u:varName", L"", false);

    // fabricate QueryStateVariable "action"
    char pszAbsoluteURL[INTERNET_MAX_URL_LENGTH];
    DWORD dw;

    ChkBool(pwszUniqueDeviceName, E_POINTER);
    ChkBool(pwszServiceType, E_POINTER);
    ChkBool(pwszDescriptionURL, E_POINTER);
    ChkBool(pwszControlURL, E_POINTER);
    ChkBool(pwszEventsURL, E_POINTER);
    ChkBool(pszBaseURL, E_POINTER);
    
    // g_pConnectionPoint has to be valid by the time this is called
    ChkBool(g_pConnectionPoint, E_UNEXPECTED);
    
    // Use this to key that the service is already initialized
    ChkBool(!m_pActionQueryStateVar, E_UNEXPECTED);
    
    m_pActionQueryStateVar = new Action(L"QueryStateVariable", L"urn:schemas-upnp-org:control-1-0");
    ChkBool(m_pActionQueryStateVar, E_OUTOFMEMORY);
    
    m_pStateVarName = new StateVar(L"", StateVar::string, false);
    ChkBool(m_pStateVarName, E_OUTOFMEMORY);

    ChkBool(m_dwChannelCookie == 0, E_UNEXPECTED);
    Chk(g_CJHttpChannel.Alloc(&m_dwChannelCookie));

    
    // TODO : The Argument class can fail to allocate
    arg.BindStateVar(m_pStateVarName);

    // Argument's push_back can fail - this has to copy the Argument parameter
    m_pActionQueryStateVar->AddInArgument(arg);
    m_pActionQueryStateVar->AddOutArgument(Argument(L"return", L"", true));

    // init data
    ChkBool(m_strUniqueServiceName.assign(pwszUniqueDeviceName), E_OUTOFMEMORY);
    ChkBool(m_strUniqueServiceName.append(L"::"), E_OUTOFMEMORY);
    ChkBool(m_strUniqueServiceName.append(pwszServiceType), E_OUTOFMEMORY);

    ChkBool(m_strType.assign(pwszServiceType), E_OUTOFMEMORY);
    
    // TODO : These can fail to allocate internally, but the function as defined
    // in string.hxx does not handle the allocation failure!
    ce::WideCharToMultiByte(CP_UTF8, pwszDescriptionURL, -1, &m_strDescriptionURL);
    ce::WideCharToMultiByte(CP_UTF8, pwszControlURL, -1, &m_strControlURL);
    ce::WideCharToMultiByte(CP_UTF8, pwszEventsURL, -1, &m_strEventsURL);

    // advise to connection point
    Chk(g_pConnectionPoint->advise(m_strUniqueServiceName, nLifeTime, this, &m_dwSubscriptionCookie));

    // convert URLs to absolute
    dw = _countof(pszAbsoluteURL);
    ChkBool(InternetCombineUrlA(*pszBaseURL, m_strDescriptionURL, pszAbsoluteURL, &dw, 0), E_FAIL);
    ChkBool(m_strDescriptionURL.assign(pszAbsoluteURL), E_OUTOFMEMORY);
    ChkBool(g_pURL_Verifier->is_url_ok(m_strDescriptionURL), E_FAIL);

    dw = _countof(pszAbsoluteURL);
    ChkBool(InternetCombineUrlA(*pszBaseURL, m_strControlURL, pszAbsoluteURL, &dw, 0), E_FAIL);
    ChkBool(m_strControlURL.assign(pszAbsoluteURL), E_OUTOFMEMORY);
    ChkBool(g_pURL_Verifier->is_url_ok(m_strControlURL), E_FAIL);
    
    dw = _countof(pszAbsoluteURL);
    ChkBool(InternetCombineUrlA(*pszBaseURL, m_strEventsURL, pszAbsoluteURL, &dw, 0), E_FAIL);
    ChkBool(m_strEventsURL.assign(pszAbsoluteURL), E_OUTOFMEMORY);
    ChkBool(g_pURL_Verifier->is_url_ok(m_strEventsURL), E_FAIL);
    
    Chk(g_CJHttpRequest.Alloc(&dwRequestCookie));
    Chk(g_CJHttpRequest.Acquire(dwRequestCookie, pRequest));
    Chk(pRequest->Init(m_dwChannelCookie, m_strDescriptionURL));
    Chk(pRequest->Open("GET"));
    Chk(pRequest->Send());
    ChkBool(hr == S_OK, E_FAIL);

    { // limited scope
        ce::SAXReader Reader;
        ce::SequentialStream<HttpRequest> Stream(*pRequest, upnp_config::max_document_size());
        ISAXXMLReader *pXMLReader = NULL;
        ce::variant v;
        Chk(Reader.Init());

        pXMLReader = Reader.GetXMLReader();
        ChkBool(pXMLReader, E_UNEXPECTED);

        v.vt = VT_UNKNOWN;
        Chk(Stream.QueryInterface(IID_ISequentialStream, (void**)&v.punkVal));
        Chk(pXMLReader->putContentHandler(this));
        Chk(pXMLReader->parse(v));
        Chk(pXMLReader->putContentHandler(NULL));
    }
    
    if (wcscmp(m_strSpecVersionMajor, L"1") != 0)
    {
        TraceTag(ttidError, "ServiceImpl: Invalid document version");
        ChkBool(false, E_FAIL);
    }

    // Bind state variables to action arguments
    for(ce::vector<Action>::iterator it = m_Actions.begin(), itEnd = m_Actions.end(); it != itEnd; ++it)
    {
        // TODO : can this fail?
        it->BindArgumentsToStateVars(m_StateVars.begin(), m_StateVars.end());
    }

Cleanup:
    g_CJHttpRequest.Release(dwRequestCookie, pRequest);
    g_CJHttpRequest.Free(dwRequestCookie);
    if (FAILED(hr))
    {
        Uninit();
    }
    return hr;
}

// AddCallback
HRESULT ServiceImpl::AddCallback(IUPnPService* pUPnPService, IUnknown *punkCallback, DWORD* pdwCookie)
{
    HRESULT hr = S_OK;
    IDispatch *pDispatch = NULL;
    IUPnPServiceCallback *pServiceCallback = NULL;    
    callback c;
    
    ChkBool(pUPnPService, E_POINTER);
    ChkBool(punkCallback, E_POINTER);
    ChkBool(pdwCookie, E_POINTER);
    
    // Init has not been called yet if this is not allocated
    ChkBool(m_pActionQueryStateVar, E_UNEXPECTED);
    
    // Without a subscription cookie, we cannot subscribe
    ChkBool(m_dwSubscriptionCookie, E_UNEXPECTED);
    ChkBool(g_pConnectionPoint, E_UNEXPECTED);
    
    if(!m_bSubscribed)
    {
        Chk(g_pConnectionPoint->subscribe(m_dwSubscriptionCookie, m_strEventsURL));
        m_bSubscribed = true;
    }

    if(SUCCEEDED(punkCallback->QueryInterface(IID_IUPnPServiceCallback, (void**)&pServiceCallback)))
    {
        c.m_type = callback::upnp_service_callback;
        c.m_pUPnPServiceCallback = pServiceCallback;
    }
    else
    {
        // TODO : Remove the IDispatch callback. Should only take IUPnPServiceCallback
        if(SUCCEEDED(punkCallback->QueryInterface(IID_IDispatch, (void**)&pDispatch)))
        {
            c.m_type = callback::dispatch;
            c.m_pDispatch = pDispatch;
        }
        else
        {
            Chk(E_FAIL);
        }
    }

    Assert(m_pUPnPService == NULL || m_pUPnPService == pUPnPService);
    
    // Not sure why the "outer" pointer is needed here.
    // TODO : Service class should just implement this method directly
    m_pUPnPService = pUPnPService;
    
    {
        ce::gate<ce::critical_section> _gate(m_csListCallback);

        ChkBool(m_listCallback.push_front(c), E_OUTOFMEMORY);
        
        pDispatch = NULL;
        pServiceCallback = NULL;

        // return iterator as cookie
        Assert(sizeof(ce::list<callback>::iterator) == sizeof(DWORD));
        *((ce::list<callback>::iterator*)pdwCookie) = m_listCallback.begin();
    }
    
Cleanup:
    if (FAILED(hr))
    {
        // Release reference from the QI
        if (pDispatch)
        {
            pDispatch->Release();
        }
        
        // Release reference from the QI
        if (pServiceCallback)
        {
            pServiceCallback->Release();
        }
    }
    
    return hr;
}


// RemoveCallback
HRESULT ServiceImpl::RemoveCallback(DWORD dwCookie)
{
    HRESULT hr = S_OK;
    
    // Init has not been called yet if this is not allocated
    ChkBool(m_pActionQueryStateVar, E_UNEXPECTED);
    
    Assert(sizeof(ce::list<callback>::iterator) == sizeof(dwCookie));

    {
        ce::list<callback>::iterator itCallback = *((ce::list<callback>::iterator*)&dwCookie);
        ce::gate<ce::critical_section> _gate(m_csListCallback);
        
        // Return failure if the item is not found
        hr = E_FAIL;

        // can't assume that dwCookie is a valid iterator so I look for it in the list
        for(ce::list<callback>::iterator it = m_listCallback.begin(), itEnd = m_listCallback.end(); it != itEnd; ++it)
        {
            if(it == itCallback)
            {
                m_listCallback.erase(it);
                
                hr = S_OK;
                break;
            }
        }
    }
    
Cleanup:
    return hr;
}

// StateVariableChanged
void ServiceImpl::StateVariableChanged(LPCWSTR pwszName, LPCWSTR pwszValue)
{
    ce::variant varValue;
    ce::vector<StateVar>::iterator itStateVar, itEndStateVar;
    
    // find the state variable
    for(itStateVar = m_StateVars.begin(), itEndStateVar = m_StateVars.end(); itStateVar != itEndStateVar; ++itStateVar)
    {
        if(0 == wcscmp(itStateVar->GetName(), pwszName))
        {
            itStateVar->Decode(pwszValue, &varValue);
            break;
        }
    }
    
    if(itStateVar != itEndStateVar)
    {
        ce::gate<ce::critical_section> _gate(m_csListCallback);

        for(ce::list<callback>::iterator it = m_listCallback.begin(), itEnd = m_listCallback.end(); it != itEnd; ++it)
        {
            // TODO : Remove the IDispatch from the callback type - should only accept the IUPnPServiceCallback
            if(it->m_type == callback::upnp_service_callback)
            {
                // IUPnPServiceCallback callback
                it->m_pUPnPServiceCallback->StateVariableChanged(m_pUPnPService, pwszName, varValue);
            }
            else
            {
                // IDispatch callback
                Assert(it->m_type == callback::dispatch);

                DISPPARAMS DispParams = {0};
                VARIANT rgvarg[4];
                UINT uArgErr;
                
                DispParams.cArgs = 4;
                DispParams.rgvarg = rgvarg;

                rgvarg[3].vt = VT_BSTR;
                rgvarg[3].bstrVal = SysAllocString(L"VARIABLE_UPDATE");
                
                // don't AddRef m_pUPnPService
                rgvarg[2].vt = VT_DISPATCH;
                rgvarg[2].pdispVal = m_pUPnPService;
                
                rgvarg[1].vt = VT_BSTR;
                rgvarg[1].bstrVal = SysAllocString(pwszName);

                VariantInit(&rgvarg[0]);
                
                if(SUCCEEDED(VariantCopy(&rgvarg[0], &varValue)))
                {
                    it->m_pDispatch->Invoke(DISPID_VALUE, IID_NULL, 0, DISPATCH_METHOD, &DispParams, NULL, NULL, &uArgErr);
                    
                    VariantClear(&rgvarg[0]);
                }
                
                VariantClear(&rgvarg[1]);
                // don't clear rgvarg[2] since we didn't AddRef it
                VariantClear(&rgvarg[3]);
            }
        }
    }
}

// AliveNotification
void ServiceImpl::AliveNotification(LPCWSTR pszUSN, LPCWSTR pszLocation, LPCWSTR pszAL, DWORD dwLifeTime)
{
}

// ServiceInstanceDied
void ServiceImpl::ServiceInstanceDied(LPCWSTR pszUSN)
{
    if(!m_bServiceInstanceDied)
    {
        ce::gate<ce::critical_section> _gate(m_csListCallback);

        m_bServiceInstanceDied = true;
        
        for(ce::list<callback>::iterator it = m_listCallback.begin(), itEnd = m_listCallback.end(); it != itEnd; ++it)
        {
            if(it->m_type == callback::upnp_service_callback)
            {
                // IUPnPServiceCallback callback
                it->m_pUPnPServiceCallback->ServiceInstanceDied(m_pUPnPService);
            }
            else
            {
                // TODO : Remove the IDispatch callback
                // IDispatch callback
                Assert(it->m_type == callback::dispatch);

                DISPPARAMS  DispParams = {0};
                VARIANT     rgvarg[2];
                UINT        uArgErr;
                
                DispParams.cArgs = 2;
                DispParams.rgvarg = rgvarg;

                rgvarg[1].vt = VT_BSTR;
                rgvarg[1].bstrVal = SysAllocString(L"SERVICE_INSTANCE_DIED");
                
                // don't AddRef m_pUPnPService
                rgvarg[0].vt = VT_DISPATCH;
                rgvarg[0].pdispVal = m_pUPnPService;

                it->m_pDispatch->Invoke(DISPID_VALUE, IID_NULL, 0, DISPATCH_METHOD, &DispParams, NULL, NULL, &uArgErr);

                VariantClear(&rgvarg[1]);
                // don't clear rgvarg[0]
            }
        }
    }
}

// local_DispGetParamPtr
static HRESULT local_DispGetParamPtr(DISPPARAMS FAR *pDispParams, unsigned int position, VARIANTARG** ppVar, unsigned int FAR *puArgErr)
{
    HRESULT hr = S_OK;
    unsigned int cPositionArgs = 0;
    
    ChkBool(pDispParams, E_POINTER);
    ChkBool(ppVar, E_POINTER);
    ChkBool(puArgErr, E_POINTER);
    
    cPositionArgs = pDispParams->cArgs - pDispParams->cNamedArgs;

    // check if the argument is among positional arguments
    if(position < cPositionArgs)
    {
        *ppVar = &pDispParams->rgvarg[pDispParams->cArgs - position - 1];
        hr = S_OK;
        goto Cleanup;
    }

    // look for the argument among named arguments
    for(unsigned int i = 0; i < pDispParams->cNamedArgs; ++i)
    {
        if(static_cast<DISPID>(position) == pDispParams->rgdispidNamedArgs[i])
        {
            *ppVar = &pDispParams->rgvarg[i];
            hr = S_OK;
            goto Cleanup;
        }
    }
    
    // argument not found
    *puArgErr = position;
    hr = DISP_E_PARAMNOTFOUND;

Cleanup:
    return hr;
}

// GetIDsOfNames
HRESULT ServiceImpl::GetIDsOfNames(
    OLECHAR FAR *FAR *rgszNames,
    unsigned int cNames, 
    DISPID FAR *rgDispId)
{
    HRESULT hr = S_OK;
    unsigned int i;

    // Init has not been called yet if this is not allocated
    ChkBool(m_pActionQueryStateVar, E_UNEXPECTED);
    
    ChkBool(cNames, S_OK);
    ChkBool(rgszNames, E_POINTER);
    ChkBool(rgDispId, E_POINTER);

    // initialize all the names to unknown
    for(i = 0; i < cNames; ++i)
    {
        rgDispId[i] = DISPID_UNKNOWN;
    }

    ce::vector<Action>::iterator it, itEnd;

    // first name is method/property name
    
    // actions (methods)
    for(it = m_Actions.begin(), itEnd = m_Actions.end(); it != itEnd; ++it)
    {
        if(0 == wcscmp(it->GetName(), rgszNames[0]))
        {
            rgDispId[0] = base_dispid + (it - m_Actions.begin());
            break;
        }
    }

    if(it != itEnd)
    {
        // if there are more names they are those of method arguments
        // check [in] arguments
        for(i = 1; i < cNames; ++i)
        {
            for(int j = 0; j < it->GetInArgumentsCount(); ++j)
            {
                if(0 == wcscmp(it->GetInArgument(j).GetName(), rgszNames[i]))
                {
                    Assert(rgDispId[i] == DISPID_UNKNOWN);
                    
                    rgDispId[i] = j;
                    break;
                }
            }
        }

        // check [out] arguments
        for(i = 1; i < cNames; ++i)
        {
            for(int j = 0, position = it->GetInArgumentsCount(); j < it->GetOutArgumentsCount(); ++j, ++position)
            {
                if(it->GetOutArgument(j).IsRetval())
                {
                    // retval doesn't count as argument in IDispatch
                    --position;
                    continue;
                }
                
                if(0 == wcscmp(it->GetOutArgument(j).GetName(), rgszNames[i]))
                {
                    Assert(rgDispId[i] == DISPID_UNKNOWN);

                    rgDispId[i] = position;
                    break;
                }
            }
        }
    }
    else
    {
        // state variables (properties)
        for(ce::vector<StateVar>::iterator itStateVar = m_StateVars.begin(), itEndStateVar = m_StateVars.end(); itStateVar != itEndStateVar; ++itStateVar)
        {
            if(0 == wcscmp(itStateVar->GetName(), rgszNames[0]))
            {
                rgDispId[0] = base_dispid + (itStateVar - m_StateVars.begin()) + m_Actions.size();
                break;
            }
        }
    }
        
    // check if any names remain unknown
    for(i = 0; i < cNames; ++i)
    {
        if(rgDispId[i] == DISPID_UNKNOWN)
        {
            hr = DISP_E_UNKNOWNNAME;
            break;
        }
    }
    
Cleanup:
    return hr;
}

// Invoke
HRESULT ServiceImpl::Invoke(
    DISPID dispIdMember, 
    WORD wFlags, 
    DISPPARAMS FAR *pDispParams, 
    VARIANT FAR *pVarResult, 
    EXCEPINFO FAR *pExcepInfo, 
    unsigned int FAR *puArgErr)
{
    HRESULT hr = S_OK;
    Action * pAction = NULL;
    ce::variant var;
    int i;
    unsigned int position;
    HttpRequest *pRequest = NULL;
    DWORD dwRequestCookie = 0;

    // Init has not been called yet if this is not allocated
    ChkBool(m_pActionQueryStateVar, E_UNEXPECTED);

    if(wFlags & ~(DISPATCH_PROPERTYGET | DISPATCH_METHOD | DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF))
    {
        Chk(E_INVALIDARG);
    }
    
    if(dispIdMember < base_dispid && dispIdMember != query_state_var_dispid)
    {
        Chk(DISP_E_MEMBERNOTFOUND);
    }

    // propput not supported by UPnP
    if((wFlags == DISPATCH_PROPERTYPUT) || (wFlags == DISPATCH_PROPERTYPUTREF))
    {
        Chk(DISP_E_MEMBERNOTFOUND);
    }
    
    if(dispIdMember >= base_dispid + m_Actions.size())
    {
        // from dispid it can only be propget
        if(!(wFlags & DISPATCH_PROPERTYGET))
        {
            Chk(DISP_E_MEMBERNOTFOUND);
        }
                
        // check if there is a state variable for this dispid
        if(dispIdMember - base_dispid - m_Actions.size() >= m_StateVars.size())
        {
            Chk(DISP_E_MEMBERNOTFOUND);
        }

        Chk(QueryStateVariable(m_StateVars[dispIdMember - base_dispid - m_Actions.size()].GetName(), pVarResult));
        
        goto Cleanup;
    }
    else
    {
        // from dispid it can only be a method
        if(!(wFlags & DISPATCH_METHOD))
        {
            Chk(DISP_E_MEMBERNOTFOUND);
        }

        if(dispIdMember == query_state_var_dispid)
        {
            pAction = m_pActionQueryStateVar;
        }
        else
        {
            assert(dispIdMember >= base_dispid && dispIdMember < base_dispid + m_Actions.size());
                
            pAction = &m_Actions[dispIdMember - base_dispid];
        }
    }

    if(pDispParams->cArgs != pAction->GetInArgumentsCount() + GetActionOutArgumentsCount(dispIdMember))
    {
        Chk(DISP_E_BADPARAMCOUNT);
    }
    
    // init [in] arguments
    for(i = 0, position = 0; i < pAction->GetInArgumentsCount(); ++i, ++position)
    {
        Argument& arg = pAction->GetInArgument(i);

        Chk(DispGetParam(pDispParams, position, arg.GetVartype(), &var, puArgErr));
        
        arg.SetValue(var);
    }

    // Send SOAP Request
    Chk(g_CJHttpRequest.Alloc(&dwRequestCookie));
    Chk(g_CJHttpRequest.Acquire(dwRequestCookie, pRequest));
    Chk(pRequest->Init(m_dwChannelCookie, m_strControlURL));
    Chk(pRequest->SendMessage(pAction->GetSoapActionName(), pAction->CreateSoapMessage()));
    m_lLastTransportStatus = pRequest->GetHttpStatus();

    // Parse SOAP response
    Chk(pAction->ParseSoapResponse(*pRequest));
    
    if(IsHttpStatusOK(m_lLastTransportStatus))
    {
        // return [out] arguments
        for(i = 0, position = pAction->GetInArgumentsCount(); i < pAction->GetOutArgumentsCount(); ++i, ++position)
        {
            Argument&   arg = pAction->GetOutArgument(i);
            ce::variant varValue;
            VARIANTARG* pvarOut = NULL;

            Chk(arg.GetValue(&varValue));
        
            if(arg.IsRetval())
            {
                ChkBool(pVarResult, E_POINTER);
                
                // return [out] argument as result of the Invoke
                Chk(VariantCopy(pVarResult, &varValue));
            
                // retval doesn't count as argument in IDispatch
                --position;
                continue;
            }

            // get pointer to variant for the [out] argument
            Chk(local_DispGetParamPtr(pDispParams, position, &pvarOut, puArgErr));

            // if [out] argument is VT_BYREF | VT_VARIANT then just return value in it
            if(pvarOut->vt == (VT_BYREF | VT_VARIANT))
            {
                Chk(VariantCopy(V_VARIANTREF(pvarOut), &varValue));

                continue;
            }

            // check if [out] parameter is a reference to proper type
            if((pvarOut->vt ^ VT_BYREF) != varValue.vt)
            {
                ChkBool(puArgErr, E_POINTER);

                *puArgErr = position;
                
                Chk(DISP_E_TYPEMISMATCH);
            }

            Assert((pvarOut->vt ^ VT_BYREF) == varValue.vt);
        
            switch(varValue.vt)
            {
                case VT_UI1 :
                    *V_UI1REF(pvarOut) = V_UI1(&varValue);
                break;
                
                case VT_UI2 :
                    *V_UI2REF(pvarOut) = V_UI2(&varValue);
                break;
                
                case VT_UI4 :
                    *V_UI4REF(pvarOut) = V_UI4(&varValue);
                break;
                
                case VT_I1  :
                    *V_I1REF(pvarOut)  = V_I1(&varValue);
                break;
                
                case VT_I2  :
                    *V_I2REF(pvarOut)  = V_I2(&varValue);
                break;
                
                case VT_I4  :
                    *V_I4REF(pvarOut)  = V_I4(&varValue);
                break;
                
                case VT_R4  :
                    *V_R4REF(pvarOut)  = V_R4(&varValue);
                break;
                
                case VT_R8  :
                    *V_R8REF(pvarOut)  = V_R8(&varValue);
                break;
                
                case VT_CY  :
                    *V_CYREF(pvarOut)  = V_CY(&varValue);
                break;
                
                case VT_BSTR: 
                    VariantClear(pvarOut);
                    *V_BSTRREF(pvarOut) = V_BSTR(&varValue);
                break;

                case VT_DATE:
                    *V_DATEREF(pvarOut) = V_DATE(&varValue);
                break;
                
                case VT_BOOL:
                    *V_BOOLREF(pvarOut) = V_BOOL(&varValue);
                break;

                case VT_ARRAY | VT_UI1:
                    VariantClear(pvarOut);
                    Chk(SafeArrayCopy(V_ARRAY(&varValue), V_ARRAYREF(pvarOut)));
                break;
            
                default:
                    Assert(0);
                break;
            }
        }

        Assert(IsHttpStatusOK(m_lLastTransportStatus));

        hr = S_OK;
        goto Cleanup;
    }

    Assert(!IsHttpStatusOK(m_lLastTransportStatus));
        
    if(m_lLastTransportStatus != HTTP_STATUS_SERVER_ERROR)
    {
        Chk(UPNP_E_TRANSPORT_ERROR);
    }

    switch(pAction->GetFaultCode())
    {
        case 0:
            hr = UPNP_E_PROTOCOL_ERROR;
        break;
        
        case 401:
            hr = UPNP_E_INVALID_ACTION;
        break;
        
        case 402:
            hr = UPNP_E_INVALID_ARGUMENTS;
        break;
        
        case 403:
            hr = UPNP_E_DEVICE_ERROR;
        break;
        
        case 404:
            hr = UPNP_E_INVALID_VARIABLE;
        break;
        
        case 501:
            hr = UPNP_E_ACTION_REQUEST_FAILED;
        break;
        
        default:
            hr = UPNP_E_ACTION_SPECIFIC_BASE + (pAction->GetFaultCode() - FAULT_ACTION_SPECIFIC_BASE);
        break;
    }
    
Cleanup:
    g_CJHttpRequest.Release(dwRequestCookie, pRequest);
    g_CJHttpRequest.Free(dwRequestCookie);
    return hr;
}

// QueryStateVariable
HRESULT ServiceImpl::QueryStateVariable(LPCWSTR pwszVariableName, VARIANT *pValue)
{
    HRESULT hr = S_OK;
    DISPPARAMS DispParams;
    ce::variant varStateVar;
    unsigned int uArgErr;

    // Init has not been called yet if this is not allocated
    ChkBool(m_pActionQueryStateVar, E_UNEXPECTED);
    
    ce::vector<StateVar>::iterator it, itEnd;

    for(it = m_StateVars.begin(), itEnd = m_StateVars.end(); it != itEnd; ++it)
    {
        if(0 == wcscmp(pwszVariableName, it->GetName()))
        {
            break;
        }
    }

    if(it == itEnd)
    {
        Chk(UPNP_E_INVALID_VARIABLE);
    }

    // bind [out] argument to the state variable
    m_pActionQueryStateVar->GetOutArgument(0).BindStateVar(&*it);
    
    // fabricate a DispParams structure
    DispParams.cArgs = 1;
    DispParams.cNamedArgs = 0;
    DispParams.rgdispidNamedArgs = NULL;
    DispParams.rgvarg = &(varStateVar = it->GetName());
    
    Chk(Invoke(query_state_var_dispid, DISPATCH_METHOD, &DispParams, pValue, NULL, &uArgErr));
    
Cleanup:
    return hr;
}

// GetActionOutArgumentsCount
int ServiceImpl::GetActionOutArgumentsCount(DISPID dispidAction)
{
    HRESULT hr = S_OK;

    int nCount = 0;
    
    // Init has not been called yet if this is not allocated
    ChkBool(m_pActionQueryStateVar, E_UNEXPECTED);
    
    if(dispidAction >= base_dispid && dispidAction < base_dispid + m_Actions.size())
    {
        Action* pAction = &m_Actions[dispidAction - base_dispid];

        // count not-retval [out] arguments
        for(int i = 0; i < pAction->GetOutArgumentsCount(); ++i)
        {
            if(!pAction->GetOutArgument(i).IsRetval())
            {
                ++nCount;
            }
        }
    }
    
 Cleanup:
    return nCount;
} 
