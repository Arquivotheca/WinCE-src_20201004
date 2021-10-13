//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <winsock2.h>
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
#define DEFAULT_PORT				40000


DWORD WINAPI SendThread(LPVOID *pParm);
DWORD WINAPI RecvThread(LPVOID *pParm);
ULONG BuildTestPacket(DWORD Len, BYTE Buff[], int nPacketNum);

TCHAR* gszMainProgName = _T("rsstres2");
DWORD  gdwMainInitFlags = INIT_NET_CONNECTION;

void PrintUsage()
{
	QAMessage(TEXT("rsstres2 [-cIPAddress] [-bBytes] [-wMSWait] [-srutv] [-mTotBytesLimit]"));
	QAMessage(TEXT("c:  Specify the server to communicate with. (default: localhost)"));
	QAMessage(TEXT("p:  Set port to use (default %d)"), DEFAULT_PORT);
	QAMessage(TEXT("b:  Specify number of bytes per send.    (randomly determined for each send if not specified)."));
	QAMessage(TEXT("m:  Set total number of MBytes to transmit. (default: 1)"));
	QAMessage(TEXT("w:  Time to wait (in milliseconds) between sends/recvs."));
	QAMessage(TEXT("s:  Run send thread"));
	QAMessage(TEXT("r:  Run receive thread"));
	QAMessage(TEXT("u:  Send and/or receive over datagram (UDP) sockets. [TCP is default]"));
	QAMessage(TEXT("l:  Set max time for select to wait in seconds. (default: %d)"), TIMEOUT_SECONDS);
	QAMessage(TEXT("e:  Set to verbose mode... data recv/send is printed to display."));
	QAMessage(TEXT("d:  Set to Deamon mode...  recv thread is not closed upon client exit."));
}

//
// Global Variables
//
LPTSTR g_szServer;
WORD g_wPort;
int g_iSockType;
DWORD g_dwMinBytesPerSend, g_dwMaxBytesPerSend;
DWORD g_dwMegsToXfer;
DWORD g_dwWaitTime;
BOOL g_bDaemon;
BOOL g_bVerbose;
int g_iFamily;
DWORD g_dwSelectTimeout;

char g_cNextChar = 'a';

