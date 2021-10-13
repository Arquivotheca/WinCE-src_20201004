//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: davimpl.cpp
Abstract: WebDAV implementation of locking functions
--*/


#include "httpd.h"


#if defined (DEBUG)
inline void DebugCheckCacheManagerLocked(void) { DEBUGCHK(g_pFileLockManager->IsLocked()); }
inline void DebugCheckCacheManagerNotLocked(void) { DEBUGCHK(! g_pFileLockManager->IsLocked()); }
#else
inline void DebugCheckCacheManagerLocked(void) { ; }
inline void DebugCheckCacheManagerNotLocked(void) { ; }
#endif






//  *********************************************************
//  Main VERB handeling routines.
//  *********************************************************

BOOL CWebDav::DavLock(void) { 
	CWebDavFileNode *pFileNode    = NULL;
	BOOL fRet                     = FALSE;
	PSTR szLockHeader;


	if (! (m_pRequest->GetPerms() & HSE_URL_FLAGS_WRITE)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: LOCK fails, write permissions not turned on\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (IsCollection()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: LOCK fails, WinCE DAV does not support locking collections\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (! CheckIfHeaders())
		goto done;

	if (! CheckLockHeader())
		goto done;

	if (NULL != (szLockHeader = m_pRequest->FindHttpHeader(cszIfToken,ccIfToken))) {
		// If LOCK sets a "Lock-Token", the only supported scenario is refreshing
		// the timeout on a lock.
		DWORD dwTimeout = 0;

		if (m_pRequest->m_dwContentLength) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDav LOCK sets Lock-Token but has a body, illegal\r\n"));
			SetStatus(STATUS_UNSUPPORTED_MEDIA_TYPE);
			goto done;
		}

		if (! GetLockTimeout(&dwTimeout) || (dwTimeout == 0)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV lock unable to set updated lock, failing request\r\n"));
			SetStatus(STATUS_BADREQ);
			goto done;
		}

		if (m_nLockTokens != 1) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: LOCK refresh timeout sent %d lock tokens, need exactly 1\r\n",m_nLockTokens));
			SetStatus(STATUS_BADREQ);
			goto done;
		}

		g_pFileLockManager->Lock();
		
		if (NULL != (pFileNode = g_pFileLockManager->GetNodeFromFileName(m_pRequest->m_wszPath))) {
			CWebDavLock *pLock;
			if (pFileNode->HasLockId(m_lockIds[0],&pLock)) {
				DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: LOCK updating timeout on LockID=%I64d, file=%s, timeout seconds=%d\r\n",m_lockIds[0],m_pRequest->m_wszPath,dwTimeout));
				pLock->SetTimeout(dwTimeout);

				SetStatusAndXMLBody(STATUS_OK);
				SendLockTags(pFileNode);
				fRet = TRUE;
			}
			else {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: LOCK update timer failed, file %s not associated with lockID %I64d\r\n",m_pRequest->m_wszPath,m_lockIds[0]));
				SetStatus(STATUS_PRECONDITION_FAILED);
			}
		}
		else {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: LOCK update timer failed, file %s does not have any locks associated with it\r\n",m_pRequest->m_wszPath));
			SetStatus(STATUS_PRECONDITION_FAILED);
		}

		g_pFileLockManager->Unlock();
	}
	else {
		// Request for a new lock.
		CLockParse      lockParse(this);
	
		if (! GetDepth(DEPTH_ZERO) || ((m_Depth != DEPTH_INFINITY) && (m_Depth != DEPTH_ZERO))) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV LOCK fails, depth not set to infinity or zero\r\n"));
			SetStatus(STATUS_BADREQ);
			goto done;
		}
#if defined (DEBUG)
		// Like IIS's behavior in accepting depth infinity.
		if (m_Depth == DEPTH_INFINITY)
			DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: Warning, LOCK accepting depth infinite tag, however it will be treated like depth: 0 because WinCE doesn't support locking collections.\r\n"));
#endif
		if (! ParseXMLBody(&lockParse,FALSE))
			goto done;

		fRet = g_pFileLockManager->CreateLock(&lockParse,this);
	}

done:
	return fRet; 
}

BOOL CWebDav::DavUnlock(void) {
	BOOL fRet = FALSE;
	__int64 iLockId;

	if (! (m_pRequest->GetPerms() & HSE_URL_FLAGS_WRITE)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: UNLOCK fails, write permissions not turned on\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (IsCollection()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: UNLOCK fails, WinCE DAV does not support locking collections\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (IsNull()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: UNLOCK fails, file %s does not exist\r\n",m_pRequest->m_wszPath));
		SetStatus(STATUS_NOTFOUND);
		goto done;		
	}

	if (0 == (iLockId = CheckLockTokenHeader())) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK); //CheckLockTokenHeader sets errors 
		goto done;
	}

	if (! g_pFileLockManager->IsFileAssociatedWithLockId(m_pRequest->m_wszPath,iLockId)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: File <%s> is not associated with LockID %I64d\r\n",m_pRequest->m_wszPath,iLockId));
		SetStatus(STATUS_PRECONDITION_FAILED);
		goto done;
	}

	if (! g_pFileLockManager->DeleteLock(iLockId)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD UNLOCK fails, lockID %I64d does not exist\r\n",iLockId));
		SetStatus(STATUS_PRECONDITION_FAILED);
		goto done;
	}

	DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: UNLOCK successfully deleted LockID %I64d\r\n",iLockId));
	fRet = TRUE;
done:
	if (fRet) {
		DEBUGCHK(m_pRequest->m_rs == STATUS_OK);
		CHttpResponse resp(m_pRequest,STATUS_NOCONTENT);
		resp.SendResponse();
	}

	return fRet;
}


//  *********************************************************
//  Helper routines for LOCK, UNLOCK, and other verbs
//  like MOVE and DELETE that query lock status.
//  *********************************************************


BOOL CWebDav::GetLockTimeout(DWORD *pdwTimeout) {
	DEBUGCHK((*pdwTimeout) == 0);
	DEBUGCHK(m_pRequest->m_idMethod == VERB_LOCK);

	CHAR *szTimeout = m_pRequest->FindHttpHeader(cszTimeOut,ccTimeOut);
	CHAR *szTrav;
	
	if (! szTimeout) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDAV did not send \"timeout:\" http header\r\n"));
		return TRUE; // lack of header is not an error.
	}

	CHeaderIter header(szTimeout);
	for (szTrav = header.GetNext(); szTrav; szTrav = header.GetNext()) {
		if (0 == _strnicmp(szTrav,cszSecond,ccSecond)) {
			szTrav += ccSecond;
			if (0 == (*pdwTimeout = atoi(szTrav))) {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: LOCK \"timeout:\" header not propertly formatted\r\n"));
				SetStatus(STATUS_BADREQ);	
				return FALSE;
			}
			break;
		}
		else if (0 == _strnicmp(szTrav,cszInfinite,ccInfinite)) {
			*pdwTimeout = INFINITE;
			break;
		}
