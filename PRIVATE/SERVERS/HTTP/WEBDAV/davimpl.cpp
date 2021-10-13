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
Abstract: WebDAV implementation
--*/

#include "httpd.h"


//
// OPTIONS helper functions
//


const char cszOptionVerbDelimiter[] = ", ";

// During call to OPTIONS, we list off verbs we support in the form ", VERBNAME1, VERBNAME2,..."
void CWebDav::WriteSupportedVerb(VERB v, BOOL fFirstVerbInList) {
	DEBUGCHK(m_pRequest->m_idMethod == VERB_OPTIONS);

	if (fFirstVerbInList)
		m_pRequest->m_bufRespHeaders.AppendCHAR(' '); // put space between HTTP header and data.
	else
		m_pRequest->m_bufRespHeaders.AppendData(cszOptionVerbDelimiter,SVSUTIL_CONSTSTRLEN(cszOptionVerbDelimiter));

	m_pRequest->m_bufRespHeaders.AppendData(rg_cszMethods[v-1],strlen(rg_cszMethods[v-1]));
}

void CWebDav::WritePublicOptions(void) {
	WriteSupportedVerb(VERB_OPTIONS,TRUE);
	WriteSupportedVerb(VERB_GET);
	WriteSupportedVerb(VERB_HEAD);
	WriteSupportedVerb(VERB_DELETE);
	WriteSupportedVerb(VERB_PUT);
	WriteSupportedVerb(VERB_POST);
	WriteSupportedVerb(VERB_COPY);
	WriteSupportedVerb(VERB_MOVE);
	WriteSupportedVerb(VERB_MKCOL);
	WriteSupportedVerb(VERB_PROPFIND);
	WriteSupportedVerb(VERB_PROPPATCH);
	WriteSupportedVerb(VERB_LOCK);
	WriteSupportedVerb(VERB_UNLOCK);
	m_pRequest->m_bufRespHeaders.AppendData(cszCRLF,ccCRLF);
}

//
//  PROPFIND implementation
//

const CHAR cszIso8601FMT[]   = "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ";

#define DEBUGMSG_VISIT_FCN(fcnName) \
	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: %a file=%s, dwProperties=0x%08x, szTraversalPath=%s, fRootDir=%d\r\n", \
	                                fcnName,(pFindData)->cFileName,dwContext,szTraversalPath,fRootDir));

