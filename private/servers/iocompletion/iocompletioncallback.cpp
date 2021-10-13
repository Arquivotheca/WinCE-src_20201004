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
#include <completioncallback.h>
#include <iocompletioncallback.h>
#include <iocompletionports.h>


//
// FORWARD DECLARATIONS
//
DWORD IoCompletionHandlerRoutine(LPVOID lpParam);
DWORD CeIoCompletionCallbackThread(LPVOID lpParam);


// ---------------------------------------
//
// CeIoCompletionCallbackEntry_t IMPLEMENTATION
//
// ---------------------------------------


// ctor
CeIoCompletionCallbackEntry_t::CeIoCompletionCallbackEntry_t() :    m_socket(INVALID_SOCKET), 
                                                                    m_overlappedList(NULL),
                                                                    m_pfnCompletionRoutine(NULL),
                                                                    m_socketClosing(FALSE)
{
    // only when adding an overlapped association, create the list
}


// specialized ctor
CeIoCompletionCallbackEntry_t::CeIoCompletionCallbackEntry_t(
            SOCKET socket, 
            LPOVERLAPPED_COMPLETION_ROUTINE pfnCompletionRoutine) : m_socket(socket), 
                                                                    m_overlappedList(NULL),
                                                                    m_pfnCompletionRoutine(pfnCompletionRoutine),
                                                                    m_socketClosing(FALSE)
{
    // only when adding an overlapped association, create the list
}


// copy ctor
CeIoCompletionCallbackEntry_t::CeIoCompletionCallbackEntry_t(
                                    const CeIoCompletionCallbackEntry_t& rhs) :
                                                                    m_socket(INVALID_SOCKET), 
                                                                    m_overlappedList(NULL),
                                                                    m_pfnCompletionRoutine(NULL),
                                                                    m_socketClosing(FALSE)
{
    m_socket = rhs.m_socket;
    m_overlappedList = rhs.m_overlappedList;
    m_pfnCompletionRoutine = rhs.m_pfnCompletionRoutine;
    m_socketClosing = rhs.m_socketClosing;
}


// dtor
CeIoCompletionCallbackEntry_t::~CeIoCompletionCallbackEntry_t()
{
}


// assignment operator
CeIoCompletionCallbackEntry_t CeIoCompletionCallbackEntry_t::operator=(
                                    const CeIoCompletionCallbackEntry_t& rhs)
{
    if(this == &rhs)
        {
        return *this;
        }

    m_socket = rhs.m_socket;
    m_overlappedList = rhs.m_overlappedList;
    m_pfnCompletionRoutine = rhs.m_pfnCompletionRoutine;
    m_socketClosing = rhs.m_socketClosing;
    return *this;
}



/*******************************************************************************
 *  AssociateOverlapped:
 *
 *      - add the overlapped pointer to the overlapped list if the socket matches
 *
 *******************************************************************************/
BOOL CeIoCompletionCallbackEntry_t::AssociateOverlapped(SOCKET socket, LPOVERLAPPED lpOverlapped)
{
    if(socket == m_socket)
        {

        // create the list if not already created
        if(!m_overlappedList)
            {
            m_overlappedList = new list<LPOVERLAPPED>;
            if(!m_overlappedList)
                {
                SetLastError( ERROR_OUTOFMEMORY );
                return FALSE;
                }
            }

        m_overlappedList->push_back(lpOverlapped);
        return TRUE;
        }

    return FALSE;
}


/*******************************************************************************
 * RemoveOverlapped:
 *
 *      - returns true if the overlapped is the in entries overlapped list
 *      - also returns the completion routine in this case
 *      - returns false otherwise
 *
 *******************************************************************************/
