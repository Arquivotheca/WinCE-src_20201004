//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
	
// SdpRecord.h : Declaration of the CSdpRecord

#ifndef __SDPRECORD_H_
#define __SDPRECORD_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CSdpRecord
class ATL_NO_VTABLE CSdpRecord : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CSdpRecord, &CLSID_SdpRecord>,
	public ISdpRecord, IDataObject
{
public:
	CSdpRecord()
	{
        Init();
	}
    ~CSdpRecord();

#ifndef UNDER_CE
DECLARE_REGISTRY_RESOURCEID(IDR_SDPRECORD)
#else
    static HRESULT WINAPI UpdateRegistry(BOOL bRegister) {return ERROR_CALL_NOT_IMPLEMENTED;}
#endif

DECLARE_GET_CONTROLLING_UNKNOWN()

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CSdpRecord)
	COM_INTERFACE_ENTRY(ISdpRecord)
	COM_INTERFACE_ENTRY(IDataObject)
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

// ISdpRecord
public:
	STDMETHOD(GetIcon)(int cxRes, int cyRes, HICON *phIcon);
	STDMETHOD(GetString)(USHORT offset, USHORT *pLangId, WCHAR **ppString);
	STDMETHOD(GetAttributeList)(USHORT **ppList, ULONG *pListSize);
	STDMETHOD(Walk)(ISdpWalk *pWalk);
	STDMETHOD(GetAttributeAsStream)(USHORT attribute, UCHAR **ppStream, ULONG *pSize);
	STDMETHOD(GetAttribute)(USHORT attribute, NodeData *pNode);
	STDMETHOD(SetAttributeFromStream)(USHORT attribute, UCHAR *pStream, ULONG size);
	STDMETHOD(SetAttribute)(USHORT attribute, NodeData *pNode);
	STDMETHOD(WriteToStream)(UCHAR **ppStream, ULONG *pStreamSize, ULONG preSize, ULONG postSize);
	STDMETHOD(CreateFromStream)(UCHAR *pStream, ULONG size);
    STDMETHOD(GetServiceClass)(LPGUID pServiceClass);

// IDataObject
public:
    STDMETHOD(GetData)(FORMATETC *pformatetcIn, STGMEDIUM *pmedium);
    STDMETHOD(GetDataHere)(FORMATETC *pformatetc, STGMEDIUM *pmedium);
    STDMETHOD(QueryGetData)(FORMATETC *pformatetc);
    STDMETHOD(GetCanonicalFormatEtc)(FORMATETC *pformatectIn, FORMATETC *pformatetcOut);
    STDMETHOD(SetData)(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease);
    STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc);
    STDMETHOD(DAdvise)(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
    STDMETHOD(DUnadvise)(DWORD dwConnection);
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppenumAdvise);

protected:
    void Init(void);
    void DestroyRecord(void);
    void LockRecord(UCHAR lock);
    HRESULT GetLangBaseInfo(USHORT *pLangId, USHORT *pMibeNum, USHORT *pAttribId);
    PSDP_NODE GetAttributeValueNode(USHORT attribute);

    SdpNodeList nodeList;

    CRITICAL_SECTION listLock;
    LONG locked;
};

#endif //__SDPRECORD_H_
