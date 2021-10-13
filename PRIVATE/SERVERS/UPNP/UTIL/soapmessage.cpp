//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>

#include "SoapMessage.h"
#include "variant.h"


// SoapMessage
SoapMessage::SoapMessage(LPCWSTR pwszMethod, LPCWSTR pwszNamespace/* = L""*/, int nMaxResponseSize/* = -1*/)
    : m_pFaultDetailHandler(NULL),
      m_strMethod(pwszMethod),
      m_nMaxResponseSize(nMaxResponseSize)
{
    EncodeForXML(pwszNamespace, &m_strNamespace);

	m_strActionResponseElement.reserve(250);
    m_strFaultCodeElement.reserve(200);
    m_strFaultDetailElement.reserve(200);
    m_strFaultStringElement.reserve(200);
    m_strOutArgumentElement.reserve(250);
}


// EncodeForXML
bool SoapMessage::EncodeForXML(LPCWSTR pwsz, ce::wstring* pstr)
{
	wchar_t		aCharacters[] = {L'<', L'>', L'\'', L'"', L'&'};
	wchar_t*	aEntityReferences[] = {L"&lt;", L"&gt;", L"&apos;", L"&quot;", L"&amp;"};
	bool		bReplaced;
	
	pstr->reserve(static_cast<ce::wstring::size_type>(1.1 * wcslen(pwsz)));
	pstr->resize(0);
	
	for(const wchar_t* pwch = pwsz; *pwch; ++pwch)
	{
		bReplaced = false;

		for(int i = 0; i < sizeof(aCharacters)/sizeof(*aCharacters); ++i)
			if(*pwch == aCharacters[i])
			{
				pstr->append(aEntityReferences[i]);
				bReplaced = true;
				break;
			}

		if(!bReplaced)
			pstr->append(pwch, 1);
	}

	return true;
}


// SetInArgument
void SoapMessage::AddInArgument(LPCWSTR pwszName, LPCWSTR pwszValue)
{
    Argument arg;
    
    arg.m_strName = pwszName;
	EncodeForXML(pwszValue, &arg.m_strValue);

    m_InArguments.push_back(arg);
}


// GetMessage
LPCWSTR SoapMessage::GetMessage()
{
    static const wchar_t pwszSoapMessageFormat[] = 
        L"<s:Envelope\n"
        L"    xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\n"
        L"    s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        L"  <s:Body>\n"
        L"    <u:%s xmlns:u=\"%s\">\n"
        L"      %s"
        L"    </u:%s>\n"
        L"  </s:Body>\n"
        L"</s:Envelope>\n";

    static const wchar_t pwszSOAPArgumentFormat[] = L"<%s>%s</%s>\n";

    int         nSize;
    ce::wstring strArguments, strArgument;

    strArguments.reserve(m_InArguments.size() * 40);

    // add all [in] arguments
    for(int i = 0; i < m_InArguments.size(); ++i)
    {
        const ce::wstring &strName = m_InArguments[i].m_strName;
        const ce::wstring &strValue = m_InArguments[i].m_strValue;

        // calculate size of argument
        nSize = sizeof(pwszSOAPArgumentFormat)/sizeof(*pwszSOAPArgumentFormat) + 
                2 * strName.size() + strValue.size();
        
        strArgument.reserve(nSize);
        
        _snwprintf(strArgument.get_buffer(), strArgument.capacity(),
                   pwszSOAPArgumentFormat,
                   static_cast<LPCWSTR>(strName),
                   static_cast<LPCWSTR>(strValue),
                   static_cast<LPCWSTR>(strName));

        strArguments += strArgument;
    }

    // calculate size of the message
    nSize = sizeof(pwszSoapMessageFormat)/sizeof(*pwszSoapMessageFormat) + 
            2 * m_strMethod.size() + m_strNamespace.size() + strArguments.size();
    
    m_strMessage.reserve(nSize);
    
    // create message
    _snwprintf(m_strMessage.get_buffer(), m_strMessage.capacity(),
            pwszSoapMessageFormat,
            static_cast<LPCWSTR>(m_strMethod),
            static_cast<LPCWSTR>(m_strNamespace),
            static_cast<LPCWSTR>(strArguments),
			static_cast<LPCWSTR>(m_strMethod));
    
    return m_strMessage;        
}


// ParseResponse
HRESULT SoapMessage::ParseResponse(SoapRequest& request)
{
    ce::SequentialStream<HttpRequest>   Stream(request, m_nMaxResponseSize);
    ce::SAXReader                       Reader;
    HRESULT			                    hr = E_UNEXPECTED;
    
    if(Reader.valid())
    {
        ce::variant v;
        
        Stream.QueryInterface(IID_ISequentialStream, (void**)&v.punkVal);
        v.vt = VT_UNKNOWN;
        
        Reader->putContentHandler(this);

        hr = Reader->parse(v);
        
        m_strFaultCode.trim(L"\n\r\t ");
        m_strFaultString.trim(L"\n\r\t ");
    }
    else
        hr = Reader.Error();

    return hr;
}


