//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "svsutil.hxx"
#include "vector.hxx"

#define SSDP_CLIENT_SEARCH_SIGNATURE 0x1608

typedef enum _SearchState {
    SEARCH_START,
    SEARCH_WAIT,
    SEARCH_ACTIVE_W_MASTER,
    SEARCH_DISCOVERING,
    SEARCH_COMPLETED
} SearchState, *PSearchState;

struct SSDP_SEARCH_NETWORK
{
    SOCKET          socket;
    DWORD           dwIndex;
    DWORD           dwScopeId;
    DWORD           dwScope;
    PSOCKADDR       pMulticastAddr;
    ce::string      strMulticastAddr;
};

typedef struct _SSDP_SEARCH_REQUEST {

    INT Type;

    INT Size;

    SearchState state;

    CHAR *szType;

    CHAR *szSearch;

    SERVICE_CALLBACK_FUNC Callback;

    VOID *Context;

    INT NumOfRetry;

    LONG fExit;

    HANDLE hThread;

    PLIST_ENTRY CurrentResponse;

    LIST_ENTRY ListResponses;

    DWORD dwTimerCookie;
	
	SVSThreadPool ThreadPool;

    HANDLE DoneEvent;

    ce::vector<SSDP_SEARCH_NETWORK> Networks;

    SOCKET WakeupSocket;

    BOOL HitWire;

    LONG bCallbackInProgress;
    
    CRITICAL_SECTION cs;
    
    long nRefCount; 

} SSDP_SEARCH_REQUEST, *PSSDP_SEARCH_REQUEST;

typedef struct _SSDP_MESSAGE_ITEM {
    LIST_ENTRY linkage;
    PSSDP_MESSAGE pSsdpMessage;
} SSDP_MESSAGE_ITEM, *PSSDP_MESSAGE_ITEM;

