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
#include <snapboot.h>

PagePool_t g_PagingPool;

// keeping the g_pxxxPool until we updated cedebugx.
static PagePool_t g_FilePool;
static const PagePool_t *g_pLoaderPool = &g_PagingPool;
static const PagePool_t *g_pFilePool   = &g_FilePool;

#define DEFAULT_PAGE_POOL_TARGET    0x200       // 2M

// page out thread/event details
PTHREAD pPageOutThread;
PTHREAD pWriteBackPageOutThread;
BYTE    g_PageOutThreadDefaultPrio   = CE_THREAD_PRIO_256_NORMAL;   // normal priority

LONG g_CntPagesNeeded; // # of pages needed to be paged out

extern PHDATA phdEvtWriteBackPageOut;
extern PHDATA phdEvtPageOut;

static DLIST g_pageoutList;

extern CRITICAL_SECTION ListWriteCS;

CRITICAL_SECTION PagePoolCS;

MEMORYSTATISTICS g_MemStat;

extern void LockModule (PMODULE pMod);
extern void UnlockModule (PMODULE pMod);

//------------------------------------------------------------------------------
// Remove an object from page pool's pageout list
//------------------------------------------------------------------------------
void PagePoolRemoveObject (PPAGEOUTOBJ pPageoutObj, BOOL fMakeInvalid)
{
    EnterCriticalSection (&ListWriteCS);
    RemoveDList (&pPageoutObj->link);
    InitDList (&pPageoutObj->link);
    if (fMakeInvalid) {
        pPageoutObj->eType = eInvalid;
    }
    LeaveCriticalSection (&ListWriteCS);
}

static void AddPageoutObject (PDLIST pdl, PPAGEOUTOBJ pPageoutObj)
{
    EnterCriticalSection (&ListWriteCS);
    RemoveDList (&pPageoutObj->link);
    if (eInvalid != pPageoutObj->eType) {
        AddToDListTail (pdl, &pPageoutObj->link);
    } else {
        InitDList (&pPageoutObj->link);
    }
    LeaveCriticalSection (&ListWriteCS);
}

//------------------------------------------------------------------------------
// Add an object into the page pool's pageout list
//------------------------------------------------------------------------------
void PagePoolAddObject (PPAGEOUTOBJ pPageoutObj)
{
    AddPageoutObject (&g_pageoutList, pPageoutObj);
}

BOOL EnumRemoveAndLockObject (PDLIST pitem, LPVOID fFileOnly)
{
    PPAGEOUTOBJ pPageoutObj = (PPAGEOUTOBJ) pitem;

    switch (pPageoutObj->eType) {
    case eProcess:
        if (fFileOnly) {
            pPageoutObj = NULL;
            break;
        }
        // fall through
        __fallthrough;
        
    case eMapFile:
        LockHDATA ((PHDATA) pPageoutObj->pObj);
        break;
    case eModule:
        if (fFileOnly) {
            pPageoutObj = NULL;
            break;
        }
        LockModule ((PMODULE) pPageoutObj->pObj);
        break;
    case ePageTree:
        PageTreeIncRef ((PPAGETREE) pPageoutObj->pObj);
        break;
    default:
        pPageoutObj = NULL;
        DEBUGCHK (0);
        break;
    }

    return NULL != pPageoutObj;    
}


//------------------------------------------------------------------------------
// Used to determine when to exit a page-out loop, and is called even if the
// pool is actually disabled.  Returns TRUE if the caller needs to keep trimming.
//------------------------------------------------------------------------------
BOOL
PGPOOLNeedsTrim (
    PagePool_t* pPool,
    LONG        cpTrimmerStop
    )
{
    BOOL fNeedTrim;
    
    EnterCriticalSection (&PagePoolCS);

    // Keep trimming as long as:
    // a) pool cur size >= target AND (note: the "=" is required to guarantee forward progress)
    // b) current free count is less than threshold to stop trimmer (including required pages)
    fNeedTrim = (pPool->CurSize >= pPool->Target)
             && (PageFreeCount < (cpTrimmerStop + g_CntPagesNeeded));

    LeaveCriticalSection (&PagePoolCS);

    return fNeedTrim;
}

