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

// Exports to the kernel
extern "C" BOOL FCFILT_AsyncFlushMapping (DWORD FSSharedFileMapId, DWORD FlushFlags);
extern "C" BOOL FCFILT_GetView (DWORD dwAddr, DWORD* pFSSharedFileMapId,
                                CommonMapView_t** ppview);
extern "C" BOOL FCFILT_ReadWriteData (DWORD FSSharedFileMapId, PBYTE pBuffer,
                                      DWORD cbToAccess, DWORD* pcbAccessed,
                                      const ULARGE_INTEGER* pliFileOffset,
                                      BOOL IsWrite);
extern "C" void FCFILT_LockUnlockFile (DWORD FSSharedFileMapId, BOOL Lock);
extern "C" BOOL FCFILT_FlushFile (DWORD FSSharedFileMapId);


//------------------------------------------------------------------------------
// GLOBAL STATE
//------------------------------------------------------------------------------

static CacheViewPool_t* g_pViewPool;
static NKCacheExports g_NKCacheFuncs;     // Kernel cache exports


static void CleanupViewPool ()
{
    if (g_pViewPool) {
        delete g_pViewPool;
        g_pViewPool = NULL;
    }
    g_NKCacheFuncs.Version = 0;
}


void InitViewPool ()
{
    // Pass function pointers to the kernel and receive some in return.
    // The kernel is not supposed to call any of these until after we
    // notify it that the cache is ready.
    FSCacheExports FSFuncs;
    FSFuncs.Version                 = KERNEL_CACHE_VERSION;
    FSFuncs.pAsyncFlushCacheMapping = FCFILT_AsyncFlushMapping;
    FSFuncs.pGetCacheView           = FCFILT_GetView;
    FSFuncs.pReadWriteCacheData     = FCFILT_ReadWriteData;
    FSFuncs.pLockUnlockFile         = FCFILT_LockUnlockFile;
    FSFuncs.pFlushFile              = FCFILT_FlushFile;

    g_NKCacheFuncs.Version = KERNEL_CACHE_VERSION;

    if (!KernelLibIoControl ((HANDLE) KMOD_CORE, IOCTL_KLIB_CACHE_REGISTER,
                             &FSFuncs, sizeof(FSCacheExports),
                             &g_NKCacheFuncs, sizeof(NKCacheExports), NULL)) {
        CleanupViewPool ();
        return;
    }

    g_pViewPool = new CacheViewPool_t ();
    if (!g_pViewPool) {
        return;
    } else if (!g_pViewPool->Init (g_NKCacheFuncs.pAllocCacheVM)) {
        CleanupViewPool ();
        return;
    }

    // Now tell the kernel that things are ready to go
    g_NKCacheFuncs.pNotifyFSReady ();
}


BOOL CheckViewPool ()
{
    return g_pViewPool ? TRUE : FALSE;
}


void FlushViewPool ()
{
    if (g_pViewPool) {
        g_pViewPool->WalkAllFileViews (MAP_FLUSH_WRITEBACK);
    }
}

