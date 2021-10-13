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
// File:    encstream.h
// 
// Contents:
//
//  Header File for encoding stream
//
//-----------------------------------------------------------------------------
#ifndef __ENCSTREAM_H__
	#define __ENCSTREAM_H__

	#include "charencoder.h"


	class CSoapShell
	{
    public:
	    virtual ~CSoapShell() {};
	    virtual HRESULT write(BYTE * pBuffer, ULONG ulLen) = 0; 
	    virtual HRESULT flush(void) = 0;
	    virtual HRESULT IsInitialized() = 0;
        virtual HRESULT setExtension(WCHAR *pcExtension, VARIANT *pvar, BOOL * pbSuccess) = 0;
        virtual HRESULT setCharset(BSTR bstrCharset)= 0;        
	    virtual void clear() = 0;
	};


	
    class CEncodingStream
	{
		public:
	  	    CEncodingStream();
		    ~CEncodingStream();

			HRESULT STDMETHODCALLTYPE Init(CSoapShell * pShell);
	        
	        HRESULT STDMETHODCALLTYPE setEncoding( 
	            BSTR bstrEncoding);
	        
	        HRESULT STDMETHODCALLTYPE write( 
	            BSTR bstr,
	            long lenWchar = -1);
	        
	        HRESULT STDMETHODCALLTYPE writeThrough( 
	            byte *buffer,
	            long lenBuffer);
	        
	        HRESULT STDMETHODCALLTYPE flush(void);
            
            HRESULT STDMETHODCALLTYPE setFaultCode(void);
	        

		private:
			CAutoP<CSoapShell> m_pSoapShell;
		    CAutoRg<BYTE> m_pvBuffer;        // buffer for the multibytes


		    CODEPAGE    m_codepage;        // codepage
		    ULONG       m_cchBuffer;    	// size of the buffer
		    UINT        m_maxCharSize;  	// maximum number of bytes of a wide char

		    WideCharToMultiByteFunc * m_pfnWideCharToMultiByte;

		    static const ULONG s_ulBufferSize;

		public:
			HRESULT IsInitialized(void);
			void Reset(void);
	};

    extern const WCHAR szInternalError[];    

#endif
