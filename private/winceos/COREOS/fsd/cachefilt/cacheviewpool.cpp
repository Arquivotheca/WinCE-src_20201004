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

#include "cachefilt.hpp"
#include <celog.h>
#include <profiler.h>

PVOID* CacheViewPool_t::s_pFreePageList;
DWORD  CacheViewPool_t::s_NumPageListEntries;


//------------------------------------------------------------------------------
// PAGELIST OPERATIONS
// The PageList is used as a scratch buffer during the kernel view flush.
// Only the pool knows how big the list has to be.
//------------------------------------------------------------------------------


PVOID* CacheViewPool_t::AllocPageList ()
{
    PVOID* pPageList = (PVOID*) InterlockedExchangePointer (&s_pFreePageList, NULL);
    if (!pPageList) {
        pPageList = new PVOID[s_NumPageListEntries];
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CacheViewPool_t::AllocPageList, Alloc PageList 0x%08x\r\n", pPageList));
    }
    return pPageList;
}

void CacheViewPool_t::FreePageList (PVOID* pPageList)
{
    if (pPageList) {
        pPageList = (PVOID*) InterlockedExchangePointer (&s_pFreePageList, pPageList);
        if (pPageList) {
            DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CacheViewPool_t::FreePageList, Free PageList 0x%08x\r\n", pPageList));
            delete [] pPageList;
        }
    }
}


//------------------------------------------------------------------------------
// CACHEVIEW OPERATIONS
//------------------------------------------------------------------------------

__inline void CeLogView (
    CacheView_t* pView,
    WORD wID
    )
{
    if (ZONE_CELOG && IsCeLogZoneEnabled (CELZONE_DEMANDPAGE)) {
        CEL_CACHEVIEW cel;
        
        cel.liMapOffset = pView->Base.liMapOffset;
        cel.MapId = (DWORD) pView->Alloc.pSharedMap;
        cel.pBaseAddr = pView->Base.pBaseAddr;
        cel.cbSize = pView->Base.cbSize;
        cel.dwDirty = pView->Base.dwDirty;
        
        CeLogData (TRUE, wID, &cel, sizeof(CEL_CACHEVIEW),
                   0, CELZONE_DEMANDPAGE, 0, FALSE);
    }
}

BOOL
CacheViewPool_t::IncViewUseCount (
    CacheView_t* pView
    )
{
    DEBUGCHK (OwnPoolLock ());
    if (pView->wUseCount < s_MaxViewUseCount) {
        pView->wUseCount++;
        return TRUE;
    }
    DEBUGCHK (0);  // Probably a leak
    return FALSE;
}


// Returns TRUE if the use-count is now zero
BOOL
CacheViewPool_t::DecViewUseCount (
    CacheView_t* pView
    )
{
    DEBUGCHK (OwnPoolLock ());
    if (pView->wUseCount > 0) {
        pView->wUseCount--;
        return !pView->wUseCount;  // Return TRUE if the count is 0
    }
    DEBUGCHK (0);
    return FALSE;
}


void
CacheViewPool_t::LRUAdd (
    CacheView_t* pView
    )
{
    DEBUGCHK (OwnPoolLock ());
    
    // Should not be on LRU yet
    DEBUGCHK (!pView->Alloc.pLRUNewer && !pView->Alloc.pLRUOlder
              && (m_pLRUNewest != pView));

    pView->Alloc.pLRUNewer = NULL;
    pView->Alloc.pLRUOlder = m_pLRUNewest;
    if (m_pLRUNewest) {
        m_pLRUNewest->Alloc.pLRUNewer = pView;
    }
    m_pLRUNewest = pView;
    if (!m_pLRUOldest) {
        m_pLRUOldest = pView;
    }
}


void
CacheViewPool_t::LRURemove (
    CacheView_t* pView
    )
{
    DEBUGCHK (OwnPoolLock ());

    // Should be on LRU already
    DEBUGCHK (pView->Alloc.pLRUNewer || pView->Alloc.pLRUOlder
              || (m_pLRUNewest == pView));
    
    if (pView->Alloc.pLRUNewer)
        pView->Alloc.pLRUNewer->Alloc.pLRUOlder = pView->Alloc.pLRUOlder;
    if (pView->Alloc.pLRUOlder)
        pView->Alloc.pLRUOlder->Alloc.pLRUNewer = pView->Alloc.pLRUNewer;
    if (m_pLRUNewest == pView)
        m_pLRUNewest = pView->Alloc.pLRUOlder;
    if (m_pLRUOldest == pView)
        m_pLRUOldest = pView->Alloc.pLRUNewer;
    
    pView->Alloc.pLRUOlder = NULL;
    pView->Alloc.pLRUNewer = NULL;
}


