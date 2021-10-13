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
//
//    vmarm.c - ARM specific VM implementations
//
#include <windows.h>
#include <vm.h>
#include <kernel.h>


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

//-----------------------------------------------------------------------------------------------------------------
//
//  PageParamFormProtect: return page protection from the PAGE_xxx flags
//
DWORD PageParamFormProtect (DWORD fProtect, DWORD dwAddr)
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
            dwPgProt |= PG_CACHE;
        }

        if (IsV6OrLater () && (VM_SHARED_HEAP_BASE > dwAddr)) {
            // user address, not global
            dwPgProt |= PG_V6_NOT_GLOBAL;
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

    if ((PG_VALID_MASK|PG_1K_MASK) & dwEntry) {
        // committed entry
        dwProtect = (PG_PROT_READ == (dwEntry & PG_PROTECTION))
                                ? PAGE_READONLY
                                : PAGE_READWRITE;
    } else if (PG_GUARD & dwEntry) {
        // guard page
        dwProtect = (PG_V6_PROT_READ == (dwEntry & PG_V6_PROTECTION))
                                ? PAGE_READONLY
                                : PAGE_READWRITE;
        dwProtect |= PAGE_GUARD;
    } else {
        dwProtect = PAGE_NOACCESS;
    }

    if ((PG_NOACCESS != dwProtect)
        && !(PG_CACHE & dwEntry)) {
        dwProtect |= PAGE_NOCACHE;
    }


    return dwProtect;
}

// commit guard page (uncached guard pages not supported)
DWORD CommitGuardPage (DWORD dwEntry)
{
    DEBUGCHK (IsGuardPage (dwEntry));

    if (IsV6OrLater ()) {
        // v6 or later, just take aways the guard bit and make page valid
        dwEntry = (dwEntry & ~PG_GUARD) | PG_VALID_MASK;
    } else {
        DWORD dwProt = 0;

        // v5 or earlier, we use v6 protection bits to keep track of the
        // page protect. Convert it to v4 protection.
        switch (dwEntry & PG_V6_PROTECTION) {
        case PG_V6_PROT_READ:
            dwProt = PG_V4_PROT_READ;
            break;
        case PG_V6_PROT_WRITE:
            dwProt = PG_V4_PROT_WRITE;
            break;
        case PG_V6_PROT_UNO_KRW:
            dwProt = PG_V4_PROT_UNO_KRW;
            break;
        case PG_V6_PROT_URO_KRW:
            dwProt = PG_V4_PROT_URO_KRW;
            break;
        default:
            DEBUGCHK (0);
        }
        dwEntry = (dwEntry & ~(PG_V4_PROTECTION)) | dwProt | PG_VALID_MASK;
    }

    return dwEntry;
}

//
// MakeWritableEntry: create a page table entry for writable page
//
DWORD MakeWritableEntry (DWORD dwPFN, DWORD dwAddr)
{
    DWORD dwEntry = dwPFN | GetWriteProtectBits (dwAddr) | PG_VALID_MASK | PG_CACHE;

    if (IsV6OrLater () && (VM_SHARED_HEAP_BASE > dwAddr)) {
        dwEntry |= PG_V6_NOT_GLOBAL;
    }

    return dwEntry;
}


