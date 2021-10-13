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
#include "netmain.h"

#define WINSOCK_VERSION_REQ		MAKEWORD(2,2)

// Flags for ActionType
#define BIND_FLAG	0x00000001
#define CONN_FLAG	0x00000002
#define SEND_FLAG	0x00000004
#define RECV_FLAG	0x00000008

#define RANDOM_SOCKTYPE			-1

#define	DEFAULT_MSGTIME			100
#define DEFAULT_WAITTIME		0
#define DEFAULT_ITERATIONS		1000
#define DEFAULT_SOCKS_B4_CLOSE	1
#define DEFAULT_PORT			_T("53123")
#define DEFAULT_XFER_BYTES		128
#define DEFAULT_SENDTYPE		0
#define DEFAULT_FAMILY			PF_UNSPEC
#define DEFAULT_SOCKTYPE		SOCK_STREAM
#define DEFAULT_SERVER			_T("localhost")

#define MAX_PORT				65535

#define TIMEOUT_SECS			1
#define TIMEOUT_USECS			0

LPTSTR			g_szServer;
int				g_iFamily;
int				g_iSockType;
DWORD			g_dwWaitTime;
DWORD			g_dwSocksBeforeClose;
DWORD			g_dwMessageFrequency;
DWORD			g_dwIterations;
LPTSTR			g_szPort;
DWORD			g_dwBytesPerXfer;
DWORD			g_dwActionType;
BOOL			g_bDaemon;
BOOL			g_fClientThread;

CRITICAL_SECTION ServerCS;
DWORD			g_dwTotalClients = 0;

char			g_bNextChar = 'a';

// Function Prototypes
DWORD   WINAPI      ClientThread(LPVOID *pParm);
DWORD   WINAPI      ServerThreadTCP(LPVOID *pParm);
DWORD   WINAPI      ServerThreadUDP(LPVOID *pParm);
ULONG BuildTestPacket(DWORD dwLen, char Buff[], int nPacketNum);

// Declarations for NetMain
extern "C" TCHAR* gszMainProgName = _T("ocstres2_vX");
extern "C" DWORD  gdwMainInitFlags = INIT_NET_CONNECTION;

#define SZ_AI_FAMILY ((pAI->ai_family == PF_INET) ? _T("PF_INET") : _T("PF_INET6"))

void PrintUsage()
{
	QAMessage(TEXT("ocstres2_vX [-s ServerName/Addr] [-f Freq] [-w Wait] [-n Iterations]"));
	QAMessage(TEXT("     [-c SocksB4Close] [-p Port] [-u | -x] [-ab] [-g | -h | -o | -r]"));
	QAMessage(TEXT("     [-b BytesToXfer] [-d]"));
	QAMessage(TEXT("General Options:"));
	QAMessage(TEXT("    f:  Number of interations to perform before displaying status (default: %d)"), DEFAULT_MSGTIME);
	QAMessage(TEXT("    w:  Number of milliseconds to wait between iterations (default: %d)"), DEFAULT_WAITTIME);
	QAMessage(TEXT("    n:  Number of iterations (default: %d)"), DEFAULT_ITERATIONS);
	QAMessage(TEXT("    c:  Number of sockets to open before closing (default: %d)"), DEFAULT_SOCKS_B4_CLOSE);
	QAMessage(TEXT("    p:  Port to connect/listen on (default: %hs)"), DEFAULT_PORT);
	QAMessage(TEXT("    i:  Version of IP to use 4, 6, or X (default: X)"));
	QAMessage(TEXT("Thread Options:"));
	QAMessage(TEXT("    a:  Run Client Thread"));
	QAMessage(TEXT("    b:  Run TCP/UDP Server Thread"));
	QAMessage(TEXT("Client Options:"));
	QAMessage(TEXT("    s:  Specify the server to communicate with. (default: %s)"), DEFAULT_SERVER);
	QAMessage(TEXT("    u:  Use datagram (UDP) sockets (default is TCP)"));
	QAMessage(TEXT("    x:  Use random mixed socket types (UDP/TCP)"));
	QAMessage(TEXT("    g:  Bind the sockets"));
	QAMessage(TEXT("    h:  connect to the server (includes binding)"));
	QAMessage(TEXT("Server Options:"));
	QAMessage(TEXT("    d:  Set server to Deamon mode"));
	QAMessage(TEXT("Data Transfer Options (all include bind/connect for client):"));
	QAMessage(TEXT("    o:  send data before closing socket"));
	QAMessage(TEXT("    r:  recv data before closing socket"));
	QAMessage(TEXT("    y:  bytes to send/recv (default: %d)"), DEFAULT_XFER_BYTES);
}

