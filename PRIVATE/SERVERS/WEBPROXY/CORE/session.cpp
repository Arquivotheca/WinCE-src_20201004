//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "session.h"
#include "proxydbg.h"
#include <ws2tcpip.h>

const char gc_szProxyPac[] =
"function FindProxyForURL(url, host)\n"
"{\n"
"if (isInNet(host, \"%s\", \"%s\"))\n"
"{\n"
"return \"DIRECT\";\n"
"}\n"
"if (isPlainHostName(host))\n"
"{\n"
"return \"DIRECT\";\n"
"}\n"
"if (host == \"127.0.0.1\")\n"
"{\n"
"return \"DIRECT\";\n"
"}\n"
"if (url.substring(0, 6) == \"https:\" ||\n"
"url.substring(0, 5) == \"http:\")\n"
"{\n"
"return \"PROXY %s:%d\";\n"
"}\n"
"return \"DIRECT\";\n"
"}\n";

extern char* g_szURLNotFound;
extern int g_cchURLNotFound;
extern char* g_szProxyAuthFailed;
extern int g_cchProxyAuthFailed;
extern char* g_szHeaderNotSupported;
extern int g_cchHeaderNotSupported;
extern char* g_szProtocolNotSupported;
extern int g_cchProtocolNotSupported;
extern char* g_szVersionNotSupported;
extern int g_cchVersionNotSupported;


extern ce::string g_strPrivateAddr;
extern ce::string g_strPrivateMask;
extern ce::string g_strHostName;
extern stringi g_strProxyPacURL;
extern int g_iPort;

ce::critical_section CHttpSession::m_csGlobal;
CHttpParser CHttpSession::m_Parser;

CHttpSession::CHttpSession(void) :
	m_fRunning(FALSE),
	m_fAuth(FALSE),
	m_fSSLTunnel(FALSE),
	m_fAuthInProgress(FALSE),
	m_dwSessionId(0),
	m_cbResponseRemain(0),
	m_cbRequestRemain(0)
{
}

CHttpSession::~CHttpSession(void)
{
}

void CHttpSession::SetId(DWORD dwSessionId)
{
	SessionLock();
	m_dwSessionId = dwSessionId;
	SessionUnlock();
}

DWORD CHttpSession::GetId(void)
{
	DWORD dwId;
	
	SessionLock();
	dwId = m_dwSessionId;
	SessionUnlock();

	return dwId;
}

DWORD CHttpSession::Start(SessionSettings* pSettings)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	ASSERT(pSettings);
	ASSERT(pSettings->pThreadPool);
	ASSERT(pSettings->pSessionMgr);
	
	SessionLock();

	ASSERT(! m_fRunning);

	// Set session data
	m_pThreadPool = pSettings->pThreadPool;
	m_pSessionMgr = pSettings->pSessionMgr;
	m_sockClient = pSettings->sockClient;
	m_saClient = pSettings->saClient;
	m_fAuth = pSettings->fAuthEnabled;
	m_iSessionTimeout = pSettings->iSessionTimeout;
	m_iMaxHeadersSize = pSettings->iMaxHeadersSize;
	
	//memcpy(&m_saPrivate, &pSettings->saPrivate, sizeof(sockaddr));
	m_saPrivate = pSettings->saPrivate;
	
	m_fRunning = TRUE;

	memset(&m_auth, 0, sizeof(AUTH_NTLM));
	m_auth.m_Conversation = NTLM_NO_INIT_CONTEXT;

	Lock();
	// Create a thread to handle the session
	m_pThreadPool->ScheduleEvent(SessionThread, this);
	Unlock();

	SessionUnlock();
	return ERROR_SUCCESS;
}

DWORD CHttpSession::Shutdown(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	SessionLock();

	ASSERT(m_fRunning);

	// Change thread status to signal shutdown
	m_fRunning = FALSE;
	
	// Delete socket data (unblock any blocking socket calls)
	if (SOCKET_ERROR == closesocket(m_sockClient)) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error closing client socket in session %d, error: %d\n"), m_dwSessionId, WSAGetLastError()));
	}	
	if (m_sockServer.valid() && (SOCKET_ERROR == closesocket(m_sockServer))) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error closing server socket in session %d, error: %d\n"), m_dwSessionId, WSAGetLastError()));
	}

	SessionUnlock();
	return dwRetVal;
}

DWORD WINAPI CHttpSession::SessionThread(LPVOID pv)
{
	CHttpSession* pInst = (CHttpSession*) pv;
	pInst->Run();
	return 0;
}

