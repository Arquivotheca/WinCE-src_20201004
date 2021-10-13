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
Abstract: ISAPI Filter handling classes
--*/

#include "httpd.h"

CISAPIFilterCon* g_pFilterCon;

const DWORD g_fIsapiFilterModule = TRUE;


const LPCWSTR cwszFilterSep = L",";   // what seperates filter ids in the registry

//  Used to increment the filter, goes up 1 normally, down 1 for RAW DATA prio inversion
#define NEXTFILTER(dwNotifyType, i)     { (dwNotifyType == SF_NOTIFY_SEND_RAW_DATA) ? i-- : i++;}

void FreeLogParams(PHTTP_FILTER_LOG pLog);

//   Creates new filter info used globally
BOOL InitFilters() {
	g_pFilterCon = new CISAPIFilterCon();
	return (g_pFilterCon && g_pFilterCon->m_nFilters!=0);
}

//  Destroys the global filter information
void CleanupFilters() {	
	if (g_pFilterCon)
		delete g_pFilterCon;
	g_pFilterCon = 0;
}

CFilterInfo* CreateCFilterInfo(void) {
	if (0 == g_pFilterCon->m_nFilters)
		return NULL;

	return new CFilterInfo;
}

//  CFilterInfo is an internal member of CHttpRequest.  Allocate buffers for it
CFilterInfo::CFilterInfo() {
	ZEROMEM(this);

	m_pdwEnable = MyRgAllocNZ(DWORD,g_pFilterCon->m_nFilters);
	if (!m_pdwEnable) {
		DEBUGMSG(ZONE_ERROR, (L"HTTPD: CFilterInfo::Init died on Alloc!\r\n"));
		return;
	}

	m_ppvContext = MyRgAllocZ(PVOID,g_pFilterCon->m_nFilters);
	if (!m_ppvContext) {
		MyFree(m_pdwEnable);
		DEBUGMSG(ZONE_ERROR, (L"HTTPD: CFilterInfo::Init died on Alloc!\r\n"));
		return;
	}

	//  A filter can disable itself for events in a session, m_pdwEnable stores this.
	memset(m_pdwEnable,0xFFFF,g_pFilterCon->m_nFilters*sizeof(DWORD));  // all true at first

	m_dwStartTick = GetTickCount();
	m_fFAccept = TRUE;
	return;
}

//  Notes:  If http request is to be persisted, this is called. 
//  Do NOT Free the Allocated Mem or the context structure here, these are persisted
//  across requests, delete at end of session.


BOOL CFilterInfo::ReInit() {
	MyFree(m_pszDenyHeader);

	if (m_pFLog) {
		MyFree(m_pFLog);  
	}
	
	//  Reset the enable structure to all 1's, starting over.
	memset(m_pdwEnable,0xFFFF,g_pFilterCon->m_nFilters*sizeof(DWORD));  // all true at first

	// The context struct and AllocMem data lasts through the session, not just a request.
	m_dwStartTick = GetTickCount();
	m_fFAccept = TRUE;
	m_dwNextReadSize = 0;
	m_dwBytesSent = 0;
	m_dwBytesReceived = 0;

	return TRUE;
}

CFilterInfo::~CFilterInfo() {
	MyFree(m_pszDenyHeader);

	if (m_pFLog) {
		MyFree(m_pFLog);
	}
	MyFree(m_pdwEnable);	
	MyFree(m_ppvContext);

	
	FreeAllocMem();	// frees m_pAllocBlock
}

//  Initilization routine for global filters.  This looks in the registry for the
//  filters, loads any dlls into memory, and puts them into a list that is 
//  ordered from Highest prio filters to lowest.

