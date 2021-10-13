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
//    vmarm.c - ARM specific VM implementations
//
#include <windows.h>
#include <vm.h>
#include <kernel.h>


ERRFALSE (!(sizeof (PAGEDIRECTORY) & VM_PAGE_OFST_MASK));

//
// V6 or later has address above 0x80000000 reference to kernel page directory automatically.
// Therefore, only 2 pages, aligned at 8K, are needed for user processes page directory
//
#define PDNumPages                  (2)
#define PDAlignment                 (0x1fff)

// need an extra page for page tables
#define PDReserveSize               (sizeof (PAGEDIRECTORY) + VM_PAGE_SIZE)

// page directory/table protection
static DWORD PTProtections;

#define IsValidPTBLEntry(entry)     IsKModeAddr ((DWORD) (entry))
#define MakeValidPTBLEntry(entry)   ((DWORD) (entry) | 0x80000000)
#define MakeReservedPTBLEntry(va)   ((DWORD) (va)    & ~0x80000000)

#define PD2PTIDX(idxPD)             (idxPD >> 2)


static DWORD GetWriteProtectBits (DWORD dwAddr)
{
    return IsInSharedHeap (dwAddr)
                            ? PG_PROT_URO_KRW               // shared heap - krw, uro
                            : (IsKModeAddr (dwAddr)
                                ? PG_PROT_UNO_KRW           // kernel address - krw, uno
                                : PG_PROT_WRITE);           // user address krw, urw
}

static DWORD GetV6WriteProtectBits (DWORD dwAddr)
{
    return IsInSharedHeap (dwAddr)
                            ? PG_V6_PROT_URO_KRW            // shared heap - krw, uro
                            : (IsKModeAddr (dwAddr)
                                ? PG_V6_PROT_UNO_KRW        // kernel address - krw, uno
                                : PG_V6_PROT_WRITE);        // user address krw, urw
}

static DWORD GetNXBit (PPROCESS pprc, DWORD dwAddr)
{
    DWORD dwPgProt = 0;

    if (NX_SUPPORT_NONE != g_dwNXFlags) {
        // set NX bit if
        // 1) kernel address, or
        // 2) flag is NX_SUPPORT_STRICT, or
        // 3) flag is NX_SUPPORT_BY_APP, and current process is NX Compatible
        if (   (VM_SHARED_HEAP_BASE <= dwAddr)
            || (NX_SUPPORT_STRICT == g_dwNXFlags)
            || ((NX_SUPPORT_BY_APP == g_dwNXFlags) && (pprc->fFlags & MF_DEP_COMPACT))) {
            dwPgProt = PG_V6_L2_NO_EXECUTE;
        }
    }
    return dwPgProt;
}

//-----------------------------------------------------------------------------------------------------------------
//
//  PageParamFormProtect: return page protection from the PAGE_xxx flags
//
DWORD PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr)
{
    DWORD dwPgProt = 0;

    DEBUGCHK (!(fProtect & ~VM_VALID_PROT));

    if (PAGE_NOACCESS == fProtect)
        dwPgProt = PG_NOACCESS;
    
    else {
        if (PAGE_GUARD & fProtect) {

            // guard page, use V6 protection bits
            // we'll adjust the protection when committing
            // guard pages
            dwPgProt = PG_GUARD |
                       ((VM_READWRITE_PROT & fProtect) 
                        ? GetV6WriteProtectBits (dwAddr)
                        : PG_V6_PROT_READ);
            
        } else {
            dwPgProt = PG_VALID_MASK | 
                       ((VM_READWRITE_PROT & fProtect) 
                        ? GetWriteProtectBits (dwAddr)
                        : PG_PROT_READ);
        }

        if (!(PAGE_NOCACHE & fProtect)) {
            if (fProtect & PAGE_WRITE_THRU) {
                dwPgProt |= PG_WRITE_THRU;
            }
            else {
                dwPgProt |= PG_CACHE;
            }
        } else if (!(PAGE_GUARD & fProtect)) {
            dwPgProt |= PG_V6_L2_NO_EXECUTE;
        }

        // special handling for V6 or later
        if (VM_SHARED_HEAP_BASE > dwAddr) {
            // user address, not global
            dwPgProt |= PG_V6_NOT_GLOBAL;
        }
        if (!((VM_EXECUTABLE_PROT|PAGE_GUARD) & fProtect)) {
            dwPgProt |= GetNXBit (pprc, dwAddr);
        }
    }

    return dwPgProt;
}


