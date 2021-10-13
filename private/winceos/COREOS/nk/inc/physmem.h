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
//    physmem.h - internal kernel physical memory header
//

#ifndef __NK_PHYSMEM_H__
#define __NK_PHYSMEM_H__

#include "kerncmn.h"

#define INUSE_PAGE      0xffff
#define MAX_PAGE_IDX    0xfff0

typedef struct {
    union {
        WORD idxBack;   // back ptr  if idxFwd != INUSE_PAGE
        WORD refCnt;    // ref-count if idxFwd == INUSE_PAGE
    };
    WORD idxFwd;
} PHYSPAGELIST, *PPHYSPAGELIST;

ERRFALSE (sizeof(PHYSPAGELIST) == sizeof(DWORD));

#define IDX_ZEROED_PAGE_LIST    0
#define IDX_UNZEROED_PAGE_LIST  1
#define IDX_DIRTY_PAGE_LIST     2
#define IDX_PAGEBASE            3

#define PFN_TO_PAGE_SHIFT       (VM_PAGE_SHIFT - PFN_SHIFT)
#define IDX_FROM_PFN(pfi, pfn)  ((((pfn) - (pfi)->paStart) >> PFN_TO_PAGE_SHIFT) + IDX_PAGEBASE)
#define PFN_FROM_IDX(pfi, idx)  ((pfi)->paStart + (((idx) - IDX_PAGEBASE) << PFN_TO_PAGE_SHIFT))

#define IS_PAGE_INUSE(pUseMap, ix)              (INUSE_PAGE == ((pUseMap)[ix]).idxFwd)

struct _REGIONINFO {
    DWORD   paStart;            /* start of available region */
    DWORD   paEnd;              /* end of region (last byte + 1) */
    PPHYSPAGELIST pUseMap;      /* ptr to page usage count map */
    DWORD   NumFreePages;       /* # of free pages in this region */
    DWORD   dwStaticMappedVA;   /* base of static mapped VA, if statically mapped*/
    DWORD   dwRAMAttributes;    /* RAM attributes */
};

struct _MEMORYINFO {
    PVOID       pKData;         /* start of kernel's data */
    PVOID       pKEnd;          /* end of kernel's data & bss */
    UINT        cFi;            /* # of entries in free memory array */
    REGIONINFO    *pFi;           /* Pointer to cFi REGIONINFO entries */
};

//
// macros
//
#define MEMORY_KERNEL_PANIC     0x10   // panic if memory drop below this (nothing work except for kernel debugger)
#define MEMORY_KERNEL_CRITICAL  0x50   // only kernel allocation is allowed if memory dropped below this level

#define IsDwordAligned(p)       (!((DWORD) (p) & 3))

#define INVALID_PHYSICAL_ADDRESS 0xFFFFFFFF

#define PHYS_MAX_32BIT_PFN          PA2PFN(0xFFFFF000)


//
// Page pool related defines
//

// Page count threshold to start page trimmer thread:
//
extern LONG g_cpTrimmerStart;

// Page count threshold to stop page trimmer thread:
//
extern LONG g_cpTrimmerStop;

// memory requests pending
extern long g_CntPagesNeeded;

//
// globals
//
extern MEMORYINFO MemoryInfo;

__declspec (noinline) void PreGrowStack (void);

#define AcquireMemoryLock(kcallid)      { PreGrowStack ();  \
                                          AcquireSpinLock (&g_physLock); \
                                          KCALLPROFON (kcallid); \
                                        }
#define ReleaseMemoryLock(kcallid)      { KCALLPROFOFF (kcallid); ReleaseSpinLock (&g_physLock); }


//
// function prototypes
//

//------------------------------------------------------------------------------
//  APIs exposed to other parts of kernel.
//------------------------------------------------------------------------------
// HoldMemory - hold cPages of physical pages
typedef enum _HLDPGEnum {
    HLDPG_FAIL=0,
    HLDPG_SUCCESS,
    HLDPG_RETRY
}HLDPGEnum;
#define MEMPRIO_KERNEL_PANIC                    0               // kernel panic threshold
#define MEMPRIO_KERNEL_CRITICAL                 1               // kernel critical threshold
#define MEMPRIO_KERNEL_LOW                      2               // kernel low threshold
#define MEMPRIO_APP_CRITICAL                    3               // app critical threshold
#define MEMPRIO_APP_LOW                         4               // app low threshold
#define MEMPRIO_HEALTHY                         5               // memory healthy watermark
#define NUM_MEM_TYPES                           6

