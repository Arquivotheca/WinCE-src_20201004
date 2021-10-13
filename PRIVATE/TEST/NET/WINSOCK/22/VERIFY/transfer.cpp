//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "ws2bvt.h"

HANDLE g_hEndThread = NULL;

#define WAIT_SECS			1			// Amount of time to wait for a connection before server
#define WAIT_USECS			0			// checks whether or not it should close
#define WAIT_DATA_SECS		0			// Amount of time to wait for incoming data
#define WAIT_DATA_USECS		500000

#define PORT_SZ				"18765"		// Port to communicate over

typedef struct __THREAD_PARAMS__ {
	int nFamily;
	int nSocketType;
	int nProtocol;
} THREAD_PARAMS;

struct in_addr IPV4_LOOPBACKADDR = {127, 0, 0, 1};

struct in6_addr IPV6_LOOPBACKADDR =	{	0x0, 0x0,
										0x0, 0x0,
										0x0, 0x0,
										0x0, 0x0,
										0x0, 0x0,
										0x0, 0x0,
										0x0, 0x0,
										0x0, 0x1 };

struct in6_addr IPV6_COMPAT =		{	0x0, 0x0,
										0x0, 0x0,
										0x0, 0x0,
										0x0, 0x0,
										0x0, 0x0,
										0x0, 0x0 };		// All IPv4 Compatible addresses begin
														// with 96 bits of zeroes

// Function Prototypes
DWORD WINAPI ServerThread(LPVOID *pParm);
void PrintFailure(SOCKADDR *psaAddr);

TESTPROCAPI TransferTest (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	
	/* Local variable declarations */
	//DWORD				dwStatus = TPR_PASS;
	WSAData				wsaData;
	SOCKADDR_STORAGE	ssRemoteAddr;
	int					nError, cbSent, cbRecvd, cbRemoteAddr;
	char				szName[32];
	THREAD_PARAMS		Params;
	ADDRINFO			Hints, *pAddrInfo = NULL, *pAI;
	BOOL				fNonLoopbackFound;
	TIMEVAL				tv_data = {WAIT_DATA_SECS, WAIT_DATA_USECS};	// time to wait for data
	HANDLE				hServerThread = NULL;
	DWORD				dwThreadId;
	SOCKET				sock = INVALID_SOCKET;
	char				byData;
	fd_set				fdReadSet;
	
    // Check our message value to see why we have been called
    if (uMsg == TPM_QUERY_THREAD_COUNT) {
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0 /* DEFAULT_THREAD_COUNT */;
		return TPR_HANDLED;
    } 
	else if (uMsg != TPM_EXECUTE) {
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
		//dwStatus = TPR_FAIL;
		goto Cleanup;
	}

	memset(&Hints, 0, sizeof(Hints));
	Hints.ai_family = Params.nFamily;
	Hints.ai_socktype = Params.nSocketType;

	// Use the local machine name to get a list of all the local addresses
	nError = getaddrinfo(szName, PORT_SZ, &Hints, &pAddrInfo);
	if(nError)
	{
		Log(FAIL, TEXT("getaddrinfo(%hs:%hs) failed with error: %d"), szName, PORT_SZ, nError);
		Log(FAIL, TEXT("Could not get local machine's addresses"));
		//dwStatus = TPR_FAIL;
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

	if(!fNonLoopbackFound)
	{
		Log(ABORT, TEXT("No local %s addresses could be found!"), 
			GetStackName(Params.nFamily));
		Log(ABORT, TEXT("Transfer test aborting..."));
		//dwStatus = TPR_ABORT;
		goto Cleanup;
	}

	// Create an event to signal server thread to exit
	g_hEndThread = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(g_hEndThread == NULL)
	{
		Log(ABORT, TEXT("CreateEvent() failed with error %d"), GetLastError());
		Log(ABORT, TEXT("Cannot continue test..."));
		//dwStatus = TPR_ABORT;
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
		//dwStatus = TPR_ABORT;
		goto Cleanup;
	}

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
			//dwStatus = TPR_FAIL;
			goto Cleanup;
		}

		// Connect to the address
		if(Params.nSocketType == SOCK_STREAM)
		{
			if(connect(sock, pAI->ai_addr, pAI->ai_addrlen))
			{
				Log(FAIL, TEXT("connect() failed with error %d"), WSAGetLastError());
				PrintFailure(pAI->ai_addr);
				//dwStatus = TPR_FAIL;
				goto CloseAndContinue;
			}
		}

		// send 1 byte
		cbSent = sendto(sock, &byData, sizeof(byData), 0, pAI->ai_addr, pAI->ai_addrlen);
		if(cbSent != sizeof(byData))
		{
			Log(FAIL, TEXT("sendto() failed to send %d bytes with error %d"), 
				sizeof(byData), WSAGetLastError());
			PrintFailure(pAI->ai_addr);
			//dwStatus = TPR_FAIL;
			goto CloseAndContinue;
		}

		// recv 1 byte
		FD_ZERO(&fdReadSet);
		FD_SET(sock, &fdReadSet);
		if(select(0, &fdReadSet, NULL, NULL, &tv_data) != 1)
		{
			Log(FAIL,  TEXT("Timed out waiting for data..."));
			PrintFailure(pAI->ai_addr);
			//dwStatus = TPR_FAIL;
			goto CloseAndContinue;
		}

		cbRemoteAddr = sizeof(ssRemoteAddr);
		cbRecvd = recvfrom(sock, &byData, sizeof(byData), 0, (SOCKADDR *)&ssRemoteAddr, &cbRemoteAddr);
		if(cbRecvd != sizeof(byData))
		{
			Log(FAIL, TEXT("recvfrom() failed to receive %d bytes with error %d"), 
				sizeof(byData), WSAGetLastError());
			PrintFailure(pAI->ai_addr);
			//dwStatus = TPR_FAIL;
			goto CloseAndContinue;
		}

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
	{
		SetEvent(g_hEndThread);
		CloseHandle(g_hEndThread);
	}

	if(hServerThread)
		CloseHandle(hServerThread);
	
	/* clean up */
	WSACleanup();
		
	/* End */
	return getCode();
}

