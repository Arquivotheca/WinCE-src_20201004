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
//  File:       S U B S . C P P
//
//  Contents:   Functions to send and receive (process) subscription requests
//
//  Notes:
//
//  Author:     danielwe   7 Oct 1999
//
//----------------------------------------------------------------------------

#include <ssdppch.h>
#pragma hdrstop

#include <httpext.h>

#include "ssdpparser.h"
#include "ssdpnetwork.h"

#include "HttpChannel.h"
#include "http_status.h"
#include "url_verifier.h"
#include "upnp_config.h"
#include "dlna.h"

extern url_verifier* g_pURL_Verifier;

LONG g_nSubscribers = 0;

#define DEFAULT_HEADERS_SIZE    512

// optional function that is called when a subscribe request is received
BOOL  (*pfSubscribeCallback)(BOOL fSubscribe, PSTR pszUri);


// SubscriptionExtensionProc
DWORD SubscriptionExtensionProc(LPEXTENSION_CONTROL_BLOCK pecb, DWORD dwIndex)
{
    BOOL fRet;
    HSE_SEND_HEADER_EX_INFO hse = {0};
    CHAR szHeaders[DEFAULT_HEADERS_SIZE];
    CHAR *pszHeaders = &szHeaders[0];
    DWORD cbHeaders = sizeof(szHeaders);
    PSTR pszResponse = NULL;
    DWORD cbResponse;
    SSDP_REQUEST* pSsdpReq;
    BOOL fNotifyNeeded = FALSE;
    UPNP_SUBSCRIBER *pSub = NULL;
    DWORD dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;
    LPSTR pNewSid = NULL;

    pSsdpReq = new SSDP_REQUEST;
    if(!pSsdpReq)
    {
        dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;
        goto Respond;
    }

    SSDP_REQUEST& ssdpReq = *pSsdpReq;
    InitializeSsdpRequest(&ssdpReq);
    ssdpReq.status = HTTP_STATUS_SERVER_ERROR;

    if (!VerifySsdpMethod(pecb->lpszMethod, &ssdpReq)
        || (ssdpReq.Method != GENA_SUBSCRIBE && ssdpReq.Method != GENA_UNSUBSCRIBE)) 
    {
        dwUpnpStatus = HTTP_STATUS_BAD_METHOD;
        goto Respond;
    }

    // Get request URI
    if (pecb->lpszQueryString)
    {
        ssdpReq.RequestUri = SsdpDup(pecb->lpszQueryString);
    }

    fRet = pecb->GetServerVariable(pecb->ConnID, "ALL_RAW", pszHeaders, &cbHeaders);
    if (!fRet && (ERROR_INSUFFICIENT_BUFFER == GetLastError()))
    {
        pszHeaders = (CHAR *)SsdpAlloc(cbHeaders);
        if (!pszHeaders)
        {
            dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;
            goto Respond;
        }

        fRet = pecb->GetServerVariable(pecb->ConnID, "ALL_RAW", pszHeaders, &cbHeaders);
        if (!fRet)
        {
            dwUpnpStatus = HTTP_STATUS_BAD_REQUEST;
            goto Respond;
        }
    }

    // no body for SUBSCRIBE and UNSUBSCRIBE
    ASSERT(pecb->cbTotalBytes == 0);
    ASSERT(pecb->cbAvailable == 0);

    // Parse the request
    ssdpReq.ContentLength = 0;
    if(!ParseHeaders(pszHeaders, &ssdpReq))
    {
        dwUpnpStatus = HTTP_STATUS_BAD_REQUEST;
        goto Respond;
    }

    // Process an unsubscribe
    if (ssdpReq.Method == GENA_UNSUBSCRIBE)
    {
        dwUpnpStatus = RemoveSubscriberFromRequest(&ssdpReq);
        goto Respond;
    }

    // Process a new subscribe
    if (!ssdpReq.Headers[GENA_SID])  // No SID means new subscribe
    {
        dwUpnpStatus = AddSubscriberFromRequest(&ssdpReq, &pszResponse, dwIndex, &pNewSid);
        goto Respond;
    }

    // Process a renew
    if (!ssdpReq.Headers[GENA_CALLBACK] && !ssdpReq.Headers[SSDP_NT])
    {
        dwUpnpStatus = ProcessResubscribeRequest(&ssdpReq, &pszResponse);
        goto Respond;
    }

    // Just plain bad
    SetLastError(ERROR_HTTP_INVALID_HEADER);
    dwUpnpStatus =  HTTP_STATUS_BAD_REQUEST;
    goto Respond;



// Make and send an HTTP response to this HTTP request.
Respond:
    pecb->dwHttpStatusCode = dwUpnpStatus;
    cbResponse = pszResponse ? strlen(pszResponse) : 0;

    hse.pszStatus = ce::http_status_string(pecb->dwHttpStatusCode);
    hse.cchStatus = strlen(hse.pszStatus);
    hse.pszHeader = pszResponse; // Should be empty for HTTP errors.
    hse.cchHeader = cbResponse;
    hse.fKeepConn = FALSE;

    // Send the HTTP response
    pecb->ServerSupportFunction(pecb->ConnID, HSE_REQ_SEND_RESPONSE_HEADER_EXV, &hse, NULL, NULL);  

    // New subscriptions now get a processing thread
    if (pNewSid)
    {
        AddSubscriberFromRequestFinal(ssdpReq.RequestUri, pNewSid);
    }

    // Free up allocated memory
    if(pSsdpReq)
    {
        FreeSsdpRequest(pSsdpReq);
        delete pSsdpReq;
    }
    if (pszHeaders != &szHeaders[0])
    {
        SsdpFree(pszHeaders);
    }
    if (pszResponse)
    {
        SsdpFree(pszResponse);
    }
    if (pNewSid)
    {
        free(pNewSid);
    }

    return HSE_STATUS_SUCCESS;
}

