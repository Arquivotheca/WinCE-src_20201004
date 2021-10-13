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
#ifndef __DEAMON_H
#define __DEAMON_H

//------------------------------------------------------------------------------

#include "NDT_Protocol.h"

//------------------------------------------------------------------------------

class CDeamon 
{
private:
    HANDLE m_hThread;
    SOCKET m_socket;

    DWORD  m_dwLastError;
    HANDLE m_hFinishEvent;

    LPTSTR m_szComputerName;
    LPTSTR m_szComputerIP;
    USHORT m_usPort;

    SOCKADDR_IN m_saLocal;
    SOCKADDR_IN m_saRemote;
    
private:
    BOOL  OpenSocket();
    void  CloseSocket();
    BOOL  DoDataReceive();
    DWORD Thread();
    
    void DumpPacketHeader(LPTSTR szProlog, NDT_CMD_HEADER* pHeader);
    void DumpPacketBody(LPTSTR szProlog, DWORD dwSize, PVOID pvBody);
    
public:
    CDeamon(USHORT usPort);
    ~CDeamon();

    DWORD Start(HANDLE hFinishEvent);
    void  Stop();

    static DWORD WINAPI DeamonThread(PVOID pvThis);
};

//------------------------------------------------------------------------------

#endif
