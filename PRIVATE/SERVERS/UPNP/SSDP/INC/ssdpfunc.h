//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


File Name:

    ssdpfunc.h

Abstract:

    This file contains cross files function prototypes.



Created: 07/15/1999

--*/
#ifndef _SSDPFUNC_
#define _SSDPFUNC_

#include "ssdptypes.h"
#include "ssdpnetwork.h"

extern LONG cInitialized;

VOID ReadCacheFileToList();

VOID WriteListCacheToFile();

// list related functions
VOID InitializeListAnnounce();

VOID CleanupListAnnounce();

BOOL IsInListAnnounce(CHAR *szUSN);

PCONTEXT_HANDLE_TYPE *GetServiceByUSN(CHAR *szUSN);

VOID SearchListAnnounce(SSDP_REQUEST *SsdpMessage, SOCKET sockRecv, PSOCKADDR_STORAGE RemoteAddress);

PSSDP_SERVICE AddToListAnnounce(SSDP_MESSAGE *pssdpMsg, DWORD flags,
                                PCONTEXT_HANDLE_TYPE *pphContext);

VOID RemoveFromListAnnounce(SSDP_SERVICE *pssdpSvc);

VOID StartAnnounceTimer(SSDP_SERVICE *pssdpSvc, LPTHREAD_START_ROUTINE pCallback);

VOID StopAnnounceTimer(SSDP_SERVICE *pssdpSvc);

BOOL SendAnnouncementOrResponse(SSDP_SERVICE *pssdpService, SOCKET sock, PSOCKADDR_STORAGE pSockAddr);
#define SendAnnouncement(pService) SendAnnouncementOrResponse(pService, INVALID_SOCKET, NULL)

VOID SendByebye(SSDP_SERVICE *pssdpService, SOCKET socket);

DWORD AnnounceTimerProc (VOID *Arg);

DWORD ByebyeTimerProc (VOID *Arg);

VOID FreeSSDPService(SSDP_SERVICE *pSSDPSvc);

// rpc related functions
INT RpcServerStart();

INT RpcServerStop();


// Parser
BOOL ComposeSSDPNotify(SSDP_SERVICE *pssdpSvc, BOOL fAlive);

BOOL InitializeSearchResponseFromRequest(PSSDP_SEARCH_RESPONSE pSearchResponse,
                                         SSDP_REQUEST *SsdpRequest,
                                         SOCKET sock,
                                         PSOCKADDR_STORAGE RemoteAddr);

DWORD SearchResponseTimerProc (VOID *Arg);

VOID StartSearchResponseTimer(PSSDP_SEARCH_RESPONSE ResponseEntry,
                              LPTHREAD_START_ROUTINE pCallback);

VOID RemoveFromListSearchResponse(PSSDP_SEARCH_RESPONSE ResponseEntry);

VOID CleanupAnnounceEntry (SSDP_SERVICE *pService);

VOID CleanupListSearchResponse(PLIST_ENTRY pListHead);

INT RegisterSsdpService ();

void FreeSsdpMessageList(SSDP_MESSAGE **pSsdpMessageList, int nEntries);
// Cache

/*VOID InitializeListCache();

VOID CleanupListCache();
VOID DestroyListCache();

BOOL UpdateListCache(SSDP_REQUEST *SsdpRequest, BOOL IsSubscribed);

INT SearchListCache(CHAR *szType, SSDP_MESSAGE ***svcList);

VOID _UpdateCache(PSSDP_MESSAGE pSsdpMessage);

BOOL WINAPI _CleanupCache();*/

HANDLE RegisterNotificationSink(DWORD dwNotificationType, LPCWSTR pwszType, LPCWSTR pwszQueryString, LPCWSTR pwszMsgQueue, HANDLE hOwner);
void DeregisterNotificationSink(HANDLE hContext);

VOID InitializeListNotify();

VOID CleanupListNotify();

VOID AddToListNotify(PSSDP_NOTIFY_REQUEST NotifyRequest);

VOID RemoveFromListNotify(PSSDP_NOTIFY_REQUEST NotifyRequest);

PSSDP_NOTIFY_REQUEST CreateNotifyRequest(NOTIFY_TYPE nt, CHAR *szType,
                                         CHAR *szEventUrl,
                                         HANDLE NotifySemaphore);

VOID CheckListNotifyForEvent(SSDP_REQUEST *SsdpRequest);

VOID CheckListNotifyForAliveByebye(SSDP_REQUEST *SsdpRequest);

BOOL IsMatchingAliveByebye(PSSDP_NOTIFY_REQUEST pNotifyRequest, 
                           SSDP_REQUEST *pSsdpRequest); 

BOOL QueuePendingNotification(PSSDP_NOTIFY_REQUEST pNotifyRequest, 
                              PSSDP_REQUEST pSsdpRequest); 

BOOL IsAliveByebyeInListNotify(SSDP_REQUEST *SsdpRequest); 

INT RetrievePendingNotification(HANDLE SyncHandle, MessageList **svcList);

VOID CleanupClientNotificationRequest(HANDLE SyncSemaphore);

VOID CheckListCacheForNotification(PSSDP_NOTIFY_REQUEST pNotifyRequest); 

VOID FreeNotifyRequest(PSSDP_NOTIFY_REQUEST NotifyRequest);

VOID ProcessSsdpRequest(PSSDP_REQUEST pSsdpRequest, RECEIVE_DATA *pData); 

VOID GetNotifyLock(); 

VOID FreeNotifyLock();

#define SsdpAlloc(x)    malloc(x)
#define SsdpFree(x)     free(x)
#define SsdpDup(x)      ((x) ? _strdup(x) : NULL)

#endif