//------------------------------------------------------------------------------
// Returns NULL if memory is unavailable.  In that case the caller should loop
// and try again, perhaps freeing up other data if possible.
//------------------------------------------------------------------------------
DWORD
PGPOOLGetPage (
    PagePool_t* pPool,
    LPDWORD     pflags,
    DWORD       dwMemPrio
    )
{
    DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;

    PREFAST_DEBUGCHK (pPool);
    
    EnterCriticalSection (&PagePoolCS);

    if (pPool->CurSize < pPool->Target) {

        // Using pool and below the target: use a page that was held for this pool
        dwPfn = GetHeldPage (pflags);
        DEBUGCHK (INVALID_PHYSICAL_ADDRESS != dwPfn);  // A held page should always be available below target

    } else {            

        // No pool or above target; use common memory.
        dwPfn = GetPagingCache (pflags, dwMemPrio);
    }
    
    if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
        pPool->CurSize++;
        DEBUGCHK (pPool->CurSize);

    } else {
        DEBUGCHK (pPool->CurSize >= pPool->Target);  // else alloc should not have failed
    }

    LeaveCriticalSection (&PagePoolCS);

    DEBUGMSG(ZONE_PAGING, (L"PGPOOLGetPage: returning %8.8lx - %u free\r\n",
                           dwPfn, PageFreeCount));

    return dwPfn;
}


//------------------------------------------------------------------------------
// Called by VMDecommitCodePages and MapFreePage to return a page back to the right pool.
//------------------------------------------------------------------------------
void
PGPOOLFreePage (
    PagePool_t* pPool,
    DWORD dwPfn,
    LPDWORD pdwEntry
    )
{
    PREFAST_DEBUGCHK (pPool);
    DEBUGMSG(ZONE_PAGING, (L"PGPOOLFreePage: %8.8lx - %u free\r\n", dwPfn, PageFreeCount));

    EnterCriticalSection (&PagePoolCS);

    if (pPool->CurSize) {
        BOOL fPageReleased;
            
        if (pPool->CurSize > pPool->Target) {
            // release the page back to common memory
            fPageReleased = FreePagingCache (dwPfn, pdwEntry);

        } else {
            // rehold the page
            fPageReleased = ReholdPagingPage (dwPfn, pdwEntry);
        }

        if (fPageReleased) {
            pPool->CurSize--;
        }
    }

    LeaveCriticalSection (&PagePoolCS);
}

DWORD GetOnePageForPaging (PagePool_t* pPool, LPDWORD pdwFlags, DWORD dwMemPrio)
{
    DWORD dwPfn;
    if (pPool) {
        dwPfn = PGPOOLGetPage (pPool, pdwFlags, dwMemPrio);
        
    } else if (HLDPG_SUCCESS == HoldMemory (dwMemPrio, 1)) {
        dwPfn = GetHeldPage (pdwFlags);
    
    } else {
        dwPfn = INVALID_PHYSICAL_ADDRESS;
    }
    return dwPfn;
}

void FreeOnePagingPage (PagePool_t *pPool, DWORD dwPfn)
{
    if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
        // this is the case when paging page is not consumed (VMMove not performed, or failed).
        // Return it to the pool if necessary.
        if (pPool) {
            // Decommit back to the right pool
            PGPOOLFreePage (pPool, dwPfn, NULL);
        } else {
            FreePhysPage (dwPfn);
        }
    }
}


