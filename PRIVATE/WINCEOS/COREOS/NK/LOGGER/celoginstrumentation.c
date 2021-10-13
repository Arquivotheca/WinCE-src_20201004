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
/*++

Abstract:
    This file implements the calls that log kernel events.

--*/

#define C_ONLY
#include "kernel.h"

typedef struct {
    union {
        CEL_PROCESS_CREATE      ProcessBase;
        CEL_EXTRA_PROCESS_INFO  ProcessExtra;
        BYTE _b;  // Work around compiler warning on zero-length array
    };
    WCHAR _sz[MAX_PATH];  // Not used directly; string starts at end of data above
} ProcessInfoBuffer;

typedef struct {
    union {
        CEL_THREAD_CREATE cl;
        BYTE _b;  // Work around compiler warning on zero-length array
    };
    WCHAR _sz[MAX_PATH];  // Accessed through cl.szName
} ThreadInfoBuffer;


// Max at 32 processes because that's likely enough for back-compat and because
// it's tough to alloc memory during a re-sync.
#define MAX_MODULE_REFERENCES 32

typedef union {  // Union so that the buffer is the max of all these and aligned properly
    struct {
        union {
            CEL_MODULE_LOAD         ModuleBase;
            CEL_EXTRA_MODULE_INFO   ModuleExtra;
            BYTE _b;  // Work around compiler warning on zero-length array
        };
        WCHAR _sz[MAX_PATH];  // Not used directly; string starts at end of data above
    };
    struct {
        int                         NumRefs;  // Current count
        CEL_MODULE_REFERENCES       ModuleRef;
        CEL_PROCESS_REFCOUNT _ref[MAX_MODULE_REFERENCES-1];  // Not used directly; array starts at end of ModuleRef
    };
} ModuleInfoBuffer;


static void CELOG_KCLogProcessInfo(PPROCESS pProcess, LPCWSTR lpProcName,
                                   ProcessInfoBuffer* pProcessBuf);
static void CELOG_EventCreate(PHDATA phd, BOOL fCreate);
static void CELOG_MutexCreate(PHDATA phd, BOOL fCreate);
static void CELOG_SemaphoreCreate(PHDATA phd, BOOL fCreate);
static void CELOG_CriticalSectionCreate(PHDATA phd);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
CELOG_Interrupt(
    DWORD dwLogValue
    )
{
    // Make use of the CeLog tracking call to count interrupts for the kernel
    // profiler.  This counter is not really related to CeLog.
    if (g_ProfilerState.bProfileTimerRunning
        && !(dwLogValue & 0x80000000)) {  


        g_ProfilerState.dwInterrupts++;
    }

    // Do the IsCeLogZoneEnabled() test here rather than in assembly because it
    // does not affect non-profiling builds.  The function call(s) to get to
    // this point are wasted work only on profiling builds in which CeLog is not
    // currently active.
    if (IsCeLogZoneEnabled(CELZONE_INTERRUPT) && g_pfnCeLogInterrupt) {
        g_pfnCeLogInterrupt(dwLogValue);
    }
}


#define MAX_MSGLEN 384

