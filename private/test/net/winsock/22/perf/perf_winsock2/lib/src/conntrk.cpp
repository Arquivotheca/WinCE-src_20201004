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
// conntrk.cpp

#include "conntrk.h"
#include "loglib.h"

#define CONNTRACK_THREAD_STACK_SIZE 131072

struct SockNode;

// Constants
//
static SockNode* g_SockArray = NULL;
static DWORD g_SockCount = 0;
static HANDLE g_hConnTrackThread = NULL;
static HANDLE g_hTerminateEvent = NULL;
static DWORD g_FreqCleanupMs = INFINITE;

// Function prototypes
//
unsigned long __stdcall ConnTrack_Thread(void* pParams);

// Structures/classes
//
struct SockNode {
    LONG sock;
    LONG tag;
    LONG used;
};

// Function implementations
//
VOID ConnTrack_Touch(CONNTRACK_HANDLE handle)
{
    SockNode* p = (SockNode*)handle;
    InterlockedExchange(&p->tag, TRUE);
}

CONNTRACK_HANDLE ConnTrack_Insert(SOCKET sock)
{
    DWORD i;
    for (i=0; i<g_SockCount; i+=1)
    {
        if (InterlockedCompareExchange(&g_SockArray[i].used, TRUE, FALSE) == FALSE)
        {
            // Got a free item; initialize it.
            g_SockArray[i].sock = sock;
            g_SockArray[i].tag = TRUE;

            Log(DEBUG_MSG, TEXT("ConnTrack_Insert(): socket %ld inserted, handle 0x%X"),
                (DWORD)sock, (DWORD)&g_SockArray[i]);

            return &g_SockArray[i];
        }
    }
    return NULL;
}

// Returns TRUE if the corresponding socket was closed because entry wasn't refreshed or
//         FALSE if the corresponding socket was not closed
BOOL ConnTrack_Remove(CONNTRACK_HANDLE handle)
{
    if (handle == NULL) return FALSE;

    SockNode* p = (SockNode*)handle;
    
    if (InterlockedCompareExchange(&p->used, FALSE, TRUE)) {
        return p->sock == INVALID_SOCKET ? TRUE : FALSE;
    }
    return FALSE;
}

BOOL ConnTrack_Init(DWORD MaxSocks, DWORD Freq)
{
    g_FreqCleanupMs = Freq;
    g_SockCount = MaxSocks;
    g_SockArray = new SockNode[g_SockCount];
    if (g_SockArray == NULL) goto error;

    for (DWORD i=0; i<g_SockCount; i+=1)
    {
        g_SockArray[i].sock = INVALID_SOCKET;
        g_SockArray[i].tag = FALSE;
        g_SockArray[i].used = FALSE;
    }

    g_hTerminateEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_hTerminateEvent == NULL) goto error;

    g_hConnTrackThread = CreateThread(
        NULL,                               // security attributes
        CONNTRACK_THREAD_STACK_SIZE,        // stack size
        ConnTrack_Thread,                   // start function
        (void *)NULL,                       // parameter to pass to the thread
        0,                                  // creation flags
        NULL);
    if (g_hConnTrackThread == NULL) goto error;

    return TRUE;

error:
    if (g_SockArray != NULL) {
        delete [] g_SockArray;
        g_SockArray = NULL;
    }
    if (g_hTerminateEvent != NULL) {
        CloseHandle(g_hTerminateEvent);
        g_hTerminateEvent = NULL;
    }

    return FALSE;
}

VOID ConnTrack_Deinit()
{
    if (g_hTerminateEvent != NULL)
    {
        if (g_hConnTrackThread != NULL)
        {
            SetEvent(g_hTerminateEvent);
            WaitForSingleObject(g_hConnTrackThread, INFINITE);
            CloseHandle(g_hConnTrackThread);
        }
        CloseHandle(g_hTerminateEvent);
    }
    if (g_SockArray != NULL) delete [] g_SockArray;
}

unsigned long __stdcall ConnTrack_Thread(void* pParams)
{
    for(;;)
    {
        DWORD ret = WaitForSingleObject(g_hTerminateEvent, g_FreqCleanupMs);
        Log(DEBUG_MSG, TEXT("ConnTrack_Thread(): running..."));
      if (ret == WAIT_OBJECT_0) break;
        for (DWORD i=0; i<g_SockCount; i+=1)
        {
            LONG used = InterlockedCompareExchange(&g_SockArray[i].used, TRUE, TRUE);
            if (used) {
                LONG tag = InterlockedCompareExchange(&g_SockArray[i].tag, FALSE, TRUE);
                if (tag) continue;
                else
                {
                    if (g_SockArray[i].sock == INVALID_SOCKET) continue;

                    // Socket not closed yet...                    
                    Log(DEBUG_MSG, TEXT("ConnTrack_Thread(): closing socket of CONNTRACK_HANDLE 0x%X"), &g_SockArray[i]);

                    SOCKET sock = g_SockArray[i].sock;
                    InterlockedExchange(&g_SockArray[i].sock, INVALID_SOCKET);
#ifdef SUPPORT_IPV6
                    shutdown(sock, SD_BOTH);
#else
                    shutdown(sock, 0x2);
#endif
                    closesocket(sock);
                }
            }
        }
    }
    return 0;
}

