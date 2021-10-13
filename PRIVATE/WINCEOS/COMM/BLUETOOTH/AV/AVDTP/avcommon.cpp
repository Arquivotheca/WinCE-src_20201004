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
// Implementation of AVDTP common components

#include <windows.h>
#include <intsafe.h>

#include <svsutil.hxx>

#include "avdtpriv.h"

#define BTHAVDTP_MIN_FRAME          48  // TODO: Is this ok?  Should we use L2CAP min frame?

#define AVDTP_COMMAND_TIMEOUT_MAX   300
#define AVDTP_COMMAND_TIMEOUT       5   
#define AVDTP_IDLE_TIMEOUT_MAX      300
#define AVDTP_IDLE_TIMEOUT          5

#define AVDTP_DISCOVER_SESSION_TIMEOUT  60

#define AVDTP_PAGE_TIMEOUT          0x2700

#define AVDTP_DEFAULT_SNIFF_DELAY   10000   // 10 seconds
#define AVDTP_DEFAULT_SNIFF_MAX     0x800   // 1.28 seconds
#define AVDTP_DEFAULT_SNIFF_MIN     0x640
#define AVDTP_DEFAULT_SNIFF_ATTEMPT 0x0C
#define AVDTP_DEFAULT_SNIFF_TIMEOUT 0x08

#define AVDTP_DEFAULT_THREAD_PRIORITY   132

#define AVDTP_SCALE                 10

using namespace AVDT_NS;

///////////////////////


namespace AVDT_NS 
{

enum AVDTPStateEnum {
    AVDTP_STATE_CLOSED,
    AVDTP_STATE_INITIALIZING,
    AVDTP_STATE_DOWN,
    AVDTP_STATE_UP,
    AVDTP_STATE_CLOSING   
};

class AVDTPState {
public:    
    AVDTPState(void)
    {
        Reset();
    }

    //
    // The following methods transition AVDTP from one state
    // to another.
    //

    inline void Initializing(void) { m_state = AVDTP_STATE_INITIALIZING; }
    inline void Initialized(void) { m_state = AVDTP_STATE_DOWN; }
    inline void StackUp(void) { m_state = AVDTP_STATE_UP; }
    inline void StackDown(void) { m_state = AVDTP_STATE_DOWN; }
    inline void Closing(void) { m_state = AVDTP_STATE_CLOSING; }
    inline void Closed(void) { m_state = AVDTP_STATE_CLOSED; }
    inline void Reset(void) { m_state = AVDTP_STATE_CLOSED; }

    //
    // The following methods check the state of AVDTP.  If the
    // layer is 'Running' it is initialized and connected to
    // its underlying layer.  If the layer is 'Connected' then
    // the stack is currently up indicating hardware is currently
    // attached and on.
    //
    
    inline BOOL IsRunning(void) 
    {  
        if ((m_state == AVDTP_STATE_UP) || (m_state == AVDTP_STATE_DOWN)) {
            return TRUE;
        }
        
        return FALSE;
    }

    inline BOOL IsConnected(void)
    {
        if (m_state == AVDTP_STATE_UP) {
            return TRUE;
        }

        return FALSE;
    }

private:
    AVDTPStateEnum m_state;
};

//
// The AVDTP_CONTEXT struct describes an upper layer that is sitting
// on top of AVDTP.  An instance of this struct is created when the
// upper layer calls EstablishDeviceContext to register itself as an
// AVDTP extension.
//
struct AVDTP_CONTEXT : public SVSRefObj {
    AVDTP_EVENT_INDICATION  ei;         // Upper layer events
    AVDTP_CALLBACKS         c;          // Upper layer callbacks

    HANDLE                  hContext;   // Unique handle to an particular owner context

    void*                   pUserContext;

    PAVDTP_ENDPOINT_INFO    pEPInfo;    // Pointer to an array of SEID info for this context
    DWORD                   cEPInfo;    // Size of the pEPInfo array

    AVDTP_CONTEXT(void)
    {
        memset(&ei, 0, sizeof(ei));
        memset(&c, 0, sizeof(c));

        pUserContext = NULL;
        pEPInfo = NULL;
        cEPInfo = 0;
        hContext = NULL;
    }

    ~AVDTP_CONTEXT(void)
    {
        if (pEPInfo) {
            delete[] pEPInfo;
        }
    }

    void *operator new (size_t iSize);
    void operator delete (void *ptr);    
};

// Task->bSignalId uses the following values for L2CAP
// related calls and uses signal id for AVDTP related
// calls.
#define AVDTP_CALL_LINK_SETUP           0xfe
#define AVDTP_CALL_LINK_CONFIG          0xff

//
// The task class is used to keep track of state specific to an AVDTP call.  For
// example, when AVDTP calls into the lower layers it sends down the hCallContext
// member so it can look up the context for a call when the asynchronous response
// comes back from the lower layers.
//
struct Task {
    HANDLE              hOwner;         // Handle to owner context
    void*               pContext;       // Call context
    HANDLE              hCallContext;   // Unique handle for async call lookup

    HANDLE              hSession;       // Handle to the session or stream

    DWORD               TimeoutCookie;  // Handle to timeout commands

    BYTE                bSignalId;      // Type of call
    BYTE                bTransaction;   // Transaction number

    // The following is used to remember data for pending calls
    BYTE                bAcpSEID;

    Task(HANDLE owner, HANDLE session, void* pCallerContext, BYTE signalId)
    {
        hOwner = owner;
        hSession = session;
        pContext = pCallerContext;
        bSignalId = signalId;

        TimeoutCookie = 0;
        bTransaction = 0;
        bAcpSEID = 0;
    }

    void *operator new (size_t iSize);
    void operator delete (void *ptr);    
};

#define InitialAllocation 20

typedef ce::list<AVDTP_CONTEXT*, ce::fixed_block_allocator<InitialAllocation> >  ContextList;
typedef ce::list<Task*, ce::fixed_block_allocator<InitialAllocation> >           TaskList;
typedef ce::list<Stream*, ce::fixed_block_allocator<InitialAllocation> >         StreamList;
typedef ce::list<Session*, ce::fixed_block_allocator<InitialAllocation> >        SessionList;

//
// This is the global AVDTP context.  Only one instance of this class exists.  It keeps track
// of all state, resources, etc in AVDTP.
//
class AVDTP : public SVSSynch, public SVSRefObj {
public:
    AVDTPState          state;

    AVDTSignal          signal;
    AVDTMedia           media;

    ContextList         listContexts;       // List of context structures
    TaskList            listTasks;          // List of calls
    SessionList         listSessions;       // List of physical sessions
    StreamList          listStreams;        // List of AVDTP streams

    FixedMemDescr*      pfmdContexts;       // FMD for contexts
    FixedMemDescr*      pfmdTasks;          // FMD for tasks
    FixedMemDescr*      pfmdSessions;       // FMD for sessions
    FixedMemDescr*      pfmdStreams;        // FMD for streams

    HANDLE              hL2CAP;             // Handle to L2CAP context
    L2CAP_INTERFACE     l2cap_if;           // L2CAP interface

    int                 cDataHeaders;   
    int                 cDataTrailers;

    DWORD               cCmdTimeoutSec;     // Timeout between sendind command & getting response (Tgavdp100 in spec)
    DWORD               cIdleTimeoutSec;    // Time before idle connection is disconnected

    // Media flow control configurable params
    DWORD               dwHciBuffThreshold;
    DWORD               dwHciBuffThresholdSleep;

    USHORT              usPageTimeout;      // HCI Page timeout used in AVDTP
    USHORT              usInMTU;            // L2CAP In MTU
    
    DWORD               bUniqueSEID;

    // Sniff mode parameters
    USHORT              usSniffDelay;
    USHORT              usSniffMax;
    USHORT              usSniffMin;
    USHORT              usSniffAttempt;
    USHORT              usSniffTimeout;

    // For debugging
    DWORD               dwHciThreadId;
   
    void Reinit(void)
    {
        state.Reset();
        
        hL2CAP = NULL;
        
        memset(&l2cap_if, 0, sizeof(l2cap_if));

        cDataHeaders = 0;
        cDataTrailers = 0;

        cCmdTimeoutSec = AVDTP_COMMAND_TIMEOUT;
        cIdleTimeoutSec = AVDTP_IDLE_TIMEOUT;

        bUniqueSEID = 0x01;

        usPageTimeout = AVDTP_PAGE_TIMEOUT;
        usInMTU = AVDTP_L2CAP_MTU_IN;

        dwHciBuffThreshold = AVDTP_HCI_BUFF_THRESHOLD;
        dwHciBuffThresholdSleep = AVDTP_HCI_BUFF_THRESHOLD_SLEEP;

        usSniffDelay = AVDTP_DEFAULT_SNIFF_DELAY;
        usSniffMax = AVDTP_DEFAULT_SNIFF_MAX;
        usSniffMin = AVDTP_DEFAULT_SNIFF_MIN;
        usSniffAttempt = AVDTP_DEFAULT_SNIFF_ATTEMPT;
        usSniffTimeout = AVDTP_DEFAULT_SNIFF_TIMEOUT;

        dwHciThreadId = 0;
    }

    AVDTP(void)
    {
        Reinit();
    }
};

}; // End namespace

static AVDTP* gpAVDTP;


//
// Helper functions
// 

void avdtp_lock(void)
{
    gpAVDTP->Lock();
}

BOOL avdtp_lock_if_connected(void) 
{
    if (! gpAVDTP) {
        return FALSE;
    }

    avdtp_lock();    

    if (! gpAVDTP->state.IsConnected()) {
        avdtp_unlock();
        return FALSE;
    }

    return TRUE;
}

BOOL avdtp_lock_if_running(void) 
{
    if (! gpAVDTP) {
        return FALSE;
    }

    avdtp_lock();    

    if (! gpAVDTP->state.IsRunning()) {
        avdtp_unlock();
        return FALSE;
    }

    return TRUE;    
}
    

void avdtp_unlock(void)
{
    gpAVDTP->Unlock();
}

BOOL avdtp_IsLocked(void)
{
    return gpAVDTP->IsLocked();
}

void avdtp_addref(void)
{
    gpAVDTP->AddRef();
}

void avdtp_delref(void)
{
    gpAVDTP->DelRef();
}

/*------------------------------------------------------------------------------
    DispatchStackEvent
    
    This function notifies all owner contexts of the specified event.
    
    Parameters:
        iEvent: Event to be signaled to all owners
------------------------------------------------------------------------------*/
void DispatchStackEvent(int iEvent)
{
    for (ContextList::iterator it = gpAVDTP->listContexts.begin(), itEnd = gpAVDTP->listContexts.end(); it != itEnd; ++it) {
        AVDTP_CONTEXT* pContext = *it;
        BT_LAYER_STACK_EVENT_IND pCallback = pContext->ei.avdtp_StackEvent;
        if (pCallback) {
            void *pUserContext = pContext->pUserContext;

            IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"Going into StackEvent notification\n"));

            pContext->AddRef();
            avdtp_unlock();

            __try {
                pCallback(pUserContext, iEvent, NULL);
            } __except (1) {
                IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] DispatchStackEvent: exception in avdtp_StackEvent!\n"));
            }

            avdtp_lock();
            pContext->DelRef();
            
            IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"Came back StackEvent notification\n"));
        }        
    }
}

/*------------------------------------------------------------------------------
    GetConnectionState
    
    This function gets the current connection state from L2CAP.
    
    Parameters: None         
------------------------------------------------------------------------------*/
void GetConnectionState(void) 
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    gpAVDTP->state.StackDown();  // By default set to disconnected    
    
    __try {
        int fConnected = FALSE;
        int dwRet = 0;
        gpAVDTP->l2cap_if.l2ca_ioctl(gpAVDTP->hL2CAP, BTH_STACK_IOCTL_GET_CONNECTED, 0, NULL, sizeof(fConnected), (char *)&fConnected, &dwRet);
        if ((dwRet == sizeof(fConnected)) && fConnected) {
            gpAVDTP->state.StackUp();
        }
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] GetConnectionState : exception in l2ca_ioctl BTH_STACK_IOCTL_GET_CONNECTED\n"));
    }
}


/*------------------------------------------------------------------------------
    WaitForRefToOneAndLock
    
    This function waits for the AVDTP ref count to drop to 1 and then takes the
    lock.
    
    Parameters: None         
    
    Returns (BOOL): TRUE indicates we successfully aquired the lock
------------------------------------------------------------------------------*/
BOOL WaitForRefToOneAndLock(void)
{
    //
    // Wait for ref count to drop to 1
    //
    
    while (1) {
        if (! avdtp_lock_if_running()) {
            IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] WaitForRefToOne : ERROR_SERVICE_DOES_NOT_EXIST\n"));
            return FALSE;
        }
        
        if (gpAVDTP->GetRefCount() == 1) {
            break;
        }

        IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"WaitForRefToOne: Waiting for ref count to drop to 1\n"));
        
        avdtp_unlock();        
        Sleep(100);
    }

    return TRUE;
}

/*------------------------------------------------------------------------------
    VerifyOwner
    
    Look up the owner context based on its handle.
    
    Parameters:
        hContext:   Unique handle to an owner context
    
    Returns (AVDT_NS.AVDTP_CONTEXT): Pointer to the owner context struct
------------------------------------------------------------------------------*/
AVDTP_CONTEXT* VerifyOwner(HANDLE hContext)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    AVDTP_CONTEXT* pOwner = (AVDTP_CONTEXT*) btutil_TranslateHandle((SVSHandle)hContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"VerifyOwner : ERROR_NOT_FOUND\n"));
        return NULL;
    }

#ifdef DEBUG
    //
    // Ensure the an owner is still in the list that has this context
    //
    BOOL fFound = FALSE;
    for (ContextList::iterator it = gpAVDTP->listContexts.begin(), itEnd = gpAVDTP->listContexts.end(); it != itEnd; ++it) {
        if ((*it)->hContext == hContext) {
            fFound = TRUE;
            break;
        }
    }
    SVSUTIL_ASSERT(fFound);
#endif // DEBUG

    return pOwner;
}

/*------------------------------------------------------------------------------
    VerifySession
    
    Look up the session based on its handle.
    
    Parameters:
        hSession:   Unique handle to a session
    
    Returns (AVDT_NS.Session): Pointer to the session struct
------------------------------------------------------------------------------*/
Session* VerifySession(HANDLE hSession)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Session* pSession = (Session*) btutil_TranslateHandle((SVSHandle)hSession);
    if (! pSession) {
        IFDBG(DebugOut(DEBUG_ERROR, L"VerifySession : ERROR_NOT_FOUND\n"));
        return NULL;
    }

#ifdef DEBUG
    //
    // Ensure the a session is still in the list that has this context
    //
    BOOL fFound = FALSE;
    for (SessionList::iterator it = gpAVDTP->listSessions.begin(), itEnd = gpAVDTP->listSessions.end(); it != itEnd; ++it) {
        if ((*it)->hContext == hSession) {
            fFound = TRUE;
            break;
        }
    }
    SVSUTIL_ASSERT(fFound);
#endif // DEBUG
    
    return pSession;
}

/*------------------------------------------------------------------------------
    VerifyStream
    
    Look up a stream based on its handle
    
    Parameters:
        hStream:    Unique handle to a stream 
    
    Returns (AVDT_NS.Stream): Pointer to a stream struct
------------------------------------------------------------------------------*/
Stream* VerifyStream(HANDLE hStream)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Stream* pStream = (Stream*) btutil_TranslateHandle((SVSHandle)hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"VerifyStream : ERROR_NOT_FOUND\n"));
        return NULL;
    }

#ifdef DEBUG
    //
    // Ensure the a stream is still in the list that has this context
    //
    BOOL fFound = FALSE;
    for (StreamList::iterator it = gpAVDTP->listStreams.begin(), itEnd = gpAVDTP->listStreams.end(); it != itEnd; ++it) {
        if ((*it)->hContext == hStream) {
            fFound = TRUE;
            break;
        }
    }
    SVSUTIL_ASSERT(fFound);

    //
    // Verify the sessions on this stream
    //
    if (pStream->hSessSignal) {
        SVSUTIL_ASSERT(VerifySession(pStream->hSessSignal));
    }
    if (pStream->hSessMedia) {
        SVSUTIL_ASSERT(VerifySession(pStream->hSessMedia));
    }    
#endif // DEBUG

    return pStream;    
}

/*------------------------------------------------------------------------------
    VerifyCall
    
    Look up a call based on its handle
    
    Parameters:
        hCall:  Unique handle to a Task (aka Call)
    
    Returns (AVDT_NS.Task): Pointer to a Task struct
------------------------------------------------------------------------------*/
Task* VerifyCall(HANDLE hCall)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Task *pCall = (Task*) btutil_TranslateHandle((SVSHandle)hCall);
    if (! pCall) {
        IFDBG(DebugOut(DEBUG_ERROR, L"VerifyCall : ERROR_NOT_FOUND\n"));
        return NULL;
    }

#ifdef DEBUG
    //
    // Ensure a call is still in the list that has this call context
    //
    BOOL fFound = FALSE;
    for (TaskList::iterator it = gpAVDTP->listTasks.begin(), itEnd = gpAVDTP->listTasks.end(); it != itEnd; ++it) {
        if ((*it)->hCallContext == hCall) {
            fFound = TRUE;
            break;
        }
    }
    SVSUTIL_ASSERT(fFound);
#endif // DEBUG
    
    return pCall;
}

/*------------------------------------------------------------------------------
    FindOwner
    
    Find an owner context based on SEID
    
    Parameters:
        bSEID:  ACP SEID 
    
    Returns (AVDT_NS.AVDTP_CONTEXT): Pointer to an owner context struct
------------------------------------------------------------------------------*/
AVDTP_CONTEXT* FindOwner(BYTE bSEID)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    AVDTP_CONTEXT* pOwner = NULL;

    for (ContextList::iterator it = gpAVDTP->listContexts.begin(), itEnd = gpAVDTP->listContexts.end(); it != itEnd; ++it) {
        for (int i = 0; i < (*it)->cEPInfo; i++) {
            if ((*it)->pEPInfo && ((*it)->pEPInfo[i].bSEID == bSEID)) {
                pOwner = *it;
                break;
            }
        }
    }

    return pOwner;
}

/*------------------------------------------------------------------------------
    FindSession
    
    Find a session based on address and channel type.
    
    Parameters:
        pba:            Address of the peer 
        bChannelType:   Signaling or Media channel
    
    Returns (AVDT_NS.Session): Pointer to a session struct
------------------------------------------------------------------------------*/
Session* FindSession(BD_ADDR* pba, BYTE bChannelType)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Session* pSession = NULL;

    for (SessionList::iterator it = gpAVDTP->listSessions.begin(), itEnd = gpAVDTP->listSessions.end(); it != itEnd; ++it) {
        if (((*it)->ba == *pba) && ((*it)->bChannelType == bChannelType)) {
            pSession = *it;
            break;
        }
    }

    return pSession;
}

/*------------------------------------------------------------------------------
    FindSession
    
    Find a session based on channel id (cid)
    
    Parameters:
        cid:    L2CAP channel id
    
    Returns (AVDT_NS.Session): Pointer to a session struct
------------------------------------------------------------------------------*/
Session* FindSession(USHORT cid)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Session* pSession = NULL;

    for (SessionList::iterator it = gpAVDTP->listSessions.begin(), itEnd = gpAVDTP->listSessions.end(); it != itEnd; ++it) {
        if ((*it)->cid == cid) {
            pSession = *it;
            break;
        }
    }

    return pSession;
}

/*------------------------------------------------------------------------------
    FindStream
    
    This function finds a stream based on the SEID and INT/ACP roles.
    
    Parameters:
        bSEID:      An SEID
        fRemote:    TRUE if we are looking for remote SEID
    
    Returns (AVDT_NS.Stream): Pointer to a stream struct
------------------------------------------------------------------------------*/
Stream* FindStream(BYTE bSEID, BOOL fRemote)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Stream* pStream = NULL;

    for (StreamList::iterator it = gpAVDTP->listStreams.begin(), itEnd = gpAVDTP->listStreams.end(); it != itEnd; ++it) {
        if ((fRemote && ((*it)->bRemoteSeid == bSEID)) ||
            (!fRemote && ((*it)->bLocalSeid == bSEID)))
        {
            pStream = *it;
            IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"FindStream: Found - fRemote:%d remote seid:%d local seid:%d\n", 
                fRemote, pStream->bRemoteSeid, pStream->bLocalSeid));        
            break;
        }        
    }

    return pStream;
}

/*------------------------------------------------------------------------------
    FindStream
    
    Find a stream based on the session handle.  Note that if the session handle
    is for a signaling session this could point to multiple streams.  In this
    case we just return the first one we find since the caller must only be
    interested in knowing if a stream exists and not finding a particular
    stream.
    
    Parameters:
        hSession: Unique handle to a session
    
    Returns (AVDT_NS.Stream): Pointer to stream struct
------------------------------------------------------------------------------*/
Stream* FindStream(HANDLE hSession)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Stream* pStream = NULL;

    for (StreamList::iterator it = gpAVDTP->listStreams.begin(), itEnd = gpAVDTP->listStreams.end(); it != itEnd; ++it) {
        if (((*it)->hSessMedia == hSession) || ((*it)->hSessSignal == hSession)) {
            // This will return the unique stream based on the media session or the first
            // signaling session in the list that matches.
            pStream = *it;
            break;
        }
    }

#ifdef DEBUG
    Session* pSession = VerifySession(hSession);
    if (! pSession) {
        SVSUTIL_ASSERT(0);
    }
#endif

    return pStream;
}

/*------------------------------------------------------------------------------
    FindOpenStream
    
    This function finds a stream in the open state with the specified session
    handle as the signaling session.    
    
    Parameters:
        hSignalSession: Unique handle to a signaling session
        hMediaSession:  Unique handle to a media session
    
    Returns (AVDT_NS.Stream): Pointer to a stream struct
------------------------------------------------------------------------------*/
Stream* FindOpenStream(HANDLE hSignalSession, HANDLE hMediaSession)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Stream* pStream = NULL;

    // Find the open stream with the specified signaling & media sessions
    for (StreamList::iterator it = gpAVDTP->listStreams.begin(), itEnd = gpAVDTP->listStreams.end(); it != itEnd; ++it) {
        if (((*it)->hSessSignal == hSignalSession) && ((*it)->state.IsInState(STREAM_STATE_OPEN)) && ((*it)->hSessMedia == hMediaSession)) {
            pStream = *it;
            break;
        }
    }

    // If we do not find the stream check if there is an open stream with no media session
    // specified (which means the stream is being opened by the peer)
    if (! pStream) {
        for (StreamList::iterator it = gpAVDTP->listStreams.begin(), itEnd = gpAVDTP->listStreams.end(); it != itEnd; ++it) {
            if (((*it)->hSessSignal == hSignalSession) && ((*it)->state.IsInState(STREAM_STATE_OPEN)) && ((*it)->hSessMedia == NULL)) {
                pStream = *it;
                break;
            }
        }
    }
    
    return pStream;
}

/*------------------------------------------------------------------------------
    FindCall
    
    This function finds a call based on the owner's pContext.
    
    Parameters:
        pCallContext: Pointer to the owner's call context
    
    Returns (AVDT_NS.Task): Pointer to a Task struct
------------------------------------------------------------------------------*/
Task* FindCall(LPVOID pCallContext)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Task* pCall = NULL;

    for (TaskList::iterator it = gpAVDTP->listTasks.begin(), itEnd = gpAVDTP->listTasks.end(); it != itEnd; ++it) {
        if ((*it)->pContext == pCallContext) {
            pCall = *it;
            break;
        }
    }

    return pCall;
}