PSTR
ComposeSubscribeResponse(UPNP_SUBSCRIBER* pSub)
{
    // DATE: when response was generated
    // SERVER: OS/version UPnP/1.0 product/version
    // SID: uuid:subscription-UUID
    // TIMEOUT: Second-actual subscription duration

    CHAR szResp[512];   // adequate
    sprintf(szResp, "SID:%s\r\n"
                    "TIMEOUT:Second-%d\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n",
                    pSub->szSid,pSub->csecTimeout);

    return _strdup(szResp);
}

//+---------------------------------------------------------------------------
//
//  Function:   AddSubscriberFromRequest
//
//  Purpose:    Given a SUBSCRIBE request, adds a subscriber to the event
//              source the request indicates.
//
//  Arguments:
//
//      pRequest [in]   Raw SUBSCRIBE message.
//      ppResp   [out]  Response
//
//  Returns:    TRUE if successful, FALSE if not. GetLastError() contains the
//              error code.
//
//  Author:     danielwe   12 Oct 1999
//
//  Notes:
//
DWORD AddSubscriberFromRequest(SSDP_REQUEST * pRequest, PSTR *ppResp, DWORD dwIndex, LPSTR *ppNewSid)
{
    UPNP_EVENT_SOURCE * pes;
    UPNP_SUBSCRIBER *   pSub = NULL;
    DWORD dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;

    AssertSz(pRequest->Method == GENA_SUBSCRIBE, "I thought you told me "
             "this was a subscription request!");

    EnterCriticalSection(&g_csListEventSource);

    if (ppNewSid == NULL)
    {
        goto Cleanup;
    }
    *ppNewSid = NULL;

    // check the message headers to see if this is a valid subscription request
    if (!pRequest->Headers[GENA_CALLBACK])
    {
        TraceTag(ttidEvents, "Didn't get a Callback header!");
        dwUpnpStatus = HTTP_STATUS_BAD_REQUEST;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto Cleanup;
    }

    if (!pRequest->RequestUri || !(*pRequest->RequestUri))
    {
        TraceTag(ttidEvents, "Didn't get a Reqeust-Uri!");
        dwUpnpStatus = HTTP_STATUS_BAD_REQUEST;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto Cleanup;
    }

    // Find the event source this SUBSCRIBE was sent to
    pes = PesFindEventSource(pRequest->RequestUri);
    if (!pes)
    {
        TraceTag(ttidEvents, "Subscription sent to unknown Request URI: %s ",
                 pRequest->RequestUri);

        // No event source matches this Request URI. return 404 Not Found.
        dwUpnpStatus = HTTP_STATUS_NOT_FOUND;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto Cleanup;
    }

    // Make the subscriber record
    pSub = (UPNP_SUBSCRIBER *) malloc(sizeof(UPNP_SUBSCRIBER));
    if (!pSub)
    {
        TraceTag(ttidEvents, "Out of memory!");
        dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;
        SetLastError(ERROR_OUTOFMEMORY);
        goto Cleanup;
    }

    // Fill in some of the other members of the subscription struct
    memset(pSub,0,sizeof(UPNP_SUBSCRIBER));
    InitializeListHead(&pSub->linkage);
    pSub->dwEventTimeout = DEFAULT_EVENT_TIMEOUT;
    pSub->pes = pes;
    pSub->szSid = SzGetNewSid();

    if (!pSub->szSid)
    {
        dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;
        SetLastError(ERROR_OUTOFMEMORY);
        goto Cleanup;
    }

    // Set limits on number of subscribers
    if (InterlockedIncrement(&g_nSubscribers) > upnp_config::max_subscribers())
    {
        TraceTag(ttidEvents, "Too many subscriptions!");
        dwUpnpStatus = HTTP_STATUS_SERVICE_UNAVAIL;
        SetLastError(ERROR_OUTOFMEMORY);
        goto Cleanup;
    }

    // Validate the Callback header and look for at least one URL that starts with "http://"
    if (!ParseCallbackUrl(pRequest->Headers[GENA_CALLBACK], &pSub->szDestUrl))
    {
        TraceTag(ttidEvents, "Callback header '%s' was invalid!",
                 pRequest->Headers[GENA_CALLBACK]);
        dwUpnpStatus = HTTP_STATUS_PRECOND_FAILED;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto Cleanup;
    }

    DWORD dw;
    if(!FixURLAddressScopeId(pSub->szDestUrl, dwIndex, NULL, &(dw = 0)) && dw != 0)
    {
        if(char* szDestUrl = (char*)malloc(dw))
        {
            if(FixURLAddressScopeId(pSub->szDestUrl, dwIndex, szDestUrl, &dw))
            {            
                free(pSub->szDestUrl);
                pSub->szDestUrl = szDestUrl;
            }
            else
            {
                free(szDestUrl);
            }
        }
    }

    if(!g_pURL_Verifier->is_url_ok(pSub->szDestUrl))
    {
        TraceTag(ttidEvents, "Callback url '%s' is invalid.", pSub->szDestUrl);
        dwUpnpStatus = HTTP_STATUS_PRECOND_FAILED;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto Cleanup;
    }
    
    if (FAILED(g_CJHttpChannel.Alloc(&pSub->dwChannelCookie)))
    {
        dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;
        SetLastError(ERROR_OUTOFMEMORY);
        goto Cleanup;
    }

    if (!(pSub->hEventSend = CreateEvent(NULL, FALSE, FALSE, NULL)))
    {
        dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;
        SetLastError(ERROR_OUTOFMEMORY);
        goto Cleanup;
    }
    
    if(pes->cProps)
    {
        pSub->rgesModified = (bool*)malloc(sizeof(bool) * pes->cProps);
        
        if (!pSub->rgesModified)
        {
            dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;
            SetLastError(ERROR_OUTOFMEMORY);
            goto Cleanup;
        }
        
        pSub->cProps = pes->cProps;
    }

    // Start the timer
    pSub->csecTimeout = DwParseTime(pRequest->Headers[GENA_TIMEOUT]);
    pSub->dwSubscriptionTime = GetTickCount();

    // We have a valid subscription, send a response to the subscriber
    *ppResp = ComposeSubscribeResponse(pSub);
    if (*ppResp == NULL)
    {
        dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;
        goto Cleanup;
    }


    // install the new subscription
    AddToListSubscriber(pes, pSub);

    // tell UPNP service about this subscription; Service will prepare the initial notify info.
    if (pfSubscribeCallback)
    {
        LeaveCriticalSection(&g_csListEventSource);
        if (!pfSubscribeCallback(TRUE, pRequest->RequestUri))
        {
            // subscription was vetoed
            pRequest->Method = GENA_UNSUBSCRIBE;
            pRequest->Headers[GENA_SID] = _strdup(pSub->szSid);
            RemoveSubscriberFromRequest(pRequest);
            pSub = NULL;    // pSub was deleted, kill my ref to it.
            pRequest->Method = GENA_SUBSCRIBE;
            dwUpnpStatus = HTTP_STATUS_SERVICE_UNAVAIL;
        }
        EnterCriticalSection(&g_csListEventSource);
        if (dwUpnpStatus == HTTP_STATUS_SERVICE_UNAVAIL)
        {
            goto Cleanup;
        }
    }
    *ppNewSid = _strdup(pSub->szSid);
    dwUpnpStatus = HTTP_STATUS_OK;

Cleanup:
    LeaveCriticalSection(&g_csListEventSource);
    if (pSub && dwUpnpStatus != HTTP_STATUS_OK)
    {
        CleanupSubscriberEntry(pSub);
        pSub = NULL;
    }
    return dwUpnpStatus;
}

