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
#include "BackChannel.h"
#include "Convert.h"

CBackChannel::CBackChannel()
{
    m_ulRemoteAddr = 0;
    m_ulPort = 0;
    m_bServerAndPortSet = FALSE;
    m_hSocket = INVALID_SOCKET;
}

CBackChannel::~CBackChannel()
{
    if (m_hSocket)
    {
        closesocket(m_hSocket);
    }
}

HRESULT CBackChannel::Init()
{
    int nCode;
    nCode = WSAStartup(MAKEWORD(1, 1), &m_wsaData);
    return nCode;
}

HRESULT CBackChannel::Destroy()
{
    WSACleanup();
    return S_OK;
}

HRESULT CBackChannel::SetServerAndPort(const TCHAR *szServAddr, ULONG ulPort)
{
   CHAR *szAnsiServAddr = NULL;
   
   CopyString(const_cast<LPTSTR>(szServAddr), static_cast<LPSTR *>(&szAnsiServAddr));

    ULONG ulRemoteAddr = inet_addr(szAnsiServAddr);
    delete szAnsiServAddr;

    if (ulRemoteAddr == INADDR_NONE)
        return S_FALSE;

    m_ulRemoteAddr = ulRemoteAddr;
    m_ulPort = ulPort;
    m_bServerAndPortSet = TRUE;
    return S_OK;
}

HRESULT CBackChannel::SendCommand(teNDPPacketType packetType, PVOID packetData, ULONG cbPacketData)
{
    DWORD dwErr = 0;

    if(m_hSocket == INVALID_SOCKET)
        return S_FALSE;

    //first send the packet type
    do {
        dwErr = send(m_hSocket, (const char *) &packetType, sizeof(teNDPPacketType), 0);
        if (dwErr == SOCKET_ERROR)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
            {
                return S_FALSE;
            }
            else
            {
                if (IsCanceled())
                    return S_FALSE;
                continue;
            }
        }

        break;
    } while (1);
    
    //then send the packet data, if any exists
    if (packetData)
        {
            do {
                dwErr = send(m_hSocket, (const char *) packetData, cbPacketData, 0);
                if (dwErr == SOCKET_ERROR)
                {
                    if (WSAGetLastError() != WSAEWOULDBLOCK)
                    {
                    _tprintf(_T("\nsend failed: %d"), WSAGetLastError());
                    return S_FALSE;
                    }
                    else
                    {
                        if (IsCanceled())
                            return S_FALSE;
                        continue;
                    }
                }

                break;
            } while (1);
        }

    return S_OK;
}

HRESULT CBackChannel::ReceiveCommand(teNDPPacketType *pPacketType)
{
    int nReadBytes;

    //receive packet type
    do {
        nReadBytes = recv(m_hSocket, (char *) pPacketType, sizeof(teNDPPacketType), 0);
        if (nReadBytes == SOCKET_ERROR)
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

    if (nReadBytes != sizeof(teNDPPacketType))
    {
        _tprintf(_T("\nrecv failed: %d"), WSAGetLastError());
        return S_FALSE;
    }

    return S_OK;
}

HRESULT CBackChannel::ReceiveParams(PVOID pPacketData, ULONG cbPacketData)
{
    int nReadBytes;

    do {
        nReadBytes = recv(m_hSocket, (char *) pPacketData, cbPacketData, 0);
        if (nReadBytes == SOCKET_ERROR)
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
    
    if (nReadBytes != (INT) cbPacketData)
    {
        _tprintf(_T("\nrecv params failed: %d"), WSAGetLastError());
        return S_FALSE;            
    }

    return S_OK;
}
VOID CBackChannel::SetFinishEvent(HANDLE hFinishEvent)
{
    m_hFinishEvent = hFinishEvent;
}

BOOL CBackChannel::IsCanceled()
{
    if (m_hFinishEvent == NULL) return FALSE;

    if (WAIT_OBJECT_0 == WaitForSingleObject(m_hFinishEvent,5))
    { 
        //Somebody wants us to leave.
        return TRUE;
    }

    return FALSE;
}