void CHttpSession::Run(void)
{
	DWORD dwErr;
	FD_SET sockSet;
	CBuffer buffer;
	BOOL fCloseSessionReq = FALSE;
	BOOL fCloseSessionResp = FALSE;

	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Session %d has been started.\n"), m_dwSessionId));

	timeval timeout = {0};
	timeout.tv_sec = m_iSessionTimeout;

	SessionLock();
	
	while (1) {
		//
		// Loop for the lifetime of the session
		//

		BOOL fServerConnected = FALSE;
		FD_ZERO(&sockSet);

		FD_SET(m_sockClient, &sockSet);
		if (m_sockServer.valid()) {
			fServerConnected = TRUE;
			FD_SET(m_sockServer, &sockSet);
		}

		SessionUnlock();
		int iSockets = select(0,&sockSet,NULL,NULL,&timeout);
		SessionLock();
	
		// Check if session was closed while blocking on select
		if (! m_fRunning) {
			IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Session %d has been signalled to shutdown.\n"), m_dwSessionId));
			break;
		}

		if (SOCKET_ERROR == iSockets) {
			IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Select call failed in session %d, error: %d.\n"), m_dwSessionId, WSAGetLastError()));
			break;
		}

		if (0 == iSockets) {
			IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Session %d has expired\n"), m_dwSessionId));
			break;
		}

		//
		// Make sure the connections are not being closed
		//

		if (FD_ISSET(m_sockClient, &sockSet)) {
			DWORD dwAvailable = 0;
			if (SOCKET_ERROR == ioctlsocket(m_sockClient, FIONREAD, &dwAvailable)) {
				IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Failed to query how much data is available on socket %d\n"), WSAGetLastError()));
				dwAvailable = 0;
			}

			// If no data is available on client socket then close the session
			if (0 == dwAvailable) {				
				IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: The client socket has been signalled to close.  Closing session %d.\n"), m_dwSessionId));
				break;
			}
		}

		if (fServerConnected && FD_ISSET(m_sockServer, &sockSet)) {
			DWORD dwAvailable = 0;
			if (SOCKET_ERROR == ioctlsocket(m_sockServer, FIONREAD, &dwAvailable)) {
				IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Failed to query how much data is available on socket %d\n"), WSAGetLastError()));
				dwAvailable = 0;
			}

			// If no data is available on server socket close the session unless auth is in progress in which case we should
			// just close the server socket.
			if (0 == dwAvailable) {
				if (m_fAuthInProgress) {
					m_sockServer.close();
					m_strServerName = "";
					continue;
				}
				break;
			}
		}

		//
		// Check for data on the sockets
		//
		
		if (FD_ISSET(m_sockClient, &sockSet)) {
			//
			// Packet arrived from client
			//

			dwErr = ProcessClientRequest(buffer, &fCloseSessionReq);
			if (ERROR_SUCCESS != dwErr) {
				IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Due to an internal error when processing a request, session %d is being aborted: %d.\n"), m_dwSessionId, dwErr));
				break;
			}

			if (fCloseSessionReq && (0 == m_cbRequestRemain)) {
				IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Proxy was signalled to close session by a request, session %d is being closed.\n"), m_dwSessionId));
				shutdown(m_sockClient, SD_SEND);
				continue;
			}
		}
		
		if (fServerConnected && FD_ISSET(m_sockServer, &sockSet)) {
			//
			// Packet arrived from server
			//

			dwErr = ProcessServerResponse(buffer, &fCloseSessionResp);
			if (ERROR_SUCCESS != dwErr) {
				IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Proxy was signalled to close session by a response or an internal error occured, session %d is being closed: %d.\n"), m_dwSessionId, dwErr));
				break;
			}

			if (fCloseSessionResp && (0 == m_cbResponseRemain)) {
				IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Proxy was signalled to close session by a response, session %d is being closed.\n"), m_dwSessionId));
				shutdown(m_sockServer, SD_SEND);
				continue;
			}
		}
	}

	// If we aborted not due to stopping service then we have to clean up the session
	if (m_fRunning) {
		Shutdown();
	}

	SessionUnlock();

	dwErr = m_pSessionMgr->RemoveSession(m_dwSessionId);

#ifdef DEBUG
	if (ERROR_SUCCESS != dwErr) {
		IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Could not delete session %d, error: %d - potential memory leak\n"), m_dwSessionId, dwErr));
	}
#endif // DEBUG

}

