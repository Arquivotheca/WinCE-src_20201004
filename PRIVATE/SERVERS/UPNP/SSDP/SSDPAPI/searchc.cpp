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

#include "ssdpfuncc.h"
#include "ssdpparser.h"
#include "ipsupport.h"
#include "upnp_config.h"

static CRITICAL_SECTION CSListSearch;

static CHAR *SearchHeader = "\"ssdp:discover\"";
static CHAR *MulticastUri = "*";
static CHAR *MX = "3";

#define MX_VALUE 3000
#define SELECT_TIMEOUT 60

DWORD WINAPI DoSsdpSearch(PSSDP_SEARCH_REQUEST SearchRequest);
DWORD WINAPI DoSsdpSearchThread(LPVOID lpvThreadParam);
//BOOL GetCacheResult(PSSDP_SEARCH_REQUEST SearchRequest);
VOID FreeSearchRequest(PSSDP_SEARCH_REQUEST SearchRequest);
VOID DoCancelCallback(PSSDP_SEARCH_REQUEST SearchRequest);
DWORD WINAPI SearchRequestTimerProc (VOID *Arg);
VOID StopSearchRequestTimer(PSSDP_SEARCH_REQUEST SearchRequest);
VOID WakeupSelect(PSSDP_SEARCH_REQUEST SearchRequest);

extern SOCKADDR_IN  MulticastAddressIPv4;
extern SOCKADDR_IN6 MulticastLinkScopeAddressIPv6;
extern SOCKADDR_IN6 MulticastSiteScopeAddressIPv6;

PSSDP_SEARCH_REQUEST CreateSearchRequest(const char *szType)
{
    PSSDP_SEARCH_REQUEST SearchRequest = NULL;
    INT Size = sizeof(SSDP_SEARCH_REQUEST);

    SearchRequest = new SSDP_SEARCH_REQUEST;

    if (SearchRequest == NULL)
    {
        TraceTag(ttidSsdpSearch, "Couldn't allocate  memory SearchRequest "
                 "for %s", szType);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    SearchRequest->Type = SSDP_CLIENT_SEARCH_SIGNATURE;
    SearchRequest->Size = Size;
    SearchRequest->state = SEARCH_START;
    SearchRequest->szType = NULL;
    SearchRequest->szSearch = NULL;
    SearchRequest->Callback = NULL;
    SearchRequest->Context = NULL;
    SearchRequest->NumOfRetry = 2;
    SearchRequest->fExit = 0;
    SearchRequest->hThread = NULL;
    SearchRequest->CurrentResponse = NULL;
    SearchRequest->dwTimerCookie = 0;
    SearchRequest->DoneEvent = NULL;
    SearchRequest->HitWire = FALSE;
    SearchRequest->bCallbackInProgress = FALSE;
    SearchRequest->nRefCount = 1;

    InitializeCriticalSection(&SearchRequest->cs);
    InitializeListHead(&SearchRequest->ListResponses);
    SearchRequest->CurrentResponse = SearchRequest->ListResponses.Flink;

    SearchRequest->szType = (CHAR *) malloc(strlen(szType)+1);
    if (SearchRequest->szType == NULL)
    {
        TraceTag(ttidSsdpNotify, "Couldn't allocate  memory for szType for %s", szType);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        FreeSearchRequest(SearchRequest);
        return NULL;
    }
    strcpy(SearchRequest->szType, szType);

    SearchRequest->DoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (SearchRequest->DoneEvent == NULL)
    {
        FreeSearchRequest(SearchRequest);
        return NULL;
    }

    return SearchRequest;
}


// AddNetwork
void AddNetwork(ce::vector<SSDP_SEARCH_NETWORK>& Networks, PSOCKADDR_STORAGE pSockaddr, DWORD dwAdapterIndex)
{
    PSOCKADDR   pMulticastAddr = NULL;
    DWORD       dwScopeId = 0, dwScope = 0;

    // select proper multicast address
    if(pSockaddr->ss_family == AF_INET)
    {
        pMulticastAddr = (PSOCKADDR)&MulticastAddressIPv4;
    }
    else if(pSockaddr->ss_family == AF_INET6)
    {
        if(IN6_IS_ADDR_LINKLOCAL(&((PSOCKADDR_IN6)pSockaddr)->sin6_addr))
        {
            // scope and scope id for the link
            dwScopeId = dwAdapterIndex;
            dwScope = 1;
	            
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
	    
            pMulticastAddr = (PSOCKADDR)&MulticastSiteScopeAddressIPv6;
        }
        else
            // scope not supported by UPnP
            return;
            
        Assert(dwScope != 0);
        Assert(dwScopeId != 0);
        
        // check if we already have address with the same scope and scope id
        for(ce::vector<SSDP_SEARCH_NETWORK>::iterator it = Networks.begin(), itEnd = Networks.end(); it != itEnd; ++it)
	        if(it->dwScopeId == dwScopeId && it->dwScope == dwScope)
	            return;
    }
    else
        return; // unknown address family

    SSDP_SEARCH_NETWORK network;
    
    if(SocketOpen(&network.socket, (PSOCKADDR)pSockaddr, dwAdapterIndex, NULL))
    {
        WCHAR lpwszAddress[MAX_ADDRESS_SIZE];
        DWORD dwSize;
        
        network.dwIndex = dwAdapterIndex;
        network.dwScopeId = dwScopeId;
        network.dwScope = dwScope;
        network.pMulticastAddr = pMulticastAddr;
        
        // store multicast address in string form
        WSAAddressToString((LPSOCKADDR)pMulticastAddr, sizeof(SOCKADDR_STORAGE), NULL, lpwszAddress, &(dwSize = sizeof(lpwszAddress)));
        ce::WideCharToMultiByte(CP_ACP, lpwszAddress, -1, &network.strMulticastAddr);
        
        Networks.push_back(network);
    }
}


// OpenSearchSockets
VOID OpenSearchSockets(PSSDP_SEARCH_REQUEST pSearchRequest)
{
    PIP_ADAPTER_ADDRESSES	pAdapterAddresses = NULL;  // buffer used by GetAdaptersAddresses()
    ULONG					ulBufSize = 0;    // size of buffer returned by GetAdaptersAddresses()
    DWORD					dwRes  = 0;   // result codes from iphelper apis
	
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
                        
                    // Loop through all the addresses returned
                    for(PIP_ADAPTER_UNICAST_ADDRESS pUnicastAddress = pAdapterIter->FirstUnicastAddress;
                        pUnicastAddress != NULL;
                        pUnicastAddress = pUnicastAddress->Next)
                    {
                        SOCKADDR_STORAGE sockaddr;
                        
                        memcpy(&sockaddr, pUnicastAddress->Address.lpSockaddr, pUnicastAddress->Address.iSockaddrLength);

                        if(sockaddr.ss_family == AF_INET)
                            AddNetwork(pSearchRequest->Networks, &sockaddr, pAdapterIter->IfIndex);
                        else
                            AddNetwork(pSearchRequest->Networks, &sockaddr, pAdapterIter->Ipv6IfIndex);
                    }
                }
            }
            
            // Tidy up
            free (pAdapterAddresses);
        }
    }
}