BOOL CISAPIFilterCon::Init() {
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;
	BOOL fChange = FALSE;
	CISAPI *pCISAPI = NULL;
	const WCHAR *wcszReg;
	WCHAR *wszFilterNames;
	DWORD dwFilterLen = 0;
	PWSTR wszToken = NULL;
	DWORD i;
	int j;

	CReg topreg(HKEY_LOCAL_MACHINE,RK_HTTPD);

	wcszReg = topreg.ValueSZ(RV_FILTER);
	wszFilterNames = MySzDupW(wcszReg);

	DEBUGMSG(ZONE_ISAPI, (L"HTTPD: Filter DLLs Reg Value <<%s>>\r\n",wszFilterNames));

	if (NULL == wszFilterNames || 0 == (dwFilterLen = wcslen(wszFilterNames))) {
		DEBUGMSG(ZONE_ISAPI,(L"HTTPD: No filters listed in registry\r\n"));
		myleave(0);  // no values => no registered filters
	}

	// count # of commas to figure how many dlls there are
	m_nFilters = 1;   // there's at least 1 if we're this far

	for (i = 0; i < dwFilterLen && wszFilterNames[i] != L'\0'; i++) {
		if (wszFilterNames[i] == L',')
			m_nFilters++;
	}
	DEBUGMSG(ZONE_ISAPI, (L"HTTPD: # of filters = %d\r\n",m_nFilters));

	if (! (m_pFilters = MyRgAllocZ(FILTERINFO,m_nFilters)))
		myleave(203);

	// now tokenize the string and load filter libs as we do it
	j = 0;
	wszToken = wcstok(wszFilterNames,cwszFilterSep);
	while (wszToken != NULL) {
		svsutil_SkipWWhiteSpace(wszToken);

		// Handles the case where there's a comma but nothing after it.
		if (*wszToken == L'\0') {
			m_nFilters--;
			break;
		}
		
		pCISAPI = m_pFilters[j].pCISAPI = new CISAPI(SCRIPT_TYPE_FILTER);
		DEBUGMSG(ZONE_ISAPI, (L"HTTPD: Initiating filter library %s\r\n",wszToken));
		if (!pCISAPI) {
			m_nFilters = j;
			if (m_nFilters == 0)
				MyFree(m_pFilters);
			
			myleave(204);
		}

		if (TRUE == pCISAPI->Load(wszToken)) {
			m_pFilters[j].wszDLLName = MySzDupW(wszToken);	
			m_pFilters[j].dwFlags = pCISAPI->GetFilterFlags();
			if (m_pFilters[j].dwFlags & SF_NOTIFY_READ_RAW_DATA)
				g_pVars->m_fReadFilters = TRUE;
			j++;
		}
		else {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Filter <<%s>> failed to load!\r\n",wszToken));
			// only messed up this filter, others may work so keep on moving
			m_nFilters--;
			delete pCISAPI;
		}
			
		wszToken = wcstok(NULL,cwszFilterSep);
	}
	DEBUGCHK(j == m_nFilters);

	// Case where every filter fails to load
	if (0 == m_nFilters)  {
	//	Cleanup();
		MyFree(m_pFilters);
		myleave(205);
	}

	// Reorder list based on priorities of filters.
	do 	{
		fChange = FALSE;
		for(j=0; j<m_nFilters-1; j++) {
			if ( (m_pFilters[j].dwFlags & SF_NOTIFY_ORDER_MASK) < (m_pFilters[j+1].dwFlags & SF_NOTIFY_ORDER_MASK)) {
				// swap the 2 filter infos
				FILTERINFO ftemp = m_pFilters[j+1];
				m_pFilters[j+1] = m_pFilters[j];
				m_pFilters[j] = ftemp;
				fChange = TRUE;
			}
		}
	} while(fChange);

	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR, (L"HTTPD: CIsapiFilterCon::Init FAILED: GLE=%d err=%d\r\n", 
			GetLastError(), err));
	
	MyFree(wszFilterNames);
	return ret;
}

void CISAPIFilterCon::Cleanup() {
	if (0 == m_pFilters)
		return;
	
	for (int i = 0; i < m_nFilters; i++) {
		m_pFilters[i].pCISAPI->Unload(m_pFilters[i].wszDLLName);
		MyFree(m_pFilters[i].wszDLLName);

		delete m_pFilters[i].pCISAPI;
	}
	MyFree(m_pFilters);
}

// Determines whether or not a filter will be called based on current set of flags
BOOL CHttpRequest::IsCurrentFilterCalled(DWORD dwNotifyType, int iCurrentFilter) {
	// Filter has not requested to be notified for current call or has disabled itself through SF_REQ_DISABLE_NOTIFICATIONS.
	if (0 == ((dwNotifyType & g_pFilterCon->m_pFilters[iCurrentFilter].dwFlags) & m_pFInfo->m_pdwEnable[iCurrentFilter]))
		return FALSE;

	// Filter only wants secure notifications but we're receiving on an insecure port, or vice versa.
	if ((m_fIsSecurePort && ! (g_pFilterCon->m_pFilters[iCurrentFilter].dwFlags & SF_NOTIFY_SECURE_PORT)) || 
	    (!m_fIsSecurePort && ! (g_pFilterCon->m_pFilters[iCurrentFilter].dwFlags & SF_NOTIFY_NONSECURE_PORT)))
		return FALSE;

	return TRUE;
}

// BOOL CHttpRequest::CallFilter

// Function:   Httpd calls this function on specified events, it's goes through
// the list and calls the registered filters for that event.

// PARAMETERS:
// 		DWORD dwNotifyType - what SF_NOTIFIY type occured

//		The last 3 parameters are optional; only 3 filter calls use them.
// 		They are used when extra data needs to be passed to the filter that isn't
// 		part of the CHttpRequest structure.  

//		PSTR *ppszBuf1  ---> SF_NOTIFY_SEND_RAW_DATA   --> The buffer about to be sent
//						---> SF_NOTIFY_URL_MAP 	       --> The virtual path (only
//							    on ServerSupportFunction with HSE_REQ_MAP_URL_TO_PATH,
//							    otherwise use CHttpRequest values.)
//					 	---> SF_NOTIFY_AUTHENTECATION  --> The remote user name
//						---> SF_NOTIFY_READ_RAW_DATA   --> Buffer about to be read

//		PSTR *ppszBuf2  ---> SF_NOTIFY_SEND_RAW_DATA   --> Unused
//						---> SF_NOTIFY_URL_MAP		   --> The physical mapping
//						---> SF_NOTIFY_AUTHENTECATION  --> The user's password
//						---> SF_NOTIFY_READ_RAW_DATA   --> Buffer about to be read
//
//		int *pcbBuf		---> SF_NOTIFY_SEND_RAW_DATA   --> Length of buffer to be sent
//						---> SF_NOTIFY_URL_MAP		   --> Length of physical path buf
//						---> SF_NOTIFY_AUTHENTECATION  --> Unused
//						---> SF_NOTIFY_READ_RAW_DATA   --> Buffer about to be read

//      int *pcbBuf2    ---> SF_NOTIFY_READ_RAW_DATA   --> size of the buffer reading into

// return values
// 		TRUE  tells calling fcn to continue normal execution
// 		FALSE tells calling fcn to terminate this request.

