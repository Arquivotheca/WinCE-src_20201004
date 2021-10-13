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

#include <bldver.h>
#include "ipsupport.h"

#define BUF_SIZE 40

static LIST_ENTRY listAnnounce;
static CRITICAL_SECTION CSListAnnounce;
static CHAR *AliveHeader = "ssdp:alive";
static CHAR *ByebyeHeader = "ssdp:byebye";
static CHAR *MulticastUri = "*";
static const CHAR c_szHttpLocalHost[] = "http://localhost";
static const CHAR c_szServerVersion[] = "Microsoft-WinCE/%d.%d UPnP/1.0 UPnP-Device-Host/1.0";

static SSDP_HEADER AliveHeaders[] = {SSDP_HOST, SSDP_CACHECONTROL, SSDP_LOCATION, SSDP_NT, SSDP_NTS, SSDP_SERVER, SSDP_USN, SSDP_LAST, SSDP_LAST};
static SSDP_HEADER ByeByeHeaders[] = {SSDP_HOST, SSDP_NT, SSDP_NTS, SSDP_USN, SSDP_LAST, SSDP_LAST};
static SSDP_HEADER ResponseHeaders[] = {SSDP_CACHECONTROL, SSDP_EXT, SSDP_LOCATION, SSDP_SERVER, SSDP_ST, SSDP_USN, SSDP_LAST, SSDP_LAST};


extern LONG_PTR bShutdown;
extern CHAR g_pszExtensionURI[500];
extern CHAR g_pszHeaderPrefix[10];
extern CHAR g_lpszNls[100];

SVSThreadPool* g_pThreadPool;

BOOL InitializeSsdpRequestFromMessage(SSDP_REQUEST *pRequest,
                                      const SSDP_MESSAGE *pssdpSrc);
VOID PrintList(LIST_ENTRY *pListHead);
VOID PrintSSDPService (const SSDP_SERVICE *pSSDPService);

VOID InitializeListAnnounce()
{
    InitializeCriticalSection(&CSListAnnounce);
    EnterCriticalSection(&CSListAnnounce);
    InitializeListHead(&listAnnounce);
    LeaveCriticalSection(&CSListAnnounce);
}

