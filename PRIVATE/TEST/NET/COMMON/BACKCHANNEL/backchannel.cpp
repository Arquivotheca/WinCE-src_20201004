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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*
      @doc LIBRARY


Module Name:

  backchannel.cpp

 @module BackChannel - A generic way to invoke commands across processes or machines. |

  Quite often developers need to invoke actions in another process or pass data between them.  This is
  especially true when testing components using the client/server model where the test app needs to
  control the server side.  This library handles the socket-level code that transfers the data back and
  forth for both the client and the server.

  First a BackChannel service is started with BackChannel_StartService.  Clients then connect to the
  listening BackChannel service, invoke operations using BackChannel_SendCommand, and then get the results from
  BackChannel_GetResult.  Finally clients disconnect from the BackChannel service with BackChannel_Disconnect.

  The BackChannel service registers a table of functions along with command numbers.  The client can invoke those
  functions via the command numbers and pass in binary data.  The command functions return a DWORD result as
  well as an arbitrary amount of binary data.  The client and server need to be kept in sync to avoid versioning
  issues.

  See the bcex sample for more detailed examples.

   <nl>Link:    backchannel.lib
   <nl>Include: backchannel.h

     @xref    <f BackChannel_ConnectEx>
     @xref    <f BackChannel_SendCommand>
	 @xref    <f BackChannel_GetResult>
     @xref    <f BackChannel_Disconnect>
     @xref    <f BackChannel_StartService>
*/

#include "backchannel.h"

#ifdef SZ_AI_FAMILY
#undef SZ_AI_FAMILY
#endif

#define SZ_AI_FAMILY ((pAI->ai_family == PF_INET) ? _T("PF_INET") : _T("PF_INET6"))

// Private Function Prototypes
DWORD WINAPI ServerThread(LPVOID *pParm);
DWORD WINAPI WorkerThread(LPVOID *pParm);

