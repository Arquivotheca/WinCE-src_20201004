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

//------------------------------------------------------------------------------
// GLOBAL STATE
//------------------------------------------------------------------------------

static NKCacheExports g_NKCacheFuncs;     // Kernel cache exports

const FSCacheExports g_FSCacheFuncs = {
    KERNEL_CACHE_VERSION,
    FCFILT_UncachedWrite,
    FCFILT_UncachedRead,
    FCFILT_UncachedSetFileSize,
};

void InitPageTree ()
{
    // Get the kernel exported cache functions. 
    g_NKCacheFuncs.Version = KERNEL_CACHE_VERSION;

    if (!KernelLibIoControl ((HANDLE) KMOD_CORE, IOCTL_KLIB_CACHE_REGISTER,
                             NULL, 0,
                             &g_NKCacheFuncs, sizeof(NKCacheExports), NULL)) {
        return;
    }
}

FORCEINLINE BOOL DoEnterVolume (HVOL hVolume, HANDLE *phLock, LPVOID *ppLockData)
{
    return IsFsPowerOffThread ()
         ? TRUE
         : (FSDMGR_AsyncEnterVolume (hVolume, phLock, ppLockData) == ERROR_SUCCESS);
}

FORCEINLINE void DoExitVolume (HANDLE hLock, LPVOID pLockData)
{
    if (!IsFsPowerOffThread ()) {
        FSDMGR_AsyncExitVolume (hLock, pLockData);
    }
}

//------------------------------------------------------------------------------
// EXPORTS TO KERNEL
//------------------------------------------------------------------------------

// Write cached pages to the underlying file
extern "C" BOOL
FCFILT_UncachedWrite (
    DWORD FSSharedFileMapId, 
    const ULARGE_INTEGER *pliOffset, 
    const BYTE *pData, 
    DWORD cbSize,
    LPDWORD pcbWritten
    )
{
    FSSharedFileMap_t* pMap = (FSSharedFileMap_t*) FSSharedFileMapId;
    HVOL hVolume = pMap->GetVolume()->GetVolumeHandle();

    HANDLE  hLock = 0;
    LPVOID  pLockData = NULL;

    BOOL result = DoEnterVolume (hVolume, &hLock, &pLockData);

    if (result) {

        DWORD cbAccessed = 0;
        result = pMap->UncachedReadWriteWithSeek ((PVOID)pData, cbSize, &cbAccessed,
                                                  pliOffset->LowPart, pliOffset->HighPart, 
                                                  TRUE);
        DoExitVolume (hLock, pLockData);

        if (pcbWritten) {
            *pcbWritten = cbAccessed;
        }
    }
    return result;    
}

// Write cached pages from the underlying file
extern "C" BOOL
FCFILT_UncachedRead (
    DWORD FSSharedFileMapId, 
    const ULARGE_INTEGER *pliOffset, 
    BYTE *pData, 
    DWORD cbSize,
    LPDWORD pcbRead
    )
{
    FSSharedFileMap_t* pMap = (FSSharedFileMap_t*) FSSharedFileMapId;
    HVOL hVolume = pMap->GetVolume()->GetVolumeHandle();

    HANDLE  hLock = 0;
    LPVOID  pLockData = NULL;

    BOOL result = DoEnterVolume (hVolume, &hLock, &pLockData);

    if (result) {
        DWORD cbAccessed = 0;
        result = pMap->UncachedReadWriteWithSeek (pData, cbSize, &cbAccessed,
                                                  pliOffset->LowPart, pliOffset->HighPart, 
                                                  FALSE);

        DoExitVolume (hLock, pLockData);

        if (pcbRead) {
            *pcbRead = cbAccessed;
        }
    }
    return result;    
}

extern "C" BOOL
FCFILT_UncachedSetFileSize (
    DWORD FSSharedFileMapId, 
    const ULARGE_INTEGER *pliSize
    )
{
    FSSharedFileMap_t* pMap = (FSSharedFileMap_t*) FSSharedFileMapId;
    HVOL hVolume = pMap->GetVolume()->GetVolumeHandle();

    HANDLE  hLock = 0;
    LPVOID  pLockData = NULL;

    BOOL result = DoEnterVolume (hVolume, &hLock, &pLockData);

    if (result) {

        result = pMap->UncachedSetEndOfFile (*pliSize);
        
        DoExitVolume (hLock, pLockData);
    }
    return result;    
    
}