BOOL SendAnnouncementOrResponse(
    SSDP_SERVICE *pssdpService,
    SOCKET sock,                    // socket to send response (INVALID_SOCKET if multicast announcement)
    PSOCKADDR_STORAGE pSockAddr)    // destination addr (NULL if mulicast announcement)
{
    PSSDPNetwork pNet = NULL;
	CHAR *pszOrigURL = pssdpService->SsdpRequest.Headers[SSDP_LOCATION];
	CHAR *pszNewURL = NULL, *pszRelativeURL;
	CHAR *pszBytes = NULL;

	Assert(pszOrigURL);
	Assert(!memcmp(pszOrigURL, c_szHttpLocalHost, sizeof(c_szHttpLocalHost)-1));
    Assert(pssdpService->SsdpRequest.Headers[SSDP_NLS] == NULL);
    Assert(pssdpService->SsdpRequest.Headers[SSDP_HOST] == NULL);
    Assert(pssdpService->SsdpRequest.Headers[SSDP_OPT] == NULL);
    
	pszRelativeURL = pszOrigURL + sizeof(c_szHttpLocalHost)-1;
	
	// we have to substitute http://localhost with http://<dottedip>.
	// allocate enough space for that.
	pssdpService->SsdpRequest.Headers[SSDP_LOCATION] = pszNewURL = (CHAR *)malloc(strlen(pszOrigURL) + 1 + MAX_ADDRESS_SIZE);
	
	if (!pszNewURL)
		return FALSE;
	
	// set OPT header
  	if(*g_pszHeaderPrefix && *g_lpszNls)
  	{
  	    if(pssdpService->SsdpRequest.Headers[SSDP_OPT] = (CHAR*)malloc(strlen(g_pszExtensionURI) + strlen(g_pszHeaderPrefix) + sizeof("; ns=") + 1))
  	        sprintf(pssdpService->SsdpRequest.Headers[SSDP_OPT], "%s; ns=%s", g_pszExtensionURI, g_pszHeaderPrefix);
  	}
	
	GetNetworkLock();
	
	while(pNet = GetNextNetwork(pNet))
	{
	    if(sock != INVALID_SOCKET && pNet->socket != sock)
	        continue;
	    
    	// set LOCATION header
    	sprintf(pssdpService->SsdpRequest.Headers[SSDP_LOCATION], "http://%s%s", pNet->pszAddressString, pszRelativeURL);
    	
		// set NLS header
  	    if(*g_lpszNls)
  	        pssdpService->SsdpRequest.Headers[SSDP_NLS] = g_lpszNls;
    		
		if(pSockAddr)
		{
	    	// ResponseHeaders has space for 2 optional headers: OPT and NLS
		    int nHeaders = sizeof(ResponseHeaders)/sizeof(*ResponseHeaders) - 2;
	    	
	    	// add optional OPT header if specified
		    if(pssdpService->SsdpRequest.Headers[SSDP_OPT])
		        ResponseHeaders[nHeaders++] = SSDP_OPT;
		    
		    // add optional NLS header if specified
		    if(pssdpService->SsdpRequest.Headers[SSDP_NLS])
		        ResponseHeaders[nHeaders++] = SSDP_NLS;
    		
	    	// compose response message
	    	ComposeSsdpResponse(&pssdpService->SsdpRequest, ResponseHeaders, nHeaders, &pszBytes);
	    	
	    	// send unicast response
	    	SocketSend(pszBytes, pNet->socket, (PSOCKADDR)pSockAddr);
		}
		else
		{
    		int nHeaders;
    		SSDP_HEADER *pIncludedHeaders;
    		
    		// set HOST header
    	    pssdpService->SsdpRequest.Headers[SSDP_HOST] = pNet->pszMulticastAddr;
		    
		    // set list of headers to be included in the message
		    if(pssdpService->SsdpRequest.Headers[SSDP_NTS] == ByebyeHeader)
		    {
		        pIncludedHeaders = ByeByeHeaders;
		        nHeaders = sizeof(ByeByeHeaders)/sizeof(*ByeByeHeaders);
		    }
		    else
		    {
		        assert(pssdpService->SsdpRequest.Headers[SSDP_NTS] == AliveHeader);
		        
		        pIncludedHeaders = AliveHeaders;
		        nHeaders = sizeof(AliveHeaders)/sizeof(*AliveHeaders);
		    }
		    
		    // AliveHeaders and ByeByeHeaders has space for 2 optional headers: OPT and NLS
		    nHeaders -= 2;
		    
		    // add optional OPT header if specified
		    if(pssdpService->SsdpRequest.Headers[SSDP_OPT])
		        pIncludedHeaders[nHeaders++] = SSDP_OPT;
		    
		    // add optional NLS header if specified
		    if(pssdpService->SsdpRequest.Headers[SSDP_NLS])
		        pIncludedHeaders[nHeaders++] = SSDP_NLS;
		        
		    // compose request message
		    ComposeSsdpRequest(&pssdpService->SsdpRequest, pIncludedHeaders, nHeaders, &pszBytes);
		    
		    // send multicast announcement
		    SocketSend(pszBytes, pNet->socket, pNet->pMulticastAddr);
		}
		
		if (pszBytes)
		{
			free(pszBytes);
			pszBytes = NULL;
		}
	}
	
	FreeNetworkLock();
	
	// restore LOCATION header
	pssdpService->SsdpRequest.Headers[SSDP_LOCATION] = pszOrigURL;
	free (pszNewURL);
	
	// restore NLS header
	pssdpService->SsdpRequest.Headers[SSDP_NLS] = NULL;
	
	// restore HOST header
	pssdpService->SsdpRequest.Headers[SSDP_HOST] = NULL;
	
	// free OPT header
    if(pssdpService->SsdpRequest.Headers[SSDP_OPT])
    {
	    free(pssdpService->SsdpRequest.Headers[SSDP_OPT]);
	    pssdpService->SsdpRequest.Headers[SSDP_OPT] = NULL;
	}
	
	return TRUE;	
}



