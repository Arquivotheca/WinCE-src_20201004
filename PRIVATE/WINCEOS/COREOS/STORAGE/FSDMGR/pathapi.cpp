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
#include "bufferpool.hpp"

typedef BufferPool<WCHAR, MAX_PATH> MaxPathPool;
MaxPathPool g_MaxPathPool (5);

#ifdef UNDER_CE

typedef HANDLE (*PFN_FIND_FIRST_DEVICE) (DeviceSearchType, LPCVOID, PDEVMGR_DEVICE_INFORMATION);

static PFN_FIND_FIRST_DEVICE g_pfnFindFirstDevice;

static LRESULT TranslateLegacyDeviceName (const WCHAR* pLegacyName,
    __out_ecount(NameChars) WCHAR* pName, size_t NameChars)
{
    if (!g_pfnFindFirstDevice) {
        return ERROR_DEV_NOT_EXIST;
    }

    if (*pLegacyName == L'\\') {
        pLegacyName++;
    }

    DEVMGR_DEVICE_INFORMATION DeviceInfo;
    DeviceInfo.dwSize = sizeof (DEVMGR_DEVICE_INFORMATION);

    // Use the FindFirstDevice API to translate a legacy device name into
    // its full device name.
    HANDLE hFind = g_pfnFindFirstDevice (DeviceSearchByLegacyName, pLegacyName, &DeviceInfo);
    if (INVALID_HANDLE_VALUE == hFind) {
        return ERROR_DEV_NOT_EXIST;
    }
    VERIFY (FindClose (hFind));

    // Use the canonical version of the name returned by FindFirstDevice.
    if (!CeGetCanonicalPathNameW(DeviceInfo.szDeviceName, pName, NameChars, 0)) {
        // Preseve the error code set by CeGetCanonicalPathNameW
        return FsdGetLastError ();
    }

    DEBUGMSG (ZONE_APIS, (L"FSDMGR!TranslateLegacyDeviceName: \"%s\" --> \"%s\"", pLegacyName, pName));

    return ERROR_SUCCESS;
}

#else 

static LRESULT TranslateLegacyDeviceName (const WCHAR* pLegacyName, WCHAR* pName, 
    size_t NameChars)
{
    return ERROR_DEV_NOT_EXIST;
}

#endif // UNDER_CE



LRESULT InitializePathAPI ()
{
#ifdef UNDER_CE
    // Initialize the handle-based file API.
    HMODULE hCoreDll = GetModuleHandle (L"coredll.dll");
    DEBUGCHK (hCoreDll);
    g_pfnFindFirstDevice = (PFN_FIND_FIRST_DEVICE)FsdGetProcAddress(hCoreDll, L"FindFirstDevice");
#endif
    return ERROR_SUCCESS;
}


// Symmetric function to free the path from SafeGetCanonicalPathW.
VOID SafeFreeCanonicalPath (LPWSTR lpszCPathName)
{
    DEBUGCHK(lpszCPathName);  // Catch cases where we free NULL
    if (lpszCPathName) {
        g_MaxPathPool.FreeBuffer (lpszCPathName);
    }
}

