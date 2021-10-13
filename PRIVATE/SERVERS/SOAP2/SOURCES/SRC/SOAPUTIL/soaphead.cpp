//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
// 
// File:    soaphead.cpp
// 
// Contents:
//
//  implementation file 
//
//        soapheader processing utilities
//    
//
//-----------------------------------------------------------------------------
#include "headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapHeaders::CSoapHeaders()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapHeaders::CSoapHeaders()
{
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapHeaders::~CSoapHeaders()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapHeaders::~CSoapHeaders()
{
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapHeaders::Init()
//
//  parameters:
//
//  description:
//      currently, does nothing, but I am sure we will need it later
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapHeaders::Init()
{
    return S_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapHeaders::AddHeaderHandler(IHeaderHandler *pHeaderHandler)
//
//  parameters:
//
//  description:
//      adds a new header handler - currently only one supported
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapHeaders::AddHeaderHandler(IHeaderHandler *pHeaderHandler)
{
    m_pHeaderHandler.Clear();
    if (pHeaderHandler)
    {
        m_pHeaderHandler = pHeaderHandler;
        m_pHeaderHandler->AddRef();
    }    
    return S_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapHeaders::ReadHeaders(ISoapReader *pReader, IUnknown *pObject)
//
//  parameters:
//
//  description:
//      walks over the headers and calls the header handlers to read them
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapHeaders::ReadHeaders(ISoapReader *pReader, IDispatch *pObject)
{
    HRESULT hr = S_OK;
    CAutoRefc<IXMLDOMNodeList> pNodeList; 
    CAutoRefc<IXMLDOMNode>    pHeaderNode;
    VARIANT_BOOL              bUnderstood;

    if (pReader->get_HeaderEntries(&pNodeList)==S_OK)
    {
        while (((hr = pNodeList->nextNode(&pHeaderNode))==S_OK) && pHeaderNode != 0)
    	{
    	    bUnderstood = VARIANT_FALSE;
    	    
    	    if (m_pHeaderHandler)
    	    {
        	    CHK(m_pHeaderHandler->readHeader(pHeaderNode, pObject, &bUnderstood));
    	    }    
    	    if (bUnderstood == VARIANT_FALSE)
    	    {
    	        // we are reading the request, and we must check if a MUSTUNDERSTAND header
    	        // was NOT understood
    	        XPathState xp;
                CAutoRefc<IXMLDOMNode> pMustUnderstandNode(NULL);
                CHK (xp.init(pHeaderNode));
                CHK (xp.addNamespace(L"xmlns:env='http://schemas.xmlsoap.org/soap/envelope/'")); //give us env: for sopa-envelope

                // search for env: ...
                CHK (_XPATHFindAttributeNode(pHeaderNode, L"env:mustUnderstand", &pMustUnderstandNode));

                if (pMustUnderstandNode)
                {
                    CAutoBSTR bstrRes;
                    
                    // get the text value
                    hr = pMustUnderstandNode->get_text(&bstrRes);
                    if (hr != S_OK)
    	            {
    	                // we had a mustanderstand, but without text, report as an error
    	                hr = WSDL_MUSTUNDERSTAND;
    	                goto Cleanup;
    	            }

                    if (bstrRes)
        	        {
        	            if (_tcscmp(bstrRes, _T("0") ))
        	            {
        	                // we had a mustanderstand, it was not handled, and it is not set defined as "0"
        	                // this is an error
        	                hr = WSDL_MUSTUNDERSTAND;
        	                goto Cleanup;
        	            }
        	        }

                    hr = S_OK;
                    
                }
    	    }
    	    
    	    pHeaderNode.Clear();
        }
    }

Cleanup:
    if (FAILED(hr))
    {
        globalAddErrorFromErrorInfo(0, hr);
        globalAddError(IDS_SOAPHEADER_READFAILED, SOAP_IDS_SOAPHEADERS, hr);
    }
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapHeaders::WriteHeaders(ISoapSerializer *pSerializer, IUnknown *pObject)
//
//  parameters:
//
//  description:
//      walks over the header handlers and tells them to write
//      note: only one handler for now
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapHeaders::WriteHeaders(ISoapSerializer *pSerializer, IDispatch *pObject)
{
    HRESULT hr = S_OK;
    VARIANT_BOOL vbBool; 
    BOOL    bHeadersOpen = false;
    
    if (!m_pHeaderHandler)
    {
        goto Cleanup;
    }

    CHK(m_pHeaderHandler->willWriteHeaders(&vbBool));

    if (vbBool == VARIANT_TRUE)
    {
        CHK(pSerializer->startHeader(0));
        bHeadersOpen =true; 
        CHK(m_pHeaderHandler->writeHeaders(pSerializer, pObject));
        CHK(pSerializer->endHeader());
    }

Cleanup:    
    if (FAILED(hr))
    {
        if (bHeadersOpen)
        {
            // try to close them, so that we can write a fault. ignore error here.
            pSerializer->endHeader();
        }
        globalAddErrorFromErrorInfo(0, hr);
        globalAddError(IDS_SOAPHEADER_WRITEFAILED, SOAP_IDS_SOAPHEADERS, hr);
    }
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilFindAttribute(IXMLDOMNode *pNode, TCHAR *pchAttributeToFind, BSTR *pbstrValue)
//
//  parameters:
//
//  description:
//        finds a given attribute for a given node 
//        returns E_FAIL if attribute is not found
//        returns E_FAIL if node was 0 
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilFindAttribute(IXMLDOMNode *pNode, const TCHAR *pchAttributeToFind, BSTR *pbstrResult)
{
    HRESULT hr = E_FAIL;
    CAutoRefc<IXMLDOMNamedNodeMap> pIXMLDOMNamedNodeMap = 0;
    CAutoRefc<IXMLDOMNode> pIXMLDOMNode = 0;
    VARIANT varValue;



    ASSERT(pchAttributeToFind != 0);
    ASSERT(pbstrResult);

    VariantInit(&varValue);

    if (pNode==0)
        goto Cleanup;

    hr = pNode->get_attributes(&pIXMLDOMNamedNodeMap);
    if (FAILED(hr) || pIXMLDOMNamedNodeMap == 0) 
    {
        goto Cleanup;
    }
    
    hr = pIXMLDOMNamedNodeMap->getNamedItem((BSTR) pchAttributeToFind, &pIXMLDOMNode);
    if(SUCCEEDED(hr) && pIXMLDOMNode)
    {
        hr = pIXMLDOMNode->get_nodeValue(&varValue);
        if (FAILED(hr)) 
        {
            goto Cleanup;
        }
        if (V_VT(&varValue)==VT_BSTR)
        {
            // just disconnect the bstr
            *pbstrResult = V_BSTR(&varValue);
            V_BSTR(&varValue) = 0; 
        }
        else
        {
            hr = E_FAIL;
            goto Cleanup;
        }
    }
        

Cleanup:
    if (hr != S_OK)
    {
        hr = E_FAIL;
    }
    VariantClear(&varValue);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

