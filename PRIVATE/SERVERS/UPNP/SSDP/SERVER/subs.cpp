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
//  File:       S U B S . C P P
//
//  Contents:   Functions to send and receive (process) subscription requests
//
//  Notes:
//
//  
//
//----------------------------------------------------------------------------

#include <ssdppch.h>
#pragma hdrstop

#include <httpext.h>

#include "ssdpparser.h"
#include "ssdpnetwork.h"

#include "http_status.h"
#include "url_verifier.h"
#include "upnp_config.h"

extern url_verifier* g_pURL_Verifier;

const CHAR c_szUrlPrefix[]             = "http://";

const DWORD g_cDefaultSubTimeout = 1800;
const DWORD g_cDefaultMaxSubTimeout = 3600;
const DWORD g_cMaxSubTimeout = 0xFFFFFFFF / 1000;

LONG g_nSubscribers = 0;

#define DEFAULT_HEADERS_SIZE	512

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

    if(pSsdpReq = new SSDP_REQUEST)
    {
        SSDP_REQUEST& ssdpReq = *pSsdpReq;
	
	    InitializeSsdpRequest(&ssdpReq);

	    ssdpReq.status = HTTP_STATUS_OK;
        
  	    if (VerifySsdpMethod(pecb->lpszMethod, &ssdpReq)
  		    && (ssdpReq.Method == GENA_SUBSCRIBE || ssdpReq.Method == GENA_UNSUBSCRIBE)) 
  	    {
		    // Get request URI
  		    if (pecb->lpszQueryString)
  			    ssdpReq.RequestUri = SsdpDup(pecb->lpszQueryString);

  		    fRet = pecb->GetServerVariable(pecb->ConnID, "ALL_RAW", pszHeaders, &cbHeaders);

		    if (!fRet && (ERROR_INSUFFICIENT_BUFFER == GetLastError()))
		    {
			    pszHeaders = (CHAR *)SsdpAlloc(cbHeaders);
			    if (pszHeaders)
			    {
				    fRet = pecb->GetServerVariable(pecb->ConnID, "ALL_RAW", pszHeaders, &cbHeaders);
			    }
		    }

		    // no body for SUBSCRIBE and UNSUBSCRIBE
		    ASSERT(pecb->cbTotalBytes == 0);
            ASSERT(pecb->cbAvailable == 0);

		    ssdpReq.ContentLength = 0;

		    if (fRet)
                if(ParseHeaders(pszHeaders, &ssdpReq))
		        {
			        fRet = FProcessSubscribeRequest(&ssdpReq, &pszResponse, &fNotifyNeeded, dwIndex);
		        }
                else
                {
                    if (ssdpReq.status == HTTP_STATUS_OK)
                        ssdpReq.status = HTTP_STATUS_BAD_REQUEST;
                }

		    if (!fRet && ssdpReq.status == HTTP_STATUS_OK)
			    ssdpReq.status = HTTP_STATUS_SERVER_ERROR;
	    }
	    else
	    {
		    ssdpReq.status = HTTP_STATUS_BAD_METHOD;
	    }
	    
	    pecb->dwHttpStatusCode = ssdpReq.status;
    }
    else
    {
        pecb->dwHttpStatusCode = HTTP_STATUS_SERVER_ERROR;
    }
  
	cbResponse = pszResponse ? strlen(pszResponse) : 0;

	hse.pszStatus = ce::http_status_string(pecb->dwHttpStatusCode);
	hse.cchStatus = strlen(hse.pszStatus);
	hse.pszHeader = pszResponse; // Should be empty for HTTP errors.
	hse.cchHeader = cbResponse;
	hse.fKeepConn = FALSE;

	pecb->ServerSupportFunction(pecb->ConnID, HSE_REQ_SEND_RESPONSE_HEADER_EX, &hse, NULL, NULL);  

	if (fNotifyNeeded)
	{
	    assert(pSsdpReq);
	    
		


		g_pThreadPool->StartTimer((LPTHREAD_START_ROUTINE)SendInitialEventNotification, pSsdpReq, 1000);
	}
	else
	{
	    if(pSsdpReq)
	    {
	        FreeSsdpRequest(pSsdpReq);
            delete pSsdpReq;
        }
	}
	
	if (pszHeaders != &szHeaders[0])
		SsdpFree(pszHeaders);

	if (pszResponse)
		SsdpFree(pszResponse);

	return HSE_STATUS_SUCCESS;
}


