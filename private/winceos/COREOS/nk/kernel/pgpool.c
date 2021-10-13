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

#include "kernel.h"
#include "pager.h"

const volatile DWORD PagePoolCriticalFreeMemory    =     256*1024;

PagePool_t g_LoaderPool;
PagePool_t g_FilePool;

// keeping the g_pxxxPool until we updated cedebugx.
static const PagePool_t *g_pLoaderPool = &g_LoaderPool;
static const PagePool_t *g_pFilePool   = &g_FilePool;

// Used to print more friendly debug messages
#ifdef DEBUG
const LPWSTR g_szPoolName[2] = {
    L"Loader",
    L"File"
};
#define PoolName(pPool) (g_szPoolName[(pPool) == &g_FilePool])
#endif // DEBUG

typedef void (*PAGEFN)(BOOL fCritical);
static BOOL PageOutForced;

extern CRITICAL_SECTION PageOutCS;
extern CRITICAL_SECTION MapCS;

// page out thread/event details
PTHREAD pPageOutThread;
PHDATA  phdPageOutEvent;
BYTE    g_PageOutThreadDefaultPrio = MAX_CE_PRIORITY_LEVELS - 1; // idle priority

#define PAGEOUT_CODE    (1 << 0)
#define PAGEOUT_FILE    (1 << 1)

LONG g_CntPagesNeeded; // # of pages needed to be paged out

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

    // If the enumeration cursor is set to the given process, move it to the
    // next process in the list
    EnterCriticalSection (&g_LoaderPool.cs);
    if (g_LoaderPool.EnumState.loader.pNextProcess == (PDLIST) pProcess) {
        g_LoaderPool.EnumState.loader.pNextProcess = pProcess->prclist.pFwd;
    }
    LeaveCriticalSection (&g_LoaderPool.cs);
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

    EnterCriticalSection (&g_LoaderPool.cs);

    pprc = (PPROCESS) g_LoaderPool.EnumState.loader.pNextProcess;
    g_LoaderPool.EnumState.loader.pNextProcess = pprc->prclist.pFwd;

    // Skip NK
    if (pprc == g_pprcNK) {
        if ((PPROCESS) pprc->prclist.pFwd != g_pprcNK) {
            pprc = (PPROCESS) pprc->prclist.pFwd;
            g_LoaderPool.EnumState.loader.pNextProcess = pprc->prclist.pFwd;
        } else {
            // NK is the only process
            pprc = NULL;
        }
    }

    LeaveCriticalSection (&g_LoaderPool.cs);

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

    // If the enumeration cursor is set to the given map, move it to the
    // next map in the list
    EnterCriticalSection (&g_FilePool.cs);
    if (g_FilePool.EnumState.file.pStartFile == (PDLIST) pMap) {
        g_FilePool.EnumState.file.pStartFile = pMap->maplink.pFwd;
    }
    if (g_FilePool.EnumState.file.pNextFile == (PDLIST) pMap) {
        g_FilePool.EnumState.file.pNextFile = pMap->maplink.pFwd;
    }
    if (g_FilePool.EnumState.file.pStartCriticalFile == (PDLIST) pMap) {
        g_FilePool.EnumState.file.pStartCriticalFile = pMap->maplink.pFwd;
    }
    if (g_FilePool.EnumState.file.pNextCriticalFile == (PDLIST) pMap) {
        g_FilePool.EnumState.file.pNextCriticalFile = pMap->maplink.pFwd;
    }
    LeaveCriticalSection (&g_FilePool.cs);
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

    EnterCriticalSection (&g_FilePool.cs);

    // Use different enumeration state depending on critical vs. normal mode
    if (fCritical) {
        ppEnumStart = &g_FilePool.EnumState.file.pStartCriticalFile;
        ppNext = &g_FilePool.EnumState.file.pNextCriticalFile;
    } else {
        ppEnumStart = &g_FilePool.EnumState.file.pStartFile;
        ppNext = &g_FilePool.EnumState.file.pNextFile;
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

    LeaveCriticalSection (&g_FilePool.cs);

    return pfsmap;
}

