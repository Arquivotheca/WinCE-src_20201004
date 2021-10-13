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
//    vmshx.c - SHx specific VM implementations
//
#include <windows.h>
#include <vm.h>
#include <kernel.h>
#include "shx.h"

//-----------------------------------------------------------------------------------------------------------------
//
//  PageParamFormProtect: return page protection from the PAGE_xxx flags
//
DWORD PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr)
{
    DWORD dwPgProt = PG_SIZE_MASK;

    DEBUGCHK (!(fProtect & ~VM_VALID_PROT));

    if (PAGE_NOACCESS == fProtect)
        dwPgProt |= PG_NOACCESS;
    
    else {

        dwPgProt |= (VM_READWRITE_PROT & fProtect) 
                        ? PG_PROT_WRITE | PG_DIRTY_MASK
                        : PG_PROT_READ | PG_DIRTY_MASK;

        dwPgProt |= (PAGE_GUARD & fProtect)
                        ? PG_GUARD
                        : PG_VALID_MASK;


        dwPgProt |= (PAGE_NOCACHE & fProtect)
                        ? PG_NOCACHE
                        : PG_CACHE;

        if (IsKModeAddr (dwAddr)) {
            dwPgProt |= PG_GLOBAL_MASK;
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

    if ((PG_VALID_MASK|PG_GUARD) & dwEntry) {
        // committed entry
        dwProtect = (PG_PROT_READ == (dwEntry & PG_PROTECTION))
                                ? PAGE_READONLY
                                : PAGE_READWRITE;

        if (IsGuardPage (dwEntry)) {
            dwProtect |= PAGE_GUARD;
        }

        if ((dwEntry & PG_CACHE_MASK) == PG_NOCACHE) {
            dwProtect |= PAGE_NOCACHE;
        }
    } else {
        dwProtect = PAGE_NOACCESS;
    }

    return dwProtect;
}


//-----------------------------------------------------------------------------------------------------------------
//
// InvalidatePages - invalidate a range of Virtual Addresses
//
void InvalidatePages (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{ 
    DWORD dwReason = dwAddr? CSF_VM_CHANGE : CSF_PROCESS_EXIT;
    if ((pprc != pVMProc) && (VM_SHARED_HEAP_BASE > dwAddr)) {
        // if non-global address, and not current VM, flush all TLB
        dwAddr = cPages = 0;
    }
    // To avoid CPU core changing during this call we must call NKCacheRangeFlush() instead of
    // OEMCacheRangeFlush() function.
    NKCacheRangeFlush ((LPVOID) dwAddr, cPages << VM_PAGE_SHIFT, CACHE_SYNC_FLUSH_TLB|dwReason);
} 

//-----------------------------------------------------------------------------------------------------------------
//
// allocate a 2nd level page table
//
PPAGETABLE AllocatePTBL (PPAGEDIRECTORY ppdir, DWORD idxPD)
{
    PPAGETABLE pptbl = (PPAGETABLE) GrabOnePage (PM_PT_ZEROED);
    if (pptbl) {
        ppdir->pte[idxPD] = (DWORD) pptbl;
    }
    return pptbl;
}

// get 2nd level page table from an entry 
PPAGETABLE GetPageTable (PPAGEDIRECTORY ppdir, DWORD idx)
{
    DWORD dwEntry = ppdir->pte[idx];
    return (dwEntry && !IsSectionMapped (dwEntry))? (PPAGETABLE) dwEntry : NULL;
}

//-----------------------------------------------------------------------------------------------------------------
//
// allocate a page directory
//
PPAGEDIRECTORY AllocatePD (void)
{
    PPAGEDIRECTORY ppdir = (PPAGEDIRECTORY) GrabOnePage (PM_PT_ZEROED);
    if (ppdir) {
        // copy the shared page directory entry from the global page directory
        memcpy (&ppdir->pte[512], &g_ppdirNK->pte[512], VM_PAGE_SIZE/2);
    }

    return ppdir;
}

//-----------------------------------------------------------------------------------------------------------------
//
// free page directory
//
void FreePD (PPAGEDIRECTORY pd)
{
    FreePhysPage (GetPFN (pd));
}

//
// _stricmp is not in fulllibc.c, kind of strange...
//
int _stricmp (const char * str1, const char * str2)
{
    int ret = 0;

    while ((0 == (ret = _tolower(*str1) - _tolower(*str2))) && *str2) {
        ++str1;
        ++str2;
    }
    return ret;
    
}

// page directory
static PAGEDIRECTORY _pdir;
static PAGETABLE     _ptblHigh, _ptblLow;

#define MAX_ASID        0x7f
#define KERNEL_ASID     0x80        // kerenl occupies the topmost bit, and will be or'd with the 
                                    // current ASID if current process is kernel.
                                    // With this, we will not need to flush TLB on process switch.
                                    // The only drawback is that the # of available ASIDs is cut into half

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
    for (i = 1; i < MAX_ASID; i ++) {
        asidList[i] = (BYTE) (i + 1);
    }
    asidList[0] = asidList[i] = 0;
}

void SHSetASID (DWORD dwASID);

void MDSwitchActive (PPROCESS pprc)
{
    DWORD dwASID = pprc->bASID;
    
    // flush TLB is we're switch from or to a process with ASID = 0 (out of ASID)
    if (!dwASID || !pActvProc->bASID || !pVMProc->bASID) {
        OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_FLUSH_TLB|CSF_PROCESS_SWITCH);
    }

    // if switching to kernel, the new asid will be the combination of current VM, and kernel's ASID
    if (KERNEL_ASID == dwASID) {
        dwASID |= pVMProc->bASID;
    }
    SHSetASID (dwASID);
}

void MDSwitchVM (PPROCESS pprc)
{
    if (pActvProc == g_pprcNK) {
        // VM change inside kernel, adjust ASID
        SHSetASID (pprc->bASID|KERNEL_ASID);
    }
    MMUTTB = (DWORD)pprc->ppdir; /* set translation table base address */    
}

void MDSetCPUASID (void)
{
    PPROCESS pprc = pActvProc;
    MDSwitchActive (pprc);
    MMUTTB = (DWORD)pVMProc->ppdir; /* set translation table base address */    
}

DWORD MDAllocateASID (void)
{
    DWORD dwRet;
    PREFAST_DEBUGCHK (nextASID <= MAX_ASID);
    LockAsidList ();
    dwRet = nextASID;
    nextASID = asidList[dwRet];
    UnlockAsidList ();
    return dwRet;
}

DWORD MDGetKernelASID (void)
{
    return KERNEL_ASID;
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

void InitPageDirectory (void)
{
    int idx;
    DWORD dwAddr = VM_KMODE_BASE;
    g_ppdirNK = &_pdir;

    // section map for address form 0x80000000 - oxc0000000
    for (idx = 0x200; idx < 0x300; idx ++, dwAddr += (1<<22)) {
        g_ppdirNK->pte[idx] = (dwAddr & PHYS_MASK) | PG_VALID_MASK;
    }
    
    // setup mapping for 0x00005000, r/0 1 page
    g_ppdirNK->pte[0] = (DWORD) &_ptblLow;    
    _ptblLow.pte[5]  = ((ulong)g_pKData-0x800 & PHYS_MASK) + 0x15a; // user read-only, 4k, cachable, !dirty, shared, valid
    InitASID ();
}



