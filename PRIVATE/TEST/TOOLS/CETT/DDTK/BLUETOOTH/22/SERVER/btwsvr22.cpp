//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include <winsock2.h>
#include <stdio.h>
#include <malloc.h>
#include <windows.h>
#include "netmain.h"
#include "tux.h"
#include "util.h"

// Required for "NetMain" 

#define _FILENAM  "btwsvr22"

extern TCHAR *gszMainProgName = TEXT(_FILENAM);	  
extern DWORD gdwMainInitFlags = 0;

// Global event to be used by threaded tests if necessary.
HANDLE g_hEvent = NULL;

DWORD g_dwTotalTests = 0;
DWORD g_dwPassTests  = 0;
DWORD g_dwFailTests  = 0;
DWORD g_dwSkipTests  = 0;
DWORD g_dwAbortTests = 0;

void Usage()
{
	OutStr(TEXT("Bluetooth winsock test server\n"));
	OutStr(TEXT("\n"));
	OutStr(TEXT("Usage : btwsvr22\n"));
	OutStr(TEXT("\n"));
	OutStr(TEXT("Examples : btwsvr22\n"));
	OutStr(TEXT("\n"));
}

DWORD WINAPI RunTestCase(LPVOID lpParameter)
{
	DWORD dwUniqueID;
	int i;
	int retcode;

	dwUniqueID = *((DWORD *)lpParameter);

	// Find test case.
	for (i = 0; g_lpFTE[i].lpDescription != NULL; i ++)
	{
		if (g_lpFTE[i].dwUniqueID == dwUniqueID)
			break;
	}

	if (g_lpFTE[i].lpDescription == NULL)
	{
		// Test case not found.
		OutStr(TEXT("Can not find test case %d\n"), dwUniqueID);
	}
	else
	{
		g_dwTotalTests ++;

		// Tests need to be run sequencially.
		WaitForSingleObject(g_hEvent, INFINITE);
		
		if (g_lpFTE[i].lpTestProc != NULL)
		{
			// Call test function.
			OutStr(TEXT("*** vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"));
			OutStr(TEXT("*** TEST STARTING\n"));
			OutStr(TEXT("***\n"));
			OutStr(TEXT("*** Test Name:      %s\n"), g_lpFTE[i].lpDescription);
			OutStr(TEXT("*** Test ID:        %u\n"), g_lpFTE[i].dwUniqueID);
			OutStr(TEXT("*** vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"));

			retcode = g_lpFTE[i].lpTestProc(TPM_EXECUTE, NULL, &g_lpFTE[i]);
			switch(retcode)
			{
			case TPR_SKIP: 
				OutStr(TEXT("Test case SKIPPED\n"));
				g_dwSkipTests ++;
				break;
			case TPR_PASS:
				OutStr(TEXT("Test case PASSED\n"));
				g_dwPassTests ++;
				break;
			case TPR_FAIL:
				OutStr(TEXT("Test case FAILED\n"));
				g_dwFailTests ++;
				break;
			case TPR_ABORT:
				OutStr(TEXT("Test case ABORTED\n"));
				g_dwAbortTests ++;
				break;
			default:
				OutStr(TEXT("Unknown result %d"), retcode);
				break;
			}

			// Call test function.
			OutStr(TEXT("*** ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n"));
			OutStr(TEXT("*** TEST COMPLETED\n"));
			OutStr(TEXT("***\n"));
			OutStr(TEXT("*** Test Name:      %s\n"), g_lpFTE[i].lpDescription);
			OutStr(TEXT("*** Test ID:        %u\n"), g_lpFTE[i].dwUniqueID);

			switch(retcode)
			{
			case TPR_SKIP: 
				OutStr(TEXT("*** Result:         %s\n"), TEXT("Skipped"));
				break;
			case TPR_PASS:
				OutStr(TEXT("*** Result:         %s\n"), TEXT("Passed"));
				break;
			case TPR_FAIL:
				OutStr(TEXT("*** Result:         %s\n"), TEXT("Failed"));
				break;
			case TPR_ABORT:
				OutStr(TEXT("*** Result:         %s\n"), TEXT("Aborted"));
				break;
			default:
				OutStr(TEXT("Unknown result %d"), retcode);
				break;
			}

			OutStr(TEXT("*** ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n"));
		}

		// Tests need to run sequentially.
		SetEvent(g_hEvent);
	}

	return TPR_HANDLED;
}

