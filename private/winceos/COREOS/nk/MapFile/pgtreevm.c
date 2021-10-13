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

//
// this file contains all the functions related to page tree that requires VM enumerations. For example, enumerating a tree view
// to clean up the VM mapping.
//


BOOL Enumerate2ndPT (PPAGEDIRECTORY ppdir, DWORD dwAddr, DWORD cPages, BOOL fWrite, PFN_PTEnumFunc pfnApply, LPVOID pEnumData);

typedef struct {
    ULARGE_INTEGER  liOfst;
    PPAGETREE       ppgtree;
    PTreePTEntry    ptte;
    PPROCESS        pprc;
    DWORD           dwAddr;         // VA of view. none-zero only if there is page locked within the range
    DWORD           dwFlags;
    LONG            cpDecommit;
} VIEWFLUSHDATA, *PVIEWFLUSHDATA;

//
// AtomicEntryDirty - if the tree is a atomic and the tree table entry is dirty
//
FORCEINLINE BOOL AtomicEntryDirty (PPAGETREE ppgtree, PTreePTEntry ptte)
{
    return (MAP_ATOMIC & ppgtree->wFlags)
        && TreeEntryDirty (*ptte);
}

//
// EnumTreeViewFlush - enumeration function to flush a tree view.
//
static BOOL EnumTreeViewFlush (PPTENTRY pEntry, LPVOID pEnumData)
{
    PVIEWFLUSHDATA pvfd = (PVIEWFLUSHDATA) pEnumData;
    PTENTRY      entry  = *pEntry;
    PTreePTEntry ptte   = pvfd->ptte; 
    PPAGETREE ppgtree   = pvfd->ppgtree;

    DEBUGCHK (OwnPgTreeLock (ppgtree));

    if (IsPageCommitted (entry)) {

        if (!pvfd->dwAddr || !VMIsPagesLocked (pvfd->pprc, pvfd->dwAddr, 1)) {

            DWORD dwPFN = PFNfromEntry (entry);

            if (!ptte) {
                ptte = PgTreeGetEntry (ppgtree, &pvfd->liOfst, NULL); 
            }
            
            PREFAST_DEBUGCHK(ptte);                         // we should never have a committed entry in view that is not in the page tree
            DEBUGCHK (TreeEntry2Count (*ptte)               // most have a ref-count > 0
                   && (TreeEntry2PFN (*ptte) == dwPFN)      // view/tree must alias to the the same physical page
                   );

            // mark the page in the page tree dirty if this is a writeable page                
            if (IsPageWritable (entry) && !(PGT_RAMBACKED & ppgtree->wFlags)) {
                
                // Make the page r/o (clear dirty bit in VM)
                *pEntry = ReadOnlyEntry (entry);

                // Make the page dirty in the page tree
                if (pvfd->liOfst.QuadPart < ppgtree->liSize.QuadPart) {
                    PgtreeSetDirty (ppgtree, ptte);
                }
            }

            // decommit the veiw if requested
            // NOTE: for atomic map: do not decommit the view page if the page is dirty unless it's
            //       final view release (failed final flush, not much we can do but los data)
            if (   (VFF_RELEASE & pvfd->dwFlags)
                || ((VFF_DECOMMIT & pvfd->dwFlags) && !AtomicEntryDirty (ppgtree, ptte))) {

                TreeEntryDecCount (ptte);
                *pEntry = MakeReservedEntry (VM_PAGER_MAPPER);
                FreePhysPage (dwPFN);
                pvfd->cpDecommit ++;
            }
        }
    }

    // update enumeration structure
    pvfd->liOfst.QuadPart += VM_PAGE_SIZE;
    pvfd->ptte             = PgtreeNextEntry (ppgtree, ptte);
    if (pvfd->dwAddr) {
        pvfd->dwAddr += VM_PAGE_SIZE;
    }

    return TRUE;       // keep enumerating
}

