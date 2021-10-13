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

// Add 2 more seconds into search delay time to pass DLNA test.
#define MX_VALUE 3000 + 2000
#define SELECT_TIMEOUT 60

DWORD WINAPI DoSsdpSearch(PSSDP_SEARCH_REQUEST SearchRequest);
DWORD WINAPI DoSsdpSearchThread(LPVOID lpvThreadParam);
//BOOL GetCacheResult(PSSDP_SEARCH_REQUEST SearchRequest);
VOID FreeSearchRequest(PSSDP_SEARCH_REQUEST SearchRequest);
VOID DoCancelCallback(PSSDP_SEARCH_REQUEST SearchRequest);
DWORD WINAPI SearchRequestTimerProc (VOID *Arg);
VOID StopSearchRequestTimer(PSSDP_SEARCH_REQUEST SearchRequest);
VOID WakeupSelect(PSSDP_SEARCH_REQUEST SearchRequest);
VOID OpenSearchSockets(PSSDP_SEARCH_REQUEST pSearchRequest);

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
        TraceTag(ttidError, "Couldn't allocate  memory SearchRequest "
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
    SearchRequest->dwNumberOfResponses = 0;
    SearchRequest->WakeupSocket = INVALID_SOCKET;
    SearchRequest->HasSockets = false;

    InitializeCriticalSection(&SearchRequest->cs);
    InitializeListHead(&SearchRequest->ListResponses);
    SearchRequest->CurrentResponse = SearchRequest->ListResponses.Flink;

    SearchRequest->szType = (CHAR *) malloc(strlen(szType)+1);
    if (SearchRequest->szType == NULL)
    {
        TraceTag(ttidError, "Couldn't allocate  memory for szType for %s", szType);
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

    SOCKADDR_IN sockaddrLocal;
    sockaddrLocal.sin_family = AF_INET;
    sockaddrLocal.sin_addr.s_addr = INADDR_ANY;
    sockaddrLocal.sin_port = 0;
    if (!SocketOpen(&SearchRequest->WakeupSocket, (PSOCKADDR)&sockaddrLocal, 0, NULL))
    {
        FreeSearchRequest(SearchRequest);
        return NULL;
    }

    OpenSearchSockets(SearchRequest);

    return SearchRequest;
}


// AddNetwork
void AddNetwork(ce::vector<SSDP_SEARCH_NETWORK>& Networks, PSOCKADDR_STORAGE pSockaddr, int iSockaddrLength, DWORD dwAdapterIndex)
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
            {
                return;
            }
            
            // scope and scope id for the site
            dwScopeId = ((PSOCKADDR_IN6)pSockaddr)->sin6_scope_id;
            dwScope = 2;
        
            pMulticastAddr = (PSOCKADDR)&MulticastSiteScopeAddressIPv6;
        }
        else
        {
            // scope not supported by UPnP
            return;
        }
            
        Assert(dwScope != 0);
        Assert(dwScopeId != 0);
        
        // check if we already have address with the same scope and scope id
        for(ce::vector<SSDP_SEARCH_NETWORK>::iterator it = Networks.begin(), itEnd = Networks.end(); it != itEnd; ++it)
        {
            if(it->dwScopeId == dwScopeId && it->dwScope == dwScope)
            {
                return;
            }
        }
    }
    else
    {
        return; // unknown address family
    }

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
    PIP_ADAPTER_ADDRESSES   pAdapterAddresses = NULL;  // buffer used by GetAdaptersAddresses()
    ULONG                   ulBufSize = 0;    // size of buffer returned by GetAdaptersAddresses()
    DWORD                   dwRes  = 0;   // result codes from iphelper apis
    
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
                    {
                        continue;
                    }
                    
                    if(!upnp_config::is_adapter_enabled(pAdapterIter->AdapterName))
                    {
                        continue;
                    }
                        
                    // Loop through all the addresses returned
                    for(PIP_ADAPTER_UNICAST_ADDRESS pUnicastAddress = pAdapterIter->FirstUnicastAddress;
                        pUnicastAddress != NULL;
                        pUnicastAddress = pUnicastAddress->Next)
                    {
                        SOCKADDR_STORAGE sockaddr;
                        
                        memcpy(&sockaddr, pUnicastAddress->Address.lpSockaddr, pUnicastAddress->Address.iSockaddrLength);

                        if(sockaddr.ss_family == AF_INET)
                        {
                            AddNetwork(pSearchRequest->Networks, &sockaddr, pUnicastAddress->Address.iSockaddrLength, pAdapterIter->IfIndex);
                        }
                        else
                        {
                            AddNetwork(pSearchRequest->Networks, &sockaddr, pUnicastAddress->Address.iSockaddrLength, pAdapterIter->Ipv6IfIndex);
                        }
                    }
                }
            }
            
            // Tidy up
            free (pAdapterAddresses);
        }
    }

    pSearchRequest->HasSockets = true;
}