//-----------------------------------------------------------------------------------------------------------------
//
// ProtectFromEntry - get page protection (PAGE_XXX) from an entry 
//
DWORD ProtectFromEntry (DWORD dwEntry)
{
    DWORD dwProtect;
    DWORD dwReadProt;
    DWORD dwWriteProt;

    if (!(dwEntry & PG_V6_L2_NO_EXECUTE)) {
        dwReadProt  = PAGE_EXECUTE_READ;
        dwWriteProt = PAGE_EXECUTE_READWRITE;
    } else {
        dwReadProt  = PAGE_READONLY;
        dwWriteProt = PAGE_READWRITE;
    }
    
    if (PG_VALID_MASK & dwEntry) {
        // committed entry
        dwProtect = (PG_PROT_READ == (dwEntry & PG_PROTECTION))
                                ? dwReadProt
                                : dwWriteProt;
    } else if (PG_GUARD & dwEntry) {
        // guard page
        dwProtect = (PG_V6_PROT_READ == (dwEntry & PG_V6_PROTECTION))
                                ? dwReadProt
                                : dwWriteProt;
        dwProtect |= PAGE_GUARD;
    } else {
        dwProtect = PAGE_NOACCESS;
    }

    if ((PG_NOACCESS != dwProtect) && !(PG_CACHE & dwEntry)) {
        dwProtect |= PAGE_NOCACHE;
    }


    return dwProtect;
}

// commit guard page (uncached guard pages not supported)
DWORD CommitGuardPage (DWORD dwEntry)
{
    DEBUGCHK (IsGuardPage (dwEntry));

    // v6 or later, just take aways the guard bit and make page valid
    dwEntry = (dwEntry & ~PG_GUARD) | PG_VALID_MASK;

    return dwEntry;
}

//
// MakeWritableEntry: create a page table entry for writable page
//
DWORD MakeWritableEntry (PPROCESS pprc, DWORD dwPFN, DWORD dwAddr)
{
    DWORD dwEntry = dwPFN | GetWriteProtectBits (dwAddr) | PG_VALID_MASK | PG_CACHE;

    if (VM_SHARED_HEAP_BASE > dwAddr) {
        // user address, not global
        dwEntry |= PG_V6_NOT_GLOBAL;
    }
    dwEntry |= GetNXBit (pprc, dwAddr);

    return dwEntry;
}

//------------------------------------------------------------------------------
// LoadPageTable - load entries into page tables from kernel structures
//
//  LoadPageTable is called for prefetch and data aborts to copy page table
// entries from the kernel virtual memory tables to the ARM h/w page tables.
//------------------------------------------------------------------------------
BOOL LoadPageTable (ulong addr)
{
    // DO NOT USE VA2PDIDX, for it'll round to 4M boundary
    DWORD idxPD      = (addr >> 20);        // VA2PDIDX (addr);
    DWORD dwEntry    = g_ppdirNK->pte[idxPD];
    BOOL  fRet       = (addr >= VM_SHARED_HEAP_BASE)
                    && !IsKModeAddr (addr)
                    && dwEntry;

    if (fRet) {

        PPROCESS       pprc  = pVMProc;
        PPAGEDIRECTORY ppdir = pprc->ppdir;
        DWORD          dwPfn = GetPFN (ppdir);
        
        if (PFNfromEntry (GetTranslationBase ()) != dwPfn) {
            // preempted between updating pVMProc and ASID/TTBR0
            UpdateAsidAndTranslationBase (pprc->bASID, dwPfn | g_pKData->dwTTBRCtrlBits);
            ppdir->pte[idxPD] = dwEntry;
        } else if (ppdir->pte[idxPD] != dwEntry) {
            ppdir->pte[idxPD] = dwEntry;
        } else {
            fRet = FALSE;
        }

        if (fRet) {
            ARMPteUpdateBarrier (&ppdir->pte[idxPD], 1);
        }
    }
    DEBUGMSG (!fRet && ZONE_VIRTMEM, (L"LoadPageTable failed, addr = %8.8lx\r\n", addr));
    return fRet;
}

