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

/*
    mdmem.h - machine dependant memory management structures
*/

#ifndef MEM_MIPS_H
#define MEM_MIPS_H

#if !R4000
    #error CPU not supported
#endif

/* Page permission bits */
#define PG_PERMISSION_MASK      0xC000003F
#define PG_GLOBAL_MASK          0x00000001
#define PG_VALID_MASK           0x00000002
#define PG_DIRTY_MASK           0x00000004
#define PG_CACHE_MASK           0x00000038  /* should be ANDed to find out cache state */
#define PG_NOCACHE              0x00000010
#define PG_CACHE                0x00000018
#define PG_PROT_READ            0x00000000
#define PG_PROT_WRITE           PG_DIRTY_MASK
#define PG_SIZE_MASK            0x00000000
#define PG_PROTECTION           (PG_PROT_READ | PG_PROT_WRITE) 
#define PG_READ_WRITE           (PG_DIRTY_MASK | PG_VALID_MASK | PG_CACHE)
#define PG_KRN_READ_WRITE       PG_READ_WRITE

// the following 2 bits can be set on a committed pages, while PG_VALID is false
#define PG_GUARD                0x80000000  // (unused by software, but should clean before writing TLB)
#define PG_NOACCESS             0x40000000  // (unused by software, but should clean before writing TLB)

// PG_CHANGEABLE defines all the bits that can be changed via VirtualSetAttributes
// any other bits are reserved by kernel and cannot be changed.
#define PG_CHANGEABLE           PG_CACHE_MASK


// bits in reserved pages (not committed), only valid when PG_VALID is NOT set
#define PG_RESERVED_PAGE        0x10
#define PG_PAGER_MASK           0x0c        
#define PG_PAGER_SHIFT          2

#define PG_AUTO_COMMIT          (VM_PAGER_AUTO   << PG_PAGER_SHIFT)  // the page is an auto-commit page
#define PG_LOADER               (VM_PAGER_LOADER << PG_PAGER_SHIFT)  // the page is paged by loader
#define PG_MAPPER               (VM_PAGER_MAPPER << PG_PAGER_SHIFT)  // the page is paged by memory mapped file

// other constants
#define PFN_SHIFT_TINY_PAGE     4                   // pfn shift when tiny page is supported
#define PFN_SHIFT_MIPS32        6                   // pfn shift for standard MIPS32/MIPS64 (no tiny page)

#define PFN_SHIFT               (g_pKData->dwPfnShift)
#define PFN_INCR                (g_pKData->dwPfnIncr)

#define VM_SECTION_SHIFT        22

/* Find the page frame # from a GPINFO pointer */

#define PHYS_MASK               0x1FFFF000          // phys page mask for address between 0x80000000-0xc0000000
#define NextPFN(pfn)            ((pfn) + PFN_INCR)
#define PFNfromEntry(entry)     ((entry) & ~PG_PERMISSION_MASK)
#define PFNfromSectionEntry(entry)  PFNfromEntry(entry)
#define PFNfrom256(x)           (((ulong)(x)<<(8-PFN_SHIFT)) & ~PG_PERMISSION_MASK)
#define PFN2PA(pfn)             ((pfn) << PFN_SHIFT)
#define PA2PFN(pfn)             ((pfn) >> PFN_SHIFT)
#define PA256FromPfn(pfn)       ((pfn) >> (8-PFN_SHIFT))

#define ReadOnlyEntry(entry)    (((entry) & ~PG_PROTECTION)|PG_PROT_READ)

/* Test the write access on a page table entry */
#define IsPageReadable(entry)   (((entry)&PG_VALID_MASK) == PG_VALID_MASK)
#define IsPageWritable(entry)   (((entry)&(PG_VALID_MASK|PG_DIRTY_MASK)) == (PG_VALID_MASK|PG_DIRTY_MASK))
#define IsPageCommitted(entry)  ((entry) > 0x3f)
#define IsGuardPage(entry)      (PG_GUARD & (entry))

// commit guard page
#define CommitGuardPage(entry)  (((entry) & ~PG_GUARD) | PG_VALID_MASK)

#define PG_SECTION_OFST_MSAK    0x003FF000  // middle 10 bits
#define SectionOfst(addr)       (((ulong) (addr) & PG_SECTION_OFST_MSAK) >> PFN_SHIFT)
#define IsSectionMapped(entry)  ((int) (entry) > 0)     // MSB not set, and not 0
#define IsPDEntryValid(entry)   ((entry) != 0)

#define GetPDEntry(ppdir, va)   ((ppdir)->pte[VA2PDIDX (va)])

#define KPAGE_PTE               0   // UserKData is hard wired in MIPS

#define VM_NUM_PD_ENTRIES       VM_NUM_PT_ENTRIES

#define VA2PDIDX(va)            ((DWORD) (va) >> VM_SECTION_SHIFT)

#define NextPDEntry(idxdir)     ((idxdir)+1)
#define PrevPDEntry(idxdir)     ((idxdir)-1)

//
// mode definition
//
#define KERNEL_MODE                 0x03
#define USER_MODE                   0x13
#define MODE_MASK                   0x1F


#ifndef ASM_ONLY
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
    return dwPFN | PG_READ_WRITE;
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


//
// ASID related function
//
DWORD MDAllocateASID (void);
DWORD MDGetKernelASID (void);
void MDFreeASID (DWORD dwASID);
void MDSwitchActive (PPROCESS pprc);


#endif  // ASM_ONLY

#endif // MEM_MIPS_H

