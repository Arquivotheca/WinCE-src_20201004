//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include "altimports.h"


// Pointers to the current DLL entry points.
// No DLLs loaded: These will be NULL.
// Only a single DLL loaded: These will be the entry points for that DLL.
// More than one DLL loaded: These will be the broadcast functions.
// These pointers are not strictly necessary since the list could be used to
// figure everything out, but they make the accesses faster.
void (*g_pfnCeLogData)();           // Receives CeLogData calls
static PFNVOID g_pfnCeLogInterrupt;        // Receives CeLogInterrupt calls
// Cannot use a global pointer for CeLogSetZones because we always need to
// use the broadcast function, in order to recalculate zones
static DWORD   g_dwCeLogTimerFrequency;    // Most recent nonzero timer frequency

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

// Set by the TLB miss handler
DWORD dwCeLogTLBMiss;

// OEM can override these globals to control CeLog behavior
DWORD dwCeLogLargeBuf;
DWORD dwCeLogSmallBuf;

// AppVerifier shim globals
PFN_SHIMINITMODULE     g_pfnShimInitModule;
PFN_SHIMWHICHMOD       g_pfnShimWhichMod;
PFN_SHIMUNDODEPENDS    g_pfnShimUndoDepends;
PFN_SHIMIOCONTROL      g_pfnShimIoControl;
PFN_SHIMGETPROCMODLIST g_pfnShimGetProcModList;
PFN_SHIMCLOSEMODULE    g_pfnShimCloseModule;
PFN_SHIMCLOSEPROCESS   g_pfnShimCloseProcess;




//------------------------------------------------------------------------------
//
//
//  APPVERIFIER SHIM DLL REGISTRATION
//
//
//------------------------------------------------------------------------------

