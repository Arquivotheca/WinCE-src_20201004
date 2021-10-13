//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: webdav.h
Abstract: Web Dav declarations
--*/


//
// Constants and enumerations
//

typedef enum {
	RT_NULL = 0,
	RT_DOCUMENT,
	RT_STRUCTURED_DOCUMENT,
	RT_COLLECTION
} RESOURCE_TYPE;

typedef enum {
	DEPTH_UNKNOWN = 0,
	DEPTH_ZERO,
	DEPTH_ONE,
	DEPTH_INFINITY,
	DEPTH_ONE_NOROOT,
	DEPTH_INFINITY_NOROOT,
} DEPTH_TYPE;

typedef enum {
	OVERWRITE_NO      = 0x0,
	OVERWRITE_YES     = 0x1,
	OVERWRITE_RENAME  = 0x2
} OVERWRITE_TYPE;



#define TokensEqualA(s1,s2,c1,c2)  (((int)(c1)==(int)(c2)) && (0==strncmp((s1),(s2),(c1))))
#define TokensEqualAI(s1,s2,c1,c2) (((c1)==(c2)) && (0==_strnicmp((s1),(s2),(c1))))

// When using const data, assume there's a cszStrName and corresponding ccStrName
#define TokensEqualAC(s1, StrName, c1)   TokensEqualA(s1,csz##StrName, c1, cc##StrName)

#define TokensEqualW2(s1,s2,c1,c2) (((c1)==(c2)) && (0==wcsncmp((s1),(s2),(c1))))
#define TokensEqualW(s1,s2,c1)     TokensEqualW2(s1,s2,c1,SVSUTIL_CONSTSTRLEN(s2))

#define DEBUG_CHECK_ERRCODE(fRet)  DEBUGCHK((fRet && IS_STATUS_2XX(m_pRequest->m_rs)) || (!fRet && !IS_STATUS_2XX(m_pRequest->m_rs)))


// 
//  Chunked Body Response Buffer
//

// How large to allocate on.  Each initial alloc will be two times greater.
#define INITIAL_CHUNK_BUFFER_SIZE      0x2000
// This is the largest the actual body (non-chunked pieces) can grow to.
#define MAX_CHUNK_BODY_SIZE            0xFFEE
// reserve first 6 bytes of the buffer for length of the chunked encode (4 bytes) and for CRLF (2 bytes).
#define CHUNK_PREAMBLE                 6
// How many bytes to set '0CRLFCRLF'
#define CHUNK_CLOSE_LEN                5
// reserve 7 extra bytes for closing of buffer.  2 for CRLF at end of each chunk, 5 for the final "0CRLFCRLF" that signifies we're done sending.
#define CHUNK_POSTAMBLE                (CHUNK_CLOSE_LEN+2)
// How much extra space to reserve for the buffer
#define CHUNK_RESERVE_SPACE            (CHUNK_PREAMBLE+CHUNK_POSTAMBLE)
// Largest buffer can grow to that holds all this stuff
#define MAX_CHUNK_BUFFER_SIZE          (INITIAL_CHUNK_BUFFER_SIZE*8)


class CChunkedBuffer {
private:
	// We use chunked-encoding on certain responses for optimization.
	// When the buffer goes above MAX_CHUNK_BODY_SIZE, we immediatly send response.

	// NOTE: these variables should *never* be accessed from functions outside this class,
	//       as there are a number of idiosyncracies in maintaining a chunked buffer.
	//       Always use accessors!

	char         *m_pszBuf;  // pointer to allocated mem
	DWORD        m_iNextIn;  // next character to read from
	DWORD        m_iSize;    // size of buffer
protected:
	CHttpRequest *m_pRequest;
	BOOL         m_fSentFirstChunk;       // Have we sent our first body response to client yet?

	void       WriteCloseChunk(PSTR szBuf, DWORD dwOffset);
public:
	CChunkedBuffer(CHttpRequest *pR) {
		memset(this,0,sizeof(*this));
		m_pRequest = pR;
	}

	~CChunkedBuffer() {
		FinalBufferConsistencyCheck();
	
		if (m_pszBuf)
			g_funcFree(m_pszBuf,g_pvFreeData);
	}

