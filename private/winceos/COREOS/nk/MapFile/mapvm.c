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

// Avoid coding errors
#undef VM_NUM_PT_ENTRIES

// For debugging purposes
struct {
    DWORD NumDataPages;
    DWORD NumPageTables;
} g_MapVMGlobals;


// Callback function used with WalkPageTree
typedef BOOL (*PFN_MapPTEnumFunc) (LPVOID pEnumData, PTENTRY* pPTE, PTENTRY* pParentPTE, WORD height);

//
// Flags used with WalkPageTree
//

// Stop enumeration whenever encountering an uncommitted entry.  Can be used in
// combination with MAP_PT_WALK_CALLBACK_IF_UNCOMMITTED; in that case, the
// callback function will be invoked, and the enumeration will only fail if
// the callback returns FALSE.
#define MAP_PT_WALK_FAIL_IF_UNCOMMITTED        0x00000001

// Invoke the callback function on uncommitted entries.  If the callback
// returns TRUE it must have committed the entry!
#define MAP_PT_WALK_CALLBACK_IF_UNCOMMITTED    0x00000002


//------------------------------------------------------------------------------
// Physical page management routines
//------------------------------------------------------------------------------

static __inline MAPPageTable* MapAllocPT ()
{
    MAPPageTable* pTable = GrabOnePage (PM_PT_ZEROED);
    DEBUGCHK (!pTable || IsKernelVa (pTable));
    if (pTable) {
        InterlockedIncrement ((LONG*)&g_MapVMGlobals.NumPageTables);
    }
    return pTable;
}

static __inline void MapFreePT (MAPPageTable* pTable)
{
    DEBUGCHK (pTable);
    DEBUGCHK (IsKernelVa (pTable));
    VERIFY (FreeKernelPage (pTable));  // Page refcnt should be 1
    InterlockedDecrement ((LONG*)&g_MapVMGlobals.NumPageTables);
}

// Free the physical refcount on a data page.  Remove it from the page pool.
static __inline void MapFreePage (DWORD dwPhysAddr, BOOL fUsePool)
{
    if (fUsePool) {
        PGPOOLFreePage (&g_FilePool, PA2PFN (dwPhysAddr));
    } else {
        FreePhysPage (PA2PFN (dwPhysAddr));
    }
}


//------------------------------------------------------------------------------
// Primary page tree operations
//------------------------------------------------------------------------------

__inline static void UnlockPageTree (PFSMAP pfsmap)
{
    LeaveCriticalSection (&pfsmap->pgtree.csVM);
}


__inline static BOOL LockPageTree (PFSMAP pfsmap)
{
    // If this check fails, we're probably calling a page tree function on a
    // mapfile that has no page tree, or that is not initialized.  It shouldn't happen.
    DEBUGCHK (MAP_PT_FLAGS_INITIALIZED & pfsmap->pgtree.flags);

    EnterCriticalSection (&pfsmap->pgtree.csVM);
    if (MAP_PT_FLAGS_INITIALIZED & pfsmap->pgtree.flags) {
        return TRUE;
    }
    // If pgtree is not initialized, there's probably no CS to leave, just in case
    LeaveCriticalSection (&pfsmap->pgtree.csVM);
    return FALSE;
}


// Return the height necessary to accommodate a file of the given size.
static WORD
TreeHeight (
    const ULARGE_INTEGER* pliFileSize
    )
{
    WORD height;

    for (height = 0; height < MAP_MAX_TREE_HEIGHT; height++) {
        if (pliFileSize->QuadPart <= TreeSizeAtHeight (height))
            return height;
    }
    
    return height;
}


