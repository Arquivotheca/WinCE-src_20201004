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
// File:    soapreader.cpp
//
// Contents:
//
// implementation file
//
// ISoapReader Interface implemenation
//
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "soaprdr.h"
#include "xpathutil.h"
#include "mssoap.h"
#include "soapglo.h"


#ifndef UNDER_CE
#include "WinCEUtils.h"
#endif


TYPEINFOIDS(ISoapReader, MSSOAPLib)

BEGIN_INTERFACE_MAP(CSoapReader)
    ADD_IUNKNOWN(CSoapReader, ISoapReader)
    ADD_INTERFACE(CSoapReader, ISoapReader)
    ADD_INTERFACE(CSoapReader, IDispatch)
END_INTERFACE_MAP(CSoapReader)

const WCHAR *       pMySelectionNS      = L"xmlns:CSR=\"http://schemas.xmlsoap.org/soap/envelope/\"";
const WCHAR *       pMyPrefix           = L"CSR";
const WCHAR *       pMyPrefix2          = L"CSR2";
const WCHAR *       pget_Envelope       = L"//CSR:Envelope";
const WCHAR *       pget_Body           = L"//CSR:Envelope/CSR:Body";
const WCHAR *       pget_Header         = L"//CSR:Envelope/CSR:Header";
const WCHAR *       pget_Header2        = L"//CSR:Envelope/CSR:Header/";
const WCHAR *       pget_Fault          = L"//CSR:Envelope/CSR:Body/CSR:Fault";
const WCHAR *       pget_FaultStr       = L"//CSR:Envelope/CSR:Body/CSR:Fault/";
const WCHAR *       pget_HeaderEntryNS  = L"xmlns:CSR=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:CSR2=\"";
const WCHAR *       pget_HeaderEntry    = L"//CSR:Envelope/CSR:Header/CSR2:";
const WCHAR *       pget_MustUnderstand = L"//CSR:Envelope/CSR:Header/*[@CSR:mustUnderstand=\"1\"]";
const WCHAR *       pget_HeaderEntries  = L"//CSR:Envelope/CSR:Header/*";
const WCHAR *       pget_BodyEntries    = L"//CSR:Envelope/CSR:Body/*";
const WCHAR *       pget_BodyEntry      = L"//CSR:Envelope/CSR:Body/CSR2:";
const WCHAR *       pget_BodyEntry2     = L"//CSR:Envelope/CSR:Body/";
const WCHAR *       pget_RPCResult      = L"//CSR:Envelope/CSR:Body/*[1]/*";
const WCHAR *       p_getRPCStruct      = L"//CSR:Envelope/CSR:Body/*[1]";
const WCHAR *       p_getRPCParameter   = L"//CSR:Envelope/CSR:Body/*[1]/CSR2:";
const WCHAR *       p_getRPCParameter2  = L"//CSR:Envelope/CSR:Body/*[1]/";

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapReader::CSoapReader()
//
//  parameters:
//
//  description:
//        Constructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapReader::CSoapReader()
{
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapReader::~CSoapReader()
//
//  parameters:
//
//  description:
//        Destructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapReader::~CSoapReader()
{
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::load(VARIANT xmlSource, BSTR  bstrSoapAction,
//                                      VARIANT_BOOL *isSuccessful)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapReader::load( VARIANT  xmlSource, BSTR  bstrSoapAction, VARIANT_BOOL  *isSuccessful )
{
#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif 
    {   
        HRESULT hr = S_OK;
#ifndef CE_NO_EXCEPTIONS
        CAutoRefc<IRequest> pRequest;
#else
        IRequest *pRequest = NULL;
#endif 

        CHK( FillDomMemberVar() );
        CHK ( m_pDOMDoc->load(xmlSource, isSuccessful) );

        if (bstrSoapAction && ::SysStringLen(bstrSoapAction)>0)
        {
            CHK(UnescapeUrl(bstrSoapAction, &m_bstrSoapAction));
        }
        else
        {
            // now see if we got a IRequest object, and if so, get the soapaction header
            if (V_VT(&xmlSource)==VT_DISPATCH || V_VT(&xmlSource)==VT_UNKNOWN)
            {
                if (V_VT(&xmlSource)==VT_DISPATCH)
                {
                    hr = V_DISPATCH(&xmlSource)->QueryInterface(IID_IRequest, (void ** )&pRequest);
                }
                else
                {
                    hr = V_UNKNOWN(&xmlSource)->QueryInterface(IID_IRequest, (void**)&pRequest);
                }
                if (SUCCEEDED(hr) && pRequest)
                {
                    CHK(RetrieveSoapAction(pRequest));
                }
                hr = S_OK;
            }

        }

    Cleanup:
#ifndef CE_NO_EXCEPTIONS
        if(pRequest)
            pRequest->Release();
#endif 
        ASSERT (hr == S_OK);
        return hr;
    }
#ifndef CE_NO_EXCEPTIONS       
    catch (...)
#else
    __except(1)
#endif    
    {
#ifndef CE_NO_EXCEPTIONS
        if(pRequest)
            pRequest->Release();
#endif 
        ASSERTTEXT (FALSE, "CSoapReader::load - Unhandled Exception");
        return E_FAIL;
    }

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::loadXML(BSTR bstrXML, VARIANT_BOOL *isSuccessful)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapReader::loadXML( BSTR bstrXML, VARIANT_BOOL *isSuccessful )
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif
        HRESULT hr = S_OK;

        CHK (FillDomMemberVar());

        CHK ( m_pDOMDoc->loadXML(bstrXML, isSuccessful) );

    Cleanup:
        ASSERT (hr == S_OK);
        return hr;
#ifndef CE_NO_EXCEPTIONS       
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapReader::loadXML - Unhandled Exception");
        return E_FAIL;
    }
#endif    
}






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::RetrieveSoapAction(IRequest *pRequest)
//
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapReader::RetrieveSoapAction( IRequest *pRequest )
{
    CAutoRefc<IRequestDictionary>   pDictionary;
    HRESULT hr = S_OK;
    CVariant vHTTPHeader;
    CVariant vValue;

    CHK (pRequest->get_ServerVariables(&pDictionary));
    CHK (vHTTPHeader.Assign(L"HTTP_SoapAction"));
    CHK (pDictionary->get_Item(vHTTPHeader, &vValue));
    
    CHK (VariantChangeType(&vValue, &vValue, 0, VT_BSTR));

  
    if (::SysStringLen(V_BSTR(&vValue))>0)
    {
        WCHAR *pchWalk=NULL;
        WCHAR *pchTarget=NULL;

        CHK(UnescapeUrl(V_BSTR(&vValue), &m_bstrSoapAction));

        // the soapaction property is most likely enclosed in quotationmarks... remove them...
        pchWalk = m_bstrSoapAction;
        pchTarget = pchWalk;
        while (pchWalk && *pchWalk)
        {
            if (*pchWalk != '\'' && *pchWalk != '"')
            {
                *pchTarget=*pchWalk;
                pchTarget++;
            }
            pchWalk++;
        }
        // zero terminate
        *pchTarget=0;
    }

Cleanup:
    ASSERT(SUCCEEDED(hr));
    return(hr);
}



////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_DOM(IUnknown **pXMLDOMDocument)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_DOM( IXMLDOMDocument **pIXMLDOMDocument )
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif    
        HRESULT hr = S_OK;

        CHK_BOOL(pIXMLDOMDocument != NULL, E_INVALIDARG);
        CHK(FillDomMemberVar());

        CHK(m_pDOMDoc->QueryInterface(IID_IXMLDOMDocument, (void**) pIXMLDOMDocument));

    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
 #ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapReader::get_DOM - Unhandled Exception");
        return E_FAIL;
    }
 #endif   
}


