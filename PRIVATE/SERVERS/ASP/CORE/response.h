//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: response.h
Abstract:   Implements Response script object.
--*/

#ifndef __RESPONSE_H_
#define __RESPONSE_H_

// #include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CResponse

// forward declarations
class CASPState;
class CRequestDictionary;

class ATL_NO_VTABLE CResponse : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CResponse, &CLSID_Response>,
	public IDispatchImpl<IResponse, &IID_IResponse, &LIBID_ASPLib>,
	public IStream
{
private:
	CASPState *m_pASPState;
	CRequestDictionary *m_pCookie;


public:

	CResponse();
	~CResponse();
	
DECLARE_REGISTRY_RESOURCEID(IDR_RESPONSE)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CResponse)
	COM_INTERFACE_ENTRY(IResponse)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IStream)
	COM_INTERFACE_ENTRY(ISequentialStream)
END_COM_MAP()

public:
// IResponse
	STDMETHOD(Write)(VARIANT varData);
	STDMETHOD(BinaryWrite)(VARIANT varData);
	STDMETHOD(Redirect)(BSTR pszURL);
	STDMETHOD(Flush)();
	STDMETHOD(End)();
	STDMETHOD(Clear)();
	STDMETHOD(AppendToLog)(BSTR pszLogData);
	STDMETHOD(AddHeader)(BSTR pszName, BSTR pszValue);
	STDMETHOD(get_Status)(/*[out, retval]*/ BSTR *pVal);
	STDMETHOD(put_Status)(/*[in]*/ BSTR newVal);
	STDMETHOD(get_Charset)(/*[out, retval]*/ BSTR *pVal);
	STDMETHOD(put_Charset)(/*[in]*/ BSTR newVal);
	STDMETHOD(get_ContentType)(/*[out, retval]*/ BSTR *pVal);
	STDMETHOD(put_ContentType)(/*[in]*/ BSTR newVal);
	STDMETHOD(get_Buffer)(/*[out, retval]*/ VARIANT_BOOL *pVal);
	STDMETHOD(put_Buffer)(/*[in]*/ VARIANT_BOOL newVal);

	STDMETHODIMP	get_Expires(VARIANT *pvarExpiresMinutesRet);
	STDMETHODIMP	put_Expires(long lExpiresMinutes);
	STDMETHODIMP	get_ExpiresAbsolute(VARIANT *pvarTimeRet);
	STDMETHODIMP	put_ExpiresAbsolute(DATE dtExpires);

	STDMETHOD(get_Cookies)(/* [out, retval] */ IRequestDictionary **ppDictReturn);

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

#endif //__RESPONSE_H_
