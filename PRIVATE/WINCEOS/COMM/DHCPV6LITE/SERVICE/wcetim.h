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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#ifndef _WCETIMER_H_
#define _WCETIMER_H_

#include <cxport.h>

// WCETimer stuff

//
//  For StateNotifyFn() to queue work item using CXPORT..
//

typedef struct
{
    CTEEvent    hCTEEvent;
    PVOID       pvContext;

}   CTE_EVENT_DATA, *PCTE_EVENT_DATA;


///////////////////////////////////////////////////////////////////////////////
//  CE function prototypes for timer routines..
///////////////////////////////////////////////////////////////////////////////


#define TQT_ADDREF(_pTQT)                                                   \
{                                                                           \
    DEBUGMSG(                                                               \
        0,                                                                  \
        (TEXT("ZCF:: +++ TQT_ADDREF[0x%x] [%d]"),                           \
        _pTQT,                                                              \
        _pTQT->dwRef+1));                                                   \
    InterlockedIncrement(&(_pTQT)->dwRef);                                  \
}

#define	TQT_DELREF(_pTQT)                                                   \
{                                                                           \
    ASSERT(_pTQT->dwRef);                                                   \
    DEBUGMSG(                                                               \
        0,                                                                  \
        (TEXT("ZCF:: --- TQT_DELREF[0x%x] [%d]"),                           \
        _pTQT,                                                              \
        _pTQT->dwRef-1));                                                   \
    if (!InterlockedDecrement(&(_pTQT)->dwRef))                             \
    {                                                                       \
        DEBUGMSG(                                                           \
            ZONE_TIMER,                                                     \
            (TEXT("ZCF:: $$$ TimerQueueTimer[0x%x] deleted. $$$\r\n"),      \
              _pTQT));                                                      \
        if (_pTQT->hFinishEvent != NULL &&                                  \
            _pTQT->hFinishEvent != INVALID_HANDLE_VALUE)                    \
        {                                                                   \
               SetEvent(_pTQT->hFinishEvent);                               \
        }                                                                   \
        DeleteCriticalSection(&(_pTQT->csTimerQueueTimer));                 \
        CTEDeinitBlockStruc(&_pTQT->TimerBlockStruc);                       \
        FREE(_pTQT);                                                        \
        DEBUGMSG(ZONE_TIMER,                                                \
            (TEXT("ZCF:: ---> TotalTQT[%d]\r\n"),                           \
            InterlockedDecrement(&g_dwTotalTQT)));                          \
    }                                                                       \
}


#define TQ_ADDREF(_pTQ)                                                     \
{                                                                           \
    DEBUGMSG(                                                               \
        0,                                                                  \
        (TEXT("ZCF:: +++ TQ_ADDREF[0x%x] [%d]"),                            \
        _pTQ,                                                               \
        _pTQ->dwRef+1));                                                    \
    InterlockedIncrement(&(_pTQ)->dwRef);                                   \
}


#define TQ_DELREF(_pTQ)                                                     \
{                                                                           \
    ASSERT(_pTQ->dwRef);                                                    \
    DEBUGMSG(                                                               \
        0,                                                                  \
        (TEXT("ZCF:: --- TQ_DELREF[0x%x] [%d]"),                            \
        _pTQ,                                                               \
        _pTQ->dwRef-1));                                                    \
    if (!InterlockedDecrement(&(_pTQ)->dwRef))                              \
    {                                                                       \
        DEBUGMSG(                                                           \
            ZONE_TIMER,                                                     \
            (TEXT("ZCF:: $$$ TimerQueue[0x%x] deleted $$$"),                \
            _pTQ));                                                         \
        DeleteCriticalSection(&(_pTQ->csTimerQueue));                       \
        FREE(_pTQ);                                                         \
        DEBUGMSG(ZONE_TIMER,                                                \
            (TEXT("ZCF:: ---> TotalTQ[%d]\r\n"),                            \
            InterlockedDecrement(&g_dwTotalTQ)));                           \
    }                                                                       \
}

typedef WAITORTIMERCALLBACKFUNC WAITORTIMERCALLBACK ;

typedef struct _TimerQueue
{
    #define CE_TIMER_SIG        0xCE0110EC

    LIST_ENTRY                  ListEntry;

#ifdef DEBUG    
    DWORD                       dwSig;
#endif

    CRITICAL_SECTION            csTimerQueue;
    LIST_ENTRY                  TimerQueueTimerList;
    HANDLE                      hFinishEvent;
    DWORD                       dwRef;

}   TIMER_QUEUE, *PTIMER_QUEUE;


typedef struct _TimerQueueTimer
{    
    #define CE_TIMERTIMER_SIG   0xCE0220EC
    
    LIST_ENTRY                  ListEntry;

#ifdef DEBUG
    DWORD                       dwSig;
#endif

    CRITICAL_SECTION            csTimerQueueTimer;
    DWORD                       dwRef;
    CTETimer                    hTimer;             // handle to CTE Timer..
    CTEBlockStruc		        TimerBlockStruc;	// used to sync stopping the timer
    BOOL                        bTimerRunning;      // timer is running.
    BOOL                        bTimerInCallBack;   // timer is in callback.
    WAITORTIMERCALLBACK         Callback;           // timer callback function
    PVOID                       Parameter;          // callback parameter
    DWORD                       DueTime;            // timer due time
    DWORD                       Period;             // timer period   
    HANDLE                      hFinishEvent;

}   TIMER_QUEUE_TIMER, *PTIMER_QUEUE_TIMER;

#define CreateTimerQueue        CE_CreateTimerQueue
#define DeleteTimerQueue        CE_DeleteTimerQueue
#define DeleteTimerQueueEx(a,b) CE_DeleteTimerQueue(a)
#define CreateTimerQueueTimer   CE_CreateTimerQueueTimer
#define DeleteTimerQueueTimer   CE_DeleteTimerQueueTimer
#define ChangeTimerQueueTimer   CE_ChangeTimerQueueTimer


HANDLE
CE_CreateTimerQueue(void);

BOOL
CE_DeleteTimerQueue(HANDLE  hTimerQueue);

BOOL 
CE_CreateTimerQueueTimer(
  PHANDLE               phNewTimer,     // handle to timer
  HANDLE                TimerQueue,     // handle to timer queue
  WAITORTIMERCALLBACK   Callback,       // timer callback function
  PVOID                 Parameter,      // callback parameter
  DWORD                 DueTime,        // timer due time
  DWORD                 Period,         // timer period
  ULONG                 Flags);         // options

BOOL CE_DeleteTimerQueueTimer(
  HANDLE TimerQueue,        // handle to timer queue
  HANDLE Timer,             // handle to timer
  HANDLE CompletionEvent);   // handle to completion event

BOOL
CE_ChangeTimerQueueTimer(
    HANDLE  TimerQueue,     // handle to timer queue
    HANDLE  Timer,          // handle to timer
    ULONG   DueTime,        // timer due time
    ULONG   Period);         // timer period


#endif	//_WCETIMER_H_

