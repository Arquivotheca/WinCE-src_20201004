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

#define MAX_HOSTS			5
#define MAX_ADDR_SIZE		32
#define MAX_NAME_SIZE		100

#define DEFAULT_MSGTIME		10
#define DEFAULT_WAITTIME	0
#define DEFAULT_REPS		0

void PrintUsage()
{
	QAMessage(TEXT("ghstres2 [-f Frequency] [-m Milliseconds] [-r Reps] [-a IPAddress] [-n HostName]"));
	QAMessage(TEXT("    f:  Number of interations to perform before displaying status (default: %d)"), DEFAULT_MSGTIME);
	QAMessage(TEXT("    m:  Number of milliseconds to wait between iterations (default: %d)"), DEFAULT_WAITTIME);
	QAMessage(TEXT("    r:  Number of reps to execute the test (default: %d)"), DEFAULT_REPS);
	QAMessage(TEXT("    a:  IP address of a host to resolve into a name"));
	QAMessage(TEXT("    n:  Name of a host to resolve into an address"));
	QAMessage(TEXT("*** - NOTE - Multiple addresses and names can be given but they must be separated"));
	QAMessage(TEXT("             with semi-colons.  Up to %d names/addresses."), MAX_HOSTS);
}

DWORD g_dwMessageFrequency;
DWORD g_dwWaitTime;
DWORD g_dwReps;
LPTSTR g_szAddresses, g_szNames;

DWORD AddressThread(LPVOID *pParm);
DWORD NameThread(LPVOID *pParm);

extern "C" LPTSTR gszMainProgName  = _T("ghstres2");
extern "C" DWORD  gdwMainInitFlags = INIT_NET_CONNECTION;

extern "C" mymain (int argc, TCHAR *argv[])
{
    WSADATA		WSAData;
    HANDLE		hAddrThread, hNameThread;
    DWORD		ThreadId;

	if(QAWasOption(_T("?")) || QAWasOption(_T("help")))
	{
		PrintUsage();
		return 0;
	}

	// Initialize globals
	g_dwMessageFrequency = DEFAULT_MSGTIME;
	g_dwWaitTime = DEFAULT_WAITTIME;
	g_dwReps = DEFAULT_REPS;
	g_szAddresses = g_szNames = NULL;

	QAGetOptionAsDWORD(_T("m"), &g_dwWaitTime);
	QAGetOptionAsDWORD(_T("r"), &g_dwReps);
	QAGetOption(_T("a"), &g_szAddresses);
	QAGetOption(_T("n"), &g_szNames);

	if(QAGetOptionAsDWORD(_T("f"), &g_dwMessageFrequency) && g_dwMessageFrequency == 0)
		g_dwMessageFrequency = 1;	// MessageFrequency can not be 0

	// Put the multiple names/addresses into they're appropriate globals


    if(WSAStartup(WINSOCK_VERSION_REQ, &WSAData) != 0)
	{
		QAError(TEXT("WSAStartup Failed"));
		return 1;
	}
    else
    {
		QAMessage(TEXT("WSAStartup SUCCESS"));
    }

	if(g_szAddresses)
	{
		if ((hAddrThread = 
			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) AddressThread,
			NULL, 0, &ThreadId)) == NULL)
		{
			QAError(TEXT("BSTRESS: CreateThread(AddressToNameThread) failed %d"),
				GetLastError());
			
			goto exit;
		}
	}

	if(g_szNames)
	{
		if ((hNameThread = 
			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) NameThread,
			NULL, 0, &ThreadId)) == NULL)
		{
			QAError(TEXT("BSTRESS: CreateThread(NameToAddressThread) failed %d"),
				GetLastError());
			
			goto exit;
		}
	}

	QAMessage(TEXT("Main thread waiting for completion..."));

exit:

	if(hAddrThread)
	{
		WaitForSingleObject(hAddrThread, INFINITE);
		CloseHandle(hAddrThread);
	}
	
	if(hNameThread)
	{
		WaitForSingleObject(hNameThread, INFINITE);
		CloseHandle(hNameThread);
	}

	WSACleanup();

	return 0;
}

