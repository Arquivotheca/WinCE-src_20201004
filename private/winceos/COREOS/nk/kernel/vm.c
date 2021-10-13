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
//
//    vm.c - VM implementations
//
#include <kernel.h>
#include <pager.h>
#include <sqmutil.h>

const volatile DWORD gdwFailPowerPaging = 0;    // Can be overwritten as a FIXUPVAR in config.bib
DWORD dwMaxUserAllocAddr = VM_DLL_BASE;         // Can be overwritten as a FIXUPVAR in config.bib


//
// This is to limit total number of process/threads in the system such that we don't use up
// kernel VM.
//
#define MAX_STACK_ADDRESS       (VM_CPU_SPECIFIC_BASE - 0x04000000)     // leave at least 64M of VM for kernel

#define VM_PD_DUP_ADDR          (VM_SHARED_HEAP_BASE - 0x400000)        // last 4M of user VM
#define VM_PD_DUP_SIZE          0x200000                                // 2M total size

#define VM_USER_LIMIT           VM_PD_DUP_ADDR

PSTATICMAPPINGENTRY g_StaticMappingList = NULL;

void MDInitStack (LPBYTE lpStack, DWORD cbSize);

static DWORD VMRelease (PPROCESS pprc, DWORD dwAddr, DWORD cPages);

static DWORD PagerNone (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    return PAGEIN_FAILURE;
}

extern CRITICAL_SECTION PhysCS;

#define LockPhysMem()       EnterCriticalSection(&PhysCS)
#define UnlockPhysMem()     LeaveCriticalSection(&PhysCS)

static BOOL Lock2VM (PPROCESS pprc1, PPROCESS pprc2)
{
    if (pprc1->dwId < pprc2->dwId) {
        EnterCriticalSection (&pprc2->csVM);
        EnterCriticalSection (&pprc1->csVM);
    } else {
        EnterCriticalSection (&pprc1->csVM);
        EnterCriticalSection (&pprc2->csVM);
    }
    if (pprc1->ppdir && pprc2->ppdir) {
        return TRUE;
    }
    LeaveCriticalSection (&pprc1->csVM);
    LeaveCriticalSection (&pprc2->csVM);
    return FALSE;
}

typedef DWORD (* PFNPager) (PPROCESS pprc, DWORD dwAddr, BOOL fWrite);

static DWORD PageAutoCommit (PPROCESS pprc, DWORD dwAddr, BOOL fWrite);

//
// the following arrays Must match the VM_PAGER_XXX declaration in vm.h
//

// the pager function
static PFNPager g_vmPagers[]  = { PagerNone,   PageFromLoader, PageFromMapper, PageAutoCommit };
// memory type based on pager
static DWORD    g_vmMemType[] = { MEM_PRIVATE, MEM_IMAGE,      MEM_MAPPED,     MEM_PRIVATE    };

static DWORD g_vaDllFree        = VM_DLL_BASE;              // lowest free VA for DLL allocaiton
static DWORD g_vaSharedHeapFree = VM_SHARED_HEAP_BASE;      // lowest free VA for shared heap
static DWORD g_vaRAMMapFree     = VM_RAM_MAP_BASE;          // lowest free VA for RAM-backed memory-mappings

// total number of stack cached
static LONG g_nStackCached;
static PSTKLIST g_pStkList;

const DWORD idxKernelSearchLimit = VA2PDIDX (VM_CPU_SPECIFIC_BASE);
const DWORD idxUserSearchLimit   = VA2PDIDX (VM_KMODE_BASE);

static DWORD g_aslrSeed;
BOOL  g_fAslrEnabled;
#define ASLR_MASK           0x3ff0000   // between 0 <--> 64M-64K, 64K aligned

