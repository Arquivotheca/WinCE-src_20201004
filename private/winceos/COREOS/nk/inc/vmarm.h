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

#ifndef _VM_ARM_H
#define _VM_ARM_H

#include <oemglobal.h>

ulong GetCpuId(void);

/* Page permission bits */
#define PG_PERMISSION_MASK              0x00000FFF
#define PG_PHYS_ADDR_MASK               0xFFFFF000    
#define PG_VALID_MASK                   0x00000002
#define PG_CACHE_MASK                   0x0000000C
#define PG_CACHE                        (g_pOemGlobal->dwARMCacheMode | g_pKData->dwPTExtraBits)
#define PG_WRITE_THRU                   0x00000008  /* TEX-C-B = 000-1-0 */
#define PG_NOCACHE                      0x00000000
#define PG_DIRTY_MASK                   0x00000000  /* no specific dirty bits. use IsPageWriteable to see if a page is writable */
#define PG_EXECUTE_MASK                 0x00000000  /* not supported by HW */
#define PG_GLOBAL_MASK                  0x00000000  /* no specific global mask */

#define PG_V4_PROTECTION                0x00000FF0
#define PG_V4_PROT_READ                 0x00000000
#define PG_V4_PROT_WRITE                0x00000FF0
#define PG_V4_PROT_URO_KRW              0x00000AA0
#define PG_V4_PROT_UNO_KRW              0x00000550  // user no access, kernel read/write

#define PG_V6_PROTECTION                0x00000230
#define PG_V6_PROT_READ                 0x00000220
#define PG_V6_PROT_WRITE                0x00000030
#define PG_V6_PROT_URO_KRW              0x00000020
#define PG_V6_PROT_UNO_KRW              0x00000010  // user no access, kernel read/write
#define PG_V6_NOT_GLOBAL                0x00000800  // non-global page
#define PG_V6_SHRAED                    0x00000400
#define PG_V6_L2_NO_EXECUTE             0x00000001

#define PG_V6_L1_NO_EXECUTE             0x00000010  // bit to indicate non-executable (V6, section entry)
#define PG_V6_L1_SHARED                 0x00010000  // bit 16 - shared bit (v6, section entry)

#define PG_L1_KRW                       0x400       // bits 10, 11 = 0b01

#define PG_PROTECTION                   (g_pKData->dwProtMask)
#define PG_PROT_READ                    (g_pKData->dwRead)
#define PG_PROT_WRITE                   (g_pKData->dwWrite)
#define PG_PROT_URO_KRW                 (g_pKData->dwKrwUro)
#define PG_PROT_UNO_KRW                 (g_pKData->dwKrwUno)      // user no access, kernel read/write

#define PG_READ_WRITE                   (PG_VALID_MASK | PG_DIRTY_MASK | PG_CACHE | PG_PROT_WRITE)
#define PG_KRN_READ_WRITE               (PG_VALID_MASK | PG_DIRTY_MASK | PG_CACHE | PG_PROT_UNO_KRW)
#define PG_KRW_URO                      (PG_VALID_MASK | PG_DIRTY_MASK | PG_CACHE | PG_PROT_URO_KRW)


// 1st level page table defs
#define PD_TYPE_MASK                    0x3

#define PD_INVALID                      0x0
#define PD_COARSE_TABLE                 0x1
#define PD_SECTION                      0x2
#define PD_FINE_TABLE                   0x3         // not used, not supported in v6

#define PG_SECTION_PROTECTION           (PG_L1_KRW | PD_SECTION)

// bits in reserved pages (not committed), only valid when PG_VALID is NOT set
#define PG_NOACCESS                     0x00000800  // overlapped with NG bit in V6
#define PG_GUARD                        0x00000400

#define PG_PAGER_MASK                   0x30        
#define PG_PAGER_SHIFT                  4
#define PG_RESERVED_PAGE                0x40        // the page is reserved only

#define PG_AUTO_COMMIT                  (VM_PAGER_AUTO   << PG_PAGER_SHIFT)  // the page is an auto-commit page
#define PG_LOADER                       (VM_PAGER_LOADER << PG_PAGER_SHIFT)  // the page is paged by loader
#define PG_MAPPER                       (VM_PAGER_MAPPER << PG_PAGER_SHIFT)  // the page is paged by memory mapped file

#define VM_SECTION_SHIFT                20


// PG_CHANGEABLE defines all the bits that can be changed via VirtualSetAttributes
// any other bits are reserved by kernel and cannot be changed.
//
// For ARM, (XScale, more specifically), because of the new Page attributes it introduce,
// we're going to give them all the bits they can set and hope for the best. 
#define PG_CHANGEABLE                   PG_PERMISSION_MASK

/* Test the write access on a page table entry */
#define IsPageReadable(entry)           ((entry) & PG_VALID_MASK)
#define IsPageWritable(entry)           (IsPageReadable (entry) && (((entry) & PG_PROTECTION) != PG_PROT_READ))
#define IsPageCommitted(entry)          ((DWORD) (entry) > VM_PAGE_SIZE)

#define IsV6PageExecutable(entry)       (!((entry) & PG_V6_L2_NO_EXECUTE))