// Send back an XML <response> block for given file or directory.
// if fIsRoot=TRUE, then we're returning info on requested URL.
// if fIsRoot=FALSE, we're searching sending info on one of its children. 
BOOL CWebDav::SendPropertiesVisitFcn(WIN32_FIND_DATA *pFindData, DWORD dwContext, WCHAR *szTraversalPath, BOOL fRootDir) {
	CHAR szBuf[MINBUFSIZE]; // temp write buffer
	SYSTEMTIME st;
	BOOL fRet    = FALSE;
	BOOL fIsRoot = (NULL==szTraversalPath);
	BOOL fIsDirectory = IsDirectory(pFindData->dwFileAttributes) ? TRUE : FALSE;
	DWORD dwProperties = dwContext;

	DEBUGMSG_VISIT_FCN("SendPropertiesVisitFcn");
	DEBUGCHK(m_pRequest->m_rs == STATUS_MULTISTATUS);

	if (! dwProperties && !m_bufUnknownTags.pBuffer) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: SendPropertiesVisitFcn() returns TRUE, no properties (known or unknown) to send to client\r\n"));
		return TRUE;
	}

	// <a:response>
	if (! m_bufResp.StartTag(cszResponse))
		goto done;

	// <a:href>http://CEDevice/fileName/...</a:href>
	if (! SetHREFTagFromDestinationPath(pFindData,szTraversalPath,fRootDir,NULL,fIsRoot))
		goto done;

	// 
	// <a:prop>
	//
	if (dwProperties) { // only put <prop> if we have known properties
		// <a:propstat>
		if (! m_bufResp.StartTag(cszPropstat))
			goto done;

		// <a:status>	
		if (! m_bufResp.SetStatusTag(STATUS_OK))
			goto done;

		if (! m_bufResp.StartTag(cszProp))
			goto done;
	}

	// <getcontentlength>
	if (dwProperties & DAV_PROP_CONTENT_LENGTH) {
		_itoa(pFindData->nFileSizeLow,szBuf,10);

		if (! m_bufResp.StartTagNoEncode(cszGetcontentLengthNS,ccGetcontentLengthNS) ||  
		    ! m_bufResp.Append(szBuf,strlen(szBuf))     ||
		    ! m_bufResp.EndTag(cszGetcontentLength))
			goto done;
	}

	// <creationdate>
	if (dwProperties & DAV_PROP_CREATION_DATE) {
		FileTimeToSystemTime(&pFindData->ftCreationTime,&st);
		DWORD cc = sprintf(szBuf,cszIso8601FMT,st.wYear,st.wMonth,st.wDay,st.wHour,
		                            st.wMinute,st.wSecond,st.wMilliseconds);

		if (! m_bufResp.StartTagNoEncode(cszCreationDateNS,ccCreationDateNS) ||  
		    ! m_bufResp.Append(szBuf,cc)            ||
		    ! m_bufResp.EndTag(cszCreationDate))
			goto done;
	}

	// <displayname>
	if (dwProperties & DAV_PROP_DISPLAY_NAME) {
		PSTR szDisplay;
		CHAR cSave = 0;
		PSTR pszSave = NULL;
		BOOL fContinue = TRUE;
		
		if (fIsRoot) {
			// The display name is simply the file name by itself, no previous path,
			// just as in the non-root case.  However we have to do more work to 
			// rip out interesting portion.
			szDisplay = m_pRequest->m_pszURL;
			GetEndOfFileWithNoSlashes(&szDisplay,&pszSave,&cSave);
		}
		else {
			MyW2UTF8(pFindData->cFileName,szBuf,sizeof(szBuf));
			szDisplay = szBuf;
		}

		DEBUGCHK(strlen(szBuf) < sizeof(szBuf)); // sizeof(szBuf) > MAX_PATH so should always be safe.
	
		if (! m_bufResp.StartTag(cszDisplayName) ||
		    ! m_bufResp.Encode(szDisplay) ||
		    ! m_bufResp.EndTag(cszDisplayName))
			fContinue = FALSE;

		if (pszSave)
			*pszSave = cSave;

		if (!fContinue)
			goto done;
	}

	// <getetag>
	if (dwProperties & DAV_PROP_GET_ETAG) {
		DWORD cc = GetETagFromFiletime(&pFindData->ftLastWriteTime,pFindData->dwFileAttributes,szBuf);

		if (! m_bufResp.StartTag(cszGetETag) ||  
		    ! m_bufResp.Append(szBuf,cc)     ||
		    ! m_bufResp.EndTag(cszGetETag))
			goto done;
	}

	// <getlastmodified>
	if (dwProperties & DAV_PROP_GET_LAST_MODIFIED) {
		FileTimeToSystemTime(&pFindData->ftCreationTime,&st);
		DWORD cc = WriteDateGMT(szBuf,&st);

		if (! m_bufResp.StartTagNoEncode(cszLastModifiedNS,ccLastModifiedNS) ||
		    ! m_bufResp.Append(szBuf,cc)            ||
		    ! m_bufResp.EndTag(cszLastModified))
			goto done;
	}

	// <resourcetype>
	if (dwProperties & DAV_PROP_RESOURCE_TYPE) {
		if (fIsDirectory) {
			if (! m_bufResp.StartTag(cszResourceType)      ||
			    ! m_bufResp.SetEmptyElement(cszCollection) ||
			    ! m_bufResp.EndTag(cszResourceType)) {
				goto done;
			}
		}
		else {
			if (! m_bufResp.SetEmptyElement(cszResourceType))
				goto done;
		}
	}

	// WebDAV doesn't support locks on directories
	if ((dwProperties & DAV_PROP_SUPPORTED_LOCK)) {
		if (fIsDirectory) {
			if (! m_bufResp.SetEmptyElement(cszSupportedLock))
				goto done;
		}
		else {
			const CHAR *cszSupportedLocks[] = { cszExclusive, cszShared} ;

			if (! m_bufResp.StartTag(cszSupportedLock))
				goto done;

			for (int i = 0; i < 2; i++) {
				if (! m_bufResp.StartTag(cszLockEntry)                ||
				    ! m_bufResp.StartTag(cszLockScope)                ||
				    ! m_bufResp.SetEmptyElement(cszSupportedLocks[i]) ||
				    ! m_bufResp.EndTag(cszLockScope)                  ||
				    ! m_bufResp.StartTag(cszLockType)                 ||
				    ! m_bufResp.SetEmptyElement(cszWrite)             ||
				    ! m_bufResp.EndTag(cszLockType)                   ||
				    ! m_bufResp.EndTag(cszLockEntry))
				{
					goto done;
				}
			}

			if (! m_bufResp.EndTag(cszSupportedLock))
				goto done;
		}
	}

	if ((dwProperties & DAV_PROP_LOCK_DISCOVERY) && !fIsDirectory) {
		BOOL fSendEmptyLockDiscovery = TRUE;
		WCHAR wszFile[MAX_PATH*2];
		WCHAR *szLockFile;

		if (!fIsDirectory) {
			if (fIsRoot)
				szLockFile = m_pRequest->m_wszPath;
			else {
				BuildFileName(wszFile,szTraversalPath,pFindData->cFileName);
				szLockFile = wszFile;
			}

			g_pFileLockManager->Lock();
			CWebDavFileNode *pFileNode = g_pFileLockManager->GetNodeFromFileName(szLockFile);

			if (pFileNode) {
				SendLockTags(pFileNode,FALSE);
				fSendEmptyLockDiscovery = FALSE;
			}
			g_pFileLockManager->Unlock();
		}

		if (fSendEmptyLockDiscovery)
			m_bufResp.SetEmptyElement(cszLockDiscovery);		
	}

	// <ishidden>
	if (dwProperties & DAV_PROP_IS_HIDDEN) {
		_itoa((pFindData->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? 1 : 0,szBuf,10);
		DEBUGCHK(strlen(szBuf) == 1);
		
		if (! m_bufResp.StartTagNoEncode(cszIsHiddenNS,ccIsHiddenNS) ||
		    ! m_bufResp.Append(szBuf,1)         ||
		    ! m_bufResp.EndTag(cszIsHidden))
			goto done;
	}

	// <iscollection>
	if (dwProperties & DAV_PROP_IS_COLLECTION) {
		_itoa(fIsDirectory,szBuf,10);
		DEBUGCHK(strlen(szBuf) == 1);
		
		if (! m_bufResp.StartTagNoEncode(cszIsCollectionNS,ccIsCollectionNS) ||
		    ! m_bufResp.Append(szBuf,1)         ||
		    ! m_bufResp.EndTag(cszIsCollection))
			goto done;
	}

	if (dwProperties & DAV_PROP_GET_CONTENT_TYPE) {
		if (fIsDirectory) {
			if (! m_bufResp.SetEmptyElement(cszGetcontentType))
				goto done;
		}
		else {
			WCHAR *szExtension = wcsrchr(pFindData->cFileName, L'.');
		
			if (szExtension) {
				m_pRequest->GetContentTypeOfRequestURI(szBuf,sizeof(szBuf),szExtension);
				
				if (! m_bufResp.StartTag(cszGetcontentType) ||
				    ! m_bufResp.Encode(szBuf)               ||
				    ! m_bufResp.EndTag(cszGetcontentType)) {
					goto done;
				}
			}
		}
	}

	if (dwProperties & DAV_PROP_W32_FILE_ATTRIBUTES) {
		sprintf(szBuf,"%08x",pFindData->dwFileAttributes);
		if (! SetTagWin32NS(&m_bufResp,cszWin32FileAttribs,ccWin32FileAttribs,szBuf))
			goto done;
	}

	if (dwProperties & DAV_PROP_W32_CREATION_TIME) {
		WriteDateGMT(szBuf,&pFindData->ftCreationTime);
		if (! SetTagWin32NS(&m_bufResp,cszWin32CreationTime,ccWin32CreationTime,szBuf))
			goto done;
	}

	if (dwProperties & DAV_PROP_W32_LAST_ACCESS_TIME) {
		WriteDateGMT(szBuf,&pFindData->ftLastAccessTime);
		if (! SetTagWin32NS(&m_bufResp,cszWin32LastAccessTime,ccWin32LastAccessTime,szBuf))
			goto done;
	}

	if (dwProperties & DAV_PROP_W32_LAST_MODIFY_TIME) {
		WriteDateGMT(szBuf,&pFindData->ftLastWriteTime);
		if (! SetTagWin32NS(&m_bufResp,cszWin32LastModifiedTime,ccWin32LastModifiedTime,szBuf))
			goto done;
	}

	// close up XML tags
	if (dwProperties) {
		if (! m_bufResp.EndTag(cszProp)  || 
		    ! m_bufResp.EndTag(cszPropstat))
			goto done;
	}

	if (! SendUnknownXMLElementsIfNeeded(STATUS_NOTFOUND))
		goto done;
	
	if (! m_bufResp.EndTag(cszResponse))
		goto done;

	fRet = TRUE;
done:

	return fRet;
}