// CloseSearchSockets
VOID CloseSearchSockets(PSSDP_SEARCH_REQUEST pSearchRequest)
{
    for(ce::vector<SSDP_SEARCH_NETWORK>::iterator it = pSearchRequest->Networks.begin(), itEnd = pSearchRequest->Networks.end(); it != itEnd; ++it)
        SocketClose(it->socket);
}


// SendSearchRequest
VOID SendSearchRequest(PSSDP_SEARCH_REQUEST pSearchRequest)
{
    for(ce::vector<SSDP_SEARCH_NETWORK>::iterator it = pSearchRequest->Networks.begin(), itEnd = pSearchRequest->Networks.end(); it != itEnd; ++it)
    {
        SSDP_REQUEST SsdpRequest;
        SSDP_HEADER SearchHeaders[] = {SSDP_HOST, SSDP_MAN, SSDP_MX, SSDP_ST};
        
        memset(&SsdpRequest, 0, sizeof(SsdpRequest));
        
        SsdpRequest.Method = SSDP_M_SEARCH;
        SsdpRequest.RequestUri = MulticastUri;
    
        SsdpRequest.Headers[SSDP_ST] = pSearchRequest->szType;
        SsdpRequest.Headers[SSDP_MAN] = SearchHeader;
        SsdpRequest.Headers[SSDP_MX] = MX;
        SsdpRequest.Headers[SSDP_HOST] = const_cast<LPSTR>(static_cast<LPCSTR>(it->strMulticastAddr));
        
        Assert(pSearchRequest->szSearch == NULL);
        
        ComposeSsdpRequest(&SsdpRequest, SearchHeaders, sizeof(SearchHeaders)/sizeof(*SearchHeaders), &pSearchRequest->szSearch);
        
        SocketSend(pSearchRequest->szSearch, it->socket, it->pMulticastAddr);
        
        free(pSearchRequest->szSearch);
        pSearchRequest->szSearch = NULL;
    }
}