DWORD CHttpSession::ProcessClientRequest(CBuffer& buffer, BOOL* pfCloseConnection)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	PBYTE pBuffer = NULL;
	CHttpHeaders headers;
	int cbSent = 0;
	int cbRequest = 0;
	int cbTotalRecved = 0;
	int cbHeaders = 0;
	
	IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: Request arrived from client in session %d.\n"), m_dwSessionId));

	//
	// Receive data from the client until at least all the headers have been read
	//

	dwRetVal = RecvAllHeaders(m_sockClient, buffer, &pBuffer, &cbTotalRecved, m_cbRequestRemain, &cbHeaders);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}
	
	IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: Request received buffer.  Total: %d, Headers: %d, Remaining: %d.  Session %d\n"), cbTotalRecved, cbHeaders, m_cbRequestRemain, m_dwSessionId));

	if ((! m_fSSLTunnel) && (0 == m_cbRequestRemain)) {		
		//
		// Now that we have all the headers, parse them.
		//

		BOOL fContinue;
		
		PBYTE pContent;
		CBuffer buffContent;
		int cbNewHeaders; 	// size of the headers after modification
		int cbContent;		// size of the content in this packet
		int cbTotalContent;	// total amount of content that has to be recved
		
		dwRetVal = m_Parser.ParseRequest(pBuffer, cbTotalRecved, headers);
		if (ERROR_SUCCESS != dwRetVal) {
			// If hit this assert then we have encountered an HTTP request that we could not parse
			ASSERT(0);
			goto exit;
		}

		// The following should only be in response headers
		if ((headers.strReason != "") || (headers.strStatusCode != "")) {
			dwRetVal = ERROR_INVALID_DATA;
			goto exit;
		}

		// The following must be in request headers
		if ((headers.strMethod == "") || (headers.strURL == "") || (headers.strVersion == "")) {
			dwRetVal = ERROR_INVALID_DATA;
			goto exit;
		}

		IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: Request Headers:\n")));
		IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: -- Method: %hs\n"), (LPCSTR)headers.strMethod));
		IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: -- URL: %.256hs\n"), (LPCSTR)headers.strURL));
		IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: -- Version: %hs\n"), (LPCSTR)headers.strVersion));

		if (headers.strURL == g_strProxyPacURL) {
			dwRetVal = HandleProxyPacRequest();
			*pfCloseConnection = TRUE;
			goto exit;
		}

		m_strCurrMethod = headers.strMethod;

		//
		// Determine how much data is left to be read in this request
		//

		cbTotalContent = atoi(headers.strContentLength);
		cbContent = cbTotalRecved - cbHeaders;
		m_cbRequestRemain = cbTotalContent - cbContent;

		//
		// IE and Netscape both append a CRLF to POST data but do not count it as part of the 
		// content length.  If we have read 2 bytes too many then check if these last two bytes
		// are CRLF on a POST request and reset the remaining bytes to zero if so.
		//
		
		if ((m_strCurrMethod == "POST") && (m_cbRequestRemain == -2)) {
			if ((pBuffer[cbTotalRecved - 2] == '\r') && (pBuffer[cbTotalRecved - 1] == '\n')) {
				cbTotalContent += 2;
				m_cbRequestRemain = 0;
			}
		}

		if (m_cbRequestRemain < 0) {
			// If this is negative, keep reading until session is closed
			*pfCloseConnection = TRUE;
			m_cbRequestRemain = -1;
		}
	

		// Immediately respond on unsupported headers
		if ((headers.strTransferEncoding != "") && (headers.strTransferEncoding != "identity")) {
			ClearRecvBuf(buffer);
			dwRetVal = HandleUnsupportedHeader();
			goto exit;
		}

		// Do not support HTTP/1.1 in browser behind proxy
		if (headers.strVersion != "HTTP/1.0") {
			ClearRecvBuf(buffer);
			dwRetVal = HandleUnsupportedVersion();
			goto exit;
		}

		// If this is not a CONNECT request then make sure the protocol is http
		if (m_strCurrMethod != "CONNECT") {
			if (0 != strncmp(headers.strURL, "http://", 7)) {
				ClearRecvBuf(buffer);
				dwRetVal = HandleUnsupportedProtocol();
				goto exit;
			}
		}

		//
		// Authenticate to user if auth is enabled
		//

		if (m_fAuth && (m_strUser == "")) {
			IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Need to authenticate session %d.\n"), m_dwSessionId));
			
			dwRetVal = HandleAuthentication(headers, buffer, pfCloseConnection);
			if (ERROR_SUCCESS != dwRetVal) {
				goto exit;
			}

			// If HandleAuthentication does not return a user then the client
			// could not be authenticated in this pass.
			if (m_strUser == "") {
				goto exit;
			}

			IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Successfully authenticated user.\n"), m_strUser));
		}				

		//
		// Call into policy filter dll to see if the request should be filtered
		//

		dwRetVal = HandleFilterRequest(headers, buffer, &fContinue, pfCloseConnection);
		if (ERROR_SUCCESS != dwRetVal) {
			goto exit;
		}

		if (! fContinue) {
			goto exit;
		}

		//
		// Do not forward the following headers
		//
		
		headers.strProxyAuthorization = "";

		//
		// Handle a connect request
		//
		
		if (m_strCurrMethod == "CONNECT") {
			//
			// Handle the CONNECT request.  A connect request can be used to tunnel SSL
			// traffic through the HTTP proxy.
			//
			dwRetVal = HandleConnectRequest(headers.strHost, headers.strPort);
			if (ERROR_SUCCESS != dwRetVal) {
				goto exit;
			}
			
			goto exit;
		}

		//
		// Check if server socket has been opened yet.  If not, open it.  If it has been
		// opened then verify that we are connected to the correct server for this request.
		//
		
		if ((! m_sockServer.valid()) || (m_strServerName != headers.strHost)) {
			dwRetVal = ConnectServer(headers.strHost);
			if (ERROR_SUCCESS != dwRetVal) {
				ClearRecvBuf(buffer);
				dwRetVal = HandleServerNotFound();
				goto exit;
			}
		}

		//
		// Copy the content to a temporary buffer while the new headers are copied back
		// to the request buffer.  If no content is present in this packet, we do not
		// have to worry about copying content around.
		//

		if (cbContent) {
			pContent = buffContent.GetBuffer(cbContent);
			if (! pContent) {
				dwRetVal = ERROR_OUTOFMEMORY;
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory.\n")));
				goto exit;
			}
			memcpy(pContent, &pBuffer[cbHeaders], cbContent);
		}

		//
		// Update the request headers and copy them along with the content into the final
		// buffer to be sent to the server
		//
		
		headers.UpdateRequest();
		cbNewHeaders = headers.GetBufferSize();

		cbRequest = cbTotalRecved;
		cbRequest += (cbNewHeaders - cbHeaders);

		pBuffer = buffer.GetBuffer(cbRequest + 1);  // add one for a null terminating char
		if(! pBuffer) {
			dwRetVal = ERROR_OUTOFMEMORY;
			IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Insufficient memory to allocate send buffer.\n")));
			goto exit;
		}

		// Fill pBuffer with new headers.
		headers.GetRequestBuffer(pBuffer);

		// Copy the content back after new headers
		if (cbContent) {
			memcpy(&pBuffer[cbNewHeaders], pContent, cbContent);
		}
	}
	else if ((! m_fSSLTunnel) && (0 != m_cbRequestRemain)) {
		//
		// If we have already parsed the headers and just need to read remaining content
		// continuing over on a new packet, then just keep reading.
		//

		if (m_cbRequestRemain >= 0) {
			m_cbRequestRemain -= cbTotalRecved;

			// IE and Netscape both append a CRLF to POST data but do not count it as part of the 
			// content length.  If we have read 2 bytes too many then check if these last two bytes
			// are CRLF on a POST request and reset the remaining bytes to zero if so.
			if ((m_strCurrMethod == "POST") && (m_cbRequestRemain == -2)) {
				if ((pBuffer[cbTotalRecved - 2] == '\r') && (pBuffer[cbTotalRecved - 1] == '\n')) {
					m_cbRequestRemain = 0;
				}
			}
		}
		else {
			*pfCloseConnection = TRUE;
		}
		
		IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Request data remaining to be read in session %d.  Now there are %d bytes left.\n"), m_dwSessionId, m_cbRequestRemain));

		cbRequest = cbTotalRecved;
	}
	else if (m_fSSLTunnel) {
		//
		// If the current connection is just tunnelling SSL data then we do not parse the
		// data, simply send it on to the server.
		//
		cbRequest = cbTotalRecved;
	}

	//
	// Send the buffer along to the server
	//

	dwRetVal = SendData(m_sockServer, (char *)pBuffer, cbRequest);
	
exit:
	return dwRetVal;
}