/*------------------------------------------------------------------------------
    FindCall
    
    This function finds a call based on the session handle, transaction, and
    signal id.
    
    Parameters:
        hSession: Handle to a session or stream
        bTransaction: Signaling transaction of the call
        bSignalId: Signaling id of the call
    
    Returns (AVDT_NS.Task): Pointer to a Task struct
------------------------------------------------------------------------------*/
Task* FindCall(HANDLE hSession, BYTE bTransaction, BYTE bSignalId)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Task* pCall = NULL;
            
    for (TaskList::iterator it = gpAVDTP->listTasks.begin(), itEnd = gpAVDTP->listTasks.end(); it != itEnd; ++it) {
        if ((bSignalId != (*it)->bSignalId) || ((*it)->bTransaction != bTransaction)) {
            // Not a match, just continue
        }
        else if ((bSignalId == AVDTP_SIGNAL_DISCOVER) ||
            (bSignalId == AVDTP_CALL_LINK_SETUP) ||
            (bSignalId == AVDTP_CALL_LINK_CONFIG)) {
            // The call matches up, now just check it is the right session.
            if ((*it)->hSession == hSession) {
                pCall = *it;
                break;
            }
        } else {
            // The call matches up, now just check that the stream this call is referencing is
            // using the signaling session specified.
            Stream* pStream = VerifyStream((*it)->hSession);
            if (pStream && (pStream->hSessSignal == hSession)) {
                pCall = *it;
                break;
            }
        }
    }

#ifdef DEBUG
    //
    // If we found the call the session or stream associated with the call must be valid
    //
    if (pCall) {
        BOOL fFound = FALSE;
        for (SessionList::iterator it = gpAVDTP->listSessions.begin(), itEnd = gpAVDTP->listSessions.end(); it != itEnd; ++it) {
            if ((*it)->hContext == hSession) {
                fFound = TRUE;
                break;
            }
        }
        if (! fFound) {
            for (StreamList::iterator it = gpAVDTP->listStreams.begin(), itEnd = gpAVDTP->listStreams.end(); it != itEnd; ++it) {
                if ((*it)->hContext == hSession) {
                    fFound = TRUE;
                    break;
                }
            }
        }
        SVSUTIL_ASSERT(fFound);
    }
#endif // DEBUG

    return pCall;
}

/*------------------------------------------------------------------------------
    UnSniffIfNeeded
    
    UnSniff the specified session if needed.
    
    Parameters:
        pSession:   Session object to unsniff
------------------------------------------------------------------------------*/
void UnSniffIfNeeded(Session* pSession)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    BT_ADDR bta = SET_NAP_SAP(pSession->ba.NAP, pSession->ba.SAP);
    unsigned char mode = 0x00;

    // Unschedule the sniff thread
    if (pSession->SniffCookie) {
        btutil_UnScheduleEvent(pSession->SniffCookie);
        pSession->SniffCookie = 0;            
    }

    SVSUTIL_ASSERT(gpAVDTP->dwHciThreadId != GetCurrentThreadId());

    avdtp_addref();
    avdtp_unlock();

    // Make sure the link is not in SNIFF
    if (ERROR_SUCCESS == BthGetCurrentMode(&bta, &mode)) {
        if (mode == 0x02) { // sniff
            IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"UnSniffIfNeeded:: Exiting Sniff Mode\n"));
            if (ERROR_SUCCESS != BthExitSniffMode(&bta)) {
                IFDBG(DebugOut(DEBUG_WARN, L"[AVDTP] UnSniffIfNeeded:: Failed to exit Sniff Mode\n"));
            }
        }
    }

    avdtp_lock();
    avdtp_delref();
}

/*------------------------------------------------------------------------------
    SniffThread
    
    This thread is called to sniff the specified session.
    
    Parameters:
        lpArg:  Pointer to session handle
    
    Returns (DWORD): 
------------------------------------------------------------------------------*/
DWORD WINAPI SniffThread(LPVOID lpArg)
{
    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"SniffThread : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }
    
    Session* pSession = VerifySession((HANDLE)lpArg);    
    if (pSession) {
        BT_ADDR bta = SET_NAP_SAP(pSession->ba.NAP, pSession->ba.SAP);
        unsigned short usInterval = 0;

        IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"SniffThread:: Entering sniff for %04x%08x\n", pSession->ba.NAP, pSession->ba.SAP));

        USHORT usSniffMax = gpAVDTP->usSniffMax;
        USHORT usSniffMin = gpAVDTP->usSniffMin;
        USHORT usSniffAttempt = gpAVDTP->usSniffAttempt;
        USHORT usSniffTimeout = gpAVDTP->usSniffTimeout;

        avdtp_addref();
        avdtp_unlock();
        
        if (ERROR_SUCCESS != BthEnterSniffMode(&bta, usSniffMax, usSniffMin, usSniffAttempt, usSniffTimeout, &usInterval)) {
            IFDBG(DebugOut(DEBUG_WARN, L"[AVDTP] SniffThread:: Failed to enter sniff mode.\n"));
        }

        avdtp_lock();
        avdtp_delref();

        pSession->SniffCookie = 0;
    }

    avdtp_unlock();

    return 0;
}

/*------------------------------------------------------------------------------
    ScheduleSniff
    
    Schedule a thread to put the specified session in sniff
    
    Parameters:
        pSession:   Session to sniff
------------------------------------------------------------------------------*/
void ScheduleSniff(Session* pSession)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"ScheduleSniff:: Scheduling sniff mode for %04x%08x\n", pSession->ba.NAP, pSession->ba.SAP));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    if (gpAVDTP->usSniffDelay) {
        if (pSession->SniffCookie) {
            btutil_UnScheduleEvent(pSession->SniffCookie);
            pSession->SniffCookie = 0;            
        }
        
        pSession->SniffCookie = btutil_ScheduleEvent(SniffThread, pSession->hContext, gpAVDTP->usSniffDelay);
    }
}

/*------------------------------------------------------------------------------
    EnsureActiveModeThread
    
    Ensure the specified session stays in active mode (or not).
    
    Parameters:
        pSession:   Session object
    
    Returns (DWORD): Thread return value
------------------------------------------------------------------------------*/
DWORD WINAPI EnsureActiveModeThread(LPVOID lpArg)
{
    HANDLE hCurrentThreadId = (HANDLE) GetCurrentThreadId();
    int nPrevThreadPriority = CeGetThreadPriority(hCurrentThreadId);
    CeSetThreadPriority(hCurrentThreadId, AVDTP_DEFAULT_THREAD_PRIORITY);
    
    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"EnsureActiveModeThread : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        CeSetThreadPriority(hCurrentThreadId, nPrevThreadPriority);
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    Session* pSession = VerifySession((HANDLE)lpArg);
    if (! pSession) {
        IFDBG(DebugOut(DEBUG_ERROR, L"EnsureActiveModeThread : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        CeSetThreadPriority(hCurrentThreadId, nPrevThreadPriority);
        return ERROR_NOT_FOUND;
    }

    BT_ADDR bta = SET_NAP_SAP(pSession->ba.NAP, pSession->ba.SAP);

    pSession->ActiveModeCookie = 0;

    UnSniffIfNeeded(pSession);

    unsigned short policy = pSession->EnsureActiveCount ? 0x0001 : 0x000f;

    avdtp_addref();
    avdtp_unlock();
    
    // Change the link policy settings accordingly
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"EnsureActiveModeThread:: Changing the link policy to %d for address %04x%08x\n", policy, GET_NAP(bta), GET_SAP(bta)));        
    if (ERROR_SUCCESS != BthWriteLinkPolicySettings(&bta, policy)) {
        IFDBG(DebugOut(DEBUG_WARN, L"[AVDTP] EnsureActiveModeThread:: Failed to write link policy settings.\n"));
    }

    avdtp_lock();
    avdtp_delref();

    if (VerifySession((HANDLE)lpArg) && pSession->EnsureActiveCount == 0) {
        // If we are no longer ensuring sniff mode then let's
        // schedule a thread to put the link in sniff.  We need
        // to do this here since the unsniff call above will
        // unschedule any existing sniff threads.
        ScheduleSniff(pSession);
    }

    avdtp_unlock();

    CeSetThreadPriority(hCurrentThreadId, nPrevThreadPriority);

    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    EnsureActiveMode
    
    This function spins off a thread to ensure we are in active mode.
    
    Parameters:
        pStream:        Stream handle
        fEnsureActive:  TRUE means make sure the link stays active, otherwise allow
                        sniff on this link.
------------------------------------------------------------------------------*/
int EnsureActiveMode(Stream* pStream, BOOL fEnsureActive)
{
    int iRes = ERROR_SUCCESS;

    SVSUTIL_ASSERT(avdtp_IsLocked());
   
    Session* pSession = VerifySession(pStream->hSessSignal);
    if (pSession) {        
        pSession->EnsureActiveCount = fEnsureActive ? pSession->EnsureActiveCount+1 : pSession->EnsureActiveCount-1;
        
        IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"EnsureActiveMode:: ba:%04x%08x ref:%d\n", pSession->ba.NAP, pSession->ba.SAP, pSession->EnsureActiveCount));
        
        if (pSession->EnsureActiveCount < 0) {
            // Reset ref in this case
            pSession->EnsureActiveCount = 0;
        }
        else if (pSession->EnsureActiveCount < 2) {
            // Unschedule an existing thread
            if (pSession->ActiveModeCookie) {
                btutil_UnScheduleEvent(pSession->ActiveModeCookie);
                pSession->ActiveModeCookie = 0;
            }

            // Spin a thread to ensure we are in active mode
            pSession->ActiveModeCookie = btutil_ScheduleEvent(EnsureActiveModeThread, pSession->hContext, 0);
        }
    } else {
        iRes = ERROR_NOT_FOUND;
    }

    return iRes;
}

// We are linked into btd.dll so cheat a little
int BthNotifyEvent (PBTEVENT pbtEvent, DWORD dwEventClass);

/*------------------------------------------------------------------------------
    NotifyAVDTPEvent
    
    Notify an AVDTP event via BT message queue
    
    Parameters:
        pStream:    Stream object related to the event 
------------------------------------------------------------------------------*/
void NotifyAVDTPEvent(Stream* pStream)
{
    Session* pSession = VerifySession(pStream->hSessSignal);
    if (pSession) {
        BD_ADDR bda = pSession->ba;
        DWORD dwState;
        if (pStream->state.IsStreaming()) {
            dwState = BT_AVDTP_STATE_STREAMING;
        } else if (pStream->state.IsOpen()) {
            dwState = BT_AVDTP_STATE_SUSPENDED;
        } else {
            SVSUTIL_ASSERT(pStream->state.IsInState(STREAM_STATE_CLOSING));
            dwState = BT_AVDTP_STATE_DISCONNECTED;
        }

        pStream->dwLastNotifiedEvent = dwState;

        BTEVENT bte;
        memset (&bte, 0, sizeof(BTEVENT));
        bte.dwEventId = BTE_AVDTP_STATE;

        BT_AVDTP_STATE_CHANGE *pEvent = (BT_AVDTP_STATE_CHANGE*) bte.baEventData;
        pEvent->dwSize = sizeof(BT_AVDTP_STATE_CHANGE);
        pEvent->dwState = dwState;
        pEvent->bta = SET_NAP_SAP(bda.NAP, bda.SAP);

        BthNotifyEvent(&bte, BTE_CLASS_AVDTP); 
    }
}


/*------------------------------------------------------------------------------
    FindTransactionContext
    
    This function finds a transaction context based on BD_ADDR, transaction id,
    and signaling id.  TransactionContext is used to keep track of implicit
    data between incoming requests and responses from the upper layer.
    
    Parameters:
        pba:            Address of the peer
        bTransaction:   Signaling transaction id
        bSignalId:      Signaling command/response id
    
    Returns (AVDT_NS.TransactionContext): Pointer to the TransactionContext
------------------------------------------------------------------------------*/
TransactionContext* FindTransactionContext(BD_ADDR* pba, BYTE bTransaction, BYTE bSignalId)
{
    TransactionContext* ptc = NULL;

    if ((bSignalId == AVDTP_SIGNAL_DISCOVER) ||
        (bSignalId == AVDTP_SIGNAL_GET_CAPABILITIES)) {
        Session* pSession = FindSession(pba, AVDTP_CHANNEL_SIGNALING);
        if (! pSession) {
            return NULL;
        }

        for (TransactionContextList::iterator it = pSession->tcl.begin(), itEnd = pSession->tcl.end(); it != itEnd; ++it) {
            if (((*it).bSignalId == bSignalId) && ((*it).bTransaction == bTransaction)) {
                ptc = &(*it);
                break;
            }
        }
    } 

    return ptc;
}

/*------------------------------------------------------------------------------
    FindStreamByTransactionContext
    
    This function looks up a stream based on Transaction Context.
    
    Parameters:
        bTransaction:   Signaling transaction id
        bSignalId:      Signaling command/response id
        ptc:            Transaction Context
    
    Returns (AVDT_NS.Stream): 
------------------------------------------------------------------------------*/
Stream* FindStreamByTransactionContext(BYTE bTransaction, BYTE bSignalId, TransactionContext* ptc)
{
    Stream* pStreamReturn = NULL;
    
    for (StreamList::iterator itStream = gpAVDTP->listStreams.begin(), itStreamEnd = gpAVDTP->listStreams.end(); itStream != itStreamEnd; ++itStream) {
        Stream* pStream = *itStream;
        for (TransactionContextList::iterator itTcl = pStream->tcl.begin(), itTclEnd = pStream->tcl.end(); itTcl != itTclEnd; ++itTcl) {
            if (((*itTcl).bSignalId == bSignalId) && ((*itTcl).bTransaction == bTransaction)) {
                pStreamReturn = pStream;
                *ptc = *itTcl;
                break;
            }
        }

        if (pStreamReturn) {
            break;
        }
    }   

    return pStreamReturn;
}

/*------------------------------------------------------------------------------
    DeleteTransactionContext
    
    This function delete a transaction context by specifying the BD_ADDR and
    the transaction context to delete.  This transaction context is associated
    with a session.
    
    Parameters:
        pba:    Address of the peer 
        ptc:    Transaction context 
------------------------------------------------------------------------------*/
void DeleteTransactionContext(BD_ADDR* pba, TransactionContext* ptc)
{
    Session* pSession = FindSession(pba, AVDTP_CHANNEL_SIGNALING);
    if (pSession) {
        for (TransactionContextList::iterator it = pSession->tcl.begin(), itEnd = pSession->tcl.end(); it != itEnd; ++it) {
            if (((*it).bSignalId == ptc->bSignalId) && ((*it).bTransaction == ptc->bTransaction)) {
                pSession->tcl.erase(it);
                break;
            }
        }
    }
}

/*------------------------------------------------------------------------------
    DeleteTransactionContext
    
    This function delete a transaction context by specifying the stream and
    the transaction context to delete.  This transaction context is associated
    with a stream.
    
    Parameters:
        pba:    Address of the peer
        ptc:    Transaction context 
------------------------------------------------------------------------------*/
void DeleteTransactionContext(Stream* pStream, TransactionContext* ptc)
{
    for (TransactionContextList::iterator it = pStream->tcl.begin(), itEnd = pStream->tcl.end(); it != itEnd; ++it) {
        if (((*it).bSignalId == ptc->bSignalId) && ((*it).bTransaction == ptc->bTransaction)) {
            pStream->tcl.erase(it);
            break;
        }
    }
}


/*------------------------------------------------------------------------------
    DeleteCall
    
    This function deletes the specified call.
    
    Parameters:
        pCall:  Call to be deleted 
------------------------------------------------------------------------------*/
void DeleteCall(Task* pTask)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    HANDLE hContext = pTask->hCallContext;

    for (TaskList::iterator it = gpAVDTP->listTasks.begin(), itEnd = gpAVDTP->listTasks.end(); it != itEnd; ++it) {
        if ((*it)->hCallContext == hContext) {            
            Task* pCall = *it;            
            gpAVDTP->listTasks.erase(it);
            btutil_CloseHandle((SVSHandle)pCall->hCallContext);
            delete pCall;
            break;
        }
    }
}

/*------------------------------------------------------------------------------
    DeleteSession
    
    This function deletes the specified session.
    
    Parameters:
        pSession: Session to be deleted
------------------------------------------------------------------------------*/
int DeleteSession(Session* pSession)
{
    for (SessionList::iterator it = gpAVDTP->listSessions.begin(), itEnd = gpAVDTP->listSessions.end(); it != itEnd; ++it) {
        if ((*it)->hContext == pSession->hContext) {
            Session* pSess = *it;
            gpAVDTP->listSessions.erase(it);
            btutil_CloseHandle((SVSHandle)pSess->hContext);
            delete pSess;
            return ERROR_SUCCESS;
        }
    }

    return ERROR_NOT_FOUND;
}

/*------------------------------------------------------------------------------
    DeleteStream
    
    This function deletes the specified stream.
    
    Parameters:
        pStream: Stream to be deleted
------------------------------------------------------------------------------*/
int DeleteStream(Stream* pStream)
{
    for (StreamList::iterator it = gpAVDTP->listStreams.begin(), itEnd = gpAVDTP->listStreams.end(); it != itEnd; ++it) {
        if ((*it)->hContext == pStream->hContext) {
            Stream* pStrm = *it;
            gpAVDTP->listStreams.erase(it);
            btutil_CloseHandle((SVSHandle)pStrm->hContext);
            delete pStrm;
            return ERROR_SUCCESS;
        }
    }

    return ERROR_NOT_FOUND;
}

/*------------------------------------------------------------------------------
    NewSession
    
    This function creates a new session and adds it to the list
    
    Parameters:
        pba:            Address of the peer
        fStage:         Initial stage of the session
        cid:            L2CAP cid of the session
        bChannelType:   Channel type (media or signaling)
    
    Returns (AVDT_NS.Session): Pointer to the Session
------------------------------------------------------------------------------*/
Session* NewSession(BD_ADDR* pba, unsigned int fStage, unsigned short cid, BYTE bChannelType, BOOL fIncoming, UCHAR l2cap_id)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Session* pSess = new Session(pba, fStage, cid, bChannelType, fIncoming, l2cap_id);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"NewSession : ERROR_OUTOFMEMORY\n"));
        return NULL;
    }

    pSess->usInMTU = gpAVDTP->usInMTU;

    pSess->hContext = (HANDLE) btutil_AllocHandle(pSess);
    if (SVSUTIL_HANDLE_INVALID == pSess->hContext) {
        IFDBG(DebugOut(DEBUG_ERROR, L"NewSession : ERROR_OUTOFMEMORY\n"));
        delete pSess;
        return NULL;
    }

    if (! gpAVDTP->listSessions.push_front(pSess)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"NewSession : Can't insert in list\n"));
        btutil_CloseHandle((SVSHandle)pSess->hContext);
        delete pSess;
        return NULL;
    }

    return pSess;
}

/*------------------------------------------------------------------------------
    NewStream
    
    This function creates a new stream and adds it to the list.
    
    Parameters:        
        bLocalSeid:     Local SEID
        bRemoteSeid:    Remote SEID
        hOwner:         Handle to the owner context
        hSessSignal:    Handle of the signaling session
    
    Returns (AVDT_NS.Stream): Pointer to the Stream
------------------------------------------------------------------------------*/
Stream* NewStream(BYTE bLocalSeid, BYTE bRemoteSeid, HANDLE hOwner, HANDLE hSessSignal)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Stream* pStream = new Stream(bLocalSeid, bRemoteSeid, hOwner, hSessSignal);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"NewStream : ERROR_OUTOFMEMORY\n"));
        return NULL;
    }

    pStream->dwHciBuffThreshold = gpAVDTP->dwHciBuffThreshold;
    pStream->dwHciBuffThresholdSleep = gpAVDTP->dwHciBuffThresholdSleep;

    pStream->hContext = (HANDLE) btutil_AllocHandle(pStream);
    if (SVSUTIL_HANDLE_INVALID == pStream->hContext) {
        IFDBG(DebugOut(DEBUG_ERROR, L"NewStream : ERROR_OUTOFMEMORY\n"));
        delete pStream;
        return NULL;
    }

    if (! gpAVDTP->listStreams.push_front(pStream)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"NewStream : Can't insert in list\n"));
        btutil_CloseHandle((SVSHandle)pStream->hContext);
        delete pStream;
        return NULL;
    }    

    return pStream;
}

/*------------------------------------------------------------------------------
    NewTask
    
    This function creates a new Task (aka call) and adds it to the list.
    
    Parameters:
        hOwner:     Handle to the owner context
        hSession:   Handle to the session or stream
        pContext:   Owner's call context
        bSignalId:  Signaling Id of the call
    
    Returns (AVDT_NS.Task): Pointer to the Task struct
------------------------------------------------------------------------------*/
Task* NewTask(HANDLE hOwner, HANDLE hSession, void* pContext, BYTE bSignalId)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    Task* pTask = new Task(hOwner, hSession, pContext, bSignalId);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"NewTask : ERROR_OUTOFMEMORY\n"));
        return NULL;
    }

    pTask->hCallContext = (HANDLE) btutil_AllocHandle(pTask);
    if (SVSUTIL_HANDLE_INVALID == pTask->hCallContext) {
        IFDBG(DebugOut(DEBUG_ERROR, L"NewTask : ERROR_OUTOFMEMORY\n"));
        delete pTask;
        return NULL;
    }

    if (! gpAVDTP->listTasks.push_front(pTask)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"NewTask : Can't insert in list\n"));
        btutil_CloseHandle((SVSHandle)pTask->hCallContext);
        delete pTask;
        return NULL;
    }
    
    return pTask; 
}

/*------------------------------------------------------------------------------
    CancelCall
    
    This function cancels a call and notifies the owner context.
    
    Parameters:
        pTask:  Task object
        iError: Reason for canceling
------------------------------------------------------------------------------*/
void CancelCall(Task* pTask, int iError)
{
    void *pUserContext = pTask->pContext;

    AVDTP_CONTEXT* pOwner = pTask->hOwner ? VerifyOwner(pTask->hOwner) : NULL;

    DeleteCall(pTask);

    if (! pOwner) {
        // No owner to notify
        return;
    }

    BT_LAYER_CALL_ABORTED pCallback = pOwner->c.avdtp_CallAborted;

    pOwner->AddRef();
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"CancelCall : going into avdtp_CallAborted\n"));

    __try {
        pCallback(pUserContext, iError);
    } __except (1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] CancelCall :: exception in avdtp_CallAborted\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"CancelCall : came out of avdtp_CallAborted\n"));    
}

static int avdtp_call_aborted(void* pCallContext, int iError);

/*------------------------------------------------------------------------------
    TimeoutCall
    
    This thread is called when a call is to be timed out.
    
    Parameters:
        pArg: Points to a call
    
    Returns (DWORD): Thread return value
------------------------------------------------------------------------------*/
DWORD WINAPI TimeoutCall(LPVOID pArg)
{
    avdtp_call_aborted((HANDLE)pArg, ERROR_TIMEOUT);
    return 0;
}

/*------------------------------------------------------------------------------
    ScheduleTimeout
    
    This function schedules a call to be timed out.
    
    Parameters:
        pCall:          Call to timeout
        iTimeoutSec:    Number of seconds before timeout
------------------------------------------------------------------------------*/
void ScheduleTimeout(Task* pCall, int iTimeoutSec)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"ScheduleTimeout %d : call:%04x%08x\n", iTimeoutSec, pCall->hCallContext));

    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    if (pCall->TimeoutCookie) {
        btutil_UnScheduleEvent(pCall->TimeoutCookie);
    }

    pCall->TimeoutCookie = btutil_ScheduleEvent(TimeoutCall, pCall->hCallContext, iTimeoutSec * 1000);
}

/*------------------------------------------------------------------------------
    ClearTimeout
    
    This function clears a session timeout.
    
    Parameters:
        pSession: Session to clear timeout for
------------------------------------------------------------------------------*/
void ClearTimeout(Session* pSession)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"ClearTimeout : session:%04x%08x\n", pSession->ba.NAP, pSession->ba.SAP));
    
    if (pSession->TimeoutCookie) {
        btutil_UnScheduleEvent(pSession->TimeoutCookie);
    }

    pSession->TimeoutCookie = 0;
}

