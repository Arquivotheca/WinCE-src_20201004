//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      client.cpp
//
// Contents:
//
//      CSoapClient class implementation
//
//----------------------------------------------------------------------------------

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


#include "soaphdr.h"
#include "wsdlutil.h"
#include "ensoapmp.h"
#include "soapmapr.h"
#include "wsdloper.h"


TYPEINFOIDS(ISOAPClient, MSSOAPLib)

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface maps
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_INTERFACE_MAP(CSoapClient)
    ADD_IUNKNOWN(CSoapClient, ISOAPClient)
    ADD_INTERFACE(CSoapClient, ISOAPClient)
    ADD_INTERFACE(CSoapClient, IDispatch)
    ADD_INTERFACE(CSoapClient, ISupportErrorInfo)
END_INTERFACE_MAP(CSoapClient)

BEGIN_INTERFACE_MAP(CClientThreadStore)
    ADD_IUNKNOWN(CClientThreadStore, IUnknown)
END_INTERFACE_MAP(CClientThreadStore)
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CClientFaultInfo::SetFields(BSTR bstrfaultcode, BSTR bstrfaultstring, BSTR bstrfaultactor, BSTR bstrdetail)
//
//  parameters:
//
//  description:
//        Stores the BSTRs into the given fields
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientFaultInfo::SetFields(BSTR bstrfaultcode, BSTR bstrfaultstring, BSTR bstrfaultactor, BSTR bstrdetail)
{
    ASSERT(!HasFaultInfo());

    m_bstrfaultcode.Assign(bstrfaultcode, FALSE);
    m_bstrfaultstring.Assign(bstrfaultstring, FALSE);
    m_bstrfaultactor.Assign(bstrfaultactor, FALSE);
    m_bstrdetail.Assign(bstrdetail, FALSE);
    m_dwFaultCodeId = 0;
    m_bhasFault = TRUE;;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CClientFaultInfo::PublishErrorInfo(void)
//
//  parameters:
//
//  description:
//        Publishes COM error object
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CClientFaultInfo::PublishErrorInfo(void)
{
    ::SetErrorInfo(0, m_pErrorInfo);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CClientThreadStore::RetrieveConnector(ISoapConnector **pConnector)
//
//  parameters:
//
//  description:
//        Retrieves connector from the client's store
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CClientThreadStore::RetrieveConnector(ISoapConnector **pConnector)
{
    CHK_ARG(pConnector && ! *pConnector);

    HRESULT hr = S_OK;
    CComPointer<ISoapConnector> pConn;

    CHK(m_ConnectorSet.RemoveHead(pConn));

    pConn.AddRef();
    *pConnector = pConn;
    pConn       = 0;
    hr          = S_OK;

Cleanup:
    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CClientThreadStore::ReturnConnector(ISoapConnector *pConnector)
//
//  parameters:
//
//  description:
//        Returns connector back to the client's store
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CClientThreadStore::ReturnConnector(ISoapConnector *pConnector)
{
    CHK_ARG(pConnector);

    HRESULT hr = S_OK;
    CComPointer<ISoapConnector> pConn(pConnector);
    CHK(m_ConnectorSet.InsertTail(pConn));

    hr = S_OK;

Cleanup:
    return hr;
}


/////////////////////////////////////////////////////////////////////////////
// CSoapClient
/////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CSoapClient::mssoapinit(BSTR bstrWSDLFile, BSTR bstrServiceName, BSTR bstrPort,
//                                                 BSTR bstrWSMLFile)
//
//  parameters:
//
//  description:
//        Initializes soap client
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapClient::mssoapinit(
                /* [in] */ BSTR bstrWSDLFile,
                /* [in, defaultvalue("")] */ BSTR bstrServiceName,
                /* [in, defaultvalue("")] */ BSTR bstrPort,
                /* [in, defaultvalue("")] */ BSTR bstrWSMLFile)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr = S_OK;
        CAutoRefc<IEnumWSDLService> pIEnumWSDLServices;
        CAutoRefc<IEnumWSDLPorts> pIEnumWSDLPorts;
        CAutoRefc<IWSDLOperation> pIOperation;
        long cFetched;
        long lIndex;
        CClientFaultInfo *pFaultInfo;
        CCritSect           critSect;
        CCritSectWrapper    csw;

        critSect.Initialize();

        CHK (csw.init(&critSect, TRUE));
        CHK (csw.Enter());

        globalResetErrors();

        if (m_bflInitSuccess)
            CHK (HRESULT_FROM_WIN32(ERROR_ALREADY_ASSIGNED));

        // WSDL file name is required
        if (IsBadReadPtr(bstrWSDLFile, sizeof(BSTR)))
            CHK (E_INVALIDARG);

        if (bstrServiceName && IsBadReadPtr(bstrServiceName, sizeof(BSTR)))
           CHK (E_INVALIDARG);

        if (bstrPort && IsBadReadPtr(bstrPort, sizeof(BSTR)))
            CHK (E_INVALIDARG);

        if (bstrWSMLFile && IsBadReadPtr(bstrWSMLFile, sizeof(BSTR)))
            CHK (E_INVALIDARG);

        // Instantiate IWSDLReader object and get an interface ptr back.
        CHK ( CoCreateInstance(CLSID_WSDLReader, NULL,
                              CLSCTX_INPROC_SERVER,
                              IID_IWSDLReader,
                              (void**)&m_pIWSDLReader) );

        // set property if exist        
#ifndef UNDER_CE
        if (m_bUseServerHTTPRequest)
        {
            VARIANT varPropValue;
            V_VT(&varPropValue) = VT_BOOL;
            V_BOOL(&varPropValue) = VARIANT_TRUE;
            CHK (m_pIWSDLReader->setProperty(_T("ServerHTTPRequest"), varPropValue));
        }
#endif 

        // tell the reader that we are loading client side information
        VARIANT varPropValue;
        V_VT(&varPropValue) = VT_BOOL;
        V_BOOL(&varPropValue) = VARIANT_FALSE;
        CHK(m_pIWSDLReader->setProperty(_T("LoadOnServer"), varPropValue));
        CHK(m_pIWSDLReader->Load(bstrWSDLFile, bstrWSMLFile));
        CHK(m_pIWSDLReader->GetSoapServices(&pIEnumWSDLServices));

        // If no servicename provided, use the first one
        if (bstrServiceName && *bstrServiceName)
        {
            hr = pIEnumWSDLServices->Find(bstrServiceName, &m_pIWSDLService);
            if (FAILED(hr))
            {
                globalAddError(SOAP_IDS_COULDNOTFINDSERVICE, SOAP_IDS_CLIENT,  hr, bstrServiceName);
                goto Cleanup;
            }
        }

        else
        {
            // If no servicename provided, use the first one
            hr = pIEnumWSDLServices->Next(1, &m_pIWSDLService, &cFetched);
            if (FAILED(hr) || cFetched != 1)          // Could not find it?
            {
                globalAddError(SOAP_IDS_NODEFAULTSERVICE, SOAP_IDS_CLIENT,  hr);
                hr = E_INVALIDARG;
                goto Cleanup;
            }
        }

        CHK(m_pIWSDLService->GetSoapPorts(&pIEnumWSDLPorts));

        // Find the requested port
        if (bstrPort && *bstrPort)
        {
            hr = pIEnumWSDLPorts->Find(bstrPort, &m_pIWSDLPort);
            if (FAILED(hr))
            {
                globalAddError(SOAP_IDS_COULDNOTFINDPORT, SOAP_IDS_CLIENT,  hr, bstrPort);
                goto Cleanup;
            }
        }
        else
        {
            // If no port provided, use the first one
            hr = pIEnumWSDLPorts->Next(1, &m_pIWSDLPort, &cFetched);
            if (FAILED(hr) || cFetched != 1)          // Could not find it?
            {
                globalAddError(SOAP_IDS_NODEFAULTPORT, SOAP_IDS_CLIENT,  hr);
                hr = E_INVALIDARG;
                goto Cleanup;
            }
        }

        // Find the operations
        CHK(m_pIWSDLPort->GetSoapOperations(&m_pIEnumWSDLOps));
        // All is fine if we make it here.

        CHK(m_pIEnumWSDLOps->Reset() );
        CHK(m_pIEnumWSDLOps->Size(&lIndex));
        CHK(m_abstrNames.SetSize(lIndex));

        lIndex = 0;

        while (((m_pIEnumWSDLOps->Next(1, &pIOperation, &cFetched)==S_OK) && pIOperation != 0))
        {
            WCHAR * name = NULL;

            // let's prefill the encoding here as well
            if (lIndex == 0)
            {
                m_bstrpreferredEncoding.Clear();
                hr = pIOperation->get_preferredEncoding(&m_bstrpreferredEncoding);
                if (FAILED(hr))
                {
                    goto Cleanup;
                }
            }


            CHK (allocateAndCopy (&name, pIOperation->getNameRef() ) );
            pIOperation.Clear();

            if (name)
            {
                //add name to the list of names, ownership is transfered
                m_abstrNames.AddName(name);
            }

            lIndex++;
        }

        // we added all names at this point, now we can go ahead and sort them
        m_abstrNames.Sort();

        // Instantiate a SoapConnector object, initialize it
        CHK(CoCreateInstance(CLSID_SoapConnectorFactory, NULL,
                              CLSCTX_INPROC_SERVER,
                              IID_ISoapConnectorFactory,
                              (void**)&m_pISoapConnFact));


        m_bflInitSuccess = TRUE;


    Cleanup:
        ASSERT (hr == S_OK);
        if (FAILED(hr))
        {
            // Do Reset first so it won't clear the error information
            Reset();
            if (SUCCEEDED(RetrieveFaultInfo(&pFaultInfo)))
            {
                pFaultInfo->FaultMsgFromResourceHr(SOAP_IDS_CLIENT, 0, hr, NULL, NULL);
            }
        }
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }

    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapClient::mssoapinit - Unhandled Exception");
        return E_FAIL;
    }
#endif 

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::get_faultcode(BSTR * pbstrFaultcode)
//
//  parameters:
//
//  description:
//        Returns the faultcode value from SOAP fault message
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::get_faultcode(BSTR * pbstrFaultcode)
{
#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif 
      {
        HRESULT hr = S_OK;
        CClientFaultInfo *pFaultInfo;

        if (IsBadWritePtr(pbstrFaultcode, sizeof(BSTR *)))
            return E_INVALIDARG;

        *pbstrFaultcode = NULL;

        hr = RetrieveFaultInfo(&pFaultInfo);
        if (SUCCEEDED(hr))
        {
            if (pFaultInfo->HasFaultInfo())
            {
                *pbstrFaultcode = SysAllocString(pFaultInfo->getfaultcode());
                if (!(*pbstrFaultcode))
                    hr = E_OUTOFMEMORY;
            }
        }

        return hr;
    }
#ifndef CE_NO_EXCEPTIONS    
    catch (...)
#else
    __except(1)
#endif
    {
        ASSERTTEXT (FALSE, "CSoapClient::get_faultcode - Unhandled Exception");
        return E_FAIL;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::get_faultstring(BSTR * pbstrFaultstring)
//
//  parameters:
//
//  description:
//        Returns the faultstring value from SOAP fault message
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::get_faultstring(
                /* [out, retval] */ BSTR * pbstrFaultstring)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr = S_OK;
        CClientFaultInfo *pFaultInfo;

        if (IsBadWritePtr(pbstrFaultstring, sizeof(BSTR *)))
            return E_INVALIDARG;
        *pbstrFaultstring = NULL;

        hr = RetrieveFaultInfo(&pFaultInfo);
        if (SUCCEEDED(hr))
        {
            if (pFaultInfo->HasFaultInfo())
            {
                *pbstrFaultstring = SysAllocString(pFaultInfo->getfaultstring());
                if (!(*pbstrFaultstring))
                    hr = E_OUTOFMEMORY;
            }
        }

        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapClient::get_faultstring - Unhandled Exception");
        return E_FAIL;
    }
#endif 

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::get_faultactor(BSTR * pbstrActor)
//
//  parameters:
//
//  description:
//        Returns the faultactor value from SOAP fault message
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::get_faultactor(
                /* [out, retval] */ BSTR * pbstrActor)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr = S_OK;
        CClientFaultInfo *pFaultInfo;

        if (IsBadWritePtr(pbstrActor, sizeof(BSTR *)))
            return E_INVALIDARG;
        *pbstrActor = NULL;

        hr = RetrieveFaultInfo(&pFaultInfo);
        if (SUCCEEDED(hr))
        {
            if (pFaultInfo->HasFaultInfo() && pFaultInfo->getfaultactor())
            {
                *pbstrActor = SysAllocString(pFaultInfo->getfaultactor());
                if (!(*pbstrActor))
                    hr = E_OUTOFMEMORY;
            }
        }
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapClient::get_faultstring - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::get_detail(BSTR * pbstrDetail)
//
//  parameters:
//
//  description:
//        Returns the detail element from SOAP fault message
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::get_detail(
                /* [out, retval] */ BSTR * pbstrDetail)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr = S_OK;
        CClientFaultInfo *pFaultInfo;

        if (IsBadWritePtr(pbstrDetail, sizeof(BSTR *)))
            return E_INVALIDARG;
        *pbstrDetail = NULL;
        hr = RetrieveFaultInfo(&pFaultInfo);
        if (SUCCEEDED(hr))
        {
            if (pFaultInfo->HasFaultInfo() && pFaultInfo->getdetail())
            {
                *pbstrDetail = SysAllocString(pFaultInfo->getdetail());
                if (!(*pbstrDetail))
                    hr = E_OUTOFMEMORY;
            }
        }
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapClient::get_detail - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::get_ConnectorProperty(BSTR PropertyName, VARIANT *pPropertyValue)
//
//  parameters:
//
//  description:
//        Returns the requested connection property
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::get_ConnectorProperty(
                    /* [in] */ BSTR PropertyName,
                    /* [out, retval] */ VARIANT *pPropertyValue)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr = S_OK;
        CClientThreadStore *pStore;
        CClientFaultInfo   *pFaultInfo = NULL;
        CComPointer<ISoapConnector> pConnector;

        VariantClear(pPropertyValue);

        CHK(RetrieveThreadStore(&pStore, true));
        pFaultInfo = pStore->getFaultInfo();
        if (!pFaultInfo)
            CHK(OLE_E_BLANK);

        // Clear previous faults/errors
        pFaultInfo->Reset();
        SetErrorInfo(0L, NULL);

        if (!m_bflInitSuccess)
        {
            pStore->getFaultInfo()->FaultMsgFromResourceHr(SOAP_IDS_CLIENT,
                SOAP_CLIENT_NOT_INITED, OLE_E_BLANK, NULL, NULL);
            CHK(OLE_E_BLANK);

        }

        CHK(RetrieveConnector(&pConnector));
        ASSERT(pConnector != 0);

        CHK(pConnector->get_Property(PropertyName, pPropertyValue));

        hr = S_OK;

    Cleanup:
        if (pConnector)
        {
            ReturnConnector(pConnector);
        }
        if (FAILED(hr) && pFaultInfo)
        {
            pFaultInfo->FaultMsgFromResourceHr(SOAP_IDS_CLIENT, 0, hr, NULL, NULL);
        }
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapClient::get_ConnectorProperty - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::put_ConnectorProperty(BSTR PropertyName, VARIANT PropertyValue)
//
//  parameters:
//
//  description:
//        Sets the requested connection property
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::put_ConnectorProperty(
                        /* [in] */ BSTR PropertyName,
                        /* [in] */ VARIANT PropertyValue)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr = S_OK;
        CClientThreadStore *pStore     = 0;
        CClientFaultInfo   *pFaultInfo = 0;
        CComPointer<ISoapConnector> pConnector;

        CHK(RetrieveThreadStore(&pStore, true));
        pFaultInfo = pStore->getFaultInfo();
        if (!pFaultInfo)
            CHK(OLE_E_BLANK);

        // Clear previous faults/errors
        pFaultInfo->Reset();
        SetErrorInfo(0L, NULL);

        if (!m_bflInitSuccess)
        {
            pStore->getFaultInfo()->FaultMsgFromResourceHr(SOAP_IDS_CLIENT,
                SOAP_CLIENT_NOT_INITED, OLE_E_BLANK, NULL, NULL);
            CHK(OLE_E_BLANK);
        }

        CHK(RetrieveConnector(&pConnector));
        ASSERT(pConnector != 0);

        CHK(pConnector->put_Property(PropertyName, PropertyValue));
        CHK(StoreProperty(PropertyName, PropertyValue));

        hr = S_OK;

    Cleanup:
        if (pConnector)
        {
            ReturnConnector(pConnector);
        }
        if (FAILED(hr) && pFaultInfo)
        {
            globalAddError(HrToMsgId(hr), SOAP_IDS_CLIENT, hr);
            pFaultInfo->FaultMsgFromResourceHr(SOAP_IDS_CLIENT, 0, hr, NULL, NULL);
        }
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapClient::put_ConnectorProperty - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::get_ClientProperty(BSTR PropertyName, VARIANT *pPropertyValue)
//
//  parameters:
//
//  description:
//        Returns the requested client property
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::get_ClientProperty(
                    /* [in] */ BSTR PropertyName,
                    /* [out, retval] */ VARIANT *pPropertyValue)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr = S_OK;
        CClientFaultInfo *pFaultInfo = NULL;

        VariantClear(pPropertyValue);

        CHK(RetrieveFaultInfo(&pFaultInfo));
        CHK_BOOL( pFaultInfo, OLE_E_BLANK);

        // Clear previous faults/errors
        pFaultInfo->Reset();
        SetErrorInfo(0L, NULL);
#ifndef UNDER_CE
        if (wcscmp(PropertyName, _T("ServerHTTPRequest"))==0)
        {
            VariantInit(pPropertyValue);
            V_VT(pPropertyValue) = VT_BOOL;
            V_BOOL(pPropertyValue) = m_bUseServerHTTPRequest ?
                    VARIANT_TRUE : VARIANT_FALSE;

        }
        else if (wcscmp(PropertyName, g_pwstrConnectorProgID) == 0)
#else
        if (wcscmp(PropertyName, g_pwstrConnectorProgID) == 0)
#endif
        {
            VariantInit(pPropertyValue);
            V_VT(pPropertyValue) = VT_BSTR;
            V_BSTR(pPropertyValue) = ::SysAllocString(m_bstrCLSIDConnector);
         }
        else if (wcscmp(PropertyName, g_pwstrEncodingAttribute) == 0)
        {
            V_VT(pPropertyValue) = VT_BSTR;
            if (m_bstrpreferredEncoding.Len()>0)
            {
                V_BSTR(pPropertyValue) = ::SysAllocString(m_bstrpreferredEncoding);
            }
            else
            {
                V_BSTR(pPropertyValue) = ::SysAllocString(g_pwstrUTF8);
            }
            CHK_BOOL( V_BSTR(pPropertyValue), E_OUTOFMEMORY);
        }
        else
        {       // Unknown property

            pFaultInfo->FaultMsgFromResourceHr(SOAP_IDS_CLIENT,
                SOAP_UNKNOWN_PROPERTY, CLIENT_UNKNOWN_PROPERTY, NULL, NULL);
            CHK(CLIENT_UNKNOWN_PROPERTY);
        }

    Cleanup:
        if (FAILED(hr) && pFaultInfo)
        {
            globalAddError(HrToMsgId(hr), SOAP_IDS_CLIENT, hr);
            pFaultInfo->FaultMsgFromResourceHr(SOAP_IDS_CLIENT, 0, hr, NULL, NULL);
        }
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapClient::get_ClientProperty - Unhandled Exception");
        return E_FAIL;
    }
#endif 

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::put_ClientProperty(BSTR PropertyName, VARIANT PropertyValue)
//
//  parameters:
//
//  description:
//        Sets the requested client property
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::put_ClientProperty(
                        /* [in] */ BSTR PropertyName,
                        /* [in] */ VARIANT PropertyValue)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr = E_INVALIDARG;
        CClientFaultInfo *pFaultInfo = NULL;

        // Clear previous faults/errors
        CHK(RetrieveFaultInfo(&pFaultInfo));
        CHK_BOOL(pFaultInfo, OLE_E_BLANK);

        pFaultInfo->Reset();
        SetErrorInfo(0L, NULL);
        
#ifndef UNDER_CE
        if (wcscmp(PropertyName, _T("ServerHTTPRequest"))==0)
        {

            if (V_VT(&PropertyValue)==VT_BOOL)
            {
                m_bUseServerHTTPRequest = V_BOOL(&PropertyValue)==VARIANT_TRUE ? true : false;
            }
        }
        else if (wcscmp(PropertyName, g_pwstrConnectorProgID) == 0)
#else
        if (wcscmp(PropertyName, g_pwstrConnectorProgID) == 0)
#endif
        {
            if (V_VT(&PropertyValue) == VT_BSTR)
            {
                if (V_BSTR(&PropertyValue) == 0 || * V_BSTR(&PropertyValue) == L'\0' ||
                    FAILED(hr = ::CLSIDFromProgID(V_BSTR(&PropertyValue), &m_CLSIDConnector)) ||
                    FAILED(hr = ::AtomicFreeAndCopyBSTR(m_bstrCLSIDConnector, V_BSTR(&PropertyValue))))
                {
                    ::FreeAndStoreBSTR(m_bstrCLSIDConnector, 0);
                    m_CLSIDConnector = CLSID_NULL;
                }
            }
        }
        else if (wcscmp(PropertyName, g_pwstrEncodingAttribute) == 0)
        {
            if (V_VT(&PropertyValue) == VT_BSTR)
            {
                CHK(m_bstrpreferredEncoding.Assign(V_BSTR(&PropertyValue)));
            }
        }
        else
        {       // Unknown property

            pFaultInfo->FaultMsgFromResourceHr(SOAP_IDS_CLIENT,
                SOAP_UNKNOWN_PROPERTY, CLIENT_UNKNOWN_PROPERTY, NULL, NULL);
            CHK(CLIENT_UNKNOWN_PROPERTY);

        }

    Cleanup:
        if (FAILED(hr) && pFaultInfo)
        {
            pFaultInfo->FaultMsgFromResourceHr(SOAP_IDS_CLIENT, 0, hr, NULL, NULL);
        }
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapClient::put_ClientProperty - Unhandled Exception");
        return E_FAIL;
    }
