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
#pragma once


//
// I/O Completion port emulation:
//
//  - Emulates generic I/O completion port behavior through
//    PostQueuedCompletionStatus and GetQueuedCompletionStatus
//    and Winsock SOCKET associated with I/O completion ports
//    by layering in a WSARecv/WSARecvFrom/WSASend/WSASendTo
//    shim to trap overlapped completion.
//
//  - CeCloseIoCompletionPort must be used instead of CloseHandle
//    since the CE kernel does not know about IoCompletionPorts
//


#ifdef __cplusplus
extern "C" {
#endif


//
// On CE, creating an IoCompletionPort must happen with
// CeCreateIoCompletionPort
//
HANDLE
WINAPI
CeCreateIoCompletionPort(
    __in     HANDLE FileHandle,
    __in_opt HANDLE ExistingCompletionPort,
    __in     ULONG_PTR CompletionKey,
    __in     DWORD NumberOfConcurrentThreads
);


//
// On CE, an IoCompletionPort must be closed with
// CeCloseIoCompletionPort since CloseHandle
// does not know about IoCompletionPorts.
//
BOOL
CeCloseIoCompletionPort(
    __in HANDLE CompletionPort
);


//
// Return the next completed overlapped operation on this IoCompletionPort after potentially waiting
//
BOOL
WINAPI
CeGetQueuedCompletionStatus(
    __in  HANDLE CompletionPort,
    __out LPDWORD lpNumberOfBytesTransferred,
    __out PULONG_PTR lpCompletionKey,
    __out LPOVERLAPPED *lpOverlapped,
    __in  DWORD Milliseconds
);



//
// Post an overlapped buffer to the IoCompletionPort
//
BOOL
WINAPI
CePostQueuedCompletionStatus(
    __in     HANDLE CompletionPort,
    __in     DWORD NumberOfBytesTransferred,
    __in     ULONG_PTR CompletionKey,
    __in_opt LPOVERLAPPED lpOverlapped
);


//
// IoCompletionPort enabled wrapper around system WSARecv
//
int
WSAAPI
CeIocWSARecv(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesRecvd,
    IN OUT LPDWORD  lpFlags,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);


//
// IoCompletionPort enabled wrapper around system WSARecvFrom
//
int
WSAAPI
CeIocWSARecvFrom(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesRecvd,
    IN OUT LPDWORD  lpFlags,
    OUT struct sockaddr FAR * lpFrom,
    IN OUT LPINT    lpFromlen,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);


//
// IoCompletionPort enabled wrapper around system WSASend
//
int
WSAAPI
CeIocWSASend(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesSent,
    IN DWORD        dwFlags,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);


//
// IoCompletionPort enabled wrapper around system WSASendTo
//
int
WSAAPI
CeIocWSASendTo(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesSent,
    IN DWORD        dwFlags,
    IN const struct sockaddr FAR * lpTo,
    IN int iTolen,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);


//
// IoCompletionPort enabled wrapper around system closesocket
//
int
WSAAPI
CeIocCloseSocket(
    IN SOCKET Socket
);


#ifdef __cplusplus
}
#endif
