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
#define DEFAULT_PORT			53123
#define DEFAULT_XFER_BYTES		128
#define DEFAULT_SENDTYPE		0
#define DEFAULT_SOCKTYPE		SOCK_STREAM
#define DEFAULT_SERVER			_T("localhost")

#define MAX_PORT				65535

#define TIMEOUT_MSECS			1000

LPTSTR			g_szServer;
int				g_iSockType;
DWORD			g_dwWaitTime;
DWORD			g_dwSocksBeforeClose;
DWORD			g_dwMessageFrequency;
DWORD			g_dwIterations;
WORD			g_wPort;
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
BOOL CheckForData(SOCKET sock, DWORD dwTimeoutInMs);

// Declarations for NetMain
extern "C" TCHAR* gszMainProgName = _T("ocstres2");
extern "C" DWORD  gdwMainInitFlags = INIT_NET_CONNECTION;

void PrintUsage()
{
	QAMessage(TEXT("ocstres2 [-s ServerName/Addr] [-f Freq] [-w Wait] [-n Iterations]"));
	QAMessage(TEXT("     [-c SocksB4Close] [-p Port] [-u | -x] [-ab] [-g | -h | -o | -r]"));
	QAMessage(TEXT("     [-b BytesToXfer] [-d]"));
	QAMessage(TEXT("General Options:"));
	QAMessage(TEXT("    f:  Number of interations to perform before displaying status (default: %d)"), DEFAULT_MSGTIME);
	QAMessage(TEXT("    w:  Number of milliseconds to wait between iterations (default: %d)"), DEFAULT_WAITTIME);
	QAMessage(TEXT("    n:  Number of iterations (default: %d)"), DEFAULT_ITERATIONS);
	QAMessage(TEXT("    c:  Number of sockets to open before closing (default: %d)"), DEFAULT_SOCKS_B4_CLOSE);
	QAMessage(TEXT("    p:  Port to connect/listen on (default: %d)"), DEFAULT_PORT);
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


//**********************************************************************************
int mymain(int argc, TCHAR* argv[])
{
    WSADATA WSAData;	
    HANDLE  hThreadClient, hThreadServerTCP, hThreadServerUDP;
    DWORD   dwThreadId, dwTempPort;
	BOOL	bClient, bServer;

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
	g_wPort = DEFAULT_PORT;
	g_iSockType = DEFAULT_SOCKTYPE;
	g_dwBytesPerXfer = DEFAULT_XFER_BYTES;
	g_bDaemon = FALSE;
	g_fClientThread = TRUE;
	bClient = bServer = FALSE;
	hThreadClient = hThreadServerTCP = hThreadServerUDP = NULL;

	// Parse command line
	QAGetOption(_T("s"), &g_szServer);
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

	if(QAGetOptionAsDWORD(_T("p"), &dwTempPort))
	{
		if(dwTempPort > MAX_PORT)
		{
			QAError(TEXT("Invalid port (%u) given!"), dwTempPort);
			return 1;
		}
		else
			g_wPort = (WORD)dwTempPort;
	}

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

#define NUM_FAMILIES	1
#define NUM_TYPES		2

//**********************************************************************************
DWORD WINAPI ClientThread(LPVOID *pParm)
{
	SOCKET			*sockArray = NULL;
	DWORD			dwTotalSocks, dwIPAddr, i, j;
	DWORD			cbRecvd, cbTotalRecvd;
	SOCKADDR_IN		saAddr, saRemoteAddr;
	hostent			*h;
	char			szServerASCII[256], *pBufSend = NULL, *pBufRecv = NULL;
	int				iSockType, iSize;
	DWORD			dwStats[NUM_FAMILIES][NUM_TYPES];

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

	if(g_dwActionType & CONN_FLAG)
	{
		// Only perform name resolution if we're connecting to a remote host
#if defined UNICODE
		wcstombs(szServerASCII, g_szServer, sizeof(szServerASCII));
#else
		strncpy(szServerASCII, g_szServer, sizeof(szServerASCII));
#endif
		
		// Do name resolution on the remote server name
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
				QAError (TEXT("ClientThread: Invalid address parameter = %s"), g_szServer);
				goto exitThread;
			}
		}
	}

	// Initialize loop variables
	dwTotalSocks = 0;
	
	while (dwTotalSocks < g_dwIterations)
	{
		for(i = 0; (i < g_dwSocksBeforeClose) && (dwTotalSocks < g_dwIterations); i++)
		{
			// Pause if desired
			if(g_dwWaitTime)
				Sleep(g_dwWaitTime);

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

			// open the socket
			sockArray[i] = socket(AF_INET, iSockType, 0);

			if(sockArray[i] == INVALID_SOCKET)
			{
				QAError(TEXT("ClientThread: socket() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}

			if(g_dwActionType & BIND_FLAG)
			{
				// Bind the socket to any address, any port
				saAddr.sin_family = AF_INET;
				saAddr.sin_addr.s_addr = INADDR_ANY;
				saAddr.sin_port = 0;
				
				if(bind(sockArray[i], (SOCKADDR *)&saAddr, sizeof(saAddr)) == SOCKET_ERROR)
				{
					QAError(TEXT("ClientThread: bind() failed, error %d = %s"),
						WSAGetLastError(), GetLastErrorText());
					goto exitThread;
				}
			}

			if(g_dwActionType & CONN_FLAG)
			{
				saRemoteAddr.sin_family = AF_INET;
				saRemoteAddr.sin_addr.s_addr = dwIPAddr;
				saRemoteAddr.sin_port = htons(g_wPort);
				
				// Actually connect only for TCP sockets
				if(iSockType == SOCK_STREAM)
				{
					if(connect(sockArray[i], (SOCKADDR *)&saRemoteAddr, sizeof(saRemoteAddr)) == SOCKET_ERROR)
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
					if((DWORD) sendto(sockArray[i], (char *)pBufSend, g_dwBytesPerXfer, 0,
						(SOCKADDR *)&saRemoteAddr, sizeof(saRemoteAddr)) != g_dwBytesPerXfer)
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
					// do a select so we don't hang if a packet is lost
					if(!CheckForData(sockArray[i], TIMEOUT_MSECS))
					{
						QAWarning(TEXT("ClientThread: timed out waiting for packet..."),
							WSAGetLastError(), GetLastErrorText());
						continue;
					}

					iSize = sizeof(saRemoteAddr);
					if((DWORD) recvfrom(sockArray[i], (char *)pBufRecv, g_dwBytesPerXfer, 0,
						(SOCKADDR *)&saRemoteAddr, &iSize) != g_dwBytesPerXfer)
					{
						QAError(TEXT("ClientThread: recvfrom() failed, error %d = %s"),
							WSAGetLastError(), GetLastErrorText());
						goto exitThread;
					}
				}
			}

			// Perform maintenance tasks
			dwTotalSocks++;

			dwStats[0][(iSockType == SOCK_STREAM) ? 0 : 1]++;

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
	}

exitThread:

	QAMessage(TEXT("--------------------------------------------------------"));
	QAMessage(TEXT("open/%s%s%s%sclose performed a total of %u time(s)"),
		(g_dwActionType & BIND_FLAG) ? _T("bind/") : _T(""),
		(g_dwActionType & CONN_FLAG) ? _T("conn/") : _T(""),
		(g_dwActionType & SEND_FLAG) ? _T("send/") : _T(""),
		(g_dwActionType & RECV_FLAG) ? _T("recv/") : _T(""),
		dwTotalSocks);
	QAMessage(TEXT("                  TCP |         UDP"));
	QAMessage(TEXT("PF_INET    %10d |  %10d"), dwStats[0][0], dwStats[0][1]);

	for(i = 0; i < g_dwSocksBeforeClose; i++)
	{
		if(sockArray[i] != INVALID_SOCKET)
		{
			shutdown(sockArray[i], 2);
			closesocket(sockArray[i]);
		}
	}

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
	SOCKET			sock = INVALID_SOCKET, sockListen;
	SOCKADDR_IN		saAddr, saRemoteAddr;
	char			*pBufSend = NULL, *pBufRecv = NULL;
	int				iSize;
	DWORD			cbRecvd, cbTotalRecvd;

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

	// create the listener socket
	sockListen = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sockListen == INVALID_SOCKET)
	{
		QAError(TEXT("SvrThreadTCP: socket() failed, error %d = %s"),
			WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}
	
	// Bind the socket to any address, specified port
	saAddr.sin_family = AF_INET;
	saAddr.sin_addr.s_addr = INADDR_ANY;
	saAddr.sin_port = htons(g_wPort);
	
	if(bind(sockListen, (SOCKADDR *)&saAddr, sizeof(saAddr)) == SOCKET_ERROR)
	{
		QAError(TEXT("SvrThreadTCP: bind() failed, error %d = %s"),
			WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}

	// Start listening on the socket (backlog of 5 is the max on CE)
	if(listen(sockListen, 5) == SOCKET_ERROR)
	{
		QAError(TEXT("SvrThreadTCP: listen() failed, error %d = %s"),
			WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}
	
	while ( g_bDaemon || (g_fClientThread && g_dwTotalClients < g_dwIterations) )
	{
		if(!CheckForData(sockListen, TIMEOUT_MSECS))
			continue;

		//QAMessage(TEXT("SvrThreadTCP: accept()'ing"));
		
		// Accept an incoming connection
		iSize = sizeof(saRemoteAddr);
		sock = accept(sockListen, (SOCKADDR *)&saRemoteAddr, &iSize);
		if(sock == INVALID_SOCKET)
		{
			QAError(TEXT("SvrThreadTCP: accept() failed, error %d = %s"),
				WSAGetLastError(), GetLastErrorText());
			goto exitThread;
		}

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
	}

exitThread:

	if(sock != INVALID_SOCKET)
	{
		shutdown(sock, 2);
		closesocket(sock);
	}

	closesocket(sockListen);

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
	SOCKET			sock = INVALID_SOCKET;
	SOCKADDR_IN		saAddr, saRemoteAddr;
	char			*pBufSend = NULL, *pBufRecv = NULL;
	int				iSize;

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

	// create the UDP socket
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	
	if(sock == INVALID_SOCKET)
	{
		QAError(TEXT("SvrThreadUDP: socket() failed, error %d = %s"),
			WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}
	
	// Bind the socket to any address, specified port (TCP and UDP receive on same port)
	saAddr.sin_family = AF_INET;
	saAddr.sin_addr.s_addr = INADDR_ANY;
	saAddr.sin_port = htons(g_wPort);
	
	if(bind(sock, (SOCKADDR *)&saAddr, sizeof(saAddr)) == SOCKET_ERROR)
	{
		QAError(TEXT("SvrThreadUDP: bind() failed, error %d = %s"),
			WSAGetLastError(), GetLastErrorText());
		goto exitThread;
	}
	
	while ( g_bDaemon || (g_fClientThread && g_dwTotalClients < g_dwIterations) )
	{
		if(!CheckForData(sock, TIMEOUT_MSECS))
			continue;

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
				(SOCKADDR *)&saRemoteAddr, sizeof(saRemoteAddr)) == SOCKET_ERROR)
			{
				QAError(TEXT("SvrThreadUDP: send() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}
		}

		EnterCriticalSection(&ServerCS);
		g_dwTotalClients++;
		LeaveCriticalSection(&ServerCS);
	}

exitThread:

	if(sock != INVALID_SOCKET)
	{
		shutdown(sock, 2);
		closesocket(sock);
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

BOOL CheckForData(SOCKET sock, DWORD dwTimeoutInMs)
{
	fd_set fdReadSet;
	TIMEVAL timeout = {dwTimeoutInMs / 1000, (dwTimeoutInMs % 1000) * 1000};

	FD_ZERO(&fdReadSet);
	FD_SET(sock, &fdReadSet);
	if(select(0, &fdReadSet, NULL, NULL, &timeout) != 1)
		return FALSE;

	return TRUE;
}