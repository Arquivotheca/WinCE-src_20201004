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


DBGPARAM dpCurSettings =
{
    TEXT("CacheFilt"),
    {
        TEXT("Initialization"), TEXT("APIs"),                   TEXT("Underlying I/O"), TEXT("Mem Alloc"), 
        TEXT("Volume"),         TEXT("File Handle"),            TEXT("File Map"),       TEXT("View Pool"), 
        TEXT(""),               TEXT(""),                       TEXT(""),               TEXT(""), 
        TEXT("CeLog"),          TEXT("Break on some warnings"), TEXT("Warnings"),       TEXT("Errors") 
    },
    CACHEFILT_ZONES
};

HANDLE g_hHeap;


//==============================================================================
//
// Implement a separate heap for all cache data
//
//==============================================================================

void* operator new (size_t cbSize) 
{ 
    return g_hHeap ? HeapAlloc (g_hHeap, 0, cbSize) : NULL;
}

void* operator new[] (size_t cbSize) 
{ 
    return operator new (cbSize);
}

void operator delete (void *pMem) 
{
    PREFAST_DEBUGCHK (NULL != g_hHeap);
    HeapFree (g_hHeap, 0, pMem);
}

void operator delete[] (void *pvMem) 
{
    return operator delete (pvMem);
}

static BOOL InitHeap ()
{
    g_hHeap = HeapCreate (0, 0x1000, 0);
    return g_hHeap ? TRUE : FALSE;
}

static void CleanupHeap()
{
    HeapDestroy (g_hHeap);
}


//==============================================================================
//
// VOLUME APIS
//
//==============================================================================

extern "C" CachedVolume_t*
FCFILT_HookVolume (
    HDSK hdsk, 
    FILTERHOOK* pHook
    )
{
    DEBUGMSG (ZONE_API && ZONE_INIT,
              (L"CACHEFILT:HookVolume, hDsk=0x%08x pHook=0x%08x\r\n", hdsk, pHook));
    
    // If something went wrong with pool initialization, don't hook any volumes.
    if (!CheckViewPool ()) {
        return NULL;
    }

    CachedVolume_t* pVolume = new CachedVolume_t (hdsk, pHook);
    DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:HookVolume, Alloc CachedVolume 0x%08x\r\n", pVolume));

    if (pVolume && !pVolume->Init ()) {
        delete pVolume;
        pVolume = NULL;
    }
    
    DEBUGMSG (ZONE_API, (L"CACHEFILT:HookVolume, return pVolume=0x%08x\r\n", pVolume));
    return pVolume;
}


extern "C" BOOL
FCFILT_UnhookVolume (
    CachedVolume_t* pVolume
    )
{
    DEBUGMSG (ZONE_API && ZONE_INIT,
              (L"CACHEFILT:UnhookVolume, pVolume=0x%08x\r\n", pVolume));
    
    delete pVolume;
    return TRUE;
}


extern "C" BOOL
FCFILT_GetVolumeInfo (
    PVOLUME pvol, 
    FSD_VOLUME_INFO *pInfo
    )
{
    CachedVolume_t *pVolume = (CachedVolume_t *) pvol;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:GetVolumeInfo, pVolume=0x%08x\r\n", pVolume));
    return pVolume->GetVolumeInfo (pInfo);
}


extern "C" void
FCFILT_Notify (
    PVOLUME pvol, 
    DWORD   dwFlags
    )
{
    CachedVolume_t* pVolume = (CachedVolume_t *) pvol;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:Notify, pVolume=0x%08x Flags=0x%08x\r\n", pVolume, dwFlags));
    pVolume->Notify (dwFlags);
}


extern "C" BOOL
FCFILT_RegisterFileSystemFunction (
    PVOLUME pvol, 
    DWORD   dwFlags
    )
{
    CachedVolume_t *pVolume = (CachedVolume_t *) pvol;
    
    DEBUGMSG (ZONE_API, (L"CACHEFILT:RegisterFileSystemFunction, pVolume=0x%08x Flags=0x%08x\r\n",
                         pVolume, dwFlags));
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.RegisterFileSystemFunction, hVolume=0x%08x\r\n",
                        pVolume->m_FilterHook.GetVolumeHandle ()));
    
    BOOL result = pVolume->m_FilterHook.RegisterFileSystemFunction ((SHELLFILECHANGEFUNC_t) dwFlags);
    
    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:RegisterFileSystemFunction, !! Failed, error=%u\r\n", GetLastError ()));
    return result;
}


