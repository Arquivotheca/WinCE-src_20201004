//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: BUFFIO.CPP
Abstract: Buffer handling class & socket IO helpers
--*/

#include "httpd.h"

#if defined (OLD_CE_BUILD) && ! defined (SEC_E_INCOMPLETE_MESSAGE)
#define SEC_E_INCOMPLETE_MESSAGE         ((HRESULT)0x80090318L)
#endif

// Wait for input on socket with timeout
int MySelect(SOCKET sock, DWORD dwMillisecs) {
	fd_set set;
	struct timeval t;

	if(dwMillisecs != INFINITE) 	{
		t.tv_sec = (dwMillisecs / 1000);
		t.tv_usec = (dwMillisecs % 1000)*1000;
	}
	FD_ZERO(&set);
	FD_SET(sock, &set);

	DEBUGMSG(ZONE_SOCKET, (L"HTTPD: Calling select(%x). Timeout=%d\r\n", sock, dwMillisecs));
	int iRet = select(0, &set, NULL, NULL, ((dwMillisecs==INFINITE) ? NULL : (&t)));
	DEBUGMSG(ZONE_SOCKET, (L"HTTPD: Select(%x) got %d\r\n", sock, iRet));
	return iRet;
}

// need space for iLen more data
BOOL CBuffer::AllocMem(DWORD dwLen) {
	// figure out buffer size
	DWORD dwAlloc = max(MINBUFSIZE, dwLen);
	
	// allocate or reallocate buffer
	if(!m_pszBuf) {
		m_pszBuf = MyRgAllocZ(char, dwAlloc);
		DEBUGMSG(ZONE_REQUEST_VERBOSE, (L"HTTPD: New buffer (data=%d size=%d buf=0x%08x)\r\n", dwLen, dwAlloc, m_pszBuf));
		m_iSize = dwAlloc;
	}
	else if((m_iSize-m_iNextIn) <= (int)dwLen) 	{
		PSTR pszTemp = MyRgReAlloc(char, m_pszBuf, m_iSize, dwAlloc+m_iSize);
		if (!pszTemp) {
			DEBUGMSG(ZONE_ERROR, (L"HTTPD: CBuffer:AllocMem(%d) failed. GLE=%d\r\n", dwLen, GetLastError()));
			return FALSE;
		}

		m_pszBuf = pszTemp;
		m_iSize += dwAlloc;
		DEBUGMSG(ZONE_REQUEST_VERBOSE, (L"HTTPD: Realloc buffer (datasize=%d oldsize=%d size=%d buf=0x%08x)\r\n", dwLen, m_iSize, dwAlloc+m_iSize, m_pszBuf));
	}
	if(!m_pszBuf) 	{
		DEBUGMSG(ZONE_ERROR, (L"HTTPD: CBuffer:AllocMem(%d) failed. GLE=%d\r\n", dwLen, GetLastError()));
		m_iNextDecrypt = m_iNextInFollow = m_iSize = m_iNextOut = m_iNextIn = 0;
		m_chSaved = 0;
		return FALSE;
	}
	return TRUE;
}



// Suck in all white space before a request.  Note:  We techinally should let
// the filter get this too, but too much work.  Also note that we could read
// past a double CRLF if there was only white space before it, again this
// is an condition so we don't care about it.

// We don't read forever or else we set ourselves up for a DoS attack.
// If we get > MAX_WHITESPACE_READ characters then we give up. 
#define MAX_WHITESPACE_READ   8

BOOL CBuffer::TrimWhiteSpace(BOOL *pfAbort) {
	int i = 0, j = 0;
	DEBUGCHK(*pfAbort == FALSE);

	while (isspace(m_pszBuf[i]) && (i < m_iNextIn) && (i < MAX_WHITESPACE_READ)) { 
		i++;
	}

	if (i == MAX_WHITESPACE_READ) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Web server has been sent greater than %d whitespace chars before HTTP headers begin.  Refusing request\r\n",MAX_WHITESPACE_READ));
		*pfAbort = TRUE;
		return FALSE;
	}
	
	if (i == 0)
		return TRUE;
		
	if (i == m_iNextIn)
		return FALSE;  // need to read more data, all white spaces so far.

	for (j = 0; j < m_iNextIn - i; j++)
		m_pszBuf[j] = m_pszBuf[j+i];

	m_iNextIn -= i;

	DEBUGMSG(ZONE_PARSER,(L"HTTPD: TrimWhiteSpace removing first %d bytes from steam\r\n",i));
	return TRUE;
}


