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

#define MAX_HOSTS			5
#define MAX_ADDR_SIZE		32
#define MAX_NAME_SIZE		256

#define DEFAULT_MSGTIME		10
#define DEFAULT_WAITTIME	0
#define DEFAULT_REPS		0

void PrintUsage()
{
	QAMessage(TEXT("ghstres2_vX [-f Frequency] [-m Milliseconds] [-r Reps] [-a IPAddress] [-n HostName]"));
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

extern "C" LPTSTR gszMainProgName  = _T("ghstres2_vX");
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
	char		szAddressesASCII[256];
	char		*szAddressArray[MAX_HOSTS];
	DWORD		i, dwReps, dwNumAddr;
	DWORD		dwResolves;
	char		szName[MAX_HOSTS][MAX_NAME_SIZE];
	char		szNameASCII[MAX_NAME_SIZE];
	ADDRINFO	AddrHints, *pAddrInfoArray[MAX_HOSTS];

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
		memset(&AddrHints, 0, sizeof(AddrHints));
		AddrHints.ai_family = PF_UNSPEC;
		AddrHints.ai_socktype = SOCK_STREAM;
		AddrHints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
		
		if(getaddrinfo(szAddressArray[i], "100", &AddrHints, &pAddrInfoArray[i]))
		{
			QAError(TEXT("getaddrinfo() error %d = %s - Invalid address parameter %d"), WSAGetLastError(), GetLastErrorText(), i);
			goto exitThread;
		}
	}

	for(dwResolves = 0, dwReps = 0; dwReps < g_dwReps; dwReps++)
	{
		for(i = 0; i < dwNumAddr; i++)
		{
			if(g_dwWaitTime)
				Sleep(g_dwWaitTime);

			if(getnameinfo(pAddrInfoArray[i]->ai_addr, pAddrInfoArray[i]->ai_addrlen, 
				szNameASCII, sizeof(szNameASCII), NULL, 0, NI_NAMEREQD))
			{
				QAError(TEXT("AddrThread: getnameinfo() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
			}
			else
			{	
				if(dwReps == 0)
				{
					// Store the name
					strncpy(szName[i], szNameASCII, MAX_NAME_SIZE);
					QAMessage(TEXT("AddrThread: Address[%hs] --> Name[%hs]"), szAddressArray[i], szName[i]);
				}
				else
				{
					// Verify that the name given has not changed
					if(_stricmp(szName[i], szNameASCII))
					{
						QAError(TEXT("AddrThread: Couldn't find original name %hs in %hs returned by gethostbyaddr()"),
							szName[i], szNameASCII);
					}
				}
			}
			dwResolves++;	
		}

		if((dwReps + 1) % g_dwMessageFrequency == 0)
			QAMessage(TEXT("AddrThread: address resolution performed %d time(s) in %d reps"), dwResolves, dwReps + 1);
	}

exitThread:

	for(i = 0; i < dwNumAddr; i++)
		freeaddrinfo(pAddrInfoArray[i]);

	QAMessage(TEXT("AddressThread--"));

	return 0;
}

//**********************************************************************************
DWORD NameThread(LPVOID *pParm)
{
	char		szNamesASCII[256];
	char		*szNameArray[MAX_HOSTS];
	DWORD		i, dwReps, dwNumName;
	DWORD		dwResolves;
	ADDRINFO	AddrHints, *pAddrInfo, *pAI;
	struct in_addr6	pAddr[MAX_HOSTS];

	QAMessage(TEXT("NameThread++"));

	memset(pAddr, 0, sizeof(pAddr));

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
			
			memset(&AddrHints, 0, sizeof(AddrHints));
			AddrHints.ai_family = PF_UNSPEC;
			AddrHints.ai_socktype = SOCK_STREAM;
			AddrHints.ai_flags = 0;
			
			if(getaddrinfo(szNameArray[i], "100", &AddrHints, &pAddrInfo))
			{
				QAError(TEXT("NameThread: getaddrinfo() failed, error %d = %s"),
					WSAGetLastError(), GetLastErrorText());
			}
			else
			{
				if(dwReps == 0)
				{
					QAMessage(TEXT("NameThread: We only check the first address returned for persistence."));
					// Store the first address
					if(pAddrInfo->ai_addr->sa_family == PF_INET)
						memcpy(&pAddr[i], &(((SOCKADDR_IN *)(pAddrInfo->ai_addr))->sin_addr.s_addr), sizeof(IN_ADDR));
					else
						memcpy(&pAddr[i], &(((SOCKADDR_IN6 *)(pAddrInfo->ai_addr))->sin6_addr.s6_addr), sizeof(struct in_addr6));
					
					// Print all the addresses
					for(pAI = pAddrInfo; pAI != NULL; pAI = pAI->ai_next)
					{
						if(pAI->ai_addr->sa_family == PF_INET)
						{
							QAMessage(TEXT("NameThrd: Name[%hs] --> AF_INET Addr[%08x]"),
								szNameArray[i], 
								htonl((DWORD)(((SOCKADDR_IN *)(pAI->ai_addr))->sin_addr.s_addr)) );
						}
						else
						{
							QAMessage(TEXT("NameThrd: Name[%hs] --> AF_INET6 Addr[%04x%04x%04x%04x%04x%04x%04x%04x]"), szNameArray[i], 
								htons((*(WORD *)&(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr.s6_addr[0]))),
								htons((*(WORD *)&(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr.s6_addr[2]))),
								htons((*(WORD *)&(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr.s6_addr[4]))),
								htons((*(WORD *)&(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr.s6_addr[6]))),
								htons((*(WORD *)&(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr.s6_addr[8]))),
								htons((*(WORD *)&(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr.s6_addr[10]))),
								htons((*(WORD *)&(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr.s6_addr[12]))),
								htons((*(WORD *)&(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr.s6_addr[14]))) );
						}
					}
				}
				else
				{
					// Verify that the address given has not changed
					for(pAI = pAddrInfo; pAI != NULL; pAI = pAI->ai_next)
					{
						if(pAI->ai_addr->sa_family == PF_INET)
						{
							if(!memcmp(&pAddr[i], &(((SOCKADDR_IN *)(pAI->ai_addr))->sin_addr.s_addr), sizeof(IN_ADDR)))
								break;
						}
						else
						{
							if(!memcmp(&pAddr[i], &(((SOCKADDR_IN6 *)(pAI->ai_addr))->sin6_addr.s6_addr), sizeof(struct in_addr6)))
								break;
						}
					}

					if(pAI == NULL)
					{
						QAError(TEXT("NameThread: Couldn't find original addr in info returned by getnameinfo()"));
					}
				}
			}

			freeaddrinfo(pAddrInfo);
			dwResolves++;
		}

		if((dwReps + 1) % g_dwMessageFrequency == 0)
			QAMessage(TEXT("NameThread: address resolution performed %d time(s) in %d reps"), dwResolves, dwReps + 1);
	}

	QAMessage(TEXT("NameThread--"));

	return 0;
}