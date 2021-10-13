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
	
// SdpWalk.h : Declaration of the CSdpWalk

#ifndef __SDPWALK_H_
#define __SDPWALK_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CSdpWalk
class ATL_NO_VTABLE CSdpWalk : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CSdpWalk, &CLSID_SdpWalk>,
	public ISdpWalk
{
public:
	CSdpWalk()
	{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))	
		m_pUnkMarshaler = NULL;
#endif		
	}

#ifndef UNDER_CE
DECLARE_REGISTRY_RESOURCEID(IDR_SDPWALK)
#else
    static HRESULT WINAPI UpdateRegistry(BOOL bRegister) {return ERROR_CALL_NOT_IMPLEMENTED;}
#endif
DECLARE_GET_CONTROLLING_UNKNOWN()

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CSdpWalk)
	COM_INTERFACE_ENTRY(ISdpWalk)
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))	
	COM_INTERFACE_ENTRY_AGGREGATE(IID_IMarshal, m_pUnkMarshaler.p)
#endif	
END_COM_MAP()

	HRESULT FinalConstruct()
	{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
		return CoCreateFreeThreadedMarshaler(
			GetControllingUnknown(), &m_pUnkMarshaler.p);
#endif			
		return S_OK;
	}

	void FinalRelease()
	{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))	
		m_pUnkMarshaler.Release();
#endif		
	}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
	CComPtr<IUnknown> m_pUnkMarshaler;
#endif	

// ISdpWalk
public:
	STDMETHOD(WalkStream)(USHORT type, USHORT specificType, UCHAR *pStream,  ULONG streamSize);
	STDMETHOD(WalkNode)(NodeData *pData, ULONG state);
};

#endif //__SDPWALK_H_
