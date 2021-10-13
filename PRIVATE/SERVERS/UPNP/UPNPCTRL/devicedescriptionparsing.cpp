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

#include "variant.h"
#include "HttpRequest.h"
#include "upnp_config.h"

#include "DeviceDescription.h"
#include "Device.h"

static wchar_t pwszRootElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
	L"<root>";

static wchar_t pwszSpecVersionMajorElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
	L"<root>"
	L"<urn:schemas-upnp-org:device-1-0>"
    L"<specVersion>"
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<major>";
    
static wchar_t pwszSpecVersionMinorElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
	L"<root>"
	L"<urn:schemas-upnp-org:device-1-0>"
    L"<specVersion>"
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<minor>";

static wchar_t pwszDeviceElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
	L"<root>"
	L"<urn:schemas-upnp-org:device-1-0>"
    L"<device>";

static wchar_t pwszURLBaseElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
	L"<root>"
	L"<urn:schemas-upnp-org:device-1-0>"
    L"<URLBase>";


// Parse
HRESULT DeviceDescription::Parse(LPCSTR pszDescriptionURL)
{
	// download device description
	HttpRequest request;
	HRESULT		hr = E_FAIL;

	if(!request.Open("GET", pszDescriptionURL))
		return E_FAIL; // TO DO: error
	
	if(!request.Send())
	{
		TraceTag(ttidError, "DeviceDescription: Error sending request for device description: 0x%08x", request.GetHresult());
		return request.GetHresult();
	}
	
	if(HTTP_STATUS_OK != request.GetStatus())
	{
		TraceTag(ttidError, "DeviceDescription: Error getting device description: %d", request.GetStatus());
		return E_FAIL; // TO DO: error
    }

	ce::SequentialStream<HttpRequest>   Stream(request, upnp_config::max_document_size());
	ce::SAXReader                       Reader;

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

	// verify version
	if(SUCCEEDED(hr) && (m_strSpecVersionMajor != L"1" || m_strSpecVersionMinor != L"0"))
	{
	    TraceTag(ttidError, "DeviceDescription: Invalid document version");
	    hr = E_FAIL;
	}
	    
	if(SUCCEEDED(hr))
	{
	    if(!m_strBaseURLTag.empty())
		    // use the value from URLBase tag
		    ce::WideCharToMultiByte(CP_UTF8, m_strBaseURLTag, -1, &m_strBaseURL);
	    else
		    // use location of device description file as the base URL
		    m_strBaseURL = pszDescriptionURL;

	    // fix relative URLs
	    for(ce::list<Device*>::iterator it = m_listDevices.begin(), itEnd = m_listDevices.end(); it != itEnd; ++it)
		    (*it)->FixDeviceURL();
	}
	
	return hr;
}


// DeviceAddedCallback
void DeviceDescription::DeviceAddedCallback(Device* pDevice)
{
	m_listDevices.push_back(pDevice);
}


// startElement
HRESULT STDMETHODCALLTYPE DeviceDescription::startElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName,
    /* [in] */ ISAXAttributes __RPC_FAR *pAttributes)
{
    SAXContentHandler::startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
    
	if(m_bAbort)
	    return E_ABORT;
	
	if(!m_bParsedRootElement)
	{
	    // this is root element of the document
	    m_bParsedRootElement = true;
	    
	    // immediately terminate parsing/downloading if root element is not valid for UPnP device description
	    if(pwszRootElement != m_strFullElementName)
	    {
	        TraceTag(ttidError, "DeviceDescription: Invalid root element");
	        return E_FAIL;
	    }
	}
	
	if(m_pTempDevice)
		m_pTempDevice->startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
	else
		// Device element
		if(pwszDeviceElement == m_strFullElementName)
		{
			// start of root device
			// this section will be parsed by new device object
			m_pTempDevice = new Device(this);
			
			// simulate startDocument for the device
			m_pTempDevice->startDocument();
		}

    return S_OK;
}


// endElement
HRESULT STDMETHODCALLTYPE DeviceDescription::endElement( 
    /* [in] */ const wchar_t __RPC_FAR *pwchNamespaceUri,
    /* [in] */ int cchNamespaceUri,
    /* [in] */ const wchar_t __RPC_FAR *pwchLocalName,
    /* [in] */ int cchLocalName,
    /* [in] */ const wchar_t __RPC_FAR *pwchQName,
    /* [in] */ int cchQName)
{
	if(m_bAbort)
	    return E_ABORT;
	
	// URLBase
	if(pwszURLBaseElement == m_strFullElementName)
		m_strBaseURLTag.trim(L"\n\r\t ");
		
	// specVersion major
	if(pwszSpecVersionMajorElement == m_strFullElementName)
	{
	    m_strSpecVersionMajor.trim(L"\n\r\t ");
	    
	    if(m_strSpecVersionMajor != L"1")
	    {
	        TraceTag(ttidError, "DeviceDescription: Invalid document version");
	        return E_FAIL;
	    }
	}
	    
	// specVersion minor
	if(pwszSpecVersionMinorElement == m_strFullElementName)
	{
	    m_strSpecVersionMinor.trim(L"\n\r\t ");
	    
	    if(m_strSpecVersionMinor != L"0")
	    {
	        TraceTag(ttidError, "DeviceDescription: Invalid document version");
	        return E_FAIL;
	    }
	}
	
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

    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE DeviceDescription::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
	if(m_bAbort)
	    return E_ABORT;
	    
	// URLBase
	if(pwszURLBaseElement == m_strFullElementName)
		m_strBaseURLTag.append(pwchChars, cchChars);

	// specVersion major
	if(pwszSpecVersionMajorElement == m_strFullElementName)
		m_strSpecVersionMajor.append(pwchChars, cchChars);
		
	// specVersion minor
	if(pwszSpecVersionMinorElement == m_strFullElementName)
		m_strSpecVersionMinor.append(pwchChars, cchChars);
		
	if(m_pTempDevice)
		m_pTempDevice->characters(pwchChars, cchChars);

    return S_OK;
}