// If a client has requested proptags that we don't support, create a new
// <propstat> element with appropriate status and a list of the tags that 
// we can't fill out.

// NOTE: This function assumes that m_bufResp has already been filled out with
// initial <response> and (potentially) a <propstat>, and that caller will
// append </response>.
BOOL CWebDav::SendUnknownXMLElementsIfNeeded(RESPONSESTATUS rs) {
	if (m_bufUnknownTags.pBuffer) {
		PCSTR szTrav = m_bufUnknownTags.pBuffer;

		if (! m_bufUnknownTags.AppendCHAR('\0'))
			return FALSE;

		if (! m_bufResp.StartTag(cszPropstat) ||
		    ! m_bufResp.SetStatusTag(rs) ||
		    ! m_bufResp.StartTag(cszProp)) {
			return FALSE;
		}

		while (*szTrav) {
			DWORD ccTrav = strlen(szTrav);

			if (! m_bufResp.BeginBraceNoNS()      ||
			    ! m_bufResp.Append(szTrav,ccTrav) ||
			    ! m_bufResp.Append("/>",2)) {
				return FALSE;
			}

			szTrav += ccTrav + 1;
		}

		if (! m_bufResp.EndTag(cszProp)     || 
		    ! m_bufResp.EndTag(cszPropstat))
			return FALSE;
	}
	return TRUE;
}

// When an unknown XML tag is hit during processing a PROPPATCH or PROPFIND request,
// save that tag (converting to ASCII first) in a buffer so that 
BOOL CWebDav::AddUnknownXMLElement(const WCHAR *wszTagName, const DWORD ccTagName) {
	SVSUTIL_ASSERT((m_pRequest->m_idMethod == VERB_PROPFIND) || (m_pRequest->m_idMethod == VERB_PROPPATCH));
	UINT uiCodePage = GetUTF8OrACP();

	int iLen = WideCharToMultiByte(uiCodePage, 0, wszTagName, ccTagName, NULL, 0, 0, 0);

	if (iLen == 0) {
		DEBUGCHK(0);
		return TRUE;
	}

	if (! m_bufUnknownTags.AllocMem(iLen+1))
		return FALSE;

	WideCharToMultiByte(uiCodePage, 0,wszTagName, ccTagName, m_bufUnknownTags.pBuffer+m_bufUnknownTags.uiNextIn, iLen, 0, 0);
	m_bufUnknownTags.uiNextIn += iLen;
	m_bufUnknownTags.AppendCHAR('\0'); // always succeeds because we alloc'd an extra byte for it above.

	return TRUE;
}

