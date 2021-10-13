//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "aspmain.h"
const BOOL c_fUseCollections = TRUE;
const char cszDomain[]  = "; domain=";
const char cszPath[]    = "; path=";
const char cszPathDefault[]    = "; path=/";
const char cszExpires[] = "; expires";


int DBCSEncodeLen(const char *szSrc);


CRequestDictionary * CreateCRequestDictionary(DICT_TYPE dt, CASPState *pASPState, PSTR pszData)
{
	CRequestDictionary *pDict = NULL;

	if (FAILED (CoCreateInstance(CLSID_RequestDictionary,NULL,CLSCTX_INPROC_SERVER,IID_IRequestDictionary,(void**) &pDict)))
		return NULL;

	pDict->m_dictType = dt;
	pDict->m_pASPState = pASPState;
	pDict->m_pszRawData = pszData;		
	
	if (pszData)
		pDict->ParseInput();

	return pDict;
}

CRequestDictionary::CRequestDictionary() 
				    : CPtrMapper(0)
{
//	m_cRef = 0;
	m_pszRawData = NULL;
}

// We inherit from CPtrMapper class, which handles the deallocation of the list
CRequestDictionary::~CRequestDictionary()		
{
//	MyFree(m_pszRawData);	// don't free this, because we never made a copy of 
							// the data in the first place.
}


//  CRequestDictionary::get_Item
//  This is called when accessing a collection object (Request.QueryString/Form/Cookies),
//  Response.Cookies.  

//  If there is no argument, for example <% var = Request.Cookies %>, then 
//  varKey is of type VT_ERROR.  Like IIS, we return the raw, unformatted string
//  in this case, except for Response.Cookies, which doesn't support being accessed
//  raw.  

//  If the argument is an integer, the i(th) element is returned.  This is 
//  only valid for Form and QueryString objects.  (ie Request.Form(1) )
//  The element is a CRequestStrList, not the string itself, because subindexing
//  is possible and we need an object to handle this.  (ie Request.Form(1)(2) would
//  get the 2nd element of the 1st var declared in the Post input)

//  If the argument is a string, the string is looked up in the table that was
//  created on CookieParse or PostParse.  Again a CRequestStrList class element
//  is returned.

//  If no string exists, or if there is no data in the VT_ERROR case, then 
//  we return a CRequestStrList that is specified as an empty string.  We have
//  only one such object per request.  The reason we return this object rather
//  than an VT_EMPTY variant is that we still need to support methods such 
//  as Request.Form("No-Data").Count.  Without an object, the value is the string "",
//  it needs to be the integer 0.



