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

#include "kernel.h"
#include "pgtree.h"
#include "pager.h"
#include <diskio.h>
#include <fsioctl.h>

ERRFALSE(sizeof(PAGETREE) <= HEAP_SIZE3);


// max size < 256MB (filesys limited it to 16MB) or # of pages can overflow
#define MAX_ATOMIC_FILE_SIZE    (0x10000 << VM_PAGE_SHIFT)

#define MAX_DIRTY_THRESHOLD     0x4000      // 64M of dirty pages system wide
#define MAX_DIRTY_PER_FILE      0x400       // 4M of dirty pages per file

#define MAX_COMMIT_THRESHOLD    0x4000      // 64M of cache pages system wide
#define MAX_COMMIT_PER_FILE     0x800       // 8M of cache pages per file

LONG g_cTotalDirty;
LONG g_cTotalCommit;

//
// UncachedRead - wrapper to read from the underlying file system
//
BOOL UncachedRead (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOffset, BYTE *pBuffer, DWORD cbSize, LPDWORD pcbRead)
{
    BOOL fRet;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    fRet = ppgtree->pFsFuncs->pUncachedRead (ppgtree->dwCacheableFileId, pliOffset, pBuffer, cbSize, pcbRead);
    SwitchActiveProcess (pprc);
    return fRet;
}

//
// UncachedWrite - wrapper to write to the underlying file system
//
BOOL UncachedWrite (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOffset, const BYTE *pData, DWORD cbSize)
{
    BOOL fRet;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    fRet = ppgtree->pFsFuncs->pUncachedWrite (ppgtree->dwCacheableFileId, pliOffset, pData, cbSize, NULL);
    SwitchActiveProcess (pprc);
    return fRet;
}

//
// UncachedSetSize - wrapper to change the size of a file
//
BOOL UncachedSetSize (PPAGETREE ppgtree, const ULARGE_INTEGER *pliSize)
{
    BOOL fRet;
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    fRet = ppgtree->pFsFuncs->pUncachedSetSize (ppgtree->dwCacheableFileId, pliSize);
    SwitchActiveProcess (pprc);
    return fRet;
}

static void PageTreeDestroy (PPAGETREE ppgtree);

const ULONGLONG TreeSizeAtHeight[] = {
    (ULONGLONG) 1 << VM_PAGE_SHIFT,                 // 4K
    (ULONGLONG) 1 << (VM_PAGE_SHIFT + 10),          // 4M
    (ULONGLONG) 1 << (VM_PAGE_SHIFT + 20),          // 4G
    (ULONGLONG) 1 << (VM_PAGE_SHIFT + 30),          // 4T

#ifdef SUPPORT_HUGEFILE
    (ULONGLONG) 1 << (VM_PAGE_SHIFT + 40),          // too big
    (ULONGLONG) 1 << (VM_PAGE_SHIFT + 50),          // way too big
#endif
};

const BYTE TreeShiftAtHeight[] = {
    VM_PAGE_SHIFT,                      // 4K
    (VM_PAGE_SHIFT + 10),               // 4M
    (VM_PAGE_SHIFT + 20),               // 4G
    (VM_PAGE_SHIFT + 30),               // 4T

#ifdef SUPPORT_HUGEFILE
    (VM_PAGE_SHIFT + 40),               // too big
    (VM_PAGE_SHIFT + 50),               // way too big
#endif
};

// For debugging purposes
struct {
    DWORD NumDataPages;
    DWORD NumPageTables;
} g_TreeStatistics;

#define MAX_TREE_HEIGHT                 (_countof(TreeSizeAtHeight) - 1)
#define MAX_TREE_SIZE                   (TreeSizeAtHeight[MAX_TREE_HEIGHT])

extern CRITICAL_SECTION FileFlushCS, DiskFlushCS;

#define PAGETREE_FILE_COUNT_INCR        0x10000
#define PAGETREE_LOCK_COUNT_INCR        1

//
// TableIndexAtHeigth - return the index of the tree table at a given height.
//
DWORD TableIndexAtHeight (const ULARGE_INTEGER *pliOfst, DWORD dwHeight)
{
    DEBUGCHK (dwHeight);    // no index per-se at height 0

    return (DWORD) ((pliOfst->QuadPart >> TreeShiftAtHeight[dwHeight-1]) & (TREE_NUM_PT_ENTRIES - 1));
}

//
// GetTreeHeight - given size, calculate the required height of page tree
//
WORD GetTreeHeight (ULONGLONG ul64Size)
{
    BYTE height;

    for (height = 0; height < MAX_TREE_HEIGHT; height ++) {
        if (ul64Size <= TreeSizeAtHeight [height]) {
            break;
        }
    }
    return height;
}

//
// AdjustSize - adjust size such that offset+size doesn't exceed the size of a page tree
//
DWORD AdjustSize (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, DWORD cbSize)
{
    if (ppgtree->liSize.QuadPart <= pliOfst->QuadPart) {
        cbSize = 0;
    } else if (pliOfst->QuadPart + cbSize > ppgtree->liSize.QuadPart) {
        cbSize = (DWORD) (ppgtree->liSize.QuadPart - pliOfst->QuadPart);
    }
    return cbSize;
}

//
// TreeAllocPT - allocate a tree table
//
FORCEINLINE PTreePageTable TreeAllocPT (void)
{
    PTreePageTable pTable = GrabOnePage (PM_PT_ZEROED);
    if (pTable) {
        InterlockedIncrement ((LONG*)&g_TreeStatistics.NumPageTables);
    }
    return pTable;
}

//
// TreeFreePT - free a tree table
//
FORCEINLINE void TreeFreePT (PTreePageTable pTable)
{
    DEBUGCHK (pTable);
    FreeKernelPage (pTable);    // Page refcnt should be 1
    InterlockedDecrement ((LONG*)&g_TreeStatistics.NumPageTables);
}

#define MAX_FLUSH_SIZE      0x40000000

//
// CalcFlushSize - calculate the size for flushing next segment. 
// NOTE: FlushNextSegment only take a DWORD as size. We need to break down calls to FlushNextSegment to sizes that fits in a DWORD
//
FORCEINLINE DWORD CalcFlushSize (const ULARGE_INTEGER *pliEnd, const ULARGE_INTEGER *pliStart)
{
    DEBUGCHK (pliEnd->QuadPart >= pliStart->QuadPart);
    
    return ((pliEnd->QuadPart - pliStart->QuadPart) < MAX_FLUSH_SIZE)
         ? ((DWORD) (pliEnd->QuadPart - pliStart->QuadPart))
         : MAX_FLUSH_SIZE;
}


//
// PgtreeNextEntry - get the next tree table entry. return NULL if reached the end of the tree table.
//
PTreePTEntry PgtreeNextEntry (PPAGETREE ppgtree, PTreePTEntry ptte)
{
    if (!ptte || (&ppgtree->RootEntry == ptte)) {
        ptte = NULL;
    } else {
        ptte ++;
        
        if (IsPageAligned (ptte)) {
            // cross page, must get it again next time
            ptte = NULL;
        }
    }
    return ptte;
}

//
// PageTreeWalk - walk the page tree, and calls a callback function for all the leaf node.
//
BOOL PageTreeWalk (PPAGETREE ppgtree, 
                   const ULARGE_INTEGER *pliOfstStart,
                   const ULARGE_INTEGER *piliOfstEnd,
                   DWORD dwFlags,
                   PFN_PageTreeEnumFunc pfnApply,
                   LPVOID pData)
{
    BOOL        fRet        = FALSE;
    DWORD       dwHeight    = ppgtree->wHeight;
    ULONGLONG   ul64Max     = TreeSizeAtHeight [dwHeight];
    ULONGLONG   ul64End     = piliOfstEnd? piliOfstEnd->QuadPart : ul64Max;
    ULARGE_INTEGER liWalk;
    PTreePTEntry ptte       = &ppgtree->RootEntry;
    PTreePTEntry ptteAtHeight[MAX_TREE_HEIGHT];

    liWalk.QuadPart = pliOfstStart? pliOfstStart->QuadPart : 0;

    DEBUGCHK (IsPageAligned (liWalk.LowPart));
    DEBUGCHK (OwnPgTreeLock (ppgtree));

    if (ul64End > ul64Max) {
        ul64End = ul64Max;
    }

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeWalk: 0x%8.8lx 0x%016I64x 0x%016I64x 0x%8.8lx\r\n", ppgtree, liWalk.QuadPart, ul64End, dwFlags));

    while (liWalk.QuadPart < ul64End) {

        for ( ; dwHeight; dwHeight --) {
            // depth-first walk
            TreePageTable *ptbl;
            
            PREFAST_DEBUGCHK (ptte);
            ptbl = TreeEntry2PTBL (*ptte);
            
            if (!ptbl) {
                break;
            }

            // remember the "parent node" before walk deeper
            ptteAtHeight[dwHeight-1] = ptte;
            ptte = &ptbl->tte[TableIndexAtHeight (&liWalk, dwHeight)];
        }

        if (!dwHeight) {
            // apply functin if we reach leaf node
            fRet = pfnApply (ppgtree, ptte, ppgtree->wHeight? ptteAtHeight[0] : NULL, &liWalk, pData);
            if (fRet) {
                break;
            }
        }

        // move on to the next entry
        ptte = PgtreeNextEntry (ppgtree, ptte);

        // move up the tree if we reach end of table
        while (!ptte && (dwHeight < ppgtree->wHeight)) {
            ptte = ptteAtHeight[dwHeight];
            dwHeight ++ ;
            
            if (PTW_DECOMMIT_TABLES & dwFlags) {
                // decommit empty tables if requested
                TreePTEntry tte = *ptte;
                if (!TreeEntry2Count (tte)) {
                    // empty table, decommit the table and decrement that count of parent entry
                    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeWalk: decommitting tree table @0x%8.8lx\r\n", TreeEntry2PTBL (tte)));

                    // free the tree table
                    *ptte = TreeMakeReservedEntry ();
                    TreeFreePT (TreeEntry2PTBL (tte));

                    // decrement the ref count of parent entry
                    if (dwHeight < ppgtree->wHeight) {
                        TreeEntryDecCount (ptteAtHeight[dwHeight]); 
                    } else {
                        // the tree is fully empty now
                        DEBUGCHK (&ppgtree->RootEntry == ptte);
                        ppgtree->wHeight = 0;
                        ul64End = 0;
                    }
                }
            }
            
            ptte = PgtreeNextEntry (ppgtree, ptte);
        }

        // move to next "page" in terms of height
        liWalk.QuadPart = (liWalk.QuadPart + TreeSizeAtHeight [dwHeight]) & (0-TreeSizeAtHeight [dwHeight]);
        
    }

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeWalk: returning %d\r\n", fRet));
    return fRet;
}

