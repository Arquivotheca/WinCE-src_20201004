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
#pragma once

#include <windows.h>
#include <sync.hxx>
#include <list.hxx>
#include <auto_xxx.hxx>
#include <svsutil.hxx>
#include <iocompletionports.h>


using namespace ce;


// declare io completion port handle resource manager
typedef auto_xxx<HANDLE, BOOL (__stdcall*)(HANDLE), CeCloseIoCompletionPort, NULL> auto_iocp;



/********************************************************************************
 *  CeIoCompletionCallbackEntry_t:
 *
 *      - class acceptable to use within standard containers
 *      - maintains the file/completion routine association
 *      - maintains overlapped/file/completion routine association
 *
 ********************************************************************************/
class CeIoCompletionCallbackEntry_t
{
    private:
        SOCKET                          m_socket;
        smart_ptr<list<LPOVERLAPPED> >  m_overlappedList;
        LPOVERLAPPED_COMPLETION_ROUTINE m_pfnCompletionRoutine;
        BOOL                            m_socketClosing;

    public:
        CeIoCompletionCallbackEntry_t();
        CeIoCompletionCallbackEntry_t(SOCKET socket, LPOVERLAPPED_COMPLETION_ROUTINE pfnCompletionRoutine);
        CeIoCompletionCallbackEntry_t(const CeIoCompletionCallbackEntry_t& rhs);
        ~CeIoCompletionCallbackEntry_t();
        CeIoCompletionCallbackEntry_t operator=(const CeIoCompletionCallbackEntry_t& rhs);

        // associates overlapped if entry associated with specified socket
        BOOL AssociateOverlapped(SOCKET socket, LPOVERLAPPED lpOverlapped);

        // remove overlapped if a member of the associated entry and returns completion routine
        BOOL RemoveOverlapped(LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE* ppfnCompletionRoutine);
        // returns whether or not the entry is associated with the specified socket
        BOOL MatchSocket(SOCKET socket);

        // returns whether or not the entry should be removed
        BOOL ShouldRemoveEntry(BOOL socketClosing);
};



//
//  null allocator ensures that only x items are allocated and freed by the dynamic allocator
//  this ensures no more than x outstanding items from the allocator and bounds the memory usage
//
template < size_t COUNT >
class null_allocator : ce::allocator
{
private:
    volatile LONG m_size;
public:
    null_allocator() : m_size(0) {}

    void* allocate(size_t size) const
    {
        LONG* msize = const_cast<LONG*>( &m_size );
        LONG curSize = InterlockedIncrement( msize );
        if( curSize > COUNT )
            {
            return NULL;
            }
        return ce::allocator::allocate(size);
    }
    void deallocate(void* ptr, size_t = 0) const
    {
        LONG* msize = const_cast<LONG*>( &m_size );
        InterlockedDecrement( msize );
        ce::allocator::deallocate(ptr);
    }
};

template <typename _Al = ce::allocator>
class threadsafe_allocator : _Al
{
private:
    critical_section m_lock;
public:
    void* allocate(size_t size)
    {
        gate<critical_section>  lock(m_lock);
        return _Al::allocate(size);
    }
    void deallocate(void* ptr, size_t = 0)
    {
        gate<critical_section>  lock(m_lock);
        _Al::deallocate(ptr);
    }
};

#define MAXREQ 200
typedef threadsafe_allocator<free_list<MAXREQ, null_allocator<MAXREQ> > > threadsafe_fixed_size_allocator;


//
//  object type allocated to queue incoming requests into the threadpool
//
typedef struct _ThreadPoolEvent
{
    LPOVERLAPPED                                    m_lpOverlapped;
    DWORD                                           m_dwNumBytes;
    LPOVERLAPPED_COMPLETION_ROUTINE                 m_pfnCompletionRoutine;
    threadsafe_fixed_size_allocator*                m_pAllocatorPool;
} ThreadPoolEvent;


/*******************************************************************************
 *  CeIoCompletionCallback_t:
 *
 *      - manages windows ce io completion callback routines
 *      - waits for posted completion status' and dispatches handlers
 *      - creates a thread to monitor completions
 *      - allows for new completion routines and files to be bound into it
 *      - automatically manages overlapped structures being added and completed
 *
 *******************************************************************************/
class CeIoCompletionCallback_t
{
    private:
        auto_iocp                                       m_hIOCP;
        auto_handle                                     m_hThread;
        smart_ptr<SVSThreadPool>                        m_pThreadPool;
        critical_section                                m_lock;
        list<CeIoCompletionCallbackEntry_t>             m_list;
        threadsafe_fixed_size_allocator                 m_requestPool;

    public:
        CeIoCompletionCallback_t();
        ~CeIoCompletionCallback_t();

        // initialize the object and put into an active state
        BOOL Initialize();

        DWORD MonitorIoCompletion();                                                                // monitors io completions
        DWORD ProcessIoCompletion(ULONG_PTR ulCompKey, DWORD dwNumBytes, LPOVERLAPPED pOverlapped); // handles an io completion

        // associates the specified overlapped entry with the socket handle entry
        BOOL AssociateOverlappedWithSocket(SOCKET socket, LPOVERLAPPED lpOverlapped);

        // creates a new entry for the socket and completion routine
        BOOL AssociateSocketAndCompletionRoutine(SOCKET socket, LPOVERLAPPED_COMPLETION_ROUTINE pfnCompletionRoutine);

        // remove entries related to a particular socket
        BOOL DisassociateSocket(SOCKET socket);
};
