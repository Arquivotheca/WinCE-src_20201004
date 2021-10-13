//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include "netmain.h"

#ifdef UNDER_CE
#include <types.h>
#include <memory.h>
#else
#include <assert.h>
#include <tchar.h>
#endif

#define TIMEOUT_SECONDS				10
#define MEG_SIZE					1048576		//  1024 * 1024
#define MAX_PACKET_LEN				1024
#define UPDATE_INTERVAL				50000
#define DEFAULT_PORT				_T("40000")
#define DEFAULT_HOST				_T("localhost")


DWORD WINAPI SendThread(LPVOID *pParm);
DWORD WINAPI RecvThread(LPVOID *pParm);
ULONG BuildTestPacket(DWORD Len, BYTE Buff[], int nPacketNum);

TCHAR* gszMainProgName = _T("rsstres2_vX");
DWORD  gdwMainInitFlags = INIT_NET_CONNECTION;

void PrintUsage()
{
	QAMessage(TEXT("rsstres2_vX [-cIPAddress] [-bBytes] [-wMSWait] [-srutv] [-mTotBytesLimit]"));
	QAMessage(TEXT("c:  Specify the server to communicate with. (default: localhost)"));
	QAMessage(TEXT("p:  Set port to use (default %s)"), DEFAULT_PORT);
	QAMessage(TEXT("b:  Specify number of bytes per send.    (randomly determined for each send if not specified)."));
	QAMessage(TEXT("m:  Set total number of MBytes to transmit. (default: 1)"));
	QAMessage(TEXT("w:  Time to wait (in milliseconds) between sends/recvs."));
	QAMessage(TEXT("s:  Run send thread"));
	QAMessage(TEXT("r:  Run receive thread"));
	QAMessage(TEXT("u:  Send and/or receive over datagram (UDP) sockets. [TCP is default]"));
	QAMessage(TEXT("l:  Set max time for select to wait in seconds. (default: %d)"), TIMEOUT_SECONDS);
	QAMessage(TEXT("e:  Set to verbose mode... data recv/send is printed to display."));
	QAMessage(TEXT("d:  Set to Deamon mode...  recv thread is not closed upon client exit."));
	QAMessage(TEXT("i:  Version of IP to use 4, 6, or X (default: X)"));
}

//
// Global Variables
//
LPTSTR g_szServer;
LPTSTR g_szPort;
int g_iSockType;
DWORD g_dwMinBytesPerSend, g_dwMaxBytesPerSend;
DWORD g_dwMegsToXfer;
DWORD g_dwWaitTime;
BOOL g_bDaemon;
BOOL g_bVerbose;
int g_iFamily;
DWORD g_dwSelectTimeout;

char g_cNextChar = 'a';

#define SZ_AI_FAMILY ((pAI->ai_family == PF_INET) ? _T("PF_INET") : _T("PF_INET6"))

