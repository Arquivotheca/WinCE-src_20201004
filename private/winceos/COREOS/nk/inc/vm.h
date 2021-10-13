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
//    vm.h - internal kernel VM header
//


#ifndef _NK_VM_H_
#define _NK_VM_H_

#include <vmlayout.h>
#include "physmem.h"

//#ifndef SHIP_BUILD
#ifdef  DEBUG
#define VM_DEBUG                1
#endif

#define KSEG0_BASE 0x80000000           // base of cached kernel physical
#define KSEG1_BASE 0xa0000000           // base of uncached kernel physical


//------------------------------------------------------------------------
//
// other constants
//
#define VM_NUM_PT_ENTRIES       (VM_PAGE_SIZE/sizeof(PTENTRY))  // # of pages in a page table
#define VM_PAGES_PER_BLOCK      (VM_BLOCK_SIZE/VM_PAGE_SIZE)    // # of pages in a block

#define VM_FREE_PAGE            0                   // MUST be 0

// lock flags
#define VM_LF_WRITE             0x0001
#define VM_LF_QUERY_ONLY        0x0002
#define VM_LF_READ              0x0004

// pager type
#define VM_PAGER_NONE           0                   // no pager
#define VM_PAGER_LOADER         1                   // loader paged
#define VM_PAGER_MAPPER         2                   // mapper paged
#define VM_PAGER_AUTO           3                   // auto commit

#define VM_MAX_PAGER            VM_PAGER_AUTO
#define VM_PAGER_POOL           0x80000000          // VMDecommit flag: part of a page pool (never stored in PT entry)


#define VM_LAST_USER_PDIDX      VA2PDIDX(VM_SHARED_HEAP_BASE)

#define VM_SECTION_SIZE         (1 << VM_SECTION_SHIFT)
#define VM_SECTION_OFST_MASK    (VM_SECTION_SIZE - 1)

#define SECTIONALIGN_UP(x)      (((x) + VM_SECTION_OFST_MASK) & ~VM_SECTION_OFST_MASK)
#define SECTIONALIGN_DOWN(x)    ((x) & ~VM_SECTION_OFST_MASK)


//-----------------------------------------------------------------------------------------------------------------
//
// VM API parameter grouping
//
// valid allocation type
#define VM_VALID_ALLOC_TYPE     (MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN | MEM_IMAGE | MEM_AUTO_COMMIT | MEM_MAPPED)
// allocation types that applications cannot use, valid only inside kernel
#define VM_INTERNAL_ALLOC_TYPE  (MEM_MAPPED)


#if defined(x86) || (defined(ARM) && !defined(ARMV5) && !defined(ARMV4))
// for x86, arm v6 and its above processors
// valid protection
#define VM_VALID_PROT           (PAGE_NOACCESS | PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE | PAGE_EXECUTE_READ \
                                 | PAGE_EXECUTE_READWRITE | PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITE_THRU)
#else
// valid protection
#define VM_VALID_PROT           (PAGE_NOACCESS | PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE | PAGE_EXECUTE_READ \
                                 | PAGE_EXECUTE_READWRITE | PAGE_GUARD | PAGE_NOCACHE)
#endif

// all read-only protections
#define VM_READONLY_PROT        (PAGE_READONLY | PAGE_EXECUTE | PAGE_EXECUTE_READ)

// all read-write protections
#define VM_READWRITE_PROT       (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)

// all executable protections
#define VM_EXECUTABLE_PROT      (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)

// index of user k-page
#define VM_KPAGE_IDX            5

// constant for 4M alignment
#define SIZE_4M                 (1 << 22)
#define MASK_4M                 (SIZE_4M - 1)
#define ALIGNUP_4M(x)           (((x) + MASK_4M) & ~ MASK_4M)
#define ALIGNDOWN_4M(x)         ((x) & ~ MASK_4M)

//------------------------------------------------------------------------
//
// VM types
//
typedef DWORD   PTENTRY, *PPTENTRY;         // page table entry is 32 bits

// 2nd-level page table
typedef struct _PAGETABLE {
    PTENTRY     pte[VM_NUM_PT_ENTRIES];
} PAGETABLE, *PPAGETABLE;

