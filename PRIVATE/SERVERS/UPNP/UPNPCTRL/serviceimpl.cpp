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

#include "ssdpapi.h"
#include "upnp.h"
#include "url_verifier.h"
#include "upnp_config.h"

#include "ServiceImpl.h"
#include "SoapRequest.h"
#include "com_macros.h"
#include "HttpRequest.h"

extern url_verifier* g_pURL_Verifier;

// ServiceImpl
ServiceImpl::ServiceImpl(LPCWSTR pwszUniqueDeviceName,
						 LPCWSTR pwszServiceType, 
						 LPCWSTR pwszDescriptionURL,
                         LPCWSTR pwszControlURL, 
                         LPCWSTR pwszEventsURL,
						 UINT    nLifeTime)
    : m_actionQueryStateVar(L"QueryStateVariable", L"urn:schemas-upnp-org:control-1-0"),
      m_statevarVarName(L"", StateVar::string, false),
	  m_dwSubscriptionCookie(0),
	  m_pUPnPService(NULL),
      m_bServiceInstanceDied(false),
      m_bParsedRootElement(false),
      m_hrInitResult(E_UNEXPECTED),
	  m_bSubscribed(false)
{
	// fabricate QueryStateVariable "action"
    Argument arg(L"u:varName", L"", false);
    
    arg.BindStateVar(&m_statevarVarName);

    m_actionQueryStateVar.AddInArgument(arg);
    m_actionQueryStateVar.AddOutArgument(Argument(L"return", L"", true));

	// init data
	m_strUniqueServiceName = pwszUniqueDeviceName;
	m_strUniqueServiceName += L"::";
	m_strUniqueServiceName += pwszServiceType;

	m_strType = pwszServiceType;
    
	ce::WideCharToMultiByte(CP_UTF8, pwszDescriptionURL, -1, &m_strDescriptionURL);
	
	ce::WideCharToMultiByte(CP_UTF8, pwszControlURL, -1, &m_strControlURL);

	ce::WideCharToMultiByte(CP_UTF8, pwszEventsURL, -1, &m_strEventsURL);

	// advise to connection point
	m_hrAdviseResult = g_pConnectionPoint->advise(m_strUniqueServiceName, nLifeTime, this, &m_dwSubscriptionCookie);
}


// ~ServiceImpl
ServiceImpl::~ServiceImpl()
{
	Assert(m_dwSubscriptionCookie == 0);
}


// UnadviseConnectionPoint
void ServiceImpl::UnadviseConnectionPoint()
{
    if(m_dwSubscriptionCookie)
		g_pConnectionPoint->unadvise(m_dwSubscriptionCookie);

    m_dwSubscriptionCookie = 0;
}