//**********************************************************************************

void PrintFailure(SOCKADDR *psaAddr)
{
	Log(FAIL, TEXT("Could not communicate with address: "));
	if(psaAddr->sa_family == AF_INET)
	{
		Log(FAIL, TEXT("   0x%08x"), ntohl(((SOCKADDR_IN *)psaAddr)->sin_addr.s_addr));
	}
	else
		PrintIPv6Addr(FAIL, (SOCKADDR_IN6 *)psaAddr);
}

//**********************************************************************************

DWORD WINAPI ServerThread(LPVOID *pParm)
{
	THREAD_PARAMS	*pParams = (THREAD_PARAMS *)pParm;
	SOCKET			sock = INVALID_SOCKET, ServiceSocket[FD_SETSIZE];
	SOCKADDR_STORAGE ssRemoteAddr;
	int				cbRemoteAddr = sizeof(ssRemoteAddr), nNumberOfSockets = 0, i;
	fd_set			fdReadSet, fdSockSet;
	DWORD			cbRecvd, cbSent;
	ADDRINFO		AddrHints, *pAddrInfo = NULL, *pAI;
	TIMEVAL			tv_limit = {WAIT_SECS, WAIT_USECS};	// wait for connection in timeout intervals
	TIMEVAL			tv_data = {WAIT_DATA_SECS, WAIT_DATA_USECS};	// time to wait for data
	char			byData;
	BOOL			fExit = FALSE;
	WSAData			wsaData;

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
	
	if (nNumberOfSockets == 0) 
	{
		Log(FAIL, TEXT("ServerThread: unable to serve on any address, ABORTING"));
		fExit = TRUE;
		goto exitThread;
	}

ServiceClient:

	sock = INVALID_SOCKET;
	
	// serve until connection is formed or until EndThread event is signalled
	while(sock == INVALID_SOCKET)	
	{
		if(WaitForSingleObject(g_hEndThread, 0) == WAIT_OBJECT_0)
		{
			fExit = TRUE;
			goto exitThread;		// We've been signalled to exit
		}

		FD_ZERO(&fdSockSet);

		for (i = 0; i < nNumberOfSockets; i++)	// want to check all available sockets
			FD_SET(ServiceSocket[i], &fdSockSet);
		
		if (select(nNumberOfSockets, &fdSockSet, 0, 0, &tv_limit) == SOCKET_ERROR)
		{
			Log(FAIL, TEXT("ServerThread: select() failed with error %d"), WSAGetLastError());
			fExit = TRUE;
			goto exitThread;  // An error here means one of our sockets is bad, no way to recover
		}
		
		for (i = 0; i < nNumberOfSockets; i++)	// check which socket is ready to process
		{
			if (FD_ISSET(ServiceSocket[i], &fdSockSet))	// proceed for connected socket
			{
				FD_CLR(ServiceSocket[i], &fdSockSet);
				if(pParams->nSocketType == SOCK_STREAM)
				{
					cbRemoteAddr = sizeof(ssRemoteAddr);
					sock = accept(ServiceSocket[i], (SOCKADDR*)&ssRemoteAddr, &cbRemoteAddr);
					if(sock == INVALID_SOCKET) 
					{
						Log(FAIL, TEXT("ServerThread: accept() failed with error %d"), WSAGetLastError());
						goto exitThread;
					}
					
					char szClientNameASCII[256];
					if (getnameinfo((SOCKADDR *)&ssRemoteAddr, cbRemoteAddr,
						szClientNameASCII, sizeof(szClientNameASCII), NULL, 0, NI_NUMERICHOST) != 0)
						strcpy(szClientNameASCII, "<<getnameinfo error>>");
					Log(ECHO, TEXT("ServerThread: Accepted connection from client %hs"), szClientNameASCII);
				}
				else
					sock = ServiceSocket[i];
				break;
			}
		}
	}

	// recv 1 byte
	FD_ZERO(&fdReadSet);
	FD_SET(sock, &fdReadSet);
	if(select(0, &fdReadSet, NULL, NULL, &tv_data) != 1)
	{
		Log(FAIL,  TEXT("ServerThread: Timed out waiting for data"));
		goto exitThread;
	}

	cbRemoteAddr = sizeof(ssRemoteAddr);
	cbRecvd = recvfrom(sock, &byData, sizeof(byData), 0, (SOCKADDR *)&ssRemoteAddr, &cbRemoteAddr);
	if(cbRecvd != sizeof(byData))
	{
		Log(FAIL, TEXT("ServerThread: recvfrom() failed to receive %d bytes with error %d"), 
			sizeof(byData), WSAGetLastError());
		goto exitThread;
	}

	// send 1 byte
	cbSent = sendto(sock, &byData, sizeof(byData), 0, (SOCKADDR *)&ssRemoteAddr, cbRemoteAddr);
	if(cbSent != sizeof(byData))
	{
		Log(FAIL, TEXT("ServerThread: sendto() failed to send %d bytes with error %d"), 
			sizeof(byData), WSAGetLastError());
		goto exitThread;
	}

exitThread:

	// Do not close sock if UDP since that is also our ServiceSocket.
	if(pParams->nSocketType == SOCK_STREAM && sock != INVALID_SOCKET)
	{
		shutdown(sock, 2);
		closesocket(sock);
	}

	// Here we do not actually exit the thread unless we were specifically told to do so
	if(!fExit)
	{
		Log(DETAIL, TEXT("ServerThread: Waiting for next client..."));
		goto ServiceClient;
	}

	for( i = 0; i < nNumberOfSockets; i++)
	{
		shutdown(ServiceSocket[i], 2);
		closesocket(ServiceSocket[i]);
	}

	WSACleanup();

	return 1;
}