// optional function that is called when a subscribe request is received
BOOL  (*pfSubscribeCallback)(BOOL fSubscribe, PSTR pszUri);
//+---------------------------------------------------------------------------
//
//  Function:   FProcessSubscribeRequest
//
//  Purpose:    Given an SSDP_REQUEST, determines whether it is a subscribe,
//              re-subscribe, or unsubscribe and passes it on for further
//              processing.
//
//  Arguments:
//      socket   [in]   Socket to send response to
//      pRequest [in]   Request to be processed
//
//  Returns:    TRUE if successful, FALSE if not. GetLastError() has error
//              code.
//
//  
//
//  Notes:
//
BOOL FProcessSubscribeRequest(SSDP_REQUEST * pRequest, PSTR *ppszResponse, BOOL *pfNotifyNeeded, DWORD dwIndex)
{
    BOOL    fResult = FALSE;
    *pfNotifyNeeded = FALSE;

    AssertSz((pRequest->Method == GENA_SUBSCRIBE) ||
             (pRequest->Method == GENA_UNSUBSCRIBE), "I thought you told me "
             "this was a subscription request!");

    if (pRequest->Method == GENA_SUBSCRIBE)
    {
        // Figure out if this is a re-subscribe request or not
        if (pRequest->Headers[GENA_SID])
        {
            if (!pRequest->Headers[GENA_CALLBACK] &&
                !pRequest->Headers[SSDP_NT])
            {
                // Having a SID header means this is supposed to be a
                // re-subscribe request
                fResult = FProcessResubscribeRequest( pRequest, ppszResponse);
            }
            else
            {
                // but, they included an NT or Callback header so this is
                // confusing and invalid.
                //
                SetLastError(ERROR_HTTP_INVALID_HEADER);
                pRequest->status =  HTTP_STATUS_BAD_REQUEST;
            }
        }
        else
        {
            // No SID header on a SUBSCRIBE request means this must be an
            // initial subscribe. 
            //
            if(g_nSubscribers < upnp_config::max_subscribers())
            {
                fResult = FAddSubscriberFromRequest(pRequest, ppszResponse, dwIndex);

                if (fResult)
                {
            	    // need to send initial notification AFTER sending the response
            	    *pfNotifyNeeded = TRUE;
                }
            }
            else
            {
                pRequest->status = HTTP_STATUS_SERVICE_UNAVAIL;
                fResult = FALSE;
            }
        }
    }
    else
    {
        fResult = FRemoveSubscriberFromRequest(pRequest, ppszResponse);
    }

    TraceResult("FProcessSubscribeRequest", fResult);
    return fResult;
}

