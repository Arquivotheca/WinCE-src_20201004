//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: request.cpp
Abstract: per-Connection thread
--*/


#include "httpd.h"

#define AUTH_FILTER_DONE     0x1000		// no more filter calls to SF_AUTH after the 1st one in a session

BOOL IsLocalFile(PWSTR wszFile);

void CHttpRequest::HandleRequest()  {
	int err = 1;
	RESPONSESTATUS ret = STATUS_BADREQ;
	DWORD dwLength = 0;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	HRINPUT hi;
	BOOL fSendDirectoryList = FALSE;
	WCHAR *wszISAPIPath;
	BOOL  fASP;
	BOOL  fRenegotiateSSL = FALSE;
	BOOL  fDelayAuth      = FALSE;
	
	m_rs = STATUS_OK;

	DEBUGMSG(ZONE_REQUEST,(L"HTTPD: HandleRequest: entry.  Current Authorization Level = %d\r\n",m_AuthLevelGranted));

	if (m_fHandleSSL)  {
		if (! HandleSSLHandShake()) {
			m_fKeepAlive = FALSE;
			return;  // HandleSSLHandshake handles all error codes, logging, et al.
		}
	}
	hi = m_bufRequest.RecvHeaders(m_socket,this);

	// Even if we get an error, continue processing, because we may have read in
	// binary data and have a filter installed that can convert it for us.
	if (hi == INPUT_TIMEOUT || hi == INPUT_ERROR)   {
		// our socket has been closed as part of web server shutdown.  Since we haven't done 
		// any real work at this stage terminate request, don't log.
		if (! IsActive())
			return; 
	
		// Either we have no data, or there's no filter to read in data, return
		if (!g_pVars->m_fFilters || !g_pVars->m_fReadFilters || m_bufRequest.Count() == 0) {
			m_fKeepAlive = FALSE;
			if (hi == INPUT_ERROR && !m_fIsSecurePort) {
				if (m_rs == STATUS_OK) { // if no called into fcn set the error code, set = bad req.
					myretleave(m_rs = STATUS_BADREQ,61);
				}
				else {
					myretleave(m_rs,62);
				}
			}
			else
				return;  // don't send any data across socket on timeout.
		}
	}

	if (g_pVars->m_fFilters &&
	    ! CallFilter(SF_NOTIFY_READ_RAW_DATA))
		myleave(231);
	
	// parse the request headers
	if(!ParseHeaders())
		myleave(50);

	//  There were numerous filter calls in ParseHeaders, make sure none requested end of connection
	if (m_pFInfo && m_pFInfo->m_fFAccept==FALSE)
		myleave(63);  // don't change m_rs, filter set it as appropriate

	// If we received a request for 'OPTIONS *' or 'OPTIONS /', this requires special case
	// handeling to avoid authentication and other virtual root and redirection checks.
	if (m_fOptionsAsterisk || (m_pVRoot && (VERB_OPTIONS==m_idMethod) && IsRootVDir())) {
		if (! m_pWebsite->m_fWebDav)
			myretleave(m_rs = STATUS_NOTIMPLEM,83);

		if (HandleWebDav()) {
			err = 0;
		}
		else {
			DEBUGCHK(!IS_STATUS_2XX(m_rs));
			err = 64;	// force error to be sent
		}
		goto done;
	}

	// If VROOT was configured to auto-redirect, skip all other checks.
	if (m_pVRoot && m_pVRoot->fRedirect) {
		DEBUGMSG(ZONE_REQUEST,(L"HTTPD: VROOT maps to redirect, sending client to URL %a\r\n",m_pVRoot->pszRedirectURL));
		CHttpResponse resp(this,STATUS_MOVED,CONN_CLOSE);
		resp.SendRedirect(m_pVRoot->pszRedirectURL);
		err = 0;
		goto done;
	}

	// check if we successfully mapped the VRoot
	if (!m_wszPath)
		myretleave(m_rs = STATUS_NOTFOUND, 59);

	//
	// Authentication checks come first
	//
	// Check if we're requiring SSL on this VRoot.
	if ((!m_fIsSecurePort && ((GetPerms() & HSE_URL_FLAGS_SSL) || (GetPerms() & HSE_URL_FLAGS_SSL128))) ||
	    (m_fIsSecurePort && (m_SSLInfo.m_dwSessionKeySize < 128) && (GetPerms() & HSE_URL_FLAGS_SSL128)))
	{
		if (g_pVars->m_fFilters)
			CallFilter(SF_NOTIFY_ACCESS_DENIED);
		myretleave(m_rs = STATUS_FORBIDDEN, 55);
	}

	// If we want or require a client cert and we don't have one, we must renegotiate with the client.
	// Only call SSLRenegotiateRequest() once per session, in event that only HSE_URL_FLAGS_NEGO_CERT 
	// is set and client doesn't provide a cert.
	if (!m_SSLInfo.m_pClientCertContext && (GetPerms() & (HSE_URL_FLAGS_REQUIRE_CERT | HSE_URL_FLAGS_NEGO_CERT)) && !m_fPerformedSSLRenegotiateRequest) {
		if (SSLRenegotiateRequest())
			fRenegotiateSSL = TRUE;
		else if (GetPerms() & HSE_URL_FLAGS_REQUIRE_CERT) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: SSL renegotiate failed and we require a certificate, fail request\r\n"));
			myleave(236);
		}

		// If we're using client certificates and we're renegotiating, we haven't read in enough
		// information to determine whether to grant or deny access at this point.
		fDelayAuth = TRUE;
	}

	if (!fDelayAuth) {
		if (AllowNTLM() || AllowBasic() || g_pVars->m_fSSL)
			HandleAuthentication();

		if ( !CheckAuth())
			myretleave(m_rs = STATUS_UNAUTHORIZED, 52);
	}

	// ISAPI filters gets only 1 call per session to notify them of SF_NOTIFY_AUTHENTICATION event.
	if (! CallAuthFilterIfNeeded())
		myleave(294);

	if (!ReadPostData(g_pVars->m_dwPostReadSize,TRUE,fRenegotiateSSL))
		myleave(233); // ReadPostData sets m_rs

	// If content-length is less than # of bytes we've read in, then we have 
	// a garbage request (possibly multiplexing).  CE web server supports 
	// one specific case of multiplexing (allows multiple POSTS in one packet)
	// to work around a known broken HTTP client, however does not handle 
	// general case so fail out if this happens.
	// (-2 because sometimes browsers tack on CRLF to end of POST data but
	// don't count it in body-length).
	if ((int)m_dwContentLength < (int)(m_bufRequest.Count()-2)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: content-length is greater than muber of bytes received, probably HTTP 1.1 pipelined request.  HTTPD does not support this.\r\n"));
		m_rs = STATUS_BADREQ;
		myleave(241);
	}

	// For SSL case we should have client cert by now, so do authentication.
	if (fDelayAuth) {
		DEBUGCHK(IsSecure());

		if (!m_SSLInfo.m_pClientCertContext && (GetPerms() & HSE_URL_FLAGS_REQUIRE_CERT)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Virtual root set HSE_URL_FLAGS_REQUIRE_CERT but no cert from client, ending request\r\n"));
			m_rs = STATUS_FORBIDDEN;
			myleave(239);
		}
		
		HandleSSLClientCertCheck();

		if (!CheckAuth())
			myretleave(m_rs = STATUS_UNAUTHORIZED, 153);
	}

	//  if we're in middle of authenticating, jump past other stuff to end
	if (m_NTLMState.m_Conversation == NTLM_PROCESSING)
		myretleave(m_rs = STATUS_UNAUTHORIZED, 65);

	m_dwFileAttributes = GetFileAttributes(m_wszPath);

	if (! (m_wszExt && g_pVars->m_fExtensions && MapScriptExtension(m_wszExt,&wszISAPIPath,&fASP)) && VerbSupportsDirBrowsing() ) {
		if ((-1) == m_dwFileAttributes)
			myretleave(m_rs = GLEtoStatus(), 60);

		if (IsDirectory(m_dwFileAttributes)) {
			if (SendRedirectIfNeeded()) {
				err = 0;
				goto done;
			}
			// If there's no default page then we send dir list (later, after some
			// extra checking).  If there is a default page match m_wszPath is 
			// updated appropriatly.  This must be done before script processing.
			fSendDirectoryList = !MapDirToDefaultPage();
		}
	}

	// HandleScript returns true if page maps to ASP or ISAPI DLL, regardless of whether
	// we have correct permissions, component was included, there was an error, etc.
	// HandleScript sets its own errors.
	if ( !fSendDirectoryList && HandleScript()) {
		err = ! (IS_STATUS_2XX(m_rs) || (m_rs == STATUS_MOVED));  // Only send message on internal error.
		goto done;
	}

	if (m_idMethod != VERB_GET && m_idMethod != VERB_HEAD) {
		// if it's not an ISAPI or ASP or there's no DAV, we can't handle anything but GET and HEAD
		if (m_idMethod == VERB_UNKNOWN || !m_pWebsite->m_fWebDav) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Request fails, received unknown verb.  Non ISAPI/ASP pages can only handle GET+HEAD\r\n"));
			myretleave(m_rs = STATUS_NOTIMPLEM, 54);
		}

		// Let DAV handle all other flavors of verbs.
		// If HandleWebDav() fails and wants HandleRequest() to send error, it will
		// set m_rs appropriatly.  Otherwise HandleWebDav succeeded.
		// In either event no need to do rest of the code in this function, so goto-done.
		if (HandleWebDav()) {
			err = 0;
		}
		else {
			DEBUGCHK(!IS_STATUS_2XX(m_rs));
			err = 64;	// force error to be sent
		}

		goto done;
	}

	// check permissions
	if (!(GetPerms() & HSE_URL_FLAGS_READ))   {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Request fails, vroot does not have HSE_URL_FLAGS_READ permissions\r\n"));
		if (g_pVars->m_fFilters)
			CallFilter(SF_NOTIFY_ACCESS_DENIED);
		myretleave(m_rs = STATUS_FORBIDDEN, 55);
	}

	if (fSendDirectoryList) {
		// In this case there's no default page but directory browsing is turned
		// off.  Return same error code IIS does.
		if (FALSE == AllowDirBrowse())  {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Request fails, directory browsing is disabled\r\n"));
			if (g_pVars->m_fFilters)
				CallFilter(SF_NOTIFY_ACCESS_DENIED);
			myretleave(m_rs = STATUS_FORBIDDEN,78);
		}
		
		if(!EmitDirListing())
			myretleave(m_rs = STATUS_INTERNALERR, 53);
		err=0;
		goto done;
	}

	// If we get to here then we're just sending a plain old static file.
	// try to open the file & get the length
	if (INVALID_HANDLE_VALUE == (hFile = MyOpenReadFile(m_wszPath))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Request fails, unable to open file %s, GLE=0x%08x\r\n",m_wszPath,GetLastError()));
		myretleave(m_rs = GLEtoStatus(), 56);
	}
		
	// get the size
	if (((DWORD)-1) == (dwLength = GetFileSize(hFile, 0)))
		myretleave(m_rs = GLEtoStatus(), 57);

	// if it's a GET check if-modified-since
	if ((m_idMethod==VERB_GET) && IsNotModified(hFile, dwLength))
		myretleave(m_rs = STATUS_NOTMODIFIED, 58);

	// if it's a HTTP/0.9 request, just send back the body. NO headers
	if (m_dwVersion <= MAKELONG(9, 0)) {
		DEBUGMSG(ZONE_RESPONSE, (L"HTTPD: Sending HTTP/0.9 response with NO headers\r\n"));
		SendFile(m_socket, hFile, this);
	}
	else {
	// create a response object & send response
	// if it's a head request, skip the actual body
		CHttpResponse resp(this);
		resp.SetBody(hFile, m_wszExt, dwLength);
		resp.SendResponse();
	}
	DEBUGMSG(ZONE_REQUEST, (L"HTTPD: HTTP Request SUCCEEDED\r\n"));

	err  = 0;
	ret = m_rs = STATUS_OK;   
