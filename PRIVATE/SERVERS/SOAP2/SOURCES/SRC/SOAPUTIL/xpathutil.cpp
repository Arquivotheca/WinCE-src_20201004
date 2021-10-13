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
//        _WSDLUtil  collection of helpful utils
//
//-----------------------------------------------------------------------------
#include "headers.h"

#include "soapglo.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


XPathState::~XPathState()
{
    HRESULT hr=S_OK;

    if (! m_pDoc)
        return;

    CHK (m_pDoc->setProperty((BSTR)g_pwstrSelNamespaces, m_vNamespace));
    CHK (m_pDoc->setProperty((BSTR)g_pwstrSelLanguage, m_vLanguage));

Cleanup:
    ASSERT(SUCCEEDED(hr));
}


HRESULT XPathState::init(IXMLDOMNode * pNode)
{
    HRESULT hr=S_OK;
    CAutoRefc<IXMLDOMDocument>  pDOM1;

    CHK_BOOL(pNode, E_INVALIDARG);
    CHK_BOOL(m_pDoc == NULL, E_FAIL);

    CHK(pNode->get_ownerDocument(&pDOM1));

    if (pDOM1 == NULL)
    {
        // if pNode=NULL pnode 'are' the document
        pNode->AddRef();
        pDOM1 = (IXMLDOMDocument*)pNode;
    }

    CHK (initDOMDocument(pDOM1));

Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;
}


HRESULT XPathState::initDOMDocument(IXMLDOMDocument * pDOMDoc)
{
    HRESULT hr=S_OK;
    CVariant varIn;

    CHK_BOOL(pDOMDoc, E_INVALIDARG);
    CHK_BOOL(m_pDoc == NULL, E_FAIL);

    CHK(pDOMDoc->QueryInterface(IID_IXMLDOMDocument2, (void**) &m_pDoc));

    CHK (m_pDoc->getProperty((BSTR)g_pwstrSelNamespaces, &m_vNamespace));
    CHK (m_pDoc->getProperty((BSTR)g_pwstrSelLanguage, &m_vLanguage));

    // by default we are going to use Xpath
    varIn.Assign(g_pwstrXpathLanguage);
    CHK( m_pDoc->setProperty((BSTR)g_pwstrSelLanguage, varIn) );

    // reset all the namespaces
    varIn.Assign(g_pwstrEmpty);
    CHK( m_pDoc->setProperty((BSTR)g_pwstrSelNamespaces, varIn) );

Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;
}



HRESULT XPathState::setLanguage(const WCHAR * pXPath)
{
    HRESULT hr=S_OK;
    CVariant varIn;

    CHK_BOOL(m_pDoc, E_FAIL);

    // pre default we are going to set the xpath language
    if (pXPath)
    {
        varIn.Assign(pXPath);
        CHK( m_pDoc->setProperty((BSTR)g_pwstrSelLanguage, varIn) );
    }

Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;
}


HRESULT XPathState::addNamespace(const WCHAR *pNamespace)
{
    HRESULT hr=S_OK;
#ifdef CE_NO_EXCEPTIONS
    WCHAR *pBuffer = NULL;
#endif


    CHK_BOOL(m_pDoc, E_FAIL);

    if (pNamespace)
    {
        CVariant varIn;
        long len = wcslen(pNamespace)+2;
#ifndef CE_NO_EXCEPTIONS     
        WCHAR * pBuffer;
#endif
        BSTR bstr;

        CHK( m_pDoc->getProperty((BSTR)g_pwstrSelNamespaces, &varIn) );

        bstr = V_BSTR(&varIn);
        if (bstr)
            len = len+::SysStringLen(bstr);
#ifndef UNDER_CE
        pBuffer = (WCHAR *) _alloca(sizeof(WCHAR) * len);
#else
#ifdef CE_NO_EXCEPTIONS
        pBuffer = new WCHAR[len];
        if(!pBuffer)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }
#else
        try{
        pBuffer = (WCHAR *) _alloca(sizeof(WCHAR) * len);
        }

        catch(...){
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }
#endif
#endif

        wcscpy(pBuffer, bstr);
        wcscat(pBuffer, L" ");
        wcscat(pBuffer, pNamespace);

        varIn.Assign(pBuffer);

        CHK( m_pDoc->setProperty((BSTR)g_pwstrSelNamespaces, varIn) );

    }

Cleanup:
#ifdef CE_NO_EXCEPTIONS
    if(pBuffer)
        delete [] pBuffer;
