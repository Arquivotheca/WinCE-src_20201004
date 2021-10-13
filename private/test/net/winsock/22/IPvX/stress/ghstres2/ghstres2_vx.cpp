//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "netmain.h"
#include <strsafe.h>
#include "Iphlpapi.h"

#define WINSOCK_VERSION_REQ     MAKEWORD(2,2)

#define MAX_HOSTS               5
#define MAX_ADDR_SIZE           32
#define MAX_NAME_SIZE           256

#define DEFAULT_MSGTIME         10
#define DEFAULT_WAITTIME        0
#define DEFAULT_REPS            0

#define IP_UNSPEC               0
#define IP_V4                   4
#define IP_V6                   6

void PrintUsage()
{
    QAMessage(TEXT("ghstres2_vX [-f Frequency] [-m Milliseconds] [-r Reps] [-a IPAddress] [-n HostName] [-ipv IPVersion] [-s] [-l]"));
    QAMessage(TEXT("    f:  Number of interations to perform before displaying status (default: %d)"), DEFAULT_MSGTIME);
    QAMessage(TEXT("    m:  Number of milliseconds to wait between iterations (default: %d)"), DEFAULT_WAITTIME);
    QAMessage(TEXT("    r:  Number of reps to execute the test (default: %d)"), DEFAULT_REPS);
    QAMessage(TEXT("    a:  IP address of a host to resolve into a name"));
    QAMessage(TEXT("    n:  Name of a host to resolve into an address"));
    QAMessage(TEXT("    s:  Flag to perform Resolution Test against primary DNS server"));
    QAMessage(TEXT("  ipv:  Flag to force IP Version.  Bypassed by -a.  [values: 4 or 6] (default: AF_UNSPEC)"));
    QAMessage(TEXT("    l:  Force the usage of legacy Get Host Stress - ghstres2.exe."));
    QAMessage(TEXT("*** - NOTE - Multiple addresses and names can be given but they must be separated"));
    QAMessage(TEXT("             with semi-colons.  Up to %d names/addresses."), MAX_HOSTS);
}

DWORD   g_dwMessageFrequency;
DWORD   g_dwWaitTime;
DWORD   g_dwReps;
DWORD   g_dwIPVersion;
BOOL    g_bGetHostAPIs = FALSE;
LPTSTR  g_szAddresses, g_szNames;

DWORD AddressThreadVx(LPVOID *pParm);
DWORD NameThreadVx(LPVOID *pParm);
DWORD AddressThread(LPVOID *pParm);
DWORD NameThread(LPVOID *pParm);

extern "C" LPTSTR gszMainProgName  = _T("ghstres2_vX");
extern "C" DWORD  gdwMainInitFlags = INIT_NET_CONNECTION;