HANDLE WINAPI FindServicesCallback (const char* szType,
                                    VOID * pReserved ,
                                    BOOL fForceSearch,
                                    SERVICE_CALLBACK_FUNC fnCallback,
                                    VOID *Context)
{
    PSSDP_SEARCH_REQUEST SearchRequest = NULL;

    DWORD ThreadId;
    HANDLE hThread;

    if (szType == NULL || fnCallback == NULL || pReserved != NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    SearchRequest = CreateSearchRequest(szType);

    if (SearchRequest == NULL)
    {
        TraceTag(ttidSsdpSearch, "Couldn't allocate  memory SearchRequest for %s", szType);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }

    SearchRequest->Callback = fnCallback;
    SearchRequest->Context = Context;

    // To-Do: Find master
    SearchRequest->state = SEARCH_DISCOVERING;

    // GetCacheResult(SearchRequest);

    // Check ForceSearch
    if (fForceSearch == TRUE 
    	//|| IsInListNotify(SearchRequest->szType) == FALSE
    	)
    {
        SOCKADDR_IN sockaddrLocal;

        sockaddrLocal.sin_family = AF_INET;
        sockaddrLocal.sin_addr.s_addr = INADDR_ANY;
        sockaddrLocal.sin_port = 0;

        SearchRequest->HitWire = TRUE;

        SocketOpen(&SearchRequest->WakeupSocket, (PSOCKADDR)&sockaddrLocal, 0, NULL);
        OpenSearchSockets(SearchRequest);

        SendSearchRequest(SearchRequest);

        SearchRequest->dwTimerCookie = SearchRequest->ThreadPool.StartTimer(SearchRequestTimerProc, (VOID *) SearchRequest, MX_VALUE);

        if (SearchRequest->dwTimerCookie == 0)
        {
            TraceTag(ttidSsdpSearch, "Failed to start search request timer.");
            
            SocketClose(SearchRequest->WakeupSocket);
            CloseSearchSockets(SearchRequest);

            FreeSearchRequest(SearchRequest);
            SetLastError(ERROR_TIMER_START_FAILED);
            return INVALID_HANDLE_VALUE;
        }
        else
        {
            TraceTag(ttidSsdpSearchResp, "Started search request timer.");
        }

        InterlockedIncrement(&SearchRequest->nRefCount);
        
        hThread = CreateThread(NULL, 0, DoSsdpSearchThread, (LPVOID) SearchRequest,
                               0, &ThreadId);

        // Get return of the thread.

        SearchRequest->hThread = (HANDLE) hThread;

        if ((unsigned long) hThread == 0 || (unsigned long) hThread == -1)
        {
            TraceTag(ttidSsdpNotify, "Falied to create thread. Error:%d",
                     GetLastError());
            
            InterlockedDecrement(&SearchRequest->nRefCount);

            FreeSearchRequest(SearchRequest);

            return INVALID_HANDLE_VALUE;
        }
        else
        {
            TraceTag(ttidSsdpNotify, "Created thread %d", ThreadId);
        }
    }
    else
    {
        ASSERT(SearchRequest->HitWire == FALSE);
        SetEvent(SearchRequest->DoneEvent);
        SearchRequest->Callback(SSDP_DONE, NULL, SearchRequest->Context);
    }
    return SearchRequest;
}

VOID DoCancelCallback(PSSDP_SEARCH_REQUEST SearchRequest)
{

    if (1 == InterlockedIncrement(&SearchRequest->fExit))
	{

		StopSearchRequestTimer(SearchRequest);

		WakeupSelect(SearchRequest);

	}
}

BOOL WINAPI FindServicesClose(HANDLE SearchHandle)
{
    PSSDP_SEARCH_REQUEST SearchRequest = (PSSDP_SEARCH_REQUEST) SearchHandle;
    BOOL fResult;

    fResult = TRUE;

    _try
    {
        // Import to get lock here to guard against SsdpCleanup.
        EnterCriticalSection(&SearchRequest->cs);
        if (SearchRequest->Type != SSDP_CLIENT_SEARCH_SIGNATURE ||
            SearchRequest->Size != sizeof(SSDP_SEARCH_REQUEST) ||
            SearchRequest->state == SEARCH_COMPLETED)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            LeaveCriticalSection(&SearchRequest->cs);
            return FALSE;
        }
        else
        {
            SearchRequest->state = SEARCH_COMPLETED;
            LeaveCriticalSection(&SearchRequest->cs);
            if (SearchRequest->Callback != NULL)
            {
                DoCancelCallback(SearchRequest);

                if(InterlockedExchange(&SearchRequest->bCallbackInProgress, SearchRequest->bCallbackInProgress) == 0)
                    // if there is no callback in progress we can wait for the thread to finish
                    WaitForSingleObject(SearchRequest->hThread, INFINITE);
                else
                    // if a callback is in progress we can't wait infinitly because the callback might never return
                    // if/when it returns we will clean up
                    WaitForSingleObject(SearchRequest->hThread, 500);

                CloseHandle(SearchRequest->hThread);
            }
            
            FreeSearchRequest(SearchRequest);
        }
    }
    _except (1)
    {
        unsigned long ExceptionCode = _exception_code();
        TraceTag(ttidSsdpSearch, "Exception 0x%lx = %ld occurred in "
                 "FindServicesClose %x", ExceptionCode, ExceptionCode, SearchHandle);
        LeaveCriticalSection(&SearchRequest->cs);
        fResult = FALSE;
    }

    return fResult;
}