// Notes:
// if FALSE is returned, m_pFilterInfo->m_fAccept is also set to false.  This
// helps the filter handle unwinding from nested filter calls.

// For example, the http server calls a filter with URL_MAP flags.  The filter
// then calls a WriteClient, which will call the filter again with event
// SF_NOTIFY_SEND_RAW_DATA.  If on the SEND_RAW_DATA call the filter returns a 
// code to stop the session, we need to store it for the original MAP_URL call,
// so that it knows to stop the session too.


BOOL CHttpRequest::CallFilter(DWORD dwNotifyType, PSTR *ppszBuf1, 
							  int *pcbBuf, PSTR *ppszBuf2, int *pcbBuf2)
{
	DEBUG_CODE_INIT;
	HTTP_FILTER_CONTEXT fc;
	LPVOID pStFilter;		// 3rd param passed on HttpFilterProc
	LPVOID pStFilterOrg; 	// stores a copy of pStFilter, so we remember alloc'd mem to cleanup, if any
	BOOL fFillStructs = TRUE;
	BOOL fReqReadNext;
	BOOL ret = FALSE;
	DWORD dwFilterCode;
	int i;

	if (0 == g_pFilterCon->m_nFilters)
		return TRUE;

	// m_pFInfo->m_fFAccept = FALSE implies no more filter calls, except on the "cleanup" calls
	DEBUGMSG(ZONE_ISAPI,(L"HTTPD: Filter notify type = 0x%08x\r\n",dwNotifyType));
	if ( (! m_pFInfo->m_fFAccept) &&
		  ! (dwNotifyType & (SF_NOTIFY_END_OF_REQUEST | SF_NOTIFY_LOG | SF_NOTIFY_END_OF_NET_SESSION | SF_NOTIFY_SEND_RESPONSE | SF_NOTIFY_SEND_RAW_DATA)))
	{
		return FALSE;
	}
	m_pFInfo->m_dwSFEvent = dwNotifyType;

	do {
		pStFilter = pStFilterOrg = NULL;
		fReqReadNext = FALSE;
		
		// Filters implement priority inversion for SEND_RAW_DATA events.
		i = (dwNotifyType == SF_NOTIFY_SEND_RAW_DATA) ? g_pFilterCon->m_nFilters - 1 : 0;
		
		while ( (dwNotifyType == SF_NOTIFY_SEND_RAW_DATA) ? i >= 0 : i < g_pFilterCon->m_nFilters ) {
			// we still reference g_pFilterCon just in case a filter failed in another request
			if (! IsCurrentFilterCalled(dwNotifyType,i)) {			
				// ith filter didn't request this notification, move along
				NEXTFILTER(dwNotifyType, i);
				continue;
			}

			m_pFInfo->m_iFIndex = i;
			if (fFillStructs && (!FillFC(&fc,dwNotifyType,&pStFilter,&pStFilterOrg,
										 ppszBuf1, pcbBuf, ppszBuf2, pcbBuf2)))
				myretleave(FALSE,210);  // memory error on FillFC
			fFillStructs = FALSE;	// after structs or filled don't refill them

			fc.pFilterContext = m_pFInfo->m_ppvContext[i];

			__try  {
				dwFilterCode = 	g_pFilterCon->m_pFilters[i].pCISAPI->CallFilter(&fc,dwNotifyType,pStFilter);
			}
			__except(1) {
				DEBUGMSG(ZONE_ERROR, (L"HTTPD: ISAPI Filter DLL <<%s>> caused exception 0x%08x\r\n", 
									        g_pFilterCon->m_pFilters[i].wszDLLName, GetExceptionCode()));

				g_pVars->m_pLog->WriteEvent(IDS_HTTPD_FILT_EXCEPTION,g_pFilterCon->m_pFilters[i].wszDLLName,GetExceptionCode(),L"HttpFilterProc",GetLastError());
				m_fKeepAlive = FALSE; 
				myleave(216);
			}

			// bail out if another filter call said to, but not on certain events (for compat with IIS)
			// This would happen if a filter triggered an event to occur which caused another filter call
			// to be made, and if that nested filter returned an error code indicating the end of session.
			if ( (! m_pFInfo->m_fFAccept) &&
			  ! (dwNotifyType & (SF_NOTIFY_END_OF_REQUEST | SF_NOTIFY_LOG | SF_NOTIFY_END_OF_NET_SESSION | SF_NOTIFY_SEND_RESPONSE | SF_NOTIFY_SEND_RAW_DATA)))
			{
				DEBUGMSG(ZONE_ISAPI,(L"HTTPD: Filter indirectly set us to end, ending CallFilter for event= %x,i=%d\r\n",dwNotifyType,i));
				myretleave(FALSE,0);
			}
			
			m_pFInfo->m_ppvContext[i] = fc.pFilterContext;

			DEBUGMSG(ZONE_ISAPI, (L"HTTPD: Filter returned response code of 0x%08x\r\n",dwFilterCode));
			switch (dwFilterCode) {
				// alert calling class that this request + net session is over
				case SF_STATUS_REQ_FINISHED:
					m_fKeepAlive = FALSE;
					m_pFInfo->m_fSentHeaders = TRUE;  // don't send back headers in these cases.
					myretleave(FALSE,0);   
				break;

				// alert calling class that this request but not the net session is over
				case SF_STATUS_REQ_FINISHED_KEEP_CONN:
					m_fKeepAlive = TRUE;
					m_pFInfo->m_fSentHeaders = TRUE;  // don't send back headers in these cases.
					myretleave(FALSE,0);   
				break;

				// goes to top of loop, handle next filter in line
				case SF_STATUS_REQ_NEXT_NOTIFICATION:
					NEXTFILTER(dwNotifyType, i);
				break;

				// not an error, just done handling current httpd event
				case SF_STATUS_REQ_HANDLED_NOTIFICATION:
					myretleave(TRUE,0);   		
				break;

				// alert calling class that this request is over
				case SF_STATUS_REQ_ERROR:
					m_fKeepAlive = FALSE;  
					m_rs = STATUS_INTERNALERR;
					myretleave(FALSE,214);
				break;

				case SF_STATUS_REQ_READ_NEXT:
					// only valid for read raw data events.  IIS ignores if returned by other filters
					// Continue running through remaining filters before reading more data
					if (dwNotifyType == SF_NOTIFY_READ_RAW_DATA) {
						fReqReadNext = TRUE;					
					}
					NEXTFILTER(dwNotifyType, i);
				break;
				
				default:	// treat like SF_STATUS_REQ_NEXT_NOTIFICATION
					DEBUGMSG(ZONE_ERROR, (L"HTTPD: Filter returned unknown/unhandled return code\r\n"));
					NEXTFILTER(dwNotifyType, i);
				break;
			}
		}

		// Only after all filters have been serviced to we handle READ_NEXT event
		// Note that having a filter call SF_READ_REQ_NEXT during a call to an 
		// ISAPI ext ReadClient is completly unsupported.  This has been docced.
		if (fReqReadNext) {
			PHTTP_FILTER_RAW_DATA pRaw = (PHTTP_FILTER_RAW_DATA) pStFilter;
			fFillStructs = TRUE; // set here in case we don't read any data and 
			                     // exit loop, so we don't call CleanupFC 2 times.

			DEBUGCHK(dwNotifyType == SF_NOTIFY_READ_RAW_DATA);

			CleanupFC(dwNotifyType, &pStFilter,&pStFilterOrg,ppszBuf1, pcbBuf, ppszBuf2);
			m_bufRequest.RecvBody(m_socket,m_pFInfo->m_dwNextReadSize ? m_pFInfo->m_dwNextReadSize : g_pVars->m_dwPostReadSize,TRUE, FALSE, this, FALSE);

			// only stop reading if we didn't get any data.  Timeouts are OK, we'll call read filter again
			if (m_bufRequest.UnaccessedCount() == 0)
				fReqReadNext = FALSE;  
			else
				m_pFInfo->m_dwBytesReceived += pRaw->cbInData; // from HTTP recv
		}
	} while (fReqReadNext);

	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR, (L"HTTPD: CIsapiFilterCon::CallFilter FAILED: GLE=%d err=%d\r\n", 
			GetLastError(), err));


	// Udpate our bytes sent/received if need be.  
	// Send bytes update must after we do our cleanup, Read bytes must happen before.
	if (dwNotifyType == SF_NOTIFY_READ_RAW_DATA) {
		// We only allocate pRaw if we need to (ie someone notified for read),
		// if this is so use that value for rx bytes.  If no one notefied for read filt, use
		// the value we were passed for bytes read by calling fcn.
	
		PHTTP_FILTER_RAW_DATA pRaw = (PHTTP_FILTER_RAW_DATA) pStFilter;
		if (pRaw)
			m_pFInfo->m_dwBytesReceived += pRaw->cbInData;
		else if (ppszBuf1)
			m_pFInfo->m_dwBytesReceived += *pcbBuf;  // from ISAPI ReadClient
		else
			m_pFInfo->m_dwBytesReceived += m_bufRequest.UnaccessedCount(); // from HTTP recv
	}			

	//  fFillStructs will be false if no filter registered for this event, which means
	//  we have no cleanup
	if (FALSE == fFillStructs)
		ret = CleanupFC(dwNotifyType, &pStFilter,&pStFilterOrg,ppszBuf1, pcbBuf, ppszBuf2);	

	if (dwNotifyType == SF_NOTIFY_SEND_RAW_DATA)
		m_pFInfo->m_dwBytesSent += *pcbBuf;

	if (!ret) {
		m_pFInfo->m_fFAccept = FALSE;
	}
	return ret;	
}

