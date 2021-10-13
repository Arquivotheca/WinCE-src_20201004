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

    TraceTag(ttidSsdpSearchResp, "Entering Critical Section %x of SSDP_SERVICE %x", Service->CSService, Service);

    EnterCriticalSection(&(Service->CSService));

    TraceTag(ttidSsdpTimer | ttidSsdpSearchResp, "Timer for search response %x expired with count = %d",
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
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        Delay = ((DWORD)now.QuadPart % (ResponseEntry->MX * 100)) * 10;

        TraceTag(ttidSsdpTimer | ttidSsdpSearchResp, "Delaying response for %d miliseconds for response %x",
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
            TraceTag(ttidSsdpTimer | ttidSsdpSearchResp, "Started search response timer for %x.", ResponseEntry);
        }
    }

    TraceTag(ttidSsdpSearchResp, "Leaving Critical Section %x of SSDP_SERVICE %x",
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
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        Delay = ((DWORD)now.QuadPart % (ResponseEntry->MX * 100)) * 10;

        TraceTag(ttidSsdpTimer | ttidSsdpSearchResp, "Delaying response for %d miliseconds", Delay);

        if(!(ResponseEntry->dwTimerCookie = g_pThreadPool->StartTimer(pCallback, ResponseEntry, Delay)))
        {
            TraceTag(ttidError, "Failed to start search resp. timer.");
            RemoveFromListSearchResponse(ResponseEntry);
            TraceTag(ttidSsdpSearchResp, "Freeing search response (%x).", ResponseEntry);
            free(ResponseEntry);
        }
        else
        {
            TraceTag(ttidSsdpTimer | ttidSsdpSearchResp, "Started search resp. timer.");
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
        TraceTag(ttidSsdpTimer | ttidSsdpSearchResp, "Search response timer stopped for "
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

    if(ResponseEntry)
    {
        free(ResponseEntry);
    }
}



VOID CleanupListSearchResponse(PLIST_ENTRY pListHead )
{
    PLIST_ENTRY p;

    TraceTag(ttidSsdpSearchResp, "----- Cleanup SSDP SearchResponsement List -----");

    // lock not required because:
    // no need to worry about insertion to the list as the service removed from listAnnounce.

    for (p = pListHead->Flink; p != pListHead; p = pListHead->Flink)
    {
        PSSDP_SEARCH_RESPONSE ResponseEntry;

        ResponseEntry = CONTAINING_RECORD (p, SSDP_SEARCH_RESPONSE , linkage);
        CleanupSearchResponseEntry(ResponseEntry);
    }
}