//
// PgtreeDecommitPage - decommit a page from the page tree. This is the only function that takes a page out of a page tree.
//
void PgtreeDecommitPage (PPAGETREE ppgtree, PTreePTEntry ptte, PTreePTEntry ptteParent)
{
    DWORD dwPFN = TreeEntry2PFN (*ptte);
    
    DEBUGMSG (ZONE_MAPFILE, (L"PgtreeDecommitPage: decommitting tree page, physical address = @0x%8.8lx\r\n", dwPFN));
    DEBUGCHK (TreeEntryIsCommitted (*ptte));
    DEBUGCHK (!TreeEntry2Count (*ptte));

    // clear the page tree entry
    *ptte = TreeMakeReservedEntry ();

    // update parent ref-count
    if (ptteParent) {
        TreeEntryDecCount (ptteParent);
    } else {
        DEBUGCHK (&ppgtree->RootEntry == ptte);
    }

    // decrement the amount of committed pages
    ppgtree->cCommitted --;

    // free the page back to paging pool or regular memory
    if (ppgtree->wFlags & PGT_USEPOOL) {
        PGPOOLFreePage (&g_PagingPool, dwPFN, NULL);
    } else {
        FreePhysPage (dwPFN);
    }

    // decrement the total committed pages
    InterlockedDecrement (&g_cTotalCommit);
}


//
// PgTreeGrowHeight - grow the height of a page tree. Allocate tree table as needed
//
BOOL PgTreeGrowHeight (PPAGETREE ppgtree, DWORD dwHeight)
{
    // do not grow height beyond file size
    if (dwHeight <= GetTreeHeight (ppgtree->liSize.QuadPart)) {

        DEBUGMSG (ZONE_MAPFILE, (L"PgTreeGrowHeight: growing page tree height from %d to %d\r\n", ppgtree->wHeight, dwHeight));
        if (ppgtree->RootEntry) {

            // non-empty tree
            while (dwHeight > ppgtree->wHeight) {
                TreePageTable *ptbl = TreeAllocPT ();
                if (!ptbl) {
                    break;
                }
                // The first entry in the new node will be the old root
                ptbl->tte[0] = ppgtree->RootEntry;
                ppgtree->RootEntry = TreeMakeCommittedEntry (ptbl, 1);  // count=1 for existing entry
                ppgtree->wHeight ++;
            }
            
        } else {
        
            // empty tree ==> any height
            if (ppgtree->wHeight < dwHeight) {
                ppgtree->wHeight = (WORD) dwHeight;
            }
            
        }
    }
    return (dwHeight <= ppgtree->wHeight);
}

//
// Get a page tree table entry of a give offset
//  - return existing entry if ppParentEntry is NULL
//  - create tree table as needed if ppParentEntry is not NULL
//  - never create table beyond file size
//
PTreePTEntry PgTreeGetEntry (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, PTreePTEntry *pptteParent)
{
    DWORD           dwHeight = GetTreeHeight (pliOfst->QuadPart + 1);
    PTreePTEntry    ptte = NULL, ptteParent = NULL;

    DEBUGMSG (ZONE_MAPFILE, (L"PgTreeGetEntry: 0x%8.8lx 0x%016I64x 0x%8.8lx\r\n", ppgtree, pliOfst->QuadPart, pptteParent));

    if (pptteParent) {
        PgTreeGrowHeight (ppgtree, dwHeight);
    }

    if (dwHeight <= ppgtree->wHeight) {

        ptte = &ppgtree->RootEntry;

        for (dwHeight = ppgtree->wHeight; dwHeight > 0; dwHeight --) {
            TreePageTable *ptbl = TreeEntry2PTBL (*ptte);
            if (!ptbl) {
                // allocate page table if within file size
                if (pptteParent && (pliOfst->QuadPart < ppgtree->liSize.QuadPart)) {
                    ptbl = TreeAllocPT ();
                }
                if (!ptbl) {
                    ptte = NULL;
                    break;
                }

                // update the entry
                *ptte = TreeMakeCommittedEntry (ptbl, 0);

                // update parent entry to include the new table created
                if (ptteParent) {
                    TreeEntryIncCount (ptteParent);
                }
            }
            ptteParent = ptte;
            ptte  = &ptbl->tte[TableIndexAtHeight (pliOfst, dwHeight)];
        }
    }

    if (ptte && pptteParent) {
        *pptteParent = ptteParent;
    }

    DEBUGMSG (ZONE_MAPFILE, (L"PgTreeGetEntry: returns 0x%8.8lx\r\n", ptte));
    return ptte;
}


typedef struct {
    ULARGE_INTEGER liOfst;
    PTreePTEntry   ptte;
} FINDDIRTYSTRUCT, *PFINDDIRTYSTRUCT;

//
// EnumFindDirtyEntry - tree enumeration to find dirty pages
//
BOOL EnumFindDirtyEntry (PPAGETREE ppgtree, PTreePTEntry ptte, PTreePTEntry ptteParent, const ULARGE_INTEGER *pliOfst, LPVOID pData)
{
    BOOL fRet = TreeEntryDirty (*ptte);

    if (fRet) {
        PFINDDIRTYSTRUCT pfd = (PFINDDIRTYSTRUCT) pData;
        pfd->liOfst.QuadPart = pliOfst->QuadPart;
        pfd->ptte            = ptte;
    }
    
    return fRet; 
}

// 
// PgtreeGetFirstDirtyEntry - find the 1st dirty page within a give range
//
PTreePTEntry PgtreeGetFirstDirtyEntry (PPAGETREE ppgtree, ULARGE_INTEGER *pliOfst, DWORD cbSize)
{
    FINDDIRTYSTRUCT fd;
    ULARGE_INTEGER  liEnd;

    DEBUGCHK (cbSize && (pliOfst->QuadPart < ppgtree->liSize.QuadPart));
    
    liEnd.QuadPart = (pliOfst->QuadPart + cbSize);

    DEBUGCHK (liEnd.QuadPart <= ppgtree->liSize.QuadPart);

    fd.ptte = NULL;
    
    if (PageTreeWalk (ppgtree, pliOfst, &liEnd, 0, EnumFindDirtyEntry, &fd)) {
        // update the offset
        pliOfst->QuadPart = fd.liOfst.QuadPart;
    }

    DEBUGMSG (ZONE_MAPFILE, (L"PgtreeGetFirstDirtyEntry: returns 0x%8.8lx\r\n", fd.ptte));
    return fd.ptte;
}

const DWORD prefetchSizes [] = {
    1 << VM_PAGE_SHIFT,             // 4k
    1 << (VM_PAGE_SHIFT+1),         // 8k
    1 << (VM_PAGE_SHIFT+2),         // 16k
    1 << (VM_PAGE_SHIFT+3),         // 32k
    1 << (VM_PAGE_SHIFT+4),         // 64k
};
// NOTE: Maximum prefetch size must be 64K or the prefetch logic won't work.
//
// unfortunately, the following C_ASSERT doesn't work...
// C_ASSERT (VM_BLOCK_SIZE == prefetchSizes[_countof (prefetchSizes) - 1]);

static DWORD CalcPrefetchSize (DWORD cbPrefetch)
{
    int idx;
    for (idx = _countof (prefetchSizes) - 1; idx > 0; idx --) {
        if (cbPrefetch >= prefetchSizes[idx]) {
            break;
        }
    }
    return prefetchSizes[idx];
}

//
// PageTreeCreate - create a page tree
//
PPAGETREE PageTreeCreate (DWORD dwCacheableFileId, const ULARGE_INTEGER *pliSize, const FSCacheExports *pFsFuncs, DWORD cbPrefetch, DWORD dwFlags)
{
    PPAGETREE ppgtree = NULL;

    // like to use C_ASSERT for the following for compile time failure, but C_ASSERT doesn't work for constant array...)
    DEBUGCHK (VM_PAGE_SIZE == prefetchSizes[0]);
    DEBUGCHK (VM_BLOCK_SIZE == prefetchSizes[_countof (prefetchSizes) - 1]);
    
    // either not cacheable or it must have the set of uncached read/write/setsize function
    DEBUGCHK (!dwCacheableFileId || pFsFuncs);

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeCreate: 0x%8.8lx 0x%016I64x 0x%8.8lx\r\n", dwCacheableFileId, pliSize->QuadPart, dwFlags));
    
    if ((pliSize->QuadPart <= MAX_TREE_SIZE)) {
        ppgtree = AllocMem (HEAP_PGTREE);

        if (ppgtree) {
            
            memset (ppgtree, 0, sizeof (PAGETREE));
            ppgtree->dwCacheableFileId = dwCacheableFileId;
            ppgtree->lRefCnt  = PAGETREE_FILE_COUNT_INCR;
            ppgtree->liSize   = *pliSize;
            ppgtree->wFlags   = (WORD) dwFlags;
            ppgtree->pFsFuncs = pFsFuncs;
            ppgtree->cbPrefetch = CalcPrefetchSize (cbPrefetch);
            ppgtree->pcsFlush = (dwFlags & PGT_DISKCACHE)
                              ? &DiskFlushCS
                              : &FileFlushCS;
            InitDList (&ppgtree->pageoutobj.link);
            if (dwCacheableFileId) {
                ppgtree->pageoutobj.eType = ePageTree;
                ppgtree->pageoutobj.pObj  = ppgtree;

                DEBUGCHK (!(dwFlags & MAP_ATOMIC));
                ppgtree->wFlags |= PGT_USEPOOL;
            } else {
                ppgtree->pageoutobj.eType = eInvalid;
            }

            InitializeCriticalSection (&ppgtree->cs);
            
            if (!ppgtree->cs.hCrit) {
                FreeMem (ppgtree, HEAP_PGTREE);
                ppgtree = NULL;
            }
        }
    }

    return ppgtree;
}


//
// PageTreeOpen - 'open' a page tree == increment the file count of a page tree
//
BOOL PageTreeOpen (PPAGETREE ppgtree)
{
    DEBUGCHK (ppgtree->lRefCnt >= PAGETREE_FILE_COUNT_INCR);
    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeOpen: 0x%8.8lx\r\n", ppgtree));
    InterlockedExchangeAdd (&ppgtree->lRefCnt, PAGETREE_FILE_COUNT_INCR);
    return TRUE;
}

