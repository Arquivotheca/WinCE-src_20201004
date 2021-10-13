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
//      duration.c
//  
//  Abstract:  
//
//      Implements duration objects for the Windows CE
//      Performance Measurement API.
//      
//------------------------------------------------------------------------------

#include <windows.h>
#include "ceperf.h"
#include "ceperf_cpu.h"
#include "ceperf_log.h"
#include "pceperf.h"
#include <ttracker.h>


// GLOBALS

// Frequency of device performance counter (from QueryPerformanceCounter)
static LARGE_INTEGER g_liPerfFreq;

TTRACKER_GLOBALS* g_pTTGlobals;
static HANDLE g_hTTracker;

static VOID ClearBeginList(TrackedItem* pItem);
static HRESULT BeginFlushCSV(FlushState* pFlush);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// LIST MANAGEMENT - one list for DurationBeginList items, and another for
// DurationDataList items.

// pIsItemFree list traversal function; test if the BeginList item is free
static BOOL IsDurBLFree(LPBYTE pItem) {
    return BEGINLIST_ISFREE(pItem);
}

// pGetNextFreeItem list traversal function; for a free BeginList item, get 
// index (on current page) of next item in freelist.
static WORD GetNextFreeDurBL(LPBYTE pItem) {
    return BEGINLIST_NEXTFREE(pItem);
}

// pFreeItem list traversal function; mark a BeginList item as being free
static VOID FreeDurBL(LPBYTE pItem, WORD wNextFreeIndex) {
    BEGINLIST_MARKFREE(pItem, wNextFreeIndex);
}

// Used to operate on g_pMaster->list.pDurBLHead list of Duration BeginList objects
static DLLListFuncs g_DurBLListFuncs = {
    IsDurBLFree,                    // pIsItemFree
    GetNextFreeDurBL,               // pGetNextFreeItem
    FreeDurBL,                      // pFreeItem
};


// pIsItemFree list traversal function; test if the DataList item is free
static BOOL IsDurDLFree(LPBYTE pItem) {
    return DATALIST_ISFREE(pItem);
}

// pGetNextFreeItem list traversal function; for a free DataList item, get 
// index (on current page) of next item in freelist.
static WORD GetNextFreeDurDL(LPBYTE pItem) {
    return DATALIST_NEXTFREE(pItem);
}

// pFreeItem list traversal function; mark a DataList item as being free
static VOID FreeDurDL(LPBYTE pItem, WORD wNextFreeIndex) {
    DATALIST_MARKFREE(pItem, wNextFreeIndex);
}

