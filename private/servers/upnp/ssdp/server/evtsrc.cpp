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
//  Author:     danielwe   8 Oct 1999
//
//----------------------------------------------------------------------------

#include <ssdppch.h>
#pragma hdrstop
#include <ssdpnetwork.h>
#include <safeint.hxx>

extern LONG cInitialized;

LIST_ENTRY          g_listEventSource;
CRITICAL_SECTION    g_csListEventSource;


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
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
void InitializeListEventSource()
{
    InitializeCriticalSection(&g_csListEventSource);
    EnterCriticalSection(&g_csListEventSource);
    InitializeListHead(&g_listEventSource);
    LeaveCriticalSection(&g_csListEventSource);
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
//  Author:     danielwe   13 Oct 1999
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
        if (!ValidateUpnpProperty(&rgProps[iProp]))
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
        if (cProps && cProps < ULONG_MAX / sizeof(UPNP_PROPERTY)) // protect against integer overflow
        {
            if(pes->rgesProps = (UPNP_PROPERTY*) malloc(sizeof(UPNP_PROPERTY) * cProps))
            {
                // Copy in property information
                //
                for (iProp = 0; iProp < cProps; iProp++)
                {
                    if(CopyUpnpProperty(&pes->rgesProps[pes->cProps], &rgProps[iProp]))
                    {
                        pes->cProps++;
                    }
                }
            }
        }
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
//  Author:     danielwe   13 Oct 1999
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
    DWORD dwError = NOERROR;
    DWORD eventSourceSize = 0;
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

            for (p = pListHead->Flink; p != pListHead;)
            {
                p = p->Flink;
                pinfo->cSubs++;
            }

            if(FALSE == safeIntUMul(pinfo->cSubs, sizeof(SUBSCRIBER_INFO), (unsigned int *)(&eventSourceSize)))
            {
                dwError = ERROR_INVALID_PARAMETER;
                goto Finish;
            }

            pinfo->rgSubs = (SUBSCRIBER_INFO *) SsdpAlloc(eventSourceSize);
            if(!(pinfo->rgSubs))
            {
                dwError = ERROR_OUTOFMEMORY;
                goto Finish;
            }

            for (iSub = 0, p = pListHead->Flink; p != pListHead; iSub++)
            {
                UPNP_SUBSCRIBER * pSub;

                pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER , linkage);
                p = p->Flink;
                pinfo->rgSubs[iSub].csecTimeout = pSub->csecTimeout;
                pinfo->rgSubs[iSub].iSeq = pSub->iSeq;

                if (pSub->szDestUrl)
                {
                    pinfo->rgSubs[iSub].szDestUrl = (LPSTR) SsdpAlloc(strlen(pSub->szDestUrl) + 1);
                    if(!(pinfo->rgSubs[iSub].szDestUrl))
                    {
                        dwError = ERROR_OUTOFMEMORY;
                        goto Finish;
                    }
                    strcpy(pinfo->rgSubs[iSub].szDestUrl, pSub->szDestUrl);
                }

                if (pSub->szSid)
                {
                    pinfo->rgSubs[iSub].szSid = (LPSTR) SsdpAlloc(strlen(pSub->szSid) + 1);
                    if(!(pinfo->rgSubs[iSub].szSid))
                    {
                        dwError = ERROR_OUTOFMEMORY;
                        goto Finish;
                    }
                    strcpy(pinfo->rgSubs[iSub].szSid, pSub->szSid);
                }
            }

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
        TraceTag(ttidError, "_GetEventSourceInfoRpc: unknown URI %s.", szEventSourceUri);
        dwError = ERROR_INTERNET_ITEM_NOT_FOUND;
    }

Finish:
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
//  Function:   ValidateUpnpProperty
//
//  Purpose:    Helper function to ensure a UPNP_PROPERTY struct is valid
//
//  Arguments:
//      pProp [in]  Property to validate
//
//  Returns:    TRUE if valid, FALSE if not
//
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
BOOL ValidateUpnpProperty(UPNP_PROPERTY * pProp)
{
    return pProp->szName && pProp->szValue && *pProp->szName;
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
//  Author:     danielwe   13 Oct 1999
//
//  Notes:      Caller must free members of new property with free()
//
BOOL CopyUpnpProperty(UPNP_PROPERTY * pPropDst, UPNP_PROPERTY * pPropSrc)
{
    Assert(pPropDst);
    Assert(pPropSrc);

    if(!(pPropDst->szName = _wcsdup(pPropSrc->szName)))
    {
        return FALSE;
    }
        
    if(!(pPropDst->szValue = _wcsdup(pPropSrc->szValue)))
    {
        return FALSE;
    }

    pPropDst->dwFlags = pPropSrc->dwFlags;

    return TRUE;
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
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
VOID FreeUpnpProperty(UPNP_PROPERTY * pPropSrc)
{
    if (pPropSrc)
    {
        if(pPropSrc->szName)
        {
            free(pPropSrc->szName);
        }

        if(pPropSrc->szValue)
        {
            free(pPropSrc->szValue);
        }
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
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
VOID FreeEventSource(UPNP_EVENT_SOURCE *pes)
{
    if (pes)
    {
        DWORD   iFree;

        if(pes->szRequestUri)
        {
            free(pes->szRequestUri);
        }

        for (iFree = 0; iFree < pes->cProps; iFree++)
        {
            FreeUpnpProperty(&pes->rgesProps[iFree]);
        }

        if(pes->rgesProps)
        {
            free(pes->rgesProps);
        }

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
//  Author:     danielwe   13 Oct 1999
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
//  Author:     danielwe   13 Oct 1999
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
//  Author:     danielwe   17 Nov 1999
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
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
VOID CleanupEventSourceEntry(UPNP_EVENT_SOURCE *pes)
{
    RemoveEntryList(&(pes->linkage));
    CleanupListSubscriber(&(pes->listSubs));
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
//  Author:     danielwe   13 Oct 1999
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
    LeaveCriticalSection(&g_csListEventSource);
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
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
VOID AddToListSubscriber(UPNP_EVENT_SOURCE *pes, UPNP_SUBSCRIBER *pSub)
{
    InsertHeadList(&pes->listSubs, &pSub->linkage);
}