//
// PageTreeClose - close a page tree.
//
void PageTreeClose (PPAGETREE ppgtree)
{
    LONG lRefCnt;

    // increment ref-count of page tree so it doesn't get destroyed while we're cleaning up
    PageTreeIncRef (ppgtree);

    // remove a 'file count' from the page tree    
    lRefCnt = InterlockedExchangeAdd (&ppgtree->lRefCnt, -PAGETREE_FILE_COUNT_INCR) - PAGETREE_FILE_COUNT_INCR;
    
    DEBUGCHK (lRefCnt > 0);
    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeClose: 0x%8.8lx, lRefCnt = %d\r\n", ppgtree, lRefCnt));

    // clean up if there are no more file for the page tree
    if (lRefCnt < PAGETREE_FILE_COUNT_INCR) {

        // remove ourself from the page out list.
        PagePoolRemoveObject (&ppgtree->pageoutobj, TRUE);

        // for non-cacheable files, flush already been done in MAPPreClose. We only need to deal 
        // with cacheable file here
        if (ppgtree->dwCacheableFileId) {
            // there cannot be any view for this page tree here as this is always going to be
            // the last ref to the file coming from file cache. all mapfiles to the file must 
            // have been closed.
            DEBUGCHK (!ppgtree->cViews);

            // ensure async flush completed by acquiring flush lock
            AcquireFlushLock (ppgtree);
            DEBUGCHK (!ppgtree->cDirty);
            ReleaseFlushLock (ppgtree);
        }
    }

    // decrement ref count and destroy tree if ref-count get to 0
    PageTreeDecRef (ppgtree);
}

void PageTreeDecRef (PPAGETREE ppgtree)
{
    if (!InterlockedDecrement (&ppgtree->lRefCnt)) {
        PageTreeDestroy (ppgtree);
    }
}


#define PAGE_IDX_IN_BLOCK(addr)     (((addr) >> VM_PAGE_SHIFT) & 0xf)

//
// PageTreeFlushNextSegment - worker function to flush page tree. Flush the next segment of consecutive dirty pages up to 64k.
//
// NOTE: pliOffset is an in/out parameter.
//
BOOL PageTreeFlushNextSegment (PPAGETREE ppgtree, PHDATA phdFile, ULARGE_INTEGER *pliOfst, DWORD cbSize, LPBYTE pFlushVM)
{
    PTreePTEntry ptte    = NULL;
    DWORD       cPages   = 0;
    BOOL        fFlushed = TRUE;
    DWORD       idx, cbToFlush;

    DEBUGCHK (IsPageAligned (pliOfst->LowPart));
    DEBUGCHK (!(ppgtree->wFlags & MAP_ATOMIC));

    PREFAST_DEBUGCHK (cbSize <= MAX_FLUSH_SIZE);

    // we need to acquire flush lock for cacheable files such that the file size won't chnage underneath us.
    AcquireFlushLock (ppgtree);

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeFlushNextSegment: 0x%8.8lx 0x%8.8lx 0x%016I64x 0x%8.8lx 0x%8.8lx\r\n", 
                            ppgtree, phdFile, pliOfst->QuadPart, cbSize, pFlushVM));

    LockPgTree (ppgtree);

    cbToFlush = AdjustSize (ppgtree, pliOfst, cbSize);

    if (cbToFlush) {
        
        ptte = PgtreeGetFirstDirtyEntry (ppgtree, pliOfst, cbToFlush);     // NOTE: pliOffset is an in/out parameter.
        
        if (ptte) {

            PTreePTEntry pWalk = ptte;
            
            // get all consecutive dirty pages within the same 64K block. We can flush more pages than requested if there are more dirty pages
            // consecutively in the same 64k block. But that's okay (and better) as we'll save on the number of calls to actual IO.
            for (idx = PAGE_IDX_IN_BLOCK (pliOfst->LowPart); pWalk && (idx < VM_PAGES_PER_BLOCK) && TreeEntryDirty (*pWalk); idx ++) {
                
                DEBUGCHK (ppgtree->cDirty);
                TreeEntryIncCount (pWalk);              // 'lock' the tree entry by increment ref-count
                PgtreeClearDirty (ppgtree, pWalk);      // clear dirty bit

                VERIFY (VMCreateKernelPageMapping (pFlushVM + (idx << VM_PAGE_SHIFT), TreeEntry2PFN (*pWalk)));
                cPages ++;
                pWalk   = PgtreeNextEntry (ppgtree, pWalk);
                
            }
        }
    }

    UnlockPgTree (ppgtree);

    if (!ptte) {
        // nothing to flush in this range
        pliOfst->QuadPart += cbSize;
        
    } else {
        // in case file size is not multiple of pages, we don't want to write the full 4k of the last page
        DWORD cbToWrite = AdjustSize (ppgtree, pliOfst, cPages << VM_PAGE_SHIFT);
        DWORD cbWritten;

        pFlushVM += (pliOfst->QuadPart & VM_BLOCK_OFST_MASK);

        CELOG_MapFileViewFlush (g_pprcNK, ppgtree, pFlushVM, cbToWrite, cPages, TRUE);

        // write to file
        if (ppgtree->dwCacheableFileId) {
            DEBUGCHK (ppgtree->pFsFuncs);
            fFlushed = UncachedWrite (ppgtree,
                                pliOfst,
                                pFlushVM,
                                cbToWrite);
        } else if (ppgtree->wFlags & PGT_USEPOOL) {
            fFlushed = PHD_WriteFileWithSeek (phdFile,
                                pFlushVM,
                                cbToWrite,
                                &cbWritten,
                                NULL,
                                pliOfst->LowPart,
                                pliOfst->HighPart);
        } else {
            // non-pageable
            fFlushed = PHD_SetLargeFilePointer (phdFile, *pliOfst)
                       && PHD_WriteFile (phdFile, pFlushVM, cbToWrite, &cbWritten, NULL)
                       && (cbWritten == cbToWrite);
        }

        CELOG_MapFileViewFlush (g_pprcNK, ppgtree, pFlushVM, cbToWrite,
                            PAGECOUNT(cbToWrite), FALSE);

        
        // 'unlocked' the pages we locked before and restore dirty bit if flush failed.
        // Also unalis pFlushVM and the physical page in the page tree
        LockPgTree (ppgtree);

        // need to call PgTreeGetEntry again here, as we might get the root entry before and the tree
        // grown while we're flushing. 
        ptte = PgTreeGetEntry (ppgtree, pliOfst, NULL);

        DEBUGCHK (ptte);

        // restore dirty bit if failed, and 'unlock' the page tree entries
        for (idx = 0; idx < cPages; idx ++) {
            DEBUGCHK (TreeEntryIsCommitted (*ptte));

            VMRemoveKernelPageMapping (pFlushVM + (idx << VM_PAGE_SHIFT));   // remove the temp mapping we created
            
            TreeEntryDecCount (ptte);        // 'unlock' the entry
            
            if (!fFlushed) {
                // failed, mark the page dirty again
                PgtreeSetDirty (ppgtree, ptte);
            }

            ptte ++;
        }

        UnlockPgTree (ppgtree);

        InvalidatePages (g_pprcNK, (DWORD) pFlushVM, cPages);

        // update pliOfst (prefast complain about arith overflow if I don't break the following into 2 statements...)
        cbSize = (cPages << VM_PAGE_SHIFT);
        pliOfst->QuadPart += cbSize;

    }
        
    ReleaseFlushLock (ppgtree);

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeFlushNextSegment: returns %d\r\n", fFlushed));
    return fFlushed;
}

typedef struct {
    DWORD dwAddr;
    DWORD cPages;
} FlushCollectStruct, *PFlushCollectStruct;

//
// EnumCollectFlushInfo - (Atomic only) enumeration function to collect the dirty pages information to be used in gathered write.
//
static BOOL EnumCollectFlushInfo (PPAGETREE ppgtree, PTreePTEntry ptte, PTreePTEntry ptteParent, const ULARGE_INTEGER *pliOfst, LPVOID pEnumData)
{
    PFlushCollectStruct pfcs   = (PFlushCollectStruct) pEnumData;

    DEBUGCHK (MAP_ATOMIC & ppgtree->wFlags);

    if (TreeEntryDirty (*ptte)) {
        FlushInfo_t *pFlush = &ppgtree->flush;
        DWORD           idx = pfcs->cPages;

        // view to dirty page should've never be unmapped
        DEBUGCHK (1 == TreeEntry2Count (*ptte));

        if (MAP_GATHER & ppgtree->wFlags) {
            // setup gather write structure
            pFlush->prgSegments[idx].Alignment = pfcs->dwAddr;
            pFlush->prgliOffsets[idx].QuadPart = pliOfst->QuadPart;

        } else {
            // setup non-gather write structure
            pFlush->pdwOffsets[idx] = pliOfst->LowPart;
        }
        // clear dirty bit
        PgtreeClearDirty (ppgtree, ptte);

        // increment # of pages to flush
        pfcs->cPages ++;
    }
    pfcs->dwAddr += VM_PAGE_SIZE;

    return !ppgtree->cDirty;   // keep walking until no dirty pages
}

//
// CollectFlushInfo - (Atomic only) collect all drity pages information in a page tree
//
static DWORD CollectFlushInfo (PPAGETREE ppgtree)
{
    ULARGE_INTEGER liEnd;
    FlushCollectStruct fcs = {0};
    FlushInfo_t* pFlush = &ppgtree->flush;
    PMAPVIEW      pview = pFlush->pview;

    liEnd.QuadPart = pview->base.liMapOffset.QuadPart + pview->base.cbSize;
    fcs.dwAddr     = (DWORD) pview->base.pBaseAddr;
    
    LockPgTree (ppgtree);

    if (ppgtree->cDirty) {
        PageTreeWalk (ppgtree, &pFlush->pview->base.liMapOffset, &liEnd, 0, EnumCollectFlushInfo, &fcs);
    }

    UnlockPgTree (ppgtree);
    DEBUGMSG (ZONE_MAPFILE, (L"CollectFlushInfo: found %d dirty pages\r\n", fcs.cPages));

    return fcs.cPages;
}

//
// RestoreDirtyPages - (Atomic only) restore dirty bit for dirty pages in case of flush failure
//
static void RestoreDirtyPages (PPAGETREE ppgtree, DWORD cPages)
{
    ULARGE_INTEGER liOfst;
    PTreePTEntry ptte;
    
    LockPgTree (ppgtree);
    
    while (cPages --) {
        liOfst.QuadPart = (ppgtree->wFlags & MAP_GATHER)
                        ? ppgtree->flush.prgliOffsets[cPages].QuadPart
                        : ppgtree->flush.pdwOffsets[cPages];
        ptte = PgTreeGetEntry (ppgtree, &liOfst, NULL);
        DEBUGCHK (ptte);
        PgtreeSetDirty (ppgtree, ptte);
    }
    
    UnlockPgTree (ppgtree);
}

