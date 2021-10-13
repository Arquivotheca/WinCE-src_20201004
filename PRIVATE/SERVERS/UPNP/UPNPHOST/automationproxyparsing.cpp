//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>

#include "ncbase.h"
#include "ncdebug.h"
#include "trace.h"
#include "variant.h"
#include "auto_xxx.hxx"
#include "AutomationProxy.h"

// Parse
HRESULT CUPnPAutomationProxy::Parse(LPWSTR pszSvcDescription)
{
	ce::auto_handle                 hFile(CreateFile(pszSvcDescription, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL));
	HRESULT			                hr;
	ce::SAXReader	                Reader;
	ce::SequentialStream<HANDLE>    Stream(hFile);

	// parse service description document
    if(Reader.valid())
    {
        ce::variant v;
        
        Stream.QueryInterface(IID_ISequentialStream, (void**)&v.punkVal);
        v.vt = VT_UNKNOWN;
        
        Reader->putContentHandler(this);

        hr = Reader->parse(v);
        
        if(FAILED(hr))
            TraceTag(ttidError, "SAXXMLReader::parse returned error 0x%08x", hr);
    }
    else
        hr = Reader.Error();

    for(ce::vector<Action>::iterator it = m_Actions.begin(), itEnd = m_Actions.end(); it != itEnd; ++it)
    {
	    // Bind state variables to action arguments
		it->BindArgumentsToStateVars(m_StateVars.begin(), m_StateVars.end());

		// get DISPID of the action
		LPCWSTR pwszName = static_cast<LPCWSTR>(it->strName);
		
		hr = m_pdispService->GetIDsOfNames(IID_NULL,
                                           const_cast<LPWSTR*>(&pwszName),
                                           1,
                                           LOCALE_SYSTEM_DEFAULT,
                                           &it->dispid);

        if (SUCCEEDED(hr))
        {
            Assert(DISPID_UNKNOWN != it->dispid);

            TraceTag(ttidAutomationProxy,
                     "CUPnPAutomationProxy::"
                     "HrBuildActionTable(): "
                     "Action %S has dispID %d",
                     it->strName,
                     it->dispid);
        }
        else
        {
            TraceError("CUPnPAutomationProxy::"
                       "HrBuildActionTable(): "
                       "Failed to get dispId",
                       hr);

			break;
        }

		// fix some redundant data memebers used in CUPnPAutomationProxy
		it->cInArgs = it->rgInArgs.size();
		it->cOutArgs = it->rgOutArgs.size();

		if(it->cOutArgs > 0)
			it->bRetVal = it->rgOutArgs[0].bRetval;
	}

	if(SUCCEEDED(hr))
	{
		// get DISPIDs for state variables
		for(ce::vector<StateVar>::iterator it = m_StateVars.begin(), itEnd = m_StateVars.end(); it != itEnd; ++it)
		{
			LPCWSTR pwszName = static_cast<LPCWSTR>(it->strName);

			hr = m_pdispService->GetIDsOfNames(IID_NULL,
											   const_cast<LPWSTR*>(&pwszName),
											   1,
											   LOCALE_SYSTEM_DEFAULT,
											   &it->dispid);

			if (SUCCEEDED(hr))
			{
				Assert(DISPID_UNKNOWN != it->dispid);

				TraceTag(ttidAutomationProxy,
						 "CUPnPAutomationProxy::"
						 "HrBuildVariableTable(): "
						 "Variable %S has dispID %d",
						 it->strName,
						 it->dispid);
			}
			else
			{
				TraceError("CUPnPAutomationProxy::"
						   "HrBuildVariableTable(): "
						   "Failed to get dispId",
						   hr);
				
				break;
			}
		}
	}

	m_cVariables = m_StateVars.size();
	m_cActions = m_Actions.size();

	return hr;
}


// BindArgumentsToStateVars
void tagUPNP_ACTION::BindArgumentsToStateVars(ce::vector<StateVar>::const_iterator itBeginStateVar, ce::vector<StateVar>::const_iterator itEndStateVar)
{
    ce::vector<Argument>::iterator it, itEnd;

    // bind [in] arguments
    for(it = rgInArgs.begin(), itEnd = rgInArgs.end(); it != itEnd; ++it)
        it->BindStateVar(itBeginStateVar, itEndStateVar);

    // bind [out] arguments
    for(it = rgOutArgs.begin(), itEnd = rgOutArgs.end(); it != itEnd; ++it)
        it->BindStateVar(itBeginStateVar, itEndStateVar);
}


// BindStateVar
void tagUPNP_ARGUMENT::BindStateVar(ce::vector<StateVar>::const_iterator it, ce::vector<StateVar>::const_iterator itEnd)
{
	for(; it != itEnd; ++it)
		if(it->strName == strStateVar)
			pusvRelated = &*it;
}


