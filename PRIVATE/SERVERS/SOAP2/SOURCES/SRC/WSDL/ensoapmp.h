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
// File:    enwsdlse.h
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
