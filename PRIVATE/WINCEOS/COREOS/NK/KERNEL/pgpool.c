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

#include "kernel.h"
#include "pager.h"


// Can be overwritten as FIXUPVARs in config.bib, eg:
//    kernel.dll:LoaderPoolTarget             00000000 00300000 FIXUPVAR
//    kernel.dll:LoaderPoolMaximum            00000000 00800000 FIXUPVAR
//    kernel.dll:FilePoolTarget               00000000 00100000 FIXUPVAR
//    kernel.dll:FilePoolMaximum              00000000 00A00000 FIXUPVAR

const volatile DWORD LoaderPoolTarget              =  3*1024*1024;
const volatile DWORD LoaderPoolMaximum             =  8*1024*1024;
const volatile DWORD LoaderPoolReleaseIncrement    =     128*1024;
const volatile DWORD LoaderPoolCriticalIncrement   =      64*1024;
const volatile DWORD LoaderPoolNormalPriority256   =          255;
const volatile DWORD LoaderPoolCriticalPriority256 =          247;

const volatile DWORD FilePoolTarget                =  1*1024*1024;
const volatile DWORD FilePoolMaximum               = 10*1024*1024;
const volatile DWORD FilePoolReleaseIncrement      =      64*1024;
const volatile DWORD FilePoolCriticalIncrement     =      64*1024;
const volatile DWORD FilePoolNormalPriority256     =          255;
const volatile DWORD FilePoolCriticalPriority256   =          251;

const volatile DWORD PagePoolCriticalFreeMemory    =     256*1024;

PagePool_t *g_pLoaderPool, *g_pFilePool;

// Used to print more friendly debug messages
#ifdef DEBUG
const LPWSTR g_szPoolName[2] = {
    L"Loader",
    L"File"
};
#define PoolName(pPool) (g_szPoolName[(pPool) == g_pFilePool])
#endif // DEBUG


typedef void PAGEFN(BOOL fCritical);
static BOOL PageOutForced;
extern long PageOutNeeded;
extern CRITICAL_SECTION PhysCS, PageOutCS;
#ifdef DEBUG
extern CRITICAL_SECTION MapCS;  // Used to check critical section usage
#endif
extern DWORD currmaxprio;

#define PAGEOUT_EXE     (1 << 0)
#define PAGEOUT_DLL     (1 << 1)
#define PAGEOUT_FILE    (1 << 2)
static void DoPageOut (DWORD PageOutType, BOOL fCritical);
static void PGPOOLSetCritical (PagePool_t* pPool, DWORD ThreadPrio256);


#ifdef DEBUG

static LONG g_PoolHeldCount;  // Number of held pages

static __inline CheckHeldCount ()
{
    if (g_pFilePool)   EnterCriticalSection (&g_pFilePool->cs);
    if (g_pLoaderPool) EnterCriticalSection (&g_pLoaderPool->cs);
    
    // g_PoolHeldCount check: Target1 + Target2 - held = min(Target1, CurSize1) + min(Target2, CurSize2)
    DEBUGCHK ((g_pLoaderPool ? (g_pLoaderPool->Target - min(g_pLoaderPool->Target, g_pLoaderPool->CurSize)) : 0)
              + (g_pFilePool ? (g_pFilePool->Target - min(g_pFilePool->Target, g_pFilePool->CurSize)) : 0)
              == g_PoolHeldCount);
    
    if (g_pLoaderPool) LeaveCriticalSection (&g_pLoaderPool->cs);
    if (g_pFilePool)   LeaveCriticalSection (&g_pFilePool->cs);
}

#else  // DEBUG
#define CheckHeldCount()
#endif // DEBUG


//------------------------------------------------------------------------------
// Notifies the page pool about process deletion.  Remove the process from the
// trim thread's enumeration state, if it's there.
//------------------------------------------------------------------------------
void
PagePoolDeleteProcess (
    PPROCESS pProcess
    )
{
    DEBUGCHK (OwnModListLock ());  // Process list cannot change beneath us
    DEBUGCHK (pProcess);

    // Check if the pool is being used and has a trim thread
    if (g_pLoaderPool && g_pLoaderPool->phdTrimThread) {
        // If the enumeration cursor is set to the given process, move it to the
        // next process in the list
        EnterCriticalSection (&g_pLoaderPool->cs);
        if (g_pLoaderPool->EnumState.loader.pNextProcess == (PDLIST) pProcess) {
            g_pLoaderPool->EnumState.loader.pNextProcess = pProcess->prclist.pFwd;
        }
        LeaveCriticalSection (&g_pLoaderPool->cs);
    }
}


