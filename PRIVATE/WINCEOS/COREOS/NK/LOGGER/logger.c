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

// Cannot use a global pointer for CeLogSetZones because we always need to
// use the broadcast function, in order to recalculate zones
static DWORD g_dwCeLogTimerFrequency; // Most recent nonzero timer frequency

// The overall zones are a union of all DLL zone settings
static DWORD g_dwZoneUser;
static DWORD g_dwZoneCE;
static DWORD g_dwZoneProcess;


// List of active DLLs
typedef struct CeLogDLLInfo_t {
    struct CeLogDLLInfo_t *pNext;     // The next DLL in the list
    // Pointers to the export functions and data
    PFNVOID pfnCeLogData;             // Receives CeLogData calls      (REQUIRED)
    PFNVOID pfnCeLogInterrupt;        // Receives CeLogInterrupt calls (OPTIONAL)
    PFNVOID pfnCeLogSetZones;         // Receives CeLogSetZones calls  (OPTIONAL)
    FARPROC pfnCeLogQueryZones;       // Receives CeLogGetZones calls, minus last param (OPTIONAL)
    DWORD   dwCeLogTimerFrequency;    // Less than or equal to QueryPerformanceFrequency()
    // Zone info for this DLL
    DWORD   dwZoneUser;
    DWORD   dwZoneCE;
    DWORD   dwZoneProcess;
    
} CeLogDLLInfo;
static CeLogDLLInfo *g_pCeLogList;    // The head of the list
static DWORD g_dwCeLogDLLCount;       // Number of DLLs in the list

// Using a type 2 heap object to track this info
ERRFALSE(sizeof(CeLogDLLInfo) <= HEAP_SIZE2);


// Simple protection around CeLog DLL registration; calls will simply fail
// instead of block if they can't get the "CS"
static DWORD g_dwCeLogRegisterCS;


// OEM can override these globals to control CeLog behavior
DWORD dwCeLogLargeBuf;
DWORD dwCeLogSmallBuf;


// CeLog zones available in all builds
#define AVAILABLE_ZONES_ALL_BUILDS                                              \
    (CELZONE_RESCHEDULE | CELZONE_MIGRATE | CELZONE_DEMANDPAGE | CELZONE_THREAD \
     | CELZONE_PROCESS | CELZONE_PRIORITYINV | CELZONE_CRITSECT | CELZONE_SYNCH \
     | CELZONE_HEAP | CELZONE_VIRTMEM | CELZONE_GWES | CELZONE_LOADER           \
     | CELZONE_MEMTRACKING | CELZONE_BOOT_TIME | CELZONE_GDI | CELZONE_DEBUG    \
     | CELZONE_ALWAYSON | CELZONE_MISC)
