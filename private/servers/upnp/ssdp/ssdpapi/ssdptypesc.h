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
#include "svsutil.hxx"
#include "vector.hxx"

#define SSDP_CLIENT_SEARCH_SIGNATURE 0x1608

typedef enum _SearchState 
{
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

typedef struct _SSDP_SEARCH_REQUEST 
{

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
    
    DWORD dwNumberOfResponses;

    DWORD dwTimerCookie;
    
    SVSThreadPool ThreadPool;

    HANDLE DoneEvent;

    ce::vector<SSDP_SEARCH_NETWORK> Networks;

    SOCKET WakeupSocket;

    BOOL HitWire;

    BOOL HasSockets;

    LONG bCallbackInProgress;
    
    CRITICAL_SECTION cs;
    
    long nRefCount; 

} SSDP_SEARCH_REQUEST, *PSSDP_SEARCH_REQUEST;