done:
	MyCloseHandle(hFile);

	if(err) {
		// end this session ASAP if we've encountered an error.
		if (m_rs == STATUS_INTERNALERR || m_rs == STATUS_BADREQ)
			m_fKeepAlive = FALSE;
		
		// if there's been an error but we're doing keep-alives, it's possible
		// there's POST data we haven't read in.  We need to read this
		// before sending response, or next time we recv() HTTP headers we'll
		// start in middle of POST rather than in the new request.
		if (m_fKeepAlive) {
			DEBUGCHK(m_dwContentLength - m_bufRequest.Count() >= 0);

			// If the amount of POST data we'd have to read is greater 
			// than the max we're willing to, then we end the request now.
			if (m_dwContentLength < g_pVars->m_dwPostReadSize) {
				if ((int)(m_dwContentLength - m_bufRequest.Count()) > 0) {
					DEBUGMSG(ZONE_REQUEST,(L"HTTP: HandleRequest: Error occured on keepalive, reading %d POST bytes now\r\n",m_dwContentLength - m_bufRequest.Count()));
					ReadPostData(m_dwContentLength - m_bufRequest.Count(),FALSE,FALSE);
				}
			}
			else {
				DEBUGMSG(ZONE_REQUEST,(L"HTTP: HandleRequest: client requested keep-alive but has content-length set = %d bytes, more than what we will read.  End session\r\n",m_dwContentLength));
				m_fKeepAlive = FALSE;
			}
		}
	
		CHttpResponse resp(this, m_rs);
		resp.SetDefaultBody();
		resp.SendResponse();
		DEBUGMSG(ZONE_REQUEST, (L"HTTPD: HTTP Request FAILED: GLE=%d err=%d status=%d (%d, %a)\r\n", 
			GetLastError(), err, ret, rgStatus[ret].dwStatusNumber, rgStatus[ret].pszStatusText));
	}

	// if in middle of NTLM request, don't do this stuff
	if (m_NTLMState.m_Conversation != NTLM_PROCESSING) {
		if (g_pVars->m_fFilters)  {
			CallFilter(SF_NOTIFY_END_OF_REQUEST);
			CallFilter(SF_NOTIFY_LOG);
		}
		g_pVars->m_pLog->WriteLog(this);
	}

	return;
}