// Returns newly allocated path if successful, NULL if probe or
// canonicalization failed.  If lpszPathName is NULL this function will
// return NULL.  Free the buffer using SafeFreeCanonicalPath.
LPWSTR SafeGetCanonicalPathW (
    LPCWSTR lpszPathName            // Caller buffer, NULL-terminated with MAX_PATH maximum length
    )
{
    LPWSTR lpszCanonicalPathName = NULL;
    LRESULT lResult = ERROR_SUCCESS;

    if (!lpszPathName) {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    lpszCanonicalPathName = g_MaxPathPool.AllocateBuffer ();  // Always MAX_PATH long
    if (!lpszCanonicalPathName) {
        lResult = ERROR_OUTOFMEMORY;
        goto exit;
    }

    __try {
        if (!CeGetCanonicalPathNameW(lpszPathName, lpszCanonicalPathName, MAX_PATH, 0)) {
            // Preseve the error code set by CeGetCanonicalPathNameW
            lResult = FsdGetLastError ();
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        lResult = ERROR_INVALID_PARAMETER;
    }

    if (ERROR_SUCCESS != lResult) {
        goto exit;
    }

    if (IsLegacyDeviceName (lpszCanonicalPathName)) {

        // After canonicalization, perform a legacy device name translation
        // if this appears to be a legacy device name.

        LPWSTR lpszDeviceName = g_MaxPathPool.AllocateBuffer ();
        if (!lpszDeviceName) {
            lResult = ERROR_OUTOFMEMORY;
            goto exit;
        }

        lResult = TranslateLegacyDeviceName (lpszCanonicalPathName, lpszDeviceName, MAX_PATH);
        if (ERROR_SUCCESS != lResult) {
            // Failed TranslateLegacyDeviceName. A device with this name
            // probably doesn't exist on the system.
            SafeFreeCanonicalPath (lpszDeviceName);
            goto exit;
        }

        // Free the original canonical name and use the device name instead.
        SafeFreeCanonicalPath(lpszCanonicalPathName);
        lpszCanonicalPathName = lpszDeviceName;
    }

    lResult = ERROR_SUCCESS;

exit:
    if (ERROR_SUCCESS != lResult) {
        if (lpszCanonicalPathName) {
            SafeFreeCanonicalPath (lpszCanonicalPathName);
            lpszCanonicalPathName = NULL;
        }
        SetLastError (lResult);
    }
    return lpszCanonicalPathName;
}

static void SimplifySearchSpec (__inout WCHAR* pSearchSpec)
{
    // Search for the last path token in the string.
    WCHAR* pLastToken = wcsrchr (pSearchSpec, L'\\');
    if (pLastToken &&
        (0 == wcscmp (L"\\*.*", pLastToken))) {
        // This search string ends in \*.*, truncate it to \* for DOS
        // behavior/compatibility.
        pLastToken[2] = L'\0';
    }
}

static inline FileExistsOnFS (HANDLE hVolume, const WCHAR* pName)
{
    DWORD Attributes = INVALID_FILE_ATTRIBUTES;
    __try {
        Attributes = AFS_GetFileAttributesW (hVolume, pName);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return (INVALID_FILE_ATTRIBUTES != Attributes);
}

static LRESULT SystemFileAccessCheck (HANDLE hVolume, DWORD MountFlags, 
    const WCHAR* pSubPathName, DWORD Access, DWORD ShareMode)
{
    // TODO: Implement system file access check.
    return ERROR_SUCCESS;
}


#ifdef UNDER_CE
EXTERN_C BOOL FSEXT_CreateDirectoryW (const WCHAR* pPathName, 
    SECURITY_ATTRIBUTES* pSecurityAttributes)
{
    // Make a local copy of the input SECURITY_ATTRIBUTES structure because
    // it contains an embedded pointer.
    SECURITY_ATTRIBUTES LocalSecurityAttributes;
    if (pSecurityAttributes && !CeSafeCopyMemory (&LocalSecurityAttributes,
            pSecurityAttributes, sizeof (SECURITY_ATTRIBUTES))) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    pSecurityAttributes = pSecurityAttributes ? &LocalSecurityAttributes : NULL;

    return FSINT_CreateDirectoryW (pPathName, pSecurityAttributes);
}
#endif //UNDER_CE

#ifdef UNDER_CE
EXTERN_C BOOL FSINT_CreateDirectoryW (const WCHAR* pPathName, 
#else
EXTERN_C BOOL FS_CreateDirectoryW (const WCHAR* pPathName, 
#endif //UNDER_CE
    SECURITY_ATTRIBUTES* pSecurityAttributes)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    BOOL fRet = FALSE;
    DWORD MountFlags;

    // Determine which volume owns this path and enter that volume.
#ifdef UNDER_CE
    HANDLE hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags, &hSecurity);
#else
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags);
#endif //UNDER_CE

    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }
    
    if ((L'\\' == pSubPathName[0]) &&
        (L'\0' == pSubPathName[1])) {

        // Single slash indicates the name of a mount point was passed in.
        // Disallow creating a directory with this name because it already
        // exists.
        SetLastError (ERROR_ALREADY_EXISTS);
        
    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        IsPathInRootDir (pCanonicalName) && 
        g_pMountTable->IsReservedName (pCanonicalName, wcslen (pCanonicalName))) {
        
        // This is a reserved name off of the root. Don't let a directory of
        // the same name be created.
        SetLastError (ERROR_ALREADY_EXISTS);

#ifdef UNDER_NT
    } else {

        SetLastError (ERROR_SUCCESS);
        __try {
            fRet = AFS_CreateDirectoryW (hVolume, pSubPathName, NULL, NULL, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }
#else
    } else {

        g_pMountTable->SecurityManager.LockVolume (hSecurity);

        PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;
        DWORD SecurityDescriptorSize = 0;

        // Requires FILE_ADD_SUBDIRECTORY access to the parent directory.
        lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
            pSubPathName, CE_DIRECTORY_OBJECT, GetCurrentToken (), 0, 
            FILE_ADD_SUBDIRECTORY, FILE_ATTRIBUTE_DIRECTORY, pSecurityAttributes,
            &pSecurityDescriptor, &SecurityDescriptorSize);

        if (ERROR_SUCCESS == lResult) {
            SetLastError (ERROR_SUCCESS);
            __try {
                fRet = AFS_CreateDirectoryW (hVolume, pSubPathName, NULL,
                    pSecurityDescriptor, SecurityDescriptorSize);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }
        } else {
            // Access check failed.
            SetLastError (lResult);
        }

        g_pMountTable->SecurityManager.FreeSecurityDescriptor (hSecurity, pSecurityAttributes,
                    pSecurityDescriptor);

        g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
    }

    g_pMountTable->SecurityManager.Release (hSecurity);
#endif //UNDER_NT

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

EXTERN_C BOOL FS_RemoveDirectoryW (const WCHAR* pPathName)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    BOOL fRet = FALSE;
    DWORD MountFlags;

    // Determine which volume owns this path and enter that volume.
#ifdef UNDER_CE
    HACLVOL hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags, &hSecurity);
#else
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags);
#endif //UNDER_CE
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if ((L'\\' == pSubPathName[0]) &&
        (L'\0' == pSubPathName[1])) {

        // Single slash indicates the name of a mount point was passed in.
        // Disallow removing a mount point.
        SetLastError (ERROR_ACCESS_DENIED);
        
    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        IsPathInRootDir (pCanonicalName) && 
        g_pMountTable->IsReservedName (pCanonicalName, wcslen (pCanonicalName))) {
        
        // This is a reserved name off of the root; it cannot be removed.
        SetLastError (ERROR_ACCESS_DENIED);

#ifdef UNDER_NT
    } else {

        SetLastError (ERROR_SUCCESS);
        __try {
            fRet = AFS_RemoveDirectoryW (hVolume, pSubPathName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }
#else
    } else {
    
        g_pMountTable->SecurityManager.LockVolume (hSecurity);

        // Requires DELETE permission to the target directory and
        // FILE_DELETE_CHILD permission to the parent directory.
        lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
            pSubPathName, CE_DIRECTORY_OBJECT, GetCurrentToken (), DELETE,
            FILE_DELETE_CHILD);

        if (ERROR_SUCCESS == lResult) {

            SetLastError (ERROR_SUCCESS);
            __try {
                fRet = AFS_RemoveDirectoryW (hVolume, pSubPathName);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }

        } else {
            // Access check failed.
            SetLastError (lResult);
        }

        g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
    }

    g_pMountTable->SecurityManager.Release (hSecurity);