PSTR
ComposeSubscribeResponse(UPNP_SUBSCRIBER* pSub)
{
	// DATE: when response was generated
	// SERVER: OS/version UPnP/1.0 product/version
	// SID: uuid:subscription-UUID
	// TIMEOUT: Second-actual subscription duration

	CHAR szResp[512];	// adequate
	sprintf(szResp,	"SID:%s\r\n"
					"TIMEOUT:Second-%d\r\n"
					"\r\n",
					pSub->szSid,pSub->csecTimeout);

	return _strdup(szResp);
}
//+---------------------------------------------------------------------------
//
//  Function:   FProcessResubscribeRequest
//
//  Purpose:    Given an SSDP_REQUEST that has been determined to be a re-
//              subscribe request, processes the message.
//
//  Arguments:
//      pRequest [in]   Request to be processed
//		ppResp   [out]  Response
//
//  Returns:    TRUE if successful, FALSE if not. GetLastError() has error
//              code.
//
//  
//
//  Notes:
//
BOOL FProcessResubscribeRequest(SSDP_REQUEST * pRequest, PSTR *ppResp)
{
    BOOL                fResult = FALSE;
    UPNP_EVENT_SOURCE * pes;
    UPNP_SUBSCRIBER *   pSub = NULL;
    PLIST_ENTRY         pListHead;
    PLIST_ENTRY         p;

    AssertSz(pRequest->Method == GENA_SUBSCRIBE, "I thought you told me "
             "this was a subscribe request!");
    AssertSz(pRequest->Headers[GENA_SID], "Why don't I have a SID header??!?!");

    if (!pRequest->RequestUri || !(*pRequest->RequestUri))
    {
        TraceTag(ttidEvents, "Didn't get a Reqeust-Uri!");
        pRequest->status =  HTTP_STATUS_BAD_REQUEST;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanup;
    }

    EnterCriticalSection(&g_csListEventSource);

    // Find the event source based on the URI
    //
    pes = PesFindEventSource(pRequest->RequestUri);
    if (!pes)
    {
        TraceTag(ttidEvents, "Re-subscribe sent to unknown Request URI: %s ",
                 pRequest->RequestUri);

        // No event source registered matches this Request URI. We'll
        // retrun 404 Not Found.
        //
        pRequest->status =  HTTP_STATUS_SERVER_ERROR;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanup;
    }

    // Found the event source this SUBSCRIBE was sent to. Now find the
    // subscriber.
    //
    //EnterCriticalSection(&pes->cs);
    pListHead = &pes->listSubs;

    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER, linkage);
        if (!_stricmp(pSub->szSid, pRequest->Headers[GENA_SID]))
        {
            // Found the subscriber they were looking for!

            break;
        }
    }

    if (p == pListHead)
    {
        TraceTag(ttidEvents, "Re-subscribe sent to unknown SID: %s. Maybe "
                 "its timer ran out?",
                 pRequest->Headers[GENA_SID]);

        // Didn't find the subscriber. We should return 412 precondition
        // failed
        //
        pRequest->status = HTTP_STATUS_PRECOND_FAILED;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        fResult = FALSE;
    }
    else
    {
        // Reset the timeout on the subscription
        //
        SetSubscriptionTimeout(pSub, pRequest->Headers[GENA_TIMEOUT]);
        *ppResp = ComposeSubscribeResponse(pSub);
        
        if (*ppResp)
        {
            // Restart the subscription timer

            StopResubscribeTimer(pSub);

            StartResubscribeTimer(pSub);

            TraceTag(ttidEvents, "Re-subscribe for %s successful.",
                     pRequest->Headers[GENA_SID]);
            fResult = TRUE;
        }
    }

    //LeaveCriticalSection(&pes->cs);

cleanup:
    LeaveCriticalSection(&g_csListEventSource);
    TraceResult("FProcessResubscribeRequest", fResult);
    return fResult;
}

//+---------------------------------------------------------------------------
//
//  Function:   SetSubscriptionTimeout
//
//  Purpose:    Sets the internal timeout for a subscription.
//
//  Arguments:
//      pSub            [in]    Subscription to set
//      szTimeoutHeader [in]    Timeout header from SSDP_REQUEST message
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
VOID SetSubscriptionTimeout(UPNP_SUBSCRIBER * pSub, LPCSTR szTimeoutHeader)
{
    // Parse the Timeout header and convert it to seconds
    //
    pSub->csecTimeout = DwParseTime(szTimeoutHeader);
}

