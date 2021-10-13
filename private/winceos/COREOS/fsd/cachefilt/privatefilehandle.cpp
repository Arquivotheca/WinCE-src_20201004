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


PrivateFileHandle_t::PrivateFileHandle_t (
    FSSharedFileMap_t* pMap,
    DWORD  dwSharing,
    DWORD  dwAccess,
    DWORD  dwFlagsAndAttributes,
    DWORD  ProcessId
    )
{
    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:PrivateFileHandle_t, pHandle=0x%08x Sharing=0x%08x Access=0x%08x ProcID=0x%08x\r\n",
                            this, dwSharing, dwAccess, ProcessId));

    m_pSharedMap = pMap;
    m_pNext = NULL;
    m_AccessFlags = dwAccess;
    m_SharingFlags = dwSharing;
    m_FilePosition.QuadPart = 0;
    m_ProcessId = ProcessId;
    
    m_PrivFileFlags = 0;
    if (dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING) {
        m_PrivFileFlags |= CACHE_PRIVFILE_UNCACHED;
    }
    if (!(dwFlagsAndAttributes & FILE_FLAG_WRITE_THROUGH)) {
        m_PrivFileFlags |= CACHE_PRIVFILE_WRITE_BACK;
    }
}


PrivateFileHandle_t::~PrivateFileHandle_t ()
{
    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:~PrivateFileHandle_t, pHandle=0x%08x\r\n", this));
    
    m_AccessFlags = 0;  // Block further use
    FSDMGR_RemoveFileLockEx (FCFILT_AcquireFileLockState, 
                             FCFILT_ReleaseFileLockState, (DWORD) this);
    
    if (m_pSharedMap) {
        m_pSharedMap->CloseFileHandle (this);  // May delete the map
        m_pSharedMap = NULL;
    }       
    DEBUGCHK (!m_pNext);
}
 

// ReadFile / WriteFile implementation
BOOL
PrivateFileHandle_t::ReadWrite (
    PBYTE   pBuffer,
    DWORD   cbToAccess,
    LPDWORD pcbAccessed, 
    BOOL    IsWrite
    )
{
    BOOL  result = FALSE;
    DWORD cbAccessed = 0;  // Local var to guarantee validity
    
    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:PrivateFileHandle_t::ReadWrite, pHandle=0x%08x Offset=0x%08x,%08x Bytes=0x%08x Write=%u\r\n",
                            this, m_FilePosition.HighPart, m_FilePosition.LowPart,
                            cbToAccess, IsWrite));

    // Pending flushes must be completed before new writes to the cache,
    // to prevent the cache from filling up with unflushable data.
    if (IsWrite && !m_pSharedMap->CompletePendingFlush (m_PrivFileFlags)) {
        return FALSE;
    }

    m_pSharedMap->AcquireIOLock ();
    
    // Check that the access is allowed and does not conflict with a lock 
    if (!(m_AccessFlags & (IsWrite ? GENERIC_WRITE : GENERIC_READ))) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:PrivateFileHandle_t::ReadWrite, !! Denying invalid access to handle (write=%u)\r\n", IsWrite));
        SetLastError (ERROR_ACCESS_DENIED);

    } else if (FSDMGR_TestFileLock (FCFILT_AcquireFileLockState,
                                    FCFILT_ReleaseFileLockState, 
                                    (DWORD) this, IsWrite ? FALSE : TRUE,
                                    cbToAccess)) {

        // FSSharedFileMap only exports *WithSeek due to lack of position
        result = m_pSharedMap->ReadWriteWithSeek (pBuffer, cbToAccess,
                                                  &cbAccessed,
                                                  m_FilePosition.LowPart,
                                                  m_FilePosition.HighPart,
                                                  IsWrite, m_PrivFileFlags);
        m_FilePosition.QuadPart += cbAccessed;
    }
      
    m_pSharedMap->ReleaseIOLock ();
    
    // Avoid page fault or crash while holding IO lock by touching ptr outside the lock
    if (pcbAccessed) {
        *pcbAccessed = cbAccessed;
    }

    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:PrivateFileHandle_t::ReadWrite, !! Failed, write=%u error=%u\r\n",
               IsWrite, GetLastError()));
    return result;
}


