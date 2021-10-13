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
#ifndef __BACKCHANNEL_H
#define __BACKCHANNEL_H
#include <tchar.h>
#include <winsock.h>
#include "NDP_protocol.h"


class CBackChannel
{
public:
    CBackChannel();
    virtual ~CBackChannel();

    HRESULT Init();

    HRESULT Destroy();

    HRESULT SetServerAndPort(const TCHAR *szServAddr, ULONG ulPort);
    HRESULT SendCommand(teNDPPacketType packetType, PVOID packetData, ULONG cbPacketData);
    virtual HRESULT ReceiveCommand(teNDPPacketType*pPacketType);
    HRESULT ReceiveParams(PVOID packetData, ULONG cbPacketData);
    VOID SetFinishEvent(HANDLE hFinishEvent);
    virtual HRESULT Setup() = 0;
    virtual HRESULT Accept() = 0;

private:
    WSADATA m_wsaData;

protected:
    ULONG m_ulRemoteAddr;
    ULONG m_ulPort;
    BOOL m_bServerAndPortSet;
    SOCKET m_hSocket;
    HANDLE m_hFinishEvent;
    BOOL IsCanceled();
};
#endif
