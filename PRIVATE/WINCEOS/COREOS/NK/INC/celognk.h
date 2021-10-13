//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//  
//------------------------------------------------------------------------------
//
//  Module Name:  
//  
//      celognk.h
//  
//  Abstract:  
//
//      Kernel's interface to the event logging functions.
//      
//------------------------------------------------------------------------------
#ifndef __CELOGNK_H_
#define __CELOGNK_H_

#ifdef IN_KERNEL

//------------------------------------------------------------------------------
//  Logging operation
//

// Include the public SDK structures.                
#include <celog.h>

// Include exported profiler functions
#include <profiler.h>


// Globals that allow a logging engine to be hot-plugged in the system.
#ifdef __cplusplus
extern void (*g_pfnCeLogData)(...);
#else
extern void (*g_pfnCeLogData)();
#endif
extern PFNVOID g_pfnCeLogInterrupt;
extern PFNVOID g_pfnCeLogSetZones;

// Instead of using the CELOGDATA macro in these inline functions, call the
// function pointer directly.
#define CELOGDATANK(T,I,D,L,Y,Z)        g_pfnCeLogData(T,I,D,L,Y,Z,0,FALSE) 

#undef IsCeLogStatus
#undef IsCeLogEnabled
#undef IsCeLogZoneEnabled
#define IsCeLogStatus(Status)           ((KInfoTable[KINX_CELOGSTATUS] & (Status)) != 0)
#define IsCeLogEnabled(Status, ZoneCE)  (IsCeLogStatus(Status) && ((KInfoTable[KINX_CELOGSTATUS] & (ZoneCE)) != 0))
#define IsCeLogZoneEnabled(ZoneCE)      (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL, (ZoneCE)))

// Set the proper status and zones
#define CeLogEnable(Status, ZoneCE)     (KInfoTable[KINX_CELOGSTATUS] = ((Status) | ((ZoneCE) & ~CELOGSTATUS_ENABLED_ANY)))
// Set the proper status without changing the zones
#define CeLogEnableStatus(Status)       (CeLogEnable((Status), KInfoTable[KINX_CELOGSTATUS] & ~CELOGSTATUS_ENABLED_ANY))
// Set the proper zones without changing the status
#define CeLogEnableZones(ZoneCE)        (CeLogEnable(KInfoTable[KINX_CELOGSTATUS] & CELOGSTATUS_ENABLED_ANY, (ZoneCE)))
// Disable all logging
#define CeLogDisable()                  (KInfoTable[KINX_CELOGSTATUS] = 0)


//------------------------------------------------------------------------------
// ZONES

// KCall profiling is turned off by default to prevent overwhelming logs
#ifndef CELOG_USER_MASK
#define CELOG_USER_MASK    0xFFBFFFFF
#endif

#ifndef CELOG_CE_MASK
#define CELOG_CE_MASK      0xFFBFFFFF
#endif

#ifndef CELOG_PROCESS_MASK
#define CELOG_PROCESS_MASK 0xFFFFFFFF
#endif


//------------------------------------------------------------------------------
// Interface to a CeLog plugin DLL
//

#define CELOG_IMPORT_VERSION            4
#define CELOG_PRIVATE_IMPORT_VERSION    1
#define CELOG_EXPORT_VERSION            2

// Private kernel functions and data provided for CeLog to use.
// Queried with IOCTL_CELOG_IMPORT_PRIVATE.
// This structure may change on future OS versions.
typedef struct _CeLogPrivateImports {
    DWORD    dwVersion;             // Version of this structure, set to CELOG_PRIVATE_IMPORT_VERSION
    FARPROC  pRegQueryValueExW;     // Pointer to NKRegQueryValueExW function
    FARPROC  pINTERRUPTS_ENABLE;    // Pointer to INTERRUPTS_ENABLE function
    FARPROC  pPhys2Virt;            // Pointer to Phys2Virt wrapper function
    FARPROC  pKCall;                // Pointer to KCall function
    FARPROC  pCeLogData;            // Pointer to kernel's CeLogData entry point
    struct KDataStruct *pKData;     // Pointer to KData struct
} CeLogPrivateImports;


// The kernel wrappers

