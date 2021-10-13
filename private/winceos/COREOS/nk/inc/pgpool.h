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
    
    // Enumeration state is used to track the thread's progress through the
    // objects it is flushing.  If an object disappears (eg. module is unloaded)
    // the kernel will remove it from this state.
    union {
        // The state differs between the loader pool and the file pool.
        struct {
            PDLIST pNextProcess;        // Current enumeration position
            // Module enumeration is tracked by the StoredModNum value in PageOutModules
        } loader;
        struct {
            // Use different enumeration pointers for critical and normal enumeration,
            // so that when we enter critical enumeration we don't lose track of which
            // file was being flushed.
            PDLIST pStartFile;          // Where enumeration started
            PDLIST pNextFile;           // Current enumeration position
            PDLIST pStartCriticalFile;  // Where critical-enum started
            PDLIST pNextCriticalFile;   // Current critical-enum position
        } file;
    } EnumState;

    CRITICAL_SECTION cs;    // Protects CurSize, Flags and EnumState
} PagePool_t;

extern PagePool_t g_LoaderPool, g_FilePool;

BOOL PagePoolInit ();

// Worker function for IOCTL_KLIB_GET_POOL_STATE.
BOOL
PagePoolGetState (
    NKPagePoolState* pState
    );

// Notifies the page pool about process deletion.  Remove the process from the
// trim thread's enumeration state, if it's there.
void
PagePoolDeleteProcess (
    PPROCESS pProcess
    );

// Skips NK.  Returns NULL if NK is the only process.  Does not track where
// enumeration started.
PPROCESS
PagePoolGetNextProcess ();

// Notifies the page pool about file deletion.  Remove the file from the
// trim thread's enumeration state, if it's there.
void
PagePoolDeleteFile (
    PFSMAP pMap
    );

// Tracks where the enumeration started, and returns NULL if we've enumerated
// through all the maps and gotten back to where we started.
PFSMAP
PagePoolGetNextFile (
    BOOL fEnumerationStart,  // TRUE: reset the start position, FALSE: check if we looped
    BOOL fCritical
    );

// GetPagingPage - Allocate a page from pool or from common RAM.
// Caller is responsible for calling FreePagingPage.
LPVOID
GetPagingPage (
    PagePool_t* pPool,
    DWORD       addr
    );

// FreePagingPage - Release a page from pool or from common RAM.
void
FreePagingPage (
    PagePool_t* pPool,
    LPVOID      pPagingMem
    );

// Called by VMDecommit and MapFreePage to return a page back to the right pool.
void
PGPOOLFreePage (
    PagePool_t* pPool,
    DWORD dwPfn
    );

// Returns TRUE if the caller needs to keep trimming.
BOOL
PGPOOLNeedsTrim (
    PagePool_t* pPool
    );

// Page-out functions
void NKForcePageout (void);


#endif  // _PG_POOL_H_

