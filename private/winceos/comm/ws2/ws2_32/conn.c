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


int WSAAPI listen (
    SOCKET s,
    int backlog) {

    int            Err, Status = 0;
    WsSocket    *pSock;

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        Status = pSock->pProvider->ProcTable.lpWSPListen(pSock->hWSPSock, 
            backlog, &Err);

        if (Status && ! Err) {
            ASSERT(0);
            Err = WSASYSCALLFAILURE;
        }

        DerefSocket(pSock);
    }

    if (Err) {
        SetLastError(Err);
        Status = SOCKET_ERROR;
    }
    
    return Status;

}    // listen()


int WSAAPI connect (
    SOCKET s,
    const struct sockaddr FAR*  name,
    int namelen) {

    return WSAConnect(s, name, namelen, NULL, NULL, NULL, NULL);

}    // connect()


int
WSAConnect(
    SOCKET s,
    const struct sockaddr FAR * name,
    int namelen,
    LPWSABUF lpCallerData,
    LPWSABUF lpCalleeData,
    LPQOS lpSQOS,
    LPQOS lpGQOS
    )
{
    int Err;
    int Status;
    WsSocket *pSock;

    Err = CheckSockaddr(name, namelen);
    if (0 == Err) {
        Err = RefSocketHandle(s, &pSock);
        if (0 == Err) {
            if (IsWsCmExtLoaded()) {
                // Let Connection Manager know about it
                Err = g_pfnWsCmExtConnect(
                                pSock,
                                name,
                                namelen,
                                lpCallerData,
                                lpCalleeData,
                                lpSQOS,
                                lpGQOS
                                );
            } else {
                Status = pSock->pProvider->ProcTable.lpWSPConnect(
                                                        pSock->hWSPSock,
                                                        name,
                                                        namelen,
                                                        lpCallerData,
                                                        lpCalleeData,
                                                        lpSQOS,
                                                        lpGQOS,
                                                        &Err
                                                        );

                if (Status && ! Err) {
                    ASSERT(0);
                    Err = WSASYSCALLFAILURE;
                }
            }
            DerefSocket(pSock);
        }
    }

    if (Err) {
        SetLastError(Err);
        Status = SOCKET_ERROR;
    } else {
        Status = 0;
    }

    return Status;

}    // WSAConnect()


int WSAAPI shutdown (
    SOCKET s,
    int how) {

    int            Err, Status = 0;
    WsSocket    *pSock;

    if ((SD_BOTH != how) && (SD_RECEIVE != how) && (SD_SEND != how)) {
        Err = WSAEINVAL;
    } else {
        Err = RefSocketHandle(s, &pSock);
        if (!Err) {
            // check validity of the how parameter in stack below
            // some cases of it may be dependent on type of socket
    
            Status = pSock->pProvider->ProcTable.lpWSPShutdown(pSock->hWSPSock, 
                how, &Err);

            if (Status && ! Err) {
                ASSERT(0);
                Err = WSASYSCALLFAILURE;
            }
            DerefSocket(pSock);
        }
    }

    if (Err) {
        SetLastError(Err);
        Status = SOCKET_ERROR;
    }
    
    return Status;

}    // shutdown()



