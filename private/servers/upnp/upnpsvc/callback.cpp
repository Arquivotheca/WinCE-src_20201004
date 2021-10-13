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
#include "pch.h"
#pragma hdrstop


// Forward Declarations
DispatchGate * FindDispatchGateForProcInternal(HANDLE hProc);
BOOL AddDispatchGateForProcInternal(HANDLE hProc);


HANDLE GetCallingProcess()
{
    HANDLE h = GetCallerProcess();
    if ( h == NULL )
        return (HANDLE)GetCurrentProcessId();
    else
        return h;
}

// maintains the list of all DispatchGates, one per process
LIST_ENTRY DispatchGate::list = {&DispatchGate::list, &DispatchGate::list};

DispatchGate::DispatchGate(HANDLE hTargetProc) :
            m_ResetMutexRequestor(FALSE),
            m_fShuttingDown(FALSE),
            m_RefCount(0),
            m_hMutexRequestor(NULL),
            m_hEvtRequest(NULL),
            m_hEvtRequestDone(NULL),
            m_pbReqBuf(NULL),
            m_pcbReqBufSize(NULL),
            m_cbReqBufSize(0),
            m_dwReturnCode(ERROR_SUCCESS),
            m_UpnpInvokeRequestContainer(NULL)
{
    InitializeListHead(&m_Link);
    m_state = CBGATE_INVALID;
    Assert(hTargetProc == GetCallingProcess());
    m_hTargetProc = hTargetProc;
    m_hEvtRequest = CreateEvent(NULL,FALSE,FALSE,NULL);   // auto-reset, non-signaled
    m_hEvtRequestDone = CreateEvent(NULL,FALSE,FALSE,NULL);   // auto-reset, non-signaled
    m_hMutexRequestor = CreateMutex(NULL,FALSE, NULL);   // initially signaled
    if (m_hEvtRequestDone && m_hEvtRequest && m_hMutexRequestor)
    {
        m_state = CBGATE_READY;
    }
}



DispatchGate::~DispatchGate()
{
    TraceTag(ttidTrace, "%s: [%08x] Deallocating DispatchGate refCount[%d]\n", __FUNCTION__, this, m_RefCount);
    m_state = CBGATE_INVALID;
    Assert(m_fShuttingDown);
    Assert(m_RefCount == 0);

    CloseHandle(m_hMutexRequestor);
    m_hMutexRequestor = NULL;

    CloseHandle(m_hEvtRequest);
    m_hEvtRequest = NULL;

    CloseHandle(m_hEvtRequestDone);
    m_hEvtRequestDone = NULL;
}



BOOL
DispatchGate::WaitForRequests(DWORD dwTime)
{
    DWORD dwWait;
    BOOL fRet;
    Assert(m_hTargetProc == GetCallingProcess());
    TraceTag(ttidControl, "%s: [%08x] Begin Waiting for Next Request\n", __FUNCTION__, this);

    if (m_state < CBGATE_READY || m_fShuttingDown)
    {
        SetLastError(ERROR_NOT_READY);
        TraceTag(ttidControl, "%s: [%08x] Error Not Ready\n", __FUNCTION__, this);
        fRet = FALSE;   // probably shutting down..
    }
    else
    {
        fRet = TRUE;
        TraceTag(ttidControl, "%s: [%08x] Waiting for Next Request\n", __FUNCTION__, this);
        dwWait = WaitForSingleObject(m_hEvtRequest, dwTime);
        if (m_fShuttingDown)
        {
            TraceTag(ttidControl, "%s: [%08x] Error Not Ready\n", __FUNCTION__, this);
            SetLastError(ERROR_NOT_READY);
            fRet = FALSE;
        }
        else if (dwWait != WAIT_OBJECT_0)
        {
            if (dwWait == WAIT_TIMEOUT)
            {
                SetLastError(dwWait);
            }
            fRet = FALSE;
        }
    }

    TraceTag(ttidControl, "%s: [%08x] Response [%S]\n", __FUNCTION__, this, fRet ? L"TRUE" : L"FALSE");
    return fRet;
}