////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_Envelope(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_Envelope( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS  
    try
    {
#endif    
        return  ( DOMElementLookup( pMySelectionNS,
                                pget_Envelope,
                                ppElement ) );
#ifndef CE_NO_EXCEPTIONS                                
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapReader::get_Envelope - Unhandled Exception");
        return E_FAIL;
    }
#endif   
}


////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_Body(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_Body( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS   
    try
    {
#endif
        return ( DOMElementLookup( pMySelectionNS,
                                pget_Body,
                                ppElement ) );
#ifndef CE_NO_EXCEPTIONS                                  
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_Body - Unhandled Exception");
        return E_FAIL;
        }
#endif        
}


////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_Header(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_Header( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS   
    try
    {
#endif    
        return (DOMElementLookup( pMySelectionNS,
                                pget_Header,
                                ppElement ) );
#ifndef CE_NO_EXCEPTIONS                                  
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_Header - Unhandled Exception");
        return E_FAIL;
        }
#endif        
}

////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_Fault(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_Fault( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS   
    try
    {
#endif    
        return ( DOMElementLookup( pMySelectionNS,
                                pget_Fault,
                                ppElement ) );
#ifndef CE_NO_EXCEPTIONS                                 
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_Fault - Unhandled Exception");
        return E_FAIL;
        }
