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
#include <sqmutil.h>
#include <pager.h>

//
// ASSERTION:
//  (1) all pages in the clean/dirty list cannot have any dirty cache entry except for the 1st 8 byts, which is used
//      for doubly linked list.
//  (2) Except for calling KC_GrabFirstPhysicalPage directly, when a page is handed out (removed from either
//      dirty or clean list), there is no dirty entry in cache for this page.
//


#ifdef PHYSPAGESTOLEDS
#define LogPhysicalPages(PageFreeCount) OEMWriteDebugLED(0,PageFreeCount);
#else
#define LogPhysicalPages(PageFreeCount) (0)
#endif


#define INIT_ZERO_PAGE_COUNT    16

#define FILESYS_PAGE        0xffff
#define MAX_REFCNT          0xfff0

////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory request rules:
//  1) any application can allocate as long as memory is above "regular app memory threshold"
//  2) start terminating apps as soon as memory drop below "low".
//  3) termination stopped when memory is above healthy
//  4) "good memory" event is set when memory is above healthy
//  5) "memory pressure" event is set when memory is below g_cpMemPressure
//  6) "memory low" event is set when memory is below g_cpMemLow
//

#define DEFAULT_THRESHOLD_KERNEL_LOW            0x100           // default kernel low threshold
#define DEFAULT_THRESHOLD_APP_CRITICAL          0x200           // default app critical threshold
#define DEFAULT_THRESHOLD_APP_LOW               0x400           // default app low threadhold
#define DEFAULT_THRESHOLD_HEALTHY               0x800           // default healthy threshold

#define TIMEOUT_TERMINATE_APPS                  1000

LONG g_cpMemoryThreshold[NUM_MEM_TYPES] = {
    MEMORY_KERNEL_PANIC,                        // (MEMPRIO_KERNEL_INTERNAL) kernel internal allocation succeed above this
    
    MEMORY_KERNEL_CRITICAL,                     // (MEMPRIO_KERNEL) extern kernel allocation succeed above this
    
    DEFAULT_THRESHOLD_KERNEL_LOW,               // (MEMPRIO_SYSTEM) allocation from system process succeed above this
    
    DEFAULT_THRESHOLD_APP_CRITICAL,             // (MEMPRIO_FOREGROUND) allocation from foreground process succeed above this
    
    DEFAULT_THRESHOLD_APP_LOW,                  // (MEMPRIO_REGULAR_APPS) any allocation succeed above this
                                                // NOTE: "memory low event" signaled if drop below this
                                                //       and kernel will start terminate background processes.

    // g_cpMemLow                               // 12.5% above app low
                                                // NOTE: "memory low event" signaled if drop below this level

    // g_cpMemPressure                          // 12.5% below healthy memory,
                                                // NOTE: "memory pressure event" signaled if drop below this
    
    DEFAULT_THRESHOLD_HEALTHY,                  // (MEMPRIO_HEALTHY) "good memory event" signaled if above this.
};

// good memory event
PHDATA phdEvtGoodMem;
PEVENT pEvtGoodMem;

// memory above foreground threshold event
PHDATA phdEvtFgMemOk;
PEVENT pEvtFgMemOk;

// memory pressure event
PHDATA phdEvtMemPressure;
PEVENT pEvtMemPressure;

// memory low event
PHDATA phdEvtMemLow;
PEVENT pEvtMemLow;

// pressure trigger
LONG g_cpMemPressure;

// memory low trigger
LONG g_cpMemLow;

// max number of pages for a single allocation
LONG g_cpMaxAlloc = 0x80000;                    // initial value of g_cpMaxAlloc == 2GB == 0x80000 pages          

////////////////////////////////////////////////////////////////////////////////////////////
// page trimmer:
//  1) start paging out pages as soon as direct available memory drop blow g_cpTrimmerStart
//  2) page out stopped when direct available memory is above g_cpTrimmerStop.
//  3) page trimmer thread priority adjusted when there are threads block waiting for trimmer to pageout memory
//  4) "page pool trimmed" event is signaled when direct available memory is above cpThrimmedThreshold.

#define DEFAULT_THRESHOLD_MIN_DIRECT_AVAIL      DEFAULT_THRESHOLD_KERNEL_LOW
#define DEFAULT_THRESHOLD_TRIMMER_START         DEFAULT_THRESHOLD_APP_CRITICAL
#define DEFAULT_THRESHOLD_TRIMMER_STOP          DEFAULT_THRESHOLD_APP_LOW

LONG g_cpMinDirectAvailable = DEFAULT_THRESHOLD_MIN_DIRECT_AVAIL;   // memory request will wait for trimmer thread to page out pages 
                                                                    // if PageFreeCount is below this

LONG g_cpTrimmerStart = DEFAULT_THRESHOLD_TRIMMER_START;              // page pool trimmer started if PageFreeCount is below this value
LONG g_cpTrimmerStop  = DEFAULT_THRESHOLD_TRIMMER_STOP;               // page pool trimmer stopped if PageFreeCount is above this value

// pageout event
PHDATA phdEvtWriteBackPageOut;
PEVENT pEvtWriteBackPageOut;

PHDATA phdEvtPageOut;
PEVENT pEvtPageOut;

// page pool trimmed event
PHDATA phdEvtPagePoolTrimmed;
PEVENT pEvtPagePoolTrimmed;

// value initialized in paging pool
extern PTHREAD pPageOutThread;
extern BYTE g_PageOutThreadDefaultPrio;

void UpdateMemoryState (void);

////////////////////////////////////////////////////////////////////////////////////////////
// physical memory manager
#define FIRST_ZEROED_PAGE(pUseMap)              (((pUseMap)[IDX_ZEROED_PAGE_LIST]).idxFwd)
#define FIRST_UNZEROED_PAGE(pUseMap)            (((pUseMap)[IDX_UNZEROED_PAGE_LIST]).idxFwd)
#define FIRST_DIRTY_PAGE(pUseMap)               (((pUseMap)[IDX_DIRTY_PAGE_LIST]).idxFwd)

#define IS_PAGE_LIST_EMPTY(pUseMap, idxHead)    ((idxHead) == (pUseMap)[idxHead].idxFwd)

PHDATA phdEvtDirtyPage;

extern CRITICAL_SECTION PhysCS;

static DWORD PFNReserveStart;   // starting PFN of the reserved memory
static DWORD PFNReserveEnd;     // ending PFN of the reserved memory

static LONG g_nDirtyPages;

SPINLOCK g_physLock;

#define OwnMemoryLock()                  (GetPCB ()->dwCpuId == g_physLock.owner_cpu)

typedef struct _SLIST SLIST, *PSLIST;
struct _SLIST {
    PSLIST pNext;
};
    
void *InterlockedPopList(volatile void *pHead)
{
    PSLIST pListHead = (PSLIST) pHead;
    PSLIST pRet;

    AcquireMemoryLock (21);
    if ((pRet = pListHead->pNext) != 0) {
        pListHead->pNext = pRet->pNext;
    }
    ReleaseMemoryLock (21);
    return pRet;
}

void InterlockedPushList(volatile void *pHead, void *pItem)
{
    PSLIST pListHead = (PSLIST) pHead;
    PSLIST pListItem = (PSLIST) pItem;
    AcquireMemoryLock (21);
    pListItem->pNext = pListHead->pNext;
    pListHead->pNext = pListItem;
    ReleaseMemoryLock (21);
}

//------------------------------------------------------------------------------
//  private functions for physmem handling
//------------------------------------------------------------------------------

// size of stack pre-grow, in dwords
#define PRE_GROW_SIZE       (128/sizeof(DWORD))
__declspec (noinline) void PreGrowStack (void)
{
    volatile DWORD stk[PRE_GROW_SIZE];
    stk[0] = 0;
}

#ifdef DEBUG
static BOOL PageZeroed (LPVOID pMem)
{
    LPDWORD pdw = (LPDWORD) pMem;
    int i;
    for (i = 0; i < 1024; i ++, pdw ++) {
        if (*pdw)
            return FALSE;
    }
    return TRUE;
}
#endif

void InitPageList (PPHYSPAGELIST pUseMap, DWORD idxHead)
{
    pUseMap[idxHead].idxBack = pUseMap[idxHead].idxFwd = (WORD) idxHead;
}

static void ReturnPageToRegion (PREGIONINFO pfi, DWORD idxHead, DWORD idxPage, BOOL fAtEnd)
{
    PPHYSPAGELIST pUseMap = pfi->pUseMap;
    PPHYSPAGELIST pHead   = &pUseMap[idxHead];
    PPHYSPAGELIST pPage   = &pUseMap[idxPage];

    SoftLog (0xbbbb0001, (DWORD) pfi->pUseMap | PcbGetCurCpu ());
    SoftLog (0xbbbb0002, (idxHead << 16) | idxPage);
    DEBUGCHK (!PFNReserveEnd || OwnMemoryLock ());

    if (fAtEnd) {
        // add page to tail of the free list
        WORD   idxBack = pHead->idxBack;
        pPage->idxBack = idxBack;
        pPage->idxFwd  = (WORD) idxHead;
        pHead->idxBack = pUseMap[idxBack].idxFwd = (WORD) idxPage;
    } else {
        // add page to head of the free list
        WORD   idxFwd  = pHead->idxFwd;
        pPage->idxFwd  = idxFwd;
        pPage->idxBack = (WORD) idxHead;
        pHead->idxFwd  = pUseMap[idxFwd].idxBack = (WORD) idxPage;
    }

    // increment # of free pages
    pfi->NumFreePages ++;
}

static void TakePageFromRegion (PREGIONINFO pfi, DWORD idxPage, DWORD dwRefCnt)
{
    PPHYSPAGELIST pUseMap = pfi->pUseMap;
    PPHYSPAGELIST pPage   = &pUseMap[idxPage];

    DEBUGCHK (OwnMemoryLock ());

    SoftLog (0xbbbb0003, (DWORD) pfi->pUseMap | PcbGetCurCpu ());
    SoftLog (0xbbbb0004, (idxPage << 16) | (WORD) dwRefCnt);

    // remove page from the free page list
    pUseMap[pPage->idxFwd].idxBack = pPage->idxBack;
    pUseMap[pPage->idxBack].idxFwd = pPage->idxFwd;

    // mark the page in use
    pPage->idxFwd = INUSE_PAGE;
    pPage->refCnt = (WORD)dwRefCnt;

    // decrement # of free pages
    pfi->NumFreePages --;

}

