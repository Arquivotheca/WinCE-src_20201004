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
//    syncobj.c - implementations of synchronization objects
//
#include <windows.h>
#include <kernel.h>
#include <watchdog.h>

extern PTHREAD pPageOutThread;
extern PHDATA  phdPageOutEvent;

/*

Note about the API set: All APIs should either return 0 on error or return
no value at-all. This will allow us to return gracefully to the user when an
argument to an API doesn't pass the PSL checks. Otherwise we will end
up throwing exceptions on invalid arguments to API calls.

*/

//
// event methods
//
static const PFNVOID EvntMthds[] = {
    (PFNVOID)EVNTCloseHandle,
    (PFNVOID)MSGQPreClose,
    (PFNVOID)EVNTModify,
    (PFNVOID)EVNTGetData,
    (PFNVOID)EVNTSetData,
    (PFNVOID)MSGQRead,
    (PFNVOID)MSGQWrite,
    (PFNVOID)MSGQGetInfo,
    (PFNVOID)WDStart,
    (PFNVOID)WDStop,
    (PFNVOID)WDRefresh,
    (PFNVOID)EVNTResumeMainThread,     // BC work around - allow ResumeThread called on process event
};

static const ULONGLONG evntSigs [] = {
    FNSIG1 (DW),                                // CloseHandle
    0,                                          // PreCloseHandle - never called from outside
    FNSIG2 (DW, DW),                            // EventModify
    FNSIG1 (DW),                                // GetEventData
    FNSIG2 (DW, DW),                            // SetEventData
    FNSIG7 (DW, O_PTR, DW, O_PDW, DW, O_PDW, O_PDW), // ReadMsgQueueEx
    FNSIG5 (DW, I_PTR, DW, DW, DW),             // WriteMsgQueue
    FNSIG2 (DW, IO_PDW),                        // GetMsgQueueInfo (2nd arg - fixed sized struct, use IO_PDW)
    FNSIG1 (DW),                                // StartWatchDogTimer
    FNSIG1 (DW),                                // StopWatchDogTimer
    FNSIG1 (DW),                                // RefreshWatchDogTimer
    FNSIG2 (DW, O_PDW),                         // ResumeMainThread
};

ERRFALSE ((sizeof(EvntMthds) / sizeof(EvntMthds[0])) == (sizeof(evntSigs) / sizeof(evntSigs[0])));


//
// mutex methods
//
static const PFNVOID MutxMthds[] = {
    (PFNVOID)MUTXCloseHandle,
    (PFNVOID)0,
    (PFNVOID)MUTXRelease,
};

static const ULONGLONG mutxSigs [] = {
    FNSIG1 (DW),                                // CloseHandle
    0,                                          // PreCloseHandle - never called from outside
    FNSIG1 (DW),                                // ReleaseMutex
};

ERRFALSE ((sizeof(MutxMthds) / sizeof(MutxMthds[0])) == (sizeof(mutxSigs) / sizeof(mutxSigs[0])));


//
// semaphore methods
//
static const PFNVOID SemMthds[] = {
    (PFNVOID)SEMCloseHandle,
    (PFNVOID)0,
    (PFNVOID)SEMRelease,
};

static const ULONGLONG semSigs [] = {
    FNSIG1 (DW),                                // CloseHandle
    0,                                          // PreCloseHandle - never called from outside
    FNSIG3 (DW, DW, IO_PDW),                    // ReleaseMutex
};

ERRFALSE ((sizeof(SemMthds) / sizeof(SemMthds[0])) == (sizeof(semSigs) / sizeof(semSigs[0])));

//
// critical section methods
//
static const PFNVOID ExtCritMthds [] = {
    (PFNVOID) CRITDelete,
    (PFNVOID) 0,
    (PFNVOID) Win32CRITEnter,
    (PFNVOID) Win32CRITLeave,
    (PFNVOID) NotSupported,         // External use CloseHandle to destory crit object
};

static const PFNVOID IntCritMthds [] = {
    (PFNVOID) CRITDelete,
    (PFNVOID) 0,
    (PFNVOID) Win32CRITEnter,
    (PFNVOID) Win32CRITLeave,
    (PFNVOID) CRITDelete,
};

static const ULONGLONG critSigs [] = {
    FNSIG1 (DW),                                // CloseHandle
    0,                                          // PreCloseHandle - never called from outside
    FNSIG2 (DW, IO_PDW),                        // CRITEnter, 2nd arguemt lpcs, fixed sized struct
    FNSIG2 (DW, IO_PDW),                        // CRITLeave, 2nd arguemt lpcs, fixed sized struct
    FNSIG1 (DW),                               // CRITDelete from kernel mode
};

ERRFALSE ((sizeof(ExtCritMthds) / sizeof(ExtCritMthds[0])) == (sizeof(critSigs) / sizeof(critSigs[0])));
ERRFALSE (sizeof(ExtCritMthds) == sizeof (IntCritMthds));


const CINFO cinfEvent = {
    { 'E', 'V', 'N', 'T' },
    DISPATCH_KERNEL_PSL,
    HT_EVENT,
    sizeof(EvntMthds)/sizeof(EvntMthds[0]),
    EvntMthds,
    EvntMthds,
    evntSigs,
    0,
    0,
    0,
};

const CINFO cinfMutex = {
    { 'M', 'U', 'T', 'X' },
    DISPATCH_KERNEL_PSL,
    HT_MUTEX,
    sizeof(MutxMthds)/sizeof(MutxMthds[0]),
    MutxMthds,
    MutxMthds,
    mutxSigs,
    0,
    0,
    0,
};

const CINFO cinfSem = {
    { 'S', 'E', 'M', 'P' },
    DISPATCH_KERNEL_PSL,
    HT_SEMAPHORE,
    sizeof(SemMthds)/sizeof(SemMthds[0]),
    SemMthds,
    SemMthds,
    semSigs,
    0,
    0,
    0,
};

const CINFO cinfCRIT = {
    { 'C', 'R', 'I', 'T' },
    DISPATCH_KERNEL_PSL,
    HT_CRITSEC,
    sizeof(ExtCritMthds)/sizeof(ExtCritMthds[0]),
    ExtCritMthds,
    IntCritMthds,
    critSigs,
    0,
    0,
    0,
};