extern "C" mymain (int argc, TCHAR *argv[])
{
    WSADATA      WSAData;
    HANDLE       hAddrThread = NULL, hNameThread = NULL;
    DWORD        ThreadId;

    if(QAWasOption(_T("?")) || QAWasOption(_T("help")))
    {
        PrintUsage();
        return 0;
    }

    // Initialize globals
    g_dwMessageFrequency = DEFAULT_MSGTIME;
    g_dwWaitTime = DEFAULT_WAITTIME;
    g_dwReps = DEFAULT_REPS;
    g_dwIPVersion = 0;
    g_szAddresses = g_szNames = NULL;

    QAGetOptionAsDWORD(_T("m"), &g_dwWaitTime);
    QAGetOptionAsDWORD(_T("ipv"), &g_dwIPVersion);
    QAGetOptionAsDWORD(_T("r"), &g_dwReps);

    QAGetOption(_T("a"), &g_szAddresses);
    QAGetOption(_T("n"), &g_szNames);
    

    if(QAGetOptionAsDWORD(_T("f"), &g_dwMessageFrequency) && g_dwMessageFrequency == 0)
        g_dwMessageFrequency = 1;    // MessageFrequency can not be 0

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

    if (QAWasOption(_T("l")))
    {
        QAMessage(TEXT("Using Legacy ghstress.exe stress API."));
        g_dwIPVersion = IP_V4;  // Only v4 was used on old stress.
        g_bGetHostAPIs = TRUE;
    }

    // if -s flag was set.
    if (QAWasOption(_T("s")))
    {
        ADDRINFO    AddrHints  = {0};
        ADDRINFO    *pAddrInfo = NULL;
        PIP_ADAPTER_DNS_SERVER_ADDRESS  dnsServerAddresses = NULL;

        CHAR    szName[MAX_NAME_SIZE];
        TCHAR   tszAdresses[MAX_NAME_SIZE];
        TCHAR   tszName[MAX_NAME_SIZE];

        DWORD   dwAddressSize   = MAX_NAME_SIZE;
        size_t  retVal          = 0;
        ULONG   outBufLen       = 0;
        INT     af              = AF_UNSPEC;

        PIP_ADAPTER_ADDRESSES pAddresses = NULL;


        // Take the IP Address of the Currently set DNS server
        // Get the Address Info and use this to extract a DNS Server

        pAddresses = (IP_ADAPTER_ADDRESSES *) malloc(outBufLen);
        
        if (pAddresses == NULL) 
        {
            QAError(TEXT("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n"));
            goto exit;
        }

        if (ERROR_BUFFER_OVERFLOW == GetAdaptersAddresses(AF_UNSPEC, 0, NULL, pAddresses, &outBufLen))
        {
            free(pAddresses);
            pAddresses = NULL;
            pAddresses = (IP_ADAPTER_ADDRESSES *) malloc(outBufLen);
            if (pAddresses == NULL) 
            {
                QAError(TEXT("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n"));
                goto exit;
            }
        }

        if (NO_ERROR == GetAdaptersAddresses(AF_UNSPEC, 0, NULL, pAddresses, &outBufLen)) 
        {
            PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;

            while (pCurrAddresses) 
            {
                if (pCurrAddresses->FirstDnsServerAddress)
                {
                    dnsServerAddresses = pCurrAddresses->FirstDnsServerAddress;
                    break;
                }

                pCurrAddresses = pCurrAddresses->Next;
            }
        }

        if (!dnsServerAddresses)
        {
            QAError(TEXT("GetAdaptersAddresses() Could not find a DNS server to use!"));
            goto exit;
        }

        // Loop through Primary and Secondary DNS servers to find an appropriate server to use for testing.
        while (dnsServerAddresses)
        {
            dwAddressSize   = MAX_NAME_SIZE;
            if(WSAAddressToString(dnsServerAddresses->Address.lpSockaddr, dnsServerAddresses->Address.iSockaddrLength, NULL, tszAdresses, &dwAddressSize))
            {
                QAMessage(TEXT("WSAAddressToString() error %d = %s - Converting DNS address."), WSAGetLastError(), GetLastErrorText());
                goto nextServer;
            }
     
            // Get the Name of the DNS Server from the IP Address (v4 on corpnet)
            // Perform rDNS to retrieve the name of the server
            // Get the Name of the DNS Server for Forward Name Resolution
            if(getnameinfo(dnsServerAddresses->Address.lpSockaddr, dnsServerAddresses->Address.iSockaddrLength, szName, sizeof(szName), NULL, 0, NI_NAMEREQD))
            {
                QAMessage(TEXT("getnameinfo() error %d = %s - Could not get DNS Name parameter."), WSAGetLastError(), GetLastErrorText());
                goto nextServer;
            }

            StringCchPrintf( tszName, MAX_NAME_SIZE, L"%hs", szName ) ;
            
            // Finally perform a getaddrinfo based off the name of the server address 
            // to find a v6 address to use.
            if(IP_V6 == g_dwIPVersion || IP_UNSPEC == g_dwIPVersion)
            {
                // Get the IP Address of the DNS Server 
                memset(&AddrHints, 0, sizeof(AddrHints));
                AddrHints.ai_family = AF_INET6;
                
                if(getaddrinfo(szName, NULL, &AddrHints, &pAddrInfo))
                {
                    QAMessage(TEXT("getaddrinfo() error %d = %s - Could not get a v6 address for DNS."), WSAGetLastError(), GetLastErrorText());
                    goto nextServer;
                }

                if (pAddrInfo)
                {
                    TCHAR   tszAdresseV6[MAX_NAME_SIZE];

                    dwAddressSize   = MAX_NAME_SIZE;
                    if(WSAAddressToString(pAddrInfo->ai_addr, pAddrInfo->ai_addrlen, NULL, tszAdresseV6, &dwAddressSize))
                    {
                        QAMessage(TEXT("WSAAddressToString() error %d = %s - Converting v6 address found."), WSAGetLastError(), GetLastErrorText());
                        goto nextServer;
                    }

                    if (IP_UNSPEC == g_dwIPVersion)
                    {
                        TCHAR temp[MAX_NAME_SIZE];
                        StringCchPrintf( temp, MAX_NAME_SIZE, L";%s", tszAdresseV6 ) ;
                        StringCchCat( tszAdresses, MAX_NAME_SIZE, temp ) ;
                    }
                    else if (IP_V6 == g_dwIPVersion)
                        StringCchPrintf( tszAdresses, MAX_NAME_SIZE, L"%s", tszAdresseV6 ) ;

                    break;
                }
            }
            else if (IP_V4 == g_dwIPVersion) // If v4, DNS address is already v4.
            {
                break;
            }
nextServer:
            dnsServerAddresses = dnsServerAddresses->Next;
        }
        
        // We've looped through all our DNS Servers and still didn't find any server
        // suitable for forward and reverse name resolution.
        if(!dnsServerAddresses)
        {
            QAError(TEXT("DNS servers assigned on this device are not appropriate for this test."), WSAGetLastError(), GetLastErrorText());
            goto exit;
        }

        // Use our newly found information in our test.
        g_szAddresses = tszAdresses;
        g_szNames = tszName;
    }
    
    if(g_szAddresses)
    {
        if (g_bGetHostAPIs)
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
        else
        {
            if ((hAddrThread = 
                CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) AddressThreadVx,
                NULL, 0, &ThreadId)) == NULL)
            {
                QAError(TEXT("BSTRESS: CreateThread(AddressToNameThread) failed %d"),
                    GetLastError());

                goto exit;
            }
        }
    }

    if(g_szNames)
    {
        if (g_bGetHostAPIs)
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
        else
        {
            if ((hNameThread = 
                CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) NameThreadVx,
                NULL, 0, &ThreadId)) == NULL)
            {
                QAError(TEXT("BSTRESS: CreateThread(NameToAddressThread) failed %d"),
                    GetLastError());

                goto exit;
            }
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
DWORD AddressThreadVx(LPVOID *pParm)
{
    char        szAddressesASCII[256];
    char        *szAddressArray[MAX_HOSTS];
    DWORD       i, dwReps, dwNumAddr;
    DWORD       dwResolves;
    char        szName[MAX_HOSTS][MAX_NAME_SIZE];
    char        szNameASCII[MAX_NAME_SIZE];
    ADDRINFO    AddrHints, *pAddrInfoArray[MAX_HOSTS];
    size_t      retVal = 0;

    QAMessage(TEXT("AddressThread++"));

    for(i = 0; i < MAX_HOSTS; i++)
        szAddressArray[i] = NULL;

#if defined UNICODE
    wcstombs_s(&retVal, szAddressesASCII, 256, g_szAddresses, sizeof(szAddressesASCII));
#else
    strncpy(szAddressesASCII, g_szAddresses, sizeof(szAddressesASCII));
#endif

    dwNumAddr = 0;
    szAddressArray[dwNumAddr++] = &szAddressesASCII[0];

    // Here we change all the ';' characters in szAddressASCII to NULLs
    // And we set each element of szAddress Array equal to the respective
    // now null-terminated address.
    for(i = 0; i < 256 && szAddressesASCII[i] != '\0' && dwNumAddr < MAX_HOSTS; i++)
    {
        if(szAddressesASCII[i] == ';')
        {
            szAddressesASCII[i] = '\0';
            if(i == 255 || szAddressesASCII[i+1] == '\0')
                break;        // Don't worry about trailing semi-colons
            szAddressArray[dwNumAddr++] = &szAddressesASCII[i+1];
        }
    }

    QAMessage(TEXT("AddrThread: Using the following %d addresses:"), dwNumAddr);
    for(i = 0; i < dwNumAddr; i++)
        QAMessage(TEXT("AddrThread:     Addr %d: %hs"), i, szAddressArray[i]);

    // Initialize our values
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
                // TODO: we currently don't support v6.  Remove this once we do.
                if (AF_INET6 != pAddrInfoArray[i]->ai_family)
                {
                    QAError(TEXT("AddrThread: getnameinfo() failed, error %d = %s"),
                        WSAGetLastError(), GetLastErrorText());
                }
            }
            else
            {    
                if(dwReps == 0)
                {
                    // Store the name
                    StringCchCopyA(szName[i], MAX_NAME_SIZE, szNameASCII);
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
DWORD NameThreadVx(LPVOID *pParm)
{
    char        szNamesASCII[256];
    char        *szNameArray[MAX_HOSTS];
    DWORD       i, dwReps, dwNumName;
    DWORD       dwResolves;
    ADDRINFO    AddrHints, *pAddrInfo, *pAI;
    struct in_addr6    pAddr[MAX_HOSTS];
    size_t retVal = 0;

    QAMessage(TEXT("NameThread++"));

    memset(pAddr, 0, sizeof(pAddr));

    for(i = 0; i < MAX_HOSTS; i++)
        szNameArray[i] = NULL;

#if defined UNICODE
    wcstombs_s(&retVal, szNamesASCII, 256, g_szNames, sizeof(szNamesASCII));
#else
    strncpy(szNamesASCII, g_szNames, sizeof(szNamesASCII));
#endif

    dwNumName = 0;
    szNameArray[dwNumName++] = &szNamesASCII[0];

    // Here we change all the ';' characters in szNamesASCII to NULLs
    // And we set each element of szNameArray equal to the respective
    // now null-terminated name.
    for(i = 0; i < 256 && szNamesASCII[i] != '\0' && dwNumName < MAX_HOSTS; i++)
    {
        if(szNamesASCII[i] == ';')
        {
            szNamesASCII[i] = '\0';
            if(i == 255 || szNamesASCII[i+1] == '\0')
                break;        // Don't worry about trailing semi-colons
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


//**********************************************************************************
DWORD AddressThread(LPVOID *pParm)
{
    char    szAddressesASCII[256];
    char    *szAddressArray[MAX_HOSTS];
    DWORD   dwAddrArray[MAX_HOSTS], i, j, dwReps, dwNumAddr;
    DWORD   dwResolves;
    HOSTENT *pHstData;
    char    szName[MAX_HOSTS][MAX_NAME_SIZE];
    BOOL    bMatch;
    size_t  retVal = 0;

    QAMessage(TEXT("AddressThread++"));

    for(i = 0; i < MAX_HOSTS; i++)
        szAddressArray[i] = NULL;

#if defined UNICODE
    wcstombs_s(&retVal, szAddressesASCII, 256, g_szAddresses, sizeof(szAddressesASCII));
#else
    strncpy(szAddressesASCII, g_szAddresses, sizeof(szAddressesASCII));
#endif

    dwNumAddr = 0;
    szAddressArray[dwNumAddr++] = &szAddressesASCII[0];

    // Here we change all the ';' characters in szAddressASCII to NULLs
    // And we set each element of szAddress Array equal to the respective
    // now null-terminated address.
    for(i = 0; i < 256 && szAddressesASCII[i] != '\0' && dwNumAddr < MAX_HOSTS; i++)
    {
        if(szAddressesASCII[i] == ';')
        {
            szAddressesASCII[i] = '\0';
            if(i == 255 || szAddressesASCII[i+1] == '\0')
                break;        // Don't worry about trailing semi-colons
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
                    StringCchCopyA(szName[i], MAX_NAME_SIZE, pHstData->h_name);
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
    char    szNamesASCII[256];
    char    *szNameArray[MAX_HOSTS];
    DWORD   i, j, dwReps, dwNumName;
    DWORD   dwResolves;
    HOSTENT *pHstData;
    char    pAddr[MAX_HOSTS][MAX_ADDR_SIZE];
    BOOL    bMatch;
    size_t  retVal = 0;

    QAMessage(TEXT("NameThread++"));

    for(i = 0; i < MAX_HOSTS; i++)
        szNameArray[i] = NULL;

#if defined UNICODE
    wcstombs_s(&retVal, szNamesASCII, 256, g_szNames, sizeof(szNamesASCII));
#else
    strncpy(szNamesASCII, g_szNames, sizeof(szNamesASCII));
#endif

    dwNumName = 0;
    szNameArray[dwNumName++] = &szNamesASCII[0];

    // Here we change all the ';' characters in szNamesASCII to NULLs
    // And we set each element of szNameArray equal to the respective
    // now null-terminated name.
    for(i = 0; i < 256 && szNamesASCII[i] != '\0' && dwNumName < MAX_HOSTS; i++)
    {
        if(szNamesASCII[i] == ';')
        {
            szNamesASCII[i] = '\0';
            if(i == 255 || szNamesASCII[i+1] == '\0')
                break;        // Don't worry about trailing semi-colons
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