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
extern PFNVOID g_pfnCeLogData;
extern PFNVOID g_pfnCeLogInterrupt;
extern PFNVOID g_pfnCeLogSetZones;

// Instead of using the CELOGDATA macro in these inline functions, call the
// function pointer directly.
#define CELOGDATANK(T,I,D,L,Y,Z)          g_pfnCeLogData(T,I,D,L,Y,Z,0,FALSE) 

#undef IsCeLogLoaded
#undef IsCeLogEnabled
#undef IsCeLogProfilingEnabled
#undef IsCeLogKernelEnabled
#define IsCeLogLoaded()           ((KInfoTable[KINX_CELOGSTATUS] & (CELOGSTATUS_ENABLED_GENERAL | CELOGSTATUS_ENABLED_PROFILE)) != 0)
#define IsCeLogEnabled()          ((KInfoTable[KINX_CELOGSTATUS] & CELOGSTATUS_ENABLED_GENERAL) != 0)
#define IsCeLogProfilingEnabled() IsCeLogLoaded()
#define IsCeLogKernelEnabled()    IsCeLogLoaded()
// Turn on all logging
#define CeLogEnableGeneral()      (KInfoTable[KINX_CELOGSTATUS] = CELOGSTATUS_ENABLED_GENERAL)
// Turn on only profiling log events
#define CeLogEnableProfiling()    (KInfoTable[KINX_CELOGSTATUS] = CELOGSTATUS_ENABLED_PROFILE)


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

#define CELOG_IMPORT_VERSION 2

// Provided by the kernel for celog to use
typedef struct _CeLogImportTable {
    DWORD    dwVersion;               // Version of this structure, set to CELOG_IMPORT_VERSION
    const FARPROC *pWin32Methods;     // Kernel Win32 (non-handle-based) methods
    const FARPROC *pEventMethods;     // Handle-based event methods
    const FARPROC *pMapMethods;       // Handle-based memory-mapped file methods
    FARPROC  INTERRUPTS_ENABLE;       // Pointer to INTERRUPTS_ENABLE function
    FARPROC  Phys2Virt;               // Pointer to Phys2Virt function
    FARPROC  InSysCall;               // Pointer to InSysCall function
    FARPROC  KCall;                   // Pointer to KCall function
    FARPROC  NKDbgPrintfW;            // Pointer to NKDbgPrintfW function
    struct KDataStruct *pKData;       // Pointer to KData struct
    DWORD   *pdwCeLogTLBMiss;         // Pointer to TLB miss count
    DWORD    dwCeLogLargeBuf;         // OEM-changeable large buffer size
    DWORD    dwCeLogSmallBuf;         // OEM-changeable small buffer size
    DWORD    dwDefaultThreadQuantum;  // The scheduler's default thread quantum
} CeLogImportTable;

#define CELOG_EXPORT_VERSION 2

void CELOG_ThreadSuspend(HANDLE hThread);


// The kernel wrappers

//----------------------------------------------------------
_inline void CELOG_ThreadSwitch(PTHREAD pThread) {
    CEL_THREAD_SWITCH cl;

    DEBUGCHK(IsCeLogKernelEnabled());
    if (pThread) {
        cl.hThread = pThread->hTh;
    } else {
        cl.hThread = 0;
    }

    CELOGDATANK(TRUE, CELID_THREAD_SWITCH, &cl, sizeof(cl), 0, CELZONE_RESCHEDULE | CELZONE_PROFILER);
}

//----------------------------------------------------------
_inline void CELOG_ThreadCreate(PTHREAD pThread, HANDLE hProcess, 
                                LPCWSTR lpProcName, BOOL fSuspended) {

    DEBUGCHK(IsCeLogKernelEnabled());
    if (pThread) {
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
            kstrcpyW(pcl->szName, lpProcName); 
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

        if (fSuspended && IsCeLogEnabled()) {
            CELOG_ThreadSuspend(pcl->hThread);
        }
    }

}

