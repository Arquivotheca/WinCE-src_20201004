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

extern "C" {
#include "kernel.h"
extern CRITICAL_SECTION MapCS;
}


// Filesys cache filter exports
FSCacheExports g_FSCacheFuncs;
static BOOL g_FSCacheReady;

#ifdef DEBUG
static DWORD g_FSCacheVMStart, g_FSCacheVMSize;
#endif // DEBUG


//------------------------------------------------------------------------------
// Kernel exports that the file cache filter calls into
//------------------------------------------------------------------------------

// The "NKSharedFileMapId" that we give to filesys is a pointer to the fsmap object.

extern "C" static LPVOID
CACHE_AllocVM (
    DWORD cbAlloc
    )
{
    // Pass the MEM_MAPPED flag to wire it to the memory-mapped file page fault handler
    LPVOID pVM = VMAlloc (g_pprcNK, 0, cbAlloc, MEM_RESERVE | MEM_MAPPED, PAGE_NOACCESS);
#ifdef DEBUG
    if (pVM) {
        g_FSCacheVMStart = (DWORD) pVM;
        g_FSCacheVMSize = cbAlloc;
    }
#endif // DEBUG
    return pVM;
}


// Filesys calls this to say the cache is now ready
extern "C" static void
CACHE_NotifyFSReady ()
{
    g_FSCacheReady = TRUE;
}


extern "C" static DWORD
CACHE_CreateMapping (
    DWORD FSSharedFileMapId,
    const ULARGE_INTEGER* pcbMapSize,
    DWORD flProtect
    )
{
    PFSMAP pfsmap = NULL;
    if (g_FSCacheReady) {
        HANDLE hMap = NKCreateFileMapping (g_pprcNK, INVALID_HANDLE_VALUE, NULL,
                                           flProtect | PAGE_INTERNALCACHE,
                                           pcbMapSize->HighPart, pcbMapSize->LowPart,
                                           NULL);
        // Assign cache-specific info here instead of hacking it into
        // the NKCreateFileMapping call.
        if (hMap) {

            PHDATA phdMap = LockHandleData (hMap, g_pprcNK);

            // If LockHandleData fails on a handle we just created, then
            // something is drastically wrong.
            PREFAST_DEBUGCHK (phdMap);

            // Increment the handle count because we're going to call
            // HNDLCloseHandle to remove it from the handle table. We must
            // keep an open-handle reference to the object in order to use
            // pre-close.
            VERIFY (DoLockHDATA (phdMap, HNDLCNT_INCR));

            pfsmap = (PFSMAP) phdMap->pvObj;

            DEBUGCHK (pfsmap->phdMap == phdMap);
            DEBUGCHK (pfsmap->dwFlags & MAP_FILECACHE);

            pfsmap->FSSharedFileMapId = FSSharedFileMapId;

            // Close the handle to remove it from the handle table. We can never
            // use a handle to access this object again. The object will still
            // have one open-handle reference, from the DoLockHDATA call, and one
            // in-use reference, from the LockHandleData call. These will be removed
            // in CACHE_PreCloseMapping and CACHE_CloseMapping, respectively.
            VERIFY (HNDLCloseHandle (g_pprcNK, hMap));
            hMap = NULL;
        }
    }
    return (DWORD) pfsmap;
}


extern "C" static BOOL
CACHE_PreCloseMapping (
    DWORD NKSharedFileMapId
    )
{
    PFSMAP pfsmap = (PFSMAP) NKSharedFileMapId;
    DEBUGCHK (pfsmap->dwFlags & MAP_FILECACHE);
    
    // Remove the handle-count to invoke the map pre-close, but keep a
    // lock-count on the map until CACHE_CloseMapping deletes it.
    DoUnlockHDATA (pfsmap->phdMap, -HNDLCNT_INCR);
    
    // Handle count removal should have caused a pre-close
    DEBUGCHK ((pfsmap->maplink.pFwd == &pfsmap->maplink)
              && (pfsmap->maplink.pBack == &pfsmap->maplink));
    return TRUE;
}


// All views to the mapping must already be unmapped.
extern "C" static BOOL
CACHE_CloseMapping (
    DWORD NKSharedFileMapId
    )
{
    PFSMAP pfsmap = (PFSMAP) NKSharedFileMapId;
    DEBUGCHK (pfsmap->dwFlags & MAP_FILECACHE);

    // Should already be pre-closed
    DEBUGCHK ((pfsmap->maplink.pFwd == &pfsmap->maplink)
              && (pfsmap->maplink.pBack == &pfsmap->maplink));

    UnlockHandleData (pfsmap->phdMap);  // Corresponds to the lock in CACHE_CreateMapping
    return TRUE;
}


extern "C" static BOOL
CACHE_GetMappingSize (
    DWORD NKSharedFileMapId,
    ULARGE_INTEGER* pcbFileSize             // Receives file size
    )
{
    PFSMAP pfsmap = (PFSMAP) NKSharedFileMapId;
    DEBUGCHK (pfsmap->dwFlags & MAP_FILECACHE);

    *pcbFileSize = pfsmap->liSize;
    return TRUE;
}