// GetOutArgumentName
LPCWSTR SoapMessage::GetOutArgumentName(int index)
{
    if(index < m_OutArguments.size() && index >= 0)
        return m_OutArguments[index].m_strName; 
    else
        return NULL;
}


// GetOutArgumentValue
LPCWSTR SoapMessage::GetOutArgumentValue(int index)
{
    if(index < m_OutArguments.size() && index >= 0)
        return m_OutArguments[index].m_strValue; 
    else
        return NULL;
}


// startDocument
HRESULT STDMETHODCALLTYPE SoapMessage::startDocument(void)
{
    static const wchar_t pwszResponse[] = L"Response";
    static const wchar_t pwszFaultcode[] = L"<faultcode>";
    static const wchar_t pwszFaultstring[] = L"<faultstring>";
    static const wchar_t pwszDetail[] = L"<detail>";
    static const wchar_t pwszFault[] = 
        L"<http://schemas.xmlsoap.org/soap/envelope/>"
        L"<Fault>";
    static const wchar_t pwszSoapBodyElementFullName[] = 
        L"<http://schemas.xmlsoap.org/soap/envelope/>"
        L"<Envelope>"
        L"<http://schemas.xmlsoap.org/soap/envelope/>"
        L"<Body>";
    
    SAXContentHandler::startDocument();

    // reset fault info
    m_strFaultCode.resize(0);
    m_strFaultString.resize(0);

    // reset out arguments vector
    m_OutArguments.clear();
    
    // initialize varaibles used for parsing
    m_strActionResponseElement = pwszSoapBodyElementFullName;
    m_strActionResponseElement += L"<";
    m_strActionResponseElement += m_strNamespace;
    m_strActionResponseElement += L"><";
    m_strActionResponseElement += m_strMethod;
    m_strActionResponseElement += pwszResponse;
    m_strActionResponseElement += L">";

    m_strFaultCodeElement = pwszSoapBodyElementFullName;
    m_strFaultCodeElement += pwszFault;

    m_strFaultDetailElement = m_strFaultStringElement = m_strFaultCodeElement;

    m_strFaultCodeElement += pwszFaultcode;
    m_strFaultStringElement += pwszFaultstring;
    m_strFaultDetailElement += pwszDetail;

    m_bParsingActionResponse = false;
    m_bParsingOutArgument = false;
    m_bParsingFaultDetail = false;
    
    return S_OK;
}


// startElement
HRESULT STDMETHODCALLTYPE SoapMessage::startElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
    SAXContentHandler::startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
    
    // start of a direct child of actionResponse element -> out argument
    if(m_bParsingActionResponse && !m_bParsingOutArgument)
    {
        m_strOutArgumentElement = m_strActionResponseElement;
        m_strOutArgumentElement.append(L"<");
        m_strOutArgumentElement.append(pwchLocalName, cchLocalName);
        m_strOutArgumentElement.append(L">");
        
        m_OutArg.m_strName.assign(pwchLocalName, cchLocalName);
        m_OutArg.m_strValue.resize(0);

        m_bParsingOutArgument = true;
    }
    
    // start of actionResponse element
    if(m_strActionResponseElement == m_strFullElementName)
        m_bParsingActionResponse = true;
    
    if(m_bParsingFaultDetail && m_pFaultDetailHandler)
        // pass to fault detail handler
        m_pFaultDetailHandler->startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);

    // start of fault detail element 
    if(m_strFaultDetailElement == m_strFullElementName)
    {
        m_bParsingFaultDetail = true;
        
        if(m_pFaultDetailHandler)
            m_pFaultDetailHandler->startDocument();
    }
    
    return S_OK;
}


// endElement
HRESULT STDMETHODCALLTYPE SoapMessage::endElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
    // end of out argument element
    if(m_strFullElementName == m_strOutArgumentElement)
    {
        m_OutArg.m_strValue.trim(L"\n\r\t ");

        m_OutArguments.push_back(m_OutArg);

        m_bParsingOutArgument = false;
    }

    // end of actionResponse element
    if(m_strActionResponseElement == m_strFullElementName)
        m_bParsingActionResponse = false;
    
    // end of fault detail element 
    if(m_strFaultDetailElement == m_strFullElementName)
    {
        m_bParsingFaultDetail = false;
        
        if(m_pFaultDetailHandler)
            m_pFaultDetailHandler->endDocument();
    }
    
    if(m_bParsingFaultDetail && m_pFaultDetailHandler)
        // pass to fault detail handler
        m_pFaultDetailHandler->endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);

    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE SoapMessage::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    if(m_strOutArgumentElement == m_strFullElementName)
        m_OutArg.m_strValue.append(pwchChars, cchChars);

    if(m_strFaultStringElement == m_strFullElementName)
        m_strFaultString.append(pwchChars, cchChars);

    if(m_strFaultCodeElement == m_strFullElementName)
        m_strFaultCode.append(pwchChars, cchChars);
    
    if(m_bParsingFaultDetail && m_pFaultDetailHandler)
        // pass to fault detail handler
        m_pFaultDetailHandler->characters(pwchChars, cchChars);

    return S_OK;
}