// Init
HRESULT ServiceImpl::Init(const ce::string& strBaseURL)
{
    // convert URLs to absolute
    char    pszAbsoluteURL[INTERNET_MAX_URL_LENGTH];
    DWORD   dw;
    
    InternetCombineUrlA(strBaseURL, m_strDescriptionURL, pszAbsoluteURL, &(dw = sizeof(pszAbsoluteURL)/sizeof(*pszAbsoluteURL)), 0);
    m_strDescriptionURL = pszAbsoluteURL;
    
    if(!g_pURL_Verifier->is_url_ok(m_strDescriptionURL))
	{
	    TraceTag(ttidError, "ServiceImpl::Init: invalid service description URL");
	    return m_hrInitResult = E_FAIL;
	}
    
    InternetCombineUrlA(strBaseURL, m_strControlURL, pszAbsoluteURL, &(dw = sizeof(pszAbsoluteURL)/sizeof(*pszAbsoluteURL)), 0);
    m_strControlURL = pszAbsoluteURL;
    
    if(!g_pURL_Verifier->is_url_ok(m_strControlURL))
	{
	    TraceTag(ttidError, "ServiceImpl::Init: invalid service control URL");
	    return m_hrInitResult = E_FAIL;
	}
    
    InternetCombineUrlA(strBaseURL, m_strEventsURL, pszAbsoluteURL, &(dw = sizeof(pszAbsoluteURL)/sizeof(*pszAbsoluteURL)), 0);
    m_strEventsURL = pszAbsoluteURL;
    
    if(!g_pURL_Verifier->is_url_ok(m_strEventsURL))
	{
	    TraceTag(ttidError, "ServiceImpl::Init: invalid service events URL");
	    return m_hrInitResult = E_FAIL;
	}
    
    // request service description document
    HttpRequest request;

	if(!request.Open("GET", m_strDescriptionURL))
		return request.GetHresult();
	
	if(!request.Send())
		return request.GetHresult();
	
	if(HTTP_STATUS_OK != request.GetStatus())
		return request.GetHresult();
	
    ce::SAXReader                       Reader;
    ce::SequentialStream<HttpRequest>   Stream(request, upnp_config::max_document_size());
    
    // parse service description document
    if(Reader.valid())
    {
        ce::variant v;
        
        Stream.QueryInterface(IID_ISequentialStream, (void**)&v.punkVal);
        v.vt = VT_UNKNOWN;
        
        Reader->putContentHandler(this);

        m_hrInitResult = Reader->parse(v);
        
        if(FAILED(m_hrInitResult))
            TraceTag(ttidError, "SAXXMLReader::parse returned error 0x%08x", m_hrInitResult);
    }
    else
        m_hrInitResult = Reader.Error();

    if(SUCCEEDED(m_hrInitResult) && (m_strSpecVersionMajor != L"1" || m_strSpecVersionMinor != L"0"))
	{
	    TraceTag(ttidError, "ServiceImpl: Invalid document version");
	    m_hrInitResult = E_FAIL;
	}
    
    if(SUCCEEDED(m_hrInitResult))
    {
        // Bind state variables to action arguments
        for(ce::vector<Action>::iterator it = m_Actions.begin(), itEnd = m_Actions.end(); it != itEnd; ++it)
            it->BindArgumentsToStateVars(m_StateVars.begin(), m_StateVars.end());
    }
    
    return m_hrInitResult;
}


// AddCallback
HRESULT ServiceImpl::AddCallback(IUPnPService* pUPnPService, IUnknown *punkCallback, DWORD* pdwCookie)
{
	callback c;
	HRESULT  hr;

	if(FAILED(m_hrInitResult))
        return m_hrInitResult;
        
	if(FAILED(m_hrAdviseResult))
		return m_hrAdviseResult;

	Assert(m_dwSubscriptionCookie != 0);
	
	if(!m_bSubscribed)
	{
		if(FAILED(hr = g_pConnectionPoint->subscribe(m_dwSubscriptionCookie, m_strEventsURL)))
			return hr;

		m_bSubscribed = true;
	}

	if(SUCCEEDED(punkCallback->QueryInterface(IID_IUPnPServiceCallback, (void**)&c.m_pUPnPServiceCallback)))
		c.m_type = callback::upnp_service_callback;
	else
		if(SUCCEEDED(punkCallback->QueryInterface(IID_IDispatch, (void**)&c.m_pDispatch)))
			c.m_type = callback::dispatch;
		else
			return E_FAIL;

	Assert(m_pUPnPService == NULL || m_pUPnPService == pUPnPService);
	
	m_pUPnPService = pUPnPService;
	
	ce::gate<ce::critical_section> _gate(m_csListCallback);

	m_listCallback.push_front(c);

	if(pdwCookie)
	{
		// return iterator as cookie
		Assert(sizeof(ce::list<callback>::iterator) == sizeof(*pdwCookie));
		*((ce::list<callback>::iterator*)pdwCookie) = m_listCallback.begin();
	}
	
	return S_OK;
}


// RemoveCallback
HRESULT ServiceImpl::RemoveCallback(DWORD dwCookie)
{
	HRESULT hr = E_INVALIDARG;
	
	if(FAILED(m_hrInitResult))
        return m_hrInitResult;
	
	Assert(sizeof(ce::list<callback>::iterator) == sizeof(dwCookie));
	ce::list<callback>::iterator itCallback = *((ce::list<callback>::iterator*)&dwCookie);

	ce::gate<ce::critical_section> _gate(m_csListCallback);

	// can't assume that dwCookie is a valid iterator so I look for it in the list
	for(ce::list<callback>::iterator it = m_listCallback.begin(), itEnd = m_listCallback.end(); it != itEnd; ++it)
		if(it == itCallback)
		{
			m_listCallback.erase(it);
			
			hr = S_OK;
			break;
		}

	return hr;
}


