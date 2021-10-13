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
    DWORD NotifyAddrChange(void);
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

