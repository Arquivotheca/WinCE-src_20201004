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

#define RANDOM_SOCKTYPE			-1

#define	DEFAULT_MSGTIME			100					// Number of binds before displaying status
													// Do not set to 0
#define DEFAULT_WAITTIME		0
#define DEFAULT_ITERATIONS		1000
#define DEFAULT_BIND_B4_CLOSE	1					// Number of binds before closing open sockets
													// Do not set to 0
#define DEFAULT_SOCKTYPE		SOCK_STREAM
#define DEFAULT_BASEPORT		18000

#define MAX_PORT				65535

#define MAX_ADDR_SIZE			128

SOCKADDR_IN		g_saBindAddr;
int				g_nSockType;
DWORD			g_dwWaitTime;
DWORD			g_dwBindsBeforeClose;
DWORD			g_dwMessageFrequency;
DWORD			g_dwIterations;
WORD			g_wBasePort;

// Function Prototypes
DWORD   WINAPI      BindThread(LPVOID *pParm);

// Declarations for NetMain
extern "C" TCHAR* gszMainProgName = _T("bstres2");
extern "C" DWORD  gdwMainInitFlags = 0;

void PrintUsage()
{
	QAMessage(TEXT("bstres2 [-f Frequency] [-m Milliseconds] [-n NumIterations] [-a | -w | -l LocalAddress] [-u | -x]"));
	QAMessage(TEXT("    f:  Number of interations to perform before displaying status (default: %d)"), DEFAULT_MSGTIME);
	QAMessage(TEXT("    m:  Number of milliseconds to wait between iterations (default: %d)"), DEFAULT_WAITTIME);
	QAMessage(TEXT("    n:  Number of binds to execute (default: %d)"), DEFAULT_ITERATIONS);
	QAMessage(TEXT("    c:  Number of sockets to bind before closing (default: %d)"), DEFAULT_BIND_B4_CLOSE);
	QAMessage(TEXT("    b:  Base port to start binding at (default: %d)"), DEFAULT_BASEPORT);
	QAMessage(TEXT("    u:  Bind datagram (UDP) sockets (default is TCP)"));
	QAMessage(TEXT("    x:  Bind random mixed socket types (UDP/TCP)"));
	QAMessage(TEXT("    l:  Bind socket to specified local address"));
	QAMessage(TEXT("    a:  Bind socket to an automatically obtained local address"));
	QAMessage(TEXT("        By default the test binds the socket to the unspecified address INADDR_ANY"));
}


//**********************************************************************************
int mymain(int argc, TCHAR* argv[])
{
    WSADATA WSAData;	
    HANDLE  hThread;
    DWORD   ThreadId;
	LPTSTR	szLocalAddress;
	DWORD	dwTempPort;
	char	szAddrASCII[MAX_ADDR_SIZE];

	if(QAWasOption(_T("?")) || QAWasOption(_T("help")))
	{
		PrintUsage();
		return 0;
	}

	// Initialize variables
	memset(&g_saBindAddr, 0, sizeof(g_saBindAddr));
	g_nSockType = DEFAULT_SOCKTYPE;
	g_dwWaitTime = DEFAULT_WAITTIME;
	g_dwBindsBeforeClose = DEFAULT_BIND_B4_CLOSE;
	g_dwMessageFrequency = DEFAULT_MSGTIME;
	g_dwIterations = DEFAULT_ITERATIONS;
	g_wBasePort = DEFAULT_BASEPORT;

	szLocalAddress = _T("");

	// Parse command line
	QAGetOptionAsDWORD(_T("m"), &g_dwWaitTime);
	QAGetOptionAsDWORD(_T("n"), &g_dwIterations);

	if(QAGetOptionAsDWORD(_T("f"), &g_dwMessageFrequency) && g_dwMessageFrequency == 0)
		g_dwMessageFrequency = 1;	// MessageFrequency can not be 0

	if(QAGetOptionAsDWORD(_T("c"), &g_dwBindsBeforeClose) && g_dwMessageFrequency == 0)
		g_dwBindsBeforeClose = 1;	// BindsBeforeClose can not be 0

	if(QAGetOptionAsDWORD(_T("b"), &dwTempPort))
	{
		if(dwTempPort > MAX_PORT)
		{
			QAError(TEXT("Invalid port (%u) given!"), dwTempPort);
			return 1;
		}
		else
			g_wBasePort = (WORD)dwTempPort;
	}
	
	if(QAWasOption(_T("u")))
		g_nSockType = SOCK_DGRAM;
	
	if(QAWasOption(_T("x")))
		g_nSockType = RANDOM_SOCKTYPE;

	if(WSAStartup(WINSOCK_VERSION_REQ, &WSAData) != 0)
	{
		QAError ( TEXT("WSAStartup Failed\n"));
		return 1;
	}
    else
    {
        QAMessage ( TEXT("WSAStartup SUCCESS\n"));
    }

	if(QAWasOption(_T("a")))
	{
		HOSTENT *pHostEnt;

		// Bind to an automatically obtained local address
		if(gethostname(szAddrASCII, MAX_ADDR_SIZE) == SOCKET_ERROR)
		{
			QAError(TEXT("gethostname() failed, error %d = %s"), 
				WSAGetLastError(), GetLastErrorText());
			goto exit;
		}
		
		if((pHostEnt = gethostbyname(szAddrASCII)) == NULL)
		{
			QAError(TEXT("gethostbyname() for %hs failed, error %d = %s"), szAddrASCII,
				WSAGetLastError(), GetLastErrorText());
			goto exit;
		}
		else
		{
			g_saBindAddr.sin_family = AF_INET;
			memcpy(&g_saBindAddr.sin_addr.s_addr, pHostEnt->h_addr_list[0], pHostEnt->h_length);
			QAMessage(TEXT("Setting local binding address to 0x%x"), g_saBindAddr.sin_addr.s_addr);
		}
	}
	else if(QAGetOption(_T("l"), &szLocalAddress))
	{
#ifdef UNICODE
		wcstombs(szAddrASCII, szLocalAddress, sizeof(szAddrASCII));
#else
		strncpy(szAddrASCII, szLocalAddress, sizeof(szAddrASCII));
#endif
		// Bind to a specified local address
		g_saBindAddr.sin_addr.s_addr = inet_addr(szAddrASCII);

		if(g_saBindAddr.sin_addr.s_addr == INADDR_NONE 
			|| g_saBindAddr.sin_addr.s_addr == 0)
		{
			QAError(TEXT("Invalid address parameter = %hs"), szAddrASCII);
			goto exit;
		}

		g_saBindAddr.sin_family = AF_INET;
	}
	else
	{
		// Bind to the unspecified address
		g_saBindAddr.sin_family = AF_INET;
		g_saBindAddr.sin_addr.s_addr = INADDR_ANY;
	}

	//
	// We use a worker thread here to maintain format compatibility with the other
	// Winsock stress tests.  Currently this extra thread is not useful though and
	// adds no value, but it may be used in the future to test simultaneous binds
	// on multiple threads.
	//

	if ((hThread = 
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) BindThread,
		NULL, 0, &ThreadId)) == NULL)
	{
		QAError (TEXT("CreateThread(BindThread) failed %d"),
			GetLastError());
		
		goto exit;
	}

	QAMessage (TEXT("Waiting for completion."));

	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);