// Take a view off the free list, if one is available, and mark it as being
// in use.  Returns NULL if there are no free views.
CacheView_t*
CacheViewPool_t::PopFreeView ()
{
    DEBUGCHK (OwnPoolLock ());

    CacheView_t* pView = m_pFirstFree;
    if (pView) {
        DEBUGCHK (!pView->wUseCount);
        DEBUGCHK (CACHE_VIEW_FREE & pView->wFlags);

        // Remove it from the free list
        m_pFirstFree = pView->Free.pNextFree;
        pView->Free.pNextFree = NULL;
        pView->wFlags &= ~CACHE_VIEW_FREE;

#ifdef DEBUG
        DEBUGCHK (m_DebugInfo.CurFree > 0);
        m_DebugInfo.CurFree--;
#endif
    }
    
    return pView;
}


// Mark a view as free and chain it on the free list.
void
CacheViewPool_t::PushFreeView (
    CacheView_t* pView
    )
{
#ifdef DEBUG
    DEBUGMSG (ZONE_VIEWPOOL,
              (L"CACHEFILT:CacheViewPool_t::PushFreeView, View 0x%08x\r\n", pView));
    DEBUGCHK (OwnPoolLock ());
    DEBUGCHK (!pView->wUseCount && !pView->wFlags);

    memset (&pView->Alloc, 0xCC, sizeof(pView->Alloc));
    
    DEBUGCHK (m_DebugInfo.CurFree < m_NumViews);
    m_DebugInfo.CurFree++;
#endif
    
    pView->wFlags |= CACHE_VIEW_FREE;

    // Add it to the free list
    pView->Free.pNextFree = m_pFirstFree;
    m_pFirstFree = pView;
}


void
CacheViewPool_t::UpdateViewSize (
    CacheView_t* pView,
    const ULARGE_INTEGER& CurFileSize
    )
{
    ULARGE_INTEGER BytesRemainingInFile;
    BytesRemainingInFile.QuadPart = CurFileSize.QuadPart - pView->Base.liMapOffset.QuadPart;
    if (BytesRemainingInFile.QuadPart < s_cbView) {
        DEBUGMSG (ZONE_VIEWPOOL, (L"CACHEFILT:CacheViewPool_t::UpdateViewSize, View 0x%08x size 0x%08x\r\n",
                                  pView, BytesRemainingInFile.LowPart));
        DEBUGCHK (BytesRemainingInFile.LowPart >= pView->Base.cbSize);  // growing only
        pView->Base.cbSize = BytesRemainingInFile.LowPart;
    } else {
        pView->Base.cbSize = s_cbView;
    }
}


void
CacheViewPool_t::AssignViewToFile (
    FSSharedFileMap_t* pMap,
    const ULARGE_INTEGER& CurFileSize,
    CacheView_t* pView,
    PVOID* pPageList,               // Scratch buffer for flushing the view
    const ULARGE_INTEGER& Offset,   // Offset of view from start of the file
    CacheView_t* pPrevInFile        // Pointer to the view preceding this one, may be NULL
    )
{
    DEBUGMSG (ZONE_VIEWPOOL, (L"CACHEFILT:CacheViewPool_t::AssignViewToFile, View 0x%08x file 0x%08x\r\n",
                              pView, pMap));
    DEBUGCHK (OwnPoolLock ());
    DEBUGCHK (!pView->Alloc.pSharedMap);  // Should not be assigned to a file

    // Initialize the view data
    pView->Base.liMapOffset = Offset;
    pView->Base.cbSize      = 0;  // Updated below
    pView->Base.dwDirty     = 0;
    pView->Alloc.pSharedMap = pMap;
    pView->Alloc.pLRUOlder  = NULL;
    pView->Alloc.pLRUNewer  = NULL;
    pView->Alloc.pPageList  = pPageList;

    // If the file ends partway through the view, modify the view size to match
    UpdateViewSize (pView, CurFileSize);
    
    // Chain onto the file view list after pPrevInFile
    if (pPrevInFile) {
        pView->Alloc.pNextInFile = pPrevInFile->Alloc.pNextInFile;
        pPrevInFile->Alloc.pNextInFile = pView;
    } else {
        pView->Alloc.pNextInFile = pMap->m_pViewList;
        pMap->m_pViewList = pView;
    }
    if (pView->Alloc.pNextInFile) {
        DEBUGCHK (pView->Alloc.pNextInFile->Alloc.pPrevInFile == pPrevInFile);
        pView->Alloc.pNextInFile->Alloc.pPrevInFile = pView;
    }
    pView->Alloc.pPrevInFile = pPrevInFile;

    CeLogView (pView, CELID_CACHE_ALLOCVIEW);
}