//
// AtomicGatheredFlush - (Atomic+Gather only) flush using gathered write.
// 
static BOOL AtomicGatheredFlush (PPAGETREE ppgtree)
{
    FlushInfo_t *pFlush = &ppgtree->flush;
    
    // push all dirty bits into the tree
    BOOL fRet = CleanupView (g_pprcNK, ppgtree, pFlush->pview, 0);

    DEBUGCHK (ppgtree->wFlags & MAP_ATOMIC);
    DEBUGCHK (ppgtree->wFlags & MAP_GATHER);
    DEBUGCHK (OwnFlushLock (ppgtree));
    DEBUGCHK (pFlush->pview);

    if (fRet) {
        // collect the dirty pages
        DWORD cPages = CollectFlushInfo (ppgtree);
        if (cPages) {
            // write out the dirty pages
            fRet = PHD_FSIoctl (((PFSMAP) pFlush->pview->phdMap->pvObj)->phdFile,
                                IOCTL_FILE_WRITE_GATHER,
                                pFlush->prgSegments, cPages << VM_PAGE_SHIFT,
                                pFlush->prgliOffsets, 0,
                                NULL, NULL);

            if (!fRet) {
                // failed. restore the dirty pages into the page tree
                RestoreDirtyPages (ppgtree, cPages);
            }
        }
    }

    return fRet;
}

//
// AtomicWriteFlushRecord (atomic non-gather only): update flush record
//
FORCEINLINE BOOL AtomicWriteFlushRecord (PPAGETREE ppgtree, PHDATA phdFile, const FSLogFlushSettings *pFlushRecord)
{
    DWORD        cbWritten;

    return PHD_WriteFileWithSeek (phdFile, 
                                  pFlushRecord,
                                  sizeof (FSLogFlushSettings), 
                                  &cbWritten,
                                  NULL,
                                  offsetof(fslog_t, dwRestoreFlags), 
                                  0)
       && (sizeof (FSLogFlushSettings) == cbWritten)
       && PHD_FlushFileBuffers (phdFile);
}

//
// AtomicCommitChanges (atomic non-gather only): final stage of flush update the flush record to "normal"
//
FORCEINLINE BOOL AtomicCommitChanges (PPAGETREE ppgtree, PHDATA phdFile, FSLogFlushSettings *pFlushRecord)
{
    pFlushRecord->dwRestoreFlags = FSLOG_RESTORE_FLAG_NONE;
    pFlushRecord->dwRestoreSize  = VM_PAGE_SIZE;
    return AtomicWriteFlushRecord (ppgtree, phdFile, pFlushRecord);
}