DWORD CHttpSession::ProcessServerResponse(CBuffer& buffer, BOOL* pfCloseConnection)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	PBYTE pBuffer = NULL;
	CHttpHeaders headers;
	BOOL fRecvedAllHeaders = FALSE;
	int cbSent = 0;
	int cbResponse = 0;
	int cbTotalRecved = 0;
	int cbHeaders = 0;		// size of the recved headers 

	IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Response arrived from server in session %d.\n"), m_dwSessionId));

	//
	// Receive data from the server until at least all the headers have been read.
	//

	dwRetVal = RecvAllHeaders(m_sockServer, buffer, &pBuffer, &cbTotalRecved, m_cbResponseRemain, &cbHeaders);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}	

	IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Response received buffer.  Total: %d, Headers: %d, Remaining: %d.  Session %d.\n"), cbTotalRecved, cbHeaders, m_cbResponseRemain, m_dwSessionId));
	
	if ((! m_fSSLTunnel) && (0 == m_cbResponseRemain) && (FALSE == *pfCloseConnection)) {
		//
		// Now that we have all the headers, parse them.
		//
		
		PBYTE pContent;
		CBuffer buffContent;
		int cbNewHeaders; 	// size of the headers after modification
		int cbContent;		// size of the content in this packet
		int cbTotalContent;	// total amount of content that has to be recved
		
		dwRetVal = m_Parser.ParseResponse(pBuffer, cbTotalRecved, headers);
		if (ERROR_SUCCESS != dwRetVal) {
			// If hit this assert then we have encountered an HTTP request that we could not parse
			ASSERT(0);
			goto exit;
		}

		// The following should only be in request headers
		if ((headers.strProxyConnection != "") || (headers.strMethod != "") || (headers.strURL != "")) {
			dwRetVal = ERROR_INVALID_DATA;
			goto exit;
		}

		// The following must be in response headers
		if ((headers.strVersion == "") || (headers.strStatusCode == "")) {
			dwRetVal = ERROR_INVALID_DATA;
			goto exit;
		}

		IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Response Headers:\n")));
		IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: -- Version: %hs\n"), (LPCSTR)headers.strVersion));
		IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: -- Status: %hs\n"), (LPCSTR)headers.strStatusCode));

		// Immediately respond on unsupported headers
		if ((headers.strTransferEncoding != "") && (headers.strTransferEncoding != "identity")) {
			dwRetVal = HandleUnsupportedHeader();
			*pfCloseConnection = TRUE;
			goto exit;
		}

		m_fAuthInProgress = (headers.strStatusCode == "401");
		cbContent = cbTotalRecved - cbHeaders;

		//
		// If no content length is specified and the status code is not 1xx, 204, or 304, then there
		// is an unknown amount of data to be read.  So just keep reading until socket is closed.
		//
		
		if (headers.strContentLength == "" && 
			(headers.strStatusCode != "204") && 
			(headers.strStatusCode != "304") && 
			(strncmp(headers.strStatusCode, "1", 1))) {
			
			IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Content-Length was not specified.  Reading response data until server closes connection in session %d.\n"), m_dwSessionId));
			cbTotalContent = cbContent;
			m_cbResponseRemain = -1;	// Signals object that there is an unknown amount of data left to read
			*pfCloseConnection = TRUE;
		}
		else {
			cbTotalContent = atoi(headers.strContentLength);
			m_cbResponseRemain = cbTotalContent - cbContent;
		}

		//
		// Server signalled proxy to close connection
		//
		
		if (headers.strConnection == "close") {
			headers.strConnection = "";
			*pfCloseConnection = TRUE;
		}

		//
		// Copy the content to a temporary buffer while the new headers are copied back
		// to the response buffer.  If no content is present in this packet, we do not
		// have to worry about copying content around.
		//
		
		if (cbContent) {
			pContent = buffContent.GetBuffer(cbContent);
			if (! pContent) {
				dwRetVal = ERROR_OUTOFMEMORY;
				IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Out of memory.\n")));
				goto exit;
			}
			memcpy(pContent, &pBuffer[cbHeaders], cbContent);
		}

		//
		// Update the response headers
		//
		
		headers.UpdateResponse();
		cbNewHeaders = headers.GetBufferSize();

		//
		// Determine how much data is left to be read in this response
		//

		cbResponse = cbTotalRecved;
		cbResponse += (cbNewHeaders - cbHeaders);
		
		pBuffer = buffer.GetBuffer(cbResponse + 1);  // add one for a null terminating char
		if(! pBuffer) {
			dwRetVal = ERROR_OUTOFMEMORY;
			IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Insufficient memory to allocate send buffer.\n")));
			goto exit;
		}

		// Copy response buffer
		headers.GetResponseBuffer(pBuffer);

		// Copy content back after headers
		if (cbContent) {
			memcpy(&pBuffer[cbNewHeaders], pContent, cbContent);
		}
	}
	else if ((! m_fSSLTunnel) && (*pfCloseConnection)) {
		//
		// If pfCloseConnection is true then the proxy should assume the data does not have any
		// headers and is just content that has to be read and forwarded to the server.  When
		// the data is completely read, the server will close the connection.
		//
		
		IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Reading response data until the connection is closed.\n")));
		cbResponse = cbTotalRecved;
	}
	else if ((! m_fSSLTunnel) && (0 != m_cbResponseRemain)) {
		//
		// If we have already parsed the headers and just need to read remaining content
		// continuing over on a new packet, then just keep reading.
		//

		if (m_cbResponseRemain >= 0) {
			m_cbResponseRemain -= cbTotalRecved;
		}
		else {
			*pfCloseConnection = TRUE;
		}
		
		IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Response data remaining to be read.  Now there are %d bytes left.\n"), m_cbResponseRemain));

		cbResponse = cbTotalRecved;
	}
	else if (m_fSSLTunnel) {
		//
		// If the current connection is just tunnelling SSL data then we do not parse the
		// data, simply send it on to the server.
		//
		cbResponse = cbTotalRecved;
	}

	//
	// Send the data along to the client
	//

	dwRetVal = SendData(m_sockClient, (char *)pBuffer, cbResponse);

