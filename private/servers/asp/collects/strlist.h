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
/*--
Module Name: strlist.h
Abstract: Helper for dict class 
--*/

#ifndef __CREQUESTSTRLIST_H_
#define __CREQUESTSTRLIST_H_



/////////////////////////////////////////////////////////////////////////////
// CRequestStrList



class CASPState;		// forward decl

class ATL_NO_VTABLE CRequestStrList : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CRequestStrList, &CLSID_RequestStrList>,
	public IDispatchImpl<IRequestStrList, &IID_IRequestStrList, &LIBID_ASPLib>
{
	friend class CRequestDictionary;
private:
	CASPState *m_pASPState;
	WCHAR *m_wszData;			// used with only one element
	WCHAR **m_pwszArray;		
	DICT_TYPE m_dictType;
	int m_nEntries;

	//  Cookie data
	DATE m_dateExpires;
	PSTR m_pszDomain;
	PSTR m_pszPath;


public:
	CRequestStrList();
	~CRequestStrList();
	
	BOOL AddStringToArray(WCHAR *wszData);

DECLARE_REGISTRY_RESOURCEID(IDR_REQUESTSTRLIST)
DECLARE_NOT_AGGREGATABLE(CRequestStrList)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CRequestStrList)
	COM_INTERFACE_ENTRY(IRequestStrList)
	COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

// ICRequestStrList
public:
	// Collect
 	STDMETHOD(get_Item)(/* [in, optional] */ VARIANT Var, /* [out, retval] */ VARIANT* pVariantReturn);
	STDMETHOD(put_Item)(/* [optional, in] */ VARIANT varKey, /* [in] */ BSTR bstrValue);
  	STDMETHOD(get__NewEnum)(/*  [out, retval] */ IUnknown** ppEnumReturn);
    STDMETHOD(get_Count)(/* [out, retval] */ int* cStrRet);
    STDMETHOD(get_Key)(/* [in] */ VARIANT VarKey,  /* [out, retval] */ VARIANT* pvar);


	STDMETHOD(get_HasKeys)(/* [out, retval] */ VARIANT_BOOL *pfHasKeys);
	STDMETHOD(put_Expires)(/*[in]*/ DATE dtExpires);
	STDMETHOD(put_Domain)( /*[in]*/ BSTR bstrDomain);
	STDMETHOD(put_Path)(   /*[in]*/ BSTR bstrPath);

	friend CRequestStrList * CreateCRequestStrList(CASPState *pASPState, 
										WCHAR *wszInitialString, DICT_TYPE dt);

};

#endif //__CREQUESTSTRLIST_H_

