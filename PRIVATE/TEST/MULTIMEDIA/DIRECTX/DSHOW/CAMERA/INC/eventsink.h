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
#ifndef _EVENTSINK_H_
#define _EVENTSINK_H_

#include <list>
#include <vector>
#include <windows.h>
#include <dshow.h>

typedef void EVENTCALLBACK(LONG lEventCode, LONG lParam1, LONG lParam2, LPVOID lpParameter);

#define DEFAULT_EVENT_COUNT 500

#define EVT_EVCODE_EQUAL 1
#define EVT_EVCODE_GREATER 2
#define EVT_EVCODE_LESS 4
#define EVT_PARAM1_EQUAL 8
#define EVT_PARAM1_GREATER 16
#define EVT_PARAM1_LESS 32
#define EVT_PARAM2_EQUAL 64
#define EVT_PARAM2_GREATER 128
#define EVT_PARAM2_LESS 256
#define EVT_EXCLUDE 512

#define MAX_DWORD               0xFFFFFFFF

typedef struct _DShowEvent
{
    long Code;
    long Param1;
    long Param2;
    DWORD FilterFlags;
} DShowEvent;

typedef struct _DShowEventCallback
{
    LPVOID lParam;
    EVENTCALLBACK *pCallback;
    DShowEvent* EventFilters;
    int nEventFilters;
} DShowEventCallback;


typedef class CDShowEventSink
{
public:
    CDShowEventSink();
    ~CDShowEventSink();

    // Clear out all logged events, leaving the filters in place
    HRESULT Purge();

    // do a full cleanup
    HRESULT Cleanup();

    // initialize the system, set the event filters.
    HRESULT Init(IMediaEvent *pMediaEvent, DWORD nCount, DShowEvent* pEventFilters);

    // Wait for the next matching event to trigger for dwMilliseconds
    HRESULT WaitOnEvent(const DShowEvent * event, DWORD dwMilliseconds);

    // Wait for one or all matching events to trigger for dwMilliseconds
    HRESULT WaitOnEvents(DWORD nCount, const DShowEvent* pEventFilters, BOOL fWaitAll, DWORD dwMilliseconds);

    // search the event queue for an event.
    DShowEvent *FindFirstEvent(DWORD nCount, const DShowEvent* pEventFilters);
    DShowEvent *FindNextEvent();
    HRESULT GetEvent(LONG *lEventCode, LONG *lParam1, LONG *lParam2, DWORD Milliseconds);
    HRESULT SetEventFilters(DWORD nCount, DShowEvent* pEventFilters);
    DWORD ProcessMediaEvents();
    HRESULT SetEventCallback(EVENTCALLBACK * pEventCallback, DWORD nCount, const DShowEvent *pEventFilters, LPVOID lParam);
    HRESULT ClearEventCallbacks();

private:
    CCritSec m_Lock;
    std::vector<DShowEvent *> m_Events;
    DShowEvent* m_EventFilters;     // Inclusions/exclusions of dshow events
    int m_EventFilterCount;

    DShowEvent* m_SearchFilters;    // list of filters to use when searching
    int m_SearchFilterCount;
    int m_EventSearchLocation;
    int m_GetEventLocation;

    BOOL m_bLoggingThreadActive;
    HANDLE m_hLoggerThread;
    HANDLE m_hLoggingEvent;

    IMediaEvent *m_pMediaEvent;

    std::vector<DShowEventCallback *> m_Callbacks;

    // internal check to see if the event matches the specified filter
    BOOL Compare(const DShowEvent *Event, const DShowEvent *Filter);
    
}DShowEventSink;


#endif
