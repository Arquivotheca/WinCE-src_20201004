//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
