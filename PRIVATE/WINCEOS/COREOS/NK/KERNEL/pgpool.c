//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
// 
//      NK Kernel loader code
// 
// 
// Module Name:
// 
//      pgpool.c
// 
// Abstract:
// 
//      This file implements fixed-size paging pool
// 
// 
//------------------------------------------------------------------------------
//
// The loader is derived from the Win32 executable file format spec.  The only
// interesting thing is how we load a process.  When we load a process, we
// simply schedule a thread in a new process context.  That thread (in coredll)
// knows how to read in an executable, and does so.  Then it traps into the kernel
// and starts running in the new process.  Most of the code below is simply a direct
// implimentation of the executable file format specification
//
//------------------------------------------------------------------------------

#include "kernel.h"

#define MIN_PGPOOL_SIZE     (64*PAGE_SIZE)          // minimum paging pool size 256K

// uncomment the following line to debug PAGING POOL
// #define DEBUG_PAGING_POOL

#ifdef DEBUG_PAGING_POOL
#undef  ZONE_PAGING
#define ZONE_PAGING     1
#endif

//
// the paging pool entry
//
// NOTE: we use index instead of direct pointer becuase we want to keep the size of this entry of 
//       size 16 byte. Allocation and cache is better utilized this way.
//
typedef struct _PagingPoolEntry {
    WORD idxPrev;       // index to the previous entry
    WORD idxNext;       // index to the next entry
    WORD idxNextInMod;  // index to the next entry of the same MOD/PROC
    WORD pad;           // padding
    DWORD va;           // VA of the page
    LPVOID pModProc;    // pointer to MODULE/PROC that owns this page
} PAGINGPOOL, *PPAGINGPOOL;


// the size of the paging pool. Defualt 0, which uses unlimited paging pool
DWORD cbNKPagingPoolSize = (DWORD) -1;

static LPBYTE gpPagingPool;             // the paging memory pool
static PPAGINGPOOL gpPoolArr;           // the paging pool structure array
static LONG gnFreePages;                // total number of free pages
static WORD gidxFree;                   // index of the head of the free list
static WORD gidxHead;                   // head of the used list (circular)

extern CRITICAL_SECTION PagerCS, VAcs;  // the pager critical section

#define FREE_PAGE_VA        0xabababab

BOOL InitPgPool (void)
{
    int i;

#ifdef DEBUG_PAGING_POOL
    cbNKPagingPoolSize = MIN_PGPOOL_SIZE;
#endif

    // treat -1 as 0
    if ((DWORD) -1 == cbNKPagingPoolSize) {
        cbNKPagingPoolSize = 0;
    }

    // do nothing if not using paging pool
    if (!cbNKPagingPoolSize) {
        return TRUE;
    }

    // adjust to min/max, align to page size
    if (cbNKPagingPoolSize < MIN_PGPOOL_SIZE) {
        cbNKPagingPoolSize = MIN_PGPOOL_SIZE;
    } else {
        cbNKPagingPoolSize = PAGEALIGN_UP(cbNKPagingPoolSize);
    }

    // initially all pages are free
    gnFreePages = (cbNKPagingPoolSize / PAGE_SIZE);

    // make sure we don't have too many pages that we can account for
    if ((DWORD)gnFreePages >= (DWORD) INVALID_PG_INDEX) {
        gnFreePages = INVALID_PG_INDEX - 1;
        cbNKPagingPoolSize = gnFreePages * PAGE_SIZE;
    }

    // total memory needed == PAGE_ALIGN_UP(gnFreePages * sizeof (PAGINGPOOL)) + cbNKPagingPoolSize, carve it from the begining of free RAM
    gpPagingPool = (LPBYTE) (pTOC->ulRAMFree + MemForPT);
    MemForPT += cbNKPagingPoolSize + PAGEALIGN_UP(gnFreePages * sizeof (PAGINGPOOL));
    gpPoolArr = (PPAGINGPOOL) (gpPagingPool + cbNKPagingPoolSize);

    DEBUGMSG (1, (L"Using Paging Pool, size = 0x%8.8lx, Addr = 0x%8.8lx, gpPoolArr = 0x%8.8lx\r\n", cbNKPagingPoolSize, gpPagingPool, gpPoolArr));

    // initialize the free list
    for (i = 0; i < gnFreePages; i ++) {
        gpPoolArr[i].idxNext = (WORD) (i+1);
#ifdef DEBUG
        gpPoolArr[i].va = FREE_PAGE_VA;
        gpPoolArr[i].pModProc = (LPVOID) 0xabababab;
        gpPoolArr[i].idxPrev = INVALID_PG_INDEX;
        gpPoolArr[i].idxNextInMod = INVALID_PG_INDEX;
#endif
    }

    // take care of 'next' of the last element
    gpPoolArr[gnFreePages-1].idxNext = INVALID_PG_INDEX;

    gidxHead = INVALID_PG_INDEX;

    return TRUE;
}


