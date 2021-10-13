//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __SOAP_MESSAGE__
#define __SOAP_MESSAGE__

#include "string.hxx"
#include "vector.hxx"
#include "sax.h"
#include "SoapRequest.h"

// SoapMessage
class SoapMessage : ce::SAXContentHandler
{
public:
    SoapMessage(LPCWSTR pwszMethod, LPCWSTR pwszNamespace = L"", int nMaxResponseSize = -1);

    // call
    void AddInArgument(LPCWSTR pwszName, LPCWSTR pwszValue);
    LPCWSTR GetMessage();
    
    // response
    HRESULT ParseResponse(SoapRequest& request);
    
    DWORD GetOutArgumentsCount()
        {return m_OutArguments.size(); }

    LPCWSTR GetOutArgumentName(int index);
    LPCWSTR GetOutArgumentValue(int index);

    LPCWSTR GetFaultCode()
        {return m_strFaultCode; }

    LPCWSTR GetFaultString()
        {return m_strFaultString; }

    void SetFaultDetailHandler(ce::SAXContentHandler* p)
        {m_pFaultDetailHandler = p; }

private:
	static bool EncodeForXML(LPCWSTR pwsz, ce::wstring* pstr);

// ISAXContentHandler
private:
    virtual HRESULT STDMETHODCALLTYPE startDocument(void);
    
    virtual HRESULT STDMETHODCALLTYPE startElement(
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName,
        /* [in] */ ISAXAttributes __RPC_FAR *pAttributes);
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const wchar_t __RPC_FAR *pwchQName,
        /* [in] */ int cchQName);
    
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const wchar_t __RPC_FAR *pwchChars,
        /* [in] */ int cchChars);
    
private:
    struct Argument
    {
        ce::wstring m_strName;
        ce::wstring m_strValue;
    };

    ce::wstring             m_strNamespace;
    ce::wstring             m_strMethod;
    ce::wstring             m_strMessage;
    ce::vector<Argument>    m_InArguments;
    ce::vector<Argument>    m_OutArguments;
    ce::wstring             m_strFaultCode;
    ce::wstring             m_strFaultString;
    ce::SAXContentHandler*	m_pFaultDetailHandler;
    int                     m_nMaxResponseSize;
    
    // used during parsing
    ce::wstring             m_strActionResponseElement;
    ce::wstring             m_strOutArgumentElement;
    ce::wstring             m_strFaultCodeElement;
    ce::wstring             m_strFaultStringElement;
    ce::wstring             m_strFaultDetailElement;
    Argument                m_OutArg;
    bool                    m_bParsingActionResponse;
    bool                    m_bParsingOutArgument;
    bool                    m_bParsingFaultDetail;
};

#endif // __SOAP_MESSAGE__