#endif //UNDER_NT

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
#ifdef UNDER_CE
    HACLVOL hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags, &hSecurity);
#else
        LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags);
#endif //UNDER_CE
    if (ERROR_SUCCESS == lResult) {

    if ((L'\\' == pSubPathName[0]) &&
        (L'\0' == pSubPathName[1])) {
            
            // The sub path is a single slash, so we will return the attributes
            // for the mount point.

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

            // Query the attributes prior to checking access because we need to know
            // if this is a direcotry or a file.
            SetLastError (ERROR_SUCCESS);
            __try {
                Attributes = AFS_GetFileAttributesW (hVolume, pSubPathName);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }

#ifdef UNDER_NT
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
#else
            if (INVALID_FILE_ATTRIBUTES != Attributes) {

                if (!(AFS_FLAG_MOUNTROM & MountFlags)) {
                    // Only ROM file systems are allowed to return ROM file
                    // attributes.
                    DEBUGCHK (!((FILE_ATTRIBUTE_INROM | FILE_ATTRIBUTE_ROMMODULE) & Attributes));
                    Attributes &= ~(FILE_ATTRIBUTE_INROM | FILE_ATTRIBUTE_ROMMODULE);
                }

                if (AFS_FLAG_SYSTEM & MountFlags) {
                    // All files and directories on SYSTEM volumes have the
                    // SYSTEM file attribute.
                    Attributes |= FILE_ATTRIBUTE_SYSTEM;
                }

                // Perform a security check after getting the attributes.
                DWORD ObjectType = (FILE_ATTRIBUTE_DIRECTORY & Attributes) ?
                    CE_DIRECTORY_OBJECT : CE_FILE_OBJECT;

                g_pMountTable->SecurityManager.LockVolume (hSecurity);

                // Requires FILE_READ_ATTRIBUTES permission to the target file or
                // directory OR FILE_LIST_DIRECTORY permission to the parent.
                lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
                    pSubPathName, ObjectType, GetCurrentToken (), FILE_READ_ATTRIBUTES, 0);

                if (ERROR_ACCESS_DENIED == lResult) {
                    // Did not have FILE_READ_ATTRIBUTES permission to the target file,
                    // check for FILE_LIST_DIRECTORY permission to the parent instead.
                    lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
                        pSubPathName, ObjectType, GetCurrentToken (), 0, FILE_LIST_DIRECTORY);
                }

                if (ERROR_SUCCESS != lResult) {
                    // Failed the access check, so return invalid attributes.
                    Attributes = INVALID_FILE_ATTRIBUTES;
                    SetLastError (lResult);
                }

                g_pMountTable->SecurityManager.UnlockVolume (hSecurity);

            }
#endif //UNDER_NT
        }
#ifdef UNDER_CE
        g_pMountTable->SecurityManager.Release (hSecurity);
#endif //UNDER_CE
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
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubPathName;
    BOOL fRet = FALSE;
    DWORD MountFlags;

    // Determine which volume owns this path and enter that volume.
#ifdef UNDER_CE
    HACLVOL hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags, &hSecurity);
#else
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags);
#endif
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if ((L'\\' == pSubPathName[0]) &&
        (L'\0' == pSubPathName[1])) {
        
        // The attributes of a mount point or the root directory cannot
        // be modified, so disallow if the subpath is empty (indicating
        // this is the root of a mount point).
        SetLastError (ERROR_ACCESS_DENIED);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        !FileExistsOnFS (hVolume, pSubPathName) &&
        FileExistsInROM (pCanonicalName)) {

        // Cannot change the attributes of a file that exists only in ROM.
        SetLastError (ERROR_ACCESS_DENIED);

    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        IsPathInRootDir (pCanonicalName) && 
        g_pMountTable->IsReservedName (pCanonicalName, wcslen (pCanonicalName))) {

        // Cannot change the attributes of a reserved root.
        SetLastError (ERROR_ACCESS_DENIED);

#ifdef UNDER_NT
    } else {

        // Explicitly disallow setting ROM attributes on any file.
        NewAttributes &= ~(FILE_ATTRIBUTE_INROM | FILE_ATTRIBUTE_ROMMODULE);

        SetLastError (ERROR_SUCCESS);
        __try {
            fRet = AFS_SetFileAttributesW (hVolume, pSubPathName, NewAttributes);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }
#else
    } else {

        // Explicitly disallow setting ROM attributes on any file.
        NewAttributes &= ~(FILE_ATTRIBUTE_INROM | FILE_ATTRIBUTE_ROMMODULE);

        g_pMountTable->SecurityManager.LockVolume (hSecurity);

        // Check the attributes prior to performing access check because
        // we need to know if it is a file or directory.
        DWORD ExistingAttributes = INVALID_FILE_ATTRIBUTES;

        __try {
            ExistingAttributes = AFS_GetFileAttributesW (hVolume, pSubPathName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }

        DWORD ObjectType = (INVALID_FILE_ATTRIBUTES != ExistingAttributes) && 
            (FILE_ATTRIBUTE_DIRECTORY & ExistingAttributes) ?
            CE_DIRECTORY_OBJECT : CE_FILE_OBJECT;

        // Requires FILE_WRITE_ATTRIBUTES permission to the target file or
        // directory.
        lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
            pSubPathName, ObjectType, GetCurrentToken (), FILE_WRITE_ATTRIBUTES,
            0, NewAttributes);

        if (ERROR_SUCCESS == lResult) {

            SetLastError (ERROR_SUCCESS);
            __try {
                fRet = AFS_SetFileAttributesW (hVolume, pSubPathName, NewAttributes);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }

        } else {

            // Failed security check.
            SetLastError (lResult);
        }

        g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
    }

    g_pMountTable->SecurityManager.Release (hSecurity);
