//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: davmain.cpp
Abstract: WebDAV main verb handeling routines
--*/

#include "httpd.h"

const DWORD g_fWebDavModule = TRUE;

//
// Global initialization and deinitialization routines
// 

BOOL CGlobalVariables::InitWebDav(CReg *pWebsite) {
	// WebDAV is enabled by default, but can be disabled via registry.
	if (!pWebsite->ValueDW(RV_WEBDAV,TRUE))
		return FALSE;

	if (m_fRootSite && !g_pFileLockManager)
		g_pFileLockManager = new CWebDavFileLockManager;

	if (! (g_pFileLockManager && g_pFileLockManager->IsInitialized()))
		return FALSE;

	m_dwDavDefaultLockTimeout = pWebsite->ValueDW(RV_DEFAULT_DAV_LOCK,DEFAULT_DAV_LOCK_TIMEOUT);
	m_dwDavDefaultLockTimeout = m_dwDavDefaultLockTimeout / 1000; // store number in seconds.
	return TRUE;
}

void CGlobalVariables::DeInitWebDav(void) {
	DEBUGCHK(m_fRootSite);
	DEBUGCHK(g_fState == SERVICE_STATE_UNLOADING);

	if (g_pFileLockManager)
		delete g_pFileLockManager;

	return;
}

//
// Map a verb request into appropriate DAV handler
//
typedef BOOL (WINAPI *PFN_DAVEXECUTE)(CWebDav *pThis);

BOOL DavUnsupported(CWebDav *pThis) { return pThis->DavUnsupported(); }
BOOL DavOptions(CWebDav *pThis) { return pThis->DavOptions() ; }
BOOL DavPut(CWebDav *pThis) { return pThis->DavPut() ; }
BOOL DavMove(CWebDav *pThis) { return pThis->DavMove() ; }
BOOL DavCopy(CWebDav *pThis) { return pThis->DavCopy() ; }
BOOL DavDelete(CWebDav *pThis) { return pThis->DavDelete() ; }
BOOL DavMkcol(CWebDav *pThis) { return pThis->DavMkcol() ; }
BOOL DavPropfind(CWebDav *pThis) { return pThis->DavPropfind() ; }
BOOL DavProppatch(CWebDav *pThis) { return pThis->DavProppatch() ; }
BOOL DavLock(CWebDav *pThis) { return pThis->DavLock() ; }
BOOL DavUnlock(CWebDav *pThis) { return pThis->DavUnlock() ; }

#if 0
BOOL DavSearch(CWebDav *pThis) { return pThis->DavSearch() ; }
BOOL DavSubscribe(CWebDav *pThis) { return pThis->DavUnsubscribe() ; }
BOOL DavUnsubscribe(CWebDav *pThis) { return pThis->DavUnsubscribe() ; }
BOOL DavPoll(CWebDav *pThis) { return pThis->DavPoll() ; }
BOOL DavBDelete(CWebDav *pThis) { return pThis->DavBDelete() ; }
BOOL DavBCopy(CWebDav *pThis) { return pThis->DavBCopy() ; }
BOOL DavBMove(CWebDav *pThis) { return pThis->DavBMove() ; }
BOOL DavBProppatch(CWebDav *pThis) { return pThis->DavBProppatch() ; }
BOOL DavBPropfind(CWebDav *pThis) { return pThis->DavBPropfind() ; }
BOOL DavX_MS_Enumatts(CWebDav *pThis) { return pThis->DavX_MS_Enumatts() ; }
#endif // 0

// NOTE: this follows same order as "enum VERB" in request.h
PFN_DAVEXECUTE davTable[] = {
	DavUnsupported,
	DavUnsupported,
	DavUnsupported,
	DavUnsupported,
	DavOptions,
	DavPut,
	DavMove,
	DavCopy,
	DavDelete,
	DavMkcol,
	DavPropfind,
	DavProppatch,
//	DavSearch,  // SharePoint + IIS Specific
	DavLock,
	DavUnlock,
#if 0 // unsupported DAV vebs
	DavSubscribe,
	DavUnsubscribe,
	DavPoll,
	DavBDelete,
	DavBCopy,
	DavBMove,
	DavBProppatch,
	DavBPropfind,
	DavX_MS_Enumatts,
#endif // 0 
};

