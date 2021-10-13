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
// File:    ensoappt.h
//
// Contents:
//
//  Header File
//
//        IEnumWSDLPorts  Interface describtion
//
//-----------------------------------------------------------------------------
#ifndef __ENSOAPPT_H_INCLUDED__
#define __ENSOAPPT_H_INCLUDED__


class CEnumWSDLPorts : public IEnumWSDLPorts
{
	public:
		CEnumWSDLPorts();
		~CEnumWSDLPorts();
	public:
	    HRESULT STDMETHODCALLTYPE Next(long celt, IWSDLPort **ppWSDLPort, long *pulFetched);
		HRESULT STDMETHODCALLTYPE Skip(long celt);
		HRESULT STDMETHODCALLTYPE Reset(void);
	    HRESULT STDMETHODCALLTYPE Clone(IEnumWSDLPorts **ppenum);
		HRESULT STDMETHODCALLTYPE Find(BSTR bstrOperationToFind, IWSDLPort **ppIWSDLPort);
        
        

    DECLARE_INTERFACE_MAP;

	public: 
		HRESULT	Add(IWSDLPort *pWSDLPort);
		HRESULT Size(long *pulSize);
        HRESULT Copy(CEnumWSDLPorts *pOrg); 

protected:
	CList<IWSDLPort *> m_portList;
};


#endif
