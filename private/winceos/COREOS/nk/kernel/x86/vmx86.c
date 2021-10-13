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
//    vmx86.c - x86 specific VM implementations
//
#include <windows.h>
#include <vm.h>
#include <kernel.h>

//-----------------------------------------------------------------------------------------------------------------
//
//  PageParamFormProtect: return page protection from the PAGE_xxx flags
//
DWORD PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr)
{
    DWORD dwPgProt = (fProtect & PAGE_WRITE_THRU)? (PG_WRITE_THRU_MASK|PG_PROT_WRITE) : 0;
    fProtect &= ~PAGE_WRITE_THRU;

    if (!KSEN_PageParamFormProtect(pprc, fProtect, dwAddr)) {
        DEBUGCHK (!(fProtect & ~VM_VALID_PROT));

        if (PAGE_NOACCESS == fProtect)
            dwPgProt = PG_NOACCESS;

        else {
            if (VM_READWRITE_PROT & fProtect) {
                dwPgProt |= PG_PROT_WRITE;
            } else {
                dwPgProt = PG_PROT_READ;
            }

            if (dwAddr >= VM_KMODE_BASE) {
                dwPgProt &= ~PG_USER_MASK;
            }
            
            if (VM_EXECUTABLE_PROT & fProtect) {
                dwPgProt |= PG_EXECUTE_MASK;
            }

            if (PAGE_NOCACHE & fProtect) {
                dwPgProt |= PG_NOCACHE;
            }

            dwPgProt |= (PAGE_GUARD & fProtect)? PG_GUARD : PG_VALID_MASK;
        }
    }

    KSLV_PageParamFormProtect (pprc, fProtect, dwAddr, &dwPgProt);    
    return dwPgProt;
}


//-----------------------------------------------------------------------------------------------------------------
//
// ProtectFromEntry - get page protection (PAGE_XXX) from an entry 
//
DWORD ProtectFromEntry (DWORD dwEntry)
{
    DWORD dwProtect;
    BOOL fExecute = (PG_EXECUTE_MASK & dwEntry);

    if (!KSEN_ProtectFromEntry (dwEntry)) {

        if (PG_WRITE_MASK & dwEntry) {
            dwProtect = fExecute? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
        } else {
            dwProtect = fExecute? PAGE_EXECUTE_READ : PAGE_READONLY;
        }

        if (PG_GUARD & dwEntry) {
            dwProtect |= PAGE_GUARD;
            
        } else if (!(PG_VALID_MASK & dwEntry)) {
            // not guard, not valid - no access
            dwProtect = PAGE_NOACCESS;
        }

        if (PG_NOCACHE & dwEntry) {
            dwProtect |= PAGE_NOCACHE;
        }
    }

    KSLV_ProtectFromEntry (dwEntry, &dwProtect);
    return dwProtect;
}

//-----------------------------------------------------------------------------------------------------------------
//
// InvalidatePages - invalidate a range of Virtual Addresses
//
void InvalidatePages (PPROCESS pprc, DWORD dwAddr, DWORD cPages)
{ 
    // we'll flush TLB on process switch.
    NKCacheRangeFlush (0, 0, CACHE_SYNC_FLUSH_TLB);
} 


//-----------------------------------------------------------------------------------------------------------------
//
// allocate a 2nd level page table
//
PPAGETABLE AllocatePTBL (PPAGEDIRECTORY ppdir, DWORD idxPD)
{
    PPAGETABLE pptbl = (PPAGETABLE) GrabOnePage (PM_PT_ZEROED);
    if (pptbl) {
        ppdir->pte[idxPD] = (DWORD) GetPFN (pptbl) 
                          | (((ppdir == g_ppdirNK) || (idxPD >= VA2PDIDX(VM_SHARED_HEAP_BASE)))
                                ? PG_KRN_READ_WRITE
                                : PG_READ_WRITE);
    }
    return pptbl;
}

// get 2nd level page table from an entry 
PPAGETABLE GetPageTable (PPAGEDIRECTORY ppdir, DWORD idx)
{
    DWORD dwEntry = ppdir->pte[idx];
    return (dwEntry && !IsSectionMapped (dwEntry))? (PPAGETABLE) Pfn2Virt (PFNfromEntry (dwEntry)) : NULL;
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

//-----------------------------------------------------------------------------------------------------------------
//
// cpu dependent code to switch page directory
//
void MDSwitchVM (PPROCESS pprc)
{
    if (!InPrivilegeCall()) {
        // access cr0 require ring-0 privilege. Use KCall to get to ring-0. The KCall can be removed if we
        // have kernel running at ring-0 (currently kernel running at ring-1)
        KCall ((PKFN) MDSwitchVM, pprc);
    } else {
        DWORD dwPFN = GetPFN (pprc->ppdir);
        
        // update CR3
        _asm {
            mov eax, dwPFN
            mov cr3, eax        // update MUST BE LAST INSTRUCTION
        }
    }
}

void MDSetCPUASID (void)
{
    DWORD dwPFN = GetPFN (pVMProc->ppdir);

    // update CR3 only when it's dirrent from current VM, for CR3 update will flush TLB
    _asm { 
        mov eax, cr3
        mov ecx, dwPFN
        cmp eax, ecx
        je  noUpdate
        mov cr3, ecx
    noUpdate:
    }
}

void SetFPEmul (PTHREAD pth, DWORD dwMode)
{
    DEBUGCHK (g_pCoredllFPEmul);
        
    // if using floating point emulation, switch to the right handler
    if (USER_MODE == dwMode) {
        pth->lpFPEmul = g_pCoredllFPEmul;
    } else {
        pth->lpFPEmul = g_pKCoredllFPEmul;
    }

    if (!InSysCall ()) {        
        KCall ((PKFN) InitFPUIDTs, pth->lpFPEmul);

    } else {
        InitFPUIDTs (pth->lpFPEmul);
    }
}