/*------------------------------------------------------------------------------
    ClearTimeout
    
    This function clears a stream timeout.
    
    Parameters:
        pStream: Stream to clear timeout for
------------------------------------------------------------------------------*/
void ClearTimeout(Stream* pStream)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"ClearTimeout : stream:0x%08x\n", pStream->hContext));
    
    if (pStream->TimeoutCookie) {
        btutil_UnScheduleEvent(pStream->TimeoutCookie);
    }

    pStream->TimeoutCookie = 0;
}

/*------------------------------------------------------------------------------
    ClearTimeout
    
    This function clears a call timeout.
    
    Parameters:
        pCall: Call to clear timeout for
------------------------------------------------------------------------------*/
void ClearTimeout(Task* pCall)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"ClearTimeout : call:0x%08x\n", pCall->hCallContext));
    
    if (pCall->TimeoutCookie) {
        btutil_UnScheduleEvent(pCall->TimeoutCookie);
    }

    pCall->TimeoutCookie = 0;
}

/*------------------------------------------------------------------------------
    CloseSession
    
    This function closes the specified session with the specified error.
    
    Parameters:
        pSession:   Session to close
        iError:     Reason for closing
------------------------------------------------------------------------------*/
int CloseSession(Session* pSession, int iError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"CloseSession : %04x%08x\n", pSession->ba.NAP, pSession->ba.SAP));        
        
    SVSUTIL_ASSERT(avdtp_IsLocked());

    ClearTimeout(pSession);

    // Remove all outstanding calls associated with this session
    for (TaskList::iterator it = gpAVDTP->listTasks.begin(), itEnd = gpAVDTP->listTasks.end(); it != itEnd;) {
        if ((*it)->hSession == pSession->hContext) {
            Task* pCall = *it;
            it++;
            CancelCall(pCall, iError);
        } else {
            ++it;
        }
    }    

    // Disconnect the L2CAP channel if the stack is up
    if (gpAVDTP->state.IsConnected() && pSession->cid != INVALID_CID) {
        HANDLE h = gpAVDTP->hL2CAP;
        unsigned short cid = pSession->cid;

        L2CA_Disconnect_In pCallback = gpAVDTP->l2cap_if.l2ca_Disconnect_In;

        avdtp_addref();
        avdtp_unlock();

        IFDBG(DebugOut (DEBUG_AVDTP_CALLBACK, L"CloseSession : going into l2ca_Disconnect_In\n"));

        __try {
            pCallback(h, NULL, cid);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] CloseSession :: Exception in l2ca_Disconnect_In\n"));
        }

        avdtp_lock();
        avdtp_delref();
        
        IFDBG(DebugOut (DEBUG_AVDTP_CALLBACK, L"CloseSession : came out of l2ca_Disconnect_In\n"));
    }
    
    // Remove the session from the list and delete it
    DeleteSession(pSession);
        
    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    SendPendingPacketsThread
    
    This function sends any packets pending on the existing stream.  Packets
    can be pending when the session is not up yet when a call is sent.
    
    Parameters:
        pSession: 
------------------------------------------------------------------------------*/
DWORD WINAPI SendPendingPacketsThread(LPVOID lpArg)
{
    HANDLE hSession = (Session*)lpArg;

    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"SendPendingPacketsThread : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }
    
    Session* pSession = VerifySession(hSession);
    if (! pSession) {
        IFDBG(DebugOut(DEBUG_ERROR, L"SendPendingPacketsThread : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }    

    int iRes = ERROR_SUCCESS;

    // TODO: Can GetCapabilities get ahead of discover in the list?  Both probably can't be in the list so this should be fine.
    for (TaskList::iterator it = gpAVDTP->listTasks.begin(), itEnd = gpAVDTP->listTasks.end(); it != itEnd; ++it) {
        Task* pCall = *it;
        if (pCall->bSignalId == AVDTP_SIGNAL_DISCOVER) {
            if (pCall->hSession == pSession->hContext) {
                pCall->bTransaction = pSession->bTransaction;
                iRes = gpAVDTP->signal.DiscoverRequest(pSession, (LPVOID)pCall->hCallContext, &pSession->ba);
            }
            if (ERROR_SUCCESS != iRes) {
                break;
            }
        } else if (pCall->bSignalId == AVDTP_SIGNAL_GET_CAPABILITIES) {
            Stream* pStream = VerifyStream(pCall->hSession);
            if (pStream && (pStream->hSessSignal == pSession->hContext)) {
                pCall->bTransaction = pSession->bTransaction;
                iRes = gpAVDTP->signal.GetCapabilitiesRequest(pSession, (LPVOID)pCall->hCallContext, pCall->bAcpSEID);
            }
            if (ERROR_SUCCESS != iRes) {
                break;
            }
        } 
    }

    avdtp_unlock();

    return iRes;
}

/*------------------------------------------------------------------------------
    SessionIsUp
    
    This function is called when a session comes up.
    
    Parameters:
        pSession:   The session that just came up 
------------------------------------------------------------------------------*/
void SessionIsUp(Session* pSession)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++SessionIsUp: ba:%04x%08x\n", pSession->ba.NAP, pSession->ba.SAP));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Session* pSessSignal = FindSession(&pSession->ba, AVDTP_CHANNEL_SIGNALING);
    if (pSessSignal) {
        // If we find a singaling session with same address, it means this
        // is the media session that has been opened.
        
        pSession->bChannelType = AVDTP_CHANNEL_MEDIA;

        // There must only be one stream in the open state with the
        // specified signaling session, and no media session yet.
        Stream* pStream = FindOpenStream(pSessSignal->hContext, pSession->hContext);
        if (pStream) {
            pStream->hSessMedia = pSession->hContext;

            AVDTP_CONTEXT* pOwner = VerifyOwner(pStream->hOwner);

            // If necessary, send delayed open confirmation
            if (pOwner && pStream->pOpenCallContext) {
                AVDTP_Open_Out pCallback = pOwner->c.avdtp_Open_Out;
                void *pCallContext = pStream->pOpenCallContext;
                HANDLE hStream = pStream->hContext;
                
                pOwner->AddRef();
                avdtp_unlock();                

                __try {
                    pCallback(pCallContext, hStream, 0);
                } __except(1) {
                    IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] SessionIsUp : exception in avdtp_Open_Out\n"));
                }

                avdtp_lock();
                pOwner->DelRef();    
            }            
        } else {
            SVSUTIL_ASSERT(0);
        }


    } else {
        // This is a signaling session
        pSession->bChannelType = AVDTP_CHANNEL_SIGNALING;

        // The session is now up.  Any packets that were pending should now be sent out.
        if (! btutil_ScheduleEvent(SendPendingPacketsThread, pSession->hContext)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"SessionIsUp : Failed to schedule pending packet send thread, closing session.\n"));
            CloseSession(pSession, ERROR_OPERATION_ABORTED);
        }
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--SessionIsUp\n", pSession->ba.NAP, pSession->ba.SAP));
}

/*------------------------------------------------------------------------------
    StartSession
    
    This function is called to create a new session and start it.
    
    Parameters:
        pba:            Address of the peer
        bChannelType:   Channel type of the session (media or signaling)
    
    Returns (AVDT_NS.Session): Pointer to the Session
------------------------------------------------------------------------------*/
Session* StartSession(BD_ADDR* pba, BYTE bChannelType)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++StartSession: ba:%04x%08x\n", pba->NAP, pba->SAP));
    
    int iRes = ERROR_SUCCESS;
    
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    Session* pSession = NewSession(pba, NONE, 0, bChannelType, FALSE, 0);
    if (! pSession) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    Task* pTask = NewTask(NULL, pSession->hContext, NULL, AVDTP_CALL_LINK_SETUP);
    if (! pTask) {
        iRes = ERROR_OUTOFMEMORY;
        goto exit;
    }

    if (bChannelType == AVDTP_CHANNEL_SIGNALING) {
        SVSUTIL_ASSERT(gpAVDTP->dwHciThreadId != GetCurrentThreadId());
        BthWritePageTimeout(gpAVDTP->usPageTimeout); // if this fails, we continue anyway
    }

    HANDLE h = gpAVDTP->hL2CAP;
    HANDLE hContext = pTask->hCallContext;
    L2CA_ConnectReq_In pCallback = gpAVDTP->l2cap_if.l2ca_ConnectReq_In;

    avdtp_addref();
    avdtp_unlock();

    iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback(h, (LPVOID)hContext, AVDTP_PSM, pba);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] Exception in l2ca_ConnectReq_In\n"));
    }

    avdtp_lock();
    avdtp_delref();

    if (! gpAVDTP->state.IsConnected()) {
        iRes = ERROR_OPERATION_ABORTED;
        goto exit;
    }

exit:
    if (ERROR_SUCCESS != iRes) {
        if (pTask) {
            DeleteCall(pTask);            
        }
        if (pSession) {
            CloseSession(pSession, ERROR_OPERATION_ABORTED);
            pSession = NULL;                
        }
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--StartSession: result=%d\n", iRes));
    
    return pSession;
}

/*------------------------------------------------------------------------------
    TimeoutSession
    
    This thread is called when a session is to be timed out.
    
    Parameters:
        pArg: Pointer to the session
    
    Returns (DWORD): Thread return value
------------------------------------------------------------------------------*/
DWORD WINAPI TimeoutSession(LPVOID pArg)
{
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"TimeoutSession : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    Session* pSession = VerifySession((HANDLE)pArg);
    if (pSession) {
        IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"TimeoutSession : %04x%08x\n", pSession->ba.NAP, pSession->ba.SAP));        
        CloseSession(pSession, ERROR_TIMEOUT);
    }

    avdtp_unlock();

    return 0;
}

/*------------------------------------------------------------------------------
    ScheduleTimeout
    
    This function is called to time out a session.
    
    Parameters:
        pSession:       Session to timeout
        iTimeoutSec:    Number of seconds before timeout
------------------------------------------------------------------------------*/
void ScheduleTimeout(Session* pSession, int iTimeoutSec)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"ScheduleTimeout %d : %04x%08x\n", iTimeoutSec, pSession->ba.NAP, pSession->ba.SAP));

    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    if (pSession->TimeoutCookie) {
        btutil_UnScheduleEvent(pSession->TimeoutCookie);
    }

    pSession->TimeoutCookie = btutil_ScheduleEvent(TimeoutSession, pSession->hContext, iTimeoutSec * 1000);
}

/*------------------------------------------------------------------------------
    CloseStream
    
    This function closes a specified stream.
    
    Parameters:
        pStream:            Stream object
        fCloseSignallingSession:  Close the signalling session
        iError:             Reason for closing the stream
------------------------------------------------------------------------------*/
int CloseStream(Stream* pStream, BOOL fCloseSignallingSession, int iError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"CloseStream : 0x%08X\n", pStream->hContext));   
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    ClearTimeout(pStream);

    HANDLE hStream = pStream->hContext;

    // Remove all outstanding calls associated with this stream
    for (TaskList::iterator it = gpAVDTP->listTasks.begin(), itEnd = gpAVDTP->listTasks.end(); it != itEnd;) {
        if ((*it)->hSession == hStream) {
            Task* pCall = *it;
            it++;
            CancelCall(pCall, iError);
        } else {
            ++it;
        }
    }

    if (! VerifyStream(hStream)) {
        // We may have called CancelCall which unlocks so we should recheck the stream
        return ERROR_SUCCESS;
    }

    // Check if the stream was in a configured state at one point
    BOOL fWasConfigured = ! pStream->state.IsInState(STREAM_STATE_IDLE);

    pStream->state.Closing();

    if (fWasConfigured) {
        // We only want to send stream aborted if we were in a configured state at one point.  Otherwise,
        // the upper layer has never heard of this stream and we can quietly abort.
        
        if (pStream->dwLastNotifiedEvent) {
            // Make sure we notify the msg queue of a close assuming an open or streaming message
            // was sent last.
            NotifyAVDTPEvent(pStream);
        }

        // Signal owner that this stream is being aborted
        AVDTP_CONTEXT* pOwner = VerifyOwner(pStream->hOwner);
        if (pOwner) {
            AVDTP_StreamAbortedEvent pCallback = pOwner->ei.avdtp_StreamAbortedEvent;
            if (pCallback) {
                void *pUserContext = pOwner->pUserContext;

                IFDBG(DebugOut (DEBUG_AVDTP_CALLBACK, L"Going into StreamAbortedEvent notification\n"));

                pOwner->AddRef();
                avdtp_unlock();

                __try {
                    pCallback(pUserContext, hStream, iError);
                } __except (1) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] CloseStream: exception in avdtp_StreamAbortedEvent!\n"));
                }

                avdtp_lock();
                pOwner->DelRef();
                
                IFDBG(DebugOut (DEBUG_AVDTP_CALLBACK, L"Came back StreamAbortedEvent notification\n")); 
            }
        }
    }
    
    // Close media session
    if (VerifyStream(hStream) && pStream->hSessMedia) {
        Session* pSession = VerifySession(pStream->hSessMedia);
        if (pSession) {
            CloseSession(pSession, iError);
        }
        pStream->hSessMedia = 0;
    }

    // Close the signaling session
    if (fCloseSignallingSession && VerifyStream(hStream) && pStream->hSessSignal) {
        Session* pSession = VerifySession(pStream->hSessSignal);
        if (pSession) {
            pStream->hSessSignal = 0; // clear this before trying to find a different stream
            if (! FindStream(pSession->hContext)) {
                // No other stream is using this session, go ahead and close it
                EnsureActiveMode(pStream, FALSE);
                CloseSession(pSession, iError);
            }        
        }
    }

    // Check the stream again and then delete it
    if (VerifyStream(hStream)) {
        DeleteStream(pStream);
    }   

    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    CloseStreamsBySession
    
    This function closes all streams with the secified session handle
    
    Parameters:
        hSession:   Unique handle to a session
        iError:     Reason for closing the streams
------------------------------------------------------------------------------*/
int CloseStreamsBySession(HANDLE hSession, int iError)
{
    BOOL fFound = FALSE;
        
    for (StreamList::iterator it = gpAVDTP->listStreams.begin(), itEnd = gpAVDTP->listStreams.end(); it != itEnd;) {
        if ((*it)->hSessSignal == hSession) {
            Stream* pStream = *it;
            ++it;
            CloseStream(pStream, TRUE, iError);
            fFound = TRUE;
        } else if ((*it)->hSessMedia == hSession) {
            Stream* pStream = *it;
            ++it;
            CloseStream(pStream, FALSE, iError);
            fFound = TRUE;
        } else {
            ++it;
        }
    }

    return fFound ? ERROR_SUCCESS : ERROR_NOT_FOUND;
}

/*------------------------------------------------------------------------------
    CloseStreamsByOwner
    
    Close all streams with the specified owner context
    
    Parameters:
        hOwner: Unique handle to the owner context
        iError: Reason for closing the streams
------------------------------------------------------------------------------*/
int CloseStreamsByOwner(HANDLE hOwner, int iError)
{
    BOOL fFound = FALSE;
    
    for (StreamList::iterator it = gpAVDTP->listStreams.begin(), itEnd = gpAVDTP->listStreams.end(); it != itEnd;) {
        if ((*it)->hOwner == hOwner) {
            Stream* pStream = *it;
            ++it;
            CloseStream(pStream, TRUE, iError);
            fFound = TRUE;
        } else {
            ++it;
        }
    }

    return fFound ? ERROR_SUCCESS : ERROR_NOT_FOUND;
}

/*------------------------------------------------------------------------------
    CloseIdleSessions
    
    This function closes sessions in the IDLE state (i.e. No corresponding
    stream that is configured)
    
    Parameters: None
------------------------------------------------------------------------------*/
int CloseIdleSessions(void)
{
    for (SessionList::iterator itSess = gpAVDTP->listSessions.begin(), itSessEnd = gpAVDTP->listSessions.end(); itSess != itSessEnd;) {
        Session* pSession = *itSess;
        HANDLE hSession = pSession->hContext;

        itSess++;

        BOOL fFound = FALSE;

        // Loop through streams to find this session and check if it is configured
        for (StreamList::iterator itStream = gpAVDTP->listStreams.begin(), itStreamEnd = gpAVDTP->listStreams.end(); itStream != itStreamEnd; ++itStream) {
            if ((hSession == (*itStream)->hSessSignal) || (hSession == (*itStream)->hSessMedia)) {
                fFound = TRUE;
                if (! (*itStream)->state.IsConfigured()) {
                    CloseStream(*itStream, TRUE, ERROR_CANCELLED);
                }
            }
        }

        if (! fFound) {
            // This is an idle session
            CloseSession(pSession, ERROR_CANCELLED);
        }
    }

    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    TimeoutStream
    
    This thread is called when a stream is to be timed out.
    
    Parameters:
        pArg: Pointer to the stream
    
    Returns (DWORD): Thread return value
------------------------------------------------------------------------------*/
DWORD WINAPI TimeoutStream(LPVOID pArg)
{
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"TimeoutStream : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    Stream* pStream = VerifyStream((HANDLE)pArg);
    if (pStream) {
        SVSUTIL_ASSERT(! pStream->state.IsConfigured());        
        IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"TimeoutStream : 0x%08x\n", pStream->hContext));
        CloseStream(pStream, TRUE, ERROR_TIMEOUT);
    }

    avdtp_unlock();

    return 0;
}

/*------------------------------------------------------------------------------
    ScheduleTimeout
    
    This function is called to timeout a stream
    
    Parameters:
        pStream:        Stream to timeout
        iTimeoutSec:    Number of seconds before timeout
------------------------------------------------------------------------------*/
void ScheduleTimeout(Stream* pStream, int iTimeoutSec)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"ScheduleTimeout %d : 0x%08x\n", iTimeoutSec, pStream->hContext));

    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    if (pStream->TimeoutCookie) {
        btutil_UnScheduleEvent(pStream->TimeoutCookie);
    }

    pStream->TimeoutCookie = btutil_ScheduleEvent(TimeoutStream, pStream->hContext, iTimeoutSec * 1000);
}


/*------------------------------------------------------------------------------
    L2CAPSend
    
    This helper function sends data down to L2CAP
    
    Parameters:
        lpvContext:     Call context for async processing
        cid:            L2CAP channel id
        pBuffer:        Buffer to send
        pSession:       Session object for sniff control
        fUnSniff:       Unsniff the link before sending data
                        NOTE: Must be false when called on HCI reader thread.
        fScheduleSniff: Schedule Sniff mode after send
------------------------------------------------------------------------------*/
int L2CAPSend(LPVOID lpvContext, USHORT cid, BD_BUFFER* pBuffer, Session* pSession, BOOL fUnSniff, BOOL fScheduleSniff)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    int iRes = ERROR_INTERNAL_ERROR;

    if (fUnSniff) {        
        SVSUTIL_ASSERT(pSession);
        UnSniffIfNeeded(pSession);
    }

    HANDLE h = gpAVDTP->hL2CAP;
    L2CA_DataDown_In pCallback = gpAVDTP->l2cap_if.l2ca_DataDown_In;
    
    avdtp_addref();
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"L2CAPSend : going into l2ca_DataDown_In\n"));

    __try {
        iRes = pCallback(h, lpvContext, cid, pBuffer);
    } __except (1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] L2CAPSend :: exception in l2ca_DataDown_In!\n"));
    }
    
    avdtp_lock();
    avdtp_delref();

    if (fScheduleSniff) {
        SVSUTIL_ASSERT(pSession);
        ScheduleSniff(pSession);
    }

    IFDBG(DebugOut (DEBUG_AVDTP_CALLBACK, L"L2CAPSend : came out of l2ca_DataDown_In\n"));
    IFDBG(DebugOut (DEBUG_AVDTP_TRACE, L"L2CAPSend returns %d\n", iRes));

    return iRes;
}

/*------------------------------------------------------------------------------
    GetAVDTPBuffer
    
    This function allocates an AVDPT buffer and leaves the required room for
    headers/trailers.
    
    Parameters:
        cbTotal:    L2CAP payload size required.
    
    Returns (BD_BUFFER): 
------------------------------------------------------------------------------*/
BD_BUFFER* GetAVDTPBuffer(DWORD cbTotal)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    unsigned int cbAlloc;
    if (! SUCCEEDED(CeUIntAdd3(cbTotal, gpAVDTP->cDataHeaders, gpAVDTP->cDataTrailers, &cbAlloc))) {
        return NULL;
    }
    
    BD_BUFFER* pBuffer = BufferAlloc(cbAlloc);
    if (pBuffer) {
        pBuffer->cStart = gpAVDTP->cDataHeaders;
        pBuffer->cEnd = pBuffer->cStart + cbTotal;
    }

    return pBuffer;
}

/*------------------------------------------------------------------------------
    SendConfigRequest
    
    This function sends an L2CAP config request on the specified session.
    
    Parameters:
        pSession:   Session object 
------------------------------------------------------------------------------*/
int SendConfigRequest(Session* pSession)
{
    IFDBG(DebugOut (DEBUG_AVDTP_TRACE, L"SendConfigRequest %04x%08x\n", pSession->ba.NAP, pSession->ba.SAP));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    Task* pTask = NewTask(NULL, pSession->hContext, NULL, AVDTP_CALL_LINK_CONFIG);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SendConfigRequest:: ERROR_OUTOFMEMORY\n"));
        return ERROR_OUTOFMEMORY;
    }

    HANDLE h = gpAVDTP->hL2CAP;
    HANDLE hContext = pTask->hCallContext;
    L2CA_ConfigReq_In pCallback = gpAVDTP->l2cap_if.l2ca_ConfigReq_In;
    unsigned short cid = pSession->cid;
    unsigned short mtu = pSession->usInMTU;

    avdtp_addref();
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"SendConfigRequest : going into l2ca_ConfigReq_In\n"));

    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback(h, (LPVOID)hContext, cid, mtu, 0xffff, NULL, 0, NULL);
    } __except (1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTP :: Exception in l2ca_ConfigReq_In!\n"));
    }

    avdtp_lock();
    avdtp_delref();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"SendConfigRequest : came out of l2ca_ConfigReq_In\n"));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    StackUp
    
    This thread is called when a StackUp event comes from the lower layers.
    
    Parameters:
        lpv: Not used
    
    Returns (DWORD): Thread return value
------------------------------------------------------------------------------*/
DWORD WINAPI StackUp(LPVOID lpv)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++StackUp\n"));

    if (! WaitForRefToOneAndLock()) {
        // On failure we are unlocked
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    GetConnectionState();

    if (gpAVDTP->state.IsConnected()) {
        avdtp_addref();
        DispatchStackEvent(BTH_STACK_UP);
        avdtp_delref();
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--StackUp\n"));
    
    return 0;
}

/*------------------------------------------------------------------------------
    StackDown
    
    This thread is called when StackDown event comes from the lower layers.
    
    Parameters:
        lpv: Not used 
    
    Returns (DWORD): Thread return value
------------------------------------------------------------------------------*/
DWORD WINAPI StackDown(LPVOID lpv)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++StackDown\n"));
    
    if (! WaitForRefToOneAndLock()) {
        // On failure we are unlocked
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    //
    // Clean up AVDTP state
    //
    
    avdtp_addref();

    for (StreamList::iterator itStream = gpAVDTP->listStreams.begin(), itStreamEnd = gpAVDTP->listStreams.end(); itStream != itStreamEnd;) {
        Stream* pStream = *itStream;
        ++itStream;
        CloseStream(pStream, TRUE, ERROR_OPERATION_ABORTED);
    }
    
    // All sessions & tasks should be closed at this point
    SVSUTIL_ASSERT(gpAVDTP->listSessions.empty());
    SVSUTIL_ASSERT(gpAVDTP->listTasks.empty());

    gpAVDTP->state.StackDown();

    DispatchStackEvent(BTH_STACK_DOWN);

    avdtp_delref();
    
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--StackDown\n"));

    return 0;
}

/*------------------------------------------------------------------------------
    StackReset
    
    This thread is called when a StackReset event comes from the lower layers.
    
    Parameters:
        lpv: Not used
    
    Returns (DWORD): Thread return value
------------------------------------------------------------------------------*/
DWORD WINAPI StackReset(LPVOID lpv)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++StackReset\n"));
    
    StackDown (NULL);
    StackUp (NULL);

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--StackReset\n"));
    
    return 0;
}

