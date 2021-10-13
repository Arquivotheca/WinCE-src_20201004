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

#define FILESYS_PAGE        0xffff
#define TRANSITION_PAGE     0xfffe
#define MAX_REFCNT          0xfffd

#define PAGEOUT_LOW         (( 68*1024)/VM_PAGE_SIZE)  // do pageout once below this mark, reset when above high
#define PAGEOUT_HIGH        ((132*1024)/VM_PAGE_SIZE)
#define PAGE_OUT_TRIGGER    (( 24*1024)/VM_PAGE_SIZE)  // page out if below highwater and this much paged in recently

HANDLE g_hOOMEvent;
long g_cpLowThreshold       = STACK_RESERVE;
long g_cpCriticalThreshold  = STACK_RESERVE;
long g_cpLowBlockSize       = VM_BLOCK_SIZE;
long g_cpCriticalBlockSize  = VM_PAGE_SIZE;


DLIST g_cleanPageList;
DLIST g_dirtyPageList;


// the maximum (lowest) priority to keep a thread's stack from being scavanged
// any thread of this priority or higher will never have its stack scvanaged.
// stack scavanging will never occur if set to 255
DWORD dwNKMaxPrioNoScav = 247;  // default to lowest RT priority

extern HANDLE hEvtDirtyPage;
extern PEVENT pEvtDirtyPage;

extern CRITICAL_SECTION PhysCS;
extern PTHREAD pCleanupThread;

// PageOutNeeded is set to 1 when the number of free pages drops below
// PageOutTrigger and the cleanup thread's event is signalled. When the
// cleanup thread starts performing page out, it sets the variable to 2.
// When the page free count rises above PageOutLevel, PageOutNeeded is
// reset to 0.
//
// The free page count thresholds are set based upon the Gwes OOM level.

long PageOutNeeded;
long PageOutTrigger = PAGEOUT_LOW;    // Threshold level to start page out.
long PageOutLevel = PAGEOUT_HIGH;     // Threshold to stop page out.

static DWORD PFNReserveStart;   // starting PFN of the reserved memory
static DWORD PFNReserveEnd;     // ending PFN of the reserved memory


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PFREEINFO GetRegionFromPFN (DWORD dwPfn)
{
    // don't free the reserved area
    if ((dwPfn >= PFNReserveEnd) || (dwPfn < PFNReserveStart)) {
        PFREEINFO pfi, pfiEnd;
        pfi = MemoryInfo.pFi;
        pfiEnd = pfi + MemoryInfo.cFi;
        for ( ; pfi < pfiEnd ; pfi++) {
            if (dwPfn >= pfi->paStart && dwPfn < pfi->paEnd)
                return pfi;
        }
    }
    return NULL;
}

static PFREEINFO UpdatePhysicalRefCnt (LPVOID pMem, DWORD dwRefCnt)
{
    DWORD dwPfn = GetPFN(pMem);
    PFREEINFO pfi = GetRegionFromPFN (dwPfn);

    DEBUGCHK ((dwRefCnt <= 1) || (dwRefCnt == FILESYS_PAGE));

    if (pfi) {
    
        uint ix = (dwPfn - pfi->paStart) / PFN_INCR;
        
        DEBUGCHK (!pfi->pUseMap[ix] || !dwRefCnt);

        pfi->pUseMap[ix] = (WORD)dwRefCnt;
    } else {
        DEBUGMSG (1, (L"UpdatePhysicalRefCnt: Invalid Address %8.8lx (PFN %8.8lx)\r\n", pMem, dwPfn));
        DEBUGCHK (0);
    }

    return pfi;
}

