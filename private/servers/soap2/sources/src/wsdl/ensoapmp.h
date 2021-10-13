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
// File:    ensoapmp.h
// 
// Contents:
//
//  Header File 
//
//		IEnumWSDLService  Interface describtion
//	
//-----------------------------------------------------------------------------
#ifndef __ENSOAPMP_H_INCLUDED__
#define __ENSOAPMP_H_INCLUDED__

class CEnumSoapMappers : public IEnumSoapMappers
{
	public:
		CEnumSoapMappers();
		~CEnumSoapMappers();
	public:
	    HRESULT STDMETHODCALLTYPE Next(long celt, ISoapMapper **ppSoapMapper, long *pulFetched);
		HRESULT STDMETHODCALLTYPE Skip(long celt);
		HRESULT STDMETHODCALLTYPE Reset(void);
	    HRESULT STDMETHODCALLTYPE Clone(IEnumSoapMappers **ppenum);

    DECLARE_INTERFACE_MAP;

	public: 
	    HRESULT Clone(IEnumSoapMappers **ppenum, bool bForInput);    
		HRESULT	Add(ISoapMapper *pWSDLOperation);
		HRESULT	AddOrdered(ISoapMapper *pWSDLOperation);		
		HRESULT FindParameter(long lparaOrder, ISoapMapper **ppSoapMapper);
		HRESULT parameterCount(long *plArgCount);
		HRESULT getNext(ISoapMapper **ppSoapMapper, DWORD *pdwCookie);
		HRESULT Size(long *pulSize);
		HRESULT CountOfArguments(long *pulParaCount);
        HRESULT Copy(CEnumSoapMappers *pOriginal);

protected:
	CList<ISoapMapper*> m_soapMapperList;
	bool	m_bRetVal;
	bool	m_bCheckedForRetVal;

};


#endif