#endif       
}



////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_FaultString(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_FaultString( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS 
    try
    {
#endif 
        return getFaultXXX (ppElement, g_pwstrFaultstring);
#ifndef CE_NO_EXCEPTIONS 
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_FaultString - Unhandled Exception");
        return E_FAIL;
        }
#endif 
}


////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_FaultCode(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_FaultCode( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS 
    try
    {
#endif 
        return getFaultXXX (ppElement, g_pwstrFaultcode);
#ifndef CE_NO_EXCEPTIONS 
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_FaultCode - Unhandled Exception");
        return E_FAIL;
        }
#endif 
}


////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_FaultActor(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_FaultActor( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS 
    try
    {
#endif 
        return getFaultXXX (ppElement, g_pwstrFaultactor);
#ifndef CE_NO_EXCEPTIONS 
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_FaultActor - Unhandled Exception");
        return E_FAIL;
        }
#endif
}



////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_FaultDetail(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_FaultDetail( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS 
    try
    {
#endif 
        return getFaultXXX (ppElement, g_pwstrDetail);
#ifndef CE_NO_EXCEPTIONS        
    }
    catch(...)
    {
        ASSERTTEXT (FALSE, "CSoapReader::get_FaultDetail - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}



////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_HeaderEntry(BSTR LocalName, BSTR NamespaceURI,
//                                                 IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_HeaderEntry( BSTR LocalName, BSTR NamespaceURI,
                                      IXMLDOMElement **ppElement)
{
#ifndef CE_NO_EXCEPTIONS 
    try
    {
#else
    __try
    {
#endif
        WCHAR *         pSelectionNS;
        WCHAR *         pXPath;

        if (ppElement == NULL)
            return E_INVALIDARG;

        if (!LocalName)
            return E_INVALIDARG;
        *ppElement = NULL;

        if (NamespaceURI ==NULL)
            NamespaceURI = (BSTR)g_pwstrEmpty;

        // build the selcection namespace of the form:
        //  xmlns:CRS=<envelopens>[ xmlns:CRS2=<NamespaceURI>]
        pSelectionNS = (WCHAR *) _alloca(sizeof(WCHAR) * (8 + wcslen(pget_HeaderEntryNS) +
                                                              wcslen(NamespaceURI) ) );
        if (wcslen(NamespaceURI))
        {
            wcscpy(pSelectionNS, pget_HeaderEntryNS);
            wcscat(pSelectionNS, NamespaceURI);
            wcscat(pSelectionNS, L"\"");
        }
        else
        {
            wcscpy(pSelectionNS, pMySelectionNS);
        }

        // build the xpath expression of the form
        //      CSR:Header/[CSR2:]<LocalName>
        pXPath = (WCHAR *) _alloca(sizeof(WCHAR) * (8 + wcslen(pget_HeaderEntry) +
                                                        wcslen(LocalName) ) );

        if (wcslen(NamespaceURI))
            wcscpy(pXPath, pget_HeaderEntry);
        else
            wcscpy(pXPath, pget_Header2);
        wcscat(pXPath, LocalName);

        return ( DOMElementLookup( pSelectionNS,
                                pXPath,
                                ppElement ) );

#ifndef CE_NO_EXCEPTIONS 
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_HeaderEntry - Unhandled Exception");
        return E_FAIL;
        }
#else
    }
    __except(1)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_HeaderEntry - Unhandled Exception");
        return E_FAIL;
        }
#endif 
}



////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_MustUnderstandHeaderEntries(IXMLDOMNodeList **ppNodeList)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_MustUnderstandHeaderEntries( IXMLDOMNodeList **ppNodeList)
{
#ifndef  CE_NO_EXCEPTIONS
    try
    {
#endif 
        return ( DOMNodeListLookup( pMySelectionNS,
                                 pget_MustUnderstand,
                                 ppNodeList ) );
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_MustUnderstandHeaderEntries - Unhandled Exception");
        return E_FAIL;
        }
#endif 
}



