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