void
CacheViewPool_t::RemoveViewFromFile (
    CacheView_t* pView
    )
{
    DEBUGCHK (OwnPoolLock ());
    DEBUGCHK (!pView->wUseCount);  // The view should not be in use
    
    CeLogView (pView, CELID_CACHE_FREEVIEW);

    // Should be on file's view list
    DEBUGCHK (pView->Alloc.pSharedMap);
    DEBUGCHK (pView->Alloc.pNextInFile || pView->Alloc.pPrevInFile
              || (pView->Alloc.pSharedMap->m_pViewList == pView));
    
    if (pView->Alloc.pNextInFile)
        pView->Alloc.pNextInFile->Alloc.pPrevInFile = pView->Alloc.pPrevInFile;
    if (pView->Alloc.pPrevInFile)
        pView->Alloc.pPrevInFile->Alloc.pNextInFile = pView->Alloc.pNextInFile;
    if (pView->Alloc.pSharedMap->m_pViewList == pView)
        pView->Alloc.pSharedMap->m_pViewList = pView->Alloc.pNextInFile;
    
    pView->Alloc.pNextInFile = NULL;
    pView->Alloc.pPrevInFile = NULL;
    pView->Alloc.pSharedMap = NULL;
    pView->Base.liMapOffset.QuadPart = 0;

    FreePageList (pView->Alloc.pPageList);
    pView->Alloc.pPageList = NULL;
}


// Find the last view that precedes the given offset in the file.
// (It may not be at an adjacent file offset)
CacheView_t*
CacheViewPool_t::FindViewBeforeOffset (
    FSSharedFileMap_t* pMap,
    const ULARGE_INTEGER& FileOffset
    )
{
    // Check the front of the list
    if (!pMap->m_pViewList
        || (pMap->m_pViewList->Base.liMapOffset.QuadPart >= FileOffset.QuadPart)) {
        return NULL;
    }
    
    // Find the position in the middle of the list
    CacheView_t* pPrev = pMap->m_pViewList;
    while (pPrev->Alloc.pNextInFile) {
        if (pPrev->Alloc.pNextInFile->Base.liMapOffset.QuadPart >= FileOffset.QuadPart) {
            return pPrev;
        }
        pPrev = pPrev->Alloc.pNextInFile;
    }
    
    // Reached the end of the list without reaching the file offset
    return pPrev;
}


// Find the first view at or after the given offset in the file.
CacheView_t*
CacheViewPool_t::FindViewAfterOffset (
    FSSharedFileMap_t* pMap,
    const ULARGE_INTEGER& FileOffset
    )
{
    CacheView_t* pView = pMap->m_pViewList;
    while (pView) {
        if (pView->Base.liMapOffset.QuadPart >= FileOffset.QuadPart) {
            return pView;
        }
        pView = pView->Alloc.pNextInFile;
    }
    
    // Reached the end of the list without reaching the file offset
    return NULL;
}


//------------------------------------------------------------------------------
// CACHEVIEW POOL OBJECT
//------------------------------------------------------------------------------

CacheViewPool_t::CacheViewPool_t ()
{
    InitializeCriticalSection (&m_csPool);
    
    m_pVM = NULL;
    m_NumViews = 0;
    m_ViewArray = NULL;
    m_pLRUOldest = NULL;
    m_pLRUNewest = NULL;
    m_pFirstFree = NULL;

    s_NumPageListEntries = s_cbView / UserKInfo[KINX_PAGESIZE];

#ifdef DEBUG
    memset (&m_DebugInfo, 0, sizeof(m_DebugInfo));
#endif
}


