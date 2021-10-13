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
#include "EventSink.h"  

CDShowEventSink::CDShowEventSink()
{
    m_Events.clear();
    m_EventFilters = NULL;
    m_EventFilterCount = 0;

    m_SearchFilters = NULL;
    m_SearchFilterCount = 0;
    m_EventSearchLocation = 0;
    m_GetEventLocation = 0;

    m_bLoggingThreadActive = FALSE;
    m_hLoggerThread = NULL;
    m_hLoggingEvent = NULL;

    m_pMediaEvent = NULL;
}

CDShowEventSink::~CDShowEventSink()
{
    Cleanup();
}

// Clear out all logged events, but leave all of the filters in place
HRESULT
CDShowEventSink::Purge()
{
    CAutoLock cObjectLock(&m_Lock);
    
    // empty m_events, calling FreeEventParams for each event before deleteing it.

    for(int i = 0; i < m_Events.size(); i++)
    {
        m_pMediaEvent->FreeEventParams(m_Events[i]->Code, m_Events[i]->Param1, m_Events[i]->Param2);
        delete m_Events[i];
        m_Events[i] = NULL;
    }

    // clear out the vector
    m_Events.clear();

    // the existing queue of events are gone, so
    // the search list is invalid
    if(m_SearchFilters)
    {
        delete [] m_SearchFilters;
        m_SearchFilters = NULL;
    }

    m_SearchFilterCount = 0;
    m_EventSearchLocation = 0;
    m_GetEventLocation = 0;

    return S_OK;
}

HRESULT
CDShowEventSink::Cleanup()
{
    HRESULT hr = S_OK;
    DWORD dwExitCode;

    // wait for the running thread to complete
    if(m_hLoggerThread)
    {
        // shutdown the thread and related things
        m_Lock.Lock();
        m_bLoggingThreadActive = FALSE;
        m_Lock.Unlock();

        do
        {
            if(!GetExitCodeThread(m_hLoggerThread, &dwExitCode))
                break;
        }while(dwExitCode == STILL_ACTIVE);

        // now the logger thread is shut down, close it out
        CloseHandle(m_hLoggerThread);
        m_hLoggerThread = NULL;
    }

    m_Lock.Lock();

    // clean up the event list, and search location now that the logger thread
    // is gone.
    Purge();

    // delete the event filter
    if(m_EventFilters)
    {
        delete [] m_EventFilters;
        m_EventFilters = NULL;
    }
    m_EventFilterCount = 0;

    // shut down the logging event now that we're done with it.
    if(m_hLoggingEvent)
    {
        CloseHandle(m_hLoggingEvent);    
        m_hLoggingEvent = NULL;
    }

    // release our reference to IMediaEvent
    if(m_pMediaEvent)
    {
        m_pMediaEvent->Release();
        m_pMediaEvent = NULL;
    }

    ClearEventCallbacks();

    // release the lock so it can be used later
    m_Lock.Unlock();

    return hr;
}

DWORD
ThreadProc(LPVOID lpParameter)
{
    // use the incoming parameter as a pointer to the CDShowEventSink class,
    // and call it to process the events.
    return ((CDShowEventSink *) lpParameter)->ProcessMediaEvents();
}

HRESULT
CDShowEventSink::Init(IMediaEvent *pMediaEvent, DWORD nCount, DShowEvent* pEventFilters)
{
    CAutoLock cObjectLock(&m_Lock);

    HRESULT hr = S_OK;

    // start with a clean slate, just in case we were already initialized.
    Cleanup();

    // set up our internal media event interface
    m_pMediaEvent = pMediaEvent;
    m_pMediaEvent->AddRef();

    if(FAILED(hr = SetEventFilters(nCount, pEventFilters)))
        goto cleanup;

    // create the logging thread thread and an event so we know
    // when additonal events were triggered
    m_hLoggingEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(m_hLoggingEvent)
    {
        m_hLoggerThread = CreateThread(NULL, 0, ThreadProc, this, CREATE_SUSPENDED, NULL);
        if(m_hLoggerThread)
        {
            m_bLoggingThreadActive = TRUE;

            // resume the monitoring thread now that we're fully initialized, it won't actually start monitoring until
            // we release the mutex after completely initializing.
            ResumeThread(m_hLoggerThread);
        }
        else hr = E_FAIL;
    }
    else hr = E_FAIL;

cleanup:
    if(FAILED(hr))
        Cleanup();

    return hr;
}

