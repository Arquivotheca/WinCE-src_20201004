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

#include <math.h>

BOOL InitializeSearchResponseFromRequest(SSDP_SEARCH_RESPONSE *pSearchResponse,
                                         SSDP_REQUEST *SsdpRequest,
                                         SOCKET sock,
                                         PSOCKADDR_STORAGE RemoteAddr)
{
    pSearchResponse->Type = SSDP_SEARCH_RESPONSE_SIGNATURE;

    pSearchResponse->dwTimerCookie = 0;

    pSearchResponse->DoneEvent =  CreateEvent(NULL, TRUE, FALSE, NULL);

    if (pSearchResponse->DoneEvent == NULL)
    {
        return FALSE;
    }

    pSearchResponse->RetryCount = NUM_RETRIES;
    pSearchResponse->MX = atol(SsdpRequest->Headers[SSDP_MX]);

    // copy all fields.
    pSearchResponse->socket = sock;
    pSearchResponse->RemoteAddr = *RemoteAddr;
    pSearchResponse->szResponse = NULL;

    return TRUE;
}

DWORD SearchResponseTimerProc (VOID *Arg)
{
    PSSDP_SEARCH_RESPONSE ResponseEntry = (PSSDP_SEARCH_RESPONSE) Arg;
    INT Delay;
    SSDP_SERVICE *Service;
    
    Service = ResponseEntry->Owner;

    TraceTag(ttidTrace, "Entering Critical Section %x of SSDP_SERVICE %x", Service->CSService, Service);

    EnterCriticalSection(&(Service->CSService));

    TraceTag(ttidSsdpTimer, "Timer for search response %x expired with count = %d",
             ResponseEntry,ResponseEntry->RetryCount );

    if (Service->state == SERVICE_NO_MASTER_CLEANUP)
    {
        TraceTag(ttidSsdpSearchResp, "Service %x is in cleanup state", Service); 
        TraceTag(ttidSsdpSearchResp, "Signal Event for Response %x", ResponseEntry); 
        SetEvent(ResponseEntry->DoneEvent);
        LeaveCriticalSection(&(Service->CSService));
        return 0;
    }
    
	assert(Service->SsdpRequest.Headers[SSDP_ST] == NULL);
	
	Service->SsdpRequest.Headers[SSDP_ST] = Service->SsdpRequest.Headers[SSDP_NT];

	SendAnnouncementOrResponse(Service, ResponseEntry->socket, &(ResponseEntry->RemoteAddr));

	Service->SsdpRequest.Headers[SSDP_ST] = NULL;

    ResponseEntry->RetryCount--;

    if (ResponseEntry->RetryCount == 0)
    {
        RemoveFromListSearchResponse(ResponseEntry);
        CloseHandle(ResponseEntry->DoneEvent);
        TraceTag(ttidSsdpSearchResp, "Done with search response (%x).", ResponseEntry);
        free(ResponseEntry);
    }
    else
    {
        srand(GetTickCount());
        Delay = (rand() % (ResponseEntry->MX * 100)) * 10;

        TraceTag(ttidSsdpTimer, "Delaying response for %d miliseconds for response %x",
                 Delay, ResponseEntry);

        if(!(ResponseEntry->dwTimerCookie = g_pThreadPool->StartTimer(SearchResponseTimerProc, ResponseEntry, Delay)))
        {
            TraceTag(ttidError, "Failed to start timer for search response %x.", ResponseEntry);
            RemoveFromListSearchResponse(ResponseEntry);
            TraceTag(ttidSsdpSearchResp, "Freeing search response (%x).", ResponseEntry);
            free(ResponseEntry);
        }
        else
        {
            TraceTag(ttidSsdpTimer, "Started search response timer for %x.", ResponseEntry);
        }
    }

    TraceTag(ttidTrace, "Leaving Critical Section %x of SSDP_SERVICE %x",
             Service->CSService, Service);
    LeaveCriticalSection(&(Service->CSService));
    
    return 0;
}

// Merge with RemoveFromListAnnounce?

