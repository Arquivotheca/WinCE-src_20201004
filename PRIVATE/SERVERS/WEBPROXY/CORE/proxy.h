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


class CHttpProxy {
public:
    CHttpProxy(void);

    DWORD Init(void);
    DWORD Deinit(void);
    DWORD Start(void);
    DWORD Stop(void);
    DWORD NotifyAddrChange(PBYTE pBufIn, DWORD cbBufIn);
    void Run(void); // called only by static method ServerThread
    DWORD SignalFilter(DWORD dwSignal);

protected:
    DWORD InitSettings(void);
    DWORD DeinitSettings(void);
    static DWORD WINAPI ServerThread(LPVOID pv);
    DWORD SetupServerSockets(void);
    void UpdateAdapterInfo(void);
    void Lock() { m_csLock.lock(); }
    void Unlock() { m_csLock.unlock(); }
    
    // Members
    ce::critical_section m_csLock;
    BOOL m_fRunning;
    ce::auto_socket m_sockServer[MAX_SOCKET_LIST];
    ce::auto_handle m_hServerThread;
    DWORD m_cSockets;
};

#endif // __PROXY_H__