BOOL CHttpRequest::IsNotModified(HANDLE hFile, DWORD dwLength) {
	if(m_ftIfModifiedSince.dwLowDateTime || m_ftIfModifiedSince.dwHighDateTime) {
		FILETIME ftModified;
		int iTemp;
		if(!GetFileTime(hFile, NULL, NULL, &ftModified)) {
			DEBUGMSG(ZONE_ERROR, (L"HTTPD: GetFileTime(%08x) failed\r\n", hFile));
			return FALSE; // assume it is modified
		}
		iTemp = CompareFileTime(&m_ftIfModifiedSince, &ftModified);

		DEBUGMSG(ZONE_RESPONSE, (L"HTTPD: IfModFT=%08x:%08x  ModFT=%08x:%08x IfModLen=%d Len=%d Compare=%d\r\n", 
			m_ftIfModifiedSince.dwHighDateTime, m_ftIfModifiedSince.dwLowDateTime ,
			ftModified.dwHighDateTime, ftModified.dwLowDateTime,
			m_dwIfModifiedLength, dwLength, iTemp));

		if((iTemp >= 0) && (m_dwIfModifiedLength==0 || (dwLength==m_dwIfModifiedLength)))
			return TRUE; // not modified
	}
	return FALSE; // assume modified
}


RESPONSESTATUS GLEtoStatus(int iGLE) {
	switch(iGLE)
	{
	case ERROR_PATH_NOT_FOUND: 
	case ERROR_FILE_NOT_FOUND: 
	case ERROR_INVALID_NAME: 
		return STATUS_NOTFOUND;
	case ERROR_ACCESS_DENIED: 
		return STATUS_FORBIDDEN;
	case ERROR_SHARING_VIOLATION:
		return STATUS_LOCKED;
	default:
		return STATUS_INTERNALERR;
	}
}