HRESULT
CDShowEventSink::SetEventFilters(DWORD nCount, DShowEvent* pEventFilters)
{
    CAutoLock cObjectLock(&m_Lock);
    HRESULT hr = S_OK;

    if(m_EventFilters)
    {
        delete[] m_EventFilters;
        m_EventFilters = NULL;
    }
    m_EventFilterCount = 0;

     // set up the optional event filtering if it was passed in
    if(pEventFilters && nCount)
    {
        if(!(m_EventFilters = new DShowEvent[nCount]))
        {
            hr = E_OUTOFMEMORY;
            goto cleanup;
        }

        m_EventFilterCount = nCount;

        memcpy(m_EventFilters, pEventFilters, nCount * sizeof(DShowEvent));
    }

cleanup:
    return hr;
}

DWORD
CDShowEventSink::ProcessMediaEvents()
{
    // while the event sink is running (not cleaned up), process media events
    BOOL bRunning = false;
    HRESULT hr = S_OK;
    LONG lEventCode = 0, lParam1 = 0, lParam2 = 0;

    do
    {
        // wait for a media event, timeout after MEDIAEVENT_TIMEOUT
        hr = m_pMediaEvent->GetEvent(&lEventCode, &lParam1, &lParam2, DEFAULT_EVENT_COUNT);
        {
            CAutoLock cObjectLock(&m_Lock);

            // if the event is found, check it, otherwise see if we timed out
            if(SUCCEEDED(hr))
            {
                BOOL bAddEvent = FALSE;
                DShowEvent dse;

                dse.Code = lEventCode;
                dse.Param1 = lParam1;
                dse.Param2 = lParam2;
                dse.FilterFlags = 0;

                // if there's a filter, see whether or not this event is in the filter,
                // otherwise default to putting it in the list
                if(m_EventFilters)
                {
                    for(int i = 0; i < m_EventFilterCount && !bAddEvent; i++)
                    {
                        if(Compare(&dse, &(m_EventFilters[i])))
                            bAddEvent = TRUE;
                    }
                }
                else bAddEvent = TRUE;

                if(bAddEvent)
                {
                    // allocate and add the event to the queue
                    DShowEvent *pEvent = new DShowEvent;

                    if(pEvent)
                    {
                        memcpy(pEvent, &dse, sizeof(DShowEvent));
                        m_Events.push_back(pEvent);

                        // set the logged event
                        SetEvent(m_hLoggingEvent);
                    }

                }

                // for each of the callbacks
                for(int i = 0; i < m_Callbacks.size(); i++)
                {
                    if(m_Callbacks[i]->nEventFilters > 0 && m_Callbacks[i]->EventFilters)
                    {
                        for(int j = 0; j < m_Callbacks[i]->nEventFilters; j++)
                        {
                            if(Compare(&dse, &(m_Callbacks[i]->EventFilters[j])))
                            {
                                LPVOID lParam = m_Callbacks[i]->lParam;
                                EVENTCALLBACK *pCallback = m_Callbacks[i]->pCallback;
                                m_Lock.Unlock();
                                pCallback(lEventCode, lParam1, lParam2, lParam);
                                m_Lock.Lock();
                            }
                        }
                    }
                    else
                    {
                        LPVOID lParam = m_Callbacks[i]->lParam;
                        EVENTCALLBACK *pCallback = m_Callbacks[i]->pCallback;
                        m_Lock.Unlock();
                        pCallback(lEventCode, lParam1, lParam2, lParam);
                        m_Lock.Lock();
                    }

                }

                // if the event wasn't added, make sure we free it now or it'll get leaked
                if(!bAddEvent)
                    m_pMediaEvent->FreeEventParams(lEventCode, lParam1, lParam2);
            }

            // figure out whether or not we're supposed to be running while we still hold the lock
            bRunning = m_bLoggingThreadActive;
        }
    // as long as we're supposed to be running, keep monitoring the events.
    }while(bRunning);

    return 1;
}

