//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <winsock2.h>
#include <stdio.h>
#include <malloc.h>
#include <ws2bth.h>
#include <tux.h>
#include "util.h"

// WAIT_IO_COMPLETION is not in CE's winbase.h
#ifndef WAIT_IO_COMPLETION
#define WAIT_IO_COMPLETION                  STATUS_USER_APC
#endif

/*    Test Functions     */
extern int  DataChunkTst(int tot_bytes, int chunks);
int ListenTst(int backlog);

/*  Local Test functions */
BOOL CreateSocketInformation(SOCKET s);
void FreeSocketInformation(DWORD Index);

//int checkflag = false;

extern int PORT1;
extern int PORT2;
extern int PORT3;
extern int PORT4;

/*----------------------------------------   Connection Mgmt Tests ----------------------------------------*/

/* Listen call Test */
int ListenFunctionalTst(int port, int backlog)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	bool TstResult;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Bind(&sock, port);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, backlog);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server, port);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

//
//TestId        : 1.2.8
//Title         : Connection management test (backlog = 0)
//Description   : Listen with a backlog = 0 and connect() from peer
//                
//
TESTPROCAPI f1_2_8(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//Listen with backlog 0 and connect on PORT2
    return ListenFunctionalTst(PORT2, 0);
}

//
//TestId        : 1.2.9
//Title         : Connection management test (backlog = SOMAXCONN)
//Description   : Listen with a backlog = SOMAXCONN and connect() from peer
//                
//
TESTPROCAPI f1_2_9(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//Listen with backlog SOMAXCONN and connect on PORT3
    return ListenFunctionalTst(PORT3, SOMAXCONN);
}

//
//TestId        : 1.2.10
//Title         : Connection management test (backlog = 8)
//Description   : Listen with a backlog = 8 and connect() from peer
//                
//
TESTPROCAPI f1_2_10(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//Listen with backlog 8 and connect on PORT1
    return ListenFunctionalTst(PORT1, 8);
}

//
//TestId        : 1.2.11
//Title         : Connection management test (backlog = SOMAXCONN + 1)
//Description   : Listen with a backlog = SOMAXCONN + 1 and connect() from peer
//                
//
TESTPROCAPI f1_2_11(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	TEST_ENTRY;

	//Listen with backlog 8 and connect on PORT2
    return ListenFunctionalTst(PORT2, (int) ((DWORD)SOMAXCONN + (DWORD)1));
}

