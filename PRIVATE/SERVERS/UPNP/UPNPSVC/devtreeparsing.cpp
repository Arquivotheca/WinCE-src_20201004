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

struct CoGate
{
	CoGate(DWORD dwCoInit = COINIT_MULTITHREADED)
		{CoInitializeEx(NULL, dwCoInit); }

	~CoGate()
		{CoUninitialize(); }
};

// Parse
BOOL HostedDeviceTree::Parse(LPCWSTR pwszDeviceDescription)
{
	HRESULT			hr;
    CoGate          co;
	ce::SAXReader	Reader;

	// first pass - verify specVersion and get URLBase (optional)
	if(Reader.valid())
    {
        Reader->putContentHandler(this);

        VARIANT vt;

        vt.vt = VT_BSTR;
        vt.bstrVal = SysAllocString(pwszDeviceDescription);
        
        hr = Reader->parse(vt);
        
        if(FAILED(hr))
            TraceTag(ttidError, "SAXXMLReader::parse returned error 0x%08x", hr);

        // avoid VariantClear to minimize dependency on oleaut32
        SysFreeString(vt.bstrVal);
    }
    else
        hr = Reader.Error();
	
	if(SUCCEEDED(hr))
	{
		// verify spec version
		m_strSpecVersionMajor.trim(L"\n\r\t ");
		m_strSpecVersionMinor.trim(L"\n\r\t ");

		if(m_strSpecVersionMajor != L"1" || m_strSpecVersionMinor != L"0")
			// invalid spec version
			return false;
	}

	return SUCCEEDED(hr);
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
		m_pTempDevice->startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
	else
	{
		// Device element
		if(pwszDeviceElement == m_strFullElementName)
		{
			// start of root device
			// this section will be parsed by new device object
			m_pTempDevice = new HostedDevice(this);
			
			// simulate startDocument for the device
			m_pTempDevice->startDocument();
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
		m_pTempDevice->endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
	else
		m_SAXWriter.endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);

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
			m_strSpecVersionMajor.append(pwchChars, cchChars);

		// major
		if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><minor>", static_cast<LPCWSTR>(m_strFullElementName) + nSpecVersionLength))
			m_strSpecVersionMinor.append(pwchChars, cchChars);
	}
		
	if(m_pTempDevice)
		m_pTempDevice->characters(pwchChars, cchChars);
	else
		m_SAXWriter.characters(pwchChars, cchChars);
	
	return S_OK;
}


// endDocument
HRESULT STDMETHODCALLTYPE HostedDeviceTree::endDocument(void)
{
	SAXContentHandler::endDocument();

	m_SAXWriter.endDocument();
	
	return S_OK;
}