struct _VALIST {
    PVALIST     pNext;          // next pointer
    LPVOID      pVaBase;        // VM allocation base
    DWORD       cbVaSize;       // size of allocation size
    PALIAS      pAlias;         // point to alias if exists
    DWORD       dwTag;          // tag associated with this allocation
#ifdef VM_DEBUG
#define VM_MAX_CALLSTACK 19
    DWORD       callstack[VM_MAX_CALLSTACK];  // to keep the callstack of the allocation
#endif
};

struct _STKLIST {
    PSTKLIST    pNextStk;       // next pointer
    LPVOID      pStkBase;       // stack base
    DWORD       cbStkSize;      // stack size
};

struct _ALIAS {
    DWORD       dwProcessId;    // which process
    DWORD       dwAliasBase;     // alias base address
    DWORD       cbSize;
};

#ifdef VM_DEBUG
#define HEAP_VALIST     HEAP_EVENT
#else
#define HEAP_VALIST     HEAP_APISET // TODO: change this to HEAP_STKLIST when pAlias field is removed from VALIST
#endif

//
// the following 2 functions are used in vmxxx.h, need to define them before 
// include the header files

// convert a physical page number to the statically mapped kernel address
LPVOID Pfn2Virt (DWORD dwPfn);

#if defined(x86)
#include "vmx86.h"
#elif defined(ARM)
#include "vmarm.h"
#elif defined (MIPS)
#include "vmmips.h"
#elif defined (SHx)
#include "vmshx.h"
#endif

// page directory defined after including machine dependent part
typedef struct _PPAGEDIRECTORY {
    PTENTRY     pte[VM_NUM_PD_ENTRIES];
} PAGEDIRECTORY, *PPAGEDIRECTORY;


//------------------------------------------------------------------------
//
// useful macros
//

// find 2nd level page table index from va
#define VA2PT2ND(va)            (((DWORD) (va) >> 12) & 0x3ff)
#define IsPageAligned(va)       (!((DWORD) (va) & VM_PAGE_OFST_MASK))
#define IsBlockAligned(va)      (!((DWORD) (va) & VM_BLOCK_OFST_MASK))
#define IsSectionAligned(va)    (!((DWORD) (va) & VM_SECTION_OFST_MASK))

//------------------------------------------------------------------------
//
// Function protytypes
//

// convert a statically mapped kernel address to a physical page number
DWORD  GetPFN (LPCVOID pvAddr);

// get the MEM_XXX bits from a committed page table entry
DWORD ProtectFromEntry (DWORD dwEntry);

// get the page protection bits (PG_XXX) from MEM_XXX bits
DWORD PageParamFormProtect (PPROCESS pprc, DWORD fProtect, DWORD dwAddr);

// get the 2nd level page table given a page directory and index
PPAGETABLE GetPageTable (PPAGEDIRECTORY ppdir, DWORD idx);

// find static mapped device address
LPVOID FindDeviceMapping (DWORD pa256, DWORD cbSize);

// find static mapped address by NKCreateStaticMapping
LPVOID FindStaticMapping (DWORD dwPhysBase, DWORD dwSize);

// validate if (dwBase -> dwBase + dwSize) is a valid user stack
_inline BOOL IsValidUserStack (DWORD dwBase, DWORD dwSize)
{
    return !(dwBase & VM_BLOCK_OFST_MASK)
        && !(dwSize & VM_BLOCK_OFST_MASK)
        && ((int) dwSize > 0)
        && IsValidUsrPtr ((LPCVOID) dwBase, dwSize, TRUE);
}

//-----------------------------------------------------------------------------------------------------------------
//
//  IsValidProtect: check if protection is valid
//
//  NOTE: the bottom 8 bit of the protection is mutually exclusive. i.e. there
//        must be exactly one bit set in the bottom 8 bits
//
#if (defined(ARM) && !defined(ARMV5) && !defined(ARMV4))
		// for arm v6 and its above processors

