//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: davutil.cpp
Abstract: WebDAV utility functions
--*/


#include "httpd.h"

// Returns pointer to value part of the HTTP header, skipping past white space between ":" and value.
// NOTE: this string is not NULL terminated, but terminated by \r\n.


// Skip past "http(s)://CEHostName/".
PSTR SkipHTTPPrefixAndHostName(PSTR szURL) {
	PSTR szSlash;
	PSTR szFinalURL;
	
	if (*szURL == '/')
		return szURL; // URL is relative, no http://.../ stuff to begin with

	if (NULL == (szSlash = strchr(szURL,'/')))
		return NULL;

	// skip opening 2 /'s.
	szSlash += (szSlash[1] == '/') ? 2 : 1;

	// skip the host name
	szFinalURL = strchr(szSlash,'/');
	return szFinalURL ? szFinalURL : szSlash;
}


// Retrieves "Depth:" HTTP header and maps to approrpiate enumeration
BOOL CWebDav::GetDepth(DEPTH_TYPE depthDefault) {
	PSTR szHeader;

	// If we have depth already we're done.
	if (m_Depth != DEPTH_UNKNOWN)
		return TRUE;

	if (NULL == (szHeader = m_pRequest->FindHttpHeader(cszDepth,ccDepth))) {
		m_Depth = depthDefault;
		return TRUE;
	}

	if (0 == _strnicmp(szHeader,csz0,cc0))
		m_Depth = DEPTH_ZERO;
	else if (0 == _strnicmp(szHeader,csz1,cc1))
		m_Depth = DEPTH_ONE;
	else if (0 == _strnicmp(szHeader,cszInfinity,ccInfinity))
		m_Depth = DEPTH_INFINITY;
	else if (0 == _strnicmp(szHeader,csz1NoRoot,cc1NoRoot))
		m_Depth = DEPTH_ONE_NOROOT;
	else
		return FALSE;

	return TRUE;
}

void CWebDav::GetOverwrite(void) {
	PSTR szHeader = m_pRequest->FindHttpHeader(cszOverwrite,ccOverwrite);

	if ((NULL == szHeader) || (*szHeader == 't') || (*szHeader == 'T'))
		m_Overwrite = OVERWRITE_YES;
	else
		m_Overwrite = OVERWRITE_NO;

	szHeader = m_pRequest->FindHttpHeader(cszAllowRename,ccAllowRename);

	if (szHeader && (*szHeader == 't' || *szHeader == 'T'))
		m_Overwrite |= OVERWRITE_RENAME;
}


// szSrc and szDest are physical paths of source and destination of a move/copy.
// They cannot be a subset of one another, i.e. 'move \a \a\b' is disallowed.
BOOL IsPathSubDir(WCHAR *szSrc, DWORD ccSrc, WCHAR *szDest, DWORD ccDest) {
	if ((ccSrc == 0) || (ccDest == 0))
		return TRUE;

	if (ccSrc == ccDest)
		return (0 == PathNameCompare(szSrc,szDest));

	WCHAR *szShorter;
	WCHAR *szLonger;
	DWORD ccShorter;
	DWORD ccLonger;

	if (ccSrc > ccDest) {
		szShorter = szDest;
		szLonger  = szSrc;
		ccShorter = ccDest;
		ccLonger  = ccSrc;
	}
	else {
		szShorter = szSrc;
		szLonger  = szDest;
		ccShorter = ccSrc;
		ccLonger  = ccDest;	
	}

	// look for shorter path.  We check back-slashes to make sure things are really
	// equal, i.e. without backslash check '\windows\foo' and '\windows\foobar'
	// would match, which isn't what we're looking for.

	if (0 == PathNameCompareN(szSrc,szDest,ccShorter)) {
		if ( (L'\\' == szShorter[ccShorter-1]) ||
		     (L'/'  == szShorter[ccShorter-1]) ||
		     (L'\\' == szLonger [ccShorter])   ||
		     (L'/' == szLonger [ccShorter])) {
			return TRUE;
		}
	}
	return FALSE;
}

inline void SetFiletime(FILETIME *pftDest, FILETIME *pftSrc) {
	memcpy(pftDest,pftSrc,sizeof(FILETIME));
}

BOOL CWebDav::DavGetFileAttributesEx() {
	if (! m_fRetrievedFileAttribs) {
		if (!GetFileAttributesEx(m_pRequest->m_wszPath,GetFileExInfoStandard,&m_fileAttribs)) {
			DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: GetFileAttributesEx(%s) fails, GLE=0x%08x\r\n",m_pRequest->m_wszPath,GetLastError()));
			return FALSE;
		}

		m_fRetrievedFileAttribs = TRUE;
		return TRUE;
	}
	return TRUE;
}

// Make a copy of this data here rather than (possibly) having to alloc 1 KB of RAM
// in m_bufRespHeaders for every response.
const CHAR cszDefaultXMLHttpHeader[] = "Content-Type: text/xml\r\nTransfer-Encoding: chunked\r\n\r\n";

#define ADD_CRLF(sz,ccWrite)  (sz)[(ccWrite++)] = '\r'; (sz)[(ccWrite++)] = '\n';