//
// PageTreeViewFlush - flush a page tree view. 
//
// dwFlags: VFF_DECOMMIT - decommit the VM only
//          VFF_RELEASE  - final release of a view
//
BOOL PageTreeViewFlush (PPAGETREE ppgtree, 
                        PPROCESS pprc, 
                        const BYTE *pStartAddr, 
                        const ULARGE_INTEGER *pliOfst, 
                        DWORD cbSize, 
                        DWORD dwFlags)
{
    VIEWFLUSHDATA   vfd;
    PPAGEDIRECTORY  ppdir;
    DWORD           cPages = PAGECOUNT (cbSize);
    BOOL            fPageLocked;
    
    DEBUGCHK (IsPageAligned (pStartAddr) && IsPageAligned (pliOfst->LowPart));

    vfd.liOfst      = *pliOfst;
    vfd.ppgtree     = ppgtree;
    vfd.dwFlags     = dwFlags;
    vfd.cpDecommit  = 0;
    vfd.ptte        = NULL;
    vfd.pprc        = pprc;
    vfd.dwAddr      = 0;

    LockPgTree (ppgtree);

    ppdir = LockVM (pprc);

    DEBUGCHK (ppdir);

    // we will only fail if there is any page locked.
    fPageLocked = VMIsPagesLocked (pprc, (DWORD) pStartAddr, cPages);
    if (fPageLocked) {
        // at least a page is locked, we need to check if page is locked during decommit.
        vfd.dwAddr = (DWORD) pStartAddr;
    }

    Enumerate2ndPT (ppdir, (DWORD) pStartAddr, cPages, FALSE, EnumTreeViewFlush, &vfd);

    InvalidatePages (pprc, (DWORD) pStartAddr, cPages);

    UnlockVM (pprc);

    UnlockPgTree (ppgtree);

    if (vfd.cpDecommit) {
        // update memory statistic
        UpdateMemoryStatistic (PageTreeUsePool(ppgtree)? &pprc->msSharePaged : &pprc->msShareNonPaged, -vfd.cpDecommit);
    }

    return !fPageLocked;
}


typedef struct {
    ULARGE_INTEGER  liOfst;
    PPAGETREE       ppgtree;
    PPROCESS        pprc;
    PTreePTEntry    ptte;
    DWORD           cPagesMapped;
    DWORD           cPagesExisting;
    DWORD           dwPgProt;
    BOOL            fFlushTLB;
} MAPRANGEDATA, *PMAPRANGEDATA;

extern CRITICAL_SECTION PhysCS;

//
// EnumMapRange - enumeration function to map a tree page to VM
//
BOOL EnumMapRange (PPTENTRY pEntry, LPVOID pEnumData)
{
    PMAPRANGEDATA pmrd  = (PMAPRANGEDATA) pEnumData;
    BOOL      fPastEOF  = (pmrd->ppgtree->liSize.QuadPart <= pmrd->liOfst.QuadPart);

    if (!fPastEOF) {
        PTENTRY       entry     = *pEntry;
        PTreePTEntry  ptte      = pmrd->ptte;
        PPAGETREE     ppgtree   = pmrd->ppgtree;

        if (IsPageCommitted (entry)) {
            // already committed, just change protection if needed
            if (!IsPageWritable (entry) && IsPageWritable (pmrd->dwPgProt)) {
                *pEntry = MakeCommittedEntry (PFNfromEntry (entry), pmrd->dwPgProt);
                pmrd->fFlushTLB = TRUE;
            }
            pmrd->cPagesExisting ++;
            
        } else {
            // not committed, try to get it from page tree
            
            if (!ptte) {
                ptte = PgTreeGetEntry (ppgtree, &pmrd->liOfst, NULL);
            }

            if (ptte && TreeEntryIsCommitted (*ptte)) {
                
                DWORD dwPFN = TreeEntry2PFN (*ptte);
                
                EnterCriticalSection (&PhysCS);
                VERIFY (DupPhysPage (dwPFN, NULL));
                LeaveCriticalSection (&PhysCS);
                TreeEntryIncCount (ptte);
                *pEntry = MakeCommittedEntry (dwPFN, pmrd->dwPgProt);
                pmrd->cPagesMapped ++;
            }
        }

        pmrd->liOfst.QuadPart += VM_PAGE_SIZE;
        pmrd->ptte             = PgtreeNextEntry (ppgtree, ptte);
    }
    
    // stop enumeration if past file size
    return !fPastEOF;
}

//
// map existing pages in page tree to VM. returns the number of pages mapped
//
// NOTE: This is the only function that creates a VM mapping of a view to page tree pages.
//
DWORD PageTreeMapRange (PPAGETREE ppgtree,
                        PPROCESS  pprc,
                        LPCVOID   pAddr, 
                        const ULARGE_INTEGER *pliOfst,
                        DWORD     cPages,
                        DWORD     fProtect)
{
    MAPRANGEDATA    mrd;
    PPAGEDIRECTORY  ppdir;
    // write access to atomic map require serializing with flushes or we can flush unatomic data
    BOOL            fTakeFlushLock = (MAP_ATOMIC & ppgtree->wFlags) && ((PAGE_READWRITE|PAGE_EXECUTE_READWRITE) & fProtect);
    
    DEBUGCHK (IsPageAligned (pAddr) && IsPageAligned (pliOfst->LowPart));

    mrd.liOfst         = *pliOfst;
    mrd.ppgtree        = ppgtree;
    mrd.pprc           = pprc;
    mrd.cPagesMapped   = 0;
    mrd.cPagesExisting = 0;
    mrd.ptte   = NULL;
    mrd.dwPgProt       = PageParamFormProtect (pprc, fProtect, (DWORD) pAddr);
    mrd.fFlushTLB      = FALSE;

    if (fTakeFlushLock) {
        AcquireFlushLock (ppgtree);
    }

    LockPgTree (ppgtree);

    ppdir = LockVM (pprc);

    DEBUGCHK (ppdir);

    Enumerate2ndPT (ppdir, (DWORD) pAddr, cPages, FALSE, EnumMapRange, &mrd);

    if (mrd.fFlushTLB) {
        InvalidatePages (pprc, (DWORD) pAddr, cPages);
    }

    UnlockVM (pprc);

    UnlockPgTree (ppgtree);

    if (fTakeFlushLock) {
        ReleaseFlushLock (ppgtree);
    }
    
    if (mrd.cPagesMapped) {
        UpdateMemoryStatistic (PageTreeUsePool(ppgtree)? &pprc->msSharePaged : &pprc->msShareNonPaged, mrd.cPagesMapped);
    }

    return mrd.cPagesMapped + mrd.cPagesExisting;
    
}