//
// AtomicRestoreFile (atomic non-gather only): recover the file content in the case where a flush is incomplete
//
static BOOL AtomicRestoreFile (PPAGETREE ppgtree, FSLogFlushSettings *pFlushRecord)
{
    FlushInfo_t *pFlush  = &ppgtree->flush;
    BOOL         fRet    = TRUE;
    PHDATA       phdFile = ((PFSMAP) pFlush->pview->phdMap->pvObj)->phdFile;
    DWORD        cPages  = pFlushRecord->dwRestoreSize >> 16;
    LPVOID       pBuffer = VMAlloc (g_pprcNK, NULL, VM_PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    DWORD        cbAccessed;

    DEBUGCHK (FSLOG_RESTORE_FLAG_FLUSHED == pFlushRecord->dwRestoreFlags);

    // read the array of offsets to restore from
    fRet =  cPages
         && pBuffer
         && IsPageAligned (pFlushRecord->dwRestoreStart)
         && (pFlushRecord->dwRestoreStart < MAX_ATOMIC_FILE_SIZE)
         && (cPages <= PAGECOUNT (pFlushRecord->dwRestoreStart))
         && PHD_ReadFileWithSeek (phdFile, 
                                pFlush->pdwOffsets, 
                                cPages * sizeof (DWORD), 
                                &cbAccessed, 
                                NULL, 
                                pFlushRecord->dwRestoreStart + (cPages << VM_PAGE_SHIFT),
                                0)
         && (cPages * sizeof (DWORD) == cbAccessed);

    if (fRet) {
        DWORD idx = 0;
        do {
            fRet = IsPageAligned (pFlush->pdwOffsets[idx])
                && (pFlush->pdwOffsets[idx] < pFlushRecord->dwRestoreStart)
                   // read from tail of the file
                && PHD_ReadFileWithSeek (phdFile, 
                                pBuffer, 
                                VM_PAGE_SIZE,
                                &cbAccessed,
                                NULL, 
                                pFlushRecord->dwRestoreStart + (idx << VM_PAGE_SHIFT),
                                0)
                && (VM_PAGE_SIZE == cbAccessed);

            if (!fRet) {
                break;
            }

            // special handling of log page - make sure flush record is intact
            if (0 == pFlush->pdwOffsets[idx]) {
                memcpy (&((fslog_t *) pBuffer)->dwRestoreFlags, pFlushRecord, sizeof (FSLogFlushSettings));
            }
            
            // write to actual location
            fRet = PHD_WriteFileWithSeek (phdFile, 
                                pBuffer, 
                                VM_PAGE_SIZE,
                                &cbAccessed,
                                NULL, 
                                pFlush->pdwOffsets[idx],
                                0)
                && (VM_PAGE_SIZE == cbAccessed);
            
        } while (fRet && (++ idx < cPages));

        if (fRet) {
            pFlushRecord->dwRestoreFlags = FSLOG_RESTORE_FLAG_NONE;
            pFlushRecord->dwRestoreSize  = VM_PAGE_SIZE;
            // commit the changes 
            fRet = PHD_FlushFileBuffers (phdFile)
                && AtomicCommitChanges (ppgtree, phdFile, pFlushRecord);
        }

    }
    if (pBuffer) {
        VMFreeAndRelease (g_pprcNK, pBuffer, VM_PAGE_SIZE);
    }
    return fRet;
}

//
// AtomicWritePages (atomic non-gather only): write an array of pages to file. 
//      dwRestoreStart == 0 --> write to actual file location
//      dwRestoreStart != 0 --> append the pages to end of file, and append the array of offset after the pages
//
static BOOL AtomicWritePages (PPAGETREE ppgtree, DWORD cPages, DWORD dwRestoreStart)
{
    FlushInfo_t *pFlush     = &ppgtree->flush;
    PMAPVIEW     pview      = pFlush->pview;
    PHDATA       phdFile    = ((PFSMAP) pview->phdMap->pvObj)->phdFile;
    LPBYTE       pBase      = pview->base.pBaseAddr;
    DWORD        dwViewBase = pview->base.liMapOffset.LowPart;
    DWORD        idx        = 0;
    BOOL         fRet       = TRUE;
    DWORD        cbWritten;

    DEBUGCHK (!pview->base.liMapOffset.HighPart);
    
    while (idx < cPages) {
        DWORD idxEnd, cbToWrite;

        DEBUGCHK (pFlush->pdwOffsets[idx] >= dwViewBase);
        DEBUGCHK (IsPageAligned (pFlush->pdwOffsets[idx]));

        // count the number of consecutive pages such that we can do it in a single write
        for (idxEnd = idx + 1; idxEnd < cPages; idxEnd ++) {
            // the offsets must be mono-increasing.
            DEBUGCHK (pFlush->pdwOffsets[idxEnd] > pFlush->pdwOffsets[idxEnd-1]);
            if (pFlush->pdwOffsets[idxEnd-1] + VM_PAGE_SIZE != pFlush->pdwOffsets[idxEnd]) {
                break;
            }
        }

        // write the pages to disk
        cbToWrite = (idxEnd - idx) << VM_PAGE_SHIFT;
        if (!PHD_WriteFileWithSeek (phdFile, 
                                    pBase + (pFlush->pdwOffsets[idx] - dwViewBase),
                                    cbToWrite,
                                    &cbWritten,
                                    NULL,
                                    dwRestoreStart ? (dwRestoreStart + (idx << VM_PAGE_SHIFT))  // append to end of file
                                                   : pFlush->pdwOffsets[idx],                   // actual page write
                                    0)
           || (cbToWrite != cbWritten)) {
           fRet = FALSE;
           break;
        }
        idx = idxEnd;
    }

    if (fRet && dwRestoreStart) {
        // write the array of offsets
        DEBUGCHK (idx == cPages);
        fRet = PHD_WriteFileWithSeek (phdFile, 
                                    pFlush->pdwOffsets,
                                    cPages * sizeof(DWORD),
                                    &cbWritten,
                                    NULL,
                                    dwRestoreStart + (cPages << VM_PAGE_SHIFT),
                                    0)
            && ((cPages * sizeof(DWORD)) == cbWritten);
    }

    // commit the write if successful
    return fRet && PHD_FlushFileBuffers (phdFile);
}

extern const ULARGE_INTEGER liZero;
//
// Update flush record in the page tree.
// - called only when the log record page is dirty during the 2nd phase of the flush.
// - does not change the dirty status of the page.
//
FORCEINLINE BOOL AtomicUpdateFlushRecord (PPAGETREE ppgtree, const FSLogFlushSettings *pFlushRecord)
{
    fslog_t *pLogPage = (fslog_t *) ReserveTempVM ();
    
    if (pLogPage) {
        
        PTreePTEntry ptte;

        DEBUGMSG (ZONE_MAPFILE, (L"UpdateFlushRecord: updateing flush record to 0x%8.8lx 0x%8.8lx\r\n", pFlushRecord->dwRestoreFlags, pFlushRecord->dwRestoreSize));
        
        LockPgTree (ppgtree);

        // get the tree entry of the log page
        ptte = PgTreeGetEntry (ppgtree, &liZero, NULL);

        PREFAST_DEBUGCHK (ptte);
        DEBUGCHK (TreeEntryIsCommitted (*ptte));

        // create a tmp mapping to the log page
        VERIFY (VMCreateKernelPageMapping (pLogPage, TreeEntry2PFN (*ptte)));

        memcpy (&pLogPage->dwRestoreFlags, pFlushRecord, sizeof (FSLogFlushSettings));

        UnlockPgTree (ppgtree);

        // remove teh temp mapping
        VMRemoveKernelPageMapping (pLogPage);
        
        // invalidate the TLB
        InvalidatePages (g_pprcNK, (DWORD) pLogPage, 1);

        FreeTempVM (pLogPage);
        
    }

    return NULL != pLogPage;
}

//
// AtomicNonGatheredFlush (atomic non-gather only): perform flush on atomic non-gather map file.
//
static BOOL AtomicNonGatheredFlush (PPAGETREE ppgtree)
{
    FlushInfo_t *pFlush     = &ppgtree->flush;
    PMAPVIEW     pview      = pFlush->pview;
    PHDATA       phdFile    = ((PFSMAP) pview->phdMap->pvObj)->phdFile;
    BOOL         fRet       = FALSE;
    DWORD        cbRead;
    FSLogFlushSettings FlushRecord;

    DEBUGCHK (pview);

    // read the flush record
    if (PHD_ReadFileWithSeek (phdFile, &FlushRecord, sizeof (FlushRecord), &cbRead, NULL, offsetof (fslog_t, dwRestoreFlags), 0)
        && (sizeof (FlushRecord) == cbRead)) {

        fRet = ((FSLOG_RESTORE_FLAG_NONE == FlushRecord.dwRestoreFlags) || AtomicRestoreFile (ppgtree, &FlushRecord))
            && CleanupView (g_pprcNK, ppgtree, pview, 0);

        if (fRet) {
            // collect the dirty pages
            DWORD cPages = CollectFlushInfo (ppgtree);

            DEBUGCHK (FSLOG_RESTORE_FLAG_NONE == FlushRecord.dwRestoreFlags);
            DEBUGCHK (!(FlushRecord.dwRestoreSize >> 16));

            if (cPages) {
                FlushRecord.dwRestoreFlags = FSLOG_RESTORE_FLAG_FLUSHED;
                FlushRecord.dwRestoreSize |= (cPages << 16);
                DEBUGMSG (ZONE_MAPFILE, (L"AtomicNonGatheredFlush: appending %d pages to the end of file\r\n", cPages));

                DEBUGCHK (FlushRecord.dwRestoreStart);
                
                // write the pages to the end of file
                fRet = AtomicWritePages (ppgtree, cPages, FlushRecord.dwRestoreStart)   // append to end of file
                    && AtomicWriteFlushRecord (ppgtree, phdFile, &FlushRecord);         // update state to "flushed"

                if (fRet) {
                    // There is no need to restore dirty pages once we reach here,
                    // as we've already written all the dirty pages to end of the file
                    // and indidated in the flush record that the flush needs to be 
                    // carried on. Restoring dirty bits only causes unnecessary
                    // flushes in case of failure.

                    // now we're ready to write to the acutal file location. Before we
                    // do that, we must make sure we don't overwrite the flush record.
                    if (0 == pFlush->pdwOffsets[0]) {
                        // log page is dirty. i.e. we can overwrite the log page
                        ULARGE_INTEGER liOfst;
                        
                        liOfst.QuadPart = offsetof (fslog_t, dwRestoreFlags);
                        DEBUGCHK (liOfst.LowPart < VM_PAGE_SIZE);
                        DEBUGCHK (liOfst.LowPart + sizeof (FSLogFlushSettings) <= VM_PAGE_SIZE);

                        DEBUGMSG (ZONE_MAPFILE, (L"AtomicNonGatheredFlush: log page is dirty, updating flush record in log page\r\n"));
                        fRet = AtomicUpdateFlushRecord (ppgtree, &FlushRecord);
                    }

                    DEBUGMSG (ZONE_MAPFILE, (L"AtomicNonGatheredFlush: writing %d pages to actual location\r\n", cPages));
                    fRet = fRet
                        && AtomicWritePages (ppgtree, cPages, 0)                    // write to actual file location
                        && AtomicCommitChanges (ppgtree, phdFile, &FlushRecord)    // update state to "normal"
                        && AtomicUpdateFlushRecord (ppgtree, &FlushRecord);     //This is required to update the commit changes in flushrecord into
                                                                                //RAM view of file, AtomicCommitChanges updated only flash version of file
                                                                                //hive backup is taken from RAM, without this update incorrect value of Flushrecord will be used
                } else {
                    RestoreDirtyPages (ppgtree, cPages);
                }
            }
        }
    }

    return fRet;
}

//
// PageTreeAtomicFlush - (atomic only) perform flush for a atomic page tree
//
BOOL PageTreeAtomicFlush (PPAGETREE ppgtree, DWORD dwFlags)
{
    BOOL  fRet = TRUE;

    // write-back or decommit must be set
    DEBUGCHK (dwFlags & (VFF_WRITEBACK|VFF_DECOMMIT));
    
    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeAtomicFlush: 0x%8.8lx, 0x%x\r\n", ppgtree, dwFlags));

    // need to serialize flush with unmap/pagin
    // when readlock is held, size cannot be changed, view cannot be unmapped and there cannot be any 
    // pagin for write. pgain for read can occur at the same time.

    if (VFF_DECOMMIT == dwFlags) {
        // this is the case when we try to page out a mapfile while memory is low. We don't want to get into
        // contention with flush, where we would need to wait for flush to finish. Use TryEnterCS and bail
        // if we can't take the CS right away
        fRet = TryAcquireFlushLock (ppgtree);
    } else {

        AcquireFlushLock (ppgtree);
    }

    if (fRet) {
        FlushInfo_t *pFlush = &ppgtree->flush;

        if (pFlush->pview) {

            if (dwFlags & VFF_WRITEBACK) {
                // write-back and release should never com together
                DEBUGCHK (!(dwFlags & VFF_RELEASE));

                // push all dirty bits into the tree
                fRet = (ppgtree->wFlags & MAP_GATHER)
                     ? AtomicGatheredFlush (ppgtree)
                     : AtomicNonGatheredFlush (ppgtree);
            }

            // regardless flush success or not, we'll try to page out r/o pages
            if (dwFlags & VFF_DECOMMIT) {
                CleanupView (g_pprcNK, ppgtree, pFlush->pview, dwFlags);
                fRet = PageTreePageOut (ppgtree, NULL, 0, dwFlags);

                if (dwFlags & VFF_RELEASE) {
                    // view unmapped. We need to clear the tree regardless flush success or not.
                    // we can potentially lost data if it failed. However, the logging mechanism
                    // in registry/cedb will ensure that it's still transactional.
                    PageTreeFreeFlushBuffer (ppgtree);
                }
            }
            
        }
        ReleaseFlushLock (ppgtree);
    }
    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeAtomicFlush: returns %d\r\n", fRet));
    
    return fRet;
}

//
// PageTreeFlush - flush content of a non-atomic page tree to the underlying file system. 
//
BOOL PageTreeFlush (PPAGETREE ppgtree, PHDATA phdFile, const ULARGE_INTEGER *pliOfst, DWORD cbSize, PBOOL pfCancelFlush, LPBYTE pReuseFlushVM)
{
    BOOL  fFlushed = TRUE;
    DEBUGCHK (!(ppgtree->wFlags & MAP_ATOMIC));
        
    if (ppgtree->cDirty) {
        LPBYTE pFlushVM = pReuseFlushVM
                        ? pReuseFlushVM
                        : (LPBYTE) ReserveTempVM ();
        if (pFlushVM) {

            ULARGE_INTEGER liOfstEnd;
            ULARGE_INTEGER liOfst;
            
            liOfst.QuadPart = pliOfst? pliOfst->QuadPart : 0;

            DEBUGCHK (IsPageAligned (liOfst.LowPart));

            if (cbSize) {
                cbSize = AdjustSize (ppgtree, &liOfst, cbSize);
                liOfstEnd.QuadPart = liOfst.QuadPart + cbSize;

            } else {
                // full file flush
                liOfstEnd.QuadPart = ppgtree->liSize.QuadPart;
            }

            DEBUGMSG (ZONE_MAPFILE, (L"PageTreeFlush: 0x%8.8lx 0x%8.8lx 0x%016I64x 0x%8.8lx 0x%8.8lx\r\n", 
                                    ppgtree, phdFile, (pliOfst?pliOfst->QuadPart:0), cbSize, pFlushVM));
            
            while (liOfst.QuadPart < liOfstEnd.QuadPart) {

                // flush in segments (consecutive dirty pages)
                fFlushed = (pfCancelFlush && *pfCancelFlush)
                         ? FALSE
                         : PageTreeFlushNextSegment (ppgtree, phdFile, &liOfst, CalcFlushSize (&liOfstEnd, &liOfst), pFlushVM);

                if (!fFlushed || !ppgtree->cDirty) {
                    // stop if failed, or no more dirty pages
                    break;
                }
            }

            if (pFlushVM != pReuseFlushVM) {
                FreeTempVM (pFlushVM);
            }
        } else {
            fFlushed = FALSE;
        }
    }

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeFlush returns: fFlushed = %d\r\n", fFlushed));
    return fFlushed;
}

//
// PageTreeWritePage - worker function for PageTreeWrite. Write a page to the page tree. 
//
DWORD PageTreeWritePage (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, LPBYTE pWriteVM, const BYTE *pData, DWORD cbSize)
{
    ULARGE_INTEGER  liPageOfst  = *pliOfst;
    DWORD           dwResult    = PAGEIN_SUCCESS;
    DWORD           dwPageOfst  = (pliOfst->LowPart & VM_PAGE_OFST_MASK);
    DWORD           dwPFN       = INVALID_PHYSICAL_ADDRESS;
    PTreePTEntry    ptte;

    liPageOfst.LowPart -= dwPageOfst;

    DEBUGCHK (ppgtree->dwCacheableFileId && (dwPageOfst + cbSize <= VM_PAGE_SIZE));

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeWritePage: 0x%8.8lx 0x%016I64x 0x%8.8lx 0x%8.8lx\r\n", 
                            ppgtree, pliOfst->QuadPart, cbSize, pWriteVM));
            
    LockPgTree (ppgtree);

    // see if the page to write is already in the apge tree
    ptte = PgTreeGetEntry (ppgtree, &liPageOfst, NULL);
    
    if (ptte && TreeEntryIsCommitted (*ptte)) {
        // already in tree, just use it
        VERIFY (VMCreateKernelPageMapping (pWriteVM, TreeEntry2PFN (*ptte)));
        
    } else {
        // page not exist yet. 
        PTreePTEntry ptteParent = NULL;
        DWORD        dwFlags    = PM_PT_ZEROED;

        // release tree lock to set setup a new page
        UnlockPgTree (ppgtree);

        // get a paging page
        dwPFN = PGPOOLGetPage (&g_PagingPool, &dwFlags, MEMPRIO_KERNEL);
        
        if (INVALID_PHYSICAL_ADDRESS == dwPFN) {
            dwResult = PAGEIN_RETRY_PAGEPOOL;
            
        } else {

            // create mapping to the paging page
            VERIFY (VMCreateKernelPageMapping (pWriteVM, dwPFN));

            // read from file if needed
            if (cbSize < VM_PAGE_SIZE) {    // partial page
                DWORD cbRead = 0;

                DEBUGCHK (ppgtree->pFsFuncs);

                // We don't want to issue disk io unless we have to. We don't need to do disk read if
                //      (1) we're writing from the start of the last page of the file, and 
                //      (2) the size to write is at least to the end of file.
                //
                // NOTE: we're accessing page tree size outside of page tree lock here. In case there is a race
                //       between size change and write:
                //       (1) if there is no write issued after size change, the content is still valid.
                //       (2) if there is another write to the same file after size change, we're racing regardless.
                //           one of the content will be lost.
                //       Currently file system serializes all read/write/size-change to the same file. So there
                //       wouldn't be a race. However, the implementation will still be valid even if file system
                //       removes the constraint.
                if (!dwPageOfst && (liPageOfst.QuadPart + cbSize >= ppgtree->liSize.QuadPart)) {
                    cbRead = cbSize;                // just update cbRead as we're going to overwrite the whole thing
                } else if (!UncachedRead (ppgtree, &liPageOfst, (LPBYTE) pWriteVM - dwPageOfst, VM_PAGE_SIZE, &cbRead)) {
                    dwResult = PAGEIN_FAILURE;
                    dwFlags &= ~PM_PT_NEEDZERO;     // clear PM_PT_NEEDZERO so we don't do memset unnecessarily.
                }

                if ((cbRead < VM_PAGE_SIZE) && (dwFlags & PM_PT_NEEDZERO)) {
                    // fill tail of the page with 0
                    memset (pWriteVM - dwPageOfst + cbRead, 0, VM_PAGE_SIZE - cbRead);
                }
            }
        }

        // re-take the lock
        LockPgTree (ppgtree);

        // get the page tree entry again. This time we'll create map table if it doesn't exist yet.
        if (PAGEIN_SUCCESS == dwResult) {

            if (liPageOfst.QuadPart < ppgtree->liSize.QuadPart) {

                ptte = PgTreeGetEntry (ppgtree, &liPageOfst, &ptteParent);

                if (ptte) {
                    
                    if (TreeEntryIsCommitted (*ptte)) {
                        // someone beats us paging in the page, use that page instead

                        // remove the page we use for paging
                        VERIFY (VMRemoveKernelPageMapping (pWriteVM) == dwPFN);

                        // invalidate TLB, for we are going to remap into the same VM
                        InvalidatePages (g_pprcNK, (DWORD) pWriteVM - dwPageOfst, 1);

                        // create new mapping to the paged that is paged in
                        VERIFY (VMCreateKernelPageMapping (pWriteVM, TreeEntry2PFN (*ptte)));
                        
                    } else {
                        // new page, move the page to page tree
                        PgtreeCommitPage (ppgtree, ptte, ptteParent, dwPFN);

                        PagePoolAddObject (&ppgtree->pageoutobj);

                        dwPFN = INVALID_PHYSICAL_ADDRESS;   // mark the page used so we don't free it.
                    }
                    
                } else {
                    // OOM trying to allocate tree table
                    dwResult = PAGEIN_RETRY_MEMORY;
                }
            } else {

                // file size shrinked below offset
                DEBUGCHK (0);   // shouldn't happen...
                dwResult = PAGEIN_FAILURE;
            }
        }
        
    }

    if (PAGEIN_SUCCESS == dwResult) {
        // must do the copy before we release page tree lock. Or race can occur and read a all-0 page.
        if (pData) {
            memcpy (pWriteVM, pData, cbSize);
        } else {
            memset (pWriteVM, 0, cbSize);
        }

        // set dirty bit of the entry
        PgtreeSetDirty (ppgtree, ptte);
    }

    UnlockPgTree (ppgtree);

    VMRemoveKernelPageMapping (pWriteVM);
    
    if (INVALID_PHYSICAL_ADDRESS != dwPFN) {
        // paging page not consumed.
        PGPOOLFreePage (&g_PagingPool, dwPFN, NULL);
    }

    // invalidate the TLB
    InvalidatePages (g_pprcNK, (DWORD) pWriteVM - dwPageOfst, 1);

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeWritePage returns, dwResult = %d\r\n", dwResult));

    return dwResult;
}

