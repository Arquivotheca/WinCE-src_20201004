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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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

struct _FREEINFO {
    DWORD   paStart;            /* start of available region */
    DWORD   paEnd;              /* end of region (last byte + 1) */
    PWORD   pUseMap;            /* ptr to page usage count map */
};

struct _MEMORYINFO {
    PVOID       pKData;         /* start of kernel's data */
    PVOID       pKEnd;          /* end of kernel's data & bss */
    uint        cFi;            /* # of entries in free memory array */
    FREEINFO    *pFi;           /* Pointer to cFi FREEINFO entries */
};

//
// macros
//
#define STACK_RESERVE       15
#define MIN_PROCESS_PAGES   STACK_RESERVE

// MUST be kept in sync with data structure in coreos\filesys\filesys.h
#define MAX_MEMORY_SECTIONS     16

#define PM_PT_ZEROED        0           // zeroed page
#define PM_PT_ANY           1           // dirty page okay

#define IsDwordAligned(p)       (!((DWORD) (p) & 3))

#define IsValidKPtr(p)  ( \
           IsDwordAligned (p) \
        && ((char *)(p) >= (char *)MemoryInfo.pKData)   \
        && ((char *)(p) <  (char *)MemoryInfo.pKEnd)    \
        )

#define INVALID_PHYSICAL_ADDRESS 0xFFFFFFFF

#define WillTriggerPageOut(cpNeed)  ((cpNeed)+PageOutTrigger > PageFreeCount)

extern long PageOutTrigger;

//
// globals
//
extern MEMORYINFO MemoryInfo;


//
// function prototypes
//

// HoldPages - hold cPages of physical pages
BOOL HoldPages (int cpNeed, BOOL fQueryOnly);

// get alredy held pages
DWORD GetHeldPage (DWORD dwPageType);

// Return a page from allocated status to held status
DWORD ReholdPage (DWORD dwPfn);

// DupPhysPage - increment the ref count of physical page
BOOL DupPhysPage (DWORD dwPFN);

// FreePhysPage - decrement the ref count of a physical page, free the page if
// ref count == 0.  Returns TRUE if the page was freed.
BOOL FreePhysPage (DWORD dwPFN);

//
// KC_GrabFirstPhysPage - grab a page, fHeld specifies if we already hold the page.
//
// NOTE: It's possible to return a dirty page even if PM_PT_ZEROED is requested.
//       The caller of this function can check pMem->pFwd to see if the page is dirty
//       and zero the page if requested
//
PDLIST KC_GrabFirstPhysPage (DWORD dwRefCnt, DWORD dwPageType, BOOL fHeld);

// grab a page
LPVOID GrabOnePage (DWORD dwPageType);

// InitMemoryPool -- initialize physical memory pool
void InitMemoryPool (void);

// get physical memory from NK to object store
BOOL NKGetKPhys (void *ptr, ulong length);

// give physical memory to NK from object store
BOOL NKGiveKPhys (void *ptr, ulong length);

// clean dirty pages in the background (function never returns)
void CleanPagesInTheBackground (void);

#endif // __NK_PHYSMEM_H__