_inline WORD pg_dequeue (PPGPOOL_Q ppq)
{
    WORD idx = ppq->idxHead;
    if (INVALID_PG_INDEX != idx)
        ppq->idxHead = gpPoolArr[idx].idxNextInMod;
    return idx;
}

_inline void pg_enqueue (PPGPOOL_Q ppq, WORD idx)
{
    DEBUGCHK (INVALID_PG_INDEX != idx);
    gpPoolArr[idx].idxNextInMod = INVALID_PG_INDEX;

    if (INVALID_PG_INDEX == ppq->idxHead) {
        // queue was empty
        ppq->idxHead = idx;
    } else {
        DEBUGCHK (INVALID_PG_INDEX != ppq->idxTail);
        gpPoolArr[ppq->idxTail].idxNextInMod = idx;
    }
    ppq->idxTail = idx;
}

_inline void pg_AddToUsedPage (WORD idx)
{
    if (INVALID_PG_INDEX == gidxHead) {
        gpPoolArr[idx].idxNext = gpPoolArr[idx].idxPrev = gidxHead = idx;
    } else {
        WORD idxPrev = gpPoolArr[gidxHead].idxPrev;
        gpPoolArr[idx].idxNext = gidxHead;
        gpPoolArr[idx].idxPrev = idxPrev;
        gpPoolArr[gidxHead].idxPrev = gpPoolArr[idxPrev].idxNext = idx;
    }
}

_inline void pg_RemoveFromUsedPage (WORD idx)
{
    DEBUGCHK (INVALID_PG_INDEX != idx);
    if (gpPoolArr[idx].idxNext == idx) {
        // last page
        DEBUGCHK ((idx == gidxHead) && (idx == gpPoolArr[idx].idxPrev));
        gidxHead = INVALID_PG_INDEX;
    } else {
        WORD idxPrev = gpPoolArr[idx].idxPrev;
        WORD idxNext = gpPoolArr[idx].idxNext;
        gpPoolArr[idxPrev].idxNext = idxNext;
        gpPoolArr[idxNext].idxPrev = idxPrev;
        if (gidxHead == idx)
            gidxHead = idxNext;
    }
}

