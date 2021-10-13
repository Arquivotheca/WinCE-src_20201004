//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __SESSION_H__
#define __SESSION_H__

#include "global.h"
#include "svsutil.hxx"
#include "sync.hxx"
#include "auto_xxx.hxx"
#include "list.hxx"
#include "parser.h"
#include "utils.h"
#include "auth.h"

typedef ce::auto_xxx<SOCKET, int (__stdcall *)(SOCKET), closesocket, INVALID_SOCKET, ce::ref_counting>        auto_socket;

class CHttpSession;
typedef ce::list<CHttpSession, ce::free_list<FREE_LIST_SIZE> >    list;

class CSessionMgr;

extern CSessionMgr* g_pSessionMgr;


typedef struct _SessionSettings {
    auto_socket            sockClient;
    SOCKADDR_STORAGE       saClient;
} SessionSettings;


DWORD SendCustomPacket(SOCKET sock, CHttpHeaders& headers);
DWORD SendDeniedRequest(SOCKET sock, const char* pszURL);
DWORD SendMessage(SOCKET sock, const char* szCode, const char* szReason, const char* szContent, int cchContent);
DWORD SendData(SOCKET sock, const char* szData, int cbData);
DWORD GetHeadersLength(const PBYTE pBuffer);
DWORD GetHostAddr(const char* szHost, int iPort, sockaddr_in* psa);
void GetHostAndPort(const char* szSource, char* szHost, int* piPort);
DWORD NonBlockingRecv(SOCKET sock, PBYTE pBuffer, int cbBuffer, int* piBytesRecved);


class CHttpSession {
public:
    CHttpSession();
    virtual ~CHttpSession();

    DWORD GetId(void);
    void SetId(DWORD dwSessionId);
    DWORD Start(SessionSettings* pSettings);
    DWORD Shutdown(void);
    void Run(void);
    static DWORD WINAPI SessionThread(LPVOID pv);
    
protected:
    DWORD ProcessMessage(CBuffer& buffer, BOOL* pfCloseConnection, BOOL fRequest);
    DWORD HandleClientMessage(CBuffer& buffer, int* cchBuffer, int cchHeaders, int* pcchSent, BOOL* pfCloseConnection);
    DWORD HandleServerMessage(CBuffer& buffer, int* pcchBuffer, int cchHeaders, int* pcchSent, BOOL* pfCloseConnection);
    DWORD ProxyRecv(SOCKET sock, CBuffer& buffer, int* pcbTotalRecved, int cbRemain, int* pcbHeaders);
    void ClearRecvBuf(CBuffer& buffer);
    DWORD HandleConnectRequest(stringi& strHost, stringi& strPort);
    DWORD ConnectServer(const char* szHost, int iPort = DEFAULT_HTTP_PORT);
    DWORD HandleAuthentication(CHttpHeaders& headers, CBuffer& buffer, BOOL* pfCloseConnection);
    DWORD HandleFilterRequest(const CHttpHeaders& headers, CBuffer& buffer, BOOL* pfContinue, BOOL* pfCloseConnection);
    DWORD HandleProxyPacRequest(void);
    DWORD HandleChunked(CBuffer& buffer, int* pcchBuffer, int* pcchSent, BOOL fRequest);
    DWORD ReadChunkSize(SOCKET recvSock, CBuffer& buffer, int* pcbRecved, DWORD* pdwChunkSize, DWORD* pdwChunkHeaderSize);
    
    void SessionLock(void) { m_csSession.lock(); }
    void SessionUnlock(void) { m_csSession.unlock(); }

    DWORD m_dwSessionId;
    ce::critical_section_with_copy m_csSession;
    AUTH_NTLM m_auth;

    stringi m_strServerName;
    SOCKADDR_STORAGE m_saClient;
    
    auto_socket m_sockClient;
    auto_socket m_sockServer;

    int m_cbResponseRemain;
    int m_cbRequestRemain;

    BOOL m_fRunning;
    BOOL m_fSSLTunnel;
    BOOL m_fSSLTunnelThruSecondProxy;
    BOOL m_fAuthInProgress;
    BOOL m_fChunked;
    
    string m_strCurrMethod;
    stringi m_strUser;
};

class CSessionMgr {
public:
    CSessionMgr(void) { m_dwNextId = 0; }
    
    DWORD StartSession(SessionSettings* pSettings, DWORD* pdwSessionId);
    DWORD RemoveSession(DWORD dwSessionId);
    DWORD RemoveAllSessions(void);
    DWORD ShutdownAllSessions(void);
    int GetSessionCount(void);

private:
    void Lock(void) { m_csSessions.lock(); }
    void Unlock(void) { m_csSessions.unlock(); }

    ce::critical_section m_csSessions;
    list m_SessionList;
    DWORD m_dwNextId;
};

#endif // __SESSION_H__

