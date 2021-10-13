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

#define PAGEALIGN_UP(X)         (((X) + VM_PAGE_OFST_MASK) & ~VM_PAGE_OFST_MASK)
#define PAGEALIGN_DOWN(X)       ((X) & ~VM_PAGE_OFST_MASK)
#define PAGECOUNT(x)            (((x) + VM_PAGE_OFST_MASK) >> VM_PAGE_SHIFT)

#define BLOCKALIGN_UP(x)        (((x) + VM_BLOCK_OFST_MASK) & ~VM_BLOCK_OFST_MASK)
#define BLOCKALIGN_DOWN(x)      ((x) & ~VM_BLOCK_OFST_MASK)


#define VM_LAST_USER_PDIDX      VA2PDIDX(VM_SHARED_HEAP_BASE)


//-----------------------------------------------------------------------------------------------------------------
//
// VM API parameter grouping
//
// valid allocation type
#define VM_VALID_ALLOC_TYPE     (MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN | MEM_IMAGE | MEM_AUTO_COMMIT | MEM_MAPPED)
// allocation types that applications cannot use, valid only inside kernel
#define VM_INTERNAL_ALLOC_TYPE  (MEM_MAPPED)

// valid protection
#define VM_VALID_PROT           (PAGE_NOACCESS | PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE | PAGE_EXECUTE_READ \
                                 | PAGE_EXECUTE_READWRITE | PAGE_GUARD | PAGE_NOCACHE)

// all read-only protections
#define VM_READONLY_PROT        (PAGE_READONLY | PAGE_EXECUTE | PAGE_EXECUTE_READ)

// all read-write protections
#define VM_READWRITE_PROT       (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)

// all executable protections
#define VM_EXECUTABLE_PROT      (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)


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
    union   {
        PALIAS      pAlias;     // point to alias if exists
        PPTENTRY    pPTE;       // original PTE
    };
#ifdef VM_DEBUG
#define MAX_CALLSTACK 20
    DWORD       callstack[MAX_CALLSTACK];  // to keep the callstack of the allocation
#endif
};

struct _STKLIST {
    PSTKLIST    pNextStk;       // next pointer
    LPVOID      pStkBase;       // stack base
    DWORD       cbStkSize;      // stack size
};

struct _ALIAS {
    DWORD       dwProcessId;    // which process
    DWORD       dwAliasBase;    // alias base address
    DWORD       cbSize;
};

#ifdef VM_DEBUG
#define HEAP_VALIST     HEAP_EVENT
#else
#define HEAP_VALIST     HEAP_STKLIST
#endif

//
// the following 2 functions are used in vmxxx.h, need to define them before 
// include the header files

// convert a physical page number to the statically mapped kernel address
LPVOID Pfn2Virt (DWORD dwPfn);

__inline LPVOID Pfn2VirtUC (DWORD dwPfn)
{
    LPVOID pva = Pfn2Virt (dwPfn);
    if (pva) {
        pva = (LPVOID) ((DWORD) pva | 0x20000000);
    }
    return pva;
}

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
#define IsPageAligned(va)       (!((va) & VM_PAGE_OFST_MASK))
#define IsBlockAligned(va)      (!((va) & VM_BLOCK_OFST_MASK))


//------------------------------------------------------------------------
//
// Function protytypes
//

// convert a statically mapped kernel address to a physical page number
DWORD  GetPFN (LPCVOID pvAddr);

// get the MEM_XXX bits from a committed page table entry
DWORD ProtectFromEntry (DWORD dwEntry);

// get the page protection bits (PG_XXX) from MEM_XXX bits
DWORD PageParamFormProtect (DWORD fProtect, DWORD dwAddr);


// validate if (dwBase -> dwBase + dwSize) is a valid user stack
_inline BOOL IsValidUserStack (DWORD dwBase, DWORD dwSize)
{
    return !(dwBase & VM_BLOCK_OFST_MASK)
        && !(dwSize & VM_BLOCK_OFST_MASK)
        && ((int) dwSize > 0)
        && IsValidUsrPtr ((LPCVOID) dwBase, dwSize, TRUE);
}


// AllocatePTBL - allocate a new page for pagetable
PPAGETABLE AllocatePTBL (void);

// AllocatePD - allocate a new page directory
PPAGEDIRECTORY AllocatePD (void);

// free page directory
void FreePD (PPAGEDIRECTORY pd);

// translate a virtual address to kernel static mapped address
LPVOID GetKAddr (LPCVOID pAddr);
LPVOID GetKAddrOfProcess (PPROCESS pprc, LPCVOID pAddr);
DWORD GetPFNOfProcess(PPROCESS, LPCVOID);


BOOL VMFindAlloc (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, BOOL fRelease);
void VMFreeAllocList (PPROCESS pprc);
BOOL VMAddAllocToList (PPROCESS pprc, LPVOID pAddr, DWORD cbSize, PALIAS pAlias);


void ZeroPage(void *pvPage);

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
BOOL VMProcessPageFault (PPROCESS pprc, DWORD dwAddr, BOOL fWrite);

// VMInit: Initialize per-process VM (called at process creation) 
BOOL VMInit (PPROCESS pprc);

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
    DWORD cbSize,           // # of pages
    DWORD fProtect
    );

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
    LPVOID lpvaddr,                     // address to query
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
    DWORD fdwAction);


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
// VMMapCopy: VirtualCopy from process VM to mapfile page tree.
//
BOOL VMMapCopy (PFSMAP pfsmapDest, PPROCESS pprcSrc, LPVOID pAddrPrc, DWORD cbSize);

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


#endif  // _NK_VM_H_

