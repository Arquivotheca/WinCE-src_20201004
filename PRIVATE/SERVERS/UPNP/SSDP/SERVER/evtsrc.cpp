//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       E V T S R C . C P P
//
//  Contents:
//
//  Notes:
//
//  
//
//----------------------------------------------------------------------------

#include <ssdppch.h>
#pragma hdrstop
#include "ssdpnetwork.h"

extern LONG cInitialized;

LIST_ENTRY          g_listEventSource;
CRITICAL_SECTION    g_csListEventSource;
static HANDLE          g_hThrdEventSource;
static DWORD        g_fShutdownEventThread;
HANDLE          g_hEvtEventSource;

//
// Worker thread responsible for sending events to all subscribers
//
DWORD
WINAPI
EventSourceWorkerThread(PVOID )
{
    PLIST_ENTRY p, p2;
    PLIST_ENTRY pListHead = &g_listEventSource;
    
	while (!g_fShutdownEventThread)
    {
        UPNP_EVENT_SOURCE *pes;
        UPNP_SUBSCRIBER *pSub;
        bool bPendingEvents = false;
        
        EnterCriticalSection(&g_csListEventSource);
        //
        // Walk the list of event sources searching for the first source with pending events
        // If we find one, move it to the end of the list
        // (For this to work consistently, new event sources must be added to the head of the list)
        for (p = pListHead->Flink; p != pListHead; p = p->Flink)
        {
            pes = CONTAINING_RECORD (p, UPNP_EVENT_SOURCE, linkage);
            
            // Check if there events pending for any subscribers;
            // Flag all subscribers with event pending
            // If a new subscriber is added to this event source while we are sending an event
            // it will not have the flag set, which is correct.
            for (p2 = pes->listSubs.Flink; p2 != &pes->listSubs; p2 = p2->Flink)
            {
                pSub = CONTAINING_RECORD(p2, UPNP_SUBSCRIBER, linkage);
                
                if (pSub->iSeq != 0)
                {
                    for(unsigned i = 0; i < pSub->cProps; ++i)
                        if(pSub->rgesModified[i])
                        {
                            pSub->flags |= F_UPNPSUB_PENDINGEVENT;
                            bPendingEvents = true;
                            break;
                        }
                }
                else
                {
                    TraceTag(ttidEvents, "EventSourceWorkerThread: Skipping subscriber with iSeq == 0");
                }
            }
                            
            if(bPendingEvents)
            {
                // round robin scheduling of event source: put this event source at the end of the list
                // This also makes it possible to locate the event source in the next while loop
                RemoveEntryList(p);
                InsertTailList(pListHead, p);
                break;
            }
        }

        if (bPendingEvents)
        {
            TraceTag(ttidEvents, "Processing Event for %s:\n-------------------", pes->szRequestUri);
            // we have an event to send to one or more subscribers
            // We give up the critical section while iterating through the subscriber list so
            // make sure the event source at the tail of the list is the same one we are
            // processing. If it's not then the event source must have been deleted!
            while (!IsListEmpty(pListHead) 
                  && CONTAINING_RECORD(pListHead->Blink, UPNP_EVENT_SOURCE, linkage) == pes)
            {
                // go through each subscriber, and pick out the first one with PENDINGEVENT
                // flag set. 
                for (p2 = pes->listSubs.Flink; p2 != &pes->listSubs; p2 = p2->Flink)
                {
                     pSub = CONTAINING_RECORD(p2, UPNP_SUBSCRIBER, linkage);
                     if (pSub->flags & F_UPNPSUB_PENDINGEVENT)
                     {
                         break;
                     }
                }

                if (p2 != &pes->listSubs)
                {
                    // we broke out of the above for loop => we found a subscriber to send to
                    // clear the PENDINGEVENT flag, so we skip past this subscriber next time.
                    pSub->flags &= ~F_UPNPSUB_PENDINGEVENT;

                    // fields after leaving the Critical Section.
					// make a copy of the destination URL and SID since we can't touch the subscriber
                    ce::string strUrl = pSub->szDestUrl;
					ce::string strSid = pSub->szSid;
                    UINT iSeq = pSub->iSeq;
                    LPSTR pszBody = NULL; 

					// increment the subscriber's sequence number - handle overflow
					if(++pSub->iSeq == 0)
						pSub->iSeq = 1;
                    
                    // create event body
                    if(SUCCEEDED(HrComposeXmlBodyFromEventSource(pes, pSub, false, &pszBody)))
                    {
                        // now leave the critical section. 
                        // All event source and subscriber pointers are off limits after this.
                        //
                        LeaveCriticalSection(&g_csListEventSource);
                        
                        HrSubmitEventToSubscriber(0, strSid, iSeq, pszBody, strUrl);

                        free(pszBody);
                        
                        EnterCriticalSection(&g_csListEventSource);
                    }
                }
                else
                {
                    // there are no more subscribers to send this event
                    break;
                }
            }
            
			TraceTag(ttidEvents, "EventSourceWorkerThread: ---Done sending event ----");
        }
        
        LeaveCriticalSection(&g_csListEventSource);
        
		if (!bPendingEvents)
        {
            // no more events to be sent to anyone. Go to sleep ....
            TraceTag(ttidEvents, "EventSourceWorkerThread: Sleeping...");
            WaitForSingleObject(g_hEvtEventSource,INFINITE);
        }
    }

    return 0;
}