	BOOL AllocMem(DWORD cbSize);
	BOOL Append(PCSTR pszData, DWORD ccData);
	BOOL FlushBuffer(BOOL fFinalChunk=FALSE);
	BOOL AbortiveError(void);

	BOOL AppendCHAR(const CHAR cAppend) {
		return Append((char *)&cAppend,sizeof(CHAR));
	}

	BOOL IsEmpty(void) {
		return ((m_pszBuf==NULL) && (m_iSize==0));
	}

#if defined (DEBUG)
	BOOL       m_fFinalDataSent;  // will be set true once we're done sending
	
	void BufferConsistencyChecks(void) {
		if (m_pszBuf)
			DEBUGCHK(m_iNextIn && m_iSize);
		else
			DEBUGCHK(!m_iNextIn && !m_iSize);

		DEBUGCHK(m_iNextIn <= m_iSize);

		if (m_iNextIn)
			DEBUGCHK(m_iNextIn >= CHUNK_PREAMBLE);

		// buffer is allocated on predefined boundaries.
		switch (m_iSize) {
			case 0:
			case INITIAL_CHUNK_BUFFER_SIZE:
			case (INITIAL_CHUNK_BUFFER_SIZE*2):
			case (INITIAL_CHUNK_BUFFER_SIZE*4):
			case MAX_CHUNK_BUFFER_SIZE:
				break;

			default:
				DEBUGCHK(0);
		}
	}

	// Make sure that we've flushed the buffer.
	void FinalBufferConsistencyCheck(void) {
		BufferConsistencyChecks();
		DEBUGCHK(m_iNextIn == 0 || m_iNextIn == CHUNK_PREAMBLE);
		if (m_iNextIn)
			DEBUGCHK(!m_pRequest->m_fResponseHTTP11 || m_fFinalDataSent);
	}
#else
	void BufferConsistencyChecks(void)     { ; }
	void FinalBufferConsistencyCheck(void) { ; }
#endif // DEBUG

};

extern const char  cszPrefixBraceOpen[];
extern const DWORD ccPrefixBraceOpen;
extern const char  cszPrefixBraceEnd[];
extern const DWORD ccPrefixBraceEnd;

class CXMLBuffer : public CChunkedBuffer {

public:
	CXMLBuffer(CHttpRequest *pR) : CChunkedBuffer(pR) { 
		; 
	}
	~CXMLBuffer() { ; }

	BOOL Encode(PCSTR pszData);
	BOOL EncodeURL(PCSTR pszData);

	BOOL BeginBrace(void) {
		return Append(cszPrefixBraceOpen,ccPrefixBraceOpen);
	}

	BOOL CloseBrace(void) {
		return AppendCHAR('>');
	}

	BOOL EndBrace(void) {
		return Append(cszPrefixBraceEnd,ccPrefixBraceEnd);
	}

	BOOL BeginBraceNoNS(void) {
		return AppendCHAR('<');
	}

	BOOL StartTag(const CHAR *szElementName)  {
		if (!BeginBrace() ||
		    !Encode(szElementName) ||
		    !CloseBrace())
			return FALSE;
		return TRUE;
	}

	BOOL StartTagNoEncode(const CHAR *szElementName, int ccElementName=-1) {
		if (!BeginBrace() ||
		    !Append(szElementName,(ccElementName==-1) ? strlen(szElementName) : ccElementName) ||
		    !CloseBrace())
			return FALSE;
		return TRUE;
	}
	
	BOOL EndTag(const CHAR *szElementName) { 
		if (!EndBrace() ||
		    !Encode(szElementName) ||
		    !CloseBrace() )
			return FALSE;
		return TRUE;
	}

	BOOL SetCRLF(void) {
		return Append("\r\n",2);
	}

	BOOL SetEmptyElement(const CHAR *szElementName) {
		if (!BeginBrace() ||
		    !Encode(szElementName) ||
		    !Append("/>",2))
			return FALSE;
		return TRUE;
	}

	BOOL SetEmptyElementNoEncode(const CHAR *szElementName, int ccElementName=-1) {
		if (!BeginBrace() ||
		    !Append(szElementName,(ccElementName==-1) ? strlen(szElementName) : ccElementName) ||
		    !Append("/>",2))
			return FALSE;
		return TRUE;
	}