// Webdav entry point web server calls into.
// Note that WebDav handles the vast majority of errors it hits with descriptive 
// XML tags and a "207 Multistatus" response.  HandleWebDav() only returns
// FALSE when it needs the web server to send a 4XX or 5XX error code to the client.
BOOL CHttpRequest::HandleWebDav(void) {
	CWebDav dav(this);

	DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: WebDAV called to handle HTTP verb <<%a>>\r\n",m_pszMethod));

	DEBUGCHK(m_idMethod <= VERB_LAST_VALID && m_idMethod != VERB_UNKNOWN);
	DEBUGCHK(m_idMethod <= ARRAYSIZEOF(davTable));
	DEBUGCHK(m_bufRespBody.m_iNextIn    == 0);
	DEBUGCHK(m_rs == STATUS_OK);

	if (dav.IsCollection() || dav.IsNull())
		RemoveTrailingSlashToPhysicalPath();

	m_fResponseHTTP11 = MAKELONG(1,1) <= m_dwVersion;

	if (m_fDisabledNagle == FALSE) {
		// To increase performance of WebDAV, disable Nagling.
		BOOL fDisable = TRUE;
		int iErr;

		if (ERROR_SUCCESS != (iErr = setsockopt(m_socket,IPPROTO_TCP,TCP_NODELAY,(const char*)&fDisable,sizeof(BOOL)))) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: setsockopt(0x%08x,TCP_NODELAY) failed, err = 0x%08x\r\n",m_socket,iErr));
			DEBUGCHK(0);
		}
		m_fDisabledNagle = TRUE;
	}

	BOOL fRet = (davTable[m_idMethod])(&dav);
	if (dav.m_fXMLBody)
		dav.FinalizeMultiStatusResponse();
	
	dav.FinalConsistencyCheck(fRet);
	DEBUGCHK(! g_pFileLockManager->IsLocked());

	DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: WebDav processing completed.  Returning %d to HTTPD, status code = %d\r\n",fRet,rgStatus[m_rs].dwStatusNumber));
	return fRet;
}

// 
//  Handle HTTP verbs
// 

//
// DavOptions
// OPTIONS requests the capabilities of the server or the available operations on a given resource.
//

const CHAR cszDavCompliance[] = "DAV:";
const CHAR cszCompliance[]    = "1, 2";

const CHAR cszCache_Control[] = "Cache-Control:";
const CHAR cszCache_Control_Private[]	= "private";
// const CHAR cszCache_Control_NoCache[] = "no-cache";


BOOL CWebDav::DavOptions(void) {
	BOOL fRet = FALSE;
	BOOL fWriteAccess = (m_pRequest->GetPerms() & HSE_URL_FLAGS_WRITE);

	// alloc big enough buffer now so we don't have to check every AddHeader() call.
	if (!m_pRequest->m_bufRespHeaders.AllocMem(HEADERBUFSIZE)) {
		SetStatus(STATUS_INTERNALERR);
		goto done;
	}

	// "*" requests options for the entire server
	if (0 == strcmp(m_pRequest->m_pszURL,"*") || 0 == strcmp(m_pRequest->m_pszURL,"/*")) {
		m_pRequest->m_bufRespHeaders.AppendData(cszAllow,ccAllow);
		WritePublicOptions();

		m_pRequest->m_bufRespHeaders.AddHeader(cszAccept_Ranges,cszBytes);
		fRet = TRUE;
		goto done; // make sure common case headers are handled
	}

	if (m_pRequest->GetPerms() & HSE_URL_FLAGS_READ) {
		if (! CheckIfHeaders()) {
			SetStatus(STATUS_BADREQ);
			fRet = FALSE;
			goto done;
		}
	}

	// It doesn't make sense to check against LOCKs in this case, as OPTIONS doesn't modify resource in any way.

	// Print out "Allow: " header
	m_pRequest->m_bufRespHeaders.AppendData(cszAllow,ccAllow);
	m_pRequest->m_bufRespHeaders.AppendCHAR(' ');
	WriteSupportedVerb(VERB_OPTIONS,TRUE);
	WriteSupportedVerb(VERB_GET);
	WriteSupportedVerb(VERB_HEAD);

	if (fWriteAccess) {
		WriteSupportedVerb(VERB_DELETE);

		if (!IsCollection())
			WriteSupportedVerb(VERB_PUT);
	}

	if (m_pRequest->IsScript(TRUE))
		WriteSupportedVerb(VERB_POST);

	if (! IsCollection())
		WriteSupportedVerb(VERB_MKCOL);

	if (! IsNull()) {
		WriteSupportedVerb(VERB_COPY);

		if (fWriteAccess) {
			WriteSupportedVerb(VERB_MOVE);
			WriteSupportedVerb(VERB_PROPPATCH);
//			m_pRequest->m_bufRespHeaders.AppendData(cszDavSearch,SVSUTIL_CONSTSTRLEN(cszDavSearch));
		}
	}

	WriteSupportedVerb(VERB_LOCK);
	WriteSupportedVerb(VERB_UNLOCK);
	m_pRequest->m_bufRespHeaders.AppendData(cszCRLF,ccCRLF);

	m_pRequest->m_bufRespHeaders.AddHeader(cszAccept_Ranges,(m_rt == RT_COLLECTION) ? cszNone : cszBytes);
	
	fRet = TRUE;
done:
	if (fRet) {
		// common headers.
//		m_pRequest->m_bufRespHeaders.AddHeader(cszDasl,cszSqlQuery);
		m_pRequest->m_bufRespHeaders.AppendData(cszPublic, ccPublic);
		WritePublicOptions();

		m_pRequest->m_bufRespHeaders.AddHeader(cszDavCompliance, cszCompliance);
		m_pRequest->m_bufRespHeaders.AddHeader(cszCache_Control, cszCache_Control_Private);

		CHttpResponse resp(m_pRequest);
		resp.SetZeroLenBody();
		resp.SendResponse();
	}

	DEBUGCHK(m_pRequest->m_bufRespHeaders.m_iNextIn < HEADERBUFSIZE); 	// make sure we didn't use more than we claimed we would...
	return fRet;
}


