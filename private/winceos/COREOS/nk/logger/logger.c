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

Abstract:
 This file implements the NK kernel logging support.

Functions:


Notes:

--*/
#define C_ONLY
#include "kernel.h"


// Pointers to the current DLL entry points.
// No DLLs loaded: These will be NULL.
// Only a single DLL loaded: These will be the entry points for that DLL.
// More than one DLL loaded: These will be the broadcast functions.
// These pointers are not strictly necessary since the list could be used to
// figure everything out, but they make the accesses faster.
void (*g_pfnCeLogData)();             // Receives CeLogData calls
PFNVOID g_pfnCeLogInterrupt;          // Receives CeLogInterrupt calls
PFNVOID g_pfnCeLogInterruptData;      // Receives CeLogInterruptData calls

// Cannot use a global pointer for CeLogSetZones because we always need to
// use the broadcast function, in order to recalculate zones
static DWORD g_dwCeLogTimerFrequency; // Most recent nonzero timer frequency

// On many SMP machines it's not safe to call QPC during an interrupt, so
// keep a global flag rather than checking on every interrupt
static BOOL  g_fQPCIsSafeDuringInterrupts;

// The overall zones are a union of all DLL zone settings
static DWORD g_dwZoneUser;
static DWORD g_dwZoneCE;
static DWORD g_dwZoneProcess;

// To get the scheduler's default thread quantum
extern POEMGLOBAL g_pOemGlobal;

// List of active DLLs
typedef struct CeLogDLLInfo_t {
    struct CeLogDLLInfo_t *pNext;     // The next DLL in the list
    // Pointers to the export functions and data
    PFNVOID pfnCeLogData;             // Receives CeLogData calls      (REQUIRED)
    PFNVOID pfnCeLogInterrupt;        // Receives CeLogInterrupt calls (OPTIONAL)
    PFNVOID pfnCeLogInterruptData;    // Receives CeLogInterruptData calls (OPTIONAL)
    PFNVOID pfnCeLogSetZones;         // Receives CeLogSetZones calls  (OPTIONAL)
    FARPROC pfnCeLogQueryZones;       // Receives CeLogGetZones calls, minus last param (OPTIONAL)
    DWORD   dwCeLogTimerFrequency;    // Less than or equal to QueryPerformanceFrequency()
    // Zone info for this DLL
    DWORD   dwZoneUser;
    DWORD   dwZoneCE;
    DWORD   dwZoneProcess;
    // NOTENOTE No room for more struct members without exceeding HEAP_SIZE2 on non-x86
} CeLogDLLInfo;
static CeLogDLLInfo *g_pCeLogList;    // The head of the list
static DWORD g_dwCeLogDLLCount;       // Number of DLLs in the list

// Using a type 2 heap object to track this info
ERRFALSE(sizeof(CeLogDLLInfo) <= HEAP_SIZE2);

typedef struct CeLogLockedEvent {
    HANDLE hEvent;
    PHDATA phdEvent;
    struct CeLogLockedEvent* pNext;
} CeLogLockedEvent;

CeLogLockedEvent* g_pCeLogLockedEventList;  // Protected by CeLog spinlock

ERRFALSE(sizeof(CeLogLockedEvent) <= HEAP_SIZE0);


// Used by celog.dll itself, and also by the logger to synchronize operations
// without generating its own calls to log CeLog events.
static SPINLOCK g_CeLogSpinLock;


// CeLog zones available in all builds
#define AVAILABLE_ZONES_ALL_BUILDS                                              \
    (CELZONE_RESCHEDULE | CELZONE_MIGRATE | CELZONE_DEMANDPAGE | CELZONE_THREAD \
     | CELZONE_PROCESS | CELZONE_PRIORITYINV | CELZONE_CRITSECT | CELZONE_SYNCH \
     | CELZONE_HEAP | CELZONE_VIRTMEM | CELZONE_GWES | CELZONE_LOADER | CELZONE_LOWMEM \
     | CELZONE_MEMTRACKING | CELZONE_BOOT_TIME | CELZONE_GDI | CELZONE_DEBUG    \
     | CELZONE_ALWAYSON | CELZONE_MISC | CELZONE_INTERRUPT | CELZONE_PROFILER)
// CeLog zones available only in profiling builds
#define AVAILABLE_ZONES_PROFILING \
    (CELZONE_TLB | CELZONE_KCALL)


// Define the zones available on this kernel
#define AVAILABLE_ZONES (AVAILABLE_ZONES_ALL_BUILDS | AVAILABLE_ZONES_PROFILING)


