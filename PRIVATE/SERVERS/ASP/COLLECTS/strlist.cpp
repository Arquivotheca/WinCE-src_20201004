//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: strlist.cpp
Abstract: Helper for dict class 
--*/
#include "aspmain.h"


/////////////////////////////////////////////////////////////////////////////
// CRequestStrList


CRequestStrList::CRequestStrList()
{
	m_pszPath = m_pszDomain = NULL;
	m_pwszArray = NULL;

	m_pASPState = 0;
	m_wszData = 0;
	m_nEntries = 0;

	m_dictType = QUERY_STRING_TYPE;
	m_dateExpires = 0;

}

CRequestStrList * CreateCRequestStrList(CASPState *pASPState, 
										WCHAR *wszInitialString, DICT_TYPE dt)
{
	CRequestStrList *pStrList = NULL;
	if (FAILED (CoCreateInstance(CLSID_RequestStrList,NULL,CLSCTX_INPROC_SERVER,IID_IRequestStrList,(void**) &pStrList)))
		return NULL;

	pStrList->m_wszData = wszInitialString;
	pStrList->m_nEntries = wszInitialString ? 1 : 0;
	pStrList->m_pASPState = pASPState;
	pStrList->m_dictType = dt;

	return pStrList;
}


CRequestStrList::~CRequestStrList()
{
	int i;
	MyFree(m_wszData);

	if (m_nEntries > 1)
	{
		for (i = 0; i < m_nEntries; i++)
			MyFree(m_pwszArray[i]);
		MyFree(m_pwszArray);
	}
	MyFree(m_pszDomain);
	MyFree(m_pszPath);
}