STDMETHODIMP CRequestDictionary::get_Item(VARIANT varKey, VARIANT* pvarReturn)
{
	DEBUG_CODE_INIT;
	HRESULT ret = DISP_E_EXCEPTION;

    VariantInit(pvarReturn);
    VARIANT *pvarKey = &varKey;
	BSTR bstr = NULL;
	int iIndex;
	CRequestStrList *pStrList = NULL;
	DWORD vt;
	
	//  For Request.Cookies we convert from ANSI, otherwise change from DBCS.
	//  This is like ASP on IIS.	
	LONG lCodePage = (m_dictType == REQUEST_COOKIE_TYPE) ? CP_ACP : m_pASPState->m_lCodePage;

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
		{
			ASP_ERR(IDS_E_UNKNOWN);
			myleave(811);
		}
		vt = VT_I4;
	}

	// directly accessing object, no arguments.  
	// This is only valid for Request objects.  It gives the user
	// the original, unformatted/unparsed string from the client.
	if (  VT_ERROR == vt)
	{
		// This would be made on <% var = Response.Cookies %> (or Request.ServerVariables), no raw dumping.  Like IIS.
		if ( RESPONSE_COOKIE_TYPE == m_dictType || SERVER_VARIABLE_TYPE == m_dictType )
		{
			ASP_ERR(IDS_E_EXPECTED_STRING);
			myleave(802);
		}	

		if (m_pszRawData)
		{					
			if ( FAILED( SysAllocStringFromSz(m_pszRawData, 0, &bstr, lCodePage)))
			{
				ASP_ERR(IDS_E_NOMEM);
				myleave(803);
			}

			V_VT(pvarReturn) = VT_BSTR;
			V_BSTR(pvarReturn) = bstr;
		}
		else		//  no data of requested type, ie Request.Form but not POST data sent
		{
			//  Point it at the empty string object, only one of these per ASP session
			V_VT(pvarReturn) = VT_DISPATCH;		
			m_pASPState->m_pEmptyString->QueryInterface(IID_IRequestStrList, (void **) (&V_DISPATCH(pvarReturn)));
		}
	}
	else if (VT_I4 == vt)		//lookup based on index 
	{
		// Can't index cookies or server variables based on an integer
		if ( RESPONSE_COOKIE_TYPE == m_dictType ||
			 REQUEST_COOKIE_TYPE  == m_dictType ||
			 SERVER_VARIABLE_TYPE == m_dictType )
		{
			ASP_ERR(IDS_E_EXPECTED_STRING);
			myleave(804);
		}	

		iIndex = V_I4(pvarKey);
		if (iIndex <= 0 ||
			iIndex > m_nEntries)
		{
			ASP_ERR(IDS_E_BAD_INDEX);
			myleave(804);
		}

		pStrList = (CRequestStrList *)  (m_pMapInfo[iIndex - 1].pbData);
        pStrList->QueryInterface(IID_IRequestStrList, (void **) (&V_DISPATCH(pvarReturn)));
		V_VT(pvarReturn) = VT_DISPATCH;		
	}	// VT_I4
	else if (VT_BSTR == vt)		// Lookup based on string.
	{
		if (SERVER_VARIABLE_TYPE == m_dictType) 
		{
			GetServerVariables(V_BSTR(pvarKey),pvarReturn);
		}
		else if (Lookup( V_BSTR(pvarKey), (void **) &pStrList))
		{
			pStrList->QueryInterface(IID_IRequestStrList, (void **) (&V_DISPATCH(pvarReturn)));
			V_VT(pvarReturn) = VT_DISPATCH;			
		}		
		else
		{	
			m_pASPState->m_pEmptyString->QueryInterface(IID_IRequestStrList, (void **) (&V_DISPATCH(pvarReturn)));
			V_VT(pvarReturn) = VT_DISPATCH;
		}
	}	// VT_BSTR
	//  vt not an integer or string.  We follow IIS's lead as far as what error to report.
	else		
	{	
		ASP_ERR(IDS_E_EXPECTED_STRING);		
		myleave(806);
	}

	ret = S_OK;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CRequestDictionary::get_Item failed, err = %d, GLE=%X\r\n",
							  err,GetLastError()));
	
	VariantClear(&varKeyCopy);
	return ret;
}

//  put_Item.  Used for assigning values to Cookie response data.

//  This will be called on cases 
//  Response.Cookies = "Value"
//  Response.Cookies("index") = "Value"

//  It will NOT be called on Response.Cookies("index1")("index2") = "value",
//  instead this will be a CRequestDictionary::get_Item with "index1", then
//  a CRequestStrList::put_Item with "index2", and "value" through object created