/*------------------------------------------------------------------------------
    StackDisconnect
    
    This thread is called when a StackDisconnect event comes from the lower 
    layers.
    
    Parameters:
        lpv: Not used. 
    
    Returns (DWORD): Thread return value
------------------------------------------------------------------------------*/
DWORD WINAPI StackDisconnect(LPVOID lpv)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++StackReset\n"));

    avdtp_CloseDriverInstance();
        
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++StackReset\n"));
    
    return 0;
}

/*------------------------------------------------------------------------------
    AuthThread
    
    This thread is called for incoming or outgoing connection request in order
    to authenticate and encrypt all AVDTP baseband links.
    
    Parameters:
        lpv: Void pointer which in this case is a session handle 
    
    Returns (DWORD): Thread return value
------------------------------------------------------------------------------*/
DWORD WINAPI AuthThread(LPVOID lpv)
{
    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"AuthThread : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    BOOL fSecError = FALSE;
    
    HANDLE hSession = (HANDLE) lpv;

    Session* pSession = VerifySession(hSession);
    if (! pSession) {
        IFDBG(DebugOut (DEBUG_ERROR, L"AuthThread : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }    

    BD_ADDR ba = pSession->ba;
    unsigned char id = pSession->l2cap_id;
    unsigned short cid = pSession->cid;

    L2CA_ConnectResponse_In pCallback = gpAVDTP->l2cap_if.l2ca_ConnectResponse_In;
    HANDLE h = gpAVDTP->hL2CAP;

    // Send a pending connect response for incoming

    if (pSession->fIncoming) {
        avdtp_addref();
        avdtp_unlock();

        IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"AuthThread : going into l2ca_ConnectResponse_In\n"));

        int iRes = ERROR_INTERNAL_ERROR;
        __try {
            iRes = pCallback(h, NULL, &ba, id, cid, 1, 1);
        } __except (1) {
            IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] AuthThread :: exception in l2ca_ConnectResponse_In\n"));
        }

        avdtp_lock();
        avdtp_delref();
    }     

    // Authenticate & encrypt the link
    
    BT_ADDR bt = SET_NAP_SAP(ba.NAP, ba.SAP);

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"AuthThread : Setting auth & encryption on AVDTP link\n"));

    avdtp_addref();
    avdtp_unlock();

    int iSecErr = BthAuthenticate(&bt);
    if (iSecErr == ERROR_SUCCESS) {
        iSecErr = BthSetEncryption(&bt, TRUE);
    }

    avdtp_lock();
    avdtp_delref();   

    if (iSecErr != ERROR_SUCCESS) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AuthThread : Error setting auth & encryption on AVDTP link: %d\n", iSecErr));
        fSecError = TRUE;
    }

    // Ensure the session is still up
    if (pSession != VerifySession(hSession)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] AuthThread : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    // Send the corresponding response
        
    if (pSession->fIncoming) {
        avdtp_addref();
        avdtp_unlock();

        IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"AuthThread : going into l2ca_ConnectResponse_In\n"));

        int iRes = ERROR_INTERNAL_ERROR;
        __try {
            iRes = pCallback(h, NULL, &ba, id, cid, (fSecError ? 3 : 0), 0);
        } __except (1) {
            IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] AuthThread :: exception in l2ca_ConnectResponse_In\n"));
        }

        avdtp_lock();
        avdtp_delref();

        IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"AuthThread : came out of l2ca_ConnectResponse_In result=%d\n", iRes));

        if (VerifySession(hSession)) {
            if (!fSecError && (iRes == ERROR_SUCCESS) && gpAVDTP->state.IsConnected()) {
                if (ERROR_SUCCESS != SendConfigRequest(pSession)) {
                    // On error close sesssion
                    CloseSession(pSession, ERROR_PROTOCOL_UNREACHABLE);
                }
            } else {
                CloseSession(pSession, (fSecError ? iSecErr : iRes));
            }
        }
    } else {
        if (fSecError) {
            CloseSession (pSession, iSecErr);
        } else {
            int iErr = SendConfigRequest(pSession);
            if (gpAVDTP->state.IsConnected() && (iErr != ERROR_SUCCESS)) {
                CloseSession (pSession, iErr);
            }
        }
    }
    
    avdtp_unlock();

    return ERROR_SUCCESS;    
}

//
// Lower layer events
//


/*------------------------------------------------------------------------------
    avdtp_stack_event
    
    This function is called when a Stack event comes from L2CAP.
    
    Parameters:
        pUserContext:   AVDPT context
        iEvent:         The event the fired
        pEventContext:  Any context associated with the event
------------------------------------------------------------------------------*/
static int avdtp_stack_event(
                void* pUserContext, 
                int iEvent, 
                void* pEventContext)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_stack_event 0x%08X iEvent:%d pEventContext:0x%08X\n", 
        pUserContext, iEvent, pEventContext));

    SVSUTIL_ASSERT(pUserContext == gpAVDTP);

    switch (iEvent) {
        case BTH_STACK_RESET:
            IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"AVDTP : Stack reset\n"));
            btutil_ScheduleEvent(StackReset, NULL);
            break;

        case BTH_STACK_DOWN:
            IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"AVDTP : Stack down\n"));
            btutil_ScheduleEvent(StackDown, NULL);
            break;

        case BTH_STACK_UP:
            IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"AVDTP : Stack up\n"));
            btutil_ScheduleEvent(StackUp, NULL);
            break;

        case BTH_STACK_DISCONNECT:
            IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"AVDTP : Stack disconnect\n"));
            btutil_ScheduleEvent(StackDisconnect, NULL);
            break;

        default:
            IFDBG(DebugOut(DEBUG_ERROR, L"AVDTP : Unknown event, disconnect out of paranoia\n"));
            btutil_ScheduleEvent(StackDown, NULL);
            
    }

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_stack_event:: ERROR_SUCCESS\n"));
    
    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_config_ind
    
    This function is called when an L2CAP config request is received from the
    peer.  We need to send a config response.
    
    Parameters:
        pUserContext:       AVDTP context
        id:                 
        cid:                Channel Id
        usOutMTU:           MTU for sends
        usInFlushTO:        Flush t/o
        pInFlow:            Flow spec
        cOptNum:            Number of options
        ppExtendedOptions:  Extended options
------------------------------------------------------------------------------*/
static int avdtp_config_ind(
                void* pUserContext, 
                unsigned char id, 
                unsigned short cid, 
                unsigned short usOutMTU, 
                unsigned short usInFlushTO, 
                struct btFLOWSPEC *pInFlow, 
                int cOptNum, 
                struct btCONFIGEXTENSION** ppExtendedOptions)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_config_ind 0x%08X id:%d cid:%d usOutMTU:%d usInFlushTO:%d cOptNum:%d\n", 
        pUserContext, id, cid, usOutMTU, usInFlushTO, cOptNum));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_config_ind : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    // Find the session
    Session* pSess = FindSession(cid);

    // Send a response
    if (pSess) {
        HANDLE h = gpAVDTP->hL2CAP;
        L2CA_ConfigResponse_In pCallback = gpAVDTP->l2cap_if.l2ca_ConfigResponse_In;
            
        if ((usOutMTU && (usOutMTU < BTHAVDTP_MIN_FRAME)) || (pInFlow && (pInFlow->service_type != 0x01))) {
            // Send negative response
            
            IFDBG(DebugOut (DEBUG_WARN, L"[AVDTP] avdtp_config_ind :: not supported parameters!\n"));

            struct btFLOWSPEC flowspec;

            if (pInFlow && (pInFlow->service_type != 0x01)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[AVDTP] avdtp_config_ind :: not supported flowspec!\n"));
                flowspec = *pInFlow;
                pInFlow = &flowspec;
                pInFlow->service_type = 0x01;
            } else
                pInFlow = NULL;

            if (usOutMTU && (usOutMTU < BTHAVDTP_MIN_FRAME)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[AVDTP] avdtp_config_ind :: not supported MTU (%d)!\n", usOutMTU));
                usOutMTU = BTHAVDTP_MIN_FRAME;
            } else
                usOutMTU = 0;

            avdtp_addref();
            avdtp_unlock();        

            IFDBG(DebugOut (DEBUG_AVDTP_CALLBACK, L"avdtp_config_ind : going into l2ca_ConfigResponse_In (rejecting the flow or MTU)\n"));

            int iRes = ERROR_INTERNAL_ERROR;

            __try {
                iRes = pCallback (h, NULL, id, cid, 1, usOutMTU, usInFlushTO, pInFlow, 0, NULL);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] avdtp_config_ind Exception in l2ca_ConfigResponse_In\n"));
            }
            
            avdtp_lock();
            avdtp_delref();
            
            IFDBG(DebugOut (DEBUG_AVDTP_CALLBACK, L"avdtp_config_ind : coming out of l2ca_ConfigResponse_In result=%d\n", iRes));
        } else {
            // Send positive response
            
            avdtp_addref();
            avdtp_unlock();

            IFDBG(DebugOut (DEBUG_AVDTP_CALLBACK, L"avdtp_config_ind : going into l2ca_ConfigResponse_In\n"));

            int iRes = ERROR_INTERNAL_ERROR;

            __try {
                iRes = pCallback (h, NULL, id, cid, 0, 0, usInFlushTO, NULL, 0, NULL);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] avdtp_config_ind Exception in l2ca_ConfigResponse_In\n"));
            }

            avdtp_lock();
            avdtp_delref();

            IFDBG(DebugOut (DEBUG_AVDTP_CALLBACK, L"avdtp_config_ind : coming out of l2ca_ConfigResponse_In result=%d\n", iRes));

            if (gpAVDTP->state.IsConnected() && (iRes == ERROR_SUCCESS)) {
                pSess->usOutMTU = usOutMTU ? usOutMTU : L2CAP_MTU;
                pSess->fStage |= CONFIG_IND_DONE;
                if (pSess->fStage == UP) {
                    SessionIsUp(pSess);
                }
            }
        }
    } else {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_config_ind:: Unknown CID\n"));
    }

    avdtp_unlock();    

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_config_ind:: ERROR_SUCCESS\n"));

    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_connect_ind
    
    This function is called when a peer connects to us on the A2DP PSM.  We
    must authenticate/encrypt the link and respond to the connection request.
    
    Parameters:
        pUserContext:   AVDTP context
        pba:            Address of the peer
        cid:            Channel id
        id:             
        psm:            Protocol multiplexer for AVDTP
------------------------------------------------------------------------------*/
static int avdtp_connect_ind(
                void* pUserContext, 
                BD_ADDR* pba, 
                unsigned short cid, 
                unsigned char id, 
                unsigned short psm)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_connect_ind 0x%08X bd:%04x%08x cid:%d id:%d psm:%d\n", 
        pUserContext, pba->NAP, pba->SAP, cid, id, psm));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_connect_ind : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    BYTE bChannelType = AVDTP_CHANNEL_SIGNALING;
    if (FindSession(pba, AVDTP_CHANNEL_SIGNALING)) {
        // We found an existing session so this must be the media session
        bChannelType = AVDTP_CHANNEL_MEDIA;
    }

    // We check pSession is valid in AuthThread.  AuthThread also responds to the 
    // connect request.
    Session* pSession = NewSession(pba, CONNECTED, cid, bChannelType, TRUE, id);
    if (pSession)
    {
        btutil_ScheduleEvent(AuthThread, pSession->hContext, 0);
    }
    
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_connect_ind:: ERROR_SUCCESS\n", 
        pUserContext, pba->NAP, pba->SAP, cid, id, psm));

    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_disconnect_ind
    
    This function is called when the peer disconnects the L2CAP channel.
    
    Parameters:
        pUserContext:   AVDTP context
        cid:            Channel Id
        iErr:           Reason for disconnect
------------------------------------------------------------------------------*/
static int avdtp_disconnect_ind(
                void* pUserContext, 
                unsigned short cid, 
                int iErr)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_disconnect_ind 0x%08X cid:%d iErr:%d\n", 
        pUserContext, cid, iErr));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_disconnect_ind : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    Session* pSession = FindSession(cid);
    if (pSession) {
        pSession->cid = INVALID_CID;

        // Close any streams associated with the session.  If no streams are found, just close
        // the session.
        if (ERROR_NOT_FOUND == CloseStreamsBySession(pSession->hContext, iErr)) {
            SVSUTIL_ASSERT(pSession->bChannelType != AVDTP_CHANNEL_MEDIA); // media channel must have associated stream
            CloseSession(pSession, iErr);
        }
    }
    
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_disconnect_ind:: ERROR_SUCCESS\n"));

    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_data_up_ind
    
    This function is called when the peer sends us data on an AVDTP channel.
    
    Parameters:
        pUserContext:   AVDTP context 
        cid:            Channel Id 
        pBuffer:        Buffer received 
------------------------------------------------------------------------------*/
static int avdtp_data_up_ind(
                void* pUserContext, 
                unsigned short cid, 
                BD_BUFFER* pBuffer)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_data_up_ind 0x%08X cid:%d pBuffer:0x%08X len=%d\n",
        pUserContext, cid, pBuffer, BufferTotal(pBuffer)));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_data_up_ind : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

#ifdef DEBUG
    gpAVDTP->dwHciThreadId = GetCurrentThreadId();
#endif

    int iRes = ERROR_NOT_FOUND;

    Session* pSess = FindSession(cid);

    if (pSess) {
        if (pSess->bChannelType == AVDTP_CHANNEL_SIGNALING) {
            iRes = gpAVDTP->signal.ProcessPacket(pSess, pBuffer);
        } else if (pSess->bChannelType == AVDTP_CHANNEL_MEDIA) {
            iRes = gpAVDTP->media.ProcessPacket(pSess, pBuffer);
        } else {
            // We only support signaling & media channels
            SVSUTIL_ASSERT(0);
            iRes = ERROR_INTERNAL_ERROR;
        }

        if (iRes != ERROR_SUCCESS) {
            IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_data_up_ind : Error processing packet, closing session. result=%d\n", iRes));
            if (ERROR_NOT_FOUND == CloseStreamsBySession(pSess->hContext, iRes)) {
                CloseSession(pSess, iRes);
            }
        }
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_data_up_ind:: iRes:%d\n", iRes));

    return iRes;
}


//
// Lower layer callbacks
//


/*------------------------------------------------------------------------------
    avdtp_call_aborted
    
    This function is called when a call is aborted.
    
    Parameters:
        pCallContext:   Call context
        iError:         Reason for the call being aborted
------------------------------------------------------------------------------*/
static int avdtp_call_aborted(
                void* pCallContext, 
                int iError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_call_aborted 0x%08X iError:%d\n",
        pCallContext, iError));

    if (! avdtp_lock_if_running()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_call_aborted : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Task* pTask = VerifyCall((HANDLE)pCallContext);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_call_aborted : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    // TODO: If this is the initial connect request we should set cid=INVALID_CID in order to not send disconnect request in this case

    Session* pSession = NULL;

    // Find the stream or session associated with this call
    if ((pTask->bSignalId == AVDTP_SIGNAL_DISCOVER) ||
        (pTask->bSignalId == AVDTP_CALL_LINK_SETUP) ||
        (pTask->bSignalId == AVDTP_CALL_LINK_CONFIG)) {
        pSession = VerifySession((HANDLE)pTask->hSession);
        if (pSession) {
            if (ERROR_NOT_FOUND == CloseStreamsBySession(pSession->hContext, iError)) {
                CloseSession(pSession, iError);
            }
        }
    } else {
        Stream* pStream = VerifyStream(pTask->hSession);
        if (pStream) {
            CloseStream(pStream, TRUE, iError);
        } else {
            SVSUTIL_ASSERT(0);
        }
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_call_aborted:: ERROR_SUCCESS\n"));

    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_config_req_out
    
    This function is called when we receive a config response from the peer.
    
    Parameters:
        pCallContext:       Call context 
        usResult:           Config result 
        usInMTU:            MTU for receives 
        usOutFlushTO:       Flush t/o 
        pOutFlow:           Flow options 
        cOptNum:            Number of options 
        ppExtendedOptions:  ppExtendedOptions
------------------------------------------------------------------------------*/
static int avdtp_config_req_out(
                void* pCallContext, 
                unsigned short usResult, 
                unsigned short usInMTU, 
                unsigned short usOutFlushTO, 
                struct btFLOWSPEC *pOutFlow, 
                int cOptNum, 
                struct btCONFIGEXTENSION** ppExtendedOptions)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_config_req_out 0x%08X result:%d InMTU:%d OutFlushTO:%d cOptNum:%d\n",
        pCallContext, usResult, usInMTU, usOutFlushTO, cOptNum));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_config_req_out : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    Task* pCall = VerifyCall((HANDLE)pCallContext);
    if ((! pCall) || (pCall->bSignalId != AVDTP_CALL_LINK_CONFIG)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_config_req_out : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Session* pSess = VerifySession(pCall->hSession);
    if ((! pSess) || ((pSess->fStage & CONNECTED) != CONNECTED)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_connect_req_out : ERROR_NOT_FOUND\n"));
        DeleteCall(pCall);
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    DeleteCall(pCall);

    if (usResult == 0) {
        // The config response indicates success
        pSess->usInMTU = usInMTU ? usInMTU : L2CAP_MTU;
        pSess->fStage |= CONFIG_REQ_DONE;
        if (pSess->fStage == UP) {
            SessionIsUp(pSess);
        }
    } else {
        CloseSession(pSess, ERROR_PROTOCOL_UNREACHABLE);
    }
 
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_config_req_out:: ERROR_SUCCESS\n"));
    
    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_config_response_out
    
    This function is called when our config response is sent.  No need to
    implement this function.
    
------------------------------------------------------------------------------*/
static int avdtp_config_response_out(
                void* pCallContext, 
                unsigned short result)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_config_response_out 0x%08X result:%d\n",
        pCallContext, result));

    // No need to do anything here.

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_config_response_out:: ERROR_SUCCESS\n"));
    
    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_connect_req_out
    
    This function is called when we receive a connect response.
    
    Parameters:
        pCallContext:   Call context
        cid:            Channel Id
        usResult:       Result of the connect request
        usStatus:       Status (see L2CAP spec)
------------------------------------------------------------------------------*/
static int avdtp_connect_req_out(
                void* pCallContext, 
                unsigned short cid, 
                unsigned short usResult, 
                unsigned short usStatus)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_connect_req_out 0x%08X cid:%d result:%d status:%d\n",
        pCallContext, cid, usResult, usStatus));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_connect_req_out : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    Task* pCall = VerifyCall((HANDLE)pCallContext);
    if ((! pCall) || (pCall->bSignalId != AVDTP_CALL_LINK_SETUP)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_connect_req_out : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Session* pSess = VerifySession(pCall->hSession);
    if ((! pSess) || ((pSess->fStage & NONE) != NONE)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_connect_req_out : ERROR_NOT_FOUND\n"));
        DeleteCall(pCall);
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    DeleteCall(pCall);

    if (usResult == 0) {
        pSess->cid = cid;
        pSess->fStage = CONNECTED;
        pSess->fIncoming = FALSE;  // Set this just to be sure
        
        btutil_ScheduleEvent(AuthThread, pSess->hContext, 0);
    } else {
        int iErr = ERROR_INTERNAL_ERROR;
        
        if (usResult == 2)
            iErr = ERROR_UNKNOWN_PORT;
        else if (usResult == 3)
            iErr = ERROR_ACCESS_DENIED;
        else if (usResult == 4)
            iErr = ERROR_REMOTE_SESSION_LIMIT_EXCEEDED;

        CloseSession(pSess, iErr);
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_connect_req_out:: ERROR_SUCCESS\n"));
    
    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_connect_response_out
    
    This function is called when our response is sent.  This does not need to
    be implemented.
    
------------------------------------------------------------------------------*/
static int avdtp_connect_response_out(
                void* pCallContext, 
                unsigned short result)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_connect_response_out 0x%08X result:%d\n",
        pCallContext, result));

    // No need to do anything here

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_connect_response_out:: ERROR_SUCCESS\n"));
    
    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_data_down_out
    
    This function is called asynchronously when our send completes.  We don't
    need to implement this.

------------------------------------------------------------------------------*/
static int avdtp_data_down_out(
                void* pCallContext, 
                unsigned short result)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_data_down_out 0x%08X result:%d\n",
        pCallContext, result));

    // No need to do anything here.

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_data_down_out:: ERROR_SUCCESS\n"));
    
    return ERROR_SUCCESS;
}

/*------------------------------------------------------------------------------
    avdtp_disconnect_out
    
    This function is called whena disconnect response arrives.  We do not need
    to implement this function.
    
------------------------------------------------------------------------------*/
static int avdtp_disconnect_out(
                void* pCallContext, 
                unsigned short result)
{
    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"++avdtp_disconnect_out 0x%08X result:%d\n",
        pCallContext, result));

    // No need to do anything here.

    IFDBG(DebugOut(DEBUG_AVDTP_CALLBACK, L"--avdtp_disconnect_out:: ERROR_SUCCESS\n"));
    
    return ERROR_SUCCESS;
}


//
// AVDTP interface
// 



