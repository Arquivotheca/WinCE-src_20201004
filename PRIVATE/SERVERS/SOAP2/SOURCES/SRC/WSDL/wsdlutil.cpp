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
// File:    wsdlutil.cpp
// 
// Contents:
//
//  implementation file 
//
//		_WSDLUtil  collection of helpful utils
//	
//	Created 
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "wsdlutil.h"




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilGetNodeText(IXMLDOMNode *pNode, BSTR *pbstrText)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilGetNodeText(IXMLDOMNode *pNode, BSTR *pbstrText)
{
	HRESULT hr = E_FAIL;

	ASSERT(pNode != 0);

	hr = pNode->get_text(pbstrText);
	if (FAILED(hr)) 
	{
		goto Cleanup;
	}

Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilFindFirstChild(IXMLDOMNode *pNode, IXMLDOMNode **ppChild)
//
//  parameters:
//
//  description:
//      takes a node, get's the childnodes and returns first childnode of type element
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilFindFirstChild(IXMLDOMNode *pNode, IXMLDOMNode **ppChild)
{
    HRESULT                   hr = S_OK;
    CAutoRefc<IXMLDOMNodeList> pNodeList;
    CAutoRefc<IXMLDOMNode>    pChild;
	DOMNodeType               type;

    CHK(pNode->get_childNodes(&pNodeList));
    while (pNodeList->nextNode(&pChild)==S_OK)
    {
        CHK(pChild->get_nodeType(&type));
        if (type == NODE_ELEMENT)
        {
            // found the first element child
            *ppChild= pChild.PvReturn();
            break;
        }
        pChild.Clear();                
    }

    if (!*ppChild)
        hr = S_FALSE;

Cleanup:
    return hr;
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////











/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilSplitQName(TCHAR *pchStringToStrip, TCHAR *pchPrefix, BSTR *pbstrName)
//
//  parameters:
//		string to search
//  description:
//		searches for the first ':' (colon) and returns pointer afterwards
//		if no colon found, returns first char
//      if prefix is passed in, copies prefix into it
//      if name is passed in, copies name into it
//  returns: 
//      S_OK
//      or E_OUTOFMEMORY
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilSplitQName(TCHAR *pchStringToStrip, TCHAR *pchPrefix, BSTR *pbstrName)
{
	TCHAR *pchTemp = pchStringToStrip;
    TCHAR *pchPrefixOrg = pchPrefix;
    HRESULT hr = S_OK; 

#ifndef UNDER_CE
    CHK_BOOL(pchTemp, S_OK);
#else //note: this code was okay, but prefast complained
    if(!pchTemp)
    {
        hr=S_OK;
        goto Cleanup;
    }
#endif

		
	// now find the start of the binding name
	while (pchTemp  && *pchTemp && *pchTemp != _T(':'))
	{
        if (pchPrefix)
        {
           *pchPrefix++ = *pchTemp;
           *pchPrefix = 0;            
        }

		pchTemp++;;
	}
#ifndef UNDER_CE
	if (*pchTemp==_T(':'))
#else 
    if(pchTemp && *pchTemp==_T(':'))
#endif 
	{
		pchTemp++;	    
	}
	else
	{
        pchTemp = pchStringToStrip;	    
        // no prefix
        if (pchPrefixOrg)
        {
            *pchPrefixOrg = 0; 
        }
	}

    
    if (pbstrName)
    {
        *pbstrName = ::SysAllocString(pchTemp);
        CHK_BOOL(*pbstrName, E_OUTOFMEMORY); 
    }

Cleanup:
	return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilGetRootNodeFromReader(ISoapReader *pSoapReader, IXMLDOMNode **pNode)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilGetRootNodeFromReader(ISoapReader *pSoapReader, IXMLDOMNode **pNode)
{
	HRESULT hr = E_FAIL;
	CAutoRefc<IXMLDOMDocument> 		pReturn=0;


	if (!pSoapReader)
	{
		hr = E_INVALIDARG;
		goto Cleanup;
	}

	hr = pSoapReader->get_DOM(&pReturn);
	if (FAILED(hr)) 
	{
		goto Cleanup;
	}
	hr = pReturn->QueryInterface(IID_IXMLDOMNode, (void**)pNode);
	if (FAILED(hr)) 
	{
		goto Cleanup;
	}


Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilGetStyle(IXMLDOMNode *pNode, BOOL *pIsDocuement)
//
//  parameters:
//      returns the style attribute, called from port and operation
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilGetStyle(IXMLDOMNode *pNode, BOOL *pIsDocument)
{
    HRESULT hr = E_FAIL;
    CAutoBSTR   bstrTemp;

    hr = _WSDLUtilFindAttribute(pNode, _T("style"), &bstrTemp);
    if (SUCCEEDED(hr))
    {
        if (_tcscmp(bstrTemp, g_pwstrDocument)==0)
        {
            *pIsDocument = true;
        }
        else
        {
            *pIsDocument = false;
        }
        
    }
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilGetStyleString(BSTR *pbstrStyle, BOOL bValue)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilGetStyleString(BSTR *pbstrStyle, BOOL bValue)
{
    if (!pbstrStyle)
        return(E_INVALIDARG);
        
    *pbstrStyle = ::SysAllocString(bValue ? g_pwstrDocument : g_pwstrRPC);
    if (!*pbstrStyle)
        return(E_OUTOFMEMORY);

    return (S_OK);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilFindExtensionInfo(IXMLDOMNode *pNode, BSTR *pbstrEncoding, BOOL *pbCreateHrefs)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilFindExtensionInfo(IXMLDOMNode *pNode, BSTR *pbstrEncoding, BOOL *pbCreateHrefs)
{

    CAutoRefc<IXMLDOMNode> pExtensionNode;
    CAutoBSTR               bstrHref; 
    HRESULT                 hr = S_OK;

    // find the prefered encoding if present, if NOT, default to UTF-8    
    // we pass this string into operation INIT and copy there, as the long term plan
    // is to have this per input/output part of the operation
    // right now just one per binding

    hr = pNode->selectSingleNode(_T("./ext:binding"), &pExtensionNode);
	if (hr == S_OK) 
	{
	    if (pbstrEncoding)
	    {
        	_WSDLUtilFindAttribute(pExtensionNode, (WCHAR*)g_pwstrEncodingAttribute, pbstrEncoding); 
	    }	
    	_WSDLUtilFindAttribute(pExtensionNode, (WCHAR*)g_pwstrCreateHREFs, &bstrHref); 
    }

    
    if (pbstrEncoding && !*pbstrEncoding)
    {
        *pbstrEncoding = ::SysAllocString(g_pwstrUTF8);
        CHK_BOOL(*pbstrEncoding, E_OUTOFMEMORY);
    }

    if (bstrHref)
    {
        *pbCreateHrefs = (*bstrHref == _T('1'));
    }
Cleanup:
    return (hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilFindDocumentation(IXMLDOMNode *pNode, BSTR *pbstrDocumentation)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilFindDocumentation(IXMLDOMNode *pNode, BSTR *pbstrDocumentation)
{
    HRESULT hr; 
    CAutoRefc<IXMLDOMNode> pDocumentation; 
    
	hr = pNode->selectSingleNode(_T(".//def:documentation"), &pDocumentation);
	if (SUCCEEDED(hr) && pDocumentation) 
	{
		// failure can be ignored, as documentation is not required
		CHK(_WSDLUtilGetNodeText(pDocumentation, pbstrDocumentation));
	}
    hr = S_OK;
Cleanup:
    return hr; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilReturnAutomationBSTR(BSTR *pbstrOut, const TCHAR *pchIn)
//
//  parameters:
//
//  description:
//      shortcut for parameter checking etc...
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _WSDLUtilReturnAutomationBSTR(BSTR *pbstrOut, const TCHAR *pchIn)
{
    if (!pbstrOut)
        return E_INVALIDARG;

    *pbstrOut = 0; 

    if (pchIn && _tcslen(pchIn) > 0)
    {
        *pbstrOut = ::SysAllocString(pchIn);
        if (!*pbstrOut)
            return E_OUTOFMEMORY;
    }

    return S_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