BOOL CeIoCompletionCallbackEntry_t::RemoveOverlapped(
            LPOVERLAPPED lpOverlapped, 
            LPOVERLAPPED_COMPLETION_ROUTINE* ppfnCompletionRoutine)
{
    list<LPOVERLAPPED>::iterator    it;
    list<LPOVERLAPPED>::iterator    itEnd;

    ASSERT(lpOverlapped);
    ASSERT(ppfnCompletionRoutine);
    ASSERT(m_overlappedList);

    if(!m_overlappedList)
        {
        SetLastError( ERROR_OUTOFMEMORY );
        return FALSE;
        }

    for(it = m_overlappedList->begin(), itEnd = m_overlappedList->end(); it != itEnd; ++it)
        {

        if(*it == lpOverlapped)
            {
            *ppfnCompletionRoutine = m_pfnCompletionRoutine;
            m_overlappedList->erase(it);
            return TRUE;
            }
        }

    return FALSE;
}


/*******************************************************************************
 *  MatchSocket:
 *
 *      - return true if the socket is the same as stored in the entry
 *      - returns false otherwise
 *
 *******************************************************************************/
BOOL CeIoCompletionCallbackEntry_t::MatchSocket(SOCKET socket)
{
    return (m_socket == socket);
}



/*******************************************************************************
 * ShouldRemoveEntry:
 *
 *      - returns true if the socket is closing
 *      - and the overlapped buffers list is empty
 *
 *******************************************************************************/
BOOL CeIoCompletionCallbackEntry_t::ShouldRemoveEntry(BOOL socketClosing)
{
    if(TRUE == socketClosing)
        {
        // ensure that no one can reference this entry again except
        // to reverse lookup overlapped buffers with completion routines
        m_socket = INVALID_SOCKET;
        m_socketClosing = socketClosing;
        }

    return (TRUE == m_socketClosing) && !!m_overlappedList && (m_overlappedList->empty());
}


// ---------------------------------------
//
// CeIoCompletionCallback_t IMPLEMENTATION
//
// ---------------------------------------


const size_t g_MaxThreadPoolSize = 1;


// ctor
CeIoCompletionCallback_t::CeIoCompletionCallback_t() :  m_hIOCP(NULL),
                                                        m_hThread(NULL),
                                                        m_pThreadPool(NULL)
{
    svsutil_Initialize();
}


// dtor
CeIoCompletionCallback_t::~CeIoCompletionCallback_t()
{
    svsutil_DeInitialize();

    //
    //  svsthreadpool shutdown should have occured when the handler thread shutsdown
    //
}



/*******************************************************************************
 *  Initialize:
 *
 *      - creates an unbinded io completion port
 *      - creates a thread to monitor the completion port
 *
 *******************************************************************************/
BOOL CeIoCompletionCallback_t::Initialize()
{
    // create io completion port
    m_hIOCP = CeCreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if( NULL == m_hIOCP )
        {
        // io completion port was unable to be setup properly
        // unable to create io completion port
        SetLastError( ERROR_GEN_FAILURE );
        ASSERT(false);
        goto Finish;
        }

    m_pThreadPool = new SVSThreadPool( g_MaxThreadPoolSize );
    if( NULL == m_pThreadPool )
        {
        //
        //  io completion port monitoring thread unable to setup properly
        //  out of memory creating threadpool
        //
        SetLastError( ERROR_OUTOFMEMORY );
        ASSERT(false);
        goto Finish;
        }

    // create io completion port monitoring thread
    m_hThread = CreateThread(NULL, 0, CeIoCompletionCallbackThread, (LPVOID)this, 0, NULL);
    if( NULL == m_hThread )
        {
        //
        // io completion port monitoring thread unable to setup properly
        // error unable to create completion port monitoring thread
        // return error out of memory
        //
        SetLastError( ERROR_OUTOFMEMORY );
        ASSERT(false);
        goto Finish;
        }

    return TRUE;
Finish:
    return FALSE;
}



/*******************************************************************************
 *  AssociateOverlappedWithSocket:
 *
 *      - associate the overlapped structure with the socket
 *      - typically occurs when WSASend, WSASendTo, WSARecv, WSARecvFrom called
 *
 *******************************************************************************/
BOOL CeIoCompletionCallback_t::AssociateOverlappedWithSocket(SOCKET socket, LPOVERLAPPED lpOverlapped)
{
    list<CeIoCompletionCallbackEntry_t>::iterator   it;
    list<CeIoCompletionCallbackEntry_t>::iterator   itEnd;
    BOOL                                            retCode = FALSE;

    {
    gate<critical_section> lock(m_lock);

    for(it = m_list.begin(), itEnd = m_list.end(); it != itEnd; ++it)
        {
        if(it->AssociateOverlapped(socket, lpOverlapped))
            {
            // found the overlapped entries association
            retCode = TRUE;
            break;
            }
        }

    // lock automatically released
    }

    return retCode;
}


