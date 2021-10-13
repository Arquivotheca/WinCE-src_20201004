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
//      session.c
//  
//  Abstract:  
//
//      Implements session management for the Windows CE
//      Performance Measurement API.
//      
//------------------------------------------------------------------------------

#include <windows.h>
#include "ceperf.h"
#include "ceperf_cpu.h"
#include "ceperf_log.h"
#include "pceperf.h"

#ifdef UNDER_CE
#include <celog.h>
#endif


static HRESULT CleanupSession(HANDLE hSession, SessionHeader* pSession);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Update master info after a CPU counter change.  BeginList and DataList items
// must be resized after this call.
static HRESULT UpdateMasterCPUCounters(
    CePerfMaster* pMaster,
    DWORD         dwCPUFlags
    )
{
    HRESULT hResult = ERROR_SUCCESS;
    WORD    wCounter;
    
    // Copy the counter information into the master page so that all processes
    // have access to it
    if (dwCPUFlags & CEPERF_CPU_ENABLE) {
        if (pCPUCounterDescriptor
            && pCPUCounterDescriptor[0]
            && (pCPUCounterDescriptor[0] <= CEPERF_MAX_CPUCOUNTER_COUNT)) {

            // The master map could be modified asynchronously while we're
            // validating, but this validation is only for convenience (more
            // helpful failure if someone passes invalid info by accident).
            // The master map settings will be validated each time they're used.

            pMaster->dwGlobalFlags |= MASTER_CPUPERFCTR_ENABLED;
            pMaster->CPU.wNumCounters = (WORD) pCPUCounterDescriptor[0];
            pMaster->CPU.wTotalCounterSize = 0;
            
            // Calculate the amount of space needed to store a set of counters
            for (wCounter = 1;
                 (wCounter <= pCPUCounterDescriptor[0]) && (hResult == ERROR_SUCCESS);
                 wCounter++) {

                if (pCPUCounterDescriptor[wCounter] > CEPERF_MAX_CPUCOUNTER_SIZE) {
                    // Individual perf counter is too large
                    hResult = CEPERF_HR_NOT_SUPPORTED;
                    goto error;
                } else if (pMaster->CPU.wTotalCounterSize + pCPUCounterDescriptor[wCounter] > CEPERF_MAX_CPUCOUNTER_TOTAL) {
                    // Total set of perf counters is too large
                    hResult = CEPERF_HR_NOT_ENOUGH_MEMORY;
                    goto error;
                } else {
                    pMaster->CPU.rgbCounterSize[wCounter-1] = (BYTE) pCPUCounterDescriptor[wCounter];
                    pMaster->CPU.wTotalCounterSize += pMaster->CPU.rgbCounterSize[wCounter-1];
                }
            }
            // Round up to the nearest 8-byte boundary
            pMaster->CPU.wTotalCounterSize = (pMaster->CPU.wTotalCounterSize + sizeof(LARGE_INTEGER)-1) & ~(sizeof(LARGE_INTEGER)-1);
        
        } else {
            // CePerf is unable to store the data from the perf counters
            hResult = CEPERF_HR_NOT_SUPPORTED;
        }
    }
    
error:
    if ((dwCPUFlags & CEPERF_CPU_DISABLE) || (hResult != ERROR_SUCCESS)) {
        // Disabled -- no counters
        pMaster->dwGlobalFlags &= ~MASTER_CPUPERFCTR_ENABLED;
        pMaster->CPU.wNumCounters = 0;
        pMaster->CPU.wTotalCounterSize = 0;
        memset(pMaster->CPU.rgbCounterSize, 0, CEPERF_MAX_CPUCOUNTER_COUNT*sizeof(BYTE));
    }
    
    // BeginList and DataList items contain counter data, so their sizes
    // may be changing
    pMaster->list.wDurBLSize = sizeof(DurationBeginList)
                               + sizeof(DurationBeginCounters) * CEPERF_DURATION_FRAME_LIMIT
                               + pMaster->CPU.wTotalCounterSize * CEPERF_DURATION_FRAME_LIMIT;
    pMaster->list.wDurDLSize = sizeof(DurationDataList)
                               + (NUM_BUILTIN_COUNTERS + pMaster->CPU.wNumCounters) * sizeof(DiscreteCounterData);

    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static HRESULT OpenMasterMap()
{
    HRESULT hResult = ERROR_SUCCESS;
    HANDLE  hMutex = NULL;
    HANDLE  hMap = NULL;
    CePerfMaster* pMaster = NULL;
    BOOL    fFirst = FALSE;

    DEBUGMSG(ZONE_MEMORY, (TEXT("+OpenMasterMap\r\n")));

    if (ZONE_BREAK) {
        DEBUGCHK(0);
    }
    if (ZONE_MEMORY && ZONE_VERBOSE) {
        DumpMemorySizes();
    }


    //
    // First initialize the mutex so we can use it to avoid race conditions
    //

    SetLastError(0);
    hMutex = CreateMutex(NULL, TRUE, MASTER_MUTEX_NAME);
    if (!hMutex) {
        hResult = GetLastError();
        hResult = HRESULT_FROM_WIN32(hResult);
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Unable to create master mutex (0x%08x)\r\n"), hResult));
        goto exit;
    }
    
    // Use the existence of the mutex to determine whether this is the first
    // time the master map is being created.
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        DEBUGMSG(ZONE_LOCK, (TEXT("+CePerf Lock (create)\r\n")));
        fFirst = TRUE;
    } else {
        WaitForSingleObject(hMutex, INFINITE);
    }
    
    // We now own the mutex
    __try {

        //
        // Open the master map
        //
    
        // Reserve the full amount of VM; pages will be committed only when they
        // are used and decommitted when they are freed
    
        SetLastError(0);
        hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                 CEPERF_MAP_SIZE, MASTER_MAP_NAME);
        if (!hMap) {
            hResult = GetLastError();
            hResult = HRESULT_FROM_WIN32(hResult);
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Unable to create master map (0x%08x)\r\n"), hResult));
            goto exit;
        }
        // Did the mapping already exist?
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            DEBUGCHK(fFirst);
            fFirst = TRUE;
        }
    
        pMaster = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, CEPERF_MAP_SIZE);
        if (!pMaster) {
            hResult = GetLastError();
            hResult = HRESULT_FROM_WIN32(hResult);
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Unable to view master map (0x%08x)\r\n"), hResult));
            goto exit;
        }
    
        DEBUGMSG(ZONE_MEMORY, (TEXT("CePerf: pMaster=0x%08x reserved=0x%08x\r\n"),
                               pMaster, CEPERF_MAP_SIZE));
    
        
        //
        // Initialize the map if it is new
        //
    
        if (fFirst) {
            WORD i;
            
            DEBUGMSG(ZONE_MEMORY, (TEXT("CePerf: Commit page 0x%08x (master)\r\n"),
                                   pMaster));
    
            pMaster->dwGlobalFlags = 0;
            
            // Initialize the session table.  Linked list of free references.
            pMaster->wFirstFree = 0;
            pMaster->wMaxAlloc = 0;
            for (i = 0; i < MAX_SESSION_COUNT; i++) {
                pMaster->rgSessionRef[i] = MAKE_MASTER_FREEREF(0, i + 1);
            }

            // CPU perf counter info is unused until the first call to
            // CePerfControlCPU
            UpdateMasterCPUCounters(pMaster, CEPERF_CPU_DISABLE);
            
            // Data lists are empty
            pMaster->list.offsetDurBLHead = 0;
            pMaster->list.offsetDurDLHead = 0;
            
            // Initialize the page table.  The first page (the master page) is
            // allocated, and the rest are free
            memset(pMaster->rgdwPageMask, 0, PAGE_MASK_LEN*sizeof(DWORD));
            pMaster->rgdwPageMask[0] = MASTER_PAGEBIT(0);

            DEBUGMSG(1, (TEXT("CePerf is now live!\r\n")));
        }
    
        
        // SUCCESS: Initialize globals
        g_hMasterMap = hMap;
        g_pMaster = pMaster;
        g_hMasterMutex = hMutex;

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in OpenMasterMap!\r\n")));
        hResult = CEPERF_HR_EXCEPTION;
    }

