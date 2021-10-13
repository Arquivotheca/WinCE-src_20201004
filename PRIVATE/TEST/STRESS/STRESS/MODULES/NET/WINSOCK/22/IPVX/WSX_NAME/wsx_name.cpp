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
#include <stressutils.h>

#ifndef UNDER_CE
#include <tchar.h>
#endif

#define THREAD_WAIT_INTERVAL	(60 * 1000)		// Time (in ms) to wait for each worker
												// thread at the end of the test

#define PAUSE_MS				5000			// Time (in ms) to wait between resolution
												// attempts

#define WINSOCK_VERSION_REQ		MAKEWORD(2,2)

#define JUNK_PORT				"100"			// Any valid port number will do since we
												// only want to resolve names/addrs

#define DEFAULT_TARGET			_T("localhost")		// Default name/address to resolve

#define DEFAULT_THREADS			4				// Default number of worker test threads

#define MAX_RANDOM_NAME_LEN		63				// Maximum size host name to generate
												// when resolving a random name
												// NOTE - DNS cannot handle names larger
												// than 63 characters


// Global Variables
HANDLE				g_hTimesUp = NULL;
CRITICAL_SECTION	g_csResults;
char				g_szHostName[NI_MAXHOST];
ADDRINFO			*g_paiAddrs;
DWORD				g_dwNumAddrs;
BOOL				g_fSpecifiedOnly;


// Function Prototypes
BOOL Setup(LPSTR szTargetASCII);
BOOL Cleanup();

BOOL GetAddrFromList(SOCKADDR_STORAGE *pssAddr, int *pcbAddrSize);
BOOL MakeRandomAddr(SOCKADDR_STORAGE *pssAddr, int *pcbAddrSize);
BOOL GetLocalAddr(SOCKADDR_STORAGE *pssAddr, int *pcbAddrSize);
BOOL MakeRandomName(LPSTR pBuf, int cbBufSize);
void AddrToString(SOCKADDR *psaAddr, TCHAR szAddrString[]);
UINT DoGetNameStressIteration (void);
UINT DoGetAddrStressIteration (void);
DWORD WINAPI TestThreadProc (LPVOID pv);

#define MIN(x,y)	((x < y) ? x : y)

//
//***************************************************************************************