	// Setting tags in an 207 multistatus XML blob
	BOOL SetStatusTag(RESPONSESTATUS rs);
};

#define RECEIVE_SIZE_UNINITIALIZED   ((DWORD)-1)

// For keeping track of receiving POST data.
class CReceiveBody {
public:
	BOOL  fChunked;
	// length of unread portion of current chunk, or length of remaining POST body in non-chunked.
	DWORD dwRemaining;

	CReceiveBody() {
		dwRemaining = RECEIVE_SIZE_UNINITIALIZED;
	}

	~CReceiveBody() { ; }
} ;


// For recursing through directory structure.
class CWebDav;
class CWebDavFileNode;

typedef BOOL (WINAPI *PFN_RECURSIVE_VISIT)(CWebDav *pThis, WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot);

BOOL SendPropertiesVisitFcn    (CWebDav *pThis, WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot);
BOOL SendPropertyNamesVisitFcn (CWebDav *pThis, WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot);
BOOL DeleteVisitFcn            (CWebDav *pThis, WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot);
BOOL MoveCopyVisitFcn          (CWebDav *pThis, WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot);


// When recursing through directories performing a move or copy, info to keep 
// our place in the stack during calls to MoveCopyVisitFcn().
class CMoveCopyContext { 
public:
	DWORD dwSrcPermFlags;  // HSE_URL_FLAGS_xxx perms for source VRoot
	DWORD dwDestPermFlags; // HSE_URL_FLAGS_xxx perms for destination VRoot

	PSTR  szDestURL;       // canonicalized destination URL
	WCHAR *wszDestPath;    // root of physical path to write data to.
	DWORD ccDestPath;      // number of WCHARs in dest physical path
	DWORD ccSrcPath;       // number of WCHARs in source physical path
	BOOL  fDeleteSrc;      // copy or move files?

	CMoveCopyContext(BOOL fDelSrc, DWORD dwSrc, DWORD dwDest, PSTR szURL, WCHAR *wszPath, DWORD ccSrcP) {
		dwSrcPermFlags   = dwSrc;
		dwDestPermFlags  = dwDest;
		szDestURL        = szURL;
		wszDestPath      = wszPath;
		ccDestPath       = wcslen(wszDestPath);
		ccSrcPath        = ccSrcP;
		fDeleteSrc       = fDelSrc;
	}

	~CMoveCopyContext() {
		// do not free string resources, they are pointers to static buffers in calling function.
	}
};

// When recursing through directories on a delete, options info
class CDavDeleteContext {
public:
	CHAR *szURL;
	BOOL fSendErrorsToClient;

	CDavDeleteContext(CHAR *sz, BOOL f) { 
		szURL = sz;
		fSendErrorsToClient = f;
	}
};

typedef SVSExpArray<__int64> LockIdArray;

//
//  Main WebDAV class
//
class CWebDav {
friend class CWebDavFileLockManager;

public:
	CHttpRequest     *m_pRequest;             // request class
private:
	RESOURCE_TYPE    m_rt;                    // are we a directory, file, or a non-existant resource?
	DEPTH_TYPE       m_Depth;                 // "Depth:" HTTP header
	DWORD            m_Overwrite;             // "overwrite: " HTTP header
	CXMLBuffer       m_bufResp;               // Response class that generates chunked HTTP and encodes XML data.
	SVSSimpleBuffer  m_bufUnknownTags;        // PROPFIND or PROPPATCH tags that WinCE doesn't support are stored in a NULL separeted list of strings to be sent back later to client.
	CHAR             m_szHostName[MAX_PATH];  // cache the host name to only call gethostname() once
	DWORD            m_ccHostName;            // length of m_szHostName
	CReceiveBody     m_rcvBody;               // Context used when calling WriteFile during a PUT
	BOOL             m_fSetStatus;            // Have we sent "207" headers already?
public:
	BOOL             m_fXMLBody;              // are we sending XML?