// This function reads eithr request-headers from the socket
// terminated by a double CRLF, OR reads a post-body from the socket
// terminated by having read the right number of bytes
// 
// We are keeping the really simple--we read the entire header
// into one contigous buffer before we do anything.
//
// dwLength is -1 for reading headers, or Content-Length for reading body
// or 0 is content-length is unknown, in which case it reads until EOF

// Note: This function is more than just a generic receive, it takes into account
// HTTP headers + POST status of current request.

HRINPUT CBuffer::RecvToBuf(SOCKET sock, DWORD dwLength, DWORD dwTimeout, BOOL fFromFilter, BOOL fFirstPostRead, CHttpRequest *pRequest, BOOL fSSLRenegotiate) {
	DEBUG_CODE_INIT;
	int iScan = 0;
	HRINPUT ret = INPUT_ERROR;

	BOOL  fReadHeaders        = (dwLength == (DWORD)-1);
	BOOL  fIsSecure           = pRequest->IsSecure();
	BOOL  fScanHeadersForCRLF = fIsSecure ? FALSE : TRUE;

	// fSSLSkipFirstRead = TRUE when we have data was read in as part of the SSL handshake 
	// that is actually part of the HTTP request.  In this case decrypt what we have before calling recv(),
	// it's possible the entire HTTP request has already been read in so a recv() would block.
	BOOL  fSSLSkipFirstRead = (fIsSecure && m_iNextIn && fReadHeaders) ? TRUE : FALSE;
	DWORD dwBytesDecrypted = 0;
	DWORD dwBytesRemainingToBeRead;

	// dwSSLOffset is pointer to data that remains encrypted.
	// It's possible we have unencrypted data from a previous HTTP request (but 
	// when fSSLSkipFirstRead=TRUE we use different code path, so set=0).
	DWORD dwSSLOffset  = !fSSLSkipFirstRead ? m_iNextIn - m_iNextDecrypt : 0;

	// In cases web server requests a renegotiate *after* client has sent us all its POST
	// data, we read POST data first but then have to keep reading to get renegotiate params.
	// To do this we allocate extra data and keep listening.
	BOOL fForceSSLRenegotiate;

	// In typical case (i.e. reading HTTP headers) we read up to maximum header size.
	// However when we force SSL renegotiation, we'll read up to max header size+max POST size
	// as we've read the headers already.
	DWORD dwMaxBufferSize = g_pVars->m_dwMaxHeaderReadSize;
	if (!fReadHeaders)
		dwMaxBufferSize += g_pVars->m_dwPostReadSize;

	DEBUGMSG(ZONE_REQUEST_VERBOSE,(L"HTTPD: RecvToBuf() sock=%d,dwLength=%d,fFromFilter=%d,fFirstPostRead=%d,fSSLRenegotiate=%d,fReadHeaders=%d,fIsSecure=%d\r\n",
	                                sock,dwLength,fFromFilter,fFirstPostRead,fSSLRenegotiate,fReadHeaders,fIsSecure));
	WriteBufferToDebugOut(FALSE);
	BufferConsistencyChecks();

	// Some clients ignore the fact that we're an HTTP 1.0 server and send
	// us multiple HTTP requests on the same packet.  If we detect this situation when
	// reading in data we make note of it at the time and clean things up when reading 
	// in the headers to the next request.
	if (fReadHeaders && m_iNextRequestBegin) {
		DEBUGCHK(m_pszBuf[m_iNextRequestBegin] == 0);
		m_pszBuf[0] = m_chNextRequestSaved;

		DEBUGMSG(ZONE_REQUEST_VERBOSE,(L"HTTPD: End Cleanup: m_iNextRequestSize=%d,m_iNextRequestBegin=%d\r\n",m_iNextRequestSize,m_iNextRequestBegin));
		
		for (int i = 1; i < m_iNextRequestSize; i++)
			m_pszBuf[i] = m_pszBuf[m_iNextRequestBegin+i];

		m_iNextIn = m_iNextRequestSize;

		m_iNextRequestBegin  = m_iNextRequestSize = 0;
		m_chNextRequestSaved = 0;
		fScanHeadersForCRLF = TRUE;
	}

	// Both IE and Netscape tack on a trailing \r\n to POST data but don't
	// count it as part of the Content-length.  IIS doesn't pass the \r\n
	// to the script engine, so we don't either.  To do this, we set
	// the \r to \0.  Also we reset m_iNextIn.  This \r\n code is only
	// relevant when RecvToBuf is called from HandleRequest, otherwise
	// we assume it's a filter calling us and don't interfere.
	if (!fReadHeaders) {
		if (!fSSLRenegotiate && !fFromFilter && ((m_iNextIn-m_iNextOut) >= (int) dwLength))  {
			if (fFirstPostRead && (m_iNextIn-m_iNextOut) > (int) dwLength+2) {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: WARNING: more than one HTTP messages have been sent in one packet!  This is not valid HTTP 1.0! "
				                       L"Web server is cleaning up message and will process normally now, FIX CLIENT to make it HTTP 1.0 (not only 1.1) aware\r\n"));
				// client has sent us more than one HTTP request (or at least beginning of 2nd request)
				// in the packet we read on initial header read-in.  Save off information for next request.

				DEBUGMSG(ZONE_REQUEST_VERBOSE,(L"HTTPD: Begin cleanup: m_iNextRequestBegin=%d, m_iNextRequestSize=%d, m_iNextIn=%d,m_iNextOut=%d,dwLength=%d",m_iNextRequestBegin,m_iNextRequestSize,m_iNextIn,m_iNextOut,dwLength));
				
				m_iNextRequestBegin  = m_iNextOut + dwLength;
				m_iNextRequestSize   = m_iNextIn - m_iNextRequestBegin; // length of buffer
				m_iNextInFollow      = m_iNextIn = m_iNextRequestBegin;
				m_chNextRequestSaved = m_pszBuf[m_iNextIn];
				myretleave(INPUT_NOCHANGE,0);
			}
		
			DEBUGCHK( ((m_iNextIn-m_iNextOut) == (int) dwLength) ||
					  ((m_iNextIn-m_iNextOut) == (int) dwLength+2));

			m_iNextInFollow = m_iNextIn = m_iNextOut + dwLength;
			// Everything has been read into POST request already, no processing.
			myretleave(INPUT_NOCHANGE,0);
		}
		if (!fFromFilter) {
			dwLength = dwLength - (m_iNextIn - m_iNextOut);   // account for amount of POST data already in
		}
		m_iNextInFollow = m_iNextIn;

		// allocate or reallocate buffer.  Since we already know size we want, do it here rather than later.
		if(!AllocMem(dwLength+1))
			myretleave(INPUT_ERROR, 103);		
	}
	dwBytesRemainingToBeRead = dwLength;
	fForceSSLRenegotiate = (fSSLRenegotiate && (dwLength == 0));

	for(;;)  {
		// see if we got the double CRLF for HTTP Headers.
		if(fReadHeaders && fScanHeadersForCRLF)  {
			BOOL fScan = TRUE;		
			if (iScan == 0 && m_iNextIn) {
				BOOL fAbort = FALSE;
				fScan = TrimWhiteSpace(&fAbort);
				if (fAbort)  // too much white space was sent between keep-alive requests
					myretleave(INPUT_ERROR,111);
			}
			if (fScan) {
				while(iScan+3 < (fIsSecure ? (int) dwBytesDecrypted : m_iNextIn))  {
					if(m_pszBuf[iScan]=='\r' && m_pszBuf[iScan+1]=='\n' && m_pszBuf[iScan+2]=='\r' && m_pszBuf[iScan+3]=='\n') {
						DEBUGMSG(ZONE_REQUEST_VERBOSE,(L"HTTPD: RecvToBuf: found double CRLF to close headers\r\n"));
						myretleave(INPUT_OK,0);
					}
					iScan++;
				}
			}
		}
		// else see if we have the number of bytes we want.  
		// Browsers sometimes tack an extra \r\n to very end of POST data, even
		// though they don't include it in the Content-Length field.  IIS
		// never passes this extra \r\n to ISAPI extensions, neither do we.
		else if((!fSSLRenegotiate || (fSSLRenegotiate && !fForceSSLRenegotiate)) && 
		         !fReadHeaders && (dwBytesRemainingToBeRead == 0))  {
			DEBUGCHK(fIsSecure || ((int)dwLength  + 2 == (m_iNextIn-m_iNextInFollow) ||
			         (int)dwLength == (m_iNextIn-m_iNextInFollow)));

			DEBUGMSG(ZONE_REQUEST_VERBOSE,(L"HTTPD: RecvToBuf: we've got required # of bytes on POST!\r\n"));
//			m_iNextIn = m_iNextInFollow+(int)dwLength;	// don't copy trailing \r\n
			myretleave(INPUT_OK,0);
		}

		if ((fReadHeaders || fForceSSLRenegotiate) && ((int)dwMaxBufferSize <= m_iNextIn)) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Client has sent >= %d bytes in HTTP headers or SSL renegotiate, rejecting request!\r\n",dwMaxBufferSize));
			myretleave(INPUT_ERROR,109);
		}
		// fSSLSkipFirstRead = TRUE in case where we have data already read in but haven't decrypted
		// it yet - it's possible that the client will send no more data so select would timeout if we call it.
		if (!fSSLSkipFirstRead) {
			// check if we have input. If we are waiting for subsequent input (i.e. not the start of a request)
			// then drop the timeout value lower, and if we timeout return ERROR, not TIMEOUT
			switch(MySelect(sock, ((m_iNextIn ? g_dwServerTimeout : dwTimeout)))) {
				case 0: 			myretleave((m_iNextIn ? INPUT_ERROR : INPUT_TIMEOUT),100);
				case SOCKET_ERROR: 	myretleave(INPUT_ERROR, 101);
			}
		}

		// check how much input is waiting
		DWORD dwAvailable   = 0;
		DWORD dwBytesToRecv = 0;

		if (!fSSLSkipFirstRead) {
			if(ioctlsocket(sock, FIONREAD, &dwAvailable))
				myretleave(INPUT_ERROR, 102);

			if (fReadHeaders || fForceSSLRenegotiate) {
				// only read up to dwMaxBufferSize.  If we don't get double CRLF on 
				// next pass through loop then we'll bomb out.
				if (dwAvailable + m_iNextIn > dwMaxBufferSize)
					dwBytesToRecv = dwMaxBufferSize - m_iNextIn;
				else
					dwBytesToRecv = dwAvailable;

				if(!AllocMem(dwBytesToRecv+1))
					myretleave(INPUT_ERROR, 103);
			}	
			else {                       // Read in only requested amount of POST
				dwBytesToRecv = min(dwAvailable,dwBytesRemainingToBeRead);
				// we don't call AllocMem() here because we allocated all we'll need at top of function.
			}

			DEBUGMSG(ZONE_REQUEST_VERBOSE,(L"HTTPD: RecvToBuf: available bytes=%d, bytes we'll recv=%d\r\n",dwAvailable,dwBytesToRecv));
		}