//+---------------------------------------------------------------------------
//
//  Function:   FAddSubscriberFromRequest
//
//  Purpose:    Given a SUBSCRIBE request, adds a subscriber to the event
//              source the request indicates.
//
//  Arguments:
//
//      pRequest [in]   Raw SUBSCRIBE message.
//		ppResp	 [out]	Response
//
//  Returns:    TRUE if successful, FALSE if not. GetLastError() contains the
//              error code.
//
//  
//
//  Notes:
//
BOOL FAddSubscriberFromRequest(SSDP_REQUEST * pRequest, PSTR *ppResp, DWORD dwIndex)
{
    BOOL                fResult = FALSE;
    UPNP_EVENT_SOURCE * pes;
    UPNP_SUBSCRIBER *   pSub = NULL;

    AssertSz(pRequest->Method == GENA_SUBSCRIBE, "I thought you told me "
             "this was a subscription request!");

    // First, do some checking on the message headers to see if this is a
    // valid subscription request
    //

    if (!pRequest->Headers[GENA_CALLBACK])
    {
        pRequest->status = HTTP_STATUS_BAD_REQUEST;
        TraceTag(ttidEvents, "Didn't get a Callback header!");
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanup;
    }

    if (!pRequest->RequestUri || !(*pRequest->RequestUri))
    {
        TraceTag(ttidEvents, "Didn't get a Reqeust-Uri!");
        pRequest->status = HTTP_STATUS_BAD_REQUEST;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanup;
    }

    EnterCriticalSection(&g_csListEventSource);
    pes = PesFindEventSource(pRequest->RequestUri);
    if (!pes)
    {
        pRequest->status =HTTP_STATUS_SERVER_ERROR;
        TraceTag(ttidEvents, "Subscription sent to unknown Request URI: %s ",
                 pRequest->RequestUri);

        // No event source registered matches this Request URI. We'll
        // retrun 404 Not Found.
        //
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanupL;
    }

    // Found the event source this SUBSCRIBE was sent to
    //
    pSub = (UPNP_SUBSCRIBER *) malloc(sizeof(UPNP_SUBSCRIBER));
    if (!pSub)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        goto cleanupL;
    }

    memset(pSub,0,sizeof(UPNP_SUBSCRIBER));
    // Validate the Callback header and look for at least one URL that
    // starts with "http://"
    //
    if (!FParseCallbackUrl(pRequest->Headers[GENA_CALLBACK], &pSub->szDestUrl))
    {
        TraceTag(ttidEvents, "Callback header '%s' was invalid!",
                 pRequest->Headers[GENA_CALLBACK]);
        pRequest->status = HTTP_STATUS_PRECOND_FAILED;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanupL;
    }

    DWORD dw;
    
	if(!FixURLAddressScopeId(pSub->szDestUrl, dwIndex, NULL, &(dw = 0)) && dw != 0)
        if(char* szDestUrl = (char*)malloc(dw))
            if(FixURLAddressScopeId(pSub->szDestUrl, dwIndex, szDestUrl, &dw))
            {            
                free(pSub->szDestUrl);
                pSub->szDestUrl = szDestUrl;
            }
            else
                free(szDestUrl);

    if(!g_pURL_Verifier->is_url_ok(pSub->szDestUrl))
    {
        TraceTag(ttidEvents, "Callback url '%s' is invalid.", pSub->szDestUrl);
        pRequest->status = HTTP_STATUS_PRECOND_FAILED;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanupL;
    }
    
    pSub->hEventCleanup = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Fill in some of the other members of the subscription struct
    //
    pSub->flags = 0;
    pSub->pes = pes;
    pSub->iSeq = 0;
    pSub->szSid = SzGetNewSid();
    if (!pSub->szSid)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        goto cleanupL;
    }
    
    if(pes->cProps)
    {
        pSub->rgesModified = (bool*)malloc(sizeof(bool) * pes->cProps);
        
        if (!pSub->rgesModified)
        {
            SetLastError(ERROR_OUTOFMEMORY);
            goto cleanupL;
        }
        
        pSub->cProps = pes->cProps;
    }

    pSub->dwTimerCookie = 0;
    pSub->fInTimer = FALSE;

    // Ok, now we have a valid subscription, so we need to send a response
    // to the subscriber

    SetSubscriptionTimeout(pSub, pRequest->Headers[GENA_TIMEOUT]);
    if (*ppResp = ComposeSubscribeResponse(pSub))
    {
        LPSTR      szSid;
        LPSTR      szDestUrl;
        DWORD       iSeq;

        // Must hold the event source lock across adding the subscriber and
        // sending the notify message
        //EnterCriticalSection(&pes->cs);

        szSid = _strdup(pSub->szSid);
        szDestUrl = _strdup(pSub->szDestUrl);
        iSeq = pSub->iSeq;

        // Ok we've added the subscription and notified so it's ok to allow
        // pre-empting now
#if DBG
        PrintSubscription(pSub);
#endif
        //PersistSubscriberList();

        AddToListSubscriber(pes, pSub);

        //LeaveCriticalSection(&pes->cs);
        LeaveCriticalSection(&g_csListEventSource);

		// tell UPNP device about this subscription
		// this callback also gives the device a chance to update the evented variables
		if (pfSubscribeCallback)
			if (!pfSubscribeCallback(TRUE, pRequest->RequestUri))
			{
				// subscription was vetoed
				RemoveFromListSubscriber(pSub);
				goto cleanup;
			}
       // hr = HrSendInitialNotifyMessage(pes, 0, szSid, iSeq, szDestUrl);

        free(szDestUrl);
        // hack to make the SID available to the caller
        // so that an initial event notification can be sent
        pRequest->Headers[GENA_SID] = szSid;

        // REVIEW: What to do if we can't send initial notify message??
        StartResubscribeTimer(pSub);

        fResult = TRUE;
    }
    else
    	goto cleanupL;

    TraceResult("FAddSubscriberFromRequest", fResult);
    return fResult;