int VerifyPhysmemList (PREGIONINFO pfi, DWORD idxHead)
{
    PPHYSPAGELIST pUseMap = pfi->pUseMap;
    DWORD dwIdx     = pUseMap[idxHead].idxFwd;
    DWORD dwIdxPrev = idxHead;
    int   nPages    = 0;

    while (dwIdx != idxHead) {
        if (IS_PAGE_INUSE (pUseMap, dwIdx)) {
            DEBUGMSG (1, (L"VerifyPhysmemList: In-use page in free-list idx = %8.8lx (%8.8lx)\r\n", dwIdx, pUseMap[dwIdx]));
            DEBUGCHK (0);
            break;
        }
        if (pUseMap[dwIdx].idxBack != dwIdxPrev) {
            DEBUGMSG (1, (L"VerifyPhysmemList: List corrupted at idx = %8.8lx, (%8.8lx %8.8lx)\r\n", dwIdx, pUseMap[dwIdx], pUseMap[dwIdxPrev]));
            DEBUGCHK (0);
            break;
        }
        dwIdxPrev = dwIdx;
        dwIdx = pUseMap[dwIdx].idxFwd;
        nPages ++;
    }
    DEBUGMSG (ZONE_MEMORY && (dwIdx == idxHead), (L"VerifyPhysList idxHead = %d, nPages = %8.8lx\r\n", idxHead, nPages));
    return (dwIdx == idxHead)? nPages : -1;
}

BOOL VerifyRegion (PREGIONINFO pfi)
{
    int nPagesDirty    = VerifyPhysmemList (pfi, IDX_DIRTY_PAGE_LIST);
    int nPagesZeroed   = VerifyPhysmemList (pfi, IDX_ZEROED_PAGE_LIST);
    int nPagesUnZeroed = VerifyPhysmemList (pfi, IDX_UNZEROED_PAGE_LIST);

    BOOL fRet = (nPagesDirty >= 0)
             && (nPagesUnZeroed >= 0)
             && (nPagesZeroed >= 0)
             && ((DWORD) (nPagesDirty + nPagesUnZeroed + nPagesZeroed) == pfi->NumFreePages);
    return fRet;
}

static void MergePageList (PPHYSPAGELIST pUseMap, DWORD idxDst, DWORD idxSrc)
{
    WORD idxFwd = pUseMap[idxSrc].idxFwd;
    DEBUGCHK (OwnMemoryLock ());

    if (idxFwd != idxSrc) {
        // source list not empty, merge it into the destination
        WORD idxBack   = pUseMap[idxSrc].idxBack;
        WORD idxDstFwd = pUseMap[idxDst].idxFwd;

        DEBUGCHK (idxBack != idxSrc);
        SoftLog (0xbbbb0005, (DWORD) pUseMap | PcbGetCurCpu ());
        
        // we have idxFwd and idxBack of source saved, destroy the source list (make it empty)
        InitPageList (pUseMap, idxSrc);

        // merge the source into the destination
        pUseMap[idxFwd].idxBack     = (WORD)idxDst;
        pUseMap[idxBack].idxFwd     = idxDstFwd;
        pUseMap[idxDstFwd].idxBack  = idxBack;
        pUseMap[idxDst].idxFwd      = idxFwd;
    }
}

static void PHYS_DoClearDirtyPageList (void)
{
    PREGIONINFO pfi;
    DEBUGCHK (OwnCS (&PhysCS));
    
    // discard cache
    NKCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD);
    AcquireMemoryLock (21);
    for (pfi = MemoryInfo.pFi; pfi < (MemoryInfo.pFi+MemoryInfo.cFi); pfi ++) {
        MergePageList (pfi->pUseMap, IDX_UNZEROED_PAGE_LIST, IDX_DIRTY_PAGE_LIST);
        DEBUGCHK (VerifyRegion (pfi));
    }
    ReleaseMemoryLock (21);
    g_nDirtyPages = 0;
}

static void ClearDirtyPageList (void)
{
    // reason for grabbing physCS:
    // in order to move pages from dirty list to unzero'd list, we need to make sure that we none of the pages
    // in the list has any stale data in cache. i.e. we need to flush the cache before we start. However, it's 
    // still possible that, after we flush the cache, new dirty pages got put into the list. And we endup putting
    // dirty pages into unzero's page list.
    //
    // since freeing any phys page requires physCS, we can do this safely if we hold the CS while moving the list.

    EnterCriticalSection (&PhysCS);
    if (g_nDirtyPages) {
        PHYS_DoClearDirtyPageList ();
    }
    
    LeaveCriticalSection (&PhysCS);
}

//------------------------------------------------------------------------------
// PHYS_UpdateFreePageCount - update total # of free pages
//------------------------------------------------------------------------------
static BOOL PHYS_UpdateFreePageCount (int incr) 
{
    long nPages;
    AcquireMemoryLock (32);
    nPages = PageFreeCount + incr;
    if (nPages >= 0) {
        PageFreeCount = nPages;
        PageFreeCount_Pool += incr;

        if (nPages < g_pKData->MinPageFree) {
            g_pKData->MinPageFree = nPages;
        }
        if (PageFreeCount_Pool < (long) KInfoTable[KINX_MINPAGEFREE]) {
            KInfoTable[KINX_MINPAGEFREE] = PageFreeCount_Pool;
        }
        UpdateSQMMarkers (SQM_MARKER_PHYSLOWBYTES, PageFreeCount_Pool);
    }
    LogPhysicalPages(PageFreeCount);    
    ReleaseMemoryLock (32);
    return nPages >= 0;
}

#define PHYSF_NO_INC_FREE_COUNT     0x01
#define PHYSF_LINK_AT_END           0x02
#define PHYSF_PAGING_CACHE          0x04