//------------------------------------------------------------------------------
// CACHED FILEMAP OPERATIONS
//------------------------------------------------------------------------------


// Called every time a new cached PrivateFileHandle is created.
BOOL
FSSharedFileMap_t::InitCaching ()
{
    // Only create a new page tree if one doesn't already exist and the volume
    // allows caching.  Errors here are non-fatal; the file will just be uncached.
    if (!m_PageTreeId && !m_pVolume->IsUncached ()) {

        ULARGE_INTEGER FileSize;
        if (!UncachedGetFileSize (&FileSize)) {
            DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::InitCaching, !! Failed to get file size, file will be uncached\r\n"));
            goto exit;
        } 
        
        DWORD PageTreeId = g_NKCacheFuncs.pPageTreeCreate ((DWORD)this, &FileSize, &g_FSCacheFuncs, VM_BLOCK_SIZE, 0);

        if (PageTreeId && (0 != InterlockedCompareExchange ((LONG*)&m_PageTreeId, PageTreeId, 0))) {
            // The page tree has already been created on another thread, so close this
            // page tree
            g_NKCacheFuncs.pPageTreeClose (PageTreeId);
        }
        
        if (!m_PageTreeId) {
            DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::InitCaching, !! Failed to initialize kernel cache map, file will be uncached\r\n"));
        } 
    }

exit:      
    return (m_PageTreeId != 0);
}


// Called when the FSSharedFileMap is being destroyed or when we're abandoning caching it
BOOL
FSSharedFileMap_t::CleanupCaching (BOOL fWriteBack)
{
    BOOL result = TRUE;

    if (fWriteBack) {
        result = FlushMap();
        
    }

    if (m_PageTreeId) {
        g_NKCacheFuncs.pPageTreeClose (m_PageTreeId);
    }
    m_PageTreeId = 0;

    return result;
}

DWORD
FSSharedFileMap_t::GetPageTreeId ()
{
    if (!m_PageTreeId)
    {
        // If a page tree doesn't currently exist, create one.
        InitCaching();
    }
    return m_PageTreeId;
}

DWORD
FSSharedFileMap_t::TransferPageTree ()
{
    DWORD PageTreeId = 0;
    
    AcquireMapLock();

    VERIFY (FlushMap());
    VERIFY (InvalidateMap());
    PageTreeId = (DWORD)InterlockedExchange ((LONG*)&m_PageTreeId, 0);

    ReleaseMapLock();

    DEBUGMSG (ZONE_FILEMAP, (TEXT("CACHEFILT:FSSharedFileMap_t::TransferPageTree, file %s, page tree %d.\r\n"),
              m_pFileName, m_PageTreeId));

    return PageTreeId;
}

DWORD
FSSharedFileMap_t::SetFlags (
    DWORD dwFlagsToSet
    )
{
    volatile DWORD OldFlags;
    volatile DWORD NewFlags;

    // Use interlocked API to avoid taking a critical section since
    // this can be invoked synchronously during file I/O or
    // asynchronously from the write-back thread.
    do {

        // Set the specified flags on this shared map object.
        OldFlags = m_SharedMapFlags;
        NewFlags = OldFlags | dwFlagsToSet;

    } while (OldFlags != (DWORD)InterlockedCompareExchange ((LONG*)&m_SharedMapFlags, NewFlags, OldFlags));

    return OldFlags;
}


DWORD
FSSharedFileMap_t::ClearFlags (
    DWORD dwFlagsToClear
    )
{
    volatile DWORD OldFlags;
    volatile DWORD NewFlags;

    // Use interlocked API to avoid taking a critical section since
    // this can be invoked synchronously during file I/O or
    // asynchronously from the write-back thread.
    do {

        // Clear the specified flags on this shared map object.
        OldFlags = m_SharedMapFlags;
        NewFlags = OldFlags & ~dwFlagsToClear;

    } while (OldFlags != (DWORD)InterlockedCompareExchange ((LONG*)&m_SharedMapFlags, NewFlags, OldFlags));

    return OldFlags;
}