#endif //UNDER_NT

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

EXTERN_C BOOL FS_DeleteFileW (const WCHAR* pFileName)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pFileName);
    if (!pCanonicalName) {
        return FALSE;
    }

    HANDLE hVolume;

    const WCHAR* pSubPathName;
    BOOL fRet = FALSE;
    DWORD MountFlags;

    // Determine which volume owns this path and enter that volume.
#ifdef UNDER_CE
    HACLVOL hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags, &hSecurity);
#else
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags);
#endif //UNDER_CE
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if ((L'\\' == pSubPathName[0]) &&
        (L'\0' == pSubPathName[1])) {
        
        // Cannot delete a mount point.
        SetLastError (ERROR_FILE_NOT_FOUND);
        
    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        IsPathInRootDir (pCanonicalName) && 
        g_pMountTable->IsReservedName (pCanonicalName, wcslen (pCanonicalName))) {

        // Cannot delete a reserved name.
        SetLastError (ERROR_FILE_NOT_FOUND);
    
    } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
        !FileExistsOnFS (hVolume, pSubPathName) &&
        FileExistsInROM (pCanonicalName)) {
    
        // Cannot delete a file that exists only in ROM.
        SetLastError (ERROR_ACCESS_DENIED);

#ifdef UNDER_NT
    } else {

        SetLastError (ERROR_SUCCESS);
        __try {
            fRet = AFS_DeleteFileW (hVolume, pSubPathName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }
#else
    } else {

        g_pMountTable->SecurityManager.LockVolume (hSecurity);

        // Requires DELETE permission to the target file and FILE_DELETE_CHILD
        // permission to the parent directory.
        lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (
            hSecurity, pSubPathName, CE_FILE_OBJECT, GetCurrentToken (),
            DELETE, FILE_DELETE_CHILD);

        if (ERROR_SUCCESS == lResult) {

            SetLastError (ERROR_SUCCESS);
            __try {
                fRet = AFS_DeleteFileW (hVolume, pSubPathName);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }

        } else {
            // Failed security check.
            SetLastError (lResult);
        }

        g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
    }

    g_pMountTable->SecurityManager.Release (hSecurity);
#endif //UNDER_NT

exit:
    SafeFreeCanonicalPath (pCanonicalName);
    return fRet;
}

EXTERN_C BOOL FS_MoveFileW (const WCHAR* pSourceFileName, const WCHAR* pDestFileName)
{
    // Copy and canonicalize the paths before processing the path name.
    WCHAR* pCanonicalSourceName = SafeGetCanonicalPathW (pSourceFileName);
    if (!pCanonicalSourceName) {
        return FALSE;
    }

    WCHAR* pCanonicalDestName = SafeGetCanonicalPathW (pDestFileName);
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
#ifdef UNDER_CE
    HACLVOL hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalSourceName,
        &pSubSourceName, &hSourceVolume, &MountFlags, &hSecurity);
#else
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalSourceName,
        &pSubSourceName, &hSourceVolume, &MountFlags);