typedef void (* PFN_InitSyncObj) (LPVOID pObj, DWORD dwParam1, DWORD dwParam2);
typedef void (* PFN_DeInitSyncObj) (LPVOID pObj);

extern DLIST g_NameList;
extern CRITICAL_SECTION NameCS;

//
// structure to define object specific methods of kernel sync-objects (mutex, event, and semaphore)
// NOTE: name comparision is object specific because it's not at the same offset
//       for individual objects. We should probabaly look into changing the structures
//       so they're at the same offset and then we don't need a compare-name method.
//
typedef struct _SYNCOBJ_METHOD {
    PFN_InitSyncObj     pfnInit;
    PFN_DeInitSyncObj   pfnDeInit;
    const CINFO         *pci;
    DWORD               dwObjectType;
} SYNCOBJ_METHOD, *PSYNCOBJ_METHOD;

//------------------------------------------------------------------------------
// initialize a mutex
//------------------------------------------------------------------------------
static void InitMutex (LPVOID pObj, DWORD bInitialOwner, DWORD unused)
{
    PMUTEX pMutex = (PMUTEX) pObj;
    if (bInitialOwner) {
        PTHREAD pCurTh = pCurThread;
        pMutex->LockCount = 1;
        pMutex->pOwner    = pCurTh;
        SCHL_LinkMutexOwner (pCurTh, pMutex);
    }
}


//------------------------------------------------------------------------------
// initialize an event
//------------------------------------------------------------------------------
static void InitEvent (LPVOID pObj, DWORD fManualReset, DWORD fInitState)
{
    PEVENT pEvent = (PEVENT) pObj;
    pEvent->state = (BYTE) fInitState;
    pEvent->manualreset = (fManualReset? 1 : 0);
    pEvent->bMaxPrio = THREAD_RT_PRIORITY_IDLE;
}

//------------------------------------------------------------------------------
// initialize a semaphore
//------------------------------------------------------------------------------
static void InitSemaphore (LPVOID pObj, DWORD lInitCount, DWORD lMaxCount)
{
    PSEMAPHORE pSem = (PSEMAPHORE) pObj;

    pSem->lMaxCount = (LONG) lMaxCount;
    pSem->lCount = (LONG) lInitCount;
}

//------------------------------------------------------------------------------
// delete a mutex
//------------------------------------------------------------------------------
static void DeInitMutex (LPVOID pObj)
{
    PMUTEX pMutex = (PMUTEX) pObj;
    
    CELOG_MutexDelete (pMutex);

    SCHL_UnlinkMutexOwner (pMutex);

    DEBUGCHK (!pMutex->proxyqueue.pHead);
}

//------------------------------------------------------------------------------
// delete an event
//------------------------------------------------------------------------------
static void DeInitEvent (LPVOID pObj)
{
    PEVENT pEvent = (PEVENT) pObj;

    CELOG_EventDelete (pEvent);

    // close the message queue if this is a message queue
    MSGQClose (pEvent);
    
    // delete the watchdog object if this is a watchdog event
    WDDelete (pEvent);

    DEBUGCHK (!pEvent->proxyqueue.pHead);
}

//------------------------------------------------------------------------------
// delete a semaphore
//------------------------------------------------------------------------------
static void DeInitSemaphore (LPVOID pObj)
{
    PSEMAPHORE pSem = (PSEMAPHORE) pObj;

    CELOG_SemaphoreDelete (pSem);

    DEBUGCHK (!pSem->proxyqueue.pHead);

}

//
// Event methods
//
static SYNCOBJ_METHOD eventMethod = { 
    InitEvent,
    DeInitEvent,
    &cinfEvent,
    KERNEL_EVENT
    };

//
// MsgQ methods (implemented by events)
//
static SYNCOBJ_METHOD msgqMethod = { 
    InitEvent,
    DeInitEvent,
    &cinfEvent,
    KERNEL_MSGQ
    };

//
// Watchdog methods (implemented by events)
//
static SYNCOBJ_METHOD wdogMethod = { 
    InitEvent,
    DeInitEvent,
    &cinfEvent,
    KERNEL_WDOG
    };

//
// Mutex methods
//
static SYNCOBJ_METHOD mutexMethod = {
    InitMutex,
    DeInitMutex,
    &cinfMutex,
    KERNEL_MUTEX
    };

//
// Semaphore methods
//
static SYNCOBJ_METHOD semMethod   = {
    InitSemaphore,
    DeInitSemaphore,
    &cinfSem,
    KERNEL_SEMAPHORE
    };

extern PHDATA AllocHData (PCCINFO pci, PVOID pvObj);

//------------------------------------------------------------------------------
// common function to create/open a sync-object
//------------------------------------------------------------------------------
static HANDLE OpenOrCreateSyncObject (
    DWORD                   dwRequestedAccess,
    DWORD                   dwParam1, 
    DWORD                   dwParam2, 
    LPCWSTR                 pszName,
    PSYNCOBJ_METHOD         pObjMethod,
    BOOL                    fCreate)
{
    DWORD  dwErr = 0;
    HANDLE hObj = NULL;
    PNAME  pObjName = DupStrToPNAME (pszName);

    // error if open existing without a name
    if ((!fCreate && !pszName)          // open with no name
        || (pszName && !pObjName)) {    // invalid name
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        LPVOID  pObj = NULL;
        PPROCESS pprc = pActvProc;

        if (fCreate) {

            // create object
            pObj = AllocMem (HEAP_SYNC_OBJ);
            if (pObj) {
            
                memset (pObj, 0, HEAP_SIZE_SYNC_OBJ);

                // call object init function
                pObjMethod->pfnInit (pObj, dwParam1, dwParam2);
                
            } else {
                dwErr = ERROR_OUTOFMEMORY;
            }
        } else {
            DEBUGCHK (pObjName);
        }

        if (!dwErr) {
            hObj = pObjName
                ? HNDLCreateNamedHandle (pObjMethod->pci, pObj, pprc, dwRequestedAccess, pObjName, pObjMethod->dwObjectType, fCreate, &dwErr)
                : HNDLCreateHandle (pObjMethod->pci, pObj, pprc, 0);

            if (!hObj && !dwErr) {
                dwErr = ERROR_OUTOFMEMORY;
            }
        }
            
        // clean up on error. In case of ERROR_ALREADY_EXIST, we still need to free the object
        // created because the handle refers to the existing object.
        if (dwErr) {
            if (pObj) {
                // call object de-init if object already created
                pObjMethod->pfnDeInit (pObj);
                FreeMem (pObj, HEAP_SYNC_OBJ);
            }

            if (pObjName) {
                FreeName (pObjName);
            }
            
        }
    }


    KSetLastError (pCurThread, dwErr);
    return hObj;
   
}