//
//TestId        : 1.2.16
//Title         : Get Peer Name Test 
//Description   : getpeername() call test
//                
//
TESTPROCAPI f1_2_16(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT3);
		if (retcode == FAIL)
		{	
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = GetPeerName(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave

	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		//Sleep for 0.5 secs.

		Sleep(500);

		retcode = Connect(&sock, &server,PORT3);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = GetPeerName(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

//
//TestId        : 1.2.17
//Title         : Get Sock Name Test 
//Description   : getsockname() call test
//                
//
TESTPROCAPI f1_2_17(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT1);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = GetSockName(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave

	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Sleep for 0.5 secs.

		Sleep(500);

		retcode = Connect(&sock, &server,PORT1);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = GetSockName(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

//
//TestId        : 1.5.6
//Title         : Remote Disconnect Notification Test
//Description   : Disconnect the listener with "hard" close on socket, and check if the client
//                gets the disconnect notification.
//
TESTPROCAPI f1_5_6(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	int tot_bytes = 1000;
	int chunks = 10;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{	
				TstResult = false;
				goto FailExit;
		}

		retcode = Bind(&sock,PORT2);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&sock, 5);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,PORT2);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		Sleep(5000);

		//Now this call should fail
		retcode = Write(&sock, tot_bytes , chunks);
		if (retcode == SUCCESS)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

	}
    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

/*----------------------------------------   IO Model Tests ----------------------------------------*/

//
//Title         : IO Model Client
//Description   : Used by all IO Model tests by infinfo->Mode = SLAVE. The IO Clients behave as an
//                echo client - which sent a series of packets and expect the same returned back by
//                the server.
//
int IOModelClient(int port)
{
	SOCKET sock = 0;//Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	int tot_bytes = 1000;
	int chunks = 1000;
	int i = 0;
	int retcode;
	bool TstResult;

	//Initialize TstResult for each Test
	TstResult = true;

	retcode = Socket(&sock);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	//Sleep for 0.5 secs.

	Sleep(500);

	retcode = Connect(&sock, &server,port);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

    for(i=0; i < 10; ++i)
	{
		retcode = Write(&sock, tot_bytes, chunks);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Read(&sock, tot_bytes);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}

	retcode = Closesocket(&sock);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }

	return TPR_FAIL;
}

#define DATA_BUFSIZE 8192

typedef struct _SOCKET_INFORMATION {
   CHAR Buffer[DATA_BUFSIZE];
   WSABUF DataBuf;
   SOCKET Socket;
   OVERLAPPED Overlapped;
   DWORD BytesSEND;
   DWORD BytesRECV;
} SOCKET_INFORMATION, * LPSOCKET_INFORMATION;

DWORD TotalSockets = 0;
LPSOCKET_INFORMATION SocketArray[FD_SETSIZE];

//
//TestId        : 2.5
//Title         : Synchronous Select Test
//Description   : This IO Model Test implements an "echo" server using Sync - Select Model. The server
//                waits for a connection from a client, and spews back all packet it receives from the
//                client.
//				  CE does not support non-blocking socket
//
TESTPROCAPI f2_5(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET ListenSocket = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET AcceptSocket = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH client;
	FD_SET WriteSet;
	FD_SET ReadSet;
	DWORD i;
	DWORD Total;
	ULONG NonBlock;
	DWORD Flags;
	DWORD SendBytes;
	DWORD RecvBytes;
	int retcode;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	//If the mode on current node is MASTER
	if(g_bIsServer)
	{
	   
		// Prepare a socket to listen for connections.
		retcode = WSA_Socket(&ListenSocket);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Bind(&ListenSocket,PORT1);
		if (retcode == FAIL)
		{	
				TstResult = false;
				goto FailExit;
		}

		//Pass backlog 
		retcode = Listen(&ListenSocket, 5);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		// Change the socket mode on the listening socket from blocking to
		// non-block so the application will not block waiting for requests.
		NonBlock = 1;
		retcode = Ioctlsocket(&ListenSocket, FIONBIO, NonBlock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
 
		bool Terminate_Flag = false;

		while(true)
		{
			// Prepare the Read and Write socket sets for network I/O notification.
			FD_ZERO(&ReadSet);
			FD_ZERO(&WriteSet);

			// Always look for connection attempts.

			FD_SET(ListenSocket, &ReadSet);

			// Set Read and Write notification for each socket based on the
			// current state the buffer.  If there is data remaining in the
			// buffer then set the Write set otherwise the Read set.

			for (i = 0; i < TotalSockets; i++)
				if (SocketArray[i]->BytesRECV > SocketArray[i]->BytesSEND)
										FD_SET(SocketArray[i]->Socket, &WriteSet);
			else
				FD_SET(SocketArray[i]->Socket, &ReadSet);

			int i;
			for(i=0; i <= 1000; ++i);

			if ((Total = select(0, &ReadSet, &WriteSet, NULL, NULL)) == SOCKET_ERROR)
			{
				OutStr(TEXT("select() returned with error %d\n"), WSAGetLastError());

				TstResult = false;
				goto FailExit;
			}

			OutStr(TEXT("\t>> ...Select Succeeded\n"));

			// Check for arriving connections on the listening socket.
			if (FD_ISSET(ListenSocket, &ReadSet))
			{
				OutStr(TEXT("...FD_ISSET() Succeeded on ReadSet\n"));
				Total--;
				retcode = Accept( &ListenSocket, &AcceptSocket, &client);
				if (retcode == FAIL)
				{	
					TstResult = false;
					goto FailExit;
				}

				// Set the accepted socket to non-blocking mode so the server will
				// not get caught in a blocked condition on WSASends

				NonBlock = 1;
				retcode = Ioctlsocket(&AcceptSocket, FIONBIO, NonBlock);
				if (retcode == FAIL)
				{
					TstResult = false;
					goto FailExit;
				}

				if (CreateSocketInformation(AcceptSocket) == FALSE)
				{
					TstResult = false;
					goto FailExit;
				}
			}

			// Check each socket for Read and Write notification until the number
			// of sockets in Total is satisfied.

			for (i = 0; Total > (DWORD) 0 && (DWORD) i < TotalSockets; i++)
			{
				LPSOCKET_INFORMATION SocketInfo = SocketArray[i];

				// If the ReadSet is marked for this socket then this means data
				// is available to be read on the socket.

				if (FD_ISSET(SocketInfo->Socket, &ReadSet))
				{
					Total--;

					SocketInfo->DataBuf.buf = SocketInfo->Buffer;
					SocketInfo->DataBuf.len = DATA_BUFSIZE;

					Flags = 0;
					if (WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes,
					&Flags, NULL, NULL) == SOCKET_ERROR)
					{
						if (WSAGetLastError() != WSAEWOULDBLOCK)
						{
							OutStr(TEXT("\t>> WSARecv() failed with error %d\n"), WSAGetLastError());
							FreeSocketInformation(i);
						}

						continue;
					} 
					else
					{
						OutStr(TEXT("\t>> WSARecv() ... Bytes received  : %d\n"), RecvBytes);

						SocketInfo->BytesRECV = RecvBytes;

						// If zero bytes are received, this indicates the peer closed the
						// connection.
						if (RecvBytes == 0)
						{
							FreeSocketInformation(i);
							Terminate_Flag = true;
							break;
						}
					}	
				}

				// If the WriteSet is marked on this socket then this means the internal
				// data buffers are available for more data.

				if (FD_ISSET(SocketInfo->Socket, &WriteSet))
				{
					Total--;

					SocketInfo->DataBuf.buf = SocketInfo->Buffer + SocketInfo->BytesSEND;
					SocketInfo->DataBuf.len = SocketInfo->BytesRECV - SocketInfo->BytesSEND;

					if (WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, 0,
					NULL, NULL) == SOCKET_ERROR)
					{
						if (WSAGetLastError() != WSAEWOULDBLOCK)
						{
							OutStr(TEXT("\t>> WSASend() failed with error %d\n"), WSAGetLastError());

							FreeSocketInformation(i);
						}

					continue;
					}
					else
					{
						SocketInfo->BytesSEND += SendBytes;

						if (SocketInfo->BytesSEND == SocketInfo->BytesRECV)
						{
							SocketInfo->BytesSEND = 0;
							SocketInfo->BytesRECV = 0;
						}
					} // else
				}//if
		 
			}//for
	  
			if (Terminate_Flag)
			{
				retcode = Closesocket(&ListenSocket);
				if (retcode == FAIL)
				{
					TstResult = false;
					goto FailExit;
				}

				break;
			}
		}//while

	} /* End of Master node */
	else 
	{
		return IOModelClient(PORT1);
	} /* End of slave node */

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (ListenSocket)
    {
        Closesocket(&ListenSocket);
    }
    if (AcceptSocket)
    {
        Closesocket(&AcceptSocket);
    }

	return TPR_FAIL;  
}

//
//TestId        : 3.1.1
//Title         : Socket Options Test - Test for SO_ACCEPTCONN
//Description   : Test for SO_ACCEPTCONN on listening socket
//
TESTPROCAPI f3_1_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	int retcode;
	BOOL iVal;
	int ilen;
	bool TstResult;

	TEST_ENTRY;

    //Initialize TstResult for each Test
    TstResult = true; 

    retcode = Socket(&sock);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	retcode = Bind(&sock,PORT2);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	//Pass backlog 
	retcode = Listen(&sock, 5);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	ilen = sizeof(iVal);
    retcode = getsockopt(sock, SOL_SOCKET, SO_ACCEPTCONN, (char *) &iVal, &ilen);

    if (retcode == SOCKET_ERROR)
    {
        OutStr(TEXT("getsockopt(SO_ACCEPTCONN) failed: %d\n"), WSAGetLastError());
		TstResult = false;
        goto FailExit;
    }
    if (iVal) 
	{
		OutStr(TEXT("TRUE : Listen Socket ... SO_ACCEPTCONN - Test passed\n"));
	}
	else {
		OutStr(TEXT("FALSE : Listen Socket ... SO_ACCEPTCONN - Test failed\n"));
	    TstResult = false;
	    goto FailExit;

	}

    retcode = Closesocket(&sock);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }

	return TPR_FAIL;
}