extern int mymain(int argc, TCHAR* argv[])
{
	WSADATA wsd;
	SOCKET listenSock, dataSock, bakSock = INVALID_SOCKET;
	SOCKADDR_IN local, remote;
	int retcode, addrlen;
	CONTROL_PACKET packet;
	HANDLE hThread = NULL; // assuming test cases are executed sequentially
	DWORD dwThreadID, dwExitCode;
	DWORD dwUniqueID;
	BOOL bTestDone = FALSE;

	if (argc != 1)
	{
		Usage();

		return 0;
	}

	// Set flags
	g_bIsServer = TRUE;
	g_bNoServer = FALSE;

    // Initialize our global critical section.
	g_hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hEvent == NULL)
	{
		retcode = GetLastError();
		OutStr(TEXT("Failed to create syncronization event with error %d\n"), retcode);

		return retcode;
	}

	// Initialize winsock2.
	if (WSAStartup(MAKEWORD(2, 2), &wsd)  != 0) 
	{
		OutStr(TEXT("WSAStartup call failed with error %d\n"), retcode = WSAGetLastError());
		WSACleanup();

	    return retcode;
	}

	 // Get local Bluetooth device address.
	if ((retcode = DiscoverLocalRadio(&g_LocalRadioAddr)) != SUCCESS)
	{
		 OutStr(TEXT("DiscoverLocalRadio failed with error %d\n"), retcode);

		 return -1;
	}

	// Listen on server port.
	listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (listenSock == INVALID_SOCKET)
	{
		OutStr(TEXT("socket call failed with error %d\n"), retcode = WSAGetLastError());

		return retcode;
	}

	memset((void *)&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_port = htons(DEFAULT_PORT);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	retcode = bind(listenSock, (SOCKADDR *)&local, sizeof(local));
	if (retcode == SOCKET_ERROR)
	{
		OutStr(TEXT("bind call failed with error %d\n"), retcode = WSAGetLastError());

		return retcode;
	}

	retcode = listen(listenSock, 5);
	if (retcode == SOCKET_ERROR)
	{
		OutStr(TEXT("listen call failed with error %d\n"), retcode = WSAGetLastError());

		return retcode;
	}

	OutStr(TEXT("Listening on port %d\n"), ntohs(local.sin_port));

	// Receive and handle commands.
	while(!bTestDone)
	{
        memset(&remote, 0, sizeof(remote));
        remote.sin_family = AF_INET;
		addrlen = sizeof(remote);

		dataSock = accept(listenSock, (SOCKADDR *)&remote, &addrlen);
		//OutStr(TEXT("ACCEPTED %d"), dataSock);
		if (dataSock == INVALID_SOCKET )
		{
			OutStr(TEXT("accept call failed with error %d\n"), retcode = WSAGetLastError());

			return retcode;
		}

		if (bakSock != INVALID_SOCKET)
		{
			shutdown(bakSock, 0);
			closesocket(bakSock);
			//OutStr(TEXT("CLOSED %d"), bakSock);
			bakSock = INVALID_SOCKET;
		}

		retcode = ReceiveControlPacket(dataSock, &packet);
		//OutStr(TEXT("RECEIVED %d"), dataSock);
		if (retcode != SUCCESS)
		{
			OutStr(TEXT("ReceiveControlPacket failed with error %d\n"), retcode);
		
			return -1;
		}
		
		switch(packet.dwCommand)
		{
		case EXCHANGE_BT_ADDR:
			//OutStr(TEXT("EXCHANGE_BT_ADDR"));
			memcpy((void *)&g_RemoteRadioAddr, (void *)packet.Data, sizeof(g_RemoteRadioAddr));
			memcpy((void *)packet.Data, (void *)&g_LocalRadioAddr, sizeof(g_LocalRadioAddr));
			break;
		case BEGIN_TEST:
			//OutStr(TEXT("BEGIN_TEST"));
			memcpy((void *)&dwUniqueID, (void *)&packet.Data, sizeof(dwUniqueID));
			hThread = CreateThread(NULL, 0, RunTestCase, (LPVOID)(&dwUniqueID), 0 , &dwThreadID);
			if (hThread == NULL)
			{
				OutStr(TEXT("CreateThread failed with error %d\n"), retcode = GetLastError());
		
				return retcode;
			}

			break;
		case END_TEST:
			//OutStr(TEXT("END_TEST"));
			if (hThread != NULL)
			{
				// Give it some time to finish.
				if (WaitForSingleObject(hThread, 10000) == WAIT_FAILED)
				{
					OutStr(TEXT("WaitForSingleObject failed with error %d\n"), retcode = GetLastError());
		
					return retcode;
				}

				
				retcode = GetExitCodeThread(hThread, &dwExitCode);
				if (retcode == 0)
				{
					OutStr(TEXT("GetExitCodeThread failed with error %d\n"), retcode = GetLastError());
		
					return retcode;
				}
				
				if (dwExitCode == STILL_ACTIVE)
				{
					retcode = TerminateThread(hThread, 0);
					OutStr(TEXT("Timeout, abort test case!"));
					if (retcode == 0)
					{
						OutStr(TEXT("TerminateThread failed error = %u"), GetLastError());
					}
					SetEvent(g_hEvent);
					g_dwAbortTests ++;
				}
					
				CloseHandle(hThread);
				hThread = NULL;
			}

			break;
		case TEST_DONE:
			//OutStr(TEXT("TEST_DONE"));
			if (hThread != NULL)
			{
				// Give it some time to finish.
				if (WaitForSingleObject(hThread, 10000) == WAIT_FAILED)
				{
					OutStr(TEXT("WaitForSingleObject failed with error %d\n"), retcode = GetLastError());
			
					return retcode;
				}

				
				retcode = GetExitCodeThread(hThread, &dwExitCode);
				if (retcode == 0)
				{
					OutStr(TEXT("GetExitCodeThread failed with error %d\n"), retcode = GetLastError());
		
					return retcode;
				}
				
				if (dwExitCode == STILL_ACTIVE)
				{
					retcode = TerminateThread(hThread, 0);
					OutStr(TEXT("Timeout, abort test case!"));
					if (retcode == 0)
					{
						OutStr(TEXT("TerminateThread failed error = %u"), GetLastError());
					}
					SetEvent(g_hEvent);
					g_dwAbortTests ++;
				}
					
				CloseHandle(hThread);
				hThread = NULL;
			}
			//bTestDone = TRUE;   // don't exit

			// Print result.
			OutStr(TEXT("*** ==================================================================\n"));
			OutStr(TEXT("*** SUITE SUMMARY\n"));
			OutStr(TEXT("***\n"));
			OutStr(TEXT("*** Passed: %10u\n"), g_dwPassTests);
			OutStr(TEXT("*** Failed: %10u\n"), g_dwFailTests);
			OutStr(TEXT("*** Skipped:%10u\n"), g_dwSkipTests);
			OutStr(TEXT("*** Aborted:%10u\n"), g_dwAbortTests);
			OutStr(TEXT("*** -------- ---------\n"));
			OutStr(TEXT("*** Total:  %10u\n"), g_dwTotalTests);
			OutStr(TEXT("***\n"));
			OutStr(TEXT("*** ==================================================================\n"));

			// Reset global variable.
			g_dwPassTests  = 0;
			g_dwFailTests  = 0;
			g_dwSkipTests  = 0;
			g_dwAbortTests = 0;
			g_dwTotalTests = 0;

			OutStr(TEXT("\n"));
			OutStr(TEXT("*** ==================================================================\n"));
			OutStr(TEXT("***\n"));
			OutStr(TEXT("Test suite done, waiting for client to start new tests\n"));
			OutStr(TEXT("***\n"));
			OutStr(TEXT("*** ==================================================================\n"));
			OutStr(TEXT("\n"));

			break;
		default:
			OutStr(TEXT("Unknown command %d"), packet.dwCommand); 
			break;
		}

		retcode = SendControlPacket(dataSock, &packet);
		//OutStr(TEXT("SENT %d"), dataSock);
		if (retcode != SUCCESS)
		{
			OutStr(TEXT("SendControlPacket failed with error %d\n"), retcode);
	
			return FAIL;
		}	
		
		// Close sockets.
		//shutdown(dataSock, 0);
		//closesocket(dataSock);
		bakSock = dataSock;
	}
	
	// Close sockets.
	if (bakSock != INVALID_SOCKET)
	{
		shutdown(bakSock, 0);
		closesocket(bakSock);
	}
	shutdown(listenSock, 0);
	closesocket(listenSock);

	// Clean up winsock2.
	WSACleanup();

    // This is a good place to destroy our global critical section.
	CloseHandle(g_hEvent);

	// Print result.
	OutStr(TEXT("*** ==================================================================\n"));
	OutStr(TEXT("*** SUITE SUMMARY\n"));
	OutStr(TEXT("***\n"));
	OutStr(TEXT("*** Passed: %10u\n"), g_dwPassTests);
	OutStr(TEXT("*** Failed: %10u\n"), g_dwFailTests);
	OutStr(TEXT("*** Skipped:%10u\n"), g_dwSkipTests);
	OutStr(TEXT("*** Aborted:%10u\n"), g_dwAbortTests);
	OutStr(TEXT("*** -------- ---------\n"));
	OutStr(TEXT("*** Total:  %10u\n"), g_dwTotalTests);
	OutStr(TEXT("***\n"));
	OutStr(TEXT("*** ==================================================================\n"));

	return 0;
}