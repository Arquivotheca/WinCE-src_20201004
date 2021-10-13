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
// consetup.cpp
// Connection setup

#ifdef SUPPORT_IPV6
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <winsock.h>
#endif
#include <mstcpip.h>
#include "loglib.h"
#include "consetup.h"
#ifndef UNDER_CE
#include <assert.h>
#endif

extern TCHAR  *g_tszInterfaceToBind;
extern BOOL   g_fHardBindToInterface;

// --------------------------------------------------------------------
WORD GetSocketLocalPort(SOCKET sock)
//
// Returns local port number (only works if called after the socket
// was bound with bind())
// Port returned will be in host-byte-order!
// --------------------------------------------------------------------
{
#ifdef SUPPORT_IPV6
    SOCKADDR_STORAGE addr;
#else
    SOCKADDR addr;
#endif
    int addr_len;

    memset((void*)&addr, 0, sizeof(addr));
    addr_len = sizeof(addr);
    if (SOCKET_ERROR == getsockname(sock, (LPSOCKADDR)&addr, &addr_len))
        return 0; // error

#ifdef SUPPORT_IPV6
    switch (addr.ss_family)
#else
    switch (addr.sa_family)
#endif
    {
    case AF_INET:
        return ntohs(((struct sockaddr_in*)&addr)->sin_port);
#ifdef SUPPORT_IPV6
    case AF_INET6:
        return ntohs(((struct sockaddr_in6*)&addr)->sin6_port);
#endif
    default:
        return 0;
    }
}

// --------------------------------------------------------------------
INT ListenSocket(
    WORD wIpVer,
    INT protocol,               // SOCK_STREAM or SOCK_DGRAM
    WORD* pwListenPort,         // on which port to listen on - if 0, then will fill-in with port number selected
    SOCKET* ListeningSocks[],
    INT* pNumListeningSocks,
    DWORD* pdwWSAError)
// --------------------------------------------------------------------
{
    INT nRet = 0;
#ifdef SUPPORT_IPV6
    const INT MAX_NUM_SOCKETS = FD_SETSIZE;
    CHAR szPort[10];
#else
    const INT MAX_NUM_SOCKETS = 1;
#endif
    INT i;

    *pdwWSAError = 0;

    Log(DEBUG_MSG, TEXT("+ListenSocket()"));

    *ListeningSocks = new SOCKET[MAX_NUM_SOCKETS];
    for (i=0; i<MAX_NUM_SOCKETS; i+=1)
        (*ListeningSocks)[i] = INVALID_SOCKET;
    *pNumListeningSocks = 0;

#ifdef SUPPORT_IPV6
    ADDRINFO Hints;
    ADDRINFO *AddrInfo, *AI;

    //
    // By setting the AI_PASSIVE flag in the hints to getaddrinfo, we're
    // indicating that we intend to use the resulting address(es) to bind
    // to a socket(s) for accepting incoming connections.  This means that
    // when the Address parameter is NULL, getaddrinfo will return one
    // entry per allowed protocol family containing the unspecified address
    // for that family.
    //
    memset(&Hints, 0, sizeof(Hints));
    if (wIpVer == 4)  
        Hints.ai_family = PF_INET;
    else if (wIpVer == 6)
        Hints.ai_family = PF_INET6;
    else
        Hints.ai_family = PF_UNSPEC;
    Hints.ai_socktype = protocol;
    Hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

    _itoa_s(*pwListenPort, szPort, 10, 10);
    *pdwWSAError = getaddrinfo(NULL, szPort, &Hints, &AddrInfo);
    if (*pdwWSAError != 0)
    {
        Log(ERROR_MSG, TEXT("getaddrinfo failed with error %d"), *pdwWSAError);
        goto Error;
    }

    //
    // For each address getaddrinfo returned, we create a new socket,
    // bind that address to it, and create a queue to listen on.
    //
    for (i = 0, AI = AddrInfo; AI != NULL ; AI = AI->ai_next)
    {
        // Highly unlikely, but check anyway.
        if (i == MAX_NUM_SOCKETS)
        {
            Log(WARNING_MSG, TEXT("WARNING: getaddrinfo() returned more addresses than we could use."));
            break;
        }

        // Support only PF_INET and PF_INET6.
        if ((AI->ai_family != PF_INET) && (AI->ai_family != PF_INET6)) continue;

        // Open a socket with the correct address family for this address.
        (*ListeningSocks)[i] = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol);
        if ((*ListeningSocks)[i] == INVALID_SOCKET)
        {
            *pdwWSAError = WSAGetLastError();
            Log(WARNING_MSG, TEXT("socket() failed with error %ld"), *pdwWSAError);
            continue;
        }

        //
        // bind() associates a local address and port combination
        // with the socket just created. This is most useful when
        // the application is a server that has a well-known port
        // that clients know about in advance.
        //
        if (bind((*ListeningSocks)[i], AI->ai_addr, AI->ai_addrlen) == SOCKET_ERROR) {
            *pdwWSAError = WSAGetLastError();
            Log(ERROR_MSG, TEXT("bind() failed with error %ld"), *pdwWSAError);
            closesocket((*ListeningSocks)[i]);
            continue;
        }

        //
        // listen() for incoming connections (all connections are TCP based)
        //
        if (protocol == SOCK_STREAM)
        {
            if (listen((*ListeningSocks)[i], 5) == SOCKET_ERROR)
            {
                *pdwWSAError = WSAGetLastError();
                Log(ERROR_MSG, TEXT("listen() failed with error %ld"), *pdwWSAError);
                closesocket((*ListeningSocks)[i]);
                continue;
            }
        }

        if (*pwListenPort == 0)
            *pwListenPort = GetSocketLocalPort(*ListeningSocks[i]);

        Log(DEBUG_MSG, TEXT("Listening on port %d, protocol %s, protocol family %s (sd %d, #%d)"),
               *pwListenPort,
               AI->ai_socktype == SOCK_STREAM ? TEXT("TCP") : TEXT("UDP"),
               (AI->ai_family == PF_INET) ? TEXT("PF_INET") : TEXT("PF_INET6"),
               (*ListeningSocks)[i], i);

        //
        // Update count if and only if successfully listening on a socket
        //
        i += 1;
    }
    *pNumListeningSocks = i;

    if (pNumListeningSocks > 0) *pdwWSAError = 0;
    
    freeaddrinfo(AddrInfo);
