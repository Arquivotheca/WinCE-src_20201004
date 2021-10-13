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
// File:    wsdlport.cpp
//
// Contents:
//
//  implementation file
//
//        IWSDLPort Interface implemenation
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "wsdlserv.h"
#include "wsdlport.h"
#include "wsdloper.h"
#include "enwsdlop.h"


BEGIN_INTERFACE_MAP(CWSDLPort)
    ADD_IUNKNOWN(CWSDLPort, IWSDLPort)
    ADD_INTERFACE(CWSDLPort, IWSDLPort)
END_INTERFACE_MAP(CWSDLPort)


/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLPort::CWSDLPort()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLPort::CWSDLPort()
{
    m_pOperationsEnum = 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLPort::~CWSDLPort()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLPort::~CWSDLPort()
{
    CAutoRefc<CWSDLOperation> pWSDLOperation=0;
    long    lFetched;

    TRACEL((2, "Port shutdown %S\n",  m_bstrName));
    if (m_pOperationsEnum)
    {

        m_pOperationsEnum->Reset();

        while(m_pOperationsEnum->Next(1, (IWSDLOperation**)&pWSDLOperation, &lFetched)==S_OK)
        {
            ((CWSDLOperation*)pWSDLOperation)->Shutdown();
            release(&pWSDLOperation);
        }
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLPort::get_name(BSTR *pbstrPortName)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLPort::get_name(BSTR *pbstrPortName)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrPortName, m_bstrName);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLPort::get_documentation(BSTR *bstrServicedocumentation)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLPort::get_documentation(BSTR *bstrServicedocumentation)
{
    return _WSDLUtilReturnAutomationBSTR(bstrServicedocumentation, m_bstrDocumentation);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLPort::get_address(BSTR *pbstrAddress)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLPort::get_address(BSTR *pbstrAddress)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrAddress , m_bstrLocation);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLPort::get_bindStyle(BSTR *pbstrbindStyle)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLPort::get_bindStyle(BSTR *pbstrbindStyle)
{

    return(_WSDLUtilGetStyleString(pbstrbindStyle, m_fDocumentMode));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLPort::get_transport(BSTR *pbstrtransport)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLPort::get_transport(BSTR *pbstrtransport)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrtransport, m_bstrBindTransport);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLPort::GetSoapOperations(IEnumWSDLOperations **ppIWSDLOperations)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLPort::GetSoapOperations(IEnumWSDLOperations **ppIWSDLOperations)
{
    HRESULT hr = S_OK;

    ASSERT(m_pOperationsEnum!=0);

    if (ppIWSDLOperations==0)
        return(E_INVALIDARG);

    hr = m_pOperationsEnum->Clone(ppIWSDLOperations);

    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLPort::Init(IXMLDOMNode *pPortNode, ISoapTypeMapperFactory * ptypeFactory)
//
//  parameters:
//
//  description:
//
//  returns:
//      S_OK: successfully init
//      S_FALSE: not a SOAP port
//      error : one of the errors that the XML methods can throw
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLPort::Init(IXMLDOMNode *pPortNode, ISoapTypeMapperFactory * ptypeFactory)
{
    HRESULT         hr = E_FAIL;

    CAutoRefc<IXMLDOMNode>         pBinding=0;
    CAutoRefc<IXMLDOMNode>         pLocation=0;
    CAutoRefc<IXMLDOMNodeList>    pNodeList=0;

    CAutoBSTR                   bstrTemp;


    ASSERT(pPortNode != 0);

    hr = _WSDLUtilFindAttribute(pPortNode, _T("name"), &m_bstrName);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_PORTNONAME, WSDL_IDS_PORT,hr);
        goto Cleanup;
    }

    CHK(_WSDLUtilFindDocumentation(pPortNode, &m_bstrDocumentation));

    TRACEL((2, "Port Init %S\n",  m_bstrName));

    hr = _WSDLUtilFindAttribute(pPortNode, _T("binding"), &bstrTemp);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_PORTNOBINDING, WSDL_IDS_PORT,hr, m_bstrName);
        goto Cleanup;
    }
    // now find the start of the binding name

    CHK(_WSDLUtilSplitQName(bstrTemp, 0, &m_bstrBinding));


    // now get the location of the soap:address element

    hr = pPortNode->selectSingleNode(_T("./soap:address"), &pLocation);
    if (FAILED(hr) || pLocation==0)
    {
        // this is probably NOT a soap binding,
        hr = S_FALSE;
        goto Cleanup;
    }

    hr = _WSDLUtilFindAttribute(pLocation, _T("location"), &m_bstrLocation);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_NOLOCATIONATTR, WSDL_IDS_PORT,hr, m_bstrName);
        goto Cleanup;
    }


    // now find the bindings
    hr = _XPATHUtilFindNodeListFromRoot(pPortNode, _T("//def:binding"), &pNodeList);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_BINDINGSNOTFOUND, WSDL_IDS_PORT,hr, m_bstrName);
        goto Cleanup;
    }

    while (((hr = pNodeList->nextNode(&pBinding))==S_OK) && pBinding != 0)
    {
        // now check the match of the binding names
        bstrTemp.Clear();
        hr = _WSDLUtilFindAttribute(pBinding, _T("name"), &bstrTemp);
        if (FAILED(hr))
        {
            globalAddError(WSDL_IDS_BINDINGSNONAME, WSDL_IDS_PORT,hr, m_bstrName);
            goto Cleanup;
        }

        if (wcscmp(bstrTemp, m_bstrBinding)==0)
        {
            // found the right one
            hr = AnalyzeBinding(pBinding, ptypeFactory);
            if (FAILED(hr))
            {
                globalAddError(WSDL_IDS_ANALYZEBINDINGFAILED, WSDL_IDS_PORT,hr, m_bstrName);
            }
            // exit anyway
            goto Cleanup;
        }
        release(&pBinding);
    }

    // if we arrive here, we did not find the right guy...
    hr = E_FAIL;
    globalAddError(WSDL_IDS_BINDINGSNOTFOUND, WSDL_IDS_PORT,hr, m_bstrName);

