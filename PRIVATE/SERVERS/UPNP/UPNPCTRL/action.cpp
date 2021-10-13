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

#include "Action.h"
#include "SoapMessage.h"
#include "upnp_config.h"

// Action
Action::Action(LPCWSTR pwszName, LPCWSTR pwszNamespace)
	: m_strName(pwszName),
      m_strNamespace(pwszNamespace)
{
	ce::wstring strSoapActionName;

    strSoapActionName = L"\"";
    strSoapActionName += m_strNamespace;
    strSoapActionName += L"#";
    strSoapActionName += m_strName;
    strSoapActionName += L"\"";

	ce::WideCharToMultiByte(CP_ACP, strSoapActionName, -1, &m_strSoapActionName);
}


// CreateSoapMessage
LPCWSTR Action::CreateSoapMessage()
{
    SoapMessage soap(m_strName, m_strNamespace, upnp_config::max_action_response());

    for(ce::vector<Argument>::iterator it = m_InArguments.begin(), itEnd = m_InArguments.end(); it != itEnd; ++it)
        soap.AddInArgument(it->GetName(), it->ToString());

    return m_strSoapMessage = soap.GetMessage();
}


// ParseSoapResponse
void Action::ParseSoapResponse(SoapRequest& request)
{
    SoapMessage soap(m_strName, m_strNamespace, upnp_config::max_action_response());
    ce::wstring strArgName;
    ce::wstring strArgValue;

    soap.SetFaultDetailHandler(this);
    
    m_strFaultCode.resize(0);
    m_strFaultDescription.resize(0);
    
    soap.ParseResponse(request);

    m_strFaultCode.trim(L"\n\r\t ");
    m_strFaultDescription.trim(L"\n\r\t ");

    // clear [out] arguments
    for(ce::vector<Argument>::iterator it = m_OutArguments.begin(), itEnd = m_OutArguments.end(); it != itEnd; ++it)
        it->Reset();
    
    // extract [out] arguments from SOAP
    for(int i = 0, nCount = soap.GetOutArgumentsCount(); i < nCount; ++i)
    {
        strArgName = soap.GetOutArgumentName(i);
        strArgValue = soap.GetOutArgumentValue(i);

        // match to [out] argument by name
        for(ce::vector<Argument>::iterator it = m_OutArguments.begin(), itEnd = m_OutArguments.end(); it != itEnd; ++it)
        {
            if(it->GetName() == strArgName)
                it->SetValue(strArgValue);
        }
    }
}


// BindArgumentsToStateVars
void Action::BindArgumentsToStateVars(ce::vector<StateVar>::const_iterator itBeginStateVar, ce::vector<StateVar>::const_iterator itEndStateVar)
{
    ce::vector<Argument>::iterator it, itEnd;

    // bind [in] arguments
    for(it = m_InArguments.begin(), itEnd = m_InArguments.end(); it != itEnd; ++it)
        it->BindStateVar(itBeginStateVar, itEndStateVar);

    // bind [out] arguments
    for(it = m_OutArguments.begin(), itEnd = m_OutArguments.end(); it != itEnd; ++it)
        it->BindStateVar(itBeginStateVar, itEndStateVar);
}


// characters
HRESULT STDMETHODCALLTYPE Action::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    static wchar_t* pwszErrorCodeElement = 
        L"<urn:schemas-upnp-org:control-1-0>"
        L"<UPnPError>"
        L"<urn:schemas-upnp-org:control-1-0>"
        L"<errorCode>";

    static wchar_t* pwszErrorDescriptionElement = 
        L"<urn:schemas-upnp-org:control-1-0>"
        L"<UPnPError>"
        L"<urn:schemas-upnp-org:control-1-0>"
        L"<errorDescription>";

    if(pwszErrorCodeElement == m_strFullElementName)
        m_strFaultCode.append(pwchChars, cchChars);

    if(pwszErrorDescriptionElement == m_strFullElementName)
        m_strFaultDescription.append(pwchChars, cchChars);
    
    return S_OK;
}
