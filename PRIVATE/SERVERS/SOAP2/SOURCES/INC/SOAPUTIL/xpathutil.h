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
// File:    xpathutil.h
//
// Contents:
//
//  Header File
//
//        utility routines for MSXML Xpath functions
//
//-----------------------------------------------------------------------------

#ifndef __XPATHUTIL_H_INCLUDED__
#define __XPATHUTIL_H_INCLUDED__

#include <msxml2.h>

class XPathState
{
public:
    XPathState() : m_pDoc(NULL)   {};
    ~XPathState();

    HRESULT init(IXMLDOMNode * pNode);
    HRESULT initDOMDocument(IXMLDOMDocument * pDOMDoc);
    HRESULT setLanguage(const WCHAR * pXPath);
    HRESULT addNamespace(const WCHAR *pNamespace);

private:
    CAutoRefc<IXMLDOMDocument2> m_pDoc;
    CVariant    m_vLanguage;
    CVariant    m_vNamespace;
};


HRESULT _XPATHGetNamespaceURIForPrefix(
    IXMLDOMNode * pNode,
    const WCHAR * pPrefix,
    BSTR * pbNamespaceURI);

HRESULT _XPATHFindAttribute(
    IXMLDOMNode * pNode,
    const WCHAR * pName,
    BSTR * pAttribute);

HRESULT _XPATHFindAttributeNode(
    IXMLDOMNode * pNode,
    const WCHAR * pName,
    IXMLDOMNode ** pResultNode);

HRESULT _XPATHUtilPrepareLanguage(
            IXMLDOMNode *pRootNode,
            const WCHAR * pSelectionNS);

HRESULT _XPATHUtilPrepareLanguage(
            IXMLDOMNode *pRootNode,
            WCHAR ** ppPrefix,
            WCHAR ** ppSelectionNS,
            const int   cb);


HRESULT _XPATHUtilPrepareLanguage(
            IXMLDOMNode *pRootNode,
            WCHAR * pPrefix,
            WCHAR * pSelectionNS);

HRESULT _XPATHUtilResetLanguage(IXMLDOMNode *pRootNode);

HRESULT _XPATHUtilFindNodeFromRoot(
            IXMLDOMNode *pNode,
            const WCHAR *pchElementsToFind,
            IXMLDOMNode **ppNode);

HRESULT _XPATHUtilFindNodeListFromRoot(
            IXMLDOMNode *pNode,
            const WCHAR *pchElementsToFind,
            IXMLDOMNodeList **ppNode);

HRESULT _XPATHUtilFindNodeList(
            IXMLDOMNode *pStartNode,
            const WCHAR *pchElementsToFind,
            IXMLDOMNodeList **ppNodeList);

HRESULT FollowHref(
            IXMLDOMNode **ppNode);

#endif  // __XPATHUTIL_H_INCLUDED__

// End Of File