#if defined (DEBUG)
		else {
			DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: Received unknown field in \"timeout:\" header.  Ignoring, no error\r\n"));
		}
#endif // DEBUG
	}

	if (*pdwTimeout > MAX_DAV_LOCK_TIMEOUT) {
		DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: WebDAV LOCK requested timeout of %ud seconds, setting to maximum allowed %ud seconds\r\n",*pdwTimeout,MAX_DAV_LOCK_TIMEOUT));
		*pdwTimeout = MAX_DAV_LOCK_TIMEOUT;
	}

	return TRUE;
}


PSTR CHeaderIter::GetNext(void) {
	ResetSaved();
	PSTR szTrav = m_szNextString;

	SkipWhiteSpaceNonCRLF(szTrav);

	if (*szTrav == 0 || *szTrav == '\r')
		return NULL;

	m_szStartString = szTrav;

	// look for ','
	while ((*szTrav != '\r') && (*szTrav != ','))
		szTrav++;

	m_szNextString = szTrav+1;
	szTrav--;

	// backup to remove existing spaces
	while (IsNonCRLFSpace(*szTrav)) szTrav--;

	m_szSave  = (szTrav+1);
	cSave     = *m_szSave;
	*m_szSave = 0;

	return m_szStartString;
}

// Skip W/ in weak header parsing
inline PSTR SkipWeakTagIfNeeded(PSTR pszToken) {
	if (*pszToken == 'W' && *(pszToken+1) == '/')
		return pszToken+2;

	return pszToken;
}

BOOL ETagEqualsHeader(PSTR szHeader, PSTR szETag) {
	DEBUGCHK(szHeader);
	PSTR    szToken;

	CHeaderIter headIter(szHeader);

	for (szToken = headIter.GetNext(); szToken; szToken = headIter.GetNext()) {
		szToken = SkipWeakTagIfNeeded(szToken);

		if (*szToken == '*')
			return TRUE;
		else if (0 == strcmp(szETag,szToken))
			return TRUE;
	}
	
	return FALSE;
}

int WriteOpaqueLockToken(PSTR szBuf, __int64 iLockId, BOOL fClosingBrackets) {
	PSTR szWrite = szBuf;

	if (fClosingBrackets)
		*szWrite++ = '<';

	strcpy(szWrite,cszOpaquelocktokenPrefix);
	szWrite += ccOpaquelocktokenPrefix;

	strcpy(szWrite,g_pFileLockManager->m_szLockGUID);
	szWrite += SVSUTIL_GUID_STR_LENGTH;

	*szWrite++ = ':';

	szWrite += sprintf(szWrite,"%I64d",iLockId);

	if (fClosingBrackets) {
		*szWrite++ = '>';
		*szWrite++ = 0;
	}

	return (szWrite - szBuf);
}

// Checks that szHeader is equal to "opaquelocktoken:CE_SERVER_GUID:" and removes the LockID after
// returns a pointer to next.  Returns 0 on failure.
__int64 GetLockIDFromOpaqueHeader(PSTR szToken) {
	__int64 iLockId;

	if (0 != _strnicmp(szToken,cszOpaquelocktokenPrefix,ccOpaquelocktokenPrefix)) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: DAV Lock header check fails, '<' not followed by %s\r\n",cszOpaquelocktokenPrefix));
		return 0;
	}

	szToken += ccOpaquelocktokenPrefix;
	if (0 != _strnicmp(szToken,g_pFileLockManager->m_szLockGUID,SVSUTIL_GUID_STR_LENGTH)) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: DAV Lock header check fails, GUID sent does not match system's, which is <<%s>>\r\n",g_pFileLockManager->m_szLockGUID));
		return 0;
	}

	szToken += SVSUTIL_GUID_STR_LENGTH;
	if (*szToken != ':') {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: DAV Lock header check fails, GUID sent does not contain ':' at end\r\n"));
		return 0;
	}
	szToken++;  // skip ':'

	if (0 == (iLockId = _atoi64(szToken))) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: DAV Lock header check fails, GUID does not contain 64 bit lockID\r\n"));
		return 0;
	}
	return iLockId;
}


#define OPEN_URI         '<'
#define CLOSE_URI        '>'
#define OPEN_LIST        '('
#define CLOSE_LIST       ')'
#define OPEN_LOCK_TOKEN  '<'
#define CLOSE_LOCK_TOKEN '>'
#define OPEN_ETAG        '['
#define CLOSE_ETAG       ']'


// Per RFC 2518, section 9.5:
//   Lock-Token = "Lock-Token" ":" Coded-URL
//   Coded-URL  = "<" URI ">"

__int64 CWebDav::CheckLockTokenHeader(void) {
	PSTR szLockToken = NULL;
	PSTR szTrav;
	PSTR szEndOfURI;
	PSTR szEndOfHeader;
	__int64 iLockId = 0;

	DEBUGCHK(m_pRequest->m_idMethod == VERB_UNLOCK);

	if (NULL == (szLockToken = m_pRequest->FindHttpHeader(cszLockToken,ccLockToken))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: UNLOCK fails, \"Lock-token\" not sent and is required\r\n"));
		SetStatus(STATUS_BADREQ);
		goto done;
	}
	szTrav = szLockToken;

	svsutil_SkipWhiteSpace(szTrav);
	if (*szTrav != OPEN_URI) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: UNLOCK fails, \"Lock-token\" header does not begin with '<'\r\n"));
		SetStatus(STATUS_BADREQ);
		goto done;
	}
	szTrav++;  // skip '<'

	szEndOfHeader = strchr(szLockToken,'\r');
	szEndOfURI = strchr(szTrav,CLOSE_URI);

	if (!szEndOfURI || (szEndOfURI > szEndOfHeader)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: UNLOCK fails,\"Lock-token\" did not terminate with '>'\r\n"));
		SetStatus(STATUS_BADREQ);
		goto done;
	}

	iLockId = GetLockIDFromOpaqueHeader(szTrav);

	if (iLockId == 0) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: UNLOCK fails, \"Lock-token\" fields formatted incorrectly\r\n"));
		SetStatus(STATUS_BADREQ);
		goto done;
	}
