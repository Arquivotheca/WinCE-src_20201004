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
#include "echothread.h"
#include "wsx_conn.h"


// Global Variables
HANDLE				g_hTimesUp;
HANDLE				g_hEchoThreadRdy;
CRITICAL_SECTION	g_csResults;
ADDRINFO			*g_paiAddrs;
DWORD				g_dwNumAddrs;
DWORD				g_dwWaitTime;

// Function Prototypes
BOOL Setup(LPSTR szServerASCII, LPSTR szPortASCII);
UINT DoStressIteration (void);
DWORD WINAPI TestThreadProc (LPVOID pv);
BOOL Cleanup(void);
int GetRandomSockType(void);

//
//***************************************************************************************


int wmain( int argc, WCHAR** argv )
{
	int				retValue = 0;  // Module return value.  You should return 0 unless you had to abort.
	MODULE_PARAMS	mp;            // Cmd line args arranged here.  Used to init the stress utils.
	STRESS_RESULTS	results;       // Cumulative test results for this module.
	
	DWORD			dwStartTime = GetTickCount(), dwEndTime;
	HANDLE			*hThread, hServerThread;
	DWORD			dwThreads, dwThreadId, i;
	BOOL			fRunServerThread = FALSE;
	LPTSTR			szServer, szPort;
	char			szServerASCII[256], szPortASCII[6];


	///////////////////////////////////////////////////////
	// Initialization
	//

	memset(&mp, 0, sizeof(mp));
	memset(&results, 0, sizeof(results));
	g_dwNumAddrs = 0;
	g_paiAddrs = NULL;
	g_hTimesUp = g_hEchoThreadRdy = NULL;
	g_dwWaitTime = DEFAULT_WAITTIME;

	dwThreads = DEFAULT_THREADS;
	szPort = DEFAULT_PORT;
	szServer = DEFAULT_SERVER;


	// Read the cmd line args into a module params struct
	// You must call this before InitializeStressUtils()

	// Be sure to use the correct version, depending on your 
	// module entry point.

	if ( !ParseCmdLine_wmain (argc, argv, &mp) )
	{
		LogFail (TEXT("Failure parsing the command line!  exiting ..."));

		return CESTRESS_ABORT; // Use the abort return code
	}


	// You must call this before using any stress utils 

	InitializeStressUtils (
							_T("wsx_conn"),	// Module name to be used in logging
							LOGZONE(SLOG_SPACE_NET, SLOG_WINSOCK),		// Logging zones used by default
							&mp				// Module params passed on the cmd line
							);


	LogVerbose (TEXT("Starting at %d"), dwStartTime);


	// Module-specific parsing of the 'user' portion of the cmd line.
	//

	InitUserCmdLineUtils(mp.tszUser, NULL);
	User_GetOptionAsDWORD(_T("w"), &g_dwWaitTime);
	User_GetOptionAsDWORD(_T("t"), &dwThreads);
	User_GetOption(_T("p"), &szPort);

	if(!szPort)
	{
		// Bad command line. "-p" was the last option specified, no argument.
		DebugBreak();
		return CESTRESS_ABORT;
	}

	if(_tcsicmp(mp.tszServer, TEXT("localhost")))
		szServer = mp.tszServer;
	else
		fRunServerThread = TRUE;

	// Now that we've got the server name/addr and port in TCHAR form,
	// convert them to ASCII for the Winsock functions
#if defined UNICODE
	wcstombs(szServerASCII, szServer, sizeof(szServerASCII));
	wcstombs(szPortASCII, szPort, sizeof(szPortASCII));
#else
	strncpy(szServerASCII, szServer, sizeof(szServerASCII));
	strncpy(szPortASCII, szPort, sizeof(szPortASCII));
#endif
	
	// It is important that this come after any code to get a port from the command line,
	// since this code will pick a random port if we're testing against the localhost
	if(fRunServerThread)
	{
		int nNewPort = GetRandomRange(5024, 65535);
		_itoa(nNewPort, szPortASCII, 10);
		LogVerbose(TEXT("Using port %hs instead of the default port"), szPortASCII);
	}
		



	if(!Setup(szServerASCII, szPortASCII))
		return CESTRESS_ABORT;

	///////////////////////////////////////////////////////
	// Main test loop
	//

	hThread = (HANDLE *)malloc(dwThreads * sizeof(HANDLE));
	if (NULL == hThread)
	{
	    LogWarn2(TEXT("Memory allocation failed.  Aborting."));
	    retValue = CESTRESS_ABORT;
	    goto cleanup;
	}

	InitializeCriticalSection (&g_csResults);

	// Create a manual-reset event that will signal the worker threads
	// when it's time to shutdown
	g_hTimesUp = CreateEvent (NULL, TRUE, FALSE, NULL);

	// Create a auto-reset event that will signal when the echo thread
	// is ready
	g_hEchoThreadRdy = CreateEvent (NULL, FALSE, FALSE, NULL);

	// Spin a server thread, if necessary
	if(fRunServerThread)
	{
		// Setup server parameters
		EchoThreadParms ServerParms;
		memset(&ServerParms, 0, sizeof(ServerParms));
		ServerParms.nFamily = DEFAULT_FAMILY;
		ServerParms.nSockType = ALL_SOCK_TYPES;
		ServerParms.nMaxDataXferSize = DATA_XFER_SIZE;
		ServerParms.fBlackholeMode = FALSE;
		ServerParms.szPortASCII = szPortASCII;

		hServerThread = CreateThread (NULL, 0, ServerThreadProc, &ServerParms, 0, &dwThreadId);
		if(hServerThread == NULL)
		{
			LogFail(TEXT("Failed to create server thread! Error = %d"), GetLastError());
			retValue = CESTRESS_ABORT;
			goto cleanup;
		}

		if(WaitForSingleObject(g_hEchoThreadRdy, 10000) != WAIT_OBJECT_0)
		{
			LogFail(TEXT("EchoThread is hung! Aborting..."));
			retValue = CESTRESS_ABORT;
			goto cleanup;
		}
	}

	// Spin one or more worker threads to do the actual testing.
	LogVerbose (TEXT("Creating %i worker threads"), dwThreads);
	for (i = 0; i < dwThreads; i++)
	{
		hThread[i] = CreateThread (NULL, 0, TestThreadProc, &results, 0, &dwThreadId);
		if (hThread[i])
			LogVerbose(TEXT("Thread %i (0x%x) successfully created"), i, hThread[i]);
		else
			LogFail(TEXT("Failed to create thread %i! Error = %d"), i, GetLastError());
	}

	// Main thread is now the timer
	Sleep(mp.dwDuration);

	// Signal the worker threads that they must finish up
	SetEvent(g_hTimesUp);


	///////////////////////////////////////////////////////
	// Clean-up, report results, and exit
	//
	
cleanup:
	
	// Wait for all of the workers to return.
	
	// NOTE: Don't terminate the threads yourself.  Go ahead and let them hang.
	//       The harness will decide whether to let your module hang or
	//       to terminate the process.

	LogComment (TEXT("Time's up signaled.  Waiting for threads to finish ..."));

	for(i = 0; i < dwThreads; i++)
	{
		if(hThread[i])
		{
			WaitForSingleObject(hThread[i], INFINITE);
			LogVerbose (L"Thread %i (0x%x) finished", i, hThread[i]);
			CloseHandle(hThread[i]);
		}
	}

	if(hServerThread)
	{
		WaitForSingleObject(hServerThread, INFINITE);
		LogVerbose (L"Server Thread (0x%x) finished", hServerThread);
		CloseHandle(hServerThread);
	}
	
	DeleteCriticalSection (&g_csResults);

	// DON'T FORGET:  You must report your results to the harness before exiting
	ReportResults (&results);
	
	dwEndTime = GetTickCount();
	LogVerbose(TEXT("Leaving at %d, duration (min) = %d"),
					dwEndTime,
					(dwEndTime - dwStartTime) / 60000
					);

	Cleanup();

	if (NULL != g_hTimesUp)
	{
		CloseHandle(g_hTimesUp);
		g_hTimesUp = NULL;
	}

	if (NULL != g_hEchoThreadRdy)
	{
		CloseHandle(g_hEchoThreadRdy);
		g_hEchoThreadRdy = NULL;
	}

	if(NULL != hThread)
		free(hThread);

	return retValue;
}