BOOL
PrivateFileHandle_t::ReadWriteScatterGather (
    FILE_SEGMENT_ELEMENT aSegmentArray[], 
    DWORD   cbToAccess, 
    LPDWORD pReserved,
    BOOL    IsWrite
    )
{
    // FILE_SEGMENT_ELEMENT can be interchanged with ULARGE_INTEGER
    ULARGE_INTEGER* pOffsetArray = (ULARGE_INTEGER*) pReserved;
    BOOL result = FALSE;
    
    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:PrivateFileHandle_t::ReadWriteScatterGather, pHandle=0x%08x Bytes=0x%08x Write=%u\r\n",
                            this, cbToAccess, IsWrite));
    
    // Pending flushes must be completed before new writes to the cache,
    // to prevent the cache from filling up with unflushable data.
    if (IsWrite && !m_pSharedMap->CompletePendingFlush (m_PrivFileFlags)) {
        return FALSE;
    }

    m_pSharedMap->AcquireIOLock ();
    
    // Check that the access is allowed
    if (!(m_AccessFlags & (IsWrite ? GENERIC_WRITE : GENERIC_READ))) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:PrivateFileHandle_t::ReadWriteScatterGather, !! Denying invalid access to handle (write=%u)\r\n", IsWrite));
        SetLastError (ERROR_ACCESS_DENIED);
    } else {
        // NOTENOTE no checking for conflict with locked file ranges
        
        // If the user didn't pass in an offset array, create one for them.
        if (!pOffsetArray) {
            DWORD cbPage   = UserKInfo[KINX_PAGESIZE];
            // Round up to page boundary
            DWORD NumPages = (cbToAccess + cbPage - 1) / cbPage;

            pOffsetArray = new ULARGE_INTEGER [NumPages];
            DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:PrivateFileHandle_t::ReadWriteScatterGather, Alloc OffsetArray of %u blocks, 0x%08x\r\n",
                                   NumPages, pOffsetArray));
            if (pOffsetArray) {
                // Fill the array with pages starting at the given offset
                ULONGLONG Offset = m_FilePosition.QuadPart;
                for (DWORD PageNum = 0; PageNum < NumPages; PageNum++) {        
                    pOffsetArray[PageNum].QuadPart = Offset;
                    Offset += cbPage;
                }
            }
        }
        
        if (pOffsetArray) {
            result = m_pSharedMap->ReadWriteScatterGather (aSegmentArray,
                                                           cbToAccess,
                                                           pOffsetArray,
                                                           IsWrite, m_PrivFileFlags);
            
            // Move file offset if the user did not pass in an offset array
            if (result && !pReserved) {
                m_FilePosition.QuadPart += cbToAccess;
            }
        }
    }
      
    m_pSharedMap->ReleaseIOLock ();
    
    // Free the offset array if the user did not pass one in
    if (!pReserved && pOffsetArray) {
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:PrivateFileHandle_t::ReadWriteScatterGather, Free OffsetArray 0x%08x\r\n", pOffsetArray));
        delete [] pOffsetArray;
    }
    
    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:PrivateFileHandle_t::ReadWriteScatterGather, !! Failed, write=%u error=%u\r\n",
               IsWrite, GetLastError()));
    return result;
}