// Announce all registered services on all active networks
// This is called when there is a network change, for example, 
// an IP address change or a new network card
// This is implemented as a synchronous call, since it is called
// infrequently.
VOID SendAllAnnouncements()
{
    PLIST_ENTRY p;
    PLIST_ENTRY pListHead = &listAnnounce;
    TraceTag(ttidSsdpAnnounce, "SendAllAnnouncements entered");
    EnterCriticalSection(&CSListAnnounce);
    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        SSDP_SERVICE *pService;

        pService = CONTAINING_RECORD (p, SSDP_SERVICE, linkage);

        EnterCriticalSection(&pService->CSService);
        SendAnnouncement(pService);
        LeaveCriticalSection(&pService->CSService);

    }

    TraceTag(ttidSsdpAnnounce, "SendAllAnnouncements done");
    LeaveCriticalSection(&CSListAnnounce);
}

DWORD AnnounceTimerProc (VOID *Arg)
{
    SSDP_SERVICE *pssdpService = (SSDP_SERVICE *) Arg;
    unsigned long Timeout;

    EnterCriticalSection(&(pssdpService->CSService));

    TraceTag(ttidSsdpTimer, "Announcement timer of %x expired with count = %d",
             pssdpService,pssdpService->iRetryCount );

    if (pssdpService->state == SERVICE_NO_MASTER_CLEANUP)
    {
        SetEvent(pssdpService->CleanupEvent);
        LeaveCriticalSection(&(pssdpService->CSService));
        return 0;
    }

	pssdpService->SsdpRequest.Headers[SSDP_NTS] = AliveHeader;
	SendAnnouncement(pssdpService);

    pssdpService->iRetryCount--;

    if (pssdpService->iRetryCount == 0)
    {
        // To-Do: The current limit on life time is 49.7 days.  (32 bits in milliseconds)
        // 32 bit in seconds should be enough.
        // Need to add field remaining time ...

        pssdpService->iRetryCount = NUM_RETRIES;
        Timeout = (pssdpService->iLifeTime - ANNOUNCE_MARGIN) * 1000;

        if(!(pssdpService->dwTimerCookie = g_pThreadPool->StartTimer(AnnounceTimerProc, pssdpService, Timeout)))
        {
            TraceTag(ttidError, "Failed to start cache timer for %x.",
                     pssdpService);
        }
        else
        {
            TraceTag(ttidSsdpTimer, "Started cache timer.");
        }
    }
    else
    {
        Timeout = RETRY_INTERVAL;

        if(!(pssdpService->dwTimerCookie = g_pThreadPool->StartTimer(AnnounceTimerProc, pssdpService, Timeout)))
        {
            TraceTag(ttidError, "Failed to start retry timer.");
        }
        else
        {
            TraceTag(ttidSsdpTimer, "Started retry timer.");
        }
    }

    LeaveCriticalSection(&(pssdpService->CSService));
    
    return 0;
}

DWORD ByebyeTimerProc (VOID *Arg)
{
    SSDP_SERVICE *pssdpService = (SSDP_SERVICE *) Arg;
    unsigned long Timeout;

    if (InterlockedExchange(&bShutdown, bShutdown) != 0)
    {
        FreeSSDPService(pssdpService);
        return 0;
    }

    //GetNetworkLock();

    EnterCriticalSection(&(pssdpService->CSService));

    TraceTag(ttidSsdpTimer, "Byebye timer of %x expired with count = %d",
             pssdpService,pssdpService->iRetryCount );

	pssdpService->SsdpRequest.Headers[SSDP_NTS] = ByebyeHeader;
	SendAnnouncement(pssdpService);

    //FreeNetworkLock();

    pssdpService->iRetryCount--;

    if (pssdpService->iRetryCount == 0)
    {
        TraceTag(ttidSsdpAnnounce, "Done with sending byebyes.");
        LeaveCriticalSection(&(pssdpService->CSService));
        FreeSSDPService(pssdpService);
    }
    else
    {
        Timeout = RETRY_INTERVAL;

        if(!(pssdpService->dwTimerCookie = g_pThreadPool->StartTimer(ByebyeTimerProc, pssdpService, Timeout)))
        {
            TraceTag(ttidError, "Failed to start byebye retry timer.");
        }
        else
        {
            TraceTag(ttidSsdpTimer, "Started byebye retry timer.");
        }
        
        LeaveCriticalSection(&(pssdpService->CSService));
    }
    
    return 0;
}

