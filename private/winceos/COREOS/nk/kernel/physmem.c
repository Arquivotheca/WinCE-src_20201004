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

// minimum low and critical levels
#define MIN_LOW_CP      0x100 // 1M
#define MIN_CRITICAL_CP 0x40  // 256k

//
// Page pool settings (see physmem.h for details)
// default values here doesn't really matter as these
// will be overwritten once shell/gwes calls into us.
//
long PageOutTrigger;
long PageOutLevel;
long g_cpPageOutLow;
long g_cpPageOutHigh;
static PHDATA g_phdOOMEvent;
long g_cpNotifyEventThreshold = MIN_LOW_CP;
long g_cpLowThreshold         = MIN_LOW_CP;
long g_cpCriticalThreshold    = MIN_CRITICAL_CP;
long g_cpLowBlockSize         = 4;
long g_cpCriticalBlockSize    = 2;
LONG PagePoolTrimState = PGPOOL_TRIM_NOTINIT;
extern PHDATA phdPageOutEvent;
extern PTHREAD pPageOutThread;
extern BYTE g_PageOutThreadDefaultPrio;

#define FIRST_ZEROED_PAGE(pUseMap)              (((pUseMap)[IDX_ZEROED_PAGE_LIST]).idxFwd)
#define FIRST_UNZEROED_PAGE(pUseMap)            (((pUseMap)[IDX_UNZEROED_PAGE_LIST]).idxFwd)
#define FIRST_DIRTY_PAGE(pUseMap)               (((pUseMap)[IDX_DIRTY_PAGE_LIST]).idxFwd)

#define IS_PAGE_LIST_EMPTY(pUseMap, idxHead)    ((idxHead) == (pUseMap)[idxHead].idxFwd)

// the maximum (lowest) priority to keep a thread's stack from being scavanged
// any thread of this priority or higher will never have its stack scvanaged.
// stack scavanging will never occur if set to 255
DWORD dwNKMaxPrioNoScav = 247;  // default to lowest RT priority

PHDATA phdEvtDirtyPage;

extern CRITICAL_SECTION PhysCS, PagerCS;

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
BOOL PHYS_UpdateFreePageCount (int incr) 
{
    long nPages;
    AcquireMemoryLock (32);
    nPages = PageFreeCount + incr;
    if (nPages >= 0) {
        PageFreeCount = nPages;
        InterlockedExchangeAdd (&PageFreeCount_Pool, incr);

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
        InterlockedIncrement (&PageFreeCount_Pool);        
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
                InterlockedDecrement (&PageFreeCount_Pool);
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
                InterlockedDecrement (&PageFreeCount_Pool);
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


#if defined (MIPS) || defined (SH4)
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

        // Checking is to be done only for statically mapped region
        if (!pFi->dwStaticMappedVA) {
            break;              // Regions beyond this are also going to be dynamically mapped
        }
        
        len = PFN2PA(pFi->paEnd - pFi->paStart);
        if(dwAddr - pFi->dwStaticMappedVA < len) {
            return TRUE;
        }
    }
    return FALSE;
}
#endif

//------------------------------------------------------------------------------
// Return a page from allocated status to held status
//------------------------------------------------------------------------------
BOOL ReholdPage (DWORD paPFN)
{
    PREGIONINFO pfi      = GetRegionFromPFN(paPFN);
    BOOL        fSuccess = TRUE;
    uint        ix;

    PREFAST_DEBUGCHK (pfi);

    ix = IDX_FROM_PFN (pfi, paPFN);
    
    EnterCriticalSection(&PhysCS);

    DEBUGCHK (IS_PAGE_INUSE(pfi->pUseMap, ix));
    DEBUGCHK (MAX_REFCNT >= pfi->pUseMap[ix].refCnt);

    DEBUGMSG(ZONE_PHYSMEM, (TEXT("ReholdPage: PFN=%8.8lx ix=%x rc=%d\r\n"), 
            paPFN, ix, pfi->pUseMap[ix].refCnt));
    
    if (! --pfi->pUseMap[ix].refCnt) {

        PHYS_LinkFreePage (pfi, IDX_DIRTY_PAGE_LIST, ix, PHYSF_NO_INC_FREE_COUNT);
        g_nDirtyPages ++;

        DEBUGMSG(ZONE_PHYSMEM, (TEXT("ReholdPage: PFN=%8.8lx\r\n"), paPFN));

        EVNTModify (GetEventPtr (phdEvtDirtyPage), EVENT_SET);
        
    } else {
    
        // Decrement PageFreeCount if it's not 0. The reason is that paging pool is no longer the
        // owner of this page (i.e. it lost a page), and we need to hold an extra page to account
        // for it. Though it's desireable to go through "HoldPages" logic, we can't do it here. 
        // For we're most likely in low-memory/paging-out situation and calling HoldPages here can
        // lead to deadlock/livelock.
        fSuccess = PHYS_UpdateFreePageCount (-1);
    }

    LeaveCriticalSection(&PhysCS);

    return fSuccess;
}