// Launch the subscriber's send thread
DWORD AddSubscriberFromRequestFinal(LPCSTR pUri, LPCSTR pSid)
{
    DWORD hr = E_FAIL;
    DWORD dwThreadId = 0;
    HANDLE hThread = NULL;
    UPNP_SUBSCRIBER *pSub = NULL;

    EnterCriticalSection(&g_csListEventSource);

    if (!pSid || !pUri)
    {
        goto done;
    }


    // Search for the subscriber record
    pSub = SubFindSubscriber(pUri, pSid);
    if (!pSub)
    {
        goto done;
    }

    // Create the event sending thread
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SubscriberSendThread, pSub, 0, &dwThreadId);
    if (hThread)
    {
        CloseHandle(hThread);
        pSub->bThreaded = true;
        hr = S_OK;
        goto done;
    }

done:
    // On failure, kill the subscriber
    if (hr != S_OK && pSub)
    {
        CleanupSubscriberEntry(pSub);
        SetLastError(ERROR_OUTOFMEMORY);
    }

    LeaveCriticalSection(&g_csListEventSource);
    return hr;
}

//+---------------------------------------------------------------------------
//
//  Function:   ProcessResubscribeRequest
//
//  Purpose:    Given an SSDP_REQUEST that has been determined to be a re-
//              subscribe request, processes the message.
//
//  Arguments:
//      pRequest [in]   Request to be processed
//      ppResp   [out]  Response
//
//  Returns:    TRUE if successful, FALSE if not. GetLastError() has error
//              code.
//
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
DWORD ProcessResubscribeRequest(SSDP_REQUEST * pRequest, PSTR *ppResp)
{
    UPNP_SUBSCRIBER *   pSub = NULL;
    DWORD dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;

    EnterCriticalSection(&g_csListEventSource);

    if (!pRequest->RequestUri || !(*pRequest->RequestUri))
    {
        TraceTag(ttidEvents, "Didn't get a Reqeust-Uri!");
        dwUpnpStatus =  HTTP_STATUS_BAD_REQUEST;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto Cleanup;
    }

    // Find the subscriber
    pSub = SubFindSubscriber(pRequest->RequestUri, pRequest->Headers[GENA_SID]);
    if (!pSub)
    {
        // Didn't find the subscriber. We should return 412 precondition failed
        TraceTag(ttidEvents, "Re-subscribe sent to unknown SID: %s", pRequest->Headers[GENA_SID]);
        dwUpnpStatus = HTTP_STATUS_PRECOND_FAILED;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto Cleanup;
    }

    // Reset the timeout on the subscription
    pSub->csecTimeout = DwParseTime(pRequest->Headers[GENA_TIMEOUT]);
    *ppResp = ComposeSubscribeResponse(pSub);
    if (*ppResp)
    {
        // Restart the subscription timer
        pSub->dwSubscriptionTime = GetTickCount();
        dwUpnpStatus = HTTP_STATUS_OK;
    }

Cleanup:
    LeaveCriticalSection(&g_csListEventSource);
    return dwUpnpStatus;
}

