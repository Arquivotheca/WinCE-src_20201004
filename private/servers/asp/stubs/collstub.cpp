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
Module Name: collstub.cpp
--*/



#include "aspmain.h"

const BOOL c_fUseCollections = FALSE;


CRequestDictionary::CRequestDictionary()
{
	DEBUGCHK(FALSE);
}

CRequestStrList::CRequestStrList()
{
	DEBUGCHK(FALSE);
}

CPtrMapper::CPtrMapper(int i)
{
	DEBUGCHK(FALSE);
}	

CRequestDictionary * CreateCRequestDictionary(DICT_TYPE dt, CASPState *pASPState, PSTR pszData)
{
	DEBUGCHK(FALSE);
	return NULL;
}


CRequestDictionary::~CRequestDictionary()
{
	DEBUGCHK(FALSE);
}
BOOL CRequestDictionary::WriteCookies()
{
	DEBUGCHK(FALSE);
	return FALSE;
}

CRequestStrList::~CRequestStrList()
{
	DEBUGCHK(FALSE);
}

CRequestStrList * CreateCRequestStrList(CASPState *pASPState, 
										WCHAR *wszInitialString, DICT_TYPE dt)
{
	DEBUGCHK(FALSE);
	return NULL;
}


STDMETHODIMP CRequestStrList::get_HasKeys(VARIANT_BOOL *pfHasKeys)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}


STDMETHODIMP CRequestStrList::put_Expires(DATE dtExpires)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}


STDMETHODIMP CRequestStrList::put_Domain(BSTR bstrDomain)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}


STDMETHODIMP CRequestStrList::put_Path(BSTR bstrPath)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}


STDMETHODIMP CRequestStrList::get_Item(struct tagVARIANT,struct tagVARIANT *)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}


STDMETHODIMP CRequestStrList::get_Key(struct tagVARIANT,struct tagVARIANT *)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}


STDMETHODIMP CRequestStrList::get_Count(int *)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}

STDMETHODIMP CRequestStrList::put_Item(VARIANT varKey, BSTR bstrValue)
{
	DEBUGCHK(FALSE);
	return DISP_E_EXCEPTION;
}

STDMETHODIMP CRequestStrList::get__NewEnum(struct IUnknown * *)
{
	DEBUGCHK(FALSE);
	return DISP_E_EXCEPTION;
}


STDMETHODIMP CRequestDictionary::get_Item(struct tagVARIANT,struct tagVARIANT *)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}


STDMETHODIMP CRequestDictionary::get_Key(struct tagVARIANT,struct tagVARIANT *)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}


STDMETHODIMP CRequestDictionary::get_Count(int *)
{
	DEBUGCHK(FALSE);
	return E_NOTIMPL;
}

STDMETHODIMP CRequestDictionary::put_Item(VARIANT varKey, BSTR bstrValue)
{
	DEBUGCHK(FALSE);
	return DISP_E_EXCEPTION;
}


STDMETHODIMP CRequestDictionary::get__NewEnum(struct IUnknown * *)
{
	DEBUGCHK(FALSE);
	return DISP_E_EXCEPTION;
}

CPtrMapper::~CPtrMapper(void)
{
	DEBUGCHK(FALSE);
}