/*------------------------------------------------------------------------------
    avdtp_DiscoverReq_In
    
    This function is called by the upper layer to send a Discover request to
    the specified peer.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Handle to the owner's call context
        pba:            Address of the peer
------------------------------------------------------------------------------*/
static int avdtp_DiscoverReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BD_ADDR* pba)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_DiscoverReq_In 0x%08X call:0x%08X ba:%04x%08x\n",
        hDeviceContext, pCallContext, pba->NAP, pba->SAP));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_DiscoverReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_DiscoverReq_In : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Session* pSess = FindSession(pba, AVDTP_CHANNEL_SIGNALING);
    if (! pSess) {
        // No session found so let's start one
        pSess = StartSession(pba, AVDTP_CHANNEL_SIGNALING);
        if (! pSess) {
            IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_DiscoverReq_In : Failed to create AVDTP session\n"));
            avdtp_unlock();
            return ERROR_NOT_READY;
        }
    }

    ClearTimeout(pSess);
    ScheduleTimeout(pSess, AVDTP_DISCOVER_SESSION_TIMEOUT);
    
    Task* pTask = NewTask(pOwner->hContext, pSess->hContext, pCallContext, AVDTP_SIGNAL_DISCOVER);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_DiscoverReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    int iRes = ERROR_SUCCESS;

    // If the session is up we send the request, otherwise the task
    // is left pending and will be sent when the session eventually
    // comes up.
    if (pSess->fStage == UP) {
        pTask->bTransaction = pSess->bTransaction;
        iRes = gpAVDTP->signal.DiscoverRequest(pSess, (LPVOID)pTask->hCallContext, pba);
    }

    if ((iRes != ERROR_SUCCESS) && gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_DiscoverReq_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_DiscoverRsp_In
    
    This function is called by the upper layer to send a Discover response to
    the specified peer.  This function may not immediately send the results
    since it may need to wait for multiple owners to respond to the discover.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Handle to the owner's call context
        bTransaction:   Signaling transaction id
        pba:            Address of the peer
        pEPInfo:        Endpoint info
        cEndpoints:     Number of endpoints
        bError:         Error to send
------------------------------------------------------------------------------*/
static int avdtp_DiscoverRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                BD_ADDR* pba,
                PAVDTP_ENDPOINT_INFO pEPInfo,
                DWORD cEndpoints,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_DiscoverRsp_In 0x%08X call:0x%08X transaction:%d ba:%04x%08x error:%d\n",
        hDeviceContext, pCallContext, bTransaction, pba->NAP, pba->SAP, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_DiscoverRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    if (cEndpoints > 8) {
        // Limit the number of endpoints per extension
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_DiscoverRsp_In : Too many endpoints specified in response\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_DiscoverRsp_In : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Session* pSession = FindSession(pba, AVDTP_CHANNEL_SIGNALING);
    if (! pSession) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_DiscoverRsp_In : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    ClearTimeout(pSession);
    ScheduleTimeout(pSession, gpAVDTP->cIdleTimeoutSec);

    // Store the discover response in the transaction context.  Each extension will be
    // responding to this request so we must store each one.

    TransactionContext* ptc = FindTransactionContext(pba, bTransaction, AVDTP_SIGNAL_DISCOVER);
    if (! ptc) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_DiscoverRsp_In : FindTransactionContext failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    DiscoverResponse dr;
    memset(&dr, 0, sizeof(dr));
    
    memcpy(dr.EPInfo, pEPInfo, cEndpoints*sizeof(AVDTP_ENDPOINT_INFO));
    dr.cEndpoints = cEndpoints;
    dr.bError = bError;

    if (! ptc->drl.push_front(dr)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_DiscoverRsp_In : FindTransactionContext failed with ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    int iRes = ERROR_SUCCESS;

    if (ptc->drl.size() == gpAVDTP->listContexts.size()) {
        // We have received a response from each extension.  Time to send the response.
        
        IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"avdtp_DiscoverRsp_In : Sending the Discover Response\n"));

        AVDTP_ENDPOINT_INFO buffer[5];
        PAVDTP_ENDPOINT_INFO pTotalEPInfo = NULL;
        DWORD cTotalEndpoints = 0;
        BYTE bSendError = 0;

        // Determine the number of endpoints or if there is an error to return
        for (DiscoverResponseList::iterator it = ptc->drl.begin(), itEnd = ptc->drl.end(); it != itEnd; ++it) {
            DiscoverResponse* pdr = &(*it);

            if (pdr->bError) {
                // If we have an error, just send it immediately
                cTotalEndpoints = 0;
                bSendError = pdr->bError;
                break;
            }
            
            cTotalEndpoints += pdr->cEndpoints;
        }

        if (! bSendError) {
            // If no error, make sure we have a large enough buffer to return the
            // endpoints and use the stack buffer if small enough.
            
            if (cTotalEndpoints > ARRAYSIZE(buffer)) {
                // Need to allocate more memory
                pTotalEPInfo = new AVDTP_ENDPOINT_INFO[cTotalEndpoints];
                if (! pTotalEPInfo) {
                    IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_DiscoverRsp_In : ERROR_OUTOFMEMORY\n"));
                    avdtp_unlock();
                    return ERROR_OUTOFMEMORY;
                }
            } else {
                pTotalEPInfo = buffer;
            }

            DWORD cbEPInfo = 0;
            for (DiscoverResponseList::iterator it = ptc->drl.begin(), itEnd = ptc->drl.end(); it != itEnd; ++it) {
                DiscoverResponse* pdr = &(*it);
                memcpy(&pTotalEPInfo[cbEPInfo], pdr->EPInfo, pdr->cEndpoints*sizeof(AVDTP_ENDPOINT_INFO));
                cbEPInfo += pdr->cEndpoints;
            }
        }

        iRes = gpAVDTP->signal.DiscoverResponse(pSession, bTransaction, pTotalEPInfo, cTotalEndpoints, bSendError);

        DeleteTransactionContext(pba, ptc);
    }

    // On success, allocate memory for EP info in owner context and set it
    if (ERROR_SUCCESS == iRes) {
        if (pOwner->cEPInfo < cEndpoints) {
            delete[] pOwner->pEPInfo;
            pOwner->pEPInfo = NULL;
        }

        if (! pOwner->pEPInfo) {
            pOwner->pEPInfo = new AVDTP_ENDPOINT_INFO[cEndpoints];
            if (! pOwner->pEPInfo) {
                avdtp_unlock();
                return ERROR_OUTOFMEMORY;
            }
        }

        memcpy(pOwner->pEPInfo, pEPInfo, cEndpoints*sizeof(AVDTP_ENDPOINT_INFO));
        pOwner->cEPInfo = cEndpoints;
    } 
    
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_DiscoverRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_GetCapabilitiesReq_In
    
    This function is called by the upper layer to send a GetCapabilities
    request to the specified peer.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Handle to the owner's all context
        pba:            Address of the specified peer
        bAcpSEID:       SEID of the ACP
------------------------------------------------------------------------------*/
static int avdtp_GetCapabilitiesReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BD_ADDR* pba,
                BYTE bAcpSEID)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_GetCapabilitiesReq_In 0x%08X call:0x%08X ba:%04x%08x AcpSeid:%d\n",
        hDeviceContext, pCallContext, pba->NAP, pba->SAP, bAcpSEID));

    // Check SEID param is valid
    if (bAcpSEID > 0x3F) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_SetConfigurationReq_In : ACP SEID is invalid\n"));
        return ERROR_INVALID_PARAMETER;
    }

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_GetCapabilitiesReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetCapabilitiesReq_In : ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Session* pSess = FindSession(pba, AVDTP_CHANNEL_SIGNALING);
    if (! pSess) {
        // No session found so let's start one
        pSess = StartSession(pba, AVDTP_CHANNEL_SIGNALING);
        if (! pSess) {
            IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetCapabilitiesReq_In : Failed to create AVDTP session\n"));
            avdtp_unlock();
            return ERROR_NOT_READY;
        }
    }

    ClearTimeout(pSess);

    // Check for an existing stream with the remote SEID
    Stream* pStream = FindStream(bAcpSEID, TRUE);
    if (! pStream)
    {
        pStream = NewStream(0, bAcpSEID, pOwner->hContext, pSess->hContext);
    }    
    
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetCapabilitiesReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    ScheduleTimeout(pStream, gpAVDTP->cIdleTimeoutSec);
        
    Task* pTask = NewTask(pOwner->hContext, pStream->hContext, pCallContext, AVDTP_SIGNAL_GET_CAPABILITIES);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetCapabilitiesReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    pTask->bAcpSEID = bAcpSEID;

    int iRes = ERROR_SUCCESS;

    // If the session is up we send the request, otherwise the task
    // is left pending and will be sent when the session eventually
    // comes up.
    if (pSess->fStage == UP) {
        pTask->bTransaction = pSess->bTransaction;
        iRes = gpAVDTP->signal.GetCapabilitiesRequest(pSess, (LPVOID)pTask->hCallContext, bAcpSEID);
    }

    if ((iRes != ERROR_SUCCESS) && gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }
    
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_GetCapabilitiesReq_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_GetCapabilitiesRsp_In
    
    This function is called by the upper layer to send a GetCapabilities
    response to the specified peer.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Handle to the owner's call context
        bTransaction:   Signaling transaction id
        pba:            Address of the specified peer
        pCapabilityIE:  Capabilities of the local device
        cCapabilityIE:  Number of capabilities
        bError:         Error to send
------------------------------------------------------------------------------*/
static int avdtp_GetCapabilitiesRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                BD_ADDR* pba,
                PAVDTP_CAPABILITY_IE pCapabilityIE,
                DWORD cCapabilityIE,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_GetCapabilitiesRsp_In 0x%08X call:0x%08X transaction:%d ba:%04x%08x error:%d\n",
        hDeviceContext, pCallContext, bTransaction, pba->NAP, pba->SAP, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_GetCapabilitiesRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetCapabilitiesRsp_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    TransactionContext* ptc = FindTransactionContext(pba, bTransaction, AVDTP_SIGNAL_GET_CAPABILITIES);
    if (! ptc) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetCapabilitiesRsp_In : FindTransactionContext failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    BYTE bAcpSEID = ptc->u.GetCapabilities.bAcpSEID;
    
    DeleteTransactionContext(pba, ptc);
   
    Stream* pStream = FindStream(bAcpSEID, FALSE);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetCapabilitiesRsp_In : FindStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetCapabilitiesRsp_In : VerifySession failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    ClearTimeout(pSess);

    if (bError) {
        // On error, we need to clean up the stream
        DeleteStream(pStream);
    }

    int iRes = gpAVDTP->signal.GetCapabilitiesResponse(pSess, bTransaction, pCapabilityIE, cCapabilityIE, bError);

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_GetCapabilitiesRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_SetConfigurationReq_In
    
    This function is called by the upper layer to send a SetConfiguration
    command to the specified peer.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Handle to the owner's call context
        pba:            Address of the specified peer
        bAcpSEID:       SEID of the ACP
        bIntSEID:       SEID of the INT
        pCapabilityIE:  Configuration to set
        cCapabilityIE:  Number of configurations
------------------------------------------------------------------------------*/
static int avdtp_SetConfigurationReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BD_ADDR* pba,
                BYTE bAcpSEID,
                BYTE bIntSEID,
                PAVDTP_CAPABILITY_IE pCapabilityIE,
                DWORD cCapabilityIE)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_SetConfigurationReq_In 0x%08X call:0x%08X AcpSeid:%d IntSeid:%d\n",
        hDeviceContext, pCallContext, bAcpSEID, bIntSEID));

    // Check SEID params are valid
    if ((bAcpSEID > 0x3F) || (bIntSEID > 0x3F)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_SetConfigurationReq_In : ACP or INT SEID is invalid\n"));
        return ERROR_INVALID_PARAMETER;
    }

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {           
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_SetConfigurationReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SetConfigurationReq_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = FindStream(bAcpSEID, TRUE);
    if (! pStream || pStream->state.IsConfigured()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SetConfigurationReq_In : FindStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }    

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    pStream->bLocalSeid = bIntSEID;

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SetConfigurationReq_In : VerifySession failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Task* pTask = NewTask(pOwner->hContext, pStream->hContext, pCallContext, AVDTP_SIGNAL_SET_CONFIGURATION);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetCapabilitiesReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    pTask->bTransaction = pSess->bTransaction;

    ScheduleTimeout(pTask, gpAVDTP->cCmdTimeoutSec);

    int iRes = gpAVDTP->signal.SetConfigurationRequest(pSess, (LPVOID)pTask->hCallContext, bAcpSEID, bIntSEID, pCapabilityIE, cCapabilityIE);

    // Since we are INT here we want to store the IntSEID for scenarios where
    // the ACP will send us a command (such as CLOSE) by using our SEID
    if (ERROR_SUCCESS == iRes) {
        if (pOwner->cEPInfo < 1) {
            delete[] pOwner->pEPInfo;
            pOwner->pEPInfo = NULL;
        }

        if (! pOwner->pEPInfo) {
            pOwner->pEPInfo = new AVDTP_ENDPOINT_INFO[1];
            if (! pOwner->pEPInfo) {
                avdtp_unlock();
                return ERROR_OUTOFMEMORY;
            }
        }

        memset(&pOwner->pEPInfo[0], 0, sizeof(AVDTP_ENDPOINT_INFO));
        pOwner->pEPInfo[0].dwSize = sizeof(AVDTP_ENDPOINT_INFO);
        pOwner->pEPInfo[0].bSEID = bIntSEID;
        
        pOwner->cEPInfo = 1;
    } else if (gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }
    
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_SetConfigurationReq_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_SetConfigurationRsp_In
    
    This function is called by the upper layer to send a SetConfiguration
    response to the specified peer.
    
    Parameters:
        hDeviceContext:     Handle to the owner context
        pCallContext:       Handle to the owner's call context
        bTransaction:       Signaling transaction id
        hStream:            Handle to the stream
        bServiceCategory:   Service category to cause error
        bError:             Error to send
------------------------------------------------------------------------------*/
static int avdtp_SetConfigurationRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                HANDLE hStream,
                BYTE bServiceCategory,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_SetConfigurationRsp_In 0x%08X call:0x%08X transaction:%d stream:0x%08X sc:%d error:%d\n",
        hDeviceContext, pCallContext, bTransaction, hStream, bServiceCategory, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_SetConfigurationRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SetConfigurationRsp_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SetConfigurationRsp_In : VerifyStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSession = VerifySession(pStream->hSessSignal);
    if (! pSession) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SetConfigurationRsp_In : VerifySession failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    if (! bError) {
        // Stream transitions to a configured state if no error
        pStream->state.Configured();
        ClearTimeout(pStream);
    }

    int iRes = gpAVDTP->signal.SetConfigurationResponse(pSession, bTransaction, bServiceCategory, bError);

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_SetConfigurationRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_GetConfigurationReq_In
    
    This function is called by the upper layer to send a GetConfiguration
    request to the specified peer.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Handle to the owner's call context
        hStream:        Handle to the stream
------------------------------------------------------------------------------*/
static int avdtp_GetConfigurationReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                HANDLE hStream)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_GetConfigurationReq_In 0x%08X call:0x%08X stream:0x%08X\n",
        hDeviceContext, pCallContext, hStream));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {           
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_GetConfigurationReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetConfigurationReq_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetConfigurationReq_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetConfigurationReq_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    Task* pTask = NewTask(pOwner->hContext, pStream->hContext, pCallContext, AVDTP_SIGNAL_GET_CONFIGURATION);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetConfigurationReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    pTask->bTransaction = pSess->bTransaction;

    ScheduleTimeout(pTask, gpAVDTP->cCmdTimeoutSec);

    int iRes = gpAVDTP->signal.GetConfigurationRequest(pStream, pSess, (LPVOID)pTask->hCallContext);

    if ((iRes != ERROR_SUCCESS) && gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_GetConfigurationReq_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_GetConfigurationRsp_In
    
    This function is called by the upper layer to send a GetConfiguration
    response to the specified peer.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        bTransaction:   Signaling transaction id
        hStream:        Handle to the stream
        pCapabilityIE:  Configuration response data
        cCapabilityIE:  Number of configurations 
        bError:         Error to send
------------------------------------------------------------------------------*/
static int avdtp_GetConfigurationRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                HANDLE hStream,
                PAVDTP_CAPABILITY_IE pCapabilityIE,
                DWORD cCapabilityIE,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_GetConfigurationRsp_In 0x%08X call:0x%08X transaction:%d stream:0x%08X error:%d\n",
        hDeviceContext, pCallContext, bTransaction, hStream, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_GetConfigurationRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetConfigurationRsp_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetConfigurationRsp_In : VerifyStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetConfigurationRsp_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    int iRes = gpAVDTP->signal.GetConfigurationResponse(pSess, bTransaction, pCapabilityIE, cCapabilityIE, bError);

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_GetConfigurationRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_OpenReq_In
    
    This function is called by the upper layer to send an Open request on the
    specified stream.    
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        hStream:        Handle to the stream
------------------------------------------------------------------------------*/
static int avdtp_OpenReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                HANDLE hStream)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_OpenReq_In 0x%08X call:0x%08X stream:0x%08X\n",
        hDeviceContext, pCallContext, hStream));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {           
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_OpenReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_OpenReq_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream || ! pStream->state.IsConfigured()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_OpenReq_In : ERROR_NOT_READY\n"));
        avdtp_unlock();
        return ERROR_NOT_READY;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    pStream->pOpenCallContext = NULL;

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_OpenReq_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    Task* pTask = NewTask(pOwner->hContext, pStream->hContext, pCallContext, AVDTP_SIGNAL_OPEN);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_OpenReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    pTask->bTransaction = pSess->bTransaction;

    ScheduleTimeout(pTask, gpAVDTP->cCmdTimeoutSec);

    int iRes = gpAVDTP->signal.OpenStreamRequest(pStream, pSess, (LPVOID)pTask->hCallContext);

    if ((iRes != ERROR_SUCCESS) && gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_OpenReq_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_OpenRsp_In
    
    This function is called by the upper layer to send an Open response on the
    specified stream.    
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        bTransaction:   Signaling transaction id
        hStream:        Handle to the stream
        bError:         Error to send
------------------------------------------------------------------------------*/
static int avdtp_OpenRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                HANDLE hStream,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_OpenRsp_In 0x%08X call:0x%08X transaction:%d stream:0x%08X error:%d\n",
        hDeviceContext, pCallContext, bTransaction, hStream, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_OpenRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_OpenRsp_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_OpenRsp_In : VerifyStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_OpenRsp_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    int iRes = gpAVDTP->signal.OpenStreamResponse(pSess, bTransaction, bError);

    // Update stream state
    if (iRes == ERROR_SUCCESS) {
        pStream = VerifyStream(hStream);
        if (pStream && !bError) {
            pStream->state.Open();
            NotifyAVDTPEvent(pStream);
        }
    } 

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_OpenRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_CloseReq_In
    
    This function is called by the upper layer to send a Close request on the
    specified stream.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        hStream:        Handle to the stream
------------------------------------------------------------------------------*/
static int avdtp_CloseReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                HANDLE hStream)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_CloseReq_In 0x%08X call:0x%08X stream:0x%08X\n",
        hDeviceContext, pCallContext, hStream));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {           
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_CloseReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_CloseReq_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream || ! pStream->state.IsOpen()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_CloseReq_In : ERROR_NOT_READY\n"));
        avdtp_unlock();
        return ERROR_NOT_READY;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_CloseReq_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    Task* pTask = NewTask(pOwner->hContext, pStream->hContext, pCallContext, AVDTP_SIGNAL_CLOSE);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_CloseReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    pTask->bTransaction = pSess->bTransaction;

    ScheduleTimeout(pTask, gpAVDTP->cCmdTimeoutSec);

    int iRes = gpAVDTP->signal.CloseStreamRequest(pStream, pSess, (LPVOID)pTask->hCallContext);

    if ((iRes != ERROR_SUCCESS) && gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_CloseReq_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_CloseRsp_In
    
    This function is called by the upper layer to send a Close response on the
    specified stream.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        bTransaction:   Signaling transaction id
        hStream:        Handle to the stream
        bError:         Error to send
------------------------------------------------------------------------------*/
static int avdtp_CloseRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                HANDLE hStream,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_CloseRsp_In 0x%08X call:0x%08X transaction:%d stream:0x%08X error:%d\n",
        hDeviceContext, pCallContext, bTransaction, hStream, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_CloseRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_CloseRsp_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_CloseRsp_In : VerifyStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_CloseRsp_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    int iRes = gpAVDTP->signal.CloseStreamResponse(pSess, bTransaction, bError);

    // Update stream state
    if (iRes == ERROR_SUCCESS) {
        pStream = VerifyStream(hStream);
        if (pStream && !bError) {
            pStream->state.Closing();
        }
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_CloseRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_StartReq_In
    
    This function is called by the upper layer to send a Start request on the
    specified streams.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        pStreamHandles: Array of Stream handles
        cStreamHandles: Number of Stram handles
------------------------------------------------------------------------------*/
static int avdtp_StartReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                HANDLE* pStreamHandles,
                DWORD cStreamHandles)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_StartReq_In 0x%08X call:0x%08X\n",
        hDeviceContext, pCallContext));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {           
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_StartReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_StartReq_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    // Loop through each stream and verify its state
    for (DWORD i = 0; i < cStreamHandles; i++) {
        Stream* pStream = VerifyStream(pStreamHandles[i]);
        if (! pStream || ! pStream->state.IsInState(STREAM_STATE_OPEN)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_StartReq_In : Handle %d is not ready - ERROR_NOT_READY\n", i));
            avdtp_unlock();
            return ERROR_NOT_READY;
        }

        SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);
    }

    // This API will only support one stream handle for now.  To handle multiple streams we would need
    // to group each stream by the session is used and send multiple requests and then reassemble the
    // response.  There is no need to do this since the streams can be started individually.  We will
    // leave the API as is in case we want to extend it in the future.
    if (cStreamHandles != 1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_StartReq_In : This API only supports starting one stream handle at a time\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }
    
    Stream* pStream = VerifyStream(pStreamHandles[0]);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_StartReq_In : ERROR_NOT_FOUND\n", i));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_GetConfigurationReq_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    // Use the first stream handle for the task
    Task* pTask = NewTask(pOwner->hContext, pStreamHandles[0], pCallContext, AVDTP_SIGNAL_START);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_StartReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    pTask->bTransaction = pSess->bTransaction;

    ScheduleTimeout(pTask, gpAVDTP->cCmdTimeoutSec);
    
    int iRes = gpAVDTP->signal.StartStreamRequest(pSess, (LPVOID)pTask->hCallContext, &pStream, cStreamHandles);

    if ((iRes != ERROR_SUCCESS) && gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_StartReq_In:: result=%d\n", iRes));
    
    return iRes;
}
                
