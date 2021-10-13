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

// Replace
void Replace(LPWSTR pwsz, wchar_t wchOld, wchar_t wchNew)
{
	for (LPWSTR pwch = pwsz; *pwch; pwch++)
		if (*pwch == wchOld)
			*pwch = wchNew;
}


// FileToURL
//
// Make  a relative URL pointing to the named file
//
// input: \windows\upnp\sampledevice\foo.xml or sampledevice\foo.xml
// output: /upnp/sampledevice/foo.xml
//
BOOL HostedDevice::FileToURL(LPCWSTR pwszFileIn, ce::wstring* pstrURL)
{
	

	ce::wstring strFile(pwszFileIn);
	LPCWSTR	pwszFile = static_cast<LPCWSTR>(strFile);
	
	assert(pwszFile != static_cast<LPCWSTR>(*pstrURL));
	
	if (_wcsnicmp(c_szLocalWebRootDir, pwszFile, celems(c_szLocalWebRootDir) - 1) == 0)
	{
		// skip past the \Windows\upnp part
		pwszFile += celems(c_szLocalWebRootDir) - 1;
	}
	else 
		if (*pwszFile == '\\')
		{
			// should be under the UPNP directory
			return FALSE;
		}

	// TODO: check if the file is present
	
	*pstrURL = c_szURLPrefix;
	*pstrURL += pwszFile;

	// convert \ to /
	Replace(pstrURL->get_buffer(), L'\\', L'/');

	return TRUE;
}


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

static wchar_t pwszIconURLElement[] = 
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<iconList>"
    L"<urn:schemas-upnp-org:device-1-0>"
    L"<icon>"
	L"<urn:schemas-upnp-org:device-1-0>"
	L"<url>";

static wchar_t pwszUDNElement[] =
	L"<urn:schemas-upnp-org:device-1-0>"
	L"<UDN>";

static wchar_t pwszPresentationURLElement[] =
	L"<urn:schemas-upnp-org:device-1-0>"
	L"<presentationURL>";

static wchar_t pwszSCPDURLElement[] =
	L"<urn:schemas-upnp-org:device-1-0>"
	L"<SCPDURL>";

static wchar_t pwszControlURLElement[] =
	L"<urn:schemas-upnp-org:device-1-0>"
	L"<controlURL>";

static wchar_t pwszEventSubURL[] =
	L"<urn:schemas-upnp-org:device-1-0>"
	L"<eventSubURL>";


// startDocument
HRESULT STDMETHODCALLTYPE HostedDevice::startDocument(void)
{
    SAXContentHandler::startDocument();

    // create a UDN
	m_pRoot->CreateUDN(&m_pszUDN);

    return S_OK;
}


// startElement
HRESULT STDMETHODCALLTYPE HostedDevice::startElement( 
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
			m_pTempDevice = new HostedDevice(m_pRoot);
			
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

			*m_ppNextService = new HostedService(this);
		}

		// Icon element
		if(pwszIconURLElement == m_strFullElementName)
		{
			// starting parsing icon
			m_strIconURL.resize(0);

			// write
		}
	
		m_pRoot->SAXWriter()->startElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName, pAttributes);
	}

    return S_OK;
}