extern "C" static BOOL
CACHE_SetMappingSize (
    DWORD NKSharedFileMapId,
    const ULARGE_INTEGER* pcbNewFileSize    // New size of the file
    )
{
    PFSMAP pfsmap = (PFSMAP) NKSharedFileMapId;
    DEBUGCHK (pfsmap->dwFlags & MAP_FILECACHE);
    return MapVMResize (pfsmap, *pcbNewFileSize);
}


extern "C" static BOOL
CACHE_FlushView (
    DWORD  NKSharedFileMapId,
    CommonMapView_t* pViewBase,
    PVOID* pPageList,
    DWORD  NumPageListEntries,
    DWORD  FlushFlags
    )
{
    BOOL   result = FALSE;
    PFSMAP pfsmap = (PFSMAP) NKSharedFileMapId;
    DEBUGCHK (pfsmap->dwFlags & MAP_FILECACHE);

    DEBUGCHK (!IsKernelVa (pViewBase->pBaseAddr));  // Don't hand static-mapped VA to file system

    // The view must be inside the cache VM
    DEBUGCHK ((DWORD) pViewBase->pBaseAddr >= g_FSCacheVMStart);
    DEBUGCHK ((DWORD) pViewBase->pBaseAddr + pViewBase->cbSize <= g_FSCacheVMStart + g_FSCacheVMSize);

    // Construct a temporary flush info struct for flushing the view.
    ViewFlushInfo_t flush;
    flush.nogather.prgPages = pPageList;
    flush.dwNumElements = NumPageListEntries;
    flush.cbFlushSpace = 0;  // Unused by DoFlushView

    // The file cache filter is responsible ensuring that decommitted
    // pages aren't paged back in again afterward.
    DWORD dwErr = DoFlushView (g_pprcNK, pfsmap, pViewBase, &flush,
                               pViewBase->pBaseAddr, pViewBase->cbSize,
                               FlushFlags);
    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    } else {
        result = TRUE;
    }

    return result;
}


#ifdef DEBUG
extern "C" static BOOL
CACHE_DebugCheckCSUsage ()
{
    // Return TRUE if things are okay and FALSE if there's an error
    return ! OwnCS (&MapCS);
}
#endif // DEBUG


//------------------------------------------------------------------------------
// Local kernel helpers
//------------------------------------------------------------------------------

extern "C" BOOL
LockCacheView (
    DWORD   dwAddr,
    PFSMAP* ppfsmap,
    CommonMapView_t** ppViewBase
    )
{
    DWORD NKSharedFileMapId = 0;
    DEBUGCHK (pActvProc == g_pprcNK);  // Never call out to filesys if active proc != NK
    if (g_FSCacheReady
        && g_FSCacheFuncs.pGetCacheView (dwAddr, &NKSharedFileMapId, ppViewBase)) {

        PFSMAP pfsmap = (PFSMAP) NKSharedFileMapId;
        DEBUGCHK (pfsmap->dwFlags & MAP_FILECACHE);
        *ppfsmap = pfsmap;
        return TRUE;
    }
    return FALSE;
}


extern "C" void
UnlockCacheView (
    PFSMAP pfsmap,
    CommonMapView_t* pViewBase
    )
{
    // No work to do.
}


//------------------------------------------------------------------------------
// Caching setup
//------------------------------------------------------------------------------

extern "C" BOOL
RegisterForCaching (
    const FSCacheExports* pFSFuncs,
    NKCacheExports* pNKFuncs
    )
{
    BOOL result = TRUE;

    // Take in the FS exports.  No security checks since this function
    // can only be called by other kernel mode code.
    if (g_FSCacheFuncs.Version
        || (KERNEL_CACHE_VERSION != pFSFuncs->Version)) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        result = FALSE;
    } else {
        memcpy (&g_FSCacheFuncs, pFSFuncs, sizeof(FSCacheExports));
    }
    
    // Return the NK exports
    if (KERNEL_CACHE_VERSION != pNKFuncs->Version) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        result = FALSE;
    } else {
        pNKFuncs->pAllocCacheVM         = CACHE_AllocVM;
        pNKFuncs->pNotifyFSReady        = CACHE_NotifyFSReady;
        pNKFuncs->pCreateCacheMapping   = CACHE_CreateMapping;
        pNKFuncs->pPreCloseCacheMapping = CACHE_PreCloseMapping;
        pNKFuncs->pCloseCacheMapping    = CACHE_CloseMapping;
        pNKFuncs->pGetCacheMappingSize  = CACHE_GetMappingSize;
        pNKFuncs->pSetCacheMappingSize  = CACHE_SetMappingSize;
        pNKFuncs->pFlushCacheView       = CACHE_FlushView;

        // Exported for debug purposes only
#ifdef DEBUG
        pNKFuncs->pDebugCheckCSUsage   = CACHE_DebugCheckCSUsage;
#else  // DEBUG
        pNKFuncs->pDebugCheckCSUsage   = NULL;
#endif // DEBUG
    }
    
    return result;
}
