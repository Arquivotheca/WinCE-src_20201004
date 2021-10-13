//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include "session.h"
#include "proxydbg.h"
#include "filter.h"
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


CHttpSession::CHttpSession(void) :
    m_fRunning(FALSE),
    m_fSSLTunnel(FALSE),
    m_fSSLTunnelThruSecondProxy(FALSE),
    m_fAuthInProgress(FALSE),
    m_fChunked(FALSE),
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
    
    SessionLock();

    ASSERT(! m_fRunning);

    // Set session data
    m_sockClient = pSettings->sockClient;
    m_saClient = pSettings->saClient;

    m_fRunning = TRUE;

    memset(&m_auth, 0, sizeof(AUTH_NTLM));
    m_auth.m_Conversation = NTLM_NO_INIT_CONTEXT;

    // Create a thread to handle the session
    g_pThreadPool->ScheduleEvent(SessionThread, this);

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

    IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: Closing client socket in session %d, sock=%d\n"), m_dwSessionId, m_sockClient));
    IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: If valid, we will also close server socket in session %d, sock=%d\n"), m_dwSessionId, m_sockServer));
    
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

    IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: Session %d has been started.\n"), m_dwSessionId));

    timeval timeout = {0};
    timeout.tv_sec = g_pSettings->iSessionTimeout;

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
            IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: Session %d has been signalled to shutdown.\n"), m_dwSessionId));
            break;
        }

        if (SOCKET_ERROR == iSockets) {
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Select call failed in session %d, error: %d.\n"), m_dwSessionId, WSAGetLastError()));
            break;
        }

        if (0 == iSockets) {
            IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: Session %d has expired\n"), m_dwSessionId));
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
                IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: The client socket has been signalled to close.  Closing session %d.\n"), m_dwSessionId));
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

            dwErr = ProcessMessage(buffer, &fCloseSessionReq, TRUE);
            if (ERROR_SUCCESS != dwErr) {
                IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Due to an internal error when processing a request, session %d is being aborted: %d.\n"), m_dwSessionId, dwErr));
                break;
            }

            if (fCloseSessionReq && (0 == m_cbRequestRemain)) {
                IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: Proxy was signalled to close session by a request, session %d is being closed.\n"), m_dwSessionId));
                shutdown(m_sockClient, SD_SEND);
                continue;
            }
        }
        
        if (fServerConnected && FD_ISSET(m_sockServer, &sockSet)) {
            //
            // Packet arrived from server
            //

            dwErr = ProcessMessage(buffer, &fCloseSessionResp, FALSE);
            if (ERROR_SUCCESS != dwErr) {
                IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Proxy was signalled to close session by a response or an internal error occured, session %d is being closed: %d.\n"), m_dwSessionId, dwErr));
                break;
            }

            if (fCloseSessionResp && (0 == m_cbResponseRemain)) {
                IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: Proxy was signalled to close session by a response, session %d is being closed.\n"), m_dwSessionId));
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

    dwErr = g_pSessionMgr->RemoveSession(m_dwSessionId);

#ifdef DEBUG
    if (ERROR_SUCCESS != dwErr) {
        IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Could not delete session %d, error: %d - potential memory leak\n"), m_dwSessionId, dwErr));
    }
#endif // DEBUG

}

DWORD CHttpSession::ProcessMessage(CBuffer& buffer, BOOL* pfCloseConnection, BOOL fRequest)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    int cchBuffer = 0;
    int cchSent = 0;
    int cchHeaders = 0;        // size of the recved headers 

    IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: %s(s) arrived from server in session %d.\n"), (fRequest?L"Request":L"Response"), m_dwSessionId));

    // This method is generic for both requests and responses.  Need to set the following
    // variables based on fRequest parameter.
    int* pcbRemain = fRequest ? &m_cbRequestRemain : &m_cbResponseRemain;
    SOCKET sock = fRequest ? m_sockClient : m_sockServer;

    //
    // Receive data until at least all the headers have been read.
    //

    dwRetVal = ProxyRecv(sock, buffer, &cchBuffer, *pcbRemain, &cchHeaders);
    if (ERROR_SUCCESS != dwRetVal) {
        goto exit;
    }
        
    IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Received buffer.  Total: %d, Headers: %d, Remaining: %d.  Session %d.\n"), cchBuffer, cchHeaders, m_cbResponseRemain, m_dwSessionId));
    
    //
    // We may have read more than one request since multiple requests can be pipelined.
    //
    while(1) {
        // The proxy will only handle one request/response.  If more than one message is in the buffer, this
        // is determined by the fact that we sent out less data than we received.
        if (fRequest) {
            dwRetVal = HandleClientMessage(buffer, &cchBuffer, cchHeaders, &cchSent, pfCloseConnection);
        }
        else {
            dwRetVal = HandleServerMessage(buffer, &cchBuffer, cchHeaders, &cchSent, pfCloseConnection);
        }

        if (ERROR_SUCCESS != dwRetVal) {
            goto exit;
        }  

        if (m_fChunked) {
            PBYTE pBuffer = buffer.GetBuffer(0, FALSE);
            ASSERT(pBuffer);

            IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: Detected chunked transfer in session %d.\n"), m_dwSessionId));
            
            // Copy remaining data to beginning of buffer, reset sizes, and keep going
            if (cchBuffer > cchSent) {
                memmove(pBuffer, pBuffer + cchSent, cchBuffer - cchSent);
            }            
            cchBuffer -= cchSent;
            cchSent = 0;
            pBuffer[cchBuffer] = '\0';
            
            // Receive all chunks and forward it to the client
            dwRetVal = HandleChunked(buffer, &cchBuffer, &cchSent, fRequest);
            if (ERROR_SUCCESS != dwRetVal) {
                goto exit;
            }
        }

        if (*pfCloseConnection) {
            break;
        }

        if ((cchSent > 0) && (cchSent < cchBuffer)) {
            PBYTE pBuffer = buffer.GetBuffer(0, FALSE);
            ASSERT(pBuffer);

            IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Detected pipelined %s.\n"), (fRequest?L"request":L"response")));

            // Copy remaining data to beginning of buffer, reset sizes, and keep going
            memmove(pBuffer, pBuffer + cchSent, cchBuffer - cchSent);
            cchBuffer -= cchSent;
            cchSent = 0;
            pBuffer[cchBuffer] = '\0';
            
            cchHeaders = GetHeadersLength(pBuffer);
            if (0 == cchHeaders) {
                dwRetVal = ProxyRecv(sock, buffer, &cchBuffer, *pcbRemain, &cchHeaders);
                if (ERROR_SUCCESS != dwRetVal) {
                    goto exit;
                }
            }
        }
        else {
            break;
        }
    }

exit:
    return dwRetVal;
}

