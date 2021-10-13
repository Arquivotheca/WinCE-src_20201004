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

#include <windows.h>
#include <winsock2.h>

//
// Completion Callbacks are an IO Completion Ports Layer
// that provide a callback thread that calls a completion routine
// when queued completion statuses are returned
//


#ifdef __cplusplus
extern "C" {
#endif


typedef
VOID
(WINAPI *LPOVERLAPPED_COMPLETION_ROUTINE)(
    DWORD dwErrorCode,
    DWORD dwNumberOfBytesTransferred,
    LPOVERLAPPED lpOverlapped
);



BOOL InitializeIoCompletionCallbacks();
BOOL UnInitializeIoCompletionCallbacks();



//
// CompletionCallback enabled wrapper around system WSARecv
//
int
WSAAPI
CeCcbWSARecv(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesRecvd,
    IN OUT LPDWORD  lpFlags,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);

//
// CompletionCallback enabled wrapper around system WSARecvFrom
//
int
WSAAPI
CeCcbWSARecvFrom(
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
// CompletionCallback enabled wrapper around system WSASend
//
int
WSAAPI
CeCcbWSASend(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesSent,
    IN DWORD        dwFlags,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);


//
// CompletionCallback enabled wrapper around system WSASendTo
//
int
WSAAPI
CeCcbWSASendTo(
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
// CompletionCallback enabled wrapper around system closesocket
//
int
WSAAPI
CeCcbCloseSocket(
    IN SOCKET Socket
);


//
// CompletionCallback enabled wrapper around system BindIoCompletionCallback
//
BOOL
WINAPI
CeCcbBindIoCompletionCallback(
    IN HANDLE FileHandle,
    IN LPOVERLAPPED_COMPLETION_ROUTINE Function,
    IN ULONG Flags
);


#ifdef __cplusplus
}
#endif