//+---------------------------------------------------------------------------
//
//  Function:   InitializeListEventSource
//
//  Purpose:    Initializes the event source list structures
//
//  Arguments:
//      (none)
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
BOOL InitializeListEventSource()
{
    DWORD dwThreadId;
    InitializeCriticalSection(&g_csListEventSource);
    EnterCriticalSection(&g_csListEventSource);
    InitializeListHead(&g_listEventSource);
    g_fShutdownEventThread = FALSE;
    g_hEvtEventSource = CreateEvent(NULL,FALSE, FALSE, NULL);
    if (g_hEvtEventSource)
        g_hThrdEventSource = CreateThread(NULL, 0, EventSourceWorkerThread, NULL, 0, &dwThreadId);
    LeaveCriticalSection(&g_csListEventSource);
    return g_hEvtEventSource && g_hThrdEventSource;
}

//+---------------------------------------------------------------------------
//
//  Function:   RegisterUpnpEventSource
//
//  Purpose:    Public API to register a URI as a UPnP event source
//
//  Arguments:
//      szRequestUri [in]   URI to register as an event source
//      cProps       [in]   Number of properties this event source supplies
//      rgProps      [in]   List of properties
//
//  Returns:    TRUE if successful, FALSE if not. GetLastError() contains the
//              error code.
//
//  
//
//  Notes:
//
BOOL WINAPI RegisterUpnpEventSource(
    /* [string][in] */ LPCSTR szRequestUri,
    /* [in] */ DWORD cProps,
    /* [in] */ UPNP_PROPERTY __RPC_FAR *rgProps)
{
    UPNP_EVENT_SOURCE *     pes = NULL;
    DWORD                   iProp;
    DWORD                   dwError = NOERROR;

    if (InterlockedExchange(&cInitialized, cInitialized) == 0)
    {
        dwError = ERROR_NOT_READY;
    }
    // Validate params
    //
    else if (!szRequestUri )
    {
        TraceTag(ttidError, "_RegisterUpnpEventSourceRpc: error %ld.",
                   HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));
        dwError = ERROR_INVALID_PARAMETER;
    }

    // Validate property info
    //
    else for (iProp = 0; iProp < cProps; iProp++)
    {
        if (!FValidateUpnpProperty(&rgProps[iProp]))
        {
            TraceTag(ttidError, "_RegisterUpnpEventSourceRpc: error %ld.",
                       HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }
    }
    if (dwError)
    {
    	SetLastError(dwError);
    	return FALSE;
    }

    EnterCriticalSection(&g_csListEventSource);

    // Look to see if they've already registered this event source
    //
    if (PesFindEventSource(szRequestUri))
    {
        TraceTag(ttidError, "_RegisterUpnpEventSourceRpc: error %ld.",
                   HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
        dwError = ERROR_ALREADY_EXISTS;
    }
    else if (!(pes = (UPNP_EVENT_SOURCE *) malloc(sizeof(UPNP_EVENT_SOURCE))))
    {
        TraceTag(ttidError, "_RegisterUpnpEventSourceRpc: error %ld.",
                   HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY));
        dwError = ERROR_OUTOFMEMORY;
    }
	else 
	{
	    pes->cProps = 0;
	    pes->rgesProps = NULL;
	    pes->fCleanup = FALSE;
	    pes->szRequestUri = _strdup(szRequestUri);
	    if (cProps)
	    {
		    pes->rgesProps = (UPNP_PROPERTY*) malloc(sizeof(UPNP_PROPERTY) * cProps);

		    // Copy in property information
		    //
		    for (iProp = 0; iProp < cProps; iProp++)
		    {
		        CopyUpnpProperty(&pes->rgesProps[iProp], &rgProps[iProp]);
		        pes->cProps++;
		    }

		    Assert(pes->cProps == cProps);
		}
	    //InitializeCriticalSection(&(pes->cs));
	    InitializeListHead(&(pes->listSubs));

	    // read the LIST_ENTRY
	    InsertHeadList(&g_listEventSource, &(pes->linkage));

	    PrintListEventSource(&g_listEventSource);
    }
    LeaveCriticalSection(&g_csListEventSource);

    TraceError("_RegisterUpnpEventSourceRpc", HRESULT_FROM_WIN32(dwError));
    if (dwError)
    {
    	SetLastError(dwError);
    	return FALSE;
    }
    return TRUE;
    	
}