VOID RemoveFromListSearchResponse(PSSDP_SEARCH_RESPONSE ResponseEntry)
{

    PSSDP_SERVICE Service;

    Service = ResponseEntry->Owner;

    EnterCriticalSection(&Service->CSService);
    RemoveEntryList(&(ResponseEntry->linkage));
    LeaveCriticalSection(&Service->CSService);
}

VOID StartSearchResponseTimer(PSSDP_SEARCH_RESPONSE ResponseEntry,
                              LPTHREAD_START_ROUTINE pCallback)
{
    INT Delay;
    BOOL fFree = FALSE;
    SSDP_SERVICE *Service;

    Service = ResponseEntry->Owner;

    EnterCriticalSection(&(Service->CSService));

    _try
    {
        srand(GetTickCount());
        Delay = (rand() % (ResponseEntry->MX * 100)) * 10;
        
        TraceTag(ttidSsdpTimer, "Delaying response for %d miliseconds",
                 Delay);

        if(!(ResponseEntry->dwTimerCookie = g_pThreadPool->StartTimer(pCallback, ResponseEntry, Delay)))
        {
            TraceTag(ttidError, "Failed to start search resp. timer.");
            RemoveFromListSearchResponse(ResponseEntry);
            TraceTag(ttidSsdpSearchResp, "Freeing search response (%x).", ResponseEntry);
            free(ResponseEntry);
        }
        else
        {
            TraceTag(ttidSsdpTimer, "Started search resp. timer.");
        }
    }
    _finally
    {
        LeaveCriticalSection(&(Service->CSService));
    }
}

VOID StopSearchResponseTimer(PSSDP_SEARCH_RESPONSE ResponseEntry)
{
    SSDP_SERVICE *Service;

    Service = ResponseEntry->Owner;

    EnterCriticalSection(&Service->CSService);

    TraceTag(ttidSsdpSearchResp, "Stopping SearchResponsement timer for "
             "service %x", ResponseEntry);

    if (g_pThreadPool->StopTimer(ResponseEntry->dwTimerCookie))
    {
        TraceTag(ttidSsdpTimer, "Search response timer stopped for "
                 "service %x", ResponseEntry);
        LeaveCriticalSection(&(Service->CSService));
    }
    else
    {
        // Timer is running.
        LeaveCriticalSection(&(Service->CSService));
        TraceTag(ttidSsdpSearchResp, "Search Response timer is running, "
                 "wait ...%x", ResponseEntry);
        WaitForSingleObject(ResponseEntry->DoneEvent, INFINITE);
        TraceTag(ttidSsdpSearchResp, "Wait on search response (%x) returned.",
                 ResponseEntry);
    }
}

VOID CleanupSearchResponseEntry(PSSDP_SEARCH_RESPONSE ResponseEntry)
{
    TraceTag(ttidSsdpSearchResp, "Cleanup Search Response Entry %x", ResponseEntry);
    RemoveFromListSearchResponse(ResponseEntry);
    StopSearchResponseTimer(ResponseEntry);
    CloseHandle(ResponseEntry->DoneEvent);
    free(ResponseEntry);
}

VOID CleanupListSearchResponse(PLIST_ENTRY pListHead )
{
    PLIST_ENTRY p;
    CRITICAL_SECTION CSService;

    TraceTag(ttidSsdpSearchResp, "----- Cleanup SSDP SearchResponsement "
             "List -----");

    for (p = pListHead->Flink; p != pListHead;)
    {
        PSSDP_SEARCH_RESPONSE ResponseEntry;

        ResponseEntry = CONTAINING_RECORD (p, SSDP_SEARCH_RESPONSE , linkage);

        CSService = ResponseEntry->Owner->CSService;
        CleanupSearchResponseEntry(ResponseEntry);

        // No need to worry about insertion to the list as the service removed
        // from listAnnounce.

        EnterCriticalSection(&CSService);
        p = pListHead->Flink;
        LeaveCriticalSection(&CSService);
    }
}