void ARMPteUpdateBarrier (LPDWORD pdwEntry, DWORD cEntries)
{
    if (g_pOemGlobal->pfnPTEUpdateBarrier) {
        g_pOemGlobal->pfnPTEUpdateBarrier (pdwEntry, cEntries * sizeof (DWORD));
    } else {
        V6_WriteBarrier (); // DSB
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// InvalidatePages - invalidate a range of Virtual Addresses
//
void InvalidatePages (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    DWORD dwReason = dwAddr? CSF_VM_CHANGE : CSF_PROCESS_EXIT;
    // for v6 or later, only flush TLB

    if ((pprc != pVMProc) && (VM_SHARED_HEAP_BASE > dwAddr)) {
        // if non-global address, and not current VM, flush all TLB
        dwAddr = cPages = 0;
    }
    NKCacheRangeFlush ((LPVOID) dwAddr, cPages << VM_PAGE_SHIFT, CACHE_SYNC_FLUSH_TLB|dwReason);
} 

static void MapPageTable (PPAGETABLE pPTS, DWORD idxPTBL, DWORD dwPfn)
{
    DWORD dwPtblAddr;
    BOOL fNeedFlushCached = !IsFlushUncachedVASupported();
    // Make the entry valid and Virtual copy the physical page to map 2nd-level page table
    DEBUGCHK (!(VM_PAGE_OFST_MASK & pPTS->pte[idxPTBL]));
    DEBUGCHK (pPTS->pte[idxPTBL]);
    dwPtblAddr = MakeValidPTBLEntry (pPTS->pte[idxPTBL]);
    // always map as cacheable first to flush cache if Flush Uncached VA is not supported.
    VERIFY (VMCopyPhysical (g_pprcNK, dwPtblAddr, dwPfn, 1, fNeedFlushCached? PG_KRN_READ_WRITE : PTProtections, FALSE));
    NKCacheRangeFlush ((LPVOID)dwPtblAddr, VM_PAGE_SIZE, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);

    if (fNeedFlushCached) {
        PTENTRY *ppte = &(GetPageTable (g_ppdirNK, VA2PDIDX (dwPtblAddr))->pte[VA2PT2ND (dwPtblAddr)]);
        // change the cache attributes to specific setting (PTProtections)
        *ppte = MakeCommittedEntry(dwPfn, PTProtections);

        ARMPteUpdateBarrier (ppte, 1);
        NKCacheRangeFlush ((LPVOID)dwPtblAddr, VM_PAGE_SIZE, CACHE_SYNC_FLUSH_D_TLB);
    }
    pPTS->pte[idxPTBL] = dwPtblAddr;
}

void SetupPDEntres (LPDWORD pde, DWORD dwPfn)
{
    // setup page directory entries
    dwPfn |=  PD_COARSE_TABLE | g_pOemGlobal->dwARM1stLvlBits;
    // setup 4 entries in page directory
    if (!pde[0]) pde[0] = dwPfn;
    if (!pde[1]) pde[1] = dwPfn + 0x400;
    if (!pde[2]) pde[2] = dwPfn + 0x800;
    if (!pde[3]) pde[3] = dwPfn + 0xC00;
    ARMPteUpdateBarrier (pde, 4);
}

#ifdef DEBUG
static BOOL ValidatePageTables (PPAGEDIRECTORY ppdir)
{
    int idx;
    int limit = (ppdir == g_ppdirNK)? 0xf00 : 0x700;
    for (idx = 0; idx < limit; idx +=4) {
        if (ppdir->pte[idx] && !IsSectionMapped (ppdir->pte[idx])) {
            PPAGETABLE pptbl = GetPageTable (ppdir, idx);
            if (GetPFN (pptbl) != PFNfromEntry (ppdir->pte[idx])) {
                PPAGETABLE pPTS = (PPAGETABLE) (ppdir+1);
                ERRORMSG (ZONE_ERROR, (L"ValidatePageTables failed!! idx = %8.8lx addresss = %8.8lx value = %8.8lx, expecting = %8.8lx\r\n", 
                    idx, &pPTS->pte[PD2PTIDX(idx)], pPTS->pte[PD2PTIDX(idx)], pPTS->pte[PD2PTIDX(idx)]|0x80000000));
                break;
            }
            DEBUGMSG (ZONE_MEMORY, (L"%8.8lx: %8.8lx %8.8lx %8.8lx\r\n", idx, pptbl, ppdir->pte[idx], GetPFN (pptbl)));
        }
    }
    return limit == idx;
}
#endif

//-----------------------------------------------------------------------------------------------------------------
//
// allocate a 2nd level page table
//
PPAGETABLE AllocatePTBL (PPAGEDIRECTORY ppdir, DWORD idxPD)
{
    DWORD dwPfn = INVALID_PHYSICAL_ADDRESS;
    DWORD flags = PM_PT_ZEROED;
    PPAGETABLE pptbl = NULL;

    DEBUGCHK ((ppdir == g_pprcNK->ppdir) || (idxPD < 0x800));
    
    if (InSysCall ()) {
        // this should only occur during system startup, where the page can be zero'ed with
        // cached address. Make sure we flush both L1 and L2 cache.
        LPVOID pPage = GrabOnePage (flags);
        DEBUGCHK (pPage);
        dwPfn = GetPFN (pPage);
        
    } else if (HLDPG_SUCCESS == HoldMemory (MEMPRIO_KERNEL_INTERNAL, 1)) {
        dwPfn = PHYS_GrabFirstPhysPage (1, &flags, TRUE);
    }

    if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
        PPAGETABLE pPTS = (PPAGETABLE) (ppdir+1);
        DWORD idxPTBL = PD2PTIDX(idxPD);        // convert ARM page directy index to page table index

        DEBUGCHK (!IsValidPTBLEntry (pPTS->pte[idxPTBL]));

        if (!pPTS->pte[idxPTBL]) {
            // VM for kernel page tables are pre-reserved. We should've never
            // get here with ppdir == g_ppdirNK.
            LPVOID pptblVM;
            DEBUGCHK (ppdir != g_ppdirNK);
            
            pptblVM = VMReserve (g_pprcNK, VM_BLOCK_SIZE, 0, 0);
            if (pptblVM) {
                DWORD idxBase = idxPTBL & ~0xf, idx;
                DWORD dwEntry = MakeReservedPTBLEntry (pptblVM);
                for (idx = 0; idx < PAGECOUNT (VM_BLOCK_SIZE); idx ++, dwEntry += VM_PAGE_SIZE) {
                    pPTS->pte[idx+idxBase] = dwEntry;
                }
            }
        }

        if (pPTS->pte[idxPTBL]) {
            // Make the entry valid and Virtual copy the physical page to map 2nd-level page table
            MapPageTable (pPTS, idxPTBL, dwPfn);

            pptbl = (PPAGETABLE) pPTS->pte[idxPTBL];

            if (PM_PT_NEEDZERO & flags) {
                ZeroPage (pptbl);
            }

            NKCacheRangeFlush (pptbl, VM_PAGE_SIZE, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);

            // setup 4 entries in page directory
            SetupPDEntres (&ppdir->pte[idxPD], dwPfn);

            DEBUGCHK (GetPageTable (ppdir, idxPD) == pptbl);

        } else {
            // failed to allocate VM, fail the call.
            FreePhysPage (dwPfn);
        }
    }

    DEBUGMSG (ZONE_VIRTMEM, (L"AllocatePTBL returns %8.8lx\r\n", pptbl));
    DEBUGCHK (ValidatePageTables (ppdir));

    return pptbl;
}

void FreePageTable (PPAGETABLE pptbl)
{
    VERIFY (VMDecommit (g_pprcNK, pptbl, VM_PAGE_SIZE, VM_PAGER_NONE, NULL));
}

// get 2nd level page table from an entry 
PPAGETABLE GetPageTable (PPAGEDIRECTORY ppdir, DWORD idxPD)
{
    PPAGETABLE pPTS = (PPAGETABLE) (ppdir+1);
    DWORD dwEntry = pPTS->pte[PD2PTIDX(idxPD)];
    return IsValidPTBLEntry (dwEntry)? (PPAGETABLE) dwEntry : NULL;
}


//-----------------------------------------------------------------------------------------------------------------
//
// allocate a page directory
//
PPAGEDIRECTORY AllocatePD (void)
{
    PPAGEDIRECTORY ppdir = NULL;
    LPBYTE pReserve = (LPBYTE) VMReserve (g_pprcNK, PDReserveSize, 0, 0);

    // commit a page for "page table" table
    if (pReserve && VMCommit (g_pprcNK, pReserve+sizeof(PAGEDIRECTORY), VM_PAGE_SIZE, PAGE_READWRITE, PM_PT_ZEROED)) {

        const DWORD dwNumPDPages = PDNumPages;
        DWORD dwPfn = GetContiguousPages (dwNumPDPages, PDAlignment, 0);

        if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
            BOOL fNeedFlushCached = !IsFlushUncachedVASupported();

            // always map as cacheable first to flush cache if Flush Uncached VA is not supported.
            VERIFY (VMCopyPhysical (g_pprcNK, (DWORD) pReserve, dwPfn, dwNumPDPages, fNeedFlushCached? PG_KRN_READ_WRITE : PTProtections, FALSE));
            ppdir = (PPAGEDIRECTORY) pReserve;
            NKCacheRangeFlush (ppdir, dwNumPDPages << VM_PAGE_SHIFT, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);

            if (fNeedFlushCached) {
                PTENTRY *ppte, *pptePD;

                // change the cache attributes to specific setting (PTProtections)
                 pptePD = &(GetPageTable (g_ppdirNK, VA2PDIDX (ppdir))->pte[VA2PT2ND (ppdir)]);
                ppte = pptePD + dwNumPDPages;
                while (ppte-- != pptePD) {
                    // ppdir is block aligned, its PTEs must be in the same 2nd-level table (page)
                    DEBUGCHK (PAGEALIGN_DOWN((DWORD)ppte) == PAGEALIGN_DOWN((DWORD)pptePD));
                    *ppte = MakeCommittedEntry(PFNfromEntry(*ppte), PTProtections);
                }

                ARMPteUpdateBarrier (pptePD, dwNumPDPages);
                NKCacheRangeFlush ((LPVOID) ppdir, dwNumPDPages << VM_PAGE_SHIFT, CACHE_SYNC_FLUSH_D_TLB);
            }

            memset (&ppdir->pte[0], 0, 0x1C00);
            memcpy (&ppdir->pte[0x700], &g_ppdirNK->pte[0x700], 0x400);
            if (g_pOemGlobal->pfnPTEUpdateBarrier) {
                g_pOemGlobal->pfnPTEUpdateBarrier (&ppdir->pte[0], dwNumPDPages << VM_PAGE_SHIFT);
            }
        }
    }
    if (!ppdir && pReserve) {
        VERIFY (VMFreeAndRelease (g_pprcNK, pReserve, PDReserveSize));
    }
    return ppdir;
}