VOID StartAnnounceTimer(PSSDP_SERVICE pssdpSvc, LPTHREAD_START_ROUTINE pCallback)
{
    EnterCriticalSection(&(pssdpSvc->CSService));

    // Assume send will be called once before start the timer
    pssdpSvc->iRetryCount = NUM_RETRIES-1;
    
    if(!(pssdpSvc->dwTimerCookie = g_pThreadPool->StartTimer(pCallback, pssdpSvc, RETRY_INTERVAL)))
    {
        TraceTag(ttidError, "Announcement timer failed to start "
                 "for service %x", pssdpSvc);
    }
    else
    {
        TraceTag(ttidSsdpTimer, "Announcement timer started for service "
                 "%x", pssdpSvc);
    }

    LeaveCriticalSection(&(pssdpSvc->CSService));
}

VOID StopAnnounceTimer(SSDP_SERVICE *pSSDPSvc)
{
    EnterCriticalSection(&(pSSDPSvc->CSService));

    Assert(pSSDPSvc->state == SERVICE_NO_MASTER_CLEANUP);

    TraceTag(ttidSsdpTimer, "Stopping Announcement timer for service %x", pSSDPSvc);

    if (g_pThreadPool->StopTimer(pSSDPSvc->dwTimerCookie))
    {
        TraceTag(ttidSsdpTimer, "Announcement timer stopped for service %x", pSSDPSvc);
        LeaveCriticalSection(&(pSSDPSvc->CSService));
    }
    else
    {
        // Timer is running, wait for CleanupEvent
        TraceTag(ttidSsdpAnnounce, "Announcement timer is running, wait ...%x", pSSDPSvc);
        LeaveCriticalSection(&(pSSDPSvc->CSService));
        WaitForSingleObject(pSSDPSvc->CleanupEvent, INFINITE);
    }
}


PSSDP_SERVICE AddToListAnnounce(SSDP_MESSAGE *pssdpMsg, DWORD flags, PCONTEXT_HANDLE_TYPE *pphContext)
{
    SSDP_SERVICE *pssdpService;

    // To-Do: Check for duplicates.

    // To-Do: QueryState

    // Create SSDPService from SSDP_MESSAGE

    pssdpService = (SSDP_SERVICE *) malloc (sizeof(SSDP_SERVICE));

    if (pssdpService == NULL)
    {
        return NULL;
    }

    pssdpService->Type = SSDP_SERVICE_SIGNATURE;

    if (InitializeSsdpRequestFromMessage(&(pssdpService->SsdpRequest),
                                         pssdpMsg) == FALSE)
    {
        free(pssdpService);
        return NULL;
    };

    pssdpService->iLifeTime = pssdpMsg->iLifeTime;

    pssdpService->iRetryCount = 0;

    pssdpService->dwTimerCookie = 0;

    pssdpService->CleanupEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (pssdpService->CleanupEvent == NULL)
    {
        free(pssdpService);
        return NULL;
    }

    pssdpService->flags = flags;
    pssdpService->RpcContextHandle = pphContext;




    // Important: Set to NULL to prevent freeing memory.
    pssdpService->SsdpRequest.Headers[SSDP_NTS] = NULL;
   
   	pssdpService->SsdpRequest.RequestUri = MulticastUri; // for notify
   
    // To-Do: Query Per Network State Matrix state
    pssdpService->state = SERVICE_ACTIVE_NO_MASTER;

    InitializeCriticalSection(&(pssdpService->CSService));
    InitializeListHead(&(pssdpService->listSearchResponse));

    // ALWAYS get the list lock before the service lock.

    EnterCriticalSection(&CSListAnnounce);

    // read the LIST_ENTRY
    EnterCriticalSection(&(pssdpService->CSService));
    InsertHeadList(&listAnnounce, &(pssdpService->linkage));
    LeaveCriticalSection(&(pssdpService->CSService));

    //PrintList(&listAnnounce);
    TraceTag(ttidSsdpAnnounce, "New SSDP service announcement:\n");
#ifdef DEBUG
    PrintSSDPService(pssdpService);
#endif    

    LeaveCriticalSection(&CSListAnnounce);

    return pssdpService;
}

