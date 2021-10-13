//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: response.cpp
Abstract:  Implements Response script object.
--*/

#include "aspmain.h"

/////////////////////////////////////////////////////////////////////////////
// CResponse

// FILETIME (100-ns intervals) to minutes (10 x 1000 x 1000 x 60)
#define FILETIME_TO_MINUTES ((__int64)600000000L)


CResponse::CResponse()
{
	m_pASPState = (CASPState *) TlsGetValue(g_dwTlsSlot);
	DEBUGCHK(m_pASPState);

	if ( c_fUseCollections )
	{
		m_pCookie = CreateCRequestDictionary(RESPONSE_COOKIE_TYPE,m_pASPState,NULL);

		if (! m_pCookie)
			return ;
			
		m_pASPState->m_pResponseCookie = m_pCookie; 									   
		m_pCookie->AddRef();	
	}
	else
	{
		m_pCookie = NULL;
	}	
}

CResponse::~CResponse()
{
	if (m_pCookie)
		m_pCookie->Release();
}


STDMETHODIMP CResponse::get_Cookies(IRequestDictionary **ppDictReturn)
{
	DEBUGMSG(ZONE_REQUEST,(L"ASP: In CResponse::get_Cookies!!\r\n"));
//	DebugBreak();
	
	if (m_pCookie)
		return m_pCookie->QueryInterface(IID_IRequestDictionary,(void **) ppDictReturn);

	return E_NOTIMPL;
}


// NOTE:  We store a copy of fBufferedResponse in both ASP and httpd, the only
// variable we do this with.  We do this because the Write fcns in ASP need access
// to it, as do the write fcns that are buried deep with calls to httpd
STDMETHODIMP CResponse::get_Buffer(VARIANT_BOOL *pVal)
{
	if (m_pASPState->m_fBufferedResponse)
		*pVal = 1;
	else
		*pVal = 0;
		
	return S_OK;
}


//  Note:  If user wants to have a buffered response, the line that sets this
//  needs to be the 1st or right after the preproc directives (though spaces are OK).
//  Otherwise the default is no buffering, and data will have been sent by the
//  time this call is made.  This is like IIS 4.0.
STDMETHODIMP CResponse::put_Buffer(VARIANT_BOOL newVal)
{
	if (m_pASPState->m_fSentHeaders == TRUE)
	{
		ASP_ERR(IDS_E_SENT_HEADERS);
		return DISP_E_EXCEPTION;
	}

	//  Should never die, this is only an accessor.
	if ( ! m_pASPState->m_pACB->SetBuffer(m_pASPState->m_pACB->ConnID, newVal))
	{
		ASP_ERR(IDS_E_HTTPD);
		return DISP_E_EXCEPTION;
	}

	m_pASPState->m_fBufferedResponse = newVal;
	return S_OK;
}

STDMETHODIMP CResponse::get_Expires(VARIANT * pvarExpiresTimeRet)
{
	SYSTEMTIME st;
	__int64 ft;

	// no expires time set
	if (0 == m_pASPState->m_iExpires)
	{
		V_VT(pvarExpiresTimeRet) = VT_EMPTY;
		return S_OK;
	}     


	GetSystemTime(&st);
	SystemTimeToFileTime(&st, (FILETIME*) &ft);
	
    ft = (m_pASPState->m_iExpires - ft) / FILETIME_TO_MINUTES;

    if (ft < 0)
    	ft = 0;

    V_VT(pvarExpiresTimeRet) = VT_I4;
    V_I4(pvarExpiresTimeRet) = (long) ft;

	return S_OK;
}


STDMETHODIMP CResponse::put_Expires(long lExpiresMinutes)
{
	SYSTEMTIME st;
	__int64    ft;

	if (m_pASPState->m_fSentHeaders)
	{
		ASP_ERR(IDS_E_SENT_HEADERS);
		return DISP_E_EXCEPTION;
	}


	// Treat 0 and negative values like "Expires now"
	if (lExpiresMinutes < 0)
		lExpiresMinutes  = 0;		
	
	GetSystemTime(&st); 
	SystemTimeToFileTime(&st, (FILETIME*)&ft);

	ft += FILETIME_TO_MINUTES * lExpiresMinutes;

	// If there are multiple calls to this and/or ExpiresAbsolute, we use the
	// time that comes nearest in the future 

	if ( ft < m_pASPState->m_iExpires ||  m_pASPState->m_iExpires == 0 )
		m_pASPState->m_iExpires =  ft;

	return S_OK;
}


