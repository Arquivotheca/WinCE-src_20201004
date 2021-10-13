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

#include "Device.h"
#include "Service.h"
#include "HttpRequest.h"

static wchar_t pwszServiceElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<serviceList>"
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<service>";

static wchar_t pwszDeviceElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<deviceList>"
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<device>";

static wchar_t pwszIconElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<iconList>"
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<icon>";


// startElement
HRESULT STDMETHODCALLTYPE Device::startElement( 
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
		// parsing section for nested device
		m_pTempDevice->startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
	else
	{
		// Device element
		if(pwszDeviceElement == m_strFullElementName)
		{
			// start of new device (nested device)
			// this section will be parsed by new device object
			m_pTempDevice = new Device(m_pDeviceDescription, m_pRootDevice, this);
			
			// simulate startDocument for the nested device
			m_pTempDevice->startDocument();
		}

		// Service element
		if(pwszServiceElement == m_strFullElementName)
		{
			// starting to parse service
			m_strServiceType.resize(0);
			m_strServiceId.resize(0);
			m_strServiceDescriptionURL.resize(0);
			m_strServiceControlURL.resize(0);
			m_strServiceEventSubURL.resize(0);
		}

		// Icon element
		if(pwszIconElement == m_strFullElementName)
		{
			// starting parsing icon
			m_strIconWidth.resize(0);
			m_strIconHeight.resize(0);
			m_strIconDepth.resize(0);
			m_strIconURL.resize(0);
			m_strIconMimeType.resize(0);
		}
	}

    return S_OK;
}