// endElement
HRESULT STDMETHODCALLTYPE HostedDevice::endElement( 
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
		*m_ppNextChildDev = m_pTempDevice;
		m_ppNextChildDev = &m_pTempDevice->m_pNextDev;
		
		m_pTempDevice = NULL;
	}

	if(m_pTempDevice)
		// parsing section for nested device
		m_pTempDevice->endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
	else
	{
		// UDN
		if(pwszUDNElement == m_strFullElementName)
		{
			m_strUDN.trim(L"\n\r\t ");
			m_pszOrigUDN = StrDupW(m_strUDN);

			m_pRoot->SAXWriter()->characters(m_pszUDN, wcslen(m_pszUDN));
		}
		
		// presentationURL
		if(pwszPresentationURLElement == m_strFullElementName)
		{
			m_strPresentationURL.trim(L"\n\r\t ");
			
			if(!m_strPresentationURL.empty())
			{
			    FileToURL(m_strPresentationURL, &m_strPresentationURL);
			
			    // add device description URL as query string
			    m_strPresentationURL += L"?";
			    m_strPresentationURL += c_szURLPrefix;
			    m_strPresentationURL += m_pRoot->DescFileName() + celems(c_szLocalWebRootDir) - 1;
			}
			
			m_pRoot->SAXWriter()->characters(m_strPresentationURL, m_strPresentationURL.size());
		}

		// Icon element
		if(pwszIconURLElement == m_strFullElementName)
		{
			// trim white spaces
			m_strIconURL.trim(L"\n\r\t ");

			FileToURL(m_strIconURL, &m_strIconURL);

			m_pRoot->SAXWriter()->characters(m_strIconURL, m_strIconURL.size());
		}

		//
		// service properties
		//
		static const int nServiceLength = sizeof(pwszServiceElement)/sizeof(*pwszServiceElement) - 1;

		if(0 == wcsncmp(pwszServiceElement, m_strFullElementName, nServiceLength))
		{			
			// SCPDURL
			if(0 == wcscmp(pwszSCPDURLElement, static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
			{
				m_strServiceDescriptionURL.trim(L"\n\r\t ");
				(*m_ppNextService)->m_pszSCPDPath = StrDupW(m_strServiceDescriptionURL);

				// fix service description URL
				FileToURL(m_strServiceDescriptionURL, &m_strServiceDescriptionURL);

				m_pRoot->SAXWriter()->characters(m_strServiceDescriptionURL, m_strServiceDescriptionURL.size());
			}

			// controlURL
			if(0 == wcscmp(pwszControlURLElement, static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
			{
				// set control URL
				m_strServiceControlURL.reserve(100);

				m_strServiceControlURL = L"/upnpisapi?";
				
				int nURI = m_strServiceControlURL.size();
				
				m_strServiceControlURL += UDN();
                m_strServiceControlURL += L"+";
				m_strServiceControlURL += m_strServiceId;

				// set URI to the query portion of the URL (UDN+ServiceID)
				(*m_ppNextService)->m_pszaUri = StrDupWtoA(static_cast<LPCWSTR>(m_strServiceControlURL) + nURI);

				m_pRoot->SAXWriter()->characters(m_strServiceControlURL, m_strServiceControlURL.size());
			}
			
			// eventSubURL
			if(0 == wcscmp(pwszEventSubURL, static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
			{
				// set eventing URL
				m_strServiceEventSubURL.reserve(100);

				m_strServiceEventSubURL = L"/upnpisapi?";
				
				m_strServiceEventSubURL += UDN();
                m_strServiceEventSubURL += L"+";
				m_strServiceEventSubURL += m_strServiceId;

				m_pRoot->SAXWriter()->characters(m_strServiceEventSubURL, m_strServiceEventSubURL.size());
			}
		}

		// Service element
		if(pwszServiceElement == m_strFullElementName)
		{
			// trim white spaces
			m_strServiceType.trim(L"\n\r\t ");
			m_strServiceId.trim(L"\n\r\t ");

			// copy data from strings using during parsing
			(*m_ppNextService)->m_pszServiceId = StrDupW(m_strServiceId);
			(*m_ppNextService)->m_pszServiceType = StrDupW(m_strServiceType);

			// add service to collection
			m_ppNextService = &(*m_ppNextService)->m_pNextService;
		}

		m_pRoot->SAXWriter()->endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
	}

    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE HostedDevice::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    if(m_pTempDevice)
		// parsing section for nested device
		m_pTempDevice->characters(pwchChars, cchChars);
	else
	{
		bool bSkipWriting = false;
		
		//
		// device properties
		//
		// deviceType
		if(L"<urn:schemas-upnp-org:device-1-0><deviceType>" == m_strFullElementName)
			m_strDeviceType.append(pwchChars, cchChars);

		// UDN
		if(pwszUDNElement == m_strFullElementName)
		{
			m_strUDN.append(pwchChars, cchChars);
			bSkipWriting = true;
		}

		// presentationURL
		if(pwszPresentationURLElement == m_strFullElementName)
		{
			m_strPresentationURL.append(pwchChars, cchChars);
			bSkipWriting = true;
		}

		//
		// icon properties
		//
		if(0 == wcscmp(pwszIconURLElement, m_strFullElementName))
		{			
			m_strIconURL.append(pwchChars, cchChars);
			bSkipWriting = true;
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
			if(0 == wcscmp(pwszSCPDURLElement, static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
			{
				m_strServiceDescriptionURL.append(pwchChars, cchChars);
				bSkipWriting = true;
			}

			// controlURL
			if(0 == wcscmp(pwszControlURLElement, static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
				// ignore controlURL from xml, it will be generated
				bSkipWriting = true;

			// eventSubURL
			if(0 == wcscmp(pwszEventSubURL, static_cast<LPCWSTR>(m_strFullElementName) + nServiceLength))
				// ignore controlURL from xml, it will be generated
				bSkipWriting = true;
		}

		if(!bSkipWriting)
			m_pRoot->SAXWriter()->characters(pwchChars, cchChars);
	}

    return S_OK;
}


// endDocument
HRESULT STDMETHODCALLTYPE HostedDevice::endDocument(void)
{
    // we finished parsing XML so now we can trim white spaces from string members
	m_strDeviceType.trim(L"\n\r\t ");

	// copy data from strings using during parsing
	m_pszDeviceType = StrDupW(m_strDeviceType);

	return S_OK;
}