DWORD GetFamilyIndex(int iFamily)
{
	switch(iFamily)
	{
	case PF_INET:
		return 0;
	case PF_INET6:
		return 1;
	default:
		return 2;
	}
}
			
DWORD GetTypeIndex(int iType)
{
	switch(iType)
	{
	case SOCK_STREAM:
		return 0;
	default:
		return 1;
	}
}


//**********************************************************************************
int mymain(int argc, TCHAR* argv[])
{
    WSADATA WSAData;	
    HANDLE  hThreadClient, hThreadServerTCP, hThreadServerUDP;
    DWORD   dwThreadId;
	BOOL	bClient, bServer;
	LPTSTR	szIPVersion;

	if(QAWasOption(_T("?")) || QAWasOption(_T("help")))
	{
		PrintUsage();
		return 0;
	}

	// Initialize variables
	g_szServer = DEFAULT_SERVER;
	g_dwWaitTime = DEFAULT_WAITTIME;
	g_dwSocksBeforeClose = DEFAULT_SOCKS_B4_CLOSE;
	g_dwMessageFrequency = DEFAULT_MSGTIME;
	g_dwIterations = DEFAULT_ITERATIONS;
	g_szPort = DEFAULT_PORT;
	g_iFamily = DEFAULT_FAMILY;
	g_iSockType = DEFAULT_SOCKTYPE;
	g_dwBytesPerXfer = DEFAULT_XFER_BYTES;
	g_bDaemon = FALSE;
	g_fClientThread = TRUE;
	bClient = bServer = FALSE;
	hThreadClient = hThreadServerTCP = hThreadServerUDP = NULL;

	// Parse command line
	QAGetOption(_T("s"), &g_szServer);
	QAGetOption(_T("p"), &g_szPort);
	QAGetOptionAsDWORD(_T("w"), &g_dwWaitTime);
	QAGetOptionAsDWORD(_T("n"), &g_dwIterations);
	QAGetOptionAsDWORD(_T("y"), &g_dwBytesPerXfer);

	if(QAGetOptionAsDWORD(_T("f"), &g_dwMessageFrequency) && !g_dwMessageFrequency)
		g_dwMessageFrequency = 1;

	if(QAGetOptionAsDWORD(_T("c"), &g_dwSocksBeforeClose) && !g_dwSocksBeforeClose)
		g_dwSocksBeforeClose = 1;

	if(QAWasOption(_T("u")))
		g_iSockType = SOCK_DGRAM;
	
	if(QAWasOption(_T("x")))
		g_iSockType = RANDOM_SOCKTYPE;

	if(QAWasOption(_T("g")))
	{
		g_dwActionType |= BIND_FLAG;
		QAMessage(TEXT("Data Xfer: Bind ON"));
	}

	if(QAWasOption(_T("h")))
	{
		g_dwActionType |= BIND_FLAG | CONN_FLAG;
		QAMessage(TEXT("Data Xfer: Connect ON"));
	}

	if(QAWasOption(_T("o")))
	{
		g_dwActionType |= SEND_FLAG | BIND_FLAG | CONN_FLAG;
		QAMessage(TEXT("Data Xfer: Send ON"));
	}

	if(QAWasOption(_T("r")))
	{
		g_dwActionType |= RECV_FLAG | BIND_FLAG | CONN_FLAG;
		QAMessage(TEXT("Data Xfer: Recv ON"));
	}

	if(QAWasOption(_T("a")))
	{
		bClient = TRUE;
		QAMessage(TEXT("Client Thread ON"));
	}
		
	if(QAWasOption(_T("b")))
	{
		bServer = TRUE;
		QAMessage(TEXT("Server Thread ON"));
	}

	if(QAWasOption(_T("d")))
	{
		g_bDaemon = TRUE;
		QAMessage(TEXT("Daemon mode ON"));
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
	
    if(WSAStartup(WINSOCK_VERSION_REQ, &WSAData) != 0)
	{
		QAError(TEXT("WSAStartup Failed"));
		return FALSE;
	}
    else
	{
        QAMessage(TEXT("WSAStartup SUCCESS"));
    }

	InitializeCriticalSection(&ServerCS);

	if(bServer)
	{
		QAMessage(TEXT("Creating Server Threads"));

		if((hThreadServerTCP = 
				CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ServerThreadTCP,
				NULL, 0, &dwThreadId)) == NULL)
		{
			QAError (TEXT("CreateThread(ServerThreadTCP) failed %d"),
				GetLastError());
				
			goto exit;
		}

		if((hThreadServerUDP = 
				CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ServerThreadUDP,
				NULL, 0, &dwThreadId)) == NULL)
		{
			QAError (TEXT("CreateThread(ServerThreadUDP) failed %d"),
				GetLastError());
				
			goto exit;
		}
	}

	if(bClient)
	{
		QAMessage (TEXT("Creating Client Thread"));

		if ((hThreadClient = 
				CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ClientThread,
				NULL, 0, &dwThreadId)) == NULL)
		{
			QAError (TEXT("CreateThread(ClientThread) failed %d"),
				GetLastError());
				
			goto exit;
		}
	}


	QAMessage(TEXT("Waiting for completion."));

	if (bServer)
	{
		WaitForSingleObject(hThreadServerTCP, INFINITE);
		WaitForSingleObject(hThreadServerUDP, INFINITE);
	}

	if (bClient)
		WaitForSingleObject(hThreadClient, INFINITE);

