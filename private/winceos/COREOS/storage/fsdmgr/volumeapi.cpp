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

HANDLE hVolumeAPI;

#ifdef UNDER_CE
const PFNVOID AFSAPIInternalMethods[NUM_AFS_APIS] = {
        (PFNVOID)FSDMGR_Close,
        (PFNVOID)FSDMGR_PreClose,
        (PFNVOID)FSDMGR_CreateDirectoryW,
        (PFNVOID)FSDMGR_RemoveDirectoryW,
        (PFNVOID)FSDMGR_GetFileAttributesW,
        (PFNVOID)FSDMGR_SetFileAttributesW,
        (PFNVOID)FSDMGR_CreateFileW,
        (PFNVOID)FSDMGR_DeleteFileW,
        (PFNVOID)FSDMGR_MoveFileW,
        (PFNVOID)FSDMGR_FindFirstFileW,
        (PFNVOID)NULL,
        (PFNVOID)NULL,
        (PFNVOID)FSDMGR_DeleteAndRenameFileW,
        (PFNVOID)FSDMGR_CloseAllFileHandles,
        (PFNVOID)FSDMGR_GetDiskFreeSpaceW,
        (PFNVOID)FSDMGR_NotifyMountedFS,
        (PFNVOID)FSDMGR_RegisterFileSystemFunction,
        (PFNVOID)FSDMGR_FindFirstChangeNotificationW,
        (PFNVOID)NULL,
        (PFNVOID)NULL,
        (PFNVOID)NULL,
        (PFNVOID)FSDMGR_FsIoControlW,
        (PFNVOID)FSDMGR_SetFileSecurityW,
        (PFNVOID)FSDMGR_GetFileSecurityW
};

const PFNVOID AFSAPIExternalMethods[NUM_AFS_APIS] = {
        (PFNVOID)NULL, // FSDMGR_Close,
        (PFNVOID)NULL, // FSDMGR_PreClose,
        (PFNVOID)NULL, // FSDMGR_CreateDirectoryW,
        (PFNVOID)NULL, // FSDMGR_RemoveDirectoryW,
        (PFNVOID)NULL, // FSDMGR_GetFileAttributesW,
        (PFNVOID)NULL, // FSDMGR_SetFileAttributesW,
        (PFNVOID)NULL, // FSDMGR_CreateFileW,
        (PFNVOID)NULL, // FSDMGR_DeleteFileW,
        (PFNVOID)NULL, // FSDMGR_MoveFileW,
        (PFNVOID)NULL, // FSDMGR_FindFirstFileW,
        (PFNVOID)NULL,
        (PFNVOID)NULL,
        (PFNVOID)NULL, // FSDMGR_DeleteAndRenameFileW,
        (PFNVOID)NULL, // FSDMGR_CloseAllFileHandles,
        (PFNVOID)NULL, // FSDMGR_GetDiskFreeSpaceW,
        (PFNVOID)NULL, // FSDMGR_NotifyMountedFS,
        (PFNVOID)NULL, // FSDMGR_RegisterFileSystemFunction,
        (PFNVOID)NULL, // FSDMGR_FindFirstChangeNotificationW,
        (PFNVOID)NULL,
        (PFNVOID)NULL,
        (PFNVOID)NULL,
        (PFNVOID)NULL, // FSDMGR_FsIoControlW,
        (PFNVOID)NULL, // FSDMGR_SetFileSecurityW,
        (PFNVOID)NULL, // FSDMGR_GetFileSecurityW
};