//**********************************************************************************
extern "C"
int mymain(int argc, TCHAR* argv[])
{
    WSADATA		WSAData;	
    HANDLE		hThreadSend = NULL;
    HANDLE		hThreadRecv = NULL;
    DWORD		dwThreadId;
	DWORD		dwTempPort;
    WORD		WSAVerReq = MAKEWORD(2,2);

	BOOL		bRecv, bSend;

	if(QAWasOption(_T("?")) || QAWasOption(_T("help")))
	{
		PrintUsage();
		return 0;
	}

	// Set up defaults
	g_szServer = _T("localhost");
	g_wPort = DEFAULT_PORT;
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
	if(QAGetOptionAsDWORD(_T("p"), &dwTempPort))
		g_wPort = (WORD)dwTempPort;
	QAGetOptionAsDWORD(_T("m"), &g_dwMegsToXfer);
	QAGetOptionAsDWORD(_T("l"), &g_dwSelectTimeout);

	QAMessage(TEXT("Server = %s.  Port = %d"), g_szServer, g_wPort);
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
	SOCKADDR_IN saAddr, saRemoteAddr;
	hostent	*h;
	char szServerASCII[256];

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
#else
	strncpy(szServerASCII, g_szServer, sizeof(szServerASCII));
#endif

	// perform name resolution
	if ((dwIPAddr = inet_addr(szServerASCII)) == INADDR_NONE)
	{
		// remote server is not a dotted decimal IP address
		h = gethostbyname(szServerASCII);
		if(h != NULL)
		{
			memcpy( &dwIPAddr, h->h_addr_list[0], sizeof(dwIPAddr) );
		}
		else
		{
			QAError (TEXT("Invalid address parameter = %s"), g_szServer);
			goto exitThread;
		}
	}

	// Create socket
	sock = socket(AF_INET, g_iSockType, 0);

	if(sock == INVALID_SOCKET)
	{
		QAError(TEXT("socket() failed with error %d = %s"), WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}

	saAddr.sin_family = AF_INET;
	saAddr.sin_addr.s_addr = INADDR_ANY;
	saAddr.sin_port = 0;

	saRemoteAddr.sin_family = AF_INET;
	saRemoteAddr.sin_addr.s_addr = dwIPAddr;
	saRemoteAddr.sin_port = htons(g_wPort);

	// bind the socket
	if(bind(sock, (SOCKADDR *)&saAddr, sizeof(saAddr)) == SOCKET_ERROR)
	{
		QAError(TEXT("bind() failed with error %d = %s"), WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}

	if(g_iSockType == SOCK_STREAM)
	{
		// connect
		if(connect(sock, (SOCKADDR *)&saRemoteAddr, sizeof(saRemoteAddr)) == SOCKET_ERROR)
		{
			QAError(TEXT("connect() failed with error %d = %s"), WSAGetLastError(), GetLastErrorText());
			goto exitThread;
		}
		else
			QAMessage(TEXT("connect()'d successfully to 0x%08x"), dwIPAddr);
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
			if ((iBytesSent = sendto(sock, (char *) pBuf, dwLen, 0, (SOCKADDR *)&saRemoteAddr, sizeof(saRemoteAddr))) == SOCKET_ERROR)
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
	SOCKET accepted_sock, sock;
	DWORD dwLen, dwFirstTickCount, dwNewTickCount;
	DWORD dwCurrBytesRecvd, dwTotalBytesRecvd, dwLastBytesRecvdCount, dwMegCounter;
	int iTimeoutCnt, iSelRet, iBytesRecvd, iSize;
	fd_set fdSet;
	TIMEVAL selTimeOut = { g_dwSelectTimeout, 0 };
	SOCKADDR_IN saAddr, saRemoteAddr;

	QAMessage(TEXT("RecvThread++"));

	accepted_sock = sock = INVALID_SOCKET;

	// Allocate the packet buffer
	pBuf = (BYTE *) LocalAlloc(LPTR, g_dwMaxBytesPerSend);

	if(pBuf == NULL)
	{
		QAError(TEXT("Can't allocate enough memory for packet."));
		goto exitThread;
	}

	// Create socket
	sock = socket(AF_INET, g_iSockType, 0);

	if(sock == INVALID_SOCKET)
	{
		QAError(TEXT("socket() failed with error %d = %s"), WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}

	saAddr.sin_family = AF_INET;
	saAddr.sin_addr.s_addr = INADDR_ANY;
	saAddr.sin_port = htons(g_wPort);

	// bind the socket
	if(bind(sock, (SOCKADDR *)&saAddr, sizeof(saAddr)) == SOCKET_ERROR)
	{
		QAError(TEXT("bind() failed with error %d = %s"), WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}

	if(g_iSockType == SOCK_STREAM)
	{
		// listen
		if(listen(sock, 5) == SOCKET_ERROR)
		{
			QAError(TEXT("listen() failed with error %d = %s"), WSAGetLastError(), GetLastErrorText());
			goto exitThread;
		}
	}

ServiceClient:

	if(g_iSockType == SOCK_STREAM)
	{
		// accept
		iSize = sizeof(saRemoteAddr);
		accepted_sock = accept(sock, (SOCKADDR *)&saRemoteAddr, &iSize);
		if(accepted_sock == INVALID_SOCKET)
		{
			QAError(TEXT("accept() failed with error %d = %s"), WSAGetLastError(), GetLastErrorText());
			goto exitThread;
		}
		else
			QAMessage(TEXT("Accepted connection from client at address 0x%08x"), saRemoteAddr.sin_addr.s_addr);
	}
	else
	{
		accepted_sock = sock;
		sock = INVALID_SOCKET;
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
	if(accepted_sock != INVALID_SOCKET)
	{
		shutdown(accepted_sock, 2);
		closesocket(accepted_sock);
	}

	if(g_bDaemon)
	{
		QAMessage(TEXT("Finished serving this client... waiting for new client."));
		goto ServiceClient;
	}

	if(sock != INVALID_SOCKET)
	{
		shutdown(sock, 2);
		closesocket(sock);
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