#define PFN_INCR                        VM_PAGE_SIZE
#define PFN_SHIFT                       0
#define NextPFN(pfn)                    ((pfn) + PFN_INCR)
#define PFNfromEntry(entry)             ((entry) & PG_PHYS_ADDR_MASK)
#define PFNfromSectionEntry(entry)      ((entry) & 0xFFF00000)
#define PFNfrom256(pg256)               (((ulong)(pg256)<<8) & ~VM_PAGE_OFST_MASK)
#define PFN2PA(pfn)                     (pfn)
#define PA2PFN(pfn)                     (pfn)
#define PA256FromPfn(pfn)               ((pfn) >> 8)
#define ReadOnlyEntry(entry)            (((entry) & ~PG_PROTECTION)|PG_PROT_READ)

#define PG_SECTION_OFST_MSAK            0x000FF000  // middle 8 bits
#define SectionOfst(addr)               ((ulong) (addr) & PG_SECTION_OFST_MSAK)

#define GetPDEntry(ppdir, va)           ((ppdir)->pte[(DWORD) (va) >> VM_SECTION_SHIFT])

#define KPAGE_PTE   0   // Not used on ARM

#define VM_NUM_PD_ENTRIES               VM_PAGE_SIZE        // ((4*VM_PAGE_SIZE)/sizeof(DWORD))

#define VA2PDIDX(va)                    ((((DWORD) (va)) >> 22) << 2)
#define IsSectionMapped(entry)          (((entry) & PD_TYPE_MASK) == PD_SECTION)

#define IsGlobalPageSupported()         TRUE

__inline BOOL IsPDEntryValid(DWORD dwPDEntry)
{
    return ((dwPDEntry & PD_TYPE_MASK) == PD_COARSE_TABLE) || ((dwPDEntry & PD_TYPE_MASK) == PD_SECTION);
}

#define NextPDEntry(idxdir)             ((idxdir)+4)
#define PrevPDEntry(idxdir)             ((idxdir)-4)

#define IsV6OrLater()                   (g_pKData->dwArchitectureId >= ARMArchitectureV6)

//
//   ARM Processor Family                   Architecture[19:16]              Part Number [15:4]
// ------------------------------          ----------------------           ----------------------
//    ARM1136     (v6)                             0x7                               0xB36
//    ARM1156     (v6)                             0xF                               0xB56  
//    ARM1176     (v6)                             0xF                               0xB76
//    ARM11MP     (v6)                             0xF                               0xB02
//    Cortex M0   (v6)                             0xC                               0xC20
//    Cortex M1   (v6)                             0xC                               0xC21
//    Cortex M3   (v7-M)                           0xF                               0xC23
//    Cortex R4   (v7-R)                           0xF                               0xC14
//    Cortex A5   (v7)                             0xF                               0xC05
//    Cortex A8   (v7)                             0xF                               0xC08
//    Cortex A9   (v7)                             0xF                               0xC09
//
__inline BOOL IsV7()
{
    //Only Cortex Ax are ARMv7
    DWORD dwPrimaryPartNumber = (GetCpuId() >> 4) & 0xfff;
    return((g_pKData->dwArchitectureId ==0xF)  && (dwPrimaryPartNumber >= 0xC00)  && (dwPrimaryPartNumber <= 0xC09));
}

// Assume all of non-SMP ARMv6 and later does not support Flush with Uncached VA
#define IsFlushUncachedVASupported()    (!IsV6OrLater() || (g_pKData->nCpus > 1))

//
// setup Page directory entry
//

__inline DWORD GetPagerType (DWORD dwEntry)
{
    return (dwEntry & PG_PAGER_MASK) >> PG_PAGER_SHIFT;
}

//
// MakeReservedEntry: create a page table entry for reserved page
//
__inline DWORD MakeReservedEntry (DWORD dwPagerType)
{
    return ((VM_PAGER_NONE == dwPagerType)? PG_RESERVED_PAGE : (dwPagerType << PG_PAGER_SHIFT));
}


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
DWORD MakeWritableEntry (PPROCESS pprc, DWORD dwPFN, DWORD dwAddr);

//
// IsGuardPage: test if a page is a guard page
//
__inline BOOL  IsGuardPage (DWORD dwEntry) {
    return IsPageCommitted (dwEntry)            // a committed page
        && !(PG_VALID_MASK & dwEntry)           // valid bit not set
        && ((PG_GUARD & dwEntry) == PG_GUARD);  // guard page bits set
}

//
// commit guard page
//
DWORD CommitGuardPage (DWORD dwEntry);

void V6_WriteBarrier (void);

//
// ASID related function
//
// max set to 0xfe. 0xff is not valid, used to deal with prefetch and branch prediction during ASID/TTBR update.
#define INVALID_ASID    0xff
#define MAX_ASID        0xfe
DWORD MDAllocateASID (void);
DWORD MDGetKernelASID (void);
void MDFreeASID (DWORD dwASID);

// no action required on active process change
#define MDSwitchActive(x)

// TTBR related functions
DWORD GetTranslationBase (void);
void SetupTTBR1 (DWORD dwTTbr);
void UpdateAsidAndTranslationBase (DWORD asid, DWORD dwTTbr);

extern PCDeviceTableEntry g_pOEMDeviceTable;
extern PADDRMAP g_pOEMAddressTable;
void ARMPteUpdateBarrier (LPDWORD pdwEntry, DWORD cEntries);


#endif // _VM_ARM_H