#else
    struct sockaddr_in addrServer;

    *ListeningSocks = new SOCKET[1];

    (*ListeningSocks)[0] = socket(AF_INET, protocol, 0);
    if ((*ListeningSocks)[0] == INVALID_SOCKET)
    {
        *pdwWSAError = WSAGetLastError();
        Log(ERROR_MSG, TEXT("socket() failed with error %ld"), *pdwWSAError);
        goto Error;
    }

    addrServer.sin_family = AF_INET;     
    addrServer.sin_addr.s_addr = htonl(INADDR_ANY);
    addrServer.sin_port = htons(*pwListenPort);

    if (SOCKET_ERROR == bind((*ListeningSocks)[0], (PSOCKADDR)&addrServer, sizeof(addrServer)))
    {
        *pdwWSAError = WSAGetLastError();
        Log(ERROR_MSG, TEXT("bind(sockListen) failed, WSAError: %d"), *pdwWSAError);
        closesocket((*ListeningSocks)[0]);
        goto Error;
    }

    // initialize listening sockets
    //
    if (protocol == SOCK_STREAM && SOCKET_ERROR == listen((*ListeningSocks)[0], 5)) {
        *pdwWSAError = WSAGetLastError();
        Log(ERROR_MSG, TEXT("listen() failed, WSAError: %ld"), *pdwWSAError);
        closesocket((*ListeningSocks)[0]);
        goto Error;
    }

    *pNumListeningSocks += 1;
    if (*pwListenPort == 0) {
        *pwListenPort = GetSocketLocalPort((*ListeningSocks)[0]);
    }

    Log(DEBUG_MSG, TEXT("Listening on port %d, protocol %s, protocol family %s (sd %d, #%d)"),
        *pwListenPort,
        protocol == SOCK_STREAM ? TEXT("TCP") : TEXT("UDP"),
        TEXT("AF_INET"),
        (*ListeningSocks)[0], 0);
#endif

Error:
    Log(DEBUG_MSG, TEXT("-ListenSocket()"));

    // If at least one socket has been successfully setup, then return success.
    if (*pNumListeningSocks > 0) return 0;

    // Cleanup
    if (*ListeningSocks != NULL)
    {
        for (i=0; i<MAX_NUM_SOCKETS; i+=1)
        {
            if ((*ListeningSocks)[i] != INVALID_SOCKET &&
                (*ListeningSocks)[i] != SOCKET_ERROR)
                closesocket((*ListeningSocks)[i]);
        }
        delete [] *ListeningSocks;
        *ListeningSocks = NULL;
    }
    return SOCKET_ERROR;      
}