//------------------------------------------------------------------------------
// GetPagingPage - Allocate a page from pool or from common RAM.
//------------------------------------------------------------------------------
LPVOID
GetPagingPage (
    PagePool_t* pPool,
    PPROCESS    pprc,
    DWORD       dwAddr
    )
{
    LPVOID pReservation = VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0);
    LPVOID pPagingMem   = NULL;

    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK)); // addr must be page aligned
    DEBUGMSG(ZONE_PAGING && !pReservation, (L"GetPagingPage: VA1 Failed!\r\n"));

    if (pReservation) {
        
        DWORD flags = PM_PT_ZEROED;  
        DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;
        DWORD dwMemPrio = CalcMemPrioForPaging (pprc);

        dwPfn = GetOnePageForPaging (pPool, &flags, dwMemPrio);

        if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
            // VirtualCopy the page at the same offset from 64KB as the page that's in
            // the target process, to avoid page coloring problems in the cache.               
            pPagingMem = (LPVOID) ((DWORD) pReservation + (dwAddr & VM_BLOCK_OFST_MASK));
            VERIFY (VMCreateKernelPageMapping (pPagingMem, dwPfn));
            if (flags & PM_PT_NEEDZERO) {
                // we got the page from dirty page list, need to zero it ourselves
                ZeroPage (pPagingMem);
            }
        
        } else {
            DEBUGMSG(ZONE_PAGING, (L"GetPagingPage: PPGP Failed!\r\n"));
            VMFreeAndRelease (g_pprcNK, pReservation, VM_BLOCK_SIZE);
        }
    }

    return pPagingMem;
}