#define MEMPRIO_KERNEL_INTERNAL     MEMPRIO_KERNEL_PANIC        // kernel internal allocation (fail only if panic)
#define MEMPRIO_KERNEL              MEMPRIO_KERNEL_CRITICAL     // allocation from other kernel components (fail if below kernel critical threshold)
#define MEMPRIO_SYSTEM              MEMPRIO_KERNEL_LOW          // allocation from system processes (fail if below kernel low threshold)
#define MEMPRIO_FOREGROUND          MEMPRIO_APP_CRITICAL        // allocation from foreground process (fail if below app critical threshold)
#define MEMPRIO_REGULAR_APPS        MEMPRIO_APP_LOW             // allocation from regular apps (fail if below app low threshold)  


extern LONG g_cpMemoryThreshold[NUM_MEM_TYPES];

#define PageAvailable(cMemPrio, cPages)         (PageFreeCount_Pool - (cPages) >= g_cpMemoryThreshold[cMemPrio])

// request memory
HLDPGEnum HoldMemory (DWORD cMemPrio, LONG cPages);
BOOL HoldMemoryWithWait (DWORD dwMemoryPrio, LONG cPages);
DWORD GetPagingCache (LPDWORD pdwFlags, DWORD dwMemPrio);
BOOL WaitForPagingMemory (PPROCESS pprc, DWORD dwPgRslt);

// release memory
PCREGIONINFO FreePhysPage (DWORD dwPFN);
BOOL FreePagingCache (DWORD dwPFN, LPDWORD pdwEntry);
BOOL ReholdPagingPage (DWORD dwPfn, LPDWORD pdwEntry);
void ReleaseHeldMemory (LONG cPages);

// get alredy held pages
DWORD GetHeldPage (LPDWORD pflags);

// convert a statically mapped kernel address to a physical page number
DWORD  GetPFN (LPCVOID pvAddr);

// DupPhysPage - increment the ref count of physical page
BOOL DupPhysPage (DWORD dwPFN, PBOOL pfIsMemory);

// grab a page
LPVOID GrabOnePage (DWORD dwPageType);

// free a grabbed page
void FreeKernelPage (LPVOID pPage);

// test for available memory on creating kernel objects.
// - throttle or fail if direct available memory is too low.
BOOL KernelObjectMemoryAvailable (DWORD dwMemPrio);

// register OOM notification
BOOL NKRegisterOOMNotification (PHANDLE phMemLow, PHANDLE phMemPressure, PHANDLE phGoodMem);


#define PM_PT_ZEROED                0x1000
#define PM_PT_STATIC_ONLY           0x2000
#define PM_PT_PHYS_32BIT            0x4000
#define PM_PT_NEEDZERO              0x8000      // out flag, to indicate page needs to be zero'd
// grab the 1st available physical page
DWORD PHYS_GrabFirstPhysPage (DWORD dwRefCnt, LPDWORD pflags, BOOL fHeld);

// InitMemoryPool -- initialize physical memory pool
void InitMemoryPool (void);

// read memory manager registry settings
void ReadMemoryMangerRegistry (void);

// clean dirty pages in the background (function never returns)
void CleanPagesInTheBackground (void);

// find the corresponding phsycal region of a physcal page
PREGIONINFO GetRegionFromPFN (DWORD dwPfn);

// check if an address is in kernel mode and actually mapped
BOOL IsStaticMappedAddress(DWORD dwAddr);

//------------------------------------------------------------------------------
//  APIs exposed outside of kernel.
//------------------------------------------------------------------------------
// get physical memory from NK to object store
BOOL NKGetKPhys (DWORD *pPages, int length);

// give physical memory to NK from object store
BOOL NKGiveKPhys (const DWORD *pPages, int length);

// set OOM handler
VOID NKSetOOMEventEx (HANDLE hEvtLowMemoryState, HANDLE hEvtGoodMemoryState, DWORD cpLowMemEvent, DWORD cpLow, DWORD cpCritical, DWORD cpLowBlockSize, DWORD cpCriticalBlockSize);

#endif // __NK_PHYSMEM_H__