extern "C" BOOL
FCFILT_FsIoControl(
    PVOLUME pvol,
    DWORD   dwIoControlCode,
    PVOID   pInBuf,
    DWORD   nInBufSize,
    PVOID   pOutBuf,
    DWORD   nOutBufSize,
    PDWORD  pBytesReturned,
    OVERLAPPED* pOverlapped
    )
{
    CachedVolume_t *pVolume = (CachedVolume_t *) pvol;
    
    DEBUGMSG (ZONE_API, (L"CACHEFILT:FsIoControl, pVolume=0x%08x ioctl=%u pIn=0x%08x(%u) pOut=0x%08x(%u)\r\n",
                         pVolume, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize));
    // pOverlapped is not supported -- stop passing it or disable this zone to stop the DEBUGCHK
    DEBUGCHK (!pOverlapped || !ZONE_WARNING_BREAK);
    return pVolume->FsIoControl (dwIoControlCode, pInBuf, nInBufSize,
                                 pOutBuf, nOutBufSize, pBytesReturned);
}


//==============================================================================
//
// PATH-BASED APIS
//
//==============================================================================

extern "C" BOOL
FCFILT_CreateDirectoryW (
    PVOLUME pvol, 
    PCWSTR  pwsPathName, 
    PSECURITY_ATTRIBUTES pSecurityAttributes
    )
{
    BOOL result = FALSE;
    CachedVolume_t* pVolume = (CachedVolume_t*) pvol;

    DEBUGMSG (ZONE_API, (L"CACHEFILT:CreateDirectory, pVolume=0x%08x Name=%s\r\n",
                         pVolume, pwsPathName));
    
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.CreateDirectoryW, hVolume=0x%08x\r\n",
                        pVolume->m_FilterHook.GetVolumeHandle ()));
    result = pVolume->m_FilterHook.CreateDirectoryW (pwsPathName, pSecurityAttributes);

    if (result) {
        pVolume->m_pDirectoryHash->HashVolume();
        pVolume->m_pDirectoryHash->AddFilePathToHashMap( pwsPathName );
    }

    // If the commit transactions is delayed until idle, then signal the idle thread
    // that the file system was modified.
    if (pVolume->CommitTransactionsOnIdle()) {
        pVolume->SignalWrite();
    }

    // Treat as a warning if the directory already exists.  Else it's an error.
    DEBUGMSG (!result
              && (ZONE_WARNING || (ZONE_ERROR && (ERROR_ALREADY_EXISTS != GetLastError ()))),
              (L"CACHEFILT:CreateDirectory, !! Failed, error=%u\r\n", GetLastError ()));
    return result;
}


extern "C" BOOL
FCFILT_RemoveDirectoryW (
    PVOLUME pvol, 
    PCWSTR  pwsPathName
    )
{
    CachedVolume_t *pVolume = (CachedVolume_t *) pvol;

    DEBUGMSG (ZONE_API, (L"CACHEFILT:RemoveDirectoryW, pVolume=0x%08x Name=%s\r\n",
                         pVolume, pwsPathName));
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.RemoveDirectoryW, hVolume=0x%08x\r\n",
                        pVolume->m_FilterHook.GetVolumeHandle ()));
    
    // This will bail immediately if the volume is already hashed or we're not
    // supposed to be hashing it.  It has to be called during create file,
    // because the underlying file system may not be accessible when initializing
    // the filter.
    pVolume->m_pDirectoryHash->HashVolume();

    BOOL result = FALSE;
    if (!pVolume->m_pDirectoryHash->FileMightExist (pwsPathName) ) {
        SetLastError( ERROR_PATH_NOT_FOUND );
    } else {
        result = pVolume->m_FilterHook.RemoveDirectoryW (pwsPathName);
    }

    // If the commit transactions is delayed until idle, then signal the idle thread
    // that the file system was modified.
    if (pVolume->CommitTransactionsOnIdle()) {
        pVolume->SignalWrite();
    }

    // Treat as a warning if the directory is not found.  Else it's an error.
    DEBUGMSG (!result
              && (ZONE_WARNING || (ZONE_ERROR && (ERROR_PATH_NOT_FOUND == GetLastError ()))),
              (L"CACHEFILT:RemoveDirectoryW, !! Failed, error=%u\r\n", GetLastError ()));
    return result;
}