//----------------------------------------------------------
_inline void CELOG_ThreadSwitch(PTHREAD pThread)
{
    // Logged for both general and profiling mode, all zones, because celog.dll
    // requires thread switches to know when to flush its small buffer.
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {

        CEL_THREAD_SWITCH cl;

        if (pThread) {
            cl.hThread = pThread->hTh;
        } else {
            cl.hThread = 0;
        }

        CELOGDATANK(TRUE, CELID_THREAD_SWITCH, &cl, sizeof(cl), 0, CELZONE_RESCHEDULE | CELZONE_PROFILER);
    }
}

//----------------------------------------------------------
_inline void CELOG_ThreadQuantumExpire()
{
    if (IsCeLogZoneEnabled(CELZONE_RESCHEDULE)) {
        CELOGDATANK(TRUE, CELID_THREAD_QUANTUMEXPIRE, NULL, 0, 0, CELZONE_RESCHEDULE);
    }
}

//----------------------------------------------------------
_inline void CELOG_ThreadSetPriority(HANDLE hThread, int nPriority)
{
    if (IsCeLogZoneEnabled(CELZONE_THREAD)) {
        CEL_THREAD_PRIORITY cl;
        
        cl.hThread = hThread;
        cl.nPriority = nPriority;
    
        CELOGDATANK(TRUE, CELID_THREAD_PRIORITY, &cl, sizeof(CEL_THREAD_PRIORITY), 0, CELZONE_THREAD);
    }
}

//----------------------------------------------------------
_inline void CELOG_ThreadSetQuantum(HANDLE hThread, DWORD dwQuantum)
{
    if (IsCeLogZoneEnabled(CELZONE_THREAD)) {
        CEL_THREAD_QUANTUM cl;
        
        cl.hThread = hThread;
        cl.dwQuantum = dwQuantum;
    
        CELOGDATANK(TRUE, CELID_THREAD_QUANTUM, &cl, sizeof(CEL_THREAD_QUANTUM), 0, CELZONE_THREAD);
    }
}

//----------------------------------------------------------
_inline void CELOG_ThreadSuspend(HANDLE hThread)
{
    if (IsCeLogZoneEnabled(CELZONE_RESCHEDULE)) {
        CEL_THREAD_SUSPEND cl;
        cl.hThread = hThread;
        CELOGDATANK(TRUE, CELID_THREAD_SUSPEND, &cl, sizeof(CEL_THREAD_SUSPEND), 0, CELZONE_RESCHEDULE);
    }
}

//----------------------------------------------------------
_inline void CELOG_ThreadResume(HANDLE hThread)
{
    if (IsCeLogZoneEnabled(CELZONE_RESCHEDULE)) {
        CEL_THREAD_RESUME cl;
        cl.hThread = hThread;
        CELOGDATANK(TRUE, CELID_THREAD_RESUME, &cl, sizeof(CEL_THREAD_RESUME), 0, CELZONE_RESCHEDULE);
    }
}

//----------------------------------------------------------
_inline void CELOG_ThreadCreate(PTHREAD pThread, HANDLE hProcess, 
                                LPCWSTR lpProcName, BOOL fSuspended)
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_THREAD | CELZONE_PROFILER)
        && pThread) {

        BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_THREAD_CREATE)];
        PCEL_THREAD_CREATE pcl = (PCEL_THREAD_CREATE) pTmp;
        WORD wLen = 0;

        pcl->hProcess    = hProcess;
        pcl->hThread     = pThread->hTh;
        pcl->hModule     = (HANDLE)0;  // The reader can figure out hModule from dwStartAddr
        pcl->dwStartAddr = pThread->dwStartAddr;
        pcl->nPriority   = pThread->bBPrio;

        // If the process name was passed in, we assume this is the primary
        // thread of a process, and take the process' name and module handle.
        // Otherwise, look up the thread's function name and module handle.
        if (lpProcName) {
            wLen = strlenW(lpProcName) + 1;
            if (wLen <= MAX_PATH) {
                kstrcpyW(pcl->szName, lpProcName); 
            } else {
                wLen = 0;
            }
#ifdef NKPROF  // GetThreadName only available in profiling builds
        } else {
            GetThreadName(pThread, &(pcl->hModule), pcl->szName);
            if (pcl->szName[0] != 0) {
                wLen = strlenW(pcl->szName) + 1;
            }
#endif // NKPROF
        }
        
        CELOGDATANK(TRUE, CELID_THREAD_CREATE, (PVOID) pcl, 
                    (WORD)(sizeof(CEL_THREAD_CREATE) + (wLen * sizeof(WCHAR))),
                    0, CELZONE_THREAD | CELZONE_PROFILER);
    }
    
    if (fSuspended && pThread) {
        CELOG_ThreadSuspend(pThread->hTh);
    }
}