typedef struct {
    ULARGE_INTEGER  liOfst;
    PPAGETREE       ppgtree;
    PTreePTEntry    ptte;
    PTreePTEntry    ptteParent;
} TREEFILLDATA, *PTREEFILLDATA;

//
// EnumTreeFill - enumeration function to fill a tree. Only used for non-pageable mapfiles
//
BOOL EnumTreeFill (PPTENTRY pEntry, LPVOID pData)
{
    PTREEFILLDATA ptfd = (PTREEFILLDATA) pData;
    DWORD        dwPFN = PFNfromEntry (*pEntry);
    
    DEBUGCHK (dwPFN);

    if (!ptfd->ptte) {
        ptfd->ptte = PgTreeGetEntry (ptfd->ppgtree, &ptfd->liOfst, &ptfd->ptteParent);
        // we just created all the tree tables. The call to PgTreeGetEntry should never fail.
        PREFAST_DEBUGCHK (ptfd->ptte);
    }

    DEBUGCHK (!TreeEntryIsCommitted (*ptfd->ptte));

    // clear the PTE of VM
    *pEntry = MakeReservedEntry (0);

    // move the page into the tree table
    PgtreeCommitPage (ptfd->ppgtree, ptfd->ptte, ptfd->ptteParent, dwPFN);

    // move on to next page
    ptfd->liOfst.LowPart += VM_PAGE_SIZE;
    ptfd->ptte            = PgtreeNextEntry (ptfd->ppgtree, ptfd->ptte);

    return TRUE;    // keep enumerating
}

//
// CreateTreeTables - create all the tree tables of a page tree for a given tree size. Only used for non-pageable mapfiles
//
BOOL CreateTreeTables (PPAGETREE ppgtree, DWORD cbSize)
{
    BOOL fRet = TRUE;

    if (cbSize > VM_PAGE_SIZE) {
        PTreePTEntry    ptte;
        ULARGE_INTEGER  liOfst;
        liOfst.QuadPart = cbSize;

        // create tables for offsets 4M and above
        while (fRet && (liOfst.QuadPart > SIZE_4M)) {
            liOfst.LowPart = PAGEALIGN_DOWN (liOfst.LowPart-1);
            while (fRet && !PgTreeGetEntry (ppgtree, &liOfst, &ptte)) {
                fRet = WaitForPagingMemory (g_pprcNK, PAGEIN_RETRY_MEMORY);
            }
            liOfst.LowPart -= SIZE_4M;
        }

        // now create the table for the 1st 4M
        liOfst.LowPart = VM_PAGE_SIZE;
        while (fRet && !PgTreeGetEntry (ppgtree, &liOfst, &ptte)) {
            fRet = WaitForPagingMemory (g_pprcNK, PAGEIN_RETRY_MEMORY);
        }
    }
    return fRet;
}

//
// PageTreeFill - fill a non-pageable page tree with the content we just read from file.
//
BOOL PageTreeFill (PPAGETREE ppgtree, LPCVOID pAddr, DWORD cbSize)
{
    // before we take the VM lock, we need to make sure all the tree tables are
    // allocated. Otherwise we can end up wating for memory while holding VM lock.
    // NOTE: we don't need to take the tree lock here as the page tree is just created
    //       and there cannot be any view for this tree. i.e. we have exclusive access
    //       to the page tree.
    BOOL fRet = CreateTreeTables (ppgtree, cbSize);

    DEBUGCHK (IsKModeAddr ((DWORD) pAddr));
    DEBUGCHK (cbSize == ppgtree->liSize.QuadPart);

    if (fRet) {
        TREEFILLDATA tfd = {0};
    
        tfd.ppgtree = ppgtree;

        LockVM (g_pprcNK);

        Enumerate2ndPT (g_ppdirNK, (DWORD) pAddr, PAGECOUNT (cbSize), FALSE, EnumTreeFill, &tfd);

        UnlockVM (g_pprcNK);
    }

    return fRet;    
}


