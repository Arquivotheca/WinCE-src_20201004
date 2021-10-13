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
// File:    wsdlread.cpp
//
// Contents:
//
//  implementation file
//
//        IWSDLReader Interface implemenation
//
//-----------------------------------------------------------------------------
#ifndef UNDER_CE
#include "msxml2.h"
#include "WinCEUtils.h"
#endif


#include "headers.h"
#include "wsdlread.h"
#include "wsdlserv.h"
#include "wsdloper.h"
#include "enwsdlse.h"
#include "typemapr.h"
#include "xsdpars.h"


BEGIN_INTERFACE_MAP(CWSDLReader)
    ADD_IUNKNOWN(CWSDLReader, IWSDLReader)
    ADD_INTERFACE(CWSDLReader, IWSDLReader)
END_INTERFACE_MAP(CWSDLReader)

/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLReader::CWSDLReader()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLReader::CWSDLReader()
{
    m_pWSDLDom = 0;
    m_pWSMLDom = 0;
    m_pServiceList = 0;
#ifndef UNDER_CE
    m_bUseServerHTTPRequest = false;
#endif 
    m_bInitSuccessfull = false;
    m_bLoadOnServer = true;
    m_critSect.Initialize();

    TRACEL((1, "Reader constructed \n\n"));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLReader::~CWSDLReader()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLReader::~CWSDLReader()
{
    TRACEL((1, "Reader shutdown \n\n"));
    m_critSect.Delete();

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLReader::Load(WCHAR *pchWSDLFileSpec, WCHAR *pchSMLFileSpec)
//
//  parameters:
//
//  description:
//        the load method loads the passed in filespecs into xmldoms to  analyze it's content
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLReader::Load(WCHAR *pchWSDLFileSpec, WCHAR *pchSMLFileSpec)
{
    HRESULT hr = E_FAIL;
    CCritSectWrapper csw(&m_critSect);

    CHK (csw.Enter());

#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 

        if (m_bInitSuccessfull)
        {
            hr = E_UNEXPECTED;
            globalAddError(WSDL_IDS_INITCALLEDTWICE,WSDL_IDS_READER,  hr);
            goto Cleanup;

        }

        // verify locale existence


        if (IsValidLocale(LCID_TOUSE, LCID_INSTALLED)==0)
        {
            // conversion code will fail, bail out
            hr = E_FAIL;
            globalAddError(WSDL_IDS_ENGLISHNOTINSTALLED, WSDL_IDS_READER, hr);
            goto Cleanup;

        }


        hr = LoadDom(pchWSDLFileSpec, &m_pWSDLDom);
        if (FAILED(hr))
        {
            globalAddError(WSDL_IDS_LOADFAILED,WSDL_IDS_READER,  hr);
            goto Cleanup;
        }

        CHK(CoCreateInstance(CLSID_SoapTypeMapperFactory, NULL,
                              CLSCTX_INPROC_SERVER,
                              IID_ISoapTypeMapperFactory,
                              (void**)&m_ptypeFactory));

        // we need to add the schema to this guy.... pass the whole document for now

        CHK(m_ptypeFactory->addSchema(m_pWSDLDom));


        hr = AnalyzeWSDLDom();

        if (FAILED(hr))
        {
            globalAddError(WSDL_IDS_ANALYZEWSDLFAILED,WSDL_IDS_READER,  hr);
            goto Cleanup;
        }

        if (pchSMLFileSpec && wcslen(pchSMLFileSpec) > 0)
        {
            // only create a failure if a filename was given
            hr = LoadDom(pchSMLFileSpec, &m_pWSMLDom);
            if (FAILED(hr))
            {
                globalAddError(WSML_IDS_WSMLLOADFAILED, WSDL_IDS_READER, hr);
                goto Cleanup;
            }

            hr = AnalyzeWSMLDom();
            if (FAILED(hr))
            {
                globalAddError(WSML_IDS_WSMLANALYZEFAILED, WSDL_IDS_READER, hr);
                goto Cleanup;
            }

        }
#ifndef CE_NO_EXCEPTIONS
    }

    catch(...)
    {
        hr = E_UNEXPECTED;
    }
#endif 


Cleanup:
    if (hr==E_FAIL)
    {
        // change generic failures to E_INVALIDARG, as the failures is 99% sure to be caused
        // by invalid XML passed in
        hr = E_INVALIDARG;
    }
    else if (SUCCEEDED(hr))
    {
        m_bInitSuccessfull = true;
    }
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLReader::GetSoapServices(IEnumWSDLService **ppWSDLServiceEnum)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLReader::GetSoapServices(IEnumWSDLService **ppWSDLServiceEnum)
{

    HRESULT hr = S_OK;

    ASSERT(m_pServiceList!=0);

    if (ppWSDLServiceEnum==0)
        return(E_INVALIDARG);

    if (!m_pServiceList)
        return(E_FAIL);

    hr = m_pServiceList->Clone(ppWSDLServiceEnum);


    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLReader::ParseRequest(IUnknown *pSoapReader, IWSDLPort **ppIWSDLPort, IWSDLOperation **ppIWSDLOperation)
//
//  parameters:
//
//  description:
//        this method is intended as a shortcut to find a certain operation based on
//            an incoming message
//  returns:
//            the operation with all soapmappers filled out
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLReader::ParseRequest(ISoapReader *pSoapReader, IWSDLPort **ppIWSDLPort,
        IWSDLOperation **ppIWSDLOperation)
{
    HRESULT hr = E_FAIL;
    CAutoRefc<IEnumWSDLService>     pServiceList=0;
    CAutoRefc<IWSDLService>            pService=0;
    CAutoRefc<IEnumWSDLPorts>          pPortList=0;
    CAutoRefc<IWSDLPort>              pPort=0;
    CAutoRefc<IEnumWSDLOperations>     pOperationList=0;
    CAutoRefc<IWSDLOperation>        pOperation=0;
    CAutoBSTR                       bstrSoapAction;
    long                            lFetched=0;

    if (ppIWSDLOperation==0 || pSoapReader==0 || ppIWSDLPort ==0)
    {
        hr = E_INVALIDARG;
        goto Cleanup;
    }

    *ppIWSDLOperation = NULL;
    *ppIWSDLPort = NULL;

    CHK(pSoapReader->get_soapAction(&bstrSoapAction));


    // for now, we use a really bad implmentation:
    //        iterate over all services
    //        iterate over all ports
    //        iterate over all operations
    //            try to load
    //                -> if failed, continue
    //                -> if succeeded, that's the guy, compare the soapaction property

    ASSERT(m_pServiceList!=0);

    if (!m_pServiceList)
    {
        hr = E_FAIL;
        goto Cleanup;
    }

    hr = GetSoapServices(&pServiceList);

    if (FAILED(hr))
    {
        goto Cleanup;
    }


    while(pServiceList->Next(1, &pService, &lFetched)==S_OK)
    {
        hr = pService->GetSoapPorts(&pPortList);
        if (FAILED(hr))
        {
            goto Cleanup;
        }
        while (pPortList->Next(1, &pPort, &lFetched)==S_OK)
        {
            // now we have a port.
            hr = pPort->GetSoapOperations(&pOperationList);
            if (FAILED(hr))
            {
                goto Cleanup;
            }
            while (pOperationList->Next(1, &pOperation, &lFetched)==S_OK)
            {
                // first verify the soapaction
                if (((CWSDLOperation*)(IWSDLOperation*)pOperation)->compareSoapAction(bstrSoapAction))
                {
                    // that matches, try to load the guy
                    hr = pOperation->Load(pSoapReader, VARIANT_TRUE);
                    if (SUCCEEDED(hr))
                    {
                        // found the correct operation
                        *ppIWSDLOperation = pOperation;
                        (*ppIWSDLOperation)->AddRef();
                        *ppIWSDLPort = pPort;
                        pPort->AddRef();
                        goto Cleanup;
                    }
                }
                (pOperation.PvReturn())->Release();
            }
            (pPort.PvReturn())->Release();
            (pOperationList.PvReturn())->Release();
        }
        (pService.PvReturn())->Release();
        (pPortList.PvReturn())->Release();
    }
    // if we did not bail out above, we failed
    if (FAILED(hr))
    {
        // now this means that we tried loading a matching guy and he FAILED while restoring values
        globalAddError(WSDL_IDS_LOADREQUESTFAILED, WSDL_IDS_READER,  hr, bstrSoapAction);
    }
    else
    {
        hr = E_FAIL;
        globalAddError(WSDL_IDS_PARSEREQUESTFAILED, WSDL_IDS_READER,  hr, bstrSoapAction);
    }



Cleanup:
    ASSERT(hr==S_OK);
    return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////







/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLReader::LoadDom(WCHAR *pchFileSpec, IXMLDOMDocument **pDom)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLReader::LoadDom(WCHAR *pchFileSpec, IXMLDOMDocument2 **ppDom)
{
    HRESULT             hr = E_FAIL;
    HRESULT             hrError;
    IXMLDOMDocument2    *pXMLDoc = 0;
    CAutoRefc<IXMLDOMParseError> pIParseError=0;
    CAutoBSTR           bstrReason;
    long                lFilePos=0;
    long                lLinePos=0;

    VARIANT_BOOL        fSuccess;
    VARIANT                varWSDLFile;
    VARIANT                varProp;

    VariantInit(&varWSDLFile);
    VariantInit(&varProp);
    //
    // Create an XML document object
    //
#ifndef UNDER_CE
    hr = CoCreateInstance(CLSID_FreeThreadedDOMDocument30, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, (void **)&pXMLDoc);
#else
    hr = CoCreateInstance(CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, (void **)&pXMLDoc);
#endif
 
#ifndef UNDER_CE
   if (hr)
#else
   if (FAILED(hr))
#endif

    {
        goto Cleanup;
    }

    // init the variant
    VariantInit(&varWSDLFile);
    V_VT(&varWSDLFile)      =    VT_BSTR;
    V_BSTR(&varWSDLFile)       =    pchFileSpec; // write the result to the output stream

    // turn validation OFF, so that users can put a schema into the template
#ifndef UNDER_CE
    if (FAILED(hr == pXMLDoc->put_validateOnParse(false)))
#else
    if (FAILED(hr = pXMLDoc->put_validateOnParse(false)))
#endif 
    {
        goto Cleanup;
    }
#ifndef UNDER_CE
    if (FAILED(hr == pXMLDoc->put_resolveExternals(false)))
#else
    if (FAILED(hr = pXMLDoc->put_resolveExternals(false)))
#endif 
    {
        goto Cleanup;
    }

#ifndef UNDER_CE
    if (FAILED(hr == pXMLDoc->put_async(false)))    
#else
    if (FAILED(hr = pXMLDoc->put_async(false)))
#endif
    {
        goto Cleanup;
    }

#ifndef UNDER_CE
    if (m_bUseServerHTTPRequest)
    {
        V_VT(&varProp) = VT_BOOL;
        V_BOOL(&varProp) = VARIANT_TRUE;
        hr = pXMLDoc->setProperty(_T("ServerHTTPRequest"), varProp);
        if (FAILED(hr))
        {
            goto Cleanup;
        }
    }
#endif 

    hr = pXMLDoc->load(varWSDLFile, &fSuccess);
    if (hr || fSuccess == VARIANT_FALSE)
    {
        // now first get the parseerror information
        hrError = pXMLDoc->get_parseError(&pIParseError);
        if (SUCCEEDED(hrError) && pIParseError)
        {
            hrError = pIParseError->get_reason(&bstrReason);
            if (SUCCEEDED(hrError))
            {
                pIParseError->get_line(&lFilePos);
                pIParseError->get_linepos(&lLinePos);
                globalAddError(WSDL_IDS_XMLDOCUMENTLOADFAILED, WSDL_IDS_READER, hr, bstrReason, lFilePos, lLinePos);
            }
        }

        hr = E_INVALIDARG;
    }
    assign(ppDom, pXMLDoc);

Cleanup:
    release(&pXMLDoc);
    ASSERT(hr==S_OK);
    return (hr);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLReader::AnalyzeWSDLDom(void)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLReader::AnalyzeWSDLDom(void)
{
    HRESULT hr = E_FAIL;
    CAutoRefc<IXMLDOMNode> pXDN=0;
    IXMLDOMNode     *pService=0;
    CWSDLService    *pServiceDesc=0;
    CAutoRefc<IXMLDOMNodeList>    pNodeList=0;
    CAutoRefc<IXMLDOMNode>        pDefNode=0;
    CAutoRefc<IXMLDOMNode>        pSchemaNode=0;

    schemaRevisionNr             revision;


    long                        lFetched;
    CAutoBSTR                   bstrTemp;
    CAutoBSTR                   bstrURI;

    ASSERT(m_pWSDLDom!=0);

    hr = m_pWSDLDom->QueryInterface(IID_IXMLDOMNode, (void **)&pXDN); // Check the return value.
    if (FAILED(hr))
    {
        goto Cleanup;
    }


    // get the schema namespace
    // we do this by first checking for a schema in the new namespace, then for a schema in the old namespace

    if (FAILED(_XSDFindRevision(m_pWSDLDom, &revision)))
    {
        hr = E_INVALIDARG;
        globalAddError(WSDL_IDS_INVALIDSCHEMA,WSDL_IDS_READER,  hr);
        goto Cleanup;

    }
    CHK(_XSDSetupDefaultXPath(pXDN, revision));


    // now find the bindings
    hr = _XPATHUtilFindNodeListFromRoot(pXDN, _T("/def:definitions/def:service"), &pNodeList);
    if (FAILED(hr))
    {
        goto Cleanup;
    }

    m_pServiceList = new CSoapObject<CEnumWSDLService>(INITIAL_REFERENCE);
    if (!m_pServiceList)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }


    // iterate over all services and get the guys...
    while (((hr = pNodeList->nextNode(&pService))==S_OK) && pService != 0)
    {
        pServiceDesc = new CSoapObject<CWSDLService>(1);
        if (!pServiceDesc)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }
        hr = pServiceDesc->Init(pService, m_ptypeFactory);
        if (FAILED(hr))
        {
            globalAddError(WSDL_IDS_INITSERVICEFAILED,WSDL_IDS_READER,  hr);
            release(&pServiceDesc);
            goto Cleanup;
        }
        hr = m_pServiceList->Add(pServiceDesc);
        if (FAILED(hr))
        {
            goto Cleanup;
        }
        release(&pService);
        release(&pServiceDesc);
    }
    // if we got here and there is anything in the list, everything is fine
    hr = m_pServiceList->Size(&lFetched);
    if (SUCCEEDED(hr) && lFetched > 0)
    {
        hr = S_OK;
    }
    else
    {
        hr = E_INVALIDARG;
        globalAddError(WSDL_IDS_NOSERVICE,WSDL_IDS_READER,  hr);
    }




Cleanup:
    release(&pServiceDesc);
    release(&pService);
    ASSERT(hr==S_OK);

    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLReader::AnalyzeWSMLDom(void)
//
//  parameters:
//
//  description:
//        takes the current service and adds the WSML metainformation to those objects
//    returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLReader::AnalyzeWSMLDom(void)
{
    HRESULT hr = E_FAIL;
    CAutoRefc<IXMLDOMNode>     pXDN=0;
    CAutoRefc<IXMLDOMNode>     pService=0;
    CWSDLService            *pServiceDesc=0;
    BSTR                    bstrName=0;
    long                    lFetched;
    CAutoFormat             autoFormat;

    ASSERT(m_pWSMLDom!=0);

    hr = m_pWSMLDom->QueryInterface(IID_IXMLDOMNode, (void **)&pXDN);
    if (FAILED(hr))
    {
        goto Cleanup;
    }


    m_pServiceList->Reset();
    while(m_pServiceList->Next(1, (IWSDLService**)&pServiceDesc, &lFetched)==S_OK)
    {
        hr = pServiceDesc->get_name(&bstrName);
        if (FAILED(hr))
        {
            goto Cleanup;
        }
        CHK(autoFormat.sprintf(_T("//service[@name=\"%s\"]"), bstrName));
        hr = pXDN->selectSingleNode(&autoFormat, &pService);
        if (FAILED(hr) || pService==0)
        {
            globalAddError(WSML_IDS_SERVICENOTFOUND, WSDL_IDS_READER, hr, bstrName);
            hr = E_FAIL;
            goto Cleanup;
        }
        hr = pServiceDesc->AddWSMLMetaInfo(pService, m_pWSDLDom, m_pWSMLDom, m_bLoadOnServer);
        if (FAILED(hr))
        {
            globalAddError(WSML_IDS_INITSERVICEFAILED, WSDL_IDS_READER, hr, bstrName);
            goto Cleanup;
        }

        free_bstr(bstrName);
        release(&pServiceDesc);
        release(&pService);
    }




Cleanup:
    free_bstr(bstrName);
    release(&pServiceDesc);
    release(&pService);
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  HRESULT CWSDLReader::setProperty (BSTR bstrPropname, VARIANT varPropValue)
//
//  parameters:
//      bstrPropName -> name of property to set
//      varPropValue -> propertyvalue
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLReader::setProperty (BSTR bstrPropname, VARIANT varPropValue)
{
    HRESULT hr = E_INVALIDARG;

    // we currently implement only one property
    if (!bstrPropname)
    {
        goto Cleanup;
    }

#ifndef UNDER_CE
    if (wcscmp(bstrPropname, _T("ServerHTTPRequest"))==0)
    {
        if (V_VT(&varPropValue)==VT_BOOL)
        {
            m_bUseServerHTTPRequest = V_BOOL(&varPropValue)==VARIANT_TRUE ? true : false;
            hr = S_OK;
        }
    }
#endif

    if (wcscmp(bstrPropname, _T("LoadOnServer"))==0)
    {
        if (V_VT(&varPropValue)==VT_BOOL)
        {
            m_bLoadOnServer = V_BOOL(&varPropValue)==VARIANT_TRUE ? true : false;
            hr = S_OK;
        }
    }

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