DWORD AddHttpHeader(CHAR *szBuf, PCSTR szHeaderName, DWORD ccHeaderName, PCSTR szHeaderValue, DWORD ccHeaderValue) {
	DWORD ccWritten = 0; 
	
	strcpy(szBuf,szHeaderName);
	ccWritten = ccHeaderName;
	szBuf[ccWritten++] = ' ';

	strcpy(szBuf+ccWritten,szHeaderValue);
	ccWritten += ccHeaderValue;
	ADD_CRLF(szBuf,ccWritten);

	return ccWritten;
}

// For success cases (i.e. 2XX) where we also need to send XML in body.
BOOL CWebDav::SetStatusAndXMLBody(RESPONSESTATUS rs) {
	if (!m_fSetStatus) {
		CHAR  szHeaders[MINBUFSIZE];
		DWORD ccWritten;

		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav setting response %d headers\r\n",rgStatus[rs].dwStatusNumber));

		ccWritten = AddHttpHeader(szHeaders,cszContent_Type,ccContent_Type,
		                                    cszTextXML,ccTextXML);

		if (m_pRequest->m_fResponseHTTP11) {
			ccWritten += AddHttpHeader(szHeaders+ccWritten,cszTransfer_Encoding,ccTransfer_Encoding,
			                                     cszChunked,ccChunked);
			ADD_CRLF(szHeaders,ccWritten); // closing CRLF at end of HTTP headers
			szHeaders[ccWritten++] = 0;
			CHttpResponse resp(m_pRequest, rs);
			resp.SendHeaders(szHeaders,NULL);
		}
		else {
			m_pRequest->m_rs = rs;
			m_pRequest->m_bufRespHeaders.AppendData(szHeaders,ccWritten);
		}
		m_fSetStatus = TRUE;

		if (! m_bufResp.Append(cszXMLVersionA, ccXMLVersionA))
			return FALSE;

		if ((m_pRequest->m_rs == STATUS_MULTISTATUS) && !m_bufResp.StartTagNoEncode(cszMultiStatusNS,ccMultiStatusNS))
			return FALSE;

		DEBUGCHK(ccWritten < sizeof(szHeaders));
		m_fXMLBody = TRUE;
	}
	return TRUE;
}

BOOL CWebDav::FinalizeMultiStatusResponse(void) {
	DEBUGCHK(m_fSetStatus && m_fXMLBody);
	m_bufResp.BufferConsistencyChecks();

	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav flushing buffer for the final time\r\n"));

	if ((m_pRequest->m_rs == STATUS_MULTISTATUS) && !m_bufResp.EndTag(cszMultiStatus))
		return FALSE;

	return m_bufResp.FlushBuffer(TRUE);
}


// Creates string in format "http(s)://CeServerName/[szRoot]".  Calling function
// will optionally add remainder of URL
BOOL CWebDav::BuildBaseURL(PCSTR szRoot, BOOL fHttpHeader, BOOL fAppendSlash) {
	BOOL fRet = FALSE;
	PCSTR szHostName;

#if defined (DEBUG)
	if (fAppendSlash)
		DEBUGCHK(!URLHasTrailingSlashA(szRoot));
#endif

	if (m_pRequest->m_pszHost) {
		szHostName = m_pRequest->m_pszHost;
		m_ccHostName = strlen(m_pRequest->m_pszHost);
	}
	else { 
		if (m_ccHostName == 0) {
			if (0 != gethostname(m_szHostName, sizeof(m_szHostName)))
				goto done;

			m_ccHostName = strlen(m_szHostName);
		}
		szHostName = m_szHostName;
	}

	if (fHttpHeader) {
		if (! m_pRequest->m_bufRespHeaders.AppendData(GetHttpPrefix(),GetHttpPrefixLen())   ||
		    ! m_pRequest->m_bufRespHeaders.AppendData(szHostName,m_ccHostName)            ||
		    ! m_pRequest->m_bufRespHeaders.EncodeURL(szRoot)                ||
		    (fAppendSlash && !m_pRequest->m_bufRespHeaders.AppendCHAR('/'))                ||
		    ! m_pRequest->m_bufRespHeaders.AppendData(cszCRLF,2)) {
			goto done;
		}
	}
	else {
		if (! m_bufResp.Append(GetHttpPrefix(),GetHttpPrefixLen())   ||
		    ! m_bufResp.Encode(szHostName)                           ||
		    ! m_bufResp.EncodeURL(szRoot)                            ||
            (fAppendSlash && !m_bufResp.AppendCHAR('/'))) {
			goto done;
		}
	}

	fRet = TRUE;
done:
	return fRet;
}


// Creates string in format "http(s)://CeServerName/[Root]/", where Root
// is the originally requested URL.  The URL is cleaned up for sending if needed.
BOOL CWebDav::BuildBaseURLFromSourceURL(BOOL fHttpHeader) {
	// For directories, tack trailing '/' to end of string if not there.  For files,
	// remove it.

	BOOL fAddSlash = IsCollection() && ! m_pRequest->URLHasTrailingSlash();
	return BuildBaseURL(m_pRequest->m_pszURL,fHttpHeader,fAddSlash);
}

