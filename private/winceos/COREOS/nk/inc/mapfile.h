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
//    mapfile.h - internal kernel memory mapped file header
//

#ifndef _NK_MAPFILE_H_
#define _NK_MAPFILE_H_

#include "kerncmn.h"


//------------------------------------------------------------------------------
// PAGE TREE IMPLEMENTATION
//------------------------------------------------------------------------------

// The page tree is an imitation of the page tables, but is a software-only
// object, unknown to the hardware.  It is only used for memory-mapped files;
// never for process VM.  It is very similar to the page tables, but points to
// virtual addresses instead of physical.  The organization of the page tree
// is that the lowest level is composed of MAPPageTable objects, pointing at
// pages of memory.  Above that are MAPPageTable objects pointing at the lower
// levels of page tables.  We also keep a use-counter for each table, so that
// we know when it no longer points to any pages.

typedef DWORD MapPTEntry;

#define MAP_NUM_PT_ENTRIES   (VM_PAGE_SIZE / sizeof(MapPTEntry))

typedef struct {
    MapPTEntry pte[MAP_NUM_PT_ENTRIES];  // Points either to pages or to other PTs
} MAPPageTable;

// atomic view size requirements
ERRFALSE ((ULONGLONG) 64 * 1024 * VM_PAGE_SIZE < (DWORD) -1);

// 4096 * 1024^6 exceeds 64 bits (2^64)
#define MAP_MAX_TREE_HEIGHT  ((WORD)6)
ERRFALSE(((ULONGLONG)-1 / VM_PAGE_SIZE + 1)
         <= (ULONGLONG) MAP_NUM_PT_ENTRIES * MAP_NUM_PT_ENTRIES * MAP_NUM_PT_ENTRIES
                      * MAP_NUM_PT_ENTRIES * MAP_NUM_PT_ENTRIES * MAP_NUM_PT_ENTRIES);

// Each shift is a multiplication by 1024, starting at VM_PAGE_SIZE
#define MAP_PT_SHIFT(height)                    (((height) * 10) + VM_PAGE_SHIFT)
#define TreeSizeAtHeight(height)                (((height) >= MAP_MAX_TREE_HEIGHT) ? (ULONGLONG)-1 : ((ULONGLONG) 1 << MAP_PT_SHIFT(height)))
#define TreeNodeIndex(offset64, height)         ((0 == (height)) ? (WORD)MAP_NUM_PT_ENTRIES : ((WORD) ((offset64) >> MAP_PT_SHIFT((height)-1)) & (MAP_NUM_PT_ENTRIES - 1)));
#define TreeOffsetAtHeight(offset64, height)    ((offset64) & ((ULONGLONG) (MAP_NUM_PT_ENTRIES-1) << MAP_PT_SHIFT((height) - 1)))

// Page table entry definition.  The top part of the entry is used for the page
// pointer.  The bottom part of the entry is a use counter.  Use counters for
// tree nodes track how many node entries are in use inside the node.  Use
// counters for data pages track how many views the page is paged into at the
// current time.  A page cannot be paged into more than MAP_PT_MAX_COUNT views
// at a time.
#define MAP_PT_POINTER_MASK                     ((DWORD) ~(VM_PAGE_SIZE - 1))
#define MAP_PT_PHYSADDR_MASK                    ((DWORD) ~(VM_PAGE_SIZE - 1))
#define MAP_PT_COUNT_MASK                       ((DWORD) (VM_PAGE_SIZE - 1))
#define MAP_PT_MAX_COUNT                        MAP_PT_COUNT_MASK
#define MapEntryIsCommitted(pte)                (0 != (pte))
#define MapEntry2PTBL(pte)                      ((MAPPageTable*) ((pte) & MAP_PT_POINTER_MASK))
#define MapEntry2PhysAddr(pte)                  ((DWORD)         ((pte) & MAP_PT_PHYSADDR_MASK))
#define MapEntry2Count(pte)                     ((WORD)          ((pte) & MAP_PT_COUNT_MASK))
#define MapMakeReservedEntry()                  ((MapPTEntry)    0)
#define MapMakeCommittedEntry(pPage, count)     ((MapPTEntry)    ((DWORD) (pPage) | (count)))