extern "C" HANDLE
FCFILT_CreateFileW (
    PVOLUME pvol,
    HANDLE  hProcess,
    PCWSTR  pwsFileName,
    DWORD   dwAccess,
    DWORD   dwShareMode,
    PSECURITY_ATTRIBUTES pSecurityAttributes,
    DWORD   dwCreate,
    DWORD   dwFlagsAndAttributes,
    HANDLE  hTemplateFile
    )
{
    CachedVolume_t* pVolume = (CachedVolume_t*) pvol;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:CreateFile, pVolume=0x%08x ProcId=0x%08x Name=%s Access=0x%08x ShareMode=0x%08x Create=0x%08x Attribs=0x%08x\r\n",
                         pVolume, hProcess, pwsFileName, dwAccess, dwShareMode, dwCreate,
                         dwFlagsAndAttributes));
    return pVolume->CreateFile ((DWORD) hProcess, pwsFileName, dwAccess, dwShareMode, 
                                pSecurityAttributes, dwCreate, dwFlagsAndAttributes,
                                hTemplateFile);
}


extern "C" BOOL
FCFILT_MoveFileW (
    PVOLUME pvol, 
    PCWSTR  pwsOldFileName, 
    PCWSTR  pwsNewFileName
    )
{
    CachedVolume_t* pVolume = (CachedVolume_t*) pvol;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:MoveFileW, pVolume=0x%08x OldName=%s NewName=%s\r\n",
                         pVolume, pwsOldFileName, pwsNewFileName));
    return pVolume->MoveFile (pwsOldFileName, pwsNewFileName);
}


extern "C" BOOL
FCFILT_DeleteAndRenameFileW (
    PVOLUME pvol,
    PCWSTR  pwsOldFileName, 
    PCWSTR  pwsNewFileName
    )
{
    CachedVolume_t* pVolume = (CachedVolume_t*) pvol;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:DeleteAndRenameFile, pVolume=0x%08x OldName=%s NewName=%s\r\n", 
              pVolume, pwsOldFileName, pwsNewFileName));
    return pVolume->DeleteAndRenameFile (pwsOldFileName, pwsNewFileName);
}


extern "C" BOOL
FCFILT_DeleteFileW (
    PVOLUME pvol, 
    PCWSTR  pwsFileName
    )
{
    CachedVolume_t *pVolume = (CachedVolume_t *) pvol;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:DeleteFile, pVolume=0x%08x Name=%s\r\n", pVolume, pwsFileName));
    return pVolume->DeleteFile (pwsFileName);
}


extern "C" DWORD
FCFILT_GetFileAttributesW (
    PVOLUME pvol,
    PCWSTR  pwsFileName
    )
{
    CachedVolume_t * pVolume = (CachedVolume_t *) pvol;
    DWORD dwError = ERROR_SUCCESS;

    DEBUGMSG (ZONE_API, (L"CACHEFILT:GetFileAttributesW, pVolume=0x%08x Name=%s\r\n",
                         pVolume, pwsFileName));
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.GetFileAttributesW, hVolume=0x%08x\r\n",
                        pVolume->m_FilterHook.GetVolumeHandle ()));
    
    // This will bail immediately if the volume is already hashed or we're not
    // supposed to be hashing it.  It has to be called during create file,
    // because the underlying file system may not be accessible when initializing
    // the filter.
    pVolume->m_pDirectoryHash->HashVolume();

    DWORD Attribs = INVALID_FILE_ATTRIBUTES;
    if (!pVolume->m_pDirectoryHash->FileMightExist( pwsFileName, &dwError ) ) {
        SetLastError( dwError );
    } else {
        Attribs = pVolume->m_FilterHook.GetFileAttributesW (pwsFileName);
    }

    // Treat as a warning since we often use GFA to test for existence.  Error if something other than not-found.
