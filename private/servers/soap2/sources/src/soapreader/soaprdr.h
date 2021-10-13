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
// File:    soaprdr.h
// 
// Contents:
//
//  Header File 
//
//		ISoapReader Interface describtion
//	
//
//-----------------------------------------------------------------------------
#ifndef __SOAPREADER_H_INCLUDED__
	#define __SOAPREADER_H_INCLUDED__

		class CSoapReader: public CDispatchImpl<ISoapReader>
		{
		public:
			CSoapReader();
			~CSoapReader();
			
		    DECLARE_INTERFACE_MAP;

		public:
			HRESULT STDMETHODCALLTYPE load(
				VARIANT xmlSource, 
				BSTR    bstrSoapAction,
				VARIANT_BOOL *isSuccessful);
				
        	HRESULT STDMETHODCALLTYPE loadXML(
        		BSTR bstrXML, 
        		VARIANT_BOOL *isSuccessful);
        		
 			HRESULT STDMETHODCALLTYPE get_DOM(
 				IXMLDOMDocument **pIXMLDOMDocument);

 			HRESULT STDMETHODCALLTYPE get_Envelope( 
             	IXMLDOMElement **ppElement);
        
         	HRESULT STDMETHODCALLTYPE get_Body( 
             	IXMLDOMElement **ppElement);
        
	        HRESULT STDMETHODCALLTYPE get_Header( 
        	     IXMLDOMElement **ppElement);
        
    	    HRESULT STDMETHODCALLTYPE get_Fault( 
            	 IXMLDOMElement **ppElement);
        
	        HRESULT STDMETHODCALLTYPE get_FaultString( 
    	         IXMLDOMElement **ppElement);
        
        	HRESULT STDMETHODCALLTYPE get_FaultCode( 
             	IXMLDOMElement **ppElement);
        
         	HRESULT STDMETHODCALLTYPE get_FaultActor( 
             	IXMLDOMElement **ppElement);
        
         	HRESULT STDMETHODCALLTYPE get_FaultDetail( 
             	IXMLDOMElement **ppElement);
        
         	HRESULT STDMETHODCALLTYPE get_HeaderEntry( 
            	BSTR LocalName,
            	BSTR NamespaceURI,
             	IXMLDOMElement **ppElement);
        
         	HRESULT STDMETHODCALLTYPE get_MustUnderstandHeaderEntries( 
             	IXMLDOMNodeList **ppNodeList);
        
         	HRESULT STDMETHODCALLTYPE get_HeaderEntries( 
             	IXMLDOMNodeList **ppNodeList);
        
         	HRESULT STDMETHODCALLTYPE get_BodyEntries( 
             	IXMLDOMNodeList **ppNodeList);
        
         	HRESULT STDMETHODCALLTYPE get_BodyEntry( 
            	BSTR LocalName,
            	BSTR NamespaceURI,
             	IXMLDOMElement **ppElement);
        
         	HRESULT STDMETHODCALLTYPE get_RPCStruct( 
             	IXMLDOMElement **ppElement);
        
         	HRESULT STDMETHODCALLTYPE get_RPCParameter( 
            	BSTR LocalName,
            	BSTR NamespaceURI,
             	IXMLDOMElement **ppElement);
        
         	HRESULT STDMETHODCALLTYPE get_RPCResult( 
             	IXMLDOMElement **ppElement);
                
             HRESULT STDMETHODCALLTYPE get_soapAction(
             			BSTR *pbstrSoapAction);						
                
        

		protected:

		private:
			CAutoRefc<IXMLDOMDocument2>	m_pDOMDoc;
            CAutoBSTR                   m_bstrSoapAction;

		
			HRESULT FillDomMemberVar(void);

			HRESULT DOMElementLookup(
	            const WCHAR * pSelectionNS,
        	    const WCHAR * pXpath,
            	IXMLDOMElement **ppElement);

			HRESULT DOMNodeListLookup(
	            const WCHAR * pSelectionNS,
        	    const WCHAR * pXpath,
            	IXMLDOMNodeList **ppElement);
        
            HRESULT RetrieveSoapAction(IRequest *pRequest);

            HRESULT getFaultXXX( 
                IXMLDOMElement **ppElement,
                const WCHAR * element);
            
           	
		};




#endif


// End of File


