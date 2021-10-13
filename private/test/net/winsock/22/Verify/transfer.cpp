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
#include "ws2bvt.h"
#include <strsafe.h>

static HANDLE g_hEndThread              = NULL;
static HANDLE g_ServerThreadReady       = NULL;

#define WAIT_FOR_SERVER_THREAD_MS       INFINITE
#define PORT_SZ                         "18765"     // Port to communicate over
#define CLIENT_SEND_DATA                'C'
#define SERVER_RECV_DATA                CLIENT_SEND_DATA                
#define SERVER_SEND_DATA                'S'
#define CLIENT_RECV_DATA                SERVER_SEND_DATA
#define HOST_NAME_STR_SIZE              32
#define CLIENT_THREAD                   "ClientThread"
#define SERVER_THREAD                   "ServerThread"
#define ONE_SOCKET_HANDLE_READY         1

typedef struct __THREAD_PARAMS__ {
    int nFamily;
    int nSocketType;
    int nProtocol;
    int nSockets;
} THREAD_PARAMS;

struct in_addr IPV4_LOOPBACKADDR = {127, 0, 0, 1};

struct in6_addr IPV6_LOOPBACKADDR = {   
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x1 };

struct in6_addr IPV6_COMPAT =   {    
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x0,
    0x0, 0x0 }; // All IPv4 Compatible addresses begin with 96 bits of zeroes

// Function Prototypes
static DWORD WINAPI ServerThread(LPVOID *pParm);

static void         PrintFailure(SOCKADDR *psaAddr);

//**********************************************************************************

void PrintFailure(char * SourceString, SOCKADDR *psaAddr)
{
    if(psaAddr != NULL)
    {
        if(AF_INET == psaAddr->sa_family)
        {
            if (SourceString != NULL)
            {
                Log(FAIL, TEXT("%s: Could not communicate with address: 0x%08x"), SourceString, ntohl(((SOCKADDR_IN *)psaAddr)->sin_addr.s_addr));
            }
            else
            {
                Log(FAIL, TEXT("Could not communicate with address: 0x%08x"), ntohl(((SOCKADDR_IN *)psaAddr)->sin_addr.s_addr));
            }
        }
        else
        {
            PrintIPv6Addr(FAIL, (SOCKADDR_IN6 *)psaAddr);
        }
    }
}

