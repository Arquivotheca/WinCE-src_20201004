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
#include "storeincludes.hpp"
#include "stgmarshal.hpp"

#ifdef UNDER_CE
const PFNVOID FileAPIMethods[NUM_FILE_APIS] = {
    (PFNVOID)FSDMGR_CloseFile,
    (PFNVOID)0,
    (PFNVOID)FSDMGR_ReadFile,
    (PFNVOID)FSDMGR_WriteFile,
    (PFNVOID)FSDMGR_GetFileSize,
    (PFNVOID)FSDMGR_SetFilePointer,
    (PFNVOID)FSDMGR_GetFileInformationByHandle,
    (PFNVOID)FSDMGR_FlushFileBuffers,
    (PFNVOID)FSDMGR_GetFileTime,
    (PFNVOID)FSDMGR_SetFileTime,
    (PFNVOID)FSDMGR_SetEndOfFile,
    (PFNVOID)FSDMGREXT_DeviceIoControl,
    (PFNVOID)FSDMGR_ReadFileWithSeek,
    (PFNVOID)FSDMGR_WriteFileWithSeek,
    (PFNVOID)FSDMGR_LockFileEx,
    (PFNVOID)FSDMGR_UnlockFileEx,
    (PFNVOID)FSDMGR_CancelIo,
    (PFNVOID)FSDMGR_CancelIoEx,
};

const PFNVOID FileAPIDirectMethods[NUM_FILE_APIS] = {
    (PFNVOID)FSDMGR_CloseFile,
    (PFNVOID)0,
    (PFNVOID)FSDMGR_ReadFile,
    (PFNVOID)FSDMGR_WriteFile,
    (PFNVOID)FSDMGR_GetFileSize,
    (PFNVOID)FSDMGR_SetFilePointer,
    (PFNVOID)FSDMGR_GetFileInformationByHandle,
    (PFNVOID)FSDMGR_FlushFileBuffers,
    (PFNVOID)FSDMGR_GetFileTime,
    (PFNVOID)FSDMGR_SetFileTime,
    (PFNVOID)FSDMGR_SetEndOfFile,
    (PFNVOID)FSDMGRINT_DeviceIoControl,
    (PFNVOID)FSDMGR_ReadFileWithSeek,
    (PFNVOID)FSDMGR_WriteFileWithSeek,
    (PFNVOID)FSDMGR_LockFileEx,
    (PFNVOID)FSDMGR_UnlockFileEx,
    (PFNVOID)FSDMGR_CancelIo,
    (PFNVOID)FSDMGR_CancelIoEx,
};

const ULONGLONG FileAPISigs[NUM_FILE_APIS] = {
    FNSIG1(DW),                                 // CloseFileHandle
    FNSIG0(),
    FNSIG5(DW,O_PTR,DW,O_PDW,IO_PDW),           // ReadFile
    FNSIG5(DW,I_PTR,DW,O_PDW,IO_PDW),           // WriteFile
    FNSIG2(DW,O_PDW),                           // GetFileSize
    FNSIG4(DW,DW,IO_PDW,DW),                    // SetFilePointer
    FNSIG3(DW,O_PTR,DW),                        // GetFileInformationByHandle
    FNSIG1(DW),                                 // FlushFileBuffers
    FNSIG4(DW,O_PI64,O_PI64,O_PI64),            // GetFileTime
    FNSIG4(DW,IO_PI64,IO_PI64,IO_PI64),         // SetFileTime
    FNSIG1(DW),                                 // SetEndOfFile,
    FNSIG8(DW,DW,IO_PTR,DW,IO_PTR,DW,O_PDW,IO_PDW), // DeviceIoControl
    FNSIG7(DW,O_PTR,DW,O_PDW,IO_PDW,DW,DW),     // ReadFileWithSeek
    FNSIG7(DW,I_PTR,DW,O_PDW,IO_PDW,DW,DW),     // WriteFileWithSeek
    FNSIG6(DW,DW,DW,DW,DW,IO_PDW),              // LockFileEx
    FNSIG5(DW,DW,DW,DW,IO_PDW),                 // UnlockFileEx
    FNSIG1(DW),                                 // CancelIo
    FNSIG3(DW,I_PTR,DW),                        // CancelIoEx
};
#endif // UNDER_CE