	DWORD            m_dwPropPatchUpdate;     // fields that have been updated during PROPPATCH.

private:
	WIN32_FILE_ATTRIBUTE_DATA m_fileAttribs;            // cache attribs to only call GetFileAttributesEx(m_pRequest->m_wszPath,...) once.
	BOOL                      m_fRetrievedFileAttribs;  // have we called GetFileAttributesEx() yet?
	LockIdArray               m_lockIds;                // store lock tokens
	int                       m_nLockTokens;            // number of lock tokens we've parsed from (if: XXX field)

	// ******************************************************
	// Utility functions
	// ******************************************************
	BOOL IsGLELocked(void) {
		



		DWORD dw = GetLastError();
		return ((dw == ERROR_SHARING_VIOLATION) || (dw == ERROR_ACCESS_DENIED));
	}

	// Is file locked and if so, do we have tokens corresponding to it?
	BOOL CheckLockTokens(void)       { 
		return (m_nLockTokens && IsGLELocked()); 
	}

	BOOL SetMultiStatusResponse(void) { return SetStatusAndXMLBody(STATUS_MULTISTATUS); }
	BOOL SetMultiStatusErrorFromDestinationPath(RESPONSESTATUS rs, WIN32_FIND_DATA *pFindData, WCHAR *szDirectory, BOOL fRootDir, PCSTR szRootURL=NULL,BOOL fSkipFileMunge=FALSE);
	BOOL SetMultiStatusErrorFromURL(RESPONSESTATUS rs, PCSTR szRootURL);

	BOOL GetDepth(DEPTH_TYPE depthDefault=DEPTH_INFINITY);
	void GetOverwrite(void);
	BOOL AddContentLocationHeaderIfNeeded(void);
	BOOL AddContentLocationHeader(void);

	BOOL BuildBaseURLFromSourceURL(BOOL fHttpHeader=FALSE);
	BOOL BuildBaseURL(PCSTR szRoot, BOOL fHttpHeader, BOOL fAppendSlash);
	BOOL SetHREFTagFromDestinationPath(WIN32_FIND_DATA *pFindData, WCHAR *szDirectory, BOOL fRootDir, PCSTR szRootURL=NULL, BOOL fSkipFileMunge=FALSE);
	BOOL RecursiveVisitDirs(WCHAR *szPath, DWORD dwContext, PFN_RECURSIVE_VISIT pfnVisit, BOOL fDeferDirectoryProcess);

	BOOL DavGetFileAttributesEx(void);
	BOOL SendLockErrorInfoIfNeeded(WCHAR *szPath);
	void SendLockedConflictOrErr(WCHAR *szPath, WCHAR *szAlternatPath=NULL);

	// ******************************************************
	// Routines tightly bound to VERB implementation
	// ******************************************************

	// PROPFIND
public:
	BOOL SendProperties(DWORD dwDavPropFlags,BOOL fSendNamesOnly);
	BOOL AddUnknownXMLElement(const WCHAR *wszTagName, const DWORD ccTagName);
private:
	BOOL SendSubsetOfProperties(void);

	// PROPPATCH
	BOOL SendPropPatchResponse(RESPONSESTATUS rs);
	BOOL SendUnknownXMLElementsIfNeeded(RESPONSESTATUS rs);

	// OPTIONS
	void WriteSupportedVerb(VERB v, BOOL fFirstVerbInList=FALSE);
	void WritePublicOptions(void);

	// PROPPUT
	BOOL WriteBodyToFile(HANDLE hFile, BOOL fChunked);
	BOOL ReadNextBodySegment(PSTR szBuf, DWORD *pcbRead);

	// DELETE
	BOOL DeleteCollection(WCHAR *szPath, CHAR *szURL);

	// MOVE+COPY
	BOOL MoveCopyResource(BOOL fDeleteSrc);
	PSTR GetDestinationHeader(PSTR *ppszDestinationPrefix, PSTR *ppszDestinationURL, PSTR *ppszSave);
	BOOL HasAccessToDestVRoot(CHAR *szDestURL, WCHAR *szPath, PVROOTINFO pVRoot);
	BOOL MoveCopyCollection(CMoveCopyContext *pcContext);
	BOOL MoveCopyFile(BOOL fDeleteSrc, WCHAR *wszSrcPath, WCHAR *wszDestPath);