//+---------------------------------------------------------------------------
//
//  Function:   DeregisterUpnpEventSource
//
//  Purpose:    Public API to remove an event source
//
//  Arguments:
//      szRequestUri [in]   URI to identify event source to remove
//
//  Returns:    TRUE if successful, FALSE if not. GetLastError() contains the
//              error code.
//
//  
//
//  Notes:
//
BOOL WINAPI DeregisterUpnpEventSource(
    /* [string][in] */ LPCSTR szRequestUri)
{
    DWORD               dwError = NOERROR;
    UPNP_EVENT_SOURCE * pes;

    if (InterlockedExchange(&cInitialized, cInitialized) == 0)
    {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    EnterCriticalSection(&g_csListEventSource);

    pes = PesFindEventSource(szRequestUri);
    if (pes)
    {
        TraceTag(ttidEvents, "DeregisterUpnpEventSource: removing event "
                 "source %s.", szRequestUri);
        CleanupEventSourceEntry(pes);
        FreeEventSource(pes);
    }
    else
    {
        TraceTag(ttidError, "DeregisterUpnpEventSource: unknown URI %s.",
                 szRequestUri);
        dwError = ERROR_INTERNET_ITEM_NOT_FOUND;
    }

    LeaveCriticalSection(&g_csListEventSource);

    TraceError("_RegisterUpnpEventSourceRpc", HRESULT_FROM_WIN32(dwError));
    if (dwError)
    {
    	SetLastError(dwError);
    	return FALSE;
    }
    return TRUE;
}

BOOL WINAPI GetEventSourceInfo(
    /* [string][in] */ LPCSTR szEventSourceUri,
    /* [out] */ EVTSRC_INFO __RPC_FAR *__RPC_FAR *ppinfo)
{
    DWORD               dwError = NOERROR;
    UPNP_EVENT_SOURCE * pes;

    if (InterlockedExchange(&cInitialized, cInitialized) == 0)
    {
       SetLastError(ERROR_NOT_READY);
       return FALSE;
    }
    EnterCriticalSection(&g_csListEventSource);

    pes = PesFindEventSource(szEventSourceUri);
    if (pes)
    {
        EVTSRC_INFO *   pinfo;

        pinfo = (EVTSRC_INFO *)SsdpAlloc(sizeof(EVTSRC_INFO));
        if (pinfo)
        {
            PLIST_ENTRY pListHead = &pes->listSubs;
            PLIST_ENTRY p;
            DWORD       iSub;

            ZeroMemory(pinfo, sizeof(EVTSRC_INFO));
            //EnterCriticalSection(&pes->cs);

            for (p = pListHead->Flink; p != pListHead;)
            {
                p = p->Flink;
                pinfo->cSubs++;
            }

            pinfo->rgSubs =
                (SUBSCRIBER_INFO *)SsdpAlloc(sizeof(SUBSCRIBER_INFO) * pinfo->cSubs);

            for (iSub = 0, p = pListHead->Flink; p != pListHead; iSub++)
            {
                UPNP_SUBSCRIBER * pSub;

                pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER , linkage);
                p = p->Flink;
                pinfo->rgSubs[iSub].csecTimeout = pSub->csecTimeout;
                pinfo->rgSubs[iSub].iSeq = pSub->iSeq;

                if (pSub->szDestUrl)
                {
                    pinfo->rgSubs[iSub].szDestUrl =
                        (LPSTR) SsdpAlloc(strlen(pSub->szDestUrl) + 1);
                    strcpy(pinfo->rgSubs[iSub].szDestUrl, pSub->szDestUrl);
                }

                if (pSub->szSid)
                {
                    pinfo->rgSubs[iSub].szSid =
                        (LPSTR) SsdpAlloc(strlen(pSub->szSid) + 1);
                    strcpy(pinfo->rgSubs[iSub].szSid, pSub->szSid);
                }
            }

            //LeaveCriticalSection(&pes->cs);

            AssertSz(iSub == pinfo->cSubs, "Index doesn't match count!");

            *ppinfo = pinfo;
        }
        else
        {
            dwError = ERROR_OUTOFMEMORY;
        }
    }
    else
    {
        TraceTag(ttidError, "_GetEventSourceInfoRpc: unknown URI %s.",
                 szEventSourceUri);
        dwError = ERROR_INTERNET_ITEM_NOT_FOUND;
    }

    LeaveCriticalSection(&g_csListEventSource);

    TraceError("_GetEventSourceInfoRpc", HRESULT_FROM_WIN32(dwError));
    if (dwError)
    {
    	SetLastError(dwError);
    	return FALSE;
    }
    return TRUE;
}

