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

//------------------------------------------------------------------------------
//
//  Module Name:  
//  
//      ceperf.c
//  
//  Abstract:  
//
//      Implements the Windows CE Performance Measurement API.
//      
//------------------------------------------------------------------------------

#include <windows.h>
#include "ceperf.h"
#include "pceperf.h"


//------------------------------------------------------------------------------
// DLL GLOBALS
//------------------------------------------------------------------------------

//
// Shared across all DLL instances
//

HANDLE g_hMasterMap;
CePerfMaster *g_pMaster;
HANDLE g_hMasterMutex;
WCHAR  g_szDefaultFlushSubKey[MAX_PATH];

//
// Different for each DLL instance
//

DWORD  g_dwDLLSessionCount;
DLLSessionTable* g_pDLLSessions;
DLLInternalList* g_pInternalList;
CRITICAL_SECTION g_DLLCS;

#ifdef CEPERF_UNIT_TEST
CePerfOSCalls g_OSCalls = {
    QueryPerformanceFrequency,
    QueryPerformanceCounter,
    GetTickCount,
    GetThreadTimes,
    GetCurrentThreadId,
};
#endif // CEPERF_UNIT_TEST


// DLL Exports
HRESULT CePerf_Init(CePerfGlobals**);
HRESULT CePerf_Deinit(CePerfGlobals**);
HRESULT CePerf_OpenSession(HANDLE, LPCWSTR, DWORD, const CEPERF_SESSION_INFO* lpInfo);
HRESULT CePerf_CloseSession(HANDLE);
HRESULT CePerf_RegisterTrackedItem(HANDLE, HANDLE*, CEPERF_ITEM_TYPE, LPCWSTR, DWORD, const CEPERF_EXT_ITEM_DESCRIPTOR*);
HRESULT CePerf_DeregisterTrackedItem(HANDLE);
HRESULT CePerf_RegisterBulk(HANDLE, CEPERF_BASIC_ITEM_DESCRIPTOR*, DWORD, DWORD);
HRESULT CePerf_DeregisterBulk(CEPERF_BASIC_ITEM_DESCRIPTOR*, DWORD);
HRESULT CePerf_BeginDuration(HANDLE);
HRESULT CePerf_IntermediateDuration(HANDLE, CEPERF_ITEM_DATA*);
HRESULT CePerf_EndDurationWithInformation(HANDLE, DWORD, const BYTE*, DWORD, CEPERF_ITEM_DATA*);
HRESULT CePerf_AddStatistic(HANDLE, DWORD, CEPERF_ITEM_DATA*);
HRESULT CePerf_SetStatistic(HANDLE, DWORD);
HRESULT CePerf_ControlSession(HANDLE, LPCWSTR, DWORD, DWORD, const CEPERF_SESSION_INFO*);
HRESULT CePerf_ControlCPU(DWORD, LPVOID, DWORD);
HRESULT CePerf_FlushSession(HANDLE, LPCWSTR, DWORD, DWORD);
BOOL    CePerf_IOControl(DWORD, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);



#ifdef UNDER_CE

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("CePerf"),
    {
        TEXT("<unused>"), TEXT("API"),         TEXT("Lock"),     TEXT("Memory"),
        TEXT("Verbose"),  TEXT("<unused>"),    TEXT("<unused>"), TEXT("<unused>"),
        TEXT("Session"),  TEXT("TrackedItem"), TEXT("Duration"), TEXT("Statistic"),
        TEXT("<unused>"), TEXT("<unused>"),    TEXT("<unused>"), TEXT("Error")
    },
    DEFAULT_ZONES
};
#endif // DEBUG
#define GetDebugZones()     ((VOID)0)

#else // UNDER_CE

DWORD g_dwDebugZones = DEFAULT_ZONES;

//------------------------------------------------------------------------------
// GetDebugZones - NT helper to allow zones to be set in the environment via the
// CEPERF_ZONES variable.
//------------------------------------------------------------------------------

static VOID GetDebugZones() {
    char* pEnvVarValue;
    pEnvVarValue = getenv("CEPERF_ZONES");
    if (pEnvVarValue) {
        g_dwDebugZones = strtoul(pEnvVarValue, NULL, 16);
        RETAILMSG(ZONE_VERBOSE, (TEXT("CePerf: debug zones = 0x%08X\r\n"), g_dwDebugZones));
    }
}

#endif // UNDER_CE


