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
//      duration.c
//  
//  Abstract:  
//
//      Implements duration objects for the Windows CE
//      Performance Measurement API.
//      
//------------------------------------------------------------------------------

#ifndef CEPERF_ENABLE
#define CEPERF_ENABLE
#endif

#include <windows.h>
#include "ceperf.h"
#include "ceperf_log.h"
#include "ceperf_cpu.h"
#include "pceperf.h"


// GLOBALS

// Frequency of device performance counter (from QueryPerformanceCounter)
static LARGE_INTEGER g_liPerfFreq;

// This global is only valid during session flush
static FlushGlobals g_Flush;


static VOID ClearBeginList(TrackedItem* pItem);
static HRESULT BeginFlushCSV();


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
        if (!QueryPerformanceFrequency(&g_liPerfFreq)) {
            pulMSCount->QuadPart = 0;
            return;
        }
    }

    iVal = (__int64)pulPerfCount->QuadPart;
    iVal *= 1000000;
    iVal /= g_liPerfFreq.QuadPart;

    pulMSCount->QuadPart = iVal;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
_inline static HRESULT GetCurrentPerfCounters(
    DWORD          dwRecordingFlags,    // Used to figure out which perf counters to get
    LARGE_INTEGER* pliPerfCount,        // Receives perf counter timestamp
    DWORD*         pdwTickCount,        // Receives tick count timestamp
    LPBYTE         pCPUPerfCounters     // Receives CPU performance counters, size g_pMaster->CPU.wTotalCounterSize
    )
{
    if (dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
        if (!QueryPerformanceCounter(pliPerfCount)) {
            return CEPERF_HR_BAD_PERFCOUNT_DATA;
        }
    }
    
    if (dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
        *pdwTickCount = GetTickCount();
    }
    
    if ((dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
        && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {
        return pCPUQueryPerfCounters(pCPUPerfCounters, g_pMaster->CPU.wTotalCounterSize);
    }

    return ERROR_SUCCESS;
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
    TrackedItem* pItem,
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt  // Will not be non-NULL on creation;
                                            // will be NULL on recreation
    )
{
    DurationDataList *pDL = NULL;
    DurationBeginList *pBL = NULL;
    DEBUGCHK(pItem->type == CEPERF_TYPE_DURATION);
    
    pItem->data.Duration.dwErrorCount = 0;

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
        CEPERF_DISCRETE_COUNTER_DATA* pData;
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
             (wCounter < 2+g_pMaster->CPU.wNumCounters) && (wCounter < 2+CEPERF_MAX_CPUCOUNTER_COUNT);
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
    
    pBL = (DurationBeginList *)GET_MAP_POINTER(pItem->data.Duration.offsetBL);
    // Free private RAM for the begin list.  Walk each item in the chain.
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
    SessionHeader* pSession,
    DWORD          dwFlushFlags,
    HANDLE         hOutput          // hKey or hFile or INVALID_HANDLE_VALUE,
                                    // depending on output storage location
    )
{
    DEBUGCHK(!g_Flush.pSession && !g_Flush.dwFlushFlags && !g_Flush.hOutput
             && !g_Flush.lpTempBuffer && !g_Flush.dwTempBufferSize);

    // If writing to RAM, just use the RAM buffer; otherwise alloc a temp buffer
    if (pSession->settings.dwStorageFlags & CEPERF_STORE_RAM) {
        DEBUGCHK(0);  // RAM buffer NYI
        g_Flush.lpTempBuffer = NULL;
        g_Flush.dwTempBufferSize = 0;
    } else {
        // Calculate maximum possible buffer size needed
        g_Flush.dwTempBufferSize = sizeof(CEPERF_LOGDATA_DURATION_DISCRETE)
                                   + 2*sizeof(CEPERF_DISCRETE_COUNTER_DATA);
        if (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED) {
            g_Flush.dwTempBufferSize += g_pMaster->CPU.wNumCounters*sizeof(CEPERF_DISCRETE_COUNTER_DATA);
        }

        g_Flush.lpTempBuffer = LocalAlloc(LMEM_FIXED, g_Flush.dwTempBufferSize);
        if (!g_Flush.lpTempBuffer) {
            HRESULT hResult;
            
            g_Flush.dwTempBufferSize = 0;
            hResult = GetLastError();
            hResult = HRESULT_FROM_WIN32(hResult);
            return hResult;
        }
    }

    g_Flush.pSession     = pSession;
    g_Flush.dwFlushFlags = dwFlushFlags;
    g_Flush.hOutput      = hOutput;

    if (pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
        return BeginFlushCSV();
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Clean up flush resources
VOID DurationFlushEnd()
{
    DEBUGCHK(g_Flush.pSession && g_Flush.hOutput && g_Flush.lpTempBuffer
             && g_Flush.dwTempBufferSize);

    if (g_Flush.pSession
        && !(g_Flush.pSession->settings.dwStorageFlags & CEPERF_STORE_RAM)
        && g_Flush.lpTempBuffer) {
        LocalFree(g_Flush.lpTempBuffer);
    }
    
    g_Flush.lpTempBuffer     = NULL;
    g_Flush.dwTempBufferSize = 0;
    g_Flush.pSession         = NULL;
    g_Flush.dwFlushFlags     = 0;
    g_Flush.hOutput          = 0;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static HRESULT BeginFlushCSV()
{
    HRESULT hResult = ERROR_SUCCESS;
    WORD    wCounter;
    
    // Print CSV header to the output file.
    
    // Don't print it if there are no Duration objects in this session...  All
    // Durations have a BL so pDurBLHead is a good way to tell
    if (g_pMaster->list.offsetDurBLHead != 0) {

        hResult = FlushChars(&g_Flush, 
                             TEXT("\r\nSession Name,")
                             TEXT("Item Name,")
                             TEXT("Number of Successful Begin/End Pairs,")
                             TEXT("Number of Errors,")

                             TEXT("Min Time Delta (us/QPC),")
                             TEXT("Max Time Delta (us/QPC),")
                             TEXT("Total Time Delta (us/QPC),")

                             TEXT("Min Time Delta (ms/GTC),")
                             TEXT("Max Time Delta (ms/GTC),")
                             TEXT("Total Time Delta (ms/GTC)")
                             );
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }

        // Print value names -- won't overflow the buffer
        for (wCounter = 0;
             (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
             wCounter++) {
            hResult = FlushChars(&g_Flush,
                                 TEXT(",Min CPU%02u Delta,")
                                 TEXT("Max CPU%02u Delta,")
                                 TEXT("Total CPU%02u Delta"),
                                 wCounter, wCounter, wCounter);
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        }

        hResult = FlushChars(&g_Flush, TEXT("\r\n"));
    }
    
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write descriptor for Duration item into a binary stream.
static HRESULT FlushDescriptorBinary(
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    struct {
        CEPERF_LOGDATA_ITEM_DESCRIPTOR item;
        WCHAR szItemName[CEPERF_MAX_ITEM_NAME_LEN];
    } buf;
    DWORD dwLen;

    // BUGBUG code duplication between Duration and Statistic code
    
    buf.item.header.bID = CEPERF_LOGID_ITEM_DESCRIPTOR;
    buf.item.header.bReserved = 0;
    buf.item.header.wSize = sizeof(CEPERF_LOGDATA_ITEM_DESCRIPTOR);
    buf.item.hTrackedItem = hTrackedItem;
    buf.item.type = CEPERF_TYPE_DURATION;
    buf.item.dwRecordingFlags = pItem->dwRecordingFlags;
    
    if (SUCCEEDED(StringCchLength(pItem->szItemName, CEPERF_MAX_ITEM_NAME_LEN, &dwLen))) {
        StringCchCopyN(buf.szItemName, CEPERF_MAX_ITEM_NAME_LEN, pItem->szItemName, dwLen);
        buf.item.header.wSize += (WORD) ((dwLen + 1) * sizeof(WCHAR));
    }

    // Write the buffer to the output location
    return FlushBytes(&g_Flush, (LPBYTE) &buf.item, buf.item.header.wSize);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write descriptor for Duration item into a text stream.
_inline static HRESULT FlushDescriptorText(
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    return FlushChars(&g_Flush,
                      TEXT("\thDuration= 0x%08x, dwRecordingFlags=0x%08x, %s\r\n"),
                      (DWORD)hTrackedItem, pItem->dwRecordingFlags, pItem->szItemName);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write (discrete) data from min-record Duration item into the registry
static HRESULT FlushMinDataToRegistry(
    TrackedItem* pItem
    )
{
    LONG lResult;
    DurationDataList *pDL = (DurationDataList *)GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);
    CEPERF_DISCRETE_COUNTER_DATA* pCurCounter;
    HKEY hkDuration = INVALID_HANDLE_VALUE;

    DEBUGCHK(g_Flush.hOutput != INVALID_HANDLE_VALUE);
    
    // Create per-Duration item key below g_Flush.hOutput
    lResult = RegCreateKeyEx((HKEY)g_Flush.hOutput, pItem->szItemName, 0, NULL, 0,
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

        // Perf count timestamp
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            // Data must be written in microseconds
            ULARGE_INTEGER ulMicroseconds;

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

            pCurCounter++;
        }

        // Tick count timestamp
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

            pCurCounter++;
        }

        // CPU performance counter data
        if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
            && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {
            WORD  wCounter;
            WCHAR szSum[12];
            WCHAR szMax[12];
            WCHAR szMin[12];

            // Dump CEPERF_DISCRETE_COUNTER_DATA for each perf counter
            for (wCounter = 0;
                 (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
                 wCounter++) {

                // Print value names -- won't overflow the buffer
                swprintf(szSum, TEXT("CPU%02uSum"), wCounter);
                swprintf(szMin, TEXT("CPU%02uMin"), wCounter);
                swprintf(szMax, TEXT("CPU%02uMax"), wCounter);

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
static HRESULT FlushMinDataBinary(
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    CEPERF_LOGDATA_DURATION_DISCRETE* pOutput = (CEPERF_LOGDATA_DURATION_DISCRETE*)g_Flush.lpTempBuffer;
    WORD  wNumCounters;
    DurationDataList *pDL = (DurationDataList *)GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);

    pOutput->header.bID = CEPERF_LOGID_DURATION_DISCRETE;
    pOutput->header.bReserved = 0;
    pOutput->header.wSize = sizeof(CEPERF_LOGDATA_DURATION_DISCRETE);

    pOutput->hTrackedItem = hTrackedItem;
    pOutput->dwCount      = pDL->dwNumVals;
    pOutput->dwErrorCount = pItem->data.Duration.dwErrorCount;

    // Skip the rest if the Duration was never succesfully entered
    if (pDL->dwNumVals > 0) {
        // Calculate how many counters need to be copied
        wNumCounters = 0;
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
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            wNumCounters++;
        }
        
        pOutput->header.wSize += (wNumCounters*sizeof(CEPERF_DISCRETE_COUNTER_DATA));
    
        // Copy all of the counters into the buffer
        DEBUGCHK(pOutput->header.wSize <= g_Flush.dwTempBufferSize);  // discarded if doesn't fit
        if (pOutput->header.wSize <= g_Flush.dwTempBufferSize) {
            memcpy(((LPBYTE)(pOutput)) + sizeof(CEPERF_LOGDATA_DURATION_DISCRETE),
                   DATALIST_PDATA(pDL),
                   wNumCounters * sizeof(CEPERF_DISCRETE_COUNTER_DATA));
        }
    }

    // Write the buffer to the output location
    return FlushBytes(&g_Flush, (LPBYTE)pOutput, pOutput->header.wSize);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write (discrete) data from min-record Duration item into a text stream.
static HRESULT FlushMinDataText(
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
        hResult = FlushChars(&g_Flush, TEXT("\thDuration= 0x%08x\r\n"),
                             (DWORD)hTrackedItem);
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }
    
    hResult = FlushChars(&g_Flush, TEXT("\t\tCount=%u, ErrorCount=%u\r\n"),
                         pDL->dwNumVals,
                         pItem->data.Duration.dwErrorCount);
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // Skip the rest if the Duration was never succesfully entered
    if (pDL->dwNumVals > 0) {
        CEPERF_DISCRETE_COUNTER_DATA* pCurCounter;
        ULARGE_INTEGER ulAverage;
    
        pCurCounter = DATALIST_PDATA(pDL);
    
        // Write each counter
    
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            ULARGE_INTEGER ulMin, ulMax;

            // Convert to microseconds
            PerformanceCounterToMicroseconds(&pCurCounter->ulTotal, &ulAverage);
            PerformanceCounterToMicroseconds(&pCurCounter->ulMinVal, &ulMin);
            PerformanceCounterToMicroseconds(&pCurCounter->ulMaxVal, &ulMax);
            
            // Calculate average
            ulAverage.QuadPart *= 10;  // for decimal point later
            ulAverage.QuadPart /= pDL->dwNumVals;
            
            DEBUGCHK((ulMax.QuadPart >= (ulAverage.QuadPart / 10))
                     && (ulMin.QuadPart <= (ulAverage.QuadPart / 10)));  // sanity check
            DEBUGCHK((pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart)
                     && (ulMax.QuadPart >= ulMin.QuadPart));  // sanity check
            
            hResult = FlushChars(&g_Flush, 
                                 TEXT("\t\tPerfCount(us): Min=%I64u, Max=%I64u, Avg=%I64u.%u\r\n"),
                                 ulMin.QuadPart, ulMax.QuadPart,
                                 ulAverage.QuadPart / 10,
                                 (DWORD)(ulAverage.QuadPart % 10));
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
    
            pCurCounter++;
        }
        
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            ulAverage.QuadPart = pCurCounter->ulTotal.QuadPart;
            ulAverage.QuadPart *= 10;  // for decimal point later
            ulAverage.QuadPart /= pDL->dwNumVals;
            
            DEBUGCHK((pCurCounter->ulMaxVal.QuadPart >= (ulAverage.QuadPart / 10))
                     && (pCurCounter->ulMinVal.QuadPart <= (ulAverage.QuadPart / 10)));  // sanity check
            DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check
            DEBUGCHK(pCurCounter->ulMaxVal.HighPart == 0);  // TickCount should only be using lower 32 bits
            DEBUGCHK(pCurCounter->ulMinVal.HighPart == 0);  // TickCount should only be using lower 32 bits
            
            hResult = FlushChars(&g_Flush,
                                 TEXT("\t\tTickCount(ms): Min=%I64u, Max=%I64u, Avg=%I64u.%u\r\n"),
                                 pCurCounter->ulMinVal.QuadPart,
                                 pCurCounter->ulMaxVal.QuadPart,
                                 ulAverage.QuadPart / 10,
                                 (DWORD)(ulAverage.QuadPart % 10));
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
    
            pCurCounter++;
        }
        
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR) {
            WORD  wCounter;
    
            // Dump CEPERF_DISCRETE_COUNTER_DATA for each perf counter
            for (wCounter = 0;
                 (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
                 wCounter++) {

                ulAverage.QuadPart = pCurCounter->ulTotal.QuadPart;
                ulAverage.QuadPart *= 10;  // for decimal point later
                ulAverage.QuadPart /= pDL->dwNumVals;
    
                DEBUGCHK((pCurCounter->ulMaxVal.QuadPart >= (ulAverage.QuadPart / 10))
                         && (pCurCounter->ulMinVal.QuadPart <= (ulAverage.QuadPart / 10)));  // sanity check
                DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check

                hResult = FlushChars(&g_Flush,
                                     TEXT("\t\tCPU%02u:     Min=%I64u, Max=%I64u, Avg=%I64u.%u\r\n"),
                                     wCounter, pCurCounter->ulMinVal.QuadPart,
                                     pCurCounter->ulMaxVal.QuadPart,
                                     ulAverage.QuadPart / 10,
                                     (DWORD)(ulAverage.QuadPart % 10));
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
    const CEPERF_DISCRETE_COUNTER_DATA *pCurCounter
    )
{
    HRESULT hResult;

    if (pCurCounter) {
        // Counter is active -- write the data
        DEBUGCHK(pCurCounter->ulMaxVal.QuadPart >= pCurCounter->ulMinVal.QuadPart);  // sanity check

        // Min, max, total
        hResult = FlushULargeCSV(&g_Flush, &(pCurCounter->ulMinVal));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        hResult = FlushULargeCSV(&g_Flush, &(pCurCounter->ulMaxVal));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        hResult = FlushULargeCSV(&g_Flush, &(pCurCounter->ulTotal));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }

    // Counter is not being recorded -- write empty data
    return FlushChars(&g_Flush, TEXT(",,,"));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write (discrete) data from min-record Duration item into a comma-separated
// text stream.
static HRESULT FlushMinDataCSV(
    TrackedItem* pItem
    )
{
    HRESULT hResult;
    WORD    wCounter;
    DurationDataList *pDL = (DurationDataList *)GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);
    // Write the session name first
    if (g_Flush.pSession->szSessionName[0]) {
        hResult = FlushBytes(&g_Flush, (LPBYTE)(g_Flush.pSession->szSessionName),
                             wcslen(g_Flush.pSession->szSessionName)*sizeof(WCHAR));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }

    // Write basic data
    hResult = FlushChars(&g_Flush, TEXT(",%s,%u,%u"),
                         pItem->szItemName,
                         pDL->dwNumVals,
                         pItem->data.Duration.dwErrorCount);
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // Skip the rest if the Duration was never succesfully entered
    if (pDL->dwNumVals > 0) {
        CEPERF_DISCRETE_COUNTER_DATA* pCurCounter;
        CEPERF_DISCRETE_COUNTER_DATA  CounterUS;
    
        pCurCounter = DATALIST_PDATA(pDL);
    
        // Write each counter
    
        // Special case perfcount because it needs to be converted to microseconds
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            PerformanceCounterToMicroseconds(&pCurCounter->ulTotal, &CounterUS.ulTotal);
            PerformanceCounterToMicroseconds(&pCurCounter->ulMinVal, &CounterUS.ulMinVal);
            PerformanceCounterToMicroseconds(&pCurCounter->ulMaxVal, &CounterUS.ulMaxVal);
            hResult = FlushCounterCSV(&CounterUS);
            pCurCounter++;
        } else {
            hResult = FlushCounterCSV(NULL);
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            hResult = FlushCounterCSV(pCurCounter);
            pCurCounter++;
        } else {
            hResult = FlushCounterCSV(NULL);
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        
        // Dump each CPU perf counter
        for (wCounter = 0;
             (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
             wCounter++) {

            if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR) {
                hResult = FlushCounterCSV(pCurCounter);
                pCurCounter++;
            } else {
                hResult = FlushCounterCSV(NULL);
            }
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        }
    }
    
    hResult = FlushChars(&g_Flush, TEXT("\r\n"));
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllItems.  Write descriptor and discrete data
// to the storage location.  Clear the data if necessary.
HRESULT FlushDuration(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    LPVOID       lpWalkParam      // Ignored
    )
{
    HRESULT hResult = ERROR_SUCCESS;

    // Will be called with Statistic and LocalStatistic items too
    if (pItem->type == CEPERF_TYPE_DURATION) {
    
        // Write data and/or descriptors of discrete tracked items,
        // or descriptors of continuous tracked items, to the storage location.
        
        // Flush the descriptor first
        if (g_Flush.dwFlushFlags & CEPERF_FLUSH_DESCRIPTORS) {
            if (g_Flush.pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                // Binary output
                hResult = FlushDescriptorBinary(hTrackedItem, pItem);
            } else if (g_Flush.pSession->settings.dwStorageFlags & CEPERF_STORE_TEXT) {
                // Text output
                hResult = FlushDescriptorText(hTrackedItem, pItem);
            }
        }
        
        // Then flush the data
        if (g_Flush.dwFlushFlags & CEPERF_FLUSH_DATA) {
            // Min-record mode is the only one that has data to flush
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                if (g_Flush.pSession->settings.dwStorageFlags & CEPERF_STORE_REGISTRY) {
                    hResult = FlushMinDataToRegistry(pItem);
                } else if (g_Flush.pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
                    hResult = FlushMinDataCSV(pItem);
                } else if (g_Flush.pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                    // Binary output
                    hResult = FlushMinDataBinary(hTrackedItem, pItem);
                } else {
                    // Text output
                    hResult = FlushMinDataText(hTrackedItem, pItem,
                                               (g_Flush.dwFlushFlags & CEPERF_FLUSH_DESCRIPTORS) ? TRUE : FALSE);
                }
            }
        }
    
        if (g_Flush.dwFlushFlags & CEPERF_FLUSH_AND_CLEAR) {
            DurationDataList *pDL;
            ClearBeginList(pItem);
            pItem->data.Duration.dwErrorCount = 0;            
            pDL = (DurationDataList *)GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);
            // Min-record mode is the only one that has data to clear
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                WORD wCounter;
                CEPERF_DISCRETE_COUNTER_DATA* pData;
                memset(pDL, 0, g_pMaster->list.wDurDLSize);
        
                // Set "min" values to -1
                pData = DATALIST_PDATA(pDL);
                for (wCounter = 0;
                     (wCounter < 2+g_pMaster->CPU.wNumCounters) && (wCounter < 2+CEPERF_MAX_CPUCOUNTER_COUNT);
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
// Write (continuous) data from full-record or short-record Duration item into
// a binary stream.
static HRESULT RecordContinuousBinary(
    SessionHeader*    pSession,
    HANDLE            hTrackedItem,
    TrackedItem*      pItem,
    BYTE              bCePerfLogID,
    DWORD             dwErrorCode,      // Treated as an error if non-zero
    CEPERF_ITEM_DATA* lpData,           // Buffer with current counter values
    const BYTE*       lpInfoBuf,        // Buffer of additional information to log, or NULL
    DWORD             dwInfoBufSize     // Size of information buffer, in bytes
    )
{
    DEBUGCHK(0);  // NYI
    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write (continuous) data from full-record or short-record Duration item into
// a text stream.
static HRESULT RecordContinuousText(
    SessionHeader*    pSession,
    HANDLE            hTrackedItem,
    TrackedItem*      pItem,
    BYTE              bCePerfLogID,
    DWORD             dwErrorCode,      // Treated as an error if non-zero and End
    CEPERF_ITEM_DATA* lpData,           // Buffer with current counter values
    const BYTE*       lpInfoBuf,        // Buffer of additional information to log, or NULL
    DWORD             dwInfoBufSize     // Size of information buffer, in bytes
    )
{
    DEBUGCHK(0);  // NYI
    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write (continuous) data from full-record or short-record Duration item into
// a comma-separated text stream.
static HRESULT RecordContinuousCSV(
    SessionHeader*    pSession,
    HANDLE            hTrackedItem,
    TrackedItem*      pItem,
    BYTE              bCePerfLogID,
    DWORD             dwErrorCode,      // Treated as an error if non-zero and End
    CEPERF_ITEM_DATA* lpData,           // Buffer with current counter values
    const BYTE*       lpInfoBuf,        // Buffer of additional information to log, or NULL
    DWORD             dwInfoBufSize     // Size of information buffer, in bytes
    )
{
    DEBUGCHK(0);  // NYI
    return ERROR_SUCCESS;
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
        hThread = (HANDLE)GetCurrentThreadId();
    }
    
    pCounters = NULL;
    pBL = (DurationBeginList *)GET_MAP_POINTER(pItem->data.Duration.offsetBL);
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
            pBL = (DurationBeginList *)GET_MAP_POINTER(0);  // Stop looping
        } else {
            if ((bFrame == 0) && (pBL->offsetNextDBL != 0)) {
                // Did not encounter an entry for this thread; walk to the next DBL and
                // keep looking
                pBL = (DurationBeginList *)GET_MAP_POINTER(pBL->offsetNextDBL);
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
        hThread = (HANDLE)GetCurrentThreadId();
    }
    
    pBL = (DurationBeginList *)GET_MAP_POINTER(pItem->data.Duration.offsetBL);
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
        }
        
        // Should always find a frame when in shared mode, should not get here
        DEBUGCHK(!(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_SHARED));
        // pNext must be null in limited record mode
        DEBUGCHK((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_UNLIMITED)
                 || (pBL->offsetNextDBL == 0));
        
        // Not found - keep looking for this thread in the list
        pBL = (DurationBeginList *)GET_MAP_POINTER(pBL->offsetNextDBL);
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
// Store the data
//   RECORD_MIN: Does nothing
//   RECORD_SHORT & RECORD_FULL: Writes continous data to the output stream
static BOOL RecordBeginData(
    SessionHeader* pSession,
    HANDLE         hTrackedItem,
    TrackedItem*   pItem,
    DurationBeginCounters* pCounters,   // Current perf counters
    DWORD          dwErrorCode          // Treated as an error if non-zero
    )
{
    // Full-record mode records current counter values; write to storage
    if (!(pSession->settings.dwStatusFlags & CEPERF_STATUS_STORAGE_DISABLED)
        && (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_FULL)) {
        DEBUGCHK(0);  // NYI
    }
    return ERROR_SUCCESS;
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
    
    // Full-record mode records current counter values; write to storage
    if ((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_FULL)
        && !(pSession->settings.dwStatusFlags & CEPERF_STATUS_STORAGE_DISABLED)) {
        if (pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
            // Binary output
            RecordContinuousBinary(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_INTERMEDIATE, dwErrorCode, lpData, NULL, 0);
        } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_TEXT) {
            // Text output
            RecordContinuousText(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_INTERMEDIATE, dwErrorCode, lpData, NULL, 0);
        } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
            // CSV output
            RecordContinuousCSV(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_INTERMEDIATE, dwErrorCode, lpData, NULL, 0);
        }
    }
    
    if (pCounters) {
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            // Calculate the delta between Begin and End value
            lpData->Duration.liPerfCount.QuadPart -= pCounters->liPerfCount.QuadPart;
            DEBUGCHK(lpData->Duration.liPerfCount.HighPart == 0);
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

        // Short-record mode records counter diff; write to storage
        if ((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_SHORT)
            && !(pSession->settings.dwStatusFlags & CEPERF_STATUS_STORAGE_DISABLED)) {
            if (pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                // Binary output
                RecordContinuousBinary(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_INTERMEDIATE, dwErrorCode, lpData, NULL, 0);
            } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_TEXT) {
                // Text output
                RecordContinuousText(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_INTERMEDIATE, dwErrorCode, lpData, NULL, 0);
            } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
                // CSV output
                RecordContinuousCSV(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_INTERMEDIATE, dwErrorCode, lpData, NULL, 0);
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
    CEPERF_DISCRETE_COUNTER_DATA* pMinData = DATALIST_PDATA(pDL);

    DEBUGCHK(!(pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_NONE));
    
    // Record error
    if (dwErrorCode != 0) {
        pItem->data.Duration.dwErrorCount++;
    } else {
        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
            pDL->dwNumVals++;
        }
    }
    
    // Full-record mode records current counter values; write to storage
    if ((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_FULL)
        && !(pSession->settings.dwStatusFlags & CEPERF_STATUS_STORAGE_DISABLED)) {
        if (pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
            // Binary output
            RecordContinuousBinary(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_END, dwErrorCode, lpData, lpInfoBuf, dwInfoBufSize);
        } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_TEXT) {
            // Text output
            RecordContinuousText(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_END, dwErrorCode, lpData, lpInfoBuf, dwInfoBufSize);
        } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
            // CSV output
            RecordContinuousCSV(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_END, dwErrorCode, lpData, lpInfoBuf, dwInfoBufSize);
        }
    }
    
    if (pCounters) {
        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
            // Calculate the delta between Begin and End value
            DEBUGCHK(lpData->Duration.liPerfCount.QuadPart >= pCounters->liPerfCount.QuadPart);
            DEBUGCHK((lpData->Duration.dwTickCount >= pCounters->dwTickCount)
                     || (((int)pCounters->dwTickCount < 0) && ((int)lpData->Duration.dwTickCount > 0)));  // GetTickCount will roll over on debug builds sometimes
            lpData->Duration.liPerfCount.QuadPart -= pCounters->liPerfCount.QuadPart;

            // Record the data
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                if (dwErrorCode == 0) {
                    UpdateDurationDataUL(pMinData, (ULARGE_INTEGER*)&lpData->Duration.liPerfCount);
                }
                pMinData++;
            }
        }

        if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
            // Calculate the delta between Begin and End value
            DEBUGCHK(lpData->Duration.dwTickCount >= pCounters->dwTickCount);
            lpData->Duration.dwTickCount -= pCounters->dwTickCount;

            // Record the data
            if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                if (dwErrorCode == 0) {
                    UpdateDurationDataDW(pMinData, lpData->Duration.dwTickCount);
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
                                UpdateDurationDataDW(pMinData, *((BYTE*)lpEndData));
                            } 
                            pMinData++;                            
                        }
                        break;
    
                    case sizeof(WORD):
                        *((WORD*)lpEndData) -= *((WORD*)lpBeginData);
                        // Record the data
                        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                            if (dwErrorCode == 0) {
                                UpdateDurationDataDW(pMinData, *((WORD*)lpEndData));
                            } 
                            pMinData++;
                        }
                        break;
    
                    case sizeof(DWORD):
                        *((DWORD*)lpEndData) -= *((DWORD*)lpBeginData);
                        // Record the data
                        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                            if (dwErrorCode == 0) {
                                UpdateDurationDataDW(pMinData, *((DWORD*)lpEndData));
                            }
                            pMinData++;                            
                        }
                        break;
    
                    case sizeof(ULARGE_INTEGER):
                        (((ULARGE_INTEGER*)lpEndData)->QuadPart -= ((ULARGE_INTEGER*)lpBeginData)->QuadPart);
                        // Record the data
                        if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                            if (dwErrorCode == 0) {
                                UpdateDurationDataUL(pMinData, (ULARGE_INTEGER*)lpEndData);
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

        // Short-record mode records counter diff; write to storage
        if ((pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_SHORT)
            && !(pSession->settings.dwStatusFlags & CEPERF_STATUS_STORAGE_DISABLED)) {
            if (pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                // Binary output
                RecordContinuousBinary(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_END, dwErrorCode, lpData, lpInfoBuf, dwInfoBufSize);
            } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_TEXT) {
                // Text output
                RecordContinuousText(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_END, dwErrorCode, lpData, lpInfoBuf, dwInfoBufSize);
            } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
                // CSV output
                RecordContinuousCSV(pSession, hTrackedItem, pItem, CEPERF_LOGID_DURATION_END, dwErrorCode, lpData, lpInfoBuf, dwInfoBufSize);
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
                                             &(pCounters->dwTickCount),
                                             BEGINCOUNTERS_PCPU(pCounters));
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

        // Write to storage
        RecordBeginData(pSession, hTrackedItem, pItem, pCounters, hResult);
                    
        ITEM_EPILOG(pSession);
    }

    DEBUGMSG(ZONE_API && ZONE_DURATION,
             (TEXT("-CePerfBeginDuration (%u)\r\n"), hResult));
    
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

        // Record the diff in the counters from Begin to Intermediate, write to
        // storage, and put the result into lpData
        RecordIntermediateData(pSession, hTrackedItem, pItem, pCounters,
                               hResult, lpData);

    exit:
        ITEM_EPILOG(pSession);
    }
    
    DEBUGMSG(ZONE_API && ZONE_DURATION,
             (TEXT("-CePerfIntermediateDuration (%u)\r\n"), hResult));
    
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
             (TEXT("-CePerfEndDurationWithInformation (%u)\r\n"), hResult));
    
    return hResult;
}

