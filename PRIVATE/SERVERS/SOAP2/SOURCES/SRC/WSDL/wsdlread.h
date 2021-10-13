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
// File:    wsdlread.h
//
// Contents:
//
//  Header File
//
//        IWSDLReader Interface describtion
//
//-----------------------------------------------------------------------------
#ifndef __WSDLREADER_H_INCLUDED__
#define __WSDLREADER_H_INCLUDED__

class CWSDLService;
class CEnumWSDLService;




class CWSDLReader : public IWSDLReader
{

public:
    CWSDLReader();
    ~CWSDLReader();

public:
    HRESULT STDMETHODCALLTYPE Load(WCHAR *pchWSDLFileSpec, WCHAR *pchSMLFileSpec);
    HRESULT STDMETHODCALLTYPE GetSoapServices(IEnumWSDLService **ppWSDLServiceEnum);
    HRESULT STDMETHODCALLTYPE ParseRequest(ISoapReader *pSoapReader,
                    IWSDLPort      **ppIWSDLPort,
                    IWSDLOperation **ppIWSDLOperation);

    HRESULT STDMETHODCALLTYPE setProperty (BSTR bstrPropname, VARIANT varPropValue);


    DECLARE_INTERFACE_MAP;

protected:
    HRESULT LoadDom(WCHAR *pchFileSpec, IXMLDOMDocument2 **ppDom);
    HRESULT AnalyzeWSDLDom(void);
    HRESULT AnalyzeWSMLDom(void);

private:
    CAutoRefc<IXMLDOMDocument2>    m_pWSDLDom;
    CAutoRefc<IXMLDOMDocument2>    m_pWSMLDom;
    CAutoRefc<CEnumWSDLService>    m_pServiceList;
    CAutoRefc<ISoapTypeMapperFactory> m_ptypeFactory;
#ifndef UNDER_CE
    bool                           m_bUseServerHTTPRequest;
#endif 
    bool                           m_bLoadOnServer;
    bool                           m_bInitSuccessfull;
    CCritSect                      m_critSect;
};


#endif