//------------------------------------------------------------------------------
// Used to determine when to exit a page-out loop, and is called even if the
// pool is actually disabled.  Returns TRUE if the caller needs to keep trimming.
//------------------------------------------------------------------------------
BOOL
PGPOOLNeedsTrim (
    PagePool_t* pPool
    )
{
    BOOL fNeedTrim = FALSE;
    
    EnterCriticalSection (&pPool->cs);

    // Keep trimming as long as:
    // a) pool cur size > target AND
    // b) either page out forced specified OR
    // c) current free count is less than threshold to stop trimmer (including required pages)
    if ((pPool->CurSize > pPool->Target)
        && (PageOutForced 
            || (PageFreeCount < (PageOutLevel + g_CntPagesNeeded)))) {
        fNeedTrim = TRUE;
    }

    LeaveCriticalSection (&pPool->cs);

    return fNeedTrim;
}

//------------------------------------------------------------------------------
// Returns NULL if memory is unavailable.  In that case the caller should loop
// and try again, perhaps freeing up other data if possible.
//------------------------------------------------------------------------------
static DWORD
PGPOOLGetPage (
    PagePool_t* pPool,
    LPDWORD     pflags
    
    )
{
    DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;

    DEBUGCHK (pPool);
    
    EnterCriticalSection (&pPool->cs);

    if (pPool->CurSize < pPool->Target) {

        // Using pool and below the target: use a page that was held for this pool
        dwPfn = GetHeldPage (pflags);
        DEBUGCHK (INVALID_PHYSICAL_ADDRESS != dwPfn);  // A held page should always be available below target

    } else {            

        // No pool or above target; use common memory.
        if (HLDPG_SUCCESS == HoldPages (1)) {
            dwPfn = GetHeldPage (pflags);
            if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
                InterlockedIncrement (&PageFreeCount_Pool);
            }                    
        }
    }
    
    if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
        pPool->CurSize++;
        DEBUGCHK (pPool->CurSize);

    } else {
        DEBUGCHK (pPool->CurSize >= pPool->Target);  // else alloc should not have failed
    }

    LeaveCriticalSection (&pPool->cs);

    DEBUGMSG(ZONE_PAGING, (L"PGPOOLGetPage (%s): returning %8.8lx - %u free\r\n",
                           PoolName(pPool), dwPfn, PageFreeCount));

    return dwPfn;
}


//------------------------------------------------------------------------------
// Called by VMDecommit and MapFreePage to return a page back to the right pool.
//------------------------------------------------------------------------------
void
PGPOOLFreePage (
    PagePool_t* pPool,
    DWORD dwPfn
    )
{
    DEBUGCHK (pPool);
    DEBUGMSG(ZONE_PAGING, (L"PGPOOLFreePage (%s): %8.8lx - %u free\r\n",
                           PoolName(pPool), dwPfn, PageFreeCount));

    EnterCriticalSection (&pPool->cs);

    if (pPool->CurSize) {
        
        if (pPool->CurSize > pPool->Target) {
            // release the page back to common memory
            FreePhysPage (dwPfn);
            InterlockedDecrement (&PageFreeCount_Pool);

        } else {
            // Below the target: return the page to held state for use by this pool
            if (!ReholdPage (dwPfn)) {
                // this can only occur when we're trying to free a paging page that 
                // is VirtualCopy'd, and there is NO physical page available. There
                // is nothing we can do but reduce the paging pool size for we can't
                // get that page back at this point. We might want to accquire pages
                // if possible when there are pages available in the future.
                pPool->Target --;
            }
        }

        pPool->CurSize--;
    }

    LeaveCriticalSection (&pPool->cs);
}