done:
	return iLockId;
}


// Constants and logic in CheckLockHeader per RFC 2518, section 9.4 parsing of "If:" header rules
//	Format of the If header
//		If = "If" ":" ( 1*No-tag-list | 1*Tagged-list)
//		No-tag-list = List
//		Tagged-list = Resource 1*List
//		Resource = Coded-url
//		List = "(" 1*(["Not"](State-token | "[" entity-tag "]")) ")"
//		State-token = Coded-url
//		Coded-url = "<" URI ">"

// Parses "If:" HTTP header to see what locks we have.
BOOL CWebDav::CheckLockHeader(void) {
	PSTR    szLockHeaderInBuf;    // points inside main request class, do not free
	PSTR    szLockHeader        = NULL;
	PSTR    szEndOfHeader       = NULL;
	PSTR    szEndOfStatement    = NULL;
	BOOL    fRet                = FALSE;
//	BOOL    fFirstPass          = TRUE;
	WCHAR   *wszPhysicalPath    = NULL;
	PSTR    szTrav;
	SVSSimpleBuffer urlBuf(512);

	if (NULL == (szLockHeaderInBuf = m_pRequest->FindHttpHeader(cszIfToken,ccIfToken)))
		return TRUE;

	szEndOfHeader = strchr(szLockHeaderInBuf,'\r');
	DEBUGCHK(szEndOfHeader); // otherwise invalid HTTP headers

	*szEndOfHeader = 0;
	szLockHeader = MySzDupA(szLockHeaderInBuf);
	*szEndOfHeader = '\r';

	if (!szLockHeader) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV out of memory\r\n"));
		SetStatus(STATUS_INTERNALERR);
		goto done;
	}
	szTrav = szLockHeader;

	while (*szTrav) {
		urlBuf.Reset();
	
		svsutil_SkipWhiteSpace(szTrav);
		if (*szTrav != OPEN_URI) {

#if 0  // New policy is to be more lenient, accept this case at any time.  See RFC 2518, section 8.9.6 for example of why we accept this.
			// On the first pass through loop, we'll treat a no-tag production 
			// as if it refers to the request URI
		
			if (!fFirstPass) {
				DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDAV does not have '<' for tagged-list, skipping to end\r\n"));
				fRet = TRUE;
				goto done;
			}
#endif

			MyFreeNZ(wszPhysicalPath);
			wszPhysicalPath = NULL;
		}
		else {
			PSTR szEndOfUrl = NULL;
			szTrav++;  // skip '<'
			szTrav = SkipHTTPPrefixAndHostName(szTrav);

			if (szTrav)
				szEndOfUrl = strchr(szTrav,CLOSE_URI);
			
			if (NULL == szEndOfUrl) {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV skipping host name and headers of tagged-URL failed in \"if:\" header\r\n"));
				SetStatus(STATUS_BADREQ);
				goto done;
			}

			// need to copy results to a new buffer, as canonicilization changes data in place.
			if (! urlBuf.AllocMem(szEndOfUrl-szTrav+1)) {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV out of memory\r\n"));
				SetStatus(STATUS_INTERNALERR);
				goto done;
			}
			*szEndOfUrl = 0;
			MyInternetCanonicalizeUrlA(szTrav,urlBuf.pBuffer,NULL,0);
			*szEndOfUrl = CLOSE_URI;

			MyFreeNZ(wszPhysicalPath);

			wszPhysicalPath = m_pRequest->m_pWebsite->m_pVroots->URLAtoPathW(urlBuf.pBuffer);
			szTrav = szEndOfUrl+1;  // skip past the URI and the '>'
		}
//		fFirstPass = FALSE;

		svsutil_SkipWhiteSpace(szTrav);

		if (*szTrav != OPEN_LIST) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV \"If:\" headers did not send a statement, got %c when expecting %c\r\n",*szTrav,OPEN_LIST));
			SetStatus(STATUS_BADREQ);
			goto done;
		}
		szTrav++; // skip '('
		svsutil_SkipWhiteSpace(szTrav);

		if (NULL == (szEndOfStatement = strchr(szTrav,CLOSE_LIST))) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV \"If:\" headers did not send closing end of statement ')' character\r\n"));
			SetStatus(STATUS_BADREQ);
			goto done;
		}
		*szEndOfStatement = 0;

		while (szTrav < szEndOfStatement) {
			// A list has one or more state-tokens and/or entity tags.  Parse these now.
			// There are two types of "failure" conditions we can hit during parsing: 
			// an out and out parse error (for instance not closing '>'), which should cause
			// request to fail, send 400.  However we can also have case where a lock doesn't match
			// what we were requested, but this is allowed if it's preceeded by a "NOT" token.

			BOOL fNot;
			BOOL fSuccess = FALSE;

			// NOT statement's scope is for the immediatly proceeding state-token/entity tag.
			if (0 == _strnicmp(szTrav,cszNot,ccNot)) {
				fNot = TRUE;
				szTrav += ccNot;
				svsutil_SkipWhiteSpace(szTrav);
			}
			else 
				fNot = FALSE;

			if (*szTrav == OPEN_LOCK_TOKEN) {
				PSTR szEndToken;
				if (NULL == (szEndToken = strchr(szTrav,CLOSE_LOCK_TOKEN))) {
					DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV \"If:\" headers did not send closing end of statement '>' character\r\n"));
					SetStatus(STATUS_BADREQ);
					goto done;
				}

				*szEndToken = 0;
				fSuccess = CheckTokenExpression(++szTrav,wszPhysicalPath);
				*szEndToken = CLOSE_LOCK_TOKEN;
				szTrav = szEndToken+1;
			}
			else if (*szTrav == OPEN_ETAG) {
				PSTR szEndEtag;
				if (NULL == (szEndEtag = strchr(szTrav,CLOSE_ETAG))) {
					DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV \"If:\" headers did not send closing end of statement ']' character\r\n"));
					SetStatus(STATUS_BADREQ);
					goto done;
				}

				*szEndEtag = 0;
				fSuccess = CheckETagExpression(++szTrav,wszPhysicalPath);
				*szEndEtag = CLOSE_ETAG;
				szTrav = szEndEtag+1;
			}
			else {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDav \"If:\" headers did not set '<' or '[' inside statement, invalid statement\r\n"));
				SetStatus(STATUS_BADREQ);
				goto done;
			}

			// Either we successfully matched a token but we shouldn't have (NOT before rule)
			// or we didn't match a token and there was not a "NOT" string.
			if ((fSuccess && fNot) || (!fSuccess && !fNot)) {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: DAV parsing \"If:\" headers failed, condition not met\r\n"));
				SetStatus(STATUS_PRECONDITION_FAILED);
				goto done;
			}

			svsutil_SkipWhiteSpace(szTrav);
		}

		*szEndOfStatement = CLOSE_LIST;
		szTrav = szEndOfStatement + 1;
		szEndOfStatement  = NULL;

		svsutil_SkipWhiteSpace(szTrav);
	}
	DEBUGCHK((szTrav-szLockHeader) == (int)strlen(szLockHeader));
	
	fRet = TRUE;