// Code below is exactly the same as ServiceParsing.cpp in upnpctrl.
// Keep it this way.

static wchar_t* pwszActionNameElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<actionList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<action>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<name>";

static wchar_t* pwszArgumentElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<actionList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<action>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argumentList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argument>";

static wchar_t* pwszArgumentNameElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<actionList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<action>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argumentList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argument>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<name>";

static wchar_t* pwszArgumentDirectionElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<actionList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<action>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argumentList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argument>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<direction>";

static wchar_t* pwszArgumentRetvalElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<actionList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<action>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argumentList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argument>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<retval>";

static wchar_t* pwszArgumentStateVarElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<actionList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<action>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argumentList>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<argument>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<relatedStateVariable>";

static wchar_t* pwszStateVarElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<serviceStateTable>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<stateVariable>";

static wchar_t* pwszStateVarNameElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<serviceStateTable>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<stateVariable>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<name>";

static wchar_t* pwszStateVarTypeElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<serviceStateTable>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<stateVariable>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<dataType>";

// startElement
HRESULT STDMETHODCALLTYPE CUPnPAutomationProxy::startElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
    SAXContentHandler::startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
    
    // start of action element
    if(m_strFullElementName == pwszActionNameElement)
    {
        m_strActionName.resize(0);
    }

    // start of argument element
    if(m_strFullElementName == pwszArgumentElement)
    {
        m_strArgumentDirection.resize(0);
        m_strArgumentName.resize(0);
        m_strStateVarName.resize(0);
        m_bRetval = false;
    }

    // start of state variable element
    if(m_strFullElementName == pwszStateVarElement)
    {
        m_strStateVarName.resize(0);
        m_strStateVarType.resize(0);
        m_bSendEvents = true;

        const wchar_t*  pwchSendEvents;
        int             cbSendEvents;

        if(SUCCEEDED(pAttributes->getValueFromName(L"", 0, L"sendEvents", 10, &pwchSendEvents, &cbSendEvents)) &&
           cbSendEvents == 2 &&
           0 == wcsncmp(L"no", pwchSendEvents, 2))
        {
           m_bSendEvents = false;
        }
    }

    // start of retval element
    if(m_strFullElementName == pwszArgumentRetvalElement)
        m_bRetval = true;

    return S_OK;
}


// endElement
HRESULT STDMETHODCALLTYPE CUPnPAutomationProxy::endElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
    // end of action element
    if(m_strFullElementName == pwszActionNameElement)
    {
        // Add action
        m_strActionName.trim(L"\n\r\t ");
        
        m_Actions.push_back(Action(m_strActionName, m_strType));

        Assert(m_Actions.size());
        
        m_pCurrentAction = &m_Actions[m_Actions.size() - 1];
    }

    // end of argument element
    if(m_strFullElementName == pwszArgumentElement)
    {
        // Add argument
        m_strArgumentDirection.trim(L"\n\r\t ");
        m_strArgumentName.trim(L"\n\r\t ");
        m_strStateVarName.trim(L"\n\r\t ");
        
        Assert(m_strArgumentDirection == L"in" || m_strArgumentDirection == L"out");
        
        if(m_strArgumentDirection == L"in")
            m_pCurrentAction->AddInArgument(Argument(m_strArgumentName, m_strStateVarName, m_bRetval));
        
        if(m_strArgumentDirection == L"out")
            m_pCurrentAction->AddOutArgument(Argument(m_strArgumentName, m_strStateVarName, m_bRetval));
    }

    // end of state variable element
    if(m_strFullElementName == pwszStateVarElement)
    {
        // Add state variable
        m_strStateVarName.trim(L"\n\r\t ");
        m_strStateVarType.trim(L"\n\r\t ");

        m_StateVars.push_back(StateVar(m_strStateVarName, m_strStateVarType, m_bSendEvents));
    }
    
    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE CUPnPAutomationProxy::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    // action name
    if(pwszActionNameElement == m_strFullElementName)
        m_strActionName.append(pwchChars, cchChars);

    // argument direction 
    if(pwszArgumentDirectionElement == m_strFullElementName)
        m_strArgumentDirection.append(pwchChars, cchChars);

    // argument name
    if(pwszArgumentNameElement == m_strFullElementName)        
        m_strArgumentName.append(pwchChars, cchChars);

    // related state variable name
    if(pwszArgumentStateVarElement == m_strFullElementName)    
        m_strStateVarName.append(pwchChars, cchChars);

    // state variable name
    if(pwszStateVarNameElement == m_strFullElementName)
        m_strStateVarName.append(pwchChars, cchChars);
    
    // state variable type
    if(pwszStateVarTypeElement == m_strFullElementName)
        m_strStateVarType.append(pwchChars, cchChars);

    return S_OK;
}