#endif

    ASSERT (SUCCEEDED(hr));
    return hr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Method:      _XPATHGetNamespaceURIForPrefix
//
// Synopsis:    Given a prefix and a DOM node, find a corresponding namespace
//              in scope.
//
// Arguments:   pNode -[in]  xml dom node
//              pPrefix     - [in]  namespace prefix
//              pbNamespaceURI - [out] namespace-uri
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHGetNamespaceURIForPrefix
    (
    IXMLDOMNode * pNode,
    const WCHAR * pPrefix,
    BSTR * pbNamespaceURI
    )
{
    HRESULT hr;
    CAutoRefc<IXMLDOMNode>  pResult;
    XPathState xp;

    WCHAR acBuffer[250];
    *pbNamespaceURI = NULL;

    ASSERT(pNode && pPrefix && pbNamespaceURI);

    xp.init(pNode);
    swprintf(acBuffer, L"ancestor-or-self::*[namespace::%.100s]/namespace::%.100s", pPrefix, pPrefix);

    showNode (pNode);

    CHK (pNode->selectSingleNode( acBuffer, &pResult));

    if (pResult)
    {
        showNode(pResult);
        CHK(pResult->get_text(pbNamespaceURI));
    }


Cleanup:
    ASSERT(SUCCEEDED(hr));
    return hr;
}