//------------------------------------------------------------------------------
// common function to close a sync-object handle
//------------------------------------------------------------------------------
static BOOL CloseSyncObjectHandle (LPVOID pObj, PSYNCOBJ_METHOD pSyncMethod)
{
    pSyncMethod->pfnDeInit (pObj);
    FreeMem (pObj, HEAP_SYNC_OBJ);

    return TRUE;
}

//------------------------------------------------------------------------------
// create an event (event, msgq, wdog)
//------------------------------------------------------------------------------
static HANDLE DoCreateEvent (PSYNCOBJ_METHOD pObjMethod, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName) 
{
    HANDLE hEvent;

    DEBUGMSG(ZONE_ENTRY, (L"NKCreateEvent entry: %8.8lx %8.8lx %8.8lx\r\n",
                          fManReset, fInitState, lpEventName));

    hEvent = OpenOrCreateSyncObject (EVENT_ALL_ACCESS, fManReset, fInitState, lpEventName, pObjMethod, TRUE);
        
    DEBUGMSG(ZONE_ENTRY, (L"NKCreateEvent exit: %8.8lx\r\n", hEvent));

    return hEvent;
}

HANDLE NKCreateEvent (LPSECURITY_ATTRIBUTES lpsa, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName) 
{
    return DoCreateEvent (&eventMethod, fManReset, fInitState, lpEventName);
}

HANDLE NKCreateMsgQEvent (LPSECURITY_ATTRIBUTES lpsa, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName) 
{
    return DoCreateEvent (&msgqMethod, fManReset, fInitState, lpEventName);
}

HANDLE NKCreateWdogEvent (LPSECURITY_ATTRIBUTES lpsa, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName) 
{
    return DoCreateEvent (&wdogMethod, fManReset, fInitState, lpEventName);
}