__inline BOOL IsValidProtect (DWORD fProtect)
{
    DWORD dwExclusive = fProtect & 0xff;
    
    return dwExclusive
            && !(dwExclusive & (dwExclusive - 1))
            // PAGE_NOACCESS is not valid with other flag set
            && (!(fProtect & PAGE_NOACCESS) || (PAGE_NOACCESS == fProtect))
            && !(fProtect & ~(VM_VALID_PROT))
            // If WRITE_THRU is set, must have PAGE_READWRITE or PAGE_EXECUTE_READWRITE set
            && (!(fProtect & PAGE_WRITE_THRU) || (fProtect & (PAGE_READWRITE|PAGE_EXECUTE_READWRITE)));
}

#else

__inline BOOL IsValidProtect (DWORD fProtect)
{
    DWORD dwExclusive = fProtect & 0xff;
    
    return dwExclusive
            && !(dwExclusive & (dwExclusive - 1))
            // PAGE_NOACCESS is not valid with other flag set
            && (!(fProtect & PAGE_NOACCESS) || (PAGE_NOACCESS == fProtect))
#if defined(x86)
        && !(fProtect & ~(VM_VALID_PROT|PAGE_WRITE_THRU));
#else
        && !(fProtect & ~VM_VALID_PROT);
#endif
}

#endif


// AllocatePTBL - allocate a new page for pagetable
PPAGETABLE AllocatePTBL (PPAGEDIRECTORY ppdir, DWORD idxPD);

#ifdef ARM
void FreePageTable (PPAGETABLE pptbl);
#else
#define FreePageTable(pptbl)        FreePhysPage (GetPFN (pptbl))
#endif

// AllocatePD - allocate a new page directory
PPAGEDIRECTORY AllocatePD (void);

// free page directory
void FreePD (PPAGEDIRECTORY pd);

// translate a virtual address to kernel static mapped address
LPVOID GetKAddr (LPCVOID pAddr);
LPVOID GetKAddrOfProcess (PPROCESS pprc, LPCVOID pAddr);
DWORD GetPFNOfProcess(PPROCESS, LPCVOID);

BOOL VMFindAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, PDWORD pdwItemTag);
BOOL VMAddAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, PALIAS pAlias, PDWORD pdwTag);
void VMFreeAllocList (PPROCESS pprc);
BOOL VMRemoveAlloc (PPROCESS pprc, LPVOID pAddr, DWORD dwTag);


void ZeroPage(void *pvPage);

#ifdef IN_KERNEL
// 
// IsKernelVa - check if an address is in kernel static mapping area or kernel heap
__inline BOOL IsKernelVa (LPCVOID pAddr)
{
    DWORD dwAddr = (DWORD) pAddr;
#if defined(MIPS) || defined(SH4)
    return (DWORD) (dwAddr - KSEG0_BASE) < 0x40000000;
#else
    return (dwAddr >= VM_KMODE_BASE)
        && (dwAddr < g_pKData->dwKVMStart);
#endif
}

// IsInKVM - check if an address is in Kernel VM area
__inline BOOL IsInKVM (DWORD dwAddr)
{
    return (dwAddr >= g_pKData->dwKVMStart)
        && (dwAddr < VM_CPU_SPECIFIC_BASE);
}

#endif
// page fault handling functions

//-----------------------------------------------------------------------------------------------------------------
//
// KC_VMDemandCommit: Demand commit a stack page for exception handler.
// NOTE: This function is called within KCall.
//
DWORD KC_VMDemandCommit (DWORD dwAddr);

//
// return values for DemandCommit
//
#define DCMT_FAILED       0       // demand commit failed
#define DCMT_NEW          1       // committed a new page
#define DCMT_OLD          2       // page already commited


//-----------------------------------------------------------------------------------------------------------------
//
// VMProcessPageFault: Page fault handler. The handle to the process had been locked.
//
BOOL VMProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite, DWORD dwPc);

// KernelVMInit: initialize kernel VM
BOOL KernelVMInit (void);

// VMInit: Initialize per-process VM (called at process creation) 
PPAGEDIRECTORY VMInit (PPROCESS pprc);

// VMDelete: Delete and free all VM, and free the pagetables
void VMDelete (PPROCESS pprc);