	// LOCK+UNLOCK
	BOOL CheckIfHeaders(void);
	BOOL CheckLockHeader(void);
	__int64 CheckLockTokenHeader(void);
	BOOL CheckTokenExpression(PSTR szToken, WCHAR *szPath);
	BOOL CheckETagExpression(PSTR szToken, WCHAR *szPath);
	BOOL SendLockTags(CWebDavFileNode *pLockedNode, BOOL fPrintPropTag=TRUE);

	BOOL GetLockTimeout(DWORD *pdwTimeout);

	// When building up URLs based on current, do we do http:// or https:// ?
	PCSTR GetHttpPrefix()     { return m_pRequest->IsSecure() ? cszHttpsPrefix : cszHttpPrefix ; }
	DWORD GetHttpPrefixLen()  { return m_pRequest->IsSecure() ? ccHttpsPrefix  : ccHttpPrefix; }

	BOOL  IsContentTypeXML(void);
	BOOL  HasServerReadAllData(void) { return (m_pRequest->m_bufRequest.Count() == m_pRequest->m_dwContentLength); }
	
	RESPONSESTATUS VerifyXMLBody(void);
	BOOL ParseXMLBody(ISAXContentHandler *pParser, BOOL fSetDefaultStatus=TRUE);

	BOOL SetHREFTag(PCSTR szURL=NULL);
	BOOL SetResponseTag(PCSTR szURL, RESPONSESTATUS rs);

	// Wrap filesystem calls in event we need to unlock
	BOOL DavDeleteFile(WCHAR *szFile);

public:
	CWebDav(CHttpRequest *pRequest) : m_bufResp(pRequest) {
		m_pRequest = pRequest;

		if (pRequest->m_dwFileAttributes == (-1))
			m_rt = RT_NULL;
		else if (IsDirectory(pRequest->m_dwFileAttributes))
			m_rt = RT_COLLECTION;
		else
			m_rt = RT_DOCUMENT;

		m_Depth           = DEPTH_UNKNOWN;
		m_ccHostName      = 0;
		m_fSetStatus      = FALSE;
		m_fXMLBody        = FALSE;
		m_nLockTokens     = 0;
		m_fRetrievedFileAttribs = FALSE;
		m_Overwrite = OVERWRITE_YES;

		m_dwPropPatchUpdate = 0;
	}

	~CWebDav() {
		;
	}

	void SetStatus(DWORD dw) {
		m_pRequest->m_rs = (RESPONSESTATUS) dw;
		m_fSetStatus = TRUE;
	}
	BOOL SetStatusAndXMLBody(RESPONSESTATUS rs);

	BOOL FinalizeMultiStatusResponse(void);

	void FinalConsistencyCheck(BOOL fCodeReturnedToWebServer) {
#if defined (DEBUG)
		m_bufResp.FinalBufferConsistencyCheck();

		// If we're not using multistatus XML response, then nothing 
		// better have written to the buffer (data isn't flushed in this case).
		if (!m_fXMLBody)
			DEBUGCHK(m_bufResp.IsEmpty());

		// If we return TRUE to HTTPD, then we better be a 2XX status code.
		// Similiarly, if we return FALSE we better not be a 2XX status.
		DEBUG_CHECK_ERRCODE(fCodeReturnedToWebServer);
#endif
	}

	// Simple accessors
	BOOL IsCollection(void)  { return (m_rt==RT_COLLECTION); }
	BOOL IsDocument(void)    { return (m_rt==RT_DOCUMENT);   }
	BOOL IsNull(void)        { return (m_rt==RT_NULL);       }
	BOOL HasBody(void)       { return (m_pRequest->m_dwContentLength != 0) ; }

	// recursive directory helpers
	BOOL SendPropertiesVisitFcn(WIN32_FIND_DATA *pFindData, DWORD dwContext, WCHAR *szPhysicalPath, BOOL fRootDir);
	BOOL SendPropertyNamesVisitFcn(WIN32_FIND_DATA *pFindData, DWORD dwContext, WCHAR *szTraversalPath, BOOL fRootDir);
	BOOL DeleteVisitFcn(WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot);
	BOOL MoveCopyVisitFcn(WIN32_FIND_DATAW *pFindData, DWORD dwContext, WCHAR *szPath, BOOL fIsRoot);

	// shared mode