#if 0
   DEBUGMSG (((DWORD)-1 == Attribs)
              && (ZONE_WARNING || (ZONE_ERROR && (ERROR_FILE_NOT_FOUND != GetLastError ())
                                              && (ERROR_PATH_NOT_FOUND != GetLastError ()))),
              (L"CACHEFILT:GetFileAttributesW, !! Failed, error=%u\r\n", GetLastError ()));
#endif
   return Attribs;
}


extern "C" BOOL
FCFILT_SetFileAttributesW (
    PVOLUME pvol, 
    PCWSTR  pwsFileName, 
    DWORD   dwFileAttributes
    )
{
    CachedVolume_t *pVolume = (CachedVolume_t *) pvol;
    DWORD dwError = ERROR_SUCCESS;

    DEBUGMSG (ZONE_API, (L"CACHEFILT:SetFileAttributesW, pVolume=0x%08x Name=%s Attrib=0x%08x\r\n",
                         pVolume, pwsFileName, dwFileAttributes));

    // This will bail immediately if the volume is already hashed or we're not
    // supposed to be hashing it.  It has to be called during create file,
    // because the underlying file system may not be accessible when initializing
    // the filter.
    pVolume->m_pDirectoryHash->HashVolume();

    BOOL result = FALSE;
    if (!pVolume->m_pDirectoryHash->FileMightExist( pwsFileName, &dwError ) ) {
        SetLastError( dwError );
    } else {
        result = pVolume->SetFileAttributes (pwsFileName, dwFileAttributes);
    }
    
    return result;
}


extern "C" BOOL
FCFILT_GetDiskFreeSpaceW (
    PVOLUME pvol,
    PCWSTR  pwsPathName,
    PDWORD  pSectorsPerCluster, 
    PDWORD  pBytesPerSector,
    PDWORD  pFreeClusters, 
    PDWORD  pClusters
    )
{
    CachedVolume_t * pVolume = (CachedVolume_t *) pvol;
    
    DEBUGMSG (ZONE_API, (L"CACHEFILT: GetDiskFreeSpaceW, pVolume=0x%08x Name=%s\r\n",
                         pVolume, pwsPathName));
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.GetDiskFreeSpaceW, hVolume=0x%08x\r\n",
                        pVolume->m_FilterHook.GetVolumeHandle ()));
    
    // NOTENOTE this call doesn't account for the amount of space that will be taken up
    // or freed after cached file changes are flushed to the disk.  But it might be
    // too big of a perf hit to flush all file data or file sizes.
    BOOL result = pVolume->m_FilterHook.GetDiskFreeSpaceW (pwsPathName, pSectorsPerCluster, 
                                                           pBytesPerSector, pFreeClusters,
                                                           pClusters);

    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:GetDiskFreeSpaceW, !! Failed, error=%u\r\n", GetLastError ()));
    return result;
}


//==============================================================================
//
// FILE HANDLE-BASED APIS
//
//==============================================================================


extern "C" BOOL
FCFILT_CloseFile (
    PFILE pfile
    )
{
    LRESULT LastError = GetLastError();
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t*) pfile;

    DEBUGMSG (ZONE_API, (L"CACHEFILT:CloseFile, pHandle=0x%08x\r\n", pHandle));
    
    // This call will remove the handle from its map, and delete the map and
    // the volume if necessary.
    DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:CloseFile, Free PrivateFileHandle 0x%08x\r\n", pHandle));
    delete pHandle;

    // Restore previously set error code.
    SetLastError(LastError);
    
    return TRUE;
}


extern "C" BOOL
FCFILT_ReadFile (
    PFILE  pfile,
    PBYTE  pBuffer, 
    DWORD  cbRead, 
    PDWORD pcbRead, 
    OVERLAPPED* pOverlapped
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:ReadFile, pHandle=0x%08x Bytes=0x%08x\r\n",
                         pHandle, cbRead));
    // pOverlapped is not supported -- stop passing it or disable this zone to stop the DEBUGCHK
    DEBUGCHK (!pOverlapped || !ZONE_WARNING_BREAK);
    return pHandle->ReadWrite (pBuffer, cbRead, pcbRead, FALSE);
}


