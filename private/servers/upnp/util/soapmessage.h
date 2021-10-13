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
#ifndef __SOAP_MESSAGE__
#define __SOAP_MESSAGE__

#include "string.hxx"
#include "vector.hxx"
#include "sax.h"
#include "HttpRequest.h"

// SoapMessage
class SoapMessage : ce::SAXContentHandler
{
public:
    SoapMessage(LPCWSTR pwszMethod, LPCWSTR pwszNamespace = L"", int nMaxResponseSize = -1);

    // call
    void AddInArgument(LPCWSTR pwszName, LPCWSTR pwszValue);
    LPCWSTR GetMessage();
    
    // response
    HRESULT ParseResponse(HttpRequest& request);
    
    DWORD GetOutArgumentsCount();

    LPCWSTR GetOutArgumentName(unsigned int index);
    LPCWSTR GetOutArgumentValue(unsigned int index);

    LPCWSTR GetFaultCode();
    LPCWSTR GetFaultString();

    void SetFaultDetailHandler(ce::SAXContentHandler* p);

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
    ce::SAXContentHandler*  m_pFaultDetailHandler;
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