	// HTTP VERB handeler routines
	// These functions return TRUE if they returned everything they needed
	// to to client, FALSE if they've set an error code that HandleRequest needs to 
	// process and send back.
	
	BOOL DavUnsupported(void);
	BOOL DavOptions(void);
	BOOL DavPut(void);
	BOOL DavMove(void);
	BOOL DavCopy(void);
	BOOL DavDelete(void);
	BOOL DavMkcol(void);
	BOOL DavPropfind(void);
	BOOL DavProppatch(void);
//	BOOL DavSearch(void);
	BOOL DavLock(void);
	BOOL DavUnlock(void);

#if 0
	BOOL DavSubscribe(void)
	BOOL DavUnsubscribe(void);
	BOOL DavPoll(void);
	BOOL DavBDelete(void);
	BOOL DavBCopy(void);
	BOOL DavBMove(void);
	BOOL DavBProppatch(void);
	BOOL DavBPropfind(void);
	BOOL DavX_MS_Enumatts(void);
#endif // 0

	//
	// Functions to process SAX data of selected nodes.  The functions return
	// HRESULTs because the value is directly returned to the SAX processor.
	//
	HRESULT ProcessWin32FileAttribs(SVSSimpleBuffer *pBuf);

	BOOL IsFileNameTooLongForFilesys(const WCHAR *szFile) {
		return (wcslen(szFile) > (MAX_PATH-1));
	}

	BOOL IsSrcFileNameLenTooLong(void) {
		return IsFileNameTooLongForFilesys(m_pRequest->m_wszPath);
	}

	BOOL IsFileLocked(const WCHAR *szPath);

	// When directory is non-empty, then deleting a file recursively failed, which means
	// that it set the error for us.
	BOOL SendErrorOnRemoveDirectoryFailure(void) {
		return (GetLastError() != ERROR_DIR_NOT_EMPTY);
	}
};


//
//  Utility functions
//

void GetEndOfFileWithNoSlashes(PSTR *ppszStart, PSTR *ppszSave, CHAR *pcSave);
PSTR SkipHTTPPrefixAndHostName(PSTR szURL);
BOOL IsPathSubDir(WCHAR *szSrc, DWORD ccSrc, WCHAR *szDest, DWORD ccDest);
void GetCurrentFileTime(FILETIME *pft);
void BuildFileName(WCHAR *szWriteBuf, WCHAR *szPath, WCHAR *szFileName);


BOOL SetEmptyTagWin32NS(CXMLBuffer *pBuffer, PCSTR pszElement, DWORD ccElement);
BOOL SetTagWin32NS(CXMLBuffer *pBuffer, PCSTR pszName, DWORD ccName, PCSTR szValue);

//
// DAV constant strings and #defines
//

// DAV XML strings
extern const CHAR  cszXMLVersionA[];
extern const DWORD ccXMLVersionA;
extern const CHAR  cszMultiStatusNS[];
extern const DWORD ccMultiStatusNS;
extern const CHAR  cszMultiStatus[];
extern const DWORD ccMultiStatus;
extern const CHAR  cszResponse[];
extern const DWORD ccResponse;
extern const CHAR  cszHref[];
extern const DWORD ccHref;
extern const CHAR  cszPropstat[];
extern const DWORD ccPropstat;
extern const CHAR  cszProp[];
extern const DWORD ccProp;
extern const CHAR  cszPropNS[];
extern const DWORD ccPropNS;
extern const CHAR  cszStatus[];
extern const DWORD ccStatus;
extern const CHAR  cszGetcontentLengthNS[];
extern const DWORD ccGetcontentLengthNS;
extern const CHAR  cszGetcontentLength[];
extern const DWORD ccGetcontentLength;
extern const CHAR  cszDisplayName[];
extern const DWORD ccDisplayName;
extern const CHAR  cszCreationDateNS[];
extern const DWORD ccCreationDateNS;
extern const CHAR  cszCreationDate[];
extern const DWORD ccCreationDate;
extern const CHAR  cszGetETag[];
extern const DWORD ccGetETag;
extern const CHAR  cszLastModifiedNS[];
extern const DWORD ccLastModifiedNS;
extern const CHAR  cszLastModified[];
extern const DWORD ccLastModified;
extern const CHAR  cszResourceType[];
extern const DWORD ccResourceType;
extern const CHAR  cszCollection[];
extern const DWORD ccCollection;
extern const CHAR  cszIsHiddenNS[];
extern const DWORD ccIsHiddenNS;
extern const CHAR  cszIsHidden[];
extern const DWORD ccIsHidden;
extern const CHAR  cszIsCollectionNS[];
extern const DWORD ccIsCollectionNS;
extern const CHAR  cszIsCollection[];
extern const DWORD ccIsCollection;
extern const CHAR  cszTextXML[];
extern const DWORD ccTextXML;
extern const CHAR  cszApplicationXML[];
extern const DWORD ccApplicationXML;
extern const CHAR  cszGetcontentType[];
extern const DWORD ccGetcontentType;