static BOOL TryInvalidateAddr (DWORD dwAddr, DWORD dwInUseOrig)
{
    PSECTION pscn;
    MEMBLOCK *pmbMaster;
    DWORD   dwZeroAddr = ZeroPtr(dwAddr);
    int     ixBlock = (dwAddr >> VA_BLOCK) & BLOCK_MASK;
    int     ixPage = (dwAddr >> VA_PAGE) & PAGE_MASK;
    DWORD   dwEntry = 0;

    DEBUGCHK (!IsKernelVa (dwAddr));

    EnterCriticalSection (&VAcs);
    DEBUGMSG(ZONE_PAGING,(L"TryInvalidateAddr: dwAddr = %8.8lx, dwInUseOrig = %8.8lx\r\n", dwAddr, dwInUseOrig));

    // find the right section based on address
    if (dwZeroAddr >= (DWORD) DllLoadBase) {
        // DLL's address
        DEBUGCHK ((dwZeroAddr >= (DWORD) DllLoadBase) && !(dwInUseOrig & 1));
        pscn = (dwZeroAddr < (1 << VA_SECTION))? &NKSection : SectionTable[dwZeroAddr >> VA_SECTION];
    } else {
        // Process address or memory mapped area
        DEBUGCHK (!dwInUseOrig);
        pscn = SectionTable[dwAddr >> VA_SECTION];
    }

    pmbMaster = (*pscn)[ixBlock];
    if (!pmbMaster || (NULL_BLOCK == pmbMaster) || (RESERVED_BLOCK == pmbMaster) || !pmbMaster->aPages[ixPage]) {
        // the page has already freed. just return TRUE
        LeaveCriticalSection (&VAcs);
        return TRUE;
    }

    // we can only page it out if it's not locked
    if (!pmbMaster->cLocks) {

        dwEntry = pmbMaster->aPages[ixPage];

        DEBUGCHK (dwEntry);

        // if it's the old style DLL, need to make sure the page is not locked in any process
        if (dwInUseOrig && (dwZeroAddr < (1 << VA_SECTION))) {
            int i;
            MEMBLOCK *pmb;
            DWORD dwInUse;
            // make sure no page is locked.
            for (i = 1, dwInUse = dwInUseOrig; dwInUse; i ++) {
                DEBUGCHK (i < MAX_PROCESSES);
                if (dwInUse & (1 << i)) {
                    dwInUse &= ~(1 << i);
                    pmb = (*(SectionTable[i+1]))[ixBlock];
                    DEBUGCHK (NULL_BLOCK != pmb);
                    if ((RESERVED_BLOCK != pmb) && pmb->cLocks) {
                        // page is locked, can't page this one out
                        dwEntry = 0;
                        break;
                    }
                }
            }

            if (dwEntry) {
                // page isn't locked. Page it out
                for (i = 1, dwInUse = dwInUseOrig; dwInUse; i ++) {
                    DEBUGCHK (i < MAX_PROCESSES);
                    if (dwInUse & (1 << i)) {
                        dwInUse &= ~(1 << i);
                        pmb = (*(SectionTable[i+1]))[ixBlock];
                        DEBUGCHK (NULL_BLOCK != pmb);
                        if (RESERVED_BLOCK != pmb) {
                            DEBUGCHK (!pmb->aPages[ixPage] || ((pmb->aPages[ixPage] & -PAGE_SIZE) == (dwEntry & -PAGE_SIZE)));
                            pmb->aPages[ixPage] = 0;
#if HARDWARE_PT_PER_PROC
                            // after we updated all platforms to support single entry TLB flush, 
                            // we'll do the same for all CPUs
                            InvalidateRange (MapPtrProc(dwZeroAddr, ProcArray + i), PAGE_SIZE);
#endif
                        }
                    }
                }
            }
        }
        
        // update the common part (process, shared, or secure slot)
        if (dwEntry) {
            OEMCacheRangeFlush (MapPtrProc(dwZeroAddr, ProcArray), PAGE_SIZE, CACHE_SYNC_INSTRUCTIONS);
            pmbMaster->aPages[ixPage] = 0;
        }
    }
    LeaveCriticalSection (&VAcs);

    DEBUGMSG (!dwEntry && ZONE_PAGING, (L"TryInvalidateAddr: dwAddr (%8.8lx) locked.\r\n", dwAddr));

    if (dwEntry) {
        // We're counting on Invalidate Range for anything other than x86 will flush the TLB.
        // MUST update this if we decided to do range flush on TLB for other CPUs
        InvalidateRange ((LPVOID) dwAddr, PAGE_SIZE);
        return TRUE;
    }
    return FALSE;
}