// StateVariableChanged
void ServiceImpl::StateVariableChanged(LPCWSTR pwszName, LPCWSTR pwszValue)
{
	ce::variant						varValue;
	ce::vector<StateVar>::iterator	itStateVar, itEndStateVar;

	// find the state variable
	for(itStateVar = m_StateVars.begin(), itEndStateVar = m_StateVars.end(); itStateVar != itEndStateVar; ++itStateVar)
		if(0 == wcscmp(itStateVar->GetName(), pwszName))
		{
			itStateVar->Decode(pwszValue, &varValue);
			break;
		}
	
	if(itStateVar != itEndStateVar)
	{
		ce::gate<ce::critical_section> _gate(m_csListCallback);

        for(ce::list<callback>::iterator it = m_listCallback.begin(), itEnd = m_listCallback.end(); it != itEnd; ++it)
			if(it->m_type == callback::upnp_service_callback)
			{
                // IUPnPServiceCallback callback
				it->m_pUPnPServiceCallback->StateVariableChanged(m_pUPnPService, pwszName, varValue);
			}
			else
			{
				// IDispatch callback
				Assert(it->m_type == callback::dispatch);

				DISPPARAMS  DispParams = {0};
				VARIANT     rgvarg[4];
				UINT        uArgErr;
				
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
                // don't clear rgvarg[2]
                VariantClear(&rgvarg[3]);
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
		    if(it->m_type == callback::upnp_service_callback)
		    {
                // IUPnPServiceCallback callback
			    it->m_pUPnPServiceCallback->ServiceInstanceDied(m_pUPnPService);
		    }
		    else
		    {
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


// DispGetParamPtr
HRESULT DispGetParamPtr(DISPPARAMS FAR *pDispParams, unsigned int position, VARIANTARG** ppVar, unsigned int FAR *puArgErr)
{
    unsigned int cPositionArgs = pDispParams->cArgs - pDispParams->cNamedArgs;

    // check if the argument is among positional arguments
    if(position < cPositionArgs)
    {
        *ppVar = &pDispParams->rgvarg[pDispParams->cArgs - position - 1];
        return S_OK;
    }

    // look for the argument among named arguments
    for(unsigned int i = 0; i < pDispParams->cNamedArgs; ++i)
        if(static_cast<DISPID>(position) == pDispParams->rgdispidNamedArgs[i])
        {
            *ppVar = &pDispParams->rgvarg[i];
            return S_OK;
        }
    
    CHECK_POINTER(puArgErr);
    
    // argument not found
    *puArgErr = position;

    return DISP_E_PARAMNOTFOUND;
}


// GetIDsOfNames
HRESULT ServiceImpl::GetIDsOfNames(OLECHAR FAR *FAR *rgszNames,
                                   unsigned int cNames, 
                                   DISPID FAR *rgDispId)
{
    if(FAILED(m_hrInitResult))
        return m_hrInitResult;
        
    if(!cNames)
        return S_OK;

    CHECK_POINTER(rgszNames);
    CHECK_POINTER(rgDispId);

    unsigned int i;

    // initialize all the names to unknown
    for(i = 0; i < cNames; ++i)
        rgDispId[i] = DISPID_UNKNOWN;

    ce::vector<Action>::iterator it, itEnd;

    // first name is method/property name
    
    // actions (methods)
    for(it = m_Actions.begin(), itEnd = m_Actions.end(); it != itEnd; ++it)
        if(0 == wcscmp(it->GetName(), rgszNames[0]))
        {
            rgDispId[0] = base_dispid + (it - m_Actions.begin());
            break;
        }

    if(it != itEnd)
    {
        // if there are more names they are those of method arguments
        // check [in] arguments
        for(i = 1; i < cNames; ++i)
        {
            for(int j = 0; j < it->GetInArgumentsCount(); ++j)
                if(0 == wcscmp(it->GetInArgument(j).GetName(), rgszNames[i]))
                {
                    Assert(rgDispId[i] == DISPID_UNKNOWN);
                    
                    rgDispId[i] = j;
                    break;
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
            if(0 == wcscmp(itStateVar->GetName(), rgszNames[0]))
            {
                rgDispId[0] = base_dispid + (itStateVar - m_StateVars.begin()) + m_Actions.size();
                break;
            }
    }
        
    // check if any names remain unknown
    for(i = 0; i < cNames; ++i)
        if(rgDispId[i] == DISPID_UNKNOWN)
            return DISP_E_UNKNOWNNAME;
    
    return S_OK;
}


// Invoke
HRESULT ServiceImpl::Invoke(DISPID dispIdMember, 
							WORD wFlags, 
							DISPPARAMS FAR *pDispParams, 
							VARIANT FAR *pVarResult, 
							EXCEPINFO FAR *pExcepInfo, 
							unsigned int FAR *puArgErr)
{
    Action*         pAction;
    ce::variant     var;
    HRESULT         hr;
    int             i;
    unsigned int    position;

    if(FAILED(m_hrInitResult))
        return m_hrInitResult;
    
    if(wFlags & ~(DISPATCH_PROPERTYGET | DISPATCH_METHOD | DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF))
        return E_INVALIDARG;
    
    if(dispIdMember < base_dispid && dispIdMember != query_state_var_dispid)
        return DISP_E_MEMBERNOTFOUND;

    // propput not supported by UPnP
    if((wFlags == DISPATCH_PROPERTYPUT) || (wFlags == DISPATCH_PROPERTYPUTREF))
        return DISP_E_MEMBERNOTFOUND;
    
    if(dispIdMember >= base_dispid + m_Actions.size())
    {
        // from dispid it can only be propget
        if(!(wFlags & DISPATCH_PROPERTYGET))
            return DISP_E_MEMBERNOTFOUND;
                
        // check if there is a state variable for this dispid
        if(dispIdMember - base_dispid - m_Actions.size() >= m_StateVars.size())
            return DISP_E_MEMBERNOTFOUND;

        return QueryStateVariable(m_StateVars[dispIdMember - base_dispid - m_Actions.size()].GetName(), pVarResult);
    }
    else
    {
        // from dispid it can only be a method
        if(!(wFlags & DISPATCH_METHOD))
            return DISP_E_MEMBERNOTFOUND;

        if(dispIdMember == query_state_var_dispid)
            pAction = &m_actionQueryStateVar;
        else
        {
            assert(dispIdMember >= base_dispid && dispIdMember < base_dispid + m_Actions.size());
                
            pAction = &m_Actions[dispIdMember - base_dispid];
        }
    }

    // init [in] arguments
    for(i = 0, position = 0; i < pAction->GetInArgumentsCount(); ++i, ++position)
    {
        Argument& arg = pAction->GetInArgument(i);

        if(FAILED(hr = DispGetParam(pDispParams, position, arg.GetVartype(), &var, puArgErr)))
            return hr;
        
        arg.SetValue(var);
    }

    SoapRequest request;

    // Send SOAP request
    request.SendMessage(m_strControlURL, pAction->GetSoapActionName(), pAction->CreateSoapMessage());
    
    m_lLastTransportStatus = request.GetStatus();

    if(request.GetError() != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(request.GetError());

    // Parse SOAP response
    pAction->ParseSoapResponse(request);
    
    if(HTTP_STATUS_OK == m_lLastTransportStatus)
    {
        // return [out] arguments
        for(i = 0, position = pAction->GetInArgumentsCount(); i < pAction->GetOutArgumentsCount(); ++i, ++position)
        {
            Argument&   arg = pAction->GetOutArgument(i);
            ce::variant varValue;

            if(FAILED(hr = arg.GetValue(&varValue)))
                return hr;
        
            if(arg.IsRetval())
            {
                CHECK_POINTER(pVarResult);

                // return [out] argument as result of the Invoke
                if(FAILED(hr = VariantCopy(pVarResult, &varValue)))
					return hr;
            
                // retval doesn't count as argument in IDispatch
                --position;
                continue;
            }

            VARIANTARG* pvarOut;

            // get pointer to variant for the [out] argument
            if(FAILED(hr = DispGetParamPtr(pDispParams, position, &pvarOut, puArgErr)))
                return hr;

            // if [out] argument is VT_BYREF | VT_VARIANT then just return value in it
            if(pvarOut->vt == (VT_BYREF | VT_VARIANT))
            {
                if(FAILED(hr = VariantCopy(V_VARIANTREF(pvarOut), &varValue)))
					return hr;

                continue;
            }

            // check if [out] parameter is a reference to proper type
            if((pvarOut->vt ^ VT_BYREF) != varValue.vt)
            {
			    CHECK_POINTER(puArgErr);

				*puArgErr = position;
				return DISP_E_TYPEMISMATCH;
			}

            Assert((pvarOut->vt ^ VT_BYREF) == varValue.vt);
        
            switch(varValue.vt)
            {
                case VT_UI1:    *V_UI1REF(pvarOut) = V_UI1(&varValue);
                                break;

                case VT_UI2:    *V_UI2REF(pvarOut) = V_UI2(&varValue);
                                break;

                case VT_UI4:    *V_UI4REF(pvarOut) = V_UI4(&varValue);
                                break;

                case VT_I1:     *V_I1REF(pvarOut) = V_I1(&varValue);
                                break;

                case VT_I2:     *V_I2REF(pvarOut) = V_I2(&varValue);
                                break;

                case VT_I4:     *V_I4REF(pvarOut) = V_I4(&varValue);
                                break;

                case VT_R4:     *V_R4REF(pvarOut) = V_R4(&varValue);
                                break;

                case VT_R8:     *V_R8REF(pvarOut) = V_R8(&varValue);
                                break;

                case VT_CY:     *V_CYREF(pvarOut) = V_CY(&varValue);
                                break;

                case VT_BSTR:   VariantClear(pvarOut);
                                *V_BSTRREF(pvarOut) = V_BSTR(&varValue);
                                break;

                case VT_DATE:   *V_DATEREF(pvarOut) = V_DATE(&varValue);
                                break;

                case VT_BOOL:   *V_BOOLREF(pvarOut) = V_BOOL(&varValue);
                                break;

                case VT_ARRAY | VT_UI1:
                                VariantClear(pvarOut);
								if(FAILED(hr = SafeArrayCopy(V_ARRAY(&varValue), V_ARRAYREF(pvarOut))))
									return hr;
                                break;
            
                default:        Assert(0);
                                break;
            }
        }

        Assert(m_lLastTransportStatus == HTTP_STATUS_OK);

        return S_OK;
    }

    Assert(m_lLastTransportStatus != HTTP_STATUS_OK);
        
    if(m_lLastTransportStatus != HTTP_STATUS_SERVER_ERROR)
        return UPNP_E_TRANSPORT_ERROR;

    switch(pAction->GetFaultCode())
    {
        case 0:   return UPNP_E_PROTOCOL_ERROR;
        case 401: return UPNP_E_INVALID_ACTION;
        case 402: return UPNP_E_INVALID_ARGUMENTS;
        case 403: return UPNP_E_DEVICE_ERROR; // TO DO: don't we need a specific error for this?
        case 404: return UPNP_E_INVALID_VARIABLE;
        case 501: return UPNP_E_ACTION_REQUEST_FAILED;
        default:  return UPNP_E_DEVICE_ERROR;
    }
}


// QueryStateVariable
HRESULT ServiceImpl::QueryStateVariable(LPCWSTR pwszVariableName, VARIANT *pValue)
{
    DISPPARAMS      DispParams;
    ce::variant     varStateVar;
    unsigned int    uArgErr;

    if(FAILED(m_hrInitResult))
        return m_hrInitResult;
    
    ce::vector<StateVar>::iterator it, itEnd;

    for(it = m_StateVars.begin(), itEnd = m_StateVars.end(); it != itEnd; ++it)
        if(0 == wcscmp(pwszVariableName, it->GetName()))
            break;

    if(it == itEnd)
        return UPNP_E_INVALID_VARIABLE;

    // bind [out] argument to the state variable
    m_actionQueryStateVar.GetOutArgument(0).BindStateVar(&*it);
    
    // fabricate a DispParams structure
    DispParams.cArgs = 1;
    DispParams.cNamedArgs = 0;
    DispParams.rgdispidNamedArgs = NULL;
    DispParams.rgvarg = &(varStateVar = it->GetName());
    
    return Invoke(query_state_var_dispid, DISPATCH_METHOD, &DispParams, pValue, NULL, &uArgErr);
}


// GetActionOutArgumentsCount
int ServiceImpl::GetActionOutArgumentsCount(DISPID dispidAction)
{
    int nCount = 0;

    if(FAILED(m_hrInitResult))
        return m_hrInitResult;

    if(dispidAction >= base_dispid && dispidAction < base_dispid + m_Actions.size())
    {
        Action* pAction = &m_Actions[dispidAction - base_dispid];

        // count not-retval [out] arguments
        for(int i = 0; i < pAction->GetOutArgumentsCount(); ++i)
        {
            if(!pAction->GetOutArgument(i).IsRetval())
                ++nCount;
        }
    }
    
    return nCount;
} 