BOOL 
FSSharedFileMap_t::IsDirty () 
{
    volatile DWORD PageTreeId = m_PageTreeId;
    
    return PageTreeId 
        ? g_NKCacheFuncs.pIsPageTreeDirty(PageTreeId)
        : FALSE;
}

// This is really just a debug wrapper around our usage of the kernel size
BOOL
FSSharedFileMap_t::GetCachedFileSize (
    ULARGE_INTEGER* pFileSize
    )
{
    BOOL result = g_NKCacheFuncs.pPageTreeGetSize (m_PageTreeId, pFileSize);

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
    ULARGE_INTEGER NewFileSize,
    BOOL fDeleteFile
    )
{
    // Someone could be in trouble if they were paging in while the file resize
    // occurs, but the I/O lock should protect against that.
    DEBUGCHK (fDeleteFile || OwnMapLock ());
    DEBUGCHK (fDeleteFile || m_PageTreeId);  // Cached file only unless it's from DeleteFile

    return m_PageTreeId
         ? g_NKCacheFuncs.pPageTreeSetSize (m_PageTreeId, &NewFileSize, fDeleteFile)
         : TRUE;
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

    AcquireMapLock ();

    BOOL fUncached = !m_PageTreeId;
    BOOL fWriteBack = (m_pVolume->IsWriteBack () && (PrivateFileFlags & CACHE_PRIVFILE_WRITE_BACK)) || m_pVolume->IsFlushingDisabled();

    if (fUncached) {
        // Uncached access    
        result = UncachedReadWriteScatterGather (aSegmentArray, cbToAccess,
                                                 pOffsetArray, IsWrite);
   
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

                // Transfer data between the cache and the user buffer
                if (IsWrite) {
                    result = g_NKCacheFuncs.pPageTreeWrite (m_PageTreeId, &pOffsetArray[PageNum], (BYTE*)aSegmentArray[PageNum].Buffer, cbPage, 0);
                    if (result) {
                        WroteData = TRUE;
                        OldFileSize = NewFileSize;
                    }
                } else {
                    result = g_NKCacheFuncs.pPageTreeRead (m_PageTreeId, &pOffsetArray[PageNum], (BYTE*)aSegmentArray[PageNum].Buffer, cbPage, 0);
                }

                if (IsWrite && !result) {
                    // Restore old file size if the write didn't complete
                    VERIFY (SetCachedFileSize (OldFileSize));
                }
                
                PageNum++;
            }
            
            // Now handle write-back or write through.
            if (WroteData) {
                UpdateWriteTime();
                if (fWriteBack) {
                    // Write-back
                    m_pVolume->SignalWrite ();
                } 
            }
        }
    }
    ReleaseMapLock ();

    return result;
}