//**********************************************************************************
DWORD AddressThread(LPVOID *pParm)
{
	char	szAddressesASCII[256];
	char	*szAddressArray[MAX_HOSTS];
	DWORD	dwAddrArray[MAX_HOSTS], i, j, dwReps, dwNumAddr;
	DWORD	dwResolves;
	HOSTENT *pHstData;
	char	szName[MAX_HOSTS][MAX_NAME_SIZE];
	BOOL	bMatch;

	QAMessage(TEXT("AddressThread++"));

	for(i = 0; i < MAX_HOSTS; i++)
		szAddressArray[i] = NULL;

#if defined UNICODE
	wcstombs(szAddressesASCII, g_szAddresses, sizeof(szAddressesASCII));
#else
	strncpy(szAddressesASCII, g_szAddresses, sizeof(szAddressesASCII));
#endif

	dwNumAddr = 0;
	szAddressArray[dwNumAddr++] = &szAddressesASCII[0];

	// Here we change all the ';' characters in szAddressASCII to NULLs
	// And we set each element of szAddress Array equal to the respective
	// now null-terminated address.
	for(i = 0; i < 256 && szAddressesASCII[i] != '\0' && dwNumAddr <= MAX_HOSTS; i++)
	{
		if(szAddressesASCII[i] == ';')
		{
			szAddressesASCII[i] = '\0';
			if(i == 255 || szAddressesASCII[i+1] == '\0')
				break;		// Don't worry about trailing semi-colons
			szAddressArray[dwNumAddr++] = &szAddressesASCII[i+1];
		}
	}

	QAMessage(TEXT("AddrThread: Using the following %d addresses:"), dwNumAddr);
	for(i = 0; i < dwNumAddr; i++)
		QAMessage(TEXT("AddrThread:     Addr %d: %hs"), i, szAddressArray[i]);

	for(i = 0; i < dwNumAddr; i++)
	{
		if((dwAddrArray[i] = inet_addr(szAddressArray[i])) == INADDR_NONE)
		{
			QAError(TEXT("Invalid address parameter = %hs\n"), szAddressArray[i]);
			goto exitThread;
		}
	}

	for(dwResolves = 0, dwReps = 0; dwReps < g_dwReps; dwReps++)
	{
		for(i = 0; i < dwNumAddr; i++)
		{
			if(g_dwWaitTime)
				Sleep(g_dwWaitTime);

			pHstData = gethostbyaddr( (char *)&dwAddrArray[i], sizeof(DWORD), AF_INET);
			if(pHstData == NULL)
			{
				QAError(TEXT("AddrThread: gethostbyaddr() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
			}
			else
			{	
				if(dwReps == 0)
				{
					// Store the name
					strncpy(szName[i], pHstData->h_name, MAX_NAME_SIZE);
					QAMessage(TEXT("AddrThread: Address[%hs] --> Name[%hs]"), szAddressArray[i], szName[i]);
				}
				else
				{
					// Verify that the name given has not changed
					bMatch = FALSE;
					
					if(_stricmp(szName[i], pHstData->h_name) == 0)
						bMatch = TRUE;
					else if(pHstData->h_aliases)
					{
						// Might be an alias
						for(j = 0; pHstData->h_aliases[j] != NULL; j++)
						{
							if(_stricmp(szName[i], pHstData->h_aliases[j]) == 0)
							{
								bMatch = TRUE;
								break;
							}
						}
					}
					
					if(!bMatch)
					{
						QAError(TEXT("AddrThread: Couldn't find original name %hs in info returned by gethostbyaddr()"),
							szName[i]);
					}
				}
			}
			dwResolves++;	
		}

		if((dwReps + 1) % g_dwMessageFrequency == 0)
			QAMessage(TEXT("AddrThread: address resolution performed %d time(s) in %d reps"), dwResolves, dwReps + 1);
	}

exitThread:

	QAMessage(TEXT("AddressThread--"));

	return 0;
}

//**********************************************************************************
DWORD NameThread(LPVOID *pParm)
{
	char	szNamesASCII[256];
	char	*szNameArray[MAX_HOSTS];
	DWORD	i, j, dwReps, dwNumName;
	DWORD	dwResolves;
	HOSTENT *pHstData;
	char	pAddr[MAX_HOSTS][MAX_ADDR_SIZE];
	BOOL	bMatch;

	QAMessage(TEXT("NameThread++"));

	for(i = 0; i < MAX_HOSTS; i++)
		szNameArray[i] = NULL;

#if defined UNICODE
	wcstombs(szNamesASCII, g_szNames, sizeof(szNamesASCII));
#else
	strncpy(szNamesASCII, g_szNames, sizeof(szNamesASCII));
#endif

	dwNumName = 0;
	szNameArray[dwNumName++] = &szNamesASCII[0];

	// Here we change all the ';' characters in szNamesASCII to NULLs
	// And we set each element of szNameArray equal to the respective
	// now null-terminated name.
	for(i = 0; i < 256 && szNamesASCII[i] != '\0' && dwNumName <= MAX_HOSTS; i++)
	{
		if(szNamesASCII[i] == ';')
		{
			szNamesASCII[i] = '\0';
			if(i == 255 || szNamesASCII[i+1] == '\0')
				break;		// Don't worry about trailing semi-colons
			szNameArray[dwNumName++] = &szNamesASCII[i+1];
		}
	}

	QAMessage(TEXT("NameThread: Using the following %d names:"), dwNumName);
	for(i = 0; i < dwNumName; i++)
		QAMessage(TEXT("NameThread:     Name %d: %hs"), i, szNameArray[i]);

	for(dwResolves = 0, dwReps = 0; dwReps < g_dwReps; dwReps++)
	{
		for(i = 0; i < dwNumName; i++)
		{
			if(g_dwWaitTime)
				Sleep(g_dwWaitTime);
			
			pHstData = gethostbyname(szNameArray[i]);
			if(pHstData == NULL)
			{
				QAError(TEXT("NameThread: gethostbyname() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
			}
			else
			{
				if(dwReps == 0)
				{
					// Store the first address
					memcpy(pAddr[i], pHstData->h_addr_list[0], pHstData->h_length);
					QAMessage(TEXT("NameThread: Name[%hs] --> Addr[%x]"), szNameArray[i], htonl(*((DWORD *)pAddr[i])));
				}
				else
				{
					// Verify that the address given has not changed
					bMatch = FALSE;
					
					for(j = 0; pHstData->h_addr_list[j] != NULL; j++)
					{
						if(memcmp(pAddr[i], pHstData->h_addr_list[j], pHstData->h_length) == 0)
						{
							bMatch = TRUE;
							break;
						}
					}
					
					if(!bMatch)
					{
						QAError(TEXT("NameThread: Couldn't find original addr %d in info returned by gethostbyname()"),
							(DWORD *)pAddr[i]);
					}
				}
			}
			dwResolves++;
		}

		if((dwReps + 1) % g_dwMessageFrequency == 0)
			QAMessage(TEXT("NameThread: address resolution performed %d time(s) in %d reps"), dwResolves, dwReps + 1);
	}

	QAMessage(TEXT("NameThread--"));

	return 0;
}