BOOL CHttpRequest::MapDirToDefaultPage(void)  {
	WCHAR wszTemp[MAX_PATH];
	DWORD ccNext = 0;

	if (m_ccWPath > ARRAYSIZEOF(wszTemp)) {
		DEBUGMSG(ZONE_REQUEST,(L"HTTPD: path is too long, will not attempt MapDirToDefaultPage\r\n"));
		return FALSE;
	}
	
	// make temp copy of dir path. append \ if reqd
	wcscpy(wszTemp, m_wszPath);
	// if we get here we have a trailing \ or / (otherwise we sent a redirect instead)
	DEBUGCHK(wszTemp[m_ccWPath-1]=='/' || wszTemp[m_ccWPath-1]=='\\');

	if (!m_pWebsite->m_wszDefaultPages)
		return FALSE;

	PWSTR wszNext=m_pWebsite->m_wszDefaultPages;
	ccNext = (1+wcslen(wszNext));

	for(; *wszNext; ccNext=(1+wcslen(wszNext)), wszNext+=ccNext) {
		if (ccNext + m_ccWPath >= ARRAYSIZEOF(wszTemp)) {
			// we'd overflow the buffer, and not interesting anyway because resulting name 
			// would be longer than filesys can handle in the 1st place.
			continue; 
		}
		
		wcscpy(wszTemp+m_ccWPath, wszNext);
		if((-1) != GetFileAttributes(wszTemp)) {	
			PWSTR pwsz;
			// found something
			DEBUGMSG(ZONE_RESPONSE, (L"HTTPD: Converting dir path (%s) to default page(%s)\r\n", m_wszPath, wszTemp));
			MyFree(m_wszPath);
			if (NULL == (m_wszPath = MySzDupW(wszTemp)))
				return FALSE;

			m_ccWPath = wcslen(m_wszPath);

			MyFree(m_wszExt);
			if (pwsz = wcsrchr(m_wszPath, '.'))
				m_wszExt = MySzDupW(pwsz);

			return TRUE;
		}
	}
	DEBUGMSG(ZONE_REQUEST, (L"HTTPD: No default page found in dir path (%s)\r\n", m_wszPath));
	return FALSE;
}


BOOL CHttpRequest::SendRedirectIfNeeded(void) {
	int iLen = strlen(m_pszURL);
	DEBUGCHK(iLen >= 1);

	if (! URLHasTrailingSlash()) {
		// we have allocated one extra char in m_pszURL already in case 
		// we needed to send a redirect back (see parser.cpp)
		m_pszURL[iLen]='/';
		m_pszURL[iLen+1]=0;

		CHttpResponse resp(this, STATUS_MOVED);
		resp.SendRedirect(m_pszURL); // send a special redirect body
		m_pszURL[iLen]=0; // restore m_pszURL 
		return TRUE;
	}

	return FALSE;
}


const char cszDirHeader1[] = "<head><title>%s - %s</title></head><body><H1>%s - %s</H1><hr>";
const char cszDirHeader2[] = "<A HREF=\"%s\">%s</A><br><br>";
const char cszPre[]        = "<pre>";
const char cszDirFooter[]  = "</pre><hr></body>";

#define MAXENTRYSIZE	(150+MAX_PATH+MAX_PATH)

