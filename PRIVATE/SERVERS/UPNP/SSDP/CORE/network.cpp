//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <ssdppch.h>
#pragma hdrstop

#include <iphlpapi.h>
#include "md5.h"

#include "ipsupport.h"
#include "ssdpnetwork.h"
#include "upnp_config.h"

long            g_bExitSSDPReadThread = FALSE;

CHAR            g_lpszNls[100];     // string representation of current NLS
MD5_CTX         g_md5Nls;           // md5 context used during calculation of NLS
extern CHAR     g_pszNlsFormat[65];

extern SOCKADDR_IN  MulticastAddressIPv4;
extern SOCKADDR_IN6 MulticastLinkScopeAddressIPv6;
extern SOCKADDR_IN6 MulticastSiteScopeAddressIPv6;

// a list of distinct networks hosted on this machine
static LIST_ENTRY listNetwork;
static CRITICAL_SECTION CSListNetwork;

// prototypes
static PSSDPNetwork AddToListNetwork(PSOCKADDR_STORAGE IpAddress);
VOID FreeSSDPNetwork(SSDPNetwork *pSSDPNetwork);

// functions
VOID GetNetworkLock()
{
    EnterCriticalSection(&CSListNetwork);
}

VOID FreeNetworkLock()
{
    LeaveCriticalSection(&CSListNetwork);
}

// should be called while holding NetworkLock
PSSDPNetwork GetNextNetwork(PSSDPNetwork prev)
{
    PLIST_ENTRY p = listNetwork.Flink;
    PLIST_ENTRY pListHead = &listNetwork;
    if (prev)
    	p = prev->linkage.Flink;

    if (p != &listNetwork)
    {
    	return CONTAINING_RECORD (p, SSDPNetwork, linkage);
    }
    return NULL;
}


/***********************************************************
Function :   AddSocketToNetworkList()
  
    New function pulled out of  GetNetworks() as we may need to do this twice for each
    interface.  Once for v4 & once for v6.
    
    Open & bind a socket (using SocketOpen() ) & then add it to the list

***********************************************************/
void AddSocketToNetworkList(PSOCKADDR_STORAGE pSockaddr, int iSockaddrLength, DWORD dwAdapterIndex)
{
    SOCKET      socketToOpen = INVALID_SOCKET;
    PSOCKADDR   pMulticastAddr = NULL;
    DWORD       dwScopeId = 0, dwScope = 0;

    // select proper multicast address
    if(pSockaddr->ss_family == AF_INET)
    {
        pMulticastAddr = (PSOCKADDR)&MulticastAddressIPv4;
        
        ((PSOCKADDR_IN)pSockaddr)->sin_port = htons(SSDP_PORT);
    }
    else if(pSockaddr->ss_family == AF_INET6)
    {
        ((PSOCKADDR_IN6)pSockaddr)->sin6_port = htons(SSDP_PORT);
        
        if(IN6_IS_ADDR_LINKLOCAL(&((PSOCKADDR_IN6)pSockaddr)->sin6_addr))
        {
	        // scope and scope id for the link
	        dwScopeId = dwAdapterIndex;
	        dwScope = 1;
            
            // multicast address with link scope
            pMulticastAddr = (PSOCKADDR)&MulticastLinkScopeAddressIPv6;
        }
        else if(IN6_IS_ADDR_SITELOCAL(&((PSOCKADDR_IN6)pSockaddr)->sin6_addr))
        {
            // check if site scope should be supported
            if(upnp_config::site_scope() < 3)
                return;
            
            // scope and scope id for the site
            dwScopeId = ((PSOCKADDR_IN6)pSockaddr)->sin6_scope_id;
            dwScope = 2;
            
            // multicast address with site scope
            pMulticastAddr = (PSOCKADDR)&MulticastSiteScopeAddressIPv6;
        }
        else
            // scope not supported by UPnP
            return;
            
        Assert(dwScope != 0);
        Assert(dwScopeId != 0);
        
        // check if we already have address with the same scope and scope id
        PSSDPNetwork pNet = NULL;
        
        GetNetworkLock();

	    while(pNet = GetNextNetwork(pNet))
	        if(pNet->dwScopeId == dwScopeId && pNet->dwScope == dwScope)
	            break;
        
	    FreeNetworkLock();
        
	    if(pNet != NULL)
	        return;
    }
    else
        return; // unknown address family
     
    // create a socket
    if(SocketOpen(&socketToOpen, (PSOCKADDR)pSockaddr, dwAdapterIndex, pMulticastAddr))
    {
        SSDPNetwork *pSsdpNetwork;

        // create network
        pSsdpNetwork = (SSDPNetwork*) malloc (sizeof(SSDPNetwork));

        if (pSsdpNetwork != NULL)
        {
            WCHAR lpwszAddress[MAX_ADDRESS_SIZE];
            DWORD dwSize;
            
            // add address to the digest
            MD5Update(&g_md5Nls, (uchar*)pSockaddr, iSockaddrLength);
            
            memset(pSsdpNetwork, 0, sizeof(*pSsdpNetwork));
            
            pSsdpNetwork->Type = SSDP_NETWORK_SIGNATURE;
            pSsdpNetwork->state = NETWORK_INIT;
            pSsdpNetwork->pMulticastAddr = pMulticastAddr;
            pSsdpNetwork->dwIndex = dwAdapterIndex;
            pSsdpNetwork->dwScopeId = dwScopeId;
            pSsdpNetwork->dwScope = dwScope;
            pSsdpNetwork->socket = socketToOpen;
            
            // set UPnP port so that it is included in address string
            if(pSockaddr->ss_family == AF_INET)
            {    
                ((PSOCKADDR_IN)pSockaddr)->sin_port = htons(upnp_config::port());
            }
            else
            {    
                ((PSOCKADDR_IN6)pSockaddr)->sin6_port = htons(upnp_config::port());
                ((PSOCKADDR_IN6)pSockaddr)->sin6_scope_id = 0;
            }
            
            // store address in string form
            WSAAddressToString((LPSOCKADDR)pSockaddr, sizeof(SOCKADDR_STORAGE), NULL, lpwszAddress, &(dwSize = sizeof(lpwszAddress)));
            wcstombs(pSsdpNetwork->pszAddressString, lpwszAddress, sizeof(pSsdpNetwork->pszAddressString));
            
            // store multicast address in string form
            WSAAddressToString((LPSOCKADDR)pMulticastAddr, sizeof(SOCKADDR_STORAGE), NULL, lpwszAddress, &(dwSize = sizeof(lpwszAddress)));
            wcstombs(pSsdpNetwork->pszMulticastAddr, lpwszAddress, sizeof(pSsdpNetwork->pszMulticastAddr));
            
            // Add it to the list
            GetNetworkLock();
            InsertHeadList(&listNetwork, &(pSsdpNetwork->linkage));
            FreeNetworkLock();
        }
        else 
        {            
            TraceTag(ttidSsdpNetwork, "AddSocketToNetworkList() - couldn't allocate pSsdpNetwork");
        }
    }   // END if( SocketOpen() )
}


