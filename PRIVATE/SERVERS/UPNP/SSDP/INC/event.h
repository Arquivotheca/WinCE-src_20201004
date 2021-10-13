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
//  File:       E V E N T . H
//
//  Contents:   Private eventing functions
//
//  Notes:
//
//  
//
//----------------------------------------------------------------------------

inline ULONGLONG ULONGLONG_FROM_FILETIME(FILETIME ft) 
{
    return (*((ULONGLONG *)&(ft))); 
}

inline FILETIME FILETIME_FROM_ULONGLONG(ULONGLONG ll) 
{
    return (*((FILETIME *)&(ll)));
}
//
// Controlled Device structures
//

struct _UPNP_EVENT_SOURCE;

#define 	F_UPNPSUB_PENDINGEVENT			0x00000001
#define		F_UPNPSUB_NOTRESPONDING			0x00000002

typedef struct _UPNP_SUBSCRIBER
{
    LIST_ENTRY          linkage;
    DWORD				flags;				// see F_UPNPSUB_xxx flags above
    struct _UPNP_EVENT_SOURCE * pes;
    LPSTR               szDestUrl;          // URL to send NOTIFYs to
    DWORD               csecTimeout;        // Timeout period
    DWORD               iSeq;               // Event sequence number
    LPSTR               szSid;              // Subscription Identifier (UUID)
    DWORD               dwTimerCookie;      // re-subscription timer
    BOOL                fInTimer;           // Set if executing timer callback
    HANDLE              hEventCleanup;
    DWORD               cProps;             // Number of properties supported
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

BOOL InitializeListEventSource(VOID);
BOOL CleanupListEventSource();
BOOL FAddSubscriberFromRequest(SSDP_REQUEST * pRequest, PSTR *ppResp, DWORD dwIndex);
BOOL FValidateUpnpProperty(UPNP_PROPERTY * pProp);
VOID CopyUpnpProperty(UPNP_PROPERTY * pPropDst, UPNP_PROPERTY * pPropSrc);
VOID FreeUpnpProperty(UPNP_PROPERTY * pPropSrc);
VOID FreeEventSource(UPNP_EVENT_SOURCE *pes);
VOID RemoveFromListEventSource(UPNP_EVENT_SOURCE *pes);
UPNP_EVENT_SOURCE * PesFindEventSource(LPCSTR szRequestUri);
UPNP_EVENT_SOURCE * PesVerifyEventSource(UPNP_EVENT_SOURCE *pes);
VOID PrintListEventSource(LIST_ENTRY *pListHead);
VOID CleanupEventSourceEntry (UPNP_EVENT_SOURCE *pes);
VOID PrintEventSource(const UPNP_EVENT_SOURCE *pes);
BOOL FRemoveSubscriberFromRequest(SSDP_REQUEST * pRequest,PSTR *ppResp);

BOOL FProcessResubscribeRequest(SSDP_REQUEST * pRequest, PSTR *ppResp);
BOOL FProcessSubscribeRequest(SSDP_REQUEST * pRequest, PSTR *ppResp, BOOL *pfNotifyNeeded, DWORD dwIndex);
DWORD SendInitialEventNotification(SSDP_REQUEST *pRequest);

DWORD ResubscribeTimerProc (VOID *Arg);
VOID RemoveFromListSubscriber(UPNP_SUBSCRIBER * pSub);
VOID StartResubscribeTimer(UPNP_SUBSCRIBER * pSub);
VOID StopResubscribeTimer(UPNP_SUBSCRIBER * pSub);
VOID FreeUpnpSubscriber(UPNP_SUBSCRIBER* pSub);
VOID CleanupSubscriberEntry(UPNP_SUBSCRIBER * pSub);
VOID CleanupListSubscriber(PLIST_ENTRY pListHead);
#if DBG
VOID PrintSubscription(UPNP_SUBSCRIBER * pSub);
#endif
DWORD DwParseTime(LPCSTR szTime);
BOOL FParseCallbackUrl(LPCSTR szCallbackUrl, LPSTR *pszOut);
VOID SetSubscriptionTimeout(UPNP_SUBSCRIBER * pSub, LPCSTR szTimeoutHeader);
LPSTR SzGetNewSid(VOID);
BOOL FSendSubscribeResponse(SOCKET socket, UPNP_SUBSCRIBER* pSub);
VOID AddToListSubscriber(UPNP_EVENT_SOURCE *pes, UPNP_SUBSCRIBER *pSub);

HRESULT HrSendInitialNotifyMessage(UPNP_EVENT_SOURCE *pes, DWORD dwFlags,
                                   LPCSTR szSid, DWORD iSeq, LPCSTR szDestUrl);
HRESULT HrSubmitEventToSubscriber(DWORD dwFlags,
                                  LPCSTR szSid, DWORD iSeq, LPCSTR szEventBody,
                                  LPCSTR szDestUrl);
BOOL FUpdateEventSourceWithProps(UPNP_EVENT_SOURCE *pes, DWORD cProps,
                                 UPNP_PROPERTY *rgProps);
HRESULT HrComposeXmlBodyFromEventSource(UPNP_EVENT_SOURCE *pes,
                                        UPNP_SUBSCRIBER *pSub,
                                        BOOL fAllProps,
                                        LPSTR *pszOut);
VOID IncrementSequenceNumber(UPNP_EVENT_SOURCE *pes, LPCSTR szSid);
VOID StopClientResubscribeTimer(PSSDP_NOTIFY_REQUEST pRequest);
DWORD ClientResubscribeTimerProc(VOID *pvContext);
VOID StartClientResubscribeTimer(PSSDP_NOTIFY_REQUEST pRequest);

extern LIST_ENTRY          g_listEventSource;
extern CRITICAL_SECTION    g_csListEventSource;
extern HANDLE          g_hEvtEventSource;
extern LONG g_nSubscribers;