////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_HeaderEntries(IXMLDOMNodeList **ppNodeList)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_HeaderEntries( IXMLDOMNodeList **ppNodeList)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        return ( DOMNodeListLookup( pMySelectionNS,
                                 pget_HeaderEntries,
                                 ppNodeList ) );

#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_HeaderEntries - Unhandled Exception");
        return E_FAIL;
        }
#endif 
}



////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_BodyEntries(IXMLDOMNodeList **ppNodeList)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_BodyEntries( IXMLDOMNodeList **ppNodeList)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        return ( DOMNodeListLookup( pMySelectionNS,
                                 pget_BodyEntries,
                                 ppNodeList ) );
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_BodyEntries - Unhandled Exception");
        return E_FAIL;
        }
#endif 
}



////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_BodyEntry(BSTR LocalName, BSTR NamespaceURI,
//                                               IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_BodyEntry( BSTR LocalName, BSTR NamespaceURI,
                                    IXMLDOMElement **ppElement)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#else
    __try
    {
#endif 

        HRESULT         hr(S_OK);
        WCHAR *         pSelectionNS;
        WCHAR *         pXPath;

        if (ppElement == NULL)
            return E_INVALIDARG;

        if (!LocalName)
            return E_INVALIDARG;

        if (NamespaceURI ==NULL)
            NamespaceURI = (BSTR)g_pwstrEmpty;

        // build the selcection namespace of the form:
        //  xmlns:CRS=<envelopens>[ xmlns:CRS2=<NamespaceURI>]
        pSelectionNS = (WCHAR *) _alloca(sizeof(WCHAR) * (8 + wcslen(pget_HeaderEntryNS) +
                                                              wcslen(NamespaceURI) ) );
        if (wcslen(NamespaceURI))
        {
            wcscpy(pSelectionNS, pget_HeaderEntryNS);
            wcscat(pSelectionNS, NamespaceURI);
            wcscat(pSelectionNS, L"\"");
        }
        else
        {
            wcscpy(pSelectionNS, pMySelectionNS);
        }

        // build the xpath expression of the form
        //      CSR:Body/CSR2:<LocalName>
        pXPath = (WCHAR *) _alloca(2 * (8 + wcslen(pget_BodyEntry) +
                                             wcslen(LocalName) ) );
        if (wcslen(NamespaceURI))
        {
            wcscpy(pXPath, pget_BodyEntry);
        }
        else
        {
             wcscpy(pXPath, pget_BodyEntry2);
        }
        wcscat(pXPath, LocalName);

        CHK ( DOMElementLookup( pSelectionNS,
                                pXPath,
                                ppElement ) );

    Cleanup:
        ASSERT (hr == S_OK);
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_BodyEntry - Unhandled Exception");
        return E_FAIL;
        }
#else
    }
   __except(1)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_BodyEntry - Unhandled Exception");
        return E_FAIL;
        }
#endif 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_soapAction(BSTR *pbstrSoapAction)
//
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_soapAction(BSTR *pbstrSoapAction)
{
    if (!pbstrSoapAction)
        return(E_INVALIDARG);
    if (m_bstrSoapAction.Len())
    {
        *pbstrSoapAction = ::SysAllocString(m_bstrSoapAction);
        if (!*pbstrSoapAction)
            return(E_OUTOFMEMORY);
    }
    else
    {
        *pbstrSoapAction = 0;
    }
    return(S_OK);
}




////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_RPCStruct(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_RPCStruct( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT                     hr(S_OK);

        CHK ( DOMElementLookup( pMySelectionNS,
                                p_getRPCStruct,
                                ppElement ) );

    Cleanup:
        ASSERT (hr == S_OK);
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_RPCStruct - Unhandled Exception");
        return E_FAIL;
        }
#endif /* CE_NO_EXCEPTIONS */
}