//
// WalkPageTree: Recursively walk the entire tree, or the given range of the
// tree, and call the callback function on every data page and tree node.
//
static BOOL
WalkPageTree (
    PFN_MapPTEnumFunc pfnApply,          // Enumeration callback
    LPVOID            pEnumData,         // Data to pass to the callback
    PTENTRY*          pPTE,              // Current entry
    PTENTRY*          pParentPTE,        // Parent's entry (used if this entry is added/removed)
    WORD              height,            // Height of current entry
    const ULARGE_INTEGER* pliStartOffset,// Offset from this part of the tree.  May be null.
    DWORD*            pcbSizeRemaining,  // Amount of data left to enumerate.  May be null.
    DWORD             flags              // MAP_PT_WALK_*
    )
{
    DEBUGCHK (!pliStartOffset || !(pliStartOffset->LowPart & VM_PAGE_OFST_MASK));  // page-aligned

    if (!MapEntryIsCommitted (*pPTE)) {
        // Non-committed entry.  The callback might commit the entry for us.
        if (!(MAP_PT_WALK_CALLBACK_IF_UNCOMMITTED & flags)
            || !pfnApply (pEnumData, pPTE, pParentPTE, height)) {
            
            // If we're keeping track of a size-remaining value, reduce it by
            // the amount of uncommitted space that we are jumping.
            if (pcbSizeRemaining) {
                // Jump the space, but count it into the total
                ULONGLONG cbJump = TreeSizeAtHeight (height);
                if (pliStartOffset) {
                    // Jumping over the total minus the start offset
                    DEBUGCHK (cbJump >= TreeOffsetAtHeight (pliStartOffset->QuadPart, height));
                    cbJump -= TreeOffsetAtHeight (pliStartOffset->QuadPart, height);

                    // Start offset is always page-aligned, but g_TSAH[6] is not.
                    // This can lead to cbJump not page-aligned.
                    cbJump = PAGEALIGN_UP (cbJump);
                }
                if (*pcbSizeRemaining > cbJump) {
                    DEBUGCHK (0 == (cbJump >> 32));
                    *pcbSizeRemaining -= (DWORD) cbJump;
                    DEBUGCHK ((int) (*pcbSizeRemaining) >= 0);
                } else {
                    *pcbSizeRemaining = 0;
                }
            }
            
            return (MAP_PT_WALK_FAIL_IF_UNCOMMITTED & flags) ? FALSE : TRUE;
        }
        
        // If pfnApply returned TRUE on an uncommitted entry, it must have
        // committed the entry.
        DEBUGCHK (MapEntryIsCommitted (*pPTE));
    }

    // Committed entries -- apply the callback to all the entries in a node
    // before applying it to the node itself.
    if (height > 0) {
        // The entry is a tree node
        MAPPageTable* pTable = MapEntry2PTBL (*pPTE);
        WORD idx;
        
        // Depth first walk.  Walk all the nodes below this node first, then
        // call the callback function on this node.

        // If there's a start offset, then start there.  Otherwise start at
        // the beginning.  Either way, go until *pcbSizeRemaining is zero.
        if (pliStartOffset) {
            idx = TreeNodeIndex (pliStartOffset->QuadPart, height);
            DEBUGCHK (idx < MAP_NUM_PT_ENTRIES);
        } else {
            idx = 0;
        }

        // Walk the nodes under this one, using a range if necessary
        while ((idx < MAP_NUM_PT_ENTRIES)
               && (!pcbSizeRemaining || (*pcbSizeRemaining > 0))) {

            PREFAST_ASSUME(pTable);
            
            if (!WalkPageTree (pfnApply, pEnumData,
                               &pTable->pte[idx], pPTE, height - 1,
                               pliStartOffset, pcbSizeRemaining, flags)) {
                return FALSE;
            }
            
            idx++;

            // Only pass pliStartOffset on the first call.  After that, keep
            // going until size-remaining is depleted.
            pliStartOffset = NULL;
        }
    
    } else {
        // The entry is a data page
        
        // If we're keeping track of a size-remaining value, reduce it by
        // the committed page we are processing.
        if (pcbSizeRemaining) {
            (*pcbSizeRemaining) -= VM_PAGE_SIZE;
            DEBUGCHK ((int) (*pcbSizeRemaining) >= 0);
        }
    }
    
    return pfnApply (pEnumData, pPTE, pParentPTE, height);
}


//------------------------------------------------------------------------------
// Helper functions used with the WalkPageTree enumerator
//------------------------------------------------------------------------------

// Update the count inside the parent tree node whenever adding or removing a
// page from the tree.
static __inline BOOL
UpdateParentCounter (
    PTENTRY* pParentPTE,    // Entry to update
    int increment           // Value to add/remove
    )
{
    LPVOID ParentPointer = MapEntry2PTBL (*pParentPTE);
    WORD   ParentCounter = MapEntry2Count (*pParentPTE);

    // Safety check the counter change.  ParentCounter is unsigned so underflow
    // will be caught here too.
    ParentCounter += (WORD)increment;
    if (ParentCounter <= MAP_NUM_PT_ENTRIES) {  // Parent is always a tree node
        *pParentPTE = MapMakeCommittedEntry (ParentPointer, ParentCounter);
        return TRUE;
    }
    
    DEBUGCHK (0);  // Should not get here
    return FALSE;
}