exit:

	WSACleanup();

	return 0;
}

//**********************************************************************************
DWORD WINAPI BindThread(LPVOID *pParm)
{
	SOCKET			*sockArray;
	DWORD			dwTotalBound, dwTCPBound, i, j;
	int				iSockType;

	QAMessage(TEXT("BindThread++"));

	sockArray = (SOCKET *) malloc( sizeof(SOCKET) * g_dwBindsBeforeClose );

	if(sockArray == NULL)
	{
		QAError(TEXT("Could not allocate enough memory for socket array!"));
		return 1;
	}

	for(i = 0; i < g_dwBindsBeforeClose; i++)
		sockArray[i] = INVALID_SOCKET;

	dwTotalBound = 0;
	dwTCPBound = 0;
	
	while (dwTotalBound < g_dwIterations)
	{
		for(i = 0; (i < g_dwBindsBeforeClose) && (dwTotalBound < g_dwIterations); i++)
		{
			// Pause if desired
			if(g_dwWaitTime)
				Sleep(g_dwWaitTime);

			// Figure out our socktype for this iteration
			if(g_nSockType == RANDOM_SOCKTYPE)
			{
				if(GetRandomNumber(1) == 0)
					iSockType = SOCK_STREAM;
				else
					iSockType = SOCK_DGRAM;
			}
			else
				iSockType = g_nSockType;

			// create the socket
			sockArray[i] = socket(AF_INET, iSockType, 0);

			if(sockArray[i] == INVALID_SOCKET)
			{
				QAError(TEXT("socket() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}

			// Perform the bind
			g_saBindAddr.sin_port = htons((WORD)((dwTotalBound + g_wBasePort) % MAX_PORT));

			if(bind(sockArray[i], (SOCKADDR *)&g_saBindAddr, sizeof(g_saBindAddr)) == SOCKET_ERROR)
			{
				QAError(TEXT("bind() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}

			// Perform maintenance tasks
			dwTotalBound++;

			if(iSockType == SOCK_STREAM)
				dwTCPBound++;

			if((dwTotalBound % g_dwMessageFrequency == 0) && dwTotalBound > 0)
			{
				QAMessage(TEXT("bind performed %u time(s) - %u TCP, %u UDP"),
					dwTotalBound, dwTCPBound, dwTotalBound - dwTCPBound);
			}
		}

		// Close the sockets opened
		for(j = 0; j < i; j++)
		{
			if(closesocket(sockArray[j]) == SOCKET_ERROR)
			{
				QAError(TEXT("closesocket() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
			}
			sockArray[j] = INVALID_SOCKET;
		}
	}

exitThread:

	for(i = 0; i < g_dwBindsBeforeClose; i++)
	{
		if(sockArray[i] != INVALID_SOCKET)
			closesocket(sockArray[i]);
	}

	free(sockArray);

	QAMessage(TEXT("BindThread--"));

	return 0;
}