VOID FreeSSDPService(SSDP_SERVICE *pSSDPSvc)
{
    //
    // We have to make sure that no thread has any reference to this service at this point
    // Do we need ref count?
    //
	pSSDPSvc->SsdpRequest.Headers[SSDP_NTS] = NULL;	// make sure this header element is not freed.
    pSSDPSvc->SsdpRequest.RequestUri = NULL; 		//make sure this header element is not freed
	
    FreeSsdpRequest(&(pSSDPSvc->SsdpRequest));

    DeleteCriticalSection(&(pSSDPSvc->CSService));

    CloseHandle(pSSDPSvc->CleanupEvent);

    free(pSSDPSvc);
}

VOID RemoveFromListAnnounce(SSDP_SERVICE *pssdpSvc)
{
    EnterCriticalSection(&CSListAnnounce);
    EnterCriticalSection(&pssdpSvc->CSService);
    RemoveEntryList(&(pssdpSvc->linkage));
#ifdef DEBUG    
    PrintSSDPService(pssdpSvc);
#endif    
    LeaveCriticalSection(&pssdpSvc->CSService);
    //PrintList(&listAnnounce);
    LeaveCriticalSection(&CSListAnnounce);
}

BOOL IsInListAnnounce(CHAR *szUSN)
{
    PLIST_ENTRY p;
    LIST_ENTRY *pListHead = &listAnnounce;
    BOOL found = FALSE;

    EnterCriticalSection(&CSListAnnounce);
    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        SSDP_SERVICE *pService;
        pService = CONTAINING_RECORD (p, SSDP_SERVICE, linkage);

        EnterCriticalSection(&(pService->CSService));

        if (strcmp(pService->SsdpRequest.Headers[SSDP_USN], szUSN) == 0)
        {
            found = TRUE;
            LeaveCriticalSection(&(pService->CSService));
            break;
        }
        LeaveCriticalSection(&(pService->CSService));
    }

    LeaveCriticalSection(&CSListAnnounce);
    return found;
}

// It is necessary to hold the list lock while comparing type and put in the PSRT.

VOID SearchListAnnounce(SSDP_REQUEST *SsdpMessage, SOCKET sockRecv, PSOCKADDR_STORAGE pRemoteAddr)
{

    PLIST_ENTRY p;
    LIST_ENTRY *pListHead = &listAnnounce;

    EnterCriticalSection(&CSListAnnounce);
    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        SSDP_SERVICE *pService;
        pService = CONTAINING_RECORD (p, SSDP_SERVICE, linkage);

        EnterCriticalSection(&(pService->CSService));

        if (strcmp(SsdpMessage->Headers[SSDP_ST], "ssdp:all") == 0 ||
            strncmp(SsdpMessage->Headers[SSDP_ST], pService->SsdpRequest.Headers[SSDP_NT], strlen(SsdpMessage->Headers[SSDP_ST])) == 0)
        {
            PSSDP_SEARCH_RESPONSE ResponseEntry;

            ResponseEntry = (PSSDP_SEARCH_RESPONSE) malloc(sizeof(SSDP_SEARCH_RESPONSE));

            if (ResponseEntry == NULL)
            {
                TraceTag(ttidError, "Failed to allocate response "
                         "entry.");
                LeaveCriticalSection(&(pService->CSService));
                continue;
            }

            if (InitializeSearchResponseFromRequest(ResponseEntry,
                                                    SsdpMessage,
                                                    sockRecv,
                                                    pRemoteAddr) == FALSE)
            {
                LeaveCriticalSection(&(pService->CSService));
                continue;
            }

            ResponseEntry->Owner = pService;

            InsertHeadList(&(pService->listSearchResponse),
                           &(ResponseEntry->linkage));

            StartSearchResponseTimer(ResponseEntry, SearchResponseTimerProc);

        }
        LeaveCriticalSection(&(pService->CSService));
    }
    LeaveCriticalSection(&CSListAnnounce);
}