//-----------------------------------------------------------------------------------------------------------------
//
// free page directory
//
void FreePD (PPAGEDIRECTORY pd)
{
    PPAGETABLE pPTS = (PPAGETABLE) (pd+1);
    int   idx;

    for (idx = 0; idx < VM_NUM_PT_ENTRIES; idx += PAGECOUNT (VM_BLOCK_SIZE)) {
        if (pPTS->pte[idx]) {
            VERIFY (VMFreeAndRelease (g_pprcNK, (LPVOID)MakeValidPTBLEntry(pPTS->pte[idx]), VM_BLOCK_SIZE));
        }
    }

    VERIFY (VMFreeAndRelease (g_pprcNK, pd, PDReserveSize));
}

void UpdatePageTableProtectionBits (void)
{
    // Page Table protection - user no access, kernel r/w, NX
    PTProtections = PG_VALID_MASK | PG_DIRTY_MASK | PG_PROT_UNO_KRW 
                  | PG_V6_L2_NO_EXECUTE
                  | g_pOemGlobal->dwPageTableCacheBits;
}


//
// reserve VM for kernel page tables. This is only done once during bootup, before cache is enabled.
//
void ReserveVMForKernelPageTables (void)
{
    PPAGETABLE pptbl = GrabOnePage (0);
    DWORD dwPfn      = GetPFN (pptbl);
    DWORD dwKernelPT = g_pprcNK->vaFree;
    DWORD idxPD      = VA2PDIDX(dwKernelPT);
    DWORD dwEntry    = MakeReservedEntry (VM_PAGER_NONE);
    DWORD idx;

    // Page Table protection - user no access, kernel r/w, NX
    UpdatePageTableProtectionBits ();

    DEBUGCHK (!(dwKernelPT & MASK_4M));         // must be 4M aligned
    g_pprcNK->vaFree += SIZE_4M;                // 4M VM needed for kernel page tables

    // the whole 4M is reserved.
    for (idx = 0; idx < VM_NUM_PT_ENTRIES; idx ++) {
        pptbl->pte[idx] = dwEntry;
    }

    // setup page directory entires for this page table
    SetupPDEntres (&g_ppdirNK->pte[idxPD], dwPfn);

    // update page mapping in the page table to this page
    idxPD = PD2PTIDX (idxPD);       // convert page directory index to page table index
    pptbl->pte[idxPD] = dwPfn | PTProtections;

    // update the kernel pagetable mappings to be all "reserved", except the last page, which
    // is already setup in ARMSetup
    pptbl   = (PPAGETABLE) (g_ppdirNK+1);
    dwEntry = MakeReservedPTBLEntry (dwKernelPT);
    for (idx = 0; idx < VM_NUM_PT_ENTRIES-1; idx ++, dwEntry += VM_PAGE_SIZE) {
        pptbl->pte[idx] = dwEntry;
    }

    // map the page table at dwKernelPT
    pptbl->pte[idxPD] = MakeValidPTBLEntry (pptbl->pte[idxPD]);    
}


