//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tchar.h>
#include <stressutils.h>

#define WINSOCK_VERSION_REQ		MAKEWORD(2,2)

#define STATUS_FREQUENCY		1000		// print out our status every time this number
											// of iterations has been performed

#define DEFAULT_WAITTIME		50			// Default time to wait (in ms) between binds.
											// This is used to lessen the chance that
											// multiple, concurrent bind tests will
											// interfere with one another and to prevent
											// the test from cycling thru the ports too
											// quickly (WSAEADDRINUSE).

#define MIN_PORT				1			// Defines the port range in which to bind
#define MAX_PORT				65535		// sockets

#define MAX_ADDRS				100			// Maximum number of addresses to choose
											// from for binding purposes

// Function Prototypes
void AddAddrsToArray(ADDRINFO *pAddrInfo);
BOOL Setup(void);
UINT DoStressIteration (void);
BOOL Cleanup(void);
int GetRandomSockType(void);

// Global Variables
ADDRINFO	*g_rgaiAddrs[MAX_ADDRS],		// We use an array of ADDRINFO pointers to 
											// speed up accessing random addrs for bind()
			*g_paiLocalAddrs,				// list of local addresses
			*g_paiUnspecAddrs,				// list of INADDR_ANY addresses
			*g_paiLoopbackAddrs;			// list of loopback ("localhost") addresses
DWORD		g_dwNumAddrs;

//
//***************************************************************************************

int WINAPI WinMain ( 
					HINSTANCE hInstance,
					HINSTANCE hInstPrev,
#ifdef UNDER_CE
					LPWSTR wszCmdLine,
#else
					LPSTR szCmdLine,
#endif
					int nCmdShow
					)
{
	DWORD			dwWaitTime, dwStartTime = GetTickCount();
	UINT			ret;        
	LPTSTR			tszCmdLine;
	int				retValue = 0;	// Module return value.
									// You should return 0 unless you had to abort.
	MODULE_PARAMS	mp;				// Cmd line args arranged here.
									// Used to init the stress utils.
	STRESS_RESULTS	results;		// Cumulative test results for this module.
	DWORD			dwIterations;

	ZeroMemory((void*) &mp, sizeof(mp));
	ZeroMemory((void*) &results, sizeof(results));

	
	// NOTE:  Modules should be runnable on desktop whenever possible,
	//        so we need to convert to a common cmd line char width:

#ifdef UNDER_NT
	tszCmdLine =  GetCommandLine();
#else
	tszCmdLine  = wszCmdLine;
#endif



	///////////////////////////////////////////////////////
	// Initialization
	//

	// Read the cmd line args into a module params struct.
	// You must call this before InitializeStressUtils().

	// Be sure to use the correct version, depending on your 
	// module entry point.

	if ( !ParseCmdLine_WinMain (tszCmdLine, &mp) )
	{
		LogFail(TEXT("Failure parsing the command line!  exiting ..."));
		return CESTRESS_ABORT; // Use the abort return code
	}


	// You must call this before using any stress utils 
	InitializeStressUtils (
							_T("wsx_bind"), // Module name to be used in logging
							LOGZONE(SLOG_SPACE_NET, SLOG_WINSOCK),     // Logging zones used by default
							&mp          // Module params passed on the cmd line
							);


	// TODO: Module-specific parsing of tszUser ...
	InitUserCmdLineUtils(mp.tszUser, NULL);
	if(!User_GetOptionAsDWORD(_T("w"), &dwWaitTime))
		dwWaitTime = DEFAULT_WAITTIME;

	if(!Setup())
		return CESTRESS_ABORT;

	LogVerbose (TEXT("Starting at tick = %u"), dwStartTime);

	///////////////////////////////////////////////////////
	// Main test loop
	//

	dwIterations = 0;

	while ( CheckTime(mp.dwDuration, dwStartTime) )
	{
		dwIterations++;

		// Pause if we're supposed to
		if(dwWaitTime)
			Sleep(dwWaitTime);

		ret = DoStressIteration();

		if (ret == CESTRESS_ABORT)
		{
			LogFail (TEXT("Aborting on test iteration %i!"), results.nIterations);
			retValue = CESTRESS_ABORT;
			break;
		}

		RecordIterationResults (&results, ret);

		if(dwIterations % STATUS_FREQUENCY == 0)
			LogVerbose(TEXT("completed %u iterations"), dwIterations);
	}



	///////////////////////////////////////////////////////
	// Clean-up, report results, and exit
	//
	Cleanup();
	
	DWORD dwEnd = GetTickCount();
	LogVerbose (TEXT("Leaving at tick = %d, duration (min) = %d"),
					dwEnd,
					(dwEnd - dwStartTime) / 60000
					);

	// DON'T FORGET:  You must report your results to the harness before exiting
	ReportResults (&results);

	return retValue;
}


void AddAddrsToArray(ADDRINFO *pAddrInfo)
{
	for(ADDRINFO *pAI = pAddrInfo; pAI != NULL && g_dwNumAddrs < MAX_ADDRS; pAI = pAI->ai_next)
		g_rgaiAddrs[g_dwNumAddrs++] = pAI;
}

