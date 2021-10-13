//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pch.h"
#pragma hdrstop

/*
   The DispatchGate synchronizes one or more 'requestor' threads with a single 'request handler' thread
   The requestor sequence is :
   EnterGate() - get exclusive access ( can block)
   DoCallback() - wait for the request to be dispatched by the single handler thread (usually blocks)
   LeaveGate() - release exclusive access

   The handler thread sequence  is (in a loop):
   WaitForRequests() - wait for a requestor
   DispatchRequests() - do a PerformCallback into a target process

   There is typically one DispatchGate object per process, though there could be more. There must be only
   one request handler thread calling the WaitForRequests() and DispatchRequests() method.
   
   Since the requests are handled synchronously, there is a single input request buffer
   that is reused on each callback request. 
   Output buffers, if any are allocated per callback.
   TODO: cleanup the buffer handling - its not very intuitive and requires too many copies on output
*/


DispatchGate::DispatchGate(HANDLE hTargetProc,
        PFUPNPCALLBACK pfCallback,
        PVOID pvContext,
        PBYTE pbReq,    //input buffer in Target proc 
        DWORD cbReqSize
        )
            :
            m_RefCount(1),
            m_fShuttingDown(FALSE),
            m_pfCallback(pfCallback),
            m_pvContext(pvContext),
            m_pbReqBuf(pbReq),
            m_cbReqBufSize(cbReqSize),
            m_nDescs(0),
            m_pDescs(NULL)
            //m_pbRespBuf(NULL),
           // m_cbRespBufSize(0)
{
    InitializeCriticalSection(&m_cs);
    m_state = CBGATE_INVALID;
    Assert(hTargetProc == GetCallerProcess());
    m_hTargetProc = hTargetProc;
    m_hEvtRequest = CreateEvent(NULL,FALSE,FALSE,NULL);   // auto-reset, non-signaled
    m_hEvtRequestDone = CreateEvent(NULL,FALSE,FALSE,NULL);   // auto-reset, non-signaled
    m_hMutexRequestor = CreateMutex(NULL,FALSE, NULL);   // initially signaled
    if (m_hEvtRequestDone && m_hEvtRequest && m_hMutexRequestor && m_pfCallback)
    {
        m_state = CBGATE_READY;
    }
}

DispatchGate::~DispatchGate()
{
    DWORD retry = 0;
    m_state = CBGATE_INVALID;
    m_fShuttingDown = TRUE;
    Assert(m_RefCount >= 0);
    while (m_RefCount > 0 && retry++ < 10)
    {
        SetEvent(m_hEvtRequest);
        SetEvent(m_hEvtRequestDone);
        Sleep(retry);
    }
    CloseHandle(m_hMutexRequestor);
    CloseHandle(m_hEvtRequest);
    CloseHandle(m_hEvtRequestDone);
    DeleteCriticalSection(&m_cs);
}