//------------------------------------------------------------------------------
// PHYS_LinkFreePage - add a physical page to clean or dirty free page list
//------------------------------------------------------------------------------
static void  PHYS_LinkFreePage (PREGIONINFO pfi, DWORD idxHead, DWORD idxPage, DWORD dwLinkFlag) 
{

    AcquireMemoryLock (32);

    DEBUGCHK (IS_PAGE_INUSE (pfi->pUseMap, idxPage));
    ReturnPageToRegion (pfi, idxHead, idxPage, PHYSF_LINK_AT_END & dwLinkFlag);

    if (!(PHYSF_NO_INC_FREE_COUNT & dwLinkFlag)) {
        PageFreeCount ++;
        if (!(PHYSF_PAGING_CACHE & dwLinkFlag)) {
            PageFreeCount_Pool ++;
        }
    }
    LogPhysicalPages(PageFreeCount);

    ReleaseMemoryLock (32);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
PHYS_TakeSpecificPage(
    PREGIONINFO pfi, 
    uint ix
    ) 
{
    BOOL fRet;
    AcquireMemoryLock (40);
    fRet = !IS_PAGE_INUSE (pfi->pUseMap, ix);
    if (fRet) {
        TakePageFromRegion (pfi, ix, 1);
    }
    ReleaseMemoryLock (40);
    return fRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
PHYS_GiveSpecificPage(
    PREGIONINFO pfi, 
    uint ix
    ) 
{
    AcquireMemoryLock (40);
    DEBUGCHK (IS_PAGE_INUSE (pfi->pUseMap, ix));
    ReturnPageToRegion (pfi, IDX_UNZEROED_PAGE_LIST, ix, FALSE);
    ReleaseMemoryLock (40);
}

static PREGIONINFO PHYS_GetAUnzeroedPage (LPDWORD pdwIdx)
{
    PREGIONINFO pfi = NULL;
    
    AcquireMemoryLock  (24);

    if (PageFreeCount) {
        DWORD idxRegion, idxPage;

        for (idxRegion = 0; idxRegion < MemoryInfo.cFi; idxRegion ++) {

            idxPage = FIRST_UNZEROED_PAGE (MemoryInfo.pFi[idxRegion].pUseMap);

            if (IDX_UNZEROED_PAGE_LIST != idxPage) {
                pfi = &MemoryInfo.pFi[idxRegion];

                TakePageFromRegion (pfi, idxPage, 1);
                
                *pdwIdx = idxPage;
                PageFreeCount --;
                PageFreeCount_Pool --;
                LogPhysicalPages(PageFreeCount);
                break;
            }
        }
    }
    
    ReleaseMemoryLock  (24);
    
    return pfi;
}


//
// PHYS_GrabFirstPhysPage - grab a page, fHeld specifies if we already hold the page.
//
// NOTE: It's possible to return a dirty page even if PM_PT_ZEROED is requested.
//       The caller of this function can check *pdwPageType to determine if 
//       zeroing is needed (*pfNeedZero != 0, iff zeroing is needed)
//
DWORD PHYS_GrabFirstPhysPage (DWORD dwRefCnt, LPDWORD pflags, BOOL fHeld)
{
    DWORD dwPfn      = INVALID_PHYSICAL_ADDRESS;
    BOOL  fNeedFlush = FALSE;

    DEBUGCHK (PFNReserveEnd);   // memory must be initialized

    AcquireMemoryLock (32);

    DEBUGCHK ((dwRefCnt <= 1) || (dwRefCnt == FILESYS_PAGE));
    if (PageFreeCount || fHeld) {

        PREGIONINFO pfi, pfiDirty = NULL;
        DWORD idxPage = 0, flags = *pflags;

        // search in reverse order to use dynamic mapped memory first.
        // try to find the 1st region with non-dirty pages to avoid cache flushing
        for (pfi = (MemoryInfo.pFi+MemoryInfo.cFi-1); pfi >= MemoryInfo.pFi; pfi --) {

            if ((flags & PM_PT_STATIC_ONLY) && !pfi->dwStaticMappedVA) {
                continue;
            }

            if ((flags & PM_PT_PHYS_32BIT) && (pfi->paEnd > PHYS_MAX_32BIT_PFN)) {
                continue;
            }

            if (pfi->NumFreePages) {

                if (flags & PM_PT_ZEROED) {
                            
                    // requesting zero'd page, try zero'd page list first
                    idxPage = FIRST_ZEROED_PAGE (pfi->pUseMap);
        
                    if (IDX_ZEROED_PAGE_LIST == idxPage) {
                        // try unzero'd page
                        idxPage = FIRST_UNZEROED_PAGE (pfi->pUseMap);
                        // we get the page from clean list, caller needs to zero the page
                        *pflags |= PM_PT_NEEDZERO;
                    }

                } else {
                    // requesting page that doesn't required to be zero'd, try unzeroed list first
                    idxPage = FIRST_UNZEROED_PAGE (pfi->pUseMap);
        
                    if (IDX_UNZEROED_PAGE_LIST == idxPage) {
                        // no dirty page available, use clean page
                        idxPage = FIRST_ZEROED_PAGE (pfi->pUseMap);
        
                    }
                }

                if (IDX_PAGEBASE <= idxPage) {
                    // found a page we can use
                    break;
                }

                // only dirty pages available in this region, mark this region in case
                // the only pages we can find are dirty.
                if (!pfiDirty) {
                    pfiDirty = pfi;
                }
            }
        }

        if (MemoryInfo.pFi > pfi) {
            // When we get here, either
            // 1) there is only dirty page available, or
            // 2) no page that meet our requirement available (e.g. no static mapped page available)
            pfi = pfiDirty;
            if (pfi) {
                idxPage     = FIRST_DIRTY_PAGE (pfi->pUseMap);
                fNeedFlush  = TRUE;
            }
        }

        if (pfi) {
#ifdef DEBUG
            if (IS_PAGE_INUSE (pfi->pUseMap, idxPage)
                || (idxPage < IDX_PAGEBASE)
                || (idxPage >=  MAX_PAGE_IDX)) {
                NKLOG (pfi);
                NKLOG (idxPage);
                NKLOG (pfi->pUseMap[idxPage]);
                DEBUGCHK (0);
            }
#endif

            TakePageFromRegion (pfi, idxPage, dwRefCnt);

            dwPfn = PFN_FROM_IDX (pfi, idxPage);

            if (!fHeld) {
                PageFreeCount --;
                PageFreeCount_Pool --;
                if (PageFreeCount < g_pKData->MinPageFree) {
                    g_pKData->MinPageFree = PageFreeCount;
                }
                if (PageFreeCount_Pool < (long) KInfoTable[KINX_MINPAGEFREE]) {
                    KInfoTable[KINX_MINPAGEFREE] = PageFreeCount_Pool;
                }

                UpdateSQMMarkers (SQM_MARKER_PHYSLOWBYTES, PageFreeCount_Pool);                
                LogPhysicalPages(PageFreeCount);
            }
        }
    }
    
    ReleaseMemoryLock (32);

    if (INVALID_PHYSICAL_ADDRESS == dwPfn) {
        if (PageFreeCount) {
            NKDbgPrintfW (L"!!! PHYS_GrabFirstPhysPage: Unable to find memory of the specified type (%8.8lx).\r\n", *pflags);
        } else {
            OEMWriteDebugString (L"!!! PHYS_GrabFirstPhysPage: Completely Out Of Memory\r\n");
        }
    } else if (fNeedFlush) {
        // got the page from dirty page list, discard cache before return the page.

        if (InSysCall ()) {
            NKCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD);
        } else {
            ClearDirtyPageList ();
        }
    }
    DEBUGMSG (ZONE_MEMORY, (L"PHYS_GrabFirstPhysicalPage returns: dwPfn = %8.8lx\r\n", dwPfn));

    return dwPfn;
}

//------------------------------------------------------------------------------
//  APIs exposed to other parts of kernel.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// get a page that is already held
//------------------------------------------------------------------------------
DWORD GetHeldPage (LPDWORD pflags)
{
    DWORD dwPfn = PHYS_GrabFirstPhysPage (1, pflags, TRUE);
    DEBUGMSG(ZONE_PHYSMEM,(TEXT("GetHeldPage: Returning %8.8lx (*pflags = %8.8lx)\r\n"), dwPfn, *pflags));
    return dwPfn;
}

//------------------------------------------------------------------------------
// find the corresponding phsycal region of a physcal page
//------------------------------------------------------------------------------
PREGIONINFO GetRegionFromPFN (DWORD dwPfn)
{
    // don't free the reserved area
    if ((dwPfn >= PFNReserveEnd) || (dwPfn < PFNReserveStart)) {
        PREGIONINFO pfi, pfiEnd;
        pfi = MemoryInfo.pFi;
        pfiEnd = pfi + MemoryInfo.cFi;
        for ( ; pfi < pfiEnd ; pfi++) {
            if (dwPfn >= pfi->paStart && dwPfn < pfi->paEnd)
                return pfi;
        }
    }
    return NULL;
}


//------------------------------------------------------------------------------
// verify that an address is actually mapped in static memory
//------------------------------------------------------------------------------
BOOL IsStaticMappedAddress(DWORD dwAddr)
{
    PREGIONINFO pFi, pFiEnd;
    DWORD len;

    pFi = MemoryInfo.pFi;
    pFiEnd = pFi + MemoryInfo.cFi;
    
    // iterate through memory info, checking for the address in mapped regions
    for( ; pFi < pFiEnd; ++pFi) {
        if (!pFi->dwStaticMappedVA) {
            break;
        }
        len = PFN2PA(pFi->paEnd - pFi->paStart);
        if(dwAddr - pFi->dwStaticMappedVA < len)
            return TRUE;
    }
    return FALSE;
}

PEVENT pEvtDirtyPage;

//------------------------------------------------------------------------------
// RemovePage: called by loader.c at system initialization (KernelFindMemory) to reserve
//             pages for object store. Single threaded, no protection needed.
//------------------------------------------------------------------------------
void RemovePage (DWORD dwPfn) 
{
    PREGIONINFO pfi = GetRegionFromPFN (dwPfn);

    DEBUGCHK ((INVALID_PHYSICAL_ADDRESS != dwPfn) && pfi);

    // this funciton is only called during startup. No spinlock protection is needed
    if (pfi) {
        DWORD idxPage = IDX_FROM_PFN (pfi, dwPfn);
        pfi->pUseMap[idxPage].idxFwd = INUSE_PAGE;
        pfi->pUseMap[idxPage].refCnt = FILESYS_PAGE;
    }
}


//------------------------------------------------------------------------------
// InitMemoryPool -- initialize physical memory pool
//
// There are 2 global memory list:
//      g_dirtyPageList keeps the list of dirty pages
//      g_cleanPageList keeps the lsit of clean pages
//
//------------------------------------------------------------------------------
void InitMemoryPool (void) 
{
    DWORD ix, ixEnd;
    PREGIONINFO pfi, pfiEnd;

    /* Fill in data fields in user visible kernel data page */
    KInfoTable[KINX_PROCARRAY] = (long)g_pprcNK;
    KInfoTable[KINX_PAGESIZE]  = VM_PAGE_SIZE;
    KInfoTable[KINX_PFN_SHIFT] = PFN_SHIFT;
    KInfoTable[KINX_PFN_MASK]  = (DWORD)PFNfromEntry(~0);
    KInfoTable[KINX_MEMINFO]   = (long)&MemoryInfo;
    KInfoTable[KINX_KDATA_ADDR] = (DWORD)g_pKData;

    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD);

    for (pfi = MemoryInfo.pFi, pfiEnd = pfi+MemoryInfo.cFi ; pfi < pfiEnd ; ++pfi) {
        DEBUGMSG(ZONE_MEMORY, (TEXT("InitMemoryPool: Init range: map=%8.8lx start=%8.8lx end=%8.8lx\r\n"),
                pfi->pUseMap, pfi->paStart, pfi->paEnd));

        // initialize all the page lists
        InitPageList (pfi->pUseMap, IDX_ZEROED_PAGE_LIST);
        InitPageList (pfi->pUseMap, IDX_UNZEROED_PAGE_LIST);
        InitPageList (pfi->pUseMap, IDX_DIRTY_PAGE_LIST);

        ixEnd = IDX_FROM_PFN (pfi, pfi->paEnd);

        for (ix = IDX_PAGEBASE; ix < ixEnd; ix ++) {
            
            if (!IS_PAGE_INUSE (pfi->pUseMap, ix)) {

                // link the page to dirty list.
                ReturnPageToRegion (pfi, IDX_UNZEROED_PAGE_LIST, ix, FALSE);
                PageFreeCount++;
                PageFreeCount_Pool++;
                LogPhysicalPages(PageFreeCount);

                // increment total number of pages
                KInfoTable[KINX_NUMPAGES]++;
            }
        }
        DEBUGCHK (VerifyRegion (pfi));
    }

    PFNReserveStart = GetPFN ((LPVOID)pTOC->ulRAMFree);
    PFNReserveEnd = GetPFN ((LPVOID) (pTOC->ulRAMFree + MemForPT));
    
    KInfoTable[KINX_MINPAGEFREE] = PageFreeCount;
    g_pKData->MinPageFree = PageFreeCount;

    // initialize in SQM marker structure
    InitSQMMarkers ();
    
    DEBUGMSG(ZONE_MEMORY,(TEXT("InitMemoryPool done, free=%d\r\n"),PageFreeCount));
}