// Adds "Content-Location:" HTTP response headers.
BOOL CWebDav::AddContentLocationHeader(void) {
	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: Adding \"Content-Location:\" headers\r\n"));

	if (! m_pRequest->m_bufRespHeaders.AppendData(cszContent_Location,ccContent_Location)         || 
	    ! m_pRequest->m_bufRespHeaders.AppendData(" ",1)                       ||
	    ! BuildBaseURLFromSourceURL(TRUE))
	{
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Out of memory, unable to add http headers\r\n"));
		m_pRequest->m_bufRespHeaders.Reset(); 
		SetStatus(STATUS_INTERNALERR);
		return FALSE;
	}
	return TRUE;
}

// Adds "Content-Location:" to response headers when no trailing "/" on directories, effectively a redirect
BOOL CWebDav::AddContentLocationHeaderIfNeeded(void) {
	int iLen = strlen(m_pRequest->m_pszURL);
	DEBUGCHK(iLen >= 1);

	// If directory and no trailing "/", or a file and a trailing "/", 
	if (m_pRequest->URLHasTrailingSlash() == IsCollection())
		return TRUE; // no redirection needed.

	return AddContentLocationHeader();
}


// Writes contents of HTTP body into hFile
BOOL CWebDav::WriteBodyToFile(HANDLE hFile, BOOL fChunked) {
	CHAR  szBuf[HEADERBUFSIZE];
	DWORD cbBuf;
	DWORD dwWritten;

	DEBUGCHK(hFile != INVALID_HANDLE_VALUE);
	DEBUGCHK(!fChunked); // Chunked NYI...

	m_rcvBody.fChunked = fChunked;

	DEBUGMSG(ZONE_WEBDAV,(L"HTTPD: WebDav WriteBodyToFile called, hFile=0x%08x, fChunked=%d\r\n",hFile,fChunked));

	// write any data we already have into the file immediatly.
	if (!fChunked) {
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav WriteBodyToFile writes initial %d bytes to file\r\n",m_pRequest->m_bufRequest.Count()));
		if (! WriteFile(hFile,m_pRequest->m_bufRequest.Data(),m_pRequest->m_bufRequest.Count(),&dwWritten,NULL)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WriteFile fails, GLE=0x%08x\r\n",GetLastError()));
			return FALSE;
		}

		// Request read in everything
		if (m_pRequest->m_dwContentLength == m_pRequest->m_bufRequest.Count())
			return TRUE;
	}

	// There's additional data that needs to be recv()'d
	while (1) {
		cbBuf = sizeof(szBuf);

		if (! ReadNextBodySegment(szBuf,&cbBuf))
			return FALSE;

		if (cbBuf == 0)
			return TRUE;

		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav WriteBodyToFile writes extra %d bytes to file\r\n",cbBuf));
		if (! WriteFile(hFile,szBuf,cbBuf,&dwWritten,NULL)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: WriteFile fails, GLE=0x%08x\r\n",GetLastError()));
			return FALSE;
		}
	}

	DEBUGCHK(0);
	return FALSE;
}

// Reads body segment, keeping track of state as it goes along.
// Returns FALSE on errors or timeouts.  When done reading request, sets *pcbRead=0.

BOOL CWebDav::ReadNextBodySegment(PSTR szBuf, DWORD *pcbRead) {
	BOOL  fRet;

	if (m_rcvBody.dwRemaining == RECEIVE_SIZE_UNINITIALIZED) {
		if (m_rcvBody.fChunked)
			DEBUGCHK(0);
		else {
			m_rcvBody.dwRemaining = m_pRequest->m_dwContentLength-m_pRequest->m_bufRequest.Count();
			DEBUGCHK((int)m_rcvBody.dwRemaining > 0);
		}
	}

	if ((int) m_rcvBody.dwRemaining <= 0) {
		*pcbRead = 0;
		return TRUE;
	}

	*pcbRead = min(*pcbRead,m_rcvBody.dwRemaining);
	fRet = m_pRequest->ReadClient(szBuf,pcbRead);

	m_rcvBody.dwRemaining -= *pcbRead;

	if (!m_rcvBody.fChunked)
		return fRet;

	// chunked needs to parse through buffer (actually to the end of, +5 bytes at most) to look for next read size.
	return FALSE;
}

BOOL CWebDav::IsContentTypeXML(void) {
	PSTR szHeader;
	DWORD ccContent;
	
	const CHAR rgcDelimitSet[] = { ';', '\t', ' ', '\r', '\0' };

	if (NULL == (szHeader = m_pRequest->FindHttpHeader(cszContent_Type,ccContent_Type))) {
		// Note: If there is no content-type header we should return FALSE in this case,
		// however certain existing clients do not set the "content-type" field but send XML
		// anyway.  To work with them return TRUE.
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: Warning: WebDAV client did not set content-type, treating as if content-type: text/xml\r\n"));
		return TRUE;
	}

	// Content-length: may have extra info like "; charset=\"utf8\" appended to content-type, ignore it.
	ccContent = strcspn(szHeader,rgcDelimitSet);
	DEBUGCHK(ccContent);

	if ((TokensEqualAI(szHeader,cszTextXML,ccContent,ccTextXML)) ||
	    (TokensEqualAI(szHeader,cszApplicationXML,ccContent,ccApplicationXML)))
	{
		return TRUE;
	}
	return FALSE;
}