typedef struct {
    BOOL fUsePool;   // Whether to give pages to the page pool or physical mem
    BOOL fFreeAll;   // Whether to free all pages
} FreePage_EnumData;

//
// FreePage: WalkPageTree callback to free this part of the page tree.  This
// function is used to free tree pages after the last view to them is unmapped,
// and to free tree pages when the mapfile is being destroyed.
//
static BOOL
FreePage (
    LPVOID   pEnumData,     // Pointer to a FreePage_EnumData struct
    PTENTRY* pPTE,          // Current entry
    PTENTRY* pParentPTE,    // Parent's entry (used if this entry is removed)
    WORD     height
    )
{
    FreePage_EnumData* pData = (FreePage_EnumData*) pEnumData;

    DEBUGCHK (MapEntryIsCommitted (*pPTE));

    if (height > 0) {
        // The entry is a tree node
        MAPPageTable* pTable = MapEntry2PTBL (*pPTE);
        WORD count = MapEntry2Count (*pPTE);
        
        // If freeing everything, the table should now be empty
        DEBUGCHK (!pData->fFreeAll || (0 == count));

        if (0 == count) {
            // The node is now empty, so free it
            MapFreePT (pTable);
            *pPTE = MapMakeReservedEntry ();
            VERIFY (UpdateParentCounter (pParentPTE, -1));
        }
    } else {
        // The entry is a data page
        DWORD dwPhysAddr = MapEntry2PhysAddr (*pPTE);
        WORD  count = MapEntry2Count (*pPTE);
        
        // The page is being removed from a view, or a non-pageable mapping is
        // being destroyed.  Decrement the refcount on the page.  Free it if it
        // is the last refcount or if we're freeing all pages.
        if ((count > 1) && !pData->fFreeAll) {
            // There are still references left -- decrement the refcount
            count--;
            *pPTE = MapMakeCommittedEntry (dwPhysAddr, count);
        } else {
            // This was the last reference to the page
            
            // The physical refcount to the page may still be >1, for example
            // if someone VirtualCopied the page from the view to another
            // virtual address.  But it will no longer count as part of a mapfile.
            
            MapFreePage (dwPhysAddr, pData->fUsePool);
            *pPTE = MapMakeReservedEntry ();
            VERIFY (UpdateParentCounter (pParentPTE, -1));
            InterlockedDecrement ((LONG*)&g_MapVMGlobals.NumDataPages);
        }
    }

    return TRUE;
}


typedef struct {
    DWORD  dwPhysNewPage;   // physical address of the New page
    BOOL   fSuccess;        // Used to return whether the addition was successful
    WORD   InitialCount;    // Page use counter (0 if pageable and 1 if non-pageable)
} AddPage_EnumData;

//
// AddPage: WalkPageTree callback to add a page to the tree.  Also
// adds tree nodes as necessary.
//
static BOOL
AddPage (
    LPVOID   pEnumData,     // Pointer to an AddPage_EnumData struct
    PTENTRY* pPTE,          // Current entry
    PTENTRY* pParentPTE,    // Parent's entry (used if this entry is added)
    WORD     height
    )
{
    AddPage_EnumData* pData;

    // Can only add if the entry is not already committed
    if (MapEntryIsCommitted (*pPTE)) {
        DEBUGCHK (0 == height);  // Should not reach here on tree nodes
        return FALSE;
    }

    if (height > 0) {
        // The entry is a tree node
        MAPPageTable* pTable = MapAllocPT ();
        if (!pTable) {
            // Some other tree nodes might have been added before reaching this
            // failure.  They won't be cleaned up immediately, but they will
            // eventually be cleaned up by MapVMDecommit or MapVMCleanup.
            return FALSE;
        }
        
        // count=0 since no node entries are committed yet
        *pPTE = MapMakeCommittedEntry (pTable, 0);
        VERIFY (UpdateParentCounter (pParentPTE, +1));
        
        return TRUE;
    }
    
    // The entry is a data page -- reached the location where we need to insert
    // the new data.  Move the physical directly from the VM page table and free
    // the source. This way we don't need to touch go through physical memory 
    // manager to acquire extra locks.
    pData = (AddPage_EnumData*) pEnumData;

    // Add the page with count = 1 to account for the current view.  If the
    // page is coming from initialization of a non-pageable mapping, then
    // there may not actually be a view, but the refcount will be removed
    // when the non-pageable mapping is destroyed.
    *pPTE = MapMakeCommittedEntry (pData->dwPhysNewPage, pData->InitialCount);
    VERIFY (UpdateParentCounter (pParentPTE, +1));
    InterlockedIncrement ((LONG*)&g_MapVMGlobals.NumDataPages);
    
    pData->fSuccess = TRUE;
    
    // Return FALSE to stop enumeration
    return FALSE;
}


