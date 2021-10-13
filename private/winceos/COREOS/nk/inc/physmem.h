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
    uint        cFi;            /* # of entries in free memory array */
    REGIONINFO    *pFi;           /* Pointer to cFi REGIONINFO entries */
};

//
// macros
//
#define STACK_RESERVE       15
#define MIN_PROCESS_PAGES   STACK_RESERVE

#define IsDwordAligned(p)       (!((DWORD) (p) & 3))

#define INVALID_PHYSICAL_ADDRESS 0xFFFFFFFF

#define PHYS_MAX_32BIT_PFN          PA2PFN(0xFFFFF000)


//
// Page pool related defines
//

// Page count threshold to start page trimmer thread:
// PageFreeCount < (g_cpPageOutLow + g_cpLowThreshold)
extern long g_cpPageOutLow;

// Page count threshold to stop page trimmer thread:
// PageFreeCount > (g_cpPageOutHigh + g_cpLowThreshold)
extern long g_cpPageOutHigh;

// Page count threshold to start page trimmer thread:
// PageOutTrigger = (g_cpPageOutLow + g_cpLowThreshold)
extern long PageOutTrigger;

// Page count threshold to stop page trimmer thread:
// PageOutLevel =  (g_cpPageOutHigh + g_cpLowThreshold)
extern long PageOutLevel;

// Page count threshold below which page trimmer
// thread runs at current thread priority or lower
// == device is in low memory below this threshold
extern long g_cpLowThreshold;

// Page count of maximum allocation allowed when
// device memory is below low threshold
extern long g_cpLowBlockSize;

// Page count threshold below which page trimmer
// thread runs at current thread priority or lower
// == device is in critical memory below this threshold
extern long g_cpCriticalThreshold;

// Page count of maximum allocation allowed when
// device memory is below critical threshold
extern long g_cpCriticalBlockSize;

// Minimum # of pages to create a new process
extern long g_cpMinPageProcCreate;

// PagePoolTrimState can be one of the following values
enum {
    PGPOOL_TRIM_NOTINIT = 0,
    PGPOOL_TRIM_IDLE,
    PGPOOL_TRIM_SIGNALED,
    PGPOOL_TRIM_RUNNING
};
extern LONG PagePoolTrimState;
#define IsPagePoolTrimIdle(p) ((p) == PGPOOL_TRIM_IDLE)

// Minimum pages to be paged out
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
// HoldPages - hold cPages of physical pages
typedef enum _HLDPGEnum {
    HLDPG_FAIL=0,
    HLDPG_SUCCESS,
    HLDPG_RETRY
}HLDPGEnum;
DWORD HoldPages (int cpNeed);
BOOL HoldPagesWithWait (int cpNeed);

// get alredy held pages
DWORD GetHeldPage (LPDWORD pflags);

// Return a page from allocated status to held status
BOOL ReholdPage (DWORD dwPfn);

// DupPhysPage - increment the ref count of physical page
BOOL DupPhysPage (DWORD dwPFN);

// FreePhysPage - decrement the ref count of a physical page, free the page if
// ref count == 0.  Returns TRUE if the page was freed.
BOOL FreePhysPage (DWORD dwPFN);

// FreeKernelPage - free a kernel page
BOOL FreeKernelPage (LPVOID pPage);

// grab a page
LPVOID GrabOnePage (DWORD dwPageType);


#define PM_PT_ZEROED                0x1000
#define PM_PT_STATIC_ONLY           0x2000
#define PM_PT_PHYS_32BIT            0x4000
#define PM_PT_NEEDZERO              0x8000      // out flag, to indicate page needs to be zero'd
// grab the 1st available physical page
DWORD PHYS_GrabFirstPhysPage (DWORD dwRefCnt, LPDWORD pflags, BOOL fHeld);

// InitMemoryPool -- initialize physical memory pool
void InitMemoryPool (void);

// clean dirty pages in the background (function never returns)
void CleanPagesInTheBackground (void);

// find the corresponding phsycal region of a physcal page
PREGIONINFO GetRegionFromPFN (DWORD dwPfn);

#if defined(MIPS) || defined(SH4)
// check if an address is in kernel mode and actually mapped
BOOL IsStaticMappedAddress(DWORD dwAddr);
#endif

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