/*------------------------------------------------------------------------------
    avdtp_StartRsp_In
    
    This function is called by the upper layer to send a Start response on the
    specified streams.  This function may not send the response out immediately
    since it might need to wait for responses to come from multiple extensions.
    
    Parameters:
        hDeviceContext:         Handle to owner context
        pCallContext:           Pointer to owner's call context
        bTransaction:           Signaling transaction id
        hFirstFailingStream:    Handle to the first failing stream to start
        bError:                 Error to send
------------------------------------------------------------------------------*/
static int avdtp_StartRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                HANDLE hFirstFailingStream,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_StartRsp_In 0x%08X call:0x%08X transaction:%d stream:0x%08X error:%d\n",
        hDeviceContext, pCallContext, bTransaction, hFirstFailingStream, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_StartRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_StartRsp_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    TransactionContext tc;
    
    Stream* pStream = FindStreamByTransactionContext(bTransaction, AVDTP_SIGNAL_START, &tc);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_StartRsp_In : FindStreamByTransactionContext failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    DeleteTransactionContext(pStream, &tc);

    HANDLE hStream = pStream->hContext;

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_StartRsp_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    Stream* pFailStream = NULL;
    if (bError && hFirstFailingStream) {
        pFailStream = VerifyStream(hFirstFailingStream);        
    }

    int iRes = gpAVDTP->signal.StartStreamResponse(pSess, bTransaction, pFailStream, bError);

    // Update stream state
    if (iRes == ERROR_SUCCESS) {
        pStream = VerifyStream(hStream);
        if (pStream && !bError) {
            pStream->state.Streaming();
            EnsureActiveMode(pStream, TRUE);
            NotifyAVDTPEvent(pStream);
        }
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_StartRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_SuspendReq_In
    
    This function is called by the upper layer to send a Suspend request on the
    specified stream.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        pStreamHandles: Array of stream handles
        cStreamHandles: Number of stream handles
------------------------------------------------------------------------------*/
static int avdtp_SuspendReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                HANDLE* pStreamHandles,
                DWORD cStreamHandles)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_SuspendReq_In 0x%08X call:0x%08X\n",
        hDeviceContext, pCallContext));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {           
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_SuspendReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SuspendReq_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    // Loop through each stream and verify its state
    for (DWORD i = 0; i < cStreamHandles; i++) {
        Stream* pStream = VerifyStream(pStreamHandles[i]);
        if (! pStream || ! pStream->state.IsInState(STREAM_STATE_STREAMING)) {
            IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SuspendReq_In : Handle %d is not ready - ERROR_NOT_READY\n", i));
            avdtp_unlock();
            return ERROR_NOT_READY;
        }

        SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);
    }

    // This API will only support one stream handle for now.  To handle multiple streams we would need
    // to group each stream by the session is used and send multiple requests and then reassemble the
    // response.  There is no need to do this since the streams can be started individually.  We will
    // leave the API as is in case we want to extend it in the future.
    if (cStreamHandles != 1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SuspendReq_In : This API only supports suspending one stream handle at a time\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }
    
    Stream* pStream = VerifyStream(pStreamHandles[0]);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SuspendReq_In : ERROR_NOT_FOUND\n", i));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SuspendReq_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    // Use the first stream handle for the task
    Task* pTask = NewTask(pOwner->hContext, pStreamHandles[0], pCallContext, AVDTP_SIGNAL_SUSPEND);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SuspendReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    pTask->bTransaction = pSess->bTransaction;

    ScheduleTimeout(pTask, gpAVDTP->cCmdTimeoutSec);
    
    int iRes = gpAVDTP->signal.SuspendRequest(pSess, (LPVOID)pTask->hCallContext, &pStream, cStreamHandles);

    if ((iRes != ERROR_SUCCESS) && gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_SuspendReq_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_SuspendRsp_In
    
    This function is called by the upper layer to send a Suspend response on
    the specified streams.
    
    Parameters:
        hDeviceContext:         Handle to the owner context
        pCallContext:           Pointer to the owner's call context
        bTransaction:           Signaling transaction id
        hFirstStream:           Handle to the first stream that was suspended
        hFirstFailingStream:    Handle to the first failing stream
        bError:                 Error to send
------------------------------------------------------------------------------*/
static int avdtp_SuspendRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                HANDLE hFirstStream,
                HANDLE hFirstFailingStream,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_SuspendRsp_In 0x%08X call:0x%08X transaction:%d stream:0x%08X failstream:0x%08X error:%d\n",
        hDeviceContext, pCallContext, bTransaction, hFirstStream, hFirstFailingStream, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_SuspendRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SuspendRsp_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    TransactionContext tc;
    
    Stream* pStream = FindStreamByTransactionContext(bTransaction, AVDTP_SIGNAL_SUSPEND, &tc);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SuspendRsp_In : FindStreamByTransactionContext failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    DeleteTransactionContext(pStream, &tc);

    HANDLE hStream = pStream->hContext;

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_SuspendRsp_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    Stream* pFailStream = NULL;
    if (bError && hFirstFailingStream) {
        pFailStream = VerifyStream(hFirstFailingStream);        
    }

    int iRes = gpAVDTP->signal.SuspendResponse(pSess, bTransaction, pFailStream, bError);

    // Update stream state
    if (iRes == ERROR_SUCCESS) {
        pStream = VerifyStream(hStream);
        if (pStream && !bError) {
            pStream->state.Open();
            EnsureActiveMode(pStream, FALSE);
            NotifyAVDTPEvent(pStream);
        }
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_SuspendRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_ReconfigureReq_In
    
    This function is called by the upper layered to send a Reconfigure request
    on the specified stream.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        hStream:        Handle to the stream
        pCapabilityIE:  Configuration to send
        cCapabilityIE:  Number of configurations
------------------------------------------------------------------------------*/
static int avdtp_ReconfigureReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                HANDLE hStream,
                PAVDTP_CAPABILITY_IE pCapabilityIE,
                DWORD cCapabilityIE)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_ReconfigureReq_In 0x%08X call:0x%08X hStream:0x%08X\n",
        hDeviceContext, pCallContext, hStream));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {           
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_ReconfigureReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_ReconfigureReq_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_ReconfigureReq_In : VerifyStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }    

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_ReconfigureReq_In : VerifySession failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Task* pTask = NewTask(pOwner->hContext, pStream->hContext, pCallContext, AVDTP_SIGNAL_RECONFIGURE);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_ReconfigureReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    pTask->bTransaction = pSess->bTransaction;

    ScheduleTimeout(pTask, gpAVDTP->cCmdTimeoutSec);

    int iRes = gpAVDTP->signal.ReconfigureRequest(pStream, pSess, (LPVOID)pTask->hCallContext, pCapabilityIE, cCapabilityIE);

    if ((iRes != ERROR_SUCCESS) && gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }
    
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_ReconfigureReq_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_ReconfigureRsp_In
    
    This function is called by the upper layer to send a Reconfigure response
    on the specified stream.
    
    Parameters:
        hDeviceContext:     Handle to the owner context
        pCallContext:       Pointer to the owner's call context
        bTransaction:       Signaling transaction id
        hStream:            Handle to the stream
        bServiceCategory:   Service cagetory that caused the error
        bError:             Error to send
------------------------------------------------------------------------------*/
static int avdtp_ReconfigureRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                HANDLE hStream,
                BYTE bServiceCategory,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_ReconfigureRsp_In 0x%08X call:0x%08X transaction:%d stream:0x%08X sc:%d error:%d\n",
        hDeviceContext, pCallContext, bTransaction, hStream, bServiceCategory, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_ReconfigureRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_ReconfigureRsp_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_ReconfigureRsp_In : VerifyStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSession = VerifySession(pStream->hSessSignal);
    if (! pSession) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_ReconfigureRsp_In : VerifySession failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    int iRes = gpAVDTP->signal.ReconfigureResponse(pSession, bTransaction, bServiceCategory, bError);

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_ReconfigureRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

static int avdtp_SecurityControlReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                HANDLE hStream,
                USHORT cbSecurityControlData,
                PBYTE pSecurityControlData)
{
    IFDBG(DebugOut(DEBUG_WARN, L"[AVDTP] avdtp_SecurityControlReq_In:: ERROR_CALL_NOT_IMPLEMENTED\n"));    
    return ERROR_CALL_NOT_IMPLEMENTED;
}

static int avdtp_SecurityControlRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                HANDLE hStream,
                USHORT cbSecurityControlData,
                PBYTE pSecurityControlData,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_WARN, L"[AVDTP] avdtp_SecurityControlRsp_In:: ERROR_CALL_NOT_IMPLEMENTED\n"));    
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/*------------------------------------------------------------------------------
    avdtp_AbortReq_In
    
    This function is called by the upper layer to abort the specified stream.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        hStream:        Handle to the stream
------------------------------------------------------------------------------*/
static int avdtp_AbortReq_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                HANDLE hStream)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_AbortReq_In 0x%08X call:0x%08X stream:0x%08X\n",
        hDeviceContext, pCallContext, hStream));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {           
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_AbortReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_AbortReq_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_AbortReq_In : ERROR_NOT_READY\n"));
        avdtp_unlock();
        return ERROR_NOT_READY;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_AbortReq_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    Task* pTask = NewTask(pOwner->hContext, pStream->hContext, pCallContext, AVDTP_SIGNAL_ABORT);
    if (! pTask) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_AbortReq_In : ERROR_OUTOFMEMORY\n"));
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    pTask->bTransaction = pSess->bTransaction;

    ScheduleTimeout(pTask, gpAVDTP->cCmdTimeoutSec);

    int iRes = gpAVDTP->signal.AbortRequest(pStream, pSess, (LPVOID)pTask->hCallContext);

    if ((iRes != ERROR_SUCCESS) && gpAVDTP->state.IsRunning()) {
        DeleteCall(pTask);
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_AbortReq_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_AbortRsp_In
    
    This function is called by the upper layer to send an Abort response on the
    specified stream.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        pCallContext:   Pointer to the owner's call context
        bTransaction:   Signaling transaction id
        hStream:        Handle to the stream
        bError:         Non-zero to send an error
------------------------------------------------------------------------------*/
static int avdtp_AbortRsp_In(
                HANDLE hDeviceContext,
                void* pCallContext,
                BYTE bTransaction,
                HANDLE hStream,
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_AbortRsp_In 0x%08X call:0x%08X transaction:%d stream:0x%08X error:%d\n",
        hDeviceContext, pCallContext, bTransaction, hStream, bError));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_AbortRsp_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_AbortRsp_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_AbortRsp_In : VerifyStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT(pStream->hOwner == pOwner->hContext);

    Session* pSess = VerifySession(pStream->hSessSignal);
    if (! pSess) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_AbortRsp_In : ERROR_INVALID_PARAMETER\n"));
        avdtp_unlock();
        return ERROR_INVALID_PARAMETER;
    }

    int iRes = gpAVDTP->signal.AbortResponse(pSess, bTransaction, bError);

    // Update stream state
    if (iRes == ERROR_SUCCESS) {
        pStream = VerifyStream(hStream);
        if (pStream && !bError) {
            pStream->state.Aborting();
        }
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_AbortRsp_In:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_WriteReq_In
    
    This function is called by the upper layer to send an AVDTP packet over
    the media channel.
    
    Parameters:
        hDeviceContext: Handle to the owner context
        hStream:        Handle to the stream
        pBuffer:        Buffer to send
        dwTimeInfo:     Time info (from RTP spec)
        pPayloadType:   Payload type (from RTP spec)
        usMarker:       Marker bit (from RTP spec)
------------------------------------------------------------------------------*/
static int avdtp_WriteReq_In(
                HANDLE hDeviceContext,
                HANDLE hStream,
                BD_BUFFER* pBuffer,
                DWORD dwTimeInfo,
                BYTE pPayloadType,
                USHORT usMarker)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_WriteReq_In 0x%08X stream:0x%08X time:%d type:%d marker:%d size:%d\n",
        hDeviceContext, hStream, dwTimeInfo, pPayloadType, usMarker, BufferTotal(pBuffer)));

    // Take the lock if we are in a connected state
    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_WriteReq_In : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    AVDTP_CONTEXT *pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_WriteReq_In : VerifyOwner failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_WriteReq_In : VerifyStream failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    if (! pStream->state.IsStreaming()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_WriteReq_In : VerifyStream failed with ERROR_NOT_READY\n"));
        avdtp_unlock();
        return ERROR_NOT_READY;
    }

    Session* pSession = VerifySession(pStream->hSessMedia);
    if (! pSession) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_WriteReq_In : VerifySession failed with ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    if (! pStream->state.IsStreaming()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_WriteReq_In : We're not in a streaming state.\n"));
        avdtp_unlock();
        return ERROR_NOT_READY;
    }

    int iRes = gpAVDTP->media.WriteRequest(pStream, pSession, pBuffer, dwTimeInfo, pPayloadType, usMarker);

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_WriteReq_In:: result=%d\n", iRes));
    
    return iRes;
}

static int check_io_control_parms(
                int cInBuffer,
                char *pInBuffer,
                int cOutBuffer,
                char *pOutBuffer,
                int *pcDataReturned,
                char *pSpace,
                int cSpace) 
{
    --cSpace;

    __try {
        if (pcDataReturned)  {
            *pcDataReturned = 0;
            PREFAST_SUPPRESS(419, "We don't need to validate cOutBuffer because we are in a try/except for the purpose of validating these params");
            memset (pOutBuffer, 0, cOutBuffer);
        } else if (pOutBuffer || cOutBuffer)
            return FALSE;

        int i = 0;
        while (cInBuffer > 0) {
            pSpace[i = (i < cSpace) ? i + 1 : 0] = *pInBuffer++;
            --cInBuffer;
        }
    } __except(1) {
        return FALSE;
    }

    return TRUE;
}


static int avdtp_ioctl (
                HANDLE hDeviceContext,
                int fSelector,
                int cInBuffer,
                char* pInBuffer,
                int cOutBuffer,
                char* pOutBuffer,
                int* pcDataReturned) 
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++avdtp_ioctl 0x%08X selector:%d\n",
        hDeviceContext, fSelector));
    
    char chCheck;
    if (! check_io_control_parms (cInBuffer, pInBuffer, cOutBuffer, pOutBuffer, pcDataReturned, &chCheck, 1)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"avdtp_ioctl returns ERROR_INVALID_PARAMETER (exception)\n"));
        return ERROR_INVALID_PARAMETER;
    }

    if (! avdtp_lock_if_running()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_ioctl :: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    AVDTP_CONTEXT* pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_ioctl :: ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    int iRes = ERROR_INVALID_OPERATION;

    switch (fSelector) {
        case BTH_STACK_IOCTL_GET_CONNECTED:
            if ((cInBuffer == 0) && (cOutBuffer == 4)) {
                iRes = ERROR_SUCCESS;

                int iCount = gpAVDTP->state.IsConnected();

                pOutBuffer[0] = iCount & 0xff;
                pOutBuffer[1] = (iCount >> 8) & 0xff;
                pOutBuffer[2] = (iCount >> 16) & 0xff;
                pOutBuffer[3] = (iCount >> 24) & 0xff;
                *pcDataReturned = 4;
            } else {
                iRes = ERROR_INVALID_PARAMETER;
            }
            
            break;

        case BTH_AVDTP_IOCTL_SET_BIT_RATE:
            if (cInBuffer == sizeof(AVDTP_SET_BIT_RATE)) {
                PAVDTP_SET_BIT_RATE pSetBitRate = (PAVDTP_SET_BIT_RATE)pInBuffer;

                Stream* pStream = VerifyStream(pSetBitRate->hStream);
                if (pStream) {
                    pStream->dwBitsPerSec = pSetBitRate->dwBitRate;
                    pStream->cbCodecHeader = pSetBitRate->cbCodecHeader;
                    iRes = ERROR_SUCCESS;
                } else {
                    iRes = ERROR_NOT_FOUND;
                }                
            } else {
                iRes = ERROR_INVALID_PARAMETER;
            }
            
            break;

        case BTH_AVDTP_IOCTL_TERMINATE_IDLE_CONNECTIONS:
            iRes = CloseIdleSessions();            
            break;

        case BTH_AVDTP_IOCTL_GET_UNIQUE_SEID:
            if (cOutBuffer == 1) {
                if (gpAVDTP->bUniqueSEID == 0x40) {
                    // We are out of unique SEIDs
                    iRes = ERROR_ALREADY_ASSIGNED;
                } else {
                    pOutBuffer[0] = gpAVDTP->bUniqueSEID;
                    *pcDataReturned = 1;

                    gpAVDTP->bUniqueSEID++;

                    iRes = ERROR_SUCCESS;
                }                
            } else {
                iRes = ERROR_INVALID_PARAMETER;
            }

            break;

        case BTH_AVDTP_IOCTL_GET_MEDIA_PARAMS:
            if (cInBuffer == sizeof(AVDTP_GET_MEDIA_PARAMS)) {
                PAVDTP_GET_MEDIA_PARAMS pMediaParams = (PAVDTP_GET_MEDIA_PARAMS)pInBuffer;

                iRes = ERROR_NOT_FOUND;

                Stream* pStream = VerifyStream(pMediaParams->hStream);
                if (pStream) {
                    Session* pSession = VerifySession(pStream->hSessMedia);
                    if (pSession) {
                        pMediaParams->usMediaMTU = pSession->usOutMTU;
                        iRes = ERROR_SUCCESS;
                    }
                }
            } else {
                iRes = ERROR_INVALID_PARAMETER;
            }

            break;
    }

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--avdtp_ioctl:: result=%d\n", iRes));
    
    return iRes;
}

/*------------------------------------------------------------------------------
    avdtp_AbortCall
    
    This function is called by the upper layer to abort one of its async calls.
    
    Parameters:
        hDeviceContext: Handle to the owner context 
        pCallContext:   Pointer to the owner's call context 
------------------------------------------------------------------------------*/
int avdtp_AbortCall(
                HANDLE hDeviceContext, 
                void *pCallContext) 
{
    int iRes = ERROR_INVALID_PARAMETER;

    if (! avdtp_lock_if_connected()) {        
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_AbortCall : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }
    
    Task* pCall = FindCall(pCallContext);

    avdtp_unlock();
    
    if (pCall) {
        iRes = avdtp_call_aborted((LPVOID)pCall->hCallContext, ERROR_CANCELLED);
    }

    return iRes;    
}

//
// The following functions are called from the lower layer when
// AVDTP commands & responses are received.
//


/*------------------------------------------------------------------------------
    DiscoverRequestUp
    
    This function is called when we receive a Discover request from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
------------------------------------------------------------------------------*/
BYTE DiscoverRequestUp(
                Session* pSession, 
                BYTE bTransaction)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++DiscoverRequestUp: ba:%04x%08x bTransaction:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    BYTE bRet = AVDTP_ERROR_SUCCESS;

    {
        TransactionContext tc;
        tc.bTransaction = bTransaction;
        tc.bSignalId = AVDTP_SIGNAL_DISCOVER;
        
        if (! pSession->tcl.push_front(tc)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] DiscoverRequestUp : ERROR_OUTOFMEMORY\n"));
            return AVDTP_ERROR_UNDEFINED;
        }
    }

    for (ContextList::iterator it = gpAVDTP->listContexts.begin(), itEnd = gpAVDTP->listContexts.end(); it != itEnd; ++it) {
        AVDTP_CONTEXT* pOwner = *it;

        AVDTP_Discover_Ind pCallback = pOwner->ei.avdtp_Discover_Ind;
        void* pUserContext = pOwner->pUserContext;

        BD_ADDR ba = pSession->ba;

        pOwner->AddRef();
        avdtp_unlock();

        int iRes = ERROR_INTERNAL_ERROR;

        __try {
            iRes = pCallback(pUserContext, bTransaction, &ba);
        } __except(1) {
            IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] DiscoverRequestUp : exception in avdtp_Discover_Ind\n"));
        }

        avdtp_lock();
        pOwner->DelRef();

        if (ERROR_SUCCESS != iRes) {
            bRet = AVDTP_ERROR_UNDEFINED;
            break;
        }
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--DiscoverRequestUp: bRet=%d\n", bRet));
    
    return bRet;
}

/*------------------------------------------------------------------------------
    DiscoverResponseUp
    
    This function is called when we receive a Discover response from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        pAcpEPInfo:     Endpoint info
        cAcpEPInfo:     Number of endpoints
        bError:         Error sent from peer
------------------------------------------------------------------------------*/
int DiscoverResponseUp(
                Session* pSession, 
                BYTE bTransaction,
                PAVDTP_ENDPOINT_INFO pAcpEPInfo, 
                DWORD cAcpEPInfo, 
                BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++DiscoverResponseUp: ba:%04x%08x bTransaction:%d cAcpEPInfo:%d Error:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, cAcpEPInfo, bError));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_DISCOVER);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"DiscoverResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"DiscoverResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_Discover_Out pCallback = pOwner->c.avdtp_Discover_Out;
    void *pCallContext = pCall->pContext;
    
    DeleteCall(pCall);

    BD_ADDR ba = pSession->ba;

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, &ba, pAcpEPInfo, cAcpEPInfo, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] DiscoverResponseUp : exception in avdtp_Discover_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--DiscoverResponseUp: iRes=%d\n", iRes));

    return iRes;
}

/*------------------------------------------------------------------------------
    GetCapabilitiesRequestUp
    
    This function is called when we receive a GetCapabilities request from the
    peer on the specified session.    
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bAcpSEID:       SEID of the ACP
------------------------------------------------------------------------------*/
BYTE GetCapabilitiesRequestUp(
                Session* pSession,
                BYTE bTransaction,
                BYTE bAcpSEID)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++GetCapabilitiesRequestUp: ba:%04x%08x bTransaction:%d bAcpSEID:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bAcpSEID));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    BYTE bRet = AVDTP_ERROR_SUCCESS;

    AVDTP_CONTEXT* pOwner = FindOwner(bAcpSEID);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"GetCapabilitiesRequestUp : Could not find owner context for SEID %d - ERROR_NOT_FOUND\n", bAcpSEID));
        return AVDTP_ERROR_BAD_SEID;
    }

    SVSUTIL_ASSERT(VerifyOwner(pOwner->hContext));

    // Look for an existing stream with the same local SEID
    Stream* pStream = FindStream(bAcpSEID, FALSE);
    if (! pStream)
    {
        pStream = NewStream(bAcpSEID, 0, pOwner->hContext, pSession->hContext);
    }
    
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"GetCapabilitiesRequestUp : ERROR_OUTOFMEMORY\n"));
        return AVDTP_ERROR_UNDEFINED;
    }

    if (! pStream->state.IsConfigured()) {
        ScheduleTimeout(pStream, gpAVDTP->cIdleTimeoutSec);
    }

    {
        TransactionContext tc;
        tc.bTransaction = bTransaction;
        tc.bSignalId = AVDTP_SIGNAL_GET_CAPABILITIES;
        tc.u.GetCapabilities.bAcpSEID = bAcpSEID;
        
        if (! pSession->tcl.push_front(tc)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"GetCapabilitiesRequestUp : ERROR_OUTOFMEMORY\n"));
            return AVDTP_ERROR_UNDEFINED;
        }
    }

    AVDTP_GetCapabilities_Ind pCallback = pOwner->ei.avdtp_GetCapabilities_Ind;
    void* pUserContext = pOwner->pUserContext;

    BD_ADDR ba = pSession->ba;

    pOwner->AddRef();
    avdtp_unlock();

    int iRes = ERROR_INTERNAL_ERROR;
    
    __try {
        iRes = pCallback(pUserContext, bTransaction, &ba, bAcpSEID);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] GetCapabilitiesRequestUp : exception in avdtp_GetCapabilitiesRequest_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    if (ERROR_SUCCESS != iRes) {
        bRet = AVDTP_ERROR_UNDEFINED;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--GetCapabilitiesRequestUp: bRet=%d\n", bRet));

    return bRet;
}

/*------------------------------------------------------------------------------
    GetCapabilitiesResponseUp
    
    This function is called when we receive a GetCapabilities response from the
    peer device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        pCapabilityIE:  Capability information elements
        cCapabilityIE:  Number of capabilities
        bError:         Error sent to us
------------------------------------------------------------------------------*/
int GetCapabilitiesResponseUp(
            Session* pSession,
            BYTE bTransaction,
            PAVDTP_CAPABILITY_IE pCapabilityIE,
            DWORD cCapabilityIE,
            BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++GetCapabilitiesResponseUp: ba:%04x%08x transaction:%d cCapabilities:%d Error:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, cCapabilityIE, bError));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_GET_CAPABILITIES);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"GetCapabilitiesResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"GetCapabilitiesResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    // TODO: On error we should delete the stream immediately instead of timing it out

    AVDTP_GetCapabilities_Out pCallback = pOwner->c.avdtp_GetCapabilities_Out;
    void *pCallContext = pCall->pContext;
    
    DeleteCall(pCall);

    BD_ADDR ba = pSession->ba;

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, &ba, pCapabilityIE, cCapabilityIE, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] GetCapabilitiesResponseUp : exception in avdtp_GetCapabilities_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--GetCapabilitiesResponseUp: iRes=%d\n", iRes));

    return iRes;    
}

/*------------------------------------------------------------------------------
    SetConfigurationRequestUp
    
    This function is called when we receive a SetConfiguration request from the
    peer device on the specified session.
    
    Parameters:
        pSession:           Session object
        bTransaction:       Signaling transaction id
        bAcpSEID:           SEID of the ACP
        bIntSEID:           SEID of the INT
        pConfig:            Configuration elements
        cConfig:            Number of configurations
        pbFailedCategory:   Service category that caused the failure
------------------------------------------------------------------------------*/
BYTE SetConfigurationRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bAcpSEID,
            BYTE bIntSEID,
            PAVDTP_CAPABILITY_IE pConfig,
            DWORD cConfig,
            PBYTE pbFailedCategory)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++SetConfigurationRequestUp: ba:%04x%08x transaction:%d bAcpSEID:%d bIntSEID:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bAcpSEID, bIntSEID));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    BYTE bRet = AVDTP_ERROR_SUCCESS;
    *pbFailedCategory = 0;

    if (cConfig == 0) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SetConfigurationRequestUp : No configuration specified\n"));
        return AVDTP_ERROR_BAD_PAYLOAD_FORMAT;
    }
    
    AVDTP_CONTEXT* pOwner = FindOwner(bAcpSEID);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SetConfigurationRequestUp : Could not find owner context for SEID %d - AVDTP_ERROR_BAD_SEID\n", bAcpSEID));
        *pbFailedCategory = pConfig[0].bServiceCategory; // On error, pick first category per test spec
        return AVDTP_ERROR_BAD_SEID;
    }

    SVSUTIL_ASSERT(VerifyOwner(pOwner->hContext));

    Stream* pStream = FindStream(bAcpSEID, FALSE);
    if (! pStream) {
        pStream = NewStream(bAcpSEID, 0, pOwner->hContext, pSession->hContext);
        if (pStream)
        {
            ScheduleTimeout(pStream, gpAVDTP->cIdleTimeoutSec);
        }
    }

    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SetConfigurationRequestUp : AVDTP_ERROR_BAD_SEID\n"));
        *pbFailedCategory = pConfig[0].bServiceCategory; // On error, pick first category per test spec
        return AVDTP_ERROR_BAD_SEID;
    }

    if (pStream->state.IsConfigured()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SetConfigurationRequestUp : AVDTP_ERROR_SEP_IN_USE\n"));
        *pbFailedCategory = pConfig[0].bServiceCategory; // On error, pick first category per test spec
        return AVDTP_ERROR_SEP_IN_USE;
    }

    pStream->bRemoteSeid = bIntSEID;

    AVDTP_SetConfiguration_Ind pCallback = pOwner->ei.avdtp_SetConfiguration_Ind;
    void* pUserContext = pOwner->pUserContext;

    HANDLE hStream = pStream->hContext;
    BD_ADDR ba = pSession->ba;

    pOwner->AddRef();
    avdtp_unlock();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback(pUserContext, bTransaction, hStream, &ba, bAcpSEID, bIntSEID, pConfig, cConfig);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] SetConfigurationRequestUp : exception in avdtp_SetConfiguration_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    if (ERROR_SUCCESS != iRes) {
        bRet = AVDTP_ERROR_UNDEFINED;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--SetConfigurationRequestUp: bRet=%d\n", bRet));

    return bRet;    
}