DWORD CHttpSession::HandleClientMessage(CBuffer& buffer, int* pcchBuffer, int cchHeaders, int* pcchSent, BOOL* pfCloseConnection)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    CHttpHeaders headers;

    *pcchSent = 0;
    
    PBYTE pBuffer = buffer.GetBuffer(0, FALSE);
    ASSERT(pBuffer);

    if (m_fSSLTunnel) {
        //
        // If the current connection is just tunnelling SSL data then we do not parse the
        // data, simply send it on to the server.
        //
        *pcchSent = *pcchBuffer;
    }
    else if (0 != m_cbRequestRemain) {
        //
        // If we have already parsed the headers and just need to read remaining content
        // continuing over on a new packet, then just keep reading.
        //

        if (m_cbRequestRemain >= 0) {
            m_cbRequestRemain -= *pcchBuffer;

            // IE and Netscape both append a CRLF to POST data but do not count it as part of the 
            // content length.  If we have read 2 bytes too many then check if these last two bytes
            // are CRLF on a POST request and reset the remaining bytes to zero if so.
            if ((m_strCurrMethod == gc_MethodPost) && (m_cbRequestRemain == -2)) {
                if ((*pcchBuffer >= 2) && (pBuffer[*pcchBuffer - 2] == '\r') && (pBuffer[*pcchBuffer - 1] == '\n')) {
                    m_cbRequestRemain = 0;
                }
            }
        }
        else {
            *pfCloseConnection = TRUE;
        }
        
        IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: Request data remaining to be read in session %d.  Now there are %d bytes left.\n"), m_dwSessionId, m_cbRequestRemain));

        *pcchSent = *pcchBuffer;
    }
    else {
        //
        // Now that we have all the headers, parse them.
        //

        BOOL fContinue = TRUE;
        BOOL fAdditionalContent = FALSE;
        
        PBYTE pContent = NULL;
        CBuffer buffContent;
        int cbNewHeaders = 0;     // size of the headers after modification
        int cbContent = 0;          // size of the content in this packet
        int cbTotalContent = 0; // total amount of content that has to be recved
        
        dwRetVal = g_pParser->ParseRequest(pBuffer, *pcchBuffer, headers);
        if (ERROR_SUCCESS != dwRetVal) {
            // If hit this assert then we have encountered an HTTP request that we could not parse
            ASSERT(0);
            goto exit;
        }

        ASSERT(headers.strReason == ""); 
        ASSERT(headers.strStatusCode == "");

        // The following must be in request headers
        if ((headers.strMethod == "") || (headers.strURL == "") || (headers.strVersion == "")) {
            dwRetVal = ERROR_INVALID_DATA;
            goto exit;
        }    

        IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: Request Headers:\n")));
        IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: -- Method: %hs\n"), (LPCSTR)headers.strMethod));
        IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: -- Host: %hs\n"), (LPCSTR)headers.strHost));
        IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: -- Version: %hs\n"), (LPCSTR)headers.strVersion));

        if ((headers.strHost == "") && (headers.strVersion == gc_HTTP11)) {
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: No host header was present, returning Bad Request.\n")));
            dwRetVal = SendMessage(m_sockClient, "400", gc_Reason400, NULL, 0);
            *pfCloseConnection = TRUE;
            goto exit;
        }

        g_pSettings->Lock();
        if (0 == strncmp(headers.strURL, g_pSettings->strProxyPacURL, g_pSettings->strProxyPacURL.length())) {
            g_pSettings->Unlock();
            dwRetVal = HandleProxyPacRequest();
            *pfCloseConnection = TRUE;
            goto exit;
        }
        g_pSettings->Unlock();

        m_strCurrMethod = headers.strMethod;

        if (headers.strTransferEncoding == gc_TEChunked) {
            m_fChunked = TRUE;
        }

        //
        // Determine how much data is left to be read in this request
        //

        cbTotalContent = atoi(headers.strContentLength);
        if (*pcchBuffer - cchHeaders > *pcchBuffer ){ 
            dwRetVal = ERROR_INVALID_DATA;
            goto exit;
        }
        cbContent = *pcchBuffer - cchHeaders;
        m_cbRequestRemain = cbTotalContent - cbContent;

        //
        // IE and Netscape both append a CRLF to POST data but do not count it as part of the 
        // content length.  If we have read 2 bytes too many then check if these last two bytes
        // are CRLF on a POST request and reset the remaining bytes to zero if so.
        //
        
        if ((m_strCurrMethod == gc_MethodPost) && (m_cbRequestRemain == -2)) {
            if ((*pcchBuffer >= 2) && (pBuffer[*pcchBuffer - 2] == '\r') && (pBuffer[*pcchBuffer - 1] == '\n')) {
                cbTotalContent += 2;
                m_cbRequestRemain = 0;
            }
        }

        // Only send headers if chunked or if we detected a pipelined request
        if (m_fChunked || (m_cbRequestRemain < 0)) {
            m_cbRequestRemain = 0;
            fAdditionalContent = TRUE;
        }    

        // If this is not a CONNECT request then make sure the protocol is http
        if (m_strCurrMethod != gc_MethodConnect) {
            if (0 != strncmp(headers.strURL, "http://", 7)) {
                ClearRecvBuf(buffer);
                dwRetVal = SendMessage(m_sockClient, "502", gc_Reason502, g_pErrors->szProtocolNotSupported, g_pErrors->cchProtocolNotSupported);
                goto exit;
            }
        }

        //
        // If authentication is in progress, then finish before calling into filter
        //

        if ((m_strUser == "") && (headers.strProxyAuthorization != "")) {
            IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Authenticating session %d.\n"), m_dwSessionId));
            
            dwRetVal = HandleAuthentication(headers, buffer, pfCloseConnection);
            if (ERROR_SUCCESS != dwRetVal) {
                goto exit;
            }

            // If HandleAuthentication does not return a user then the client
            // could not be authenticated in this pass.
            if (m_strUser == "") {
                goto exit;
            }

            IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Successfully authenticated user %hs.\n"), (LPCSTR)m_strUser));
        }

        //
        // Call into proxy filter
        //
        
        dwRetVal = HandleFilterRequest(headers, buffer, &fContinue, pfCloseConnection);
        if ((ERROR_NOT_AUTHENTICATED == dwRetVal) && (m_strUser == "")) {
            // Need to authenticate before calling filter
            IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Filter requires authentication in session %d.\n"), m_dwSessionId));
            dwRetVal = HandleAuthentication(headers, buffer, pfCloseConnection);
            goto exit;
        }
        else if (ERROR_SUCCESS != dwRetVal) {
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
        // Handle a CONNECT request
        //
        
        if (m_strCurrMethod == gc_MethodConnect) {
            //
            // Handle the CONNECT request.  A connect request can be used to tunnel SSL
            // traffic through the HTTP proxy.
            //
            if (g_pSettings->strSecondProxyHost == "") {
                dwRetVal = HandleConnectRequest(headers.strHost, headers.strPort);
                if (ERROR_SUCCESS != dwRetVal) {
                    goto exit;
                }
                
                goto exit;
            }
            else {
                m_fSSLTunnelThruSecondProxy = TRUE;
            }            
        }

        //
        // Handle an OPTIONS request
        //

        if (m_strCurrMethod == gc_MethodOptions) {
            DWORD dwMaxForwards = atoi(headers.strMaxForwards);
            if ((headers.strMaxForwards == "") || (0 == dwMaxForwards)) {
                dwRetVal = SendMessage(m_sockClient, "200", gc_Reason200, NULL, 0);
                goto exit;
            }
            else if (headers.strMaxForwards != "") {
                _itoa(dwMaxForwards-1, headers.strMaxForwards.get_buffer(), 10);
            }            
        }

        //
        // Check if server socket has been opened yet.  If not, open it.  If it has been
        // opened then verify that we are connected to the correct server for this request.
        // If a secondary proxy has been specified, connect to this proxy rather than the
        // origin server.
        //

        if (g_pSettings->strSecondProxyHost != "") {
            if ((! m_sockServer.valid()) || (m_strServerName != g_pSettings->strSecondProxyHost)) {
                dwRetVal = ConnectServer(g_pSettings->strSecondProxyHost, g_pSettings->iSecondProxyPort);
                if (ERROR_SUCCESS != dwRetVal) {
                    ClearRecvBuf(buffer);
                    dwRetVal = SendMessage(m_sockClient, "502", "Proxy Error (Invalid data)", g_pErrors->szURLNotFound, g_pErrors->cchURLNotFound);
                    goto exit;
                }
            }
        }
        else if ((! m_sockServer.valid()) || (m_strServerName != headers.strHost)) {
            dwRetVal = ConnectServer(headers.strHost);
            if (ERROR_SUCCESS != dwRetVal) {
                ClearRecvBuf(buffer);
                dwRetVal = SendMessage(m_sockClient, "502", "Proxy Error (Invalid data)", g_pErrors->szURLNotFound, g_pErrors->cchURLNotFound);
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
            memcpy(pContent, &pBuffer[cchHeaders], cbContent);
        }

        //
        // Update the request headers and copy them along with the content into the final
        // buffer to be sent to the server
        //
        
        headers.UpdateRequest((g_pSettings->strSecondProxyHost != ""));
        cbNewHeaders = headers.GetBufferSize();
        *pcchBuffer += (cbNewHeaders - cchHeaders);
        *pcchSent = fAdditionalContent ? (cbNewHeaders + cbTotalContent) : *pcchBuffer;

        pBuffer = buffer.GetBuffer(*pcchBuffer);  // add one for a null terminating char
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
        
    //
    // Send the buffer along to the server
    //

    dwRetVal = SendData(m_sockServer, (char *)pBuffer, *pcchSent);
    
exit:
    return dwRetVal;
}

DWORD CHttpSession::HandleServerMessage(CBuffer& buffer, int* pcchBuffer, int cchHeaders, int* pcchSent, BOOL* pfCloseConnection)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    CHttpHeaders headers;

    ASSERT(pcchBuffer);
    ASSERT(pfCloseConnection);
    ASSERT(pcchSent);

    *pcchSent = 0;
    
    PBYTE pBuffer = buffer.GetBuffer(0, FALSE);
    ASSERT(pBuffer);

    if (m_fSSLTunnel) {
        //
        // If the current connection is just tunnelling SSL data then we do not parse the
        // data, simply send it on to the server.
        //
        *pcchSent = *pcchBuffer;
    }
    else if (0 != m_cbResponseRemain) {
        //
        // If we have already parsed the headers and just need to read remaining content
        // continuing over on a new packet, then just keep reading.
        //

        if (m_cbResponseRemain >= 0) {
            m_cbResponseRemain -= *pcchBuffer;
        }
        else {
            *pfCloseConnection = TRUE;
        }
        
        IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Response data remaining to be read.  Now there are %d bytes left.\n"), m_cbResponseRemain));

        *pcchSent = *pcchBuffer;
    }
    else if (*pfCloseConnection) {
        //
        // If pfCloseConnection is true then the proxy should assume the data does not have any
        // headers and is just content that has to be read and forwarded to the server.  When
        // the data is completely read, the server will close the connection.
        //
        
        IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Reading response data until the connection is closed.\n")));
        *pcchSent = *pcchBuffer;
    }
    else {
        //
        // Time to parse headers...
        //

        BOOL fAdditionalContent = FALSE;
        
        PBYTE pContent = NULL;
        CBuffer buffContent;
        int cbNewHeaders = 0;   // size of the headers after modification
        int cbContent = 0;      // size of the content in this packet
        int cbTotalContent = 0;    // total amount of content that has to be recved
        
        dwRetVal = g_pParser->ParseResponse(pBuffer, *pcchBuffer, headers);
        if (ERROR_SUCCESS != dwRetVal) {
            // If hit this assert then we have encountered an HTTP request that we could not parse
            ASSERT(0);
            goto exit;
        }

        ASSERT(headers.strMethod == "");
        ASSERT(headers.strURL == "");

        // The following must be in response headers
        if ((headers.strVersion == "") || (headers.strStatusCode == "")) {
            dwRetVal = ERROR_INVALID_DATA;
            goto exit;
        }

        IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Response Headers:\n")));
        IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: -- Version: %hs\n"), (LPCSTR)headers.strVersion));
        IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: -- Status: %hs\n"), (LPCSTR)headers.strStatusCode));

        if (headers.strTransferEncoding == gc_TEChunked) {
            m_fChunked = TRUE;
        }

        m_fAuthInProgress = (headers.strStatusCode == "401");

        if ((*pcchBuffer)-cchHeaders > *pcchBuffer) {   //no underflow
            dwRetVal = ERROR_INVALID_DATA;
            goto exit;
        }
        cbContent = *pcchBuffer - cchHeaders;

        if (m_fSSLTunnelThruSecondProxy) {
            // We need to set up tunnel after response from secondary proxy
            if (headers.strStatusCode == "200") {
                m_fSSLTunnel = TRUE;
            }
            else {
                *pfCloseConnection = TRUE;
            }
        }

        //
        // Determine how much response data needs to still be read in this session.
        //
        
        if (m_fChunked) {
            m_cbResponseRemain = 0;
            cbTotalContent = 0;
            fAdditionalContent = TRUE;            
        }
        else if (m_fSSLTunnel) {
            m_cbResponseRemain = 0;
        }
        else if (headers.strContentLength == "" &&
            (headers.strStatusCode != "204") && 
            (headers.strStatusCode != "304") && 
            (strncmp(headers.strStatusCode, "1", 1))) {
            
            IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Content-Length was not specified.  Reading response data until server closes connection in session %d.\n"), m_dwSessionId));
            cbTotalContent = cbContent;
            m_cbResponseRemain = -1;    // Signals object that there is an unknown amount of data left to read
            *pfCloseConnection = TRUE;
        }        
        else {
            cbTotalContent = atoi(headers.strContentLength);
            m_cbResponseRemain = cbTotalContent - cbContent;
            if (m_cbResponseRemain < 0) {
                m_cbResponseRemain = 0;
                fAdditionalContent = TRUE;
            }
        }

        //
        // Server signalled proxy to close connection
        //
        
        if (headers.strConnection == gc_ConnClose) {
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
            memcpy(pContent, &pBuffer[cchHeaders], cbContent);
        }

        //
        // Update the response headers
        //
        
        headers.UpdateResponse();
        cbNewHeaders = headers.GetBufferSize();
        *pcchBuffer += (cbNewHeaders - cchHeaders);
        *pcchSent = fAdditionalContent ? (cbNewHeaders + cbTotalContent) : *pcchBuffer;
    
        pBuffer = buffer.GetBuffer(*pcchBuffer);  // add one for a null terminating char
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

    //
    // Send the data along to the client
    //

    dwRetVal = SendData(m_sockClient, (char *)pBuffer, *pcchSent);

exit:
    if (ERROR_SUCCESS != dwRetVal) {
        SendMessage(m_sockClient, "502", gc_Reason502, g_pErrors->szMiscError, g_pErrors->cchMiscError);
    }
    
    return dwRetVal;
}

void CHttpSession::ClearRecvBuf(CBuffer& buffer)
{
    DWORD dwRetVal;
    PBYTE pBuffer = buffer.GetBuffer(0, FALSE);
    
    while (m_cbRequestRemain > 0) {
        int cbRecved = 0;
        SOCKET sock = m_sockClient;
        SessionUnlock();
        dwRetVal = NonBlockingRecv(sock, pBuffer, buffer.GetSize(), &cbRecved);
        SessionLock();
        if ((ERROR_SUCCESS != dwRetVal) || (0 == cbRecved)) {
            break;
        }
        m_cbRequestRemain -= cbRecved;
    }

    m_cbRequestRemain = 0;
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
        response.strReason = gc_Reason200Conn;
        response.UpdateResponse();
        
        dwRetVal = SendCustomPacket(m_sockClient, response);
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

DWORD CHttpSession::ConnectServer(const char* szHost, int iPort)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    CHAR szPort[16];
    stringi strNewHost;
    ADDRINFO *pAI = NULL;

    IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Trying to connect to host %S on port %d in session %d.\n"), szHost, iPort, m_dwSessionId));

    sprintf(szPort, "%d", iPort);
    dwRetVal = getaddrinfo(szHost, szPort, NULL, &pAI);
    if (ERROR_SUCCESS != dwRetVal) {
        goto exit;        
    }

    m_sockServer = socket(pAI->ai_family, SOCK_STREAM, 0);
    if (! m_sockServer.valid()) {
        dwRetVal = WSAGetLastError();
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Error creating socket %d.\n"), dwRetVal));
        goto exit;
    }

    // Connect to host
    if (SOCKET_ERROR == connect(m_sockServer, pAI->ai_addr, pAI->ai_addrlen)) {
        dwRetVal = WSAGetLastError();
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Connect call failed with error %d.\n"), dwRetVal));
        m_sockServer.close();
        goto exit;
    }

    m_strServerName = szHost;
    
    IFDBG(DebugOut(ZONE_CONNECT, _T("WebProxy: Connected to host %S on port %d in session %d.\n"), szHost, ((iPort == -1) ? DEFAULT_HTTP_PORT : iPort), m_dwSessionId));

exit:
    if (pAI) {
        freeaddrinfo(pAI);
    }
    
    return dwRetVal;
}

DWORD CHttpSession::HandleAuthentication(CHttpHeaders& headers, CBuffer& buffer, BOOL* pfCloseConnection)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    DWORD dwAuthType = 0;
    CHttpHeaders response;
    BOOL fAuthSuccess = FALSE;
    int cbUser = MAX_USER_NAME;

    while (1) {
        // Get authorization data
        g_pParser->ParseAuthorization(headers, &dwAuthType);

        if ((AUTH_TYPE_UNKNOWN == dwAuthType) || (headers.strProxyAuthorization == "")) {
            //
            // Unknown Authentication type, resend Authenticate headers
            //
            
            *pfCloseConnection = TRUE;
            response.strProxyConnection = gc_ConnClose;

            if (g_pSettings->fNTLMAuth) {
                response.strProxyAuthenticate = gc_AuthNTLM;
            }
            if (g_pSettings->fBasicAuth) {
                response.strProxyAuthenticate2 = gc_AuthBasic;
            }
        }
        else if (AUTH_TYPE_NTLM == dwAuthType) {        
            //
            // NTLM Authentication
            //
            
            string strNTLMAuth;
            int cbNTLMAuthOut;
            
            GetMaxNTLMBuffBase64Size((DWORD *)&cbNTLMAuthOut);
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
                response.strConnection = gc_ConnKeepAlive;
                response.strProxyConnection = gc_ConnKeepAlive;
                response.strProxyAuthenticate = gc_AuthNTLM;
                response.strProxyAuthenticate += " ";
                response.strProxyAuthenticate += strNTLMAuth;
            }
            else {
                *pfCloseConnection = TRUE;
                response.strProxyConnection = gc_ConnClose;
                
                if (g_pSettings->fNTLMAuth) {
                    response.strProxyAuthenticate = gc_AuthNTLM;
                }
                if (g_pSettings->fBasicAuth) {
                    response.strProxyAuthenticate2 = gc_AuthBasic;
                }
            }
        }
        else {
            //
            // Basic authentication
            //

            m_strUser.reserve(cbUser);
            
            dwRetVal = HandleBasicAuth(headers.strProxyAuthorization, &fAuthSuccess, m_strUser.get_buffer(), cbUser);
            if (ERROR_SUCCESS != dwRetVal) {
                goto exit;
            }

            if ((! fAuthSuccess) || (m_strUser == "")) {
                *pfCloseConnection = TRUE;
                response.strProxyConnection = gc_ConnClose;
                if (g_pSettings->fNTLMAuth) {
                    response.strProxyAuthenticate = gc_AuthNTLM;
                }
                if (g_pSettings->fBasicAuth) {
                    response.strProxyAuthenticate2 = gc_AuthBasic;
                }
            }
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
        response.strReason = gc_Reason407;

        char szBuf[5];
        response.strContentLength = _itoa(g_pErrors->cchProxyAuthFailed, szBuf, 10);

        response.UpdateResponse();

        // Send headers
        dwRetVal = SendCustomPacket(m_sockClient, response);
        if (ERROR_SUCCESS != dwRetVal) {
            goto exit;
        }

        // Send "auth failed" content message
        dwRetVal = SendData(m_sockClient, g_pErrors->szProxyAuthFailed, g_pErrors->cchProxyAuthFailed);
        if (ERROR_SUCCESS != dwRetVal) {
            goto exit;
        }

        IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Sent authentication response with status 407, to client in session %d.\n"), m_dwSessionId));
    }

exit:
    return dwRetVal;
}

DWORD CHttpSession::HandleProxyPacRequest(void)
{
    CHAR szProxyPac[1024];
    int cchProxyPac;
    
    IFDBG(DebugOut(ZONE_REQUEST, _T("WebProxy: Sending proxy.pac file to client.\n")));

    g_pSettings->Lock();

    ULONG ulIpMask = inet_addr(g_pSettings->strPrivateMaskV4);
    ULONG ulIpAddr = inet_addr(g_pSettings->strPrivateAddrV4);
    
    sockaddr_in dest;
    dest.sin_addr.S_un.S_addr = (ulIpAddr  & ulIpMask);
    string strDest = inet_ntoa(dest.sin_addr);

    sprintf(szProxyPac, gc_szProxyPac, (LPCSTR)strDest, (LPCSTR)g_pSettings->strPrivateMaskV4, (LPCSTR)g_pSettings->strPrivateAddrV4, g_pSettings->iPort);
    cchProxyPac = strlen(szProxyPac);

    g_pSettings->Unlock();

    return SendMessage(m_sockClient, "200", gc_Reason200, szProxyPac, cchProxyPac);
}

DWORD CHttpSession::ProxyRecv(SOCKET sock, CBuffer& buffer, int* pcbTotalRecved, int cbRemain, int* pcbHeaders)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    int cbHeaders = 0;
    
    PBYTE pBuffer = buffer.GetBuffer(0, FALSE);
    ASSERT(pBuffer);
    
    ASSERT(pcbTotalRecved);
        
    //
    // If all headers have not yet been read, we have to read them.  Otherwise, this is just reading
    // another chunk of data.
    //

    while (0 == cbHeaders) {
        int cbBuffer;
        int cbRecved = 0;
        int cbRecvBufferIn;
        PBYTE pBuff;

        // If we have recved some data and are still in this loop then we have to recv more.  Double the buffer
        // size and proceed to recv more data.
        cbBuffer = *pcbTotalRecved;
        if (cbBuffer >= DEFAULT_BUFFER_SIZE - 1) {
            cbBuffer *= 2;
            if (cbBuffer > g_pSettings->iMaxBufferSize) {
                dwRetVal = ERROR_INVALID_DATA;
                goto exit;
            }
        }
        else {
            cbBuffer = DEFAULT_BUFFER_SIZE - 1;
        }

        pBuffer = buffer.GetBuffer(cbBuffer + 1, *pcbTotalRecved ? TRUE : FALSE);    // add 1 to append '\0'
        if(! pBuffer) {
            dwRetVal = ERROR_OUTOFMEMORY;
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Insufficient memory to allocate recv buffer.\n")));
            goto exit;
        }

        pBuff = pBuffer;

        cbRecvBufferIn = buffer.GetSize() - *pcbTotalRecved - 1;
        if ((cbRemain > 0) && (cbRecvBufferIn > cbRemain)) {
            //
            // If we have read all the headers and size of recv buffer is larger than what is left
            // that has to be read, then just read the amount that has to be read for this response.
            // Doing this ensures the start of a response will always be at the start of a packet.
            //
            cbRecvBufferIn = cbRemain;
        }

        SessionUnlock();        
        dwRetVal = NonBlockingRecv(sock, &pBuff[*pcbTotalRecved], cbRecvBufferIn, &cbRecved);
        SessionLock();
        if (ERROR_SUCCESS != dwRetVal) {
            goto exit;
        }

        IFDBG(DebugOut(ZONE_PACKETS, _T("Received the following packet:\n")));
        IFDBG(DumpBuff(pBuff, cbRecved));

        *pcbTotalRecved += cbRecved;
        pBuff[*pcbTotalRecved] = '\0';

        // If we have already parsed the headers or we are just tunneling SSL data, 
        // then break from this loop now.
        if ((0 != cbRemain) || m_fSSLTunnel) {
            break;
        }

        // Both IE and Netscape append CRLF to POST but do not count it as part of the content-length.  It is possible
        // for these two bytes to be sent in their own packet and get unrecognized until this point.
        if ((m_strCurrMethod == gc_MethodPost) && (cbRecved == 2) && (pBuff[0] == '\r') && (pBuff[1] == '\n')) {
            m_cbRequestRemain = 2; // Reset this to two bytes left
            break;
        }

        // This call will return 0 if all the headers have not been recved
        cbHeaders = GetHeadersLength(pBuff);
    }

exit:
    if (pcbHeaders) {
        *pcbHeaders = cbHeaders;
    }
    
    return dwRetVal;
}