CacheViewPool_t::~CacheViewPool_t ()
{
    // Should not be any allocated views remaining
    DEBUGCHK (!m_pLRUOldest && !m_pLRUNewest);
    if (m_pLRUOldest || m_pLRUNewest) {
        WalkAllFileViews (MAP_FLUSH_WRITEBACK | MAP_FLUSH_DECOMMIT_RO
                          | MAP_FLUSH_DECOMMIT_RW | MAP_FLUSH_HARD_FREE_VIEW);
        DEBUGCHK (!m_pLRUOldest && !m_pLRUNewest);
        m_pLRUOldest = NULL;
        m_pLRUNewest = NULL;
    }

    if (m_ViewArray) {
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:~CacheViewPool_t, Free descriptors 0x%08x\r\n", m_ViewArray));
        delete [] m_ViewArray;
        m_ViewArray = NULL;
    }
    if (m_pVM) {
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:~CacheViewPool_t, Free VM 0x%08x\r\n", m_pVM));
        VERIFY (VirtualFree ((VOID*) m_pVM, 0, MEM_RELEASE));
        m_pVM = NULL;
    }
        
    m_NumViews = 0;
    m_pFirstFree = NULL;

    DeleteCriticalSection (&m_csPool);
}


BOOL
CacheViewPool_t::Init (
    pfnNKAllocCacheVM pAllocCacheVM     // Kernel VM alloc function pointer
    )
{
    if (!m_csPool.hCrit) {
        DEBUGMSG (ZONE_VIEWPOOL | ZONE_ERROR,
                  (L"CACHEFILT:CacheViewPool_t::Init, !! Failed to initialize pool CS\r\n"));
        return FALSE;
    }
    if (m_pVM || m_ViewArray) {
        DEBUGCHK (0);
        return FALSE;
    }
    
    
    // Figure out the VM size to use
    LONG  result;
    DWORD type;
    DWORD cbVal = sizeof(DWORD);
    DWORD cbVirtual;
    WCHAR szCacheFiltKey[] = TEXT("System\\StorageManager\\CacheFilt");
    result = RegQueryValueEx (HKEY_LOCAL_MACHINE, TEXT("CacheVMSize"),
                              (DWORD*) szCacheFiltKey, &type,
                              (BYTE*) &cbVirtual, &cbVal);
    if ((ERROR_SUCCESS != result) || (REG_DWORD != type) || (sizeof(DWORD) != cbVal)) {
        cbVirtual = CACHE_DEFAULT_VM_SIZE;
    }
    if (cbVirtual < CACHE_MINIMUM_VM_SIZE) {
        cbVirtual = CACHE_MINIMUM_VM_SIZE;
    } else if (cbVirtual > CACHE_MAXIMUM_VM_SIZE) {
        cbVirtual = CACHE_MAXIMUM_VM_SIZE;
    }

    
    // Allocate the pool of VM, using the specified size if possible
    do {
        // Round up to view size boundary
        cbVirtual = (cbVirtual + s_cbView - 1) & ~(s_cbView - 1);

        // VirtualAlloc call passing flags filesys can't pass
        m_pVM = pAllocCacheVM (cbVirtual);
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CacheViewPool_t::Init, Alloc VM 0x%08x\r\n", m_pVM));
        if (m_pVM)
            break;
        
        // Failed, try a smaller VM size
        cbVirtual /= 2;
    
    } while (cbVirtual >= CACHE_MINIMUM_VM_SIZE);
    
    if (!m_pVM) {
        DEBUGMSG (ZONE_VIEWPOOL | ZONE_ERROR,
                  (L"CACHEFILT:CacheViewPool_t::Init, !! Failed to allocate VM, error=%u\r\n",
                   GetLastError ()));
        return FALSE;
    }
    
    DEBUGMSG (ZONE_INIT | ZONE_VIEWPOOL,
              (L"CACHEFILT:CacheViewPool_t::Init, Pool VM Addr=0x%08x Size=0x%08x\r\n",
               m_pVM, cbVirtual));
    
    // Initialize the view descriptors
    m_NumViews = cbVirtual / s_cbView;
    m_ViewArray = new CacheView_t[m_NumViews];
    DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CacheViewPool_t::Init, Alloc descriptors 0x%08x\r\n", m_ViewArray));
    if (!m_ViewArray) {
        DEBUGMSG (ZONE_VIEWPOOL | ZONE_ERROR,
                  (L"CACHEFILT:CacheViewPool_t::Init, !! Failed to allocate descriptors, error=%u\r\n",
                   GetLastError ()));
        return FALSE;
    }

    AcquirePoolLock ();  // PushFreeView expects CS ownership

    LPBYTE NextBase = (LPBYTE) m_pVM;
    for (DWORD index = 0; index < m_NumViews; index++) {
        m_ViewArray[index].Base.pBaseAddr = NextBase;
        m_ViewArray[index].Base.cbSize    = s_cbView;
        m_ViewArray[index].wUseCount      = 0;
        m_ViewArray[index].wFlags         = 0;
        NextBase += s_cbView;

        // Chain onto the free list
        PushFreeView (&m_ViewArray[index]);
    }

    ReleasePoolLock ();

    return TRUE;
}