STDMETHODIMP CResponse::get_ExpiresAbsolute(VARIANT *pvarTimeRet)
{
	SYSTEMTIME st;
	OLECHAR wszDate[50];
	DWORD cbDate = 50;
	int i;
	DWORD dw = 0;

	V_VT(pvarTimeRet) = VT_DATE;

	// no expires time set
	if (0 == m_pASPState->m_iExpires)
	{
		return S_OK;
	}     

	FileTimeToSystemTime( (FILETIME*) &m_pASPState->m_iExpires, &st);


	// Note:  Docs require 1st parameter ignrored, docs want LOCALE_SYSTEM_DEFAULT there
	// This format is for Variant conversion, has nothing to do with httpd headers
	i = GetDateFormat(LOCALE_SYSTEM_DEFAULT , 0, &st, L"M'/'d'/'yyyy' '",wszDate, cbDate);

	dw = GetLastError();
	cbDate -= i;
	
	i += GetTimeFormat(LOCALE_SYSTEM_DEFAULT , 0,&st, L"h':'mm':'ss' '",
					  wszDate + i - 1, cbDate);

	if (st.wHour < 12)
	{
		wcscpy(wszDate + i, L"AM");
	}
	else
	{
		wcscpy(wszDate + i, L"PM");
	}
	
	VarDateFromStr(wszDate,LOCALE_SYSTEM_DEFAULT,0, &V_DATE(pvarTimeRet));

	return S_OK;
}


//  Notes:  DATE is made up of 2 adjacent doubles.  In bottom 32 bits there's the date, 
//  top 32 bits specify the time.  It's possible for script writer to leave one of these 
//  blank, ie only set the date or time to expire.  

//  If no date is specified, we assume expires today.  
//  If no time is specified, we assume expires midnight of the date set.
STDMETHODIMP CResponse::put_ExpiresAbsolute(DATE dtExpires)
{
	HRESULT hr = DISP_E_EXCEPTION;
	SYSTEMTIME st;
	__int64 ft;

	if (m_pASPState->m_fSentHeaders)
	{
		ASP_ERR(IDS_E_SENT_HEADERS);
		return DISP_E_EXCEPTION;
	}

	VariantTimeToSystemTime(dtExpires,&st);
	
	// time specified but no date (assume today)
	if (int(dtExpires) == 0)
	{
		SYSTEMTIME stToday;
		GetSystemTime(&stToday);

		// set the date of the original structure
		st.wYear = stToday.wYear;
		st.wMonth = stToday.wMonth;
		st.wDayOfWeek = stToday.wDayOfWeek;
		st.wDay = stToday.wDay;
	}
	// If the is date specified, time=0 would be midnight.  No changes needed.

	SystemTimeToFileTime(&st, (FILETIME*)&ft);

	// If there are multiple calls to this and/or Expires, we use the
	// time that comes nearest in the future 

	if ( ft < m_pASPState->m_iExpires ||  m_pASPState->m_iExpires == 0)
		m_pASPState->m_iExpires =  ft;

	return S_OK;
}


STDMETHODIMP CResponse::get_Status(BSTR *pVal)
{
	// they've set a custom string, use it
	if (NULL != m_pASPState->m_bstrResponseStatus)
	{
		*pVal = SysAllocString(m_pASPState->m_bstrResponseStatus);
		if (NULL ==  *pVal)
		{
			ASP_ERR(IDS_E_NOMEM);
			return DISP_E_EXCEPTION;
		}

		return S_OK;
	}

	// if no custom val set then we send a 200 by default, like IIS ASP.
	*pVal = SysAllocString(L"200 OK");
	if (NULL == *pVal)
	{
		ASP_ERR(IDS_E_NOMEM);
		return DISP_E_EXCEPTION;
	}

	return S_OK;
}