HRESULT
CDShowEventSink::SetEventCallback(EVENTCALLBACK * pEventCallback, DWORD nCount, const DShowEvent *pEventFilters, LPVOID lParam)
{
    HRESULT hr = S_OK;

    CAutoLock cObjectLock(&m_Lock);

    if(!(nCount > 0 && pEventFilters == NULL) && pEventCallback)
    {
        // create a new DShowEventCallback
        DShowEventCallback * pCallback = new DShowEventCallback;

        if(pCallback)
        {
            DShowEvent *pDSE = NULL;

            memset(pCallback, 0, sizeof(DShowEventCallback));

            // create a new DShowEvent
            if(nCount > 0 && pEventFilters)
            {
                pDSE = new DShowEvent[nCount];
                if(pDSE)
                {
                    memcpy(pDSE, &pEventFilters, sizeof(DShowEvent) * nCount);

                    // copy them into m_Callbacks
                    pCallback->EventFilters = pDSE;
                    pCallback->nEventFilters = nCount;
                }
                else
                {
                    hr = E_FAIL;
                    delete pCallback;
                    pCallback = NULL;
                }
            }

            if(SUCCEEDED(hr))
            {
                pCallback->lParam = lParam;
                pCallback->pCallback = pEventCallback;

                m_Callbacks.push_back(pCallback);
            }
        }
        else hr = E_FAIL;
    }
    else hr = E_FAIL;

    return hr;
}

HRESULT
CDShowEventSink::ClearEventCallbacks()
{
    HRESULT hr = S_OK;

    CAutoLock cObjectLock(&m_Lock);

    // for each of the callbacks
    for(int i = 0; i < m_Callbacks.size(); i++)
    {
        // delete the event filters
        delete[] m_Callbacks[i]->EventFilters;
        // delete the callback
        delete m_Callbacks[i];
        m_Callbacks[i] = NULL;
    }

    // clear the callback vector
    m_Callbacks.clear();

    return hr;
}


HRESULT
CDShowEventSink::GetEvent(LONG *lEventCode, LONG *lParam1, LONG *lParam2, DWORD dwMilliseconds)
{
    HRESULT hr = E_ABORT;

    if(lEventCode && lParam1 && lParam2)
    {

        // grab the lock
        m_Lock.Lock();

        if(m_GetEventLocation == m_Events.size())
        {
            // we know that there are no new events, so reset the event state
            // so we're triggered on the next new one.
            ResetEvent(m_hLoggingEvent);

            // release the lock so we're not waiting holding the lock
            m_Lock.Unlock();

            // wait on the event trigger, either we'll time out or we'll be triggered
            WaitForSingleObject(m_hLoggingEvent, dwMilliseconds);

            // grab the lock before we start looking through to see if the event came
            m_Lock.Lock();
        }


        // if the current position is a valid position
        if(m_GetEventLocation < m_Events.size())
        {
            *lEventCode = m_Events[m_GetEventLocation]->Code;
            *lParam1 = m_Events[m_GetEventLocation]->Param1;
            *lParam2 = m_Events[m_GetEventLocation]->Param2;
            m_GetEventLocation++;
            hr = S_OK;
        }

        // release the lock
        m_Lock.Unlock();
    }
    else hr = E_FAIL;

    return hr;
}

// wait for the matching event
HRESULT
CDShowEventSink::WaitOnEvent(const DShowEvent * event, DWORD dwMilliseconds)
{
    return WaitOnEvents(1, event, true, dwMilliseconds);
}