//------------------------------------------------------------------------------
//
// GrabOnePage gets the next available physical page
//
//------------------------------------------------------------------------------
LPVOID GrabOnePage (DWORD dwPageType) 
{
    DWORD  flags = dwPageType|PM_PT_STATIC_ONLY;
    LPVOID pMem  = NULL;

    if (!PFNReserveEnd) {
        // memory pool not initialized yet, grab it directly from pTOC->ulRAMFree
        
        // system startup, physical memory pool is not initialized
        pMem = (LPVOID) (pTOC->ulRAMFree + MemForPT);
        MemForPT += VM_PAGE_SIZE;
        if (PM_PT_ZEROED & flags) {
            flags |= PM_PT_NEEDZERO;
        }
            
    } else {

        DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;
        if (InSysCall ()) {
            dwPfn = PHYS_GrabFirstPhysPage (1, &flags, FALSE);
            
        } else if (HLDPG_SUCCESS == HoldPages (1)) {
            dwPfn = PHYS_GrabFirstPhysPage (1, &flags, TRUE);
        }    

        if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
            pMem = Pfn2Virt (dwPfn);
            DEBUGCHK (pMem);
        }
    }

    if (pMem && (PM_PT_NEEDZERO & flags)) {
        ZeroPage (pMem);
    }

    SoftLog (0xeeee0001, (DWORD)pMem | flags);
    DEBUGMSG(ZONE_MEMORY,(TEXT("GrabOnePage: Returning %8.8lx\r\n"), pMem));
    return pMem;
}

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
                InterlockedIncrement (&PageFreeCount_Pool);
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

    DEBUGCHK (pMem && pptbl);
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
// Check to see if current page limits allow cpNeed pages to be granted
//------------------------------------------------------------------------------
static BOOL CheckForFreePages (int cpNeed, LONG cpFreePages) 
{
    BOOL bRet;
    
    if (cpNeed <= g_cpCriticalBlockSize) {
        // requested size is <= critical block size.
        // do we have enough room including stack threshold?
        bRet = ((cpNeed + STACK_RESERVE) <= cpFreePages);

    } else if (cpNeed <= g_cpLowBlockSize) {
        // requested size is <= low block size.
        // do we have enough room including critical threshold?
        bRet = ((cpNeed + g_cpCriticalThreshold) <= cpFreePages);

    } else {
        // requested size is > low block size.
        // do we have enough room including low threshold?
        bRet = ((cpNeed + g_cpLowThreshold) <= cpFreePages);
    }

    return bRet;
}

//------------------------------------------------------------------------------
// Same as HoldPages but returns only if:
// a) success, pages held OR
// b) failure after retrying HoldPages several times
//
//------------------------------------------------------------------------------
#define MAX_HOLD_PAGES_TIMEOUT_MS 30000 //(30 seconds)
BOOL HoldPagesWithWait (int cpNeed)
{
    HLDPGEnum enHeld;
    PTHREAD pth = pCurThread;
    int iSleep = 5; // start with 5 msec sleep on failure    

    // page out needs PagerCS
    DEBUGCHK (!OwnCS (&PagerCS));

    // update # of pages we might need to page out
    InterlockedExchangeAdd ((LONG*)&g_CntPagesNeeded, cpNeed);
    
    //
    // committing pages, try to hold the pages before acquiring VM lock, 
    // such that we can page out pages if needed.
    // 
    for ( ; ; ) {
        enHeld = HoldPages (cpNeed);            

        // if we fail to grab the pages and there is potential to get those
        // pages, sleep a little while to let the page out thread run.
        if (HLDPG_RETRY != enHeld) { 
            break;
        }

        // try again as there are pages in pool which can be released
        if (iSleep < MAX_HOLD_PAGES_TIMEOUT_MS)
            iSleep *= 2;

        // do not use Sleep/NKSleep as we won't be able to terminate the thread        
        UB_Sleep (iSleep);

        // check for terminated thread in a non-psl context
        if ((pActvProc == pth->pprcOwner) && GET_DYING(pth) && !GET_DEAD(pth)) {
            break;
        }
        
    }

    // reduce the # of pages needed count
    InterlockedExchangeAdd ((LONG*)&g_CntPagesNeeded, -cpNeed);

    return (enHeld == HLDPG_SUCCESS);
}

