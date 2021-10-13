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
//  File:       E V E N T . H
//
//  Contents:   Private eventing functions
//
//  Notes:
//
//  Author:     danielwe   14 Oct 1999
//
//----------------------------------------------------------------------------


#include "HttpChannel.h"

inline ULONGLONG ULONGLONG_FROM_FILETIME(FILETIME ft) 
{
    return (*((ULONGLONG *)&(ft))); 
}

inline FILETIME FILETIME_FROM_ULONGLONG(ULONGLONG ll) 
{
    return (*((FILETIME *)&(ll)));
}

#ifndef Chk
#define Chk(val) { hr = (val); if (FAILED(hr)) goto Cleanup; }
#endif

#ifndef ChkBool
#define ChkBool(val, x) { if (!(val)) { hr = x; goto Cleanup; } }
#endif

//
// Controlled Device structures
//

struct _UPNP_EVENT_SOURCE;

#define DEFAULT_EVENT_TIMEOUT       30
#define MINIMAL_EVENT_TIMEOUT       3


typedef struct _UPNP_SUBSCRIBER
{
    LIST_ENTRY          linkage;
    DWORD               dwEventTimeout;     // how long to wait when sending event to the subscriber
                                            // this value is decreased when subscriber is not responding
    struct _UPNP_EVENT_SOURCE * pes;
    LPSTR               szDestUrl;          // URL to send NOTIFYs to
    DWORD               csecTimeout;        // Timeout period
    DWORD               iSeq;               // Event sequence number
    LPSTR               szSid;              // Subscription Identifier (UUID)
    bool                bShutdown;          // Flag that subscriber is being removed.
    DWORD               dwLastSendTime;     // ms time of last transmission
    DWORD               cProps;             // Number of properties supported
    HANDLE              hEventSend;         // To wake up the sending thread
    bool                bThreaded;          // A thread is running for this subscriber
    DWORD               dwChannelCookie;    // Persistent connection channel reference
    DWORD               dwSubscriptionTime; // Time of latest resubscribe
    bool*               rgesModified;       // Array of Modified flags for properties
                                            // rgesModified[n] is true if the subscriber hasn't 
                                            // been notified about current value of the property n
} UPNP_SUBSCRIBER;

    
typedef struct _UPNP_EVENT_SOURCE
{
    LIST_ENTRY          linkage;
    LPSTR               szRequestUri;       // URI that identifies subscriptions
                                            // SUBSCRIBE and UNSUBSCRIBE to
    DWORD               cProps;             // Number of properties supported
                                            // by the event source
    UPNP_PROPERTY*      rgesProps;          // Array of properties
    LIST_ENTRY          listSubs;           // List of subscribers (UPNP_SUBSCRIBER)
    //CRITICAL_SECTION    cs;
    BOOL                fCleanup;
} UPNP_EVENT_SOURCE;

// Type of subscription request to send
typedef enum _ESSR_TYPE
{
    SSR_SUBSCRIBE,
    SSR_RESUBSCRIBE,
    SSR_UNSUBSCRIBE,
} ESSR_TYPE;



void SendNotifyToSub(UPNP_SUBSCRIBER *pSub, LPCSTR pEvent);
DWORD WINAPI SubscriberSendThread(UPNP_SUBSCRIBER *pSub);
void InitializeListEventSource(VOID);
BOOL CleanupListEventSource();
DWORD AddSubscriberFromRequest(SSDP_REQUEST * pRequest, PSTR *ppResp, DWORD dwIndex, LPSTR *pNewSid);
DWORD AddSubscriberFromRequestFinal(LPCSTR pUri, LPCSTR pSid);
BOOL ValidateUpnpProperty(UPNP_PROPERTY * pProp);
BOOL CopyUpnpProperty(UPNP_PROPERTY * pPropDst, UPNP_PROPERTY * pPropSrc);
VOID FreeUpnpProperty(UPNP_PROPERTY * pPropSrc);
VOID FreeEventSource(UPNP_EVENT_SOURCE *pes);
VOID RemoveFromListEventSource(UPNP_EVENT_SOURCE *pes);
UPNP_EVENT_SOURCE * PesFindEventSource(LPCSTR szRequestUri);
UPNP_SUBSCRIBER   * SubFindSubscriber(LPCSTR pUri, LPCSTR pSid);
UPNP_EVENT_SOURCE * PesVerifyEventSource(UPNP_EVENT_SOURCE *pes);
VOID PrintListEventSource(LIST_ENTRY *pListHead);
VOID CleanupEventSourceEntry (UPNP_EVENT_SOURCE *pes);
VOID PrintEventSource(const UPNP_EVENT_SOURCE *pes);
bool EncodeForXML(LPCWSTR pwsz, ce::wstring* pstr);

DWORD ProcessSubscribeRequest(SSDP_REQUEST * pRequest, PSTR *ppResp, UPNP_SUBSCRIBER **ppSub, DWORD dwIndex);
DWORD ProcessResubscribeRequest(SSDP_REQUEST * pRequest, PSTR *ppResp);
DWORD RemoveSubscriberFromRequest(SSDP_REQUEST * pRequest);

VOID RemoveFromListSubscriber(UPNP_SUBSCRIBER * pSub);
VOID StartResubscribeTimer(UPNP_SUBSCRIBER * pSub);
VOID StopResubscribeTimer(UPNP_SUBSCRIBER * pSub);
VOID CleanupSubscriberEntry(UPNP_SUBSCRIBER * pSub);
VOID CleanupListSubscriber(PLIST_ENTRY pListHead);
DWORD DwParseTime(LPCSTR szTime);
BOOL ParseCallbackUrl(LPCSTR szCallbackUrl, LPSTR *pszOut);
VOID SetSubscriptionTimeout(UPNP_SUBSCRIBER * pSub, LPCSTR szTimeoutHeader);
LPSTR SzGetNewSid(VOID);
BOOL SendSubscribeResponse(SOCKET socket, UPNP_SUBSCRIBER* pSub);
VOID AddToListSubscriber(UPNP_EVENT_SOURCE *pes, UPNP_SUBSCRIBER *pSub);

HRESULT HrSubmitEventToSubscriber(DWORD dwFlags, DWORD dwTimeout,
                                  LPCSTR szSid, DWORD iSeq, LPCSTR szEventBody,
                                  DWORD dwChannelCookie, LPCSTR szUrl);
BOOL UpdateEventSourceWithProps(UPNP_EVENT_SOURCE *pes, DWORD flags, DWORD cProps,
                                 UPNP_PROPERTY *rgProps);
HRESULT HrComposeXmlBodyFromEventSource(UPNP_EVENT_SOURCE *pes, UPNP_SUBSCRIBER *pSub, LPCSTR *ppNotify);
VOID IncrementSequenceNumber(UPNP_EVENT_SOURCE *pes, LPCSTR szSid);
VOID StopClientResubscribeTimer(PSSDP_NOTIFY_REQUEST pRequest);
DWORD ClientResubscribeTimerProc(VOID *pvContext);
VOID StartClientResubscribeTimer(PSSDP_NOTIFY_REQUEST pRequest);

extern LIST_ENTRY          g_listEventSource;
extern CRITICAL_SECTION    g_csListEventSource;
extern HANDLE              g_hEvtEventSource;
extern LONG                g_nSubscribers;
extern DWORD               g_fShutdownEventThread;