BOOL
DispatchGate::WaitForRequests(DWORD dwTime)
{
    DWORD dwWait;
    BOOL fRet;
    Assert(m_hTargetProc == GetCallerProcess());
    if (m_state < CBGATE_READY || m_fShuttingDown)
    {
        SetLastError(ERROR_NOT_READY);
        fRet = FALSE;   // probably shutting down..
    }
    else {

        fRet = TRUE;
        dwWait = WaitForSingleObject(m_hEvtRequest, dwTime);
        if (m_fShuttingDown)
        {   // probably shutting down..
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
    return fRet;
}

BOOL
DispatchGate::DispatchRequest()
{
	CALLBACKINFO cbi;
	BOOL fRet = TRUE;
	
    Assert(m_state == CBGATE_REQUEST_WAITING);
    Assert(m_hTargetProc == GetCallerProcess());
    m_state = CBGATE_REQUEST_HANDLING;
	
	Assert(m_pfCallback);

    TraceTag(ttidControl, "DispatchGate::DispatchRequest - TargetProc %x Request size %d\n", m_hTargetProc, m_cbReqBufUsed);
	cbi.hProc = m_hTargetProc;
	cbi.pfn = (FARPROC)m_pfCallback;
	cbi.pvArg0 = m_pvContext;

    // try/except because we are accessing memory (m_pbReqBuf) and calling code (m_pfCallback) in the application space 
	__try {
	    PBYTE *pbRespBuf = NULL;
	    DWORD cbResp = 0;
	    DWORD dwRet;
	    // Layout is
	    // struct {
	    //      DWORD nDescs;
	    //      TAG_DESC Desc[nDescs];
	    //      BYTE data1[];
	    //      BYTE data2[];
	    //          ...
	    //      }
	    
	    PDWORD pnDescs = (PDWORD)m_pbReqBuf;
	    TAG_DESC *pDesc = (TAG_DESC *) (pnDescs + 1);
	    PBYTE pbData = (PBYTE) (pDesc + m_nDescs);
	    *pnDescs = m_nDescs;
	    // serialize the tagged data descriptors into the big Request Buf
	    for (DWORD i=0; i< m_nDescs; (pbData += m_pDescs[i].cbLength), i++)
	    {
	        pDesc[i].wTag = m_pDescs[i].wTag;
	        pDesc[i].cbLength = m_pDescs[i].cbLength;
	        pDesc[i].pbData = pbData;
	        memcpy(pbData, m_pDescs[i].pbData, m_pDescs[i].cbLength);
	    }
	    Assert(m_cbReqBufUsed == (unsigned)(pbData - m_pbReqBuf));
	    
		dwRet = PerformCallBack(&cbi, m_pbReqBuf, m_cbReqBufUsed, &pbRespBuf, &cbResp);

        m_dwReturnCode = dwRet;
		
        /* not dealing with responses for now.
		if (fRet && pbRespBuf && cbResp)
		{
		    if (m_pbRespBuf)
		        delete m_pbRespBuf; // prior response was abandoned ?
		    
		    m_pbRespBuf = new BYTE [cbResp];
		    m_cbRespBufUsed = cbResp;
		    if (m_pbRespBuf)
		    {
		        memcpy(m_pbRespBuf, MapPtrToProcess(pbRespBuf, m_hTargetProc), cbResp);
		    }
		    else
		        fRet = FALSE;
		}
		*/
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		TraceTag(ttidError,"Exception in PerformCallback  hProc=%x\n",  m_hTargetProc);
		fRet = FALSE;
	}

    m_state = CBGATE_REQUEST_DONE;
    SetEvent(m_hEvtRequestDone);
    return fRet;
}


BOOL
DispatchGate::EnterGate(DWORD dwTime)
{
    DWORD dwWait;
    BOOL fRet = FALSE;
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
    TraceTag(ttidControl, "DispatchGate::EnterGate return %d\n", fRet);
    return fRet;
}

BOOL
DispatchGate::DoCallback(
    DWORD nInDescs,
    TAG_DESC *pInDescs,
    DWORD dwTimeout)
{
    DWORD dwWait;
    DWORD cbBufTotal;
    Assert(m_state >= CBGATE_ENTERED);
    if (m_state < CBGATE_ENTERED || m_fShuttingDown)
    {
        TraceTag(ttidError, "DispatchGate::DoCallback entry: error incorrect state (%d) or shutting down (%d)\n", m_state, m_fShuttingDown); 
        return FALSE;
    }

    cbBufTotal = sizeof(DWORD) + nInDescs*sizeof(TAG_DESC);
    for (DWORD i = 0; i < nInDescs;i++)
        cbBufTotal += pInDescs[i].cbLength;
    
    if (cbBufTotal > m_cbReqBufSize)
    {
        // TODO: this may need to be improved
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    m_cbReqBufUsed = cbBufTotal;

    m_pDescs = pInDescs;
    m_nDescs = nInDescs;
    
    m_state = CBGATE_REQUEST_WAITING;
    SetEvent(m_hEvtRequest);

    dwWait = WaitForSingleObject(m_hEvtRequestDone, dwTimeout);

    TraceTag(ttidControl,"DispatchGate::DoCallback RequestDone (dwWait = %d) \n", dwWait);

    m_pDescs = NULL;
    m_nDescs = 0;
    
    if (m_fShuttingDown || m_state != CBGATE_REQUEST_DONE)
    {
        TraceTag(ttidError, "DispatchGate::DoCallback exit: error state (%d) or shutting down (%d)\n", m_state, m_fShuttingDown); 
        return FALSE;
    }
    
    m_state = CBGATE_ENTERED;
    
    return m_dwReturnCode;
}

BOOL
DispatchGate::LeaveGate()
{
    Assert(m_state >= CBGATE_ENTERED);
    m_state = CBGATE_READY;
    ReleaseMutex(m_hMutexRequestor);
    TraceTag(ttidControl, "DispatchGate::LeaveGate\n");
    return TRUE;    
}

// mark the gate as shutting down and wake up the ProcessCallbacks thread so it can delete the object.
BOOL
DispatchGate::Shutdown()
{
    m_fShuttingDown = TRUE;
    SetEvent(m_hEvtRequest);
    Assert(m_RefCount >= 0);
    return TRUE;
}


#define MAX_PROCS   32
DispatchGate *g_pCBGate[MAX_PROCS];

DispatchGate * FindDispatchGateForProc(HANDLE hProc)
{
    int i;
    DispatchGate *pCBGate = NULL;
    for (i = 0; i < MAX_PROCS; i++)
    {
        if (g_pCBGate[i] && g_pCBGate[i]->GetTargetProc() == hProc)
        {
            pCBGate = g_pCBGate[i];
            pCBGate->IncRef();
            break;
        }
    }
    return (pCBGate);
}


BOOL ProcessCallbacks(PFUPNPCALLBACK pfCallback, PVOID pvContext,  PBYTE pReqBuf, DWORD cbReqBuf)
{
    HANDLE hCallerProc = GetCallerProcess();
    DispatchGate *pCBGate;
    int i;
    BOOL fRet;
    TraceTag(ttidTrace, "ProcessCallbacks entered\n");
    EnterCriticalSection(&g_csUPNP);
    pCBGate = FindDispatchGateForProc(hCallerProc);
    if (!pCBGate)
    {
        pCBGate = new DispatchGate(hCallerProc, pfCallback, pvContext, pReqBuf, cbReqBuf);
        for (i = 0; i < MAX_PROCS; i++)
        {
            if (!g_pCBGate[i])
            {
                g_pCBGate[i] = pCBGate;
                if (pCBGate)
                    pCBGate->IncRef();  // refcount = 2 (1 for global list, 1 for this thread)
                break;
            }
        }
        
        if(i >= MAX_PROCS)
        {
            TraceTag(ttidError, "Reached limit of %d processes and cannot create a new DispatchGate for Proc %x\n", MAX_PROCS, hCallerProc);
            delete pCBGate;
            
            return FALSE;
        }
    }
    LeaveCriticalSection(&g_csUPNP);
    if (!pCBGate)
    {
        TraceTag(ttidError, "Cannot create a new DispatchGate for Proc %x\n", hCallerProc);
        return FALSE;
    }
    TraceTag(ttidControl, "ProcessCallbacks: entering dispatch loop for Proc %x\n", hCallerProc);
    fRet = TRUE;
    while (fRet)
    {
        fRet = pCBGate->WaitForRequests(INFINITE);
        if (fRet)
        {
            fRet = pCBGate->DispatchRequest();
        }
    }
    TraceTag(ttidControl, "ProcessCallbacks: exiting dispatch loop for Proc %x\n", hCallerProc);

    EnterCriticalSection(&g_csUPNP);
    if (g_pCBGate[i] == pCBGate)
        g_pCBGate[i] = NULL;
    LeaveCriticalSection(&g_csUPNP);
    
    pCBGate->DecRef();
    delete pCBGate;
    
    return fRet;
    
}

BOOL CancelCallbacks()
{
    DispatchGate *pCBGate = NULL;
    int i;
    HANDLE hProc = GetCallerProcess();
    TraceTag(ttidControl, "CancelCallbacks entered for Proc %x\n", hProc);
    EnterCriticalSection(&g_csUPNP);
    for (i = 0; i < MAX_PROCS; i++)
    {
        if (g_pCBGate[i] && g_pCBGate[i]->GetTargetProc() == hProc)
        {
            pCBGate = g_pCBGate[i];
            g_pCBGate[i] = NULL;
            break;
        }
    }
    LeaveCriticalSection(&g_csUPNP);

    if (pCBGate)
    {
        BOOL fRet;
        LONG refCount;
        fRet =  pCBGate->Shutdown();    // mark the thread as shutting down
        refCount = pCBGate->DecRef();  // release the global reference
        TraceTag(ttidControl, "CancelCallbacks: DispatchGate refCount = %d\n", refCount);
        return fRet;
    }
    return FALSE;
}