//-----------------------------------------------------------------------------------------------------------------
//
//  NextAslrBase: Get the next ASLR base
//
__inline DWORD NextAslrBase (void)
{
    DWORD dwRet = 0;
    if (g_fAslrEnabled) {
        g_aslrSeed = ((g_aslrSeed << 5) | (g_aslrSeed >> 27)) ^ randdw1 ^ randdw2;
        dwRet = g_aslrSeed & ASLR_MASK;
    }
    return dwRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  IsValidAllocType: check if allocation type is valid
//
__inline BOOL IsValidAllocType (DWORD dwAddr, DWORD fAllocType)
{
    fAllocType &= ~MEM_TOP_DOWN;        // MEM_TOP_DOWN is ignored
    return fAllocType && !(fAllocType & ~VM_VALID_ALLOC_TYPE);
}


//-----------------------------------------------------------------------------------------------------------------
//
//  IsValidFreeType: check if free type is valid
//
__inline BOOL IsValidFreeType (DWORD dwAddr, DWORD cbSize, DWORD dwFreeType)
{
    return (MEM_DECOMMIT == dwFreeType)
        || ((MEM_RELEASE == dwFreeType) && !cbSize && !(dwAddr & VM_BLOCK_OFST_MASK));
}

//-----------------------------------------------------------------------------------------------------------------
//
//  IsValidLockOpt: check if lock option is valid
//
__inline BOOL IsValidLockOpt (DWORD fOption)
{
    return !(fOption & ~(VM_LF_QUERY_ONLY | VM_LF_READ | VM_LF_WRITE));
}


//-----------------------------------------------------------------------------------------------------------------
//
// MemTypeFromAddr - get memory type based on address
//
static DWORD MemTypeFromAddr (PPROCESS pprc, DWORD dwAddr)
{
    // There is not enough information to get the original reservation type for
    // committed pages, so just always return MEM_PRIVATE.
    return MEM_PRIVATE;
}

//-----------------------------------------------------------------------------------------------------------------
//
// MemTypeFromReservation - get memory type based on reservation
//
__inline DWORD MemTypeFromReservation (DWORD dwEntry)
{
    return g_vmMemType[GetPagerType (dwEntry)];
}


//-----------------------------------------------------------------------------------------------------------------
//
//  EntryFromReserveType: return entry, based on resevation type
//
static BOOL EntryFromReserveType (DWORD fReserveType)
{
    DWORD dwPagerType;
    if (fReserveType & MEM_AUTO_COMMIT) {
        dwPagerType = VM_PAGER_AUTO;
    } else if (fReserveType & MEM_IMAGE) {
        dwPagerType = VM_PAGER_LOADER;
    } else if (fReserveType & MEM_MAPPED) {
        dwPagerType = VM_PAGER_MAPPER;
    } else {
        dwPagerType = VM_PAGER_NONE;
    }
    return MakeReservedEntry (dwPagerType);
}

//-----------------------------------------------------------------------------------------------------------------
//
// FindVAItem - find a matching node in given va list
// Return matching node
//
static PVALIST FindVAItem (PVALIST pListHead, DWORD dwAddr)
{
    PVALIST pTrav = pListHead;
    while (pTrav) {
        if (dwAddr - (DWORD) pTrav->pVaBase < pTrav->cbVaSize) {
            break;
        }
        pTrav = pTrav->pNext;
    }
    return pTrav;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMFindAlloc - find a matching node in the process va list
// Return tag of matching va node
//
BOOL VMFindAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, PDWORD pdwItemTag)
{
    BOOL fRet = FALSE;    
    DEBUGCHK ((int)cbSize >= 0);
    if (((pprc == g_pprcNK) || ((DWORD) pAddr < dwMaxUserAllocAddr))    // valid address?
        && ((int) cbSize >= 0)                                          // valid size?
        && LockVM (pprc)) {

        PVALIST pVaItem = FindVAItem (pprc->pVaList, (DWORD) pAddr);

        fRet = pVaItem
            && ((DWORD) pAddr - (DWORD) pVaItem->pVaBase + cbSize <= pVaItem->cbVaSize);

        if (fRet && pdwItemTag) {
            *pdwItemTag = pVaItem->dwTag;
        }

        UnlockVM (pprc);        
    }
    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// RemoveVAItem - remove a matching node in the given list
// Return matching node
//
static PVALIST RemoveVAItem (PVALIST *ppHead, LPVOID pAddr, DWORD dwTag)
{
    // find matching node
    PVALIST *ppTrav, pTrav = NULL;        
    for (ppTrav = ppHead; NULL != (pTrav = *ppTrav); ppTrav = &pTrav->pNext) {            
        if (pAddr == pTrav->pVaBase) {
            if (dwTag == pTrav->dwTag) {
                *ppTrav = pTrav->pNext;
            } else {
                pTrav = NULL;
            }
            break;
        }
    }

    return pTrav;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMRemoveAlloc - remove a matching node in process va list and free the virtual memory
//
BOOL VMRemoveAlloc (PPROCESS pprc, LPVOID pAddr, DWORD dwTag)
{
    PVALIST pTrav = NULL;
    
    if (((pprc == g_pprcNK) || ((DWORD) pAddr < dwMaxUserAllocAddr))    // valid address?
        && LockVM (pprc)) {

        // remove matching node
        pTrav = RemoveVAItem (&pprc->pVaList, pAddr, dwTag);
        if (pTrav) {
#ifdef ARM
            PVALIST pUncachedItem = RemoveVAItem (&pprc->pUncachedList, pAddr, 0);
            if (pUncachedItem) {
                FreeMem (pUncachedItem, HEAP_VALIST);
            }
#endif
        }

        UnlockVM (pprc);
        
        if (pTrav) {
            VERIFY (VMFreeAndRelease (pprc, pAddr, pTrav->cbVaSize));            
#ifdef ARM
            VMUnalias (pTrav->pAlias);
#endif
            FreeMem (pTrav, HEAP_VALIST);
        }
    }
    
    return (pTrav != NULL);
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeAllocList - free all entries in process va list
//
void VMFreeAllocList (PPROCESS pprc)
{
    if (LockVM (pprc)) {
        PVALIST pVaList;
        PVALIST pVaItem;
#ifdef ARM
        PVALIST pUncachedList;
#endif

        pprc->bState = PROCESS_STATE_VM_CLEARED;
        pVaList = pprc->pVaList;
        pprc->pVaList = NULL;
#ifdef ARM
        pUncachedList = pprc->pUncachedList;
        pprc->pUncachedList = NULL;
#endif
        UnlockVM (pprc);

        while (NULL != (pVaItem = pVaList)) {
            pVaList = pVaItem->pNext;
#ifdef ARM
            // for ARM, we need to call VMFreeAndRelease to decrement physical ref-count before
            // calling VMUnalias, or the ref-count will be > 1 and we won't be able to turn cache
            // back on.
            VERIFY (VMFreeAndRelease (pprc, pVaItem->pVaBase, pVaItem->cbVaSize));
            VMUnalias (pVaItem->pAlias);
#endif
            FreeMem (pVaItem, HEAP_VALIST);
        }

#ifdef ARM
        while (NULL != (pVaItem = pUncachedList)) {
            pUncachedList = pVaItem->pNext;
            FreeMem (pVaItem, HEAP_VALIST);
        }
#endif
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMAddAlloc - add a node to process va list
// Return system tag if pdwTag and *pdwTag == 0
//
BOOL VMAddAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, PALIAS pAlias, PDWORD pdwTag)
{
    PVALIST pVaItem = AllocMem (HEAP_VALIST);
    BOOL fRet = FALSE;

    if (pVaItem) {

#ifdef VM_DEBUG
#ifdef x86
        if (g_pfnKrnRtlDispExcp) {
            THRDGetCallStack (pCurThread, _countof(pVaItem->callstack), &pVaItem->callstack[0], 0, 2);
        }
#endif        
#endif
        pVaItem->pVaBase  = pAddr;
        pVaItem->cbVaSize = cbSize;
        pVaItem->pAlias   = pAlias;
        if (LockVM (pprc)) {
            DEBUGCHK (IsVMAllocationAllowed (pprc));
            if (IsVMAllocationAllowed (pprc)) {
                DWORD dwTag = 0;
                fRet = TRUE;                
                if (pdwTag) {
                    __try {
                        dwTag = *pdwTag;
                        if (!dwTag) {
                            dwTag = *pdwTag = ((++pprc->nCurSysTag) | 0x80000000);
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        fRet = FALSE;
                    }
                }
                if (fRet) {
                    pVaItem->dwTag    = dwTag;
                    pVaItem->pNext    = pprc->pVaList;
                    pprc->pVaList     = pVaItem;
                }
            }
            UnlockVM (pprc);
        }

        if (!fRet) {
            FreeMem (pVaItem, HEAP_VALIST);
        }
    }
    
    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  Enumerate2ndPT: Enumerate 2nd-level page table, pass pointer to 2nd level page table entry to
//                  the 'apply' function. Enumeration stop right away and returns false if the apply function
//                  returns false. Or returns ture if it finishes enumerate cPages
//
BOOL Enumerate2ndPT (PPAGEDIRECTORY ppdir, DWORD dwAddr, DWORD cPages, BOOL fWrite, PFN_PTEnumFunc pfnApply, LPVOID pEnumData)
{
    DWORD       idxdir = VA2PDIDX (dwAddr);   // 1st-level PT index
    PPAGETABLE  pptbl  = GetPageTable (ppdir, idxdir);
    DWORD       cpLeft = cPages;

    DEBUGCHK (dwAddr);
    DEBUGCHK ((int) cPages > 0);
    DEBUGCHK (pfnApply);
    DEBUGCHK (ppdir);

    if (pptbl) {

#ifdef ARM
        DWORD idxPteChanged = VM_NUM_PT_ENTRIES;
#endif
        DWORD idx2nd = VA2PT2ND (dwAddr);   // 2nd-level PT index
        DWORD dwEntry;

        for ( ; ; ) {

            DEBUGMSG (ZONE_VIRTMEM, (L"Enumerate2ndPT: idxdir = %8.8lx, idx2nd = %8.8lx, ppdir->pte[idxdir] = %8.8lx\r\n",
                idxdir, idx2nd, ppdir->pte[idxdir]));
            PREFAST_ASSUME (idx2nd < VM_NUM_PT_ENTRIES);
            dwEntry = pptbl->pte[idx2nd];
            
            if (fWrite                                                  // asking for write permission
                && IsPageCommitted (dwEntry)                            // already commited
                && g_pOemGlobal->pfnIsRom (PA256FromPfn (PFNfromEntry (dwEntry)))) {     // not writeable
                // can't do R/W on ROM
                break;
            }

            // apply the function to a 2nd level page table entry
            if (!pfnApply (&pptbl->pte[idx2nd], pEnumData)) {
                break;
            }
            DEBUGMSG (ZONE_VIRTMEM, (L"Enumerate2ndPT: &pptbl->pte[idx2nd] = %8.8lx, pptbl->pte[idx2nd] = %8.8lx\r\n", 
                &pptbl->pte[idx2nd], pptbl->pte[idx2nd]));

#ifdef ARM
            if ((dwEntry != pptbl->pte[idx2nd])                         // PTE updated
                && (VM_NUM_PT_ENTRIES == idxPteChanged)                 // 1st change in this table?
                && ((dwEntry | pptbl->pte[idx2nd]) & PG_VALID_MASK)) {  // at least one of old/new value is valid
                // page table entry changed. record the index
                idxPteChanged = idx2nd;
            }
#endif            
            // move to next index
            idx2nd ++;

            // done? 
            if (! -- cpLeft) {
                break;
            }

            // if we're at the end of the PT, move to the next
            if (VM_NUM_PT_ENTRIES == idx2nd) {

#ifdef ARM
                if (idxPteChanged < idx2nd) {
                    ARMPteUpdateBarrier (&pptbl->pte[idxPteChanged], idx2nd - idxPteChanged);
                    idxPteChanged = VM_NUM_PT_ENTRIES;
                }
#endif

                idxdir = NextPDEntry (idxdir);
                if (VM_NUM_PD_ENTRIES == idxdir) {
                    // end of 1st level page table, shouldn't get here for
                    // we should've encounter "end of reservation".
                    DEBUGCHK (0);
                    break;
                }
                pptbl = GetPageTable (ppdir, idxdir);
                if (!pptbl) {
                    // completely un-touched directory (no page-table), fail the enumeration.
                    break;
                }
                // continue from the beginning of the new PT
                idx2nd = 0;
            }

        }
#ifdef ARM
        if (idxPteChanged < idx2nd) {
            DEBUGCHK (pptbl);
            ARMPteUpdateBarrier (&pptbl->pte[idxPteChanged], idx2nd - idxPteChanged);
        }
#endif
    }

    return !cpLeft;
}



//-----------------------------------------------------------------------------------------------------------------
//
//  CountFreePagesInSection: given a 1st-level PT entry, count the number of free pages in this section,
//                           starting from page of index 'idx2nd'.
//  returns: # of free pages
//
static DWORD CountFreePagesInSection (PPAGEDIRECTORY ppdir, DWORD idxPD, DWORD idx2nd)
{
    DWORD cPages = 0;
    PPAGETABLE pptbl = GetPageTable (ppdir, idxPD);

    DEBUGCHK (!(idx2nd & 0xf));     // idx2nd must be multiple of VM_PAGES_PER_BLOCK

    if (pptbl) {

        // 2nd level page table exist, scan the free pages
        cPages = 0;
        while ((idx2nd < VM_NUM_PT_ENTRIES) && (VM_FREE_PAGE == pptbl->pte[idx2nd])) {
            idx2nd += VM_PAGES_PER_BLOCK;
            cPages += VM_PAGES_PER_BLOCK;
        }
    } else if (!IsSectionMapped (ppdir->pte[idxPD])) {

        // 1st level page table entry is 0, the full 4M section is free
        cPages = VM_NUM_PT_ENTRIES - idx2nd;
        
    }

    return cPages;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  CountFreePages: count the number of free pages starting from dwAddr, stop if exceeds cPages.
//  returns: # of free pages in block granularity
//
static DWORD CountFreePages (PPAGEDIRECTORY ppdir, DWORD dwAddr, DWORD cPages)
{
    DWORD idxdir = VA2PDIDX (dwAddr);  // 1st-level PT index
    DWORD idx2nd = VA2PT2ND (dwAddr);  // 2nd-level PT index
    DWORD idxLimit = IsKModeAddr (dwAddr)? idxKernelSearchLimit : idxUserSearchLimit;

    DWORD nTotalFreePages, nCurFreePages;

    DEBUGCHK (ppdir);
    DEBUGCHK (!(dwAddr & (VM_BLOCK_OFST_MASK)));   // must be 64K aligned
    DEBUGCHK (dwAddr);
    DEBUGCHK ((int) cPages > 0);

    for (nTotalFreePages = 0; idxdir < idxLimit; idxdir = NextPDEntry (idxdir), idx2nd = 0) {
        nCurFreePages = CountFreePagesInSection (ppdir, idxdir, idx2nd);
        nTotalFreePages += nCurFreePages;

        if ((nTotalFreePages >= cPages)                             // got enough page?
            || (VM_NUM_PT_ENTRIES - idx2nd != nCurFreePages)) {     // can we span to the next 'section'?
            break;
        }
    }
    
    return nTotalFreePages;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  CountUsedPages: count the number of used pages starting from dwAddr.
//  returns: # of used pages in block granularity (e.g. return 16 if there are 10 used pages)
//
static DWORD CountUsedPages (PPAGEDIRECTORY ppdir, DWORD dwAddr)
{
    DWORD idxdir = VA2PDIDX (dwAddr);  // 1st-level PT index
    DWORD idx2nd = VA2PT2ND (dwAddr);  // 2nd-level PT index
    DWORD idxLimit = IsKModeAddr (dwAddr)? idxKernelSearchLimit : idxUserSearchLimit;
    PPAGETABLE pptbl;
    DWORD cPages;

    DEBUGCHK (!(dwAddr & (VM_BLOCK_OFST_MASK)));   // must be 64K aligned
    DEBUGCHK (dwAddr);
    DEBUGCHK (ppdir);

    // iterate through the 1st level page table entries
    for (cPages = 0; (idxdir < idxLimit) && ppdir->pte[idxdir]; idxdir = NextPDEntry (idxdir), idx2nd = 0) {

        pptbl = GetPageTable (ppdir, idxdir);
        if (pptbl) {

            // iterate through the 2nd level page tables, 16 page at a time
            while ((idx2nd < VM_NUM_PT_ENTRIES) && (VM_FREE_PAGE != pptbl->pte[idx2nd])) {
                idx2nd += VM_PAGES_PER_BLOCK;
                cPages += VM_PAGES_PER_BLOCK;
            }
            // check if this is the end of reservation
            if ((idx2nd < VM_NUM_PT_ENTRIES) || !pptbl->pte[VM_NUM_PT_ENTRIES-1]) {
                break;
            }
        } else {
            DEBUGCHK (IsSectionMapped (ppdir->pte[idxdir]));
            cPages += VM_NUM_PT_ENTRIES - idx2nd;
        }

    }

    return cPages;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  SearchFreePages: search from dwAddr till dwLimit for cPages of consecutive free pages
//
static DWORD SearchFreePages (PPAGEDIRECTORY ppdir, LPDWORD pdwBase, DWORD dwLimit, DWORD cPages)
{
    DWORD cpFound = 0;
    DWORD dwAddr  = *pdwBase;

    DEBUGCHK (dwAddr);
    DEBUGCHK ((int) cPages > 0);
    DEBUGMSG (ZONE_VIRTMEM, (L"SearchFreePages: Search free pages from 0x%8.8lx for 0x%x pages, limit = 0x%8.8lx\r\n", dwAddr, cPages, dwLimit));

    if (dwAddr < dwLimit) {

        while ((dwAddr + (cPages << VM_PAGE_SHIFT)) <= dwLimit) {
            
            // count # of free pages starting from dwAddr
            cpFound = CountFreePages (ppdir, dwAddr, cPages);

            if (cpFound >= cPages) {
                // got enough pages
                break;
            }

            // skip all used pages and the found free pages that can't satisfy the request
            dwAddr += cpFound << VM_PAGE_SHIFT;
            dwAddr += CountUsedPages (ppdir, dwAddr) << VM_PAGE_SHIFT;

            if (dwAddr >= dwLimit) {
                break;
            }
            
            if (!cpFound) {
                // this can only happen when *pdwBase is at an allocated address. Here's how it can happen
                //      | used | free | used |
                // if an allocation take the "free" slot, the "base free" address will be advanced to the 
                // next block, which is in use.
                *pdwBase = dwAddr;
            }
        }

    }

    DEBUGMSG (ZONE_VIRTMEM, (L"SearchFreePages: returns 0x%8.8lx\r\n", (cpFound >= cPages)? dwAddr : 0));
    return (cpFound >= cPages)? dwAddr : 0;

}

//-----------------------------------------------------------------------------------------------------------------
//
//  CountPagesNeeded: 2nd level page table enumeration funciton, count uncommitted pages
//
static BOOL CountPagesNeeded (const DWORD *pdwEntry, LPVOID pEnumData)
{
    LPDWORD pcPagesNeeded = (LPDWORD) pEnumData;
    
    if (VM_FREE_PAGE == *pdwEntry) {
        return FALSE;
    }
    
    // update page needed if not committed
    if (!IsPageCommitted (*pdwEntry)) {
        // not commited
        *pcPagesNeeded += 1;
    }
    return TRUE;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  VMScan: scan VM from dwAddr for cPages, count # of uncommitted pages
//  returns: dwAddr if cPages were found in the same VM reservation
//           0 if there are < cPages in the same VM reservation
//
static DWORD VMScan (PPROCESS pprc, DWORD dwAddr, DWORD cPages, LPDWORD pcPagesNeeded, BOOL fWrite)
{
    DEBUGCHK (dwAddr);                          // dwAddr cannot be NULL
    DEBUGCHK ((int) cPages > 0);                // at least 1 page
    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));   // dwAddr must be page aligned

    *pcPagesNeeded = 0;

    return Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, fWrite, CountPagesNeeded, pcPagesNeeded)
        ? dwAddr
        : 0;

}

#define VM_MAP_LIMIT        0xFFFE0000

//-----------------------------------------------------------------------------------------------------------------
//
//  DoVMReserve: reserve VM
//
static DWORD DoVMReserve (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD dwSearchBase, DWORD fAllocType)
{
    PPAGEDIRECTORY  ppdir = pprc->ppdir;      // 1st level page table
    LPDWORD  pdwBaseFree = &pprc->vaFree;
    DEBUGCHK ((int) cPages > 0);
    DEBUGCHK (ppdir);

    DEBUGCHK (!dwSearchBase || !dwAddr);
    DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - process-id: %8.8lx, dwAddr = %8.8lx, cPages = 0x%x, base = %8.8lx, fAllocType = %8.8lx\r\n",
        pprc->dwId, dwAddr, cPages, dwSearchBase, fAllocType));
    
    if (dwAddr) {

        if (dwAddr & VM_BLOCK_OFST_MASK) {
            // reservation must be 64k aligned
            DEBUGMSG (TRUE, (L"!!!! dwAddr = %8.8lx is not 64k aligned for process %8.8lx !!!!\r\n", dwAddr, pprc->dwId));

            dwAddr = 0;

        } else {

            if (dwAddr >= VM_SHARED_HEAP_BASE) {
                ppdir = g_ppdirNK;
                
            } else if ((ppdir == g_ppdirNK)
                && (dwAddr >= VM_DLL_BASE)
                && (dwAddr <  VM_RAM_MAP_BASE)) {
                // reserving user mode DLL address
                pdwBaseFree = &g_vaDllFree;
            }

            // specific address given, verify the range is free
            if (CountFreePages (ppdir, dwAddr, cPages) < cPages) {
                dwAddr = 0;
            }
        }
        
    } else {
        // no range given, find whatever fit
        DWORD dwLimit;
    
        switch (dwSearchBase) {
        case VM_DLL_BASE:
            pdwBaseFree = &g_vaDllFree;
            dwLimit = VM_RAM_MAP_BASE;
            break;
        case VM_RAM_MAP_BASE:
            pdwBaseFree = &g_vaRAMMapFree;
            dwLimit = VM_USER_LIMIT;
            break;
        case VM_SHARED_HEAP_BASE:
            pdwBaseFree = &g_vaSharedHeapFree;
            dwLimit = VM_SHARED_HEAP_BOUND;
            ppdir = g_ppdirNK;
            break;
        default:
            dwLimit = (IsKModeAddr (*pdwBaseFree) ? VM_CPU_SPECIFIC_BASE : VM_DLL_BASE);
            break;
        }

        DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - searching from va = %8.8lx\r\n", *pdwBaseFree));
        
        dwAddr = SearchFreePages (ppdir, pdwBaseFree, dwLimit, cPages);

        // ASLR support - base of all user mode regions can be between 64K - 64M
        // i.e. we can be searching from the middle of the VM. If we fail to find a page, 
        // search again from the start.
        if (!dwAddr && !IsKModeAddr (dwLimit)) {
            dwLimit = *pdwBaseFree;
            *pdwBaseFree = dwSearchBase? dwSearchBase : VM_USER_BASE;
            dwAddr = SearchFreePages (ppdir, pdwBaseFree, dwLimit, cPages);
        }

        DEBUGMSG (!dwAddr, (L"!!!! Out of VM for process %8.8lx !!!!\r\n", pprc->dwId));
        DEBUGCHK (dwAddr);
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - use Addr %8.8lx\r\n", dwAddr));

    if (dwAddr) {
        // found enough free pages to reserve
        PPAGETABLE  pptbl;                        // 2nd level page table
        DWORD dwAddrEnd = dwAddr + (cPages << VM_PAGE_SHIFT) - 1;   // end address
        DWORD idxdirEnd = VA2PDIDX (dwAddrEnd);     // end idx for 1st level PT
        DWORD idx;

        // allocate 2nd level page tables if needed
        for (idx = VA2PDIDX (dwAddr); idx <= idxdirEnd; idx = NextPDEntry (idx)) {
            if (!ppdir->pte[idx]) {
                DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - allocating 2nd-level page table\r\n"));
                pptbl = AllocatePTBL (ppdir, idx);
                if (!pptbl) {
                    // out of memory
                    dwAddr = 0;
                    break;
                }
                DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - 2nd-level page table at %8.8lx\r\n", pptbl));
            }
        }

        if (dwAddr) {
            
            int   idx2nd  = VA2PT2ND (dwAddrEnd);               // idx to 2nd level PT
            DWORD dwEntry = EntryFromReserveType (fAllocType);  // entry value for the reserved page
            
            pptbl = GetPageTable (ppdir, idxdirEnd);
            DEBUGCHK (pptbl);

            DEBUGMSG (ZONE_VIRTMEM, (L"last page table at %8.8lx\r\n", pptbl));

            pptbl->pte[idx2nd] = dwEntry;
//            DEBUGMSG (ZONE_VIRTMEM, (L"pptbl->pte[0x%x] (addr %8.8lx) set to %8.8lx\r\n", idx2nd, &pptbl->pte[idx2nd], dwEntry));

            // walking backward for to update the entries
            while (--cPages) {

                if (--idx2nd < 0) {
                    idxdirEnd = PrevPDEntry (idxdirEnd);
                    pptbl = GetPageTable (ppdir, idxdirEnd);
                    idx2nd  = VM_NUM_PT_ENTRIES - 1;
                    DEBUGMSG (ZONE_VIRTMEM, (L"move to previous page table at %8.8lx\r\n", pptbl));
                    
                }
                pptbl->pte[idx2nd] = dwEntry;
//                DEBUGMSG (ZONE_VIRTMEM, (L"pptbl->pte[0x%x] (addr %8.8lx) set to %8.8lx\r\n", idx2nd, &pptbl->pte[idx2nd], dwEntry));
            }

            // update pprc->vaFree if necessary
            if (dwAddr == *pdwBaseFree) {
                // round up to the next block
                *pdwBaseFree = (dwAddrEnd + VM_BLOCK_SIZE- 1) & -VM_BLOCK_SIZE;
            }
        }
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"DoVMReserve - returns %8.8lx\r\n", dwAddr));
    return dwAddr;
}


typedef struct {
    DWORD dwAddr;
    DWORD dwPgProt;
    DWORD dwPageType;
    BOOL  fFlush;
} VMCommitStruct, *PVMCommitStruct;

//-----------------------------------------------------------------------------------------------------------------
//
//  CommitPages: 2nd level page table enumeration funciton, uncommitted all uncommitted pages
//  NOTE: the required number of pages had been held prior to the enumeration
//
static BOOL CommitPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVMCommitStruct pvs     = (PVMCommitStruct) pEnumData;
    DWORD           flag    = pvs->dwPageType;
    DWORD           dwEntry = *pdwEntry;
    
    DWORD           dwPFN   = IsPageCommitted (dwEntry)
                            ? PFNfromEntry (dwEntry)
                            : GetHeldPage (&flag);
    DWORD           dwNewEntry;
    
    DEBUGCHK (!(PM_PT_NEEDZERO & pvs->dwPageType));

    dwNewEntry = MakeCommittedEntry (dwPFN, pvs->dwPgProt);

    if (flag & PM_PT_NEEDZERO) {
        //
        // we need to zero the page before return. In order to do that, we need to
        // commit the page to a writable page first.
        // 
        // NOTE: this must be a newly committed page
        BOOL fWriteable = IsPageWritable (dwNewEntry);

        // temporary make the PTE writeable, such that we can zero the page
        *pdwEntry = fWriteable
                  ? dwNewEntry
                  : MakeWritableEntry (g_pprcNK, dwPFN, pvs->dwAddr);

#ifdef ARM
        ARMPteUpdateBarrier (pdwEntry, 1);
#endif
        
        // zero the page
        ZeroPage ((LPVOID) pvs->dwAddr);

        if (!fWriteable) {
            // writeback the zero'd page
            NKCacheRangeFlush ((LPVOID) pvs->dwAddr, VM_PAGE_SIZE, CACHE_SYNC_WRITEBACK);

            // restore PTE - Update barrier will be issued after returned to Enumerate2ndPT
            *pdwEntry = dwNewEntry;
        }

    } else {
        *pdwEntry = dwNewEntry;
    }

    pvs->fFlush |= !IsSameEntryType (dwEntry, dwNewEntry);
    pvs->dwAddr += VM_PAGE_SIZE;
    
    return TRUE;

}

BOOL PHYS_UpdateFreePageCount (int incr);

FORCEINLINE void ReleaseHeldPages (int cPages)
{
    PHYS_UpdateFreePageCount (cPages);
}

//-----------------------------------------------------------------------------------------------------------------
//
// DoVMAlloc: worker function to allocate VM. 
//
static LPVOID 
DoVMAlloc(
    PPROCESS pprc,          // the process where VM is to be allocated  
    DWORD dwAddr,           // starting address
    DWORD cPages,           // # of pages
    DWORD fAllocType,       // allocation type
    DWORD fProtect,         // protection
    DWORD dwPageType,       // when committing, what type of page to use
    LPDWORD pdwErr          // error code if failed
    )
{
    DWORD dwRet = 0;
    BOOL  fHeld = FALSE;

    DEBUGCHK (cPages && !*pdwErr);
    
    // if dwAddr == 0, must reserve first
    if (!dwAddr) {
        fAllocType |= MEM_RESERVE;
    }

    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));
    DEBUGMSG (ZONE_VIRTMEM, (L"DoVMAlloc - proc-id: %8.8lx, dwAddr = %8.8lx, cPages = 0x%x, fAllocType = %8.8lx, dwPageType = %8.8lx\r\n",
        pprc->dwId, dwAddr, cPages, fAllocType, dwPageType));

    if (MEM_COMMIT & fAllocType) {
        fHeld = HoldPagesWithWait ((int)cPages);
        if (!fHeld) {
            // we don't have enough pages to satisfy this request; fail the call.
            *pdwErr = ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    DEBUGCHK (!fHeld || !*pdwErr);

    if (!*pdwErr) {
        
        DWORD cPageNeeded = cPages;

        if (LockVM (pprc)) {

            dwRet = (fAllocType & MEM_RESERVE)
                ? DoVMReserve (pprc, dwAddr, cPages, 0, fAllocType)         // reserving VM
                : VMScan (pprc, dwAddr, cPages, &cPageNeeded, fProtect & VM_READWRITE_PROT);    // not reserving, must be commiting. Check if
                                                                            // the range to be commited is from a single reservation.
                                                                            // count #of page needed to commit

            if (!dwRet) {
                *pdwErr = dwAddr? ERROR_INVALID_PARAMETER : ERROR_NOT_ENOUGH_MEMORY;
                if (ERROR_NOT_ENOUGH_MEMORY == *pdwErr) {
                    UpdateSQMMarkers (SQM_MARKER_VMOOMCOUNT, +1);
                }

            } else if (fAllocType & MEM_COMMIT) {

                PPROCESS pprcVM = pprc;
                VMCommitStruct vs = { dwRet, PageParamFormProtect (pprc, fProtect, dwRet), dwPageType, FALSE };

                DEBUGCHK (fHeld);
                PREFAST_ASSUME (cPages - cPageNeeded <= cPages);

                if (cPageNeeded < cPages) {
                    ReleaseHeldPages (cPages - cPageNeeded);
                }
            
                // user mode address - must operate on the correct VM
                if ((dwRet < VM_SHARED_HEAP_BASE) && (pprc != pVMProc)) {
                    pprcVM = SwitchVM (pprc);
                }
                
                // got enough pages, update entries
                Enumerate2ndPT (pprc->ppdir, dwRet, cPages, 0, CommitPages, &vs);

                if (vs.fFlush) {
                    InvalidatePages (pprc, dwRet, cPages);
                    if (PAGE_NOCACHE & fProtect) {
#ifdef ARM
                        // flush everything if flush with uncached VA is not supported.
                        if (!IsFlushUncachedVASupported()) {
                            NKCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);
                        }
                        else
#endif
                        {
                            NKCacheRangeFlush ((LPVOID) dwRet, cPages << VM_PAGE_SHIFT, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);
                        }
                    }
                }

                // switch VM back if changed
                if (pprcVM != pprc) {
                    SwitchVM (pprcVM);
                }
            }

            UnlockVM (pprc);

        } else {
            *pdwErr = ERROR_INVALID_PARAMETER;
        }

        if (*pdwErr) {

            DEBUGCHK (!dwRet);

            if (fHeld) {
                ReleaseHeldPages (cPages);
            }
        }
        
        CELOG_VirtualAlloc(pprc, dwRet, dwAddr, cPages, fAllocType, fProtect);
            
    }
    
    DEBUGMSG (ZONE_VIRTMEM||!dwRet, (L"DoVMAlloc - returns %8.8lx\r\n", dwRet));

    return (LPVOID) dwRet;
}

typedef struct {
    LPDWORD pPages;
    DWORD   dwPgProt;
    BOOL    fPhys;
    DWORD   dwPagerType;
} VSPStruct, *PVSPStruct;

static BOOL EnumSetPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVSPStruct pvsps = (PVSPStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    DWORD dwPFN = pvsps->pPages[0];

    if (!dwEntry || IsPageCommitted (dwEntry)) {
        return FALSE;
    }
    if (!pvsps->fPhys) {
        dwPFN = GetPFN ((LPVOID)dwPFN);
    }
    if (!DupPhysPage (dwPFN)) {
        return FALSE;
    }
    pvsps->dwPagerType = GetPagerType (dwEntry);
    pvsps->pPages ++;
    *pdwEntry = MakeCommittedEntry (dwPFN, pvsps->dwPgProt);
    return TRUE;
}

//
// VMSetPages: (internal function) map the address from dwAddr of cPages pages, to the pages specified
//             in array pointed by pPages. The pages are treated as physical page number if PAGE_PHYSICAL
//             is specified in fProtect. Otherwise, it must be page aligned
//
BOOL VMSetPages (PPROCESS pprc, DWORD dwAddr, LPDWORD pPages, DWORD cPages, DWORD fProtect)
{
    PPAGEDIRECTORY ppdir = LockVM (pprc);
    BOOL fPhys = fProtect & PAGE_PHYSICAL;
    BOOL fRet  = (ppdir != NULL);
    fProtect &= ~PAGE_PHYSICAL;

    DEBUGCHK (pPages && cPages && IsValidProtect (fProtect) && !(VM_PAGE_OFST_MASK & dwAddr));
    
    if (fRet) {
        VSPStruct vsps = { pPages, PageParamFormProtect (pprc, fProtect, dwAddr), fPhys, 0 };

        // Calling DupPhysPage requires holding PhysCS
        LockPhysMem ();
        fRet = Enumerate2ndPT (ppdir, dwAddr, cPages, FALSE, EnumSetPages, &vsps);
        UnlockPhysMem ();

        UnlockVM (pprc);
        
        // on error, decommit the pages we committed
        if (!fRet && (0 != (cPages = vsps.pPages - pPages))) {
            VERIFY (VMDecommit (pprc, (LPVOID) dwAddr, cPages << VM_PAGE_SHIFT, vsps.dwPagerType));
        }

    }

    return fRet;
}


typedef struct _VCPhysStruct {
    DWORD dwPfn;        // current phys page number
    DWORD dwPgProt;      // page attributes
    BOOL  fIncRef;
} VCPhysStruct, *PVCPhysStruct;

//-----------------------------------------------------------------------------------------------------------------
//
//  VCPhysPages: 2nd level page table enumeration funciton, VirtualCopy physical pages
//
static BOOL VCPhysPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVCPhysStruct pvcps = (PVCPhysStruct) pEnumData;
    BOOL fRet = (VM_FREE_PAGE != *pdwEntry)                         // must be reserved
             && !IsPageCommitted (*pdwEntry)                        // cannot be committed
             && (!pvcps->fIncRef || DupPhysPage (pvcps->dwPfn));    // refcount can be incremented

    if (fRet) {
        *pdwEntry = MakeCommittedEntry (pvcps->dwPfn, pvcps->dwPgProt);
        pvcps->dwPfn = NextPFN (pvcps->dwPfn);
    }
    return fRet;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMCopyPhysical: VirtualCopy from physical pages. 
//
BOOL
VMCopyPhysical(
    PPROCESS pprc,              // the destination process
    DWORD dwAddr,               // destination address
    DWORD dwPfn,                // physical page number
    DWORD cPages,               // # of pages
    DWORD dwPgProt,             // protection
    BOOL  fIncRef
    )
{
    BOOL  fRet = FALSE;
    DEBUGCHK (dwAddr);
    DEBUGCHK ((int) cPages > 0);
    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));

    DEBUGMSG (ZONE_VIRTMEM, (L"VMCopyPhysical - process id: %8.8lx, dwAddr %8.8lx, dwPfn = %8.8lx cPages = 0x%x\r\n",
        pprc->dwId, dwAddr, dwPfn, cPages));
    
    if (LockVM (pprc)) {

        VCPhysStruct vcps = { dwPfn, dwPgProt, fIncRef };

        // Calling DupPhysPage requires holding PhysCS
        if (fIncRef) {
            LockPhysMem ();
        }
        fRet = Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, 0, VCPhysPages, &vcps);
        if (fIncRef) {
            UnlockPhysMem ();
        }
        
        UnlockVM (pprc);
    }
    
    DEBUGMSG (ZONE_VIRTMEM, (L"VMCopyPhysical returns 0x%x\r\n", fRet));

    return fRet;
}

typedef struct _VCVirtStruct {
    PPAGEDIRECTORY ppdir;   // 1st level page table
    PPAGETABLE pptbl;       // 2nd level page table
    DWORD idxdir;           // index to 1st level pt
    DWORD idx2nd;           // index to 2nd level pt
    DWORD dwPgProt;         // page attributes
    DWORD dwPagerType;
} VCVirtStruct, *PVCVirtStruct;

//-----------------------------------------------------------------------------------------------------------------
//
//  VCVirtPages: 2nd level page table enumeration funciton, VirtualCopy virtual pages
//
static BOOL VCVirtPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    BOOL fRet = !IsPageCommitted (*pdwEntry);

    if (fRet) {
        PVCVirtStruct pvcvs = (PVCVirtStruct) pEnumData;
        DWORD dwPfn = PFNfromEntry (pvcvs->pptbl->pte[pvcvs->idx2nd]);

        pvcvs->dwPagerType = GetPagerType (*pdwEntry);

        fRet = DupPhysPage (dwPfn);
        if (fRet) {

            *pdwEntry = MakeCommittedEntry (dwPfn, pvcvs->dwPgProt);

            if (VM_NUM_PT_ENTRIES == ++ pvcvs->idx2nd) {
                pvcvs->idx2nd = 0;
                pvcvs->idxdir = NextPDEntry (pvcvs->idxdir);
                pvcvs->pptbl  = GetPageTable (pvcvs->ppdir, pvcvs->idxdir);
            }
        }
    }

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMCopyVirtual: VirtualCopy between 2 Virtual addresses
//
static BOOL
VMCopyVirtual(
    PPROCESS pprcDest,      // the destination process
    DWORD dwDestAddr,       // destination address
    PPROCESS pprcSrc,       // the source process
    DWORD dwSrcAddr,        // source address, NULL if PAGE_PHYSICAL
    DWORD cPages,           // # of pages
    DWORD dwPgProt,         // protection
    LPDWORD pdwErr          // error code
    )
{
    DWORD cPageReserved;
    BOOL fRet = FALSE;

    DEBUGCHK (dwDestAddr);
    DEBUGCHK ((int) cPages > 0);
    DEBUGCHK (!(dwDestAddr & VM_PAGE_OFST_MASK));
    DEBUGCHK (!(dwSrcAddr & VM_PAGE_OFST_MASK));

    // default to not valid parameter
    *pdwErr = ERROR_INVALID_PARAMETER;
    
    if (!pprcSrc)
        pprcSrc = pprcDest;

    DEBUGMSG (ZONE_VIRTMEM, (L"VMCopyVirtual - dst process-id: %8.8lx, src process-id: %8.8lx, dwDestAddr %8.8lx, dwSrcAddr = %8.8lx cPages = 0x%x\r\n",
        pprcDest->dwId, pprcSrc->dwId, dwDestAddr, dwSrcAddr, cPages));

    // locking 2 proceses's VM needs to take care of CS ordering, or we might 
    // run into dead lock.
    if (Lock2VM (pprcDest, pprcSrc)) {

        PPAGEDIRECTORY ppdir = pprcSrc->ppdir;
        DWORD         idxdir = VA2PDIDX(dwSrcAddr);
        PPAGETABLE     pptbl = GetPageTable (ppdir, idxdir);
        VCVirtStruct    vcvs = { ppdir, pptbl, idxdir, VA2PT2ND(dwSrcAddr), dwPgProt, 0 };

        // verify parameters - destination, must be all reserved, source - must be all committed
        if (VMScan (pprcDest, dwDestAddr, cPages, &cPageReserved, 0)
            && (cPageReserved == cPages)
            && VMScan (pprcSrc, dwSrcAddr, cPages, &cPageReserved, 0)
            && (0 == cPageReserved)) {

            DEBUGCHK (pptbl);

            LockPhysMem ();
            fRet = Enumerate2ndPT (pprcDest->ppdir, dwDestAddr, cPages, 0, VCVirtPages, &vcvs);
            UnlockPhysMem ();

            *pdwErr = fRet? 0 : ERROR_OUTOFMEMORY;

        }
        DEBUGMSG ((ERROR_OUTOFMEMORY == *pdwErr) || !fRet,
            (L"VMCopyVirtual Failed, cPages = %8.8lx, cpReserved = %8.8lx\r\n", cPages, cPageReserved));

        UnlockVM (pprcDest);
        UnlockVM (pprcSrc);

        // if fail due to OOM, decommit the pages
        if (ERROR_OUTOFMEMORY == *pdwErr) {
            VERIFY (VMDecommit (pprcDest, (LPVOID) dwDestAddr, cPages << VM_PAGE_SHIFT, vcvs.dwPagerType));
        }
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"VMCopyVirtual - returns 0x%x\r\n", fRet));

    return fRet;
}