// Used to operate on g_pMaster->list.pDurDLHead list of Duration DataList objects
static DLLListFuncs g_DurDLListFuncs = {
    IsDurDLFree,                    // pIsItemFree
    GetNextFreeDurDL,               // pGetNextFreeItem
    FreeDurDL,                      // pFreeItem
};


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Convert a LARGE_INTEGER value from performance counter ticks to microseconds
_inline static VOID PerformanceCounterToMicroseconds(
    const ULARGE_INTEGER* pulPerfCount,  // Performance counter ticks
    ULARGE_INTEGER*       pulMSCount     // Receives value in microseconds
    )
{
    __int64 iVal;

    if ((g_liPerfFreq.LowPart == 0) && (g_liPerfFreq.HighPart == 0)) {
        if (!CePerf_QueryPerformanceFrequency(&g_liPerfFreq)) {
            pulMSCount->QuadPart = 0;
            return;
        }
    }

    // Play some games to avoid overflow and underflow.  If the multiplication
    // is going to overflow, then divide first.  Otherwise multiply first.
    iVal = (__int64)pulPerfCount->QuadPart;
    if (iVal > (MAXLONGLONG / 1000000)) {
        // The multiplication would overflow: divide first
        iVal /= g_liPerfFreq.QuadPart;
        iVal *= 1000000;
    } else {
        // Multiply first
        iVal *= 1000000;
        iVal /= g_liPerfFreq.QuadPart;
    }

    pulMSCount->QuadPart = iVal;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
_inline static HRESULT GetCurrentPerfCounters(
    DWORD          dwRecordingFlags,    // Used to figure out which perf counters to get
    LARGE_INTEGER* pliPerfCount,        // Receives perf counter timestamp
    LARGE_INTEGER* pliThreadTickCount,  // Receives thread tick counter timestamp
    DWORD*         pdwTickCount,        // Receives absolute tick count timestamp
    LPBYTE         pCPUPerfCounters     // Receives CPU performance counters, size g_pMaster->CPU.wTotalCounterSize
    )
{
    if (dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
        if (!CePerf_QueryPerformanceCounter(pliPerfCount)) {
            return CEPERF_HR_BAD_PERFCOUNT_DATA;
        }
    }
    
    if (dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT) {
        if (dwRecordingFlags & CEPERF_DURATION_RECORD_SHARED) {
            // Special case - "shared" and "per-thread" are mutually exclusive,
            // so skip collecting per-thread times on shared durations.
            pliThreadTickCount->QuadPart = 0;
        } else {
            FILETIME CreationTime, ExitTime, KernelTime, UserTime;
            __int64* pKernelTime = (__int64*) &KernelTime;  // Reference as __int64 using pointers
            __int64* pUserTime = (__int64*) &UserTime;
            
            if (!CePerf_GetThreadTimes((HANDLE) CePerf_GetCurrentThreadId(),
                                       &CreationTime, &ExitTime,
                                       &KernelTime, &UserTime)) {
                return CEPERF_HR_BAD_PERFCOUNT_DATA;
            }

            // Return the total of kernel + user
            pliThreadTickCount->QuadPart = *pKernelTime + *pUserTime;
            // Convert to millisecond ticks: FILETIME is in 100-ns intervals
            pliThreadTickCount->QuadPart /= 10000;
        }
    }
    
    if (dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
        *pdwTickCount = CePerf_GetTickCount();
    }
    
    if ((dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
        && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {
        return pCPUQueryPerfCounters(pCPUPerfCounters, g_pMaster->CPU.wTotalCounterSize);
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Unload the TTracker DLL
static VOID CleanupTTracker(
    TrackedItem* pItem
    )
{
    DEBUGCHK(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_TTRACKER);

    // Returns FALSE when there are no more DLL references
    if (!TTrackerUnregister(g_hTTracker)) {
        g_hTTracker = NULL;
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Load the TTracker DLL
static HRESULT InitTTracker(
    TrackedItem* pItem
    )
{
    HRESULT hResult;

    DEBUGCHK(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_TTRACKER);

    // Don't try to keep reloading ttracker.dll if it fails to load once
    if (g_pMaster->dwGlobalFlags & MASTER_TTRACKER_FAILURE) {
        hResult = CEPERF_HR_NO_DLL;
    } else {
        // TTracker keeps refcounts if we register more than once
        hResult = TTrackerRegisterWithoutLogFile(&g_hTTracker);
        DEBUGCHK(!SUCCEEDED(hResult) || g_hTTracker);  // Success returns non-NULL handle
        if (!SUCCEEDED(hResult)) {
            g_pMaster->dwGlobalFlags |= MASTER_TTRACKER_FAILURE;
        }
    }
    
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Flush all the data that TTracker has collected so far.  Any process can flush
// the TTracker data, no matter which process enabled TTracker.
static HRESULT FlushTTracker(
    HANDLE hTrackedItem,
    TrackedItem* pItem,
    BOOL   fFlush,      // Write to a file?
    BOOL   fClear       // Clear after flushing?
    )
{
    HRESULT hResult = CEPERF_HR_INVALID_HANDLE;
    BOOL NeedCleanup = FALSE;

    DEBUGCHK(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_TTRACKER);

    // Temporarily open TTracker if this process doesn't already have it open
    if (!g_hTTracker) {
        hResult = InitTTracker(pItem);
        NeedCleanup = SUCCEEDED(hResult);
    }
    
    if (g_hTTracker) {
        if (fFlush) {
            const SessionHeader* pSession;

            // Derive the output filename from the session output filename
            pSession = LookupSessionHandle(ITEM_SESSION_HANDLE(hTrackedItem));
            if (pSession && (pSession->settings.dwStorageFlags & CEPERF_STORE_FILE)) {
                // Append to the filename the session is using
                WCHAR szTTrackerFileName[MAX_PATH];
                if (SUCCEEDED(StringCchCopy(szTTrackerFileName, MAX_PATH, pSession->settings.szStoragePath))
                    && SUCCEEDED(StringCchCat(szTTrackerFileName, MAX_PATH, TEXT(".ttracker.txt")))) {

                    hResult = TTrackerLogToFile(g_hTTracker, szTTrackerFileName,
                                                TTRACKER_LOG_PROCESSES | TTRACKER_LOG_THREADS,
                                                pItem->szItemName);
                }
            }
        }

        // Clear the data if requested
        if (fClear) {
            HRESULT hTTResult = TTrackerClearKernelLog();
            
            // Only overwrite successful result
            if (SUCCEEDED(hResult)) {
                hResult = hTTResult;
            }
        }
    }

    if (NeedCleanup) {
        CleanupTTracker(pItem);
    }

    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Start or stop collecting TTracker thread data
static HRESULT StartStopTTracker(
    TrackedItem* pItem,
    BOOL fStart     // TRUE=start, FALSE=stop
    )
{
    DEBUGCHK(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_TTRACKER);
    
    // Let TTracker worry about running more than one at once
    return TTrackerKernelLoggingEnable(fStart);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Validate pExt and copy resulting values (mapped pointers, etc) to pNewExt.
// Return TRUE if valid and successfully copied.
BOOL ValidateDurationDescriptor(
    const CEPERF_BASIC_ITEM_DESCRIPTOR* pBasic,
    const CEPERF_EXT_ITEM_DESCRIPTOR*   pExt,
    CEPERF_EXT_ITEM_DESCRIPTOR*         pNewExt
    )
{
    if (pBasic->type == CEPERF_TYPE_DURATION) {
        if (!pExt) {
            memset(pNewExt, 0, sizeof(CEPERF_EXT_ITEM_DESCRIPTOR));
            return TRUE;
        }
    }
    
    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// The TrackedItem object is already initialized with basic info; this func
// initializes additional Duration-specific data.
BOOL InitializeDurationData(
    HANDLE hTrackedItem,
    TrackedItem* pItem,
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt  // Will not be non-NULL on creation;
                                            // will be NULL on recreation
    )
{
    DurationDataList *pDL = NULL;
    DurationBeginList *pBL = NULL;
    DEBUGCHK(pItem->type == CEPERF_TYPE_DURATION);
    
    pItem->data.Duration.dwErrorCount = 0;

    // Initialize TTracker if necessary
    if ((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_TTRACKER)
        && (!SUCCEEDED(InitTTracker(pItem)))) {
        return FALSE;
    }

    // Allocate private RAM for the begin list
    pBL = (DurationBeginList*) AllocListItem(&g_DurBLListFuncs,
                                             g_pMaster->list.wDurBLSize,
                                             &(g_pMaster->list.offsetDurBLHead));
    if (!pBL) {
        return FALSE;
    }
    pItem->data.Duration.offsetBL = GET_MAP_OFFSET(pBL); // store the offset
    // Initialize the data
    memset((LPBYTE)pBL, 0, g_pMaster->list.wDurBLSize);
    
    DEBUGMSG(ZONE_ITEM && ZONE_MEMORY,
             (TEXT("CePerf: New BeginList=0x%08x for pItem=0x%08x\r\n"),
              pBL, pItem));
    
    if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
        DiscreteCounterData* pData;
        WORD wCounter;

        // Allocate private RAM for the deltas
        pDL = (DurationDataList*) AllocListItem(&g_DurDLListFuncs,
                                                g_pMaster->list.wDurDLSize,
                                                &(g_pMaster->list.offsetDurDLHead));
        if (!pDL) {
            // Release the BeginList we just allocated
            FreeListItem(&g_DurBLListFuncs, g_pMaster->list.wDurBLSize,
                         &(g_pMaster->list.offsetDurBLHead),
                         (LPBYTE)pBL, FALSE);
            pItem->data.Duration.offsetBL = 0;
            return FALSE;
        }
        pItem->data.Duration.MinRecord.offsetDL = GET_MAP_OFFSET(pDL); // store the offset
        // Initialize the data
        memset(pDL, 0, g_pMaster->list.wDurDLSize);
        
        // Set "min" values to -1
        pData = DATALIST_PDATA(pDL);
        for (wCounter = 0;
             (wCounter < NUM_BUILTIN_COUNTERS + g_pMaster->CPU.wNumCounters)
             && (wCounter < NUM_BUILTIN_COUNTERS + CEPERF_MAX_CPUCOUNTER_COUNT);
             wCounter++) {
            DEBUGCHK((LPBYTE)&pData[wCounter].ulMinVal.QuadPart < ((LPBYTE)pDL + g_pMaster->list.wDurDLSize));
            pData[wCounter].ulMinVal.HighPart = (DWORD)-1;
            pData[wCounter].ulMinVal.LowPart  = (DWORD)-1;
        }
        

        DEBUGMSG(ZONE_ITEM && ZONE_MEMORY,
                 (TEXT("CePerf: New DataList=0x%08x for pItem=0x%08x\r\n"),
                  pDL, pItem));
    }
    
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Marks the item's data as not in use.  Does not check to see if the data pages
// need to be released, unless fReleasePages is TRUE.
VOID FreeDurationData(
    TrackedItem* pItem,
    BOOL         fReleasePages
    )
{
    DurationBeginList *pBL;
    DEBUGCHK(pItem->type == CEPERF_TYPE_DURATION);
    
    // Clean up TTracker if necessary
    if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_TTRACKER) {
        CleanupTTracker(pItem);
    }

    // Free private RAM for the begin list.  Walk each item in the chain.
    pBL = (DurationBeginList *)GET_MAP_POINTER(pItem->data.Duration.offsetBL);
    while (pBL) {
        DurationBeginList* pTemp;

        DEBUGMSG(ZONE_ITEM && ZONE_MEMORY,
                 (TEXT("CePerf: Free BeginList=0x%08x for pItem=0x%08x\r\n"),
                  pBL, pItem));
        
        pTemp = pBL;
        pBL = (DurationBeginList *)GET_MAP_POINTER(pTemp->offsetNextDBL);
        pItem->data.Duration.offsetBL = pTemp->offsetNextDBL;
        FreeListItem(&g_DurBLListFuncs, g_pMaster->list.wDurBLSize,
                     &(g_pMaster->list.offsetDurBLHead),
                     (LPBYTE)pTemp, fReleasePages);
    }

    if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
        // Free private RAM for the deltas
        if (pItem->data.Duration.MinRecord.offsetDL != 0) {
            DEBUGMSG(ZONE_ITEM && ZONE_MEMORY,
                     (TEXT("CePerf: Free DataList=0x%08x for pItem=0x%08x\r\n"),
                      GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL), pItem));

            FreeListItem(&g_DurDLListFuncs, g_pMaster->list.wDurDLSize,
                         &(g_pMaster->list.offsetDurDLHead),
                         (LPBYTE)GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL),
                         fReleasePages);
            pItem->data.Duration.MinRecord.offsetDL = 0;
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Called after deleting a lot of tracked objects, to clean up any unused pages.
VOID FreeUnusedDurationPages()
{
    // Check the BeginList list
    FreeUnusedListPages(&g_DurBLListFuncs, g_pMaster->list.wDurBLSize, &(g_pMaster->list.offsetDurBLHead));

    // Check the DataList list
    FreeUnusedListPages(&g_DurDLListFuncs, g_pMaster->list.wDurDLSize, &(g_pMaster->list.offsetDurDLHead));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Prepare resources for doing the flush
HRESULT DurationFlushBegin(
    FlushState* pFlush
    )
{
    if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
        return BeginFlushCSV(pFlush);
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static HRESULT BeginFlushCSV(
    FlushState* pFlush
    )
{
    HRESULT hResult = ERROR_SUCCESS;
    WORD    wCounter;
    
    // Print CSV header to the output file.
    
    // Don't print it if there are no Duration objects in this session...  All
    // Durations have a BL so pDurBLHead is a good way to tell
    if (g_pMaster->list.offsetDurBLHead != 0) {

        hResult = FlushChars(pFlush, 
                             TEXT("\r\nSession Name,")
                             TEXT("Item Name,")
                             TEXT("Number of Successful Begin/End Pairs,")
                             TEXT("Number of Errors,")

                             TEXT("Min Time Delta (us/QPC),")
                             TEXT("Max Time Delta (us/QPC),")
                             TEXT("Total Time Delta (us/QPC),")
                             TEXT("StdDev Time Delta (us/QPC),")

                             TEXT("Min Thread Run-Time Delta (ms),")
                             TEXT("Max Thread Run-Time Delta (ms),")
                             TEXT("Total Thread Run-Time Delta (ms),")
                             TEXT("StdDev Thread Run-Time Delta (ms),")

                             TEXT("Min Time Delta (ms/GTC),")
                             TEXT("Max Time Delta (ms/GTC),")
                             TEXT("Total Time Delta (ms/GTC),")
                             TEXT("StdDev Time Delta (ms/GTC),")
                             );
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }

        // Print value names -- won't overflow the buffer
        for (wCounter = 0;
             (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
             wCounter++) {
            hResult = FlushChars(pFlush,
                                 TEXT(",Min CPU%02u Delta,")
                                 TEXT("Max CPU%02u Delta,")
                                 TEXT("Total CPU%02u Delta,")
                                 TEXT("StdDev CPU%02u Delta,"),
                                 wCounter, wCounter, wCounter, wCounter);
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        }

        hResult = FlushChars(pFlush, TEXT("\r\n"));
    }
    
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write descriptor for Duration item into a text stream.
_inline static HRESULT FlushDescriptorText(
    FlushState*  pFlush,
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    return FlushChars(pFlush,
                      TEXT("\thDuration= 0x%08x, dwRecordingFlags=0x%08x, %s\r\n"),
                      (DWORD) hTrackedItem, pItem->dwRecordingFlags, pItem->szItemName);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write (discrete) data from min-record Duration item into the registry
static HRESULT FlushMinDataToRegistry(
    FlushState*  pFlush,
    TrackedItem* pItem
    )
{
    LONG lResult;
    DurationDataList *pDL = (DurationDataList *) GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);
    DiscreteCounterData* pCurCounter;
    HKEY hkDuration = INVALID_HANDLE_VALUE;
    double dStdDev;

    DEBUGCHK(pFlush->hOutput != INVALID_HANDLE_VALUE);
    
    // Create per-Duration item key below pFlush->hOutput
    lResult = RegCreateKeyEx((HKEY) pFlush->hOutput, pItem->szItemName, 0, NULL, 0,
#ifdef UNDER_CE
                             0,
#else
                             KEY_ALL_ACCESS,
#endif // UNDER_CE
                             NULL, &hkDuration, NULL);
    if (lResult != ERROR_SUCCESS) {
        goto exit;
    }
    
    pCurCounter = DATALIST_PDATA(pDL);

    // BUGBUG ignoring output errors for now
    // BUGBUG not deleting old values in the registry in case recording mode changed

    // dwNumVals
    lResult = RegSetValueEx(hkDuration, TEXT("Count"), 0, REG_DWORD,
                            (LPBYTE) &(pDL->dwNumVals),
                            sizeof(DWORD));
    DEBUGCHK(lResult == ERROR_SUCCESS);

    // dwErrorCount
    lResult = RegSetValueEx(hkDuration, TEXT("ErrorCount"), 0, REG_DWORD,
                            (LPBYTE) &(pItem->data.Duration.dwErrorCount),
                            sizeof(DWORD));
    DEBUGCHK(lResult == ERROR_SUCCESS);

    // Skip the rest if the Duration was never succesfully entered
    if (pDL->dwNumVals > 0) {

        // Absolute perf count timestamp
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            // Data must be written in microseconds
            ULARGE_INTEGER ulMicroseconds;
            ULARGE_INTEGER ulStdDev;

            DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check
            
            PerformanceCounterToMicroseconds(&pCurCounter->ulTotal, &ulMicroseconds);
            lResult = RegSetValueEx(hkDuration, TEXT("PerfCountSum"), 0, REG_BINARY,
                                    (LPBYTE) &ulMicroseconds, sizeof(ULARGE_INTEGER));
            DEBUGCHK(lResult == ERROR_SUCCESS);

            PerformanceCounterToMicroseconds(&pCurCounter->ulMaxVal, &ulMicroseconds);
            lResult = RegSetValueEx(hkDuration, TEXT("PerfCountMax"), 0, REG_BINARY,
                                    (LPBYTE) &ulMicroseconds, sizeof(ULARGE_INTEGER));
            DEBUGCHK(lResult == ERROR_SUCCESS);

            PerformanceCounterToMicroseconds(&pCurCounter->ulMinVal, &ulMicroseconds);
            lResult = RegSetValueEx(hkDuration, TEXT("PerfCountMin"), 0, REG_BINARY,
                                    (LPBYTE) &ulMicroseconds, sizeof(ULARGE_INTEGER));
            DEBUGCHK(lResult == ERROR_SUCCESS);

            // Yuck -- convert to ULONGLONG and back to get microseconds
            dStdDev = GetStandardDeviation(pCurCounter, pDL->dwNumVals);
            //DEBUGCHK(dStdDev <= MAXDOUBLE / 10); // detect overflow
            ulStdDev.QuadPart = (ULONGLONG) (dStdDev * 10);    // *10 for decimal point later
            PerformanceCounterToMicroseconds(&ulStdDev, &ulMicroseconds);
            dStdDev = ((double) ulMicroseconds.QuadPart) / 10; // /10 to return decimal point to correct place
            lResult = RegSetValueEx(hkDuration, TEXT("PerfCountStdDev"), 0, REG_BINARY,
                                    (LPBYTE) &dStdDev, sizeof(double));
            DEBUGCHK(lResult == ERROR_SUCCESS);

            pCurCounter++;
        }

        // Thread run-time tick count
        if (pItem->dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT) {
            // Special case - "shared" and "per-thread" are mutually exclusive,
            // so per-thread times are always N/A on shared durations.
            if (!(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_SHARED)) {
                DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check
                
                lResult = RegSetValueEx(hkDuration, TEXT("ThreadTickSum"), 0, REG_BINARY,
                                        (LPBYTE) &(pCurCounter->ulTotal), sizeof(ULARGE_INTEGER));
                DEBUGCHK(lResult == ERROR_SUCCESS);
                lResult = RegSetValueEx(hkDuration, TEXT("ThreadTickMax"), 0, REG_BINARY,
                                        (LPBYTE) &(pCurCounter->ulMaxVal.QuadPart), sizeof(ULARGE_INTEGER));
                DEBUGCHK(lResult == ERROR_SUCCESS);
                lResult = RegSetValueEx(hkDuration, TEXT("ThreadTickMin"), 0, REG_BINARY,
                                        (LPBYTE) &(pCurCounter->ulMinVal.QuadPart), sizeof(ULARGE_INTEGER));
                DEBUGCHK(lResult == ERROR_SUCCESS);

                dStdDev = GetStandardDeviation(pCurCounter, pDL->dwNumVals);
                lResult = RegSetValueEx(hkDuration, TEXT("ThreadTickStdDev"), 0, REG_BINARY,
                                        (LPBYTE) &dStdDev, sizeof(double));
                DEBUGCHK(lResult == ERROR_SUCCESS);
            }

            pCurCounter++;
        }

        // Absolute tick count timestamp
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check
            
            lResult = RegSetValueEx(hkDuration, TEXT("TickCountSum"), 0, REG_BINARY,
                                    (LPBYTE) &(pCurCounter->ulTotal), sizeof(ULARGE_INTEGER));
            DEBUGCHK(lResult == ERROR_SUCCESS);
            // Could dump tick count as REG_DWORD but using binary so that reader code
            // can work the same way for all value types
            lResult = RegSetValueEx(hkDuration, TEXT("TickCountMax"), 0, REG_BINARY,
                                    (LPBYTE) &(pCurCounter->ulMaxVal.QuadPart), sizeof(ULARGE_INTEGER));
            DEBUGCHK(lResult == ERROR_SUCCESS);
            lResult = RegSetValueEx(hkDuration, TEXT("TickCountMin"), 0, REG_BINARY,
                                    (LPBYTE) &(pCurCounter->ulMinVal.QuadPart), sizeof(ULARGE_INTEGER));
            DEBUGCHK(lResult == ERROR_SUCCESS);

            dStdDev = GetStandardDeviation(pCurCounter, pDL->dwNumVals);
            lResult = RegSetValueEx(hkDuration, TEXT("TickCountStdDev"), 0, REG_BINARY,
                                    (LPBYTE) &dStdDev, sizeof(double));
            DEBUGCHK(lResult == ERROR_SUCCESS);

            pCurCounter++;
        }

        // CPU performance counter data
        if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
            && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {
            WORD  wCounter;
            WCHAR szSum[12];
            WCHAR szMax[12];
            WCHAR szMin[12];
            WCHAR szStdDev[15];

            // Dump DiscreteCounterData for each perf counter
            for (wCounter = 0;
                 (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
                 wCounter++) {

                // Prefast needs some help figuring out the loop invariant
                __analysis_assume(wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);

                // Print value names -- won't overflow the buffer
                swprintf(szSum,    TEXT("CPU%02uSum"),    wCounter);
                swprintf(szMin,    TEXT("CPU%02uMin"),    wCounter);
                swprintf(szMax,    TEXT("CPU%02uMax"),    wCounter);
                swprintf(szStdDev, TEXT("CPU%02uStdDev"), wCounter);

                DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check

                lResult = RegSetValueEx(hkDuration, szSum, 0, REG_BINARY,
                                        (LPBYTE) &(pCurCounter->ulTotal), sizeof(ULARGE_INTEGER));
                DEBUGCHK(lResult == ERROR_SUCCESS);
                lResult = RegSetValueEx(hkDuration, szMin, 0, REG_BINARY,
                                        (LPBYTE) &(pCurCounter->ulMaxVal.QuadPart), sizeof(ULARGE_INTEGER));
                DEBUGCHK(lResult == ERROR_SUCCESS);
                lResult = RegSetValueEx(hkDuration, szMax, 0, REG_BINARY,
                                        (LPBYTE) &(pCurCounter->ulMinVal.QuadPart), sizeof(ULARGE_INTEGER));
                DEBUGCHK(lResult == ERROR_SUCCESS);

                dStdDev = GetStandardDeviation(pCurCounter, pDL->dwNumVals);
                lResult = RegSetValueEx(hkDuration, szStdDev, 0, REG_BINARY,
                                        (LPBYTE) &dStdDev, sizeof(double));
                DEBUGCHK(lResult == ERROR_SUCCESS);

                pCurCounter++;
            }
        }
    }

exit:
    // Close per-Duration item data output key
    if (hkDuration != INVALID_HANDLE_VALUE) {
        RegCloseKey(hkDuration);
    }

    return (HRESULT) lResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write (discrete) data from min-record Duration item into a binary stream.
#pragma warning(disable:4200) // nonstandard extensions warning on zero-size array
static HRESULT FlushMinDataBinary(
    FlushState*  pFlush,
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    // Convenient declaration of the data that will be flushed
#pragma pack(push,4)
    typedef struct {
        CEPERF_LOGID id;
        CEPERF_LOG_DURATION_DISCRETE duration;
        // Followed by the array of data for each counter
        CEPERF_DISCRETE_COUNTER_DATA counter[0];
    } Data;
#pragma pack(pop)
    DurationDataList *pDL = (DurationDataList *) GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);
    WORD wNumCounters = 0;

    DEBUGCHK(pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG);

    // Calculate how many counters need to be copied
    if (pDL->dwNumVals > 0) {
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR) {
            wNumCounters = g_pMaster->CPU.wNumCounters;
            if (wNumCounters > CEPERF_MAX_CPUCOUNTER_COUNT) {
                DEBUGCHK (0);  // Someone has corrupted the master map
                return CEPERF_HR_BAD_CPUCOUNTER_DATA;
            }
        }
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            wNumCounters++;
        }
        if (pItem->dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT) {
            // Note - If the duration is shared, per-thread times are always N/A so values will be 0.
            // "Shared" and "per-thread" are mutually exclusive.
            wNumCounters++;
        }
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            wNumCounters++;
        }
    }
        
    if (pFlush->lpTempBuffer
        && (pFlush->cbTempBuffer >= sizeof(Data) + wNumCounters*sizeof(CEPERF_DISCRETE_COUNTER_DATA))) {
        Data* pData = (Data*) pFlush->lpTempBuffer;
        DiscreteCounterData* pSourceData;
        DWORD cbData;
        WORD wCounter;

        pData->id = CEPERF_LOGID_DURATION_DISCRETE;
        pData->duration.dwItemID = (DWORD) hTrackedItem;
        pData->duration.dwNumVals = pDL->dwNumVals;
        pData->duration.dwErrorCount = pItem->data.Duration.dwErrorCount;
        cbData = sizeof(CEPERF_LOGID) + sizeof(CEPERF_LOG_DURATION_DISCRETE);

        // Copy all of the counters into the buffer
        pSourceData = DATALIST_PDATA(pDL);
        for (wCounter = 0; wCounter < wNumCounters; wCounter++) {
            pData->counter[wCounter].ulTotal  = pSourceData[wCounter].ulTotal;
            pData->counter[wCounter].ulMinVal = pSourceData[wCounter].ulMinVal;
            pData->counter[wCounter].ulMaxVal = pSourceData[wCounter].ulMaxVal;
            pData->counter[wCounter].dStdDev  = GetStandardDeviation(&pSourceData[wCounter], pDL->dwNumVals);
        }
        cbData += wNumCounters*sizeof(CEPERF_DISCRETE_COUNTER_DATA);

        // Write the buffer to the output location
        return FlushBytes(pFlush, (LPBYTE) pData, cbData);
    }

    DEBUGCHK(0);  // Underestimated necessary buffer size
    return CEPERF_HR_NOT_ENOUGH_MEMORY;
}
#pragma warning(default:4200) // nonstandard extensions warning on zero-size array


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write (discrete) data from min-record Duration item into a text stream.
static HRESULT FlushMinDataText(
    FlushState*  pFlush,
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    BOOL         fDescriptorFlushed   // Simplify output if the descriptor
                                      // has already been flushed.
    )
{
    HRESULT hResult;
    DurationDataList * pDL = (DurationDataList *)GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);

    // Write basic data
    if (!fDescriptorFlushed) {
        // Add the Duration handle if there's no descriptor on the output already
        hResult = FlushChars(pFlush, TEXT("\thDuration= 0x%08x\r\n"),
                             (DWORD)hTrackedItem);
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }
    
    hResult = FlushChars(pFlush, TEXT("\t\tCount=%u, ErrorCount=%u\r\n"),
                         pDL->dwNumVals,
                         pItem->data.Duration.dwErrorCount);
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // Skip the rest if the Duration was never succesfully entered
    if (pDL->dwNumVals > 0) {
        DiscreteCounterData* pCurCounter;
        ULARGE_INTEGER ulAverage;
        double dStdDev;
    
        pCurCounter = DATALIST_PDATA(pDL);
    
        // Write each counter
    
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            ULARGE_INTEGER ulMin, ulMax, ulTotal, ulStdDev;

            // Convert to microseconds
            PerformanceCounterToMicroseconds(&pCurCounter->ulTotal, &ulTotal);
            PerformanceCounterToMicroseconds(&pCurCounter->ulMinVal, &ulMin);
            PerformanceCounterToMicroseconds(&pCurCounter->ulMaxVal, &ulMax);
            // Yuck -- convert to ULONGLONG and back to get microseconds
            dStdDev = GetStandardDeviation(pCurCounter, pDL->dwNumVals);
            //DEBUGCHK(dStdDev <= MAXDOUBLE / 10); // detect overflow
            ulStdDev.QuadPart = (ULONGLONG) (dStdDev * 10);  // *10 for decimal point later
            PerformanceCounterToMicroseconds(&ulStdDev, &ulStdDev);  // OK to convert in-place
            dStdDev = ((double) ulStdDev.QuadPart) / 10;     // /10 to return decimal point to correct place
            
            // Calculate average
            DEBUGCHK(ulTotal.QuadPart <= (0xffffffffffffffff) / 100); // detect overflow
            ulAverage.QuadPart = ulTotal.QuadPart * 10;  // *10 for decimal point later
            ulAverage.QuadPart /= pDL->dwNumVals;
            
            DEBUGCHK((ulMax.QuadPart >= (ulAverage.QuadPart / 10))
                     && (ulMin.QuadPart <= (ulAverage.QuadPart / 10)));  // sanity check
            DEBUGCHK((pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart)
                     && (ulMax.QuadPart >= ulMin.QuadPart));  // sanity check

            hResult = FlushChars(pFlush, 
                                 TEXT("\t\tPerfCount(us): Min=%I64u, Max=%I64u, Total=%I64u, Avg=%I64u.%u, StdDev=%.1lf\r\n"),
                                 ulMin.QuadPart, ulMax.QuadPart, ulTotal.QuadPart,
                                 ulAverage.QuadPart / 10,
                                 (DWORD)(ulAverage.QuadPart % 10),
                                 dStdDev);
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
    
            pCurCounter++;
        }
        
        if (pItem->dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT) {
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_SHARED) {
                // Special case - "shared" and "per-thread" are mutually exclusive,
                // so per-thread times are always N/A on shared durations.
                hResult = FlushChars(pFlush, TEXT("\t\tThreadTicks(ms): N/A on shared duration\r\n"));
            } else {
                ulAverage.QuadPart = pCurCounter->ulTotal.QuadPart;
                DEBUGCHK(ulAverage.QuadPart <= (0xffffffffffffffff) / 100); // detect overflow
                ulAverage.QuadPart *= 10;  // *10 for decimal point later
                ulAverage.QuadPart /= pDL->dwNumVals;
                
                DEBUGCHK((pCurCounter->ulMaxVal.QuadPart >= (ulAverage.QuadPart / 10))
                         && (pCurCounter->ulMinVal.QuadPart <= (ulAverage.QuadPart / 10)));  // sanity check
                DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check
                DEBUGCHK(pCurCounter->ulMaxVal.HighPart == 0);  // TickCount should only be using lower 32 bits
                DEBUGCHK(pCurCounter->ulMinVal.HighPart == 0);  // TickCount should only be using lower 32 bits
                
                dStdDev = GetStandardDeviation(pCurCounter, pDL->dwNumVals);

                hResult = FlushChars(pFlush,
                                     TEXT("\t\tThreadTicks(ms): Min=%I64u, Max=%I64u, Total=%I64u, Avg=%I64u.%u, StdDev=%.1lf\r\n"),
                                     pCurCounter->ulMinVal.QuadPart,
                                     pCurCounter->ulMaxVal.QuadPart,
                                     pCurCounter->ulTotal.QuadPart,
                                     ulAverage.QuadPart / 10,
                                     (DWORD)(ulAverage.QuadPart % 10),
                                     dStdDev);
            }
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
    
            pCurCounter++;
        }
        
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            ulAverage.QuadPart = pCurCounter->ulTotal.QuadPart;
            DEBUGCHK(ulAverage.QuadPart <= (0xffffffffffffffff) / 100); // detect overflow
            ulAverage.QuadPart *= 10;  // *10 for decimal point later
            ulAverage.QuadPart /= pDL->dwNumVals;
            
            DEBUGCHK((pCurCounter->ulMaxVal.QuadPart >= (ulAverage.QuadPart / 10))
                     && (pCurCounter->ulMinVal.QuadPart <= (ulAverage.QuadPart / 10)));  // sanity check
            DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check
            DEBUGCHK(pCurCounter->ulMaxVal.HighPart == 0);  // TickCount should only be using lower 32 bits
            DEBUGCHK(pCurCounter->ulMinVal.HighPart == 0);  // TickCount should only be using lower 32 bits
            
            dStdDev = GetStandardDeviation(pCurCounter, pDL->dwNumVals);

            hResult = FlushChars(pFlush,
                                 TEXT("\t\tTickCount(ms): Min=%I64u, Max=%I64u, Total=%I64u, Avg=%I64u.%u, StdDev=%.1lf\r\n"),
                                 pCurCounter->ulMinVal.QuadPart,
                                 pCurCounter->ulMaxVal.QuadPart,
                                 pCurCounter->ulTotal.QuadPart,
                                 ulAverage.QuadPart / 10,
                                 (DWORD)(ulAverage.QuadPart % 10),
                                 dStdDev);
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
    
            pCurCounter++;
        }
        
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR) {
            WORD  wCounter;
    
            // Dump DiscreteCounterData for each perf counter
            for (wCounter = 0;
                 (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
                 wCounter++) {

                ulAverage.QuadPart = pCurCounter->ulTotal.QuadPart;
                DEBUGCHK(ulAverage.QuadPart <= (0xffffffffffffffff) / 10); // detect overflow
                ulAverage.QuadPart *= 10;  // *10 for decimal point later
                ulAverage.QuadPart /= pDL->dwNumVals;
    
                DEBUGCHK((pCurCounter->ulMaxVal.QuadPart >= (ulAverage.QuadPart / 10))
                         && (pCurCounter->ulMinVal.QuadPart <= (ulAverage.QuadPart / 10)));  // sanity check
                DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check

                dStdDev = GetStandardDeviation(pCurCounter, pDL->dwNumVals);

                hResult = FlushChars(pFlush,
                                     TEXT("\t\tCPU%02u:     Min=%I64u, Max=%I64u, Total=%I64u, Avg=%I64u.%u, StdDev=%.1lf\r\n"),
                                     wCounter, pCurCounter->ulMinVal.QuadPart,
                                     pCurCounter->ulMaxVal.QuadPart,
                                     pCurCounter->ulTotal.QuadPart,
                                     ulAverage.QuadPart / 10,
                                     (DWORD)(ulAverage.QuadPart % 10),
                                     dStdDev);
                if (hResult != ERROR_SUCCESS) {
                    return hResult;
                }
        
                pCurCounter++;
            }
        }
    }
    
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static HRESULT FlushCounterCSV(
    FlushState* pFlush,
    const DiscreteCounterData *pCurCounter,
    DWORD dwNumVals
    )
{
    HRESULT hResult;

    if (pCurCounter) {
        // Counter is active -- write the data
        double dStdDev;

        DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check

        // Min, max, total
        hResult = FlushULargeCSV(pFlush, &(pCurCounter->ulMinVal));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        hResult = FlushULargeCSV(pFlush, &(pCurCounter->ulMaxVal));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        hResult = FlushULargeCSV(pFlush, &(pCurCounter->ulTotal));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        
        // Standard deviation
        dStdDev = GetStandardDeviation(pCurCounter, dwNumVals);
        hResult = FlushDoubleCSV(pFlush, dStdDev);
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        
        return ERROR_SUCCESS;
    
    }

    // Counter is not being recorded -- write empty data
    return FlushChars(pFlush, TEXT(",,,,"));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write (discrete) data from min-record Duration item into a comma-separated
// text stream.
static HRESULT FlushMinDataCSV(
    FlushState*  pFlush,
    TrackedItem* pItem
    )
{
    HRESULT hResult;
    WORD    wCounter;
    DurationDataList *pDL = (DurationDataList *)GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);
    
    // Write the session name first
    if (pFlush->pSession->szSessionName[0]) {
        hResult = FlushBytes(pFlush, (LPBYTE)(pFlush->pSession->szSessionName),
                             wcslen(pFlush->pSession->szSessionName)*sizeof(WCHAR));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }

    // Write basic data
    hResult = FlushChars(pFlush, TEXT(",%s,%u,%u"),
                         pItem->szItemName,
                         pDL->dwNumVals,
                         pItem->data.Duration.dwErrorCount);
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // Skip the rest if the Duration was never succesfully entered
    if (pDL->dwNumVals > 0) {
        DiscreteCounterData* pCurCounter;
    
        pCurCounter = DATALIST_PDATA(pDL);
    
        // Write each counter
    
        // Special case perfcount because it needs to be converted to microseconds
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            DiscreteCounterData  CounterUS;

            PerformanceCounterToMicroseconds(&pCurCounter->ulTotal, &CounterUS.ulTotal);
            PerformanceCounterToMicroseconds(&pCurCounter->ulMinVal, &CounterUS.ulMinVal);
            PerformanceCounterToMicroseconds(&pCurCounter->ulMaxVal, &CounterUS.ulMaxVal);
            
            // Convert the StdDev value.  "M" will not be used while "S" is basically the
            // square of the StdDev.  So convert to US by transforming S *twice* to square it.
            CounterUS.liM.QuadPart = 0;
            CounterUS.liS.QuadPart = pCurCounter->liS.QuadPart;
            PerformanceCounterToMicroseconds((ULARGE_INTEGER*) &CounterUS.liS, (ULARGE_INTEGER*) &CounterUS.liS);
            PerformanceCounterToMicroseconds((ULARGE_INTEGER*) &CounterUS.liS, (ULARGE_INTEGER*) &CounterUS.liS);

            hResult = FlushCounterCSV(pFlush, &CounterUS, pDL->dwNumVals);
            pCurCounter++;
        } else {
            hResult = FlushCounterCSV(pFlush, NULL, 0);
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        
        if (pItem->dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT) {
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_SHARED) {
                // Special case - "shared" and "per-thread" are mutually exclusive,
                // so per-thread times are always N/A on shared durations.
                hResult = FlushCounterCSV(pFlush, NULL, 0);
            } else {
                hResult = FlushCounterCSV(pFlush, pCurCounter, pDL->dwNumVals);
            }
            pCurCounter++;
        } else {
            hResult = FlushCounterCSV(pFlush, NULL, 0);
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            hResult = FlushCounterCSV(pFlush, pCurCounter, pDL->dwNumVals);
            pCurCounter++;
        } else {
            hResult = FlushCounterCSV(pFlush, NULL, 0);
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        
        // Dump each CPU perf counter
        for (wCounter = 0;
             (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
             wCounter++) {

            if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR) {
                hResult = FlushCounterCSV(pFlush, pCurCounter, pDL->dwNumVals);
                pCurCounter++;
            } else {
                hResult = FlushCounterCSV(pFlush, NULL, 0);
            }
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        }
    }
    
    hResult = FlushChars(pFlush, TEXT("\r\n"));
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllItems.  Write descriptor and discrete data
// to the storage location.  Clear the data if necessary.
HRESULT Walk_FlushDuration(
    HANDLE        hTrackedItem,
    TrackedItem*  pItem,
    LPVOID        lpWalkParam      // FlushState* pFlush
    )
{
    FlushState* pFlush = (FlushState*) lpWalkParam;
    HRESULT hResult = ERROR_SUCCESS;

    // Will be called with Statistic and LocalStatistic items too
    if (pItem->type == CEPERF_TYPE_DURATION) {
    
        // Write data and/or descriptors of discrete tracked items,
        // or descriptors of continuous tracked items, to the storage location.
        
        // Flush the descriptor first
        if (pFlush->dwFlushFlags & CEPERF_FLUSH_DESCRIPTORS) {
            if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                // Binary output
                hResult = FlushItemDescriptorBinary(pFlush, hTrackedItem, pItem, CEPERF_TYPE_DURATION);
            } else if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_TEXT) {
                // Text output
                hResult = FlushDescriptorText(pFlush, hTrackedItem, pItem);
            }
        }
        
        // Then flush the data
        if (pFlush->dwFlushFlags & CEPERF_FLUSH_DATA) {
            // Min-record mode is the only one that has data to flush
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_REGISTRY) {
                    hResult = FlushMinDataToRegistry(pFlush, pItem);
                } else if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
                    hResult = FlushMinDataCSV(pFlush, pItem);
                } else if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                    // Binary output
                    hResult = FlushMinDataBinary(pFlush, hTrackedItem, pItem);
                } else {
                    // Text output
                    hResult = FlushMinDataText(pFlush, hTrackedItem, pItem,
                                               (pFlush->dwFlushFlags & CEPERF_FLUSH_DESCRIPTORS) ? TRUE : FALSE);
                }
            }
        }
    
        // Flush and/or clear TTracker data if present
        if ((pFlush->dwFlushFlags & (CEPERF_FLUSH_DATA | CEPERF_FLUSH_AND_CLEAR))
            && (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_TTRACKER)) {
            // NOTE: If there are multiple TTracker items, we'll flush more than once
            HRESULT hTTResult = FlushTTracker(hTrackedItem, pItem,
                                              pFlush->dwFlushFlags & CEPERF_FLUSH_DATA,
                                              pFlush->dwFlushFlags & CEPERF_FLUSH_AND_CLEAR);
            
            // Only overwrite successful result
            if (SUCCEEDED(hResult)) {
                hResult = hTTResult;
            }
        }

        if (pFlush->dwFlushFlags & CEPERF_FLUSH_AND_CLEAR) {
            DurationDataList *pDL;

            ClearBeginList(pItem);
            pItem->data.Duration.dwErrorCount = 0;            
            pDL = (DurationDataList *) GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);
            
            // Min-record mode is the only one that has data to clear
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                WORD wCounter;
                DiscreteCounterData* pData;
                memset(pDL, 0, g_pMaster->list.wDurDLSize);
        
                // Set "min" values to -1
                pData = DATALIST_PDATA(pDL);
                for (wCounter = 0;
                     (wCounter < NUM_BUILTIN_COUNTERS + g_pMaster->CPU.wNumCounters)
                     && (wCounter < NUM_BUILTIN_COUNTERS + CEPERF_MAX_CPUCOUNTER_COUNT);
                     wCounter++) {
                    DEBUGCHK((LPBYTE)&pData[wCounter].ulMinVal.QuadPart < ((LPBYTE)pDL) + g_pMaster->list.wDurDLSize);
                    pData[wCounter].ulMinVal.HighPart = (DWORD)-1;
                    pData[wCounter].ulMinVal.LowPart  = (DWORD)-1;
                }
            }
        }
    }
    
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Returns the index a new entry for this thread in the Begin list, or 
// CEPERF_DURATION_FRAME_LIMIT if there's no room.  Parameters are somewhat
// redundant but allow for easy communication with ReleaseThreadBeginFrame.
static VOID ClaimNextThreadBeginFrame(
    TrackedItem*            pItem,
    DurationBeginCounters** ppCounters, // Will receive pointer to new counters
    DurationBeginList**     ppBL        // Will receive pointer to BL containing frame
    )
{
    DurationBeginCounters* pCounters;
    DurationBeginList* pBL;
    BYTE   bFrame, bNewFrame;
    HANDLE hThread;
    WORD   wTotalCounterSize = g_pMaster->CPU.wTotalCounterSize;  // Prevent asynchronous changes to g_pMaster

    *ppCounters = NULL;
    *ppBL = NULL;
    
    // Safety check size
    if (wTotalCounterSize > CEPERF_MAX_CPUCOUNTER_TOTAL) {
        return;
    }

    // ALGORITHM TO FIND OPEN SLOT DURING BEGIN:  Loop bFrameHigh --> 0.
    // Need to find first NULL slot after last entry for the current thread.
    // Can't just take the first NULL slot because it must be in proper
    // nested order with respect to other entries for this thread.
    // - If encountered an entry for this thread, take the first NULL
    //   slot after it.
    //   * Limited record mode:  If no such NULL slot is available, fail to
    //     record data.
    //   * Unlimited record mode:  If no such NULL slot is available, allocate
    //     a new DurationBeginList and chain on the head of the whole list.
    // - If no entry for this thread is encountered, take the first NULL
    //   slot.
    //   * Limited record mode:  If no such NULL slot is available, fail to
    //     record data.
    //   * Unlimited record mode:  If no NULL slot is available, walk to pNext
    //     and repeat this algorithm.  If hit an entry for this thread or hit
    //     pNext=NULL, allocate a new DurationBeginList and chain on the
    //     head of the whole list.

    // Un-shared and Shared Record Mode use the same algorithm by using
    // INVALID_HANDLE_VALUE for all threads in shared mode.
    if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_SHARED) {
        hThread = INVALID_HANDLE_VALUE;
    } else {
        hThread = (HANDLE) CePerf_GetCurrentThreadId();
    }
    
    pCounters = NULL;
    pBL = (DurationBeginList *) GET_MAP_POINTER(pItem->data.Duration.offsetBL);
    bNewFrame = CEPERF_DURATION_FRAME_LIMIT;
    
    DEBUGCHK(pBL);
    while (pBL) {
        bNewFrame = CEPERF_DURATION_FRAME_LIMIT;  // Frame to use for new data

        // Look for the first NULL (after the current thread) in the list
        bFrame = pBL->bFrameHigh+1;  // Start 1 too high
        DEBUGCHK(bFrame <= CEPERF_DURATION_FRAME_LIMIT);
        while (bFrame > 0) {
            bFrame--;
            
            pCounters = BEGINLIST_PCOUNTERS(pBL, bFrame, wTotalCounterSize);
            if (pCounters->hThread == NULL) {
                // Free
                bNewFrame = bFrame;
            } else if (pCounters->hThread == hThread) {
                // Found the current thread - stop looking
                if (bNewFrame < CEPERF_DURATION_FRAME_LIMIT) {
                    goto exit;  // Found a NULL frame to use
                }
                break;
            }
        }
        // Did not find the current thread

        if (bNewFrame < CEPERF_DURATION_FRAME_LIMIT) {
            // Found a NULL slot to use
            goto exit;
        } else if (pBL->bFrameHigh < CEPERF_DURATION_FRAME_LIMIT-1) {
            // There's room in the current list but we need to increase bFrameHigh
            pBL->bFrameHigh++;

            // Thread safety check -- BUGBUG is this enough?
            if (*((volatile WORD*)&(pBL->bFrameHigh)) > CEPERF_DURATION_FRAME_LIMIT-1) {
                pBL->bFrameHigh = CEPERF_DURATION_FRAME_LIMIT-1;
            }

            bNewFrame = pBL->bFrameHigh;
            goto exit;
        }
        
        // No free slots found
        if (!(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_UNLIMITED)) {
            pBL = (DurationBeginList *) GET_MAP_POINTER(0);  // Stop looping
        } else {
            if ((bFrame == 0) && (pBL->offsetNextDBL != 0)) {
                // Did not encounter an entry for this thread; walk to the next DBL and
                // keep looking
                pBL = (DurationBeginList *) GET_MAP_POINTER(pBL->offsetNextDBL);
            } else {
                // Found this thread, or no pNext; allocate a new
                // DurationBeginList and chain on the head of the whole list.
                pBL = (DurationBeginList*) AllocListItem(&g_DurBLListFuncs,
                                                         g_pMaster->list.wDurBLSize,
                                                         &(g_pMaster->list.offsetDurBLHead));
                if (GET_MAP_OFFSET(pBL) == 0) {
                    return;
                }

                // Initialize the data
                memset(pBL, 0, g_pMaster->list.wDurBLSize);

                DEBUGMSG(ZONE_ITEM && ZONE_MEMORY,
                         (TEXT("CePerf: New BeginList=0x%08x for pItem=0x%08x\r\n"),
                          pBL, pItem));

                // Chain on the head of the whole list
                pBL->offsetNextDBL = pItem->data.Duration.offsetBL;
                pItem->data.Duration.offsetBL = GET_MAP_OFFSET(pBL);

                // Use the first frame in the new BL
                bNewFrame = 0;
                goto exit;
            }
        }
    }
    
    // Unlimited mode shouldn't get here
    DEBUGCHK(!(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_UNLIMITED));
    
    // Failed to find a free slot - an error will be recorded
    
