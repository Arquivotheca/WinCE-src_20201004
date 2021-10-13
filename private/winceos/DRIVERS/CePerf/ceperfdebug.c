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
//      ceperfdebug.c
//  
//  Abstract:  
//
//      Implements debugging routines for the Windows CE
//      Performance Measurement API.
//      
//------------------------------------------------------------------------------

#include <windows.h>
#include "ceperf.h"
#include "ceperf_cpu.h"
#include "pceperf.h"


#ifdef DEBUG

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID DumpVariableStructs(
    WORD wNumCPUCounters,       // Number of individual counters
    WORD wTotalCPUCounterSize   // Total counter data, in bytes
    )
{
    DWORD dwBytesNeeded;

    DEBUGMSG(1, (TEXT("For CPU counter size %ux%u:\r\n"),
                 wNumCPUCounters, wTotalCPUCounterSize));

    // Figure out how much RAM is required for the begin list
    dwBytesNeeded = sizeof(DurationBeginList)
                    + (sizeof(DurationBeginCounters) + wTotalCPUCounterSize) * CEPERF_DURATION_FRAME_LIMIT;
    DEBUGMSG(1, (TEXT("  Max Duration BeginList          = %u\r\n"), dwBytesNeeded));

    // Figure out how much RAM is required for Duration min-record deltas
    dwBytesNeeded = 1                   // perf count
                    + 1                 // tick count
                    + wNumCPUCounters;  // CPU counters
    dwBytesNeeded *= sizeof(DiscreteCounterData);
    dwBytesNeeded += sizeof(DurationDataList);
    DEBUGMSG(1, (TEXT("  Max Duration MinRecord deltas   = %u\r\n"), dwBytesNeeded));

    // Figure out how much RAM is required for a Statistic short-record delta
    dwBytesNeeded = sizeof(DurationDataList) + sizeof(DiscreteCounterData);
    DEBUGMSG(1, (TEXT("  Max Statistic ShortRecord delta = %u\r\n"), dwBytesNeeded));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID DumpMemorySizes()
{
    TypeData temp;
    DEBUGMSG(1, (TEXT("MEMORY SIZE DUMP:\r\n")));
    DEBUGMSG(1, (TEXT("  Items in 1st page: %u  following: %u\r\n"),
                 MAX_ITEMS_ON_FIRST_PAGE, MAX_ITEMS_ON_OTHER_PAGES));
    DEBUGMSG(1, (TEXT("  Max # sessions: %u  items: %u\r\n"),
                 MAX_SESSION_COUNT, MAX_ITEM_COUNT));
    DEBUGMSG(1, (TEXT("  sizeof(CePerfMaster)=%u\r\n"), sizeof(CePerfMaster)));
    DEBUGMSG(1, (TEXT("  sizeof(DLLSessionTable)=%u\r\n"), sizeof(DLLSessionTable)));
    DEBUGMSG(1, (TEXT("  sizeof(SessionHeader)=%u  .settings=%u\r\n"),
                 sizeof(SessionHeader), sizeof(SessionSettings)));
    DEBUGMSG(1, (TEXT("  sizeof(ItemPageHeader)=%u\r\n"), sizeof(ItemPageHeader)));
    DEBUGMSG(1, (TEXT("  sizeof(TrackedItem)=%u\r\n"), sizeof(TrackedItem)));
    DEBUGMSG(1, (TEXT("  sizeof(TypeData)=%u  .Duration=%u  .Statistic=%u  .LocalStatistic=%u\r\n"),
                 sizeof(temp), sizeof(temp.Duration), sizeof(temp.Statistic), sizeof(temp.LocalStatistic)));
    DEBUGMSG(1, (TEXT("  sizeof(DurationBeginList)=%u\r\n"), sizeof(DurationBeginList)));
    DEBUGMSG(1, (TEXT("  sizeof(DurationBeginCounters)=%u\r\n"), sizeof(DurationBeginCounters)));
    DEBUGMSG(1, (TEXT("  sizeof(DurationDataList)=%u\r\n"), sizeof(DurationDataList)));
    DEBUGMSG(1, (TEXT("  sizeof(DiscreteCounterData)=%u\r\n"), sizeof(DiscreteCounterData)));

    DumpVariableStructs(0, 0);                // No counters
    DumpVariableStructs(1, sizeof(DWORD));    // One DWORD counter
    DumpVariableStructs(2, 2*sizeof(LARGE_INTEGER));  // Typical XScale
    DumpVariableStructs(3, 3*sizeof(LARGE_INTEGER));  // XScale max
    DumpVariableStructs(6, 6*sizeof(DWORD));  // Broadcom max
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID DumpMasterInfo()
{
    DWORD i, dwMaskNum;

    if (g_hMasterMutex) {
        DEBUGMSG(1, (TEXT("  Master mutex:  h=0x%08x\r\n"), g_hMasterMutex));
    } else {
        DEBUGMSG(1, (TEXT("  Master mutex:  DNE\r\n")));
    }
    
    if (!g_hMasterMap && !g_pMaster) {
        DEBUGMSG(1, (TEXT("  Master map:    DNE\r\n")));
    } else {
        DEBUGMSG(1, (TEXT("  Master map:    h=0x%08x  p=0x%08x\r\n"), g_hMasterMap, g_pMaster));

        if (g_pMaster) {
            DEBUGMSG(1, (TEXT("  dwGlobalFlags=0x%08x\r\n"), g_pMaster->dwGlobalFlags));
            DEBUGMSG(1, (TEXT("  Master Page Table:\r\n")));
            DEBUGMSG(1, (TEXT("    MaskNum  Address Range          Mask\r\n")));
            for (dwMaskNum = 0; dwMaskNum < PAGE_MASK_LEN; dwMaskNum++) {
                DEBUGMSG(1, (TEXT("    %4u     0x%08x-0x%08x  0x%08x\r\n"),
                             dwMaskNum,
                             (LPBYTE)g_pMaster + dwMaskNum * PGSIZE * 32,
                             (LPBYTE)g_pMaster + (dwMaskNum+1) * PGSIZE * 32 - 1,
                             g_pMaster->rgdwPageMask[dwMaskNum]));
            }
            
            // BUGBUG pDurBLHead, pDurDataHead
            // BUGBUG CPU perf counter info
            
            DEBUGMSG(1, (TEXT("  Master Session Table:\r\n")));
            DEBUGMSG(1, (TEXT("    First free: %u\r\n"), g_pMaster->wFirstFree));
            DEBUGMSG(1, (TEXT("    Max alloc:  %u\r\n"), g_pMaster->wMaxAlloc));
            DEBUGMSG(1, (TEXT("    HNum  SessionRef  hSession    pSession    pQuick      NextFree\r\n")));
            for (i = 0; i <= g_pMaster->wMaxAlloc; i++) {
                DWORD dwSessionRef = g_pMaster->rgSessionRef[i];
                DWORD dwVersion = MASTER_VERSION(dwSessionRef);
                if (MASTER_ISFREE(dwSessionRef)) {
                    // This handle is free
                    DEBUGMSG(1, (TEXT("    %4u  0x%08x  0x%08x  0x%08x  0x%08x  %u\r\n"),
                                 i, dwSessionRef,
                                 MAKE_SESSION_HANDLE(dwVersion, i), NULL,
                                 MASTER_SESSIONPTR(dwSessionRef),
                                 MASTER_NEXTFREE(dwSessionRef)));
                } else {
                    // This handle is allocated
                    DEBUGMSG(1, (TEXT("    %4u  0x%08x  0x%08x  0x%08x\r\n"),
                                 i, dwSessionRef,
                                 MAKE_SESSION_HANDLE(dwVersion, i),
                                 MASTER_SESSIONPTR(dwSessionRef)));
                }
            }
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID DumpDLLSessionTable()
{
    DLLSessionTable* pTable;
    WORD  i;
    DWORD dwCount = 0;  // Validate running total of DLL sessions
    
    DEBUGMSG(1, (TEXT("  DLL Session Count=0x%08x\r\n"), g_dwDLLSessionCount));
    DEBUGMSG(1, (TEXT("  DLL Session Table:\r\n")));
    for (pTable = g_pDLLSessions; pTable; pTable = pTable->pNext ){
        DEBUGMSG(1, (TEXT("  0x%08x: pNext=0x%08x\r\n"), pTable, pTable->pNext));
        for (i = 0; i < TABLE_COUNT; i++) {
            if (pTable->rghSession[i] != INVALID_HANDLE_VALUE) {
                DEBUGMSG(1, (TEXT("    %4u  hSession=0x%08x  dwRefCount=0x%08x\r\n"),
                             i, pTable->rghSession[i], pTable->rgdwRefCount[i]));
                dwCount += pTable->rgdwRefCount[i];
            } else {
                DEBUGMSG(1, (TEXT("    %4u  hSession=0x%08x\r\n"),
                             i, pTable->rghSession[i]));
            }
        }
    }
    
    if (dwCount != g_dwDLLSessionCount) {
        DEBUGMSG(1, (TEXT("  !! Session count=0x%08x does not match DLL session count\r\n"),
                     dwCount));
        DEBUGCHK(0);
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID DumpSession(
    HANDLE hSession,
    BOOL   fDumpItems,  // Dump descriptors for all items
    BOOL   fDumpData    // Dump data for all items
    )
{
    SessionHeader* pSession;
    
    pSession = LookupSessionHandle(hSession);
    if (pSession) {
        DEBUGMSG(1, (TEXT("hSession=0x%08x, pSession=0x%08x  %s\r\n"),
                     hSession, pSession, pSession->szSessionName));
        DEBUGMSG(1, (TEXT("  dwRefCount    = 0x%08x\r\n"), pSession->dwRefCount));
        DEBUGMSG(1, (TEXT("  Settings:\r\n")));
        DEBUGMSG(1, (TEXT("    dwStatusFlags   = 0x%08x\r\n"), pSession->settings.dwStatusFlags));
        DEBUGMSG(1, (TEXT("    dwStorageFlags  = 0x%08x\r\n"), pSession->settings.dwStorageFlags));
        DEBUGMSG(1, (TEXT("    szStoragePath   = %s\r\n"), pSession->settings.szStoragePath));
        DEBUGMSG(1, (TEXT("    Duration Flags  = 0x%08x\r\n"), pSession->settings.rgdwRecordingFlags[CEPERF_TYPE_DURATION]));
        DEBUGMSG(1, (TEXT("    Statistic Flags = 0x%08x\r\n"), pSession->settings.rgdwRecordingFlags[CEPERF_TYPE_STATISTIC]));
        DEBUGMSG(1, (TEXT("    LocStat Flags   = 0x%08x\r\n"), pSession->settings.rgdwRecordingFlags[CEPERF_TYPE_LOCALSTATISTIC]));
        DEBUGMSG(1, (TEXT("  dwNumObjects  = 0x%08x\r\n"), pSession->dwNumObjects));

        if (fDumpItems || fDumpData) {
            DumpSessionItemList(hSession, pSession, fDumpData);
        } else {
            DEBUGMSG(1, (TEXT("  pNextPage     = 0x%08x\r\n"), (DWORD)GET_MAP_POINTER(pSession->items.offsetNextPage)));
            DEBUGMSG(1, (TEXT("    wStartIndex = 0x%04x\r\n"), pSession->items.wStartIndex));
            DEBUGMSG(1, (TEXT("    wFirstFree  = 0x%04x\r\n"), pSession->items.wFirstFree));
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Dump descriptors and maybe data for all items
VOID DumpSessionItemList(
    HANDLE hSession,
    SessionHeader*  pSession,
    BOOL fDumpData
    )
{
    ItemPageHeader* pHeader;
    TrackedItem*    pItem;
    DWORD           dwIndex, dwPageIndex;
    DWORD           dwItemsFound;
    
    // NOTENOTE not using WalkAllItems because this function dumps the free
    // items in addition to the allocated ones.
    
    if (pSession) {
        DEBUGMSG(1, (TEXT("  pNextPage     = 0x%08x\r\n"), (DWORD)GET_MAP_POINTER(pSession->items.offsetNextPage)));
        DEBUGMSG(1, (TEXT("    wStartIndex = 0x%04x\r\n"), pSession->items.wStartIndex));
        DEBUGMSG(1, (TEXT("    wFirstFree  = 0x%04x\r\n"), pSession->items.wFirstFree));

        dwIndex = 0;        // Index overall within the session
        dwPageIndex = 0;    // Index on the current page
        dwItemsFound = 0;   // Number of in-use items found

        pItem = SESSION_PFIRSTITEM(pSession);
        pHeader = NULL;

        // Stop iterating at first page boundary after finding the last allocated item
        while ((dwItemsFound < pSession->dwNumObjects)
               || (((dwIndex - MAX_ITEMS_ON_FIRST_PAGE) % MAX_ITEMS_ON_OTHER_PAGES) != 0)) {

            DEBUGCHK(dwIndex < MAX_ITEM_COUNT);
            if (!TRACKEDITEM_ISFREE(pItem)) {
                // Item is in use
                dwItemsFound++;
                DumpTrackedItem(MAKE_ITEM_HANDLE(hSession, dwIndex), pItem, (LPVOID) FALSE);
            } else {
                // Item is free
                DEBUGMSG(1, (TEXT("  hTrackedItem=0x%08x, pTrackedItem=0x%08x  (FREE)  wNextFree=0x%02x\r\n"),
                             MAKE_ITEM_HANDLE(hSession, dwIndex), pItem,
                             TRACKEDITEM_NEXTFREE(pItem)));
            }
            DEBUGCHK(dwItemsFound <= pSession->dwNumObjects);

            // Increment counters and pointer
            dwIndex++;
            dwPageIndex++;
            pItem++;

            // Deal with page wrap
            if (dwIndex <= MAX_ITEMS_ON_FIRST_PAGE) {
                // First page
                if (dwPageIndex >= MAX_ITEMS_ON_FIRST_PAGE) {
                    // Go to the next page
                    dwPageIndex = 0;

                    pHeader = (ItemPageHeader *)GET_MAP_POINTER(pSession->items.offsetNextPage);
                    if (!pHeader) {
                        DEBUGCHK(0);
                        return;
                    }
                    
                    DEBUGMSG(1, (TEXT("hSession=0x%08x, pItemPageHeader=0x%08x\r\n"),
                                 hSession, pHeader));
                    DEBUGMSG(1, (TEXT("  wStartIndex=0x%04x, wFirstFree=0x%04x\r\n"),
                                 pHeader->wStartIndex, pHeader->wFirstFree));

                    DEBUGCHK(dwIndex == pHeader->wStartIndex + (DWORD) 0);
                    pItem = ITEMPAGE_PFIRSTITEM(pHeader);
                } else {
                    DEBUGCHK(dwIndex == dwPageIndex);
                }
            } else {
                // Following page
                if (dwPageIndex >= MAX_ITEMS_ON_OTHER_PAGES) {
                    // Go to the next page
                    dwPageIndex = 0;
                    pHeader = (ItemPageHeader*)GET_MAP_POINTER(pHeader->offsetNextPage);
                    if (!pHeader) {
                        // Should only happen when we're done iterating
                        DEBUGCHK((dwItemsFound == pSession->dwNumObjects)
                                 && (((dwIndex - MAX_ITEMS_ON_FIRST_PAGE) % MAX_ITEMS_ON_OTHER_PAGES) == 0));
                        return;
                    }
                     
                    DEBUGMSG(1, (TEXT("hSession=0x%08x, pItemPageHeader=0x%08x\r\n"),
                                 hSession, pHeader));
                    DEBUGMSG(1, (TEXT("  wStartIndex=0x%04x, wFirstFree=0x%04x\r\n"),
                                 pHeader->wStartIndex, pHeader->wFirstFree));

                    DEBUGCHK(dwIndex == pHeader->wStartIndex + (DWORD) 0);
                    pItem = ITEMPAGE_PFIRSTITEM(pHeader);
                } else {
                    DEBUGCHK(dwIndex == pHeader->wStartIndex + dwPageIndex);
                }
            }
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID DumpDurationData(
    DiscreteCounterData* pData,
    DWORD dwNumVals
    )
{
    double dStdDev;
    
    DEBUGMSG(1, (TEXT("      ulMin          = 0x%08x %08x\r\n"),
                 pData->ulMinVal.HighPart, pData->ulMinVal.LowPart));
    DEBUGMSG(1, (TEXT("      ulMax          = 0x%08x %08x\r\n"),
                 pData->ulMaxVal.HighPart, pData->ulMaxVal.LowPart));
    DEBUGMSG(1, (TEXT("      ulTotal        = 0x%08x %08x\r\n"),
                 pData->ulTotal.HighPart, pData->ulTotal.LowPart));
    DEBUGMSG(1, (TEXT("      dwAverage      = 0x%08x\r\n"),
                 (DWORD) (pData->ulTotal.QuadPart / dwNumVals)));
    
    dStdDev = GetStandardDeviation(pData, dwNumVals);
    DEBUGMSG(1, (TEXT("      dStdDev        = %.1lf\r\n"), dStdDev));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID DumpCPUPerfCounters(
    LPBYTE pData
    )
{
    WORD wCounter;

    for (wCounter = 0;
         (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
         wCounter++) {

        // Different case for each data size
        switch (g_pMaster->CPU.rgbCounterSize[wCounter]) {
        case sizeof(BYTE):
            DEBUGMSG(1, (TEXT("        CPU Ctr %02x: 0x%02x\r\n"),
                         wCounter, *((BYTE*)pData)));
            break;
        
        case sizeof(WORD):
            DEBUGMSG(1, (TEXT("        CPU Ctr %02x: 0x%04x\r\n"),
                         wCounter, *((WORD*)pData)));
            break;
        
        case sizeof(DWORD):
            DEBUGMSG(1, (TEXT("        CPU Ctr %02x: 0x%08x\r\n"),
                         wCounter, *((DWORD*)pData)));
            break;
        
        case sizeof(ULARGE_INTEGER):
            {
                ULARGE_INTEGER* pulData = (ULARGE_INTEGER*) pData;
                DEBUGMSG(1, (TEXT("        CPU Ctr %02x: 0x%08x %08x\r\n"),
                             wCounter, pulData->HighPart, pulData->LowPart));
                break;
            }
        
        default:
            {
                WORD wByte;

                // Bytewise dump
                DEBUGMSG(1, (TEXT("        CPU Ctr %02x:"), wCounter));
                for (wByte = 0; wByte < g_pMaster->CPU.rgbCounterSize[wCounter]; wByte++) {
                    DEBUGMSG(1, (TEXT(" %02x"), pData[wByte]));
                }
                DEBUGMSG(1, (TEXT("\r\n")));
                break;
            }
        }
        
        pData += g_pMaster->CPU.rgbCounterSize[wCounter];
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID DumpDurationBeginList(
    DWORD dwItemRecordingFlags,
    DurationBeginList* pBL
    )
{
    DurationBeginCounters* pCounters;
    WORD wTotalCounterSize = g_pMaster->CPU.wTotalCounterSize;  // Prevent asynchronous changes to g_pMaster
    BYTE bFrame;

    DEBUGMSG(1, (TEXT("    pBeginList=0x%08x:\r\n"), pBL));
    DEBUGMSG(1, (TEXT("        FrameHigh=%u\r\n"), pBL->bFrameHigh));

    DEBUGCHK(pBL->bFrameHigh < CEPERF_DURATION_FRAME_LIMIT);
    for (bFrame = 0; bFrame < CEPERF_DURATION_FRAME_LIMIT; bFrame++) {

        pCounters = BEGINLIST_PCOUNTERS(pBL, bFrame, wTotalCounterSize);

        if (bFrame <= pBL->bFrameHigh) {
            DEBUGMSG(1, (TEXT("        [%u] hThread     = 0x%08x\r\n"),
                         bFrame, pCounters->hThread));

            // Dump frame if it's in use
            if (pCounters->hThread != NULL) {
                // Dump absolute perf count if it's being tracked
                if (dwItemRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
                    DEBUGMSG(1, (TEXT("        Abs Perf Count:   0x%08x %08x\r\n"),
                                 pCounters->liPerfCount.HighPart,
                                 pCounters->liPerfCount.LowPart));
                }

                // Dump per-thread tick count if it's being tracked
                if (dwItemRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT) {
                    DEBUGMSG(1, (TEXT("        Per-Thread Ticks: 0x%08x %08x\r\n"),
                                 pCounters->liThreadTicks.HighPart,
                                 pCounters->liThreadTicks.LowPart));
                }

                // Dump absolute tick count if it's being tracked
                if (dwItemRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
                    DEBUGMSG(1, (TEXT("        Abs Tick Count:   0x%08x\r\n"),
                                 pCounters->dwTickCount));
                }

                // Followed by CPU performance counter data
                if ((dwItemRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
                    && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {
                    DumpCPUPerfCounters(BEGINCOUNTERS_PCPU(pCounters));
                }
            }
        } else {
            // Beyond bFrameHigh, sanity check
            DEBUGCHK(pCounters->hThread == NULL);
        }
    }

    // Walk to the next list on the chain
    if (pBL->offsetNextDBL != 0) {
        DEBUGCHK(dwItemRecordingFlags & CEPERF_DURATION_RECORD_UNLIMITED);
        DumpDurationBeginList(dwItemRecordingFlags, (DurationBeginList *)GET_MAP_POINTER(pBL->offsetNextDBL));
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Can be used standalone or as a helper function to pass to WalkAllItems.
HRESULT DumpTrackedItem(
    HANDLE       hTrackedItem,
    TrackedItem* pItem,
    LPVOID       lpWalkParam
    )
{
    BOOL fDumpData = (BOOL) lpWalkParam;
    WORD wCounter;    
        
    DEBUGCHK(pItem);
    if (pItem) {
        DEBUGMSG(1, (TEXT("  hTrackedItem=0x%08x, pTrackedItem=0x%08x  %s\r\n"),
                     hTrackedItem, pItem, pItem->szItemName));
        
        DEBUGMSG(1, (TEXT("    dwRefCount       = 0x%08x\r\n"), pItem->dwRefCount));
        DEBUGMSG(1, (TEXT("    type             = %u\r\n"),     pItem->type));
        DEBUGMSG(1, (TEXT("    dwRecordingFlags = 0x%08x\r\n"), pItem->dwRecordingFlags));

        if (fDumpData) {
            DEBUGMSG(1, (TEXT("  DATA:\r\n")));
            switch (pItem->type) {
            case CEPERF_TYPE_DURATION: {
                DurationDataList *pDL = (DurationDataList *)GET_MAP_POINTER(pItem->data.Duration.MinRecord.offsetDL);        
                // Begin list
                DumpDurationBeginList(pItem->dwRecordingFlags,
                                      (DurationBeginList *)GET_MAP_POINTER(pItem->data.Duration.offsetBL));
                
                // Discrete data
                DEBUGMSG(1, (TEXT("    dwErrorCount: %u\r\n"),
                             pItem->data.Duration.dwErrorCount));
                if (pItem->dwRecordingFlags & CEPERF_DURATION_RECORD_MIN) {
                    DiscreteCounterData* pData;
                    
                    DEBUGMSG(1, (TEXT("    dwNumVals = 0x%08x\r\n"),
                                 pDL->dwNumVals));
                    
                    pData = DATALIST_PDATA(pDL);
                    
                    // Absolute perf count will come first
                    if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_PERFCOUNT) {
                        DEBUGMSG(1, (TEXT("    Abs Performance Counter Delta:\r\n")));
                        // BUGBUG need to convert to microseconds instead of perf counter ticks
                        DumpDurationData(pData, pDL->dwNumVals);
                        pData++;
                    }
                    
                    // Per-thread tick count will come next
                    if (pItem->dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT) {
                        DEBUGMSG(1, (TEXT("    Per-Thread Ticks Delta:\r\n")));
                        DumpDurationData(pData, pDL->dwNumVals);
                        pData++;
                    }
                    
                    // Then absolute tick count
                    if (pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_TICKCOUNT) {
                        DEBUGMSG(1, (TEXT("    Abs Tick Count Delta:\r\n")));
                        DumpDurationData(pData, pDL->dwNumVals);
                        pData++;
                    }
                    
                    // Followed by CPU performance counter data
                    if ((pItem->dwRecordingFlags & CEPERF_RECORD_ABSOLUTE_CPUPERFCTR)
                        && (g_pMaster->dwGlobalFlags & MASTER_CPUPERFCTR_ENABLED)) {
                        // Dump DurationData for each perf counter
                        for (wCounter = 0;
                             (wCounter < g_pMaster->CPU.wNumCounters) && (wCounter < CEPERF_MAX_CPUCOUNTER_COUNT);
                             wCounter++) {
                            DEBUGMSG(1, (TEXT("    CPU Counter %02x Delta:\r\n"), wCounter));
                            DumpDurationData(pData, pDL->dwNumVals);
                            pData++;
                        }
                    }
                
                } else {
                    // No data to print for full & short record mode
                    DEBUGMSG(1, (TEXT("    none\r\n")));
                }
                break;
            }
            case CEPERF_TYPE_STATISTIC:
                DEBUGMSG(1, (TEXT("    ulValue          = 0x%08x %08x\r\n"),
                             pItem->data.Statistic.ulValue.HighPart,
                             pItem->data.Statistic.ulValue.LowPart));

                if (pItem->dwRecordingFlags & CEPERF_STATISTIC_RECORD_SHORT) {
                    DEBUGMSG(1, (TEXT("    dwNumVals = 0x%08x\r\n"),
                                 ((DurationDataList *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals));
                    DEBUGMSG(1, (TEXT("    Change Data:\r\n")));
                    DumpDurationData(STATDATA_PDATA(pItem->data.Statistic.ShortRecord.offsetChangeData),
                                    ((DurationDataList *)GET_MAP_POINTER(pItem->data.Statistic.ShortRecord.offsetChangeData))->dwNumVals);
                }
                break;
            
            case CEPERF_TYPE_LOCALSTATISTIC:
                DEBUGMSG(1, (TEXT("    lpValue          = 0x%08x\r\n"),
                             pItem->data.LocalStatistic.MinRecord.lpValue));
                DEBUGMSG(1, (TEXT("    dwValSize        = 0x%08x\r\n"),
                             pItem->data.LocalStatistic.MinRecord.dwValSize));
                break;
            
            default:
                // Invalid type
                DEBUGMSG(1, (TEXT("    none\r\n")));
                break;
            }
        }
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID DumpList(
    DLLListFuncs* pListFuncs,       // List operators
    WORD wListItemSize,             // List item size
    DWORD* poffsetListHeader   // List head pointer
    )
{
    ListPageHeader* pHeader;
    LPBYTE          pItem;
    DWORD           dwIndex;
    WORD            wPageIndex, wPageMax;
    
    dwIndex = 0;        // Index overall within the list
    wPageMax = MAX_LIST_ITEMS_PER_PAGE(wListItemSize);

    pHeader = (ListPageHeader *)GET_MAP_POINTER(*poffsetListHeader);
    while (pHeader) {

        pItem = LISTPAGE_PFIRSTOBJECT(pHeader);
        
        DEBUGMSG(1, (TEXT("pListPageHeader=0x%08x\r\n"), pHeader));
        DEBUGMSG(1, (TEXT("  wStartIndex=0x%04x, wFirstFree=0x%02x\r\n"),
                     pHeader->wStartIndex, pHeader->wFirstFree));
        
        // BUGBUG this check will fail if a page is deallocated from the middle
        DEBUGCHK(pHeader->wStartIndex == dwIndex);
        
        for (wPageIndex = 0; wPageIndex < wPageMax; wPageIndex++) {
            if (pListFuncs->pIsItemFree(pItem)) {
                DEBUGMSG(1, (TEXT("  0x%02x  pListItem=0x%08x  (FREE)  wNextFree=0x%02x\r\n"),
                             wPageIndex, pItem, pListFuncs->pGetNextFreeItem(pItem)));
            } else {
                DEBUGMSG(1, (TEXT("  0x%02x  pItem=0x%08x  (INUSE)\r\n"),
                             wPageIndex, pItem));
            }
        
            // Increment counters and pointer
            dwIndex++;
            pItem += wListItemSize;
        }
        
        pHeader = (ListPageHeader*)GET_MAP_POINTER(pHeader->offsetNextPage);
    }
}

#endif  // DEBUG