// --------------------------------------------------------------------
SOCKET ConnectSocketByAddrFromSocket(
    SOCKET sock,             // will use this socket (instead of creating a new one) if in_sock = INVALID_SOCKET
    SOCKET addr_sock,        // socket from which to derive address
    WORD wServerPort,
    WORD wIpVer,
    INT protocol,            // SOCK_STREAM or SOCK_DGRAM
    BOOL fHardClose,
    LPSOCKADDR lpLocalAddrOut, DWORD* pdwLocalAddrOutLen   ,    // will fill this in if not NULL
    LPSOCKADDR lpRemoteAddrOut, DWORD* pdwRemoteAddrOutLen, // will fill this in if not NULL
    DWORD* pdwWSAError)
// --------------------------------------------------------------------
{
#ifdef SUPPORT_IPV6
    SOCKADDR_STORAGE remote_addr;  // incoming address
#else
    SOCKADDR remote_addr;          // incoming address
#endif
    int remote_addr_len = sizeof(remote_addr);

    Log(DEBUG_MSG, TEXT("+ConnectSocketByAddrFromSock()"));
    
    if (getpeername(addr_sock, (LPSOCKADDR)&remote_addr, &remote_addr_len) == SOCKET_ERROR)
    {
        *pdwWSAError = WSAGetLastError();
        Log(ERROR_MSG, TEXT("getpeername() failed, error=%ld"), *pdwWSAError);
        goto Error;
    }

    sock = ConnectSocketByAddr(
        sock, (LPSOCKADDR)&remote_addr, remote_addr_len,
        wServerPort, wIpVer, protocol, fHardClose,
        lpLocalAddrOut, pdwLocalAddrOutLen,
        lpRemoteAddrOut, pdwRemoteAddrOutLen,
        pdwWSAError);

Error:
    Log(DEBUG_MSG, TEXT("-ConnectSocketByAddrFromSock()"));
    return sock;
}


// --------------------------------------------------------------------
SOCKET ConnectSocketByAddr(
    SOCKET sock,             // will use this socket (instead of creating a new one) if sock = INVALID_SOCKET
    LPSOCKADDR pServerAddr, INT nServerAddrLen,
    WORD wServerPort,
    WORD wIpVer,
    INT protocol,               // SOCK_STREAM or SOCK_DGRAM
    BOOL fHardClose,
    LPSOCKADDR lpLocalAddrOut, DWORD* pdwLocalAddrOutLen   ,    // will fill this in if not NULL
    LPSOCKADDR lpRemoteAddrOut, DWORD* pdwRemoteAddrOutLen, // will fill this in if not NULL
    DWORD* pdwWSAError
// --------------------------------------------------------------------
) {
    TCHAR* szAddr_copy = NULL;
    INT nAddrLen;

    Log(DEBUG_MSG, TEXT("+ConnectSocketByAddr()"));

#ifdef SUPPORT_IPV6
    CHAR szAddr[NI_MAXHOST];
#else
    CHAR* szAddr;
#endif

#ifdef SUPPORT_IPV6
    if (getnameinfo(pServerAddr, nServerAddrLen,
        szAddr, sizeof(szAddr), NULL, 0,
        NI_NUMERICHOST | (protocol == SOCK_DGRAM ? NI_DGRAM : 0))
        != 0)
    {
        *pdwWSAError = WSAGetLastError();
        Log(ERROR_MSG, TEXT("getnameinfo() failed, error=%ld"), *pdwWSAError);
        goto Error;
    }
#else
    szAddr = inet_ntoa(((SOCKADDR_IN*)pServerAddr)->sin_addr);
    if (szAddr == NULL)
    {
        Log(ERROR_MSG, TEXT("inet_ntoa() failed"));
        goto Error;
    }
#endif
    nAddrLen = strlen(szAddr) + 1;
    szAddr_copy = new TCHAR[nAddrLen];
    
#ifdef UNICODE
    MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS,
        szAddr, nAddrLen,
        szAddr_copy, nAddrLen);
#else
    strcpy(szAddr_copy, szAddr);