DWORD CHttpSession::ReadChunkSize(SOCKET recvSock, CBuffer& buffer, int* pcbRecved, DWORD* pdwChunkSize, DWORD* pdwChunkHeaderSize)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    DWORD dwReadAttempts = 0;

    ASSERT(pcbRecved);    
    ASSERT(pdwChunkSize);
    ASSERT(pdwChunkHeaderSize);

    *pdwChunkSize = 0;
    *pdwChunkHeaderSize = 0;

    //
    // Get the next chunk size.  The format of this string is as follows:
    // "chunk-size [chunk-extension]\r\n"
    // We want to return chunk_size and the total length of this string.
    //

    while (dwReadAttempts < MAX_CHUNKED_READ_ATTEMPTS) {
        CHAR* pszStopString = NULL;
        CHAR* pszEndChunk = NULL;
        PBYTE pBuffer = buffer.GetBuffer(0, FALSE);
        ASSERT(pBuffer);

        // Read to end of chunk-extension
        pszEndChunk = strstr((char*)pBuffer, "\r\n");
        if (NULL == pszEndChunk) {
            // Need to read more data to read past chunk-extension
            dwRetVal = ProxyRecv(recvSock, buffer, pcbRecved, -1, NULL);
            if (ERROR_SUCCESS != dwRetVal) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Could not recv data for rest of chunk size.\n")));
                goto exit;
            }

            dwReadAttempts++;
            continue;
        }

        // Read chunk-size
        *pdwChunkSize = strtol((char*)pBuffer, &pszStopString, 16);
        if ((0 == *pdwChunkSize) && (pszStopString == (char*)pBuffer)) {
            dwRetVal = ERROR_INVALID_DATA;
            goto exit;
        }

        // Calculate total chunk header size (Add two for \r\n)
        *pdwChunkHeaderSize = (pszEndChunk - (char*)pBuffer) + 2;
        break;        
    }
    
    if (0 == *pdwChunkHeaderSize) {
        dwRetVal = ERROR_INVALID_DATA;
    }