BOOL
PrivateFileHandle_t::ReadWriteWithSeek (
    PBYTE  pBuffer, 
    DWORD  cbToAccess, 
    PDWORD pcbAccessed, 
    DWORD  dwLowOffset,
    DWORD  dwHighOffset,
    BOOL   IsWrite
    )
{
    BOOL  result = FALSE;
    DWORD cbAccessed = 0;  // Local var to guarantee validity
    
    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:PrivateFileHandle_t::ReadWriteWithSeek, pHandle=0x%08x Offset=0x%08x,%08x Bytes=0x%08x Write=%u\r\n",
                            this, dwHighOffset, dwLowOffset, cbToAccess, IsWrite));
    
    // Special case: call with all 0's to determine whether the function is supported
    if (!pBuffer && !cbToAccess && !pcbAccessed
        && !dwLowOffset && !dwHighOffset
        && (m_AccessFlags & (IsWrite ? GENERIC_WRITE : GENERIC_READ))) {
        // Pass through because the filter doesn't know
        return m_pSharedMap->ReadWriteWithSeek (0, 0, 0, 0, 0, 0, IsWrite);
    }

    // Pending flushes must be completed before new writes to the cache,
    // to prevent the cache from filling up with unflushable data.
    if (IsWrite && !m_pSharedMap->CompletePendingFlush (m_PrivFileFlags)) {
        return FALSE;
    }

    m_pSharedMap->AcquireIOLock ();
    
    // Check that access is allowed and does not conflict with a lock 
    if (!(m_AccessFlags & (IsWrite ? GENERIC_WRITE : GENERIC_READ))) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:PrivateFileHandle_t::ReadWriteWithSeek, !! Denying invalid access to handle (write=%u)\r\n", IsWrite));
        SetLastError (ERROR_ACCESS_DENIED);

    } else if (FSDMGR_TestFileLockEx (FCFILT_AcquireFileLockState,
                                      FCFILT_ReleaseFileLockState, 
                                      (DWORD) this, IsWrite ? FALSE : TRUE,
                                      cbToAccess, dwLowOffset, dwHighOffset)) {

        result = m_pSharedMap->ReadWriteWithSeek (pBuffer, cbToAccess,
                                                  &cbAccessed,
                                                  dwLowOffset, dwHighOffset,
                                                  IsWrite, m_PrivFileFlags);
        m_FilePosition.LowPart  = dwLowOffset;
        m_FilePosition.HighPart = dwHighOffset;
        m_FilePosition.QuadPart += cbAccessed;
    }
      
    m_pSharedMap->ReleaseIOLock ();
    
    // Avoid page fault or crash while holding IO lock by touching ptr outside the lock
    if (pcbAccessed) {
        *pcbAccessed = cbAccessed;
    }

    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:PrivateFileHandle_t::ReadWriteWithSeek, !! Failed, write=%u error=%u\r\n",
              IsWrite, GetLastError()));
    return result;
}


BOOL
PrivateFileHandle_t::SetEndOfFile ()
{
    BOOL result = FALSE;

    m_pSharedMap->AcquireIOLock ();

    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:PrivateFileHandle_t::SetEndOfFile, pHandle=0x%08x Offset=0x%08x,%08x\r\n",
                            this, m_FilePosition.HighPart, m_FilePosition.LowPart));

    // Check that write is allowed and does not conflict with a lock 
    if (!(m_AccessFlags & GENERIC_WRITE)) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:PrivateFileHandle_t::SetEndOfFile, !! Denying modification to non-writable handle\r\n"));
        SetLastError (ERROR_ACCESS_DENIED);
    } else {
        // NOTENOTE no checking for conflict with locked file ranges
        result = m_pSharedMap->SetEndOfFile (m_FilePosition);

        // Flush file buffers on a write-through handle
        if (!(m_PrivFileFlags & CACHE_PRIVFILE_WRITE_BACK)) {
            result =  m_pSharedMap->FlushFileBuffers() && result;
        }
    }

    m_pSharedMap->ReleaseIOLock ();
    
    return result;
}


