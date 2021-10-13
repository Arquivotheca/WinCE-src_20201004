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
// File:
//      server.cpp
//
// Contents:
//
//      Implementation of CSoapServer
//
//-----------------------------------------------------------------------------
#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


#include "soaphdr.h"
#include "wsdloper.h"

TYPEINFOIDS(ISOAPServer, MSSOAPLib)

BEGIN_INTERFACE_MAP(CSoapServer)
	ADD_IUNKNOWN(CSoapServer, ISOAPServer)
	ADD_INTERFACE(CSoapServer, ISOAPServer)
	ADD_INTERFACE(CSoapServer, ISupportErrorInfo)
	ADD_INTERFACE(CSoapServer, IDispatch)
END_INTERFACE_MAP(CSoapServer)



// those little helpers are used to avoid the try_catch problems otherwise
CServerFaultInfo * createFaultInfo(void)
{
    return(new CServerFaultInfo());
}


void deleteFaultInfo(CServerFaultInfo *p)
{
    delete p;
}

/////////////////////////////////////////////////////////////////////////////
// CSoapServer

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapServer::Init( BSTR bstrWSDLFile, BSTR bstrWSMLFileSpec)
//
//  parameters:
//
//  description:
//        
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapServer::Init(
    			/* [in] */ BSTR bstrWSDLFile,
    			/* [in] */ BSTR bstrWSMLFileSpec)
{
    HRESULT                     hr = S_OK;
#ifndef UNDER_CE
	VARIANT                     varPropValue;
#endif 
	SET_TRACE_LEVEL(3);
  __try
  {
    // WSDL file name is required
    if (!bstrWSDLFile || !(*bstrWSDLFile))
        return E_INVALIDARG;

    // WSML file is ALSO required on the server
    if (!bstrWSMLFileSpec || !(*bstrWSMLFileSpec))
        return E_INVALIDARG;



    release(&m_pIWSDLReader);

    m_serializerState = enSerializerInit;

	// Instantiate IWSDLReader object and get an interface ptr back.

	CHK(CoCreateInstance(CLSID_WSDLReader, NULL, CLSCTX_INPROC_SERVER, IID_IWSDLReader, (void**)&m_pIWSDLReader));

#ifndef UNDER_CE
	V_VT(&varPropValue) = VT_BOOL;
	V_BOOL(&varPropValue) = VARIANT_TRUE;
	CHK(m_pIWSDLReader->setProperty(_T("ServerHTTPRequest"), varPropValue));
#endif 

	CHK(m_pIWSDLReader->Load(bstrWSDLFile, bstrWSMLFileSpec));


  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    TRACE(("SoapServer - Init(): Exception fired \n"));
  }

Cleanup:
    if (FAILED(hr))
    {
        CServerFaultInfo    *pSoapFaultInfo=0;
        pSoapFaultInfo = createFaultInfo();
        if (pSoapFaultInfo)
        {
            // this will setup the errinfo
           pSoapFaultInfo->FaultMsgFromResourceHr(SOAP_IDS_SERVER, 0, hr,
                NULL, NULL);
        }
        delete pSoapFaultInfo;

        globalResetErrors();
    }
	return hr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapServer::SoapInvoke (VARIANT varInput, IUnknown *pStreamOut, BSTR  bstrSoapAction)
//
//  parameters:
//
//  description:
//        
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapServer::SoapInvoke (/* [in] */ VARIANT varInput, // some interface implementing a stream or similiar
										IUnknown *pStreamOut,
										BSTR  bstrSoapAction)
{
    HRESULT             hr = S_OK;
    VARIANT_BOOL        bSuccess;
    CAutoRefc<IEnumSoapMappers>   pIEnumSoapMaps; 
    CAutoRefc<IWSDLPort>  	       pIWSDLPort;
    CAutoRefc<IWSDLOperation>       pIWSDLOp;
    CAutoRefc<ISoapReader>        pISoapReader; 
    CAutoRefc<ISoapSerializer>      pISoapSerializer;
    CAutoRefc<ISOAPError>         pISoapError;
    CAutoBSTR            bstrEncoding;
    CServerFaultInfo   *pSoapFaultInfo   = 0;    
    TCHAR               *pchEncStyle; 

#ifndef UNDER_CE
    try
    {
#endif 
        TRACE( ("%5d: Entering Server::SoapInvoke\n", GetCurrentThreadId()) );
    	SetErrorInfo(0L, NULL);

        m_serializerState = enSerializerInit;

        // before checking anything else, create the faultinfo and the serializer (if possible)

        pSoapFaultInfo = createFaultInfo();
        CHK_MEM(pSoapFaultInfo);

        CHK_BOOL(pStreamOut, E_INVALIDARG);

        CHK(CoCreateInstance(CLSID_SoapSerializer, NULL, CLSCTX_INPROC_SERVER, IID_ISoapSerializer, (void**)&pISoapSerializer));

        VARIANT vStream;
        VariantInit(&vStream);
        vStream.vt = VT_UNKNOWN;
        V_UNKNOWN(&vStream) = pStreamOut;

        CHK(pISoapSerializer->Init(vStream));

        if (!m_pIWSDLReader)
        {
            globalAddError(SOAP_IDS_SERVER_NOTINITIALIZED, SOAP_IDS_SERVER, hr);
            hr = E_FAIL;
            goto Cleanup;
        }

        // Instantiate SoapReader to read the input message
        CHK(CoCreateInstance(CLSID_SoapReader, NULL,
                              CLSCTX_INPROC_SERVER,
                              IID_ISoapReader,
                              (void**)&pISoapReader));


        // Pass the XMLDOMDocument into SoapReader.
        hr = pISoapReader->load(varInput, bstrSoapAction, &bSuccess);
        if (!bSuccess || FAILED(hr))
        {   // Try to obtain info from the interface, if no SOAP
            // interface, we will construct a generic one later.
            if (hr==S_FALSE)
            {
                // change this to invalid arg
                hr = E_INVALIDARG;
            }

            if (!FAILED(hr))
            {
                hr = CONN_E_BAD_REQUEST;
            }
            else
            {
                globalAddError(SOAP_IDS_SERVER_COULDNOTLOADREQUEST, SOAP_IDS_SERVER, hr);
            }
            goto Cleanup;
        }


        // Pass the SoapReader to WSDLReader.
        CHK(m_pIWSDLReader->ParseRequest(pISoapReader, &pIWSDLPort, &pIWSDLOp));

        pSoapFaultInfo->SetCurrentPort(pIWSDLPort);

        CHK(pIWSDLOp->get_preferredEncoding(&bstrEncoding));

        // get the EncodingStyle from the operation
        pchEncStyle = ((CWSDLOperation*)(IWSDLOperation*)pIWSDLOp)->getEncoding(false); 


        // start the envelope, so that headers can be written
        CHK(pISoapSerializer->startEnvelope((BSTR)g_pwstrEnvPrefix, (BSTR)pchEncStyle, bstrEncoding));
        m_serializerState = enSerializerEnvelope;

        hr = pIWSDLOp->ExecuteOperation(pISoapReader, pISoapSerializer);
        if (FAILED(hr))
        {
            CAutoBSTR bstrAddress; 
            // set the actor correctly
            if (SUCCEEDED(pIWSDLPort->get_address(&bstrAddress)))
            {
                globalSetActor(bstrAddress); 
            }
            CHK(hr);
        }

        // Construct the response message stream
        CHK(ConstructSoapResponse(pSoapFaultInfo, pISoapSerializer, pIWSDLOp, pStreamOut));

#ifndef UNDER_CE
    }
    catch(...)
    {
        hr = E_UNEXPECTED; 
    }
#endif 

Cleanup:

    if (FAILED(hr))
        hr = ConstructGenericSoapErrorMessage(pSoapFaultInfo, pISoapSerializer, 0, 0, hr, NULL, NULL);

    TRACE( ("%5d: Exiting Server::SoapInvoke\n\n", GetCurrentThreadId()) );
    deleteFaultInfo(pSoapFaultInfo);
    return hr;
}


static INVOKE_METHOD rgSoapServerMembers[] =
{
    { L"Init",	            DISPID_SOAPSERVER_INIT           },
    { L"SoapInvoke",        DISPID_SOAPSERVER_SOAPINVOKE     },
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  CSoapServer::GetIDsOfNames( REFIID riid, OLECHAR FAR* FAR* rgszNames, UINT cNames, 
//                                         LCID lcid, DISPID FAR* rgdispid )
//
//  parameters:
//
//  description:
//        
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapServer::GetIDsOfNames( REFIID riid, OLECHAR FAR* FAR* rgszNames, UINT cNames, LCID lcid, DISPID FAR* rgdispid )
{
	HRESULT	hr = S_OK;

    __try
    {
	    // try quick lookup first
	    hr = FindIdsOfNames( rgszNames, cNames, rgSoapServerMembers, NUMELEM(rgSoapServerMembers), lcid, &rgdispid[0] );

	    // not found?
	    if( hr == DISP_E_UNKNOWNNAME )
	    {
		    // try expensive typelib lookup
		    hr = CDispatchImpl<ISOAPServer>::GetIDsOfNames(riid, rgszNames, cNames, lcid, rgdispid);
	    }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        TRACE(("SoapServer - GetIDsOfNames(): Exception fired \n"));
    }

	return hr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapServer::Invoke( DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, 
//                          DISPPARAMS FAR* pdispparams, VARIANT FAR* pvarResult, EXCEPINFO FAR* pexcepinfo,
//                          UINT FAR* puArgErr)
//
//  parameters:
//
//  description:
//        
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapServer::Invoke
	(
	DISPID dispidMember,
	REFIID riid,
	LCID lcid,
	WORD wFlags,
	DISPPARAMS FAR* pdispparams,
	VARIANT FAR* pvarResult,
	EXCEPINFO FAR* pexcepinfo,
	UINT FAR* puArgErr
	)
{
	INVOKE_ARG rgArgs[4];
	UINT cArgs = 0;
	DISPID dispid = -1;
	HRESULT hrErr;

    __try
    {
	    //check if pdispparams is NULL.
	    //if NULL is passed for pdispparams...
#ifndef UNDER_CE
	    if (pdispparams == NULL)
		    JMP_ERR((hrErr=E_INVALIDARG),errExit);

	    //NOTE: Adding this check requires that a non-null be passed even in the case of DISPATCH_PROPERTYGET.
	    dispid = (pdispparams->cNamedArgs && !(wFlags & (DISPATCH_PROPERTYPUT)))
		    ? -1 : dispidMember;
#else
    	if (pdispparams == NULL)
        {
            hrErr = E_INVALIDARG;          
		    JMP_ERR(1,errExit);
        }
        else
        {
	    //NOTE: Adding this check requires that a non-null be passed even in the case of DISPATCH_PROPERTYGET.
	    dispid = (pdispparams->cNamedArgs && !(wFlags & (DISPATCH_PROPERTYPUT)))
		    ? -1 : dispidMember;
        }
#endif


	    switch( dispid )
	    {
		    case DISPID_SOAPSERVER_INIT:
		    {
        	    static const VARTYPE rgTypes[] = { VT_BSTR, VT_BSTR };
			    cArgs = NUMELEM(rgTypes);

			    JMP_FAIL(hrErr = FailedError(PrepareInvokeArgs( pdispparams, rgArgs, rgTypes, cArgs ),
				    pexcepinfo), errExit );

			    hrErr =  FailedError(Init(VARMEMBER(&rgArgs[0].vArg, bstrVal),
			                    VARMEMBER(&rgArgs[1].vArg, bstrVal)), pexcepinfo );
			    break;
		    }
		    case DISPID_SOAPSERVER_SOAPINVOKE:
		    {
			    static const VARTYPE rgTypes[] = {VT_VARIANT, VT_UNKNOWN, VT_BSTR|VT_OPTIONAL};
			    cArgs = NUMELEM(rgTypes);

			    JMP_FAIL(hrErr = FailedError(PrepareInvokeArgs( pdispparams, rgArgs, rgTypes, cArgs ),
				    pexcepinfo), errExit );

			    hrErr =  FailedError(SoapInvoke(rgArgs[0].vArg, VARMEMBER(&rgArgs[1].vArg, punkVal), VARMEMBER(&rgArgs[2].vArg, bstrVal) ), pexcepinfo);
		        break;
		    }
		    default:
			    hrErr =  CDispatchImpl<ISOAPServer>::Invoke(
				    dispidMember,
				    riid,
				    lcid,
				    wFlags,
				    pdispparams,
				    pvarResult,
				    pexcepinfo,
				    puArgErr
				    ) ;
			    if( pexcepinfo && pexcepinfo->scode )
				    hrErr = FailedError( pexcepinfo->scode, pexcepinfo );
	    }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        TRACE(("SoapServer - Invoke(): Exception fired \n"));
    }

errExit:
	if( cArgs )
		ClearInvokeArgs( rgArgs, cArgs );

	return hrErr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  CSoapServer::InterfaceSupportsErrorInfo(REFIID riid)
//
//  parameters:
//
//  description:
//        
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapServer::InterfaceSupportsErrorInfo(REFIID riid)
{
	if (InlineIsEqualGUID(IID_ISOAPServer,riid))
			return S_OK;
	return S_FALSE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  CSoapServer::ConstructSoapResponse (CServerFaultInfo   *pSoapFaultInfo, ISoapSerializer *pISoapSerializer,
//                                                 IWSDLOperation *pIWSDLOperation, IUnknown  *pStreamOut)
//
//  parameters:
//
//  description:
//        
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapServer::ConstructSoapResponse(
                CServerFaultInfo   *pSoapFaultInfo,
                ISoapSerializer *pISoapSerializer,
                IWSDLOperation *pIWSDLOperation,
                IUnknown	   *pStreamOut)
{
    HRESULT     hr = S_OK;
    CAutoRefc<ISOAPError>  pISoapErr;
    CAutoRefc<IErrorInfo>  pIErrInfo;

    CHK(pISoapSerializer->startBody(/* BSTR enc_style_uri */ NULL));
    m_serializerState = enSerializerBody;;

    // Operation was successful
    // we will not arrive here if an error happened before
    CHK(pIWSDLOperation->Save(pISoapSerializer, VARIANT_FALSE));
    hr = S_OK;

Cleanup:
    if (SUCCEEDED(hr))
    {
        // End of Soap body element
        CHK(pISoapSerializer->endBody());
        // Finally end the envelope
        CHK(pISoapSerializer->endEnvelope());
    }
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  CSoapServer::ConstructGenericSoapErrorMessage(CServerFaultInfo   *pSoapFaultInfo,
//                              ISoapSerializer *pISoapSerializer, DWORD   dwFaultcodeId, DWORD   MsgId,
//                              HRESULT hrRes, WCHAR * pwstrDetail,va_list*Arguments)
//  parameters:
//
//  description:
//        
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CSoapServer::ConstructGenericSoapErrorMessage(
                CServerFaultInfo   *pSoapFaultInfo,
                ISoapSerializer *pISoapSerializer,
                DWORD   dwFaultcodeId,
                DWORD   MsgId,
                HRESULT hrRes,
                WCHAR * pwstrDetail,
                va_list*Arguments)
{
    HRESULT     hr = S_OK;
    CMemWStr                tempbuf;
#ifndef UNDER_CE
    UINT                    buflen;
#endif 

    CHK_BOOL(pSoapFaultInfo && pISoapSerializer, E_FAIL);

#ifndef UNDER_CE
    if (hrRes)
#else
    if(S_OK != hrRes)
#endif 
    {
        if (MsgId == 0)
        {
            // Find a MsgId that most closely relates to the error
            MsgId = HrToMsgId(hrRes);
        }
        CHK(pSoapFaultInfo->FaultMsgFromResHr(dwFaultcodeId,
                MsgId, hrRes, Arguments));
    }
    else
    {
        if (MsgId == 0)
            MsgId = SOAP_IDS_UNSPECIFIED_ERR;
        CHK(pSoapFaultInfo->FaultMsgFromRes(dwFaultcodeId,
                MsgId, NULL, Arguments));
    }

    if (m_serializerState == enSerializerInit)
    {
        CHK(pISoapSerializer->startEnvelope((BSTR)g_pwstrEnvPrefix, (BSTR)g_pwstrEncStyleNS, L""));
        m_serializerState = enSerializerEnvelope;
    }
    if (m_serializerState == enSerializerEnvelope)
    {
        CHK(pISoapSerializer->startBody(/* BSTR enc_style_uri */ NULL));
        m_serializerState = enSerializerBody;
    }
  	CHK(pISoapSerializer->startFault(pSoapFaultInfo->getfaultcode(),
                    pSoapFaultInfo->getfaultstring(),
                    pSoapFaultInfo->getfaultactor(), 
                    pSoapFaultInfo->getNamespace()));

    // Get the detail info here
    if (pSoapFaultInfo->getdetail())
    {
        CHK(pISoapSerializer->startFaultDetail( ));
        if (pSoapFaultInfo->hasISoapError())
        {
            CHK(pISoapSerializer->writeXML(pSoapFaultInfo->getdetail()));
        }
        else
        {
            CHK(pSoapFaultInfo->writeDetailTree(pISoapSerializer));
        }    
        CHK(pISoapSerializer->endFaultDetail());
    }
    CHK(pISoapSerializer->endFault());
    // Reset the fault info, since we have used this one up.
    pSoapFaultInfo->Reset();

    // now reset the global error list
    globalResetErrors();

    


Cleanup:
    if (SUCCEEDED(hr))
    {
        // End of Soap body element
        CHK(pISoapSerializer->endBody());
        // Finally end the envelope
        CHK(pISoapSerializer->endEnvelope());
    }
    ASSERT(hr == S_OK);
    return (hr);
}


/////////////////////////////////////////////////////////////////////////////
// CServerFaultInfo

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CServerFaultInfo::FaultMsgFromRes ( DWORD   dwFaultCodeId, DWORD   dwResourceId, 
//                                                WCHAR * pwstrdetail, va_list *Arguments)
//  parameters:
//
//  description:
//        
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CServerFaultInfo::FaultMsgFromRes
    (
        DWORD   dwFaultCodeId,  // SOAP_IDS_SERVER(default)/SOAP_IDS_MUSTUNDERSTAND/SOAP_IDS_VERSIONMISMATCH
        DWORD   dwResourceId,   // Resource id for the fault string
        WCHAR * pwstrdetail,    // detail part (optional)
       	va_list	*Arguments		// Arguments to be passed to the resource string
    )
{
    CMemBSTR        bstractor;

    if (m_bhasFault)
    {
        // We already have a fault message here
        return S_OK;
    }

    if (dwFaultCodeId == 0)
        dwFaultCodeId = SOAP_IDS_SERVER;
    else
        ASSERT(dwFaultCodeId != SOAP_IDS_CLIENT);

    // Fill in the CFaultInfo members
	if (m_pIWSDLPort && FAILED(m_pIWSDLPort->get_address(&bstractor)))
	{
	    return E_FAIL;
	}
	return  CFaultInfo::FaultMsgFromResourceString(
                dwFaultCodeId,
                dwResourceId,
                (WCHAR *) pwstrdetail,
                (WCHAR *) bstractor.get(),
       	        Arguments);

}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CServerFaultInfo::FaultMsgFromResHr (DWORD dwFaultCodeId, DWORD dwResourceId,
//                                                 HRESULT hrErr, va_list *Arguments)
//  parameters:
//
//  description:
//        
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CServerFaultInfo::FaultMsgFromResHr
    (
        DWORD   dwFaultCodeId,
        DWORD   dwResourceId,
        HRESULT hrErr,
       	va_list	*Arguments
    )
{
    CMemBSTR        bstractor;

    if (m_bhasFault)
    {
        // We already have a fault message here
        return S_OK;
    }

    if (hrErr == WSDL_MUSTUNDERSTAND)
        dwFaultCodeId = SOAP_IDS_MUSTUNDERSTAND;

    if (dwFaultCodeId == 0)
    {
        dwFaultCodeId = SOAP_IDS_SERVER;
    }
    else
        ASSERT(dwFaultCodeId != SOAP_IDS_CLIENT);

    // Fill in the CFaultInfo members
	if (m_pIWSDLPort && FAILED(m_pIWSDLPort->get_address(&bstractor)))
	{
	    return E_FAIL;
	}

    return  CFaultInfo::FaultMsgFromResourceHr (
                dwFaultCodeId,
                dwResourceId,
                hrErr,
                (WCHAR *)bstractor.get(),
                Arguments);

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:     HRESULT CServerFaultInfo::writeDetailTree(ISoapSerializer *pSerializer)
//
//  parameters:
//
//  description:
//      this will persist the detail information in a namespace declared subtree inside
//      the fault detail
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CServerFaultInfo::writeDetailTree(ISoapSerializer *pSerializer)
{
    HRESULT hr = S_OK;
    CErrorListEntry *pErrorList;

    TCHAR               achBuffer[ONEK+1];



    CHK(pSerializer->startElement((BSTR)g_pwstrErrorInfoElement, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
    CHK(pSerializer->SoapNamespace((BSTR)g_pwstrErrorNSPrefix, (BSTR)g_pwstrMSErrorNS));

    CHK(pSerializer->startElement((BSTR)g_pwstrErrorReturnHR,  0, 0, (BSTR)g_pwstrErrorNSPrefix));
    getReturnCodeAsString(achBuffer, 0);
    CHK(pSerializer->writeString(achBuffer));
    CHK(pSerializer->endElement());

    // before we start the callstack, we write out the errorinfo info, assuming there is some
    globalGetError(&pErrorList);

    if (pErrorList && pErrorList->Size() > 0)
    {
        CHK(saveErrorInfo(pSerializer, pErrorList));
        CHK(pSerializer->startElement((BSTR) g_pwstrErrorCallstack, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
        CHK(saveFaultDetail(pSerializer, pErrorList));
    }
    else
    {
        // if we don't have a global error list, just persist the detail string information
        CHK(pSerializer->startElement((BSTR) g_pwstrErrorCallstack, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
        CHK(pSerializer->startElement((BSTR) g_pwstrErrorCallElement, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
        CHK(pSerializer->startElement((BSTR) g_pwstrErrorDescription, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
        CHK(pSerializer->writeString(getdetail()));
        CHK(pSerializer->endElement());
        CHK(pSerializer->endElement());
    }
    // close the callstack
    CHK(pSerializer->endElement());
    // close the errorinfo
    CHK(pSerializer->endElement());

Cleanup:

    ASSERT(SUCCEEDED(hr));
    return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CServerFaultInfo::saveFaultDetail(ISoapSerializer *pSerializer, CErrorListEntry *pErrorList)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CServerFaultInfo::saveFaultDetail(ISoapSerializer *pSerializer, CErrorListEntry *pErrorList)
{
    HRESULT hr = S_OK;
    UINT                uiSize;
    CErrorDescription   *pError;
    TCHAR               achBuffer[ONEK+1];

    uiSize = pErrorList->Size();

    for (int i=0; i < (int)uiSize;i++ )
    {
        pError = pErrorList->GetAt(i);
        if (wcslen(pError->GetDescription())>0)
        {
            CHK(pSerializer->startElement((BSTR) g_pwstrErrorCallElement, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
            CHK(pSerializer->startElement((BSTR) g_pwstrErrorComponent, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
            CHK(pSerializer->writeString(pError->GetComponent()));
            CHK(pSerializer->endElement());

            CHK(pSerializer->startElement((BSTR)g_pwstrErrorDescription, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
            CHK(pSerializer->writeString(pError->GetDescription()));
            CHK(pSerializer->endElement());

            CHK(pSerializer->startElement((BSTR) g_pwstrErrorReturnHR, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
            getReturnCodeAsString(achBuffer, pError);
            CHK(pSerializer->writeString(achBuffer));
            CHK(pSerializer->endElement());
            CHK(pSerializer->endElement());
        }
    }


Cleanup:
    ASSERT(hr==S_OK);
    return (hr);

}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CServerFaultInfo::saveErrorInfo(ISoapSerializer *pSerializer, CErrorListEntry *pErrorList)
//
//  parameters:
//
//  description:
//      if the errorlist holds on to an ErrorInfo structure, save it
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CServerFaultInfo::saveErrorInfo(ISoapSerializer *pSerializer, CErrorListEntry *pErrorList)
{
    HRESULT hr;
    CAutoBSTR bstrDescription;
    CAutoBSTR bstrHelpFile;
    CAutoBSTR bstrSource;
    DWORD    dwHelpContext;
    HRESULT   hrFromSource;
    TCHAR     achBuffer[ONEK];

    // returns S_FALSE if no valid errorinfo was stored
    hr = pErrorList->GetErrorInfo(&bstrDescription, &bstrSource, &bstrHelpFile, &dwHelpContext, &hrFromSource);

    if (hr==S_OK)
    {
        CHK(pSerializer->startElement((BSTR) g_pwstrErrorServerElement, 0, 0, (BSTR)g_pwstrErrorNSPrefix));

        if(bstrDescription)
        {
            CHK(pSerializer->startElement((BSTR) g_pwstrErrorDescription, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
            CHK(pSerializer->writeString(bstrDescription));
            CHK(pSerializer->endElement());
        }
        if (bstrSource)
        {
            CHK(pSerializer->startElement((BSTR) g_pwstrErrorSource, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
            CHK(pSerializer->writeString(bstrSource));
            CHK(pSerializer->endElement());
        }

        if (bstrHelpFile)
        {
            CHK(pSerializer->startElement((BSTR) g_pwstrErrorHelpFile, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
            CHK(pSerializer->writeString(bstrHelpFile));
            CHK(pSerializer->endElement());

            CHK(pSerializer->startElement((BSTR) g_pwstrErrorHelpContext, 0, 0, (BSTR)g_pwstrErrorNSPrefix));
            swprintf(achBuffer, _T("%ld"), dwHelpContext);
            CHK(pSerializer->writeString(achBuffer));
            CHK(pSerializer->endElement());
        }
        CHK(pSerializer->endElement());
    }
    hr = S_OK;



Cleanup:
    return hr;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: TCHAR * CServerFaultInfo::getReturnCodeAsString(TCHAR *pchBuffer, CErrorDescription *pError)
//
//  parameters:
//
//  description:
//      if no error description is passed in,
//          checks if there is one and returns top of stack
//          or returns hrError
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CServerFaultInfo::getReturnCodeAsString(TCHAR *pchBuffer, CErrorDescription *pError)
{
    HRESULT hrError;

    hrError = m_hrError;

    if (!pError)
    {
        CErrorListEntry     *pErrorList;

        globalGetError(&pErrorList);
        if (pErrorList)
        {
            pErrorList->GetReturnCode(&hrError);
        }
    }
    else
    {
        hrError = pError->GetErrorCode();
    }

    swprintf(pchBuffer, L"%ld", hrError);

}