// locking/unlocking process pages
BOOL VMAddToLockPageList     (PPROCESS pprc, DWORD dwAddr, DWORD cPages);
BOOL VMRemoveFromLockPageList   (PPROCESS pprc, DWORD dwAddr, DWORD cPages);
BOOL VMIsPagesLocked (PPROCESS pprc, DWORD dwAddr, DWORD cPages);

// InvalidatePages - invalidate TLB for a range of pages
void InvalidatePages (PPROCESS pprc, DWORD dwAddr, DWORD cPages);

void MDSwitchVM (PPROCESS pprc);
void MDSetCPUASID (void);

BOOL
VMCopyPhysical(
    PPROCESS pprc,              // the destination process
    DWORD dwAddr,               // destination address
    DWORD dwPfn,                // physical page number
    DWORD cPages,               // # of pages
    DWORD dwPgProt,             // protection
    BOOL  fIncRef               // whether to increment ref-count or not
    );

//-----------------------------------------------------------------------------------------------------------------
//
// VMReadProcessMemory: read process memory. 
//      return 0 if succeed, error code if failed
//
DWORD VMReadProcessMemory (PPROCESS pprc, LPCVOID pAddr, LPVOID pBuf, DWORD cbSize);

//-----------------------------------------------------------------------------------------------------------------
//
// VMWriteProcessMemory: write process memory. 
//      return 0 if succeed, error code if failed
//
DWORD VMWriteProcessMemory (PPROCESS pprc, LPVOID pAddr, LPCVOID pBuf, DWORD cbSize);

//-----------------------------------------------------------------------------------------------------------------
//
// VMDecommit: function to decommit VM, (internal only)
//
BOOL VMDecommit (
    PPROCESS pprc,
    LPVOID pAddr,
    DWORD cbSize,
    DWORD dwPagerType);


//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeAndRelease - decommit and release VM, (internal only)
//
BOOL
VMFreeAndRelease (
    PPROCESS pprc,
    LPVOID lpvAddr,
    DWORD cbSize);

//-----------------------------------------------------------------------------------------------------------------
//
// VMAlloc: main function to allocate VM. 
//
LPVOID 
VMAlloc (
    PPROCESS pprc,      // process
    LPVOID lpvaddr,     // starting address
    DWORD  cbSize,      // size, in byte, of the allocation
    DWORD  fAllocType,  // allocation type
    DWORD  fProtect     // protection
    );
 
 //-----------------------------------------------------------------------------------------------------------------
 //
 // VMCommit: commit memory (internal only)
 //
 LPVOID 
 VMCommit (
     PPROCESS pprc,          // process
     LPVOID lpvaddr,         // starting address
     DWORD  cbSize,          // size, in byte, of the allocation
     DWORD  fProtect,        // protection
     DWORD  dwPageType       // PM_PT_XXX
     );

//-----------------------------------------------------------------------------------------------------------------
//
// VMCopy: main function to VirtualCopy VM, kernel only, not exposed to user mode apps
//
BOOL 
VMCopy (
    PPROCESS pprcDst,       // destination process
    DWORD dwDestAddr,       // destination address
    PPROCESS pprcSrc,       // the source process
    DWORD dwSrcAddr,        // source address, NULL if PAGE_PHYSICAL
    DWORD cbSize,           // size, in bytes
    DWORD fProtect          // protection
    );

//-----------------------------------------------------------------------------------------------------------------
//
// VMMapUserAddrToVoid: remap user mode address to "void".
//
BOOL
VMMapUserAddrToVoid (
    PPROCESS pprc,              // process
    LPVOID lpvaddr,             // starting address
    DWORD  cbSize               // size, in byte, of the address to be remapped
    );