PCONTEXT_HANDLE_TYPE * GetServiceByUSN(CHAR *szUSN)
{
    PLIST_ENTRY p;
    LIST_ENTRY *pListHead = &listAnnounce;

    EnterCriticalSection(&CSListAnnounce);
    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        SSDP_SERVICE *pService;
        pService = CONTAINING_RECORD (p, SSDP_SERVICE, linkage);

        EnterCriticalSection(&(pService->CSService));

        if (strcmp(pService->SsdpRequest.Headers[SSDP_USN], szUSN) == 0)
        {
            LeaveCriticalSection(&(pService->CSService));
            LeaveCriticalSection(&CSListAnnounce);
            return pService->RpcContextHandle;
        }
        else
        {
            LeaveCriticalSection(&(pService->CSService));
        }

    }
    LeaveCriticalSection(&CSListAnnounce);
    return NULL;
}

VOID PrintList(LIST_ENTRY *pListHead)
{
    PLIST_ENTRY p;
    int i = 1;

    TraceTag(ttidSsdpAnnounce, "----- SSDP Announcement List -----");

    EnterCriticalSection(&CSListAnnounce);
    for (p = pListHead->Flink; p != pListHead; p = p->Flink, i++)
    {
        SSDP_SERVICE *pService;

        TraceTag(ttidSsdpAnnounce, "----- SSDP Service Content %d -----", i);
        pService = CONTAINING_RECORD (p, SSDP_SERVICE, linkage);
        PrintSSDPService(pService);
    }
    LeaveCriticalSection(&CSListAnnounce);
}

VOID CleanupAnnounceEntry (SSDP_SERVICE *pService)
{
    EnterCriticalSection(&CSListAnnounce);

    EnterCriticalSection(&pService->CSService);
    RemoveEntryList(&(pService->linkage));
    LeaveCriticalSection(&pService->CSService);

    LeaveCriticalSection(&CSListAnnounce);

    StopAnnounceTimer(pService);

    CleanupListSearchResponse(&(pService->listSearchResponse));
}

VOID CleanupListAnnounce()
{
    PLIST_ENTRY p;
    PLIST_ENTRY pListHead = &listAnnounce;

    TraceTag(ttidSsdpAnnounce, "----- Cleanup SSDP Announcement List -----");

    EnterCriticalSection(&CSListAnnounce);
    for (p = pListHead->Flink; p && p != pListHead;)
    {
        SSDP_SERVICE *pService;

        pService = CONTAINING_RECORD (p, SSDP_SERVICE, linkage);

        p = p->Flink;

        EnterCriticalSection(&pService->CSService);
        pService->state = SERVICE_NO_MASTER_CLEANUP;
        LeaveCriticalSection(&pService->CSService);

        CleanupAnnounceEntry(pService);

        FreeSSDPService(pService);
    }

    LeaveCriticalSection(&CSListAnnounce);
    DeleteCriticalSection(&CSListAnnounce);
}

VOID PrintSSDPService(const SSDP_SERVICE *pSSDPService)
{
    PrintSsdpRequest(&(pSSDPService->SsdpRequest));
}

