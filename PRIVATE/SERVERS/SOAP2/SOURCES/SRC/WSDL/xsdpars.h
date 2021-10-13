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