//
// PageAutoCommit: auto-commit a page.
// NOTE: PageAutoCommit returns PAGEIN_FAILURE FOR ALREADY COMMITTED PAGE.
//       The reason being that it'll have to be a PERMISSION FAULT if
//       that happen.
//
static DWORD PageAutoCommit (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    DWORD dwRet = PAGEIN_RETRY;
    PPAGEDIRECTORY ppdir = LockVM (pprc);

    DEBUGMSG (ZONE_VIRTMEM, (L"Auto-Commiting Page %8.8lx\r\n", dwAddr));
    DEBUGCHK ((pprc == pVMProc) || IsKModeAddr (dwAddr));
    
    if (ppdir) {
        DWORD idxdir = VA2PDIDX (dwAddr);
        DWORD idx2nd = VA2PT2ND (dwAddr);
        PPAGETABLE pptbl = GetPageTable (ppdir, idxdir);

        if (IsPageCommitted (pptbl->pte[idx2nd])) {
            // already commited
            dwRet = IsPageWritable (pptbl->pte[idx2nd])? PAGEIN_SUCCESS : PAGEIN_FAILURE;

        } else  if (HLDPG_SUCCESS == HoldPages (1)) {
            DWORD flags = PM_PT_ZEROED;     // need zero for security reason, don't want to 
                                            // give back a page with existing data from other processes
            pptbl->pte[idx2nd] = MakeWritableEntry (pprc, GetHeldPage (&flags), dwAddr);
#ifdef ARM
            ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
            if (PM_PT_NEEDZERO & flags) {
                ZeroPage ((LPVOID) (dwAddr & ~VM_PAGE_OFST_MASK));
            }
            dwRet = PAGEIN_SUCCESS;

        } else {
            // low on memory, retry (do nothing)
        }

        UnlockVM (pprc);

    } else {
        dwRet = PAGEIN_FAILURE;
        DEBUGCHK (0);   // should've never happen. 
    }
    return dwRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMProcessPageFault: Page fault handler.
//
BOOL VMProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite, DWORD dwPc)
{
    DWORD idxdir        = VA2PDIDX (dwAddr);
    DWORD idx2nd        = VA2PT2ND (dwAddr);
    BOOL  fRet          = FALSE;
    BOOL  fTerminated   = FALSE;
    PTHREAD pth         = pCurThread;
    DWORD dwSaveLastErr = KGetLastError (pth);
    PPAGETABLE pptbl;

    // use kernel as the process to page if above shared heap.
    if (dwAddr > VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    }
    pptbl = GetPageTable (pprc->ppdir, idxdir);

    DEBUGMSG (ZONE_VIRTMEM|ZONE_PAGING, (L"VMProcessPageFault: pprc = %8.8lx, dwAddr = %8.8lx, fWrite = %8.8lx\r\n",
        pprc, dwAddr, fWrite));
    CELOG_PageFault(pprc, dwAddr, TRUE, fWrite);

    // check for page faults in power handler
    if (IsInPwrHdlr()) {
        NKDbgPrintfW(_T("VMProcessPageFault Error: Page fault occurred while in power handler! Address = 0x%08x\r\n"), dwAddr);
        
        // Always return FALSE in debug images to catch the exception in the proper place. 
#ifndef DEBUG
        if (gdwFailPowerPaging == 1) 
#endif  
        {
            // Fail this call so that an exception will be raised at 
            // the address of the code which is being paged-in.
            return FALSE;
        }
    }

    if (pptbl) {
        DWORD    dwEntry = pptbl->pte[idx2nd];

        // special case for committed page - only mapfile handles write to r/o page.
        PFNPager pfnPager = IsPageCommitted (dwEntry)? PageFromMapper : g_vmPagers[GetPagerType(dwEntry)];
        DWORD    dwPgRslt;
#ifdef DEBUG
        DWORD    dwRetries = 0;
#endif
        PPROCESS pprcActv = pActvProc;

        DEBUGMSG (ZONE_VIRTMEM|ZONE_PAGING, (L"VMProcessPageFault: PageType = %d, PageFunc = %8.8lx\r\n", 
            GetPagerType(pptbl->pte[idx2nd]), pfnPager));

        if (!KSEN_ProcessPageFault (pprc, dwAddr, fWrite, dwPc, dwEntry)) {
            while (!fTerminated) {
                
                fRet = IsPageWritable (dwEntry)                     // the page is writable
                       || (!fWrite && IsPageReadable (dwEntry));    // or we're requesting read, and the page is readable

                if (fRet || InSysCall ()) {
                    break;
                }

                dwPgRslt = pfnPager (pprc, dwAddr, fWrite);
                
                if (PAGEIN_RETRY != dwPgRslt) {
                    fRet = (PAGEIN_SUCCESS == dwPgRslt);
                    break;
                }

                DEBUGMSG (ZONE_PAGING|ZONE_PAGING, (TEXT("Page function returned 'retry'\r\n")));

                // do not use NKSleep as we won't be able to terminate the thread
                UB_Sleep (250);

                // check for terminated thread in a non-psl context
                fTerminated = ((pprcActv == pth->pprcOwner) && GET_DYING(pth) && !GET_DEAD(pth));
                
                // DEBUGMSG every 5 secs to help catch hangs
                DEBUGMSG (0 == (dwRetries = (dwRetries+1)%20), (TEXT("VMProcessPageFault: still looping\r\n")));

                dwEntry = pptbl->pte[idx2nd];
            }
                   
            // handle guard pages
            if (IsGuardPage (dwEntry)) {
                InterlockedCompareExchange ((PLONG) &pptbl->pte[idx2nd], CommitGuardPage (dwEntry), dwEntry); 
#ifdef ARM
                ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
            }

        } else {
            KSLV_ProcessPageFault (pprc, dwAddr, fWrite, dwPc, &pptbl->pte[idx2nd], &fRet);
        }
    }

    DEBUGMSG (ZONE_VIRTMEM|ZONE_PAGING, (L"VMProcessPageFault: returns = %d\r\n", fRet));   

    // notify kernel debugger if page-in successfully.
    if (fRet) {
        (*HDPageIn) (PAGEALIGN_DOWN(dwAddr), fWrite);
        KSetLastError (pth, dwSaveLastErr);
    }

    CELOG_PageFault(pprc, dwAddr, FALSE, fRet);

    return (fRet || fTerminated);
}


//-----------------------------------------------------------------------------------------------------------------
//
// KC_VMDemandCommit: Demand commit a stack page for exception handler.
// NOTE: This function is called within KCall.
//
DWORD KC_VMDemandCommit (DWORD dwAddr)
{
    DWORD dwRet = DCMT_OLD;

    if (!IsInKVM (dwAddr)) {
        DEBUGCHK (dwAddr > VM_KMODE_BASE);
        return (dwAddr > VM_KMODE_BASE)? DCMT_OLD : DCMT_FAILED;
    }

    if (IsInKVM (dwAddr)) {
        PPAGETABLE pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (dwAddr));
        DWORD idx2nd = VA2PT2ND (dwAddr);

        DEBUGMSG (ZONE_VIRTMEM, (L"KC_VMDemandCommit: pptbl = %8.8lx, idx2nd = %8.8lx\r\n", pptbl, idx2nd));

        DEBUGCHK (pptbl);   // we will always call KC_VMDemandCommit with a valid secure stack
        if (!IsPageWritable (pptbl->pte[idx2nd])) {
            DWORD flags = 0;
            DWORD dwPfn = PHYS_GrabFirstPhysPage (1, &flags, FALSE);
            
            DEBUGMSG (ZONE_VIRTMEM, (L"KC_VMDemandCommit: Committing page %8.8lx\r\n", dwAddr));
            DEBUGCHK (GetPagerType (pptbl->pte[idx2nd]) == VM_PAGER_AUTO);

            if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
                pptbl->pte[idx2nd] = MakeWritableEntry (g_pprcNK, dwPfn, dwAddr);
#ifdef ARM
                ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
                dwRet = DCMT_NEW;
            } else {
                RETAILMSG (1, (L"!!!!KC_VMDemandCommit: Completely OUT OF MEMORY!!!!\r\n"));
                dwRet = DCMT_FAILED;
            }
        }
    }

    return dwRet;
}


typedef struct _LockEnumStruct {
    DWORD       cPagesNeeded;       // how many page we need to page in
    BOOL        fWrite;             // write access?
} LockEnumStruct, *PLockEnumStruct;


static BOOL CheckLockable (const DWORD *pdwEntry, LPVOID pEnumData)
{
    DWORD dwEntry = *pdwEntry;
    PLockEnumStruct ples = (PLockEnumStruct) pEnumData;
    BOOL  fRet;

    if (IsPageCommitted (dwEntry)) {
        fRet = ples->fWrite? IsPageWritable (dwEntry) : IsPageReadable (dwEntry);
    } else {
        fRet = (VM_PAGER_NONE != GetPagerType (dwEntry));
        ples->cPagesNeeded ++;
    }
    return fRet;
}

typedef struct _PagingEnumStruct {
    PPROCESS pprc;
    DWORD    dwAddr;
    LPDWORD  pPFNs;
    BOOL     fWrite;
} PagingEnumStruct, *PPagingEnumStruct;

PREFAST_SUPPRESS (25033, "pdwEntry cannot be constant as *pdwEntry can be changed in VMProcessPageFault");
static BOOL PageInOnePage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PPagingEnumStruct ppe = (PPagingEnumStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    BOOL fRet = IsPageWritable (dwEntry)                     // the page is writable
           || (!ppe->fWrite && IsPageReadable (dwEntry));    // or we're requesting read, and the page is readable

    if (!fRet
        && !VMProcessPageFault (ppe->pprc, ppe->dwAddr, ppe->fWrite, 0)) {
        DEBUGMSG (ZONE_VIRTMEM, (L"Paging in %8.8lx for process %8.8lx failed\r\n", ppe->dwAddr, ppe->pprc));
        return FALSE;
    }
    DEBUGCHK (IsPageCommitted (*pdwEntry));

    ppe->dwAddr += VM_PAGE_SIZE;

    // update pfn array if requested
    if (ppe->pPFNs) {
        ppe->pPFNs[0] = PFNfromEntry (*pdwEntry);
        ppe->pPFNs ++;
    }

    return TRUE;
}

//
// using the simplest O(n) algorithm here.
//
//  We're assuming that the locked page list will not grow extensively - only drivers
//  doing DMA should be locking pages for long period of time. All other locking should
//  be temporary and would be unlocked soon. 
//
//  We will have to revisit this if we found that the locked page list grows beyond control
//  for it'll affect the perf of VM allocation if the list gets too long.
//

//
// locking/unlocking VM pages. 
//
BOOL VMAddToLockPageList     (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    PLOCKPAGELIST plp = (PLOCKPAGELIST) AllocMem (HEAP_LOCKPAGELIST);
    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));

    if (plp) {
        plp->dwAddr = dwAddr;
        plp->cbSize = (cPages << VM_PAGE_SHIFT);
        plp->pNext  = pprc->pLockList;
        pprc->pLockList = plp;
    }
    return TRUE;
}

BOOL VMRemoveFromLockPageList   (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    DWORD cbSize = cPages << VM_PAGE_SHIFT;
    PLOCKPAGELIST *pplp, plp;

    for (pplp = &pprc->pLockList; NULL != (plp = *pplp); pplp = &plp->pNext) {
        if (   IsValidKPtr (plp)
            && (dwAddr == plp->dwAddr)
            && (cbSize == plp->cbSize)) {
            // found, remove from list
            *pplp = plp->pNext;
            FreeMem (plp, HEAP_LOCKPAGELIST);
            return TRUE;
        }
    }

    
    return FALSE;
}

void VMFastUnlockPages (PPROCESS pprc, PLOCKPAGELIST plpUnlock)
{
    PLOCKPAGELIST *pplp, plp;
    VERIFY (LockVM (pprc));
    for (pplp = &pprc->pLockList; NULL != (plp = *pplp); pplp = &plp->pNext) {
        if (plp == plpUnlock) {
            *pplp = plp->pNext;
            break;
        }
    }
    UnlockVM (pprc);
}

// NOTE: VMFastLockPages doesn't page in the pages, it only add the range specified to the lock page list
void VMFastLockPages (PPROCESS pprc, PLOCKPAGELIST plp)
{
    VERIFY (LockVM (pprc));
    plp->pNext = pprc->pLockList;
    pprc->pLockList = plp;
    UnlockVM (pprc);
}

DWORD dwMaxLockListIters;

BOOL VMIsPagesLocked (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    PLOCKPAGELIST plp;
    DWORD cbSize = cPages << VM_PAGE_SHIFT;
#ifdef VM_DEBUG
    DWORD nIters = 0;
#endif
    for (plp = pprc->pLockList; plp; plp = plp->pNext) {
#ifdef VM_DEBUG
        nIters ++;
#endif
        if ((dwAddr < plp->dwAddr + plp->cbSize)
            && (dwAddr + cbSize > plp->dwAddr)) {
            return TRUE;
        }
    }

#ifdef VM_DEBUG
    if (dwMaxLockListIters < nIters) {
        dwMaxLockListIters = nIters;
    }
#endif

    return FALSE;
}