done:
	DebugCheckCacheManagerNotLocked();
	MyFreeNZ(szLockHeader);
	MyFreeNZ(wszPhysicalPath);

	if (szEndOfStatement)
		*szEndOfStatement = CLOSE_LIST;
		
	return fRet;
}


// Checks contetns of <...> statement in an "if:" lock header

// Note: Don't set error in here or in helper functions because if "Not"
// prepends the string then failure is expected.
BOOL CWebDav::CheckTokenExpression(PSTR szToken, WCHAR *szPath) {
	svsutil_SkipWhiteSpace(szToken);
	__int64 iLockId;

	if (*szToken == '\"' || *szToken == '<')
		szToken++;

	if (0 == (iLockId = GetLockIDFromOpaqueHeader(szToken)))
		return FALSE;

	if (szPath && ! g_pFileLockManager->IsFileAssociatedWithLockId(szPath,iLockId))
		return FALSE;

	// store the lockId for possible later use during the request.
	if (! m_lockIds.SRealloc(m_nLockTokens+1))
		return FALSE;

	m_lockIds[m_nLockTokens++] = iLockId;
	return TRUE;
}

// Don't set error here because if "not" prepends string then failure is expected.
BOOL CWebDav::CheckETagExpression(PSTR szToken, WCHAR *szPath) {
	svsutil_SkipWhiteSpace(szToken);

	WIN32_FILE_ATTRIBUTE_DATA w32Data;
	WIN32_FILE_ATTRIBUTE_DATA *pData;

	CHAR szETag[MAX_PATH];

	// If szPath is known use it, otherwise default to 
	if (szPath) {
		if (! GetFileAttributesEx(szPath,GetFileExInfoStandard,&w32Data)) {
			DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: GetFileAttributesEx(%s) failed, GLE=0x%08x\r\n",szPath,GetLastError()));
			return FALSE;
		}
		pData = &w32Data;
	}
	else {
		if (! DavGetFileAttributesEx())
			return FALSE;

		pData = &m_fileAttribs;
	}

	GetETagFromFiletime(&pData->ftLastWriteTime,pData->dwFileAttributes,szETag);

	szToken = SkipWeakTagIfNeeded(szToken);
	return (0 == (strcmp(szToken,szETag)));
}

// looks at 'if-match' and 'if-not-match' fields to determine whether to continue processing.
BOOL CWebDav::CheckIfHeaders(void) {
	PSTR szHeader;
	CHAR szETag[256];

	if (IsNull()) {
		// If file doesn't exist, "If-match" headers are invalid.
		if (m_pRequest->FindHttpHeader(cszIf_Match,ccIf_Match)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: resource %s is not existant, so \"if-match\" headers sent cause request to fail\r\n",m_pRequest->m_wszPath));
			SetStatus(STATUS_PRECONDITION_FAILED);
			return FALSE;
		}
		return TRUE;
	}

	if (! DavGetFileAttributesEx()) {
		DEBUGCHK(0);  // resource should be NULL in this case.
		return FALSE;
	}

	GetETagFromFiletime(&m_fileAttribs.ftLastWriteTime,m_fileAttribs.dwFileAttributes,szETag);

	if (szHeader = m_pRequest->FindHttpHeader(cszIf_Match,ccIf_Match)) {
		if (! ETagEqualsHeader(szHeader,szETag)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV request fails because if-match header doesn't match, required etag=%s\r\n",szETag));
			SetStatus(STATUS_PRECONDITION_FAILED);
			return FALSE;
		}
	}

	if (szHeader = m_pRequest->FindHttpHeader(cszIf_None_Match,ccIf_None_Match)) {
		if (ETagEqualsHeader(szHeader,szETag)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDAV request fails because if-not-match header matches etag=%s\r\n",szETag));
			SetStatus(STATUS_PRECONDITION_FAILED);
			return FALSE;
		}
	}

	return TRUE;
}

// prints out information about lock(s) associated with a file.
BOOL CWebDav::SendLockTags(CWebDavFileNode *pLockedNode, BOOL fPrintPropTag) {
	DebugCheckCacheManagerLocked();
	CHAR szBuf[256];

	BOOL fRet = FALSE;

	CWebDavLock *pLock = pLockedNode->m_pLockList;
	PCSTR szScope = pLockedNode->IsSharedLock() ? cszShared : cszExclusive;

	if (fPrintPropTag) {
		if (! m_bufResp.StartTagNoEncode(cszPropNS,ccPropNS))
			goto done;
	}

	while (pLock) {
		WriteOpaqueLockToken(szBuf,pLock->m_i64LockId,FALSE);

		if (! m_bufResp.StartTag(cszLockDiscovery)    ||
		    ! m_bufResp.StartTag(cszActiveLock)       ||
		    ! m_bufResp.StartTag(cszLockType)         ||
		    ! m_bufResp.SetEmptyElement(cszWrite)     || // spec only supports <write> locks currently
		    ! m_bufResp.EndTag(cszLockType)           ||
		    ! m_bufResp.StartTag(cszLockScope)        ||
		    ! m_bufResp.SetEmptyElement(szScope)      ||
		    ! m_bufResp.EndTag(cszLockScope)          ||
		    ! m_bufResp.StartTag(cszLockTokenTag)     ||
		    ! m_bufResp.StartTag(cszHref)             ||
		    ! m_bufResp.Encode(szBuf)                 ||
		    ! m_bufResp.EndTag(cszHref)               ||
		    ! m_bufResp.EndTag(cszLockTokenTag)) 
		{
			goto done;
		}

		if (pLock->m_szOwner) {
			if (! m_bufResp.BeginBrace()  ||
			    ! m_bufResp.Append(cszOwner,ccOwner)) {
				goto done;
			}

			if (pLock->m_szOwnerNSTags) {
				if (! m_bufResp.AppendCHAR(' ') ||
				    ! m_bufResp.Append(pLock->m_szOwnerNSTags,pLock->m_ccOwnerNSTags)) {
					goto done;
				}
			}

			if (! m_bufResp.CloseBrace() ||
			    ! m_bufResp.Append(pLock->m_szOwner,pLock->m_ccOwner)    ||
			    ! m_bufResp.EndTag(cszOwner)) 
			{
				goto done;
			}
		}

		sprintf(szBuf,"%s%d",cszSecond,pLock->m_dwBaseTimeout);

		if ( !m_bufResp.StartTag(cszDepthTag)                   ||
		     !m_bufResp.Append(csz0,cc0)  ||
		     !m_bufResp.EndTag(cszDepthTag)                     ||
		     !m_bufResp.StartTag(cszTimeoutTag)                 ||
		     !m_bufResp.Encode(szBuf)                           ||
		     !m_bufResp.EndTag(cszTimeoutTag))
		{
			goto done;
		}

		if (! m_bufResp.EndTag(cszActiveLock)          ||
		    ! m_bufResp.EndTag(cszLockDiscovery))
		{
			goto done;
		}

		pLock = pLock->m_pNext;
	}

	if (fPrintPropTag) {
		if (! m_bufResp.EndTag(cszProp))
			goto done;
	}

	fRet = TRUE;
done:
	return fRet;
}

