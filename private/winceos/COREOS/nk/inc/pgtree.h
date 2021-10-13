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

#pragma once

//
//    pgtree.h - page related declarations
//
//------------------------------------------------------------------------------
// PAGE TREE IMPLEMENTATION
//------------------------------------------------------------------------------

// The page tree is an imitation of the page tables, but is a software-only
// object, unknown to the hardware.  It is only used for memory-mapped files;
// never for process VM.  It is very similar to the page tables, but points to
// virtual addresses instead of physical.  The organization of the page tree
// is that the lowest level is composed of TreePageTable objects, pointing at
// pages of memory.  Above that are TreePageTable objects pointing at the lower
// levels of page tables.  We also keep a use-counter for each table, so that
// we know when it no longer points to any pages.


#ifndef _PAGE_TREE_H_
#define _PAGE_TREE_H_

typedef DWORD TreePTEntry, *PTreePTEntry;

#define TREE_NUM_PT_ENTRIES   (VM_PAGE_SIZE / sizeof(TreePTEntry))

typedef struct {
    TreePTEntry tte[TREE_NUM_PT_ENTRIES];  // Points either to pages or to other PTs
} TreePageTable, *PTreePageTable;


typedef struct _FlushInfo_t {
    PMAPVIEW  pview;
    // this structure is only used for atomic map
    union {
        // structure for gathered flush
        struct {
            FILE_SEGMENT_ELEMENT* prgSegments;      // Array of segments, one per page
            ULARGE_INTEGER*       prgliOffsets;     // Array of page offsets in file, one per segment
        };

        // for non-gathered atomic flush
        struct {
            LPDWORD               pdwOffsets;       // Array of dword offset
        };
    };
} FlushInfo_t;


typedef struct _PAGETREE {
    
    ULARGE_INTEGER liSize;      // size of the page tree (MUST BE 8 byte aligned)

    TreePTEntry RootEntry;
    
    // Height is the number of page table levels above a page.
    //      Height = 0 : Points straight at the page    (4KB or below)
    //      Height = 1 : Only one PT                    (4MB or below)
    //      Height = 2 : One top PT + 2nd level of PTs  (4GB or below)
    //      Hieght = 3 : One top PT + 3nd level of PTs  (4TB or below)
    WORD wHeight;
    WORD wFlags;                // PGT_*, below

    DWORD dwCacheableFileId;    // non-zero if file is cached, zero if uncached.
    LONG  lRefCnt;              // refcount of the page tree
    DWORD cDirty;               // # of dirty pages
    DWORD cCommitted;           // # of pages committed in the tree
    LONG  cViews;               // # of views of the page tree
    const FSCacheExports *pFsFuncs;   // functions to call for uncached read/write/setsize
    DWORD cbPrefetch;           // prefetch size, must be a power of 2 and <= 64k
    FlushInfo_t flush;          // flush information (used only for atomic mapping)

    PAGEOUTOBJ  pageoutobj;     // Pageable object (used only for cacheable file)
    
    // This critical section should be used whenever accessing the root or
    // anything underneath it.
    CRITICAL_SECTION cs;
    CRITICAL_SECTION *pcsFlush;
    
} PAGETREE, *PPAGETREE;

// Page tree flags are defined in pkfuncs.h as it needs to be shared between kernel and cachefilt

// page tree constants
#define PGT_PT_PHYS_ADDR_MASK           0xFFFFF000
#define PGT_PT_COUNT_MASK               0x7FF           // 11 bits
#define PGT_PT_DIRTY_BIT                0x800

#define PGT_MAX_COUNT                   PGT_PT_COUNT_MASK
#define PGT_MAX_VIEWS                   (PGT_MAX_COUNT - 0xff) 


#define TreeEntry2PhysAddr(tte)         ((tte) & PGT_PT_PHYS_ADDR_MASK)
#define TreeEntry2PFN(tte)              PA2PFN(TreeEntry2PhysAddr(tte))
#define TreeEntry2Count(tte)            ((tte) & PGT_PT_COUNT_MASK)
#define TreeEntry2PTBL(tte)             ((PTreePageTable) TreeEntry2PhysAddr(tte))
#define TreeMakeReservedEntry()         ((TreePTEntry) 0)
#define TreeEntryDirty(tte)             ((tte) & PGT_PT_DIRTY_BIT)
#define TreeEntryDecCount(ptte)         ((*(ptte))--)
#define TreeEntryIncCount(ptte)         ((*(ptte))++)
#define TreeMakeCommittedEntry(phys,cnt) ((TreePTEntry) (phys) | (cnt))