//------------------------------------------------------------------------------
// Skips NK.  Returns NULL if NK is the only process.  Does not track where
// enumeration started.
//------------------------------------------------------------------------------
PPROCESS
PagePoolGetNextProcess ()
{
    PPROCESS pprc = NULL;

    DEBUGCHK (OwnModListLock ());  // Process list cannot change beneath us

    DEBUGCHK (g_pLoaderPool);
    if (g_pLoaderPool) {
        EnterCriticalSection (&g_pLoaderPool->cs);

        pprc = (PPROCESS) g_pLoaderPool->EnumState.loader.pNextProcess;
        g_pLoaderPool->EnumState.loader.pNextProcess = pprc->prclist.pFwd;

        // Skip NK
        if (pprc == g_pprcNK) {
            if ((PPROCESS) pprc->prclist.pFwd != g_pprcNK) {
                pprc = (PPROCESS) pprc->prclist.pFwd;
                g_pLoaderPool->EnumState.loader.pNextProcess = pprc->prclist.pFwd;
            } else {
                // NK is the only process
                pprc = NULL;
            }
        }

        LeaveCriticalSection (&g_pLoaderPool->cs);
    }

    return pprc;
}


//------------------------------------------------------------------------------
// Notifies the page pool about file deletion.  Remove the file from the
// trim thread's enumeration state, if it's there.
//------------------------------------------------------------------------------
void
PagePoolDeleteFile (
    PFSMAP pMap
    )
{
    DEBUGCHK (OwnCS (&MapCS));  // Lists cannot change beneath us
    DEBUGCHK (pMap);

    // Check if the pool is being used and has a trim thread
    if (g_pFilePool && g_pFilePool->phdTrimThread) {
        // If the enumeration cursor is set to the given map, move it to the
        // next map in the list
        EnterCriticalSection (&g_pFilePool->cs);
        if (g_pFilePool->EnumState.file.pStartFile == (PDLIST) pMap) {
            g_pFilePool->EnumState.file.pStartFile = pMap->maplink.pFwd;
        }
        if (g_pFilePool->EnumState.file.pNextFile == (PDLIST) pMap) {
            g_pFilePool->EnumState.file.pNextFile = pMap->maplink.pFwd;
        }
        if (g_pFilePool->EnumState.file.pStartCriticalFile == (PDLIST) pMap) {
            g_pFilePool->EnumState.file.pStartCriticalFile = pMap->maplink.pFwd;
        }
        if (g_pFilePool->EnumState.file.pNextCriticalFile == (PDLIST) pMap) {
            g_pFilePool->EnumState.file.pNextCriticalFile = pMap->maplink.pFwd;
        }
        LeaveCriticalSection (&g_pFilePool->cs);
    }
}


