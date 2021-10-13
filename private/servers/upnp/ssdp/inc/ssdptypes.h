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
#ifndef _SSDPTYPES_
#define _SSDPTYPES_

#include <windef.h>
#ifndef UNDER_CE
#include <winsock2.h>

#include <wchar.h>
#include <time.h>
#include <assert.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <cedefs.h>
#endif
#include <malloc.h>

#include "ssdp.h"
#include "list.h"
#include "ssdpparser.h"
#include "common.h"

#define SSDP_SERVICE_SIGNATURE 0x1601

#define SSDP_SEARCH_RESPONSE_SIGNATURE 0x1603
#define SSDP_CACHE_ENTRY_SIGNATURE 0x1604
#define SSDP_NOTIFY_REQUEST_SIGNATURE 0x1605
#define SSDP_PENDING_NOTIFY_SIGNATURE 0x1606
#define SSDP_EVENT_REQUEST_SIGNATURE 0x1607

typedef enum _ServiceState 
{
    SERVICE_INIT,
    SERVICE_ACTIVE_NO_MASTER,
    SERVICE_ACTIVE_W_MASTER,
    SERVICE_NO_MASTER_CLEANUP
} ServiceState, *PServiceState;

typedef struct _SSDP_SERVICE 
{

    LIST_ENTRY linkage;

    INT Type;

    DWORD dwTimerCookie;

    INT iRetryCount;

    UINT iLifeTime;

    SSDP_REQUEST SsdpRequest;

    ServiceState state;

    LIST_ENTRY listSearchResponse;

    CRITICAL_SECTION CSService;

    HANDLE CleanupEvent;

    DWORD flags;

    PCONTEXT_HANDLE_TYPE *RpcContextHandle;

} SSDP_SERVICE, *PSSDP_SERVICE;

typedef struct _SsdpSearchResponse 
{

   LIST_ENTRY linkage;

   INT Type;

   DWORD dwTimerCookie;

   INT RetryCount;

   LONG MX;

   SOCKET socket;
   
   SOCKADDR_STORAGE RemoteAddr;

   CHAR * szResponse;

   SSDP_SERVICE *Owner;

   HANDLE DoneEvent;

} SSDP_SEARCH_RESPONSE, *PSSDP_SEARCH_RESPONSE;

typedef struct _SSDP_NOTIFY_REQUEST 
{

    LIST_ENTRY linkage;

    INT Type;

    INT Size;

    CHAR *szNT;

    CHAR *szUrl;

    DWORD csecTimeout;

    CHAR *szSid;

    DWORD dwTimerCookie;
    BOOL    fInTimer;       // set if timer proc is running
    HANDLE hEventCleanup;
    BOOL fCleanup;

    HANDLE hProc;       // client process
    SERVICE_CALLBACK_FUNC pfCallback;   // callback in client process
    PVOID pvCallbackCtxt;   //callback context
    
    CRITICAL_SECTION CSlistPending;
    CRITICAL_SECTION csSubscribe;

    // list of pending notifications
    LIST_ENTRY listPendingNotification;

} SSDP_NOTIFY_REQUEST, *PSSDP_NOTIFY_REQUEST;

typedef struct _SSDP_CACHE_ENTRY 
{

    LIST_ENTRY linkage;

    INT Type;

    INT Size;

    ULONGLONG ExpireTime;

    SSDP_REQUEST SsdpRequest;

} SSDP_CACHE_ENTRY, *PSSDP_CACHE_ENTRY;

typedef struct _SSDP_PENDING_NOTIFICATION 
{

    LIST_ENTRY linkage;

    INT Type;

    INT Size;

    SSDP_REQUEST SsdpRequest;

} SSDP_PENDING_NOTIFICATION, *PSSDP_PENDING_NOTIFICATION;

typedef struct _RECEIVE_DATA
{
    SOCKET socket;
    DWORD dwIndex;
    CHAR *szBuffer;
    SOCKADDR_STORAGE RemoteSocket;
} RECEIVE_DATA;

// queue of SSDP requests
class ssdp_queue
{
    enum {_Size = 15};
    
public:
    enum result {was_full = 1, was_not_full = 2} ;
    
    ssdp_queue()
        : nFront(0), nBack(0)
    {
        InitializeCriticalSection(&cs);
        
#ifdef DEBUG
        memset(a, 0, sizeof(a));
#endif        
    }
    
    ~ssdp_queue()
    {
        flush();
        
        DeleteCriticalSection(&cs);
    }
    
    void flush()
    {
        EnterCriticalSection(&cs);
        
        // Empty the queue of any remaining messages
        // This has to use pop as in release the pointer
        // in the member buffer is not null'd out
        RECEIVE_DATA *p = NULL;
        
        while(p = pop())
        {
            if (p->szBuffer)
            {
                free(p->szBuffer);
            }
            free(p);
        }
        
        LeaveCriticalSection(&cs);
    }
        
    // push
    result push(RECEIVE_DATA* p)
    {
        EnterCriticalSection(&cs);
        
        result ret = was_not_full;
        
        if(full())
        {
            RECEIVE_DATA* p = pop();
            
            if(p->szBuffer)
            {
                free(p->szBuffer);
            }
            free(p);
            
            ret = was_full;
        }
        
        Assert(a[nBack % _Size] == NULL);
        Assert(p != NULL);
        
        a[nBack++ % _Size] = p;
        
        if(nBack == 0xFFFFFFFF)
        {
            nBack -= nFront - nFront % _Size;
            nFront = nFront % _Size;
        }
        
        LeaveCriticalSection(&cs);
        
        return ret;
    }
    
    // pop
    RECEIVE_DATA* pop()
    {
        RECEIVE_DATA* p = NULL;
        
        EnterCriticalSection(&cs);
        
        if(!empty())
        {
            unsigned n;
            
            p = a[n = nFront++ % _Size];
#ifdef DEBUG
            a[n] = NULL;
#endif            
        }
        
        LeaveCriticalSection(&cs);
        
        return p;
    }
    
    bool empty()
        {return nFront == nBack; }
        
private:
        
    bool full()
        {return nBack - nFront == _Size; }
    
private:
    RECEIVE_DATA*       a[_Size];
    unsigned            nFront;
    unsigned            nBack;
    CRITICAL_SECTION    cs;
};

#endif // _SSDPTYPES_