//----------------------------------------------------------
_inline void CELOG_ThreadCloseHandle(HANDLE hThread)
{
    if (IsCeLogZoneEnabled(CELZONE_THREAD)) {
        CEL_THREAD_CLOSE cl;
        cl.hThread  = hThread;
        CELOGDATANK(TRUE, CELID_THREAD_CLOSE, &cl, sizeof(CEL_THREAD_CLOSE), 0, CELZONE_THREAD);
    }
}

//----------------------------------------------------------
_inline void CELOG_ThreadTerminate(HANDLE hThread)
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_THREAD | CELZONE_PROFILER)) {
        CEL_THREAD_TERMINATE cl;
        cl.hThread  = hThread;
        CELOGDATANK(TRUE, CELID_THREAD_TERMINATE, &cl, sizeof(CEL_THREAD_TERMINATE), 0, CELZONE_THREAD | CELZONE_PROFILER);
    }
}

//----------------------------------------------------------
_inline void CELOG_ThreadDelete(HANDLE hThread)
{
    if (IsCeLogZoneEnabled(CELZONE_THREAD)) {
        CEL_THREAD_DELETE cl;
        cl.hThread  = hThread;
        CELOGDATANK(TRUE, CELID_THREAD_DELETE, &cl, sizeof(CEL_THREAD_DELETE), 0, CELZONE_THREAD);
    }
}

//----------------------------------------------------------
_inline void CELOG_ProcessCreate(
    HANDLE  hProcess,       // Process being created
    LPCWSTR lpProcName,     // Process name, not including path to EXE
    DWORD   dwVMBase        // VM base address the process is loaded at
    )
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_PROCESS | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {

        BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_PROCESS_CREATE)];
        PCEL_PROCESS_CREATE pclBase = (PCEL_PROCESS_CREATE) pTmp;
        WORD wLen;
        
        // Special case: fixup nk.exe to tell where the code lives, not the VM
        if (dwVMBase == (SECURE_SECTION << VA_SECTION)) {
            dwVMBase = pTOC->physfirst;
        }
        
        // Log base process info
        wLen = 0;
        pclBase->hProcess = hProcess;
        pclBase->dwVMBase = dwVMBase;
        if (lpProcName) {
            wLen = strlenW(lpProcName) + 1;
            if (wLen <= MAX_PATH) {
                kstrcpyW(pclBase->szName, lpProcName); 
            } else {
                wLen = 0;
            }
        }
        
        CELOGDATANK(TRUE, CELID_PROCESS_CREATE, (PVOID) pclBase,
                    (WORD) (sizeof(CEL_PROCESS_CREATE) + (wLen * sizeof(WCHAR))), 0,
                    CELZONE_PROCESS | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER);
    }
}

//----------------------------------------------------------
// Similar to GetNameFromOE.
// Returns # chars copied, including NULL terminator, or 0 if none.
_inline WORD CELOG_FileInfoFromOE(
    openexe_t* pOE,             // OE from process or module
    DWORD*     pdwOID,          // Receives file OID, if applicable
    LPWSTR     pszFullPath      // Receives full path to file; must be MAX_PATH chars long
    )
{
    WORD wLen = 0;  // either 0 or length INCLUDING null

    *pdwOID = 0;

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
    
    } else if (pOE->bIsOID) {
        *pdwOID = (DWORD) pOE->ceOid;
        wLen = 0;  // Full path is unavailable, can be obtained from OID
    
    } else if (pOE->lpName && pOE->lpName->name) {
        wLen = strlenW(pOE->lpName->name) + 1; // +1 for null
        if (wLen <= MAX_PATH) {
            kstrcpyW(pszFullPath, pOE->lpName->name); 
        } else {
            wLen = 0;
        }
    }

    return wLen;
}

