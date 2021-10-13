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
//    pgpool.h - paging pool related declarations
//

#ifndef _PG_POOL_H_
#define _PG_POOL_H_


typedef struct {
    // Parameters (constant)
    WORD   Target;                      // Target size, in pages (2^16 * 4096 = 256MB max)
    WORD   Maximum;                     // Maximum size, in pages
    WORD   ReleaseIncrement;            // Pages to release below target
    WORD   CriticalIncrement;           // Where to start & stop critical state, in pages
    WORD   NormalThreadPriority;
    WORD   CriticalThreadPriority;
    
    PHDATA phdTrimThread;               // Pointer to trim thread HDATA (thread is locked)
    HANDLE hTrimEvent;
    WORD   __unused;  // alignment/padding

    // Current state
    WORD   Flags;                       // PGPOOL_FLAG_*
    WORD   CurSize;                     // Current number of pool pages
    WORD   CurThreadPriority;

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

    // Debugging information
    DWORD TrimCount;        // # times the Target was exceeded and trim thread ran
    DWORD TrimTime;         // Amount of time (ms) the trim thread has run
    DWORD CriticalCount;    // # times the thread went into critical mode
    DWORD CriticalTime;     // Amount of time (ms) trim thread has run at critical prio
    DWORD FailCount;        // # allocs that failed for lack of memory

} PagePool_t;

#define PGPOOL_FLAG_TRIMMING       ((WORD)0x0001) // Trim thread is currently running
#define PGPOOL_FLAG_CRITICAL       ((WORD)0x0002) // Trim thread is currently running at critical priority
// Critical-mode trim already went through all R/O pages and didn't free enough.
// Now write back data at critical priority (BAD for perf but no options left!)
// Only happens to the file pool since the loader pages are all R/O.
#define PGPOOL_FLAG_CRITICAL_WRITE ((WORD)0x0004)
#define PGPOOL_FLAG_EXIT           ((WORD)0x0008) // Trim thread must now exit

#define PoolIsTrimming(pPool)               ((pPool)->Flags & PGPOOL_FLAG_TRIMMING)
#define PoolAboveTrimThreshold(pPool)       ((pPool)->CurSize >= (pPool)->Target)
#define PoolTrimIsComplete(pPool)           ((pPool)->CurSize <= ((pPool)->Target - (pPool)->ReleaseIncrement))

#define PoolIsCritical(pPool)               ((pPool)->Flags & PGPOOL_FLAG_CRITICAL)
#define PoolAboveCriticalThreshold(pPool)   ((pPool)->CurSize >= ((pPool)->Maximum - (pPool)->CriticalIncrement))
#define PoolCriticalIsComplete(pPool)       ((pPool)->CurSize <= ((pPool)->Maximum - 2*(pPool)->CriticalIncrement))

#define PagePoolCriticalFreePages           ((long) (PagePoolCriticalFreeMemory / VM_PAGE_SIZE))
#define EnoughMemoryToLeaveCritical()       (PageFreeCount > PagePoolCriticalFreePages)

extern PagePool_t *g_pLoaderPool, *g_pFilePool;


BOOL PagePoolInit ();
void SetPagingPoolPrio (DWORD ThreadPrio256);

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
    LPVOID*     ppReservation,
    DWORD       addr,
    BOOL        fUsePool,
    DWORD       fProtect
    );

// FreePagingPage - Release a page from pool or from common RAM.
void
FreePagingPage (
    PagePool_t* pPool,
    LPVOID      pPagingMem,
    LPVOID      pReservation,
    BOOL        fFromPool,
    DWORD       PagingResult
    );

// Called by VMDecommit and MapFreePage to return a page back to the right pool.
void
PGPOOLFreePage (
    PagePool_t* pPool,
    LPVOID pPage
    );

// Returns TRUE if the caller needs to keep trimming in the current state
// (critical or not).  Returns FALSE if the pool no longer needs trimming or
// if the caller needs to change state.
BOOL
PGPOOLNeedsTrim (
    PagePool_t* pPool,
    BOOL        fCritical   // Is the caller trimming in critical mode or normal?
    );


// Page-out functions
void PageOutIfNeeded (void);
void NKForcePageout (void);


#endif  // _PG_POOL_H_