Cleanup:
    ASSERT(SUCCEEDED(hr));
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLPort::AnalyzeBinding(IXMLDOMNode *pBinding, ISoapTypeMapperFactory *ptypeFactory)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLPort::AnalyzeBinding(IXMLDOMNode *pBinding, ISoapTypeMapperFactory *ptypeFactory)
{
    HRESULT                     hr = E_FAIL;
    CAutoBSTR                   bstrEncoding;
    CAutoRefc<IXMLDOMNode>         pSoapBinding=0;
    CAutoRefc<IXMLDOMNode>         pOperation=0;
    CAutoRefc<IXMLDOMNode>         pEncodingNode=0;
    CAutoRefc<IXMLDOMNodeList>    pNodeList=0;
    CWSDLOperation                *pWSDLOperation=0;
    long                        lSize=0;
#ifndef UNDER_CE
    BOOL                        fEqual;
#endif 
    BOOL                        bCreateHrefs=false;
    CAutoBSTR                   bstrTemp;


    ASSERT(pBinding!=0);

    // now we need to get the binding information
    // first copy the string over, so that we don't have to parse this again
    hr = _WSDLUtilFindAttribute(pBinding, _T("type"), &bstrTemp);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_BINDINGNOTYPE, WSDL_IDS_PORT,hr, m_bstrName);
        goto Cleanup;
    }
    // remove the optional specifier from the name and copy over
    // now find the start of the binding name
    CHK(_WSDLUtilSplitQName(bstrTemp, 0, &m_bstrPortType));

    hr = pBinding->selectSingleNode(_T("./soap:binding"), &pSoapBinding);
    if (hr != S_OK)
    {
        globalAddError(WSDL_IDS_PORTTYPENOBINDING, WSDL_IDS_PORT,hr, m_bstrName);
        goto Cleanup;
    }

    hr = _WSDLUtilGetStyle(pSoapBinding,&m_fDocumentMode);
    if (FAILED(hr))
    {
        // found no attribute, set the default to document mode
        m_fDocumentMode = true;
        hr = S_OK;
    }

    hr = _WSDLUtilFindAttribute(pSoapBinding, _T("transport"), &m_bstrBindTransport);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_PORTNOTRANSPORT, WSDL_IDS_PORT,hr, m_bstrName);
        goto Cleanup;
    }

    bCreateHrefs = false;
    CHK(_WSDLUtilFindExtensionInfo(pBinding, &bstrEncoding, &bCreateHrefs));

    // now create the operations....


    hr = pBinding->selectNodes(_T("./def:operation"), &pNodeList);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_PORTNOOPERATIONS, WSDL_IDS_PORT,hr, m_bstrName);
        goto Cleanup;
    }

    // create the enum to hold the operations
    m_pOperationsEnum = new CSoapObject<CEnumWSDLOperations>(INITIAL_REFERENCE);