cleanupL:
    LeaveCriticalSection(&g_csListEventSource);

cleanup:
    FreeUpnpSubscriber(pSub);

    return FALSE;
}
//+---------------------------------------------------------------------------
//
//  Function:   SendInitialEventNotification
//
//  Purpose:    Send the initial event notification to a subscriber
//				
//
DWORD SendInitialEventNotification(PSSDP_REQUEST pRequest)
{
    HRESULT		hr = E_FAIL;
    LPSTR		szBody = NULL;

    AssertSz(pRequest->Method == GENA_SUBSCRIBE, "I thought you told me "
             "this was a subscribe request!");
    AssertSz(pRequest->Headers[GENA_SID], "Why don't I have a SID header??!?!");

    if (!pRequest->RequestUri || !(*pRequest->RequestUri))
    {
        TraceTag(ttidEvents, "Didn't get a Reqeust-Uri!");
        FreeSsdpRequest(pRequest);
        delete pRequest;
        return 0;
    }

    EnterCriticalSection(&g_csListEventSource);

    // Find the event source based on the URI
    //
    if(UPNP_EVENT_SOURCE *pes = PesFindEventSource(pRequest->RequestUri))
    {
	    //EnterCriticalSection(&pes->cs);
	    PLIST_ENTRY		pListHead = &pes->listSubs;
		UPNP_SUBSCRIBER *pSub = NULL;

	    // Found the event source this SUBSCRIBE was sent to. Now find the
	    // subscriber.
	    for (PLIST_ENTRY p = pListHead->Flink; p != pListHead; p = p->Flink)
	    {
	        pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER, linkage);

	        if (!_stricmp(pSub->szSid, pRequest->Headers[GENA_SID]))
	        {
	            // Found the subscriber they were looking for!
	            break;
	        }
	    }

	    if (p != pListHead)
	    {
	        Assert(pSub);
			AssertSz(pSub->iSeq == 0, "Can't send initial notify if sequence number is not 0!");

            if (SUCCEEDED(hr = HrComposeXmlBodyFromEventSource(pes, pSub, pSub->iSeq == 0, &szBody)))
				HrSubmitEventToSubscriber(0, pSub->szSid, pSub->iSeq, szBody, pSub->szDestUrl);

			++pSub->iSeq;
	    }
	    //LeaveCriticalSection(&pes->cs);
	}

	LeaveCriticalSection(&g_csListEventSource);

	TraceError("Couldn't SendInitialEventNotification", hr);

    if (szBody)
        free(szBody);
        
    FreeSsdpRequest(pRequest);
    delete pRequest;
    
    return 0;
}