static WORD PageOutPage (void)
{
    WORD        idxEntry, idxOrig = gidxHead;
    DWORD       va, dwInuse;
    PPGPOOL_Q   ppq;

    DEBUGCHK (!gnFreePages && (INVALID_PG_INDEX != gidxHead));
    // 
    // try to find a page that we can page it out (can't page it out if it's locked).
    //
    do {

        // always start from head pointer as it'll be updated on every loop
        idxEntry = gidxHead;
        va = gpPoolArr[idxEntry].va;

        // advance the head pointer
        gidxHead = gpPoolArr[idxEntry].idxNext;

        DEBUGCHK (INVALID_PG_INDEX != gidxHead);
        DEBUGMSG (ZONE_PAGING, (L"PageOutPage: Trying paging out entry %8.8lx, va = %8.8lx\r\n", idxEntry, va));

        if (ZeroPtr (va) < (DWORD) DllLoadBase) {
            // process
            ppq = &((PPROCESS) gpPoolArr[idxEntry].pModProc)->pgqueue;
            dwInuse = 0;
        } else if ((va >= FIRST_MAPPER_ADDRESS) && (va < LAST_MAPPER_ADDRESS)) {
            // memory mapped file
            ppq = &((LPFSMAP) gpPoolArr[idxEntry].pModProc)->pgqueue;
            dwInuse = 0;

        } else {
            // module
            PMODULE pMod = (PMODULE) gpPoolArr[idxEntry].pModProc;
            ppq = &pMod->pgqueue;
            dwInuse = pMod->inuse;
        }
        
        DEBUGCHK (idxEntry == ppq->idxHead);
        
        // remove the entry from the page owned by the process/module
        pg_dequeue (ppq);
        
        // try to remove the page entry from VM
        if (TryInvalidateAddr (va, dwInuse)) {
            // successfully paged out the page, remove it from the used list and return the entry
            pg_RemoveFromUsedPage (idxEntry);
#ifdef DEBUG
            {
                LPCWSTR pszName;
                if (ZeroPtr (va) < (DWORD) DllLoadBase) {
                    pszName = L"Process";
                } else if ((va >= FIRST_MAPPER_ADDRESS) && (va < LAST_MAPPER_ADDRESS)) {
                    pszName = L"MapFile";
                } else {
                    pszName = L"Module";
                }
                
                gpPoolArr[idxEntry].va = FREE_PAGE_VA;
                DEBUGMSG (ZONE_PAGING, (L"PageOutPage: Paged out page at %8.8lx, in %s %8.8lx\r\n",
                    va, pszName, gpPoolArr[idxEntry].pModProc));
            }
#endif
            return idxEntry;
        }

        // the page is locked, link it back to the end of the list owned by the process/module
        pg_enqueue (ppq, idxEntry);

        
    } while (gidxHead != idxOrig);

    // we've exhausted the paging pool and can't find a page to page out.
    // fail the request.
    DEBUGMSG (1, (L"PageOutPage: ALL PAGES IN PAGING POOL ARE LOCKED\r\n"));
    return INVALID_PG_INDEX;
}

static WORD GetFreePage (void)
{
    WORD idxEntry;
    
    DEBUGCHK ((gnFreePages > 0) && (INVALID_PG_INDEX != gidxFree) && IsKernelVa (gpPoolArr[gidxFree].va));
    idxEntry = gidxFree;
    gidxFree = gpPoolArr[gidxFree].idxNext;
    -- gnFreePages;

    DEBUGCHK (gnFreePages || (INVALID_PG_INDEX == gidxFree));
    DEBUGMSG (ZONE_PAGING, (L"GetFreePage: Got free page at index %8.8lx, va = %8.8lx\r\n", idxEntry, gpPoolArr[idxEntry].va));
        
    return idxEntry;
}