/***********************************************************
Function :   GetNetworks()
  
    Build list of sockets on which we send & recieve SSDP multicast announcements.
    Almost complete rewrite of function which previously used getAdapters Info() to 
    build the list of addresses and chose the 1st address per adapter.
    
    Loop through all the available adapters (interfaces)
    For each adapter loop through the available addresses
    Use addresses which are either loopback, or where the adapter is multicast capable
    If address found is IPv4, just take the first one we find per adapter
    If it's IPv6 then look at all the addresses & pick the most preferred (order is 
    Site Local 1st preference, then Link Local else we use a global address)
    For the IPv6 loopback we select the standard loopback, i.e we'll take ::1 in preference
    to fe80::1 .
    For each chosen address open a socket  & add it to the list using AddSocketToNetworkList()
    We also now store the adapter guid, so that we can match V4 & V6 addresses to the same
    interface in order to build the AL header.

    On sucess returns 0.

***********************************************************/
INT GetNetworks()
{
    PIP_ADAPTER_ADDRESSES   pAdapterAddresses = NULL;  // buffer used by GetAdaptersAddresses()
    ULONG                   ulBufSize = 0;    // size of buffer returned by GetAdaptersAddresses()
    DWORD                   dwRes  = 0;   // result codes from iphelper apis
    bool                    bIPv6 = false;
    
    MD5Init(&g_md5Nls);
    
    // Find out size of returned buffer
    dwRes = GetAdaptersAddresses(
                upnp_config::family(),
                GAA_FLAG_SKIP_ANYCAST |GAA_FLAG_SKIP_DNS_SERVER,
                NULL,
                pAdapterAddresses,
                &ulBufSize
                );
    
    if(ulBufSize)
    {
        // Allocate sufficient Space
        pAdapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(ulBufSize));
        if (pAdapterAddresses !=NULL)
        {
            // Get Adapter List
            dwRes = GetAdaptersAddresses(
                    upnp_config::family(), 
                    GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_DNS_SERVER,
                    NULL,
                    pAdapterAddresses,
                    &ulBufSize
                    );
            
            if (dwRes == ERROR_SUCCESS)
            {
                // Loop through all the adapters (interfaces) returned
                for(PIP_ADAPTER_ADDRESSES pAdapterIter = pAdapterAddresses; pAdapterIter != NULL; pAdapterIter = pAdapterIter->Next)
                {
                    // don't use tunneling adapters
                    if(pAdapterIter->IfType == IF_TYPE_TUNNEL)
                        continue;
                    
                    if(!upnp_config::is_adapter_enabled(pAdapterIter->AdapterName))
                        continue;
                    
                    TraceTag(ttidSsdpNetwork, "GetNetworks() - Adapter %d : %S \n", pAdapterIter->IfIndex, pAdapterIter->FriendlyName);

                    // Loop through all the addresses returned
                    for(PIP_ADAPTER_UNICAST_ADDRESS pUnicastAddress = pAdapterIter->FirstUnicastAddress;
                        pUnicastAddress != NULL;
                        pUnicastAddress = pUnicastAddress->Next)
                    {
                        SOCKADDR_STORAGE sockaddr = {0};
                        
                        memcpy(&sockaddr, pUnicastAddress->Address.lpSockaddr, pUnicastAddress->Address.iSockaddrLength);

                        if(sockaddr.ss_family == AF_INET)
                            AddSocketToNetworkList(&sockaddr, pUnicastAddress->Address.iSockaddrLength, pAdapterIter->IfIndex);
                        else
                        {
                            AddSocketToNetworkList(&sockaddr, pUnicastAddress->Address.iSockaddrLength, pAdapterIter->Ipv6IfIndex);
                            
                            bIPv6 = true;
                        }
                    }
                }
            }
            
            // Tidy up
            free (pAdapterAddresses);
        }
    }

    MD5Final(&g_md5Nls);
    
    sprintf(g_lpszNls, g_pszNlsFormat, ((WORD*)g_md5Nls.digest)[0],
                                       ((WORD*)g_md5Nls.digest)[1],
                                       ((WORD*)g_md5Nls.digest)[2],
                                       ((WORD*)g_md5Nls.digest)[3],
                                       ((WORD*)g_md5Nls.digest)[4],
                                       ((WORD*)g_md5Nls.digest)[5],
                                       ((WORD*)g_md5Nls.digest)[6],
                                       ((WORD*)g_md5Nls.digest)[7]);
    
    if(!bIPv6 && !upnp_config::use_nls_for_IPv4())
        g_lpszNls[0] = '\x0';

    return 0;
}


