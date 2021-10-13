//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*
    mdmem.h - machine dependant memory management structures
*/

#ifndef MEM_x86_H
#define MEM_x86_H

//  The user mode virtual address space is 2GB split into 64 sections
// of 512 blocks of 16 4K pages. For some platforms, the # of blocks in
// a section may be limited to fewer than 512 blocks to reduce the size
// of the intermeditate tables. E.G.: Since the PeRP only has 5MB of total
// RAM, the sections are limited to 4MB. This results in 64 block sections.
//
// Virtual address format:
//  3322222 222221111 1111 110000000000
//  1098765 432109876 5432 109876543210
//  zSSSSSS BBBBBBBBB PPPP oooooooooooo

#define PAGE_MASK       0x00F
#define BLOCK_MASK      0x1FF
#define SECTION_MASK    0x03F
#define RESERVED_SECTIONS 1     // reserve section 0 for current process

// Bit offsets of page, block & section in a virtual address:
#define VA_PAGE         12
#define VA_BLOCK        16
#define VA_SECTION      25

#define PAGE_SIZE 4096      /* page size */

#define PAGES_PER_BLOCK  (0x10000 / PAGE_SIZE)

// # of pages needed for Page Table per process
#define HARDWARE_PT_PER_PROC   8

//
// A Page Table Entry on an Intel x86 has the following definition.
//

typedef struct _HARDWARE_PTE {
    ULONG Valid : 1;
    ULONG Write : 1;
    ULONG Owner : 1;
    ULONG WriteThrough : 1;
    ULONG CacheDisable : 1;
    ULONG Accessed : 1;
    ULONG Dirty : 1;
    ULONG LargePage : 1;
    ULONG Global : 1;
    ULONG ExecuteOnly : 1;  // software field
    ULONG reserved1 : 1;    // software field
    ULONG reserved2 : 1;    // software field
    ULONG PageFrameNumber : 20;
} HARDWARE_PTE, *PHARDWARE_PTE;


/* Page permission bits */
#define PG_PERMISSION_MASK     0x00000FFF
#define PG_PHYS_ADDR_MASK      0xFFFFF000    
#define PG_VALID_MASK          0x00000001
#define PG_WRITE_MASK          0x00000002
#define PG_USER_MASK           0x00000004
#define PG_WRITE_THRU_MASK     0x00000008
#define PG_CACHE_MASK          0x00000010
#define PG_NOCACHE             0x00000010
#define PG_CACHE               0
#define PG_ACCESSED_MASK       0x00000020
#define PG_DIRTY_MASK          0x00000040
#define PG_LARGE_PAGE_MASK     0x00000080  // Set on page directory entries only
#define PG_PAT_MASK            0x00000080  // Set on page table entries only
#define PG_GLOBAL_MASK         0x00000100
#define PG_EXECUTE_MASK        0x00000200  // Unused by hardware    
#define PG_GUARD               0x00000400  // Software field used to return non zero from MakePagePerms
#define PG_RESERVED2           0x00000800  // unused.
#define PG_SIZE_MASK           0

#define PG_PROT_READ           PG_USER_MASK
#define PG_PROT_WRITE          (PG_USER_MASK | PG_WRITE_MASK)
#define PG_PROTECTION          (PG_USER_MASK | PG_WRITE_MASK)

#define PG_READ_WRITE          (PG_VALID_MASK | PG_DIRTY_MASK | PG_CACHE | PG_PROT_WRITE)

#define PG_KRN_READ_WRITE      (PG_VALID_MASK | PG_DIRTY_MASK | PG_CACHE | PG_WRITE_MASK)

// PG_CHANGEABLE defines all the bits that can be changed via VirtualSetAttributes
// any other bits are reserved by kernel and cannot be changed.
#define PG_CHANGEABLE          (PG_CACHE_MASK | PG_WRITE_THRU_MASK | PG_RESERVED2 | PG_PAT_MASK)

typedef struct _PAGETABLE {
    DWORD   PTE[1024];
} PAGETABLE, * PPAGETABLE;

//
// !!! NOTE: PHYS_TO_LIN and LIN_TO_PHYS MUST only be used when the address is in RAM !!!
//
extern DWORD g_dwRAMOfstVA2PA;
#define PHYS_TO_LIN(x) ((DWORD)(x) - g_dwRAMOfstVA2PA)
#define LIN_TO_PHYS(x) ((DWORD)(x) + g_dwRAMOfstVA2PA)

extern char KDBase[];
#define KPAGE_PTE (LIN_TO_PHYS(KDBase) + PG_VALID_MASK + PG_CACHE_MASK + PG_USER_MASK)

#define INVALID_POINTER_VALUE   0xC0000000

#define UNKNOWN_BASE    (~0)
#define BAD_PAGE        (~1ul)

#ifndef VERIFY_READ_FLAG
#define VERIFY_READ_FLAG    0
#define VERIFY_EXECUTE_FLAG 0
#define VERIFY_WRITE_FLAG   1
#define VERIFY_KERNEL_OK    2
#endif

#ifndef ASM_ONLY

#define SWITCHKEY(oldval, newval) ((oldval) = (pCurThread)->aky, (pCurThread)->aky = (newval))
#define GETCURKEY() ((pCurThread)->aky)
#define SETCURKEY(newval) ((pCurThread)->aky = (newval))

#define PFN_INCR  PAGE_SIZE
/* Find the page frame # from a GPINFO pointer */
#define GetPFN(pgpi)  ((ulong)(pgpi) & 0x7FFFF000)
#define NextPFN(pfn)  ((pfn) + PFN_INCR)
#define PFNfromEntry(entry)   ((entry) & 0xFFFFF000)
#define PFNfrom256(pg256)   ((ulong)(pg256)<<8 & ~(PAGE_SIZE-1))
#define PFN2PA(pfn)   (pfn)
#define PA2PFN(pfn)   (pfn)

/* Return virtual address from page frame number */
extern LPVOID Phys2Virt (DWORD pfn);
#define Phys2VirtUC(pfn)   ((LPVOID) ((DWORD) Phys2Virt (pfn) | 0x20000000))

/* Test the write access on a page table entry */
#define IsPageWritable(entry)   (((entry)&PG_WRITE_MASK) != 0)
#define IsPageReadable(entry)   (((entry)&PG_VALID_MASK) == PG_VALID_MASK)

#endif // not ASM_ONLY

#endif // MEM_x86_H