BOOL CXMLBuffer::SetStatusTag(RESPONSESTATUS rs) {
	CHAR szStatus[256];
	int iLen = sprintf(szStatus,"HTTP/1.1 %d %s",rgStatus[rs].dwStatusNumber,rgStatus[rs].pszStatusText);
	DEBUGCHK(iLen < sizeof(szStatus));
	DEBUGCHK(rs < STATUS_MAX);

	return (StartTag(cszStatus)   &&
	        Append(szStatus,iLen) &&
	        EndTag(cszStatus));
}

BOOL CWebDav::SetHREFTag(PCSTR szURL) {
	if (! m_bufResp.StartTag(cszHref))
		return FALSE;

	if (!BuildBaseURLFromSourceURL())
		return FALSE;

	if (szURL && ! m_bufResp.EncodeURL(szURL))
		return FALSE;

	if (! m_bufResp.EndTag(cszHref))
		return FALSE;

	return TRUE;
}

BOOL CWebDav::SetHREFTagFromDestinationPath(WIN32_FIND_DATA *pFindData, WCHAR *szDirectory, BOOL fRootDir, PCSTR szRootURL, BOOL fSkipFileMunge) {
	CHAR  szBuf[MINBUFSIZE]; // temp write buffer

	// When we're accessing root of physical filesystem, offset is different.
	DWORD ccDestPath = m_pRequest->IsRootPath() ? 0 : m_pRequest->m_ccWPath;

	if (!szRootURL)
		szRootURL = m_pRequest->m_pszURL;

	if (! m_bufResp.StartTag(cszHref))
		return FALSE;

	BOOL fAddSlash = (pFindData && !fSkipFileMunge && szDirectory && !URLHasTrailingSlashA(szRootURL));
	
	if (!BuildBaseURL(szRootURL,FALSE,fAddSlash))
		return FALSE;

	if (!fSkipFileMunge) {
		// Build up the remainder of URL by adding everything past the root of the destination path.
		DWORD ccBuf = 0;

		if (!fRootDir) {
			DEBUGCHK(wcslen(szDirectory+ccDestPath+1) > 0);
			DEBUGCHK((szDirectory[ccDestPath] == '\\') || (szDirectory[ccDestPath] == '/'));

			DWORD cc = MyW2UTF8(szDirectory+ccDestPath+1,szBuf,sizeof(szBuf));
			if (0 == cc)
				return FALSE;

			ccBuf += (cc-1);

			DEBUGCHK(szBuf[ccBuf] == 0);
			for (DWORD i = 0; i < ccBuf; i++) {
				if (szBuf[i] == '\\')
					szBuf[i] = '/';
			}
			szBuf[ccBuf]   = '/';
			szBuf[++ccBuf] = 0;
		}

		if (pFindData->cFileName[0]) {
			DWORD cc = MyW2UTF8(pFindData->cFileName,szBuf+ccBuf,sizeof(szBuf)-ccBuf);
			if (0 == cc)
				return FALSE;

			ccBuf += (cc-1);
		}

		if (IsDirectory(pFindData)) {
			szBuf[ccBuf]   = '/';
			szBuf[++ccBuf] = 0;
		}
		DEBUGCHK((ccBuf+1) < sizeof(szBuf));
		DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav SetHREFTagFromDestinationPath sets buffer = <<%a>>\r\n",szBuf));

		if (! m_bufResp.EncodeURL(szBuf))
			return FALSE;
	}

	if (! m_bufResp.EndTag(cszHref))
		return FALSE;

	return TRUE;
}

// Buildup <response> blob.
BOOL CWebDav::SetResponseTag(PCSTR szURL, RESPONSESTATUS rs) {
	DEBUGCHK(m_pRequest->m_rs == STATUS_MULTISTATUS);

	return ( m_bufResp.StartTag(cszResponse) &&
	         SetHREFTag(szURL)               &&
	         m_bufResp.SetStatusTag(rs)      &&
	         m_bufResp.EndTag(cszResponse));
}

BOOL CWebDav::SetMultiStatusErrorFromDestinationPath(RESPONSESTATUS rs, WIN32_FIND_DATA *pFindData, WCHAR *szDirectory, BOOL fRootDir, PCSTR szRootURL, BOOL fSkipFileMunge) {
	if (! SetMultiStatusResponse())
		return FALSE;

	return ( m_bufResp.StartTag(cszResponse) &&
	         SetHREFTagFromDestinationPath(pFindData,szDirectory,fRootDir,szRootURL,fSkipFileMunge)  &&
	         m_bufResp.SetStatusTag(rs)      &&
	         m_bufResp.EndTag(cszResponse));
}