STDMETHODIMP CRequestDictionary::put_Item(VARIANT varKey, BSTR bstrValue)
{
	DEBUG_CODE_INIT;
	VARIANT *pvarKey = &varKey;
    HRESULT ret = DISP_E_EXCEPTION;
	int iIndex =0;
	WCHAR *wszValue = NULL;
	WCHAR *wszName  = NULL;
	CRequestStrList *pStrList = NULL;
	DWORD vt;

    VARIANT varKeyCopy;
    VariantInit(&varKeyCopy);
    vt = V_VT(pvarKey);

	if (NULL == bstrValue)
		myretleave(S_OK,809);

	if (m_dictType != RESPONSE_COOKIE_TYPE)
	{
		ASP_ERR(IDS_E_READ_ONLY);
		myleave(810);
	}
	
	if (TRUE == m_pASPState->m_fSentHeaders)
	{
		ASP_ERR(IDS_E_SENT_HEADERS);
		myleave(808);
	}

    // Use VariantResolveDispatch which will:
    //
    //     *  Copy BYREF variants for us using VariantCopyInd
    //     *  handle E_OUTOFMEMORY for us
    //     *  get the default value from an IDispatch, which seems
    //        like an appropriate conversion.
    //
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

	// this would correspond to Response.Cookies = "Value", or Response.Cookies(5) = "value",
	// No support in IIS or in CE ASP.
	if (vt != VT_BSTR)
	{
		ASP_ERR(IDS_E_EXPECTED_STRING);
		myleave(812);
	}

	if (NULL == (wszValue = MySzDupW(bstrValue)))
	{
		ASP_ERR(IDS_E_NOMEM);
		myleave(817);
	}

	if ( Lookup( V_BSTR(pvarKey), (void**) &pStrList))
	{
		if (FALSE == pStrList->AddStringToArray(wszValue))
		{
			ASP_ERR(IDS_E_NOMEM);
			myleave(815);
		}
		// Do NOT add reference here, we're just overwriting it!
	}
	else 	// new element
	{
		if (NULL == (wszName = MySzDupW( V_BSTR(pvarKey))))
		{
			ASP_ERR(IDS_E_NOMEM);
			myleave(818);
		}
		pStrList = CreateCRequestStrList(m_pASPState,wszValue,m_dictType);
		
		if ( NULL == pStrList || FALSE == Set(wszName, (void *) pStrList))
		{
			MyFree(wszName);
			ASP_ERR(IDS_E_NOMEM);
			myleave(816);
		}
	}		
	
	ret = S_OK;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CRequestDictionary::put_Item failed, err = %d, GLE=%X\r\n",
							 err, GetLastError()));

	if (FAILED(ret))
		MyFree(wszValue);
		
	VariantClear(&varKeyCopy);
	return ret;
}

STDMETHODIMP CRequestDictionary::get_Count(int* cStrRet)
{
	if (SERVER_VARIABLE_TYPE == m_dictType)
		return E_NOTIMPL;
	
	*cStrRet = m_nEntries;
	return S_OK;
}

// Note: WinCE does not support enumeration through collection objects in ASP, unlike IIS.
STDMETHODIMP CRequestDictionary::get_Key(VARIANT VarKey, VARIANT* pvar)
{
	ASP_ERR(IDS_E_NOT_IMPL);
	return E_NOTIMPL;
}

STDMETHODIMP CRequestDictionary::get__NewEnum(IUnknown** ppEnumReturn)
{
	ASP_ERR(IDS_E_NOT_IMPL);
	return E_NOTIMPL;
}