//+---------------------------------------------------------------------------
//
//  Function:   FRemoveSubscriberFromRequest
//
//  Purpose:    Given an UNSUBSCRIBE request, removes the subscription from
//              the list within the event source.
//
//  Arguments:
//      
//      pRequest [in]   Request to be processed
//      ppResp   [out]	Response
//
//  Returns:    TRUE if successful, FALSE if not. GetLastError() contains the
//              error code.
//
//  
//
//  Notes:
//
BOOL FRemoveSubscriberFromRequest(SSDP_REQUEST * pRequest, PSTR *ppResp)
{
    BOOL                fResult = FALSE;
    UPNP_EVENT_SOURCE * pes;
    PLIST_ENTRY         pListHead;
    PLIST_ENTRY         p;

    AssertSz(pRequest->Method == GENA_UNSUBSCRIBE, "I thought you told me "
             "this was an unsubscribe request!");

    // First, do some checking on the message headers to see if this is a
    // valid unsubscribe request
    //

    if (!pRequest->Headers[GENA_SID] || !(*(pRequest->Headers[GENA_SID])))
    {
        TraceTag(ttidEvents, "Didn't get a valid SID header!");
        pRequest->status = HTTP_STATUS_BAD_REQUEST;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanup;
    }

    if (!pRequest->RequestUri || !(*pRequest->RequestUri))
    {
        TraceTag(ttidEvents, "Didn't get a Reqeust-Uri!");
        pRequest->status = HTTP_STATUS_BAD_REQUEST;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanup;
    }

    EnterCriticalSection(&g_csListEventSource);

    // Find the event source based on the URI
    //
    pes = PesFindEventSource(pRequest->RequestUri);
    if (!pes)
    {
        TraceTag(ttidEvents, "Unsubscrbe sent to unknown Request URI: %s ",
                 pRequest->RequestUri);

        // No event source registered matches this Request URI. We'll
        // retrun 404 Not Found.
        //
        pRequest->status = HTTP_STATUS_SERVER_ERROR;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        goto cleanupL;
    }

    // Found the event source this SUBSCRIBE was sent to. Now find the
    // subscriber.
    //
    //EnterCriticalSection(&pes->cs);
    pListHead = &pes->listSubs;

    for (p = pListHead->Flink; p != pListHead; p = p->Flink)
    {
        UPNP_SUBSCRIBER * pSub;

        pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER, linkage);
        if (!_stricmp(pSub->szSid, pRequest->Headers[GENA_SID]))
        {
            TraceTag(ttidEvents, "Unsubscribing SID: %s ",
                     pRequest->Headers[GENA_SID]);

            // Found the subscriber they were looking for!
            //
            CleanupSubscriberEntry(pSub);
            //pRequest->status = HTTP_STATUS_OK;
            fResult = TRUE;
            break;
        }
    }

    //LeaveCriticalSection(&pes->cs);

    if (p == pListHead)
    {
        TraceTag(ttidEvents, "Unsubscribe sent to unknown SID: %s ",
                 pRequest->Headers[GENA_SID]);

        // Didn't find the subscriber. We should return 404 Not Found.
        pRequest->status = HTTP_STATUS_SERVER_ERROR;
        SetLastError(ERROR_INTERNET_ITEM_NOT_FOUND);
        fResult = FALSE;
    }

cleanupL:
    LeaveCriticalSection(&g_csListEventSource);
    // signal the unsubscribe to the upper layers
    if (fResult && pfSubscribeCallback)
    	pfSubscribeCallback(FALSE,pRequest->RequestUri);
cleanup:
    TraceResult("FRemoveSubscriberFromRequest", fResult);
    return fResult;
}

//+---------------------------------------------------------------------------
//
//  Function:   ResubscribeTimerProc
//
//  Purpose:    Callback that is called when the re-subscription timer has
//              run out.
//
//  Arguments:
//      pvContext   [in]  Context variable. This is the pSub pointer.
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
DWORD ResubscribeTimerProc(VOID *pvContext)
{
    UPNP_SUBSCRIBER * pSub = (UPNP_SUBSCRIBER *) pvContext;
    UPNP_EVENT_SOURCE *pes;
    LPSTR pstrUri;

	pSub->fInTimer = TRUE;
    pes = pSub->pes;

    TraceTag(ttidEvents, "ResubscribeTimerProc: Entering");

    EnterCriticalSection(&g_csListEventSource);
    //EnterCriticalSection(&(pes->cs));

    if (pes->fCleanup)
    {
        SetEvent(pSub->hEventCleanup);
        TraceTag(ttidEvents, "ResubscribeTimerProc: Signalling done event");
        pSub->fInTimer = FALSE;
        //LeaveCriticalSection(&(pes->cs));
        LeaveCriticalSection(&g_csListEventSource);
        return 0;
    }

    TraceTag(ttidEvents, "ResubscribeTimerProc: Leaving");

    RemoveFromListSubscriber(pSub);

	pstrUri = _strdup(pes->szRequestUri);
	
    //LeaveCriticalSection(&(pes->cs));
    LeaveCriticalSection(&g_csListEventSource);

	if (pstrUri && pfSubscribeCallback)
	{
		pfSubscribeCallback(FALSE, pstrUri);
		free(pstrUri);
	}
	pSub->fInTimer = FALSE;
    FreeUpnpSubscriber(pSub);
    
    return 0;
}

//+---------------------------------------------------------------------------
//
//  Function:   RemoveFromListSubscriber
//
//  Purpose:    Removes the given subscriber from the list within the event
//              source.
//
//  Arguments:
//      pSub [in]   Subscriber to be removed
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
VOID RemoveFromListSubscriber(UPNP_SUBSCRIBER * pSub)
{
    InterlockedDecrement(&g_nSubscribers);
    
    //EnterCriticalSection(&pes->cs);
    RemoveEntryList(&(pSub->linkage));
    //LeaveCriticalSection(&pes->cs);
}