TESTPROCAPI TransferTest (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) 
{
    /* Local variable declarations */
    WSAData             wsaData = {0};
    SOCKADDR_STORAGE    ssRemoteAddr = {0};
    int                 nError = 0, cbSent = 0, cbRecvd = 0, cbRemoteAddr = 0;
    char                szName[HOST_NAME_STR_SIZE];
    THREAD_PARAMS       Params = {0};
    ADDRINFO            Hints = {0}, *pAddrInfo = NULL, *pAI = NULL;
    BOOL                fNonLoopbackFound = 0;
    HANDLE              hServerThread = NULL;
    DWORD               dwThreadId = 0;
    SOCKET              sock = INVALID_SOCKET;
    char                byData = CLIENT_SEND_DATA;
    fd_set              fdReadSet = {0};
    int                 RetSelect=0;
    
    // Check our message value to see why we have been called
    if (TPM_QUERY_THREAD_COUNT == uMsg) 
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0 /* DEFAULT_THREAD_COUNT */;
        return TPR_HANDLED;
    } 
    else if (TPM_EXECUTE != uMsg) 
    {
        return TPR_NOT_HANDLED;
    }

    /* Initialize variable for this test */
    switch(lpFTE->dwUserData)
    {
    case TRANS_TCP_IPV6:
        Params.nFamily = AF_INET6;
        break;
    case TRANS_TCP_IPV4:
        Params.nFamily = AF_INET;
        break;
    default:
        Params.nFamily = AF_UNSPEC;
        break;
    }
    Params.nSocketType = SOCK_STREAM;
    Params.nProtocol = 0;
    Params.nSockets = 0;

    nError = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (nError) 
    {
        Log(FAIL, TEXT("WSAStartup Failed: %d"), nError);
        Log(FAIL, TEXT("Fatal Error: Cannot continue test!"));
        return TPR_FAIL;
    }

    // Get the local machine name
    if(gethostname(szName, sizeof(szName)) == SOCKET_ERROR)
    {
        Log(FAIL, TEXT("gethostname() failed with error: %d"), WSAGetLastError());
        Log(FAIL, TEXT("Could not get the local machine name"));
        goto Cleanup;
    }

    memset(&Hints, 0, sizeof(Hints));
    Hints.ai_family = Params.nFamily;
    Hints.ai_socktype = Params.nSocketType;

    // Use the local machine name to get a list of all the local addresses
    nError = getaddrinfo(szName, PORT_SZ, &Hints, &pAddrInfo);
    if(nError == WSANO_DATA)
    {
        Log(SKIP, TEXT("Could not get a %s address for this test"), GetStackName(Params.nFamily));
        Log(SKIP, TEXT("This usually occurs when either there is no network or network card present."));
        if(Params.nFamily == PF_INET6)
            Log(SKIP, TEXT("Or if you do not have a v6 enabled router on any network to which the device is connected."));
        goto Cleanup;
    }
    else if(nError)
    {
        Log(FAIL, TEXT("getaddrinfo(%hs:%hs) failed with error: %d"), szName, PORT_SZ, nError);
        Log(FAIL, TEXT("Could not get local machine's addresses"));
        goto Cleanup;
    }

    // Check to see if machine has any network adapters for this family
    fNonLoopbackFound = FALSE;
    for(pAI = pAddrInfo; pAI != NULL && !fNonLoopbackFound; pAI = pAI->ai_next)
    {
        if(pAI->ai_family == AF_INET)
        {
            if(memcmp(&(((SOCKADDR_IN *)pAI->ai_addr)->sin_addr), &IPV4_LOOPBACKADDR, sizeof(IPV4_LOOPBACKADDR)))
                fNonLoopbackFound = TRUE;
        }
        else if(pAI->ai_family == AF_INET6)
        {
            if(memcmp(&(((SOCKADDR_IN6 *)pAI->ai_addr)->sin6_addr), &IPV6_LOOPBACKADDR, sizeof(IPV6_LOOPBACKADDR)))
                fNonLoopbackFound = TRUE;
        }
    }
    //Count of host addresses. Send this to the Server Thread,
    //helps keep synchronization between Client & Server thread simple
    for(pAI = pAddrInfo; pAI != NULL; pAI = pAI->ai_next)
    {
        Params.nSockets++;
    }
    
    if(!fNonLoopbackFound)
    {
        Log(SKIP, TEXT("No local %s addresses could be found!"), 
            GetStackName(Params.nFamily));
        Log(SKIP, TEXT("This usually occurs when either there is no network or network card present."));
        if(Params.nFamily == PF_INET6)
            Log(SKIP, TEXT("Or if you do not have a v6 enabled router on any network to which the device is connected."));
        Log(SKIP, TEXT("Skipping this transfer test..."));
        goto Cleanup;
    }

    // Create an event to signal server thread to exit
    g_hEndThread = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(g_hEndThread == NULL)
    {
        Log(ABORT, TEXT("CreateEvent() for g_hEndThread failed with error %d"), GetLastError());
        Log(ABORT, TEXT("Cannot continue test..."));
        goto Cleanup;
    }

    // Create an event to signal server thread is ready to accept data
    g_ServerThreadReady = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(g_ServerThreadReady == NULL)
    {
        Log(ABORT, TEXT("CreateEvent() for g_ServerThreadReady failed with error %d"), GetLastError());
        Log(ABORT, TEXT("Cannot continue test..."));
        goto Cleanup;
    }

    // Start a server thread
    if ((hServerThread = 
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ServerThread,
        &Params, 0, &dwThreadId)) == NULL)
    {
        Log(ABORT, TEXT("CreateThread() failed with error %d"), 
            GetLastError());
        Log(ABORT, TEXT("Cannot continue test..."));
        goto Cleanup;
    }

    Log(ECHO, TEXT("%d: ClientThread: Server Thread create() complete..."),GetTickCount());
    // For each address connect to the server thread and send it a byte
    for(pAI = pAddrInfo; pAI != NULL; pAI = pAI->ai_next)
    {
        // Make sure address is not IPv4 Compatible (Not supported in CE)
        if(pAI->ai_family == AF_INET6 && 
            memcmp(&(((SOCKADDR_IN6 *)pAI->ai_addr)->sin6_addr), &IPV6_LOOPBACKADDR, sizeof(IPV6_LOOPBACKADDR)) &&
            memcmp(&(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr), &IPV6_COMPAT, sizeof(IPV6_COMPAT)) == 0)
        {
            Log(DETAIL, TEXT("Windows CE does not support connecting to IPv4 Compatible addresses"));
            Log(DETAIL, TEXT("Skipping: "));
            PrintIPv6Addr(DETAIL, (SOCKADDR_IN6 *)(pAI->ai_addr));
            continue;
        }

       // Create a socket
        sock = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
        if(sock == INVALID_SOCKET)
        {
            Log(FAIL, TEXT("socket(%s, %s, %s) failed with error %d"), 
                GetFamilyName(pAI->ai_family), GetTypeName(pAI->ai_socktype),
                GetProtocolName(pAI->ai_protocol), WSAGetLastError());
            Log(FAIL, TEXT("Cannot continue test..."));
            goto Cleanup;
        }

        // Connect to the address
        if(Params.nSocketType == SOCK_STREAM)
        {
            if(connect(sock, pAI->ai_addr, pAI->ai_addrlen))
            {
                Log(FAIL, TEXT("%d: ClientThread: connect() failed with error %d"), GetTickCount(), WSAGetLastError());
                PrintFailure(CLIENT_THREAD, pAI->ai_addr);
                goto CloseAndContinue;
            }
            Log(ECHO, TEXT("%d: ClientThread: connect() complete..."),GetTickCount());
        }

        // send 1 byte
        byData = CLIENT_SEND_DATA;
        cbSent = sendto(sock, &byData, sizeof(byData), 0, pAI->ai_addr, pAI->ai_addrlen);
        if(cbSent != sizeof(byData))
        {
            Log(FAIL, TEXT("%d: ClientThread: sendto() failed to send %d bytes with error %d"), 
               GetTickCount(), sizeof(byData), WSAGetLastError());
            PrintFailure(CLIENT_THREAD, pAI->ai_addr);
            goto CloseAndContinue;
        }
        Log(ECHO, TEXT("%d: ClientThread: sendto() complete..."),GetTickCount());

        // recv 1 byte
        FD_ZERO(&fdReadSet);
        FD_SET(sock, &fdReadSet);
        //Wait till the read socket is ready.
        //Replaced timeouts with infinite wait since 
        //the intention is to check send() & recv() functionality
        //not how fast it is. Moreover having a timeout can cause a failure
        //if there is another task hogging CPU
        RetSelect = select(0, &fdReadSet, NULL, NULL, NULL);
        if(ONE_SOCKET_HANDLE_READY != RetSelect)
        {
            Log(FAIL,  TEXT("%d: ClientThread: Timed out waiting for data... %d, select returned %d, sock = %d"),GetTickCount(),WSAGetLastError(),RetSelect, sock);
            PrintFailure(CLIENT_THREAD, pAI->ai_addr);
            goto CloseAndContinue;
        }
        Log(ECHO, TEXT("%d: ClientThread: select() complete..., sock = %d"),GetTickCount(), sock);
        
        cbRemoteAddr = sizeof(ssRemoteAddr);
        cbRecvd = recvfrom(sock, &byData, sizeof(byData), 0, (SOCKADDR *)&ssRemoteAddr, &cbRemoteAddr);
        //Make sure data received by client is the one sent by the server
        if((cbRecvd != sizeof(byData)) || (byData != CLIENT_RECV_DATA))
        {
            Log(FAIL, TEXT("%d: ClientThread recvfrom() failed to receive %d bytes with error %d"), 
                GetTickCount(),sizeof(byData), WSAGetLastError());
            PrintFailure(CLIENT_THREAD, pAI->ai_addr);
            goto CloseAndContinue;
        }
        Log(ECHO, TEXT("%d: ClientThread: recvfrom() complete..."),GetTickCount());

CloseAndContinue:
        // close the connection
        shutdown(sock, SD_BOTH);
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

Cleanup:
    if(sock != INVALID_SOCKET)
        closesocket(sock);
    // Signal Server Thread to terminate
    if(g_hEndThread)
        SetEvent(g_hEndThread);
    if(hServerThread)
    {
        Log(ECHO, TEXT("%d: ClientThread: Waiting for ServerThread to exit..."),GetTickCount());
        //Wait for server thread to complete. 
        //Do not use timeouts as the test case does not intend to check the OS performance
        WaitForSingleObject(hServerThread, WAIT_FOR_SERVER_THREAD_MS);
        CloseHandle(hServerThread);
    }
    if(g_hEndThread)
        CloseHandle(g_hEndThread);
    if(g_ServerThreadReady)
        CloseHandle(g_ServerThreadReady);
    /* clean up */
    WSACleanup();

    Log(ECHO, TEXT("%d: ClientThread complete..."),GetTickCount());
    
    return getCode();
}