// CloseSearchSockets
VOID CloseSearchSockets(PSSDP_SEARCH_REQUEST pSearchRequest)
{
    if (!pSearchRequest->HasSockets)
    {
        return;
    }
    for(ce::vector<SSDP_SEARCH_NETWORK>::iterator it = pSearchRequest->Networks.begin(), itEnd = pSearchRequest->Networks.end(); it != itEnd; ++it)
    {
        SocketClose(it->socket);
        it->socket = INVALID_SOCKET;
    }
    pSearchRequest->HasSockets = false;
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

        if(pSearchRequest->szSearch)
        {
            free(pSearchRequest->szSearch);
        }

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
        TraceTag(ttidError, "Couldn't allocate  memory SearchRequest for %s", szType);
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
        SearchRequest->HitWire = TRUE;

        SendSearchRequest(SearchRequest);

        SearchRequest->dwTimerCookie = SearchRequest->ThreadPool.StartTimer(SearchRequestTimerProc, (VOID *) SearchRequest, MX_VALUE);

        if (SearchRequest->dwTimerCookie == 0)
        {
            TraceTag(ttidError, "Failed to start search request timer.");
            
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
            TraceTag(ttidError, "Falied to create thread. Error:%d",
                     GetLastError());

            InterlockedDecrement(&SearchRequest->nRefCount);
            FreeSearchRequest(SearchRequest);

            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return INVALID_HANDLE_VALUE;
        }
        else
        {
            TraceTag(ttidSsdpNotify, "Created thread %d", ThreadId);
        }
    }
    else
    {
        ASSERT(FALSE);
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

                WaitForSingleObject(SearchRequest->hThread, INFINITE);

                CloseHandle(SearchRequest->hThread);
            }
            
            FreeSearchRequest(SearchRequest);
        }
    }
    _except (1)
    {
        unsigned long ExceptionCode = _exception_code();
        TraceTag(ttidError, "Exception 0x%lx = %ld occurred in "
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
        TraceTag(ttidError, "Couldn't allocate  memory SearchRequest "
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
        SearchRequest->HitWire = TRUE;

        SendSearchRequest(SearchRequest);

        SearchRequest->dwTimerCookie = SearchRequest->ThreadPool.StartTimer(SearchRequestTimerProc, (VOID *) SearchRequest, MX_VALUE);

        if (SearchRequest->dwTimerCookie == 0)
        {
            TraceTag(ttidError, "Failed to start search request timer.");

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

BOOL WINAPI GetFirstService (HANDLE hFindServices, PSSDP_MESSAGE_ITEM *ppSsdpService)
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
            *ppSsdpService = CONTAINING_RECORD (SearchRequest->CurrentResponse,
                                              SSDP_MESSAGE_ITEM , linkage);
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
        TraceTag(ttidError, "Exception 0x%lx = %ld occurred in "
                 "GetFirstService %x", ExceptionCode,
                 ExceptionCode, hFindServices);
        return FALSE;
    }
}

BOOL WINAPI GetNextService (HANDLE hFindServices, PSSDP_MESSAGE_ITEM *ppSsdpService)
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
            *ppSsdpService = CONTAINING_RECORD (SearchRequest->CurrentResponse,
                                              SSDP_MESSAGE_ITEM , linkage);
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
        TraceTag(ttidError, "Exception 0x%lx = %ld occurred in "
                 "GetNextService %x", ExceptionCode, ExceptionCode, hFindServices);
        return FALSE;
    }
}

VOID FreeSearchRequest(PSSDP_SEARCH_REQUEST SearchRequest)
{
    Assert(SearchRequest);
    long refCnt = InterlockedDecrement(&SearchRequest->nRefCount);
    if(refCnt == 0)
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
            for (int i = 0; i < NUM_OF_URLS; i++)
            {
                if (pMessageItem->rgszLocations[i])
                {
                    free(pMessageItem->rgszLocations[i]);
                    pMessageItem->rgszLocations[i] = NULL;
                }
            }
            free(pMessageItem);
        }

        if (SearchRequest->WakeupSocket != INVALID_SOCKET)
        {
            SocketClose(SearchRequest->WakeupSocket);
            SearchRequest->WakeupSocket = INVALID_SOCKET;
        }

        CloseSearchSockets(SearchRequest);

        if(SearchRequest->DoneEvent)
        {
            CloseHandle(SearchRequest->DoneEvent);
            SearchRequest->DoneEvent = NULL;
        }

        if(SearchRequest->szType)
        {
            free(SearchRequest->szType);
            SearchRequest->szType = NULL;
        }

        if(SearchRequest->szSearch)
        {
            free(SearchRequest->szSearch);
            SearchRequest->szSearch = NULL;
        }

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

    pSsdpMessage = CopySsdpMessage(pSsdpMessageIn);
    if (pSsdpMessage)
    {
        pMessageItem->pSsdpMessage = pSsdpMessage;
        // Move the URL location into the array of locations
        pMessageItem->rgszLocations[0] = pSsdpMessage->szLocHeader;
        pSsdpMessage->szLocHeader = NULL;
        InsertHeadList(ListResponse, &(pMessageItem->linkage));
        return TRUE;
    }
    else
    {
        TraceTag(ttidError, "Failed to allocate memory for SsdpMessage.");
        free(pMessageItem);
        return FALSE;
    }
}