#endif
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
    
    if (L'\0' == *pSubSourceName) {
        
        // Empty string indicates the name of a mount point was passed in.
        // Disallow renaming a mount point.
        SetLastError (ERROR_ACCESS_DENIED);
    
    } else if (L'\0' == *pSubDestName) {
    
        // Empty string indicates the name of a mount point was passed in.
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
            g_pMountTable->IsReservedName (pCanonicalSourceName, wcslen (pCanonicalSourceName))) {
            
            // Disallow renaming a reserved name.
            SetLastError (ERROR_ACCESS_DENIED);

        } else if ((AFS_FLAG_ROOTFS & MountFlags) &&
            IsPathInRootDir (pCanonicalDestName) && 
            g_pMountTable->IsReservedName (pCanonicalDestName, wcslen (pCanonicalDestName))) {

            // Disallow renaming over a reserved name.
            SetLastError (ERROR_ALREADY_EXISTS);

#ifdef UNDER_NT
        } else {
                    
            SetLastError (ERROR_SUCCESS);
            __try {
                fRet = AFS_MoveFileW (hSourceVolume, pSubSourceName, pSubDestName);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }
        }

    } else {    // hSourceVolume != hDestVolume

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
            // MoveFile behavior. 

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
#else
        } else {

            g_pMountTable->SecurityManager.LockVolume (hSecurity);

            DWORD Attributes = INVALID_FILE_ATTRIBUTES;

            __try {
                Attributes = AFS_GetFileAttributesW (hSourceVolume, pSubSourceName);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_EXCEPTION_IN_SERVICE);
            }

            DWORD ObjectType = (INVALID_FILE_ATTRIBUTES != Attributes) && 
                (FILE_ATTRIBUTE_DIRECTORY & Attributes) ?
                CE_DIRECTORY_OBJECT : CE_FILE_OBJECT;

            // Requires FILE_DELETE_CHILD permission to the parent directory of
            // the souce file or directory. Does NOT require DELETE permission
            // to the object itself because it is only being moved or renamed,
            // not deleted.
            lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
                pSubSourceName, ObjectType, GetCurrentToken (), 0, FILE_DELETE_CHILD);

            if (ERROR_SUCCESS == lResult) {

                // Requires FILE_ADD_FILE permission to the parent directory of the
                // target if moving a file. Requires FILE_ADD_SUBDIRECTORY permission
                // to the parent directory of the target if moving a directory.
                DWORD ParentAccessMask = FILE_ADD_FILE;
                if ((INVALID_FILE_ATTRIBUTES != Attributes) &&
                    (FILE_ATTRIBUTE_DIRECTORY & Attributes)) {
                    // Moving a directory, not a file.
                    ParentAccessMask = FILE_ADD_SUBDIRECTORY;
                }

                lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
                    pSubDestName, ObjectType, GetCurrentToken (), 0, ParentAccessMask);

                if (ERROR_SUCCESS == lResult) {
                    // Call MoveFile on the underlying volume.
                    SetLastError (ERROR_SUCCESS);
                    __try {
                        fRet = AFS_MoveFileW (hSourceVolume, pSubSourceName, pSubDestName);
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        SetLastError (ERROR_EXCEPTION_IN_SERVICE);
                    }

                } else {
                    // Failed acccess check on the target file or directory.
                    SetLastError (lResult);
                }

            } else {
                // Failed access check on the source file or directory.
                SetLastError (lResult);
            }

            g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
        }

    } else {    // hSourceVolume != hDestVolume

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
            g_pMountTable->SecurityManager.LockVolume (hSecurity);

            // Requires FILE_WRITE_ATTRIBUTES permission to the source file.
            lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
                pSubSourceName, CE_FILE_OBJECT, GetCurrentToken (), FILE_WRITE_ATTRIBUTES,
                0, FILE_ATTRIBUTE_NORMAL);

            if (ERROR_SUCCESS == lResult) {
                // Attempt to clear any read-only attribute prior to deleting the file.
                __try {
                    AFS_SetFileAttributesW (hSourceVolume, pSubSourceName, FILE_ATTRIBUTE_NORMAL);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            }

            // Requires DELETE permission to the source file and FILE_DELETE_CHILD
            // permission to the source's parent directory.
            lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (
                hSecurity, pSubSourceName, CE_FILE_OBJECT, GetCurrentToken (),
                DELETE, FILE_DELETE_CHILD);

            if (ERROR_SUCCESS == lResult) {
                // Delete the old file to emulate MoveFile with CopyFile. If this
                // fails, we still succeed the MoveFile but the original source file
                // will still exist on the source volume.
                __try {
                    AFS_DeleteFileW (hSourceVolume, pSubSourceName);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                }
            }

            g_pMountTable->SecurityManager.UnlockVolume (hSecurity);

            // Succeed even if the deletion failed.
            fRet = TRUE;
        }
    }

    g_pMountTable->SecurityManager.Release (hSecurity);
#endif //UNDER_NT

exit:
    SafeFreeCanonicalPath (pCanonicalSourceName);
    SafeFreeCanonicalPath (pCanonicalDestName);
    return fRet;
}

EXTERN_C BOOL FS_DeleteAndRenameFileW (const WCHAR* pDestFileName, 
    const WCHAR* pSourceFileName)
{
    // Copy and canonicalize the paths before processing the path name.
    WCHAR* pCanonicalSourceName = SafeGetCanonicalPathW (pSourceFileName);
    if (!pCanonicalSourceName) {
        return FALSE;
    }

    // Only retrieve the security object from the destination volume. If the
    // volumes are not the same, DeleteAndRenameFile will fail prior to 
    // checking access.
    WCHAR* pCanonicalDestName = SafeGetCanonicalPathW (pDestFileName);
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
#ifdef UNDER_CE
    HANDLE hSecurity;
    lResult = g_pMountTable->GetVolumeFromPath (pCanonicalDestName,
        &pSubDestName, &hDestVolume, &hSecurity);
#else
    lResult = g_pMountTable->GetVolumeFromPath (pCanonicalDestName,
        &pSubDestName, &hDestVolume);
#endif //UNDER_CE
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

    if ((L'\0' == *pSubSourceName) ||
        (L'\0' == *pSubDestName)) {
            
        // Empty string indicates the name of a mount point was passed in.
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
        g_pMountTable->IsReservedName (pCanonicalSourceName, wcslen (pCanonicalSourceName))) ||
        (IsPathInRootDir (pCanonicalDestName) && 
        g_pMountTable->IsReservedName (pCanonicalDestName, wcslen (pCanonicalDestName)))) {

        // Cannot delete or rename a reserved name.
        SetLastError (ERROR_ACCESS_DENIED);

#ifdef UNDER_NT
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
#else
    } else {

        g_pMountTable->SecurityManager.LockVolume (hSecurity);

        // Requires FILE_DELETE_CHILD permission to the parent directory of the
        // souce file.
        lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
            pSubSourceName, CE_FILE_OBJECT, GetCurrentToken (), 0, FILE_DELETE_CHILD);

        if (ERROR_SUCCESS == lResult) {

            // Requires DELETE permission to the destination file. Requires
            // FILE_DELETE_CHILD and FILE_ADD_FILE permission to the parent
            // directory of the destination file.
            lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
                pSubDestName, CE_FILE_OBJECT, GetCurrentToken (), DELETE,
                FILE_DELETE_CHILD | FILE_ADD_FILE);

            if (ERROR_SUCCESS == lResult) {

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
            } else {
                // Failed access check on the destination file.
                SetLastError (lResult);
            }
        } else {
            // Failed access check on the source file.
            SetLastError (lResult);
        }

        g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
        g_pMountTable->SecurityManager.Release (hSecurity);
    }
#endif //UNDER_NT
    
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

#ifdef UNDER_CE
    HANDLE hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags, &hSecurity);
#else
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags);
#endif //UNDER_CE
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

