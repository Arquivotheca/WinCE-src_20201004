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
// File:    enwsdlop.h
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
