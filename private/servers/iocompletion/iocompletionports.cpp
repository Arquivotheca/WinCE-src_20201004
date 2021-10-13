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

//
// I/O Completion port emulation:
//
// Emulates generic I/O completion port behavior through PostQueuedCompletionStatus and GetQueuedCompletionStatus
// and Winsock SOCKET associated with I/O completion ports by layering in a WSARecv/WSARecvFrom/WSASend/WSASendTo
// shim to trap overlapped completion.
//
// CeCloseIoCompletionPort must be used instead of CloseHandle since the CE kernel does not know about IoCompletionPorts
//

#include <windows.h>
#include <winsock2.h>
#include <linklist.h>
#include <LockFreeSpinInitializer.hxx>

#ifdef __cplusplus
extern "C" {
#endif


//
//  DEBUG NOTE:
//      to turn on debug zones for this component
//      mark the debug flags with value 1 and rebuild
//
#define DEBUG_IOCP      0
#define DEBUG_IOCP_API  0
#define DEBUG_IOCP_LIST 0


#ifdef DEBUG
void DebugLinkedList( LPCWSTR pszList, PLIST_ENTRY listHead, DWORD listSize )
{
    DEBUGMSG(DEBUG_IOCP_LIST, (TEXT("LinkedList: [%s]\r\n"), pszList));
    DEBUGMSG(DEBUG_IOCP_LIST, (TEXT("LinkedList:\tHEntry[0x%08x] Flink[0x%08x] Blink[0x%08x] ListSize[%d]\r\n"), listHead, listHead->Flink, listHead->Blink, listSize));

    PLIST_ENTRY entry = listHead->Flink;
    while( ( NULL != entry ) && ( listHead != entry ) ) {
        DEBUGMSG(DEBUG_IOCP_LIST, (TEXT("LinkedList:\t\tLEntry[0x%08x] Flink[0x%08x] Blink[0x%08x]\r\n"), entry, entry->Flink, entry->Blink));
        entry = entry->Flink;
    }
}
#define DumpLinkedList DebugLinkedList
#else
#define DumpLinkedList
#endif


BOOL SetEventData(HANDLE Event, DWORD Data);
DWORD GetEventData(HANDLE Event);

// client can overwrite the g_Priority256 value to change the threads priority
DWORD g_Priority256 = CE_THREAD_PRIO_256_NORMAL;

//
// Structure to emulate XP I/O completion ports
//
typedef struct _IO_COMPLETION_PORT {
    CRITICAL_SECTION Lock;
    LIST_ENTRY       GlobalList;      // global list of IO_COMPLETION_PORTs
    LIST_ENTRY       CompletionList;  // list of queued completions (IO_COMPLETION_STATUSs)
    HANDLE           Event;
    DWORD            cWaitingThreads;
    volatile LONG    RefCnt;
    BOOL             Closed;
} IO_COMPLETION_PORT, * PIO_COMPLETION_PORT;

//
// I/O completion objects queued to an I/O completion port
//
typedef struct _IO_COMPLETION_STATUS {
    LIST_ENTRY      List;
    LPOVERLAPPED    lpOverlapped;
    ULONG_PTR       CompletionKey;
    DWORD           dwNumberOfBytesTransferred;
    BYTE            fPostedByPostQueue;
} IO_COMPLETION_STATUS, *PIO_COMPLETION_STATUS;

//
// Structure to track association between a socket handle and an I/O completion port.
// Multiple sockets may be associated with one I/O completion port.
//
typedef struct _IO_COMPLETION_SOCKET {
    LIST_ENTRY      GlobalList;
    SOCKET          Socket;
    ULONG_PTR       CompletionKey;
    PIO_COMPLETION_PORT Port;
    volatile LONG   RefCnt;
    HANDLE          RecvEvent;
    HANDLE          SendEvent;
    LIST_ENTRY      PendingRecvList;  // list of pending IO_COMPLETION_STATUSs
    LIST_ENTRY      PendingSendList;  // list of pending IO_COMPLETION_STATUSs
} IO_COMPLETION_SOCKET, *PIO_COMPLETION_SOCKET;

//
// IoCompletionPort globals.
//
LockFreeSpinInitializer IoCompletionPortSystemInitialized;
CRITICAL_SECTION        IoCompletionPortLock;
LIST_ENTRY              IoCompletionPortList;
LIST_ENTRY              IoSocketList;
DWORD                   IoSocketListLen;

//
// IoSocket Thread Control
//
HANDLE IoSocketListChangedEvent;
BOOL   IoSocketCompletionThreadRunning;

DWORD IoSocketCompletionThread(LPVOID Nothing); // implemented in this file

#define IO_COMPLETION_STATUS_FREE_LIST_SIZE 10
PIO_COMPLETION_STATUS IoCompletionStatusFreeList[IO_COMPLETION_STATUS_FREE_LIST_SIZE];

// Forward referenes
void PostCompleteAsyncIo(
    PIO_COMPLETION_SOCKET IoSocket,
    PIO_COMPLETION_STATUS IoStatus,
    DWORD BytesTransferred,
    DWORD WSFlags,
    DWORD Status
    );

//
// Employ an "interlocked cache array" to avoid excessive LocalAlloc calls
//
PIO_COMPLETION_STATUS
AllocIoCompletionStatus(void)
{
    DWORD i;
    PIO_COMPLETION_STATUS IoCompletionStatus = NULL;

    for (i = 0; i < IO_COMPLETION_STATUS_FREE_LIST_SIZE; i++) {
        //
        // Find one in the cache
        //
        if ( NULL != IoCompletionStatusFreeList[i]) {
            //
            // Try to allocate it
            //
            IoCompletionStatus = (PIO_COMPLETION_STATUS) InterlockedExchange( (LONG*)&IoCompletionStatusFreeList[i], 0 );
            if ( NULL != IoCompletionStatus) {
                //
                // Got one!
                //
                DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  Alloc IoCompletionStatus[0x%08x] FreeListIndex[%d]\r\n"), IoCompletionStatus, i));
                return IoCompletionStatus;
            }
        }
    }

    IoCompletionStatus = (PIO_COMPLETION_STATUS) LocalAlloc( LPTR, sizeof(IO_COMPLETION_STATUS) );
    ASSERT( NULL != IoCompletionStatus );
    DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  Alloc IoCompletionStatus[0x%08x] NEW\r\n"), IoCompletionStatus));
    return IoCompletionStatus;
}

//
// Employ an "interlocked cache array" to avoid excessive LocalFree calls
//
void
FreeIoCompletionStatus(
    PIO_COMPLETION_STATUS IoCompletionStatus
    )
{
    DWORD i = 0;

    ASSERT( NULL != IoCompletionStatus );

    for (i = 0; i < IO_COMPLETION_STATUS_FREE_LIST_SIZE; i++) {
        //
        // Find an empty slot in the cache
        //
        if ( NULL == IoCompletionStatusFreeList[i] ) {
            //
            // Try to free it
            //
            IoCompletionStatus = (PIO_COMPLETION_STATUS) InterlockedExchange( (LONG*)&IoCompletionStatusFreeList[i], (DWORD)IoCompletionStatus );
            if ( NULL == IoCompletionStatus ) {
                //
                // Done!
                //
                ASSERT( NULL != IoCompletionStatusFreeList[i] );
                DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  Free IoCompletionStatus[0x%08x] FreeListIndex[%d]\r\n"), IoCompletionStatusFreeList[i], i));
                return;
            }
        }
    }

    //
    // No empty slots, just free the memory.
    //
    ASSERT( NULL != IoCompletionStatus );

    DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  Free IoCompletionStatus[0x%08x] LocalFree\r\n"), IoCompletionStatus));
    LocalFree( IoCompletionStatus );
}


_inline
void
RefIoSocket(
    PIO_COMPLETION_SOCKET IoSocket
    )
{
    EnterCriticalSection( &IoCompletionPortLock );

    ASSERT( NULL != IoSocket );
    ASSERT( 0 < IoSocket->RefCnt );

    LONG refCnt = InterlockedIncrement( (LPLONG) &(IoSocket->RefCnt) );
    DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  AddRef IoSocket[0x%08x] RefCount[%d]\r\n"), IoSocket, refCnt));

    LeaveCriticalSection( &IoCompletionPortLock );
}


//
// Scan the IO_COMPLETION_SOCKET list IoSocketList for one matching the given SOCKET.
// If found, the IO_COMPLETION_SOCKET's ref count is incremented.
//
PIO_COMPLETION_SOCKET
FindAndRefIoSocket(
    SOCKET Socket
    )
{
    PLIST_ENTRY Entry = NULL;
    PIO_COMPLETION_SOCKET IoSocket = NULL;

    EnterCriticalSection( &IoCompletionPortLock );
    Entry = IoSocketList.Flink;
    while ( Entry != &IoSocketList ) {
        IoSocket = CONTAINING_RECORD( Entry, IO_COMPLETION_SOCKET, GlobalList );
        ASSERT( NULL != IoSocket );
        ASSERT( (PIO_COMPLETION_SOCKET) 0xcccccccc != IoSocket );
        Entry = Entry->Flink;
        if ( IoSocket->Socket == Socket ) {
            RefIoSocket( IoSocket );
            DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  FindAndRefIoSocket IoSocket[0x%08x] Socket[%d]\r\n"), IoSocket, Socket));
            LeaveCriticalSection( &IoCompletionPortLock );

            return IoSocket;

        }
    }
    LeaveCriticalSection( &IoCompletionPortLock );

    DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  FindAndRefIoSocket IoSocket[NULL] Socket[%d]\r\n"), Socket));
    return NULL;
}


void
DerefIoCompletionPort(
    PIO_COMPLETION_PORT Port
    )
{
    PLIST_ENTRY Entry = NULL;
    PIO_COMPLETION_STATUS CompletionStatus = NULL;
    ASSERT( 0xcccccccc != Port->RefCnt );
    LONG refCnt = InterlockedDecrement( (LPLONG) &(Port->RefCnt) );
    DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  DerefIoCompletionPort Port[0x%08x] RefCnt[%d]\r\n"), Port, refCnt));

    if ( 0 == refCnt ) {
        EnterCriticalSection( &IoCompletionPortLock );
        RemoveEntryList( &Port->GlobalList );
        LeaveCriticalSection( &IoCompletionPortLock );

        //
        // Drop any remaining I/Os
        //
        EnterCriticalSection( &Port->Lock );
        Entry = Port->CompletionList.Flink;
        while ( Entry != &Port->CompletionList ) {
            CompletionStatus = (PIO_COMPLETION_STATUS) CONTAINING_RECORD( Entry, IO_COMPLETION_STATUS, List );
            Entry = Entry->Flink;
            FreeIoCompletionStatus( CompletionStatus );
        }

        CloseHandle( Port->Event );
        LeaveCriticalSection( &Port->Lock );
        DeleteCriticalSection( &Port->Lock );

        LocalFree( Port );
    }
}   // DerefIoCompletionPort

void
DerefIoSocket(
    PIO_COMPLETION_SOCKET IoSocket
    )
{
    EnterCriticalSection( &IoCompletionPortLock );

    ASSERT( 0xcccccccc != IoSocket->RefCnt );

    LONG refCnt = InterlockedDecrement( (LPLONG) &(IoSocket->RefCnt) );
    DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  DerefIoSocket IoSocket[0x%08x] RefCnt[%d]\r\n"), IoSocket, refCnt));

    if ( 0 == refCnt ) {
        ASSERT( 0 >= IoSocket->RefCnt );

        DumpLinkedList( L"IoSocketList [PreRemove]", &IoSocketList, IoSocketListLen );
        RemoveEntryList( &IoSocket->GlobalList );
        IoSocketListLen--;
        DumpLinkedList( L"IoSocketList [PostRemove]", &IoSocketList, IoSocketListLen );

        EnterCriticalSection( &IoSocket->Port->Lock );

        CloseHandle( IoSocket->RecvEvent );
        CloseHandle( IoSocket->SendEvent );

        DerefIoCompletionPort( IoSocket->Port );
        LeaveCriticalSection( &IoSocket->Port->Lock );

        DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  DerefIoSocket [Delete] IoSocket[0x%08x] RefCnt[%d]\r\n"), IoSocket, IoSocket->RefCnt));
        LocalFree( IoSocket );
    }
    
    LeaveCriticalSection( &IoCompletionPortLock );
}


//
// Remove all of the IO_COMPLETION_STATUSs from the specified list
//
void
CleanupCompletionList(
    PIO_COMPLETION_SOCKET IoSocket,
    PLIST_ENTRY ListHead
    )
{
    PLIST_ENTRY Entry = NULL;
    PIO_COMPLETION_STATUS CompletionStatus = NULL;

    Entry = ListHead->Flink;
    while ( Entry != ListHead ) {
        CompletionStatus = (PIO_COMPLETION_STATUS) CONTAINING_RECORD( Entry, IO_COMPLETION_STATUS, List );
        Entry = Entry->Flink;
        DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP:  CleanupCompletionList IoSocket[0x%08x] ListHead[0x%08x] CompletionStatus[0x%08x]\r\n"), IoSocket, ListHead, CompletionStatus));
        PostCompleteAsyncIo( IoSocket, CompletionStatus, 0, 0, WSA_OPERATION_ABORTED );
    }
}


//
// Break the association between an IO_COMPLETION_SOCKET and an IO_COMPLETION_PORT
//
void
DisassociateIoSocket(
    PIO_COMPLETION_SOCKET IoSocket
    )
{
    //
    //  we need to hold the global lock during disassociation
    //  this is to prevent race conditions with others accessing
    //  the IoSocket we are attempting to remove
    //
    EnterCriticalSection( &IoCompletionPortLock );

    EnterCriticalSection( &IoSocket->Port->Lock );
    CleanupCompletionList( IoSocket, &IoSocket->PendingRecvList );
    CleanupCompletionList( IoSocket, &IoSocket->PendingSendList );
    LeaveCriticalSection( &IoSocket->Port->Lock );

    //
    //  this should be the last reference held
    //  it can be deleted/cleaned up here or in
    //  IoSocketCompletionThread
    //
    DerefIoSocket( IoSocket );

    //
    //  we signal the IoSocketCompletionThread
    //  which will awake and block on the global lock
    //
    SetEvent( IoSocketListChangedEvent );

    LeaveCriticalSection( &IoCompletionPortLock );
}


//
// Start IoSocketCompletionThread if it is not running
//
// Return FALSE if it cannot be started
//
BOOL StartIoSocketCompletionThreadIfNeeded(void)
{
    HANDLE Thread;

    EnterCriticalSection( &IoCompletionPortLock );
    if ( NULL != IoSocketCompletionThreadRunning ) {
        LeaveCriticalSection( &IoCompletionPortLock );
        return TRUE;
    }

    Thread = CreateThread( NULL, 0, IoSocketCompletionThread, NULL, 0, NULL );
    if ( NULL != Thread ) {
        IoSocketCompletionThreadRunning = TRUE;
        LeaveCriticalSection( &IoCompletionPortLock );
        CeSetThreadPriority( Thread, g_Priority256 );
        CloseHandle( Thread );
        return TRUE;
    }
    return FALSE;
}   // StartIoSocketCompletionThreadIfNeeded



//
// OneTimeInitializeIoCompletionPorts is used to perform onetime init
//
void OneTimeInitializeIoCompletionPorts()
{
    InitializeCriticalSection( &IoCompletionPortLock );
    InitializeListHead( &IoCompletionPortList );
    InitializeListHead( &IoSocketList );
    IoSocketListChangedEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
}

//
// CeCreateIoCompletionPort is used to either create a new IoCompletionPort or to
// associate a HANDLE (SOCKET) with an existing IoCompletionPort.
//
// The creation phase will return a Win32 event that has an IO_COMPLETION_PORT
// object associated with it via SetEventData
//
HANDLE
WINAPI
CeCreateIoCompletionPort(
    __in     HANDLE FileHandle,
    __in_opt HANDLE ExistingCompletionPort,
    __in     ULONG_PTR CompletionKey,
    __in     DWORD NumberOfConcurrentThreads
    )
{
    // performs initialization if necessary
    IoCompletionPortSystemInitialized.InitializeIfNecessary( OneTimeInitializeIoCompletionPorts );

    DEBUGMSG(DEBUG_IOCP_API, (TEXT("ICOP: [API] CreateIoCompletionPort FileHandle[0x%08x] ExistingPort[0x%08x] Key[%d]\r\n"), FileHandle, ExistingCompletionPort, CompletionKey));

    if ( INVALID_HANDLE_VALUE == FileHandle ) {
        //
        // Create a new IoCompletion port
        //
        PIO_COMPLETION_PORT Port = NULL;

        //
        // Create a context object and an event and associate the context object with the event.
        //
        Port = (PIO_COMPLETION_PORT) LocalAlloc( LPTR, sizeof(IO_COMPLETION_PORT) );

        if ( NULL != Port ) {
            Port->Event = CreateEvent( NULL, FALSE, FALSE, NULL );
            if ( NULL != Port->Event ) {
                InitializeCriticalSection( &Port->Lock );
                InitializeListHead( &Port->CompletionList );
                Port->cWaitingThreads = 0;
                Port->RefCnt = 1;
                Port->Closed = FALSE;
                SetEventData( Port->Event, (DWORD)Port );
                EnterCriticalSection( &IoCompletionPortLock );

                //
                // If this is the 1st port, start the Socket waiting IO thread
                //
                if( TRUE == IsListEmpty( &IoCompletionPortList ) ) {
                    StartIoSocketCompletionThreadIfNeeded();
                }
                InsertTailList( &IoCompletionPortList, &Port->GlobalList );
                LeaveCriticalSection( &IoCompletionPortLock );

                DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP: CreateIoCompletionPort Port[0x%08x] Port->Event[0x%08x] Port->RefCnt[%d]\r\n"), Port, Port->Event, Port->RefCnt));
                return Port->Event;
            }

            LocalFree( Port );
        }
    } else {

    //
    // Associate a file handle (SOCKET) with an existing IoCompletionPort
    //
        PIO_COMPLETION_SOCKET IoSocket = NULL;
        PIO_COMPLETION_PORT   Port = (PIO_COMPLETION_PORT) GetEventData( ExistingCompletionPort );

        if ( NULL == Port ) {
            SetLastError( ERROR_INVALID_HANDLE );
            return NULL;
        }

        //
        // Make sure an association has not already been made
        //
        IoSocket = FindAndRefIoSocket( (SOCKET)FileHandle );
        if ( NULL != IoSocket ) {
            Port = IoSocket->Port;
            DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP: CreateIoCompletionPort Association Already Exists Port->Event[0x%08x] IoSocket[0x%08x]\r\n"), Port->Event, IoSocket));
            DerefIoSocket( IoSocket );
            return Port->Event;
        }

        //
        // The thread uses WaitForMultipleObjects with a send and receive event per socket
        // and IoSocketListChangedEvent, so it cannot handle more than 31 sockets.
        //
        if ( IoSocketListLen < ((MAXIMUM_WAIT_OBJECTS-1)/2) ) {
            //
            // Allocate and initialize a new IO_COMPLETION_SOCKET to track the port/socket
            // association
            //
            IoSocket = (PIO_COMPLETION_SOCKET) LocalAlloc( LPTR, sizeof(IO_COMPLETION_SOCKET) );
            if ( NULL != IoSocket ) {
                IoSocket->RecvEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
                IoSocket->SendEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
                if ( IoSocket->RecvEvent && IoSocket->SendEvent ) {
                    InterlockedIncrement( (LPLONG) &(Port->RefCnt) );
                    IoSocket->RefCnt = 1;
                    InitializeListHead( &IoSocket->PendingRecvList );
                    InitializeListHead( &IoSocket->PendingSendList );
                    IoSocket->Port = Port;
                    IoSocket->Socket = (SOCKET) FileHandle;
                    IoSocket->CompletionKey = CompletionKey;

                    EnterCriticalSection( &IoCompletionPortLock );
                    DumpLinkedList( L"IoSocketList [PreInsert]", &IoSocketList, IoSocketListLen );
                    InsertTailList( &IoSocketList, &IoSocket->GlobalList );
                    IoSocketListLen++;
                    DumpLinkedList( L"IoSocketList [PostInsert]", &IoSocketList, IoSocketListLen );
                    LeaveCriticalSection( &IoCompletionPortLock );

                    SetEvent( IoSocketListChangedEvent );
                    DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP: CreateIoCompletionPort Port[0x%08x] IoSocket[0x%08x] IoSocket->RefCnt[%d]\r\n"), Port, IoSocket, IoSocket->RefCnt));
                    return Port->Event;

                } else {
                    //
                    // Clean up after failure
                    //
                    if ( NULL != IoSocket->RecvEvent ) {
                        CloseHandle( IoSocket->RecvEvent );
                    }
                    if ( NULL != IoSocket->SendEvent ) {
                        CloseHandle( IoSocket->SendEvent );
                    }
                }
                ASSERT( NULL != IoSocket );
                LocalFree( IoSocket );
            }
        } else {
            RETAILMSG(1, (L"IOCP:CreateIoCompletionPort - already %d sockets associated!\r\n", IoSocketListLen));
            ASSERT( FALSE );
        }
    }

    SetLastError( ERROR_OUTOFMEMORY );

    return NULL;
}   // CreateIoCompletionPort


//
// On CE, an IoCompletionPort must be closed with CeCloseIoCompletionPort since CloseHandle
// does not know about IoCompletionPorts.
//
BOOL
CeCloseIoCompletionPort(
    __in HANDLE CompletionPort
    )
{
    PLIST_ENTRY Entry = NULL;
    PIO_COMPLETION_SOCKET IoSocket = NULL;
    PIO_COMPLETION_PORT Port = (PIO_COMPLETION_PORT) GetEventData( CompletionPort );
    if (NULL == Port) {
        // unknown completion port - defer to system close handle
        return CloseHandle( CompletionPort );
    }

    DEBUGMSG(DEBUG_IOCP_API, (TEXT("ICOP: [API] CloseIoCompletionPort PortHandle[0x%08x] Port[0x%08x]\r\n"), CompletionPort, Port));


    //
    // Make sure the port is no longer exposed
    //
    SetEventData( CompletionPort, (DWORD)0 );
    EnterCriticalSection( &IoCompletionPortLock );
    Port->Closed = TRUE;
    RemoveEntryList( &Port->GlobalList );

    //
    // Find any associated SOCKETs and break the port/socket association
    //
    EnterCriticalSection( &Port->Lock );
    Entry = IoSocketList.Flink;
    while ( Entry != &IoSocketList ) {
        IoSocket = CONTAINING_RECORD( Entry, IO_COMPLETION_SOCKET, GlobalList );
        Entry = Entry->Flink;
        if ( IoSocket->Port == Port ) {
            DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP: CloseIoCompletionPort DisassociateSocket Port[0x%08x] IoSocket[0x%08x] IoSocket->RefCnt[%d]\r\n"), Port, IoSocket, IoSocket->RefCnt));
            DisassociateIoSocket( IoSocket );
        }
    }


    //
    // Signal any remaining waiting threads
    //
    while ( NULL != Port->cWaitingThreads ) {
        SetEvent( Port->Event );
    }
    LeaveCriticalSection( &Port->Lock );
    LeaveCriticalSection( &IoCompletionPortLock );


    DerefIoCompletionPort( Port );

    EnterCriticalSection( &IoCompletionPortLock );

    //
    // If this is the last port, signal to stop the Socket waiting IO thread
    //
    if( TRUE == IsListEmpty( &IoCompletionPortList ) )
    {
        SetEvent( IoSocketListChangedEvent );
    }

    LeaveCriticalSection( &IoCompletionPortLock );

    return TRUE;
}


//
// Return the next completed overlapped operation on this IoCompletionPort after potentially waiting
//
BOOL
WINAPI
CeGetQueuedCompletionStatus(
    __in  HANDLE CompletionPort,
    __out LPDWORD lpNumberOfBytesTransferred,
    __out PULONG_PTR lpCompletionKey,
    __out LPOVERLAPPED *lpOverlapped,
    __in  DWORD Milliseconds
    )
{
    PIO_COMPLETION_STATUS CompletionStatus = NULL;
    PIO_COMPLETION_PORT Port = NULL;
    BOOL IsEmpty = FALSE;
    BOOL result = TRUE;

    //
    // Check parameters
    //
    if ( ( NULL == CompletionPort )             ||
         ( NULL == lpNumberOfBytesTransferred ) ||
         ( NULL == lpCompletionKey )            ||
         ( NULL == lpOverlapped ) )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    //
    // Initialize out parameters
    //
    *lpNumberOfBytesTransferred = 0;
    *lpCompletionKey = 0;
    *lpOverlapped = NULL;

    //
    // Get port context
    //
    Port = (PIO_COMPLETION_PORT) GetEventData( CompletionPort );
    if ( NULL == Port ) {
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }

    DEBUGMSG(DEBUG_IOCP_API, (TEXT("ICOP: [API] GetQueuedCompletionStatus Port[0x%08x]\r\n"), Port));

    EnterCriticalSection( &Port->Lock );

    while ( TRUE == IsListEmpty( &Port->CompletionList ) ) {
        Port->cWaitingThreads++;
        LeaveCriticalSection( &Port->Lock );

        //
        // No completions queued, so wait
        //
        WaitForSingleObject( Port->Event, Milliseconds );

        EnterCriticalSection( &Port->Lock );
        Port->cWaitingThreads--;
        if ( TRUE == Port->Closed ) {
            //
            // Port was closed out from under this thread, so exit.
            //
            LeaveCriticalSection( &Port->Lock );
            SetLastError( ERROR_INVALID_HANDLE );
            return FALSE;
        }
    }

    //
    // Remove and return the first queued completion.
    //
    CompletionStatus = (PIO_COMPLETION_STATUS) RemoveHeadList( &Port->CompletionList );
    LeaveCriticalSection( &Port->Lock );

    *lpCompletionKey    = CompletionStatus->CompletionKey;
    *lpOverlapped       = CompletionStatus->lpOverlapped;

    // if posted by PostQueuedCompletionStatus, the value is not stored in OVERLAPPED structure.
    if( TRUE == CompletionStatus->fPostedByPostQueue )
    {
        *lpNumberOfBytesTransferred = CompletionStatus->dwNumberOfBytesTransferred;
    }
    else if ( NULL != CompletionStatus->lpOverlapped ) {
        *lpNumberOfBytesTransferred = CompletionStatus->lpOverlapped->InternalHigh;
        if( CompletionStatus->lpOverlapped->OffsetHigh == WSA_OPERATION_ABORTED )
        {
            SetLastError( WSA_OPERATION_ABORTED );
            //
            //  operation aborted does not require special handling
            //  CompletionStatus->lpOverlapped->OffsetHigh indicates the error code
            //  particular completions should always be returned and the error read from there
            //
            result = TRUE;
        }
    }

    FreeIoCompletionStatus( CompletionStatus );

    DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP: GetQueuedCompletionStatus Port[0x%08x] Overlapped[0x%08x]\r\n"), Port, lpOverlapped));
    return result;

}   // GetQueuedCompletionStatus


//
//  only insert the completion status into the completed list when GetQueuedCompletionStatus will be called
//  in this case we differentiate whether or not this will occur based on the existence of the completion routine
//
void
InternalPostQueuedCompletionStatus(
    PIO_COMPLETION_PORT Port,
    PIO_COMPLETION_STATUS CompletionStatus
    )
{
    BOOL NeedSignal = FALSE;

    //
    // Queue it
    //
    EnterCriticalSection( &Port->Lock );
    NeedSignal = ( Port->cWaitingThreads != 0 );
    InsertTailList( &Port->CompletionList, &CompletionStatus->List );
    LeaveCriticalSection( &Port->Lock );

    //
    // Signal the port
    //
    if ( TRUE == NeedSignal ) {
        SetEvent( Port->Event );
    }
}   // InternalPostQueuedCompletionStatus


BOOL
WINAPI
CePostQueuedCompletionStatus(
    __in     HANDLE CompletionPort,
    __in     DWORD NumberOfBytesTransferred,
    __in     ULONG_PTR CompletionKey,
    __in_opt LPOVERLAPPED lpOverlapped
    )
{
    PIO_COMPLETION_STATUS CompletionStatus = NULL;
    PIO_COMPLETION_PORT Port = NULL;

    //
    // Check port handle
    //
    if ( NULL == CompletionPort ) {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    //
    // Get port context
    //
    Port = (PIO_COMPLETION_PORT) GetEventData( CompletionPort );
    if ( NULL == Port ) {
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }

    DEBUGMSG(DEBUG_IOCP_API, (TEXT("ICOP: [API] PostQueuedCompletionStatus Port[0x%08x] Overlapped[0x%08x]\r\n"), Port, lpOverlapped));

    //
    // Allocate IO_COMPLETION_STATUS for this request
    //
    CompletionStatus = AllocIoCompletionStatus();
    if ( NULL == CompletionStatus ) {
        SetLastError( ERROR_OUTOFMEMORY );
        return FALSE;
    }

    //
    // Remember completion status
    //
    CompletionStatus->fPostedByPostQueue = TRUE;
    CompletionStatus->CompletionKey = CompletionKey;
    CompletionStatus->lpOverlapped = lpOverlapped;
    CompletionStatus->dwNumberOfBytesTransferred = NumberOfBytesTransferred;

    InternalPostQueuedCompletionStatus( Port, CompletionStatus );

    return TRUE;

}   // PostQueuedCompletionStatus

#define IO_TYPE_SEND TRUE
#define IO_TYPE_RECV FALSE

//
// Find and ref associated IO_COMPLETION_SOCKET and allocate an IO_COMPLETION_STATUS for a new I/O request
// Return TRUE for success and FALSE for failure
//
BOOL
PrepareAsyncIo(
    SOCKET Socket,
    BOOL   IsSend,
    LPOVERLAPPED lpOverlapped,
    PIO_COMPLETION_SOCKET * ppIoSocket,
    PIO_COMPLETION_STATUS * ppIoStatus
    )
{
    PIO_COMPLETION_SOCKET IoSocket = NULL;
    PIO_COMPLETION_STATUS IoStatus = NULL;

    *ppIoSocket = NULL;
    *ppIoStatus = NULL;

    //
    // Track the OVERLAPPED operation with an IO_COMPLETION_STATUS.
    //
    if ( NULL != lpOverlapped ) {
        IoSocket = FindAndRefIoSocket( Socket );
        if ( NULL != IoSocket ) {
            *ppIoSocket = IoSocket;

            IoStatus = AllocIoCompletionStatus();
            if ( NULL != IoStatus ) {
                IoStatus->fPostedByPostQueue = FALSE;
                IoStatus->lpOverlapped = lpOverlapped;
                IoStatus->CompletionKey = IoSocket->CompletionKey;
                EnterCriticalSection( &IoSocket->Port->Lock );
                //
                // Assign an appropriate event handle to the overlapped struct and
                // queue the I/O request
                //
                if ( TRUE == IsSend ) {
                    lpOverlapped->hEvent = IoSocket->SendEvent;
                    InsertTailList( &IoSocket->PendingSendList, &IoStatus->List );
                } else {
                    lpOverlapped->hEvent = IoSocket->RecvEvent;
                    InsertTailList( &IoSocket->PendingRecvList, &IoStatus->List );
                }

                // Resetting lpOverlapped->Internal, so that it can later be checked to see
                // if the IO operation has been completed or not
                lpOverlapped->Internal = 0;
                LeaveCriticalSection( &IoSocket->Port->Lock );

                *ppIoStatus = IoStatus;
                DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP: PrepareAysncIo IoSocket[0x%08x] IoStatus[0x%08x]\r\n"), IoSocket, IoStatus));

            } else {
                //
                // Failed to allocate a new IO_COMPLETION_STATUS
                //
                DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP: PrepareAysncIo Failure to Allocate IO_COMPLETION_STATUS\r\n")));
                *ppIoSocket = NULL;
                DerefIoSocket( IoSocket );
                return FALSE;
            }
        }
    }

    //
    // lpOverlapped==NULL indicates a synchronous I/O on a socket.
    //
    return TRUE;        // Just call Winsock normally
}   // PrepareAsyncIo


//
// CE's Winsock does not signal an overlapped complete when it completes synchronously
// so we have to catch this condition, fix up the overlapped fields so it looks like
// it completed and then post the completion.
//
void
PostCompleteAsyncIo(
    PIO_COMPLETION_SOCKET IoSocket,
    PIO_COMPLETION_STATUS IoStatus,
    DWORD BytesTransferred,
    DWORD WSFlags,
    DWORD Status
    )
{
    EnterCriticalSection( &IoSocket->Port->Lock );
    RemoveEntryList( &IoStatus->List );
    LeaveCriticalSection( &IoSocket->Port->Lock );

    //
    // Fix up the overlapped fields for non-pending I/O's
    //
    IoStatus->lpOverlapped->InternalHigh =  BytesTransferred;
    IoStatus->lpOverlapped->Offset =        WSFlags;
    IoStatus->lpOverlapped->OffsetHigh =    Status; // error code
    IoStatus->lpOverlapped->Internal   =    0; // I/O state

    //
    // Signal the IoCompletionPort if no IOCOMPLETION routine available
    //
    InternalPostQueuedCompletionStatus( IoSocket->Port, IoStatus );

    DerefIoSocket( IoSocket );
}   // PostCompleteAsyncIo


//
// IoCompletionPort enabled wrapper around system WSARecv
//
int
WSAAPI
CeIocWSARecv(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesRecvd,
    IN OUT LPDWORD  lpFlags,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
    PIO_COMPLETION_SOCKET IoSocket = NULL;
    PIO_COMPLETION_STATUS IoStatus = NULL;
    int Ret = ERROR_SUCCESS;
    int Err = ERROR_SUCCESS;

    if ( FALSE == PrepareAsyncIo( Socket, IO_TYPE_RECV, lpOverlapped, &IoSocket, &IoStatus ) ) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DEBUGMSG(DEBUG_IOCP_API, (TEXT("ICOP: [API] IocWSARecv Socket[0x%08x] lpOverlapped[0x%08x]\r\n"), Socket, lpOverlapped));

    //
    // Sockets not associated with an IO completion port call Winsock normally
    //
    Ret = WSARecv(
                Socket,
                lpBuffers,
                dwBufferCount,
                lpNumberOfBytesRecvd,
                lpFlags,
                lpOverlapped,
                lpCompletionRoutine
                );
    if ( ERROR_SUCCESS != Ret) {
        Err = GetLastError();
    } else {
        Err = 0;
    }

    if ( ( ERROR_IO_PENDING != Err ) && ( NULL != IoSocket ) ) {
        //
        // It completed right away.
        //
        PostCompleteAsyncIo( IoSocket, IoStatus, *lpNumberOfBytesRecvd, *lpFlags, Err );
    }

    return Ret;
}

//
// IoCompletionPort enabled wrapper around system WSARecvFrom
//
int
WSAAPI
CeIocWSARecvFrom(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesRecvd,
    IN OUT LPDWORD  lpFlags,
    OUT struct sockaddr FAR * lpFrom,
    IN OUT LPINT    lpFromlen,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
    PIO_COMPLETION_SOCKET IoSocket = NULL;
    PIO_COMPLETION_STATUS IoStatus = NULL;
    int Ret = ERROR_SUCCESS;
    int Err = ERROR_SUCCESS;

    if ( FALSE == PrepareAsyncIo( Socket, IO_TYPE_RECV, lpOverlapped, &IoSocket, &IoStatus ) ) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DEBUGMSG(DEBUG_IOCP_API, (TEXT("ICOP: [API] IocWSARecvFrom Socket[0x%08x] lpOverlapped[0x%08x]\r\n"), Socket, lpOverlapped));

    //
    // Sockets not associated with an IO completion port call Winsock normally
    //
    Ret = WSARecvFrom(
                Socket,
                lpBuffers,
                dwBufferCount,
                lpNumberOfBytesRecvd,
                lpFlags,
                lpFrom,
                lpFromlen,
                lpOverlapped,
                lpCompletionRoutine
                );
    if ( ERROR_SUCCESS != Ret ) {
        Err = GetLastError();
    } else {
        Err = 0;
    }

    if ( ( ERROR_IO_PENDING != Err ) && ( NULL != IoSocket ) ) {
        //
        // It completed right away.
        //
        PostCompleteAsyncIo( IoSocket, IoStatus, *lpNumberOfBytesRecvd, *lpFlags, Err );
    }

    return Ret;
}


//
// IoCompletionPort enabled wrapper around system WSASend
//
int
WSAAPI
CeIocWSASend(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesSent,
    IN DWORD        dwFlags,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
    PIO_COMPLETION_SOCKET IoSocket = NULL;
    PIO_COMPLETION_STATUS IoStatus = NULL;
    int Ret = ERROR_SUCCESS;
    int Err = ERROR_SUCCESS;

    if ( FALSE == PrepareAsyncIo( Socket, IO_TYPE_SEND, lpOverlapped, &IoSocket, &IoStatus ) ) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DEBUGMSG(DEBUG_IOCP_API, (TEXT("ICOP: [API] IocWSASend Socket[0x%08x] lpOverlapped[0x%08x]\r\n"), Socket, lpOverlapped));

    //
    // Sockets not associated with an IO completion port call Winsock normally
    //
    Ret = WSASend(
                Socket,
                lpBuffers,
                dwBufferCount,
                lpNumberOfBytesSent,
                dwFlags,
                lpOverlapped,
                lpCompletionRoutine
                );
    if ( ERROR_SUCCESS != Ret ) {
        Err = GetLastError();
    } else {
        Err = 0;
    }

    if ( ( ERROR_IO_PENDING != Err ) && ( NULL != IoSocket ) ) {
        //
        // It completed right away.
        //
        PostCompleteAsyncIo( IoSocket, IoStatus, *lpNumberOfBytesSent, dwFlags, Err );
    }

    return Ret;
}


//
// IoCompletionPort enabled wrapper around system WSASendTo
//
int
WSAAPI
CeIocWSASendTo(
    IN SOCKET       Socket,
    IN OUT LPWSABUF lpBuffers,
    IN DWORD        dwBufferCount,
    OUT LPDWORD     lpNumberOfBytesSent,
    IN DWORD        dwFlags,
    IN const struct sockaddr FAR * lpTo,
    IN int iTolen,
    IN LPWSAOVERLAPPED lpOverlapped,
    IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
    PIO_COMPLETION_SOCKET IoSocket = NULL;
    PIO_COMPLETION_STATUS IoStatus = NULL;
    int Ret = ERROR_SUCCESS;
    int Err = ERROR_SUCCESS;

    if ( FALSE == PrepareAsyncIo( Socket, IO_TYPE_SEND, lpOverlapped, &IoSocket, &IoStatus ) ) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DEBUGMSG(DEBUG_IOCP_API, (TEXT("ICOP: [API] IocWSASendTo Socket[0x%08x] lpOverlapped[0x%08x]\r\n"), Socket, lpOverlapped));

    //
    // Sockets not associated with an IO completion port call Winsock normally
    //
    Ret = WSASendTo(
                Socket,
                lpBuffers,
                dwBufferCount,
                lpNumberOfBytesSent,
                dwFlags,
                lpTo,
                iTolen,
                lpOverlapped,
                lpCompletionRoutine
                );
    if ( ERROR_SUCCESS != Ret ) {
        Err = GetLastError();
    } else {
        Err = 0;
    }

    if ( ( ERROR_IO_PENDING != Err ) && ( NULL != IoSocket ) ) {
        //
        // It completed right away.
        //
        PostCompleteAsyncIo( IoSocket, IoStatus, *lpNumberOfBytesSent, dwFlags, Err );
    }

    return Ret;
}


//
// IoCompletionPort enabled wrapper around system  *$
//
int
WSAAPI
CeIocCloseSocket(
    IN SOCKET Socket
    )
{
    // perform the following operations only if initialized
    if( TRUE == IoCompletionPortSystemInitialized.Initialized() ) {
        PIO_COMPLETION_SOCKET IoSocket = NULL;
        IoSocket = FindAndRefIoSocket( Socket );
        if ( NULL != IoSocket ) {
            DEBUGMSG(DEBUG_IOCP_API, (TEXT("ICOP: [API] IocCloseSocket Socket[0x%08x] IoSocket[0x%08x]\r\n"), Socket, IoSocket));
            DisassociateIoSocket( IoSocket );
            DerefIoSocket( IoSocket );
        }
    }

    //
    // Sockets not associated with an IO completion port call Winsock normally
    //
    return  closesocket( Socket );
}


#define SOCKET_LIST_CHANGED_EVENT WAIT_OBJECT_0
#define WAIT_FOR_ALL TRUE
#define WAIT_FOR_ONE FALSE




void
CollectCompletedIOs(
    PIO_COMPLETION_SOCKET IoSocket,
    PLIST_ENTRY pCompletedList,
    HANDLE hTriggeredEvt)
{
    BOOL result = TRUE;
    DWORD wsaResult = NO_ERROR;
    PIO_COMPLETION_STATUS IoStatus = NULL;
    BOOL fAddToList = FALSE;
    DWORD bytesTransf = 0;
    DWORD flags = 0;
    PLIST_ENTRY pPendingList = NULL;

    if ( hTriggeredEvt == IoSocket->RecvEvent ) {
        pPendingList = &IoSocket->PendingRecvList;
    } else {
        pPendingList = &IoSocket->PendingSendList;
    }

    DEBUGMSG(DEBUG_IOCP, (TEXT("ICOP: CollectCompletedIOs IoSocket[0x%08x]\r\n"), IoSocket));

    if ( FALSE == IsListEmpty( pPendingList ) ) {
        PLIST_ENTRY Entry = pPendingList->Flink;
        while ( Entry != pPendingList ) {
            IoStatus = (PIO_COMPLETION_STATUS) CONTAINING_RECORD( Entry, IO_COMPLETION_STATUS, List );

            // if IO operation status is not progress, it is completed
            if ( NULL != IoStatus->lpOverlapped )
            {
                result = WSAGetOverlappedResult( IoSocket->Socket, IoStatus->lpOverlapped, &bytesTransf, FALSE, &flags );
                wsaResult = WSAGetLastError();
                if( result || ( !result && ERROR_IO_PENDING != wsaResult ) ) {
                    Entry = RemoveHeadList( pPendingList );
                    InsertTailList( pCompletedList, Entry );
                    Entry = pPendingList->Flink;
                } else {
                    // Rest of the IOs should be pending, so quit.
                    break;
                }
            } else {
                // Rest of the IOs should be pending, so quit.
                break;
            }
        }
    }
}


//
// Thread to wait on socket overlapped I/O so the I/O completions can be posted to the associated
// IoCompletionPort
//

DWORD
IoSocketCompletionThread(
    LPVOID Nothing
    )
{
    HANDLE EventArray[MAXIMUM_WAIT_OBJECTS];
    PIO_COMPLETION_SOCKET IoSocketArray[MAXIMUM_WAIT_OBJECTS];
    PIO_COMPLETION_SOCKET IoSocket = NULL;
    PIO_COMPLETION_STATUS IoStatus = NULL;
    PLIST_ENTRY Entry = NULL;
    LIST_ENTRY completedIOs = { 0 };


    DWORD i = 0;
    DWORD Ret = ERROR_SUCCESS;
    DWORD NumEvents = 1;

    EventArray[0] = IoSocketListChangedEvent;
    IoSocketArray[0] = NULL;    // Unused to maintain one-for-one mapping
    Ret = SOCKET_LIST_CHANGED_EVENT;

    InitializeListHead( &completedIOs );

    while ( TRUE ) {

        if ( SOCKET_LIST_CHANGED_EVENT == Ret ) {
            //
            // IoSocket list changed. Deref the current list of IoSockets and rebuild list
            //
            for ( i = 1; i < NumEvents; i += 2 ) {
                DerefIoSocket( IoSocketArray[i] );
            }

            NumEvents = 1;

            EnterCriticalSection( &IoCompletionPortLock );
            if ( TRUE == IsListEmpty( &IoCompletionPortList ) ) {
                //
                // There are no more IoCompletionPorts so there is no reason for this thread to keep running.
                //
                IoSocketCompletionThreadRunning = FALSE;
                LeaveCriticalSection( &IoCompletionPortLock );
                DEBUGMSG(DEBUG_IOCP, (L"IOCP:IoSocketCompletionThread exiting\n"));
                break;
            }

            //
            // Build array of events for WaitForMultipleObjects and a parallel array to map
            // events to IoSockets
            //
            Entry = IoSocketList.Flink;
            while ( ( Entry != &IoSocketList ) && ( NumEvents < MAXIMUM_WAIT_OBJECTS-1 ) ) {
               IoSocket = CONTAINING_RECORD( Entry, IO_COMPLETION_SOCKET, GlobalList );
               Entry = Entry->Flink;
               RefIoSocket( IoSocket );
               EventArray[NumEvents] =   IoSocket->RecvEvent;
               EventArray[NumEvents+1] = IoSocket->SendEvent;
               IoSocketArray[NumEvents] = IoSocket;
               IoSocketArray[NumEvents+1] = IoSocket;
               NumEvents += 2;
            }
            LeaveCriticalSection( &IoCompletionPortLock );

        } else if ( Ret < NumEvents ) {
            //
            // One of the IoSockets had an I/O complete
            //
            IoSocket = IoSocketArray[Ret];
            EnterCriticalSection( &IoSocket->Port->Lock );
            Entry = NULL;

            CollectCompletedIOs( IoSocket, &completedIOs, EventArray[Ret] );
            LeaveCriticalSection( &IoSocket->Port->Lock );

            Entry = completedIOs.Flink;

            while ( ( NULL != Entry ) && ( Entry != &completedIOs ) ) {

                RemoveHeadList( &completedIOs );
                IoStatus = (PIO_COMPLETION_STATUS) CONTAINING_RECORD( Entry, IO_COMPLETION_STATUS, List );

                //
                // Post the completion to the IoCompletionPort
                //
                InternalPostQueuedCompletionStatus( IoSocket->Port, IoStatus );

                DerefIoSocket( IoSocket );
                Entry = completedIOs.Flink;
            }
        } else {
            ASSERT( FALSE );
        }

        Ret = WaitForMultipleObjects( NumEvents, EventArray, WAIT_FOR_ONE, INFINITE );
    }

    return 0;
}   // IoSocketCompletionThread


#ifdef __cplusplus
}
#endif