//
//TestId        : 3.1.2
//Title         : Socket Options Test - Test for SO_ACCEPTCONN
//Description   : Test for SO_ACCEPTCONN on non-listening socket
//
TESTPROCAPI f3_1_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	int retcode;
	BOOL iVal;
	int ilen;
	bool TstResult;

	TEST_ENTRY;

    //Initialize TstResult for each Test
    TstResult = true; 

    retcode = Socket(&sock);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	retcode = Bind(&sock,PORT3);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	ilen = sizeof(iVal);
    retcode = getsockopt(sock, SOL_SOCKET, SO_ACCEPTCONN, (char *) &iVal, &ilen);

    if (retcode == SOCKET_ERROR)
    {
        OutStr(TEXT("getsockopt(SO_ACCEPTCONN) failed: %d\n"), WSAGetLastError());
		TstResult = false;
		goto FailExit;
    }

    if (iVal) 
	{
		OutStr(TEXT("TRUE : Non Listen Socket ... SO_ACCEPTCONN Test Failed\n"));
		TstResult = false;
		goto FailExit;
	}
	else {
		OutStr(TEXT("FALSE : Non Listen Socket ... SO_ACCEPTCONN Test Passed\n"));
		OutStr(TEXT("[Test Passed]\n"));
	}

    retcode = Closesocket(&sock);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }

	return TPR_FAIL;
}