// Displace an existing view.  Releases the pool CS temporarily!!
BOOL
CacheViewPool_t::FreeOldestView ()
{
    DEBUGCHK (OwnPoolLock ());

    // Else find a used view and displace it.  Take the oldest unlocked view.
    CacheView_t* pView = m_pLRUOldest;
    while (pView) {
        // There might be in-use views on the LRU if they are being flushed by someone else
        if (!pView->wUseCount) {
            // NOTE: FlushCacheView releases the pool lock temporarily.
            // But it will not free the view if it's locked in the meantime.
            DEBUGMSG (ZONE_VIEWPOOL, (L"CACHEFILT:CacheViewPool_t::FreeOldestView, Attempt to displace view 0x%08x from file 0x%08x\r\n",
                                      pView, pView->Alloc.pSharedMap));
            if (FlushCacheView (pView, MAP_FLUSH_WRITEBACK
                                       | MAP_FLUSH_DECOMMIT_RO
                                       | MAP_FLUSH_SOFT_FREE_VIEW, NULL)) {
#ifdef DEBUG
                DEBUGMSG (ZONE_VIEWPOOL,
                          (L"CACHEFILT:CacheViewPool_t::FreeOldestView, Displaced view 0x%08x\r\n", pView));
                m_DebugInfo.Displacements++;
#endif
                return TRUE;
            } else {
                // We let go of the pool lock, so the LRU might have changed.
                // But don't restart the loop since the flush could have failed
                // due to a persistent file system failure case (like disk full).
                DEBUGMSG (ZONE_VIEWPOOL,
                          (L"CACHEFILT:CacheViewPool_t::FreeOldestView, Failed to displace view 0x%08x\r\n", pView));
            }
        }
        // The flush failed -- move on to the next view
        pView = pView->Alloc.pLRUNewer;
    }
    
    return FALSE;
}


// Get a pointer to the view and map, given the address.  The view
// in-use count should already be incremented.
BOOL
CacheViewPool_t::GetView (
    DWORD  dwAddr,
    DWORD* pNKSharedFileMapId,
    CommonMapView_t** ppview
    )
{
    // The address may not be part of the cache, so check that first.
    DWORD VMOffset = dwAddr - (DWORD) m_pVM;
    if (VMOffset >= (m_NumViews * s_cbView)) {
        return FALSE;
    }
    
    DWORD ViewIndex = VMOffset / s_cbView;
    DEBUGCHK (ViewIndex <= m_NumViews);

    if ((m_ViewArray[ViewIndex].wFlags & CACHE_VIEW_FREE) ||
        (m_ViewArray[ViewIndex].wUseCount == 0)) {
        return FALSE;
    }
    
    *ppview = &m_ViewArray[ViewIndex].Base;
    *pNKSharedFileMapId = m_ViewArray[ViewIndex].Alloc.pSharedMap->m_NKSharedFileMapId;
    
    return TRUE;
}