BOOL
DispatchGate::DispatchRequest()
{
    BOOL fRet = TRUE;

    Assert(m_state == CBGATE_REQUEST_WAITING);
    Assert(m_hTargetProc == GetCallingProcess());
    Assert(m_pcbReqBufSize);
    Assert(m_UpnpInvokeRequestContainer);
    m_state = CBGATE_REQUEST_HANDLING;
    m_ResetMutexRequestor = TRUE;
    TraceTag(ttidControl, "%s: [%08x] Dispatching Request\n", __FUNCTION__, this);

    // insufficient buffer to process invoke request
    if (m_UpnpInvokeRequestContainer->GetUpnpInvokeRequestSize() > m_cbReqBufSize)
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        TraceTag(ttidControl, "%s: [%08x] Insufficient Buffer to Dispatch Request\n", __FUNCTION__, this);
        return FALSE;
    }

    *m_pcbReqBufSize = m_UpnpInvokeRequestContainer->GetUpnpInvokeRequestSize();
    TraceTag(ttidControl, "%s: [%08x] Upnp Invoke Request Container[%08x]\n", __FUNCTION__, this, m_UpnpInvokeRequestContainer);
    TraceTag(ttidControl, "%s: [%08x] Upnp Invoke Request[%08x]\n", __FUNCTION__, this, m_UpnpInvokeRequestContainer->GetUpnpInvokeRequest());
    TraceTag(ttidControl, "%s: [%08x] TargetProc[%08x]\n", __FUNCTION__, this, m_hTargetProc);
    TraceTag(ttidControl, "%s: [%08x] Request Buffer[%08x]\n", __FUNCTION__, this, m_pbReqBuf);
    TraceTag(ttidControl, "%s: [%08x] Request Size[%d]\n", __FUNCTION__, this, *m_pcbReqBufSize);

    // try/except because we are accessing memory (m_pbReqBuf) 
    __try  {
        memcpy(m_pbReqBuf, m_UpnpInvokeRequestContainer->GetUpnpInvokeRequest(), *m_pcbReqBufSize);
        TraceTag(ttidControl, "%s: [%08x] Copied Buffer to Invoke Handler Process\n", __FUNCTION__, this);
        // expect m_dwReturnedCode to be set when the thread comes back from processing the invoke message
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        TraceTag(ttidError,"%s: [%08x] Exception in PerformCallback  hProc=%x\n",  m_hTargetProc);
        fRet = FALSE;
    }

    Assert(!SentinelSet(m_pbReqBuf) && fRet);
    TraceTag(ttidControl, "%s: [%08x] Indicate that the Reset Mutex Requestor is valid\n", __FUNCTION__, this);
    Assert(m_state == CBGATE_REQUEST_HANDLING);

    return fRet;
}



/**
 *  EnterGate:
 *
 *      - attempts to access a mutex
 *      - the mutex is expected to be initially opened
 *      - after this call either the mutex is acquired
 *      - or there was a timeout waiting for it
 *
 */
BOOL
DispatchGate::EnterGate(DWORD dwTime)
{
    DWORD dwWait;
    BOOL fRet = FALSE;

    TraceTag(ttidControl, "%s: [%08x] Entering Dispatch Gate - Wait[%08x]\n", __FUNCTION__, this, dwTime);
    if (m_state >= CBGATE_READY && !m_fShuttingDown)
    {
        dwWait = WaitForSingleObject(m_hMutexRequestor, dwTime);
        if (dwWait == WAIT_OBJECT_0)
        {
            m_state = CBGATE_ENTERED;
            fRet = TRUE;
        }
        //else timeout
    }
    TraceTag(ttidControl, "%s: [%08x] Entered Dispatch Gate - Wait[%08x]\n", __FUNCTION__, this, dwTime);
    return fRet;
}



/**
 *  DoCallback:
 *
 *      - stores data required for the invoke request handler thread
 *      - signals the thread and waits for the handler to complete
 *
 */