exit:
	return dwRetVal;
}

DWORD CHttpSession::HandleConnectRequest(stringi& strHost, stringi& strPort)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	//
	// Only accept CONNECT requests on HTTPS port.  CONNECT requests on other ports represent
	// a security risk and therefore, are rejected.
	//

	if (strPort == DEFAULT_HTTPS_PORT_SZ) {
		dwRetVal = ConnectServer(strHost, DEFAULT_HTTPS_PORT);
		if (ERROR_SUCCESS != dwRetVal) {
			goto exit;
		}

		CHttpHeaders response;
		response.strStatusCode = "200";
		response.strReason = "Connection established";
		response.UpdateResponse();
		
		dwRetVal = SendCustomPacket(response, FALSE);
		if (ERROR_SUCCESS == dwRetVal) {
			m_fSSLTunnel = TRUE;
		}
	}
	else {
		IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: A CONNECT request was attempted on a port other than 443 - this is not allowed.\n")));
		dwRetVal = ERROR_INVALID_PARAMETER;
	}
	
exit:
	return dwRetVal;
}

DWORD CHttpSession::HandleAuthentication(CHttpHeaders& headers, CBuffer& buffer, BOOL* pfCloseConnection)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	CHttpHeaders response;
	BOOL fAuthSuccess = FALSE;
	string strNTLMAuth;
	int cbNTLMAuthOut = 128;
	int cbUser = 128;

	while (1) {
		// Get authorization data
		m_Parser.ParseNTLMAuthorization(headers);

		// If there is no authorization data then just return the authentication protocol
		if (headers.strProxyAuthorization != "") {			
			GetMaxNTLMBuffSize((DWORD *)&cbNTLMAuthOut);
			strNTLMAuth.reserve(cbNTLMAuthOut);
			m_strUser.reserve(cbUser);

			dwRetVal = HandleNTLMAuth(headers.strProxyAuthorization, strNTLMAuth.get_buffer(), cbNTLMAuthOut, m_strUser.get_buffer(), cbUser, &m_auth);
			if (ERROR_INSUFFICIENT_BUFFER == dwRetVal) {
				cbUser *= 2;
				FreeNTLMHandles(&m_auth);
				m_auth.m_Conversation = NTLM_NO_INIT_CONTEXT;
				continue;
			}
			else if (ERROR_SUCCESS != dwRetVal) {
				FreeNTLMHandles(&m_auth);
				m_auth.m_Conversation = NTLM_NO_INIT_CONTEXT;
				goto exit;
			}

			if (NTLM_DONE == m_auth.m_Conversation) {
				FreeNTLMHandles(&m_auth);
				m_auth.m_Conversation = NTLM_NO_INIT_CONTEXT;
			}

			//
			// If m_strUser is empty then we did not authenticate on this pass.  In this case strNTLMAuth should have data in
			// which case you will return this to the client.  If strNTLMAuth is also NULL then we failed to authenticate and
			// should start from the beginning again.
			//
			
			if (m_strUser != "") {
				fAuthSuccess = TRUE;
			}
			else if (strNTLMAuth != "") {
				response.strConnection = "Keep-Alive";
				response.strProxyConnection = "Keep-Alive";
				response.strProxyAuthenticate = "NTLM ";
				response.strProxyAuthenticate += strNTLMAuth;
			}
			else {
				*pfCloseConnection = TRUE;
				//response.strProxyConnection = "close";
				response.strProxyAuthenticate = "NTLM";
			}
		}
		else {
			*pfCloseConnection = TRUE;
			//response.strProxyConnection = "close";
			response.strProxyAuthenticate = "NTLM";
		}
		
		break;
	}

	// 
	// If we did not successfully authenticate, then respond to the client now with
	// a 407 error.
	//
	
	if (! fAuthSuccess) {
		ClearRecvBuf(buffer);
		
		response.strStatusCode = "407";
		response.strReason = "Proxy Authentication Required";

		char szBuf[5];
		response.strContentLength = _itoa(g_cchProxyAuthFailed, szBuf, 10);

		response.UpdateResponse();

		// Send headers
		dwRetVal = SendCustomPacket(response, FALSE);
		if (ERROR_SUCCESS != dwRetVal) {
			goto exit;
		}

		// Send "auth failed" content message
		dwRetVal = SendData(m_sockClient, g_szProxyAuthFailed, g_cchProxyAuthFailed);
		if (ERROR_SUCCESS != dwRetVal) {
			goto exit;
		}

		IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Sent authentication response with status 407, to client in session %d.\n"), m_dwSessionId));
	}

exit:
	return dwRetVal;
}

DWORD DoFilter(PPROXY_HTTP_REQUEST pRequest)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	__try {
		dwRetVal = ProxyFilterHttpRequest(pRequest);
	}
	__except (1) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Exception occured in ProxyFilterHttpRequest - aborting session.\n")));
		dwRetVal = ERROR_INTERNAL_ERROR;
	}
	return dwRetVal;
}