//------------------------------------------------------------------------------
// GetPagingPage - Allocate a page from pool or from common RAM.
// Caller is responsible for calling FreePagingPage.
//------------------------------------------------------------------------------
LPVOID
GetPagingPage (
    PagePool_t* pPool,
    DWORD       addr
    )
{
    LPVOID pReservation = VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0);
    LPVOID pPagingMem   = NULL;

    DEBUGCHK (!(addr & VM_PAGE_OFST_MASK)); // addr must be page aligned
    
    // Use VirtualAlloc/VirtualFree instead of allocating a physical page
    // directly, for a couple of reasons.
    // 1 - It avoids cache problems by preserving 64KB offsets and by flushing
    //     the cache during VirtualFree.
    // 2 - It is not vulnerable to bad file system implementations corrupting
    //     physical memory beyond the end of the page.
    
    // Allocate temporary memory in secure slot and perform paging there. Will
    // VirtualCopy to destination and free later.

    DEBUGMSG(ZONE_PAGING && !pReservation, (L"GetPagingPage: VA1 Failed!\r\n"));

    if (pReservation) {

        // Commit using a page-pool page
        DWORD flags = PM_PT_ZEROED;
        DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;

        // Commit a page at the same offset from 64KB as the page that's in
        // the target process, to avoid page coloring problems in the cache.
        pPagingMem = (LPVOID) ((DWORD)pReservation + (addr & VM_BLOCK_OFST_MASK));

        if (pPool) {
            dwPfn = PGPOOLGetPage (pPool, &flags);
            
        } else if (HLDPG_SUCCESS == HoldPages (1)) {
            dwPfn = GetHeldPage (&flags);
        }

        if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
            VERIFY (VMCopyPhysical (g_pprcNK, (DWORD) pPagingMem, dwPfn, 1, PageParamFormProtect (g_pprcNK, PAGE_READWRITE, (DWORD) pPagingMem), FALSE));
            if (flags & PM_PT_NEEDZERO) {
                // we got the page from dirty page list, need to zero it ourselves
                ZeroPage (pPagingMem);
            }
        } else {
            DEBUGMSG(ZONE_PAGING, (L"GetPagingPage: PPGP Failed!\r\n"));
            VMFreeAndRelease (g_pprcNK, pReservation, VM_BLOCK_SIZE);
            pPagingMem = NULL;
        }

    }

    return pPagingMem;
}