/*******************************************************************************
 *  AssociateSocketAndCompletionRoutine:
 *
 *      - ensure that socket is not already bound
 *      - create a new callback entry
 *      - bind the socket to the iocompletion port
 *
 *******************************************************************************/
BOOL CeIoCompletionCallback_t::AssociateSocketAndCompletionRoutine(SOCKET socket, LPOVERLAPPED_COMPLETION_ROUTINE pfnCompletionRoutine)
{
    list<CeIoCompletionCallbackEntry_t>::iterator   it;
    list<CeIoCompletionCallbackEntry_t>::iterator   itEnd;
    CeIoCompletionCallbackEntry_t                   callback(socket, pfnCompletionRoutine);
    BOOL                                            retCode = FALSE;
    gate<critical_section>                          lock(m_lock);

    for(it = m_list.begin(), itEnd = m_list.end(); it != itEnd; ++it)
        {
        if(it->MatchSocket(socket))
            {
            //
            // error attempting to bind socket twice
            //
            SetLastError( ERROR_INVALID_PARAMETER );
            ASSERT(false);
            goto Finish;
            }
        }

    // socket not yet associated
    HANDLE hPort = CeCreateIoCompletionPort((HANDLE) socket, m_hIOCP, 0, 0);
    if(NULL == hPort)
        {
        ASSERT(false);
        goto Finish;
        }

    // insert entry at back of callback list using copy constructor
    m_list.push_back(callback);
    retCode = TRUE;

Finish:
    return retCode;
}



/*******************************************************************************
 *  DisassociateSocket:
 *
 *      - remove callback entry
 *      - typically occurs when the socket is closing
 *
 *******************************************************************************/
BOOL CeIoCompletionCallback_t::DisassociateSocket(SOCKET socket)
{
    list<CeIoCompletionCallbackEntry_t>::iterator   it;
    list<CeIoCompletionCallbackEntry_t>::iterator   itEnd;
    BOOL                                            retCode = FALSE;

    {
    gate<critical_section>                          lock(m_lock);

    for(it = m_list.begin(), itEnd = m_list.end(); it != itEnd; ++it)
        {
        if(it->MatchSocket(socket))
            {
            // if should remove entry - remove callback entry from list using equality
            // else the entry is marked as should remove
            // the entry will wait until all outstanding overlapped buffers are completed
            if(it->ShouldRemoveEntry(TRUE))
                {
                m_list.erase(it);
                }

            retCode = TRUE;
            break;
            }
        }

    // lock automatically released
    }

    return retCode;
}


/*******************************************************************************
 *  MonitorIoCompletion:
 *
 *      - monitors the queued completion status and fires off handlers when received
 *
 *******************************************************************************/
DWORD CeIoCompletionCallback_t::MonitorIoCompletion()
{
    DWORD dwNumBytesTransferred;
    ULONG_PTR ulCompKey;
    LPOVERLAPPED lpOverlapped;

    for(;;)
        {
        if(CeGetQueuedCompletionStatus(m_hIOCP, &dwNumBytesTransferred, &ulCompKey, &lpOverlapped, INFINITE))
            {
            if(lpOverlapped)
                {
                this->ProcessIoCompletion(ulCompKey, dwNumBytesTransferred, lpOverlapped);
                }
            else
                {
                //
                // shutdown requested
                // shutdown the threadpool
                //
                m_pThreadPool->Shutdown( false );
                goto Finish;
                }
            }
        else
            {
            //
            //  retailmsg output here as this indicates a fatal error
            //  should investigate if this occurs
            //
            RETAILMSG(true, (L"Unexpected error retrieving queued completion status 0x%08x", GetLastError()));
            ASSERT(false);
            }
        }

  Finish:
    return ERROR_SUCCESS;
}


