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
/*++


File Name:

    ssdpfunc.h

Abstract:

    This file contains cross files function prototypes.

Author: Ting Cai

Created: 07/15/1999

--*/
#ifndef _SSDPFUNC_
#define _SSDPFUNC_

#include "ssdptypes.h"
#include "ssdpnetwork.h"
#include "ssdpioctl.h"

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

BOOL RegisterNotificationSink(
    HANDLE                                  hOwner, 
    DWORD                                   dwNotificationType, 
    ce::marshal_arg<ce::copy_in, LPCWSTR>   pwszUSN, 
    ce::marshal_arg<ce::copy_in, LPCWSTR>   pwszQueryString, 
    ce::marshal_arg<ce::copy_in, LPCWSTR>   pwszMsgQueue, 
    ce::marshal_arg<ce::copy_out, HANDLE*>  phHotify);
    
BOOL DeregisterNotificationSink(ce::PSL_HANDLE hContext);

BOOL BeginSSDPforDLNA();   // DLNA must call before first RegisterUpnpServiceImpl
BOOL EndSSDPforDLNA();     // DLNA must call after last RegisterUpnpServiceImpl

BOOL RegisterUpnpServiceImpl(PSSDP_MESSAGE pSsdpMessage, DWORD flags, HANDLE* phService);

BOOL RegisterUpnpServiceIoctl(
    ce::marshal_arg<ce::copy_in, PSSDP_MESSAGE> pSsdpMessage, 
    DWORD                                       flags, 
    ce::marshal_arg<ce::copy_out, HANDLE*>      phService);

BOOL DeregisterUpnpServiceImpl(ce::PSL_HANDLE hRegister, BOOL fByebye);

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