// wait for a list of matching events for dwMilliseconds, until returning.
HRESULT
CDShowEventSink::WaitOnEvents(DWORD nCount, const DShowEvent* pEventFilters, BOOL fWaitAll, DWORD dwMilliseconds)
{
    HRESULT hr = E_FAIL;
    BOOL bEventFound = FALSE;
    int nCurrentPosition = 0;
    std::list<const DShowEvent *> Filters;
    std::list<const DShowEvent *>::iterator itr;

    // grab the lock
    m_Lock.Lock();

    // make a copy of the event list
    for(int i = 0; i < nCount; i++)
        Filters.push_back(&(pEventFilters[i]));

    // get the current time marker
    DWORD dwStartTime = GetTickCount();
    DWORD dwTimeElapsed = 0;
    DWORD dwCurrentTickCount = 0;

    // look through all of the events until we find what we're looking for.
    // if it's not already there, then watch stuff as it comes in looking for it.
    do
    {
        int ValidEventCount = m_Events.size();

        // if the current position is the end position, then we need to wait
        // for more events to come in
        if(nCurrentPosition == ValidEventCount)
        {
            // we know that there are no new events, so reset the event state
            // so we're triggered on the next new one.
            ResetEvent(m_hLoggingEvent);

            // release the lock so we're not waiting holding the lock
            m_Lock.Unlock();

            // handle clock rollover
            dwTimeElapsed = 0;
            dwCurrentTickCount = GetTickCount();
            if(dwCurrentTickCount >= dwStartTime)
                dwTimeElapsed = dwCurrentTickCount - dwStartTime;
            else dwTimeElapsed = (MAX_DWORD - dwStartTime) + dwCurrentTickCount;

            // wait on the event trigger, either we'll time out or we'll be triggered
            // if we're on the edge of the timout, wait a little bit longer so we're confidently past the timout,
            // before triggering an error.
            WaitForSingleObject(m_hLoggingEvent, dwMilliseconds - dwTimeElapsed);

            // grab the lock before we start looking through to see if the event came
            m_Lock.Lock();

            // now that we've waited, reset the valid event count
            ValidEventCount = m_Events.size();
        }

        // if the current position is a valid position
        if(nCurrentPosition < ValidEventCount)
        {
            // look through each of the events in the requested event queue
            for(itr = Filters.begin(); itr != Filters.end() && (!bEventFound || fWaitAll); itr++)
            {
                // check if the current position matches the requested event
                if(Compare(m_Events[nCurrentPosition], *itr))
                {
                    // if it matches set the flag that an event was found
                    bEventFound = TRUE;

                    // if it matches and fWaitAll, remove it from the list so we don't continue to look for it
                    if(fWaitAll)
                    {
                        Filters.remove(*itr);

                        // NOTE: if the event is found and removed, then the iterator is
                        // going to be out of sync with the vector. make sure that will
                        // never happen (even though we should be popping out of the loop
                        // when we find the event.
                        //itr = Filters.begin();
                        break;
                    }
                }
            }

            // go to the next position in the event queue. We'll either look through all of the
            // events already in the queue, or look until we hit the end and wait for more
            // to come in.
            nCurrentPosition++;

        }

        dwTimeElapsed = 0;
        dwCurrentTickCount = GetTickCount();
        if(dwCurrentTickCount >= dwStartTime)
            dwTimeElapsed = dwCurrentTickCount - dwStartTime;
        else dwTimeElapsed = (MAX_DWORD - dwStartTime) + dwCurrentTickCount;

    // while the timeout hasn't been hit, and a success situation hasn't been hit, keep looking.
    } while(!(dwTimeElapsed > dwMilliseconds) &&
              !(fWaitAll && Filters.size() == 0) &&
              !(!fWaitAll && bEventFound));

    // if we're waiting for all, and the incoming list is empty, then we succeeded
    if(fWaitAll && Filters.size() == 0)
        hr = S_OK;
    // otherwise, if we weren't waiting for all and a single event was found, then we succeeded
    else if(!fWaitAll && bEventFound)
        hr = S_OK;
    // otherwise we failed.
    else hr = E_FAIL;

    // release the lock
    m_Lock.Unlock();

    return hr;
}

// Search the event history for the next matching event
DShowEvent *
CDShowEventSink::FindFirstEvent(DWORD nCount, const DShowEvent* pEventFilters)
{  
    CAutoLock cObjectLock(&m_Lock);

    DShowEvent *dse = NULL;

    // cleanup the old search list
    if(m_SearchFilters)
    {
        delete[] m_SearchFilters;
        m_SearchFilters = NULL;
    }

    m_SearchFilterCount = 0;
    m_EventSearchLocation = 0;

    // if the user didn't give us an event to look for, then
    // we can't look for it.
    if(NULL != pEventFilters && 0 < nCount)
    {
        m_SearchFilterCount = nCount;

        // grab a local copy of the filter list, and do the search
        if(m_SearchFilters = new DShowEvent[m_SearchFilterCount])
        {
            memcpy(m_SearchFilters, pEventFilters, m_SearchFilterCount * sizeof(DShowEvent));

            dse = FindNextEvent();
        }
    }
    else if(NULL == pEventFilters && nCount == 0)
    {
        dse = FindNextEvent();
    }

    return dse;
}