exit:
    // Unique: Without REPLACE_BEGIN, a second begin is a failure.
    // With REPLACE_BEGIN, a second begin replaces the first.
    if ((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_UNIQUE)
        && (bNewFrame > 0)) {

        DEBUGCHK(bNewFrame == 1); // Should not go above 1 frame

        if ((pItem->dwRecordingFlags & CEPERF_DURATION_REPLACE_BEGIN)
            // The thread ID must match
            && (BEGINLIST_PCOUNTERS(pBL, 0, wTotalCounterSize)->hThread == hThread)) {
            // Second begin should replace the first
            bNewFrame = 0;
        } else {
            // First begin takes precedence
            bNewFrame = CEPERF_DURATION_FRAME_LIMIT;
        }
    }

    if (bNewFrame < CEPERF_DURATION_FRAME_LIMIT) {
        // Found a frame -- claim it for this thread
        
        DEBUGMSG(ZONE_API && ZONE_DURATION && ZONE_MEMORY,
                 (TEXT("--> +BL 0x%08x frame %u  hThread=0x%08x\r\n"),
                  pBL, bNewFrame, hThread));

        // Assumes pBL and bNewFrame have all been set properly
        pCounters = BEGINLIST_PCOUNTERS(pBL, bNewFrame, wTotalCounterSize);

        // NOTENOTE not threadsafe.  The worst that could happen is that two
        // Duration enter events stomp each other.  If they end up mixing perf
        // counters, that's okay, because the counters will be pretty close to
        // correct since the two threads were running at about the same time.
        // One thread will overwrite the other's handle.  The "loser" will
        // record an error when it ends the Duration.

        // BUGBUG if the "loser" is making nested calls, it may mismatch 
        // begin/enter data if it loses a frame in this manner.
        pCounters->hThread = hThread;
    
        // Thread safety check -- BUGBUG is this enough?
        if (*((volatile DWORD*)&(pCounters->hThread)) == (DWORD)hThread) {
            *ppCounters = pCounters;
            *ppBL = pBL;
        } else {
            bNewFrame = CEPERF_DURATION_FRAME_LIMIT;
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Returns the index of the current thread's most recent entry in the Begin list,
// or CEPERF_DURATION_FRAME_LIMIT if not found.
static VOID FindPrevThreadBeginFrame(
    TrackedItem*            pItem,
    DurationBeginCounters** ppCounters, // Will receive pointer to new counters
    DurationBeginList**     ppBL        // Will receive pointer to BL containing frame
    )
{
    DurationBeginList* pBL;
    DurationBeginCounters* pCounters;
    BYTE   bFrame;
    HANDLE hThread;
    WORD   wTotalCounterSize = g_pMaster->CPU.wTotalCounterSize;  // Prevent asynchronous changes to g_pMaster
    
    *ppCounters = NULL;
    *ppBL = NULL;

    // Safety check size
    if (wTotalCounterSize > CEPERF_MAX_CPUCOUNTER_TOTAL) {
        return;
    }

    // ALGORITHM TO FIND MATCHING BEGIN SLOT DURING INTERMEDIATE/END:
    // Loop bFrameHigh --> 0.  Stop at first entry for the current thread.
    // * Limited record mode: if not found, record an error.
    // * Unlimited record mode: if not found, jump to pNext and keep
    //   looking.

    // Un-shared and Shared Record Mode use the same algorithm by using
    // INVALID_HANDLE_VALUE for all threads in shared mode.
    if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_SHARED) {
        hThread = INVALID_HANDLE_VALUE;
    } else {
        hThread = (HANDLE) CePerf_GetCurrentThreadId();
    }
    
    pBL = (DurationBeginList *) GET_MAP_POINTER(pItem->data.Duration.offsetBL);
    DEBUGCHK(GET_MAP_OFFSET(pBL));
    while (pBL) {
        bFrame = pBL->bFrameHigh+1;  // Start 1 too high
        DEBUGCHK(bFrame <= CEPERF_DURATION_FRAME_LIMIT);
        while (bFrame > 0) {
            bFrame--;
            
            pCounters = BEGINLIST_PCOUNTERS(pBL, bFrame, wTotalCounterSize);
            if (pCounters->hThread == hThread) {
                // Found the current thread
                DEBUGMSG(ZONE_API && ZONE_DURATION && ZONE_MEMORY,
                         (TEXT("-->  BL 0x%08x frame %u  hThread=0x%08x\r\n"),
                          pBL, bFrame, hThread));
                *ppCounters = pCounters;
                *ppBL = pBL;
                return;
            }

            // In shared mode, any non-null frame should be a match
            DEBUGCHK(!(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_SHARED)
                     || (pCounters->hThread == NULL));
        }
        
        // pNext must be null in limited record mode
        DEBUGCHK((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_UNLIMITED)
                 || (pBL->offsetNextDBL == 0));
        
        // Not found - keep looking for this thread in the list
        pBL = (DurationBeginList *) GET_MAP_POINTER(pBL->offsetNextDBL);
    }

    // Not found, an error will be recorded
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Release the Begin block, and free the BeginList if no other frames are used.
static VOID ReleaseThreadBeginFrame(
    TrackedItem*           pItem,
    DurationBeginCounters* pCounters,
    DurationBeginList*     pBL
    )
{
    DurationBeginList* pTemp;
    BYTE bFrame;
    WORD wTotalCounterSize = g_pMaster->CPU.wTotalCounterSize;  // Prevent asynchronous changes to g_pMaster

    // Safety check size
    if (wTotalCounterSize > CEPERF_MAX_CPUCOUNTER_TOTAL) {
        return;
    }

    // Release the frame by clearing the thread ID
    pCounters->hThread = NULL;

    // Unlimited Record mode: Check whether the BeginList needs to be freed
    if ((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_UNLIMITED)
        && (pBL == (DurationBeginList *)GET_MAP_POINTER(pItem->data.Duration.offsetBL))) {  // Check only for head of list
        
        // Never free last BL
        while (pBL && (pBL->offsetNextDBL !=0)) {
            DEBUGCHK(pBL->bFrameHigh < CEPERF_DURATION_FRAME_LIMIT);
            for (bFrame = 0; bFrame <= pBL->bFrameHigh; bFrame++) {
                pCounters = BEGINLIST_PCOUNTERS(pBL, bFrame, wTotalCounterSize);
                if (pCounters->hThread != NULL) {
                    // Frame is being used; pBL can't be freed yet
                    return;
                }
            }
            
            // All frames are free; pBL should be freed
            pTemp = pBL;
            pItem->data.Duration.offsetBL = pBL->offsetNextDBL;
            pBL = (DurationBeginList *)GET_MAP_POINTER(pBL->offsetNextDBL);  // Keep checking with next frame

            DEBUGMSG(ZONE_ITEM && ZONE_MEMORY,
                     (TEXT("CePerf: Free BeginList=0x%08x for pItem=0x%08x\r\n"),
                      pTemp, pItem));
            
            // Now free pTemp
            FreeListItem(&g_DurBLListFuncs, g_pMaster->list.wDurBLSize,
                         &(g_pMaster->list.offsetDurBLHead),
                         (LPBYTE)pTemp, FALSE);  // Skip page free, for perf reasons
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Free all BeginList items and clear all data.
static VOID ClearBeginList(
    TrackedItem* pItem
    )
{
    DurationBeginList* pBL = (DurationBeginList *)GET_MAP_POINTER(pItem->data.Duration.offsetBL);
    DurationBeginList* pTemp;

    // Unlimited Record mode: Free all but the last BeginList
    if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_UNLIMITED) {
        // Never free last BL
        while (pBL && (pBL->offsetNextDBL != 0)) {
            pTemp = pBL;
            pItem->data.Duration.offsetBL = pBL->offsetNextDBL;
            pBL = (DurationBeginList *)GET_MAP_POINTER(pBL->offsetNextDBL);  // Keep checking with next frame
    
            DEBUGMSG(ZONE_ITEM && ZONE_MEMORY,
                     (TEXT("CePerf: Free BeginList=0x%08x for pItem=0x%08x\r\n"),
                      pTemp, pItem));
    
            // Now free pTemp
            FreeListItem(&g_DurBLListFuncs, g_pMaster->list.wDurBLSize,
                         &(g_pMaster->list.offsetDurBLHead),
                         (LPBYTE)pTemp, FALSE);  // Skip page free, for perf reasons
        }
    }

    // Clear the last BeginList
    if (pBL) {
        memset(pBL, 0, sizeof(DurationBeginList));
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// RECORD_FULL: Writes continous data to the output stream
static HRESULT RecordCeLogFullData(
    SessionHeader* pSession,
    HANDLE         hTrackedItem,
    TrackedItem*   pItem,
    LPBYTE         pCPUCounters,   // Data blob with current CPU perf counters
    HRESULT        hResult,        // Did the Begin/Intermediate/End succeed?
    CEPERF_LOGID   id              // Is it a begin/intermediate/end?
    )
{
    FlushState flush;

    // Convenient declaration of the data that will be flushed
#pragma pack(push,4)
    struct {
        CEPERF_LOGID id;
        CEPERF_LOG_DURATION_FULL duration;
        // Followed by the CPU performance counters
        BYTE CounterBuf[CEPERF_MAX_CPUCOUNTER_COUNT*sizeof(ULARGE_INTEGER)];
    } Data;
#pragma pack(pop)
    DWORD cbData;

    DEBUGCHK(pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG);
    DEBUGCHK(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_FULL);
    
    Data.id = id;
    Data.duration.dwItemID = (DWORD) hTrackedItem;
    Data.duration.hResult = hResult;
    cbData = sizeof(CEPERF_LOGID) + sizeof(CEPERF_LOG_DURATION_FULL);

    // Copy the CPU performance counters into the buffer
    if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
        && (g_pMaster->CPU.wTotalCounterSize < CEPERF_MAX_CPUCOUNTER_COUNT*sizeof(ULARGE_INTEGER))) {
        memcpy(Data.CounterBuf, pCPUCounters, g_pMaster->CPU.wTotalCounterSize);
        cbData += g_pMaster->CPU.wTotalCounterSize;
    }

    // Write the buffer to the output location
    flush.pSession = pSession;
    flush.hOutput = INVALID_HANDLE_VALUE;
    flush.lpTempBuffer = NULL;
    flush.cbTempBuffer = 0;
    flush.dwFlushFlags = 0;
    return FlushBytes(&flush, (LPBYTE) &Data, cbData);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Get the current counters and store the data
//   RECORD_MIN: Does nothing but fill in lpData
//   RECORD_SHORT & RECORD_FULL: Writes continous data to the output stream
static BOOL RecordIntermediateData(
    SessionHeader* pSession,
    HANDLE         hTrackedItem,
    TrackedItem*   pItem,
    DurationBeginCounters* pCounters,   // Perf counters from matching Begin
    DWORD          dwErrorCode,         // Treated as an error if non-zero
    CEPERF_ITEM_DATA* lpData            // Buffer with current counter values,
                                        // will receive counter deltas
    )
{
    DEBUGCHK(!(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_NONE));
    
    if (pCounters) {
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            // Calculate the delta between Begin and End value
            lpData->Duration.liPerfCount.QuadPart -= pCounters->liPerfCount.QuadPart;
            DEBUGCHK(lpData->Duration.liPerfCount.HighPart == 0);  // No underflow
        }

        if (pItem->dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT) {
            // Calculate the delta between Begin and End value
            lpData->Duration.liThreadTicks.QuadPart -= pCounters->liThreadTicks.QuadPart;
            DEBUGCHK(lpData->Duration.liThreadTicks.HighPart == 0);  // No underflow
        }

        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            // Calculate the delta between Begin and End value
            DEBUGCHK((lpData->Duration.dwTickCount >= pCounters->dwTickCount)
                     || (((int)pCounters->dwTickCount < 0) && ((int)lpData->Duration.dwTickCount > 0)));  // GetTickCount will roll over on debug builds sometimes
            lpData->Duration.dwTickCount -= pCounters->dwTickCount;
        }

        if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
            && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {

            LPBYTE lpBeginData, lpEndData;
            WORD   wCounter;

            lpBeginData = BEGINCOUNTERS_PCPU(pCounters);
            lpEndData   = ((LPBYTE)lpData) + sizeof(CEPERF_ITEM_DATA);
            for (wCounter = 0;
                 (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
                 wCounter++) {

                // Buffer overflow checks
                DEBUGCHK(lpBeginData + g_pMaster->CPU.rgbCounterSize[wCounter] <= BEGINCOUNTERS_PCPU(pCounters) + g_pMaster->CPU.wTotalCounterSize);
                DEBUGCHK(lpEndData   + g_pMaster->CPU.rgbCounterSize[wCounter] <= ((LPBYTE)lpData) + lpData->wSize);
                if ((((LPBYTE)lpData) + lpData->wSize) - lpEndData <= g_pMaster->CPU.rgbCounterSize[wCounter]) {
                    
                    // Calculate the delta between Begin and End value and store in lpData
                    switch (g_pMaster->CPU.rgbCounterSize[wCounter]) {
                    case sizeof(BYTE):
                        *((BYTE*)lpEndData) -= *((BYTE*)lpBeginData);
                        break;
    
                    case sizeof(WORD):
                        *((WORD*)lpEndData) -= *((WORD*)lpBeginData);
                        break;
    
                    case sizeof(DWORD):
                        *((DWORD*)lpEndData) -= *((DWORD*)lpBeginData);
                        break;
    
                    case sizeof(ULARGE_INTEGER):
                        (((ULARGE_INTEGER*)lpEndData)->QuadPart -= ((ULARGE_INTEGER*)lpBeginData)->QuadPart);
                        break;
    
                    default:
                        // Cannot manage this size
                        DEBUGCHK(0);
                        break;
                    }
                }

                lpBeginData += g_pMaster->CPU.rgbCounterSize[wCounter];
                lpEndData   += g_pMaster->CPU.rgbCounterSize[wCounter];
            }
        }
    }

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Get the current counters and store the data
//   RECORD_MIN: Records data in private RAM
//   RECORD_SHORT & RECORD_FULL: Writes continous data to the output stream
static BOOL RecordEndData(
    SessionHeader* pSession,
    HANDLE         hTrackedItem,
    TrackedItem*   pItem,
    DurationBeginCounters* pCounters,   // Perf counters from matching Begin
    DWORD          dwErrorCode,         // Treated as an error if non-zero
    CEPERF_ITEM_DATA* lpData,           // Buffer with current counter values,
                                        // will receive counter deltas
    const BYTE*    lpInfoBuf,           // Buffer of additional information to log, or NULL
    DWORD          dwInfoBufSize        // Size of information buffer, in bytes
    )
{
    DurationDataList *pDL = (DurationDataList *)GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);
    DiscreteCounterData* pMinData = DATALIST_PDATA(pDL);

    DEBUGCHK(!(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_NONE));
    
    // Check for erroneous data before recording anything
    if (!dwErrorCode && pCounters) {
        // Track if the high performance counter ever went backwards
        if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT)
            && (lpData->Duration.liPerfCount.QuadPart < pCounters->liPerfCount.QuadPart)) {
            dwErrorCode = (DWORD) CEPERF_HR_BAD_PERFCOUNT_DATA;
        }

        // Track if the thread run-time tick counter ever went backwards
        if ((pItem->dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT)
            && (lpData->Duration.liThreadTicks.QuadPart < pCounters->liThreadTicks.QuadPart)) {
            dwErrorCode = (DWORD) CEPERF_HR_BAD_PERFCOUNT_DATA;
        }

        // Track if the tick count ever went backwards
        if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT)
            && (lpData->Duration.dwTickCount < pCounters->dwTickCount)
            // GetTickCount rollover is OK and happens on debug builds
            && !(((int)pCounters->dwTickCount < 0) && ((int)lpData->Duration.dwTickCount > 0))) {
            dwErrorCode = (DWORD) CEPERF_HR_BAD_PERFCOUNT_DATA;
        }

        // NOTENOTE not checking for erroneous CPU perfctr data right now
    }

    // Record error
    if (dwErrorCode != 0) {
        pItem->data.Duration.dwErrorCount++;
    } else {
        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
            pDL->dwNumVals++;
        }
    }
    
    if (pCounters) {
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            // Calculate the delta between Begin and End value
            lpData->Duration.liPerfCount.QuadPart -= pCounters->liPerfCount.QuadPart;

            // Record the data
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                if (dwErrorCode == 0) {
                    UpdateDurationDataUL(pMinData, (ULARGE_INTEGER*)&lpData->Duration.liPerfCount,
                                         pDL->dwNumVals);
                }
                pMinData++;
            }
        }

        if (pItem->dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT) {
            // Calculate the delta between Begin and End value
            lpData->Duration.liThreadTicks.QuadPart -= pCounters->liThreadTicks.QuadPart;

            // Record the data
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                if (dwErrorCode == 0) {
                    UpdateDurationDataUL(pMinData, (ULARGE_INTEGER*)&lpData->Duration.liThreadTicks,
                                         pDL->dwNumVals);
                }
                pMinData++;
            }
        }

        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            // Calculate the delta between Begin and End value
            lpData->Duration.dwTickCount -= pCounters->dwTickCount;

            // Record the data
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                if (dwErrorCode == 0) {
                    UpdateDurationDataDW(pMinData, lpData->Duration.dwTickCount,
                                         pDL->dwNumVals);
                } 
                pMinData++;
            }
        }

        if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
            && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {

            LPBYTE         lpBeginData, lpEndData;
            WORD           wCounter;

            lpBeginData = BEGINCOUNTERS_PCPU(pCounters);
            lpEndData   = ((LPBYTE)lpData) + sizeof(CEPERF_ITEM_DATA);
            for (wCounter = 0;
                 (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
                 wCounter++) {

                // Buffer overflow checks
                DEBUGCHK(lpBeginData + g_pMaster->CPU.rgbCounterSize[wCounter] <= BEGINCOUNTERS_PCPU(pCounters) + g_pMaster->CPU.wTotalCounterSize);
                DEBUGCHK(lpEndData   + g_pMaster->CPU.rgbCounterSize[wCounter] <= ((LPBYTE)lpData) + lpData->wSize);
                if ((((LPBYTE)lpData) + lpData->wSize) - lpEndData <= g_pMaster->CPU.rgbCounterSize[wCounter]) {

                    // Calculate the delta between Begin and End value
                    switch (g_pMaster->CPU.rgbCounterSize[wCounter]) {
                    case sizeof(BYTE):
                        *((BYTE*)lpEndData) -= *((BYTE*)lpBeginData);
                        // Record the data
                        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                            if (dwErrorCode == 0) {
                                UpdateDurationDataDW(pMinData, *((BYTE*)lpEndData), pDL->dwNumVals);
                            } 
                            pMinData++;                            
                        }
                        break;
    
                    case sizeof(WORD):
                        *((WORD*)lpEndData) -= *((WORD*)lpBeginData);
                        // Record the data
                        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                            if (dwErrorCode == 0) {
                                UpdateDurationDataDW(pMinData, *((WORD*)lpEndData), pDL->dwNumVals);
                            } 
                            pMinData++;
                        }
                        break;
    
                    case sizeof(DWORD):
                        *((DWORD*)lpEndData) -= *((DWORD*)lpBeginData);
                        // Record the data
                        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                            if (dwErrorCode == 0) {
                                UpdateDurationDataDW(pMinData, *((DWORD*)lpEndData), pDL->dwNumVals);
                            }
                            pMinData++;                            
                        }
                        break;
    
                    case sizeof(ULARGE_INTEGER):
                        (((ULARGE_INTEGER*)lpEndData)->QuadPart -= ((ULARGE_INTEGER*)lpBeginData)->QuadPart);
                        // Record the data
                        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                            if (dwErrorCode == 0) {
                                UpdateDurationDataUL(pMinData, (ULARGE_INTEGER*)lpEndData, pDL->dwNumVals);
                            }
                            pMinData++;
                        }
                        break;
    
                    default:
                        // Cannot manage this size
                        DEBUGCHK(0);
                        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                            pMinData++;
                        }
                        break;
                    }
                }

                lpBeginData += g_pMaster->CPU.rgbCounterSize[wCounter];
                lpEndData   += g_pMaster->CPU.rgbCounterSize[wCounter];
            }
        }
    }
    
    return TRUE;
}


