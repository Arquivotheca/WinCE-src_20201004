//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "echothread.h"
#include <linklist.h>

#define WINSOCK_VERSION_REQ		MAKEWORD(2,2)

typedef struct {
	PLIST_ENTRY Flink;
    PLIST_ENTRY Blink;
	SOCKET sock;							// Socket Handle
} SocketNode;

typedef struct {
	PLIST_ENTRY Flink;
    PLIST_ENTRY Blink;
	HANDLE hThread;							// Thread Handle of the ProvWorkerThread
											//   function in charge of this provider
	GUID ProviderId;						// GUID ID of this provider, 
											//   all error nodes have ID == 0
	LIST_ENTRY SocketList;					// List of sockets associated
											//   with this provider
} ProviderNode;

// Global Variables
EchoThreadParms g_Params;					// Holds general parameters needed by
											// the echo thread

#define SZ_AI_FAMILY	((pAI->ai_family == PF_INET) ? TEXT("PF_INET") : \
						((pAI->ai_family == PF_INET6) ? TEXT("PF_INET6") : \
						TEXT("UNKNOWN")))

// Function Prototypes
BOOL DestroyList(PLIST_ENTRY pListHead);
BOOL DestroyProviderList(PLIST_ENTRY pProvListHead);
ProviderNode *NewProviderNode(GUID *lpProviderGuid);
SocketNode *NewSocketNode(SOCKET sock);

// Thread Prototypes
DWORD WINAPI ProvWorkerThread(LPVOID *pParm);
DWORD WINAPI TCPEchoThread(LPVOID *pParm);