////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_RPCParameter(BSTR LocalName, BSTR NamespaceURI,
//                                                  IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_RPCParameter( BSTR LocalName, BSTR NamespaceURI,
                                       IXMLDOMElement **ppElement)
{
#ifndef CE_NO_EXCEPTIONS
    try {
#else
    __try {
#endif 
        HRESULT         hr(S_OK);
        WCHAR *         pSelectionNS;
        WCHAR *         pXPath;

        if (ppElement == NULL)
            return E_INVALIDARG;

        if (!LocalName)
            return E_INVALIDARG;

        if (NamespaceURI ==NULL)
            NamespaceURI = (BSTR)g_pwstrEmpty;

        // build the selcection namespace of the form:
        //  xmlns:CRS=<envelopens>[ xmlns:CRS2=<NamespaceURI>]
        pSelectionNS = (WCHAR *) _alloca(sizeof(WCHAR) * (8 + wcslen(pget_HeaderEntryNS) +
                                                              wcslen(NamespaceURI) ) );
        if (wcslen(NamespaceURI))
        {
            wcscpy(pSelectionNS, pget_HeaderEntryNS);
            wcscat(pSelectionNS, NamespaceURI);
            wcscat(pSelectionNS, L"\"");
        }
        else
        {
            wcscpy(pSelectionNS, pMySelectionNS);
        }

        // build the xpath expression of the form
        //      CSR:Body/*[1]/[CSR2:]<LocalName>
        pXPath = (WCHAR *) _alloca(2 * (8 + wcslen(p_getRPCParameter) +
                                             wcslen(LocalName) ) );
        if (wcslen(NamespaceURI))
        {
            wcscpy(pXPath, p_getRPCParameter);
        }
        else
        {
             wcscpy(pXPath, p_getRPCParameter2);
        }
        wcscat(pXPath, LocalName);

        CHK ( DOMElementLookup( pSelectionNS,
                                pXPath,
                                ppElement ) );

    Cleanup:
        ASSERT (hr == S_OK);
        return hr;
    }
#ifndef CE_NO_EXCEPTIONS
    catch (...)
#else
    __except(1)
#endif 
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_RPCParameter - Unhandled Exception");
        return E_FAIL;
        }
}


////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapReader::get_RPCResult(IXMLDOMElement **ppElement)
//
//  parameters:
//
//  description:
//
//  returns:
//
/////////////////////////////////////////////////////////////
HRESULT CSoapReader::get_RPCResult( IXMLDOMElement **ppElement )
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT                     hr(S_OK);

        CHK ( DOMElementLookup( pMySelectionNS,
                                pget_RPCResult,
                                ppElement ) );

    Cleanup:
        ASSERT (hr == S_OK);
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapReader::get_RPCResult - Unhandled Exception");
        return E_FAIL;
        }
#endif 
}






///////////////////////////////////////////////////////////////////////////////
//  function: HRESULT FillDomMemberVar(void)
//
//  parameters:
//
//  description:
//        Fills the member variable with a DOM pointer if currently uninitialized
//
//  returns:
//
///////////////////////////////////////////////////////////////////////////////
HRESULT CSoapReader::FillDomMemberVar(void)
{
    HRESULT         hr(S_OK);
    VARIANT_BOOL    vb(-1);


    if (m_pDOMDoc == NULL)
    {
#ifndef UNDER_CE 
        // no DOM - we have to create one
        CHK ( CoCreateInstance(CLSID_FreeThreadedDOMDocument30, NULL, 
                        CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, (void**)&m_pDOMDoc) );
#else
        CHK ( CoCreateInstance(CLSID_DOMDocument, NULL, 
               CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument2, (void**)&m_pDOMDoc) );               
       
        CHK(m_pDOMDoc->put_resolveExternals(false));            
    
#endif
        CHK ( m_pDOMDoc->put_preserveWhiteSpace(vb) );
    }

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}


///////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT DOMElementLookup(const WCHAR * pSelectionNS, const WCHAR * pXpath,
//                                     IXMLDOMElement **ppElement)
//
//  parameters:
//      pNamespace  - the namespace used in the xpath
//      pURI        - the URI for this namespace
//      pXPath      - the xpath expression used
//
//  description:
//        Do a XPath Element lookup in the XML document
//
//      to look up the Envelope definition in the XML document:
//
//      DOMElementLookup( L"xmlns:CSR=\"http://schemas.xmlsoap.org/soap/envelope/\"",
//                        L"CSR:Envelope",
//                        ppElement ) );
//
//  returns:
//
///////////////////////////////////////////////////////////////////////////////
HRESULT CSoapReader::DOMElementLookup( const WCHAR * pSelectionNS,
            const WCHAR * pXpath, IXMLDOMElement **ppElement)
{
    HRESULT                     hr(S_OK);
    CAutoRefc<IXMLDOMNode>         pNode;

    XPathState  xp;

    CHK_BOOL(m_pDOMDoc, E_FAIL);
    CHK_BOOL(ppElement, E_INVALIDARG);
    *ppElement = NULL;

    CHK (xp.initDOMDocument(m_pDOMDoc));
    CHK (xp.addNamespace(pSelectionNS));

    CHK ( m_pDOMDoc->selectSingleNode((BSTR) pXpath, &pNode) );

    if (pNode)
        CHK (pNode->QueryInterface(IID_IXMLDOMElement, (void **)ppElement));
Cleanup:
    return (hr);
}