#endif
    sock = ConnectSocket(
        sock, szAddr_copy, wServerPort, wIpVer, protocol, fHardClose,
        lpLocalAddrOut, pdwLocalAddrOutLen,
        lpRemoteAddrOut, pdwRemoteAddrOutLen,
        pdwWSAError);

Error:
    if (szAddr_copy != NULL) delete [] szAddr_copy;
    
    Log(DEBUG_MSG, TEXT("-ConnectSocketByAddr()"));

    return sock;
}

// --------------------------------------------------------------------
SOCKET ConnectSocket(
    SOCKET sock,            // will use this socket (instead of creating a new one) if sock = INVALID_SOCKET
    TCHAR* szServerName,
    WORD wServerPort,
    WORD wIpVer,
    INT protocol,           // SOCK_STREAM or SOCK_DGRAM
    BOOL fHardClose,
    LPSOCKADDR lpLocalAddrOut, DWORD* pdwLocalAddrOutLen   ,    // will fill this in if not NULL
    LPSOCKADDR lpRemoteAddrOut, DWORD* pdwRemoteAddrOutLen, // will fill this in if not NULL
    DWORD* pdwWSAError) 
// --------------------------------------------------------------------
{
    BOOL fError = FALSE;
    char szServerAddr[MAX_SERVER_NAME_LEN];
    const int nConnAttempts = 3;
    int i;
    *pdwWSAError = 0;

    Log(DEBUG_MSG, TEXT("+ConnectSocket()"));
    Log(DEBUG_MSG, TEXT("performing name resolution"));

    // Get the server's address
#ifdef UNICODE
    wcstombs_s(NULL, szServerAddr, MAX_SERVER_NAME_LEN, szServerName, sizeof(szServerAddr));
#else
    strcpy(szServerAddr, szServerName);
#endif

#ifdef SUPPORT_IPV6
    ADDRINFO Hints, *pAddrInfo, *AI;
    memset(&Hints, 0, sizeof(Hints));

    if (wIpVer == 4)  
        Hints.ai_family = PF_INET;
    else if (wIpVer == 6)
        Hints.ai_family = PF_INET6;
    else
        Hints.ai_family = PF_UNSPEC;

    Hints.ai_socktype = protocol;
    Hints.ai_flags = 0;

    char szPort[10];
    _itoa_s(wServerPort, szPort, 10, 10);
    if(getaddrinfo(szServerAddr, szPort, &Hints, &pAddrInfo) != 0 || !pAddrInfo)
    {
        *pdwWSAError = WSAGetLastError();
        Log(ERROR_MSG, TEXT("getaddrinfo() failed for server %hs and port %hs, error = %d"),
            szServerAddr, szPort, *pdwWSAError);
        goto Error;
    }

    Log(DEBUG_MSG, TEXT("getaddrinfo() succeeded for %hs, port %hs"),
        szServerAddr, szPort);

#else
    SOCKADDR_IN ServerSockAddr;
    HOSTENT *pHostEnt = NULL;
    
    memset(&ServerSockAddr, 0, sizeof(ServerSockAddr));
    ServerSockAddr.sin_family = AF_INET;
    ServerSockAddr.sin_port = htons(wServerPort);

    if((ServerSockAddr.sin_addr.s_addr = inet_addr(szServerAddr)) == INADDR_NONE)
    {
        // It's not an IP Address - maybe its a name
        pHostEnt = gethostbyname(szServerAddr);

        if(pHostEnt == NULL)
        {
            // It's not a valid name either
            Log(ERROR_MSG, TEXT("Bad server name: %hs"), szServerAddr);
            goto Error;
        }
        else
        {
            // Ok, it was a valid server name, record the address
            memcpy(&(ServerSockAddr.sin_addr.s_addr), pHostEnt->h_addr_list[0], pHostEnt->h_length);
        }
    }
#endif

    // connect to the server
#ifdef SUPPORT_IPV6
    for(AI = pAddrInfo; AI != NULL; AI = AI->ai_next)
    {
        // Keep trying addresses until we find one we can connect to
        if(INVALID_SOCKET == sock &&
           INVALID_SOCKET == (sock = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol)))
        {
            *pdwWSAError = WSAGetLastError();
            Log(ERROR_MSG, TEXT("socket() failed, error = %d"), *pdwWSAError);
            goto Error;
        }
        else
        {
            //
            // Hard bind
            //
            if (g_fHardBindToInterface == TRUE)
            {
                if (g_tszInterfaceToBind == NULL) 
                {
                    Log(ERROR_MSG, TEXT("InterfaceToBind = NULL, can not bind to local address.\r\n"));
                }
                else
                {
                    SOCKADDR_STORAGE ssLocalAddr;
                    INT iSize = sizeof(ssLocalAddr);
                    if(WSAStringToAddress(g_tszInterfaceToBind, AI->ai_family, NULL, (SOCKADDR *)&ssLocalAddr, &iSize))
                    {
                        Log(ERROR_MSG, TEXT("WSAStringToAddress(%s) failed, error %d"), g_tszInterfaceToBind, WSAGetLastError());
                        continue;
                    }
    
                    if(bind(sock, (SOCKADDR *)&ssLocalAddr, iSize) == SOCKET_ERROR)
                    {
                        Log(ERROR_MSG, TEXT("bind() failed for hard bind to address %s, error %d"),g_tszInterfaceToBind, WSAGetLastError());
                        continue;
                    }
                    else
                    {
                        Log(DEBUG_MSG, TEXT("Bound successfully to local address: %s"), g_tszInterfaceToBind);
                    }
    
                    DWORD dwOptVal = 1;
                    if(WSAIoctl(sock, SIO_UCAST_IF, &dwOptVal, sizeof(dwOptVal), NULL, 0, (DWORD *)&iSize, NULL, NULL))
                    {
                        Log(ERROR_MSG, TEXT("WSAIoctl(SIO_UCAST_IF) or Hard Bind, error %d"), WSAGetLastError());
                        continue;
                    }
                }
            }
            // retry each connection a few times before giving up
            for(i=0; i<nConnAttempts; i+=1) {

                Log(DEBUG_MSG, TEXT("Attempting %s connection..."),
                    AI->ai_socktype == SOCK_STREAM ? TEXT("SOCK_STREAM") :
                    (AI->ai_socktype == SOCK_DGRAM ? TEXT("SOCK_DGRAM") : TEXT("?")));

                if (connect(sock, AI->ai_addr, AI->ai_addrlen) == SOCKET_ERROR)
                {
                    *pdwWSAError = WSAGetLastError();
                    if (*pdwWSAError == 10056)
                        break; // connected
                    else
                    {
                        Log(WARNING_MSG, TEXT("connect() failed, error=%ld, retrying..."), *pdwWSAError);
                        closesocket(sock);
                        sock = INVALID_SOCKET;
                        Sleep(2000);
                        continue;
                    }
                }
                else break;
            }
            if (i < nConnAttempts) break; // connected
        }
    }

    if(AI == NULL)
    {
        Log(ERROR_MSG, TEXT("Couldn't connect to server %s"), szServerName);
        goto Error;
    }

    if (!fError)
    {
        // WOO, HOO: Connected!
        char szAddrName[NI_MAXHOST];
        if (getnameinfo((LPSOCKADDR)AI->ai_addr, AI->ai_addrlen, szAddrName,
                sizeof(szAddrName), NULL, 0, NI_NUMERICHOST) != 0)
            strcpy_s(szAddrName, NI_MAXHOST, "<unknown>");

        Log(DEBUG_MSG, TEXT("Connected to %hs port %d (socktype=%s, family=%s)"),
            szAddrName, ntohs(SS_PORT(AI->ai_addr)),
            AI->ai_socktype == SOCK_STREAM ? TEXT("SOCK_STREAM") : (AI->ai_socktype == SOCK_DGRAM ? TEXT("SOCK_DGRAM") : TEXT("?")),
            AI->ai_family == PF_INET ? TEXT("PF_INET") : (AI->ai_family == PF_INET6 ? TEXT("PF_INET6") : TEXT("?")));

        //
        // copy local and peer addresses if requested
        //
        if (lpLocalAddrOut != NULL && pdwLocalAddrOutLen != NULL)
        {
            if (getsockname(sock, (LPSOCKADDR)lpLocalAddrOut, (int*)pdwLocalAddrOutLen) == SOCKET_ERROR)
            {
                Log(WARNING_MSG, TEXT("getsockname() failed, error = %d"), WSAGetLastError());
            }
        }
        if (lpRemoteAddrOut != NULL && pdwRemoteAddrOutLen != NULL)
        {
            if (*pdwRemoteAddrOutLen < AI->ai_addrlen) 
            {
                // we are not using big enough structures
                Log(WARNING_MSG, TEXT("Not enough room to store remote addr"));
                ASSERT(FALSE);
            }
            else
            {
                memcpy(lpRemoteAddrOut, AI->ai_addr, AI->ai_addrlen);
                *pdwRemoteAddrOutLen = AI->ai_addrlen;
            }
        }

        freeaddrinfo(pAddrInfo);
        pAddrInfo = NULL;
    }
