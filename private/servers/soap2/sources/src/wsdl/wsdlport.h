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
// File:    wsdlport.h
//
// Contents:
//
//  Header File
//
//        IWSDLPort Interface describtion
//
//-----------------------------------------------------------------------------
#ifndef __WSDLPORT_H_INCLUDED__
#define __WSDLPORT_H_INCLUDED__


class CEnumWSDLOperations;

class CWSDLPort : public IWSDLPort
{

public:
    CWSDLPort();
    ~CWSDLPort();

public:
    HRESULT STDMETHODCALLTYPE get_name(BSTR *pbstrName);
    HRESULT STDMETHODCALLTYPE get_address(BSTR *pbstrAddress);
    HRESULT STDMETHODCALLTYPE get_bindStyle(BSTR *pbstrbindStyle);
    HRESULT STDMETHODCALLTYPE get_transport(BSTR *pbstrtransport);
    HRESULT STDMETHODCALLTYPE get_documentation(BSTR *bstrServiceDocumentation);
    HRESULT STDMETHODCALLTYPE GetSoapOperations(IEnumWSDLOperations **ppIWSDLOperations);

    DECLARE_INTERFACE_MAP;

public:
    HRESULT Init(IXMLDOMNode *pPortNode, ISoapTypeMapperFactory *ptypeFactory);
    HRESULT AddWSMLMetaInfo(IXMLDOMNode *pServiceNode,
                            CWSDLService *pWSDLService,
                            IXMLDOMDocument *pWSDLDom,
                            IXMLDOMDocument *pWSMLDom,
                            bool bLoadOnServer);

protected:
    HRESULT AnalyzeBinding(IXMLDOMNode *pBinding, ISoapTypeMapperFactory *ptypeFactory);


private:
    BOOL            m_fDocumentMode;
    CAutoBSTR       m_bstrDocumentation;
    CAutoBSTR       m_bstrName;
    CAutoBSTR       m_bstrBinding;
    CAutoBSTR       m_bstrLocation;
    CAutoBSTR       m_bstrPortType;
    CAutoBSTR       m_bstrBindTransport;

    CAutoRefc<CEnumWSDLOperations>         m_pOperationsEnum;
};

#endif