//----------------------------------------------------------
void
CELOG_OutputDebugString(
    DWORD   dwProcessId,
    DWORD   dwThreadId,
    LPCWSTR pszMessage
    )
{
    if (IsCeLogZoneEnabled(CELZONE_DEBUG)) {
        struct {
            union {
                CEL_DEBUG_MSG cl;
                BYTE _b;  // Work around compiler warning on zero-length array
            };
            WCHAR _sz[MAX_MSGLEN];  // Accessed through cl.szMessage
        } buf;
        WORD wLen;
    
        buf.cl.pid = dwProcessId;
        buf.cl.tid = dwThreadId;
    
        wLen = (WORD) (NKwcsncpy(buf.cl.szMessage, pszMessage, MAX_MSGLEN) + 1);
    
        g_pfnCeLogData(TRUE, CELID_DEBUG_MSG, (PVOID) &buf.cl,
                       (WORD)(sizeof(CEL_DEBUG_MSG) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_DEBUG, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_ThreadMigrate(
    DWORD dwProcessId
    )
{
    if (IsCeLogZoneEnabled(CELZONE_MIGRATE)) {
        CEL_THREAD_MIGRATE cl;

        cl.hProcess   = (HANDLE) dwProcessId;
        cl.dwReserved = 0;  // Someday we may use this to indicate migrating in vs. out

        g_pfnCeLogData(TRUE, CELID_THREAD_MIGRATE, &cl, sizeof(CEL_THREAD_MIGRATE),
                       0, CELZONE_MIGRATE, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_KCThreadSwitch(           // Called inside a KCall
    PTHREAD pThread
    )
{
    // Logged for both general and profiling mode, all zones, because celog.dll
    // requires thread switches to know when to flush its small buffer.
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {

        CEL_THREAD_SWITCH cl;

        if (pThread) {
            cl.hThread = (HANDLE) pThread->dwId;
        } else {
            cl.hThread = 0;
        }

        g_pfnCeLogData(TRUE, CELID_THREAD_SWITCH, &cl, sizeof(cl),
                       0, CELZONE_RESCHEDULE | CELZONE_PROFILER, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_KCThreadRunnable(           // Called inside a KCall
    PTHREAD pThread
    )
{
    if (IsCeLogZoneEnabled(CELZONE_RESCHEDULE)) {

        CEL_THREAD_RUNNABLE cl;

        if (pThread) {
            cl.hThread = (HANDLE) pThread->dwId;
        } else {
            cl.hThread = 0;
        }

        g_pfnCeLogData(TRUE, CELID_THREAD_RUNNABLE, &cl, sizeof(cl),
                       0, CELZONE_RESCHEDULE, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_KCThreadQuantumExpire()   // Called inside a KCall
{
    if (IsCeLogZoneEnabled(CELZONE_RESCHEDULE)) {
        g_pfnCeLogData(TRUE, CELID_THREAD_QUANTUMEXPIRE, NULL, 0,
                       0, CELZONE_RESCHEDULE, 0, FALSE);
    }
}

//----------------------------------------------------------
_inline static void
CELOG_KCLogPriorityInvert(      // Called inside a KCall
    PTHREAD pThread,
    PCEL_SYSTEM_INVERT pclInvert  // Temp storage space to use during this call
    )
{
    if (IsCeLogZoneEnabled(CELZONE_PRIORITYINV)) {
        pclInvert->hThread   = (HANDLE) pThread->dwId;
        pclInvert->nPriority = pThread->bCPrio;

        g_pfnCeLogData(TRUE, CELID_SYSTEM_INVERT, (PVOID) pclInvert,
                       sizeof(CEL_SYSTEM_INVERT), 0, CELZONE_PRIORITYINV,
                       0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_KCThreadPriorityInvert(   // Called inside a KCall
    PTHREAD pThread
    )
{
    CEL_SYSTEM_INVERT cl;       // Temp buffer on stack
    CELOG_KCLogPriorityInvert(pThread, &cl);
}

//----------------------------------------------------------
void
CELOG_KCThreadSetPriority(      // Called inside a KCall
    PTHREAD pThread,
    int     nPriority
    )
{
    if (IsCeLogZoneEnabled(CELZONE_THREAD)) {
        CEL_THREAD_PRIORITY cl;
        
        cl.hThread   = (HANDLE) pThread->dwId;
        cl.nPriority = nPriority;
    
        g_pfnCeLogData(TRUE, CELID_THREAD_PRIORITY, &cl, sizeof(CEL_THREAD_PRIORITY),
                       0, CELZONE_THREAD, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_ThreadSetQuantum(
    PTHREAD pThread,
    DWORD   dwQuantum
    )
{
    if (IsCeLogZoneEnabled(CELZONE_THREAD)) {
        CEL_THREAD_QUANTUM cl;
        
        cl.hThread   = (HANDLE) pThread->dwId;
        cl.dwQuantum = dwQuantum;
    
        g_pfnCeLogData(TRUE, CELID_THREAD_QUANTUM, &cl, sizeof(CEL_THREAD_QUANTUM),
                       0, CELZONE_THREAD, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_KCThreadSuspend(  // Called inside a KCall
    PTHREAD pThread
    )
{
    if (IsCeLogZoneEnabled(CELZONE_RESCHEDULE)) {
        CEL_THREAD_SUSPEND cl;
        cl.hThread = (HANDLE) pThread->dwId;
        g_pfnCeLogData(TRUE, CELID_THREAD_SUSPEND, &cl, sizeof(CEL_THREAD_SUSPEND),
                       0, CELZONE_RESCHEDULE, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_KCThreadResume(
    PTHREAD pThread
    )
{
    if (IsCeLogZoneEnabled(CELZONE_RESCHEDULE)) {
        CEL_THREAD_RESUME cl;
        cl.hThread = (HANDLE) pThread->dwId;
        g_pfnCeLogData(TRUE, CELID_THREAD_RESUME, &cl, sizeof(CEL_THREAD_RESUME),
                       0, CELZONE_RESCHEDULE, 0, FALSE);
    }
}

//----------------------------------------------------------
static void
CELOG_KCLogThreadInfo(           // May be done inside a KCall
    PTHREAD  pThread,
    PPROCESS pProcess,
    ProcStartInfo* pPSI,         // Used if this is a new process
    ThreadInfoBuffer* pThreadBuf // Temp storage space to use during this call
    )
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_THREAD | CELZONE_PROFILER)) {
        WORD wLen = 0;

        pThreadBuf->cl.hProcess    = (HANDLE) pProcess->dwId;
        pThreadBuf->cl.hThread     = (HANDLE) pThread->dwId;
        pThreadBuf->cl.hModule     = (HANDLE)0;  // The reader can figure out hModule from dwStartAddr
        pThreadBuf->cl.dwStartAddr = pThread->dwStartAddr;
        pThreadBuf->cl.nPriority   = pThread->bBPrio;

        // Name the main thread of a new process after the process.  But on
        // process create, the process name is not yet stored in pProcess.  It's
        // in pPSI instead.
        if (pPSI) {
            wLen = (WORD) (NKwcsncpy(pThreadBuf->cl.szName, pPSI->pszImageName, MAX_PATH) + 1);
        } else if ((pThread == MainThreadOfProcess(pProcess))
                   && pProcess->lpszProcName) {
            // Main thread gets the process' name
            pThreadBuf->cl.hModule = (HANDLE) pProcess->dwId;
            wLen = (WORD) (NKwcsncpy(pThreadBuf->cl.szName, pProcess->lpszProcName, MAX_PATH) + 1);
        
            // THRDCreate sets the thread start address to the function that
            // the thread starts with -- which is CreateNewProc for the main
            // thread of a process.  Meanwhile when that thread gets into
            // CreateNewProcHelper, it RE-SETS its start address to the "real"
            // start address, which is the process entry point (usually
            // something like WinMainCRTStartup).  This causes a discrepancy,
            // where CeLog would log the start address as CreateNewProc when a
            // process is created, and later, if there's a re-sync, log the
            // "real" start address.  Deal with the discrepancy by *always*
            // logging the main thread's start address as CreateNewProc.
            pThreadBuf->cl.dwStartAddr = (DWORD) CreateNewProc;

        } else {
            // Use the profiler symbols to get the thread name, if possible
            GetThreadName(pThread, &(pThreadBuf->cl.hModule), pThreadBuf->cl.szName);
            if (pThreadBuf->cl.szName[0] != 0) {
                wLen = (WORD) (NKwcslen(pThreadBuf->cl.szName) + 1);
            }
        }

        g_pfnCeLogData(TRUE, CELID_THREAD_CREATE, (PVOID) pThreadBuf,
                       (WORD)(sizeof(CEL_THREAD_CREATE) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_THREAD | CELZONE_PROFILER, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_ThreadCreate(
    PTHREAD  pThread,
    PPROCESS pProcess,
    LPVOID   lpvThreadParam
    )
{
    union {
        ProcessInfoBuffer ProcessBuf;
        ThreadInfoBuffer  ThreadBuf;
    } TempBuffer;
    ProcStartInfo* pPSI = NULL;  // Used if this is a new process
    
    // On process create, pThread->dwStartAddr is CreateNewProc and
    // lpvThreadParam is a ProcStartInfo containing the process name.
    if (pThread->dwStartAddr == (DWORD) CreateNewProc) {
        // This is the main thread of a new process.  Log a ProcessCreate event
        // before logging the thread creation.  Do this outside the zone-check
        // in case the process zone is on when the thread zone is not.
        pPSI = lpvThreadParam;
        CELOG_KCLogProcessInfo(pProcess, pPSI->pszImageName, &TempBuffer.ProcessBuf);
    }

    CELOG_KCLogThreadInfo(pThread, pProcess, pPSI, &TempBuffer.ThreadBuf);

    // The kernel creates all threads suspended, and resumes them if necessary
    CELOG_KCThreadSuspend(pThread);
}

//----------------------------------------------------------
void
CELOG_ThreadTerminate(
    PTHREAD pThread
    )
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_THREAD | CELZONE_PROFILER)) {
        CEL_THREAD_TERMINATE cl;
        cl.hThread  = (HANDLE) pThread->dwId;
        g_pfnCeLogData(TRUE, CELID_THREAD_TERMINATE, &cl, sizeof(CEL_THREAD_TERMINATE),
                       0, CELZONE_THREAD | CELZONE_PROFILER, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_ThreadDelete(
    PTHREAD pThread
    )
{
    if (IsCeLogZoneEnabled(CELZONE_THREAD)) {
        CEL_THREAD_DELETE cl;
        cl.hThread  = (HANDLE) pThread->dwId;
        g_pfnCeLogData(TRUE, CELID_THREAD_DELETE, &cl, sizeof(CEL_THREAD_DELETE),
                       0, CELZONE_THREAD, 0, FALSE);
    }
}

//----------------------------------------------------------
static void
CELOG_KCLogProcessInfo(            // May be done inside a KCall
    PPROCESS pProcess,             // Process being created
    LPCWSTR  lpProcName,           // Process name, not including path to EXE
    ProcessInfoBuffer* pProcessBuf // Temp storage space to use during this call
    )
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_PROCESS | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {
        DWORD dwVMBase;
        WORD wLen = 0;
        
        // Special case: fixup nk.exe to tell where the code lives, not the VM
        if (pProcess != g_pprcNK) {
            dwVMBase = 0;
        } else {
            dwVMBase = pTOC->physfirst;
        }
        
        // Log base process info
        pProcessBuf->ProcessBase.hProcess = (HANDLE) pProcess->dwId;
        pProcessBuf->ProcessBase.dwVMBase = dwVMBase;
        if (lpProcName) {
            wLen = (WORD) (NKwcsncpy(pProcessBuf->ProcessBase.szName, lpProcName, MAX_PATH) + 1);
        }
        
        g_pfnCeLogData(TRUE, CELID_PROCESS_CREATE, (PVOID) &pProcessBuf->ProcessBase,
                       (WORD) (sizeof(CEL_PROCESS_CREATE) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_PROCESS | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER,
                       0, FALSE);
    }
}

//----------------------------------------------------------
// Similar to GetNameFromOE.
// Returns # chars copied, including NULL terminator, or 0 if none.
static WORD
CELOG_KCFileNameFromOE(         // May be done inside a KCall
    openexe_t* pOE,             // OE from process or module
    LPWSTR     pszFullPath      // Receives full path to file; must be MAX_PATH chars long
    )
{
    WORD wLen = 0;  // either 0 or length INCLUDING null

    if (pOE->filetype & FA_DIRECTROM) {
        LPWSTR pDest;
        LPSTR  pSrc;

        memcpy(pszFullPath, L"\\Windows\\", 9*sizeof(WCHAR));
        wLen = 10;  // 9 + null
        pDest = &pszFullPath[9];
        pSrc = pOE->tocptr->lpszFileName;
        while (*pSrc && (wLen < MAX_PATH)) {
            *pDest = (WCHAR) *pSrc;
            pDest++;
            pSrc++;
            wLen++;
        }
        *pDest = 0;
    
    } else if (pOE->lpName) {
        wLen = (WORD) (NKwcsncpy(pszFullPath, pOE->lpName->name, MAX_PATH) + 1);
    }

    return wLen;
}

//----------------------------------------------------------
static void
CELOG_KCLogProcessInfoEx(          // May be done inside a KCall
    PPROCESS pProcess,
    ProcessInfoBuffer* pProcessBuf // Temp storage space to use during this call
    )
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_PROCESS | CELZONE_LOADER | CELZONE_PROFILER)) {

        WORD  wLen = 0;
    
        // Log extra process info
        pProcessBuf->ProcessExtra.hProcess   = (HANDLE) pProcess->dwId;
        pProcessBuf->ProcessExtra.dwVMLen    = pProcess->e32.e32_vsize;
        pProcessBuf->ProcessExtra.dwOID      = 0;  // The kernel doesn't track OIDs anymore
        
        if (pProcess == g_pprcNK) {
            // Special case: nk.exe code does not run in slot 0
            pProcessBuf->ProcessExtra.dwCodeBase = pTOC->physfirst;
        } else {
            // EXE code other than NK always loads at this addr
            pProcessBuf->ProcessExtra.dwCodeBase = 0x00010000;
        }
        
        wLen = CELOG_KCFileNameFromOE(&pProcess->oe, pProcessBuf->ProcessExtra.szFullPath);
    
        g_pfnCeLogData(TRUE, CELID_EXTRA_PROCESS_INFO, (PVOID) &pProcessBuf->ProcessExtra,
                       (WORD) (sizeof(CEL_EXTRA_PROCESS_INFO) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_PROCESS | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER,
                       0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_ProcessCreateEx(
    PPROCESS pProcess
    )
{
    ProcessInfoBuffer ProcessBuf;
    CELOG_KCLogProcessInfoEx(pProcess, &ProcessBuf);
}

//----------------------------------------------------------
void
CELOG_ProcessTerminate(
    PPROCESS pProcess
    )
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_PROCESS | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {
        CEL_PROCESS_TERMINATE cl;
        cl.hProcess = (HANDLE) pProcess->dwId;
        g_pfnCeLogData(TRUE, CELID_PROCESS_TERMINATE, &cl, sizeof(CEL_PROCESS_TERMINATE),
                       0, CELZONE_PROCESS | CELZONE_MEMTRACKING | CELZONE_PROFILER,
                       0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_ProcessDelete(
    PPROCESS pProcess
    )
{
    if (IsCeLogZoneEnabled(CELZONE_PROCESS)) {
        CEL_PROCESS_DELETE cl;
        cl.hProcess = (HANDLE) pProcess->dwId;
        g_pfnCeLogData(TRUE, CELID_PROCESS_DELETE, &cl, sizeof(CEL_PROCESS_DELETE),
                       0, CELZONE_PROCESS, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_Sleep(
    DWORD dwTimeout
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_SLEEP cl;
        cl.dwTimeout = dwTimeout;
        g_pfnCeLogData(TRUE, CELID_SLEEP, &cl, sizeof(cl),
                       0, CELZONE_SYNCH, 0, FALSE);
    }
}

//----------------------------------------------------------
// Creation for event, mutex, semaphore, critical section objects.
void CELOG_OpenOrCreateHandle(
    PHDATA phd,
    BOOL   fCreate          // TRUE = create, FALSE = open
    )
{
    




    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        // HT_FSMAP is logged from the mapfile code
        switch (phd->pci->type) {
        case HT_EVENT:
            CELOG_EventCreate(phd, fCreate);
            break;
        case HT_MUTEX:
            CELOG_MutexCreate(phd, fCreate);
            break;
        case HT_SEMAPHORE:
            CELOG_SemaphoreCreate(phd, fCreate);
            break;
        case HT_CRITSEC:
            DEBUGCHK(fCreate);
            CELOG_CriticalSectionCreate(phd);
            break;
        default:
            break;
        }
    }
}

//----------------------------------------------------------
static void
CELOG_EventCreate(
    PHDATA phd,
    BOOL   fCreate          // TRUE = create, FALSE = open
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        PEVENT pEvent = (PEVENT) phd->pvObj;
        struct {
            union {
                CEL_EVENT_CREATE cl;
                BYTE _b;  // Work around compiler warning on zero-length array
            };
            WCHAR _sz[MAX_PATH];  // Accessed through cl.szName
        } buf;
        WORD wLen = 0;

        buf.cl.hEvent        = (HANDLE) pEvent;
        buf.cl.fManual       = pEvent->manualreset;
        buf.cl.fInitialState = pEvent->state;
        buf.cl.fCreate       = fCreate;

        if (phd->pName) {
            wLen = (WORD) (NKwcsncpy(buf.cl.szName, phd->pName->name, MAX_PATH) + 1);
        }

        g_pfnCeLogData(TRUE, CELID_EVENT_CREATE, (PVOID) &buf.cl,
                       (WORD) (sizeof(CEL_EVENT_CREATE) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_SYNCH, 0, FALSE);
    }
}

//----------------------------------------------------------
// Event Set, Reset, Pulse
void
CELOG_EventModify(
    PEVENT pEvent,
    DWORD  type
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        switch (type) {
        
        case EVENT_SET: {
            CEL_EVENT_SET cl;
            cl.hEvent = (HANDLE) pEvent;
            g_pfnCeLogData(TRUE, CELID_EVENT_SET, &cl, sizeof(CEL_EVENT_SET),
                           0, CELZONE_SYNCH, 0, FALSE);
            break;
        }
        
        case EVENT_RESET: {
            CEL_EVENT_RESET cl;
            cl.hEvent = (HANDLE) pEvent;
            g_pfnCeLogData(TRUE, CELID_EVENT_RESET, &cl, sizeof(CEL_EVENT_RESET),
                           0, CELZONE_SYNCH, 0, FALSE);
            break;
        }
        
        case EVENT_PULSE: {
            CEL_EVENT_PULSE cl;
            cl.hEvent = (HANDLE) pEvent;
            g_pfnCeLogData(TRUE, CELID_EVENT_PULSE, &cl, sizeof(CEL_EVENT_PULSE),
                           0, CELZONE_SYNCH, 0, FALSE);
            break;
        }
        
        default:
            ;
        }
    }
}

//----------------------------------------------------------
void
CELOG_EventDelete(
    PEVENT pEvent
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_EVENT_DELETE cl;
        cl.hEvent = (HANDLE) pEvent;
        g_pfnCeLogData(TRUE, CELID_EVENT_DELETE, &cl, sizeof(CEL_EVENT_DELETE),
                       0, CELZONE_SYNCH, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_WaitForMultipleObjects(
    DWORD   cObjects,
    PHDATA* phds,
    DWORD   dwTimeout
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        struct {
            union {
                CEL_WAIT_MULTI cl;
                BYTE _b;  // Work around compiler warning on zero-length array
            };
            HANDLE _rgh[MAXIMUM_WAIT_OBJECTS];  // Accessed through cl.hHandles
        } buf;

        // Sanity-check on the handle count
        if (cObjects <= MAXIMUM_WAIT_OBJECTS) {
            DWORD dwObject;

            buf.cl.fWaitAll  = FALSE;
            buf.cl.dwTimeout = dwTimeout;
            for (dwObject = 0; dwObject < cObjects; dwObject++) {
                buf.cl.hHandles[dwObject] = (HANDLE) phds[dwObject]->pvObj;
            }

            g_pfnCeLogData(TRUE, CELID_WAIT_MULTI, (LPVOID) &buf.cl,
                           (WORD) (sizeof(CEL_WAIT_MULTI) + (cObjects * sizeof(HANDLE))),
                           0, CELZONE_SYNCH, 0, FALSE);
        }
    }
}

//----------------------------------------------------------
static void
CELOG_MutexCreate(
    PHDATA phd,
    BOOL   fCreate          // TRUE = create, FALSE = open
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        PMUTEX pMutex = (PMUTEX) phd->pvObj;
        struct {
            union {
                CEL_MUTEX_CREATE cl;
                BYTE _b;  // Work around compiler warning on zero-length array
            };
            WCHAR _sz[MAX_PATH];  // Accessed through cl.szName
        } buf;
        WORD wLen = 0;

        buf.cl.hMutex = (HANDLE) pMutex;
        if (phd->pName) {
            wLen = (WORD) (NKwcsncpy(buf.cl.szName, phd->pName->name, MAX_PATH) + 1);
        }

        g_pfnCeLogData(TRUE, CELID_MUTEX_CREATE, (PVOID) &buf.cl,
                       (WORD) (sizeof(CEL_MUTEX_CREATE) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_SYNCH, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_MutexRelease(
    PMUTEX pMutex
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_MUTEX_RELEASE cl;
        cl.hMutex = (HANDLE) pMutex;
        g_pfnCeLogData(TRUE, CELID_MUTEX_RELEASE, &cl, sizeof(CEL_MUTEX_RELEASE),
                       0, CELZONE_SYNCH, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_MutexDelete(
    PMUTEX pMutex
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_MUTEX_DELETE cl;
        cl.hMutex = (HANDLE) pMutex;
        g_pfnCeLogData(TRUE, CELID_MUTEX_DELETE, &cl, sizeof(CEL_MUTEX_DELETE),
                       0, CELZONE_SYNCH, 0, FALSE);
    }
}

//----------------------------------------------------------
static void
CELOG_SemaphoreCreate(
    PHDATA phd,
    BOOL   fCreate          // TRUE = create, FALSE = open
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        PSEMAPHORE pSemaphore = (PSEMAPHORE) phd->pvObj;
        struct {
            union {
                CEL_SEM_CREATE cl;
                BYTE _b;  // Work around compiler warning on zero-length array
            };
            WCHAR _sz[MAX_PATH];  // Accessed through cl.szName
        } buf;
        WORD wLen = 0;

        buf.cl.hSem        = (HANDLE) pSemaphore;
        buf.cl.dwInitCount = (DWORD) pSemaphore->lCount;
        buf.cl.dwMaxCount  = (DWORD) pSemaphore->lMaxCount;

        if (phd->pName) {
            wLen = (WORD) (NKwcsncpy(buf.cl.szName, phd->pName->name, MAX_PATH) + 1);
        }

        g_pfnCeLogData(TRUE, CELID_SEM_CREATE, (PVOID) &buf.cl,
                       (WORD) (sizeof(CEL_SEM_CREATE) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_SYNCH, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_SemaphoreRelease(
    PSEMAPHORE pSemaphore,
    LONG       lReleaseCount,
    LONG       lPreviousCount
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_SEM_RELEASE cl;
        
        cl.hSem            = (HANDLE) pSemaphore;
        cl.dwReleaseCount  = (DWORD)  lReleaseCount;
        cl.dwPreviousCount = (DWORD)  lPreviousCount;
    
        g_pfnCeLogData(TRUE, CELID_SEM_RELEASE, &cl, sizeof(CEL_SEM_RELEASE),
                       0, CELZONE_SYNCH, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_SemaphoreDelete(
    PSEMAPHORE pSemaphore
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_SEM_DELETE cl;
        cl.hSem = (HANDLE) pSemaphore;
        g_pfnCeLogData(TRUE, CELID_SEM_DELETE, &cl, sizeof(CEL_SEM_DELETE),
                       0, CELZONE_SYNCH, 0, FALSE);
    }
}

//----------------------------------------------------------
static void
CELOG_CriticalSectionCreate(
    PHDATA phd
    )
{
    if (IsCeLogZoneEnabled(CELZONE_CRITSECT)) {
        PCRIT pCriticalSection = (PCRIT) phd->pvObj;
        CEL_CRITSEC_INIT cl;

        






        
        cl.hCS = (HANDLE) pCriticalSection->lpcs;
        g_pfnCeLogData(TRUE, CELID_CS_INIT, &cl, sizeof(CEL_CRITSEC_INIT),
                       0, CELZONE_CRITSECT, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_CriticalSectionDelete(
    PCRIT pCriticalSection
    )
{
    if (IsCeLogZoneEnabled(CELZONE_CRITSECT)) {
        CEL_CRITSEC_DELETE cl;
        cl.hCS = (HANDLE) pCriticalSection->lpcs;
        g_pfnCeLogData(TRUE, CELID_CS_DELETE, &cl, sizeof(CEL_CRITSEC_DELETE),
                       0, CELZONE_CRITSECT, 0, FALSE);
    }
}

//----------------------------------------------------------
// Only logged if we block when entering
void
CELOG_CriticalSectionEnter(
    PCRIT pCriticalSection
    )
{
    if (IsCeLogZoneEnabled(CELZONE_CRITSECT)) {
        CEL_CRITSEC_ENTER cl;
        
#ifdef MIPSIV
        

        if (IsValidKPtr(&cl)) {
            return;
        }
#endif // MIPSIV
    
        cl.hCS          = (HANDLE) pCriticalSection->lpcs;
        cl.hOwnerThread = pCriticalSection->lpcs->OwnerThread;
    
        g_pfnCeLogData(TRUE, CELID_CS_ENTER, &cl, sizeof(CEL_CRITSEC_ENTER),
                       0, CELZONE_CRITSECT, 0, FALSE);
    }
}

//----------------------------------------------------------
// Only logged if another thread was blocked waiting for the CS
void
CELOG_CriticalSectionLeave(
    PCRIT   pCriticalSection,
    PTHREAD pNewOwnerThread     // May be NULL
    )
{
    if (pNewOwnerThread
        && IsCeLogZoneEnabled(CELZONE_CRITSECT)) {
        CEL_CRITSEC_LEAVE cl;
        
        cl.hCS          = (HANDLE) pCriticalSection->lpcs;
        cl.hOwnerThread = (HANDLE) pNewOwnerThread->dwId;
    
        g_pfnCeLogData(TRUE, CELID_CS_LEAVE, &cl, sizeof(CEL_CRITSEC_LEAVE),
                       0, CELZONE_CRITSECT, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_VirtualAlloc(
    PPROCESS pProcess,
    DWORD    dwResult,
    DWORD    dwAddress,
    DWORD    cPages,
    DWORD    dwType,
    DWORD    dwProtect
    )
{
    if (dwResult && IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        CEL_VIRTUAL_ALLOC_EX cl;
    
        cl.hProcess  = (HANDLE) pProcess->dwId;
        cl.dwResult  = dwResult;
        cl.dwAddress = dwAddress;
        cl.dwSize    = cPages * VM_PAGE_SIZE;
        cl.dwType    = dwType;
        cl.dwProtect = dwProtect;
        
        g_pfnCeLogData(TRUE, CELID_VIRTUAL_ALLOC_EX, &cl, sizeof(CEL_VIRTUAL_ALLOC_EX),
                       0, CELZONE_VIRTMEM, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_VirtualCopy(
    PPROCESS pProcessDest,
    DWORD    dwDest,
    PPROCESS pProcessSource,
    DWORD    dwSource,
    DWORD    cPages,
    DWORD    dwPfn,
    DWORD    dwProtect
    )
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        CEL_VIRTUAL_COPY_EX cl;
    
        if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
            dwProtect |= PAGE_PHYSICAL;
        }

        cl.hDestProc = (HANDLE) pProcessDest->dwId;
        cl.dwDest    = dwDest;
        cl.hSrcProc  = (HANDLE) pProcessSource->dwId;
        cl.dwSource  = dwSource;
        cl.dwSize    = cPages * VM_PAGE_SIZE;
        cl.dwProtect = dwProtect;
        
        g_pfnCeLogData(TRUE, CELID_VIRTUAL_COPY_EX, &cl, sizeof(CEL_VIRTUAL_COPY_EX),
                       0, CELZONE_VIRTMEM, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_VirtualFree(
    PPROCESS pProcess,
    DWORD    dwAddress,
    DWORD    cPages,
    DWORD    dwType
    )
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        CEL_VIRTUAL_FREE_EX cl;
    
        cl.hProcess  = (HANDLE) pProcess->dwId;
        cl.dwAddress = dwAddress;
        cl.dwSize    = cPages * VM_PAGE_SIZE;
        cl.dwType    = dwType;
        
        g_pfnCeLogData(TRUE, CELID_VIRTUAL_FREE_EX, &cl, sizeof(CEL_VIRTUAL_FREE_EX),
                       0, CELZONE_VIRTMEM, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_MapFileCreate(
    PFSMAP pMap,
    PNAME  pMapName
    )
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        struct {
            union {
                CEL_MAPFILE_CREATE_EX cl;
                BYTE _b;  // Work around compiler warning on zero-length array
            };
            WCHAR _sz[MAX_PATH];  // Accessed through cl.szName
        } buf;
        WORD wLen = 0;
    
        buf.cl.hMap          = (HANDLE) pMap;
        buf.cl.flProtect     = pMap->dwFlags & ~(MAP_PAGEABLE | MAP_DIRECTROM | MAP_ATOMIC | MAP_GATHER);
        buf.cl.dwMapFlags    = pMap->dwFlags & (MAP_PAGEABLE | MAP_DIRECTROM | MAP_ATOMIC | MAP_GATHER);
        buf.cl.dwMaxSizeHigh = pMap->liSize.HighPart;
        buf.cl.dwMaxSizeLow  = pMap->liSize.LowPart;
    
        if (pMapName) {
            wLen = (WORD) (NKwcsncpy(buf.cl.szName, pMapName->name, MAX_PATH) + 1);
        }
    
        g_pfnCeLogData(TRUE, CELID_MAPFILE_CREATE_EX, (PVOID) &buf.cl,
                       (WORD) (sizeof(CEL_MAPFILE_CREATE_EX) + (wLen * sizeof(WCHAR))),
                       0, CELZONE_VIRTMEM, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_MapFileDestroy(
    PFSMAP pMap
    )
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        CEL_MAPFILE_DESTROY cl;
        cl.hMap = (HANDLE) pMap;
        g_pfnCeLogData(TRUE, CELID_MAPFILE_DESTROY, &cl,
                       sizeof(CEL_MAPFILE_DESTROY),
                       0, CELZONE_VIRTMEM, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_MapFileViewOpen(
    PPROCESS pProcess,
    PMAPVIEW pView
    )
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        CEL_MAPFILE_VIEW_OPEN_EX cl;
        ULARGE_INTEGER liFileOffset;

        cl.hMap             = (HANDLE) pView->phdMap->pvObj;
        cl.hProcess         = (HANDLE) pProcess->dwId;
        cl.lpBaseAddress    = (pView->base.pBaseAddr + pView->wViewOffset);
        
        // Reconstruct user parameters
        if (pView->wFlags & PAGE_READWRITE) {
            cl.dwDesiredAccess = FILE_MAP_WRITE;
        } else {
            cl.dwDesiredAccess = FILE_MAP_READ;
        }
        liFileOffset.QuadPart = pView->base.liMapOffset.QuadPart + pView->wViewOffset;
        cl.dwFileOffsetHigh   = liFileOffset.HighPart;
        cl.dwFileOffsetLow    = liFileOffset.LowPart;
        cl.dwLen              = (pView->base.cbSize - pView->wViewOffset);
        
        g_pfnCeLogData(TRUE, CELID_MAPFILE_VIEW_OPEN_EX, &cl,
                       sizeof(CEL_MAPFILE_VIEW_OPEN_EX),
                       0, CELZONE_VIRTMEM, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_MapFileViewClose(
    PPROCESS pProcess,
    PMAPVIEW pView
    )
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        CEL_MAPFILE_VIEW_CLOSE cl;
        cl.hProcess      = (HANDLE) pProcess->dwId;
        cl.lpBaseAddress = (pView->base.pBaseAddr + pView->wViewOffset);
        g_pfnCeLogData(TRUE, CELID_MAPFILE_VIEW_CLOSE, &cl,
                       sizeof(CEL_MAPFILE_VIEW_CLOSE),
                       0, CELZONE_VIRTMEM, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_MapFileViewFlush(
    PPROCESS pProcess,
    PFSMAP   pfsmap,
    LPBYTE   pFlushStart,
    DWORD    cbSize,
    DWORD    cDirtyPages,
    BOOL     fBegin         // TRUE=beginning of flush, FALSE=end of flush
    )
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM | CELZONE_DEMANDPAGE)) {
        CEL_MAPFILE_VIEW_FLUSH cl;

        cl.lpBaseAddress = pFlushStart;
        cl.dwLen         = cbSize;
        cl.hProcess      = (HANDLE) pProcess->dwId;
        cl.dwNumPages    = cDirtyPages;
        cl.wFlushFlags   = fBegin ? CEL_MAPFLUSH_BEGIN : 0;

        if (pfsmap->dwFlags & MAP_GATHER) {
            cl.wFlushFlags |= CEL_FlushMapGather;
        } else if (pfsmap->dwFlags & MAP_ATOMIC) {
            cl.wFlushFlags |= CEL_FlushMapAtomic;
        } else {
            cl.wFlushFlags |= CEL_FlushMapSimple;
        }
            
        g_pfnCeLogData(TRUE, CELID_MAPFILE_VIEW_FLUSH, &cl, sizeof(CEL_MAPFILE_VIEW_FLUSH),
                       0, CELZONE_VIRTMEM | CELZONE_DEMANDPAGE, 0, FALSE);
    }
}

//----------------------------------------------------------
// Uses the view-flush event, but doesn't really pertain to a view
void
CELOG_MapFileValidate(
    DWORD    cbSize,
    DWORD    dwRestoreSize,
    DWORD    dwRestoreFlags,
    BOOL     fBegin         // TRUE=beginning of flush, FALSE=end of flush
    )
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM | CELZONE_DEMANDPAGE)) {
        CEL_MAPFILE_VIEW_FLUSH cl;

        cl.lpBaseAddress = 0;
        cl.dwLen         = cbSize;
        cl.dwNumPages    = (dwRestoreSize >> 16);
        cl.wFlushFlags   = CEL_ValidateFile | CEL_MAPFLUSH_BEGIN;
        if (FSLOG_RESTORE_FLAG_UNFLUSHED == dwRestoreFlags) {
            cl.wFlushFlags |= CEL_MAPFLUSH_NOWRITEOUT;
        }
            
        g_pfnCeLogData(TRUE, CELID_MAPFILE_VIEW_FLUSH, &cl, sizeof(CEL_MAPFILE_VIEW_FLUSH),
                       0, CELZONE_VIRTMEM | CELZONE_DEMANDPAGE, 0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_PageFault(
    PPROCESS pProcess,
    DWORD    dwAddress,
    BOOL     fBegin,        // TRUE=beginning of page-in, FALSE=end of page-in
    BOOL     fOtherFlag     // BEGIN: r/w flag (1=r/w, 0=r/o)
                            // END:   success flag (1=success, 0=failure)
    )
{
    if (IsCeLogZoneEnabled(CELZONE_DEMANDPAGE)) {
        CEL_SYSTEM_PAGE_IN cl;
    
        memset(&cl, 0, sizeof(cl));
        cl.dwAddress = dwAddress;
        if (fBegin) {
            cl.fReadWrite = fOtherFlag;
        } else {
            cl.fEndPageIn = 1;
            cl.fSuccess   = fOtherFlag;
        }
        cl.hProcess = (HANDLE) pProcess->dwId;
    
        g_pfnCeLogData(TRUE, CELID_SYSTEM_PAGE_IN, &cl, sizeof(CEL_SYSTEM_PAGE_IN),
                       0, CELZONE_DEMANDPAGE, 0, FALSE);
    }
}

//----------------------------------------------------------
static void
CELOG_KCLogModuleInfo(           // May be done inside a KCall
    PMODULE           pModule,
    PPROCESS          pProcess,  // Process that loaded the module, or NULL for first load
    ModuleInfoBuffer* pModuleBuf // Temp storage space to use during this call
    )
{
    WORD wLen;

    // Log base module info
    wLen = 0;
    pModuleBuf->ModuleBase.hProcess = (HANDLE) (pProcess ? pProcess->dwId : (DWORD)-1);
    pModuleBuf->ModuleBase.hModule  = (HANDLE) pModule;
    pModuleBuf->ModuleBase.dwBase   = (DWORD)  pModule->BasePtr;
    if (pModule->lpszModName) {
        wLen = (WORD) (NKwcsncpy(pModuleBuf->ModuleBase.szName, pModule->lpszModName, MAX_PATH) + 1);
    }

    g_pfnCeLogData(TRUE, CELID_MODULE_LOAD, (PVOID) &pModuleBuf->ModuleBase,
                   (WORD)(sizeof(CEL_MODULE_LOAD) + (wLen * sizeof(WCHAR))),
                   0, CELZONE_LOADER | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER,  // CELZONE_THREAD necessary for getting thread names
                   0, FALSE);

    
    // Log extra module info
    pModuleBuf->ModuleExtra.hModule = (HANDLE) pModule;
    pModuleBuf->ModuleExtra.dwVMLen = pModule->e32.e32_vsize;
    
    // Set up the flags based on the module location
    #define RESONLY_FLAGS   (LOAD_LIBRARY_AS_DATAFILE | DONT_RESOLVE_DLL_REFERENCES)
    pModuleBuf->ModuleExtra.dwModuleFlags = 0;
    if (RESONLY_FLAGS == (pModule->wFlags & RESONLY_FLAGS)) {
        pModuleBuf->ModuleExtra.dwModuleFlags |= CEL_MODULE_FLAG_DATAONLY;
    }
    if ((int)pModule->BasePtr < 0) {
        pModuleBuf->ModuleExtra.dwModuleFlags |= CEL_MODULE_FLAG_KERNEL;
    }

    pModuleBuf->ModuleExtra.dwOID   = 0;  // The kernel doesn't track OIDs anymore
    wLen = CELOG_KCFileNameFromOE(&pModule->oe, pModuleBuf->ModuleExtra.szFullPath);

    g_pfnCeLogData(TRUE, CELID_EXTRA_MODULE_INFO, (PVOID) &pModuleBuf->ModuleExtra,
                   (WORD)(sizeof(CEL_EXTRA_MODULE_INFO) + (wLen * sizeof(WCHAR))),
                   0, CELZONE_LOADER | CELZONE_PROFILER, 0, FALSE);
}

//----------------------------------------------------------
void
CELOG_ModuleLoad(
    PPROCESS pProcess,  // Process that loaded the module, or NULL for first load
    PMODULE  pModule
    )
{
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_LOADER | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {
        ModuleInfoBuffer ModuleBuf; // Temp buffer on stack
        
        CELOG_KCLogModuleInfo(pModule, pProcess, &ModuleBuf);
    }
}

//----------------------------------------------------------
void
CELOG_ModuleFree(
    PPROCESS pProcess,  // Process that unloaded the module, or NULL for final unload
    PMODULE  pModule
    )
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_LOADER | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {

        CEL_MODULE_FREE cl;
        
        cl.hProcess = (HANDLE) (pProcess ? pProcess->dwId : (DWORD)-1);
        cl.hModule  = (HANDLE) pModule;
    
        g_pfnCeLogData(TRUE, CELID_MODULE_FREE, &cl, sizeof(CEL_MODULE_FREE),
                       0, CELZONE_LOADER | CELZONE_MEMTRACKING | CELZONE_PROFILER,
                       0, FALSE);
    }
}

//----------------------------------------------------------
void
CELOG_LaunchingFilesys()
{
    if (IsCeLogZoneEnabled(CELZONE_BOOT_TIME)) {
        CEL_BOOT_TIME cl;
        cl.dwAction = BOOT_TIME_LAUNCHING_FS;
        g_pfnCeLogData(TRUE, CELID_BOOT_TIME, &cl, sizeof(CEL_BOOT_TIME),
                       0, CELZONE_BOOT_TIME, 0, FALSE);
    }
}


//------------------------------------------------------------------------------
//
// CELOG RESYNC:  These functions are very similar to those above, except they
// use a global data buffer to minimize stack usage.  The global buffer is
// thread-safe due to the fact that the re-sync is done as a KCall.
//
//------------------------------------------------------------------------------

union {  // Union so that the buffer is the max of all these and aligned properly
    ProcessInfoBuffer ProcessBuf;
    ThreadInfoBuffer  ThreadBuf;
    ModuleInfoBuffer  ModuleBuf;
    CEL_SYSTEM_INVERT PriorityInvertBuf;
} g_CeLogSyncBuffer;


//------------------------------------------------------------------------------
// Helper func to generate CeLog process events, minimizing stack usage.
//------------------------------------------------------------------------------
void
CELOG_SyncProcess(      // Done inside a KCall
    PPROCESS pProcess
    )
{
    CELOG_KCLogProcessInfo(pProcess, pProcess->lpszProcName, &g_CeLogSyncBuffer.ProcessBuf);
    CELOG_KCLogProcessInfoEx(pProcess, &g_CeLogSyncBuffer.ProcessBuf);
}


//------------------------------------------------------------------------------
// Helper func to generate a CeLog thread create event, minimizing stack usage.
//------------------------------------------------------------------------------
void
CELOG_SyncThread(      // Done inside a KCall
    PTHREAD  pThread,
    PPROCESS pProcess
    )
{
    CELOG_KCLogThreadInfo(pThread, pProcess, NULL, &g_CeLogSyncBuffer.ThreadBuf);

    // Log priority inversion if necessary
    if (GET_BPRIO(pThread) != GET_CPRIO(pThread)) {
        CELOG_KCLogPriorityInvert(pThread, &g_CeLogSyncBuffer.PriorityInvertBuf);
    }
}


//------------------------------------------------------------------------------
// Helper func to generate CeLog module events, minimizing stack usage.
//------------------------------------------------------------------------------
void
CELOG_SyncModule(      // Done inside a KCall
    PMODULE pModule
    )
{
    CELOG_KCLogModuleInfo(pModule, NULL, &g_CeLogSyncBuffer.ModuleBuf);
}


//------------------------------------------------------------------------------
// Helper func to generate a CeLog module reference-count list event.
// Since we're always inside a KCall, uses global state to store the refcounts
// until we've accumulated the entire list of process refcounts, or exceeded
// the maximum.  It's tough to alloc memory during a KCall so max out instead.
//------------------------------------------------------------------------------
BOOL
CELOG_SyncModuleRefCount(
    PMODULELIST pml,
    PPROCESS    pProcess,   // May be NULL
    BOOL        Reset       // Used only if pProcess=NULL
    )
{
    ModuleInfoBuffer* pModuleBuf = &g_CeLogSyncBuffer.ModuleBuf;

    DEBUGCHK (InSysCall());  // Must be in a KCall

    // Called numerous times to build up global state before finally logging to CeLog:
    // 1. pProcess=NULL + Reset=1 restarts enumeration at the beginning.
    // 2. pProcess=non-null adds one process refcount to the array, up to a max.
    // 3. pProcess=NULL + Reset=0 logs the array to CeLog and ends enumeration.

    if (pProcess) {
        // Add one process refcount to the array, up to a max
        if (pModuleBuf->NumRefs < MAX_MODULE_REFERENCES) {
            pModuleBuf->ModuleRef.ref[pModuleBuf->NumRefs].hProcess   = (HANDLE) pProcess->dwId;
            pModuleBuf->ModuleRef.ref[pModuleBuf->NumRefs].dwRefCount = pml->wRefCnt;
            pModuleBuf->NumRefs++;
        } else {
            return FALSE;
        }

    } else if (Reset) {
        // Restart enumeration at the beginning
        pModuleBuf->NumRefs = 0;
        pModuleBuf->ModuleRef.hModule = (HANDLE) pml->pMod;

    } else {
        // Log the array to CeLog
        if (pModuleBuf->NumRefs) {
            WORD wLen = sizeof(CEL_MODULE_REFERENCES)
                        + ((pModuleBuf->NumRefs - 1) * sizeof(CEL_PROCESS_REFCOUNT));  // -1 since first ref is in CEL_MODULE_REFERENCES
            g_pfnCeLogData(TRUE, CELID_MODULE_REFERENCES, (PVOID) &pModuleBuf->ModuleRef,
                           wLen, 0, CELZONE_LOADER | CELZONE_PROFILER, 0, FALSE);
        }
        pModuleBuf->NumRefs = 0;
    }

    return TRUE;
}