//----------------------------------------------------------
_inline void CELOG_ProcessCreateEx(
    PPROCESS pProc
    )
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_PROCESS | CELZONE_LOADER | CELZONE_PROFILER)) {

        BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_EXTRA_PROCESS_INFO)];
        PCEL_EXTRA_PROCESS_INFO pclExtra = (PCEL_EXTRA_PROCESS_INFO) pTmp;
        WORD wLen;
        
        // Log extra process info
        pclExtra->hProcess = pProc->hProc;
        pclExtra->dwVMLen = pProc->e32.e32_vsize;
        wLen = CELOG_FileInfoFromOE(&pProc->oe, &pclExtra->dwOID,
                                    pclExtra->szFullPath);

        CELOGDATANK(TRUE, CELID_EXTRA_PROCESS_INFO, (PVOID) pclExtra,
                    (WORD) (sizeof(CEL_EXTRA_PROCESS_INFO) + (wLen * sizeof(WCHAR))), 0,
                    CELZONE_PROCESS | CELZONE_LOADER | CELZONE_PROFILER);
    }
}

//----------------------------------------------------------
_inline void CELOG_ProcessCloseHandle(HANDLE hProcess)
{
    if (IsCeLogZoneEnabled(CELZONE_PROCESS)) {
        CEL_PROCESS_CLOSE cl;
        cl.hProcess = hProcess;
        CELOGDATANK(TRUE, CELID_PROCESS_CLOSE, &cl, sizeof(CEL_PROCESS_CLOSE), 0, CELZONE_PROCESS);
    }
}

//----------------------------------------------------------
_inline void CELOG_ProcessTerminate(HANDLE hProcess)
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_PROCESS | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {
        CEL_PROCESS_TERMINATE cl;
        cl.hProcess = hProcess;
        CELOGDATANK(TRUE, CELID_PROCESS_TERMINATE, &cl, sizeof(CEL_PROCESS_TERMINATE), 0, CELZONE_PROCESS | CELZONE_MEMTRACKING | CELZONE_PROFILER);
    }
}

//----------------------------------------------------------
_inline void CELOG_ProcessDelete(HANDLE hProcess)
{
    if (IsCeLogZoneEnabled(CELZONE_PROCESS)) {
        CEL_PROCESS_DELETE cl;
        cl.hProcess = hProcess;
        CELOGDATANK(TRUE, CELID_PROCESS_DELETE, &cl, sizeof(CEL_PROCESS_DELETE), 0, CELZONE_PROCESS);
    }
}

//----------------------------------------------------------
_inline void CELOG_Sleep(DWORD dwTimeout)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_SLEEP cl;
        cl.dwTimeout = dwTimeout;
        CELOGDATANK(TRUE, CELID_SLEEP, &cl, sizeof(cl), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_EventCreate(HANDLE hEvent, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName, BOOL fCreate)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_EVENT_CREATE)];
        WORD wLen = 0;
        PCEL_EVENT_CREATE pcl = (PCEL_EVENT_CREATE) pTmp;
        
        pcl->hEvent = hEvent;
        pcl->fManual = fManReset;
        pcl->fInitialState = fInitState;
        pcl->fCreate = fCreate;
    
        if (lpEventName) {
            wLen = strlenW(lpEventName) + 1;
            if (wLen <= MAX_PATH) {
                kstrcpyW(pcl->szName, lpEventName); 
            } else {
                wLen = 0;
            }
        }
    
        CELOGDATANK(TRUE, CELID_EVENT_CREATE, (PVOID) pcl, (WORD) (sizeof(CEL_EVENT_CREATE) + (wLen * sizeof(WCHAR))), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
// Event Set, Reset, Pulse
_inline void CELOG_Event(HANDLE hEvent, DWORD type)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        switch (type) {
        
        case EVENT_SET: {
            CEL_EVENT_SET cl;
            cl.hEvent = hEvent;
            CELOGDATANK(TRUE, CELID_EVENT_SET, &cl, sizeof(CEL_EVENT_SET), 0, CELZONE_SYNCH);
            break;
        }
        
        case EVENT_RESET: {
            CEL_EVENT_RESET cl;
            cl.hEvent = hEvent;
            CELOGDATANK(TRUE, CELID_EVENT_RESET, &cl, sizeof(CEL_EVENT_RESET), 0, CELZONE_SYNCH);
            break;
        }
        
        case EVENT_PULSE: {
            CEL_EVENT_PULSE cl;
            cl.hEvent = hEvent;
            CELOGDATANK(TRUE, CELID_EVENT_PULSE, &cl, sizeof(CEL_EVENT_PULSE), 0, CELZONE_SYNCH);
            break;
        }
        
        default:
            ;
        }
    }
}