exit:
    return dwRetVal;
}

DWORD CHttpSession::HandleChunked(CBuffer& buffer, int* pcchBuffer, int* pcchSent, BOOL fRequest)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    DWORD dwChunkSize = 0;          // Size of chunk
    DWORD dwChunkHeaderSize = 0;    // Size of chunk header
    DWORD dwReadAttempts = 0;       // Number of attempts to read chunk header before giving up
    int cbChunkRemain = 0;          // Amount of data left to read in chunk
    int cbSend = 0;                 // Size of buffer to send to Client
    int cchBuffer;                  // Total Amount of data in buffer
    SOCKET recvSock, sendSock;

    recvSock = fRequest ? m_sockClient : m_sockServer;
    sendSock = fRequest ? m_sockServer : m_sockClient;

    ASSERT(pcchBuffer);
    ASSERT(pcchSent);
    
    PBYTE pBuffer = buffer.GetBuffer(0, FALSE);
    ASSERT(pBuffer);

    cchBuffer = *pcchBuffer;

    while (m_fChunked) {
        if (0 == cchBuffer) {
            // Read some more data if the buffer is empty
            dwRetVal = ProxyRecv(recvSock, buffer, &cchBuffer, -1, NULL);
            if (ERROR_SUCCESS != dwRetVal) {
                goto exit;
            } 
        }
        
        if (cbChunkRemain != 0) {
            // We still have data left in the chunk to forward to client
            cbSend = (cbChunkRemain < cchBuffer) ? cbChunkRemain : cchBuffer;
            cbChunkRemain -= cbSend;
        }
        else {
            // We are at the start of the next chunk
            
            DWORD dwTotalChunkSize;
            int cbDeltaRecv = cchBuffer;

            dwRetVal = ReadChunkSize(recvSock, buffer, &cchBuffer, &dwChunkSize, &dwChunkHeaderSize);
            if (ERROR_SUCCESS != dwRetVal) {
                IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Invalid chunk syntax.\n")));
                goto exit;
            }

            pBuffer = buffer.GetBuffer(0, FALSE);
            ASSERT(pBuffer);

            *pcchBuffer += (cchBuffer - cbDeltaRecv);
            dwTotalChunkSize = dwChunkSize + dwChunkHeaderSize + 2;

            IFDBG(DebugOut(ZONE_SESSION, _T("WebProxy: Received chunk of size %d.  Forwarding to %s.\n"), dwChunkSize, (fRequest?L"server":L"client")));

            if (0 == dwChunkSize) {
                m_fChunked = FALSE; // We are done
                
                do {
                    cbSend = GetHeadersLength(pBuffer);
                    if (0 == cbSend) {
                        // Need to recv more data
                        dwRetVal = ProxyRecv(recvSock, buffer, &cchBuffer, -1, NULL);
                        if (ERROR_SUCCESS != dwRetVal) {
                            goto exit;
                        }
                        pBuffer = buffer.GetBuffer(0, FALSE);
                        ASSERT(pBuffer);                        
                    }
                } while (0 == cbSend);
            }
            else if (cchBuffer < dwTotalChunkSize) {
                cbChunkRemain = dwTotalChunkSize - cchBuffer;
                cbSend = cchBuffer;
            }
            else {
                cbSend = dwTotalChunkSize;
            }
        }
        
        dwRetVal = SendData(sendSock, (char *)pBuffer, cbSend);
        if (ERROR_SUCCESS != dwRetVal) {
            goto exit;
        }

        *pcchSent += cbSend;

        // If we did not send all data in the buffer, move the rest of the
        // data to the front of the buffer.
        if (cchBuffer > cbSend) {
            memmove(pBuffer, pBuffer + cbSend, cchBuffer - cbSend);
            pBuffer[cchBuffer-cbSend] = '\0';
        }

        cchBuffer -= cbSend;
    }

