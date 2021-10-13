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
// File:    soapserializer.cpp
// 
// Contents:
//
//  implementation file 
//
//        ISoapMapper Interface implemenation
//    
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "soapstream.h"
#include "soapser.h"
#include "mssoap.h"

TYPEINFOIDS(ISoapSerializer, MSSOAPLib)


BEGIN_INTERFACE_MAP(CSoapSerializer)
    ADD_IUNKNOWN(CSoapSerializer, ISoapSerializer)
    ADD_INTERFACE(CSoapSerializer, ISoapSerializer)
    ADD_INTERFACE(CSoapSerializer, IDispatch)
END_INTERFACE_MAP(CSoapSerializer) 


static const WCHAR  *acXMLencdef    = L"UTF-8";


// shortcut to check the m_eState 
#define chks(s)        (m_eState == (s))

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::CSoapSerializer()
//
//  parameters:
//
//  description: Constructor 
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapSerializer::CSoapSerializer() :
    m_pcSoapPrefix(NULL),
    m_pcDefaultNamespaceURI((WCHAR*)g_pwstrEmpty),
    m_eState(created),
    m_bElementOpen(false),
    m_bFaultOccured(FALSE),
    m_pEncodingStream(NULL)
{
    TRACEL( (2,"%5d: Serializer created\n", GetCurrentThreadId()) );
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::~CSoapSerializer()
//
//  parameters:
//
//  description: Destructor
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CSoapSerializer::~CSoapSerializer()
{
    TRACEL( (2,"%5d: Serializer shutdown\n", GetCurrentThreadId()) );
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::Init(VARIANT vIn)
//
//  parameters:
//
//  description:
//        
//  returns: initialize the serializer with a destination stream
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::Init(VARIANT vIn)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif
        HRESULT hr(S_OK);       
#ifndef UNDER_CE
        IUnknown *pTemp(NULL);
#else
        IUnknown *pTemp = NULL;
#endif  
        CAutoRefc<IUnknown> pRefTemp(NULL);
        CAutoRefc<IStream> pIStream(NULL);       
        CAutoRefc<IResponse> pIResponse(NULL); 
        CAutoP<CStreamShell> pStreamShell(NULL);

        if (!m_pEncodingStream)
            m_pEncodingStream = new CEncodingStream();
#ifdef UNDER_CE
           CHK_BOOL(m_pEncodingStream, E_OUTOFMEMORY);
#endif

        if (!(vIn.vt & (VT_UNKNOWN | VT_DISPATCH)))
            return E_INVALIDARG;
        
        // handle byref case
        if (vIn.vt & VT_BYREF)
            pTemp = *(vIn.ppunkVal);
        else
            pTemp = vIn.punkVal;

           if (vIn.vt & VT_DISPATCH)
           {
               // we got a dispatch object here, get the IUnknown in an AutoPtr
            CHK ( pTemp->QueryInterface(IID_IUnknown, (void**)&pRefTemp) );

            pTemp = pRefTemp;
           }

        CHK_BOOL(pTemp, E_INVALIDARG);

        pStreamShell = new CStreamShell();
        CHK_BOOL(pStreamShell, E_OUTOFMEMORY);
    

        // see if we can get a stream
        if (SUCCEEDED (pTemp->QueryInterface(IID_IStream, (void**)&pIStream)) )
        {
            // we got a stream
            CHK(pStreamShell->InitStream(pIStream));
            
            pIStream.PvReturn();
            CHK (m_pEncodingStream->Init(pStreamShell.PvReturn()));
            goto Cleanup;
        }
        
        // see if we can get a stream
        if (SUCCEEDED (pTemp->QueryInterface(IID_IResponse, (void**)&pIResponse)) )
        {
            // we got a stream
            CHK(pStreamShell->InitResponse(pIResponse));
            
            pIResponse.PvReturn();
            CHK (m_pEncodingStream->Init(pStreamShell.PvReturn()));
            goto Cleanup;
        }

        hr = E_FAIL;

    Cleanup:    
        ASSERT (hr == S_OK);
        return hr;
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::Init - Unhandled Exception");
        return E_FAIL;
    }
#endif
}





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::startEnvelope(env_Prefix, enc_style_uri, xml_encoding)
//
//  parameters:
//
//  description: Open a new envelope, can overwrite the default SOAP-ENV
//            prefix, allows definition of an encoding style
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::startEnvelope( 
                            BSTR env_Prefix,
                            BSTR enc_style_uri,
                            BSTR xml_encoding) 
{

#ifdef _DEBUG
    DWORD handle = GetCurrentThreadId();
#endif    
    TRACEL((2, "%5d: Env::startEnvelope -this:%d\n", handle, this));    
   
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif
        HRESULT    hr;
        CHK( Initialized() );
        

        // the only legal state we should be in here is 'initialized'
        CHK_BOOL (chks(initialized), E_FAIL);

        // the current level has to be 0 at this point 
        // if not we already created some stuff before the envelope started
        CHK_BOOL (m_nsHelper.getLevel() == 0, E_FAIL);


        if ( (!xml_encoding) || (wcslen(xml_encoding) == 0))
        {
            CAutoBSTR bstrTemp;

            CHK(bstrTemp.Assign((BSTR) acXMLencdef));
            CHK(_WriterStartDocument(bstrTemp));
        }
        else
        {
            CHK(_WriterStartDocument(xml_encoding));            
        }

        // we are at a new level
        m_nsHelper.PushLevel();

        if ( (env_Prefix) && (wcslen(env_Prefix)) )
        {
            CHK ( m_nsHelper.AddNamespace(env_Prefix, g_pwstrEnvNS) );
        }
        else
        {
            CHK ( m_nsHelper.AddNamespace(g_pwstrEnvPrefix, g_pwstrEnvNS) );
        }

        m_pcSoapPrefix = (m_nsHelper.FindNamespace(g_pwstrEnvNS, NULL))->getPrefix();
        ASSERT (m_pcSoapPrefix);


        // we need this set before we can open an element
        m_eState = envelope_opened;
        m_bFaultOccured = FALSE;

        CHK (startElement((WCHAR *) g_pwstrEnv, (WCHAR *) g_pwstrEnvNS, enc_style_uri,  m_pcSoapPrefix) );
        
        // make sure we have an attribute for this namespace definition 
        CHK (m_ElementStack.AddAttribute((WCHAR *)g_pwstrXmlns, (WCHAR *)g_pwstrEmpty, m_pcSoapPrefix, (WCHAR *)g_pwstrEnvNS) );

    Cleanup:
        TRACEL((2, "%5d: Env::startEnvelope --- exit, HR=%x", handle, hr));    
        ASSERT(hr==S_OK);
        return (hr);
        
#ifndef CE_NO_EXCEPTIONS
    }
    
    catch (...)
        {
        ASSERTTEXT (FALSE, "CSoapSerializer::startEnvelope - Unhandled Exception");
        return E_FAIL;
        }
