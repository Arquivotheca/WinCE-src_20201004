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
/*++


Module Name:

    tpcore.cpp

Abstract:

    Thread pool core implementaiton


--*/

#include "cetp.hpp"
#include <NKpriv.h>
#include <errorrep.h>

// use variable here in case we want to make it configurable.
LONG g_ThreadMaxLimit = TP_THRD_MAX_LIMIT;
LONG g_ThreadMaxDefault = TP_THRD_DFLT_MAX;

#define TP_THRD_STACKSIZE           0x20000                 // thread pool default stack size set to 128k

void LogTPCommon(WORD wID, PTP_ITEM pTpItem)
{
    if (IsLoggingEnabled ()) {
        CEL_TP_ITEM cti;

        cti.pPool                   = pTpItem->pPool;
        cti.wState                  = pTpItem->wState;
        cti.wType                   = pTpItem->wType;
        cti.hModule                 = pTpItem->hModule;
        cti.Context                 = pTpItem->Context;
        cti.DoneEvent               = pTpItem->DoneEvent;
        cti.dwCallbackInProcess     = pTpItem->dwCallbackInProcess;
        cti.dwCallbackRemaining     = pTpItem->dwCallbackRemaining;
        cti.dwDueTime               = pTpItem->dwDueTime;
        cti.dwPeriod                = pTpItem->dwPeriod;
        cti.hObject                 = pTpItem->hObject;
        cti.hCompletionEvent        = pTpItem->hCompletionEvent;
        cti.dwCookie                = pTpItem->dwCookie;
        cti.Callback                = pTpItem->Callback;
        
        CeLogData (
            TRUE,
            wID,
            &cti,
            sizeof (cti),
            0,
            TpCeLogZone (),
            0,
            FALSE);
    }
}


// struct used in enumeration function to expire threads
typedef struct {
    PThreadPool pPool;
    DWORD       dwCurrTime;
    DWORD       dwTimeout;
} ENUMEXPIRESTRUCT, *PENUMEXPIRESTRUCT;

DWORD GetWaitTimeInMilliseconds (PFILETIME pllftDue)
{
    DWORD dwWaitTime = INFINITE;
    if (pllftDue) {
        LONGLONG llftDue = *(PLONGLONG)pllftDue;

        if (llftDue <= 0) {
            // relative time, convert to +msec (round up from 100ns unit to ms)
            dwWaitTime = FileTimeToMillisecond (-llftDue); 

        } else {
            // absolute time, convert delay to msec
            LONGLONG llftNow;
            
            GetCurrentFT ((PFILETIME) &llftNow);

            if (llftDue <= llftNow) {
                dwWaitTime = 0;
            } else {
                dwWaitTime = FileTimeToMillisecond (llftDue - llftNow);
            }
        }
        if (dwWaitTime > MAX_THREADPOOL_WAIT_TIME) {
            dwWaitTime = MAX_THREADPOOL_WAIT_TIME;
        }
    }

    return dwWaitTime;
}

typedef struct {
    DLIST dlDeactived;
    DLIST dlInProgress;
    DWORD dwCookie;
    HANDLE hCleanupEvent;
} ENUMMATCHSTRUCT, *PENUMMATCHSTRUCT;

static BOOL EnRemoveCookie (PDLIST pItem, LPVOID pEnumData)
{
    PENUMMATCHSTRUCT pems = (PENUMMATCHSTRUCT) pEnumData;
    PTP_ITEM      pTpItem = (PTP_ITEM) pItem;
    if (pTpItem->dwCookie == pems->dwCookie) {
        pTpItem->dwCallbackRemaining = 0;
        pTpItem->wState &= ~TP_ITEM_STATE_ACTIVATED;
        
        RemoveDList (pItem);

        if (!pTpItem->dwCallbackInProcess) {
            AddToDListTail (&pems->dlDeactived, pItem);
        } else {
            AddToDListTail (&pems->dlInProgress, pItem);
            pTpItem->wState |= TP_ITEM_STATE_DELETE;
            pTpItem->hCompletionEvent = pems->hCleanupEvent;
        }
    }
    return FALSE;   // keep enumerating
}

//
// check if a callback instantance is valid
// 
void ValidateCallbackInstance (PTP_CALLBACK_INSTANCE pci)
{
    PTP_ITEM pTpItem;
    // pci must be on stack of current thread.
    if (((DWORD) pci > (DWORD) &pTpItem) && ((DWORD) pci < (DWORD) UTlsPtr())) {
    
        pTpItem = pci->pTpItem;

        if (!pTpItem->pPool) {
            // the functions that call this function are all "void" funciton.
            // the only way to report error is via exception
            RaiseException ((DWORD) STATUS_INVALID_PARAMETER, 0, 0, NULL);
        }
    }
}

//
// dList enumeration function
// - delete a timer node
//
// Note: list cs is NOT held when calling this
//
BOOL EnDeleteItem (
    PDLIST pItem,
    LPVOID pENumData
    )
{
    InitDList (pItem);  // DestroyDList already remove the item from queue,
                        // need to reset it as TpDeleteItem will call RemoveDList.
    ((CThreadPool*) pENumData)->TpDeleteItem (pItem);
    return FALSE; // to continue enumeration
}

//
// dList enumeration function
// - shutdown a pool thread (phase 1)
//
// Note: list cs is held when calling this
//
BOOL EnSignalExit ( 
    PDLIST pItem, 
    LPVOID pEnumData 
    )
{
    PTP_THRD pThrd = (PTP_THRD) pItem; 
    pThrd->Flags |= TP_THRD_FLAGS_EXIT;
    NKResumePriv (pThrd->ThreadHandle);
    return FALSE; // to continue enumeration
}

/*++

    Routine Description:
        signal an idle thread to exit. This is called to
        trim the thread pool if no work has
        been scheduled for some time.

    Arguments:
        Thread to exit
        pdlExpired - the list to queue the thread to.

    Return Value:

    --*/