///////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT DOMNodeListLookup(const WCHAR * pSelectionNS, const WCHAR * pXpath,
//                                      IXMLDOMNodeList **ppNodeList)
//
//  parameters:
//      pNamespace  - the namespace used in the xpath
//      pURI        - the URI for this namespace
//      pXPath      - the xpath expression used
//
//  description:
//        Do a XPath Element lookup in the XML document
//
//      to look up the Envelope definition in the XML document:
//
//      DOMElementLookup( L"xmlns:CSR=\"http://schemas.xmlsoap.org/soap/envelope/\"",
//                        L"CSR:Envelope",
//                        ppElement ) );
//
//  returns:
//
///////////////////////////////////////////////////////////////////////////////
HRESULT CSoapReader::DOMNodeListLookup( const WCHAR * pSelectionNS,
            const WCHAR * pXpath, IXMLDOMNodeList **ppNodeList)
{
    HRESULT hr(S_OK);
    XPathState xp;

    CHK_BOOL(m_pDOMDoc, E_FAIL);
    CHK_BOOL(ppNodeList != NULL, E_INVALIDARG);
    *ppNodeList = NULL;

    CHK (xp.initDOMDocument(m_pDOMDoc));
    CHK (xp.addNamespace(pSelectionNS));

    CHK ( m_pDOMDoc->selectNodes((BSTR) pXpath, ppNodeList) );

Cleanup:
    return (hr);
}




///////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT getFaultXXX(IXMLDOMElement **ppElement, const WCHAR * element)
//
//  parameters:
//      ppElement  - the return node
//      element    - the element we are looking for
//
//  description:
//
//  returns:
//
///////////////////////////////////////////////////////////////////////////////
HRESULT CSoapReader::getFaultXXX( 
            IXMLDOMElement **ppElement,
            const WCHAR * element)
{
    HRESULT hr  = S_OK;
    WCHAR * pCombinedXPath;

    // build the xpath expression of the form
    //      pXpath[CSR:]element
#ifndef UNDER_CE
    pCombinedXPath = (WCHAR *) _alloca(sizeof(WCHAR) * (wcslen(pget_FaultStr) + wcslen(element) + 10 /*+ wcslen(pMyPrefix) */) );
#else
#ifdef CE_NO_EXCEPTIONS
        pCombinedXPath = new WCHAR[wcslen(pget_FaultStr) + wcslen(element) + 10 /*+ wcslen(pMyPrefix) */];
        if(!pCombinedXPath)
            return E_OUTOFMEMORY;
#else
    try{
       pCombinedXPath = (WCHAR *) _alloca(sizeof(WCHAR) * (wcslen(pget_FaultStr) + wcslen(element) + 10 /*+ wcslen(pMyPrefix) */) );
    }
    catch(...){
        return E_OUTOFMEMORY;
    }
#endif
#endif
    // build the path without the prefix
    wcscpy(pCombinedXPath, pget_FaultStr);
    wcscat(pCombinedXPath, element);

    hr =DOMElementLookup( pMySelectionNS, pCombinedXPath, ppElement );
    if (SUCCEEDED(hr) && (ppElement))
#ifdef CE_NO_EXCEPTIONS
        {
            delete [] pCombinedXPath;
            return hr;
        }
#else
        return hr;
#endif

    // we did not find it with the xpath without a prefix, try with a prefix
    wcscpy(pCombinedXPath, pget_FaultStr);
    wcscat(pCombinedXPath, pMyPrefix);
    wcscat(pCombinedXPath, L":");
    wcscat(pCombinedXPath, element);
    
#ifdef CE_NO_EXCEPTIONS
    hr = DOMElementLookup( pMySelectionNS, pCombinedXPath, ppElement );
    delete [] pCombinedXPath;
    return hr;
#else
    return DOMElementLookup( pMySelectionNS, pCombinedXPath, ppElement );
#endif
}


// End Of File
