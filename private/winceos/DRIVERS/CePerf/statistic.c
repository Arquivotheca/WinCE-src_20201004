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
//      statistic.c
//  
//  Abstract:  
//
//      Implements statistic objects for the Windows CE
//      Performance Measurement API.
//      
//------------------------------------------------------------------------------

#include <windows.h>
#include "ceperf.h"
#include "ceperf_log.h"
#include "pceperf.h"


// GLOBALS


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// LIST MANAGEMENT - one list for StatisticData items.

// pIsItemFree list traversal function; test if the DataList item is free
static BOOL IsStatDataFree(LPBYTE pItem) {
    return STATDATA_ISFREE(pItem);
}

// pGetNextFreeItem list traversal function; for a free StatisticData item, get 
// index (on current page) of next item in freelist.
static WORD GetNextFreeStatData(LPBYTE pItem) {
    return STATDATA_NEXTFREE(pItem);
}

// pFreeItem list traversal function; mark a StatisticData item as being free
static VOID FreeStatData(LPBYTE pItem, WORD wNextFreeIndex) {
    STATDATA_MARKFREE(pItem, wNextFreeIndex);
}

// Used to operate on g_pMaster->list.pStatDataHead list of StatisticData objects
static DLLListFuncs g_StatDataListFuncs = {
    IsStatDataFree,                   // pIsItemFree
    GetNextFreeStatData,              // pGetNextFreeItem
    FreeStatData,                     // pFreeItem
};


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Validate pExt and copy resulting values (mapped pointers, etc) to pNewExt.
// Return TRUE if valid and successfully copied.
BOOL ValidateStatisticDescriptor(
    const CEPERF_BASIC_ITEM_DESCRIPTOR* pBasic,
    const CEPERF_EXT_ITEM_DESCRIPTOR*   pExt,
    CEPERF_EXT_ITEM_DESCRIPTOR*         pNewExt
    )
{
    if (pBasic->type == CEPERF_TYPE_STATISTIC) {
        if (!pExt) {
            memset(pNewExt, 0, sizeof(CEPERF_EXT_ITEM_DESCRIPTOR));
            return TRUE;
        }
    
    } else if (pBasic->type == CEPERF_TYPE_LOCALSTATISTIC) {
        if (pExt && (pExt->wVersion == 1)
            && pExt->LocalStatistic.lpValue && pExt->LocalStatistic.dwValSize) {
            pNewExt->LocalStatistic.lpValue   = pExt->LocalStatistic.lpValue;
            pNewExt->LocalStatistic.dwValSize = pExt->LocalStatistic.dwValSize;
            return TRUE;
        }
    }
    
    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Compare the extended descriptor from pItem against pExt, return TRUE if same.
BOOL CompareStatisticDescriptor(
    TrackedItem* pItem,
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt
    )
{
    if ((pItem->type == CEPERF_TYPE_LOCALSTATISTIC)
        && (pExt->LocalStatistic.lpValue == pItem->data.LocalStatistic.MinRecord.lpValue)
        && (pExt->LocalStatistic.dwValSize == pItem->data.LocalStatistic.MinRecord.dwValSize)) {
        // Item matches the descriptor
        return TRUE;
    }
    
    // Otherwise no match
    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// The TrackedItem object is already initialized with basic info; this func
// initializes additional Statistic-specific data.  Used for LocalStatistic
// items too.
BOOL InitializeStatisticData(
    TrackedItem* pItem,
    const CEPERF_EXT_ITEM_DESCRIPTOR* pExt  // Will not be non-NULL on creation;
                                            // will be NULL on recreation
    )
{
    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        memset(&pItem->data.Statistic.ulValue, 0, sizeof(ULARGE_INTEGER));

        if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT) {
            DiscreteCounterData* pData;

            // Allocate private RAM for the deltas
            StatisticData *pChangeData = (StatisticData*)
                AllocListItem(&g_StatDataListFuncs, STATDATA_SIZE,
                              &(g_pMaster->list.offsetStatDataHead));
            if (!pChangeData) {
                return FALSE;
            }
            
            // Initialize the data
            
            pItem->data.Statistic.ShortRecord.offsetChangeData = GET_MAP_OFFSET(pChangeData);
            memset(pChangeData, 0, STATDATA_SIZE);

            // Set "min" value to -1
            pData = STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData);
            DEBUGCHK((LPBYTE)&pData->ulMinVal.QuadPart < ((LPBYTE)pChangeData) + STATDATA_SIZE);
            pData->ulMinVal.HighPart = (DWORD)-1;
            pData->ulMinVal.LowPart = (DWORD)-1;
            
            DEBUGMSG(ZONE_ITEM && ZONE_MEMORY,
                     (TEXT("CePerf: New StatisticData=0x%08x for pItem=0x%08x\r\n"),
                     pChangeData, pItem));
        }

        return TRUE;
    
    } else if (pItem->type == CEPERF_TYPE_LOCALSTATISTIC) {
        // If we are reinitializing the item, it had better have valid data
        DEBUGCHK(pExt || (pItem->data.LocalStatistic.MinRecord.lpValue
                          && pItem->data.LocalStatistic.MinRecord.dwValSize));
        
        if (pExt) {
            pItem->data.LocalStatistic.MinRecord.lpValue   = pExt->LocalStatistic.lpValue;
            pItem->data.LocalStatistic.MinRecord.dwValSize = pExt->LocalStatistic.dwValSize;
            pItem->data.LocalStatistic.MinRecord.ProcessId = GetCurrentProcessId();
        }
        
        return TRUE;
    }
    
    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Marks the item's data as not in use.  Does not check to see if the data pages