//------------------------------------------------------------------------------
// This function is responsible for:
// a) granting cpNeed pages for the caller if successful
// b) set the OOM event if the physical memory is below a certain threshold
// c) signal the page trimmer thread if pageouts from page pool are needed
//------------------------------------------------------------------------------
DWORD HoldPages (int cpNeed)
{
    BOOL bRet = FALSE;
    BOOL fSetOomEvent = FALSE;
    HLDPGEnum enRet = HLDPG_FAIL;

    EnterCriticalSection (&PhysCS);

    // See if we can grant cpNeed pages from the current free page count
    bRet = CheckForFreePages (cpNeed, PageFreeCount);
    if (!bRet) {
        // free all cached stacks
        // NOTE: We must release physical memory CS before acquiring VMcs, or we can deadlock.
        LeaveCriticalSection (&PhysCS);
        VMFreeExcessStacks (0);
        EnterCriticalSection (&PhysCS);
        bRet = CheckForFreePages (cpNeed, PageFreeCount);
    }    

    // if pages available, update page free count
    if (bRet) {
        
        LONG PageFreeCount_Pool_Prev = PageFreeCount_Pool;
        bRet = PHYS_UpdateFreePageCount (-cpNeed);

        if (bRet) {

            // OOM state is updated if the page free count transitions 
            // from normal to the "notify" level or from the notify level to low/critical or from low to critical
            fSetOomEvent = (((PageFreeCount_Pool_Prev >= g_cpNotifyEventThreshold) && (PageFreeCount_Pool < g_cpNotifyEventThreshold))
                || ((PageFreeCount_Pool_Prev >= g_cpLowThreshold) && (PageFreeCount_Pool < g_cpLowThreshold))
                || ((PageFreeCount_Pool_Prev >= g_cpCriticalThreshold) && (PageFreeCount_Pool < g_cpCriticalThreshold)));

            // PagePool trim thread priority is updated if trim thread
            // is not currently running and the current priority of the
            // thread is higher than its default priority
            if (IsPagePoolTrimIdle (PagePoolTrimState)
                && (GET_CPRIO(pPageOutThread) < g_PageOutThreadDefaultPrio)) {
                SCHL_SetThreadBasePrio (pPageOutThread, g_PageOutThreadDefaultPrio);
            }

            enRet = HLDPG_SUCCESS;
        }
    }

    // Adjust the page pool trim thread priority and state
    if (((PageFreeCount < PageOutTrigger) 
            && (PageFreeCount < PageFreeCount_Pool))
        || (!bRet && CheckForFreePages (cpNeed, PageFreeCount_Pool))) {

        DWORD dwCurrThdPrio = GET_CPRIO(pCurThread);       

        // Adjust page trim thread priorities to run at-least
        // with current thread priority if:
        // a) failed current hold pages call OR
        // b) free page count is below low threshold
        if ((!bRet || PageFreeCount < g_cpLowThreshold) 
            && (dwCurrThdPrio < GET_CPRIO(pPageOutThread))) {
            SCHL_SetThreadBasePrio (pPageOutThread, dwCurrThdPrio);
        }

        // signal the page trim thread to start page out if trim
        // thread is not already running or signaled
        if (InterlockedTestExchange (&PagePoolTrimState, PGPOOL_TRIM_IDLE, PGPOOL_TRIM_SIGNALED) == PGPOOL_TRIM_IDLE) {
            EVNTModify (GetEventPtr (phdPageOutEvent), EVENT_SET);
        }

        if (!bRet) {
            enRet = HLDPG_RETRY;
        }
    }
    
    LeaveCriticalSection(&PhysCS);

    // signal OOM handler if we're in low memory condition
    if (fSetOomEvent) {
        CELOG_LowMemSignalled(PageFreeCount, cpNeed, g_cpLowThreshold, g_cpCriticalThreshold, g_cpLowBlockSize, g_cpCriticalBlockSize);
        if(g_phdOOMEvent) {
            EVNTModify (GetEventPtr (g_phdOOMEvent), EVENT_SET);
        }
        UpdateSQMMarkers (SQM_MARKER_PHYSOOMCOUNT, +1);
    }
    
    return enRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL DupPhysPage (DWORD dwPFN)
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
    return fRet;
}