VOID InitializeListNetwork()
{
    InitializeCriticalSection(&CSListNetwork);
    GetNetworkLock();
    InitializeListHead(&listNetwork);
    FreeNetworkLock();
}


VOID FreeSSDPNetwork(SSDPNetwork *pSSDPNetwork)
{
    GetNetworkLock();
    
    RemoveEntryList(&(pSSDPNetwork->linkage));

    if (pSSDPNetwork->socket != INVALID_SOCKET)
        SocketClose(pSSDPNetwork->socket);
    
    free(pSSDPNetwork);
    
    FreeNetworkLock();
}


VOID CleanupListNetwork()
{
    TraceTag(ttidSsdpNetwork, "----- Cleanup SSDP Network List -----");
    StopListenOnAllNetworks();
    DeleteCriticalSection(&CSListNetwork);
}


extern DWORD ProcessReceiveBuffer(VOID* pvContext);

static DWORD WINAPI SSDPReadThread(void*)
{
	int ReturnValue;
	CHAR *pszData;
	SOCKADDR_STORAGE RemoteSocket;
	fd_set recvset;
	UINT i;
	static ssdp_queue queue;

	// thread exits when one of the sockets is closed
	while (1)
	{
        PLIST_ENTRY p;
        PLIST_ENTRY pListHead = &listNetwork;
        
    	FD_ZERO(&recvset);
    	
    	// go through the network list and construct a socket list to recv on
    	GetNetworkLock();
        
        for (p = pListHead->Flink, i = 0; p != pListHead; p = p->Flink, ++i)
        {
            SSDPNetwork *pssdpNetwork;

            pssdpNetwork = CONTAINING_RECORD (p, SSDPNetwork, linkage);
            FD_SET(pssdpNetwork->socket, &recvset);
        }
        
    	FreeNetworkLock();
    	
        if(i)
            TraceTag(ttidSsdpNetwork, "Listening on %d network(s).", i); 
        else
            TraceTag(ttidError, "No networks to listen on!"); 
    	
	    ReturnValue = select(0, &recvset, NULL, NULL, NULL);
	    
	    if (!ReturnValue || ReturnValue == SOCKET_ERROR)
	    {
	        TraceTag(ttidSsdpNetwork, "----- select failed with error code "
	                 "%d -----", GetLastError());
	        break;
	    }
	    
	    if (InterlockedExchange(&g_bExitSSDPReadThread, g_bExitSSDPReadThread))
	        break;
	    
	    for (i = 0; i < recvset.fd_count; i++)
	    {
    	    if (SocketReceive(recvset.fd_array[i], &pszData, &RemoteSocket) == TRUE)
    	    {
    	        if (RECEIVE_DATA* pData = (RECEIVE_DATA *)malloc(sizeof(RECEIVE_DATA)))
    	        {
    	            CopyMemory(&pData->RemoteSocket, &RemoteSocket, sizeof(RemoteSocket));
    	            
    	            pData->socket = recvset.fd_array[i];
    	            pData->szBuffer = pszData;
    	            pData->dwIndex = 0;
    	            
    	            GetNetworkLock();
    	            
    	            // find index of the interface that received the request
    	            for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    	            {
                        SSDPNetwork *pssdpNetwork;

                        pssdpNetwork = CONTAINING_RECORD (p, SSDPNetwork, linkage);
    	                
    	                if(pssdpNetwork->socket == recvset.fd_array[i])
    	                {
    	                    pData->dwIndex = pssdpNetwork->dwIndex;
    	                    break;
    	                }
    	            }
    	            
    	            FreeNetworkLock();
    	            
    	            if(ssdp_queue::was_not_full == queue.push(pData))
    	                g_pThreadPool->ScheduleEvent(ProcessReceiveBuffer, &queue);
    	        }
    	        else
    	        {
    	            TraceError("Couldn't allocate sufficient memory!", E_OUTOFMEMORY);
    	            free(pszData);
    	        }
    	    }
	    }
    }
    
    return 0;
}