BOOL InitializeSsdpRequestFromMessage(SSDP_REQUEST *pRequest,
                                      const SSDP_MESSAGE *pssdpSrc)
{
    CHAR buffer[BUF_SIZE];

	InitializeSsdpRequest(pRequest);
	
    pRequest->Method = SSDP_NOTIFY;

    if (pssdpSrc->szType != NULL)
    {
        pRequest->Headers[SSDP_NT] = (CHAR *) malloc(sizeof(CHAR) *
                                                     (strlen(pssdpSrc->szType) + 1));
        if (pRequest->Headers[SSDP_NT]  == NULL)
        {
            TraceTag(ttidError, "Failed to allocate memory for SSDP Headers");
            return FALSE;
        }
        strcpy(pRequest->Headers[SSDP_NT], pssdpSrc->szType);
    }

    if (pssdpSrc->szUSN != NULL)
    {
        pRequest->Headers[SSDP_USN] = (CHAR*) malloc(sizeof(CHAR) *
                                                     (strlen(pssdpSrc->szUSN) + 1));
        if (pRequest->Headers[SSDP_USN]  == NULL)
        {
            TraceTag(ttidError, "Failed to allocate memory for SSDP Headers");
            return FALSE;
        }
        strcpy(pRequest->Headers[SSDP_USN], pssdpSrc->szUSN);
    }

    if (pssdpSrc->szAltHeaders != NULL)
    {
        pRequest->Headers[SSDP_AL] = (CHAR*) malloc(sizeof(CHAR) *
                                                    (strlen(pssdpSrc->szAltHeaders) + 1));
        if (pRequest->Headers[SSDP_AL]  == NULL)
        {
            TraceTag(ttidError, "Failed to allocate memory for SSDP Headers");
            return FALSE;
        }
        strcpy(pRequest->Headers[SSDP_AL], pssdpSrc->szAltHeaders);
    }
    
    if (pssdpSrc->szNls != NULL)
    {
        pRequest->Headers[SSDP_NLS] = (CHAR*) malloc(sizeof(CHAR) *
                                                    (strlen(pssdpSrc->szNls) + 1));
        if (pRequest->Headers[SSDP_NLS]  == NULL)
        {
            TraceTag(ttidError, "Failed to allocate memory for SSDP Headers");
            return FALSE;
        }
        strcpy(pRequest->Headers[SSDP_NLS], pssdpSrc->szNls);
    }

    if (pssdpSrc->szLocHeader != NULL)
    {
        pRequest->Headers[SSDP_LOCATION] = (CHAR*) malloc(sizeof(CHAR) *
                                                          (strlen(pssdpSrc->szLocHeader) + 1));
        if (pRequest->Headers[SSDP_LOCATION]  == NULL)
        {
            TraceTag(ttidSsdpAnnounce, "Failed to allocate memory for SSDP Headers");
            return FALSE;
        }
        strcpy(pRequest->Headers[SSDP_LOCATION], pssdpSrc->szLocHeader);
    }

    _itoa(pssdpSrc->iLifeTime,buffer, 10);

    pRequest->Headers[SSDP_CACHECONTROL] = (CHAR *) malloc(sizeof(CHAR) *
                                                           (strlen("max-age=") +
                                                            strlen(buffer) + 1));
    if (pRequest->Headers[SSDP_CACHECONTROL]  == NULL)
    {
        TraceTag(ttidSsdpAnnounce, "Failed to allocate memory for SSDP Headers");
        return FALSE;
    }
    sprintf(pRequest->Headers[SSDP_CACHECONTROL], "max-age=%d",
            pssdpSrc->iLifeTime);


	// need enough space for major version and minor version
	pRequest->Headers[SSDP_SERVER]= (CHAR *) malloc(sizeof(c_szServerVersion)+5+5);
    if (pRequest->Headers[SSDP_SERVER]  == NULL)
    {
        TraceTag(ttidSsdpAnnounce, "Failed to allocate memory for SSDP Headers");
        return FALSE;
    }
    sprintf(pRequest->Headers[SSDP_SERVER],c_szServerVersion,CE_MAJOR_VER, CE_MINOR_VER);
    
    /*
    pssdpDest->Content = (CHAR*) malloc(sizeof(CHAR) * (strlen(pssdpSrc->szContent) + 1));
    strcpy(pssdpDest->Content, pssdpSrc->szContent);
    */

    return TRUE;
}
// Publication
HANDLE WINAPI RegisterUpnpService(PSSDP_MESSAGE pSsdpMessage, DWORD flags)
{
	DWORD error = ERROR_SUCCESS;
	PSSDP_SERVICE pSSDPSvc;
    if (InterlockedExchange(&cInitialized, cInitialized) == 0) 
    {
        error = ERROR_NOT_READY; 
    }
    else if (pSsdpMessage->szUSN == NULL || pSsdpMessage->szType == NULL)
    {
        error = ERROR_INVALID_PARAMETER;
    }
    else if (pSsdpMessage->szAltHeaders == NULL && pSsdpMessage->szLocHeader == NULL)
    {
        error = ERROR_INVALID_PARAMETER;
    }
    else if (IsInListAnnounce(pSsdpMessage->szUSN))
    {
        error = ERROR_DUPLICATE_SERVICE;
    }
	else if ((pSSDPSvc = AddToListAnnounce(pSsdpMessage, flags, NULL /* pphContext */)) == NULL)
	{
        error = ERROR_NOT_ENOUGH_MEMORY;
    }
    else
    {

    	// send on all networks
    	pSSDPSvc->SsdpRequest.Headers[SSDP_NTS] = AliveHeader;
    	SendAnnouncement(pSSDPSvc);

    	// We need a timer for each network, but one list for all networks.
    	StartAnnounceTimer(pSSDPSvc, AnnounceTimerProc);
    	return (HANDLE)pSSDPSvc;

    }
	SetLastError(error);
    return INVALID_HANDLE_VALUE;
}