DWORD
PrivateFileHandle_t::SetFilePointer (
    LONG  lDistanceToMove, 
    PLONG lpDistanceToMoveHigh, 
    DWORD dwMoveMethod
    )
{
    ULARGE_INTEGER ReturnValue;
    ULARGE_INTEGER DistanceToMove;

    ReturnValue.QuadPart = (ULONGLONG) -1;
    
    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:PrivateFileHandle_t::SetFilePointer, pHandle=0x%08x\r\n",
                            this));

    // SetFilePointer sets LastError to NO_ERROR so that callers can tell the
    // difference between low size=0xFFFFFFFF and an error.
    SetLastError (NO_ERROR);

    DistanceToMove.LowPart = lDistanceToMove;
    if (lpDistanceToMoveHigh) {
        DistanceToMove.HighPart = *lpDistanceToMoveHigh;
    } else if (lDistanceToMove >= 0) {
        DistanceToMove.HighPart = 0;
    } else {
        DistanceToMove.QuadPart = (LONGLONG) lDistanceToMove;
    }

    m_pSharedMap->AcquireIOLock ();

    // NOTENOTE Object store FS requires either read or write access to set
    // the file pointer, but FATFS does not, so don't block it.

    // Check for validity and calculate the new position
    switch (dwMoveMethod) {
    case FILE_BEGIN:
        {
            // Avoid underflow
            if ((LONGLONG) DistanceToMove.QuadPart < 0) {
                SetLastError (ERROR_NEGATIVE_SEEK);
                goto exit;
            }
            break;
        }
    
    case FILE_CURRENT:
        {
            // Avoid overflow and underflow
            if ((LONGLONG) DistanceToMove.QuadPart < 0) {
                // Negative distance
                if (m_FilePosition.QuadPart < (ULONGLONG) (0 - (LONGLONG)DistanceToMove.QuadPart)) {
                    SetLastError (ERROR_NEGATIVE_SEEK);
                    goto exit;
                }
            } else {
                // Positive distance, don't go beyond max file size (-1)
                if (DistanceToMove.QuadPart > (ULONGLONG)-1 - m_FilePosition.QuadPart) {
                    SetLastError (ERROR_NEGATIVE_SEEK);
                    goto exit;
                }
            }
            DistanceToMove.QuadPart += m_FilePosition.QuadPart;
            break;
        }
    
    case FILE_END:
        {
            ULARGE_INTEGER FileSize;
            
            FileSize.HighPart = 0;
            FileSize.LowPart = m_pSharedMap->GetFileSize (&FileSize.HighPart);
            
            // Avoid overflow and underflow
            if ((LONGLONG) DistanceToMove.QuadPart < 0) {
                // Negative distance
                if (FileSize.QuadPart < (ULONGLONG) (0 - (LONGLONG)DistanceToMove.QuadPart)) {
                    SetLastError (ERROR_NEGATIVE_SEEK);
                    goto exit;
                }
            } else {
                // Positive distance, don't go beyond max file size (-1)
                if (DistanceToMove.QuadPart > (ULONGLONG)-1 - FileSize.QuadPart) {
                    SetLastError (ERROR_NEGATIVE_SEEK);
                    goto exit;
                }
            }
            DistanceToMove.QuadPart += FileSize.QuadPart;
            break;
        }

    default:
        SetLastError (ERROR_INVALID_PARAMETER);
        goto exit;
    }

    m_FilePosition = DistanceToMove;
    ReturnValue = DistanceToMove;

    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:PrivateFileHandle_t::SetFilePointer, pHandle=0x%08x Offset=0x%08x,%08x\r\n",
                            this, m_FilePosition.HighPart, m_FilePosition.LowPart));

exit:
    m_pSharedMap->ReleaseIOLock ();
    
    // Avoid page fault or crash while holding IO lock by touching ptr outside the lock
    if (lpDistanceToMoveHigh) {
        *lpDistanceToMoveHigh = ReturnValue.HighPart; 
    }
    return ReturnValue.LowPart;
}


BOOL
PrivateFileHandle_t::LockFileEx (
    DWORD dwFlags, 
    DWORD dwReserved,
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh,
    OVERLAPPED * pOverlapped
    )
{
    // This function does not make a copy of the file position, but the position
    // is only read once during the lock process (besides this debug printf).

    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:PrivateFileHandle_t::LockFileEx, pHandle=0x%08x Offset=0x%08x,%08x Bytes=0x%08x,%08x\r\n",
                            this, m_FilePosition.HighPart, m_FilePosition.LowPart,
                            nNumberOfBytesToLockHigh, nNumberOfBytesToLockLow));
    return FSDMGR_InstallFileLock ( 
        (PACQUIREFILELOCKSTATE) FCFILT_AcquireFileLockState, 
        (PRELEASEFILELOCKSTATE) FCFILT_ReleaseFileLockState,
        (DWORD) this, 
        dwFlags,
        dwReserved, 
        nNumberOfBytesToLockLow, 
        nNumberOfBytesToLockHigh, 
        pOverlapped);
}