LPBYTE GetPagingPage (PPGPOOL_Q ppq, o32_lite *optr, BYTE filetype, WORD *pidx)
{
    if (!cbNKPagingPoolSize                                                                     // no paging pool
        || (optr                                                                                // not memory mapped file
            && ((optr->o32_flags & (IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_NOT_PAGED))               // not read only or not pageable
                || ((FT_ROMIMAGE == filetype) && !(optr->o32_flags & IMAGE_SCN_COMPRESSED))))   // in ROM and not compressed
        ) {
        DEBUGMSG (cbNKPagingPoolSize && ZONE_PAGING, 
            (L"GetPagingPage: Not using paging pool (filetype = %d, o32_flags = %8.8lx)\r\n", filetype, optr->o32_flags));
        return NULL;
    }
    
    DEBUGCHK (PagerCS.OwnerThread == hCurThread);
    DEBUGCHK ((gnFreePages > 0) || (INVALID_PG_INDEX == gidxFree));

    // grab a page either from the free list or page out a page
    *pidx = gnFreePages? GetFreePage () : PageOutPage ();
    if (INVALID_PG_INDEX == *pidx) {
        // ALL PAGES LOCKED!!
        DEBUGCHK (0);
        return NULL;
    }
    
    DEBUGCHK (FREE_PAGE_VA == gpPoolArr[*pidx].va);
    DEBUGMSG (ZONE_PAGING, (L"GetPagingPage: Found page (va = %8.8lx, *pidx = %8.8lx)\r\n", 
        gpPagingPool + (((DWORD) *pidx) << VA_PAGE), *pidx));
    return gpPagingPool + (((DWORD) *pidx) << VA_PAGE);
    
}

void AddPageToFreeList (WORD idx)
{
    DEBUGCHK (PagerCS.OwnerThread == hCurThread);
    DEBUGCHK (FREE_PAGE_VA == gpPoolArr[idx].va);

    gpPoolArr[idx].idxNext = gidxFree;
    gidxFree = idx;
    gnFreePages ++;
    
    DEBUGMSG (ZONE_PAGING, (L"AddPageToFreeList: (idx = %8.8lx)\r\n", idx));
}

void AddPageToQueue (PPGPOOL_Q ppq, WORD idx, DWORD vaddr, LPVOID pModProc)
{
    DWORD zaddr = ZeroPtr (vaddr);
    DEBUGCHK (PagerCS.OwnerThread == hCurThread);
    DEBUGCHK (FREE_PAGE_VA == gpPoolArr[idx].va);

    // for Modules, uses the secure slot address
    if (zaddr >= (DWORD) DllLoadBase)
        vaddr = (DWORD) MapPtrProc (zaddr, ProcArray);
    // now the page is used for the new user mode va
    gpPoolArr[idx].va = vaddr;
    gpPoolArr[idx].pModProc = pModProc;

    // add the entry to the used list
    pg_AddToUsedPage (idx);

    // add the entry to the process/module's owned list
    pg_enqueue (ppq, idx);
#ifdef DEBUG
    {
        LPCWSTR pszName;
        if (zaddr < (DWORD) DllLoadBase) {
            pszName = L"Process";
        } else if ((zaddr >= FIRST_MAPPER_ADDRESS) && (zaddr < LAST_MAPPER_ADDRESS)) {
            pszName = L"MapFile";
        } else {
            pszName = L"Module";
        }
        DEBUGMSG (ZONE_PAGING, (L"AddPageToQueue: add entry %8.8lx (%8.8lx) to %s %8.8lx owned list\r\n", 
            idx, vaddr, pszName, pModProc));
    }
#endif

}

void FreeAllPagesFromQ (PPGPOOL_Q ppq)
{
    WORD idx;
    EnterCriticalSection (&PagerCS);

    while (INVALID_PG_INDEX != (idx = pg_dequeue (ppq))) {

        // remove from the used list
        pg_RemoveFromUsedPage (idx);

        DEBUGCHK (gpPoolArr[idx].va != FREE_PAGE_VA);

#ifdef DEBUG            
        gpPoolArr[idx].va = FREE_PAGE_VA;
#endif

        // add to free list
        AddPageToFreeList (idx);

    }
    
    LeaveCriticalSection (&PagerCS);
}