//  This is called for sub-indexed values.  For instance, 
//  on Request.Form("Foo")(1), the Request.Form("Foo") will return a CRequestStrList
//  object, which will be called with the argument (1) in this fcn.
STDMETHODIMP CRequestStrList::get_Item(VARIANT varKey, VARIANT* pvarReturn)
{
	DEBUG_CODE_INIT;
    VARIANT *pvarKey = &varKey;
	BSTR bstr = NULL;		// LATER -- remove
	WCHAR *wszTempBuf = NULL;
	WCHAR *wszTrav = NULL;
	HRESULT ret = DISP_E_EXCEPTION;
	DWORD vt;
	LONG lCodePage = (m_dictType == REQUEST_COOKIE_TYPE) ? CP_ACP : m_pASPState->m_lCodePage;
		
	
    VariantInit(pvarReturn);

    // Use VariantResolveDispatch which will:
    //
    //     *  Copy BYREF variants for us using VariantCopyInd
    //     *  handle E_OUTOFMEMORY for us
    //     *  get the default value from an IDispatch, which seems
    //        like an appropriate conversion.
    //
    VARIANT varKeyCopy;
    VariantInit(&varKeyCopy);
    vt = V_VT(pvarKey);

    if ((vt != VT_BSTR) && (vt != VT_I2) && (vt != VT_I4))
	{
        if (FAILED(VariantResolveDispatch(&varKeyCopy, &varKey)))
		{
			ASP_ERR(IDS_E_PARSER);
			myleave(811);
		}
		pvarKey = &varKeyCopy;
	}
    vt = V_VT(pvarKey);

	switch (vt)
	{
	case VT_I1:  case VT_I2:               case VT_I8:
	case VT_UI1: case VT_UI2: case VT_UI4: case VT_UI8:
	case VT_R4:  case VT_R8:
		// Coerce all integral types to VT_I4
		if (FAILED(VariantChangeTypeEx(pvarKey, pvarKey, lCodePage, 0, VT_I4)))
			myleave(821);

		vt = VT_I4;
	}
	
	// Access the string data itself if vt == VT_ERROR.
	// If there are multiple fields of the same name, print them out in a
	// comma separated list. IE if Request.Post("Names") that had N fields, 
	// return as output "Name1, Name2, ..., NameN"
	
	if ( VT_ERROR == vt)
	{	
		if (m_nEntries == 0)			// do nothing, empty string
			;
		else if (m_nEntries == 1)		// just one entry in this object
		{
			if (NULL == (bstr = SysAllocString(m_wszData)))
			{
				ASP_ERR(IDS_E_NOMEM);
				myleave(822);
			}
		}
		else		// create the comma separated list
		{
			int i;
			int iOutput = 0;

			// Finds out how big a buffer we need.
			for (i = 0; i < m_nEntries; i++)
			{	
				iOutput += wcslen(m_pwszArray[i]) + 2;
			}
			
			// put into an ANSI string first	
			if (NULL == (wszTempBuf = MyRgAllocNZ(WCHAR, iOutput)))
			{
				ASP_ERR(IDS_E_NOMEM);
				myleave(823);
			}		
			wszTrav = wszTempBuf;

			// run through the list, copying string over.
			for (i = 0; i < m_nEntries; i++)
			{	
				wcscpy(wszTrav,m_pwszArray[i]);
				wszTrav += wcslen(m_pwszArray[i]);  
				
				// Put the sepearating ", " in the string on all but last elem in list
				if (i != m_nEntries - 1)
				{		
					wszTrav[0] = L',';	
					wszTrav[1] = L' ';
					wszTrav += 2;
				}			
			}

			if (NULL == (bstr = SysAllocString(wszTempBuf)))
			{
				ASP_ERR(IDS_E_NOMEM);
				myleave(824);
			}		
		}
		V_VT(pvarReturn) = VT_BSTR;
		V_BSTR(pvarReturn) = bstr;
	}	// VT_NULL

	// In this case we get just 1 string if possible, ie
	// Request.Form("Names")(j) returns "NameJ"
	else if ( VT_I4 == vt)
	{
		int iIndex = V_I4(pvarKey);

		if (iIndex <= 0 ||
			iIndex > m_nEntries)
		{
			ASP_ERR(IDS_E_BAD_INDEX);
			myleave(825);
		}

		if (m_nEntries == 1)
		{
			if (NULL == (bstr = SysAllocString(m_wszData)))
			{
				ASP_ERR(IDS_E_NOMEM);
				myleave(826);
			}
		}
		else
		{
			if (NULL == (bstr = SysAllocString(m_pwszArray[iIndex - 1])))
			{
				ASP_ERR(IDS_E_NOMEM);
				myleave(827);
			}
		}
		V_VT(pvarReturn) = VT_BSTR;
		V_BSTR(pvarReturn) = bstr;		
	}
	// NOTE:  this could with vt = VT_BSTR in the case: 
	// variable = Response.Cookies("Value")("Index").  
	// IIS ASP does support this Cookies collections only.  CE does not, in order to keep code smaller.
	else		
	{
		ASP_ERR(IDS_E_BAD_SUBINDEX);		
		myleave(828);
	}

	ret = S_OK;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CRequestStrList::get_Item failed, err = %d, GLE=%X\r\n",
							 err, GetLastError()));

	VariantClear(&varKeyCopy);
	MyFree(wszTempBuf);

	return ret;
}

// NOTE.  this could with vt = VT_BSTR in the case: 
// Response.Cookies("Value")("Index") = "Value".  
// IIS ASP does support this Cookies collections only.  WinCE does not to keep code smaller.

STDMETHODIMP CRequestStrList::put_Item(VARIANT varKey, BSTR bstrValue)
{
	ASP_ERR(IDS_E_BAD_SUBINDEX);
	return DISP_E_EXCEPTION;
}

// Note: Unlike IIS, WinCE does not have support for enumeration on ASP collection objects.
STDMETHODIMP CRequestStrList::get__NewEnum(IUnknown** ppEnumReturn)
{
	ASP_ERR(IDS_E_NOT_IMPL);
	return DISP_E_EXCEPTION;
}