//  CRequest::ParseInput()
//  This takes the raw data receieved from the POST, query string, or incoming cookies
//  and breaks it into individual items that can be more easily indexed.
BOOL CRequestDictionary::ParseInput()
{
	DEBUG_CODE_INIT;
	PSTR pszName = NULL;
	PSTR pszValue = NULL;
	CRequestStrList *pStrList = NULL;
	PSTR pszTrav = NULL;	
	PSTR pszTempRawData = NULL;
	WCHAR *wszName = NULL;		// Don't free at end of function
	WCHAR *wszValue = NULL;		// Don't free at end of function
	BOOL ret = FALSE;
	PSTR pszSeperator = (m_dictType == REQUEST_COOKIE_TYPE) ? ";" : "&" ;


	// Codepage Note:
	// We follow IIS's lead.  For POST / QueryString data we'll let 
	// them send it as DBCS characters, so we use the script's set codepage. 
	LONG lCodePage = (m_dictType == REQUEST_COOKIE_TYPE) ? CP_ACP : m_pASPState->m_lCodePage;


	ASP_ERR(IDS_E_NOMEM);		// only error that can occur

	// DecodeFromURL mangles the string, so we work with a copy of it (this allows
	// us to send raw data back to user on request)
	if (NULL == (pszTrav = pszTempRawData = MySzDupA(m_pszRawData)))
		myleave(1009);

	while ( *pszTrav != '\0')
	{		
		// If there is no "=" in the data sent from browser, we don't report an error
		// to script writer, though obviously the data will be corrupt in such a case.
		
		svsutil_DecodeFromURL( &pszTrav, "=", pszName = pszTrav, lCodePage);
		svsutil_DecodeFromURL( &pszTrav, pszSeperator, pszValue = pszTrav, lCodePage);		

		if ((NULL == (wszName = MySzDupAtoW(pszName))) ||
		    (NULL == (wszValue = MySzDupAtoW(pszValue))))
		{
			MyFree(wszName);		// this wouldn't get freed in error case
			myleave(1003);
		}

		// name is already in data set
		if ( Lookup(wszName, (void **) &pStrList) )
		{
			if (FALSE == pStrList->AddStringToArray(wszValue))
			{
				myleave(1001);
			}
			MyFree(wszName);
			// Don't AddRef here!
		}
		else 	// new element
		{
			pStrList = CreateCRequestStrList(m_pASPState,wszValue,m_dictType);
			
			if ( NULL == pStrList || FALSE == Set(wszName, (void *) pStrList))
			{
				myleave(1002);
			}
			// pStrList->AddRef();  Now that we use CoCreateInstance instead of new, don't do AddRef
		}	
	}

	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: ParseInput failed, err = %d\r\n",err));

	if (ret)
		ASP_ERR(IDS_E_DEFAULT);		// set error back to default
		
	MyFree(pszTempRawData);

	return ret;
}