#define LockPgTree(ppgtree)             EnterCriticalSection (&(ppgtree)->cs)
#define UnlockPgTree(ppgtree)           LeaveCriticalSection (&(ppgtree)->cs)
#define PageTreeUsePool(ppgtree)        ((ppgtree)->wFlags & PGT_USEPOOL)

DWORD TableIndexAtHeight (const ULARGE_INTEGER *pliOfst, DWORD dwHeight);
PTreePTEntry PgtreeNextEntry (PPAGETREE ppgtree, PTreePTEntry ptte);
PTreePTEntry PgTreeGetEntry (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, PTreePTEntry *pptte);

extern LONG g_cTotalDirty, g_cTotalCommit;

FORCEINLINE BOOL TreeEntryIsCommitted (TreePTEntry tte) 
{
    return 0 != tte;
}


#ifdef IN_KERNEL
FORCEINLINE BOOL OwnPgTreeLock (PPAGETREE ppgtree)
{
    return ppgtree && OwnCS (&ppgtree->cs);
}

FORCEINLINE void PgtreeSetDirty (PPAGETREE ppgtree, PTreePTEntry ptte)
{
    if (!TreeEntryDirty (*ptte)) {
        ppgtree->cDirty ++;
        *ptte |= PGT_PT_DIRTY_BIT;
        InterlockedIncrement (&g_cTotalDirty);
    }
}

FORCEINLINE void PgtreeClearDirty (PPAGETREE ppgtree, PTreePTEntry ptte)
{
    if (TreeEntryDirty (*ptte)) {
        ppgtree->cDirty --;
        *ptte &= ~PGT_PT_DIRTY_BIT;
        InterlockedDecrement (&g_cTotalDirty);
    }
}

FORCEINLINE void PgtreeCommitPage (PPAGETREE ppgtree, PTreePTEntry ptte, PTreePTEntry ptteParent, DWORD dwPFN)
{
    DEBUGCHK (!TreeEntryIsCommitted (*ptte));
    
    *ptte = TreeMakeCommittedEntry (PFN2PA (dwPFN), 0);
    if (ptteParent) {
        TreeEntryIncCount (ptteParent);
    } else {
        DEBUGCHK (&ppgtree->RootEntry == ptte);
    }
    ppgtree->cCommitted ++;
    InterlockedIncrement (&g_cTotalCommit);
}
#endif

typedef BOOL (* PFN_PageTreeEnumFunc) (PPAGETREE ppgtree, PTreePTEntry ptte, PTreePTEntry ptteParent, const ULARGE_INTEGER *pliOfst, LPVOID pData);

//
// walk the page tree and apply a function to leave nodes.
// NOTE: only walk existing nodes. No side effect to tree tables.
//
BOOL PageTreeWalk (PPAGETREE ppgtree, 
                   const ULARGE_INTEGER *pliOfstStart,
                   const ULARGE_INTEGER *piliOfstEnd,
                   DWORD dwFlags,                   // flags below
                   PFN_PageTreeEnumFunc pfnApply,
                   LPVOID pData);

// flags to pass to PageTreeWalk
#define PTW_DECOMMIT_TABLES         0x0001          // decommit tables if no "child page" exist

//
// push the dirty bit of the view to page tree. decommit if requested
//
BOOL PageTreeViewFlush (PPAGETREE ppgtree, 
                        PPROCESS pprc, 
                        const BYTE *pStartAddr, 
                        const ULARGE_INTEGER *pliOfst, 
                        DWORD cbSize, 
                        DWORD dwFlags);

#define VFF_DECOMMIT    0x0001      // decommit the view pages
#define VFF_WRITEBACK   0x0002      // writeback
#define VFF_RELEASE     0x0004      // special flag for atomic mapping. Release the view

//
// fill the page tree with memory contents (used in non-pageable mapfile)
//
BOOL PageTreeFill (PPAGETREE ppgtree, LPCVOID pAddr, DWORD cbSize);