BOOL CHttpRequest::EmitDirListing(void)  {
	WCHAR wszBuf1[MAX_PATH+10];
	WCHAR wszBuf2[MAX_PATH];
	char szHostBuf[MAX_PATH];

	if (m_ccWPath > ARRAYSIZEOF(wszBuf1))
		return FALSE;
	
	// generate listing into a buffer
	int  iSize = DIRBUFSIZE;
	PSTR pszBuf = MyRgAllocNZ(CHAR, iSize);
	int  iNext = 0;
	if(!pszBuf)
		return FALSE;

	// we know have enough space for the headers
	if ( 0 != gethostname(szHostBuf, sizeof(szHostBuf)))
		szHostBuf[0] = '\0';

	iNext += sprintf(pszBuf+iNext, cszDirHeader1, szHostBuf, m_pszURL, szHostBuf, m_pszURL);

	// find the parent path ignore the trailing slash (always present)

	int iURLLen = strlen(m_pszURL) - 2;
	for(int i=iURLLen; i>=0; i--)  {
		if(m_pszURL[i]=='/' || m_pszURL[i]=='\\')  {
			// Holds the string [Link to parent directory].
			WCHAR wszParentDirectory[256];
			CHAR szParentDirectory[256];
			LoadString(g_hInst,IDS_LINKTOPARENTDIR,wszParentDirectory,ARRAYSIZEOF(wszParentDirectory));
			MyW2A(wszParentDirectory,szParentDirectory,sizeof(szParentDirectory));

			// save & restore one char to temporarily truncate the URL at the parent path (incl slash)
			char chSave=m_pszURL[i+1];
			m_pszURL[i+1] = 0;
			
			iNext += sprintf(pszBuf+iNext, cszDirHeader2, m_pszURL, szParentDirectory);

			m_pszURL[i+1] = chSave;
			break;
		}
	}

	strcpy(pszBuf+iNext,cszPre);
	iNext += CONSTSIZEOF(cszPre);

	// create Find pattern
	DEBUGCHK(m_ccWPath == wcslen(m_wszPath));
	DEBUGCHK(URLHasTrailingSlashW(m_wszPath,m_ccWPath));
	WIN32_FIND_DATA fd;
	wcscpy(wszBuf1, m_wszPath);
	wcscpy(wszBuf1+m_ccWPath, L"*");

	// now iterate the files & subdirs (if any)
	HANDLE hFile = FindFirstFile(wszBuf1, &fd);
	if(INVALID_HANDLE_VALUE != hFile) {
		do {
			// check for space
			if((iSize-iNext) < MAXENTRYSIZE)  {
				if(!(pszBuf = MyRgReAlloc(CHAR, pszBuf, iSize, iSize+DIRBUFSIZE)))
					return FALSE;
				iSize += DIRBUFSIZE;
			}
			// convert date
			FILETIME   ftLocal;
			SYSTEMTIME stLocal;
			FileTimeToLocalFileTime(&fd.ftLastAccessTime, &ftLocal);
			FileTimeToSystemTime(&ftLocal, &stLocal);
			// format date
			GetDateFormat(LOCALE_SYSTEM_DEFAULT, DATE_SHORTDATE, &stLocal, NULL, wszBuf1, CCHSIZEOF(wszBuf1));
			GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_NOSECONDS, &stLocal, NULL, wszBuf2, CCHSIZEOF(wszBuf2));
			// generate HTML entry.
			const char cszDirEntry[]   = "%12S  %10S  %12d <A HREF=\"%S\">%S</A><br>";
			iNext += sprintf(pszBuf+iNext, cszDirEntry, wszBuf1, wszBuf2, fd.nFileSizeLow, fd.cFileName, fd.cFileName);
		}
		while(FindNextFile(hFile, &fd));
		// CloseHandle(hFile);	// This throws an exception on WinNT, use FindClose instead
		FindClose(hFile);
	}
	// emit footer
	strcpy(pszBuf+iNext,cszDirFooter);
	iNext += sizeof(cszDirFooter)-1;

	// create a response object & attach this body, then send headers & body
	CHttpResponse resp(this);
	resp.SetBody(pszBuf, cszTextHtml);
	resp.SendResponse();
	// free the buffer
	MyFree(pszBuf);
	return TRUE;
}


void CHttpRequest::Init(SOCKET sock, BOOL fSecure)  {
	memset(this, 0, sizeof(*this));

	m_dwSig              = CHTTPREQUEST_SIG;
	m_pFInfo             = CreateCFilterInfo();
	m_socket             = sock;
	m_dwVersion          = MAKELONG(0,1);
	m_fIsSecurePort      = fSecure;
	m_fHandleSSL         = fSecure && !g_pVars->m_fSSLSkipProcessing;
	m_pWebsite           = g_pVars;
	m_dwFileAttributes   = (DWORD)INVALID_HANDLE_VALUE;

	// put ourselves into linked list.
	m_pNext           = g_pRequestList;
	g_pRequestList    = this;
}


void CHttpRequest::RemoveFromList(void) {
	// Remove ourselves from global listening list
	EnterCriticalSection(&g_CritSect);

	g_pVars->m_nConnections--;
	DEBUGCHK(g_pVars->m_nConnections >= 0);

	CHttpRequest *pTrav = g_pRequestList;
	CHttpRequest *pFollow = NULL;
	
	while (pTrav) {
		if (pTrav == this) {
			if (pTrav == g_pRequestList)
				g_pRequestList = pTrav->m_pNext;

			if (pFollow)
				pFollow->m_pNext = pTrav->m_pNext;

			break;
		}

		pFollow = pTrav;
		pTrav   = pTrav->m_pNext;
	}
	DEBUGCHK(pTrav);

	CloseSocket(); // need to be procetced by CritSec because CloseAllConnections() my close on another thread.
	LeaveCriticalSection(&g_CritSect);
}

CHttpRequest::~CHttpRequest() {
	DEBUGMSG(ZONE_REQUEST,(L"HTTPD: Calling CHttpRequest destructor\r\n"));
	DEBUGCHK(m_dwSig == CHTTPREQUEST_SIG);

	FreeHeaders();
	FreeAuth();

	if(m_pFInfo) {
		delete m_pFInfo;
		m_pFInfo = 0;
	}

	FreeNTLMHandles(&m_NTLMState);
	FreePersistedAuthInfo();

	if (m_fHandleSSL)
		CloseSSLSession();

	RemoveFromList();
}


//  Called right before each HTTP Request (multiple times for a persisted session)
//  Frees request specific data, like destructor but keeps session data
//  (Filter alloc'd mem, NTLM state) in place.