//------------------------------------------------------------------------------
// Tracks where the enumeration started, and returns NULL if we've enumerated
// through all the maps and gotten back to where we started.
//------------------------------------------------------------------------------
PFSMAP
PagePoolGetNextFile (
    BOOL fEnumerationStart,  // TRUE: reset the start position, FALSE: check if we looped
    BOOL fCritical
    )
{
    PFSMAP  pfsmap = NULL;
    PDLIST *ppNext, *ppEnumStart;

    DEBUGCHK (OwnCS (&MapCS));

    DEBUGCHK (g_pFilePool);
    if (g_pFilePool) {
        EnterCriticalSection (&g_pFilePool->cs);

        // Use different enumeration state depending on critical vs. normal mode
        if (fCritical) {
            ppEnumStart = &g_pFilePool->EnumState.file.pStartCriticalFile;
            ppNext = &g_pFilePool->EnumState.file.pNextCriticalFile;
        } else {
            ppEnumStart = &g_pFilePool->EnumState.file.pStartFile;
            ppNext = &g_pFilePool->EnumState.file.pNextFile;
        }

        // Figure out the next item to be enumerated
        if (*ppNext == &g_MapList) {
            pfsmap = (PFSMAP) g_MapList.pFwd;
        } else {
            pfsmap = (PFSMAP) *ppNext;
        }

        // Track the enumeration start position
        if (fEnumerationStart) {
            // Mark this spot as the start of enumeration
            *ppEnumStart = (PDLIST) pfsmap;
        } else {
            // Check if we've returned back to where we started enumerating
            if ((PDLIST) pfsmap == *ppEnumStart) {
                pfsmap = NULL;
            }
        }
        
        // Move enumeration to the next map if we found something
        if (pfsmap) {
            *ppNext = pfsmap->maplink.pFwd;
        }

        LeaveCriticalSection (&g_pFilePool->cs);
    }

    return pfsmap;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static __inline void
LeaveCritical (
    PagePool_t* pPool
    )
{
    DEBUGCHK (OwnCS (&pPool->cs));

    // Adding the current time to the critical time results in
    // (CriticalTime) = (End Time) - (Start Time).
    
    pPool->Flags &= ~(PGPOOL_FLAG_CRITICAL | PGPOOL_FLAG_CRITICAL_WRITE);
    pPool->CriticalTime += (KCall ((PKFN) GetCurThreadUTime) + KCall ((PKFN) GetCurThreadKTime));
    
    // No attempt to stay above currmaxprio during OOM, because we won't leave
    // critical state during OOM until we're below the pool target size.
    if (THRDSetPrio256 (pCurThread, pPool->NormalThreadPriority)) {
        pPool->CurThreadPriority = pPool->NormalThreadPriority;
    }
    
    DEBUGMSG(ZONE_PAGING, (L"PGPOOL: %s TrimThread return to normal priority\r\n",
                           PoolName(pPool)));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static __inline void
LeaveTrim (
    PagePool_t* pPool
    )
{
    DEBUGCHK (OwnCS (&pPool->cs));

    pPool->Flags &= ~PGPOOL_FLAG_TRIMMING;
    NKResetEvent (g_pprcNK, pPool->hTrimEvent);
    pPool->TrimTime = (KCall ((PKFN) GetCurThreadUTime) + KCall ((PKFN) GetCurThreadKTime));
}


//------------------------------------------------------------------------------
// Common trim thread implementation, used for both pools.
//------------------------------------------------------------------------------
static DWORD WINAPI
PagePoolTrimThread (
    LPVOID pvarg    // pool pointer
    )
{
    PagePool_t* pPool = (PagePool_t*) pvarg;
    DWORD       PageOutFlags;

    DEBUGCHK (pPool && pPool->phdTrimThread);

    if (pPool == g_pLoaderPool) {
        PageOutFlags = PAGEOUT_EXE | PAGEOUT_DLL;
    } else {
        PageOutFlags = PAGEOUT_FILE;
    }

    while ((WAIT_OBJECT_0 == NKWaitForSingleObject (pPool->hTrimEvent, INFINITE))
           && !(pPool->Flags & PGPOOL_FLAG_EXIT)) {

        EnterCriticalSection (&pPool->cs);
        
        DEBUGMSG(ZONE_PAGING, (L"PGPOOL: %s TrimThread starting, pool size %u target %u max %u\r\n",
                               PoolName(pPool), pPool->CurSize, pPool->Target, pPool->Maximum));
        
        DEBUGCHK (PoolIsTrimming (pPool));
        do {
            // TRUE=critical mode, R/O pages only.
            // FALSE=non-critical mode, R/O and R/W pages.
            BOOL fCriticalMode = (PoolIsCritical (pPool)
                                  && !(pPool->Flags & PGPOOL_FLAG_CRITICAL_WRITE));
            
            // Page out without holding the critical section.
            LeaveCriticalSection (&pPool->cs);
            DoPageOut (PageOutFlags, fCriticalMode);
            EnterCriticalSection (&pPool->cs);

            if (PoolIsCritical (pPool)) {
                // Leave critical if:
                if (!PoolAboveTrimThreshold (pPool)                         // Below target so paging out would not free up system memory
                    || (PoolCriticalIsComplete (pPool)                      // Below critical level for pool
                        && EnoughMemoryToLeaveCritical ()                   // Below critical level for system
                        && (pPool->NormalThreadPriority <= currmaxprio))) { // Above OOM maximum thread priority
                    // Clear the critical flag and return self to normal priority
                    LeaveCritical (pPool);
                } else {
                    // Possibility #1: There was a race condition where another
                    // thread consumed pages while we didn't hold the CS.
                    // No problem, go back for another loop.

                    // Possibility #2: File pool, there are so many dirty pages
                    // that we need to flush in order to free up enough RAM.
                    // Enter critical-flush mode.
                    if (pPool == g_pFilePool) {
                        pPool->Flags |= PGPOOL_FLAG_CRITICAL_WRITE;
                    }
                }
            }
        
        } while (!PoolTrimIsComplete (pPool));

        // Done looping, clear the trim flag
        DEBUGCHK (!PoolIsCritical (pPool));
        LeaveTrim (pPool);
        
        DEBUGMSG(ZONE_PAGING, (L"PGPOOL: %s TrimThread waiting, pool size %u target %u max %u\r\n",
                               PoolName(pPool), pPool->CurSize, pPool->Target, pPool->Maximum));
        
        LeaveCriticalSection (&pPool->cs);
    }

    HNDLCloseHandle (g_pprcNK, pPool->hTrimEvent);
    memset (pPool, 0, sizeof(PagePool_t));
    
    DEBUGMSG(ZONE_PAGING, (L"PGPOOL: %s TrimThread exit!\r\n", PoolName(pPool)));
    return 0;
}


//------------------------------------------------------------------------------
// Used to determine when to exit a page-out loop, and is called even if the
// pool is actually disabled.  Returns TRUE if the caller needs to keep trimming
// in the current state (critical or not).  Returns FALSE if the pool no longer
// needs trimming or if the caller needs to change state.
//------------------------------------------------------------------------------
BOOL
PGPOOLNeedsTrim (
    PagePool_t* pPool,
    BOOL        fCritical   // Is the caller trimming in critical mode or normal?
    )
{
    // Note that the return value may be wrong as soon as this call returns, but
    // that's okay because we're only really ballparking anyway.
    
    if (pPool && pPool->Maximum) {
        // Pool is in use
        if (pPool->phdTrimThread) {
            // Pool can exceed its target
            BOOL fNeedTrim;
            
            // If the caller is in the wrong mode we want it to stop its current
            // trim cycle.
            EnterCriticalSection (&pPool->cs);
            if (fCritical || PoolIsCritical (pPool)) {
                // We need to keep trimming if:
                //      Not yet below critical level for pool.
                //      The system is low on memory, paging out will return memory
                //          to the system, not the pool.
                //      We're in OOM, and running at "normal" priority would block
                //          the trim thread.
                fNeedTrim = !PoolCriticalIsComplete (pPool)
                            || (!EnoughMemoryToLeaveCritical ()
                                && PoolAboveTrimThreshold (pPool))
                            || (pPool->NormalThreadPriority > currmaxprio);
            } else {
                fNeedTrim = !PoolTrimIsComplete (pPool);
            }
            LeaveCriticalSection (&pPool->cs);

            return fNeedTrim;

        } else {
            // Maximum == Target, pool will never be trimmed
            return FALSE;
        }
    }

    // If the loader pool is not being used, then use other factors to decide
    // if pages need to be released.
    return (PageOutNeeded || PageOutForced);
}


//------------------------------------------------------------------------------
// Returns NULL if memory is unavailable.  In that case the caller should loop
// and try again, perhaps freeing up other data if possible.
//------------------------------------------------------------------------------
static LPVOID
PGPOOLGetPage (
    PagePool_t* pPool
    )
{
    LPVOID pPage = NULL;

    if (pPool) {
        EnterCriticalSection (&pPool->cs);

        // Adjust the trim thread state if necessary
        if (pPool->phdTrimThread) {
            if (!PoolIsTrimming (pPool)) {
                if (PoolAboveTrimThreshold (pPool)) {
                    // About to cross over the target threshold -- wake the trimmer thread
                    DEBUGMSG(ZONE_PAGING, (L"PGPOOLGetPage: Waking %s TrimThread at normal priority\r\n",
                                           PoolName(pPool)));

                    pPool->Flags |= PGPOOL_FLAG_TRIMMING;
                    pPool->TrimCount++;
                    // pPool->TrimTime is managed by the trim thread
                    NKSetEvent (g_pprcNK, pPool->hTrimEvent);
                }

            } else if (!PoolIsCritical (pPool)
                       && (PoolAboveCriticalThreshold (pPool)
                           || !EnoughMemoryToLeaveCritical ())) {
                // About to cross over the critical threshold -- raise the trimmer
                // thread to critical priority
                PGPOOLSetCritical (pPool, pPool->CriticalThreadPriority);
            }
        }

        // Get a page, if available
        if (!pPool->Maximum) {
            // Maximum = 0 means no real pool, just use common memory
            pPage = GrabOnePage (PM_PT_ZEROED);

        } else if (pPool->CurSize < pPool->Target) {
            // Below the target: use a page that was held for this pool
            pPage = Pfn2Virt (GetHeldPage (PM_PT_ZEROED));
            DEBUGCHK (pPage);  // A held page should always be available below target
#ifdef DEBUG
            InterlockedDecrement(&g_PoolHeldCount);
#endif

        } else if (pPool->CurSize < pPool->Maximum) {
            // Above the target and below the max: try to get a page from common memory
            pPage = GrabOnePage (PM_PT_ZEROED);
        }
        
        if (pPage) {
            pPool->CurSize++;
            DEBUGCHK (pPool->CurSize);
        } else {
            DEBUGCHK (pPool->CurSize >= pPool->Target);  // else alloc should not have failed
            pPool->FailCount++;
        
            // No pages left.  Boost the trim thread to the priority of this thread.
            PGPOOLSetCritical (pPool, GET_CPRIO (pCurThread));
        }

        LeaveCriticalSection (&pPool->cs);

        CheckHeldCount ();

        DEBUGMSG(ZONE_PAGING, (L"PGPOOLGetPage (%s): returning %8.8lx - %u free\r\n",
                               PoolName(pPool), pPage, PageFreeCount));
    }

    return pPage;
}


//------------------------------------------------------------------------------
// Called by VMDecommit and MapFreePage to return a page back to the right pool.
//------------------------------------------------------------------------------
void
PGPOOLFreePage (
    PagePool_t* pPool,
    LPVOID pPage
    )
{
    if (pPool) {
        DEBUGMSG(ZONE_PAGING, (L"PGPOOLFreePage (%s): %8.8lx - %u free\r\n",
                               PoolName(pPool), pPage, PageFreeCount));

        EnterCriticalSection (&pPool->cs);

        DEBUGCHK (pPool->CurSize);
        if (pPool->CurSize) {
            if (!pPool->Maximum || (pPool->CurSize > pPool->Target)) {
                // No real pool or above the target: release the page back to common memory
                FreePhysPage (GetPFN (pPage));
            } else {
                // Below the target: return the page to held state for use by this pool
                if (ReholdPage (GetPFN (pPage))) {
#ifdef DEBUG
                    InterlockedIncrement(&g_PoolHeldCount);
#endif
                } else {
                    // this can only occur when we're trying to free a paging page that 
                    // is VirtualCopy'd, and there is NO physical page available. There
                    // is nothing we can do but reduce the paging pool size for we can't
                    // get that page back at this point. We might want to accquire pages
                    // if possible when there are pages available in the future.
                    if (pPool->Maximum == pPool->Target) {
                        pPool->Maximum --;  // no trimmer thread, update max too
                    }
                    pPool->Target --;
                    if (pPool->ReleaseIncrement > pPool->Target)
                        pPool->ReleaseIncrement = pPool->Target;
                    if (pPool->CriticalIncrement > (pPool->Maximum - pPool->Target) / 2)
                        pPool->CriticalIncrement = (pPool->Maximum - pPool->Target) / 2;
                }
            }

            pPool->CurSize--;

            // Don't adjust the trim thread state; let the thread do that itself
        }

        LeaveCriticalSection (&pPool->cs);

        CheckHeldCount ();
    }
}


//------------------------------------------------------------------------------
// GetPagingPage - Allocate a page from pool or from common RAM.
// Caller is responsible for calling FreePagingPage.
//------------------------------------------------------------------------------
LPVOID
GetPagingPage (
    PagePool_t* pPool,
    LPVOID*     ppReservation,
    DWORD       addr,
    BOOL        fUsePool,
    DWORD       fProtect
    )
{
    LPVOID pReservation = NULL;
    LPVOID pPagingMem = NULL;
    
    // Use VirtualAlloc/VirtualFree instead of allocating a physical page
    // directly, for a couple of reasons.
    // 1 - It avoids cache problems by preserving 64KB offsets and by flushing
    //     the cache during VirtualFree.
    // 2 - It is not vulnerable to bad file system implementations corrupting
    //     physical memory beyond the end of the page.
    
    // Allocate temporary memory in secure slot and perform paging there. Will
    // VirtualCopy to destination and free later.
    pReservation = VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0);
    if (!pReservation) {
        DEBUGMSG(ZONE_PAGING, (L"GetPagingPage: VA1 Failed!\r\n"));
        return NULL;
    }
    
    // Commit a page at the same offset from 64KB as the page that's in
    // the target process, to avoid page coloring problems in the cache.
    pPagingMem = (LPVOID) ((DWORD)pReservation + (addr & VM_BLOCK_OFST_MASK));

    if (fUsePool && pPool) {
        // Commit using a page-pool page
        LPVOID pTemp = PGPOOLGetPage (pPool);
        if (pTemp) {
            // pTemp is a static-mapped address so the VirtualCopy will not
            // increment the physical page refcount, which is perfect.
            DEBUGCHK (IsKernelVa (pTemp));
            if (!VMFastCopy (g_pprcNK, (DWORD) pPagingMem, g_pprcNK,
                             (DWORD) pTemp, VM_PAGE_SIZE, fProtect)) {
                DEBUGMSG(ZONE_PAGING, (L"GetPagingPage: VMFC Failed!\r\n"));
                PGPOOLFreePage (pPool, pTemp);
                pPagingMem = NULL;
            }
        } else {
            DEBUGMSG(ZONE_PAGING, (L"GetPagingPage: PPGP Failed!\r\n"));
            pPagingMem = NULL;
        }
    } else {
        // Commit from common RAM
        if (!VMAlloc (g_pprcNK, pPagingMem, VM_PAGE_SIZE, MEM_COMMIT, fProtect)) {
            DEBUGMSG(ZONE_PAGING, (L"GetPagingPage: VA2 Failed!\r\n"));
            pPagingMem = NULL;
        }
    }
    
    if (!pPagingMem) {
        VERIFY (VMFreeAndRelease (g_pprcNK, pReservation, VM_BLOCK_SIZE));
    } else {
        *ppReservation = pReservation;
    }

    return pPagingMem;
}


//------------------------------------------------------------------------------
// FreePagingPage - Release a page from pool or from common RAM.
//------------------------------------------------------------------------------
void
FreePagingPage (
    PagePool_t* pPool,
    LPVOID      pPagingMem,
    LPVOID      pReservation,
    BOOL        fFromPool,
    DWORD       PagingResult
    )
{
    // Decommit the page and return it to the pool if necessary.
    if ((PAGEIN_SUCCESS != PagingResult) && pPagingMem && fFromPool && pPool) {
        // Decommit back to the right pool
        VMDecommit (g_pprcNK, pPagingMem, VM_PAGE_SIZE,
                    (pPool == g_pFilePool) ? (VM_PAGER_MAPPER | VM_PAGER_POOL)
                                           : (VM_PAGER_LOADER | VM_PAGER_POOL));
    }

    if (pReservation) {
        VERIFY (VMFreeAndRelease (g_pprcNK, pReservation, VM_BLOCK_SIZE));
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
InitPoolFromParameters(
    PagePool_t* pPool,
    PagePoolParameters* pParams
    )
{
    memset (pPool, 0, sizeof(PagePool_t));

    pPool->Target                 = (WORD) PAGECOUNT (pParams->Target);
    pPool->Maximum                = (WORD) PAGECOUNT (pParams->Maximum);
    pPool->ReleaseIncrement       = (WORD) PAGECOUNT (pParams->ReleaseIncrement);
    pPool->CriticalIncrement      = (WORD) PAGECOUNT (pParams->CriticalIncrement);
    pPool->NormalThreadPriority   = pParams->NormalPriority256;
    pPool->CriticalThreadPriority = pParams->CriticalPriority256;

    // Sanity check the relationships between parameters
    if (pPool->Target > pPool->Maximum)
        pPool->Target = pPool->Maximum;
    if (pPool->ReleaseIncrement > pPool->Target)
        pPool->ReleaseIncrement = pPool->Target;
    if (pPool->CriticalIncrement > (pPool->Maximum - pPool->Target) / 2)
        pPool->CriticalIncrement = (pPool->Maximum - pPool->Target) / 2;
    if (pPool->NormalThreadPriority > 255)
        pPool->NormalThreadPriority = 255;
    if (pPool->CriticalThreadPriority > 255)
        pPool->CriticalThreadPriority = 255;
    if (pPool->CriticalThreadPriority > pPool->NormalThreadPriority)
        pPool->CriticalThreadPriority = pPool->NormalThreadPriority;
    pPool->CurThreadPriority = pPool->NormalThreadPriority;

    // Target == Maximum == 0 means no pool
    if (pPool->Target && pPool->Maximum) {

        DEBUGMSG (ZONE_PAGING, (TEXT("PGPOOL: %s pool pages, Target=%u Max=%u ReleaseIncr=%u CritIncr=%u Prio=%u CritPrio=%u\r\n"),
                                PoolName(pPool), pPool->Target, pPool->Maximum,
                                pPool->ReleaseIncrement, pPool->CriticalIncrement,
                                pPool->NormalThreadPriority, pPool->CriticalThreadPriority));

        // Target == Maximum means no trim thread or event
        if (pPool->Target != pPool->Maximum) {
            HANDLE hTrimThread;

            if (!(pPool->hTrimEvent = NKCreateEvent (NULL, TRUE, FALSE, NULL))
                || !(hTrimThread = CreateKernelThread (PagePoolTrimThread, pPool,
                                                       pPool->NormalThreadPriority, 0))) {
                if (pPool->hTrimEvent) {
                    HNDLCloseHandle (g_pprcNK, pPool->hTrimEvent);
                }
                memset (pPool, 0, sizeof(PagePool_t));
                return FALSE;
            }
            
            // Lock to get the PHDATA but unlock immediately
            pPool->phdTrimThread = LockHandleData (hTrimThread, g_pprcNK);
            UnlockHandleData (pPool->phdTrimThread);

        } else {
            DEBUGMSG (ZONE_PAGING, (TEXT("PGPOOL: no trim thread for %s pool\r\n"),
                                    PoolName(pPool)));
        }
    
        if (!HoldPages (pPool->Target, FALSE)) {
            DEBUGMSG (ZONE_PAGING, (TEXT("PGPOOL: unable to reserve %u pages for %s pool, disabling\r\n"),
                                    pPool->Target, PoolName(pPool)));
            // Get the thread to exit and clean up
            pPool->Flags |= PGPOOL_FLAG_EXIT;
            NKSetEvent (g_pprcNK, pPool->hTrimEvent);
            return FALSE;
        }

#ifdef DEBUG
        g_PoolHeldCount += pPool->Target;
#endif
    }

    InitializeCriticalSection (&pPool->cs);
    DEBUGCHK (pPool->cs.hCrit);

    // Initialize enumeration state
    if (pPool == g_pLoaderPool) {
        pPool->EnumState.loader.pNextProcess = (PDLIST) g_pprcNK;
        DEBUGCHK (NULL == csarray[CSARRAY_LOADERPOOL]);  // Not yet assigned to anything
        csarray[CSARRAY_LOADERPOOL] = &pPool->cs;
    } else {
        pPool->EnumState.file.pStartFile         = &g_MapList;
        pPool->EnumState.file.pNextFile          = &g_MapList;
        pPool->EnumState.file.pStartCriticalFile = &g_MapList;
        pPool->EnumState.file.pNextCriticalFile  = &g_MapList;
        DEBUGCHK (NULL == csarray[CSARRAY_FILEPOOL]);  // Not yet assigned to anything
        csarray[CSARRAY_FILEPOOL] = &pPool->cs;
    }

    return TRUE;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
PagePoolInit ()
{
    NKPagePoolParameters params, OEMParams;
    BOOL fResult = FALSE;

    g_pLoaderPool = AllocMem (HEAP_PAGEPOOL);
    if (!g_pLoaderPool) {
        return FALSE;
    }

    g_pFilePool = AllocMem (HEAP_PAGEPOOL);
    if (!g_pFilePool) {
        FreeMem (g_pLoaderPool, HEAP_PAGEPOOL);
        g_pLoaderPool = NULL;
        return FALSE;
    }
    
    // Collect all the pool parameters
    params.NKVersion = 1;
    params.OEMVersion = 0;

    params.Loader.Target              = LoaderPoolTarget;
    params.Loader.Maximum             = LoaderPoolMaximum;
    params.Loader.ReleaseIncrement    = LoaderPoolReleaseIncrement;
    params.Loader.CriticalIncrement   = LoaderPoolCriticalIncrement;
    params.Loader.NormalPriority256   = (WORD) LoaderPoolNormalPriority256;
    params.Loader.CriticalPriority256 = (WORD) LoaderPoolCriticalPriority256;
    
    params.File.Target                = FilePoolTarget;
    params.File.Maximum               = FilePoolMaximum;
    params.File.ReleaseIncrement      = FilePoolReleaseIncrement;
    params.File.CriticalIncrement     = FilePoolCriticalIncrement;
    params.File.NormalPriority256     = (WORD) FilePoolNormalPriority256;
    params.File.CriticalPriority256   = (WORD) FilePoolCriticalPriority256;
    
    // Call an IOCTL to allow the OEM to adjust parameters at run-time
    // Only overwrite params if OEM IOCTL results are valid
    memcpy (&OEMParams, &params, sizeof(NKPagePoolParameters));
    if (OEMIoControl (IOCTL_HAL_GET_POOL_PARAMETERS, &OEMParams,
                      sizeof(NKPagePoolParameters), NULL, 0, NULL)
        && (1 == OEMParams.NKVersion)
        && (1 == OEMParams.OEMVersion)) {
        memcpy (&params, &OEMParams, sizeof(NKPagePoolParameters));
    }
    
    InitPoolFromParameters (g_pLoaderPool, &params.Loader);
    DEBUGMSG(g_pLoaderPool->Target, (TEXT("PGPOOL: Reserved %u pages for %s pool\r\n"), g_pLoaderPool->Target, PoolName(g_pLoaderPool)));
    
    InitPoolFromParameters (g_pFilePool, &params.File);
    DEBUGMSG(g_pFilePool->Target, (TEXT("PGPOOL: Reserved %u pages for %s pool\r\n"), g_pFilePool->Target, PoolName(g_pFilePool)));

    return TRUE;
}


//------------------------------------------------------------------------------
// Put the pool into critical state if it's not already so.  Set the thread prio
// as necessary.  Called when the pool needs to enter critical.  Also called
// when the pool runs out of memory, and during OOM to ensure that the pool trim
// thread continues to run when low-priority threads are blocked from running.
//------------------------------------------------------------------------------
static void
PGPOOLSetCritical (
    PagePool_t* pPool,
    DWORD ThreadPrio256
    )
{
    if (pPool) {
        EnterCriticalSection (&pPool->cs);

        if (pPool->phdTrimThread) {
            PTHREAD pth = GetThreadPtr (pPool->phdTrimThread);

            // Enter critical mode if above target and not already critical
            if (PoolIsTrimming (pPool) && !PoolIsCritical (pPool)) {
                pPool->Flags |= PGPOOL_FLAG_CRITICAL;
                pPool->CriticalCount++;

                // Subtract the current time from the critical time.  When the
                // thread leaves critical status it will add the current time,
                // resulting in (CriticalTime) = (End Time) - (Start Time).
                pPool->CriticalTime -= (pth->dwUTime + pth->dwKTime);
            }

            // Boost the trim thread to the necessary priority
            if (pPool->CurThreadPriority > ThreadPrio256) {
                DEBUGMSG (ZONE_PAGING, (L"PGPOOLGetPage: Raising %s TrimThread to prio %u\r\n",
                                        PoolName (pPool), ThreadPrio256));
                if (THRDSetPrio256 (pPool->phdTrimThread->pvObj, ThreadPrio256)) {
                    pPool->CurThreadPriority = (WORD) ThreadPrio256;
                }
            }
        }

        LeaveCriticalSection (&pPool->cs);
    }
}

void
SetPagingPoolPrio (
    DWORD ThreadPrio256
    )
{
    // Hack to guarantee that we page out before the OOM thread pops up
    // the out-of-memory dialog.  If the prio is greater than idle, set
    // the pool to one level above the current (OOM) thread.
    // BUGBUG this won't work if we have multiple processors; we probably
    // should call NKForcePageout here but it's not clear if that's safe
    // to do on the gwes OOM thread.
    if (ThreadPrio256 < (MAX_CE_PRIORITY_LEVELS - 1)) {
        DWORD CurThreadPrio = GET_CPRIO (pCurThread);
        if (CurThreadPrio > 0) {
            ThreadPrio256 = CurThreadPrio - 1;
        }
    }

    // The pools may not actually be above their targets, so the threads
    // may not actually be running now.  But keep their prios up to date.
    PGPOOLSetCritical (g_pLoaderPool, ThreadPrio256);
    PGPOOLSetCritical (g_pFilePool, ThreadPrio256);
}


//------------------------------------------------------------------------------
// Helper for PagePoolGetState / IOCTL_KLIB_GET_POOL_STATE.
//------------------------------------------------------------------------------
static void
PGPOOLGetInfo (
    PagePool_t* pPool,
    PagePoolState* pState,          // Receives current pool state
    PagePoolParameters* pParams     // Receives pool parameters
    )
{
    if (pPool) {

        EnterCriticalSection (&pPool->cs);

        pParams->Target              = pPool->Target * VM_PAGE_SIZE;
        pParams->Maximum             = pPool->Maximum * VM_PAGE_SIZE;
        pParams->ReleaseIncrement    = pPool->ReleaseIncrement * VM_PAGE_SIZE;
        pParams->CriticalIncrement   = pPool->CriticalIncrement * VM_PAGE_SIZE;
        pParams->NormalPriority256   = pPool->NormalThreadPriority;
        pParams->CriticalPriority256 = pPool->CriticalThreadPriority;

        pState->CurrentSize          = pPool->CurSize * VM_PAGE_SIZE;
        pState->TrimCount            = pPool->TrimCount;
        pState->TrimTime             = 0;
        pState->CriticalCount        = pPool->CriticalCount;
        pState->FailCount            = pPool->FailCount;
        // Current thread priority is not exposed right now

        if (pPool->phdTrimThread) {
            PTHREAD pth = GetThreadPtr (pPool->phdTrimThread);
            pState->TrimTime         = (pth->dwUTime + pth->dwKTime);
        }
        
        if (PoolIsCritical (pPool)) {
            // This return value is not correct -- it is an overestimation.
            // It would only be correct if the trim thread had not been
            // switched out since it entered critical state.
            pState->CriticalTime     = pPool->CriticalTime + OEMGetTickCount();
        } else {
            pState->CriticalTime     = pPool->CriticalTime;
        }

        LeaveCriticalSection (&pPool->cs);

    } else {
        memset (pParams, 0, sizeof(PagePoolParameters));
        memset (pState, 0, sizeof(PagePoolState));
    }
}


//------------------------------------------------------------------------------
// Worker function for IOCTL_KLIB_GET_POOL_STATE.
//------------------------------------------------------------------------------
BOOL
PagePoolGetState (
    NKPagePoolState* pState
    )
{
    if (1 == pState->Version) {
        PGPOOLGetInfo (g_pLoaderPool, &pState->LoaderState, &pState->LoaderParams);
        PGPOOLGetInfo (g_pFilePool, &pState->FileState, &pState->FileParams);
        return TRUE;
    }
    
    return FALSE;
}


//------------------------------------------------------------------------------
// DoPageOut - Page out file-backed objects until enough memory is released.
//------------------------------------------------------------------------------
static void
DoPageOut (
    DWORD PageOutType,
    BOOL  fCritical
    )
{
    // PageOutFunc ordering matches PAGEOUT_ flags
    static PAGEFN * const PageOutFunc[3] = { PageOutProcesses, PageOutModules, PageOutFiles };
    static int state;
    int tries = 0;
    
    DEBUGCHK (!OwnLoaderLock (g_pprcNK));  // Check CS ordering rules
    EnterCriticalSection (&PageOutCS);

    while (tries++ < 3) {
        // Call only the specified functions, and only if necessary/prudent
        if ((PageOutType & (1 << state))
            && PGPOOLNeedsTrim ((2 == state) ? g_pFilePool : g_pLoaderPool, fCritical)) {
            (PageOutFunc[state]) (fCritical);
        }
        state = (state+1) % 3;
    }
    
    LeaveCriticalSection (&PageOutCS);
}


//------------------------------------------------------------------------------
// Called by the main kernel thread to free up memory if necessary.
//------------------------------------------------------------------------------
void PageOutIfNeeded (void)
{
    if (InterlockedTestExchange (&PageOutNeeded, 1, 2) == 1) {
        // low memory, PageOut whatever we can, and notify PSL
        VMFreeExcessStacks (0);
        do {
            DoPageOut (PAGEOUT_EXE | PAGEOUT_DLL | PAGEOUT_FILE, FALSE);
            NKPSLNotify (DLL_MEMORY_LOW, 0, 0);
        } while (InterlockedTestExchange (&PageOutNeeded, 1, 2) == 1);
    }
}


//------------------------------------------------------------------------------
// ForcePageout API
//------------------------------------------------------------------------------
void NKForcePageout (void)
{
    TRUSTED_API_VOID (L"NKForcePageout");

    VMFreeExcessStacks (0);

    PageOutForced = 1;
    DoPageOut(PAGEOUT_EXE | PAGEOUT_DLL | PAGEOUT_FILE, FALSE);
    PageOutForced = 0;
}