BOOL CWebDav::SetMultiStatusErrorFromURL(RESPONSESTATUS rs, PCSTR szRootURL) {
	if (! SetMultiStatusResponse())
		return FALSE;

	return ( m_bufResp.StartTag(cszResponse) &&
	         SetHREFTagFromDestinationPath(NULL,NULL,FALSE,szRootURL,TRUE)  &&
	         m_bufResp.SetStatusTag(rs)      &&
	         m_bufResp.EndTag(cszResponse));
}

// Stub functions to call into class
BOOL SendPropertiesVisitFcn(CWebDav *pThis, WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot) {
	return pThis->SendPropertiesVisitFcn(pFindData, dwContext, szPath, fIsRoot);
}

BOOL SendPropertyNamesVisitFcn(CWebDav *pThis, WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot) {
	return pThis->SendPropertyNamesVisitFcn(pFindData, dwContext, szPath, fIsRoot);
}

BOOL DeleteVisitFcn(CWebDav *pThis, WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot) {
	return pThis->DeleteVisitFcn(pFindData, dwContext, szPath, fIsRoot);
}

BOOL MoveCopyVisitFcn(CWebDav *pThis, WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot) {
	return pThis->MoveCopyVisitFcn(pFindData, dwContext, szPath, fIsRoot);
}


inline WCHAR * RemoveLastSlash(WCHAR *szBuffer) {
	WCHAR *wszEnd = wcsrchr(szBuffer,L'\\');

	// In case where szBuffer='\\', don't remove root slash
	if (wszEnd == szBuffer)
		wszEnd = szBuffer+1;
	
	*wszEnd = 0;
	return wszEnd;
}

typedef struct {
	WIN32_FIND_DATAW findData;
} RecurseStructInfo;


BOOL CWebDav::RecursiveVisitDirs(WCHAR *szPath, DWORD dwContext, PFN_RECURSIVE_VISIT pfnVisit, BOOL fDeferDirectoryProcess) {
	WIN32_FIND_DATAW findData;
	BOOL      fRet = FALSE;
	BOOL      fMoreFiles = TRUE;

	WCHAR     wszBuf1[(10+MAX_PATH)*2];
	WCHAR     *wszEnd;
	DWORD     ccBuf1;

	// keeps track of recursive decent through directory tree
	SVSExpArray<RecurseStructInfo> findDataArray;
	HANDLE    hFileStack[MAX_PATH];
	int       iStackPtr = -1;
	BOOL      fIsRoot;

	wcscpy(wszBuf1,szPath);
	ccBuf1 = wcslen(wszBuf1);
	wszEnd = wszBuf1 + ccBuf1;

	if (URLHasTrailingSlashW(wszBuf1,ccBuf1))
		wcscpy(wszEnd,L"*");
	else
		wcscpy(wszEnd,L"\\*");

	if (INVALID_HANDLE_VALUE == (hFileStack[0] = FindFirstFile(wszBuf1, &findData))) {
		// root directory is empty.  Not an error, but we are done in this case.
		fRet = TRUE;
		goto done;
	}
	*wszEnd = 0;
	iStackPtr = 0;

	GetDepth();

	do {
startFileList:
		DEBUGCHK((iStackPtr >= 0) && (iStackPtr < SVSUTIL_ARRLEN(hFileStack)));
		fIsRoot = (iStackPtr==0);

		do {
			if (!fMoreFiles)
				break;

			if (IsDirectory(&findData) && fDeferDirectoryProcess) {
				if (!findDataArray.SRealloc(iStackPtr+1))
					goto done;

				findDataArray[iStackPtr].findData = findData;
			}
			else if (! pfnVisit(this,&findData,dwContext,wszBuf1,fIsRoot))
				goto done;

			if (IsDirectory(&findData) && (m_Depth == DEPTH_INFINITY)) {
				if (!URLHasTrailingSlashW(wszBuf1))
					*wszEnd++ = '\\';

				wcscpy(wszEnd,findData.cFileName);
				wszEnd = wszEnd + wcslen(findData.cFileName);
				wcscpy(wszEnd,L"\\*");
				DEBUGCHK(wcslen(wszBuf1) < SVSUTIL_ARRLEN(wszBuf1));

				// If call fails, continue processing.
				if (INVALID_HANDLE_VALUE == (hFileStack[++iStackPtr] = FindFirstFile(wszBuf1, &findData))) {
					*wszEnd = 0;
					iStackPtr--;

					if (fDeferDirectoryProcess) {
						if (! pfnVisit(this,&findDataArray[iStackPtr].findData,dwContext,wszBuf1,fIsRoot))
							goto done;
					}

					wszEnd = RemoveLastSlash(wszBuf1);
					continue;
				}
				*wszEnd = 0;
				goto startFileList;
			}
		}
		while (FindNextFile(hFileStack[iStackPtr], &findData));

		if (fDeferDirectoryProcess && !fIsRoot) {
			DEBUGCHK(iStackPtr >= 1);
			DEBUGCHK(IsDirectory(&findDataArray[iStackPtr-1].findData));
			if (! pfnVisit(this,&findDataArray[iStackPtr-1].findData,dwContext,wszBuf1,fIsRoot))
				goto done;
		}

		wszEnd = RemoveLastSlash(wszBuf1);

		FindClose(hFileStack[iStackPtr--]);

		if (iStackPtr == -1)
			break;

		fMoreFiles = FindNextFile(hFileStack[iStackPtr], &findData);
	} while (1);

	fRet = TRUE;
done:
	for (int i = 0; i <= iStackPtr; i++) {
		DEBUGCHK(hFileStack[i] != INVALID_HANDLE_VALUE);
		FindClose(hFileStack[i]);
	}

	return fRet;
}