STDMETHODIMP CResponse::put_Status(BSTR newVal)
{
	if (NULL == newVal)
		return S_OK;

	if (TRUE == m_pASPState->m_fSentHeaders)
	{
		ASP_ERR(IDS_E_SENT_HEADERS);
		return DISP_E_EXCEPTION;
	}

	if (m_pASPState->m_bstrResponseStatus)
		SysFreeString(m_pASPState->m_bstrResponseStatus);

	m_pASPState->m_bstrResponseStatus = SysAllocString(newVal);
	if (NULL == m_pASPState->m_bstrResponseStatus)
	{
		ASP_ERR(IDS_E_NOMEM);
		return DISP_E_EXCEPTION;
	}
	
	return S_OK;
}


STDMETHODIMP CResponse::get_ContentType(BSTR *pVal)
{
	// they've set a custom string, use it
	if (NULL != m_pASPState->m_bstrContentType)
	{
		*pVal = SysAllocString(m_pASPState->m_bstrContentType);
		if (NULL ==  *pVal)
		{
			ASP_ERR(IDS_E_NOMEM);
			return DISP_E_EXCEPTION;
		}
	}
	else		// Use the default content type
	{
		if (FAILED (SysAllocStringFromSz((PSTR)cszTextHTML,0,pVal,CP_ACP)))
		{			
			ASP_ERR(IDS_E_NOMEM);
			return DISP_E_EXCEPTION;
		}
	}
	return S_OK;
}

STDMETHODIMP CResponse::put_ContentType(BSTR newVal)
{
	if (NULL == newVal)
		return S_OK;

	if (TRUE == m_pASPState->m_fSentHeaders)
	{
		ASP_ERR(IDS_E_SENT_HEADERS);
		return DISP_E_EXCEPTION;
	}

	if (m_pASPState->m_bstrContentType)
		SysFreeString(m_pASPState->m_bstrContentType);

	m_pASPState->m_bstrContentType = SysAllocString(newVal);
	if (NULL == m_pASPState->m_bstrContentType)
	{
		ASP_ERR(IDS_E_NOMEM);
		return DISP_E_EXCEPTION;
	}
	
	return S_OK;
}

STDMETHODIMP CResponse::get_Charset(BSTR *pVal)
{
	// they've set a custom string, use it
	if (NULL != m_pASPState->m_bstrCharSet)
	{
		*pVal = SysAllocString(m_pASPState->m_bstrCharSet);
		if (NULL ==  *pVal)
		{
			ASP_ERR(IDS_E_NOMEM);
			return DISP_E_EXCEPTION;
		}
	}
	return S_OK;
}

STDMETHODIMP CResponse::put_Charset(BSTR newVal)
{
	if (NULL == newVal)
		return S_OK;

	if (m_pASPState->m_fSentHeaders)
	{
		ASP_ERR(IDS_E_SENT_HEADERS);
		return DISP_E_EXCEPTION;
	}

	if (m_pASPState->m_bstrCharSet)
		SysFreeString(m_pASPState->m_bstrCharSet);

	m_pASPState->m_bstrCharSet = SysAllocString(newVal);
	if (NULL == m_pASPState->m_bstrCharSet)
	{
		ASP_ERR(IDS_E_NOMEM);
		return DISP_E_EXCEPTION;
	}
	
	return S_OK;
}

STDMETHODIMP CResponse::AddHeader(BSTR bstrName, BSTR bstrValue)
{
	DEBUG_CODE_INIT;
	PSTR pszName = NULL;
	PSTR pszValue = NULL;

	DWORD ret = DISP_E_EXCEPTION;

	if (!bstrName || !bstrValue)
		myretleave(S_OK,152);

	if (m_pASPState->m_fSentHeaders)
	{
		ASP_ERR(IDS_E_SENT_HEADERS);
		myleave(154);
	}
	
	//  Codepage note:  Since this is an http header, we convert to ANSI string 
	pszName = MySzDupWtoA(bstrName);
	pszValue = MySzDupWtoA(bstrValue);

	if (NULL == pszName ||
	    NULL == pszValue)
	{
		ASP_ERR(IDS_E_NOMEM);
		myleave(151);
	}

	if (FALSE == m_pASPState->m_pACB->AddHeader(m_pASPState->m_pACB->ConnID, 
												   pszName, pszValue))
	{
		ASP_ERR(IDS_E_NOMEM);		// only reason call can fail
		myleave(155);
	}

	ret = S_OK;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CResponse::AddHeaders died, err = %d, GLE=%X\r\n",err,GetLastError()));

	MyFree(pszName);
	MyFree(pszValue);
		
	return ret;
}