// Certain VERBs want STATUS_CONFLICT on a not found, or can also handle LOCKed.
void CWebDav::SendLockedConflictOrErr(WCHAR *szPath, WCHAR *szAlternatPath) {
	if (SendLockErrorInfoIfNeeded(szPath))
		return;

	if (szAlternatPath && IsFileLocked(szAlternatPath)) {
		SetMultiStatusErrorFromURL(STATUS_LOCKED,m_pRequest->m_pszURL);
		return;
	}

	DWORD dwErr = GetLastError();

	if (dwErr == ERROR_PATH_NOT_FOUND)
		SetStatus(STATUS_CONFLICT);
	else
		SetStatus(GLEtoStatus(dwErr));
}

BOOL CWebDav::IsFileLocked(const WCHAR *szPath) {
	BOOL fRet = FALSE;

	if (IsGLELocked()) {
		g_pFileLockManager->Lock();

		if (g_pFileLockManager->GetNodeFromFileName((WCHAR*)szPath))
			fRet = TRUE;

		g_pFileLockManager->Unlock();
	}
	return fRet;
}

// If a file is locked and we have lock info about it, then send this to the client.
// If we're not locked, return FALSE so that the calling function can set its own error.
BOOL CWebDav::SendLockErrorInfoIfNeeded(WCHAR *szPath) {
	BOOL fRet = FALSE;

	if (IsGLELocked()) {
		g_pFileLockManager->Lock();
		CWebDavFileNode *pFileNode = g_pFileLockManager->GetNodeFromFileName(szPath);
		if (pFileNode) {
			SetStatusAndXMLBody(STATUS_LOCKED);
			SendLockTags(pFileNode);
			fRet = TRUE;
		}
		g_pFileLockManager->Unlock();
	}

	return fRet;
}

//  *********************************************************
//  Implementation of Lock cache manager
//  *********************************************************

static const char cszXMLNS[] = "xmlns:";
static const DWORD ccXMLNS   = SVSUTIL_CONSTSTRLEN(cszXMLNS);

//
//  CWebDavLock
//
BOOL CWebDavLock::InitLock(PSTR szOwner, CNameSpaceMap *pNSMap, __int64 iLockId, DWORD dwBaseTimeout) {
	// m_szOwner is allowed to be NULL, not a required field.
	m_pNext         = NULL;
	m_szOwner = m_szOwnerNSTags = NULL;

	if (szOwner && (NULL == (m_szOwner = MySzDupA(szOwner))))
		return FALSE;

	if (m_szOwner)
		m_ccOwner = strlen(m_szOwner);
	else
		m_ccOwner = 0;

	// Setup NameSpace prefix->URI tags that are put in <owner> XML tag.
	if (m_szOwner && pNSMap) {
		CNameSpaceMap *pTrav = pNSMap;
		int iLenRequired = 0;
		int iOffset      = 0;

		while (pTrav) {
			iLenRequired += MyW2UTF8(pTrav->m_szPrefix,0,0) + MyW2UTF8(pTrav->m_szURI,0,0) + ccXMLNS + 15;
			pTrav = pTrav->m_pNext;
		}

		if (NULL == (m_szOwnerNSTags = MySzAllocA(iLenRequired)))
			return FALSE;

		pTrav = pNSMap;

		while (pTrav) {
			// If there are multiple URI's with the same Namespace, use the last one
			// put into the list for the URI.
			CNameSpaceMap *pNode = pTrav;
			while (pNode->m_pChild)
				pNode = pNode->m_pChild;
			
			
			// The code below is effectively doing this sprintf.  Don't use sprintf
			// because we want UTF8 conversion if it's available, sprintf does CP_ACP on %S.
			// iOffset += sprintf(m_szOwnerNSTags+iOffset," %s%S=\"%S\"",cszXMLNS,pTrav->m_szPrefix,pNode->m_szURI);

			// don't spit out ' ' on 1st pass.
			if (iOffset != 0)
				m_szOwnerNSTags[iOffset++] = ' ';

			strcpy(m_szOwnerNSTags+iOffset,cszXMLNS);
			iOffset += ccXMLNS;

			iOffset += MyW2UTF8(pTrav->m_szPrefix,m_szOwnerNSTags+iOffset,iLenRequired-iOffset) - 1;
			m_szOwnerNSTags[iOffset++] = '=';
			m_szOwnerNSTags[iOffset++] = '\"';

			iOffset += MyW2UTF8(pNode->m_szURI,m_szOwnerNSTags+iOffset,iLenRequired-iOffset) - 1;
			m_szOwnerNSTags[iOffset++] = '\"';

			pTrav = pTrav->m_pNext;
		}
		m_szOwnerNSTags[iOffset++] = 0;

		m_ccOwnerNSTags = strlen(m_szOwnerNSTags);
		DEBUGCHK(m_ccOwnerNSTags < (DWORD)iLenRequired);
	}
	else {
		m_ccOwnerNSTags = 0;
	}

	m_i64LockId     = iLockId;
	SetTimeout(dwBaseTimeout);

	return TRUE;
}

