//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
	
// SdpSearch.h : Declaration of the CSdpSearch

#ifndef __SDPSEARCH_H_
#define __SDPSEARCH_H_

#include "resource.h"       // main symbols
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#include "BluetoothEvents.h"
#endif

/////////////////////////////////////////////////////////////////////////////
// CSdpSearch
class ATL_NO_VTABLE CSdpSearch : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CSdpSearch, &CLSID_SdpSearch>,
	public ISdpSearch
{
public:
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))	
	CSdpSearch() : hRadio(INVALID_HANDLE_VALUE), hConnection(0)
	{
		m_pUnkMarshaler = NULL;
	}
	~CSdpSearch();
#else
    CSdpSearch()  {;}
    ~CSdpSearch() {;}
#endif


#ifndef UNDER_CE
DECLARE_REGISTRY_RESOURCEID(IDR_SDPSEARCH)
#else
    static HRESULT WINAPI UpdateRegistry(BOOL bRegister) {return ERROR_CALL_NOT_IMPLEMENTED;}
#endif

DECLARE_GET_CONTROLLING_UNKNOWN()

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CSdpSearch)
	COM_INTERFACE_ENTRY(ISdpSearch)
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

// ISdpSearch
public:
	STDMETHOD(Begin)(ULONGLONG *pAddress, ULONG fConnect);
	STDMETHOD(End)();
	STDMETHOD(ServiceSearch)(SdpQueryUuid *pUuidList,
                             ULONG listSize,
                             ULONG *pHandles,
                             USHORT *pNumHandles);
	STDMETHOD(AttributeSearch)(ULONG handle,
                               SdpAttributeRange *pRangeList,
                               ULONG numRanges,
                               ISdpRecord **ppSdpRecord);
	STDMETHOD(ServiceAndAttributeSearch)(SdpQueryUuid *pUuidList,
                                         ULONG listSize,
                                         SdpAttributeRange *pRangeList,
                                         ULONG numRanges,
                                         ISdpRecord ***pppSdpRecords,
                                         ULONG *pNumRecords);
// BluetoothEvents
public:
    void LocalRadioInsertion(TCHAR *pszDeviceName, BOOL Enumerated);

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))	
public:
    HANDLE hRadio;
    HANDLE hConnection;
#endif
};

#endif //__SDPSEARCH_H_
