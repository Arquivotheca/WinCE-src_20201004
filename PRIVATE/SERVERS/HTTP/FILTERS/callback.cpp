//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: filter.cpp
Abstract: ISAPI Filter call back functions
--*/

// NOTE: CE doesn't support advanced header management on SF_NOTIFY_SEND_RESPONSE.
// IIS allows response headers to be deleted or set to different values after 
// they are set with a call to SetHeader or AddHeader.
// CE dumps all headers into a buffer, so this behavior is not allowed.

#include "httpd.h"


//  Allocates and returns a buffer size cbSize.  This will be deleted at the
//  end of the session.

//  The MSDN docs on this claim this returns a BOOL.  Looking in httpfilt.h
//  shows that the actual implementation is a VOID *.

VOID* WINAPI AllocMem(PHTTP_FILTER_CONTEXT pfc, DWORD cbSize, DWORD dwReserved) {
	CHECKPFC(pfc);
	//	NOTE:  alloc mem isn't directly related to connection, don't check m_pFINfo->fAccept like
	//	the other filters do

	return ((CHttpRequest*)pfc->ServerContext)->m_pFInfo->AllocMem(cbSize,dwReserved);
}

VOID* CFilterInfo::AllocMem(DWORD cbSize, DWORD dwReserved) {
	if (cbSize == 0)
		return NULL;
	
	if (NULL == m_pAllocMem) {
		m_pAllocMem = MyRgAllocZ(PVOID,VALUE_GROW_SIZE);
		if (! (m_pAllocMem))  {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: CFilterInfo::AllocMem failed on inital alloc, GLE=%d\r\n",GetLastError()));
			return NULL;
		}
	}
	else if (m_nAllocBlocks % VALUE_GROW_SIZE == 0) {
		m_pAllocMem = MyRgReAlloc(PVOID,m_pAllocMem,m_nAllocBlocks,m_nAllocBlocks + VALUE_GROW_SIZE);
		if ( !m_pAllocMem) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: CFilterInfo::AllocMem failed on re-allocing for block %d, GLE=%d\r\n",m_nAllocBlocks+1,GetLastError()));
			return NULL;
		}
	}

	if (! (m_pAllocMem[m_nAllocBlocks] = MyRgAllocNZ(PVOID,cbSize))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: CFilterInfo::AllocMem failed on allocating block %d, GLE=%d\r\n",m_nAllocBlocks+1,GetLastError()));
		return NULL;
	}

	m_nAllocBlocks++;
	return m_pAllocMem[m_nAllocBlocks-1];
}

 
void CFilterInfo::FreeAllocMem() {
	DWORD dwI;

	if (0 == m_nAllocBlocks || ! (m_pAllocMem))
		return;

	for (dwI = 0; dwI < m_nAllocBlocks; dwI++) {
		MyFree(m_pAllocMem[dwI]);
	}
	MyFree(m_pAllocMem);
}


//   Adds httpd headers.
//   In effect this is identical to a call to SetHeader or AddHeader except that
//   the user formats the whole string (name and value) before caalling
//   AddResponseHeaders, in Add/Set Header they come in 2 seperate fields.

BOOL WINAPI AddResponseHeaders(PHTTP_FILTER_CONTEXT pfc,LPSTR lpszHeaders,DWORD dwReserved) {
	CHECKPFC(pfc);
	CHECKPTR(lpszHeaders);
	CHECKFILTER(pfc);

	if (dwReserved != 0) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	
	return ((CHttpRequest*)pfc->ServerContext)->AddResponseHeaders(lpszHeaders,dwReserved);
}