extern "C" BOOL
FCFILT_ReadFileScatter (
    PFILE  pfile, 
    FILE_SEGMENT_ELEMENT aSegmentArray[], 
    DWORD  nNumberOfBytesToRead, 
    PDWORD pReserved,
    OVERLAPPED* pOverlapped
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:ReadFileScatter, pHandle=0x%08x Bytes=0x%08x\r\n",
                         pHandle, nNumberOfBytesToRead));
    // pOverlapped is not supported -- stop passing it or disable this zone to stop the DEBUGCHK
    DEBUGCHK (!pOverlapped || !ZONE_WARNING_BREAK);
    return pHandle->ReadWriteScatterGather (aSegmentArray, nNumberOfBytesToRead,
                                            pReserved, FALSE);
}


extern "C" BOOL
FCFILT_ReadFileWithSeek (
    PFILE  pfile, 
    PBYTE  pBuffer, 
    DWORD  cbRead, 
    PDWORD pcbRead, 
    OVERLAPPED* pOverlapped,
    DWORD  dwLowOffset,
    DWORD  dwHighOffset)
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:ReadFileWithSeek, pHandle=0x%08x Offset=0x%08x,%08x Bytes=0x%08x\r\n",
                         pHandle, dwHighOffset, dwLowOffset, cbRead));
    // pOverlapped is not supported -- stop passing it or disable this zone to stop the DEBUGCHK
    DEBUGCHK (!pOverlapped || !ZONE_WARNING_BREAK);
    return pHandle->ReadWriteWithSeek (pBuffer, cbRead, pcbRead,
                                       dwLowOffset, dwHighOffset, FALSE);
}


extern "C" BOOL
FCFILT_WriteFile (
    PFILE  pfile, 
    const BYTE* pBuffer, 
    DWORD  cbWrite, 
    PDWORD pcbWritten, 
    OVERLAPPED* pOverlapped
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:WriteFile, pHandle=0x%08x Bytes=0x%08x\r\n",
                         pHandle, cbWrite));
    // pOverlapped is not supported -- stop passing it or disable this zone to stop the DEBUGCHK
    DEBUGCHK (!pOverlapped || !ZONE_WARNING_BREAK);
    return pHandle->ReadWrite ((PBYTE) pBuffer, cbWrite, pcbWritten, TRUE);
}


extern "C" BOOL
FCFILT_WriteFileGather (
    PFILE  pfile, 
    FILE_SEGMENT_ELEMENT aSegmentArray[], 
    DWORD  nNumberOfBytesToWrite, 
    PDWORD pReserved,
    OVERLAPPED* pOverlapped)
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:WriteFileGather, pHandle=0x%08x Bytes=0x%08x\r\n",
                         pHandle, nNumberOfBytesToWrite));
    // pOverlapped is not supported -- stop passing it or disable this zone to stop the DEBUGCHK
    DEBUGCHK (!pOverlapped || !ZONE_WARNING_BREAK);
    return pHandle->ReadWriteScatterGather (aSegmentArray, nNumberOfBytesToWrite,
                                            pReserved, TRUE);
}


extern "C" BOOL
FCFILT_WriteFileWithSeek (
    PFILE  pfile, 
    const BYTE* pBuffer, 
    DWORD  cbWrite, 
    PDWORD pcbWritten, 
    OVERLAPPED* pOverlapped,
    DWORD  dwLowOffset, 
    DWORD  dwHighOffset
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t*) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:WriteFileWithSeek, pHandle=0x%08x\r\n", pHandle));
    // pOverlapped is not supported -- stop passing it or disable this zone to stop the DEBUGCHK
    DEBUGCHK (!pOverlapped || !ZONE_WARNING_BREAK);
    return pHandle->ReadWriteWithSeek ((PBYTE) pBuffer, cbWrite, pcbWritten,
                                       dwLowOffset, dwHighOffset, TRUE);
}