exit:
    if (hMutex != NULL) {
        DEBUGMSG(ZONE_LOCK, (TEXT("-CePerf Lock\r\n")));
        ReleaseMutex(hMutex);
    }
    
    if (hResult != ERROR_SUCCESS) {
        if (pMaster != NULL) {
            UnmapViewOfFile(pMaster);
        }
        if (hMap != NULL) {
            CloseHandle(hMap);
        }
        if (hMutex != NULL) {
            CloseHandle(hMutex);
        }
    }
    
    DEBUGMSG(ZONE_MEMORY, (TEXT("-OpenMasterMap\r\n")));
    
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Must own master mutex to call CloseMasterMap; the mutex will be released and
// closed.
static VOID CloseMasterMap()
{
    HANDLE hTemp;

    DEBUGMSG(ZONE_MEMORY, (TEXT("+CloseMasterMap\r\n")));
    
#ifdef DEBUG
    // RAM will be freed when the last handle is closed, but even still it's
    // worth checking that the RAM was properly freed already
    if (g_pMaster
        && (g_pMaster->wMaxAlloc == 0)                      // Only possible open session is #0
        && (MASTER_ISFREE(g_pMaster->rgSessionRef[0]))) {   // Session #0 not open
        DWORD dwMaskNum;
        for (dwMaskNum = 0; dwMaskNum < PAGE_MASK_LEN; dwMaskNum++) {
            // All pages should be marked as free, except the very first one
            if (dwMaskNum != 0) {
                DEBUGCHK(g_pMaster->rgdwPageMask[dwMaskNum] == 0);
            } else {
                DEBUGCHK(g_pMaster->rgdwPageMask[dwMaskNum] == 1);
            }
        }
    }
#endif // DEBUG

    DEBUGMSG(1, (TEXT("CePerf is now shut down for this process.\r\n")));

    if (g_pMaster) {
        UnmapViewOfFile(g_pMaster);
        g_pMaster = NULL;
    }
    if (g_hMasterMap) {
        CloseHandle(g_hMasterMap);
        g_hMasterMap = NULL;
    }
    
    if (g_hMasterMutex) {
        hTemp = g_hMasterMutex;
        g_hMasterMutex = NULL;
        DEBUGMSG(ZONE_LOCK, (TEXT("-CePerf Lock (delete)\r\n")));
        ReleaseMutex(hTemp);
        CloseHandle(hTemp);
    }

    DEBUGMSG(ZONE_MEMORY, (TEXT("-CloseMasterMap\r\n")));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// BUGBUG:  NOT TRULY SAFE!  AcquireLoggerLockControl needs to block until all
// loggers have released the lock.  To accomplish that, AcquireLoggerLock needs
// to add the thread handle to a list and ReleaseLoggerLock needs to remove it.

#define LOGGERLOCK_CONTROLMASK          ((DWORD)0x80000000)

BOOL AcquireLoggerLock(LoggerLock* pLock)
{
    DWORD dwOldLock, dwNewLock;
    
    dwOldLock = pLock->dwLock;
    while (!(dwOldLock & LOGGERLOCK_CONTROLMASK)) {
        dwNewLock = InterlockedCompareExchange((volatile long*) &pLock->dwLock, dwOldLock+1, dwOldLock);
        if (dwNewLock == dwOldLock) {
            DEBUGMSG(ZONE_LOCK && ZONE_VERBOSE,
                     (TEXT("+Logger Lock 0x%08x lock=0x%08x hThread=0x%08x\r\n"),
                      pLock, pLock->dwLock, CePerf_GetCurrentThreadId()));
            return TRUE;
        }
        dwOldLock = dwNewLock;
    }
    return FALSE;
}

VOID ReleaseLoggerLock(LoggerLock* pLock)
{
    DEBUGMSG(ZONE_LOCK && ZONE_VERBOSE,
             (TEXT("-Logger Lock 0x%08x lock=0x%08x hThread=0x%08x\r\n"),
              pLock, pLock->dwLock, CePerf_GetCurrentThreadId()));
    DEBUGCHK(pLock->dwLock & ~LOGGERLOCK_CONTROLMASK);  // Lock count should be nonzero

    InterlockedDecrement((volatile long*) &pLock->dwLock);
}

// Controllers call AcquireControllerLock before performing any operations.
// The global controller lock blocks more than one controller from taking the
// session logger lock, so these functions don't have to check for that.  The
// purpose of "logger lock control" is to stop loggers from taking the
// session logger lock.

VOID AcquireLoggerLockControl(LoggerLock* pLock)
{
    DEBUGCHK(!(pLock->dwLock & LOGGERLOCK_CONTROLMASK));  // Nobody should currently control the lock

    InterlockedExchangeAdd((volatile long*) &pLock->dwLock, LOGGERLOCK_CONTROLMASK);

    DEBUGMSG(ZONE_LOCK,
             (TEXT("+Logger Lock 0x%08x lock=0x%08x hThread=0x%08x CONTROL\r\n"),
              pLock, pLock->dwLock, CePerf_GetCurrentThreadId()));
}

VOID ReleaseLoggerLockControl(LoggerLock* pLock)
{
    DEBUGCHK(pLock->dwLock & LOGGERLOCK_CONTROLMASK);  // We should currently control the lock
    DEBUGMSG(ZONE_LOCK,
             (TEXT("-Logger Lock 0x%08x lock=0x%08x hThread=0x%08x CONTROL\r\n"),
              pLock, pLock->dwLock, CePerf_GetCurrentThreadId()));

    InterlockedExchangeAdd((volatile long*) &pLock->dwLock, 0-LOGGERLOCK_CONTROLMASK);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
SessionHeader* LookupSessionHandle(
    HANDLE hSession
    )
{
    WORD  wIndex;
    DWORD dwSessionRef;

    // Validate the handle as much as possible
    wIndex = SESSION_INDEX(hSession);
    if (wIndex < MAX_SESSION_COUNT) {
        dwSessionRef = g_pMaster->rgSessionRef[wIndex];
        if (!MASTER_ISFREE(dwSessionRef)
            && (MASTER_VERSION(dwSessionRef) == SESSION_VERSION(hSession))) {
            return MASTER_SESSIONPTR(dwSessionRef);
        }
    }
    
    return NULL;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Not all calls to this function own the master mutex, so changes must be
// atomic.
LPVOID AllocPage()
{
    LPVOID lpPage = NULL;
    DWORD  dwMaskNum, dwBitNum;
    DWORD  dwOldMask, dwNewMask;

    for (dwMaskNum = 0; dwMaskNum < PAGE_MASK_LEN; dwMaskNum++) {
        dwOldMask = g_pMaster->rgdwPageMask[dwMaskNum];  // Get local copy
        while (dwOldMask != ((DWORD)-1)) {
            // At least one page free within the mask
            DWORD dwTemp = ~(dwOldMask);  // invert for alg
            
            // Fast algorithm for finding the least significant bit that is 1
            dwTemp &= (0 - dwTemp);
            dwBitNum =   (((dwTemp & 0xFFFF0000)!=0) << 4)
                       | (((dwTemp & 0xFF00FF00)!=0) << 3)
                       | (((dwTemp & 0xF0F0F0F0)!=0) << 2)
                       | (((dwTemp & 0xCCCCCCCC)!=0) << 1)
                       |  ((dwTemp & 0xAAAAAAAA)!=0);

            // Mark the page as being allocated.  Do it atomically to avoid
            // collisions.
            dwNewMask = dwOldMask | MASTER_PAGEBIT(dwBitNum);
            dwTemp = InterlockedCompareExchange((volatile long*) &(g_pMaster->rgdwPageMask[dwMaskNum]),
                                                dwNewMask, dwOldMask);
            if (dwTemp == dwOldMask) {
                // Successful assignment.  Now we own the page and can modify it.

                lpPage = ((LPBYTE)g_pMaster)         // base address of VM allocation
                         + (dwMaskNum * PGSIZE * 32) // plus offset of section (of 32 pages)
                         + (dwBitNum * PGSIZE);      // plus offset of page within section
                
                // The page will be committed when it is touched.  If there isn't
                // enough RAM to commit the page, touching it will cause an 
                // exception.
                __try {
                    *((LPDWORD)lpPage) = 0;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(ZONE_MEMORY, (TEXT("CePerf: Commit page 0x%08x failed\r\n"), lpPage));
                    
                    // Mark the page as being free again.  Do it atomically to 
                    // avoid collisions.
                    do {
                        dwOldMask = dwNewMask;                  // Current value
                        dwNewMask &= ~MASTER_PAGEBIT(dwBitNum); // Desired value
                        dwNewMask = InterlockedCompareExchange((volatile long*) &(g_pMaster->rgdwPageMask[dwMaskNum]),
                                                               dwNewMask, dwOldMask);
                        // Will only continue looping if the value changes under us
                    } while (dwNewMask != dwOldMask);

                    return NULL;
                }
            
                DEBUGMSG(ZONE_MEMORY, (TEXT("CePerf: Commit page 0x%08x\r\n"), lpPage));

#ifdef DEBUG
                memset(lpPage, 0xCC, PGSIZE);
#endif // DEBUG
                
                return lpPage;
            
            } else {
                // The mask changed out from under us; start again with the new value
                dwOldMask = dwTemp;
            }
        }
    }

    DEBUGMSG(ZONE_MEMORY, (TEXT("CePerf: Page alloc failed\r\n")));
    return NULL;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Not all calls to this function own the master mutex, so changes must be
// atomic.
VOID FreePage(LPVOID lpPage)
{
    DWORD dwMaskNum;
    DWORD dwPageNum, dwPageBit;
    DWORD dwOldMask, dwNewMask;
    
    // Sanity check on VM range
    if ((lpPage > (LPVOID)g_pMaster)
        && ((LPVOID)(((LPBYTE)g_pMaster) + CEPERF_MAP_SIZE) > lpPage)) {

        dwPageNum = ((DWORD)lpPage - (DWORD)g_pMaster) / PGSIZE;  // # pages from start
        dwPageBit = MASTER_PAGEBIT(dwPageNum);
        dwMaskNum = MASTER_PAGEINDEX(dwPageNum);
        
        dwNewMask = g_pMaster->rgdwPageMask[dwMaskNum];  // Get local copy
        DEBUGCHK(dwNewMask & dwPageBit);  // Should be marked as allocated
        if (dwNewMask & dwPageBit) {
            DEBUGMSG(ZONE_MEMORY, (TEXT("CePerf: Decommit page 0x%08x\r\n"), lpPage));

#ifdef DEBUG
            memset(lpPage, 0xDD, PGSIZE);
#endif // DEBUG

            // There's no way to decommit the physical page.
            // Mark the page as free.  Do it atomically to avoid collisions.
            do {
                dwOldMask = dwNewMask;                  // Current value
                dwNewMask &= ~dwPageBit;                // Desired value
                dwNewMask = InterlockedCompareExchange((volatile long*) &(g_pMaster->rgdwPageMask[dwMaskNum]),
                                                       dwNewMask, dwOldMask);
                // Will only continue looping if the value changes under us
            } while (dwNewMask != dwOldMask);
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL AddToDLLSessionTable(
    HANDLE hSession
    )
{
    DLLSessionTable* pTable;
    WORD i;

    DEBUGCHK(hSession != INVALID_HANDLE_VALUE);
    
    // BUGBUG slow algorithm but assuming each process actually has a low 
    // number of open sessions, it's fine.
    for (pTable = g_pDLLSessions; pTable; pTable = pTable->pNext) {
        for (i = 0; i < TABLE_COUNT; i++) {
            if (pTable->rghSession[i] == INVALID_HANDLE_VALUE) {
                // The table is kept packed so the new session is not present
                pTable->rghSession[i] = hSession;
                pTable->rgdwRefCount[i] = 1;
                return TRUE;
            } else if (pTable->rghSession[i] == hSession) {
                // Already present in the table; increment the refcount
                if (pTable->rgdwRefCount[i] < (DWORD)-1) {  // rollover safety
                    pTable->rgdwRefCount[i]++;
                }
                return TRUE;
            }
            
            // Session index should be unique
            DEBUGCHK(SESSION_INDEX(pTable->rghSession[i]) != SESSION_INDEX(hSession));
        }
    }

    // Else need to allocate a new table
    pTable = LocalAlloc(LMEM_FIXED, sizeof(DLLSessionTable));
    if (!pTable) {
        return FALSE;
    }

    pTable->rghSession[0] = hSession;
    pTable->rgdwRefCount[0] = 1;

    memset(&pTable->rgdwRefCount[1], 0, (TABLE_COUNT-1) * sizeof(DWORD));
    memset(&pTable->rghSession[1], 0xFF, (TABLE_COUNT-1) * sizeof(HANDLE));
    pTable->pNext = g_pDLLSessions;
    g_pDLLSessions = pTable;

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL RemoveFromDLLSessionTable(
    HANDLE hSession
    )
{
    DLLSessionTable* pTable;
    DLLSessionTable* pPrevTable;
    WORD i;

    DEBUGCHK(hSession != INVALID_HANDLE_VALUE);
    
    // BUGBUG slow algorithm but assuming each process actually has a low 
    // number of open sessions, it's fine.
    for (pPrevTable = NULL, pTable = g_pDLLSessions;
         pTable;
         pPrevTable = pTable, pTable =  pTable->pNext) {

        for (i = 0; i < TABLE_COUNT; i++) {
            if (pTable->rghSession[i] == INVALID_HANDLE_VALUE) {
                // The table is kept packed so the session is not present
                if(NULL != pTable->pNext){  // if there is another table, the handle
                    break;                // could exist there. break into outer loop.
                }
                else{
                    DEBUGCHK(0);
                    return FALSE;
                }
            } else if (pTable->rghSession[i] == hSession) {
                // Found it in the table; decrement the refcount
                pTable->rgdwRefCount[i]--;
                if (pTable->rgdwRefCount[i] == 0) {
                    // Last open handle; pack the array
                    DLLSessionTable* pLastTable;
                    WORD iLast;

                    for (pLastTable = pTable;
                         pLastTable->pNext;
                         pPrevTable = pLastTable, pLastTable = pLastTable->pNext)
                        ;
                    // now pPrevTable points to the next-to-last table
                    for (iLast = TABLE_COUNT;
                         (iLast > 0) && (pLastTable->rghSession[iLast-1] == INVALID_HANDLE_VALUE);
                         iLast--)
                        ;
                    DEBUGCHK(iLast > 0);  // Not supposed to have an empty table on end
                    if (iLast > 0) {
                        // Found the last occupied entry; swap it into the
                        // slot being vacated
                        pTable->rghSession[i] = pLastTable->rghSession[iLast-1];
                        pTable->rgdwRefCount[i] = pLastTable->rgdwRefCount[iLast-1];
                        pLastTable->rghSession[iLast-1] = INVALID_HANDLE_VALUE;
                        pLastTable->rgdwRefCount[iLast-1] = 0;

                        // Free the last table if it's empty
                        if (iLast == 1) {
                            if (pPrevTable) {
                                pPrevTable->pNext = NULL;
                            } else {
                                g_pDLLSessions = NULL;
                            }
                            LocalFree(pLastTable);
                        }
                    }
                }
                
                return TRUE;
            }
            
            // Session index should be unique
            DEBUGCHK(SESSION_INDEX(pTable->rghSession[i]) != SESSION_INDEX(hSession));
        }
    }

    // Got through the whole table, the session is not present
    DEBUGCHK(0);
    return FALSE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Clean up any leaked sessions on process exit
static BOOL CleanupDLLSessionTable()
{
    DLLSessionTable* pTable;
    DLLSessionTable* pTempTable;
    SessionHeader*   pSession;
    WORD i;

    DEBUGCHK((g_hMasterMutex && g_pMaster)
             || ((g_pDLLSessions == NULL) && (g_dwDLLSessionCount == 0)));

    if (g_hMasterMutex && g_pMaster) {
        AcquireControllerLock();
        __try {
            pTable = g_pDLLSessions;
            while (pTable) {
                for (i = 0; i < TABLE_COUNT; i++) {
                    if (pTable->rghSession[i] != INVALID_HANDLE_VALUE) {
                        // One or more leaked handles to this session
                        pSession = LookupSessionHandle(pTable->rghSession[i]);
                        DEBUGCHK(pSession);
                        if (pSession) {
                            DEBUGMSG(pTable->rgdwRefCount[i],
                                     (TEXT("CePerf: Cleaning up %u leaked handles to hSession=0x%08x %s\r\n"),
                                      pTable->rgdwRefCount[i], pTable->rghSession[i],
                                      pSession->szSessionName));
                            
                            while (pTable->rgdwRefCount[i]--) {
                                DEBUGCHK(pSession->dwRefCount > 0);
                                if (pSession->dwRefCount > 0) {
                                    pSession->dwRefCount--;
                                }
                                
                                DEBUGCHK(g_dwDLLSessionCount > 0);
                                if (g_dwDLLSessionCount > 0) {
                                    g_dwDLLSessionCount--;
                                }
                            }
                            
                            if (pSession->dwRefCount == 0) {
                                CleanupSession(pTable->rghSession[i], pSession);
                            }
                        } else {
                            DEBUGMSG(pTable->rgdwRefCount[i],
                                     (TEXT("CePerf: Cleaning up %u leaked handles to hSession=0x%08x (invalid)\r\n"),
                                      pTable->rgdwRefCount[i], pTable->rghSession[i]));
                        }
                    }
                }
        
                // Now remove this table
                pTempTable = pTable;
                pTable =pTable->pNext;
                LocalFree(pTempTable);
            }
        
            g_pDLLSessions = NULL;
        
            // Close the master map if this was the last session for this DLL instance
            DEBUGCHK(g_dwDLLSessionCount == 0);
            if (g_dwDLLSessionCount == 0) {
                CloseMasterMap();  // Releases and deletes mutex!
                DEBUGCHK(*((volatile HANDLE*)&g_hMasterMutex) == NULL);
            }
        
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Safety catch so that we release the lock no matter what
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CleanupDLLSessionTable!\r\n")));
        }

        // If the mutex still exists then we still own it
        if (*((volatile HANDLE*)&g_hMasterMutex)) {  // Force re-read of global to avoid local copy
            ReleaseControllerLock();
        }
    }
    
    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Clean up on process exit so as not to allow any leaks
VOID CleanupProcess()
{
    if (ZONE_VERBOSE) {
        DEBUGMSG(ZONE_VERBOSE, (TEXT("CEPERF GLOBALS:  (process cleanup)\r\n")));
        DumpMasterInfo();
        DumpDLLSessionTable();
    }

    CleanupDLLSessionTable();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Returns TRUE if exactly one bit is set in the value
_inline static BOOL OneBitSet(
    DWORD dwValue
    )
{
    return (dwValue                          // At least one bit turned on
            && !(dwValue & (dwValue - 1)));  // No more than one bit turned on
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Returns TRUE if the flags are valid for this type, otherwise returns FALSE.
BOOL ValidateRecordingFlags(
    DWORD dwRecordingFlags,
    CEPERF_ITEM_TYPE type
    )
{
    DWORD dwModeFlags, dwSemanticFlags;

    if ((dwRecordingFlags == 0) || (dwRecordingFlags == CEPERF_DEFAULT_FLAGS)) {
        return TRUE;
    }

    switch (type) {
    case CEPERF_TYPE_DURATION:
        dwModeFlags = VALID_DURATION_RECORDING_FLAGS;
        dwSemanticFlags = VALID_DURATION_RECORDING_SEMANTIC_FLAGS;
        // Unique and unlimited are mutually exclusive
        if ((dwRecordingFlags & CEPERF_DURATION_RECORD_UNIQUE)
            && (dwRecordingFlags & CEPERF_DURATION_RECORD_UNLIMITED)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Invalid duration flags, \"unique\" and \"unlimited\" are mutually exclusive.\r\n")));
            return FALSE;
        }
        // Replacement flags require unique flag
        if ((dwRecordingFlags & VALID_DURATION_UNIQUE_REPLACEMENT_FLAGS)
            && !(dwRecordingFlags & CEPERF_DURATION_RECORD_UNIQUE)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Invalid duration flags, \"replace begin\" requires \"unique.\"\r\n")));
            return FALSE;
        }
        // Shared and per-thread are mutually exclusive.  But we add the
        // per-thread flag to many objects globally from mdpgperf.h.  So
        // don't fail registration with that combination.  Just warn the
        // user that per-thread recording won't work on shared objects.
        if ((dwRecordingFlags & CEPERF_DURATION_RECORD_SHARED)
            && (dwRecordingFlags & CEPERF_RECORD_THREAD_TICKCOUNT)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Invalid duration flags, \"shared\" and per-thread tick count are\r\n")));
            DEBUGMSG(ZONE_ERROR, (TEXT("mutually exclusive.  Recording per-thread time is not possible on objects\r\n")));
            DEBUGMSG(ZONE_ERROR, (TEXT("that are shared between threads.  Skipping per-thread recording.\r\n")));
            // But allow the object to be created with these flags...
        }
        break;
    case CEPERF_TYPE_STATISTIC:
        dwModeFlags = VALID_STATISTIC_RECORDING_FLAGS;
        dwSemanticFlags = 0;
        break;
    case CEPERF_TYPE_LOCALSTATISTIC:
        dwModeFlags = VALID_LOCALSTATISTIC_RECORDING_FLAGS;
        dwSemanticFlags = 0;
        break;
    default:
        return FALSE;
    }

    if (dwRecordingFlags & ~(dwModeFlags | dwSemanticFlags | VALID_COMMON_RECORDING_FLAGS)) {
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Failing registration of item with invalid flags 0x%08X.\r\n"),
                 dwRecordingFlags & ~(dwModeFlags | dwSemanticFlags | VALID_COMMON_RECORDING_FLAGS)));
        return FALSE;
    }
    // Exactly one mode flag must be set
    if (!OneBitSet(dwRecordingFlags & dwModeFlags)) {
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Failing registration of item with multiple recording mode flags 0x%08X.\r\n"),
                 dwRecordingFlags & dwModeFlags));
        return FALSE;
    }

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Called by CePerfOpenSession and CePerfControlSession to determine which 
// session settings to use.  lpSettings must be initialized to existing session
// settings.  (If the session is not already open then lpSettings should be all
// 0 to indicate that nothing is yet set.)
HRESULT ValidateSessionSettings(
    LPCWSTR lpszSessionPath,            // IN: Name of session being opened
    const CEPERF_SESSION_INFO* lpInfo,  // IN: Session info from CePerfOpenSession/CePerfControlSession
    DWORD dwStatusFlags,                // IN: Status flags from CePerfOpenSession/CePerfControlSession
    SessionSettings* lpSettings         // IN: Settings of current session
                                        // OUT: New session settings to use
    )
{
    WORD i;

    // STATUS FLAGS

    if (dwStatusFlags == CEPERF_DEFAULT_FLAGS) {
        lpSettings->dwStatusFlags = dwStatusFlags;
    } else if (dwStatusFlags != 0) {
        if (dwStatusFlags & ~VALID_STATUS_FLAGS) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with invalid status flags 0x%08X.\r\n"),
                     dwStatusFlags & ~VALID_STATUS_FLAGS));
            return CEPERF_HR_INVALID_PARAMETER;
        }
        // Exactly one recording flag must be set
        if (!OneBitSet(dwStatusFlags & VALID_RECORDING_STATUS_FLAGS)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with multiple recording status flags 0x%08X.\r\n"),
                     dwStatusFlags & VALID_RECORDING_STATUS_FLAGS));
            return CEPERF_HR_INVALID_PARAMETER;
        }
        // Exactly one storage flag must be set
        if (!OneBitSet(dwStatusFlags & VALID_STORAGE_STATUS_FLAGS)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with multiple storage status flags 0x%08X.\r\n"),
                     dwStatusFlags & VALID_STORAGE_STATUS_FLAGS));
            return CEPERF_HR_INVALID_PARAMETER;
        }
        lpSettings->dwStatusFlags = dwStatusFlags;
    }

    
    if (lpInfo) {
        // Validate struct version
        if (lpInfo->wVersion != 1) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Invalid wVersion value in CEPERF_SESSION_INFO.\r\n")));
            return CEPERF_HR_INVALID_PARAMETER;
        }
            
        // STORAGE FLAGS

        if (lpInfo->dwStorageFlags == CEPERF_DEFAULT_FLAGS) {
            lpSettings->dwStorageFlags = lpInfo->dwStorageFlags;
        } else if (lpInfo->dwStorageFlags != 0) {

            if (lpInfo->dwStorageFlags & ~VALID_STORAGE_FLAGS) {
                DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with invalid storage flags 0x%08X.\r\n"),
                         lpInfo->dwStorageFlags & ~VALID_STORAGE_FLAGS));
                return CEPERF_HR_INVALID_PARAMETER;
            }
            // Exactly one storage location flag must be set
            if (!OneBitSet(lpInfo->dwStorageFlags & VALID_STORAGE_LOCATION_FLAGS)) {
                DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with multiple storage location flags 0x%08X.\r\n"),
                         lpInfo->dwStorageFlags & VALID_STORAGE_LOCATION_FLAGS));
                return CEPERF_HR_INVALID_PARAMETER;
            }

            // Storage type and overflow flags are validated based on location flag
            switch (lpInfo->dwStorageFlags & VALID_STORAGE_LOCATION_FLAGS) {
            case CEPERF_STORE_FILE:
                // FILE ==> type != binary, overflow != newest, filename required
                
                // Exactly one storage type flag must be set
                if (!OneBitSet(lpInfo->dwStorageFlags & VALID_STORAGE_TYPE_FLAGS)) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with multiple storage type flags 0x%08X.\r\n"),
                             lpInfo->dwStorageFlags & VALID_STORAGE_TYPE_FLAGS));
                    return CEPERF_HR_INVALID_PARAMETER;

                // Filename required
                } else if (!lpInfo->lpszStoragePath || !lpInfo->lpszStoragePath[0]) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with CEPERF_STORE_FILE but no filename.\r\n")));
                    return CEPERF_HR_INVALID_PARAMETER;
                
                // Writing to a binary file is not supported -- use CeLog instead
                } else if (lpInfo->dwStorageFlags & CEPERF_STORE_BINARY) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with binary file output - try CEPERF_STORE_CELOG instead.\r\n")));
                    return CEPERF_HR_NOT_SUPPORTED;
                }
                lpSettings->dwStorageFlags = lpInfo->dwStorageFlags | CEPERF_STORE_OLDEST;

                break;
            
            case CEPERF_STORE_CELOG:
                // CELOG ==> type = binary, overflow=0, no filename
                if ((lpInfo->dwStorageFlags & VALID_STORAGE_TYPE_FLAGS) != CEPERF_STORE_BINARY) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with CEPERF_STORE_CELOG but not CEPERF_STORE_BINARY.\r\n")));
                    return CEPERF_HR_INVALID_PARAMETER;
                } else if ((lpInfo->dwStorageFlags & VALID_STORAGE_OVERFLOW_FLAGS) != 0) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with CEPERF_STORE_CELOG and nonzero overflow flags.\r\n")));
                    return CEPERF_HR_INVALID_PARAMETER;
                } else if (lpInfo->lpszStoragePath && lpInfo->lpszStoragePath[0]) {
                    // CePerf can't honor the file name -- you have to start celogflush.exe
                    // or some other CeLog client
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with CEPERF_STORE_CELOG plus a filename - CePerf cannot\r\n")));
                    DEBUGMSG(ZONE_ERROR, (TEXT("control the CeLog output file.  Use CeLogFlush.exe or other CeLog client to manage the output file.\r\n")));
                    return CEPERF_HR_NOT_SUPPORTED;
                }
                lpSettings->dwStorageFlags = lpInfo->dwStorageFlags;
                break;
                    
            case CEPERF_STORE_DEBUGOUT:
                // DEBUGOUT ==> type != binary, overflow=0
                lpSettings->dwStorageFlags = 0;
                
                // Default to text if not set (CSV is also allowed)
                if ((lpInfo->dwStorageFlags & VALID_STORAGE_TYPE_FLAGS) == 0) {
                    lpSettings->dwStorageFlags |= CEPERF_STORE_TEXT;
                
                } else if (!OneBitSet(lpInfo->dwStorageFlags & VALID_STORAGE_TYPE_FLAGS)) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with multiple storage type flags 0x%08X.\r\n"),
                             lpInfo->dwStorageFlags & VALID_STORAGE_TYPE_FLAGS));
                    return CEPERF_HR_INVALID_PARAMETER;
                } else if (lpInfo->dwStorageFlags & CEPERF_STORE_BINARY) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with CEPERF_STORE_DEBUGOUT and CEPERF_STORE_BINARY.\r\n")));
                    return CEPERF_HR_INVALID_PARAMETER;
                } else if (lpInfo->dwStorageFlags & VALID_STORAGE_OVERFLOW_FLAGS) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with CEPERF_STORE_DEBUGOUT and nonzero overflow flags.\r\n")));
                    return CEPERF_HR_INVALID_PARAMETER;
                }
                lpSettings->dwStorageFlags |= lpInfo->dwStorageFlags;
                break;
            
            case CEPERF_STORE_REGISTRY:
                // REGISTRY ==> type != binary, type != CSV, overflow=0
                if (lpInfo->dwStorageFlags & (CEPERF_STORE_BINARY | CEPERF_STORE_CSV)) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with CEPERF_STORE_REGISTRY and binary or CSV flag set.\r\n")));
                    return CEPERF_HR_INVALID_PARAMETER;
                } else if (lpInfo->dwStorageFlags & VALID_STORAGE_OVERFLOW_FLAGS) {
                    DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with CEPERF_STORE_REGISTRY and nonzero overflow flags.\r\n")));
                    return CEPERF_HR_INVALID_PARAMETER;
                }
                lpSettings->dwStorageFlags = (lpInfo->dwStorageFlags & ~CEPERF_STORE_TEXT);
                break;
            
            default:
                // Should not be possible to get here
                DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Unexpected storage location flags.\r\n")));
                DEBUGCHK(0);
                return CEPERF_HR_INVALID_PARAMETER;
            }
        }
        
        
        // STORAGE PATH

        if (lpInfo->lpszStoragePath) {
            wcsncpy(lpSettings->szStoragePath, lpInfo->lpszStoragePath, MAX_PATH);
            lpSettings->szStoragePath[MAX_PATH-1] = 0;
        }
        
        
        // PER-TYPE RECORDING FLAGS

        if (!ValidateRecordingFlags(lpInfo->rgdwRecordingFlags[CEPERF_TYPE_DURATION],
                                    CEPERF_TYPE_DURATION)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with invalid duration recording flags.\r\n")));
            return CEPERF_HR_INVALID_PARAMETER;
        } else if (!ValidateRecordingFlags(lpInfo->rgdwRecordingFlags[CEPERF_TYPE_STATISTIC],
                                           CEPERF_TYPE_STATISTIC)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with invalid statistic recording flags.\r\n")));
            return CEPERF_HR_INVALID_PARAMETER;
        } else if (!ValidateRecordingFlags(lpInfo->rgdwRecordingFlags[CEPERF_TYPE_LOCALSTATISTIC],
                                           CEPERF_TYPE_LOCALSTATISTIC)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerfOpenSession: Failing creation of session with invalid local-statistic recording flags.\r\n")));
            return CEPERF_HR_INVALID_PARAMETER;
        }
        for (i = 0; i < CEPERF_NUMBER_OF_TYPES; i++) {
            if (lpInfo->rgdwRecordingFlags[i] != 0) {
                lpSettings->rgdwRecordingFlags[i] = lpInfo->rgdwRecordingFlags[i];
            }
        }
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Called by CePerfOpenSession to determine which session settings to use
// when creating a new session.
HRESULT GetSessionSettings(
    LPCWSTR lpszSessionPath,            // IN: Name of session being opened
    const CEPERF_SESSION_INFO* lpInfo,  // IN: Session info from CePerfOpenSession
    DWORD dwStatusFlags,                // IN: Status flags from CePerfOpenSession
    SessionSettings* lpSettings         // OUT: New session settings to use
    )
{
    HRESULT hResult;

    // The algorithm that will be used to determine the settings for a session
    // (dwStatusFlags and lpInfo) is:
    // 1. On CePerfOpenSession, if another handle to the session is already 
    //    currently open, the current settings will remain the same, even if 
    //    dwStatusFlags or lpInfo contain different values.
    //    NOTE: That case is handled outside this function because
    //          GetSessionSettings will not be called.
    // 2. Otherwise, if dwStatusFlags or lpInfo is specified (non-zero), those
    //    settings will be used.
    // 3. (NOTE!  Considering not supporting this step for now!)  Otherwise, if
    //    the session settings are provided in the registry, those settings
    //    will be used.
    // 4. Otherwise, examine the parent and on up the hierarchy to the root, 
    //    applying #1 and then #3 from the first session found.  For example if
    //    the parent session is currently open, its settings will be used.  If 
    //    not, but the parent session has settings in the registry, those 
    //    settings will be used.  If not, we examine the parent's parent and up
    //    until we reach the root.
    // 5. If root settings are not in the registry, the session settings will 
    //    default so that recording and storage are disabled.
    // Once a session is open, its settings can only be changed by a call to 
    // CePerfControlSession.

    // Validate all of the parameters that were provided
    hResult = ValidateSessionSettings(lpszSessionPath, lpInfo, dwStatusFlags,
                                      lpSettings);
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // For the status and storage flags, 0 means the same as 
    // CEPERF_DEFAULT_FLAGS.  So just use one value to reduce the number of 
    // cases to check for.
    // For recording flags, 0 means not to override the tracked item settings.
    if (lpSettings->dwStatusFlags == 0)
        lpSettings->dwStatusFlags = CEPERF_DEFAULT_FLAGS;
    if (lpSettings->dwStorageFlags == 0)
        lpSettings->dwStorageFlags = CEPERF_DEFAULT_FLAGS;
    
    // Look up parent flags for any that require it
    if ((lpSettings->dwStatusFlags == CEPERF_DEFAULT_FLAGS)
        || (lpSettings->dwStorageFlags == CEPERF_DEFAULT_FLAGS)
        || (lpSettings->rgdwRecordingFlags[CEPERF_TYPE_DURATION] == CEPERF_DEFAULT_FLAGS)
        || (lpSettings->rgdwRecordingFlags[CEPERF_TYPE_STATISTIC] == CEPERF_DEFAULT_FLAGS)
        || (lpSettings->rgdwRecordingFlags[CEPERF_TYPE_LOCALSTATISTIC] == CEPERF_DEFAULT_FLAGS)) {
        // BUGBUG NYI
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: default session settings NYI\r\n")));
        DEBUGCHK(0);
        return CEPERF_HR_NOT_SUPPORTED;
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static HRESULT CreateNewSession(
    HANDLE* lphSession,
    LPCWSTR lpszSessionPath,
    DWORD   dwStatusFlags,
    const CEPERF_SESSION_INFO* lpInfo
    )
{
    HRESULT hResult;
    SessionHeader* pSession = NULL;
    WORD    wIndex, wVersion;
    DWORD   dwPageIndex;
    SessionSettings settings;  // Temp copy on stack until the memory is allocated
    TrackedItem* pItem;

    // First determine which settings to use
    memset(&settings, 0, sizeof(SessionSettings));
    hResult = GetSessionSettings(lpszSessionPath, lpInfo, dwStatusFlags, &settings);
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }
    
    hResult = CEPERF_HR_NOT_ENOUGH_MEMORY;

    // Alloc memory for the session header
    pSession = AllocPage();
    if (pSession) {
        // Alloc a session handle
        if (g_pMaster->wFirstFree < MAX_SESSION_COUNT) {
            wIndex = g_pMaster->wFirstFree;
            g_pMaster->wFirstFree = MASTER_NEXTFREE(g_pMaster->rgSessionRef[wIndex]);
            if (wIndex > g_pMaster->wMaxAlloc) {
                g_pMaster->wMaxAlloc = wIndex;
            }

            // Store the session pointer in the master session table
            wVersion = MASTER_VERSION(g_pMaster->rgSessionRef[wIndex]);
            if ((wIndex == 0) && (wVersion == 0)) {
                // Special case: don't allow the session at index=0 to have
                // version=0, because that would create hSession=0 which is
                // unsafe; apps may call it by mistake.  This also disallows
                // hTrackedItem=0.
                wVersion = 1;
            }
            g_pMaster->rgSessionRef[wIndex] = MAKE_MASTER_ALLOCREF(wVersion, pSession);

            // Get the handle
            *lphSession = MAKE_SESSION_HANDLE(wVersion, wIndex);

            DEBUGMSG(ZONE_SESSION || ZONE_MEMORY,
                     (TEXT("CePerf: New hSession=0x%08x pSession=0x%08x\r\n"),
                      *lphSession, pSession));

            hResult = ERROR_SUCCESS;

        } else {
            DEBUGMSG(ZONE_MEMORY, (TEXT("CePerf: Handle alloc failed!\r\n")));
            FreePage(pSession);
        }
    }

    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // Initialize the session header
    if (lpszSessionPath) {
        wcsncpy(pSession->szSessionName, lpszSessionPath, MAX_PATH);
        pSession->szSessionName[MAX_PATH-1] = 0;
    } else {
        memset(pSession->szSessionName, 0, MAX_PATH*sizeof(WCHAR));
    }
    pSession->dwRefCount = 1;
    memcpy(&pSession->settings, &settings, sizeof(SessionSettings));
    pSession->lock.dwLock = 0;
    pSession->dwNumObjects = 0;
    
    // Initialize the tracked items on the session header page
    pSession->items.offsetNextPage = 0;
    pSession->items.wFirstFree = 0;
    pSession->items.wStartIndex = 0;
    pItem = SESSION_PFIRSTITEM(pSession);
    for (dwPageIndex = 0; dwPageIndex < MAX_ITEMS_ON_FIRST_PAGE; dwPageIndex++) {
        TRACKEDITEM_MARKFREE(pItem, dwPageIndex+1);
        pItem++;
    }

    return ERROR_SUCCESS;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static HRESULT CleanupSession(
    HANDLE hSession,
    SessionHeader* pSession
    )
{
    WORD  wIndex, wVersion;
    
    // Not safety-checking the handle because it has already been used to look
    // up pSession
    
    DEBUGMSG(ZONE_SESSION || ZONE_MEMORY,
             (TEXT("CePerf: Free hSession=0x%08x pSession=0x%08x\r\n"),
              hSession, pSession));

    // BUGBUG clean up any recording & storage resources
    
    // Wait for all existing loggers to stop, and block further logging
    AcquireLoggerLockControl(&pSession->lock);  // Will not be released
    
    // Free the session page and its chain of item pages, including data for all
    // tracked items.
    DeleteAllItems(hSession, pSession);
    DEBUGCHK(pSession->items.offsetNextPage == 0);  // Should be cleaned up now
    FreePage(pSession);

    // Free the session handle
    wIndex = SESSION_INDEX(hSession);
    DEBUGCHK(wIndex < MAX_SESSION_COUNT);
    DEBUGCHK(!MASTER_ISFREE(g_pMaster->rgSessionRef[wIndex]));
    wVersion = SESSION_VERSION(hSession);
    DEBUGCHK(MASTER_VERSION(g_pMaster->rgSessionRef[wIndex]) == wVersion);
    g_pMaster->rgSessionRef[wIndex] = MAKE_MASTER_FREEREF(wVersion+1, g_pMaster->wFirstFree);
    g_pMaster->wFirstFree = wIndex;

    // Recalculate MaxAlloc index if necessary
    if (wIndex >= g_pMaster->wMaxAlloc) {
        for (wIndex = MAX_SESSION_COUNT - 1;
             (wIndex > 0) && MASTER_ISFREE(g_pMaster->rgSessionRef[wIndex]);
             wIndex--)
            ;
        g_pMaster->wMaxAlloc = wIndex;
        DEBUGCHK(g_pMaster->wMaxAlloc < MAX_SESSION_COUNT);
    }
    
    return ERROR_SUCCESS;
}


// Used to pass data to and from ControlSession during a session walk.
typedef struct {
    DWORD dwStatusFlags;
    const CEPERF_SESSION_INFO* lpInfo;
} ControlInfo;


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllSessions.  Change settings for a single
// session.
static HRESULT ControlSession(
    HANDLE         hSession,
    SessionHeader* pSession,
    LPVOID         lpWalkParam  // const ControlInfo* pControlInfo
    )
{
    const ControlInfo* pControlInfo = (const ControlInfo*) lpWalkParam;
    SessionSettings    settings;   // Temp copy while validating / changing
    DWORD              dwNewStatusFlags = pControlInfo->dwStatusFlags;  // Writeable copy
    HRESULT            hResult;

    DEBUGMSG(ZONE_SESSION,
             (TEXT("CePerf: Control pSession=0x%08x %s\r\n"),
              pSession, pSession->szSessionName));
    
    // Allow recording status = 0 to leave the recording status unchanged
    if ((dwNewStatusFlags & VALID_RECORDING_STATUS_FLAGS) == 0) {
        dwNewStatusFlags |= (pSession->settings.dwStatusFlags & VALID_RECORDING_STATUS_FLAGS);
    }
    // Allow storage status = 0 to leave the storage status unchanged
    if ((dwNewStatusFlags & VALID_STORAGE_STATUS_FLAGS) == 0) {
        dwNewStatusFlags |= (pSession->settings.dwStatusFlags & VALID_STORAGE_STATUS_FLAGS);
    }
    
    // Validate parameters and build temporary copy of new settings
    memcpy(&settings, &pSession->settings, sizeof(SessionSettings));
    hResult = ValidateSessionSettings(pSession->szSessionName, pControlInfo->lpInfo,
                                      dwNewStatusFlags, &settings);
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // Look up parent flags for any that require it
    if ((settings.dwStatusFlags == CEPERF_DEFAULT_FLAGS)
        || (settings.dwStorageFlags == CEPERF_DEFAULT_FLAGS)
        || (settings.rgdwRecordingFlags[CEPERF_TYPE_DURATION] == CEPERF_DEFAULT_FLAGS)
        || (settings.rgdwRecordingFlags[CEPERF_TYPE_STATISTIC] == CEPERF_DEFAULT_FLAGS)
        || (settings.rgdwRecordingFlags[CEPERF_TYPE_LOCALSTATISTIC] == CEPERF_DEFAULT_FLAGS)) {
        // BUGBUG NYI
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: default session settings NYI\r\n")));
        DEBUGCHK(0);
        return CEPERF_HR_NOT_SUPPORTED;
    }

    // Now prepare to use the new settings without actually modifying the
    // session yet, in case of failure
    // BUGBUG nothing to do until continuous recording is implemented
    

    //
    // Finally everything is ready, so make the switch
    //
    
    hResult = ERROR_SUCCESS;

    // Wait for all existing loggers to stop, and block further logging
    AcquireLoggerLockControl(&pSession->lock);
    
    memcpy(&pSession->settings, &settings, sizeof(SessionSettings));
    
    // If settings for any type of item change, data for all items must
    // be cleared.
    if ((settings.rgdwRecordingFlags[CEPERF_TYPE_DURATION] != 0)
        || (settings.rgdwRecordingFlags[CEPERF_TYPE_STATISTIC] != 0)
        || (settings.rgdwRecordingFlags[CEPERF_TYPE_LOCALSTATISTIC] != 0)) {
        hResult = WalkAllItems(hSession, pSession, Walk_ChangeTypeSettings,
                               (LPVOID)&settings.rgdwRecordingFlags, FALSE);
    }
    
    // Now allow logging again
    ReleaseLoggerLockControl(&pSession->lock);

    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Used specifically for writing discrete data during a flush, not writing
// continuous data.
HRESULT FlushBytes(
    FlushState* pFlush,
    LPBYTE lpBuffer,
    DWORD  dwBufSize
    )
{
    HRESULT hResult = ERROR_SUCCESS;
    DWORD   dwBytesWritten;

    switch (pFlush->pSession->settings.dwStorageFlags & VALID_STORAGE_LOCATION_FLAGS) {
    case CEPERF_STORE_FILE:
        if (!WriteFile(pFlush->hOutput, lpBuffer, dwBufSize, &dwBytesWritten, NULL)
            || (dwBytesWritten != dwBufSize)) {
            hResult = GetLastError();
            hResult = HRESULT_FROM_WIN32(hResult);
        }
        break;
    
    case CEPERF_STORE_CELOG:
        CeLogData(TRUE, CELID_CEPERF, lpBuffer, (WORD)dwBufSize, 0, CELZONE_ALWAYSON, 0, FALSE);
        break;

    case CEPERF_STORE_DEBUGOUT:
        NKDbgPrintfW((LPWSTR)lpBuffer);
        break;

    default:
        DEBUGCHK(0);
        break;
    }

    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Used specifically for writing discrete data during a flush, not writing
// continuous data.
HRESULT FlushChars(
    FlushState* pFlush,
    __format_string LPWSTR lpszFormat,
    ...
    )
{
    #define FLUSHCHARS_TEMPBUF_SIZE  (2*MAX_PATH)  // Arbitrary length, MAX_PATH is a little smaller than desired
    va_list arglist;
    WCHAR   szTemp[FLUSHCHARS_TEMPBUF_SIZE];
    size_t  cchLen;
    HRESULT hr;

    // Compose a single string using the input args
    va_start(arglist, lpszFormat);
    hr = StringCchVPrintfW(szTemp, FLUSHCHARS_TEMPBUF_SIZE, lpszFormat, arglist);
    if (SUCCEEDED(hr)) {
        hr = StringCchLengthW(szTemp, FLUSHCHARS_TEMPBUF_SIZE, &cchLen);
        if (SUCCEEDED(hr)) {
            return FlushBytes(pFlush, (LPBYTE)szTemp, cchLen*sizeof(WCHAR));
        }
    }

    return hr;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Used specifically for writing discrete data during a flush, not writing
// continuous data.
HRESULT FlushULargeCSV(
    FlushState* pFlush,
    const ULARGE_INTEGER* pulValue
    )
{
    HRESULT hResult;
    
    // BUGBUG working around some issues Excel seems to have with large numbers
    // (actually with more than some number of non-zero digits in a row, it
    // chops off the lower digits)

    // Leading comma separator
    hResult = FlushChars(pFlush, TEXT(","));
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    // Write formula if the total is larger than 32 bits
    if (pulValue->HighPart) {
        hResult = FlushChars(pFlush, TEXT("=(%u*(2^32))+"), pulValue->HighPart);
        if (hResult != ERROR_SUCCESS) {
            return hResult;
        }
    }
    
    return FlushChars(pFlush, TEXT("%u"), pulValue->LowPart);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Used specifically for writing discrete data during a flush, not writing
// continuous data.
HRESULT FlushDoubleCSV(
    FlushState* pFlush,
    double dValue
    )
{
    HRESULT hResult;
    
    // Leading comma separator
    hResult = FlushChars(pFlush, TEXT(","));
    if (hResult != ERROR_SUCCESS) {
        return hResult;
    }

    return FlushChars(pFlush, TEXT("%.1lf"), dValue);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HRESULT FlushSessionDescriptorBinary(
    FlushState* pFlush,
    HANDLE      hSession
    )
{
    // Convenient declaration of the data that will be flushed
#pragma pack(push,4)
    typedef struct {
        CEPERF_LOGID id;
        CEPERF_LOG_SESSION_DESCRIPTOR session;
        WCHAR szPaths[2*MAX_PATH];
    } Data;
#pragma pack(pop)

    DEBUGCHK(pFlush->pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG);
    if (pFlush->lpTempBuffer && (pFlush->cbTempBuffer >= sizeof(Data))) {
        Data*  pData = (Data*) pFlush->lpTempBuffer;
        DWORD  cbData;
        size_t cchCopy;
        WCHAR* pCur;

        pData->id = CEPERF_LOGID_SESSION_DESCRIPTOR;
        pData->session.dwSessionID = (DWORD) hSession;
        pData->session.dwStatusFlags = pFlush->pSession->settings.dwStatusFlags;
        pData->session.dwStorageFlags = pFlush->pSession->settings.dwStorageFlags;
        memcpy(pData->session.rgdwRecordingFlags, pFlush->pSession->settings.rgdwRecordingFlags, CEPERF_NUMBER_OF_TYPES*sizeof(DWORD));
        cbData = sizeof(CEPERF_LOGID) + sizeof(CEPERF_LOG_SESSION_DESCRIPTOR);
        
        // Followed by 2 strings: the session path (name) and storage path
        // (output file).  Both should be NULL terminated and max MAX_PATH.
        pCur = &pData->szPaths[0];

        if (SUCCEEDED(StringCchLength(pFlush->pSession->szSessionName, MAX_PATH, &cchCopy))) {
            StringCchCopyN(pCur, MAX_PATH, pFlush->pSession->szSessionName, cchCopy);
        } else {
            pCur[0] = 0;
            cchCopy = 0;
        }
        cbData += (cchCopy+1)*sizeof(WCHAR);
        pCur += (cchCopy+1);

        if (SUCCEEDED(StringCchLength(pFlush->pSession->settings.szStoragePath, MAX_PATH, &cchCopy))) {
            StringCchCopyN(pCur, MAX_PATH, pFlush->pSession->settings.szStoragePath, cchCopy);
        } else {
            pCur[0] = 0;
            cchCopy = 0;
        }
        cbData += (cchCopy+1)*sizeof(WCHAR);

        // Write the buffer to the output location
        return FlushBytes(pFlush, (LPBYTE) pData, cbData);
    }

    DEBUGCHK(0);  // Underestimated necessary buffer size
    return CEPERF_HR_NOT_ENOUGH_MEMORY;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Helper function to pass to WalkAllSessions.  Flush a single session.
static HRESULT FlushSession(
    HANDLE         hSession,    // Session handle is just used to create item handles
    SessionHeader* pSession,
    LPVOID         lpWalkParam  // DWORD dwFlushFlags
    )
{
    HRESULT hResult = ERROR_SUCCESS;
    HRESULT hrTemp;
    FlushState flush;

    flush.pSession = pSession;
    flush.dwFlushFlags = (DWORD) lpWalkParam;
    flush.hOutput = INVALID_HANDLE_VALUE;
    flush.lpTempBuffer = NULL;
    flush.cbTempBuffer = 0;

    DEBUGMSG(ZONE_SESSION,
             (TEXT("CePerf: Flush pSession=0x%08x %s\r\n"),
              pSession, pSession->szSessionName));

    // Check to see if the session has any data to flush or any place to flush
    // it to.
    if ((flush.dwFlushFlags & (CEPERF_FLUSH_DATA | CEPERF_FLUSH_DESCRIPTORS))
        && (pSession->settings.dwStatusFlags & CEPERF_STATUS_STORAGE_DISABLED)) {
        flush.dwFlushFlags &= ~(CEPERF_FLUSH_DATA | CEPERF_FLUSH_DESCRIPTORS);
        hResult = CEPERF_HR_STORAGE_DISABLED;
    } else if ((flush.dwFlushFlags & CEPERF_FLUSH_DATA)
               && (pSession->settings.dwStatusFlags & CEPERF_STATUS_RECORDING_DISABLED)) {
        flush.dwFlushFlags &= ~CEPERF_FLUSH_DATA;
        hResult = CEPERF_HR_RECORDING_DISABLED;
    }
    // Keep going even if data's not getting flushed, if the caller wants to clear
    if ((hResult != ERROR_SUCCESS)
        && !(flush.dwFlushFlags & CEPERF_FLUSH_AND_CLEAR)) {
        goto exit;
    }
    
    // Allocate scratch buffer
    flush.cbTempBuffer = 4096;
    flush.lpTempBuffer = LocalAlloc(LMEM_FIXED, flush.cbTempBuffer);
    if (!flush.lpTempBuffer) {
        flush.cbTempBuffer = 0;
        hResult = HRESULT_FROM_WIN32(GetLastError());
        goto exit;
    }

    // Open output file/key if necessary
    if (hResult == ERROR_SUCCESS) {
        // BUGBUG need to support case where the file is already open for continuous output
        if (pSession->settings.dwStorageFlags & CEPERF_STORE_FILE) {
            DWORD dwFileOffset = 0;
            DWORD dwFileOffsetHigh = 0;

            flush.hOutput = CreateFile(pSession->settings.szStoragePath,
                                       GENERIC_WRITE, FILE_SHARE_READ,
                                       NULL, OPEN_ALWAYS, 0, 0);
            if (flush.hOutput == INVALID_HANDLE_VALUE) {
                hResult = HRESULT_FROM_WIN32(GetLastError());
                goto exit;
            }

            dwFileOffset = SetFilePointer(flush.hOutput, 0, (long*) &dwFileOffsetHigh, FILE_END);
            if ((dwFileOffset == 0) && (dwFileOffsetHigh == 0)) {
                // This is the beginning of the file, and needs to be identified
                // as a Unicode file.
                BYTE UnicodeStamp[2] = {0xFF, 0xFE};  // Byte order mark (BOM)
                if (!(WriteFile(flush.hOutput, UnicodeStamp, 2*sizeof(BYTE), &dwFileOffsetHigh, NULL))) {
                    DEBUGMSG(ZONE_SESSION,
                             (TEXT("CePerf: Unable to designate file as Unicode text \r\n"),
                              pSession, pSession->szSessionName));
                }
            }

        } else if (pSession->settings.dwStorageFlags & CEPERF_STORE_REGISTRY) {
            WCHAR szKeyName[2*MAX_PATH];
            LONG  lResult;

            // Data is kept in a "data" subkey under the session key
            if (SUCCEEDED(hrTemp = StringCchPrintf(szKeyName, 2*MAX_PATH, TEXT("%s\\%s\\Data"),
                                                   g_szDefaultFlushSubKey, pSession->szSessionName))) {

                lResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, szKeyName, 0, NULL, 0,
#ifdef UNDER_CE
                                         0,
#else
                                         KEY_ALL_ACCESS,
#endif // UNDER_CE
                                         NULL, (HKEY*)&flush.hOutput, NULL);
                if (lResult != ERROR_SUCCESS) {
                    hResult = lResult;
                    flush.hOutput = INVALID_HANDLE_VALUE;
                    goto exit;
                }
            } else {
                hResult = hrTemp;
                flush.hOutput = INVALID_HANDLE_VALUE;
                goto exit;
            }
        
        } else if ((pSession->settings.dwStorageFlags & CEPERF_STORE_CELOG)
                   && (flush.dwFlushFlags & CEPERF_FLUSH_DESCRIPTORS)) {
            hResult = FlushSessionDescriptorBinary(&flush, hSession);
        }
    }
    
    // Do the flush in two passes: one for Durations and a second for Statistics
    // and LocalStatistics.  Not as efficient as doing it all in one pass, but
    // flush performance is not important and it makes the data a lot easier
    // to parse.
    
    // Prepare resources for flushing Durations
    hResult = DurationFlushBegin(&flush);
    if (hResult != ERROR_SUCCESS) {
        goto exit;
    }
    // Flush Durations
    hrTemp = WalkAllItems(hSession, pSession, Walk_FlushDuration, &flush, FALSE);
    if (hrTemp != ERROR_SUCCESS) {
        hResult = hrTemp;
        goto exit;
    }
    // Could clean up now but it doesn't hurt to wait
    
    // Prepare resources for flushing Statistics and LocalStatistics
    hResult = StatisticFlushBegin(&flush);
    if (hResult != ERROR_SUCCESS) {
        goto exit;
    }
    // Flush Statistics and LocalStatistics
    hrTemp = WalkAllItems(hSession, pSession, Walk_FlushStatistic, &flush, FALSE);
    if (hrTemp != ERROR_SUCCESS) {
        hResult = hrTemp;
        goto exit;
    }

exit:
    // Close output file/key if necessary
    if (flush.hOutput != INVALID_HANDLE_VALUE) {
        if (pSession->settings.dwStorageFlags & CEPERF_STORE_REGISTRY) {
            RegCloseKey((HKEY) flush.hOutput);
        } else {
            CloseHandle(flush.hOutput);
        }
    }

    // Free scratch buffer
    if (flush.lpTempBuffer) {
        LocalFree(flush.lpTempBuffer);
    }

    return hResult;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Update memory allocations and reset counters after a CPU counter change
static HRESULT UpdateAllSessionCPUCounters(
    BOOL  fForce                    // TRUE=continue even when errors occur;
                                    // used when shutting down after an error
    )
{
    HRESULT hResult = ERROR_SUCCESS;
    HRESULT hrTemp;
    WORD    wIndex;
    SessionHeader* pSession;
    
    // Wipe the data in all of the lists
    FreeAllListPages(&g_pMaster->list.offsetDurBLHead);
    FreeAllListPages(&g_pMaster->list.offsetDurDLHead);
    FreeAllListPages(&g_pMaster->list.offsetStatDataHead);
    
    // Tell every session to wipe any remaining data, and reallocate list items
    DEBUGCHK(g_pMaster->wMaxAlloc < MAX_SESSION_COUNT);
    for (wIndex = 0; wIndex <= g_pMaster->wMaxAlloc; wIndex++) {
        DWORD dwSessionRef = g_pMaster->rgSessionRef[wIndex];
        if (!MASTER_ISFREE(dwSessionRef)) {
            pSession = MASTER_SESSIONPTR(dwSessionRef);
            DEBUGCHK(pSession);
            if (pSession) {
                HANDLE hSession = MAKE_SESSION_HANDLE(MASTER_VERSION(dwSessionRef), wIndex);

                DEBUGMSG(ZONE_SESSION,
                         (TEXT("CePerf: Update CPU Counters for pSession=0x%08x %s\r\n"),
                          pSession, pSession->szSessionName));
    
                // Wipe any remaining tracked data, and reallocate list items.
                hrTemp = WalkAllItems(hSession, pSession, Walk_InitializeTypeData, NULL, TRUE);
                if ((hrTemp != ERROR_SUCCESS) && (hResult == ERROR_SUCCESS)) {
                    hResult = hrTemp;
                    if (!fForce) {
                        return hResult;
                    }
                }
            }
        }
    }
    
    return hResult;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// There is no global logger lock.  This function can be used to acquire or
// release logger locks for all sessions.
static HRESULT ModifyAllSessionLoggerLocks(
    BOOL  fAcquire      // TRUE=acquire, FALSE=release
    )
{
    HRESULT hResult = ERROR_SUCCESS;
    WORD    wIndex;
    SessionHeader* pSession;
    
    __try {

        DEBUGCHK(g_pMaster->wMaxAlloc < MAX_SESSION_COUNT);
        for (wIndex = 0; wIndex <= g_pMaster->wMaxAlloc; wIndex++) {
            DWORD dwSessionRef = g_pMaster->rgSessionRef[wIndex];
            if (!MASTER_ISFREE(dwSessionRef)) {
                pSession = MASTER_SESSIONPTR(dwSessionRef);
                DEBUGCHK(pSession);
                if (pSession) {
                    if (fAcquire) {
                        AcquireLoggerLockControl(&pSession->lock);
                    } else {
                        ReleaseLoggerLockControl(&pSession->lock);
                    }
                }
            }
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the controller lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in ModifyAllSessionLoggerLocks! (%u)\r\n"),
                 fAcquire));
        hResult = CEPERF_HR_EXCEPTION;
    }

    return hResult;
}


// Helper function prototype used with WalkAllSessions
typedef HRESULT (*PFN_SessionFunction) (HANDLE hSession, SessionHeader* pSession, LPVOID lpWalkParam);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Walk all sessions, by name or by handle.  For each session that matches the
// specified name/handle, call the supplied function with the supplied argument.
// Keeps walking until pSessionFunction returns something other than
// ERROR_SUCCESS.
static HRESULT WalkAllSessions(
    HANDLE  hSession,           // Handle of session, or INVALID_HANDLE_VALUE
                                // to refer to the session by name
    LPCWSTR lpszSessionPath,    // Name of session, or NULL to refer to the
                                // session by handle
    BOOL    fHierarchy,         // TRUE=call the function on all sessions below
                                // the specified session; FALSE=call only on the
                                // specified session
    PFN_SessionFunction pSessionFunction,// Function to call on all sessions
    LPVOID  lpWalkParam         // Argument to pass to function
    )
{
    HRESULT hResult = ERROR_SUCCESS;
    HRESULT hrTemp;
    SessionHeader* pSession;
    WORD    wIndex;
    size_t  cchLen;
    BOOL    fFound = FALSE;     // Were any matching sessions found?

    // Four cases:
    //   * Name provided,    hierarchy: scan all sessions by name
    //   * No name provided, hierarchy: lookup name from handle, then
    //                                  scan all sessions by name
    //   * Name provided,    single: scan by name until we find the session
    //   * No name provided, single: lookup session from handle
    
    wIndex = 0;        // Index to begin iterating from
    cchLen = MAX_PATH; // Length of string to compare 

    // Find session name & index from handle if necessary
    if (!lpszSessionPath) {
        // NULL + INVALID_HANDLE_VALUE means the root session
        if (hSession != INVALID_HANDLE_VALUE) {
            pSession = LookupSessionHandle(hSession);
            if (pSession) {
                lpszSessionPath = pSession->szSessionName;
                if (!fHierarchy) {
                    // Just want to call on one session, and we already know its index
                    wIndex = SESSION_INDEX(hSession);
                }
            } else {
                return CEPERF_HR_INVALID_HANDLE;
            }
        }
    }

    if (lpszSessionPath && fHierarchy) {
        cchLen = wcslen(lpszSessionPath);
    }

    // Look for sessions that the function needs to be invoked on
    DEBUGCHK(g_pMaster->wMaxAlloc < MAX_SESSION_COUNT);
    for ( ; wIndex <= g_pMaster->wMaxAlloc; wIndex++) {
        DWORD dwSessionRef = g_pMaster->rgSessionRef[wIndex];
        if (!MASTER_ISFREE(dwSessionRef)) {
            pSession = MASTER_SESSIONPTR(dwSessionRef);
            DEBUGCHK(pSession);
            if (pSession) {
                if (lpszSessionPath) {
                    if (fHierarchy) {
                        // Looking for lpszSessionPath to be a substring of pSession->szSessionName
                        if (!wcsnicmp(lpszSessionPath, pSession->szSessionName, cchLen)) {
                            fFound = TRUE;
                            hrTemp = pSessionFunction(MAKE_SESSION_HANDLE(MASTER_VERSION(dwSessionRef), wIndex),
                                                      pSession, lpWalkParam);
                            if (hrTemp != ERROR_SUCCESS) {
                                hResult = hrTemp;
                            }
                        }
                    } else {
                        // Looking for an exact match
                        if (!wcsnicmp(lpszSessionPath, pSession->szSessionName, MAX_PATH)) {
                            return pSessionFunction(MAKE_SESSION_HANDLE(MASTER_VERSION(dwSessionRef), wIndex),
                                                    pSession, lpWalkParam);
                        }
                    }
                } else {
                    // lpszSessionPath=NULL means the root session
                    if (fHierarchy) {
                        // All open sessions match
                        fFound = TRUE;
                        hrTemp = pSessionFunction(MAKE_SESSION_HANDLE(MASTER_VERSION(dwSessionRef), wIndex),
                                                  pSession, lpWalkParam);
                        if (hrTemp != ERROR_SUCCESS) {
                            hResult = hrTemp;
                        }
                    } else {
                        // Looking for an exact match
                        if (pSession->szSessionName[0] == 0) {
                            return pSessionFunction(MAKE_SESSION_HANDLE(MASTER_VERSION(dwSessionRef), wIndex),
                                                    pSession, lpWalkParam);
                        }
                    }
                }
            }
        }
    }

    if (lpszSessionPath && !fFound) {
        // Searching hierarchy by name found no sessions
        hResult = CEPERF_HR_BAD_SESSION_NAME;
    }

    return hResult;
}


//------------------------------------------------------------------------------
// CePerfOpenSession - Open an existing session or create a new one
//------------------------------------------------------------------------------
HRESULT CePerf_OpenSession(
    HANDLE* lphSession,         // Will receive handle to open perf session on
                                // success, or INVALID_HANDLE_VALUE on failure
    LPCWSTR lpszSessionPath,    // Name of session (NULL-terminated, limit
                                // MAX_PATH, case-insensitive), or NULL. If two 
                                // applications open the same session name, 
                                // they will share the same session between 
                                // them.  NULL is the global "root" session.  
                                // Use "\\" to create hierarchy.
    DWORD   dwStatusFlags,      // CEPERF_STATUS_* flags, or 0 to default to
                                // settings of existing session (if present),
                                // or to settings of parent session
    const CEPERF_SESSION_INFO* lpInfo  // Pointer to extended information about
                                // session data recording and storage, or NULL
                                // to default to settings of existing session
                                // (if present), or to settings of parent session
    )
{
    DWORD   LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT hResult = ERROR_SUCCESS;
    HANDLE  hSession = INVALID_HANDLE_VALUE;
    WORD    wIndex;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_SESSION,
             (TEXT("+CePerfOpenSession(%s, 0x%08x)\r\n"),
              lpszSessionPath, dwStatusFlags));

    if (!lphSession) {
        hResult = CEPERF_HR_INVALID_PARAMETER;
        goto exit;
    }
    *lphSession = INVALID_HANDLE_VALUE;

    // Open the master map if this is the first session for this DLL instance
    if (!g_hMasterMutex) {
        HRESULT hrTemp = OpenMasterMap();
        if (hrTemp != ERROR_SUCCESS) {
            hResult = hrTemp;
            goto exit;
        }
    }
    
    AcquireControllerLock();
    // Logger lock control is not required to create a new session
    __try {

        if (ZONE_VERBOSE) {
            DEBUGMSG(ZONE_VERBOSE, (TEXT("CEPERF GLOBALS:  (before session open)\r\n")));
            DumpMasterInfo();
            DumpDLLSessionTable();
        }
    
        // Determine whether the session is already open
        // BUGBUG linear search, may need to speed this up?
        pSession = NULL;
        DEBUGCHK(g_pMaster->wMaxAlloc < MAX_SESSION_COUNT);
        for (wIndex = 0; (wIndex <= g_pMaster->wMaxAlloc) && !pSession; wIndex++) {
            if (!MASTER_ISFREE(g_pMaster->rgSessionRef[wIndex])) {
                pSession = MASTER_SESSIONPTR(g_pMaster->rgSessionRef[wIndex]);
                if (lpszSessionPath) {
                    if (wcsnicmp(lpszSessionPath, pSession->szSessionName, MAX_PATH)) {
                        pSession = NULL;
                    }
                } else {
                    if (pSession->szSessionName[0] != 0) {
                        pSession = NULL;
                    }
                }
            }
        }
        
        if (pSession) {
            // The session is already open
            wIndex--;  // artifact of loop
            pSession->dwRefCount++;
            hSession = MAKE_SESSION_HANDLE(MASTER_VERSION(g_pMaster->rgSessionRef[wIndex]), wIndex);
            DEBUGMSG(ZONE_SESSION || ZONE_MEMORY,
                     (TEXT("CePerf: Open hSession=0x%08x pSession=0x%08x\r\n"),
                      hSession, pSession));
        
        } else {
            // The session is not open; create it
            hResult = CreateNewSession(&hSession, lpszSessionPath, dwStatusFlags, lpInfo);
        }
    
        // Add to the session table for leak cleanup on process exit
        if (hResult == ERROR_SUCCESS) {
            g_dwDLLSessionCount++;
            AddToDLLSessionTable(hSession);
            
            // Set the pointer last for safety in case of exception
            *lphSession = hSession;
        }
        
        if (ZONE_VERBOSE) {
            DEBUGMSG(ZONE_VERBOSE, (TEXT("CEPERF GLOBALS:  (after session open)\r\n")));
            DumpMasterInfo();
            DumpDLLSessionTable();
        }
        if (ZONE_SESSION && (hSession != INVALID_HANDLE_VALUE)) {
            DumpSession(hSession, FALSE, FALSE);
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CePerfOpenSession!\r\n")));
        hResult = CEPERF_HR_EXCEPTION;
    }
    ReleaseControllerLock();
    
exit:
    DEBUGMSG(ZONE_API && ZONE_SESSION,
             (TEXT("-CePerfOpenSession (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}


//------------------------------------------------------------------------------
// CePerfCloseSession - Close an open session handle
//------------------------------------------------------------------------------
HRESULT CePerf_CloseSession(
    HANDLE hSession             // Perf session handle to close
    )
{
    DWORD   LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT hResult = CEPERF_HR_INVALID_HANDLE;
    SessionHeader* pSession;

    DEBUGMSG(ZONE_API && ZONE_SESSION,
             (TEXT("+CePerfCloseSession(0x%08x)\r\n"),
              hSession));
    
    if (!g_hMasterMutex) {
        // No mutex means there aren't any open handles
        goto exit;
    }

    AcquireControllerLock();
    __try {
    
        pSession = LookupSessionHandle(hSession);
        if (pSession) {
            pSession->dwRefCount--;
            if (pSession->dwRefCount == 0) {
                hResult = CleanupSession(hSession, pSession);
            } else {
                hResult = ERROR_SUCCESS;
            }
        }
        
        // Remove from the session table
        if (hResult == ERROR_SUCCESS) {
            g_dwDLLSessionCount--;
            RemoveFromDLLSessionTable(hSession);
        }
        
        // Close the master map if this was the last session for this DLL instance
        if (g_dwDLLSessionCount == 0) {
            // BUGBUG temporary hack, hold the master map open as long as there
            // are CPU counter settings in it
            if (g_pMaster->CPU.wNumCounters == 0) {
                CloseMasterMap();  // Releases and deletes mutex!
                DEBUGCHK(*((volatile HANDLE*)&g_hMasterMutex) == NULL);
            }
        }
    
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CePerfCloseSession!\r\n")));
        hResult = CEPERF_HR_EXCEPTION;
    }

    // If the mutex still exists then we still own it
    if (*((volatile HANDLE*)&g_hMasterMutex)) {  // Force re-read of global to avoid local copy
        ReleaseControllerLock();
    }
    
exit:
    DEBUGMSG(ZONE_API && ZONE_SESSION, (TEXT("-CePerfCloseSession (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}


//------------------------------------------------------------------------------
// CePerfControlSession - Change session recording and storage settings, or
// enable/disable recording.
//------------------------------------------------------------------------------
HRESULT CePerf_ControlSession(
    HANDLE  hSession,           // Handle of session, or INVALID_HANDLE_VALUE
                                // to refer to the session by name
    LPCWSTR lpszSessionPath,    // Name of session, or NULL to refer to the
                                // session by handle
    DWORD   dwControlFlags,     // CEPERF_CONTROL_* flags
    DWORD   dwStatusFlags,      // CEPERF_STATUS_* flags, or 0 to leave unchanged
    const CEPERF_SESSION_INFO* lpInfo  // Pointer to extended information about
                                // session data recording and storage, or NULL
                                // to leave unchanged
    )
{
    DWORD       LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT     hResult = CEPERF_HR_INVALID_HANDLE;
    BOOL        fNewInstance = FALSE;
    ControlInfo info;

    DEBUGMSG(ZONE_API && ZONE_SESSION,
             (TEXT("+CePerfControlSession(0x%08x, %s, 0x%08x, 0x%08x)\r\n"),
              hSession, lpszSessionPath, dwControlFlags, dwStatusFlags));

    // Validate parameters
    if (dwControlFlags & ~VALID_CONTROL_FLAGS) {  // Others will be validated later
        hResult = CEPERF_HR_INVALID_PARAMETER;
        goto exit;
    }
    
    // Open the master map if there are no sessions open for this DLL instance.
    if (!g_hMasterMutex) {
        hResult = OpenMasterMap();
        if (hResult != ERROR_SUCCESS) {
            goto exit;
        }
        fNewInstance = TRUE;
    }
    
    AcquireControllerLock();
    __try {

        info.dwStatusFlags = dwStatusFlags;
        info.lpInfo = lpInfo;

        // Call ControlSession on the specified sessions
        hResult = WalkAllSessions(hSession, lpszSessionPath,
                                  (dwControlFlags & CEPERF_CONTROL_HIERARCHY) ? TRUE : FALSE,
                                  ControlSession, (LPVOID) &info);

        // Clean up any unused memory
        FreeUnusedDurationPages();
        FreeUnusedStatisticPages();

        // Close the master map if this was not previously open for this DLL instance
        if (fNewInstance && (g_dwDLLSessionCount == 0)) {
            CloseMasterMap();  // Releases and deletes mutex!
            DEBUGCHK(*((volatile HANDLE*)&g_hMasterMutex) == NULL);
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CePerfControlSession!\r\n")));
        hResult = CEPERF_HR_EXCEPTION;
    }
    
    // If the mutex still exists then we still own it
    if (*((volatile HANDLE*)&g_hMasterMutex)) {  // Force re-read of global to avoid local copy
        ReleaseControllerLock();
    }

exit:
    DEBUGMSG(ZONE_API && ZONE_SESSION, (TEXT("-CePerfControlSession (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}


//------------------------------------------------------------------------------
// CePerfControlCPU - Re-program the perf counters for the CPU.
//------------------------------------------------------------------------------
HRESULT CePerf_ControlCPU(
    DWORD  dwCPUFlags,          // CEPERF_CPU_* flags
    LPVOID lpCPUControlStruct,  // Control structure defined for this CPU
    DWORD  dwCPUControlSize     // Size of control structure
    )
{
    DWORD   LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT hResult = CEPERF_HR_NOT_SUPPORTED;
    HRESULT hrTemp;

    DEBUGMSG(ZONE_API && ZONE_SESSION,
             (TEXT("+CePerfControlCPU(0x%08x)\r\n"),
              dwCPUFlags));

    // Validate params
    if (!OneBitSet(dwCPUFlags & VALID_CPU_STATUS_FLAGS)
        || (dwCPUFlags & ~(VALID_CPU_STATUS_FLAGS | VALID_CPU_RECORDING_FLAGS))) {
        hResult = CEPERF_HR_INVALID_PARAMETER;
        goto exit;
    }

    // Open the master map if there are no sessions open for this DLL instance.
    if (!g_hMasterMutex) {
        hrTemp = OpenMasterMap();
        if (hrTemp != ERROR_SUCCESS) {
            hResult = hrTemp;
            goto exit;
        }
        // ... Now need to hold the master map open to maintain these settings
        // BUGBUG basically a leak, how to clean it up??
    }
    
    AcquireControllerLock();
    hResult = ModifyAllSessionLoggerLocks(TRUE);  // Block ALL logging during CPU setting changes
    if (hResult == ERROR_SUCCESS) {
        __try {
    
            if (pCPUControlPerfCounters) {
                // Tell the CPU to use the new counter settings.
                hResult = pCPUControlPerfCounters(dwCPUFlags, lpCPUControlStruct,
                                                  dwCPUControlSize);
                
                // Put new information in the master page
                if (hResult == ERROR_SUCCESS) {
                    hResult = UpdateMasterCPUCounters(g_pMaster, dwCPUFlags);
                }
                
                // Prepare all the data structures to use the new counter settings,
                // and clear all the data
                if (hResult == ERROR_SUCCESS) {
                    hResult = UpdateAllSessionCPUCounters(FALSE);
                }
    
                // All error cases fall through to here
                if (hResult != ERROR_SUCCESS)  {
                    // Try to disable the CPU perf counters, using the params
                    // that were passed in
                    dwCPUFlags &= ~CEPERF_CPU_ENABLE;
                    dwCPUFlags |= CEPERF_CPU_DISABLE;
                    pCPUControlPerfCounters(dwCPUFlags, lpCPUControlStruct, dwCPUControlSize);
                    UpdateMasterCPUCounters(g_pMaster, dwCPUFlags);
                    UpdateAllSessionCPUCounters(TRUE);
                }
    
            } else {
                hResult = CEPERF_HR_NOT_SUPPORTED;
            }
        
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Safety catch so that we release the locks no matter what
            DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CePerfControlCPU!\r\n")));
            hResult = CEPERF_HR_EXCEPTION;
        }
    }
    ModifyAllSessionLoggerLocks(FALSE);  // Resume logging
    ReleaseControllerLock();

exit:
    DEBUGMSG(ZONE_API && ZONE_SESSION, (TEXT("-CePerfControlCPU (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}


//------------------------------------------------------------------------------
// CePerfFlushSession - Write data and/or descriptors of discrete tracked items,
// or descriptors of continuous tracked items, to the storage location.
//------------------------------------------------------------------------------
HRESULT CePerf_FlushSession(
    HANDLE  hSession,           // Handle of session, or INVALID_HANDLE_VALUE
                                // to refer to the session by name
    LPCWSTR lpszSessionPath,    // Name of session, or NULL to refer to the
                                // session by handle
    DWORD   dwFlushFlags,       // CEPERF_FLUSH_* flags
    DWORD   dwReserved          // Not currently used; set to 0
    )
{
    DWORD   LastError = GetLastError();  // Explicitly preserve LastError across the call
    HRESULT hResult = CEPERF_HR_INVALID_HANDLE;
    BOOL    fNewInstance = FALSE;

    DEBUGMSG(ZONE_API && ZONE_SESSION,
             (TEXT("+CePerfFlushSession(0x%08x, %s, 0x%08x)\r\n"),
              hSession, lpszSessionPath, dwFlushFlags));

    // Validate parameters
    if ((dwFlushFlags & ~VALID_FLUSH_FLAGS)
        || (dwReserved != 0)) {
        hResult = CEPERF_HR_INVALID_PARAMETER;
        goto exit;
    }
    
    // Open the master map if there are no sessions open for this DLL instance.
    if (!g_hMasterMutex) {
        hResult = OpenMasterMap();
        if (hResult != ERROR_SUCCESS) {
            goto exit;
        }
        fNewInstance = TRUE;
    }
    
    AcquireControllerLock();
    // Logger lock control is not required to flush session, even if clearing data
    __try {
    
        // Call FlushSession on the appropriate sessions
        hResult = WalkAllSessions(hSession, lpszSessionPath,
                                  (dwFlushFlags & CEPERF_FLUSH_HIERARCHY) ? TRUE : FALSE,
                                  FlushSession, (LPVOID) dwFlushFlags);
    
        // Close the master map if this was not previously open for this DLL instance
        if (fNewInstance && (g_dwDLLSessionCount == 0)) {
            CloseMasterMap();  // Releases and deletes mutex!
            DEBUGCHK(*((volatile HANDLE*)&g_hMasterMutex) == NULL);
        }

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Safety catch so that we release the lock no matter what
        DEBUGMSG(ZONE_ERROR, (TEXT("CePerf: Exception in CePerfFlushSession!\r\n")));
        hResult = CEPERF_HR_EXCEPTION;
    }
    
    // If the mutex still exists then we still own it
    if (*((volatile HANDLE*)&g_hMasterMutex)) {  // Force re-read of global to avoid local copy
        ReleaseControllerLock();
    }

exit:
    DEBUGMSG(ZONE_API && ZONE_SESSION, (TEXT("-CePerfFlushSession (0x%08x)\r\n"), hResult));
    
    SetLastError(LastError);  // Preserve LastError across the call
    return hResult;
}

