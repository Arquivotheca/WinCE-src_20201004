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
// File:    soapserializer.h
// 
// Contents:
//
//  Header File 
//
//		ISoapSerializer Interface describtion
//	
//
//-----------------------------------------------------------------------------
#ifndef __SOAPSERIALIZER_H_INCLUDED__
	#define __SOAPSERIALIZER_H_INCLUDED__

	#include "nspace.h"
	#include "elementstack.h"
	#include "soapstream.h"

	class CSoapSerializer:  public CDispatchImpl<ISoapSerializer>
	{
		public:
			CSoapSerializer();
			~CSoapSerializer();
				
			
		    DECLARE_INTERFACE_MAP;

			HRESULT STDMETHODCALLTYPE Init( 
				VARIANT vDestination);
	
       		HRESULT STDMETHODCALLTYPE startEnvelope( 
            	BSTR env_Prefix,
            	BSTR enc_style_uri,
            	BSTR xml_encoding);
        
        	HRESULT STDMETHODCALLTYPE endEnvelope( void );
        
        	HRESULT STDMETHODCALLTYPE startHeader( 
        		BSTR enc_style_uri);
        	        
        	HRESULT STDMETHODCALLTYPE startHeaderElement( 
            	BSTR name,
            	BSTR ns_uri,
            	int  mustUnderstand,
            	BSTR actor_uri,
            	BSTR enc_style_uri,
            	BSTR prefix);
        
        	HRESULT STDMETHODCALLTYPE endHeaderElement( void);
        
        	HRESULT STDMETHODCALLTYPE endHeader( void);
        
        	HRESULT STDMETHODCALLTYPE startBody( 
	            BSTR enc_style_uri);
        
        	HRESULT STDMETHODCALLTYPE endBody( void);
        
        	HRESULT STDMETHODCALLTYPE startElement( 
            	BSTR name,
            	BSTR ns_uri,
            	BSTR enc_style_uri,
            	BSTR prefix);
        
        	HRESULT STDMETHODCALLTYPE endElement( void );
        
        	HRESULT STDMETHODCALLTYPE SoapAttribute( 
            	BSTR name,
            	BSTR ns_uri,
            	BSTR value,
            	BSTR prefix);
     	
        	HRESULT STDMETHODCALLTYPE SoapNamespace( 
            	BSTR prefix,
            	BSTR ns_uri);

        	HRESULT STDMETHODCALLTYPE SoapDefaultNamespace( 
            	BSTR ns_uri);

        	HRESULT STDMETHODCALLTYPE writeString( 
            	BSTR string);
        
        	HRESULT STDMETHODCALLTYPE writeBuffer( 
            	long len,
            	char * buffer);

        	HRESULT STDMETHODCALLTYPE startFault( 
            	BSTR faultcode,
            	BSTR faultstring,
            	BSTR faultactor,
            	BSTR faultcodeNS);
        
        	HRESULT STDMETHODCALLTYPE startFaultDetail ( 
	            BSTR enc_style_uri);
        	
        	HRESULT STDMETHODCALLTYPE endFaultDetail( void);
        
        	HRESULT STDMETHODCALLTYPE endFault( void);
        	
        	HRESULT STDMETHODCALLTYPE reset( void); 
        
        	HRESULT STDMETHODCALLTYPE writeXML( 
            	BSTR string);
        	
            HRESULT STDMETHODCALLTYPE getPrefixForNamespace( 
                BSTR ns_string,
                BSTR *ns_prefix);
            
        	HRESULT STDMETHODCALLTYPE get_EncodingStream( 
            	IUnknown **ppStream);
  	
		public:
            HRESULT _WriterStartDocument(BSTR pchEncoding);
            HRESULT _WriterEndDocument(void);
            HRESULT _WriterEndElement(const wchar_t *pwchNamespaceUri, 
                                            int cchNamespaceUri, 
                                            const wchar_t * pwchLocalName, 
                                            int cchLocalName,
                                            const wchar_t * pwchQName,
                                            int cchQName);
            HRESULT _WriterCharacters(const WCHAR* pchChars, int cbChars);
            HRESULT _WriterAddAttribute(WCHAR *pchURI,WCHAR *pchLocalName, WCHAR *pchQName, WCHAR *pchType, WCHAR *pchValue);
            HRESULT _WriterStartElement(const wchar_t *pwchNamespaceUri, 
                                            int cchNamespaceUri, 
                                            const wchar_t * pwchLocalName, 
                                            int cchLocalName,
                                            const wchar_t * pwchQName,
                                            int cchQName);
                                    
		private:
			HRESULT Initialized();
			HRESULT PushElement(WCHAR * pPrefix, WCHAR * pName, WCHAR * pnsURI);
			CElementStackEntry * PopElement(void);
			CElementStackEntry * PeekElement(void);
			HRESULT FlushElement(void);
			WCHAR * NamespaceHandler(WCHAR * prefix, WCHAR* ns_uri);
			HRESULT SAXAttribute(CElement * pele);
			HRESULT FixNamespace(CElement * pEle, WCHAR * pCDN);


			WCHAR *										m_pcSoapPrefix;
			WCHAR *										m_pcDefaultNamespaceURI;
			CNamespaceHelper							m_nsHelper;
			CElementStack								m_ElementStack;
			CAutoP<CEncodingStream>  					m_pEncodingStream;

			enum SerializerState 
				{
				created,
				initialized,
				envelope_opened,
				header_opened,
				header_closed,
				body_opened,
				body_closed,
				envelope_closed,
				done,
				};
			SerializerState 							m_eState;
            
			bool             							m_bElementOpen;
			bool m_bFaultOccured;   // set to true if SoapFault is inside the message
            
	};




#endif


// End of File


