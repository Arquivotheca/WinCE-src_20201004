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