int wmain( int argc, WCHAR** argv )
{
	int				retValue = 0;  // Module return value.  You should return 0 unless you had to abort.
	MODULE_PARAMS	mp;            // Cmd line args arranged here.  Used to init the stress utils.
	STRESS_RESULTS	results;       // Cumulative test results for this module.
	
	DWORD			dwEnd;         // End time	 
	DWORD			dwStartTime = GetTickCount();
	DWORD			dwExited, i, dwThreadId, dwThreads;
	HANDLE			*hThread;
	LPTSTR			szTarget;
	char			szTargetASCII[NI_MAXHOST];



	///////////////////////////////////////////////////////
	// Initialization
	//

	memset(&mp, 0, sizeof(mp));
	memset(&results, 0, sizeof(results));

	szTarget = DEFAULT_TARGET;
	dwThreads = DEFAULT_THREADS;
	g_paiAddrs = NULL;
	g_dwNumAddrs = 0;
	g_fSpecifiedOnly = FALSE;
	strcpy(g_szHostName, "");


	// Read the cmd line args into a module params struct
	// You must call this before InitializeStressUtils()

	// Be sure to use the correct version, depending on your 
	// module entry point.

	if ( !ParseCmdLine_wmain (argc, argv, &mp) )
	{
		LogFail (_T("Failure parsing the command line!  exiting ..."));
		return CESTRESS_ABORT; // Use the abort return code
	}


	// You must call this before using any stress utils 
	InitializeStressUtils (
							_T("wsx_name"), // Module name to be used in logging
							LOGZONE(SLOG_SPACE_NET, SLOG_WINSOCK),       // Logging zones used by default
							&mp             // Module params passed on the cmd line
							);


	LogVerbose (_T("Starting at %d"), dwStartTime);


	// Module-specific parsing of the 'user' portion of the cmd line.
	InitUserCmdLineUtils(mp.tszUser, NULL);
	User_GetOptionAsDWORD(_T("t"), &dwThreads);
	if(User_WasOption(_T("o")))
		g_fSpecifiedOnly = TRUE;

	if(_tcsicmp(mp.tszServer, TEXT("localhost")))
		szTarget = mp.tszServer;

#if defined UNICODE
	wcstombs(szTargetASCII, szTarget, sizeof(szTargetASCII));
#else
	strncpy(szTargetASCII, szTarget, sizeof(szTargetASCII));
#endif


	///////////////////////////////////////////////////////
	// Main test loop
	//

	hThread = (HANDLE *)malloc(dwThreads * sizeof(HANDLE));
	if (NULL == hThread)
	{
	    LogWarn2(_T("Memory allocation failed.  Aborting."));
	    retValue = CESTRESS_ABORT;
	    goto cleanup;
	}

	if(!Setup(szTargetASCII))
	{
		retValue = CESTRESS_ABORT;
		goto cleanup;
	}

	InitializeCriticalSection (&g_csResults);


	// Spin one or more worker threads to do the actually testing.

	LogVerbose (_T("Creating %i worker threads"), dwThreads);

	g_hTimesUp = CreateEvent (NULL, TRUE, FALSE, NULL);

	for (i = 0; i < dwThreads; i++)
	{
		hThread[i] = CreateThread (NULL, 0, TestThreadProc, &results, 0, &dwThreadId);

		if (hThread[i])
		{
			LogVerbose (_T("Thread %i (0x%x) successfully created"), i, hThread[i]);
		}
		else
		{
			LogFail (_T("Failed to create thread %i! Error = %d"), i, GetLastError());
		}
	}

	// Main thread is now the timer
	Sleep (mp.dwDuration);

	// Signal the worker threads that they must finish up
	SetEvent (g_hTimesUp);


	///////////////////////////////////////////////////////
	// Clean-up, report results, and exit
	//
	
	
	// Wait for all of the workers to return.
	
	// NOTE: Don't terminate the threads yourself.  Go ahead and let them hang.
	//       The harness will decide whether to let your module hang or
	//       to terminate the process.

	LogComment (_T("Time's up signaled.  Waiting for threads to finish ..."));

	dwExited = 0;

	while (dwExited != dwThreads)
	{
		for (i = 0; i < dwThreads; i++)
		{
			if ( hThread[i] )
			{
				if (WAIT_OBJECT_0 == WaitForSingleObject (hThread[i], THREAD_WAIT_INTERVAL))
				{
					LogVerbose (_T("Thread %i (0x%x) finished"), i, hThread[i]);

					CloseHandle(hThread[i]);
					hThread[i] = NULL;
					dwExited++;
				}
			}
		}
	}

	DeleteCriticalSection (&g_csResults);


	Cleanup();

	// Finish
	dwEnd = GetTickCount();
	LogVerbose (_T("Leaving at %d, duration (ms) = %d, (sec) = %d, (min) = %d"),
					dwEnd,
					dwEnd - dwStartTime,
					(dwEnd - dwStartTime) / 1000,
					(dwEnd - dwStartTime) / 60000
					);

cleanup:

	if (g_hTimesUp)
		CloseHandle(g_hTimesUp);

	if (hThread)
		free(hThread);


	// DON'T FORGET:  You must report your results to the harness before exiting
	ReportResults (&results);

	return retValue;
}