BOOL FreeOneLibrary (PMODULE, BOOL);
extern CRITICAL_SECTION ModListcs;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL KernelLibIoControl_Verifier(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    switch (dwIoControlCode) {
        case IOCTL_VERIFIER_IMPORT: {
            VerifierImportTable *pImports = (VerifierImportTable*)lpInBuf;

            extern const PFNVOID Win32Methods[];
            extern void* KmodeEntries(void);

//            extern const PFNVOID ExtraMethods[];

            if ((nInBufSize != sizeof(VerifierImportTable))
                || (pImports->dwVersion != VERIFIER_IMPORT_VERSION)) {
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            pImports->dwVersion       = VERIFIER_IMPORT_VERSION;
            pImports->pWin32Methods   = (FARPROC*) Win32Methods;
            pImports->NKDbgPrintfW    = (FARPROC) NKDbgPrintfW;
            pImports->pExtraMethods = (FARPROC*) KmodeEntries();
            pImports->pKData          = &KData;
            pImports->KAsciiToUnicode = (FARPROC) KAsciiToUnicode;
            pImports->LoadOneLibraryW = (FARPROC) LoadOneLibraryW;
            pImports->FreeOneLibrary = (FARPROC) FreeOneLibrary;
            pImports->AllocMem = (FARPROC) AllocMem;
            pImports->FreeMem = (FARPROC) FreeMem;
            pImports->AllocName = (FARPROC) AllocName;
            pImports->InitializeCriticalSection = (FARPROC) InitializeCriticalSection;
            pImports->EnterCriticalSection = (FARPROC) EnterCriticalSection;
            pImports->LeaveCriticalSection = (FARPROC) LeaveCriticalSection;
            pImports->pModListcs = & ModListcs;

            break;
        }
        case IOCTL_VERIFIER_REGISTER: {
            VerifierExportTable *pExports = (VerifierExportTable*)lpInBuf;

            if ((nInBufSize != sizeof(VerifierExportTable))
                || (pExports->dwVersion != VERIFIER_EXPORT_VERSION)) {
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            // Hook up the logging function pointers
            g_pfnShimInitModule     = (PFN_SHIMINITMODULE)     pExports->pfnShimInitModule;
            g_pfnShimWhichMod       = (PFN_SHIMWHICHMOD)       pExports->pfnShimWhichMod;
            g_pfnShimUndoDepends    = (PFN_SHIMUNDODEPENDS)    pExports->pfnShimUndoDepends;
            g_pfnShimIoControl      = (PFN_SHIMIOCONTROL)      pExports->pfnShimIoControl;
            g_pfnShimGetProcModList = (PFN_SHIMGETPROCMODLIST) pExports->pfnShimGetProcModList;
            g_pfnShimCloseModule    = (PFN_SHIMCLOSEMODULE)    pExports->pfnShimCloseModule;
            g_pfnShimCloseProcess   = (PFN_SHIMCLOSEPROCESS)   pExports->pfnShimCloseProcess;

            break;
        }
        default:
            if (g_pfnShimIoControl) {
                return g_pfnShimIoControl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
            }
            return FALSE;
    }

    return TRUE;
}





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
static BOOL AddCeLogDLL(
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
    g_dwZoneCE      |= pDLL->dwZoneCE;
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


#ifdef NKPROF

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

#endif  // NKPROF


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static _inline void 
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
        g_dwZoneCE      |= pDLL->dwZoneCE;
        g_dwZoneProcess |= pDLL->dwZoneProcess;
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Wrappers because these calls are functions on some processors and macros on
// others.
DWORD
Phys2VirtWrapper(
    DWORD pfn
    )
{
    return (DWORD) Phys2Virt(pfn);
}

BOOL
InSysCallWrapper(
    )
{
    return InSysCall();
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

            extern const PFNVOID Win32Methods[];
            extern const PFNVOID EvntMthds[];
            extern const PFNVOID MapMthds[];
            extern DWORD dwDefaultThreadQuantum;

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

                pImports->pCreateEventW              = (FARPROC) SC_CreateEvent;
                pImports->pEventModify               = (FARPROC) SC_EventModify;
                pImports->pCreateFileMappingW        = (FARPROC) SC_CreateFileMapping;
                pImports->pMapViewOfFile             = (FARPROC) MapMthds[ID_FSMAP_MAPVIEWOFFILE];  // 0 if nomapfile
                pImports->pUnmapViewOfFile           = (FARPROC) SC_UnmapViewOfFile;
                pImports->pCloseHandle               = (FARPROC) SC_CloseHandle;
                pImports->pLockPages                 = (FARPROC) SC_LockPages;
                pImports->pUnlockPages               = (FARPROC) SC_UnlockPages;
                pImports->pVirtualAlloc              = (FARPROC) Win32Methods[W32_VirtualAlloc];
                pImports->pVirtualFree               = (FARPROC) Win32Methods[W32_VirtualFree];
                pImports->pMapPtrToProcess           = (FARPROC) SC_MapPtrToProcess;
                pImports->pQueryPerformanceCounter   = (FARPROC) SC_QueryPerformanceCounter;
                pImports->pQueryPerformanceFrequency = (FARPROC) SC_QueryPerformanceFrequency;
                pImports->pNKDbgPrintfW              = (FARPROC) NKDbgPrintfW;
                pImports->pCeLogReSync               = (FARPROC) SC_CeLogReSync;
                pImports->pGetLastError              = (FARPROC) SC_GetLastError;
                pImports->pSetLastError              = (FARPROC) SC_SetLastError;
                pImports->pGetThreadCallStack        = (FARPROC) NKGetThreadCallStack;
                pImports->pInSysCall                 = (FARPROC) InSysCallWrapper;
                pImports->pdwCeLogTLBMiss            = &dwCeLogTLBMiss;
                pImports->dwCeLogLargeBuf            = dwCeLogLargeBuf;
                pImports->dwCeLogSmallBuf            = dwCeLogSmallBuf;
                pImports->dwDefaultThreadQuantum     = dwDefaultThreadQuantum;

            } else {
                // Versions 2 & 3 used a totally different import structure from
                // current DLL versions.
                DWORD* prgdwImports;

                // Check import version and struct size
                if (pImports->dwVersion == 2) {
                    if (nInBufSize != 14*sizeof(DWORD)) {
                        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                        return FALSE;
                    }
                } else if (pImports->dwVersion == 3) {
                    if (nInBufSize != 16*sizeof(DWORD)) {
                        KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                        return FALSE;
                    }
                } else {
                    ERRORMSG(1, (TEXT("CeLog DLL version is not supported\r\n")));
                    KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                    return FALSE;
                }
                
                prgdwImports = (DWORD*)lpInBuf;
                // prgdwImports[0] is the version
                prgdwImports[ 1] = (DWORD) Win32Methods;
                prgdwImports[ 2] = (DWORD) EvntMthds;
                prgdwImports[ 3] = (DWORD) MapMthds;
                prgdwImports[ 4] = (DWORD) INTERRUPTS_ENABLE;
                prgdwImports[ 5] = (DWORD) Phys2VirtWrapper;
                prgdwImports[ 6] = (DWORD) InSysCallWrapper;
                prgdwImports[ 7] = (DWORD) KCall;
                prgdwImports[ 8] = (DWORD) NKDbgPrintfW;
                prgdwImports[ 9] = (DWORD) &KData;
                prgdwImports[10] = (DWORD) &dwCeLogTLBMiss;
                prgdwImports[11] = (DWORD) dwCeLogLargeBuf;
                prgdwImports[12] = (DWORD) dwCeLogSmallBuf;
                prgdwImports[13] = (DWORD) dwDefaultThreadQuantum;

                if (pImports->dwVersion == 3) {
                    prgdwImports[14] = (DWORD) SC_CloseHandle;
                    prgdwImports[15] = (DWORD) KmodeEntries();
                }
            }

            break;
        }

        case IOCTL_CELOG_IMPORT_PRIVATE: {
            //
            // A CeLog DLL is requesting private information about the kernel
            //

            CeLogPrivateImports *pImports = (CeLogPrivateImports*)lpInBuf;

            // PRIVATE IMPORT TABLE VERSIONS:
            // 1 = Current
            // (Did not exist in 4.2 and earlier)
            if ((pImports->dwVersion != CELOG_PRIVATE_IMPORT_VERSION)
                || (nInBufSize != sizeof(CeLogPrivateImports))) {
                ERRORMSG(1, (TEXT("CeLog DLL version does not match kernel\r\n")));
                KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            pImports->pRegQueryValueExW   = (FARPROC) NKRegQueryValueExW;
            pImports->pINTERRUPTS_ENABLE  = (FARPROC) INTERRUPTS_ENABLE;
            pImports->pPhys2Virt          = (FARPROC) Phys2VirtWrapper;
            pImports->pKCall              = (FARPROC) KCall;
            pImports->pCeLogData          = (FARPROC) SC_CeLogData;
            pImports->pKData              = &KData;
            
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

#ifdef NKPROF
            if (g_pfnCeLogInterrupt == NULL) {
                g_pfnCeLogInterrupt = pExports->pfnCeLogInterrupt;
            } else if ((g_pfnCeLogInterrupt != BroadcastCeLogInterrupt)
                       && (pExports->pfnCeLogInterrupt != NULL)) {
                g_pfnCeLogInterrupt = BroadcastCeLogInterrupt;
            }
#endif  // NKPROF

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
#ifdef NKPROF
                g_pfnCeLogInterrupt  = g_pCeLogList->pfnCeLogInterrupt;
#endif
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
                    if (strcmpW (lpInBuf, g_keyNames[i]) == 0) {
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
                g_dwZoneCE      |= pDLL->dwZoneCE;
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





//------------------------------------------------------------------------------
//
//
//  KERNEL EXPORT APIS
//
//
//------------------------------------------------------------------------------


#ifdef NKPROF

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
CeLogInterrupt( 
    DWORD dwLogValue
    )
{
    // Make use of the CeLog tracking call to count interrupts for the kernel
    // profiler.  This counter is not really related to CeLog.
    extern BOOL  bProfileTimerRunning;
    extern DWORD g_ProfilerState_dwInterrupts;
    
    if (bProfileTimerRunning
        && !(dwLogValue & 0x80000000)) {  


        g_ProfilerState_dwInterrupts++;
    }

    // Do the IsCeLogZoneEnabled() test here rather than in assembly because it 
    // does not affect non-profiling builds.  The function call(s) to get to
    // this point are wasted work only on profiling builds in which CeLog is not 
    // currently active.
    if (IsCeLogZoneEnabled(CELZONE_INTERRUPT) && g_pfnCeLogInterrupt) {
        g_pfnCeLogInterrupt(dwLogValue);
    }
}

#endif // NKPROF


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
CeLogThreadMigrate(
    HANDLE hProcess,
    DWORD dwReserved
    )
{
    DEBUGCHK(IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL));
    if (IsCeLogZoneEnabled(CELZONE_MIGRATE)) {
        CEL_THREAD_MIGRATE cl;
        
        cl.hProcess = hProcess;
        // Ignore the reserved DWORD for now
        
        g_pfnCeLogData(TRUE, CELID_THREAD_MIGRATE, &cl, sizeof(CEL_THREAD_MIGRATE),
                       0, CELZONE_MIGRATE, 0, FALSE);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_CeLogData( 
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
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {  // Do not log during profiling
        // Check caller buffer access
        if ((KERN_TRUST_FULL != pCurProc->bTrustLevel)
            && pData && wLen) {
            pData = SC_MapPtrWithSize(pData, wLen, SC_GetCallerProcess());
            DEBUGCHK(pData);
            if (!pData) {
                // Caller does not have access to the buffer.  Allow the event
                // to be logged, but without any data.
                wLen = 0;
            }
        }

        g_pfnCeLogData(fTimeStamp, wID, pData, wLen, dwZoneUser, dwZoneCE, wFlag, fFlagged);
    }
}


// CeLog zones available in all builds
#define AVAILABLE_ZONES_ALL_BUILDS                                              \
    (CELZONE_RESCHEDULE | CELZONE_MIGRATE | CELZONE_DEMANDPAGE | CELZONE_THREAD \
     | CELZONE_PROCESS | CELZONE_PRIORITYINV | CELZONE_CRITSECT | CELZONE_SYNCH \
     | CELZONE_HEAP | CELZONE_VIRTMEM | CELZONE_LOADER | CELZONE_MEMTRACKING    \
     | CELZONE_ALWAYSON | CELZONE_MISC | CELZONE_BOOT_TIME)
// CeLog zones available only in profiling builds
#define AVAILABLE_ZONES_PROFILING \
    (CELZONE_INTERRUPT | CELZONE_TLB | CELZONE_KCALL | CELZONE_PROFILER)


// Define the zones available on this kernel
#ifdef NKPROF
#define AVAILABLE_ZONES (AVAILABLE_ZONES_ALL_BUILDS | AVAILABLE_ZONES_PROFILING)
#else
#define AVAILABLE_ZONES (AVAILABLE_ZONES_ALL_BUILDS)
#endif // NKPROF


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
SC_CeLogSetZones( 
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    DWORD dwZoneProcess
    )
{
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        BroadcastCeLogSetZones(dwZoneUser, dwZoneCE, dwZoneProcess);

        // Set the new zones in UserKData
        CeLogEnableZones(g_dwZoneCE);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
SC_CeLogGetZones( 
    LPDWORD lpdwZoneUser,        // User-defined zones
    LPDWORD lpdwZoneCE,          // Predefined zones
    LPDWORD lpdwZoneProcess,     // Process zones
    LPDWORD lpdwAvailableZones   // Zones supported by this kernel
    )
{
    BOOL fResult = FALSE;

    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        // Check caller buffer access
        if (KERN_TRUST_FULL != pCurProc->bTrustLevel) {
            lpdwZoneUser       = SC_MapPtrWithSize(lpdwZoneUser, sizeof(DWORD), SC_GetCallerProcess());
            lpdwZoneCE         = SC_MapPtrWithSize(lpdwZoneCE, sizeof(DWORD), SC_GetCallerProcess());
            lpdwZoneProcess    = SC_MapPtrWithSize(lpdwZoneProcess, sizeof(DWORD), SC_GetCallerProcess());
            lpdwAvailableZones = SC_MapPtrWithSize(lpdwAvailableZones, sizeof(DWORD), SC_GetCallerProcess());
        }

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


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CELOG RESYNC API:  This function generates a series of calls to CeLogData, 
// to log all currently-existing processes and threads.
//

// This buffer is used during resync to minimize stack usage
union {  // Union so that the buffer is the max of all these and aligned properly
    struct {
        union {
            CEL_PROCESS_CREATE      ProcessBase;
            CEL_EXTRA_PROCESS_INFO  ProcessExtra;
            CEL_THREAD_CREATE       ThreadBase;
            CEL_MODULE_LOAD         ModuleBase;
            CEL_EXTRA_MODULE_INFO   ModuleExtra;
            CEL_SYSTEM_INVERT       Invert;
        };
        WCHAR _sz[MAX_PATH];  // Not used directly; string starts at end of data above
    };
    struct {
        CEL_MODULE_REFERENCES       ModuleRef;
        CEL_PROCESS_REFCOUNT        _ref[MAX_PROCESSES-1];  // Not used directly; list starts at end of ModuleRef
    };
} g_CeLogSyncBuffer;

// Cause the scheduler's default thread quantum to be visible
extern DWORD dwDefaultThreadQuantum;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper func to generate CeLog process events, minimizing stack usage.
_inline static void 
CELOG_SyncProcess(
    PPROCESS pProc
    )
{
    PCEL_PROCESS_CREATE     pclBase  = &g_CeLogSyncBuffer.ProcessBase;
    PCEL_EXTRA_PROCESS_INFO pclExtra = &g_CeLogSyncBuffer.ProcessExtra;
    DWORD dwVMBase;
    WORD  wLen;

    // Special case: fixup nk.exe to tell where the code lives, not the VM
    dwVMBase = pProc->dwVMBase;
    if (dwVMBase == (SECURE_SECTION << VA_SECTION)) {
        dwVMBase = pTOC->physfirst;
    }

    // Log base process info
    wLen = 0;
    pclBase->hProcess = pProc->hProc;
    pclBase->dwVMBase = dwVMBase;
    if (pProc->lpszProcName) {
        wLen = strlenW(pProc->lpszProcName) + 1;
        if (wLen <= MAX_PATH) {
            kstrcpyW(pclBase->szName, pProc->lpszProcName); 
        } else {
            wLen = 0;
        }
    }

    g_pfnCeLogData(TRUE, CELID_PROCESS_CREATE, (PVOID) pclBase,
                   (WORD) (sizeof(CEL_PROCESS_CREATE) + (wLen * sizeof(WCHAR))),
                   0, CELZONE_PROCESS | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER,  // CELZONE_THREAD necessary for getting thread names
                   0, FALSE);

    // Log extra process info
    pclExtra->hProcess = pProc->hProc;
    pclExtra->dwVMLen  = pProc->e32.e32_vsize;
    wLen = CELOG_FileInfoFromOE(&pProc->oe, &pclExtra->dwOID,
                                pclExtra->szFullPath);
        
    g_pfnCeLogData(TRUE, CELID_EXTRA_PROCESS_INFO, (PVOID) pclExtra,
                   (WORD) (sizeof(CEL_EXTRA_PROCESS_INFO) + (wLen * sizeof(WCHAR))),
                   0, CELZONE_PROCESS | CELZONE_LOADER | CELZONE_PROFILER,
                   0, FALSE);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper func to generate a CeLog thread create event, minimizing stack usage.
_inline static void 
CELOG_SyncThread(
    PTHREAD pThread,
    HANDLE  hProcess
    )
{
    PCEL_THREAD_CREATE pcl = &g_CeLogSyncBuffer.ThreadBase;
    WORD wLen = 0;

    pcl->hProcess    = hProcess;
    pcl->hThread     = pThread->hTh;
    pcl->hModule     = (HANDLE)0;  // The reader can figure out hModule from dwStartAddr
    pcl->dwStartAddr = pThread->dwStartAddr;
    pcl->nPriority   = pThread->bBPrio;

    if (pThread == pThread->pOwnerProc->pMainTh) {
        // Main thread gets the process' name
        pcl->hModule = hProcess;
        wLen = strlenW(pThread->pOwnerProc->lpszProcName) + 1;
        if (wLen <= MAX_PATH) {
            kstrcpyW(pcl->szName, pThread->pOwnerProc->lpszProcName); 
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

    g_pfnCeLogData(TRUE, CELID_THREAD_CREATE, (PVOID) pcl, 
                   (WORD)(sizeof(CEL_THREAD_CREATE) + (wLen * sizeof(WCHAR))),
                   0, CELZONE_THREAD | CELZONE_PROFILER, 0, FALSE);
    
    // Log priority inversion if necessary
    if (IsCeLogZoneEnabled(CELZONE_PRIORITYINV)
        && (pThread->bBPrio != pThread->bCPrio)) {

        PCEL_SYSTEM_INVERT pclInvert = &g_CeLogSyncBuffer.Invert;

        pclInvert->hThread   = pThread->hTh;
        pclInvert->nPriority = pThread->bCPrio;

        g_pfnCeLogData(TRUE, CELID_SYSTEM_INVERT, (PVOID) pclInvert,
                       sizeof(CEL_SYSTEM_INVERT), 0, CELZONE_PRIORITYINV,
                       0, FALSE);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper func to generate CeLog module events, minimizing stack usage.
_inline static void 
CELOG_SyncModule(
    HANDLE  hProcess,
    PMODULE pMod
    )
{
    PCEL_MODULE_LOAD       pclBase  = &g_CeLogSyncBuffer.ModuleBase;
    PCEL_EXTRA_MODULE_INFO pclExtra = &g_CeLogSyncBuffer.ModuleExtra;
    PCEL_MODULE_REFERENCES pclRef   = &g_CeLogSyncBuffer.ModuleRef;
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
    
    g_pfnCeLogData(TRUE, CELID_MODULE_LOAD, (PVOID) pclBase,
                   (WORD)(sizeof(CEL_MODULE_LOAD) + (wLen * sizeof(WCHAR))),
                   0, CELZONE_LOADER | CELZONE_THREAD | CELZONE_MEMTRACKING | CELZONE_PROFILER,  // CELZONE_THREAD necessary for getting thread names
                   0, FALSE);

    // Log extra module info
    pclExtra->hModule = (HANDLE) pMod;
    pclExtra->dwVMLen = pMod->e32.e32_vsize;
    pclExtra->dwModuleFlags = CELOG_ModuleFlags((DWORD)pMod->BasePtr);
    wLen = CELOG_FileInfoFromOE(&pMod->oe, &pclExtra->dwOID,
                                pclExtra->szFullPath);

    g_pfnCeLogData(TRUE, CELID_EXTRA_MODULE_INFO, (PVOID) pclExtra,
                   (WORD)(sizeof(CEL_EXTRA_MODULE_INFO) + (wLen * sizeof(WCHAR))),
                   0, CELZONE_LOADER | CELZONE_PROFILER,
                   0, FALSE);

    // Log per-process refcounts for the module
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
        g_pfnCeLogData(TRUE, CELID_MODULE_REFERENCES, (PVOID) pclRef,
                       (WORD) (sizeof(CEL_MODULE_REFERENCES) + ((wLen - 1) * sizeof(CEL_PROCESS_REFCOUNT))),
                       0, CELZONE_LOADER | CELZONE_PROFILER, 0, FALSE);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
SC_CeLogReSync()
{
    CEL_LOG_MARKER marker;

    // KCall so nobody changes ProcArray or pModList out from under us
    if (!InSysCall()) {
        return KCall((PKFN)SC_CeLogReSync);
    }

    KCALLPROFON(74);

    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) {
        // Send a marker
        marker.dwFrequency = g_dwCeLogTimerFrequency;
        marker.dwDefaultQuantum = dwDefaultThreadQuantum;
        marker.dwVersion = CELOG_VERSION;
        g_pfnCeLogData(FALSE, CELID_LOG_MARKER, &marker, sizeof(CEL_LOG_MARKER),
                       0, CELZONE_ALWAYSON, 0, FALSE);


        // Log the list of processes and threads
        if (IsCeLogEnabled(CELOGSTATUS_ENABLED_ANY,
                           CELZONE_PROCESS | CELZONE_THREAD
                           | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {
            ACCESSKEY ulOldKey;
            int       nProc;

            // Since we're in a KCall, we must limit stack usage, so we can't
            // call CELOG_ProcessCreate, CELOG_ThreadCreate, CELOG_ModuleLoad
            // -- instead use CELOG_Sync*.
    
            SWITCHKEY(ulOldKey, 0xffffffff); // Need access to touch procnames
            for (nProc = 0; nProc < 32; nProc++) {
    
                if (ProcArray[nProc].dwVMBase) {
                    THREAD* pThread;
    
                    // Log the process
                    CELOG_SyncProcess(&ProcArray[nProc]);
    
                    // Log each of the process' threads
                    if (IsCeLogEnabled(CELOGSTATUS_ENABLED_ANY,
                                       CELZONE_THREAD | CELZONE_PROFILER)) {
                        pThread = ProcArray[nProc].pTh;
                        while (pThread != NULL) {
                            CELOG_SyncThread(pThread, pThread->pOwnerProc->hProc);
                            pThread = pThread->pNextInProc;
                        }
                    }
                }
            }
            SETCURKEY(ulOldKey);
        }

        // Log the list of modules
        if (IsCeLogEnabled(CELOGSTATUS_ENABLED_ANY,
                           CELZONE_LOADER | CELZONE_THREAD
                           | CELZONE_MEMTRACKING | CELZONE_PROFILER)) {
            PMODULE pMod;
            for (pMod = pModList; pMod; pMod = pMod->pMod) {
                CELOG_SyncModule(INVALID_HANDLE_VALUE, pMod);
            }
        }

        // Send a sync end marker
        g_pfnCeLogData(FALSE, CELID_SYNC_END, NULL, 0, 0, CELZONE_ALWAYSON,
                       0, FALSE);
    }

    KCALLPROFOFF(74);

    return TRUE;
}