#if defined (DEBUG)
		else
			DEBUGMSG(ZONE_REQUEST_VERBOSE,(L"HTTPD: RecvToBuf has %d bytes of data already from SSL negotiation, using it instead of calling recv()\r\n",m_iNextIn));
#endif

		DEBUGCHK((m_iSize-m_iNextIn) >= (int)dwBytesToRecv);
		BufferConsistencyChecks();

		int iRecv = 0;
		if (fSSLSkipFirstRead) {
			// We already have data from SSL handshake that needs to be decrypted.
			// trick receive logic into thinking we read preexisting data 
			// into buffer on the loop above.  After this point, there's no difference
			// between if we got data left over from handshake or if we got it from first receive 
			// at any point during the request.
			iRecv = m_iNextIn;
			m_iNextIn = 0;
			fSSLSkipFirstRead = FALSE;
		}
		else {
			// typical case, read data.
			iRecv = recv(sock, m_pszBuf+m_iNextIn, dwBytesToRecv, 0) ;
			DEBUGMSG(ZONE_SOCKET, (L"HTTPD: recv(%x) got %d\r\n", sock, iRecv));

			if (iRecv == 0 && !fForceSSLRenegotiate) {
				DEBUGMSG(ZONE_REQUEST_VERBOSE,(L"HTTPD: RecvToBuf: iRecv==0, m_iNextIn=%d\r\n",m_iNextIn));
				myretleave((m_iNextIn ? INPUT_OK : INPUT_TIMEOUT), 0); 
			} // got EOF. If we have any data return OK, else return TIMEOUT
			else if (iRecv == SOCKET_ERROR) { 
				DEBUGMSG(ZONE_REQUEST_VERBOSE,(L"HTTPD: RecvToBuf: iRecv==SOCKET_ERROR, m_iNextIn=%d\r\n",m_iNextIn));
				myretleave(((GetLastError()==WSAECONNRESET) ? INPUT_TIMEOUT : INPUT_ERROR), 104);
			}
		}

		// The following block is executed if we want to decrypt SSL.  When reading http
		// headers on secure channel, always do SSLDecrypt because we don't know how big headers 
		// will be.  When reading in POST data, if we know we haven't recv()'d enough data yet, call
		// recv() again before starting decryption as an optimization.
		if (fForceSSLRenegotiate || (fIsSecure && (fReadHeaders || (0 >= (int)dwBytesRemainingToBeRead-iRecv)))) {
			DWORD dwBytesDecryptedInCall=0, dwExtra=0, dwIgnore=0;
			DWORD dwSSLOffsetOrg = dwSSLOffset;
			DWORD dwDecLen = dwSSLOffset+iRecv;
			BOOL fCompletedRenegotiate = FALSE;

			// Calls into SSL/HTTP layer to perform decryption.  See function definition for explanation of paramaters.
			SECURITY_STATUS scRet = pRequest->SSLDecrypt(m_pszBuf+m_iNextDecrypt,
			                                   dwDecLen,&dwBytesDecryptedInCall,&dwSSLOffset,&dwExtra,&dwIgnore,&fCompletedRenegotiate);

			if ((scRet != SEC_E_OK) &&
			    (scRet != SEC_E_INCOMPLETE_MESSAGE)) {
			    DEBUGMSG(ZONE_ERROR,(L"HTTPD: SSLDecrypt fails, scRet=0x%08x\r\n",scRet));
				myretleave(INPUT_ERROR,115);
			}

			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: dwBytesDecryptedInCall=%d,dwBytesDecrypted=%d,dwIgnore=%d,dwBytesRemainingToBeRead=%d,dwDecLen=%d,dwSSLOffset=%d,dwSSLOffsetOrg=%d\r\n",
			                                    dwBytesDecryptedInCall,dwBytesDecrypted,dwIgnore,dwBytesRemainingToBeRead,dwDecLen,dwSSLOffset,dwSSLOffsetOrg));

			dwBytesDecrypted += dwBytesDecryptedInCall;
			m_iNextDecrypt   += dwBytesDecryptedInCall;

			// iRecv is number of bytes previous receive call got, however because 
			// we may be throwing out unneeded trailer information the next place
			// we read in may be a little lower than it was before call.
			if (scRet == SEC_E_OK)
				iRecv = (int)(dwBytesDecryptedInCall-dwSSLOffsetOrg);
			else
				iRecv -= (int) dwIgnore;

			dwBytesRemainingToBeRead -= dwIgnore;

			// we renegotiate and don't have any POST data to read, or have finished with POST data
			if (fForceSSLRenegotiate && fCompletedRenegotiate)  {
				DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: SSLDecrypt completed negotiate, dwBytesDecryptedInCall=%d,m_iNextIn=%d\r\n",dwBytesDecryptedInCall,m_iNextIn + dwBytesDecryptedInCall));
				if (scRet == SEC_E_OK)
					m_iNextIn += iRecv;
				myretleave(scRet == S_OK ? INPUT_OK : INPUT_ERROR,119);
			}

			// Once we get 1st part of unencrypted data we'll start scanning over the HTTP headers.
			if (fReadHeaders && dwBytesDecryptedInCall)
				fScanHeadersForCRLF = TRUE;

			if (scRet == SEC_E_OK) {
				if (!fReadHeaders) {
					if (dwBytesDecrypted >= dwLength)
						dwBytesRemainingToBeRead = 0;
					else
						dwBytesRemainingToBeRead = iRecv + dwLength - dwBytesDecrypted;
				}
			}
			else if (scRet == SEC_E_INCOMPLETE_MESSAGE && !fReadHeaders)  {
				// We don't have complete message but we know how long it's going to be.
				// This will make main loop continue.  For Headers we just call elsewhere.
				if (!AllocMem(dwExtra+iRecv))
					myretleave(INPUT_ERROR, 103);

				dwBytesRemainingToBeRead += dwExtra;
			}

			// we've read all the POST data off the wire, however we still
			// need more packets to come across to contain the renegotiation information.
			if ((dwBytesRemainingToBeRead == 0) && fSSLRenegotiate && !fForceSSLRenegotiate) {
				DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: All POST data read but still need renegotiate for client certs, fForceSSLRenegotiate=TRUE\r\n"));
				fForceSSLRenegotiate = TRUE;
			}
		}
		else
			dwSSLOffset += iRecv;

		m_iNextIn += iRecv;
		WriteBufferToDebugOut(FALSE);

		if (dwBytesRemainingToBeRead)
			dwBytesRemainingToBeRead -= iRecv;

		BufferConsistencyChecks();
		DEBUGCHK(!fReadHeaders || (int)dwMaxBufferSize >= m_iNextIn);
	}
	DEBUGCHK(0); // no fall through
	