//------------------------------------------------------------------------------
// IOControl: Only used for unit testing
//------------------------------------------------------------------------------
BOOL
CePerf_IOControl(
    DWORD   dwInstData,
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
#ifdef CEPERF_UNIT_TEST
    switch (dwIoControlCode) {
    case IOCTL_CEPERF_REPLACE_OSCALLS:
        if (lpInBuf && (nInBufSize >= sizeof(CePerfOSCalls))) {
            CePerfOSCalls* pOSCalls = (CePerfOSCalls*) lpInBuf;
            if (pOSCalls->pfnQueryPerformanceCounter) {
                g_OSCalls.pfnQueryPerformanceCounter   = pOSCalls->pfnQueryPerformanceCounter;
            }
            if (pOSCalls->pfnQueryPerformanceFrequency) {
                g_OSCalls.pfnQueryPerformanceFrequency = pOSCalls->pfnQueryPerformanceFrequency;
            }
            if (pOSCalls->pfnGetTickCount) {
                g_OSCalls.pfnGetTickCount              = pOSCalls->pfnGetTickCount;
            }
            if (pOSCalls->pfnGetCurrentThreadId) {
                g_OSCalls.pfnGetCurrentThreadId        = pOSCalls->pfnGetCurrentThreadId;
            }
            return TRUE;
        }
        break;
    default:
        break;
    }
#endif // CEPERF_UNIT_TEST

    return FALSE;
}


//------------------------------------------------------------------------------
// Clean up internal data for one DLL/EXE that has loaded CePerf.  After this
// call the DLL/EXE cannot access CePerf without re-loading CePerf.dll.
//------------------------------------------------------------------------------

static VOID CleanupDLLInternal(
    DLLInternalList* pListItem
    )
{
    CePerfGlobals* pGlobals;

    if (pListItem->ppGlobals) {
        pGlobals = pListItem->pGlobals;
        
        DEBUGMSG(ZONE_MEMORY,
                 (TEXT("CePerf: Cleanup internal list=0x%08x ppGlobals=0x%08x pGlobals=0x%08x\r\n"),
                  pListItem, pListItem->ppGlobals, pGlobals));

        UTlsPtr()[TLSSLOT_KERNEL] |= (TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG);  // Don't break into the kernel debugger if this excepts
        __try {
            // If a DLL opens a session, then unloads, then some other data gets
            // stored in the memory the DLL used to use, the global pointer could
            // be used for some other data.  So sanity-check it before modifying it.
            if (*(pListItem->ppGlobals) == pListItem->pGlobals) {
                // The caller is not yet unloaded; prevent it from calling into
                // CePerf after CePerf.dll is unloaded.
                *(pListItem->ppGlobals) = NULL;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // This exception occurs during process exit if a DLL opens a session,
            // then unloads without without closing its session, then CePerf unloads.
            // There is no way to control the unload order; if CePerf unloads before
            // its client DLLs then there will be no exceptions, but if client DLLs
            // unload first they can get this exception.  The client DLL can remove
            // the exception by closing their session before unloading.  However
            // their reference will also be cleaned up when CePerf.dll unloads.
            RETAILMSG(ZONE_ERROR, (TEXT("CePerf: Handled exception during unload.  This means that a DLL\r\n")));
            RETAILMSG(ZONE_ERROR, (TEXT("in this process was unloaded without closing its CePerf sessions.\r\n")));
            RETAILMSG(ZONE_ERROR, (TEXT("The DLL's pCePerfInternal value was previously at address 0x%08x.\r\n"),
                      pListItem->ppGlobals));
            RETAILMSG(ZONE_ERROR, (TEXT("To find the source, look up which DLL was previously at this address.\r\n")));
        }
        UTlsPtr()[TLSSLOT_KERNEL] &= ~(TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG);

        DEBUGCHK(pGlobals);
        if (pGlobals) {
#ifdef DEBUG
            // Attempt to catch stale pointer references
            memset(pGlobals, 0xCC, sizeof(CePerfGlobals));
#endif

            LocalFree(pGlobals);
        }
    }

#ifdef DEBUG
    // Attempt to catch stale pointer references
    memset(pListItem, 0xCC, sizeof(DLLInternalList));
#endif
    
    LocalFree(pListItem);
}


//------------------------------------------------------------------------------
// Clean up all references that all modules within this process have to
// CePerf.dll, and free the memory that has been allocated for them.
//------------------------------------------------------------------------------

static VOID CleanupDLLInternalList()
{
    DLLInternalList* pTemp;

    EnterCriticalSection(&g_DLLCS);
    
    // Clean up each pointer in the list
    while (g_pInternalList) {
        pTemp = g_pInternalList;
        g_pInternalList = pTemp->pNext;
        CleanupDLLInternal(pTemp);
    }

    LeaveCriticalSection(&g_DLLCS);
}


//------------------------------------------------------------------------------
// GetRegistryParams -- Function to get initial settings from the registry at 
// the time CePerf.dll is loaded.  The only way that this function can fail is
// if the StringCopy fails.  If there are no registry settings to be read, then
// the default settings are used. 
//------------------------------------------------------------------------------
static VOID GetRegistryParams()
{
    HKEY hkMetaData = NULL;
    LONG lResult;

    // Open HKLM\System\CePerf and detect whether there is a 
    // default registry flush key available
    lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"System\\CePerf", 0,
#ifdef UNDER_CE
                        0,
#else
                         KEY_ALL_ACCESS,
#endif // UNDER_CE
                         &hkMetaData);

    if (lResult == ERROR_SUCCESS) {
        // The key was opened.  Attempt to read the value: "DefaultFlushSubKey" 
        DWORD dwSubKeyBytes = MAX_PATH *sizeof(WCHAR);
        lResult = RegQueryValueEx(hkMetaData, L"DefaultFlushSubKey", NULL,
                                  NULL, (BYTE *)g_szDefaultFlushSubKey,
                                  &dwSubKeyBytes);
        RegCloseKey(hkMetaData);
    }
    if (lResult != ERROR_SUCCESS) {
        // the key either cannot be opened or the value and data was not present.
        // Use the default value of HKLM\System\CePerf
        if (FAILED(StringCchCopy(g_szDefaultFlushSubKey, MAX_PATH, L"System\\CePerf"))) {
            DEBUGMSG(ZONE_MEMORY,
                     (TEXT("CePerf: GetRegistryParams Failed to Copy the default string\r\n")));
        }
    }    
}