//-----------------------------------------------------------------------------------------------------------------
//
// InvalidatePages - invalidate a range of Virtual Addresses
//
void InvalidatePages (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{
    if (IsV6OrLater ()) {
        // for v6 or later, only flush TLB

        if ((pprc != pVMProc) && (VM_SHARED_HEAP_BASE > dwAddr)) {
            // if non-global address, and not current VM, flush all TLB
            dwAddr = cPages = 0;
        }
        OEMCacheRangeFlush ((LPVOID) dwAddr, cPages << VM_PAGE_SHIFT, CACHE_SYNC_FLUSH_TLB);
    } else {
        // flush everything if pre-v6
        OEMCacheRangeFlush ((LPVOID) dwAddr, cPages << VM_PAGE_SHIFT, (CACHE_SYNC_FLUSH_TLB|CACHE_SYNC_DISCARD|CACHE_SYNC_INSTRUCTIONS));
    }
} 

//-----------------------------------------------------------------------------------------------------------------
//
// allocate a 2nd level page table
//
PPAGETABLE AllocatePTBL (void)
{
    PPAGETABLE pptbl = (PPAGETABLE) GrabOnePage (PM_PT_ZEROED);

    if (pptbl) {
        OEMCacheRangeFlush (pptbl, VM_PAGE_SIZE, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);
        // ARM page table/directory may not be cacheable
        if (!g_pOemGlobal->dwTTBRCacheBits) {
            pptbl = (PPAGETABLE) ((DWORD) pptbl | 0x20000000);
        }
    }

    return pptbl;
}

//
// V6 or later has address above 0x80000000 reference to kernel page directory automatically.
// Therefore, only 2 pages, aligned at 8K, are needed for user processes page directory
//
#define PDNumPages      (IsV6OrLater()? 2 : 4)
#define PDAlignment     (IsV6OrLater()? 0x1fff : 0x3fff)


//-----------------------------------------------------------------------------------------------------------------
//
// allocate a page directory
//
PPAGEDIRECTORY AllocatePD (void)
{
    DWORD dwPfn = GetContiguousPages (PDNumPages, PDAlignment, 0);
    PPAGEDIRECTORY ppdir = NULL;

    if (INVALID_PHYSICAL_ADDRESS != dwPfn) {
        ppdir = (PPAGEDIRECTORY) ( ( g_pOemGlobal->dwTTBRCacheBits ) ? Pfn2Virt(dwPfn) : Pfn2VirtUC (dwPfn) );
        DEBUGCHK (ppdir);

        OEMCacheRangeFlush ((LPVOID)((DWORD)ppdir &~0x20000000), VM_PAGE_SIZE*PDNumPages, CACHE_SYNC_DISCARD|CACHE_SYNC_L2_DISCARD);

        // clear the PTEs below shared heap
        memset (&ppdir->pte[0], 0, 0x1C00);
        // copy shared heap and above for pre-v6 CPUs, shared heap only for V6 or later.
        if (!IsV6OrLater ()) {
            // from shared heap and above shared
            memcpy (&ppdir->pte[0x700], &g_ppdirNK->pte[0x700], 0x2400);
        } else {
            // from shared heap only
            memcpy (&ppdir->pte[0x700], &g_ppdirNK->pte[0x700], 0x0400);
        }

    }
    
    return ppdir;
}

//-----------------------------------------------------------------------------------------------------------------
//
// free page directory
//
void FreePD (PPAGEDIRECTORY pd)
{
    DWORD dwPfn = GetPFN (pd);
    int cPages = PDNumPages;
    int i;
    for (i = 0; i < cPages; i ++) {
        FreePhysPage (dwPfn);
        dwPfn = NextPFN (dwPfn);
    }
}

//-----------------------------------------------------------------------------------------------------------------
//
// setup page directory for a 4K page table
//
void SetupPDEntry (PPAGEDIRECTORY ppdir, DWORD idxdir, PPAGETABLE pptbl, BOOL fkmode)
{
    DWORD dwEntry = GetPFN (pptbl) | PD_COARSE_TABLE | g_pOemGlobal->dwARM1stLvlBits;

    DEBUGCHK (!IsV6OrLater () || (ppdir == g_pprcNK->ppdir) || (idxdir < 0x800));

    DEBUGMSG (ZONE_VIRTMEM, (L"Setting up Page Directory at idx 0x%8.8lx, with %8.8lx\r\n", idxdir, dwEntry));
    // setup 4 entries in page directory
    ppdir->pte[idxdir]   = dwEntry;
    ppdir->pte[idxdir+1] = dwEntry + 0x400;
    ppdir->pte[idxdir+2] = dwEntry + 0x800;
    ppdir->pte[idxdir+3] = dwEntry + 0xC00;

    ARMPteUpdateBarrier(&ppdir->pte[idxdir], 4);
}

// max set to 0xfe. 0xff is invalid, used to deal with prefetch and branch prediction during ASID/TTBR update.
#define INVALID_ASID    0xff
#define MAX_ASID        0xfe

void UpdateTranslationBase (DWORD dwPfn);
DWORD GetTranslationBase (void);
void SetupTTBR1 (DWORD dwPfn);
void ARMSetASID (DWORD dwASID);


//
// ASID handling
//
static BYTE asidList[MAX_ASID+1];
static DWORD nextASID = 1;

void InitASID (void)
{
    if (IsV6OrLater ()) {
        int i;
        for (i = 1; i < MAX_ASID; i ++) {
            asidList[i] = (BYTE) (i + 1);
        }
        asidList[0] = asidList[i] = 0;

        // setup address above 0x80000000 to use kernel's page directory
        SetupTTBR1 (GetPFN (g_pprcNK->ppdir));
    }
}

void MDSwitchVM (PPROCESS pprc)
{
    if (IsV6OrLater ()) {
        // flush TLB if switching to/from ASID 0
        if (!pVMProc->bASID || !pprc->bASID) {
            if (g_pOemGlobal->f_V6_VIVT_ICache) {
                // if platform has VIVT I-Cache, we need to flush I-Cache if we're
                // switch to/from ASID 0
                OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_INSTRUCTIONS);
            }
            OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_FLUSH_TLB);
        }

        // adjust ASID and TTBR
        UpdateAsidAndTranslationBase (pprc->bASID,(GetPFN(pprc->ppdir)|g_pKData->dwTTBRCtrlBits) );
        
    } else {
        // pre-v6, VIVT cache, flush everything.
        OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_FLUSH_TLB|CACHE_SYNC_DISCARD|CACHE_SYNC_INSTRUCTIONS);

        // update TTBR0 - MUST BE LAST INSTRUCTION
        UpdateTranslationBase (GetPFN (pprc->ppdir));
    }

}