done:
	BufferConsistencyChecks();

	// Always make this buffer into a null terminated string
	if (m_pszBuf)
		m_pszBuf[m_iNextIn] = 0;	  

	WriteBufferToDebugOut(TRUE);

	DEBUGMSG(ZONE_REQUEST, (L"HTTPD: end RecvToBuf (ret=%d err=%d iGLE=%d)\r\n", ret, err, GLE(err)));
	return ret;
}


// tokenize the input stream: We always skip leading white-space
// once we're in the token, we stop on whitespace or EOL, depending
// on the fWS param
BOOL CBuffer::NextToken(PSTR* ppszTok, int* piLen, BOOL fWS, BOOL fColon)  {
	int i, j;
	// restore saved char, if any
	if(m_chSaved)  {
		DEBUGCHK(m_pszBuf[m_iNextOut]==0);
		m_pszBuf[m_iNextOut] = m_chSaved;
		m_chSaved = 0;
	}
	
	for(i=m_iNextOut; i<m_iNextIn; i++) {
		// if not whitespace break
		if(! (m_pszBuf[i]==' ' || m_pszBuf[i]=='\t') )
			break;
	}
	for(j=i; j<m_iNextIn; j++) {
		// if we get an EOL, it's always end of token
		if(m_pszBuf[j]=='\r' || m_pszBuf[j]=='\n')
			break;
		// if fWS==TRUE and we got white-space, then end of token
		if(fWS && (m_pszBuf[j]==' ' || m_pszBuf[j]=='\t'))
			break;

		// HTTP 1.1 headers may start immediatly without a space between name/value
		// (i.e. header-name:value).
		if (fColon && (j>0) && (m_pszBuf[j-1]==':'))
			break;
	}
	m_iNextOut = j;
	*piLen = (j-i);
	*ppszTok = &(m_pszBuf[i]);
	if(i==j) {
		DEBUGMSG(ZONE_TOKEN, (L"HTTPD: Got NULL token"));
		return FALSE;
	}
	else {
		// save a char so we can null-terminate the current token
		m_chSaved = m_pszBuf[m_iNextOut];
		m_pszBuf[m_iNextOut] = 0;
		DEBUGMSG(ZONE_TOKEN, (L"HTTPD: Got token (%a) Len %d\r\n", *ppszTok, (*piLen)));
		return TRUE;
	}
}