void CWebDavLock::SetTimeout(DWORD dwSecondsToLive) {
	DEBUGCHK(dwSecondsToLive != 0);

	__int64 ft;
	GetCurrentFT((FILETIME*)&ft);
	m_ftExpires = ft + (dwSecondsToLive*1000*FILETIME_TO_MILLISECONDS);
	m_dwBaseTimeout = dwSecondsToLive;
}

void CWebDavLock::DeInitLock(void) {
	MyFreeNZ(m_szOwner);
	MyFreeNZ(m_szOwnerNSTags);
}

//
//  CWebDavFileNode
//
BOOL CWebDavFileNode::InitFileNode(WCHAR *szFile, DAV_LOCK_SHARE_MODE lockShareMode, HANDLE hFile) {
	DebugCheckCacheManagerLocked();
	DEBUGCHK(IsValidLockState(lockShareMode));
	DEBUGCHK(hFile != INVALID_HANDLE_VALUE);

	if (NULL == (m_szFileName = MySzDupW(szFile)))
		return FALSE;

	m_lockShareMode  = lockShareMode;
	m_hFile          = hFile;
	m_pNext          = NULL;
	m_pLockList      = NULL;
	m_fBusy          = FALSE;
	return TRUE;
}

void CWebDavFileNode::DeInitFileNode(void) {
	DebugCheckCacheManagerLocked();
	CWebDavLock *pNext;

	while (m_pLockList) {
		pNext = m_pLockList->m_pNext;
		DeInitAndFreeLock(m_pLockList);
		m_pLockList = pNext;
	}

	MyFreeNZ(m_szFileName);
	MyCloseHandle(m_hFile);
}

BOOL CWebDavFileNode::AddLock(PSTR szOwner, CNameSpaceMap *pNSMap, __int64 iLockId, DWORD dwBaseTimeout) {
	CWebDavLock *pLock = g_pFileLockManager->AllocFixedLock();
	if (!pLock)
		return FALSE;

	if (! pLock->InitLock(szOwner,pNSMap,iLockId,dwBaseTimeout)) {
		g_pFileLockManager->FreeFixedLock(pLock);
		return FALSE;
	}

	pLock->m_pNext = m_pLockList;
	m_pLockList    = pLock;

	return TRUE;
}

void CWebDavFileNode::DeInitAndFreeLock(CWebDavLock *pLockList) {
	DebugCheckCacheManagerLocked();
	pLockList->DeInitLock();
	g_pFileLockManager->FreeFixedLock(pLockList);
}

BOOL CWebDavFileNode::DeleteLock(CWebDavLock *pLockList) {
	DebugCheckCacheManagerLocked();

	// If another thread is using lock in PUT/MOVE/COPY/... then leave it alone.
	if (IsBusy())
		return FALSE;

	CWebDavLock *pTrav   = m_pLockList;
	CWebDavLock *pFollow = NULL;

	// remove item from linked list
	while (pTrav && (pTrav != pLockList)) {
		pFollow = pTrav;
		pTrav   = pTrav->m_pNext;
	}
	DEBUGCHK(pTrav); // pLockList must be a valid node.

	if (pFollow)
		pFollow->m_pNext = pTrav->m_pNext;
	else {
		DEBUGCHK(pLockList == m_pLockList);
		m_pLockList = pLockList->m_pNext;
	}

	DeInitAndFreeLock(pLockList);
	return TRUE;
}

void CWebDavFileNode::RemoveTimedOutLocks(FILETIME *pft) {
	DebugCheckCacheManagerLocked();

	// If another thread is using lock in PUT/MOVE/COPY/... then leave it alone.
	if (IsBusy())
		return;

	CWebDavLock *pTrav   = m_pLockList;
	CWebDavLock *pFollow = NULL;

	while (pTrav) {
		CWebDavLock *pNext = pTrav->m_pNext;

		if (pTrav->HasLockExpired(pft)) {
			DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: Lock for file %s, lockID=%I64d has expired, removing\r\n",m_szFileName,pTrav->m_i64LockId));
			DeInitAndFreeLock(pTrav);

			if (pFollow)
				pFollow->m_pNext = pNext;
			else {
				DEBUGCHK(pTrav == m_pLockList);
				m_pLockList = pNext;
			}
		}
		else {
			pFollow = pTrav;
		}
		pTrav = pNext;
	}
}


BOOL CWebDavFileNode::HasLockId(__int64 iLockId, CWebDavLock **ppLockMatch) {
	DEBUGCHK(m_pLockList); // if list is NULL, *this should've been deleted.
	DebugCheckCacheManagerLocked();

	CWebDavLock *pLock = m_pLockList;

	while (pLock) {
		if (pLock->m_i64LockId == iLockId) {
			if (ppLockMatch)
				*ppLockMatch = pLock;
			return TRUE;
		}

		pLock = pLock->m_pNext;
	}
	return FALSE;
}

// A client has passed one or more LockIDs to our server.  Verify that at least one
// of them corresponds with the lockIDs associated with this file.
BOOL CWebDavFileNode::HasLockIdInArray(LockIdArray *pLockIDs, int numLockIds) {
	DebugCheckCacheManagerLocked();
	DEBUGCHK(pLockIDs && numLockIds);	

	for (int i = 0; i < numLockIds; i++) {
		if (HasLockId((*pLockIDs)[i]))
			return TRUE;
	}
	return FALSE;
}

// 
//  Utility
//
unsigned short us_rand (void) {
	unsigned short usRes = (unsigned short)rand();
	if (rand() > RAND_MAX / 2)
		usRes |= 0x8000;

	return usRes;
}

typedef int (*CeGenerateGUID_t) (GUID *);

void GenerateGUID (GUID *pGuid) {
	HMODULE hLib = LoadLibrary (L"lpcrt.dll");
	if (hLib) {
		CeGenerateGUID_t CeGenerateGUID = (CeGenerateGUID_t)GetProcAddress (hLib, L"CeGenerateGUID");
		int fRet = (CeGenerateGUID && CeGenerateGUID (pGuid) == 0);
		FreeLibrary (hLib);

		if (fRet)
			return;
	}

	srand (GetTickCount ());
	pGuid->Data1 = (us_rand () << 16) | us_rand();
	pGuid->Data2 = us_rand();
	pGuid->Data3 = us_rand();
	for (int i = 0 ; i < 8 ; i += 2) {
		unsigned short usRand = us_rand();
		pGuid->Data4[i]     = (unsigned char)(usRand & 0xff);
		pGuid->Data4[i + 1] = (unsigned char)(usRand >> 8);
	}
}