//
// CChunkedBuffer implementation
//

// Initial buffer is 8KB, second one is 16 KB, then 32, then 64KB.  We grow quickly
// because most likely scenarios are the tiny case (< 8KB) or the very large (>64KB), 
// so there's no point in reallocating 1 or 2 KB at a time.

// Only alloc on specified boundaries
inline DWORD GetNextBufSize(DWORD cbRequired) {
	if (cbRequired <= INITIAL_CHUNK_BUFFER_SIZE)
		return INITIAL_CHUNK_BUFFER_SIZE;
	else if (cbRequired <= (INITIAL_CHUNK_BUFFER_SIZE*2))
		return INITIAL_CHUNK_BUFFER_SIZE*2;
	else if (cbRequired <= (INITIAL_CHUNK_BUFFER_SIZE*4))
		return INITIAL_CHUNK_BUFFER_SIZE*4;
	else
		return MAX_CHUNK_BUFFER_SIZE;
}

BOOL CChunkedBuffer::AllocMem(DWORD cbSize) {
	BufferConsistencyChecks();
	DWORD dwAlloc = cbSize + m_iNextIn + CHUNK_RESERVE_SPACE;
	
	if (!m_pszBuf) {
		m_iSize = GetNextBufSize(dwAlloc);
		if (NULL == (m_pszBuf  = MyRgAllocNZ(char,m_iSize)))
			return FALSE;

		m_iNextIn = CHUNK_PREAMBLE;
	}
	else if (((DWORD)(m_iSize - m_iNextIn) <= cbSize) && (m_iSize != MAX_CHUNK_BUFFER_SIZE)) {
		dwAlloc = GetNextBufSize(dwAlloc);
		PSTR szTempBuf = (PSTR) g_funcRealloc(m_pszBuf,dwAlloc,g_pvAllocData);

		if (!szTempBuf)
			return FALSE;

		m_iSize  = dwAlloc;
		m_pszBuf = szTempBuf;
	}

	BufferConsistencyChecks();
	return TRUE;
}


// GetEndOfFileWithNoSlashes returns the last portion of URL between last '/'
// and optionally trailing '/' (or '\').
// Given the URL foobar/myfile.txt, this function will set szStart=myfile.txt 
// and not modify ppszSave or pcSave.
// Given URL foobar/dirName/, this function will set *ppszStart=dirName, set
// *ppszSave=0 (will be closing '/') and *pcSave=/.

void GetEndOfFileWithNoSlashes(PSTR *ppszStart, PSTR *ppszSave, CHAR *pcSave) {
	DEBUGCHK((*ppszSave) == NULL);
	DEBUGCHK(*pcSave == 0);

	PSTR pszTrav = *ppszStart;
	PSTR pszLastSlash         = NULL;
	PSTR pszSecondToLastSlash = NULL;

	if (strlen(pszTrav) == 1) // special case, len 1
		return;

	while (*pszTrav) {
		if (*pszTrav == '/' || *pszTrav == '\\') {
			pszSecondToLastSlash = pszLastSlash;
			pszLastSlash = pszTrav;
		}

		pszTrav++;
	}
	DEBUGCHK(pszLastSlash);

	// A slash was last char before NULL
	if ((pszTrav - pszLastSlash) == 1) {
		*pcSave = *pszLastSlash;
		*ppszSave = pszLastSlash;
		*pszLastSlash = 0;

		DEBUGCHK(pszSecondToLastSlash);
		if (pszSecondToLastSlash)
			*ppszStart = pszSecondToLastSlash+1;
	}
	else {
		if (pszLastSlash)
			*ppszStart = pszLastSlash+1;
	}
}


//
//  Response buffer implementation routines.
//

const CHAR rg_XmlCharacters[]        = {'<', '>', '\'', '"', '&'};
const CHAR *rgsz_EntityReferences[]  = {"&lt;", "&gt;", "&apos;", "&quot;", "&amp;"};
const DWORD dwNumReferences          = SVSUTIL_ARRLEN(rg_XmlCharacters);


BOOL CXMLBuffer::Encode(PCSTR pszData) {
	// Translate pszData into encoded XML.  We have to call
	// CChunkedBuffer::Append every bit individually because we may
	// have to send across a chunked response at any time.

	PSTR  pszTrav = (PSTR)pszData;
	DWORD i;

	while (*pszTrav) {
		for (i = 0; i < dwNumReferences; i++) {
			if (rg_XmlCharacters[i] == *pszTrav) {
				if (! Append(rgsz_EntityReferences[i],strlen(rgsz_EntityReferences[i])))
					return FALSE;
				break;
			}
		}

		if ((*pszTrav == '\r') && (*(pszTrav+1) == '\n')) {
			if (! Append(pszTrav,2))
				return FALSE;

			pszTrav++; // skip past '\n'
		}
		if (*pszTrav == '\n') {
			if (! Append("\r\n",2))
				return FALSE;
		}
		else if (i == dwNumReferences) {
			if (! Append(pszTrav,1))
				return FALSE;
		}

		pszTrav++;
	}
	return TRUE;
}