BOOL CHttpRequest::ReInit() {
	DEBUGMSG(ZONE_REQUEST,(L"HTTPD: Calling CHttpRequest ReInit (between requests)\r\n"));
	DEBUGCHK(m_dwSig == CHTTPREQUEST_SIG); // catch memory 

	FreeHeaders();
	FreeAuth();		

	m_idMethod           = VERB_UNKNOWN;
	m_dwFileAttributes   = (DWORD)INVALID_HANDLE_VALUE;
	m_fOptionsAsterisk   = FALSE;
	m_ccWPath            = 0;
	m_pVRoot             = NULL;
	m_fBufferedResponse  = FALSE;
	m_fResponseHTTP11    = FALSE;

	m_bufRequest.Reset();				
	m_bufRespHeaders.Reset();
	m_bufRespBody.Reset();
	memset(&m_ftIfModifiedSince,0,sizeof(m_ftIfModifiedSince));

	if (m_pFInfo) {
		if ( !m_pFInfo->ReInit() )
			return FALSE;
	}

	// NTLM stuff.  If we're in middle of conversation, don't delete NTLM state info
	// We never free the library here, only in the destructor.
	if (m_NTLMState.m_Conversation == NTLM_DONE)  {
		FreeNTLMHandles(&m_NTLMState);

		//  Set the flags so that we know the context isn't initialized.  This
		//  would be relevent if user typed the wrong password.
		m_NTLMState.m_Conversation = NTLM_NO_INIT_CONTEXT;
	}

	// Certain values need to be zeroed
	m_dwContentLength = m_dwIfModifiedLength = m_dwVersion = 0;

	m_fKeepAlive = FALSE;
	m_hrPreviousReadClient = INPUT_OK;

	return TRUE;
}


//  If a filter makes a call to ServerSupportFunction to SEND_RESPONSE_HEADERS,
//  the http engine no longer directly display the requested page.  In this
//  case the filter acts like an ISAPI extension, it's responsible for returning
//  it's own content.  (Like IIS).  

//  This isn't the same as ASP's concept of having sent headers.  ASP's sent headers
//  stops script from doing other calls to send headers.  If Filter wants to send
//  more headers (which will appear in client browser window) fine, we copy IIS.

//  This is small enough that it's not worth putting into a stub.
BOOL CHttpRequest::FilterNoResponse(void) {
	if (m_pFInfo && m_pFInfo->m_fSentHeaders)
		return TRUE;

	return FALSE;
}

// Note:  This has not been made part of the ISAPI component because we need
// to do checking as to whether the requested operation is valid given our current
// component set.
BOOL CHttpRequest::HandleScript()  {
	DEBUG_CODE_INIT;
	BOOL ret = TRUE;		// Is page a ASP/ISAPI?

	WCHAR *wszMappedISAPI;
	WCHAR *wszExecutePath;
	BOOL  fMappedASP   = FALSE;
	BOOL  fMappedISAPI = FALSE;

	if (g_pVars->m_fExtensions && m_wszExt && MapScriptExtension(m_wszExt,&wszMappedISAPI,&fMappedASP)) {
		if (!fMappedASP) { 
			// If fMappedASP = TRUE we use asp.dll, otherwise wszMappedISAPI points to  ISAPI extension.
			fMappedISAPI = TRUE;
			wszExecutePath = wszMappedISAPI;
		}
		else
			wszExecutePath = m_wszPath;
	}
	else 
		wszExecutePath = m_wszPath;

	if (fMappedASP || fMappedISAPI) {
		// Using a script mapping
		if (m_pszPathInfo) {
			DEBUGCHK(0); // shouldn't be able to set path info on anything but .asp or .dll m_wszExt.
			MyFree(m_pszPathInfo);
		}
		if (NULL == (m_pszPathInfo = MySzDupA(m_pszURL))) {
			m_rs = STATUS_INTERNALERR;
			myleave(59);
		}
	}

	// If the file extension is .dll but the permissions flags don't have 
	// HSE_URL_FLAGS_EXECUTE, we send the dll as a file.  Like IIS.
	// If the VRoot was not executable we would download the dll, regardless of whether
	// extenions are a component or not.  Like IIS.
	if (GetScriptType() == SCRIPT_TYPE_EXTENSION || fMappedISAPI ||
	   (GetPerms() & HSE_URL_FLAGS_EXECUTE) &&  
	   (m_wszExt && (0==_wcsicmp(m_wszExt, L".DLL")))) 	{
		if (FALSE == g_pVars->m_fExtensions) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Cannot load ISAPI extension because ISAPI is not a component of httpd.dll\r\n"));
			m_rs = STATUS_NOTIMPLEM;
			myleave(88);
		}

		if (! IsLocalFile(wszExecutePath)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Cannot load ISAPI extension because it is not on the local device\r\n"));
			m_rs = STATUS_FORBIDDEN;
			if (g_pVars->m_fFilters)
				CallFilter(SF_NOTIFY_ACCESS_DENIED);
			myleave(87);
		}

		if (! TranslateRequestToScript()) {
			ret = (m_rs != STATUS_OK); // return TRUE to stop remainder of HandleRequest processing on err
			goto done;
		}

		if (! SetPathTranslated()) {
			m_rs = STATUS_INTERNALERR;
			myleave(94);
		}

		if (! ExecuteISAPI(wszExecutePath)) {
			m_rs = STATUS_INTERNALERR;
			myleave(53);
		}
	}

	// check if it's an executable ASP.  If the appropriate permissions aren't set,
	// we send an access denied message.  Never download an ASP file's source
	// code under any conditions.
	else if(GetScriptType() == SCRIPT_TYPE_ASP || fMappedASP || 
	   m_wszExt && (0==_wcsicmp(m_wszExt, L".ASP"))) {
		if (FALSE == g_pVars->m_fASP) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Cannot load ASP interpretter because ASP is not a component of httpd.dll\r\n"));
			m_rs = STATUS_NOTIMPLEM;
			myleave(89);
		}

		if ( ! (GetPerms() & (HSE_URL_FLAGS_EXECUTE | HSE_URL_FLAGS_SCRIPT))) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: cannot load ASP because vroot does not have HSE_URL_FLAGS_EXECUTE | HSE_URL_FLAGS_SCRIPT set\r\n"));
			m_rs = STATUS_FORBIDDEN;
			
			if (g_pVars->m_fFilters)
				CallFilter(SF_NOTIFY_ACCESS_DENIED);
			myleave(79);
		}
	
		if (!IsLocalFile(m_wszPath)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Cannot load ASP page because it is not on the local device\r\n"));
			m_rs = STATUS_FORBIDDEN;

			if (g_pVars->m_fFilters)
				CallFilter(SF_NOTIFY_ACCESS_DENIED);
			myleave(87);
		}

		if (! TranslateRequestToScript()) {
			ret = (m_rs != STATUS_OK); // return TRUE to stop remainder of HandleRequest processing on err
			goto done;
		}

		if (! SetPathTranslated()) {
			m_rs = STATUS_INTERNALERR;
			myleave(94);
		}

		if(!ExecuteASP()) {
			// ExecuteASP sets m_rs on error. 
			myleave(92);
		}
	}
	else { // Neither an ASP or ISAPI.
		ret = FALSE;
	}