//+---------------------------------------------------------------------------
//
//  Function:   FValidateUpnpProperty
//
//  Purpose:    Helper function to ensure a UPNP_PROPERTY struct is valid
//
//  Arguments:
//      pProp [in]  Property to validate
//
//  Returns:    TRUE if valid, FALSE if not
//
//  
//
//  Notes:
//
BOOL FValidateUpnpProperty(UPNP_PROPERTY * pProp)
{
    return pProp->szName && pProp->szValue &&
           *pProp->szName;
}

//+---------------------------------------------------------------------------
//
//  Function:   CopyUpnpProperty
//
//  Purpose:    Copies a given property to another property
//
//  Arguments:
//      pPropDst [out]  Destination property
//      pPropSrc [in]   Source property
//
//  Returns:    Nothing
//
//  
//
//  Notes:      Caller must free members of new property with free()
//
VOID CopyUpnpProperty(UPNP_PROPERTY * pPropDst, UPNP_PROPERTY * pPropSrc)
{
    Assert(pPropDst);
    Assert(pPropSrc);

    pPropDst->szName = _wcsdup(pPropSrc->szName);
    pPropDst->szValue = _wcsdup(pPropSrc->szValue);
    pPropDst->dwFlags = pPropSrc->dwFlags;
}

//+---------------------------------------------------------------------------
//
//  Function:   FreeUpnpProperty
//
//  Purpose:    Frees the memory used by a UPNP_PROPERTY struct
//
//  Arguments:
//      pPropSrc [in]   Property to free
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
VOID FreeUpnpProperty(UPNP_PROPERTY * pPropSrc)
{
    if (pPropSrc)
    {
        free(pPropSrc->szName);
        free(pPropSrc->szValue);
    }
}