DWORD CHttpSession::HandleFilterRequest(const CHttpHeaders& headers, CBuffer& buffer, BOOL* pfContinue, BOOL* pfCloseConnection)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	char szNewURLBuf[128];
	int cbNewURL = sizeof(szNewURLBuf);
	char* pszNewURL = szNewURLBuf;
	int cResizeBuf = 0;

	ASSERT(pfContinue);
	ASSERT(pfCloseConnection);

	// If no filter is provided then just return success and leave
	// the request unchanged.
	if (NULL == ProxyFilterHttpRequest) {
		*pfContinue = TRUE;
		return ERROR_SUCCESS;
	}
	
	*pfContinue = FALSE;
	*pfCloseConnection = FALSE;

	stringi strCanonicalizedURL;
	strCanonicalizedURL.reserve(headers.strURL.capacity()); // Canonicalized string is no bigger than original
	MyInternetCanonicalizeUrlA(headers.strURL, strCanonicalizedURL.get_buffer());	

	while(1) {
		PROXY_HTTP_REQUEST request;
		request.dwSize = sizeof(request);
		request.psaClient = (SOCKADDR_STORAGE*)&m_saClient;
		request.cbsaClient = sizeof(m_saClient);
		request.szUser = m_strUser;
		request.cchUser = m_strUser.length();
		request.szURL = strCanonicalizedURL;
		request.cchURL = strCanonicalizedURL.length();
		request.szURLOut = pszNewURL;
		request.cbURLOut = cbNewURL;
		request.psaProxy = (SOCKADDR_STORAGE*)&m_saPrivate;
		request.cbsaProxy = sizeof(m_saPrivate);

		pszNewURL[0] = '\0';
		
		dwRetVal = DoFilter(&request);
		if (ERROR_INSUFFICIENT_BUFFER == dwRetVal) {
			//
			// Only give filter two chances to resize buffer then close session.
			//
			cResizeBuf++;
			if (cResizeBuf > 1) {
				dwRetVal = ERROR_INTERNAL_ERROR;
				goto exit;
			}
			
			//
			// Alloc larger buffer if necessary
			//
			cbNewURL = request.cbURLOut;
			pszNewURL = new char[cbNewURL];
			if (! pszNewURL) {
				dwRetVal = ERROR_OUTOFMEMORY;
				goto exit;
			}
		}
		else if (ERROR_SUCCESS != dwRetVal) {
			//
			// Internal error in ProxyFilterHttpRequest
			//
			goto exit;
		}
		else {
			//
			// Function succeeded.  If URLs are the same afterwards then just continue with
			// the request.  Otherwise, handle the denied request.
			//
			if (strCanonicalizedURL == request.szURLOut) {
				*pfContinue = TRUE;
				break;
			}
			else if ((0 == request.cbURLOut) || (0 == strcmp(request.szURLOut, ""))) {
				*pfCloseConnection = TRUE;
				dwRetVal = HandleForbiddenRequest();
				goto exit;
			}
			else {
				ClearRecvBuf(buffer);
				dwRetVal = HandleDeniedRequest(request.szURLOut);
				goto exit;
			}
		}
	}

exit:
	if (szNewURLBuf != pszNewURL) {
		delete[] pszNewURL;
	}
	return dwRetVal;
}

DWORD CHttpSession::HandleUnsupportedHeader(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	CHttpHeaders response;

	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Encountered an unsupported header - returning 'Bad Gateway' to client.\n")));

	response.strStatusCode = "502";
	response.strReason = "Bad Gateway";

	char szBuf[5];
	response.strContentLength = _itoa(g_cchHeaderNotSupported, szBuf, 10);

	response.UpdateResponse();

	// Send headers
	dwRetVal = SendCustomPacket(response, FALSE);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}

	// Send "Bad Gateway"
	dwRetVal = SendData(m_sockClient, g_szHeaderNotSupported, g_cchHeaderNotSupported);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}

exit:
	return dwRetVal;
}

DWORD CHttpSession::HandleUnsupportedVersion(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	CHttpHeaders response;

	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Encountered an unsupported version - returning 'HTTP Version not supported' to client.\n")));

	response.strStatusCode = "505";
	response.strReason = "HTTP Version not supported";

	char szBuf[5];
	response.strContentLength = _itoa(g_cchVersionNotSupported, szBuf, 10);

	response.UpdateResponse();

	// Send headers
	dwRetVal = SendCustomPacket(response, FALSE);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}

	// Send "Bad Gateway"
	dwRetVal = SendData(m_sockClient, g_szVersionNotSupported, g_cchVersionNotSupported);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}

exit:
	return dwRetVal;
	
}

DWORD CHttpSession::HandleUnsupportedProtocol(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	CHttpHeaders response;
	
	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: Encountered an unsupported protocol - returning 'Bad Gateway' to client.\n")));

	response.strStatusCode = "502";
	response.strReason = "Bad Gateway";

	char szBuf[5];
	response.strContentLength = _itoa(g_cchProtocolNotSupported, szBuf, 10);

	response.UpdateResponse();

	// Send headers
	dwRetVal = SendCustomPacket(response, FALSE);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}

	// Send "Bad Gateway"
	dwRetVal = SendData(m_sockClient, g_szProtocolNotSupported, g_cchProtocolNotSupported);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}

exit:
	return dwRetVal;
}

DWORD CHttpSession::HandleDeniedRequest(const char* pszURL)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	CHttpHeaders response;

	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: HTTP request has been denied.  A new URL is being substituted.\n")));

	response.strStatusCode = "302";
	response.strReason = "Found";
	response.strContentLength = 0;
	response.strContentType = "text/html";
	response.strLocation = pszURL;

	response.UpdateResponse();

	// Send "Found" message with new URL
	dwRetVal = SendCustomPacket(response, FALSE);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}

exit:
	return dwRetVal;
}

DWORD CHttpSession::HandleForbiddenRequest(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	CHttpHeaders response;

	IFDBG(DebugOut(ZONE_OUTPUT, _T("WebProxy: HTTP request has been denied.  Returning forbidden.\n")));

	response.strStatusCode = "403";
	response.strReason = "Forbidden";
	response.strContentLength = 0;

	response.UpdateResponse();

	// Send "Found" message with new URL
	dwRetVal = SendCustomPacket(response, FALSE);
	if (ERROR_SUCCESS != dwRetVal) {
		goto exit;
	}

exit:
	return dwRetVal;
}


DWORD CHttpSession::HandleServerNotFound(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	
	IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Sending 502 response to client.\n")));

	CHttpHeaders response;
	response.strStatusCode = "502";
	response.strReason = "Proxy Error (Invalid data)";

	char szBuf[5];
	response.strContentLength = _itoa(g_cchURLNotFound, szBuf, 10);

	response.UpdateResponse();

	// Send "proxy error"
	dwRetVal = SendCustomPacket(response, FALSE);
	if (ERROR_SUCCESS == dwRetVal) {
		dwRetVal = SendData(m_sockClient, g_szURLNotFound, g_cchURLNotFound);
	}
	
	return dwRetVal;
}

