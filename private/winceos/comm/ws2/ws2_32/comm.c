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
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

comm.c

communications related winsock2 functions


FILE HISTORY:
    OmarM     22-Sep-2000

*/


#include "winsock2p.h"
#include <mswsock.h>
#include <perfcomm.h>

int WSAAPI WSARecv(
    SOCKET s,
    LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd,
    LPDWORD lpFlags,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionROUTINE) {

    int            Err, Status;
    WsSocket    *pSock;
    WSATHREADID    WsaThreadId;

    PerfCommBegin(WINSOCK_DurRecv);

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        WsaThreadId.ThreadHandle = (HANDLE)GetCurrentThreadId();

        Status = pSock->pProvider->ProcTable.lpWSPRecv(pSock->hWSPSock, 
            lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, 
            lpOverlapped, lpCompletionROUTINE, &WsaThreadId, &Err);

        if (Status && ! Err)
            Err = WSASYSCALLFAILURE;

        DerefSocket(pSock);
    }

    if (Err) {
        Status = SOCKET_ERROR;
        SetLastError(Err);
        PerfCommEndError(WINSOCK_DurRecv, Err);
    } else {
        Status = 0;
        PerfCommEnd(WINSOCK_DurRecv);
    }

    return Status;

}    // WSARecv()


// WSARecvMsg
int WSPAPI WSARecvMsg (
    SOCKET s,
    LPWSAMSG lpMsg,
    LPDWORD lpNumberOfBytesRecvd,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {

    int            Err, Status;
    WsSocket    *pSock;
    WSATHREADID    WsaThread;

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        WsaThread.ThreadHandle = (HANDLE)GetCurrentThreadId();

        Status = pSock->pProvider->ProcTable.lpWSPIoctl(pSock->hWSPSock, 
                SIO_RECV_MSG, lpMsg, sizeof(*lpMsg), NULL, 
                0, lpNumberOfBytesRecvd, lpOverlapped, 
                lpCompletionRoutine, &WsaThread, &Err);

        if (Status && ! Err) {
            ASSERT(0);
            Err = WSASYSCALLFAILURE;
        }
        
        DerefSocketHandle(pSock->hSock);
    }

    if (Err) {
        SetLastError(Err);
        Status = SOCKET_ERROR;
    } else
        Status = 0;

    return Status;
} // WSARecvMsg()    


int WSAAPI recv(
    SOCKET s,
    char * buf,
    int len,
    int flags) {

    WSABUF    WsaBuf;
    DWORD    cRcvd;
    DWORD    dwFlags;
    int        Status;

    WsaBuf.len = len;
    WsaBuf.buf = buf;
    dwFlags = flags;

    Status = WSARecv(s, &WsaBuf, 1, &cRcvd, &dwFlags, NULL, NULL);

    if (!Status)
        Status = cRcvd;

    return Status;

}    // recv()


int WSAAPI WSARecvFrom(
    SOCKET s,
    LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd,
    LPDWORD lpFlags,
    struct sockaddr FAR * lpFrom,
    LPINT lpFromlen,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionROUTINE) {

    int            Err, Status;
    WsSocket    *pSock;
    WSATHREADID    WsaThreadId;

    PerfCommBegin(WINSOCK_DurRecvFrom);

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        if (lpFrom) {
            if (lpFromlen)
                Err = CheckSockaddr(lpFrom, *lpFromlen);
            else
                Err = WSAEFAULT;
        }

        if (! Err ) {

            WsaThreadId.ThreadHandle = (HANDLE)GetCurrentThreadId();

            Status = pSock->pProvider->ProcTable.lpWSPRecvFrom(
                pSock->hWSPSock, lpBuffers, dwBufferCount, 
                lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, 
                lpOverlapped, lpCompletionROUTINE, &WsaThreadId, &Err);

            if (Status && ! Err)
                Err = WSASYSCALLFAILURE;
        }
        DerefSocket(pSock);
    }

    if (Err) {
        Status = SOCKET_ERROR;
        SetLastError(Err);
        PerfCommEndError(WINSOCK_DurRecvFrom, Err);
    } else {
        Status = 0;
        PerfCommEnd(WINSOCK_DurRecvFrom);
    }

    return Status;

}    // WSARecvFrom()