//+---------------------------------------------------------------------------
//
//  Function:   FreeEventSource
//
//  Purpose:    Frees the memory used by an event source struct
//
//  Arguments:
//      pes [in]    event source to free
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
VOID FreeEventSource(UPNP_EVENT_SOURCE *pes)
{
    if (pes)
    {
        DWORD   iFree;

        //DeleteCriticalSection(&(pes->cs));

        free(pes->szRequestUri);

        for (iFree = 0; iFree < pes->cProps; iFree++)
        {
            FreeUpnpProperty(&pes->rgesProps[iFree]);
        }

        free(pes->rgesProps);
        free(pes);
    }
}

//+---------------------------------------------------------------------------
//
//  Function:   RemoveFromListEventSource
//
//  Purpose:    Removes the given event source from the list of event sources
//
//  Arguments:
//      pes [in]    Event source to remove
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
VOID RemoveFromListEventSource(UPNP_EVENT_SOURCE *pes)
{
    EnterCriticalSection(&g_csListEventSource);
    RemoveEntryList(&(pes->linkage));
    PrintListEventSource(&g_listEventSource);
    LeaveCriticalSection(&g_csListEventSource);
}

//+---------------------------------------------------------------------------
//
//  Function:   PesFindEventSource
//
//  Purpose:    Searches the list of event sources looking for the one
//              identified by szRequestUri
//
//  Arguments:
//      szRequestUri [in]   URI to identify event source
//
//  Returns:    Event source that matches the URI passed in, or NULL if not
//              found.
//
//  
//
//  Notes:      You must be holding the g_csListEventSource lock before
//              calling this!!!
//
UPNP_EVENT_SOURCE * PesFindEventSource(LPCSTR szRequestUri)
{
    PLIST_ENTRY         p;
    PLIST_ENTRY         pListHead = &g_listEventSource;
    UPNP_EVENT_SOURCE * pesRet = NULL;

    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        UPNP_EVENT_SOURCE * pes;

        pes = CONTAINING_RECORD (p, UPNP_EVENT_SOURCE, linkage);

        if (!_stricmp(szRequestUri, pes->szRequestUri))
        {
            pesRet = pes;
            break;
        }
    }

    return pesRet;
}

//+---------------------------------------------------------------------------
//
//  Function:   PesVerifyEventSource
//
//  Purpose:    Ensures that the given event source pointer is still valid
//
//  Arguments:
//      pes [in]    Event source to check
//
//  Returns:    The same pointer if valid, or NULL if not
//
//  
//
//  Notes:      You must be holding the event source list lock prior to
//              calling this function!!
//
UPNP_EVENT_SOURCE * PesVerifyEventSource(UPNP_EVENT_SOURCE *pes)
{
    PLIST_ENTRY         p;
    PLIST_ENTRY         pListHead = &g_listEventSource;
    BOOL                fFound = FALSE;
    UPNP_EVENT_SOURCE * pesRet = NULL;

    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        UPNP_EVENT_SOURCE * pesT;

        pesT = CONTAINING_RECORD (p, UPNP_EVENT_SOURCE, linkage);
        if (pesT == pes)
        {
            pesRet = pes;
            break;
        }
    }

    return pesRet;
}

VOID PrintListEventSource(LIST_ENTRY *pListHead)
{
    PLIST_ENTRY p;
    int i = 1;

    TraceTag(ttidEvents, "----- SSDP Event Source List -----");

    EnterCriticalSection(&g_csListEventSource);
    for (p = pListHead->Flink; p != pListHead; p = p->Flink, i++)
    {
        UPNP_EVENT_SOURCE *pes;

        TraceTag(ttidEvents, "----- SSDP Event Source #%d -----", i);
        pes = CONTAINING_RECORD (p, UPNP_EVENT_SOURCE, linkage);
        PrintEventSource(pes);
    }
    LeaveCriticalSection(&g_csListEventSource);
}

