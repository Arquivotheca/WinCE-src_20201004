//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _SSDPAPI_H
#define _SSDPAPI_H

#include <windows.h>
#include "ssdp.h"
#include "../inc/ssdperror.h"

#ifdef  __cplusplus
extern "C" {
#endif

// DO NOT REORDER THIS ENUMERATION.  ADD NEW VALUES TO THE END.
//   if you do, change the necessary code in upnpdevicefinder.cpp
typedef enum _SSDP_CALLBACK_TYPE {
    SSDP_FOUND = 0,
    SSDP_ALIVE = 1,
    SSDP_BYEBYE = 2,
    SSDP_DONE = 3,
    SSDP_EVENT = 4,
    SSDP_DEAD = 5,
} SSDP_CALLBACK_TYPE, *PSSDP_CALLBACK_TYPE;

typedef void (WINAPI *SERVICE_CALLBACK_FUNC)(SSDP_CALLBACK_TYPE CallbackType,
                                      CONST SSDP_MESSAGE *pSsdpService,
                                      void *pContext);

VOID WINAPI FreeSsdpMessage(PSSDP_MESSAGE pSsdpMessage);

HANDLE WINAPI RegisterUpnpService(PSSDP_MESSAGE pSsdpMessage, DWORD flags);

BOOL WINAPI DeregisterUpnpService(HANDLE hRegister, BOOL fByebye);

BOOL WINAPI DeregisterUpnpServiceByUSN(char * szUSN, BOOL fByebye);

HANDLE WINAPI RegisterNotification(DWORD nt, const wchar_t* pwszUSN, const wchar_t* pwszQueryString, const wchar_t* pwszMsgQueue);

BOOL WINAPI DeregisterNotification(HANDLE hNotification);

void CleanupNotifications(HANDLE hOwner);

BOOL WINAPI RegisterUpnpEventSource(LPCSTR szRequestUri, DWORD cProps,
                                    UPNP_PROPERTY *rgProps);
BOOL WINAPI DeregisterUpnpEventSource(LPCSTR szRequestUri);

BOOL WINAPI SubmitUpnpPropertyEvent(LPCSTR szEventSourceUri, DWORD dwFlags,
                                    DWORD cProps, UPNP_PROPERTY *rgProps);

BOOL WINAPI SubmitEvent(LPCSTR szEventSourceUri, DWORD dwFlags,
                        LPCSTR szHeaders, LPCSTR szEventBody);

BOOL WINAPI GetEventSourceInfo(LPCSTR szEventSourceUri, EVTSRC_INFO **ppinfo);

HANDLE WINAPI FindServices (const char* szType, void *pReserved , BOOL fForceSearch);

HANDLE WINAPI FindServicesCallback (const char * szType,
                             void * pReserved ,
                             BOOL fForceSearch,
                             SERVICE_CALLBACK_FUNC fnCallback,
                             void *Context
                             );

BOOL WINAPI GetFirstService (HANDLE hFindServices, PSSDP_MESSAGE *ppSsdpService);

BOOL WINAPI GetNextService (HANDLE hFindServices, PSSDP_MESSAGE *ppSsdpService);

BOOL WINAPI FindServicesClose(HANDLE hSearch);

BOOL WINAPI CleanupCache();

BOOL WINAPI SsdpStartup();

void WINAPI SsdpCleanup();

#ifdef  __cplusplus
}   /* ... extern "C" */
#endif

#endif // _SSDPAPI_H