const ULONGLONG AFSAPISigs[NUM_AFS_APIS] = {
        FNSIG1(DW),                                     // CloseVolume
        FNSIG1(DW),                                     // PreCloseVolume
        FNSIG5(DW,I_WSTR,I_WSTR,I_PTR,DW),              // CreateDirectoryW
        FNSIG2(DW,I_WSTR),                              // RemoveDirectoryW
        FNSIG2(DW,I_WSTR),                              // GetFileAttributesW
        FNSIG3(DW,I_WSTR, DW),                          // SetFileAttributesW
        FNSIG11(DW,DW,I_WSTR,DW,DW,I_PDW,DW,DW,DW,I_PTR,DW), // CreateFileW
        FNSIG2(DW,I_WSTR),                              // DeleteFileW
        FNSIG3(DW,I_WSTR,I_WSTR),                       // MoveFileW
        FNSIG5(DW,DW,I_WSTR,IO_PTR,DW),                 // FindFirstFileW
        FNSIG0(),                                       // CeRegisterFileSystemNotification
        FNSIG0(),                                       // CeOidGetInfo
        FNSIG3(DW,I_WSTR,I_WSTR),                       // PrestoChangoFileName
        FNSIG2(DW,DW),                                  // CloseAllFiles
        FNSIG6(DW,I_WSTR,O_PDW,O_PDW,O_PDW,O_PDW),      // GetDiskFreeSpace
        FNSIG2(DW,DW),                                  // Notify
        FNSIG2(DW,I_PDW),                               // CeRegisterFileSystemFunction
        FNSIG5(DW,DW,I_WSTR,DW,DW),                     // FindFirstChangeNotification
        FNSIG0(),                                       // FindNextChangeNotification
        FNSIG0(),                                       // FindCloseChangeNotification
        FNSIG0(),                                       // CeGetChangeNotificationInfo
        FNSIG9(DW,DW,DW,IO_PTR,DW,IO_PTR,DW,O_PDW,IO_PDW), // CeFsIoControl
        FNSIG5(DW,I_WSTR,DW,I_PTR,DW),                  // SetFileSecurityW
        FNSIG6(DW,I_WSTR,DW,O_PTR,DW,O_PDW)             // GetFileSecurityW
};
#endif // UNDER_CE

LRESULT InitializeVolumeAPI ()
{
#ifdef UNDER_CE
    // Initialize the handle-based file API.
    hVolumeAPI = CreateAPISet (const_cast<CHAR*> ("PFSD"), NUM_AFS_APIS, AFSAPIExternalMethods, AFSAPISigs);
    VERIFY (RegisterAPISet (hVolumeAPI, HT_AFSVOLUME | REGISTER_APISET_TYPE));
    VERIFY (RegisterDirectMethods (hVolumeAPI, AFSAPIInternalMethods));
#endif
    return ERROR_SUCCESS;
}

EXTERN_C BOOL FSDMGR_PreClose(MountedVolume_t* pVolume)
{
    // Detach the volume. This will remove the attached reference
    // and the final thread to exit will call Destroy.
    LRESULT lResult = pVolume->Detach ();
    SetLastError (lResult);
    return (ERROR_SUCCESS == lResult);
}

EXTERN_C BOOL FSDMGR_Close(MountedVolume_t* pVolume)
{
    delete pVolume;
    return TRUE;
}