//------------------------------------------------------------------------------
// open an existing named event (event, msgq, wdog)
//------------------------------------------------------------------------------
static HANDLE DoOpenEvent (PSYNCOBJ_METHOD pObjMethod, DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpEventName) 
{
    HANDLE hEvent = NULL;    

    DEBUGMSG(ZONE_ENTRY, (L"NKOpenEvent entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                          dwDesiredAccess, fInheritHandle, lpEventName));
    
    // Validate args
    if (!dwDesiredAccess || (dwDesiredAccess & ~EVENT_ALL_ACCESS) || fInheritHandle) {

        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);

    } else {
        hEvent = OpenOrCreateSyncObject (dwDesiredAccess, FALSE, FALSE, lpEventName, pObjMethod, FALSE);
    }
    
    DEBUGMSG(ZONE_ENTRY, (L"NKOpenEvent exit: %8.8lx\r\n", hEvent));

    return hEvent;
}

HANDLE NKOpenEvent (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpEventName) 
{
    return DoOpenEvent (&eventMethod, dwDesiredAccess, fInheritHandle, lpEventName);
}

HANDLE NKOpenMsgQEvent (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpEventName) 
{
    return DoOpenEvent (&msgqMethod, dwDesiredAccess, fInheritHandle, lpEventName);
}

HANDLE NKOpenWdogEvent (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpEventName) 
{
    return DoOpenEvent (&wdogMethod, dwDesiredAccess, fInheritHandle, lpEventName);
}

//------------------------------------------------------------------------------
// EVNTSetData - set datea field of an event
//------------------------------------------------------------------------------
BOOL EVNTSetData (PEVENT lpe, DWORD dwData)
{
    BOOL fRet = !(PROCESS_EVENT & lpe->manualreset);    // SetEventData not allowed for process event
    DEBUGMSG(ZONE_ENTRY,(L"EVNTSetData entry: %8.8lx %8.8lx\r\n",lpe,dwData));
    if (fRet) {
        lpe->dwData = dwData;
    }
    DEBUGMSG(ZONE_ENTRY,(L"EVNTSetData exit: %d\r\n", fRet));
    return fRet;
}

//------------------------------------------------------------------------------
// EVNTGetData - get datea field of an event
//------------------------------------------------------------------------------
DWORD EVNTGetData (PEVENT lpe)
{
    DEBUGMSG(ZONE_ENTRY,(L"EVNTGetData entry: %8.8lx\r\n",lpe));

    if (!lpe->dwData) {
        KSetLastError (pCurThread, 0);  // no error
    }

    DEBUGMSG(ZONE_ENTRY,(L"EVNTGetData exit: %8.8lx\r\n", lpe->dwData));
    return lpe->dwData;

}

//------------------------------------------------------------------------------
// EVNTResumeMainThread - BC workaround, call ResumeThread on process event
//------------------------------------------------------------------------------
BOOL EVNTResumeMainThread (PEVENT lpe, LPDWORD pdwRetVal)
{
    DWORD dwRet = (DWORD)-1;

    DEBUGMSG(ZONE_ENTRY,(L"EVNTResumeMainThread entry: %8.8lx\r\n",lpe));
    if (PROCESS_EVENT & lpe->manualreset) {
        PHDATA phdThrd = LockHandleData ((HANDLE)lpe->dwData, g_pprcNK);
        PTHREAD    pth = GetThreadPtr (phdThrd);

        if (pth) {
            dwRet = SCHL_ThreadResume (pth);
        } else {
            KSetLastError (pCurThread, ERROR_INVALID_HANDLE);
        }
        
        UnlockHandleData (phdThrd);
    }
    *pdwRetVal = dwRet;
    DEBUGMSG(ZONE_ENTRY,(L"EVNTResumeMainThread exit: dwRet = %8.8lx\r\n", dwRet));

    return (DWORD) -1 != dwRet;
}

void SignalAllBlockers (PTWO_D_NODE *ppQHead)
{
    PTHREAD pThWake;
    while (*ppQHead) {
        pThWake = NULL;
        SCHL_WakeOneThreadFlat (ppQHead, &pThWake);
        if (pThWake) {
            SCHL_MakeRunIfNeeded (pThWake);
        }
    }
}

//------------------------------------------------------------------------------
// ForceEventModify - set/pulse/reset an event, bypass PROCESS_EVENT checking
//------------------------------------------------------------------------------
DWORD ForceEventModify (PEVENT pEvt, DWORD action)
{
    PTHREAD pThWake;
    DWORD   dwErr = 0;
    DEBUGMSG(ZONE_SCHEDULE,(L"ForceEventModify entry: %8.8lx %8.8lx\r\n",pEvt,action));

    CELOG_EventModify (pEvt, action);

    switch (action) {
    case EVENT_PULSE:
    case EVENT_SET:
        if (pEvt->phdIntr) {
            // interrupt event
            pThWake = SCHL_EventModIntr (pEvt, action);
            if (pThWake) {
                SCHL_MakeRunIfNeeded (pThWake);
            }
            
        } else if (pEvt->manualreset) {
            // manual reset event
            PTWO_D_NODE pBlockers   = NULL;
            PTHREAD     pCurTh      = pCurThread;
            DWORD       dwNewPrio   = SCHL_EventModMan (pEvt, &pBlockers, action);

            if (pBlockers) {
                DWORD dwOldPrio = GET_CPRIO (pCurTh);

                SET_NOPRIOCALC(pCurTh);
                if (dwNewPrio < dwOldPrio)
                    SET_CPRIO (pCurTh, dwNewPrio);

                SignalAllBlockers (&pBlockers);                
                
                CLEAR_NOPRIOCALC(pCurTh);
                if (dwNewPrio < dwOldPrio)
                    SCHL_AdjustPrioDown ();
            }

        } else {
            pThWake = NULL;
            while (SCHL_EventModAuto (pEvt, action, &pThWake))
                ;
            if (pThWake) {
                SCHL_MakeRunIfNeeded (pThWake);
            }
        }
        break;
    case EVENT_RESET:
        pEvt->state = 0;
        break;
    default:
        dwErr = ERROR_INVALID_HANDLE;
        DEBUGCHK(0);
    }
    DEBUGMSG(ZONE_SCHEDULE,(L"ForceEventModify exit: dwErr = %8.8lx\r\n",dwErr));
    return dwErr;
}

//------------------------------------------------------------------------------
// EVNTModify - set/pulse/reset an event
//------------------------------------------------------------------------------
BOOL EVNTModify (PEVENT lpe, DWORD type)
{
    DWORD dwErr;
    DEBUGMSG(ZONE_ENTRY,(L"EVNTModify entry: %8.8lx %8.8lx\r\n",lpe,type));

    dwErr = (lpe->manualreset & PROCESS_EVENT)
        ? ERROR_INVALID_HANDLE              // wait-only event, cannot be signaled
        : ForceEventModify (lpe, type);

    if (dwErr) {
        SetLastError (dwErr);
    }
    DEBUGMSG(ZONE_ENTRY,(L"EVNTModify exit: dwErr = %8.8lx\r\n", dwErr));
    return !dwErr;
}

//------------------------------------------------------------------------------
// closehandle an event
//------------------------------------------------------------------------------
BOOL EVNTCloseHandle (PEVENT lpe)
{
    DEBUGMSG(ZONE_ENTRY,(L"EVNTCloseHandle entry: %8.8lx\r\n",lpe));
    
    CloseSyncObjectHandle (lpe, &eventMethod);

    DEBUGMSG(ZONE_ENTRY,(L"EVNTCloseHandle exit: %8.8lx\r\n",TRUE));
    return TRUE;
}

//------------------------------------------------------------------------------
// NKEventModify - set/reset/pulse an event, with handle
//------------------------------------------------------------------------------
BOOL NKEventModify (PPROCESS pprc, HANDLE hEvent, DWORD type)
{
    PHDATA phd = LockHandleData (hEvent, pprc);
    PEVENT lpe = GetEventPtr (phd);
    BOOL   fRet = FALSE;

    if (lpe) {
        fRet = EVNTModify (lpe, type);
    }

    UnlockHandleData (phd);

    DEBUGMSG (!fRet, (L"NKEventModify Failed\r\n"));

    return fRet;
}

//------------------------------------------------------------------------------
// NKIsNamedEventSignaled - check if an event is signaled
//------------------------------------------------------------------------------
BOOL NKIsNamedEventSignaled (LPCWSTR pszName, DWORD dwFlags)
{
    PHDATA phd;
    BOOL fRet = FALSE;
    PCCINFO pci = &cinfEvent;
    PNAME  pName = DupStrToPNAME (pszName);
    
    EnterCriticalSection (&NameCS);

    // iterate through the name list to find existing named handle and if found check the current state
    for (phd = (PHDATA) g_NameList.pFwd; (PHDATA) &g_NameList != phd; phd = (PHDATA) phd->dl.pFwd) {
        if ((phd->pci == pci) && !NKwcscmp (phd->pName->name, pName->name)) {
            PEVENT lpe = GetEventPtr (phd);
            if (lpe) {
                fRet = lpe->state;
            }
            break;
        }
    }

    LeaveCriticalSection (&NameCS);

    if (pName) {
        FreeName (pName);
    }

    return fRet;
}

//------------------------------------------------------------------------------
// LockIntrEvt - called from InterruptInitialize, lock an interrupt event to
//               prevent it from being freed. ptr to HDATA is saved in the event
//               structure.
//------------------------------------------------------------------------------
PEVENT LockIntrEvt (HANDLE hIntrEvt)
{
    PHDATA phd  = LockHandleData (hIntrEvt, pActvProc);
    PEVENT pEvt = GetEventPtr (phd);
    if  (pEvt && !pEvt->manualreset && !pEvt->proxyqueue.pHead) {
        pEvt->phdIntr = phd;
    } else {
        UnlockHandleData (phd);
        pEvt = NULL;
    }
    return pEvt;
}

//------------------------------------------------------------------------------
// UnlockIntrEvt - called from InterruptDisable, unlock an interrupt event
//------------------------------------------------------------------------------
BOOL UnlockIntrEvt (PEVENT pIntrEvt)
{
    return (pIntrEvt && pIntrEvt->phdIntr)? UnlockHandleData (pIntrEvt->phdIntr) : FALSE;
}


//------------------------------------------------------------------------------
// create a semaphore
//------------------------------------------------------------------------------
HANDLE NKCreateSemaphore (LPSECURITY_ATTRIBUTES lpsa, LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName) 
{
    HANDLE hSem = NULL;

    DEBUGMSG(ZONE_ENTRY,(L"NKCreateSemaphore entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",lpsa,lInitialCount,lMaximumCount,lpName));
    
    if ((lInitialCount < 0) || (lMaximumCount < 0) || (lInitialCount > lMaximumCount)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);

    } else {
        hSem = OpenOrCreateSyncObject (SEMAPHORE_ALL_ACCESS, (DWORD) lInitialCount, (DWORD) lMaximumCount, lpName, &semMethod, TRUE);
    }

    DEBUGMSG(ZONE_ENTRY,(L"NKCreateSemaphore exit: %8.8lx\r\n",hSem));
    return hSem;
}