static HANDLE g_hSsdpReadThread;
static HANDLE g_hSsdpNetMonThread;
static HANDLE g_hNetMonShutdownEvent;

INT ListenOnAllNetworks(HWND)
{
    //INT ReturnValue;
	DWORD dwSsdpThreadId;

    TraceTag(ttidSsdpNetwork, "ListenOnAllNetworks ...");
    GetNetworks();
	
	InterlockedExchange(&g_bExitSSDPReadThread, FALSE);
	
	g_hSsdpReadThread = CreateThread(
					NULL, 
					0, //cbStack
					(LPTHREAD_START_ROUTINE)SSDPReadThread,
					NULL,
					0,
					&dwSsdpThreadId);
					
	if (!g_hSsdpReadThread)
	{
        TraceTag(ttidError, "Failed to create SSDPReadThread.  Error code (%d).", GetLastError());
	}

    return g_hSsdpReadThread != NULL;
}

void StopListenOnAllNetworks()
{
    PLIST_ENTRY p;
    PLIST_ENTRY pListHead = &listNetwork;
    
    TraceTag(ttidSsdpNetwork, "StopListenOnAllNetworks ...");
    
    InterlockedExchange(&g_bExitSSDPReadThread, TRUE);
    
    GetNetworkLock();

    for (p = pListHead->Flink; p && p != pListHead;)
    {
        SSDPNetwork *pssdpNetwork;

        pssdpNetwork = CONTAINING_RECORD (p, SSDPNetwork, linkage);

        p = p->Flink;

        FreeSSDPNetwork(pssdpNetwork);
    }

    FreeNetworkLock();
    
	if (g_hSsdpReadThread != NULL)
	{
		WaitForSingleObject(g_hSsdpReadThread,INFINITE);
		CloseHandle(g_hSsdpReadThread);
		g_hSsdpReadThread = NULL;
	}
}

extern VOID SendAllAnnouncements();