EXTERN_C BOOL FSDMGR_CreateDirectoryW (MountedVolume_t* pVolume, const WCHAR* pPathName,
    SECURITY_ATTRIBUTES* pSecurityAttributes, PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD /* SecurityDescriptorSize */)
{
    BOOL fRet = FALSE;

    SECURITY_ATTRIBUTES SecurityAttributes;

    DEBUGCHK (!pSecurityAttributes);
    if (pSecurityDescriptor) {
        SecurityAttributes.nLength = sizeof (SECURITY_ATTRIBUTES);
        SecurityAttributes.lpSecurityDescriptor = pSecurityDescriptor;
        SecurityAttributes.bInheritHandle = FALSE;
        pSecurityAttributes = &SecurityAttributes;
    }

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();

        fRet = pFileSystem->CreateDirectoryW (pPathName, pSecurityAttributes);

        if (fRet) {
            pVolume->NotifyPathChange (pPathName, TRUE, FILE_ACTION_ADDED);
            pFileSystem->NotifyChangeinFreeSpace();
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_RemoveDirectoryW (MountedVolume_t* pVolume, const WCHAR* pPathName)
{
    BOOL fRet = FALSE;

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();
        
        fRet = pFileSystem->RemoveDirectoryW (pPathName);
        
        if (fRet) {
            pVolume->NotifyPathChange (pPathName, TRUE, FILE_ACTION_REMOVED);
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }
    
    return fRet;
}

EXTERN_C DWORD FSDMGR_GetFileAttributesW (MountedVolume_t* pVolume, const WCHAR* pPathName)
{
    DWORD Attributes = INVALID_FILE_ATTRIBUTES;

    // If this is the root of the volume, then return the directory attribute.  Under CE,
    // FS_GetFileAttributes will perform this check, however, this can also be called
    // directly under the desktop.
    if (((pPathName[0] == L'\\' || pPathName[0] == L'/') && (pPathName[1] == L'\0')) ||
        (pPathName[0] == L'\0')) {
        return FILE_ATTRIBUTE_DIRECTORY;
    }

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {
        
        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();
        
        Attributes = pFileSystem->GetFileAttributesW (pPathName);

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return Attributes;
}

EXTERN_C BOOL FSDMGR_SetFileAttributesW (MountedVolume_t* pVolume, const WCHAR* pPathName, 
    DWORD Attributes)
{
    BOOL fRet = FALSE;

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {
        
        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();

        fRet = pFileSystem->SetFileAttributes (pPathName, Attributes);
        
        if (fRet) {
            pVolume->NotifyPathChange (pPathName, FALSE, FILE_ACTION_MODIFIED);
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }
    
    return fRet;
}

EXTERN_C BOOL FSDMGR_DeleteFileW (MountedVolume_t* pVolume, const WCHAR* pFileName)
{
    BOOL fRet = FALSE;

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {
        
        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();
        
        fRet =  pFileSystem->DeleteFileW (pFileName);

        if (fRet) {
            pVolume->NotifyPathChange (pFileName, FALSE, FILE_ACTION_REMOVED);
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }
    
    return fRet;
}

EXTERN_C BOOL FSDMGR_MoveFileW (MountedVolume_t* pVolume, const WCHAR* pOldFileName, 
    const WCHAR* pNewFileName)
{
    BOOL fRet = FALSE;

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();        
        DWORD Attributes = pFileSystem->GetFileAttributesW (pOldFileName);

        fRet =  pFileSystem->MoveFile (pOldFileName, pNewFileName);

        if (fRet) {

            // Determine if it was a directory or 
            BOOL fDirectory = FALSE;
            if ((INVALID_FILE_ATTRIBUTES != Attributes) &&
                (FILE_ATTRIBUTE_DIRECTORY & Attributes)) {
                fDirectory = TRUE;
            }
            
            pVolume->NotifyMoveFileEx (pOldFileName, pNewFileName, fDirectory);
            pFileSystem->NotifyChangeinFreeSpace();
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }
    
    return fRet;
}

EXTERN_C BOOL FSDMGR_DeleteAndRenameFileW (MountedVolume_t* pVolume, const WCHAR* pDestFileName, 
    const WCHAR* pSourceFileName)
{
    // NOTE: The volume must already be entered!
    BOOL fRet = FALSE;

    LRESULT lResult = pVolume->EnterWithWait (); 

    if (ERROR_SUCCESS == lResult) {
    
        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();
        
        // Call DeleteAndRenameFile on the file system.
        fRet =  pFileSystem->DeleteAndRenameFileW (pDestFileName, pSourceFileName);

        if (fRet) {
            
            // DeleteAndRenameFile is essentially an atomic DeleteFile+MoveFile operation,
            // so it should generate two notifications (delete, move).
            pVolume->NotifyPathChange (pDestFileName, FALSE, FILE_ACTION_REMOVED);
            pVolume->NotifyMoveFile (pSourceFileName, pDestFileName);
        }
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_CloseAllFileHandles (MountedVolume_t* /* pVolume */, HANDLE /* hProcess */)
{
    // File handles are all closed by the kernel on process exit.
    return TRUE;
}   

EXTERN_C BOOL FSDMGR_GetDiskFreeSpaceW (MountedVolume_t* pVolume, const WCHAR* pPathName, 
    DWORD* pSectorsPerCluster, DWORD* pBytesPerSector, DWORD* pFreeClusters, DWORD* pClusters)
{
    BOOL fRet = FALSE;

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {
    
        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();
        
        fRet = pFileSystem->GetDiskFreeSpaceW (pPathName, pSectorsPerCluster, pBytesPerSector,
            pFreeClusters, pClusters);
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_RegisterFileSystemFunction (MountedVolume_t* pVolume, 
    SHELLFILECHANGEFUNC_t pFn)
{
    BOOL fRet = FALSE;

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();
        
        fRet = pFileSystem->RegisterFileSystemFunction (pFn);
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C HANDLE FSDMGR_FindFirstFileW (MountedVolume_t* pVolume, HANDLE hProcess, 
    const WCHAR* pPathName, WIN32_FIND_DATAW* pFindData, DWORD SizeOfFindData)
{       
#ifdef UNDER_CE
    if (sizeof (WIN32_FIND_DATAW) != SizeOfFindData) {
        DEBUGCHK (0); // AFS_FindFirstFileW_Trap macro was called directly w/out proper size.
        SetLastError (ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
#endif    
    
    HANDLE h = INVALID_HANDLE_VALUE;    

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {

        // Get the topmost file system object associated with the volume.
        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();

        //
        // Perform the FindFirstFile call on the file system.
        //
        HANDLE hInt = pFileSystem->FindFirstFileW (hProcess, pPathName, pFindData);

        if (INVALID_HANDLE_VALUE != hInt) {
            
            // The file system succeeded creating/opening the file, so 
            // allocate an FileSystemHandle_t object and associate it with the 
            // MountedVolume_t object.
            h = pVolume->AllocSearchHandle ((DWORD)hInt, pFileSystem);

            if (INVALID_HANDLE_VALUE == h) {
                // Failed to allocate a handle object, so close the handle
                // that was returned by the file system.
                pFileSystem->FindClose (reinterpret_cast<DWORD> (hInt));
            }
        }
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return h;
}

EXTERN_C HANDLE FSDMGR_CreateFileW (MountedVolume_t* pVolume, HANDLE hProcess, 
    const WCHAR* pPathName, DWORD Access, DWORD ShareMode, 
    SECURITY_ATTRIBUTES* pSecurityAttributes, DWORD Create, 
    DWORD FlagsAndAttributes, HANDLE hTemplate,
    PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD /* SecurityDescriptorSize */)
{    
    HANDLE h = INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES SecurityAttributes;

    DEBUGCHK (!pSecurityAttributes);
    if (pSecurityDescriptor) {
        SecurityAttributes.nLength = sizeof (SECURITY_ATTRIBUTES);
        SecurityAttributes.lpSecurityDescriptor = pSecurityDescriptor;
        SecurityAttributes.bInheritHandle = FALSE;
        pSecurityAttributes = &SecurityAttributes;
    }

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {

        // Get the topmost file system object associated with the volume.
        // If another filter is hooked between now and the time we call
        // create file, we'll still use this one. This file system object
        // will be referenced by the handle object.
        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();
       
        HANDLE hInt = pFileSystem->CreateFileW (hProcess, pPathName, Access, 
                ShareMode, pSecurityAttributes, Create, 
                FlagsAndAttributes, hTemplate);
        
        if (INVALID_HANDLE_VALUE != hInt) {
            
            // The file system succeeded creating/opening the file, so 
            // allocate an FileSystemHandle_t object and associate it with the 
            // MountedVolume_t object.

            // Indicate whether or not this is a console or psuedo-device
            // handle when creating the FileSystemHandle_t object. This will dictate
            // how notifications are performed on the handle.
            DWORD HandleType = FileSystemHandle_t::HDL_FILE;
            if (IsConsoleName (pPathName)) {
                HandleType |= FileSystemHandle_t::HDL_CONSOLE;
            }

            // Allocte a new handle object to track this item.
            h = pVolume->AllocFileHandle (reinterpret_cast<DWORD> (hInt), 
                HandleType, pFileSystem, pPathName, Access);
        
            if (INVALID_HANDLE_VALUE != h) {

                // Perform notifications only when the handle is for a real file,
                // not a pseudo device handle or console handle. Devices and streams 
                // shouldn't ever generate notifications.
                if (FileSystemHandle_t::HDL_FILE == HandleType &&
                    !IsPsuedoDeviceName (pPathName)) {
                    
                    // Successfully opened the file. Perform file notifications
                    // as required.
                    if (ERROR_ALREADY_EXISTS != GetLastError()) {
                        
                        // The file did not previously exist; notify that it has
                        // been added.
                        if (OPEN_EXISTING != Create) {
                            pVolume->NotifyPathChange (pPathName, FALSE, FILE_ACTION_ADDED);
                            pFileSystem->NotifyChangeinFreeSpace();
                        }

                    } else if ((CREATE_ALWAYS == Create) || 
                            (TRUNCATE_EXISTING == Create)) {
                            
                        // The file previously existed but is being re-created or 
                        // truncated so notify that it has changed.
                        pVolume->NotifyPathChange (pPathName, FALSE, FILE_ACTION_MODIFIED);
                    }
                }
            
            } else {
            
                // Failed to allocate a handle object, so close the handle
                // that was returned by the file system.
                pFileSystem->CloseFile (reinterpret_cast<DWORD> (hInt));
            }

        } else if (pPathName && (_wcsicmp (pPathName, L"\\VOL:") == 0)) {

            // Clear error code set by the FSD when it failed to open VOL: on its own.
            SetLastError (ERROR_SUCCESS);

            // We failed to open the file, but if it is VOL: special case this
            // and allow direct partition access.
            h = pVolume->AllocFileHandle (reinterpret_cast<DWORD> (hInt), 
                FileSystemHandle_t::HDL_PSUEDO, pFileSystem, pPathName, Access);
        }
        
        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }
    
    return h;
}

EXTERN_C HANDLE FSDMGR_FindFirstChangeNotificationW (MountedVolume_t* pVolume, HANDLE hProc, 
    const WCHAR* pPathName, BOOL fWatchSubtree, DWORD NotifyFilter)
{
    HANDLE h = INVALID_HANDLE_VALUE;

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {
    
        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();

        DWORD Attributes = pFileSystem->GetFileAttributesW (pPathName);

        if ((wcscmp (pPathName, L"\\") == 0) || (wcscmp (pPathName, L"") == 0) ||
            ((INVALID_FILE_ATTRIBUTES != Attributes)
                && (FILE_ATTRIBUTE_DIRECTORY & Attributes))) {
            
            h = pVolume->NotifyCreateEvent (hProc, pPathName, fWatchSubtree, NotifyFilter);
            
        } else {
        
            SetLastError (ERROR_PATH_NOT_FOUND);
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }
    
    return h;
}

BOOL FSDMGR_GetVolumeInfo (MountedVolume_t* pVolume, CE_VOLUME_INFO_LEVEL InfoLevel, 
    void* pInfo, DWORD InfoBytes, DWORD* pBytesReturned)
{
    // NOTENOTE: The volume should be already entered (this API should only be
    // called internally.

    if (CeVolumeInfoLevelStandard == InfoLevel) {

        if (sizeof(CE_VOLUME_INFO) != InfoBytes) {
            SetLastError (ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        // Retrieve standard volume info.

        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();

        // Get FSD specific volume information. This includes
        // file system block size, fs name, etc.
        FSD_VOLUME_INFO FsdInfo;
        memset (&FsdInfo, 0, sizeof (FSD_VOLUME_INFO));
        FsdInfo.cbSize = sizeof (FSD_VOLUME_INFO);
    #ifdef UNDER_CE    
        FsdInfo.dwBlockSize = UserKInfo[KINX_PAGESIZE];
    #else
        FsdInfo.dwBlockSize = 4096;
    #endif
        pFileSystem->GetVolumeInfo (&FsdInfo);

        MountableDisk_t* pDisk = pVolume->GetDisk ();

        // Retrieve disk-specific volume information stuch as the
        // partition and store names, if any.
        CE_VOLUME_INFO VolumeInfo = {0};
        VolumeInfo.cbSize = sizeof (CE_VOLUME_INFO);
        pDisk->GetVolumeInfo (&VolumeInfo);
       
        // Add the flags and attributes that were reported by the FSD
        // to the volume info structure.
        VolumeInfo.dwAttributes |= FsdInfo.dwAttributes;
        VolumeInfo.dwFlags |= FsdInfo.dwFlags;

        // Return the blocksize that was reported by the FSD.
        VolumeInfo.dwBlockSize = FsdInfo.dwBlockSize;

        // NOTE: The dwFSVersion, szFSDDesc, and szFSDSubType members
        // of the FSD_VOLUME_INFO structure are not reported via the
        // standard CE_VOLUME_INFO structure and are ignored.

        // Add appropriate attributes according to the mount flags.
        DWORD MountFlags = pVolume->GetMountFlags ();

        if (AFS_FLAG_HIDDEN & MountFlags) {
            VolumeInfo.dwAttributes |= CE_VOLUME_ATTRIBUTE_HIDDEN;
        }
        
        if (AFS_FLAG_BOOTABLE & MountFlags) {
            VolumeInfo.dwAttributes |= CE_VOLUME_ATTRIBUTE_BOOT;
        }
        
        if (AFS_FLAG_SYSTEM & MountFlags) {
            VolumeInfo.dwAttributes |= CE_VOLUME_ATTRIBUTE_SYSTEM;
        }

        if (AFS_FLAG_NETWORK & MountFlags) {
            // NOTE: The UNC handler is assumed to be the network
            // redirector file system.
            VolumeInfo.dwAttributes |= CE_VOLUME_FLAG_NETWORK;
        }

        if (pBytesReturned) {
            DWORD BytesReturned = sizeof (CE_VOLUME_INFO);
            CeSafeCopyMemory (pBytesReturned, &BytesReturned, sizeof (DWORD));
        }

        // Copy our volume info structure to the output structure.
        if (!CeSafeCopyMemory (pInfo, &VolumeInfo, sizeof(CE_VOLUME_INFO))) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        return TRUE;
    
    } else if (CeVolumeInfoLevelSecurity == InfoLevel) {

        if (sizeof(CE_VOLUME_SECURITY_INFO) != InfoBytes) {
            SetLastError (ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        // Retreive the volume security class name.
        CE_VOLUME_SECURITY_INFO VolumeSecurityInfo = {0};
        VolumeSecurityInfo.cbSize = sizeof(CE_VOLUME_SECURITY_INFO);

        STOREINFO StoreInfo = {0};
        StoreInfo.cbSize = sizeof(STOREINFO);

        MountableDisk_t* pDisk = pVolume->GetDisk();

        LRESULT lResult = pDisk->GetSecurityClassName(VolumeSecurityInfo.szSecurityClassName,
                                    SECURITYCLASSNAMESIZE);

        if (NO_ERROR != lResult) {
            SetLastError (lResult);
            return FALSE;
        }

        if (!CeSafeCopyMemory (pInfo, &VolumeSecurityInfo, sizeof(CE_VOLUME_SECURITY_INFO))) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }      

        return TRUE;

    } else {

        // Unexpected value for InfoLevel.
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }    

}

EXTERN_C BOOL FSDMGR_FsIoControlW (MountedVolume_t* pVolume, HANDLE hProc, 
    DWORD Fsctl, void* pInBuf, DWORD InBufSize, void* pOutBuf, DWORD OutBufSize, 
    DWORD* pBytesReturned, OVERLAPPED* pOverlapped)
{    
    BOOL fRet = FALSE;

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {
    
        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();
        
        switch (Fsctl) {

            case FSCTL_GET_VOLUME_INFO:
                if (pInBuf && pOutBuf &&
                    (InBufSize == sizeof(CE_VOLUME_INFO_LEVEL))) {
            
                    CE_VOLUME_INFO_LEVEL InfoLevel;
            
                    if (CeSafeCopyMemory(&InfoLevel, pInBuf, sizeof(CE_VOLUME_INFO_LEVEL))) {
                        fRet = FSDMGR_GetVolumeInfo (pVolume, InfoLevel, pOutBuf, OutBufSize, pBytesReturned);
                    }
                                
                } else {
                    SetLastError(ERROR_INVALID_PARAMETER);
                }
                break;

            default:
                fRet = pFileSystem->FsIoControl (hProc, Fsctl, pInBuf, InBufSize, pOutBuf,
                    OutBufSize, pBytesReturned, pOverlapped);
                break;
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }
    
    return fRet;
}

EXTERN_C void FSDMGR_NotifyMountedFS (MountedVolume_t *pVolume, DWORD Flags)
{
    // NOTE: We don't enter the volume here because we're in the
    // power handler routine. At this point the volume table is
    // locked (volumes cannot be added or destroyed), so this is
    // perfectly safe.

    FileSystem_t* pFileSystem = pVolume->GetFileSystem ();
    DWORD FsdFlags = pFileSystem->GetFSDFlags ();

    if (FileSystem_t::FSD_FLAGS_POWER_NOTIFY & FsdFlags) {

        if (FSNOTIFY_POWER_OFF & Flags) {
            // Power down the volume.
            pVolume->PowerDown ();
        }

        // The FSD is the last to know about a power down and the first
        // to know about a power up.
        pFileSystem->Notify (Flags);

        if (FSNOTIFY_POWER_ON & Flags) {
            // Power up the volume.
            pVolume->PowerUp ();
        }
    }
}

EXTERN_C BOOL FSDMGR_GetFileSecurityW (MountedVolume_t* pVolume,
    const WCHAR* pPathName, SECURITY_INFORMATION RequestedInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD Length, DWORD* pLengthNeeded)
{
    BOOL fRet = FALSE;    

    UNREFERENCED_PARAMETER(RequestedInformation);

#ifdef UNDER_CE
    // This should only ever be called internally, so this parameter
    // should already have been validated.
    DEBUGCHK (CE_SECURITY_INFORMATION == RequestedInformation);
#endif // UNDER_CE

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();

        // Open an internal handle to the file for query access with full
        // sharing.
        HPROCESS hProcess = reinterpret_cast<HPROCESS> (GetCurrentProcessId ());
        HANDLE hInt = pFileSystem->CreateFileW (hProcess, pPathName, 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);

        if (hInt && (INVALID_HANDLE_VALUE != hInt)) {

            // Issue an internal FSCTL to read the security descriptor.
            fRet = pFileSystem->DeviceIoControl (reinterpret_cast<DWORD> (hInt),
                FSCTL_READ_OR_WRITE_SECURITY_DESCRIPTOR, NULL, 0,
                pSecurityDescriptor, Length, pLengthNeeded, NULL);

            VERIFY (pFileSystem->CloseFile (reinterpret_cast<DWORD> (hInt)));

            if (!fRet && (ERROR_INSUFFICIENT_BUFFER == GetLastError ()) &&
                (NULL == pSecurityDescriptor) && (0 == Length)) {
                // The caller was just requesting the buffer size, so succeed.
                SetLastError (ERROR_SUCCESS);
                fRet = TRUE;
            }

        } else {

            // Unable to open the file, maintain the error code set by the
            // file system.
            fRet = FALSE;
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}

EXTERN_C BOOL FSDMGR_SetFileSecurityW (MountedVolume_t* pVolume,
    const WCHAR* pPathName, SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD Length)
{
    BOOL fRet = FALSE;

    UNREFERENCED_PARAMETER(SecurityInformation);

#ifdef UNDER_CE
    // This should only ever be called internally, so this parameter
    // should already have been validated.
    DEBUGCHK (CE_SECURITY_INFORMATION == SecurityInformation);
#endif // UNDER_CE

    LRESULT lResult = pVolume->EnterWithWait ();

    if (ERROR_SUCCESS == lResult) {

        FileSystem_t* pFileSystem = pVolume->GetFileSystem ();

        // Open an internal handle to the file for query access with full
        // sharing.
        HPROCESS hProcess = reinterpret_cast<HPROCESS> (GetCurrentProcessId ());
        HANDLE hInt = pFileSystem->CreateFileW (hProcess, pPathName, 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);

        if (hInt && (INVALID_HANDLE_VALUE != hInt)) {

            // Issue an internal FSCTL to write the security descriptor.
            fRet = pFileSystem->DeviceIoControl (reinterpret_cast<DWORD> (hInt),
                FSCTL_READ_OR_WRITE_SECURITY_DESCRIPTOR,
                pSecurityDescriptor, Length, NULL, 0, NULL, NULL);

            VERIFY (pFileSystem->CloseFile (reinterpret_cast<DWORD> (hInt)));

        } else {

            // Unable to open the file, maintain the error code set by the
            // file system.
            fRet = FALSE;
        }

        pVolume->Exit ();

    } else {

        SetLastError (lResult);
    }

    return fRet;
}