typedef struct {
    PTENTRY RootEntry;
    
    // Height is the number of page table levels above a page.
    // May be between 0 and 6.
    //      Height = 0 : Points straight at the page    (4KB or below)
    //      Height = 1 : Only one PT                    (4MB or below)
    //      Height = 2 : One top PT + 2nd level of PTs  (4GB or below)
    //      Height = 6 is maximum
    WORD height;
    WORD flags;     // MAP_PT_FLAGS_*, below
    
    // This critical section should be used whenever accessing the root or
    // anything underneath it.
    CRITICAL_SECTION csVM;
} MAPPageTree;

// CRITICAL SECTION ORDERING RULES:
// 1. Two PageTrees should never be locked by the same thread at the same time.
// 2. If locking a PageTree and process VM at the same time, lock the PageTree
//    first.
// 3. If locking MapCS and a PageTree at the same time, acquire MapCS first.

#define MAP_PT_FLAGS_INITIALIZED  (0x0001)


// MapVM* are mapfile functions which operate on the page tree.  They are
// different from VMMap*, which operate on process page tables.

//
// MapVMInit: Initialize a page tree for a newly opened file
//
BOOL
MapVMInit (
    PFSMAP pfsmap
    );

//
// MapVMCleanup: Delete the page tree as a file is being closed
//
void
MapVMCleanup (
    PFSMAP pfsmap
    );

//
// MapVMAddPage: Add a data page to the specified position in the page tree.
// Increments the physical refcount on the page if successful.  This is the 
// only way that nodes or pages are added to the page tree.
// (Used during the process of MapVMFill or on its own during page faults)
//
BOOL
MapVMAddPage (
    PFSMAP pfsmap,
    ULARGE_INTEGER liFileOffset,
    LPVOID pPage,
    WORD   InitialCount     // Initial use-count for the page, 0 if pageable or 1 if non-pageable
    );

//
// MapVMCopy: VirtualCopy from the mapfile page tree to process VM.
//
BOOL
MapVMCopy (
    PFSMAP   pfsmap,
    PPROCESS pprcDest,
    LPVOID   pAddrInProc,
    ULARGE_INTEGER liFileOffset,
    DWORD    cbSize,
    DWORD    fProtect
    );

//
// MapVMFill: VirtualCopy from process VM to the mapfile page tree.  Used to
// populate a non-pageable mapfile.  Copies the entire mapping.
//
BOOL
MapVMFill (
    PFSMAP   pfsmap,
    PPROCESS pprcSrc,
    LPVOID   pAddrInProc,
    DWORD    cbSize
    );

//
// MapVMDecommit: Decommit pages from a view in process VM.  Used to
// reclaim physical memory in low-memory conditions, and to clean up when a
// view is unmapped.  Will discard only clean pages, or all, depending on
// DiscardDirty.
//
BOOL
MapVMDecommit (
    PFSMAP   pfsmap,
    PPROCESS pprc,
    LPVOID   pAddrInProc,
    ULARGE_INTEGER liFileOffset,
    DWORD    cbSize,
    BOOL     DiscardDirty       // TRUE: discard all, FALSE: discard only clean (R/O) pages
    );

//
// MapVMFreePage: Decrement the view reference count for the data page at the
// specified position in the page tree.  Frees the page if it is no longer part
// of any views.  Used to release memory when a view is unmapped or when a page
// is removed from a view (used during the process of MapVMDecommit).
//
BOOL
MapVMFreePage (
    PFSMAP pfsmap,
    ULARGE_INTEGER liFileOffset
    );

//
// MapVMResize: Grow or shrink the page tree to the new size.  If shrinking,
// all views to cut-off parts of the map must already be unmapped, so that
// those pages are already flushed and decommitted.  Used to resize file
// cache mappings.
//
BOOL
MapVMResize (
    PFSMAP pfsmap,
    ULARGE_INTEGER liNewSize
    );


#ifdef DEBUG
//
// MapVMValidate: Sanity-check the page tree contents
//
BOOL
MapVMValidate (
    PFSMAP pfsmap
    );
#endif // DEBUG


//------------------------------------------------------------------------------
// MAIN MAPFILE FUNCTIONALITY
//------------------------------------------------------------------------------

typedef struct _ViewCount_t {
    LONG  count;

#ifdef DEBUG
    DWORD signature;  // debug safety checking
#endif // DEBUG
} ViewCount_t;

#define MAP_VIEWCOUNT_SIG       (0x11999999)