typedef struct {
    PPROCESS pprc;
    LPVOID   pAddrInProc;
    DWORD    fProtect;
    BOOL     fSuccess;    // Used to return whether the copy was successful
} CopyPageToProcess_EnumData;

//
// CopyPageToProcess: WalkPageTree callback to VC a page from the tree to
// process VM.
//
static BOOL
CopyPageToProcess (
    LPVOID   pEnumData,     // Pointer to a CopyPageToProcess_EnumData struct
    PTENTRY* pPTE,
    PTENTRY* pParentPTE,    // ignored
    WORD     height
    )
{
    DEBUGCHK (MapEntryIsCommitted (*pPTE));
    if (0 == height) {
        // The entry is a data page
        CopyPageToProcess_EnumData* pData = (CopyPageToProcess_EnumData*) pEnumData;
        DWORD  dwPhysAddr = MapEntry2PhysAddr (*pPTE);
        WORD   count = MapEntry2Count (*pPTE);

        // Prevent the count of views that the page is paged into from reaching
        // the maximum.  If the maximum is reached, we must remove the page from
        // at least one view before it can be added to this view.
        if (count >= MAP_PT_MAX_COUNT-1) {
            DEBUGMSG (ZONE_MAPFILE, (TEXT("CopyPageToProcess: Page exceeded maximum view count!\r\n")));
            return FALSE;
        }
        
        pData->fSuccess = VMCopyPhysical (pData->pprc, (DWORD) pData->pAddrInProc, 
                                            PA2PFN(dwPhysAddr), 1, 
                                            PageParamFormProtect (pData->pprc, pData->fProtect, (DWORD) pData->pAddrInProc),
                                            TRUE);


        if (pData->fSuccess) {

            // Increment the count of views that the page is paged into
            count++;
            DEBUGCHK (count < MAP_PT_MAX_COUNT);  // Safety checked above
            *pPTE = MapMakeCommittedEntry (dwPhysAddr, count);
        }

        // Advance iteration over the source memory
        pData->pAddrInProc = (LPVOID) ((DWORD) pData->pAddrInProc + VM_PAGE_SIZE);
        
        DEBUGMSG (ZONE_MAPFILE && !pData->fSuccess,
                  (TEXT("CopyPageToProcess: VMCopy failed, pid=%8.8lx addr=%8.8lx page=%8.8lx protect=%8.8lx\r\n"),
                   pData->pprc->dwId, pData->pAddrInProc, dwPhysAddr, pData->fProtect));

        return pData->fSuccess;
    }
    
    // Otherwise keep enumerating
    return TRUE;
}


#ifdef DEBUG
typedef struct {
    DWORD  NumDataPages;
    DWORD  NumPageTables;
    PFSMAP pfsmap;
} ValidatePage_EnumData;

//
// ValidatePage: WalkPageTree callback to sanity-check the page tree.
//
static BOOL
ValidatePage (
    LPVOID   pEnumData,     // Pointer to a ValidatePage_EnumData struct
    PTENTRY* pPTE,
    PTENTRY* pParentPTE,    // ignored
    WORD     height
    )
{
    ValidatePage_EnumData* pData = (ValidatePage_EnumData*) pEnumData;
    WORD count;

    DEBUGCHK (MapEntryIsCommitted (*pPTE));

    count = MapEntry2Count (*pPTE);
    if (height > 0) {
        // The entry is a tree node
        MAPPageTable* pTable = MapEntry2PTBL (*pPTE);
        
        DEBUGCHK (pTable && IsKernelVa (pTable));
        DEBUGCHK (count && (count <= MAP_NUM_PT_ENTRIES));
        
        pData->NumPageTables++;
        DEBUGCHK (pData->NumPageTables <= g_MapVMGlobals.NumPageTables);
    
    } else {
        // The entry is a data page
        DEBUGCHK (MapEntry2PhysAddr (*pPTE));
        DEBUGCHK (count < MAP_PT_MAX_COUNT);
        
        // Atomic & cache maps only have 1 view to any page
        DEBUGCHK (!(pData->pfsmap->dwFlags & (MAP_ATOMIC | MAP_FILECACHE)) || (1 >= count));
        
        pData->NumDataPages++;
        DEBUGCHK (pData->NumDataPages <= g_MapVMGlobals.NumDataPages);
    }
    
    return TRUE;
}
#endif // DEBUG


