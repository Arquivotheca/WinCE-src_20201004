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
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    vm_x86.h - x86 specific VM settings
//
#ifndef _VM_X86_H_
#define _VM_X86_H_

/* Page permission bits */
#define PG_PERMISSION_MASK      0x000007DF  // exclude PG_ACCESSED_MASK
#define PG_PHYS_ADDR_MASK       0xFFFFF000    
#define PG_VALID_MASK           0x00000001
#define PG_WRITE_MASK           0x00000002
#define PG_USER_MASK            0x00000004
#define PG_WRITE_THRU_MASK      0x00000008
#define PG_CACHE_MASK           0x00000010
#define PG_NOCACHE              0x00000010
#define PG_CACHE                0
#define PG_ACCESSED_MASK        0x00000020
#define PG_DIRTY_MASK           0x00000040
#define PG_LARGE_PAGE_MASK      0x00000080  // Set on page directory entries only
#define PG_PAT_MASK             0x00000080  // Set on page table entries only
#define PG_GLOBAL_MASK          0x00000100
#define PG_EXECUTE_MASK         0x00000200  // Unused by hardware    
#define PG_GUARD                0x00000400  // Software field used to return non zero from MakePagePerms
#define PG_SIZE_MASK            0

#define PG_SECTION_OFST_MSAK    0x003FF000  // middle 10 bits

#define PG_SECTION_PROTECTION   (PG_VALID_MASK|PG_LARGE_PAGE_MASK|PG_WRITE_MASK|PG_DIRTY_MASK)

#define PG_NOACCESS             PG_USER_MASK
#define PG_PROT_READ            PG_USER_MASK
#define PG_PROT_WRITE          (PG_USER_MASK | PG_WRITE_MASK)
#define PG_PROTECTION          (PG_USER_MASK | PG_WRITE_MASK)
#define PG_PROT_UNO_KRW         PG_WRITE_MASK

#define PG_READ_WRITE          (PG_VALID_MASK | PG_DIRTY_MASK | PG_CACHE | PG_PROT_WRITE)
#define PG_KRN_READ_WRITE      (PG_VALID_MASK | PG_DIRTY_MASK | PG_CACHE | PG_WRITE_MASK)

// PG_CHANGEABLE defines all the bits that can be changed via VirtualSetAttributes
// any other bits are reserved by kernel and cannot be changed.
#define PG_CHANGEABLE          (PG_CACHE_MASK | PG_WRITE_THRU_MASK | PG_PAT_MASK)

//
// The following definition is valid only if PG_VALID is not set.
// MUST NOT COLIDE WITH PG_VALID
//
#define PG_RESERVED_PAGE        0x00000080  // the page is reserved only
#define PG_PAGER_MASK           0x00000300  // 2 bits for pager type
#define PG_PAGER_SHIFT          8

#define PG_AUTO_COMMIT          (VM_PAGER_AUTO   << PG_PAGER_SHIFT)  // the page is an auto-commit page
#define PG_LOADER               (VM_PAGER_LOADER << PG_PAGER_SHIFT)  // the page is paged by loader
#define PG_MAPPER               (VM_PAGER_MAPPER << PG_PAGER_SHIFT)  // the page is paged by memory mapped file

//
// useful macros
//
#define PFN_INCR                    VM_PAGE_SIZE
#define PFN_SHIFT                   0
#define NextPFN(pfn)                ((pfn) + PFN_INCR)
#define PFNfromEntry(entry)         ((entry) & PG_PHYS_ADDR_MASK)
#define PFNfromSectionEntry(entry)  PFNfromEntry(entry)
#define PFN2PA(pfn)                 (pfn)
#define PA2PFN(pfn)                 (pfn)
#define PFNfrom256(pg256)           (((ulong)(pg256)<<8) & ~VM_PAGE_OFST_MASK)
#define SectionOfst(addr)           ((ulong) (addr) & PG_SECTION_OFST_MSAK)
#define PA256FromPfn(pfn)           ((pfn) >> 8)

#define GetPDEntry(ppdir, va)       ((ppdir)->pte[VA2PDIDX (va)])

#define ReadOnlyEntry(ent)          ((ent) & ~(PG_WRITE_MASK|PG_WRITE_THRU_MASK))

// Test the access on a page table entry
#define IsPageWritable(entry)       (((entry) & (PG_WRITE_MASK|PG_VALID_MASK)) == (PG_WRITE_MASK|PG_VALID_MASK))
#define IsPageReadable(entry)       ((entry) & PG_VALID_MASK)
#define IsPageCommitted(entry)      ((DWORD) (entry) > VM_PAGE_SIZE)
#define IsSectionMapped(entry)      (PG_LARGE_PAGE_MASK & (entry))
#define IsPDEntryValid(entry)       ((entry) & PG_VALID_MASK)
#define IsGuardPage(entry)          (PG_GUARD & (entry))

// 1st level page table is CPU dependent
// find 1st level page table index from va

#define VM_NUM_PD_ENTRIES           VM_NUM_PT_ENTRIES

#define VM_SECTION_SHIFT            22

#define VA2PDIDX(va)                ((DWORD) (va) >> VM_SECTION_SHIFT)

#define NextPDEntry(idxdir)         ((idxdir)+1)
#define PrevPDEntry(idxdir)         ((idxdir)-1)

// commit guard page
#define CommitGuardPage(entry)      (((entry) & ~PG_GUARD) | PG_VALID_MASK)

#define IsGlobalPageSupported()     (g_pKData->dwCpuCap & CPUID_PGE)

//
// ASID related function - no ASID
//
#define MDAllocateASID()        0
#define MDGetKernelASID()       0
#define MDFreeASID(x)
#define MDSwitchActive(x)

//
// functions prototypes
//

//
// IsSameEntryType: check if 2 entris are of the same protection/reservation
//
__inline BOOL IsSameEntryType (DWORD dwEntry1, DWORD dwEntry2)
{
    return (dwEntry1 & PG_PERMISSION_MASK) == (dwEntry2 & PG_PERMISSION_MASK);
}


//
// MakeCommittedEntry: create a page table entry for committed page
//
__inline DWORD MakeCommittedEntry (DWORD dwPFN, DWORD dwPgProt)
{
    return dwPFN | dwPgProt;
}

//
// MakeWritableEntry: create a page table entry for writable page
//
__inline DWORD MakeWritableEntry (PPROCESS pprc, DWORD dwPFN, DWORD dwAddr)
{
    UNREFERENCED_PARAMETER (pprc);
    return dwPFN
        | PG_WRITE_MASK
        | PG_VALID_MASK
        | ((dwAddr < VM_KMODE_BASE)
            ? PG_USER_MASK
            : 0);
}

//
// MakeReservedEntry: create a page table entry for reserved page
//
__inline DWORD MakeReservedEntry (DWORD dwPagerType)
{
    return ((VM_PAGER_NONE == dwPagerType)? PG_RESERVED_PAGE : (dwPagerType << PG_PAGER_SHIFT));
}

__inline DWORD GetPagerType (DWORD dwEntry)
{
    return (dwEntry & PG_PAGER_MASK) >> PG_PAGER_SHIFT;
}

extern PCDeviceTableEntry g_pOEMDeviceTable;
extern PADDRMAP g_pOEMAddressTable;

#endif _VM_X86_H_

