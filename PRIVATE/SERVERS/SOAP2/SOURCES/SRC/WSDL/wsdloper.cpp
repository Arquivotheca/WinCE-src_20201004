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
// File:    wsdloper.cpp
//
// Contents:
//
//  implementation file
//
//        IWSDLReader Interface implemenation
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "wsdloper.h"
#include "soapmapr.h"
#include "ensoapmp.h"
#include "xsdpars.h"

BEGIN_INTERFACE_MAP(CWSDLOperation)
    ADD_IUNKNOWN(CWSDLOperation, IWSDLOperation)
    ADD_INTERFACE(CWSDLOperation, IWSDLOperation)
END_INTERFACE_MAP(CWSDLOperation)


/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLOperation::CWSDLOperation()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLOperation::CWSDLOperation()
{
    memset(this, 0, sizeof(CWSDLOperation));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLOperation::~CWSDLOperation()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLOperation::~CWSDLOperation()
{

    TRACEL((3, "Operation shutdown %S\n", m_bstrName));

    delete m_pWSDLInputMessage;
    delete m_pWSDLOutputMessage;
    delete [] m_ppchparameterOrder;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLOperation::Shutdown(void)
//
//  parameters:
//
//  description:
//        called by parent when he is destroyed
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void CWSDLOperation::Shutdown(void)
{
    CAutoRefc<CSoapMapper>     pSoapMapper=0;
    DWORD                  dwCookie;

    // before we shut down, we need to tell all mappers to release
    // their soaptypemapper interfaces, if they have any...

    dwCookie = 0;
    while(m_pEnumSoapMappers->getNext((ISoapMapper**)&pSoapMapper, &dwCookie)==S_OK)
    {
        pSoapMapper->Shutdown();
        release(&pSoapMapper);
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::get_name(BSTR *pbstrOperationName)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::get_name(BSTR *pbstrOperationName)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrOperationName , m_bstrName);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::get_style(BSTR *pbstrStyle)
//
//  parameters:
//
//  description:
//      returns the style attribute for the operation
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::get_style(BSTR *pbstrStyle)
{
    return(_WSDLUtilGetStyleString(pbstrStyle, m_fDocumentMode));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: const WCHAR *CWSDLOperation::get_nameRef()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
const WCHAR * CWSDLOperation::getNameRef()
{
    return (WCHAR *) m_bstrName;;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::get_documentation(BSTR *bstrServicedocumentation)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::get_documentation(BSTR *bstrServicedocumentation)
{
    return _WSDLUtilReturnAutomationBSTR(bstrServicedocumentation, m_bstrDocumentation);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::get_soapAction(BSTR *pbstrSoapAction)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::get_soapAction(BSTR *pbstrSoapAction)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrSoapAction, m_bstrSoapAction);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: BOOL CWSDLOperation::compareSoapAction(TCHAR *pchSoapActionToCompare)
//
//  parameters:
//
//  description:
//      compares a passed in string with the soapaction property.
//  returns:
//      true if matches
/////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CWSDLOperation::compareSoapAction(TCHAR *pchSoapActionToCompare)
{
    if (pchSoapActionToCompare)
    {
        return(wcscmp(pchSoapActionToCompare, m_bstrSoapAction)==0);
    }
    return(FALSE);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::GetOperationParts(IEnumSoapMappers * ppIEnumSoapMappers)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::GetOperationParts(IEnumSoapMappers **ppIEnumSoapMappers)
{
    ASSERT(m_pEnumSoapMappers!=0);

    if (ppIEnumSoapMappers==0)
        return(E_INVALIDARG);

    return(m_pEnumSoapMappers->Clone(ppIEnumSoapMappers));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



#ifdef CE_NO_EXCEPTIONS
HRESULT       
SafeInvoke(IDispatch *pD, BOOL *fException,
            DISPID dispIdMember, 
            REFIID riid, LCID lcid, 
            WORD wFlags, 
            DISPPARAMS FAR *pDispParams, 
            VARIANT FAR *pVarResult, 
            EXCEPINFO FAR *pExcepInfo, 
            unsigned int FAR *puArgErr)
{
    HRESULT hr;
    __try{
            *fException = FALSE;
        hr = pD->Invoke(dispIdMember,
                    riid,
                    lcid,
                    wFlags,
                    pDispParams,
                    pVarResult,
                    pExcepInfo,
                    puArgErr);
        return hr;
    }
    __except(1){
        *fException = TRUE;
        return E_FAIL;
    }
}
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::ExecuteOperation(ISoapReader *pISoapReader, ISoapSerializer *pISoapSerializer)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::ExecuteOperation(ISoapReader *pISoapReader, ISoapSerializer *pISoapSerializer)
{
    HRESULT         hr = E_INVALIDARG;
    DISPID            dispid;
#ifndef UNDER_CE
    long            lRes;
#endif 
    DISPPARAMS         dispparams;
    VARIANTARG        *pvarg=0;
    VARIANT            *pvBuffer=0;
    VARIANT            varRes;
    EXCEPINFO         exceptInfo;
    unsigned int     uArgErr;
    smIsInputEnum    enInput;
    LPOLESTR        p;
    LONG            lCallIndex;
    LONG            lVarType;
    DWORD           dwCookie;

    CAutoRefc<ISoapMapper>     pSoapMapper=0;
    CAutoRefc<IDispatch>     pDispatch=0;
    CAutoRefc<IHeaderHandler> pHeaderHandler=0;

    CSoapHeaders            soapHeaders;



    VariantInit(&varRes);
    memset(&dispparams, 0, sizeof dispparams);
    memset(&exceptInfo, 0, sizeof(EXCEPINFO));


    if (!m_pDispatchHolder)
        goto Cleanup;

    hr = m_pDispatchHolder->GetDispatchPointer(&pDispatch);
    ERROR_ONHR1( hr, WSDL_IDS_OPERNOOBJECT, WSDL_IDS_OPERATION, m_bstrMethodName);

    // do header preprocessing
    if (m_pHeaderHandler)
    {
        hr = m_pHeaderHandler->GetHeaderPointer(&pHeaderHandler);
        ERROR_ONHR1( hr, WSDL_IDS_OPERNOHEADERHANDLER, WSDL_IDS_OPERATION, m_bstrMethodName);
    }


    // init the helper class
    CHK(soapHeaders.Init());
    CHK(soapHeaders.AddHeaderHandler(pHeaderHandler));


    hr = soapHeaders.ReadHeaders(pISoapReader , pDispatch);
    ERROR_ONHR1( hr, WSDL_IDS_OPERREADHEADERSFAILED, WSDL_IDS_OPERATION, m_bstrMethodName);

    if (m_dispID == DISPID_UNKNOWN)
    {
        p = m_bstrMethodName;
        // dispid was not provided in the WSML file
        hr  = pDispatch->GetIDsOfNames(IID_NULL, &p,1,LOCALE_SYSTEM_DEFAULT, &dispid);
        ERROR_ONHR1( hr, WSDL_IDS_OPERNODISPIDFOUND, WSDL_IDS_OPERATION, m_bstrMethodName);
    }
    else
    {
        dispid = m_dispID;
    }


    // now build the dispparam array

    CHK (m_pEnumSoapMappers->CountOfArguments((long*)&(dispparams.cArgs)));

    if (dispparams.cArgs > 0)
    {
        pvarg = new VARIANTARG[dispparams.cArgs];
        pvBuffer = new VARIANT[dispparams.cArgs];

        CHK_BOOL ( pvarg && pvBuffer, E_OUTOFMEMORY);

        dispparams.rgvarg = pvarg;
        memset(pvarg, 0, sizeof(VARIANTARG) * dispparams.cArgs);
        memset(pvBuffer, 0, sizeof(VARIANT) * dispparams.cArgs);

        // everything setup, now walk the enum to set those guys up. Fill them backwards (parameter passing is
        // in reversed order
        dwCookie = 0;
        while ((hr = m_pEnumSoapMappers->getNext(&pSoapMapper, &dwCookie))==S_OK)
        {
            CHK ( pSoapMapper->get_callIndex(&lCallIndex) );

            // now check where the guy goes to
            if (lCallIndex != -1)
            {

                // that indicates this is the return value, so ignore him
                // remember opposite order for the position;
                lCallIndex = dispparams.cArgs - lCallIndex;

                VARIANT* _pvarg = &(pvarg[lCallIndex]);

                CHK ( pSoapMapper->get_comValue(_pvarg) );

                CHK(pSoapMapper->get_isInput(&enInput));
                CHK(pSoapMapper->get_variantType(&lVarType));
                if (enInput != smInput)
                {
                    // do not create a byref handle in case of a safearray | dispatch
//                    if ((VT_ARRAY | VT_DISPATCH) != V_VT(_pvarg))
                    {
                        // now this indicates by ref for us
                        // take one of the buffer guys and use him

                        VARIANT* _pvBuffer = &(pvBuffer[lCallIndex]);

                        VariantInit(_pvBuffer);
                        memcpy(_pvBuffer, _pvarg, sizeof(VARIANT));
                           V_VT(_pvarg) = V_VT(_pvBuffer) | VT_BYREF;

                        if (lVarType == (LONG) VT_DECIMAL)
                        {
                            V_DECIMALREF(_pvarg) = &V_DECIMAL(_pvBuffer);
                        }
                        else
                        {
                            if (lVarType == (LONG) VT_VARIANT)
                            {
                                   V_VT(_pvarg) = VT_VARIANT | VT_BYREF;
                                   V_VARIANTREF(_pvarg) = _pvBuffer;
                                   if (V_VT(_pvBuffer) == VT_VARIANT)
                                   {
                                        // in the case of a '[out] Variant *' parameter:
                                        // we should not point to a variant instance since we can not marshall this
                                        V_VT(_pvBuffer) = VT_EMPTY;
                                   }
                            }
                            else
                            {
                                pvarg[lCallIndex].pulVal = &(pvBuffer[lCallIndex].ulVal);
                            }
                        }
                    }
                }


            }
            release(&pSoapMapper);
        }
    }
   
#ifdef CE_NO_EXCEPTIONS
    {
    BOOL fException;
    hr = SafeInvoke(pDispatch, 
                    &fException,
                    dispid,
                    IID_NULL,
                    LOCALE_SYSTEM_DEFAULT,
                    DISPATCH_METHOD,
                    &dispparams,
                    &varRes,
                    &exceptInfo,
                    &uArgErr);

    if(fException)
    {
        hr = E_FAIL;
        globalAddError(WSDL_IDS_OPERCAUSEDEXECPTION, WSDL_IDS_OPERATION, hr, m_bstrMethodName);                                
        TRACEL((1, "\nInvoke of serverobject failed !!!\n\n"));
    }
    }
#else
    try
    {
    hr = pDispatch->Invoke(dispid,
                    IID_NULL,
                    LOCALE_SYSTEM_DEFAULT,
                    DISPATCH_METHOD,
                    &dispparams,
                    &varRes,
                    &exceptInfo,
                    &uArgErr);
    }
    catch(...)
    {
        hr = E_FAIL;
        globalAddError(WSDL_IDS_OPERCAUSEDEXECPTION, WSDL_IDS_OPERATION, hr, m_bstrMethodName);
        TRACEL((1, "\nInvoke of serverobject failed !!!\n\n"));
    }
#endif

    if (FAILED(hr))
    {
        setupInvokeError(pDispatch, &exceptInfo, dispparams.cArgs-uArgErr, hr);
        goto Cleanup;
    }

    // now set the values as they were coming back
    dwCookie = 0;
    while ((hr = m_pEnumSoapMappers->getNext(&pSoapMapper, &dwCookie))==S_OK)
    {
        CHK ( pSoapMapper->get_callIndex(&lCallIndex) );
        if (lCallIndex == -1)
        {
            // take the return variant
            CHK(pSoapMapper->put_comValue(varRes));
        }
        else
        {
            lCallIndex = dispparams.cArgs - lCallIndex;
            if (V_ISBYREF(&(pvarg[lCallIndex])))
            {
                hr = pSoapMapper->put_comValue(pvBuffer[lCallIndex]);
                VariantClear(&(pvBuffer[lCallIndex]));
                if (FAILED(hr))
                {
                    goto Cleanup;
                }
            }
        }
        release(&pSoapMapper);
    }
    if (SUCCEEDED(hr))
    {
        // ignore S_FALSE from the loop above
        hr = S_OK;
    }

    // if we arrived here, all is well. Write headers
    CHK(soapHeaders.WriteHeaders(pISoapSerializer, pDispatch));



Cleanup:

    for (long i=0;i< (long)dispparams.cArgs;i++)
    {
        VariantClear(&(pvarg[i]));
        VariantClear(&(pvBuffer[i]));
    }

    // clear possible contents in excpetioninfo
    ::SysFreeString(exceptInfo.bstrSource);
    ::SysFreeString(exceptInfo.bstrDescription);
    ::SysFreeString(exceptInfo.bstrHelpFile);

    VariantClear(&varRes);
    delete [] pvarg;
    delete [] pvBuffer;
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  HRESULT CWSDLOperation::setupInvokeErrorIDispatch *pDispatch, EXCEPINFO *pxcepInfo, unsigned int uArgErr, HRESULT hrErrorCode)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::setupInvokeError(IDispatch *pDispatch,
                                        EXCEPINFO *pxcepInfo, unsigned int uArgErr,
                                        HRESULT hrErrorCode)
{
    HRESULT hr =            S_OK;
    CAutoRefc<ISoapMapper>     pSoapMapper=0;
    LONG                    lCallIndex;
#ifndef UNDER_CE
    LONG                    lRes;
#endif 
    CAutoBSTR               bstrName;
    DWORD                  dwCookie;


    // lets  check if the object supported ISoapError. If so, get the SoapError information
    globalAddErrorFromISoapError(pDispatch, hrErrorCode);



    switch (hrErrorCode)
    {
        case DISP_E_BADPARAMCOUNT:
        case DISP_E_PARAMNOTOPTIONAL:
            globalAddError(WSDL_IDS_EXECBADPARAMCOUNT, WSDL_IDS_OPERATION, hrErrorCode, m_bstrMethodName);
            break;
        case DISP_E_BADVARTYPE:
            globalAddError(WSDL_IDS_EXECBADVARTYPE, WSDL_IDS_OPERATION, hrErrorCode, m_bstrMethodName);
            break;
        case DISP_E_EXCEPTION:
            globalAddErrorFromErrorInfo(pxcepInfo, hrErrorCode);
            break;
        case DISP_E_MEMBERNOTFOUND:
            globalAddError(WSDL_IDS_EXECMEMBERNOTFOUND, WSDL_IDS_OPERATION, hrErrorCode, m_bstrMethodName);
            break;
        case DISP_E_TYPEMISMATCH:
        case DISP_E_OVERFLOW:
            // find the right mapper
            dwCookie = 0;
            while ((hr = m_pEnumSoapMappers->getNext(&pSoapMapper, &dwCookie))==S_OK)
            {
                hr = pSoapMapper->get_callIndex(&lCallIndex);
                if (FAILED(hr))
                {
                    goto Cleanup;
                }
                if (lCallIndex == (long)uArgErr)
                {
                    // take the return variant
                    hr = pSoapMapper->get_elementName(&bstrName);
                    if (FAILED(hr))
                    {
                        goto Cleanup;
                    }
                    globalAddError(WSDL_IDS_EXECBADTYPE, WSDL_IDS_OPERATION, hrErrorCode, bstrName);
                    goto Cleanup;
                }
                  release(&pSoapMapper);
            }
            // if we ever got here, than this most likely a problem in the WSML file (parameter number != real numbers)
            globalAddError(WSDL_IDS_EXECBADTYPE, WSDL_IDS_OPERATION, hrErrorCode, _T("unknown"));
            hr = S_OK;

            break;

        default:
            globalAddError(WSDL_IDS_EXECUNKNOWNERROR, WSDL_IDS_OPERATION, hrErrorCode, hrErrorCode);
    }


Cleanup:

    globalAddError(WSDL_IDS_EXECFAILED, WSDL_IDS_OPERATION, hrErrorCode, m_bstrMethodName);

    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::Init(IXMLDOMNode *pOperationNode, ISoapTypeMapperFactory *ptypeFactory,
//		                        TCHAR *pchPortType, BOOL fDocumentMode, BSTR bstrpreferredEncoding, BOOL fCreateHrefs)
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
HRESULT CWSDLOperation::Init(IXMLDOMNode *pOperationNode, ISoapTypeMapperFactory *ptypeFactory,
                        TCHAR *pchPortType, BOOL fDocumentMode,
                        BSTR bstrpreferredEncoding, BOOL fCreateHrefs)
{
    HRESULT                 hr = E_FAIL;
    CAutoRefc<IXMLDOMNode>  pSoapAction=0;
    CAutoRefc<IXMLDOMNode>  pPortInfo=0;
    CAutoFormat              autoFormat;



    ASSERT(pOperationNode!=0);
    ASSERT(pchPortType!=0);

    m_pWSDLInputMessage = new CWSDLMessagePart();
    m_pWSDLOutputMessage = new CWSDLMessagePart();
    m_pEnumSoapMappers = new CSoapObject<CEnumSoapMappers>(INITIAL_REFERENCE);

#ifdef UNDER_CE
    if(!m_pWSDLInputMessage || !m_pWSDLOutputMessage || !m_pEnumSoapMappers)
    {
        if(m_pWSDLInputMessage) 
            delete m_pWSDLInputMessage;
        if(m_pWSDLOutputMessage)
            delete m_pWSDLOutputMessage;
        if(m_pEnumSoapMappers)
            delete m_pEnumSoapMappers;
            
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }
#endif 
    m_fDocumentMode = fDocumentMode;

    m_fCreateHrefs = fCreateHrefs;
    CHK(m_bstrpreferedEncoding.Assign(bstrpreferredEncoding));

    if (!m_pWSDLInputMessage || !m_pWSDLOutputMessage || !m_pEnumSoapMappers || !m_bstrpreferedEncoding)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    hr = _WSDLUtilFindAttribute(pOperationNode, _T("name"), &m_bstrName);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_OPERNONAME, WSDL_IDS_OPERATION, hr);
        goto Cleanup;
    }

       CHK(_WSDLUtilFindExtensionInfo(pOperationNode, 0, &m_fCreateHrefs));

    CHK(_WSDLUtilFindDocumentation(pOperationNode, &m_bstrDocumentation));


    TRACEL((3, "Operation Init %S\n", m_bstrName));
    // now get the soapaction

    hr = pOperationNode->selectSingleNode(_T("./soap:operation"), &pSoapAction);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_OPERNOSOAPOPER, WSDL_IDS_OPERATION, hr, m_bstrName);
        goto Cleanup;
    }

    hr = _WSDLUtilFindAttribute(pSoapAction, _T("soapAction"), &m_bstrSoapAction);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_OPERNOSOAPACTION, WSDL_IDS_OPERATION, hr, m_bstrName);
        goto Cleanup;
    }
    CHK(autoFormat.sprintf(_T("//def:portType[@name=\"%s\"]/def:operation[@name=\"%s\"]"), pchPortType, m_bstrName));
    hr = _XPATHUtilFindNodeFromRoot(pOperationNode, &autoFormat, &pPortInfo);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_OPERNOTFOUNDINPORT, WSDL_IDS_OPERATION, hr, &autoFormat);
        goto Cleanup;
    }



    // this does not have to be there, it's optional
    if (SUCCEEDED(_WSDLUtilFindAttribute(pPortInfo, _T("parameterOrder"), &m_bstrparameterOrder)))
    {
        prepareOrderString();
    }

    // let's see if style was overwritten, ignore HR, as this is a valid to be absent

    _WSDLUtilGetStyle(pSoapAction,&m_fDocumentMode);

    hr = m_pWSDLInputMessage->Init(pOperationNode, ptypeFactory, pchPortType, _T("input"), m_bstrName, this);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_OPERINITINPUTFA, WSDL_IDS_OPERATION, hr, m_bstrName);
        goto Cleanup;
    }

    hr = m_pWSDLOutputMessage->Init(pOperationNode, ptypeFactory, pchPortType, _T("output"), m_bstrName, this);
    if (FAILED(hr))
    {
        if (hr != E_NOINTERFACE)
        {
            globalAddError(WSDL_IDS_OPERINITOUTPUTFA, WSDL_IDS_OPERATION, hr, m_bstrName);
            goto Cleanup;
        }
    }
    // if the input/output messages return S_FALSE, that indicates that those messageparts were not
    // speced for SOAPbinding, but something else. ignore that, as we can not be called on those
    hr = S_OK;



Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::prepareOrderString(void)
//
//  parameters:
//
//  description:
//      takes the parameterOrder attribute and strtoks it.
//      while doing so it creates an array of pointers to the substrings
//  returns:
//      S_OK or E_OUTOFMEMORY
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::prepareOrderString(void)
{
    TCHAR *pchToken;
    ULONG   ulOrder = 0;
    ULONG   ulCount = 16;
    HRESULT hr = S_OK;

    delete [] m_ppchparameterOrder;
    m_cbparaOrder = 0;

    m_ppchparameterOrder = new TCHAR*[16];
    if (!m_ppchparameterOrder)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    memset(m_ppchparameterOrder, 0, sizeof(TCHAR*));

    if (m_bstrparameterOrder && m_bstrparameterOrder.Len() > 0)
    {
        pchToken = _tcstok(m_bstrparameterOrder, _T(" "));
        while (pchToken)
        {
            if (ulOrder >= ulCount)
            {
                // need to realloc that array
                ulCount += 16;
#ifndef UNDER_CE
                m_ppchparameterOrder = (TCHAR**)realloc(m_ppchparameterOrder, sizeof(TCHAR**)*ulCount);
                if (!m_ppchparameterOrder)
                {
                    hr = E_OUTOFMEMORY;
                    goto Cleanup;
                }
#else //change makes sure we dont leak if realloc fails
                TCHAR **ppTemp = (TCHAR**)realloc(m_ppchparameterOrder, sizeof(TCHAR**)*ulCount);
               
                if (!ppTemp)
                {
                    delete [] m_ppchparameterOrder;
                    m_ppchparameterOrder = NULL;
                    hr = E_OUTOFMEMORY;
                    goto Cleanup;
                }                
                m_ppchparameterOrder = ppTemp;
#endif 
            }

            m_ppchparameterOrder[ulOrder] = pchToken;
            pchToken = _tcstok(0, _T(" "));
            ulOrder++;
            m_cbparaOrder = ulOrder;
        }
    }
Cleanup:
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::AddMapper(CSoapMapper *pSoapMapper, CWSDLMessagePart *pMessage, bool fInput, long lOrder)
//
//  parameters:
//
//  description:
//      does several things
//          a) add the mapper to the collection
//          b) if in/out set the original mapper to in/out type and update the saveorder for output
//          c) set's the parameter order of the mapper if parameterOrder is set in the wsdl
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::AddMapper(CSoapMapper *pSoapMapper, CWSDLMessagePart *pMessage, bool fInput, long lOrder)
{
    HRESULT         hr=S_OK;
    CAutoRefc<ISoapMapper>  pMapper;
#ifndef UNDER_CE
    TCHAR            *pchFound;
#endif 
    long            lparaOrder=0;
#ifndef UNDER_CE
    long            lCurOrder;
    long            ret;
#endif 
    DWORD          dwCookie;
    CEnumSoapMappers *pMappers;

    ASSERT(pSoapMapper!=0);
    ASSERT(m_pEnumSoapMappers!=0);

    // we need to check the parameterOrder attribute to see if the part of that mapper is mentioned there...
    if (m_cbparaOrder > 0)
    {
        for (lparaOrder = 0; lparaOrder < m_cbparaOrder; lparaOrder++)
        {
            if (pSoapMapper->getPartName() && _tcscmp(pSoapMapper->getPartName(), m_ppchparameterOrder[lparaOrder])==0)
            {
                break;
            }
        }
        if (lparaOrder == m_cbparaOrder)
        {
            // not found, make return value
            lparaOrder = -1;
            if (!m_fDocumentMode)
            {
                // in RPC mode, save this guy first
                lOrder = -1 ;
            }
        }
    }
    else
    {
        m_pEnumSoapMappers->parameterCount(&lparaOrder);
    }

    pSoapMapper->setparameterOrder(lparaOrder);

    // the next thing we do is check if that mapper (in case of output message) is already in
    // the list (input message) -> we then don't add it, but make it in/out
    dwCookie = 0;
    while (m_pEnumSoapMappers->getNext(&pMapper, &dwCookie)==S_OK)
    {
        if (wcscmp(((CSoapMapper*)(ISoapMapper*)pMapper)->getVarName(),((CSoapMapper*)pSoapMapper)->getVarName())==0)
        {
            // equal guys, change the old mapper to in/out, forget the new mapper
            ((CSoapMapper*)(ISoapMapper*)pMapper)->setInput(smInOut);
            CHK(pMessage->AddOrdered(pMapper));
            // need to shutdown the original
               pSoapMapper->Shutdown();
            pMapper.Clear();
            goto Cleanup;
        }
        pMapper.Clear();
    }
    // first see if we are adding the same parameterOrder twice
    if (m_cbparaOrder > 0)
    {
        if (m_pEnumSoapMappers->FindParameter(lparaOrder, 0)==S_OK)
        {
            // we already a parameter with this order, so there must be something wrong
            hr = E_INVALIDARG;
            globalAddError(WSDL_IDS_OPERPARAORDERINVALID, WSDL_IDS_OPERATION, hr, m_bstrMethodName);
            goto Cleanup;
        }
    }

    if (isDocumentMode() && pMessage->IsLiteral() && pSoapMapper->isBasedOnType())
    {
        // check if we have ONLY ONE part type= in this mode
        pMappers = pMessage->getMappers();
        dwCookie = 0;
        while (pMappers->getNext(&pMapper, &dwCookie)==S_OK)
        {
            if (((CSoapMapper*)(ISoapMapper*)pMapper)->isBasedOnType())
            {
                hr = E_INVALIDARG;
                globalAddError(WSDL_IDS_OPERMULTIPLEPARTSWITHTYPE, WSDL_IDS_OPERATION, hr, m_bstrMethodName);
                goto Cleanup;
            }
        }
    }


    hr = m_pEnumSoapMappers->Add(pSoapMapper);
    if (FAILED(hr))
        goto Cleanup;


    // so we have no parameterOrder attribute. NOW, if this is the FIRST OUTPUT MAPPER, and it's NOT AN IN/OUT
    //  we want to give him the result signature
    if (m_cbparaOrder == 0 && !fInput && lOrder == 0)
    {
        pSoapMapper->setparameterOrder(-1);
    }

       hr = pMessage->AddOrdered(pSoapMapper);
    if (FAILED(hr))
        goto Cleanup;



Cleanup:
     ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLMessagePart::CWSDLMessagePart()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLMessagePart::CWSDLMessagePart()
{
    memset(this, 0, sizeof(CWSDLMessagePart));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWSDLMessagePart::~CWSDLMessagePart()
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CWSDLMessagePart::~CWSDLMessagePart()
{
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLMessagePart::Init(IXMLDOMNode *pOperationNode, ISoapTypeMapperFactory *ptypeFactory, 
//					TCHAR *pchPortType, TCHAR *pchMessageType, TCHAR *pchOperationName, CWSDLOperation *pParent)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLMessagePart::Init(IXMLDOMNode *pOperationNode, ISoapTypeMapperFactory *ptypeFactory, TCHAR *pchPortType, TCHAR *pchMessageType, TCHAR *pchOperationName, CWSDLOperation *pParent)
{
    HRESULT hr = E_FAIL;

    CAutoBSTR                   bstrPartName;
    CAutoRefc<IXMLDOMNode>         pBody=0;
    CAutoRefc<IXMLDOMNode>         pPortInfo=0;
    CAutoRefc<IXMLDOMNode>         pPart=0;
    CAutoRefc<IXMLDOMNodeList>     pNodeList=0;
    CAutoRefc<IXMLDOMNodeList>     pSubList=0;
    CAutoBSTR                   bstrNSUri;
    CAutoBSTR                   bstrParameterWithPrefix;
    CSoapMapper                    *pSoapMapper=0;
    bool                        fInput=false;
    long                        lOrder=0;
    long                        lLength;

    CAutoFormat                 autoFormat;



    ASSERT(pOperationNode!=0);
    ASSERT(pchPortType != 0);

    // first get the subnode

    m_pEnumMappersToSave = new CSoapObject<CEnumSoapMappers>(INITIAL_REFERENCE);
    if (!m_pEnumMappersToSave)
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    fInput = *pchMessageType==_T('i') ? true : false;

    CHK(autoFormat.sprintf(_T("def:%s/soap:body"), pchMessageType));
    m_fBody = true;
    hr = pOperationNode->selectSingleNode(&autoFormat, &pBody);
    if (hr != S_OK)
    {
        // now maybe this guys is a HEADER definition, so search for that guy
        CHK(autoFormat.sprintf(_T("def:%s/soap:header"), pchMessageType));
        hr = pOperationNode->selectSingleNode(&autoFormat, &pBody);
        m_fBody=false;
    }

    if (hr != S_OK)
    {
//        globalAddError(WSDL_IDS_OPERNOBODYHEADER, WSDL_IDS_OPERATION, hr, pchOperationName);
        // remove the error. the input/output COULD be mime: or any other binding. Check in the operation
        // to remove this message part.
        goto Cleanup;
    }

    {
        CAutoBSTR bstrTemp;

        hr = _WSDLUtilFindAttribute(pBody, _T("use"), &bstrTemp);
        if (FAILED(hr))
        {
            globalAddError(WSDL_IDS_OPERNOUSEATTR, WSDL_IDS_OPERATION, hr, pchOperationName);
            goto Cleanup;
        }

        m_fIsLiteral = (wcscmp(bstrTemp, _T("literal"))==0);
    }


    if (m_fIsLiteral == false)
    {

        if (pParent->isDocumentMode())
        {
            // now we have document/encoded. This is NOT supported, error OUT
            hr = E_FAIL;
            globalAddError(WSDL_IDS_UNSUPPORTEDMODE, WSDL_IDS_OPERATION, hr, pchOperationName);
            goto Cleanup;
        }

        // if use = encoding, we need an encoding style
        hr = _WSDLUtilFindAttribute(pBody, _T("encodingStyle"), &m_bstrEncodingStyle);

        if (FAILED(hr))
        {
            // error. if the style is not literal, it's encoded, hence encodingstyle has to be there
            globalAddError(WSDL_IDS_NOENCODINGSTYLE, WSDL_IDS_OPERATION, hr, pchOperationName);
            goto Cleanup;

        }
        else
        {
            // now check the encodingstyle, only SOAP defined encoding is supported
            if (_tcsncmp(m_bstrEncodingStyle, g_pwstrEncStyleNS,
                        wcslen(g_pwstrEncStyleNS))!=0)
            {
                hr = E_FAIL;
                globalAddError(WSDL_IDS_WRONGENCODINGSTYLE, WSDL_IDS_OPERATION, hr, m_bstrEncodingStyle);
                goto Cleanup;
            }

        }

        hr = _WSDLUtilFindAttribute(pBody, _T("namespace"),  &m_bstrNameSpace);
        if (FAILED(hr) || m_bstrNameSpace.Len() == 0)
        {
            globalAddError(WSDL_IDS_NONONAMSPACE, WSDL_IDS_OPERATION, hr, pchOperationName);
            goto Cleanup;
        }


    }
    else
    {
        // search for the namespace anyway for literal mode, but ignore if not there
        _WSDLUtilFindAttribute(pBody, _T("namespace"),  &m_bstrNameSpace);
        if (m_bstrNameSpace.Len() == 0)
        {
            m_bstrNameSpace.Clear();
        }
    }


    // now we got everything we need OUT of the binding/opertaion node, now we need to find the correct porttype etc

    CHK(autoFormat.sprintf(_T("//def:portType[@name=\"%s\"]/def:operation[@name=\"%s\"]/def:%s"), pchPortType, pchOperationName, pchMessageType));


    hr = _XPATHUtilFindNodeFromRoot(pOperationNode, &autoFormat, &pPortInfo);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_OPERNOTFOUNDINPORT, WSDL_IDS_OPERATION, hr, &autoFormat);
        goto Cleanup;
    }

    {
        CAutoBSTR bstrTemp;

        hr = _WSDLUtilFindAttribute(pPortInfo, _T("message"), &bstrTemp);
        if (FAILED(hr))
        {
            globalAddError(WSDL_IDS_OPERMSGPARTINPORTNF, WSDL_IDS_OPERATION, hr, pchOperationName);
            goto Cleanup;
        }

        _WSDLUtilSplitQName(bstrTemp, 0, &bstrPartName);
    }

    // we will ignore the name in the port for all cases ...
    // we will still have to verify if this is true for document and rpc mode
    if (!fInput)
    {
        CHK(autoFormat.sprintf(_T("%s%s"), pchOperationName, _T("Response")));
        CHK(m_bstrMessageName.Assign(&autoFormat));
    }
    else
    {
        CHK(m_bstrMessageName.Assign(pchOperationName));
    }

    // now we have the message definiton. We need to walk over the parts and get the data out of there

    // first see if the message definition is there, this HAS to be there
    CHK(autoFormat.sprintf(_T("//def:message[@name=\"%s\"]"), bstrPartName));
    hr = _XPATHUtilFindNodeFromRoot(pOperationNode, &autoFormat, &pPart);
    if (FAILED(hr))
    {
        globalAddError(WSDL_IDS_OPERMSGPARTNF, WSDL_IDS_OPERATION, hr, bstrPartName);
        goto Cleanup;
    }
    pPart.Clear();

    CHK(autoFormat.sprintf(_T("//def:message[@name=\"%s\"]/def:part"), bstrPartName));
    hr = _XPATHUtilFindNodeListFromRoot(pOperationNode, &autoFormat, &pNodeList);
    if (FAILED(hr))
    {
        // that's fine, there don't need to be parts,could be a message with NO parameters
        hr = S_OK;
        goto Cleanup;
    }

    if (m_fIsLiteral && pParent->isDocumentMode())
    {
        // feature hack. This is to support the part name=parameters that .NET is generating
        hr = pNodeList->get_length(&lLength);
        if (FAILED(hr))
        {
            goto Cleanup;
        }
        if (lLength == 1)
        {
            hr = pNodeList->nextNode(&pPart);
            if (FAILED(hr))
            {
                goto Cleanup;
            }
            // reset the nodelist for the standard enum below
            pNodeList->reset();

            if (pPart)
            {
                CAutoBSTR bstrTemp;
                hr = _WSDLUtilFindAttribute(pPart, _T("name"),  &bstrTemp);
                if (FAILED(hr))
                {
                    globalAddError(WSDL_IDS_MAPPERNONAME, WSDL_IDS_MAPPER, hr);
                    goto Cleanup;
                }
                if (_tcscmp(bstrTemp, _T("parameters"))==0)
                {
                    hr = _WSDLUtilFindAttribute(pPart, _T("element"),  &bstrParameterWithPrefix);
                    if (SUCCEEDED(hr))
                    {
                        // got it. this is the case where we need to expand one part into LOT's of MAPPERs.
                        hr = _XSDFindChildElements(pPart, &pSubList, bstrParameterWithPrefix, WSDL_IDS_OPERATION, &bstrNSUri);
                        if (FAILED(hr))
                        {
                            globalAddError(WSDL_IDS_CREATINGPARAMETERS, WSDL_IDS_OPERATION, hr, bstrTemp, m_bstrMessageName);
                            goto Cleanup;
                        }
                        // remove the prefix from the wrapper
                        CHK(_WSDLUtilSplitQName(bstrParameterWithPrefix, 0, &m_bstrParametersWrapper));
                        // take the schemanamespace
                        CHK(m_bstrNameSpace.Assign(bstrNSUri));
                        pNodeList.Clear();
                        pNodeList = pSubList.PvReturn();
                        m_fWrapperNeeded = true;
                    }
                }
            }
        }
        else
        {
            // so we have more than ONE part. Check that there is NO type in there, otherwise throw error
            // per spec, in document/literal, you are NOT allowed to have more than one type, or type/elements mixed
            CAutoRefc<IXMLDOMNodeList> pTempList;
            CHK(autoFormat.sprintf(_T("//def:message[@name=\"%s\"]/def:part[@type]"), bstrPartName));
            hr = _XPATHUtilFindNodeListFromRoot(pOperationNode, &autoFormat, &pTempList);
            if (SUCCEEDED(hr))
            {
                hr = E_INVALIDARG;
                globalAddError(WSDL_IDS_DOCLITERALTOOMANY, WSDL_IDS_OPERATION, hr, bstrPartName);
                goto Cleanup;
              }
            hr = S_OK;
        }
    }
    else if (!m_fIsLiteral && !pParent->isDocumentMode())
    {
        // rpc/encoded again. check if we have any part/element in there
        CAutoRefc<IXMLDOMNodeList> pTempList;

        CHK(autoFormat.sprintf(_T("//def:message[@name=\"%s\"]/def:part[@element]"), bstrPartName));
        hr = _XPATHUtilFindNodeListFromRoot(pOperationNode, &autoFormat, &pTempList);
        if (SUCCEEDED(hr))
        {
            hr = E_INVALIDARG;
            globalAddError(WSDL_IDS_RCPENCODEDWITHELMENT, WSDL_IDS_OPERATION, hr, bstrPartName);
            goto Cleanup;
        }
    }


    // so we did NOT expand a parameters/element, do the normal stuff
    pPart.Clear();
    while (((hr = pNodeList->nextNode(&pPart))==S_OK) && pPart != 0)
    {
        // all of those parts go into a soapmapper...
        pSoapMapper = new CSoapObject<CSoapMapper>(INITIAL_REFERENCE);
        if (!pSoapMapper)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }
        hr = pSoapMapper->Init(pPart, ptypeFactory, fInput, bstrNSUri, pParent);
        if (FAILED(hr))
        {
            globalAddError(WSDL_IDS_INITMAPPERFAILED, WSDL_IDS_OPERATION, hr, pchOperationName);
            goto Cleanup;
        }
        CHK(pParent->AddMapper(pSoapMapper, this, fInput, lOrder));

        pPart.Clear();
         release(&pSoapMapper);
        lOrder++;
    }


    hr = S_OK;


Cleanup:
    release(&pSoapMapper);
    release(&pPart);
     ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLMessagePart::AddOrdered(ISoapMapper *pMapper)
//
//  parameters:
//
//  description:
//      adds the mapper to an ordered list for output
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLMessagePart::AddOrdered(ISoapMapper *pMapper)
{
    return (m_pEnumMappersToSave->AddOrdered(pMapper));
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::AddWSMLMetaInfo(IXMLDOMNode *pServiceNode,  CWSDLService *pWSDLService,
//							IXMLDOMDocument *pWSDLDom,  IXMLDOMDocument *pWSMLDom, bool bLoadOnServer)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::AddWSMLMetaInfo(IXMLDOMNode *pPortNode,
                                        CWSDLService *pWSDLService,
                                        IXMLDOMDocument *pWSDLDom,
                                        IXMLDOMDocument *pWSMLDom,
                                        bool bLoadOnServer)
{
    HRESULT         hr = S_OK;

    CAutoRefc<IXMLDOMNode>     pObject=0;
    CAutoRefc<IXMLDOMNode>     pExecute=0;
    CAutoRefc<CSoapMapper>     pSoapMapper=0;
    DWORD                        dwCookie;
    long                           lFetched;
    CAutoFormat                   autoFormat;
    CAutoBSTR                    bstrPortHeaderHandler;
    CAutoBSTR                    bstrOperationHeaderHandler;



    ASSERT(bLoadOnServer ? pPortNode!=0 : true);



    if (bLoadOnServer)
    {
        // first get the subnode

        CHK(autoFormat.sprintf(_T("./operation[@name=\"%s\"]/execute"), m_bstrName));

        _WSDLUtilFindAttribute(pPortNode, g_pwstrHeaderHandlerAttribute, &bstrPortHeaderHandler);


        hr = pPortNode->selectSingleNode(&autoFormat, &pExecute);
        if (hr != S_OK)
        {
            globalAddError(WSML_IDS_OPERATIONNOEXECUTE, WSDL_IDS_OPERATION, hr, m_bstrName);
            hr = E_FAIL;
            goto Cleanup;
        }

        if (pExecute)
        {
            // found the right guy. now get the information

            // check for local headerHandler
            _WSDLUtilFindAttribute(pExecute, g_pwstrHeaderHandlerAttribute, &bstrOperationHeaderHandler);

            // we need the method name
            hr = _WSDLUtilFindAttribute(pExecute, _T("method"), &m_bstrMethodName);
            if (FAILED(hr))
            {
                globalAddError(WSML_IDS_OPERATIONNOMETHODNAME, WSDL_IDS_OPERATION, hr, m_bstrName);
                goto Cleanup;
            }

            // we need the dispid -> faster than dynamic lookup
            {
                CAutoBSTR bstrTemp;

                hr = _WSDLUtilFindAttribute(pExecute, _T("dispID"), &bstrTemp);
                if (SUCCEEDED(hr))
                {
                    // convert the string to a dispid
                    m_dispID = _wtoi(bstrTemp);
                }
                else
                {
                    m_dispID = DISPID_UNKNOWN;
                }
            }


            // now we need the reference to the object used...
            {
                CAutoBSTR bstrTemp;

                hr = _WSDLUtilFindAttribute(pExecute, _T("uses"), &bstrTemp);
                if (FAILED(hr))
                {
                    globalAddError(WSML_IDS_OPERATIONNOUSESATTR, WSDL_IDS_OPERATION, hr, m_bstrName);
                    goto Cleanup;
                }
                // now get the pointer to the dispatchmapper, we can hold on to this guy
                //    as long as the service object lives. which will live as long as we do
                // as we addrefed the guy
                m_pDispatchHolder = pWSDLService->GetDispatchHolder(bstrTemp);
                if (!m_pDispatchHolder)
                {
                    globalAddError(WSML_IDS_OPERATIONINVALIDDISP, WSDL_IDS_OPERATION, hr, m_bstrName);
                    hr = E_INVALIDARG;
                    goto Cleanup;
                }
                m_pDispatchHolder->AddRef();
            }

            // check for valid headerhandler
            m_pHeaderHandler = pWSDLService->GetHeaderHandler(bstrOperationHeaderHandler ? bstrOperationHeaderHandler : bstrPortHeaderHandler);
            if (m_pHeaderHandler)
            {
                m_pHeaderHandler->AddRef();
            }


            // now we have to walk the mappers and let them add their data
            dwCookie = 0;
            while(m_pEnumSoapMappers->getNext((ISoapMapper**)&pSoapMapper, &dwCookie)==S_OK)
            {
                hr = ((CSoapMapper*)pSoapMapper)->AddWSMLMetaInfo(pExecute,
                                                                  pWSDLService,
                                                                  pWSDLDom,
                                                                  pWSMLDom,
                                                                  bLoadOnServer);
                if (FAILED(hr))
                {
                    globalAddError(WSML_IDS_MAPPERINITFAILED, WSDL_IDS_OPERATION, hr, m_bstrName);
                    goto Cleanup;
                }
                release(&pSoapMapper);
            }
            hr =m_pEnumSoapMappers->CountOfArguments((long*)&(lFetched));
            if (FAILED(hr))
            {
                goto Cleanup;
            }
        }
    }
    else
    {
        // on the client just forward to the mappers...
        dwCookie = 0;
        while(m_pEnumSoapMappers->getNext((ISoapMapper**)&pSoapMapper, &dwCookie)==S_OK)
        {
            hr = ((CSoapMapper*)pSoapMapper)->AddWSMLMetaInfo(pExecute,
                                                              pWSDLService,
                                                              pWSDLDom,
                                                              pWSMLDom,
                                                              bLoadOnServer);
            if (FAILED(hr))
            {
                globalAddError(WSML_IDS_MAPPERINITFAILED, WSDL_IDS_OPERATION, hr, m_bstrName);
                goto Cleanup;
            }
            release(&pSoapMapper);
        }

    }



Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////







/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::get_objectProgID(BSTR *pbstrobjecProgID)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::get_objectProgID(BSTR *pbstrobjectProgID)
{
    if (!pbstrobjectProgID)
    {
        return (E_INVALIDARG);
    }
    if (!m_pDispatchHolder)
    {
        // we don't have a dispatch holder
        *pbstrobjectProgID = ::SysAllocString(_T("undefined"));
        if (!*pbstrobjectProgID)
        {
            return(E_OUTOFMEMORY);
        }
        return (S_OK);
    }
    return m_pDispatchHolder->GetProgID(pbstrobjectProgID);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::get_objectMethod(BSTR *pbstrobjectMethod)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::get_objectMethod(BSTR *pbstrobjectMethod)
{
    return _WSDLUtilReturnAutomationBSTR(pbstrobjectMethod, m_bstrMethodName);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////







/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::SaveHeaders(ISoapSerializer *pISoapSerializer, VARIANT_BOOL vbInput)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::SaveHeaders(ISoapSerializer *pISoapSerializer, VARIANT_BOOL vbInput)
{
    return(E_NOTIMPL);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::Save(ISoapSerializer *pISoapSerializer, VARIANT_BOOL vbInput)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::Save(ISoapSerializer *pISoapSerializer, VARIANT_BOOL vbInput)
{

    HRESULT                     hr=S_OK;
    ISoapMapper                    *pSoapMapper=0;

#ifndef UNDER_CE
    long                        lFetched;
#endif 
    CWSDLMessagePart            *pMessageToUse;
    CAutoRefc<IEnumSoapMappers> pEnumMappers;
    bool                        fUseNameSpace=false;
    bool                        fUseWrapper;
    DWORD                       dwCookie;
    enEncodingStyle             enStyle;
    long                       lFlags=0;


    pMessageToUse = vbInput==VARIANT_TRUE ? m_pWSDLInputMessage : m_pWSDLOutputMessage;

    fUseWrapper = m_fDocumentMode==false || pMessageToUse->needsWrapper();

    if (m_fDocumentMode)
    {
        enStyle = enDocumentLiteral;
    }
    else
    {
        enStyle = pMessageToUse->IsLiteral() ? enRPCLiteral : enRPCEncoded;
    }

    if (fUseWrapper)
    {
        // we only write the wrapper for RPC mode

        fUseNameSpace = (pMessageToUse->IsLiteral() && pMessageToUse->GetNameSpace()==0);

        if (pMessageToUse->needsWrapper())
        {
            hr = pISoapSerializer->startElement(pMessageToUse->GetWrapperName(), 0, 0, 0);
            pISoapSerializer->SoapDefaultNamespace(pMessageToUse->GetNameSpace());
        }
        else
        {
            hr = pISoapSerializer->startElement(pMessageToUse->GetWrapperName(),
                        fUseNameSpace ? 0 : pMessageToUse->GetNameSpace(),
                        NULL,
                        NULL);
        }

        if (FAILED(hr))
        {
            ASSERT("Writing first message part to stream failed");
            hr = E_FAIL;
            goto Cleanup;
        }
    }
    // once you can handle different namespaces, you can replace L"m" with NULL and a
    // new namespace will  be generated

    if (m_fCreateHrefs)
    {
        lFlags |= c_saveIDRefs;
    }
    dwCookie = 0;
    // now walk over the mappers for the input/output method and serialize them
    while (pMessageToUse->getMappers()->getNext(&pSoapMapper, &dwCookie)==S_OK)
    {
        // set the encoding for this operation
        CHK(pSoapMapper->Save(pISoapSerializer, pMessageToUse->GetEncodingStyle(), enStyle, !fUseNameSpace ? pMessageToUse->GetNameSpace() : 0, lFlags));
        release(&pSoapMapper);
    }

    if (m_fCreateHrefs)
    {
        if (fUseWrapper)
        {
            // close the RPC mode element
            CHK(pISoapSerializer->endElement());
            fUseWrapper = false;
        }

        dwCookie = 0;
        // now walk over the mappers for the input/output method and serialize them
        while (pMessageToUse->getMappers()->getNext(&pSoapMapper, &dwCookie)==S_OK)
        {
            // set the encoding for this operation
            CHK(((CSoapMapper*)(ISoapMapper*)pSoapMapper)->SaveValue(pISoapSerializer, pMessageToUse->GetEncodingStyle(), (enEncodingStyle)enStyle));
            release(&pSoapMapper);
        }
    }


Cleanup:
    release(&pSoapMapper);

    if (fUseWrapper)
    {
        // close the RPC mode element
        if (FAILED(pISoapSerializer->endElement()))
        {
            ASSERT("Writing method end stream failed");
            hr = E_FAIL;
        }
    }
    // now we are done...

    if (FAILED(hr))
    {
        globalSetActor(m_bstrSoapAction);
    }
    ASSERT(hr == S_OK);
    return(hr);

}





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::Load(ISoapReader *pISoapReader, VARIANT_BOOL vbInput)
//
//  parameters:
//
//  description:
//      this works as follows:
//          RPC mode -> search for the wrapper element with a namespace qualified XPath
//          docuement mdoe -> search for the SOAP:BODY
//          pass this as a parent element into the soapmapper load routines
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::Load(ISoapReader *pISoapReader, VARIANT_BOOL vbInput)
{
    HRESULT                        hr = E_FAIL;
    CAutoRefc<IXMLDOMNode>         pRootNode=0;
    CAutoRefc<IXMLDOMNode>         pNode=0;
    CAutoRefc<IEnumSoapMappers> pEnumMappers;
    ISoapMapper                    *pSoapMapper=0;
#ifndef UNDER_CE
    long                        lFetched;
#endif 
    CWSDLMessagePart            *pMessageToUse;
    bool                        fUseWrapper;
    DWORD                       dwCookie;
    enEncodingStyle                enStyle;
    bool                        bUseMessageNS=false;
    CAutoFormat                 autoFormat;



    hr = _WSDLUtilGetRootNodeFromReader(pISoapReader, &pRootNode);
    if (FAILED(hr))
    {
        goto Cleanup;
    }


    pMessageToUse = vbInput==VARIANT_TRUE ? m_pWSDLInputMessage : m_pWSDLOutputMessage;
    fUseWrapper = m_fDocumentMode==false || pMessageToUse->needsWrapper();

    if (m_fDocumentMode)
    {
        enStyle = enDocumentLiteral;
    }
    else
    {
        enStyle = pMessageToUse->IsLiteral() ? enRPCLiteral : enRPCEncoded;
    }

    // set up search queries and the bUseMessageNS flag
    if (fUseWrapper)
    {
        // search the RPC wrapper element
        // prepare namespace lookup
        if (pMessageToUse->IsLiteral() && pMessageToUse->GetNameSpace()==0)
        {
            // if literal, there will be no explicit namespace, preparesetup for global soap envelope namespace
            // so look for SOAPENV:BODY/message
            hr = _XPATHUtilPrepareLanguage(pRootNode, (WCHAR*)g_pwstrMessageNSPrefix, (WCHAR*)g_pwstrEnvNS);
            CHK(autoFormat.sprintf(_T("//%s:%s/%s"), g_pwstrMessageNSPrefix, _T("Body"), pMessageToUse->GetWrapperName()));

        }
        else
        {
            // if encode, there will be an explicit namespace, preparesetup for local  namespace
            // and look for namespace:message
            hr = _XPATHUtilPrepareLanguage(pRootNode, (WCHAR*)g_pwstrMessageNSPrefix, pMessageToUse->GetNameSpace());
            CHK(autoFormat.sprintf(_T("//%s:%s"), g_pwstrMessageNSPrefix, pMessageToUse->GetWrapperName()));
            bUseMessageNS= enStyle == enRPCLiteral;

        }
        CHK(hr);
    }


    // this indicates loading a request. Here all kind of complicated rules apply, if we have a wrapper or not
    if (vbInput == VARIANT_TRUE)
    {
        if (fUseWrapper)
        {
            // now let's see if we are there.
            hr = _XPATHUtilFindNodeFromRoot(pRootNode, &autoFormat, &pNode);
            if (hr != S_OK)
            {
                hr = E_INVALIDARG;
                goto Cleanup;
            }
            // now we should have the correct node
            // walk over the mappers and tell them to load themselves....

            if (bUseMessageNS==false)
            {
                // reset the namespace lookup
                // if the namespace of the message IS used, we will do xpath lookup in the mappers as well...
                _XPATHUtilResetLanguage(pRootNode);
            }

        }
        else
        {
            CHK(findBodyNode(pRootNode, &pNode));
        }
    }
    else
    {
        // now if we load a response everything is easy. just go and find the body and take it's first child node
        CAutoRefc<IXMLDOMNode> pBody;

        CHK(findBodyNode(pRootNode, &pBody));

        if (fUseWrapper)
        {
            CHK(_WSDLUtilFindFirstChild(pBody, &pNode));
        }
        else
        {
            pNode = pBody.PvReturn();
        }
    }

    // now walk over the mappers for the input/output method and load them

    dwCookie = 0;

    showNode(pNode);

    // now walk over the mappers for the input/output method and serialize them
    while (pMessageToUse->getMappers()->getNext(&pSoapMapper, &dwCookie)==S_OK)
    {
        hr = pSoapMapper->Load(pNode, pMessageToUse->GetEncodingStyle(), enStyle, bUseMessageNS ? pMessageToUse->GetNameSpace() : 0);
        if (FAILED(hr))
        {
            //    value and the fragment was not found for this mapper.
            goto Cleanup;
        }
        release(&pSoapMapper);
    }


Cleanup:
    if (FAILED(hr) && hr != E_INVALIDARG)
    {
        globalSetActor(m_bstrSoapAction);
    }

    release(&pSoapMapper);
    return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::findBodyNode(IXMLDOMNode *pRootNode, IXMLDOMNode **ppNode)
//
//  parameters:
//      node as return value
//  description:
//      finds the soap/body node
//  returns:
//      S_OK found it
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::findBodyNode(IXMLDOMNode *pRootNode, IXMLDOMNode **ppNode)
{
    HRESULT hr;
    CAutoFormat autoFormat;

    // find the SOAP:BODY element
    // prepare namespace lookup
    CHK( _XPATHUtilPrepareLanguage(pRootNode, (WCHAR*)g_pwstrMessageNSPrefix, (WCHAR*)g_pwstrEnvNS));
    // now let's see if we are there.
    // first find body/header
    CHK(autoFormat.sprintf(_T("//%s:%s"), g_pwstrMessageNSPrefix, _T("Body")));
    hr = _XPATHUtilFindNodeFromRoot(pRootNode, &autoFormat, ppNode);
    if (hr != S_OK)
    {
        hr = E_INVALIDARG;
        goto Cleanup;
    }
    // now we should have the correct node
    // walk over the mappers and tell them to load themselves....
    _XPATHUtilResetLanguage(pRootNode);

Cleanup:
    return hr;

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::verifyPartName(smIsInputEnum snInput, TCHAR *pchPartName)
//
//  parameters:
//
//  description:
//       verifies that the partname is unique in the current mapper collection
//  returns:
//      S_OK: yep, unique
//      E_FAIL: nope, not unique
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::verifyPartName(smIsInputEnum snInput, TCHAR *pchPartName)
{
    HRESULT                        hr = E_FAIL;
    ISoapMapper                    *pSoapMapper=0;
#ifndef UNDER_CE
    LONG                        lFetched;
#endif 
    DWORD                     dwCookie;
    smIsInputEnum               inOut;

    dwCookie = 0;
    while(m_pEnumSoapMappers->getNext(&pSoapMapper, &dwCookie)==S_OK)
    {
        pSoapMapper->get_isInput(&inOut);
        if (inOut == snInput)
        {
            if (wcscmp(((CSoapMapper*)pSoapMapper)->getPartName(), pchPartName)==0)
            {
                hr = E_FAIL;
                goto Cleanup;
            }
        }


        release(&pSoapMapper);
    }
    hr = S_OK;


Cleanup:
    release(&pSoapMapper);
    return(hr);

}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::get_hasHeader( VARIANT_BOOL *pbHeader)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::get_hasHeader( VARIANT_BOOL *pbHeader)
{
    if (!pbHeader || !m_pWSDLInputMessage)
    {
        return(E_INVALIDARG);
    }
    *pbHeader = m_pWSDLInputMessage->IsBody() ? VARIANT_FALSE : VARIANT_TRUE;

    return (S_OK);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////









/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::get_messageName(BSTR bstrMessageName, bool fInput)
//
//  parameters:
//
//  description:
//        helper called from soapmapper
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::get_messageName(BSTR *pbstrMessageName, bool fInput)
{
    CWSDLMessagePart    *pMessageToUse;

    pMessageToUse = fInput ? m_pWSDLInputMessage : m_pWSDLOutputMessage;

    return _WSDLUtilReturnAutomationBSTR(pbstrMessageName , pMessageToUse->GetMessageName());
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: TCHAR *CWSDLOperation::getMessageNameSpace(bool fInput)
//
//  parameters:
//
//  description:
//        helper called from soapmapper
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
TCHAR * CWSDLOperation::getMessageNameSpace(bool fInput)
{
    CWSDLMessagePart    *pMessageToUse;
    pMessageToUse = fInput ? m_pWSDLInputMessage : m_pWSDLOutputMessage;
    if (pMessageToUse)
    {
        return pMessageToUse->GetNameSpace();
    }
    return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: TCHAR * CWSDLOperation::getEncoding(bool fInput)
//
//  parameters:
//
//  description:
//        helper called from soapmapper
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
TCHAR * CWSDLOperation::getEncoding(bool fInput)
{
    CWSDLMessagePart    *pMessageToUse;

    pMessageToUse = fInput ? m_pWSDLInputMessage : m_pWSDLOutputMessage;
    if (pMessageToUse)
    {
        return (pMessageToUse->GetEncodingStyle());
    }

    return (0);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWSDLOperation::get_preferredEncoding(BSTR * pbstrpreferredEncoding)
//
//  parameters:
//
//  description:
//      returns the encoding specified with an extensibility attribute inside the WSDLFile
//          note that m_bstrpreferedEncoding will default to UTF-8 if not set in the file
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWSDLOperation::get_preferredEncoding(BSTR * pbstrpreferredEncoding)
{
    HRESULT hr = S_OK;

    if (!pbstrpreferredEncoding)
    {
        hr = E_INVALIDARG;
        goto Cleanup;
    }

    *pbstrpreferredEncoding = 0;

    if (m_bstrpreferedEncoding.Len()>0)
    {
        *pbstrpreferredEncoding = ::SysAllocString(m_bstrpreferedEncoding);

        if (!*pbstrpreferredEncoding)
        {
            hr = E_OUTOFMEMORY;
        }
    }


Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