//+---------------------------------------------------------------------------
//
//  Function:   CleanupEventSourceEntry
//
//  Purpose:    Cleans up the structures and memory associated with an event
//              source struct.
//
//  Arguments:
//      pes [in]    Event source to clean up
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
VOID CleanupEventSourceEntry(UPNP_EVENT_SOURCE *pes)
{
    RemoveEntryList(&(pes->linkage));

    //EnterCriticalSection(&pes->cs);
    CleanupListSubscriber(&(pes->listSubs));
    //LeaveCriticalSection(&pes->cs);
}

//+---------------------------------------------------------------------------
//
//  Function:   CleanupListEventSource
//
//  Purpose:    Cleans up the event source list
//
//  Arguments:
//      (none)
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
BOOL CleanupListEventSource()
{
    PLIST_ENTRY p;
    PLIST_ENTRY pListHead = &g_listEventSource;

    TraceTag(ttidEvents, "----- Cleanup SSDP Event Source List -----");

    EnterCriticalSection(&g_csListEventSource);
    for (p = pListHead->Flink; p && p != pListHead;)
    {
        UPNP_EVENT_SOURCE *pes;

        pes = CONTAINING_RECORD (p, UPNP_EVENT_SOURCE, linkage);
        p = p->Flink;

        pes->fCleanup = TRUE;

        CleanupEventSourceEntry(pes);
        FreeEventSource(pes);
    }
    // Close the event source worker thread
    //      First set the shutdown flag, then set the event that the thread is waiting on, then wait for the thread to exit.
    g_fShutdownEventThread = TRUE;
    if (g_hEvtEventSource)
    {
        SetEvent(g_hEvtEventSource);   
    }
    LeaveCriticalSection(&g_csListEventSource);
    if (g_hThrdEventSource)
    {
        WaitForSingleObject(g_hThrdEventSource, INFINITE);
        CloseHandle(g_hThrdEventSource);
        g_hThrdEventSource = NULL;
    }
    if (g_hEvtEventSource)
    {
        CloseHandle(g_hEvtEventSource);
        g_hEvtEventSource = NULL;
    }
    DeleteCriticalSection(&g_csListEventSource);
    return TRUE;
}

VOID PrintEventSource(const UPNP_EVENT_SOURCE *pes)
{
    DWORD   iProp;

    TraceTag(ttidEvents, "Event source at address 0x%08X", pes);
    TraceTag(ttidEvents, "==============================================");
    TraceTag(ttidEvents, "Request-Uri         : %s", pes->szRequestUri);
    TraceTag(ttidEvents, "Cleaning up?        : %s",
             pes->fCleanup ? "Yes" : "No");
    TraceTag(ttidEvents, "Number of properties: %d", pes->cProps);

    for (iProp = 0; iProp < pes->cProps; iProp++)
    {
        TraceTag(ttidEvents, "------------------------------");
        TraceTag(ttidEvents, "   %d) Name     : %s", iProp,
                 pes->rgesProps[iProp].szName);
        TraceTag(ttidEvents, "   %d) Value    : %s", iProp,
                 pes->rgesProps[iProp].szValue);
    }

    TraceTag(ttidEvents, "==============================================");
}

//+---------------------------------------------------------------------------
//
//  Function:   AddToListSubscriber
//
//  Purpose:    Adds a subscriber to the list within an event source
//
//  Arguments:
//      pes  [in]   Event source to add to
//      pSub [in]   Subscription to add
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
VOID AddToListSubscriber(UPNP_EVENT_SOURCE *pes, UPNP_SUBSCRIBER *pSub)
{
    InterlockedIncrement(&g_nSubscribers);
    
    //EnterCriticalSection(&pes->cs);
    InsertHeadList(&pes->listSubs, &pSub->linkage);
    //LeaveCriticalSection(&pes->cs);
}