BOOL PrivateFileHandle_t::UnlockFileEx (
    DWORD dwReserved, 
    DWORD nNumberOfBytesToUnlockLow, 
    DWORD nNumberOfBytesToUnlockHigh,
    OVERLAPPED * pOverlapped
    )
{
    // This function does not make a copy of the file position, but the position
    // is only read once during the lock process (besides this debug printf).
    DEBUGMSG (ZONE_HANDLE, (L"CACHEFILT:PrivateFileHandle_t::UnlockFileEx, pHandle=0x%08x Offset=0x%08x,%08x Bytes=0x%08x,%08x\r\n",
                            this, m_FilePosition.HighPart, m_FilePosition.LowPart,
                            nNumberOfBytesToUnlockHigh, nNumberOfBytesToUnlockLow));
    return FSDMGR_RemoveFileLock ( 
        (PACQUIREFILELOCKSTATE) FCFILT_AcquireFileLockState, 
        (PRELEASEFILELOCKSTATE) FCFILT_ReleaseFileLockState,
        (DWORD) this, 
        dwReserved, 
        nNumberOfBytesToUnlockLow, 
        nNumberOfBytesToUnlockHigh, 
        pOverlapped);
}


//
// Helper functions used by LockFileEx and UnlockFileEx
//

BOOL
FCFILT_AcquireFileLockState (
    DWORD dwHandle, 
    PFILELOCKSTATE* ppFileLockState
    )
{
    PrivateFileHandle_t *pHandle = (PrivateFileHandle_t *) dwHandle;
    if (pHandle && pHandle->m_pSharedMap) {
        return pHandle->AcquireFileLockState (ppFileLockState);
    }
    return FALSE;
}


BOOL
FCFILT_ReleaseFileLockState (
    DWORD dwHandle, 
    PFILELOCKSTATE* ppFileLockState
    )
{
    PrivateFileHandle_t *pHandle = (PrivateFileHandle_t *)dwHandle;
    if (pHandle && pHandle->m_pSharedMap) {
        return pHandle->m_pSharedMap->ReleaseFileLockState (ppFileLockState);
    }
    return FALSE;
}


BOOL
PrivateFileHandle_t::DeviceIoControl (
    DWORD  dwIoControlCode,
    PVOID  pInBuf,
    DWORD  nInBufSize,
    PVOID  pOutBuf,
    DWORD  nOutBufSize,
    PDWORD pBytesReturned
    )
{
    BOOL result = TRUE;

    if (FSCTL_SET_FILE_CACHE == dwIoControlCode) {
        if (pInBuf && (sizeof(FILE_CACHE_INFO) >= nInBufSize)) {
            // Only disable is supported right now
            FILE_CACHE_INFO* pCacheInfo = (FILE_CACHE_INFO*) pInBuf;
            if (FileCacheDisableStandard == pCacheInfo->fInfoLevelId) {
                // Disable caching for this file handle.
                // Any existing cached data will be flushed eventually.
                m_PrivFileFlags |= CACHE_PRIVFILE_UNCACHED;
            } else {
                SetLastError (ERROR_NOT_SUPPORTED);
                result = FALSE;
            }
        } else {
            SetLastError (ERROR_INVALID_PARAMETER);
            result = FALSE;
        }
    }

    if (FSCTL_SET_EXTENDED_FLAGS == dwIoControlCode) {
        // Disable caching if file is set to transact data.
        if (nInBufSize >= sizeof(DWORD)) {
            DWORD* pdwFlags = (DWORD*) pInBuf;
            if (*pdwFlags & CE_FILE_FLAG_TRANS_DATA) {
                m_PrivFileFlags |= CACHE_PRIVFILE_UNCACHED;
            }
        }
    }

    if (!m_pSharedMap->DeviceIoControl (dwIoControlCode, pInBuf, nInBufSize,
                                        pOutBuf, nOutBufSize, pBytesReturned,
                                        m_PrivFileFlags)) {
        result = FALSE;
    }
    return result;
}
