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

#pragma once

#include <windows.h>
#include <sync.hxx>
#include <list.hxx>

namespace DvrPsMux
{

//
// ------------- SafeQueue_t --------------
//

typedef ce::gate<ce::critical_section_with_copy> Gate_t;

template <class Entry, class Allocator>
class SafeQueue_t :
    protected ce::list<Entry, Allocator>
{
    typedef ce::list<Entry, Allocator> BaseList_t;
    typedef ce::gate<ce::critical_section_with_copy> Gate_t;

    ce::critical_section_with_copy m_Lock;
    HANDLE m_NotEmptyEvent;
    bool m_QueueIsClosed;

    bool
    WaitAndLock(HANDLE AbortEvent = NULL)
    {
        // AbortEvent is optional in this method but I'll always
        // be using it in my impl, so ASSERT here in case I
        // forget.
        ASSERT(AbortEvent);

        if (!m_NotEmptyEvent)
            {
            ASSERT(false);
            return false;
            }

        // If we take the queue lock and then wait on the event,
        // we can deadlock. But if we wait first before trying
        // to get the lock, the event can reset after we get the
        // signal & before we take the lock. So we employ a
        // strategy that ensures the event is signalled AND we
        // have the lock, at the same time:
        // 1. wait for the event
        // 2. get the lock
        // 3. TEST the event (don't block); if it has been reset
        //    then release the lock and return to a wait state
        //
        // The event is guaranteed to remain in the signalled
        // state as long as we have the lock.
        HANDLE WaitEvents[2] = { m_NotEmptyEvent, AbortEvent };
        long EventCount = (AbortEvent ? 2 : 1);

        while (true)
            {
            DWORD WaitResult = WaitForMultipleObjects(
                EventCount,
                WaitEvents,
                FALSE,
                INFINITE
                );
            if (WaitResult != WAIT_OBJECT_0)
                {
                return false;
                }

            m_Lock.lock();

            WaitResult = WaitForMultipleObjects(
                EventCount,
                WaitEvents,
                FALSE,
                0
                );
            if (WaitResult == WAIT_OBJECT_0)
                {
                break;
                }

            m_Lock.unlock();

            if (WaitResult != WAIT_TIMEOUT)
                {
                return false;
                }
            }

        // this means we have the lock
        return true;
    }

public:
    SafeQueue_t() :
        m_NotEmptyEvent(NULL),
        m_QueueIsClosed(false)
    {
        m_NotEmptyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        ASSERT(m_NotEmptyEvent);
    }

    ~SafeQueue_t()
    {
        if (m_NotEmptyEvent)
            {
            CloseHandle(m_NotEmptyEvent);
            }
    }

    template<class _Val>
    bool
    PushBack(
        const _Val& Value
        )
    {
        Gate_t Gate(m_Lock);
        if (m_QueueIsClosed)
            {
            return false;
            }

        bool DoSetEvent = (BaseList_t::size() == 0 && m_NotEmptyEvent);
        bool Succeeded = BaseList_t::push_back(Value);
        if (Succeeded && DoSetEvent)
            {
            ASSERT(BaseList_t::size() > 0);
            SetEvent(m_NotEmptyEvent);
            }
        return Succeeded;
    }

    void
    PopFront()
    {
        Gate_t Gate(m_Lock);

        ASSERT(BaseList_t::size() > 0);

        bool DoResetEvent =
            (BaseList_t::size() == 1 && m_NotEmptyEvent);
        BaseList_t::pop_front();
        ASSERT(!DoResetEvent || BaseList_t::size() == 0);
        if (DoResetEvent)
            {
            ResetEvent(m_NotEmptyEvent);
            }
    }

    Entry&
    Front()
    {
        Gate_t Gate(m_Lock);

        ASSERT(BaseList_t::size() > 0);
        return BaseList_t::front();
    }

    long
    Size()
    {
        Gate_t Gate(m_Lock);

        return BaseList_t::size();
    }

    Entry*
    WaitFront(HANDLE AbortEvent = NULL)
    {
        bool Succeeded = WaitAndLock(AbortEvent);
        ASSERT(Succeeded);

        Entry* pEntry = NULL;

        if (BaseList_t::size() > 0 && Succeeded)
            {
            pEntry = &BaseList_t::front();
            }

        m_Lock.unlock();
        return pEntry;
    }

    void Close()
    {
        Gate_t Gate(m_Lock);

        m_QueueIsClosed = true;
        if (m_NotEmptyEvent)
            {
            // if the queue is closed we don't want consumers
            // to block on this event, so we'll signal it
            SetEvent(m_NotEmptyEvent);
            }
    }

    void Open()
    {
        Gate_t Gate(m_Lock);
        ASSERT( 0 == BaseList_t::size() );
        m_QueueIsClosed = false;
        if (BaseList_t::size() == 0 && m_NotEmptyEvent)
            {
            ResetEvent(m_NotEmptyEvent);
            }
    }
};

}