DWORD CHttpSession::HandleProxyPacRequest(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	CHAR szProxyPac[1024];
	int ciProxyPac;
	
	IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Sending proxy.pac file to client.\n")));

	ULONG ulIpMask = inet_addr(g_strPrivateMask);
	ULONG ulIpAddr = m_saPrivate.sin_addr.S_un.S_addr;
	
	sockaddr_in dest;
	dest.sin_addr.S_un.S_addr = (ulIpAddr  & ulIpMask);
	string strDest = inet_ntoa(dest.sin_addr);

	sprintf(szProxyPac, gc_szProxyPac, (LPCSTR)strDest, (LPCSTR)g_strPrivateMask, (LPCSTR)g_strPrivateAddr, g_iPort);
	ciProxyPac = strlen(szProxyPac);

	CHttpHeaders response;
	response.strStatusCode = "200";
	response.strReason = "OK";
	
	char szBuf[5];
	response.strContentLength = _itoa(ciProxyPac, szBuf, 10);

	response.UpdateResponse();

	dwRetVal = SendCustomPacket(response, FALSE);
	if (ERROR_SUCCESS == dwRetVal) {
		dwRetVal = SendData(m_sockClient, szProxyPac, ciProxyPac);
	}

	return dwRetVal;
}

DWORD CHttpSession::ConnectServer(const char* szHost, int iPort)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	stringi strNewHost;
	sockaddr_in saServer;

	IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Trying to connect to host %S on port %d in session %d.\n"), szHost, ((iPort == -1) ? DEFAULT_HTTP_PORT : iPort), m_dwSessionId));

	// Create socket
	m_sockServer = socket(PF_INET, SOCK_STREAM, 0);
	if (! m_sockServer.valid()) {
		dwRetVal = WSAGetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error creating socket %d.\n"), dwRetVal));
		goto exit;
	}

	// Get host and port to connect on
	int iSpecifiedPort;
	strNewHost.reserve(strlen(szHost));
	GetHostAndPort(szHost, strNewHost.get_buffer(), &iSpecifiedPort);
	if (iPort == -1) {
		iPort = iSpecifiedPort;
	}

	// Convert host name to IP address
	dwRetVal = GetHostAddr(strNewHost, iPort, &saServer);
	if (ERROR_SUCCESS != dwRetVal) {
		m_sockServer.close();
		goto exit;
	}

	// Connect to host
	if (SOCKET_ERROR == connect(m_sockServer, (sockaddr*)&saServer, sizeof(sockaddr))) {
		dwRetVal = WSAGetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Connect call failed with error %d.\n"), dwRetVal));
		m_sockServer.close();
		goto exit;
	}

	m_strServerName = szHost;
	
	IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Connected to host %S on port %d in session %d.\n"), szHost, ((iPort == -1) ? DEFAULT_HTTP_PORT : iPort), m_dwSessionId));

exit:
	return dwRetVal;
}

DWORD CHttpSession::RecvAllHeaders(SOCKET sock, CBuffer& buffer, PBYTE* ppBuffer, int* pcbTotalRecved, int cbRemain, int* pcbHeaders)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	ASSERT(pcbTotalRecved);
	ASSERT(pcbHeaders);
	ASSERT(ppBuffer);
	
	//
	// Keep recving data until we have all the headers.  cbHeaders will be a positive integer representing
	// the size of the headers when they have been recved.
	//

	*pcbTotalRecved = 0;	
	*pcbHeaders = 0;
	while (0 == *pcbHeaders) {
		int cbBuffer;
		int cbRecved = 0;
		int cbRecvBufferIn;
		PBYTE pBuff;

		// If we have recved some data and are still in this loop then we have to recv more.  Double the buffer
		// size and proceed to recv more data.
		cbBuffer = *pcbTotalRecved;
		if (0 != cbBuffer) {
			cbBuffer *= 2;
		}
		else {
			cbBuffer = DEFAULT_BUFFER_SIZE - 1;
		}

		*ppBuffer = buffer.GetBuffer(cbBuffer + 1, *pcbTotalRecved ? TRUE : FALSE);	// add 1 to append '\0'
		if(! *ppBuffer) {
			dwRetVal = ERROR_OUTOFMEMORY;
			IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Insufficient memory to allocate recv buffer.\n")));
			goto exit;
		}

		pBuff = *ppBuffer;

		cbRecvBufferIn = buffer.GetSize() - *pcbTotalRecved - 1;
		if ((cbRemain > 0) && (cbRecvBufferIn > cbRemain)) {
			//
			// If we have read all the headers and size of recv buffer is larger than what is left
			// that has to be read, then just read the amount that has to be read for this response.
			// Doing this ensures the start of a response will always be at the start of a packet.
			//
			cbRecvBufferIn = cbRemain;
		}

		
		dwRetVal = MyRecv(sock, &pBuff[*pcbTotalRecved], cbRecvBufferIn, &cbRecved);
		if (ERROR_SUCCESS != dwRetVal) {
			goto exit;
		}

		IFDBG(DebugOut(ZONE_PACKETS, _T("Received the following packet:\n")));
		IFDBG(DumpBuff(pBuff, cbRecved));

		*pcbTotalRecved += cbRecved;

		// If we have already parsed the headers or we are just tunneling SSL data, 
		// then break from this loop now.
		if ((0 != cbRemain) || m_fSSLTunnel) {
			break;
		}

		if (*pcbTotalRecved > m_iMaxHeadersSize) {
			dwRetVal = ERROR_INVALID_DATA;
			goto exit;
		}

		// Both IE and Netscape append CRLF to POST but do not count it as part of the content-length.  It is possible
		// for these two bytes to be sent in their own packet and get unrecognized until this point.
		if ((m_strCurrMethod == "POST") && (cbRecved == 2) && (pBuff[0] == '\r') && (pBuff[1] == '\n')) {
			m_cbRequestRemain = 2; // Reset this to two bytes left
			break;
		}

		pBuff[*pcbTotalRecved] = '\0';

		// This call will return 0 if all the headers have not been recved
		*pcbHeaders = GetHeadersLength(pBuff);
	}