// Only returns property names associated with resource, not their values.
BOOL CWebDav::SendPropertyNamesVisitFcn(WIN32_FIND_DATA *pFindData, DWORD dwContext, WCHAR *szTraversalPath, BOOL fRootDir) {
	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav PROPFIND returning all property names\r\n"));
	DEBUGCHK(m_pRequest->m_rs == STATUS_MULTISTATUS);
	BOOL fIsRoot = (NULL==szTraversalPath);

	BOOL fRet = FALSE;

	if (! m_bufResp.StartTag(cszResponse))
		goto done;

	// <a:href>http://CEDevice/fileName/...</a:href>
	if (! SetHREFTagFromDestinationPath(pFindData,szTraversalPath,fRootDir,NULL,fIsRoot))
		goto done;

	// <a:propstat>
	if (! m_bufResp.StartTag(cszPropstat))
		goto done;

	// <a:status>	
	if (! m_bufResp.SetStatusTag(STATUS_OK))
		goto done;

	// <a:prop>
	if (! m_bufResp.StartTag(cszProp))
		goto done;

	if (! m_bufResp.SetEmptyElementNoEncode(cszGetcontentLengthNS,ccGetcontentLengthNS) ||  
	    ! m_bufResp.SetEmptyElementNoEncode(cszCreationDateNS,ccCreationDateNS) ||  
	    ! m_bufResp.SetEmptyElementNoEncode(cszDisplayName,ccDisplayName) ||
	    ! m_bufResp.SetEmptyElementNoEncode(cszGetETag,ccGetETag) ||
	    ! m_bufResp.SetEmptyElementNoEncode(cszLastModifiedNS,ccLastModifiedNS) ||
	    ! m_bufResp.SetEmptyElementNoEncode(cszSupportedLock,ccSupportedLock) ||
	    ! m_bufResp.SetEmptyElementNoEncode(cszLockDiscovery,ccLockDiscovery) ||
	    ! m_bufResp.SetEmptyElementNoEncode(cszIsHiddenNS,ccIsHiddenNS) ||
	    ! m_bufResp.SetEmptyElementNoEncode(cszIsCollectionNS,ccIsCollectionNS))
		goto done;

	// close up XML tags
	if (! m_bufResp.EndTag(cszProp)     || 
	    ! m_bufResp.EndTag(cszPropstat) ||
	    ! m_bufResp.EndTag(cszResponse))
		goto done;

	fRet = TRUE;
done:
	return fRet;
}

// Send all properties
BOOL CWebDav::SendProperties(DWORD dwDavPropFlags, BOOL fSendNamesOnly) {
	PFN_RECURSIVE_VISIT pfnVisit = fSendNamesOnly ? ::SendPropertyNamesVisitFcn : (::SendPropertiesVisitFcn);

	DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: WebDav PROPFIND sending properties for %s\r\n",m_pRequest->m_wszPath));
	DEBUGCHK(m_pRequest->m_rs == STATUS_OK);

	if (! DavGetFileAttributesEx()) {
		DEBUGCHK(0); // earlier GetFileAttributes() worked, this should too.
		SetStatus(STATUS_INTERNALERR);
		return FALSE;
	}

	SetMultiStatusResponse();

	// Print out the root node first.
	if (! pfnVisit(this,(WIN32_FIND_DATA*)&m_fileAttribs,dwDavPropFlags,NULL,FALSE))
		goto done;

	// If we're not a directory or if depth=0 on directory, we're done.
	if (! IsCollection() || (m_Depth == DEPTH_ZERO)) {
		return TRUE;
	}

	if (IsCollection() && (! m_pRequest->AllowDirBrowse())) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: PROPFIND requests collection info but dir brows turned off, only returning info on root dir\r\n"));
		return TRUE;
	}

	RecursiveVisitDirs(m_pRequest->m_wszPath,dwDavPropFlags,pfnVisit,FALSE);
done:
	// always return TRUE because we've set "Multistatus" error code.
	DEBUGCHK(m_pRequest->m_rs == STATUS_MULTISTATUS);
	return TRUE;
}

// Client has given us a restricted list of properties and/or files it wants
BOOL CWebDav::SendSubsetOfProperties(void) {
	DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: WebDav PROPFIND will send subset of properties based on XML body\r\n"));
	DEBUGCHK(m_pRequest->m_rs == STATUS_OK);

	BOOL fRet = FALSE;
	CPropFindParse cPropFind(this);

	return ParseXMLBody(&cPropFind);
}


void BuildFileName(WCHAR *szWriteBuf, WCHAR *szPath, WCHAR *szFileName) {
	DWORD ccFile;

	wcscpy(szWriteBuf,szPath);
	ccFile = wcslen(szWriteBuf);
	szWriteBuf[ccFile++] = '\\';
	wcscpy(szWriteBuf+ccFile,szFileName);
	DEBUGCHK(wcslen(szWriteBuf) < MAX_PATH);
}


//
//  PROPPATCH
//


// We'll pretend like we can handle all Win32 properties (we can only do Win32FileAttributes),
// and will return "403" for other properties.
BOOL CWebDav::SendPropPatchResponse(RESPONSESTATUS rs) {
	if ((m_dwPropPatchUpdate == 0) && (NULL == m_bufUnknownTags.pBuffer)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: PROPPATCH did not set any elements, bad request\r\n"));
		SetStatus(STATUS_BADREQ);
		return FALSE;
	}

	if (! SetMultiStatusResponse()         ||
	    ! m_bufResp.StartTag(cszResponse)  ||
	    ! SetHREFTag()) {
		return  FALSE;
	}

	if (m_dwPropPatchUpdate) {
		if ( ! m_bufResp.StartTag(cszPropstat) ||
		     ! m_bufResp.SetStatusTag(rs)           ||
		     ! m_bufResp.StartTag(cszProp))
		{
		   	return FALSE;
		}

		if ((m_dwPropPatchUpdate & DAV_PROP_W32_FILE_ATTRIBUTES) && ! SetEmptyTagWin32NS(&m_bufResp,cszWin32FileAttribs,ccWin32FileAttribs))
			return FALSE;

		if ((m_dwPropPatchUpdate & DAV_PROP_W32_LAST_MODIFY_TIME) && ! SetEmptyTagWin32NS(&m_bufResp,cszWin32LastModifiedTime,ccWin32LastModifiedTime))
			return FALSE;

		if ((m_dwPropPatchUpdate & DAV_PROP_W32_CREATION_TIME) && ! SetEmptyTagWin32NS(&m_bufResp,cszWin32CreationTime,ccWin32CreationTime))
			return FALSE;

		if ((m_dwPropPatchUpdate & DAV_PROP_W32_LAST_ACCESS_TIME) && ! SetEmptyTagWin32NS(&m_bufResp,cszWin32LastAccessTime,ccWin32LastAccessTime))
			return FALSE;

		if (! m_bufResp.EndTag(cszProp)  ||
		    ! m_bufResp.EndTag(cszPropstat))
			return FALSE;
	}

	if (! SendUnknownXMLElementsIfNeeded(STATUS_FORBIDDEN))
		return FALSE;

	if (! m_bufResp.EndTag(cszResponse)) 
		return FALSE;

	return TRUE;
}