STDMETHODIMP CResponse::AppendToLog(BSTR bstrLogData)
{
	HRESULT ret = DISP_E_EXCEPTION;
	// Codepage note:  IIS converts this to ANSI string to keep log in ANSI format, so do we.
	PSTR pszLogData = NULL;
	DWORD cbLogData = 0;

	if (NULL == bstrLogData)
	{
		ret = S_OK;
		goto done;
	}

	pszLogData = MySzDupWtoA(bstrLogData);
	if (NULL == pszLogData)
	{
		ASP_ERR(IDS_E_NOMEM);
		goto done;
	}

	cbLogData = strlen(pszLogData);
	if (FALSE == m_pASPState->m_pACB->ServerSupportFunction(m_pASPState->m_pACB->ConnID,
									  HSE_APPEND_LOG_PARAMETER,(LPVOID) pszLogData, 
									  &cbLogData, NULL))
	{							
		ASP_ERR(IDS_E_NOMEM);		
		goto done;
	}

	ret = S_OK;
done:
	MyFree(pszLogData);

	return ret;
}


STDMETHODIMP CResponse::Clear()
{
	if (FALSE == m_pASPState->m_fBufferedResponse)
	{
		ASP_ERR(IDS_E_BUFFER_ON);
		return DISP_E_EXCEPTION;
	}

	if (FALSE == m_pASPState->m_pACB->Clear(m_pASPState->m_pACB->ConnID))
	{
		ASP_ERR(IDS_E_HTTPD);		
		return DISP_E_EXCEPTION;
	}
	
	return S_OK;
}


STDMETHODIMP CResponse::End()
{
	EXCEPINFO ex = {0, 0, NULL, NULL, NULL, 0, NULL, NULL, E_THREAD_INTERRUPT};

	if (FALSE == m_pASPState->m_fSentHeaders)
	{
		m_pASPState->SendHeaders();
	}
	
	m_pASPState->m_pACB->Flush(m_pASPState->m_pACB->ConnID);
	m_pASPState->m_piActiveScript->InterruptScriptThread(SCRIPTTHREADID_CURRENT ,&ex,0);
														
	return S_OK;
}


STDMETHODIMP CResponse::Flush()
{
	if (FALSE == m_pASPState->m_fBufferedResponse)
	{
		ASP_ERR(IDS_E_BUFFER_ON);
		return DISP_E_EXCEPTION;
	}

	if (m_pASPState->m_fSentHeaders == FALSE)
	{
		if (FALSE == m_pASPState->SendHeaders())
			return DISP_E_EXCEPTION;
	}


	if (FALSE == m_pASPState->m_pACB->Flush(m_pASPState->m_pACB->ConnID))
	{
		ASP_ERR(IDS_E_HTTPD);		// something went wrong on send(), given
									// this it's unlikely error code will make
									// it to user either but we'll try
		return DISP_E_EXCEPTION;
	}

	return S_OK;
}