typedef struct _ViewFlushInfo_t {
    DWORD   dwNumElements;      // Number of segments/pages in array (= number of pages in view)
    DWORD   cbFlushSpace;       // size of prgPages/prgSegments+prgulOffsets.  Redundant with dwNumElements, saved only for convenience.
    
    union {
        // Gathered maps only: param space for WriteFileGather
        struct {
            FILE_SEGMENT_ELEMENT* prgSegments;    // Array of segments, one per page
            ULARGE_INTEGER*       prgliOffsets;   // Array of page offsets in file, one per segment
        } gather;

        // Non-gathered maps only: space for list of all pages being flushed
        struct {
            PVOID*                prgPages;       // Array of pages being flushed
        } nogather;
    };
} ViewFlushInfo_t;


typedef struct _MAPVIEW {
    DLIST   link;               // list of views, must be first
    CommonMapView_t base;
    PHDATA  phdMap;             // the HDATA of the map file
    DWORD   dwRefCount;         // Bottom bits: Lock Count, how many threads are
                                // actively accessing the view
                                // Top bits: View Count, how many instances of
                                // the view are open at once
    WORD    wViewOffset;        // offset of the view where the view is mapped
    WORD    wFlags;             // flags/protection

    union {
        // File-backed maps: Flush info
        ViewFlushInfo_t flush;

        // RAM-backed maps only: counter shared between all views of the map
        // inside the same process
        struct {
            ViewCount_t*          pViewCntOfMapInProc;
        } rambacked;
    };

#ifdef DEBUG
    DWORD   signature;  // debug safety checking
#endif // DEBUG
} MAPVIEW;

#define MAP_VIEW_INCR           0x00010000
#define MAP_LOCK_INCR           0x00000001
#define MAP_MAX_VIEW_REFCOUNT   ((WORD)-1)
#define MAP_VIEW_COUNT(dwRef)   HIWORD(dwRef)
#define MAP_LOCK_COUNT(dwRef)   LOWORD(dwRef)

#define MAP_VIEW_SIG            (0x22999999)


typedef struct _FSMAP {
    DLIST       maplink;        // List of maps, must be first
    PHDATA      phdMap;         // Pointer back to the map handle
    PHDATA      phdFile;        // File, or NULL for just vm
    ULARGE_INTEGER liSize;      // Size of the file mapping
    DWORD       dwNumViews;     // # of views created for this mapfile
    DWORD       dwFlags;        // Flags/protection
    
    union {
        // Direct-ROM mappings only
        LPBYTE  pROMBase;       // Points directly to ROM
        
        // RAM-backed, or fixed-address mappings only
        struct {
            LPBYTE  pKrnBase;   // Kernel mode base for entire mapping
            LPBYTE  pUsrBase;   // User mode base for entire mapping
        };

        // Cache mappings only
        DWORD   FSSharedFileMapId;  // Opaque handle the kernel uses to refer to the cache mapping
    };

    MAPPageTree pgtree;         // Page tree tracking all pages for this file

#ifdef DEBUG
    DWORD       signature;      // debug safety checking
#endif // DEBUG

} FSMAP;

#define MAP_FSMAP_SIG           (0x33999999)

// NOTE: The following attributes cannot conflict with
//       PAGE_READWRITE, PAGE_READONLY, and SEC_COMMIT
#define MAP_PAGEABLE        0x1000
#define MAP_DIRECTROM       0x2000  // File mapped directly from ROM
#define MAP_ATOMIC          0x4000  // Always flush atomically (all pages or no pages) -- used by CEDB volumes and registry hives
#define MAP_GATHER          0x8000  // Perf improvement: transact with WriteFileGather when flushing.  May also be atomic.
#define MAP_FILECACHE       0x0100  // Mapping created by the file system cache
#define MAP_USERFIXADDR     0x0200  // user views always mapped to fix address

// Flags the user can pass to CreateFileMapping
#define MAP_VALID_PROT      (PAGE_READONLY | PAGE_READWRITE | SEC_COMMIT | PAGE_INTERNALDBMAPPING | PAGE_FIX_ADDRESS_READONLY | PAGE_INTERNALCACHE)
// Internal flags that cannot be passed to CreateFileMapping by the user
#define PAGE_INTERNALCACHE  SEC_VLM  // Collide with SEC_VLM because we don't expect it to be used with mapfiles


extern DLIST g_MapList;  // Protected by MapCS


//
// MapfileInit - init function of memory mapped file
//
BOOL MapfileInit (void);