//+---------------------------------------------------------------------------
//
//  Function:   StartResubscribeTimer
//
//  Purpose:    Starts the re-subscription timer for a subscription
//
//  Arguments:
//      pSub [in]   Subscription to start timer for
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
VOID StartResubscribeTimer(UPNP_SUBSCRIBER * pSub)
{
    UPNP_EVENT_SOURCE *     pes;

    pes = pSub->pes;

    //EnterCriticalSection(&(pes->cs));

    if(!(pSub->dwTimerCookie = g_pThreadPool->StartTimer(ResubscribeTimerProc, pSub, pSub->csecTimeout * 1000)))
    {
        TraceTag(ttidEvents, "Failed to start subscriber timer.");
        RemoveFromListSubscriber(pSub);
        FreeUpnpSubscriber(pSub);
    }
    else
    {
        TraceTag(ttidEvents, "Started subscriber timer for %d sec.",
                 pSub->csecTimeout);
    }

    //LeaveCriticalSection(&(pes->cs));
}

//+---------------------------------------------------------------------------
//
//  Function:   StopResubscribeTimer
//
//  Purpose:    Stops the resubscription timer for the given subscription
//
//  Arguments:
//      pSub [in]   Pointer to subscription
//
//  Returns:    Nothing
//
//  
//
//  Notes:      WARNING! The event source lock MUST be held before entering
//              this function!
//
VOID StopResubscribeTimer(UPNP_SUBSCRIBER * pSub)
{
    UPNP_EVENT_SOURCE *pes;

    pes = pSub->pes;
    pes->fCleanup = TRUE;

    TraceTag(ttidEvents, "Stopping Resubscribe timer for pSub 0x%08X", pSub);

    if (g_pThreadPool->StopTimer(pSub->dwTimerCookie))
    {
        TraceTag(ttidEvents, "Resubscribe timer stopped for pSub 0x%08X", pSub);
    }
    else
    {
        TraceTag(ttidEvents, "Resubscribe timer wasn't stoppable for"
                 " pSub 0x%08X", pSub);
	    if (pSub->fInTimer)
	    {
	        // Timer is running. Should rarely happen
	        TraceTag(ttidEvents, "Resubscribe timer is running, wait ...0x%08X", pSub);
	        //LeaveCriticalSection(&pes->cs);
	        WaitForSingleObject(pSub->hEventCleanup, INFINITE);
	        //EnterCriticalSection(&pes->cs);
	    }
	}

    pes->fCleanup = FALSE;
}