extern const CHAR  cszSupportedLock[];
extern const DWORD ccSupportedLock;
extern const CHAR  cszLockEntry[];
extern const DWORD ccLockEntry;

extern const CHAR  cszMSNamespace[];
extern const DWORD ccMSNamespace;
extern const CHAR  cszWin32FileAttribs[];
extern const DWORD ccWin32FileAttribs;
extern const CHAR  cszWin32LastAccessTime[];
extern const DWORD ccWin32LastAccessTime;
extern const CHAR  cszWin32LastModifiedTime[];
extern const DWORD ccWin32LastModifiedTime;
extern const CHAR  cszWin32CreationTime[];
extern const DWORD ccWin32CreationTime;

// LOCK XML strings
extern const CHAR  cszLockDiscovery[];
extern const DWORD ccLockDiscovery;
extern const CHAR  cszActiveLock[];
extern const DWORD ccActiveLock;
extern const CHAR  cszLockType[];
extern const DWORD ccLockType;
extern const CHAR  cszWrite[];
extern const DWORD ccWrite;
extern const CHAR  cszLockScope[];
extern const DWORD ccLockScope;
extern const CHAR  cszShared[];
extern const DWORD ccShared;
extern const CHAR  cszExclusive[];
extern const DWORD ccExclusive;
extern const CHAR  cszLockTokenTag[];
extern const DWORD ccLockTokenTag;
extern const CHAR  cszOwner[];
extern const DWORD ccOwner;
extern const CHAR  cszTimeoutTag[];
extern const DWORD ccTimeoutTag;
extern const CHAR  cszDepthTag[];
extern const DWORD ccDepthTag;

extern const CHAR  cszSecond[];
extern const DWORD ccSecond;
extern const CHAR  cszInfinite[];
extern const DWORD ccInfinite;

extern const CHAR  cszNot[];
extern const DWORD ccNot;


// DAV header data
extern const CHAR  csz0[];
extern const DWORD cc0;
extern const CHAR  csz1[];
extern const DWORD cc1;
extern const CHAR  cszInfinity[];
extern const DWORD ccInfinity;
extern const CHAR  csz1NoRoot[];
extern const DWORD cc1NoRoot;


// DAV properties that can be requested on PROPFIND
#define  DAV_PROP_CONTENT_LENGTH        0x00000001
#define  DAV_PROP_CREATION_DATE         0x00000002
#define  DAV_PROP_DISPLAY_NAME          0x00000004
#define  DAV_PROP_GET_ETAG              0x00000008
#define  DAV_PROP_GET_LAST_MODIFIED     0x00000010
#define  DAV_PROP_RESOURCE_TYPE         0x00000020
#define  DAV_PROP_SUPPORTED_LOCK        0x00000040
#define  DAV_PROP_IS_HIDDEN             0x00000080
#define  DAV_PROP_IS_COLLECTION         0x00000100
#define  DAV_PROP_LOCK_DISCOVERY        0x00000200
#define  DAV_PROP_GET_CONTENT_TYPE      0x00000400
#define  DAV_PROP_W32_FILE_ATTRIBUTES   0x00000800
#define  DAV_PROP_W32_CREATION_TIME     0x00001000
#define  DAV_PROP_W32_LAST_ACCESS_TIME  0x00002000
#define  DAV_PROP_W32_LAST_MODIFY_TIME  0x00004000
#define  DAV_PROP_ALL                   0xFFFFFFFF