// Search the event history for the next matching event
DShowEvent *
CDShowEventSink::FindNextEvent()
{
    CAutoLock cObjectLock(&m_Lock);
    DShowEvent *dse = NULL;

    // search from the last event position, and search until we hit the end or have a match
    for(; m_EventSearchLocation < m_Events.size() && !dse; m_EventSearchLocation++)
    {
        // if we have a valid search filter, then check for a match,
        // if we don't, then just return the next event in the vector
        if(m_SearchFilters)
        {
            // Check through all of the filters to see if this event matches anything the user wants.
            for(int FilterSearchLocation = 0; FilterSearchLocation < m_SearchFilterCount && !dse; FilterSearchLocation++)
            {
                // see if we have a match
                if(Compare(m_Events[m_EventSearchLocation], &m_SearchFilters[FilterSearchLocation]))
                {
                    // we found it, so set the return poiner to indicate it was found
                    dse = m_Events[m_EventSearchLocation];
                }
            }
        }
        else dse = m_Events[m_EventSearchLocation];
    }

    return dse;
}

// Check to see if the event matches the specified event filter
BOOL
CDShowEventSink::Compare(const DShowEvent * Event, const DShowEvent *Filter)
{
    BOOL bRetVal = false;
    BOOL bRetVal1 = false;
    BOOL bRetVal2 = false;
    BOOL bRetVal3 = false;

    // Event Code Check

    // basic logic, if a check was requested, then
    // any one of the requests will validate the check.
    // if no check was requested, then ignore this parameter
    // if a check was requested, and none succeed then its 
    // not a match.
    if(Filter->FilterFlags & EVT_EVCODE_GREATER || 
        Filter->FilterFlags & EVT_EVCODE_LESS || 
        Filter->FilterFlags & EVT_EVCODE_EQUAL)
    {
        if(Filter->FilterFlags & EVT_EVCODE_GREATER)
            bRetVal1 |= (Event->Code > Filter->Code);
        if(Filter->FilterFlags & EVT_EVCODE_LESS)
            bRetVal1 |= (Event->Code < Filter->Code);
        if(Filter->FilterFlags & EVT_EVCODE_EQUAL)
            bRetVal1 |= (Event->Code == Filter->Code);
    } else bRetVal1 = true;

    // Param1 Check
    if(Filter->FilterFlags & EVT_PARAM1_GREATER || 
        Filter->FilterFlags & EVT_PARAM1_LESS || 
        Filter->FilterFlags & EVT_PARAM1_EQUAL)
    {
        if(Filter->FilterFlags & EVT_PARAM1_GREATER)
            bRetVal2 |= (Event->Param1 > Filter->Param1);
        if(Filter->FilterFlags & EVT_PARAM1_LESS)
            bRetVal2 |= (Event->Param1 < Filter->Param1);
        if(Filter->FilterFlags & EVT_PARAM1_EQUAL)
            bRetVal2 |= (Event->Param1 == Filter->Param1);
    }
    else bRetVal2 = true;

    // Param2 Check
    if(Filter->FilterFlags & EVT_PARAM2_GREATER || 
        Filter->FilterFlags & EVT_PARAM2_LESS || 
        Filter->FilterFlags & EVT_PARAM2_EQUAL)
    {
        if(Filter->FilterFlags & EVT_PARAM2_GREATER)
            bRetVal3 |= (Event->Param2 > Filter->Param2);
        if(Filter->FilterFlags & EVT_PARAM2_LESS)
            bRetVal3 |= (Event->Param2 < Filter->Param2);
        if(Filter->FilterFlags & EVT_PARAM2_EQUAL)
            bRetVal3 |= (Event->Param2 == Filter->Param2);
    }
    else bRetVal3 = true;

    // as long as all of the checks above succeeded, then
    // we have a match for the user requested filter
    bRetVal = bRetVal1 && bRetVal2 && bRetVal3;

    // The user actually wants the opposite of the check
    if(Filter->FilterFlags & EVT_EXCLUDE)
        bRetVal = !bRetVal;

    return bRetVal;
}