/*------------------------------------------------------------------------------
    SetConfigurationResponseUp
    
    This function is called when we receive a SetConfiguration response from
    the peer device on the specified session.
    
    Parameters:
        pSession:           Session object
        bTransaction:       Signaling transaction id
        bServiceCategory:   Service category that caused the error
        bError:             Error sent to us
------------------------------------------------------------------------------*/
int SetConfigurationResponseUp(
            Session* pSession, 
            BYTE bTransaction, 
            BYTE bServiceCategory, 
            BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++SetConfigurationResponseUp: ba:%04x%08x transaction:%d ServiceCategory:%d Error:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bServiceCategory, bError));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_SET_CONFIGURATION);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"SetConfigurationResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
    
    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"SetConfigurationResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_SetConfiguration_Out pCallback = pOwner->c.avdtp_SetConfiguration_Out;
    void *pCallContext = pCall->pContext;
    HANDLE hStream = pCall->hSession;

    ClearTimeout(pCall);
    DeleteCall(pCall);

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"SetConfigurationResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    // TODO: On error, timeout the stream (or can I just leave the existing timeout?)

    if (! bError) {
        pStream->state.Configured();
        ClearTimeout(pStream);
    }

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, hStream, bServiceCategory, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] SetConfigurationResponseUp : exception in avdtp_SetConfiguration_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--SetConfigurationResponseUp: iRes=%d\n", iRes));

    return iRes;    
}

/*------------------------------------------------------------------------------
    GetConfigurationRequestUp
    
    This function is called when we receive a GetConfiguration request from the
    peer device on the specified stream.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bAcpSEID:       SEID of the ACP
------------------------------------------------------------------------------*/
BYTE GetConfigurationRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bAcpSEID) 
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++GetConfigurationRequestUp: ba:%04x%08x transaction:%d bAcpSEID:%d bIntSEID:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bAcpSEID, bAcpSEID));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    BYTE bRet = AVDTP_ERROR_SUCCESS;
    
    AVDTP_CONTEXT* pOwner = FindOwner(bAcpSEID);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] GetConfigurationRequestUp : Could not find owner context for SEID %d - ERROR_NOT_FOUND\n", bAcpSEID));
        return AVDTP_ERROR_BAD_SEID;
    }

    SVSUTIL_ASSERT(VerifyOwner(pOwner->hContext));

    Stream* pStream = FindStream(bAcpSEID, FALSE);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] GetConfigurationRequestUp : ERROR_NOT_FOUND\n"));
        return AVDTP_ERROR_BAD_SEID;
    }

    AVDTP_GetConfiguration_Ind pCallback = pOwner->ei.avdtp_GetConfiguration_Ind;
    void* pUserContext = pOwner->pUserContext;

    HANDLE hStream = pStream->hContext;
    BD_ADDR ba = pSession->ba;    

    pOwner->AddRef();
    avdtp_unlock();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback(pUserContext, bTransaction, hStream);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] GetConfigurationRequestUp : exception in avdtp_GetConfiguration_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    if (ERROR_SUCCESS != iRes) {
        bRet = AVDTP_ERROR_UNDEFINED;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--GetConfigurationRequestUp: bRet=%d", iRes));

    return bRet;    
}

/*------------------------------------------------------------------------------
    GetConfigurationResponseUp
    
    This function is called when we receive a GetConfiguration response from
    the peer device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        pConfig:        Configuration elements
        cConfig:        Number of configurations
        bError:         Error sent to us
------------------------------------------------------------------------------*/
int GetConfigurationResponseUp(
            Session* pSession,
            BYTE bTransaction,
            PAVDTP_CAPABILITY_IE pConfig,
            DWORD cConfig,
            BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++GetConfigurationResponseUp: ba:%04x%08x transaction:%d cConfig:%d Error:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, cConfig, bError));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_GET_CONFIGURATION);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"GetConfigurationResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
    
    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"GetConfigurationResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_GetConfiguration_Out pCallback = pOwner->c.avdtp_GetConfiguration_Out;
    void *pCallContext = pCall->pContext;
    HANDLE hStream = pCall->hSession;

    ClearTimeout(pCall);
    DeleteCall(pCall);

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"GetConfigurationResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, hStream, pConfig, cConfig, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] GetConfigurationResponseUp : exception in avdtp_GetConfiguration_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--GetConfigurationResponseUp: iRes=%d\n", iRes));

    return iRes;
}

/*------------------------------------------------------------------------------
    OpenRequestUp
    
    This function is called when we receive an Open request from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bAcpSEID:       SEID of the ACP
------------------------------------------------------------------------------*/
BYTE OpenRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bAcpSEID)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++OpenRequestUp: ba:%04x%08x transaction:%d AcpSEID:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bAcpSEID));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    BYTE bRet = AVDTP_ERROR_SUCCESS;
    
    AVDTP_CONTEXT* pOwner = FindOwner(bAcpSEID);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] OpenRequestUp : Could not find owner context for SEID %d - ERROR_NOT_FOUND\n", bAcpSEID));
        return AVDTP_ERROR_BAD_SEID;
    }

    SVSUTIL_ASSERT(VerifyOwner(pOwner->hContext));

    Stream* pStream = FindStream(bAcpSEID, FALSE);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] OpenRequestUp : ERROR_NOT_FOUND\n"));
        return AVDTP_ERROR_BAD_SEID;
    }

    if (! pStream->state.IsInState(STREAM_STATE_CONFIGURED)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] OpenRequestUp : ERROR_INVALID_STATE\n"));
        return AVDTP_ERROR_BAD_STATE;
    }

    pStream->pOpenCallContext = NULL;

    AVDTP_Open_Ind pCallback = pOwner->ei.avdtp_Open_Ind;
    void* pUserContext = pOwner->pUserContext;

    HANDLE hStream = pStream->hContext;

    pOwner->AddRef();
    avdtp_unlock();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback(pUserContext, bTransaction, hStream);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] OpenRequestUp : exception in avdtp_Open_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    if (ERROR_SUCCESS != iRes) {
        bRet = AVDTP_ERROR_UNDEFINED;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--OpenRequestUp: bRet=%d\n", bRet));
        
    return bRet;
}

/*------------------------------------------------------------------------------
    OpenResponseUp
    
    This function is called when we receive an Open response from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bError:         Error sent to us
------------------------------------------------------------------------------*/
int OpenResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++OpenResponseUp: ba:%04x%08x transaction:%d bError:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bError));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_OPEN);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"OpenResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
    
    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"OpenResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_Open_Out pCallback = pOwner->c.avdtp_Open_Out;
    void *pCallContext = pCall->pContext;
    HANDLE hStream = pCall->hSession;

    ClearTimeout(pCall);
    DeleteCall(pCall);

    if (! bError) {
        Stream* pStream = VerifyStream(hStream);
        if (! pStream) {
            IFDBG(DebugOut (DEBUG_ERROR, L"OpenResponseUp : ERROR_NOT_FOUND\n"));
            return ERROR_NOT_FOUND;
        }
        
        pStream->state.Open();

        // Save this away for when the session is actually up
        pStream->pOpenCallContext = pCallContext;        
        
        Session* pSessMedia = StartSession(&pSession->ba, AVDTP_CHANNEL_MEDIA);

        // We need to verify the stream again since StartSession unlocks.
        if (pStream != VerifyStream(hStream)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"OpenResponseUp : Stream disappeared from under us:%d\n", iRes));
            return iRes;
        } 
        
        if (! pSessMedia) {
            // Failed to start the media session
            CloseStream(pStream, TRUE, iRes);                       
            IFDBG(DebugOut (DEBUG_ERROR, L"OpenResponseUp : Failed to start streaming - error:%d\n", iRes));
            return iRes;
        }

        pStream->hSessMedia = pSessMedia->hContext;

        NotifyAVDTPEvent(pStream);

        // We don't indicate this up until the Media channel is up
        return ERROR_SUCCESS;
    }

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, hStream, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] OpenResponseUp : exception in avdtp_Open_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--OpenResponseUp: iRes=%d\n", iRes));

    return iRes;    
}

/*------------------------------------------------------------------------------
    CloseRequestUp
    
    This function is called when we receive a Close request from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bAcpSEID:       SEID of the ACP
------------------------------------------------------------------------------*/
BYTE CloseRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bAcpSEID)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++CloseRequestUp: ba:%04x%08x transaction:%d AcpSEID:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bAcpSEID));    
    
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    BYTE bRet = AVDTP_ERROR_SUCCESS;
    
    AVDTP_CONTEXT* pOwner = FindOwner(bAcpSEID);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] CloseRequestUp : Could not find owner context for SEID %d - ERROR_NOT_FOUND\n", bAcpSEID));
        return AVDTP_ERROR_BAD_SEID;
    }

    SVSUTIL_ASSERT(VerifyOwner(pOwner->hContext));

    Stream* pStream = FindStream(bAcpSEID, FALSE);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] CloseRequestUp : ERROR_NOT_FOUND\n"));
        return AVDTP_ERROR_BAD_SEID;
    }

    if (! pStream->state.IsOpen()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] CloseRequestUp : ERROR_INVALID_STATE\n"));
        return AVDTP_ERROR_BAD_STATE;
    }

    AVDTP_Close_Ind pCallback = pOwner->ei.avdtp_Close_Ind;
    void* pUserContext = pOwner->pUserContext;

    HANDLE hStream = pStream->hContext;

    pOwner->AddRef();
    avdtp_unlock();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback(pUserContext, bTransaction, hStream);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] CloseRequestUp : exception in avdtp_Close_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    if (ERROR_SUCCESS != iRes) {
        bRet = AVDTP_ERROR_UNDEFINED;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--CloseRequestUp: bRet=%d\n", bRet));

    return bRet;
}

/*------------------------------------------------------------------------------
    CloseResponseUp
    
    This function is called when we receive a Close response from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object  
        bTransaction:   Signaling transaction id
        bError:         Error sent to us
------------------------------------------------------------------------------*/
int CloseResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++CloseResponseUp: ba:%04x%08x transaction:%d Error:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bError));    
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;
    
    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_CLOSE);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"CloseResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
    
    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"CloseResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_Close_Out pCallback = pOwner->c.avdtp_Close_Out;
    void *pCallContext = pCall->pContext;
    HANDLE hStream = pCall->hSession;

    ClearTimeout(pCall);
    DeleteCall(pCall);

    if (! bError) {
        Stream* pStream = VerifyStream(hStream);
        if (! pStream) {
            IFDBG(DebugOut (DEBUG_ERROR, L"CloseResponseUp : ERROR_NOT_FOUND\n"));
            return ERROR_NOT_FOUND;
        }
      
        CloseStream(pStream, TRUE, ERROR_GRACEFUL_DISCONNECT);
    }

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, hStream, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] CloseResponseUp : exception in avdtp_Close_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--CloseResponseUp: iRes=%d\n", iRes));

    return iRes;
}

/*------------------------------------------------------------------------------
    StartRequestUp
    
    This function is called when we receive a Start request from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        pbSEIDList:     Array of SEIDs to be started
        cbSEIDList:     Number of SEIDs (or byte count)
------------------------------------------------------------------------------*/
BYTE StartRequestUp(
            Session* pSession,
            BYTE bTransaction,
            PBYTE pbSEIDList,
            DWORD cbSEIDList)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++StartRequestUp: ba:%04x%08x transaction:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction));    
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    BYTE bRet = AVDTP_ERROR_SUCCESS;
    
    // Our implementation only allows for one stream to be started at a time.  If 
    // multiple streams are included we only look at the first one
    SVSUTIL_ASSERT(cbSEIDList == 1);

    AVDTP_CONTEXT* pOwner = FindOwner(pbSEIDList[0]);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] StartRequestUp : Could not find owner context for SEID %d - ERROR_NOT_FOUND\n", pbSEIDList[0]));
        return AVDTP_ERROR_BAD_SEID;
    }

    SVSUTIL_ASSERT(VerifyOwner(pOwner->hContext));

    Stream* pStream = FindStream(pbSEIDList[0], FALSE);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] StartRequestUp : ERROR_NOT_FOUND\n"));
        return AVDTP_ERROR_BAD_SEID;
    }

    if (! pStream->state.IsInState(STREAM_STATE_OPEN)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] StartRequestUp : ERROR_INVALID_STATE\n"));
        return AVDTP_ERROR_BAD_STATE;
    }

    {
        TransactionContext tc;
        tc.bTransaction = bTransaction;
        tc.bSignalId = AVDTP_SIGNAL_START;
       
        if (! pStream->tcl.push_front(tc)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"StartRequestUp : ERROR_OUTOFMEMORY\n"));
            return AVDTP_ERROR_UNDEFINED;
        }
    }

    AVDTP_Start_Ind pCallback = pOwner->ei.avdtp_Start_Ind;
    void* pUserContext = pOwner->pUserContext;

    HANDLE hStream = pStream->hContext;

    pOwner->AddRef();
    avdtp_unlock();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback(pUserContext, bTransaction, &hStream, 1);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] StartRequestUp : exception in avdtp_Close_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    if (ERROR_SUCCESS != iRes) {
        bRet = AVDTP_ERROR_UNDEFINED;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--StartRequestUp: bRet=%d\n", bRet));

    return bRet;
}

/*------------------------------------------------------------------------------
    StartResponseUp
    
    This function is called when we receive a Start response from the peer
    device on the specified session.
    
    Parameters:
        pSession:           Session object
        bTransaction:       Signaling transaction id
        bFirstFailingSEID:  First failing SEID
        bError:             Error sent to us
------------------------------------------------------------------------------*/
int StartResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bFirstFailingSEID,
            BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++StartResponseUp: ba:%04x%08x transaction:%d FirstFailingSEID:%d Error:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bFirstFailingSEID, bError));    
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;
    
    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_START);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] StartResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
    
    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] StartResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_Start_Out pCallback = pOwner->c.avdtp_Start_Out;
    void *pCallContext = pCall->pContext;

    // Right now we only start 1 stream.  In case of multiple streams 
    // we must lookup by bFirstFailingSEID
    HANDLE hStream = pCall->hSession;

    ClearTimeout(pCall);
    DeleteCall(pCall);

    if (! bError) {
        Stream* pStream = VerifyStream(hStream);
        if (! pStream) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] StartResponseUp : ERROR_NOT_FOUND\n"));
            return ERROR_NOT_FOUND;
        }
      
        pStream->state.Streaming();
        EnsureActiveMode(pStream, TRUE);
        NotifyAVDTPEvent(pStream);

        hStream = NULL;
    }

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, hStream, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] StartResponseUp : exception in avdtp_Close_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--StartResponseUp: iRes=%d\n", iRes));

    return iRes;
}

/*------------------------------------------------------------------------------
    SuspendRequestUp
    
    This function is called when we receive a Suspend request from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        pbSEIDList:     Array of SEIDs to suspend
        cbSEIDList:     Number of SEIDs (or byte count)
------------------------------------------------------------------------------*/
BYTE SuspendRequestUp(
            Session* pSession,
            BYTE bTransaction,
            PBYTE pbSEIDList,
            DWORD cbSEIDList)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++SuspendRequestUp: ba:%04x%08x transaction:%d seid:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, pbSEIDList[0]));    
    
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    BYTE bRet = AVDTP_ERROR_SUCCESS;

    // Our implementation only allows for one stream to be suspended at a time.  If 
    // multiple streams are included we only look at the first one.
    SVSUTIL_ASSERT(cbSEIDList == 1);

    AVDTP_CONTEXT* pOwner = FindOwner(pbSEIDList[0]);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SuspendRequestUp : Could not find owner context for SEID %d - ERROR_NOT_FOUND\n", pbSEIDList[0]));
        return AVDTP_ERROR_BAD_SEID;
    }

    SVSUTIL_ASSERT(VerifyOwner(pOwner->hContext));

    Stream* pStream = FindStream(pbSEIDList[0], FALSE);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SuspendRequestUp : ERROR_NOT_FOUND\n"));
        return AVDTP_ERROR_BAD_SEID;
    }

    if (! pStream->state.IsStreaming()) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SuspendRequestUp : ERROR_INVALID_STATE\n"));
        return AVDTP_ERROR_BAD_STATE;
    }

    {
        TransactionContext tc;
        tc.bTransaction = bTransaction;
        tc.bSignalId = AVDTP_SIGNAL_SUSPEND;
       
        if (! pStream->tcl.push_front(tc)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"SuspendRequestUp : ERROR_OUTOFMEMORY\n"));
            return AVDTP_ERROR_UNDEFINED;
        }
    }

    AVDTP_Suspend_Ind pCallback = pOwner->ei.avdtp_Suspend_Ind;
    void* pUserContext = pOwner->pUserContext;

    HANDLE hStream = pStream->hContext;

    pOwner->AddRef();
    avdtp_unlock();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback(pUserContext, bTransaction, &hStream, 1);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] SuspendRequestUp : exception in avdtp_Close_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    if (ERROR_SUCCESS != iRes) {
        bRet = AVDTP_ERROR_UNDEFINED;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--SuspendRequestUp: bRet=%d\n", bRet));

    return bRet;
   
}

/*------------------------------------------------------------------------------
    SuspendResponseUp
    
    This function is called when we receive a Suspend response from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bFailedSEID:    First Failed SEID
        bError:         Error sent to us
------------------------------------------------------------------------------*/
int SuspendResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bFailedSEID,
            BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++SuspendResponseUp: ba:%04x%08x transaction:%d FailedSEID:%d Error:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bFailedSEID, bError));    
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;
    
    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_SUSPEND);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SuspendResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
    
    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SuspendResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_Suspend_Out pCallback = pOwner->c.avdtp_Suspend_Out;
    void *pCallContext = pCall->pContext;

    HANDLE hStream = pCall->hSession;
    HANDLE hFailedStream = 0;

    ClearTimeout(pCall);
    DeleteCall(pCall);

    if (! bError) {
        Stream* pStream = VerifyStream(hStream);
        if (! pStream) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] SuspendResponseUp : ERROR_NOT_FOUND\n"));
            return ERROR_NOT_FOUND;
        }
      
        pStream->state.Open();
        EnsureActiveMode(pStream, FALSE);
        NotifyAVDTPEvent(pStream);
    } else {
        Stream* pFailedStream = FindStream(bFailedSEID, TRUE);
        if (pFailedStream) {
            hFailedStream = pFailedStream->hContext;
        }
    }

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, hStream, hFailedStream, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] SuspendResponseUp : exception in avdtp_Suspend_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--SuspendResponseUp: iRes=%d\n", iRes));

    return iRes;   
}

/*------------------------------------------------------------------------------
    ReconfigureRequestUp
    
    This function is called when we receive a Reconfigure request from the peer
    device on the specified session.
    
    Parameters:
        pSession:           Session object
        bTransaction:       Signaling transaction id
        bAcpSEID:           SEID of the ACP
        pConfig:            Configuration elements
        cConfig:            Number of configurations
        pbFailedCategory:   Service category which caused failure
------------------------------------------------------------------------------*/
BYTE ReconfigureRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bAcpSEID,
            PAVDTP_CAPABILITY_IE pConfig,
            DWORD cConfig,
            PBYTE pbFailedCategory)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++ReconfigureRequestUp: ba:%04x%08x transaction:%d bAcpSEID:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bAcpSEID));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    BYTE bRet = AVDTP_ERROR_SUCCESS;
    *pbFailedCategory = 0;

    if (cConfig == 0) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] ReconfigureRequestUp : No configuration specified\n"));
        return AVDTP_ERROR_BAD_PAYLOAD_FORMAT;
    }
    
    AVDTP_CONTEXT* pOwner = FindOwner(bAcpSEID);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] ReconfigureRequestUp : Could not find owner context for SEID %d - ERROR_NOT_FOUND\n", bAcpSEID));
        *pbFailedCategory = pConfig[0].bServiceCategory; // On error, pick first category per test spec
        return AVDTP_ERROR_SEP_NOT_IN_USE;
    }

    SVSUTIL_ASSERT(VerifyOwner(pOwner->hContext));

    Stream* pStream = FindStream(bAcpSEID, FALSE);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] ReconfigureRequestUp : AVDTP_ERROR_SEP_NOT_IN_USE\n"));
        *pbFailedCategory = pConfig[0].bServiceCategory; // On error, pick first category per test spec
        return AVDTP_ERROR_SEP_NOT_IN_USE;
    }

    // Check that all service categories are valid for reconfigure
    for (int i = 0; i < cConfig; i++) {
        if (pConfig[i].bServiceCategory != AVDTP_CAT_MEDIA_CODEC) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] ReconfigureRequestUp : AVDTP_ERROR_INVALID_CAPABILITIES\n"));
            *pbFailedCategory = pConfig[i].bServiceCategory;
            return AVDTP_ERROR_INVALID_CAPABILITIES;
        }
    }

    if (! pStream->state.IsInState(STREAM_STATE_OPEN)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] ReconfigureRequestUp : AVDTP_ERROR_BAD_STATE\n"));
        *pbFailedCategory = pConfig[0].bServiceCategory; // On error, pick first category per test spec
        return AVDTP_ERROR_BAD_STATE;
    }    

    AVDTP_Reconfigure_Ind pCallback = pOwner->ei.avdtp_Reconfigure_Ind;
    void* pUserContext = pOwner->pUserContext;

    HANDLE hStream = pStream->hContext;

    pOwner->AddRef();
    avdtp_unlock();

    int iRes = ERROR_INTERNAL_ERROR;
    
    __try {
        iRes = pCallback(pUserContext, bTransaction, hStream, pConfig, cConfig);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] ReconfigureRequestUp : exception in avdtp_Reconfigure_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    if (ERROR_SUCCESS != iRes) {
        bRet = AVDTP_ERROR_UNDEFINED;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--ReconfigureRequestUp: bRet=%d\n", bRet));

    return bRet;    
}

/*------------------------------------------------------------------------------
    ReconfigureResponseUp
    
    This function is called when we receive a Reconfigure response from the
    peer device on the specified session.
    
    Parameters:
        pSession:           Session object
        bTransaction:       Signaling transaction id
        bServiceCategory:   Service category that caused the error
        bError:             Error sent to us
------------------------------------------------------------------------------*/
int ReconfigureResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bServiceCategory,
            BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++ReconfigureResponseUp: ba:%04x%08x transaction:%d ServiceCategory:%d Error:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bServiceCategory, bError));
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;

    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_RECONFIGURE);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"ReconfigureResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
    
    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"ReconfigureResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_Reconfigure_Out pCallback = pOwner->c.avdtp_Reconfigure_Out;
    void *pCallContext = pCall->pContext;
    HANDLE hStream = pCall->hSession;

    ClearTimeout(pCall);
    DeleteCall(pCall);

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"ReconfigureResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, hStream, bServiceCategory, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] ReconfigureResponseUp : exception in avdtp_Reconfigure_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--ReconfigureResponseUp: iRes=%d\n", iRes));

    return iRes;
}

