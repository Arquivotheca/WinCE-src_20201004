//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

