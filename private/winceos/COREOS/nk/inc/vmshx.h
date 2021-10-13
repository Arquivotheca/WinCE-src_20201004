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
    vmshx.h - machine dependant memory management structures
*/

#ifndef VM_SHx_H
#define VM_SHx_H

#include <vmlayout.h>

//////////////////////////////////////////////////////////////////////////
//
// A valid page entry is defined as one with the following bits in 
// PTEL register set:
// 
// PG_VALID_MASK, PG_GLOBAL_MSK, PG_DIRTY_MASK, PG_CACHE_MASK,
// PG_4K_MASK, and PG_PROT_WRITE
//
// That leaves with following bits to be used for page management:
// bits 10 and 11 as they are unused for 4k pages
// bit 1 as write-through flag is not used
//
// Those unused bits (for invalid page entries) are re-used as follows 
// (assuming: numbering start with bit 0 ... 31)
//
// entry[0]: to mark a page reserved (overloaded with write-through bit)
// entry[10] and entry[11]: used for pager type mask. Since there are 
// three possible pager types, we need two bits to define pager types.
// Also these same bits are used to denote a guard page or page with
// no access.
//
// Following bits in PTEL should never be used as they are used to set
// assistance register values: entry[9], entry[29] ... entry[31]
//////////////////////////////////////////////////////////////////////////


//
// The following bit definitions are taken from SH4 documentation for PTEL 
// register description.

// top three bits are reserved and bottom 12 bits are used for page 
// management; PFN for 4k pages is from entry[12] ... entry[28]
#define PG_PERMISSION_MASK      0x00000FFF
#define PG_EXECUTE_MASK         0x00000000 //unsued by HW
#define PG_WRITE_THROUGH        0x00000001
#define PG_GLOBAL_MASK          0x00000002 // set on shared page across all processes (for addr > 2G)
#define PG_DIRTY_MASK           0x00000004 // set on written entry
#define PG_CACHE                0x00000008 // page cacheable or not
#define PG_CACHE_MASK           0x00000008
#define PG_NOCACHE              0x00000000
#define PG_4K_MASK              0x00000010
#define PG_64K_MASK             0x00000080 // unused
#define PG_1M_MASK              0x00000090 // unused
#define PG_PROTECTION           0x00000060 // mask for page protection
#define PG_PROT_KREAD           0x00000000 // kernel read-only
#define PG_PROT_KWRITE          0x00000020 // kernel read/write
#define PG_PROT_READ            0x00000040 // user read-only
#define PG_PROT_WRITE           0x00000060 // user read/write
#define PG_VALID_MASK           0x00000100 // set on valid entry

#define PG_SIZE_MASK            PG_4K_MASK

#define PG_READ_WRITE           (PG_SIZE_MASK | PG_VALID_MASK | PG_DIRTY_MASK | PG_CACHE_MASK | PG_PROT_WRITE)
#define PG_KRN_READ_WRITE       PG_READ_WRITE

// UserKPage: user read-only, 4k, cachable, !dirty, shared, valid
#define USERKPAGE_PROTECTION    (PG_VALID_MASK|PG_GLOBAL_MASK|PG_CACHE|PG_4K_MASK|PG_PROT_READ)

// PG_CHANGEABLE defines all the bits that can be changed via VirtualSetAttributes
// only allow cahceable bit to be updated via that API.
#define PG_CHANGEABLE           (PG_CACHE_MASK)

// Page type bits
#define PG_PAGER_MASK           0xC00 // entry[10] and entry[11] are unused for 4k pages        
#define PG_PAGER_SHIFT          10
#define PG_AUTO_COMMIT          (VM_PAGER_AUTO   << PG_PAGER_SHIFT)  // the page is an auto-commit page
#define PG_LOADER               (VM_PAGER_LOADER << PG_PAGER_SHIFT)  // the page is paged by loader
#define PG_MAPPER               (VM_PAGER_MAPPER << PG_PAGER_SHIFT)  // the page is paged by memory mapped file

// following bits are valid only while PG_VALID is false
#define PG_RESERVED_PAGE        0x01 // overloaded with write-through bit entry[0]
#define PG_GUARD                0x00000400  // guard page -- overloaded with pager mask bits
#define PG_NOACCESS             0x00000800  // page with no access -- overloaded with pager mask bits

#define PFN_INCR                VM_PAGE_SIZE
#define PFN_SHIFT               0

#define VM_SECTION_SHIFT        22


// Find the page frame #
#define PHYS_MASK               0x1FFFF000          // phys page mask for address between 0x80000000-0xc0000000
#define NextPFN(pfn)            ((pfn) + PFN_INCR)
#define PFNfromEntry(entry)     ((entry) & ~PG_PERMISSION_MASK)
#define PFNfromSectionEntry(entry)  PFNfromEntry(entry)
#define PFNfrom256(x)           (((ulong)(x)<<8) & ~PG_PERMISSION_MASK)
#define PFN2PA(pfn)             ((pfn))
#define PA2PFN(pfn)             ((pfn))
#define PA256FromPfn(pfn)       ((pfn) >> 8)

#define ReadOnlyEntry(entry)    (((entry) & ~PG_PROTECTION)|PG_PROT_READ)

/* Test the write access on a page table entry */
#define IsPageWritable(entry)   (((entry)&(PG_PROT_WRITE|PG_VALID_MASK)) == (PG_PROT_WRITE|PG_VALID_MASK))
#define IsPageReadable(entry)   (((entry)&PG_VALID_MASK) == PG_VALID_MASK)
#define IsPageCommitted(entry)  ((entry&~PG_PAGER_MASK) > VM_PAGE_SIZE)
#define IsGuardPage(entry)      ((IsPageCommitted (entry)) && ((PG_GUARD & entry) == PG_GUARD))

// commit guard page
#define CommitGuardPage(entry)  (((entry) & ~PG_GUARD) | PG_VALID_MASK)


// reference to kpage from user page directory
#define KPAGE_PTE ((((ulong)g_pKData - 0x800) & PHYS_MASK) \
            + PG_4K_MASK + PG_VALID_MASK \
            + PG_GLOBAL_MASK + PG_CACHE_MASK + PG_PROT_READ)

#define VM_NUM_PD_ENTRIES       VM_NUM_PT_ENTRIES

// pdentry macros
#define VA2PDIDX(va)            ((DWORD) (va) >> VM_SECTION_SHIFT)
#define NextPDEntry(idxdir)     ((idxdir)+1)
#define PrevPDEntry(idxdir)     ((idxdir)-1)
#define GetPDEntry(ppdir, va)   ((ppdir)->pte[VA2PDIDX (va)])


// section macros
#define PG_SECTION_OFST_MSAK    0x003FF000  // middle 10 bits
#define SectionOfst(addr)       (((ulong) (addr) & PG_SECTION_OFST_MSAK))
#define IsSectionMapped(entry)  ((int) (entry) > 0)     // MSB not set, and not 0
#define IsPDEntryValid(entry)   ((entry) != 0)

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

#endif // VM_SHx_H

