//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
// OEMRasServer.exe: This daemon will run on the RAS server. It will accept 
// both TCP and UDP packets from the server.
//
// This exe does not take any parameters. The default values are:
// Packet Size: 1024
// Number of Packets per iteration: 100
//

#include <winsock.h>
#include <windows.h>

#include <stdio.h>

//
// Macros, constants etc.
//
#define DEFAULT_PACKET_SIZE 1024
#define DEFAULT_PORT		11122

//
// Function Prototypes
//
void TCPRunServer();
void UDPRunServer();
void OEMRasPrint(TCHAR *pFormat, ...);

//
// Main: Start of execution
//
#ifdef UNDER_CE
	int _tmain(int argc, TCHAR* argv[]) 
#else
	int main (int argc, char * argv[])
#endif
{
	WSADATA			WSAData;
    WORD			WSAVerReq = MAKEWORD(1,1);

    // Thread information
	DWORD  dwThreadIdTCP, dwThreadIdUDP;
	HANDLE  TCPThread, UDPThread;

	OEMRasPrint(TEXT("*** OEM RAS Stress ***"));
	
	if(WSAStartup(WSAVerReq, &WSAData) != 0)
	{
		OEMRasPrint(TEXT("WSAStartup Failed"));

		return 0;
	}

	//
	// Create threads here: 1) TCP receive, 2) UDP receive
	//
	if((TCPThread = 
		CreateThread(NULL, 0, 
		(LPTHREAD_START_ROUTINE) TCPRunServer,
		NULL, 0, &dwThreadIdTCP)) == NULL)
	{
		OEMRasPrint(TEXT("CreateThread(TCPThread) FAILED: %d"), GetLastError());
		
		goto Cleanup;
	}
	else 
	{
		OEMRasPrint(TEXT("CreateThread(TCPThread) PASSED"));	
	}

	if((UDPThread = 
		CreateThread(NULL, 0, 
		(LPTHREAD_START_ROUTINE) UDPRunServer,
		NULL, 0, &dwThreadIdUDP)) == NULL)
	{
		OEMRasPrint(TEXT("CreateThread(UDPThread) FAILED: %d"), GetLastError());
		
		goto Cleanup;
	}
	else
	{
		OEMRasPrint(TEXT("CreateThread(UDPThread) PASSED"));
	}
	
	WaitForSingleObject(TCPThread, INFINITE);
	WaitForSingleObject(UDPThread, INFINITE);

Cleanup:
	
	// Close all thread handles if open
	if(UDPThread)
		CloseHandle(UDPThread);

	if(TCPThread)
		CloseHandle(TCPThread);

	return 0;
}