/*******************************************************************************
 *  ProcessIoCompletion:
 *
 *      - find the callback entry via the overlapped association
 *      - remove the overlapped association and call the callback routine
 *
 *      - DESIGN NOTE:
 *          if moving to a threadpool implementation - replace this routine
 *
 *******************************************************************************/
DWORD CeIoCompletionCallback_t::ProcessIoCompletion(ULONG_PTR ulCompKey, DWORD dwNumBytes, LPOVERLAPPED pOverlapped)
{
    list<CeIoCompletionCallbackEntry_t>::iterator   it;
    list<CeIoCompletionCallbackEntry_t>::iterator   itEnd;
    LPOVERLAPPED_COMPLETION_ROUTINE                 pfnCompletionRoutine = NULL;

    {
    gate<critical_section> lock(m_lock);

    for(it = m_list.begin(), itEnd = m_list.end(); it != itEnd; ++it)
        {
        if(it->RemoveOverlapped(pOverlapped, &pfnCompletionRoutine))
            {
            if(it->ShouldRemoveEntry(FALSE))
                {
                // remove the entry if all outstanding overlapped buffers completed
                m_list.erase(it);
                }

            // found the overlapped entries association
            break;
            }
        }

    // lock automatically released
    }

    //
    // NOTE:  do not hold locks when calling the completion routine
    //
    if(pfnCompletionRoutine)
        {
        ThreadPoolEvent* pThreadPoolEvent = (ThreadPoolEvent*) m_requestPool.allocate( sizeof(ThreadPoolEvent) );
        if( NULL == pThreadPoolEvent )
            {
            //
            //  out of entries in the pool - fail the request since we are getting hammered
            //  the sender needs to start throttling down the requests to our endpoint
            //
            RETAILMSG(true, (L"IOCC: OOM thread pool request pool is full")); 
            ASSERT(false);
            }
        else
            {
            pThreadPoolEvent->m_lpOverlapped = pOverlapped;
            pThreadPoolEvent->m_dwNumBytes = dwNumBytes;
            pThreadPoolEvent->m_pfnCompletionRoutine = pfnCompletionRoutine;
            pThreadPoolEvent->m_pAllocatorPool = &m_requestPool;

            //
            //  schedule event to be handled by some thread in the pool
            //
            m_pThreadPool->ScheduleEvent( IoCompletionHandlerRoutine, (LPVOID) pThreadPoolEvent );
            }

        }
    else
        {
        //
        //  retailmsg output here as this indicates a fatal error
        //  should investigate if this occurs
        //
        RETAILMSG(true, (L"Unexpected error: overlapped not found [0x%08x]", GetLastError()));
        ASSERT(false);
        }


    //
    // NOTE:  this routine suppresses errors and sends debug output
    //
    return ERROR_SUCCESS;
}



/*******************************************************************************
 *  IoCompletionHandlerRoutine:
 *
 *      - call the users completion callback routine
 *      - thread routine
 *
 *******************************************************************************/
DWORD IoCompletionHandlerRoutine(LPVOID lpParam)
{
    ASSERT( NULL != lpParam );

    //
    //  copy threadpool event object and release the pool object
    //
    ThreadPoolEvent  threadPoolEvent = *((ThreadPoolEvent*) lpParam);
    threadPoolEvent.m_pAllocatorPool->deallocate( lpParam );


    //
    //  send the request to the completion callback
    //
    threadPoolEvent.m_pfnCompletionRoutine(
                                    threadPoolEvent.m_lpOverlapped->OffsetHigh,
                                    threadPoolEvent.m_dwNumBytes,
                                    threadPoolEvent.m_lpOverlapped);
    return ERROR_SUCCESS;
}


/*******************************************************************************
 *  CeIoCompletionCallbackThread:
 *
 *      - thread routine simply calls object method
 *
 *******************************************************************************/
DWORD CeIoCompletionCallbackThread(LPVOID lpParam)
{
    ASSERT(lpParam);    // fundamental implementation error

    CeIoCompletionCallback_t* pCompletionCallback = (CeIoCompletionCallback_t*)lpParam;
    return pCompletionCallback->MonitorIoCompletion();
}
