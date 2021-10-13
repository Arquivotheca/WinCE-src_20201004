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
// File:    enwsdlse.h
// 
// Contents:
//
//  Header File 
//
//        IEnumWSDLService  Interface describtion
//
//-----------------------------------------------------------------------------
#ifndef __ENWSDLSER_H_INCLUDED__
#define __ENWSDLSER_H_INCLUDED__

class CEnumWSDLService : public IEnumWSDLService
{

	public:
		CEnumWSDLService();
		~CEnumWSDLService();

	public:
	    HRESULT STDMETHODCALLTYPE Next(long celt, IWSDLService **ppWSDLService, long *pulFetched);
		HRESULT STDMETHODCALLTYPE Skip(long celt);
		HRESULT STDMETHODCALLTYPE Reset(void);
	    HRESULT STDMETHODCALLTYPE Clone(IEnumWSDLService **ppenum);
		HRESULT STDMETHODCALLTYPE Find(BSTR bstrServiceToFind, IWSDLService **ppWSDLService);

    DECLARE_INTERFACE_MAP;

public: 
		HRESULT	Add(IWSDLService *pWSDLService);
		HRESULT Size(long *pulSize);
        HRESULT Copy(CEnumWSDLService *pOrg);

protected:
	CList<IWSDLService *> m_serviceList;

};




#endif