int WSAAPI recvfrom(
    SOCKET s,
    char * buf,
    int len,
    int flags,
    struct sockaddr FAR* from,
    int FAR* fromlen) {

    WSABUF    WsaBuf;
    DWORD    cRcvd;
    DWORD    dwFlags;
    int        Status;

    WsaBuf.len = len;
    WsaBuf.buf = buf;
    dwFlags = flags;

    Status = WSARecvFrom(s, &WsaBuf, 1, &cRcvd, &dwFlags, from, fromlen, 
        NULL, NULL);

    if (!Status)
        Status = cRcvd;

    return Status;

}    // recvfrom()


int WSAAPI WSASend(
    SOCKET        s,
    LPWSABUF    lpBuffers,
    DWORD        dwBufferCount,
    LPDWORD        lpNumberOfBytesSent,
    DWORD        dwFlags,
    LPWSAOVERLAPPED    lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE    lpCompletionROUTINE) {

    int            Err, Status;
    WsSocket    *pSock;
    WSATHREADID    WsaThreadId;

    PerfCommBegin(WINSOCK_DurSend);

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        WsaThreadId.ThreadHandle = (HANDLE)GetCurrentThreadId();

        Status = pSock->pProvider->ProcTable.lpWSPSend(pSock->hWSPSock, 
            lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, 
            lpOverlapped, lpCompletionROUTINE, &WsaThreadId, &Err);

        if (Status && ! Err)
            Err = WSASYSCALLFAILURE;

        DerefSocket(pSock);
    }

    if (Err) {
        Status = SOCKET_ERROR;
        SetLastError(Err);
        PerfCommEndError(WINSOCK_DurSend, Err);
    } else {
        Status = 0;
        PerfCommEnd(WINSOCK_DurSend);
    }

    return Status;

}    // WSASend()


int WSAAPI send(
    SOCKET    s,
    const char FAR *    buf,
    int        len,
    int        flags) {

    WSABUF    WsaBuf;
    DWORD    cSent;
    int        Status;

    WsaBuf.len = len;
    WsaBuf.buf = (char *)buf;

    Status = WSASend(s, &WsaBuf, 1, &cSent, flags, NULL, NULL);

    if (!Status)
        Status = cSent;

    return Status;

}    // send()


int WSAAPI WSASendTo(
    SOCKET            s,
    LPWSABUF        lpBuffers,
    DWORD            dwBufferCount,
    LPDWORD            lpNumberOfBytesSent,
    DWORD            dwFlags,
    const struct sockaddr FAR *    lpTo,
    INT                iToLen,
    LPWSAOVERLAPPED    lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE    lpCompletionROUTINE) {

    int            Err, Status;
    WsSocket    *pSock;
    WSATHREADID    WsaThreadId;

    PerfCommBegin(WINSOCK_DurSendTo);

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        WsaThreadId.ThreadHandle = (HANDLE)GetCurrentThreadId();

        Status = pSock->pProvider->ProcTable.lpWSPSendTo(pSock->hWSPSock, 
            lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo,
            iToLen, lpOverlapped, lpCompletionROUTINE, &WsaThreadId, &Err);

        if (Status && ! Err)
            Err = WSASYSCALLFAILURE;

        DerefSocket(pSock);
    }

    if (Err) {
        Status = SOCKET_ERROR;
        SetLastError(Err);
        PerfCommEndError(WINSOCK_DurSendTo, Err);
    } else {
        Status = 0;
        PerfCommEnd(WINSOCK_DurSendTo);
    }

    return Status;

}    // WSASendTo()


int WSAAPI sendto(
    SOCKET    s,
    const char FAR *    buf,
    int        len,
    int        flags,
    const struct sockaddr FAR*    to,
    int        tolen) {

    WSABUF    WsaBuf;
    DWORD    cSent;
    int        Status;

    WsaBuf.len = len;
    WsaBuf.buf = (char *)buf;

    Status = WSASendTo(s, &WsaBuf, 1, &cSent, flags, to, tolen, NULL, NULL);

    if (!Status)
        Status = cSent;

    return Status;

}    // sendto()