void PageOutNextFile (LPBYTE pReuseVM);

//
// LimitFileCache - limit the size of file cache. Currently we only limit the # of dirty pages. We might want to consider
//                  limit total number of pages used in file cache if we found file cache is eating up too much memory.
//
void LimitFileCache (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, DWORD cbSize, ULARGE_INTEGER *pliFlush, LPBYTE pFlushVM)
{
    // only apply to pure file write
    if (!ppgtree->cViews) {
        
        // if this file has too many dirty pages, page it out first
        if (ppgtree->cDirty > MAX_DIRTY_PER_FILE) {
            DWORD cbToFlush;

            DEBUGCHK (pliOfst->QuadPart >= pliFlush->QuadPart);
            DEBUGCHK (IsPageAligned (pliFlush->LowPart));
            do {
                cbToFlush = PAGEALIGN_DOWN (CalcFlushSize (pliOfst, pliFlush));

                if (!cbToFlush) {
                    break;
                }
                if (!PageTreeFlushNextSegment (ppgtree, NULL, pliFlush, cbToFlush, pFlushVM)) {
                    // hardware failure?
                    break;
                }

                if (pliFlush->QuadPart >= pliOfst->QuadPart) {
                    break;
                }
                
            } while (ppgtree->cDirty > MAX_DIRTY_PER_FILE);

            if (ppgtree->cDirty > MAX_DIRTY_PER_FILE) {
                // we've flushed everything from start of file to the offset we're writing, and
                // there are still many dirty pages, flush the tail of the file

                // page align up the end of the range
                pliFlush->QuadPart = pliOfst->QuadPart + cbSize + VM_PAGE_OFST_MASK;
                pliFlush->LowPart  = PAGEALIGN_DOWN (pliFlush->LowPart);
                
                PageTreeFlush (ppgtree, NULL, pliFlush, 0, NULL, pFlushVM);

                // reset flush position
                pliFlush->QuadPart = 0;
            }
            
        }
        // lock order issue: we can't flush file cache if we're writing to diskcache. 
        if (!(ppgtree->wFlags & PGT_DISKCACHE)) {
            // if we still have too many dirty pages, page out other files
            while (g_cTotalDirty > MAX_DIRTY_THRESHOLD) {
                PageOutNextFile (pFlushVM);
            }
        }
    }
}

//
// PageTreeWrite - write data into the page tree
//
BOOL PageTreeWrite (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, const BYTE *pData, DWORD cbSize, LPDWORD pcbWritten)
{
    BOOL   fRet     = FALSE;
    LPBYTE pWriteVM = ReserveTempVM ();
    DWORD  cbWritten  = 0;

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeWrite: 0x%8.8lx 0x%016I64x 0x%8.8lx 0x%8.8lx\r\n", 
                            ppgtree, pliOfst->QuadPart, cbSize, pWriteVM));

    if (pWriteVM) {
        ULARGE_INTEGER liOfst = *pliOfst;
        ULARGE_INTEGER liFlush = {0};
        DWORD cbPageOfst = (pliOfst->LowPart & VM_PAGE_OFST_MASK);
        DWORD cbToWrite  = VM_PAGE_SIZE - cbPageOfst;
        DWORD dwResult   = PAGEIN_SUCCESS;

        // only cache filter would ever call this function
        DEBUGCHK (ppgtree->dwCacheableFileId);

        cbSize = AdjustSize (ppgtree, &liOfst, cbSize);

        while (cbSize) {

            if (cbToWrite > cbSize) {
                cbToWrite = cbSize;
            }

            dwResult = PageTreeWritePage (ppgtree, &liOfst, pWriteVM + (liOfst.LowPart & VM_BLOCK_OFST_MASK), pData, cbToWrite);

            if (PAGEIN_FAILURE == dwResult) {
                break;
            }

            if (PAGEIN_SUCCESS == dwResult) {
                cbWritten       += cbToWrite;
                cbSize          -= cbToWrite;
                if (!cbSize) {
                    // limit file cache so it doesn't drain our memory
                    LimitFileCache (ppgtree, &liOfst, cbToWrite, &liFlush, pWriteVM);
                    break;
                }
                liOfst.QuadPart += cbToWrite;
                // pData == NULL iff zeroing pages
                if (pData) {
                    pData       += cbToWrite;
                }
                cbPageOfst       = 0;
                cbToWrite        = VM_PAGE_SIZE;

                DEBUGCHK (IsPageAligned (liOfst.LowPart));

                // limit file cache so it doesn't drain our memory
                LimitFileCache (ppgtree, &liOfst, cbSize, &liFlush, pWriteVM);

            } else {
                // OOM, retry
                if (!WaitForPagingMemory (g_pprcNK, dwResult)) {
                    DEBUGCHK (0);
                    break;
                }
                dwResult = PAGEIN_SUCCESS;
            }
            
            cbSize = AdjustSize (ppgtree, &liOfst, cbSize);
        }

        FreeTempVM (pWriteVM);

        fRet = (PAGEIN_SUCCESS == dwResult);
    }

    if (pcbWritten) {
        *pcbWritten = cbWritten;
    }
    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeWrite returns: fRet = %d, cbWritten = 0x%8.8lx\r\n", fRet, cbWritten));
    
    return fRet;
}

//
// read existing page from the page tree to buffer passed in
// NOTE: this function can copy up to 4M of data at a time
//
DWORD PageTreeReadExisting (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, LPBYTE pBuffer, DWORD cbToRead, LPBYTE pReadVM)
{
    DWORD  cbRead     = 0;
    DWORD  dwOfstLow  = pliOfst->LowPart;
    DWORD  dwPageOfst = (dwOfstLow & VM_PAGE_OFST_MASK);
    DWORD  cbInPage   = VM_PAGE_SIZE - dwPageOfst;
    PTreePTEntry ptte;
    
    LockPgTree (ppgtree);

    ptte = PgTreeGetEntry (ppgtree, pliOfst, NULL);

    while (ptte && TreeEntryIsCommitted (*ptte)) {

        LPBYTE pPage = pReadVM + (dwOfstLow & VM_BLOCK_OFST_MASK) - dwPageOfst;

        if (cbInPage > cbToRead) {
            cbInPage = cbToRead;
        }

        // map the page and perform copying
        VERIFY (VMCreateKernelPageMapping (pPage, TreeEntry2PFN (*ptte)));
        memcpy (pBuffer, pPage+dwPageOfst, cbInPage);

        // unmap the page
        VERIFY (INVALID_PHYSICAL_ADDRESS != VMRemoveKernelPageMapping (pPage));
        InvalidatePages (g_pprcNK, (DWORD) pPage, 1);

        cbToRead    -= cbInPage;
        cbRead      += cbInPage;

        if (!cbToRead) {
            break;
        }

        // move to next page        
        dwOfstLow   += cbInPage;
        pBuffer     += cbInPage;
        dwPageOfst   = 0;
        cbInPage     = VM_PAGE_SIZE;
        ptte         = PgtreeNextEntry (ppgtree, ptte);
    }

    UnlockPgTree (ppgtree);
    return cbRead;
}

