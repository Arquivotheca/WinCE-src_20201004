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
//      statistic.c
//  
//  Abstract:  
//
//      Implements statistic objects for the Windows CE
//      Performance Measurement API.
//      
//------------------------------------------------------------------------------

#ifndef CEPERF_ENABLE
#define CEPERF_ENABLE
#endif

#include <windows.h>
#include "ceperf.h"
#include "ceperf_log.h"
#include "pceperf.h"


// GLOBALS

// This global is only valid during session flush
static FlushGlobals g_Flush;


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
            // Check permission and map to process in one step
            pNewExt->LocalStatistic.lpValue = MapPtrToProcWithSize(pExt->LocalStatistic.lpValue,
                                                                   pExt->LocalStatistic.dwValSize,
                                                                   GetCurrentProcess());
            if (pNewExt->LocalStatistic.lpValue) {
                pNewExt->LocalStatistic.dwValSize = pExt->LocalStatistic.dwValSize;
                return TRUE;
            }
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
            CEPERF_DISCRETE_COUNTER_DATA* pData;

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
    } else {
        // Calculate maximum possible buffer size needed
        g_Flush.dwTempBufferSize = sizeof(CEPERF_LOGDATA_STATISTIC_DISCRETE)
                                   + sizeof(DurationDataList);
                                   + sizeof(CEPERF_DISCRETE_COUNTER_DATA);

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
        // BeginFlushCSV

        // BUGBUG want to avoid printing this if there are no Statistic items in
        // the session!  (no way to tell?)
        return FlushChars(&g_Flush,
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
// Clean up flush resources
VOID StatisticFlushEnd()
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
// Write descriptor for Statistic item into a binary stream.
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
// Write descriptor for Statistic item into a text stream.
_inline static HRESULT FlushDescriptorText(
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        return FlushChars(&g_Flush,
                          TEXT("\thStatistic=0x%08x, dwRecordingFlags=0x%08x, %s\r\n"),
                          (DWORD)hTrackedItem, pItem->dwRecordingFlags, pItem->szItemName);
    } else {  // LocalStatistic
        DEBUGCHK(pItem->type == CEPERF_TYPE_LOCALSTATISTIC);
        return FlushChars(&g_Flush,
                          TEXT("\thLocalStat=0x%08x, dwRecordingFlags=0x%08x, %s\r\n"),
                          (DWORD)hTrackedItem, pItem->dwRecordingFlags, pItem->szItemName);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write discrete data from Statistic item into the registry
static HRESULT FlushDataToRegistry(
    HKEY         hkOutput,
    TrackedItem* pItem
    )
{
    LONG   lResult;
    LPBYTE pValue;
    DWORD  dwValSize;
    StatisticData *pChangeData = NULL;
    DEBUGCHK(hkOutput != INVALID_HANDLE_VALUE);

    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        pValue = (LPBYTE) &(pItem->data.Statistic.ulValue);
        dwValSize = sizeof(ULARGE_INTEGER);
    } else {
        DEBUGCHK(pItem->type == CEPERF_TYPE_LOCALSTATISTIC);
        // Note, the RegSetValue call will fail if the caller doesn't have
        // access to the data, for security reasons.
        pValue = (LPBYTE) pItem->data.LocalStatistic.MinRecord.lpValue;
        dwValSize = pItem->data.LocalStatistic.MinRecord.dwValSize;
    }
    lResult = RegSetValueEx(hkOutput, pItem->szItemName, 0,
                            REG_BINARY, pValue, sizeof(ULARGE_INTEGER));

    // Copy short data (Skip if the Statistic was never modified)
    if ((pItem->type == CEPERF_TYPE_STATISTIC)
        && (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT)
        && (pChangeData->dwNumVals > 0)) {
        HKEY hkStatistic;

        // Need to create a key for the Statistic item, to keep its
        // short-record data separate from other items'.
        if (RegCreateKeyEx(hkOutput, pItem->szItemName, 0, NULL, 0,
#ifdef UNDER_CE
                           0,
#else
                           KEY_ALL_ACCESS,
#endif // UNDER_CE
                           NULL, &hkStatistic, NULL) == ERROR_SUCCESS) {

            CEPERF_DISCRETE_COUNTER_DATA* pData;
            
            pData = STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData);
            pChangeData = (StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData);
            RegSetValueEx(hkStatistic, TEXT("ChangeCount"), 0, REG_DWORD,
                          (LPBYTE) &(pChangeData->dwNumVals),
                          sizeof(DWORD));
            RegSetValueEx(hkStatistic, TEXT("ChangeSum"), 0, REG_BINARY,
                          (LPBYTE) &(pData->ulTotal), sizeof(ULARGE_INTEGER));
            RegSetValueEx(hkStatistic, TEXT("ChangeMax"), 0, REG_BINARY,
                          (LPBYTE) &(pData->ulMaxVal), sizeof(ULARGE_INTEGER));
            RegSetValueEx(hkStatistic, TEXT("ChangeMin"), 0, REG_BINARY,
                          (LPBYTE) &(pData->ulMinVal), sizeof(ULARGE_INTEGER));

            RegCloseKey(hkStatistic);
        }
    }

    return (HRESULT) lResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write discrete data from Statistic item into a binary stream.
static HRESULT FlushDataBinary(
    HANDLE       hTrackedItem,
    TrackedItem* pItem
    )
{
    CEPERF_LOGEVENT_HEADER* pHeader = (CEPERF_LOGEVENT_HEADER*)g_Flush.lpTempBuffer;  // Rather hackish
    
    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        CEPERF_LOGDATA_STATISTIC_DISCRETE* pOutput = (CEPERF_LOGDATA_STATISTIC_DISCRETE*)g_Flush.lpTempBuffer;

        pOutput->header.bID = CEPERF_LOGID_STATISTIC_DISCRETE;
        pOutput->header.bReserved = 0;
        pOutput->header.wSize = sizeof(CEPERF_LOGDATA_STATISTIC_DISCRETE);

        pOutput->hTrackedItem = hTrackedItem;
        pOutput->ulValue      = pItem->data.Statistic.ulValue;

        // Copy short data (Skip if the Statistic was never modified)
        if ((pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT)
            && (((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals > 0)) {

            pOutput->header.wSize += (sizeof(DurationDataList) + sizeof(CEPERF_DISCRETE_COUNTER_DATA));
            DEBUGCHK(pOutput->header.wSize <= g_Flush.dwTempBufferSize);
            memcpy(((LPBYTE)(pOutput)) + sizeof(CEPERF_LOGDATA_STATISTIC_DISCRETE),
                   (LPBYTE)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData),
                   sizeof(DurationDataList) + sizeof(CEPERF_DISCRETE_COUNTER_DATA));
        }
    } else {  // LocalStatistic
        CEPERF_LOGDATA_LOCALSTATISTIC_DISCRETE* pOutput = (CEPERF_LOGDATA_LOCALSTATISTIC_DISCRETE*)g_Flush.lpTempBuffer;
        
        DEBUGCHK(pItem->type == CEPERF_TYPE_LOCALSTATISTIC);

        pOutput->header.bID = CEPERF_LOGID_LOCALSTATISTIC_DISCRETE;
        pOutput->header.bReserved = 0;
        pOutput->header.wSize = (WORD) (sizeof(CEPERF_LOGDATA_LOCALSTATISTIC_DISCRETE) + pItem->data.LocalStatistic.MinRecord.dwValSize);

        pOutput->hTrackedItem = hTrackedItem;

        // Copy the data
        // Note, the copy will get an exception if the caller doesn't have
        // access to the data, for security reasons.
        __try {
            DEBUGCHK(pItem->dwRecordingFlags & CEPERF_LOCALSTATISTIC_RECORD_MIN);
            if (pOutput->header.wSize > g_Flush.dwTempBufferSize) {
                pOutput->header.wSize = (WORD)g_Flush.dwTempBufferSize;  // Copy what we can...
            }
            memcpy(((LPBYTE)(pOutput)) + sizeof(CEPERF_LOGDATA_LOCALSTATISTIC_DISCRETE),
                   (LPBYTE)(pItem->data.LocalStatistic.MinRecord.lpValue),
                   pOutput->header.wSize - sizeof(CEPERF_LOGDATA_LOCALSTATISTIC_DISCRETE));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
        }
    }

    // Write the buffer to the output location
    return FlushBytes(&g_Flush, (LPBYTE)g_Flush.lpTempBuffer, pHeader->wSize);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write discrete data from Statistic item into a text stream.
static HRESULT FlushDataText(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    BOOL         fDescriptorFlushed   // Simplify output if the descriptor
                                      // has already been flushed.
    )
{
    HRESULT hResult;

    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        // Write basic data
        if (!fDescriptorFlushed) {
            // Add the Statistic handle if there's no descriptor on the output already
            hResult = FlushChars(&g_Flush, TEXT("\thStatistic=0x%08x, "),
                                 (DWORD)hTrackedItem);
        } else {
            hResult = FlushChars(&g_Flush, TEXT("\t\t"));
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
        
        hResult = FlushChars(&g_Flush, TEXT("ulValue=0x%08x %08x\r\n"),
                             pItem->data.Statistic.ulValue.HighPart,
                             pItem->data.Statistic.ulValue.LowPart);
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }

        // Copy short data (Skip if the Statistic was never modified)
        if ((pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT)
            && (((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals > 0)) {

            CEPERF_DISCRETE_COUNTER_DATA* pData;
            ULARGE_INTEGER ulAverage;
            
            pData = STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData);
            
            ulAverage.QuadPart = pData->ulTotal.QuadPart;
            ulAverage.QuadPart *= 10;  // for decimal point later
            ulAverage.QuadPart /= ((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals;

            DEBUGCHK((pData->ulMaxVal.QuadPart >= (ulAverage.QuadPart / 10))
                     && (pData->ulMinVal.QuadPart <= (ulAverage.QuadPart / 10)));  // sanity check
            DEBUGCHK(pData->ulMaxVal.QuadPart >= pData->ulMinVal.QuadPart);  // sanity check
            
            hResult = FlushChars(&g_Flush,
                                 TEXT("\t\tChanges:   Count=%u, Min=%I64u, Max=%I64u, Avg=%I64u.%u\r\n"),
                                 ((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals,
                                 pData->ulMinVal.QuadPart, pData->ulMaxVal.QuadPart,
                                 (DWORD)(ulAverage.QuadPart / 10),
                                 (DWORD)(ulAverage.QuadPart % 10));
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        }

    } else {  // LocalStatistic
        DEBUGCHK(pItem->type == CEPERF_TYPE_LOCALSTATISTIC);
        
        // Write basic data
        if (!fDescriptorFlushed) {
            // Add the Statistic handle if there's no descriptor on the output already
            hResult = FlushChars(&g_Flush,
                                 TEXT("\thLocalStat=0x%08x, value=0x"),
                                 (DWORD)hTrackedItem);
        } else {
            hResult = FlushChars(&g_Flush, TEXT("\t\tvalue=0x"));
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }

        // Note, the copy will get an exception if the caller doesn't have
        // access to the data, for security reasons.
        __try {
            switch (pItem->data.LocalStatistic.MinRecord.dwValSize) {
            case sizeof(BYTE):
                hResult = FlushChars(&g_Flush, TEXT("%02x\r\n"),
                                     *((BYTE*)(pItem->data.LocalStatistic.MinRecord.lpValue)));
                break;

            case sizeof(WORD):
                hResult = FlushChars(&g_Flush, TEXT("%04x\r\n"),
                                     *((WORD*)(pItem->data.LocalStatistic.MinRecord.lpValue)));
                break;

            case sizeof(DWORD):
                hResult = FlushChars(&g_Flush, TEXT("%08x\r\n"),
                                     *((DWORD*)(pItem->data.LocalStatistic.MinRecord.lpValue)));
                break;

            case sizeof(ULARGE_INTEGER): {
                ULARGE_INTEGER* pulData = (ULARGE_INTEGER*) (pItem->data.LocalStatistic.MinRecord.lpValue);
                hResult = FlushChars(&g_Flush, TEXT("%08x %08x\r\n"),
                                     pulData->HighPart, pulData->LowPart);
                break;
            }
            
            default:
                hResult = CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
              hResult = CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }
    
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write discrete data from Statistic item into a comma-separated text stream.
static HRESULT FlushDataCSV(
    TrackedItem* pItem
    )
{
    HRESULT hResult;
    
    // Write the session name first
    if (g_Flush.pSession->szSessionName[0]) {
        hResult = FlushBytes(&g_Flush, (LPBYTE)(g_Flush.pSession->szSessionName),
                             wcslen(g_Flush.pSession->szSessionName)*sizeof(WCHAR));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }

    // Then the item name
    hResult = FlushChars(&g_Flush, TEXT(",%s"), pItem->szItemName);
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // Then the current value
    if (pItem->type == CEPERF_TYPE_STATISTIC) {
        hResult = FlushULargeCSV(&g_Flush, &(pItem->data.Statistic.ulValue));
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }

        // Copy short data (Skip if the Statistic was never modified)
        if ((pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT)
            && (((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals > 0)) {
            
            CEPERF_DISCRETE_COUNTER_DATA* pData;

            // Count
            pData = STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData);
            hResult = FlushChars(&g_Flush, TEXT(",%u"),
                                 ((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals);
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        
            // Min, max, total
            hResult = FlushULargeCSV(&g_Flush, &(pData->ulMinVal));
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
            hResult = FlushULargeCSV(&g_Flush, &(pData->ulMaxVal));
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
            hResult = FlushULargeCSV(&g_Flush, &(pData->ulTotal));
            if (hResult != ERROR_SUCCESS) {
                return hResult;
            }
        }
    
    } else {  // LocalStatistic
        // Note, the copy will get an exception if the caller doesn't have
        // access to the data, for security reasons.
        __try {
            switch (pItem->data.LocalStatistic.MinRecord.dwValSize) {
            case sizeof(BYTE):
                hResult = FlushChars(&g_Flush, TEXT(",%u"),
                                     *((BYTE*)(pItem->data.LocalStatistic.MinRecord.lpValue)));
                break;

            case sizeof(WORD):
                hResult = FlushChars(&g_Flush, TEXT(",%u"),
                                     *((WORD*)(pItem->data.LocalStatistic.MinRecord.lpValue)));
                break;

            case sizeof(DWORD):
                hResult = FlushChars(&g_Flush, TEXT(",%u"),
                                     *((DWORD*)(pItem->data.LocalStatistic.MinRecord.lpValue)));
                break;

            case sizeof(ULARGE_INTEGER):
                hResult = FlushULargeCSV(&g_Flush, (ULARGE_INTEGER*)(pItem->data.LocalStatistic.MinRecord.lpValue));
                break;
            
            default:
                FlushChars(&g_Flush, TEXT(","));
                hResult = CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            hResult = CEPERF_HR_BAD_LOCALSTATISTIC_DATA;
        }
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }

    hResult = FlushChars(&g_Flush, TEXT("\r\n"));
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write continuous data from Statistic item into a binary stream.
static HRESULT RecordContinuousBinary(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    BYTE         bCePerfLogID
    )
{
    DEBUGCHK(0);  // NYI
    
    DEBUGCHK(pItem->type == CEPERF_TYPE_STATISTIC);  // No LocalStatistic continuous data
    
    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Write continuous data from Statistic item into a text stream.
static HRESULT RecordContinuousText(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    BYTE         bCePerfLogID
    )
{
    DEBUGCHK(0);  // NYI
    
    DEBUGCHK(pItem->type == CEPERF_TYPE_STATISTIC);  // No LocalStatistic continuous data
    
    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllItems.  Write descriptor and discrete data
// to the storage location.  Clear the data if necessary.
HRESULT FlushStatistic(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    LPVOID       lpWalkParam      // Ignored
    )
{
    HRESULT hResult = ERROR_SUCCESS;

    // Will be called with Duration items too
    if ((pItem->type == CEPERF_TYPE_STATISTIC)
        || (pItem->type == CEPERF_TYPE_LOCALSTATISTIC)) {

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
            if (g_Flush.pSession->settings.dwStorageFlags & CEPERF_STORE_REGISTRY) {
                hResult = FlushDataToRegistry((HKEY)g_Flush.hOutput, pItem);
            } else if (g_Flush.pSession->settings.dwStorageFlags & CEPERF_STORE_CSV) {
                hResult = FlushDataCSV(pItem);
            } else if (g_Flush.pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                // Binary output
                hResult = FlushDataBinary(hTrackedItem, pItem);
            } else {
                // Text output
                hResult = FlushDataText(hTrackedItem, pItem,
                                        (g_Flush.dwFlushFlags & CEPERF_FLUSH_DESCRIPTORS) ? TRUE : FALSE);
            }
        }
    
        // Finally clear the data if necessary
        if (g_Flush.dwFlushFlags & CEPERF_FLUSH_AND_CLEAR) {
            if (pItem->type == CEPERF_TYPE_STATISTIC) {
                memset(&pItem->data.Statistic.ulValue, 0, sizeof(ULARGE_INTEGER));
                if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT) {
                    CEPERF_DISCRETE_COUNTER_DATA* pData;
    
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
        if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_FULL) {
            // Full-record writes continuous data to the storage location
            if (!(pSession->settings.dwStatusFlags & CEPERF_STATUS_STORAGE_DISABLED)) {
                if (pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                    // Binary output
                    hResult = RecordContinuousBinary(hTrackedItem, pItem,
                                                     CEPERF_LOGID_STATISTIC_ADD);
                } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_TEXT) {
                    // Text output
                    hResult = RecordContinuousText(hTrackedItem, pItem,
                                                   CEPERF_LOGID_STATISTIC_ADD);
                }
            }
        } else if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT) {
            // Short-record keeps some additional discrete data
           ((StatisticData *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals++;
            UpdateDurationDataDW(STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData),
                                 dwValue);
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
    
    DEBUGMSG(ZONE_API && ZONE_STATISTIC, (TEXT("-CePerfAddStatistic (%u)\r\n"), hResult));
    
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
    HRESULT        hResult = ERROR_SUCCESS;
    TrackedItem*   pItem;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_STATISTIC,
             (TEXT("+CePerfSetStatistic(0x%08x, %u)\r\n"),
              hTrackedItem, dwValue));

    if (ITEM_PROLOG(&pSession, &pItem, hTrackedItem, CEPERF_TYPE_STATISTIC, &hResult)) {
            
        // Update the value
        pItem->data.Statistic.ulValue.QuadPart = dwValue;

        // Do any additional work for this recording mode
        if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_FULL) {
            // Full-record writes continuous data to the storage location
            if (!(pSession->settings.dwStatusFlags & CEPERF_STATUS_STORAGE_DISABLED)) {
                if (pSession->settings.dwStorageFlags & CEPERF_STORE_BINARY) {
                    // Binary output
                    hResult = RecordContinuousBinary(hTrackedItem, pItem,
                                                     CEPERF_LOGID_STATISTIC_SET);
                } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_TEXT) {
                    // Text output
                    hResult = RecordContinuousText(hTrackedItem, pItem,
                                                   CEPERF_LOGID_STATISTIC_SET);
                }
            }
        }
        // BUGBUG Should short-record update change data with delta?
        
        ITEM_EPILOG(pSession);
    }
    
    DEBUGMSG(ZONE_API && ZONE_STATISTIC, (TEXT("-CePerfSetStatistic (%u)\r\n"), hResult));
    
    return hResult;
}

