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
/*******************************************************************
 *
 * ScrSite.h
 *
 *
 *******************************************************************
 *
 * Description: Implementation of IActiveScriptSite
 *
 ******************************************************************/

// #include "activscp.h"

class CASPState;

class CScriptSite : public IActiveScriptSite
{
public:
	CScriptSite()   { m_cRef = 0; }
	~CScriptSite()  { ; }

// IUnknown
	STDMETHODIMP_(ULONG) AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);
	STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject);

private:
// IActiveScriptSite methods
	STDMETHOD(GetLCID)(unsigned long *);
	STDMETHOD(GetItemInfo)(const unsigned short *bstrName,unsigned long,struct IUnknown ** ,struct ITypeInfo ** );
	STDMETHOD(GetDocVersionString)(unsigned short ** );
	STDMETHOD(OnScriptTerminate)(const struct tagVARIANT *,const struct tagEXCEPINFO *);
	STDMETHOD(OnStateChange)(enum tagSCRIPTSTATE);
	STDMETHOD(OnScriptError)(struct IActiveScriptError *);
	STDMETHOD(OnEnterScript)(void);
	STDMETHOD(OnLeaveScript)(void);

// Data memebers
private:
	long			m_cRef;
};