//
//TestId        : 3.2
//Title         : Socket Options Test - Test for SO_TYPE
//Description   : Test for SO_TYPE on socket
//
TESTPROCAPI f3_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	int retcode;
	SOCKET sock;
	int iVal;
	int ilen;
	bool TstResult;

	TEST_ENTRY;

	//Initialize TstResult for each Test
	TstResult = true; 

    retcode = Socket(&sock);
    if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

	ilen = sizeof(iVal);
    retcode = getsockopt(sock, SOL_SOCKET, SO_TYPE, (char *) &iVal, &ilen);

    if (retcode == SOCKET_ERROR)
    {
        OutStr(TEXT("getsockopt(SO_TYPE) failed: %d\n"), iVal);
 		TstResult = false;
		goto FailExit;
    }
 
    if (iVal == SOCK_STREAM)
	{
        OutStr(TEXT("[Test Passed]: SO_TYPE = SOCK_STREAM\n")); 
	}
    else if (iVal == SOCK_DGRAM)
	{
       OutStr(TEXT("[Test Failed]: SO_TYPE = SOCK_DGRAM\n")); 
	   TstResult = false;
	   goto FailExit;
	}
    else if (iVal == SOCK_RDM)
	{
       OutStr(TEXT("[Test Failed]: SO_TYPE = SOCK_RDM\n"));
	   TstResult = false;
	   goto FailExit;
	}
    else if (iVal == SOCK_SEQPACKET)
	{
       OutStr(TEXT("[Test Failed]: SO_TYPE = SOCK_SEQPKACKET\n")); 
	   TstResult = false;
	   goto FailExit;
	}
    else if (iVal == SOCK_RAW)
	{
       OutStr(TEXT("[Test Failed]: SO_TYPE = SOCK_RAW\n")); 
	   TstResult = false;
	   goto FailExit;
	}
    else
	{
       OutStr(TEXT("[Test Failed]: SO_TYPE = Unknown\n"));
	   TstResult = false;
	   goto FailExit;
	}

    retcode = Closesocket(&sock);
	if (retcode == FAIL)
	{
	   TstResult = false;
	   goto FailExit;
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }

	return TPR_FAIL;
}

//
//TestId        : 3.4.1 
//Title         : Socket Options Test - Test for SO_LINGER with getsockopt
//Description   : Test for SO_LINGER with getsockopt()
//
TESTPROCAPI f3_4_1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	struct linger {
		u_short l_onoff;
		u_short l_linger;
	};
	int ilen;
	struct linger iVal;
	bool TstResult;

	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	if(g_bIsServer)
	{
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT2);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		ilen = sizeof(iVal);
		retcode = getsockopt(sock, SOL_SOCKET, SO_LINGER, (char *) &iVal, &ilen);

		if (retcode == SOCKET_ERROR)
		{
				OutStr(TEXT("getsockopt(SO_LINGER) failed: %d\n"), WSAGetLastError());
				TstResult = false;
				goto FailExit;
		}

		if (iVal.l_onoff)
		{
			OutStr(TEXT("\t>> Lingering is on\n"));
		} else {
			OutStr(TEXT("\t>> Lingering is off\n"));
	
		}

		//Pass backlog 
		retcode = Listen(&sock, 0);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,PORT2);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
				TstResult = false;
				goto FailExit;
		}
	}

    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS;
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

