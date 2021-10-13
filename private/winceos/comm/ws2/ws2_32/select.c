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

select.c

select related winsock2 functions


FILE HISTORY:
    OmarM     16-Oct-2000

*/


#include "winsock2p.h"
#include "windev.h"

// structure to allow optimizing the predominant case of select'ing on one or two sockets
typedef struct _STACK_SOCK_FD {
    u_int osf_count_select1;
    SOCKET osf_array_select1;
    u_int osf_count_select2;
    SOCKET osf_array_select2;
    u_int osf_count_map1;
    SOCKET osf_array_map1;
    u_int osf_count_map2;
    SOCKET osf_array_map2;
} STACK_SOCK_FD;

#define STACK_FDS_SIZE 2    // Number of select sockets supported on stack

void DerefFds(fd_set *pFds, int *pcRefs) {
    
    u_int        i;

    __try {
        if (pFds) {
            for (i = 0; (i < pFds->fd_count) && (*pcRefs > 0); i++) {
                DerefSocketHandle(pFds->fd_array[i]);
                (*pcRefs)--;
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        ;    // don't need to do anything here yet.
    }

}    // DerefFds()


int CopyFds(fd_set **pNewSelect, fd_set **pNewMap, fd_set *pUserFds, int *pcTotal, int *pcRefs, 
            WsProvider **ppProv, STACK_SOCK_FD * pStackFds) {

    int            Err;
    u_int        cCount, i;
    WsSocket    *pSock;
    fd_set      *pSelect;
    fd_set      *pMap;
    BOOL        bFromHeap;

    if (NULL == pUserFds) {
        return 0;
    }
    
    Err = 0;
    pSelect = NULL;
    bFromHeap = TRUE;

    __try {
        cCount = pUserFds->fd_count;
        if (cCount) {
            i = cCount * sizeof(SOCKET) + sizeof(int);
            if (STACK_FDS_SIZE >= cCount) {
                pSelect = (fd_set *)pStackFds;
                bFromHeap = FALSE;
            } else {
                pSelect = LocalAlloc(LPTR, i*2);    // allocate enough for pSelect and pMap
            }
            if (NULL == pSelect) {
                Err = WSA_NOT_ENOUGH_MEMORY;
            } else {
                pMap = (fd_set *)((DWORD)pSelect + i);
                *pcTotal         += cCount;
                pMap->fd_count    = cCount;
                pSelect->fd_count = cCount;
                for (i = 0; i < cCount; i++) {
                    Err = RefSocketHandle(pUserFds->fd_array[i], &pSock);
                    if (Err) {
                        break;
                    } else {
                        (*pcRefs)++;
                        pMap->fd_array[i] = pSock->hWSPSock;
                        pSelect->fd_array[i] = pSock->hWSPSock;
                        if (*ppProv) {
                            if (*ppProv != pSock->pProvider) {
                                Err = WSAEINVAL;
                                break;
                            }
                         } else
                            *ppProv = pSock->pProvider;
                    }    
                }
                if (0 == Err) {
                    *pNewSelect = pSelect;
                    *pNewMap = pMap;
                }
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        Err = WSAEFAULT;
    }

    if ((Err) && (bFromHeap) && (pSelect)) {
        LocalFree(pSelect);
    }
    
    return Err;

}    // CopyFds()


//
// Map select results from WSP socket contexts to WS2 user socket handles
//
void ResultFds(fd_set *pUser, fd_set *pSelect, fd_set *pMap) {
    u_int i, j;
    u_int n, m;

    if (pUser && pSelect && pMap) {
        n = pSelect->fd_count;
        m =  pMap->fd_count;

        for (i = 0; i < n; i++) {
            for (j = 0; j < m; j++) {
                if (pSelect->fd_array[i] == pMap->fd_array[j]) {
                    pSelect->fd_array[i] = pUser->fd_array[j];
                    break;
                }
            }
        }

        pUser->fd_count = n;
        for (i = 0; i < n; i++) {
            pUser->fd_array[i] = pSelect->fd_array[i];
        }
    } else if (pUser) {
        __try {
            pUser->fd_count = 0;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            ;    // don't need to do anything here yet.
        }
    }

}   // ResultFds


int WSAAPI select (
    IN int            nfds,
    IN OUT fd_set    *readfds,
    IN OUT fd_set    *writefds,
    IN OUT fd_set    *exceptfds,
    IN const struct timeval FAR * timeout) {
    
    WsProvider  *pProv;
    int         Err;
    int         i;
    fd_set      * Select[3];
    fd_set      * Map[3];       // auxillary array to help map from WSP socket context to WS2 user handle.
    fd_set      * User[3];
    STACK_SOCK_FD StackFds[3];  // stack allocated space to avoid heap allocations for small select requests
    
    int                cTotal, cRefs, cReady;
    struct timeval    Time, *pTime;

    cTotal = cRefs = 0;

    Err = 0;
    for (i = 0; i < 3; i++) {
        Select[i] = Map[i] = NULL;
    }
    User[0] = readfds;
    User[1] = writefds;
    User[2] = exceptfds;

    cReady = SOCKET_ERROR;
    
    // copy timeout to make sure that we don't crash in lower layers b/c of
    // bad timeout address...
    pTime = (struct timeval *)timeout;
    if (pTime) {
        __try {
            pTime = &Time;
            memcpy(&Time, timeout, sizeof(Time));
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            Err = WSAEFAULT;
        }
    }
    if (! Err) {
        pProv = NULL;

        for (i = 0; (i < 3) && (0 == Err); i++) {
            Err = CopyFds(&Select[i], &Map[i], User[i], &cTotal, &cRefs, &pProv, &StackFds[i]);
        }
        if (!Err) {
            if (!cTotal || !pProv) {
                Err = WSAEINVAL;
            } else {
                cReady = pProv->ProcTable.lpWSPSelect(nfds, Select[0], Select[1], Select[2],
                                                        pTime, &Err);
            }
        }
    }

    // cleanup

    for (i=0; i<3; i++) {
        DerefFds(User[i], &cRefs);
        ResultFds(User[i], Select[i], Map[i]);
        if ((Select[i]) && (Select[i] != (fd_set *)&StackFds[i])) {
            LocalFree(Select[i]);
        }
    }

    if (SOCKET_ERROR == cReady) {
        SetLastError(Err);
    }
    return cReady;

}    // select()


int __WSAFDIsSet (
    SOCKET       fd,
    FD_SET FAR * set) {

    int i = (set->fd_count & 0xFFFF);

    while (i--)
        if (set->fd_array[i] == fd)
            return 1;

    return 0;

}   // __WSAFDIsSet()


int WPUFDIsSet(
    SOCKET s,  
    FD_SET FAR * set){

    return __WSAFDIsSet(s, set);

}    // WPUFDIsSet()


int WSAAPI WSAEventSelect(
    IN SOCKET s,
    IN WSAEVENT hEventObject,
    IN long lNetworkEvents) {

    int            Err, Status;
    WsSocket    *pSock;

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        Status = pSock->pProvider->ProcTable.lpWSPEventSelect(pSock->hWSPSock, 
            hEventObject, lNetworkEvents, &Err);

        if (Status && ! Err)
            Err = WSASYSCALLFAILURE;

        DerefSocket(pSock);
    }

    if (Err) {
        Status = SOCKET_ERROR;
        SetLastError(Err);
    } else
        Status = 0;
    
    return Status;

}    // WSAEventSelect()


int WSAAPI WSAEnumNetworkEvents(
    IN SOCKET s,
    IN WSAEVENT hEventObject,
    OUT LPWSANETWORKEVENTS lpNetworkEvents) {

    int            Err, Status;
    WsSocket    *pSock;

    Err = RefSocketHandle(s, &pSock);
    if (!Err) {
        Status = pSock->pProvider->ProcTable.lpWSPEnumNetworkEvents(
            pSock->hWSPSock, hEventObject, lpNetworkEvents, &Err);

        if (Status && ! Err)
            Err = WSASYSCALLFAILURE;

        DerefSocket(pSock);
    }

    if (Err) {
        Status = SOCKET_ERROR;
        SetLastError(Err);
    } else
        Status = 0;
    
    return Status;

}    // WSAEnumNetworkEvents()