//------------------------------------------------------------------------------
//  calls this function to register an Out Of Memory event, which
//  HoldPages sets when free physical memory drops below the given threshold.
//------------------------------------------------------------------------------
VOID 
NKSetOOMEvent(
    HANDLE hEvent, 
    DWORD cpLow, 
    DWORD cpCritical,
    DWORD cpLowBlockSize, 
    DWORD cpCriticalBlockSize
    ) 
{
    TRUSTED_API_VOID (L"NKSetOOMEvent");
    DEBUGMSG(ZONE_ENTRY,(L"NKSetOOMEvent entry: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        hEvent,cpLow,cpCritical,cpLowBlockSize,cpCriticalBlockSize));

    if (g_hOOMEvent) {
        HNDLCloseHandle (g_pprcNK, g_hOOMEvent);
    }
    HNDLDuplicate (pActvProc, hEvent, g_pprcNK, &g_hOOMEvent);
    g_cpLowThreshold = cpLow;
    g_cpCriticalThreshold = cpCritical;
    g_cpLowBlockSize = cpLowBlockSize;
    g_cpCriticalBlockSize = cpCriticalBlockSize;
    PageOutLevel = g_cpLowThreshold + PAGEOUT_HIGH;
    PageOutTrigger = g_cpLowThreshold + PAGEOUT_LOW;
    DEBUGMSG(ZONE_ENTRY,(L"NKSetOOMEvent exit\r\n"));
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void  LinkPage (PDLIST pListHead, PDLIST pMem, PFREEINFO pfi, DWORD dwPfn) 
{
    uint ix = (dwPfn - pfi->paStart) / PFN_INCR;
    DEBUGCHK (pfi->pUseMap[ix]);
    
    AddToDListHead (pListHead, pMem);
    pfi->pUseMap[ix] = 0;
    PageFreeCount++;
    LogPhysicalPages(PageFreeCount);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void  LinkDirtyPage (PDLIST pMem, PFREEINFO pfi, DWORD dwPfn) 
{
    KCALLPROFON(32);
    LinkPage (&g_dirtyPageList, pMem, pfi, dwPfn);
    KCALLPROFOFF(32);
}

//------------------------------------------------------------------------------
// link a page into dirty list without incrementing PageFreeCount
//------------------------------------------------------------------------------
static void  LinkDirtyPageOnly (PDLIST pMem, PFREEINFO pfi, DWORD dwPfn) 
{
    uint ix;
    KCALLPROFON(32);

    ix = (dwPfn - pfi->paStart) / PFN_INCR;
    DEBUGCHK (pfi->pUseMap[ix]);
    
    AddToDListHead (&g_dirtyPageList, pMem);
    pfi->pUseMap[ix] = 0;
    LogPhysicalPages(PageFreeCount);
    KCALLPROFOFF(32);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void  LinkCleanPage (PDLIST pMem, PFREEINFO pfi, DWORD dwPfn) 
{
    KCALLPROFON(32);
    LinkPage (&g_cleanPageList, pMem, pfi, dwPfn);
    KCALLPROFOFF(32);
}

//
// KC_GrabFirstPhysPage - grab a page, fHeld specifies if we already hold the page.
//
// NOTE: It's possible to return a dirty page even if PM_PT_ZEROED is requested.
//       The caller of this function can check pMem->pFwd to see if the page is dirty
//       and zero the page if requested
//
PDLIST KC_GrabFirstPhysPage (DWORD dwRefCnt, DWORD dwPageType, BOOL fHeld)
{
    PDLIST pMem = NULL;
    BOOL   fNeedClean = (PM_PT_ZEROED == dwPageType);

    KCALLPROFON(32);

    if (PageFreeCount || fHeld) {
    
        if (!fNeedClean) {
            // try dirty list first
            
            if (!IsDListEmpty (&g_dirtyPageList)) {
                pMem = g_dirtyPageList.pFwd;

            // try clean page list if there is no dirty pages        
            } else if (!IsDListEmpty (&g_cleanPageList)) {
                pMem = g_cleanPageList.pFwd;
            }

        // try clean page list first, if requesting clean page
        } else if (!IsDListEmpty (&g_cleanPageList)) {
            pMem = g_cleanPageList.pFwd;
            fNeedClean = FALSE;     // indicate the page is already cleaned

        // if there is a dirty page, just use it
        } else if (!IsDListEmpty (&g_dirtyPageList)) {
            pMem = g_dirtyPageList.pFwd;
        }

        PREFAST_DEBUGCHK (pMem);
        
            
        RemoveDList (pMem);

        UpdatePhysicalRefCnt (pMem, dwRefCnt);

        if (!fNeedClean) {
            // clear back/fwd ptr to get a zero'd page.
            pMem->pBack = pMem->pFwd = NULL;
        }

        if (!fHeld) {
            if (-- PageFreeCount < (long) KInfoTable[KINX_MINPAGEFREE]) {
                KInfoTable[KINX_MINPAGEFREE] = PageFreeCount;
            }
            
            LogPhysicalPages(PageFreeCount);
        }
        
    } else {
        if (!PFNReserveEnd) {
            // physical memory pool is not initialized
            pMem = (LPVOID) (pTOC->ulRAMFree + MemForPT);
            MemForPT += VM_PAGE_SIZE;
            
        } else {
            // physical memory pool is initialized and we are out of free pages
            OEMWriteDebugString (L"!!! KC_GrabFirstPhysPage: Completely Out Of Memory\r\n");
        }
    }
    KCALLPROFOFF(32);
    // debug build softlog, useful to find cache issue
    SoftLog (0x33333333, (DWORD)pMem);

    return pMem;
}

static void ZeroPageIfNecessary (PDLIST pMem, BOOL fFlush)
{
    BOOL fDirty = (NULL != pMem->pFwd);
    if (fDirty) {
        // need to zero the page
        ZeroPage (pMem);
        // debug build softlog, useful to find cache issue
        SoftLog (0x55555555, (DWORD) pMem);
    }

    if (fFlush) {
        // debug build softlog, useful to find cache issue
        SoftLog (0x55555556, (DWORD) pMem);
        OEMCacheRangeFlush (pMem, fDirty? VM_PAGE_SIZE : sizeof (DLIST), CACHE_SYNC_WRITEBACK);
    }
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

//------------------------------------------------------------------------------
// get a page that is already held
//------------------------------------------------------------------------------
DWORD GetHeldPage (DWORD dwPageType)
{
    DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;
    PDLIST pMem = (PDLIST) KCall ((PKFN) KC_GrabFirstPhysPage, 1, dwPageType, TRUE);

    if (pMem) {
        ZeroPageIfNecessary (pMem, TRUE);
#ifdef DEBUG
        if (PM_PT_ZEROED == dwPageType) {
            DEBUGCHK (PageZeroed (pMem));
        }
#endif
        dwPfn = GetPFN (pMem);
    }
    DEBUGMSG(ZONE_PHYSMEM,(TEXT("GetHeldPage: Returning %8.8lx (%8.8lx)\r\n"), dwPfn, Pfn2Virt (dwPfn)));
    return dwPfn;
}

//------------------------------------------------------------------------------
// Return a page from allocated status to held status
//------------------------------------------------------------------------------
DWORD ReholdPage (DWORD paPFN)
{
    PFREEINFO pfi = GetRegionFromPFN(paPFN);
    DWORD dwCnt = 1;
    uint ix;

    PREFAST_DEBUGCHK (pfi);

    ix = (paPFN - pfi->paStart) / PFN_INCR;
    DEBUGMSG(ZONE_PHYSMEM, (TEXT("ReholdPage: PFN=%8.8lx ix=%x rc=%d\r\n"), 
            paPFN, ix, pfi->pUseMap[ix]));
    
    EnterCriticalSection(&PhysCS);
    
    DEBUGCHK (pfi->pUseMap[ix]);

    if (1 == pfi->pUseMap[ix]) {
        LPVOID pPage = Pfn2Virt(paPFN);
        DEBUGCHK (pPage);
        
        KCall ((PKFN)LinkDirtyPageOnly, pPage, pfi, paPFN);

        DEBUGMSG(ZONE_PHYSMEM, (TEXT("ReholdPage: PFN=%8.8lx (%8.8lx)\r\n"), paPFN, Pfn2Virt (paPFN)));

        EVNTModify (pEvtDirtyPage, EVENT_SET);  
        
    } else {
    
        DEBUGCHK (MAX_REFCNT >= pfi->pUseMap[ix]);
        pfi->pUseMap[ix] --;

        // Decrement PageFreeCount if it's not 0. The reason is that paging pool is no longer the
        // owner of this page (i.e. it lost a page), and we need to hold an extra page to account
        // for it. Though it's desireable to go through "HoldPages" logic, we can't do it here. 
        // For we're most likely in low-memory/paging-out situation and calling HoldPages here can
        // lead to deadlock/livelock.
        do {
            dwCnt = PageFreeCount;
        } while (dwCnt 
             && (InterlockedCompareExchange (&PageFreeCount, dwCnt - 1, dwCnt) != dwCnt));

    }

    LeaveCriticalSection(&PhysCS);

    return dwCnt;
}

//------------------------------------------------------------------------------
//
// GrabOnePage gets the next available physical page
//
//------------------------------------------------------------------------------
LPVOID GrabOnePage (DWORD dwPageType) 
{
    PDLIST pMem = NULL;

    if (InSysCall ()) {
        pMem = KC_GrabFirstPhysPage (1, dwPageType, FALSE);
        
    } else if (HoldPages (1, FALSE)) {
        pMem = (PDLIST) KCall ((PKFN) KC_GrabFirstPhysPage, 1, dwPageType, TRUE);
    }    

    if (pMem) {
        ZeroPageIfNecessary (pMem, TRUE);
#ifdef DEBUG
        if (PM_PT_ZEROED == dwPageType) {
            DEBUGCHK (PageZeroed (pMem));
        }
#endif
    }

    DEBUGMSG(ZONE_PHYSMEM,(TEXT("GrabOnePage: Returning %8.8lx\r\n"), pMem));
    return pMem;
}

//------------------------------------------------------------------------------
// RemovePage: called by loader.c at system initialization (KernelFindMemory) to reserve
//             pages for object store. Single threaded, no protection needed.
//------------------------------------------------------------------------------
void RemovePage (DWORD dwAddr) 
{
    // should probably debugchk if this function is called after KernelFindMemory
    VERIFY (UpdatePhysicalRefCnt ((LPVOID) dwAddr, FILESYS_PAGE));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
NKGiveKPhys(
    void *ptr, 
    ulong length
    ) 
{
    LPVOID *pPageList = (LPVOID *)ptr;
    FREEINFO *pfi;
    LPBYTE pPage;
    DWORD loop, dwPfn;

    TRUSTED_API (L"NKGiveKPhys", FALSE);

    DEBUGMSG(ZONE_ENTRY,(L"NKGiveKPhys entry: %8.8lx %8.8lx\r\n",ptr,length));

    // flush D cache
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD);
    
    EnterCriticalSection(&PhysCS);

    for (loop = 0; loop < length; loop++) {
        pPage = pPageList[loop];
        dwPfn = GetPFN (pPage);

        if (   (INVALID_PHYSICAL_ADDRESS != dwPfn)
            && (pfi = GetRegionFromPFN (dwPfn))) {
            
            DEBUGCHK (FILESYS_PAGE == pfi->pUseMap[(dwPfn-pfi->paStart)/PFN_INCR]);

            KCall ((PKFN) LinkDirtyPage, pPage, pfi, dwPfn);
        } else {
            ERRORMSG(1, (TEXT("GiveKPhys : invalid address 0x%08x (PFN 0x%08x)\r\n"), pPage, dwPfn));
            DEBUGCHK(0);
        }

    }
    KInfoTable[KINX_NUMPAGES] += length;
    // If there are enough free pages, clear the PageOutNeeded flag.
    if (PageFreeCount > PageOutLevel)
        PageOutNeeded = 0;

    LeaveCriticalSection(&PhysCS);
    
    // signal dirty pages available
    NKSetEvent (g_pprcNK, hEvtDirtyPage);

    DEBUGMSG(ZONE_ENTRY,(L"NKGiveKPhys exit: %8.8lx\r\n", TRUE));
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
NKGetKPhys(
    void *ptr, 
    ulong length
    ) 
{
    PDLIST *pPages = (PDLIST *)ptr;
    BOOL bRet = FALSE;
    DWORD dwCount;
    PDLIST pMem;

    TRUSTED_API (L"NKGetKPhys", FALSE);

    DEBUGMSG(ZONE_ENTRY,(L"NKGetKPhys entry: %8.8lx %8.8lx\r\n", ptr, length));

    if ((int) length > 0) {

        // free all cached stacks
        VMFreeExcessStacks (0);

        EnterCriticalSection (&PhysCS);

        for (dwCount = length; (int) dwCount > 0; dwCount--, pPages ++) {
            if (((length > 1) && ((DWORD)PageFreeCount <= dwCount + PageOutTrigger))
                || !(pMem = (PDLIST) KCall ((PKFN)KC_GrabFirstPhysPage, FILESYS_PAGE, PM_PT_ZEROED, FALSE))) {
                break;
            }
            KInfoTable[KINX_NUMPAGES]--;
            *pPages = pMem;
        }

        LeaveCriticalSection (&PhysCS);

        if (bRet = !dwCount) {
            // got all the pages, zero them if necessary
            pPages = (PDLIST *)ptr;
            for (dwCount = length; (int) dwCount > 0; dwCount--, pPages ++) {
                ZeroPageIfNecessary (*pPages, FALSE);
            }
            
            OEMCacheRangeFlush (0, 0, CACHE_SYNC_WRITEBACK);

        } else {
            // failed, give back all the pages we got
            NKGiveKPhys (ptr,length - dwCount);
            RETAILMSG(1,(L"Error getting pages!\r\n"));
        }
    }
    DEBUGMSG(ZONE_ENTRY,(L"NKGetKPhys exit: %8.8lx\r\n",bRet));

    return bRet;
}
    

#define INIT_ZERO_PAGE_COUNT    16

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
    DWORD dwToClean = INIT_ZERO_PAGE_COUNT;
    PDLIST pPageList = &g_cleanPageList;
    PFREEINFO pfi, pfiEnd;
    DWORD dwPFN;
    PDLIST pPage;
    
    /* Fill in data fields in user visible kernel data page */
    KInfoTable[KINX_PROCARRAY] = (long)g_pprcNK;
    KInfoTable[KINX_PAGESIZE] = VM_PAGE_SIZE;
#ifdef PFN_SHIFT
    KInfoTable[KINX_PFN_SHIFT] = PFN_SHIFT;
#endif
    KInfoTable[KINX_PFN_MASK] = (DWORD)PFNfromEntry(~0);
    KInfoTable[KINX_MEMINFO] = (long)&MemoryInfo;
    KInfoTable[KINX_KDATA_ADDR] = (long)g_pKData;

    InitDList (&g_cleanPageList);
    InitDList (&g_dirtyPageList);

    PFNReserveStart = GetPFN ((LPVOID)pTOC->ulRAMFree);
    PFNReserveEnd = GetPFN ((LPVOID) (pTOC->ulRAMFree + MemForPT));
    
    OEMCacheRangeFlush (0, 0, CACHE_SYNC_DISCARD);

    for (pfi = MemoryInfo.pFi, pfiEnd = pfi+MemoryInfo.cFi ; pfi < pfiEnd ; ++pfi) {
        DEBUGMSG(ZONE_MEMORY, (TEXT("InitMemoryPool: Init range: map=%8.8lx start=%8.8lx end=%8.8lx\r\n"),
                pfi->pUseMap, pfi->paStart, pfi->paEnd));
        pPage = 0;
        for (dwPFN = pfi->paStart; dwPFN < pfi->paEnd; dwPFN += PFN_INCR) {
            if (!pfi->pUseMap[(dwPFN-pfi->paStart)/PFN_INCR]) {

                if (!dwToClean --) {
                    pPageList = &g_dirtyPageList;
                }
                
                pPage = Pfn2Virt(dwPFN);

                // adding to clean list, zero the page
                if (&g_cleanPageList == pPageList) {
                    ZeroPage (pPage);
                }

                // link the page to either clean or dirty list. Note that we're in system initialization phase,
                // no protection is required
                AddToDListHead (pPageList, pPage);
                PageFreeCount++;
                LogPhysicalPages(PageFreeCount);

                // increment total number of pages
                KInfoTable[KINX_NUMPAGES]++;
            }
        }
    }

    // write-back cache for the pages we zero'd
    OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK);
    KInfoTable[KINX_MINPAGEFREE] = PageFreeCount;
    DEBUGMSG(ZONE_MEMORY,(TEXT("InitMemoryPool done, free=%d\r\n"),PageFreeCount));
}

PDLIST GetADirtyPage (PFREEINFO *ppfi, LPDWORD pdwPfn)
{
    PDLIST pMem = NULL;
    KCALLPROFON (24);
    if (PageFreeCount && !IsDListEmpty (&g_dirtyPageList)) {
        DWORD dwPfn;
        PFREEINFO pfi;
        pMem = g_dirtyPageList.pFwd;
        PageFreeCount --;
        dwPfn = GetPFN (pMem);
        DEBUGCHK (INVALID_PHYSICAL_ADDRESS != dwPfn);
        pfi = GetRegionFromPFN (dwPfn);
        DEBUGCHK (pfi);

        pfi->pUseMap[(dwPfn - pfi->paStart) / PFN_INCR] = 1;
        RemoveDList (pMem);

        *ppfi = pfi;
        *pdwPfn = dwPfn;
    }
    KCALLPROFOFF (24);
    return pMem;
}

//
// CleanPagesInTheBackground cleanup dirty pages while we're idle.
// The is optional and is here to improve performance. The idea is that when
// there are pages in the dirty page list, and we get into idle, we might as well clean the 
// dirty pages such that future memory allocation is more likely to get a clean page.
//
void CleanPagesInTheBackground (void)
{

    PDLIST    pMem;
    PFREEINFO pfi = NULL;
    DWORD     dwPfn = 0;
    DEBUGMSG (ZONE_SCHEDULE, (L"Start cleaning dirty pages in the background...\r\n"));

    // run at idle priority
    THRDSetPrio256 (pCurThread, THREAD_RT_PRIORITY_IDLE);
    while (TRUE) {
        NKWaitForSingleObject (hEvtDirtyPage, INFINITE);

        DEBUGMSG (ZONE_PHYSMEM, (L"CleanPagesInTheBackground - pFwd = %8.8lx, pBack = %8.8lx\r\n",
                        g_dirtyPageList.pFwd, g_dirtyPageList.pBack));

        while (pMem = (PDLIST) KCall ((PKFN) GetADirtyPage, &pfi, &dwPfn)) {
            DEBUGMSG (ZONE_PHYSMEM, (L"Cleaning page %8.8lx\r\n", pMem));
            // debug build softlog, useful to find cache issue
            SoftLog (0x88888888, (DWORD)pMem);
            ZeroPage (pMem);

            OEMCacheRangeFlush (pMem, VM_PAGE_SIZE, CACHE_SYNC_WRITEBACK);

            // link the list back to the clean page list
            KCall ((PKFN) LinkCleanPage, pMem, pfi, dwPfn);

            // debug build softlog, useful to find cache issue
            SoftLog (0x88888889, (DWORD)pMem);
        }
    }
    DEBUGCHK (0);
    NKExitThread (0);

    DEBUGCHK (0);
    // should've never exited.
}

_inline BOOL AllocationExceedLowMemThreshold (long cpNeed)
{
    return ((cpNeed > g_cpLowBlockSize) && (cpNeed+g_cpLowThreshold > PageFreeCount))               // low mem, cpNeed > g_cpLowBlockSize
        || ((cpNeed > g_cpCriticalBlockSize) && (cpNeed + g_cpCriticalThreshold > PageFreeCount));  // critical mem, cpNeed > g_cpCriticalBlockSize
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL HoldPages (int cpNeed, BOOL fQueryOnly)
{
    BOOL fSetOomEvent = FALSE;
    BOOL bRet;
    DWORD pfc, pfc2;

    EnterCriticalSection (&PhysCS);
    
    // Check if this request will drop the page free count below the
    // page out trigger and signal the clean up thread to start doing
    // pageouts if so.
    if (WillTriggerPageOut (cpNeed)
        && (!PageOutNeeded || (PagedInCount > PAGE_OUT_TRIGGER))) {
        
        PageOutNeeded = 1;
        PagedInCount = 0;

        // low memory condition hit. wakeup the "pageout" thread to page out pages
        if (GET_CPRIO(pCurThread) < GET_CPRIO(pCleanupThread))
            KCall((PKFN)SetThreadBasePrio, pCleanupThread, (DWORD)GET_CPRIO(pCurThread));
        NKSetEvent (g_pprcNK, hAlarmThreadWakeup);
    }

    bRet = (cpNeed+g_cpLowThreshold <= PageFreeCount);

    if (!bRet) {
        // free all cached stacks
        // NOTE: We must release physical memory CS before acquiring VMcs, or we can deadlock.
        LeaveCriticalSection (&PhysCS);
        VMFreeExcessStacks (0);
        EnterCriticalSection (&PhysCS);
        bRet = (cpNeed+g_cpLowThreshold <= PageFreeCount);
    }

    // Do not allow a request of size g_cpLowBlockSize to succeed if
    // doing so would leave less than the low threshold.  Same with
    // g_cpCriticalBlockSize and g_cpCriticalThreshold.
    if (!bRet && !AllocationExceedLowMemThreshold (cpNeed)) {

        // Memory is low. Notify OOM Handler, so that it can ask
        // the user to close some apps.
        if (g_hOOMEvent
            && ((PageFreeCount >= g_cpLowThreshold)
                || ((PageFreeCount < cpNeed + g_cpCriticalThreshold)))) {
            fSetOomEvent = TRUE;
        }
        
        if (cpNeed + STACK_RESERVE <= PageFreeCount) {
            bRet = TRUE;
        }
    }

    if (bRet && !fQueryOnly) {
        pfc = InterlockedExchangeAdd (&PageFreeCount, -cpNeed) - cpNeed;   // decrement PageFreeCount by page needed
        
        // update page free water mark. 
        //
        // NOTE: don't use PageFreeCount directly since other threads might increment
        // it and it'll make the water mark inaccurate. And if it been decremented, other threads will
        // update it correctly.
        do {
            pfc2 = KInfoTable[KINX_MINPAGEFREE];
        } while ((pfc2 > pfc)
            && (InterlockedTestExchange ((PLONG)&KInfoTable[KINX_MINPAGEFREE], pfc2, pfc) != (LONG)pfc2));
        LogPhysicalPages(PageFreeCount);
    }

    LeaveCriticalSection(&PhysCS);

    // signal OOM handler if we're in low memory condition
    if (fSetOomEvent) {
        NKSetEvent (g_pprcNK, g_hOOMEvent);
    }
    return bRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL DupPhysPage (DWORD dwPFN)
{
    PFREEINFO pfi;
    uint ix;
    BOOL fRet = TRUE;

    // caller of DupPhysPage must hold PhysCS
    DEBUGCHK (OwnCS (&PhysCS));
    
    if ((pfi = GetRegionFromPFN(dwPFN)) != 0) {
        ix = (dwPFN - pfi->paStart) / PFN_INCR;

        fRet = (pfi->pUseMap[ix] < MAX_REFCNT);
        DEBUGCHK (pfi->pUseMap[ix]);

        if (fRet) {
            DEBUGMSG(ZONE_PHYSMEM, (TEXT("DupPhysPage: PFN=%8.8lx ix=%x rc=%d\r\n"),
                dwPFN, ix, pfi->pUseMap[ix]));
            ++pfi->pUseMap[ix];
        } else {
            DEBUGCHK (0);
            RETAILMSG (1, (L"DupPhysPage failed - too many alias to the same page, PFN = %8.8lx #alias = 0x%x\r\n",
                dwPFN, pfi->pUseMap[ix]));
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
    BOOL fFreed = FALSE;
    PFREEINFO pfi;
    uint ix;

    if ((pfi = GetRegionFromPFN(paPFN)) != 0) {
        ix = (paPFN - pfi->paStart) / PFN_INCR;
        DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePhysPage: PFN=%8.8lx ix=%x rc=%d\r\n"), paPFN,
                ix, pfi->pUseMap[ix]));
        
        EnterCriticalSection(&PhysCS);
        
        DEBUGCHK (pfi->pUseMap[ix]);

        if (1 == pfi->pUseMap[ix]) {
            LPVOID pPage = Pfn2Virt(paPFN);
            DEBUGCHK (pPage);
            
            KCall ((PKFN)LinkDirtyPage, pPage, pfi, paPFN);

            SoftLog (0x44444444, (DWORD) pPage);
            DEBUGMSG(ZONE_PHYSMEM, (TEXT("FreePhysPage: PFN=%8.8lx (%8.8lx)\r\n"), paPFN, Pfn2Virt (paPFN)));

            EVNTModify (pEvtDirtyPage, EVENT_SET);  

            // If there are enough free pages, clear the PageOutNeeded flag.
            if (PageFreeCount > PageOutLevel)
                PageOutNeeded = 0;

            fFreed = TRUE;
            
        } else if (MAX_REFCNT >= pfi->pUseMap[ix]) {
            pfi->pUseMap[ix] --;
        }

        LeaveCriticalSection(&PhysCS);  
    }
    return fFreed;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL
TakeSpecificPage(
    PFREEINFO pfi, 
    uint ix,
    PDLIST pMem
    ) 
{
    BOOL fRet;
    KCALLPROFON(40);
    if (fRet = !pfi->pUseMap[ix]) { 
        // debug build softlog, useful to find cache issue
        SoftLog (0x22222222, (DWORD)pMem);
        pfi->pUseMap[ix] = TRANSITION_PAGE;
        RemoveDList (pMem);
    }
    KCALLPROFOFF(40);
    return fRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void
GiveSpecificPage(
    PFREEINFO pfi, 
    uint ix, 
    PDLIST pMem
    ) 
{
    KCALLPROFON(40);
    if (TRANSITION_PAGE == pfi->pUseMap[ix]) {
        // debug build softlog, useful to find cache issue
        SoftLog (0x22222223, (DWORD)pMem);
        pfi->pUseMap[ix] = 0;
        AddToDListHead (&g_dirtyPageList, pMem);
    }
    KCALLPROFOFF(40);
}


//------------------------------------------------------------------------------
//
// Try to grab physically contiguous pages. This routine needs to be preemptive
// as much as possible, so we may take some pages and end up giving them back if
// a full string of pages can't be found. Pages are potentially coming and 
// going, but we'll only make one pass through memory.
//
//------------------------------------------------------------------------------
DWORD
GetContiguousPages(
    DWORD dwPages,
    DWORD dwAlignment, 
    DWORD dwFlags
    ) 
{
    PFREEINFO pfi, pfiEnd;
    uint ixWalk, ixTarget, ixStart, ixTotal, ixTemp;
    DWORD paRet = INVALID_PHYSICAL_ADDRESS;
    
    if (((DWORD) PageFreeCount > dwPages + MIN_PROCESS_PAGES) && HoldPages(dwPages, FALSE)) {
        //
        // We've at least got enough pages available and that many have been set
        // aside for us (though not yet assigned). 
        //
        pfi = &MemoryInfo.pFi[0];
        pfiEnd = pfi + MemoryInfo.cFi;

        for (; pfi < pfiEnd ; ++pfi) {
            //
            // For each memory section, scan the free pages.
            //
            ixTotal = (pfi->paEnd - pfi->paStart) / PFN_INCR;
            //
            // Find a string of free pages
            //
            for (ixStart = 0; ixStart <= (ixTotal - dwPages); ) {
                
                if (pfi->pUseMap[ixStart] || (dwAlignment & PFN2PA(pfi->paStart + (ixStart * PFN_INCR))) ) {
                    //
                    // Page in use or doesn't match alignment request, skip ahead
                    //
                    ixStart++;
                } else {
                    ixTarget = ixStart + dwPages;
                    for (ixWalk = ixStart; ixWalk < ixTarget; ixWalk++) {
                        
                        if (pfi->pUseMap[ixWalk] || 
                            !KCall((PKFN)TakeSpecificPage, pfi, ixWalk, Pfn2Virt(pfi->paStart + (ixWalk * PFN_INCR)))) {

                            //
                            // Page in use, free all the pages so far and reset start
                            //
                            for (ixTemp = ixStart; ixTemp < ixWalk; ixTemp++) {
                                KCall((PKFN)GiveSpecificPage, pfi, ixTemp, Pfn2Virt(pfi->paStart + (ixTemp * PFN_INCR)));
                            }
                            NKSetEvent (g_pprcNK, hEvtDirtyPage);
                            ixStart = ixWalk + 1;
                            break;
                        } 
                    }

                    if (ixWalk == ixTarget) {
                        for (ixWalk = ixStart; ixWalk < ixTarget; ixWalk++) {
                            DEBUGCHK (TRANSITION_PAGE == pfi->pUseMap[ixWalk]);
                            pfi->pUseMap[ixWalk] = 1;
                        }                        
                        paRet = pfi->paStart + (ixStart * PFN_INCR);
                        goto foundPages;
                    }
                }
            }
        }
        //
        // We couldn't lock down a contiguous section so free up the "held" 
        // pages before leaving.
        //
        InterlockedExchangeAdd (&PageFreeCount, dwPages);
    }
foundPages:
    if (INVALID_PHYSICAL_ADDRESS != paRet) {
        // the pages we got can come from both clean and dirty list, just do a
        // CACHE_SYNC_DISCARD here to make sure cache coherency.
        // Pages will be zero'd uncached by caller.
        OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD);
    } else {
        NKLOG (PageFreeCount);
        DEBUGMSG (1, (L"GetContiguousPages failed, PageFreeCount = 0x%8.8lx\r\n", PageFreeCount));
        //DumpDwords ((const DWORD *)MemoryInfo.pFi[0].pUseMap, (MemoryInfo.pFi[0].paEnd-MemoryInfo.pFi[0].paStart)/(4*PFN_INCR));
    }
    DEBUGMSG(ZONE_PHYSMEM,(TEXT("GetContiguousPages: Returning %8.8lx\r\n"), paRet));
    return paRet;
}


