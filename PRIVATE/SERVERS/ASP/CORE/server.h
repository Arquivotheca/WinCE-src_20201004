//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Server.h : Declaration of the CServer

#ifndef __SERVER_H_
#define __SERVER_H_

// #include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CServer

class CASPState;		// forward declaration

class ATL_NO_VTABLE CServer : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CServer, &CLSID_Server>,
	public IDispatchImpl<IServer, &IID_IServer, &LIBID_ASPLib>
{
private:
	CASPState *m_pASPState;
public:

	CServer();
	

DECLARE_REGISTRY_RESOURCEID(IDR_SERVER)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CServer)
	COM_INTERFACE_ENTRY(IServer)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

// IServer
public:

	STDMETHOD(get_URLEncode)(/*[in] */ BSTR pszName, /* [out, retval] */ BSTR *pVal);
	STDMETHOD(get_MapPath)(  /*[in] */ BSTR pszName, /* [out, retval] */ BSTR *pVal);
};

#endif //__SERVER_H_