#endif  
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::endEnvelope()
//
//  parameters:
//
//  description: end of envelope
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::endEnvelope( void )
{
#ifndef CE_NO_EXCEPTIONS
    try
#else
    __try
#endif    
    {
        HRESULT    hr;
        
        CHK (Initialized());
        CHK ( FlushElement() ); 


        // check the state
        CHK_BOOL (chks(envelope_opened) || chks(header_closed) || chks(body_closed), E_FAIL);

        // need to manipulate the state, since we can not write a 'endElement'
        // in the header_closed or body_closed state
        m_eState = envelope_opened;

        // we are ending the document
        CHK(endElement());
        
        m_nsHelper.PopLevel();
        // we now have to be at level 0
        CHK_BOOL(m_nsHelper.getLevel() == 0, E_FAIL);
        
        CHK(_WriterEndDocument());
        m_eState = envelope_closed;

        // release the encoding stream, to close the stream
        delete (CEncodingStream*) (m_pEncodingStream.PvReturn());
        
    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);     
    }
#ifndef CE_NO_EXCEPTIONS       
    catch (...)
#else
    __except(1)
#endif   
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::endEnvelope - Unhandled Exception");
        return E_FAIL;
    }

}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::startHeader(BSTR enc_style_uri)
//
//  parameters:
//
//  description: Start header in soap envelope
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::startHeader( BSTR enc_style_uri )
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif
        HRESULT    hr;
        
        CHK (Initialized());
        CHK (FlushElement() ); 

        // check the state
        CHK_BOOL (chks(envelope_opened), E_FAIL);
        m_eState = header_opened;

        CHK (startElement((WCHAR *) g_pwstrHeader, (WCHAR *)g_pwstrEnvNS, enc_style_uri,  m_pcSoapPrefix) );


    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS        
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::startHeader - Unhandled Exception");
        return E_FAIL;
    }