//----------------------------------------------------------
_inline void CELOG_ThreadCloseHandle(HANDLE hThread) {
    
    CEL_THREAD_CLOSE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hThread  = hThread;

    CELOGDATANK(TRUE, CELID_THREAD_CLOSE, &cl, sizeof(CEL_THREAD_CLOSE), 0, CELZONE_THREAD);
}

//----------------------------------------------------------
_inline void CELOG_ThreadTerminate(HANDLE hThread) {
    
    CEL_THREAD_TERMINATE cl;
    
    DEBUGCHK(IsCeLogKernelEnabled());
    cl.hThread  = hThread;

    CELOGDATANK(TRUE, CELID_THREAD_TERMINATE, &cl, sizeof(CEL_THREAD_TERMINATE), 0, CELZONE_THREAD | CELZONE_PROFILER);
}

//----------------------------------------------------------
_inline void CELOG_ThreadDelete(HANDLE hThread) {
    
    CEL_THREAD_DELETE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hThread  = hThread;

    CELOGDATANK(TRUE, CELID_THREAD_DELETE, &cl, sizeof(CEL_THREAD_DELETE), 0, CELZONE_THREAD);
}

//----------------------------------------------------------
_inline void CELOG_ProcessCreate(HANDLE hProcess, LPCWSTR lpProcName, DWORD dwVmBase) {
    BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_PROCESS_CREATE)];
    PCEL_PROCESS_CREATE pcl = (PCEL_PROCESS_CREATE) pTmp;
    WORD wLen = 0;
    
    DEBUGCHK(IsCeLogKernelEnabled());
    
    // Special case: fixup nk.exe to tell where the code lives, not the VM
    if (dwVmBase == (SECURE_SECTION << VA_SECTION)) {
        dwVmBase = pTOC->physfirst;
    }
    
    pcl->hProcess = hProcess;
    pcl->dwVMBase = dwVmBase;
    
    if (lpProcName) {
        wLen = strlenW(lpProcName) + 1;
        kstrcpyW(pcl->szName, lpProcName); 
    }
    
    CELOGDATANK(TRUE, CELID_PROCESS_CREATE, (PVOID) pcl,
                (WORD) (sizeof(CEL_PROCESS_CREATE) + (wLen * sizeof(WCHAR))), 0,
                CELZONE_PROCESS | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER);
}

//----------------------------------------------------------
_inline void CELOG_ProcessCloseHandle(HANDLE hProcess) {
    
    CEL_PROCESS_CLOSE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hProcess = hProcess;
    
    CELOGDATANK(TRUE, CELID_PROCESS_CLOSE, &cl, sizeof(CEL_PROCESS_CLOSE), 0, CELZONE_PROCESS);
}

//----------------------------------------------------------
_inline void CELOG_ProcessTerminate(HANDLE hProcess) {
    
    CEL_PROCESS_TERMINATE cl;
    
    DEBUGCHK(IsCeLogKernelEnabled());
    cl.hProcess = hProcess;
    
    CELOGDATANK(TRUE, CELID_PROCESS_TERMINATE, &cl, sizeof(CEL_PROCESS_TERMINATE), 0, CELZONE_PROCESS | CELZONE_MEMTRACKING | CELZONE_PROFILER);
}

//----------------------------------------------------------
_inline void CELOG_ProcessDelete(HANDLE hProcess) {
    
    CEL_PROCESS_DELETE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hProcess = hProcess;
    
    CELOGDATANK(TRUE, CELID_PROCESS_DELETE, &cl, sizeof(CEL_PROCESS_DELETE), 0, CELZONE_PROCESS);
}

