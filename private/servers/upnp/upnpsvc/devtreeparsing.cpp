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

// Parse
BOOL HostedDeviceTree::Parse(LPCWSTR pwszDeviceDescription)
{
    HRESULT hr = S_OK;
    BOOL fResult = FALSE;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    ce::SAXReader   Reader;
    ISAXXMLReader *pXMLReader = NULL;
    VARIANT vt;
    ce::auto_bstr bstrDescription;
    
    Chk(Reader.Init());
    
    pXMLReader = Reader.GetXMLReader();
    ChkBool(pXMLReader, E_FAIL);
    
    // first pass - verify specVersion and get URLBase (optional)
    Chk(pXMLReader->putContentHandler(this));

    bstrDescription = SysAllocString(pwszDeviceDescription);
    ChkBool(bstrDescription, E_OUTOFMEMORY);
    
    vt.vt = VT_BSTR;
    vt.bstrVal = bstrDescription;
    
    Chk(pXMLReader->parse(vt));

Cleanup:
    if (pXMLReader)
    {
        pXMLReader->putContentHandler(NULL);
    }
    
    if(SUCCEEDED(hr))
    {
        // verify spec version
        m_strSpecVersionMajor.trim(L"\n\r\t ");
        m_strSpecVersionMinor.trim(L"\n\r\t ");

        if(m_strSpecVersionMajor != L"1" || m_strSpecVersionMinor != L"0")
        {
            // invalid spec version
            fResult = false;
        }
        else
        {
            fResult = true;
        }
    }
    else
    {
        TraceTag(ttidError, "HostedDeviceTree::Parse returning false : error 0x%08x", hr);
    }

    CoUninitialize();
    return fResult;
}


static wchar_t pwszSpecVersionElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<root>"
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<specVersion>";

static wchar_t pwszDeviceElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<root>"
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<device>";

// startDocument
HRESULT STDMETHODCALLTYPE HostedDeviceTree::startDocument(void)
{
    SAXContentHandler::startDocument();

    m_SAXWriter.Open(m_pszDescFileName);

    m_SAXWriter.startDocument();

    return S_OK;
}

// startElement
HRESULT STDMETHODCALLTYPE HostedDeviceTree::startElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
    SAXContentHandler::startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);

    if(m_pTempDevice)
    {
        m_pTempDevice->startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
    }
    else
    {
        // Device element
        if(pwszDeviceElement == m_strFullElementName)
        {
            // start of root device
            // this section will be parsed by new device object
            m_pTempDevice = new HostedDevice(this);
            if(m_pTempDevice)
            {
                // simulate startDocument for the device
                m_pTempDevice->startDocument();
            }
        }

        m_SAXWriter.startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
    }

    return S_OK;
}


// endElement
HRESULT STDMETHODCALLTYPE HostedDeviceTree::endElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
    // Device element
    if(pwszDeviceElement == m_strFullElementName)
    {
        Assert(m_pTempDevice);

        // finished parsing root device;
        // simulate endDocument
        m_pTempDevice->endDocument();

        Assert(m_pRootDevice == NULL);
        
        m_pRootDevice = m_pTempDevice;

        m_pTempDevice = NULL;
    }
        
    if(m_pTempDevice)
    {
        m_pTempDevice->endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
    }
    else
    {
        m_SAXWriter.endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
    }

    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE HostedDeviceTree::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    // specVersion
    static const int nSpecVersionLength = sizeof(pwszSpecVersionElement)/sizeof(*pwszSpecVersionElement) - 1;

    if(0 == wcsncmp(pwszSpecVersionElement, m_strFullElementName, nSpecVersionLength))
    {           
        // major
        if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><major>", static_cast<LPCWSTR>(m_strFullElementName) + nSpecVersionLength))
        {
            m_strSpecVersionMajor.append(pwchChars, cchChars);
        }

        // major
        if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><minor>", static_cast<LPCWSTR>(m_strFullElementName) + nSpecVersionLength))
        {
            m_strSpecVersionMinor.append(pwchChars, cchChars);
        }
    }
        
    if(m_pTempDevice)
    {
        m_pTempDevice->characters(pwchChars, cchChars);
    }
    else
    {
        m_SAXWriter.characters(pwchChars, cchChars);
    }
    
    return S_OK;
}


// endDocument
HRESULT STDMETHODCALLTYPE HostedDeviceTree::endDocument(void)
{
    SAXContentHandler::endDocument();

    m_SAXWriter.endDocument();
    
    return S_OK;
}