// Attempts to lock the view at the requested offset of the file.
// Note: LockView/UnlockView are only for user level operations.  Asynchronous
// operations like write-back should use IncViewUseCount/DecViewUseCount
// instead.  Otherwise they'll end up adding/removing the view from the LRU
// multiple times.
BOOL
CacheViewPool_t::LockView (
    FSSharedFileMap_t* pMap,
    const ULARGE_INTEGER& CurFileSize,
    const ULARGE_INTEGER& StartFileOffset,  // View alignment is not required
    DWORD cbToLock,                         // Size may fall inside or outside the view
    BOOL  IsWrite,                          // Is the view about to be dirtied?
    LockedViewInfo& info,
    CacheView_t* pPrevView                  // Shortcut walk to new view; may not be valid
    )
{
    BOOL   result = FALSE;
    PVOID* pPageList = NULL;

    // Round down to the nearest view boundary
    ULARGE_INTEGER RealFileOffset = StartFileOffset;
    RealFileOffset.LowPart &= ~(s_cbView - 1);
    
    AcquirePoolLock ();
    
    
    //
    // First check if it's already chained on the map, and find the 
    // position where it should be chained.
    //
    
    CacheView_t* pView;
    CacheView_t* pPrev;
    do {
        // Optimization: Start from the previously accessed view.
        // pPrevView is a "hint" for where we might find the view preceding the
        // one being locked.  But pPrevView is not locked and may not point to
        // the previous view any longer.  So validate it before using it.
        if (pPrevView
            && !(pPrevView->wFlags & CACHE_VIEW_FREE)
            && (pPrevView->Alloc.pSharedMap == pMap)
            && (pPrevView->Base.liMapOffset.QuadPart + s_cbView == RealFileOffset.QuadPart)) {
            pPrev = pPrevView;
        } else {
            pPrev = FindViewBeforeOffset (pMap, RealFileOffset);
        }

        pView = pPrev ? pPrev->Alloc.pNextInFile : pMap->m_pViewList;
        
        // We must not write to any views unless we have a PageList for flushing them.
        // So allocate that first if we don't already have one to use.
        if (IsWrite
            && (!pView || (pView->Base.liMapOffset.QuadPart != RealFileOffset.QuadPart) || !pView->Alloc.pPageList)
            && ((pPageList = AllocPageList ()) == 0)) {
            // Out of memory!  Sleep a little and try again.
#ifdef DEBUG
            m_DebugInfo.LockFailures++;
#endif
            
            pView = NULL;
            ReleasePoolLock ();
            Sleep (100);
            AcquirePoolLock ();

        } else if (pView && (pView->Base.liMapOffset.QuadPart == RealFileOffset.QuadPart)) {
            // This view matching the current offset is already on the list.  Lock it.
            DEBUGMSG (ZONE_VIEWPOOL,
                      (L"CACHEFILT:CacheViewPool_t::LockView, Found existing view 0x%08x attached to file 0x%08x\r\n", pView, pMap));
            result = IncViewUseCount (pView);
            DEBUGCHK (result);
            LRURemove (pView);

            if (IsWrite) {
                if (!pView->Alloc.pPageList) {
                    DEBUGCHK (pPageList);
                    pView->Alloc.pPageList = pPageList;
                } else {
                    DEBUGCHK (!pPageList);
                }
            }
            
        // The view for the current offset isn't yet on the view list.
        // Grab a free view if possible.
        } else if (0 != (pView = PopFreeView ())) {
            // Lock the view and assign it to the map.
            DEBUGMSG (ZONE_VIEWPOOL,
                      (L"CACHEFILT:CacheViewPool_t::LockView, Popped free view 0x%08x\r\n", pView));
            result = IncViewUseCount (pView);
            DEBUGCHK (result);
            DEBUGCHK (!IsWrite || pPageList);  // All writes should receive a PageList
            AssignViewToFile (pMap, CurFileSize, pView, pPageList, RealFileOffset, pPrev);
#ifdef DEBUG
            m_DebugInfo.Allocations++;
#endif

        } else {
            // We're going to loop again so release the PageList
            if (pPageList) {
                FreePageList (pPageList);
                pPageList = NULL;
            }

            // Freeing a view involves releasing the pool lock.  After that pPrev will
            // no longer be valid, and the targeted view might have been added to the
            // file by someone else in the meantime too.  So free a view and then go
            // back to retry the lock.
            if (!FreeOldestView ()) {
                // Could not find a view to use!  Sleep a little and try again.
#ifdef DEBUG
                m_DebugInfo.LockFailures++;
                DEBUGCHK (0);  // Not fatal but bad for perf, should be rare
#endif
                
                ReleasePoolLock ();
                Sleep (100);
                AcquirePoolLock ();
            }
        }
    
    } while (!pView);

    if (result) {
        // If the file has grown, the view size may need to be updated.
        if (pView->Base.cbSize < s_cbView) {
            UpdateViewSize (pView, CurFileSize);
        }

        // Return a view info struct that reflects the exact file offset requested,
        // but if cbToLock is too large to fit, don't extend past the end of the view.
        info.pBaseAddr = (LPBYTE) (StartFileOffset.LowPart & (s_cbView - 1));  // Offset from start of real view
        info.cbView    = (pView->Base.cbSize - (DWORD) info.pBaseAddr);
        if (info.cbView > cbToLock) {
            info.cbView = cbToLock;
        }
        info.pBaseAddr = pView->Base.pBaseAddr + (DWORD) info.pBaseAddr;
        info.pView = pView;
    }
    
    ReleasePoolLock ();

    return result;
}