BOOL WINAPI DeregisterUpnpService(HANDLE hRegister, BOOL fByebye)
{
    SSDP_SERVICE *pSSDPSvc = (SSDP_SERVICE *) (hRegister);
    DWORD error = ERROR_SUCCESS;

    TraceTag(ttidSsdpAnnounce, "%x is being deregistered.", pSSDPSvc);

    if (InterlockedExchange(&cInitialized, cInitialized) == 0) 
    {
        error = ERROR_NOT_READY; 
    }
    else if (!hRegister || (INVALID_HANDLE_VALUE == hRegister))
    {
        error = ERROR_INVALID_PARAMETER;
    }
    else if (pSSDPSvc->Type != SSDP_SERVICE_SIGNATURE)
    {
        TraceTag(ttidSsdpAnnounce, "%x has type %d, not valid", pSSDPSvc, pSSDPSvc->Type);
        error = ERROR_INVALID_PARAMETER;
    }
    else
    {
	    EnterCriticalSection(&(pSSDPSvc->CSService));

	    if (pSSDPSvc->state != SERVICE_ACTIVE_NO_MASTER &&
	        pSSDPSvc->state != SERVICE_ACTIVE_W_MASTER)
	    {
	        // Ignore possible duplicate deregistration
	        LeaveCriticalSection(&(pSSDPSvc->CSService));
	        TraceTag(ttidSsdpAnnounce, "%x is not active.", pSSDPSvc);
	        error = ERROR_INVALID_PARAMETER;

	    }
	    else
	    {
	        pSSDPSvc->state = SERVICE_NO_MASTER_CLEANUP;

	        LeaveCriticalSection(&(pSSDPSvc->CSService));
		    CleanupAnnounceEntry(pSSDPSvc);

		    // Annoucement timer should be stopped by now and entry is not on listAnnounce.
		    // To-Do: What if system service goes down when we are sending byebye?
		    // Check for NT service.

		    if (fByebye)
		    {
		        ResetEvent(pSSDPSvc->CleanupEvent);
				pSSDPSvc->SsdpRequest.Headers[SSDP_NTS] = ByebyeHeader;
				SendAnnouncement(pSSDPSvc);
		        StartAnnounceTimer(pSSDPSvc, ByebyeTimerProc);
		    }
		    else
		    {
		        FreeSSDPService(pSSDPSvc);
		    }
		    return TRUE;

	    }
	}

	SetLastError(error);
    return FALSE;
}

BOOL WINAPI DeregisterUpnpServiceByUSN(
                              /* [string][in] */ LPSTR szUSN,
                              /* [in] */ BOOL fByebye)
{
    HANDLE handle = (HANDLE)GetServiceByUSN((LPSTR)szUSN);

    if (handle != NULL)
    {
        return DeregisterUpnpService(handle, fByebye);
    }
    else
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
}

