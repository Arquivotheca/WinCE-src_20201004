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

#ifndef __DICT_H_
#define __DICT_H_


class CASPState;		// forward decl


class ATL_NO_VTABLE CRequestDictionary : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CRequestDictionary, &CLSID_RequestDictionary>,
	public IDispatchImpl<IRequestDictionary, &IID_IRequestDictionary, &LIBID_ASPLib>,
	public CPtrMapper
{
private:
	CASPState *m_pASPState;
	DICT_TYPE m_dictType;
	PSTR m_pszRawData;				// original, unformatted data.  
									// Read only!! Not a copy of httpd data buffer.			
//	long			m_cRef;
	

public:
	BOOL ParseInput();		// parses post, query string, and cookie data
	BOOL WriteCookies();	// for Response.Cookies, setting header

	STDMETHODIMP GetServerVariables(BSTR bstrName, VARIANT* pvarReturn);
	
	CRequestDictionary();
	CRequestDictionary(DICT_TYPE dt, CASPState *pASPState, PSTR pszData);
	~CRequestDictionary();
	friend CRequestDictionary * CreateCRequestDictionary(DICT_TYPE dt, CASPState *pASPState, PSTR pszData);
	friend STDMETHODIMP CRequest::get_Form(IRequestDictionary **ppDictReturn);

DECLARE_REGISTRY_RESOURCEID(IDR_REQUESTDICTIONARY)
DECLARE_NOT_AGGREGATABLE(CRequestDictionary)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CRequestDictionary)
	COM_INTERFACE_ENTRY(IRequestDictionary)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

	// Collect
 	STDMETHOD(get_Item)(/* [in, optional] */ VARIANT Var, /* [out, retval] */ VARIANT* pVariantReturn);
	STDMETHOD(put_Item)(/* [optional, in] */ VARIANT varKey, /* [in] */ BSTR bstrValue);
  	STDMETHOD(get__NewEnum)(/*  [out, retval] */ IUnknown** ppEnumReturn);
    STDMETHOD(get_Count)(/* [out, retval] */ int* cStrRet);
    STDMETHOD(get_Key)(/* [in] */ VARIANT VarKey,  /* [out, retval] */ VARIANT* pvar);
};



CRequestDictionary * CreateCRequestDictionary();
CRequestDictionary * CreateCRequestDictionary(DICT_TYPE dt, 
						CASPState *pASPState, PSTR pszData);


#endif
