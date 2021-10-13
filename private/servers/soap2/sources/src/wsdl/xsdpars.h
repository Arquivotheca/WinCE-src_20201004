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
//+----------------------------------------------------------------------------
//
// 
// File:    xsdpars.h
// 
// Contents:
//
//  Header File 
//
//		xsdparsing functionallity
//	
//
//-----------------------------------------------------------------------------
#ifndef __XSDPARS_H_INCLUDED__
#define __XSDPARS_H_INCLUDED__


     
    HRESULT _XSDFindURIForPrefix(IXMLDOMNode *pStartNode, TCHAR *pchPrefix, BSTR * pbstrURI);
    HRESULT _XSDLFindTargetSchema(TCHAR *pchURI, IXMLDOMNode *pDom, IXMLDOMNode **ppSchemaNode);
    HRESULT _XSDFindChildElements(IXMLDOMNode *pStartNode, IXMLDOMNodeList **ppResultList, TCHAR * pchElementName, int iIDS, BSTR *pbstrNSUri);
    HRESULT _XSDFindTargetNameSpace(IXMLDOMNode *pStartNode, BSTR *pbstrURI);


    schemaRevisionNr _XSDIsPublicSchema(TCHAR *pchURI);
    HRESULT _XSDFindRevision(IXMLDOMDocument2 *pDocument, schemaRevisionNr *pRevisionNr);
    HRESULT _XSDFindRevision(IXMLDOMNode *pNode, schemaRevisionNr *pRevisionNr);
    HRESULT _XSDFindRevision(ISoapSerializer* pSoapSerializer, schemaRevisionNr *pRevision);
    HRESULT _XSDFindNodeInSchemaNS(const TCHAR *pchQuery, IXMLDOMNode *pStartNode, IXMLDOMNode **ppNode);

    const TCHAR * _XSDSchemaNS(schemaRevisionNr revision);
    const TCHAR * _XSDSchemaInstanceNS(schemaRevisionNr revision);
    const TCHAR * _XSDSchemaInstanceXPATH(schemaRevisionNr revision);

    HRESULT _XSDAddSchemaNS(ISoapSerializer* pSoapSerializer, schemaRevisionNr revision, BSTR *pPrefix);
    HRESULT _XSDAddSchemaInstanceNS(ISoapSerializer* pSoapSerializer, schemaRevisionNr revision, BSTR *pPrefix);
    HRESULT _XSDAddTKDataNS(ISoapSerializer* pSoapSerializer, schemaRevisionNr revision, BSTR *pPrefix);
    HRESULT _XSDAddNS(ISoapSerializer* pSoapSerializer, BSTR *pPrefix, WCHAR *pchNS);

    HRESULT _XSDSetupDefaultXPath(IXMLDOMNode*pNode, schemaRevisionNr enRevision);



#endif