//
// Takes in the addr/name to resolve and creates a global list of
// addresses and a global host name
//
BOOL Setup(LPSTR szTargetASCII)
{
	WSADATA WSAData;
	ADDRINFO Hints, *pAddrInfo;

	// Initialize the Winsock layer
	if(WSAStartup(WINSOCK_VERSION_REQ, &WSAData) != 0)
	{
		LogFail(TEXT("WSAStartup Failed"));
		return FALSE;
	}

	// First, determine if we were given a name or address
	memset(&Hints, 0, sizeof(Hints));
	Hints.ai_family = PF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;
	Hints.ai_flags = AI_NUMERICHOST;
	if(getaddrinfo(szTargetASCII, JUNK_PORT, &Hints, &pAddrInfo))
	{
		if(WSAGetLastError() == EAI_NONAME)
		{
			// the string is probably a host name
			strncpy(g_szHostName, szTargetASCII, sizeof(g_szHostName)); 
		}
		else
		{
			LogFail(TEXT("getaddrinfo(%hs) failed, error = %d"),
				szTargetASCII, WSAGetLastError());
			return FALSE;
		}
	}
	else
	{
		if(pAddrInfo == NULL)
		{
			LogFail(TEXT("getaddrinfo() succeeded but we didn't get an address!"));
			return FALSE;
		}

		// If address, then we get the name first

		// Note: We will accept an address if we can't get a true name.
		// We do this primarily because there is no current popular support
		// for IPv6 reverse name resolution.
		if(getnameinfo(pAddrInfo->ai_addr, pAddrInfo->ai_addrlen,
			g_szHostName, sizeof(g_szHostName),
			NULL, 0,
			0))
		{
			LogFail(TEXT("getnameinfo() failed, error = %d"), WSAGetLastError());
			freeaddrinfo(pAddrInfo);
			return FALSE;
		}

		freeaddrinfo(pAddrInfo);
	}

	// Now we get the list of addresses from the name
	memset(&Hints, 0, sizeof(Hints));
	Hints.ai_family = PF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(g_szHostName, JUNK_PORT, &Hints, &g_paiAddrs))
	{
		LogFail(TEXT("getaddrinfo(%hs) failed, error = %d"),
				g_szHostName, WSAGetLastError());
		return FALSE;
	}

	// Count addresses
	g_dwNumAddrs = 0;
	for(ADDRINFO *pAI = g_paiAddrs; pAI != NULL; pAI = pAI->ai_next)
	{
		// Quick check to make sure we don't have any blatently bad address structures
		if(pAI->ai_addrlen > sizeof(SOCKADDR_STORAGE))
		{
			DebugBreak();
		}
		g_dwNumAddrs++;
	}

	if(g_dwNumAddrs == 0)
	{
		LogFail(TEXT("getaddrinfo(%hs) succeeded but we didn't get an address!"),
			g_szHostName);
		return FALSE;
	}

	return TRUE;
}


BOOL Cleanup(void)
{
	if(g_paiAddrs)
		freeaddrinfo(g_paiAddrs);

	if(WSACleanup() == SOCKET_ERROR)
	{
		LogWarn2(TEXT("WSACleanup() failed, error = %d"), WSAGetLastError());
		return FALSE;
	}

	return TRUE;
}


BOOL GetAddrFromList(SOCKADDR_STORAGE *pssAddr, int *pcbAddrSize)
{
	ADDRINFO *pAI;
	DWORD i;

	if(pssAddr == NULL || pcbAddrSize == NULL ||
		g_dwNumAddrs == 0 || g_paiAddrs == NULL)
		return FALSE;

	// Pick a random address from the list
	i = GetRandomRange(0, g_dwNumAddrs - 1);
	for(pAI = g_paiAddrs; (pAI != NULL) && (i-- > 0); pAI = pAI->ai_next)
	{}

	if(pAI != NULL)
	{
		*pcbAddrSize = pAI->ai_addrlen;
		memcpy(pssAddr, pAI->ai_addr, pAI->ai_addrlen);
	}
	else
	{
		// Our global address list has become corrupted somehow
		DebugBreak();
		return FALSE;
	}

	return TRUE;
}