/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Method:      _XPATHFindAttribute
//
// Synopsis:    Given a attribute name a DOM node, find a corresponding attribute value
//
//
// Arguments:   pNode -[in]  xml dom node
//              pName - [in]  attribute name including prefix
//              pAttribute - [out] attribute
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHFindAttribute(
    IXMLDOMNode * pNode,
    const WCHAR * pName,
    BSTR * pAttribute)
{
    HRESULT hr(S_OK);
    CAutoRefc<IXMLDOMNode>  pResult(NULL);
    
    CHK_BOOL(pAttribute, E_INVALIDARG);
    *pAttribute = NULL;
    
    CHK (_XPATHFindAttributeNode(pNode, pName, &pResult));

    if (pResult)
    {
        //showNode(pResult);
        CHK(pResult->get_text(pAttribute));
    }


Cleanup:
    ASSERT(SUCCEEDED(hr));
    return hr;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Method:      _XPATHFindAttributeNode
//
// Synopsis:    Given a attribute name and a DOM node, find a corresponding attribute value and return the node
//        
//
// Arguments:   pNode -[in]  xml dom node
//              pName - [in]  attribute name including prefix
//              pResultNode - [out] attribute node
//
// Result: node is NULL if nothing found (and S_FALSE)
//  node and S_OK
// E_INVALIDARG
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHFindAttributeNode(
    IXMLDOMNode * pNode,
    const WCHAR * pName,
    IXMLDOMNode ** pNodeResult)
{
    HRESULT hr(S_OK);

    WCHAR acBuffer[250];

    CHK_BOOL(pNodeResult, E_INVALIDARG);
    *pNodeResult = NULL;
    
    CHK_BOOL(pNode, E_INVALIDARG);
    CHK_BOOL(pName, E_INVALIDARG);

    swprintf(acBuffer, L"./@%.200s", pName);

    //showNode (pNode);
    CHK (pNode->selectSingleNode(acBuffer, pNodeResult));

Cleanup:
    ASSERT(SUCCEEDED(hr));
    return hr;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XPATHUtilPrepareLanguage(IXMLDOMNode *pRootNode, TCHAR *pchPrefix, TCHAR *pchNameSpace)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHUtilPrepareLanguage(
            IXMLDOMNode *pRootNode,
            const WCHAR * pSelectionNS)
{
    HRESULT hr = E_FAIL;

    CAutoRefc<IXMLDOMDocument2> pDoc;
    VARIANT     varIn;

    VariantInit(&varIn);

    CHK (pRootNode->QueryInterface(IID_IXMLDOMDocument2, (void**)&pDoc));

    V_VT(&varIn)    = VT_BSTR;
    V_BSTR(&varIn)  = L"XPath";

    CHK( pDoc->setProperty(L"SelectionLanguage", varIn) );

    V_BSTR(&varIn) = ::SysAllocString(pSelectionNS);

    hr = pDoc->setProperty(L"SelectionNamespaces", varIn);
    ::SysFreeString(V_BSTR(&varIn));

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XPATHUtilPrepareLanguage(IXMLDOMNode *pRootNode, TCHAR *pchPrefix, TCHAR *pchNameSpace)
//
//  parameters:
//      pPrefix: namespace prefix
//      pSelectionNS: namespace URI
//  description:
//     just contacts the prefix and the URI and calls the real implemenation
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHUtilPrepareLanguage(
            IXMLDOMNode *pRootNode,
            WCHAR ** ppPrefix,
            WCHAR ** ppSelectionNS,
            const int      cb)
{
    WCHAR *pchBuffer=0;
    HRESULT hr;
    int     iLen=0;

#ifndef UNDER_CE
    for (int i =0;i<cb ;i++ )
    {
        iLen += wcslen(ppPrefix[i])+wcslen(ppSelectionNS[i])+15;
    }
#else
    for (int j =0;j<cb ;j++ )
    {
        iLen += wcslen(ppPrefix[j])+wcslen(ppSelectionNS[j])+15;
    }
#endif

#ifdef UNDER_CE
    int i = 0;
#endif 


    pchBuffer = new WCHAR[iLen];
    if (!pchBuffer)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

#ifndef UNDER_CE 
    for (int i = 0;i<cb ;i++ )
#else
    for (i = 0; i<cb; i++)
#endif
    {
        if (i==0)
        {
            wcscpy(pchBuffer, L"xmlns:");
        }
        else
        {
            wcscat(pchBuffer, L" xmlns:");
        }

        wcscat(pchBuffer, ppPrefix[i]);
        wcscat(pchBuffer, L"=\"");
        wcscat(pchBuffer, ppSelectionNS[i]);
        wcscat(pchBuffer, L"\"");
    }


    hr = _XPATHUtilPrepareLanguage(pRootNode, pchBuffer);


Cleanup:
    ASSERT(hr==S_OK);
    delete [] pchBuffer;
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XPATHUtilPrepareLanguage(IXMLDOMNode *pRootNode, TCHAR *pchPrefix, TCHAR *pchNameSpace)
//
//  parameters:
//      pPrefix: namespace prefix
//      pSelectionNS: namespace URI
//  description:
//     just contacts the prefix and the URI and calls the real implemenation
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHUtilPrepareLanguage(
            IXMLDOMNode *pRootNode,
            WCHAR * pPrefix,
            WCHAR * pSelectionNS)
{

    return(_XPATHUtilPrepareLanguage(pRootNode, &pPrefix, &pSelectionNS, 1));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////








/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XPATHUtilResetLanguage(IXMLDOMNode *pRootNode)
//
//  parameters:
//
//  description:
//      resets the selection language to default
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHUtilResetLanguage(IXMLDOMNode *pRootNode)
{
    HRESULT hr = E_FAIL;
    VARIANT     varIn;
    CAutoRefc<IXMLDOMDocument2> pDoc;

    VariantInit(&varIn);

    CHK (pRootNode->QueryInterface(IID_IXMLDOMDocument2, (void**)&pDoc));

    V_VT(&varIn)    = VT_BSTR;
    V_BSTR(&varIn)  = L"XSLPattern";

    hr = pDoc->setProperty(L"SelectionLanguage", varIn);

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilFindNodeListFromRoot
//
//  parameters:
//
//  description:
//        finds the root node, then searches for the first element with
//      a given pattern
//
//  returns:
//      E_FAIL if pattern not found
//      MSXML specific error result
//      S_OK if not problem
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHUtilFindNodeListFromRoot(
            IXMLDOMNode     *pNode,
            const WCHAR     *pchElementsToFind,
            IXMLDOMNodeList **ppNodeList)
{
    HRESULT                     hr = E_FAIL;
    CAutoRefc<IXMLDOMDocument>  pDoc;
    CAutoRefc<IXMLDOMNode>      pXDN;

    ASSERT(pNode!=0);
    ASSERT(pchElementsToFind!=0);

    CHK_BOOL(ppNodeList!=0, E_INVALIDARG);
    *ppNodeList = NULL;


    CHK ( pNode->get_ownerDocument(&pDoc) );

    if (pDoc == NULL)
    {
        // we are already at the root and pDoc is NULL (msxml behavior)
        pXDN = pNode;
        pNode->AddRef();
    }
    else
    {
        CHK ( pDoc->QueryInterface(IID_IXMLDOMNode, (void **)&pXDN));
    }

    CHK(_XPATHUtilFindNodeList(pXDN, pchElementsToFind, ppNodeList));

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _WSDLUtilFindNodeFromRoot
//
//  parameters:
//
//  description:
//        finds the root node, then searches for the first element with
//      a given pattern
//
//  returns:
//      E_FAIL if pattern not found
//      MSXML specific error result
//      S_OK if not problem
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHUtilFindNodeFromRoot(
            IXMLDOMNode *pNode,
            const WCHAR *pchElementsToFind,
            IXMLDOMNode **ppNode)
{
    HRESULT                     hr = E_FAIL;
    CAutoRefc<IXMLDOMDocument>  pDoc;
    CAutoRefc<IXMLDOMNode>      pXDN;

    ASSERT(pNode!=0);
    ASSERT(pchElementsToFind!=0);
    ASSERT(ppNode!=0);

    //make sure the return result is NULLed out
    *ppNode = NULL;

    CHK ( pNode->get_ownerDocument(&pDoc) );

    if (pDoc == NULL)
    {
        // we are already at the root and pDoc is NULL (msxml behavior)
        pXDN = pNode;
        pNode->AddRef();
    }
    else
    {
        CHK ( pDoc->QueryInterface(IID_IXMLDOMNode, (void **)&pXDN));
    }


    CHK ( pXDN->selectSingleNode((BSTR) pchElementsToFind, ppNode) );
    CHK_BOOL(*ppNode != NULL, E_FAIL);

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT _XPATHUtilDoNodesExist
//
//  parameters:
//
//  description:
//        searches for a subnode list from the given startnode based on pattern
//
//  returns:
//      E_FAIL if pattern not found -> checks if result list is 0 length to determine E_FAIL
//      MSXML specific error result
//      S_OK if not problem
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT _XPATHUtilFindNodeList(IXMLDOMNode *pStartNode,
                             const WCHAR *pchElementsToFind,
                             IXMLDOMNodeList **ppNodeList)
{
      CAutoRefc<IXMLDOMNodeList>  pNList;
    HRESULT                     hr = E_FAIL;
    LONG                        lLen;


    ASSERT(pStartNode!=0);
    ASSERT(pchElementsToFind!=0);

    CHK_BOOL(ppNodeList!=0, E_INVALIDARG);
    *ppNodeList = NULL;

    CHK ( pStartNode->selectNodes((BSTR) pchElementsToFind, &pNList) );
    CHK_BOOL(pNList != NULL, E_FAIL);

    CHK (pNList->get_length(&lLen));

    // if the len is 0, there are no nodes found
    CHK_BOOL(lLen > 0, E_FAIL);

     // the pattern was found, put it into the output variable
    *ppNodeList = pNList.PvReturn();

Cleanup:
    return (hr);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT FollowHref
//
//  parameters:
//
//  description:
//      function checks if an HREF definition is used.
//      if there it follows the href
//
//  returns:
//      the function will return S_OK in the successful case. In this case *ppNode will point to the node
//      to continue processing on. This can be the original node, or it can be the original node,
//      or the node retrieved from following the href.
//
//      in the error case **pNode will not be effected, but error can be occuring due to program errors
//      or due to wrong href's
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT FollowHref(IXMLDOMNode **ppNode)
{
    HRESULT hr = S_OK;
    CAutoBSTR bstrHref;
    IXMLDOMNode* pNode = NULL;

    ASSERT (ppNode);
    CHK_BOOL(*ppNode, E_INVALIDARG);

    showNode(*ppNode);

    if (SUCCEEDED(_WSDLUtilFindAttribute(*ppNode, g_pwstrHref, &bstrHref)))
    {
        XPathState xp;
        CAutoFormat autoFormat;
        WCHAR * pchhref = bstrHref;

        pNode = *ppNode;

        // add the envelope to the search namespace
        xp.init(pNode);
        // add the envelope to the search namespace
        CHK (xp.addNamespace(g_pwstrXpathEnv)); //give us env:envelopeNamespace

        if (*bstrHref == _T('#'))
            pchhref++;
        CHK_BOOL(*pchhref, E_INVALIDARG);

        CHK(autoFormat.sprintf(_T("env:Envelope//*[@id='%s']"), pchhref));

        // this returns E_FAIL if no node found
        hr = _XPATHUtilFindNodeFromRoot(pNode, &autoFormat, &pNode);
        ERROR_ONHR1( hr, WSML_IDS_COULDNOTFINDHREF, WSDL_IDS_MAPPER, bstrHref);
        }

Cleanup:
    if (SUCCEEDED(hr) && pNode)
    {
        // whatever was in the input node has to get released
        (*ppNode)->Release();

        // and take the new answer
        *ppNode = pNode;

        // no need to release pNode, if we make it here, we have a successful hr,
        // which means we have the node addrefed in FindNodeFromRoot,
        // and need to preserve the addref.
    }
    return (hr);
}