void SignalThreadToExit (
    PTP_THRD pThrd
    )
{        
    LONG Flags = pThrd->Flags;

    ASSERT (!(Flags & TP_THRD_FLAGS_EXIT));
    
    // thread state = dying
    pThrd->Flags &= ~(TP_THRD_FLAGS_IDLE | TP_THRD_FLAGS_SUSP);
    pThrd->Flags |= TP_THRD_FLAGS_EXIT;

    // remove the thread from active list
    RemoveDList (&pThrd->Link);
    InitDList (&pThrd->Link);
    
    // resume the thread if needed
    if (Flags & TP_THRD_FLAGS_SUSP) {
        NKResumePriv (pThrd->ThreadHandle);
    }             
}


/*++

    Routine Description:
        Resume an idle thread to pickup the 1st callback to execute.
        There MUST BE at least one callback ready to be executed. 

    Arguments:
        Thread to resume

    Return Value:

    --*/
void CThreadPool::ResumeThrd (
    PTP_THRD pThrd
    )
{
    PTP_ITEM pTpItem = (PTP_ITEM) m_dlExpiredWork.pFwd;
    LONG     Flags   = pThrd->Flags;

    ASSERT (!(Flags & TP_THRD_FLAGS_EXIT));
    ASSERT (IsPoolCsOwned ());
    ASSERT (!IsDListEmpty (&m_dlExpiredWork));

    // move thread to active end
    RemoveDList (&pThrd->Link);
    AddToDListTail (&m_dlActiveThrds, &pThrd->Link);

    // associate the timer with the thread
    pThrd->pTpItem = pTpItem;
    
    // decrement # of callback remaining
    DecrementCallbackRemaining (pTpItem);

    // increment # of callback in progress
    IncrementCallbackInProgress (pTpItem);
    
    // thread state = busy + not suspended
    pThrd->Flags &= ~(TP_THRD_FLAGS_SUSP | TP_THRD_FLAGS_IDLE);

    // resume thread
    if (Flags & TP_THRD_FLAGS_SUSP) {
        NKResumePriv (pThrd->ThreadHandle);
    }             
}

/*++

    Routine Description:
        Schedule any timers in the expired
        queue.

    Arguments: 
        None
        
    Return Value:  
        None
        
    --*/
BOOL CThreadPool::ProcessExpiredItems ()
{
    ASSERT (IsPoolCsOwned());

    while (!IsDListEmpty (&m_dlExpiredWork)) {
        PTP_THRD pThrd = GetIdleThread ();
        if (!pThrd) {
            break;
        }
        ResumeThrd (pThrd);                    
    }
    return !IsDListEmpty (&m_dlExpiredWork);
}

/*++

    Routine Description:
        Process an expired thread pool item.

    Arguments:
        Worker thread executing this timer/work
        Timer/work node

    Return Value:        

    --*/
void CThreadPool::ProcessItem (PTP_THRD pThrd, PTP_ITEM pTpItem)
{
    TP_CALLBACK_INSTANCE cbi     = { pTpItem };
    
    ASSERT (!IsPoolCsOwned());
    ASSERT (pTpItem->dwCallbackInProcess);

#ifndef SHIP_BUILD    
    // for debugging purposes, cache the last time we ran and what callback we made
    pThrd->LastCallback = pTpItem->Callback;        
    pThrd->LastTickCount = GetTickCount();
#endif
    LogTPCallbackStart(pTpItem);

    switch (pTpItem->wType) {
    case TP_TYPE_USER_WORK:
        pTpItem->wState |= TP_ITEM_STATE_DELETE;        // destory item after callback
                                                        // NOTE: safe to access Flags with lock as we're the only thread who can access it.
        pTpItem->UserWorkCallback (pTpItem->Context);
        break;        
    case TP_TYPE_SIMPLE:
        pTpItem->wState |= TP_ITEM_STATE_DELETE;        // destory item after callback
                                                        // NOTE: safe to access Flags with lock as we're the only thread who can access it.
        pTpItem->SimpleCallback (&cbi, pTpItem->Context);
        break;

    case TP_TYPE_TIMER:
        pTpItem->TimerCallback (&cbi, pTpItem->Context, (PTP_TIMER) pTpItem);
        break;
        
    case TP_TYPE_WORK:
        pTpItem->WorkCallback (&cbi, pTpItem->Context, (PTP_WORK)pTpItem);
        break;

    case TP_TYPE_WAIT:
        pTpItem->WaitCallback (&cbi, pTpItem->Context, (PTP_WAIT) pTpItem, pTpItem->dwWaitResult);
        break;
        
    case TP_TYPE_TIMERQ_TIMER:
        pTpItem->WaitOrTimerCallback (pTpItem->Context, TRUE);
        break;
    case TP_TYPE_REGWAIT:
    case TP_TYPE_REGWAIT_ONCE:
        pTpItem->WaitOrTimerCallback (pTpItem->Context, WAIT_TIMEOUT == pTpItem->dwWaitResult);
        break;

    default:
        ASSERT(0);
        break;
    }


    // handle WhateverWhenCallbackReturn API called during callback
    if (cbi.pcs) {
        LeaveCriticalSection (cbi.pcs);
    }

    if (cbi.hMutex) {
        ReleaseMutex (cbi.hMutex);
    }

    if (cbi.hEvent) {
        SetEvent (cbi.hEvent);
    }

    if (cbi.hSemaphore) {
        ReleaseSemaphore (cbi.hSemaphore, cbi.lSemRelCnt, NULL);
    }

    if (cbi.hModule) {
        FreeLibrary (cbi.hModule);
    }

    LogTPCallbackStop();
}

BOOL CThreadPool::EnExpireItem (PDLIST pdl, LPVOID pParam)
{
    PTP_ITEM pTpItem = (PTP_ITEM)pdl;
    PENUMEXPIRESTRUCT pes = (PENUMEXPIRESTRUCT) pParam;
    LONG lTimeLeft = (LONG)(pTpItem->dwDueTime - pes->dwCurrTime);

    if (lTimeLeft > 0) {
        pes->dwTimeout = (DWORD) lTimeLeft;
    } else {
        pes->pPool->IncrementCallbackRemaining (pTpItem);
        pTpItem->dwDueTime += pTpItem->dwPeriod;
    }

    return lTimeLeft > 0;
}