//
// NKCreateFileMapping - CreateFileMapping API
//
HANDLE
NKCreateFileMapping (
    PPROCESS pprc,
    HANDLE hFile, 
    LPSECURITY_ATTRIBUTES lpsa,
    DWORD flProtect, 
    DWORD dwMaximumSizeHigh, 
    DWORD dwMaximumSizeLow, 
    LPCTSTR lpName 
    );

//
// MAPMapView - implementation of MapViewOfFile
//
LPVOID MAPMapView (PPROCESS pprc, HANDLE hMap, DWORD dwAccess, DWORD dwOffsetHigh, DWORD dwOffsetLow, DWORD cbSize);

//
// MAPUnmapView - implementation of UnmapViewOfFile
//
BOOL MAPUnmapView (PPROCESS pprc, LPVOID pAddr);

//
// MAPFlushView - implement FlushViewOfFile
//
BOOL MAPFlushView (PPROCESS pprc, LPVOID pAddr, DWORD cbSize);

//
// MAPUnmapAllViews - unmap all views on process exit
//
void MAPUnmapAllViews (PPROCESS pprc);

#ifdef ARM
//
// MAPUncacheViews - turn a view uncache at a particular address
//
BOOL MAPUncacheViews (PPROCESS pprc, DWORD dwAddr, DWORD cbSize);
#endif

// MAP_VALIDATE_SIG always evaluates to TRUE so that it can be used in expressions
#ifdef DEBUG
#define MAP_SET_SIG(pObj, sig)       ((pObj)->signature = sig)
#define MAP_VALIDATE_SIG(pObj, sig)  (DEBUGCHK ((sig) == (pObj)->signature), TRUE)
#else
#define MAP_SET_SIG(pObj, sig)
#define MAP_VALIDATE_SIG(pObj, sig)  (TRUE)
#endif // DEBUG


//------------------------------------------------------------------------------
// FLUSH FUNCTIONALITY
//------------------------------------------------------------------------------

// Temporary status during the progress of a flush
typedef struct {
    PPROCESS pprc;
    PFSMAP   pfsmap;
    CommonMapView_t* pViewBase;
    ViewFlushInfo_t* pFlush;
    FSLogFlushSettings FileStruct;
} FlushParams;

typedef BOOL  (*PFN_MapFlushProlog) (FlushParams* pInfo);
typedef BOOL  (*PFN_MapFlushEpilog) (FlushParams* pInfo, BOOL fSuccess, DWORD cTotalPages, DWORD idxLastPage);
typedef BOOL  (*PFN_MapFlushRecordPage) (FlushParams* pInfo, PVOID pAddr, DWORD idxPage);
typedef DWORD (*PFN_MapFlushWritePages) (FlushParams* pInfo, DWORD cPages);

typedef struct {
    PFN_MapFlushProlog     pfnProlog;      // Optional, may be NULL
    PFN_MapFlushEpilog     pfnEpilog;
    PFN_MapFlushRecordPage pfnRecordPage;
    PFN_MapFlushWritePages pfnWritePages;
} MapFlushFunctions;


// This function would be in vm.h, except for the dependency on the types above

//
// VMMapClearDirtyPages - Call a mapfile callback function on every dirty page
//                        in the range, and mark them read-only before returning.
//                        Returns the number of pages that were cleared.
//
DWORD VMMapClearDirtyPages (PPROCESS pprc, LPBYTE pAddr, DWORD cbSize,
                            PFN_MapFlushRecordPage pfnRecordPage, FlushParams* pInfo);


// Exports from mapflush.c
DWORD ValidateFile(PFSMAP pfsmap);

DWORD DoFlushView (PPROCESS pprc, PFSMAP pfsmap, CommonMapView_t* pview, ViewFlushInfo_t* pFlush, LPBYTE pAddr, DWORD cbSize, DWORD FlushFlags);

void PageOutFiles (BOOL fCritical);
void WriteBackAllFiles ();


//------------------------------------------------------------------------------
// FILE CACHE FUNCTIONALITY
//------------------------------------------------------------------------------

// Filesys cache filter exports
extern FSCacheExports g_FSCacheFuncs;

BOOL RegisterForCaching (const FSCacheExports* pFSFuncs, NKCacheExports* pNKFuncs);
BOOL LockCacheView (DWORD dwAddr, PFSMAP* ppfsmap, CommonMapView_t** ppview);
void UnlockCacheView (PFSMAP pfsmap, CommonMapView_t* pview);


#endif  // _NK_MAPFILE_H_