void VMFreeLockPageList (PPROCESS pprc)
{
    DEBUGCHK (pprc != g_pprcNK);
    if (LockVM (pprc)) {
        PLOCKPAGELIST plp;
        pprc->bState = PROCESS_STATE_NO_LOCK_PAGES;
        while (NULL != (plp = pprc->pLockList)) {
            DEBUGMSG (ZONE_ERROR, (L"ERROR: VMFreeLockPageList - Process'%s' exited with address 0x%8.8lx of size %0x%x still locked\r\n",
                pprc->lpszProcName, plp->dwAddr, plp->cbSize));
            DEBUGCHK (0);
            pprc->pLockList = plp->pNext;
            if (IsValidKPtr (plp)) {
                FreeMem (plp, HEAP_LOCKPAGELIST);
            }
        }
        UnlockVM (pprc);
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// DoVMLockPages: worker function to lock VM pages.
//      returns 0 if success, error code if failed
//
static DWORD DoVMLockPages (PPROCESS pprc, DWORD dwAddr, DWORD cPages, LPDWORD pPFNs, DWORD fOptions)
{
    PPAGEDIRECTORY ppdir;
    DWORD dwErr = 0;

    if (dwAddr >= VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    }

    ppdir = LockVM (pprc);
    
    if (ppdir) {

        DEBUGCHK (IsLockPageAllowed (pprc));

        // add the pages to locked VM list, to prevent them from being paged out        
        if (   !IsLockPageAllowed (pprc)                                // lock pages not allowed
            || !VMAddToLockPageList (pprc, dwAddr, cPages)) {           // failed to add to locked page list
            DEBUGMSG (ZONE_VIRTMEM, (L"DoVMLockPages: failed 2\r\n"));
            dwErr = ERROR_NOT_ENOUGH_MEMORY;

        }

        // release VM lock before we start paging in 
        UnlockVM (pprc);

        if (!dwErr) {
            
            // page in the pages
            // NOTE: We *cannot* lock VM here for it will introduce deadlock if pager is invoked. However, the
            //       enumeration here is safe because we've already added the pages to the 'locked page list',
            //       thus these pages cannot be paged out, and page tables cannot be destroyed.
            //       When we encounter pages that needs to be paged in, the pager functions handles VM
            //       locking/unlocking themselves. 
            //
            PagingEnumStruct pe = { pprc, dwAddr, pPFNs, fOptions & VM_LF_WRITE };

            // page in all the pages
            if (!Enumerate2ndPT (ppdir, dwAddr, cPages, 0, PageInOnePage, &pe)) {
                DEBUGMSG (ZONE_VIRTMEM, (L"Unable to Page in from %8.8lx for %d Pages, fWrite = %d\r\n", dwAddr, cPages, fOptions & VM_LF_WRITE));
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            }

            // unlock the pages if query only or error occurs
            if (dwErr || (VM_LF_QUERY_ONLY & fOptions)) {
                VERIFY (LockVM (pprc));
                VERIFY (VMRemoveFromLockPageList (pprc, dwAddr, cPages));
                UnlockVM (pprc);
            }
        }

    } else {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    return dwErr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// DoVMUnlockPages: worker function to unlock VM.
//      returns 0 if success, error code if failed
//
static DWORD DoVMUnlockPages (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    if (dwAddr >= VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    }
    if (LockVM (pprc)) {
        if (VMRemoveFromLockPageList (pprc, dwAddr, cPages)) {
            dwErr = 0;
        }
        UnlockVM (pprc);
    }
    return dwErr;
}


//-----------------------------------------------------------------------------------------------------------------
//
//  DecommitPages: 2nd level page table enumeration function, Decommit virtual pages
//      pEnumData: (DWORD) the pager type, or (VM_MAX_PAGER+1) for release.
//                 May also include VM_PAGER_POOL flag for releasing to pool.
//
static BOOL DecommitPages (LPDWORD pdwEntry, LPCVOID pEnumData)
{
    DWORD dwEntry = *pdwEntry;
    DWORD dwPagerType = ((DWORD) pEnumData) & ~VM_PAGER_POOL;

    // decommit the page if committed
    if (IsPageCommitted (dwEntry)) {
        DWORD dwPFN = PFNfromEntry (dwEntry);

        // must clear the entry before freeing the page back to memory pool, or
        // we can get into racing condition if preempted.
        *pdwEntry = MakeReservedEntry (dwPagerType);

        if ((((DWORD) pEnumData) & VM_PAGER_POOL)) {
            // Return to the proper page pool
            DEBUGCHK (VM_PAGER_LOADER == dwPagerType);
            DEBUGCHK (GetRegionFromPFN (dwPFN));
            PGPOOLFreePage (&g_LoaderPool, dwPFN);
        } else {
            FreePhysPage (dwPFN);
        }
    }

    // make entry free if releasing the region
    if (VM_MAX_PAGER < dwPagerType) {
        *pdwEntry = VM_FREE_PAGE;
    }
    
    return TRUE;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMDecommit: worker function to decommit pages for VirtualFree. 
//      return 0 if succeed, error code if failed
//
BOOL VMDecommit (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, DWORD dwPagerType)
{
    DWORD dwAddr = (DWORD) pAddr;
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    DWORD dwOfst = dwAddr & VM_PAGE_OFST_MASK;
    DWORD cPages = PAGECOUNT (cbSize + dwOfst);
    
    DEBUGCHK ((int) cPages > 0);
    dwAddr -= dwOfst;

    DEBUGMSG (ZONE_VIRTMEM, (L"VMDecommit - Decommitting from %8.8lx, for 0x%x pages\r\n", dwAddr, cPages));
    if (LockVM (pprc)) {

        if (!VMIsPagesLocked (pprc, dwAddr, cPages)) {

            VERIFY (Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, 0, DecommitPages, (LPVOID) dwPagerType));

            InvalidatePages (pprc, dwAddr, cPages);
            dwErr = 0;

            CELOG_VirtualFree(pprc, dwAddr, cPages, MEM_DECOMMIT);
        }

        UnlockVM (pprc);
    }

    if (!dwErr) {
        (*HDPageOut) (PAGEALIGN_DOWN(dwAddr), cPages);
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }
    DEBUGMSG (ZONE_VIRTMEM, (L"VMDecommit - returns dwErr = 0x%x\r\n", dwErr));
    
    return !dwErr;
}

#define VM_MAX_PAGES       (0x80000000 >> VM_PAGE_SHIFT)


//-----------------------------------------------------------------------------------------------------------------
//
// VMRelease: worker function to release VM reservation. 
//      return 0 if succeed, error code if failed
//
static DWORD VMRelease (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    PPAGEDIRECTORY ppdir = LockVM (pprc);
    DWORD dwErr = ERROR_INVALID_PARAMETER;

    DEBUGCHK (!(dwAddr & VM_BLOCK_OFST_MASK));
    DEBUGCHK (cPages);

    if (ppdir) {

        if (VMIsPagesLocked (pprc, dwAddr, cPages)) {
            DEBUGMSG (ZONE_ERROR, (L"ERROR: VMRelease - Cannot release VM from dwAddr = %8.8lx of %8.8lx pages because the pages are locked\r\n", dwAddr, cPages));
            
        } else {
            LPDWORD pdwBaseFree = &pprc->vaFree;
            DWORD   dwDummy = 0;

            DEBUGMSG (ZONE_VIRTMEM, (L"VMRelease: release VM from dwAddr = %8.8lx of %8.8lx pages\r\n", dwAddr, cPages));

            // pass (VM_MAX_PAGER + 1) to DecommitPages will release the region
            VERIFY (Enumerate2ndPT (ppdir, dwAddr, cPages, 0, DecommitPages, (LPVOID) (VM_MAX_PAGER + 1)));
            InvalidatePages (pprc, dwAddr, cPages);

            if (pprc == g_pprcNK) {
                // NK - take care of special ranges
                if (IsInSharedHeap (dwAddr)) {
                    pdwBaseFree = &g_vaSharedHeapFree;
                } else if (IsInRAMMapSection (dwAddr)) {
                    pdwBaseFree = &g_vaRAMMapFree;
                } else if (!IsInKVM (dwAddr)) {
                    DEBUGCHK (dwAddr >= VM_DLL_BASE);
                    // When debugchk failed, someone in kernel mode is calling VirtualFree with a not valid user mode address.
                    // Don't update any VM free base in this case
                    pdwBaseFree = (dwAddr >= VM_DLL_BASE)? &g_vaDllFree : &dwDummy;
                }
            }

            if (*pdwBaseFree > dwAddr) {
                *pdwBaseFree = dwAddr;
            }
            dwErr = 0;
            
            CELOG_VirtualFree(pprc, dwAddr, cPages, MEM_RELEASE);
        }
        UnlockVM (pprc);
    }

    if (!dwErr) {
        (*HDPageOut) (PAGEALIGN_DOWN(dwAddr), cPages);
    }

    return dwErr;
}


//-----------------------------------------------------------------------------------------------------------------
//
// ChangeProtection: 2nd level PT enumeration funciton. change paget table entry protection
//          (DWORD) pEnumData  is the new permission
//
static BOOL ChangeProtection (LPDWORD pdwEntry, LPVOID pEnumData)
{
    *pdwEntry = MakeCommittedEntry (PFNfromEntry (*pdwEntry), (DWORD) pEnumData);

    return TRUE;
}


//-----------------------------------------------------------------------------------------------------------------
//
// DoVMProtect: worker function to change VM protection. 
//      return 0 if succeed, error code if failed
//
static DWORD DoVMProtect (PPROCESS pprc, DWORD dwAddr, DWORD cPages, DWORD fNewProtect, LPDWORD pdwOldProtect)
{
    DWORD dwErr = 0;
    DWORD cpNeeded = 0;         // # of uncommited pages
    
    if (LockVM (pprc)) {

        if (VMIsPagesLocked (pprc, dwAddr, cPages)) {
            dwErr = ERROR_SHARING_VIOLATION;

        } else if (!VMScan (pprc, dwAddr, cPages, &cpNeeded, fNewProtect & VM_READWRITE_PROT)  // pages in the same reserved region?
            || cpNeeded) {                                                              // all page committed?
            dwErr = ERROR_INVALID_PARAMETER;
        
        } else {
            PPAGEDIRECTORY ppdir = pprc->ppdir;

            __try {
                DWORD dwOldProtect;
                PPAGETABLE pptbl = GetPageTable (ppdir, VA2PDIDX (dwAddr));
                DEBUGCHK (pptbl);
                dwOldProtect = ProtectFromEntry (pptbl->pte[VA2PT2ND(dwAddr)]);

                // flush cache if we change protection to non-cacheable (PAGE_NOCACHE)
                if ((fNewProtect & PAGE_NOCACHE) && !(dwOldProtect & PAGE_NOCACHE)) {
                    NKCacheRangeFlush ((LPVOID) dwAddr, cPages << VM_PAGE_SHIFT, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);
                }

                if (pdwOldProtect) {
                    *pdwOldProtect = dwOldProtect;
                }

                VERIFY (Enumerate2ndPT (ppdir, dwAddr, cPages, 0, ChangeProtection, (LPVOID) PageParamFormProtect (pprc, fNewProtect, dwAddr)));

                InvalidatePages (pprc, dwAddr, cPages);
                
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
        }

        UnlockVM (pprc);
    } else {
        dwErr = ERROR_INVALID_PARAMETER;
    }
    return dwErr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMFastAllocCopyToKernel: (internal only) reserve VM and VirtualCopy from source proceess/address.
//              Source is always pVMProc, Destination process is always kernel, and always R/O
//
static LPVOID
VMFastAllocCopyToKernel (
    LPCVOID  pAddr,                 // address to be copy from
    DWORD    cbSize                 // size of the ptr
    )
{
    DWORD dwOfst = (DWORD) pAddr & VM_PAGE_OFST_MASK;
    DWORD dwAddr = (DWORD) pAddr & ~VM_PAGE_OFST_MASK;
    DWORD dwBlkOfst = ((DWORD)pAddr & VM_BLOCK_OFST_MASK);
    LPBYTE pDst = VMReserve (g_pprcNK, cbSize + dwBlkOfst, 0, 0);
    DWORD dwErr = 0;

    if (!pDst) {
        dwErr  = ERROR_NOT_ENOUGH_MEMORY;
        
    } else {
        DWORD cPages;
        DWORD _pPFNs[64];   // local buffer up to 64 pages
        LPDWORD pPFNs = _pPFNs;

        pDst   += dwBlkOfst - dwOfst;       // page align destination
        cbSize += dwOfst;                   // size, starting from paged aligned address
        cPages = PAGECOUNT (cbSize);
        
        if ((cPages > 64) && (NULL == (pPFNs = NKmalloc (cPages * sizeof (DWORD))))) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
            
        } else {
            dwErr = DoVMLockPages (pVMProc, dwAddr, cPages, pPFNs, VM_LF_READ);
            if (!dwErr) {

                if (!VMSetPages (g_pprcNK, (DWORD) pDst, pPFNs, cPages, PAGE_READONLY|PAGE_PHYSICAL)) {
                    dwErr = ERROR_NOT_ENOUGH_MEMORY;
                }

                VERIFY (!DoVMUnlockPages (pVMProc, dwAddr, cPages));
            }
        }
            
        if (_pPFNs != pPFNs) {
            NKfree (pPFNs);
        }

        if (dwErr) {
            VERIFY (VMFreeAndRelease (g_pprcNK, pDst, cbSize));
        } else {
            pDst += dwOfst;
        }
    }

    if (dwErr) {
        NKSetLastError (dwErr);
    }

    // NOTE: NO CACHE FLUSH HERE EVEN FOR VIVT CACHE. CALLER IS RESPONSIBLE FOR FLUSHING CACHE
    return dwErr? NULL : pDst;

}



#define LOCAL_COPY_SIZE      2*MAX_PATH // cico optimization for strings and small buffers

//-----------------------------------------------------------------------------------------------------------------
//
// VMReadProcessMemory: read process memory. 
//      return 0 if succeed, error code if failed
//
DWORD VMReadProcessMemory (PPROCESS pprc, LPCVOID pAddr, LPVOID pBuf, DWORD cbSize)
{
    DWORD    dwErr     = 0;
    PTHREAD  pth       = pCurThread;
    DWORD    dwSaveKrn = pth->tlsPtr[TLSSLOT_KERNEL];

    pth->tlsPtr[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;

    if ((int) cbSize <= 0) {
        dwErr = cbSize? ERROR_INVALID_PARAMETER : 0;
        
    } else if ((DWORD) pAddr >= VM_SHARED_HEAP_BASE) {

        // reading kernel address, direct copy 
        if (!CeSafeCopyMemory (pBuf, pAddr, cbSize)) {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    
    } else {

        // reading user-mode address. Need to switch VM regardless, for we can be called from
        // a kernel thread, which doesn't have any VM affinity.
        PPROCESS pprcOldVM = SwitchVM (pprc);

        //
        // Active process switch is needed because we don't flush cache/TLB when CPU supports ASID.
        // In which case, we can end up leaving wrong entry in TLB if we don't switch active process.
        //
        PPROCESS pprcOldActv = SwitchActiveProcess (g_pprcNK);

        if (((DWORD) pBuf >= VM_SHARED_HEAP_BASE) || (pprc == pprcOldVM)) {
            // pBuf is accesible after VM switch, just copy
            if (!CeSafeCopyMemory (pBuf, pAddr, cbSize)) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
        } else {
            // pBuf is not accesible after VM switch.
            BYTE    _localCopy[LOCAL_COPY_SIZE];
            LPVOID  pLocalCopy = _localCopy;

            if (cbSize <= LOCAL_COPY_SIZE) {
                // small size - use CICO
                if (!CeSafeCopyMemory (pLocalCopy, pAddr, cbSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                } 
            } else {
                pLocalCopy = VMFastAllocCopyToKernel (pAddr, cbSize);
                if (!pLocalCopy) {
                    dwErr = ERROR_NOT_ENOUGH_MEMORY;
                }
            }

            if (!dwErr) {
                DEBUGCHK (pprcOldVM);   // kernel thread calling ReadProcesssMemory with a user buffer. Should never happen.

                // Note: the VM switch below will flush cache on VIVT. So no explicit cache flush
                //       is needed here.

                SwitchActiveProcess (pprcOldActv);  // switch back to original active process
                SwitchVM (pprcOldVM);               // switch back to orignial VM
                if (!CeSafeCopyMemory (pBuf, pLocalCopy, cbSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
            }

            if ((_localCopy != pLocalCopy) && pLocalCopy) {
                VERIFY (VMFreeAndRelease (g_pprcNK, pLocalCopy, cbSize));
            }
        }

        // we may end-up doing one extra switch here, but it's alright for the calls will be no-op.
        SwitchActiveProcess (pprcOldActv);
        SwitchVM (pprcOldVM);
    }

    pth->tlsPtr[TLSSLOT_KERNEL] = dwSaveKrn;

    return dwErr;
}

static BOOL ReplacePages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    DWORD dwEntry = *pdwEntry;
    BOOL fRet = IsPageCommitted (dwEntry);

    if (fRet) {
        PVCVirtStruct pvcvs = (PVCVirtStruct) pEnumData;
        DWORD dwPfn = PFNfromEntry (pvcvs->pptbl->pte[pvcvs->idx2nd]);

        EnterCriticalSection (&PhysCS);
        fRet = DupPhysPage (dwPfn);
        LeaveCriticalSection (&PhysCS);

        if (fRet) {
            *pdwEntry = MakeCommittedEntry (dwPfn, dwEntry & PG_PERMISSION_MASK);

            FreePhysPage (PFNfromEntry (dwEntry));
            
            if (VM_NUM_PT_ENTRIES == ++ pvcvs->idx2nd) {
                pvcvs->idx2nd = 0;
                pvcvs->idxdir = NextPDEntry (pvcvs->idxdir);
                pvcvs->pptbl  = GetPageTable (pvcvs->ppdir, pvcvs->idxdir);
            }
        }
    }

    return fRet;
}

//
// copy-on-write support only for user mode 
// exe addresses or user mode dll address
//
static DWORD VMCopyOnWrite (PPROCESS pprc, LPVOID pAddr, LPCVOID pBuf, DWORD cbSize)
{
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    DWORD dwAddr = (DWORD) pAddr;
    DWORD dwProt = 0;
    DWORD dwPageOfst = dwAddr & VM_PAGE_OFST_MASK;
    DWORD  cPages = PAGECOUNT  (cbSize + dwPageOfst);

    DEBUGCHK (pprc != g_pprcNK);                        // cannot be kernel
    DEBUGCHK (pprc == pVMProc);                         // pprc must be current VM
    DEBUGCHK ((DWORD) pBuf >= VM_SHARED_HEAP_BASE);     // pBuf must be global accessible area

    // EXE address (not in ROM)
    if ((dwAddr < (DWORD) pprc->BasePtr + pprc->e32.e32_vsize)
        && !(pprc->oe.filetype & FA_XIP)) {

        // change the protection to read-write
        if(VMProtect(pprc, pAddr, cbSize, PAGE_READWRITE, &dwProt)) {

            // update the code pages
            if (CeSafeCopyMemory(pAddr, pBuf, cbSize))
            {
                // flush the cache for the address range
                dwAddr -= dwPageOfst;
                // write-back d-cache and flush i-cache
                NKCacheRangeFlush ((LPVOID)dwAddr,  cPages << VM_PAGE_SHIFT, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS);
                dwErr = 0;
            }

            // revert back the permissions on the page
            // the following call will flush the tlb
            VMProtect(pprc, pAddr, cbSize, dwProt, NULL);
        }
    }
    
    // DLL address or failed to write exe address
    if (dwErr && (dwAddr > VM_DLL_BASE) && (dwAddr < VM_RAM_MAP_BASE)) {
            
        // try to do copy on write.
        DWORD dwBlkOfst  = dwAddr & VM_BLOCK_OFST_MASK;
        LPBYTE pBase = (LPBYTE) VMReserve (g_pprcNK, cbSize + dwBlkOfst, 0, 0);
        LPBYTE pBaseCopy = pBase + dwBlkOfst - dwPageOfst;

        if (!pBase || !VMCommit (g_pprcNK, pBaseCopy, cPages << VM_PAGE_SHIFT, PAGE_READWRITE, 0)) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
            
        } else {
            
            dwAddr -= dwPageOfst;       // page align start address

            // copy original content IN PAGES from target to the local copy.
            if (CeSafeCopyMemory (pBaseCopy, (LPCVOID) dwAddr, cPages << VM_PAGE_SHIFT) // copy original contents 
                && CeSafeCopyMemory (pBaseCopy + dwPageOfst, pBuf, cbSize)              // copy the inteneded data to be written
                && LockVM (pprc)) {                                                     // process not being destroyed.

                VCVirtStruct   vcvs;

                // write-back d-cache and flush i-cache
                NKCacheRangeFlush ((LPVOID)dwAddr,  cPages << VM_PAGE_SHIFT, CACHE_SYNC_WRITEBACK|CACHE_SYNC_INSTRUCTIONS);

                // setup arguments to replace pages
                vcvs.idxdir = VA2PDIDX (pBaseCopy);
                vcvs.idx2nd = VA2PT2ND (pBaseCopy);

                // lock kernel VM
                vcvs.ppdir  = LockVM (g_pprcNK);
                vcvs.pptbl  = GetPageTable (g_ppdirNK, vcvs.idxdir);

                // replace the destination pages, 
                dwErr = Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, 0, ReplacePages, &vcvs)
                                ? 0
                                : ERROR_INVALID_PARAMETER;

                // this will flush the tlb
                InvalidatePages (pprc, dwAddr, cPages);
                
                UnlockVM (g_pprcNK);
                UnlockVM (pprc);

            }
        }

        if (pBase) {
            VERIFY (VMFreeAndRelease (g_pprcNK, pBase, cbSize + dwBlkOfst));
        }
    }

    return dwErr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMWriteProcessMemory: write process memory. 
//      return 0 if succeed, error code if failed
//
DWORD VMWriteProcessMemory (PPROCESS pprc, LPVOID pAddr, LPCVOID pBuf, DWORD cbSize)
{
    DWORD    dwErr     = 0;
    PTHREAD  pth       = pCurThread;
    DWORD    dwSaveKrn = pth->tlsPtr[TLSSLOT_KERNEL];

    pth->tlsPtr[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;

    if ((int) cbSize <= 0) {
        dwErr = cbSize? ERROR_INVALID_PARAMETER : 0;
        
    } else if ((DWORD) pAddr >= VM_SHARED_HEAP_BASE) {

        // writing kernel address, direct copy 
        if (!CeSafeCopyMemory (pAddr, pBuf, cbSize)) {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    
    } else {

        // writing to user mode address.
        BYTE    _localCopy[LOCAL_COPY_SIZE];
        LPVOID  pLocalCopy = _localCopy;

        // kernel threads should've never call WriteProcessMemory with a user buffer as source.
        DEBUGCHK (pth->pprcVM || ((DWORD) pBuf >= VM_SHARED_HEAP_BASE));

        if (((DWORD) pBuf < VM_SHARED_HEAP_BASE) && (pprc != pVMProc)) {
            
            // pBuf not accessible after VM switch. make a local copy
            if (cbSize <= LOCAL_COPY_SIZE) {
                // small size - use CICO
                if (!CeSafeCopyMemory (pLocalCopy, pBuf, cbSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
            } else {
                pLocalCopy = VMFastAllocCopyToKernel (pBuf, cbSize);
                if (!pLocalCopy) {
                    dwErr = ERROR_NOT_ENOUGH_MEMORY;
                }
            }

            // switch the source to the local copy
            pBuf = pLocalCopy;
        }

        if (!dwErr) {

            // VM Switch here ensure that we're writing the cache back before the copy for VIVT cache
            PPROCESS pprcOldVM   = SwitchVM (pprc);
            PPROCESS pprcOldActv = SwitchActiveProcess (g_pprcNK);

            if (!CeSafeCopyMemory (pAddr, pBuf, cbSize)) {
                // try copy-on-write only if pprc is a debuggee controlled by the calling process                
                dwErr = ((pprc == g_pprcNK) 
                          || !(pprc->pDbgrThrd)
                          || (pprc->pDbgrThrd->pprcOwner != pth->pprcOwner))
                                ? ERROR_INVALID_PARAMETER
                                : VMCopyOnWrite (pprc, pAddr, pBuf, cbSize);                
            }

            SwitchActiveProcess (pprcOldActv);
            SwitchVM (pprcOldVM);
        }

        if ((_localCopy != pLocalCopy) && pLocalCopy) {
            VMFreeAndRelease (g_pprcNK, pLocalCopy, cbSize);
        }
    }

    pth->tlsPtr[TLSSLOT_KERNEL] = dwSaveKrn;

    return dwErr;
}


typedef struct {
    DWORD dwMask;
    DWORD dwNewFlags;
    DWORD dw1stEntry;
} VSAStruct, *PVSAStruct;

//-----------------------------------------------------------------------------------------------------------------
//
// ChangeAttrib: 2nd level PT enumeration funciton. change paget table entry attributes
//               based on mask and newflags
//
static BOOL ChangeAttrib (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVSAStruct pvsa = (PVSAStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    // 1st call - update dwIstEntry

    if (!pvsa->dw1stEntry) {
        pvsa->dw1stEntry = dwEntry;
    }
    *pdwEntry = (dwEntry & ~pvsa->dwMask) | (pvsa->dwMask & pvsa->dwNewFlags);
    return TRUE;
}



//
// VMSetAttributes - change the attributes of a range of VM
//
BOOL VMSetAttributes (PPROCESS pprc, LPVOID lpvAddress, DWORD cbSize, DWORD dwNewFlags, DWORD dwMask, LPDWORD lpdwOldFlags)  
{
    DWORD dwErr = 0;
    VSAStruct vsa = { dwMask, dwNewFlags, 0 };

    DEBUGMSG (ZONE_VIRTMEM, (L"VMSetAttributes: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                        pprc, lpvAddress, cbSize, dwNewFlags, dwMask, lpdwOldFlags));

    DEBUGCHK (pActvProc == g_pprcNK);
    
    // validate parameter
    if ((int) cbSize <= 0) {                                                  // size is too big
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else {

        DWORD dwAddr = (DWORD) lpvAddress & ~VM_PAGE_OFST_MASK;             // page align the address
        DWORD cPages = PAGECOUNT (cbSize + ((DWORD) lpvAddress - dwAddr));  // total pages
        DWORD cpNeeded = 0;                                                 // # of uncommited pages

        DEBUGMSG (dwMask & ~PG_CHANGEABLE,
            (L"WARNING: VMSetAttributes changing bits that is not changeable, dwMask = %8.8lx\r\n", dwMask));

        if (dwAddr >= VM_SHARED_HEAP_BASE) {
            pprc = g_pprcNK;
        }

#ifdef ARM
        if ((g_pKData->nCpus > 1) && (PG_CACHE_MASK & dwNewFlags)) {
            // multi-core, setting to cached - must set 'S' bit
            vsa.dwMask     |= PG_V6_SHRAED;
            vsa.dwNewFlags |= PG_V6_SHRAED;
        }
#endif

        if (LockVM (pprc)) {

            if (!VMScan (pprc, dwAddr, cPages, &cpNeeded, 0)            // pages in the same reserved region?
                || cpNeeded) {                                          // all page committed?
                dwErr = ERROR_INVALID_PARAMETER;
            } else {
                VERIFY (Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, 0, ChangeAttrib, &vsa));
                InvalidatePages (pprc, dwAddr, cPages);
            }

            UnlockVM (pprc);
        } else {
            dwErr = ERROR_INVALID_PARAMETER;
        }
        
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    } else if (lpdwOldFlags) {
        VERIFY (CeSafeCopyMemory (lpdwOldFlags, &vsa.dw1stEntry, sizeof (DWORD)));
    }

    return !dwErr;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMAllocCopy: (internal only) reserve VM and VirtualCopy from source proceess/address.
//              Destination process is always the process whose VM is in use (pVMProc)
//
LPVOID
VMAllocCopy (
    PPROCESS pprcSrc,               // source process
    PPROCESS pprcDst,               // destination process
    LPVOID   pAddr,                 // address to be copy from
    DWORD    cbSize,                // size of the ptr
    DWORD    fProtect,              // protection,
    PALIAS   pAlias                 // ptr to alias structure
    )
{
    BOOL fPhys = (fProtect & PAGE_PHYSICAL);
    DWORD dwOfst = (fPhys? (DWORD)pAddr << 8 : (DWORD)pAddr) & VM_PAGE_OFST_MASK;
    DWORD dwAddr = (DWORD) pAddr - dwOfst;
    DWORD dwBlkOfst = (fPhys? (DWORD)pAddr << 8 : (DWORD)pAddr) & VM_BLOCK_OFST_MASK;
    LPBYTE pDst = VMReserve (pprcDst, cbSize + dwBlkOfst, 0, 0);
    DWORD dwErr = 0;

    if (!pDst) {
        dwErr  = ERROR_NOT_ENOUGH_MEMORY;
        
    } else {
        DWORD cPages;
        pDst   += dwBlkOfst - dwOfst;       // page align destination
        cbSize += dwOfst;                   // size, starting from paged aligned address
        cPages = PAGECOUNT (cbSize);
        
        if (fPhys || IsKernelVa (pAddr)) {
            // NOTE: call to PFNFrom256 must use original address, for the physical address is already shifted
            //       right by 8, masking bottom 12 bits can result in wrong physical address.
            DWORD dwPfn = fPhys? PFNfrom256 ((DWORD) pAddr) : GetPFN ((LPVOID) dwAddr);
            DEBUGCHK (INVALID_PHYSICAL_ADDRESS != dwPfn);

            VERIFY (VMCopyPhysical (pprcDst, (DWORD) pDst, dwPfn, cPages, PageParamFormProtect (pprcDst, fProtect&~PAGE_PHYSICAL, (DWORD) pDst), FALSE));

        } else {
            DWORD _pPFNs[64];   // local buffer up to 64 pages
            LPDWORD pPFNs = _pPFNs;
            
            if ((cPages > 64) && (NULL == (pPFNs = NKmalloc (cPages * sizeof (DWORD))))) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
                
            } else {
                dwErr = DoVMLockPages (pprcSrc, dwAddr, cPages, pPFNs, (PAGE_READWRITE & fProtect)? VM_LF_WRITE : VM_LF_READ);
                if (!dwErr) {

                    if (!VMSetPages (pprcDst, (DWORD) pDst, pPFNs, cPages, fProtect|PAGE_PHYSICAL)) {
                        dwErr = ERROR_NOT_ENOUGH_MEMORY;
                    }
#ifdef ARM
                    // For virtually-tagged cache AllocateAlias adds PAGE_NOCACHE
                    // to fProtect, so the dest buffer will be uncached.  But the
                    // source buffer still needs to be uncached.
                    else if (pAlias && !VMAlias (pAlias, pprcSrc, dwAddr, cbSize, fProtect)) {
                        dwErr = ERROR_NOT_ENOUGH_MEMORY;
                    }
#endif                
                    VERIFY (!DoVMUnlockPages (pprcSrc, dwAddr, cPages));
                }
            }
            
            if (_pPFNs != pPFNs) {
                NKfree (pPFNs);
            }
        }

        if (dwErr) {
            VERIFY (VMFreeAndRelease (pprcDst, pDst, cbSize));
        } else {
            pDst += dwOfst;
        }
    }

    if (dwErr) {
        NKSetLastError (dwErr);
    }
    return dwErr? NULL : pDst;

}

void MDSetupUserKPage (PPAGETABLE pptbl);
void MDClearUserKPage (PPAGETABLE pptbl);

void SetupFirstBlock (PPAGETABLE pptbl)
{
    int idx;
    DWORD dwEntry = EntryFromReserveType (0);
    
    // 1st 64K reseved no access, except UserKData page
    for (idx = 0; idx < VM_PAGES_PER_BLOCK; idx ++) {
        pptbl->pte[idx] = dwEntry;
    }

    MDSetupUserKPage (pptbl);
    
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMInit: Initialize per-process VM (called at process creation) 
//
PPAGEDIRECTORY VMInit (PPROCESS pprc)
{
    PPAGEDIRECTORY ppdir = AllocatePD ();
    DEBUGCHK (pprc != g_pprcNK);
    
    if (ppdir) {

        PPAGETABLE pptbl = AllocatePTBL (ppdir, 0);
        if (pptbl) {

            SetupFirstBlock (pptbl);

            pprc->ppdir = ppdir;
            pprc->bASID = (BYTE) MDAllocateASID ();

            // reserved 64K VM at the beginning of VM_DLL_BASE, as region barrier
            VERIFY (VMAlloc (pprc, (LPVOID) VM_DLL_BASE, VM_BLOCK_SIZE, MEM_RESERVE, PAGE_NOACCESS));

            // ASLR support, VM base start randomly from 64K to 64M
            pprc->vaFree = NextAslrBase () + VM_USER_BASE;

        } else {
            FreePD (ppdir);
        }
    }

    DEBUGMSG (!ppdir, (L"!!! VMInit Failed, failed to allocate page directory !!!\r\n"));
    return pprc->ppdir;
}

BOOL KernelVMInit (void)
{
    PPAGETABLE pptbl = GetPageTable (g_ppdirNK, 0);
    if (!pptbl) {
        pptbl = AllocatePTBL (g_ppdirNK, 0);
    }
    if (pptbl) {
        DWORD dwKDllFirst   = (ROMChain->pTOC->dllfirst << 16);
        DWORD dwKDllEnd     = (ROMChain->pTOC->dlllast << 16);
        ROMChain_t *pROM    = ROMChain->pNext;

        // find out the start and end address of ROM KDLLs
        while (pROM) {
            if (dwKDllFirst > (pROM->pTOC->dllfirst << 16)) {
                dwKDllFirst = (pROM->pTOC->dllfirst << 16);
            }

            if (dwKDllEnd   < (pROM->pTOC->dlllast << 16)) {
                dwKDllEnd   = (pROM->pTOC->dlllast << 16);
            }

            pROM = pROM->pNext;
        }

        g_pKData->dwKDllFirst = dwKDllFirst;
        g_pKData->dwKDllEnd   = dwKDllEnd;

        SetupFirstBlock (pptbl);
        
        PcbSetVMProc (g_pprcNK);
        
        // reserve 1st 64K of shared heap for protection
        VERIFY (DoVMReserve (g_pprcNK, 0, PAGECOUNT (VM_BLOCK_SIZE), VM_SHARED_HEAP_BASE, 0));
        
#ifndef SHIP_BUILD
        // reserve the block which includes the freed memory/heap sentinel value (0xCCCCCCCC) on debug images
        VERIFY (DoVMReserve (g_pprcNK, 0xCCCC0000, PAGECOUNT (VM_BLOCK_SIZE), 0, 0));
#endif

        //
        // Reserve VM for XIP kernel DLLs.
        //
        DEBUGMSG (ZONE_VIRTMEM, (L"Reserve VM for kernel XIP DLls, first = %8.8lx, last = %8.8lx\r\n",
                        g_pKData->dwKDllFirst, g_pKData->dwKDllEnd));

        VERIFY (DoVMReserve (g_pprcNK, g_pKData->dwKDllFirst, PAGECOUNT (g_pKData->dwKDllEnd - g_pKData->dwKDllFirst), 0, MEM_IMAGE));
        g_pprcNK->bASID = (BYTE) MDGetKernelASID ();

        if (dwMaxUserAllocAddr > VM_SHARED_HEAP_BASE) {
            dwMaxUserAllocAddr = VM_SHARED_HEAP_BASE;
        } else if (dwMaxUserAllocAddr < VM_DLL_BASE) {
            dwMaxUserAllocAddr = VM_DLL_BASE;
        }

    }

    DEBUGMSG (!pptbl, (L"KernelVMInit Failed, pptbl = %8.8lx, pprc->ppdir = %8.8lx\r\n", pptbl, g_pprcNK->ppdir));
    return NULL != pptbl;
}

//-----------------------------------------------------------------------------------------------------------------
//
// FreeAllPagesInPTBL: Decommit all pages in a page table
//
static void FreeAllPagesInPTBL (PCPAGETABLE pptbl)
{
    int idx;
    DWORD dwEntry;

    for (idx = 0; idx < VM_NUM_PT_ENTRIES; idx ++) {
        dwEntry = pptbl->pte[idx];
        if (IsPageCommitted (dwEntry)) {
            FreePhysPage (PFNfromEntry(dwEntry));
        }
    }
 }

//-----------------------------------------------------------------------------------------------------------------
//
// VMDelete: Delete per-process VM (called when a process is fully exited)
//
void VMDelete (PPROCESS pprc)
{
    PPAGEDIRECTORY ppdir = LockVM (pprc);
    if (ppdir) {

        PPAGETABLE pptbl = GetPageTable (ppdir, 0);

        // make sure that this VM is never the VM proc on any core
        if (pprc == pVMProc) {
            SwitchVM (g_pprcNK);
        }
        SendIPI (IPI_INVALIDATE_VM, (DWORD) pprc);

        pprc->ppdir = NULL;

        UnlockVM (pprc);

        // debug build softlog, useful to find cache issue
        SoftLog (0x11111111, pprc->dwId);
        // we know that VMInit will create the 0th pagetable 1st. If it doesn't exit,
        // there there is no 2nd-level page table.
        if (pptbl) {
            int idx = 0;
            // clear the User KPage entry, so we don't free it
            MDClearUserKPage (pptbl);

            for (idx = 0; idx < VM_LAST_USER_PDIDX; idx = NextPDEntry (idx)) {
                pptbl = GetPageTable (ppdir, idx);
                if (pptbl) {
                    FreeAllPagesInPTBL (pptbl);
                    FreePageTable (pptbl);
                }
            }

        }
        // free page directory
        FreePD (ppdir);

        InvalidatePages (pprc, 0, 0);

        // asid for the process is no long used
        MDFreeASID (pprc->bASID);

        // debug build softlog, useful to find cache issue
        SoftLog (0x11111112, pprc->dwId);
    }
 }

//-----------------------------------------------------------------------------------------------------------------
//
// VMCommit: commit memory (internal only)
//
LPVOID 
VMCommit (
    PPROCESS pprc,          // process
    LPVOID lpvaddr,         // starting address
    DWORD  cbSize,          // size, in byte, of the allocation
    DWORD  fProtect,
    DWORD  dwPageType       // VM_PT_XXX
    )
{
    DWORD dwErr = 0;
#ifdef DEBUG
    LPVOID pAddr = lpvaddr;
#endif
    // internal call, arguements must be 'perfect' aligned
    DEBUGCHK (!((DWORD) lpvaddr & VM_PAGE_OFST_MASK));

    lpvaddr = DoVMAlloc (pprc, 
                    (DWORD)lpvaddr, 
                    PAGECOUNT (cbSize), 
                    MEM_COMMIT,
                    fProtect,
                    dwPageType,
                    &dwErr);
    if (!lpvaddr) {
        KSetLastError (pCurThread, dwErr);
    }

    DEBUGMSG (!lpvaddr, (L"VMCommit failed to commit addr 0x%8.8lx, of size 0x%8.8lx\r\n", pAddr, cbSize));
    return lpvaddr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMAlloc: main function to allocate VM. 
//
LPVOID 
VMAlloc (
    PPROCESS pprc,          // process
    LPVOID lpvaddr,         // starting address
    DWORD  cbSize,          // size, in byte, of the allocation
    DWORD  fAllocType,      // allocation type
    DWORD  fProtect         // protection
    )
{
    DWORD    dwAddr = (DWORD) lpvaddr;
    DWORD    dwEnd  = dwAddr + cbSize;
    DWORD    dwErr  = 0;
    LPVOID   lpRet  = NULL;

    // verify arguments
    if (!IsValidAllocType (dwAddr, fAllocType)              // valid fAllocType?
        || ((int) cbSize <= 0)                              // valid size?
        || !IsValidProtect (fProtect)) {                    // valid fProtect?
        dwErr = ERROR_INVALID_PARAMETER;
        DEBUGMSG (ZONE_VIRTMEM, (L"VMAlloc failed, not valid parameter %8.8lx, %8.8lx %8.8lx %8.8lx\r\n",
            lpvaddr, cbSize, fAllocType, fProtect));
    } else {
        DWORD cPages;           // Number of pages

        // page align start address
        dwAddr &= ~VM_PAGE_OFST_MASK;
        cPages = PAGECOUNT (dwEnd - dwAddr);
        lpRet = DoVMAlloc (pprc, dwAddr, cPages, fAllocType, fProtect, PM_PT_ZEROED, &dwErr);

    }
    
    KSetLastError (pCurThread, dwErr);
    
    return lpRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMAllocPhys: allocate contiguous physical memory. 
//
LPVOID VMAllocPhys (PPROCESS pprc, DWORD cbSize, DWORD fProtect, DWORD dwAlignMask, DWORD dwFlags, PULONG pPhysAddr)
{
    LPVOID pAddr = NULL;
    DWORD  dwPfn = INVALID_PHYSICAL_ADDRESS;
    DWORD  dwErr = ERROR_INVALID_PARAMETER;

    DEBUGMSG (ZONE_VIRTMEM, (L"VMAllocPhys: %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
                pprc->dwId, cbSize, dwAlignMask, dwFlags, pPhysAddr));

    // validate parameters
    if ((pActvProc == g_pprcNK)     // kernel only API
        && ((int)cbSize > 0)        // valid size
        && IsValidProtect(fProtect) // valid protection
        && pPhysAddr                // valid address to hold returned physical address
        && !dwFlags) {              // dwflags must be 0
        
        DWORD cPages = PAGECOUNT (cbSize);
        pAddr = VMReserve (pprc, cbSize, 0, 0);
        if (!pAddr
            || ((dwPfn = GetContiguousPages (cPages, dwAlignMask, dwFlags)) == INVALID_PHYSICAL_ADDRESS)) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
            DEBUGMSG (ZONE_VIRTMEM, (L"VMAllocPhys: Couldn't get %d contiguous pages\r\n", cPages));
            
        } else {

            DEBUGMSG (ZONE_VIRTMEM, (L"VMAllocPhys: Got %d contiguous pages @ %8.8lx\r\n", cPages, dwPfn));
            // mapped as cacheable RW to clear the page first.
            VERIFY (VMCopyPhysical (pprc, (DWORD) pAddr, dwPfn, cPages, PageParamFormProtect (pprc, PAGE_READWRITE, (DWORD) pAddr), FALSE));
            __try {
                // we'll be zeroing the pages cached.
                memset (pAddr, 0, cPages << VM_PAGE_SHIFT);

                // VMProtect will flush the cache when changing protect to uncached.
                dwErr = DoVMProtect (pprc, (DWORD)pAddr, cPages, fProtect|PAGE_NOCACHE, NULL);

                if (!dwErr) {
                    *pPhysAddr = PFN2PA (dwPfn);
                    dwErr = VMAddAlloc (pprc, pAddr, cbSize, NULL, NULL)? 0 : ERROR_NOT_ENOUGH_MEMORY;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
        }

        if (dwErr && pAddr) {
            // free up VM reserved
            VMRelease (pprc, (DWORD) pAddr, cPages);
        }
        
    }

    KSetLastError (pCurThread, dwErr);

    DEBUGMSG (ZONE_VIRTMEM, (L"VMAllocPhys: returns %8.8lx, (dwErr = %8.8lx)\r\n",
                    pAddr, dwErr));

    return dwErr? NULL : pAddr;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMReserve: (internal only) reserve VM in DLL/SharedHeap/ObjectStore
//
LPVOID 
VMReserve (
    PPROCESS pprc,                  // which process
    DWORD   cbSize,                 // size of the reservation
    DWORD   fMemType,               // memory type (0, MEM_IMAAGE, MEM_MAPPED, MEM_AUTO_COMMIT)
    DWORD   dwSearchBase            // search base
                                    //      0 == same as VMAlloc (pprc, NULL, cbSize, MEM_RESERVE|fMemType, PAGE_NOACCESS)
                                    //      VM_DLL_BASE - reserve VM for DLL loading
                                    //      VM_SHARED_HEAP_BASE - reserve VM for shared heap
                                    //      VM_STATIC_MAPPING_BASE - reserve VM for static mapping
) {
    LPVOID lpRet = NULL;
    DWORD  cPages = PAGECOUNT (cbSize);

    if (LockVM (pprc)) {
        lpRet = (LPVOID) DoVMReserve (pprc, 0, cPages, dwSearchBase, fMemType);
        UnlockVM (pprc);
        CELOG_VirtualAlloc(pprc, (DWORD)lpRet, 0, cPages, fMemType, 0);
    }

    return lpRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMCreateKernelPageMapping: create mapping of a kernel page.
//         SOURCE ADDRESS MUST BE A KERNEL MODE ADDRESS.
// NOTE: physical page maintance of the mapping MUST BEN HANDLED EXPLICITLY IN THE CALLER FUNCTION.
//
BOOL VMCreateKernelPageMapping (LPVOID pAddr, DWORD dwPFN)
{
    PPAGETABLE pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (pAddr));

    if (pptbl) {
        DWORD idx2nd = VA2PT2ND(pAddr);
        pptbl->pte[idx2nd] = dwPFN | PG_KRN_READ_WRITE;
#ifdef ARM
        ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
    }

    return NULL != pptbl;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMRemoveKernelPageMapping: (interanl) remove the mapping of a kernel page.
//         SOURCE ADDRESS MUST BE A KERNEL MODE ADDRESS.
// NOTE: physical page maintance of the mapping MUST BEN HANDLED EXPLICITLY IN THE CALLER FUNCTION.
//
DWORD VMRemoveKernelPageMapping (LPVOID pPage)
{
    // NOTE: we do not need to acquire csVM here, for kernel 2nd-level page table is never freed. And
    //       since we have the (paging) pages reserved, no one should be changing the 2nd-level page
    //       table in this range.
    PPAGETABLE pptbl = GetPageTable (g_ppdirNK, VA2PDIDX(pPage));
    DWORD     idx2nd = VA2PT2ND (pPage);
    DWORD    dwEntry = pptbl->pte[idx2nd];
    DEBUGCHK (pptbl);
    pptbl->pte[idx2nd] = MakeReservedEntry (VM_PAGER_NONE);
#ifdef ARM
    ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif

    return IsPageCommitted (dwEntry)? PFNfromEntry (dwEntry) : INVALID_PHYSICAL_ADDRESS;
}

typedef struct {
    DWORD dwSrcAddr;
    DWORD dwPgProt;
} EnumMovePageStruct, *PEnumMovePageStruct;

static BOOL EnumMovePage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    BOOL fRet = !IsPageCommitted (*pdwEntry);
    if (fRet) {
        //
        // NOTE: WE DO NOT TAKE KERNEL VM's lock, for we know this range is reserved only by us and no one else
        //       should be touching it.
        //
        PEnumMovePageStruct pemps = (PEnumMovePageStruct) pEnumData;
        PPAGETABLE pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (pemps->dwSrcAddr));
        DWORD     idx2nd = VA2PT2ND (pemps->dwSrcAddr);
        DEBUGCHK (pptbl);

        if (IsPageCommitted (pptbl->pte[idx2nd])) {
            *pdwEntry = MakeCommittedEntry (PFNfromEntry (pptbl->pte[idx2nd]), pemps->dwPgProt);
            pptbl->pte[idx2nd] = MakeReservedEntry (VM_PAGER_NONE);
#ifdef ARM
            ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
        }

        pemps->dwSrcAddr += VM_PAGE_SIZE;
    }
    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMMove: (interanl) move the mapping from one VA to the other.
//          (1) SOURCE ADDRESS MUST BE A KERNEL MODE ADDRESS.
//          (2) Cache operation is performed based on the protection (fProtect) requested.
//
BOOL
VMMove (
    PPROCESS pprcDst,               // destination process
    DWORD  dwDstAddr,               // destination address
    DWORD  dwSrcAddr,               // source address (must be a kernel, non-static-mapped address)
    DWORD  cbSize,                  // size to move
    DWORD  fProtect                 // destination protection
)
{
    PPAGEDIRECTORY ppdir;
    BOOL fRet = FALSE;

    // must be in Kernel VirtualAlloc's addresses
    DEBUGCHK (IsInKVM (dwSrcAddr));
    // block offset must match
    DEBUGCHK ((dwSrcAddr & VM_BLOCK_OFST_MASK) == (dwDstAddr & VM_BLOCK_OFST_MASK));
    // must be page aligned
    DEBUGCHK (!(dwSrcAddr & VM_PAGE_OFST_MASK));

    // perform required cache operations
    if ((VM_EXECUTABLE_PROT & fProtect)
#ifdef ARM
        || IsVirtualTaggedCache ()
#endif
        ) {
        NKCacheRangeFlush ((LPVOID) dwSrcAddr, cbSize, CACHE_SYNC_WRITEBACK | CACHE_SYNC_INSTRUCTIONS | CSF_LOADER);
    }

    ppdir = LockVM (pprcDst);
    if (ppdir) {
        EnumMovePageStruct emps = { dwSrcAddr, PageParamFormProtect (pprcDst, fProtect, dwDstAddr) };
        fRet = Enumerate2ndPT (ppdir, dwDstAddr, PAGECOUNT (cbSize), FALSE, EnumMovePage, &emps);
        UnlockVM (pprcDst);
    }
    return fRet;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMCopy: main function to VirtualCopy VM, kernel only, not exposed to user mode apps
//
BOOL 
VMCopy (
    PPROCESS pprcDst,       // the destination process
    DWORD dwDestAddr,       // destination address
    PPROCESS pprcSrc,       // the source process, NULL if PAGE_PHYSICAL or same as destination
    DWORD dwSrcAddr,        // source address
    DWORD cbSize,           // size, in bytes
    DWORD fProtect          // protection
    )
{
    DWORD    dwErr = ERROR_INVALID_PARAMETER;
    BOOL     fPhys = (fProtect & PAGE_PHYSICAL);
    DWORD    dwOfstSrc = (fPhys? (dwSrcAddr << 8) : dwSrcAddr) & VM_PAGE_OFST_MASK;
    DWORD    dwOfstDest = dwDestAddr & VM_PAGE_OFST_MASK;
    BOOL     fRet = FALSE;

    fProtect &= ~PAGE_PHYSICAL;

    // verify arguments
    if (((pprcDst != g_pprcNK) && ((int) dwDestAddr < VM_USER_BASE))    // valid address?
        || ((int) cbSize <= 0)                                          // valid size?
        || (dwOfstSrc != dwOfstDest)                                    // not the same page offset
        || !IsValidProtect (fProtect)) {                                // valid fProtect?
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        // do the copy
        DWORD dwPgProt = PageParamFormProtect (pprcDst, fProtect, dwDestAddr);
        DWORD cPages = PAGECOUNT (cbSize + dwOfstDest); // Number of pages
        DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;

        dwDestAddr -= dwOfstDest;

        if (!pprcSrc)
            pprcSrc = pprcDst;

        if (fPhys) {
            dwPfn = PFNfrom256 (dwSrcAddr);
        } else if (IsKernelVa ((LPVOID) dwSrcAddr)) {
            // kernel address, use physical version of the copy function as they're section mapped
            dwPfn = GetPFN ((LPVOID) (dwSrcAddr-dwOfstSrc));
        }

        fRet = (INVALID_PHYSICAL_ADDRESS != dwPfn)
            ? VMCopyPhysical (pprcDst, dwDestAddr, dwPfn, cPages, dwPgProt, FALSE)
            : VMCopyVirtual (pprcDst, dwDestAddr, pprcSrc, dwSrcAddr - dwOfstSrc, cPages, dwPgProt, &dwErr);
    
        if (fRet) {
            CELOG_VirtualCopy(pprcDst, dwDestAddr, pprcSrc, dwSrcAddr, cPages, dwPfn, fProtect);
        }
    }

    if (!fRet) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: VMCopy - Failed, dwErr = %8.8lx\r\n", dwErr));
        KSetLastError (pCurThread, dwErr);
    }

    return fRet;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMFastCopy: (internal only) duplicate VM mapping from one to the other. 
// NOTE: (1) This is a fast function, no scanning, validation is performed. 
//       (2) Source and destination must both be Virtual addresses and must be page aligned.
//       (3) source and destination must have the same block offset, unless source is static-mapped kernel
//           address.
//       (4) Caller must be verifying all parameters before calling this.
//
BOOL VMFastCopy (
    PPROCESS pprcDst,       // the destination process
    DWORD dwDstAddr,        // destination address
    PPROCESS pprcSrc,       // the source process, NULL if PAGE_PHYSICAL or same as destination
    DWORD dwSrcAddr,        // source address
    DWORD cbSize,           // # of pages
    DWORD fProtect
    )
{
    BOOL fRet;
    DWORD dwPgProt = PageParamFormProtect (pprcDst, fProtect, dwDstAddr);
    DWORD cPages   = PAGECOUNT (cbSize);
    
    DEBUGCHK (!(dwDstAddr & VM_PAGE_OFST_MASK));
    DEBUGCHK (!(dwSrcAddr & VM_PAGE_OFST_MASK));
    DEBUGCHK (cPages);
    DEBUGCHK (IsKernelVa ((LPCVOID) dwSrcAddr)
              || ((dwDstAddr & VM_BLOCK_OFST_MASK) == (dwSrcAddr & VM_BLOCK_OFST_MASK)));
    
    if (IsKernelVa ((LPCVOID) dwSrcAddr)) {
        fRet = VMCopyPhysical (pprcDst, dwDstAddr, GetPFN ((LPCVOID) dwSrcAddr), cPages, dwPgProt, FALSE);

    } else {
        fRet = Lock2VM (pprcDst, pprcSrc);
        if (fRet) {
            PPAGEDIRECTORY ppdir = pprcSrc->ppdir;
            DWORD         idxdir = VA2PDIDX(dwSrcAddr);
            PPAGETABLE     pptbl = GetPageTable (ppdir, idxdir);
            VCVirtStruct    vcvs = { ppdir, pptbl, idxdir, VA2PT2ND(dwSrcAddr), dwPgProt, 0 };

            LockPhysMem ();
            fRet = Enumerate2ndPT (pprcDst->ppdir, dwDstAddr, cPages, 0, VCVirtPages, &vcvs);
            UnlockPhysMem ();

            UnlockVM (pprcSrc);
            UnlockVM (pprcDst);
        }
    }

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  VCCommittedPage: 2nd level page table enumeration funciton for duplicate VM mapping for partially committed range.
//
static BOOL VCCommittedPage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    BOOL            fRet        = !IsPageCommitted (*pdwEntry);
    PVCVirtStruct   pvcvs       = (PVCVirtStruct) pEnumData;
    DWORD           dwSrcEntry  = pvcvs->pptbl->pte[pvcvs->idx2nd];

    // create the mapping in destination if the source is commited. 
    if (fRet && IsPageCommitted (dwSrcEntry)) {

        DWORD dwPfn = PFNfromEntry (dwSrcEntry);
        fRet = DupPhysPage (dwPfn);
        
        if (fRet) {
            *pdwEntry = MakeCommittedEntry (dwPfn, pvcvs->dwPgProt);
        }
    }
    // move on to the next page
    if (VM_NUM_PT_ENTRIES == ++ pvcvs->idx2nd) {
        pvcvs->idx2nd = 0;
        pvcvs->idxdir = NextPDEntry (pvcvs->idxdir);
        pvcvs->pptbl  = GetPageTable (pvcvs->ppdir, pvcvs->idxdir);
    }

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMCopyCommittedPages: (internal only) duplicate VM mapping from kernel to a process at same address.
//      used primarily for mapping DLL addresses.
//
BOOL VMCopyCommittedPages (
    PPROCESS pprc,          // the destination process
    DWORD    dwAddr,        // destination address
    DWORD    cbSize,        // size
    DWORD    fProtect
    )
{
    BOOL            fRet;
    DWORD           dwPgProt    = PageParamFormProtect (pprc, fProtect, dwAddr);
    DWORD           cPages      = PAGECOUNT (cbSize);
    PPAGEDIRECTORY  ppdir       = g_ppdirNK;
    DWORD           idxdir      = VA2PDIDX(dwAddr);
    PPAGETABLE      pptbl       = GetPageTable (ppdir, idxdir);
    VCVirtStruct    vcvs        = { ppdir, pptbl, idxdir, VA2PT2ND(dwAddr), dwPgProt, 0 };

    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));
    DEBUGCHK (cPages);
    DEBUGCHK (pptbl);

    Lock2VM (pprc, g_pprcNK);
    
    LockPhysMem ();
    fRet = Enumerate2ndPT (pprc->ppdir, dwAddr, cPages, 0, VCCommittedPage, &vcvs);
    UnlockPhysMem ();

    UnlockVM (g_pprcNK);
    UnlockVM (pprc);

    return fRet;
}



//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeAndRelease - decommit and release VM
//
BOOL
VMFreeAndRelease (PPROCESS pprc, LPVOID lpvAddr, DWORD cbSize)
{
    DWORD dwOfst = (DWORD)lpvAddr & VM_BLOCK_OFST_MASK;
    return !VMRelease (pprc, (DWORD)lpvAddr-dwOfst, PAGECOUNT (cbSize+dwOfst));
}

typedef struct _QueryEnumStruct {
    DWORD dwMatch;
    DWORD cPages;
} QueryEnumStruct, *PQueryEnumStruct;

//-----------------------------------------------------------------------------------------------------------------
//
// CountSamePages: 2nd level PT enumeration funciton. check if all entries are committed or all of them are uncommited.
//
static BOOL CountSamePages (const DWORD *pdwEntry, LPVOID pEnumData)
{
    PQueryEnumStruct pqe = (PQueryEnumStruct) pEnumData;

    if (IsSameEntryType (*pdwEntry, pqe->dwMatch)) {
        pqe->cPages ++;
        return TRUE;
    }

    // not match, stop enumeration
    return FALSE;
}

static BOOL EnumCheckThreadStack (PCDLIST pdl, LPVOID pParam)
{
    PMEMORY_BASIC_INFORMATION pmi = (PMEMORY_BASIC_INFORMATION) pParam;
    PCTHREAD pth = (PCTHREAD) pdl;
    BOOL fRet = ((DWORD) pmi->BaseAddress - pth->dwOrigBase < pth->dwOrigStkSize);

    if (fRet) {
        pmi->AllocationBase = (LPVOID) pth->dwOrigBase;
        pmi->RegionSize     = pth->dwOrigStkSize;
    }

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMQuery: function to Query VM
//
DWORD
VMQuery (
    PPROCESS pprc,                      // process
    LPCVOID lpvaddr,                    // address to query
    PMEMORY_BASIC_INFORMATION  pmi,     // structure to fill
    DWORD  dwLength                     // size of the buffer allocate for pmi
)
{
    DWORD    dwAddr = (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK;
    DWORD    cbRet  = sizeof (MEMORY_BASIC_INFORMATION);
    DWORD    dwErr  = 0;

    //
    // NOTE: pointer validation (pmi) must have been done before calling this function 
    //
    
    // verify arguments
    if ((int) dwLength < sizeof (MEMORY_BASIC_INFORMATION)) {   // valid size?
        dwErr = ERROR_BAD_LENGTH;
        
    } else if (dwAddr < VM_USER_BASE) {                         // valid address?
        dwErr = ERROR_INVALID_PARAMETER;

    } else {

        DWORD           idxdir = VA2PDIDX (dwAddr);
        DWORD           idx2nd = VA2PT2ND (dwAddr);
        PPAGEDIRECTORY  ppdir;
        PPAGETABLE      pptbl;
        DWORD           cPages;

        pmi->AllocationBase = NULL;
        pmi->BaseAddress    = (LPVOID) dwAddr;
        pmi->RegionSize     = 0;

        // check stack 1st if it's not from kernel
        if (pprc != g_pprcNK) {
            LockLoader (pprc);
            EnumerateDList (&pprc->thrdList, EnumCheckThreadStack, pmi);
            UnlockLoader (pprc);
        }

        ppdir = LockVM (pprc);
        if (ppdir) {
        
            pptbl = GetPageTable (ppdir, idxdir);
            if (!pptbl || (VM_FREE_PAGE == pptbl->pte[idx2nd])) {

                // dwAddr is in a free region

                // count the # of free pages from next block
                cPages = CountFreePages (ppdir, (dwAddr+VM_BLOCK_SIZE) & ~VM_BLOCK_OFST_MASK, VM_MAX_PAGES);

                // add the # of free pages in this block            
                cPages += VM_PAGES_PER_BLOCK - (idx2nd % VM_PAGES_PER_BLOCK);

                // update the structure
                pmi->AllocationBase = NULL;
                pmi->Protect        = PAGE_NOACCESS;
                pmi->State          = MEM_FREE;
                pmi->Type           = MEM_PRIVATE;
                
            } else {
            
                // dwAddr is in a reserved region
                DWORD dwEntry = pptbl->pte[idx2nd];
                QueryEnumStruct qe = { dwEntry, 0 };
                PVALIST pvalc;
                DWORD   dwEnd;
                
                // enumerate PT to find all matched entries
                Enumerate2ndPT (ppdir, dwAddr, VM_MAX_PAGES, 0, CountSamePages, &qe);

                // calculate the end address of the region found
                dwEnd = dwAddr + (qe.cPages << VM_PAGE_SHIFT);
                    
                if (pmi->AllocationBase) {
                    // stack
                    if (dwEnd > (DWORD) pmi->AllocationBase + pmi->RegionSize) {
                        dwEnd = (DWORD) pmi->AllocationBase + pmi->RegionSize;
                    }
                    
                } else {
                    // not stack, search VA list.
                    
                    // initialize allocation base to be beginning of the block
                    pmi->AllocationBase = (PVOID) (dwAddr & ~VM_BLOCK_OFST_MASK);

                    // try to find the reservation base, and adjust end address if needed

                    // search virtual allocation list to get more allocation information
                    for (pvalc = pprc->pVaList; pvalc; pvalc = pvalc->pNext) {
                        DWORD dwBase = (DWORD) pvalc->pVaBase;

                        if (dwAddr - dwBase < pvalc->cbVaSize) {
                            // found virtual allocation
                            // update allocation base
                            pmi->AllocationBase = (PVOID) dwBase;
                            // update end address if beyond the allocation
                            if (dwEnd > dwBase + pvalc->cbVaSize) {
                                dwEnd = dwBase + pvalc->cbVaSize;
                            }
                            break;
                        }
                        if ((dwBase > dwAddr) && (dwEnd > dwBase)) {
                            // an separate allocation found between dwAddr and dwEnd
                            // update dwEnd
                            dwEnd = dwBase;
                        }
                        
                    }
                }

                // update cPages
                cPages = PAGECOUNT (dwEnd - dwAddr);

                // update protect/state/type
                if (IsPageCommitted (dwEntry)) {
                    pmi->Protect    = ProtectFromEntry (dwEntry);
                    pmi->State      = MEM_COMMIT;
                    pmi->Type       = MemTypeFromAddr (pprc, dwAddr);
                } else {
                    pmi->Protect    = PAGE_NOACCESS;
                    pmi->State      = MEM_RESERVE;
                    pmi->Type       = MemTypeFromReservation (dwEntry);
                }
            }

            UnlockVM (pprc);

            // common part
            pmi->AllocationProtect  = PAGE_NOACCESS;     // allocation protect always PAGE_NOACCESS
            pmi->RegionSize         = cPages << VM_PAGE_SHIFT;

        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
        cbRet = 0;
    }

    return cbRet;
}

// section map is always created with kernel r/w, user no access, cached
LPVOID
VMCreateSectionMap (
    DWORD pa256,            // physical address >> 8
    DWORD cbSize,           // size of the section map
    DWORD dwPgProt)         // protection bits
{
    LPVOID pAddr = NULL;
    
    DEBUGCHK (IsSectionAligned (pa256 << 8));
    DEBUGCHK (IsSectionAligned (cbSize));

    return pAddr;
}

BOOL
VMRemoveSectionMap (
    LPVOID pAddr,           // physical address >> 8
    DWORD cbSize)           // size of the section map
{
    BOOL fRet = FALSE;
    
    DEBUGCHK (IsSectionAligned (pAddr));
    DEBUGCHK (IsSectionAligned (cbSize));

    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMLockPages: function to lock VM, kernel only, not exposed to user mode apps
//
BOOL
VMLockPages (
    PPROCESS pprc,              // process
    LPVOID lpvaddr,             // address to query
    DWORD  cbSize,              // size to lock
    LPDWORD pPFNs,              // the array to retrieve PFN
    DWORD   fOptions            // options: see LOCKFLAG_*
)
{
    DWORD dwAddr = (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK;
    DWORD dwErr = 0;

    DEBUGMSG (pCurThread && !pCurThread->bDbgCnt && ZONE_VIRTMEM, (L"VMLockPages (%8.8lx, %8.8lx, %8.8lx, %8.8lx, %8.8lx), pprc->csVM.OwnerThread = %8.8lx\r\n",
            pprc, lpvaddr, cbSize, pPFNs, fOptions, pprc->csVM.OwnerThread));
//    if (((pprc != g_pprcNK) && ((int) dwAddr < VM_USER_BASE))       // valid address?
    if ((dwAddr < VM_USER_BASE)             // valid address?
        || ((int) cbSize <= 0)              // valid size?
        || !IsValidLockOpt (fOptions)) {    // valid options?
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else {
        DWORD cPages = PAGECOUNT (cbSize + ((DWORD) lpvaddr - dwAddr));

        // special treatment for kernel addresses
        if (IsKernelVa (lpvaddr)) {
            // kernel address, section mapped
            if (!IsKernelVa ((LPBYTE) lpvaddr + cbSize)) {
                dwErr = ERROR_INVALID_PARAMETER;
            } else if (pPFNs) {
                DWORD dwPFN;
                for (dwPFN = GetPFN ((LPVOID) dwAddr); cPages --; pPFNs ++, dwPFN = NextPFN (dwPFN)) {
                    *pPFNs = dwPFN;
                }
            }
        } else {
            dwErr = DoVMLockPages (pprc, dwAddr, cPages, pPFNs, fOptions);
        }
    }


    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return !dwErr;
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMUnlockPages: function to unlock VM, kernel only, not exposed to user mode apps
//
BOOL
VMUnlockPages (
    PPROCESS pprc,              // process
    LPVOID lpvaddr,             // address to query
    DWORD  cbSize               // size to lock
)
{
    DWORD dwAddr = (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK;
    DWORD dwErr = 0;

    DEBUGMSG (pCurThread && !pCurThread->bDbgCnt && ZONE_VIRTMEM, (L"VMUnlockPages (%8.8lx, %8.8lx, %8.8lx)\r\n",
        pprc, lpvaddr, cbSize));
    
//    if (((pprc != g_pprcNK) && ((int) dwAddr < VM_USER_BASE))       // valid address?
    if ((dwAddr < VM_USER_BASE)             // valid address?
        || ((int) cbSize <= 0)) {           // valid size?
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else if (!IsKernelVa (lpvaddr)) {
        DWORD cPages = PAGECOUNT (cbSize + ((DWORD) lpvaddr - dwAddr));
        dwErr = DoVMUnlockPages (pprc, dwAddr, cPages);
        
    }


    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return !dwErr;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMProtect: main function to change VM protection. 
//
BOOL 
VMProtect (
    PPROCESS pprc,              // process
    LPVOID lpvaddr,             // starting address
    DWORD  cbSize,              // size, in byte, of the allocation
    DWORD  fNewProtect,         // new protection
    LPDWORD pfOldProtect        // old protect value
    )
{
    DWORD dwAddr = (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK;
    DWORD cPages = PAGECOUNT (cbSize + ((DWORD) lpvaddr - dwAddr));
    DWORD dwErr = 0;

    DEBUGCHK ((pprc == g_pprcNK) || ((int) dwAddr >= VM_USER_BASE));
    DEBUGCHK ((int) cbSize > 0);
    
    if (!KSEN_VirtualProtect (pprc, dwAddr, cPages, fNewProtect) && !IsValidProtect (fNewProtect)) {      // valid protection?
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else {
        dwErr = DoVMProtect (pprc, dwAddr, cPages, fNewProtect, pfOldProtect);
    }


    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    KSLV_VirtualProtect (pprc, dwAddr, cPages, fNewProtect, &dwErr);
    return !dwErr;
}

static DWORD g_pfnVoidPages[VM_PAGES_PER_BLOCK];      // 16 pages

static BOOL EnumMapToVoid (LPDWORD pdwEntry, LPVOID pEnumData)
{
    LPDWORD pdwAddr = (LPDWORD) pEnumData;
    DWORD   dwEntry = *pdwEntry;

    if (IsPageCommitted (dwEntry)) {
        // redirect page table entries to void pages
        *pdwEntry = g_pfnVoidPages[(*pdwAddr >> VM_PAGE_SHIFT) & 0xf] | (*pdwAddr & VM_PAGE_OFST_MASK);

        // return the phys page back to phys memory pool
        FreePhysPage (PFNfromEntry (dwEntry));
    }
    
    *pdwAddr += VM_PAGE_SIZE;   // move to next page
    return TRUE;                // keep enumerating
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMMapUserAddrToVoid: remap user mode address to "void".
//
BOOL
VMMapUserAddrToVoid (
    PPROCESS pprc,              // process
    LPVOID lpvaddr,             // starting address
    DWORD  cbSize               // size, in byte, of the range to be remapped
    )
{
    DWORD dwErr  = ERROR_INVALID_PARAMETER;

    if (((int) cbSize > 0)
        && (pprc != g_pprcNK)
        && VMFindAlloc (pprc, lpvaddr, cbSize, NULL)) {

        DWORD dwAddr = (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK;
        DWORD cPages = PAGECOUNT (cbSize + ((DWORD) lpvaddr - dwAddr));
        PPAGEDIRECTORY ppdir = LockVM (pprc);
        
        if (ppdir) {

            if (!VMIsPagesLocked (pprc, dwAddr, cPages)) {

                // use bottom bits to encode page protection
                dwAddr |= PageParamFormProtect (pprc, PAGE_READWRITE, dwAddr);

                VERIFY (Enumerate2ndPT (ppdir, dwAddr, cPages, TRUE, EnumMapToVoid, &dwAddr));

                // dwAddr is changed inside EnumMapToVoid, use lpvaddr to get to the original address.
                InvalidatePages (pprc, (DWORD) lpvaddr & ~VM_PAGE_OFST_MASK, cPages);

                dwErr = 0;
            }
            UnlockVM (pprc);

        }
        
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    return !dwErr;
    
}

#ifdef ARM

void VMAddUncachedAllocation (PPROCESS pprc, DWORD dwAddr, PVALIST pUncached)
{
    if (LockVM (pprc)) {

        PVALIST pVaItem = NULL;

        if (!FindVAItem (pprc->pUncachedList, dwAddr)) {

            // find the reservation of the address to be uncached
            pVaItem = FindVAItem (pprc->pVaList, dwAddr);

            if (!pVaItem) {
                // Someone VirtualFree'd a Virtual Allocation *before* VirtualAlloc returned.
                // Most likely freeing a dangling pointer, or malicious attack. Don't need to
                // keep track of it.
                DEBUGCHK (0);
            } else {
                pUncached->cbVaSize = pVaItem->cbVaSize;
                pUncached->pVaBase  = pVaItem->pVaBase;
                pUncached->pNext    = pprc->pUncachedList;
                pprc->pUncachedList = pUncached;
            }
        }
        UnlockVM (pprc);

        if (!pVaItem) {
            // allocation already exists in uncached list, or the debugchk above. Free pUncached
            FreeMem (pUncached, HEAP_VALIST);
        }
    }
}


typedef struct {
    PPROCESS pprc;
    DWORD    dwAddr;
} EnumAliasStruct, *PEnumAliasStruct;

void KC_UncacheOnePage (LPDWORD pdwEntry, LPVOID pAddr)
{
    KCALLPROFON (0);
    *pdwEntry &= ~PG_CACHE;
    OEMCacheRangeFlush (pAddr, VM_PAGE_SIZE, CACHE_SYNC_DISCARD | CACHE_SYNC_FLUSH_D_TLB);
    KCALLPROFOFF (0);
}

PREGIONINFO GetRegionFromPFN (DWORD dwPfn);

static BOOL UncachePage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PEnumAliasStruct peas = (PEnumAliasStruct) pEnumData;

    DEBUGCHK (IsPageCommitted (*pdwEntry));

    if (   (*pdwEntry & PG_CACHE)      // cache enabled
        && GetRegionFromPFN (PFNfromEntry (*pdwEntry))) {   // RAM address
        // we only need to flush cache if we're current VM, or kernel address
        if ((peas->pprc == g_pprcNK) || (peas->pprc == pVMProc) || !pCurThread->pprcVM) {
            // we need to do flush+turning off cache in a KCall. Otherwise we'll get into trouble
            // if we got preempted and the page got brought into cache again.
            KCall ((PKFN) KC_UncacheOnePage, pdwEntry, peas->dwAddr);
        } else {
            // The VM is not active, there is no way there's anything of this address in cache.
            // just remove the cache bit
            *pdwEntry &= ~PG_CACHE;
        }

        DEBUGMSG (ZONE_VIRTMEM, (L"Page at %8.8lx:%8.8lx is uncached\r\n", peas->pprc->dwId, peas->dwAddr));
    }

    peas->dwAddr += VM_PAGE_SIZE;
    return TRUE;
}


BOOL
VMAlias (
    PALIAS   pAlias,
    PPROCESS pprc,
    DWORD    dwAddr,
    DWORD    cbSize,
    DWORD    fProtect)
{
    BOOL fRet;

    DEBUGCHK (!(dwAddr & VM_PAGE_OFST_MASK));
    DEBUGCHK ((int) cbSize > 0);
    DEBUGCHK (IsVirtualTaggedCache ());

    if (dwAddr >= VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    }

    // once we turn views uncached, we can't turn cache back on (probably possible
    // with some very complicated scanning). We'll investigate the possibility in the
    // future if this turns out to be a perf killer.
    fRet = MAPUncacheViews (pprc, dwAddr, cbSize);
    if (!fRet) {

        // TBD - perf improvement
        // alias to code, don't bother turning off cache.
        //
        //if (!(fProtect & PAGE_READWRITE) && IsCodeAddr (pAddr, cbSize)) {
        //    return TRUE;
        //}

        PPAGEDIRECTORY ppdir = LockVM (pprc);

        if (ppdir) {
            BOOL  fAliased = FALSE;

            fRet = IsVMAllocationAllowed (pprc);
            DEBUGCHK (fRet);    // VirtualAllocCopy to a process that is in final stage of exiting...
            
            if (fRet) {
                EnumAliasStruct eas = { pprc, dwAddr };

                fAliased = Enumerate2ndPT (ppdir, dwAddr, PAGECOUNT (cbSize), FALSE, UncachePage, &eas);
            }
            UnlockVM (pprc);

            if (fAliased) {
                pAlias->dwAliasBase = dwAddr;
                pAlias->cbSize      = cbSize;
                pAlias->dwProcessId = pprc->dwId;
            }
        }
    }

    return fRet;
}

static BOOL TryCachePage (LPDWORD pdwEntry, LPVOID pEnumData)
{

    if (!(*pdwEntry & PG_CACHE)) {

        PEnumAliasStruct peas = (PEnumAliasStruct) pEnumData;

        // dont' cache it if it's committed uncached.
        if (!FindVAItem (peas->pprc->pUncachedList, peas->dwAddr)) {

            DWORD            dwPfn = PFNfromEntry (*pdwEntry);
            
            DEBUGCHK (OwnCS (&PhysCS));
            
            if (IsPageCommitted (*pdwEntry)) {
                
                PREGIONINFO pfi = GetRegionFromPFN (dwPfn);

                if (pfi) {
                
                    uint ix = IDX_FROM_PFN (pfi, dwPfn);
                    
                    if (1 == pfi->pUseMap[ix].refCnt) {
                        *pdwEntry |= PG_CACHE;
                        DEBUGMSG (ZONE_VIRTMEM, (L"Turning cache back on for page at %8.8lx:%8.8lx\r\n", peas->pprc->dwId, peas->dwAddr));
                    }
                }
            }
        }

        peas->dwAddr += VM_PAGE_SIZE;
    }

    return TRUE;
}

BOOL VMUnalias (PALIAS pAlias)
{
    if (pAlias) {

        if (pAlias->dwAliasBase) {
            PHDATA phd = LockHandleData ((HANDLE) pAlias->dwProcessId, g_pprcNK);
            PPROCESS pprc = GetProcPtr (phd);

            DEBUGCHK (!(pAlias->dwAliasBase & VM_PAGE_OFST_MASK));
            DEBUGCHK (pAlias->cbSize > 0);
            if (pprc) {

                PPAGEDIRECTORY ppdir = LockVM (pprc);
                
                if (ppdir) {
                    EnumAliasStruct eas = {pprc, pAlias->dwAliasBase};

                    LockPhysMem ();
                    Enumerate2ndPT (ppdir, pAlias->dwAliasBase, PAGECOUNT (pAlias->cbSize), FALSE, TryCachePage, &eas);
                    UnlockPhysMem ();

                    UnlockVM (pprc);
                }
            }
            UnlockHandleData (phd);
        }
        FreeMem (pAlias, HEAP_ALIAS);
    }
    return TRUE;
}

#endif

static DWORD PfnFromPDEntry (DWORD dwPDEntry)
{
    DEBUGCHK (dwPDEntry);
    
#if defined (x86) || defined (ARM)
    return PFNfromEntry (dwPDEntry);
#else
    return GetPFN ((LPVOID) dwPDEntry);
#endif
}

static void DoMappPD (PPAGETABLE pptbl, PCPAGEDIRECTORY ppdir, DWORD idxPT, 
                DWORD idxPDStart, DWORD idxPDEnd, LPBYTE pdbits)
{
    DWORD dwPfn;

#ifdef ARM
    // ARM - page table/directory access need to be uncached unless override by dwPageTableCacheBits.
    DWORD dwPgProt = PageParamFormProtect (pVMProc, PAGE_READONLY|PAGE_NOCACHE, VM_PD_DUP_ADDR)|g_pOemGlobal->dwPageTableCacheBits;
#else
    DWORD dwPgProt = PageParamFormProtect (pVMProc, PAGE_READONLY, VM_PD_DUP_ADDR);
#endif
    LockPhysMem ();
    for ( ; idxPDStart < idxPDEnd; idxPT ++, idxPDStart = NextPDEntry (idxPDStart)) {
        if (ppdir->pte[idxPDStart]
            && ((dwPfn = PfnFromPDEntry (ppdir->pte[idxPDStart])) != INVALID_PHYSICAL_ADDRESS)
            && DupPhysPage (dwPfn)) {
            PREFAST_DEBUGCHK (idxPT < 512); // no more than 512 page tables
            pptbl->pte[idxPT] = MakeCommittedEntry (dwPfn, dwPgProt);
            pdbits[idxPT] = 1;
        }
    }
    UnlockPhysMem ();
    
}

static DWORD VMMapKernelPD (PPROCMEMINFO ppmi)
{
    DWORD dwRet = 0;
    PPROCESS pprc = pVMProc;
       
    // We don't need to lock kernel VM because pagetables of kernel never got freed.
    PPAGEDIRECTORY ppdir = LockVM (pprc);
    
    DEBUGCHK (ppdir);
    dwRet = DoVMReserve (pprc, VM_PD_DUP_ADDR, PAGECOUNT (VM_PD_DUP_SIZE), 0, 0);
    if (dwRet) {

        PPAGETABLE pptbl = GetPageTable (ppdir, VA2PDIDX(dwRet));
        DEBUGCHK (pptbl);

        // Map from shared heap area (0x70000000 - 0x80000000)
        DoMappPD (pptbl, g_ppdirNK, 
                        0,
                        VA2PDIDX (VM_SHARED_HEAP_BASE),
                        VA2PDIDX (VM_KMODE_BASE),
                        ppmi->pdbits);

        // Map from kernel XIP DLL area, object store, and kernel VM (0xC0000000 - 0xF0000000 (or 0xE0000000 on SHx))
        DoMappPD (pptbl, g_ppdirNK, 
                        (g_pKData->dwKVMStart - VM_SHARED_HEAP_BASE) >> 22,
                        VA2PDIDX (g_pKData->dwKVMStart),
                        VA2PDIDX (VM_CPU_SPECIFIC_BASE),
                        ppmi->pdbits);
        

#ifdef ARM
        ARMPteUpdateBarrier (&pptbl->pte[0], PAGECOUNT (VM_PD_DUP_SIZE));
#endif
    
        
    }
    
    UnlockVM (pprc);

    ppmi->fIsKernel = TRUE;

    return dwRet;
}

static DWORD VMMapAppPD (PPROCESS pprc, PPROCMEMINFO ppmi)
{
    DWORD dwRet = 0;
    PPROCESS pprcDest = pVMProc;

    if (Lock2VM (pprc, pprcDest)) {

        DEBUGCHK (pprc->ppdir && pprcDest->ppdir);
        dwRet = DoVMReserve (pprcDest, VM_PD_DUP_ADDR, PAGECOUNT (VM_PD_DUP_SIZE), 0, 0);
        if (dwRet) {
            PPAGETABLE pptbl = GetPageTable (pprcDest->ppdir, VA2PDIDX(dwRet));
            DEBUGCHK (pptbl);
            
            // Dup from user address area (0 - 0x6fc00000)
            DoMappPD (pptbl, pprc->ppdir, 
                            0,
                            0,
                            VA2PDIDX (VM_PD_DUP_ADDR),
                            ppmi->pdbits);
#ifdef ARM
            ARMPteUpdateBarrier (&pptbl->pte[0], PAGECOUNT (VM_PD_DUP_SIZE));
#endif
    
        
        }        
        
        UnlockVM (pprc);
        UnlockVM (pprcDest);
    }

    ppmi->dwExeEnd = (DWORD) pprc->BasePtr + pprc->e32.e32_vsize;

    return dwRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMDupPageDirectory: map page directory of a process. The mapping of the page-directory
//                     is read-only. 
//
// NOTE: For security purpose, we force the mapping to be at a fixed addresss, such that the page
//       protection cannot be changed. As a result, there can be only one mapping at any given time.
//
LPVOID VMMapPD (DWORD dwProcId, PPROCMEMINFO ppmi)
{
    DWORD dwRet = 0;
    PROCMEMINFO pmiLocal = {0};
    
    if (dwProcId == g_pprcNK->dwId) {
        dwRet = VMMapKernelPD (&pmiLocal);
    } else {
        PHDATA phd = LockHandleData ((HANDLE) dwProcId, g_pprcNK);
        PPROCESS pprc = GetProcPtr (phd);

        if (pprc) {
            dwRet = VMMapAppPD (pprc, &pmiLocal);
        }
        UnlockHandleData (phd);
    }

    if (dwRet) {
        CeSafeCopyMemory (ppmi, &pmiLocal, sizeof (pmiLocal));
    }

    return (LPVOID) dwRet;
}

BOOL VMUnmapPD (LPVOID pDupPD)
{
    return (VM_PD_DUP_ADDR == (DWORD) pDupPD)
        ? VMFreeAndRelease (pVMProc, pDupPD, VM_PD_DUP_SIZE)
        : FALSE;
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeExcessStacks: (internal only) free excess cached stack
//
void VMFreeExcessStacks (long nMaxStks)
{
    PSTKLIST pStkLst;

    while (g_nStackCached > nMaxStks) {
        pStkLst = InterlockedPopList (&g_pStkList);
        if (!pStkLst) {
            break;
        }
        // decrement of count must occur after pop
        DEBUGMSG (ZONE_MEMORY, (L"VMFreeExcessStacks %8.8lx, size %8.8lx\r\n", pStkLst->pStkBase, pStkLst->cbStkSize));
        DEBUGCHK (KRN_STACK_SIZE == pStkLst->cbStkSize);
        InterlockedDecrement (&g_nStackCached);
        VERIFY (VMFreeAndRelease (g_pprcNK, pStkLst->pStkBase, pStkLst->cbStkSize));
        FreeMem (pStkLst, HEAP_STKLIST);
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMCacheStack: (internal only) cache a stack for future use
// NOTE: callable inside KCall
//
void VMCacheStack (PSTKLIST pStkList, DWORD dwBase, DWORD cbSize)
{
    pStkList->cbStkSize = cbSize;
    pStkList->pStkBase  = (LPVOID)dwBase;
    // must increment cnt before push
    InterlockedIncrement (&g_nStackCached);
    InterlockedPushList (&g_pStkList, pStkList);
}

//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeStack: (internal only) free or cache a stack for later use
//
void VMFreeStack (PPROCESS pprc, DWORD dwBase, DWORD cbSize)
{
    PSTKLIST pStkList;
    if (   (pprc != g_pprcNK)
        || (KRN_STACK_SIZE != cbSize)
        || (g_nStackCached >= MAX_STK_CACHE)
        || (NULL == (pStkList = AllocMem (HEAP_STKLIST)))) {
        VMFreeAndRelease (pprc, (LPVOID) dwBase, cbSize);
    } else {
        VMCacheStack (pStkList, dwBase, cbSize);
    }
}


//-----------------------------------------------------------------------------------------------------------------
//
// VMCreateStack: (internal only) Create a stack and commmit the bottom-most page
//
LPBYTE
VMCreateStack (
    PPROCESS pprc,
    DWORD cbSize
    )
{
    PSTKLIST pStkLst  = NULL;
    LPBYTE   pKrnStk, pUsrStk;

    DEBUGCHK (!(cbSize & VM_BLOCK_OFST_MASK));   // size must be multiple of 64K

    // try to use cached stacks if it's for kernel
    if ((KRN_STACK_SIZE == cbSize)
        && (pprc == g_pprcNK)
        && (NULL != (pStkLst = InterlockedPopList (&g_pStkList)))) {
        InterlockedDecrement (&g_nStackCached);
    }
    
    if (pStkLst) {
        // got an old stack
        pKrnStk = pStkLst->pStkBase;

        DEBUGCHK (pKrnStk);
        DEBUGCHK (!((DWORD)pKrnStk & VM_BLOCK_OFST_MASK));
        
        // free pStkLst
        FreeMem (pStkLst, HEAP_STKLIST);

    // no old stack available, create a new one
    } else {

        pKrnStk = (LPBYTE) VMReserve (g_pprcNK, cbSize, MEM_AUTO_COMMIT, 0);
        if (pKrnStk) {
            if (((DWORD) pKrnStk < MAX_STACK_ADDRESS)
                && VMCommit (g_pprcNK, pKrnStk + cbSize - VM_PAGE_SIZE, VM_PAGE_SIZE, PAGE_READWRITE, PM_PT_ZEROED)) {
                // initialize stack
            } else {
                // commit stack failed - out of memory
                VMFreeAndRelease (g_pprcNK, pKrnStk, cbSize);
                pKrnStk = NULL;
            }
        }
    }

    // initialize return value
    pUsrStk = pKrnStk;

    if (pUsrStk) {

#ifdef x86
        // x86 has FPU status saved on stack, and we need to re-initialize it.
        MDInitStack (pKrnStk, cbSize);
#endif
        // NOTE: NEVER ACCESS *pUsrStk in this function, or it can cause cache in-consistency

        if ((pprc == g_pprcNK)
            || (NULL != (pUsrStk = VMReserve (pprc, cbSize, MEM_AUTO_COMMIT, 0)))) {
        
            LPDWORD  tlsKrn = TLSPTR (pKrnStk, cbSize);
            LPDWORD  tlsUsr = TLSPTR (pUsrStk, cbSize);
            DWORD    stkBound;
            
            if (pStkLst) {
                // zero TLS/CRTGLOBAL if old stack
                DEBUGCHK (pprc == g_pprcNK);
                stkBound = tlsKrn[PRETLS_STACKBOUND];
                memset (tlsKrn-PRETLS_RESERVED, 0, sizeof(DWORD)*(TLS_MINIMUM_AVAILABLE+PRETLS_RESERVED));
                memset (CRTGlobFromTls (tlsKrn), 0, sizeof (crtGlob_t));
            } else {
                stkBound = (DWORD) tlsUsr & ~VM_PAGE_OFST_MASK;
            }

            tlsKrn[PRETLS_STACKBOUND] = stkBound;
            tlsKrn[PRETLS_STACKBASE]  = (DWORD) pUsrStk;
            tlsKrn[PRETLS_STACKSIZE]  = cbSize;
            tlsKrn[TLSSLOT_RUNTIME]   = (DWORD) CRTGlobFromTls (tlsUsr);
            tlsKrn[PRETLS_TLSBASE]    = (DWORD) tlsUsr;
        }

        if (pUsrStk != pKrnStk) {
            if (pUsrStk) {
                VERIFY (VMFastCopy (pprc, (DWORD) pUsrStk + cbSize - VM_PAGE_SIZE, g_pprcNK, (DWORD) pKrnStk + cbSize - VM_PAGE_SIZE, VM_PAGE_SIZE, PAGE_READWRITE));
            }
            // we don't need to write-back cache here because the call to VMFreeAndRelease will write-back and discard
            VMFreeAndRelease (g_pprcNK, pKrnStk, cbSize);
        }
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"VMCreateStack returns %8.8lx\r\n", pUsrStk));
    return pUsrStk;
}

static DWORD _GetPFN (PPROCESS pprc, LPCVOID pAddr)
{
    DWORD    dwPFN   = INVALID_PHYSICAL_ADDRESS;
    DWORD    dwEntry = GetPDEntry (pprc->ppdir, pAddr);

    DEBUGCHK (!((DWORD) pAddr & VM_PAGE_OFST_MASK));

    if (IsSectionMapped (dwEntry)) {
        dwPFN = PFNfromSectionEntry (dwEntry) + SectionOfst (pAddr);
        
    } else {
        PPAGETABLE pptbl = GetPageTable (pprc->ppdir, VA2PDIDX (pAddr));
        if (pptbl) {
            dwEntry = pptbl->pte[VA2PT2ND(pAddr)];
            if (IsPageCommitted (dwEntry)) {
                dwPFN = PFNfromEntry (dwEntry);
            }
        }
    }
    return dwPFN;
}

//
// get physical page number of a virtual address of a process
//
DWORD GetPFNOfProcess (PPROCESS pprc, LPCVOID pAddr)
{
    if ((DWORD) pAddr >= VM_SHARED_HEAP_BASE) {
        pprc = g_pprcNK;
    } else if (!pprc) {
        pprc = pVMProc;
    }
    return _GetPFN (pprc, pAddr);
}



//
// get physical page number of a virtual address
//
DWORD GetPFN (LPCVOID pAddr)
{
    return _GetPFN (((DWORD) pAddr >= VM_SHARED_HEAP_BASE)? g_pprcNK : pVMProc, pAddr);
}


LPVOID GetKAddrOfProcess (PPROCESS pprc, LPCVOID pAddr)
{
    DWORD dwOfst = (DWORD)pAddr & VM_PAGE_OFST_MASK;
    DWORD dwPfn = GetPFNOfProcess (pprc, (LPVOID) ((DWORD) pAddr & -VM_PAGE_SIZE));

    if ((INVALID_PHYSICAL_ADDRESS != dwPfn) && (NULL != (pAddr = Pfn2Virt (dwPfn)))) {
        pAddr = (LPBYTE)pAddr + dwOfst;
    } else {
        pAddr = NULL;
    }

    return (LPVOID) pAddr;
}

LPVOID GetKAddr (LPCVOID pAddr)
{
    return GetKAddrOfProcess (NULL, pAddr);
}

//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
//
// Shared heap support (K-Mode R/W, U-Mode R/O), range (VM_SHARED_HEAP_BASE - VM_SHARED_HEAP_BOUND)
//
//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
LPVOID NKVirtualSharedAlloc (LPVOID lpvAddr, DWORD cbSize, DWORD fdwAction, PDWORD pdwTag)
{
    DWORD dwErr = 0;

    DEBUGMSG (ZONE_VIRTMEM, (L"NKVirtualShareAlloc: %8.8lx %8.8lx %8.8lx\r\n", lpvAddr, cbSize, fdwAction));
    
    if (!fdwAction || (~(MEM_COMMIT|MEM_RESERVE) & fdwAction)) {
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else if (!lpvAddr) {
        // NULL lpvAddr, always RESERVE
        fdwAction |= MEM_RESERVE;

    } else if (!IsInSharedHeap ((DWORD) lpvAddr) || (MEM_COMMIT != fdwAction)) {
        // committing, but address not in shared heap
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (!dwErr) {

        if (MEM_RESERVE & fdwAction) {
            DEBUGCHK (!lpvAddr);
            lpvAddr = VMReserve (g_pprcNK, cbSize, 0, VM_SHARED_HEAP_BASE);
        }

        if (!lpvAddr) {
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
            
        } else {
            if ((MEM_COMMIT & fdwAction)
                && !VMAlloc (g_pprcNK, lpvAddr, cbSize, MEM_COMMIT, PAGE_READWRITE)) {
                // failed committing memory
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            }

            if (MEM_RESERVE & fdwAction) {

                if (!dwErr
                    && (!VMAddAlloc (g_pprcNK, lpvAddr, cbSize, NULL, pdwTag))) {
                    dwErr = ERROR_NOT_ENOUGH_MEMORY;
                }
                
                if (dwErr) {
                    // failed committing memory, release VM
                    VERIFY (VMFreeAndRelease (g_pprcNK, lpvAddr, cbSize));
                }
            }
        }

    }

    if (dwErr) {
        NKSetLastError (dwErr);
    }

    DEBUGMSG (ZONE_VIRTMEM || dwErr, (L"NKVirtualShareAlloc returns: %8.8lx, GetLastError() = %8.8lx\r\n", dwErr? NULL : lpvAddr, dwErr));
    return dwErr? NULL : lpvAddr;
}


//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
//
// Memory-Mapped file support
//
//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------

typedef struct {
    DWORD   cDirtyPages;
    LPBYTE  pAddr;
    PFN_MapFlushRecordPage pfnRecordPage;
    FlushParams* pParams;
} VMMapCountStruct, *PVMMapCountStruct;


static BOOL MapCountDirtyPages (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVMMapCountStruct pvmcs = (PVMMapCountStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    BOOL  fRet    = TRUE;
    
    if (IsPageWritable (dwEntry)) {
        // pfnRecordPage may modify the page, so leave it R/W until after the call
        fRet = pvmcs->pfnRecordPage (pvmcs->pParams, pvmcs->pAddr, pvmcs->cDirtyPages);
        *pdwEntry = ReadOnlyEntry (dwEntry);
        pvmcs->cDirtyPages++;
    }
    
    // advance the address
    pvmcs->pAddr += VM_PAGE_SIZE;
    
    return fRet;
}


//
// VMMapClearDirtyPages - Call a mapfile callback function on every dirty page
//                        in the range, and mark them read-only before returning.
//                        Returns the number of pages that were cleared.
//
DWORD VMMapClearDirtyPages (PPROCESS pprc, LPBYTE pAddr, DWORD cbSize,
                            PFN_MapFlushRecordPage pfnRecordPage, FlushParams* pParams)
{
    VMMapCountStruct vmcs = { 0, pAddr, pfnRecordPage, pParams };
    DWORD cPages = PAGECOUNT (cbSize);
    
    DEBUGCHK (!((DWORD) pAddr & VM_PAGE_OFST_MASK));

    if (LockVM (pprc)) {
        
        if (VMIsPagesLocked (pprc, (DWORD) pAddr, cPages)) {
            DEBUGMSG (ZONE_MAPFILE, (L"VMMapClearDirtyPages: Cannot clear Addr=%8.8lx, %u pages because the pages are locked\r\n", pAddr, cPages));
            KSetLastError (pCurThread, ERROR_SHARING_VIOLATION);
            vmcs.cDirtyPages = (DWORD) -1;

        } else {
            Enumerate2ndPT (pprc->ppdir, (DWORD) pAddr, cPages, FALSE,
                            MapCountDirtyPages, &vmcs);
    
            // flush TLB if there is any dirty page
            if (vmcs.cDirtyPages) {
                InvalidatePages (pprc, (DWORD) pAddr, cPages);
            }
        }

        UnlockVM (pprc);
    }
    
    return vmcs.cDirtyPages;
}


// Enumeration state for VMMapMove
typedef struct {
    PFSMAP  pfsmap;
    LPBYTE  pAddr;
    ULARGE_INTEGER liFileOffset;
} VMMapMoveStruct, *PVMMapMoveStruct;

static BOOL EnumMapMovePage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVMMapMoveStruct pvmcs = (PVMMapMoveStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    
    // Use count=1 for the page because this procedure is only used for
    // non-pageable mappings, which need to hold all the pages until the
    // mapfile is destroyed, rather than releasing them when the last view
    // is destroyed.
    BOOL  fRet = IsPageCommitted (dwEntry) && MapVMAddPage (pvmcs->pfsmap, pvmcs->liFileOffset, pvmcs->pAddr, 1);
    
    DEBUGCHK (IsPageCommitted (dwEntry));
    
    if (fRet) {
        // we successfully moved the page to the mapfile tree, change the entry to reserved
        *pdwEntry = MakeReservedEntry (VM_PAGER_NONE);
        // advance the address
        pvmcs->pAddr += VM_PAGE_SIZE;
        pvmcs->liFileOffset.QuadPart += VM_PAGE_SIZE;
    }
    
    return fRet;
}


//
// VMMapMove: move page mapping from process VM to mapfile page tree.
//
BOOL VMMapMove (PFSMAP pfsmapDest, PPROCESS pprcSrc, LPVOID pAddrPrc, DWORD cbSize)
{
    VMMapMoveStruct vmcs;
    DWORD cPages = PAGECOUNT (cbSize);
    BOOL  fRet = FALSE;

    DEBUGCHK (!((DWORD)pAddrPrc & VM_PAGE_OFST_MASK));
    DEBUGCHK (pprcSrc == g_pprcNK);
    DEBUGCHK (IsKModeAddr ((DWORD) pAddrPrc));
    
    DEBUGMSG (ZONE_MAPFILE, (L"VMMapMove: Copy from %8.8lx of pid = %8.8lx, size %8.8lx to pfsmap = %8.8lx\r\n",
                pAddrPrc, pprcSrc->dwId, cbSize, pfsmapDest));
                
    // Cannot acquire the mapfile VM CS after acquiring the process VM CS.
    // Since EnumMapMovePage calls back into the mapfile VM code, the caller of
    // VMMapMove must already have acquired the mapfile VM CS.
    DEBUGCHK (OwnCS (&pfsmapDest->pgtree.csVM));

    vmcs.pfsmap = pfsmapDest;
    vmcs.pAddr = pAddrPrc;
    vmcs.liFileOffset.QuadPart = 0;  // Copying entire file, start at beginning

#ifdef ARM
    if (IsVirtualTaggedCache ()) {
        NKCacheRangeFlush (pAddrPrc, cbSize, CACHE_SYNC_WRITEBACK);
    }
#endif

    if (LockVM (pprcSrc)) {
        fRet = Enumerate2ndPT (pprcSrc->ppdir, (DWORD) pAddrPrc, cPages, FALSE,
                               EnumMapMovePage, &vmcs);
        UnlockVM (pprcSrc);

        DEBUGCHK (!fRet || (vmcs.liFileOffset.QuadPart == cbSize));
    }

    return fRet;
}


//
// VMMapMove: move a paging page from process VM to mapfile page tree.
//
BOOL VMMapMovePage (
    PFSMAP pfsmap,
    ULARGE_INTEGER liFileOffset,
    LPVOID pPage,
    WORD   InitialCount     // Initial use-count for the page, 0 if pageable or 1 if non-pageable
    )
{
    BOOL fRet;
    
    DEBUGCHK (IsInKVM ((DWORD) pPage));
#ifdef ARM
    if (IsVirtualTaggedCache ()) {
        NKCacheRangeFlush (pPage, VM_PAGE_SIZE, CACHE_SYNC_WRITEBACK);
    }
#endif

    fRet = MapVMAddPage (pfsmap, liFileOffset, pPage, InitialCount);
    if (fRet) {
        VMRemoveKernelPageMapping (pPage);
    }
    return fRet;
}

// Enumeration state for VMMapDecommit
typedef struct {
    PFSMAP   pfsmap;
    PPROCESS pprc;
    LPBYTE   pAddr;
    ULARGE_INTEGER liFileOffset;
    BOOL     fFreeAll;
    BOOL     fHasLockedPages;
    BOOL     fCommittedPagesRemaining;
} VMMapDecommitStruct, *PVMMapDecommitStruct;

static BOOL MapDecommitPage (LPDWORD pdwEntry, LPVOID pEnumData)
{
    PVMMapDecommitStruct pvmds = (PVMMapDecommitStruct) pEnumData;
    DWORD dwEntry = *pdwEntry;
    BOOL  fRet = TRUE;
    
    if (IsPageCommitted (dwEntry)) {
        if (pvmds->fFreeAll || !IsPageWritable (dwEntry)) {
            // Don't decommit if it's locked
            if (pvmds->fHasLockedPages
                && VMIsPagesLocked (pvmds->pprc, (DWORD) pvmds->pAddr, 1)) {
                DEBUGMSG (ZONE_MAPFILE, (L"VMMapClearDirtyPages: Skipping decommit on locked page at Addr=%8.8lx\r\n", pvmds->pAddr));
                pvmds->fCommittedPagesRemaining = TRUE;
            } else {
                // DecommitPages removes the page from the view.  If this is the last
                // view it was paged into, MapVMFreePage removes the page from the page
                // tree and gives it back to the page pool.
                if (!DecommitPages (pdwEntry, (LPVOID) VM_PAGER_MAPPER)
                    || !MapVMFreePage (pvmds->pfsmap, pvmds->liFileOffset)) {
                    // Failed
                    DEBUGCHK (0);
                    pvmds->fCommittedPagesRemaining = TRUE;
                    fRet = FALSE;
                }
            }
        } else {
            // Writable page is still committed
            pvmds->fCommittedPagesRemaining = TRUE;
        }
    }
    
    // advance the address
    pvmds->pAddr += VM_PAGE_SIZE;
    pvmds->liFileOffset.QuadPart += VM_PAGE_SIZE;
    
    return fRet;
}


//
// VMMapDecommit: Decommit view memory from process VM.  Will discard only
// read-only pages, or all, depending on fFreeAll.  Maintains mapfile page tree
// page refcounts.  Returns TRUE if all pages have been decommitted and FALSE
// if there are committed pages remaining.  (Note, however, that unless care is
// taken to prevent new commits after the decommit, the commit status could be
// invalidated immediately.)
//
BOOL
VMMapDecommit (
    PFSMAP   pfsmap,
    const ULARGE_INTEGER* pliFileOffset,
    PPROCESS pprc,
    LPVOID   pAddrPrc,
    DWORD    cbSize,
    BOOL     fFreeAll       // TRUE: discard all, FALSE: discard only R/O pages
    )
{
    VMMapDecommitStruct vmds;
    DWORD cPages = PAGECOUNT (cbSize);
    BOOL  fRet = FALSE;

    DEBUGCHK (!((DWORD)pAddrPrc & VM_PAGE_OFST_MASK));
    
    DEBUGMSG (ZONE_MAPFILE, (L"VMMapDecommit: Decommit from %8.8lx of pid = %8.8lx, size %8.8lx in pfsmap = %8.8lx\r\n",
                             pAddrPrc, pprc->dwId, cbSize, pfsmap));
                
    // Cannot acquire the mapfile VM CS after acquiring the process VM CS.
    // Since MapDecommitPage calls back into the mapfile VM code, the caller of
    // VMMapDecommit must already have acquired the mapfile VM CS.
    DEBUGCHK (OwnCS (&pfsmap->pgtree.csVM));

    vmds.pfsmap = pfsmap;
    vmds.pprc = pprc;
    vmds.pAddr = pAddrPrc;
    vmds.liFileOffset.QuadPart = pliFileOffset->QuadPart;
    vmds.fFreeAll = fFreeAll;
    vmds.fCommittedPagesRemaining = FALSE;

    if (LockVM (pprc)) {
        // write-back all dirty pages
        NKCacheRangeFlush (NULL, 0, CACHE_SYNC_WRITEBACK);

        // If any pages are locked, we leave those but decommit the rest.
        // fHasLockedPages skips walking the lock list on each page if none are locked.
        vmds.fHasLockedPages = VMIsPagesLocked (pprc, (DWORD) pAddrPrc, cPages);

        fRet = Enumerate2ndPT (pprc->ppdir, (DWORD) pAddrPrc, cPages, FALSE,
                               MapDecommitPage, &vmds);
        InvalidatePages (pprc, (DWORD) pAddrPrc, cPages);
        UnlockVM (pprc);

        DEBUGCHK (!fRet || (vmds.pAddr == (LPBYTE) PAGEALIGN_UP ((DWORD) pAddrPrc + cbSize)));

        if (vmds.fCommittedPagesRemaining) {
            // There are still locked pages or dirty pages remaining
            fRet = FALSE;
            DEBUGCHK (!fFreeAll);
        }
    }

    return fRet;
}


//
//
// VMMapTryExisting: used for paging, change VM attributes if page already committed
//
// NOTE: caller must have verified that the addresses are valid, and writeable if fWrite is TRUE
//
VMMTEResult VMMapTryExisting (PPROCESS pprc, LPVOID pAddrPrc, DWORD fProtect)
{
    PPAGEDIRECTORY ppdir = LockVM (pprc);
    VMMTEResult result = VMMTE_FAIL;

    DEBUGMSG (ZONE_MAPFILE && ZONE_PAGING,
              (L"VMMapTryExisting: %8.8lx %8.8lx (%8.8lx)\r\n",
               pprc, pAddrPrc, fProtect));

    if (ppdir) {
        PPAGETABLE pptbl = GetPageTable (ppdir, VA2PDIDX(pAddrPrc));
        DWORD idx2nd = VA2PT2ND (pAddrPrc);
        if (pptbl) {
            DWORD dwEntry = pptbl->pte[idx2nd];

            if (IsPageCommitted (dwEntry)) {
                DWORD dwPgProt;
                BOOL  fPageIsCurrentlyWritable = IsPageWritable (dwEntry);
                
                // If the page is already committed writable, change the requested protection to writable too,
                // such that we don't change the protection of the page uncessarily.
                if (fPageIsCurrentlyWritable) {
                    switch (fProtect) {
                    case PAGE_READONLY:
                        fProtect = PAGE_READWRITE;
                        break;
                    case PAGE_EXECUTE_READ:
                        fProtect = PAGE_EXECUTE_READWRITE;
                        break;
                    }
                }
                dwPgProt = PageParamFormProtect (pprc, fProtect, (DWORD) pAddrPrc);
                result   = VMMTE_ALREADY_EXISTS;
                
                // page already committed. Change protection if needed
                if (!IsSameEntryType (dwEntry, dwPgProt)) {
                    // permission changed.
                    if (IsPageWritable (dwPgProt) && !fPageIsCurrentlyWritable) {
                        // changing a r/o page to r/w, return success
                        result = VMMTE_SUCCESS;
                    }
                    pptbl->pte[idx2nd] = MakeCommittedEntry (PFNfromEntry (dwEntry), dwPgProt);
#ifdef ARM
                    ARMPteUpdateBarrier (&pptbl->pte[idx2nd], 1);
#endif
                    InvalidatePages (pprc, (DWORD) pAddrPrc, 1);
                }

            }
        }
        UnlockVM (pprc);
    }

    DEBUGMSG ((VMMTE_FAIL == result) && ZONE_MAPFILE && ZONE_PAGING,
              (L"VMMapTryExisting - no existing page at %8.8lx\r\n", pAddrPrc));

    return result;

}


//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
//
// static mapping support
//
//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------

//
// Find out if given physical base and size match with any existing static mapping
//
LPVOID FindStaticMapping (DWORD dwPhysBase, DWORD dwSize)
{
    LPVOID MappedVirtual = NULL;
    PSTATICMAPPINGENTRY pItem = g_StaticMappingList;
    DWORD dwPhysEnd = dwPhysBase + (dwSize >> 8);

    // check for overflow
    if (dwPhysEnd < dwPhysBase
        || (dwPhysEnd < (dwSize >> 8))) {
        return NULL;
    }

    // scan the list
    while (pItem) {

        // check for range match
        if ((dwPhysBase >= pItem->dwPhysStart)
            && (dwPhysEnd <= pItem->dwPhysEnd)) {
            MappedVirtual = (LPVOID) ((DWORD)pItem->lpvMappedVirtual + ((dwPhysBase - pItem->dwPhysStart) << 8));
            break;
        }

        pItem = pItem->pNext;
    }

    return MappedVirtual;
}

//
// Add a entry to the head of the static mapping list
//
void AddStaticMapping(DWORD dwPhysBase, DWORD dwSize, LPVOID MappedVirtual)
{
    PSTATICMAPPINGENTRY pvItem = (PSTATICMAPPINGENTRY) ((g_pprcNK->csVM.hCrit) ? AllocMem (HEAP_STATICMAPPING_ENTRY): GrabOnePage (PM_PT_ZEROED));
    DWORD dwPhysEnd = dwPhysBase + (dwSize >> 8);

    // check for overflow
    if (dwPhysEnd < dwPhysBase
        || (dwPhysEnd < (dwSize >> 8))) {
        return;
    }

    if (pvItem) {
        pvItem->lpvMappedVirtual = MappedVirtual;
        pvItem->dwPhysStart = dwPhysBase;
        pvItem->dwPhysEnd = dwPhysEnd;
        if (g_pprcNK->csVM.hCrit)
        {
            InterlockedPushList (&g_StaticMappingList, pvItem);
        }
        else
        {
            pvItem->pNext = g_StaticMappingList;
            g_StaticMappingList = pvItem;
        }
    }
    
    return;
}

#if defined (ARM) || defined (x86)

PCDeviceTableEntry g_pOEMDeviceTable;
PADDRMAP g_pOEMAddressTable;

static DeviceTableEntry dummyTable;

// find static mapped device address
LPVOID FindDeviceMapping (DWORD pa256, DWORD cbSize)
{
    LPVOID pRet = NULL;
    PCDeviceTableEntry pdevtbl = g_pOEMDeviceTable;

    DEBUGCHK (cbSize && IsPageAligned (cbSize));
    if (pdevtbl) {
        cbSize >>= 8;   // shift size by 8 to simplify calcuation

        // iterate through the table
        for ( ; pdevtbl->VirtualAddress; pdevtbl ++) {
            if (   (pa256 >= pdevtbl->ShiftedPhysicalAddress)
                && ((pa256 + cbSize) <= (pdevtbl->ShiftedPhysicalAddress + (pdevtbl->Size >> 8)))) {
                // found
                pRet = (LPVOID) (pdevtbl->VirtualAddress
                                + ((pa256 - pdevtbl->ShiftedPhysicalAddress) << 8));
                break;
            }
        }
    }

    return pRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PVOID 
Pfn2Virt(
    DWORD pfn
    ) 
{
    LPVOID   pRet = NULL;
    PADDRMAP pAddrMap;
    DWORD    dwSize;

    for (pAddrMap = g_pOEMAddressTable; pAddrMap->dwSize; pAddrMap ++) {
#ifdef ARM
        dwSize = pAddrMap->dwSize << VM_SECTION_SHIFT;    // ARM - size in MB
#else
        dwSize = pAddrMap->dwSize;          // x86 - size in bytes
#endif
        if ((DWORD) (pfn - pAddrMap->dwPA) < dwSize) {
            pRet = (LPVOID) (pfn - pAddrMap->dwPA + pAddrMap->dwVA);
            break;
        }
    }

    DEBUGMSG(ZONE_PHYSMEM && !pRet, (TEXT("Phys2Virt() : PFN (0x%08X) not found!\r\n"), pfn));
    return pRet;
}

DWORD CalcMaxDeviceMapping (void)
{
    DWORD dwMaxMapping;

    if (CE_NEW_MAPPING_TABLE == g_pOEMAddressTable->dwVA) {
        // new device table mapping
        PCDeviceTableEntry  pDevTblEntry;
        PCADDRMAP           pAddrTblEntry;
        DWORD               dwSize;

        dwMaxMapping = VM_KMODE_BASE;
        g_pOEMDeviceTable = (PCDeviceTableEntry) g_pOEMAddressTable->dwPA;
        if (!g_pOEMDeviceTable) {
            // this is the case where we want to use the new style mapping (no mapping for uncached area), but
            // there is no statically mapped device addresses (e.g. CEPC)
            g_pOEMDeviceTable = &dummyTable;
        }
        
        g_pOEMAddressTable ++;                      // skip the 1st entry

        // search OEMAddressTable 1st
        for (pAddrTblEntry = g_pOEMAddressTable; pAddrTblEntry->dwVA; pAddrTblEntry ++) {
#ifdef ARM
            dwSize = pAddrTblEntry->dwSize << 20;   // ARM - size in MB
#else
            dwSize = pAddrTblEntry->dwSize;         // x86 - size in bytes
#endif
            if (dwMaxMapping < pAddrTblEntry->dwVA + dwSize) {
                dwMaxMapping = pAddrTblEntry->dwVA + dwSize;
            }
        }

        // now search OEMDeviceTable
        for (pDevTblEntry = g_pOEMDeviceTable; pDevTblEntry->VirtualAddress; pDevTblEntry ++) {
            if (dwMaxMapping < pDevTblEntry->VirtualAddress + pDevTblEntry->Size) {
                dwMaxMapping = pDevTblEntry->VirtualAddress + pDevTblEntry->Size;
            }
        }

        // 4M align the address
        dwMaxMapping = ALIGNUP_4M (dwMaxMapping);

    } else {
        DWORD idxCached   = 0x80000000 >> VM_SECTION_SHIFT;
        DWORD idxUncached = 0xA0000000 >> VM_SECTION_SHIFT;
        DWORD idxEnd      = 0xC0000000 >> VM_SECTION_SHIFT;
        // BC mode, create mapping for 0xa0000000-0xc0000000
        dwMaxMapping = VM_NKVM_BASE;
        
        for ( ; idxUncached < idxEnd; idxUncached ++, idxCached ++) {
            if (g_ppdirNK->pte[idxCached]) {
#ifdef ARM
                // ARM - the "cached mapping" is actually uncached at this point. 
                //       just do a simple copy
                g_ppdirNK->pte[idxUncached] = g_ppdirNK->pte[idxCached];
                if (IsV6OrLater ()) {
                    g_ppdirNK->pte[idxUncached] |= PG_V6_L1_NO_EXECUTE;
                }
#else
                g_ppdirNK->pte[idxUncached] = g_ppdirNK->pte[idxCached] | PG_NOCACHE;
#endif
            }
        }
    }

    return dwMaxMapping;          // 4M alignment
}

void MapDeviceTable (void)
{
    if (g_pOEMDeviceTable) {
        PCDeviceTableEntry pDevTblEntry;
        PPAGETABLE         pptbl;
        DWORD              dwVa, dwPfn, dwSize, dwPgProt, idx, pfnIncr;
        DWORD              pgGlobal = IsGlobalPageSupported()? PG_GLOBAL_MASK : 0;

        for (pDevTblEntry = g_pOEMDeviceTable; pDevTblEntry->Size; pDevTblEntry ++) {
            dwSize   = pDevTblEntry->Size;
            dwPfn    = PFNfrom256 (pDevTblEntry->ShiftedPhysicalAddress);
            dwVa     = pDevTblEntry->VirtualAddress;
            // all entries must be page aligned
            DEBUGCHK (IsPageAligned (PFN2PA (dwPfn)));
            DEBUGCHK (IsPageAligned (dwVa));
            DEBUGCHK (IsPageAligned (dwSize));
            if (   IsSectionAligned (PFN2PA (dwPfn))
                && IsSectionAligned (dwVa)
                && IsSectionAligned (dwSize)) {
                dwPgProt = pDevTblEntry->Attributes | PG_SECTION_PROTECTION | pgGlobal;
#ifdef ARM
                if (IsV6OrLater ()) {
                    dwPgProt |= PG_V6_L1_NO_EXECUTE;
                }
#endif
                pfnIncr  = PA2PFN (VM_SECTION_SIZE);
                // all section aligned, section map the entry
                for (idx = dwVa >> VM_SECTION_SHIFT; dwSize; idx ++, dwPfn += pfnIncr, dwSize -= VM_SECTION_SIZE)  {
                    g_ppdirNK->pte[idx] = dwPfn | dwPgProt;
                }
                
            } else {

                dwPgProt = pDevTblEntry->Attributes | PG_PROT_UNO_KRW | PG_VALID_MASK | PG_DIRTY_MASK | pgGlobal;
                pfnIncr  = PA2PFN (VM_PAGE_SIZE);

#ifdef ARM
                if (IsV6OrLater ()) {
                    dwPgProt |= PG_V6_L2_NO_EXECUTE;
                }
#endif
                do {
                    pptbl = GetPageTable (g_ppdirNK, VA2PDIDX (dwVa));
                    if (!pptbl) {
                        pptbl = AllocatePTBL (g_ppdirNK, VA2PDIDX (dwVa));
                        // if the following debugchk failed, it means either
                        // 1) we're out of memory on system startup, or
                        // 2) the virtual address is overlapped with another section mapped address (most likely in OEMAddressTable)
                        // the system won't boot in either case, no point checking pptbl.
                        PREFAST_DEBUGCHK (pptbl);
                    }
                    // overlapping Virtual Address found if the following debugchk is hit
                    DEBUGCHK (!pptbl->pte[VA2PT2ND (dwVa)]);
                    
                    pptbl->pte[VA2PT2ND (dwVa)] = dwPfn | dwPgProt;
                    dwSize -= VM_PAGE_SIZE;
                    dwVa   += VM_PAGE_SIZE;
                    dwPfn  += pfnIncr;
                } while (dwSize);

            }
        }
    }
}




#else
#define MAX_NON_TLB_PA256       (0x20000000 >> 8)

// find static mapped device address
LPVOID FindDeviceMapping (DWORD pa256, DWORD cbSize)
{
    LPVOID pRet = NULL;
    DEBUGCHK (cbSize && IsPageAligned (cbSize));
    if (   (pa256 < MAX_NON_TLB_PA256)
        && (pa256 + (cbSize >> 8) <= MAX_NON_TLB_PA256)) {
        pRet = (LPVOID) ((pa256 << 8) | KSEG1_BASE);
    }
    return pRet;
}

//------------------------------------------------------------------------------
// convert page table entry to the corresponding non-tlb address
//------------------------------------------------------------------------------
PVOID Pfn2Virt (DWORD dwPfn)
{
    return (LPVOID) ((dwPfn << PFN_SHIFT) | KSEG0_BASE);
}


#endif


//-----------------------------------------------------------------------------------------------------------------
//
// NKCreateStaticMapping: Create a static mapping
//
LPVOID
NKCreateStaticMapping(
    DWORD dwPhysBase,
    DWORD cbSize
    ) 
{
    LPVOID pRet = NULL;
    DEBUGMSG (ZONE_VIRTMEM, (L"CreateStaticMapping: 0x%8.8lx 0x%8.8lx\r\n", dwPhysBase, cbSize));

    cbSize = PAGEALIGN_UP(cbSize);
    DEBUGCHK(cbSize);
    
    if (!cbSize || !IsPageAligned ((dwPhysBase<<8))) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: CreateStaticMapping -Not valid Parameters 0x%8.8lx 0x%8.8lx\r\n", dwPhysBase, cbSize));
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    } else {
        // search statically mapp device addresses 1st
        pRet = FindDeviceMapping (dwPhysBase, cbSize);

        if (!pRet) {
            // try to find existing mappings
            pRet = FindStaticMapping (dwPhysBase, cbSize);

            if (!pRet) {
                // not in the cached static mapping list
                pRet = VMReserve (g_pprcNK, cbSize, 0, 0);

                if (pRet) {
                    if(!VMCopy (g_pprcNK, (DWORD) pRet, NULL, dwPhysBase, cbSize, PAGE_PHYSICAL|PAGE_NOCACHE|PAGE_READWRITE)) {
                        VMFreeAndRelease (g_pprcNK, pRet, cbSize);
                        pRet = NULL;

                    } else {
                        // add to the static mapping list.
                        AddStaticMapping(dwPhysBase, cbSize, pRet);
                    }
                }
            }
        }
        
    }
    DEBUGMSG (ZONE_VIRTMEM, (L"CreateStaticMapping: returns %8.8lx\r\n", pRet));

    return pRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// NKDeleteStaticMapping: Delete a static mapping
//
BOOL
NKDeleteStaticMapping (
    LPVOID pVirtAddr,
    DWORD dwSize
    )
{
    BOOL fRet = IsKernelVa (pVirtAddr)
            || (   IsInKVM ((DWORD) pVirtAddr)
                && IsInKVM ((DWORD) pVirtAddr + dwSize));
    /*
                // 
                // For b/c reasons, not releasing any static mapping allocation 
                // since most existing drivers do not call DeleteStaticMapping
                // and also static mapping allocations are not refcounted
                // currently.
                //
                && VMFreeAndRelease (g_pprcNK, pVirtAddr, dwSize));
    */

    DEBUGMSG (!fRet, (L"!ERROR: DeleteStaticMapping (%8.8lx %8.8lx) Failed\r\n", pVirtAddr, dwSize));
    return fRet;
}

//-----------------------------------------------------------------------------------------------------------------
//
// ASLRInit - initialize ASLR settings
//
ULONGLONG GetTimeInTicks (DWORD dwType);

void ASLRInit (BOOL fAslrEnabled, DWORD dwASLRSeed)
{
    LARGE_INTEGER liTick;
    DWORD i;
    liTick.QuadPart = GetTimeInTicks (TM_LOCALTIME);

    g_fAslrEnabled = fAslrEnabled;
    randdw1 ^= (liTick.LowPart ^  liTick.HighPart);
    
    if (fAslrEnabled) {
        g_aslrSeed          = dwASLRSeed;
        g_vaDllFree        += NextAslrBase ();
        g_vaRAMMapFree     += NextAslrBase ();
        g_vaSharedHeapFree += NextAslrBase ();

        NextAslrBase ();
        g_pKData->dwPslTrapSeed = g_aslrSeed & 0x000FFFFC;
    }
    g_pKData->dwSyscallReturnTrap = SYSCALL_RETURN_RAW ^ g_pKData->dwPslTrapSeed;

    for (i = 1; i < g_pKData->nCpus; i ++) {
        g_ppcbs[i]->dwPslTrapSeed = g_pKData->dwPslTrapSeed;
        g_ppcbs[i]->dwSyscallReturnTrap = g_pKData->dwSyscallReturnTrap;
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// InitVoidPages - intiailze support VMMapToVoid
//
void InitVoidPages (void)
{
    int i;
    for (i = 0; i < VM_PAGES_PER_BLOCK; i ++) {
        g_pfnVoidPages[i] = GetPFN ((LPVOID) (pTOC->ulRAMFree+MemForPT));
        MemForPT += VM_PAGE_SIZE;
    }
}

