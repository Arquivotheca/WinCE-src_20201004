//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: BUFFIO.H
Abstract: Buffer handling class & socket IO helpers
--*/


// returned from the socket IO functions
typedef enum {
	INPUT_OK = 0,
	INPUT_TIMEOUT = 1,
	INPUT_ERROR = 2,
	INPUT_NOCHANGE = 3
	//INPUT_EOF = 2,
}
HRINPUT;

class CHttpRequest;		// forward declaration
typedef struct _SSL_INFO SSL_INFO;

int MySelect(SOCKET sock, DWORD dwMillisecs);

class CBuffer {
public:
	PSTR	m_pszBuf;
	int		m_iSize;
	int		m_iNextOut;
	int		m_iNextIn;
	int     m_iNextInFollow;   // next place to read, needed by raw read filters.  
	int     m_iNextDecrypt;    // marks the edge of unencrypted data during an SSL connection
	CHAR	m_chSaved;	// Used by the parser

	// Handles case where we get multiple HTTP requests on one packet.
	// Handle 1st HTTP request 1st and save info about where 2nd request begins
	// for when we handle it.
	int     m_iNextRequestBegin;    // where does it begin in m_pszBuf?
	int     m_iNextRequestSize;     // number of bytes of next request(s) we've read off wire already.
	CHAR    m_chNextRequestSaved;   // 1st char in next request (byte set to \0 so ISAPIs won't read it)

	// There's cases (like ISAPI read filter or ReadClient) where we can mess
	// up the state of the HTTP request IF m_iNextRequestBegin != 0.  If person
	// writes their ISAPI correctly it won't be an issue, but if they don't data could
	// end up coming in out of order during a keep-alive, so in this case we'll 
	// ignore anything we have.  Not ideal, but ideally clients shouldn't be sending 
	// multiple HTTP requests to in one packet to an HTTP 1.0 server in 1st place.
	void    InvalidateNextRequestIfAlreadyRead() { m_iNextRequestBegin = 0; }

	// Functions
	BOOL AllocMem(DWORD dwLen);
	BOOL NextToken(PSTR* ppszTok, int* piLen, BOOL fWS, BOOL fColon=FALSE);

	CBuffer() {
		memset(this, 0, sizeof(*this));
	}

	~CBuffer() {
		MyFree(m_pszBuf);
	}

	//  Used to reset a buffer through the course of 1 session, uses 
	//  same allocated mem block. (don't change m_iSize, leave m_iNextRequestXXX alone)
	void Reset() {
		m_iNextDecrypt = m_iNextInFollow = m_iNextOut = m_iNextIn = m_chSaved = 0;
	}

	// accessors
	DWORD Count() { return m_iNextIn - m_iNextOut; } 	
	BOOL  HasPostData()    { return (m_iNextIn > m_iNextOut);  }
	PBYTE Data()  { return (PBYTE)(m_pszBuf + m_iNextOut); }
	DWORD UnaccessedCount() { return m_iNextIn - m_iNextInFollow; }  // this is data that hasn't been modified yet, for filters
	DWORD AvailableBufferSize()  { return m_iSize - m_iNextInFollow; }  // size of buffer, used by filters
	PBYTE FilterRawData()  { return (PBYTE)(m_pszBuf + m_iNextInFollow); }
	PSTR Headers() { return m_pszBuf; }  // Http headers are at beginning of buf
	DWORD GetINextOut()  { return m_iNextOut; }
	BOOL TrimWhiteSpace(BOOL *pfAbort);


	// input functions
	HRINPUT RecvToBuf(SOCKET sock, DWORD dwLength, DWORD dwTimeout, BOOL fFromFilter, BOOL fFirstPostRead, CHttpRequest *pRequest,BOOL fSSLRenegotiate);
	HRINPUT RecvHeaders(SOCKET sock, CHttpRequest *pRequest) {
		return RecvToBuf(sock, (DWORD)-1, KEEPALIVETIMEOUT,FALSE,FALSE,pRequest,FALSE);
	};

	HRINPUT RecvBody(SOCKET sock, DWORD dwLength, BOOL fFromFilter, BOOL fFirstPostRead, CHttpRequest *pRequest, BOOL fSSLRenegotiate) { 
		DEBUGCHK(m_pszBuf && m_iSize);
		return RecvToBuf(sock, dwLength, RECVTIMEOUT,fFromFilter,fFirstPostRead,pRequest,fSSLRenegotiate);
	};
	BOOL NextTokenWS(PSTR* ppszTok, int* piLen)  { return NextToken(ppszTok, piLen, TRUE); }
	BOOL NextTokenEOL(PSTR* ppszTok, int* piLen) { return NextToken(ppszTok, piLen, FALSE); }
	BOOL NextTokenHeaderName(PSTR* ppszTok, int* piLen) { return NextToken(ppszTok, piLen, FALSE, TRUE); }
	BOOL NextLine();
	BOOL AddHeader(PCSTR pszName, PCSTR pszValue, BOOL fAddColon=FALSE);

	BOOL AppendCHAR(const CHAR cAppend) {
		return AppendData((char *)&cAppend,sizeof(CHAR));
	}

	BOOL EncodeURL(PCSTR pszData);

#if defined (DEBUG)
	void WriteBufferToDebugOut(BOOL fPrintAll);
	void BufferConsistencyChecks(void) {
		DEBUGCHK(m_iNextIn >= m_iNextOut);
		DEBUGCHK(m_iNextIn <= m_iSize);
		DEBUGCHK(m_iNextIn >= m_iNextDecrypt);
		DEBUGCHK(m_iNextIn >= m_iNextInFollow);
	}
#else
	void WriteBufferToDebugOut(BOOL fPrintAll) {; }
	void BufferConsistencyChecks(void) { ; }
#endif
	
	// output functions
	BOOL AppendData(PCSTR pszData, int iLen);
	BOOL SendBuffer(CHttpRequest *pRequest);
	BOOL FilterDataUpdate(PVOID pvData, DWORD cbData, BOOL fModifiedPointer);
};