//------------------------------------------------------------------------------
// open an existing named semaphore
//------------------------------------------------------------------------------
HANDLE NKOpenSemaphore (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpName) 
{
    HANDLE hSem = NULL;

    DEBUGMSG(ZONE_ENTRY, (L"NKOpenSemaphore: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                          dwDesiredAccess, fInheritHandle, lpName));
    
    // Validate args
    if (!dwDesiredAccess || (dwDesiredAccess & ~SEMAPHORE_ALL_ACCESS) || fInheritHandle) {

        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);

    } else {
        hSem  = OpenOrCreateSyncObject (dwDesiredAccess, FALSE, FALSE, lpName, &semMethod, FALSE);
    }
    
    DEBUGMSG(ZONE_ENTRY, (L"NKOpenSemaphore exit: %8.8lx\r\n", hSem));

    return hSem ;
}


//------------------------------------------------------------------------------
// SEMRelease - release a semaphore
//------------------------------------------------------------------------------
BOOL SEMRelease (PSEMAPHORE pSem, LONG lReleaseCount, LPLONG lpPreviousCount) 
{
    LONG    prev;
    PTHREAD pth;
    DWORD   dwErr = 0;

    DEBUGMSG(ZONE_ENTRY,(L"SEMRelease entry: %8.8lx %8.8lx %8.8lx\r\n", pSem, lReleaseCount, lpPreviousCount));
    if ((prev = SCHL_SemAdd (pSem, lReleaseCount)) == -1) {
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else {

        CELOG_SemaphoreRelease(pSem, lReleaseCount, prev);

        pth = 0;
        while (SCHL_SemPop (pSem, &lReleaseCount, &pth)) {
            if (pth) {
                SCHL_MakeRunIfNeeded (pth);
                pth = 0;
            }
        }

        if (lpPreviousCount) {
            __try {
                *lpPreviousCount = prev;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
        }
    }

    KSetLastError (pCurThread, dwErr);
    return !dwErr;
}


//------------------------------------------------------------------------------
// closehandle a semaphore
//------------------------------------------------------------------------------
BOOL SEMCloseHandle (PSEMAPHORE lpSem) 
{
    DEBUGMSG(ZONE_ENTRY,(L"SEMCloseHandle entry: %8.8lx\r\n",lpSem));

    CloseSyncObjectHandle (lpSem, &semMethod);

    DEBUGMSG(ZONE_ENTRY,(L"SEMCloseHandle exit: %8.8lx\r\n",TRUE));
    return TRUE;
}

//------------------------------------------------------------------------------
//  create a mutex
//------------------------------------------------------------------------------
HANDLE NKCreateMutex (LPSECURITY_ATTRIBUTES lpsa, BOOL bInitialOwner, LPCTSTR lpName) 
{
    HANDLE hMutex;
    DEBUGMSG(ZONE_ENTRY,(L"NKCreateMutex entry: %8.8lx %8.8lx %8.8lx\r\n",lpsa,bInitialOwner,lpName));

    hMutex = OpenOrCreateSyncObject (MUTANT_ALL_ACCESS, bInitialOwner, 0, lpName, &mutexMethod, TRUE);

    DEBUGMSG(ZONE_ENTRY,(L"NKCreateMutex exit: %8.8lx\r\n",hMutex));
    return hMutex;
}


//------------------------------------------------------------------------------
// open an existing named mutex
//------------------------------------------------------------------------------
HANDLE NKOpenMutex (DWORD dwDesiredAccess, BOOL fInheritHandle, LPCWSTR lpName) 
{
    HANDLE hMutex = NULL;

    DEBUGMSG(ZONE_ENTRY, (L"NKOpenMutex entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                          dwDesiredAccess, fInheritHandle, lpName));
    
    // Validate args
    if (!dwDesiredAccess || (dwDesiredAccess & ~MUTANT_ALL_ACCESS) || fInheritHandle) {

        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);

    } else {
        hMutex  = OpenOrCreateSyncObject (dwDesiredAccess, FALSE, FALSE, lpName, &mutexMethod, FALSE);
    }
    
    DEBUGMSG(ZONE_ENTRY, (L"NKOpenMutex exit: %8.8lx\r\n", hMutex));

    return hMutex ;
}

DWORD GiveupMutex (PMUTEX pMutex, LPCRITICAL_SECTION lpcs)
{
    PTHREAD pNewOwner = NULL;
    DWORD NewOwnerId = 0;
    
    // try to give the CS/mutex to a thread who's waiting. SCHL_ReleaseMutex returns TRUE if we need to keep
    // searching. The reason for the loop is that threads blocked on the cs can be terminated/signaled
    // and abandon the wait. i.e. we're looping here to find the highest priority thread that 
    // still want to acquire CS/mutex.
    while (SCHL_ReleaseMutex (pMutex, lpcs, &pNewOwner))
        ;
    
    if (pNewOwner) {
        // other thread got the CS, put it back to the ready queue
        NewOwnerId = pNewOwner->dwId;
        SCHL_YieldToNewMutexOwner (pNewOwner);
    }

    return NewOwnerId;
}

//------------------------------------------------------------------------------------------
// DoLeaveMutex - worker function to release a mutex (already removed from owned object list)
// Returns the ID of the new owner thread.
//------------------------------------------------------------------------------------------
static DWORD DoReleaseMutex (PMUTEX pMutex, LPCRITICAL_SECTION lpcs) 
{
    DWORD NewOwnerId = 0;

    if (SCHL_PreReleaseMutex (pMutex, lpcs)) {
        NewOwnerId = GiveupMutex (pMutex, lpcs);
    }

    SCHL_PostUnlinkCritMut (pMutex, lpcs);
    return NewOwnerId;
}

//------------------------------------------------------------------------------
//  release a mutex
//------------------------------------------------------------------------------
BOOL MUTXRelease (PMUTEX pMutex) 
{
    BOOL fRet = (pMutex->pOwner == pCurThread);

    if (!fRet) {
        SetLastError(ERROR_NOT_OWNER);
        
    } else {
        CELOG_MutexRelease (pMutex);
        if (pMutex->LockCount > 1) {
            pMutex->LockCount--;
        } else {
            DEBUGCHK (pMutex->LockCount == 1);
            DoReleaseMutex (pMutex, NULL);
        }
    }
    
    return fRet;
}


//------------------------------------------------------------------------------
// closehandle a mutex
//------------------------------------------------------------------------------
BOOL MUTXCloseHandle (PMUTEX pMutex) 
{
    DEBUGMSG(ZONE_ENTRY,(L"MUTXCloseHandle entry: %8.8lx\r\n",pMutex));

    CloseSyncObjectHandle (pMutex, &mutexMethod);

    DEBUGMSG(ZONE_ENTRY,(L"MUTXCloseHandle exit: %8.8lx\r\n",TRUE));
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Kernel fast (Reader/writer) lock.
//
// IMPORTANT NOTE:
//      The lock is implemented with one thing in mind - fast. There is no nesting support whatsoever.
//      i.e. it'll deadlock if you call AcquireReadLock/AcquireWriteLock on the same lock with
//      any combination. USE WITH CARE.
// 


// initilize a fast lock
BOOL InitializeFastLock (PFAST_LOCK lpFastLock)
{
    memset (lpFastLock, 0, sizeof (FAST_LOCK));

    return TRUE;
}

// delete a fast lock
BOOL DeleteFastLock (PFAST_LOCK pFastLock)
{
    DEBUGCHK (!pFastLock->proxyqueue.pHead);
    DEBUGCHK (!pFastLock->lLock);
    return TRUE;
}

// worker function to release a fast lock
static void DoReleaseFastLock (PFAST_LOCK pFastLock)
{
    PTHREAD pth;

    while (NULL != (pth = SCHL_UnblockNextThread (pFastLock))) {
        if (pth == pFastLock->pOwner) {
            // writer woken up, yield to the writer so it can release the lock to avoid convoy
            SCHL_YieldToNewMutexOwner (pth);
            // once we wake up the writer, we can't wakeup any more threads.
            break;
        }
        SCHL_MakeRunIfNeeded (pth);
    }
}

// acquire a fast lock for read access
void AcquireReadLock (PFAST_LOCK pFastLock)
{
    DWORD dwOldLockValue;
    DEBUGCHK (!pFastLock->pOwner || (pFastLock->pOwner != pCurThread));
    do {
        dwOldLockValue = pFastLock->lLock;
        
        if (dwOldLockValue >= RWL_CNTMAX) {
            SCHL_WaitForReadLock (pFastLock);
            break;
        }
    } while (InterlockedCompareExchange (&pFastLock->lLock, dwOldLockValue+1, dwOldLockValue) != (LONG) dwOldLockValue);

    DEBUGCHK (!pFastLock->pOwner);
}

// release a fast lock for read access
void ReleaseReadLock (PFAST_LOCK pFastLock)
{
    DEBUGCHK (!(pFastLock->lLock & RWL_XBIT));
    DEBUGCHK (pFastLock->lLock & RWL_CNTMAX);
    DEBUGCHK (!pFastLock->pOwner);

    if (InterlockedDecrement (&pFastLock->lLock) == RWL_WBIT) {
        // we're the last reader, and Wait-bit is set.
        DoReleaseFastLock (pFastLock);
    }
}

// acquire a fast lock for write access
void AcquireWriteLock (PFAST_LOCK pFastLock)
{
    DEBUGCHK (!pFastLock->pOwner || (pFastLock->pOwner != pCurThread));
    if (InterlockedCompareExchange (&pFastLock->lLock, RWL_XBIT, 0) != 0) {
        // lLock non-zero -> can't get the lock with fast path
        SCHL_WaitForWriteLock (pFastLock);
        SCHL_MutexFinalBoost ((PMUTEX) pFastLock, NULL);
    } else {
        DEBUGCHK (!pFastLock->pOwner);
        DEBUGCHK (CRIT_STATE_NOT_OWNED == pFastLock->bListed);
        pFastLock->pOwner = pCurThread;
        // there is a chance where we fast-path the lock, and before we set the owner field
        // another reader/writer gets in and blocked. In which case, our priority will not be boosted.
        // Check if the wait bit is set after we update the owner field to make sure we got our
        // priority boosted correctly.
        if (pFastLock->lLock & RWL_WBIT) {
            SCHL_MutexFinalBoost ((PMUTEX) pFastLock, NULL);
        }
    }
    DEBUGCHK (pFastLock->pOwner == pCurThread);
}

// release a fast lock for write access
void ReleaseWriteLock (PFAST_LOCK pFastLock)
{
#ifdef DEBUG
    PTHREAD pCurTh = pCurThread;
#endif
    DEBUGCHK ((pFastLock->lLock & ~RWL_WBIT) == RWL_XBIT);
    DEBUGCHK (pFastLock->pOwner == pCurTh);

    pFastLock->pOwner = NULL;
#ifdef ARM
    if (IsV6OrLater ()) {
        V6_WriteBarrier ();
    }
#endif
    // if no one waiting on this lock, just clear the X-bit and return
    if (InterlockedCompareExchange (&pFastLock->lLock, 0, RWL_XBIT) != RWL_XBIT) {
        // wait-bit is set - need to wakeup reader/writer
        SCHL_PreReleaseWriteLock (pFastLock);
        DoReleaseFastLock (pFastLock);
        SCHL_PostUnlinkCritMut ((PMUTEX) pFastLock, NULL);
    }
    DEBUGCHK (!pCurTh || (pFastLock->pOwner != pCurTh));
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IMPORTANT NOTE FOR CRITICAL SECTION:
//
//  Kernel and user mode CS are handled differently. The hCrit member of CS is a pointer when CS is created by
//  kernel (ONLY KERNEL, NOT INCLUDING OTHER DLL LOADED INTO KERNEL). For the others, hCrit member is a handle.
//
//  Therefore, the hCrit member SHOULD NEVER BE REFERENCED WITHIN CS HANDLING CODE. An PMUTEX argument is passed
//  along all the CS functions to access the CRIT structure.
//
//


//------------------------------------------------------------------------------
// Critical section related functions

//------------------------------------------------------------------------------
// CRITCreate - create a critical section
//------------------------------------------------------------------------------
PMUTEX CRITCreate (LPCRITICAL_SECTION lpcs) 
{
    PMUTEX pcrit = NULL;

#ifdef ARM
#ifdef DEBUG
    MEMORY_BASIC_INFORMATION mbi;
    PPROCESS pprc;

    if (((DWORD)lpcs)>=0x80000000)
        pprc = g_pprcNK;
    else
        pprc = pVMProc;

    ZeroMemory(&mbi,sizeof(mbi));
    if (sizeof(mbi)==VMQuery(pprc, lpcs, &mbi, sizeof(mbi)))
    {
        if (mbi.Protect & PAGE_NOCACHE)
        {
            RETAILMSG(1, (TEXT("\r\n\r\n***Invalid CS Address**\r\nALLOCATING CS IN UNCACHED MEMORY (0x%08X)\r\nThis is not compatible with ARM SMP.\r\n\r\n"), lpcs));
            DEBUGCHK(FALSE);
        }
    }
#endif
#endif

    if (IsDwordAligned (lpcs)) {
        pcrit= AllocMem(HEAP_MUTEX);

        if (pcrit) {
            memset (pcrit, 0, sizeof(MUTEX));
            pcrit->lpcs = lpcs;
        }
    }
    DEBUGMSG(ZONE_ENTRY,(L"CRITCreate exit: %8.8lx\r\n", pcrit));
    return pcrit;
}

//------------------------------------------------------------------------------
// CRITDelete - delete a critical section
//------------------------------------------------------------------------------
BOOL CRITDelete (PMUTEX pcrit) 
{
    CELOG_CriticalSectionDelete(pcrit);

    DEBUGCHK (!pcrit->proxyqueue.pHead);

    FreeMem (pcrit, HEAP_MUTEX);
    return TRUE;
}

#define CS_ABANDON_INCREMENT        0x10000

//------------------------------------------------------------------------------
// CRITEnter - EnterCriticalSection
//------------------------------------------------------------------------------
void CRITEnter (PMUTEX pCrit, LPCRITICAL_SECTION lpcs) 
{
    DEBUGCHK (lpcs == pCrit->lpcs);
    
    if (lpcs == pCrit->lpcs) {
        
        PTHREAD     pCurTh = pCurThread;
        PROXY       Prox;
        PMUTEX      pCritMut = NULL;
        DWORD       dwSignalState;

        DEBUGCHK (!InSysCall () && IsValidKPtr (pCrit));
        DEBUGCHK (!pCurTh->lpProxy);
        DEBUGCHK (pCrit->pOwner != pCurTh);
        DEBUGCHK (!OwnCS (lpcs));
        
        Prox.pQDown     = NULL;
        Prox.wCount     = pCurTh->wCount;
        Prox.pObject    = (LPBYTE)pCrit;
        Prox.bType      = HT_CRITSEC;
        Prox.pTh        = pCurTh;

        // Changing Wait State here is safe without holding spinlock, for we're not in any queue, and
        // there is no way any other thread/interrupt can wake us up.
        pCurTh->bWaitState = WAITSTATE_PROCESSING;

        dwSignalState = SCHL_CSWaitPart1 (&pCritMut, &Prox, pCrit, lpcs);

        DEBUGCHK ((SIGNAL_STATE_NOT_SIGNALED == dwSignalState) || !Prox.pQDown);            // cannot be in any queue if signaled
        DEBUGCHK ((SIGNAL_STATE_SIGNALED != dwSignalState) || (pCurTh == pCrit->pOwner));   // we must be the owner if signaled

        if (SIGNAL_STATE_NOT_SIGNALED == dwSignalState) {
            // CS owned by another thread, boost the thread's priority if needed

            DEBUGCHK(!pCritMut || (pCritMut == pCrit));
            if (pCritMut) {
                SCHL_BoostMutexOwnerPriority (pCurTh, pCritMut);
            }

            // Log that this thread is about to block and which thread currently owns the CS
            CELOG_CriticalSectionEnter (pCrit, lpcs);

            SCHL_CSWaitPart2 (pCrit, lpcs);

            DEBUGCHK (!pCurTh->lpProxy);

            // remove current thread from the CS's waiting queue
            if (Prox.pQDown && SCHL_DequeuePrioProxy (&Prox) && (pCurTh != pCrit->pOwner)) {
                // priority of current thread changed due to acquisition of the CS.
                SCHL_DoReprioCritMut (pCrit);
            }

        }
        DEBUGCHK (!Prox.pQDown);
        
        if (pCrit->pOwner == pCurTh) {
            SCHL_MutexFinalBoost (pCrit, lpcs);
            lpcs->LockCount = 1;
            DEBUGCHK (lpcs->OwnerThread == (HANDLE)pCurTh->dwId);
        } else {
            DEBUGCHK ((DWORD) lpcs->OwnerThread != pCurTh->dwId);
            DEBUGCHK (!IsKModeAddr ((DWORD)lpcs));
            // if the thread is going to be terminated upon return of CRITEnter
            // increment needtrap by CS_ABANDON_INCREMENT (0x10000).
            DEBUGCHK (GET_DYING (pCurTh) && !GET_DEAD (pCurTh) && (pActvProc == pCurTh->pprcOwner));
            DEBUGMSG (ZONE_SCHEDULE, (L"Thread %8.8lx terminated while waiting for CS (%8.8lx, %8.8lx)\r\n", pCurTh->dwId, lpcs, pCrit));
            InterlockedExchangeAdd ((PLONG) &lpcs->needtrap, CS_ABANDON_INCREMENT);
        }
    }
}

//------------------------------------------------------------------------------
// CRITLeave - LeaveCriticalSection
//------------------------------------------------------------------------------
void CRITLeave (PMUTEX pCrit, LPCRITICAL_SECTION lpcs) 
{
    DEBUGCHK (pCrit->lpcs == lpcs);

    if (pCrit->lpcs == lpcs) {

        // need to do this before we release the critical section.
        // update needtrap if there is any abandomed wait
        if (lpcs->needtrap > CS_ABANDON_INCREMENT) {
            // decrement needtrap by abandom count, and remove the abandom count.
            // NOTE: we use top 16 bits of needtrap to track abandom count.
            DWORD dwNewTrapCnt;
            DWORD dwCurTrapCnt;
            do {
                dwCurTrapCnt = lpcs->needtrap;
                dwNewTrapCnt = LOWORD(dwCurTrapCnt) - HIWORD (dwCurTrapCnt);
            } while (InterlockedCompareExchange ((PLONG)&lpcs->needtrap, dwNewTrapCnt, dwCurTrapCnt) != (LONG) dwCurTrapCnt);
        }

        DoReleaseMutex (pCrit, lpcs);
        
        DEBUGCHK (pCrit->pOwner != pCurThread);

        // can't de-ref lpcs for lpcs can potentially be decommitted after LeaveCriticalSection is called.
        // DEBUGCHK (((DWORD)lpcs->OwnerThread|1) != (dwCurThId|1));
    }
}

//------------------------------------------------------------------------------
// WIN32 exports for CriticalSection
//------------------------------------------------------------------------------

static BOOL LockCSMemory (volatile CRITICAL_SECTION *lpcs, PLOCKPAGELIST plp)
{
    BOOL fRet = ((DWORD) lpcs >= VM_SHARED_HEAP_BASE);
    if (fRet) {
        // locking not needed, set size to 0
        plp->cbSize = 0;
    } else {
        // locking needed
        PPROCESS pprc = pVMProc;
        plp->dwAddr = PAGEALIGN_DOWN ((DWORD) &lpcs->OwnerThread);
        plp->cbSize = VM_PAGE_SIZE;
        VMFastLockPages (pprc, plp);

        __try {
            HANDLE hOwner = lpcs->OwnerThread;
            // make sure we can write to it. Since we can't arbitary change the memory,
            // use InterlockedCompareExchange to make sure we don't alter the content.
            InterlockedCompareExchangePointer (&lpcs->OwnerThread, hOwner, hOwner);
            fRet = TRUE;
            
        } __finally {

            if (!fRet) {
                // exception while trying to acces lpcs, unlock the pages
                VMFastUnlockPages (pprc, plp);
            }
        }
    }
    return fRet;
}

static void UnlockCSMemory (PLOCKPAGELIST plp)
{
    if (plp->cbSize) {
        VMFastUnlockPages (pVMProc, plp);
    }
}

// EnterCriticalSection - set the UserBlock bit
void Win32CRITEnter (PMUTEX pCrit, LPCRITICAL_SECTION lpcs) 
{
    LOCKPAGELIST lp;
    if (LockCSMemory (lpcs, &lp)) {
        PTHREAD pCurTh = pCurThread;
        SET_USERBLOCK (pCurTh);
        CRITEnter (pCrit, lpcs);
        CLEAR_USERBLOCK (pCurTh);
        UnlockCSMemory (&lp);
    }
}

void Win32CRITLeave (PMUTEX pCrit, LPCRITICAL_SECTION lpcs)
{
    LOCKPAGELIST lp;
    if (LockCSMemory (lpcs, &lp)) {
        CRITLeave (pCrit, lpcs);
        UnlockCSMemory (&lp);
    }
}

static BOOL CRITEnoughMemAvailable ()
{
    int i = 0;
    DWORD dwCurrThdPrio = GET_CPRIO(pCurThread);
    
    // signal the thread if it is not running
    if (InterlockedCompareExchange (&PagePoolTrimState, PGPOOL_TRIM_SIGNALED, PGPOOL_TRIM_IDLE) == PGPOOL_TRIM_IDLE) {
        EVNTModify (GetEventPtr (phdPageOutEvent), EVENT_SET);
    }

    if (dwCurrThdPrio <= GET_CPRIO(pPageOutThread)) {
        SCHL_SetThreadBasePrio (pPageOutThread, dwCurrThdPrio ? (dwCurrThdPrio - 1) : dwCurrThdPrio);
    }

    do {
        NKSleep (2*i);
    } while ((i++ < 8)
        && (PageFreeCount <= g_cpCriticalThreshold)
        && (PageFreeCount_Pool > g_cpCriticalThreshold));

    return (PageFreeCount > g_cpCriticalThreshold);
}

// InitializeCriticalSection - call CRITCreate and make it a handle
HANDLE EXTCRITCreate (LPCRITICAL_SECTION lpcs)
{
    HANDLE  hCrit = NULL;
    PPROCESS pprc = pActvProc;

    if (   (pprc->fFlags & PROC_PSL_SERVER)                 // PSL server
        || (PageFreeCount > g_cpCriticalThreshold)          // got enough memory
        || ((PageFreeCount_Pool > g_cpCriticalThreshold) && CRITEnoughMemAvailable())) {    // got enough memory after paging out

        PMUTEX pMutex = CRITCreate (lpcs);
        if (pMutex) {
            // create the MUTEX handle
            hCrit = HNDLCreateHandle (&cinfCRIT, pMutex, pActvProc, 0);

            if (!hCrit) {
                CRITDelete (pMutex);
            }
        }
    } else {
        // terminate the current process
        NKD (L"Critial OOM reached, terminate current process (%8.8lx)\r\n", pprc->dwId);
        DebugBreak ();
        PROCTerminate (pprc, 0);
    }

    return hCrit;
}