STDMETHODIMP CRequestStrList::get_Key(VARIANT VarKey,  VARIANT* pvar)
{	
	ASP_ERR(IDS_E_NOT_IMPL);
	return DISP_E_EXCEPTION;
}

STDMETHODIMP CRequestStrList::get_Count(int* cStrRet)
{
	*cStrRet = m_nEntries;
	return S_OK;
}

//  AddStringToArray - 
//  For case x=a&x=b coming from POST or QueryString data this keeps values in 
//  an array with entries "a" and "b"
//  We don't check for repeated entries, ie x=a&x=a, each of them get a separate
//  entry in the array.
BOOL CRequestStrList::AddStringToArray(WCHAR *wszData)
{
	// We only store 1 value with Cookies.  Overwrite value if one exists already.
	// (or if this is first element being put into array, use m_wszData
	// rather than allocating an array)
	
	if (m_dictType == RESPONSE_COOKIE_TYPE ||
		m_dictType == REQUEST_COOKIE_TYPE  ||
		0 == m_nEntries)
	{
		MyFree(m_wszData);
		m_wszData = wszData;
		m_nEntries = 1;
		return TRUE;		
	}

	// If exactly 1 element in the array, adding a 2nd requires to do initial
	// allocation of the array.
	if (m_nEntries == 1)		
	{
		m_pwszArray = MyRgAllocZ(WCHAR *,VALUE_GROW_SIZE);
		if (NULL == m_pwszArray)
		{
			ASP_ERR(IDS_E_NOMEM);
			return FALSE;
		}
		m_pwszArray[0] = m_wszData;		
		m_wszData = NULL;				// so we know which one to dealloc
	}

	// time to reallocate memory.
	if (m_nEntries % VALUE_GROW_SIZE == 0)
	{
		m_pwszArray = MyRgReAlloc(PWSTR, m_pwszArray,m_nEntries,VALUE_GROW_SIZE + m_nEntries);
		if (NULL == m_pwszArray)
		{
			ASP_ERR(IDS_E_NOMEM);
			return FALSE;
		}
	}

	m_pwszArray[m_nEntries] = wszData;
	m_nEntries++;
	return TRUE;
}


STDMETHODIMP CRequestStrList::get_HasKeys(VARIANT_BOOL *pfHasKeys)
{
	*pfHasKeys = (m_nEntries >= 1);
	return S_OK;
}


STDMETHODIMP CRequestStrList::put_Expires(DATE dtExpires)
{
	if ( RESPONSE_COOKIE_TYPE != m_dictType )
	{
		ASP_ERR(IDS_E_READ_ONLY)
		return DISP_E_EXCEPTION;
	}

	m_dateExpires = dtExpires;
	return S_OK;
}


STDMETHODIMP CRequestStrList::put_Domain( BSTR bstrDomain)
{
	if ( RESPONSE_COOKIE_TYPE != m_dictType )
	{
		ASP_ERR(IDS_E_READ_ONLY)
		return DISP_E_EXCEPTION;
	}

	//  Codepage note:  Since this is an http header we convert into an ANSI, not DBCS
	MyFree(m_pszDomain);
	if ( NULL == (m_pszDomain = MySzDupWtoA(bstrDomain)))
	{
		ASP_ERR(IDS_E_NOMEM);
		return DISP_E_EXCEPTION;
	}
	return S_OK;
}


STDMETHODIMP CRequestStrList::put_Path(  BSTR bstrPath)
{
	if ( RESPONSE_COOKIE_TYPE != m_dictType )
	{
		ASP_ERR(IDS_E_READ_ONLY)
		return DISP_E_EXCEPTION;
	}

	//  Codepage note:  Since this is an http header we convert into an ANSI, not DBCS
	MyFree(m_pszPath);
	if ( NULL == (m_pszPath = MySzDupWtoA(bstrPath)))
	{
		ASP_ERR(IDS_E_NOMEM);
		return DISP_E_EXCEPTION;
	}
	return S_OK;
}