//-----------------------------------------------------------------------------------------------------------------
//
// VMFastCopy: (internal only) duplicate VM mapping from one to the other. 
// NOTE: (1) This is a fast function, no scanning, validation is performed. 
//       (2) Source and destination must both be Virtual addresses and must be page aligned.
//       (3) source and destination must have the same block offset, unless source is static-mapped kernel
//           address.
//       (4) Caller must be verifying all parameters before calling this.
//
BOOL VMFastCopy (
    PPROCESS pprcDst,       // the destination process
    DWORD dwDstAddr,        // destination address
    PPROCESS pprcSrc,       // the source process, NULL if PAGE_PHYSICAL or same as destination
    DWORD dwSrcAddr,        // source address
    DWORD cbSize,           // size
    DWORD fProtect
    );

//-----------------------------------------------------------------------------------------------------------------
//
// VMCopyCommittedPages: (internal only) duplicate VM mapping from kernel to a process at same address.
//      used primarily for mapping DLL addresses.
//
BOOL VMCopyCommittedPages (
    PPROCESS pprc,          // the destination process
    DWORD    dwAddr,        // destination address
    DWORD    cbSize,        // size 
    DWORD    fProtect
    );

//-----------------------------------------------------------------------------------------------------------------
//
// VMMove: (interanl) move the mapping from one VA to the other.
//          (1) SOURCE ADDRESS MUST BE A KERNEL MODE ADDRESS.
//          (2) Cache operation is performed based on the protection (fProtect) requested.
//
BOOL
VMMove (
    PPROCESS pprcDst,               // destination process
    DWORD  dwDstAddr,               // destination address
    DWORD  dwSrcAddr,               // source address (must be a kernel, non-static-mapped address)
    DWORD  cbSize,                  // size to move
    DWORD  fProtect                 // destination protection
);

//-----------------------------------------------------------------------------------------------------------------
//
// VMCreateKernelPageMapping: create mapping of a kernel page.
//         SOURCE ADDRESS MUST BE A KERNEL MODE ADDRESS.
// NOTE: physical page maintance of the mapping MUST BEN HANDLED EXPLICITLY IN THE CALLER FUNCTION.
//
BOOL VMCreateKernelPageMapping (LPVOID pAddr, DWORD dwPFN);

//-----------------------------------------------------------------------------------------------------------------
//
// VMRemoveKernelPageMapping: (interanl) remove the mapping of a kernel page.
//         SOURCE ADDRESS MUST BE A KERNEL MODE ADDRESS.
// NOTE: physical page maintance of the mapping MUST BEN HANDLED EXPLICITLY IN THE CALLER FUNCTION.
//
DWORD VMRemoveKernelPageMapping (LPVOID pPage);


#ifdef ARM
//
// VMAlias: ARM specific (internal only)
//
//      keep track of aliasing of addresses, need to turn cache off whenever alias is created
//
BOOL VMAlias (
    PALIAS   pAlias,        // ptr to alias structure
    PPROCESS pprc,          // process
    DWORD    dwAddr,        // start address to be uncached. MUST BE PAGE ALIGNED
    DWORD    cbSize,        // size of the block to be uncached.
    DWORD    fProtect
    );

//
// VMUnlias: ARM specific (internal only)
//
//      Called when alias is removed. try to turn cache on if alias no longer exist
//
BOOL VMUnalias (
    PALIAS   pAlias         // ptr to alais structure
    );

//
// VMAddUncachedAllocation: ARM specific (internal only)
//
//      Called when VirtualAlloc is called with PAGE_NOCACHE. We need to keep track of it
//      for VIVT, such that when VMUnalias is called, we don't turn the cache back on on
//      these pages.
//
void VMAddUncachedAllocation (PPROCESS pprc, DWORD dwAddr, PVALIST pUncached);

#endif

//-----------------------------------------------------------------------------------------------------------------
//
// VMQuery: function to Query VM
//
DWORD
VMQuery (
    PPROCESS pprc,                      // process
    LPCVOID lpvaddr,                     // address to query
    PMEMORY_BASIC_INFORMATION  pmi,     // structure to fill
    DWORD  dwLength                     // size of the buffer allocate for pmi
);

//-----------------------------------------------------------------------------------------------------------------
//
// VMLockPages: function to lock VM, kernel only, not exposed to user mode apps
//
BOOL
VMLockPages (
    PPROCESS pprc,                      // process
    LPVOID lpvaddr,                     // address to query
    DWORD  cbSize,                      // size to lock
    LPDWORD pPFNs,                      // the array to retrieve PFN
    DWORD   fOptions                    // options: see LOCKFLAG_*
);