//------------------------------------------------------------------------------
// Map VM functionality
//------------------------------------------------------------------------------

//
// MapVMInit: Initialize a page tree for a newly opened file
//
BOOL
MapVMInit (
    PFSMAP pfsmap
    )
{
    // Direct-ROM mapfiles should not get here
    DEBUGCHK (!(pfsmap->dwFlags & MAP_DIRECTROM));

    DEBUGCHK (!(MAP_PT_FLAGS_INITIALIZED & pfsmap->pgtree.flags));

    // The page tables are only allocated on demand as needed
    pfsmap->pgtree.RootEntry = MapMakeReservedEntry ();
    pfsmap->pgtree.height = TreeHeight (&pfsmap->liSize);
    InitializeCriticalSection (&pfsmap->pgtree.csVM);
    DEBUGCHK (pfsmap->pgtree.csVM.hCrit);
    pfsmap->pgtree.flags |= MAP_PT_FLAGS_INITIALIZED;
    
    DEBUGMSG (ZONE_MAPFILE, (TEXT("MapVMInit: map %8.8lx size %8.8lx %8.8lx ==> height %u\r\n"),
                             pfsmap, pfsmap->liSize.HighPart, pfsmap->liSize.LowPart,
                             pfsmap->pgtree.height));
    
    return TRUE;
}