//
// CleanPagesInTheBackground cleanup dirty pages while we're idle.
// The is optional and is here to improve performance. The idea is that when
// there are pages in the dirty page list, and we get into idle, we might as well clean the 
// dirty pages such that future memory allocation is more likely to get a clean page.
//
void CleanPagesInTheBackground (void)
{
    PTHREAD     pCurTh = pCurThread;
    LPVOID      pMem   = VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0);
    PPAGETABLE  pptbl  = GetPageTable (g_ppdirNK, VA2PDIDX (pMem));
    DWORD       idx2nd = VA2PT2ND (pMem);
    PREGIONINFO pfi;
    DWORD       idxPage, idxMinPageIdx = 0xffff;
    DWORD       dwFlags;
    DEBUGMSG (ZONE_SCHEDULE, (L"Start cleaning dirty pages in the background...\r\n"));

    PREFAST_DEBUGCHK (pMem && pptbl);
    // set thread affinity to 1, such that we only pullte the cache of one CPU while
    // performing page zeroing, and we don't need to notify other CPUs of cache cleaning.
    SCHL_SetThreadAffinity (pCurTh, 1);

    // run at idle priority
    THRDSetPrio256 (pCurThread, THREAD_RT_PRIORITY_IDLE);

    for ( ; ; ) {
        DoWaitForObjects (1, &phdEvtDirtyPage, INFINITE);

        ClearDirtyPageList ();

        while (NULL != (pfi = PHYS_GetAUnzeroedPage (&idxPage))) {

            DEBUGMSG (ZONE_PHYSMEM, (L"Cleaning page PFN=%8.8lx\r\n", PFN_FROM_IDX (pfi, idxPage)));

            pptbl->pte[idx2nd] = PFN_FROM_IDX (pfi, idxPage) | PG_KRN_READ_WRITE;
#ifdef ARM
            ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
            ZeroPage (pMem);

            // 1) flush local CPU only for we have affinity set and zeroing only affect 
            //    the cache of the CPU we run on.
            // 2) Need to call flush twice, one for cache, the other for TLB. For VIPT cache requires
            //    valid page table mapping to flush cache.
            OEMCacheRangeFlush (pMem, VM_PAGE_SIZE, CACHE_SYNC_DISCARD|CSF_CURR_CPU_ONLY);
            pptbl->pte[idx2nd] = PG_RESERVED_PAGE;
#ifdef x86
            KCall ((PKFN) OEMCacheRangeFlush, pMem, VM_PAGE_SIZE, CACHE_SYNC_FLUSH_D_TLB|CSF_CURR_CPU_ONLY);
#else
            OEMCacheRangeFlush (pMem, VM_PAGE_SIZE, CACHE_SYNC_FLUSH_D_TLB|CSF_CURR_CPU_ONLY);
#endif

            if (idxMinPageIdx > idxPage) {
                idxMinPageIdx = idxPage;
                dwFlags = PHYSF_LINK_AT_END;
            } else {
                dwFlags = 0;
            }

            // link the list back to the clean page list
            PHYS_LinkFreePage (pfi, IDX_ZEROED_PAGE_LIST, idxPage, dwFlags);
        }

        // always insert at front from now on.
        idxMinPageIdx = 0;
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL DupPhysPage (DWORD dwPFN, PBOOL pfIsMemory)
{
    PREGIONINFO pfi  = GetRegionFromPFN (dwPFN);
    BOOL        fRet = TRUE;

    // caller of DupPhysPage must hold PhysCS
    DEBUGCHK (OwnCS (&PhysCS));
    
    if (pfi) {
        DWORD ix = IDX_FROM_PFN (pfi, dwPFN);

        DEBUGCHK (IS_PAGE_INUSE (pfi->pUseMap, ix));
        DEBUGCHK (pfi->pUseMap[ix].refCnt);

        fRet = (pfi->pUseMap[ix].refCnt < MAX_REFCNT);
        if (fRet) {
            DEBUGMSG(ZONE_PHYSMEM, (TEXT("DupPhysPage: PFN=%8.8lx ix=%x rc=%d\r\n"),
                dwPFN, ix, pfi->pUseMap[ix].refCnt));
            ++pfi->pUseMap[ix].refCnt;
        } else {
            DEBUGCHK (0);
            RETAILMSG (1, (L"DupPhysPage failed - too many alias to the same page, PFN = %8.8lx #alias = 0x%x\r\n",
                dwPFN, pfi->pUseMap[ix].refCnt));
        }
    }
    if (pfIsMemory) {
        *pfIsMemory = (NULL != pfi);
    }

    return fRet;
}

//------------------------------------------------------------------------------
// Helper function used by GetContiguousPages
//------------------------------------------------------------------------------
DWORD
MarkContinguousPages(
    DWORD dwPages,
    DWORD dwAlignment,
    DWORD dwFlags
    )
{
    DWORD paRet = INVALID_PHYSICAL_ADDRESS;
    PREGIONINFO pfi;
    int         ixStart, ixEnd, ixFree, ixLimit; // MUST be signed, for ixEnd can be negative.
    int         idxPfi;
    BOOL        fSetDirtyPageEvent = FALSE;
    BOOL        f2Pass;
    static int  idxLastScan[MAX_MEMORY_SECTIONS];

    DEBUGCHK ((int) dwPages > 0);

    DataMemoryBarrier ();

    //
    // We've at least got enough pages available and that many have been set
    // aside for us (though not yet assigned). 
    //
    PREFAST_DEBUGCHK (MemoryInfo.cFi > 0);
    PREFAST_DEBUGCHK (MemoryInfo.cFi <=  MAX_MEMORY_SECTIONS);

    
    for (idxPfi = MemoryInfo.cFi - 1; idxPfi >= 0; idxPfi --) {
        pfi = MemoryInfo.pFi + idxPfi;

        //
        // For each memory section, scan the free pages.
        //

        // the region dosn't have enough pages we asked for, skip it.
        if (pfi->NumFreePages < dwPages) {
            continue;
        }

        ixFree = idxLastScan[idxPfi];
        ixLimit = IDX_FROM_PFN (pfi, pfi->paEnd);

        DEBUGCHK (ixLimit > (int) dwPages);
        DEBUGCHK (ixFree < ixLimit);

        if (!ixFree) {
            ixFree = IDX_PAGEBASE;
        }
        f2Pass = (IDX_PAGEBASE != ixFree);

        if (IS_PAGE_INUSE (pfi->pUseMap, ixFree)) {
            // the page is currently in use
            ixEnd = ixFree;
            do {
                ixFree ++;
                if (ixFree == ixLimit) {
                    ixFree = IDX_PAGEBASE;
                    f2Pass = FALSE;
                }
            } while ((ixEnd != ixFree) && IS_PAGE_INUSE (pfi->pUseMap, ixFree));
            if (ixFree == ixEnd) {
                // someone took all the free pages while we're scanning
                continue;
            }
        } else {
            // the page is not currently in use, scan backward to find the 1st free page
            while ((ixFree > IDX_PAGEBASE) && !IS_PAGE_INUSE (pfi->pUseMap, ixFree-1)) {
                ixFree --;
            }
        }

        DEBUGCHK (ixFree < ixLimit);

        // note: we don't use lock to protect the value, the information we keep in idxLastScan doesn't have to be accurate,
        //       as it's just a hint to where to start scanning.
        idxLastScan[idxPfi] = ixFree;
        
        ixStart = ixFree;
        ixEnd   = ixLimit - dwPages;
        
        //
        // Find a string of free pages
        //
        for ( ; ; ) {

            if (ixStart > ixEnd) {
                if (f2Pass) {
                    // restart from beginning of the memory region
                    f2Pass  = FALSE;
                    ixStart = IDX_PAGEBASE;
                    ixEnd   = ixFree - dwPages;
                    continue;
                }
                // failed
                break;
            }
            
            if (IS_PAGE_INUSE (pfi->pUseMap, ixStart) || (dwAlignment & PFN2PA(PFN_FROM_IDX (pfi, ixStart)))) {
                //
                // Page in use or doesn't match alignment request, skip ahead
                //
                ixStart++;
            } else {
                int ixWalk, ixTarget, ixTemp;

                //
                // we're going to make it a 2-pass process here. For
                // 1) Take/GiveSpecific page requires us to acquire spinlock
                // 2) On a failed attemp, we need to flush cache. 
                // so we will not even try to take the pages unless it's even remotely possible (!IS_PAGE_INUSE(idx)).
                //
                ixTarget = ixStart + dwPages;
                
                for (ixWalk = ixStart; ixWalk < ixTarget; ixWalk++) {
                    if (IS_PAGE_INUSE (pfi->pUseMap, ixWalk)) {
                        break;
                    }
                }
                if (ixWalk == ixTarget) {
                    // 1st pass successful, try to take the pages
                    
                    for (ixWalk = ixStart; ixWalk < ixTarget; ixWalk++) {
                        if (!PHYS_TakeSpecificPage (pfi, ixWalk)) {
                            // failed to take the page, has to restart
                            break;
                        }
                    }
                    if (ixWalk == ixTarget) {
                        // successfully takes all the pages update return value.
                        paRet = PFN_FROM_IDX (pfi, ixStart);
                        idxLastScan[idxPfi] = (ixTarget < ixLimit)? ixTarget : IDX_PAGEBASE;
                        break;
                    }

                    // pages were taken away between our 1st and 2nd pass. give the pages back
                    // and restart. 
                    ClearDirtyPageList ();
                    fSetDirtyPageEvent = TRUE;
                    for (ixTemp = ixStart; ixTemp < ixWalk; ixTemp++) {
                        PHYS_GiveSpecificPage (pfi, ixTemp);
                    }
                }

                // failed to find pages, restart from ixWalk+1
                ixStart = ixWalk + 1;

            }
        }
        if (INVALID_PHYSICAL_ADDRESS != paRet) {
            // found the pages we need
            break;
        }
    }

    if (fSetDirtyPageEvent) {
        FastSignalEvent (pEvtDirtyPage);
    }
    
    return paRet;
}

//------------------------------------------------------------------------------
//
// Try to grab physically contiguous pages. This routine needs to be preemptive
// as much as possible, so we may take some pages and end up giving them back if
// a full string of pages can't be found. Pages are potentially coming and 
// going, but we'll only make one pass through memory.
//
// NOTE: GetContiguous page does NOT return zero'd page. Caller is responsible of
//       zeroing the pages if desire.
//
//------------------------------------------------------------------------------
DWORD
GetContiguousPages(
    DWORD dwPages,
    DWORD dwAlignment, 
    DWORD dwFlags
    ) 
{
    DWORD paRet = INVALID_PHYSICAL_ADDRESS;
    BOOL fHeld = HoldMemoryWithWait (MEMPRIO_KERNEL, dwPages);
    
    if (fHeld) {

        paRet = MarkContinguousPages (dwPages, dwAlignment, dwFlags);
        if (INVALID_PHYSICAL_ADDRESS == paRet) {
            // Try pageout everything and retry to mark continguous pages
            // File flush requires calling out to filesys which requires active proc to be NK
            PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
            NKForcePageout ();
            SwitchActiveProcess (pprc);
            paRet = MarkContinguousPages (dwPages, dwAlignment, dwFlags);
        }
        
        if (INVALID_PHYSICAL_ADDRESS != paRet) {
            // the pages we got can come from clean and dirty list, we need to discard caches 
            // to make sure that we don't introduce cache aliasing.
            ClearDirtyPageList ();
        
        } else {
            //
            // We couldn't lock down a contiguous section
            //
            ReleaseHeldMemory (dwPages);
            DEBUGMSG (ZONE_ERROR, (L"ERROR: GetContiguousPages failed, PageFreeCount = 0x%8.8lx\r\n", PageFreeCount));
        }
    }

    DEBUGMSG(ZONE_PHYSMEM,(TEXT("GetContiguousPages: Returning %8.8lx\r\n"), paRet));
    return paRet;
}

//------------------------------------------------------------------------------
//  APIs exposed outside of kernel.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  calls this function to register an Out Of Memory event, which
//  HoldPages sets when free physical memory drops below the given threshold.
//------------------------------------------------------------------------------
VOID
NKSetOOMEventEx(
    HANDLE hEvtLowMemoryState, 
    HANDLE hEvtGoodMemoryState, 
    DWORD cpLowMemEvent, 
    DWORD cpLow, 
    DWORD cpCritical, 
    DWORD cpLowBlockSize, 
    DWORD cpCriticalBlockSize
    )
{
    NKDbgPrintfW (L"!!!!NKSetOOMEventEx no longer supported. Use RegisterOOMNotification instead!!!\r\n");
    DebugBreak ();
}

BOOL IsFileSysPage (DWORD pa256)
{
    DWORD dwPfn = PFNfrom256 (pa256);
    PREGIONINFO pfi = GetRegionFromPFN (dwPfn);
    if (pfi) {
        DWORD idxPage = IDX_FROM_PFN (pfi, dwPfn);
        DEBUGCHK (IS_PAGE_INUSE (pfi->pUseMap, idxPage) && (FILESYS_PAGE == pfi->pUseMap[idxPage].refCnt));
        return IS_PAGE_INUSE (pfi->pUseMap, idxPage) && (FILESYS_PAGE == pfi->pUseMap[idxPage].refCnt);
    }
    DEBUGCHK (0);
    return FALSE;
}

//------------------------------------------------------------------------------
//  transfer object store memory to program memory
//------------------------------------------------------------------------------
BOOL NKGiveKPhys (const DWORD *pPages, int length)
{
    BOOL fRet = FALSE;
    DWORD pa256;
    int loop;

    DEBUGMSG(ZONE_ENTRY,(L"NKGiveKPhys entry: %8.8lx %8.8lx\r\n",pPages,length));

    if (!length) {
        // releasing 1 statically mapped page
        pa256 = PA256FromPfn (GetPFN (pPages));
        fRet = IsFileSysPage (pa256);
        if (fRet) {
            pPages = &pa256;
            length = 1;
        }
    } else {

        for (loop = 0; loop < length; loop ++) {
            if (!IsFileSysPage (pPages[loop])) {
                break;
            }
        }
        fRet = (loop == length);
    }

    if (fRet) {
        DWORD dwPfn;
        REGIONINFO *pfi;
        DWORD idxPage;
        
        // clean the dirty page list
        ClearDirtyPageList ();

        for (loop = 0; loop < length; loop++) {
            dwPfn = PFNfrom256 (pPages[loop]);
            pfi = GetRegionFromPFN (dwPfn);
            PREFAST_DEBUGCHK (pfi);
            idxPage = IDX_FROM_PFN (pfi, dwPfn);
            DEBUGCHK (IS_PAGE_INUSE (pfi->pUseMap, idxPage));
            DEBUGCHK (FILESYS_PAGE == pfi->pUseMap[idxPage].refCnt);
            PHYS_LinkFreePage (pfi, IDX_UNZEROED_PAGE_LIST, idxPage, 0);
        }

        // increment total pages
        InterlockedExchangeAdd ((PLONG) &KInfoTable[KINX_NUMPAGES], length);

        // got more memory, update memory state
        EnterCriticalSection (&PhysCS);
        UpdateMemoryState ();
        LeaveCriticalSection (&PhysCS);

        // signal dirty pages available
        FastSignalEvent (pEvtDirtyPage);
    }

    DEBUGMSG(ZONE_ENTRY,(L"NKGiveKPhys exit: %8.8lx\r\n", fRet));
    return fRet;
}



//------------------------------------------------------------------------------
//  transfer program memory to object store
// NOTE: NKGetKPhys NO LONGER GUARANTEE PAGE ZERO'D
//------------------------------------------------------------------------------
BOOL NKGetKPhys (DWORD *pPages, int length)
{
    BOOL   fRet;
    DWORD  dwPfn, pa256;
    DWORD  flags = 0;

    DEBUGMSG(ZONE_ENTRY,(L"NKGetKPhys entry: %8.8lx %8.8lx\r\n", pPages, length));

    // free all cached stacks
    VMFreeExcessStacks (0);

    if (!pPages) {
        flags  = PM_PT_STATIC_ONLY;
        pPages = &pa256;
        length = 1;
    }

    fRet = (length > 0) && (HLDPG_SUCCESS == HoldMemory (MEMPRIO_HEALTHY, length));

    if (fRet) {

        // decrement total pages
        InterlockedExchangeAdd ((PLONG) &KInfoTable[KINX_NUMPAGES], -length);

        while (length --) {
            // should never fail unless requesting static mapped page.
            dwPfn = PHYS_GrabFirstPhysPage (FILESYS_PAGE, &flags, TRUE);
            DEBUGCHK ((INVALID_PHYSICAL_ADDRESS != dwPfn) || (PM_PT_STATIC_ONLY == flags));

            if (INVALID_PHYSICAL_ADDRESS == dwPfn) {
                DEBUGCHK (PM_PT_STATIC_ONLY == flags);
                DEBUGCHK (1 == length);
                // increment total pages, and return the page we held
                InterlockedExchangeAdd ((PLONG) &KInfoTable[KINX_NUMPAGES], length);

                PHYS_UpdateFreePageCount (length);
                fRet = FALSE;
                break;
            }
            pPages[length] = PA256FromPfn (dwPfn);
            if (PM_PT_STATIC_ONLY == flags) {
                fRet = (BOOL) Pfn2Virt (dwPfn);
                DEBUGCHK (fRet);
            }
        }
        // clean the dirty page list
        ClearDirtyPageList ();

    }

    DEBUGMSG(ZONE_ENTRY,(L"NKGetKPhys exit: %8.8lx\r\n", fRet));

    return fRet;
}

// signal memory events if needed
static void SignalLowMemoryEventsIfNeeded (LONG cPages)
{
    LONG pfcPool = PageFreeCount_Pool;
    
    DEBUGCHK (OwnCS (&PhysCS));
    if (pfcPool - cPages < g_cpMemPressure) {
        // memory under pressure

        // reset the "good memory" event
        FastResetEvent (pEvtGoodMem);

        // set the memory under pressure event
        FastSignalEvent ((pfcPool - cPages < g_cpMemLow)? pEvtMemLow : pEvtMemPressure);

        // reset "foreground memory okay" event if needed
        if (pfcPool - cPages < g_cpMemoryThreshold[MEMPRIO_FOREGROUND]) {
            FastResetEvent (pEvtFgMemOk);
        }
    }
}

//
// Hold memory - request memory of a certain amount, give a memory type.
//
HLDPGEnum HoldMemory (DWORD dwMemPrio, LONG cPages)
{
    HLDPGEnum eRet = HLDPG_FAIL;
    DEBUGMSG (ZONE_MEMORY, (L"HoldMemory enter: %d %d\r\n", dwMemPrio, cPages));

    // Assume kernel's memory priority during PSL notification.
    // NOTE: the (dwMemPrio > MEMPRIO_KERNEL) check is required for the function can be
    //       called during bootup and tlsSecure is not setup yet.
    if (   (dwMemPrio > MEMPRIO_KERNEL)
        && (pCurThread->tlsSecure[TLSSLOT_KERNEL] & TLSKERN_IN_PSLNOTIFY)) {
        dwMemPrio = MEMPRIO_KERNEL;
    }

    EnterCriticalSection (&PhysCS);

    if (PageAvailable (dwMemPrio, cPages)) {

        LONG cMinDirectRequired = (g_cpMemoryThreshold[dwMemPrio] < g_cpMinDirectAvailable)
                                ? g_cpMemoryThreshold[dwMemPrio]
                                : g_cpMinDirectAvailable;

        // if the number of direct page available is not enough, retry
        if (PageFreeCount - cPages < cMinDirectRequired) {
            eRet= HLDPG_RETRY;
            
        } else {

            VERIFY (PHYS_UpdateFreePageCount (-cPages));
            eRet = HLDPG_SUCCESS;

            // signal/reset memory events if needed
            SignalLowMemoryEventsIfNeeded (0);

            // signal/update page trimmer if needed
            if ((PageFreeCount < g_cpTrimmerStart) && (PageFreeCount_Pool > PageFreeCount)) {

                // singal the page out thread
                FastSignalEvent (pEvtWriteBackPageOut);
            }
        }
    }
    
    LeaveCriticalSection (&PhysCS);

    DEBUGMSG (ZONE_MEMORY, (L"HoldMemory leave: %d\r\n", eRet));
    return eRet;
}

BOOL WaitForTrimmer (PHDATA phdEvtWait)
{
    PTHREAD pth = pCurThread;
    DWORD dwWaitResult;

    // singal the page out thread
    FastSignalEvent (pEvtPageOut);

    // raise the priority of a thread to be at least as high as current thread
    SCHL_RaiseThreadPrio (pPageOutThread, GET_CPRIO (pth));

    // wait for trimmer to finish its work
    SET_USERBLOCK (pth);
    dwWaitResult = DoWaitForObjects (1, &phdEvtWait, INFINITE);
    CLEAR_USERBLOCK (pth);
    return WAIT_OBJECT_0 == dwWaitResult;

}

FORCEINLINE void SignalPagingEvent (void)
{
    FastSignalEvent ((PageFreeCount >= g_cpMinDirectAvailable + g_CntPagesNeeded)
                     ? pEvtPagePoolTrimmed
                     : pEvtPageOut);
}

static void DecrementPagesNeeded (LONG cPages)
{
    cPages = InterlockedExchangeAdd ((LONG*)&g_CntPagesNeeded, -cPages);

    if (pEvtPageOut) {      // The rest requires memory manager to be initialzed
        SignalPagingEvent ();

        // adjust page out thread priority if no-one is waiting
        if (!cPages
            && pPageOutThread
            && (GET_BPRIO(pPageOutThread) != g_PageOutThreadDefaultPrio)) {
            // no-one is waiting for trimmer anymore, reset the trimmer priority
            AcquireSchedulerLock (0);
            // need to test again, in case other thread preempted us and blocked on memory request again
            if (!g_CntPagesNeeded) {
                SCHL_SetThreadBasePrio (pPageOutThread, g_PageOutThreadDefaultPrio);
            }
            ReleaseSchedulerLock (0);
        }
    }

}

FORCEINLINE void SignalBlockersIfNeeded (void)
{
    DEBUGCHK (OwnCS (&PhysCS));
    
    if (PageFreeCount >= g_cpMinDirectAvailable + g_CntPagesNeeded) {
        FastSignalEvent (pEvtPagePoolTrimmed);
    }
}

#define TIMEOUT_LAST_TRY    1000

//
// TryHoldMemory - helper function to hold cPages of memory
//          - make sure we have enough memory
//          - wait for trimmer to page out pages if direct available memory is too low.
// returns:
//      HLDPG_SUCCESS - page is held successfully
//      HLDPG_FAIL    - failed to get the memory
//      HLDPG_RETRY   - the thread is terminated while waiting
//
static HLDPGEnum TryHoldMemory (DWORD dwMemoryPrio, LONG cPages)
{
    HLDPGEnum enHeld = HoldMemory (dwMemoryPrio, cPages);

    if (HLDPG_RETRY == enHeld) {

        // Take into account all the pending allocations. Fail if it cannot be satisfied.
        if (!PageAvailable (dwMemoryPrio, g_CntPagesNeeded)) {
            enHeld = HLDPG_FAIL;

        } else {

            // cAllocBlockedForPageOut should be small. If we see this number grows beyond control, 
            // we need to adjust the trigger threshold.
            InterlockedIncrement (&g_MemStat.cAllocBlockedForPageOut);

            while (WaitForTrimmer (phdEvtPagePoolTrimmed)) {
                // update memory priority as process can change its foreground state while blocked
                enHeld = HoldMemory (dwMemoryPrio, cPages);
                if (HLDPG_RETRY != enHeld) {
                    // relay the event to the next thread waiting on for memory.
                    FastSignalEvent (pEvtPagePoolTrimmed);
                    break;
                }
            }
        }
    }

    return enHeld;
}

//
// HoldMemoryWithWait - main function to request memory from memory manager.
// Algorithm:
//      (1) try to grab the memory requested, if the direct available memory is too low, wait for trimmer to page out some pages. 
//      (2) if we fail, we'll signal low-memory events and  wait for "good memory event" with a time out.
//           Currently timeout is set to 1 second, might want to consider using registry.
//      (3) after the wait (either timeout or the good memory event signaled), we'll try again. fail if we can't get it.
//
// NOTE:
//      (a) OOM handler is optional. In case there is no OOM handler, memory allocation will be throttled and fail if there
//          is no memory released during the period we waited.
//      (b) If implemented OOM handler, it should behave without user interaction. Asking user to "kill a process" is
//          not a good way to deal with it. i.e. we should always do Auto-OOM in case we want to implement a OOM handler.
//
BOOL HoldMemoryWithWait (DWORD dwMemoryPrio, LONG cPages)
{
    HLDPGEnum enHeld;
    PTHREAD   pCurTh = pCurThread;

    DEBUGCHK (dwMemoryPrio > MEMPRIO_KERNEL_INTERNAL);

    DEBUGMSG (ZONE_MEMORY, (L"HoldMemoryWithWait enter: %d %d\r\n", dwMemoryPrio, cPages));

    //
    // check for obvious error. Don't get into OOM handling if it's going to fail for sure.
    //
    if (cPages > g_cpMaxAlloc) {
        RETAILMSG (1, (L"!ERROR! Unusually large memory allocation request for %d pages, Uninitialized variables??\r\n", cPages));
        ASSERT_RETAIL (0);
        return FALSE;
    }

    // update # of pages we might need to page out
    InterlockedExchangeAdd (&g_CntPagesNeeded, cPages);

    // try to get the memory we need
    enHeld = TryHoldMemory (dwMemoryPrio, cPages);

    // if we can't get the memory, signal memory events and hope OOM handler will release some memory for us.
    if (HLDPG_FAIL == enHeld) {
        DWORD dwWaitResult;

        // cant' satisfy memory allocation, try to terminate apps
        RETAILMSG (1, (L"HoldMemoryWithWait: memory low, signaling memory events, cPages = 0x%x\r\n", cPages));

        EnterCriticalSection (&PhysCS);
        SignalLowMemoryEventsIfNeeded (g_CntPagesNeeded);
        g_MemStat.cAllocWaitedForOOM ++;
        LeaveCriticalSection (&PhysCS);

        // wait and see if memory got released by low memory handler. 
        // Time out value is TIMEOUT_LAST_TRY, might want to consider using registry settings
        SET_USERBLOCK (pCurTh);
        dwWaitResult = DoWaitForObjects (1, &phdEvtGoodMem, TIMEOUT_LAST_TRY);
        CLEAR_USERBLOCK (pCurTh);

        if (WAIT_FAILED != dwWaitResult) {
            // try to get the memory we need again. Do this even if we timed-out waiting for "good memory" event, 
            // for there could still be memory released.
            enHeld = TryHoldMemory (dwMemoryPrio, cPages);
        }
    }

    // reduce the # of pages needed count
    DecrementPagesNeeded (cPages);

    InterlockedIncrement (&g_MemStat.cAllocs);

    RETAILMSG (HLDPG_FAIL == enHeld, (L"!! Memory request for %d pages failed, memory priority = %d!!\r\n", cPages, dwMemoryPrio));

    DEBUGMSG (ZONE_MEMORY, (L"HoldMemoryWithWait exit: %d\r\n", enHeld));

    return (enHeld == HLDPG_SUCCESS);
}

static BOOL WaitForOnePage (PHDATA phdEvtWait)
{
    BOOL fRet;
    
    // cAllocBlockedForPageOut should be small. If we see this number grows beyond control, 
    // we need to adjust the trigger threshold.
    InterlockedIncrement (&g_MemStat.cAllocBlockedForPageOut);

    InterlockedIncrement (&g_CntPagesNeeded);
    fRet = WaitForTrimmer (phdEvtWait);

    DecrementPagesNeeded (1);
    
    return fRet;
}

BOOL KernelObjectMemoryAvailable (DWORD dwMemPrio)
{
    BOOL fRet = (dwMemPrio <= MEMPRIO_SYSTEM);

    if (!fRet) {
        fRet = PageAvailable (dwMemPrio-1, 1);

        if (fRet && (PageFreeCount < g_cpMinDirectAvailable)) {

            WaitForOnePage (phdEvtPagePoolTrimmed);

        }
    }
    return fRet;
}

BOOL WaitForPagingMemory (PPROCESS pprc, DWORD dwPgRslt)
{
    // wait for different memory events based on memory priority
    return WaitForOnePage (((PAGEIN_RETRY_PAGEPOOL == dwPgRslt) || (CalcMemPrioForPaging (pprc) < MEMPRIO_FOREGROUND))
                    ? phdEvtPagePoolTrimmed
                    : phdEvtFgMemOk);
}


void UpdateMemoryState (void)
{
    LONG pfcPool = PageFreeCount_Pool;
    DEBUGCHK (OwnCS (&PhysCS));
    
    // update memory status
    if (pfcPool - g_CntPagesNeeded >=  g_cpMemoryThreshold[MEMPRIO_FOREGROUND]) {

        FastSignalEvent (pEvtFgMemOk);
        
        if (pfcPool - g_CntPagesNeeded >=  g_cpMemoryThreshold[MEMPRIO_HEALTHY]) {
            // note: order is important here. reset 1st, set later.
            FastResetEvent (pEvtMemLow);
            FastResetEvent (pEvtMemPressure);
            FastSignalEvent (pEvtGoodMem);
        }
    }
    // if someone is blocked on trimmer, signal events if needed
    SignalBlockersIfNeeded ();
}


//------------------------------------------------------------------------------
// FreePhysPage - decrement the ref count of a physical page, free the page if
// ref count == 0.  Returns PREGIONINFO if the page is in RAM, NULL otherwise.
//------------------------------------------------------------------------------
PCREGIONINFO FreePhysPage (DWORD dwPFN)
{
    PREGIONINFO pfi = GetRegionFromPFN(dwPFN);

    if (pfi) {
        DWORD ix = IDX_FROM_PFN (pfi, dwPFN);

        DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePhysPage: PFN=%8.8lx ix=%x rc=%d\r\n"), dwPFN,
                ix, pfi->pUseMap[ix].refCnt));
        
        EnterCriticalSection(&PhysCS);
        
        DEBUGCHK (IS_PAGE_INUSE (pfi->pUseMap, ix));
        DEBUGCHK (pfi->pUseMap[ix].refCnt);

        if (MAX_REFCNT < pfi->pUseMap[ix].refCnt) {
            // object store page
            pfi = NULL;
            
        } else if (!--pfi->pUseMap[ix].refCnt) {

            PHYS_LinkFreePage (pfi, IDX_DIRTY_PAGE_LIST, ix, 0);
            g_nDirtyPages ++;
            
            FastSignalEvent (pEvtDirtyPage);

            DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePhysPage: PFN=%8.8lx\r\n"), dwPFN));

            UpdateMemoryState ();
        }

        LeaveCriticalSection(&PhysCS);
    }
    return pfi;
}