//-----------------------------------------------------------------------------------------------------------------
//
// VMUnlockPages: function to unlock VM, kernel only, not exposed to user mode apps
//
BOOL
VMUnlockPages (
    PPROCESS pprc,                      // process
    LPVOID lpvaddr,                     // address to query
    DWORD  cbSize                       // size to lock
);

//-----------------------------------------------------------------------------------------------------------------
//
// VMFastLockPages/VMFastUnlockPages: [kernel internal only]
//      used internally to guarantee that pages will not be page out once paged in.
// NOTE: VMFastLockpages does NOT guarantee the pages is brought in. The caller is responsible for
//       bring in the pages.
//
void VMFastLockPages (PPROCESS pprc, PLOCKPAGELIST plp);
void VMFastUnlockPages (PPROCESS pprc, PLOCKPAGELIST plp);

//-----------------------------------------------------------------------------------------------------------------
//
// VMProtect: main function to change VM protection. 
//
BOOL 
VMProtect (
    PPROCESS pprc,          // the process where VM is to be allocated  
    LPVOID lpvaddr,         // starting address
    DWORD  cbSize,          // size, in byte, of the allocation
    DWORD  fNewProtect,     // new protection
    LPDWORD pfOldProtect    // old protect value
);

//
// VMSetAttributes - change the attributes of a range of VM
//
BOOL VMSetAttributes (
    PPROCESS pprc, 
    LPVOID lpvAddress, 
    DWORD cbSize, 
    DWORD dwNewFlags, 
    DWORD dwMask, 
    LPDWORD lpdwOldFlags);

//-----------------------------------------------------------------------------------------------------------------
//
// VMReserve: (internal only) reserve VM in DLL/SharedHeap/ObjectStore
//
LPVOID 
VMReserve (
    PPROCESS pprc,                  // the process where VM is to be allocated  
    DWORD   cbSize,                 // size of the reservation
    DWORD   fMemType,               // memory type (0, MEM_IMAAGE, MEM_MAPPED, MEM_AUTO_COMMIT)
    DWORD   dwSearchBase            // search base
                                    //      0 == same as VMAlloc (pprc, NULL, cbSize, MEM_RESERVE|fMemType, PAGE_NOACCESS)
                                    //      VM_DLL_BASE - reserve VM for DLL loading
                                    //      VM_SHARED_HEAP_BASE - reserve VM for shared heap
);


//-----------------------------------------------------------------------------------------------------------------
//
// VMAllocCopy: (internal only) reserve VM and VirtualCopy from source proceess/address.
//              Destination process is always the process whose VM is in use (pVMProc)
//
LPVOID
VMAllocCopy (
    PPROCESS pprcSrc,               // source process
    PPROCESS pprcDst,               // destination process
    LPVOID   pAddr,                 // address to be copy from
    DWORD    cbSize,                // size of the ptr
    DWORD    fProtect,              // protection
    PALIAS   pAlias                 // ptr to ALIAS structure
);

//
// Map/Unmap Page Directory of a process (privileged operation), used mostly 
// in memory tools (e.g. 'mi' in shell.exe).
//
LPVOID VMMapPD (DWORD dwProcId, PPROCMEMINFO ppmi);
BOOL VMUnmapPD (LPVOID pDupPD);

//
// VMSetPages: (internal function) map the address from dwAddr of cPages pages, to the pages specified
//             in array pointed by pPages. The pages are treated as physical page number if PAGE_PHYSICAL
//             is specified in fProtect. Otherwise, it must be page aligned
//
BOOL VMSetPages (
    PPROCESS pprc,
    DWORD dwAddr,
    LPDWORD pPages,
    DWORD cPages,
    DWORD fProtect);

//-----------------------------------------------------------------------------------------------------------------
//
// VMAllocPhys: allocate contiguous physical memory. 
//
LPVOID VMAllocPhys (
    PPROCESS pprc,
    DWORD cbSize,
    DWORD fProtect,
    DWORD dwAlignMask,
    DWORD dwFlags,
    PULONG pPhysAddr);