exit:    
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
    if (g_pProxyFilter->GetFilterCount() == 0) {
        *pfContinue = TRUE;
        return ERROR_SUCCESS;
    }
    
    *pfContinue = FALSE;
    *pfCloseConnection = FALSE;

    stringi strCanonicalizedURL;
    strCanonicalizedURL.reserve(headers.strURL.capacity()); // Canonicalized string is no bigger than original
    svsutil_HttpCanonicalizeUrlA(headers.strURL, strCanonicalizedURL.get_buffer());    

    while(1) {
        SOCKADDR_IN saPrivate;
        saPrivate.sin_family = AF_INET;
        g_pSettings->Lock();
        saPrivate.sin_addr.S_un.S_addr = inet_addr(g_pSettings->strPrivateAddrV4);
        g_pSettings->Unlock();
        
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
        request.psaProxy = (SOCKADDR_STORAGE*)&saPrivate;
        request.cbsaProxy = sizeof(saPrivate);

        pszNewURL[0] = '\0';
        
        dwRetVal = g_pProxyFilter->FilterRequest(&request);
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
        else if (ERROR_NOT_AUTHENTICATED == dwRetVal) {
            //
            // Filter requires user to be authenticated
            //
            goto exit;
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
                dwRetVal = SendMessage(m_sockClient, "403", gc_Reason403, NULL, 0);
                goto exit;
            }
            else {
                ClearRecvBuf(buffer);
                dwRetVal = SendDeniedRequest(m_sockClient, request.szURLOut);
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



//
// Miscellaneous functions used by session objects
//

DWORD GetHeadersLength(const PBYTE pBuffer)
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

DWORD NonBlockingRecv(SOCKET sock, PBYTE pBuffer, int cbBuffer, int* piBytesRecved)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    FD_SET sockSet;

    ASSERT(piBytesRecved);
    ASSERT(INVALID_SOCKET != sock);

    timeval timeout = {0};
    timeout.tv_sec = 10;

    FD_ZERO(&sockSet);
    FD_SET(sock, &sockSet);

    int iSockets = select(0,&sockSet,NULL,NULL,&timeout);

    if ((SOCKET_ERROR == iSockets) || (0 == iSockets)) {
        dwRetVal = ERROR_TIMEOUT;
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: NonBlockingRecv timed out trying to recv data\n")));
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

DWORD SendDeniedRequest(SOCKET sock, const char* pszURL)
{
    CHttpHeaders response;

    IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: HTTP request has been denied.  A new URL is being substituted.\n")));

    response.strStatusCode = "302";
    response.strReason = gc_Reason302;
    response.strContentLength = "0";
    response.strContentType = "text/html";
    response.strLocation = pszURL;

    response.UpdateResponse();

    // Send "Found" message with new URL
    return SendCustomPacket(sock, response);
}

DWORD SendMessage(SOCKET sock, const char* szCode, const char* szReason, const char* szContent, int cchContent)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    CHttpHeaders response;

    IFDBG(DebugOut(ZONE_RESPONSE, _T("WebProxy: Sending response with code %hs.\n"), szCode));

    response.strStatusCode = szCode;
    response.strReason = szReason;

    char szBuf[5];
    response.strContentLength = _itoa(cchContent, szBuf, 10);

    response.UpdateResponse();

    dwRetVal = SendCustomPacket(sock, response);
    if (cchContent && (ERROR_SUCCESS == dwRetVal)) {
        dwRetVal = SendData(sock, szContent, cchContent);
    }

    return dwRetVal;
}

DWORD SendCustomPacket(SOCKET sock, CHttpHeaders& headers)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    CBuffer buffer;
    PBYTE pBuffer;
    
    int cbSize = headers.GetBufferSize();
    
    pBuffer = buffer.GetBuffer(cbSize + 1); // Add one for terminating null char
    if (! pBuffer) {
        dwRetVal = ERROR_OUTOFMEMORY;
        goto exit;
    }

    headers.GetResponseBuffer(pBuffer);

    dwRetVal = SendData(sock, (char *)pBuffer, cbSize);
#ifdef DEBUG
    if (ERROR_SUCCESS != dwRetVal) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Send returned error on sock %d (error:%d) with %d bytes.\n"), sock, dwRetVal, cbSize));
    }
#endif //DEBUG

exit:
    return dwRetVal;
}

DWORD SendData(SOCKET sock, const char* szData, int cbData)
{
    int cbSent = send(sock, szData, cbData, 0);
    if (SOCKET_ERROR == cbSent) {
        IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Send returned error %d with %d bytes.\n"), WSAGetLastError(), cbSent));
        return WSAGetLastError();
    }
    else {
#ifdef DEBUG
        if (cbSent != cbData) {
            IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Tried to send %d bytes but actually sent %d bytes.\n"), cbData, cbSent));
            ASSERT(0);
        }
#endif // DEBUG

        return ERROR_SUCCESS;
    }
}