// need to be released, unless fReleasePages is TRUE.
VOID FreeStatisticData(
    TrackedItem* pItem,
    BOOL         fReleasePages
    )
{
    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT) {
            // Free private RAM for the deltas
            if (pItem->data.Statistic.ShortRecord.offsetChangeData != 0) {
                DEBUGMSG(ZONE_ITEM && ZONE_MEMORY,
                         (TEXT("CePerf: Free StatisticData=0x%08x for pItem=0x%08x\r\n"),
                          GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData), pItem));
    
                FreeListItem(&g_StatDataListFuncs, STATDATA_SIZE,
                             &(g_pMaster->list.offsetStatDataHead),
                             (LPBYTE)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData),
                             fReleasePages);
                pItem->data.Statistic.ShortRecord.offsetChangeData = 0;
            }
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Dereference a LocalStatistic item
BOOL ReadLocalStatistic(
    TrackedItem* pItem,
    LPBYTE       pBuffer,   // Buffer to read the data into
    DWORD        cbBuffer
    )
{
    HANDLE hProcess;
    DWORD  cbToRead, cbRead;
    BOOL   result = FALSE;

    // Note, the copy will fail if the caller doesn't have
    // access to the data, for security reasons.

    DEBUGCHK(pItem->type == CEPERF_TYPE_LOCALSTATISTIC);
    hProcess = OpenProcess(PROCESS_VM_READ, FALSE,
                           pItem->data.LocalStatistic.MinRecord.ProcessId);
    if (hProcess) {
        cbToRead = pItem->data.LocalStatistic.MinRecord.dwValSize;
        if ((cbBuffer >= cbToRead)
            && ReadProcessMemory(hProcess,
                                 pItem->data.LocalStatistic.MinRecord.lpValue,
                                 pBuffer, cbToRead, &cbRead)
            && (cbRead == cbToRead)) {
            result = TRUE;
        }
        CloseHandle(hProcess);
    }
    return result;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Called after deleting a lot of tracked objects, to clean up any unused pages.
VOID FreeUnusedStatisticPages()
{
    // Check the StatisticData list
    FreeUnusedListPages(&g_StatDataListFuncs, STATDATA_SIZE,
                        &(g_pMaster->list.offsetStatDataHead));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Prepare resources for doing the flush
HRESULT StatisticFlushBegin(
    FlushState* pFlush
    )
{
    if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
        // BeginFlushCSV

        // BUGBUG want to avoid printing this if there are no Statistic items in
        // the session!  (no way to tell?)
        return FlushChars(pFlush,
                          TEXT("\r\nSession Name,")
                          TEXT("Item Name,")
                          TEXT("Current Value,")

                          TEXT("Number of Changes,")
                          TEXT("Min Change,")
                          TEXT("Max Change,")
                          TEXT("Total Change\r\n"));
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write descriptor for Statistic item into a text stream.
_inline static HRESULT FlushDescriptorText(
    FlushState*  pFlush,
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        return FlushChars(pFlush,
                          TEXT("\thStatistic=0x%08x, dwRecordingFlags=0x%08x, %s\r\n"),
                          (DWORD)hTrackedItem, pItem->dwRecordingFlags, pItem->szItemName);
    } else {  // LocalStatistic
        DEBUGCHK(pItem->type == CEPERF_TYPE_LOCALSTATISTIC);
        return FlushChars(pFlush,
                          TEXT("\thLocalStat=0x%08x, dwRecordingFlags=0x%08x, %s\r\n"),
                          (DWORD)hTrackedItem, pItem->dwRecordingFlags, pItem->szItemName);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write discrete data from Statistic item into the registry
static HRESULT FlushDataToRegistry(
    FlushState*  pFlush,
    TrackedItem* pItem
    )
{
    LONG   lResult;
    LPBYTE pValue;
    DWORD  dwValSize;
    StatisticData* pChangeData;

    DEBUGCHK(pFlush->hOutput != INVALID_HANDLE_VALUE);

    // This function doesn't use much of the temp buffer space
    if (pFlush->cbTempBuffer < sizeof(ULARGE_INTEGER)) {
        return CEPERF_HR_NOT_ENOUGH_MEMORY;
    }

    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        pValue = (LPBYTE) &(pItem->data.Statistic.ulValue);
        dwValSize = sizeof(ULARGE_INTEGER);
    } else {
        DEBUGCHK(pItem->type == CEPERF_TYPE_LOCALSTATISTIC);
        if (!ReadLocalStatistic(pItem, pFlush->lpTempBuffer, sizeof(ULARGE_INTEGER))) {
            return CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
        }
        pValue = pFlush->lpTempBuffer;
        dwValSize = pItem->data.LocalStatistic.MinRecord.dwValSize;
    }
    lResult = RegSetValueEx((HKEY) pFlush->hOutput, pItem->szItemName, 0,
                            REG_BINARY, pValue, sizeof(ULARGE_INTEGER));

    // Copy short data (Skip if the Statistic was never modified)
    pChangeData = (StatisticData *) GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData);
    if ((pItem->type == CEPERF_TYPE_STATISTIC)
        && (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT)
        && (pChangeData->dwNumVals > 0)) {
        HKEY hkStatistic;

        // Need to create a key for the Statistic item, to keep its
        // short-record data separate from other items'.
        if (RegCreateKeyEx((HKEY) pFlush->hOutput, pItem->szItemName, 0, NULL, 0,
#ifdef UNDER_CE
                           0,
#else
                           KEY_ALL_ACCESS,
#endif // UNDER_CE
                           NULL, &hkStatistic, NULL) == ERROR_SUCCESS) {

            DiscreteCounterData* pData;
            double dStdDev;
            
            pData = STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData);
            RegSetValueEx(hkStatistic, TEXT("ChangeCount"), 0, REG_DWORD,
                          (LPBYTE) &(pChangeData->dwNumVals),
                          sizeof(DWORD));
            RegSetValueEx(hkStatistic, TEXT("ChangeSum"), 0, REG_BINARY,
                          (LPBYTE) &(pData->ulTotal), sizeof(ULARGE_INTEGER));
            RegSetValueEx(hkStatistic, TEXT("ChangeMax"), 0, REG_BINARY,
                          (LPBYTE) &(pData->ulMaxVal), sizeof(ULARGE_INTEGER));
            RegSetValueEx(hkStatistic, TEXT("ChangeMin"), 0, REG_BINARY,
                          (LPBYTE) &(pData->ulMinVal), sizeof(ULARGE_INTEGER));
            dStdDev = GetStandardDeviation(pData, pChangeData->dwNumVals);
            RegSetValueEx(hkStatistic, TEXT("ChangeStdDev"), 0, REG_BINARY,
                          (LPBYTE) &dStdDev, sizeof(double));

            RegCloseKey(hkStatistic);
        }
    }

    return (HRESULT) lResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write discrete data from Statistic item into a binary stream.
// Defining 2 different "Data" members in the same function confuses the tools 
// so split into 2 different functions
static HRESULT FlushStatisticDataBinary(
    FlushState*  pFlush,
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    // Convenient declaration of the data that will be flushed
#pragma pack(push,4)
    typedef struct {
        CEPERF_LOGID id;
        CEPERF_LOG_STATISTIC_DISCRETE statistic;
        CEPERF_DISCRETE_COUNTER_DATA counter;  // If short-record
    } Data;
#pragma pack(pop)

    DEBUGCHK(pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG);
    if (pFlush->lpTempBuffer && (pFlush->cbTempBuffer >= sizeof(Data))) {
        Data* pData = (Data*) pFlush->lpTempBuffer;
        StatisticData* pChangeData = (StatisticData *) GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData);
        DWORD cbData;

        pData->id = CEPERF_LOGID_STATISTIC_DISCRETE;
        pData->statistic.dwItemID = (DWORD) hTrackedItem;
        memcpy(&pData->statistic.ulValue.QuadPart, &pItem->data.Statistic.ulValue.QuadPart, sizeof(ULARGE_INTEGER));
        cbData = sizeof(CEPERF_LOGID) + sizeof(CEPERF_LOG_STATISTIC_DISCRETE);

        // Copy short data (Skip if the Statistic was never modified)
        if ((pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT)
            && (pChangeData->dwNumVals > 0)) {
            DiscreteCounterData* pCounter = STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData);

            pData->statistic.dwNumChanges = pChangeData->dwNumVals;
            pData->counter.ulTotal = pCounter->ulTotal;
            pData->counter.ulMinVal = pCounter->ulMinVal;
            pData->counter.ulMaxVal = pCounter->ulMaxVal;
            pData->counter.dStdDev = GetStandardDeviation(pCounter, pChangeData->dwNumVals);
            cbData += sizeof(CEPERF_DISCRETE_COUNTER_DATA);
        } else {
            pData->statistic.dwNumChanges = 0;
        }

        // Write the buffer to the output location
        return FlushBytes(pFlush, (LPBYTE) pData, cbData);
    }

    DEBUGCHK(0);  // Underestimated necessary buffer size
    return CEPERF_HR_NOT_ENOUGH_MEMORY;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write discrete data from LocalStatistic item into a binary stream.
// Defining 2 different "Data" members in the same function confuses the tools 
// so split into 2 different functions
static HRESULT FlushLocalStatisticDataBinary(
    FlushState*  pFlush,
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    // Convenient declaration of the data that will be flushed
#pragma pack(push,4)
    typedef struct {
        CEPERF_LOGID id;
        CEPERF_LOG_LOCALSTATISTIC_DISCRETE LocalStatistic;
        BYTE data[8];
    } Data;
#pragma pack(pop)

    DEBUGCHK(pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG);
    if (pFlush->lpTempBuffer && (pFlush->cbTempBuffer >= sizeof(Data))) {
        Data* pData = (Data*) pFlush->lpTempBuffer;
        DWORD cbData;

        pData->id = CEPERF_LOGID_LOCALSTATISTIC_DISCRETE;
        pData->LocalStatistic.dwItemID = (DWORD) hTrackedItem;
        cbData = sizeof(CEPERF_LOGID) + sizeof(CEPERF_LOG_LOCALSTATISTIC_DISCRETE)
                 + pItem->data.LocalStatistic.MinRecord.dwValSize;

        // Copy the data
        DEBUGCHK(pItem->dwRecordingFlags & CEPERF_LOCALSTATISTIC_RECORD_MIN);
        if (!ReadLocalStatistic(pItem, pData->data, 8)) {
            return CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
        }

        // Write the buffer to the output location
        return FlushBytes(pFlush, (LPBYTE) pData, cbData);
    }

    DEBUGCHK(0);  // Underestimated necessary buffer size
    return CEPERF_HR_NOT_ENOUGH_MEMORY;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write discrete data from Statistic item into a text stream.
static HRESULT FlushDataText(
    FlushState*  pFlush,
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    BOOL         fDescriptorFlushed   // Simplify output if the descriptor
                                      // has already been flushed.
    )
{
    HRESULT hResult;

    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        // Write basic data
        // Add the Statistic handle if there's no descriptor on the output already
        if (!fDescriptorFlushed) {
            hResult = FlushChars(pFlush, TEXT("\thStatistic=0x%08x, ulValue=0x%08x %08x\r\n"),
                                 (DWORD)hTrackedItem,
                                 pItem->data.Statistic.ulValue.HighPart,
                                 pItem->data.Statistic.ulValue.LowPart);
        } else {
            hResult = FlushChars(pFlush, TEXT("\t\tulValue=0x%08x %08x\r\n"),
                                 pItem->data.Statistic.ulValue.HighPart,
                                 pItem->data.Statistic.ulValue.LowPart);
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        
        // Copy short data (Skip if the Statistic was never modified)
        if ((pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT)
            && (((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals > 0)) {

            DiscreteCounterData* pData;
            ULARGE_INTEGER ulAverage;
            double dStdDev;
            DWORD dwNumVals;
            
            pData = STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData);
            
            dwNumVals = ((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals;
            ulAverage.QuadPart = pData->ulTotal.QuadPart;
            DEBUGCHK(ulAverage.QuadPart <= (0xffffffffffffffff) / 10); // detect overflow
            ulAverage.QuadPart *= 10;  // *10 for decimal point later
            ulAverage.QuadPart /= dwNumVals;

            DEBUGCHK((pData->ulMaxVal.QuadPart >= (ulAverage.QuadPart / 10))
                     && (pData->ulMinVal.QuadPart <= (ulAverage.QuadPart / 10)));  // sanity check
            DEBUGCHK(pData->ulMaxVal.QuadPart >= pData->ulMinVal.QuadPart);  // sanity check

            dStdDev = GetStandardDeviation(pData, dwNumVals);
            
            hResult = FlushChars(pFlush,
                                 TEXT("\t\tChanges:   Count=%u, Min=%I64u, Max=%I64u, Total=%I64u, Avg=%I64u.%u, StdDev=%.1lf\r\n"),
                                 dwNumVals,
                                 pData->ulMinVal.QuadPart, pData->ulMaxVal.QuadPart,
                                 pData->ulTotal.QuadPart,
                                 ulAverage.QuadPart / 10,
                                 (DWORD)(ulAverage.QuadPart % 10),
                                 dStdDev);

            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        }

    } else {  // LocalStatistic
        BYTE TempBuf[sizeof(ULARGE_INTEGER)];

        DEBUGCHK(pItem->type == CEPERF_TYPE_LOCALSTATISTIC);
        
        // Write basic data
        if (!fDescriptorFlushed) {
            // Add the Statistic handle if there's no descriptor on the output already
            hResult = FlushChars(pFlush,
                                 TEXT("\thLocalStat=0x%08x, value=0x"),
                                 (DWORD)hTrackedItem);
        } else {
            hResult = FlushChars(pFlush, TEXT("\t\tvalue=0x"));
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }

        if (!ReadLocalStatistic(pItem, TempBuf, sizeof(TempBuf))) {
            hResult = CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
        } else {
            switch (pItem->data.LocalStatistic.MinRecord.dwValSize) {
            case sizeof(BYTE):
                hResult = FlushChars(pFlush, TEXT("%02x\r\n"), *((BYTE*) TempBuf));
                break;
            case sizeof(WORD):
                hResult = FlushChars(pFlush, TEXT("%04x\r\n"), *((WORD*) TempBuf));
                break;
            case sizeof(DWORD):
                hResult = FlushChars(pFlush, TEXT("%08x\r\n"), *((DWORD*) TempBuf));
                break;
            case sizeof(ULARGE_INTEGER):
                {
                    ULARGE_INTEGER* pulData = (ULARGE_INTEGER*) TempBuf;
                    hResult = FlushChars(pFlush, TEXT("%08x %08x\r\n"),
                                         pulData->HighPart, pulData->LowPart);
                    break;
                }
            default:
                hResult = CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
            }
        }
    }
    
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write discrete data from Statistic item into a comma-separated text stream.
static HRESULT FlushDataCSV(
    FlushState*  pFlush,
    TrackedItem* pItem
    )
{
    HRESULT hResult;
    
    // Write the session name first
    if (pFlush->pSession->szSessionName[0]) {
        hResult = FlushBytes(pFlush, (LPBYTE)(pFlush->pSession->szSessionName),
                             wcslen(pFlush->pSession->szSessionName)*sizeof(WCHAR));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }

    // Then the item name
    hResult = FlushChars(pFlush, TEXT(",%s"), pItem->szItemName);
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // Then the current value
    if (pItem->type == CEPERF_TYPE_STATISTIC) {

#pragma warning(disable:4366)  // The result of the unary '&' operator may be unaligned
        hResult = FlushULargeCSV(pFlush, &(pItem->data.Statistic.ulValue));
#pragma warning(default:4366)

        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }

        // Copy short data (Skip if the Statistic was never modified)
        if ((pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT)
            && (((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals > 0)) {
            
            DWORD dwNumVals = ((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals;
            DiscreteCounterData* pData = STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData);
            double dStdDev;

            // Count
            hResult = FlushChars(pFlush, TEXT(",%u"), dwNumVals);
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        
            // Min, max, total, stddev
            hResult = FlushULargeCSV(pFlush, &(pData->ulMinVal));
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
            hResult = FlushULargeCSV(pFlush, &(pData->ulMaxVal));
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
            hResult = FlushULargeCSV(pFlush, &(pData->ulTotal));
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
            dStdDev = GetStandardDeviation(pData, dwNumVals);
            hResult = FlushDoubleCSV(pFlush, dStdDev);
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        }
    
    } else {  // LocalStatistic
        BYTE TempBuf[sizeof(ULARGE_INTEGER)];

        if (!ReadLocalStatistic(pItem, TempBuf, sizeof(TempBuf))) {
            hResult = CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
        } else {
            switch (pItem->data.LocalStatistic.MinRecord.dwValSize) {
            case sizeof(BYTE):
                hResult = FlushChars(pFlush, TEXT(",%u"), *((BYTE*) TempBuf));
                break;
            case sizeof(WORD):
                hResult = FlushChars(pFlush, TEXT(",%u"), *((WORD*) TempBuf));
                break;
            case sizeof(DWORD):
                hResult = FlushChars(pFlush, TEXT(",%u"), *((DWORD*) TempBuf));
                break;
            case sizeof(ULARGE_INTEGER):
                hResult = FlushULargeCSV(pFlush, (ULARGE_INTEGER*) TempBuf);
                break;
            default:
                FlushChars(pFlush, TEXT(","));
                hResult = CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
            }
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }

    hResult = FlushChars(pFlush, TEXT("\r\n"));
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllItems.  Write descriptor and discrete data
// to the storage location.  Clear the data if necessary.
HRESULT Walk_FlushStatistic(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    LPVOID       lpWalkParam      // FlushState* pFlush
    )
{
    FlushState* pFlush = (FlushState*) lpWalkParam;
    HRESULT hResult = ERROR_SUCCESS;

    // Will be called with Duration items too
    if ((pItem->type == CEPERF_TYPE_STATISTIC)
        || (pItem->type == CEPERF_TYPE_LOCALSTATISTIC)) {

        // Write data and/or descriptors of discrete tracked items,
        // or descriptors of continuous tracked items, to the storage location.
        
        // Flush the descriptor first
        if (pFlush->dwFlushFlags & CEPERF_FLUSH_DESCRIPTORS) {
            if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                // Binary output
                hResult = FlushItemDescriptorBinary(pFlush, hTrackedItem, pItem, pItem->type);
            } else if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_TEXT) {
                // Text output
                hResult = FlushDescriptorText(pFlush, hTrackedItem, pItem);
            }
        }
        
        // Then flush the data
        if (pFlush->dwFlushFlags & CEPERF_FLUSH_DATA) {
            if (((pItem->type == CEPERF_TYPE_STATISTIC) && !(pItem->dwRecordingFlags & (CEPERF_STATISTIC_RECORD_NONE | CEPERF_STATISTIC_RECORD_FULL)))
                || ((pItem->type == CEPERF_TYPE_LOCALSTATISTIC) && !(pItem->dwRecordingFlags & CEPERF_LOCALSTATISTIC_RECORD_NONE))) {
                if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_REGISTRY) {
                    hResult = FlushDataToRegistry(pFlush, pItem);
                } else if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
                    hResult = FlushDataCSV(pFlush, pItem);
                } else if (pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                    // Binary output
                    if (pItem->type == CEPERF_TYPE_STATISTIC) {
                        hResult = FlushStatisticDataBinary(pFlush, hTrackedItem, pItem);
                    } else {  // LocalStatistic
                        hResult = FlushLocalStatisticDataBinary(pFlush, hTrackedItem, pItem);
                    }
                } else {
                    // Text output
                    hResult = FlushDataText(pFlush, hTrackedItem, pItem,
                                            (pFlush->dwFlushFlags & CEPERF_FLUSH_DESCRIPTORS) ? TRUE : FALSE);
                }
            }
        }
    
        // Finally clear the data if necessary
        if (pFlush->dwFlushFlags & CEPERF_FLUSH_AND_CLEAR) {
            if (pItem->type == CEPERF_TYPE_STATISTIC) {
                memset(&pItem->data.Statistic.ulValue, 0, sizeof(ULARGE_INTEGER));
                if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT) {
                    DiscreteCounterData* pData;
    
                    memset((LPBYTE)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData), 0, STATDATA_SIZE);
    
                    // Set "min" value to -1
                    pData = STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData);
                    pData->ulMinVal.HighPart = (DWORD)-1;
                    pData->ulMinVal.LowPart  = (DWORD)-1;
                }
            }
            // Don't clear LocalStatistic items -- treat them as read-only for
            // security reasons.
        }
    }

    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// RECORD_FULL: Writes continous data to the output stream
static HRESULT RecordCeLogFullData(
    SessionHeader* pSession,
    HANDLE         hTrackedItem,
    TrackedItem*   pItem
    )
{
    FlushState flush;

    // Convenient declaration of the data that will be flushed
#pragma pack(push,4)
    struct {
        CEPERF_LOGID id;
        CEPERF_LOG_STATISTIC_FULL statistic;
    } Data;
#pragma pack(pop)
    DWORD cbData;

    DEBUGCHK(pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG);
    DEBUGCHK(pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_FULL);
    
    Data.id = CEPERF_LOGID_STATISTIC_FULL;
    Data.statistic.dwItemID = (DWORD) hTrackedItem;
    Data.statistic.ulValue = pItem->data.Statistic.ulValue;
    cbData = sizeof(CEPERF_LOGID) + sizeof(CEPERF_LOG_STATISTIC_FULL);

    // Write the buffer to the output location
    flush.pSession = pSession;
    flush.hOutput = INVALID_HANDLE_VALUE;
    flush.lpTempBuffer = NULL;
    flush.cbTempBuffer = 0;
    flush.dwFlushFlags = 0;
    return FlushBytes(&flush, (LPBYTE) &Data, cbData);
}


//------------------------------------------------------------------------------
// CePerfIncrementStatistic - Increment a statistic by 1
// CePerfAddStatistic - Add a value to a statistic
//------------------------------------------------------------------------------
HRESULT CePerf_AddStatistic(
    HANDLE hTrackedItem,        // Handle of statistic item being added to
    DWORD  dwValue,             // Value to add to the statistic
    CEPERF_ITEM_DATA* lpData    // Buffer to receive new value of statistic, or
                                // NULL (recommend set to NULL for best 
                                // performance).
                                // wVersion and wSize must be set in the struct.
    )
{
    DWORD          LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT        hResult = ERROR_SUCCESS;
    TrackedItem*   pItem;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_STATISTIC,
             (TEXT("+CePerfAddStatistic(0x%08x, %u)\r\n"),
              hTrackedItem, dwValue));
    
    if (ITEM_PROLOG(&pSession, &pItem, hTrackedItem, CEPERF_TYPE_STATISTIC, &hResult)) {
            
        // Update the value
        pItem->data.Statistic.ulValue.QuadPart += dwValue;
        DEBUGMSG(pItem->data.Statistic.ulValue.QuadPart < dwValue,
                 (TEXT("CePerf: Statistic value rollover\r\n")));
        
        // Do any additional work for this recording mode
        if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT) {
            // Short-record keeps some additional discrete data
            StatisticData* pChangeData = (StatisticData *) GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData);
            pChangeData->dwNumVals++;
            UpdateDurationDataDW(STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData),
                                 dwValue, pChangeData->dwNumVals);
        }

        // Record to CeLog if necessary
        if ((pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG)
            && (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_FULL)) {
            hResult = RecordCeLogFullData(pSession, hTrackedItem, pItem);
        }
        
        // Put the result into lpData
        if (lpData) {
            // Validate struct version and size
            if ((lpData->wVersion != 1)
                || (lpData->wSize < sizeof(CEPERF_ITEM_DATA))) {
                hResult = CEPERF_HR_INVALID_PARAMETER;
                goto exit;
            }
            
            lpData->Statistic.ulValue.QuadPart = pItem->data.Statistic.ulValue.QuadPart;
        }
        
    exit:
        ITEM_EPILOG(pSession);
    }
    
    DEBUGMSG(ZONE_API && ZONE_STATISTIC, (TEXT("-CePerfAddStatistic (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}


//------------------------------------------------------------------------------
// CePerfSetStatistic - Set a statistic to a specific value
//------------------------------------------------------------------------------
HRESULT CePerf_SetStatistic(
    HANDLE hTrackedItem,        // Handle of statistic item being set
    DWORD  dwValue              // Value to set the statistic to
    )
{
    DWORD          LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT        hResult = ERROR_SUCCESS;
    TrackedItem*   pItem;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_STATISTIC,
             (TEXT("+CePerfSetStatistic(0x%08x, %u)\r\n"),
              hTrackedItem, dwValue));

    if (ITEM_PROLOG(&pSession, &pItem, hTrackedItem, CEPERF_TYPE_STATISTIC, &hResult)) {
            
        // Do any additional work for this recording mode
        if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT) {
            // Short-record keeps some additional discrete data
            StatisticData* pChangeData = (StatisticData *) GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData);
            DWORD dwDelta;

            // Calculate the delta in the value
            dwDelta = (DWORD) (dwValue - pItem->data.Statistic.ulValue.QuadPart);

            pChangeData->dwNumVals++;
            UpdateDurationDataDW(STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData),
                                 dwDelta, pChangeData->dwNumVals);
        }
        
        // Update the value
        pItem->data.Statistic.ulValue.QuadPart = dwValue;

        // Record to CeLog if necessary
        if ((pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG)
            && (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_FULL)) {
            hResult = RecordCeLogFullData(pSession, hTrackedItem, pItem);
        }

        ITEM_EPILOG(pSession);
    }
    
    DEBUGMSG(ZONE_API && ZONE_STATISTIC, (TEXT("-CePerfSetStatistic (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}
