//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