/*
// This is original table used to determine whether or not we need to encode, from httplite.
// There is no need for 1 bit worth of information to take up a byte of ROM, so this
// table is further compressed in isAcceptable array below, where 2 rows take up one entry.
// Note below that we reverse order of bits for least significant bit.
const CHAR isAcceptableExpanded[] = 
    {0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,    // 2x   !"#$%&'()*+,-./  
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0,    // 3x  0123151189:;<=>?  
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    // 4x  @ABCDEFGHIJKLMNO  
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1,    // 5X  PQRSTUVWXYZ[\]^_  
     0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    // 6x  `abcdefghijklmno  
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0};   // 7X  pqrstuvwxyz{|}~  DEL 
*/

const DWORD isAcceptable[] = {  
	0x2FFFFF92, // 00101111111111111111111110010010
	0xAFFFFFFF, // 10101111111111111111111111111111
	0x47FFFFFE, // 01000111111111111111111111111110
};

BOOL RequiresEncode(CHAR c) {
	if ((c >= 32) && (c <128)) {
		int i = (c & 0x60) >> 6; // rip out 6th and 7th bits
		int m = c & 0x1F;
		DEBUGCHK((i >= 0) && (i <= SVSUTIL_ARRLEN(isAcceptable)));

		return ! (isAcceptable[i] & (1 << m));
	}
	return TRUE;
}

BOOL CBuffer::EncodeURL(PCSTR pszData) {
	PSTR  pszTrav;
	CHAR  szBuf[3];

	szBuf[0] = HEX_ESCAPE;

	for (pszTrav = (PSTR)pszData; *pszTrav; pszTrav++) {
		CHAR c = *pszTrav;

		if (RequiresEncode(c)) {
			szBuf[1] = g_szHexDigits[c >> 4];
			szBuf[2] = g_szHexDigits[c & 15];
		
			if (! AppendData(szBuf,3))
				return FALSE;
		}
		else if (! AppendCHAR(*pszTrav))
			return FALSE;
	}
	return TRUE;
}

BOOL CXMLBuffer::EncodeURL(PCSTR pszData) {
	PSTR  pszTrav;
	CHAR  szBuf[3];

	szBuf[0] = HEX_ESCAPE;

	for (pszTrav = (PSTR)pszData; *pszTrav; pszTrav++) {
		CHAR c = *pszTrav;

		if (RequiresEncode(c)) {
			szBuf[1] = g_szHexDigits[c >> 4];
			szBuf[2] = g_szHexDigits[c & 15];
		
			if (! Append(szBuf,3))
				return FALSE;
		}
		else if (! AppendCHAR(*pszTrav))
			return FALSE;
	}
	return TRUE;
}

BOOL CChunkedBuffer::Append(PCSTR pszData, DWORD ccData) {
	DEBUGCHK(!m_fFinalDataSent);

	if (!AllocMem(ccData))
		return FALSE;

	while ((m_iNextIn+ccData) >= MAX_CHUNK_BODY_SIZE) {
		DWORD ccCopy = MAX_CHUNK_BODY_SIZE-m_iNextIn;
		memcpy(m_pszBuf+m_iNextIn,pszData,ccCopy);
		ccData    -= ccCopy;
		pszData   += ccCopy;
		m_iNextIn += ccCopy;

		if (!FlushBuffer())
			return FALSE;
	}

	memcpy(m_pszBuf+m_iNextIn,pszData,ccData);
	m_iNextIn += ccData;
		
	BufferConsistencyChecks();
	return TRUE;
}