//----------------------------------------------------------
_inline void CELOG_EventCloseHandle(HANDLE hEvent)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_EVENT_CLOSE cl;
        cl.hEvent = hEvent;
        CELOGDATANK(TRUE, CELID_EVENT_CLOSE, &cl, sizeof(CEL_EVENT_CLOSE), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_EventDelete(HANDLE hEvent)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_EVENT_DELETE cl;
        cl.hEvent = hEvent;
        CELOGDATANK(TRUE, CELID_EVENT_DELETE, &cl, sizeof(CEL_EVENT_DELETE), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_WaitForMultipleObjects(
    DWORD dwNumObjects,
    CONST HANDLE* lpHandles,
    BOOL fWaitAll,
    DWORD dwTimeout
    )
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        BYTE pTmp[(MAXIMUM_WAIT_OBJECTS * sizeof(HANDLE)) + sizeof(CEL_WAIT_MULTI)];
        PCEL_WAIT_MULTI pcl = (PCEL_WAIT_MULTI) pTmp;
        
        // Impose a limit on the number of handles logged
        if (dwNumObjects > MAXIMUM_WAIT_OBJECTS) {
            dwNumObjects = MAXIMUM_WAIT_OBJECTS;
        }
        
        pcl->fWaitAll = fWaitAll;
        pcl->dwTimeout = dwTimeout;
        memcpy(pcl->hHandles, lpHandles, dwNumObjects * sizeof(HANDLE));
    
        CELOGDATANK(TRUE, CELID_WAIT_MULTI, (LPVOID) pcl, (WORD) (sizeof(CEL_WAIT_MULTI) + (dwNumObjects * sizeof(HANDLE))), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_MutexCreate(HANDLE hMutex, LPMUTEX lpMutex)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_MUTEX_CREATE)];
        PCEL_MUTEX_CREATE pcl = (PCEL_MUTEX_CREATE) pTmp;
        WORD wLen = 0;
        
        pcl->hMutex = hMutex;
        if (lpMutex && lpMutex->name && lpMutex->name->name) {
            wLen = strlenW(lpMutex->name->name) + 1;
            if (wLen <= MAX_PATH) {
                kstrcpyW(pcl->szName, lpMutex->name->name);
            } else {
                wLen = 0;
            }
        }
    
        CELOGDATANK(TRUE, CELID_MUTEX_CREATE, (PVOID) pcl, (WORD) (sizeof(CEL_MUTEX_CREATE) + (wLen * sizeof(WCHAR))), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_MutexRelease(HANDLE hMutex)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_MUTEX_RELEASE cl;
        cl.hMutex = hMutex;
        CELOGDATANK(TRUE, CELID_MUTEX_RELEASE, &cl, sizeof(CEL_MUTEX_RELEASE), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_MutexCloseHandle(HANDLE hMutex)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_MUTEX_CLOSE cl;
        cl.hMutex = hMutex;
        CELOGDATANK(TRUE, CELID_MUTEX_CLOSE, &cl, sizeof(CEL_MUTEX_CLOSE), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_MutexDelete(HANDLE hMutex)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_MUTEX_DELETE cl;
        cl.hMutex = hMutex;
        CELOGDATANK(TRUE, CELID_MUTEX_DELETE, &cl, sizeof(CEL_MUTEX_DELETE), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_SemaphoreCreate(HANDLE hSem, LONG lInitCount, LONG lMaxCount, LPCWSTR szName)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_SEM_CREATE)];
        PCEL_SEM_CREATE pcl = (PCEL_SEM_CREATE) pTmp;
        WORD wLen = 0;
        
        pcl->hSem = hSem;
        pcl->dwInitCount = (DWORD) lInitCount;
        pcl->dwMaxCount = (DWORD) lMaxCount;
        
        if (szName) {
            wLen = strlenW(szName) + 1;
            if (wLen <= MAX_PATH) {
                kstrcpyW(pcl->szName, szName);
            } else {
                wLen = 0;
            }
        }
    
        CELOGDATANK(TRUE, CELID_SEM_CREATE, (PVOID) pcl, (WORD) (sizeof(CEL_SEM_CREATE) + (wLen * sizeof(WCHAR))), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_SemaphoreRelease(HANDLE hSem, LONG lReleaseCount, LONG lPreviousCount)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_SEM_RELEASE cl;
        
        cl.hSem = hSem;
        cl.dwReleaseCount = (DWORD) lReleaseCount;
        cl.dwPreviousCount = (DWORD) lPreviousCount;
    
        CELOGDATANK(TRUE, CELID_SEM_RELEASE, &cl, sizeof(CEL_SEM_RELEASE), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_SemaphoreCloseHandle(HANDLE hSem)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_SEM_CLOSE cl;
        cl.hSem = hSem;
        CELOGDATANK(TRUE, CELID_SEM_CLOSE, &cl, sizeof(CEL_SEM_CLOSE), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_SemaphoreDelete(HANDLE hSem)
{
    if (IsCeLogZoneEnabled(CELZONE_SYNCH)) {
        CEL_SEM_DELETE cl;
        cl.hSem = hSem;
        CELOGDATANK(TRUE, CELID_SEM_DELETE, &cl, sizeof(CEL_SEM_DELETE), 0, CELZONE_SYNCH);
    }
}

//----------------------------------------------------------
_inline void CELOG_CSInit(CRITICAL_SECTION *lpcs)
{
    if (IsCeLogZoneEnabled(CELZONE_CRITSECT)) {
        CEL_CRITSEC_INIT cl;
        cl.hCS = (HANDLE)MapPtrProc(lpcs, pCurProc);
        CELOGDATANK(TRUE, CELID_CS_INIT, &cl, sizeof(CEL_CRITSEC_INIT), 0, CELZONE_CRITSECT);
    }
}

//----------------------------------------------------------
_inline void CELOG_CSDelete(CRITICAL_SECTION *lpcs)
{
    if (IsCeLogZoneEnabled(CELZONE_CRITSECT)) {
        CEL_CRITSEC_DELETE cl;
        cl.hCS = (HANDLE)MapPtrProc(lpcs, pCurProc);
        CELOGDATANK(TRUE, CELID_CS_DELETE, &cl, sizeof(CEL_CRITSEC_DELETE), 0, CELZONE_CRITSECT);
    }
}

//----------------------------------------------------------
// Only logged if we block when entering
_inline void CELOG_CSEnter(CRITICAL_SECTION *lpcs, HANDLE hOwnerThread)
{
    if (IsCeLogZoneEnabled(CELZONE_CRITSECT)) {
        CEL_CRITSEC_ENTER cl;
        
#ifdef MIPSIV
        

        if (IsValidKPtr(&cl)) {
            return;
        }
#endif // MIPSIV
    
        cl.hCS = (HANDLE)MapPtrProc(lpcs, pCurProc);
        cl.hOwnerThread = hOwnerThread;
    
        CELOGDATANK(TRUE, CELID_CS_ENTER, &cl, sizeof(CEL_CRITSEC_ENTER), 0, CELZONE_CRITSECT);
    }
}

//----------------------------------------------------------
// Only logged if another thread was blocked waiting for the CS
_inline void CELOG_CSLeave(CRITICAL_SECTION *lpcs, HANDLE hNewOwner)
{
    if (IsCeLogZoneEnabled(CELZONE_CRITSECT)) {
        CEL_CRITSEC_LEAVE cl;
        
#ifdef MIPSIV
        

        if (IsValidKPtr(&cl)) {
            return;
        }
#endif // MIPSIV
    
        cl.hCS = (HANDLE)MapPtrProc(lpcs, pCurProc);
        cl.hOwnerThread = hNewOwner;
    
        CELOGDATANK(TRUE, CELID_CS_LEAVE, &cl, sizeof(CEL_CRITSEC_LEAVE), 0, CELZONE_CRITSECT);
    }
}

//----------------------------------------------------------
_inline void CELOG_VirtualAlloc(DWORD dwResult, DWORD dwAddress, DWORD dwSize, DWORD dwType, DWORD dwProtect)
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        CEL_VIRTUAL_ALLOC cl;
    
        cl.dwResult = dwResult;
        cl.dwAddress = dwAddress;
        cl.dwSize = dwSize;
        cl.dwType = dwType;
        cl.dwProtect = dwProtect;
        
        CELOGDATANK(TRUE, CELID_VIRTUAL_ALLOC, &cl, sizeof(CEL_VIRTUAL_ALLOC), 0, CELZONE_VIRTMEM);
    }
}

//----------------------------------------------------------
_inline void CELOG_VirtualCopy(DWORD dwDest, DWORD dwSource, DWORD dwSize, DWORD dwProtect)
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        CEL_VIRTUAL_COPY cl;
    
        cl.dwDest = dwDest;
        cl.dwSource = dwSource;
        cl.dwSize = dwSize;
        cl.dwProtect = dwProtect;
        
        CELOGDATANK(TRUE, CELID_VIRTUAL_COPY, &cl, sizeof(CEL_VIRTUAL_COPY), 0, CELZONE_VIRTMEM);
    }
}

//----------------------------------------------------------
_inline void CELOG_VirtualFree(DWORD dwAddress, DWORD dwSize, DWORD dwType)
{
    if (IsCeLogZoneEnabled(CELZONE_VIRTMEM)) {
        CEL_VIRTUAL_FREE cl;
    
        cl.dwAddress = dwAddress;
        cl.dwSize = dwSize;
        cl.dwType = dwType;
        
        CELOGDATANK(TRUE, CELID_VIRTUAL_FREE, &cl, sizeof(CEL_VIRTUAL_FREE), 0, CELZONE_VIRTMEM);
    }
}

//----------------------------------------------------------
_inline void CELOG_SystemPage(DWORD dwAddress, BOOL fReadWrite)
{
    if (IsCeLogZoneEnabled(CELZONE_DEMANDPAGE)) {
        CEL_SYSTEM_PAGE cl;
        
        cl.dwAddress = dwAddress;
        cl.fReadWrite = fReadWrite;
    
        CELOGDATANK(TRUE, CELID_SYSTEM_PAGE, &cl, sizeof(CEL_SYSTEM_PAGE), 0, CELZONE_DEMANDPAGE);
    }
}

//----------------------------------------------------------
_inline void CELOG_SystemInvert(HANDLE hThread, int nPriority)
{
    if (IsCeLogZoneEnabled(CELZONE_PRIORITYINV)) {
        CEL_SYSTEM_INVERT cl;
        
        cl.hThread = hThread;
        cl.nPriority = nPriority;
    
        CELOGDATANK(TRUE, CELID_SYSTEM_INVERT, &cl, sizeof(CEL_SYSTEM_INVERT), 0, CELZONE_PRIORITYINV);
    }
}


#define CELOG_ModuleFlags(dwVMBase)                                            \
    (IsKernelVa(dwVMBase) ? CEL_MODULE_FLAG_KERNEL                             \
     : (IsInResouceSection(dwVMBase) ? CEL_MODULE_FLAG_DATAONLY : 0))


//----------------------------------------------------------
_inline void CELOG_ModuleLoad(
    HANDLE  hProcess,       // Process that loaded the module, or (-1) for first load
    PMODULE pMod
    )
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_LOADER | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER)
        && pMod) {

        union {  // Union so that the buffer is the max of all these and aligned properly
            struct {
                union {
                    CEL_MODULE_LOAD         ModuleBase;
                    CEL_EXTRA_MODULE_INFO   ModuleExtra;
                    BYTE                    bUnused;     // Work around compiler warning
                };
                WCHAR _sz[MAX_PATH];  // Not used directly; string starts at end of data above
            };
            struct {
                CEL_MODULE_REFERENCES       ModuleRef;
                CEL_PROCESS_REFCOUNT        _ref[MAX_PROCESSES-1];  // Not used directly; list starts at end of ModuleRef
            };
        } temp;
        PCEL_MODULE_LOAD       pclBase  = &temp.ModuleBase;
        PCEL_EXTRA_MODULE_INFO pclExtra = &temp.ModuleExtra;
        PCEL_MODULE_REFERENCES pclRef   = &temp.ModuleRef;
        WORD wLen;
        WORD wProc;
        
        // Log base module info
        wLen = 0;
        pclBase->hProcess = hProcess;
        pclBase->hModule  = (HANDLE) pMod;
        pclBase->dwBase   = ZeroPtr(pMod->BasePtr);
        if (pMod->lpszModName) {
            wLen = strlenW(pMod->lpszModName) + 1;
            if (wLen <= MAX_PATH) {
                kstrcpyW(pclBase->szName, pMod->lpszModName);
            } else {
                wLen = 0;
            }
        }
        CELOGDATANK(TRUE, CELID_MODULE_LOAD, (PVOID) pclBase,
                    (WORD) (sizeof(CEL_MODULE_LOAD) + (wLen * sizeof(WCHAR))), 0,
                    CELZONE_LOADER | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER);
        
        // Log extra module info
        pclExtra->hModule = (HANDLE) pMod;
        pclExtra->dwVMLen = pMod->e32.e32_vsize;
        pclExtra->dwModuleFlags = CELOG_ModuleFlags((DWORD)pMod->BasePtr);
        wLen = CELOG_FileInfoFromOE(&pMod->oe, &pclExtra->dwOID,
                                    pclExtra->szFullPath);

        CELOGDATANK(TRUE, CELID_EXTRA_MODULE_INFO, (PVOID) pclExtra,
                    (WORD) (sizeof(CEL_EXTRA_MODULE_INFO) + (wLen * sizeof(WCHAR))), 0,
                    CELZONE_LOADER | CELZONE_PROFILER);
        
        // Log per-process refcounts for the module
        if ((DWORD)hProcess == (DWORD)-1) {
            wLen = 0;
            pclRef->hModule = (HANDLE) pMod;
            for (wProc = 0; wProc < MAX_PROCESSES; wProc++) {
                if (pMod->refcnt[wProc] > 0) {
                    pclRef->ref[wLen].hProcess = ProcArray[wProc].hProc;
                    pclRef->ref[wLen].dwRefCount = pMod->refcnt[wProc];
                    wLen++;
                }
            }
            
            if (wLen > 0) {
                CELOGDATANK(TRUE, CELID_MODULE_REFERENCES, (PVOID) &pclRef,
                            (WORD) (sizeof(CEL_MODULE_REFERENCES) + ((wLen - 1) * sizeof(CEL_PROCESS_REFCOUNT))), 0,
                            CELZONE_LOADER | CELZONE_PROFILER);
            }
        }
    }
}