void ReleaseHeldMemory (LONG cPages)
{
    DEBUGCHK (cPages > 0);
    EnterCriticalSection (&PhysCS);
    PHYS_UpdateFreePageCount (cPages);
    UpdateMemoryState ();
    LeaveCriticalSection (&PhysCS);
}

DWORD GetPagingCache (LPDWORD pflags, DWORD dwMemPrio)
{
    DWORD dwPFN = INVALID_PHYSICAL_ADDRESS;
    BOOL  fGotPage = FALSE;
    LONG  cpThreshold;

    // Assume kernel's memory priority during PSL notification.
    if (pCurThread->tlsSecure[TLSSLOT_KERNEL] & TLSKERN_IN_PSLNOTIFY) {
        dwMemPrio = MEMPRIO_KERNEL;
    }

    cpThreshold = (g_cpMemoryThreshold[dwMemPrio] > g_cpMinDirectAvailable)
                ? g_cpMinDirectAvailable
                : g_cpMemoryThreshold[dwMemPrio];

    EnterCriticalSection (&PhysCS);
    AcquireMemoryLock (32);
    if (PageFreeCount > cpThreshold) {
        PageFreeCount --;
        LogPhysicalPages (PageFreeCount);    
        fGotPage = TRUE;
    }
    ReleaseMemoryLock (32);

    if (fGotPage) {
        dwPFN = PHYS_GrabFirstPhysPage (1, pflags, TRUE);
    } else {
        // always signal trimmer thread - need to pageout something to make room for this request
        FastSignalEvent (pEvtPageOut);
        SCHL_RaiseThreadPrio (pPageOutThread, GET_CPRIO (pCurThread));
    }
    LeaveCriticalSection (&PhysCS);
    
    return dwPFN;
}