//------------------------------------------------------------------------------
// CePerfDeinit - Clean up caller's function pointer table and NULL out its
// global pointer.  This function may be called more than once per CePerf DLL
// instance if multiple callers exist inside the same process.
// (eg. Two different DLLs calling into CePerf.)
//------------------------------------------------------------------------------

HRESULT CePerf_Deinit(
    CePerfGlobals** ppGlobals
    )
{
    DWORD   LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT hResult = CEPERF_HR_INVALID_PARAMETER;
    DLLInternalList* pInternal;

    EnterCriticalSection(&g_DLLCS);
    if (ppGlobals) {

        // Remove the internal pointer from the list
        if (g_pInternalList) {
            if (g_pInternalList->ppGlobals == ppGlobals) {
                // Remove from the head of the list
                pInternal = g_pInternalList;
                g_pInternalList = pInternal->pNext;
            } else {
                DLLInternalList* pIter;

                pInternal = NULL;
                pIter = g_pInternalList;
                while (pIter->pNext) {
                    if (pIter->pNext->ppGlobals == ppGlobals) {
                        // Remove from the list
                        pInternal = pIter->pNext;
                        pIter->pNext = pIter->pNext->pNext;
                    } else {
                        pIter = pIter->pNext;
                    }
                }

                if (!pInternal) {
                    // Not found
                    goto exit;
                }
            }
            CleanupDLLInternal(pInternal);
        }

        hResult = ERROR_SUCCESS;
    }

exit:    
    LeaveCriticalSection(&g_DLLCS);
    SetLastError(LastError);  // Preserve LastError across the call

    return hResult;
}


//------------------------------------------------------------------------------
// CePerfInit - Init function pointer table for the caller, to minimize the
// amount of code in CePerf.h.  Also gets a copy of the caller's table pointer,
// for cleaning up on DLL exit.  This function may be called more than once
// per CePerf DLL instance if multiple callers exist inside the same process.
// (eg. Two different DLLs calling into CePerf.)
//------------------------------------------------------------------------------