//
// CWebDavFileLockManager
//

// start with same LockID as IIS
#define DAV_INITIAL_LOCK_ID      ((50 << 32) + 50)

#if defined (DEBUG)
// Debug verification that CWebDavFileLockManager is only instantiated once 
// per web server being loaded / unloaded
int cWebDavFileLockManagerInitializations = 0;
#endif

CWebDavFileLockManager::CWebDavFileLockManager() {
#if defined (DEBUG)
	DEBUGCHK(cWebDavFileLockManagerInitializations == 0);
	cWebDavFileLockManagerInitializations++;
#endif

	m_pFileLockList = NULL;

	m_iNextLockId = DAV_INITIAL_LOCK_ID;

	GUID lockGuid;
	GenerateGUID(&lockGuid);

	sprintf(m_szLockGUID,SVSUTIL_GUID_FORMAT_A,SVSUTIL_RGUID_ELEMENTS(lockGuid));
	DEBUGCHK(strlen(m_szLockGUID) == SVSUTIL_GUID_STR_LENGTH);

	m_pfmdFileNodes = svsutil_AllocFixedMemDescr (sizeof(CWebDavFileNode), 10);
	m_pfmdLocks = svsutil_AllocFixedMemDescr (sizeof(CWebDavLock), 10);

	m_pFileLockList = NULL;
}

CWebDavFileLockManager::~CWebDavFileLockManager() {
	DEBUGCHK(g_fState == SERVICE_STATE_SHUTTING_DOWN || g_fState == SERVICE_STATE_UNLOADING);

	Lock();

	// remove everything in FileLock List
	while (m_pFileLockList) {
		CWebDavFileNode *pNext = m_pFileLockList->m_pNext;
		DeInitAndFreeFileNode(m_pFileLockList);
		m_pFileLockList = pNext;
	}

	if (m_pfmdFileNodes)
		svsutil_ReleaseFixedNonEmpty(m_pfmdFileNodes);

	if (m_pfmdLocks)
		svsutil_ReleaseFixedNonEmpty (m_pfmdLocks);

	Unlock();
}


CWebDavFileNode* CWebDavFileLockManager::GetNodeFromID(__int64 iLockId, CWebDavLock **ppLockMatch) {
	DEBUGCHK(IsLocked());
	CWebDavFileNode *pFile = m_pFileLockList;

	while (pFile) {
		if (pFile->HasLockId(iLockId,ppLockMatch))
			return pFile;

		pFile = pFile->m_pNext;
	}
	return NULL;
}

CWebDavFileNode * CWebDavFileLockManager::GetNodeFromFileName(WCHAR *szFileName) {
	DEBUGCHK(IsLocked());
	CWebDavFileNode *pTrav = m_pFileLockList;

	while (pTrav) {
		if (0 == PathNameCompare(pTrav->m_szFileName,szFileName))
			return pTrav;

		pTrav = pTrav->m_pNext;
	}
	return NULL;
}

BOOL CWebDavFileLockManager::IsFileAssociatedWithLockId(WCHAR *szFileName, __int64 iLockId) {
	BOOL fRet = FALSE;
	Lock();

	CWebDavFileNode *pFileNode = GetNodeFromFileName(szFileName);

	if (pFileNode)
		fRet = pFileNode->HasLockId(iLockId);

	Unlock();

#if defined (DEBUG)
	if (!fRet)
		DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: File <%s> is not associated with LockID %I64d\r\n",szFileName,iLockId));
#endif

	return fRet;
}

	
void CWebDavFileLockManager::RemoveTimedOutLocks(void) {
	Lock();

	FILETIME ft;
	GetCurrentFT(&ft);

	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: file lock cleanup thread woke up, ft.dwHighDateTime = 0x%08x, ft.dwLowDateTime = 0x%08x\r\n",
	                              ft.dwHighDateTime,ft.dwLowDateTime));

	CWebDavFileNode *pTrav   = m_pFileLockList;
	CWebDavFileNode *pFollow = NULL;

	while (pTrav) {
		CWebDavFileNode *pNext = pTrav->m_pNext;
		pTrav->RemoveTimedOutLocks(&ft);

		// if there are no more locks associated with file resource, remove the node.
		if (! pTrav->HasLocks()) { 
			DeInitAndFreeFileNode(pTrav);
			
			if (pFollow)
				pFollow->m_pNext = pNext;
			else {
				DEBUGCHK(pTrav == m_pFileLockList);
				m_pFileLockList = pNext;
			}
		}
		else {
			pFollow = pTrav;
		}
		pTrav = pNext;
	}

	Unlock();
}

void CWebDavFileLockManager::DeInitAndFreeFileNode(CWebDavFileNode *pFileNode) {
	DEBUGCHK(IsLocked());
	pFileNode->DeInitFileNode();
	svsutil_FreeFixed(pFileNode,m_pfmdFileNodes);
}