//------------------------------------------------------------------------------
// FreePagingCache - Return a page that we used for paging cache
// NOTE: if pdwEntry is NOT NULL, it's coming from paging out loader page, where
//       we only decommit if ref-count is 1.
//------------------------------------------------------------------------------
BOOL FreePagingCache (DWORD paPFN, LPDWORD pdwEntry)
{
    PREGIONINFO pfi = GetRegionFromPFN (paPFN);
    BOOL        fPageReleased = FALSE;
    DWORD       ix;

    PREFAST_DEBUGCHK (pfi);

    ix  = IDX_FROM_PFN (pfi, paPFN);
    
    DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePagingCache: PFN=%8.8lx ix=%x rc=%d\r\n"), paPFN,
            ix, pfi->pUseMap[ix].refCnt));
    
    EnterCriticalSection(&PhysCS);
    
    DEBUGCHK (IS_PAGE_INUSE (pfi->pUseMap, ix));
    DEBUGCHK (pfi->pUseMap[ix].refCnt);
    DEBUGCHK (MAX_REFCNT >= pfi->pUseMap[ix].refCnt);

    if (!pdwEntry || (1 == pfi->pUseMap[ix].refCnt)) {

        if (pdwEntry) {
            // reset entry before freeing the page back to physical memory pool
            *pdwEntry = MakeReservedEntry (VM_PAGER_LOADER);
        }

        if (!--pfi->pUseMap[ix].refCnt) {

            PHYS_LinkFreePage (pfi, IDX_DIRTY_PAGE_LIST, ix, PHYSF_PAGING_CACHE);
            g_nDirtyPages ++;
            
            DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePagingCache: PFN=%8.8lx\r\n"), paPFN));

            // if someone is blocked on trimmer, signal events if needed
            SignalBlockersIfNeeded ();

            FastSignalEvent (pEvtDirtyPage);

            fPageReleased = TRUE;
        }
    }

    LeaveCriticalSection(&PhysCS);

    return fPageReleased;
}