#endif
}


        
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::startHeaderElement(    BSTR name, BSTR ns_uri, int  mustUnderstand,
//                                                  BSTR actor_uri, BSTR enc_style_uri, BSTR prefix)
//
//  parameters:
//
//  description: adding a header element, will create namespace if needed
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::startHeaderElement( 
                BSTR name,
                BSTR ns_uri,
                int  mustUnderstand,
                BSTR actor_uri,
                BSTR enc_style_uri,
                BSTR prefix)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif
        HRESULT    hr;
#ifndef UNDER_CE        
        WCHAR *    pPrefix;
#endif
        CHK (Initialized());
        CHK (FlushElement() ); 

        // check the state
        CHK_BOOL (chks(header_opened), E_FAIL);

        CHK (startElement( name , ns_uri, enc_style_uri,prefix) );

        if (mustUnderstand == 1)
            {
            CHK (m_ElementStack.AddAttribute(m_pcSoapPrefix, (WCHAR *)g_pwstrEnvNS, (WCHAR *)g_pwstrMustUnderstand, L"1") );
        }

        if ( (actor_uri != NULL) && (wcslen((WCHAR*)actor_uri) > 0))
            {
            CHK (m_ElementStack.AddAttribute(m_pcSoapPrefix, (WCHAR *)g_pwstrEnvNS, (WCHAR *)g_pwstrActor, (WCHAR *)actor_uri) );
        }


    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS       
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::startHeaderElement - Unhandled Exception");
        return E_FAIL;
    }
#endif
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::endHeaderElement()
//
//  parameters:
//
//  description: close a header element, no parameter needed
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::endHeaderElement( void)
{
    return endElement();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::endHeader()
//
//  parameters:
//
//  description: creates the closing soap-header tag 
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::endHeader( void)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif    
        HRESULT hr = S_OK;
    
        // check the state
        CHK_BOOL (chks(header_opened), E_FAIL);
        CHK(endElement());
    
        m_eState = header_closed;
    
    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS        
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::endHeader - Unhandled Exception");
        return E_FAIL;
    }
#endif    
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::startBody( BSTR enc_style_uri)
//
//  parameters:
//
//  description: signals the start of the buddy and allows definition of encoding
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::startBody( 
    BSTR enc_style_uri)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif    
        HRESULT    hr;
    
        CHK (Initialized());
        CHK (FlushElement() ); 

        // check the state
        CHK_BOOL (chks(envelope_opened) || chks(header_closed), E_FAIL);
        m_eState = body_opened;
        
        CHK (startElement( (WCHAR *) g_pwstrBody, (WCHAR *)g_pwstrEmpty,  enc_style_uri, m_pcSoapPrefix) );

    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS        
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::startBody - Unhandled Exception");
        return E_FAIL;
    }
#endif    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::endBody()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::endBody( void)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif    
        HRESULT hr = S_OK;
    
        // check the state
        CHK_BOOL (chks(body_opened), E_FAIL);
        CHK(endElement());
    
        m_eState = body_closed;
    
    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS       
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::endBody - Unhandled Exception");
        return E_FAIL;
    }
#endif    
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::startElement(BSTR name, BSTR ns_uri,
//                                          BSTR enc_style_uri, BSTR prefix)
//
//  parameters:
//
//  description: start a element, including namespace and encoding definition
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::startElement( 
    BSTR name,
    BSTR ns_uri,
    BSTR enc_style_uri,
    BSTR prefix)

{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif    
        HRESULT    hr;
        WCHAR * pPrefix;

        CHK (Initialized());
        CHK (FlushElement() ); 
        TRACEL((2, "%5d: Env::startElement\n", GetCurrentThreadId() ));    
        CHK_BOOL (chks(envelope_opened) || chks(header_opened) || chks(body_opened), E_FAIL);

        if (!prefix)
            prefix = (BSTR) g_pwstrEmpty;
        if (!ns_uri)
            ns_uri = (BSTR) g_pwstrEmpty;
        if (!name)
            name = (BSTR) g_pwstrEmpty;
        if (!enc_style_uri)
            enc_style_uri = (BSTR) g_pwstrNone;

        m_nsHelper.PushLevel();
        
        CHK(PushElement(prefix, name, ns_uri));
        pPrefix = NamespaceHandler(prefix, ns_uri);
        // in any case we have a prefix now, additionaly a namespace declaration
        // has been added to the attribute list by the previous call if it was
        // necessary.

        if (wcscmp(prefix, pPrefix))
        {
            // this should only happen if prefix was ""
            ASSERT (wcslen(prefix) == 0);
            CElementStackEntry * pele;

            pele = PeekElement();
            CHK(pele->setPrefix(pPrefix));
        }

        if (wcsicmp(g_pwstrNone, enc_style_uri))
        {
            // the string is not "NONE", we have to output some attribute
            if (wcsicmp(g_pwstrStandard, enc_style_uri) == NULL)
            {
                // we want the standard encoding style here
                enc_style_uri = (BSTR) g_pwstrEncStyleNS;
            }
            
            // now we have encoding style which is some kind of string, a URL set above, 
            // or even an empty string
            //
            // we can just output it
               CHK (m_ElementStack.AddAttribute(m_pcSoapPrefix, (WCHAR *)g_pwstrEnvNS, (WCHAR *)g_pwstrEncStyle, enc_style_uri) );
        }                    

    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS       
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::startElement - Unhandled Exception");
        return E_FAIL;
    }
#endif    
}





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::endElement()
//
//  parameters:
//
//  description: no parameter needed, all information is known internally
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::endElement( void )
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif    
        HRESULT    hr;
        CAutoP<CElementStackEntry> pele(NULL);
        
        CHK (Initialized());
        CHK (FlushElement() ); 
        TRACEL((2, "%5d: Env::endElement\n", GetCurrentThreadId() ));    
        CHK_BOOL (chks(envelope_opened) || chks(header_opened) || chks(body_opened), E_FAIL);

        pele = PopElement();

        CHK_BOOL (pele != NULL, E_FAIL);
        
        CHK(_WriterEndElement(
            pele->getPrefix(),wcslen((WCHAR *)pele->getPrefix()), 
            pele->getName(),  wcslen((WCHAR *)pele->getName()),
            pele->getQName(), wcslen((WCHAR *)pele->getQName()) ) 
            );
        
        CHK (m_nsHelper.PopLevel());
        
    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
        