#define NUM_FAMILES			2			// Just AF_INET (IPv4) and AF_INET6 (IPv6)

BOOL MakeRandomAddr(SOCKADDR_STORAGE *pssAddr, int *pcbAddrSize)
{
	if(pssAddr == NULL || pcbAddrSize == NULL)
		return FALSE;

	// Decide between families (IPv4 and IPv6)
	switch(GetRandomRange(0,NUM_FAMILES))
	{
	case 1:
		((SOCKADDR_IN6 *)pssAddr)->sin6_family = PF_INET6;
		((DWORD *)&((SOCKADDR_IN6 *)pssAddr)->sin6_addr)[0] = GetRandomNumber(0xFFFFFFFF);
		((DWORD *)&((SOCKADDR_IN6 *)pssAddr)->sin6_addr)[1] = GetRandomNumber(0xFFFFFFFF);
		((DWORD *)&((SOCKADDR_IN6 *)pssAddr)->sin6_addr)[2] = GetRandomNumber(0xFFFFFFFF);
		((DWORD *)&((SOCKADDR_IN6 *)pssAddr)->sin6_addr)[3] = GetRandomNumber(0xFFFFFFFF);
		*pcbAddrSize = sizeof(SOCKADDR_IN6);
		break;
	default:
		((SOCKADDR_IN *)pssAddr)->sin_family = PF_INET;
		((SOCKADDR_IN *)pssAddr)->sin_addr.s_addr = GetRandomNumber(0xFFFFFFFF);
		*pcbAddrSize = sizeof(SOCKADDR_IN);
		break;
	}

	return TRUE;
}


BOOL GetLocalAddr(SOCKADDR_STORAGE *pssAddr, int *pcbAddrSize)
{
	char szLocalHostName[NI_MAXHOST];
	ADDRINFO Hints, *pAddrInfo, *pAI;
	DWORD i, dwNumAddrs;

	// Get the local host name
	if(gethostname(szLocalHostName, NI_MAXHOST) == SOCKET_ERROR)
	{
		LogWarn2(TEXT("gethostname() failed, error = %d"), WSAGetLastError());
		return FALSE;
	}

	memset(&Hints, 0, sizeof(Hints));

	// use the local host name to get a list of local addresses
	if(getaddrinfo(szLocalHostName, JUNK_PORT, &Hints, &pAddrInfo))
	{
		LogWarn2(TEXT("getaddrinfo(%hs) failed for local host name, error = %d"),
			szLocalHostName, WSAGetLastError());
		return FALSE;
	}

	// count the addresses
	for(dwNumAddrs = 0, pAI = pAddrInfo; pAI != NULL; pAI = pAI->ai_next)
		dwNumAddrs++;
	
	if(dwNumAddrs)
	{
		// pick one
		i = GetRandomRange(0, dwNumAddrs - 1);
		for(pAI = pAddrInfo; pAI != NULL && i--; pAI = pAI->ai_next)
		{}

		// copy it into the provider SOCKADDR_STORAGE structure
		*pcbAddrSize = pAI->ai_addrlen;
		memcpy(pssAddr, pAI->ai_addr, pAI->ai_addrlen);
	}

	if(dwNumAddrs == 0 || pAI == NULL)
	{
		// Something very wrong if we get here.  We got no local addresses or
		// we counted more addresses than are actually there
		DebugBreak();
		freeaddrinfo(pAddrInfo);
		return FALSE;
	}

	freeaddrinfo(pAddrInfo);
	return TRUE;
}


BOOL MakeRandomName(LPSTR pBuf, int cbBufSize)
{
	int i, cbRandSize;

	if(pBuf == NULL || cbBufSize <= 0)
		return FALSE;

	// Get a random string length (account for terminating NULL)
	cbRandSize = GetRandomRange(0, cbBufSize - 1);

	for(i = 0; i < cbRandSize; i++)
		pBuf[i] = (char)GetRandomRange((DWORD)'A', (DWORD)'Z');

	// Add terminating NULL character
	pBuf[i] = '\0';

	return TRUE;
}