#define LockAsidList()      EnterCriticalSection(&g_pprcNK->csVM);
#define UnlockAsidList()    LeaveCriticalSection(&g_pprcNK->csVM);

//
// ASID handling
//
static BYTE asidList[MAX_ASID+1];
static DWORD nextASID = 1;

void InitASID (void)
{
    int i;
    DWORD dwTTbr = GetPFN (g_pprcNK->ppdir) | g_pKData->dwTTBRCtrlBits;
    for (i = 1; i < MAX_ASID; i ++) {
        asidList[i] = (BYTE) (i + 1);
    }
    asidList[0] = asidList[i] = 0;

    // setup address above 0x80000000 to use kernel's page directory
    UpdateAsidAndTranslationBase (g_pprcNK->bASID, dwTTbr);
    SetupTTBR1 (dwTTbr);
}

void MDSwitchVM (PPROCESS pprc)
{
    DWORD dwTTbr = GetPFN (pprc->ppdir) | g_pKData->dwTTBRCtrlBits;

    // update TTBR and ASID
    UpdateAsidAndTranslationBase (pprc->bASID, dwTTbr);

    // flush TLB if switching to/from ASID 0
    if (!pVMProc->bASID || !pprc->bASID) {
        DWORD dwSyncFlag = CACHE_SYNC_FLUSH_TLB;
        if (g_pOemGlobal->f_V6_VIVT_ICache) {
            // if platform has VIVT I-Cache, we need to flush I-Cache if we're
            // switch to/from ASID 0
            dwSyncFlag |= CACHE_SYNC_INSTRUCTIONS;
        }
        NKCacheRangeFlush (NULL, 0, dwSyncFlag|CSF_CURR_CPU_ONLY|CSF_PROCESS_SWITCH);
    }
}

DWORD MDGetKernelASID (void)
{
    return MDAllocateASID ();
}

void MDSetCPUASID (void)
{
    PPROCESS pprc = pVMProc;
    if (!pprc->bASID || (GetPFN (pprc->ppdir) != GetTranslationBase ())) {
        MDSwitchVM (pprc);
    }
}

DWORD MDAllocateASID (void)
{
    DWORD dwRet = 0;
    PREFAST_DEBUGCHK (nextASID <= MAX_ASID);
    LockAsidList ();
    dwRet = nextASID;
    nextASID = asidList[dwRet];
    UnlockAsidList ();
    return dwRet;
}

void MDFreeASID (DWORD dwASID)
{
    if (dwASID) {
        PREFAST_DEBUGCHK (dwASID <= MAX_ASID);
        LockAsidList ();
        asidList[dwASID] = (BYTE) nextASID;
        nextASID = dwASID;
        UnlockAsidList ();
    }
}