BOOL CWebDav::DavPropfind(void) { 
	BOOL fRet = FALSE;

	if (! (m_pRequest->GetPerms() & HSE_URL_FLAGS_READ)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: PROPFIND returns access denied because HSE_URL_FLAGS_READ is not set\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

#if 0
	if (! m_pRequest->m_dwContentLength) {
		

		if (! m_pRequest->FindHttpHeader(cszTransfer_Encoding,ccTransfer_Encoding)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: PROPFIND fails because no content-length or transfer-encoding was set\r\n"));
			SetStatus(STATUS_REQUEST_TOO_LARGE);
			goto done;
		}
	}
#endif

	if (m_rt == RT_NULL) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: PROPFIND fails because file %s does not exist\r\n",m_pRequest->m_wszPath));
		SetStatus(STATUS_NOTFOUND);
		goto done;
	}

	if (m_rt == RT_COLLECTION) {
		// depth only matters for directories
		if (! GetDepth()) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: PROPFIND fails because depth not set for directory\r\n"));
			SetStatus(STATUS_BADREQ);
			goto done;
		}
	}

	if (! CheckIfHeaders()) {
		SetStatus(STATUS_PRECONDITION_FAILED);
		goto done;
	}

	if (! AddContentLocationHeaderIfNeeded())
		goto done;

	if (! HasBody())
		fRet = SendProperties(DAV_PROP_ALL,FALSE);
	else 
		fRet = SendSubsetOfProperties();

done:
	return fRet;
}