exit:
	return dwRetVal;
}

DWORD CHttpSession::MyRecv(SOCKET sock, PBYTE pBuffer, int cbBuffer, int* piBytesRecved)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	FD_SET sockSet;

	ASSERT(piBytesRecved);
	ASSERT(INVALID_SOCKET != sock);

	timeval timeout = {0};
	timeout.tv_sec = 10;

	FD_ZERO(&sockSet);
	FD_SET(sock, &sockSet);

	SessionUnlock();
	int iSockets = select(0,&sockSet,NULL,NULL,&timeout);
	SessionLock();

	if ((SOCKET_ERROR == iSockets) || (0 == iSockets)) {
		dwRetVal = ERROR_TIMEOUT;
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: MyRecv timed out trying to recv data\n")));
		goto exit;
	}

	*piBytesRecved = recv(sock, (char *)pBuffer, cbBuffer, 0);
	if ((SOCKET_ERROR == *piBytesRecved)) {
		dwRetVal = WSAGetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Recv on sock %d, buf 0x%08X, len %d, failed with error %d.\n"), sock, pBuffer, cbBuffer, dwRetVal));
		goto exit;
	}

	if (0 == *piBytesRecved) {
		dwRetVal = ERROR_NO_DATA;
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Recv on sock %d, buf 0x%08X, len %d, cannot receive anymore data.\n"), sock, pBuffer, cbBuffer));
		goto exit;
	}

exit:
	return dwRetVal;
}

void CHttpSession::ClearRecvBuf(CBuffer& buffer)
{
	DWORD dwRetVal;
	PBYTE pBuffer = buffer.GetBuffer(0, FALSE);
	
	while (m_cbRequestRemain > 0) {
		int cbRecved = 0;
		dwRetVal = MyRecv(m_sockClient, pBuffer, buffer.GetSize(), &cbRecved);
		if ((ERROR_SUCCESS != dwRetVal) || (0 == cbRecved)) {
			break;
		}
		m_cbRequestRemain -= cbRecved;
	}

	m_cbRequestRemain = 0;
}

DWORD CHttpSession::GetHeadersLength(const PBYTE pBuffer)
{
	DWORD dwLenEnd = 4;
	ASSERT(pBuffer);

	// Read until CRLF x2
	char* chEnd = strstr((char*)pBuffer, "\r\n\r\n");
	if (chEnd == NULL) {
		// Or LFx2
		dwLenEnd = 2;
		chEnd = strstr((char*)pBuffer, "\n\n");
		if (chEnd == NULL) {
			return 0;
		}
	}

	return (chEnd - (char*)pBuffer) + dwLenEnd;
}

DWORD CHttpSession::GetHostAddr(const char* szHost, int iPort, sockaddr_in* psa)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	struct addrinfo aiHints; 
	struct addrinfo* paiServer = NULL;
	char szPort[16];

	ASSERT(psa);
	memset(psa, 0, sizeof(sockaddr));
	
	memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = PF_INET;
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_protocol = IPPROTO_TCP;

	_itoa(iPort, szPort, 10);
	
	if (0 != getaddrinfo(szHost, szPort, &aiHints, &paiServer)) {
		dwRetVal = WSAGetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error getting addr info for host %a: %d.\n"), szHost, dwRetVal));
		m_sockServer.close();
		goto exit;
	}

	ASSERT(paiServer->ai_addr);
	*psa = *(sockaddr_in*)paiServer->ai_addr;

exit:
	if (paiServer) {
		freeaddrinfo(paiServer);
	}
	return dwRetVal;
}

void CHttpSession::GetHostAndPort(const char* szSource, char* szHost, int* piPort)
{
	strcpy(szHost, szSource);
	char* pszPort = strchr(szHost, ':');
	if (NULL == pszPort) {
		*piPort = DEFAULT_HTTP_PORT;
		return;
	}
	
	*pszPort = '\0';
	pszPort++;
	*piPort = atoi(pszPort);
	if (0 == *piPort) {
		*piPort = DEFAULT_HTTP_PORT;
	}
}

DWORD CHttpSession::SendCustomPacket(CHttpHeaders& headers, BOOL fRequest)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	CBuffer buffer;
	PBYTE pBuffer;
	SOCKET sock;
	
	int cbSize = headers.GetBufferSize();
	
	pBuffer = buffer.GetBuffer(cbSize + 1); // Add one for terminating null char
	if (! pBuffer) {
		dwRetVal = ERROR_OUTOFMEMORY;
		goto exit;
	}

	// If this is a request then send data to client, otherwise to server.
	if (fRequest) {
		headers.GetRequestBuffer(pBuffer);
		sock = m_sockServer;
	}
	else {
		headers.GetResponseBuffer(pBuffer);
		sock = m_sockClient;
	}

	dwRetVal = SendData(sock, (char *)pBuffer, cbSize);
	if (ERROR_SUCCESS != dwRetVal) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Send returned error %d with %d bytes.\n"), dwRetVal));
		goto exit;
	}

exit:
	return dwRetVal;
}

DWORD CHttpSession::SendData(SOCKET sock, const char* szData, int cbData)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	
	int cbSent = send(sock, szData, cbData, 0);
	if (SOCKET_ERROR == cbSent) {
		dwRetVal = WSAGetLastError();
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Send returned error %d with %d bytes.\n"), dwRetVal, cbSent));
		goto exit;
	}

#ifdef DEBUG
	if (cbSent != cbData) {
		IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Tried to send %d bytes but actually sent %d bytes, in session %d.\n"), cbData, cbSent, m_dwSessionId));
		ASSERT(0);
	}
#endif // DEBUG
	
exit:
	return dwRetVal;
}