//
// dList enumeration function
// - destroy a wait group
//
BOOL CThreadPool::EnDestroyWaitGroup (PDLIST pdl, LPVOID)
{
    CThreadPoolWaitGroup *pWaitGroup = (CThreadPoolWaitGroup *) pdl;
    pWaitGroup->Destroy ();
    return FALSE;
}


//
// dList enumeration function
// - try to reserve a wait spot in a wait group
//
BOOL CThreadPool::EnReserveWait (PDLIST pdl, LPVOID)
{
    CThreadPoolWaitGroup *pWaitgrp = (CThreadPoolWaitGroup *) pdl;

    return pWaitgrp->ReserveWait ();
}

/*++

    Routine Description:
        Scan and process all expired work items.
        If possible schedule the work items on any
        available idle threads immediately; if not
        create additional pool threads to process
        work.

    Arguments:
        ptr to dword to receive new timeout
        
    Return Value:        
        - # of worker threads to create to service 
        expired work

    --*/
BOOL CThreadPool::ExpireItems (LPDWORD pdwTimeout)
{
    ASSERT (IsPoolCsOwned());

    ENUMEXPIRESTRUCT es = { this, GetTickCount (), INFINITE };

    EnumerateDList (&m_dlPendingWork, EnExpireItem, &es);

    *pdwTimeout = es.dwTimeout;

    return ProcessExpiredItems ();
}

/*++

    Routine Description:
        Insert the given timer in pending work list
        at the right slot (sorted by delay).

    Arguments: 
        thread pool item
        Delay after which to schedule the timer

    Return Value:  
        TRUE - if timer is inserted at head of list
        FALSE - otherwise
        
    --*/
BOOL CThreadPool::InsertItem (PTP_ITEM pTpItem)
{
    ASSERT (IsPoolCsOwned ());

    RemoveDList (&pTpItem->Link);

    PDLIST pTrav = m_dlPendingWork.pFwd;

    pTpItem->wState |= TP_ITEM_STATE_ACTIVATED;
    
    if (!IsDListEmpty(&m_dlPendingWork)) {
        // add in the sorted order of expiry time            
        do {
            if ((LONG)(((PTP_ITEM)pTrav)->dwDueTime - pTpItem->dwDueTime) > 0) {
                break;
            }
            pTrav = pTrav->pFwd;
        } while (pTrav != &m_dlPendingWork); 
    }
    AddToDListTail (pTrav, &pTpItem->Link);
    return &m_dlPendingWork == pTpItem->Link.pBack;
}
    
/*++

    Routine Description:
        Scan and retire all idle threads beyond
        the minimum # of threads required
        in the pool.

    Arguments:            

    Return Value:                   
        
    --*/
void CThreadPool::RetireExcessIdleThreads ()
{       
    PDLIST pTrav;
    LONG lExpiredThrds = 0;
    LONG cMinThreads = m_MinThrdCnt;

    ASSERT (IsPoolCsOwned());

    // first skip # of required (minimum) active threads in the pool
    // scan backwards as busy threads are at the end of the list
    // we want to expire idle threads so scan busy threads first
    for (pTrav = m_dlActiveThrds.pBack; ((pTrav != &m_dlActiveThrds) && cMinThreads); pTrav = pTrav->pBack) {
        cMinThreads--;
    }

    // of the remaining threads, expire all idle threads
    while (pTrav != &m_dlActiveThrds) {
        PDLIST pPrev = pTrav->pBack;
        if (IsPoolThrdIdle((PTP_THRD)pTrav)) {
            SignalThreadToExit ((PTP_THRD)pTrav);
            m_CurThrdCnt--;
            lExpiredThrds++;
        }            
        pTrav = pPrev;
    }        

    DEBUGMSG ((DBGTP && lExpiredThrds), (L"CETP Expired Thread Pool: 0x%8.8lx Count: %d\r\n", this, lExpiredThrds));                     
}    

//
// post callback handling 
//
HANDLE CThreadPool::PostCallbackProcess (PTP_ITEM pTpItem)
{
    // post callback handling
    HANDLE hEvt = NULL;
    
    ASSERT (IsPoolCsOwned ());
        
    if (DecrementCallbackInProgress (pTpItem)) {
        // no more callback pending, clean up.
        
        BOOL fIsWait = (pTpItem->pPool != (PTP_POOL) this);

        // completion event is coming from external. It should only be set after
        // post callback completed. Since we can delete the item if it's marked
        // for deletion, we need to save a local copy.
        HANDLE hCompeletionEvent = pTpItem->hCompletionEvent;
        pTpItem->hCompletionEvent = NULL;

        SetEvent (pTpItem->DoneEvent);

        if (pTpItem->wState & TP_ITEM_STATE_DELETE) {
            // item should be freed
            if (fIsWait) {
                ((CThreadPoolWaitGroup *) pTpItem->pPool)->ReleaseWait ();
            }
            TpDeleteItem (pTpItem);
        } else {

            if (fIsWait) {
                // reactivate if hObject != NULL
                if (pTpItem->hObject) {
                    hEvt = ((CThreadPoolWaitGroup*)pTpItem->pPool)->ReactivateWait (pTpItem);
                } else {
                    // clear active
                    pTpItem->wState &= ~TP_ITEM_STATE_ACTIVATED;
                }
            } else {
                // reactivate if periodic
                if (!pTpItem->dwPeriod && !(pTpItem->wState & TP_ITEM_DUE_TIME_VALID)) {
                    // clear active
                    pTpItem->wState &= ~TP_ITEM_STATE_ACTIVATED;
                    
                } else {
                    pTpItem->wState &= ~TP_ITEM_DUE_TIME_VALID; // clear the 1-shot valid flag

                    if (InsertItem (pTpItem)) {
                        // timeout smaller than existing, signal driver thread to process
                        hEvt = m_hEvtStateChanged;
                    }
                }
            }
        }
        if (hCompeletionEvent) {
            SetEvent (hCompeletionEvent);
        }
    }

    return hEvt;
}
    


