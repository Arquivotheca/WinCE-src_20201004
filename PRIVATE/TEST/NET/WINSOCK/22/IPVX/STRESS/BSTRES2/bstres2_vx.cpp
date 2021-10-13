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

LPSTR			g_pLocalAddrASCII;
int				g_iFamily, g_iSockType;
DWORD			g_dwWaitTime;
DWORD			g_dwBindsBeforeClose;
DWORD			g_dwMessageFrequency;
DWORD			g_dwIterations;
WORD			g_wBasePort;

// Function Prototypes
DWORD   WINAPI      BindThread(LPVOID *pParm);

// Declarations for NetMain
extern "C" TCHAR* gszMainProgName = _T("bstres2_vX");
extern "C" DWORD  gdwMainInitFlags = 0;

void PrintUsage()
{
	QAMessage(TEXT("bstres2_vX [-f Frequency] [-m Milliseconds] [-n NumIterations] [-a | -w | -l LocalAddress] [-u | -x]"));
	QAMessage(TEXT("    f:  Number of interations to perform before displaying status (default: %d)"), DEFAULT_MSGTIME);
	QAMessage(TEXT("    m:  Number of milliseconds to wait between iterations (default: %d)"), DEFAULT_WAITTIME);
	QAMessage(TEXT("    n:  Number of binds to execute (default: %d)"), DEFAULT_ITERATIONS);
	QAMessage(TEXT("    c:  Number of sockets to bind before closing (default: %d)"), DEFAULT_BIND_B4_CLOSE);
	QAMessage(TEXT("    b:  Base port to start binding at (default: %d)"), DEFAULT_BASEPORT);
	QAMessage(TEXT("    u:  Bind datagram (UDP) sockets (default is TCP)"));
	QAMessage(TEXT("    x:  Bind random mixed socket types (UDP/TCP)"));
	QAMessage(TEXT("    i:  Version of IP to use 4, 6, or X (default: X)"));
	QAMessage(TEXT("    l:  Bind socket to specified local address"));
	QAMessage(TEXT("    a:  Bind socket to an automatically obtained local address"));
	QAMessage(TEXT("        By default the test binds the socket to the unspecified address INADDR_ANY"));
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
    HANDLE  hThread;
    DWORD   ThreadId;
	LPTSTR	szLocalAddress;
	DWORD	dwTempPort;
	char	szAddrASCII[MAX_ADDR_SIZE];
	LPTSTR	szIPVersion;

	if(QAWasOption(_T("?")) || QAWasOption(_T("help")))
	{
		PrintUsage();
		return 0;
	}

	// Initialize variables
	g_pLocalAddrASCII = NULL;
	g_iSockType = PF_UNSPEC;
	g_iSockType = DEFAULT_SOCKTYPE;
	g_dwWaitTime = DEFAULT_WAITTIME;
	g_dwBindsBeforeClose = DEFAULT_BIND_B4_CLOSE;
	g_dwMessageFrequency = DEFAULT_MSGTIME;
	g_dwIterations = DEFAULT_ITERATIONS;
	g_wBasePort = DEFAULT_BASEPORT;

	szLocalAddress = _T("");

	// Parse command line
	QAGetOptionAsDWORD(_T("m"), &g_dwWaitTime);
	QAGetOptionAsDWORD(_T("n"), &g_dwIterations);

	if(QAGetOptionAsDWORD(_T("f"), &g_dwMessageFrequency) && !g_dwMessageFrequency)
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
		g_iSockType = SOCK_DGRAM;
	
	if(QAWasOption(_T("x")))
		g_iSockType = RANDOM_SOCKTYPE;

	if(WSAStartup(WINSOCK_VERSION_REQ, &WSAData) != 0)
	{
		QAError ( TEXT("WSAStartup Failed\n"));
		return 1;
	}
    else
    {
        QAMessage ( TEXT("WSAStartup SUCCESS\n"));
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
		}
	}

	if(QAWasOption(_T("a")))
	{
		if(gethostname(szAddrASCII, MAX_ADDR_SIZE - 1))
		{
			QAError(TEXT("Error getting local host name, error = %d\r\n"), WSAGetLastError());
			goto exit; 
		}
		else
			g_pLocalAddrASCII = szAddrASCII;
	}
	else if(QAGetOption(_T("l"), &szLocalAddress))
	{
#ifdef UNICODE
		wcstombs(szAddrASCII, szLocalAddress, sizeof(szAddrASCII));
#else
		strncpy(szAddrASCII, szLocalAddress, sizeof(szAddrASCII));
#endif
		g_pLocalAddrASCII = szAddrASCII;
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

#define NUM_FAMILIES	3
#define NUM_TYPES		2

//**********************************************************************************
DWORD WINAPI BindThread(LPVOID *pParm)
{
	SOCKET			*sockArray;
	DWORD			dwTotalBound, dwTCPBound, i, j;
	int				iSockType;
	char			szPortASCII[10];
	ADDRINFO		AddrHints, *pAddrInfo, *pAI;
	DWORD			dwStats[NUM_FAMILIES][NUM_TYPES];
	
	// For debug printing
	struct in_addr6	pAddr;
	BOOL			fFirstRep;

	QAMessage(TEXT("BindThread++"));

	sockArray = (SOCKET *) malloc( sizeof(SOCKET) * g_dwBindsBeforeClose );

	if(sockArray == NULL)
	{
		QAError(TEXT("Could not allocate enough memory for socket array!"));
		return 1;
	}

	for(i = 0; i < g_dwBindsBeforeClose; i++)
		sockArray[i] = INVALID_SOCKET;

	memset(&dwStats, 0, sizeof(dwStats));

	dwTotalBound = 0;
	dwTCPBound = 0;
	i = 0;
	fFirstRep = TRUE;
	
	while(dwTotalBound < g_dwIterations)
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
		
		memset(&AddrHints, 0, sizeof(AddrHints));
		AddrHints.ai_family = g_iFamily;
		AddrHints.ai_socktype = iSockType;
		AddrHints.ai_flags = AI_PASSIVE;

		_itoa((g_wBasePort + dwTotalBound) % MAX_PORT, szPortASCII, 10);
		
		if(getaddrinfo(g_pLocalAddrASCII, szPortASCII, &AddrHints, &pAddrInfo))
		{
			QAError(TEXT("getaddrinfo(%hs) error %d = %s"), g_pLocalAddrASCII, WSAGetLastError(), GetLastErrorText());
			goto exitThread;
		}

		for(pAI = pAddrInfo; pAI != NULL && dwTotalBound < g_dwIterations; pAI = pAI->ai_next)
		{
			// create the socket
			sockArray[i] = socket(pAI->ai_family, pAI->ai_socktype, pAI->ai_protocol);
			
			if(sockArray[i] == INVALID_SOCKET)
			{
				QAError(TEXT("socket() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}

			//QAMessage(TEXT("Address Family = %s"), (pAI->ai_family == PF_UNSPEC) ? _T("PF_UNSPEC") : ((pAI->ai_family == PF_INET6) ? _T("PF_INET6") : (pAI->ai_family == PF_INET) ? _T("PF_INET") : _T("Unknown")));
			
			// Perform the bind			
			if(bind(sockArray[i], pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
			{
				QAError(TEXT("bind() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
				goto exitThread;
			}
			
			// Perform maintenance tasks
			dwTotalBound++;
			i++;

			dwStats[GetFamilyIndex(pAI->ai_family)][GetTypeIndex(iSockType)]++;

			if(fFirstRep)
			{
				if(pAI->ai_family == PF_INET)
				{
					memcpy(&pAddr, &(((SOCKADDR_IN *)(pAI->ai_addr))->sin_addr), sizeof(IN_ADDR));
					QAMessage(TEXT("[%d] bound to AF_INET addr[%08x]"), dwTotalBound, htonl(*((DWORD *)&pAddr)));
				}
				else
				{
					memcpy(&pAddr, &(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr), sizeof(struct in_addr6));
					QAMessage(TEXT("[%d] bound to AF_INET6 addr[%04x%04x%04x%04x%04x%04x%04x%04x]"), dwTotalBound, 
						htons((*(WORD *)&(pAddr.s6_addr[0]))), htons((*(WORD *)&(pAddr.s6_addr[2]))),
						htons((*(WORD *)&(pAddr.s6_addr[4]))), htons((*(WORD *)&(pAddr.s6_addr[6]))),
						htons((*(WORD *)&(pAddr.s6_addr[8]))), htons((*(WORD *)&(pAddr.s6_addr[10]))),
						htons((*(WORD *)&(pAddr.s6_addr[12]))), htons((*(WORD *)&(pAddr.s6_addr[14]))) );
				}
			}

			if(i == g_dwBindsBeforeClose)
			{
				for(j = 0; j < i; j++)
				{
					if(closesocket(sockArray[j]) == SOCKET_ERROR)
					{
						QAError(TEXT("closesocket() failed, error %d = %s"), WSAGetLastError(), GetLastErrorText());
					}
					sockArray[j] = INVALID_SOCKET;
				}

				i = 0;
			}

			if((dwTotalBound % g_dwMessageFrequency == 0) && dwTotalBound > 0)
			{
				QAMessage(TEXT("bind performed a total of %u time(s)"), dwTotalBound);
			}
		}

		fFirstRep = FALSE;

		freeaddrinfo(pAddrInfo);
	}

exitThread:

	QAMessage(TEXT("----------------------------------------------"));
	QAMessage(TEXT("bind performed a total of %u time(s)"), dwTotalBound);
	QAMessage(TEXT("                 TCP  |        UDP"));
	QAMessage(TEXT("PF_INET        %10d   |      %10d"), dwStats[0][0], dwStats[0][1]);
	QAMessage(TEXT("PF_INET6       %10d   |      %10d"), dwStats[1][0], dwStats[1][1]);
	QAMessage(TEXT("Unknown        %10d   |      %10d"), dwStats[2][0], dwStats[2][1]);

	for(i = 0; i < g_dwBindsBeforeClose; i++)
	{
		if(sockArray[i] != INVALID_SOCKET)
			closesocket(sockArray[i]);
	}

	free(sockArray);

	QAMessage(TEXT("BindThread--"));

	return 0;
}