HANDLE WINAPI FindServices (const char* szType, VOID *pReserved , BOOL fForceSearch)
{
    DWORD ReturnValue;
    PSSDP_SEARCH_REQUEST SearchRequest;

    if (szType == NULL || pReserved != NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
    SearchRequest = CreateSearchRequest(szType);

    if (SearchRequest == NULL)
    {
        TraceTag(ttidSsdpSearch, "Couldn't allocate  memory SearchRequest "
                 "for %s", szType);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return INVALID_HANDLE_VALUE;
    }

    // To-Do: Find master
    SearchRequest->state = SEARCH_DISCOVERING;

    // GetCacheResult(SearchRequest);

    // Check ForceSearch
    if (fForceSearch == TRUE 
    	//|| IsInListNotify(SearchRequest->szType) == FALSE
    	)
    {
        SOCKADDR_IN sockaddrLocal;

        sockaddrLocal.sin_family = AF_INET;
        sockaddrLocal.sin_addr.s_addr = INADDR_ANY;
        sockaddrLocal.sin_port = 0;

        SearchRequest->HitWire = TRUE;

        SocketOpen(&SearchRequest->WakeupSocket, (PSOCKADDR)&sockaddrLocal, 0, NULL);
        OpenSearchSockets(SearchRequest);

        SendSearchRequest(SearchRequest);

        SearchRequest->dwTimerCookie = SearchRequest->ThreadPool.StartTimer(SearchRequestTimerProc, (VOID *) SearchRequest, MX_VALUE);
		
        if (SearchRequest->dwTimerCookie == 0)
        {
            TraceTag(ttidSsdpSearch, "Failed to start search request timer.");
            
            SocketClose(SearchRequest->WakeupSocket);
            CloseSearchSockets(SearchRequest);

            FreeSearchRequest(SearchRequest);
            SetLastError(ERROR_TIMER_START_FAILED);
            return INVALID_HANDLE_VALUE;
        }
        else
        {
            TraceTag(ttidSsdpSearchResp, "Started search request timer.");
        }

        ReturnValue = DoSsdpSearch(SearchRequest);

        if (ReturnValue != 0)
        {
            FindServicesClose(SearchRequest);
            return INVALID_HANDLE_VALUE;
        }
    }

    if (IsListEmpty(&SearchRequest->ListResponses))
    {
        FindServicesClose(SearchRequest);
        SetLastError(ERROR_NO_MORE_SERVICES);
        return INVALID_HANDLE_VALUE;
    }
    else
    {
        return SearchRequest;
    }
}

BOOL WINAPI GetFirstService (HANDLE hFindServices, PSSDP_MESSAGE *ppSsdpService)
{
    PSSDP_SEARCH_REQUEST SearchRequest = (PSSDP_SEARCH_REQUEST) hFindServices;
    PLIST_ENTRY pListHead;

    *ppSsdpService = NULL;

    _try
    {
        if (SearchRequest->Type != SSDP_CLIENT_SEARCH_SIGNATURE ||
            SearchRequest->Size != sizeof(SSDP_SEARCH_REQUEST))
        {
            return FALSE;
        }

        pListHead = &SearchRequest->ListResponses;
        SearchRequest->CurrentResponse = pListHead->Flink;

        if (SearchRequest->CurrentResponse != pListHead)
        {
            PSSDP_MESSAGE_ITEM pMessageItem;

            pMessageItem = CONTAINING_RECORD (SearchRequest->CurrentResponse,
                                              SSDP_MESSAGE_ITEM , linkage);
            *ppSsdpService = pMessageItem->pSsdpMessage;
            SearchRequest->CurrentResponse = SearchRequest->CurrentResponse->Flink;
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    _except (1)
    {
        unsigned long ExceptionCode = _exception_code();
        TraceTag(ttidSsdpSearch, "Exception 0x%lx = %ld occurred in "
                 "GetFirstService %x", ExceptionCode,
                 ExceptionCode, hFindServices);
        return FALSE;
    }
}

BOOL WINAPI GetNextService (HANDLE hFindServices, PSSDP_MESSAGE *ppSsdpService)
{
    PSSDP_SEARCH_REQUEST SearchRequest = (PSSDP_SEARCH_REQUEST) hFindServices;
    PLIST_ENTRY pListHead;

    _try
    {
        if (SearchRequest->Type != SSDP_CLIENT_SEARCH_SIGNATURE ||
            SearchRequest->Size != sizeof(SSDP_SEARCH_REQUEST))
        {
            return FALSE;
        }

        pListHead = &SearchRequest->ListResponses;

        if (SearchRequest->CurrentResponse != pListHead)
        {
            PSSDP_MESSAGE_ITEM pMessageItem;

            pMessageItem = CONTAINING_RECORD (SearchRequest->CurrentResponse,
                                              SSDP_MESSAGE_ITEM , linkage);
            *ppSsdpService = pMessageItem->pSsdpMessage;
            SearchRequest->CurrentResponse = SearchRequest->CurrentResponse->Flink;
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    _except (1)
    {
        unsigned long ExceptionCode = _exception_code();
        TraceTag(ttidSsdpSearch, "Exception 0x%lx = %ld occurred in "
                 "GetNextService %x", ExceptionCode, ExceptionCode, hFindServices);
        return FALSE;
    }
}

VOID FreeSearchRequest(PSSDP_SEARCH_REQUEST SearchRequest)
{
    if(InterlockedDecrement(&SearchRequest->nRefCount) == 0)
    {
        PLIST_ENTRY pListHead = &SearchRequest->ListResponses;
        PLIST_ENTRY p = pListHead->Flink;

        EnterCriticalSection(&SearchRequest->cs);

        while (p != pListHead)
        {
            PSSDP_MESSAGE_ITEM pMessageItem;

            pMessageItem = CONTAINING_RECORD (p, SSDP_MESSAGE_ITEM , linkage);

            p = p->Flink;

            RemoveEntryList(&pMessageItem->linkage);
            FreeSsdpMessage(pMessageItem->pSsdpMessage);
            free(pMessageItem);
        }

        if(SearchRequest->DoneEvent)
            CloseHandle(SearchRequest->DoneEvent);
    
        if(SearchRequest->szType)
            free(SearchRequest->szType);

        if(SearchRequest->szSearch)
            free(SearchRequest->szSearch);

        LeaveCriticalSection(&SearchRequest->cs);

        DeleteCriticalSection(&SearchRequest->cs);
    
        delete SearchRequest;
    }
}

BOOL AddRequestToListResponse(PLIST_ENTRY ListResponse, const SSDP_MESSAGE *pSsdpMessageIn)
{
    PSSDP_MESSAGE_ITEM pMessageItem;
    PSSDP_MESSAGE pSsdpMessage;

    pMessageItem = (PSSDP_MESSAGE_ITEM) malloc(sizeof(SSDP_MESSAGE_ITEM));
    if (pMessageItem == NULL)
    {
        return FALSE;
    }

    if (pSsdpMessage = CopySsdpMessage(pSsdpMessageIn) )
    {
        pMessageItem->pSsdpMessage = pSsdpMessage;
        InsertHeadList(ListResponse, &(pMessageItem->linkage));
        return TRUE;
    }
    else
    {
        TraceTag(ttidSsdpNotify, "Failed to allocate memory for SsdpMessage.");
        free(pMessageItem);
        return FALSE;
    }
}


BOOL IsInTheListResponse(PSSDP_SEARCH_REQUEST SearchRequest, CHAR* szUSN)
{
    PLIST_ENTRY p;
    PLIST_ENTRY pListHead;

    EnterCriticalSection(&SearchRequest->cs);
    pListHead = &SearchRequest->ListResponses;
    p = pListHead->Flink;

    while (p != pListHead)
    {
        PSSDP_MESSAGE_ITEM pMessageItem;

        pMessageItem = CONTAINING_RECORD (p, SSDP_MESSAGE_ITEM , linkage);

        if (strcmp(pMessageItem->pSsdpMessage->szUSN, szUSN) == 0)
        {
            LeaveCriticalSection(&SearchRequest->cs);
            return TRUE;
        }
        p = p->Flink;
    }
    LeaveCriticalSection(&SearchRequest->cs);
    return FALSE;
}

BOOL CallbackOnSearchResult(PSSDP_SEARCH_REQUEST SearchRequest,
                            PSSDP_MESSAGE pSsdpMessage)
{
    EnterCriticalSection(&SearchRequest->cs);
    
    if(SearchRequest->state != SEARCH_COMPLETED)
    {
        InterlockedIncrement(&SearchRequest->bCallbackInProgress);

        LeaveCriticalSection(&SearchRequest->cs);

        SearchRequest->Callback(SSDP_FOUND, pSsdpMessage ,SearchRequest->Context);
        InterlockedDecrement(&SearchRequest->bCallbackInProgress); 
    }
    else
        LeaveCriticalSection(&SearchRequest->cs);

    return TRUE;
}

static void
SearchCallback(
	SSDP_CALLBACK_TYPE CallbackType,	// ignored
    CONST SSDP_MESSAGE *pSsdpMessage,
    void *pContext)
{
	PSSDP_SEARCH_REQUEST SearchRequest = (PSSDP_SEARCH_REQUEST)pContext;
	
	EnterCriticalSection(&SearchRequest->cs);
    AddRequestToListResponse(&SearchRequest->ListResponses, pSsdpMessage);
    LeaveCriticalSection(&SearchRequest->cs);
}

/*BOOL GetCacheResult(PSSDP_SEARCH_REQUEST SearchRequest)
{

    // No lock necessary as szType is read only after we return handle and won't be
    // freed until this thread exits.
    // And we have not returned the call yet.

    LookupCache(SearchRequest->szType, SearchCallback, SearchRequest);
    

    if (SearchRequest->Callback != NULL)
    {
        PLIST_ENTRY p;
        PLIST_ENTRY pListHead = &SearchRequest->ListResponses;

        p = pListHead->Flink;

        while (p != pListHead)
        {
            PSSDP_MESSAGE_ITEM pMessageItem;

            pMessageItem = CONTAINING_RECORD (p, SSDP_MESSAGE_ITEM , linkage);
            CallbackOnSearchResult(SearchRequest, pMessageItem->pSsdpMessage);
            p = p->Flink;
        }

        // Update cache from here
        // SearchRequest->CurrentResponse = SearchRequest->ListResponses.Blink;
    }

    return TRUE;
}*/


DWORD WINAPI DoSsdpSearchThread(LPVOID lpvThreadParam)
{
    HINSTANCE hLib = LoadLibrary(L"upnpctrl.dll");
	
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    PSSDP_SEARCH_REQUEST SearchRequest = (PSSDP_SEARCH_REQUEST) lpvThreadParam;
    
    DWORD dwRet = DoSsdpSearch(SearchRequest);

    FreeSearchRequest(SearchRequest);

    CoUninitialize();
    
    FreeLibraryAndExitThread(hLib, dwRet);

    return dwRet;
}


DWORD WINAPI DoSsdpSearch(PSSDP_SEARCH_REQUEST SearchRequest)
{
    // Get cache content put it in the list response.

    fd_set ReadSocketSet;
    struct timeval SelectTimeOut;
    INT nRet;

    // In case the loopback packet failed, this is our last resort.
    SelectTimeOut.tv_sec = SELECT_TIMEOUT;
    SelectTimeOut.tv_usec = 0;

    // It is OK to check fExit with CSListSearch, as the memory will not be
    // freed until this thread exits.
    while (InterlockedExchange(&SearchRequest->fExit, SearchRequest->fExit) == 0)
    {
        CHAR *RecvBuf = NULL;
        SOCKADDR_STORAGE RemoteSocket;
        SSDP_REQUEST SsdpResponse;
        INT SocketSize = sizeof(RemoteSocket);
        unsigned int i;

        FD_ZERO(&ReadSocketSet);

        EnterCriticalSection(&SearchRequest->cs);

        FD_SET(SearchRequest->WakeupSocket, &ReadSocketSet);
        
        for(ce::vector<SSDP_SEARCH_NETWORK>::iterator it = SearchRequest->Networks.begin(), itEnd = SearchRequest->Networks.end(); it != itEnd; ++it)
            FD_SET(it->socket, &ReadSocketSet);

        LeaveCriticalSection(&SearchRequest->cs);

        nRet = select(-1, &ReadSocketSet, NULL, NULL, &SelectTimeOut);

        if (nRet == SOCKET_ERROR)
        {
            TraceTag(ttidSsdpSearch, "select failed with error %d",
                     GetLastError());
            break;
        }

        if (nRet == 0)
        {
            TraceTag(ttidSsdpSearch, "!!! select timed out !!!, where is "
                     "loopback packet? ");
            break;
        }

        for (i = 0; i < ReadSocketSet.fd_count; i++)
	    {
            if (TRUE == SocketReceive(ReadSocketSet.fd_array[i], &RecvBuf, &RemoteSocket))
            {
                InitializeSsdpRequest(&SsdpResponse);

                if (ParseSsdpResponse(RecvBuf, &SsdpResponse) == TRUE)
                {
                    DWORD dw;
        			
			        if(SsdpResponse.Headers[SSDP_LOCATION])
			        {
			            DWORD dwIndex = 0;
			            
			            // find the network on which the response arrived
			            for(ce::vector<SSDP_SEARCH_NETWORK>::iterator it = SearchRequest->Networks.begin(), itEnd = SearchRequest->Networks.end(); it != itEnd; ++it)
			                if(it->socket == ReadSocketSet.fd_array[i])
			                {
			                    dwIndex = it->dwIndex;
			                    break;
			                }
			                
			            Assert(dwIndex != 0);
			            
			            if(!FixURLAddressScopeId(SsdpResponse.Headers[SSDP_LOCATION], dwIndex, NULL, &(dw = 0)) && dw != 0)
                            if(char* pszLocation = (char*)SsdpAlloc(dw))
                                if(FixURLAddressScopeId(SsdpResponse.Headers[SSDP_LOCATION], dwIndex, pszLocation, &dw))
                                {
                                    SsdpFree(SsdpResponse.Headers[SSDP_LOCATION]);
                                    SsdpResponse.Headers[SSDP_LOCATION] = pszLocation;
                                }
                                else
                                    SsdpFree(pszLocation);
                    }
                    
                    // Check duplicate
                    PSSDP_MESSAGE pSsdpMessage;
                    PSSDP_MESSAGE_ITEM pMessageItem;

                    if (!IsInTheListResponse(SearchRequest,
                                             SsdpResponse.Headers[SSDP_USN]))
                    {
                        pMessageItem = (PSSDP_MESSAGE_ITEM) malloc(sizeof(SSDP_MESSAGE_ITEM));
                        if (pMessageItem == NULL)
                        {
                            TraceTag(ttidSsdpSearch, "Error in recvfrom. Code (%d)",
                                     WSAGetLastError());
                        }
                        else
                        {

                            if (pSsdpMessage = InitializeSsdpMessageFromRequest(&SsdpResponse) )
                            {
                                pMessageItem->pSsdpMessage = pSsdpMessage;

                                if (SearchRequest->Callback != NULL)
                                {
                                    CallbackOnSearchResult(SearchRequest,
                                                           pSsdpMessage);
                                }

                                EnterCriticalSection(&SearchRequest->cs);
                                InsertHeadList(&SearchRequest->ListResponses,
                                               &(pMessageItem->linkage));
                                LeaveCriticalSection(&SearchRequest->cs);
                		        // Update the server cache
                
                    	        //UpdateCache(pSsdpMessage);
                            }
                            else
                            {
                                free(pMessageItem);
                            }
                        }

                
                    }

                    FreeSsdpRequest(&SsdpResponse);
                }

                free(RecvBuf);
            }
        }
    }

    EnterCriticalSection(&SearchRequest->cs);
    
    SocketClose(SearchRequest->WakeupSocket);
    CloseSearchSockets(SearchRequest);

    if (SearchRequest->Callback != NULL && SearchRequest->state != SEARCH_COMPLETED)
    {
        InterlockedIncrement(&SearchRequest->bCallbackInProgress); 

        LeaveCriticalSection(&SearchRequest->cs);

        SearchRequest->Callback(SSDP_DONE, NULL, SearchRequest->Context);

        InterlockedDecrement(&SearchRequest->bCallbackInProgress); 
    }
    else
        LeaveCriticalSection(&SearchRequest->cs);

    return 0;
}

DWORD WINAPI SearchRequestTimerProc (VOID *Arg)
{
    PSSDP_SEARCH_REQUEST SearchRequest = (PSSDP_SEARCH_REQUEST) Arg;

    TraceTag(ttidSsdpSearch, "Entering Critical Section %x", CSListSearch);
    EnterCriticalSection(&SearchRequest->cs);

    TraceTag(ttidSsdpSearch, "Search request timer of %x expired with count = %d",
             SearchRequest,SearchRequest->NumOfRetry);

    if (InterlockedExchange(&SearchRequest->fExit, SearchRequest->fExit) == 1)
    {
        SetEvent(SearchRequest->DoneEvent);
        LeaveCriticalSection(&SearchRequest->cs);
        return 0;
    }

    _try
    {
        if (SearchRequest->NumOfRetry == 0)
        {
            TraceTag(ttidSsdpSearchResp, "The last search request timed out"
                     ".%x ", SearchRequest);
            InterlockedIncrement(&SearchRequest->fExit);
            WakeupSelect(SearchRequest);
            SetEvent(SearchRequest->DoneEvent);
        }
        else
        {
            SendSearchRequest(SearchRequest);

            SearchRequest->NumOfRetry--;

            SearchRequest->dwTimerCookie = SearchRequest->ThreadPool.StartTimer(SearchRequestTimerProc, (VOID *) SearchRequest, MX_VALUE);

			if (SearchRequest->dwTimerCookie == 0)
            {
                TraceTag(ttidSsdpSearchResp, "Failed to start search resp. timer.");
                InterlockedIncrement(&SearchRequest->fExit);
                WakeupSelect(SearchRequest);
                SetEvent(SearchRequest->DoneEvent);
            }
            else
            {
                TraceTag(ttidSsdpSearchResp, "Started search request timer.");
            }
        }
    }
    _finally
    {
        TraceTag(ttidSsdpSearchResp, "Leaving Critical Section %x", CSListSearch);

        LeaveCriticalSection(&SearchRequest->cs);
    }

	return 0;
}

VOID StopSearchRequestTimer(PSSDP_SEARCH_REQUEST SearchRequest)
{
    EnterCriticalSection(&SearchRequest->cs);

    // CSListSearch->fExit should be 1

    TraceTag(ttidSsdpAnnounce, "Stopping Search Request timer for %x",
             SearchRequest);

    if (SearchRequest->ThreadPool.StopTimer(SearchRequest->dwTimerCookie))
    {
        TraceTag(ttidSsdpAnnounce, "Search request timer stopped for service "
                 "%x", SearchRequest);
        LeaveCriticalSection(&SearchRequest->cs);
    }
    else
    {
        // Assume CTEStartTimer returned before FindServiceCallback returned.
        // Timer is running, wait for DoneEvent
        TraceTag(ttidSsdpAnnounce, "Search request timer is running, wait "
                 "...%x", SearchRequest);
        LeaveCriticalSection(&SearchRequest->cs);
        WaitForSingleObject(SearchRequest->DoneEvent, INFINITE);
    }
}

VOID WakeupSelect(PSSDP_SEARCH_REQUEST SearchRequest)
{
    SOCKADDR_IN  SockAddr;
    INT AddrSize = sizeof( SockAddr);

    ASSERT(InterlockedExchange(&SearchRequest->fExit, SearchRequest->fExit) == 1);

    if ( getsockname(SearchRequest->WakeupSocket,
                     (struct sockaddr *) &SockAddr,
                     &AddrSize) != SOCKET_ERROR)
    {
        SockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        TraceTag(ttidSsdpSearch, "Sending to 127.0.0.1:%d",
                 ntohs(SockAddr.sin_port));
        // Will select get this if the socket is not bound to ADDR_ANY?
        SocketSend("Q", SearchRequest->WakeupSocket, (PSOCKADDR)&SockAddr);
    }
    else
    {
        TraceTag(ttidSsdpSearch, "Failed to send loopback packet. (%d)",
                 GetLastError());
        // select will eventually timeout, just slower.
    }
}