PSSDP_MESSAGE_ITEM FindMessageItemInListResponse(PSSDP_SEARCH_REQUEST SearchRequest, CHAR* szUSN)
{
    PLIST_ENTRY p;
    PLIST_ENTRY pListHead;
    PSSDP_MESSAGE_ITEM pReturn = NULL;

    EnterCriticalSection(&SearchRequest->cs);
    pListHead = &SearchRequest->ListResponses;
    p = pListHead->Flink;

    while (p != pListHead)
    {
        PSSDP_MESSAGE_ITEM pMessageItem = CONTAINING_RECORD (p, SSDP_MESSAGE_ITEM , linkage);
        if (pMessageItem->pSsdpMessage &&  pMessageItem->pSsdpMessage->szUSN)
        {
            if (strcmp(pMessageItem->pSsdpMessage->szUSN, szUSN) == 0)
            {
                pReturn = pMessageItem;
                break;
            }
        }
        p = p->Flink;
    }
    LeaveCriticalSection(&SearchRequest->cs);
    return pReturn;
}

void AddURLToMessageItem(PSSDP_MESSAGE_ITEM pMessageItem, CHAR *szURL)
{
    if (!szURL || !pMessageItem)
    {
        return;
    }

    for (int i = 0; i < _countof(pMessageItem->rgszLocations); i++)
    {
        if (pMessageItem->rgszLocations[i] == NULL)
        {
            int len = sizeof(char) * (strlen(szURL)+1);
            pMessageItem->rgszLocations[i] = (char*)malloc(len);
            if (pMessageItem->rgszLocations[i] != NULL)
            {
                memcpy(pMessageItem->rgszLocations[i], szURL, len);
            }
            break;
        }
        else if (strcmp(pMessageItem->rgszLocations[i], szURL) == 0)
        {
            break;
        }
    }
}


BOOL CallbackOnSearchResult(PSSDP_SEARCH_REQUEST SearchRequest, PSSDP_MESSAGE pSsdpMessage)
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
    SSDP_CALLBACK_TYPE CallbackType,    // ignored
    CONST SSDP_MESSAGE *pSsdpMessage,
    void *pContext)
{
    PSSDP_SEARCH_REQUEST SearchRequest = (PSSDP_SEARCH_REQUEST)pContext;

    EnterCriticalSection(&SearchRequest->cs);
    if(++SearchRequest->dwNumberOfResponses <= upnp_config::max_search_results())
    {
        AddRequestToListResponse(&SearchRequest->ListResponses, pSsdpMessage);
    }
    LeaveCriticalSection(&SearchRequest->cs);
}


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