BOOL CWebDav::DavProppatch(void) {
	BOOL fRet     = FALSE;
	BOOL fProceed = TRUE;

	CPropPatchParse cPropPatch(this);
	CWebDavFileNode *pFileNode = NULL;

	if (! (m_pRequest->GetPerms() & HSE_URL_FLAGS_WRITE)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: PROPPATCH fails, VRoot does not have HSE_URL_FLAGS_WRITE permission\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (! AddContentLocationHeaderIfNeeded())
		goto done;

	if (! CheckIfHeaders()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	if (! CheckLockHeader()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	// We need to explicitly look this file up in our lock manager cache because
	// holding a file handle to a file (even with minimal sharing) will not prevent
	// a call to SetFileAttributes on it from succeeding.

	g_pFileLockManager->Lock();
	if (! g_pFileLockManager->CanPerformLockedOperation(m_pRequest->m_wszPath,NULL,&m_lockIds,m_nLockTokens,&pFileNode,NULL)) {
		DEBUGCHK(pFileNode);
		SetStatusAndXMLBody(STATUS_LOCKED);
		SendLockTags(pFileNode);
		fProceed = FALSE;
	}
	g_pFileLockManager->Unlock();

	if (!fProceed)
		goto done;

	if (ParseXMLBody(&cPropPatch,FALSE)) {
		DEBUGCHK(m_pRequest->m_rs == STATUS_OK);
		fRet = SendPropPatchResponse(STATUS_OK);
	}
	
done:
	return fRet;
}


BOOL CWebDav::DavMkcol(void) { 
	BOOL fRet = FALSE;

	if (! (m_pRequest->GetPerms() & HSE_URL_FLAGS_WRITE)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MKCOL fails, VRoot does not have HSE_URL_FLAGS_WRITE permission\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (m_pRequest->m_dwContentLength || m_pRequest->FindHttpHeader(cszContent_Type,ccContent_Type)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MKCOL fails, client has sent body, server doesn't support body\r\n"));
		SetStatus(STATUS_UNSUPPORTED_MEDIA_TYPE);
		goto done;
	}

	if (! CheckIfHeaders()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	if (! CheckLockHeader()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	if (IsSrcFileNameLenTooLong()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MKCOl fails, request URI is too long\r\n")); 
		SetStatus(STATUS_REQUEST_TOO_LARGE);
		goto done;
	}

	if (! CreateDirectoryW(m_pRequest->m_wszPath,NULL)) {
		DWORD dwErr = GetLastError();
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MKCOL fails, CreateDirectoryW fails, GLE=0x%08x\r\n",dwErr));

		if ((dwErr == ERROR_FILE_EXISTS) || (dwErr == ERROR_ALREADY_EXISTS))
			SetStatus(STATUS_METHODNOTALLOWED);
		else if (dwErr == ERROR_PATH_NOT_FOUND)
			SetStatus(STATUS_CONFLICT);
		else
			SetStatus(STATUS_INTERNALERR);
	}
	else {
		AddContentLocationHeader();

		CHttpResponse resp(m_pRequest,STATUS_CREATED);
		resp.SetZeroLenBody();
		resp.SendResponse();
		fRet = TRUE;
	}
	
done:
	return fRet;
}

BOOL CWebDav::DavPut(void) { 
	BOOL   fRet = FALSE;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	BOOL   fFileExists;
	BOOL   fLockedFile = FALSE;

	if (! (m_pRequest->GetPerms() & HSE_URL_FLAGS_WRITE)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: PUT fails, VRoot does not have HSE_URL_FLAGS_WRITE permission\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (m_pRequest->FindHttpHeader(cszContent_Range,ccContent_Range)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: PUT fails, CE DAV does not support \"Content-Range\" setting\r\n"));
		SetStatus(STATUS_NOTIMPLEM);
		goto done;
	}

#if 0
	if (! m_pRequest->m_dwContentLength) { // NOTE: XP redirector frequently sends 0 length files, so this check is not safe.
		// no chunked-encoding currently
		if (! m_pRequest->FindHttpHeader(cszTransfer_Encoding,ccTransfer_Encoding)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: PUT fails because no content-length or transfer-encoding was set\r\n"));
			SetStatus(STATUS_LENGTH_REQUIRED);
			goto done;
		}
	}
#endif // 0

	if (IsCollection()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: PUT fails, attempting to write to a directory\r\n"));
		SetStatus(STATUS_CONFLICT);
		goto done;
	}

	if (IsSrcFileNameLenTooLong()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: PUT fails, request URI is too long\r\n")); 
		SetStatus(STATUS_REQUEST_TOO_LARGE);
		goto done;
	}

	if (! CheckIfHeaders()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	if (! CheckLockHeader()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	if (INVALID_HANDLE_VALUE == (hFile = MyWriteFile(m_pRequest->m_wszPath))) {
		if (CheckLockTokens()) {
			// Does file have a lock and did client send lock tokens to access it?
			CWebDavFileNode *pFileNode;

			g_pFileLockManager->Lock();

			if (g_pFileLockManager->CanPerformLockedOperation(m_pRequest->m_wszPath,NULL,&m_lockIds,m_nLockTokens,&pFileNode,NULL)) {
				DEBUGCHK(g_pFileLockManager->IsLocked());

				hFile = pFileNode->m_hFile;
				SetFilePointer(hFile,0,NULL,FILE_BEGIN);

				fLockedFile = TRUE;
			}

			g_pFileLockManager->Unlock();
		}
	}

	if (INVALID_HANDLE_VALUE == hFile) {
		DWORD dwErr = GetLastError();
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: PUT fails, unable to open file %s, GLE=0x%08x\r\n",m_pRequest->m_wszPath,dwErr));

		SendLockedConflictOrErr(m_pRequest->m_wszPath);	
		goto done;
	}

	fFileExists = (GetLastError() == ERROR_ALREADY_EXISTS);

	if (WriteBodyToFile(hFile,FALSE)) {		
		SetEndOfFile(hFile); // In event we're writing more data to file than existed previously.
	
		CHttpResponse resp(m_pRequest,fFileExists ? STATUS_OK : STATUS_CREATED);
		resp.SetBody(hFile,NULL,0);
		resp.SetZeroLenBody();
		resp.UnSetMimeType();
		resp.SendResponse();
		fRet = TRUE;
	}
	else {
		SetStatus(STATUS_INTERNALERR);
	}

done:
	// Close file if its a valid handle, but only if it's not a previously locked file.
	if ((hFile != INVALID_HANDLE_VALUE) && !fLockedFile)
		CloseHandle(hFile);

	return fRet;
}


BOOL CWebDav::DavDelete(void) { 
	BOOL   fRet = FALSE;

	if (! (m_pRequest->GetPerms() & HSE_URL_FLAGS_WRITE)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: DELETE fails, VRoot does not have HSE_URL_FLAGS_WRITE permission\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (IsNull()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: DELETE fails, file %s does not exist\r\n",m_pRequest->m_wszPath));
		SetStatus(STATUS_NOTFOUND);
		goto done;
	}

	if (! AddContentLocationHeaderIfNeeded())
		goto done;

	if (! CheckIfHeaders()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	if (! CheckLockHeader()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	if (IsCollection()) {
		fRet = DeleteCollection(m_pRequest->m_wszPath,m_pRequest->m_pszURL);
	}
	else { // single file
		if (!DavDeleteFile(m_pRequest->m_wszPath)) {
			// If we fail because we're locked, send this info to the client
			if (! SendLockErrorInfoIfNeeded(m_pRequest->m_wszPath))
				SetStatus(GLEtoStatus());
		}
		else
			fRet = TRUE;
	}
	
done:
	if (fRet) {
		CHttpResponse resp(m_pRequest,STATUS_NOCONTENT);
		resp.SetZeroLenBody();
		resp.SendResponse();
		m_fSetStatus = TRUE;
	}

	// Possible for us to have returned multistatus contained detailed info, 
	// in which case HTTPD shouldn't put err messages on it.
	return IS_STATUS_2XX(m_pRequest->m_rs); 
}

// Unimplemented
BOOL CWebDav::DavUnsupported(void) { 
	SetStatus(STATUS_NOTIMPLEM);
	return FALSE; 
}

BOOL CWebDav::MoveCopyResource(BOOL fDeleteSrc) {
	DWORD dwAccess = fDeleteSrc ? (HSE_URL_FLAGS_READ | HSE_URL_FLAGS_WRITE) : HSE_URL_FLAGS_READ;
	BOOL  fRet = FALSE;

	// Pointer to original URL sent in HTTP headers, and ptr to '\0' to set to '\r'
	PSTR  szDestPrefix     = NULL;
	PSTR  szDestURL        = NULL;
	PSTR  szSave           = NULL;
	WCHAR *wszDestPath     = NULL;

	BOOL        fDestExists  = TRUE;
	PVROOTINFO  pVRoot; 	// Use destinations VROOT and permission (not m_pRequest's necessarily) to determine what we can do.

	WIN32_FILE_ATTRIBUTE_DATA  wDestAttribs;


	if (! (m_pRequest->GetPerms() & dwAccess)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, action is forbidden\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	GetOverwrite();

	// Depth for MOVE must be infinite.  For copy it may be 0 or infinite.
	if (!GetDepth() || (fDeleteSrc && (m_Depth != DEPTH_INFINITY)) ||
	    (!fDeleteSrc && (m_Depth != DEPTH_INFINITY && m_Depth != DEPTH_ZERO))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, depth header is corrupt or invalid for verb requested\r\n"));
		SetStatus(STATUS_BADREQ);
		goto done;
	}

	if (! CheckIfHeaders()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	if (! CheckLockHeader()) {
		DEBUGCHK(m_pRequest->m_rs != STATUS_OK);
		goto done;
	}

	if (NULL == (szDestURL = GetDestinationHeader(&szDestPrefix,&szDestURL,&szSave))) {
		DEBUGCHK(! IS_STATUS_2XX(m_pRequest->m_rs));
		goto done;
	}

	if (IsNull()) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, resource to move (%s) does not exist\r\n",m_pRequest->m_wszPath));
		SetStatus(STATUS_NOTFOUND);
		goto done;
	}

	wszDestPath = m_pRequest->m_pWebsite->m_pVroots->URLAtoPathW(szDestURL,&pVRoot);

	if (IsSrcFileNameLenTooLong() || IsFileNameTooLongForFilesys(wszDestPath)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: move/copy fails, source or dest file name too long for filesys\r\n")); 
		SetStatus(STATUS_REQUEST_TOO_LARGE);
		goto done;
	}

	if (!HasAccessToDestVRoot(szDestURL,wszDestPath,pVRoot)) {
		SetStatus(STATUS_UNAUTHORIZED);
		goto done;
	}

	if ((*wszDestPath == '/' || *wszDestPath == '\\') && (*(wszDestPath+1) == 0)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, request set destination as root of filesystem\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (! GetFileAttributesEx(wszDestPath,GetFileExInfoStandard,&wDestAttribs)) {
		if (GetLastError() == ERROR_ACCESS_DENIED) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, GLE=0x%08x\r\n"));
			SetStatus(STATUS_FORBIDDEN);
			goto done;
		}
		fDestExists = FALSE;
	}

	if (!fDestExists) {
		if (!m_pRequest->m_bufRespHeaders.AddHeader(cszLocation,szDestPrefix))
			goto done;
	}

	if (fDestExists && IsDirectory(&wDestAttribs) && 
	    !(pVRoot->dwPermissions & HSE_URL_FLAGS_READ)) {

		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, destination directory does not have read permissions enabled\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	if (IsPathSubDir(m_pRequest->m_wszPath,m_pRequest->m_ccWPath,wszDestPath,wcslen(wszDestPath))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, destination or source url is subset of the other path, not allowed\r\n"));
		SetStatus(STATUS_FORBIDDEN);
		goto done;
	}

	// Only add location header on copy because on move, src will be deleted on success.
	if (!fDeleteSrc && !AddContentLocationHeaderIfNeeded())
		goto done;

	if ((! (m_Overwrite & OVERWRITE_YES)) && fDestExists) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, destination exists but overwrite option is turned off\r\n"));
		SetStatus(STATUS_PRECONDITION_FAILED);
		goto done;
	}

	// delete the destination before copying, per spec.
	if ((m_Overwrite & OVERWRITE_YES) && fDestExists)  {
		if (IsDirectory(&wDestAttribs)) {
			if (! DeleteCollection(wszDestPath,szDestURL)) {
				fRet = (m_pRequest->m_rs == STATUS_MULTISTATUS);
				goto done;
			}
		}
		else { // Single file
			if (!DavDeleteFile(wszDestPath)) {
				SendLockedConflictOrErr(wszDestPath);
				goto done;
			}
		}
	}

	if (IsCollection()) {
		DEBUGCHK(! URLHasTrailingSlashW(m_pRequest->m_wszPath));

		CMoveCopyContext cMoveContext(fDeleteSrc,m_pRequest->GetPerms(),pVRoot->dwPermissions,szDestURL,wszDestPath,m_pRequest->m_ccWPath);
		fRet = MoveCopyCollection(&cMoveContext);
	}
	else {
		if (FALSE == MoveCopyFile(fDeleteSrc,m_pRequest->m_wszPath,wszDestPath)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WebDav move or copy fails, GLE = 0x%08x\r\n"));
			SendLockedConflictOrErr(wszDestPath,m_pRequest->m_wszPath);
			fRet = (m_pRequest->m_rs == STATUS_MULTISTATUS);
			goto done;
		}
		fRet = TRUE;
	}

done:
	if (fRet && (m_pRequest->m_rs != STATUS_MULTISTATUS)) {
		DEBUGCHK(m_pRequest->m_rs == STATUS_OK);

		CHttpResponse resp(m_pRequest,fDestExists ? STATUS_NOCONTENT : STATUS_CREATED);
		resp.SetZeroLenBody();
		resp.SendResponse();
	}

	if (szSave) { 
		*szSave = '\r';
		m_pRequest->DebugSetHeadersInvalid(FALSE);
	}

	MyFreeNZ(szDestURL);
	MyFreeNZ(wszDestPath);

	return fRet;
}

BOOL CWebDav::DavMove(void) {
	return MoveCopyResource(TRUE);
}

BOOL CWebDav::DavCopy(void) {
	return MoveCopyResource(FALSE); 
}

#if 0
BOOL CWebDav::DavSearch(void) { return DavUnsupported(); }
BOOL CWebDav::DavSubscribe(void) { return FALSE; }
BOOL CWebDav::DavUnsubscribe(void) { return FALSE; }
BOOL CWebDav::DavPoll(void) { return FALSE; }
BOOL CWebDav::DavBDelete(void) { return FALSE; }
BOOL CWebDav::DavBCopy(void) { return FALSE; }
BOOL CWebDav::DavBMove(void) { return FALSE; }
BOOL CWebDav::DavBProppatch(void) { return FALSE; }
BOOL CWebDav::DavBPropfind(void) { return FALSE; }
BOOL CWebDav::DavX_MS_Enumatts(void) { return FALSE; }
#endif

// Constant strings relating to WebDAV

// DAV XML strings
const CHAR  cszXMLVersionA[]             = "<?xml version=\"1.0\" ?>";
const DWORD ccXMLVersionA                = SVSUTIL_CONSTSTRLEN(cszXMLVersionA);
const CHAR  cszMultiStatusNS[]           = "multistatus xmlns:a=\"DAV:\" xmlns:b=\"urn:uuid:c2f41010-65b3-11d1-a29f-00aa00c14882/\" xmlns:c=\"xml:\" xmlns:d=\"urn:schemas-microsoft-com:\"";
const DWORD ccMultiStatusNS              = SVSUTIL_CONSTSTRLEN(cszMultiStatusNS);
const CHAR  cszMultiStatus[]             = "multistatus";
const DWORD ccMultiStatus                = SVSUTIL_CONSTSTRLEN(cszMultiStatus);
const CHAR  cszResponse[]                = "response";
const DWORD ccResponse                   = SVSUTIL_CONSTSTRLEN(cszResponse);
const CHAR  cszHref[]                    = "href";
const DWORD ccHref                       = SVSUTIL_CONSTSTRLEN(cszHref);
const CHAR  cszPropstat[]                = "propstat";
const DWORD ccPropstat                   = SVSUTIL_CONSTSTRLEN(cszPropstat);
const CHAR  cszProp[]                    = "prop";
const DWORD ccProp                       = SVSUTIL_CONSTSTRLEN(cszProp);
const CHAR  cszPropNS[]                  = "prop xmlns:a=\"DAV:\"";
const DWORD ccPropNS                     = SVSUTIL_CONSTSTRLEN(cszPropNS);
const CHAR  cszStatus[]                  = "status";
const DWORD ccStatus                     = SVSUTIL_CONSTSTRLEN(cszStatus);
const CHAR  cszGetcontentLengthNS[]      = "getcontentlength b:dt=\"int\"";
const DWORD ccGetcontentLengthNS         = SVSUTIL_CONSTSTRLEN(cszGetcontentLengthNS);
const CHAR  cszGetcontentLength[]        = "getcontentlength";
const DWORD ccGetcontentLength           = SVSUTIL_CONSTSTRLEN(cszGetcontentLength);

const CHAR  cszDisplayName[]             = "displayname";
const DWORD ccDisplayName                = SVSUTIL_CONSTSTRLEN(cszDisplayName);
const CHAR  cszCreationDateNS[]          = "creationdate b:dt=\"dateTime.tz\"";
const DWORD ccCreationDateNS             = SVSUTIL_CONSTSTRLEN(cszCreationDateNS);
const CHAR  cszCreationDate[]            = "creationdate";
const DWORD ccCreationDate               = SVSUTIL_CONSTSTRLEN(cszCreationDate);
const CHAR  cszGetETag[]                 = "getetag";
const DWORD ccGetETag                    = SVSUTIL_CONSTSTRLEN(cszGetETag);
const CHAR  cszLastModifiedNS[]          = "getlastmodified b:dt=\"dateTime.rfc1123\"";
const DWORD ccLastModifiedNS             = SVSUTIL_CONSTSTRLEN(cszLastModifiedNS);
const CHAR  cszLastModified[]            = "getlastmodified";
const DWORD ccLastModified               = SVSUTIL_CONSTSTRLEN(cszLastModified);
const CHAR  cszResourceType[]            = "resourcetype";
const DWORD ccResourceType               = SVSUTIL_CONSTSTRLEN(cszResourceType);
const CHAR  cszCollection[]              = "collection";
const DWORD ccCollection                 = SVSUTIL_CONSTSTRLEN(cszCollection);
const CHAR  cszIsHiddenNS[]              = "ishidden b:dt=\"boolean\"";
const DWORD ccIsHiddenNS                 = SVSUTIL_CONSTSTRLEN(cszIsHiddenNS);
const CHAR  cszIsHidden[]                = "ishidden";
const DWORD ccIsHidden                   = SVSUTIL_CONSTSTRLEN(cszIsHidden);
const CHAR  cszIsCollectionNS[]          = "iscollection b:dt=\"boolean\"";
const DWORD ccIsCollectionNS             = SVSUTIL_CONSTSTRLEN(cszIsCollectionNS);
const CHAR  cszIsCollection[]            = "iscollection";
const DWORD ccIsCollection               = SVSUTIL_CONSTSTRLEN(cszIsCollection);
const CHAR  cszSupportedLock[]           = "supportedlock";
const DWORD ccSupportedLock              = SVSUTIL_CONSTSTRLEN(cszSupportedLock);
const CHAR  cszLockEntry[]               = "lockentry";
const DWORD ccLockEntry                  = SVSUTIL_CONSTSTRLEN(cszLockEntry);
const CHAR  cszTextXML[]                 = "text/xml";
const DWORD ccTextXML                    = SVSUTIL_CONSTSTRLEN(cszTextXML);
const CHAR  cszApplicationXML[]          = "application/xml";
const DWORD ccApplicationXML             = SVSUTIL_CONSTSTRLEN(cszApplicationXML);
const CHAR  cszGetcontentType[]          = "getcontenttype";
const DWORD ccGetcontentType             = SVSUTIL_CONSTSTRLEN(cszGetcontentType);

// Win32 specific proptags.  We only currently support setting Win32FileAttributes.
const CHAR  cszMSNamespace[]             = "urn:schemas-microsoft-com:";
const DWORD ccMSNamespace                = SVSUTIL_CONSTSTRLEN(cszMSNamespace);

const CHAR  cszWin32FileAttribs[]        = "Win32FileAttributes";
const DWORD ccWin32FileAttribs           = SVSUTIL_CONSTSTRLEN(cszWin32FileAttribs);
const CHAR  cszWin32LastAccessTime[]     = "Win32LastAccessTime";
const DWORD ccWin32LastAccessTime        = SVSUTIL_CONSTSTRLEN(cszWin32LastAccessTime);
const CHAR  cszWin32LastModifiedTime[]   = "Win32LastModifiedTime";
const DWORD ccWin32LastModifiedTime      = SVSUTIL_CONSTSTRLEN(cszWin32LastModifiedTime);
const CHAR  cszWin32CreationTime[]       = "Win32CreationTime";
const DWORD ccWin32CreationTime          = SVSUTIL_CONSTSTRLEN(cszWin32CreationTime);


// LOCK XML strings
const CHAR  cszLockDiscovery[]           = "lockdiscovery";
const DWORD ccLockDiscovery              = SVSUTIL_CONSTSTRLEN(cszLockDiscovery);
const CHAR  cszActiveLock[]              = "activelock";
const DWORD ccActiveLock                 = SVSUTIL_CONSTSTRLEN(cszActiveLock);
const CHAR  cszLockType[]                = "locktype";
const DWORD ccLockType                   = SVSUTIL_CONSTSTRLEN(cszLockType);
const CHAR  cszWrite[]                   = "write";
const DWORD ccWrite                      = SVSUTIL_CONSTSTRLEN(cszWrite);
const CHAR  cszLockScope[]               = "lockscope";
const DWORD ccLockScope                  = SVSUTIL_CONSTSTRLEN(cszLockScope);
const CHAR  cszShared[]                  = "shared";
const DWORD ccShared                     = SVSUTIL_CONSTSTRLEN(cszShared);
const CHAR  cszExclusive[]               = "exclusive";
const DWORD ccExclusive                  = SVSUTIL_CONSTSTRLEN(cszExclusive);
const CHAR  cszLockTokenTag[]            = "locktoken";
const DWORD ccLockTokenTag               = SVSUTIL_CONSTSTRLEN(cszLockTokenTag);
const CHAR  cszOwner[]                   = "owner";
const DWORD ccOwner                      = SVSUTIL_CONSTSTRLEN(cszOwner);
const CHAR  cszTimeoutTag[]              = "timeout";
const DWORD ccTimeoutTag                 = SVSUTIL_CONSTSTRLEN(cszTimeoutTag);
const CHAR  cszDepthTag[]                = "depth";
const DWORD ccDepthTag                   = SVSUTIL_CONSTSTRLEN(cszDepthTag);

const CHAR  cszSecond[]                  = "Second-";
const DWORD ccSecond                     = SVSUTIL_CONSTSTRLEN(cszSecond);
const CHAR  cszInfinite[]                = "Infinite";
const DWORD ccInfinite                   = SVSUTIL_CONSTSTRLEN(cszInfinite);

const CHAR  cszNot[]                     = "not";
const DWORD ccNot                        = SVSUTIL_CONSTSTRLEN(cszNot); 

// DAV header data
const CHAR  csz0[]                       = "0";
const DWORD cc0                          = SVSUTIL_CONSTSTRLEN(csz0);
const CHAR  csz1[]                       = "1";
const DWORD cc1                          = SVSUTIL_CONSTSTRLEN(csz1);
const CHAR  cszInfinity[]                = "infinity";
const DWORD ccInfinity                   = SVSUTIL_CONSTSTRLEN(cszInfinity);
const CHAR  csz1NoRoot[]                 = "1, noroot";
const DWORD cc1NoRoot                    = SVSUTIL_CONSTSTRLEN(csz1NoRoot);
// const CHAR cszInfinityNoRoot      = "infinity,noroot";


const char  cszPrefixBraceOpen[]         = "<a:";
const DWORD ccPrefixBraceOpen            = SVSUTIL_CONSTSTRLEN(cszPrefixBraceOpen);
const char  cszPrefixBraceEnd[]          = "</a:";
const DWORD ccPrefixBraceEnd             = SVSUTIL_CONSTSTRLEN(cszPrefixBraceEnd);