#else
    // Create a socket
    if(sock == INVALID_SOCKET &&
        (sock = socket(AF_INET, protocol, 0)) == INVALID_SOCKET)
    {
        *pdwWSAError = WSAGetLastError();
        Log(ERROR_MSG, TEXT("socket() failed, error = %d"), *pdwWSAError);
        goto Error;
    }

    // retry each connection a few times before giving up
    for(i=0; i<nConnAttempts; i+=1)
    {
        if(connect(sock, (LPSOCKADDR)&ServerSockAddr, sizeof(ServerSockAddr)) == SOCKET_ERROR)
        {
            *pdwWSAError = WSAGetLastError();
            if (*pdwWSAError == 10056)
            {
                *pdwWSAError = 0;
                break; // connected
            }
            else
            {
                Log(WARNING_MSG, TEXT("connect() failed, error=%ld, retrying..."), *pdwWSAError);
                Sleep(2000);
                continue;
            }
        } else break;
    }
    if (i >= nConnAttempts) 
    {
        closesocket(sock);
        sock = INVALID_SOCKET;
        goto Error;
    }

    Log(DEBUG_MSG, TEXT("Connected to %hs port %d (protocol=%s, family=%s)"),
        inet_ntoa(ServerSockAddr.sin_addr), ntohs(ServerSockAddr.sin_port),
        protocol == SOCK_STREAM ?
            _T("SOCK_STREAM") : 
            (protocol == SOCK_DGRAM ? _T("SOCK_DGRAM") : _T("?")),
        _T("AF_INET"));

    // copy local and peer addresses if requested
    if (lpLocalAddrOut != NULL && pdwLocalAddrOutLen != NULL)
    {
        if (getsockname(sock, (LPSOCKADDR)lpLocalAddrOut, (int*)pdwLocalAddrOutLen) == SOCKET_ERROR)
            Log(WARNING_MSG, TEXT("getsockname() failed, error = %d"), WSAGetLastError());
    }
    if (lpRemoteAddrOut != NULL && pdwRemoteAddrOutLen != NULL)
    {
        if (getpeername(sock, (LPSOCKADDR)lpRemoteAddrOut, (int*)pdwRemoteAddrOutLen) == SOCKET_ERROR)
            Log(WARNING_MSG, TEXT("getpeername() failed, error %d"), WSAGetLastError());
    }