//----------------------------------------------------------
_inline void CELOG_ModuleFree(HANDLE hProcess, HANDLE hModule)
{
    // Logged for both general and profiling mode
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE,
                       CELZONE_LOADER | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {

        CEL_MODULE_FREE cl;
        
        cl.hProcess = hProcess;
        cl.hModule = hModule;
    
        CELOGDATANK(TRUE, CELID_MODULE_FREE, &cl, sizeof(CEL_MODULE_FREE), 0, CELZONE_LOADER | CELZONE_MEMTRACKING | CELZONE_PROFILER);
    }
}

//----------------------------------------------------------
_inline void CELOG_LaunchingFilesys(void)
{
    if (IsCeLogZoneEnabled(CELZONE_BOOT_TIME)) {
        CEL_BOOT_TIME cl;
        cl.dwAction = BOOT_TIME_LAUNCHING_FS;
        CELOGDATANK(TRUE, CELID_BOOT_TIME, &cl, sizeof(CEL_BOOT_TIME), 0, CELZONE_BOOT_TIME);
    }
}


//
// Special events that are only logged in profiling builds, to avoid the
// performance hit they'd cause.
//

#ifdef NKPROF

//----------------------------------------------------------
_inline void CELOG_SystemTLB(DWORD dwCount)
{
    if (IsCeLogZoneEnabled(CELZONE_TLB)) {
        CEL_SYSTEM_TLB cl;
        cl.dwCount = dwCount;
        CELOGDATANK(TRUE, CELID_SYSTEM_TLB, &cl, sizeof(CEL_SYSTEM_TLB), 0, CELZONE_TLB);
    }
}

//----------------------------------------------------------
#define CELOG_KCallEnter(nID)                                                  \
    g_pfnCeLogData(TRUE, CELID_KCALL_ENTER, &nID, sizeof(int), 0, CELZONE_KCALL, 0, FALSE)

//----------------------------------------------------------
#define CELOG_KCallLeave(nID)                                                  \
    g_pfnCeLogData(TRUE, CELID_KCALL_LEAVE, &nID, sizeof(int), 0, CELZONE_KCALL, 0, FALSE)

#else // NKPROF

#define CELOG_SystemTLB(dwCount)                                       ((void)0)
#define CELOG_KCallEnter(nID)                                          ((void)0)
#define CELOG_KCallLeave(nID)                                          ((void)0)

#endif // NKPROF


#undef CELOGDATANK

#endif // IN_KERNEL

#endif // __CELOGNK_H_