//------------------------------------------------------------------------------
// FreePagingPage - Release a page to pool
//------------------------------------------------------------------------------
void
FreePagingPage (
    PagePool_t* pPool,    
    LPVOID      pPagingMem
    )
{    
    DWORD dwPfn;
    
    DEBUGCHK (IsInKVM ((DWORD)pPagingMem));
    DEBUGCHK (IsPageAligned (pPagingMem));

    dwPfn = VMRemoveKernelPageMapping (pPagingMem);        
    VMFreeAndRelease (g_pprcNK, (LPVOID)(BLOCKALIGN_DOWN((DWORD)pPagingMem)), VM_BLOCK_SIZE);
    FreeOnePagingPage (pPool, dwPfn);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void UpdatePagePoolTarget (
    PagePool_t* pPool,
    DWORD dwTgtPages
    )
{
    DWORD dwOldTarget = pPool->Target;

    if ((dwTgtPages > dwOldTarget)
        && (HLDPG_SUCCESS == HoldMemory (MEMPRIO_HEALTHY, dwTgtPages - dwOldTarget))) {

        pPool->Target = dwTgtPages;
        DEBUGMSG (ZONE_PAGING, (TEXT("PGPOOL: PagePoolTarget set to 0x%x pages\r\n"), dwOldTarget));
        
    } else if (dwTgtPages != dwOldTarget) {
        RETAILMSG (1, (L"Fail to set page pool target to 0x%x pages, target unchanged (0x%x)\r\n", dwOldTarget));
    }
}

extern LONG g_cpMinDirectAvailable;
#define EXTRA_PAGES_FOR_WRITE_BACK      0x100

#define PAGEOUT_FORCE                   0x0001
#define PAGEOUT_DECOMMIT_ONLY           0x0002
#define PAGEOUT_FILE_ONLY               0x0004

BOOL PageOutNextObject (PDLIST pLocalList, LPBYTE pReuseVM, DWORD dwFlags)
{
    TPageoutType eType = eInvalid;
    PPAGEOUTOBJ pPageoutObj;

    EnterCriticalSection (&ListWriteCS);

    pPageoutObj = (PPAGEOUTOBJ) EnumerateDList (&g_pageoutList, EnumRemoveAndLockObject, (LPVOID) (dwFlags & PAGEOUT_FILE_ONLY));
    if (pPageoutObj) {
        RemoveDList (&pPageoutObj->link);
        InitDList (&pPageoutObj->link);

        eType = pPageoutObj->eType;
    }

    LeaveCriticalSection (&ListWriteCS);

    if (pPageoutObj) {

        // page out the object
        switch (eType) {
        case eProcess:
            {
                PPROCESS pprc = GetProcPtr (pPageoutObj->pObj);
                
                if (pprc) {
                    g_MemStat.cProcessPagedOut ++;

                    if (!PageOutOneProcess (pprc)) {
                        // not fully paged out, add back to the list
                        AddPageoutObject (pLocalList, pPageoutObj);
                    }
                }

                UnlockHandleData (pPageoutObj->pObj);
            }
            break;
        case eModule:
            {
                PMODULE pMod = (PMODULE) pPageoutObj->pObj;

                g_MemStat.cModulePagedOut ++;
                
                if (!PageOutOneModule (pMod)) {
                    // not fully paged out, add back to the list
                    AddPageoutObject (pLocalList, pPageoutObj);
                }

                UnlockModule (pMod);
            }
            break;
        case eMapFile:
            {
                PFSMAP pfsmap = GetMapFilePtr (pPageoutObj->pObj);

                g_MemStat.cMapFilePagedOut ++;

                if (!PageOutOneMapFile (pfsmap, (dwFlags & PAGEOUT_DECOMMIT_ONLY), pReuseVM)) {
                    // not fully paged out, add back to the list
                    AddPageoutObject (pLocalList, pPageoutObj);
                }

                UnlockHandleData (pPageoutObj->pObj);
            }
            break;
        case ePageTree:
            {
                PPAGETREE ppgtree = pPageoutObj->pObj;

                g_MemStat.cMapFilePagedOut ++;

                if (!PageOutOnePageTree (ppgtree, (dwFlags & PAGEOUT_DECOMMIT_ONLY), pReuseVM)) {
                    // not fully paged out, add back to the list
                    AddPageoutObject (pLocalList, pPageoutObj);
                }

                PageTreeDecRef (ppgtree);
            }
            break;
        default:
            // we should've never get this
            DEBUGCHK (0);
            break;
        }
    }

    return NULL != pPageoutObj;

}

void PageOutNextFile (LPBYTE pReuseVM)
{
    PageOutNextObject (&g_pageoutList, pReuseVM, PAGEOUT_FILE_ONLY);
}

//------------------------------------------------------------------------------
// DoPageOut - worker function to page out pageable objects
//------------------------------------------------------------------------------
void DoPageOut (DWORD dwFlags, LONG cpTrimmerStop, LPBYTE pReuseVM)
{
    DLIST       localPageoutList;
    BOOL        fForce = (dwFlags & PAGEOUT_FORCE);

    // never call this function with PAGEOUT_FILE_ONLY, it can lead to indefinite loop
    DEBUGCHK (!(dwFlags & PAGEOUT_FILE_ONLY));

    InitDList (&localPageoutList);
    
    while (fForce || PGPOOLNeedsTrim (&g_PagingPool, cpTrimmerStop)) {

        if (!PageOutNextObject (&localPageoutList, pReuseVM, dwFlags)) {
            break;
        }

    }

    if (!IsDListEmpty (&localPageoutList)) {
        EnterCriticalSection (&ListWriteCS);
        MergeDList (&g_pageoutList, &localPageoutList);
        LeaveCriticalSection (&ListWriteCS);
    }
}

//-------------------------------------------------------------------------------------
// Thread to write-back and page-out code/data. The thread starts running when
// the direct available memory is below g_cpTrimStart, or PGFStartPageOut is called.
//-------------------------------------------------------------------------------------
void WINAPI WriteBackPageOutThread (void)
{
    LPBYTE pFlushVM = VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0);
    for ( ; ; ) {
        DoWaitForObjects (1, &phdEvtWriteBackPageOut, INFINITE);

        VMFreeExcessStacks (0);

        DoPageOut (0, g_cpTrimmerStop, pFlushVM);
        
        DEBUGMSG (ZONE_PAGING, (L"WriteBackPageOutThread 0x%8.8lx, 0x%8.8lx (pagefree, pagefree_pool) \r\n", PageFreeCount, PageFreeCount_Pool));        
    }
}