// Effectively does a memset(0) on pFindData, but don't call memset to avoid 
// having to zero out long cFileName struct, can have same effect by setting its first byte = 0.
void ZeroW32FData(WIN32_FIND_DATAW *pFindData) {
	pFindData->dwFileAttributes = 0;
	pFindData->ftCreationTime.dwLowDateTime = pFindData->ftLastAccessTime.dwLowDateTime = pFindData->ftLastWriteTime.dwLowDateTime = 0;
	pFindData->ftCreationTime.dwHighDateTime = pFindData->ftLastAccessTime.dwHighDateTime = pFindData->ftLastWriteTime.dwHighDateTime = 0;
	pFindData->nFileSizeHigh = pFindData->nFileSizeLow = pFindData->dwOID = 0;
	pFindData->cFileName[0] = 0;
}

//
//  DELETE
//
BOOL CWebDav::DeleteVisitFcn(WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szTraversalPath, BOOL fRootDir) {
	CDavDeleteContext *pContext = (CDavDeleteContext*)dwContext;

	DEBUGCHK(! URLHasTrailingSlashW(szTraversalPath));

	DEBUGMSG_VISIT_FCN("DeleteVisitFcn");
	RESPONSESTATUS rs = STATUS_MAX;

	BOOL fRet = FALSE;
	
	if (IsDirectory(pFindData)) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav removing directory %s\r\n",szTraversalPath));
		fRet = RemoveDirectory(szTraversalPath);

		if (!fRet) {
			DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: RemoveDirectory(%s) failed, GLE=0x%08x\r\n",szTraversalPath,GetLastError()));
			// On non-empty dir, lower level set error
			if (SendErrorOnRemoveDirectoryFailure()) {
				// We don't use existing pFindData because it has the node directory name encoded in it,
				// but szTraversalPath also has this info in it, so we'd end up duplicating final file name.
				WIN32_FIND_DATAW w32FData;
				ZeroW32FData(&w32FData);
				SetMultiStatusErrorFromDestinationPath(GLEtoStatus(),&w32FData,szTraversalPath,fRootDir,pContext->szURL);
				return TRUE;
			}
			else {
				fRet = TRUE;
			}
		}
	}
	else {
		WCHAR szFile[MAX_PATH*2];
		if (!(m_pRequest->GetPerms() & HSE_URL_FLAGS_SCRIPT_SOURCE) && IsPathScript(pFindData->cFileName)) {
#if defined (DEBUG)
			BuildFileName(szFile,szTraversalPath,pFindData->cFileName);
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Cannot MOVE/COPY %s, it is a script but SCRIPT_SOURCE permissions not granted\r\n",szFile));
#endif
			rs = STATUS_FORBIDDEN;
		}
		else {
			BuildFileName(szFile,szTraversalPath,pFindData->cFileName);
			fRet = DavDeleteFile(szFile);

			if (IsFileLocked(szFile)) {
				rs = STATUS_LOCKED;
			}
		}
	}

	if (!fRet && pContext->fSendErrorsToClient) {
		rs = (rs == STATUS_MAX) ? GLEtoStatus() : rs;
		SetMultiStatusErrorFromDestinationPath(rs,pFindData,szTraversalPath,fRootDir,pContext->szURL);
	}
	return TRUE;
}

BOOL CWebDav::DeleteCollection(WCHAR *wszPath, CHAR *szURL) {
	CDavDeleteContext cDel(szURL,TRUE);
	
	BOOL fRet = FALSE;

	// per spec, depth must be infinite
	if (! GetDepth() || m_Depth != DEPTH_INFINITY) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: DELETE fails, depth not valid or not set to \"infinity\"\r\n"));
		SetStatus(STATUS_BADREQ);
		return FALSE;
	}

	if (RecursiveVisitDirs(wszPath,(DWORD)&cDel,::DeleteVisitFcn,TRUE)) {
		if (! RemoveDirectory(wszPath)) {
			DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: WebDav removing directory %s, GLE=0x%08x\r\n",wszPath,GetLastError()));

			if (SendErrorOnRemoveDirectoryFailure())
				SetMultiStatusErrorFromURL(GLEtoStatus(),szURL);
		}
		else
			fRet = TRUE;
	}
	return fRet;
}


//
//  MOVE/COPY
//

// Returns a new buffer containing the canonicalized URL.  ppszDestinationPrefix is 
// pointer to raw headers containing http://... part of URL, ppszDestinationURL
// points to the URL portion.
PSTR CWebDav::GetDestinationHeader(PSTR *ppszDestinationPrefix, PSTR *ppszDestinationURL, PSTR *ppszSave) {
	PSTR  szDestinationPrefix  = NULL;
	PSTR  szDestinationURL     = NULL;
	PSTR  szDestination        = NULL;

	// Destination: header contains location of resource to write to.  May
	// be in form "http://CEDevice/destination", so need to crack this out, canonicalize,
	// and then find VRoot on the device it corresponds to (not necessarily the
	// vroot of m_pszURL).

	if (NULL == (szDestinationPrefix = m_pRequest->FindHttpHeader(cszDestination,ccDestination))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, \"Destination:\" request header not present\r\n"));
		SetStatus(STATUS_BADREQ);
		goto done;
	}

	*ppszSave = strchr(szDestinationPrefix,'\r');
	DEBUGCHK(*(*ppszSave)); // request wouldn't have past had it been wrongly formatted
	*(*ppszSave) = 0;
	m_pRequest->DebugSetHeadersInvalid(TRUE);

	if (NULL == (szDestinationURL = SkipHTTPPrefixAndHostName(szDestinationPrefix))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, \"Destination:\" header not formatted corectly.\r\n"));
		SetStatus(STATUS_BADREQ);
		goto done;
	}

	// szDestinationURL is pointer into m_pRequest->m_bufRequest.  Need 
	// a new copy of buffer for canonicalization.  +1 to store possible '/' at end.
	if (NULL == (szDestination = MySzAllocA(strlen(szDestinationURL)+1))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, server out of memory\r\n"));
		SetStatus(STATUS_INTERNALERR);
		goto done;
	}

	MyInternetCanonicalizeUrlA(szDestinationURL,szDestination,NULL,0);
	RemoveTrailingSlashIfNeededA(szDestination);

