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

#include "ServiceImpl.h"

static wchar_t* pwszRootElement = 
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<scpd>";
    
static wchar_t pwszSpecVersionMajorElement[] = 
    L"<urn:schemas-upnp-org:service-1-0>"
	L"<scpd>"
	L"<urn:schemas-upnp-org:service-1-0>"
    L"<specVersion>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<major>";
    
static wchar_t pwszSpecVersionMinorElement[] = 
    L"<urn:schemas-upnp-org:service-1-0>"
	L"<scpd>"
	L"<urn:schemas-upnp-org:service-1-0>"
    L"<specVersion>"
    L"<urn:schemas-upnp-org:service-1-0>"
    L"<minor>";      

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
HRESULT STDMETHODCALLTYPE ServiceImpl::startElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
    SAXContentHandler::startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
    
    if(!m_bParsedRootElement)
	{
	    // this is root element of the document
	    m_bParsedRootElement = true;
	    
	    // immediately terminate parsing/downloading if root element is not valid for UPnP service description 
	    if(pwszRootElement != m_strFullElementName)
	    {
	        TraceTag(ttidError, "ServiceImpl: Invalid root element");
	        return E_FAIL;
	    }
	}
    
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
HRESULT STDMETHODCALLTYPE ServiceImpl::endElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
    // specVersion major
	if(pwszSpecVersionMajorElement == m_strFullElementName)
	{
	    m_strSpecVersionMajor.trim(L"\n\r\t ");
	    
	    if(m_strSpecVersionMajor != L"1")
	    {
	        TraceTag(ttidError, "ServiceImpl: Invalid document version");
	        return E_FAIL;
	    }
	}
	    
	// specVersion minor
	if(pwszSpecVersionMinorElement == m_strFullElementName)
	{
	    m_strSpecVersionMinor.trim(L"\n\r\t ");
	    
	    if(m_strSpecVersionMinor != L"0")
	    {
	        TraceTag(ttidError, "ServiceImpl: Invalid document version");
	        return E_FAIL;
	    }
	}
    
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
HRESULT STDMETHODCALLTYPE ServiceImpl::characters( 
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
        
    // specVersion major
	if(pwszSpecVersionMajorElement == m_strFullElementName)
		m_strSpecVersionMajor.append(pwchChars, cchChars);
		
	// specVersion minor
	if(pwszSpecVersionMinorElement == m_strFullElementName)
		m_strSpecVersionMinor.append(pwchChars, cchChars);

    return S_OK;
}
