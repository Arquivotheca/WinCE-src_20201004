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

#ifndef MEM_MIPS_H
#define MEM_MIPS_H

#if !R4000
    #error CPU not supported
#endif

#define PAGE_SIZE 4096      /* page size */

#define HARDWARE_PT_PER_PROC   0   /* no hardware page tables */
#define SECURE_SECTION      0x61    // VM at 0xC2XXXXXX

//++
// These symbols are duplicated from kernel.h so they can
// be used by the MIPS assembler code without including kernel.h

// Bit offsets of block & section in a virtual address:
#define VA_BLOCK        16
#define VA_SECTION      25
#define VA_PAGE         12

#define PAGE_MASK       0x00F
#define BLOCK_MASK      0x1FF
#define SECTION_MASK    0x03F

#define VERIFY_READ_FLAG    0
#define VERIFY_EXECUTE_FLAG 0
#define VERIFY_WRITE_FLAG   1
#define VERIFY_KERNEL_OK    2
//-- End of duplicated symbols

#define ASID_MASK 0xFF

/* Page permission bits */
#define PG_PERMISSION_MASK  0xC000003F
#define PG_GUARD            0x80000000  /* unused by HW */
#define PG_EXECUTE_MASK     0x40000000  /* unused by HW */
#define PG_GLOBAL_MASK      0x00000001
#define PG_VALID_MASK       0x00000002
#define PG_DIRTY_MASK       0x00000004
#define PG_CACHE_MASK       0x00000038  /* should be ANDed to find out cache state */
#define PG_NOCACHE          0x00000010
#define PG_CACHE            0x00000018
#define PG_PROT_READ        0x00000000
#define PG_PROT_WRITE       PG_DIRTY_MASK
#define PG_SIZE_MASK        0x00000000
#define PG_PROTECTION       (PG_PROT_READ | PG_PROT_WRITE) 
#define PG_READ_WRITE       (PG_DIRTY_MASK | PG_VALID_MASK | PG_CACHE)
#define PG_KRN_READ_WRITE   PG_READ_WRITE

// PG_CHANGEABLE defines all the bits that can be changed via VirtualSetAttributes
// any other bits are reserved by kernel and cannot be changed.
#define PG_CHANGEABLE       PG_CACHE_MASK

// MemBlock structure layout
#define mb_lock     0
#define mb_uses     4
#define mb_flags    5
#define mb_ixBase   6
#define mb_hPf      8
#define mb_pages    12

#define INVALID_POINTER_VALUE      0xC0000000
#define NK_IO_BASE      (SECURE_VMBASE + 0x02000000)
#define NK_IO_SIZE      0x20000000 - 0x02000000  // 512 MB - secure slot


#define BAD_PAGE            (~0x3Ful)
#define PFN_SHIFT_R41XX     4                   // R41XX supports tiny page and has a different PFN_SHIFT
#define PFN_SHIFT_MIPS32    6                   // standard for MIPS32/MIPS64

#define PFN_SHIFT           (KData.dwPfnShift)
#define PFN_INCR            (KData.dwPfnIncr)

#define SWITCHKEY(oldval, newval) ((oldval) = (pCurThread)->aky, CurAKey = (pCurThread)->aky = (newval))
#define GETCURKEY() ((pCurThread)->aky)
#define SETCURKEY(newval) (CurAKey = (pCurThread)->aky = (newval))

/* Query & set thread's kernel vs. user mode state */
#define GetThreadMode(pth) ((pth)->ctx.Psr & MODE_MASK)
#define SetThreadMode(pth, mode) ((pth)->ctx.Psr = (mode))

/* Query & set kernel vs. user mode state via Context */
#define GetContextMode(pctx) ((pctx)->Psr & MODE_MASK)
#define SetContextMode(pctx, mode) ((pctx)->Psr = (mode))

#define PFN_MASK (~(PAGE_SIZE-1) & 0x1FFFFFFF)
/* Find the page frame # from a GPINFO pointer */
#define GetPFN(pgpi)  (((ulong)(pgpi) & PFN_MASK) >> PFN_SHIFT)
#define NextPFN(pfn)  ((pfn) + PFN_INCR)
#define PFNfromEntry(entry)   ((entry) & 0x3FFFFFC0)
#define PFNfrom256(x) ((ulong)(x)<<(8-PFN_SHIFT) & 0x3FFFFFC0)
#define PFN2PA(pfn)   ((pfn) << PFN_SHIFT)
#define PA2PFN(pfn)   ((pfn) >> PFN_SHIFT)

#define KERNEL_MODE 0x03
#define USER_MODE 0x13
#define MODE_MASK 0x1F

/* Return virtual address from page frame number */
#define Phys2Virt(pfn)  ((PVOID)(((pfn)<< PFN_SHIFT) | 0x80000000))
#define Phys2VirtUC(pfn)  ((PVOID)(((pfn)<< PFN_SHIFT) | 0xA0000000))

/* Test the write access on a page table entry */
#define IsPageWritable(entry)   (((entry)&PG_DIRTY_MASK) != 0)
#define IsPageReadable(entry)   (((entry)&PG_VALID_MASK) == PG_VALID_MASK)

#define KPAGE_PTE 0

#endif // MEM_MIPS_H

