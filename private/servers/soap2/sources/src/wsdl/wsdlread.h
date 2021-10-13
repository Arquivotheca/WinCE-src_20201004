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