BOOL CHttpRequest::AddResponseHeaders(LPSTR lpszHeaders,DWORD dwReserved) {
	// Can't set response headers on 0.9 version.  Err code is like IIS.
	if (m_dwVersion <= MAKELONG(9, 0)) 	{
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if ( m_pFInfo->m_dwSFEvent & (SF_NOTIFY_END_OF_NET_SESSION | SF_NOTIFY_END_OF_REQUEST |
								  SF_NOTIFY_LOG | SF_NOTIFY_SEND_RAW_DATA | 
								  SF_NOTIFY_SEND_RESPONSE))
		return TRUE;

	//  Note:  We don't use CBuffer::AddHeaders because it does formatting, we assume
	//  here filter alreadf formatted everything correctly.  
	
	return m_bufRespHeaders.AppendData(lpszHeaders, strlen(lpszHeaders));
}

BOOL WINAPI GetServerVariable(PHTTP_FILTER_CONTEXT pfc, PSTR psz, PVOID pv, PDWORD pdw) {
	CHECKPFC(pfc);
	CHECKPTRS3(psz, pv, pdw);

	// We don't check m_fFAccept here because this function is read only,
	// doesn't try and do anything across the network.  Like IIS.
	return ((CHttpRequest*)pfc->ServerContext)->GetServerVariable(psz, pv, pdw, TRUE);
}

BOOL WINAPI WriteClient(PHTTP_FILTER_CONTEXT pfc, PVOID pv, PDWORD pdw, DWORD dwFlags) {
	CHECKPFC(pfc);
	CHECKPTRS2(pv, pdw);
//	CHECKFILTER(pfc);    // we always accepts WriteClient, even if filter has returned an error at some point.

	if(dwFlags != 0) { 
		SetLastError(ERROR_INVALID_PARAMETER); 
		return FALSE; 
	}
	return ((CHttpRequest*)pfc->ServerContext)->WriteClient(pv,pdw,TRUE);
}

//  Misc Server features.
BOOL WINAPI ServerSupportFunction(PHTTP_FILTER_CONTEXT pfc, enum SF_REQ_TYPE sfReq,
									PVOID pData, DWORD ul1, DWORD ul2)
{
	CHECKPFC(pfc);
	CHECKFILTER(pfc);
	
	return ((CHttpRequest*)pfc->ServerContext)->ServerSupportFunction(sfReq, pData, ul1, ul2);
}



BOOL CHttpRequest::ServerSupportFunction(enum SF_REQ_TYPE sfReq,PVOID pData, DWORD ul1, DWORD ul2) {
	switch (sfReq) {
		case SF_REQ_ADD_HEADERS_ON_DENIAL:
		{
			return MyStrCatA(&m_pFInfo->m_pszDenyHeader,(PSTR) pData);
		}

		case SF_REQ_DISABLE_NOTIFICATIONS:
		{
			// u11 has data as to which flags to deactivate for this session,

			// For example, setting u11 = SF_NOTIFY_SEND_RAW_DATA | SF_NOTIFY_LOG
			// would disable notifications to the filter that called this fcn for 
			// those events on this request only (per request, not per session!).
			// These values are reset to 1's at the end of each request in CHttpRequest::ReInit
			
			m_pFInfo->m_pdwEnable[m_pFInfo->m_iFIndex] &=  (m_pFInfo->m_pdwEnable[m_pFInfo->m_iFIndex] ^ ul1);  
			return TRUE;
		}

		case SF_REQ_SEND_RESPONSE_HEADER:
		{
			// no Connection header...let ISAPI send one if it wants
			CHttpResponse resp(this, STATUS_OK, CONN_CLOSE);
			// no body, default or otherwise (leave that to the ISAPI), but add default headers
			resp.SendResponse((PSTR) ul1, (PSTR) pData, TRUE);


			// The reasons for doing this are documented in FilterNoResponse()
			// Note:  We don't check this sent headers here because IIS doesn't,
			// so it's possible to make multiple calls with this option even though it
			// doesn't make that much sense for a filter to do so.
			
			m_pFInfo->m_fSentHeaders = TRUE;
			m_fKeepAlive = FALSE;
			return TRUE;  		
		}

		case SF_REQ_SET_NEXT_READ_SIZE:	
		{
			m_pFInfo->m_dwNextReadSize = ul1;
			return TRUE;
		}

//		case SF_REQ_NORMALIZE_URL:		//
//		case SF_REQ_GET_CONNID:			// IIS doesn't support in versions 4+.
//		case SF_REQ_GET_PROPERTY:		// Relates to meta database, IIS only feature.
//		case SF_REQ_SET_PROXY_INFO:		// No proxy support in CE.
		default:
		{
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Unsupported or invald ServerSupportFcn request = 0x%08x\r\n",sfReq));
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}
	}
	
	return TRUE;
}



//***************************************************************************
// Extended filter header options
// 
// These fcns allow the filter to perform more advanced header fncs automatically,
// On PREPROC_HEADERS event, it's possible for the filter to change CHttpRequest
// state information through the SetHeaders and AddHeaders callback.  We support
// changing URL, Version, If-modified-since, and method through the preproc 
// headers calls.

// In a SEND_RESPONSE event, it's possible for the filter to modify the response
// headers through SetHEaders or AddHeaders.

// Unlike IIS, we don't allow the filter to delete a header once it's set, or
// to overwrite a header's information with new info.  We only allow the header
// data to be appended to.

//***************************************************************************


BOOL WINAPI GetHeader(PHTTP_FILTER_CONTEXT pfc, LPSTR lpszName, LPVOID lpvBuffer, LPDWORD lpdwSize) {
	CHECKPFC(pfc);
	CHECKPTRS3(lpszName, lpvBuffer, lpdwSize);
	CHECKFILTER(pfc);

	return ((CHttpRequest*)pfc->ServerContext)->GetHeader(lpszName, lpvBuffer, lpdwSize);
}

BOOL WINAPI SetHeader(PHTTP_FILTER_CONTEXT pfc, LPSTR lpszName, LPSTR lpszValue) {
	CHECKPFC(pfc);
	CHECKPTRS2(lpszName, lpszValue);
	CHECKFILTER(pfc);

	return ((CHttpRequest*)pfc->ServerContext)->SetHeader(lpszName, lpszValue);
}


// Retrieves raw header value for specified name.
BOOL CHttpRequest::GetHeader(LPSTR lpszName, LPVOID lpvBuffer, LPDWORD lpdwSize) {
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;
	DWORD cbSizeNeeded;
	char szBuf[MAXHEADERS];
	PSTR pszRet = (PSTR)-1;
	PSTR pszTrav = NULL;
	PSTR pszEndOfHeaders = NULL;
	DWORD cbName;

	// this is the only event we allow this for.  This is like IIS, but not docced in MSDN.
	if (m_pFInfo->m_dwSFEvent != SF_NOTIFY_PREPROC_HEADERS) {
		DEBUGMSG(ZONE_ISAPI,(L"HTTPD: GetHeader fails, must be on SF_NOTIFY_PREPROC_HEADERS event only\r\n"));
		SetLastError(ERROR_INVALID_INDEX);
		myleave(1405);
	}

	// For method, url, and version (from simple request line) we get the data
	// from CHttpRequest
	if (0==_stricmp(lpszName, "version")) {
		sprintf(szBuf, cszHTTPVER, HIWORD(m_dwVersion), LOWORD(m_dwVersion));
		pszRet = szBuf;
	}
	else if (0 == _stricmp(lpszName, "url"))
		pszRet = m_pszURL;
	else if (0 == _stricmp(lpszName, "method"))
		pszRet = m_pszMethod;
	else if (NULL != (pszTrav = (PSTR) m_bufRequest.Headers()))  {
		// if it's not one of the 3 special values, we search through the raw
		// buffer for the header name.
		pszTrav = strstr(pszTrav,cszCRLF);  // skip past simple http header
		pszEndOfHeaders = pszTrav + m_bufRequest.GetINextOut();

		cbName = strlen(lpszName);
		
		for (; pszTrav; pszTrav = strstr(pszTrav,cszCRLF)) {
			pszTrav += sizeof("\r\n") - 1;

			// reached end of headers, double CRLF
			if (*pszTrav == '\r')
				break;

			// Make sure we don't walk off the end of the buffer.
			if ((int) cbName > (pszEndOfHeaders - pszTrav))
				break;
				
			if (0 == _memicmp(pszTrav,lpszName,cbName)) {
				pszTrav += cbName;
				if (' ' == *pszTrav) 		{	// must be a space next for a match
					pszRet = pszTrav + 1;
					pszTrav = strstr(pszTrav,cszCRLF);
					DEBUGCHK(pszTrav != NULL);	// should catch improperly formatted headers in parser
					*pszTrav = 0;  // make this the end of string temporarily
					break;		
				}
			}
		}
	}

	if(((PSTR)(-1) == pszRet) || !pszRet) {
		DEBUGMSG(ZONE_ISAPI,(L"HTTPD: GetHeader filter failed, unknown header"));
		// unknown var
		SetLastError(ERROR_INVALID_INDEX);
		myleave(1400);
	}
		
	if((cbSizeNeeded = strlen(pszRet)+1) > *lpdwSize) {
		*lpdwSize = cbSizeNeeded;
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		DEBUGMSG(ZONE_ISAPI,(L"HTTPD: GetHeader fails, buffer not large enough to hold results"));
		myleave(1401);
	}
	memcpy(lpvBuffer, pszRet, cbSizeNeeded);
	ret = TRUE;

done:
	if (pszTrav)
		*pszTrav = '\r'; 	// reset the value
	return ret;
}

BOOL CHttpRequest::SetHeader(LPSTR lpszName, LPSTR lpszValue) {
	DEBUG_CODE_INIT;
	PSTR pszNew = NULL;
	BOOL ret = FALSE;

  	DEBUGMSG(ZONE_ISAPI,(L"HTTPD: Servicing SetHeader request with name<<%a>>,value<<%a>> \r\n",lpszName,lpszValue));
	if (0==_stricmp(lpszName, "version")) {
		int iMajor, iMinor;
		if (2 != sscanf(lpszValue, cszHTTPVER, &iMajor, &iMinor)) {
			SetLastError(ERROR_INVALID_PARAMETER);
			DEBUGMSG(ZONE_ISAPI,(L"HTTPD: SetHeader fails on invalid HTTP version string\r\n"));
			myleave(255);
		}
		m_dwVersion = MAKELONG(iMinor, iMajor);
	}
	else if (0 == _stricmp(lpszName, "url")) {
		pszNew = MySzDupA(lpszValue);
		if (!pszNew)
			myleave(256);

		MyFree(m_pszURL);  
		m_pszURL = pszNew;
		ret = TRUE;
	}
	else if (0 == _stricmp(lpszName, "method"))  {
		pszNew  = MySzDupA(lpszValue);
		if (!pszNew)
			myleave(257);

		MyFree(m_pszMethod);
		m_pszMethod = pszNew;
		ret = TRUE;
	}
	// Note: Unlike IIS, CE does not support modifying headers at PREPROC event.
	else if (m_pFInfo->m_dwSFEvent == SF_NOTIFY_PREPROC_HEADERS) {
		myretleave(TRUE,0);
	}
	else   {// custom response header, like "Content-length:" or whatever else
		// Can't set response headers on 0.9 version.  Err code is like IIS.
		if (m_dwVersion <= MAKELONG(9, 0)) {
			DEBUGMSG(ZONE_ISAPI,(L"HTTPD: SetHeader fails, cannot set headers on pre HTTP 1.0 requests\r\n"));
			SetLastError(ERROR_NOT_SUPPORTED);
			myleave(258);
		}
		
		ret = m_bufRespHeaders.AddHeader(lpszName,lpszValue);
	}

done:

	return ret;
}

//  This handles a special case for Filters / extensions.  IF a call is made
//  to ISAPI Extension ServerSupportFunction with HSE_REQ_MAP_URL_TO_PATH, then 
//  a filter call to SF_NOTIFY_URL_MAP is performed.  

//  We can't call the filter directly because SF_NOTIFY_URL_MAP usually gets
//  path info from the CHttpRequest class, but in this case it's getting it's
//  data from and writing out to a user buffer.

//  pszBuf is the original string passed into ServerSupportFunction by the ISAPI.  
//  	When the function begins it has a virtual path, on successful termination it has a physical path
//  pdwSize is it's size
//  wszPath is it's mapped virtual root path
//  dwBufNeeded is the size of the buffer required to the physical path.

BOOL CHttpRequest::FilterMapURL(PSTR pszBuf, WCHAR *wszPath, DWORD *pdwSize, DWORD dwBufNeeded, PSTR pszURLEx) {
	DEBUG_CODE_INIT;
	PSTR pszPhysicalOrg;
	PSTR pszPhysicalNew;
	PSTR pszVirtual = pszURLEx ? pszURLEx : pszBuf;
	DWORD cbBufNew = dwBufNeeded;
	BOOL ret = FALSE;

	// Regardless of if buffer was big enough to hold the original data, we always 
	// allocate a buf for the filter.  The filter may end up changing the data
	// so it's small enough to fit in the buffer.

	if (NULL == (pszPhysicalOrg = MyRgAllocNZ(CHAR,dwBufNeeded)))
		myleave(710);
	MyW2A(wszPath, pszPhysicalOrg, dwBufNeeded);
	pszPhysicalNew = pszPhysicalOrg;	// Keep a copy of pointer for freeing, CallFilter may modify it
	
	if ( !CallFilter(SF_NOTIFY_URL_MAP,&pszVirtual,(int*) &cbBufNew,&pszPhysicalNew))
		myleave(711);

	//  Buffer isn't big enough
	if (*pdwSize < cbBufNew) {
		DEBUGMSG(ZONE_ISAPI,(L"HTTPD: Mapping URL fails because buffer isn't large enough"));
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		myleave(712);
	}

	// Copy changes over.  pszPhysicalNew will be filter set or will be
	// the original wszPath converted to ASCII without filter.
	if (NULL != pszPhysicalNew) {
		memcpy(pszBuf,pszPhysicalNew,cbBufNew); 
	}
	
	ret = TRUE;
done:

	*pdwSize =  cbBufNew;
	MyFree(pszPhysicalOrg);
	return ret;
}