// sets up the FilterContext data structure so it makes sense to filterr being called.
// Only called once per filter event.

// ppStFilter is sent to filter dlls.
// ppStFilterOrg is a copy, it stores what we've allocated and is used to free up mem, as
// the filter would cause server mem leaks without it as a reference

// Last three parameters are the same as CallFilter

BOOL CHttpRequest::FillFC(PHTTP_FILTER_CONTEXT pfc, DWORD dwNotifyType, 
						  LPVOID *ppStFilter, LPVOID *ppStFilterOrg,
						  PSTR *ppszBuf1, int *pcbBuf, PSTR *ppszBuf2, int *pcbBuf2)
{
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;

	DEBUGCHK((*ppStFilterOrg) == NULL);
	DEBUGCHK((*ppStFilter)    == NULL);
	
	switch (dwNotifyType) {
		case SF_NOTIFY_END_OF_NET_SESSION:
		case SF_NOTIFY_END_OF_REQUEST:
			break;

		case SF_NOTIFY_READ_RAW_DATA:
		{
			PHTTP_FILTER_RAW_DATA pRawData = NULL;
			*ppStFilter = pRawData = MyAllocNZ(HTTP_FILTER_RAW_DATA);
			if (!pRawData)
				myleave(220);

			// We use UnaccessedCount() rather than Count() member of Buffio class
			// because we want the entire buffer for the Filter to read.  For instance,
			// if we had http headers and POST data, Count would only return the size
			// of the headers, while UnaccessedCount returns the size of everything.
			// We give filter the whole buffer.  For IIS compat.

			if (!ppszBuf1)  { // Use buffer in CHttpResponse class
				pRawData->cbInData =   m_bufRequest.UnaccessedCount();
				pRawData->cbInBuffer = m_bufRequest.AvailableBufferSize();
				pRawData->pvInData =   m_bufRequest.FilterRawData();  
			}	
			else  { //  Called from ISAPI ReadClient()
				pRawData->cbInBuffer  = *pcbBuf2;
				pRawData->cbInData = *pcbBuf;
				pRawData->pvInData = (PVOID) *ppszBuf1;
			}

			pRawData->dwReserved = 0;
			*ppStFilterOrg = MyAllocNZ(HTTP_FILTER_RAW_DATA);

			if (!(*ppStFilterOrg)) {
				MyFree(pRawData);
				*ppStFilter = NULL;
				myleave(240);
			}
			memcpy(*ppStFilterOrg,*ppStFilter,sizeof(HTTP_FILTER_RAW_DATA));		
		}
		break;

		case SF_NOTIFY_PREPROC_HEADERS:
		{
			PHTTP_FILTER_PREPROC_HEADERS pPreProc = NULL;
			*ppStFilter = pPreProc = MyAllocNZ(HTTP_FILTER_PREPROC_HEADERS);
			*ppStFilterOrg = NULL;

			if (!pPreProc)
				myleave(221);
			pPreProc->GetHeader = ::GetHeader;
			pPreProc->SetHeader = ::SetHeader;
			pPreProc->AddHeader = ::SetHeader;
			pPreProc->HttpStatus = 0;		// no response status code this return 
			pPreProc->dwReserved = 0;	
		}
		break;

		case SF_NOTIFY_URL_MAP:
		{
			PHTTP_FILTER_URL_MAP pUrlMap = NULL;
			*ppStFilter = pUrlMap = MyAllocNZ(HTTP_FILTER_URL_MAP);

			if (!pUrlMap)
				myleave(222);

			if (NULL == ppszBuf1) {  // usual case, use data in CHttpRequest
				pUrlMap->pszURL = m_pszURL;
				pUrlMap->pszPhysicalPath = MySzDupWtoA(m_wszPath);

				if (pUrlMap->pszPhysicalPath)
					pUrlMap->cbPathBuff = MyStrlenA(pUrlMap->pszPhysicalPath);
			}
			else  { // called from ISAPI ext with HSE_REQ_MAP_URL_TO_PATH
				pUrlMap->pszURL = (PSTR) *ppszBuf1;
				pUrlMap->pszPhysicalPath = (PSTR) *ppszBuf2;
				pUrlMap->cbPathBuff = *pcbBuf;
			}
			*ppStFilterOrg = MyAllocNZ(HTTP_FILTER_URL_MAP);

			if ( !pUrlMap->pszURL || ! pUrlMap->pszPhysicalPath || ! (*ppStFilterOrg))  {
				if (NULL == ppszBuf1) // only in this case do we have to clean this up.
					MyFree(pUrlMap->pszPhysicalPath);

				MyFree((*ppStFilterOrg));
				*ppStFilterOrg = NULL;

				MyFree(pUrlMap);
				*ppStFilter = NULL;
				myleave(320);
			}

			memcpy(*ppStFilterOrg,*ppStFilter,sizeof(HTTP_FILTER_URL_MAP));
		}
		break;

		case SF_NOTIFY_AUTHENTICATION:
		{
			DEBUGCHK(NULL != ppszBuf1 && NULL != ppszBuf2);
			
			PHTTP_FILTER_AUTHENT pAuth = NULL;
			*ppStFilter = pAuth = MyAllocNZ(HTTP_FILTER_AUTHENT);

			if (!pAuth)
				myleave(223);

			// pszUser + pszPassword are static buffers made in AuthenticateFilter fcn
			pAuth->pszUser = (PSTR) *ppszBuf1; 
			pAuth->cbUserBuff = SF_MAX_USERNAME;
			pAuth->pszPassword = (PSTR) *ppszBuf2;		 
			pAuth->cbPasswordBuff = SF_MAX_PASSWORD;
		}
		break;

		case SF_NOTIFY_ACCESS_DENIED:
		{
			PHTTP_FILTER_ACCESS_DENIED pDenied = NULL;
			*ppStFilter = pDenied = MyAllocNZ(HTTP_FILTER_ACCESS_DENIED);

			if (!pDenied)
				myleave(224);

			pDenied->pszURL = m_pszURL;
			pDenied->pszPhysicalPath = MySzDupWtoA(m_wszPath);	
			pDenied->dwReason = SF_DENIED_LOGON;	
			*ppStFilterOrg = MyAllocNZ(HTTP_FILTER_ACCESS_DENIED);

			if (! pDenied->pszURL || ! pDenied->pszPhysicalPath || !(*ppStFilterOrg)) {
				MyFree(pDenied->pszPhysicalPath);
				MyFree((*ppStFilterOrg));
				MyFree(pDenied);
				*ppStFilter = NULL;
				myleave(322);
			}
			memcpy(*ppStFilterOrg,*ppStFilter,sizeof(HTTP_FILTER_ACCESS_DENIED));
		}
		break;

		case SF_NOTIFY_SEND_RESPONSE:
		{
			PHTTP_FILTER_SEND_RESPONSE pSendRes = NULL;
			*ppStFilter = pSendRes = MyAllocNZ(HTTP_FILTER_PREPROC_HEADERS);
			
			if (!pSendRes)
				myleave(225);

			pSendRes->GetHeader  = ::GetHeader;
			pSendRes->SetHeader  = ::SetHeader;
			pSendRes->AddHeader  = ::SetHeader;
			pSendRes->HttpStatus = rgStatus[m_rs].dwStatusNumber;	
			pSendRes->dwReserved = 0;
		}
		break;

		case SF_NOTIFY_SEND_RAW_DATA:
		{
			PHTTP_FILTER_RAW_DATA pRawData = NULL;
			*ppStFilter = pRawData = MyAllocNZ(HTTP_FILTER_RAW_DATA);
			*ppStFilterOrg = MyAllocNZ(HTTP_FILTER_RAW_DATA);

			if (!pRawData || !(*ppStFilterOrg)) {
				MyFree(pRawData);
				MyFree((*ppStFilterOrg));
				*ppStFilter = *ppStFilterOrg = NULL;
				myleave(220);
			}

			pRawData->pvInData = *ppszBuf1;  
			pRawData->cbInData = *pcbBuf;
			pRawData->cbInBuffer = *pcbBuf;
			pRawData->dwReserved = 0;

			memcpy(*ppStFilterOrg,*ppStFilter,sizeof(HTTP_FILTER_RAW_DATA));
		}
		break;

		case SF_NOTIFY_LOG:
		{
			PHTTP_FILTER_LOG pLog = NULL;
			CHAR szHostBuf[MAX_PATH];
			
			*ppStFilter = pLog = MyAllocZ(HTTP_FILTER_LOG);
			if (!pLog)
				myleave(226);

			if (NULL == (pLog->pszClientHostName = (CHAR*)svsutil_AllocZ(LOG_REMOTE_ADDR_SIZE*sizeof(CHAR))))
				goto cleanupFCLog;

			GetRemoteAddress(m_socket,(PSTR) pLog->pszClientHostName,FALSE,LOG_REMOTE_ADDR_SIZE);

			if ( 0 == gethostname(szHostBuf, sizeof(szHostBuf))) {
				if (NULL == (pLog->pszServerName = MySzDupA(szHostBuf)))
					goto cleanupFCLog;
			}
			else
				pLog->pszServerName = cszEmpty;

			pLog->pszClientUserName = m_pszRemoteUser;
			pLog->pszOperation = m_pszMethod;  
			pLog->pszTarget = m_pszURL;
			pLog->pszParameters = m_pszQueryString;
			pLog->dwWin32Status = GetLastError();
			pLog->dwBytesSent = m_pFInfo->m_dwBytesSent;		
			pLog->dwBytesRecvd = m_pFInfo->m_dwBytesReceived;
			pLog->msTimeForProcessing = GetTickCount() - m_pFInfo->m_dwStartTick;

			pLog->dwHttpStatus = rgStatus[m_rs].dwStatusNumber;

			// don't pass NULL, give 'em empty string on certain cases
			if ( ! pLog->pszClientUserName)  
				pLog->pszClientUserName = cszEmpty;

			if ( ! pLog->pszParameters )
				pLog->pszParameters = cszEmpty;

			*ppStFilterOrg = MyAllocNZ(HTTP_FILTER_LOG);

cleanupFCLog:
			if (    !pLog->pszClientUserName  || !pLog->pszServerName  
			    ||  !pLog->pszOperation       || !pLog->pszTarget      
			    ||  !pLog->pszParameters      || !pLog->pszClientHostName  
			    ||   !(*ppStFilterOrg))
			{
				MyFree((*ppStFilterOrg));
				MyFree(pLog->pszClientHostName);

				if (pLog->pszServerName != cszEmpty) {
					MyFree(pLog->pszServerName);
				}

				MyFree(pLog);
				*ppStFilter = NULL;
				myleave(344);
			}

			memcpy(*ppStFilterOrg,*ppStFilter,sizeof(HTTP_FILTER_LOG));
		}
		break;

		default:
			DEBUGCHK(0);
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: FillFC received unknown notification type = %d\r\n",dwNotifyType));
			*ppStFilterOrg = *ppStFilter = NULL;
			myleave(246);
		break;
	}

	// the pfc is always the same regardless of dwNotifyType
	pfc->cbSize = sizeof(*pfc);
	pfc->Revision = HTTP_FILTER_REVISION;
	pfc->ServerContext = (PVOID) this;
	pfc->ulReserved = 0;
	pfc->fIsSecurePort = m_fIsSecurePort;
	//  pfc->pFilterContext is filled in calling loop in CallFilter()
	pfc->GetServerVariable = ::GetServerVariable;
	pfc->AddResponseHeaders = ::AddResponseHeaders;
	pfc->WriteClient = ::WriteClient;
	pfc->AllocMem = ::AllocMem;
	pfc->ServerSupportFunction = ::ServerSupportFunction;

	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"HTTPD: FillFC failed on mem alloc, GLE = %d, err= %d\r\n",GetLastError(),err));
	
	return ret;
}