// skip rest of current line and CRLF
BOOL CBuffer::NextLine() {
	int i, j;
	
	// restore saved char, if any
	if(m_chSaved) {
		DEBUGCHK(m_pszBuf[m_iNextOut]==0);
		m_pszBuf[m_iNextOut] = m_chSaved;
		m_chSaved = 0;
	}

	for(i=m_iNextOut, j=i+1; j<m_iNextIn; i++, j++) {
		if(m_pszBuf[i]=='\r' && m_pszBuf[j]=='\n') {
			m_iNextOut = j+1;
			DEBUGMSG(ZONE_TOKEN, (L"HTTPD: NextLine: OK\r\n"));
			return TRUE;
		}
	}
	DEBUGMSG(ZONE_TOKEN, (L"HTTPD: NextLine: error\r\n"));
	return FALSE;
}	

BOOL CBuffer::AppendData(PCSTR pszData, int iLen) {
	// make sure we have enough memory
	if(!AllocMem(iLen+1))
		return FALSE;

	DEBUGCHK((m_iSize-m_iNextIn) >= iLen);
	memcpy(m_pszBuf+m_iNextIn, pszData, iLen);
	m_iNextIn += iLen;
	return TRUE;
}



BOOL CBuffer::SendBuffer(CHttpRequest *pRequest) {
	DEBUGCHK(m_iNextOut==0);
	DEBUGCHK(m_chSaved==0);

	if(!m_iNextIn) {
		DEBUGMSG(ZONE_REQUEST_VERBOSE, (L"HTTPD: SendBuffer: empty\r\n"));
		return TRUE;
	}

	BOOL fRet = pRequest->SendData(m_pszBuf,m_iNextIn);
	m_iNextIn = 0;
	return fRet;
}