void TCPRunServer()
{
	SOCKET			sockAccepted = INVALID_SOCKET, sock = INVALID_SOCKET;
	SOCKADDR_IN		sockAddr;
	int				iLen = sizeof(sockAddr);
	DWORD			dwBytesRead = 0;	
	int				iSockType		= SOCK_STREAM;
	WORD			wPort			= DEFAULT_PORT;
	DWORD			dwPacketSize	= DEFAULT_PACKET_SIZE;
	char			*pBuf;

	pBuf = (char *)LocalAlloc(LPTR, sizeof(char)*dwPacketSize);
	if (pBuf == NULL)
	{
		OEMRasPrint(TEXT("FAILED(pBuf) FAILED."));
		return;
	}

	if((sock = socket(AF_INET, iSockType, 0)) == INVALID_SOCKET) 
	{
		OEMRasPrint(TEXT("TCP socket() FAILED, error = %d"), WSAGetLastError());
		
		goto Cleanup;
	}
	else 
	{
		OEMRasPrint(TEXT("TCP socket() PASSED"));		
	}
	
	sockAddr.sin_port        = htons(wPort);
	sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sockAddr.sin_family      = AF_INET;
	
	if(bind(sock, (SOCKADDR *)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) 
	{
		OEMRasPrint(TEXT("bind() FAILED, error = %d"), WSAGetLastError());

		goto Cleanup;
	}
	else 
	{
		OEMRasPrint(TEXT("bind() PASSED"));		
	}

	// Listen to the server socket for client connections
	if (listen(sock, SOMAXCONN) == SOCKET_ERROR) 
	{
		OEMRasPrint(TEXT("listen() FAILED, error = %d"), WSAGetLastError());
		
		goto Cleanup;
	}
	else 
	{
		OEMRasPrint(TEXT("listen() PASSED"));		
	}
	
	while(1) 
	{
		memset(&sockAddr, 0, sizeof(sockAddr));
		sockAddr.sin_family = AF_INET;
		
		// Accept the next client connection
		if ((sockAccepted = accept(sock, (SOCKADDR *)&sockAddr, &iLen)) == INVALID_SOCKET)
		{
			OEMRasPrint(TEXT("accept() FAILED, error = %d"), WSAGetLastError());

			continue;
		}
		else 
		{
			OEMRasPrint(TEXT("accept() PASSED"));		
		}
		
		// Start Receive Loop
		while(1) 
		{
			// Get incoming packets
			if ((dwBytesRead = recv(sockAccepted, pBuf, dwPacketSize, 0)) == SOCKET_ERROR) 
			{
				OEMRasPrint(TEXT("recv() FAILED, error = %d"), WSAGetLastError());
				
				break;
			}
			
			// if the connection closed, we're done with this client
			if(dwBytesRead == 0) 
			{
				break;
			}
			
			OEMRasPrint(TEXT("recv()'d %d bytes"), dwBytesRead);
		}
		
		// close the sockets
		shutdown(sockAccepted, 0x02);
		closesocket(sockAccepted);
	}
	
Cleanup:
	LocalFree(pBuf);
	closesocket(sock);
}

void UDPRunServer()
{
	char 			*pBuf;
	SOCKET			sockAccepted = INVALID_SOCKET, sock = INVALID_SOCKET;
	SOCKADDR_IN		sockAddr;
	int				dwPacketSize=DEFAULT_PACKET_SIZE;
	int				cbAddrSize, cbBytesRead;

	pBuf = (char *)LocalAlloc(LPTR, sizeof(char)*dwPacketSize);
	if (pBuf == NULL)
	{
		OEMRasPrint(TEXT("LocalAlloc(pBuf) FAILED."));
		return;
	}
	
	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) 
	{
		OEMRasPrint(TEXT("socket() FAILED, error = %d"), WSAGetLastError());
		
		return;
	}
	
	sockAddr.sin_family      = AF_INET;
	sockAddr.sin_port        = htons(DEFAULT_PORT);
	sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if(bind(sock, (SOCKADDR *)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) 
	{
		OEMRasPrint(TEXT("bind() FAILED, error = %d"), WSAGetLastError());

		closesocket(sock);
		return;
	}
	
	while(1) 
	{
		// Get incoming packets
		cbAddrSize = sizeof(sockAddr);
		cbBytesRead = recvfrom(sock, pBuf, dwPacketSize, 0, (SOCKADDR *)&sockAddr, &cbAddrSize);

		if (cbBytesRead == SOCKET_ERROR)
			OEMRasPrint(TEXT("recvfrom() FAILED, error = %d"), WSAGetLastError());
		else
			OEMRasPrint(TEXT("recvfrom() got %d bytes."), cbBytesRead);
	}
	
	closesocket(sock);
	LocalFree(pBuf);
}

//
// Print Utility
//
void OEMRasPrint(TCHAR *pFormat, ...)
{
	va_list ArgList;
	TCHAR	Buffer[256];

	va_start (ArgList, pFormat);

	(void)wvsprintf (Buffer, pFormat, ArgList);

#ifndef UNDER_CE
	_putws(Buffer);
#else
	OutputDebugString(Buffer);
#endif

	va_end(ArgList);
}
