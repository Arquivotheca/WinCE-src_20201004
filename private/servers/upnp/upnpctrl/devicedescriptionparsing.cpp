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

#include "variant.h"
#include "HttpChannel.h"
#include "HttpRequest.h"
#include "upnp_config.h"

#include "DeviceDescription.h"
#include "Device.h"
#include "Dlna.h"

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
    HRESULT     hr = S_OK;
    DWORD dwChannelCookie = 0;
    DWORD dwRequestCookie = 0;
    HttpRequest *pRequest = NULL;

    // alloc channel, alloc and acquire request
    Chk(g_CJHttpChannel.Alloc(&dwChannelCookie));
    Chk(g_CJHttpRequest.Alloc(&dwRequestCookie));
    Chk(g_CJHttpRequest.Acquire(dwRequestCookie, pRequest));

    Chk(pRequest->Init(dwChannelCookie, pszDescriptionURL));
    Chk(pRequest->Open("GET"));
    Chk(pRequest->Send());
    ChkBool(hr == S_OK, E_FAIL);

    { // limit scope 
        ISAXXMLReader *pXMLReader = NULL;
        ce::SequentialStream<HttpRequest>   Stream(*pRequest, upnp_config::max_document_size());
        ce::SAXReader  Reader;
        Chk(Reader.Init());
        ChkBool(pXMLReader = Reader.GetXMLReader(), E_OUTOFMEMORY);

        VARIANT v;
        v.punkVal = NULL;
        Stream.QueryInterface(IID_ISequentialStream, (void**)&v.punkVal);
        v.vt = VT_UNKNOWN;

        pXMLReader->putContentHandler(this);
        hr = pXMLReader->parse(v);
        pXMLReader->putContentHandler(NULL);
        Chk(hr);

        if (v.punkVal)
        {
            v.punkVal->Release();
        }
    }

    // verify version
    ChkBool(m_strSpecVersionMajor == L"1", E_FAIL);
    if(!m_strBaseURLTag.empty())
    {
        // use the value from URLBase tag
        ce::WideCharToMultiByte(CP_UTF8, m_strBaseURLTag, -1, &m_strBaseURL);
    }
    else
    {
        // use location of device description file as the base URL
        m_strBaseURL = pszDescriptionURL;
    }

    // fix relative URLs
    for(ce::list<Device*>::iterator it = m_listDevices.begin(), itEnd = m_listDevices.end(); it != itEnd; ++it)
    {
        (*it)->FixDeviceURL();
    }

Cleanup:
    g_CJHttpRequest.Release(dwRequestCookie, pRequest);
    g_CJHttpRequest.Free(dwRequestCookie);
    g_CJHttpChannel.Free(dwChannelCookie);
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
    {
        return E_ABORT;
    }

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
            m_pTempDevice = new Device(this);
            if(!m_pTempDevice)
            {
                return ERROR_OUTOFMEMORY;
            }

            // simulate startDocument for the device
            m_pTempDevice->startDocument();
        }
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
    {
        return E_ABORT;
    }
    
    // URLBase
    if(pwszURLBaseElement == m_strFullElementName)
    {
        m_strBaseURLTag.trim(L"\n\r\t ");
    }
        
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
    {
        m_pTempDevice->endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
    }

    return SAXContentHandler::endElement(pwchNamespaceUri, cchNamespaceUri, pwchLocalName, cchLocalName, pwchQName, cchQName);
}


// characters
HRESULT STDMETHODCALLTYPE DeviceDescription::characters( 
    /* [in] */ const wchar_t __RPC_FAR *pwchChars,
    /* [in] */ int cchChars)
{
    if(m_bAbort)
    {
        return E_ABORT;
    }
        
    // URLBase
    if(pwszURLBaseElement == m_strFullElementName)
    {
        m_strBaseURLTag.append(pwchChars, cchChars);
    }

    // specVersion major
    if(pwszSpecVersionMajorElement == m_strFullElementName)
    {
        m_strSpecVersionMajor.append(pwchChars, cchChars);
    }
        
    // specVersion minor
    if(pwszSpecVersionMinorElement == m_strFullElementName)
    {
        m_strSpecVersionMinor.append(pwchChars, cchChars);
    }
        
    if(m_pTempDevice)
    {
        m_pTempDevice->characters(pwchChars, cchChars);
    }

    return S_OK;
}