#ifdef UNDER_CE
    if(!m_pOperationsEnum)
    {
       hr = E_OUTOFMEMORY;
       goto Cleanup;
    } 
#endif

    while (((hr = pNodeList->nextNode(&pOperation))==S_OK) && pOperation!=0)
    {
        pWSDLOperation = new CSoapObject<CWSDLOperation>(INITIAL_REFERENCE);

#ifdef UNDER_CE        
        if(!pWSDLOperation)
        {
           hr = E_OUTOFMEMORY;
           goto Cleanup;
        } 
#endif

        hr = pWSDLOperation->Init(pOperation, ptypeFactory, m_bstrPortType, m_fDocumentMode, bstrEncoding, bCreateHrefs);
        if (FAILED(hr))
        {
            globalAddError(WSDL_IDS_PORTOPERATIONINITFAILED, WSDL_IDS_PORT,hr, m_bstrName);
            ((CWSDLOperation*)pWSDLOperation)->Shutdown();
            pWSDLOperation->Release();
            goto Cleanup;
        }
        release(&pOperation);
        m_pOperationsEnum->Add(pWSDLOperation);
        pWSDLOperation->Release();
    }
    if (m_pOperationsEnum->Size(&lSize)==S_OK && lSize>0)
    {
        hr = S_OK;
    }
    else
    {
        globalAddError(WSDL_IDS_PORTNOOPERATIONS, WSDL_IDS_PORT,hr, m_bstrName);
    }



Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLPort::AddWSMLMetaInfo(IXMLDOMNode *pServiceNode,CWSDLService *pWSDLService,
//						IXMLDOMDocument *pWSDLDom, IXMLDOMDocument *pWSMLDom, bool bLoadOnServer)
//
//  parameters:
//
//  description:
//        port has nothing to do, just pass it on....
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLPort::AddWSMLMetaInfo(IXMLDOMNode *pServiceNode,
                                    CWSDLService *pWSDLService,
                                    IXMLDOMDocument *pWSDLDom,
                                    IXMLDOMDocument *pWSMLDom,
                                    bool bLoadOnServer)
{
    HRESULT                 hr = S_OK;
    long                    lFetched;
    IWSDLOperation            *pWSDLOperation=0;
       CAutoRefc<IXMLDOMNode>     pPortNode=0;
    CAutoFormat             autoFormat;

    ASSERT(m_pOperationsEnum!=0);


    // if we are loading on the client, we don't need to do this.
    //  all we want is forward this to the mappers so that they can check the custom mappers
    // first get the subnode
    //

    if (bLoadOnServer)
    {
        CHK(autoFormat.sprintf(_T("//service[@name=\"%s\"]/port[@name=\"%s\"]"), pWSDLService->getName(), m_bstrName));
        hr = pServiceNode->selectSingleNode(&autoFormat, &pPortNode);
        if (hr != S_OK)
        {
            globalAddError(WSML_IDS_NOPORTTYPE, WSDL_IDS_PORT, hr, m_bstrName);
            hr = E_FAIL;
            goto Cleanup;
        }
    }


    m_pOperationsEnum->Reset();

    while(m_pOperationsEnum->Next(1, &pWSDLOperation, &lFetched)==S_OK)
    {
        hr = ((CWSDLOperation*)pWSDLOperation)->AddWSMLMetaInfo(pPortNode,
                                                pWSDLService,
                                                pWSDLDom,
                                                pWSMLDom,
                                                bLoadOnServer);
        if (FAILED(hr))
        {
            globalAddError(WSML_IDS_OPERATIONINITFAILED, WSDL_IDS_PORT, hr, m_bstrName);
            goto Cleanup;
        }
        release(&pWSDLOperation);
    }
Cleanup:
    release(&pWSDLOperation);
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