UINT DoGetNameStressIteration (void)
{
	UINT uRet = CESTRESS_PASS;
	char szHostName[NI_MAXHOST], szServ[NI_MAXSERV];
	SOCKADDR_STORAGE ssAddr;
	int cbAddrSize, nChoice;
	BOOL fExpectError;
	//TCHAR szAddrString[128];

	// Initialize local variables
	szHostName[0] = '\0';
	memset(&ssAddr, 0, sizeof(SOCKADDR_STORAGE));
	cbAddrSize = 0;

	// Here we choose want we're going to do
	// If the user has specified that he only wants to use the addr/name he specifed,
	// we set nChoice to 0
	nChoice = (g_fSpecifiedOnly) ? 0 : GetRandomRange(0,2);		// Choose between three options

	switch(nChoice)
	{
	case 1:
		// Create a random 'valid' address
		//LogVerbose(TEXT("DoGetNameStressIteration: Random addr"));
		MakeRandomAddr(&ssAddr, &cbAddrSize);
		fExpectError = TRUE;
		break;
	case 2:
		// Use a 'local' address (loopback if no others available)
		//LogVerbose(TEXT("DoGetNameStressIteration: Local addr"));
		GetLocalAddr(&ssAddr, &cbAddrSize);
		fExpectError = FALSE;
		break;
	default:
		// Get a random address from the list of addresses we created in Setup()
		//LogVerbose(TEXT("DoGetNameStressIteration: Specified addr"));
		GetAddrFromList(&ssAddr, &cbAddrSize);
		fExpectError = FALSE;
		break;
	}

	// Reverse name resolution currently unsupported for v6.
	if(ssAddr.ss_family == PF_INET6)
		fExpectError = TRUE;			

	//AddrToString((SOCKADDR *)&ssAddr, szAddrString);
	//LogVerbose(TEXT("DoGetNameStressIteration: Choice = %d, using %s"), nChoice, szAddrString);

	if(cbAddrSize == 0)
	{
		// should never be here
		LogFail(TEXT("No address to resolve!"));
		return CESTRESS_ABORT;
	}

	if(getnameinfo((SOCKADDR *)&ssAddr, cbAddrSize,
		szHostName, sizeof(szHostName),
		szServ, sizeof(szServ),
		NI_NUMERICSERV))
	{
		// Don't log resolution failures when we expect them
		if(( fExpectError && (WSAGetLastError() == EAI_NONAME || WSAGetLastError() == EAI_NODATA) )
			|| (ssAddr.ss_family == PF_INET6) )
		{
			LogVerbose(TEXT("getnameinfo() returned error as expected"));
		}
		else
		{
			LogFail(TEXT("getnameinfo() failed for family %d, error = %d"), ssAddr.ss_family, WSAGetLastError());
			uRet = CESTRESS_FAIL;
		}
	}

	return uRet;
}

UINT DoGetAddrStressIteration (void)
{
	UINT uRet = CESTRESS_PASS;
	ADDRINFO Hints, *pAddrInfo;
	int nChoice;
	BOOL fExpectError;
	char szNodeName[NI_MAXHOST];

	// Initialize local variables
	pAddrInfo = NULL;
	memset(&Hints, 0, sizeof(Hints));

	// Here we choose want we're going to do
	// If the user has specified that he only wants to use the addr/name he specifed,
	// we set nChoice to 0
	nChoice = (g_fSpecifiedOnly) ? 0 : GetRandomRange(0,1);		// Choose between two options

	switch(nChoice)
	{
	case 1:
		// Create a random name
		MakeRandomName(szNodeName, MAX_RANDOM_NAME_LEN);
		fExpectError = TRUE;
		break;
	default:
		// Use the name we got from the addr/name provided
		strncpy(szNodeName, g_szHostName, MIN(sizeof(szNodeName), sizeof(g_szHostName)));
		fExpectError = FALSE;
		break;
	}

	//LogVerbose(TEXT("DoGetAddrStressIteration: Choice = %d, use name %hs"), nChoice, szNodeName);

	// Now we get the list of addresses from the name
	Hints.ai_family = PF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(szNodeName, JUNK_PORT, &Hints, &pAddrInfo))
	{
		// don't report failures if we are expecting an error and the name didn't resolve
		if(!fExpectError || (WSAGetLastError() != EAI_NONAME))
		{
			LogFail(TEXT("getaddrinfo(%hs) failed, error = %d"),
				g_szHostName, WSAGetLastError());
			uRet = CESTRESS_FAIL;
		}
	}
	else
		freeaddrinfo(pAddrInfo);

	return uRet;
}