extern "C" BOOL
FCFILT_DeviceIoControl (
    PFILE  pfile, 
    DWORD  dwIoControlCode,
    PVOID  pInBuf, 
    DWORD  nInBufSize, 
    PVOID  pOutBuf,
    DWORD  nOutBufSize,
    PDWORD pBytesReturned,
    OVERLAPPED* pOverlapped
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t*) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:DeviceIoControl, pHandle=%d ioctl=%u pIn=0x%08x(%u) pOut=0x%08x(%u)\r\n",
                         pHandle, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize));
    // pOverlapped is not supported -- stop passing it or disable this zone to stop the DEBUGCHK
    DEBUGCHK (!pOverlapped || !ZONE_WARNING_BREAK);
    return pHandle->DeviceIoControl (dwIoControlCode, pInBuf, nInBufSize, pOutBuf, 
                                     nOutBufSize, pBytesReturned);
}


extern "C" BOOL
FCFILT_FlushFileBuffers (
    PFILE pfile
    )
{
    BOOL result = TRUE;

    PrivateFileHandle_t *pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:FlushFileBuffers, pHandle=0x%08x\r\n", pHandle));

    // Check that write is allowed and does not conflict with a lock 
    if (!(pHandle->GetAccess() & GENERIC_WRITE)) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:FlushFileBuffers, !! Denying modification to non-writable handle\r\n"));
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }     
    
    result = pHandle->m_pSharedMap->FlushMap (MAP_FLUSH_WRITEBACK);

    result = pHandle->m_pSharedMap->FlushFileBuffers() && result;

    return result;
}


extern "C" BOOL
FCFILT_GetFileInformationByHandle (
    PFILE pfile,
    PBY_HANDLE_FILE_INFORMATION pFileInfo
    )
{
    PrivateFileHandle_t *pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT: GetFileInformationByHandle, pHandle=0x%08x\r\n", pHandle));
    return pHandle->m_pSharedMap->GetFileInformationByHandle (pFileInfo);
}


extern "C" DWORD
FCFILT_GetFileSize (
    PFILE  pfile,
    PDWORD pFileSizeHigh
    )
{
    PrivateFileHandle_t *pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:GetFileSize, pHandle=0x%08x\r\n", pHandle));
    return pHandle->m_pSharedMap->GetFileSize (pFileSizeHigh);
}


extern "C" BOOL
FCFILT_SetEndOfFile (
    PFILE pfile
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:SetEndOfFile, pHandle=0x%08x\r\n", pHandle));
    return pHandle->SetEndOfFile ();
}


extern "C" DWORD
FCFILT_SetFilePointer (
    PFILE pfile, 
    LONG  lDistanceToMove, 
    PLONG pDistanceToMoveHigh, 
    DWORD dwMoveMethod
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:SetFilePointer, pHandle=0x%08x Dist=0x%08x,%08x Method=%u\r\n",
                         pHandle, pDistanceToMoveHigh ? *pDistanceToMoveHigh : 0, lDistanceToMove, dwMoveMethod));
    return pHandle->SetFilePointer (lDistanceToMove, pDistanceToMoveHigh, dwMoveMethod);
}


extern "C" BOOL
FCFILT_GetFileTime (
    PFILE     pfile, 
    FILETIME* pCreation,
    FILETIME* pLastAccess, 
    FILETIME* pLastWrite
    )
{
    PrivateFileHandle_t *pHandle = (PrivateFileHandle_t *)pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:GetFileTime, pHandle=0x%08x\r\n", pHandle));
    return pHandle->m_pSharedMap->GetFileTime (pCreation, pLastAccess, pLastWrite);
}


extern "C" BOOL
FCFILT_SetFileTime (
    PFILE pfile, 
    CONST FILETIME* pCreation, 
    CONST FILETIME* pLastAccess, 
    CONST FILETIME* pLastWrite
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:SetFileTime, pHandle=0x%08x\r\n", pHandle));

    // Check that write is allowed and does not conflict with a lock 
    if (!(pHandle->GetAccess() & GENERIC_WRITE)) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:SetFileTime, !! Denying modification to non-writable handle\r\n"));
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }     
    
    return pHandle->m_pSharedMap->SetFileTime (pCreation, pLastAccess, pLastWrite);
}