BOOL CRequestDictionary::WriteCookies()
{
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;

	SYSTEMTIME st;
	CHAR szCookie[2048];  // buffer to hold entire cookie
	CHAR szExpires[50];

	PSTR pszCookie = szCookie;
		
	DWORD cbCookieBuf = sizeof(szCookie);		// size of cookie buffer
	DWORD cbLen;								// length of data in cookie buffer

	PSTR pszTrav;
	CHAR szName[512];
	CHAR szValue[1024];
	PSTR pszName  = szName;
	PSTR pszValue = szValue;
	DWORD cbName;
	DWORD cbValue;
	DWORD cbNameBuf  = sizeof(szName);
	DWORD cbValueBuf = sizeof(szValue);
	DWORD cbNeeded = 0;
	
	int i;

	CRequestStrList *pList = NULL;

	if (m_nEntries == 0)
		return TRUE;
	ASP_ERR(IDS_E_NOMEM);   // only error that can occur

	for (i = 0; i < m_nEntries; i++)
	{
		cbLen = 0;
		pList = (CRequestStrList *) m_pMapInfo[i].pbData;
		DEBUGCHK(pList != NULL && pList->m_nEntries != 0 && pList->m_wszData);

		if (0 == MyW2A(m_pMapInfo[i].wszKey, pszName, cbNameBuf))
		{
			if (0 == (cbNeeded = MyW2A(m_pMapInfo[i].wszKey,0,0)))
				myleave(1040);

			cbNameBuf = cbNeeded + 1024;   // allocate needed buf + a little extra
			if (pszName != szName)
				pszName = MyRgReAlloc(CHAR,pszName,cbNameBuf,cbNeeded);
			else
				pszName = MyRgAllocNZ(CHAR,cbNameBuf);

			if (!pszName)
				myleave(1041);
	
			MyW2A(m_pMapInfo[i].wszKey,pszName, cbNameBuf);
		}

		if (0 == MyW2A(pList->m_wszData, pszValue, cbValueBuf))
		{
			if (0 == (cbNeeded = MyW2A(pList->m_wszData,0,0)))
				myleave(1042);

			cbValueBuf = cbNeeded + 1024;   // allocate needed buf + a little extra
			if (pszValue != szValue)
				pszValue = MyRgReAlloc(CHAR,pszValue,cbValueBuf,cbNeeded);
			else
				pszValue = MyRgAllocNZ(CHAR,cbValueBuf);

			if (!pszValue)
				myleave(1043);
				
			MyW2A(pList->m_wszData, pszValue, cbValueBuf);
		}
			
		cbName  = URLEncodeLen(pszName);
		cbValue = URLEncodeLen(pszValue); 

		cbNeeded = cbName + cbValue + 10 +
				  (pList->m_pszDomain   ? DBCSEncodeLen(pList->m_pszDomain) + CONSTSIZEOF(cszDomain) : 0) +
				  (pList->m_pszPath     ? DBCSEncodeLen(pList->m_pszPath)   + CONSTSIZEOF(cszPath)   : CONSTSIZEOF(cszPathDefault)) +
				  (pList->m_dateExpires ? sizeof(szExpires) + CONSTSIZEOF(cszExpires) + 10           : 0);

		// Reallocate the memory if we need to.
		if (cbCookieBuf < cbNeeded)
		{
			cbCookieBuf = cbNeeded + 1024;
			if (pszCookie != szCookie) 
				pszCookie = MyRgReAlloc(CHAR,pszCookie,cbCookieBuf,cbNeeded);
			else
				pszCookie = MyRgAllocNZ(CHAR,cbCookieBuf);

			if (!pszCookie)
				myleave(1048);
		}
		
		pszTrav = URLEncode(pszCookie,pszName);
		*pszTrav++ = '=';
		pszTrav = URLEncode(pszTrav,pszValue);

		if (pList->m_pszDomain)
		{
			strcpy(pszTrav,cszDomain);
			pszTrav += CONSTSIZEOF(cszDomain);
			pszTrav = DBCSEncode(pszTrav, pList->m_pszDomain);	
		}
		
		// Expires header, always an ANSI string anyway.
		if (pList->m_dateExpires != 0)
		{
			VariantTimeToSystemTime(pList->m_dateExpires,&st);

			sprintf(szExpires, cszDateOutputFmt, rgWkday[st.wDayOfWeek], st.wDay, 
							rgMonth[st.wMonth], st.wYear, st.wHour, st.wMinute, st.wSecond);

			pszTrav += sprintf(pszTrav,"; Expires=\"%s\"",szExpires);
		}

		// we always have a path set, even if ASP script didn't request one
		if ( pList->m_pszPath)
		{
			strcpy(pszTrav,cszPath);	
			pszTrav += CONSTSIZEOF(cszPath);
			pszTrav = DBCSEncode(pszTrav, pList->m_pszPath);
		}
		else
		{
			//  Setting the path to "/" will always match, so browser will always
			//  return this cookie data. 
			strcpy(pszTrav,cszPathDefault);	
			pszTrav += CONSTSIZEOF(cszPathDefault);
		}
		DEBUGCHK((pszTrav - pszCookie) < (int)cbNeeded); 

		// The web server tacks ":" onto end of Set-Cookie by default. 
		if (FALSE == m_pASPState->m_pACB->AddHeader(m_pASPState->m_pACB->ConnID,
										   "Set-Cookie",pszCookie))
		{
			myleave(1053);
		}
	}

	ret = TRUE;
done:
	if (pszName != szName)
		MyFree(pszName);

	if (pszValue != szValue)		
		MyFree(pszValue);

	if (pszCookie != szCookie)
		MyFree(pszCookie);

	if (ret)
		ASP_ERR(IDS_E_DEFAULT);

	return ret;
}