#endif // #ifdef SUPPORT_IPV6 (else)

    if (fHardClose) {
        // Set the linger options so that a hard close is performed when closesocket
        // is invoked. This is achieved by setting SO_LINGER with interval==0.
        struct linger hardClose;
        hardClose.l_onoff = 1;
        hardClose.l_linger = 0;

        if (SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&hardClose, sizeof(hardClose)))
        {
            *pdwWSAError = WSAGetLastError();
            Log(ERROR_MSG, TEXT("setsockopt() failed, WSA Error: %ld"), *pdwWSAError);
            closesocket(sock);
            sock = INVALID_SOCKET;
            goto Error;
        }
    }

Error:
    Log(DEBUG_MSG, TEXT("-ConnectSocket()"));

#ifdef SUPPORT_IPV6
    if (pAddrInfo != NULL) freeaddrinfo(pAddrInfo);
#endif

    if (sock == INVALID_SOCKET || sock == SOCKET_ERROR)
        sock = (SOCKET)SOCKET_ERROR;
    return sock;
}

// --------------------------------------------------------------------
SOCKET AcceptConnection(
    SOCKET ListeningSockArray[],
    INT nListeningSockCount,
    HANDLE hStopEvent,
    DWORD dwTimeoutMs,
    DWORD* pdwWSAError)