//
// create a page tree
//
PPAGETREE PageTreeCreate (DWORD dwCacheableFileId, const ULARGE_INTEGER * pliSize, const FSCacheExports *pFsFuncs, DWORD cbPrefetch, DWORD dwFlags);

//
// Open an existing page tree
//
BOOL PageTreeOpen (PPAGETREE ppgtree);

//
// close a page tree (dec-ref and destroy if ref == 0)
//
void PageTreeClose (PPAGETREE ppgtree);

//
// map existing pages in page tree to VM. returns the number of pages mapped
//
DWORD PageTreeMapRange (PPAGETREE ppgtree,
                        PPROCESS  pprc,
                        LPCVOID   pAddr, 
                        const ULARGE_INTEGER *pliOfst,
                        DWORD     cPages,
                        DWORD     fProtect);

//
// allocate flush buffer for Atomic map
//
BOOL PageTreeAllocateFlushBuffer (PPAGETREE ppgtree, PMAPVIEW pview);


//
// free flush buffer for atomic mapping
//
void PageTreeFreeFlushBuffer (PPAGETREE ppgtree);

//
// reading from file to page tree (used for Paging and PageTreeRead)
//
DWORD PageTreeFetchPages (PPAGETREE ppgtree, PHDATA phdFile, const ULARGE_INTEGER *pliOfst, LPBYTE pReadVM);

//
// flushing
//
BOOL PageTreeFlush (PPAGETREE ppgtree, PHDATA phdFile, const ULARGE_INTEGER *pliOfst, DWORD cbSize, PBOOL pfCancelFlush, LPBYTE pReuseFlushVM);
BOOL PageTreeAtomicFlush (PPAGETREE ppgtree, DWORD dwFlags);

//
//
//
BOOL PageTreeSetSize (PPAGETREE ppgtree, const ULARGE_INTEGER *pliSize, BOOL fDeleteFile);
BOOL PageTreeGetSize (PPAGETREE ppggree, ULARGE_INTEGER *pliSize);
BOOL PageTreeRead (PPAGETREE ppgtree, const ULARGE_INTEGER * pliOfst, LPBYTE pBuffer, DWORD cbSize, LPDWORD pcbRead);
BOOL PageTreeWrite (PPAGETREE ppgtree, const ULARGE_INTEGER * pliOfst, const BYTE * pData, DWORD cbSize, LPDWORD pcbWritten);
BOOL PageTreeIsDirty (PPAGETREE ppgtree);
PPAGETREE GetPageTree (PHDATA phdFile);


//
// flush lock design:
//
// the usage of flush lock is to guard:
// - flush and size changes for regular map
// - flush/unmap/write/size for atomic map
//
// We can easily implement per-tree flush lock. However, flush operations eventually get down to the hardware and
// will be serialized at the hardware level. As a result, we won't get concurrency as we expected.
//
// for now, we'll just use 1 single flush lock for everything. If it turns out to be a bottleneck, we can consider:
//  1) another flush lock for atomic maps
//  2) a flush lock per atomic map, a single lock for regular maps
//  3) a flush lock per map
//
#ifdef IN_KERNEL

FORCEINLINE void AcquireFlushLock (PPAGETREE ppgtree)
{
    EnterCriticalSection (ppgtree->pcsFlush);
}

FORCEINLINE BOOL TryAcquireFlushLock (PPAGETREE ppgtree)
{
    return TryEnterCriticalSection (ppgtree->pcsFlush);
}

FORCEINLINE void ReleaseFlushLock (PPAGETREE ppgtree)
{
    LeaveCriticalSection (ppgtree->pcsFlush);
}

FORCEINLINE BOOL OwnFlushLock (PPAGETREE ppgtree)
{
    return OwnCS (ppgtree->pcsFlush);
}

#endif

//
// page out
//
BOOL PageTreePageOut (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, DWORD cbSize, DWORD dwFlags);


//
// ref-counting
//
FORCEINLINE void PageTreeIncRef (PPAGETREE ppgtree)
{
    InterlockedIncrement (&ppgtree->lRefCnt);
}
void PageTreeDecRef (PPAGETREE ppgtree);

// view counting
BOOL PageTreeIncView (PPAGETREE ppgtree);
void PageTreeDecView (PPAGETREE ppgtree);


#endif  // _PAGE_TREE_H_