done:
	if (szDestination) {
		*ppszDestinationPrefix = szDestinationPrefix;
		*ppszDestinationURL    = szDestinationURL;
	}

#if defined (DEBUG)
	if (m_pRequest->m_rs == STATUS_OK)
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav GetDestinationHeader sets canonicalized dest root = <<%a>>, destURL = <<%a>>\r\n",
		         szDestination,szDestinationURL));
#endif

	return szDestination;
}

// Determines whether we have write access to "Destination:" URL.  Parameters passed in are the 
// access permisions of the vroot in destination.
BOOL CWebDav::HasAccessToDestVRoot(CHAR *szDestURL, WCHAR *szPath, PVROOTINFO pVRoot) {
	AUTHLEVEL       authGranted = AUTH_PUBLIC;
	RESPONSESTATUS  rs   = STATUS_INTERNALERR;
	BOOL            fRet = FALSE;

	authGranted = m_pRequest->DeterminePermissionGranted(pVRoot->wszUserList,m_pRequest->m_AuthLevelGranted);

	if (authGranted < pVRoot->AuthLevel) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, destination root requires auth level (%d), user only has access (%d)\r\n",
		                        pVRoot->AuthLevel,authGranted));
		rs = STATUS_UNAUTHORIZED;
		goto done;
	}

	if (! (pVRoot->dwPermissions & HSE_URL_FLAGS_WRITE)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, virtual root of destination does not support writing\r\n"));
		rs = STATUS_FORBIDDEN;
		goto done;
	}

	if ( (!(pVRoot->dwPermissions & HSE_URL_FLAGS_SCRIPT_SOURCE)) && IsPathScript(szPath,pVRoot->ScriptType)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: MOVE/COPY fails, attempting to write to a script resource but no HSE_URL_FLAGS_SCRIPT_SOURCE flags\r\n"));
		rs = STATUS_FORBIDDEN;
		goto done;
	}

	fRet = TRUE;
done:
	if (! fRet) {
		DEBUGCHK(rs != STATUS_INTERNALERR);
		SetMultiStatusErrorFromURL(rs,szDestURL);
	}

	return fRet;                             
}

BOOL RemoveDirVisitFcn(CWebDav *pThis, WIN32_FIND_DATA *pFindData, DWORD dwContext, WCHAR *szTraversalPath, BOOL fRootDir) {
	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav removing empty directories after move operation\r\n"));
	DEBUGMSG_VISIT_FCN("RemoveDirVisitFcn");

	// When we're doing a move of a collection, run through directory tree
	// (depth first) and remove directories of source.  Note that we don't delete
	// files and RemoveDirectory will fail if files are in sub-dir; this is by
	// design because source files may not be legitamitly moved due to error and
	// we wouldn't delete them in this case.

	if (IsDirectory(pFindData->dwFileAttributes)) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav RemoveDirectory(%s) on cleanup from move\r\n",szTraversalPath));

		// don't use ZONE_ERROR because it's legit to fail here
		if (! RemoveDirectory(szTraversalPath))
			DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav RemoveDirectory(%s) failed, GLE=0x%08x\r\n",szTraversalPath,GetLastError()));
	}
	return TRUE;
}

BOOL CWebDav::MoveCopyCollection(CMoveCopyContext *pcContext) {
	BOOL fRet = FALSE;

	DEBUGCHK(m_pRequest->m_rs == STATUS_OK);

	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav calls CreateDirectory(%s)\r\n",pcContext->wszDestPath));
	if (!CreateDirectory(pcContext->wszDestPath,NULL)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: CreateDirectory(%s) failed, GLE=0x%08x\r\n",pcContext->wszDestPath,GetLastError()));
		SetMultiStatusErrorFromURL(GLEtoStatus(),pcContext->szDestURL);
		return TRUE;
	}

	fRet = RecursiveVisitDirs(m_pRequest->m_wszPath,(DWORD)pcContext,::MoveCopyVisitFcn,FALSE);
	if (pcContext->fDeleteSrc) {
		DEBUGCHK(m_Depth == DEPTH_INFINITY);
		// Recursively delete source directories now that we're through.

		if (RecursiveVisitDirs(m_pRequest->m_wszPath,0,::RemoveDirVisitFcn,TRUE))
			RemoveDirectory(m_pRequest->m_wszPath);
	}
	return fRet;
}


BOOL DeleteFileAndFileLock(WCHAR *szFile, CWebDavFileNode *pFileNode) {
	DEBUGCHK(g_pFileLockManager->IsLocked());

	if (pFileNode) {
		g_pFileLockManager->RemoveFileNodeFromLinkedList(pFileNode);
		g_pFileLockManager->DeInitAndFreeFileNode(pFileNode);
	}

	if (DeleteFile(szFile))
		return TRUE;

	DEBUGMSG(ZONE_ERROR,(L"HTTPD: DeleteFile(%s) fails, GLE=0x%08x\r\n",szFile,GetLastError()));
	return FALSE;
}