STDMETHODIMP CResponse::Redirect(BSTR bstrURL)
{
	DEBUG_CODE_INIT;
	HRESULT ret = DISP_E_EXCEPTION;
	PSTR pszRedirect = NULL;

	if (NULL == bstrURL)
		return S_OK;

	// after sending headers, it's too late for this
	if (TRUE == m_pASPState->m_fSentHeaders)
	{
		ASP_ERR(IDS_E_SENT_HEADERS);
		myleave(171);
	}

	// Codepage note:  Use specified code page, like IIS.
	pszRedirect = MySzDupWtoA(bstrURL,-1,m_pASPState->m_lCodePage);
	if (NULL == pszRedirect)
	{
		ASP_ERR(IDS_E_NOMEM);
		myleave(172);
	}

	if (! m_pASPState->SendHeaders(FALSE))
		myleave(175);

	if (FALSE == m_pASPState->m_pACB->ServerSupportFunction(m_pASPState->m_pACB->ConnID, 
											   HSE_REQ_SEND_URL_REDIRECT_RESP,
											   (LPVOID) pszRedirect,0,0))
	{
		ASP_ERR(IDS_E_HTTPD);
		myleave(173);
	}

	// Make sure web server doesn't send any data after this request.
	m_pASPState->m_fSentHeaders = TRUE;
	m_pASPState->m_pACB->Clear(m_pASPState->m_pACB->ConnID);
	
	ret = S_OK;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CASPState::Redirect failed, err = %d\r\n",err));

	MyFree(pszRedirect);

	if (SUCCEEDED(ret))
		End();		// CResponse::End. don't process anything more on this page 
					// on success.  
	return ret;
}


// No Unicode to ASCII conversion.  Send data raw.
STDMETHODIMP CResponse::BinaryWrite(VARIANT varData)
{
	return m_pASPState->BinaryWrite(varData);
}


STDMETHODIMP CASPState::BinaryWrite(VARIANT varData)
{
	DEBUG_CODE_INIT;
	HRESULT ret = DISP_E_EXCEPTION;
	HRESULT hr;
	VARTYPE vt;
	VARIANT *pvarKey;
	DWORD nLen = 0;
    VARIANT varKeyCopy;
    SAFEARRAY* pvarBuffer = NULL;
    long lLBound = 0;
    long lUBound = 0;
    void *lpData = NULL;
    DWORD cch = 0;
    
    VariantInit(&varKeyCopy);
	vt = varData.vt;

	if (FAILED(VariantResolveDispatch(&varKeyCopy, &varData)))
	{
		m_aspErr = IDS_E_PARSER;
		myleave(100);
	}	
	
	pvarKey = &varKeyCopy;
	vt = V_VT(pvarKey);

	if (vt != (VT_ARRAY|VT_UI1))
	{
		if (FAILED(hr = VariantChangeType(pvarKey, pvarKey,0, VT_ARRAY|VT_UI1)))
		{
			if (GetScode(hr) == E_OUTOFMEMORY)
			{
				m_aspErr = IDS_E_NOMEM;
				myleave(101);
			}
			else
			{
				m_aspErr = IDS_E_UNKNOWN;
				myleave(107);
			}	
			// 	otherwise it's an unknown error, the default
		}
	}
    pvarBuffer = V_ARRAY(pvarKey);

	// If any of these fail, we leave the error as the default Unknown
    if (SafeArrayGetDim(pvarBuffer) != 1)
    	myleave(102);    

    if (FAILED(SafeArrayGetLBound(pvarBuffer, 1, &lLBound)))
		myleave(104);

    if (FAILED(SafeArrayGetUBound(pvarBuffer, 1, &lUBound)))
		myleave(105);
   
    if (FAILED(SafeArrayAccessData(pvarBuffer, &lpData)))
		myleave(106);

    cch = lUBound - lLBound + 1;
    DEBUGCHK(cch > 0);

	if (m_fBufferedResponse == FALSE && 
	    m_fSentHeaders == FALSE)
	{
		if (FALSE == SendHeaders())
			myleave(116);
	}

	if (! m_pACB->WriteClient(m_pACB->ConnID, (LPVOID) lpData, &cch, 0))
	{
		m_aspErr = IDS_E_HTTPD;
		myleave(108);		
	}
	m_cbBody += cch;

	ret = S_OK;	
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CResponse::BinaryWrite failed, err = %d",err));
	if (pvarBuffer)
		SafeArrayUnaccessData(pvarBuffer);

	return ret;
}


// Performs Unicode to ASCII conversion before sending across the wire.
STDMETHODIMP CResponse::Write(VARIANT varData)
{
	return m_pASPState->Write(varData);
}