done:
	DEBUGMSG_ERR(ZONE_REQUEST,(L"HTTPD: HandleScript failed, err = %d, m_rs = %d\r\n",err,m_rs));
	return ret;
}


//  wszFile is the physical file we're going to try to load.  Function returns
//  true if file is local and false if it is on a network drive.

//  The only ways a file can be non-local on CE are if it has a UNC name 
//  (\\machineshare\share\file) or if it is mapped under the NETWORK directory.
//  However, the Network folder doesn't have to be named "network", so we
//  use the offical method to get the name
BOOL IsLocalFile(PWSTR wszFile) {
	// Are we requested a UNC name
	if ( wcslen(wszFile) >= 2) 	{
		if ( (wszFile[0] == '\\' || wszFile[0] == '/') && 
		     (wszFile[1] == '\\' || wszFile[1] == '/'))  {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Extension or ASP requested is not on local file system, access denied\r\n"));
			return FALSE;
		}
	}

#if defined  (UNDER_CE) && !defined (OLD_CE_BUILD)
	CEOIDINFO ceOidInfo;
	DWORD dwNetworkLen;


	if (!CeOidGetInfo(OIDFROMAFS(AFS_ROOTNUM_NETWORK), &ceOidInfo))  {
		return TRUE;	// if we can't load it assume that it's not supported in general, so it is local file
	}

	dwNetworkLen =  wcslen(ceOidInfo.infDirectory.szDirName);
	if (0 == wcsnicmp(ceOidInfo.infDirectory.szDirName,wszFile,dwNetworkLen))  {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Extension or ASP requested is not on local file system, access denied\r\n"));
		return FALSE;
	}
#endif

	return TRUE;
}

// dwMaxSizeToRead is HKLM\Comm\Httpd\PostReadSize in typcilac case, or
// is unread data if fInitialPostRead=0, which means that we're handling
// an error condition on keep-alive and need to read remaining post data.

// Note that we do NOT suck in remaining POST data if an ISAPI extension
// ran and had more data than was initially read in because it's the ISAPI's
// job to read all this data off the wire using ReadClient if they're going
// to do a keep-alive; if they don't do this then HTTPD will get parse errors,
// like IIS.

BOOL CHttpRequest::ReadPostData(DWORD dwMaxSizeToRead, BOOL fInitialPostRead, BOOL fSSLRenegotiate) {
	BOOL ret = TRUE;
	HRINPUT hi;
	
	if ((m_dwContentLength && dwMaxSizeToRead) || fSSLRenegotiate)  {
		DWORD dwRead = min(m_dwContentLength,dwMaxSizeToRead);

		hi = m_bufRequest.RecvBody(m_socket, dwRead, !fInitialPostRead,fInitialPostRead,this,fSSLRenegotiate);

		if (m_rs != STATUS_OK) {
			// if !STATUS_OK we've set err already, probably SSL forbidden issue.
			ret = FALSE;
		}
		else if ((hi != INPUT_OK) && (hi != INPUT_NOCHANGE))  { 
			m_rs = STATUS_BADREQ;
			ret = FALSE;
		}
		// If no new data was read (hi = INPUT_NOCHANGE) don't call filter.
		else if (g_pVars->m_fFilters  &&
		    hi != INPUT_NOCHANGE && 
		    ! CallFilter(SF_NOTIFY_READ_RAW_DATA)) {
			// let filter set error code.
			ret = FALSE;
		}	
	}

	if (ret == TRUE)
		m_dwInitialPostRead = m_bufRequest.m_iNextInFollow = m_bufRequest.m_iNextIn;

	return ret;
}