//----------------------------------------------------------
_inline void CELOG_Sleep(DWORD dwTimeout) {
    CEL_SLEEP cl;

    DEBUGCHK(IsCeLogEnabled());
    cl.dwTimeout = dwTimeout;

    CELOGDATANK(TRUE, CELID_SLEEP, &cl, sizeof(cl), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_EventCreate(HANDLE hEvent, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName, BOOL fCreate) {
    
    BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_EVENT_CREATE)];
    WORD wLen = 0;
    PCEL_EVENT_CREATE pcl = (PCEL_EVENT_CREATE) pTmp;
    
    DEBUGCHK(IsCeLogEnabled());
    pcl->hEvent = hEvent;
    pcl->fManual = fManReset;
    pcl->fInitialState = fInitState;
    pcl->fCreate = fCreate;

    if (lpEventName) {
        wLen = strlenW(lpEventName) + 1;
        kstrcpyW(pcl->szName, lpEventName); 
    }

    CELOGDATANK(TRUE, CELID_EVENT_CREATE, (PVOID) pcl, (WORD) (sizeof(CEL_EVENT_CREATE) + (wLen * sizeof(WCHAR))), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
// Event Set, Reset, Pulse
_inline void CELOG_Event(HANDLE hEvent, DWORD type) {
    
   DEBUGCHK(IsCeLogEnabled());
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
   
   default:;
   }
}

//----------------------------------------------------------
_inline void CELOG_EventCloseHandle(HANDLE hEvent) {
    
    CEL_EVENT_CLOSE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hEvent = hEvent;

    CELOGDATANK(TRUE, CELID_EVENT_CLOSE, &cl, sizeof(CEL_EVENT_CLOSE), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_EventDelete(HANDLE hEvent) {
    
    CEL_EVENT_DELETE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hEvent = hEvent;

    CELOGDATANK(TRUE, CELID_EVENT_DELETE, &cl, sizeof(CEL_EVENT_DELETE), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_WaitForMultipleObjects(
    DWORD dwNumObjects,
    CONST HANDLE* lpHandles,
    BOOL fWaitAll,
    DWORD dwTimeout
    )
{
    BYTE pTmp[(MAXIMUM_WAIT_OBJECTS * sizeof(HANDLE)) + sizeof(CEL_WAIT_MULTI)];
    PCEL_WAIT_MULTI pcl = (PCEL_WAIT_MULTI) pTmp;
    
    DEBUGCHK(IsCeLogEnabled());
    
    // Impose a limit on the number of handles logged
    if (dwNumObjects > MAXIMUM_WAIT_OBJECTS) {
        dwNumObjects = MAXIMUM_WAIT_OBJECTS;
    }
    
    pcl->fWaitAll = fWaitAll;
    pcl->dwTimeout = dwTimeout;
    memcpy(pcl->hHandles, lpHandles, dwNumObjects * sizeof(HANDLE));

    CELOGDATANK(TRUE, CELID_WAIT_MULTI, (LPVOID) pcl, (WORD) (sizeof(CEL_WAIT_MULTI) + (dwNumObjects * sizeof(HANDLE))), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_MutexCreate(HANDLE hMutex, LPMUTEX lpMutex) {
    
    BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_MUTEX_CREATE)];
    PCEL_MUTEX_CREATE pcl = (PCEL_MUTEX_CREATE) pTmp;
    WORD wLen = 0;
    
    DEBUGCHK(IsCeLogEnabled());
    pcl->hMutex = hMutex;
    if (lpMutex && lpMutex->name && lpMutex->name->name) {
        wLen = strlenW(lpMutex->name->name) + 1;
        kstrcpyW(pcl->szName, lpMutex->name->name);
    }

    CELOGDATANK(TRUE, CELID_MUTEX_CREATE, (PVOID) pcl, (WORD) (sizeof(CEL_MUTEX_CREATE) + (wLen * sizeof(WCHAR))), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_MutexRelease(HANDLE hMutex) {
    
    CEL_MUTEX_RELEASE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hMutex = hMutex;

    CELOGDATANK(TRUE, CELID_MUTEX_RELEASE, &cl, sizeof(CEL_MUTEX_RELEASE), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_MutexCloseHandle(HANDLE hMutex) {
    
    CEL_MUTEX_CLOSE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hMutex = hMutex;

    CELOGDATANK(TRUE, CELID_MUTEX_CLOSE, &cl, sizeof(CEL_MUTEX_CLOSE), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_MutexDelete(HANDLE hMutex) {
    
    CEL_MUTEX_DELETE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hMutex = hMutex;

    CELOGDATANK(TRUE, CELID_MUTEX_DELETE, &cl, sizeof(CEL_MUTEX_DELETE), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_SemaphoreCreate(HANDLE hSem, LONG lInitCount, LONG lMaxCount, LPCWSTR szName) {
    
    BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_SEM_CREATE)];
    PCEL_SEM_CREATE pcl = (PCEL_SEM_CREATE) pTmp;
    WORD wLen = 0;
    
    DEBUGCHK(IsCeLogEnabled());
    pcl->hSem = hSem;
    pcl->dwInitCount = (DWORD) lInitCount;
    pcl->dwMaxCount = (DWORD) lMaxCount;
    
    if (szName) {
        wLen = strlenW(szName) + 1;
        kstrcpyW(pcl->szName, szName);
    }

    CELOGDATANK(TRUE, CELID_SEM_CREATE, (PVOID) pcl, (WORD) (sizeof(CEL_SEM_CREATE) + (wLen * sizeof(WCHAR))), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_SemaphoreRelease(HANDLE hSem, LONG lReleaseCount, LONG lPreviousCount) {
    
    CEL_SEM_RELEASE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hSem = hSem;
    cl.dwReleaseCount = (DWORD) lReleaseCount;
    cl.dwPreviousCount = (DWORD) lPreviousCount;

    CELOGDATANK(TRUE, CELID_SEM_RELEASE, &cl, sizeof(CEL_SEM_RELEASE), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_SemaphoreCloseHandle(HANDLE hSem) {
    
    CEL_SEM_CLOSE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hSem = hSem;

    CELOGDATANK(TRUE, CELID_SEM_CLOSE, &cl, sizeof(CEL_SEM_CLOSE), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
_inline void CELOG_SemaphoreDelete(HANDLE hSem) {
    
    CEL_SEM_DELETE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hSem = hSem;

    CELOGDATANK(TRUE, CELID_SEM_DELETE, &cl, sizeof(CEL_SEM_DELETE), 0, CELZONE_SYNCH);
}

//----------------------------------------------------------
// Only logged if we block when entering
_inline void CELOG_CSEnter(HANDLE hCS, HANDLE hOwnerThread) {
    
    CEL_CRITSEC_ENTER cl;
    
#ifdef MIPSIV
    

    if (IsValidKPtr(&cl)) {
        return;
    }
#endif // MIPSIV

    DEBUGCHK(IsCeLogEnabled());
    cl.hCS = hCS;
    cl.hOwnerThread = hOwnerThread;

    CELOGDATANK(TRUE, CELID_CS_ENTER, &cl, sizeof(CEL_CRITSEC_ENTER), 0, CELZONE_CRITSECT);
}

//----------------------------------------------------------
// Only logged if another thread was blocked waiting for the CS
_inline void CELOG_CSLeave(HANDLE hCS, HANDLE hNewOwner) {
    
    CEL_CRITSEC_LEAVE cl;
    
#ifdef MIPSIV
    

    if (IsValidKPtr(&cl)) {
        return;
    }
#endif // MIPSIV

    DEBUGCHK(IsCeLogEnabled());
    cl.hCS = hCS;
    cl.hOwnerThread = hNewOwner;

    CELOGDATANK(TRUE, CELID_CS_LEAVE, &cl, sizeof(CEL_CRITSEC_LEAVE), 0, CELZONE_CRITSECT);
}

//----------------------------------------------------------
_inline void CELOG_VirtualAlloc(DWORD dwResult, DWORD dwAddress, DWORD dwSize, DWORD dwType, DWORD dwProtect) {
    
    CEL_VIRTUAL_ALLOC cl;

    DEBUGCHK(IsCeLogEnabled());
    cl.dwResult = dwResult;
    cl.dwAddress = dwAddress;
    cl.dwSize = dwSize;
    cl.dwType = dwType;
    cl.dwProtect = dwProtect;
    
    CELOGDATANK(TRUE, CELID_VIRTUAL_ALLOC, &cl, sizeof(CEL_VIRTUAL_ALLOC), 0, CELZONE_VIRTMEM);
}

//----------------------------------------------------------
_inline void CELOG_VirtualCopy(DWORD dwDest, DWORD dwSource, DWORD dwSize, DWORD dwProtect) {
    
    CEL_VIRTUAL_COPY cl;

    DEBUGCHK(IsCeLogEnabled());
    cl.dwDest = dwDest;
    cl.dwSource = dwSource;
    cl.dwSize = dwSize;
    cl.dwProtect = dwProtect;
    
    CELOGDATANK(TRUE, CELID_VIRTUAL_COPY, &cl, sizeof(CEL_VIRTUAL_COPY), 0, CELZONE_VIRTMEM);
}

//----------------------------------------------------------
_inline void CELOG_VirtualFree(DWORD dwAddress, DWORD dwSize, DWORD dwType) {
    
    CEL_VIRTUAL_FREE cl;

    DEBUGCHK(IsCeLogEnabled());
    cl.dwAddress = dwAddress;
    cl.dwSize = dwSize;
    cl.dwType = dwType;
    
    CELOGDATANK(TRUE, CELID_VIRTUAL_FREE, &cl, sizeof(CEL_VIRTUAL_FREE), 0, CELZONE_VIRTMEM);
}

//----------------------------------------------------------
_inline void CELOG_SystemPage(DWORD dwAddress, BOOL fReadWrite) {
    
    CEL_SYSTEM_PAGE cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.dwAddress = dwAddress;
    cl.fReadWrite = fReadWrite;

    CELOGDATANK(TRUE, CELID_SYSTEM_PAGE, &cl, sizeof(CEL_SYSTEM_PAGE), 0, CELZONE_DEMANDPAGE);
}

//----------------------------------------------------------
_inline void CELOG_SystemInvert(HANDLE hThread, int nPriority) {
    
    CEL_SYSTEM_INVERT cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hThread = hThread;
    cl.nPriority = nPriority;

    CELOGDATANK(TRUE, CELID_SYSTEM_INVERT, &cl, sizeof(CEL_SYSTEM_INVERT), 0, CELZONE_PRIORITYINV);
}

//----------------------------------------------------------
_inline void CELOG_ThreadSetPriority(HANDLE hThread, int nPriority) {
    
    CEL_THREAD_PRIORITY cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hThread = hThread;
    cl.nPriority = nPriority;

    CELOGDATANK(TRUE, CELID_THREAD_PRIORITY, &cl, sizeof(CEL_THREAD_PRIORITY), 0, CELZONE_THREAD);
}

//----------------------------------------------------------
_inline void CELOG_ThreadSetQuantum(HANDLE hThread, DWORD dwQuantum) {
    
    CEL_THREAD_QUANTUM cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hThread = hThread;
    cl.dwQuantum = dwQuantum;

    CELOGDATANK(TRUE, CELID_THREAD_QUANTUM, &cl, sizeof(CEL_THREAD_QUANTUM), 0, CELZONE_THREAD);
}

//----------------------------------------------------------
_inline void CELOG_ThreadSuspend(HANDLE hThread) {
    
    CEL_THREAD_SUSPEND cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hThread = hThread;

    CELOGDATANK(TRUE, CELID_THREAD_SUSPEND, &cl, sizeof(CEL_THREAD_SUSPEND), 0, CELZONE_RESCHEDULE);
}

//----------------------------------------------------------
_inline void CELOG_ThreadResume(HANDLE hThread) {
    
    CEL_THREAD_RESUME cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.hThread = hThread;

    CELOGDATANK(TRUE, CELID_THREAD_RESUME, &cl, sizeof(CEL_THREAD_RESUME), 0, CELZONE_RESCHEDULE);
}

//----------------------------------------------------------
_inline void CELOG_ModuleLoad(HANDLE hProcess, HANDLE hModule, LPCWSTR szName, DWORD dwBaseOffset) {
    
    DEBUGCHK(IsCeLogKernelEnabled());
    if (hModule) {
       BYTE pTmp[(MAX_PATH*sizeof(WCHAR)) + sizeof(CEL_MODULE_LOAD)];
       PCEL_MODULE_LOAD pcl = (PCEL_MODULE_LOAD) pTmp;
       WORD wLen = 0;
      
       pcl->hProcess = hProcess;
       pcl->hModule  = hModule;
       pcl->dwBase   = ZeroPtr(dwBaseOffset);
      
       if (szName) {
           wLen = strlenW(szName) + 1;
           kstrcpyW(pcl->szName, szName);
       }
   
       CELOGDATANK(TRUE, CELID_MODULE_LOAD, (PVOID) pcl,
                   (WORD) (sizeof(CEL_MODULE_LOAD) + (wLen * sizeof(WCHAR))), 0,
                   CELZONE_LOADER | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER);
    }
}

//----------------------------------------------------------
_inline void CELOG_ModuleFree(HANDLE hProcess, HANDLE hModule) {
    
    CEL_MODULE_FREE cl;
    
    DEBUGCHK(IsCeLogKernelEnabled());
    cl.hProcess = hProcess;
    cl.hModule = hModule;

    CELOGDATANK(TRUE, CELID_MODULE_FREE, &cl, sizeof(CEL_MODULE_FREE), 0, CELZONE_LOADER | CELZONE_MEMTRACKING | CELZONE_PROFILER);
}

//----------------------------------------------------------
_inline void CELOG_LaunchingFilesys(void) {
    
    CEL_BOOT_TIME cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.dwAction = BOOT_TIME_LAUNCHING_FS;
    
    CELOGDATANK(TRUE, CELID_BOOT_TIME, &cl, sizeof(CEL_BOOT_TIME), 0, CELZONE_BOOT_TIME);
}


//
// Special events that are only logged in profiling builds, to avoid the
// performance hit they'd cause.
//

#ifdef CELOG

//----------------------------------------------------------
_inline void CELOG_SystemTLB(DWORD dwCount) {
    
    CEL_SYSTEM_TLB cl;
    
    DEBUGCHK(IsCeLogEnabled());
    cl.dwCount = dwCount;

    CELOGDATANK(TRUE, CELID_SYSTEM_TLB, &cl, sizeof(CEL_SYSTEM_TLB), 0, CELZONE_TLB);
}

//----------------------------------------------------------
#define CELOG_KCallEnter(nID)                                                     \
    g_pfnCeLogData(TRUE, CELID_KCALL_ENTER, &nID, sizeof(int), 0, CELZONE_KCALL, 0, FALSE)

//----------------------------------------------------------
#define CELOG_KCallLeave(nID)                                                     \
    g_pfnCeLogData(TRUE, CELID_KCALL_LEAVE, &nID, sizeof(int), 0, CELZONE_KCALL, 0, FALSE)

#else // CELOG

#define CELOG_SystemTLB(dwCount)                                       ((void)0)
#define CELOG_KCallEnter(nID)                                          ((void)0)
#define CELOG_KCallLeave(nID)                                          ((void)0)

#endif // CELOG


#undef CELOGDATANK

#endif // IN_KERNEL

#endif // __CELOGNK_H_
