//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
	
// SdpNodeContainer.h : Declaration of the CSdpNodeContainer

#ifndef __SDPNODECONTAINER_H_
#define __SDPNODECONTAINER_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CSdpNodeContainer
class ATL_NO_VTABLE CSdpNodeContainer : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CSdpNodeContainer, &CLSID_SdpNodeContainer>,
	public ISdpNodeContainer
{
public:
	CSdpNodeContainer()
	{
		Init();
	}
	~CSdpNodeContainer();


#ifndef UNDER_CE
DECLARE_REGISTRY_RESOURCEID(IDR_SDPNODECONTAINER)
#else
    static HRESULT WINAPI UpdateRegistry(BOOL bRegister) {return ERROR_CALL_NOT_IMPLEMENTED;}
#endif
DECLARE_GET_CONTROLLING_UNKNOWN()

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CSdpNodeContainer)
	COM_INTERFACE_ENTRY(ISdpNodeContainer)
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

// ISdpNodeContainer
public:
	STDMETHOD(GetNodeStringData)(ULONG nodeIndex, NodeData *pData);
	STDMETHOD(CreateFromStream)(UCHAR *pStream, ULONG size);
	STDMETHOD(GetNodeCount)(ULONG *pNodeCount);
	STDMETHOD(LockContainer)(UCHAR lock);
	STDMETHOD(GetNode)(ULONG nodeIndex, NodeData *pData);
	STDMETHOD(SetNode)(ULONG nodeIndex, NodeData *pData);
	STDMETHOD(Walk)(ISdpWalk *pWalk);
	STDMETHOD(SetType)(NodeContainerType type);
	STDMETHOD(GetType)(NodeContainerType *pType);
	STDMETHOD(AppendNode)(NodeData *pData);
	STDMETHOD(WriteStream)(UCHAR *pStream, ULONG *pNumBytesWritten);
    STDMETHOD(CreateStream)(UCHAR **ppStream, ULONG *pSize);
	STDMETHOD(GetStreamSize)(ULONG *pSize);

protected:
	void Init();

    NodeContainerType containerType;
	LONG locked;
    ULONG streamSize;
    UCHAR streamSizeValid;

    SdpNodeList nodeList;

    CRITICAL_SECTION listLock;
};

#endif //__SDPNODECONTAINER_H_