BOOL CWebDav::MoveCopyFile(BOOL fDeleteSrc, WCHAR *wszSrcPath, WCHAR *wszDestPath) {
	BOOL fRet;
	CWebDavFileNode *pSrcNode  = NULL;
	CWebDavFileNode *pDestNode = NULL;

	HANDLE hSrc       = INVALID_HANDLE_VALUE;
	HANDLE hDest      = INVALID_HANDLE_VALUE;
	BOOL   fCloseSrc  = FALSE;
	BOOL   fCloseDest = FALSE;
	CHAR   szBuf[MINBUFSIZE*3];
	DWORD  dwRead;
	DWORD  dwDummy;

	if (fDeleteSrc) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav calls MoveFile(%s,%s)\r\n",wszSrcPath,wszDestPath));
		fRet = MoveFile(wszSrcPath,wszDestPath);
	}
	else {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav calls CopyFile(%s,%s,%d)\r\n",wszSrcPath,wszDestPath,(m_Overwrite & OVERWRITE_YES)));
		fRet = CopyFile(wszSrcPath,wszDestPath,(m_Overwrite & OVERWRITE_YES) ? FALSE : TRUE);
	}

	if (fRet)
		return TRUE;

	// If initial move/copy didn't work, perhaps it's because the file is LOCKED.
	// If client sent correct lock tokens along, unlock now.
	if (!CheckLockTokens())
		goto done;

	g_pFileLockManager->Lock();
	if (! g_pFileLockManager->CanPerformLockedOperation(wszSrcPath,wszDestPath,&m_lockIds,m_nLockTokens,&pSrcNode,&pDestNode)) {
		g_pFileLockManager->Unlock();
		DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: Client sent lock tokens, but they didn't correspond to file names %s and/or %s, move/copy failed\r\n",wszSrcPath,wszDestPath));
		goto done;
	}
	DEBUGCHK(g_pFileLockManager->IsLocked()); // on success, function leaves manager lockes to ensure pSrcNode and pDestNode are still valid.
	DEBUGCHK(pSrcNode || pDestNode); // at least one should be non-NULL on CanPerformLockedOperation() success.

	// It's possible that both files aren't lock.  If we don't get handle to file from
	// an existing lock, create it here.
	if (pSrcNode) {
		hSrc = pSrcNode->m_hFile;
		pSrcNode->SetBusy(TRUE);
	}
	else {
		fCloseSrc = TRUE;
		if (INVALID_HANDLE_VALUE == (hSrc = CreateFile(wszSrcPath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0))) {
			DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: CreateFile(%s) on src file during move/copy failed, GLE = 0x%08x\r\n",wszSrcPath,GetLastError()));
			goto doneInCS;
		}
	}

	if (pDestNode) {
		hDest = pDestNode->m_hFile;
		pDestNode->SetBusy(TRUE);
	}
	else {
		fCloseDest = TRUE;
		if (INVALID_HANDLE_VALUE == (hDest = CreateFile(wszDestPath, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0))) {
			DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: CreateFile(%s) on dest file during move/copy failed, GLE = 0x%08x\r\n",wszSrcPath,GetLastError()));
			goto doneInCS;
		}
	}
	// Since we could be copying megabytes, don't hold lock all this time.
	g_pFileLockManager->Unlock();

	SetFilePointer(hSrc,0,NULL,FILE_BEGIN);
	SetFilePointer(hDest,0,NULL,FILE_BEGIN);

	fRet = TRUE;

	while (ReadFile(hSrc, szBuf, sizeof(szBuf), &dwRead, 0) && dwRead) {
		if (! WriteFile(hDest,szBuf,dwRead,&dwDummy,NULL)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WriteFile(%s) during locked move/copy failed, GLE=0x%08x\r\n",wszDestPath,GetLastError()));
			fRet = FALSE;
			break;
		}
	}
	SetEndOfFile(hDest);

	g_pFileLockManager->Lock();

doneInCS:
	DEBUGCHK(g_pFileLockManager->IsLocked());

	if (pSrcNode)
		pSrcNode->SetBusy(FALSE);

	if (pDestNode)
		pDestNode->SetBusy(FALSE);

	if (fCloseSrc)
		MyCloseHandle(hSrc);

	if (fCloseDest)
		MyCloseHandle(hDest);
	
	if (fRet && fDeleteSrc)
		DeleteFileAndFileLock(wszSrcPath,pSrcNode);

	g_pFileLockManager->Unlock();
done:
	DEBUGCHK(! g_pFileLockManager->IsLocked());
	DEBUGMSG(ZONE_ERROR,(L"HTTPD: Failed move/copy %s-->%s, GLE=0x%08x\r\n",wszSrcPath,wszDestPath,GetLastError()));
	return fRet;
}

// MoveCopyVisitFcn is called by RecursiveVisitDirs when copying or moving a directory tree.
// It is responsible for creating directories that don't exist, checking to make sure
// if a file is a script that we have permission to access it, and doing copy/move itself.

// szTraversalPath is the path of the directory of the source that we're currently visiting,
// pFindData->cFileName is file name in the source (except when source is a directory
// itself, in which case szTraversalPath contains complete path).  

// Per the DAV spec, even if a specific operation fails for whatever reason, we 
// keep trying the move/copy.