//+---------------------------------------------------------------------------
//
//  Function:   RemoveSubscriberFromRequest
//
//  Purpose:    Given an UNSUBSCRIBE request, removes the subscription from
//              the list within the event source.
//
//  Arguments:
//      
//      pRequest [in]   Request to be processed
//      ppResp   [out]  Response
//
//  Returns:    TRUE if successful, FALSE if not. GetLastError() contains the
//              error code.
//
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
DWORD RemoveSubscriberFromRequest(SSDP_REQUEST * pRequest)
{
    BOOL  fResult = FALSE;
    UPNP_SUBSCRIBER * pSub = NULL;
    DWORD dwUpnpStatus = HTTP_STATUS_SERVER_ERROR;

    EnterCriticalSection(&g_csListEventSource);

    // First, do some checking on the message headers to see if this is a
    // valid unsubscribe request

    if (!pRequest->Headers[GENA_SID] || !(*(pRequest->Headers[GENA_SID])))
    {
        TraceTag(ttidEvents, "Didn't get a valid SID header!");
        dwUpnpStatus = HTTP_STATUS_BAD_REQUEST;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto Cleanup;
    }

    if (!pRequest->RequestUri || !(*pRequest->RequestUri))
    {
        TraceTag(ttidEvents, "Didn't get a Reqeust-Uri!");
        dwUpnpStatus = HTTP_STATUS_BAD_REQUEST;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto Cleanup;
    }


    // Find the subscriber based on the URI
    pSub = SubFindSubscriber(pRequest->RequestUri, pRequest->Headers[GENA_SID]);
    if (!pSub)
    {
        // Didn't find the subscriber. We should return 404 Not Found.
        TraceTag(ttidEvents, "Unsubscribe sent to unknown SID: %s ", pRequest->Headers[GENA_SID]);
        dwUpnpStatus = HTTP_STATUS_NOT_FOUND;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        fResult = FALSE;
        goto Cleanup;
    }

    // Found the subscriber they were looking for!
    CleanupSubscriberEntry(pSub);
    dwUpnpStatus = HTTP_STATUS_OK;
    fResult = true;

Cleanup:
    LeaveCriticalSection(&g_csListEventSource);

    // signal the unsubscribe to the upper layers
    if (fResult && pfSubscribeCallback)
    {
        pfSubscribeCallback(FALSE, pRequest->RequestUri);
    }

    return dwUpnpStatus;
}