//
// GetPagesForTreeFetch - Get pages from paging/memory pool for Fetching pages into the page tree
//
DWORD GetPagesForTreeFetch (PagePool_t *pPool, PPROCESS pprc, LPBYTE pPage, DWORD cPages)
{
    DWORD dwResult = PAGEIN_SUCCESS;
    DWORD dwMemPrio = CalcMemPrioForPaging (pprc);
    DEBUGCHK (IsPageAligned (pPage));

    while (cPages --) {
        DWORD dwFlags  = pPool? 0 : PM_PT_ZEROED;
        DWORD dwPFN    = GetOnePageForPaging (pPool, &dwFlags, dwMemPrio);

        if (INVALID_PHYSICAL_ADDRESS == dwPFN) {
            dwResult = PAGEIN_RETRY_PAGEPOOL;
            break;
        }
        VERIFY (VMCreateKernelPageMapping (pPage, dwPFN));
        if (dwFlags & PM_PT_NEEDZERO) {
            ZeroPage (pPage);
        }
        pPage += VM_PAGE_SIZE;
    }
    return dwResult;
}

//
// FreePagesForTreeFetch - release the unused pages back to paging/memory pool
//
void FreePagesForTreeFetch (PagePool_t *pPool, LPBYTE pPage, DWORD cPages)
{
    DEBUGCHK (pPool || (cPages == 1));
    while (cPages --) {
        FreeOnePagingPage (pPool, VMRemoveKernelPageMapping (pPage));
        pPage += VM_PAGE_SIZE;
    }
}

//
// MovePagesToPageTree - move the pages that we just fetched into page tree.
//
void MovePagesToPageTree (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, LPBYTE pPage, DWORD cbSize)
{
    ULARGE_INTEGER  liOfst = *pliOfst;
    DWORD           cPages = PAGECOUNT (cbSize);
    ULONGLONG       ulTreeSize;
    PTreePTEntry    ptte, ptteParent;
    DWORD           dwPFN;

    DEBUGCHK (IsPageAligned (liOfst.LowPart));
    
    LockPgTree (ppgtree);

    ulTreeSize = ppgtree->liSize.QuadPart;
    ptte = PgTreeGetEntry (ppgtree, &liOfst, &ptteParent);

    while (cPages && ptte && (liOfst.QuadPart < ulTreeSize)) {
        if (!TreeEntryIsCommitted (*ptte)) {
            dwPFN = VMRemoveKernelPageMapping (pPage);
            DEBUGCHK (INVALID_PHYSICAL_ADDRESS != dwPFN);
            PgtreeCommitPage (ppgtree, ptte, ptteParent, dwPFN);
        }
        cPages --;
        pPage += VM_PAGE_SIZE;
        liOfst.QuadPart += VM_PAGE_SIZE;
        ptte   = (&ppgtree->RootEntry == ptte)
               ? PgTreeGetEntry (ppgtree, &liOfst, &ptteParent)
               : PgtreeNextEntry (ppgtree, ptte);
        
    }
    
    UnlockPgTree (ppgtree);
}

//
// PageTreeFetchPages - fetch pages into the page tree. perform pre-fetch if neeeded
//
DWORD PageTreeFetchPages (PPAGETREE ppgtree, PHDATA phdFile, const ULARGE_INTEGER *pliOfst, LPBYTE pReadVM)
{
    DWORD           dwResult = PAGEIN_SUCCESS;
    ULARGE_INTEGER  liOfst   = *pliOfst;
    DWORD           cbSize   = ppgtree->cbPrefetch;       // always use prefetch size
    PTreePTEntry ptte;

    DEBUGCHK (IsBlockAligned (pReadVM));
    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeFetchPages: 0x%8.8lx 0x%8.8lx 0x%016I64x 0x%8.8lx\r\n", ppgtree, phdFile, pliOfst->QuadPart, cbSize));

    // mask off the low bits (note: "-cbSize == ~(cbSize -1) here)
    liOfst.LowPart &= - (LONG) cbSize;

    // update pReadVM to align block-offset with what we want to read
    pReadVM        += (liOfst.LowPart & VM_BLOCK_OFST_MASK);

    DEBUGCHK (IsPageAligned (liOfst.LowPart));

    // skip committed pages
    LockPgTree (ppgtree);

    ptte = PgTreeGetEntry (ppgtree, &liOfst, NULL);

    while (cbSize && ptte && TreeEntryIsCommitted (*ptte)) {
        liOfst.LowPart += VM_PAGE_SIZE;
        pReadVM        += VM_PAGE_SIZE;
        cbSize         -= VM_PAGE_SIZE;
        ptte = PgtreeNextEntry (ppgtree, ptte);
    }

    UnlockPgTree (ppgtree);

    if (cbSize) {
        PagePool_t *pPool  = (PGT_USEPOOL & ppgtree->wFlags)? &g_PagingPool : NULL;
        DWORD       cPages = PAGECOUNT (cbSize);
        DWORD       cbRead = 0;
        
        dwResult = GetPagesForTreeFetch (pPool, pActvProc, pReadVM, cPages);
        
        if (PAGEIN_SUCCESS == dwResult) {

            if (ppgtree->dwCacheableFileId) {
                // cacheable file
                DEBUGCHK (ppgtree->pFsFuncs);
                if (!UncachedRead (ppgtree, &liOfst, pReadVM, cbSize, &cbRead)) {
                    dwResult = PAGEIN_FAILURE;
                }
            } else if (phdFile) {
                // regular file
                if (!PHD_ReadFileWithSeek (phdFile, pReadVM, cbSize, &cbRead, NULL, liOfst.LowPart, liOfst.HighPart)) {
                    dwResult = PAGEIN_FAILURE;
                }
            } else {
                // ram-backed
                DEBUGCHK (1 == cPages);
                cbRead = VM_PAGE_SIZE;
            }

            if (PAGEIN_SUCCESS == dwResult) {
                // memset to 0 for partial pages.
                if (!IsPageAligned (cbRead)) {
                    memset (pReadVM + cbRead, 0, VM_PAGE_SIZE - (cbRead & VM_PAGE_OFST_MASK));
                }
                MovePagesToPageTree (ppgtree, &liOfst, pReadVM, cbRead);

            } else {
                // hardware failure?
                DEBUGCHK (0);
            }
        }
        
        // regardless success or not, free the pages. pages used would have been removed
        // from the VM to the page tree
        FreePagesForTreeFetch (pPool, pReadVM, cPages);
        InvalidatePages (g_pprcNK, (DWORD) pReadVM, cPages);
    }
    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeFetchPages: returns %d\r\n", dwResult));

    return dwResult;
}

//
// PageTreeRead - read pages from pages tree and fetch data into page tree if needed
//
BOOL PageTreeRead (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, LPBYTE pBuffer, DWORD cbSize, LPDWORD pcbRead)
{
    BOOL   fRet    = FALSE;
    LPBYTE pReadVM = (LPBYTE) ReserveTempVM ();
    DWORD  cbTotalRead = 0;

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeRead: 0x%8.8lx 0x%016I64x 0x%8.8lx 0x%8.8lx\r\n", 
                            ppgtree, pliOfst->QuadPart, cbSize, pReadVM));

    DEBUGCHK (ppgtree->dwCacheableFileId);
    
    if (pReadVM) {

        ULARGE_INTEGER liOfst = *pliOfst;
        DWORD dwResult  = PAGEIN_SUCCESS;
        DWORD cbRead;

        cbSize = AdjustSize (ppgtree, &liOfst, cbSize);

        while (cbSize) {

            // read from existing pages in the page tree
            cbRead = PageTreeReadExisting (ppgtree, &liOfst, pBuffer, cbSize, pReadVM);

            while (cbRead) {
                DEBUGCHK (cbRead <= cbSize);

                cbTotalRead     += cbRead;
                cbSize          -= cbRead;
                if (!cbSize) {
                    break;
                }
                liOfst.QuadPart += cbRead;
                pBuffer         += cbRead;
                cbRead = PageTreeReadExisting (ppgtree, &liOfst, pBuffer, cbSize, pReadVM);
            }

            if (!cbSize) {
                // done, we have all the data we need
                break;
            }

            // fetch pages into the page tree
            dwResult = PageTreeFetchPages (ppgtree, NULL, &liOfst, pReadVM);

            PagePoolAddObject (&ppgtree->pageoutobj);

            if (PAGEIN_FAILURE == dwResult) {
                break;
            }

            if (PAGEIN_SUCCESS != dwResult) {
                if (!WaitForPagingMemory (g_pprcNK, dwResult)) {
                    // thread terminated. Could this ever occur?
                    DEBUGCHK (0);
                    break;
                }
                dwResult = PAGEIN_SUCCESS;
            }
            
            cbSize = AdjustSize (ppgtree, &liOfst, cbSize);
        }

        FreeTempVM (pReadVM);

        fRet = (PAGEIN_SUCCESS == dwResult);
        
    }

    if (pcbRead) {
        *pcbRead = cbTotalRead;
    }

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeRead returns: fRet = %d, cbTotalRead = 0x%8.8lx\r\n", fRet, cbTotalRead));

    return fRet;
}

//
// EnumDiscardPage - called when file size shrinked. discard all pages after new size.
//
BOOL EnumPageOutPage (PPAGETREE ppgtree, PTreePTEntry ptte, PTreePTEntry ptteParent, const ULARGE_INTEGER *pliOfst, LPVOID pData)
{
    if (TreeEntryIsCommitted (*ptte)) {
        DWORD dwFlags = *(LPDWORD)pData;
        
        // discard the dirty bit if requested
        if (VFF_RELEASE & dwFlags) {
            PgtreeClearDirty (ppgtree, ptte);
        }
        
        // decommit if no ref to this page.
        if (!TreeEntry2Count (*ptte) && !TreeEntryDirty (*ptte)) {
            PgtreeDecommitPage (ppgtree, ptte, ptteParent);
        }
    }

    return FALSE;   // keep going
}