static inline BOOL SafeCopyViewData (LPVOID pDst, LPCVOID pSrc, DWORD cbSize)
{
#ifndef DEBUG
    // On retail/ship builds, disable breaking into the debugger on an
    // exception triggered by a failed copy to/from a view. The copy
    // will fail when the underlying i/o fails or when there is insuicient
    // physical memory resulting in a ReadFile/WriteFile failure being
    // returned to the caller.
    DWORD dwOldUTlsFlags = UTlsPtr()[TLSSLOT_KERNEL];   
    UTlsPtr()[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT | TLSKERN_NOFAULTMSG;
#endif // DEBUG

    BOOL result = CeSafeCopyMemory (pDst, pSrc, cbSize);

#ifndef DEBUG
    UTlsPtr()[TLSSLOT_KERNEL] = dwOldUTlsFlags;
#endif // DEBUG

    return result;
}

//------------------------------------------------------------------------------
// EXPORTS TO KERNEL
//------------------------------------------------------------------------------

// The "FSSharedFileMapId" that we give to the kernel is a FSSharedFileMap_t*.

// Flush all views belonging to the map
extern "C" BOOL
FCFILT_AsyncFlushMapping (
    DWORD FSSharedFileMapId,
    DWORD FlushFlags
    )
{
    // Kernel should not call filesys while holding MapCS
    DEBUGCHK (g_NKCacheFuncs.pDebugCheckCSUsage ());
    FSSharedFileMap_t* pMap = (FSSharedFileMap_t*) FSSharedFileMapId;
    
    // Check for unsupported flush flags
    DEBUGCHK (!(FlushFlags & (MAP_FLUSH_DECOMMIT_RW | MAP_FLUSH_CACHE_MASK)));
    
    return pMap->AsyncFlush (FlushFlags);
}



extern "C" BOOL
FCFILT_GetView (
    DWORD  dwAddr,
    DWORD* pNKSharedFileMapId,
    CommonMapView_t** ppview
    )
{
    // Kernel should not call filesys while holding MapCS
    DEBUGCHK (g_NKCacheFuncs.pDebugCheckCSUsage ());
    if (g_pViewPool->GetView (dwAddr, pNKSharedFileMapId, ppview)) {
        return TRUE;
    }
    return FALSE;
}

// Write a single cache page (or part of a page) to the underlying file
extern "C" BOOL
FCFILT_ReadWriteData (
    DWORD  FSSharedFileMapId,
    PBYTE  pBuffer,
    DWORD  cbToAccess,
    DWORD* pcbAccessed,
    const ULARGE_INTEGER* pliFileOffset,
    BOOL   IsWrite
    )
{
    // Kernel should not call filesys while holding MapCS
    DEBUGCHK (g_NKCacheFuncs.pDebugCheckCSUsage ());

    FSSharedFileMap_t* pMap = (FSSharedFileMap_t*) FSSharedFileMapId;
    return pMap->UncachedReadWriteWithSeek (pBuffer, cbToAccess, pcbAccessed,
                                            pliFileOffset->LowPart,
                                            pliFileOffset->HighPart, IsWrite);
}

// Enter or leave the critical section for the underlying file
extern "C" void
FCFILT_LockUnlockFile (
    DWORD FSSharedFileMapId,
    BOOL  Lock
    )
{
    FSSharedFileMap_t* pMap = (FSSharedFileMap_t*) FSSharedFileMapId;
    pMap->LockUnlock (Lock);
}

// FlushFileBuffers for the underlying file
extern "C" BOOL
FCFILT_FlushFile (
    DWORD FSSharedFileMapId
    )
{
    // Kernel should not call filesys while holding MapCS
    DEBUGCHK (g_NKCacheFuncs.pDebugCheckCSUsage ());
    
    FSSharedFileMap_t* pMap = (FSSharedFileMap_t*) FSSharedFileMapId;
    return pMap->UncachedFlushFileBuffers ();
}


//------------------------------------------------------------------------------
// CACHED FILEMAP OPERATIONS
//------------------------------------------------------------------------------


// Called every time a new cached PrivateFileHandle is created.
BOOL
FSSharedFileMap_t::InitCaching ()
{
    // Errors here are non-fatal; the file will just be uncached.
    if (!m_NKSharedFileMapId                                // Caching not already initialized
        && CheckViewPool ()                                 // View pool is usable
        && !m_pVolume->IsUncached ()                        // Volume allows caching
        && (m_SharedMapFlags | CACHE_SHAREDMAP_PAGEABLE)) { // Read/WriteFileWithSeek required

        ULARGE_INTEGER FileSize;
        if (!UncachedGetFileSize (&FileSize)) {
            DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::InitCaching, !! Failed to get file size, file will be uncached\r\n"));
        } else {
            DWORD flProtect;
            if (m_SharedMapFlags & CACHE_SHAREDMAP_WRITABLE) {
                flProtect = PAGE_READWRITE;
            } else {
                flProtect = PAGE_READONLY;
            }
            m_NKSharedFileMapId = g_NKCacheFuncs.pCreateCacheMapping ((DWORD) this, &FileSize, flProtect);
            if (!m_NKSharedFileMapId) {
                DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::InitCaching, !! Failed to initialize kernel cache map, file will be uncached\r\n"));
            }
        }
    }
    return (m_NKSharedFileMapId ? TRUE : FALSE);
}


// Called before the FSSharedFileMap is destroyed, to clean up kernel references.
BOOL
FSSharedFileMap_t::PreCloseCaching ()
{
    BOOL result = TRUE;

    // The preclose also acquires the kernel flush lock which forces async work to complete
    DEBUGCHK (!(m_SharedMapFlags & CACHE_SHAREDMAP_PRECLOSED));
    if (m_NKSharedFileMapId && !g_NKCacheFuncs.pPreCloseCacheMapping (m_NKSharedFileMapId)) {
        result = FALSE;
    }

    m_SharedMapFlags |= CACHE_SHAREDMAP_PRECLOSED;
    return result;
}


// Called when the FSSharedFileMap is being destroyed or when we're abandoning caching it
BOOL
FSSharedFileMap_t::CleanupCaching ()
{
    BOOL result = TRUE;

    if (!(m_SharedMapFlags & CACHE_SHAREDMAP_PRECLOSED)
        && !PreCloseCaching ()) {
        result = FALSE;
    }
    if (!FlushFileBuffers (MAP_FLUSH_WRITEBACK | MAP_FLUSH_DECOMMIT_RO
                           | MAP_FLUSH_DECOMMIT_RW | MAP_FLUSH_HARD_FREE_VIEW)) {
        result = FALSE;
    }
    if (m_NKSharedFileMapId && !g_NKCacheFuncs.pCloseCacheMapping (m_NKSharedFileMapId)) {
        result = FALSE;
    }
    m_NKSharedFileMapId = 0;

    return result;
}


void
FSSharedFileMap_t::SetPendingFlush ()
{
    DWORD OldFlags;
    DWORD NewFlags;

    // Use interlocked API to avoid taking a critical section since
    // this can be invoked synchronously during file I/O or
    // asynchronously from the write-back thread.
    do {

        // Set the pending flush flag on this shared map object.
        OldFlags = m_SharedMapFlags;
        NewFlags = OldFlags | CACHE_SHAREDMAP_PENDING_FLUSH;

    } while (OldFlags != InterlockedCompareExchange ((LONG*)&m_SharedMapFlags, NewFlags, OldFlags));

    if (!(CACHE_SHAREDMAP_PENDING_FLUSH & OldFlags)) {
        // The flag was not previously set, so indicate that the
        // volume has at least one map object with a pending flush.
        m_pVolume->SetPendingFlush ();
    }
}


void
FSSharedFileMap_t::ClearPendingFlush ()
{
    DWORD OldFlags;
    DWORD NewFlags;

    // Use interlocked API to avoid taking a critical section since
    // this can be invoked synchronously during file I/O or
    // asynchronously from the write-back thread.
    do {

        // Clear the pending flush flag on this shared map object.
        OldFlags = m_SharedMapFlags;
        NewFlags = OldFlags & ~CACHE_SHAREDMAP_PENDING_FLUSH;

    } while (OldFlags != InterlockedCompareExchange ((LONG*)&m_SharedMapFlags, NewFlags, OldFlags));

    if (CACHE_SHAREDMAP_PENDING_FLUSH & OldFlags) {
        // The flag was previously set, so clear a pending flush on
        // the volume.
        m_pVolume->ClearPendingFlush ();
    }
}


BOOL
FSSharedFileMap_t::CompletePendingFlush (DWORD PrivateFileFlags)
{
    // Pending flushes must be completed before new writes to the cache,
    // to prevent the cache from filling up with unflushable data.
    if (m_NKSharedFileMapId && !(PrivateFileFlags & CACHE_PRIVFILE_UNCACHED)  // Cached access
        && !m_pVolume->CompletePendingFlushes ()) {
        return FALSE;
    }
    return TRUE;
}


// This is really just a debug wrapper around our usage of the kernel size
BOOL
FSSharedFileMap_t::GetCachedFileSize (
    ULARGE_INTEGER* pFileSize
    )
{
    BOOL result = g_NKCacheFuncs.pGetCacheMappingSize (m_NKSharedFileMapId, pFileSize);

#ifdef DEBUG
    // Check for consistency between the kernel and the underyling file system
    if (result) {
        ULARGE_INTEGER UncachedFileSize;
        if (UncachedGetFileSize (&UncachedFileSize)
            && (UncachedFileSize.QuadPart != pFileSize->QuadPart)) {
            DEBUGCHK (0);
        }
    }
#endif // DEBUG
    
    return result;
}

// g_NKCacheFuncs.pSetCacheMappingSize requires that if shrinking, all
// views to cut-off parts of the map must already be unmapped, so that
// those pages are already flushed and decommitted.  So all calls to
// g_NKCacheFuncs.pSetCacheMappingSize must go through SetCachedFileSize.
BOOL
FSSharedFileMap_t::SetCachedFileSize (
    ULARGE_INTEGER NewFileSize
    )
{
    // Someone could be in trouble if they were paging in while the file resize
    // occurs, but the I/O lock should protect against that.
    DEBUGCHK (OwnIOLock ());
    DEBUGCHK (m_NKSharedFileMapId);  // Cached file only

    ULARGE_INTEGER OldFileSize;
    if (!GetCachedFileSize (&OldFileSize)) {
        return FALSE;
    }

    // If the file is shrinking, flush and decommit cache views that are being cut off.
    // If the new size falls partway into an open view, the entire view will be freed.
    if (NewFileSize.QuadPart < OldFileSize.QuadPart) {

        // Use the volume write-back lock to prevent other threads from walking
        // the views while they're being destroyed.
        m_pVolume->AcquireWriteBackLock ();
        
        // WalkViewsAfterOffset should only return failure if the flush fails;
        // it should still free the views no matter what.
        g_pViewPool->WalkViewsAfterOffset (
            this, NewFileSize,
            MAP_FLUSH_WRITEBACK | MAP_FLUSH_DECOMMIT_RO | MAP_FLUSH_DECOMMIT_RW
            | MAP_FLUSH_HARD_FREE_VIEW);

        m_pVolume->ReleaseWriteBackLock ();
    }
    
    // Update the kernel representation of the file
    if (!g_NKCacheFuncs.pSetCacheMappingSize (m_NKSharedFileMapId, &NewFileSize)) {
        return FALSE;
    }
    
    // Now change the actual file
    if (!UncachedSetEndOfFile (NewFileSize)) {
        // Restore old kernel cache mapping size
        VERIFY (g_NKCacheFuncs.pSetCacheMappingSize (m_NKSharedFileMapId, &OldFileSize));
        return FALSE;
    }

    return TRUE;
}


// ReadFileScatter / WriteFileGather
// The caller should call CompletePendingFlush before calling this function,
// to minimize the chance of overflowing the cache with unflushable data.
BOOL
FSSharedFileMap_t::ReadWriteScatterGather (
    FILE_SEGMENT_ELEMENT aSegmentArray[],   // Already access-checked and duplicated by FSDMGR
    DWORD cbToAccess, 
    ULARGE_INTEGER* pOffsetArray,           // Already access-checked and duplicated by FSDMGR
    BOOL  IsWrite,
    DWORD PrivateFileFlags
    )
{
    ULARGE_INTEGER OldFileSize;
    BOOL result;
    
    AcquireIOLock ();

    if (!m_NKSharedFileMapId || (PrivateFileFlags & CACHE_PRIVFILE_UNCACHED)) {
        // Uncached access
        
        // Flush and invalidate any existing views in the cache, to prevent inconsistency
        // between cached & uncached handles
        result = g_pViewPool->WalkAllViews (this, MAP_FLUSH_WRITEBACK | MAP_FLUSH_DECOMMIT_RO);
        if (result) {
            result = UncachedReadWriteScatterGather (aSegmentArray, cbToAccess,
                                                     pOffsetArray, IsWrite);
        }

        // Update the cached copy of the file size, if it changed
        ULARGE_INTEGER NewFileSize;
        if (IsWrite && m_NKSharedFileMapId
            && (!UncachedGetFileSize (&NewFileSize)  // Should only be growing, not shrinking
                || !g_NKCacheFuncs.pSetCacheMappingSize (m_NKSharedFileMapId, &NewFileSize))) {
            // We can't update the cache representation of the file!
            DEBUGCHK (0);
            CleanupCaching ();  // Force off all caching for this file
        }
    
    // Query current file size
    } else if (!GetCachedFileSize (&OldFileSize)) {
        result = FALSE;
    
    } else {
        // Cached access
        BOOL  WroteData = FALSE;  // Set to true if we wrote anything
        DWORD cbPage    = UserKInfo[KINX_PAGESIZE];
        DWORD NumPages  = cbToAccess / cbPage;
        DWORD PageNum   = 0;
        ULARGE_INTEGER EndOffset, NewFileSize;
        CacheViewPool_t::LockedViewInfo info;

        // Must be accessing an even number of pages
        if (0 != (cbToAccess & (cbPage - 1))) {
            SetLastError (ERROR_INVALID_PARAMETER);
            result = FALSE;
        } else {

            NewFileSize = OldFileSize;
            result = TRUE;
            while ((PageNum < NumPages) && result) {
                // Read/write a single page at a time

                EndOffset = pOffsetArray[PageNum];
                EndOffset.QuadPart += cbPage;

                // Check end offset against file size
                if (EndOffset.QuadPart > OldFileSize.QuadPart) {
                    // Reading past the end of the file: fail
                    // The write will grow the file: pre-adjust the file size
                    if (!IsWrite || !SetCachedFileSize (EndOffset)) {
                        result = FALSE;
                        break;
                    } else {
                        NewFileSize = EndOffset;
                    }
                }

                result = g_pViewPool->LockView (this, NewFileSize, pOffsetArray[PageNum],
                                                cbPage, IsWrite, info, NULL);
                if (result) {
                    if (IsWrite) {
                        result = SafeCopyViewData (info.pBaseAddr, aSegmentArray[PageNum].Buffer, cbPage);
                        if (result) {
                            WroteData = TRUE;
                            OldFileSize = NewFileSize;
                        }
                    } else {
                        result = SafeCopyViewData (aSegmentArray[PageNum].Buffer, info.pBaseAddr, cbPage);
                    }

                    g_pViewPool->UnlockView (this, info);
                }

                if (IsWrite && !result) {
                    // Restore old file size if the write didn't complete
                    VERIFY (SetCachedFileSize (OldFileSize));
                }
                
                PageNum++;
            }
            
            // Now handle write-back or write through.
            if (WroteData) {
                if (m_pVolume->IsWriteBack () && (PrivateFileFlags && CACHE_PRIVFILE_WRITE_BACK)) {
                    // Write-back
                    m_pVolume->SignalWrite ();
                } else {
                    // Write-through, fail if the write can't be committed
                    if (!g_pViewPool->WalkAllViews (this, MAP_FLUSH_WRITEBACK)) {
                        result = FALSE;
                    }
                }
            }
        }
    }
    ReleaseIOLock ();

    return result;
}





BOOL
FSSharedFileMap_t::ReadWriteWithSeek (
    PBYTE  pBuffer,             // Comes from the user
    DWORD  cbToAccess, 
    PDWORD pcbAccessed,         // Does NOT come from the user
    DWORD  dwLowOffset,
    DWORD  dwHighOffset,
    BOOL   IsWrite,
    DWORD  PrivateFileFlags
    )
{
    BOOL result = FALSE;
    ULARGE_INTEGER StartOffset;
    
    StartOffset.HighPart = dwHighOffset;
    StartOffset.LowPart = dwLowOffset;
    
    AcquireIOLock ();
    
    if (!pBuffer && !cbToAccess && !pcbAccessed && !dwLowOffset && !dwHighOffset) {
        // Special case: call with all 0's to determine whether the function is supported
        result = UncachedReadWriteWithSeek (0, 0, 0, 0, 0, IsWrite);
    
    } else if (!m_NKSharedFileMapId || (PrivateFileFlags & CACHE_PRIVFILE_UNCACHED)) {
        // Uncached access
        
        // Flush and invalidate any existing views in the cache, to prevent inconsistency
        // between cached & uncached handles
        result = g_pViewPool->WalkViewsInRange (this, StartOffset, cbToAccess,
                                                MAP_FLUSH_WRITEBACK | MAP_FLUSH_DECOMMIT_RO);
        if (result) {
            result = UncachedReadWriteWithSeek (pBuffer, cbToAccess, pcbAccessed,
                                                dwLowOffset, dwHighOffset,
                                                IsWrite);
        }
    
        // Update the cached copy of the file size, if it changed
        ULARGE_INTEGER NewFileSize;
        if (IsWrite && m_NKSharedFileMapId
            && (!UncachedGetFileSize (&NewFileSize)  // Should only be growing, not shrinking
                || !g_NKCacheFuncs.pSetCacheMappingSize (m_NKSharedFileMapId, &NewFileSize))) {
            // We can't update the cache representation of the file!
            DEBUGCHK (0);
            CleanupCaching ();  // Force off all caching for this file
        }

    } else {
        ULARGE_INTEGER EndOffset, CurOffset, OldFileSize;

        // Cached access
        DEBUGCHK (pBuffer && pcbAccessed);  // Guaranteed by PrivateFileHandle

        EndOffset = StartOffset;
        EndOffset.QuadPart += cbToAccess;
        if (EndOffset.QuadPart < StartOffset.QuadPart) {
            // Overflow -- end access at 64 bits
            EndOffset.QuadPart = (ULONGLONG) -1;
            cbToAccess = (DWORD) (EndOffset.QuadPart - StartOffset.QuadPart);
        }

        // Query current file size
        if (!GetCachedFileSize (&OldFileSize)) {
            result = FALSE;
        
        } else {
            // Check end offset against file size
            ULARGE_INTEGER NewFileSize = OldFileSize;
            if (EndOffset.QuadPart > OldFileSize.QuadPart) {
                if (!IsWrite) {
                    // Reading past the end of the file: succeed but return less bytes than requested
                    ULARGE_INTEGER cbAdjust;
                    cbAdjust.QuadPart = EndOffset.QuadPart - OldFileSize.QuadPart;
                    if (cbAdjust.HighPart || (cbToAccess < cbAdjust.LowPart)) {
                        // No way to read any data: 0-byte access
                        cbToAccess = 0;
                        EndOffset = StartOffset;

                    } else {
                        // Read part of the requested data
                        cbToAccess -= cbAdjust.LowPart;
                        EndOffset = OldFileSize;
                    }
                } else {
                    // The write will grow the file: pre-adjust the file size
                    if (!SetCachedFileSize (EndOffset)) {
                        result = FALSE;
                        goto exit;
                    }
                    NewFileSize = EndOffset;
                }
            }
        
            result = TRUE;
            
            // Transfer data between the cache and the user buffer
            CacheViewPool_t::LockedViewInfo info;
            CacheView_t* pPrevView = NULL;  // Short-cut walk to new view
            CurOffset = StartOffset;
            while (cbToAccess) {
                result = g_pViewPool->LockView (this, NewFileSize, CurOffset, cbToAccess,
                                                IsWrite, info, pPrevView);
                if (result) {
                    



                    

                    


                    // Read/write from the locked view
                    if (IsWrite) {
                        result = SafeCopyViewData (info.pBaseAddr, pBuffer, info.cbView);
                    } else {
                        result = SafeCopyViewData (pBuffer, info.pBaseAddr, info.cbView);
                    }

                    pPrevView = info.pView;
                    g_pViewPool->UnlockView (this, info);
                }

                if (!result) {
                    // Completely out of usable views, invalid user buffer, or reading past EOF
                    break;
                }
                
                // Advance the position
                CurOffset.QuadPart += info.cbView;
                cbToAccess -= info.cbView;
                *pcbAccessed += info.cbView;
                pBuffer += info.cbView;
            }

            // Now handle write-back or write through.
            if (IsWrite) {
                if (*pcbAccessed) {  // If we wrote any data
                    if (m_pVolume->IsWriteBack () && (PrivateFileFlags && CACHE_PRIVFILE_WRITE_BACK)) {
                        // Write-back
                        m_pVolume->SignalWrite ();
                    } else {
                        // Write-through, fail if the write can't be committed
                        if (!g_pViewPool->WalkViewsInRange (this, StartOffset, *pcbAccessed,
                                                            MAP_FLUSH_WRITEBACK)) {
                            result = FALSE;
                        }
                    }
                }
                
                // Fix file size if the write didn't complete
                if (!result || cbToAccess) {
                    if (OldFileSize.QuadPart > CurOffset.QuadPart) {
                        // The write did not grow the file
                        VERIFY (SetCachedFileSize (OldFileSize));
                    } else {
                        // The write grew the file but possibly not as much as expected
                        VERIFY (SetCachedFileSize (CurOffset));
                    }
                }
            }
        }
    }

exit:
    ReleaseIOLock ();

    return result;
}


BOOL
FSSharedFileMap_t::SetEndOfFile (
    ULARGE_INTEGER Offset
    )
{
    BOOL result = FALSE;
    
    AcquireIOLock ();
    
    if (m_NKSharedFileMapId) {
        // Cached file
        result = SetCachedFileSize (Offset);
    } else {
        // Uncached file
        result = UncachedSetEndOfFile (Offset);
    }

    ReleaseIOLock ();

    if (result) {
        m_pVolume->SignalWrite ();
    }
    
    return result;
}


BOOL
FSSharedFileMap_t::FlushFileBuffers (
    DWORD FlushFlags
    )
{
    BOOL result = TRUE;
    
    //m_pVolume->AcquireUnderlyingIOLock ();
    
    if (m_NKSharedFileMapId && !g_pViewPool->WalkAllViews (this, FlushFlags)) {
        result = FALSE;
    }
    
    //m_pVolume->ReleaseUnderlyingIOLock ();

    DEBUGCHK (!m_pVolume->OwnUnderlyingIOLock ());  // Else can't call ClearPendingFlush
    if (result) {
        ClearPendingFlush ();
    }
    
    return result;
}


BOOL
FSSharedFileMap_t::FlushView (
    CommonMapView_t* pView,
    DWORD  FlushFlags,
    PVOID* pPageList
    )
{
    DEBUGCHK (m_NKSharedFileMapId);

    // Remove cache-specific flags the kernel won't understand
    FlushFlags &= ~MAP_FLUSH_CACHE_MASK;
    
    // Call into the kernel to flush and decommit the pages
    BOOL result = g_NKCacheFuncs.pFlushCacheView (m_NKSharedFileMapId, pView,
                                                  pPageList, g_pViewPool->GetNumPageListEntries (),
                                                  FlushFlags);
    if (!result) {
        SetPendingFlush ();
    }
    return result;
}


DWORD
FSSharedFileMap_t::GetFileSize (
    PDWORD pFileSizeHigh        // Comes from the user
    )
{
    ULARGE_INTEGER FileSize;
    
    FileSize.QuadPart = (ULONGLONG) -1;
    SetLastError (NO_ERROR);
    
    AcquireIOLock ();
    
    if (m_NKSharedFileMapId) {
        // Cached file
        GetCachedFileSize (&FileSize);
    } else {
        // Uncached file
        UncachedGetFileSize (&FileSize);
    }
    
    ReleaseIOLock ();

    if (pFileSizeHigh) {
        *pFileSizeHigh = FileSize.HighPart;
    }
    return FileSize.LowPart;
}


BOOL
FSSharedFileMap_t::GetFileInformationByHandle (
    PBY_HANDLE_FILE_INFORMATION pFileInfo       // Comes from the user
    )
{
    ULARGE_INTEGER FileSize;
    BOOL result;
    
    FileSize.QuadPart = (ULONGLONG) -1;
    
    AcquireIOLock ();
    
    result = UncachedGetFileInformationByHandle (pFileInfo);
    if (result && m_NKSharedFileMapId) {
        

        result = GetCachedFileSize (&FileSize);
    }
    
    ReleaseIOLock ();

    if (result && m_NKSharedFileMapId && pFileInfo) {
        pFileInfo->nFileSizeHigh = FileSize.HighPart;
        pFileInfo->nFileSizeLow  = FileSize.LowPart;
    }
    return result;
}


BOOL
FSSharedFileMap_t::GetFileTime (
    FILETIME* pCreation,
    FILETIME* pLastAccess, 
    FILETIME* pLastWrite
    )
{
    
    return UncachedGetFileTime (pCreation, pLastAccess, pLastWrite);
}


BOOL
FSSharedFileMap_t::SetFileTime (
    const FILETIME* pCreation, 
    const FILETIME* pLastAccess, 
    const FILETIME* pLastWrite
    )
{
    
    return UncachedSetFileTime (pCreation, pLastAccess, pLastWrite);
}


BOOL
FSSharedFileMap_t::DeviceIoControl (
     DWORD  dwIoControlCode, 
     PVOID  pInBuf, 
     DWORD  nInBufSize, 
     PVOID  pOutBuf, 
     DWORD  nOutBufSize,
     PDWORD pBytesReturned,
     DWORD  PrivateFileFlags
     )
{
    BOOL result = TRUE;
    
    switch (dwIoControlCode) {

    // FSDMGR converts these IOCTLs to regular calls to the FCFILT_ entry
    // points, so they should never get here.  Deny them if they happen,
    // because they'd need to go through the cache.
    case IOCTL_FILE_GET_VOLUME_INFO:    // FSCTL_GET_VOLUME_INFO
    case IOCTL_FILE_WRITE_GATHER:
    case IOCTL_FILE_READ_SCATTER:
        DEBUGMSG (ZONE_ERROR, (TEXT("CACHEFILT:FSSharedFileMap_t::DeviceIoControl, !! Got an IOCTL that should have been an API call: %u\r\n"),
                               dwIoControlCode));
        DEBUGCHK (0);
        SetLastError (ERROR_NOT_SUPPORTED);
        return FALSE;

    case FSCTL_SET_EXTENDED_FLAGS:
        // Deny transaction flags for cached files
        if (m_NKSharedFileMapId && !(PrivateFileFlags & CACHE_PRIVFILE_UNCACHED) // Cached access
            && (nInBufSize >= sizeof(DWORD))) {
            DWORD* pdwFlags = (DWORD*) pInBuf;
            if (*pdwFlags & CE_FILE_FLAG_TRANS_DATA) {
                DEBUGMSG (ZONE_WARNING, (TEXT("CACHEFILT:FSSharedFileMap_t::DeviceIoControl, Denying transactioning on cached file.\r\n"),
                                         dwIoControlCode));
                SetLastError (ERROR_NOT_SUPPORTED);
                return FALSE;
            }
        }
        break;

    case FSCTL_FLUSH_BUFFERS:
        result = FlushFileBuffers (MAP_FLUSH_WRITEBACK);
        break;

    case FSCTL_SET_FILE_CACHE:
        // The PrivateFileHandle took care of everything.
        break;

    case FSCTL_GET_STREAM_INFORMATION:
        // No work to do.  Even though the cache keeps its own copy of the file size, the
        // size is passed through to the underlying file system by SetCachedFileSize as
        // soon as it changes.  So the file system should report the right size.
        break;

    default:
        break;
    }
    
    if (!UncachedDeviceIoControl (dwIoControlCode, pInBuf, nInBufSize,
                                  pOutBuf, nOutBufSize, pBytesReturned)) {
        result = FALSE;
    }
    return result;
}