//**********************************************************************************

DWORD WINAPI ServerThread(LPVOID *pParm)
{
    THREAD_PARAMS       *pParams = (THREAD_PARAMS *)pParm;
    SOCKET              sock = INVALID_SOCKET, ServiceSocket[FD_SETSIZE];
    SOCKADDR_STORAGE    ssRemoteAddr = {0};
    int                 cbRemoteAddr = sizeof(ssRemoteAddr), nNumberOfSockets = 0;
    fd_set              fdReadSet = {0}, fdSockSet = {0};
    DWORD               cbRecvd = 0, cbSent = 0;
    ADDRINFO            AddrHints = {0}, *pAddrInfo = NULL, *pAI = NULL;
    char                byData = SERVER_SEND_DATA;
    BOOL                fExit = FALSE;
    WSAData             wsaData = {0};
    int                 RetSelect = 0;
    int                 Index = 0, i = 0;
    
    WSAStartup(MAKEWORD(2,2), &wsaData);

    memset(&AddrHints, 0, sizeof(AddrHints));
    AddrHints.ai_family = pParams->nFamily;
    AddrHints.ai_socktype = pParams->nSocketType;
    AddrHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

    if(getaddrinfo(NULL, PORT_SZ, &AddrHints, &pAddrInfo))
    {
        Log(ABORT,  TEXT("ServerThread: getaddrinfo() error %d"), WSAGetLastError());
        Log(ABORT,  TEXT("ServerThread: Could not get any local addresses to serve on"));
        WSACleanup();
        return 0;
    }

    for(pAI = pAddrInfo, i = 0; pAI != NULL; pAI = pAI->ai_next) 
    {
        if((pAI->ai_family == PF_INET) || (pAI->ai_family == PF_INET6)) // support only PF_INET and PF_INET6.
        {
            if (i == FD_SETSIZE) 
            {
                Log(DETAIL,  TEXT("ServerThread: getaddrinfo returned more supported addresses than we could use."));
                break;
            }
            ServiceSocket[i] = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
            if (ServiceSocket[i] == INVALID_SOCKET)
            {
                Log(ECHO, TEXT("ServerThread: socket(%s, %s, %s) failed with error %d"), 
                    GetFamilyName(pAI->ai_family), GetTypeName(pAI->ai_socktype),
                    GetProtocolName(pAI->ai_protocol), WSAGetLastError());
                continue;
            }

            
            if (bind(ServiceSocket[i], pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
            {
                Log(ECHO, TEXT("ServerThread: bind() failed, error %d"), WSAGetLastError());
                closesocket(ServiceSocket[i]);
                continue;
            }
            
            if(pParams->nSocketType == SOCK_STREAM)
            {
                if (listen(ServiceSocket[i], 5) == SOCKET_ERROR)
                {
                    Log(ECHO, TEXT("ServerThread: listen() failed, error %d"), WSAGetLastError());
                    closesocket(ServiceSocket[i]);
                    continue;
                }
                
                Log(ECHO, TEXT("ServerThread: Listening on port %hs, using family %s"), PORT_SZ, GetFamilyName(pAI->ai_family));
            }

            i++;
        }
    }
    
    freeaddrinfo(pAddrInfo);

    nNumberOfSockets = i;
    
    if (0 == nNumberOfSockets) 
    {
        Log(FAIL, TEXT("ServerThread: unable to serve on any address, ABORTING"));
        fExit = TRUE;
        goto exitThread;
    }

    Log(ECHO, TEXT("ServerThread: Service %d sockets"), pParams->nSockets);

    //Exit after servicing the number of sockets (pParams->nSockets) informed by the client
    //This makes the exit condition simpler compared to waiting on sockets with timeout
    while(Index < pParams->nSockets)    
    {
        sock = INVALID_SOCKET;
        Index++;
        if(WaitForSingleObject(g_hEndThread, 0) == WAIT_OBJECT_0)
        {
            fExit = TRUE;
            Log(ECHO, TEXT("%d: ServerThread Exit Condition met..."),GetTickCount());
            goto exitThread;        // We've been signalled to exit
        }

        Log(ECHO, TEXT("%d: ServerThread Servicing Client Socket %d..."),GetTickCount(), Index);
        
        FD_ZERO(&fdSockSet);

        for (i = 0; i < nNumberOfSockets; i++)    // want to check all available sockets
            FD_SET(ServiceSocket[i], &fdSockSet);

        //Wait till the read socket is ready.
        //Replaced timeouts with infinite wait since 
        //the intention is to check send() & recv() functionality
        //not how fast it is. Moreover having a timeout can cause a failure
        //if there is another task hogging CPU
        RetSelect = select(nNumberOfSockets, &fdSockSet, 0, 0, NULL) ;
        if (SOCKET_ERROR == RetSelect)
        {
            Log(FAIL, TEXT("%d: ServerThread: select() failed with error %d, select returnd %d"),GetTickCount(), WSAGetLastError(), RetSelect);
            fExit = TRUE;
            goto exitThread;  // An error here means one of our sockets is bad, no way to recover
        }

        Log(ECHO, TEXT("%d: ServerThread select1() complete..."),GetTickCount());
        
        for (i = 0; i < nNumberOfSockets; i++)    // check which socket is ready to process
        {
            if (FD_ISSET(ServiceSocket[i], &fdSockSet))    // proceed for connected socket
            {
                FD_CLR(ServiceSocket[i], &fdSockSet);
                if(pParams->nSocketType == SOCK_STREAM)
                {
                    Log(ECHO, TEXT("%d: ServerThread accept()..."),GetTickCount());
                    cbRemoteAddr = sizeof(ssRemoteAddr);
                    sock = accept(ServiceSocket[i], (SOCKADDR*)&ssRemoteAddr, &cbRemoteAddr);
                    if(sock == INVALID_SOCKET) 
                    {
                        Log(FAIL, TEXT("ServerThread: accept() failed with error %d"), WSAGetLastError());
                        goto exitThread;
                    }
                    
                    char szClientNameASCII[256];
                    Log(ECHO, TEXT("%d: ServerThread getnameinfo()..."),GetTickCount());
                    if (getnameinfo((SOCKADDR *)&ssRemoteAddr, cbRemoteAddr,
                        szClientNameASCII, sizeof(szClientNameASCII), NULL, 0, NI_NUMERICHOST) != 0)
                        StringCchCopyA(szClientNameASCII, 256, "<<getnameinfo error>>");
                    Log(ECHO, TEXT("ServerThread: Accepted connection from client %hs, sock = %d"), szClientNameASCII, sock);
                }
                else
                    sock = ServiceSocket[i];
                break;
            }
        }
        Log(ECHO, TEXT("%d: ServerThread Done accepting connections..."),GetTickCount());
    
        // recv 1 byte
        FD_ZERO(&fdReadSet);
        FD_SET(sock, &fdReadSet);

        //Wait till the read socket is ready.
        //Replaced timeouts with infinite wait since 
        //the intention is to check send() & recv() functionality
        //not how fast it is. Moreover having a timeout can cause a failure
        //if there is another task hogging CPU
        RetSelect = select(0, &fdReadSet, NULL, NULL, NULL);
        if(ONE_SOCKET_HANDLE_READY != RetSelect)
        {
            Log(FAIL,  TEXT("%d: ServerThread: Timed out waiting for data %d, Select returned %d, sock = %d"),GetTickCount(), WSAGetLastError(),RetSelect,sock) ;
        }
        else
        {
            Log(ECHO, TEXT("%d: ServerThread Select2() complete..."),GetTickCount());
            
            cbRemoteAddr = sizeof(ssRemoteAddr);

            cbRecvd = recvfrom(sock, &byData, sizeof(byData), 0, (SOCKADDR *)&ssRemoteAddr, &cbRemoteAddr);
            //Make sure data received by server is the one sent by the client
            if(cbRecvd != sizeof(byData) || (byData != SERVER_RECV_DATA))
            {
                Log(FAIL, TEXT("ServerThread: recvfrom() failed to receive %d bytes with error %d"), 
                    sizeof(byData), WSAGetLastError());
            }
            else
            {
                Log(ECHO, TEXT("%d: ServerThread recv() complete..."),GetTickCount());

                // send 1 byte
                byData = SERVER_SEND_DATA;
                cbSent = sendto(sock, &byData, sizeof(byData), 0, (SOCKADDR *)&ssRemoteAddr, cbRemoteAddr);
                if(cbSent != sizeof(byData))
                {
                    Log(FAIL, TEXT("ServerThread: sendto() failed to send %d bytes with error %d"), 
                        sizeof(byData), WSAGetLastError());
                }
                else
                {
                    Log(ECHO, TEXT("%d: ServerThread sendto complete..."),GetTickCount());
                }
            }
        }
        // Do not close sock if UDP since that is also our ServiceSocket.
        if(pParams->nSocketType == SOCK_STREAM && sock != INVALID_SOCKET)
        {
            shutdown(sock, SD_BOTH);
            closesocket(sock);
            Log(ECHO, TEXT("%d: ServerThread close()..."),GetTickCount());
        }
    }
exitThread:
    //Wait for the client thread to inform it is done processing all sockets
    if(WaitForSingleObject(g_hEndThread, INFINITE) == WAIT_OBJECT_0)
    {
        Log(ECHO, TEXT("%d: ServerThread Exit Condition met..."),GetTickCount());
    }

    for( i = 0; i < nNumberOfSockets; i++)
    {
        shutdown(ServiceSocket[i], SD_BOTH);
        closesocket(ServiceSocket[i]);
    }
    WSACleanup();

    Log(ECHO, TEXT("%d: ServerThread complete..."),GetTickCount());
    
    return 1;
}