//
// change the size of a tree
//
BOOL PageTreeSetSize (PPAGETREE ppgtree, const ULARGE_INTEGER *pliSize, BOOL fDeleteFile)
{
    BOOL fRet = (pliSize->QuadPart <= MAX_TREE_SIZE);

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeSetSize: 0x%8.8lx 0x%016I64x %d\r\n", 
                            ppgtree, pliSize->QuadPart, fDeleteFile));

    DEBUGCHK (ppgtree->dwCacheableFileId);

    if (fRet) {

        //
        // PageTreeSetSize is only called from cache filter to grow/thrink file. 
        // - It's already serialized on a per-file basis, except asynchronous flush can be performed at the same time.
        // - We only need to serialize SetSize with Flush when the file is shrinking, as growing file won't affect flushing.
        //
        
        ULARGE_INTEGER liOldSize = ppgtree->liSize;
        ULARGE_INTEGER liNewSizeInPages;

        BOOL    fDecommitNeeded  = FALSE;
        BOOL    fShrink  = (liOldSize.QuadPart > pliSize->QuadPart);
        LPBYTE  pCleanVM = NULL;

        // page align up the new size
        liNewSizeInPages.QuadPart = pliSize->QuadPart + VM_PAGE_OFST_MASK;
        liNewSizeInPages.LowPart  &= ~VM_PAGE_OFST_MASK;

        if (fShrink) {
            AcquireFlushLock (ppgtree);
        }

        LockPgTree (ppgtree);

        // - if we need to decommit pages, update size to the page-aligned new size to prevent 
        //   further access to pages we're about to decommit.
        // NOTE: we use the "new size in pages" to perform the calculation
        if (liOldSize.QuadPart > liNewSizeInPages.QuadPart) {
            fDecommitNeeded = TRUE;
            ppgtree->liSize.QuadPart = liNewSizeInPages.QuadPart;
        }

        UnlockPgTree (ppgtree);

        if (fDecommitNeeded) {
            // shrinking - unmap all the views to the page tree to prevent access to
            // pages that are no longer valid
            FlushAllViewsOfPageTree (ppgtree, VFF_DECOMMIT);

            // TBD:
            // if there are pages in the page tree that has a ref-count, we can't shrink the file size below the page.
            // i.e. we should fail the call with access violation.
        }

        // call UncachedSetSize only if it's not from DeleteFile
        if (!fDeleteFile) {

            // If shrinking to unaligned size, we might need to perform page-zeroing. 
            // Reserve VM before we acquire page tree lock.
            if (fShrink && !IsPageAligned (pliSize->LowPart)) {
                pCleanVM = ReserveTempVM ();

                fRet = (NULL != pCleanVM);
            }

            if (fRet) {
                // call down to underlining volume to change size.
                fRet = UncachedSetSize (ppgtree, pliSize);
            }
        } else {
            // must be calling with size == 0 when coming from DeleteFile
            DEBUGCHK (!pliSize->QuadPart);
        }

        LockPgTree (ppgtree);
        if (fRet) {
            ppgtree->liSize.QuadPart = pliSize->QuadPart;
            
            if (fDecommitNeeded) {
                DWORD dwFlags = VFF_RELEASE;

                // trim all pages beyond new size
                PageTreeWalk (ppgtree, &liNewSizeInPages, NULL, PTW_DECOMMIT_TABLES, EnumPageOutPage, &dwFlags);

            }

            // zero partial page at the end of file if needed
            if (pCleanVM) {
                PTreePTEntry ptte = PgTreeGetEntry (ppgtree, pliSize, NULL);
                
                DEBUGCHK (fShrink && !IsPageAligned (pliSize->LowPart));
                
                if (ptte && TreeEntryIsCommitted (*ptte)) {
                    DWORD dwPFN = TreeEntry2PFN (*ptte);
                    LPBYTE pPartialPage = pCleanVM + (pliSize->LowPart & VM_BLOCK_OFST_MASK);
                    
                    VERIFY (VMCreateKernelPageMapping (pPartialPage, dwPFN));
                    memset (pPartialPage, 0, VM_PAGE_SIZE - (pliSize->LowPart & VM_PAGE_OFST_MASK));
                    VERIFY (VMRemoveKernelPageMapping (pPartialPage) == dwPFN);

                    InvalidatePages (g_pprcNK, PAGEALIGN_DOWN ((DWORD) pPartialPage), 1);
                }
            }

        } else {
            // failed, restore old size back
            ppgtree->liSize.QuadPart = liOldSize.QuadPart;
        }
        UnlockPgTree (ppgtree);

        if (fShrink) {
            ReleaseFlushLock (ppgtree);
        }
        
        if (pCleanVM) {
            FreeTempVM (pCleanVM);
        }
    }

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeSetSize returns: %d\r\n", fRet));
    
    return fRet;

}

//
// PageTreeGetSize - get size of a page tree
// 
BOOL PageTreeGetSize (PPAGETREE ppgtree, ULARGE_INTEGER *pliSize)
{
    LockPgTree (ppgtree);
    pliSize->QuadPart = ppgtree->liSize.QuadPart;
    UnlockPgTree (ppgtree);
    return TRUE;
}

//
// PageTreeIsDirty - if a page tree has any dirty page
//
BOOL PageTreeIsDirty (PPAGETREE ppgtree)
{
    return 0 != ppgtree->cDirty;
}

//
// PageTreeReduceHeight - reduce the height of a tree
//
void PageTreeReduceHeight (PPAGETREE ppgtree)
{
    while (ppgtree->wHeight && (TreeEntry2Count (ppgtree->RootEntry) == 1)) {
        TreePageTable *ptbl = TreeEntry2PTBL (ppgtree->RootEntry);
        if (!ptbl->tte[0]) {
            // something other than the 1st node is in use. can't reduce height
            break;
        }
        ppgtree->RootEntry = ptbl->tte[0];
        ppgtree->wHeight --; 
        TreeFreePT (ptbl);
    }
}

//
// PageTreePageOut - page out a range of pages
//
BOOL PageTreePageOut (PPAGETREE ppgtree, const ULARGE_INTEGER *pliOfst, DWORD cbSize, DWORD dwFlags)
{
    ULARGE_INTEGER liStart, liEnd;
    BOOL fRet;

    liStart.QuadPart = pliOfst? pliOfst->QuadPart : 0;
    liEnd.QuadPart   = cbSize? (liStart.QuadPart + cbSize) : TreeSizeAtHeight[ppgtree->wHeight];

    DEBUGCHK (IsPageAligned (liStart.QuadPart));

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreePageOut: 0x%8.8lx 0x%016I64x, 0x%016I64x, 0x%x\r\n", 
                            ppgtree, liStart.QuadPart, liEnd.QuadPart, dwFlags));

    LockPgTree (ppgtree);

    PageTreeWalk (ppgtree, &liStart, &liEnd, PTW_DECOMMIT_TABLES, EnumPageOutPage, &dwFlags);

    fRet = !ppgtree->cCommitted;
    UnlockPgTree (ppgtree);

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreePageOut returns: %d\r\n", fRet));
    return fRet;
}

//
// PageTreeDestroy - final destruction of a page tree
//
void PageTreeDestroy (PPAGETREE ppgtree)
{
    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeDestroy: 0x%8.8lx\r\n", ppgtree));
    DEBUGCHK (!ppgtree->lRefCnt);
    DEBUGCHK (ppgtree->cs.hCrit);
    VERIFY (PageTreePageOut (ppgtree, NULL, 0, VFF_RELEASE));
    DEBUGCHK (!TreeEntryIsCommitted (ppgtree->RootEntry));

    DeleteCriticalSection (&ppgtree->cs);

    FreeMem (ppgtree, HEAP_PGTREE);
}

//
// PageTreeAllocateFlushBuffer - allocate flush buffer for Atomic map
//
BOOL PageTreeAllocateFlushBuffer (PPAGETREE ppgtree, PMAPVIEW pview)
{
    DWORD        cPages = PAGECOUNT (pview->base.cbSize);
    FlushInfo_t *pFlush = &ppgtree->flush;
    LPVOID       pFlushSpace;

    DEBUGCHK (MAP_ATOMIC & ppgtree->wFlags);
    DEBUGCHK (IsPageAligned (pview->base.cbSize));
    DEBUGCHK (IsBlockAligned (pview->base.liMapOffset.LowPart));
    DEBUGCHK (IsKModeAddr ((DWORD) pview->base.pBaseAddr));
    DEBUGCHK (!pFlush->pview);

    pFlushSpace = NKmalloc ((MAP_GATHER & ppgtree->wFlags)
                          ? ((sizeof (ULARGE_INTEGER) + sizeof (FILE_SEGMENT_ELEMENT)) * cPages)
                          : (sizeof (DWORD) * cPages));
    if (pFlushSpace) {
        pFlush->pview = pview;
        if (MAP_GATHER & ppgtree->wFlags) {
            pFlush->prgSegments  = (FILE_SEGMENT_ELEMENT*) pFlushSpace;
            pFlush->prgliOffsets = (ULARGE_INTEGER*) (pFlush->prgSegments + cPages);
        } else {
            pFlush->pdwOffsets = (LPDWORD) pFlushSpace;
        }
    }

    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeAllocateFlushBuffer: 0x%8.8lx, %8.8lx, 0x%8.8lx\r\n", ppgtree, pview, pFlushSpace));
    return NULL != pFlushSpace;
}

//
// PageTreeFreeFlushBuffer - free flush buffer for atomic mapping
//
void PageTreeFreeFlushBuffer (PPAGETREE ppgtree)
{
    DEBUGCHK (OwnFlushLock (ppgtree));
    
    DEBUGMSG (ZONE_MAPFILE, (L"PageTreeFreeFlushBuffer: 0x%8.8lx\r\n", ppgtree));

    if (ppgtree->flush.pview) {
        if (MAP_GATHER & ppgtree->wFlags) {
            NKfree (ppgtree->flush.prgSegments);
        } else {
            NKfree (ppgtree->flush.pdwOffsets);
        }
        memset (&ppgtree->flush, 0, sizeof (FlushInfo_t));
    }
}

//
// PageTreeDecView - decrement the number of view of a page tree
//
void PageTreeDecView (PPAGETREE ppgtree)
{
    DEBUGCHK (ppgtree->cViews);
    if (ppgtree) {
        InterlockedDecrement (&ppgtree->cViews);
    }
}

//
// PageTreeIncView - increment the number of view of a page tree. Return true if it's still below max
//
BOOL PageTreeIncView (PPAGETREE ppgtree)
{
    return !ppgtree
        || (InterlockedIncrement (&ppgtree->cViews) <= PGT_MAX_VIEWS);
}

//
// GetPageTree - retrieve the page tree of a file if it exists.
//
PPAGETREE GetPageTree (PHDATA phdFile)
{
    DWORD dwPageTreeId = 0;
    PHD_FSIoctl (phdFile, 
                 FSCTL_GET_PAGETREE_ID,
                 NULL, 0,
                 &dwPageTreeId, sizeof (DWORD),
                 NULL, NULL);
    return (PPAGETREE) dwPageTreeId;
}