//**********************************************************************************
DWORD WINAPI ServerThreadProc (LPVOID pv)
{
	HANDLE		hThread;
    DWORD		dwThreadId;
	LIST_ENTRY	ProviderList, *pProviderList;
	int			nUseSockType;
	ProviderNode *pTempProv;
	SocketNode	*pTempSocket;
	ADDRINFO	AddrHints, *pAI, *pAddrInfo;
	SOCKET		sock;
	WSAPROTOCOL_INFO wsaProcotolInfo;
	EchoThreadParms *pParams = (EchoThreadParms *)pv;

	if(pParams == NULL)
		return 0;
	
	memcpy(&g_Params, pParams, sizeof(g_Params));

	// Initialize variables
	pProviderList = &ProviderList;

	InitializeListHead(pProviderList);

	for(int i = 0; i < NUM_SOCKTYPES; i++)
	{
		// We loop through the socket types to minimize the locations where
		//   we create and add nodes
		switch(i)
		{
		case 0:
			nUseSockType = SOCK_STREAM;
			break;
		case 1:
			nUseSockType = SOCK_DGRAM;
			break;
		default:
			continue;
		}

		// If the user doesn't want to use this SockType, move on
		if(g_Params.nSockType != ALL_SOCK_TYPES && nUseSockType != g_Params.nSockType)
			continue;

		// Set up the hints structure
		memset(&AddrHints, 0, sizeof(AddrHints));
		AddrHints.ai_family = g_Params.nFamily;
		AddrHints.ai_socktype = nUseSockType;
		AddrHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

		// get the AddrInfo list
		if(getaddrinfo(NULL, g_Params.szPortASCII, &AddrHints, &pAddrInfo))
		{
			LogFail(TEXT("[EchoServer]: getaddrinfo() error %d"), WSAGetLastError());
			pAddrInfo = NULL;
		}

		for(pAI = pAddrInfo; pAI != NULL; pAI = pAI->ai_next) 
		{
			sock = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
			if (sock == INVALID_SOCKET)
			{
				LogWarn2(TEXT("[EchoServer]: socket() failed for family %s, error %d"),
					SZ_AI_FAMILY, WSAGetLastError());
				continue;
			}
			else if (bind(sock, pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
			{
				LogWarn1(TEXT("[EchoServer]: bind() failed, error %d"), WSAGetLastError());
				closesocket(sock);
				continue;
			}
			else 
			{
				if(nUseSockType == SOCK_STREAM)
				{
					if (listen(sock, 5) == SOCKET_ERROR)
					{
						LogWarn1(TEXT("[EchoServer]: listen() failed, error %d"),
							WSAGetLastError());
						closesocket(sock);
						continue;
					}
				}

				LogVerbose(TEXT("[EchoServer]: %s socket 0x%08x ready: port %hs, family %s"),
					(nUseSockType == SOCK_STREAM) ? TEXT("SOCK_STREAM") : TEXT("SOCK_DGRAM"),
					sock, g_Params.szPortASCII, SZ_AI_FAMILY);

				// If we made it here, we have a socket ready to receive
				//   incoming data/connections
				// Find out the provider for this socket
				memset(&wsaProcotolInfo, 0, sizeof(wsaProcotolInfo));
				







				// Now check the provider list to see if the provider
				//   exists there already
				for(pTempProv = (ProviderNode *)(pProviderList->Flink);
					(PLIST_ENTRY) pTempProv != pProviderList;
					pTempProv = (ProviderNode *)(pTempProv->Flink))
				{
					if(memcmp(&(wsaProcotolInfo.ProviderId),
						&(pTempProv->ProviderId), sizeof(GUID)) == 0)
					{
						break;		// Found it
					}
				}

				if((PLIST_ENTRY) pTempProv == pProviderList)
				{
					// Provider doesn't exist.
					// Add a new provider node to the provider list.
					pTempProv = NewProviderNode(&(wsaProcotolInfo.ProviderId));
					if(pTempProv)
						InsertTailList(pProviderList, (LIST_ENTRY *)pTempProv);
				}

				if(pTempProv)
				{
					// Add the socket to the provider node's list.
					pTempSocket = NewSocketNode(sock);
					if(pTempSocket)
						InsertTailList(&(pTempProv->SocketList),
						(LIST_ENTRY *)pTempSocket);
				}
				else
					closesocket(sock);  // Something's gone horribly wrong
			}
		} // for(pAI = pAddrInfo ...

		if(pAddrInfo)
			freeaddrinfo(pAddrInfo);

	} // for(i = 0; i < NUM_SOCKTYPES; i++)

	// For each provider in the provider list, create a worker
	//   thread to handle that provider's sockets
	for(pTempProv = (ProviderNode *)(pProviderList->Flink);
		(PLIST_ENTRY) pTempProv != pProviderList;
		pTempProv = (ProviderNode *)(pTempProv->Flink))
	{
		if((hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ProvWorkerThread,
			pTempProv, 0, &dwThreadId)) == NULL)
		{
			LogFail (TEXT("[EchoServer]: CreateThread(ProvWorkerThread) failed %d"), GetLastError());
		}
		else
			pTempProv->hThread = hThread;
	}
	
	LogVerbose(TEXT("[EchoServer]: Signalling that echothread is ready..."));

	if(g_hEchoThreadRdy)
		SetEvent(g_hEchoThreadRdy);

	LogVerbose(TEXT("[EchoServer]: Waiting for completion."));

	// Wait on all the worker threads to finish
	for(pTempProv = (ProviderNode *)(pProviderList->Flink);
		(PLIST_ENTRY) pTempProv != pProviderList;
		pTempProv = (ProviderNode *)(pTempProv->Flink))
	{
		if(pTempProv->hThread != NULL)
		{
			WaitForSingleObject(pTempProv->hThread, INFINITE);
			CloseHandle(pTempProv->hThread);
			Sleep(500); // Sleep for half a second to make sure the TCP echo threads have all gone away
		}
	}

	// All worker threads finished.  Time to clean up.
	DestroyProviderList(pProviderList);

	return 0;
}

//*********************************************************************************
BOOL DestroySocketList(PLIST_ENTRY pListHead)
{
	PLIST_ENTRY pTemp;

	//LogVerbose(TEXT("[EchoServer]: DestroySocketList++"));

	while(!IsListEmpty(pListHead))
	{
		// Get head element
		pTemp = pListHead->Flink;
		RemoveHeadList(pListHead);

		closesocket(((SocketNode *)pTemp)->sock);

		//LogVerbose(TEXT("[EchoServer]: DestroySocketList - Destroying node %x"), pTemp);

		free(pTemp);
	}

	//LogVerbose(TEXT("[EchoServer]: DestroySocketList--"));

	return TRUE;
}

//*********************************************************************************
BOOL DestroyProviderList(PLIST_ENTRY pProvListHead)
{
	ProviderNode *pTemp;

	//LogVerbose(TEXT("[EchoServer]: DestroyProviderList++"));

	while(!IsListEmpty(pProvListHead))
	{
		// Get head element
		pTemp = (ProviderNode *)(pProvListHead)->Flink;
		RemoveHeadList(pProvListHead);

		//LogVerbose(TEXT("[EchoServer]: DestroyProviderList - Destroying ProviderNode"));

		DestroySocketList(&(pTemp->SocketList));

		free(pTemp);
	}

	//LogVerbose(TEXT("[EchoServer]: DestroyProviderList--"));

	return TRUE;
}

//**********************************************************************************
ProviderNode *NewProviderNode(GUID *lpProviderGuid)
{
	ProviderNode *pNewNode;

	if(lpProviderGuid == NULL)
		return NULL;

	pNewNode = (ProviderNode *)malloc(sizeof(ProviderNode));

	if(pNewNode == NULL)
	{
		LogFail(TEXT("[EchoServer]: NewProviderNode - malloc() failed, error = %d"), GetLastError());
	}
	else
	{
		memcpy(&(pNewNode->ProviderId), lpProviderGuid, sizeof(pNewNode->ProviderId));
		InitializeListHead(&(pNewNode->SocketList));
	}

	return pNewNode;
}

//**********************************************************************************
SocketNode *NewSocketNode(SOCKET sock)
{
	SocketNode *pNewNode;

	pNewNode = (SocketNode *)malloc(sizeof(SocketNode));

	if(pNewNode == NULL)
	{
		LogFail(TEXT("[EchoServer]: NewSocketNode - malloc() failed, error = %d"), GetLastError());
	}
	else
		pNewNode->sock = sock;

	return pNewNode;
}

//**********************************************************************************
//
// The Provider Worker Thread watches all the sockets in its list for any
// incoming data (SOCK_DGRAM) or connections (SOCK_STREAM).  If then either
// echoes the data (SOCK_DGRAM) or hands the connection off to another worker
// thread.
//
DWORD	WINAPI		ProvWorkerThread(LPVOID *pParm)
{
	ProviderNode *pMyProv = (ProviderNode *)pParm;
	SocketNode *pSockNode, *pSocketList = (SocketNode *)&(pMyProv->SocketList);
	HANDLE hThread;
	DWORD dwThreadId;
	SOCKET accept_sock;
	SOCKADDR_STORAGE ssRemoteAddr;
	int nSize, nSockType, cbRecvd;
	fd_set fdReadSet;
	char *pBuf;
	TIMEVAL tvTimeOut = {0, 500000};

	//LogVerbose(TEXT("[EchoServer]: ProvWorkerThread++"));

	pBuf = (char *) malloc( g_Params.nMaxDataXferSize );

	if(pBuf == NULL)
	{
		LogFail(TEXT("[EchoServer]: ProvWorkerThread - malloc(pBuf) failed, err = %d!"),
			GetLastError());
		goto exitThread;
	}

	while(WaitForSingleObject(g_hTimesUp, 0) != WAIT_OBJECT_0)
	{
		FD_ZERO(&fdReadSet);

		for(pSockNode = (SocketNode *)(pSocketList->Flink);
			pSockNode != pSocketList;
			pSockNode = (SocketNode *)(pSockNode->Flink))
		{
			FD_SET(pSockNode->sock, &fdReadSet);
		}

		if(select(1, &fdReadSet, NULL, NULL, &tvTimeOut) == SOCKET_ERROR)
		{
			LogFail(TEXT("[EchoServer]: ProvWorkerThread - select() failed, error %d"), 
				WSAGetLastError());
			goto exitThread;
		}
		else
		{
			for(pSockNode = (SocketNode *)(pSocketList->Flink);
				pSockNode != pSocketList;
				pSockNode = (SocketNode *)(pSockNode->Flink))
			{
				if(FD_ISSET(pSockNode->sock, &fdReadSet))
				{
					nSockType = SOCK_STREAM;
					nSize = sizeof(nSockType);
					getsockopt(pSockNode->sock, SOL_SOCKET, SO_TYPE, (char *)&nSockType, &nSize);
					if(nSockType == SOCK_STREAM)
					{
						// Incomming connection
						// Accept it
						nSize = sizeof(ssRemoteAddr);
						accept_sock = accept(pSockNode->sock, (SOCKADDR*)&ssRemoteAddr, &nSize);
						if(accept_sock == INVALID_SOCKET) 
						{
							LogFail(TEXT("[EchoServer]: ProvWorkerThread - accept() failed with error %d"), 
								WSAGetLastError());
						}

						// Spawn another worker thread to handle it
						if((hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) TCPEchoThread,
							(void *)accept_sock, 0, &dwThreadId)) == NULL)
						{
							LogFail(TEXT("[EchoServer]: ProvWorkerThread - CreateThread(TCPEchoThread) failed %d"), GetLastError());
							closesocket(accept_sock);
						}
						else
							CloseHandle(hThread);
					}
					else if(nSockType == SOCK_DGRAM)
					{
						// Incomming data

						// Recv it
						nSize = sizeof(ssRemoteAddr);
						cbRecvd = recvfrom(pSockNode->sock, (char *)pBuf, g_Params.nMaxDataXferSize, 0,
							(SOCKADDR *)&ssRemoteAddr, &nSize);
						if(cbRecvd == SOCKET_ERROR)
						{
							// For some inane reason, NT returns WSAECONNRESET when a packet we
							// send gets an ICMP "Port Unreachable" message.  Ignore it.
							if(WSAGetLastError() != WSAECONNRESET)
							{
								LogFail(TEXT("[EchoServer]: ProvWorkerThread - recvfrom() failed, error %d"),
									WSAGetLastError());
							}
						}
						else if(!g_Params.fBlackholeMode)
						{
							// Echo it back
							if(sendto(pSockNode->sock, (char *)pBuf, cbRecvd, 0,
								(SOCKADDR *)&ssRemoteAddr, nSize) == SOCKET_ERROR)
							{
								LogFail(TEXT("[EchoServer]: ProvWorkerThread - sendto() failed, error %d"),
									WSAGetLastError());
							}
						} // if(cbRecvd == SOCKET_ERROR)
					} // else if(nSockType == SOCK_DGRAM)
				} // if(FD_ISSET(pSockNode->sock, &fdReadSet))
			} // for(pSockNode = (SocketNode *)
		} // if(select(1, &fdReadSet, NULL, NULL, NULL) == SOCKET_ERROR), else
	} // while(1)