//+---------------------------------------------------------------------------
//
//  Function:   FreeUpnpSubscriber
//
//  Purpose:    Frees the memory used by a subscrption
//
//  Arguments:
//      pSub [in]   Subscription to free
//
//  Returns:    Nothing
//
//  
//
//  Notes:
//
VOID FreeUpnpSubscriber(UPNP_SUBSCRIBER* pSub)
{
    if (pSub)
    {
        TraceTag(ttidEvents, "Freeing Subscriber: 0x%08X", pSub);

        if (pSub->hEventCleanup)
        	CloseHandle(pSub->hEventCleanup);
		if (pSub->szDestUrl)
        	free(pSub->szDestUrl);
        if (pSub->szSid)
        	free(pSub->szSid);
        if (pSub->rgesModified)
        	free(pSub->rgesModified);
        free(pSub);
    }
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
//  
//
//  Notes:
//
VOID CleanupSubscriberEntry(UPNP_SUBSCRIBER * pSub)
{
    RemoveFromListSubscriber(pSub);
    StopResubscribeTimer(pSub);
    FreeUpnpSubscriber(pSub);
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
//  
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

#if DBG
VOID PrintSubscription(UPNP_SUBSCRIBER * pSub)
{
    TraceTag(ttidEvents, "Subscription at address 0x%08X", pSub);
    TraceTag(ttidEvents, "--------------------------------------");

    TraceTag(ttidEvents, "Subscription timeout is %d seconds from now.", pSub->csecTimeout);

    TraceTag(ttidEvents, "Sequence #  : %d", pSub->iSeq);
    TraceTag(ttidEvents, "Callback Url: %s", pSub->szDestUrl);
    TraceTag(ttidEvents, "SID         : %s", pSub->szSid);
    TraceTag(ttidEvents, "--------------------------------------");
}
#endif

//+---------------------------------------------------------------------------
//
//  Function:   DwParseTime
//
//  Purpose:    Parses the Timeout header for a subscription
//
//  Arguments:
//      szTime [in]     Timeout value in the format defined by RFC2518
//
//  Returns:    Timeout value in SECONDS
//
//  
//
//  Notes:  NYI
//
DWORD DwParseTime(LPCSTR szTime)
{
    DWORD dwTimeoutSeconds;

	if(0 == strcmp(szTime, "Second-infinite"))
		dwTimeoutSeconds = upnp_config::max_sub_timeout();
	else
	{
		if(1 != sscanf(szTime, "Second-%d", &dwTimeoutSeconds))
			dwTimeoutSeconds = upnp_config::default_sub_timeout();

		if(dwTimeoutSeconds < upnp_config::min_sub_timeout())
			dwTimeoutSeconds = upnp_config::min_sub_timeout();

		if(dwTimeoutSeconds > upnp_config::max_sub_timeout())
			dwTimeoutSeconds = upnp_config::max_sub_timeout();
	}
			
	return dwTimeoutSeconds;
}


//+---------------------------------------------------------------------------
//
//  Function:   FParseCallbackUrl
//
//  Purpose:    Given the Callback URL header value, determines the first
//              http:// URL.
//
//  Arguments:
//      szCallbackUrl [in]  URL to process (comes from Callback: header)
//      pszOut        [out] Returns the callback URL
//
//  Returns:    TRUE if URL format is valid, FALSE if not or if out of
//              memory.
//
//  
//
//  Notes:
//
BOOL FParseCallbackUrl(LPCSTR szCallbackUrl, LPSTR *pszOut)
{
    CONST INT   c_cchPrefix = strlen(c_szUrlPrefix);
    LPSTR      szTemp;
    LPSTR      pchPos;
    LPSTR      szOrig = NULL;
    BOOL        fResult = FALSE;

    // NOTE: This function will return http:// as a URL.. Corner case, but
    // not catastrophic

    Assert(szCallbackUrl);
    Assert(pszOut);

    *pszOut = NULL;

    // Copy the original URL so we can lowercase
    //
    szTemp = _strdup(szCallbackUrl);
    szOrig = szTemp;
    _strlwr(szTemp);

    // Look for http://
    //
    pchPos = strstr(szTemp, c_szUrlPrefix);
    if (pchPos)
    {
        // Look for the closing '>'
        szTemp = pchPos + c_cchPrefix;
        while (*szTemp && *szTemp != '>')
        {
            szTemp++;
        }

        if (*szTemp)
        {
            INT     cchOut;

            Assert(*szTemp == '>');

            // Allocate space for the URL, and copy it in
            //
            cchOut = szTemp - pchPos;   //georgej: removed + 1
            *pszOut = (LPSTR) malloc(cchOut + 1);
            if (*pszOut)
            {
                strncpy(*pszOut, szCallbackUrl + (pchPos - szOrig), cchOut);
                (*pszOut)[cchOut] = 0;
                fResult = TRUE;
            }
        }
    }

    free(szOrig);

    TraceResult("FParseCallbackUrl", fResult);
    return fResult;
}


//+---------------------------------------------------------------------------
//
//  Function:   SzGetNewSid
//
//  Purpose:    Returns a new "uuid:<sid>" identifier
//
//  Arguments:
//      (none)
//
//  Returns:    Newly allocated SID string
//
//  
//
//  Notes:      Caller must free the returned string
//
LPSTR SzGetNewSid()
{
#ifndef UNDER_CE
    CHAR           szSid[256];
    UUID            uuid;
    unsigned char * szUuid;

    UuidCreate(&uuid);
    UuidToStringA(&uuid, &szUuid);
    sprintf(szSid, "uuid:%s", szUuid);
    RpcStringFreeA(&szUuid);
    return _strdup(szSid);
#else
	PCHAR	pszSid = (PCHAR)malloc(42);
	LONGLONG uuid64 = GenerateUUID64();
	if (pszSid)
		sprintf(pszSid, "uuid:%04x%04x-%04x-%04x-0000-000000000000", (WORD)uuid64, *((WORD*)&uuid64 + 1), *((WORD*)&uuid64 + 2), *((WORD*)&uuid64 + 3));
	return pszSid;
#endif
}

