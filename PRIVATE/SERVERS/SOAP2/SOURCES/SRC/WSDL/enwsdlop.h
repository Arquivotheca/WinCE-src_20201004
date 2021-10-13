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
//        IEnumWSDLService  Interface describtion
//
//-----------------------------------------------------------------------------
#ifndef __ENWSDLOP_H_INCLUDED__
#define __ENWSDLOP_H_INCLUDED__

#include "wsdloper.h"

class CEnumWSDLOperations : public IEnumWSDLOperations
{
	public:
		CEnumWSDLOperations();
		~CEnumWSDLOperations();
	public:
	    HRESULT STDMETHODCALLTYPE Next(long celt, IWSDLOperation **ppWSDLOperation, long *pulFetched);
		HRESULT STDMETHODCALLTYPE Skip(long celt);
		HRESULT STDMETHODCALLTYPE Reset(void);
	    HRESULT STDMETHODCALLTYPE Clone(IEnumWSDLOperations **ppenum);
		HRESULT STDMETHODCALLTYPE Find(BSTR bstrOperationToFind, IWSDLOperation **ppIWSDLOperation);
		HRESULT STDMETHODCALLTYPE Size(long *pulSize);

    DECLARE_INTERFACE_MAP;

	public: 
		HRESULT	Add(IWSDLOperation *pWSDLOperation);
        HRESULT Copy(CEnumWSDLOperations *pOrg);

protected:
	CList<IWSDLOperation *> m_operationList;
};


#endif