static DWORD WINAPI SSDPNetMonThread(void *pData)
{
    HANDLE          hEvents[3] = {0};
    unsigned        nEvents = 0;
    SOCKET          s[2];
    WSAOVERLAPPED   ov[2] = {0};
    int             af[] = {AF_INET, AF_INET6};
    DWORD           dwTimeout;
    unsigned        i;
    
    // set up notification events for each address family
    for(i = 0; i < sizeof(af)/sizeof(*af); ++i)
    {
        Assert(i < sizeof(s)/sizeof(*s));
        Assert(i < sizeof(ov)/sizeof(*ov));
            
        // create socket for this address family
        s[nEvents] = socket(af[i], SOCK_STREAM, 0);
        
        if(s[nEvents] != INVALID_SOCKET)
        {
            // create event to be used for notifications for this address family
            ov[nEvents].hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            
            // register notification
            if(ERROR_SUCCESS != WSAIoctl(s[nEvents], SIO_ADDRESS_LIST_CHANGE, NULL, 0, NULL, 0, NULL, &ov[nEvents], NULL) &&
               ERROR_IO_PENDING != GetLastError())
            {
                closesocket(s[nEvents]);
                CloseHandle(ov[nEvents].hEvent);
            }
            else
            {
                hEvents[nEvents] = ov[nEvents].hEvent;
                nEvents++;
            }
        }
    }
    
    // add shutdown event
    hEvents[nEvents++] = reinterpret_cast<HANDLE>(pData);
    
    dwTimeout = INFINITE;
    
    // wait for address change notifications
    while(true)
    {
        DWORD dw = WaitForMultipleObjects(nEvents, hEvents, FALSE, dwTimeout);
        
        if(dw == WAIT_FAILED)
        {
            Assert(false);
            break;
        }
        
        if(dw == WAIT_OBJECT_0 + nEvents - 1)
        {
            // shutdown event signaled
            break;
        }
        
        if(dw != WAIT_TIMEOUT)
        {
            Assert(dw - WAIT_OBJECT_0 < nEvents - 1);
            Assert(dw - WAIT_OBJECT_0 < sizeof(s)/sizeof(*s));
            Assert(dw - WAIT_OBJECT_0 < sizeof(ov)/sizeof(*ov));
            
            // reenable notification
            if(ERROR_SUCCESS != WSAIoctl(s[dw - WAIT_OBJECT_0], SIO_ADDRESS_LIST_CHANGE, NULL, 0, NULL, 0, NULL, &ov[dw - WAIT_OBJECT_0], NULL) &&
               ERROR_IO_PENDING != GetLastError())
            {
                TraceTag(ttidError, "Error 0x%x trying to reenable address change notification", GetLastError());
            }
            
            // give time for the network to settle
            // wait for 7 seconds and see if there is another address change
            dwTimeout = 7000;
            continue;
        }
        
        Assert(dw == WAIT_TIMEOUT);
        
        // no more address change notifications within 7 seconds
        // update UPnP networks

        TraceTag(ttidSsdpNetwork, "Processing address change notifiction");
            
        // Remove all the networks ...
        StopListenOnAllNetworks();
        
        // ... and bringing them up again
        ListenOnAllNetworks(NULL);
        
        SendAllAnnouncements();
        
        dwTimeout = INFINITE;
    }
    
    for(i = 0; i < nEvents - 1; ++i)
    {
        closesocket(s[i]);
        CloseHandle(hEvents[i]);
    }
    
    return 1;
}


INT StartNetworkMonitorThread()
{
	DWORD dwSsdpThreadId;

    Assert(!g_hSsdpNetMonThread);
    Assert(!g_hNetMonShutdownEvent);

    TraceTag(ttidSsdpNetwork, "Starting Network Monitor Thread ...");

    g_hNetMonShutdownEvent = CreateEventW(NULL, FALSE, FALSE, NULL);

    if (!g_hNetMonShutdownEvent)
        return FALSE;
    
    g_hSsdpNetMonThread = CreateThread(
					NULL, 
					0, //cbStack
					(LPTHREAD_START_ROUTINE)SSDPNetMonThread,
					g_hNetMonShutdownEvent,
					0,
					&dwSsdpThreadId);
	
	if (!g_hSsdpNetMonThread)
	{
        TraceTag(ttidError, "Failed to create SSDPNetMonThread.  Error code (%d).", GetLastError());
        CloseHandle(g_hNetMonShutdownEvent);
        g_hNetMonShutdownEvent = NULL;
	}

    return g_hSsdpNetMonThread != NULL;
}


INT StopNetworkMonitorThread()
{
    DWORD dwWait;
    
    if (!g_hSsdpNetMonThread)
        return TRUE;
    
    Assert(g_hNetMonShutdownEvent);
    TraceTag(ttidSsdpNetwork, "Stopping Network Monitor Thread ...");
    
    SetEvent(g_hNetMonShutdownEvent);
    
    dwWait = WaitForSingleObject(g_hSsdpNetMonThread, INFINITE);
    
    CloseHandle(g_hSsdpNetMonThread);
    CloseHandle(g_hNetMonShutdownEvent);
    
    g_hNetMonShutdownEvent = NULL;
    g_hSsdpNetMonThread = NULL;
    
    return dwWait == WAIT_OBJECT_0;
}