exitThread:

	if(pBuf)
		free(pBuf);

	//LogVerbose(TEXT("[EchoServer]: ProvWorkerThread--"));
	return 0;
}

//**********************************************************************************
DWORD WINAPI TCPEchoThread(LPVOID *pParm)
{
	SOCKET			sock = (SOCKET)pParm;
	char			*pBuf = NULL;
	int			cbRecvd;
	fd_set fdReadSet;
	TIMEVAL tvTimeOut = {0, 500000};

	//LogVerbose(TEXT("[EchoServer]: TCPEchoThread++"));

	pBuf = (char *) malloc(g_Params.nMaxDataXferSize );

	if(pBuf == NULL)
	{
		LogFail(TEXT("[EchoServer]: TCPEchoThread - Could not allocate enough memory for packet data!"));
		goto exitThread;
	}
	
	cbRecvd = 1;
	while((cbRecvd > 0) && (WaitForSingleObject(g_hTimesUp, 0) != WAIT_OBJECT_0))
	{
		FD_ZERO(&fdReadSet);
		FD_SET(sock, &fdReadSet);

		if(select(1, &fdReadSet, NULL, NULL, &tvTimeOut) > 0)
		{
			cbRecvd = recv(sock, (char *)pBuf, g_Params.nMaxDataXferSize, 0);
			if(cbRecvd == SOCKET_ERROR)
			{
				LogFail(TEXT("[EchoServer]: TCPEchoThread - recv() failed, error %d"),
					WSAGetLastError());
			}
			else if(cbRecvd > 0 && !g_Params.fBlackholeMode)
			{
				if(send(sock, (char *)pBuf, cbRecvd, 0) == SOCKET_ERROR)
				{
					LogFail(TEXT("[EchoServer]: TCPEchoThread - send() failed, error %d"),
						WSAGetLastError());
					cbRecvd = SOCKET_ERROR;	// we failed, exit the echo loop
				}
			}
		}
	}

exitThread:

	closesocket(sock);

	if(pBuf)
		free(pBuf);

	//LogVerbose(TEXT("[EchoServer]: TCPEchoThread--"));

	return 0;
}