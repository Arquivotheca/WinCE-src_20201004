//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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