//
//  @func SOCKET | BackChannel_ConnectEx |  
//  Forms a connection to a BackChannel service.
//
//  @rdesc Returns a SOCKET for the command channel connection that should be used in future calls.
//
//  @parm  IN LPSTR                    | szServer  |  Name or address of the server to connect to
//  @parm  IN LPSTR                    | szPort    |  Port number that server is listening on
//  @parm  IN int                      | iFamily   |  Address family to use
//
//  @comm:(LIBRARY)
//  This function forms the initial TCP connection from the client process to the server process typically
//  running on another machine.
//  To connect to a local server, use "localhost" for szServer.
//  Set iFamily to AF_INET (IPv4), AF_INET6 (IPv6) or AF_UNSPEC (don't care).
//
//
// @ex		The following will connect to a backchannel service on AutoNet2 listening on port 55255: |
//
//			SOCKET BCSocket = BackChannel_ConnectEx("AutoNet2", "55255", AF_UNSPEC);
//
//			if(BCSocket == INVALID_SOCKET)
//				printf("BC_ConnectEx() failed");
//
//**********************************************************************************
SOCKET BackChannel_ConnectEx(LPSTR szServer, LPSTR szPort, int iFamily)
{
	BOOL fResult = FALSE;
	SOCKET sock = INVALID_SOCKET, sockReturn = INVALID_SOCKET;
	ADDRINFO Hints, *pAddrInfo = NULL, *pAI;
	int iGAIResult;

	if(!szServer)
		szServer = "localhost";
	if(!szPort)
		szPort = "12345";

	//
	// Resolve the server name/address
	//
	memset(&Hints, 0, sizeof(Hints));
	Hints.ai_family = iFamily;
	Hints.ai_socktype = SOCK_STREAM;

	if(iGAIResult = getaddrinfo(szServer, szPort, &Hints, &pAddrInfo))
	{
		CmnPrint(PT_FAIL, TEXT("[BC] Couldn't get resolve the server name/address!( %s )"), ErrorToText( iGAIResult) );
		CmnPrint(PT_LOG, TEXT("[BC] Server - %hs, Port - %hs"), szServer, szPort);
		goto Cleanup;
	}

	CmnPrint(PT_VERBOSE, TEXT("[BC] Connecting with server - %hs"), szServer);

	//
	// Attempt to connect to each address until we find one that succeeds
	//
	for(pAI = pAddrInfo; pAI != NULL; pAI = pAI->ai_next) 
	{
		if((pAI->ai_family == PF_INET) || (pAI->ai_family == PF_INET6)) // only want PF_INET or PF_INET6
		{
			sock = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
			if (sock != INVALID_SOCKET)
			{
				if(connect(sock, pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
				{
					// Connect failed, let's close this socket and try again on the next address in the list
					closesocket(sock);
					continue;
				}

				// connect() succeeded
				break;
			}
		}
	}
	
	if (pAI == NULL) 
	{
		CmnPrint(PT_FAIL, TEXT("[BC] Unable to connect to any of the server's addresses!"));
		goto Cleanup;
	}

	sockReturn = sock;

Cleanup:

	if(pAddrInfo)
		freeaddrinfo(pAddrInfo);

	return sockReturn;
}

//
//  @func BOOL | BackChannel_SendCommand |  
//  Sends a command to the BackChannel server along with binary command data.
//
//  @rdesc Returns TRUE if the command was sent successfully.  FALSE if an error occurred.
//
//  @parm  IN SOCKET                   | sock                |  Command channel socket from BackChannel_ConnectEx
//  @parm  IN DWORD                    | dwCommand           |  Number of the command to execute
//  @parm  IN BYTE *                   | pbCommandData       |  Pointer to binary data to pass to the server
//  @parm  IN DWORD                    | dwCommandDataSize   |  Amount of binary data pointed to by pbCommandData
//
//  @comm:(LIBRARY)
//  Sends a command to the BackChannel server over an established command channel.  The caller app is responsible
//  for marshalling the command data and for ensuring the command number is correct.
//
//
// @ex		The following will invoke a remote command and pass it some simple data: |
//
//			#define MYCOMMAND	10
//
//			SOCKET BCSocket = BackChannel_ConnectEx("AutoNet2", "55255", AF_UNSPEC);
//
//			if(BCSocket != INVALID_SOCKET)
//			{
//				DWORD dwData, dwDataSize, dwResult = 0;
//				dwDataSize = sizeof(dwData);
//				if(BackChannel_SendCommand(BCSocket, MYCOMMAND, (BYTE *)&dwData, dwDataSize))
//				{
//					if(BackChannel_GetResult(BCSocket, &dwResult, NULL, NULL))
//						printf("MYCOMMAND returned %d", dwResult);
//				}
//				else
//					printf("BackChannel_ConnectEx() failed");
//			}
//
//**********************************************************************************
BOOL BackChannel_SendCommand(SOCKET sock, DWORD dwCommand, BYTE *pbCommandData, DWORD dwCommandDataSize)
{
	BOOL fResult = FALSE;
	int cbBytesXfer;
	COMMAND_PACKET_HEADER CommandPacketHeader;

	if(!pbCommandData && dwCommandDataSize)
	{
		CmnPrint(PT_FAIL, TEXT("[BC] CommandData pointer can not be NULL if dwCommandDataSize != 0"));
		goto Cleanup;
	}

	//
	// Send command to the server
	//
	CommandPacketHeader.dwCommand = dwCommand;
	CommandPacketHeader.dwDataSize = dwCommandDataSize;

	cbBytesXfer = send(sock, (char *)&CommandPacketHeader, sizeof(CommandPacketHeader), 0);
	if(cbBytesXfer == SOCKET_ERROR)
	{
		CmnPrint(PT_FAIL, TEXT("[BC] send(CommandPacketHeader) failed, error = %s (%d)"), 
			GetLastErrorText(), WSAGetLastError());
		goto Cleanup;
	}

	if(dwCommandDataSize)
	{
		cbBytesXfer = send(sock, (char *)pbCommandData, dwCommandDataSize, 0);
		if(cbBytesXfer == SOCKET_ERROR)
		{
			CmnPrint(PT_FAIL, TEXT("[BC] send(CommandData) failed, error = %s (%d)"), 
				GetLastErrorText(), WSAGetLastError());
			goto Cleanup;
		}
	}

	fResult = TRUE;

Cleanup:

	return fResult;
}

//
//  @func BOOL | BackChannel_GetResult |  
//  Gets the latest result of commands sent by the client.
//
//  @rdesc Returns TRUE if the result was received successfully.  FALSE if an error occurred.
//
//  @parm  IN SOCKET                   | sock                |  Command channel socket from BackChannel_ConnectEx
//  @parm  OUT DWORD *                 | pdwResult           |  Return value of the command 
//  @parm  OUT BYTE **                 | ppbReturnData       |  Pointer to binary data passed back by the command
//  @parm  IN OUT DWORD *              | pdwReturnDataSize   |  Amount of binary data pointed to by ppbReturnData
//
//  @comm:(LIBRARY)
//  Returns the result of a command.  Will wait for a maximum of g_ResultTimeoutSecs.
//  For each SendCommand, there must be a corresponding GetResult.  Only one GetResult
//  call should be outstanding at a time.
//
//  ppbReturnData will be allocated with malloc() to match the amount of returned data.
//  The caller should free() the buffer when it is no longer needed.
//
//
// @ex		The following will invoke a remote command and get its result: |
//
//			#define MYCOMMAND	10
//
//			SOCKET BCSocket = BackChannel_ConnectEx("AutoNet2", "55255", AF_UNSPEC);
//
//			if(BCSocket != INVALID_SOCKET)
//			{
//				char szCommandData[] = "command", *szResultData = NULL;
//				DWORD dwDataSize, dwResult = 0;
//				dwDataSize = (stren(chCommandData) + 1) * sizeof(char);
//				if(BackChannel_SendCommand(BCSocket, MYCOMMAND, (BYTE *)szCommandData, dwDataSize))
//				{
//					if(BackChannel_GetResult(BCSocket, &dwResult, &szResultData, &dwDataSize))
//					{
//						printf("MYCOMMAND returned %d and %d bytes of data", dwResult, dwDataSize);
//						if(szResultData)
//						{
//							printf("Result data = %s", szResultData);
//							free(szResultData);
//						}
//					}
//				}
//				else
//					printf("BackChannel_ConnectEx() failed");
//			}
//
//**********************************************************************************
BOOL BackChannel_GetResult(SOCKET sock, DWORD *pdwResult, BYTE **ppbReturnData, DWORD *pdwReturnDataSize)
{
	BOOL fResult = FALSE;
	RESULT_PACKET_HEADER ResultPacketHeader;
	int cbTotalBytesXfer, cbBytesXfer, nRet;
	fd_set fdSet;
	TIMEVAL tvTimeout = {g_ResultTimeoutSecs, 0};

	if(!ppbReturnData || !pdwReturnDataSize)
	{
		CmnPrint(PT_FAIL, TEXT("[BC] GetResult() OUT pointers are invalid"));
		goto Cleanup;
	}

	*pdwReturnDataSize = 0;
	*ppbReturnData = NULL;

	if(*pdwResult)
		*pdwResult = ERROR_INVALID_PARAMETER;

	//
	// Receive the result header back from the server
	//

	memset(&ResultPacketHeader, 0xcc, sizeof(ResultPacketHeader));

	cbTotalBytesXfer = 0;
	for(cbTotalBytesXfer = 0, cbBytesXfer = 1; 
		cbBytesXfer > 0 && (DWORD)cbTotalBytesXfer < sizeof(ResultPacketHeader);
		cbTotalBytesXfer += cbBytesXfer)
	{
		FD_ZERO(&fdSet);
		FD_SET(sock, &fdSet);

		if(select(0, &fdSet, NULL, NULL, &tvTimeout) != 1)
		{
			CmnPrint(PT_FAIL, TEXT("[BC] Timed out waiting for data from the server"));
			goto Cleanup;
		}

		cbBytesXfer = recv(sock, (char*)(&ResultPacketHeader) + cbTotalBytesXfer, 
			sizeof(ResultPacketHeader) - cbTotalBytesXfer, 0);

		if (cbBytesXfer == SOCKET_ERROR)
		{
			CmnPrint(PT_FAIL, TEXT("[BC] recv(ResultHeader) failed %s (%d)"), GetLastErrorText(), WSAGetLastError());
			goto Cleanup;
		}
		CmnPrint(PT_VERBOSE, TEXT("[BC] BackChannel_GetResult: cbBytesXfer = %d, cbTotalBytesXfer = %d"), cbBytesXfer, cbTotalBytesXfer);
	}
	CmnPrint(PT_VERBOSE, TEXT("[BC] BackChannel_GetResult: result = %d, size = %d"), ResultPacketHeader.dwResult, ResultPacketHeader.dwDataSize);

	if(cbTotalBytesXfer != sizeof(ResultPacketHeader))
	{
		CmnPrint(PT_FAIL, TEXT("[BC] Server didn't send all the expected data!"));
		goto Cleanup;
	}
	else if(pdwResult)
		*pdwResult = ResultPacketHeader.dwResult;

	//
	// Receive the result data back from the server
	//
	if(ResultPacketHeader.dwDataSize > 0)
	{
		// Put a check in here to make sure some bad data can't make us spin forever.
		if(ResultPacketHeader.dwDataSize > MAX_RESULT_DATA_SIZE)
		{
			CmnPrint(PT_FAIL, TEXT("[BC] Server trying to return too much data (%u bytes).  Closing this session."),
				ResultPacketHeader.dwDataSize);
			closesocket(sock);
			goto Cleanup;
		}

		// Allocate a buffer to receive the incomming data
		*ppbReturnData = (BYTE *)malloc(ResultPacketHeader.dwDataSize);

		if(!*ppbReturnData)
		{
			CmnPrint(PT_FAIL, TEXT("[BC] Out of memory.  Couldn't allocate enough space to hold result data.  Closing connection."));
			closesocket(sock);
			goto Cleanup;
		}

		for(cbTotalBytesXfer = 0, cbBytesXfer = 1; 
			cbBytesXfer > 0 && (DWORD)cbTotalBytesXfer < ResultPacketHeader.dwDataSize;
			cbTotalBytesXfer += cbBytesXfer)
		{
			FD_ZERO(&fdSet);
			FD_SET(sock, &fdSet);
			nRet = select(0, &fdSet, NULL, NULL, &tvTimeout);

			if(nRet == SOCKET_ERROR)
			{
				CmnPrint(PT_FAIL, TEXT("[BC] select() failed, error = %s (%d)"),
					GetLastErrorText(), WSAGetLastError());
				goto Cleanup;
			}
			else if(nRet == 0)
			{
				CmnPrint(PT_FAIL, TEXT("[BC] Timed out while waiting for data from the server"));
				goto Cleanup;
			}

			cbBytesXfer = recv(sock, (char *)(*ppbReturnData + cbTotalBytesXfer),
				ResultPacketHeader.dwDataSize - cbTotalBytesXfer, 0);
			if (cbBytesXfer == SOCKET_ERROR)
			{
				CmnPrint(PT_FAIL, TEXT("[BC] recv(ReturnData) failed %s (%d)"), GetLastErrorText(), WSAGetLastError());
				goto Cleanup;
			}
		}

		if(cbBytesXfer == 0 && cbTotalBytesXfer != ResultPacketHeader.dwDataSize)
		{
			CmnPrint(PT_FAIL, TEXT("[BC] Server did not send enough data before closing the connection."));
			goto Cleanup;
		}
		else
			*pdwReturnDataSize = ResultPacketHeader.dwDataSize;
	}

	fResult = TRUE;

Cleanup:

	if(ppbReturnData && *ppbReturnData && pdwReturnDataSize && !*pdwReturnDataSize)
	{
		free(*ppbReturnData);
		*ppbReturnData = NULL;
	}

	return fResult;
}

//
//  @func BOOL | BackChannel_Disconnect |  
//  Closes an already established command channel.
//
//  @rdesc TRUE if successful.  FALSE if an error occurred.
//
//  @parm  IN SOCKET                   | sock                |  Command channel socket from BackChannel_ConnectEx
//
//  @comm:(LIBRARY)
//  When a client is finished using a BackChannel command connection, it should
//  call BackChannel_Disconnect to close the TCP connection.
//
//
// @ex		The following will establish and close a command channel: |
//
//			SOCKET BCSocket = BackChannel_ConnectEx("AutoNet2", "55255", AF_UNSPEC);
//
//			if(BCSocket != INVALID_SOCKET)
//			{
//				printf("Command channel established to AutoNet2");
//				if(BackChannel_Disconnect(BCSocket))
//				{
//					printf("Command channel disconnected");
//				}
//				else
//					printf("BackChannel_Disconnect() failed");
//			}
//
//**********************************************************************************
BOOL BackChannel_Disconnect(SOCKET sock)
{
	if(sock == INVALID_SOCKET)
		return FALSE;

	shutdown(sock, SD_BOTH);
	closesocket(sock);
	return TRUE;
}


//
//  @func HANDLE | BackChannel_StartService |  
//  Gets the latest result of commands sent by the client.
//
//  @rdesc Returns a handle to an event to set when the service should be stopped.  NULL if an error occurred.
//
//  @parm  IN LPSTR                    | szPort            |  Port for the command service to listen on
//  @parm  IN LPSTARTUP_CALLBACK       | pfnStartupFunc    |  Optional function called when a new connection is formed.
//  @parm  IN LPCLEANUP_CALLBACK       | pfnCleanupFunc    |  Optional function called when a connection is shut down.
//  @parm  OUT HANDLE *                | phServiceThread   |  Handle of the main service thread is returned via this pointer.
//
//  @comm:(LIBRARY)
//  Returns the result of a command.  Will wait for a maximum of g_ResultTimeoutSecs.
//  For each SendCommand, there must be a corresponding GetResult.  Only one GetResult
//  call should be outstanding at a time.
//
//  ppbReturnData will be allocated with malloc() to match the amount of returned data.
//  The caller should free() the buffer when it is no longer needed.
//
//
// @ex		The following will start a BackChannel service with 2 commands on port 55255: |
//
//			COMMAND_HANDLER_ENTRY g_CommandHandlerArray[] =
//			{
//				TEXT("Func1"),			1,				BcExHandlerFunction1,
//				TEXT("Func2a"),			2,				BcExHandlerFunction2,
//				TEXT("Func2b"),			3,				BcExHandlerFunction2,
//				NULL,					0,				NULL	// End of List
//			};
//
//
//			BOOL ConnectionStartup(SOCKET sock)
//			{
//				// Returning TRUE indicates the connection should be processed
//				// Returning FALSE indicates the client should be dropped
//				_tprintf(TEXT("ConnectionStartup: New client connection with socket handle 0x%08x"), sock);
//				return TRUE;
//			}
//			BOOL ConnectionCleanup(SOCKET sock)
//			{
//				_tprintf(TEXT("ConnectionCleanup: Cleaning up client with socket handle 0x%08x"), sock);
//				return TRUE;
//			}
//			
//			int mymain (int argc, TCHAR* argv[])
//			{
//			WSADATA wsaData;
//			HANDLE hStopService;
//
//			CmnPrint_Initialize();
//			CmnPrint_RegisterPrintFunctionEx(PT_LOG, _tprintf, FALSE);
//			CmnPrint_RegisterPrintFunctionEx(PT_FAIL, _tprintf, FALSE);
//
//			WSAStartup(MAKEWORD(2,2), &wsaData);
//
//			HANDLE hServiceThread = NULL;
//			hStopService = BackChannel_StartService("55255", ConnectionStartup, ConnectionCleanup, &hServiceThread);
//			if(!hStopService)
//				_tprintf(TEXT("BackChannel_StartService() failed"));
//			else
//			{
//				// This stops the service after 5 minutes.
//				WaitForSingleObject(hServiceThread, 5 * 60 * 1000);
//
//				_tprintf(TEXT("Setting StopService Event"));
//				SetEvent(hStopService);
//
//				_tprintf(TEXT("Waiting for service thread to finish..."));
//				WaitForSingleObject(hServiceThread, INFINITE);
//
//				_tprintf(TEXT("Backchannel service thread exited"));
//			}
//
//			CmnPrint_Cleanup();
//
//**********************************************************************************
HANDLE BackChannel_StartService(
								LPSTR szPort, 
								LPSTARTUP_CALLBACK pfnStartupFunc, 
								LPCLEANUP_CALLBACK pfnCleanupFunc,
								HANDLE *phServiceThread)
{
	SOCKET SockServ[FD_SETSIZE];
	int i;
	HANDLE hThread = NULL, hStopService = NULL;
	DWORD dwThreadId;
	ADDRINFO Hints, *AddrInfo = NULL, *pAI;
	BOOL fResult = FALSE;
	SERVER_THREAD_DATA *pServerThreadData;

	for(i = 0; i < FD_SETSIZE; i++)
		SockServ[i] = INVALID_SOCKET;

	// Verify parameters
	if(!szPort)
	{
		CmnPrint(PT_FAIL, TEXT("[BC] Port cannot be NULL"));
		goto Cleanup;
	}

	if(!g_CommandHandlerArray)
	{
		CmnPrint(PT_FAIL, TEXT("[BC] Global Command Handler Array cannot be NULL"));
		goto Cleanup;
	}

	//
	// Get a list of available addresses to serve on
	//
	memset(&Hints, 0, sizeof(Hints));
	Hints.ai_family = PF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;
	Hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

	i = getaddrinfo(NULL, szPort, &Hints, &AddrInfo);
	if(i)
	{
		CmnPrint(PT_FAIL, TEXT("[BC] getaddrinfo() failed with error %s (%d)"),
			ErrorToText(i), i);
		goto Cleanup;
	}

	//
	// Create a list of serving sockets, one for each address
	//
	for(pAI = AddrInfo, i = 0; (pAI != NULL) && (i < FD_SETSIZE); pAI = pAI->ai_next, i++) 
	{
		if((pAI->ai_family == PF_INET) || (pAI->ai_family == PF_INET6)) // only want PF_INET or PF_INET6
		{
			SockServ[i] = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
			if (SockServ[i] != INVALID_SOCKET)
			{
				if (bind(SockServ[i], pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
				{
					CmnPrint(PT_FAIL, TEXT("[BC] bind() failed for %s, with error %s (%d)"),
						SZ_AI_FAMILY, GetLastErrorText(), WSAGetLastError());
					CmnPrint(PT_FAIL, TEXT("[BC] There is probably another service already running using the desired port."));
					freeaddrinfo(AddrInfo);
					goto Cleanup;
				}
				else 
				{
					if (listen(SockServ[i], 5) == SOCKET_ERROR)
					{
						closesocket(SockServ[i]);
						continue;
					}

					CmnPrint(PT_VERBOSE,  
						TEXT("[BC] Socket 0x%08x ready for connection with %s family on port %hs"), 
						SockServ[i], SZ_AI_FAMILY, szPort);
				}
			}
		}
	}
	
	freeaddrinfo(AddrInfo);
	
	if (i == 0) 
	{
		CmnPrint(PT_FAIL, TEXT("[BC] Unable to serve on any address. Error = %s (%d)"), 
			GetLastErrorText(), WSAGetLastError());
		goto Cleanup;
	}

	// Create Stop Service Event
	hStopService = CreateEvent(NULL, TRUE, FALSE, NULL);

	if(!hStopService)
	{
		CmnPrint(PT_FAIL, TEXT("[BC] CreateEvent failed, error = %s (%d)"), 
			GetLastErrorText(), GetLastError());
		goto Cleanup;
	}

	// Allocate Server Thread Data
	pServerThreadData = (SERVER_THREAD_DATA *)malloc(sizeof(SERVER_THREAD_DATA));

	if(!pServerThreadData)
	{
		CmnPrint(PT_FAIL, TEXT("[BC] Out of memory.  Failed to allocate ServerThreadData"));
		goto Cleanup;
	}
	else
	{
		pServerThreadData->hStopService = hStopService;
		pServerThreadData->nNumSocks = i;
		pServerThreadData->pfnStartupFunc = pfnStartupFunc;
		pServerThreadData->pfnCleanupFunc = pfnCleanupFunc;
		memcpy(pServerThreadData->SockServ, SockServ, sizeof(SockServ));
	}

	// Create a server thread here so we don't block user's execution
	if ((hThread = 
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ServerThread,
		(LPVOID)pServerThreadData, 0, &dwThreadId)) == NULL)
	{
		CmnPrint(PT_FAIL, TEXT("[BC] CreateThread(ServerThread) failed, error = %s (%d)"),
			GetLastErrorText(), GetLastError());
		free(pServerThreadData);
		goto Cleanup;
	}
	else
	{
		CmnPrint(PT_LOG, TEXT("[BC] Spawned server thread 0x%08x to handle clients on port %hs"), 
			dwThreadId, szPort);
	}

	fResult = TRUE;

Cleanup:

	if(!fResult)
	{
		if(hStopService)
		{
			CloseHandle(hStopService);
			hStopService = NULL;
		}

		for(i = 0; i < FD_SETSIZE; i++)
		{
			if(SockServ[i] != INVALID_SOCKET)
				closesocket(SockServ[i]);
		}

		hThread = NULL;
	}

	if(phServiceThread)
		*phServiceThread = hThread;
	else
		CloseHandle(hThread);

	return hStopService;
}

//**********************************************************************************
DWORD WINAPI ServerThread(LPVOID *pParm)
{
	SERVER_THREAD_DATA *pServerThreadData = (SERVER_THREAD_DATA *)pParm;
	SOCKET sock = INVALID_SOCKET, SockServ[FD_SETSIZE];
	SOCKADDR_STORAGE ssRemoteAddr;
	int i, cbRemoteAddrSize, nNumSocks;
	HANDLE hThread, hStopService;
	DWORD dwThreadId;
	fd_set fdSockSet;
	BOOL fResult = FALSE;
	TIMEVAL tvTimeout = {STOP_SERVICE_POLL_FREQ_SECS, 0};
	WORKER_THREAD_DATA *pWorkerThreadData;
	LPSTARTUP_CALLBACK pfnStartupFunc;
	LPCLEANUP_CALLBACK pfnCleanupFunc;

	// Verify parameter validity
	if(!pServerThreadData)
		return 1;
	else
	{
		// Since this is typically a long-lived thread, I copy the passed in
		// data off the heap and to the stack.  Then free the heap block.
		hStopService = pServerThreadData->hStopService;
		nNumSocks = pServerThreadData->nNumSocks;
		pfnStartupFunc = pServerThreadData->pfnStartupFunc;
		pfnCleanupFunc = pServerThreadData->pfnCleanupFunc;
		memcpy(SockServ, pServerThreadData->SockServ, sizeof(SockServ));
		free(pServerThreadData);
	}

	//
	// Wait for incomming data/connections
	//

	while(WaitForSingleObject(hStopService, 0) == WAIT_TIMEOUT)
	{
		FD_ZERO(&fdSockSet);

		for (i = 0; i < nNumSocks; i++)	// want to check all available sockets
			FD_SET(SockServ[i], &fdSockSet);

		i = select(nNumSocks, &fdSockSet, 0, 0, &tvTimeout);
		if (i == SOCKET_ERROR)
		{
			CmnPrint(PT_FAIL, TEXT("[BC] select() failed with error = %s (%d)"), 
				GetLastErrorText(), WSAGetLastError());
			goto Cleanup;
		}
		else if(i == 0)
			continue;

		for (i = 0; i < nNumSocks; i++)	// check which socket is ready to process
		{
			if (FD_ISSET(SockServ[i], &fdSockSet))	// proceed for connected socket
			{
				FD_CLR(SockServ[i], &fdSockSet);

				// Accept the incomming connection
				cbRemoteAddrSize = sizeof(ssRemoteAddr);
				sock = accept(SockServ[i], (SOCKADDR*)&ssRemoteAddr, &cbRemoteAddrSize);
				if(sock == INVALID_SOCKET) 
				{
					CmnPrint(PT_FAIL, TEXT("[BC] accept() failed with error = %s (%d)"),
						GetLastErrorText(), WSAGetLastError());
					continue;
				}

				CmnPrint(PT_VERBOSE, TEXT("[BC] Accepted client connection from socket 0x%08x"), sock);

				// Allocate Worker Thread Data
				pWorkerThreadData = (WORKER_THREAD_DATA *)malloc(sizeof(WORKER_THREAD_DATA));

				if(!pWorkerThreadData)
				{
					CmnPrint(PT_FAIL, TEXT("[BC] Out of memory.  Failed to allocate WorkerThreadData"));
					closesocket(sock);
					continue;
				}
				else
				{
					pWorkerThreadData->sock = sock;
					pWorkerThreadData->hStopService = hStopService;
					pWorkerThreadData->pfnStartupFunc = pfnStartupFunc;
					pWorkerThreadData->pfnCleanupFunc = pfnCleanupFunc;
				}

				// Spawn a worker thread to handle this client
				if ((hThread = 
					CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) WorkerThread,
					(LPVOID)pWorkerThreadData, 0, &dwThreadId)) == NULL)
				{
					CmnPrint(PT_FAIL, TEXT("[BC] CreateThread(WorkerThread) failed, error = %s (%d)"),
						GetLastErrorText(), GetLastError());
					free(pWorkerThreadData);
					closesocket(sock);
					continue;
				}
				else
				{
					CloseHandle(hThread);
					CmnPrint(PT_LOG, TEXT("[BC] Spawned worker thread 0x%08x to handle new client"), dwThreadId);
				}
			}
		}
	}

Cleanup:

	for(i = 0; i < nNumSocks && SockServ[i] != INVALID_SOCKET; i++)
		closesocket(SockServ[i]);

	if(sock != INVALID_SOCKET)
	{
		shutdown(sock, SD_BOTH);
		closesocket(sock);
	}

	if(hStopService)
		CloseHandle(hStopService);

	return 0;
}

//**********************************************************************************
DWORD WINAPI WorkerThread(LPVOID *pParm)
{
	WORKER_THREAD_DATA *pWorkerThreadData = (WORKER_THREAD_DATA *)pParm;
	SOCKET sock;
	BYTE *pbCommandData = NULL, *pbReturnData = NULL;
	DWORD dwCommandDataSize = 0, dwReturnDataSize = 0, i;
	COMMAND_PACKET_HEADER CommandPacketHeader;
	RESULT_PACKET_HEADER ResultPacketHeader;
	int cbTotalBytesXfer, cbBytesXfer, iSelRet;
	fd_set fdSet;
	BOOL fExit = FALSE;
	TIMEVAL tvTimeout = {g_CommandTimeoutSecs, 0};

	// Verify parameters
	if(!pWorkerThreadData)
		return 1;

	sock = pWorkerThreadData->sock;

	if(pWorkerThreadData->pfnStartupFunc)
		fExit = !(pWorkerThreadData->pfnStartupFunc)(sock);

	// Process until client disconnects or until we timeout waiting for data
	while(!fExit)
	{
		// Initialize variables
		pbCommandData = pbReturnData = NULL;
		dwCommandDataSize = dwReturnDataSize = 0;
		memset(&CommandPacketHeader, 0, sizeof(CommandPacketHeader));
		memset(&ResultPacketHeader, 0, sizeof(ResultPacketHeader));

		//
		// Receive the Command Packet Header
		//
		for(cbTotalBytesXfer = 0, cbBytesXfer = 1; 
			cbBytesXfer > 0 && (DWORD)cbTotalBytesXfer < sizeof(CommandPacketHeader);
			cbTotalBytesXfer += cbBytesXfer)
		{
			FD_ZERO(&fdSet);
			FD_SET(sock, &fdSet);
			iSelRet = select(0, &fdSet, NULL, NULL, &tvTimeout);

			if(iSelRet == SOCKET_ERROR)
			{
				CmnPrint(PT_FAIL, TEXT("[BC] select() failed, error = %s (%d)"),
					GetLastErrorText(), WSAGetLastError());
				goto ExitThread;
			}
			else if(iSelRet == 0)
			{
				CmnPrint(PT_WARN1, TEXT("[BC] Timed out while waiting for data from the client"));
				goto ExitThread;
			}

			cbBytesXfer = recv(sock, (char *)(&CommandPacketHeader) + cbTotalBytesXfer, 
				sizeof(CommandPacketHeader) - cbTotalBytesXfer, 0);
			if (cbBytesXfer == SOCKET_ERROR)
			{
				CmnPrint(PT_FAIL, TEXT("[BC] recv(CommandHeader) failed %s (%d)"), GetLastErrorText(), WSAGetLastError());
				goto ExitThread;
			}
			else if(cbBytesXfer == 0)
			{
				CmnPrint(PT_LOG, TEXT("[BC] Client disconnected."));
				goto ExitThread;
			}
		}

		if(cbBytesXfer == 0 && cbTotalBytesXfer != sizeof(CommandPacketHeader))
		{
			CmnPrint(PT_FAIL, TEXT("[BC] Client did not send enough data before closing the connection."));
			goto ExitThread;
		}

		CmnPrint(PT_VERBOSE, TEXT("[BC] Received Command = %u, DataSize = %u"), 
			CommandPacketHeader.dwCommand, CommandPacketHeader.dwDataSize);

		//
		// Receive any data that might be accompanying the command
		//
		if(CommandPacketHeader.dwDataSize > 0)
		{
			if(CommandPacketHeader.dwDataSize > MAX_COMMAND_DATA_SIZE)
			{
				CmnPrint(PT_FAIL, TEXT("[BC] Client trying to send too much data (%u bytes).  Closing this session."),
					CommandPacketHeader.dwDataSize);
				goto ExitThread;
			}

			// Allocate a buffer to receive the incomming data
			pbCommandData = (BYTE *)malloc(CommandPacketHeader.dwDataSize);

			if(!pbCommandData)
			{
				CmnPrint(PT_FAIL, TEXT("[BC] Out of memory.  Couldn't allocate enough space to hold command data."));
				goto ExitThread;
			}

			for(cbTotalBytesXfer = 0, cbBytesXfer = 1; 
				cbBytesXfer > 0 && (DWORD)cbTotalBytesXfer < CommandPacketHeader.dwDataSize;
				cbTotalBytesXfer += cbBytesXfer)
			{
				FD_ZERO(&fdSet);
				FD_SET(sock, &fdSet);
				iSelRet = select(0, &fdSet, NULL, NULL, &tvTimeout);

				if(iSelRet == SOCKET_ERROR)
				{
					CmnPrint(PT_FAIL, TEXT("[BC] select() failed, error = %s (%d)"),
						GetLastErrorText(), WSAGetLastError());
					goto ExitThread;
				}
				else if(iSelRet == 0)
				{
					CmnPrint(PT_WARN1, TEXT("[BC] Timed out while waiting for data from the client"));
					goto ExitThread;
				}

				cbBytesXfer = recv(sock, (char *) (pbCommandData + cbTotalBytesXfer),
					CommandPacketHeader.dwDataSize - cbTotalBytesXfer, 0);
				if (cbBytesXfer == SOCKET_ERROR)
				{
					CmnPrint(PT_FAIL, TEXT("[BC] recv(CommandData) failed %s (%d)"), GetLastErrorText(), WSAGetLastError());
					goto ExitThread;
				}
			}

			if(cbBytesXfer == 0 && cbTotalBytesXfer != CommandPacketHeader.dwDataSize)
			{
				CmnPrint(PT_FAIL, TEXT("[BC] Client did not send enough data before closing the connection."));
				goto ExitThread;
			}
			else
				dwCommandDataSize = CommandPacketHeader.dwDataSize;
		}

		CmnPrint(PT_LOG, TEXT("[BC] Processing command %d"), CommandPacketHeader.dwCommand);
		CmnPrint(PT_VERBOSE, TEXT("[BC] Command was accompanied by %d bytes of data"), CommandPacketHeader.dwDataSize);

		ResultPacketHeader.dwResult = ERROR_INVALID_FUNCTION;

		if(CommandPacketHeader.dwCommand == COMMAND_STOP_SERVICE)
		{
			// Special stop service command issued
			CmnPrint(PT_LOG, TEXT("[BC] Received STOP service command.  Service will shut down in a few seconds..."));
			if(pWorkerThreadData->hStopService)
				SetEvent(pWorkerThreadData->hStopService);
			fExit = TRUE;
			ResultPacketHeader.dwResult = ERROR_SUCCESS;
		}
		else
		{
			// Process the command
			for(i = 0; g_CommandHandlerArray[i].dwCommand != 0; i++)
			{
				// Search the Handler array for a matching command
				if(g_CommandHandlerArray[i].dwCommand == CommandPacketHeader.dwCommand)
				{
					CmnPrint(PT_VERBOSE, TEXT("[BC] Calling %s"), g_CommandHandlerArray[i].tszHandlerName);
					__try {
						// Found a matching command, call the handler function
						ResultPacketHeader.dwResult = g_CommandHandlerArray[i].pfHandlerFunction(sock, CommandPacketHeader.dwCommand, 
							pbCommandData, dwCommandDataSize, &pbReturnData, &dwReturnDataSize);
						break;
					} __except (1)
					{
						CmnPrint(PT_FAIL, TEXT("[BC] Exception in %s handler function.  Exiting worker thread!"), g_CommandHandlerArray[i].tszHandlerName);
						goto ExitThread;
					}
				}
			}

			if(g_CommandHandlerArray[i].dwCommand == 0)
				CmnPrint(PT_VERBOSE, TEXT("[BC] ERROR! Command %d is not supported."), CommandPacketHeader.dwCommand);
		}

		if(dwReturnDataSize > 0 && pbReturnData == NULL)
		{
			CmnPrint(PT_FAIL, TEXT("[BC] Handler Error - dwReturnDataSize > 0 and ReturnData == NULL"));
			goto ExitThread;
		}

		// Return the result
		ResultPacketHeader.dwDataSize = dwReturnDataSize;
		CmnPrint(PT_VERBOSE, TEXT("[BC] ResultPacketHeader.dwResult == %d."), ResultPacketHeader.dwResult);
		CmnPrint(PT_VERBOSE, TEXT("[BC] ResultPacketHeader.dwDataSize == %d."), ResultPacketHeader.dwDataSize);
		cbBytesXfer = send(sock, (char *)&ResultPacketHeader, sizeof(ResultPacketHeader), 0);
		if (cbBytesXfer == SOCKET_ERROR)
		{
			CmnPrint(PT_FAIL, TEXT("[BC] send(ResultPacketHeader) failed %s (%d)"), GetLastErrorText(), WSAGetLastError());
			goto ExitThread;
		}

		// Return any data
		if(dwReturnDataSize > 0)
		{
			cbBytesXfer = send(sock, (char *)pbReturnData, dwReturnDataSize, 0);
			if (cbBytesXfer == SOCKET_ERROR)
			{
				CmnPrint(PT_FAIL, TEXT("[BC] send(ReturnData) failed %s (%d)"), GetLastErrorText(), WSAGetLastError());
				goto ExitThread;
			}
		}

		CmnPrint(PT_VERBOSE, TEXT("[BC] Sent Result = %u, DataSize = %u"), 
			ResultPacketHeader.dwResult, ResultPacketHeader.dwDataSize);

		// Free any allocated buffers
		if(pbCommandData)
		{
			free(pbCommandData);
			pbCommandData = NULL;
		}

		if(pbReturnData)
		{
			free(pbReturnData);
			pbReturnData = NULL;
		}
	}

ExitThread:

	if(pWorkerThreadData->pfnCleanupFunc)
		(pWorkerThreadData->pfnCleanupFunc)(sock);

	if(pbCommandData)
		free(pbCommandData);

	if(pbReturnData)
		free(pbReturnData);

	// Close the socket
	if(sock != INVALID_SOCKET)
	{
		shutdown(sock, SD_BOTH);
		closesocket(sock);
	}

	if(pWorkerThreadData)
		free(pWorkerThreadData);

	return 0;
}