// FlushBuffer sends data across to client, does appropriate munging with chunk
// lengths.  If fFinalChunk=TRUE, then it will close off buffer as far as client knows.
BOOL CChunkedBuffer::FlushBuffer(BOOL fFinalChunk) {
	BufferConsistencyChecks();

	if (! AllocMem(CHUNK_RESERVE_SPACE))
		return FALSE;

	// Where to position start of send buffer depends on number of
	// bytes required to encode the content-length.
	DWORD  ccBodyLen = m_iNextIn - CHUNK_PREAMBLE;
	DWORD  ccOffset;
	DWORD  cbSend;
	BOOL   fRet;

	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: WebDav FlushBuffer called for chunked buffer send\r\n"));
	DEBUGMSG(ZONE_WEBDAV_VERBOSE,(L"HTTPD: FlushBuffer fFinalChunk=%d,ccBodyLen=%d\r\n",fFinalChunk,ccBodyLen));

	DEBUGCHK(ccBodyLen <= MAX_CHUNK_BODY_SIZE);
	DEBUGCHK(m_pszBuf);
	DEBUGCHK(!m_fFinalDataSent);

	if (m_pRequest->m_fResponseHTTP11) {
		// Calculate offsets for chunked encoding
		if (ccBodyLen <= 0xF)
			ccOffset = CHUNK_PREAMBLE - 3; // -2 for CRLF, -1 to store number
		else if (ccBodyLen <= 0xFF)
			ccOffset=  CHUNK_PREAMBLE - 4; // -2 for CRLF, -2 to store number
		else if (ccBodyLen <= 0xFFF)
			ccOffset = CHUNK_PREAMBLE - 5; // -2 for CRLF, -3 to store number
		else
			ccOffset = 0; // need all 4 bytes to store the number

		DWORD dwSprintf = sprintf(m_pszBuf+ccOffset,"%x",ccBodyLen);
		DEBUGCHK(dwSprintf == (CHUNK_PREAMBLE-2-ccOffset));

		// Last two bytes before body will always be CRLF
		m_pszBuf[CHUNK_PREAMBLE-2] = '\r';
		m_pszBuf[CHUNK_PREAMBLE-1] = '\n';

		cbSend = ccBodyLen + (CHUNK_PREAMBLE-ccOffset);

		// Last two bytes after body will always be CRLF
		m_pszBuf[m_iNextIn++] = '\r';
		m_pszBuf[m_iNextIn++] = '\n';
		cbSend += 2;

		if (fFinalChunk) {
			WriteCloseChunk(m_pszBuf,m_iNextIn);
			cbSend += CHUNK_CLOSE_LEN;
		}
	}
	else {
		// In HTTP 1.0 case, assume client doesn't know about chunked-encoding so send back regular.
		ccOffset = CHUNK_PREAMBLE;
		cbSend   = ccBodyLen;

		if (m_pRequest->m_fKeepAlive && fFinalChunk && !m_fSentFirstChunk) {
			// If 1.0 client wants keep-alives and this is our last chunk, then 
			// we know lenghth of body and can do a keep-alive
			CHttpResponse resp(m_pRequest, m_pRequest->GetStatus());
			resp.SetLength(cbSend);
			resp.SendHeaders(NULL,NULL);
		}
		else if (!m_fSentFirstChunk) {
			// We haven't sent body back yet, and either no keep-alives by client request 
			// and/or this isn't final chunk, then no keepalives.
			CHttpResponse resp(m_pRequest, m_pRequest->GetStatus(),CONN_CLOSE);
			m_pRequest->m_fKeepAlive = FALSE;
			resp.SendHeaders(NULL,NULL);
		}
		else {
			// We've already sent first part of data in no-keepalive case.
			DEBUGCHK(m_fSentFirstChunk);
			DEBUGCHK(m_pRequest->m_fKeepAlive == FALSE);
		}

		m_fSentFirstChunk = TRUE;
	}

	DEBUGCHK(cbSend < MAX_CHUNK_BUFFER_SIZE);
	DEBUGCHK(cbSend < m_iSize);

	fRet = m_pRequest->SendData(m_pszBuf+ccOffset,cbSend);

	m_iNextIn = CHUNK_PREAMBLE;
	BufferConsistencyChecks();
	return fRet;
}

// Called when an error has occured.  Ignore any data in buffer already, send back
// code to indicate we're done sending.
BOOL CChunkedBuffer::AbortiveError(void) {
	CHAR szBuf[CHUNK_POSTAMBLE];

	BufferConsistencyChecks();
	DEBUGCHK(!m_fFinalDataSent);

	m_iNextIn = CHUNK_PREAMBLE;

	WriteCloseChunk(szBuf,0);
	return m_pRequest->SendData(szBuf,CHUNK_CLOSE_LEN);
}


void CChunkedBuffer::WriteCloseChunk(PSTR szBuf, DWORD dwOffset) {
#if defined (DEBUG)
	m_fFinalDataSent = TRUE;
#endif

	m_pszBuf[dwOffset++] = '0';
	m_pszBuf[dwOffset++] = '\r';
	m_pszBuf[dwOffset++] = '\n';
	m_pszBuf[dwOffset++] = '\r';
	m_pszBuf[dwOffset++] = '\n';
}


const char  cszPrefixBraceOpenD[]         = "<d:";
const DWORD ccPrefixBraceOpenD            = SVSUTIL_CONSTSTRLEN(cszPrefixBraceOpenD);

BOOL SetEmptyTagWin32NS(CXMLBuffer *pBuffer, PCSTR pszElement, DWORD ccElement) {
	return (pBuffer->Append(cszPrefixBraceOpenD,ccPrefixBraceOpenD) &&
	        pBuffer->Append(pszElement,ccElement)                   &&
	        pBuffer->Append("/>",2));
}

BOOL SetTagWin32NS(CXMLBuffer *pBuffer, PCSTR pszName, DWORD ccName, PCSTR szValue) {
	return (pBuffer->Append(cszPrefixBraceOpenD,ccPrefixBraceOpenD) &&
	        pBuffer->Append(pszName,ccName)                         &&
	        pBuffer->AppendCHAR('>')                                &&
	        pBuffer->Append(szValue,strlen(szValue))                &&
	        pBuffer->Append("</d:",4)                               &&
	        pBuffer->Append(pszName,ccName)                         &&
	        pBuffer->CloseBrace());
}