//------------------------------------------------------------------------------
// FreePagingPage - Release a page from pool or from common RAM.
//------------------------------------------------------------------------------
void
FreePagingPage (
    PagePool_t* pPool,
    LPVOID      pPagingMem
    )
{
    DWORD dwPfn;
    
    DEBUGCHK (IsInKVM ((DWORD)pPagingMem));
    DEBUGCHK (!((DWORD)pPagingMem & VM_PAGE_OFST_MASK));

    dwPfn = VMRemoveKernelPageMapping (pPagingMem);

    VMFreeAndRelease (g_pprcNK, (LPVOID)BLOCKALIGN_DOWN((DWORD)pPagingMem), VM_BLOCK_SIZE);
    
    if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
        // this is the case when paging page is not consumed (VMMove not performed, or failed).
        // Return it to the pool if necessary.
        if (pPool) {
            // Decommit back to the right pool
            PGPOOLFreePage (pPool, dwPfn);

        } else {
            FreePhysPage (dwPfn);
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
InitPoolFromParameters(
    PagePool_t* pPool,
    DWORD dwTgtPages
    )
{
    BOOL fRet = TRUE;

    // pool point to global; no need to memset.
    pPool->Target = PAGECOUNT (dwTgtPages);

    InitializeCriticalSection (&pPool->cs);
    DEBUGCHK (pPool->cs.hCrit);

    // Initialize enumeration state
    if (pPool == &g_LoaderPool) {
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

    // Pre-hold specified number of pages
    if (pPool->Target) {
        
        DEBUGMSG (ZONE_PAGING, (TEXT("PGPOOL: %s pool pages, Target=%u\r\n"), PoolName(pPool), pPool->Target));
    
        if (HLDPG_SUCCESS != HoldPages (pPool->Target)) {
            DEBUGMSG (ZONE_PAGING, (TEXT("PGPOOL: unable to reserve %u pages for %s pool, disabling\r\n"),
                                    pPool->Target, PoolName(pPool)));
            fRet = FALSE;
        }
    }

    return fRet;
}

#define NEXT_PAGEOUT_FUNC(pfn)  ((PageOutProcesses == (pfn)) ? PageOutModules : PageOutProcesses)

//------------------------------------------------------------------------------
// DoPageOut - Page out file-backed objects until enough memory is released.
//------------------------------------------------------------------------------
static BOOL
DoPageOut (
    DWORD PageOutType
    )
{       
    BOOL fContinue = FALSE;
    
    DEBUGCHK (!OwnLoaderLock (g_pprcNK));  // Check CS ordering rules
    EnterCriticalSection (&PageOutCS);

    if ((PAGEOUT_CODE & PageOutType) && PGPOOLNeedsTrim (&g_LoaderPool)) {

        static PAGEFN pfnNextPageOut = PageOutProcesses;

        pfnNextPageOut (FALSE);
        pfnNextPageOut = NEXT_PAGEOUT_FUNC (pfnNextPageOut);

        if (PGPOOLNeedsTrim (&g_LoaderPool)) {
            pfnNextPageOut (FALSE);
            // intentionally not moving pageout function to the next here.
            // If we get here, that mean the pageout call above paged out everything in
            // its class. If DoPageOut got called again, whatever pages in there
            // are going to be newer and we don't want to page them out.
        }

        // does the pool still need to be trimmed?
        fContinue = PGPOOLNeedsTrim (&g_LoaderPool);
    }

    if ((PAGEOUT_FILE & PageOutType) && PGPOOLNeedsTrim (&g_FilePool)) {

        PageOutFiles (FALSE);

        // does the pool still need to be trimmed?
        fContinue |= PGPOOLNeedsTrim (&g_FilePool);
    }
    
    LeaveCriticalSection (&PageOutCS);

    return fContinue;
}

//------------------------------------------------------------------------------
// Called by the main kernel thread to free up memory if necessary.
//------------------------------------------------------------------------------
static void 
PageOutIfNeeded (void)
{
    // Page out if we got signaled from HoldPages code
    if (InterlockedTestExchange (&PagePoolTrimState, PGPOOL_TRIM_SIGNALED, PGPOOL_TRIM_RUNNING) == PGPOOL_TRIM_SIGNALED) {
        // low memory, PageOut whatever we can, and notify PSL
        VMFreeExcessStacks (0);

        // free page pool pages held above target
        while (DoPageOut (PAGEOUT_CODE | PAGEOUT_FILE)) {}

        // free memory in psl servers
        if (PageFreeCount < PageOutTrigger) {
            EVNTModify (GetEventPtr (phdPslNotifyEvent), EVENT_SET);
        }

        // reset the pool state back to idle
        InterlockedTestExchange (&PagePoolTrimState, PGPOOL_TRIM_RUNNING, PGPOOL_TRIM_IDLE);
    }
}

//------------------------------------------------------------------------------
// Main thread to page out code/data
//------------------------------------------------------------------------------
void WINAPI
PageOutThread (
    LPVOID lpParam
    )
{
    for ( ; ; ) {
        DoWaitForObjects (1, &phdPageOutEvent, INFINITE);
        PageOutIfNeeded ();
        DEBUGMSG (ZONE_PAGING, (L"PageOutThread 0x%8.8lx, 0x%8.8lx (pagefree, pagefree_pool) \r\n", PageFreeCount, PageFreeCount_Pool));        
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
PagePoolInit ()
{
    HANDLE hTh;
    PHDATA phd;
    NKPagePoolParameters OEMParams;
    NKPagePoolParameters *pPagePoolParam = g_pOemGlobal->pPagePoolParam;    
    long lMinMemoryPage = 2 * (pPagePoolParam->File.Target+pPagePoolParam->Loader.Target) / VM_PAGE_SIZE;

    DEBUGCHK (pPagePoolParam);

    // read in the page out low and high watermark
    g_cpPageOutLow = (g_pOemGlobal->cpPageOutLow) ? g_pOemGlobal->cpPageOutLow : PAGEOUT_LOW;
    g_cpPageOutHigh = (g_pOemGlobal->cpPageOutHigh) ? g_pOemGlobal->cpPageOutHigh : PAGEOUT_HIGH;

    // start the page out trigger and level with these values
    // these will be re-adjusted later in the boot when shell
    // calls to set the low/critical memory thresholds.
    PageOutTrigger = g_cpPageOutLow;
    PageOutLevel = g_cpPageOutHigh;
    
    // Call an IOCTL to allow the OEM to adjust parameters at run-time
    // Only overwrite params if OEM IOCTL results are valid
    memcpy (&OEMParams, pPagePoolParam, sizeof(NKPagePoolParameters));
    if (OEMIoControl (IOCTL_HAL_GET_POOL_PARAMETERS, &OEMParams,
                      sizeof(NKPagePoolParameters), NULL, 0, NULL)
        && (CURRENT_PAGE_POOL_VERSION == OEMParams.NKVersion)
        && (CURRENT_PAGE_POOL_VERSION == OEMParams.OEMVersion)) {
        memcpy (pPagePoolParam, &OEMParams, sizeof(NKPagePoolParameters));
    }

    // enable page pool only if we have enough pages on boot (currently set to 2 * pool targets)
    if (PageFreeCount <= lMinMemoryPage) {
        // disable page pool
        pPagePoolParam->Loader.Target = pPagePoolParam->File.Target = 0;
        DEBUGMSG (ZONE_PAGING, (L"Not enough memory. Page pool disabled\r\n"));                
    }

    // set up the page out thread (must be done before HoldPages is called)
    hTh = CreateKernelThread ((LPTHREAD_START_ROUTINE)PageOutThread,0,g_PageOutThreadDefaultPrio,0);
    DEBUGCHK (hTh);
    phd = LockHandleData (hTh, g_pprcNK);
    pPageOutThread = GetThreadPtr(phd);
    UnlockHandleData (phd);
    HNDLCloseHandle (g_pprcNK, hTh);

    if (!InitPoolFromParameters (&g_LoaderPool, pPagePoolParam->Loader.Target)) {
        // could not pre-commit tgt pages; disable pool.
        g_LoaderPool.Target = 0;
        DEBUGMSG(ZONE_ERROR, (TEXT("PGPOOL: Not enough pages to pre-commit %d pages. Disabling %s pool\r\n"), pPagePoolParam->Loader.Target, PoolName(&g_LoaderPool)));
    } else {        
        DEBUGMSG(g_LoaderPool.Target, (TEXT("PGPOOL: Reserved %u pages for %s pool\r\n"), g_LoaderPool.Target, PoolName(&g_LoaderPool)));
    }

    if (!InitPoolFromParameters (&g_FilePool, pPagePoolParam->File.Target)) {
        // could not pre-commit tgt pages; disable pool.
        g_FilePool.Target = 0;
        DEBUGMSG(ZONE_ERROR, (TEXT("PGPOOL: Not enough pages to pre-commit %d pages. Disabling %s pool\r\n"), pPagePoolParam->Loader.Target, PoolName(&g_FilePool)));
    } else {
        DEBUGMSG(g_FilePool.Target, (TEXT("PGPOOL: Reserved %u pages for %s pool\r\n"), g_FilePool.Target, PoolName(&g_FilePool)));
    }

    // initialize the page pool trim state to idle
    PagePoolTrimState = PGPOOL_TRIM_IDLE;
    
    return TRUE;
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
    DEBUGCHK (pPool);

    EnterCriticalSection (&pPool->cs);

    pParams->Target              = pPool->Target * VM_PAGE_SIZE;
    pState->CurrentSize          = pPool->CurSize * VM_PAGE_SIZE;

    LeaveCriticalSection (&pPool->cs);

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
        PGPOOLGetInfo (&g_LoaderPool, &pState->LoaderState, &pState->LoaderParams);
        PGPOOLGetInfo (&g_FilePool, &pState->FileState, &pState->FileParams);
        return TRUE;
    }
    
    return FALSE;
}

//------------------------------------------------------------------------------
// ForcePageout API
//------------------------------------------------------------------------------
void NKForcePageout (void)
{
    TRUSTED_API_VOID (L"NKForcePageout");

    VMFreeExcessStacks (0);

    PageOutForced = 1;
    DoPageOut(PAGEOUT_CODE | PAGEOUT_FILE);
    PageOutForced = 0;
}

