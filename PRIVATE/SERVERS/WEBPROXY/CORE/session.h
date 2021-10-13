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
#include "parser.h"
#include "list.hxx"
#include "utils.h"
#include "auth.h"

typedef ce::auto_xxx<SOCKET, int (__stdcall *)(SOCKET), closesocket, INVALID_SOCKET, ce::ref_counting>        auto_socket;

class CHttpSession;
typedef ce::list<CHttpSession, ce::free_list<FREE_LIST_SIZE> >	list;

class CSessionMgr;


typedef struct _SessionSettings {
	auto_socket		sockClient;
	sockaddr_in		saClient;
	sockaddr_in		saPrivate;
	BOOL			fAuthEnabled;
	int				iSessionTimeout;
	int				iMaxHeadersSize;
	SVSThreadPool* 	pThreadPool;
	CSessionMgr*	pSessionMgr;
} SessionSettings;

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
	DWORD ProcessClientRequest(CBuffer& buffer, BOOL* pfCloseConnection);
	DWORD ProcessServerResponse(CBuffer& buffer, BOOL* pfCloseConnection);
	DWORD ConnectServer(const char* szHost, int iPort = -1);
	DWORD RecvAllHeaders(SOCKET sock, CBuffer& buffer, PBYTE* ppBuffer, int* pcbTotalRecved, int cbRemain, int* pcbHeaders);
	DWORD MyRecv(SOCKET sock, PBYTE pBuffer, int cbBuffer, int* piBytesRecved);
	void ClearRecvBuf(CBuffer& buffer);
	DWORD GetHeadersLength(const PBYTE pBuffer);
	DWORD GetHostAddr(const char* szHost, int iPort, sockaddr_in* psa);
	void GetHostAndPort(const char* szSource, char* szHost, int* piPort);
	DWORD SendCustomPacket(CHttpHeaders& headers, BOOL fRequest);
	DWORD SendData(SOCKET sock, const char* szData, int cbData);
	DWORD HandleConnectRequest(stringi& strHost, stringi& strPort);
	DWORD HandleAuthentication(CHttpHeaders& headers, CBuffer& buffer, BOOL* pfCloseConnection);
	DWORD HandleUnsupportedHeader(void);
	DWORD HandleUnsupportedProtocol(void);
	DWORD HandleUnsupportedVersion(void);
	DWORD HandleDeniedRequest(const char* pszURL);
	DWORD HandleForbiddenRequest(void);
	DWORD HandleFilterRequest(const CHttpHeaders& headers, CBuffer& buffer, BOOL* pfContinue, BOOL* pfCloseConnection);
	DWORD HandleServerNotFound(void);
	DWORD HandleProxyPacRequest(void);
	
	void SessionLock(void) { m_csSession.lock(); }
	void SessionUnlock(void) { m_csSession.unlock(); }
	static void Lock(void) { m_csGlobal.lock(); }
	static void Unlock(void) { m_csGlobal.unlock(); }

	// members
	static ce::critical_section m_csGlobal;
	static CHttpParser m_Parser;

	DWORD m_dwSessionId;
	SVSThreadPool* m_pThreadPool;
	CSessionMgr* m_pSessionMgr;
	ce::critical_section_with_copy m_csSession;
	AUTH_NTLM m_auth;

	stringi m_strServerName;
	sockaddr_in m_saClient;
	sockaddr_in m_saPrivate;
	
	auto_socket m_sockClient;
	auto_socket m_sockServer;

	int m_cbResponseRemain;
	int m_cbRequestRemain;

	BOOL m_fRunning;
	BOOL m_fAuth;
	BOOL m_fSSLTunnel;
	BOOL m_fAuthInProgress;
	int m_iSessionTimeout;
	int m_iMaxHeadersSize;	

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
	void Lock(void) { m_csLock.lock(); }
	void Unlock(void) { m_csLock.unlock(); }
	
	ce::critical_section m_csLock;
	list m_SessionList;
	DWORD m_dwNextId;
};

#endif // __SESSION_H__