//
// Returns:
//  If new connection is accepted, returns the socket
//  If timeout expires before someone connects, returns INVALID_SOCKET
//  If error occurs, returns SOCKET_ERROR
//
// --------------------------------------------------------------------
{
    SOCKET accept_sock = INVALID_SOCKET;

    Log(DEBUG_MSG, TEXT("+AcceptConnection"));
    
#ifdef SUPPORT_IPV6
    INT i;
    DWORD dwRet;
    HANDLE* EventArray = NULL;

    if (nListeningSockCount >= MAXIMUM_WAIT_OBJECTS-1) {
        // reserve one listening object for the hStopEvent
        goto Cleanup;
    }
    EventArray = new HANDLE[nListeningSockCount + 1];
    if (EventArray == NULL) goto Cleanup;

    for (i=0; i<nListeningSockCount; i+=1) EventArray[i] = NULL;
    for (i=0; i<nListeningSockCount; i+=1) {
        EventArray[i] = WSACreateEvent();
        if (EventArray[i] == NULL) goto Cleanup;
    }
    EventArray[i] = hStopEvent;

    for (i = 0; i < nListeningSockCount; i += 1)
    {
        if (SOCKET_ERROR == WSAEventSelect(
            ListeningSockArray[i],  // server socket
            EventArray[i],          // event to be generated
            FD_ACCEPT))             // which event to wait on
        {
            *pdwWSAError = WSAGetLastError();
            Log(ERROR_MSG, TEXT("WSAEventSelect() failed on socket number %d, WSAError: %ld"),
                i, *pdwWSAError);
            accept_sock = (SOCKET)SOCKET_ERROR;
            goto Cleanup;
        }
    }

    // check if we were signalled
    //
    dwRet = WaitForMultipleObjects(
        nListeningSockCount + (hStopEvent != NULL ? 1 : 0),   // number of events (to wait on)
        EventArray,             // the event array
        FALSE,                  // just wait for any of the events to go off
        dwTimeoutMs);           // length of time

    if (dwRet >= WAIT_OBJECT_0 && dwRet < WAIT_OBJECT_0 + (DWORD)nListeningSockCount)
    {
        // The event number that went off (which is the same number as the socket number)
        INT nEventNum = dwRet - WAIT_OBJECT_0;

        Log(DEBUG_MSG, TEXT("Connection ready to be accepted on socket %d..."), nEventNum);

        // Reset listening socket back to blocking mode
        if (SOCKET_ERROR == WSAEventSelect(
            ListeningSockArray[nEventNum],
            EventArray[nEventNum], 0))
        {
            *pdwWSAError = WSAGetLastError();
            Log(ERROR_MSG, TEXT("WSAEventSelect() failed, WSAError: %ld"), *pdwWSAError);
            accept_sock = (SOCKET)SOCKET_ERROR;
            goto Cleanup;
        }
        
        dwRet = 0;
        if (SOCKET_ERROR == ioctlsocket(
            ListeningSockArray[nEventNum],
            FIONBIO, (unsigned long*)&dwRet))
        {
            Log(ERROR_MSG, TEXT("ioctlsocket() failed, WSA Error: %ld"), WSAGetLastError());
            goto Cleanup;
        }
        Log(DEBUG_MSG, TEXT("We have a connection ready to be accepted on socket %d..."),
            ListeningSockArray[nEventNum]);

        SOCKADDR_STORAGE addr;          // incoming address
        INT addrlen = sizeof(addr);
        
        accept_sock = accept(ListeningSockArray[nEventNum], (LPSOCKADDR)&addr, &addrlen);
        if (accept_sock == SOCKET_ERROR)
        {
            *pdwWSAError = WSAGetLastError();
            Log(WARNING_MSG, TEXT("accept() failed, WSA Error: %ld"), *pdwWSAError);
            goto Cleanup;
        }

        // create a thread to handle the client
        //
        Log(DEBUG_MSG, TEXT("Accepted client request"));
    }
    else if (dwRet == WAIT_OBJECT_0 + (DWORD)nListeningSockCount)
    {
        Log(DEBUG_MSG, TEXT("Received stop event"));
    }
    else if (dwRet == WAIT_TIMEOUT)
    {
        Log(WARNING_MSG, TEXT("Timeout expired"));
    }
    else
    {
        Log(ERROR_MSG, TEXT("WaitForMultipleObjects() returned unexpected value"));
    }

#else

    SOCKADDR addr;                  // incoming address
    INT addrlen = sizeof(addr);
    
    accept_sock = accept(ListeningSockArray[0], (LPSOCKADDR)&addr, &addrlen);
    if (accept_sock == SOCKET_ERROR)
    {
        *pdwWSAError = WSAGetLastError();
        Log(WARNING_MSG, TEXT("accept() failed, WSA Error: %ld"), *pdwWSAError);
        goto Cleanup;
    }
    Log(DEBUG_MSG, TEXT("Accepted client request"));
    
#endif

Cleanup:

#ifdef SUPPORT_IPV6
    if (EventArray != NULL) {
        for (i=0; i<nListeningSockCount; i+=1)
            if (EventArray[i] != NULL) CloseHandle(EventArray[i]);
        delete [] EventArray;
        // Note: Not deallocating last element containing hStopEvent
    }
#endif

    Log(DEBUG_MSG, TEXT("-AcceptConnection"));

    return accept_sock;
}