//------------------------------------------------------------------------------
// Return a page from allocated status to held status
// NOTE: if pdwEntry is NOT NULL, it's coming from paging out loader page, where
//       we only decommit if ref-count is 1.
//------------------------------------------------------------------------------
BOOL ReholdPagingPage (DWORD paPFN, LPDWORD pdwEntry)
{
    PREGIONINFO pfi = GetRegionFromPFN (paPFN);
    BOOL        fPageReleased = FALSE;
    uint        ix;

    PREFAST_DEBUGCHK (pfi);

    ix = IDX_FROM_PFN (pfi, paPFN);
    
    EnterCriticalSection(&PhysCS);

    PREFAST_DEBUGCHK (PageFreeCount);
    DEBUGCHK (IS_PAGE_INUSE(pfi->pUseMap, ix));
    DEBUGCHK (MAX_REFCNT >= pfi->pUseMap[ix].refCnt);

    DEBUGMSG(ZONE_PHYSMEM, (TEXT("ReholdPage: PFN=%8.8lx ix=%x rc=%d\r\n"), 
            paPFN, ix, pfi->pUseMap[ix].refCnt));

    if (!pdwEntry || (1 == pfi->pUseMap[ix].refCnt)) {
    
        if (pdwEntry) {
            // reset entry before freeing the page back to physical memory pool
            *pdwEntry = MakeReservedEntry (VM_PAGER_LOADER);
        }

        if (! --pfi->pUseMap[ix].refCnt) {

            PHYS_LinkFreePage (pfi, IDX_DIRTY_PAGE_LIST, ix, PHYSF_NO_INC_FREE_COUNT);
            g_nDirtyPages ++;

            DEBUGMSG(ZONE_PHYSMEM, (TEXT("ReholdPage: PFN=%8.8lx\r\n"), paPFN));

            FastSignalEvent (pEvtDirtyPage);
            
        } else {
        
            // Decrement PageFreeCount if it's not 0. The reason is that paging pool is no longer the
            // owner of this page (i.e. it lost a page), and we need to hold an extra page to account
            // for it. 
            DEBUGCHK (!pdwEntry);
            VERIFY (PHYS_UpdateFreePageCount (-1));
        }
        fPageReleased = TRUE;
    }
    LeaveCriticalSection(&PhysCS);
    
    return fPageReleased;
}

void HandlePSLNotification (void)
{
    for ( ; ; ) {
        DoWaitForObjects (1, &phdEvtMemLow, INFINITE);

        // notify PSLs that memory is low
        NKPSLNotify (DLL_MEMORY_LOW, 0, 0);

        // log memory low event
        CELOG_LowMemSignalled (PageFreeCount, 1, g_cpMemLow, g_cpMemoryThreshold[MEMPRIO_FOREGROUND], 1, 1);
        UpdateSQMMarkers (SQM_MARKER_PHYSOOMCOUNT, +1);

        DoWaitForObjects (1, &phdEvtGoodMem, INFINITE);
    }
}

void InitMemoryManager (void)
{
    LONG  cpDiff, cpThreshold;
    
    // good memory event - manual reset, initial set
    phdEvtGoodMem     = NKCreateAndLockEvent (TRUE, TRUE);
    pEvtGoodMem       = GetEventPtr (phdEvtGoodMem);

    // memory above foreground threshold event - manual reset, initial set
    phdEvtFgMemOk     = NKCreateAndLockEvent (TRUE, TRUE);
    pEvtFgMemOk       = GetEventPtr (phdEvtFgMemOk);

    // memory low event - manual reset, inital unset
    phdEvtMemLow      = NKCreateAndLockEvent (TRUE, FALSE);
    pEvtMemLow        = GetEventPtr (phdEvtMemLow);
    
    // memory pressure event - manual reset, inital unset
    phdEvtMemPressure = NKCreateAndLockEvent (TRUE, FALSE);
    pEvtMemPressure   = GetEventPtr (phdEvtMemPressure);

    // dirty page event - auto-reset, intial set
    phdEvtDirtyPage   = NKCreateAndLockEvent (FALSE, TRUE);
    pEvtDirtyPage     = GetEventPtr (phdEvtDirtyPage);

    // page pool trimmed - auto reset, initial set
    phdEvtPagePoolTrimmed = NKCreateAndLockEvent (FALSE, TRUE);
    pEvtPagePoolTrimmed = GetEventPtr (phdEvtPagePoolTrimmed);

    // page out event - auto reset, initial unset
    phdEvtWriteBackPageOut  = NKCreateAndLockEvent (FALSE, FALSE);
    pEvtWriteBackPageOut    = GetEventPtr (phdEvtWriteBackPageOut);
    phdEvtPageOut           = NKCreateAndLockEvent (FALSE, FALSE);
    pEvtPageOut             = GetEventPtr (phdEvtPageOut);

    PREFAST_DEBUGCHK (pEvtDirtyPage && pEvtGoodMem && pEvtMemLow && pEvtMemPressure && pEvtPagePoolTrimmed && pEvtWriteBackPageOut);

    // adjust memory threhold if memory available on boot is too low (default healthy should be <= 25% of total available RAM)
    cpThreshold = PageFreeCount / 4;
    if (cpThreshold < g_cpMemoryThreshold[MEMPRIO_HEALTHY]) {
        DWORD idx;
        g_cpMemoryThreshold[MEMPRIO_HEALTHY]      = cpThreshold;
        g_cpMemoryThreshold[MEMPRIO_APP_LOW]      = cpThreshold / 2;
        g_cpMemoryThreshold[MEMPRIO_APP_CRITICAL] = cpThreshold / 4;
        g_cpMemoryThreshold[MEMPRIO_KERNEL_LOW]   = cpThreshold / 8;

        // in case memory available is so low where the threshold value is lower than
        // kernel critical threshold, set it to be the same.
        for (idx = MEMPRIO_KERNEL_LOW; idx <= MEMPRIO_HEALTHY; idx ++) {
            if (g_cpMemoryThreshold[idx] >= g_cpMemoryThreshold[MEMPRIO_KERNEL_CRITICAL]) {
                break;
            }
            g_cpMemoryThreshold[idx] = g_cpMemoryThreshold[MEMPRIO_KERNEL_CRITICAL];
        }
    }
    

    cpDiff = (g_cpMemoryThreshold[MEMPRIO_HEALTHY] - g_cpMemoryThreshold[MEMPRIO_REGULAR_APPS]) >> 3;

    // memory pressure trigger - 12.5% of (healthy-"app threshold") under healthy
    g_cpMemPressure   = g_cpMemoryThreshold[MEMPRIO_HEALTHY] - cpDiff;          // 12.5% below healthy

    // memory low trigger - 12.5% above regular app threshold
    g_cpMemLow        = g_cpMemoryThreshold[MEMPRIO_REGULAR_APPS] + cpDiff;     // 12.5% above app threshold

    // memory events can be duplciated to external for singaling purpose. Make sure they are wait-only from external.    
    pEvtGoodMem->manualreset |= EXTERNAL_WAIT_ONLY;          // wait-only from external
    pEvtMemLow->manualreset |= EXTERNAL_WAIT_ONLY;           // wait-only from external
    pEvtMemPressure->manualreset |= EXTERNAL_WAIT_ONLY;      // wait-only from external

    PagePoolInit ();
}

DWORD QueryReg (LPCWSTR pszValName, LPCWSTR pszReserved, DWORD dwType, LPVOID pBuffer, DWORD cbBufSize);

void ReadMemoryMangerRegistry (void)
{
    DWORD cpSystem     = g_cpMemoryThreshold[MEMPRIO_SYSTEM];
    DWORD cpForeground = g_cpMemoryThreshold[MEMPRIO_FOREGROUND];
    DWORD cpAppLow     = g_cpMemoryThreshold[MEMPRIO_REGULAR_APPS];
    DWORD cpHealthy    = g_cpMemoryThreshold[MEMPRIO_HEALTHY];
    
    QueryReg (L"cpSystem",     L"SYSTEM\\OOM", REG_DWORD, &cpSystem,     sizeof (DWORD));
    QueryReg (L"cpForeground", L"SYSTEM\\OOM", REG_DWORD, &cpForeground, sizeof (DWORD));
    QueryReg (L"cpAppLow",     L"SYSTEM\\OOM", REG_DWORD, &cpAppLow,     sizeof (DWORD));
    QueryReg (L"cpHealthy",    L"SYSTEM\\OOM", REG_DWORD, &cpHealthy,    sizeof (DWORD));

    // the order must be strickly enforced.
    // g_cpMemoryThreshold[MEMPRIO_KERNEL_CRITICAL] < cpSystem < cpForeground < cpAppLow < cpHealthy
    // 
    if (cpHealthy > (DWORD) PageFreeCount_Pool) {
        cpHealthy = (DWORD) PageFreeCount_Pool;
    }
    
    if (((DWORD) g_cpMemoryThreshold[MEMPRIO_KERNEL_CRITICAL] < cpSystem)
        && (cpSystem < cpForeground)
        && (cpForeground < cpAppLow)
        && (cpAppLow < cpHealthy)) {

        DWORD cpDiff = (cpHealthy - cpAppLow) >> 3;

        g_cpMemoryThreshold[MEMPRIO_SYSTEM]     = cpSystem;
        g_cpMemoryThreshold[MEMPRIO_FOREGROUND] = cpForeground;
        g_cpMemoryThreshold[MEMPRIO_REGULAR_APPS] = cpAppLow;
        g_cpMemoryThreshold[MEMPRIO_HEALTHY]    = cpHealthy;

        g_cpMemPressure   = cpHealthy - cpDiff;         // 12.5% below healthy
        g_cpMemLow        = cpAppLow + cpDiff;          // 12.5% above app threshold

        // Theoretically page pool triggers doesn't have to be tied with memory thresholds.
        // However, if they're not in sync, we can get into threshing a lot. Therefore we'll derive page pool triggers from
        // the memory thresholds.
        g_cpMinDirectAvailable  = cpSystem;
        g_cpTrimmerStart        = cpForeground;
        g_cpTrimmerStop         = cpAppLow;

        if (PageFreeCount > (LONG) cpHealthy) {
            g_cpMaxAlloc = PageFreeCount - cpHealthy;
        }

    } else {
        // invalid
        NKD (L"ERROR: Invalid memory registry setting (cHealthy = %d, cpForeground = %d, cpSystem = %d), use default settings\r\n");
        DebugBreak ();
    }
    RETAILMSG (1, (L"Memory Manager settings:\r\n"));
    RETAILMSG (1, (L"     cpSystem     = 0x%x\r\n", g_cpMemoryThreshold[MEMPRIO_SYSTEM]));
    RETAILMSG (1, (L"     cpForeground = 0x%x\r\n", g_cpMemoryThreshold[MEMPRIO_FOREGROUND]));
    RETAILMSG (1, (L"     cpAppLow     = 0x%x\r\n", g_cpMemoryThreshold[MEMPRIO_REGULAR_APPS]));
    RETAILMSG (1, (L"     cpMemLow     = 0x%x\r\n", g_cpMemLow));
    RETAILMSG (1, (L"     cpPressure   = 0x%x\r\n", g_cpMemPressure));
    RETAILMSG (1, (L"     cpHealthy    = 0x%x\r\n", g_cpMemoryThreshold[MEMPRIO_HEALTHY]));
    RETAILMSG (1, (L"     cpMaxAlloc   = 0x%x\r\n", g_cpMaxAlloc));

    
    RETAILMSG (1, (L"Page Trimmer settings:\r\n"));
    RETAILMSG (1, (L"     cpTrimmerStart     = 0x%x\r\n", g_cpTrimmerStart));
    RETAILMSG (1, (L"     cpTrimmerStop      = 0x%x\r\n", g_cpTrimmerStop));

}