#ifdef UNDER_NT
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
#else
    g_pMountTable->SecurityManager.LockVolume (hSecurity);

    lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (
        hSecurity, pSubPathName, CE_FILE_OBJECT, GetCurrentToken (), 0,
        FILE_LIST_DIRECTORY);

    if (ERROR_SUCCESS == lResult) {

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

    } else {
        // Failed access check.
        SetLastError (lResult);
    }

    g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
    g_pMountTable->SecurityManager.Release (hSecurity);
#endif //UNDER_NT

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
    DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* pSecurityAttributes, 
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate)
{
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pPathName);
    if (!pCanonicalName) {
        return INVALID_HANDLE_VALUE;
    }

    if (IsLegacyDeviceName (pPathName)) {
        // For backwards compatibility, allow legacy devices to be opened
        // no matter what the creation flag by replacing with OPEN_EXISTING
        // before calling into device manager.
        Create = OPEN_EXISTING;
    }

    if (IsPathInRootDir (pCanonicalName) && 
        g_pMountTable->IsReservedName (pCanonicalName, wcslen (pCanonicalName)) &&
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

#ifdef UNDER_CE
    HACLVOL hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags, &hSecurity);
#else
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags);
#endif //UNDER_CE

    if (ERROR_SUCCESS == lResult) {

#ifdef UNDER_CE
        g_pMountTable->SecurityManager.LockVolume (hSecurity);
#endif //UNDER_CE

        HANDLE hInt = INVALID_HANDLE_VALUE;

        BOOL fPathInSystemDir = FALSE;
        
        if (AFS_FLAG_ROOTFS & MountFlags) {
            fPathInSystemDir = IsPathInSystemDir (pSubPathName);
        }

        // Don't let a any process other than the kernel call into a kernel-only volume.
        if ((AFS_FLAG_KMODE & MountFlags) &&
            (GetCurrentProcessId () != reinterpret_cast<DWORD> (hProcess))) {
            DEBUGMSG (ZONE_APIS, (L"FSDMGR!InternalCreateFileW: ACCESS DENIED: accessing kernel-only volume from user app=0x%x", hProcess));
            lResult = ERROR_ACCESS_DENIED;
        }

        if (ERROR_SUCCESS == lResult) {

            // Perform the file creation.

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
            if (fPathInSystemDir && (INVALID_FILE_ATTRIBUTES == Attributes)) {
                fDoROM = TRUE;
            }

            // We must explicitly handle the case where the caller specifies 
            // OPEN_ALWAYS and the file exists in ROM but is NOT already shadowed 
            // on the root file system. In this case, skip calling into the root 
            // fs, and open a handle to the file in ROM (because we want to open 
            // the exiting file, not create a new shadow copy of it).
            if (!fDoROM || !(OPEN_ALWAYS == Create) ||
                (!FileExistsInROM (pSubPathName))) {
                

                // When shadowing a file in ROM, persist the SYSTEM attribute
                if ((OPEN_EXISTING != Create) && 
                    fPathInSystemDir) {
                    
                    DWORD Attributes = ROM_GetFileAttributesW (pCanonicalName);
                    if (0xFFFFFFFF != Attributes) {
                        FlagsAndAttributes |= (FILE_ATTRIBUTE_SYSTEM & Attributes);
                    }
                }
                
                if (!(AFS_FLAG_NETWORK & MountFlags)) {
                    // Strip-off the GENERIC_EXECUTE bit on all file systems other
                    // than the network file system.
                    Access &= ~GENERIC_EXECUTE;
                }

                // Determine the appropriate access mask required to open the file in
                // the requested mode.
                DWORD AccessMask = 0;
                AccessMask |= (GENERIC_READ & Access) ? FILE_GENERIC_READ : 0;
                AccessMask |= (GENERIC_WRITE & Access) ? FILE_GENERIC_WRITE : 0;
                AccessMask |= (GENERIC_EXECUTE & Access) ? FILE_GENERIC_EXECUTE : 0;

                DWORD ParentAccessMask = 0;
                if ((INVALID_FILE_ATTRIBUTES == Attributes) &&
                    ((CREATE_NEW == Create) ||
                     (CREATE_ALWAYS == Create) ||
                     (OPEN_ALWAYS == Create)))
                {
                    // If creating a new file, must have add permission to the parent.
                    ParentAccessMask = FILE_ADD_FILE;
                }

#ifdef UNDER_NT
                //
                // Perform the CreateFile call on the file system.
                //
                SetLastError (ERROR_SUCCESS);
                __try {
                    h = AFS_CreateFileW (hVolume, hProcess, pSubPathName, Access,
                        ShareMode, NULL, Create, FlagsAndAttributes, hTemplate,
                        NULL, 0 );
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    SetLastError (ERROR_EXCEPTION_IN_SERVICE);
                }
#else
                PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;
                DWORD SecurityDescriptorSize = 0;

                lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
                    pSubPathName, CE_FILE_OBJECT, GetCurrentToken (), AccessMask,
                    ParentAccessMask, LOWORD (FlagsAndAttributes), pSecurityAttributes,
                    &pSecurityDescriptor, &SecurityDescriptorSize);

                if (ERROR_SUCCESS == lResult)
                {
                    
                    //
                    // Perform the CreateFile call on the file system.
                    //
                    SetLastError (ERROR_SUCCESS);
                    __try {
                        h = AFS_CreateFileW (hVolume, hProcess, pSubPathName, Access,
                            ShareMode, NULL, Create, FlagsAndAttributes, hTemplate,
                            pSecurityDescriptor, SecurityDescriptorSize );
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        SetLastError (ERROR_EXCEPTION_IN_SERVICE);
                    }
                }
                else
                {
                    // Failed VolumeAccessCheck.
                    SetLastError (lResult);
                }

                g_pMountTable->SecurityManager.FreeSecurityDescriptor (hSecurity, pSecurityAttributes,
                    pSecurityDescriptor);
#endif //UNDER_NT
            }

        } else {

            // Insufficient privilege.
            SetLastError (lResult);
            
            fDoROM = FALSE;
        }