// Helper function to actually push bits across the wire and handle HTTP header policy, etc...
HRESULT CASPState::WriteToClient(PSTR pszBody, DWORD dwSpecifiedLength) {
	DWORD dwWriteLen = dwSpecifiedLength ? dwSpecifiedLength : strlen(pszBody);

	// If we're not buffering this and we haven't sent headers yet, send them now.
	if (m_fBufferedResponse == FALSE && m_fSentHeaders == FALSE) {
		if (FALSE == SendHeaders())
			return DISP_E_EXCEPTION;
	}

	if (! m_pACB->WriteClient(m_pACB->ConnID, (LPVOID) pszBody, &dwWriteLen, 0))  {
		m_aspErr = IDS_E_HTTPD;
		return DISP_E_EXCEPTION;
	}

	m_cbBody += dwWriteLen;
	return S_OK;
}


// Exposed to caller.
STDMETHODIMP CASPState::Write(VARIANT varData)
{
	DEBUG_CODE_INIT;
	HRESULT ret = DISP_E_EXCEPTION;
	VARTYPE vt;
	VARIANT *pvarKey = &varData;
    VARIANT varKeyCopy;
    PSTR pszVal = NULL;

	VariantInit(&varKeyCopy);
	vt = varData.vt;

    if ((vt != VT_BSTR) && (vt != VT_I2) && (vt != VT_I4))
	{
		if (FAILED(VariantResolveDispatch(&varKeyCopy, &varData)))
		{
			m_aspErr = IDS_E_PARSER;
			myleave(112);
		}
		pvarKey = &varKeyCopy;
	}
	vt = V_VT(pvarKey);

	if (vt != VT_BSTR)
	{
		// Coerce other types to VT_BSTR
		if (FAILED(VariantChangeTypeEx(pvarKey, pvarKey, m_lcid, 0, VT_BSTR)))
			myleave(113);
	}	         

	// The case where we have empty data, just get out.
	if (SysStringLen(pvarKey->bstrVal) == 0)
		myretleave(S_OK,0);


	// Codepage note:  We actually do use user set codepage in this instance,
	// because it's in the body of the request and not the headers.  Like IIS.
	pszVal = MySzDupWtoA(pvarKey->bstrVal, -1, m_lCodePage);
	if (NULL == pszVal)
	{
		m_aspErr = IDS_E_NOMEM;
		myleave(115);
	}

	ret = WriteToClient(pszVal);
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: Response::Write failed, err = %d\r\n",err));

	MyFree(pszVal);
    VariantClear(&varKeyCopy);

	return ret;
}

// IStream
HRESULT STDMETHODCALLTYPE CResponse::Write(const void __RPC_FAR *pv,ULONG cb,ULONG __RPC_FAR *pcbWritten) {
	if (!pv || !cb || !pcbWritten)
		return STG_E_INVALIDPOINTER;

	HRESULT hr = m_pASPState->WriteToClient((PSTR)pv,cb);
	if (hr == S_OK) {
		*pcbWritten = cb;
		return S_OK;
	}

	*pcbWritten = 0;
	return STG_E_CANTSAVE;
}

HRESULT STDMETHODCALLTYPE CResponse::Read(void __RPC_FAR *pv, ULONG cb, ULONG __RPC_FAR *pcbRead) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CResponse::Seek(LARGE_INTEGER dlibMove,DWORD dwOrigin,ULARGE_INTEGER __RPC_FAR *plibNewPosition) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CResponse::SetSize(ULARGE_INTEGER libNewSize) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CResponse::CopyTo(IStream __RPC_FAR *pstm,ULARGE_INTEGER cb,ULARGE_INTEGER __RPC_FAR *pcbRead,ULARGE_INTEGER __RPC_FAR *pcbWritten) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CResponse::Commit(DWORD grfCommitFlags) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CResponse::Revert(void) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CResponse::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,DWORD dwLockType) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CResponse::UnlockRegion(ULARGE_INTEGER libOffset,ULARGE_INTEGER cb,DWORD dwLockType) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CResponse::Stat(STATSTG __RPC_FAR *pstatstg, DWORD grfStatFlag) {
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CResponse::Clone(IStream __RPC_FAR *__RPC_FAR *ppstm) {
	return E_NOTIMPL;
}


