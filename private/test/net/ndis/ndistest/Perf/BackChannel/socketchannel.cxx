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
#include <winsock.h>
#include "SocketChannel.h"
#include <stdio.h>
#include <tchar.h>

CClientChannel::CClientChannel() : CBackChannel()
{}

CClientChannel::~CClientChannel()
{
    if (m_hSocket)
        closesocket(m_hSocket);
}

HRESULT CClientChannel::Setup()
{
     IN_ADDR Address;
     
     if (m_bServerAndPortSet == FALSE)
         return S_FALSE;

    memcpy(&Address, &m_ulRemoteAddr, sizeof(ULONG));

    // Create a stream socket
    m_hSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_hSocket != INVALID_SOCKET) {
        sockaddr_in sinRemote;
        sinRemote.sin_family = AF_INET;
        sinRemote.sin_addr = Address;
        sinRemote.sin_port = htons(static_cast<u_short>(m_ulPort));
        if (connect(m_hSocket, (sockaddr*)&sinRemote, sizeof(sockaddr_in)) ==
                SOCKET_ERROR) {
            m_hSocket = INVALID_SOCKET;
            return S_FALSE;
        }
    }

    return S_OK;
}

HRESULT CClientChannel::Accept()
{
    return S_OK;
}