///////////////////////////////////////////////
//
// Worker thread to do the acutal testing
//

DWORD WINAPI TestThreadProc (LPVOID pv)
{
	UINT ret;
	STRESS_RESULTS* pRes = (STRESS_RESULTS*) pv; // get a ptr to the module results struct
	STRESS_RESULTS res;

	memset(&res, 0, sizeof(res));

	// Do our testing until time's up is signaled
	while (WAIT_OBJECT_0 != WaitForSingleObject (g_hTimesUp, 0))
	{
		switch(GetRandomRange(0,1))
		{
		case 0:
			ret = DoGetNameStressIteration();
			break;
		default:
			ret = DoGetAddrStressIteration();
			break;
		}

		if (ret == CESTRESS_ABORT)
			return CESTRESS_ABORT;

		// Keep a local tally of this thread's results
		RecordIterationResults(&res, ret);

		Sleep(PAUSE_MS);
	}

	LogVerbose(TEXT("Completed %d iterations (Fail: %d, Warn1: %d, Warn2: %d)"),
		res.nIterations, 
		res.nFail,
		res.nWarn1,
		res.nWarn2
		);

	// Add this thread's results to the module tally
	EnterCriticalSection(&g_csResults);
	AddResults(pRes, &res);
	LeaveCriticalSection(&g_csResults);

	return 0;
}

void AddrToString(SOCKADDR *psaAddr, TCHAR szAddrString[])
{
	if(psaAddr == NULL)
		return;

	switch(psaAddr->sa_family)
	{
	case AF_INET:
		// IPv4 Address
		_stprintf(szAddrString, TEXT("AF_INET4 Addr[%hs]"), inet_ntoa(((SOCKADDR_IN *)psaAddr)->sin_addr));
		break;
	case AF_INET6:
		// IPv6 Address
		_stprintf(szAddrString, TEXT("AF_INET6 Addr[%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x]"),
			htons((*(WORD *)&(((SOCKADDR_IN6 *)psaAddr)->sin6_addr.s6_addr[0]))),
			htons((*(WORD *)&(((SOCKADDR_IN6 *)psaAddr)->sin6_addr.s6_addr[2]))),
			htons((*(WORD *)&(((SOCKADDR_IN6 *)psaAddr)->sin6_addr.s6_addr[4]))),
			htons((*(WORD *)&(((SOCKADDR_IN6 *)psaAddr)->sin6_addr.s6_addr[6]))),
			htons((*(WORD *)&(((SOCKADDR_IN6 *)psaAddr)->sin6_addr.s6_addr[8]))),
			htons((*(WORD *)&(((SOCKADDR_IN6 *)psaAddr)->sin6_addr.s6_addr[10]))),
			htons((*(WORD *)&(((SOCKADDR_IN6 *)psaAddr)->sin6_addr.s6_addr[12]))),
			htons((*(WORD *)&(((SOCKADDR_IN6 *)psaAddr)->sin6_addr.s6_addr[14]))));
		break;
	default:
		_stprintf(szAddrString, TEXT("AF_UNKNOWN"));
		break;
	}
}