//------------------------------------------------------------------------------
// FreePhysPage - decrement the ref count of a physical page, free the page if
// ref count == 0.  Returns TRUE if the page was freed.
//------------------------------------------------------------------------------
BOOL 
FreePhysPage(
    DWORD paPFN
    ) 
{
    BOOL        fFreed  = FALSE;
    PREGIONINFO pfi     = GetRegionFromPFN(paPFN);

    if (pfi) {
        DWORD ix = IDX_FROM_PFN (pfi, paPFN);

        DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePhysPage: PFN=%8.8lx ix=%x rc=%d\r\n"), paPFN,
                ix, pfi->pUseMap[ix].refCnt));
        
        EnterCriticalSection(&PhysCS);
        
        DEBUGCHK (IS_PAGE_INUSE (pfi->pUseMap, ix));
        DEBUGCHK (pfi->pUseMap[ix].refCnt);

        if ((MAX_REFCNT >= pfi->pUseMap[ix].refCnt) && !--pfi->pUseMap[ix].refCnt) {

            PHYS_LinkFreePage (pfi, IDX_DIRTY_PAGE_LIST, ix, 0);
            g_nDirtyPages ++;
            
            DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePhysPage: PFN=%8.8lx\r\n"), paPFN));

            // if we have too many dirty pages, clean them up.
            if (g_nDirtyPages + 2 * STACK_RESERVE >= PageFreeCount) {
                PHYS_DoClearDirtyPageList ();
            }

            fFreed = TRUE;
            
        }

        LeaveCriticalSection(&PhysCS);

        if (fFreed) {
            EVNTModify (GetEventPtr (phdEvtDirtyPage), EVENT_SET);
        }
    }
    return fFreed;
}

//------------------------------------------------------------------------------
// FreePhysPage - free a kernel page  Returns TRUE if the page was freed.
//------------------------------------------------------------------------------
BOOL FreeKernelPage (LPVOID pPage)
{
    // discard the page
    return FreePhysPage (GetPFN (pPage));
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
        EVNTModify (GetEventPtr (phdEvtDirtyPage), EVENT_SET);
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
    BOOL fHeld = HoldPagesWithWait ((int)dwPages);
    
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
            PHYS_UpdateFreePageCount (dwPages);
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
    DEBUGMSG(ZONE_ENTRY,(L"NKSetOOMEventEx entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        hEvtLowMemoryState,hEvtGoodMemoryState,cpLowMemEvent,cpLow,cpCritical,cpLowBlockSize,cpCriticalBlockSize));

    if (hEvtLowMemoryState) {
        if (g_phdOOMEvent) {
            UnlockHandleData (g_phdOOMEvent);
        }
        g_phdOOMEvent = LockHandleData (hEvtLowMemoryState, pActvProc);
    }

    if (cpLow < MIN_LOW_CP)
        cpLow = MIN_LOW_CP;

    if (cpCritical < MIN_CRITICAL_CP)
        cpCritical = MIN_CRITICAL_CP;

    if (cpLowMemEvent < MIN_LOW_CP)
        cpLowMemEvent = MIN_LOW_CP;
    
    g_cpNotifyEventThreshold = cpLowMemEvent;
    g_cpLowThreshold = cpLow;
    g_cpCriticalThreshold = cpCritical;
    g_cpLowBlockSize = cpLowBlockSize;
    g_cpCriticalBlockSize = cpCriticalBlockSize;
    PageOutLevel = g_cpLowThreshold + g_cpPageOutHigh;
    PageOutTrigger = g_cpLowThreshold + g_cpPageOutLow;

    if (g_cpMinPageProcCreate > (LONG)cpLow)
        g_cpMinPageProcCreate = cpLow;
    
    DEBUGMSG(ZONE_ENTRY,(L"NKSetOOMEventEx exit\r\n"));
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
            DEBUGCHK (pfi);
            idxPage = IDX_FROM_PFN (pfi, dwPfn);
            DEBUGCHK (IS_PAGE_INUSE (pfi->pUseMap, idxPage));
            DEBUGCHK (FILESYS_PAGE == pfi->pUseMap[idxPage].refCnt);
            PHYS_LinkFreePage (pfi, IDX_UNZEROED_PAGE_LIST, idxPage, 0);
        }

        // increment total pages
        InterlockedExchangeAdd ((PLONG) &KInfoTable[KINX_NUMPAGES], length);
        
        // signal dirty pages available
        EVNTModify (GetEventPtr (phdEvtDirtyPage), EVENT_SET);
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

    fRet = (length > 0) && (HLDPG_SUCCESS == HoldPages (length));

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