// NOTENOTE called for ReadFile/WriteFile and ReadFileWithSeek/WriteFileWithSeek
// The caller should call CompletePendingFlush before calling this function,
// to minimize the chance of overflowing the cache with unflushable data.
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
   
    AcquireMapLock ();

    BOOL fUncached = !m_PageTreeId;
    BOOL fWriteBack = (m_pVolume->IsWriteBack () && (PrivateFileFlags & CACHE_PRIVFILE_WRITE_BACK)) || m_pVolume->IsFlushingDisabled();
    
    if (!pBuffer && !cbToAccess && !pcbAccessed && !dwLowOffset && !dwHighOffset) {

        // Special case: call with all 0's to determine whether the function is supported
        result = UncachedReadWriteWithSeek (0, 0, 0, 0, 0, IsWrite);
    
    } else if (fUncached) {
    
        // Uncached access        
        result = UncachedReadWriteWithSeek (pBuffer, cbToAccess, pcbAccessed,
                                            dwLowOffset, dwHighOffset,
                                            IsWrite);
    } else {

        // Cached access
        
        if (!pBuffer || !pcbAccessed) {
            SetLastError(ERROR_INVALID_PARAMETER);
            result = FALSE;
            goto exit;
        }

        *pcbAccessed = 0;

        if (!IsWrite) {
            // read - just call the page tree function to handle it
            result = g_NKCacheFuncs.pPageTreeRead (m_PageTreeId, &StartOffset, pBuffer, cbToAccess, pcbAccessed);
            
        } else {

            ULARGE_INTEGER EndOffset, OldFileSize;
            BOOL  fPreGrowFileSize;

            // pre-grow file size
            VERIFY (GetCachedFileSize (&OldFileSize));
            
            EndOffset.QuadPart = StartOffset.QuadPart + cbToAccess;

            fPreGrowFileSize = (EndOffset.QuadPart > OldFileSize.QuadPart);

            if (fPreGrowFileSize && !SetCachedFileSize (EndOffset)) {
                result = FALSE;
                goto exit;
            }

            // call the page tree function to write data
            result = g_NKCacheFuncs.pPageTreeWrite (m_PageTreeId, &StartOffset, pBuffer, cbToAccess, pcbAccessed);

            // update the write time of the file.
            UpdateWriteTime();

            if (result && fWriteBack) {
                // writeback, just signal pageout thread
                m_pVolume->SignalWrite ();
            }

            if (fPreGrowFileSize && (*pcbAccessed < cbToAccess)) {
                // size pre-grown and written less then requested, reduce file size
                EndOffset.QuadPart = StartOffset.QuadPart + *pcbAccessed;
                if (EndOffset.QuadPart < OldFileSize.QuadPart) {
                    EndOffset.QuadPart = OldFileSize.QuadPart;
                }
                VERIFY (SetCachedFileSize (EndOffset));
            }

        }
    }

exit:
    ReleaseMapLock ();

    return result;
}

BOOL
FSSharedFileMap_t::CompleteWrite (
    DWORD  cbToAccess, 
    DWORD  dwLowOffset,
    DWORD  dwHighOffset,
    DWORD  dwPrivateFileFlags
    )
{
    BOOL result = TRUE;
    ULARGE_INTEGER StartOffset;
    StartOffset.HighPart = dwHighOffset;
    StartOffset.LowPart = dwLowOffset;

    BOOL fUncached = (dwPrivateFileFlags & CACHE_PRIVFILE_UNCACHED);
    BOOL fWriteBack = (m_pVolume->IsWriteBack () && (dwPrivateFileFlags & CACHE_PRIVFILE_WRITE_BACK)) || m_pVolume->IsFlushingDisabled();

    // Explictly flush if the file was opened for write-through or
    // opened for no-buffering and a page tree exists.
    if (!fWriteBack || (m_PageTreeId && fUncached)) {               
        result = FlushPageTree (StartOffset, cbToAccess, FALSE);
    }

    // Only commit transactions if write-through is set.
    if (!fWriteBack) {
        if (result && m_pVolume->CommitTransactionsOnIdle()) {
            // If CommitTransactionsOnIdle is set, then WriteBackData will not commit
            // transactions on the underlying file system, so do it explicitly here.
            result = CommitTransactions();
        }
    }

    return result;
}

BOOL
FSSharedFileMap_t::SetEndOfFile (
    ULARGE_INTEGER Offset
    )
{
    BOOL result = FALSE;
    
    AcquireMapLock ();
    
    if (m_PageTreeId) {
        // Cached file
        result = SetCachedFileSize (Offset);
    } else {
        // Uncached file
        result = UncachedSetEndOfFile (Offset);
    }

    ReleaseMapLock ();

    if (result) {
        m_pVolume->SignalWrite ();
    }
    
    return result;
}


BOOL
FSSharedFileMap_t::WriteBackMap(
    BOOL fStopOnIO
    )
{
    BOOL fResult = TRUE;

    if (m_PageTreeId) {

        fResult = FlushMap (fStopOnIO);
    }

    return fResult;
}