BOOL Setup (LPSTR szServerASCII, LPSTR szPortASCII)
{
	WSADATA WSAData;
	ADDRINFO Hints;

	// Initialize local variables
	memset(&Hints, 0, sizeof(Hints));

	// Initialize the Winsock layer
	if(WSAStartup(WINSOCK_VERSION_REQ, &WSAData) != 0)
	{
		LogFail(TEXT("WSAStartup Failed"));
		return FALSE;
	}

	// Get a list of addresses to communicate with
	Hints.ai_family = DEFAULT_FAMILY;
	if(getaddrinfo(szServerASCII, szPortASCII, &Hints, &g_paiAddrs))
		LogWarn2(TEXT("getaddrinfo() failed, error = %d"), WSAGetLastError());

	// Count addresses
	for(ADDRINFO *pAI = g_paiAddrs; pAI != NULL; pAI = pAI->ai_next)
		g_dwNumAddrs++;

	if(g_dwNumAddrs == 0)
	{
		LogFail(TEXT("FATAL ERROR - Couldn't get any addresses to connect to - FATAL ERROR"));
		return FALSE;
	}

	return TRUE;
}

UINT DoStressIteration (void)
{
	UINT uRet = CESTRESS_PASS, i;
	ADDRINFO *pAI;
	SOCKADDR_STORAGE ssAddr;
	char pBuf[DATA_XFER_SIZE];
	int cbTotalBytesRecvd, cbBytesRecvd, cbBytesSent, cbAddrSize, nSelRet;
	int nSockType;
	SOCKET sock = INVALID_SOCKET;
	fd_set fdSet;
	TIMEVAL tvTimeOut = {TIMEOUT_MSECS / 1000, ((TIMEOUT_MSECS % 1000) * 1000)};
	WORD wPort;

	// Pick a random address from the list
	i = GetRandomRange(0, g_dwNumAddrs - 1);
	for(pAI = g_paiAddrs; (pAI != NULL) && (i-- > 0); pAI = pAI->ai_next)
	{}

	if(pAI == NULL)
	{
		// should never be here unless we have less addresses than we thought
		LogFail(TEXT("Couldn't find address!"));
		return CESTRESS_ABORT;
	}

	// Choose a random socket type
	nSockType = GetRandomSockType();

	// Create an appropriate socket
	sock = socket(pAI->ai_family, nSockType, pAI->ai_protocol);
	if(sock == INVALID_SOCKET)
	{
		LogWarn1(TEXT("socket() failed, error = %d"), WSAGetLastError());
		uRet = CESTRESS_WARN2;
		goto exit;
	}

	// Connect the socket if necessary
	if(nSockType == SOCK_STREAM)
	{
		if(connect(sock, pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
		{
			if(WSAGetLastError() == WSAEADDRINUSE)
			{
				// We've eaten up all the port == 0 sockets
				// Let's slow down and start binding ports out in a greater
				// port space (10,000 - 60,000).  That way we won't be idle
				// while waiting for the 4min winsock timer to expire on the
				// last connections
				Sleep(g_dwWaitTime);

				wPort = (WORD) GetRandomRange(10000, 60000);
				memset(&ssAddr, 0, sizeof(ssAddr));
				switch(pAI->ai_family)
				{
				case PF_INET:
					ssAddr.ss_family = PF_INET;
					((SOCKADDR_IN *)&ssAddr)->sin_port = htons(wPort);
					cbAddrSize = sizeof(SOCKADDR_IN);
					break;
				case PF_INET6:
					ssAddr.ss_family = PF_INET6;
					((SOCKADDR_IN6 *)&ssAddr)->sin6_port = htons(wPort);
					cbAddrSize = sizeof(SOCKADDR_IN6);
					break;
				default:
					LogFail(TEXT("Don't know how to make SOCKADDR structure for family %d"),
						pAI->ai_family);
					uRet = CESTRESS_FAIL;
					goto exit;
				}

				// Need to get a new socket since our old one got implicitly
				// bound by the connect() call
				closesocket(sock);
				sock = socket(pAI->ai_family, nSockType, pAI->ai_protocol);

				if(bind(sock, (SOCKADDR *)&ssAddr, cbAddrSize) == SOCKET_ERROR)
				{
					LogWarn2(TEXT("bind() failed, family = %s, type = %s, port = %d, error = %d"),
						SZ_AI_FAMILY,
						SZ_AI_TYPE,
						wPort,
						WSAGetLastError());
					uRet = CESTRESS_WARN2;
					goto exit;
				}

				if(connect(sock, pAI->ai_addr, pAI->ai_addrlen) == SOCKET_ERROR)
				{
					LogWarn2(TEXT("connect() failed, family = %s, type = %s, local port = %d, error = %d"),
						SZ_AI_FAMILY,
						SZ_AI_TYPE,
						wPort,
						WSAGetLastError());
					uRet = CESTRESS_WARN2;
					goto exit;
				}
			}
			else
			{
				LogWarn2(TEXT("connect() failed, family = %s, type = %s, error = %d"),
						SZ_AI_FAMILY,
						SZ_AI_TYPE,
						WSAGetLastError());
					uRet = CESTRESS_WARN2;
					goto exit;
			}
		}
	}

	//
	// send some junk data
	//
	if ((cbBytesSent = sendto(sock, (char *) pBuf, DATA_XFER_SIZE, 0, pAI->ai_addr, pAI->ai_addrlen)) == SOCKET_ERROR)
	{
		LogFail(TEXT("sendto() failed, family = %s, type = %s(%d), error = %d"), 
			SZ_AI_FAMILY,
			SZ_AI_TYPE, nSockType,
			WSAGetLastError());
		uRet = CESTRESS_FAIL;
		goto exit;
	}

	//
	// receive the echo'd data back
	//
	for(cbTotalBytesRecvd = 0; (cbTotalBytesRecvd < cbBytesSent) && (cbBytesRecvd > 0); 
		cbTotalBytesRecvd += cbBytesRecvd)
	{
		FD_ZERO(&fdSet);
		FD_SET(sock, &fdSet);
		nSelRet = select(0, &fdSet, NULL, NULL, &tvTimeOut);

		if(nSelRet == SOCKET_ERROR)
		{
			LogWarn1(TEXT("select(read) failed, error = %d"), WSAGetLastError()); 
			uRet = CESTRESS_WARN1;
			goto exit;
		}
		else if(nSelRet == 0)
		{
			LogWarn1(TEXT("select(read, %s) timed out"), SZ_AI_TYPE);
			uRet = CESTRESS_WARN2;
			goto exit;
		}

		cbAddrSize = sizeof(ssAddr);
		cbBytesRecvd = recvfrom(sock, (char *) pBuf, DATA_XFER_SIZE, 
			0, (SOCKADDR *)&ssAddr, &cbAddrSize);
		if (cbBytesRecvd == SOCKET_ERROR)
		{
			LogFail(TEXT("recvfrom() failed, error = %d"), WSAGetLastError());
			uRet = CESTRESS_FAIL;
			goto exit;
		}
	}

	//LogVerbose(TEXT("Successfully communicated using family %s and type %s"),
	//	SZ_AI_FAMILY, SZ_AI_TYPE);

exit:

	if(sock != INVALID_SOCKET)
	{
		shutdown(sock, SD_BOTH);
		closesocket(sock);
	}
	return uRet;
}



///////////////////////////////////////////////
//
// Worker thread to host the acutal testing
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
		ret = DoStressIteration();

		if (ret == CESTRESS_ABORT)
			return CESTRESS_ABORT;

		// Keep a local tally of this thread's results
		RecordIterationResults(&res, ret);
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