/*++

    Routine Description:
        Thread proc used by all threads in the pool 
        waiting for work.

    Arguments:
        Thread node
        
    Return Value:      

    --*/
void CThreadPool::CallbackThread (PTP_THRD pThrd)
{
    BOOL fContinue = TRUE;
    PTP_ITEM pTpItem = NULL;
    InterlockedIncrement (&m_nCallbackInProgress);
    do {
        HANDLE hEvt = NULL;
        Lock ();

        if (pTpItem) {
            hEvt = PostCallbackProcess (pTpItem);
            pTpItem = NULL;
        }
        
        // exit check (has to be the first check after acquiring the list cs lock)
        fContinue = !(pThrd->Flags & TP_THRD_FLAGS_EXIT);

        if (fContinue) {   
            if (pThrd->pTpItem) {
                // thread scheduled with a timer/work
                pTpItem = pThrd->pTpItem;
                pThrd->pTpItem = NULL;
                ASSERT (!(pThrd->Flags & (TP_THRD_FLAGS_IDLE | TP_THRD_FLAGS_SUSP)));                    
                
            } else if (!IsDListEmpty(&m_dlExpiredWork)) {
                // pop the expired work
                ASSERT (!(pThrd->Flags & TP_THRD_FLAGS_SUSP));
                ResumeThrd (pThrd);
                pTpItem = pThrd->pTpItem;
                pThrd->pTpItem = NULL;

            } else {
                // move thread to free end of queue (head)
                pThrd->Flags |= (TP_THRD_FLAGS_IDLE | TP_THRD_FLAGS_SUSP);
                if (pThrd->Link.pBack != &m_dlActiveThrds) {
                    RemoveDList (&pThrd->Link);
                    AddToDListHead (&m_dlActiveThrds, &pThrd->Link);
                }
            }
        } else {
            RemoveDList (&pThrd->Link);
            InitDList (&pThrd->Link);
        }

        Unlock ();

        if (hEvt) {
            // post callback process requires signaling an event
            SetEvent (hEvt);
        }

        // either process timer/work or suspend self
        if (pTpItem) {  

            if (IsKModeAddr ((DWORD) &fContinue)) {
                // for kernel thread pool, an exception doesn't reboot, only the thread got terminated.
                // if we don't try-except the call, the thread pool will be messed up and the whole OS
                // will be in unknow state.
                __try {
                    ProcessItem (pThrd, pTpItem);
                } __except (ReportFault (GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
                    // no-op for now. Might want to consider forcing the thread to exit.
                }
                
            } else {
                // for user mode, any exception while doing the callback will cause the exe to exit. 
                // no need to try/except there.
                ProcessItem (pThrd, pTpItem);
            }

        } else if (fContinue) {
            if (!InterlockedDecrement (&m_nCallbackInProgress)
                && IsPoolAboveTrim ()) {
                // signal driver thread to trim idle threads
                SetEvent (m_hEvtStateChanged);
            }
            NKSuspendPriv (GetCurrentThread());
            InterlockedIncrement (&m_nCallbackInProgress);
        }
    } while (fContinue);
    
    InterlockedDecrement (&m_nCallbackInProgress);
}    

void CThreadPool::CreateNewThreads ()
{
    ASSERT (!IsPoolCsOwned ());
    
    BOOL fNeedMoreThreads = FALSE;
    DEBUGMSG ((DBGTP && fNeedMoreThreads), (L"CETP Queue Full Pool: 0x%8.8lx created new threads!\r\n", this));
    while (NewCallbackThrd (&fNeedMoreThreads) && fNeedMoreThreads) {
        // empty body
    }
}


#define MIN_WAIT_TIME_TO_TRIM       5000        // seconds

/*++

    Routine Description:
        driver thread which:
        - delegates work 
        - shuts down pool threads
        - creates/trims threads if needed

    Arguments:
        None

    Return Value:     

    --*/
void CThreadPool::DriverThread ()
{
    DWORD dwTimeout = INFINITE;
    
    for ( ; ; ) {
        DWORD dwWaitResult = WaitForSingleObject (m_hEvtStateChanged, dwTimeout);

        if (WAIT_FAILED == dwWaitResult) {
            ASSERT (0);
            break;
        }

        if (m_lState & TP_STATE_DYING) {
            break;
        }

        // scan all pending work nodes
        Lock (); 

        BOOL fNewThreadsNeeded = ExpireItems (&dwTimeout);

        if (!fNewThreadsNeeded                      // no unprocessed callback
            && !m_nCallbackInProgress               // no callback in progress
            && (dwTimeout > MIN_WAIT_TIME_TO_TRIM)  // timeout greater than 5 seconds
            && IsPoolAboveTrim ()) {                // we have too many threads
            // get rid of excess idle threads
            RetireExcessIdleThreads ();
        }
        Unlock ();

        if (fNewThreadsNeeded) {
            // short of pool threads - create new threads
            CreateNewThreads ();
        }
    }

    ASSERT (!IsPoolActive());

    // destroy all wait groups (no lock required as the pool is already deactivated and there can't be more waitgroup created)
    DestroyDList (&m_dlWaitGroup, EnDestroyWaitGroup, NULL);

    // no need to acquire Poolcs as we're single threaded here (all worker thread had exited)
    
    // delete all work nodes and signal pool threads to exit                                
    DestroyDList(&m_dlPendingWork, EnDeleteItem, this);
    DestroyDList(&m_dlExpiredWork, EnDeleteItem, this);
    DestroyDList(&m_dlInactive,    EnDeleteItem, this);

}    

/*++

    Routine Description:
        Create a new thread in the thread pool.

    Arguments:
        
    Return Value:        
        TRUE - thread created
        FALSE - thread not created

    --*/
BOOL CThreadPool::NewCallbackThrd (PBOOL pfNeedMoreThreads)
{   
    HTHREAD hThread = NULL;

    ASSERT (!IsPoolCsOwned());
    
    if (IsPoolActive() && (m_CurThrdCnt < m_MaxThrdCnt)) {            
        PTP_THRD pThrd = (PTP_THRD) LocalAlloc (LMEM_FIXED, sizeof(TP_THRD));        
        
        if (pThrd) { 
            InitDList (&pThrd->Link);  
            pThrd->pTpItem          = NULL;                
            pThrd->Pool             = (PTP_POOL)this;
            pThrd->Flags            = TP_THRD_FLAGS_IDLE; 
#ifndef SHIP_BUILD
            pThrd->LastTickCount    = 0;
            pThrd->LastCallback     = NULL;
#endif

            hThread = CreateThread(NULL, m_cbStackReserve, CallbackThreadStub, pThrd, CREATE_SUSPENDED|THRD_NO_BASE_FUNCTION|STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
            if (!hThread) {
                LocalFree (pThrd);

            } else {
                pThrd->ThreadHandle = hThread;
                
                Lock ();

                if (IsPoolActive() && (m_CurThrdCnt < m_MaxThrdCnt)) {            
                    // pool is below max limit; add thread to the pool
                    m_CurThrdCnt++;

                    // one more thread accessing this object
                    IncRef ();
                    
                    if (IsDListEmpty (&m_dlExpiredWork)) {
                        // add it to idle side of the list
                        AddToDListHead (&m_dlActiveThrds, &pThrd->Link);
                    } else {
                        // assign the item to the thread
                        ResumeThrd (pThrd);
                    }

                    if (pfNeedMoreThreads) {
                        *pfNeedMoreThreads = !IsDListEmpty (&m_dlExpiredWork) && (m_CurThrdCnt < m_MaxThrdCnt);
                    }

                } else {
                    // don't add the thread to the pool as we are above limits; exit the thread
                    pThrd->Pool = NULL;
                    hThread = NULL;
                }
                
                Unlock ();
                
                ResumeThread (pThrd->ThreadHandle, NULL);                
            }
        }
    }

    return (NULL != hThread);
}

/*++

    Routine Description:
        Destructor (protected to disable calls to this from external)

    Arguments:

    Return Value:        

    --*/
CThreadPool::~CThreadPool (void) 
{

    ASSERT (!m_lRefCnt);
    if (m_hEvtStateChanged) {
        CloseHandle (m_hEvtStateChanged);
    }
    if (m_hEvtThrdExited) {
        CloseHandle (m_hEvtThrdExited);
    }

    DeleteCriticalSection (&m_PoolCs);
    
    ASSERT (IsDListEmpty(&m_dlActiveThrds));
    ASSERT (IsDListEmpty(&m_dlPendingWork));
    ASSERT (IsDListEmpty(&m_dlExpiredWork));
    ASSERT (IsDListEmpty(&m_dlInactive));
    ASSERT (IsDListEmpty(&m_dlWaitGroup));
}
    
/*++

    Routine Description:
        Constructor

    Arguments:

    Return Value:        

    --*/
CThreadPool::CThreadPool (void)
{
    memset (this, 0, sizeof(CThreadPool));

    m_MaxThrdCnt     = TP_THRD_DFLT_MAX; 
    m_MinThrdCnt     = TP_THRD_DFLT_MIN;
    m_cbStackCommit  = VM_PAGE_SIZE;
    m_cbStackReserve = TP_THRD_STACKSIZE;
    m_lRefCnt        = 1;

    InitDList (&m_dlActiveThrds);
    
    InitDList (&m_dlPendingWork);
    InitDList (&m_dlExpiredWork);
    InitDList (&m_dlWaitGroup);
    InitDList (&m_dlInactive);

    InitializeCriticalSection (&m_PoolCs);
    m_hEvtStateChanged = CreateEvent (NULL, 0, 0, NULL);
    m_hEvtThrdExited   = CreateEvent (NULL, 0, 0, NULL);
    
    // if any of the events failed to create, IsInitialized() will return
    // failure and caller will delete the pool in that case.
}

/*++

    Routine Description:
        Mark the pool state as inactive and signal driver 
        thread to exit. driver thread will start the shutdown 
        process for all the pool threads.

        Shutdown of the pool is always done in an 
        asynchronous mode where all the worker threads
        are signaled for shutdown and driver thread waits
        for these threads to shut down before deleting the
        pool object itself.

    Arguments:    

    Return Value: 

    --*/
void CThreadPool::Shutdown (void)
{     
    DWORD dwDriverThreadId;
    
    Lock ();
    
    m_lState |= TP_STATE_DYING;   // no driver thread create after this
    m_lState &= ~TP_STATE_ACTIVE; // no timer queue after this
    
    dwDriverThreadId = m_dwDriverThreadId;

    if (dwDriverThreadId) {
        IncRef ();                  // make sure thread pool object doesn't get destroyed.
    }
    // signal all worker threads to exit
    EnumerateDList (&m_dlActiveThrds, EnSignalExit, NULL);
    
    Unlock ();
    
    if (dwDriverThreadId) {

        //
        // signal driver thread to exit
        //

        SetEvent (m_hEvtStateChanged);

        // wait until no callback in progress
        while (m_nCallbackInProgress) {
            WaitForSingleObject (m_hEvtThrdExited, INFINITE);
        }

        DecRef ();

    } else {

        //
        // driver thread is not created; manually delete
        // rest of the pool
        //

        delete this;
    }
}

/*++

    Routine Description:
        Activate the thread pool. Create driver thread
        if this is the first time the pool is activated.

    Arguments:
        None

    Return Value:        
        TRUE - pool is activated
        FALSE - pool failed to activate or shutting down

    --*/
BOOL CThreadPool::Activate (void)
{               
    if (IsInitialized() && !m_dwDriverThreadId) {

        HTHREAD hThread = NULL;
        
        Lock ();

        // if pool is not shutting down and we haven't created driver thread yet, proceed
        if (!m_dwDriverThreadId && !(m_lState & TP_STATE_DYING)) {
        
            DWORD dwThreadId;
            hThread = CreateThread(NULL, 0, DriverThreadStub, this, CREATE_SUSPENDED|THRD_NO_BASE_FUNCTION, &dwThreadId);
            if (hThread) {
                m_lState |= TP_STATE_ACTIVE; // timers enabled from now on                    
                m_dwDriverThreadId = dwThreadId;
            }
        }

        Unlock ();

        if (hThread) {
            ResumeThread (hThread, NULL);
            CloseHandle (hThread);                    
        }
    }

    return (IsPoolActive());
}
    
/*++

    Routine Description:
        Set the pool thread stack max and commit bytes

    Arguments:
        max stack memory to reserve per thread
        min stack memory to commit per thread

    Return Value:            

    --*/
void CThreadPool::SetStackInfo (DWORD cbReserve, DWORD cbCommit)
{
    Lock ();
    m_cbStackCommit = cbCommit;
    m_cbStackReserve = cbReserve;
    Unlock ();
}

/*++

    Routine Description:
        Get the pool thread stack max and commit bytes

    Arguments:
        [out] stack reserve and commit values

    Return Value:            

    --*/
void CThreadPool::GetStackInfo (PDWORD pcbReserve, PDWORD pcbCommit)
{
    Lock ();    
    *pcbReserve = m_cbStackReserve;
    *pcbCommit = m_cbStackCommit;
    Unlock ();
}

/*++

    Routine Description:
        Set the minimum number of threads and if needed
        also create driver thread in the pool.

        If the pool was not active before this call, we will
        try to create the driver thread when this is called.

    Arguments:
        Count of min threads

    Return Value:
        TRUE - pool is activated with given min threads
        FALSE - pool failed to activate with given min threads

    --*/
BOOL CThreadPool::SetThrdMin (LONG cThrdMin)
{
    BOOL fRet = Activate();
    
    if (cThrdMin < TP_THRD_DFLT_MIN)
        cThrdMin = TP_THRD_DFLT_MIN;

    Lock ();    

    // update the min
    LONG lPrevMin = m_MinThrdCnt;
    m_MinThrdCnt = cThrdMin;

    // update the max if needed
    if (m_MaxThrdCnt < cThrdMin) {
        m_MaxThrdCnt = cThrdMin;
    }
    
    Unlock ();

    // create additional pool threads if needed
    if (fRet) {
        for (int i = 0; i < (cThrdMin - lPrevMin); ++i) {
            fRet = NewCallbackThrd (NULL);
            if (!fRet) {
                break;
            }
        }
    }
    
    return fRet;
}    

/*++

    Routine Description:
        Set the maximum number of threads

    Arguments:
        Count of max threads

    Return Value:            

    --*/
void CThreadPool::SetThrdMax (LONG cThrdMost)
{
    if (cThrdMost > g_ThreadMaxLimit)
        cThrdMost = g_ThreadMaxLimit;

    Lock ();    

    m_MaxThrdCnt = cThrdMost;

    // update the min if needed
    if (m_MinThrdCnt > cThrdMost) {
        m_MinThrdCnt = cThrdMost;
    }

    Unlock ();
}    

/*++

    Routine Description:
        Add work item to the sorted list to expire after
        a given delay.

        msDelay=0 is the most common usage of this
        function. optimize that case.

    Arguments:
        Item to add
        Delay after which to expire this timer
        If periodic, period interval to reschedule this timer

    Return Value: 
        TRUE - timer is queued
        FALSE - timer is not queued

    --*/    
BOOL CThreadPool::EnqueueItem (
    PTP_ITEM pTpItem,
    DWORD msDelay,
    DWORD msPeriod
    )
{ 
    BOOL fRet = Activate(); // if needed, create driver thread

    ASSERT (!IsPoolCsOwned());

    if (fRet) {
        BOOL fSignal = FALSE;
        BOOL fCreateNewThread = FALSE;

        Lock ();    

        fRet = IsPoolActive ();
        
        if (fRet) {
            // set up periodic timer value
            pTpItem->dwPeriod  = msPeriod;

            if (!msDelay) {
                // expire now
                IncrementCallbackRemaining (pTpItem);
                if (msPeriod) {
                    pTpItem->dwDueTime = GetTickCount () + msPeriod;
                }

                fCreateNewThread = ProcessExpiredItems ();
            } else {
                // expire later
                pTpItem->dwDueTime = GetTickCount () + msDelay;

                if (!IsCallbackPending (pTpItem)) {
                    fSignal = InsertItem(pTpItem);
                } else {
                    // set a one-shot flag indicating duetime is valid and should be queued after callback
                    pTpItem->wState |= TP_ITEM_DUE_TIME_VALID;
                }
                
            }
        }

        Unlock ();

        if (fCreateNewThread) {
            NewCallbackThrd (NULL);
        }
        
        if (fSignal) {
            SetEvent (m_hEvtStateChanged);
        }
    }
    
    return fRet;
}

/*++

    Routine Description:
        This function removes a scheduled thread run from the queue.
        If the scheduled run has already expired, then the work node
        is not removed. This only removes pending work item.

    Arguments:
        pTpItem - item to remove
        hCompletionEvent - event to signal when complete
        fCancelPending - cancel expired but yet to started callback
        fDelete - delete the timer node


    Return Value:
        TRUE - timer is de-queued
        FALSE - timer is not de-queued (either expired or not found)

    --*/
BOOL CThreadPool::DequeueItem (
    PTP_ITEM pTpItem,
    HANDLE hCompletionEvent,
    BOOL fCancelPending,
    BOOL fDelete
    )
{               

    Lock ();    

    // clear the object field
    pTpItem->hObject = NULL;

    // clear period field
    pTpItem->dwPeriod = 0;

    if (fCancelPending && pTpItem->dwCallbackRemaining) {
        pTpItem->dwCallbackRemaining = 0;
        RemoveDList (&pTpItem->Link);
        AddToDListTail (&m_dlInactive, &pTpItem->Link);
    }

    BOOL fDequeued = !IsCallbackPending (pTpItem);

    pTpItem->wState &= ~(TP_ITEM_STATE_ACTIVATED|TP_ITEM_DUE_TIME_VALID);

    if (fDequeued) {
        // callback not in progress

        RemoveDList (&pTpItem->Link);
        AddToDListTail (&m_dlInactive, &pTpItem->Link);
        SetEvent (pTpItem->DoneEvent);

    } else {
        // callback in progress

        if (INVALID_HANDLE_VALUE != hCompletionEvent) {
            pTpItem->hCompletionEvent = hCompletionEvent;
        }

        if (fDelete) {
            // set the "delete flag" and delete the item after callback
            pTpItem->wState |= TP_ITEM_STATE_DELETE;
            fDelete = FALSE;        // can't delete on the spot, will be deleted after callback complete
        }
    }

    if (fDelete) {
        TpDeleteItem (pTpItem);
        pTpItem = NULL;     // to catch de-reference to pTpObj after delete
    }

    Unlock ();

    return fDequeued;

}


// 
// Delete all items of the same cookie
//  
void CThreadPool::DeleteAllItems (
    DWORD dwCookie,
    PDLIST pdlInProgress,       // DLIST header to receive the list of callback in progress
    HANDLE hCleanupEvent        // internal event to be signaled for cleanup (i.e. whenever an inprogress callback completed)
    )
{
    ENUMMATCHSTRUCT ems;
    InitDList (&ems.dlDeactived);
    InitDList (&ems.dlInProgress);
    ems.dwCookie = dwCookie;
    ems.hCleanupEvent = hCleanupEvent;

    Lock ();
    EnumerateDList (&m_dlExpiredWork, EnRemoveCookie, &ems);
    EnumerateDList (&m_dlPendingWork, EnRemoveCookie, &ems);
    EnumerateDList (&m_dlInactive,    EnRemoveCookie, &ems);
    Unlock ();

    // destroy the deactivated list
    DestroyDList (&ems.dlDeactived, EnDeleteItem, this);

    // return the the in-progress DLIST
    MergeDList (pdlInProgress, &ems.dlInProgress);
}


/*++

    Routine Description:
        This function finds a wait group that can take addition
        "wait" and reserve it. Create a new wait group if it can't 
        find any.

    Arguments:
        none

    Return Value:
        the wait group found/created. NULL if failed.        
--*/
CThreadPoolWaitGroup *CThreadPool::ReserveWaitInWaitGroup ()
{
    CThreadPoolWaitGroup *pWaitgGrp = NULL;
    
    Lock ();    

    if (IsPoolActive ()) {
        pWaitgGrp = (CThreadPoolWaitGroup *) EnumerateDList (&m_dlWaitGroup, EnReserveWait, NULL);

        if (!pWaitgGrp) {
            pWaitgGrp = new CThreadPoolWaitGroup;
            if (pWaitgGrp) {
                if (pWaitgGrp->Initialize (this)) {
                    pWaitgGrp->ReserveWait ();
                    AddToDListTail (&m_dlWaitGroup, (PDLIST) pWaitgGrp);
                } else {
                    pWaitgGrp->Destroy ();
                    pWaitgGrp = NULL;
                }
            }
        }
    }
    Unlock ();

    return pWaitgGrp;
}


//
// create a threadpool item
//
PTP_ITEM CThreadPool::TpAllocItem (FARPROC pfn, PVOID pv, PTP_POOL pPool, PVOID pMod, WORD wType, DWORD dwCookie)
{        
    PTP_ITEM pTpItem = (PTP_ITEM) LocalAlloc(LMEM_FIXED|LMEM_ZEROINIT, sizeof(TP_ITEM)); 
    
    if (pTpItem) {
        pTpItem->Callback    = pfn;
        pTpItem->Context     = pv;
        pTpItem->pPool       = pPool;
        pTpItem->hModule     = pMod;
        pTpItem->wType       = wType;
        pTpItem->dwCookie    = dwCookie;
        pTpItem->DoneEvent   = CreateEvent (NULL, TRUE, TRUE, NULL);

        if (pTpItem->DoneEvent) {
            Lock ();    
            AddToDListTail (&m_dlInactive, &pTpItem->Link);
            Unlock ();
            LogTPAllocItem(pTpItem);
        } else {
            LocalFree (pTpItem);
            pTpItem = NULL;
        }
    }

    return pTpItem;
}

//
// delete a threadpool item
// NOTE: proper locking must be done to ensure the DList operation
//       doesn't race.
//
void CThreadPool::TpDeleteItem (
    LPVOID pObj
    )    
{
    PTP_ITEM pTpItem = (PTP_ITEM) pObj;

    RemoveDList (&pTpItem->Link);
    InitDList (&pTpItem->Link);

    pTpItem->pPool = NULL;
    if (pTpItem->DoneEvent) {
        CloseHandle (pTpItem->DoneEvent);
        pTpItem->DoneEvent = NULL;
    }
    LocalFree (pTpItem);
    LogTPDeleteItem(pTpItem);
}



// per-process global thread pool
PThreadPool g_ProcessThreadPool;

/*++

Routine Description:

    This function returns the per process thread pool. If called
    for the first time, the global pool is also created.

Arguments:

Return Value:

    per-process thread pool 

--*/
PThreadPool
GetPerProcessThreadPool (
    void
    )
{
    PTP_POOL ptpp = (PTP_POOL)g_ProcessThreadPool;

    if (!ptpp) {
        if (NULL != (ptpp = CreateThreadpool (0))) {
            SetThreadpoolThreadMaximum (ptpp, g_ThreadMaxDefault);
            if (!SetThreadpoolThreadMinimum (ptpp, TP_THRD_DFLT_MIN)
                || InterlockedCompareExchange ((PLONG)&g_ProcessThreadPool, (LONG)ptpp, NULL)) {
                // failed to create min threads or some other thread beat us to this; free this pool.
                CloseThreadpool (ptpp);
            }
        }
    }

    return g_ProcessThreadPool;
}















//
// exported APIs
//



/*++

Routine Description:

    This function allocates a new pool of threads to use for executing
    callbacks.

Arguments:

    Reserved - Reserved for future use.

Return Value:

    PoolReturn - A pointer to the newly allocated pool of threads.

--*/
extern "C"
PTP_POOL
WINAPI
CreateThreadpool(
    __reserved PVOID reserved
    )
{
    PThreadPool pPool = new CThreadPool;

    if (pPool && !pPool->IsInitialized()) {
        pPool->Shutdown();
        pPool = NULL;
        SetLastError (ERROR_OUTOFMEMORY);
    }

    DEBUGMSG (DBGTP, (L"CETP Create Pool Pool: 0x%8.8lx\r\n", pPool));    
    return (PTP_POOL)pPool;
}

/*++

Routine Description:

    This function places a cap on the number of worker threads
    which may be allocated to process callbacks queued to the
    specified pool.

Arguments:

    Pool - The thread pool to apply a concurrency limit to.

    MaxThreads - The maximum number of threads allowed to run in the
                 pool.

Return Value:

    None.

--*/
extern "C"
void
WINAPI
SetThreadpoolThreadMaximum(
    __inout PTP_POOL ptpp,
    __in    DWORD    cMaxThreads
    )    
{
    LONG cThrdMax = (LONG)cMaxThreads;
    PThreadPool pPool = (PThreadPool)ptpp;
    if (pPool && (cThrdMax > 0)) {
        pPool->SetThrdMax (cThrdMax);
    }
}

/*++

Routine Description:

    This function dedicates a certain number of threads to
    processing callbacks queued to the specified threadpool,
    guaranteeing that the threads will be available.

Arguments:

    Pool - The thread pool to reserve threads for.

    MinThreads - The minimum number of threads to reserve for
                 processing callbacks in the pool.

Return Value:

    TRUE - The minimum number of threads for the pool
                were created.

    FALSE - Failed to created specified number of threads. Pool
                continues to use existing worker threads.

--*/
extern "C"
BOOL
WINAPI
SetThreadpoolThreadMinimum(
    __inout PTP_POOL ptpp,
    __in    DWORD    cMinThreads
    )    
{
    LONG cThrdMin = (LONG)cMinThreads;
    PThreadPool pPool = (PThreadPool)ptpp;
    return ((pPool) && (cThrdMin > 0)  && pPool->SetThrdMin (cThrdMin));
}

/*++

Routine Description:

    This function sets the stack reserve/commit sizes 
    for the threads in the pool. This does not change the 
    existing threads' stack sizes.

Arguments:

    Pool - The thread pool to apply the value to.

    StackInformation - Stack Reserve/Commit sizes.

    Note: Commit size is ignored as we auto-commit
    stack pages.
    
Return Value:

    TRUE if valid pool.

--*/
extern "C"
BOOL
WINAPI
SetThreadpoolStackInformation(
    __inout PTP_POOL           ptpp,
    __in    PTP_POOL_STACK_INFORMATION ptpsi
    )    
{
    PThreadPool pPool = (PThreadPool)ptpp;

    if (pPool) {
        pPool->SetStackInfo (ptpsi->StackReserve, ptpsi->StackCommit);
    }

    return (NULL != pPool);
}

/*++

Routine Description:

    This function returns the stack reserve/commit 
    values of the pool.

Arguments:

    Pool - The thread pool to apply the value to.

    StackInformation - Stack Reserve/Commit sizes.
    
Return Value:

    TRUE if valid pool.

--*/
extern "C"
BOOL
WINAPI
QueryThreadpoolStackInformation(
    __in    PTP_POOL           ptpp,
    __out   PTP_POOL_STACK_INFORMATION ptpsi
    )    
{
    PThreadPool pPool = (PThreadPool)ptpp;
    if (pPool) {
        DWORD cbStackReserve, cbStackCommit;
        pPool->GetStackInfo (&cbStackReserve, &cbStackCommit);
        ptpsi->StackReserve = cbStackReserve;
        ptpsi->StackCommit = cbStackCommit;
    }

    return (NULL != pPool);
}

/*++

Routine Description:

    This function releases a pool of threads.  If there are any
    outstanding item bound to the pool, the pool will 
    remain active until those outstanding items are freed,
    and then the pool will be freed asynchronously; otherwise, the
    pool will be freed immediately.

Arguments:

    Pool - The thread pool to release.

Return Value:

    None.

--*/
extern "C"
VOID
WINAPI
CloseThreadpool(
    __inout PTP_POOL ptpp
    )
{
    PThreadPool pPool = (PThreadPool)ptpp;
    ASSERT (pPool != g_ProcessThreadPool);
    
    if (pPool && (pPool != g_ProcessThreadPool)) {
        pPool->Shutdown();
    }

    DEBUGMSG (DBGTP, (L"CETP Close pool Pool: 0x%8.8lx\r\n", pPool));    
}

WINBASEAPI
VOID
WINAPI
SetEventWhenCallbackReturns(
    _Inout_ PTP_CALLBACK_INSTANCE pci,
    _In_ HANDLE evt
    )
{
    ValidateCallbackInstance (pci);
    pci->hEvent = evt;
}


WINBASEAPI
VOID
WINAPI
ReleaseSemaphoreWhenCallbackReturns(
    _Inout_ PTP_CALLBACK_INSTANCE pci,
    _In_ HANDLE sem,
    _In_ DWORD crel
    )
{
    ValidateCallbackInstance (pci);
    pci->hSemaphore = sem;
    pci->lSemRelCnt = crel;
}



WINBASEAPI
VOID
WINAPI
ReleaseMutexWhenCallbackReturns(
    _Inout_ PTP_CALLBACK_INSTANCE pci,
    _In_ HANDLE mut
    )
{
    ValidateCallbackInstance (pci);
    pci->hMutex = mut;
}


WINBASEAPI
VOID
WINAPI
LeaveCriticalSectionWhenCallbackReturns(
    _Inout_ PTP_CALLBACK_INSTANCE pci,
    _Inout_ LPCRITICAL_SECTION pcs
    )
{
    ValidateCallbackInstance (pci);
    pci->pcs = pcs;
}


WINBASEAPI
VOID
WINAPI
FreeLibraryWhenCallbackReturns(
    _Inout_ PTP_CALLBACK_INSTANCE pci,
    _In_ HMODULE mod
    )
{
    ValidateCallbackInstance (pci);
    pci->hModule = mod;
}

