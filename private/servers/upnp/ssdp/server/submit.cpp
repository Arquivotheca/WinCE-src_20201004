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
//  File:       S U B M I T . C P P
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


#include "ssdpnetwork.h"
#include "ssdpsrv.h"
#include "dlna.h"
#include "HttpChannel.h"
#include "HttpRequest.h"

#include "string.hxx"

extern LONG cInitialized;

extern const CHAR c_szHttpVersion[];
extern const CHAR c_szNt[];
extern const CHAR c_szNts[];
extern char *SsdpHeaderStr[];

const CHAR c_szNotify[]     = "NOTIFY";
const CHAR c_szColon[]      = ":";
const CHAR c_szCrlf[]       = "\r\n";
const CHAR c_szTextXML[]   = "text/xml";


// Events are appropriate to a subscriber if
// The event is a state-dump and the subscriber is new,
// Or, the event has recent modifications, and the subscriber is ongoing.
inline bool IsAppropriateSub(bool bModified, int iSeq)
{
    // old subscriber and these are changes.
    if (iSeq != 0 && bModified)
    {
        return true;
    }
    // new subscriber and this is a dump of state info.
    if (iSeq == 0 && !bModified)
    {
        return true;
    }
    // All other cases are of no interest
    return false;
}

// Provide a set of updated property information
BOOL WINAPI SubmitUpnpPropertyEvent(
    /* [string][in] */ LPCSTR szEventSourceUri,
    /* [in] */ DWORD dwFlags,
    /* [in] */ DWORD cProps,
    /* [in] */ UPNP_PROPERTY __RPC_FAR *rgProps)
{
    DWORD               dwError = NO_ERROR;
    UPNP_EVENT_SOURCE * pes;
    HRESULT             hr = S_OK;
    BOOL                fRet;

    if (InterlockedExchange(&cInitialized, cInitialized) == 0)
    {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    // Validate params
    //
    if (!cProps || !rgProps || !szEventSourceUri || !*szEventSourceUri)
    {
        TraceTag(ttidError, "_SubmitUpnpPropertyEventRpc: error %ld.",
                   HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    EnterCriticalSection(&g_csListEventSource);

    // Get the event source that the URI refers to. If we don't find it,
    // return an error.
    //
    pes = PesFindEventSource(szEventSourceUri);
    if (!pes)
    {
        TraceTag(ttidError, "_SubmitUpnpPropertyEventRpc: error %ld.",
                   HRESULT_FROM_WIN32(ERROR_INTERNET_ITEM_NOT_FOUND));
        LeaveCriticalSection(&g_csListEventSource);
        SetLastError( ERROR_INTERNET_ITEM_NOT_FOUND);
        return FALSE;
    }

    // Take the props passed in and update the event source's cache of these
    // prop values. Mark each value that changed as modified.
    //
    if (fRet = UpdateEventSourceWithProps(pes, dwFlags, cProps, rgProps))
    {
        // Notify each of the subs on this event
        for (PLIST_ENTRY p = pes->listSubs.Flink; p != &pes->listSubs; p = p->Flink)
        {
            UPNP_SUBSCRIBER *pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER, linkage);
            if (IsAppropriateSub(dwFlags & F_MARK_MODIFIED, pSub->iSeq))
            {
                SetEvent(pSub->hEventSend);
            }
        }
    }
    else
    {
        TraceTag(ttidError, "_SubmitUpnpPropertyEventRpc:"
                 "UpdateEventSourceWithPropserror %ld.",
                   HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));
        SetLastError( ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    }
    LeaveCriticalSection(&g_csListEventSource);
    return fRet;
}



// Process new property information
BOOL UpdateEventSourceWithProps(UPNP_EVENT_SOURCE *pes, DWORD dwFlags,
                                 DWORD cProps, UPNP_PROPERTY *rgProps)
{
    DWORD       iProp;
    BOOL        fResult = FALSE;
    if (!pes->cProps)
    {
        // Initialize the property list. Should be the complete set of evented variables
        Assert(!pes->rgesProps);
        Assert(!(dwFlags & F_MARK_MODIFIED))
        
        if(cProps < ULONG_MAX / sizeof(UPNP_PROPERTY)) // protect against integer overflow
        {
            pes->rgesProps = (UPNP_PROPERTY *) malloc(sizeof(UPNP_PROPERTY) * cProps);
        }
        
        if(!pes->rgesProps)
        {
            return FALSE;
        }

        // Copy in property information
        //
        for (iProp = 0; iProp < cProps; iProp++)
        {
            if(CopyUpnpProperty(&pes->rgesProps[pes->cProps], &rgProps[iProp]))
            {
                pes->cProps++;
            }
        }
        
        fResult = TRUE;
        
        // Set modified flag for all properties for all subscribers
        for (PLIST_ENTRY p = pes->listSubs.Flink; p != &pes->listSubs; p = p->Flink)
        {
            UPNP_SUBSCRIBER *pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER, linkage);
            pSub->rgesModified = (bool*) malloc(sizeof(bool) * cProps);
            memset(pSub->rgesModified, 0, sizeof(bool) * cProps);
            pSub->cProps = cProps;
        }
    }
    else
    {
        for (iProp = 0; iProp < cProps; iProp++)
        {
            DWORD           iDstProp;
            UPNP_PROPERTY*  rgesProps = pes->rgesProps;

            for (iDstProp = 0; iDstProp < pes->cProps; iDstProp++)
            {
                if (!_wcsicmp(rgProps[iProp].szName,
                            rgesProps[iDstProp].szName))
                {
                    Assert(rgProps[iProp].szValue);

                    if(wchar_t* szValue = _wcsdup(rgProps[iProp].szValue))
                    {
                        // Free the old value
                        free(rgesProps[iDstProp].szValue);

                        // And copy in the new one
                        rgesProps[iDstProp].szValue = szValue;
                        
                        // Mark the property as modified for all subscribers
                        for (PLIST_ENTRY p = pes->listSubs.Flink; p != &pes->listSubs; p = p->Flink)
                        {
                            UPNP_SUBSCRIBER *pSub = CONTAINING_RECORD (p, UPNP_SUBSCRIBER, linkage);
                            if (IsAppropriateSub(dwFlags & F_MARK_MODIFIED, pSub->iSeq))
                            {
                                pSub->rgesModified[iDstProp] = true;
                            }
                        }
                        
                        fResult = TRUE;
                    }
                    break;
                }
            }
        }
    }
    
    return fResult;
}



static const DWORD dwSubscribeTimeout = 5 * 60 * 1000;
static const DWORD dwModerationSpecTime = 200;

// Each subscriber has a thread standing by to make it calls
DWORD WINAPI SubscriberSendThread(UPNP_SUBSCRIBER *pSub)
{
    LPCSTR pNotify = NULL;
    HRESULT hr = S_OK;
    DWORD dwModerationSleepTime = 0;
    DWORD dwSubscribeSleepTime = 0;
    DWORD dwElapsedTime = 0;
    DWORD iSeq = 0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // Check input
    ChkBool (pSub, E_POINTER);
    UPNP_EVENT_SOURCE * pes = pSub->pes;
    ChkBool (pes, E_UNEXPECTED);

    // Set initial time history to succeed
    pSub->dwLastSendTime = GetTickCount() - dwModerationSpecTime;

    for (;;)
    {
        // Free previous notify message - if any
        if (pNotify)
        {
            free((void*)pNotify);
            pNotify = NULL;
        }

        // Check for termination
        ChkBool(!pSub->bShutdown, S_OK)

        // Check for timeout
        dwElapsedTime = GetTickCount() - pSub->dwSubscriptionTime;
        ChkBool(dwElapsedTime < dwSubscribeTimeout, S_FALSE);
        dwSubscribeSleepTime = dwSubscribeTimeout - dwElapsedTime;

        // Wait til woken or moderates
        if (dwModerationSleepTime)
        {
            Sleep(dwModerationSleepTime);
            dwModerationSleepTime = 0;
        }
        else
        {
            DWORD dwResult = WaitForSingleObject(pSub->hEventSend, dwSubscribeSleepTime);
            switch(dwResult)
            {
                case WAIT_TIMEOUT:  continue;  // subscription expired.  recheck values.
                case WAIT_OBJECT_0:  break;    // Event waiting.  Send it
                default:  ChkBool(0, S_FALSE); // failure.  quit
            }
        }

        // Check for termination
        ChkBool(!pSub->bShutdown, S_OK)

        // Check the Moderation Interval
        dwElapsedTime = GetTickCount() - pSub->dwLastSendTime;
        if (dwElapsedTime < dwModerationSpecTime)
        {
            dwModerationSleepTime = dwModerationSpecTime - dwElapsedTime;
            continue;
        }

        // Check for notifies - this is done locked
        EnterCriticalSection(&g_csListEventSource);
        iSeq = pSub->iSeq;                  // get current sequence number before it is changed
        pes = PesVerifyEventSource(pes);
        HRESULT hr = HrComposeXmlBodyFromEventSource(pes, pSub, &pNotify);
        LeaveCriticalSection(&g_csListEventSource);

        // go back to sleep if there is nothing to do.
        if (FAILED(hr) || !pNotify)
        {
            continue;
        }

        // Send out notify
        hr = HrSubmitEventToSubscriber(0, pSub->dwEventTimeout, pSub->szSid, iSeq, 
                                       pNotify, pSub->dwChannelCookie, pSub->szDestUrl);
        pSub->dwLastSendTime = GetTickCount();

        // If the event message has timed out mark the subscriber as non-responsive and decrease the timeout
        if(hr == HRESULT_FROM_WIN32(ERROR_INTERNET_TIMEOUT))
        {
            // cut the time by half but don't go below MINIMAL_EVENT_TIMEOUT
            pSub->dwEventTimeout /= 2;
            if(pSub->dwEventTimeout < MINIMAL_EVENT_TIMEOUT)
            {
                pSub->dwEventTimeout = MINIMAL_EVENT_TIMEOUT;
            }
        }
        else
        {
            // restore DEFAULT_EVENT_TIMEOUT
            pSub->dwEventTimeout = DEFAULT_EVENT_TIMEOUT;
        }
    }

Cleanup:
    pSub->bThreaded = false;    // Necessary to get full cleanup
    CleanupSubscriberEntry(pSub);
    CoUninitialize();
    return 0;
}


const WCHAR c_wszPropertySetBegin[] = L"<e:propertyset xmlns:e=\"urn:schemas-upnp-org:event-1-0\">";
const WCHAR c_wszPropertySetEnd[]   = L"</e:propertyset>";

const WCHAR c_wszPropertyBegin[]    = L"<e:property>";
const WCHAR c_wszPropertyEnd[]      = L"</e:property>";

//+---------------------------------------------------------------------------
//
//  Function:   HrComposeXmlBodyFromEventSource
//
//  Purpose:    Creates an XML message to be sent along with a NOTIFY. The
//              message contains the property changes according to the
//              example schema below.
//
//  Arguments:
//      pes    [in]     Pointer to event source
//      iSeq   [in]     Sequence number of subscriber
//      pszOut [out]    Returns XML body as a null-term string
//
//  Returns:    S_OK if success, E_OUTOFMEMORY if no memory or other
//              INTERNET_* errors
//
//  Author:     danielwe   12 Oct 1999
//
//  Notes:      This function knows if a subscriber has not been sent an
//              initial update and if so, submits all properties for
//              notification.
//
//              XML schema is as follows:
//
//             <e:propertyset xmlns:U="urn:schemas-upnp-org:event-1-0">
//                 <e:property>
//                     <U:foo>
//                         goodbye
//                     </U:foo>
//                 </e:property>
//                 <e:property>
//                     <U:bar>
//                         27
//                     </U:bar>
//                 </e:property>
//             </e:propertyset>
//
HRESULT HrComposeXmlBodyFromEventSource(UPNP_EVENT_SOURCE *pes,
                                        UPNP_SUBSCRIBER *pSub,
                                        LPCSTR *ppData)
{
    HRESULT     hr = E_OUTOFMEMORY;
    DWORD       iProp;
    ce::wstring strEvent, strEncodedValue;
    LPSTR pszOut = NULL;
    
    if (!pes || ! pSub || !ppData)
    {
        Assert(0);
        goto cleanup;
    }

    //
    // Must re-acquire the list lock and verify that this event source is
    // still in the list. If not, return an error and leave.
    //
    EnterCriticalSection(&g_csListEventSource);
    
    pes = PesVerifyEventSource(pes);
    if (!pes)
    {
        hr = HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
        goto cleanup;
    }
    
    // <e:propertyset xmlns:e="upnp">
    if (!strEvent.assign(c_wszPropertySetBegin))
    {
        goto cleanup;
    }
    
    // Add modified/all evented state variables
    for (iProp = 0; iProp < pes->cProps; iProp++)
    {
        UPNP_PROPERTY * pProp = &pes->rgesProps[iProp];

        if ((!(pes->rgesProps[iProp].dwFlags & UPF_NON_EVENTED)) &&
            ((pSub->rgesModified[iProp] || pSub->iSeq == 0)))
        {
            if(!EncodeForXML(pProp->szValue, &strEncodedValue))
            {
                goto cleanup;
            }
            
            // <e:property>
            if(!strEvent.append(c_wszPropertyBegin))
            {
                goto cleanup;
            }
                
            // <foo>
            if(!strEvent.append(L"  <"))
            {
                goto cleanup;
            }
                
            if(!strEvent.append(pProp->szName))
            {
                goto cleanup;
            }
                
            if(!strEvent.append(L">"))
            {
                goto cleanup;
            }
                
            //     goodbye
            if(!strEvent.append(strEncodedValue))
            {
                goto cleanup;
            }
                
            // </foo>
            if(!strEvent.append(L"</"))
            {
                goto cleanup;
            }
                
            if(!strEvent.append(pProp->szName))
            {
                goto cleanup;
            }
                
            if(!strEvent.append(L">"))
            {
                goto cleanup;
            }
                
            // </e:property>
            if(!strEvent.append(c_wszPropertyEnd))
            {
                goto cleanup;
            }
        }
    }
    
    // </e:propertyset>
    if (!strEvent.append(c_wszPropertySetEnd))
    {
        goto cleanup;
    }
        
    // convert event body to UTF8
    int     cb;
    UINT    cp = CP_UTF8;
    
    cb = WideCharToMultiByte(cp, 0, strEvent, -1, NULL, 0, 0, 0);

    if(!cb)
    {
        if(cb = WideCharToMultiByte(CP_ACP, 0, strEvent, -1, NULL, 0, 0, 0))
        {
            cp = CP_ACP;
        }
    }
    
    if(pszOut = (LPSTR)malloc(cb))
    {
        WideCharToMultiByte(cp, 0, strEvent, -1, pszOut, cb, 0, 0);
    }
    else
    {
        goto cleanup;
    }
    
    // Mark all properties as not modified for this subscriber
    for (iProp = 0; iProp < pes->cProps; iProp++)
    {
        pSub->rgesModified[iProp] = false;
    }
    
    // Deliver the prepared string
    *ppData = pszOut;

    // increment the subscriber's sequence number - handle overflow.  
    if(++pSub->iSeq == 0)
    {
        pSub->iSeq = 1;
    }

    // Success.
    hr = S_OK;

cleanup:
    LeaveCriticalSection(&g_csListEventSource);
    TraceError("HrComposeXmlBodyFromEventSource", hr);
    return hr;
}


// Enough talk! send the notify!
HRESULT HrSubmitEventToSubscriber(DWORD dwFlags, DWORD dwTimeout,
                                  LPCSTR szSid, DWORD iSeq, LPCSTR szEventBody,
                                  DWORD dwChannelCookie, LPCSTR szUrl)
{
    assert(dwTimeout >= MINIMAL_EVENT_TIMEOUT && dwTimeout <= DEFAULT_EVENT_TIMEOUT);
    char         pszSeq[16];
    DWORD        dwRequestCookie = 0;
    HttpRequest *pRequest = NULL;
    HRESULT hr = S_OK;

    // Check that we have a channel and get a request
    Chk(g_CJHttpRequest.Alloc(&dwRequestCookie));
    Chk(g_CJHttpRequest.Acquire(dwRequestCookie, pRequest));
    ChkBool(pRequest != NULL, E_UNEXPECTED);

    // prepare NOTIFY Request
    Chk(pRequest->Init(dwChannelCookie, szUrl));
    Chk(pRequest->Open("NOTIFY"));

    // add SID header   TODO - check the returns
    sprintf_s(pszSeq, "%15d", iSeq, _countof(pszSeq));
    Chk(pRequest->AddHeader("NT", "upnp:event"));
    Chk(pRequest->AddHeader("NTS", "upnp:propchange"));
    Chk(pRequest->AddHeader("SID", szSid));
    Chk(pRequest->AddHeader("SEQ", pszSeq));
    Chk(pRequest->AddHeader("CONTENT-TYPE", "text/xml; charset=\"utf-8\""));
    Chk(pRequest->AddHeader("CONTENT-LENGTH", strlen(szEventBody)));

    // write Request body
    Chk(pRequest->Write(szEventBody));

    // send Request
    Chk(pRequest->Send());
    ChkBool(hr == S_OK, E_FAIL);
        
Cleanup:
    g_CJHttpRequest.Release(dwRequestCookie, pRequest);
    g_CJHttpRequest.Free(dwRequestCookie);
    return hr;
}