//
// MapVMCleanup: Delete the page tree as a file is being closed
//
void
MapVMCleanup (
    PFSMAP pfsmap
    )
{
    // Direct-ROM mapfiles should not get here
    DEBUGCHK (!(MAP_DIRECTROM & pfsmap->dwFlags));

    if (MAP_PT_FLAGS_INITIALIZED & pfsmap->pgtree.flags) {
        if (LockPageTree (pfsmap)) {
            PTENTRY RootEntry = pfsmap->pgtree.RootEntry;
            WORD height = pfsmap->pgtree.height;
    
            DEBUGLOG (ZONE_MAPFILE, MapVMValidate (pfsmap));

            pfsmap->pgtree.RootEntry = MapMakeReservedEntry();
            pfsmap->pgtree.height = 0;
            pfsmap->pgtree.flags &= ~MAP_PT_FLAGS_INITIALIZED;
            UnlockPageTree (pfsmap);
            
            // Walk the entire tree using the FreePage callback
            if (MapEntryIsCommitted (RootEntry)) {
                FreePage_EnumData data;
                PTENTRY FalseParentEntry;
                
                // There should only be pages left in the tree if this is a
                // non-pageable mapping or a RAM-backed mapping.  If a pageable
                // mapping has pages left, then somehow a page refcount was
                // wrong and the page was left in the tree after all views were
                // closed.  That would be a waste of RAM, but it would still
                // get properly cleaned up now.
                DEBUGCHK ((!pfsmap->phdFile && !(pfsmap->dwFlags & MAP_FILECACHE)) // RAM-backed
                          || !(pfsmap->dwFlags & (MAP_PAGEABLE | MAP_DIRECTROM))); // or non-pageable

                // Use the pool if file-backed and pageable, or if file cache
                data.fUsePool = ((pfsmap->phdFile && (pfsmap->dwFlags & MAP_PAGEABLE))
                                 || (pfsmap->dwFlags & MAP_FILECACHE)) ? TRUE : FALSE;
                data.fFreeAll = TRUE;

                FalseParentEntry = MapMakeCommittedEntry (0, 1);
                WalkPageTree (FreePage, &data, &RootEntry,
                              &FalseParentEntry, height, NULL, NULL, 0);
                DEBUGCHK (!MapEntryIsCommitted (RootEntry));
                DEBUGCHK (0 == MapEntry2Count(FalseParentEntry));
            }
        }
        DeleteCriticalSection (&pfsmap->pgtree.csVM);
    }
}


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
    )
{
    BOOL   fRet = FALSE;
    
    // The fsmap or at least one of its views must have been locked during this
    // operation, to prevent the fsmap from being destroyed underneath us!
    
    DEBUGMSG (ZONE_MAPFILE, (L"MapVMAddPage: Add page %8.8lx to map %8.8lx offset %8.8lx %8.8lx\r\n",
              pPage, pfsmap, liFileOffset.HighPart, liFileOffset.LowPart));

    if (LockPageTree (pfsmap)) {
        AddPage_EnumData data;
        PTENTRY ignored = 0;
        
        DEBUGCHK (!(liFileOffset.LowPart & VM_PAGE_OFST_MASK));  // page-aligned
        DEBUGCHK (liFileOffset.QuadPart <= PAGEALIGN_UP(pfsmap->liSize.QuadPart) - VM_PAGE_SIZE);
        
        // Get the static-mapped version of the pointer
        data.dwPhysNewPage = PFN2PA (GetPFN (pPage));
        data.InitialCount  = InitialCount;
        
        // Walk the tree using the AddPage callback and the range
        data.fSuccess = FALSE;
        WalkPageTree (AddPage, &data, &pfsmap->pgtree.RootEntry,
                      &ignored, pfsmap->pgtree.height, &liFileOffset, NULL,
                      MAP_PT_WALK_CALLBACK_IF_UNCOMMITTED | MAP_PT_WALK_FAIL_IF_UNCOMMITTED);
        fRet = data.fSuccess;
        
        DEBUGLOG (ZONE_MAPFILE, MapVMValidate (pfsmap));

        UnlockPageTree (pfsmap);
    } else {
        fRet = FALSE;
    }

    return fRet;
}


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
    )
{
    BOOL fRet = FALSE;
    
    // The fsmap or at least one of its views must have been locked during this
    // operation, to prevent the fsmap from being destroyed underneath us!

    DEBUGMSG (ZONE_MAPFILE, (L"MapVMCopy: Copy from map %8.8lx offset %8.8lx %8.8lx to pid %8.8lx addr %8.8lx, size %8.8lx\r\n",
              pfsmap, liFileOffset.HighPart, liFileOffset.LowPart,
              pprcDest->dwId, pAddrInProc, cbSize));

    DEBUGCHK (!((DWORD)pAddrInProc & VM_PAGE_OFST_MASK));

    if (LockPageTree (pfsmap)) {
        CopyPageToProcess_EnumData data;
        PTENTRY ignored = 0;

        cbSize = PAGEALIGN_UP (cbSize);

        DEBUGCHK (!(liFileOffset.LowPart & VM_PAGE_OFST_MASK));  // page-aligned
        DEBUGCHK (liFileOffset.QuadPart <= PAGEALIGN_UP(pfsmap->liSize.QuadPart) - cbSize);
        
        // Walk the tree using the CopyPageToProcess callback and the range
        data.pprc        = pprcDest;
        data.pAddrInProc = pAddrInProc;
        data.fProtect    = fProtect;
        data.fSuccess    = FALSE;
        WalkPageTree (CopyPageToProcess, &data,
                      &pfsmap->pgtree.RootEntry, &ignored,
                      pfsmap->pgtree.height, &liFileOffset, &cbSize,
                      MAP_PT_WALK_FAIL_IF_UNCOMMITTED);
        fRet = data.fSuccess;
        DEBUGCHK (!fRet || (0 == cbSize));  // Should have scanned the whole range

        DEBUGLOG (ZONE_MAPFILE, MapVMValidate (pfsmap));
        
        UnlockPageTree (pfsmap);
    }
    
    return fRet;
}


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
    )
{
    BOOL fRet = FALSE;
    
    // The fsmap or at least one of its views must have been locked during this
    // operation, to prevent the fsmap from being destroyed underneath us!

    DEBUGMSG (ZONE_MAPFILE, (L"MapVMFill: Copy from pid %8.8lx addr %8.8lx to map %8.8lx, size %8.8lx\r\n",
              pprcSrc->dwId, pAddrInProc, pfsmap, cbSize));

    DEBUGCHK (!((DWORD)pAddrInProc & VM_PAGE_OFST_MASK));
    DEBUGCHK ((ULONGLONG) cbSize == pfsmap->liSize.QuadPart);

    if (LockPageTree (pfsmap)) {
        // Copying from process to mapfile page tree.  VMMapMove will call
        // MapVMAddPage on each page to add it to the tree.  Since we hold the
        // page tree VM critsec it is safe for the VM code to call MapVMAddPage
        // while holding the process' VM CS.
        fRet = VMMapMove (pfsmap, pprcSrc, pAddrInProc, PAGEALIGN_UP (cbSize));

        DEBUGLOG (ZONE_MAPFILE, MapVMValidate (pfsmap));

        UnlockPageTree (pfsmap);
    }
    
    return fRet;
}