DWORD WINAPI DoSsdpSearch(PSSDP_SEARCH_REQUEST pSearchRequest)
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
    while (InterlockedExchange(&pSearchRequest->fExit, pSearchRequest->fExit) == 0)
    {
        char *pRecvBuf = NULL;
        SOCKADDR_STORAGE RemoteSocket;
        SSDP_REQUEST SsdpResponse;
        PSSDP_MESSAGE pNewSsdpMessage = NULL;
        INT SocketSize = sizeof(RemoteSocket);
        unsigned int i;

        FD_ZERO(&ReadSocketSet);

        EnterCriticalSection(&pSearchRequest->cs);

        FD_SET(pSearchRequest->WakeupSocket, &ReadSocketSet);

        for(ce::vector<SSDP_SEARCH_NETWORK>::iterator it = pSearchRequest->Networks.begin(), itEnd = pSearchRequest->Networks.end(); it != itEnd; ++it)
            FD_SET(it->socket, &ReadSocketSet);

        LeaveCriticalSection(&pSearchRequest->cs);

        nRet = select(-1, &ReadSocketSet, NULL, NULL, &SelectTimeOut);

        if (nRet == SOCKET_ERROR)
        {
            TraceTag(ttidError, "select failed with error %d",
                     GetLastError());
            break;
        }

        if (nRet == 0)
        {
            TraceTag(ttidError, "!!! select timed out !!!, where is "
                     "loopback packet? ");
            break;
        }

        for (i = 0; i < ReadSocketSet.fd_count; i++)
        {
            // Init variables for standardized cleanup
            DWORD dw = 0;
            pRecvBuf = NULL;
            InitializeSsdpRequest(&SsdpResponse);
            pNewSsdpMessage = NULL;

            // Read and parse what can be read and parsed
            if (!SocketReceive(ReadSocketSet.fd_array[i], &pRecvBuf, &RemoteSocket))
            {
                goto RecvCleanup;
             }
            if (!ParseSsdpResponse(pRecvBuf, &SsdpResponse))
            {
                goto RecvCleanup;
             }
            if(!SsdpResponse.Headers[SSDP_LOCATION] || !SsdpResponse.Headers[SSDP_USN])
            {
                goto RecvCleanup;
             }

            // find the network on which the response arrived
            DWORD dwIndex = 0;
            for(ce::vector<SSDP_SEARCH_NETWORK>::iterator it = pSearchRequest->Networks.begin(), itEnd = pSearchRequest->Networks.end(); it != itEnd; ++it)
            {
                if(it->socket == ReadSocketSet.fd_array[i])
                {
                    dwIndex = it->dwIndex;
                    break;
                }
            }
            if (dwIndex == 0)
            {
                goto RecvCleanup;
            }

            // Address clean up ??
            dw = 0;
            if(!FixURLAddressScopeId(SsdpResponse.Headers[SSDP_LOCATION], dwIndex, NULL, &dw) && dw != 0)
            {
                if(char* pszLocation = (char*)SsdpAlloc(dw))
                {
                    if(FixURLAddressScopeId(SsdpResponse.Headers[SSDP_LOCATION], dwIndex, pszLocation, &dw))
                    {
                        SsdpFree(SsdpResponse.Headers[SSDP_LOCATION]);
                        SsdpResponse.Headers[SSDP_LOCATION] = pszLocation;
                    }
                    else
                    {
                        SsdpFree(pszLocation);
                    }
                }
            }

            // We will probably need an ssdp message.  Make it now.
            pNewSsdpMessage = InitializeSsdpMessageFromRequest(&SsdpResponse);
            if (!pNewSsdpMessage)
            {
                goto RecvCleanup;
            }

            // Callback reporting: just report the SSDP_MESSAGE.
            if (pSearchRequest->Callback != NULL)
            {
                CallbackOnSearchResult(pSearchRequest, pNewSsdpMessage);
            }

            // Collected reporting: USN is present, just add new URL to it.
            PSSDP_MESSAGE_ITEM pOldMessageItem = FindMessageItemInListResponse(
                                      pSearchRequest, SsdpResponse.Headers[SSDP_USN]);
            if (pOldMessageItem)
            {
                AddURLToMessageItem(pOldMessageItem, SsdpResponse.Headers[SSDP_LOCATION]);
                goto RecvCleanup;
            }

            // Collected reporting: USN not present, make & add new MessageItem
            PSSDP_MESSAGE_ITEM pNewMessageItem = (PSSDP_MESSAGE_ITEM) malloc(sizeof(SSDP_MESSAGE_ITEM));
            if (!pNewMessageItem)
            {
                goto RecvCleanup;
            }
            memset(pNewMessageItem, 0, sizeof(*pNewMessageItem));

            // Add in the ssdp message
            pNewMessageItem->pSsdpMessage = pNewSsdpMessage;
            pNewSsdpMessage = NULL;    // ownership transfered

            // Add to list
            EnterCriticalSection(&pSearchRequest->cs);
            InsertHeadList(&pSearchRequest->ListResponses, &(pNewMessageItem->linkage));
            LeaveCriticalSection(&pSearchRequest->cs);

RecvCleanup:
            // End of loop.   Free any allocated but untransfered objects
            if (pRecvBuf)
            {
                free(pRecvBuf);
            }
            FreeSsdpRequest(&SsdpResponse);
            if (pNewSsdpMessage)
            {
                FreeSsdpMessage(pNewSsdpMessage);
            }
        }
    }

    EnterCriticalSection(&pSearchRequest->cs);

    if (pSearchRequest->Callback != NULL && pSearchRequest->state != SEARCH_COMPLETED)
    {
        InterlockedIncrement(&pSearchRequest->bCallbackInProgress); 

        LeaveCriticalSection(&pSearchRequest->cs);

        pSearchRequest->Callback(SSDP_DONE, NULL, pSearchRequest->Context);

        InterlockedDecrement(&pSearchRequest->bCallbackInProgress); 
    }
    else
        LeaveCriticalSection(&pSearchRequest->cs);

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
                TraceTag(ttidError, "Failed to start search resp. timer.");
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
        TraceTag(ttidError, "Failed to send loopback packet. (%d)",
                 GetLastError());
        // select will eventually timeout, just slower.
    }
}

