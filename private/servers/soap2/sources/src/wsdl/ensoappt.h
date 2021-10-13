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