//
//TestId        : 3.4.2 
//Title         : Socket Options Test - Test for SO_LINGER with setsockopt
//Description   : Test for SO_LINGER with setsockopt()
//
TESTPROCAPI f3_4_2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	SOCKET sock = 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKET csock= 0; //Initialize sock = 0 to take care for the FailExit code to work properly
	SOCKADDR_BTH server;
	SOCKADDR_BTH client;
	int retcode;
	struct linger {
		u_short l_onoff;
		u_short l_linger;
	};
	int ilen, slen;
	struct linger iVal, sVal;
	bool TstResult;
  
	TEST_ENTRY;

	if (g_bNoServer)
	{
		return TPR_SKIP;
	}

	//Initialize TstResult for each Test
	TstResult = true; 

	if(g_bIsServer)
   {
		retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Bind(&sock,PORT4);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		ilen = sizeof(iVal);
		iVal.l_onoff = 1;
		iVal.l_linger = 10; //timeout secs

		retcode = setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *) &iVal, ilen);

		if (retcode == SOCKET_ERROR)
		{
			OutStr(TEXT("setsockopt(SO_LINGER) failed: %d\n"), iVal);

			TstResult = false;
			goto FailExit;
		}

		slen = sizeof(sVal);
	
		retcode = getsockopt(sock, SOL_SOCKET, SO_LINGER, (char *) &sVal, &slen);

		if (retcode == SOCKET_ERROR)
		{
			OutStr(TEXT("getsockopt(SO_LINGER) failed: %d\n"), iVal);
			TstResult = false;
			goto FailExit;
		}

		if (sVal.l_onoff)
		{
			OutStr(TEXT("\t>> Lingering is on\n"));
		} else {
			OutStr(TEXT("\t>> Lingering is off\n"));
	
		}

		//Pass backlog 
		retcode = Listen(&sock, 0);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
		
		retcode = Accept(&sock, &csock, &client);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&csock);
		if (retcode == FAIL)
		{	
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}
	else 
	{
		//Mode is Slave
	  	retcode = Socket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		//Sleep for 0.5 secs.
		Sleep(500);

		retcode = Connect(&sock, &server,PORT4);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}

		retcode = Closesocket(&sock);
		if (retcode == FAIL)
		{
			TstResult = false;
			goto FailExit;
		}
	}
    
    //Check if the test passed
	if (TstResult)
	{
		return TPR_PASS; //Test passed
	} 

FailExit:
	//Handling the failure case.
    if (sock)
    {
        Closesocket(&sock);
    }
    if (csock)
    {
        Closesocket(&csock);
    }

	return TPR_FAIL;
}

BOOL CreateSocketInformation(SOCKET s)
{
   LPSOCKET_INFORMATION SI;
      
   OutStr(TEXT("Accepted socket number %d\n"), s);

   if ((SI = (LPSOCKET_INFORMATION) GlobalAlloc(GPTR,
      sizeof(SOCKET_INFORMATION))) == NULL)
   {
      OutStr(TEXT("GlobalAlloc() failed with error %d\n"), GetLastError());
      return FALSE;
   }

   // Prepare SocketInfo structure for use.

   SI->Socket = s;
   SI->BytesSEND = 0;
   SI->BytesRECV = 0;

   SocketArray[TotalSockets] = SI;

   TotalSockets++;

   return(TRUE);
}

void FreeSocketInformation(DWORD Index)
{
   LPSOCKET_INFORMATION SI = SocketArray[Index];
   DWORD i;
   int retcode;

   retcode = Closesocket(&SI->Socket);
   if (retcode == FAIL)
   {
	   OutStr(TEXT("[Test Failed]\n"));
   }

   OutStr(TEXT("Closing socket number %d\n"), SI->Socket);

   GlobalFree(SI);

   // Squash the socket array

   for (i = Index; i < TotalSockets; i++)
   {
      SocketArray[i] = SocketArray[i + 1];
   }

   TotalSockets--;
}