BOOL
FSSharedFileMap_t::InvalidateMap ()
{
    BOOL result = TRUE;

    if (m_PageTreeId) {
        ULARGE_INTEGER StartOffset = {0};       
        result = g_NKCacheFuncs.pPageTreeInvalidate (m_PageTreeId, &StartOffset, 0, 0);
    }
    
    return result;
}


BOOL
FSSharedFileMap_t::FlushMap (
    BOOL fStopOnIO
    )
{
    ULARGE_INTEGER StartOffset = {0};       
    return FlushPageTree (StartOffset, 0, fStopOnIO);
}

BOOL
FSSharedFileMap_t::FlushPageTree(
    ULARGE_INTEGER StartOffset,
    DWORD cbSize,
    BOOL fStopOnIO
    )
{
    BOOL result = TRUE;

    volatile DWORD PageTreeId = m_PageTreeId;
    
    if (PageTreeId) {

        result = g_NKCacheFuncs.pPageTreeFlush (
                    PageTreeId, 
                    &StartOffset, 
                    cbSize, 
                    MAP_FLUSH_WRITEBACK, 
                    fStopOnIO ? m_pVolume->GetCancelFlushFlag() : NULL);
        
    }

    return result;    
}


BOOL
FSSharedFileMap_t::CommitTransactions()
{
    AcquireMapLock ();
    if (IsWriteTimeCached()) {
        SetFileTime (NULL, NULL, &m_WriteTime);
    }  
    ReleaseMapLock ();
    
    return UncachedFlushFileBuffers();
}

DWORD
FSSharedFileMap_t::GetFileSize (
    PDWORD pFileSizeHigh        // Comes from the user
    )
{
    ULARGE_INTEGER FileSize;
    
    FileSize.QuadPart = (ULONGLONG) -1;
    SetLastError (NO_ERROR);
    
    AcquireMapLock ();
    
    if (m_PageTreeId) {
        // Cached file
        GetCachedFileSize (&FileSize);
    } else {
        // Uncached file
        UncachedGetFileSize (&FileSize);
    }
    
    ReleaseMapLock ();

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
    
    AcquireMapLock ();
    
    result = UncachedGetFileInformationByHandle (pFileInfo);
    if (result && m_PageTreeId) {
        // Cached file, might be more up to date info in the cache
        // NOTENOTE currently file times are not maintained by the cache
        result = GetCachedFileSize (&FileSize);
    }
    
    if (IsWriteTimeCached()) {
        pFileInfo->ftLastWriteTime = m_WriteTime;
    }
    
    ReleaseMapLock ();

    if (result && m_PageTreeId && pFileInfo) {
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
    AcquireMapLock();
    BOOL result = UncachedGetFileTime (pCreation, pLastAccess, pLastWrite);
    if (IsWriteTimeCached() && result && (NULL !=  pLastWrite)) {
        *pLastWrite = m_WriteTime;
    }
    ReleaseMapLock();
    return result;
}


BOOL
FSSharedFileMap_t::SetFileTime (
    const FILETIME* pCreation, 
    const FILETIME* pLastAccess, 
    const FILETIME* pLastWrite
    )
{
    AcquireMapLock();
    BOOL result = UncachedSetFileTime (pCreation, pLastAccess, pLastWrite);
    if (result) {
        // If the write time is cached, then reset it, since the underlying 
        // file system contains the up to date time.
        if (IsWriteTimeCached()) {
            m_WriteTime.dwHighDateTime = 0;
            m_WriteTime.dwLowDateTime = 0;
        }
    }
    ReleaseMapLock();

    if (result) {
        m_pVolume->SignalWrite();
    }

    return result;
}

void 
FSSharedFileMap_t::UpdateWriteTime()
{
    if (m_pVolume->IsWriteBack()) {
        DEBUGCHK(OwnMapLock());
        SYSTEMTIME LocalTime;
        GetLocalTime(&LocalTime);
        
        FILETIME LocalFileTime;
        SystemTimeToFileTime(&LocalTime, &LocalFileTime);
        LocalFileTimeToFileTime(&LocalFileTime, &m_WriteTime);
    }
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

    case FSCTL_FLUSH_BUFFERS:
        result = FlushMap();
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