// Final fcn called in CallFilter, this frees any unneeded allocated memory
// and sets the last three values, if valid, to whatever the filter changed them to.
BOOL CHttpRequest::CleanupFC(DWORD dwNotifyType, LPVOID* ppStFilter, LPVOID* ppStFilterOrg,
							PSTR *ppszBuf1, int *pcbBuf, PSTR *ppszBuf2)

{
	BOOL fRet = TRUE;

	switch (dwNotifyType)
	{
		case SF_NOTIFY_END_OF_NET_SESSION:
		case SF_NOTIFY_END_OF_REQUEST:
		case SF_NOTIFY_PREPROC_HEADERS:
		case SF_NOTIFY_SEND_RESPONSE:
			break;

		case SF_NOTIFY_READ_RAW_DATA:
		{
			PHTTP_FILTER_RAW_DATA pRawData = (PHTTP_FILTER_RAW_DATA) *ppStFilter;			

			if (! ppszBuf1) { // Use buffer in CHttpResponse class
				m_bufRequest.FilterDataUpdate(pRawData->pvInData,pRawData->cbInData,pRawData->pvInData != m_bufRequest.FilterRawData());
			}
			else {
				*ppszBuf1 = (PSTR) pRawData->pvInData;
				*pcbBuf =   pRawData->cbInData;
			}
		}
		break;

		case SF_NOTIFY_AUTHENTICATION: 
		{
			PHTTP_FILTER_AUTHENT pAuth = (PHTTP_FILTER_AUTHENT) *ppStFilter;

			*ppszBuf1 = pAuth->pszUser;
			*ppszBuf2 = pAuth->pszPassword;
		}
		break;
		
		case SF_NOTIFY_SEND_RAW_DATA:
		{
			PHTTP_FILTER_RAW_DATA pRawData = (PHTTP_FILTER_RAW_DATA) *ppStFilter;
			*ppszBuf1 = (PSTR) pRawData->pvInData;
			*pcbBuf  = pRawData->cbInData;
		}
		break;

		case SF_NOTIFY_URL_MAP:
		{
			PHTTP_FILTER_URL_MAP pUrlMap = (PHTTP_FILTER_URL_MAP) *ppStFilter ;
			PHTTP_FILTER_URL_MAP pUrlMapOrg = (PHTTP_FILTER_URL_MAP) *ppStFilterOrg;

			// Case when parsing http headers from a request.
			if (NULL == ppszBuf1) {
				// since data can be modified in place, always re-copy						
				MyFree(m_wszPath);
				m_wszPath = MySzDupAtoW(pUrlMap->pszPhysicalPath);

				MyFree(pUrlMapOrg->pszPhysicalPath);
				if (!m_wszPath)
					fRet = FALSE;

				// note: don't set m_ccWPath here because it will be set in ParseHeaders() anyway.
			}
			else { // ServerSupportFunction with HSE_MAP_URL call
				*ppszBuf1 = (PSTR) pUrlMap->pszURL;
				*ppszBuf2 = pUrlMap->pszPhysicalPath;
				*pcbBuf = pUrlMap->cbPathBuff;
			}
		}
		break;

		case SF_NOTIFY_ACCESS_DENIED:
		{
			PHTTP_FILTER_ACCESS_DENIED pDeniedOrg = (PHTTP_FILTER_ACCESS_DENIED) *ppStFilterOrg;
			// If they change pDenied->pszURL we ignore it.  Since the session
			// is coming to an end because of access problems, the only thing changing 
			// pszURL could affect would be logging (which should be set through logging filter)		
			MyFree(pDeniedOrg->pszPhysicalPath);
		}
		break;

		// We use m_pFInfo->m_pFLog to store changes to the log.  When
		// we write out the log, we use the web server value by default
		// unless a particular value is non-NULL; by default all values
		// are NULL in m_pFInfo->m_pFLog unless overwritten by this code.
		case SF_NOTIFY_LOG:
		{
			PHTTP_FILTER_LOG pLog = (PHTTP_FILTER_LOG) *ppStFilter;
			PHTTP_FILTER_LOG pLogOrg = (PHTTP_FILTER_LOG ) *ppStFilterOrg;
			PHTTP_FILTER_LOG pFLog;

			// Free dynamically allocated data.
			MyFreeNZ(pLogOrg->pszClientHostName);

			if (pLogOrg->pszServerName != cszEmpty)
				MyFreeNZ(pLogOrg->pszServerName);

			// If no changes were made, break out early.
			if ( ! memcmp(pLog, pLogOrg, sizeof(HTTP_FILTER_LOG))) {
				break;  				
			}

			if (NULL == (pFLog = m_pFInfo->m_pFLog = MyAllocZ(HTTP_FILTER_LOG))) {
				// Memory errors just mean we won't bother doing logging, non-fatal.
				break;  
			}
			
			if (pLog->pszClientHostName != pLogOrg->pszClientHostName)
				pFLog->pszClientHostName = pLog->pszClientHostName;

 			if (pLog->pszServerName  != pLogOrg->pszServerName)
				pFLog->pszServerName = pLog->pszServerName;

			if (pLog->pszClientUserName  != pLogOrg->pszClientUserName)
				pFLog->pszClientUserName = pLog->pszClientUserName;

			if (pLog->pszOperation  != pLogOrg->pszOperation)
				pFLog->pszOperation = pLog->pszOperation;

			if (pLog->pszTarget  != pLogOrg->pszTarget)
				pFLog->pszTarget = pLog->pszTarget;

			if (pLog->pszParameters  != pLogOrg->pszParameters)
				pFLog->pszParameters = pLog->pszParameters ;

			pFLog->dwHttpStatus 	   = pLog->dwHttpStatus;
			pFLog->dwWin32Status       = pLog->dwWin32Status;
			pFLog->dwBytesSent         = pLog->dwBytesSent;
			pFLog->dwBytesRecvd        = pLog->dwBytesRecvd ;
			pFLog->msTimeForProcessing = pLog->msTimeForProcessing;		
		}
		
		break;

		default:
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: CleanupFCs received unknown code = %d\r\n",dwNotifyType));
		break;

	}

	MyFree(*ppStFilterOrg);
	MyFree(*ppStFilter);
	return fRet;
}

