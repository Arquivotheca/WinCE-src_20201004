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

options.c

ioctl's and socket options


FILE HISTORY:
    OmarM     16-Oct-2000

*/

#include "winsock2p.h"
#include <mswsock.h>

int CheckOptions(char * optval, int * optlen) {
    int Err = 0;

    if (optval && optlen) {
        __try {
            if (*optlen < sizeof(int))
                Err = WSAEFAULT;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Err = WSAEFAULT;
        }
    } else {
        Err = WSAEFAULT;
    }
    
    return Err;
    
}   // CheckOptions()

/*  we'll let the underlying protocols do the work
int CopyProtInfo(WsSocket *pSock, char *optval, int *optlen) {
    int Err = WSAEFAULT;

    __try {
        if (*optlen >= sizeof(WSAPROTOCOL_INFO)) {
            memcpy(optval, &pSock->ProtInfo, sizeof(WSAPROTOCOL_INFO));
            *optlen = sizeof(WSAPROTOCOL_INFO);
            Err = 0;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;   // set error code at beginning.
    }

    return Err;

}   // CopyProtInfo()
*/

int WSAAPI getsockopt (
    IN SOCKET   s,
    IN int      level,
    IN int      optname,
    OUT char    * optval,
    IN OUT int  * optlen) {

    int         Err, Status;
    WsSocket    *pSock;

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        Err = CheckOptions(optval, optlen);
        if (!Err) {
            Status = pSock->pProvider->ProcTable.lpWSPGetSockOpt(
                pSock->hWSPSock, level, optname, optval, optlen, &Err);

            if (Status && ! Err) {
                ASSERT(0);
                Err = WSASYSCALLFAILURE;
            }
        }

        DerefSocketHandle(pSock->hSock);
    }

    if (Err) {
        SetLastError(Err);
        Status = SOCKET_ERROR;
    } else
        Status = 0;
    
    return Status;

}   // getsockopt()


int WSAAPI setsockopt(
    IN SOCKET s,
    IN int level,
    IN int optname,
    IN const char * optval,
    IN int optlen) {

    int         Err, Status;
    WsSocket    *pSock;

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        Err = CheckOptions((char *)optval, &optlen);
        if (!Err) {
            Status = pSock->pProvider->ProcTable.lpWSPSetSockOpt(
                pSock->hWSPSock, level, optname, optval, optlen, &Err);

            if (Status && ! Err) {
                ASSERT(0);
                Err = WSASYSCALLFAILURE;
            }
        }
        DerefSocketHandle(pSock->hSock);
    }

    if (Err) {
        SetLastError(Err);
        Status = SOCKET_ERROR;
    } else
        Status = 0;
    
    return Status;

}   // setsockopt()


int WSPAPI WSARecvMsg (
    SOCKET s,
    LPWSAMSG lpMsg,
    LPDWORD lpNumberOfBytesRecvd,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

int WSAAPI WSAIoctl(
    IN SOCKET       s,
    IN DWORD        dwIoControlCode,
    IN LPVOID       lpvInBuffer,
    IN DWORD        cbInBuffer,
    OUT LPVOID      lpvOutBuffer,
    IN DWORD        cbOutBuffer,
    OUT LPDWORD     lpcbBytesReturned,
    IN LPWSAOVERLAPPED  lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE   lpCompletionRoutine) {

    int         Err, Status;
    WsSocket    *pSock;
    WSATHREADID WsaThread;
    BOOLEAN     bNeedDeref = FALSE;

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        bNeedDeref = TRUE;

        WsaThread.ThreadHandle = (HANDLE)GetCurrentThreadId();

        Status = pSock->pProvider->ProcTable.lpWSPIoctl(pSock->hWSPSock, 
                dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, 
                cbOutBuffer, lpcbBytesReturned, lpOverlapped, 
                lpCompletionRoutine, &WsaThread, &Err);

        if (Status && ! Err) {
            ASSERT(0);
            Err = WSASYSCALLFAILURE;
            goto Exit;
        }
        
        if (SIO_GET_EXTENSION_FUNCTION_POINTER == dwIoControlCode) {
            
            GUID RecvMsgId = WSAID_WSARECVMSG;
            
            if (cbInBuffer < sizeof(GUID) || lpvInBuffer == NULL || 
                    cbOutBuffer < sizeof(LPVOID) || lpvOutBuffer == NULL) {
                Err = WSAEFAULT;
            } else if (0 == memcmp(lpvInBuffer, &RecvMsgId, sizeof(RecvMsgId))) {
                //
                // CE doesn't support lsp extensions; assert that the provider 
                // did't return WSARecvMsg extension API, WSARecvMsg is a
                // special case & ws2 will always return its own pointer for this API
                //
                ASSERT(Err != NO_ERROR);
                
                Err = (int)&WSARecvMsg;
                memcpy(lpvOutBuffer, &Err, sizeof(DWORD));
                *lpcbBytesReturned = sizeof(LPVOID);
                Err = NO_ERROR;
            }
        }

    }

Exit:
    if (bNeedDeref) {
        DerefSocketHandle(pSock->hSock);
    }
    
    if (Err) {
        SetLastError(Err);
        Status = SOCKET_ERROR;
    } else
        Status = 0;
    
    return Status;

}   // WSAIoctl()


int WSAAPI ioctlsocket(
    IN SOCKET       s,
    IN long         cmd,
    IN OUT u_long   * argp) {

    int     Status;
    DWORD   cSize;

    Status = WSAIoctl(s, cmd, argp, sizeof(*argp), argp, sizeof(*argp), 
        &cSize, NULL, NULL);
    
    return Status;

}   // getsockopt()


