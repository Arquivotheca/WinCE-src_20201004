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
// File:    wsdlserv.cpp
// 
// Contents:
//
//  implementation file 
//
//        IWSDLReader Interface implemenation
//    
//    Created 
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "wsdlserv.h"
#include "wsdlport.h"
#include "ensoappt.h"


#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


BEGIN_INTERFACE_MAP(CWSDLService)
    ADD_IUNKNOWN(CWSDLService, IWSDLService)
    ADD_INTERFACE(CWSDLService, IWSDLService)
END_INTERFACE_MAP(CWSDLService)



BEGIN_INTERFACE_MAP(CDispatchHolder)
    ADD_IUNKNOWN(CDispatchHolder, IUnknown)
END_INTERFACE_MAP(CDispatchHolder)

/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLService::CWSDLService()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLService::CWSDLService()
{
    m_pPortsList = 0; 
    m_cbDispatchHolders=0;
    m_pDispatchHolders=0;
    m_bCustomMappersCreated = false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLService::~CWSDLService()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLService::~CWSDLService()
{

    TRACEL((2, "Service shutdown %S\n",  m_bstrName));
    for (int i=0; m_pDispatchHolders && i < m_cbDispatchHolders;i++ )
    {
        if (m_pDispatchHolders[i])
            m_pDispatchHolders[i]->Release();
    }
    delete [] m_pDispatchHolders; 
    release(&m_pPortsList);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLService::get_name(BSTR *bstrServiceName)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLService::get_name(BSTR *bstrServiceName)
{
    return _WSDLUtilReturnAutomationBSTR(bstrServiceName, m_bstrName);    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLService::get_documentation(BSTR *pbstrServicedocumentation)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLService::get_documentation(BSTR *pbstrServicedocumentation)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrServicedocumentation, m_bstrDocumentation);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLService::GetSoapPorts(IEnumWSDLPorts **ppIWSDLPorts)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLService::GetSoapPorts(IEnumWSDLPorts **ppIWSDLPorts)
{
    HRESULT hr = S_OK;;

    ASSERT(m_pPortsList!=0);
    
    if (ppIWSDLPorts==0)
        return(E_INVALIDARG);
    
    hr = m_pPortsList->Clone(ppIWSDLPorts); 
        
    ASSERT(hr==S_OK);
    
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLService::Init(IXMLDOMNode *pServiceNode, ISoapTypeMapperFactory *ptypeFactory)
//
//  parameters:
//
//  description:
//        walks the service node, get's the information it needs and creates holding
//        objects for the subparts
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLService::Init(IXMLDOMNode *pServiceNode, ISoapTypeMapperFactory *ptypeFactory)
{
    HRESULT hr = E_FAIL;
    IXMLDOMNode             *pPort = 0; 
    CAutoRefc<IXMLDOMNodeList>    pNodeList=0;
    CWSDLPort                    *pPortDesc=0;
    long                        lFetched;

    // first we need to get the attributes collection and find the "name" of this

    hr = _WSDLUtilFindAttribute(pServiceNode, _T("name"), &m_bstrName);
    if (FAILED(hr)) 
    {
        globalAddError(WSDL_IDS_SERVICENONAME, WSDL_IDS_SERVICE,  hr);                
        goto Cleanup;
    }

    // hold on to the typefactory for later
    assign(&m_pTypeFactory, ptypeFactory);

    TRACEL((2, "Service Init %S\n",  m_bstrName));
    // find the documentation subnode and get text if exists

    CHK(_WSDLUtilFindDocumentation(pServiceNode, &m_bstrDocumentation));
    
    // now find the ports subnode and get the portobject
    hr = pServiceNode->selectNodes(_T("def:port"), &pNodeList);

    if (FAILED(hr)) 
    {
        globalAddError(WSDL_IDS_SERVICENOPORTDEF, WSDL_IDS_SERVICE,  hr, m_bstrName);                
        goto Cleanup;
    }

    m_pPortsList = new CSoapObject<CEnumWSDLPorts>(INITIAL_REFERENCE);
    if (!m_pPortsList)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }
    
    // iterate over all services and get the guys...

    while (((hr = pNodeList->nextNode(&pPort))==S_OK) && pPort != 0)
    {
        pPortDesc = new CSoapObject<CWSDLPort>(1);
        if (!pPortDesc)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }
        hr = pPortDesc->Init(pPort, ptypeFactory);
        if (FAILED(hr)) 
        {
            globalAddError(WSDL_IDS_SERVICEPORTINITFAILED, WSDL_IDS_SERVICE,hr, m_bstrName);                        
            release(&pPortDesc);
            goto Cleanup;
        }
        if (hr == S_OK)
        {
            // on S_FALSE, not a SOAP_PORT, ignore the guy
            hr = m_pPortsList->Add(pPortDesc);
            if (FAILED(hr)) 
            {
                goto Cleanup;
            }
        }
        release(&pPort);
        release(&pPortDesc);
    }

    // if we got here and there is anything in the list, everything is fine
    hr = m_pPortsList->Size(&lFetched);
    if (SUCCEEDED(hr) && lFetched > 0)
    {
        hr = S_OK;
    }
    else
    {
        globalAddError(WSDL_IDS_SERVICENOPORTSDEFINED, WSDL_IDS_SERVICE,hr, m_bstrName);                            
        hr = E_INVALIDARG;
    }

Cleanup:
    release(&pPortDesc);
    release(&pPort);
    ASSERT(hr==S_OK);
    release(&pPort);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLService::AddWSMLMetaInfo(IXMLDOMNode *pServiceNode, IXMLDOMDocument *pWSDLDom, 
//                                                  IXMLDOMDocument *pWSMLDom,  bool bLoadOnServer)
//
//  parameters:
//
//  description:
//        does not have to do anything so far, beside:
//            a) check if this is a match
//            b) call subsequent objects
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLService::AddWSMLMetaInfo(IXMLDOMNode *pServiceNode, 
                                      IXMLDOMDocument *pWSDLDom, 
                                      IXMLDOMDocument *pWSMLDom, 
                                      bool bLoadOnServer)
{
    HRESULT hr = E_FAIL;

    CAutoRefc<IXMLDOMNode>         pUsing=0; 
    CAutoRefc<IXMLDOMNodeList>    pNodeList=0; 
    CAutoBSTR                   bstrTemp;
    long                        cb;
#ifndef UNDER_CE
    BOOL                        bCachable; 
#endif 
    CWSDLPort                    *pPortDesc=0;
    long                        lFetched;

    hr = _WSDLUtilFindAttribute(pServiceNode, _T("name"), &bstrTemp);
    if (FAILED(hr)) 
    {
        globalAddError(WSML_IDS_SERVICENONAME, WSDL_IDS_SERVICE, hr);            
        goto Cleanup;
    }

    if (wcscmp(bstrTemp, m_bstrName)==0)
    {

        // see if there is an headerHandler attribute, ignore failure, as there does not need to be one
        _WSDLUtilFindAttribute(pServiceNode, g_pwstrHeaderHandlerAttribute, &m_bstrHeaderHandler);
        
        hr = pServiceNode->selectNodes(_T("./using"), &pNodeList);
        if (FAILED(hr)) 
        {
            globalAddError(WSML_IDS_SERVICENOUSING, WSDL_IDS_SERVICE, hr, m_bstrName);                
            goto Cleanup;
        }
    
        // create the list to hold the objects
        hr = pNodeList->get_length(&m_cbDispatchHolders);
        if (FAILED(hr)) 
        {
            goto Cleanup;
        }
        
        if (m_cbDispatchHolders > 0)
        {
            // allocated the pointers
            m_pDispatchHolders = (CDispatchHolder**)new BYTE[sizeof(CDispatchHolder*) * m_cbDispatchHolders];
            if (!m_pDispatchHolders)
            {
                hr = E_OUTOFMEMORY;
                goto Cleanup;
            }
            ZeroMemory(m_pDispatchHolders, sizeof(CDispatchHolder*) * m_cbDispatchHolders);

            cb = 0; 
            while (((hr = pNodeList->nextNode(&pUsing))==S_OK) && pUsing!=0)
            {
                bstrTemp.Clear();
                m_pDispatchHolders[cb] = new CSoapObject<CDispatchHolder>(INITIAL_REFERENCE); 
                
#ifdef UNDER_CE
                CHK_BOOL(m_pDispatchHolders[cb], E_OUTOFMEMORY);
#endif
                
                // parse the using pointer and get the values stored.
                // we need the PROGID 
                hr = _WSDLUtilFindAttribute(pUsing, _T("cachable"), &bstrTemp);
                if (SUCCEEDED(hr)) 
                {
                    m_pDispatchHolders[cb]->m_bCachable = (BOOL) _wtoi(bstrTemp);
                }

                hr = _WSDLUtilFindAttribute(pUsing, _T("PROGID"),  &(m_pDispatchHolders[cb]->m_bstrProgID));
                if (FAILED(hr)) 
                {
                    globalAddError(WSML_IDS_SERVICENOPROGID, WSDL_IDS_SERVICE, hr, m_bstrName);        
                    goto Cleanup;
                }
                hr = _WSDLUtilFindAttribute(pUsing, _T("ID"), &(m_pDispatchHolders[cb]->m_bstrID));
                if (FAILED(hr)) 
                {
                    globalAddError(WSML_IDS_SERVICENOID, WSDL_IDS_SERVICE, hr, m_bstrName);                        
                    goto Cleanup;
                }
                CHK(m_pDispatchHolders[cb]->Init());
                release(&pUsing);
                cb++;
            }
        }

        // now register ALL custom mappers, if ANY
        CHK(registerCustomMappers(pServiceNode));

        // now forward this to ALL ports...
        m_pPortsList->Reset();
        while(m_pPortsList->Next(1, (IWSDLPort**)&pPortDesc, &lFetched)==S_OK)
        {
            hr = pPortDesc->AddWSMLMetaInfo(pServiceNode, this,pWSDLDom,pWSMLDom, bLoadOnServer);
            if (FAILED(hr)) 
            {
                globalAddError(WSML_IDS_PORTINITFAILED, WSDL_IDS_SERVICE, hr, m_bstrName);                    
                goto Cleanup;
            }
            release(&pPortDesc);
        }

    }
    else
    {
        hr = E_INVALIDARG;
        goto Cleanup;
    }


Cleanup:
    release(&pPortDesc);
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDispatchHolder *CWSDLService::GetDispatchHolder(TCHAR *pchID)
//
//  parameters:
//
//  description:
//        finds the correct dispatch holder
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CDispatchHolder *CWSDLService::GetDispatchHolder(TCHAR *pchID)
{
    for (int i=0;i<m_cbDispatchHolders ;i++ )
    {
        if (wcscmp(pchID, m_pDispatchHolders[i]->m_bstrID)==0)
        {
            return(m_pDispatchHolders[i]);
            
        }
    }
    return(0);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDispatchHolder * CWSDLService::GetHeaderHandler(TCHAR *pchHeaderHandlerID)
//
//  parameters:
//      pchHeaderHandlerID -> can be 0, if so, servicelevel attribute is used
//  description:
//        finds the correct dispatch holder for a headerhandler
//          
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CDispatchHolder * CWSDLService::GetHeaderHandler(TCHAR *pchHeaderHandlerID)
{
    if (pchHeaderHandlerID== 0)
    {
        pchHeaderHandlerID = m_bstrHeaderHandler;
    }            
    if (pchHeaderHandlerID)
    {
        return GetDispatchHolder(pchHeaderHandlerID);
    }
    return (0);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLService::registerCustomMappers(IXMLDOMNode *pServiceNode)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLService::registerCustomMappers(IXMLDOMNode *pServiceNode)
{
    HRESULT hr; 
    CAutoRefc<IXMLDOMNodeList> pNodeList;
    CAutoRefc<IXMLDOMNode>   pNode; 
    CAutoBSTR                 bstrName;
    CAutoBSTR                 bstrNS;
    CAutoBSTR                 bstrUses;
    CAutoBSTR                 bstrNodeName;
    CAutoBSTR                 bstrIID; 
    CDispatchHolder             *pDispatchHolder;
    BOOL                      fElement;
    

    hr = _XPATHUtilFindNodeList(pServiceNode, _T(".//types/*"), &pNodeList);
    if (SUCCEEDED(hr))
    {
        // there are some type declarations
        while (((hr = pNodeList->nextNode(&pNode))==S_OK) && pNode!=0)
        {
            CHK(pNode->get_nodeName(&bstrNodeName));
            CHK(_WSDLUtilFindAttribute(pNode, _T("name"), &bstrName));
            CHK(_WSDLUtilFindAttribute(pNode, _T("targetNamespace"), &bstrNS));
            CHK(_WSDLUtilFindAttribute(pNode, _T("uses"), &bstrUses));

            // iid is optional, so ignore a failure for that one. 
            if (SUCCEEDED(_WSDLUtilFindAttribute(pNode, _T("iid"), &bstrIID)))
            {
                // let's try converting, so that we can fail if invalid object
                CLSID iid; 
                if (FAILED(CLSIDFromString(bstrIID, &iid)))
                {
                    globalAddError(WSML_IDS_CUSTOMMAPPERNOINTERFACE, WSDL_IDS_MAPPER, hr, bstrName, bstrNS);                
                    hr = E_INVALIDARG;
                    goto Cleanup;
                }
            }

            // if we have everything, we have a valid entry, assuming nodename is type/element

            if (_tcscmp(bstrNodeName, _T("element"))==0)
            {
                fElement = true;
            }
            else if (_tcscmp(bstrNodeName, _T("type"))==0)
            {
                fElement = false;
            }
            else
            {
                CHK(E_INVALIDARG);
            }
            // the dispatchholder holds the real progid
            pDispatchHolder = GetDispatchHolder(bstrUses);
            if (!pDispatchHolder)
            {
                globalAddError(WSML_IDS_CUSTOMMAPPERNOOBJECT, WSDL_IDS_MAPPER, hr, bstrUses);        
                hr = E_INVALIDARG;
                goto Cleanup;
            }
        
            if (fElement)
            {
                hr = m_pTypeFactory->addElementObjectMapper(bstrName, bstrNS, pDispatchHolder->getProgID(), bstrIID);                 
            }
            else
            {
                hr = m_pTypeFactory->addTypeObjectMapper(bstrName, bstrNS, pDispatchHolder->getProgID(), bstrIID);
            }
            if (FAILED(hr))
            {
                globalAddError(WSML_IDS_CUSTOMMAPPERNOINTERFACE, WSDL_IDS_MAPPER, hr, bstrName, bstrNS);                
                goto Cleanup;
            }
            

            pNode.Clear();
            bstrUses.Clear();
            bstrNS.Clear();
            bstrName.Clear();
            bstrNodeName.Clear();
            // remember that we created one
            m_bCustomMappersCreated = true;
        }
    }

    // if finding nodes failed means no custom mapper entries
    hr = S_OK;

Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDispatchHolder::GetDispatchPointer(IDispatch **ppDispatch)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDispatchHolder::GetDispatchPointer(IDispatch **ppDispatch)
{
    if (m_bCachable && m_gipDispatch.IsOK())
    {
        return m_gipDispatch.Localize(ppDispatch);
    }
    else
    {
        return CoCreateInstance(m_clsid, 0, CLSCTX_ALL, IID_IDispatch, (void**)ppDispatch);    
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDispatchHolder::GetHeaderPointer(IHeaderHandler **ppHeader)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDispatchHolder::GetHeaderPointer(IHeaderHandler **ppHeader)
{
    HRESULT hr;
    CAutoRefc<IDispatch> pDispatch;

    CHK(GetDispatchPointer(&pDispatch));
    CHK(pDispatch->QueryInterface(IID_IHeaderHandler, (void**)ppHeader));
    

Cleanup:
    return hr;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDispatchHolder::GetProgID(BSTR *pbstrobjectProgID)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDispatchHolder::GetProgID(BSTR *pbstrobjectProgID)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrobjectProgID, m_bstrProgID);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CDispatchHolder::Init(void)
//
//  parameters:
//
//  description:
//      checks for the cachable attribute
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDispatchHolder::Init(void)
{
    HRESULT     hr = S_OK;
    LPOLESTR    p=0; 
    DWORD       dwType;
    DWORD       dwSize=_MAX_BUFFER_SIZE;
    CAutoRefc<IDispatch> pDispatch; 

#ifndef UNDER_CE
    TCHAR       achBuffer[_MAX_BUFFER_SIZE]; 
#else
    TCHAR       *achBuffer = new TCHAR[_MAX_BUFFER_SIZE]; 
    CHK_MEM(achBuffer);
#endif 

    hr = CLSIDFromProgID(m_bstrProgID, &m_clsid);
    if (FAILED(hr))
    {
        globalAddError(WSML_IDS_SERVICEINVALIDPROGID, WSDL_IDS_SERVICE, hr, m_bstrProgID);                        
        goto Cleanup;
    }
    
    if (m_bCachable)
    {
        CHkey        hKey; 
        // assume false
        m_bCachable = false; 

        // now as this is the first time, check if the component is boththreaded or freethreaded...
        // this is mainly done for VB, as the apartment model window dispatching will kill IIS
        CHK (StringFromCLSID(m_clsid, &p)); 
        wcscpy(achBuffer, _T("CLSID\\")); 
        wcscat(achBuffer, p); 
        wcscat(achBuffer, _T("\\InProcServer32")); 
        if (RegOpenKeyEx(HKEY_CLASSES_ROOT, achBuffer, 0, KEY_READ, &hKey.hkey)==ERROR_SUCCESS)
        {
            if (RegQueryValueEx(
                  hKey.hkey,            // handle to key
                  _T("ThreadingModel"),  // value name
                  0,   // reserved
                  &dwType,       // type buffer
                  (LPBYTE)achBuffer,        // data buffer
                  &dwSize      // size of data buffer
                )==ERROR_SUCCESS)
                {
                    
                 if (dwSize>0 && (wcsicmp(achBuffer, _T("Both"))==0 
                             || wcsicmp(achBuffer, _T("Free"))==0 
                             || wcsicmp(achBuffer, _T("Neutral"))==0))
                             {
                                // correct   
                                   m_bCachable = true;
                             }
                }
                            
        }
        else
        {
            // so assume an OUT of proc serve. Assume the server is able to handle
            // threading correctly
            m_bCachable = true;
        }
    }

    if (m_bCachable)
    {
        // if we are cachable, just go ahead and register the interface in the GID
        CHK(CoCreateInstance(m_clsid, 0, CLSCTX_ALL, IID_IDispatch, (void**)&pDispatch));
        CHK(m_gipDispatch.Globalize(pDispatch));
    }
    

Cleanup:
    if (p)
    {
        CoTaskMemFree(p);
    }
    
#ifdef UNDER_CE
   if(achBuffer)
        delete [] achBuffer;
#endif 
    return (hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
