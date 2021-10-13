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