exit:

	DeleteCriticalSection(&ServerCS);

	if(hThreadServerTCP)
		CloseHandle(hThreadServerTCP);

	if(hThreadServerUDP)
		CloseHandle(hThreadServerUDP);

	if(hThreadClient)
		CloseHandle(hThreadClient);

	WSACleanup();

	return 0;
}

#define NUM_FAMILIES	3
#define NUM_TYPES		2

//**********************************************************************************
DWORD WINAPI ClientThread(LPVOID *pParm)
{
	SOCKET			*sockArray = NULL;
	DWORD			dwTotalSocks, i, j;
	DWORD			cbRecvd, cbTotalRecvd;
	SOCKADDR_STORAGE saRemoteAddr;
	char			szServerASCII[256], *pBufSend = NULL, *pBufRecv = NULL;
	int				iSockType, iSize;
	DWORD			dwStats[NUM_FAMILIES][NUM_TYPES];
	ADDRINFO		AddrHints, *pAddrInfo = NULL, *pAI;
	char			szPortASCII[10];
	fd_set			fdReadSet;
	TIMEVAL			timeout = {TIMEOUT_SECS, TIMEOUT_USECS};

	QAMessage(TEXT("ClientThread++"));

	sockArray = (SOCKET *) malloc( sizeof(SOCKET) * g_dwSocksBeforeClose );

	if(sockArray == NULL)
	{
		QAError(TEXT("ClientThread: Could not allocate enough memory for socket array!"));
		goto exitThread;
	}

	for(i = 0; i < g_dwSocksBeforeClose; i++)
		sockArray[i] = INVALID_SOCKET;

	memset(&dwStats, 0, sizeof(dwStats));

	if(g_dwBytesPerXfer && (g_dwActionType & (RECV_FLAG | SEND_FLAG)))
	{
		pBufSend = (char *) malloc( g_dwBytesPerXfer );
		
		if(pBufSend == NULL)
		{
			QAError(TEXT("ClientThread: Could not allocate enough memory for send packet data!"));
			goto exitThread;
		}

		BuildTestPacket(g_dwBytesPerXfer, pBufSend, 0);

		pBufRecv = (char *) malloc( g_dwBytesPerXfer );
		
		if(pBufRecv == NULL)
		{
			QAError(TEXT("ClientThread: Could not allocate enough memory for recv packet data!"));
			goto exitThread;
		}
	}

	// Initialize loop variables
	dwTotalSocks = 0;
	i = 0;
	
	while (dwTotalSocks < g_dwIterations)
	{
		// Figure out our socktype for this iteration
		if(g_iSockType == RANDOM_SOCKTYPE)
		{
			if(GetRandomNumber(1) == 0)
				iSockType = SOCK_STREAM;
			else
				iSockType = SOCK_DGRAM;
		}
		else
			iSockType = g_iSockType;
		
		if(g_dwActionType & CONN_FLAG)
		{
#if defined UNICODE
			wcstombs(szServerASCII, g_szServer, sizeof(szServerASCII));
			wcstombs(szPortASCII, g_szPort, sizeof(szPortASCII));
#else
			strncpy(szServerASCII, g_szServer, sizeof(szServerASCII));
			strncpy(szPortASCII, g_szPort, sizeof(szPortASCII));
#endif
			
			memset(&AddrHints, 0, sizeof(AddrHints));
			AddrHints.ai_family = g_iFamily;
			AddrHints.ai_socktype = iSockType;
			
			// Use IPvX name resolution
			if(getaddrinfo(szServerASCII, szPortASCII, &AddrHints, &pAddrInfo))
			{
				QAError (TEXT("Invalid address parameter = %s"), g_szServer);
				goto exitThread;
			}
		}
		else
		{
			memset(&AddrHints, 0, sizeof(AddrHints));
			AddrHints.ai_family = g_iFamily;
			AddrHints.ai_socktype = iSockType;
			AddrHints.ai_flags = AI_NUMERICHOST;
			
			if(getaddrinfo(NULL, "0", &AddrHints, &pAddrInfo))
			{
				QAError(TEXT("getaddrinfo() error %d = %s"), WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}
		}

		for(pAI = pAddrInfo; pAI != NULL && (dwTotalSocks < g_dwIterations); pAI = pAI->ai_next)
		{
			// Pause if desired
			if(g_dwWaitTime)
				Sleep(g_dwWaitTime);

			// open the socket
			sockArray[i] = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);

			if(sockArray[i] == INVALID_SOCKET)
			{
				QAError(TEXT("ClientThread: socket() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}

			if(g_dwActionType & BIND_FLAG  && !(g_dwActionType & CONN_FLAG))
			{	
				// Bind if we're supposed to bind and not connect
				if(bind(sockArray[i], pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
				{
					QAError(TEXT("ClientThread: bind() failed, error %d = %s"),
						WSAGetLastError(), GetLastErrorText());
					goto exitThread;
				}
			}

			if(g_dwActionType & CONN_FLAG)
			{
				// Actually connect only for TCP sockets
				if(iSockType == SOCK_STREAM)
				{
					if(connect(sockArray[i], pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
					{
						QAError(TEXT("ClientThread: connect() failed, error %d = %s"),
							WSAGetLastError(), GetLastErrorText());
						goto exitThread;
					}
				}
			}

			// If we're expecting any kind of data transfer with UDP sockets, we need to send
			// a packet.  Otherwise, the other side would have to continually spam us with packets.
			if(g_dwActionType & SEND_FLAG || (iSockType == SOCK_DGRAM && (g_dwActionType & RECV_FLAG)))
			{
				if(iSockType == SOCK_STREAM)
				{
					if((DWORD) send(sockArray[i], (char *)pBufSend, g_dwBytesPerXfer, 0) != g_dwBytesPerXfer)
					{
						QAError(TEXT("ClientThread: send() failed, error %d = %s"),
							WSAGetLastError(), GetLastErrorText());
						goto exitThread;
					}
				}
				else
				{
					if((DWORD)sendto(sockArray[i], (char *)pBufSend, g_dwBytesPerXfer, 0,
						pAI->ai_addr, pAI->ai_addrlen) != g_dwBytesPerXfer)
					{
						QAError(TEXT("ClientThread: sendto() failed, error %d = %s"),
							WSAGetLastError(), GetLastErrorText());
						goto exitThread;
					}
				}
			}

			if(g_dwActionType & RECV_FLAG)
			{
				if(iSockType == SOCK_STREAM)
				{
					for(cbTotalRecvd = 0; cbTotalRecvd < g_dwBytesPerXfer; cbTotalRecvd += cbRecvd)
					{
						cbRecvd = recv(sockArray[i], (char *)pBufRecv + cbTotalRecvd, g_dwBytesPerXfer - cbTotalRecvd, 0);
						if(cbRecvd == SOCKET_ERROR)
						{
							QAError(TEXT("ClientThread: recv() failed, error %d = %s"),
								WSAGetLastError(), GetLastErrorText());
							goto exitThread;
						}
						else if(cbRecvd == 0)
						{
							QAError(TEXT("ClientThread: unexpected connection close!"));
							goto exitThread;
						}
					}
				}
				else
				{
					FD_ZERO(&fdReadSet);
					FD_SET(sockArray[i], &fdReadSet);
					if(select(0, &fdReadSet, NULL, NULL, &timeout) != 1)
					{
						QAWarning(TEXT("ClientThread: timed out waiting for packet..."),
							WSAGetLastError(), GetLastErrorText());
						continue;
					}

					iSize = sizeof(saRemoteAddr);
					cbRecvd = recvfrom(sockArray[i], (char *)pBufRecv, g_dwBytesPerXfer, 0,
						(SOCKADDR *)&saRemoteAddr, &iSize);
					if((DWORD)cbRecvd != g_dwBytesPerXfer)
					{
						QAError(TEXT("ClientThread: recvfrom() failed, return = %d, error %d = %s"),
							cbRecvd, WSAGetLastError(), GetLastErrorText());
						goto exitThread;
					}
				}
			}

			// Perform maintenance tasks
			dwTotalSocks++;
			i++;

			dwStats[GetFamilyIndex(pAI->ai_family)][GetTypeIndex(iSockType)]++;

			if(i == g_dwSocksBeforeClose)
			{
				// Close the sockets opened
				for(j = 0; j < i; j++)
				{
					shutdown(sockArray[j], 2);
					if(closesocket(sockArray[j]) == SOCKET_ERROR)
					{
						QAError(TEXT("ClientThread: closesocket() failed, error %d = %s"),
							WSAGetLastError(), GetLastErrorText());
					}
					sockArray[j] = INVALID_SOCKET;
				}

				i = 0;
			}


			if((dwTotalSocks % g_dwMessageFrequency == 0) && dwTotalSocks > 0)
			{
				QAMessage(TEXT("ClientThread: open/%s%s%s%sclose performed %u time(s)"),
					(g_dwActionType & BIND_FLAG) ? _T("bind/") : _T(""),
					(g_dwActionType & CONN_FLAG) ? _T("conn/") : _T(""),
					(g_dwActionType & SEND_FLAG) ? _T("send/") : _T(""),
					(g_dwActionType & RECV_FLAG) ? _T("recv/") : _T(""),
					dwTotalSocks);
			}
		}

		freeaddrinfo(pAddrInfo);
		pAddrInfo = NULL;
	}

exitThread:

	QAMessage(TEXT("--------------------------------------------------------"));
	QAMessage(TEXT("open/%s%s%s%sclose performed a total of %u time(s)"),
		(g_dwActionType & BIND_FLAG) ? _T("bind/") : _T(""),
		(g_dwActionType & CONN_FLAG) ? _T("conn/") : _T(""),
		(g_dwActionType & SEND_FLAG) ? _T("send/") : _T(""),
		(g_dwActionType & RECV_FLAG) ? _T("recv/") : _T(""),
		dwTotalSocks);
	QAMessage(TEXT("                   TCP |         UDP"));
	QAMessage(TEXT("PF_INET     %10d |  %10d"), dwStats[0][0], dwStats[0][1]);
	QAMessage(TEXT("PF_INET6    %10d |  %10d"), dwStats[1][0], dwStats[1][1]);
	QAMessage(TEXT("PF_UNSPEC   %10d |  %10d"), dwStats[2][0], dwStats[2][1]);

	for(i = 0; i < g_dwSocksBeforeClose; i++)
	{
		if(sockArray[i] != INVALID_SOCKET)
		{
			shutdown(sockArray[i], 2);
			closesocket(sockArray[i]);
		}
	}

	if(pAddrInfo)
		freeaddrinfo(pAddrInfo);

	if(pBufSend)
		free(pBufSend);

	if(pBufRecv)
		free(pBufRecv);

	if(sockArray)
		free(sockArray);

	g_fClientThread = FALSE;

	QAMessage(TEXT("ClientThread--"));

	return 0;
}



//**********************************************************************************
DWORD WINAPI ServerThreadTCP(LPVOID *pParm)
{
	SOCKET			sock = INVALID_SOCKET, ServiceSocket[FD_SETSIZE];
	SOCKADDR_STORAGE saRemoteAddr;
	char			*pBufSend = NULL, *pBufRecv = NULL;
	int				iSize, iNumberOfSockets = 0, i;
	fd_set			fdReadSet;
	DWORD			cbRecvd, cbTotalRecvd;
	TIMEVAL			timeout = {TIMEOUT_SECS, TIMEOUT_USECS};
	ADDRINFO		AddrHints, *pAddrInfo = NULL, *pAI;
	char			szPortASCII[10];

	QAMessage(TEXT("ServerThreadTCP++"));

	if(g_dwBytesPerXfer && (g_dwActionType & (RECV_FLAG | SEND_FLAG)))
	{
		pBufSend = (char *) malloc( g_dwBytesPerXfer );
		
		if(pBufSend == NULL)
		{
			QAError(TEXT("SvrThreadTCP: Could not allocate enough memory for send packet data!"));
			goto exitThread;
		}

		BuildTestPacket(g_dwBytesPerXfer, pBufSend, 0);

		pBufRecv = (char *) malloc( g_dwBytesPerXfer );
		
		if(pBufRecv == NULL)
		{
			QAError(TEXT("SvrThreadTCP: Could not allocate enough memory for recv packet data!"));
			goto exitThread;
		}
	}

	memset(&AddrHints, 0, sizeof(AddrHints));
	AddrHints.ai_family = g_iFamily;
	AddrHints.ai_socktype = SOCK_STREAM;
	AddrHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

#if defined UNICODE
	wcstombs(szPortASCII, g_szPort, sizeof(szPortASCII));
#else
	strncpy(szPortASCII, g_szPort, sizeof(szPortASCII));
#endif

	if(getaddrinfo(NULL, szPortASCII, &AddrHints, &pAddrInfo))
	{
		QAError(TEXT("SvrThreadTCP: getaddrinfo() error %d = %s"), WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}
	
	for(pAI = pAddrInfo, i = 0; pAI != NULL; pAI = pAI->ai_next, i++) 
	{
		if (i == FD_SETSIZE) 
		{
			QAWarning(TEXT("SvrThreadTCP: getaddrinfo returned more addresses than we could use."));
			break;
		}
		
		if((pAI->ai_family == PF_INET) || (pAI->ai_family == PF_INET6)) // supports only PF_INET and PF_INET6.
		{
			ServiceSocket[i] = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
			if (ServiceSocket[i] == INVALID_SOCKET)
			{
				QAWarning(TEXT("SvrThreadTCP: socket() failed for family %s, error %d = %s"),
					SZ_AI_FAMILY, WSAGetLastError(), GetLastErrorText());
				i--;
			}
			else if (bind(ServiceSocket[i], pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
			{
				QAWarning(TEXT("SvrThreadTCP: bind() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
				i--;
			}
			else 
			{
				if (listen(ServiceSocket[i], 5) == SOCKET_ERROR)
				{
					QAWarning(TEXT("SvrThreadTCP: listen() failed, error %d = %s"),
						WSAGetLastError(), GetLastErrorText());
					i--;
				}
				else
					QAMessage(TEXT("SvrThreadTCP: Listening on port %s, using family %s"),
					g_szPort, SZ_AI_FAMILY);
			}
		}
	}
	
	freeaddrinfo(pAddrInfo);
	
	if (i == 0) 
	{
		QAError(TEXT("SvrThreadTCP: fatal error: unable to serve on any address"));
		goto exitThread;
	}

	iNumberOfSockets = i;

ServiceClient:

	sock = INVALID_SOCKET;
	
	while((sock == INVALID_SOCKET) && 
		  (g_bDaemon || (g_fClientThread && g_dwTotalClients < g_dwIterations)) )
	{
		FD_ZERO(&fdReadSet);
		for (i = 0; i < iNumberOfSockets; i++)	// want to check all available sockets
			FD_SET(ServiceSocket[i], &fdReadSet);
		
		if (select(iNumberOfSockets, &fdReadSet, 0, 0, &timeout) == SOCKET_ERROR)
		{
			QAError(TEXT("select() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
			goto exitThread;
		}
		
		for (i = 0; i < iNumberOfSockets; i++)	// check which socket is ready to process
		{
			if (FD_ISSET(ServiceSocket[i], &fdReadSet))	// proceed for connected socket
			{
				//QAMessage(TEXT("SvrThreadTCP: accept()'ing"));
				FD_CLR(ServiceSocket[i], &fdReadSet);
				iSize = sizeof(saRemoteAddr);
				sock = accept(ServiceSocket[i], (SOCKADDR*)&saRemoteAddr, &iSize);
				if(sock == INVALID_SOCKET) 
				{
					QAError(TEXT("accept() failed with error %d = %s"), WSAGetLastError(), GetLastErrorText());
					goto exitThread;
				}
				break;
			}
		}
	}
	
	if(sock != INVALID_SOCKET)
	{
		// Pause if desired
		if(g_dwWaitTime)
			Sleep(g_dwWaitTime);
		
		// Recv & Send flags work backwards since we're the server
		if(g_dwActionType & SEND_FLAG)
		{
			//QAMessage(TEXT("SvrThreadTCP: recv()'ing"));
			for(cbTotalRecvd = 0; cbTotalRecvd < g_dwBytesPerXfer; cbTotalRecvd += cbRecvd)
			{
				cbRecvd = recv(sock, (char *)pBufRecv + cbTotalRecvd, g_dwBytesPerXfer - cbTotalRecvd, 0);
				if(cbRecvd == SOCKET_ERROR)
				{
					QAError(TEXT("SvrThreadTCP: recv() failed, error %d = %s"),
						WSAGetLastError(), GetLastErrorText());
					goto exitThread;
				}
				else if(cbRecvd == 0)
				{
					QAError(TEXT("ClientThread: unexpected connection close!"));
					goto exitThread;
				}
			}
		}
		
		if(g_dwActionType & RECV_FLAG)
		{
			//QAMessage(TEXT("SvrThreadTCP: send()'ing"));

			if(send(sock, (char *)pBufSend, g_dwBytesPerXfer, 0) == SOCKET_ERROR)
			{
				QAError(TEXT("SvrThreadTCP: send() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}
		}

		EnterCriticalSection(&ServerCS);
		g_dwTotalClients++;
		LeaveCriticalSection(&ServerCS);

		shutdown(sock, 2);
		if(closesocket(sock) == SOCKET_ERROR)
		{
			QAError(TEXT("SvrThreadTCP: closesocket() failed, error %d = %s"),
				WSAGetLastError(), GetLastErrorText());
		}
		else
			sock = INVALID_SOCKET;

		goto ServiceClient;
	}

exitThread:

	if(sock != INVALID_SOCKET)
	{
		shutdown(sock, 2);
		closesocket(sock);
	}

	for( i = 0; i < iNumberOfSockets; i++)
	{
		shutdown(ServiceSocket[i], 2);
		closesocket(ServiceSocket[i]);
	}

	if(pBufSend)
		free(pBufSend);

	if(pBufRecv)
		free(pBufRecv);

	QAMessage(TEXT("ServerThreadTCP--"));

	return 0;
}

//**********************************************************************************
DWORD WINAPI ServerThreadUDP(LPVOID *pParm)
{
	SOCKET			sock = INVALID_SOCKET, ServiceSocket[FD_SETSIZE];
	SOCKADDR_STORAGE saRemoteAddr;
	char			*pBufSend = NULL, *pBufRecv = NULL;
	int				iSize, iNumberOfSockets = 0, i;
	fd_set			fdReadSet;
	TIMEVAL			timeout = {TIMEOUT_SECS, TIMEOUT_USECS};
	ADDRINFO		AddrHints, *pAddrInfo = NULL, *pAI;
	char			szPortASCII[10];

	QAMessage(TEXT("ServerThreadUDP++"));
	
	if(g_dwActionType & RECV_FLAG)
	{
		pBufSend = (char *) malloc( g_dwBytesPerXfer );
		
		if(pBufSend == NULL)
		{
			QAError(TEXT("SvrThreadUDP: Could not allocate enough memory for send packet data!"));
			goto exitThread;
		}
		
		BuildTestPacket(g_dwBytesPerXfer, pBufSend, 0);
	}
	
	pBufRecv = (char *) malloc( g_dwBytesPerXfer );
	
	if(pBufRecv == NULL)
	{
		QAError(TEXT("SvrThreadUDP: Could not allocate enough memory for recv packet data!"));
		goto exitThread;
	}

	memset(&AddrHints, 0, sizeof(AddrHints));
	AddrHints.ai_family = g_iFamily;
	AddrHints.ai_socktype = SOCK_DGRAM;
	AddrHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

#if defined UNICODE
	wcstombs(szPortASCII, g_szPort, sizeof(szPortASCII));
#else
	strncpy(szPortASCII, g_szPort, sizeof(szPortASCII));
#endif

	if(getaddrinfo(NULL, szPortASCII, &AddrHints, &pAddrInfo))
	{
		QAError(TEXT("SvrThreadUDP: getaddrinfo() error %d = %s"), WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}
	
	for(pAI = pAddrInfo, i = 0; pAI != NULL; pAI = pAI->ai_next, i++) 
	{
		if (i == FD_SETSIZE) 
		{
			QAWarning(TEXT("SvrThreadUDP: getaddrinfo returned more addresses than we could use."));
			break;
		}
		
		if((pAI->ai_family == PF_INET) || (pAI->ai_family == PF_INET6)) // supports only PF_INET and PF_INET6.
		{
			ServiceSocket[i] = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
			if (ServiceSocket[i] == INVALID_SOCKET)
			{
				QAWarning(TEXT("SvrThreadUDP: socket() failed for family %s, error %d = %s"),
					SZ_AI_FAMILY, WSAGetLastError(), GetLastErrorText());
				i--;
			}
			else if (bind(ServiceSocket[i], pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
			{
				QAWarning(TEXT("SvrThreadUDP: bind() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
				i--;
			}
		}
	}
	
	freeaddrinfo(pAddrInfo);
	
	if (i == 0) 
	{
		QAError(TEXT("SvrThreadUDP: fatal error: unable to serve on any address"));
		goto exitThread;
	}

	iNumberOfSockets = i;

ServiceClient:

	sock = INVALID_SOCKET;
	
	while((sock == INVALID_SOCKET) && 
		  (g_bDaemon || (g_fClientThread && g_dwTotalClients < g_dwIterations)) )
	{
		FD_ZERO(&fdReadSet);
		for (i = 0; i < iNumberOfSockets; i++)	// want to check all available sockets
			FD_SET(ServiceSocket[i], &fdReadSet);
		
		if (select(iNumberOfSockets, &fdReadSet, 0, 0, &timeout) == SOCKET_ERROR)
		{
			QAError(TEXT("select() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
			goto exitThread;
		}
		
		for (i = 0; i < iNumberOfSockets; i++)	// check which socket is ready to process
		{
			if (FD_ISSET(ServiceSocket[i], &fdReadSet))	// proceed for connected socket
			{
				FD_CLR(ServiceSocket[i], &fdReadSet);
				sock = ServiceSocket[i];
				break;
			}
		}
	}

	if(sock != INVALID_SOCKET)
	{
		// Pause if desired
		if(g_dwWaitTime)
			Sleep(g_dwWaitTime);

		//QAMessage(TEXT("SvrThreadUDP: Recving"));
	
		// Always recv with UDP
		iSize = sizeof(saRemoteAddr);
		if(recvfrom(sock, (char *)pBufRecv, g_dwBytesPerXfer, 0,
			(SOCKADDR *)&saRemoteAddr, &iSize) == SOCKET_ERROR)
		{
			QAError(TEXT("SvrThreadUDP: recv() failed, error %d = %s"),
				WSAGetLastError(), GetLastErrorText());
			goto exitThread;
		}
		
		if(g_dwActionType & RECV_FLAG)
		{
			//QAMessage(TEXT("SvrThreadUDP: Sending"));

			if(sendto(sock, (char *)pBufSend, g_dwBytesPerXfer, 0,
				(SOCKADDR *)&saRemoteAddr, iSize) == SOCKET_ERROR)
			{
				QAError(TEXT("SvrThreadUDP: send() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}
		}

		EnterCriticalSection(&ServerCS);
		g_dwTotalClients++;
		LeaveCriticalSection(&ServerCS);

		goto ServiceClient;
	}

exitThread:

	if(sock != INVALID_SOCKET)
	{
		shutdown(sock, 2);
		closesocket(sock);
	}

	for( i = 0; i < iNumberOfSockets; i++)
	{
		shutdown(ServiceSocket[i], 2);
		closesocket(ServiceSocket[i]);
	}

	if(pBufSend)
		free(pBufSend);

	if(pBufRecv)
		free(pBufRecv);

	QAMessage(TEXT("ServerThreadUDP--"));

	return 0;
}


/******************************************************************/
ULONG BuildTestPacket(DWORD dwLen, char Buff[], int nPacketNum) 
{
    ULONG CheckSum = 0;

	if(dwLen == 0)
		return 0;
	
	Buff[0] = (BYTE) nPacketNum;
	CheckSum += Buff[0];
	
    for (DWORD i = 1; i < dwLen; i++) 
	{
        Buff[i] = g_bNextChar;

        if (g_bNextChar == 'z')
            g_bNextChar = 'a';
        else
            g_bNextChar;

        CheckSum += Buff[i];
    }

    return(CheckSum);
}