extern "C" BOOL
FCFILT_LockFileEx (
    PFILE pfile, 
    DWORD dwFlags, 
    DWORD dwReserved, 
    DWORD nNumberOfBytesToLockLow,
    DWORD nNumberOfBytesToLockHigh, 
    OVERLAPPED* pOverlapped
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:LockFileEx, pHandle=0x%08x Flags=0x%08x Bytes=0x%08x,%08x\r\n",
                         pHandle, dwFlags, nNumberOfBytesToLockHigh, nNumberOfBytesToLockLow));
    return pHandle->LockFileEx (dwFlags, dwReserved, nNumberOfBytesToLockLow, 
                                nNumberOfBytesToLockHigh, pOverlapped);
}


extern "C" BOOL
FCFILT_UnlockFileEx (
    PFILE pfile, 
    DWORD dwReserved, 
    DWORD nNumberOfBytesToLockLow, 
    DWORD nNumberOfBytesToLockHigh,
    OVERLAPPED* pOverlapped
    )
{
    PrivateFileHandle_t* pHandle = (PrivateFileHandle_t *) pfile;
    DEBUGMSG (ZONE_API, (L"CACHEFILT:UnlockFileEx, pHandle=0x%08x Bytes=0x%08x,%08x\r\n",
                         pHandle, nNumberOfBytesToLockHigh, nNumberOfBytesToLockLow));

    return pHandle->UnlockFileEx (dwReserved, nNumberOfBytesToLockLow,
                                  nNumberOfBytesToLockHigh, pOverlapped);
}



//==============================================================================
//
// FIND APIS
//
//==============================================================================

extern "C" BOOL
FCFILT_FindClose (
    PSEARCH pSearch
    )
{
    FindHandle_t* pFind = (FindHandle_t*) pSearch;
    BOOL result = FALSE;

    DEBUGMSG (ZONE_API, (L"CACHEFILT:FindClose, pFind=0x%08x\r\n", pFind));

    if (pFind) {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.FindClose, hVolume=0x%08x\r\n",
                            pFind->pVolume->m_FilterHook.GetVolumeHandle ()));
        result = pFind->pVolume->m_FilterHook.FindClose ((DWORD) pFind->hFindPseudoHandle);
        
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:FindClose, free FindHandle 0x%08x\r\n", pFind));
        pFind->pVolume = NULL;
        pFind->hFindPseudoHandle = INVALID_HANDLE_VALUE;
        delete pFind;
    } else {
        SetLastError (ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:FindClose, !! Failed, error=%u\r\n", GetLastError ()));
    
    return result;
}



extern "C" HANDLE
FCFILT_FindFirstFileW (
    PVOLUME pvol,
    HANDLE  hProcess,
    PCWSTR  pwsFileSpec,
    PWIN32_FIND_DATAW pfd
    )
{
    CachedVolume_t *pVolume = (CachedVolume_t *) pvol;
    DWORD dwError = ERROR_SUCCESS;

    // This will bail immediately if the volume is already hashed or we're not
    // supposed to be hashing it.  It has to be called during create file,
    // because the underlying file system may not be accessible when initializing
    // the filter.
    pVolume->m_pDirectoryHash->HashVolume();

    if (!pVolume->m_pDirectoryHash->FileMightExist( pwsFileSpec, &dwError ) ) {
        SetLastError( dwError );
        return INVALID_HANDLE_VALUE;
    }

    return FindFirstFileInternal( pvol, hProcess, pwsFileSpec, pfd );
}



extern "C" BOOL
FCFILT_FindNextFileW (
    PSEARCH pSearch, 
    PWIN32_FIND_DATAW pfd
    )
{
    FindHandle_t* pFind = (FindHandle_t*) pSearch;
    BOOL result = FALSE;

    DEBUGMSG (ZONE_API, (L"CACHEFILT:FindNextFileW, pFind=0x%08x\r\n", pFind));
    
    if (pFind) {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.FindNextFileW, hVolume=0x%08x\r\n",
                            pFind->pVolume->m_FilterHook.GetVolumeHandle ()));
        result = pFind->pVolume->m_FilterHook.FindNextFileW ((DWORD)pFind->hFindPseudoHandle, pfd); 
    } else {
        SetLastError (ERROR_INVALID_PARAMETER);
    }
    // Treat no-more-files as a warning; others are errors
