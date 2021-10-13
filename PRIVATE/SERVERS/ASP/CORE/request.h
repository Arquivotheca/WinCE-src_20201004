//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// request.h : Declaration of the CRequest

#ifndef __REQUEST_H_
#define __REQUEST_H_

// #include "resource.h"       // main symbols
// #include "dict.h"



/////////////////////////////////////////////////////////////////////////////
// CRequest
class CASPState;
class CRequestDictionary;


enum FormDataStatus {AVAILABLE, BINARYREADONLY, FORMCOLLECTIONONLY, ISTREAMONLY};

class ATL_NO_VTABLE CRequest : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CRequest, &CLSID_Request>,
	public IDispatchImpl<IRequest, &IID_IRequest, &LIBID_ASPLib>,
	public IStream
{
private:
	CASPState *m_pASPState;
	CRequestDictionary *m_pQueryString;
	CRequestDictionary *m_pForm;
	CRequestDictionary *m_pCookie;
	CRequestDictionary *m_pServerVariables;
	
	DWORD m_cbRead;		// number of bytes read on BinaryRead or IStream::Read so far
	FormDataStatus m_formState;
	BOOL  m_fParsedPost;
	
public:
	CRequest();
	~CRequest();

	BOOL SetStateIfPossible(FormDataStatus newState);
	HRESULT ReadData(BYTE *pv, ULONG cb);

DECLARE_REGISTRY_RESOURCEID(IDR_REQUEST)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CRequest)
	COM_INTERFACE_ENTRY(IRequest)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IStream)
	COM_INTERFACE_ENTRY(ISequentialStream)
END_COM_MAP()

public:
// IRequest
	STDMETHOD(get_TotalBytes)(/*[out, retval]*/ long *pVal);
	STDMETHOD(get_ServerVariables)(IRequestDictionary **ppDictReturn);
	STDMETHOD(get_QueryString)(/* [out, retval] */ IRequestDictionary **ppDictReturn);
	STDMETHOD(get_Form)(/* [out, retval] */ IRequestDictionary **ppDictReturn);
	STDMETHOD(get_Cookies)(/* [out, retval] */ IRequestDictionary **ppDictReturn);
	STDMETHOD(BinaryRead)(/* [in, out] */ VARIANT *pvarCount, 
						  /* [out, retval] */ VARIANT *pvarReturn);

// IStream
	HRESULT STDMETHODCALLTYPE Write(const void __RPC_FAR *pv,ULONG cb,ULONG __RPC_FAR *pcbWritten);
	HRESULT STDMETHODCALLTYPE Read(void __RPC_FAR *pv, ULONG cb, ULONG __RPC_FAR *pcbRead);

	HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove,DWORD dwOrigin,ULARGE_INTEGER __RPC_FAR *plibNewPosition);
	HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize);
	HRESULT STDMETHODCALLTYPE CopyTo(IStream __RPC_FAR *pstm,ULARGE_INTEGER cb,ULARGE_INTEGER __RPC_FAR *pcbRead,ULARGE_INTEGER __RPC_FAR *pcbWritten);
	HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags);
	HRESULT STDMETHODCALLTYPE Revert(void);
	HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,DWORD dwLockType);
	HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset,ULARGE_INTEGER cb,DWORD dwLockType);       
	HRESULT STDMETHODCALLTYPE Stat(STATSTG __RPC_FAR *pstatstg, DWORD grfStatFlag);
	HRESULT STDMETHODCALLTYPE Clone(IStream __RPC_FAR *__RPC_FAR *ppstm);
};

#endif //__REQUEST_H_ 