//------------------------------------------------------------------------------
// CePerfBeginDuration - Mark the beginning of the period of interest
//------------------------------------------------------------------------------
HRESULT CePerf_BeginDuration(
    HANDLE hTrackedItem         // Handle of duration item being entered
    )
{
    DWORD          LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT        hResult = ERROR_SUCCESS;
    TrackedItem*   pItem;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_DURATION,
             (TEXT("+CePerfBeginDuration(0x%08x)\r\n"),
              hTrackedItem));

    if (ITEM_PROLOG(&pSession, &pItem, hTrackedItem, CEPERF_TYPE_DURATION, &hResult)) {
            
        // Values to be filled in by ClaimNextThreadBeginFrame
        DurationBeginCounters* pCounters;
        DurationBeginList* pBL;

        // Allocate a frame to store data in
        ClaimNextThreadBeginFrame(pItem, &pCounters, &pBL);
        if (pCounters) {
            // Record the current counters in the BeginList frame.  Do this last to
            // make the counters as accurate as possible, and to avoid copying.
            hResult = GetCurrentPerfCounters(pItem->dwRecordingFlags,
                                             &(pCounters->liPerfCount),
                                             &(pCounters->liThreadTicks),
                                             &(pCounters->dwTickCount),
                                             BEGINCOUNTERS_PCPU(pCounters));
            // Starting TTracker is an extra option
            if ((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_TTRACKER)
                && (hResult == ERROR_SUCCESS)) {
                hResult = StartStopTTracker(pItem, TRUE);
            }
            // Record to CeLog if necessary
            if ((pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG)
                && (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_FULL)) {
                hResult = RecordCeLogFullData(pSession, hTrackedItem, pItem,
                                              BEGINCOUNTERS_PCPU(pCounters),
                                              hResult, CEPERF_LOGID_DURATION_BEGIN);
            }

            if (hResult != ERROR_SUCCESS) {
                // Release the matching Begin block, and free the BL if
                // no other frames are used
                ReleaseThreadBeginFrame(pItem, pCounters, pBL);
                pCounters = NULL;
            }
        } else {
            // No open slot: either a leak, too many concurrent threads, or
            // nesting that is too deep.
            hResult = CEPERF_HR_NOT_ENOUGH_MEMORY;
        }

        ITEM_EPILOG(pSession);
    }

    DEBUGMSG(ZONE_API && ZONE_DURATION,
             (TEXT("-CePerfBeginDuration (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}


#define MAX_DURATION_DATABUF_SIZE (CEPERF_MAX_CPUCOUNTER_TOTAL + sizeof(CEPERF_ITEM_DATA))

//------------------------------------------------------------------------------
// CePerfIntermediateDuration - Mark an intermediate point during the period of
// interest
//------------------------------------------------------------------------------
HRESULT CePerf_IntermediateDuration(
    HANDLE hTrackedItem,        // Handle of duration item being marked
    CEPERF_ITEM_DATA* lpData    // Buffer to receive data changes since the
                                // the beginning duration, or NULL.
                                // wVersion and wSize must be set in the struct.
    )
{
    DWORD          LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT        hResult = ERROR_SUCCESS;
    TrackedItem*   pItem;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_DURATION,
             (TEXT("+CePerfIntermediateDuration(0x%08x)\r\n"),
              hTrackedItem));

    if (ITEM_PROLOG(&pSession, &pItem, hTrackedItem, CEPERF_TYPE_DURATION, &hResult)) {
            
        WORD wBufferSize;
        // Values to be filled in by FindPrevThreadBeginFrame
        DurationBeginCounters* pCounters;
        DurationBeginList* pBL;
        BYTE DataBuf[MAX_DURATION_DATABUF_SIZE];  // Stack buffer because LocalAlloc is too high overhead

        // Calculate required data buffer size
        wBufferSize = sizeof(CEPERF_ITEM_DATA);
        if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
            && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {
            wBufferSize += g_pMaster->CPU.wTotalCounterSize;
        }
        if (wBufferSize > MAX_DURATION_DATABUF_SIZE) {
            hResult = CEPERF_HR_NOT_ENOUGH_MEMORY;
            goto exit;
        }

        // Use lpData to pass around current perf counter values if provided.
        // That lets us return data to the caller at the same time we record it.
        if (lpData == NULL) {
            lpData = (CEPERF_ITEM_DATA*) DataBuf;
            lpData->wVersion = 1;
            lpData->wSize = wBufferSize;
        } else {
            // Validate struct version and size
            if (lpData->wVersion != 1) {
                hResult = CEPERF_HR_INVALID_PARAMETER;
                goto exit;
            } else if (lpData->wSize < wBufferSize) {
                hResult = CEPERF_HR_NOT_ENOUGH_MEMORY;
                goto exit;
            }
        }

        // Query the current counters before doing anything else, to make them
        // as accurate as possible.
        hResult = GetCurrentPerfCounters(pItem->dwRecordingFlags,
                                         &lpData->Duration.liPerfCount,
                                         &lpData->Duration.liThreadTicks,
                                         &lpData->Duration.dwTickCount,
                                         ((LPBYTE)lpData) + sizeof(CEPERF_ITEM_DATA));

        // Use hResult to know if a counter query failed, but keep going for now.
        // If there was a failure, we must still clean up the corresponding Begin
        // block before exiting.

        // Find the data that was recorded by the Duration Begin call
        FindPrevThreadBeginFrame(pItem, &pCounters, &pBL);
        if (!pCounters) {
            // Thread not found in Begin list
            hResult = CEPERF_HR_BEGIN_NOT_FOUND;
        }
        // Record to CeLog if necessary
        if ((pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG)
            && (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_FULL)) {
            hResult = RecordCeLogFullData(pSession, hTrackedItem, pItem,
                                          ((LPBYTE)lpData) + sizeof(CEPERF_ITEM_DATA),
                                          hResult, CEPERF_LOGID_DURATION_INTERMEDIATE);
        }

        // Record the diff in the counters from Begin to Intermediate, write to
        // storage, and put the result into lpData
        RecordIntermediateData(pSession, hTrackedItem, pItem, pCounters,
                               hResult, lpData);

    exit:
        ITEM_EPILOG(pSession);
    }
    
    DEBUGMSG(ZONE_API && ZONE_DURATION,
             (TEXT("-CePerfIntermediateDuration (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}


//------------------------------------------------------------------------------
// CePerfEndDurationWithInformation - Mark the end of the period of interest, 
// with or without an error, and attach additional information.
// If a non-zero error code is passed:
//   * If the duration is in minimum record mode, the period that is ending
//     will be regarded as an error and its data (time change, CPU perf counter
//     changes, etc.) will not be recorded.
//   * If the duration is in short or full record mode, the error code will be
//     logged with the exit event in the data stream.
// If a buffer of additional information is passed:
//   * If the duration is in minimum record mode, the additional information
//     will not be recorded.
//   * If the duration is in short or maximum record mode, the end event and 
//     information will be added to the data stream.
//------------------------------------------------------------------------------
HRESULT CePerf_EndDurationWithInformation(
    HANDLE hTrackedItem,        // Handle of duration item being exited
    DWORD  dwErrorCode,         // Error code for the aborted duration, or 0 for no error
    const BYTE* lpInfoBuf,      // Buffer of additional information to log, or NULL
    DWORD  dwInfoBufSize,       // Size of information buffer, in bytes
    CEPERF_ITEM_DATA* lpData    // Buffer to receive data changes since the
                                // the beginning duration, or NULL.
                                // wVersion and wSize must be set in the struct.
    )
{
    DWORD          LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT        hResult = ERROR_SUCCESS;
    TrackedItem*   pItem;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_DURATION,
             (TEXT("+CePerfEndDurationWithInformation(0x%08x, 0x%08x)\r\n"),
              hTrackedItem, dwErrorCode));
    
    if (ITEM_PROLOG(&pSession, &pItem, hTrackedItem, CEPERF_TYPE_DURATION, &hResult)) {
            
        WORD wBufferSize;
        // Values to be filled in by FindPrevThreadBeginFrame
        DurationBeginCounters* pCounters;
        DurationBeginList* pBL;
        BYTE DataBuf[MAX_DURATION_DATABUF_SIZE];  // Stack buffer because LocalAlloc is too high overhead
    
        // Calculate required data buffer size
        wBufferSize = sizeof(CEPERF_ITEM_DATA);
        if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
            && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {
            wBufferSize += g_pMaster->CPU.wTotalCounterSize;
        }
        if (wBufferSize > MAX_DURATION_DATABUF_SIZE) {
            hResult = CEPERF_HR_NOT_ENOUGH_MEMORY;
            goto exit;
        }

        // Use lpData to pass around current perf counter values if provided.
        // That lets us return data to the caller at the same time we record it.
        if (lpData == NULL) {
            lpData = (CEPERF_ITEM_DATA*) DataBuf;
            lpData->wVersion = 1;
            lpData->wSize = wBufferSize;
        } else {
            // Validate struct version and size
            if (lpData->wVersion != 1) {
                hResult = CEPERF_HR_INVALID_PARAMETER;
                goto exit;
            } else if (lpData->wSize < wBufferSize) {
                hResult = CEPERF_HR_NOT_ENOUGH_MEMORY;
                goto exit;
            }
        }

        // Query the current counters before doing anything else, to make them
        // as accurate as possible.
        hResult = GetCurrentPerfCounters(pItem->dwRecordingFlags,
                                         &lpData->Duration.liPerfCount,
                                         &lpData->Duration.liThreadTicks,
                                         &lpData->Duration.dwTickCount,
                                         ((LPBYTE)lpData) + sizeof(CEPERF_ITEM_DATA));
        
        // Use hResult to know if a counter query failed, but keep going for now.
        // If there was a failure, we must still clean up the corresponding Begin
        // block before exiting.

        // Find the data that was recorded by the Duration Begin call
        FindPrevThreadBeginFrame(pItem, &pCounters, &pBL);
        if (!pCounters) {
            // Thread not found in Begin list
            hResult = CEPERF_HR_BEGIN_NOT_FOUND;
        } else {
            // Only stop TTracker if the begin frame was found
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_TTRACKER) {
                hResult = StartStopTTracker(pItem, FALSE);
            }
        }

        // Record to CeLog if necessary
        if ((pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG)
            && (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_FULL)) {
            hResult = RecordCeLogFullData(pSession, hTrackedItem, pItem,
                                          ((LPBYTE)lpData) + sizeof(CEPERF_ITEM_DATA),
                                          hResult, CEPERF_LOGID_DURATION_END);
        }
        
        // Record the diff in the counters from Begin to End, write to storage,
        // and put the result into lpData
        RecordEndData(pSession, hTrackedItem, pItem, pCounters,
                      dwErrorCode ? dwErrorCode : hResult, lpData,
                      lpInfoBuf, dwInfoBufSize);

        // Release the matching Begin block, and free the BL if no other
        // frames are used
        if (pCounters) {
            ReleaseThreadBeginFrame(pItem, pCounters, pBL);
        }
    
    exit:
        ITEM_EPILOG(pSession);
    }

    DEBUGMSG(ZONE_API && ZONE_DURATION,
             (TEXT("-CePerfEndDurationWithInformation (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}
