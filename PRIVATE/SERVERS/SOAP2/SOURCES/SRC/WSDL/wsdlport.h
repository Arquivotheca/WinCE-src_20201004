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