//**********************************************************************************
extern "C"
int mymain(int argc, TCHAR* argv[])
{
    WSADATA		WSAData;	
    HANDLE		hThreadSend = NULL;
    HANDLE		hThreadRecv = NULL;
    DWORD		dwThreadId;
    WORD		WSAVerReq = MAKEWORD(1,1);

	BOOL		bRecv, bSend;
	LPTSTR		szIPVersion;

	if(QAWasOption(_T("?")) || QAWasOption(_T("help")))
	{
		PrintUsage();
		return 0;
	}

	// Set up defaults
	g_szServer = DEFAULT_HOST;
	g_szPort = DEFAULT_PORT;
	g_iSockType = SOCK_STREAM;
	g_iFamily = PF_UNSPEC;
	g_dwMinBytesPerSend = 0;
	g_dwMaxBytesPerSend = MAX_PACKET_LEN;
	g_dwMegsToXfer = 1;
	g_dwWaitTime = 0;
	g_bDaemon = FALSE;
	g_bVerbose = FALSE;
	g_dwSelectTimeout = TIMEOUT_SECONDS;
	bRecv = FALSE;
	bSend = FALSE;


	QAGetOption(_T("c"), &g_szServer);
	QAGetOption(_T("p"), &g_szPort);
	QAGetOptionAsDWORD(_T("m"), &g_dwMegsToXfer);
	QAGetOptionAsDWORD(_T("l"), &g_dwSelectTimeout);

	QAMessage(TEXT("Server = %s.  Port = %s"), g_szServer, g_szPort);
	QAMessage(TEXT("Megabytes to Xfer = %u"), g_dwMegsToXfer);
	QAMessage(TEXT("SelectTimeout = %u"), g_dwSelectTimeout);

	QAGetOptionAsDWORD(_T("w"), &g_dwWaitTime);
	if(QAWasOption(_T("w")))
		QAMessage(TEXT("Wait between sends ON: delay = %dms"), g_dwWaitTime);

	QAGetOptionAsDWORD(_T("b"), &g_dwMaxBytesPerSend);
	if(QAWasOption(_T("b")))
	{
		g_dwMinBytesPerSend = g_dwMaxBytesPerSend;
		QAMessage(TEXT("Bytes Per Send: %d"), g_dwMaxBytesPerSend);
	}

	if(QAWasOption(_T("u")))
	{
		g_iSockType = SOCK_DGRAM;
		QAMessage(TEXT("Using Protocol: UDP"));
	}
	else
		QAMessage(TEXT("Using Protocol: TCP"));

	if(QAWasOption(_T("r")))
	{
		bRecv = TRUE;
		QAMessage(TEXT("Recv Thread ON"));
	}

	if(QAWasOption(_T("s")))
	{
		bSend = TRUE;
		QAMessage(TEXT("Send Thread ON"));
	}

	if(QAWasOption(_T("d")))
	{
		g_bDaemon = TRUE;
		QAMessage(TEXT("Daemon mode ON"));
	}

	if(QAWasOption(_T("e")))
	{
		g_bVerbose = TRUE;
		QAMessage(TEXT("Verbose mode ON"));
	}

	if(QAGetOption(_T("i"), &szIPVersion))
	{
		switch(szIPVersion[0])
		{
			case _T('4'):
				g_iFamily = PF_INET;
				break;

			case _T('6'):
				g_iFamily = PF_INET6;
				break;

			default:
				g_iFamily = PF_UNSPEC;
				break;
		}
	}

	
    if(WSAStartup(WSAVerReq, &WSAData) != 0)
	{
		QAError(TEXT("WSAStartup Failed"));
		return FALSE;
	}
    else
	{
        QAMessage(TEXT("WSAStartup SUCCESS"));
    }

	if(bRecv)
	{
		QAMessage (TEXT("Creating RecvThread"));

		if ((hThreadRecv = 
				CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) RecvThread,
				NULL, 0, &dwThreadId)) == NULL)
		{
			QAError (TEXT("CreateThread(TCPRecvThread) failed %d"),
				GetLastError());
				
			return FALSE;
		}
	}

	if(bSend)
	{
		if(bRecv && g_iSockType == SOCK_DGRAM)
			Sleep(1000);	// Give the receive thread a chance to get started.

		QAMessage(TEXT("Creating SendThread"));

		if((hThreadSend = 
				CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) SendThread,
				NULL, 0, &dwThreadId)) == NULL)
		{
			QAError (TEXT("CreateThread(TCPSendThread) failed %d"),
				GetLastError());
				
			return FALSE;
		}
	}


	QAMessage(TEXT("Waiting for completion."));

	if (bSend && hThreadSend)
	{
		WaitForSingleObject(hThreadSend, INFINITE);
		CloseHandle(hThreadSend);
		QAMessage(TEXT("SendThread:  Exit "));
	}

	if (bRecv && hThreadRecv)
	{
		WaitForSingleObject(hThreadRecv, INFINITE);
		CloseHandle(hThreadRecv);
		QAMessage(TEXT("RcvThread:  Exit "));
	}

	WSACleanup();

	return TRUE;

}