// # of pages extra to trim when page out thread is signaled to avoid thrashing.
#define EXTRA_ABOVE_MIN_AVAILABLE       0x100

//---------------------------------------------------------------------------------------------------
// Thread to page out code/data when direct avail memory is very low (under g_cpMinDirectAvailable).
// NOTE: This thread MUST never block on any external lock.
//---------------------------------------------------------------------------------------------------
void WINAPI PageOutThread (void)
{
    PTHREAD pCurTh = pCurThread;
    for ( ; ; ) {
        DoWaitForObjects (1, &phdEvtPageOut, INFINITE);

        VMFreeExcessStacks (0);
        DoPageOut (PAGEOUT_DECOMMIT_ONLY, g_cpMinDirectAvailable+EXTRA_ABOVE_MIN_AVAILABLE, NULL);

        if (PGPOOLNeedsTrim (&g_PagingPool, g_cpMinDirectAvailable)) {
            // after we page out everything, we still don't have enough memory.
            // Raise the priority of writeback thread and hope it can get us more memory.

            SCHL_SetThreadBasePrio (pWriteBackPageOutThread, GET_CPRIO (pCurTh));
            NKSleep (250);
            SCHL_SetThreadBasePrio (pWriteBackPageOutThread, CE_THREAD_PRIO_256_IDLE);
        }

        DEBUGMSG (ZONE_PAGING, (L"PageOutThread 0x%8.8lx, 0x%8.8lx (pagefree, pagefree_pool) \r\n", PageFreeCount, PageFreeCount_Pool));        
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
PagePoolInit ()
{
    PHDATA phd;

    InitDList (&g_pageoutList);

    UpdatePagePoolTarget (&g_PagingPool, DEFAULT_PAGE_POOL_TARGET);

    // set up the page out thread (must be done before HoldMemory is called)
    phd = THRDCreate (g_pprcNK, (FARPROC) PageOutThread, NULL, KRN_STACK_SIZE, TH_KMODE, g_PageOutThreadDefaultPrio|THRD_NO_BASE_FUNCTION);
    pPageOutThread = GetThreadPtr(phd);
    DEBUGCHK (pPageOutThread);
    THRDResume (pPageOutThread, NULL);

    // create write-back-pageout thread
    phd = THRDCreate (g_pprcNK, (FARPROC) WriteBackPageOutThread, NULL, KRN_STACK_SIZE, TH_KMODE, CE_THREAD_PRIO_256_IDLE|THRD_NO_BASE_FUNCTION);
    pWriteBackPageOutThread = GetThreadPtr (phd);
    DEBUGCHK (pWriteBackPageOutThread);
    THRDResume (pWriteBackPageOutThread, NULL);

#ifndef SHIP_BUILD
    OEMWriteDebugString(TEXT("PagePoolInit Complete\r\n"));
#endif
    
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
    PREFAST_DEBUGCHK (pPool);

    EnterCriticalSection (&PagePoolCS);

    pParams->Target              = pPool->Target * VM_PAGE_SIZE;
    pState->CurrentSize          = pPool->CurSize * VM_PAGE_SIZE;

    LeaveCriticalSection (&PagePoolCS);

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
        memset (pState, 0, sizeof (NKPagePoolState));
        pState->Version = 1;
        PGPOOLGetInfo (&g_PagingPool, &pState->LoaderState, &pState->LoaderParams);
        return TRUE;
    }
    
    return FALSE;
}

extern TSnapState g_snapState;
//------------------------------------------------------------------------------
// ForcePageout API
//------------------------------------------------------------------------------
void NKForcePageout (void)
{
    TRUSTED_API_VOID (L"NKForcePageout");

    if ((eSnapPass1 != g_snapState) && (eSnapPass2 != g_snapState)) {
        VMFreeExcessStacks (0);

        DoPageOut (PAGEOUT_FORCE, 0, NULL);
    }
}