#ifdef UNDER_CE
        g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
        g_pMountTable->SecurityManager.Release (hSecurity);
#endif //UNDER_CE

    } else {

        // Failed to get the volume associated with the path.
        SetLastError (lResult);
        
        if (IsPathInSystemDir (pCanonicalName)) {
            // Failed to enter the volume (root) for whatever reason, so always perform
            // a ROM search if this is the system directory.
            fDoROM = TRUE;
        }
    }

    // Handle the ROM case when in the virtual system directory:
    if (fDoROM && (INVALID_HANDLE_VALUE == h)) {
        
        // If we were unable to open the file at this point and if we've indicated
        // that ROM should be searched, then try to open the file in ROM.
        
        DWORD LastError = FsdGetLastError (ERROR_FILE_NOT_FOUND);
        
        // Try to open the file in ROM.
        h = ROM_CreateFileW (hProcess, pCanonicalName, Access, ShareMode,
            pSecurityAttributes, Create, FlagsAndAttributes, hTemplate);

        // If we didn't find the file in ROM, restore the previously set error code.
        if (INVALID_HANDLE_VALUE == h) {
            SetLastError (LastError);
        }
    }

    SafeFreeCanonicalPath (pCanonicalName);
    return h;        
}

EXTERN_C HANDLE FSINT_CreateFileW (const WCHAR* pPathName, 
    DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* pSecurityAttributes, 
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate)
{   
    HANDLE hProcess = (HANDLE)GetCurrentProcessId ();
    return InternalCreateFileW (pPathName, hProcess, Access, ShareMode,
        pSecurityAttributes, Create, FlagsAndAttributes, hTemplate);
}

EXTERN_C HANDLE FSEXT_CreateFileW (const WCHAR* pPathName, 
    DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* pSecurityAttributes, 
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate)
{
    // Make a local copy of the input SECURITY_ATTRIBUTES structure because
    // it contains an embedded pointer.
    SECURITY_ATTRIBUTES LocalSecurityAttributes;
    if (pSecurityAttributes && !CeSafeCopyMemory (&LocalSecurityAttributes,
            pSecurityAttributes, sizeof (SECURITY_ATTRIBUTES))) {
        SetLastError (ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }
    pSecurityAttributes = pSecurityAttributes ? &LocalSecurityAttributes : NULL;

    HANDLE hProcess = (HANDLE)GetCallerVMProcessId ();
    HANDLE h = InternalCreateFileW (pPathName, hProcess, Access, ShareMode, 
        pSecurityAttributes, Create, FlagsAndAttributes, hTemplate);

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
#ifdef UNDER_CE
    HACLVOL hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags, &hSecurity);
#else
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &MountFlags);
#endif //UNDER_CE
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

#ifdef UNDER_NT
    SetLastError (ERROR_SUCCESS);
    __try {
        fRet = AFS_FsIoControlW (hVolume, hProcess, Fsctl, pInBuf, InBufSize,
            pOutBuf, OutBufSize, pBytesReturned, pOverlapped);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError (ERROR_EXCEPTION_IN_SERVICE);
    }
#else
    // TODO: Add FSCTL privilege check.

    // Requires FILE_ALL_ACCESS permission to the root of the volume. This is
    // intentionally very restrictive since we're dealing with IOCTLs and we
    // don't know what they are right now.
    g_pMountTable->SecurityManager.LockVolume (hSecurity);
    lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
            L"\\", CE_DIRECTORY_OBJECT, GetCurrentToken (), FILE_ALL_ACCESS, 0);
    g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
    g_pMountTable->SecurityManager.Release (hSecurity);

    if (ERROR_SUCCESS == lResult) {
        SetLastError (ERROR_SUCCESS);
        __try {
            fRet = AFS_FsIoControlW (hVolume, hProcess, Fsctl, pInBuf, InBufSize,
                pOutBuf, OutBufSize, pBytesReturned, pOverlapped);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    } else {
        SetLastError (lResult);
        fRet = FALSE;
    }
#endif //UNDER_NT

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
#ifdef UNDER_CE
    HACLVOL hSecurity;
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume, &hSecurity);
#else
    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName, 
        &pSubPathName, &hVolume);
#endif //UNDER_CE
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        goto exit;
    }

#ifdef UNDER_NT
    SetLastError (ERROR_SUCCESS);
    __try {
        hRet = AFS_FindFirstChangeNotificationW (hVolume, hProc, pSubPathName, fWatchSubTree,
            NotifyFilter);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError (ERROR_EXCEPTION_IN_SERVICE);
    }

