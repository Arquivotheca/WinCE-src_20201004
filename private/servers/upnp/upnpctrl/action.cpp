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
    {
        soap.AddInArgument(it->GetName(), it->ToString());
    }

    return m_strSoapMessage = soap.GetMessage();
}


// ParseSoapResponse
HRESULT Action::ParseSoapResponse(HttpRequest& request)
{
    SoapMessage soap(m_strName, m_strNamespace, upnp_config::max_action_response());
    ce::wstring strArgName;
    ce::wstring strArgValue;
    HRESULT     hr;

    soap.SetFaultDetailHandler(this);
    
    m_strFaultCode.resize(0);
    m_strFaultDescription.resize(0);
    
    if(FAILED(hr = soap.ParseResponse(request)))
    {
        return hr;
    }

    m_strFaultCode.trim(L"\n\r\t ");
    m_strFaultDescription.trim(L"\n\r\t ");

    // clear [out] arguments
    for(ce::vector<Argument>::iterator it = m_OutArguments.begin(), itEnd = m_OutArguments.end(); it != itEnd; ++it)
    {
        it->Reset();
    }
    
    // extract [out] arguments from SOAP
    for(int i = 0, nCount = soap.GetOutArgumentsCount(); i < nCount; ++i)
    {
        strArgName = soap.GetOutArgumentName(i);
        strArgValue = soap.GetOutArgumentValue(i);

        // match to [out] argument by name
        for(ce::vector<Argument>::iterator it = m_OutArguments.begin(), itEnd = m_OutArguments.end(); it != itEnd; ++it)
        {
            if(it->GetName() == strArgName)
            {
                it->SetValue(strArgValue);
            }
        }
    }
    
    return hr;
}


// BindArgumentsToStateVars
void Action::BindArgumentsToStateVars(ce::vector<StateVar>::const_iterator itBeginStateVar, ce::vector<StateVar>::const_iterator itEndStateVar)
{
    ce::vector<Argument>::iterator it, itEnd;

    // bind [in] arguments
    for(it = m_InArguments.begin(), itEnd = m_InArguments.end(); it != itEnd; ++it)
    {
        it->BindStateVar(itBeginStateVar, itEndStateVar);
    }

    // bind [out] arguments
    for(it = m_OutArguments.begin(), itEnd = m_OutArguments.end(); it != itEnd; ++it)
    {
        it->BindStateVar(itBeginStateVar, itEndStateVar);
    }
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
    {
        m_strFaultCode.append(pwchChars, cchChars);
    }

    if(pwszErrorDescriptionElement == m_strFullElementName)
    {
        m_strFaultDescription.append(pwchChars, cchChars);
    }
    
    return S_OK;
}