BOOL CWebDav::MoveCopyVisitFcn(WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szTraversalPath, BOOL fRootDir) {
	CMoveCopyContext *pContext = (CMoveCopyContext *)dwContext;
	WCHAR szSource[MAX_PATH*2];
	WCHAR szDestination[MAX_PATH*2];
	DWORD ccWrite;
	DWORD ccRelativePath;
	BOOL  fSuccess = FALSE;
	RESPONSESTATUS rs = STATUS_MAX;

	DEBUGCHK(! URLHasTrailingSlashW(szTraversalPath));
	DEBUGCHK(! URLHasTrailingSlashW(pContext->wszDestPath));
	// Node we're visiting must be a subdirectory of the m_wszPath, which is source to do move/copy from.
	DEBUGCHK(0 == wcsncmp(szTraversalPath,m_pRequest->m_wszPath,m_pRequest->m_ccWPath));

	DEBUGMSG_VISIT_FCN("MoveCopyVisitFcn");

	// Buildup the destination path by concatinating the base directory of the
	// destination, the part of the current src path we're in (not including its
	// base directory).

	// root of dest dir
	wcscpy(szDestination,pContext->wszDestPath);
	ccWrite = pContext->ccDestPath;

	// tack on relative portion of src dir
	ccRelativePath = wcslen(szTraversalPath) - pContext->ccSrcPath;
	DEBUGCHK(((int)ccRelativePath >= 0));
	if (ccRelativePath) {
		szDestination[ccWrite++] = L'\\';
		wcscpy(szDestination+ccWrite,szTraversalPath+pContext->ccSrcPath);
		ccWrite += ccRelativePath;
	}

	if (IsDirectory(pFindData->dwFileAttributes)) {
		szDestination[ccWrite++] = L'\\';
		wcscpy(szDestination+ccWrite,pFindData->cFileName);

		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav calls CreateDirectory(%s)\r\n",szDestination));
		if (! CreateDirectory(szDestination,NULL)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: CreateDirectory(%s) failed during recursive move/copy operation, GLE=0x%08x\r\n",szDestination,GetLastError()));
			SetMultiStatusErrorFromDestinationPath(GLEtoStatus(),pFindData,szTraversalPath,fRootDir,
			                                        pContext->szDestURL);
			return TRUE;
		}
		return TRUE;
	}

	if (( !(pContext->dwSrcPermFlags & HSE_URL_FLAGS_SCRIPT_SOURCE) || !(pContext->dwDestPermFlags & HSE_URL_FLAGS_SCRIPT_SOURCE)) &&
	    IsPathScript(pFindData->cFileName)) 
	{
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Attempting to access script in recursive move/copy but script access not allowed, ACCESS DENIED\r\n"));
		SetMultiStatusErrorFromDestinationPath(STATUS_FORBIDDEN,pFindData,szTraversalPath,fRootDir);
		return TRUE;
	}

	// Add file name to destination.
	DEBUGCHK(! URLHasTrailingSlashW(szDestination));
	szDestination[ccWrite++] = L'\\';
	wcscpy(szDestination+ccWrite,pFindData->cFileName);
	DEBUGCHK(wcslen(szDestination) < SVSUTIL_ARRLEN(szDestination));

	// Buildup src path
	BuildFileName(szSource,szTraversalPath,pFindData->cFileName);
	DEBUGCHK(wcslen(szSource) < SVSUTIL_ARRLEN(szSource));

	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav calls %a on (%s --> %s)\r\n",pContext->fDeleteSrc ? "MOVE" : "COPY",
	                              szSource,szDestination));

	if (IsFileNameTooLongForFilesys(szSource) || IsFileNameTooLongForFilesys(szDestination)) 
		rs = STATUS_REQUEST_TOO_LARGE;
	else
		fSuccess = MoveCopyFile(pContext->fDeleteSrc,szSource,szDestination);

	if (!fSuccess) {
		PCSTR szURL = pContext->szDestURL;

		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav %a on (%s --> %s) fails, GLE=0x%08x\r\n",pContext->fDeleteSrc ? "MOVE" : "COPY",
		                          szSource,szDestination,GetLastError()));

		// Check to see if the file is locked, and also which file it is.
		if (rs == STATUS_MAX) {
			if (IsGLELocked()) {
				g_pFileLockManager->Lock();
				if (g_pFileLockManager->GetNodeFromFileName(szSource)) {
					szURL = m_pRequest->m_pszURL;
					rs = STATUS_LOCKED;
				}
				else if (g_pFileLockManager->GetNodeFromFileName(szDestination)) {
					rs = STATUS_LOCKED;
				}
				g_pFileLockManager->Unlock();
			}
			else
				rs = GLEtoStatus();
		}

		SetMultiStatusErrorFromDestinationPath(rs,pFindData,szTraversalPath,fRootDir,
			                                       szURL);
	}
	return TRUE;
}


//
//  Wrap Filesystem API calls
//


BOOL CWebDav::DavDeleteFile(WCHAR *szFile) {
	BOOL fRet = FALSE;

	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav calling DeleteFile(%s)\r\n",szFile));
	if (DeleteFile(szFile))
		return TRUE;

	// If delete failed because we have file locked and client sent lock tokens, try
	// going through lock manager.
	if (CheckLockTokens()) {
		CWebDavFileNode *pFileNode;

		g_pFileLockManager->Lock();

		if (g_pFileLockManager->CanPerformLockedOperation(szFile,NULL,&m_lockIds,m_nLockTokens,&pFileNode,NULL)) {
			// client sent a lock-token associated with the file.  Delete the lock token, and then the file
			fRet = DeleteFileAndFileLock(szFile,pFileNode);
			g_pFileLockManager->Unlock();
			return fRet;
		}
		g_pFileLockManager->Unlock();
	}

	DEBUGCHK(! g_pFileLockManager->IsLocked());
	DEBUGMSG(ZONE_ERROR,(L"HTTPD: Failed deleting file %s, GLE=0x%08x\r\n",szFile,GetLastError()));
	return FALSE;
}

