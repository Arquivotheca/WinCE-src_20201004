//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
	
// SdpStream.h : Declaration of the CSdpStream

#ifndef __SDPSTREAM_H_
#define __SDPSTREAM_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CSdpStream
class ATL_NO_VTABLE CSdpStream : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CSdpStream, &CLSID_SdpStream>,
	public ISdpStream
{
public:
	CSdpStream()
	{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))	
		m_pUnkMarshaler = NULL;
#endif		
	}

#ifndef UNDER_CE
DECLARE_REGISTRY_RESOURCEID(IDR_SDPSTREAM)
#else
    static HRESULT WINAPI UpdateRegistry(BOOL bRegister) {return ERROR_CALL_NOT_IMPLEMENTED;}
#endif

DECLARE_GET_CONTROLLING_UNKNOWN()

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CSdpStream)
	COM_INTERFACE_ENTRY(ISdpStream)
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

// ISdpStream
public:
	STDMETHOD(ByteSwapInt16)(SHORT int16, SHORT *pInt16);
	STDMETHOD(ByteSwapInt32)(LONG int32, LONG *pInt32);
	STDMETHOD(ByteSwapInt64)(LONGLONG inInt64, LONGLONG *pOutInt64);
	STDMETHOD(ByteSwapInt128)(PSDP_LARGE_INTEGER_16 pInInt128, PSDP_LARGE_INTEGER_16 pOutInt128);
	STDMETHOD(ByteSwapUint16)(USHORT uint16, USHORT *pUint16);
	STDMETHOD(ByteSwapUint32)(ULONG uint32, ULONG *pUint32);
	STDMETHOD(ByteSwapUint64)(ULONGLONG inUint64, ULONGLONG* pOutUint64);
	STDMETHOD(ByteSwapUint128)(PSDP_ULARGE_INTEGER_16 pInUint128, PSDP_ULARGE_INTEGER_16 pOutUint128);
	STDMETHOD(ByteSwapUuid128)(GUID *pInUuid128, GUID *pOutUuid128);
	STDMETHOD(Walk)(UCHAR *pStream, ULONG size, ISdpWalk *pWalk);
	STDMETHOD(Validate)(UCHAR *pStream, ULONG size, ULONG_PTR *pErrorByte);
    STDMETHOD(RetrieveRecords)(UCHAR *pStream, ULONG size, ISdpRecord **ppSdpRecords, ULONG *pNumRecords);
	STDMETHOD(RetrieveInt128)(UCHAR *pStream, PSDP_LARGE_INTEGER_16 pInt128);
	STDMETHOD(RetrieveInt64)(UCHAR *pStream, LONGLONG *pInt64);
	STDMETHOD(RetrieveInt32)(UCHAR *pStream, LONG *pInt32);
	STDMETHOD(RetrieveInt16)(UCHAR *pStream, SHORT *pInt16);
	STDMETHOD(RetrieveUint128)(UCHAR *pStream, PSDP_ULARGE_INTEGER_16 pUint128);
	STDMETHOD(RetrieveUint64)(UCHAR *pStream, ULONGLONG *pUint64);
	STDMETHOD(RetrieveUint32)(UCHAR *pStream, ULONG *pUint32);
	STDMETHOD(RetrieveUint16)(UCHAR *pStream, USHORT *pUint16);
	STDMETHOD(RetrieveUuid128)(UCHAR *pStream, GUID* pUuid128);
    STDMETHOD(NormalizeUuid)(NodeData *pDataUuid, GUID* pNormalizeUuid);
    STDMETHOD(RetrieveElementInfo)(UCHAR *pStream, SDP_TYPE *pElementType, SDP_SPECIFICTYPE *pElementSpecificType, ULONG *pElementSize, ULONG *pStorageSize, UCHAR **ppData);
    STDMETHOD(VerifySequenceOf)(UCHAR *pStream, ULONG size, SDP_TYPE ofType, UCHAR *pSpecificSizes, ULONG *pNumFound);
};

#endif //__SDPSTREAM_H_