BOOL NKRegisterOOMNotification (PHANDLE phMemLow, PHANDLE phMemPressure, PHANDLE phGoodMem)
{
    PPROCESS pprc         = pActvProc;
    DWORD    dwErr        = 0;
    HANDLE   hMemLow      = HNDLDupWithHDATA (pprc, phdEvtMemLow, &dwErr);
    HANDLE   hMemPressure = dwErr? NULL : HNDLDupWithHDATA (pprc, phdEvtMemPressure, &dwErr);
    HANDLE   hGoodMem     = dwErr? NULL : HNDLDupWithHDATA (pprc, phdEvtGoodMem, &dwErr);

    __try {
        if (!dwErr) {
            dwErr = ERROR_INVALID_PARAMETER;
            *phMemLow      = hMemLow;
            *phMemPressure = hMemPressure;
            *phGoodMem     = hGoodMem;
            dwErr = 0;
        }
        
    } __finally {
        if (dwErr) {
            HNDLCloseHandle (pprc, hMemLow);
            HNDLCloseHandle (pprc, hMemPressure);
            HNDLCloseHandle (pprc, hGoodMem);
            NKSetLastError (dwErr);
        }
    }

    return !dwErr;
}

//
// kernel page allocator design:
//  - each region is 4M-aligned and 4M in size.
//  - 1st page of the region is the header (PAGEALLOCATORLIST), where it contains the single-linked-list of free pages,
//    and the link to next region with free pages.
//  - Only a single global pointer is used (g_pPageFreeList) to keep track of regions with free pages.
//  - O(1) allocation and free.
//

typedef struct _PAGELISTHEADER      PAGELISTHEADER, *PPAGELISTHEADER;
typedef struct _PAGEALLOCATORLIST   PAGEALLOCATORLIST, *PPAGEALLOCATORLIST;

struct _PAGELISTHEADER {
    PPAGEALLOCATORLIST  pNextFreeRegion;
    LPVOID              pNextFreePage;
};

#define NUM_PAGES_PER_REGION    ((VM_PAGE_SIZE-sizeof(PAGELISTHEADER))/sizeof(LPBYTE))  // 1022 pages per region

#define GRABBED_PAGE            ((LPBYTE) 0xFFFF4321)                                   // serve as signature for debug purpose
#define FULL_REGION             ((PPAGEALLOCATORLIST) 0xFFFF8765)                       // serve as signature for debug purpose

typedef struct _PAGEALLOCATORLIST {
    PAGELISTHEADER  hdr;
    LPVOID          pPages[NUM_PAGES_PER_REGION];
} PAGEALLOCATORLIST, *PPAGEALLOCATORLIST;

PPAGEALLOCATORLIST g_pPageFreeList;
volatile LONG g_cRegionUsedUp;

ERRFALSE(sizeof (PAGEALLOCATORLIST) == VM_PAGE_SIZE);

#define IDX_PAGE_START          1
#define REGION_SIZE             SIZE_4M                 // each region is 4M in size

FORCEINLINE PPAGEALLOCATORLIST GetPageAllocator (LPVOID pPage)
{
    return (PPAGEALLOCATORLIST) ALIGNDOWN_4M ((DWORD) pPage);
}

FORCEINLINE DWORD PAGE_TO_INDEX (PPAGEALLOCATORLIST pAllocator, LPVOID pPage)
{
    DEBUGCHK (((DWORD) pPage - (DWORD) pAllocator) < REGION_SIZE);
    return (((DWORD) pPage - (DWORD) pAllocator) >> VM_PAGE_SHIFT) - IDX_PAGE_START;
}

PPAGEALLOCATORLIST CreateNewPageRegion (void)
{
    PPAGEALLOCATORLIST pAllocator = (PPAGEALLOCATORLIST) VMReserve (g_pprcNK, SIZE_4M, MEM_4MB_PAGES, 0);

    if (pAllocator) {
        if (VMCommit (g_pprcNK, pAllocator, VM_PAGE_SIZE, PAGE_READWRITE, 0)) {
            int idx;
            LPBYTE pNextPage = (LPBYTE) pAllocator + VM_PAGE_SIZE;

            // initialize pAllocator header
            pAllocator->hdr.pNextFreePage = pNextPage;
            pAllocator->hdr.pNextFreeRegion = NULL;

            // initialize the page list
            for (idx = 0; idx < NUM_PAGES_PER_REGION-1; idx ++) {
                pNextPage += VM_PAGE_SIZE;
                pAllocator->pPages[idx] = pNextPage;
            }

            pAllocator->pPages[idx] = NULL;     // last page
            
            EnterCriticalSection (&PhysCS);
            if (!g_pPageFreeList) {
                DEBUGMSG (ZONE_MEMORY, (L"New Page Region created 0x%8.8lx\r\n", g_pPageFreeList));
                g_pPageFreeList = pAllocator;
                pAllocator      = NULL;         // set pAllocator to NULL so we don't free it.
            } else {
                // someone created a new region before us, do nothing
                DEBUGMSG (ZONE_MEMORY, (L"New Page Region already created by other thread\r\n"));
            }
            LeaveCriticalSection (&PhysCS);
        }

        if (pAllocator) {
            VERIFY (VMFreeAndRelease (g_pprcNK, pAllocator, SIZE_4M));
        }

    }
    
    return g_pPageFreeList;
}

LPVOID GrabOnePage (DWORD dwPageType)
{
    LPVOID pPage = NULL;

    if (!(PM_PT_STATIC_ONLY & dwPageType)
        && !InSysCall () 
        && (HLDPG_SUCCESS == HoldMemory (MEMPRIO_KERNEL_INTERNAL, 1))) {
        PPAGEALLOCATORLIST pAllocator;
        
        do {
            if (!g_pPageFreeList) {
                // nothing available, create a new region
                LONG cRegionUsedUp = g_cRegionUsedUp;
                if (!CreateNewPageRegion ()) {
                    if (cRegionUsedUp == g_cRegionUsedUp) {
                        // failed to create a new region (OOM).
                        break;
                    } else {
                        // it's theoretically possible that the new region created
                        // got exhausted by other threads, grabbing all the pages.
                        // we'll need to retry in case a region had been used up in between
                        continue;
                    }
                }
            }

            EnterCriticalSection (&PhysCS);
            pAllocator = g_pPageFreeList;
            if (pAllocator) {

                DWORD idxPage;

                // take the 1st free page of the region
                pPage   = pAllocator->hdr.pNextFreePage;
                idxPage = PAGE_TO_INDEX (pAllocator, pPage);
                
                DEBUGCHK (IsPageAligned (pPage));

                // remove the page from the region
                pAllocator->hdr.pNextFreePage = pAllocator->pPages[idxPage];
                pAllocator->pPages[idxPage]   = GRABBED_PAGE;

                // if the region has no free pages, remove it from the free list
                if (!pAllocator->hdr.pNextFreePage) {
                    // grabbed the last page of the region, remove it from the free list
                    g_pPageFreeList = pAllocator->hdr.pNextFreeRegion;
                    pAllocator->hdr.pNextFreeRegion = FULL_REGION;
                    InterlockedIncrement (&g_cRegionUsedUp);
                }
            }
            LeaveCriticalSection (&PhysCS);
            
        } while (!pPage);

        if (pPage) {
            DWORD dwPfn = PHYS_GrabFirstPhysPage (1, &dwPageType, TRUE);
            DEBUGCHK (INVALID_PHYSICAL_ADDRESS != dwPfn);
            VERIFY (VMCreateKernelPageMapping (pPage, dwPfn));
        } else {
            ReleaseHeldMemory (1);
        }
    }

    if (!pPage) {
        // we either out of VM to do dynamic mapping or we're in non-preemitible state
        // try to grab one with static mapping.
        
        if (!PFNReserveEnd) {
            // memory pool not initialized yet, grab it directly from pTOC->ulRAMFree
            
            // system startup, physical memory pool is not initialized
            pPage = (LPVOID) (pTOC->ulRAMFree + MemForPT);
            MemForPT += VM_PAGE_SIZE;
            if (PM_PT_ZEROED & dwPageType) {
                dwPageType |= PM_PT_NEEDZERO;
            }
                
        } else {

            DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;

            dwPageType |= PM_PT_STATIC_ONLY;
            
            if (InSysCall ()) {
                dwPfn = PHYS_GrabFirstPhysPage (1, &dwPageType, FALSE);
                
            } else if (HLDPG_SUCCESS == HoldMemory (MEMPRIO_KERNEL_INTERNAL, 1)) {
                dwPfn = PHYS_GrabFirstPhysPage (1, &dwPageType, TRUE);
            }    

            if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
                pPage = Pfn2Virt (dwPfn);
                DEBUGCHK (pPage);
            }
        }
    }

    if (pPage && (PM_PT_NEEDZERO & dwPageType)) {
        ZeroPage (pPage);
    }

#ifdef DEBUG
    // interlocked/debug message is not safe before physical memory is set up
    if (PFNReserveEnd) {
        SoftLog (0xeeee0001, (DWORD)pPage | dwPageType);
        DEBUGMSG(ZONE_MEMORY,(TEXT("GrabOnePage: Returning %8.8lx\r\n"), pPage));
    }
#endif
    
    return pPage;
}

void FreeKernelPage (LPVOID pPage)
{
    if (IsStaticMappedAddress ((DWORD) pPage)) {
        VERIFY (FreePhysPage (GetPFN (pPage)));
        
        DEBUGMSG(ZONE_MEMORY,(TEXT("FreeKernelPage: freeing statically mapped page %8.8lx\r\n"), pPage));
    } else {
        PPAGEALLOCATORLIST pAllocator = GetPageAllocator (pPage);
        DWORD idxPage = PAGE_TO_INDEX (pAllocator, pPage);
        LPVOID pOldFree;
        DWORD dwPFN = VMRemoveKernelPageMapping (pPage);
        DEBUGCHK (pAllocator
              && (GRABBED_PAGE == pAllocator->pPages[idxPage])
              && (INVALID_PHYSICAL_ADDRESS != dwPFN));

        InvalidatePages (g_pprcNK, (DWORD) pPage, 1);
        VERIFY (FreePhysPage (dwPFN));

        EnterCriticalSection (&PhysCS);

        // add the page to the free list
        pOldFree = pAllocator->hdr.pNextFreePage;
        pAllocator->pPages[idxPage]   = pOldFree;
        pAllocator->hdr.pNextFreePage = pPage;

        if (!pOldFree) {
            // was out of pages in the region, now we have a free page, add it to the free list
            pAllocator->hdr.pNextFreeRegion = g_pPageFreeList;
            g_pPageFreeList = pAllocator;
        }

        LeaveCriticalSection (&PhysCS);
        DEBUGMSG(ZONE_MEMORY,(TEXT("FreeKernelPage: freeing dynamically mapped page %8.8lx\r\n"), pPage));
    }
}