//**********************************************************************************
DWORD WINAPI SendThread(LPVOID *pParm)
{
	BYTE *pBuf = NULL;
	SOCKET sock = INVALID_SOCKET;
	DWORD dwIPAddr = INADDR_ANY;
	DWORD dwLen, dwPktCount, dwFirstTickCount, dwNewTickCount;
	DWORD dwCurrBytesSent, dwTotalBytesSent, dwLastBytesSentCount, dwMegCounter;
	int iTimeoutCnt, iSelRet, iBytesSent;
	fd_set fdSet;
	TIMEVAL selTimeOut = { g_dwSelectTimeout, 0 };
	char szServerASCII[256];
	ADDRINFO AddrHints, *pAddrInfo, *pAI;
	char szPortASCII[10];

	QAMessage(TEXT("SendThread++"));

	// Allocate the packet buffer
	pBuf = (BYTE *) LocalAlloc(LPTR, g_dwMaxBytesPerSend);

	if(pBuf == NULL)
	{
		QAError(TEXT("Can't allocate enough memory for packet."));
		goto exitThread;
	}

#if defined UNICODE
	wcstombs(szServerASCII, g_szServer, sizeof(szServerASCII));
	wcstombs(szPortASCII, g_szPort, sizeof(szPortASCII));
#else
	strncpy(szServerASCII, g_szServer, sizeof(szServerASCII));
	strncpy(szPortASCII, g_szPort, sizeof(szPortASCII));
#endif
	
	memset(&AddrHints, 0, sizeof(AddrHints));
	AddrHints.ai_family = g_iFamily;
	AddrHints.ai_socktype = g_iSockType;

	// Use IPvX name resolution
	if(getaddrinfo(szServerASCII, szPortASCII, &AddrHints, &pAddrInfo))
	{
		QAError (TEXT("Invalid address parameter = %s"), g_szServer);
		goto exitThread;
	}
	
	for(pAI = pAddrInfo; pAI != NULL; pAI = pAI->ai_next)
	{
		sock = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
		if (sock == INVALID_SOCKET)
			QAWarning(TEXT("socket() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
		else 
		{
			if(pAI->ai_socktype == SOCK_STREAM)
			{
				if(connect(sock, pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
				{
					QAWarning(TEXT("connect() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
					continue;
				}

				QAMessage(TEXT("connect()'d successfully via family %s"), SZ_AI_FAMILY);
			}
			break;
		}
	}

	if(pAI == NULL)
	{
		QAError(TEXT("unable to connect to the server"));
		goto exitThread;
	}
	
	dwPktCount = 0;
	iBytesSent = 0;
	iTimeoutCnt = 0;
	dwMegCounter = 0;
	dwLastBytesSentCount = dwCurrBytesSent = dwTotalBytesSent = 0;
	dwFirstTickCount = GetTickCount();

	// Send the data
	while (dwMegCounter < g_dwMegsToXfer)
	{
		dwLen = GetRandomNumber(g_dwMaxBytesPerSend - g_dwMinBytesPerSend) + g_dwMinBytesPerSend;

		BuildTestPacket(dwLen, pBuf, dwPktCount);
		dwPktCount = (DWORD)(dwPktCount + 1);
		
		for(int i = 0; i < 3; i++)
		{
			FD_ZERO(&fdSet);
			FD_SET(sock, &fdSet);
			iSelRet = select(0, NULL, &fdSet, NULL, &selTimeOut);

			if((iSelRet == SOCKET_ERROR) || (iSelRet == 0))
			{
				QAWarning(TEXT("select(write) timed out %d"), iSelRet); 
				if (i == 2)
				{
					QAError(TEXT("timed out %d times... exiting (%d)"), 3, iSelRet);
					goto exitThread;
				}
			}
			else
				break;
		}
		
		if(g_iSockType == SOCK_STREAM)
		{
			if ((iBytesSent = send(sock, (char *) pBuf, dwLen, 0)) == SOCKET_ERROR)
			{
				QAError(TEXT("send() failed %d = %s"), WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}
		}
		else
		{
			if ((iBytesSent = sendto(sock, (char *) pBuf, dwLen, 0, pAI->ai_addr, pAI->ai_addrlen)) == SOCKET_ERROR)
			{
				QAError(TEXT("sendto() failed %d = %s"), WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}
		}

		if ((DWORD) iBytesSent != dwLen)
		{
			QAError(TEXT("send() indicated wrong number of bytes sent %d instead of %d"), iBytesSent, dwLen);
			goto exitThread;
		}

		//QAMessage(TEXT("sent %d bytes"), iBytesSent));

		// We use two counters here dwCurrBytesSent and dwTotalBytesSent due to a conflict
		// of interest.  In order to have the test handle extremely large numbers of megs
		// dwCurrBytesSent is kept under 1 meg (1048576) and used to monitor the current meg
		// Xfer'd total.
		//
		// dwTotalBytesSent is continously incremented and may actually rollover if the megs
		// Xfer'd gets too high.  However its use is localized to the timing of data transfer'd
		// so it won't make a big difference.
		//
		// We could have used just dwCurrBytesSent and simply replaced dwTotalBytesSent with 
		// dwCurrBytesSent + (MEG_SIZE * dwMegCounter).  But that would have require a lot
		// of calculations for each send (processor overhead), which we avoid here.

		dwTotalBytesSent = (DWORD)(dwTotalBytesSent + iBytesSent);
		dwCurrBytesSent = (DWORD)(dwCurrBytesSent + iBytesSent);

		if((dwTotalBytesSent - dwLastBytesSentCount) > UPDATE_INTERVAL)
		{
			dwNewTickCount = GetTickCount();
			
			if(((DWORD)(dwNewTickCount - dwFirstTickCount) >= 1000) && g_bVerbose)
				QAMessage(TEXT("Sending data at %d bps (%d bytes,  %d seconds)"),
				(DWORD) ((dwTotalBytesSent / ((DWORD)(dwNewTickCount -  dwFirstTickCount) / 1000)) * 8),
				dwTotalBytesSent, (DWORD)(dwNewTickCount - dwFirstTickCount) / 1000);
			
			dwLastBytesSentCount = dwTotalBytesSent;
		}

		// Increment the Meg Counter if necessary
		if(dwCurrBytesSent >= MEG_SIZE)
		{
			dwMegCounter++;
			QAMessage(TEXT("sent %d megs"), dwMegCounter);
			dwCurrBytesSent -= MEG_SIZE;
		}

		// Sleep after each send if asked to
		if (g_dwWaitTime)
			Sleep(g_dwWaitTime);
	}

exitThread:

	freeaddrinfo(pAddrInfo);

	// disconnect
	if(sock != INVALID_SOCKET)
	{
		shutdown(sock, 2);
		closesocket(sock);
	}

	if(pBuf)
		LocalFree(pBuf);

	QAMessage(TEXT("SendThread--"));

	return 0;

}

//**********************************************************************************
DWORD WINAPI RecvThread(LPVOID *pParm)
{
	BYTE *pBuf = NULL;
	SOCKET accepted_sock, ServiceSocket[FD_SETSIZE];
	DWORD dwLen, dwFirstTickCount, dwNewTickCount;
	DWORD dwCurrBytesRecvd, dwTotalBytesRecvd, dwLastBytesRecvdCount, dwMegCounter;
	int iTimeoutCnt, iSelRet, iBytesRecvd, iSize;
	fd_set fdSet;
	TIMEVAL selTimeOut = { g_dwSelectTimeout, 0 };
	SOCKADDR_STORAGE saRemoteAddr;
	int iNumberOfSockets = 0, i;
	ADDRINFO AddrHints, *pAddrInfo, *pAI;
	char szPortASCII[10];

	QAMessage(TEXT("RecvThread++"));

	accepted_sock = INVALID_SOCKET;

	// Allocate the packet buffer
	pBuf = (BYTE *) LocalAlloc(LPTR, g_dwMaxBytesPerSend);

	if(pBuf == NULL)
	{
		QAError(TEXT("Can't allocate enough memory for packet."));
		goto exitThread;
	}

	memset(&AddrHints, 0, sizeof(AddrHints));
	AddrHints.ai_family = g_iFamily;
	AddrHints.ai_socktype = g_iSockType;
	AddrHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

#if defined UNICODE
	wcstombs(szPortASCII, g_szPort, sizeof(szPortASCII));
#else
	strncpy(szPortASCII, g_szPort, sizeof(szPortASCII));
#endif

	if(getaddrinfo(NULL, szPortASCII, &AddrHints, &pAddrInfo))
	{
		QAError(TEXT("getaddrinfo() error %d = %s"), WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}
	
	for(pAI = pAddrInfo, i = 0; pAI != NULL; pAI = pAI->ai_next, i++) 
	{
		if (i == FD_SETSIZE) 
		{
			QAWarning(TEXT("getaddrinfo returned more addresses than we could use."));
			break;
		}
		
		if((pAI->ai_family == PF_INET) || (pAI->ai_family == PF_INET6)) // supports only PF_INET and PF_INET6.
		{
			ServiceSocket[i] = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
			if (ServiceSocket[i] == INVALID_SOCKET)
			{
				QAWarning(TEXT("socket() failed for family %s, error %d = %s"), SZ_AI_FAMILY, WSAGetLastError(), GetLastErrorText());
				i--;
			}
			else if (bind(ServiceSocket[i], pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
			{
				QAWarning(TEXT("bind() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
				i--;
			}
			else 
			{
				if(g_iSockType == SOCK_STREAM)
				{
					if (listen(ServiceSocket[i], 5) == SOCKET_ERROR)
					{
						QAWarning(TEXT("listen() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
						i--;
					}
					else
						QAMessage(TEXT("Listening on port %s, using family %s"), g_szPort, SZ_AI_FAMILY);
				}
			}
		}
	}
	
	freeaddrinfo(pAddrInfo);
	
	if (i == 0) 
	{
		QAError(TEXT("fatal error: unable to serve on any address"));
		goto exitThread;
	}

	iNumberOfSockets = i;
	fd_set SockSet;
	
	TIMEVAL tm_limit;	// wait for connection for 1 sec.
	tm_limit.tv_sec = 1;
	tm_limit.tv_usec = 0;

ServiceClient:

	accepted_sock = INVALID_SOCKET;
	
	while(accepted_sock == INVALID_SOCKET)	// serve until connection is formed
	{
		FD_ZERO(&SockSet);

		for (i = 0; i < iNumberOfSockets; i++)	// want to check all available sockets
			FD_SET(ServiceSocket[i], &SockSet);
		
		if (select(iNumberOfSockets, &SockSet, 0, 0, &tm_limit) == SOCKET_ERROR)
		{  // select() returns after 1 sec waiting period
			QAError(TEXT("select() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
			goto exitThread;
		}
		
		for (i = 0; i < iNumberOfSockets; i++)	// check which socket is ready to process
		{
			if (FD_ISSET(ServiceSocket[i], &SockSet))	// proceed for connected socket
			{
				FD_CLR(ServiceSocket[i], &SockSet);
				if(g_iSockType == SOCK_STREAM)
				{
					iSize = sizeof(saRemoteAddr);
					accepted_sock = accept(ServiceSocket[i], (SOCKADDR*)&saRemoteAddr, &iSize);
					if(accepted_sock == INVALID_SOCKET) 
					{
						QAError(TEXT("accept() failed with error %d = %s"), WSAGetLastError(), GetLastErrorText());
						goto exitThread;
					}
					
					char szClientNameASCII[NI_MAXHOST];
					if (getnameinfo((SOCKADDR*)&saRemoteAddr, iSize,
						szClientNameASCII, sizeof(szClientNameASCII), NULL, 0, NI_NUMERICHOST) != 0)
						strcpy(szClientNameASCII, "");
					QAMessage(TEXT("Accepted connection from client %hs"), szClientNameASCII);
				}
				else
					accepted_sock = ServiceSocket[i];
				break;
			}
		}
	}
	
	iBytesRecvd = 0;
	iTimeoutCnt = 0;
	dwMegCounter = 0;
	dwLastBytesRecvdCount = dwCurrBytesRecvd = dwTotalBytesRecvd = 0;
	dwFirstTickCount = GetTickCount();

	// Send the data
	while ( g_bDaemon || (dwMegCounter < g_dwMegsToXfer) )
	{
		for(int i = 0; i < 3; i++)
		{
			FD_ZERO(&fdSet);
			FD_SET(accepted_sock, &fdSet);
			iSelRet = select(0, &fdSet, NULL, NULL, &selTimeOut);

			if((iSelRet == SOCKET_ERROR) || (iSelRet == 0))
			{
				QAWarning(TEXT("select(read) timed out %d"), iSelRet); 
				if (i == 2)
				{
					QAWarning(TEXT("timed out %d times... exiting (%d)"), 3, iSelRet);
					if(g_iSockType == SOCK_STREAM)
						QAError(TEXT("Remote client is no longer sending data..."));
					QAMessage(TEXT("Total bytes recv'd: %u"), (dwMegCounter * MEG_SIZE) + dwCurrBytesRecvd);
					QAMessage(TEXT("Expected at least: %u"), (g_dwMegsToXfer * MEG_SIZE));
					goto exitThread;
				}
			}
			else
				break;
		}
		
		dwLen = g_dwMaxBytesPerSend;
		if(g_iSockType == SOCK_STREAM)
		{
			if ((iBytesRecvd = recv(accepted_sock, (char *) pBuf, dwLen, 0)) == SOCKET_ERROR)
			{
				QAError(TEXT("recv() failed %d = %s"), WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}
		}
		else
		{
			iSize = sizeof(saRemoteAddr);
			if ((iBytesRecvd = recvfrom(accepted_sock, (char *) pBuf, dwLen, 0, (SOCKADDR *)&saRemoteAddr, &iSize)) == SOCKET_ERROR)
			{
				QAError(TEXT("recvfrom() failed %d = %s"), WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}
		}

		//QAMessage(TEXT("recv %d bytes"), iBytesRecvd);

		if(g_iSockType == SOCK_STREAM && iBytesRecvd == 0)
			break;

		// We use two counters here dwCurrBytesRecvd and dwTotalBytesRecvd due to a conflict
		// of interest.  Please see the sister note in SendThread.

		dwTotalBytesRecvd = (DWORD)(dwTotalBytesRecvd + iBytesRecvd);
		dwCurrBytesRecvd = (DWORD)(dwCurrBytesRecvd + iBytesRecvd);

		if((dwTotalBytesRecvd - dwLastBytesRecvdCount) > UPDATE_INTERVAL)
		{
			dwNewTickCount = GetTickCount();
			
			if(((DWORD)(dwNewTickCount - dwFirstTickCount) >= 1000) && g_bVerbose)
				QAMessage(TEXT("Receiving data at %d bps (%d bytes,  %d seconds)"),
				(DWORD) ((dwTotalBytesRecvd / ((DWORD)(dwNewTickCount -  dwFirstTickCount) / 1000)) * 8),
				dwTotalBytesRecvd, (DWORD)(dwNewTickCount - dwFirstTickCount) / 1000);
			
			dwLastBytesRecvdCount = dwTotalBytesRecvd;
		}

		// Increment the Meg Counter if necessary
		if(dwCurrBytesRecvd >= MEG_SIZE)
		{
			dwMegCounter++;
			QAMessage(TEXT("recv'd %d megs"), dwMegCounter);
			dwCurrBytesRecvd -= MEG_SIZE;
		}

		// Sleep after each recv if asked to
		if (g_dwWaitTime)
			Sleep(g_dwWaitTime);
	}

exitThread:

	// disconnect
	if(g_iSockType == SOCK_STREAM && accepted_sock != INVALID_SOCKET)
	{
		shutdown(accepted_sock, 2);
		closesocket(accepted_sock);
	}

	if(g_bDaemon)
	{
		QAMessage(TEXT("Finished serving this client... waiting for new client."));
		goto ServiceClient;
	}

	for( i = 0; i < iNumberOfSockets; i++)
	{
		shutdown(ServiceSocket[i], 2);
		closesocket(ServiceSocket[i]);
	}

	if(pBuf)
		LocalFree(pBuf);

	QAMessage(TEXT("RecvThread--"));

	return 0;
}


//**********************************************************************************
ULONG
BuildTestPacket(DWORD Len, BYTE Buff[], int nPacketNum)
{
    ULONG CheckSum = 0;
    DWORD   i = 0;

	Buff[i] = (BYTE) nPacketNum;
	CheckSum += Buff[i];
	i++;

    for (; i < Len; i++)
    {
        Buff[i] = g_cNextChar;
        if (g_cNextChar == 'z')
            g_cNextChar = 'a';
        else
            g_cNextChar++;
        CheckSum += Buff[i];
    }
  
    return(CheckSum);
}