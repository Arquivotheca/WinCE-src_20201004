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
#include "canonicalizer.hpp"

// Mapping table of generic rights to file-specific rights.
const GENERIC_MAPPING c_FileAccessMapping =
{
    FILE_GENERIC_READ,      // GENERIC_READ
    FILE_GENERIC_WRITE,     // GENERIC_WRITE
    FILE_GENERIC_EXECUTE,   // GENERIC_EXECUTE
    FILE_ALL_ACCESS         // GENERIC_ALL
};

static inline BOOL FileExistsOnFS (HANDLE hVolume, const WCHAR* pName)
{
    DWORD Attributes = INVALID_FILE_ATTRIBUTES;
    __try {
        Attributes = AFS_GetFileAttributesW (hVolume, pName);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return (INVALID_FILE_ATTRIBUTES != Attributes);
}

static inline BOOL CheckROMShadowPermission(const WCHAR* /* pPathName */)
{
    return TRUE;
}

EXTERN_C BOOL FSEXT_CreateDirectoryW (const WCHAR* pPathName,
    SECURITY_ATTRIBUTES* /* pSecurityAttributes */)
{
    return FSINT_CreateDirectoryW (pPathName, NULL);
}

EXTERN_C BOOL FSINT_CreateDirectoryW (const WCHAR* pPathName,
    SECURITY_ATTRIBUTES* /* pSecurityAttributes */)
{
    // Copy and canonicalize the path before processing the path name.
    size_t cchCanonicalName = 0;
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName, &cchCanonicalName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    BOOL fRet = FALSE;
    DWORD MountFlags;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume, &MountFlags);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if (IsRootPathName (pSubPathName)) {

        // Disallow creating a directory with the same name as a mount point.
        SetLastError (ERROR_ALREADY_EXISTS);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        IsPathInRootDir (pCanonicalName) &&
        g_pMountTable->IsReservedName (pCanonicalName, cchCanonicalName)) {

        // This is a reserved name off of the root. Don't let a directory of
        // the same name be created.
        SetLastError (ERROR_ALREADY_EXISTS);

    } else {

        SetLastError (ERROR_SUCCESS);
        __try {
            fRet = AFS_CreateDirectoryW (hVolume, pSubPathName, NULL, NULL, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

EXTERN_C BOOL FS_RemoveDirectoryW (const WCHAR* pPathName)
{
    // Copy and canonicalize the path before processing the path name.
    size_t cchCanonicalName = 0;
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName, &cchCanonicalName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    BOOL fRet = FALSE;
    DWORD MountFlags;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume, &MountFlags);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if (IsRootPathName (pSubPathName)) {

        // Disallow removing a mount point.
        SetLastError (ERROR_ACCESS_DENIED);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        IsPathInRootDir (pCanonicalName) &&
        g_pMountTable->IsReservedName (pCanonicalName, cchCanonicalName)) {

        // This is a reserved name off of the root; it cannot be removed.
        SetLastError (ERROR_ACCESS_DENIED);

    } else {

        SetLastError (ERROR_SUCCESS);
        __try {
            fRet = AFS_RemoveDirectoryW (hVolume, pSubPathName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

EXTERN_C DWORD FS_GetFileAttributesW (const WCHAR* pPathName)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName);
    if (!pCanonicalName) {
        return INVALID_FILE_ATTRIBUTES;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    DWORD MountFlags;
    DWORD Attributes = INVALID_FILE_ATTRIBUTES;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume, &MountFlags);

    if (ERROR_SUCCESS == lResult) {

        if (IsRootPathName (pSubPathName)) {

            // Hard-code attributes for mount points.
            Attributes = FILE_ATTRIBUTE_DIRECTORY;

            if (AFS_FLAG_HIDDEN & MountFlags) {
                // This volume is hidden, so add the hidden file attribute.
                Attributes |= FILE_ATTRIBUTE_HIDDEN;
            }
            if (!(AFS_FLAG_PERMANENT & MountFlags)) {
                // Non-permanent volumes have the FILE_ATTRIBUTE_DIRECTORY attribute
                // before the AFS_FLAG_PERMANENT flag was added, ALL mounted volumes
                // had the FILE_ATTRIBUTE_DIRECTORY attribute
                Attributes |= FILE_ATTRIBUTE_TEMPORARY;
            }
            if (AFS_FLAG_SYSTEM & MountFlags) {
                // Mounted system volumes have the system attribute.
                Attributes |= FILE_ATTRIBUTE_SYSTEM;
            }

        } else {

            SetLastError (ERROR_SUCCESS);
            __try {
                Attributes = AFS_GetFileAttributesW (hVolume, pSubPathName);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }

            if (INVALID_FILE_ATTRIBUTES != Attributes) {

                if (!(AFS_FLAG_MOUNTROM & MountFlags)) {
                    // Only ROM file systems are allowed to return ROM file
                    // attributes.
                    Attributes &= ~(FILE_ATTRIBUTE_INROM);
                }

                if (AFS_FLAG_SYSTEM & MountFlags) {
                    // All files and directories on SYSTEM volumes have the
                    // SYSTEM file attribute.
                    Attributes |= FILE_ATTRIBUTE_SYSTEM;
                }
            }
        }

    } else {

        // Failed to get the volume associated with the path.
        SetLastError (lResult);
    }

    // Handle the ROM case when in the merged system directory:
    if ((INVALID_FILE_ATTRIBUTES == Attributes) &&
        IsPathInSystemDir (pCanonicalName)) {

        // Try to get the attributes of the file in ROM.

        DWORD LastError = FsdGetLastError (ERROR_FILE_NOT_FOUND);

        Attributes = ROM_GetFileAttributesW (pCanonicalName);

        // If we didn't find the file in ROM, restore the previously set error code.
        if (INVALID_FILE_ATTRIBUTES == Attributes) {
            SetLastError (LastError);
        }
    }

    SafeFreeCanonicalPath (pCanonicalName);
    return Attributes;
}

EXTERN_C BOOL FS_SetFileAttributesW (const WCHAR* pPathName, DWORD NewAttributes)
{
    // Copy and canonicalize the path before processing the path name.
    size_t cchCanonicalName = 0;
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName, &cchCanonicalName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    BOOL fRet = FALSE;
    DWORD MountFlags;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume, &MountFlags);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if (IsRootPathName (pSubPathName)) {

        // The attributes of a mount point or the root directory cannot be
        // changed.
        SetLastError (ERROR_ACCESS_DENIED);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        !FileExistsOnFS (hVolume, pSubPathName) &&
        FileExistsInROM (pCanonicalName)) {

        // Cannot change the attributes of a file that exists only in ROM.
        SetLastError (ERROR_ACCESS_DENIED);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        IsPathInRootDir (pCanonicalName) &&
        g_pMountTable->IsReservedName (pCanonicalName, cchCanonicalName)) {

        // Cannot change the attributes of a reserved root.
        SetLastError (ERROR_ACCESS_DENIED);

    } else {

        // Explicitly disallow setting ROM attributes on any file.
        NewAttributes &= ~(FILE_ATTRIBUTE_INROM | FILE_ATTRIBUTE_ROMMODULE);

        // Check the attributes prior to performing access check because
        // we need to know if it is a file or directory.
        DWORD ExistingAttributes = INVALID_FILE_ATTRIBUTES;

        __try {
            ExistingAttributes = AFS_GetFileAttributesW (hVolume, pSubPathName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }

        SetLastError (ERROR_SUCCESS);
        __try {
            fRet = AFS_SetFileAttributesW (hVolume, pSubPathName, NewAttributes);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

EXTERN_C BOOL FS_DeleteFileW (const WCHAR* pFileName)
{
    // Copy and canonicalize the path before processing the path name.
    size_t cchCanonicalName = 0;
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pFileName, &cchCanonicalName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    BOOL fRet = FALSE;
    DWORD MountFlags;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume, &MountFlags);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if (IsRootPathName (pSubPathName)) {

        // Cannot delete a mount point.
        SetLastError (ERROR_ACCESS_DENIED);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        IsPathInRootDir (pCanonicalName) &&
        g_pMountTable->IsReservedName (pCanonicalName, cchCanonicalName)) {

        // Cannot delete a reserved name.
        SetLastError (ERROR_FILE_NOT_FOUND);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        !FileExistsOnFS (hVolume, pSubPathName) &&
        FileExistsInROM (pCanonicalName)) {

        // Cannot delete a file that exists only in ROM.
        SetLastError (ERROR_ACCESS_DENIED);

    } else {

        SetLastError (ERROR_SUCCESS);
        __try {
            fRet = AFS_DeleteFileW (hVolume, pSubPathName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

EXTERN_C BOOL FS_MoveFileW (const WCHAR* pSourceFileName, const WCHAR* pDestFileName)
{
    // Copy and canonicalize the paths before processing the path name.
    size_t cchCanonicalSourceName = 0;
    WCHAR* pCanonicalSourceName = SafeGetCanonicalPathW (pSourceFileName, &cchCanonicalSourceName);
    if (!pCanonicalSourceName) {
        return FALSE;
    }

    size_t cchCanonicalDestName = 0;
    WCHAR* pCanonicalDestName = SafeGetCanonicalPathW (pDestFileName, &cchCanonicalDestName);
    if (!pCanonicalDestName) {
        SafeFreeCanonicalPath (pCanonicalSourceName);
        return FALSE;
    }

    HANDLE hSourceVolume;
    HANDLE hDestVolume;
    const WCHAR* pSubSourceName;
    const WCHAR* pSubDestName;
    DWORD MountFlags;
    BOOL fRet = FALSE;

    // Determine which volume owns each of the provided paths.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalSourceName,
        &pSubSourceName, &hSourceVolume, &MountFlags);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    // Only retrieve the security object from the destination volume. We
    // only perform access check if the volumes are the same, so we don't
    // need to retrieve the security object for both.
    lResult = g_pMountTable->GetVolumeFromPath (pCanonicalDestName,
        &pSubDestName, &hDestVolume);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if (IsRootPathName (pSubSourceName)) {

        // Disallow renaming a mount point.
        SetLastError (ERROR_ACCESS_DENIED);

    } else if (IsRootPathName (pSubDestName)) {

        // Disallow renaming over a mount point's name.
        SetLastError (ERROR_ALREADY_EXISTS);

    } else if (hSourceVolume == hDestVolume) {

        // Both the old path and the new path reside on the same file system
        // volume, so simply issue a file system MoveFile call. At this point
        // we actually have two references to the volume, but it doesn't hurt
        // anything.

        if ((AFS_FLAG_ROOTFS & MountFlags) &&
             !FileExistsOnFS (hSourceVolume, pSubSourceName) &&
             FileExistsInROM (pCanonicalSourceName)) {

             // Cannot move a file that only exists in ROM.
             SetLastError(ERROR_ACCESS_DENIED);

        } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
            IsPathInRootDir (pCanonicalSourceName) &&
            g_pMountTable->IsReservedName (pCanonicalSourceName, cchCanonicalSourceName)) {

            // Disallow renaming a reserved name.
            SetLastError (ERROR_ACCESS_DENIED);

        } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
            IsPathInRootDir (pCanonicalDestName) &&
            g_pMountTable->IsReservedName (pCanonicalDestName, cchCanonicalDestName)) {

            // Disallow renaming over a reserved name.
            SetLastError (ERROR_ALREADY_EXISTS);

        } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
            IsPathInSystemDir (pSubDestName) &&
            FileExistsInROM (pSubDestName) &&
            !CheckROMShadowPermission (pSubDestName)) {

            // Disallow moving a file over a ROM file if the caller does not
            // have ROM Shadow permission.

        } else {
                    

            DWORD Attributes = INVALID_FILE_ATTRIBUTES;

            __try {
                Attributes = AFS_GetFileAttributesW (hSourceVolume, pSubSourceName);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }

            SetLastError (ERROR_SUCCESS);
            __try {
                fRet = AFS_MoveFileW (hSourceVolume, pSubSourceName, pSubDestName);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }

        }

    } else {

        // The old path and new path reside on different file system volumes,
        // so we must use the CopyFile and DeleteFile functions to emulate
        // "moving" the file between volumes.

        DWORD Attributes = INVALID_FILE_ATTRIBUTES;

        __try {
            Attributes = AFS_GetFileAttributesW (hSourceVolume, pSubSourceName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }

        if (INVALID_FILE_ATTRIBUTES != Attributes) {

            if (FILE_ATTRIBUTE_DIRECTORY & Attributes) {

                // We're unable to copy a directory from one device to another,
                // so explicitly disallow this case with an appropriate error code.
                SetLastError (ERROR_NOT_SAME_DEVICE);

            } else if (Attributes & FILE_ATTRIBUTE_INROM) {

                // We disallow copying a file from ROM to another volume because
                // it cannot be deleted.
                SetLastError (ERROR_ACCESS_DENIED);
            }
        }

        // CopyFile will re-enter CreateFile where permissions will be checked.
        if (CopyFileW (pCanonicalSourceName, pCanonicalDestName, TRUE)) {

            // On successful CopyFile, delete the original file to mimic proper
            // MoveFile behavior. Perform necessary access checks prior to deletion.

            __try {
                AFS_SetFileAttributesW (hSourceVolume, pSubSourceName, FILE_ATTRIBUTE_NORMAL);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }

            // Delete the old file to emulate MoveFile with CopyFile. If this
            // fails, we still succeed the MoveFile but the original source file
            // will still exist on the source volume.
            __try {
                AFS_DeleteFileW (hSourceVolume, pSubSourceName);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }

            // Succeed even if the deletion failed.
            fRet = TRUE;
        }
    }

exit:
    SafeFreeCanonicalPath (pCanonicalSourceName);
    SafeFreeCanonicalPath (pCanonicalDestName);
    return fRet;
}

EXTERN_C BOOL FS_DeleteAndRenameFileW (const WCHAR* pDestFileName,
    const WCHAR* pSourceFileName)
{
    // Copy and canonicalize the paths before processing the path name.
    size_t cchCanonicalSourceName = 0;
    WCHAR* pCanonicalSourceName = SafeGetCanonicalPathW (pSourceFileName, &cchCanonicalSourceName);
    if (!pCanonicalSourceName) {
        return FALSE;
    }

    // Only retrieve the security object from the destination volume. If the
    // volumes are not the same, DeleteAndRenameFile will fail prior to
    // checking access.
    size_t cchCanonicalDestName = 0;
    WCHAR* pCanonicalDestName = SafeGetCanonicalPathW (pDestFileName, &cchCanonicalDestName);
    if (!pCanonicalDestName) {
        SafeFreeCanonicalPath (pCanonicalSourceName);
        return FALSE;
    }

    HANDLE hSourceVolume;
    HANDLE hDestVolume;
    const WCHAR* pSubSourceName;
    const WCHAR* pSubDestName;
    DWORD MountFlags;

    BOOL fRet = FALSE;

    // Determine which volume owns each of the provided paths.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalSourceName,
        &pSubSourceName, &hSourceVolume, &MountFlags);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    lResult = g_pMountTable->GetVolumeFromPath (pCanonicalDestName,
        &pSubDestName, &hDestVolume);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if (IsRootPathName (pSubSourceName) ||
        IsRootPathName (pSubDestName)) {

        // Disallow deleting or renaming a mount point.
        SetLastError (ERROR_ACCESS_DENIED);

    } else if (hSourceVolume != hDestVolume) {

        // The old path and new path reside on different file system volumes,
        // which is not allowed for DeleteAndRenameFile.

        SetLastError (ERROR_NOT_SAME_DEVICE);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
           ((!FileExistsOnFS (hSourceVolume, pSubSourceName) &&
             FileExistsInROM (pCanonicalSourceName)) ||
            (!FileExistsOnFS (hDestVolume, pSubDestName) &&
             FileExistsInROM (pCanonicalDestName)))) {

        // We cannot perform DeleteAndRenameFile with the destination or
        // source file in ROM unless it is shadowed on the root volume.
        SetLastError (ERROR_ACCESS_DENIED);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        (IsPathInRootDir (pCanonicalSourceName) &&
        g_pMountTable->IsReservedName (pCanonicalSourceName, cchCanonicalSourceName)) ||
        (IsPathInRootDir (pCanonicalDestName) &&
        g_pMountTable->IsReservedName (pCanonicalDestName, cchCanonicalDestName))) {

        // Cannot delete or rename a reserved name.
        SetLastError (ERROR_ACCESS_DENIED);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        IsPathInSystemDir (pSubDestName) &&
        FileExistsInROM (pSubDestName) &&
        !CheckROMShadowPermission (pSubDestName)) {

        // Disallow moving a file over a ROM file if the caller does not
        // have ROM Shadow permission.

    } else {

        // Both the old path and the new path reside on the same file system
        // volume, so simply issue a file system DeleteAndRenameFile call.
        SetLastError (ERROR_SUCCESS);
        __try {
            // DeleteAndRenameFile; maybe we should rename the thunk for consistency.
            fRet = AFS_PrestoChangoFileName (hSourceVolume, pSubDestName,
                pSubSourceName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }

exit:
    SafeFreeCanonicalPath (pCanonicalSourceName);
    SafeFreeCanonicalPath (pCanonicalDestName);
    return fRet;
}

EXTERN_C BOOL FS_GetDiskFreeSpaceExW(const WCHAR* pPathName, ULARGE_INTEGER* pFreeBytesAvailableToCaller,
    ULARGE_INTEGER* pTotalNumberOfBytes, ULARGE_INTEGER* pTotalNumberOfFreeBytes)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName);
    if (!pCanonicalName) {
        return FALSE;
    }

    BOOL fRet = FALSE;

    // Sanity-check that the path passed by the caller actually exists
    // prior to querying free space.
    DWORD Attributes = GetFileAttributesW (pCanonicalName);

    if (INVALID_FILE_ATTRIBUTES == Attributes) {
        // The path does not exist.
        SetLastError (ERROR_PATH_NOT_FOUND);
        goto exit;
    }

    if (!(FILE_ATTRIBUTE_DIRECTORY & Attributes)) {
        // The path exists, but it is not a directory.
        SetLastError (ERROR_DIRECTORY);
        goto exit;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    // FSDs only implement the non-Ex version of GetDiskFreeSpace.
    DWORD SectorsPerCluster = 0;
    DWORD BytesPerSector = 0;
    DWORD FreeClusters = 0;
    DWORD TotalClusters = 0;

    SetLastError (ERROR_SUCCESS);
    __try {
        fRet = AFS_GetDiskFreeSpace (hVolume, pSubPathName, &SectorsPerCluster,
            &BytesPerSector, &FreeClusters, &TotalClusters);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError (ERROR_EXCEPTION_IN_SERVICE);
    }

    if (fRet) {
        // On success, convert the GetDiskFreeSpace results into
        // GetDiskFreeSpaceEx results.
        __try {
            if (pFreeBytesAvailableToCaller) {
                pFreeBytesAvailableToCaller->QuadPart =
                    (__int64)FreeClusters * (__int64)SectorsPerCluster * (__int64)BytesPerSector;
            }
            if (pTotalNumberOfFreeBytes) {
                pTotalNumberOfFreeBytes->QuadPart =
                    (__int64)FreeClusters * (__int64)SectorsPerCluster * (__int64)BytesPerSector;
            }
            if (pTotalNumberOfBytes) {
                pTotalNumberOfBytes->QuadPart =
                    (__int64)TotalClusters * (__int64)SectorsPerCluster * (__int64)BytesPerSector;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_INVALID_PARAMETER);
            fRet = FALSE;
        }
    }

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

static HANDLE InternalFindFirstFileW (const WCHAR* pPathName,
    HANDLE hProcess, WIN32_FIND_DATA* pFindData, DWORD SizeOfFindData)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName);
    if (!pCanonicalName) {
        return INVALID_HANDLE_VALUE;
    }

    // Replace final "\*.*" with "*" for proper DOS behavior.
    SimplifySearchSpec (pCanonicalName);

    HANDLE h = INVALID_HANDLE_VALUE;

    // Enter the volume.
    HANDLE hVolume;
    const WCHAR* pSubPathName;
    DWORD MountFlags;

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume, &MountFlags);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    // Don't let a any process other than the kernel call into a kernel-only volume.
    if ((AFS_FLAG_KMODE & MountFlags) &&
        (GetCurrentProcessId () != reinterpret_cast<DWORD> (hProcess))) {
        DEBUGMSG (ZONE_APIS, (L"FSDMGR!InternalFindFirstFileW: ACCESS DENIED: accessing kernel-only volume from user app=0x%x", hProcess));
        SetLastError (ERROR_ACCESS_DENIED);
        goto exit;
    }

    if ((!(AFS_FLAG_HIDDEN & MountFlags) && IsPathInRootDir (pCanonicalName)) ||
        IsPathInSystemDir (pCanonicalName)) {

        // Paths in the root directory that DO NOT refer to hidden mount points
        // should always be enumerated by the root file system.

        h = ROOTFS_FindFirstFileW (hProcess, pCanonicalName, pFindData, SizeOfFindData);
    }

    if (INVALID_HANDLE_VALUE == h) {

        SetLastError (ERROR_SUCCESS);
        __try {
            h = AFS_FindFirstFileW (hVolume, hProcess, pSubPathName, pFindData, SizeOfFindData);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return h;
}

EXTERN_C HANDLE FSINT_FindFirstFileW (const WCHAR* pPathName,
    WIN32_FIND_DATA* pFindData, DWORD SizeOfFindData)
{
    HANDLE hProcess = (HANDLE) GetCurrentProcessId ();
    return InternalFindFirstFileW (pPathName, hProcess, pFindData, SizeOfFindData);
}

EXTERN_C HANDLE FSEXT_FindFirstFileW (const WCHAR* pPathName,
    WIN32_FIND_DATA* pFindData, DWORD SizeOfFindData)
{
    HANDLE hProcess = (HANDLE) GetCallerVMProcessId ();
    HANDLE h = InternalFindFirstFileW (pPathName, hProcess, pFindData, SizeOfFindData);
    if (INVALID_HANDLE_VALUE != h) {

        if (!::FsdDuplicateHandle (
            reinterpret_cast<HANDLE> (GetCurrentProcessId ()), h,
            reinterpret_cast<HANDLE> (GetCallerVMProcessId ()), &h, 0, FALSE,
            DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {

            DEBUGCHK (NULL);
            return INVALID_HANDLE_VALUE;
        }
    }

    return h;
}

static HANDLE InternalCreateFileW (const WCHAR* pPathName, HANDLE hProcess,
    DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* /* pSecurityAttributes */,
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate)
{
    // The TRUNCATE_EXISTING flag requires write access.
    if((TRUNCATE_EXISTING == Create) && !(GENERIC_WRITE & Access)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }

    // Copy and canonicalize the path before processing the path name.
    size_t cchCanonicalName = 0;
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName, &cchCanonicalName);
    if (!pCanonicalName) {
        return INVALID_HANDLE_VALUE;
    }

    if (IsLegacyDeviceName (pPathName)) {
        // For backwards compatibility, allow legacy devices to be opened
        // no matter what the creation flag by replacing with OPEN_EXISTING
        // before calling into device manager.
        Create = OPEN_EXISTING;
    }

    BOOL fPathInRootDir = IsPathInRootDir (pCanonicalName);

    if (fPathInRootDir &&
        g_pMountTable->IsReservedName (pCanonicalName, cchCanonicalName) &&
        (Create != OPEN_EXISTING)) {

        // This is a reserved name off of the root. Don't let a file of
        // the same name be created.  Allow OPEN_EXISTING to open directory handles.
        SetLastError (ERROR_ACCESS_DENIED);
        SafeFreeCanonicalPath (pCanonicalName);
        return INVALID_HANDLE_VALUE;
    }

    HANDLE h = INVALID_HANDLE_VALUE;
    HANDLE hVolume;
    const WCHAR* pSubPathName;
    DWORD MountFlags;
    BOOL fDoROM = FALSE;
    DWORD Attributes = 0;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume, &MountFlags);

    if (ERROR_SUCCESS != lResult) {

        // Failed to enter the volume (root) for whatever reason, so always perform
        // a ROM search if this is the system directory. We skip access checks in
        // this case, which is ok because security is not enforced prior to the root
        // file system being available.
        fDoROM = IsPathInSystemDir (pCanonicalName);
        goto exit;
    }

    // Don't let a any process other than the kernel call into a kernel-only volume.
    if ((AFS_FLAG_KMODE & MountFlags) &&
        (GetCurrentProcessId () != reinterpret_cast<DWORD> (hProcess))) {
        DEBUGMSG (ZONE_APIS, (L"FSDMGR!InternalCreateFileW: ACCESS DENIED: accessing kernel-only volume from user app=0x%x", hProcess));
        lResult = ERROR_ACCESS_DENIED;
        goto exit;
    }

    BOOL fPathInSystemDir = FALSE;

    if (AFS_FLAG_ROOTFS & MountFlags) {
        // Only files on the root file system can be in the system path.
        fPathInSystemDir = IsPathInSystemDir (pSubPathName);
    }

    // Prior to creation, determine whether or not the file already
    // exists on the file system. In the case that this is the root
    // file system, it is possible that we'll fail to open the file on
    // the root file system for a reason other than it not existing
    // (e.g. sharing violation, access violation, etc) so we need to
    // make sure to propagate that failure to the caller instead of
    // opening the shadowed (hidden) file in ROM which would lead to
    // inconsistent behavior.
    __try {
        Attributes = AFS_GetFileAttributesW (hVolume, pSubPathName);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError (ERROR_EXCEPTION_IN_SERVICE);
    }

    if ((AFS_FLAG_ROOTFS & MountFlags)
        && (INVALID_FILE_ATTRIBUTES == Attributes)) {

        // The file does not exist on the root file system (it is expected that
        // GetFileAttributes on the root file system will always work properly).

        fDoROM = fPathInSystemDir ? FileExistsInROM(pSubPathName) : FALSE;

        if (!fDoROM
            && (OPEN_EXISTING == Create)
            && (0 != _wcsicmp(pSubPathName, L"\\VOL:"))) {
            // The file does not exist in the root file system nor does
            // it exist in ROM (if it is in the system dir) so we can
            // bail out of CreateFile early with failure and skip unnecessary
            // security checks and addition calls into the file system driver.
            // Preserve the error code set by AFS_GetFileAttributes or
            // FileExistsInROM.
            lResult = FsdGetLastError (ERROR_FILE_NOT_FOUND);
            goto exit;
        }

        // We must explicitly handle the case where the caller specifies 
        // OPEN_ALWAYS and the file exists in ROM but is NOT already shadowed 
        // on the root file system. In this case, skip calling into the root 
        // fs, and open a handle to the file in ROM (because we want to open 
        // the exiting file, not create a new shadow copy of it).
        if (fDoROM
            && (OPEN_ALWAYS == Create))
        {
            goto exit;
        }
    }

    //
    // Perform security access checks
    //
                    
    // Start with an access mask of zero, and  add additional access rights based
    // on those requested by the caller.
    ACCESS_MASK AccessMask = 0;

    if (MAXIMUM_ALLOWED == Access) {

        Access = AccessMask = FILE_ALL_ACCESS;

    } else {

        // Include any non-generic rights specified by the caller, though
        // it is uncommon to use anything other than generic-rights.
        AccessMask |= Access & (STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL);

        // Map generic access right specified by caller to file-specific
        // access rights.
        AccessMask |= (GENERIC_READ & Access) ?     c_FileAccessMapping.GenericRead : 0;
        AccessMask |= (GENERIC_WRITE & Access) ?    c_FileAccessMapping.GenericWrite : 0;
        AccessMask |= (GENERIC_EXECUTE & Access) ?  c_FileAccessMapping.GenericExecute : 0;
        AccessMask |= (GENERIC_ALL & Access) ?      c_FileAccessMapping.GenericAll  : 0;
    }

    // Caller must at least have READ_CONTROL or CreateFile will not succeed.
    AccessMask |= READ_CONTROL;

    // Always require FILE_GENERIC_WRITE permission to create and/or truncate.
    AccessMask |= (CREATE_ALWAYS == Create) ?       FILE_GENERIC_WRITE : 0;
    AccessMask |= (CREATE_NEW == Create) ?          FILE_GENERIC_WRITE : 0;
    AccessMask |= (TRUNCATE_EXISTING == Create) ?   FILE_GENERIC_WRITE : 0;                
    AccessMask |= (INVALID_FILE_ATTRIBUTES == Attributes) &&
                (OPEN_ALWAYS == Create) ?         FILE_GENERIC_WRITE : 0;

    // NOTE: There is a race condition in the OPEN_ALWAYS case; another thread
    // could either create or delete the file in between the calls to
    // AFS_GetFileAttributesW and AFS_CreateFileW. It doesn't matter much
    // because the worst that can happen is one of two things:
    //
    //  1)  We allow a file to be created that the caller does not have write
    //      access to. This isn't all that bad because the caller cannot write
    //      data to it anyway (if requested GENERIC_WRITE access, we'd require
    //      FILE_GENERIC_WRITE). We know the file existed previously so it
    //      cannot be used to simply fill up the file system with empty files.
    //
    //  2)  We disallow a file to be opened that is newly created. The same
    //      race condition exists regardless.

    // Verify that the caller has permission to create/modify ROM shadow files
    // if necessary.
    if (fPathInSystemDir
        && (FILE_GENERIC_WRITE == (AccessMask & FILE_GENERIC_WRITE))
        && (FileExistsInROM (pSubPathName))
        && !CheckROMShadowPermission (pSubPathName)) {
        // Trying to write to a file that exists in ROM, so make sure the
        // caller has permission to create/modify ROM-shadow files.
        lResult = FsdGetLastError (ERROR_ACCESS_DENIED);
        goto exit;
    }

    if (!(AFS_FLAG_NETWORK & MountFlags)) {
        // Strip-off the GENERIC_EXECUTE bit on all file systems other
        // than the network file system for legacy compatibility with
        // FSDs and Filters.
        Access &= ~GENERIC_EXECUTE;
    }

    //
    // Perform the CreateFile call on the file system.
    //
    SetLastError (ERROR_SUCCESS);
    __try {
        h = AFS_CreateFileW (hVolume, hProcess, pSubPathName, Access, ShareMode,
                NULL, Create, FlagsAndAttributes, hTemplate, NULL, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        lResult = ERROR_EXCEPTION_IN_SERVICE;
    }

exit:

    if (INVALID_HANDLE_VALUE == h) {

        if (ERROR_SUCCESS != lResult) {

            // Set appropriate error code on failure.
            SetLastError (lResult);
        }

        // Handle the ROM case when in the virtual system directory:
        if (fDoROM) {

            // If we were unable to open the file at this point and if we've indicated
            // that ROM should be searched, then try to open the file in ROM.

            lResult = FsdGetLastError (ERROR_FILE_NOT_FOUND);
    
            // Try to open the file in ROM.
            h = ROM_CreateFileW (hProcess, pCanonicalName, Access, ShareMode,
                NULL, Create, FlagsAndAttributes, hTemplate);
    
            // If we didn't find the file in ROM, restore the previously set error code.
            if (INVALID_HANDLE_VALUE == h) {
                SetLastError (lResult);
            }
        }
    }

    SafeFreeCanonicalPath (pCanonicalName);
    return h;
}

EXTERN_C HANDLE FSINT_CreateFileW (const WCHAR* pPathName,
    DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* /* pSecurityAttributes */,
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate)
{
    HANDLE hProcess = (HANDLE)GetCurrentProcessId ();
    return InternalCreateFileW (pPathName, hProcess, Access, ShareMode,
        NULL, Create, FlagsAndAttributes, hTemplate);
}

EXTERN_C HANDLE FSEXT_CreateFileW (const WCHAR* pPathName,
    DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* /* pSecurityAttributes */,
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate)
{
    HANDLE hProcess = (HANDLE)GetCallerVMProcessId ();
    HANDLE h = InternalCreateFileW (pPathName, hProcess, Access, ShareMode,
        NULL, Create, FlagsAndAttributes, hTemplate);

    if (INVALID_HANDLE_VALUE != h) {

        if (!::FsdDuplicateHandle (
            reinterpret_cast<HANDLE> (GetCurrentProcessId ()), h,
            reinterpret_cast<HANDLE> (GetCallerVMProcessId ()), &h, 0, FALSE,
            DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {

            DEBUGCHK (NULL);
            return INVALID_HANDLE_VALUE;
        }
    }

    return h;
}

static DWORD VolumeIoctlAccessRequired(DWORD dwIoctl)
{
    DWORD AccessMask;

    switch (dwIoctl) {

        case FSCTL_GET_VOLUME_INFO:
            AccessMask = 0; // Anyone can query volume info, so skip the check.
            break;

        case FSCTL_FLUSH_BUFFERS:
            AccessMask = FILE_GENERIC_READ;
            break;

        default:
            AccessMask = FILE_ALL_ACCESS;
            break;
    }

    return AccessMask;
}

EXTERN_C BOOL STOREMGR_FsIoControlW (HANDLE hProcess, const WCHAR* pPathName, DWORD Fsctl,
    void* pInBuf, DWORD InBufSize, void* pOutBuf, DWORD OutBufSize,
    DWORD* pBytesReturned, OVERLAPPED* pOverlapped)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    DWORD MountFlags;
    BOOL fRet = FALSE;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume, &MountFlags);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    // Don't let a any process other than the kernel call into a kernel-only volume.
    if ((AFS_FLAG_KMODE & MountFlags) &&
        (GetCurrentProcessId () != reinterpret_cast<DWORD> (hProcess))) {
        DEBUGMSG (ZONE_APIS, (L"FSDMGR!STOREMGR_FsIoControlW: ACCESS DENIED: accessing kernel-only volume from user app=0x%x", hProcess));
        SetLastError (ERROR_ACCESS_DENIED);
        goto exit;
    }

    SetLastError (ERROR_SUCCESS);
    __try {
        fRet = AFS_FsIoControlW (hVolume, hProcess, Fsctl, pInBuf, InBufSize,
            pOutBuf, OutBufSize, pBytesReturned, pOverlapped);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError (ERROR_EXCEPTION_IN_SERVICE);
    }

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

EXTERN_C BOOL FS_IsSystemFileW (const WCHAR* pFileName)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pFileName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    DWORD MountFlags;
    BOOL fRet = FALSE;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume, &MountFlags);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if (AFS_FLAG_SYSTEM & MountFlags) {

        fRet = TRUE;

    } else {

        DWORD Attributes = INVALID_FILE_ATTRIBUTES;

        __try {
            Attributes = AFS_GetFileAttributesW (hVolume, pSubPathName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }

        fRet = (INVALID_FILE_ATTRIBUTES != Attributes) &&
            (FILE_ATTRIBUTE_SYSTEM & Attributes);
    }

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

static HANDLE InternalFindFirstChangeNotificationW (HANDLE hProc, const WCHAR* pPathName,
    BOOL fWatchSubTree, DWORD NotifyFilter)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName);
    if (!pCanonicalName) {
        return INVALID_HANDLE_VALUE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    HANDLE hRet = INVALID_HANDLE_VALUE;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubPathName, &hVolume);
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    SetLastError (ERROR_SUCCESS);
    __try {
        hRet = AFS_FindFirstChangeNotificationW (hVolume, hProc, pSubPathName, fWatchSubTree,
            NotifyFilter);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError (ERROR_EXCEPTION_IN_SERVICE);
    }

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return hRet;

}

EXTERN_C HANDLE FSEXT_FindFirstChangeNotificationW (const WCHAR* pPathName,
    BOOL fWatchSubTree, DWORD NotifyFilter)
{
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCallerVMProcessId ());
    return InternalFindFirstChangeNotificationW (hProcess, pPathName, fWatchSubTree, NotifyFilter);
}

EXTERN_C HANDLE FSINT_FindFirstChangeNotificationW (const WCHAR* pPathName,
    BOOL fWatchSubTree, DWORD NotifyFilter)
{
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCurrentProcessId ());
    return InternalFindFirstChangeNotificationW (hProcess, pPathName, fWatchSubTree, NotifyFilter);
}

EXTERN_C BOOL FSEXT_FindNextChangeNotification (HANDLE h)
{
    SetLastError (ERROR_SUCCESS);
    return NotifyGetNextChange (h, 0, NULL, 0, NULL, NULL);
}

EXTERN_C BOOL FSINT_FindNextChangeNotification (HANDLE h)
{
    SetLastError (ERROR_SUCCESS);
    return INT_NotifyGetNextChange (h, 0, NULL, 0, NULL, NULL);
}

EXTERN_C BOOL FSEXT_FindCloseChangeNotification (HANDLE h)
{
    SetLastError (ERROR_SUCCESS);
    return NotifyCloseChangeHandle (h);
}

EXTERN_C BOOL FSINT_FindCloseChangeNotification (HANDLE h)
{
    SetLastError (ERROR_SUCCESS);
    return INT_NotifyCloseChangeHandle (h);
}

EXTERN_C BOOL FSEXT_GetFileNotificationInfoW (HANDLE h, DWORD Flags, void* pBuffer,
    DWORD BufferLength, DWORD* pBytesReturned, DWORD* pBytesAvailable)
{
    SetLastError (ERROR_SUCCESS);
    return NotifyGetNextChange (h, Flags, pBuffer, BufferLength, pBytesReturned, pBytesAvailable);
}

EXTERN_C BOOL FSINT_GetFileNotificationInfoW (HANDLE h, DWORD Flags, void* pBuffer,
    DWORD BufferLength, DWORD* pBytesReturned, DWORD* pBytesAvailable)
{
    SetLastError (ERROR_SUCCESS);
    return INT_NotifyGetNextChange (h, Flags, pBuffer, BufferLength, pBytesReturned, pBytesAvailable);
}

EXTERN_C BOOL FS_GetFileSecurityW (
    const WCHAR* /* pFileName */,
    SECURITY_INFORMATION /* RequestedInformation */,
    PSECURITY_DESCRIPTOR /* pSecurityDescriptor */,
    DWORD /* Length */,
    DWORD* /* pLengthNeeded */
    )
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

EXTERN_C BOOL FS_SetFileSecurityW (
    const WCHAR* /* pFileName */,
    SECURITY_INFORMATION /* SecurityInformation */,
    PSECURITY_DESCRIPTOR /* pSecurityDescriptor */,
    DWORD /* Length */
    )
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}