BOOL
DispatchGate::DoCallback(
    UpnpInvokeRequestContainer_t &requestContainer,
    DWORD dwTimeout)
{
    DWORD dwWait;
    Assert(m_state >= CBGATE_ENTERED);

    if (m_state < CBGATE_ENTERED || m_fShuttingDown)
    {
        TraceTag(ttidError, "%s: [%08x] entry: error incorrect state (%d) or shutting down (%d)\n", __FUNCTION__, this, m_state, m_fShuttingDown); 
        return FALSE;
    }

    // store data to be transfer to the receive invoke request thread
    m_UpnpInvokeRequestContainer = &requestContainer;
    m_state = CBGATE_REQUEST_WAITING;
    TraceTag(ttidControl, "%s: [%08x] Upnp Invoke Request Container[%08x]\n", __FUNCTION__, this, m_UpnpInvokeRequestContainer);
    TraceTag(ttidControl, "%s: [%08x] Upnp Invoke Request[%08x]\n", __FUNCTION__, this, m_UpnpInvokeRequestContainer->GetUpnpInvokeRequest());

    // signal the receive invoke request thread so that it can handle the request
    SetEvent(m_hEvtRequest);
    TraceTag(ttidControl, "%s: [%08x] Reseting RequestDone Event and Setting Request Event\n", __FUNCTION__, this, m_UpnpInvokeRequestContainer);
    dwWait = WaitForSingleObject(m_hEvtRequestDone, dwTimeout);

    // expect that the invoke request has been handled by the device at this point
    TraceTag(ttidControl,"%s: [%08x] RequestDone (dwWait = %d) \n", __FUNCTION__, this, dwWait);


    // clear the upnp invoke request container
    m_UpnpInvokeRequestContainer = NULL;

    if (m_fShuttingDown || m_state != CBGATE_REQUEST_DONE)
    {
        TraceTag(ttidError, "%s: [%08x]  exit: error state (%d) or shutting down (%d)\n", __FUNCTION__, this, m_state, m_fShuttingDown); 
        return FALSE;
    }

    m_state = CBGATE_ENTERED;
    TraceTag(ttidControl, "%s: [%08x] Completed Perform Callback [%d]\n", __FUNCTION__, this, m_dwReturnCode);
    SetLastError(m_dwReturnCode);
    return (m_dwReturnCode == ERROR_SUCCESS);
}



/**
 *  LeaveGate:
 *
 *      - attempts to release a mutex
 *      - the mutex is expected to be acquired
 *      - after this call either the mutex is available
 *
 */
BOOL
DispatchGate::LeaveGate()
{
    Assert(m_state >= CBGATE_ENTERED || !m_fShuttingDown);
    m_state = CBGATE_READY;
    ReleaseMutex(m_hMutexRequestor);
    TraceTag(ttidControl, "%s: [%08x] Released Dispatch Gate\n", __FUNCTION__, this);
    return TRUE;
}



/**
 *  Shutdown:
 *
 *      - mark the dispatch gate as shutting down
 *      - close both communication events to ensure subsequent waits fail
 *      - close the requestor event to avoid race condition thread leaks
 *
 */
BOOL
DispatchGate::Shutdown()
{
    LONG refCnt;
    TraceTag(ttidControl, "%s: [%08x] Shutting Down DispatchGate\n", __FUNCTION__, this);
    if(m_fShuttingDown)
    {
        return FALSE;
    }

    m_fShuttingDown = TRUE;

    SetEvent(m_hEvtRequest);
    CloseHandle(m_hEvtRequest);
    m_hEvtRequest = NULL;

    SetEvent(m_hEvtRequestDone);
    CloseHandle(m_hEvtRequestDone);
    m_hEvtRequestDone = NULL;

    // release the global reference
    refCnt = this->DecRef();
    TraceTag(ttidControl, "%s: DispatchGate refCount = %d\n", __FUNCTION__, refCnt);

    // there should be one more reference to be decremented
    Assert(m_RefCount >= 1);
    return TRUE;
}


void
DispatchGate::SetCallerBuffer(
    PBYTE pbReq, 
    DWORD cbReqSize,
    PDWORD pcbReqSize)
{
    TraceTag(ttidControl, "%s: [%08x] Save buffer information Buffer[%08x] Size[%d]\n", __FUNCTION__, this, m_pbReqBuf, m_cbReqBufSize);
    m_pbReqBuf = pbReq;
    m_cbReqBufSize = cbReqSize;
    m_pcbReqBufSize = pcbReqSize;

    SetSentinel(m_pbReqBuf);
    Assert(SentinelSet(m_pbReqBuf));
}