BOOL CWebDavFileLockManager::CreateLock(CLockParse *pLockParams, CWebDav *pWebDav) {
	CWebDavFileNode *pFileNode        = NULL;
	CWebDavLock     *pLock            = NULL;
	BOOL       fRet                   = FALSE;
	HANDLE     hFile                  = INVALID_HANDLE_VALUE;
	WCHAR      *szFile                = pWebDav->m_pRequest->m_wszPath;
	DWORD      dwTimeout              = 0;
	BOOL       fCreateFileNode        = TRUE;
	BOOL       fFileCreated           = FALSE;
	CHAR       szLockToken[256];      // response header "Lock-token: <opaquelocktoken:GUID:LockID>"
	__int64    iLockId;


	if (pLockParams->m_lockShareMode == DAV_LOCK_UNKNOWN) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: LOCK fails, client did not set <lockscope> to <exclusive> or <shared>\r\n"));
		pWebDav->SetStatus(STATUS_BADREQ);
		return FALSE;
	}

	// Get HTTP timeout header.  If timeout not set, then no problem (will use default)
	// Function will return FALSE on syntax errors
	if (!pWebDav->GetLockTimeout(&dwTimeout))
		return FALSE;

	if (dwTimeout == 0)
		dwTimeout = pWebDav->m_pRequest->m_pWebsite->m_dwDavDefaultLockTimeout; 

	Lock();

	if (NULL != (pFileNode = GetNodeFromFileName(szFile))) {
		// found the file already has a lock associated with it.
		fCreateFileNode = FALSE;

		if (pFileNode->IsExclusiveLock() || (pLockParams->m_lockShareMode == DAV_LOCK_EXCLUSIVE)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: LOCK attempted to either gain exclusive ownership of an existing lock, or shared ownership of an exclusive lock for file <%s>\r\n",szFile));
			pWebDav->SetStatusAndXMLBody(STATUS_LOCKED);
			pWebDav->SendLockTags(pFileNode);
			goto done;
		}
	}

	if (fCreateFileNode) {
		// file does not have a lock associated with it already, create a new one now.
		DEBUGCHK(!pFileNode);

		if (INVALID_HANDLE_VALUE == (hFile = CreateFile(szFile,DAV_FILE_ACCESS,DAV_SHARE_ACCESS,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: LOCK call to CreateFile(%s) failed, GLE=0x%08x\r\n",szFile,GetLastError()));
			pWebDav->SetStatus(GLEtoStatus());
			goto done;
		}
		fFileCreated = (GetLastError() != ERROR_ALREADY_EXISTS);

		if (NULL == (pFileNode = (CWebDavFileNode*) svsutil_GetFixed(m_pfmdFileNodes))) {
			pWebDav->SetStatus(STATUS_INTERNALERR);
			goto done;
		}

		if (! pFileNode->InitFileNode(szFile,pLockParams->m_lockShareMode,hFile)) {
			pWebDav->SetStatus(STATUS_INTERNALERR);
			goto done;
		}
	}

	iLockId = GetNextID();
	if (! pFileNode->AddLock(pLockParams->m_szLockOwner,pLockParams->m_NSList,iLockId,dwTimeout)) {
		pWebDav->SetStatus(STATUS_INTERNALERR);
		goto done;
	}

	// after this point no calls that can fail should be added, or if they are
	// be sure to cleanup linked list in 'done:' handeling.
	if (fCreateFileNode) {
		pFileNode->m_pNext = m_pFileLockList;
		m_pFileLockList    = pFileNode;
	}

	if (fFileCreated) {
		pWebDav->m_pRequest->m_bufRespHeaders.AppendData(cszLocation,ccLocation);
		pWebDav->m_pRequest->m_bufRespHeaders.AppendData(" ",1);
		pWebDav->BuildBaseURLFromSourceURL(TRUE);
	}

	WriteOpaqueLockToken(szLockToken,iLockId,TRUE);
	pWebDav->m_pRequest->m_bufRespHeaders.AddHeader(cszLockToken,szLockToken,FALSE);

	pWebDav->SetStatusAndXMLBody(fFileCreated ? STATUS_CREATED : STATUS_OK);
	pWebDav->SendLockTags(pFileNode);

	fRet = TRUE;
done:
	DEBUGCHK(IsLocked());

	if (!fRet) {
		if (hFile != INVALID_HANDLE_VALUE)
			CloseHandle(hFile);
		
		if (fCreateFileNode && pFileNode)
			pFileNode->DeInitFileNode();
	}
#if defined (DEBUG)
	else 
		DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: LOCK created lock for file %s, lockID=%I64d, timeout=%d seconds\r\n",szFile,iLockId,dwTimeout));
#endif

	Unlock();
	return fRet;
}


BOOL CWebDavFileLockManager::CanPerformLockedOperation(WCHAR *szFile1, WCHAR *szFile2, LockIdArray *pLockIDs, int numLocks, CWebDavFileNode **ppFileNode1, CWebDavFileNode **ppFileNode2) {
	DEBUGCHK(IsLocked());
	BOOL fRet = FALSE;

	CWebDavFileNode *pFileNode1 = NULL;
	CWebDavFileNode *pFileNode2 = NULL;

	pFileNode1 = GetNodeFromFileName(szFile1);
	if (szFile2)
		pFileNode2 = GetNodeFromFileName(szFile2);

	if (numLocks == 0) {
		if (pFileNode1 || pFileNode2) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: File %s and/or %s require a lock token for access, but none sent by client\r\n",szFile1,szFile2));
			goto done;
		}
		fRet = TRUE;
		goto done;
	}

	if (!pFileNode1 && !pFileNode2) {
		DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: CanPerformLockedOperation cannot proceed, neither %s nor %s have any locks associated with them\r\n",szFile1,szFile2));
		goto done;
	}
	
	if (pFileNode1 && !pFileNode1->HasLockIdInArray(pLockIDs,numLocks)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: File %s has lock, but none of client lockIDS match\r\n",szFile1));
		goto done;
	}

	if (pFileNode2 && !pFileNode2->HasLockIdInArray(pLockIDs,numLocks)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: File %s has lock, but none of client lockIDS match\r\n",szFile2));
		goto done;
	}

	fRet = TRUE;
done:
	if (ppFileNode1)
		*ppFileNode1 = pFileNode1;

	if (ppFileNode2)
		*ppFileNode2 = pFileNode2;

	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: Found mapping between file names (%s,%s) and Lock tokens client sent\r\n",szFile1,szFile2));
	return fRet;
}

void CWebDavFileLockManager::RemoveFileNodeFromLinkedList(CWebDavFileNode *pNode) {
	DEBUGCHK(IsLocked());

	CWebDavFileNode *pTrav    = m_pFileLockList;
	CWebDavFileNode *pFollow  = NULL;

	while (pTrav != pNode) {
		pFollow = pTrav;
		pTrav   = pTrav->m_pNext;
	}
	DEBUGCHK(pTrav); // we know it's in list

	if (pFollow)
		pFollow->m_pNext = pTrav->m_pNext;
	else {
		DEBUGCHK(pTrav == m_pFileLockList);
		m_pFileLockList = pTrav->m_pNext;
	}
}

BOOL CWebDavFileLockManager::DeleteLock(__int64 iLockId) {
	CWebDavFileNode *pFileNode;
	CWebDavLock     *pLock;
	BOOL            fRet = FALSE;

	Lock();

	if (NULL == (pFileNode = GetNodeFromID(iLockId,&pLock)))
		goto done;

	DEBUGCHK(pLock);
	if (pFileNode->DeleteLock(pLock)) {
		if (! pFileNode->HasLocks()) {
			RemoveFileNodeFromLinkedList(pFileNode);
			DeInitAndFreeFileNode(pFileNode);
		}
		fRet = TRUE;
	}

done:
	Unlock();
	return fRet;
}