//------------------------------------------------------------------------------
//
//
//  CELOG DLL REGISTRATION
//
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static _inline BOOL EnterCeLogRegisterCS()
{
    // Cannot use true critical sections to protect registration because they
    // would generate calls into CeLog.
    AcquireSpinLock (&g_CeLogSpinLock);
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static _inline VOID LeaveCeLogRegisterCS()
{
    ReleaseSpinLock (&g_CeLogSpinLock);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Add a DLL to the list and recalculate globals
static BOOL
AddCeLogDLL(
    CeLogExportTable_V2 *pExports,
    DWORD dwVersion
    )
{
    CeLogDLLInfo *pDLL;

    // Check whether it is already registered.  We could use a refcount to
    // allow multiple loads for the same struct but as of now there's no need
    // to be that sophisticated.
    for (pDLL = g_pCeLogList; pDLL; pDLL = pDLL->pNext) {
        if ((pDLL->pfnCeLogData == pExports->pfnCeLogData)
            && (pDLL->pfnCeLogInterrupt == pExports->pfnCeLogInterrupt)
            && (pDLL->pfnCeLogSetZones == pExports->pfnCeLogSetZones)
            && (pDLL->pfnCeLogQueryZones == pExports->pfnCeLogQueryZones)
            && ((dwVersion < 3) || (pDLL->pfnCeLogInterruptData == ((CeLogExportTable_V3*)pExports)->pfnCeLogInterruptData))) {
            KSetLastError(pCurThread, ERROR_ALREADY_EXISTS);
            return FALSE;
        }
    }

    // CeLogData export is required; all others are optional
    if (pExports->pfnCeLogData == NULL) {
        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Alloc a new struct to hold the DLL info
    pDLL = AllocMem(HEAP_CELOGDLLINFO);
    if (!pDLL) {
        KSetLastError(pCurThread, ERROR_OUTOFMEMORY);
        return FALSE;
    }

    pDLL->pfnCeLogData          = pExports->pfnCeLogData;
    pDLL->pfnCeLogInterrupt     = pExports->pfnCeLogInterrupt;
    pDLL->pfnCeLogSetZones      = pExports->pfnCeLogSetZones;
    pDLL->pfnCeLogQueryZones    = pExports->pfnCeLogQueryZones;
    pDLL->dwCeLogTimerFrequency = pExports->dwCeLogTimerFrequency;
    if (dwVersion == 3) {
        pDLL->pfnCeLogInterruptData = ((CeLogExportTable_V3*)pExports)->pfnCeLogInterruptData;
    } else {
        pDLL->pfnCeLogInterruptData = NULL;
    }

    // Set global timer frequency if necessary
    if (pDLL->dwCeLogTimerFrequency != 0) {
        DEBUGMSG(g_dwCeLogTimerFrequency
                 && (g_dwCeLogTimerFrequency != pDLL->dwCeLogTimerFrequency),
                 (TEXT("CeLog: Timer frequency changing from 0x%08x to 0x%08x\r\n"),
                  g_dwCeLogTimerFrequency, pDLL->dwCeLogTimerFrequency));
        g_dwCeLogTimerFrequency = pDLL->dwCeLogTimerFrequency;
    }

    // Query the DLL zone settings
    if (pDLL->pfnCeLogQueryZones) {
        pDLL->pfnCeLogQueryZones(&(pDLL->dwZoneUser), &(pDLL->dwZoneCE), &(pDLL->dwZoneProcess));
    } else {
        pDLL->dwZoneUser    = 0;
        pDLL->dwZoneCE      = 0;
        pDLL->dwZoneProcess = 0;
    }

    // Add the DLL's zones to the global zone settings
    g_dwZoneUser    |= pDLL->dwZoneUser;
    g_dwZoneCE      |= (pDLL->dwZoneCE & AVAILABLE_ZONES);  // Reserved/unused zones cannot be turned on
    g_dwZoneProcess |= pDLL->dwZoneProcess;

    // Add to the head of the list
    pDLL->pNext = g_pCeLogList;
    g_pCeLogList = pDLL;
    g_dwCeLogDLLCount++;

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Remove a DLL from the list and recalculate globals
static BOOL
RemoveCeLogDLL(
    CeLogExportTable_V2 *pExports,
    DWORD dwVersion
    )
{
    CeLogDLLInfo *pDLL, *pPrevDLL;
    BOOL fFound = FALSE;

    // We will have to walk the whole list to recalculate the zones and timer freq
    g_dwZoneUser = 0;
    g_dwZoneCE = 0;
    g_dwZoneProcess = 0;
    if ((pExports->dwCeLogTimerFrequency != 0)
        && (g_dwCeLogTimerFrequency == pExports->dwCeLogTimerFrequency)) {
        g_dwCeLogTimerFrequency = 0;
    }

    pPrevDLL = NULL;
    pDLL = g_pCeLogList;
    while (pDLL) {

        if (!fFound
            && (pDLL->pfnCeLogData == pExports->pfnCeLogData)
            && (pDLL->pfnCeLogInterrupt == pExports->pfnCeLogInterrupt)
            && (pDLL->pfnCeLogSetZones == pExports->pfnCeLogSetZones)
            && (pDLL->pfnCeLogQueryZones == pExports->pfnCeLogQueryZones)
            && ((dwVersion < 3) || (pDLL->pfnCeLogInterruptData == ((CeLogExportTable_V3*)pExports)->pfnCeLogInterruptData))) {

            CeLogDLLInfo *pTemp = pDLL;  // temp holder

            // Found it
            fFound = TRUE;

            // Remove from the list
            if (pPrevDLL == NULL) {
                // Removing the head of the list
                g_pCeLogList = pDLL->pNext;
            } else {
                pPrevDLL->pNext = pDLL->pNext;
            }
            g_dwCeLogDLLCount--;

            // Continue iteration -- pPrevDLL does not change
            pDLL = pDLL->pNext;

            // Free the DLL info
            if (IsValidKPtr(pTemp)) {
                FreeMem(pTemp, HEAP_CELOGDLLINFO);
            }

        } else {
            // Add the DLL's zones to the global zone settings
            g_dwZoneUser    |= pDLL->dwZoneUser;
            g_dwZoneCE      |= pDLL->dwZoneCE;
            g_dwZoneProcess |= pDLL->dwZoneProcess;

            // Use the first nonzero timer frequency in the list
            if ((g_dwCeLogTimerFrequency == 0) && (pDLL->dwCeLogTimerFrequency != 0)) {
                DEBUGMSG(pExports->dwCeLogTimerFrequency
                         && (pExports->dwCeLogTimerFrequency != pDLL->dwCeLogTimerFrequency),
                         (TEXT("CeLog: Timer frequency changing from 0x%08x to 0x%08x\r\n"),
                          pExports->dwCeLogTimerFrequency, pDLL->dwCeLogTimerFrequency));
                g_dwCeLogTimerFrequency = pDLL->dwCeLogTimerFrequency;
            }

            pPrevDLL = pDLL;
            pDLL = pDLL->pNext;
        }
    }

    if (!fFound) {
        KSetLastError(pCurThread, ERROR_MOD_NOT_FOUND);
        return FALSE;
    }

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
BroadcastCeLogData(
    BOOL  fTimeStamp,
    WORD  wID,
    VOID  *pData,
    WORD  wLen,
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    WORD  wFlag,
    BOOL  fFlagged
    )
{
    CeLogDLLInfo *pDLL;

    for (pDLL = g_pCeLogList; pDLL; pDLL = pDLL->pNext) {
        pDLL->pfnCeLogData(fTimeStamp, wID, pData, wLen, dwZoneUser, dwZoneCE,
                           wFlag, fFlagged);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
BroadcastCeLogInterrupt(
    DWORD dwLogValue
    )
{
    CeLogDLLInfo *pDLL;

    for (pDLL = g_pCeLogList; pDLL; pDLL = pDLL->pNext) {
        if (pDLL->pfnCeLogInterrupt) {
            pDLL->pfnCeLogInterrupt(dwLogValue);
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
BroadcastCeLogInterruptData(
    BOOL  fTimeStamp,
    WORD  wID,
    VOID  *pData,
    WORD  wLen
    )
{
    CeLogDLLInfo *pDLL;

    for (pDLL = g_pCeLogList; pDLL; pDLL = pDLL->pNext) {
        if (pDLL->pfnCeLogInterruptData) {
            pDLL->pfnCeLogInterruptData(fTimeStamp, wID, pData, wLen);
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
BroadcastCeLogSetZones(
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    DWORD dwZoneProcess
    )
{
    CeLogDLLInfo *pDLL;

    // Global zone settings will be recalculated
    g_dwZoneUser = 0;
    g_dwZoneCE = 0;
    g_dwZoneProcess = 0;

    for (pDLL = g_pCeLogList; pDLL; pDLL = pDLL->pNext) {
        if (pDLL->pfnCeLogSetZones) {
            pDLL->pfnCeLogSetZones(dwZoneUser, dwZoneCE, dwZoneProcess);
        }

        // Query the new DLL zone settings
        if (pDLL->pfnCeLogQueryZones) {
            pDLL->pfnCeLogQueryZones(&(pDLL->dwZoneUser), &(pDLL->dwZoneCE), &(pDLL->dwZoneProcess));
        }

        // Add the DLL's zones to the global zone settings
        g_dwZoneUser    |= pDLL->dwZoneUser;
        g_dwZoneCE      |= (pDLL->dwZoneCE & AVAILABLE_ZONES);  // Reserved/unused zones cannot be turned on
        g_dwZoneProcess |= pDLL->dwZoneProcess;
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// WRAPPER FUNCTIONS

// These calls are functions on some processors and macros on others.

static BOOL CELOG_WRAP_InSysCall () {
    return InSysCall ();
}

// These functions have to take special steps to support usage by CeLog

static HANDLE CELOG_WRAP_CreateEvent (LPSECURITY_ATTRIBUTES lpsa, BOOL fManReset, BOOL fInitState, LPCWSTR lpEventName)
{
    HANDLE hEvent = NKCreateEvent (lpsa, fManReset, fInitState, lpEventName);

    // It's not safe for EventModify to lock & unlock the event handle, because CeLog
    // calls EventModify during USERBLOCK (when a thread sleeps or waits) and while
    // holding lower level kernel critical sections like rtccs and PhysCS.
    // So lock the event handle when it's created, pass around the handle data for
    // direct use, and unlock it when it's destroyed.
    if (hEvent) {
        CeLogLockedEvent* pLockedEvent = AllocMem (HEAP_CELOGLOCKEDEVENT);
        if (pLockedEvent) {
            pLockedEvent->hEvent = hEvent;
            pLockedEvent->phdEvent = LockHandleData (hEvent, g_pprcNK);

            AcquireSpinLock (&g_CeLogSpinLock);
            pLockedEvent->pNext = g_pCeLogLockedEventList;
            g_pCeLogLockedEventList = pLockedEvent;
            ReleaseSpinLock (&g_CeLogSpinLock);

            return (HANDLE) pLockedEvent->phdEvent;
        }
        HNDLCloseHandle(g_pprcNK, hEvent);
    }

    return NULL;
}

static BOOL CELOG_WRAP_EventModify (HANDLE hEvent, DWORD type)
{
    if (!InSysCall()) {
        // The "hEvent" is actually an already-locked PHDATA
        PEVENT lpe = GetEventPtr ((PHDATA) hEvent);
        if (lpe) {
            return EVNTModify (lpe, type);
        }
    }
    return FALSE;
}

// Used to close both map handle and event handle
static BOOL CELOG_WRAP_CloseHandle (HANDLE hObject)
{
    // If this is an event handle, it'll be in the locked event list.
    // If it's not there, then it's a map handle.
    CeLogLockedEvent** ppLockedEvent;
    CeLogLockedEvent* pLockedEvent;
    
    // Find and remove the handle if it's on the event list
    AcquireSpinLock (&g_CeLogSpinLock);
    ppLockedEvent = &g_pCeLogLockedEventList;
    pLockedEvent = g_pCeLogLockedEventList;
    while (pLockedEvent) {
        if (pLockedEvent->phdEvent == (PHDATA) hObject) {
            *ppLockedEvent = pLockedEvent->pNext;
            pLockedEvent->pNext = NULL;
            break;
        }
        ppLockedEvent = &pLockedEvent->pNext;
        pLockedEvent = pLockedEvent->pNext;
    }
    ReleaseSpinLock (&g_CeLogSpinLock);
    
    // Unlock the handle
    if (pLockedEvent) {
        hObject = pLockedEvent->hEvent;
        UnlockHandleData (pLockedEvent->phdEvent);
        FreeMem (pLockedEvent, HEAP_CELOGLOCKEDEVENT);
    }

    return HNDLCloseHandle(g_pprcNK, hObject);
}

static BOOL CELOG_WRAP_QueryPerformanceCounterEx(LARGE_INTEGER *lpPerformanceCount, BOOL fDuringInterrupt)
{
    // Don't call QPC during an interrupt if it's not safe, or while holding
    // spinlocks other than the scheduler spinlock
    if ((g_fQPCIsSafeDuringInterrupts || !fDuringInterrupt)
        && (!GetPCB()->ownspinlock || OwnSchedulerLock())) {
        return NKQueryPerformanceCounter (lpPerformanceCount);
    }
    return FALSE;
}

// This routine exists for backward-compatibility, but likely will fail on SMP
static BOOL CELOG_WRAP_QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount)
{
    // Pass DuringInterrupt=TRUE to avoid crashes/hangs on SMP
    return CELOG_WRAP_QueryPerformanceCounterEx(lpPerformanceCount, TRUE);
}

// These functions had a signature change or no longer exist in the kernel.

static HANDLE CELOG_WRAP_CreateFileMapping(HANDLE hFile, LPSECURITY_ATTRIBUTES lpsa, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCTSTR lpName) {
    return NKCreateFileMapping(g_pprcNK, hFile, lpsa, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
}

static LPVOID CELOG_WRAP_MapViewOfFile(HANDLE hMap, DWORD dwAccess, DWORD dwOfstHigh, DWORD dwOfstLow, DWORD cbSize) {
    return MAPMapView(g_pprcNK, hMap, dwAccess, dwOfstHigh, dwOfstLow, cbSize);
}

static BOOL CELOG_WRAP_UnmapViewOfFile(LPCVOID pAddr) {
    return MAPUnmapView(g_pprcNK, (LPVOID) pAddr);
}

static BOOL CELOG_WRAP_FlushViewOfFile(LPVOID pAddr, DWORD cbSize) {
    return MAPFlushView(g_pprcNK, pAddr, cbSize);
}

static BOOL CELOG_WRAP_LockPages(LPVOID lpvAddress, DWORD cbSize, PDWORD pPFNs, int fOptions) {
    return VMLockPages(g_pprcNK, lpvAddress, cbSize, pPFNs, fOptions);
}

static BOOL CELOG_WRAP_UnlockPages(LPVOID lpvAddress, DWORD cbSize) {
    return VMUnlockPages(g_pprcNK, lpvAddress, cbSize);
}

static LPVOID CELOG_WRAP_VirtualAlloc(LPVOID lpAddress, DWORD dwSize, DWORD flAllocationType, DWORD flProtect) {
    return PROCVMAlloc(g_pprcNK, lpAddress, dwSize, flAllocationType, flProtect, NULL);
}

static BOOL CELOG_WRAP_VirtualFree(LPVOID lpAddress, DWORD dwSize, DWORD dwFreeType) {
    return PROCVMFree(g_pprcNK, lpAddress, dwSize, dwFreeType, 0);
}

// Src and Dest process are kernel
static LPVOID CELOG_WRAP_VirtualAllocCopyInKernel (LPVOID pAddr, DWORD cbSize, DWORD dwProtect) {
    return PROCVMAllocCopy(g_pprcNK, 0, pAddr, cbSize, dwProtect);
}

static LPVOID CELOG_WRAP_MapPtrToProcess(LPVOID lpv, HANDLE hProc) {
    UNREFERENCED_PARAMETER(hProc);
    // Not much we can do here
    return lpv;
}

// Legacy function took hThread; now THRDGetCallStack takes a pThread.  Legacy
// code will now be passing a thread ID.
static ULONG CELOG_WRAP_GetThreadCallStackFromId(HANDLE hThread, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip)
{
    PHDATA  phd;
    PTHREAD pth;
    ULONG   ulResult = 0;

    phd = LockHandleData (hThread, g_pprcNK);
    if (phd) {
        pth = GetThreadPtr (phd);
        if (pth) {
            ulResult = THRDGetCallStack (pth, dwMaxFrames, lpFrames, dwFlags, dwSkip);
        }
        UnlockHandleData (phd);
    }
    return ulResult;
}

static void InitCeLogSpinLock (void)
{
    static BOOL s_CeLogSpinLockInitialized;
    if (!s_CeLogSpinLockInitialized) {
        InitializeSpinLock (&g_CeLogSpinLock);
        s_CeLogSpinLockInitialized = TRUE;
    }
}

static void CELOG_WRAP_AcquireSpinLock (void)
{
    AcquireSpinLock (&g_CeLogSpinLock);
}

static void CELOG_WRAP_ReleaseSpinLock (void)
{
    ReleaseSpinLock (&g_CeLogSpinLock);
}

static DWORD CELOG_WRAP_GetCurrentCPU (void)
{
    return PcbGetCurCpu ();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL KernelLibIoControl_CeLog(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    UNREFERENCED_PARAMETER (lpBytesReturned);
    switch (dwIoControlCode) {

        case IOCTL_CELOG_IMPORT: {
            //
            // A CeLog DLL is requesting information about the kernel
            //

            CeLogImportTable *pImports = (CeLogImportTable*)lpInBuf;

            extern const PFNVOID Win32ExtMethods[];
            extern const PFNVOID EvntMthds[];
            extern const PFNVOID MapMthds[];

            // PUBLIC IMPORT TABLE VERSIONS:
            // 5 = Current
            // 4 = CE 5.0 to 6.x
            // 3 = Intermediate builds, post-4.2
            // 2 = CE 4.1 / 4.2
            // 1 = CE 4.0
            DEBUGMSG(0, (TEXT("CeLog DLL import version: %u\r\n"), pImports->dwVersion));
            if (((pImports->dwVersion == 4) && (nInBufSize >= sizeof(CeLogImportTable_V4)))
                || ((pImports->dwVersion == 5) && (nInBufSize >= sizeof(CeLogImportTable_V5)))) {

                InitCeLogSpinLock ();

                pImports->pCreateEventW              = CELOG_WRAP_CreateEvent;
                pImports->pEventModify               = CELOG_WRAP_EventModify;
                pImports->pCreateFileMappingW        = CELOG_WRAP_CreateFileMapping;
                pImports->pMapViewOfFile             = (MapMthds[ID_FSMAP_MAPVIEWOFFILE] ? CELOG_WRAP_MapViewOfFile : NULL);  // NULL if nomapfile
                pImports->pUnmapViewOfFile           = CELOG_WRAP_UnmapViewOfFile;
                pImports->pCloseHandle               = CELOG_WRAP_CloseHandle;
                pImports->pLockPages                 = CELOG_WRAP_LockPages;
                pImports->pUnlockPages               = CELOG_WRAP_UnlockPages;
                pImports->pVirtualAlloc              = CELOG_WRAP_VirtualAlloc;
                pImports->pVirtualFree               = CELOG_WRAP_VirtualFree;
                pImports->pMapPtrToProcess           = CELOG_WRAP_MapPtrToProcess;
                pImports->pQueryPerformanceCounter   = CELOG_WRAP_QueryPerformanceCounter;
                pImports->pQueryPerformanceFrequency = NKQueryPerformanceFrequency;
                pImports->pNKDbgPrintfW              = NKDbgPrintfW;
                pImports->pCeLogReSync               = NKCeLogReSync;
                pImports->pGetLastError              = NKGetLastError;
                pImports->pSetLastError              = NKSetLastError;
                pImports->pGetThreadCallStack        = CELOG_WRAP_GetThreadCallStackFromId;
                pImports->pInSysCall                 = CELOG_WRAP_InSysCall;
                pImports->pdwCeLogTLBMiss            = &((PPCB) g_pKData)->dwTlbMissCnt;
                pImports->dwCeLogLargeBuf            = 0;
                pImports->dwCeLogSmallBuf            = 0;
                pImports->dwDefaultThreadQuantum     = g_pOemGlobal->dwDefaultThreadQuantum;

                if (pImports->dwVersion == 5) {
                    pImports->dwNumberOfCPUs             = g_pKData->nCpus;
                    pImports->pRegQueryValueExW          = NKRegQueryValueExW;
                    pImports->pCeLogData                 = NKCeLogData;
                    pImports->pAcquireCeLogSpinLock      = CELOG_WRAP_AcquireSpinLock;
                    pImports->pReleaseCeLogSpinLock      = CELOG_WRAP_ReleaseSpinLock;
                    pImports->pGetCurrentCPU             = CELOG_WRAP_GetCurrentCPU;
                    pImports->pVirtualAllocCopyInKernel  = CELOG_WRAP_VirtualAllocCopyInKernel;
                    pImports->pQueryPerformanceCounterEx = CELOG_WRAP_QueryPerformanceCounterEx;
                }

            } else {
                // Older versions are no longer supported.
                ERRORMSG(1, (TEXT("CeLog DLL version is not supported\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            break;
        }

        case IOCTL_CELOG_IMPORT_PRIVATE: {
            //
            // A CeLog DLL is requesting private information about the kernel
            //

            CeLogPrivateImports *pImports = (CeLogPrivateImports*)lpInBuf;

            // PRIVATE IMPORT TABLE VERSIONS:
            // 2 = CE 7.0 to current
            // 1 = CE 5.0 to 6.x (Did not exist in 4.2 and earlier)
            if ((pImports->dwVersion == 0)
                || (pImports->dwVersion > CELOG_PRIVATE_IMPORT_VERSION)
                || (nInBufSize != sizeof(CeLogPrivateImports))) {
                ERRORMSG(1, (TEXT("CeLog DLL version does not match kernel\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            
            if (pImports->dwVersion == 2) {
                // V2: Actually has no data, but return success
                memset(pImports, 0, sizeof(CeLogPrivateImports));
                pImports->dwVersion = 2;
            } else if (pImports->dwVersion == 1) {
                // V1: Support back-compat with old celog.dll versions
                pImports->pRegQueryValueExW    = NKRegQueryValueExW;
                pImports->pINTERRUPTS_ENABLE   = INTERRUPTS_ENABLE;
                pImports->pPfn2Virt_DO_NOT_USE = NULL;  // No longer supported - but 5.0/6.0 celog.dll didn't use it anyway
                pImports->pKCall               = KCall;
                pImports->pCeLogData           = NKCeLogData;
                pImports->pKData               = g_pKData;
            }

            break;
        }

        case IOCTL_CELOG_REGISTER: {
            //
            // A CeLog DLL is registering to receive data
            //

            CeLogExportTable_V2 *pExports = (CeLogExportTable_V2*) lpInBuf;
            DWORD dwVersion = pExports->dwVersion;

            // EXPORT TABLE VERSIONS:
            // 3 = CE 7.0 to current
            // 2 = CE 4.1 to 6.x
            // 1 = CE 4.0
            if (((dwVersion == 2) && (nInBufSize >= sizeof(CeLogExportTable_V2)))
                || ((dwVersion == 3) && (nInBufSize >= sizeof(CeLogExportTable_V3)))) {
                // OK
            } else {
                ERRORMSG(1, (TEXT("CeLog DLL version does not match kernel\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (!EnterCeLogRegisterCS()) {
                return FALSE;
            }

            // Figure out whether it's safe to call QueryPerformanceCounter 
            // during an interrupt on this platform.  It's only safe if:
            //   * there's only one CPU, or
            //   * the OEM hasn't implemented QueryPerformanceCounter in the OAL (QPC is just GetTickCount), or
            //   * the OEM explicitly says it's safe to call QPC during interrupts
            g_fQPCIsSafeDuringInterrupts =
                ((g_pKData->nCpus == 1)
                 || !g_pOemGlobal->pfnQueryPerfCounter
                 || (g_pOemGlobal->dwPlatformFlags & OAL_SMP_QPC_SAFE_DURING_INTERRUPTS));

            // Add the new DLL to the list
            if (!AddCeLogDLL(pExports, dwVersion)) {
                LeaveCeLogRegisterCS();
                return FALSE;
            }

            // For each entry point, if it is the first instance, set the pointer
            // directly to the DLL.  If it is any subsequent instance, set the
            // pointer to be the broadcast function.

            if (g_pfnCeLogData == NULL) {
                g_pfnCeLogData = pExports->pfnCeLogData;
            } else if (g_pfnCeLogData != BroadcastCeLogData) {
                g_pfnCeLogData = BroadcastCeLogData;
            }

            if (g_pfnCeLogInterrupt == NULL) {
                g_pfnCeLogInterrupt = pExports->pfnCeLogInterrupt;
            } else if ((g_pfnCeLogInterrupt != BroadcastCeLogInterrupt)
                       && (pExports->pfnCeLogInterrupt != NULL)) {
                g_pfnCeLogInterrupt = BroadcastCeLogInterrupt;
            }

            if (dwVersion == 3) {
                CeLogExportTable_V3 *pExportsV3 = (CeLogExportTable_V3*) pExports;

                if (g_pfnCeLogInterruptData == NULL) {
                    g_pfnCeLogInterruptData = pExportsV3->pfnCeLogInterruptData;
                } else if ((g_pfnCeLogInterruptData != BroadcastCeLogInterruptData)
                           && (pExportsV3->pfnCeLogInterruptData != NULL)) {
                    g_pfnCeLogInterruptData = BroadcastCeLogInterruptData;
                }
            }

            if (g_dwCeLogDLLCount == 1) {
                // First CeLog DLL to be loaded.  Enable logging.
                CeLogEnable(CELOGSTATUS_ENABLED_GENERAL, g_dwZoneCE);
            } else {
                // Set the new zones in UserKData
                CeLogEnableZones(g_dwZoneCE);
            }

            DEBUGMSG(1, (TEXT("New CeLog DLL is now live!  (%u total)\r\n"), g_dwCeLogDLLCount));

            LeaveCeLogRegisterCS();

            break;
        }

        case IOCTL_CELOG_DEREGISTER: {
            //
            // A CeLog DLL is deregistering so that it no longer receives data
            //

            CeLogExportTable_V2 *pExports = (CeLogExportTable_V2*) lpInBuf;
            DWORD dwVersion = pExports->dwVersion;

            if (((dwVersion == 2) && (nInBufSize >= sizeof(CeLogExportTable_V2)))
                || ((dwVersion == 3) && (nInBufSize >= sizeof(CeLogExportTable_V3)))) {
                // OK
            } else {
                ERRORMSG(1, (TEXT("CeLog DLL version does not match kernel\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (!EnterCeLogRegisterCS()) {
                return FALSE;
            }

            // Remove the DLL from the list
            if (!RemoveCeLogDLL(pExports, dwVersion)) {
                LeaveCeLogRegisterCS();
                return FALSE;
            }

            if (g_dwCeLogDLLCount == 0) {
                // Last CeLog DLL to be unloaded.

                // Clear the logging function pointers
                g_pfnCeLogData          = NULL;
                g_pfnCeLogInterrupt     = NULL;
                g_pfnCeLogInterruptData = NULL;

                // Disable logging
                CeLogDisable();
                // Safety to make sure zones are all turned off
                g_dwZoneCE = 0;

            } else if (g_dwCeLogDLLCount == 1) {
                // One CeLog DLL remains loaded.

                // Switch the function pointers from the broadcast functions
                // to direct calls into the remaining DLL
                g_pfnCeLogData          = g_pCeLogList->pfnCeLogData;
                g_pfnCeLogInterrupt     = g_pCeLogList->pfnCeLogInterrupt;
                g_pfnCeLogInterruptData = g_pCeLogList->pfnCeLogInterruptData;
            }

            // Set the new zones in UserKData
            CeLogEnableZones(g_dwZoneCE);


            DEBUGMSG(1, (TEXT("CeLog DLL unloaded.  (%u remaining)\r\n"), g_dwCeLogDLLCount));

            LeaveCeLogRegisterCS();

            break;
        }

        case IOCTL_CELOG_GETDESKTOPZONE: {
            //
            // A CeLog DLL is requesting desktop zone settings
            //

            if (lpInBuf && lpOutBuf && (nOutBufSize == sizeof(DWORD))) {
                DWORD i;
                extern DWORD g_dwKeys[HOST_TRANSCFG_NUM_REGKEYS];
                extern WCHAR* g_keyNames[];
                for (i = 0; i < HOST_TRANSCFG_NUM_REGKEYS; i++) {
                    if (NKwcsicmp(lpInBuf, g_keyNames[i]) == 0) {
                        *(DWORD *)lpOutBuf = g_dwKeys[i];
                        break;
                    }
                }
            } else {
                return FALSE;
            }
            break;
        }

        case IOCTL_CELOG_REPORTZONECHANGE: {
            //
            // A CeLog DLL is reporting that its zones have changed internally
            // without a call to CeLogSetZones
            //

            CeLogDLLInfo *pDLL;

            // Recalculate global zone settings -- we will have to walk the
            // whole list to recalculate the zones
            g_dwZoneUser = 0;
            g_dwZoneCE = 0;
            g_dwZoneProcess = 0;
            for (pDLL = g_pCeLogList; pDLL; pDLL = pDLL->pNext) {
                // We don't know which DLL changed so we have to query the zone
                // settings for every DLL
                if (pDLL->pfnCeLogQueryZones) {
                    pDLL->pfnCeLogQueryZones(&(pDLL->dwZoneUser), &(pDLL->dwZoneCE), &(pDLL->dwZoneProcess));
                }

                // Add the DLL's zones to the global zone settings
                g_dwZoneUser    |= pDLL->dwZoneUser;
                g_dwZoneCE      |= (pDLL->dwZoneCE & AVAILABLE_ZONES);  // Reserved/unused zones cannot be turned on
                g_dwZoneProcess |= pDLL->dwZoneProcess;
            }

            // Set the new zones in UserKData
            CeLogEnableZones(g_dwZoneCE);

            break;
        }

        default:
            return FALSE;
    }

    return TRUE;
}


void LoggerInit (void)
{
    // Auto-load logger from ROM if present
    LoadKernelLibrary(TEXT("CeLog.dll"));

    // Start profiler early in boot, if specified in ROM
    if (dwProfileBootOptions && dwProfileBootUSecInterval) {
        ProfilerControl control;

        control.dwVersion  = 1;
        control.dwOptions  = dwProfileBootOptions | PROFILE_START;
        control.dwReserved = 0;
        control.Kernel.dwUSecInterval = dwProfileBootUSecInterval;
        
        NKProfileSyscall (&control, sizeof(ProfilerControl));
    }
}


//------------------------------------------------------------------------------
//
//
//  KERNEL EXPORT APIS
//
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// This API has a different signature compared to the externally-exposed API
// from coredll and the compared to the function implemented by a CeLog DLL.
// This way it's impossible for an app to pass non-zero upper bits for WORDs.
//------------------------------------------------------------------------------
void
NKCeLogData(
    BOOL  fTimeStamp,
    DWORD dwID,
    VOID  *pData,
    DWORD dwLen,
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    DWORD dwFlag,
    BOOL  fFlagged
    )
{
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {  // Do not log during profiling
        g_pfnCeLogData(fTimeStamp, (WORD)dwID, pData, (WORD)dwLen,
                       dwZoneUser, dwZoneCE, (WORD)dwFlag, fFlagged);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
NKCeLogSetZones(
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    DWORD dwZoneProcess
    )
{
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        // Reserved/unused zones cannot be turned on
        dwZoneCE &= AVAILABLE_ZONES;

        // ZONE_ALWAYSON cannot be turned off, though the logging DLL(s) may
        // choose to ignore it
        dwZoneCE |= CELZONE_ALWAYSON;

        // Profiler zone cannot be turned off while the profiler is running,
        // though the logging DLL(s) may choose to ignore it.  It cannot be
        // turned on while the profiler is off either.
        if (IsCeLogStatus(CELOGSTATUS_ENABLED_PROFILE)) {
            dwZoneCE |= CELZONE_PROFILER;
        } else {
            dwZoneCE &= ~CELZONE_PROFILER;
        }

        BroadcastCeLogSetZones(dwZoneUser, dwZoneCE, dwZoneProcess);

        // Set the new zones in UserKData
        CeLogEnableZones(g_dwZoneCE);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
NKCeLogGetZones(
    LPDWORD lpdwZoneUser,        // User-defined zones
    LPDWORD lpdwZoneCE,          // Predefined zones
    LPDWORD lpdwZoneProcess,     // Process zones
    LPDWORD lpdwAvailableZones   // Zones supported by this kernel
    )
{
    BOOL fResult = FALSE;

    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        // Supply the available zones for this kernel
        if (lpdwAvailableZones) {
            *lpdwAvailableZones = AVAILABLE_ZONES;
        }

        // No need to call the CeLog DLLs to get current zones; we know already
        if (lpdwZoneUser) {
            *lpdwZoneUser = g_dwZoneUser;
        }
        if (lpdwZoneCE) {
            *lpdwZoneCE = g_dwZoneCE;
        }
        if (lpdwZoneProcess) {
            *lpdwZoneProcess = g_dwZoneProcess;
        }
        fResult = TRUE;
    }

    return fResult;
}


// Helper to build up a refcount list for the module (from all processes, yuck)
static void
KCSyncModuleRefCounts(
    PMODULELIST pmlTarget   // Kernel list item for the target module
    )
{
    DEBUGCHK (InSysCall ());  // Safe to walk lists

    // Skip all this work if the zones are off
    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_ANY,
                       CELZONE_LOADER | CELZONE_PROFILER)) {
        PPROCESS pProc;
        BOOL KeepIterating = TRUE;  // Stop iterating if we max out on processes

        // Call once to initialize iteration
        CELOG_SyncModuleRefCount(pmlTarget, NULL, TRUE);

        // If NK has a refcount, log it first
        if (pmlTarget->wRefCnt) {
            CELOG_SyncModuleRefCount(pmlTarget, g_pprcNK, FALSE);
        }
        
        // Now find all the processes that have refcounts on the module
        pProc = (PPROCESS) g_pprcNK->prclist.pFwd;
        do {
            PMODULELIST pmlProcFirst, pmlProcCur;
            
            // Search the process' module list for its instance of the module
            pmlProcFirst = (PMODULELIST) pProc->modList.pFwd;
            pmlProcCur = pmlProcFirst;
            do {
                if (pmlProcCur->pMod == pmlTarget->pMod) {
                    KeepIterating = CELOG_SyncModuleRefCount(pmlProcCur, pProc, FALSE);
                    // Stop searching since we found the module
                    break;
                }
                pmlProcCur = (PMODULELIST) pmlProcCur->link.pFwd;
                // Skip the final, dummy module that has pFwd=pmlProcFirst
            } while (pmlProcCur && (pmlProcFirst != ((PMODULELIST) pmlProcCur->link.pFwd)));

            pProc = (PPROCESS) pProc->prclist.pFwd;

        } while (pProc && (pProc != g_pprcNK) && KeepIterating);
        
        // Call one more time to flush the built-up list
        CELOG_SyncModuleRefCount(pmlTarget, NULL, FALSE);
    }
}


//------------------------------------------------------------------------------
// CELOG RESYNC API:  This function generates a series of calls to CeLogData,
// to log all currently-existing processes and threads.
//------------------------------------------------------------------------------
BOOL
KCallCeLogReSync()
{
    CEL_LOG_MARKER_V3 marker;

    DEBUGCHK (InSysCall ());  // Safe to walk lists

    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        // Send a marker
        marker.dwFrequency = g_dwCeLogTimerFrequency;
        marker.dwDefaultQuantum = g_pOemGlobal->dwDefaultThreadQuantum;
        marker.dwVersion = CELOG_VERSION;
        marker.dwNumCPUs = g_pKData->nCpus;
        g_pfnCeLogData(FALSE, CELID_LOG_MARKER, &marker, sizeof(CEL_LOG_MARKER_V3),
                       0, CELZONE_ALWAYSON, 0, FALSE);


        // Log the list of processes and threads
        if (IsCeLogEnabled(CELOGSTATUS_ENABLED_ANY,
                           CELZONE_PROCESS | CELZONE_THREAD
                           | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {
            PPROCESS pProc;

            // Since we're in a KCall, we must limit stack usage, so we can't
            // call CELOG_ProcessCreate, CELOG_ThreadCreate, CELOG_ModuleLoad
            // -- instead use CELOG_Sync*.

            pProc = g_pprcNK;
            do {
                // Log the process
                CELOG_SyncProcess(pProc);

                // Log each of the process' threads
                if (IsCeLogEnabled(CELOGSTATUS_ENABLED_ANY,
                                   CELZONE_THREAD | CELZONE_PROFILER)) {
                    PTHREAD pthFirst, pthCur;

                    pthFirst = (PTHREAD) pProc->thrdList.pFwd;
                    pthCur = pthFirst;
                    do {
                        CELOG_SyncThread(pthCur, pProc);
                        pthCur = (PTHREAD) pthCur->thLink.pFwd;
                        // Skip the final, dummy thread that has pFwd=pthFirst
                    } while (pthCur && (pthFirst != (PTHREAD) pthCur->thLink.pFwd));
                }

                pProc = (PPROCESS) pProc->prclist.pFwd;

            } while (pProc && (pProc != g_pprcNK));
        }

        // For backward-compatibility (used by some 3rd-party profiling tools) we need
        // to log only a single list of all modules, instead of a list per process.
        if (IsCeLogEnabled(CELOGSTATUS_ENABLED_ANY,
                           CELZONE_LOADER | CELZONE_THREAD
                           | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {
            PMODULELIST pmlFirst, pmlCur;

            // Walk the kernel module list
            pmlFirst = (PMODULELIST) g_pprcNK->modList.pFwd;
            pmlCur = pmlFirst;
            do {
                CELOG_SyncModule(pmlCur->pMod);
                KCSyncModuleRefCounts(pmlCur);

                pmlCur = (PMODULELIST) pmlCur->link.pFwd;
                // Skip the final, dummy module that has pFwd=pmlFirst
            } while (pmlCur && (pmlFirst != ((PMODULELIST) pmlCur->link.pFwd)));
        }

        // Send a sync end marker
        g_pfnCeLogData(FALSE, CELID_SYNC_END, NULL, 0, 0, CELZONE_ALWAYSON,
                       0, FALSE);
    }

    return TRUE;
}