#if 0
    DEBUGMSG (!result
              && (ZONE_WARNING || (ZONE_ERROR && (ERROR_NO_MORE_FILES != GetLastError ()))),
              (L"CACHEFILT:FindNextFileW, !! Failed, error=%u\r\n", GetLastError ()));
#endif
    return result;
}

extern "C" HANDLE
FindFirstFileInternal (
    PVOLUME pvol, 
    HANDLE  hProcess, 
    PCWSTR  pwsFileSpec, 
    PWIN32_FIND_DATAW pfd
    )
{
    CachedVolume_t *pVolume = (CachedVolume_t *) pvol;
    FindHandle_t* pFind;
    
    DEBUGMSG (ZONE_API, (L"CACHEFILT:FindFirstFileW, pVolume=0x%08x ProcId=0x%08x %s\r\n",
                         pVolume, hProcess, pwsFileSpec));
    
    pFind = new FindHandle_t;
    DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:FindFirstFileW, alloc FindHandle 0x%08x\r\n", pFind));
    if (pFind) {
        pFind->pVolume = pVolume;
        pFind->hFindPseudoHandle = INVALID_HANDLE_VALUE;
        pFind->ProcessId = (DWORD) hProcess;
        
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.FindFirstFileW, hVolume=0x%08x\r\n",
                            pVolume->m_FilterHook.GetVolumeHandle ()));
        pFind->hFindPseudoHandle = pVolume->m_FilterHook.FindFirstFileW (hProcess, pwsFileSpec, pfd);
        
        if ((INVALID_HANDLE_VALUE == pFind->hFindPseudoHandle)
            || (0 == pFind->hFindPseudoHandle)) {
            DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:FindFirstFileW, free FindHandle 0x%08x\r\n", pFind));
            pFind->pVolume = NULL;
            pFind->hFindPseudoHandle = INVALID_HANDLE_VALUE;
            delete pFind;
            pFind = NULL;
        }
    }
    
    if (!pFind) {
        // Treat no-more-files as a warning; others are errors
#if 0
        DEBUGMSG (ZONE_WARNING || (ZONE_ERROR && (ERROR_NO_MORE_FILES != GetLastError ())),
                  (L"CACHEFILT:FindFirstFileW, !! Failed, error=%u\r\n", GetLastError ()));
#endif
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE) pFind;
}


//==============================================================================
//
// MISC WORKERS
//
//==============================================================================

// Will not throw any exceptions
LPWSTR
DuplicateFileName (
    LPCWSTR pFileName
    )
{
    LPWSTR pCopy = NULL;

    __try {
        size_t cchName = 0;
        if (SUCCEEDED (StringCchLength (pFileName, MAX_PATH, &cchName))) {
            pCopy = new WCHAR [(cchName + 1) * sizeof(WCHAR)];
            DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:DuplicateFileName, alloc string 0x%08x\r\n", pCopy));
            if (pCopy) {
                if (SUCCEEDED (StringCchCopy (pCopy, cchName+1, pFileName))) {
                    // Successfully copied
                    return pCopy;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:DuplicateFileName, !! Exception\r\n"));
        SetLastError (ERROR_GEN_FAILURE);
    }
    
    if (pCopy) {
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:DuplicateFileName, free string 0x%08x\r\n", pCopy));
        delete [] pCopy;
    }
    return NULL;
}


//==============================================================================
//
// DLL ENTRY POINT
//
//==============================================================================

extern "C" BOOL WINAPI DllMain (
    HANDLE hInstance,
    DWORD  dwReason,
    LPVOID lpReserved)
{
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            RegisterDbgZones ((HINSTANCE) hInstance, &dpCurSettings);
            DisableThreadLibraryCalls ((HMODULE) hInstance);
            DEBUGMSG (ZONE_INIT, (L"CACHEFILT:DllMain, Attach process ID=0x%08x\r\n",
                                  GetCurrentProcessId ()));
            if (InitHeap ()) {
                InitVolumes ();
            }
            break;

        case DLL_PROCESS_DETACH:
            DEBUGMSG (ZONE_INIT, (L"CACHEFILT:DllMain, Detach process ID=0x%08x\r\n",
                                  GetCurrentProcessId ()));
            CleanupVolumes ();
            CleanupHeap ();
            break;

        default:
            break;
    }

    return TRUE;
}
