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
#include "SocketServer.h"
#include "Ndp_protocol.h"
#include <stdio.h>
#include <tchar.h>

CServerChannel::CServerChannel() : CBackChannel()
{
    m_hListeningSocket = NULL;
    m_bConnectionAccepted = FALSE;
    m_bServerStarted = FALSE;
    m_hFinishEvent = NULL;
}

CServerChannel::~CServerChannel()
{
    if (m_hListeningSocket)
    {
        closesocket(m_hListeningSocket);
    }
}

HRESULT CServerChannel::StartServer()
{
    IN_ADDR Address;
    sockaddr_in sinInterface;
    u_long iMode = 1;
    int iRetValue;

    if (m_bServerAndPortSet == FALSE)
        return S_FALSE;
    memcpy(&Address, &m_ulRemoteAddr, sizeof(ULONG));

    m_hListeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_hListeningSocket == INVALID_SOCKET)
        return S_FALSE;

    iRetValue = ioctlsocket(m_hListeningSocket, FIONBIO, &iMode);
    if (iRetValue == SOCKET_ERROR)
        return S_FALSE;


    sinInterface.sin_family = AF_INET;
    sinInterface.sin_addr = Address;
    sinInterface.sin_port = htons(static_cast<u_short>(m_ulPort));

    do {
        iRetValue = bind(m_hListeningSocket, (sockaddr*)&sinInterface, sizeof(sockaddr_in));
        if (iRetValue == SOCKET_ERROR)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                return S_FALSE;
            else
            {
                if (IsCanceled())
                    return S_FALSE;
                continue;
            }
        }

        break;
    } while (1);

    if ((listen(m_hListeningSocket, 5) == SOCKET_ERROR)) {
        _tprintf(_T("Listen failed: %d"), WSAGetLastError());
        return S_FALSE;
    }

    m_bServerStarted = TRUE;

    return S_OK;
}

HRESULT CServerChannel::AcceptConnection()
{
    if (m_bServerStarted == FALSE)
        return S_FALSE;

    do {
        m_hSocket = accept(m_hListeningSocket, NULL, NULL);
        if (m_hSocket == INVALID_SOCKET)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                return S_FALSE;
            else
            {
                if (IsCanceled())
                    return S_FALSE;
                continue;
            }
        }

        break;
    } while (1);

    m_bConnectionAccepted = TRUE;
    return S_OK;
}


HRESULT CServerChannel::ReceiveCommand(teNDPPacketType *pPacketType)
{
    if (m_bConnectionAccepted == FALSE)
        return S_FALSE;

    return CBackChannel::ReceiveCommand(pPacketType);
}
HRESULT CServerChannel::Setup()
{
    if (this->StartServer() != S_OK)
            return S_FALSE;

    return S_OK;
}

HRESULT CServerChannel::Accept()
{
    if (this->AcceptConnection() != S_OK)
            return S_FALSE;

    return S_OK;
}