#else
    // Caller must have FILE_GENERIC_READ permission to the directory
    // being watched.
    g_pMountTable->SecurityManager.LockVolume (hSecurity);
    lResult = g_pMountTable->SecurityManager.VolumeAccessCheck (hSecurity,
        pSubPathName, CE_DIRECTORY_OBJECT, GetCurrentToken (), FILE_GENERIC_READ, 0);

    if (ERROR_SUCCESS == lResult) {

        SetLastError (ERROR_SUCCESS);
        __try {
            hRet = AFS_FindFirstChangeNotificationW (hVolume, hProc, pSubPathName, fWatchSubTree, 
                NotifyFilter);    
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    } else {
        // Failed access check on the directory.
        SetLastError (lResult);
    }
    g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
    g_pMountTable->SecurityManager.Release (hSecurity);
#endif //UNDER_NT

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

EXTERN_C BOOL FSINT_GetFileSecurityW (
    __in const WCHAR* pFileName,
    SECURITY_INFORMATION RequestedInformation,
    __out_bcount_opt (Length) PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length,
    __out_opt DWORD* pLengthNeeded
    )
{
#ifdef UNDER_NT

    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
#else
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pFileName);
    if (!pCanonicalName)
    {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubFileName;
    HACLVOL hSecurity;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubFileName, &hVolume, &hSecurity);
    if (ERROR_SUCCESS != lResult)
    {
        SetLastError (lResult);
        goto exit;
    }

    g_pMountTable->SecurityManager.LockVolume (hSecurity);

    // Access check for GetFileSecurity is performed inside the security
    // manager.
    lResult = g_pMountTable->SecurityManager.GetFileSecurity (hSecurity,
        GetCurrentToken (), pSubFileName, RequestedInformation,
        pSecurityDescriptor, Length, pLengthNeeded);

    g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
    g_pMountTable->SecurityManager.Release (hSecurity);

exit:
    SetLastError (lResult);
    return (NO_ERROR == lResult);
#endif //UNDER_NT
}

EXTERN_C BOOL FSEXT_GetFileSecurityW (
    __in const WCHAR* pFileName,
    SECURITY_INFORMATION RequestedInformation,
    __out_bcount_opt (Length) PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length,
    __out_opt DWORD* pLengthNeeded
    )
{
#ifdef UNDER_NT

    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
#else
    // Allocate a local buffer to receive the security descriptor. The contents
    // will be copied to the caller's buffer in CeFreeDuplicateBuffer, below.
    PSECURITY_DESCRIPTOR pSecurityDescriptorLocal = NULL;
    if (pSecurityDescriptor &&
        FAILED (CeAllocDuplicateBuffer(&pSecurityDescriptorLocal,
                    pSecurityDescriptor, Length, ARG_O_PTR)))
    {
        SetLastError (ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    // Use a local DWORD to receive the length needed. This will be copied to
    // the output DWORD in CeSafeCopyMemory, below.
    DWORD LengthNeededLocal = 0;

    // NOTE: No need to copy pFileName as it will be copied during canonicalization.
    BOOL fRet = FSINT_GetFileSecurityW (pFileName, RequestedInformation, pSecurityDescriptorLocal, Length,
        pLengthNeeded ? &LengthNeededLocal : NULL);

    // Copy local security descriptor to output parameter and free the local buffer.
    if (pSecurityDescriptor &&
        FAILED (CeFreeDuplicateBuffer (pSecurityDescriptorLocal, pSecurityDescriptor, Length, ARG_O_PTR)))
    {
        SetLastError (ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    }

    // Copy local lenght needed DWORD to output parameter.
    if (pLengthNeeded &&
        !CeSafeCopyMemory (pLengthNeeded, &LengthNeededLocal, sizeof (DWORD)))
    {
        SetLastError (ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    }

    return fRet;
#endif //UNDER_NT
}

EXTERN_C BOOL FSINT_SetFileSecurityW (
    __in const WCHAR* pFileName,
    SECURITY_INFORMATION SecurityInformation,
    __in_bcount (Length) PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length
    )
{
#ifdef UNDER_NT
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
#else
    // Copy and canonicalize the path before processing the path name.
    WCHAR* pCanonicalName = SafeGetCanonicalPathW (pFileName);
    if (!pCanonicalName)
    {
        return FALSE;
    }

    HANDLE hVolume;
    const WCHAR* pSubFileName;
    HACLVOL hSecurity;

    // Determine which volume owns this path and enter that volume.

    LRESULT lResult = g_pMountTable->GetVolumeFromPath (pCanonicalName,
        &pSubFileName, &hVolume, &hSecurity);
    if (ERROR_SUCCESS != lResult)
    {
        SetLastError (lResult);
        goto exit;
    }

    g_pMountTable->SecurityManager.LockVolume (hSecurity);

    // Access check for GetFileSecurity is performed inside the security
    // manager.
    lResult = g_pMountTable->SecurityManager.SetFileSecurity (hSecurity,
        GetCurrentToken (), pSubFileName, SecurityInformation,
        pSecurityDescriptor, Length);

    g_pMountTable->SecurityManager.UnlockVolume (hSecurity);
    g_pMountTable->SecurityManager.Release (hSecurity);

exit:
    SetLastError (lResult);
    return (NO_ERROR == lResult);
#endif //UNDER_NT 
}

EXTERN_C BOOL FSEXT_SetFileSecurityW (
    __in const WCHAR* pFileName,
    SECURITY_INFORMATION SecurityInformation,
    __in_bcount (Length) PSECURITY_DESCRIPTOR pSecurityDescriptor,
    DWORD Length
    )
{
#ifdef UNDER_NT
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
#else
    // Make a local heap copy of the input security descriptor so it cannot
    // be modified asynchronously.
    PSECURITY_DESCRIPTOR pSecurityDescriptorLocal = NULL;
    if (FAILED (CeAllocDuplicateBuffer(&pSecurityDescriptorLocal,
        pSecurityDescriptor, Length, ARG_I_PTR)))
    {
        SetLastError (ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    // NOTE: No need to copy pFileName as it will be copied during canonicalization.

    BOOL fRet = FSINT_SetFileSecurityW (pFileName, SecurityInformation,
        pSecurityDescriptorLocal, Length);

    // Free the duplicate security descriptor local copy.
    VERIFY (SUCCEEDED (CeFreeDuplicateBuffer (pSecurityDescriptorLocal, 
        pSecurityDescriptor, Length, ARG_I_PTR)));

    return fRet;
#endif //UNDER_NT
}