/*===================================================================
DBCSEncode

DBCS Encode a string by escaping characters with the upper bit
set - Basically used to convert 8 bit data to 7 bit in contexts
where full encoding is not needed.

Parameters:
    szDest - Pointer to the buffer to store the URLEncoded string
    szSrc  - Pointer to the source buffer

Returns:
    A pointer to the NUL terminator is returned.
===================================================================*/

char *DBCSEncode(char *szDest, const char *szSrc)
    {
    char hex[] = "0123456789ABCDEF";

    if (!szDest)
        return NULL;
    if (!szSrc)
        {
        *szDest = '\0';
        return szDest;
        }

    while (*szSrc)
        {
        if ((*szSrc & 0x80) || (!isalnum(*szSrc) && !strchr("/$-_.+!*'(),", *szSrc)))
            {
            *szDest++ = '%';
            *szDest++ = hex[BYTE(*szSrc) >> 4];
            *szDest++ = hex[*szSrc++ & 0x0F];
            }

        else
            *szDest++ = *szSrc++;
        }

    *szDest = '\0';
    return szDest;
    }



/*===================================================================
DBCSEncodeLen

Return the storage requirements for a DBCS encoded string
(url-encoding of characters with the upper bit set ONLY)

Parameters:
    szSrc  - Pointer to the string to URL Encode

Returns:
    the number of bytes required to encode the string
===================================================================*/

int DBCSEncodeLen(const char *szSrc)
    {
    int cbURL = 1;      // add terminator now

    if (!szSrc)
        return 0;

    while (*szSrc)
        {
        cbURL += ((*szSrc & 0x80) || (!isalnum(*szSrc) && !strchr("/$-_.+!*'(),", *szSrc)))? 3 : 1;
        ++szSrc;
        }

    return cbURL;
    }


STDMETHODIMP CRequestDictionary::GetServerVariables(BSTR bstrName, VARIANT* pvarReturn) {
	DEBUG_CODE_INIT;
	PASP_CONTROL_BLOCK pACB = m_pASPState->m_pACB;
	HRESULT ret = DISP_E_EXCEPTION;
	PSTR pszName = NULL;

	CHAR szBuf[256];
	PSTR pszValue = szBuf;
	DWORD cbValue = sizeof(szBuf);
	BOOL fRet;
	BSTR bstr = NULL;

	DEBUGMSG(ZONE_REQUEST,(L"ASP: In Server Variables!!\r\n"));

	// Codepage note:  We use ANSI to do this conversion, since headers are ANSI strs
	pszName = MySzDupWtoA(bstrName);
	if (NULL == pszName)
	{
		myleave(120);
	}

	// get length of buffer.  cbValue is strlen of variable's value plus \0.
	fRet = pACB->GetServerVariable(pACB->ConnID, pszName, pszValue, &cbValue);

	if (!fRet)
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			myretleave(S_OK,121);

		pszValue = MyRgAllocNZ(CHAR,cbValue);
		if (NULL == pszValue)
		{
			myleave(122);
		}
		pszValue[cbValue - 1] = '\0';

		if (FALSE == pACB->GetServerVariable(pACB->ConnID, pszName, pszValue, &cbValue))
		{
			DEBUGCHK(FALSE);			// This should never happen		
			myleave(123);
		}
	}

	if ( FAILED(SysAllocStringFromSz(pszValue, cbValue, &bstr, CP_ACP)))
	{
		myleave(124);
	}

	V_VT(pvarReturn)   = VT_BSTR;
	V_BSTR(pvarReturn) = bstr;
	ret = S_OK;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: GetServerVariables failed, err = %d, GLE = %X, pszName = <<%a>>\r\n",
							   err, GetLastError(), pszName));

	if (ret != S_OK)
	{
		// set it to an empty string.
		m_pASPState->m_pEmptyString->QueryInterface(IID_IRequestStrList, (void **) (&V_DISPATCH(pvarReturn)));
		V_VT(pvarReturn) = VT_DISPATCH;
	}

	if (szBuf != pszValue)
		MyFree(pszValue);

	MyFree(pszName);
	return ret;
}