BOOL CHttpRequest::CheckAuth() {
	if (CheckAuth(GetAuthReq()))
		return TRUE;

	DEBUGMSG(ZONE_AUTH,(L"HTTPD: CheckAuth fails.  Access required=%d, access granted=%d\r\n",GetAuthReq(),m_AuthLevelGranted));
	if (g_pVars->m_fFilters)
		CallFilter(SF_NOTIFY_ACCESS_DENIED);

	return FALSE;
}


BOOL IsPathScript(WCHAR *szPath, SCRIPT_TYPE scriptType) {
	if ((scriptType == SCRIPT_TYPE_EXTENSION) || (scriptType == SCRIPT_TYPE_ASP))
		return TRUE;

	WCHAR *szISAPI;
	BOOL  fMappedASP;

	WCHAR *szExt = wcsrchr(szPath, '.');
	if (!szExt)
		return FALSE;

	if ((0 == _wcsicmp(szExt,L".dll")) || (0 == _wcsicmp(szExt,L".asp")))
		return TRUE;

	return (MapScriptExtension(szExt,&szISAPI,&fMappedASP));
}

// Checks to see if we're mapped to an ISAPI/ASP page.
// If fCheckAccess=TRUE, also verifies that required permissions to execute have been granted.

BOOL CHttpRequest::IsScript(BOOL fCheckAccess) {
	WCHAR *szISAPI;
	BOOL fMappedASP;
	BOOL fMappedISAPI = (m_wszExt && MapScriptExtension(m_wszExt,&szISAPI,&fMappedASP) && !fMappedASP);

	// script map to an ISAPI, direct request to ISAPI ".dll", or vroot maps to ISAPI
	if (fMappedISAPI || (m_wszExt && (0==_wcsicmp(m_wszExt, L".DLL"))) || (GetScriptType() == SCRIPT_TYPE_EXTENSION))
		return fCheckAccess ? (GetPerms() & (HSE_URL_FLAGS_EXECUTE | HSE_URL_FLAGS_READ)) : TRUE;

	// script map to a ASP, direct request to file named *.asp , or vroot maps to ASP
	if (fMappedASP || (0==_wcsicmp(m_wszExt, L".ASP")) || (GetScriptType() == SCRIPT_TYPE_ASP))
		return fCheckAccess ? (GetPerms() & (HSE_URL_FLAGS_EXECUTE | HSE_URL_FLAGS_SCRIPT)) : TRUE;

	return FALSE;
}

// Is there a 'translate: f' HTTP header?  On return, one of three things can happen.
//   1. There's no translate header, so execute script as always (typical case)
//   2. There is a 'translate: f' header but no HSE_URL_FLAGS_SCRIPT_SOURCE access on vroot.
//        Send 403 to client.
//   3. There is a 'translate: f' header and script source access is granted.  HandleRequest 
//        processes remainder of request from then, most likely pushing off to WebDAV.
BOOL CHttpRequest::TranslateRequestToScript(void) {
	PSTR szHeader = FindHttpHeader(cszTranslate,ccTranslate);

	if (szHeader && (*szHeader == 'f' || *szHeader == 'F')) {
		DEBUGMSG(ZONE_REQUEST,(L"HTTPD: \"translate: 'f'\" header sent, will not forward request to script\r\n"));

		if (! (GetPerms() & HSE_URL_FLAGS_SCRIPT_SOURCE)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: request to access script source fails,HSE_URL_FLAGS_SCRIPT_SOURCE for this vroot\r\n"));
			m_rs = STATUS_FORBIDDEN;
		}
		if ((GetScriptType() == SCRIPT_TYPE_EXTENSION) || (GetScriptType() == SCRIPT_TYPE_ASP)) {
			if ((m_idMethod != VERB_OPTIONS) && (m_idMethod != VERB_PROPFIND)) {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: request to skip translatition on script that is mapped directly to VRoot is not supported, returning 403\r\n"));
				m_rs = STATUS_FORBIDDEN;
			}
		}
		return FALSE;
	}

	return TRUE;
}

PSTR CHttpRequest::FindHttpHeader(PCSTR szHeaderName, DWORD ccHeaderName) {
	PSTR szTrav = m_bufRequest.Headers();

	DEBUGCHK(szHeaderName[ccHeaderName-1] == ':');
	DEBUGCHK(strlen(szHeaderName) == ccHeaderName);
	DEBUGCHK(!m_fHeadersInvalid); // no temporary \0's in header.

	while (*szTrav != '\r') {
		if (0 == _strnicmp(szHeaderName,szTrav,ccHeaderName)) {
			PSTR szHeaderValue = szTrav + ccHeaderName;
			while ( (szHeaderValue)[0] != '\0' && IsNonCRLFSpace((szHeaderValue)[0])) { ++(szHeaderValue); }

			return szHeaderValue;
		}
		if (NULL == (szTrav = strstr(szTrav,"\r\n")))
			break;

		szTrav += 2;
	}
	return NULL;
}

// We only call authenticate filter once per session.
BOOL CHttpRequest::CallAuthFilterIfNeeded(void) {
	if (g_pVars->m_fFilters && ! (m_dwAuthFlags & AUTH_FILTER_DONE)) {
		m_dwAuthFlags |= AUTH_FILTER_DONE;
		return AuthenticateFilter();
	}
	return TRUE;
}
