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
#include <completioncallback.h>
#include <iocompletionports.h>
#include <iocompletioncallback.h>
#include <LockFreeSpinInitializer.hxx>


//
// implement the c-shim-layer
//
CeIoCompletionCallback_t*   g_CompletionCallback;
LockFreeSpinInitializer     g_CompletionCallbackSystemInitialized;


//
// FORWARD DECLARATIONS
//
void OneTimeInitializeCompletionCallbacks();
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
)
{
    if(lpOverlapped)
        {
        BOOL result = g_CompletionCallback->AssociateOverlappedWithSocket(Socket, lpOverlapped);
        ASSERT(result);
        }

    return CeIocWSARecv(
                    Socket,
                    lpBuffers,
                    dwBufferCount,
                    lpNumberOfBytesRecvd,
                    lpFlags,
                    lpOverlapped,
                    lpCompletionRoutine);
}


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
)
{
    if(lpOverlapped)
        {
        BOOL result = g_CompletionCallback->AssociateOverlappedWithSocket(Socket, lpOverlapped);
        ASSERT(result);
        }

    return CeIocWSARecvFrom(
                    Socket,
                    lpBuffers,
                    dwBufferCount,
                    lpNumberOfBytesRecvd,
                    lpFlags,
                    lpFrom,
                    lpFromlen,
                    lpOverlapped,
                    lpCompletionRoutine);
}


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
)
{
    if(lpOverlapped)
        {
        BOOL result = g_CompletionCallback->AssociateOverlappedWithSocket(Socket, lpOverlapped);
        ASSERT(result);
        }

    return CeIocWSASend(
                    Socket,
                    lpBuffers,
                    dwBufferCount,
                    lpNumberOfBytesSent,
                    dwFlags,
                    lpOverlapped,
                    lpCompletionRoutine);
}


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
)
{
    if(lpOverlapped)
        {
        BOOL result = g_CompletionCallback->AssociateOverlappedWithSocket(Socket, lpOverlapped);
        ASSERT(result);
        }

    return CeIocWSASendTo(
                    Socket,
                    lpBuffers,
                    dwBufferCount,
                    lpNumberOfBytesSent,
                    dwFlags,
                    lpTo,
                    iTolen,
                    lpOverlapped,
                    lpCompletionRoutine);
}


//
// CompletionCallback enabled wrapper around system closesocket
//
int
WSAAPI
CeCcbCloseSocket(
    IN SOCKET Socket
)
{
    g_CompletionCallback->DisassociateSocket(Socket);

    // it is possible that some sockets are not associated
    // just call close socket on them
    return CeIocCloseSocket(Socket);
}


//
// CompletionCallback enabled wrapper around system BindIoCompletionCallback
//
BOOL
WINAPI
CeCcbBindIoCompletionCallback(
    IN HANDLE FileHandle,
    IN LPOVERLAPPED_COMPLETION_ROUTINE Function,
    IN ULONG Flags
)
{
    g_CompletionCallbackSystemInitialized.InitializeIfNecessary(OneTimeInitializeCompletionCallbacks);

    BOOL result = g_CompletionCallback->AssociateSocketAndCompletionRoutine((SOCKET) FileHandle, Function);
    if(FALSE == result)
        {
        //
        //  last error should be set
        //
        return FALSE;
        }

    return TRUE;
}


//
// Initialization Routine
//
void OneTimeInitializeCompletionCallbacks()
{
    g_CompletionCallback->Initialize();
}

//
// Use this within applications dllmain
//
BOOL InitializeIoCompletionCallbacks()
{
    g_CompletionCallback = new CeIoCompletionCallback_t();
    if(g_CompletionCallback)
        {
        return TRUE;
        }

    return FALSE;
}
BOOL UnInitializeIoCompletionCallbacks()
{
    delete g_CompletionCallback;
    return TRUE;
}
