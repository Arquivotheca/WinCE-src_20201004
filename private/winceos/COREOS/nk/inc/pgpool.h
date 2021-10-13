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
//    pgpool.h - paging pool related declarations
//

#ifndef _PG_POOL_H_
#define _PG_POOL_H_

#define CURRENT_PAGE_POOL_VERSION       1

typedef struct {
    // Parameters (constant)
    DWORD   Target;                     // Target size, in pages
    DWORD   CurSize;                    // Current number of pool pages
} PagePool_t;

extern PagePool_t g_PagingPool;

BOOL PagePoolInit ();

// Worker function for IOCTL_KLIB_GET_POOL_STATE.
BOOL
PagePoolGetState (
    NKPagePoolState* pState
    );

// Add an object into the page pool's pageout list
void
PagePoolAddObject (PPAGEOUTOBJ pPageoutObj);

// Remove an object from page pool's pageout list
void
PagePoolRemoveObject (PPAGEOUTOBJ pPageoutObj, BOOL fMakeInvalid);

// GetPagingPage - Allocate a page from pool or from common RAM.
// Caller is responsible for calling FreePagingPage.
LPVOID
GetPagingPage (
    PagePool_t* pPool,
    PPROCESS    pprc,
    DWORD       dwAddr
    );

// FreePagingPage - Release a page from pool or from common RAM.
void
FreePagingPage (
    PagePool_t* pPool,
    LPVOID      pPagingMem
    );

// Called by VMDecommitCodePages and MapFreePage to return a page back to the right pool.
void
PGPOOLFreePage (
    PagePool_t* pPool,
    DWORD dwPfn,
    LPDWORD pdwEntry
    );

//------------------------------------------------------------------------------
// Returns 0 if memory is unavailable.  In that case the caller should loop
// and try again, perhaps freeing up other data if possible.
//------------------------------------------------------------------------------
DWORD
PGPOOLGetPage (
    PagePool_t* pPool,
    LPDWORD     pflags,
    DWORD       dwMemPrio
    );

// Page-out functions
void NKForcePageout (void);

DWORD GetOnePageForPaging (PagePool_t* pPool, LPDWORD pdwFlags, DWORD dwMemPrio);
void FreeOnePagingPage (PagePool_t *pPool, DWORD dwPfn);

extern MEMORYSTATISTICS g_MemStat;


#endif  // _PG_POOL_H_