// Note: LockView/UnlockView are only for user level operations.  Asynchronous
// operations like write-back should use IncViewUseCount/DecViewUseCount
// instead.  Otherwise they'll end up adding/removing the view from the LRU
// multiple times.
void
CacheViewPool_t::UnlockView (
    FSSharedFileMap_t* pMap,
    const LockedViewInfo& info
    )
{
    AcquirePoolLock ();
    DecViewUseCount (info.pView);
    LRUAdd (info.pView);
    ReleasePoolLock ();
}


// Expects to own the pool CS on entry, and releases the CS in order to flush!
// Also, if FlushFlags says to free the view, then pView will not be valid
// after this call.  Use pNextView to iterate to the next view in the file.
BOOL
CacheViewPool_t::FlushCacheView (
    CacheView_t* pView,
    DWORD FlushFlags,
    CacheView_t** ppNextInFile      // Next view in the file after pView
    )
{
    DEBUGCHK (OwnPoolLock ());
    DEBUGCHK (!(pView->wFlags & CACHE_VIEW_FREE));

    // Freeing the view requires decommitting pages
    if (MAP_FLUSH_HARD_FREE_VIEW & FlushFlags) {
        FlushFlags |= (MAP_FLUSH_DECOMMIT_RO | MAP_FLUSH_DECOMMIT_RW);
    }
    if (MAP_FLUSH_SOFT_FREE_VIEW & FlushFlags) {
        FlushFlags |= (MAP_FLUSH_DECOMMIT_RO | MAP_FLUSH_FAIL_IF_COMMITTED);
    }

    CeLogView (pView, CELID_CACHE_FLUSHVIEW);

    DWORD PrevPriority = GetThreadPriority (GetCurrentThread());
    if (PrevPriority > THREAD_PRIORITY_NORMAL) {
       SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    }

    // Release the pool lock while flushing.  That avoids perf problems from
    // holding the lock too long, and avoids CS ordering violations.
    BOOL result = TRUE;
    BOOL InUse = TRUE;
    if (IncViewUseCount (pView)) {
        ReleasePoolLock ();
        DEBUGCHK (!OwnPoolLock ());  // No recursive acquire
    
        // 2-phase flush: write back without holding CS, decommit while holding CS.
        // Prevent race condition from page-ins after the flush, by decommitting
        // while holding the pool CS.  The view has to be locked in order to page-in,
        // so as long as we hold the only lock we can decommit and know there are no page-ins.
        DWORD DecommitFlags = FlushFlags & (MAP_FLUSH_DECOMMIT_RO | MAP_FLUSH_DECOMMIT_RW | MAP_FLUSH_FAIL_IF_COMMITTED);
        FlushFlags &= ~DecommitFlags;
        if (FlushFlags && pView->Alloc.pPageList) {  // No data to write back if no pagelist
            result = pView->Alloc.pSharedMap->FlushView (&pView->Base, FlushFlags,
                                                         pView->Alloc.pPageList);
        }

        AcquirePoolLock ();
        InUse = !DecViewUseCount (pView);

        if (DecommitFlags) {
            if (!pView->Alloc.pSharedMap->FlushView (&pView->Base, DecommitFlags,
                                                     NULL)) {
                result = FALSE;
            }
        }
    } else {
        result = FALSE;
    }

    if (PrevPriority > THREAD_PRIORITY_NORMAL) {
        SetThreadPriority (GetCurrentThread(), PrevPriority);
    }

    // Record the pNext now that we hold the CS again, before freeing pView
    if (ppNextInFile) {
        *ppNextInFile = pView->Alloc.pNextInFile;
    }

    if ((MAP_FLUSH_HARD_FREE_VIEW & FlushFlags) ||
           (MAP_FLUSH_SOFT_FREE_VIEW & FlushFlags)) {

        // Test-and-set the FREE_IN_PROGRESS flag 
        if (!(pView->wFlags & CACHE_VIEW_FREE_IN_PROGRESS)) {
    
            //
            // Free the view if requested
            //
            if ((MAP_FLUSH_SOFT_FREE_VIEW & FlushFlags) && InUse) {
                // Even if the flush succeeded, someone else is using the map now
                // so don't free it.
                result = FALSE;
            
            // Hard-free must not fail so free even if the flush failed; we have no
            // choice but to lose data.
            } else if ((MAP_FLUSH_HARD_FREE_VIEW & FlushFlags)
                       || ((MAP_FLUSH_SOFT_FREE_VIEW & FlushFlags) && result)) {
                // Mark the view as free in progress.
                pView->wFlags |= CACHE_VIEW_FREE_IN_PROGRESS;

                DWORD dwRetry = 50;

                while (InUse && dwRetry--) {

                    // The map is in use being flushed by another thread. Re-lock the
                    // view and wait until the view is no longer in use before we can
                    // free it. Only one thread per view can get into this loop because
                    // of the FREE_IN_PROGRESS flag. This condition should be very rare.
#ifdef DEBUG                                
                    m_DebugInfo.FreeRetries++;
#endif // DEBUG
                    if (IncViewUseCount (pView)) {

                        ReleasePoolLock ();
                        DEBUGCHK (!OwnPoolLock ());  // No recursive acquire

                        Sleep (100);

                        AcquirePoolLock ();
                        InUse = !DecViewUseCount (pView);

                        if (ppNextInFile) {
                            // Re-snap the next view pointer since we left the lock temporarily
                            // and it may have changed during that window.
                            *ppNextInFile = pView->Alloc.pNextInFile;
                        }

                    } else {

                        // We will never get here unless there is a ref-counting problem.
                        DEBUGCHK(0);
                        break;
                    }
                }

                ASSERT(dwRetry); // Hit the retry limit; unexpected.

                RemoveViewFromFile (pView);
                LRURemove (pView);                
                pView->wFlags &= ~CACHE_VIEW_FREE_IN_PROGRESS;
                PushFreeView (pView);
            }
        }
            // Else, we're done; some other thread is already freeing the map. We just
            // need to leave the pool lock and the other thred will free it.
    }

    return result;
}