HANDLE hFileAPI;

LRESULT InitializeFileAPI ()
{
#ifdef UNDER_CE
    // Initialize the handle-based file API.
    hFileAPI = CreateAPISet (const_cast<CHAR*> ("W32H"), NUM_FILE_APIS, FileAPIMethods, FileAPISigs);
    RegisterAPISet (hFileAPI, HT_FILE | REGISTER_APISET_TYPE);
    RegisterDirectMethods (hFileAPI, FileAPIDirectMethods);
#endif
    return ERROR_SUCCESS;
}


EXTERN_C BOOL FSDMGR_CloseFile (FileSystemHandle_t* pHandle)
{
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);
    
    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();

        // Either the handle context is valid, or this is a psuedo-handle.
        DEBUGCHK ((reinterpret_cast<DWORD> (INVALID_HANDLE_VALUE) != HandleContext) ||
                  (FileSystemHandle_t::HDL_PSUEDO & pHandle->GetHandleFlags ()));

        if ((reinterpret_cast<DWORD> (INVALID_HANDLE_VALUE) != HandleContext)) {

            // A file system should never fail CloseFile because there is
            // no way of propagating failure back up to the caller of 
            // CloseHandle at this point.
            pFileSystem->CloseFile (HandleContext);

            // Close the notification handle associated with this file handle.
            pHandle->NotifyCloseHandle ();

        }

        // The FileSystemHandle_t destructor will remove the handle from the 
        // owning volume's handle list.
        delete pHandle;
        
        pVolume->Exit ();
    
    } else {
    
        // The volume cannot be entered, just delete the handle to finalize
        // all cleanup. At this point, the volume object will still be exist
        // because there is at least one (this) handle, though it will be 
        // orphaned and cannot be entered.

        // Wait volume to close all handles first
        pHandle->WaitVolumeClosedThisHandleEvent();
        delete pHandle;
    }

    return TRUE;
}