//  Initializes buffers for CallFilter with SF_NOTIFY_AUTHENTICATION, and tries
//  to authenticate if the filter changes any data.

//  Note:  Even in the case where security isn't a component or is disabled in 
//         registry, we make this call because filter may theoretically decide to end
//		   the session based on the user information it receives.

BOOL CHttpRequest::AuthenticateFilter() {
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;

	// IIS docs promise the filter minimum sized buffers to write password
	// and user name into, we provide them
	char szUserName[SF_MAX_USERNAME];
	char szPassword[SF_MAX_PASSWORD];  

	PSTR pszNewUserName = szUserName;
	PSTR pszNewPassword = szPassword;

	// We only make notification if we're using Anonymous or BASIC auth,
	// otherwise we don't know how to decode the text/password.  Like IIS.
	if ( m_pszAuthType && 0 != _stricmp(m_pszAuthType,cszBasic))
		return TRUE;

	// write existing value, if any, into the new buffers

	//  In this case we've cracked the user name and password into ANSI already.
	if (m_pszRemoteUser) {
		// These are safe strcpy's because HandleBasicAuth cracked
		// out the strings and made sure their combined len was < MAX_PATH.
		DEBUGCHK(strlen(m_pszRemoteUser) < sizeof(szUserName));
		DEBUGCHK(strlen(m_pszPassword)   < sizeof(szPassword));
		
		strcpy(szUserName,m_pszRemoteUser);
		strcpy(szPassword,m_pszPassword);
	}
	//  We have user user name data but it hasn't been Base64 Decoded yet
	else if (m_pszRawRemoteUser) {
		DWORD dwLen = sizeof(szUserName);

		if (strlen(m_pszRawRemoteUser) >= MAXUSERPASS) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Base64 data > 256 bytes on Basic Auth, failing request\r\n"));
			m_rs = STATUS_FORBIDDEN;
			return FALSE;
		}
	
		Base64Decode(m_pszRawRemoteUser, szUserName, &dwLen);

		PSTR pszDivider =  strchr(szUserName, ':');
		if(NULL == pszDivider)
			myleave(290);
		*pszDivider++ = 0;		// seperate user & pass
	
		//  We need copies of this data for later in the fcn
		m_pszRemoteUser = MySzDupA(szUserName, strlen(szUserName));
		if (NULL == m_pszRemoteUser)
			myleave(291);
				
		m_pszPassword = MySzDupA(pszDivider);
		if (NULL == m_pszPassword)
			myleave(292);
		
		strcpy(szPassword, m_pszPassword);
		strcpy(szUserName, m_pszRemoteUser);
	}
	// Otherwise the browser didn't set any user information.
	else {
		szUserName[0] = '\0';
		szPassword[0] = '\0';
	}

	if ( ! CallFilter(SF_NOTIFY_AUTHENTICATION,&pszNewUserName, NULL, &pszNewPassword))
		myleave(293);


	//  If the filter arbitrarily denied access, then we exit. 
	//  Check if the filter has changed the user name ptr or modified it in place.
	
	if ( pszNewUserName != szUserName ||							// changed ptr
		 (m_pszRemoteUser == NULL  && szUserName[0] != '\0') ||		// modified in place
		 (m_pszRemoteUser && strcmp(szUserName, m_pszRemoteUser) )  // modified in place
	   )						
	{
		// Update m_pszRemoteUser with what was set in filter because GetServerVariable
		// may need it.
		ResetString(m_pszRemoteUser, pszNewUserName);
		if (! m_pszRemoteUser)
			myleave(294);
	}

	if ( pszNewPassword != szPassword ||
   	     (m_pszPassword == NULL  && szPassword[0] != '\0') ||
		 (m_pszPassword && strcmp(szPassword, m_pszPassword))
		 )
	{
		// GetServerVariables with AUTH_PASSWORD might request this.
		ResetString(m_pszPassword, pszNewPassword);
		if (! m_pszPassword)
			myleave(295);
	}


	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"HTTPD: Authentication for filters failed, err = %d, GLE = %d\r\n",err,GetLastError()));
	return ret;
}


//  Called on updating data from an ISAPI filter, resets internal structures.  
//  fModifiedPtr is TRUE when 

BOOL CBuffer::FilterDataUpdate(PVOID pvData, DWORD cbData, BOOL fModifiedPtr) {
	// a filter can do anything to 2nd HTTP request waiting in buffer, so
	// just mark it as invalid to play it safe.
	InvalidateNextRequestIfAlreadyRead();

	if (fModifiedPtr) {
		if ((int) (cbData - UnaccessedCount()) > 0) {
			if (! AllocMem(cbData - UnaccessedCount() + 1))
				return FALSE;

			memcpy(m_pszBuf + m_iNextInFollow, pvData, cbData);
		}
	}
	// It's possible we modified data in place, so we always update the size
	// information that the filter passed us in case it changed the # of bytes
	// in the request.

	m_iNextIn = (int)cbData + m_iNextInFollow;
	m_iNextInFollow = m_iNextIn;
	
	return TRUE;
}