HRESULT CePerf_Init(
    CePerfGlobals** ppGlobals           // Will receive function pointer table
    )
{
    DWORD   LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT hResult = CEPERF_HR_INVALID_PARAMETER;
    CePerfGlobals* pTemp;
    DLLInternalList* pInternal;
    
    EnterCriticalSection(&g_DLLCS);
    if (ppGlobals) {
        // Look for the internal pointer in the list
        pInternal = g_pInternalList;
        while (pInternal) {
            if (pInternal->ppGlobals == ppGlobals) {
                // Already in the list
                hResult = CEPERF_HR_ALREADY_EXISTS;

                // Return the pointer, in case the DLL was unloaded and then
                // reloaded in the same place.
                *ppGlobals = pInternal->pGlobals;
                goto exit;
            }
            pInternal = pInternal->pNext;
        }
        pInternal = (DLLInternalList*) LocalAlloc(LMEM_FIXED, sizeof(DLLInternalList));

        pTemp = (CePerfGlobals*) LocalAlloc(LPTR, sizeof(CePerfGlobals));
        if (!pInternal || !pTemp) {
            hResult = HRESULT_FROM_WIN32(GetLastError());
            if (pTemp)
                LocalFree(pTemp);
            if (pInternal)
                LocalFree(pInternal);
            goto exit;
        }

        // Set API pointers
        pTemp->dwVersion             = CEPERF_GLOBALS_VERSION;
        pTemp->pCePerfOpenSession    = CePerf_OpenSession;
        pTemp->pCePerfCloseSession   = CePerf_CloseSession;
        pTemp->pCePerfRegisterTrackedItem = CePerf_RegisterTrackedItem;
        pTemp->pCePerfDeregisterTrackedItem = CePerf_DeregisterTrackedItem;
        pTemp->pCePerfRegisterBulk   = CePerf_RegisterBulk;
        pTemp->pCePerfDeregisterBulk = CePerf_DeregisterBulk;
        pTemp->pCePerfBeginDuration  = CePerf_BeginDuration;
        pTemp->pCePerfIntermediateDuration = CePerf_IntermediateDuration;
        pTemp->pCePerfEndDurationWithInformation = CePerf_EndDurationWithInformation;
        pTemp->pCePerfAddStatistic   = CePerf_AddStatistic;
        pTemp->pCePerfSetStatistic   = CePerf_SetStatistic;
        pTemp->pCePerfControlSession = CePerf_ControlSession;
        pTemp->pCePerfControlCPU     = CePerf_ControlCPU;
        pTemp->pCePerfFlushSession   = CePerf_FlushSession;

        // Add the internal pointer to the list
        pInternal->pGlobals  = pTemp;
        pInternal->ppGlobals = ppGlobals;
        pInternal->pNext = g_pInternalList;
        g_pInternalList = pInternal;
        
        *ppGlobals = pTemp;

        DEBUGMSG(ZONE_MEMORY,
                 (TEXT("CePerf: New internal list=0x%08x ppGlobals=0x%08x pGlobals=0x%08x\r\n"),
                  pInternal, ppGlobals, *ppGlobals));

        GetRegistryParams();
        
        hResult = ERROR_SUCCESS;
    }
    
exit:
    LeaveCriticalSection(&g_DLLCS);
    SetLastError(LastError);  // Preserve LastError across the call
    
    return hResult;
}


//------------------------------------------------------------------------------
// DLL ENTRY
//------------------------------------------------------------------------------

BOOL WINAPI
DllEntry(HINSTANCE DllInstance, INT Reason, LPVOID Reserved)
{
    switch (Reason) {
    case DLL_PROCESS_ATTACH:
#ifdef UNDER_CE
        DEBUGREGISTER(DllInstance);
#endif
        DEBUGMSG(ZONE_SESSION,
                 (TEXT("CePerf: Process attach hInst=0x%08x hProc=0x%08x\r\n"),
                  DllInstance, GetCurrentProcessId()));

        InitializeCriticalSection(&g_DLLCS);
        GetDebugZones();

        // Disable thread attach and detach calls
        DisableThreadLibraryCalls(DllInstance);
        break;

    case DLL_PROCESS_DETACH:
        DEBUGMSG(ZONE_SESSION,
                 (TEXT("CePerf: Process detach hInst=0x%08x hProc=0x%08x\r\n"),
                  DllInstance, GetCurrentProcessId()));

        // Remove caller access to the function pointers
        CleanupDLLInternalList();

        // Clean up any leaked session handles
        CleanupProcess();
        DeleteCriticalSection(&g_DLLCS);
        
        // Sanity check on globals
        DEBUGCHK(g_hMasterMap == NULL);
        DEBUGCHK(g_pMaster == NULL);
        DEBUGCHK(g_hMasterMutex == NULL);
        DEBUGCHK(g_dwDLLSessionCount == 0);
        DEBUGCHK(g_pDLLSessions == NULL);

        break;
    
    default:
        DEBUGCHK(0);
        break;
    }
    
    return TRUE;
}