BYTE SecurityControlRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bSEID,
            USHORT cbSecurityControlData,
            PBYTE pSecurityControlData)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;
    
    // TODO: Implement me!

    return iRes;
}

int SecurityControlResponseUp(
            Session* pSession,
            BYTE bTransaction,
            USHORT cbSecurityControlData,
            PBYTE pSecurityControlData,
            BYTE bError)
{
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;
    
    // TODO: Implement me!

    return iRes;
}

/*------------------------------------------------------------------------------
    AbortRequestUp
    
    This function is called when we receive an Abort request from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bAcpSEID:       SEID of the ACP
------------------------------------------------------------------------------*/
BYTE AbortRequestUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bAcpSEID)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++AbortRequestUp: ba:%04x%08x transaction:%d AcpSEID:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bAcpSEID));    
    
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    BYTE bRet = AVDTP_ERROR_SUCCESS;
    
    AVDTP_CONTEXT* pOwner = FindOwner(bAcpSEID);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] AbortRequestUp : Could not find owner context for SEID %d - ERROR_NOT_FOUND\n", bAcpSEID));
        return AVDTP_ERROR_BAD_SEID;
    }

    SVSUTIL_ASSERT(VerifyOwner(pOwner->hContext));

    Stream* pStream = FindStream(bAcpSEID, FALSE);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] AbortRequestUp : ERROR_NOT_FOUND\n"));
        return AVDTP_ERROR_BAD_SEID;
    }

    AVDTP_Abort_Ind pCallback = pOwner->ei.avdtp_Abort_Ind;
    void* pUserContext = pOwner->pUserContext;

    HANDLE hStream = pStream->hContext;

    pOwner->AddRef();
    avdtp_unlock();

    int iRes = ERROR_INTERNAL_ERROR;

    __try {
        iRes = pCallback(pUserContext, bTransaction, hStream);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] AbortRequestUp : exception in avdtp_Abort_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    if (ERROR_SUCCESS != iRes) {
        bRet = AVDTP_ERROR_UNDEFINED;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--AbortRequestUp: bRet=%d\n", bRet));

    return bRet;
}

/*------------------------------------------------------------------------------
    AbortResponseUp
    
    This function is called when we receive an Abort response from the peer
    device on the specified session.
    
    Parameters:
        pSession:       Session object
        bTransaction:   Signaling transaction id
        bError:         Non-zero indicates an error
------------------------------------------------------------------------------*/
int AbortResponseUp(
            Session* pSession,
            BYTE bTransaction,
            BYTE bError)
{
    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"++AbortResponseUp: ba:%04x%08x transaction:%d Error:%d\n", 
        pSession->ba.NAP, pSession->ba.SAP, bTransaction, bError));    
    
    SVSUTIL_ASSERT(avdtp_IsLocked());

    int iRes = ERROR_SUCCESS;
    
    // TODO: Verify transaction matches up with request
    Task* pCall = FindCall(pSession->hContext, bTransaction, AVDTP_SIGNAL_ABORT);
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"AbortResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
    
    AVDTP_CONTEXT* pOwner = VerifyOwner(pCall->hOwner);
    if (! pOwner) {
        DeleteCall(pCall);
        IFDBG(DebugOut (DEBUG_ERROR, L"AbortResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_Abort_Out pCallback = pOwner->c.avdtp_Abort_Out;
    void *pCallContext = pCall->pContext;
    HANDLE hStream = pCall->hSession;

    ClearTimeout(pCall);
    DeleteCall(pCall);

    Stream* pStream = VerifyStream(hStream);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"AbortResponseUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
  
    pStream->state.Aborting();
        
    CloseStream(pStream, TRUE, ERROR_GRACEFUL_DISCONNECT);

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pCallContext, hStream, bError);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] AbortResponseUp : exception in avdtp_Abort_Out\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    IFDBG(DebugOut(DEBUG_AVDTP_TRACE, L"--AbortResponseUp: iRes=%d\n", iRes));

    return iRes;
}

/*------------------------------------------------------------------------------
    StreamDataUp
    
    This function is called when we receive data on the media session.
    
    Parameters:
        pSession:       Media session 
        pBuffer:        Buffer of received data
        dwTimeInfo:     Time info from peer (see RTP spec)
        bPayloadType:   Payload time from peer (see RTP spec)
        usMarker:       Marker bit from peer (see RTP spec)
        usReliability:  We make this up.  What is this??
------------------------------------------------------------------------------*/
int StreamDataUp(
            Session* pSession,
            BD_BUFFER* pBuffer,
            DWORD dwTimeInfo,
            BYTE bPayloadType,
            USHORT usMarker,
            USHORT usReliability) 
{
    SVSUTIL_ASSERT(avdtp_IsLocked());
    
    int iRes = ERROR_SUCCESS;

    Stream* pStream = FindStream(pSession->hContext);
    if (! pStream) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] StreamDataUp : ERROR_NOT_FOUND\n"));
        return ERROR_NOT_FOUND;
    }
    
    AVDTP_CONTEXT* pOwner = VerifyOwner(pStream->hOwner);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] StreamDataUp : Could not verify owner context\n"));
        return ERROR_NOT_FOUND;
    }

    AVDTP_StreamData_Up_Ind pCallback = pOwner->ei.avdtp_StreamData_Up_Ind;
    void* pUserContext = pOwner->pUserContext;

    HANDLE hStream = pStream->hContext;

    pOwner->AddRef();
    avdtp_unlock();

    __try {
        iRes = pCallback(pUserContext, hStream, pBuffer, dwTimeInfo, bPayloadType, usMarker, usReliability);
    } __except(1) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] StreamDataUp : exception in avdtp_StreamData_Up_Ind\n"));
    }

    avdtp_lock();
    pOwner->DelRef();

    return iRes;
}


//
// The following functions are called by the BT stack to 
// initialize/deinitialize the AVDTP layer.
//

void CleanUpAVDTPState(void)
{
    if (gpAVDTP->pfmdContexts) {
        svsutil_ReleaseFixedEmpty(gpAVDTP->pfmdContexts);
        gpAVDTP->pfmdContexts = NULL;
    }

    if (gpAVDTP->pfmdTasks) {
        svsutil_ReleaseFixedEmpty(gpAVDTP->pfmdTasks);
        gpAVDTP->pfmdTasks = NULL;
    }

    if (gpAVDTP->pfmdSessions) {
        svsutil_ReleaseFixedEmpty(gpAVDTP->pfmdSessions);
        gpAVDTP->pfmdSessions = NULL;
    }

    if (gpAVDTP->pfmdStreams) {
        svsutil_ReleaseFixedEmpty(gpAVDTP->pfmdStreams);
        gpAVDTP->pfmdStreams = NULL;
    }
    
    gpAVDTP->Reinit();
}

int avdtp_InitializeOnce(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    
    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"++avdtp_InitializeOnce\n"));

    if (gpAVDTP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] avdtp_InitializeOnce:: ERROR_ALREADY_EXISTS\n"));
        SVSUTIL_ASSERT(0);
        dwRetVal = ERROR_ALREADY_EXISTS;
        goto exit;
    }

    gpAVDTP = new AVDTP;
    if (! gpAVDTP) {        
        IFDBG(DebugOut (DEBUG_ERROR, L"[AVDTP] avdtp_InitializeOnce:: ERROR_OUTOFMEMORY\n"));
        dwRetVal = ERROR_OUTOFMEMORY;
        goto exit;
    }

    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"--avdtp_InitializeOnce:: ERROR_SUCCESS\n"));

exit:    
    return dwRetVal;
}

int avdtp_CreateDriverInstance(void)
{
    int iRes = ERROR_SUCCESS;
    
    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"++avdtp_CreateDriverInstance\n"));

    if (! gpAVDTP) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTP : not initialized!\n"));
        iRes = ERROR_SERVICE_DOES_NOT_EXIST;        
        goto exit;
    }

    avdtp_lock();
    
    if (gpAVDTP->state.IsRunning()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTP : already initialized!\n"));
        iRes = ERROR_SERVICE_ALREADY_RUNNING;
        goto exit;
    }

    gpAVDTP->state.Initializing();

    gpAVDTP->pfmdContexts = svsutil_AllocFixedMemDescr(sizeof(AVDTP_CONTEXT), AVDTP_SCALE);
    gpAVDTP->pfmdTasks = svsutil_AllocFixedMemDescr(sizeof(Task), AVDTP_SCALE);
    gpAVDTP->pfmdSessions = svsutil_AllocFixedMemDescr(sizeof(Session), AVDTP_SCALE);
    gpAVDTP->pfmdStreams = svsutil_AllocFixedMemDescr(sizeof(Stream), AVDTP_SCALE);
    
    if (! (gpAVDTP->pfmdContexts && gpAVDTP->pfmdTasks && gpAVDTP->pfmdSessions && gpAVDTP->pfmdStreams)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"AVDTP : OOM in init\n"));
        iRes =  ERROR_OUTOFMEMORY;
        goto exit;
    }

    bool fEnabled = true;

    DWORD dwMediaLowThreshold = 0;
    DWORD dwMediaHighThreshold = 0;
 
    HKEY hk;
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"software\\Microsoft\\bluetooth\\avdtp", 0, KEY_READ, &hk)) {
        DWORD dwVal;
        DWORD dwType = REG_DWORD;
        DWORD dwSize = sizeof(dwVal);

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"CommandTimeoutSec", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD))
            gpAVDTP->cCmdTimeoutSec = dwVal;

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"IdleTimeoutSec", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD))
            gpAVDTP->cIdleTimeoutSec = dwVal;

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"PageTimeout", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD))
            gpAVDTP->usPageTimeout = dwVal;

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"MediaLowThreshold", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD))
            dwMediaLowThreshold = dwVal;

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"MediaHighThreshold", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD))
            dwMediaHighThreshold = dwVal;

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"InMTU", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD)) {
            gpAVDTP->usInMTU = dwVal;
        }

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"HCIBufferThreshold", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD)) {
            gpAVDTP->dwHciBuffThreshold = dwVal;
        }

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"HCIBufferThresholdSleep", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD)) {
            gpAVDTP->dwHciBuffThresholdSleep = dwVal;
        }

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"SniffDelay", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD)) {
            gpAVDTP->usSniffDelay = dwVal;
        }

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"SniffMax", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD)) {
            gpAVDTP->usSniffMax = dwVal;
        }

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"SniffMin", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD)) {
            gpAVDTP->usSniffMin = dwVal;
        }

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"SniffAttempt", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD)) {
            gpAVDTP->usSniffAttempt = dwVal;
        }

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"SniffTimeout", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD)) {
            gpAVDTP->usSniffTimeout = dwVal;
        }

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"Enabled", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD)) {
            fEnabled = (dwVal == 0) ? false : true;
        }

        RegCloseKey (hk);
    }

    if (! fEnabled) {
        IFDBG(DebugOut(DEBUG_WARN, L"[AVDTP] avdtp_CreateDriverInstance:: AVDTP is disabled from the registry\n"));
        iRes = ERROR_SUCCESS;   // Return success so stack keeps loading

        // This will deallocate the previous allocations and reinit the AVDTP
        // state.  Resetting the state is important to ensure all the avdtp_xxx
        // entrypoints fail properly.
        CleanUpAVDTPState();
        goto exit;
    }

    // Verify configurable values    
    if (gpAVDTP->cCmdTimeoutSec > AVDTP_COMMAND_TIMEOUT_MAX) {
        gpAVDTP->cCmdTimeoutSec = AVDTP_COMMAND_TIMEOUT;
    }
    if (gpAVDTP->cIdleTimeoutSec > AVDTP_IDLE_TIMEOUT_MAX) {
        gpAVDTP->cIdleTimeoutSec = AVDTP_IDLE_TIMEOUT;
    }

    gpAVDTP->media.Init(dwMediaLowThreshold, dwMediaHighThreshold);
    gpAVDTP->signal.Init();    

    L2CAP_EVENT_INDICATION lei;
    memset(&lei, 0, sizeof(lei));

    L2CAP_CALLBACKS lc;
    memset(&lc, 0, sizeof(lc));

    lei.l2ca_StackEvent = avdtp_stack_event;
    lei.l2ca_ConfigInd = avdtp_config_ind;
    lei.l2ca_ConnectInd = avdtp_connect_ind;
    lei.l2ca_DisconnectInd = avdtp_disconnect_ind;
    lei.l2ca_DataUpInd = avdtp_data_up_ind;
    
    lc.l2ca_CallAborted = avdtp_call_aborted;
    lc.l2ca_ConfigReq_Out = avdtp_config_req_out;
    lc.l2ca_ConfigResponse_Out = avdtp_config_response_out;
    lc.l2ca_ConnectReq_Out = avdtp_connect_req_out;
    lc.l2ca_ConnectResponse_Out = avdtp_connect_response_out;
    lc.l2ca_DataDown_Out = avdtp_data_down_out;
    lc.l2ca_Disconnect_Out = avdtp_disconnect_out;

    iRes = L2CAP_EstablishDeviceContext (gpAVDTP, AVDTP_PSM, &lei, &lc, &gpAVDTP->l2cap_if,
            &gpAVDTP->cDataHeaders, &gpAVDTP->cDataTrailers, &gpAVDTP->hL2CAP);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] avdtp_CreateDriverInstance:: L2CAP_EstablishDeviceContext failed, %d\n", iRes));
        goto exit;
    }

    if ((! gpAVDTP->hL2CAP) || (! (gpAVDTP->l2cap_if.l2ca_AbortCall && gpAVDTP->l2cap_if.l2ca_ConfigReq_In && gpAVDTP->l2cap_if.l2ca_ConfigResponse_In &&
        gpAVDTP->l2cap_if.l2ca_ConnectReq_In && gpAVDTP->l2cap_if.l2ca_ConnectResponse_In && gpAVDTP->l2cap_if.l2ca_DataDown_In &&
        gpAVDTP->l2cap_if.l2ca_Disconnect_In)) || (gpAVDTP->cDataHeaders + gpAVDTP->cDataTrailers > 128)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[AVDTP] avdtp_CreateDriverInstance:: L2cap context inadequate, ERROR_NOT_SUPPORTED\n"));

        iRes = ERROR_NOT_SUPPORTED;
        goto exit;
    }

    gpAVDTP->state.Initialized();

    GetConnectionState();

    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"--avdtp_CreateDriverInstance:: ERROR_SUCCESS\n"));

exit:
    if (gpAVDTP) {
        if (ERROR_SUCCESS != iRes) {
            if (gpAVDTP->hL2CAP) {
                L2CAP_CloseDeviceContext(gpAVDTP->hL2CAP);
            }
            
            CleanUpAVDTPState();
        }

        avdtp_unlock();
    }
    
    return iRes;
}

int avdtp_CloseDriverInstance(void)
{
    int iRes = ERROR_SUCCESS;
    
    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"++avdtp_CloseDriverInstance\n"));

    if (avdtp_lock_if_running()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"avdtp_CloseDriverInstance :: ERROR_SERVICE_NOT_ACTIVE\n"));
        iRes = ERROR_SERVICE_NOT_ACTIVE;        
        goto exit;
    }

    gpAVDTP->state.Closing();

    // TODO: Implement me!

    CleanUpAVDTPState();

    gpAVDTP->state.Closed();

    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"--avdtp_CloseDriverInstance:: ERROR_SUCCESS\n"));

exit:
    if (ERROR_SUCCESS != iRes) {
        // TODO: Clean up   
    }

    return iRes;
}

int avdtp_UninitializeOnce(void)
{
    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"++avdtp_UninitializeOnce\n"));

    if (! gpAVDTP) {
        IFDBG(DebugOut(DEBUG_ERROR, L"--avdtp_UninitializeOnce:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;        
    }

    avdtp_lock();
    
    if (gpAVDTP->state.IsRunning()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"--avdtp_UninitializeOnce:: ERROR_DEVICE_IN_USE\n"));
        avdtp_unlock();
        return ERROR_DEVICE_IN_USE;
    }

    AVDTP* pAVDTP = gpAVDTP;
    gpAVDTP = NULL;

    avdtp_unlock(); // unlock before delete

    delete pAVDTP;

    IFDBG(DebugOut(DEBUG_AVDTP_INIT, L"--avdtp_UninitializeOnce:: ERROR_SUCCESS\n"));
  
    return ERROR_SUCCESS;
}

int AVDTP_EstablishDeviceContext
(
            void*                   pUserContext,       /* IN */
            PAVDTP_EVENT_INDICATION pInd,               /* IN */
            PAVDTP_CALLBACKS        pCall,              /* IN */
            PAVDTP_INTERFACE        pInt,               /* OUT */
            int*                    pcDataHeaders,      /* OUT */
            int*                    pcDataTrailers,     /* OUT */
            HANDLE*                 phDeviceContext)    /* OUT */
{
    IFDBG(DebugOut (DEBUG_AVDTP_INIT, L"++AVDTP_EstablishDeviceContext\n"));

    if (! gpAVDTP) {
        IFDBG(DebugOut(DEBUG_ERROR, L"--AVDTP_EstablishDeviceContext:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;        
    }

    if (! avdtp_lock_if_running()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"--AVDTP_EstablishDeviceContext:: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    AVDTP_CONTEXT* pContext = new AVDTP_CONTEXT;
    if (! pContext) {
        IFDBG(DebugOut(DEBUG_ERROR, L"--AVDTP_EstablishDeviceContext:: ERROR_OUTOFMEMORY\n"));
        return ERROR_OUTOFMEMORY; 
    }
    
    pContext->ei = *pInd;
    pContext->c = *pCall;
    pContext->pUserContext = pUserContext;
    
    pContext->hContext = (HANDLE) btutil_AllocHandle(pContext);
    if (SVSUTIL_HANDLE_INVALID == pContext->hContext) {
        IFDBG(DebugOut(DEBUG_ERROR, L"AVDTP_EstablishDeviceContext : ERROR_OUTOFMEMORY\n"));
        delete pContext;
        avdtp_unlock();
        return NULL;
    }

    if (! gpAVDTP->listContexts.push_front(pContext)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"--AVDTP_EstablishDeviceContext:: ERROR_OUTOFMEMORY\n"));
        btutil_CloseHandle((SVSHandle)pContext->hContext);
        delete pContext;
        avdtp_unlock();
        return ERROR_OUTOFMEMORY;
    }

    // TODO: Fix this up.  Right now I'm hard-coding 12.  But this should be a const and we are not including optional CSRC field.
    *pcDataHeaders = gpAVDTP->cDataHeaders + 12;
    *pcDataTrailers = gpAVDTP->cDataTrailers;
    *phDeviceContext = pContext->hContext;

    pInt->avdtp_DiscoverReq_In          = avdtp_DiscoverReq_In;
    pInt->avdtp_DiscoverRsp_In          = avdtp_DiscoverRsp_In;
    pInt->avdtp_GetCapabilitiesReq_In   = avdtp_GetCapabilitiesReq_In;
    pInt->avdtp_GetCapabilitiesRsp_In   = avdtp_GetCapabilitiesRsp_In;
    pInt->avdtp_SetConfigurationReq_In  = avdtp_SetConfigurationReq_In;
    pInt->avdtp_SetConfigurationRsp_In  = avdtp_SetConfigurationRsp_In;
    pInt->avdtp_GetConfigurationReq_In  = avdtp_GetConfigurationReq_In;
    pInt->avdtp_GetConfigurationRsp_In  = avdtp_GetConfigurationRsp_In;
    pInt->avdtp_OpenReq_In              = avdtp_OpenReq_In;
    pInt->avdtp_OpenRsp_In              = avdtp_OpenRsp_In;
    pInt->avdtp_CloseReq_In             = avdtp_CloseReq_In;
    pInt->avdtp_CloseRsp_In             = avdtp_CloseRsp_In;
    pInt->avdtp_StartReq_In             = avdtp_StartReq_In;
    pInt->avdtp_StartRsp_In             = avdtp_StartRsp_In;
    pInt->avdtp_SuspendReq_In           = avdtp_SuspendReq_In;
    pInt->avdtp_SuspendRsp_In           = avdtp_SuspendRsp_In;
    pInt->avdtp_ReconfigureReq_In       = avdtp_ReconfigureReq_In;
    pInt->avdtp_ReconfigureRsp_In       = avdtp_ReconfigureRsp_In;
    pInt->avdtp_SecurityControlReq_In   = avdtp_SecurityControlReq_In;
    pInt->avdtp_SecurityControlRsp_In   = avdtp_SecurityControlRsp_In;
    pInt->avdtp_AbortReq_In             = avdtp_AbortReq_In;
    pInt->avdtp_AbortRsp_In             = avdtp_AbortRsp_In;
    pInt->avdtp_WriteReq_In             = avdtp_WriteReq_In;
    pInt->avdtp_ioctl                   = avdtp_ioctl;
    pInt->avdtp_AbortCall               = avdtp_AbortCall;

    avdtp_unlock();

    IFDBG(DebugOut (DEBUG_AVDTP_INIT, L"--AVDTP_EstablishDeviceContext:: ERROR_SUCCESS\n"));
    
    return ERROR_SUCCESS;
}

int AVDTP_CloseDeviceContext(HANDLE hDeviceContext)
{
    IFDBG(DebugOut (DEBUG_L2CAP_TRACE, L"AVDTP_CloseDeviceContext [0x%08x]\n", hDeviceContext));

    if (! avdtp_lock_if_running()) {
        IFDBG(DebugOut(DEBUG_ERROR, L"--AVDTP_CloseDeviceContext:: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    AVDTP_CONTEXT* pOwner = VerifyOwner(hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut(DEBUG_ERROR, L"--AVDTP_CloseDeviceContext:: ERROR_NOT_FOUND\n"));
        avdtp_unlock();
        return ERROR_NOT_FOUND;
    }

    CloseStreamsByOwner(hDeviceContext, ERROR_OPERATION_ABORTED);

    for (ContextList::iterator it = gpAVDTP->listContexts.begin(), itEnd = gpAVDTP->listContexts.end(); it != itEnd;) {
        if ((*it)->hContext == hDeviceContext) {
            gpAVDTP->listContexts.erase(it);
            break;
        } 
    }
    
    avdtp_unlock();

    IFDBG(DebugOut(DEBUG_ERROR, L"--AVDTP_CloseDeviceContext:: ERROR_SUCCESS\n"));
    
    return ERROR_SUCCESS;    
}

void *AVDTP_CONTEXT::operator new (size_t iSize) {
    SVSUTIL_ASSERT (iSize == sizeof(AVDTP_CONTEXT));
    return svsutil_GetFixed (gpAVDTP->pfmdContexts);
}

void AVDTP_CONTEXT::operator delete (void *ptr) {
    svsutil_FreeFixed (ptr, gpAVDTP->pfmdContexts);
}

void *Task::operator new (size_t iSize) {
    SVSUTIL_ASSERT(iSize == sizeof(Task));
    return svsutil_GetFixed(gpAVDTP->pfmdTasks);
}

void Task::operator delete (void *ptr) {
    svsutil_FreeFixed(ptr, gpAVDTP->pfmdTasks);
}

void *Session::operator new (size_t iSize) {
    SVSUTIL_ASSERT(iSize == sizeof(Session));
    return svsutil_GetFixed(gpAVDTP->pfmdSessions);
}

void Session::operator delete (void *ptr) {
    svsutil_FreeFixed(ptr, gpAVDTP->pfmdSessions);
}

void *Stream::operator new (size_t iSize) {
    SVSUTIL_ASSERT(iSize == sizeof(Stream));
    return svsutil_GetFixed(gpAVDTP->pfmdStreams);
}

void Stream::operator delete (void *ptr) {
    svsutil_FreeFixed(ptr, gpAVDTP->pfmdStreams);
}