DWORD MDGetKernelASID (void)
{
    return MDAllocateASID ();
}

void MDSetCPUASID (void)
{
    PPROCESS pprc = pVMProc;
    if (GetPFN (pprc->ppdir) != GetTranslationBase ()) {
        MDSwitchVM (pprc);
    } else if (IsV6OrLater ()) {
        ARMSetASID (pprc->bASID);
    }
}

DWORD MDAllocateASID (void)
{
    DWORD dwRet = 0;
    if (IsV6OrLater ()) {
        PREFAST_DEBUGCHK (nextASID <= MAX_ASID);
        LockLoader (g_pprcNK);
        dwRet = nextASID;
        nextASID = asidList[dwRet];
        UnlockLoader (g_pprcNK);
    }
    return dwRet;
}

void MDFreeASID (DWORD dwASID)
{
    if (dwASID) {
        DEBUGCHK (IsV6OrLater ());
        PREFAST_DEBUGCHK (dwASID <= MAX_ASID);
        LockLoader (g_pprcNK);
        asidList[dwASID] = (BYTE) nextASID;
        nextASID = dwASID;
        UnlockLoader (g_pprcNK);
    }
}

void ARMPteUpdateBarrier (LPDWORD pdwEntry, DWORD cEntries)
{
    if (g_pOemGlobal->pfnPTEUpdateBarrier) {
        g_pOemGlobal->pfnPTEUpdateBarrier (pdwEntry, cEntries * sizeof (DWORD));
    } else if (IsV6OrLater ()) {
        V6_WriteBarrier (); // DSB
    }    
}

// get 2nd level page table from an entry 
PPAGETABLE Entry2PTBL (DWORD dwEntry)
{
    return (dwEntry && !IsSectionMapped (dwEntry))? ((g_pOemGlobal->dwTTBRCacheBits)?(PPAGETABLE)Pfn2Virt(PFNfromEntry(dwEntry)):(PPAGETABLE)Pfn2VirtUC(PFNfromEntry(dwEntry))) : NULL;
}