void DisconnectSocket(SOCKET sock)
{
    Log(DEBUG_MSG, TEXT("+DisconnectSocket()"));

    if (sock == SOCKET_ERROR || sock == INVALID_SOCKET)
    {
        Log(DEBUG_MSG, TEXT("DisconnectSocket called with invalid socket."));
    }
    else
    {
#ifdef SUPPORT_IPV6
        shutdown(sock, SD_BOTH);
#else
        shutdown(sock, 0x2);
#endif
        closesocket(sock);
    }

    Log(DEBUG_MSG, TEXT("-DisconnectSocket()"));
}

// --------------------------------------------------------------------
BOOL ApplySocketSettings(
    SOCKET sock,
    INT sock_type,
    BOOL keepalive_on,  // TRUE will turn on KEEPALIVE option, only SOCK_STREAM
    BOOL nagle_off,     // if FALSE will leave at default
    INT send_buf,       // if < 0 will leave at default
    INT recv_buf,       // if < 0 will leave at default
    DWORD* pdwWSAError)
//
// Returns:
//  TRUE if all settings applied as desired
//  FALSE if at least one of the settings failed to be applied
//
// --------------------------------------------------------------------
{
    if (sock_type == SOCK_STREAM)
    {
        if (nagle_off == TRUE)
        {
            if (SOCKET_ERROR == setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (CHAR*)&nagle_off, sizeof(BOOL)))
            {
                *pdwWSAError = WSAGetLastError();
                Log(ERROR_MSG, TEXT("setsockopt(TCP_NODELAY) failed, WSA Error: %ld"), *pdwWSAError);
                return FALSE;
            }
        }
        if (keepalive_on == TRUE)
        {
            BOOL bKeepAlive = TRUE;
            if (SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (LPSTR)&bKeepAlive, sizeof(bKeepAlive)))
            {
                *pdwWSAError = WSAGetLastError();
                Log(ERROR_MSG, TEXT("setsockopt(SO_KEEPALIVE) failed, WSA Error: %ld"), *pdwWSAError);
                return FALSE;
            }
        }
#ifndef UNDER_CE
        if (send_buf >= 0)
        {
            if (SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (CHAR*)&send_buf, sizeof(INT)))
            {
                *pdwWSAError = WSAGetLastError();
                Log(ERROR_MSG, TEXT("setsockopt(SO_SNDBUF) failed, WSA Error: %ld"), *pdwWSAError);
                return FALSE;
            }
        }
        if (recv_buf >= 0)
        {
            if (SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (CHAR*)&recv_buf, sizeof(INT)))
            {
                *pdwWSAError = WSAGetLastError();
                Log(ERROR_MSG, TEXT("setsockopt(SO_RCVBUF) failed, WSA Error: %ld"), *pdwWSAError);
                return FALSE;
            }
        }
#endif
    }
    else if (sock_type == SOCK_DGRAM)
    {
#ifndef UNDER_CE
        if (send_buf >= 0)
            if (SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (CHAR*)&send_buf, sizeof(INT)))
            {
                *pdwWSAError = WSAGetLastError();
                Log(ERROR_MSG, TEXT("setsockopt(SO_SNDBUF) failed to disable Nagle, WSA Error: %ld"), *pdwWSAError);
                return FALSE;
            }
#endif
        if (recv_buf >= 0)
            if (SOCKET_ERROR == setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (CHAR*)&recv_buf, sizeof(INT)))
            {
                *pdwWSAError = WSAGetLastError();
                Log(ERROR_MSG, TEXT("setsockopt(SO_RCVBUF) failed to disable Nagle, WSA Error: %ld"), *pdwWSAError);
                return FALSE;
            }
    }
    
    return TRUE;
}