//
// MapVMFreePage: Decrement the view reference count for the data page at the
// specified position in the page tree.  Frees the page if it is no longer part
// of any views.  Used to release memory when a view is unmapped or when a page
// is removed from a view (used during the process of MapVMDecommit).
//
BOOL
MapVMFreePage (
    PFSMAP   pfsmap,
    ULARGE_INTEGER liFileOffset
    )
{
    BOOL fRet = FALSE;

    // The fsmap or at least one of its views must have been locked during this
    // operation, to prevent the fsmap from being destroyed underneath us!
    
    DEBUGMSG (ZONE_MAPFILE, (L"MapVMFreePage: Free page of map %8.8lx offset %8.8lx %8.8lx\r\n",
              pfsmap, liFileOffset.HighPart, liFileOffset.LowPart));

    if (LockPageTree (pfsmap)) {
        DEBUGCHK (MapEntryIsCommitted (pfsmap->pgtree.RootEntry));
        if (MapEntryIsCommitted (pfsmap->pgtree.RootEntry)) {
            FreePage_EnumData data;
            PTENTRY ignored = MapMakeCommittedEntry (0, 1);
            DWORD   cbSize = VM_PAGE_SIZE;

            DEBUGCHK (0 == (liFileOffset.LowPart % VM_PAGE_SIZE));
            DEBUGCHK (liFileOffset.QuadPart <= PAGEALIGN_UP(pfsmap->liSize.QuadPart) - VM_PAGE_SIZE);

            // Use the pool if file-backed and pageable, or if file cache
            data.fUsePool = ((pfsmap->phdFile && (pfsmap->dwFlags & MAP_PAGEABLE))
                             || (pfsmap->dwFlags & MAP_FILECACHE)) ? TRUE : FALSE;
            data.fFreeAll = FALSE;

            // Walk the tree using the FreePage callback and the range
            fRet = WalkPageTree (FreePage, &data,
                                 &pfsmap->pgtree.RootEntry, &ignored,
                                 pfsmap->pgtree.height, &liFileOffset, &cbSize,
                                 MAP_PT_WALK_FAIL_IF_UNCOMMITTED);
            DEBUGCHK (fRet);  // Should not fail
        }

        DEBUGLOG (ZONE_MAPFILE, MapVMValidate (pfsmap));

        UnlockPageTree (pfsmap);
    }
    
    return fRet;
}


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
    )
{
    BOOL fRet = FALSE;
    
    // The fsmap or at least one of its views must have been locked during this
    // operation, to prevent the fsmap from being destroyed underneath us!

    DEBUGMSG (ZONE_MAPFILE, (L"MapVMDecommit: Decommit from pid %8.8lx addr %8.8lx to map %8.8lx, size %8.8lx\r\n",
              pprc->dwId, pAddrInProc, pfsmap, cbSize));

    DEBUGCHK (!(MAP_DIRECTROM & pfsmap->dwFlags));
    DEBUGCHK (!((DWORD)pAddrInProc & VM_PAGE_OFST_MASK));

    if (LockPageTree (pfsmap)) {
        // VMMapDecommit will call MapVMFreePage on each page to decrement its
        // reference count in the tree, and free if it's the last reference.  
        // Since we hold the page tree VM critsec it is safe for the VM code to
        // call MapVMFreePage while holding the process' VM CS.
        fRet = VMMapDecommit (pfsmap, &liFileOffset, pprc, pAddrInProc, cbSize, DiscardDirty);
        
        // If there are no views, there should not be any pages left in the tree
        DEBUGCHK ((pfsmap->dwNumViews || pfsmap->pKrnBase || pfsmap->pUsrBase || (pfsmap->dwFlags & MAP_FILECACHE))
                  || !MapEntryIsCommitted (pfsmap->pgtree.RootEntry));

        DEBUGLOG (ZONE_MAPFILE, MapVMValidate (pfsmap));

        UnlockPageTree (pfsmap);
    }
    
    return fRet;
}