#ifndef CE_NO_EXCEPTIONS       
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::endHeaderElement - Unhandled Exception");
        return E_FAIL;
    }
#endif    
}





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::SoapAttribute( BSTR name, BSTR ns_uri,
//                                            BSTR value, BSTR prefix)
//
//  parameters:
//
//  description: add Attribute information to the current element, 
//                the has to happen after the call to startElement.
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::SoapAttribute( 
    BSTR name,
    BSTR ns_uri,
    BSTR value,
    BSTR prefix)
{
#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif    
        HRESULT hr = S_OK;
        WCHAR *    pPrefix;

        CHK_BOOL (chks(envelope_opened) || chks(header_opened) || chks(body_opened), E_FAIL);

        if (!prefix)
            prefix = (BSTR) g_pwstrEmpty;
        if (!ns_uri)
            ns_uri = (BSTR) g_pwstrEmpty;

        pPrefix = NamespaceHandler(prefix, ns_uri);
        // in any case we have a prefix now, additionaly a namespace declaration
        // has been added to the attribute list by the previous call if it was
        // necessary.

        // the attribute is added
        CHK (m_ElementStack.AddAttribute(pPrefix, ns_uri, name, value) );

    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS        
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::SoapAttribute - Unhandled Exception");
        return E_FAIL;
    }
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::SoapNamespace( BSTR prefix, BSTR ns_uri)
//
//  parameters:
//
//  description: 
//
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CSoapSerializer::SoapNamespace( 
                BSTR prefix,
                BSTR ns_uri)
{
#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif    
        HRESULT hr = S_OK;

        CHK_BOOL (chks(envelope_opened) || chks(header_opened) || chks(body_opened), E_FAIL);

        // add namespace declaration to the list
        NamespaceHandler(prefix, ns_uri);

    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS       
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::SoapAttribute - Unhandled Exception");
        return E_FAIL;
    }
#endif    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::SoapDefaultNamespace( BSTR ns_uri)
//
//  parameters:
//
//  description: 
//                
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::SoapDefaultNamespace( 
                BSTR ns_uri)
{
#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif    
        HRESULT hr = S_OK;
        CElementStackEntry    * pele;
        
        CHK_BOOL (chks(envelope_opened) || chks(header_opened) || chks(body_opened), E_FAIL);

        pele = PeekElement();
        ASSERT (pele);
        
        CHK (pele->setDefaultNamespace(ns_uri));
        CHK (m_ElementStack.AddAttribute((WCHAR *)g_pwstrEmpty, (WCHAR *)g_pwstrEmpty, (WCHAR*)g_pwstrXmlns, ns_uri) );
        
    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS        
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::SoapAttribute - Unhandled Exception");
        return E_FAIL;
    }
#endif  
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::writeString(BSTR string)
//
//  parameters:
//
//  description: 
//                
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::writeString( 
                BSTR string)
{
#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif    
        HRESULT    hr = S_OK;

        ASSERT (string);
        if (!string)
            string = (BSTR)g_pwstrEmpty;

        CHK (Initialized());
        CHK (FlushElement() ); 
        
        CHK_BOOL (chks(envelope_opened) || chks(header_opened) || chks(body_opened), E_FAIL);
        
        CHK(_WriterCharacters(string, wcslen(string)) );

    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS       
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::writeString - Unhandled Exception");
        return E_FAIL;
    }
#endif    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::writeBuffer(long len, char * buffer)
//
//  parameters:
//
//  description: 
//                
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////       
HRESULT STDMETHODCALLTYPE CSoapSerializer::writeBuffer( 
           long len,
           char * buffer)
{
#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif    
        HRESULT                hr(S_OK);
#ifndef UNDER_CE        
        ULONG                 ret;
#endif
        ASSERT (buffer);

        

        CHK (Initialized());
        CHK (FlushElement() ); 
        CHK_BOOL (chks(envelope_opened) || chks(header_opened) || chks(body_opened), E_FAIL);

        if (m_bElementOpen)
        {
            // first end the element
            CHK (m_pEncodingStream->write((BSTR)g_pwstrClosing));
            m_bElementOpen = false;        
        }

        CHK (m_pEncodingStream->writeThrough((byte*)buffer, len));

    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS       
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::writeBuffer - Unhandled Exception");
        return E_FAIL;
    }
#endif    
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::startFault(BSTR faultcode, BSTR faultstring,
//                                        BSTR faultactor, BSTR faultcodeNS)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::startFault( 
    BSTR faultcode,
    BSTR faultstring,
    BSTR faultactor,
    BSTR faultcodeNS)
{
#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif 
        HRESULT    hr;
        WCHAR * pPrefix = L"";
        
        ASSERT (faultcode);
        ASSERT (faultstring);
        
        CHK (Initialized());
        CHK (FlushElement() ); 
        m_bFaultOccured = TRUE;
        
        CHK_BOOL (chks(body_opened), E_FAIL);

        CHK (startElement((BSTR) g_pwstrFault, (BSTR) g_pwstrEmpty, (BSTR) g_pwstrNone, m_pcSoapPrefix) );
            CHK (startElement((BSTR) g_pwstrFaultcode, (BSTR) g_pwstrEmpty, (BSTR) g_pwstrNone,  0) );

            if ( (faultcodeNS == NULL) || (wcslen(faultcodeNS) == 0) )
            {
                // the namespace we are looking at is empty, we are going
                // with the standard soap namespace
                pPrefix = m_pcSoapPrefix;
            }
            else
            {
                pPrefix = NamespaceHandler((WCHAR *) g_pwstrEmpty, faultcodeNS);
                // in any case we have a prefix now, additionaly a namespace declaration
                // has been added to the attribute list by the call if it was
                // necessary.
            }
            // now write something like:
            //          <pPrefix>:<faultcode>
            // as the actual faultcode
            CHK (writeString(pPrefix));
            CHK (writeString(L":"));
            CHK (writeString(faultcode) );
            CHK (endElement() );
            
            CHK (startElement((BSTR) g_pwstrFaultstring, (BSTR) g_pwstrEmpty, (BSTR) g_pwstrNone, 0) );
            CHK (writeString(faultstring) );
            CHK (endElement() );
            if  ( (faultactor) && (wcslen(faultactor)) )
            {
            CHK (startElement((BSTR) g_pwstrFaultactor, (BSTR) g_pwstrEmpty, (BSTR) g_pwstrNone, 0) );
            CHK (writeString(faultactor) );
            CHK (endElement() );
            }
            
    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::startFault - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::startFaultDetail(BSTR enc_style_uri)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::startFaultDetail (  
                BSTR enc_style_uri)
{
    HRESULT hr = S_OK;
    
    CHK_BOOL (chks(body_opened), E_FAIL);
    
    CHK(startElement((BSTR) g_pwstrDetail, (BSTR) g_pwstrEmpty, enc_style_uri, 0));
Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::endFaultDetail()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::endFaultDetail( void)
{
    HRESULT hr = S_OK;
    
    CHK_BOOL (chks(body_opened), E_FAIL);
    
    CHK(endElement());
Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::endFault()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::endFault(void)
{
    HRESULT hr = S_OK;
    
    CHK_BOOL (chks(body_opened), E_FAIL);
    
    CHK(endElement());
Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}







/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::reset()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::reset(void)
{
    HRESULT hr = S_OK;

    if (m_pEncodingStream)
        m_pEncodingStream->Reset();

    m_pcSoapPrefix = NULL;
    m_pcDefaultNamespaceURI = NULL;
    CHK(m_nsHelper.reset());
    CHK(m_ElementStack.reset());
    
    m_eState = created;
    m_bElementOpen = false;
        
Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::writeXML( BSTR string)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CSoapSerializer::writeXML( 
                BSTR string)
{
#ifndef CE_NO_EXCEPTIONS
    try 
    {
#endif 
        HRESULT    hr = S_OK;

        ASSERT (string);
        if (!string)
            string = (BSTR)g_pwstrEmpty;

        CHK (Initialized());
        CHK (FlushElement() ); 
        
        CHK_BOOL (chks(envelope_opened) || chks(header_opened) || chks(body_opened), E_FAIL);

        if (m_bElementOpen)
        {
            // first end the element
            CHK (m_pEncodingStream->write((BSTR)g_pwstrClosing));
            m_bElementOpen = false;        
        }
        
        CHK (m_pEncodingStream->write((BSTR)string, wcslen(string)));

    Cleanup:
        ASSERT(hr==S_OK);
        return (hr);
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::writeXML - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::getPrefixForNamespace( BSTR ns, BSTR *prefix)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CSoapSerializer::getPrefixForNamespace( 
            BSTR ns,
            BSTR *prefix)
{
#ifndef CE_NO_EXCEPTIONS
try 
    {
#endif 
        HRESULT    hr = S_OK;
        CNamespaceListEntry    * pEntry = NULL;
    
        ASSERT (ns);
        CHK_BOOL(ns, E_INVALIDARG);
        CHK_BOOL(prefix, E_INVALIDARG);
        CHK_BOOL(wcslen(ns), E_INVALIDARG);
        
        *prefix = NULL;
        
        CHK (Initialized());

        // give as an entry matching this namespace
        pEntry = m_nsHelper.FindNamespace(ns, pEntry);

        // if this entry doesn't exist, we return e_fail and set the return to an empty string
        CHK_BOOL(pEntry, E_FAIL);

        // return the value
        *prefix = ::SysAllocString(pEntry->getPrefix());
        CHK_BOOL(*prefix, E_OUTOFMEMORY);

    Cleanup:
        return (hr);
#ifndef CE_NO_EXCEPTIONS
    }
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::getPrefix - Unhandled Exception");
        return E_FAIL;
    }
#endif 

}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::get_EncodingStream(IUnknown **ppStream)
//
//  parameters:
//
//  description:
//
//  returns a reference to the EncodingSream to the caller
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::get_EncodingStream(IUnknown **ppUnknown)
{
#ifndef CE_NO_EXCEPTIONS
    try
    {
#endif 
        HRESULT hr = S_OK;

        CHK_BOOL (ppUnknown, E_INVALIDARG);
        *ppUnknown = NULL; 

        return E_NOTIMPL;

    Cleanup:
        ASSERT (hr == S_OK);
        return hr;
#ifndef CE_NO_EXCEPTIONS
    } 
    catch (...)
    {
        ASSERTTEXT (FALSE, "CSoapSerializer::get_Stream - Unhandled Exception");
        return E_FAIL;
    }
#endif 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::Initialized()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::Initialized()
    {

    HRESULT             hr(S_OK);
#ifndef UNDER_CE    
    ULONG                 ret;
#endif

    if (m_eState > initialized)    
    {
        goto Cleanup;
    }

    // add the default namespace to the list
    m_pcSoapPrefix = (WCHAR *)g_pwstrEnvNS;

    // is the output stream set
    CHK_BOOL ( SUCCEEDED(m_pEncodingStream->IsInitialized()), E_FAIL);

    if (!m_pEncodingStream)
        m_pEncodingStream = new CEncodingStream();
#ifdef CE_NO_EXCEPTIONS
    CHK_BOOL (m_pEncodingStream, E_OUTOFMEMORY);
#endif

    m_eState = initialized;
    
Cleanup:

    ASSERT(hr==S_OK);
    
    return (hr);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::PushElement(WCHAR * pPrefix, WCHAR * pName, WCHAR * pnsURI)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::PushElement(WCHAR * pPrefix, WCHAR * pName, WCHAR * pnsURI)

{
    HRESULT                     hr = S_OK;
    CAutoP<CElementStackEntry>     pele(NULL);    
#ifndef UNDER_CE
    CElementStackEntry *		pCurrentHead(NULL);
#else
    CElementStackEntry *		pCurrentHead = NULL;
#endif



    pele = new CElementStackEntry();
    CHK_BOOL (pele != NULL, E_OUTOFMEMORY);
    
    CHK (pele->Init());
    CHK (pele->setName(pName)) ;
    CHK (pele->setPrefix(pPrefix)) ;
    CHK (pele->setURI(pnsURI));
    
    
    pCurrentHead = PeekElement();
    if (pCurrentHead)
    {
        pele->setDefaultNamespace(pCurrentHead->getDefaultNamespace());
    }
    else
    {
        pele->setDefaultNamespace(g_pwstrEmpty);
    }

    m_ElementStack.Push(pele.PvReturn());

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::PopElement()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CElementStackEntry * CSoapSerializer::PopElement(void)
{
    return m_ElementStack.Pop();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::PeekElement()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CElementStackEntry * CSoapSerializer::PeekElement(void)
{
    return m_ElementStack.Peek();
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::FlushElement()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::FlushElement(void)
{
    HRESULT hr                     = S_OK;
    CElementStackEntry * pEle    = NULL;
    
    pEle = PeekElement();
    if ( (!pEle) ||  (pEle->getIsSerialized()) )
        return (S_OK);
        
    CHK (pEle->FixNamespace(&m_nsHelper));

    CHK (pEle->FlushElement(this));

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSoapSerializer::NamespaceHandler(WCHAR * prefix, WCHAR* ns_uri)
//
//  parameters:
//
//  description:
//              this function takes a prefix and a namespace URI
//              it returns the prefix matching the namespace URI or the input parameter
//              prefix if the function did not pass in a uri
//
//              after the function the namespace is registered
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////

WCHAR * CSoapSerializer::NamespaceHandler(WCHAR * prefix, WCHAR* ns_uri)
{
    HRESULT hr    = S_OK;
    WCHAR * pPrefix = NULL;
    if ((!ns_uri) || (wcslen(ns_uri) == 0) )
    {
        // no namespace uri, whatever is in prefix will do
        return prefix;
    }

    
    // there is a uri on the request. first lets see if this URI is 
    // already a URI of a given namespace
    CNamespaceListEntry    * pEntry = NULL;
        
    while (pEntry = m_nsHelper.FindNamespace(ns_uri, pEntry))
    {
        // a namespace with this URI is already defined
        if ( (prefix) && (wcslen(prefix) ) )
        {
            // there is also a prefix, is this prefix for the current 
            // call identical to the one defined in the namespace ?
            if (wcscmp(prefix, pEntry->getPrefix()) == NULL)
            {
                // namespace and URI are already defined!!
                return pEntry->getPrefix();
            }
            // the prefix(es) don't match
            // perhaps there is another matching one ?
            continue;
        }
        // we have a matching entry, but we do not have a prefix defined
        // the prefix in the matching entry is the one we care about
        return pEntry->getPrefix();
    }

    CHK ( m_nsHelper.AddNamespace(prefix, ns_uri) );
    
    pPrefix = NamespaceHandler(prefix, ns_uri);
    
    CHK (m_ElementStack.AddAttribute((WCHAR *)g_pwstrXmlns, (WCHAR*)g_pwstrEmpty, pPrefix, ns_uri) );


    return pPrefix;
    
Cleanup:
    ASSERT(hr==S_OK);
    return (prefix);
}    








/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapSerializer::_WriterStartDocument(TCHAR *pchEncoding)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::_WriterStartDocument(BSTR pchEncoding)
{

    HRESULT hr = E_FAIL;
    WCHAR       *pchXMLStart1 = L"<?xml version=\"1.0\" encoding=\"";
    WCHAR       *pchXMLStart2 = L"\" standalone=\"no\"?>"; 

    CHK(m_pEncodingStream->setEncoding(pchEncoding));

    CHK (m_pEncodingStream->write(pchXMLStart1));
    CHK (m_pEncodingStream->write(pchEncoding));
    CHK (m_pEncodingStream->write(pchXMLStart2));

Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapSerializer::_WriterEndDocument(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::_WriterEndDocument(void)
{
    HRESULT hr = S_OK;
    // flush everything
    m_pEncodingStream->flush();

    if (m_bFaultOccured)
    {
        // in an error case set the error code
        CHK (m_pEncodingStream->setFaultCode());
        // for good measurement flush again
        m_pEncodingStream->flush();
    }
    
 Cleanup:
    return(hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////




/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapSerializer::_WriterStartElement(....
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::_WriterStartElement(const wchar_t *pwchNamespaceUri, 
                                            int cchNamespaceUri, 
                                            const wchar_t * pwchLocalName, 
                                            int cchLocalName,
                                            const wchar_t * pwchQName,
                                            int cchQName)
{

    HRESULT hr = E_FAIL;  
    WCHAR        *pchXMLStart = L"<"; 

    

    if (m_bElementOpen)
    {
        // end whatever was there before...
        CHK (m_pEncodingStream->write((BSTR)g_pwstrClosing));
        m_bElementOpen = false;        
    }
    
    CHK (m_pEncodingStream->write(pchXMLStart));
    CHK (m_pEncodingStream->write((BSTR)pwchQName)); 
    
    m_bElementOpen = true;
    
Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapSerializer::_WriterEndElement(const wchar_t *pwchNamespaceUri, 
//                          int cchNamespaceUri, const wchar_t * pwchLocalName, int cchLocalName,
//                          const wchar_t * pwchQName, int cchQName)
// 
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::_WriterEndElement(const wchar_t *pwchNamespaceUri, 
                                            int cchNamespaceUri, 
                                            const wchar_t * pwchLocalName, 
                                            int cchLocalName,
                                            const wchar_t * pwchQName,
                                            int cchQName)
{
    HRESULT hr = E_FAIL;  
    WCHAR        *pchXMLFullEnd = L"/>"; 
    WCHAR        *pchXMLStart = L"</";     
      
    if (!m_bElementOpen)
    {
        // need full element 
        CHK (m_pEncodingStream->write(pchXMLStart));
        CHK (m_pEncodingStream->write((BSTR)pwchQName));         
        CHK (m_pEncodingStream->write((BSTR)g_pwstrClosing));
        
        m_bElementOpen = true;
    }
    else
    {
        CHK (m_pEncodingStream->write(pchXMLFullEnd));
    }

    
    m_bElementOpen = false;
    
Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
    
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapSerializer::_WriterCharacters(const WCHAR* pchChars, int cbChars)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer::_WriterCharacters(const WCHAR* pchChars, int cbChars)
{
    HRESULT     hr(S_OK);
#ifndef UNDER_CE
    UINT        cbIn, cbOut;
#endif    
    ULONG    cTextGood = 0;
    ULONG    cEntityLen = 0;
    const WCHAR * pwchTextWalk = pchChars;
    WCHAR * pwchEntity = NULL;
    WCHAR * pwchEntities = L"&amp;&lt;&gt;&apos;&quot;";
    

    if (m_bElementOpen)
    {
        // first end the element
        CHK (m_pEncodingStream->write((BSTR)g_pwstrClosing));
        m_bElementOpen = false;        
    }
    
    while (cbChars--)
        {
        switch (*pwchTextWalk)
            {
        case '&':
            pwchEntity = pwchEntities;
            cEntityLen = 5;
            break;
        case '<':
            pwchEntity = pwchEntities + 5;
            cEntityLen = 4;
            break;
        case '>':
            pwchEntity = pwchEntities + 9;
            cEntityLen = 4;
            break;
        case '\'':
            pwchEntity = pwchEntities + 13;
            cEntityLen = 6;
            break;
        case '\"':
            pwchEntity = pwchEntities + 19;
            cEntityLen = 6;
            break;
        default:
            cTextGood++;
            }

        if (pwchEntity)
            {
            CHK(m_pEncodingStream->write((BSTR)pchChars, cTextGood));
            pchChars += cTextGood + 1;
            cTextGood = 0;
            CHK(m_pEncodingStream->write(pwchEntity, cEntityLen));
            pwchEntity = NULL;
            }

        pwchTextWalk++;
        }

    CHK(m_pEncodingStream->write((BSTR)pchChars));
    
Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CSoapSerializer:: _WriterAddAttribute(WCHAR *pchURI,WCHAR *pchLocalName, 
//                                              WCHAR *pchQName, WCHAR *pchType, WCHAR *pchValue)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSoapSerializer:: _WriterAddAttribute(WCHAR *pchURI,WCHAR *pchLocalName, WCHAR *pchQName, WCHAR *pchType, WCHAR *pchValue)
{
    HRESULT hr;
    WCHAR    *pchSpace=L" ";
    WCHAR    *pchEqual=L"=\"";
    WCHAR    *pchQuote=L"\"";
#ifndef UNDER_CE    
    int     cbChars;
#endif

    ASSERT(m_bElementOpen);
    
    CHK (m_pEncodingStream->write(pchSpace));
    CHK (m_pEncodingStream->write(pchQName));    
    CHK (m_pEncodingStream->write(pchEqual));
    CHK (m_pEncodingStream->write(pchValue));        
    CHK (m_pEncodingStream->write(pchQuote));
 
Cleanup:
    ASSERT(hr==S_OK);
    return (hr);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////            