#if defined (DEBUG)
void CBuffer::WriteBufferToDebugOut(BOOL fPrintAll)  {
	int i = 0;

	if (! (ZONE_REQUEST_VERBOSE & dpCurSettings.ulZoneMask))
		return;

	DEBUGMSG(1,(L"HTTPD: m_iSize = %d, m_iNextOut = %d, m_iNextIn = %d, m_iNextInFollow = %d\r\n",m_iSize,m_iNextOut,m_iNextIn,m_iNextInFollow));
	DEBUGMSG(1,(L"HTTPD: m_iNextRequestBegin = %d, m_iNextRequestSize =%d, m_iNextDecrypt=%d\r\n",m_iNextRequestBegin,m_iNextRequestSize,m_iNextDecrypt));

#if defined (DEBUG_PRINT_ALL_BYTES)

	if (fPrintAll) {
		DEBUGMSG(1,(L"--BEGIN BUFFER\r\n"));
		for (i = m_iNextOut; i < m_iNextIn; i += 8) {
			DEBUGMSG(1,(L"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\r\n",
			            m_pszBuf[i],  m_pszBuf[i+1],m_pszBuf[i+2],m_pszBuf[i+3],
			            m_pszBuf[i+4],m_pszBuf[i+5],m_pszBuf[i+6],m_pszBuf[i+7]));
		}
		DEBUGMSG(1,(L"--END BUFFER\r\n"));
	}
#endif

}
#endif