BOOL Setup (void)
{
	WSADATA WSAData;
	ADDRINFO Hints;
	char szNameASCII[64];			// Host names shouldn't be more than 16 characters
									// long, but we over-allocate just to be safe.

	// Initialize global and local variables
	g_dwNumAddrs = 0;
	g_paiUnspecAddrs = g_paiLoopbackAddrs = g_paiLocalAddrs = NULL;
	memset(g_rgaiAddrs, 0, sizeof(g_rgaiAddrs));
	memset(&Hints, 0, sizeof(Hints));

	if(WSAStartup(WINSOCK_VERSION_REQ, &WSAData) != 0)
	{
		LogFail(TEXT("WSAStartup Failed"));
		return FALSE;
	}

	// Setup Unspec Addrs
	Hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	Hints.ai_family = PF_UNSPEC;
	if(getaddrinfo(NULL, "100", &Hints, &g_paiUnspecAddrs))
		LogWarn2(TEXT("getaddrinfo(Unspec) failed, error = %d"), WSAGetLastError());

	AddAddrsToArray(g_paiUnspecAddrs);

	// Setup Loopback Addrs
	Hints.ai_flags = 0;
	if(getaddrinfo("localhost", "100", &Hints, &g_paiLoopbackAddrs))
		LogWarn2(TEXT("getaddrinfo(Loopback) failed, error = %d"), WSAGetLastError());

	AddAddrsToArray(g_paiLoopbackAddrs);

	// Setup Local Addrs
	if(gethostname(szNameASCII, sizeof(szNameASCII)))
		LogWarn1(TEXT("gethostname() failed, error = %d\r\n"), WSAGetLastError());
	else
	{
		if(getaddrinfo(szNameASCII, "100", &Hints, &g_paiLocalAddrs))
			LogWarn2(TEXT("getaddrinfo(Local) failed, error = %d"), WSAGetLastError());

		AddAddrsToArray(g_paiLocalAddrs);
	}

	if(g_dwNumAddrs == 0)
	{
		LogFail(TEXT("FATAL ERROR - Couldn't get any addresses to bind to - FATAL ERROR"));
		return FALSE;
	}

	return TRUE;
}


UINT DoStressIteration (void)
{
	UINT uRet = CESTRESS_PASS, i = GetRandomRange(0, g_dwNumAddrs - 1), nSockType;
	SOCKET sock = INVALID_SOCKET;
	ADDRINFO *pAI = g_rgaiAddrs[i];
	WORD wPort;

	if(!pAI)
	{
		LogFail(TEXT("g_rgaiAddrs[%d] is NULL!"), i);
		uRet = CESTRESS_FAIL;
		goto exit;
	}

	// Create the socket with a random socket type
	nSockType = GetRandomSockType();
	sock = socket(pAI->ai_family, nSockType, pAI->ai_protocol);
	if(sock == INVALID_SOCKET)
	{
		LogWarn1(TEXT("socket() failed, error = %d"), WSAGetLastError());
		uRet = CESTRESS_WARN2;
		goto exit;
	}

	// Try to bind the socket to the randomly chosen address and port
	wPort = (WORD)GetRandomRange(MIN_PORT, MAX_PORT);

	if(pAI->ai_family == PF_INET)
		((SOCKADDR_IN *)(pAI->ai_addr))->sin_port = htons(wPort);
	else
		((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_port = htons(wPort);

	if(bind(sock, pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
	{
		if(WSAGetLastError() == WSAEADDRINUSE)
		{
			LogWarn2(TEXT("bind() failed: port %d (WSAEADDRINUSE, someone using that port)"),
				wPort, WSAGetLastError());
			uRet = CESTRESS_WARN2;
		}
		else
		{
			LogFail(TEXT("bind() failed for port %d, error = %d"), wPort, WSAGetLastError());
			uRet = CESTRESS_FAIL;
		}
	}

exit:

	if(sock != INVALID_SOCKET)
	{
		if(closesocket(sock) == SOCKET_ERROR)
		{
			LogWarn1(TEXT("closesocket() failed, error = %d"), WSAGetLastError());
			if(uRet == CESTRESS_PASS)
				uRet = CESTRESS_WARN1;
		}
	}

	return uRet;
}


BOOL Cleanup (void)
{
	if(g_paiUnspecAddrs)
		freeaddrinfo(g_paiUnspecAddrs);

	if(g_paiLoopbackAddrs)
		freeaddrinfo(g_paiLoopbackAddrs);

	if(g_paiLocalAddrs)
		freeaddrinfo(g_paiLocalAddrs);

	if(WSACleanup())
		return FALSE;

	return TRUE;
}

#define NUM_TYPES				2			// Number of socket types we will be using

int GetRandomSockType(void)
{
	int n = GetRandomRange(0, NUM_TYPES - 1);
	switch(n)
	{
	case 0:
		return SOCK_STREAM;
	case 1:
		return SOCK_DGRAM;
	}

	return 0;
}