// CeLog zones available only in profiling builds
#define AVAILABLE_ZONES_PROFILING \
    (CELZONE_INTERRUPT | CELZONE_TLB | CELZONE_PROFILER | CELZONE_KCALL)


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
    // would generate calls into CeLog.  Instead the caller does not block, it
    // simply fails if it cannot get the CS.
    if (InterlockedCompareExchange(&g_dwCeLogRegisterCS, 1, 0) == 0) {
        return TRUE;
    }

    KSetLastError(pCurThread, ERROR_SHARING_VIOLATION);
    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static _inline VOID LeaveCeLogRegisterCS()
{
    g_dwCeLogRegisterCS = 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Add a DLL to the list and recalculate globals
static BOOL
AddCeLogDLL(
    CeLogExportTable *pExports
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
            && (pDLL->pfnCeLogQueryZones == pExports->pfnCeLogQueryZones)) {
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
    CeLogExportTable *pExports
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
            && (pDLL->pfnCeLogQueryZones == pExports->pfnCeLogQueryZones)) {

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

static BOOL CELOG_WRAP_InSysCall() {
    return InSysCall();
}


// These functions had a signature change or no longer exist in the kernel.

static BOOL CELOG_WRAP_EventModify(HANDLE hEvent, DWORD func) {
    return NKEventModify(g_pprcNK, hEvent, func);
}

static HANDLE CELOG_WRAP_CreateFileMapping(HANDLE hFile, LPSECURITY_ATTRIBUTES lpsa, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCTSTR lpName) {
    return NKCreateFileMapping(g_pprcNK, hFile, lpsa, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
}

static LPVOID CELOG_WRAP_MapViewOfFile(HANDLE hMap, DWORD dwAccess, DWORD dwOfstHigh, DWORD dwOfstLow, DWORD cbSize) {
    return MAPMapView(g_pprcNK, hMap, dwAccess, dwOfstHigh, dwOfstLow, cbSize);
}

static BOOL CELOG_WRAP_UnmapViewOfFile(LPVOID pAddr) {
    return MAPUnmapView(g_pprcNK, pAddr);
}

static BOOL CELOG_WRAP_FlushViewOfFile(LPVOID pAddr, DWORD cbSize) {
    return MAPFlushView(g_pprcNK, pAddr, cbSize);
}

static BOOL CELOG_WRAP_CloseHandle(HANDLE hObject) {
    // Used to close both map handle and event handle
    return HNDLCloseHandle(g_pprcNK, hObject);
}

static BOOL CELOG_WRAP_LockPages(LPVOID lpvAddress, DWORD cbSize, PDWORD pPFNs, int fOptions) {
    return VMLockPages(g_pprcNK, lpvAddress, cbSize, pPFNs, fOptions);
}

static BOOL CELOG_WRAP_UnlockPages(LPVOID lpvAddress, DWORD cbSize) {
    return VMUnlockPages(g_pprcNK, lpvAddress, cbSize);
}

static LPVOID CELOG_WRAP_VirtualAlloc(LPVOID lpAddress, DWORD dwSize, DWORD flAllocationType, DWORD flProtect) {
    return PROCVMAlloc(g_pprcNK, lpAddress, dwSize, flAllocationType, flProtect);
}

static BOOL CELOG_WRAP_VirtualFree(LPVOID lpAddress, DWORD dwSize, DWORD dwFreeType) {
    return PROCVMFree(g_pprcNK, lpAddress, dwSize, dwFreeType);
}

static LPVOID CELOG_WRAP_MapPtrToProcess(LPVOID lpv, HANDLE hProc) {
    // Not much we can do here
    return lpv;
}

// Legacy function took hThread; now THRDGetCallStack takes a pThread.  Legacy
// code will now be passing a thread ID.
static ULONG CELOG_WRAP_GetThreadCallStackFromId(DWORD dwThreadId, ULONG dwMaxFrames, LPVOID lpFrames, DWORD dwFlags, DWORD dwSkip)
{
    PHDATA  phd;
    PTHREAD pth;
    ULONG   ulResult = 0;

    phd = LockHandleData ((HANDLE) dwThreadId, g_pprcNK);
    if (phd) {
        pth = GetThreadPtr (phd);
        if (pth) {
            ulResult = THRDGetCallStack (pth, dwMaxFrames, lpFrames, dwFlags, dwSkip);
        }
        UnlockHandleData (phd);
    }
    return ulResult;
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
            // 4 = Current
            // 3 = Intermediate builds, post-4.2
            // 2 = CE 4.1 / 4.2
            // 1 = CE 4.0
            DEBUGMSG(0, (TEXT("CeLog DLL import version: %u\r\n"), pImports->dwVersion));
            if (pImports->dwVersion == 4) {
                if (nInBufSize != sizeof(CeLogImportTable)) {
                    KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                    return FALSE;
                }

                pImports->pCreateEventW              = (FARPROC) NKCreateEvent;
                pImports->pEventModify               = (FARPROC) CELOG_WRAP_EventModify;
                pImports->pCreateFileMappingW        = (FARPROC) CELOG_WRAP_CreateFileMapping;
                pImports->pMapViewOfFile             = (FARPROC) (MapMthds[ID_FSMAP_MAPVIEWOFFILE] ? CELOG_WRAP_MapViewOfFile : NULL);  // NULL if nomapfile
                pImports->pUnmapViewOfFile           = (FARPROC) CELOG_WRAP_UnmapViewOfFile;
                pImports->pCloseHandle               = (FARPROC) CELOG_WRAP_CloseHandle;
                pImports->pLockPages                 = (FARPROC) CELOG_WRAP_LockPages;
                pImports->pUnlockPages               = (FARPROC) CELOG_WRAP_UnlockPages;
                pImports->pVirtualAlloc              = (FARPROC) CELOG_WRAP_VirtualAlloc;
                pImports->pVirtualFree               = (FARPROC) CELOG_WRAP_VirtualFree;
                pImports->pMapPtrToProcess           = (FARPROC) CELOG_WRAP_MapPtrToProcess;
                pImports->pQueryPerformanceCounter   = (FARPROC) NKQueryPerformanceCounter;
                pImports->pQueryPerformanceFrequency = (FARPROC) NKQueryPerformanceFrequency;
                pImports->pNKDbgPrintfW              = (FARPROC) NKDbgPrintfW;
                pImports->pCeLogReSync               = (FARPROC) NKCeLogReSync;
                pImports->pGetLastError              = (FARPROC) NKGetLastError;
                pImports->pSetLastError              = (FARPROC) NKSetLastError;
                pImports->pGetThreadCallStack        = (FARPROC) CELOG_WRAP_GetThreadCallStackFromId;
                pImports->pInSysCall                 = (FARPROC) CELOG_WRAP_InSysCall;
                pImports->pdwCeLogTLBMiss            = &dwCeLogTLBMiss;
                pImports->dwCeLogLargeBuf            = dwCeLogLargeBuf;
                pImports->dwCeLogSmallBuf            = dwCeLogSmallBuf;
                pImports->dwDefaultThreadQuantum     = g_pOemGlobal->dwDefaultThreadQuantum;

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
            // 1 = CE 5.0 to current (Did not exist in 4.2 and earlier)
            if ((pImports->dwVersion != CELOG_PRIVATE_IMPORT_VERSION)
                || (nInBufSize != sizeof(CeLogPrivateImports))) {
                ERRORMSG(1, (TEXT("CeLog DLL version does not match kernel\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            pImports->pRegQueryValueExW   = NKRegQueryValueExW;
            pImports->pINTERRUPTS_ENABLE  = INTERRUPTS_ENABLE;
            pImports->pPfn2Virt           = Pfn2Virt;
            pImports->pKCall              = KCall;
            pImports->pCeLogData          = NKCeLogData;
            pImports->pKData              = g_pKData;               

            break;
        }

        case IOCTL_CELOG_REGISTER: {
            //
            // A CeLog DLL is registering to receive data
            //

            CeLogExportTable *pExports = (CeLogExportTable*)lpInBuf;

            // EXPORT TABLE VERSIONS:
            // 2 = CE 4.1 to Current
            // 1 = CE 4.0
            if ((nInBufSize != sizeof(CeLogExportTable))
                || (pExports->dwVersion != CELOG_EXPORT_VERSION)) {
                ERRORMSG(1, (TEXT("CeLog DLL version does not match kernel\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (!EnterCeLogRegisterCS()) {
                return FALSE;
            }

            // Add the new DLL to the list
            if (!AddCeLogDLL(pExports)) {
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

            CeLogExportTable *pExports = (CeLogExportTable*)lpInBuf;

            if ((nInBufSize != sizeof(CeLogExportTable))
                || (pExports->dwVersion != CELOG_EXPORT_VERSION)) {
                ERRORMSG(1, (TEXT("CeLog DLL version does not match kernel\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (!EnterCeLogRegisterCS()) {
                return FALSE;
            }

            // Remove the DLL from the list
            if (!RemoveCeLogDLL(pExports)) {
                LeaveCeLogRegisterCS();
                return FALSE;
            }

            if (g_dwCeLogDLLCount == 0) {
                // Last CeLog DLL to be unloaded.

                // Clear the logging function pointers
                g_pfnCeLogData       = NULL;
                g_pfnCeLogInterrupt  = NULL;

                // Disable logging
                CeLogDisable();
                // Safety to make sure zones are all turned off
                g_dwZoneCE = 0;

            } else if (g_dwCeLogDLLCount == 1) {
                // One CeLog DLL remains loaded.

                // Switch the function pointers from the broadcast functions
                // to direct calls into the remaining DLL
                g_pfnCeLogData       = g_pCeLogList->pfnCeLogData;
                g_pfnCeLogInterrupt  = g_pCeLogList->pfnCeLogInterrupt;
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
#if 0
    // Load code-coverage buffer allocator from ROM if present
    LoadKernelLibrary(TEXT("kcover.dll"));
#endif

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
        // though the logging DLL(s) may choose to ignore it
        if (IsCeLogStatus(CELOGSTATUS_ENABLED_PROFILE)) {
            dwZoneCE |= CELZONE_PROFILER;
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
    CEL_LOG_MARKER marker;

    DEBUGCHK (InSysCall ());  // Safe to walk lists

    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        // Send a marker
        marker.dwFrequency = g_dwCeLogTimerFrequency;
        marker.dwDefaultQuantum = g_pOemGlobal->dwDefaultThreadQuantum;
        marker.dwVersion = CELOG_VERSION;
        g_pfnCeLogData(FALSE, CELID_LOG_MARKER, &marker, sizeof(CEL_LOG_MARKER),
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