EXTERN_C BOOL FSDMGR_ReadFile (FileSystemHandle_t* pHandle, void* pBuffer, DWORD BufferSize, 
    DWORD* pBytesRead, OVERLAPPED* pOverlapped)
{
    if (!pHandle->CheckAccess (GENERIC_READ)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    BOOL fRet = FALSE;

    CopyInOutParam<OVERLAPPED> Overlapped(pOverlapped);
    CopyOutParam<DWORD> BytesRead(pBytesRead);

    MountedVolume_t* pVolume;
    BOOL PagesLocked = FALSE;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        if (BufferSize && pBuffer && 
            (FileSystem_t::FSD_FLAGS_LOCK_IO_BUFFERS & pFileSystem->GetFSDFlags()) &&
            !pHandle->IsLockPagesDisabled()) {
            
            // Lock the buffer for write access.
            if (!::LockPages ((void*)pBuffer, BufferSize, NULL, LOCKFLAG_WRITE)) {
                pVolume->Exit();
                return FALSE;
            }
            PagesLocked = TRUE;
        }

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->ReadFile (HandleContext, pBuffer, BufferSize,
            &BytesRead, &Overlapped);

        if (PagesLocked) {
            // Unlock the buffer if it was previously locked.
            VERIFY (::UnlockPages ((void*)pBuffer, BufferSize));
        }        

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_WriteFile (FileSystemHandle_t* pHandle, const void* pBuffer, 
    DWORD BufferSize, DWORD* pBytesWritten, OVERLAPPED* pOverlapped)
{
    if (!pHandle->CheckAccess (GENERIC_WRITE)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    BOOL fRet = FALSE;

    CopyInOutParam<OVERLAPPED> Overlapped(pOverlapped);
    CopyOutParam<DWORD> BytesWritten(pBytesWritten);
    
    MountedVolume_t* pVolume;
    BOOL PagesLocked = FALSE;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {
        
        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        if (BufferSize && pBuffer && 
            (FileSystem_t::FSD_FLAGS_LOCK_IO_BUFFERS & pFileSystem->GetFSDFlags()) &&
            !pHandle->IsLockPagesDisabled()) {
            
            // Lock the buffer for read access.
            if (!::LockPages ((void*)pBuffer, BufferSize, NULL, LOCKFLAG_READ)) {
                pVolume->Exit();
                return FALSE;
            }
            PagesLocked = TRUE;
        }

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->WriteFile (HandleContext, pBuffer, BufferSize,
            &BytesWritten, &Overlapped);

        if (fRet) {
            pHandle->NotifyHandleChange (FILE_NOTIFY_CHANGE_LAST_WRITE);
            pFileSystem->NotifyChangeinFreeSpace();
        }

        if (PagesLocked) {
            // Unlock the buffer if it was previously locked.
            VERIFY (::UnlockPages ((void*)pBuffer, BufferSize));
        }
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C DWORD FSDMGR_GetFileSize (FileSystemHandle_t* pHandle, DWORD* pFileSizeHigh)
{
    DWORD FileSize = INVALID_FILE_SIZE;
    
    CopyOutParam<DWORD> FileSizeHigh(pFileSizeHigh);

    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        FileSize = pFileSystem->GetFileSize (HandleContext, &FileSizeHigh);
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return FileSize;
}

EXTERN_C DWORD FSDMGR_SetFilePointer (FileSystemHandle_t* pHandle, LONG DistanceToMove,
    LONG* pDistanceToMoveHigh, DWORD MoveMethod)
{
    CopyInOutParam<LONG> DistanceToMoveHigh(pDistanceToMoveHigh);

    DWORD FilePos = INVALID_SET_FILE_POINTER;

    if (!pHandle->CheckAccess (GENERIC_READ) && !pHandle->CheckAccess (GENERIC_WRITE)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FilePos;
    }    
    
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        FilePos = pFileSystem->SetFilePointer (HandleContext, DistanceToMove, 
            &DistanceToMoveHigh, MoveMethod);
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return FilePos;
}

EXTERN_C BOOL FSDMGR_GetFileInformationByHandle (FileSystemHandle_t* pHandle,
    BY_HANDLE_FILE_INFORMATION* pFileInfo, DWORD SizeOfFileInfo)
{
#ifdef UNDER_CE
    if (sizeof (BY_HANDLE_FILE_INFORMATION) != SizeOfFileInfo) {
        DEBUGCHK (0); // GetFileInformationByHandle_Trap macro was called directly w/out proper size.
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
#endif // UNDER_CE
    
    CopyOutParam<BY_HANDLE_FILE_INFORMATION> FileInfo(pFileInfo);

    BOOL fRet = FALSE;
    
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->GetFileInformationByHandle (HandleContext, 
            &FileInfo);
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_FlushFileBuffers (FileSystemHandle_t* pHandle)
{
    if (!pHandle->CheckAccess (GENERIC_WRITE)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    BOOL fRet = FALSE;
    
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->FlushFileBuffers (HandleContext);
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_GetFileTime (FileSystemHandle_t* pHandle, FILETIME* pCreation, 
    FILETIME* pLastAccess, FILETIME* pLastWrite)
{
    CopyOutParam<FILETIME> Creation(pCreation);
    CopyOutParam<FILETIME> LastAccess(pLastAccess);
    CopyOutParam<FILETIME> LastWrite(pLastWrite);

    if (!pHandle->CheckAccess (GENERIC_READ)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }    
    
    BOOL fRet = FALSE;
    
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->GetFileTime (HandleContext, &Creation, &LastAccess, 
            &LastWrite);
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_SetFileTime (FileSystemHandle_t* pHandle, const FILETIME* pCreation, 
    const FILETIME* pLastAccess, const FILETIME* pLastWrite)
{
    CopyInParam<FILETIME> Creation(pCreation);
    CopyInParam<FILETIME> LastAccess(pLastAccess);
    CopyInParam<FILETIME> LastWrite(pLastWrite);

    if (!pHandle->CheckAccess (GENERIC_WRITE)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    BOOL fRet = FALSE;
    
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);
    
    if (ERROR_SUCCESS == lResult) {
        
        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->SetFileTime (HandleContext, &Creation, &LastAccess, 
            &LastWrite);
        
        if (fRet) {
            pHandle->NotifyHandleChange (FILE_NOTIFY_CHANGE_LAST_WRITE);
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_SetEndOfFile (FileSystemHandle_t* pHandle)
{
    if (!pHandle->CheckAccess (GENERIC_WRITE)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    BOOL fRet = FALSE;

    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);
    
    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->SetEndOfFile (HandleContext);
        
        if (fRet) {
            pHandle->NotifyHandleChange (FILE_NOTIFY_CHANGE_LAST_WRITE);
            pFileSystem->NotifyChangeinFreeSpace();
        }
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

static BOOL FSDMGR_ReadWriteFileSg (FileSystemHandle_t* pHandle, HANDLE hProc, DWORD dwIoControlCode, FILE_SEGMENT_ELEMENT aSegmentArray[], DWORD nNumberOfBytes, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped)
{
    BOOL fRet = FALSE;
    DWORD HandleContext = pHandle->GetHandleContext ();
    FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();

    if (!aSegmentArray) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (dwIoControlCode == IOCTL_FILE_READ_SCATTER) {

        if (!pHandle->CheckAccess (GENERIC_READ)) {
            // Handle read access is required.
            SetLastError (ERROR_ACCESS_DENIED);
            fRet = FALSE;
        } else {
            fRet = pFileSystem->ReadFileScatter (HandleContext, hProc, aSegmentArray, nNumberOfBytes, lpReserved, lpOverlapped);
        }

    } else {

        if (!pHandle->CheckAccess (GENERIC_WRITE)) {
            // Handle write access is required.
            SetLastError (ERROR_ACCESS_DENIED);
            fRet = FALSE;
        } else {
            fRet = pFileSystem->WriteFileGather (HandleContext, hProc, aSegmentArray, nNumberOfBytes, lpReserved, lpOverlapped);
            if (fRet) {
                pHandle->NotifyHandleChange (FILE_NOTIFY_CHANGE_LAST_WRITE);
                pFileSystem->NotifyChangeinFreeSpace();
            }
        
        }
    }
    return fRet;
}

static BOOL InternalDeviceIoControl (FileSystemHandle_t* pHandle, HANDLE hProc,
    DWORD dwIoControlCode, void* pInBuf, DWORD InBufSize, void* pOutBuf, 
    DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped)
{
    BOOL fRet = FALSE;

    // We do not allow FSCTL_READ_OR_WRITE_SECURITY_DESCRIPTOR to be called
    // externally through this API (it can only be called internally). Apps
    // must use SetFileSecurity and GetFileSecurity APIs to read/write file
    // or directory security descriptors.
    if(FSCTL_READ_OR_WRITE_SECURITY_DESCRIPTOR == dwIoControlCode) {
        DEBUGMSG (ZONE_APIS, (L"FSDMGR!DeviceIoControl: FSCTL_READ_OR_WRITE_SECURITY_DESCRIPTOR cannot be externally invoked; use GetFileSecurity or SetFileSecurity instead"));
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    CopyInOutParam<OVERLAPPED> Overlapped(pOverlapped);
    CopyOutParam<DWORD> BytesReturned(pBytesReturned);
    
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);
    
    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        DWORD HandleFlags = pHandle->GetHandleFlags ();

        fRet = FALSE;
        
        if (FileSystemHandle_t::HDL_PSUEDO & HandleFlags) {

            // This is a psuedo handle opened to the volume (VOL:) so route it to the
            // disk object directly.

            MountableDisk_t* pDisk = pVolume->GetDisk ();
            lResult = pDisk->DiskIoControl (dwIoControlCode, pInBuf, InBufSize, 
                pOutBuf, OutBufSize, &BytesReturned, &Overlapped);
            
            SetLastError (lResult);
            fRet = (ERROR_SUCCESS == lResult);
            
        } else {

            switch (dwIoControlCode) {

            // Handle scatter/gather read/write.
            case IOCTL_FILE_READ_SCATTER:
            case IOCTL_FILE_WRITE_GATHER:
                // NOTE: ReadWriteFileSg verifies proper handle access.
                fRet = FSDMGR_ReadWriteFileSg (pHandle, hProc, dwIoControlCode, 
                        (FILE_SEGMENT_ELEMENT*)pInBuf, InBufSize, 
                        (LPDWORD)pOutBuf, &Overlapped);
                break;

            // Handle volume info on a file handle.
            case IOCTL_FILE_GET_VOLUME_INFO:
                if (pInBuf && pOutBuf &&
                    (InBufSize == sizeof (CE_VOLUME_INFO_LEVEL))) {
            
                    CE_VOLUME_INFO_LEVEL InfoLevel;
            
                    if (CeSafeCopyMemory (&InfoLevel, pInBuf, sizeof(CE_VOLUME_INFO_LEVEL))) {
                        fRet = FSDMGR_GetVolumeInfo (pVolume, InfoLevel, pOutBuf, OutBufSize, &BytesReturned);
                    }            
            
                } else {
                    SetLastError (ERROR_INVALID_PARAMETER);
                }
                break;

            case FSCTL_DISABLE_LOCK_PAGES:
                pHandle->DisableLockPages();

                fRet = pFileSystem->DeviceIoControl (HandleContext, dwIoControlCode,
                    pInBuf, InBufSize, pOutBuf, OutBufSize, &BytesReturned, &Overlapped);
                break;
            
            default:
                fRet = pFileSystem->DeviceIoControl (HandleContext, dwIoControlCode,
                    pInBuf, InBufSize, pOutBuf, OutBufSize, &BytesReturned, &Overlapped);
                break;
            }
        }
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGREXT_DeviceIoControl (FileSystemHandle_t* pHandle, DWORD dwIoControlCode, 
    void* pInBuf, DWORD InBufSize, void* pOutBuf, DWORD OutBufSize, 
    DWORD* pBytesReturned, OVERLAPPED* pOverlapped)
{
    HANDLE hProc = reinterpret_cast<HANDLE> (GetCallerVMProcessId ());
    return InternalDeviceIoControl (pHandle, hProc, dwIoControlCode, pInBuf, InBufSize,
            pOutBuf, OutBufSize, pBytesReturned, pOverlapped);
}

EXTERN_C BOOL FSDMGRINT_DeviceIoControl (FileSystemHandle_t* pHandle, DWORD dwIoControlCode, 
    void* pInBuf, DWORD InBufSize, void* pOutBuf, DWORD OutBufSize, 
    DWORD* pBytesReturned, OVERLAPPED* pOverlapped)
{
    HANDLE hProc = reinterpret_cast<HANDLE> (GetCurrentProcessId ());
    return InternalDeviceIoControl (pHandle, hProc, dwIoControlCode, pInBuf, InBufSize,
            pOutBuf, OutBufSize, pBytesReturned, pOverlapped);
}

EXTERN_C BOOL FSDMGR_ReadFileWithSeek (FileSystemHandle_t* pHandle, void* pBuffer, DWORD BufferSize, 
    DWORD* pBytesRead, OVERLAPPED* pOverlapped, DWORD LowOffset, DWORD HighOffset)
{
    if (!pHandle->CheckAccess (GENERIC_READ)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    BOOL fRet = FALSE;

    CopyInOutParam<OVERLAPPED> Overlapped(pOverlapped);
    CopyOutParam<DWORD> BytesRead(pBytesRead);
    
    MountedVolume_t* pVolume;
    BOOL PagesLocked = FALSE;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {
        
        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        if (BufferSize && pBuffer && 
            (FileSystem_t::FSD_FLAGS_LOCK_IO_BUFFERS & pFileSystem->GetFSDFlags()) &&
            !pHandle->IsLockPagesDisabled()) {
            
            // Lock the buffer for write access.
            if (!::LockPages ((void*)pBuffer, BufferSize, NULL, LOCKFLAG_WRITE)) {
                pVolume->Exit();
                return FALSE;
            }
            PagesLocked = TRUE;
        }


        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->ReadFileWithSeek (HandleContext, pBuffer, BufferSize,
            &BytesRead, &Overlapped, LowOffset, HighOffset);

        if (PagesLocked) {
            // Unlock the buffer if it was previously locked.
            VERIFY (::UnlockPages ((void*)pBuffer, BufferSize));
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_WriteFileWithSeek (FileSystemHandle_t* pHandle, const void* pBuffer, 
    DWORD BufferSize, DWORD* pBytesWritten, OVERLAPPED* pOverlapped, DWORD LowOffset, 
    DWORD HighOffset)
{
    if (!pHandle->CheckAccess (GENERIC_WRITE)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    BOOL fRet = FALSE;

    CopyInOutParam<OVERLAPPED> Overlapped(pOverlapped);
    CopyOutParam<DWORD> BytesWritten(pBytesWritten);
    
    MountedVolume_t* pVolume;
    BOOL PagesLocked = FALSE;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        if (BufferSize && pBuffer && 
            (FileSystem_t::FSD_FLAGS_LOCK_IO_BUFFERS & pFileSystem->GetFSDFlags()) &&
            !pHandle->IsLockPagesDisabled()) {
            
            // Lock the buffer for read access.
            if (!::LockPages ((void*)pBuffer, BufferSize, NULL, LOCKFLAG_READ)) {
                pVolume->Exit();
                return FALSE;
            }
            PagesLocked = TRUE;
        }

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->WriteFileWithSeek (HandleContext, pBuffer, BufferSize,
            &BytesWritten, &Overlapped, LowOffset, HighOffset);

        if (fRet) {
            pHandle->NotifyHandleChange (FILE_NOTIFY_CHANGE_LAST_WRITE);
            pFileSystem->NotifyChangeinFreeSpace();
        }

        if (PagesLocked) {
            // Unlock the buffer if it was previously unlocked.
            VERIFY (::UnlockPages ((void*)pBuffer, BufferSize));
        }
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_LockFileEx (FileSystemHandle_t* pHandle, DWORD Flags, DWORD Reserved, 
    DWORD NumberOfBytesToLockLow, DWORD NumberOfBytesToLockHigh, OVERLAPPED* pOverlapped)
{
    // Caller must have opened the file with read or write access.
    if (!pHandle->CheckAccess (GENERIC_WRITE) &&
        !pHandle->CheckAccess (GENERIC_READ)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    BOOL fRet = FALSE;

    CopyInOutParam<OVERLAPPED> Overlapped(pOverlapped);
    
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->LockFileEx (HandleContext, Flags, Reserved,
            NumberOfBytesToLockLow, NumberOfBytesToLockHigh, &Overlapped);

        pVolume->Exit ();
        
    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_UnlockFileEx (FileSystemHandle_t* pHandle, DWORD Reserved, DWORD NumberOfBytesToUnlockLow, 
    DWORD NumberOfBytesToUnlockHigh, OVERLAPPED* pOverlapped)
{
    // Caller must have opened the file with read or write access.
    if (!pHandle->CheckAccess (GENERIC_WRITE) &&
        !pHandle->CheckAccess (GENERIC_READ)) {
        SetLastError (ERROR_ACCESS_DENIED);
        return FALSE;
    }

    BOOL fRet = FALSE;

    CopyInOutParam<OVERLAPPED> Overlapped(pOverlapped);
    
    MountedVolume_t* pVolume;
    LRESULT lResult = pHandle->EnterWithWait (&pVolume);
    
    if (ERROR_SUCCESS == lResult ) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DEBUGCHK (pFileSystem);

        DWORD HandleContext = pHandle->GetHandleContext ();
        fRet = pFileSystem->UnlockFileEx (HandleContext, Reserved,
            NumberOfBytesToUnlockLow, NumberOfBytesToUnlockHigh, &Overlapped);

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_CancelIoEx (FileSystemHandle_t* /* pHandle */, OVERLAPPED* /* pOverlapped */, DWORD /* SizeOfOverlapped */)
{
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
}

EXTERN_C BOOL FSDMGR_CancelIo (FileSystemHandle_t* /* pHandle */)
{
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
}