// endElement
HRESULT STDMETHODCALLTYPE Device::endElement( 
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
		// finished parsing nested device;
		// simulate endDocument
		m_pTempDevice->endDocument();
		
		// add the device to collection
		if(m_pDevices)
			m_pDevices->AddItem(m_pTempDevice->m_strUDN, m_pTempDevice);
		
		m_bHasChildren = true;
		m_pTempDevice = NULL;
	}

	if(m_pTempDevice)
		// parsing section for nested device
		m_pTempDevice->endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
	else
	{
		// Service element
		if(pwszServiceElement == m_strFullElementName)
		{
			// trim white spaces
			m_strServiceType.trim(L"\n\r\t ");
			m_strServiceId.trim(L"\n\r\t ");
			m_strServiceDescriptionURL.trim(L"\n\r\t ");
			m_strServiceControlURL.trim(L"\n\r\t ");
			m_strServiceEventSubURL.trim(L"\n\r\t ");

			// convert URLs to absolute
			MakeAbsoluteURL(&m_strServiceDescriptionURL);
			MakeAbsoluteURL(&m_strServiceControlURL);
			MakeAbsoluteURL(&m_strServiceEventSubURL);
			
			// create Service object and add to collection
			if(Service *pService = new Service(m_strUDN, m_strServiceType, m_strServiceId, m_strServiceDescriptionURL, 
											   m_strServiceControlURL, m_strServiceEventSubURL, m_pDeviceDescription->m_nLifeTime, &m_pDeviceDescription->m_strBaseURL))
				if(m_pServices)
					m_pServices->AddItem(m_strServiceId, pService);
		}

		// Icon element
		if(pwszIconElement == m_strFullElementName)
		{
			// trim white spaces
			m_strIconWidth.trim(L"\n\r\t ");
			m_strIconHeight.trim(L"\n\r\t ");
			m_strIconDepth.trim(L"\n\r\t ");

			// add icon to the list
			m_listIcons.push_back(icon(m_strIconMimeType, m_strIconWidth, m_strIconHeight, m_strIconDepth, m_strIconURL));
		}
	}

    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE Device::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    if(m_pTempDevice)
		// parsing section for nested device
		m_pTempDevice->characters(pwchChars, cchChars);
	else
	{
		//
		// device properties
		//
		// deviceType
		if(L"<urn:schemas-upnp-org:device-1-0><deviceType>" == m_strFullElementName)
			m_strDeviceType.append(pwchChars, cchChars);

		// friendlyName
		if(L"<urn:schemas-upnp-org:device-1-0><friendlyName>" == m_strFullElementName)
			m_strFriendlyName.append(pwchChars, cchChars);

		// manufacturer
		if(L"<urn:schemas-upnp-org:device-1-0><manufacturer>" == m_strFullElementName)
			m_strManufacturer.append(pwchChars, cchChars);

		// manufacturerURL
		if(L"<urn:schemas-upnp-org:device-1-0><manufacturerURL>" == m_strFullElementName)
			m_strManufacturerURL.append(pwchChars, cchChars);

		// modelDescription
		if(L"<urn:schemas-upnp-org:device-1-0><modelDescription>" == m_strFullElementName)
			m_strModelDescription.append(pwchChars, cchChars);

		// modelName
		if(L"<urn:schemas-upnp-org:device-1-0><modelName>" == m_strFullElementName)
			m_strModelName.append(pwchChars, cchChars);

		// modelNumber
		if(L"<urn:schemas-upnp-org:device-1-0><modelNumber>" == m_strFullElementName)
			m_strModelNumber.append(pwchChars, cchChars);

		// modelURL
		if(L"<urn:schemas-upnp-org:device-1-0><modelURL>" == m_strFullElementName)
			m_strModelURL.append(pwchChars, cchChars);

		// serialNumber
		if(L"<urn:schemas-upnp-org:device-1-0><serialNumber>" == m_strFullElementName)
			m_strSerialNumber.append(pwchChars, cchChars);

		// UDN
		if(L"<urn:schemas-upnp-org:device-1-0><UDN>" == m_strFullElementName)
			m_strUDN.append(pwchChars, cchChars);

		// UPC
		if(L"<urn:schemas-upnp-org:device-1-0><UPC>" == m_strFullElementName)
			m_strUPC.append(pwchChars, cchChars);

		// presentationURL
		if(L"<urn:schemas-upnp-org:device-1-0><presentationURL>" == m_strFullElementName)
			m_strPresentationURL.append(pwchChars, cchChars);

		//
		// icon properties
		//
		static const int nIconLength = sizeof(pwszIconElement)/sizeof(*pwszIconElement) - 1;

		if(0 == wcsncmp(pwszIconElement, m_strFullElementName, nIconLength))
		{			
			// mimetype
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><mimetype>", static_cast<LPCWSTR>(m_strFullElementName) + nIconLength))
				m_strIconMimeType.append(pwchChars, cchChars);

			// width
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><width>", static_cast<LPCWSTR>(m_strFullElementName) + nIconLength))
				m_strIconWidth.append(pwchChars, cchChars);

			// height
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><height>", static_cast<LPCWSTR>(m_strFullElementName) + nIconLength))
				m_strIconHeight.append(pwchChars, cchChars);

			// depth
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><depth>", static_cast<LPCWSTR>(m_strFullElementName) + nIconLength))
				m_strIconDepth.append(pwchChars, cchChars);

			// url
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><url>", static_cast<LPCWSTR>(m_strFullElementName) + nIconLength))
				m_strIconURL.append(pwchChars, cchChars);
		}

		//
		// service properties
		//
		static const int nServiceLength = sizeof(pwszServiceElement)/sizeof(*pwszServiceElement) - 1;

		if(0 == wcsncmp(pwszServiceElement, m_strFullElementName, nServiceLength))
		{			
			// serviceType
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><serviceType>", static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
				m_strServiceType.append(pwchChars, cchChars);

			// serviceId
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><serviceId>", static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
				m_strServiceId.append(pwchChars, cchChars);

			// SCPDURL
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><SCPDURL>", static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
				m_strServiceDescriptionURL.append(pwchChars, cchChars);

			// controlURL
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><controlURL>", static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
				m_strServiceControlURL.append(pwchChars, cchChars);

			// eventSubURL
			if(0 == wcscmp(L"<urn:schemas-upnp-org:device-1-0><eventSubURL>", static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
				m_strServiceEventSubURL.append(pwchChars, cchChars);
		}
	}

    return S_OK;
}


// endDocument
HRESULT STDMETHODCALLTYPE Device::endDocument(void)
{
    // we finished parsing XML so now we can trim white spaces from string members
	m_strDeviceType.trim(L"\n\r\t ");
	m_strFriendlyName.trim(L"\n\r\t ");
	m_strManufacturer.trim(L"\n\r\t ");
	m_strManufacturerURL.trim(L"\n\r\t ");
	m_strModelDescription.trim(L"\n\r\t ");
	m_strModelName.trim(L"\n\r\t ");
	m_strModelNumber.trim(L"\n\r\t ");
	m_strModelURL.trim(L"\n\r\t ");
	m_strSerialNumber.trim(L"\n\r\t ");
	m_strUDN.trim(L"\n\r\t ");
	m_strUPC.trim(L"\n\r\t ");
	m_strPresentationURL.trim(L"\n\r\t ");

	m_pDeviceDescription->DeviceAddedCallback(this);

	return S_OK;
}


// FixDeviceURL
void Device::FixDeviceURL()
{
    // convert all device URLs to absolute
	MakeAbsoluteURL(&m_strManufacturerURL);
	MakeAbsoluteURL(&m_strModelURL);
	MakeAbsoluteURL(&m_strPresentationURL);
	
	// icon URL for all icons
	for(ce::list<icon>::iterator it = m_listIcons.begin(), itEnd = m_listIcons.end(); it != itEnd; ++it)
	    MakeAbsoluteURL(&it->m_strURL);
}


// MakeAbsoluteURL
void Device::MakeAbsoluteURL(ce::wstring* pstr)
{
	ce::string  strRelativeURL(INTERNET_MAX_URL_LENGTH);
	ce::string  strAbsoluteURL(INTERNET_MAX_URL_LENGTH);
	DWORD	    dw;

	if(!pstr->empty())
	{
	    wcstombs(strRelativeURL.get_buffer(), *pstr, strRelativeURL.capacity());
    	
	    if(InternetCombineUrlA(m_pDeviceDescription->m_strBaseURL, strRelativeURL, strAbsoluteURL.get_buffer(), &(dw = strAbsoluteURL.capacity()), 0))
	    {
		    pstr->reserve(strAbsoluteURL.length() + 1);
		    mbstowcs(pstr->get_buffer(), strAbsoluteURL, pstr->capacity());
	    }
	}
}