// Set start=0 size=0 to walk all views in the whole file.
BOOL
CacheViewPool_t::WalkViewsInRange (
    FSSharedFileMap_t* pMap,
    const ULARGE_INTEGER& StartFileOffset,
    DWORD cbSize,                           // size=0 means all the way to the end of the file
    DWORD FlushFlags
    )
{
    ULARGE_INTEGER RealFileOffset, EndFileOffset;
    
    // Calculate the real start and end offsets, aligned on a view boundary
    RealFileOffset = StartFileOffset;
    RealFileOffset.LowPart &= ~(s_cbView - 1);
    if (0 == cbSize) {
        // All the way to the end of the file
        EndFileOffset.QuadPart = (ULONGLONG) -1;
    } else {
        EndFileOffset.QuadPart = StartFileOffset.QuadPart + cbSize;
    }
    
    BOOL result = TRUE;
    if (EndFileOffset.QuadPart >= StartFileOffset.QuadPart) {  // Overflow check
        AcquirePoolLock ();
        
        // Find the start position of the range
        CacheView_t* pView = FindViewAfterOffset (pMap, RealFileOffset);
        while (pView && (pView->Base.liMapOffset.QuadPart < EndFileOffset.QuadPart)) {
            CacheView_t* pNextInFile = NULL;
            if (!FlushCacheView (pView, FlushFlags, &pNextInFile)) {  // NOTE: FlushCacheView releases the lock temporarily
                // Return failure but keep going to the end of the range
                result = FALSE;
            }
            pView = pNextInFile;
        }        
        
        ReleasePoolLock ();
    }

    return result;
}


// Operates on all allocated views in the pool, even those that are currently
// in use by some other thread.
BOOL
CacheViewPool_t::WalkAllFileViews (
    DWORD FlushFlags
    )
{
    AcquirePoolLock ();
    
    // Can't just walk the LRU list because locked views won't be on that list.
    BOOL result = TRUE;
    for (DWORD index = 0; index < m_NumViews; index++) {
        if (!(m_ViewArray[index].wFlags & CACHE_VIEW_FREE)) {
            if (!FlushCacheView (&m_ViewArray[index], FlushFlags, NULL)) {  // NOTE: FlushCacheView releases the lock temporarily
                // Return failure but keep going to the rest of the views
                result = FALSE;
            }
        }
    }
    
    ReleasePoolLock ();
    
    return result;
}