//
// data structures for static mapping support
//
typedef struct _STATICMAPPINGENTRY {
    struct _STATICMAPPINGENTRY *pNext;
    DWORD dwPhysStart;
    DWORD dwPhysEnd;
    LPVOID lpvMappedVirtual;
} STATICMAPPINGENTRY, *PSTATICMAPPINGENTRY;

#define MAX_STK_CACHE               4   // max number of stack we cached per-process
#define MAX_STK_INIT_COMMIT         4   // max number of pages committed when reuse old stack

//-----------------------------------------------------------------------------------------------------------------
//
// VMCreateStack: (internal only) Create a stack and commmit the bottom-most page
//
LPBYTE
VMCreateStack (
    PPROCESS    pprc,               // the process where stack is to be created
    DWORD       cbSize              // size of the stack
);

//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeStack: (internal only) free or cache a stack for later use
//
void VMFreeStack (
    PPROCESS pprc,
    DWORD dwBase,
    DWORD cbSize);

//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeExcessStacks: (internal only) free excess cached stack
//
void VMFreeExcessStacks (
    long nMaxStks);


//-----------------------------------------------------------------------------------------------------------------
//
// VMFreeLockPageList - (internal only), process cleanup, freeing locked page list
//
void VMFreeLockPageList (PPROCESS pprc);


//-----------------------------------------------------------------------------------------------------------------
//
// VMCacheStack: (internal only) cache a stack for future use
// NOTE: callable inside KCall
//
void VMCacheStack (
    PSTKLIST pStkList,
    DWORD dwBase,
    DWORD cbSize);


// shared heap support
LPVOID NKVirtualSharedAlloc (
    LPVOID lpvAddr,
    DWORD cbSize,
    DWORD fdwAction,
    PDWORD pdwTag);


//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------
//
// Memory-Mapped file support
//
//-----------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------

// VMMap* are VM functions which operate on process page tables, which are used
// for memory-mapped files.  They are different from MapVM*, which operate on
// the mapfile page tree.

//
// VMMapMove: move the address mapping from process VM to mapfile page tree.
// NOTE: move is more efficient for we'll need to go through physical memory
//       manager to inc/dec ref-count with VirtualCopy/VirtualFree. We don't 
//       need to if we simply move from one to the other.
//
BOOL VMMapMove (PFSMAP pfsmapDest, PPROCESS pprcSrc, LPVOID pAddrPrc, DWORD cbSize);

//
// VMMapMovePage: move a paging page from process VM to mapfile page tree.
//
BOOL VMMapMovePage (
    PFSMAP pfsmap,
    ULARGE_INTEGER liFileOffset,
    LPVOID pPage,
    WORD   InitialCount     // Initial use-count for the page, 0 if pageable or 1 if non-pageable
    );

//
// VMMapDecommit: Decommit view memory from process VM.  Will discard only
// read-only pages, or all, depending on fFreeAll.  Maintains mapfile page tree
// page refcounts.
//
BOOL VMMapDecommit (PFSMAP pfsmap, const ULARGE_INTEGER* pliFileOffset,
    PPROCESS pprc, LPVOID pAddrPrc, DWORD cbSize, BOOL fFreeAll);


typedef enum {
    VMMTE_FAIL,
    VMMTE_ALREADY_EXISTS,
    VMMTE_SUCCESS,
} VMMTEResult;

//
// VMMapTryExisting: used for paging, change VM attributes if page already committed
//
// NOTE: caller must have verified that the addresses are valid, and writeable if fWrite is TRUE
//
VMMTEResult VMMapTryExisting (PPROCESS pprc, LPVOID pAddrPrc, DWORD fProtect);


// VMMapClearDirtyPages is defined in mapfile.h

//-----------------------------------------------------------------------------------------------------------------
//
// type of page table enumeration function 
//
typedef BOOL (*PFN_PTEnumFunc) (LPDWORD pdwEntry, LPDWORD pEnumData);
#include "splheap.h"


#endif  // _NK_VM_H_