#endif 

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::putref_HeaderHandler(IDispatch *pHeaderHandler)
//
//  parameters:
//
//  description:
//        Sets the header handler
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::putref_HeaderHandler(IDispatch *pHeaderHandler)
{
    HRESULT hr = S_OK;

    // release the old guy
    m_pHeaderHandler.Clear();

    if (pHeaderHandler)
    {
        CHK(pHeaderHandler->QueryInterface(IID_IHeaderHandler, (void**) &m_pHeaderHandler));
    }

Cleanup:
    if (FAILED(hr))
    {
        hr = E_INVALIDARG;
    }

    return (hr);

}
/////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::EnsureTI(LCID lcid)
//
//  parameters:
//
//  description:
//        Ensure that TypeLibrary is loaded
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::EnsureTI(LCID lcid)
{
    HRESULT hr = S_OK;

    if (m_pSoapClientTypeInfo == NULL)
        hr = GetTI1(lcid);

    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::GetTI1(LCID lcid)
//
//  parameters:
//
//  description:
//        Initializes TypeInfo
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::GetTI1(LCID lcid)
{
    HRESULT hr = E_FAIL;

    if (m_pSoapClientTypeInfo != NULL)
        return S_OK;

    if (m_pSoapClientTypeInfo == NULL)
    {
        CAutoRefc<ITypeLib> pTypeLib;
        hr = LoadRegTypeLib(LIBID_MSSOAPLib, 1, 0, NULL, &pTypeLib);
        if (SUCCEEDED(hr))
        {
            CAutoRefc<ITypeInfo> spTypeInfo;
            hr = pTypeLib->GetTypeInfoOfGuid(IID_ISOAPClient, &spTypeInfo);
            if (SUCCEEDED(hr))
            {
                CAutoRefc<ITypeInfo2> spTypeInfo2;
#ifndef UNDER_CE
                if (SUCCEEDED(spTypeInfo->QueryInterface(&spTypeInfo2)))
#else
                if (SUCCEEDED(spTypeInfo->QueryInterface(IID_ISOAPClient, (void **)&spTypeInfo2)))
#endif

                {
                    (spTypeInfo.PvReturn())->Release();
                    spTypeInfo = spTypeInfo2.PvReturn();
                }

#ifndef UNDER_CE
                LoadNameCache(spTypeInfo);
#else
                hr = LoadNameCache(spTypeInfo);
                if(FAILED(hr))
                    return hr;
#endif 
                if (InterlockedCompareExchange((LPLONG) &m_pSoapClientTypeInfo, (LONG) (ITypeInfo*)spTypeInfo, 0)==0)
                   {
                       m_pSoapClientTypeInfo->AddRef();
                   }
            }
        }
    }

    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::LoadNameCache(ITypeInfo* pTypeInfo)
//
//  parameters:
//
//  description:
//        Initializes TypeInfoMap list
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::LoadNameCache(ITypeInfo* pTypeInfo)
{
    TYPEATTR* pta;
    HRESULT hr = pTypeInfo->GetTypeAttr(&pta);

    if (SUCCEEDED(hr))
    {
        if (m_pTypeInfoMap == NULL)
            m_pTypeInfoMap = new CTypeInfoMap;
            
#ifdef CE_NO_EXCEPTIONS
        if(!m_pTypeInfoMap)
            return E_OUTOFMEMORY;
#endif

        for (int i = 0; i < pta->cFuncs; i++)
        {
            FUNCDESC* pfd;
            if (SUCCEEDED(pTypeInfo->GetFuncDesc(i, &pfd)))
            {
                CAutoBSTR bstrName;
                if (SUCCEEDED(pTypeInfo->GetDocumentation(pfd->memid, &bstrName, NULL, NULL, NULL)))
                {
                    LONG    clen = ::SysStringLen(bstrName);
                    m_pTypeInfoMap->Add(bstrName.PvReturn(), clen, pfd->memid);
                }
                pTypeInfo->ReleaseFuncDesc(pfd);
            }
        }
        pTypeInfo->ReleaseTypeAttr(pta);
    }

    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CSoapClient::GetIDsOfNames (REFIID riid, LPOLESTR* rgszNames, UINT cNames,
//                                          LCID lcid, DISPID* rgdispid)
//  parameters:
//
//  description:
//        Returns Dispid of a method, making up one as needed
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapClient::GetIDsOfNames(
                REFIID riid,
                LPOLESTR* rgszNames,
                UINT cNames,
                LCID lcid,
                DISPID* rgdispid)
{
#ifndef CE_NO_EXCEPTIONS
try
    {
#endif 
        HRESULT hr = S_FALSE;

        CHK (EnsureTI(lcid));

        // process array of names
        if (m_pSoapClientTypeInfo != NULL)
        {
            CTypeInfoMapInfo* pTypeInfo;
            CLinkedList *pLinkIndex;

            // find names and fill DISPID array
            hr = S_FALSE;
            for (int i = 0; i < (int)cNames; i++)
            {
                int n = wcslen(rgszNames[i]);
                if (m_pTypeInfoMap)
                {
                    pTypeInfo = m_pTypeInfoMap->First(&pLinkIndex);
                    while (pTypeInfo)
                    {
                        if ((n == pTypeInfo->m_nLen)
                            &&
                            (wcsicmp(pTypeInfo->m_bstr, rgszNames[i]) == 0))
                        {
                            rgdispid[i] = pTypeInfo->m_id;
                            break;
                        }
                        pTypeInfo = m_pTypeInfoMap->Next(&pLinkIndex);
                    }
                    if (pTypeInfo)
                    {
                        hr = m_pSoapClientTypeInfo->GetIDsOfNames(rgszNames + i, 1, &rgdispid[i]);
                        if (FAILED(hr))
                            break;
                    }
                    else
                        hr = S_FALSE;
                }
            }
        }

        // if no method found we need to scan the current operationlist
        if (hr == S_FALSE)
        {
            ClearErrors();

            // preset result with failures...
            rgdispid[0] = DISPID_UNKNOWN;
            long lOffset = 0;
            hr = m_abstrNames.FindName( rgszNames[0], &lOffset );
            if (SUCCEEDED(hr))
            {
                // create our dispid out of the resulting offset
                rgdispid[0] = DISPID_SOAPCLIENT_UNKNOWNSTART + lOffset;
            }
            else
            {
                hr = DISP_E_UNKNOWNNAME;
            }
        }

    Cleanup:
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapClient::GetIDsOfNames - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CSoapClient::Invoke(DISPID dispidMember, REFIID riid, LCID lcid,
//                                      WORD wFlags, DISPPARAMS * pdispparams, VARIANT * pvarResult,
//                                      EXCEPINFO * pexcepinfo, UINT * puArgErr)
//
//  parameters:
//
//  description:
//        Invokes the requested method
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapClient::Invoke(
                DISPID dispidMember,
                REFIID riid,
                LCID lcid,
                WORD wFlags,
                DISPPARAMS * pdispparams,
                VARIANT * pvarResult,
                EXCEPINFO * pexcepinfo,
                UINT * puArgErr)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr;
        hr = EnsureTI(lcid);


        if (m_pSoapClientTypeInfo != NULL)
        {
            if (dispidMember >= DISPID_SOAPCLIENT_UNKNOWNSTART && dispidMember <= DISPID_SOAPCLIENT_UNKNOWNSTART + m_abstrNames.GetSize())
            {
                // that's our range....
                hr = InvokeUnknownMethod(
                            dispidMember,
                            pdispparams,
                            pvarResult,
                            pexcepinfo,
                            puArgErr);

            }

            else
                hr = m_pSoapClientTypeInfo->Invoke(
                            (IDispatch*)this,
                            dispidMember,
                            wFlags,
                            pdispparams,
                            pvarResult,
                            pexcepinfo,
                            puArgErr);
        }
        hr = FailedError(hr, pexcepinfo);

        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "SoapClient - Invoke() - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::InvokeUnknownMethod(DISPID dispid, DISPPARAMS * pdispparams,
//                          VARIANT * pvarResult, EXCEPINFO * pexcepinfo, UINT * puArgErr)
//
//  parameters:
//
//  description:
//        Invokes a method that is not in the object's type library, obtains the necessary
//        information from the WSDL file.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::InvokeUnknownMethod(
                        DISPID dispid,
                        DISPPARAMS * pdispparams,
                        VARIANT * pvarResult,
                        EXCEPINFO * pexcepinfo,
                        UINT * puArgErr)
{
    HRESULT hr = S_OK;
    CAutoRefc<IWSDLOperation> pIWSDLOp;
    CAutoRefc<IStream> pIStreamIn;
    CAutoRefc<IStream> pIStreamOut;
    CAutoRefc<ISoapReader> pISoapRead;
    CAutoRefc<ISoapConnector> pSoapConnector;
    VARIANT_BOOL bSuccess;
    BOOL bHasFault;
    VARIANTARG varArg;

    CClientThreadStore *pStore = NULL;

    TRACE(("Entering Client::InvokeUnknownMethod\n"));

    VariantInit(&varArg);

    CHK(RetrieveThreadStore(&pStore, true));
    CHK(RetrieveConnector(&pSoapConnector));

    ClearErrors();

    if (!m_bflInitSuccess)
    {
        CHK(OLE_E_BLANK);    // We map this to 'not initialized error'
    }

    if (!m_pIEnumWSDLOps)
    {
        CHK(DISP_E_MEMBERNOTFOUND);
    }

    ASSERT((dispid >= DISPID_SOAPCLIENT_UNKNOWNSTART && dispid <= DISPID_SOAPCLIENT_UNKNOWNSTART + m_abstrNames.GetSize()));

    //adjust dispid for the unknown methods
    dispid -= DISPID_SOAPCLIENT_UNKNOWNSTART;


    if (pdispparams->cNamedArgs != 0)
        CHK(DISP_E_NONAMEDARGS);

    // Find the method from the Operations list
    if (FAILED(m_pIEnumWSDLOps->Find(m_abstrNames.getName(dispid), &pIWSDLOp)))
    {
        CHK(DISP_E_MEMBERNOTFOUND);
    }


    // Get the stream
    CHK(pSoapConnector->get_InputStream(&pIStreamIn));

    // Pass the connect stream to SoapSerializer
    pStore->getSerializer()->reset();

    VARIANT vStream;
    VariantInit(&vStream);
    vStream.vt = VT_UNKNOWN;
    V_UNKNOWN(&vStream) = pIStreamIn;

    CHK(pStore->getSerializer()->Init(vStream));

    // set the output encoding on the connector
    if (m_bstrpreferredEncoding && ::SysStringLen(m_bstrpreferredEncoding) > 0)
    {
        CVariant var;
        CAutoBSTR bstrPropName;

        CHK(bstrPropName.Assign(L"HTTPCharset"));

        // we will overwrite anything that was previously set. And we don't care. If this value is different
        // from the value set on the serializer later, we would create an invalid message
        // to set the prefered encoding when working with the highlevel client.
        CHK(var.Assign(m_bstrpreferredEncoding, true));
        CHK(pSoapConnector->put_Property(bstrPropName, var));
    }


    // Begin writing out the message
    CHK(pSoapConnector->BeginMessageWSDL(pIWSDLOp));

    // Create the message body here
    CHK(ConstructSoapRequestMessage(pdispparams, pStore->getSerializer(), pIWSDLOp, pIStreamIn));

    // Send the message over to the server
    hr = pSoapConnector->EndMessage();
    if (FAILED(hr))
    {   // Try to obtain info from the interface, if no SOAP
        // interface, we will construct a generic one later.
        globalAddError(HrToMsgId(hr), SOAP_IDS_CLIENT, hr);
        globalAddError(SOAP_IDS_SENDMESSAGEFAILED, SOAP_IDS_CLIENT,  hr);
        goto Cleanup;
    }

    // Obtain the response stream
    hr = pSoapConnector->get_OutputStream(&pIStreamOut);
    if (FAILED(hr) || pIStreamOut == 0)
    {
        globalAddError(SOAP_IDS_READMESSAGEFAILED, SOAP_IDS_CLIENT,  hr);
        goto Cleanup;
    }

    // Instantiate SoapReader to read the input message
    CHK(CoCreateInstance(CLSID_SoapReader, NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_ISoapReader,
                          (void**)&pISoapRead));

    // Pass the input stream into SoapReader.
    varArg.vt = VT_UNKNOWN;
    CHK(pIStreamOut->QueryInterface(IID_IUnknown, (void **)&(varArg.punkVal)));
    hr = pISoapRead->load(varArg, NULL, &bSuccess);
    if (FAILED(hr) || !bSuccess)
    {
        if (!FAILED(hr))
        {
            hr = CONN_E_BAD_CONTENT;        // Got bad content back from server
        }
        else
        {
            globalAddError(SOAP_IDS_SERVER_COULDNOTLOADREQUEST, SOAP_IDS_CLIENT, hr);
        }
        goto Cleanup;
    }


    // It will return the HRESULT in the FAULT details if there is one
    CHK(CheckForFault(pISoapRead, &bHasFault));

    if (bHasFault)      // Got a fault back but not hresult.
    {
        hr = E_FAIL;
        goto Cleanup;
    }

    // Pass the SoapReader into WSDLReader to get the parts out
    CHK(pIWSDLOp->Load(pISoapRead, VARIANT_FALSE));

    // Process the response, errors/faults if any
    CHK(ProcessSoapResponseMessage(pdispparams, pvarResult, pexcepinfo, pISoapRead, pIWSDLOp));

Cleanup:
    if (FAILED(hr) && pStore)
    {
        // Construct a generic error message
        pStore->getFaultInfo()->FaultMsgFromResourceHr(SOAP_IDS_CLIENT, 0, hr,
                NULL, NULL);
    }
    TRACE(("Exiting Client::InvokeUnknownMethod\n"));
    ::VariantClear(&varArg);
    if (pStore && pSoapConnector)
    {
        pSoapConnector->Reset();
        pStore->ReturnConnector(pSoapConnector);
    }
    if (pStore && pStore->getFaultInfo())
    {
        pStore->getFaultInfo()->PublishErrorInfo();
    }

    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::ConstructSoapRequestMessage ( DISPPARAMS * pdispparams,
//                                      ISoapSerializer *pISoapSer, IWSDLOperation * pIWSDLOp,
//                                      ISequentialStream * pIStream)
//
//  parameters:
//
//  description:
//        Creates the SOAP envelope
//        This method assumes that parameter order matches the WSDL file order.
//        No named parameter support is provided for the dispatch parameters
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::ConstructSoapRequestMessage(
                    DISPPARAMS * pdispparams,
                    ISoapSerializer *pISoapSer,
                    IWSDLOperation * pIWSDLOp,
                    ISequentialStream * pIStream)
{
    HRESULT hr = S_OK;

#ifndef UNDER_CE
    VARIANT_BOOL    bHeaders;
#endif 
    smIsInputEnum   enIsInput;
    CAutoRefc<IEnumSoapMappers>  pIEnumSoapMaps;
    CAutoRefc<ISoapMapper>  pISoapMap;
#ifndef UNDER_CE
    long        cFetched;
#endif 
    int            i;
    int                iArgs;
    long            lArgCount;
    VARIANTARG      varArg;
    UINT            uiErr;
    VARTYPE        vtType;
    CSoapHeaders    soapHeaders;
    TCHAR           *pchEncStyle; 
    


    VariantInit(&varArg);

    // Pass the parameter values to SoapSerializer
    // call endSoapEnvelope()

    // get the EncodingStyle from the operation
   pchEncStyle = ((CWSDLOperation*)(IWSDLOperation*)pIWSDLOp)->getEncoding(true); 


    CHK(pISoapSer->startEnvelope((BSTR) g_pwstrEnvPrefix, (BSTR) pchEncStyle, m_bstrpreferredEncoding));

    if (m_pHeaderHandler)
    {
        CHK(soapHeaders.Init());
        CHK(soapHeaders.AddHeaderHandler(m_pHeaderHandler));
        CHK(soapHeaders.WriteHeaders(pISoapSer, 0));
    }

    CHK(pISoapSer->startBody(/* BSTR enc_style_uri */ NULL));

    // Go over the SoapMapper enumerations, putting the input parameter   
    // values into each one and then saving the value.
    CHK(pIWSDLOp->GetOperationParts(&pIEnumSoapMaps));

    // Arguments in DISPPARAMS are located in the inverse order.
    // but DispGetParams takes care of this...
    i = 0;
    iArgs = 0;
    // Finally add the soap body elements

    // verify that we have ENOUGH parameters for the mappers

    CHK( ((CEnumSoapMappers*)(IEnumSoapMappers*)pIEnumSoapMaps)->parameterCount(&lArgCount));
#ifndef UNDER_CE
    if (pdispparams->cArgs != lArgCount)
#else
    if (pdispparams->cArgs != (ULONG)lArgCount)
#endif 
    {
           hr = E_INVALIDARG;
           globalAddError(CLIENT_IDS_INCORRECTNUMBEROFPARAMETERS, SOAP_IDS_CLIENT,  hr);
        goto Cleanup;
    }


    while (iArgs < (int) pdispparams->cArgs)
    {
        CHK( ((CEnumSoapMappers*)(IEnumSoapMappers*)pIEnumSoapMaps)->FindParameter(iArgs, &pISoapMap));
           CHK( pISoapMap->get_isInput(&enIsInput));
        if (enIsInput != smOutput)
        {
            // Add the values from the DISPPARAMS to the SoapMapper.
               // get the comtype
            vtType = ((CSoapMapper*)(ISoapMapper*)pISoapMap)->getVTType();
            VariantClear(&varArg);
            if ( (vtType == VT_SAFEARRAY) || (vtType & VT_ARRAY))
                 hr = ArrayGetParam(pdispparams, (UINT)iArgs, &varArg);
            else
                if (vtType == VT_VARIANT)
                    hr = VariantGetParam(pdispparams, (UINT)iArgs, &varArg);
                else
                    hr = DispGetParam(pdispparams, (UINT)iArgs, vtType, &varArg, &uiErr);
            if (FAILED(hr))
            {
                CAutoBSTR bstrName;
                pISoapMap->get_elementName(&bstrName);
                globalAddError(CLIENT_IDS_DISPGETPARAMFAILED, SOAP_IDS_CLIENT,  hr, bstrName);
                goto Cleanup;
            }
            CHK( pISoapMap->put_comValue(varArg));
        }
        iArgs++;
        pISoapMap.Clear();
    }

    // Now save the operation into the body
    CHK( pIWSDLOp->Save(pISoapSer, VARIANT_TRUE));

    // End of Soap body element
    CHK( pISoapSer->endBody());
    // Finally end the envelope
    CHK( pISoapSer->endEnvelope());

Cleanup:
    VariantClear(&varArg);
    return hr;

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::ProcessSoapResponseMessage (DISPPARAMS * pdispparams,
//                                         VARIANT * pvarResult, EXCEPINFO * pexcepinfo,
//                                         ISoapReader * pISoapReader, IWSDLOperation * pIWSDLOp)
//  parameters:
//
//  description:
//        Processes the SOAP envelope response
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::ProcessSoapResponseMessage(
                    DISPPARAMS * pdispparams,
                    VARIANT * pvarResult,
                    EXCEPINFO * pexcepinfo,
                    ISoapReader * pISoapReader,
                    IWSDLOperation * pIWSDLOp)
{
    HRESULT hr = S_OK;
    CAutoRefc<IEnumSoapMappers> pIEnumSoapMaps;
    CAutoRefc<ISoapMapper>  pISoapMap;
#ifndef UNDER_CE
    long cFetched;
    smIsInputEnum enIsInput;
#endif 
    VARIANT *pvarNext = NULL;
    VARIANTARG varArg;
    LCID lcid;
    int    iArgs;
    CSoapHeaders    soapHeaders;


    VariantInit(&varArg);
    lcid = GetSystemDefaultLCID();

    VariantInit(&varArg);

    // Pass the parameter values to SoapSerializer
    // call endSoapEnvelope()

    if (m_pHeaderHandler)
    {
        CHK(soapHeaders.Init());
        CHK(soapHeaders.AddHeaderHandler(m_pHeaderHandler));
		hr = soapHeaders.ReadHeaders(pISoapReader, 0);

		if (FAILED(hr))
		{
			if (hr == WSDL_MUSTUNDERSTAND)
			{
			    globalAddError(CLIENT_IDS_HEADERNOTUNDERSTOOD, SOAP_IDS_CLIENT, hr);
			}
			goto Cleanup;
		}
    }


    CHK(pIWSDLOp->GetOperationParts(&pIEnumSoapMaps));

    // Go through the the soap response elements

    iArgs = 0;

    while (iArgs < (int) pdispparams->cArgs)
    {
        pvarNext = pdispparams->rgvarg + pdispparams->cArgs - 1 - iArgs ;
        CHK(((CEnumSoapMappers*)(IEnumSoapMappers*)pIEnumSoapMaps)->FindParameter(iArgs, &pISoapMap));
        CHK(moveResult(pISoapMap, pvarNext));
        iArgs++;
        pISoapMap.Clear();
    }

    // let's check the return value
    if (pvarResult)
    {
        hr = ((CEnumSoapMappers*)(IEnumSoapMappers*)pIEnumSoapMaps)->FindParameter(-1, &pISoapMap);
        if (FAILED(hr))
        {
               hr = E_INVALIDARG;
               globalAddError(CLIENT_IDS_INCORRECTNUMBEROFPARAMETERS, SOAP_IDS_CLIENT,  hr);
            goto Cleanup;
        }
        VariantClear(pvarResult);
        CHK(moveResult(pISoapMap, pvarResult));
    }


    // if we got here, change the S_FALSE from the ->Next to S_OK, as everything is OK
    hr = S_OK;

Cleanup:
    VariantClear(&varArg);
    return hr;

}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::moveResult(ISoapMapper *pMapper, VARIANT *pVariant)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::moveResult(ISoapMapper *pMapper, VARIANT *pVariant)
{
    HRESULT hr;
    smIsInputEnum enIsInput;
    VARIANT varArg;
     LCID lcid;

    VariantInit(&varArg);
    lcid = GetSystemDefaultLCID();

    CHK(pMapper->get_isInput(&enIsInput));
    if (enIsInput != smInput)
    {
        CHK(pMapper->get_comValue(&varArg));
        if ( (pVariant->vt == (VT_VARIANT|VT_BYREF)) ||
              (pVariant->vt == VT_EMPTY) )
        {
            VARIANT * pvarDest = pVariant;
            if (V_ISBYREF(pVariant))
                pvarDest=pVariant->pvarVal;

            if (V_ISARRAY(&varArg))
                CHK(VariantCopy(pvarDest, &varArg))
            else
                 CHK(VariantChangeTypeEx(pvarDest,&varArg, lcid, 0, varArg.vt));
             VariantClear(&varArg);
        }
        else
        {
            // Move the value over
            // but don't do it, if it's not passed by ref
            if (pVariant->vt & VT_BYREF)
            {
                CHK(VariantCopyToByRef(pVariant, &varArg, lcid));
            }
        }
    }
Cleanup:
    VariantClear(&varArg);
    return (hr);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::VariantCopyToByRef(VARIANT *pvarDest, VARIANT *pvarSrc, LCID lcid)
//
//  parameters:
//
//  description:
//        Converts variant type to its' BYREF type, clears the source
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::VariantCopyToByRef(VARIANT *pvarDest, VARIANT *pvarSrc, LCID lcid)
{
    HRESULT hr = S_OK;

    if ((pvarDest->vt & ~VT_BYREF) != pvarSrc->vt)
        CHK(VariantChangeTypeEx(pvarSrc, pvarSrc,  LCID_TOUSE, VARIANT_ALPHABOOL, pvarDest->vt & ~VT_BYREF));

    ASSERT((pvarDest->vt & ~VT_BYREF) == pvarSrc->vt);

    if ((pvarDest->vt & ~VT_BYREF) != pvarSrc->vt)
        CHK(E_FAIL);

    switch (pvarSrc->vt)
    {
        case VT_I1:
        case VT_UI1:
            // These have size of 1 byte
            *(pvarDest->pbVal) = pvarSrc->bVal;
            break;

        case VT_BOOL:
        case VT_I2:
        case VT_UI2:
            // These have size of 2 bytes
            *(pvarDest->puiVal) = pvarSrc->uiVal;
            break;
        case VT_I4:
        case VT_UI4:
        case VT_R4:
        case VT_ERROR:
            // These have 4 byte size
            *(pvarDest->pulVal) = pvarSrc->ulVal;
            break;
        case VT_I8:
        case VT_UI8:
        case VT_R8:
        case VT_DATE:
            // These have 8 byte size
            *(pvarDest->pullVal) = pvarSrc->ullVal;
            break;

        case VT_DECIMAL:
            // These have a 12 byte size
            *(pvarDest->pdecVal) = pvarSrc->decVal;
            break;

        case VT_INT:
        case VT_UINT:
        case VT_PTR:
        case VT_UNKNOWN:
        case VT_DISPATCH:
            // These have the size of an integer pointer
            //   (4 byte on 32 bit platform, 8 byte on 64bit platforms)
            *(pvarDest->puintVal) = pvarSrc->uintVal;
            break;

        case VT_BSTR:
            if (*(pvarDest->pbstrVal))
                SysFreeString(*(pvarDest->pbstrVal));
            *(pvarDest->pbstrVal) = pvarSrc->bstrVal;
            pvarSrc->bstrVal = NULL;
            break;

        case VT_CY:
            *(pvarDest->pcyVal) = pvarSrc->cyVal;
            break;

        default:
            // arrays are 'or'-ed in, we can not recognize them that easy in the switch statement
            if (V_ISARRAY(pvarSrc))
            {
                SAFEARRAY * psaSrc  = V_ARRAY(pvarSrc);
                SAFEARRAY * psaDest = *V_ARRAYREF(pvarDest);

                // arrays with any of these bits set should not get touched by us
                static const DWORD cNoTouchArray =     (FADF_AUTO | FADF_STATIC | FADF_EMBEDDED | FADF_FIXEDSIZE |    FADF_RESERVED);
                if (psaDest)
                {
                    if (psaDest->fFeatures & cNoTouchArray)
                    {
                        if (SUCCEEDED( SafeArrayCopyData(psaSrc, psaDest) ))
                        {
                            VariantClear(pvarSrc);
                            *V_ARRAYREF(pvarDest) = psaDest;
                            break;
                        }
                    }
                    else
                    {
                        CHK(SafeArrayDestroy(psaDest));
                        psaDest = NULL;
                    }
                }

                if (psaDest == NULL)
                {
                    psaDest = psaSrc;       // just take the existing one
                    *V_ARRAYREF(pvarDest) = psaDest;
                    V_ARRAY(pvarSrc) = NULL;
                    V_VT(pvarSrc) = VT_EMPTY;
                    break;
                }
                ERROR_ONHR0(E_INVALIDARG, CLIENT_IDS_ARRAYRESULTBYREF, SOAP_IDS_CLIENT);
            }
            ASSERT(FALSE);
            CHK(E_INVALIDARG);
            break;
    }

    pvarSrc->vt = VT_EMPTY;

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::CheckForFault(ISoapReader *pISoapReader, BOOL *pbHasFault)
//
//  parameters:
//
//  description:
//        Processes the SOAP FAULT response, sets up IErrorInfo and HRESULT from details, if there
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::CheckForFault(ISoapReader *pISoapReader, BOOL *pbHasFault)
{
    HRESULT hr = S_OK;
    HRESULT hresult = S_OK;


    CAutoBSTR        bstrfaultcode;
    CAutoBSTR        bstrfaultstring;
    CAutoBSTR        bstrfaultactor;
    CAutoBSTR        bstrdetail;
    CAutoBSTR        bstrTemp;

    CAutoRefc<ICreateErrorInfo>  pICrtErr;
    CAutoRefc<IErrorInfo>       pIErrInfo;

    CAutoRefc<IXMLDOMDocument>  pIXMLDOM;
    CAutoRefc<IXMLDOMNode>  pIXMLNodeFault;
    CAutoRefc<IXMLDOMNode>  pNodeTemp;
    CAutoRefc<IXMLDOMNode>  pNodeError;
    CAutoRefc<IXMLDOMNode>  pNodeHR;
    CAutoRefc<IXMLDOMNode>  pNodeServerErrorInfo;;
    CClientFaultInfo        *pFault;

    CAutoFormat autoFormat;
    WCHAR                       *achNameSpaces[] = { _T("s"), _T("e") };
    WCHAR                       *achNameSpaceURIs[] = { (WCHAR *) g_pwstrEnvNS, (WCHAR *) g_pwstrMSErrorNS };


    *pbHasFault = FALSE;

    // Find out if there is a Fault envelope in the response message
    CHK(pISoapReader->get_DOM(&pIXMLDOM));

    hr = _XPATHUtilPrepareLanguage(pIXMLDOM, &(achNameSpaces[0]), &(achNameSpaceURIs[0]), 2);
    if (FAILED(hr))
    {
        goto Cleanup;
    }

    hr = _XPATHUtilFindNodeFromRoot(pIXMLDOM, _T("//s:Envelope/s:Body/s:Fault"), &pIXMLNodeFault);
    if (FAILED(hr))
    {
        // If there is no Fault element, return
        _XPATHUtilResetLanguage(pIXMLDOM);
        hr = S_OK;
        goto Cleanup;
    }

    // if we have this, we have a fault, maybe an invalid, but it's a fault
    *pbHasFault = true;

    // get the faultcode element
    CHK(autoFormat.sprintf(_T("%s"), g_pwstrFaultcode));
    hr = pIXMLNodeFault->selectSingleNode(&autoFormat, &pNodeTemp);

    if (hr != S_OK)
    {
        // check for incorrectly done namespace qualified errors
        CHK(autoFormat.sprintf(_T("s:%s"), g_pwstrFaultcode));
        hr = pIXMLNodeFault->selectSingleNode(&autoFormat, &pNodeTemp);
    }
    if (hr != S_OK)
    {
        hr = E_FAIL;
        goto Cleanup;
    }

        // Create an error object here and fill in the parts
    CHK(CreateErrorInfo(&pICrtErr));
    CHK(pICrtErr->SetGUID(IID_ISOAPClient));
    CHK(pNodeTemp->get_text(&bstrfaultcode));
    pNodeTemp.Clear();

    CHK(pICrtErr->SetSource(bstrfaultcode));


    CHK(autoFormat.sprintf(_T("%s"), g_pwstrFaultstring));
    hr = pIXMLNodeFault->selectSingleNode(&autoFormat, &pNodeTemp);

    if (hr != S_OK)
    {
        CHK(autoFormat.sprintf(_T("s:%s"), g_pwstrFaultstring));
        hr = pIXMLNodeFault->selectSingleNode(&autoFormat, &pNodeTemp);
    }
    if (hr != S_OK)
    {
        hr = E_FAIL;
        goto Cleanup;
    }

    CHK(pNodeTemp->get_text(&bstrfaultstring));
    pNodeTemp.Clear();
    CHK(pICrtErr->SetDescription(bstrfaultstring));



    CHK(autoFormat.sprintf(_T("%s"), g_pwstrFaultactor));
    hr = pIXMLNodeFault->selectSingleNode(&autoFormat, &pNodeTemp);

    if (hr != S_OK)
    {
        CHK(autoFormat.sprintf(_T("s:%s"), g_pwstrFaultactor));
        hr = pIXMLNodeFault->selectSingleNode(&autoFormat, &pNodeTemp);
    }

    if (SUCCEEDED(hr) && pNodeTemp)
    {
        // actor is not a MUST  
        CHK(pNodeTemp->get_text(&bstrfaultactor));
    }
    pNodeTemp.Clear();

    CHK(autoFormat.sprintf(_T("%s"), g_pwstrDetail));
    hr = pIXMLNodeFault->selectSingleNode(&autoFormat, &pNodeTemp);

    if (hr != S_OK)
    {
        CHK(autoFormat.sprintf(_T("s:%s"), g_pwstrDetail));
        hr = pIXMLNodeFault->selectSingleNode(&autoFormat, &pNodeTemp);
    }


    if (SUCCEEDED(hr) && pNodeTemp)
    {
        // detail is not a MUST  
        // if we have a detail, we either find our subtree and get it out, OR
        // we just take the string itself

        CHK(autoFormat.sprintf(_T("e:%s"), g_pwstrErrorInfoElement));
        hr = pNodeTemp->selectSingleNode(&autoFormat, &pNodeError);
        if (SUCCEEDED(hr) && pNodeError)
        {
            // get the return HR OUT of the subtree
            CHK(autoFormat.sprintf(_T(".//e:%s"), g_pwstrErrorReturnHR));
            hr = pNodeError->selectSingleNode(&autoFormat, &pNodeHR);
            if (SUCCEEDED(hr) && pNodeHR)
            {
                CHK(pNodeHR->get_text(&bstrTemp));
                hresult = _wtoi(bstrTemp);
            }

            // check for special errorinfo information:
            CHK(autoFormat.sprintf(_T(".//e:%s"), g_pwstrErrorServerElement));
            hr = pNodeError->selectSingleNode(&autoFormat, &pNodeServerErrorInfo);
            if (SUCCEEDED(hr) && pNodeServerErrorInfo)
            {
                pNodeHR.Clear();
                bstrTemp.Clear();
                CHK(autoFormat.sprintf(_T(".//e:%s"), g_pwstrErrorHelpFile));
                hr = pNodeServerErrorInfo->selectSingleNode(&autoFormat, &pNodeHR);
                if (SUCCEEDED(hr) && pNodeHR)
                {
                    CHK(pNodeHR->get_text(&bstrTemp));
                }
                CHK(pICrtErr->SetHelpFile(bstrTemp));

                pNodeHR.Clear();
                bstrTemp.Clear();
                CHK(autoFormat.sprintf(_T(".//e:%s"), g_pwstrErrorDescription));
                hr = pNodeServerErrorInfo->selectSingleNode(&autoFormat, &pNodeHR);
                if (SUCCEEDED(hr) && pNodeHR)
                {
                    CHK(pNodeHR->get_text(&bstrTemp));
                }

                CHK(pICrtErr->SetDescription(bstrTemp));

                pNodeHR.Clear();
                bstrTemp.Clear();
                CHK(autoFormat.sprintf(_T(".//e:%s"), g_pwstrErrorSource));
                hr = pNodeServerErrorInfo->selectSingleNode(&autoFormat, &pNodeHR);
                if (SUCCEEDED(hr) && pNodeHR)
                {
                    CHK(pNodeHR->get_text(&bstrTemp));
                }
                CHK(pICrtErr->SetSource(bstrTemp));

                pNodeHR.Clear();
                bstrTemp.Clear();
                CHK(autoFormat.sprintf(_T(".//e:%s"), g_pwstrErrorHelpContext));
                hr = pNodeServerErrorInfo->selectSingleNode(&autoFormat, &pNodeHR);
                if (SUCCEEDED(hr) && pNodeHR)
                {
                    CHK(pNodeHR->get_text(&bstrTemp));
                }
                
#ifndef UNDER_CE             
                CHK(pICrtErr->SetHelpContext(_wtoi(bstrTemp)));
#else
                if(!bstrTemp){
                    CHK(pICrtErr->SetHelpContext(0));
                }
                else{
                    CHK(pICrtErr->SetHelpContext(_wtoi(bstrTemp)));
                }
#endif

            }
        }
        CHK(pNodeTemp->get_xml(&bstrdetail));
    }

    // We went through the whole fault message

    CHK(pICrtErr->QueryInterface(IID_IErrorInfo, (void **)&pIErrInfo))

    // Also set the m_SoapFaultInfo fields
    hr = RetrieveFaultInfo(&pFault);
    if (SUCCEEDED(hr))
    {
        pFault->SetErrorInfo(pIErrInfo);
        pFault->SetFields(bstrfaultcode.PvReturn(),
                            bstrfaultstring.PvReturn(),
                            bstrfaultactor.PvReturn(),
                            bstrdetail.PvReturn());
    }
    else
    {
        CHK(SetErrorInfo(0L, pIErrInfo));
    }
    if (hresult != S_OK)    // Return the  hresult from the fault message
        hr = hresult;

Cleanup:
    return hr;

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapClient::InterfaceSupportsErrorInfo(REFIID riid)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapClient::InterfaceSupportsErrorInfo(REFIID riid)
{
    if (InlineIsEqualGUID(IID_ISOAPClient,riid))
        return S_OK;
    return S_FALSE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::RetrieveThreadStore(CClientThreadStore **ppThreadStore, BOOL fCreateConnector)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::RetrieveThreadStore(CClientThreadStore **ppThreadStore, BOOL fCreateConnector)
{
    HRESULT hr = S_OK;
    CClientThreadStore * pStore;

    CAutoRefc<ISoapConnector> pConnector;
    CAutoRefc<ISoapSerializer> pSerializer;


    pStore = m_threadInfo.Lookup(GetCurrentThreadId());

    if (!pStore)
    {
        // first call on this thread, create one....

        pStore = new CSoapObject<CClientThreadStore>(0);
        if (!pStore)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }

        // now create the objects we need to hold on to


        // Instantiate a SoapSerializer object, initialize it
        hr = CoCreateInstance(CLSID_SoapSerializer, NULL,
                              CLSCTX_INPROC_SERVER,
                              IID_ISoapSerializer,
                              (void**)&pSerializer);

        if (FAILED(hr))
        {
            globalAddError(SOAP_IDS_COULDNOTCREATESERIALIZER, SOAP_IDS_CLIENT,  hr);
            goto Cleanup;
        }

        pStore->setSerializer(pSerializer);

        m_threadInfo.Replace(pStore, GetCurrentThreadId());
    }

    // if we got here, assign pointer for error handling

    *ppThreadStore = pStore;

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::RetrieveFaultInfo(CFaultInfo **ppFaultInfo)
//
//  parameters:
//
//  description:
//      helper, as the faultinfo is used often
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::RetrieveFaultInfo(CClientFaultInfo **ppFaultInfo)
{
    HRESULT hr = S_OK;
    CClientThreadStore *pStore;

    hr = RetrieveThreadStore(&pStore, false);
    if (FAILED(hr))
    {
        goto Cleanup;
    }

    *ppFaultInfo = pStore->getFaultInfo();

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::RetrieveConnector(ISoapConnector **pConnector)
//
//  parameters:
//
//  description:
//        Retrieves connector from the pool, creates new one if necessary.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::RetrieveConnector(ISoapConnector **pConnector)
{
    HRESULT hr = S_OK;
    CClientThreadStore *pStore = 0;
    ISoapConnector     *pConn  = 0;

    CHK_ARG(pConnector && ! *pConnector);
    CHK(RetrieveThreadStore(&pStore, false));
    ASSERT(pStore != 0);
    CHK(pStore->RetrieveConnector(&pConn));

    if (pConn == 0 && m_bflInitSuccess)
    {
        if (m_CLSIDConnector == CLSID_NULL)
        {
            // Connect to the requested port
            hr = m_pISoapConnFact->CreatePortConnector(m_pIWSDLPort, &pConn);
            if (FAILED(hr))
            {
                globalAddError(SOAP_IDS_COULDNOTCREATECONNECTOR, SOAP_IDS_CLIENT,  hr);
                goto Cleanup;
            }
        }
        else
        {
            hr = ::CoCreateInstance(m_CLSIDConnector, 0, CLSCTX_INPROC_SERVER,
                        IID_ISoapConnector, reinterpret_cast<void **>(&pConn));
            if (FAILED(hr))
            {
                globalAddError(SOAP_IDS_COULDNOTCREATECONNECTOR, SOAP_IDS_CLIENT,  hr);
                goto Cleanup;
            }

            if (FAILED(hr = pConn->ConnectWSDL(m_pIWSDLPort)))
            {
                goto Cleanup;
            }
        }

        ASSERT(pConn);

        SetProperties(pConn);

    }

    hr          = S_OK;
    *pConnector = pConn;
    pConn       = 0;

Cleanup:
    ::ReleaseInterface(pConn);
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::ReturnConnector(ISoapConnector *pConnector)
//
//  parameters:
//
//  description:
//        Returns connector into the pool after not being used anymore
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::ReturnConnector(ISoapConnector *pConnector)
{
    HRESULT hr = S_OK;
    CClientThreadStore *pStore = 0;

    CHK_ARG(pConnector);
    CHK(RetrieveThreadStore(&pStore, false));
    ASSERT(pStore != 0);
    CHK(pStore->ReturnConnector(pConnector));

    hr = S_OK;

Cleanup:
    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::StoreProperty(BSTR bstrName, VARIANT &value)
//
//  parameters:
//
//  description:
//        Stores property in the property bag.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::StoreProperty(BSTR bstrName, VARIANT &value)
{
    CPropertyEntry *pEntry    = 0;
    CPropertyEntry *pNewEntry = 0;
    HRESULT         hr        = S_OK;
    BSTR            name      = 0;

    CCritSectWrapper csw(&m_PropertyCS);
    CHK (csw.Enter());

    for (pEntry = m_PropertyBag.getHead(); pEntry != 0; pEntry = m_PropertyBag.next(pEntry))
    {
        ASSERT(pEntry->GetName() != 0);

        int compare = wcscmp(bstrName, pEntry->GetName());

        if (compare == 0)       /* equal */
        {
            pEntry->SetValue(value);
            goto Cleanup;
        }
        else if (compare > 0)   /* not found */
        {
            break;
        }
    }

    name      = ::SysAllocString(bstrName);
#ifdef UNDER_CE
    CHK_MEM(name);
#endif 
    pNewEntry = new CPropertyEntry;
#ifdef UNDER_CE
    CHK_MEM(pNewEntry);
#endif

    CHK(pNewEntry->SetName(name));
    name = 0;
    CHK(pNewEntry->SetValue(value));

    if (pEntry)
        m_PropertyBag.InsertBefore(pEntry, pNewEntry);
    else
        m_PropertyBag.InsertTail(pNewEntry);

Cleanup:
#ifdef UNDER_CE
    ::SysFreeString(name);
#else
    if(name)
        ::SysFreeString(name);
#endif 
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::SetProperties(ISoapConnector *pConnector)
//
//  parameters:
//
//  description:
//        Sets properties on a newly created connector
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::SetProperties(ISoapConnector *pConnector)
{
    HRESULT hr = S_OK;

    CPropertyEntry *pEntry    = 0;
    CCritSectWrapper csw(&m_PropertyCS);

    CHK_BOOL(pConnector, E_INVALIDARG);

    CHK (csw.Enter());

    for (pEntry = m_PropertyBag.getHead(); pEntry != 0; pEntry = m_PropertyBag.next(pEntry))
    {
        CHK(pConnector->put_Property(pEntry->GetName(), pEntry->GetValue()));
    }

Cleanup:
    return hr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::VariantGetParam (DISPPARAMS * pdispparams, UINT position,
//                                                  VARIANT * pvarResult)
//
//  parameters:
//
//  description:
//          this fuction is called in the case we are expecting a variant in the dispatch
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::VariantGetParam(
                    DISPPARAMS * pdispparams,
                    UINT position,
                    VARIANT * pvarResult)
{
    HRESULT hr = S_OK;
    VARIANTARG* va;

    CHK_BOOL (position < pdispparams->cArgs, DISP_E_PARAMNOTFOUND);

    // let va point to the variant we care about
    va  = pdispparams->rgvarg;
    va += pdispparams->cArgs -1 - position;

    // might pass a variant by ref pointing to an array
    if ( V_VT(va) == (VT_VARIANT | VT_BYREF) )
        va = V_VARIANTREF(va);

    CHK (VariantCopy(pvarResult, va));

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapClient::ArrayGetParam(DISPPARAMS * pdispparams, UINT position, VARIANT * pvarResult)
//
//  parameters:
//
//  description:
//          this fuction is called in the case we are expecting an array in the dispatch
//          the goal is to take the array reference out of the dispparam and return it
//          in the variant pvarResult
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapClient::ArrayGetParam(
                    DISPPARAMS * pdispparams,
                    UINT position,
                    VARIANT * pvarResult)
{
    HRESULT hr = S_OK;
    SAFEARRAY * psa;
    SAFEARRAY * psaCopy;
    VARIANTARG* va;

    CHK_BOOL (position < pdispparams->cArgs, DISP_E_PARAMNOTFOUND);

    // let va point to the variant we care about
    va  = pdispparams->rgvarg;
    va += pdispparams->cArgs -1 - position;

    // script might pass a variant by ref pointing to an array
    if ( V_VT(va) == (VT_VARIANT | VT_BYREF) )
        va = V_VARIANTREF(va);

    CHK_BOOL (V_ISARRAY(va), DISP_E_TYPEMISMATCH);

    // get the array reference
    if (V_ISBYREF(va))
        psa = *V_ARRAYREF(va);
    else
        psa = V_ARRAY(va);

    CHK (SafeArrayCopy(psa, &psaCopy));
    V_ARRAY(pvarResult) = psaCopy;
    // get the vartype and remove the byref


    V_VT(pvarResult) = V_VT(va) & (~VT_BYREF);

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}


/////////////////////////////////////////////////////////////////////////////
// CAbstrNames
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: int compare_for_CAbstrNamesSortCaseSensitive( const void *arg1, const void *arg2 )
//
//  parameters:
//
//  description:    need this function as the compare routine for qsort inside CAbstrNames
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int compare_for_CAbstrNamesSortCaseSensitive( const void *arg1, const void *arg2 )
{
   return wcscmp( *(WCHAR**)arg1,  *(WCHAR**)arg2 );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: int compare_forCaseSensitiveSearch( const void *arg1, const void *arg2 )
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int compare_forCaseSensitiveSearch( const void *arg1, const void *arg2 )
{
    return wcscmp( (WCHAR*)arg1,  *(WCHAR**)arg2 );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAbstrNames::SetSize(long size)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAbstrNames::SetSize(long size)
{
    HRESULT hr = S_OK;

    Clear();

    m_abstrNames = (WCHAR**) new BYTE[sizeof(WCHAR*) * size];
    CHK_BOOL(m_abstrNames != NULL, E_OUTOFMEMORY);

    memset (m_abstrNames, 0, size * sizeof(WCHAR*));
    m_lSize = size;
    m_lFilled = 0;
Cleanup:
    ASSERT(hr == S_OK);

    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: long CAbstrNames::GetSize(void) const
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
long CAbstrNames::GetSize(void) const
{
    return m_lSize;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAbstrNames::AddName(WCHAR * name)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAbstrNames::AddName(WCHAR * name)
{
    HRESULT hr = S_OK;

    CHK_BOOL(m_lFilled < m_lSize, E_FAIL);
    m_abstrNames[m_lFilled] = name;

    m_lFilled++;

Cleanup:
    ASSERT (hr == S_OK);
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CAbstrNames::Sort(void)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAbstrNames::Sort(void)
{
    qsort( (void *)m_abstrNames, m_lFilled, sizeof( WCHAR* ), compare_for_CAbstrNamesSortCaseSensitive );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CAbstrNames::FindName(WCHAR * name, long *lResult) const
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CAbstrNames::FindName(WCHAR * name, long *lResult) const
{
    WCHAR * pResult;

    ASSERT (name);
    ASSERT (lResult);

    *lResult = -1;      // this would indicate nothing found

    pResult = (WCHAR *) bsearch( name, m_abstrNames, m_lFilled,
                sizeof(WCHAR *), compare_forCaseSensitiveSearch);

    if (pResult)
    {
        // we found a result with a case-sensitive compare - a 100% match
        // we are all set and can return
        *lResult = ((long)pResult - (long) (m_abstrNames)) / sizeof(WCHAR*);
        return S_OK;
    }

    // we did not find it with a case sensitive search, let's try case-insensitive

    long lOffset = 0;
    long lTempResult = -1;
    while (lOffset < m_lFilled)
    {
        if (wcsicmp(name, m_abstrNames[lOffset]) == 0)
        {
            // we have a match
            if (lTempResult != -1)
            {
                // this is the second match we found,
                // we can not distinguish, therefore
                return E_FAIL;
            }
            lTempResult = lOffset;
        }
        lOffset ++;
    }

    // if we got here, we made it through the complete array
    if (lTempResult == -1)
    {
        // no match
        return E_FAIL;
    }

    // return the match
    *lResult = lTempResult;
    return S_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: WCHAR *  CAbstrNames::getName(long lOffset) const
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
WCHAR * CAbstrNames::getName(long lOffset) const
{
    ASSERT (lOffset >= 0);
    ASSERT (lOffset < m_lFilled);
    return m_abstrNames[lOffset];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CAbstrNames::Clear(void)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CAbstrNames::Clear(void)
{
    if (m_abstrNames)
    {
        for (int i = 0; i < m_lSize; i++)
            if (m_abstrNames[i])
                delete [] m_abstrNames[i];
        delete [] m_abstrNames;
        m_abstrNames = NULL;
    }
    m_lSize = 0;
    m_lFilled = 0;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CSoapClient::Reset(void)
//
//  parameters:
//
//  description:
//        used in init to release old references if someone calls INIT twice
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoapClient::Reset(void)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        if (m_pTypeInfoMap)
            delete m_pTypeInfoMap;
        m_pTypeInfoMap = NULL;

        m_abstrNames.Clear();
        m_pSoapClientTypeInfo.Clear();
        m_threadInfo.Shutdown();
        m_pISoapConnFact.Clear();
        m_pIEnumWSDLOps.Clear();
        m_pIWSDLPort.Clear();
        m_pIWSDLService.Clear();
        m_pIWSDLReader.Clear();

        while (! m_PropertyBag.IsEmpty())
        {
            CPropertyEntry *pEntry = m_PropertyBag.RemoveHead();
            delete pEntry;
        }
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...) {}
#endif 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CSoapClient::ClearErrors(void)
//
//  parameters:
//
//  description:
//        Clears errors
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSoapClient::ClearErrors(void)
{
    CClientThreadStore *pStore = 0;

    if (SUCCEEDED(RetrieveThreadStore(&pStore, true)))
    {
        pStore->getFaultInfo()->Reset();
    }

    ::SetErrorInfo(0L, 0);
}