void DispatchGate::CheckResetMutexRequestor(DWORD retCode)
{
    TraceTag(ttidControl, "%s: [%08x] Checking to see if Invoke Thread should be signalled\n", __FUNCTION__, this);
    if(m_ResetMutexRequestor == TRUE)
    {
        TraceTag(ttidControl, "%s: [%08x] Signalling Request Done Event\n", __FUNCTION__, this);
        Assert(m_state == CBGATE_REQUEST_HANDLING);
        m_state = CBGATE_REQUEST_DONE;
        SetEvent(m_hEvtRequestDone);
        m_ResetMutexRequestor = FALSE;
        m_dwReturnCode = retCode;
    }
}

LONG DispatchGate::IncRef() {return InterlockedIncrement(&m_RefCount);}
LONG DispatchGate::DecRef()
{
    LONG retVal = InterlockedDecrement(&m_RefCount);

    if(0 == retVal)
    {
        Assert(m_fShuttingDown);
        delete this;
    }
    return retVal;
}



/**
 *  FindDispatchGateForProc:
 *
 *      - global interface to retrieve DispatchGate by process handle
 *      - holds the global upnp lock when performing this call
 *
 */
DispatchGate * DispatchGate::FindDispatchGateForProc(HANDLE hProc)
{
    DispatchGate *pCBGate = NULL;

    EnterCriticalSection(&g_csUPNP);
    pCBGate = FindDispatchGateForProcInternal(hProc);
    LeaveCriticalSection(&g_csUPNP);

    return pCBGate;
}


/**
 *  AddDispatchGateForProc:
 *
 *      - global interface to add a new DispatchGate by process handle
 *      - holds the global upnp lock when performing this call
 *
 */
BOOL DispatchGate::AddDispatchGateForProc(HANDLE hProc)
{
    BOOL retCode = TRUE;
    DispatchGate *pCBGate = NULL;

    EnterCriticalSection(&g_csUPNP);
        {
        pCBGate = FindDispatchGateForProcInternal(hProc);
        if(pCBGate)
            {
            pCBGate->DecRef();
            LeaveCriticalSection(&g_csUPNP);
            return FALSE;
            }

        retCode = AddDispatchGateForProcInternal(hProc);
        }
    LeaveCriticalSection(&g_csUPNP);

    return retCode;
}



/**
 * RemoveDispatchGateForProc:
 *
 *      - global interface to remove DispatchGate by prcoess handle
 *      - holds the global upnp lock when performing this call
 *
 */
DispatchGate* DispatchGate::RemoveDispatchGateForProc(HANDLE hProc)
{
    DispatchGate *pCBGate = NULL;

    EnterCriticalSection(&g_csUPNP);
    pCBGate = FindDispatchGateForProcInternal(hProc);
    if(pCBGate) 
    {
        DispatchGate::Unlink(pCBGate);
    }
    LeaveCriticalSection(&g_csUPNP);

    return pCBGate;
}



DispatchGate * FindDispatchGateForProcInternal(HANDLE hProc)
{
    LIST_ENTRY *pLink = DispatchGate::list.Flink;
    DispatchGate *pCBGate = NULL;

    // assume lock is held
    while (pLink != &DispatchGate::list)
    {
        pCBGate = CONTAINING_RECORD(pLink, DispatchGate, m_Link);
        if (pCBGate->m_hTargetProc == hProc)
        {
            pCBGate->IncRef();
            goto Finish;
        }
        pLink = pLink->Flink;
    }
    return NULL;

Finish:
    return pCBGate;
}



BOOL AddDispatchGateForProcInternal(HANDLE hProc)
{
    // dispatch gate initial reference == 0
    DispatchGate* pCBGate = new DispatchGate(hProc);
    if(!pCBGate)
    {
        TraceTag(ttidError, "%s: OOM cannot create a new DispatchGate for Process [%08x]\n", hProc);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }

    pCBGate->IncRef();
    DispatchGate::Link(pCBGate);

    return TRUE;
}

