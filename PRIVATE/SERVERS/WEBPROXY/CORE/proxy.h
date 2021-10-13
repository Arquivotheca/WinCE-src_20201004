//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __PROXY_H__
#define __PROXY_H__

#include "global.h"
#include "sync.hxx"
#include "svsutil.hxx"
#include "creg.hxx"
#include "list.hxx"
#include "auto_xxx.hxx"
#include "session.h"

extern ce::string g_strPrivateAddr;
extern ce::string g_strPrivateMask;
extern ce::string g_strHostName;
extern int g_iPort;

class CHttpProxy {
public:
	CHttpProxy(void);

	DWORD Init(void);
	DWORD Deinit(void);
	DWORD Start(void);
	DWORD Stop(void);
	DWORD NotifyAddrChange(PBYTE pBufIn, DWORD cbBufIn);
	void Run(void); // called only by static method ServerThread

protected:
	DWORD InitSettings(void);
	DWORD DeinitSettings(void);
	static DWORD WINAPI ServerThread(LPVOID pv);
	DWORD SetupServerSocket(void);
	void UpdateAdapterInfo(void);
	void Lock() { m_csLock.lock(); }
	void Unlock() { m_csLock.unlock(); }
	
	// Members
	ce::critical_section m_csLock;
	int m_iMaxConnections;
	int m_iMaxHeadersSize;
	int m_iSessionTimeout;
	BOOL m_fAuth;
	BOOL m_fRunning;
	BOOL m_fFilterInitialized;
	SVSThreadPool* m_pThreadPool;
	CSessionMgr sessions;
	ce::auto_socket m_sockServer;
	ce::auto_handle m_hServerThread;
	string m_strPubIntf;
	string m_strPrivIntf;
	ce::auto_hlibrary m_hlibFilter;
	sockaddr_in m_saPublicIntf;
	sockaddr_in m_saPrivIntf;
	int cbAdapterInfo;
};

#endif // __PROXY_H__