static BOOL RemoveTreeHeight (PFSMAP pfsmap, WORD NewHeight)
{
    DEBUGCHK (OwnCS (&pfsmap->pgtree.csVM));
    while (pfsmap->pgtree.height > NewHeight) {
        MAPPageTable* pTable = MapEntry2PTBL (pfsmap->pgtree.RootEntry);
        
        // Should be exactly one committed entry, the first one
        DEBUGCHK (pTable && (1 == MapEntry2Count (pfsmap->pgtree.RootEntry))
                  && MapEntryIsCommitted (pTable->pte[0]));
        
        pfsmap->pgtree.RootEntry = pTable->pte[0];
        MapFreePT (pTable);
        pfsmap->pgtree.height--;
    }
    return TRUE;
}


static BOOL AddTreeHeight (PFSMAP pfsmap, WORD NewHeight)
{
    WORD    OldHeight = pfsmap->pgtree.height;
#ifdef DEBUG
    PTENTRY OldRoot   = pfsmap->pgtree.RootEntry;
#endif

    DEBUGCHK (OwnCS (&pfsmap->pgtree.csVM));
    while (pfsmap->pgtree.height < NewHeight) {
        MAPPageTable* pTable = MapAllocPT ();
        if (pTable) {
            // The first entry in the new node will be the old root
            pTable->pte[0] = pfsmap->pgtree.RootEntry;
            pfsmap->pgtree.RootEntry = MapMakeCommittedEntry (pTable, 1);  // count=1 for existing entry
            pfsmap->pgtree.height++;
        } else {
            // Alloc failed -- roll back to the previous root
            VERIFY (RemoveTreeHeight (pfsmap, OldHeight));
            DEBUGCHK (OldRoot == pfsmap->pgtree.RootEntry);
            return FALSE;
        }
    }

    return TRUE;
}


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
    )
{
    WORD NewHeight;
    BOOL result = FALSE;
    
    DEBUGCHK (MAP_FILECACHE & pfsmap->dwFlags);  // Not fatal but unexpected

    if (liNewSize.QuadPart == pfsmap->liSize.QuadPart) {
        return TRUE;
    }
    
    NewHeight = TreeHeight (&liNewSize);
    if (LockPageTree (pfsmap)) {

        // Since pages must already be decommitted when we are shrinking
        // the tree, the only work we need to do is to adjust the tree height.
        if (!MapEntryIsCommitted (pfsmap->pgtree.RootEntry)) {
            // No work to do if the root is not committed
            pfsmap->pgtree.height = NewHeight;
            result = TRUE;
        } else if (liNewSize.QuadPart > pfsmap->liSize.QuadPart) {
            // Growing, add height as needed
            result = AddTreeHeight (pfsmap, NewHeight);
        } else {
            // Shrinking, remove height as necessary
            result = RemoveTreeHeight (pfsmap, NewHeight);
        }        
        
        // If the height change was successful, change the size too
        if (result) {
            pfsmap->liSize = liNewSize;
        }
        
        UnlockPageTree (pfsmap);
    }

    return result;
}


#ifdef DEBUG
//
// MapVMValidate: Sanity-check the page tree contents
//
BOOL
MapVMValidate (
    PFSMAP pfsmap
    )
{
    BOOL fRet = TRUE;
    // The fsmap or at least one of its views must have been locked during this
    // operation, to prevent the fsmap from being destroyed underneath us!
    
    DEBUGMSG (ZONE_MAPFILE, (L"MapVMValidate: Validate map %8.8lx page tree\r\n", pfsmap));

    if (MAP_PT_FLAGS_INITIALIZED & pfsmap->pgtree.flags) {
        if (LockPageTree (pfsmap)) {
            ValidatePage_EnumData data;
            PTENTRY ignored = 0;
    
            data.NumDataPages = 0;
            data.NumPageTables = 0;
            data.pfsmap = pfsmap;
            
            // Walk the entire tree using the ValidatePage callback
            WalkPageTree (ValidatePage, (VOID*) &data,
                          &pfsmap->pgtree.RootEntry, &ignored,
                          pfsmap->pgtree.height, NULL, NULL, 0);
    
            UnlockPageTree (pfsmap);
        
        } else {
            fRet = FALSE;
            DEBUGCHK (0);
        }
    }
    return fRet;
}
#endif // DEBUG