//+---------------------------------------------------------------------------
//
//  Function:   CleanupSubscriberEntry
//
//  Purpose:    Cleans up memory and related structures used by a subscription
//
//  Arguments:
//      pSub [in]   Subscription to clean up
//
//  Returns:    Nothing
//
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
VOID CleanupSubscriberEntry(UPNP_SUBSCRIBER * pSub)
{
    LIST_ENTRY *pEntry = NULL;
    LIST_ENTRY *pListHead = NULL;
    bool fHasThread;

    if (pSub == NULL)
    {
        return;
    }

    // Remove from event source list
    EnterCriticalSection(&g_csListEventSource);
    pSub->bShutdown = true;
    fHasThread = pSub->bThreaded;
    RemoveEntryList(&(pSub->linkage));
    if (pSub->hEventSend)
    {
        SetEvent(pSub->hEventSend);
    }

    // If a thread is running, let it call and finish the cleanup
    if (fHasThread)
    {
        goto exit;
    }

    // Continue with final cleanup
    InterlockedDecrement(&g_nSubscribers);
    if (pSub->hEventSend)
    {
        CloseHandle(pSub->hEventSend);
        pSub->hEventSend = NULL;
    }

    if (pSub->dwChannelCookie)
    {
        g_CJHttpChannel.Free(pSub->dwChannelCookie);
        pSub->dwChannelCookie = 0;
    }

    if (pSub->szDestUrl)
    {
        free(pSub->szDestUrl);
    }
    if (pSub->szSid)
    {
        free(pSub->szSid);
    }
    if (pSub->rgesModified)
    {
        free(pSub->rgesModified);
    }
    free(pSub);

exit:
    LeaveCriticalSection(&g_csListEventSource);
}

//+---------------------------------------------------------------------------
//
//  Function:   CleanupListSubscriber
//
//  Purpose:    Cleans up the subscriber list within an event source.
//
//  Arguments:
//      pListHead [in]  Subscriber list to clean up.
//
//  Returns:    Nothing
//
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
VOID CleanupListSubscriber(PLIST_ENTRY pListHead)
{
    PLIST_ENTRY p;

    TraceTag(ttidEvents, "----- Cleanup ListSubscriber List -----");

    for (p = pListHead->Flink; p != pListHead;)
    {
        UPNP_SUBSCRIBER * pSub;

        pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER , linkage);
        p = p->Flink;
        CleanupSubscriberEntry(pSub);
    }
}

UPNP_SUBSCRIBER *SubFindSubscriber(LPCSTR pUri, LPCSTR pSid)
{
    UPNP_SUBSCRIBER *pSub = NULL;

    if (!pUri || !pSid)
    {
        return pSub;
    }

    UPNP_EVENT_SOURCE *pes = PesFindEventSource(pUri);
    if (!pes)
    {
        return pSub;
    }

    PLIST_ENTRY pListHead = &pes->listSubs;
    for (PLIST_ENTRY p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER, linkage);
        if (!_stricmp(pSub->szSid, pSid))
        {
            break;
        }
        pSub = NULL;
    }

    return pSub;
}
