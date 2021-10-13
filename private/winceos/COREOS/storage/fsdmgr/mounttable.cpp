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
#include <intsafe.h>

MountTable_t::MountTable_t() :
    m_UNCVolumeIndex (INVALID_MOUNT_INDEX),
    m_RootVolumeIndex (INVALID_MOUNT_INDEX),
    m_BootVolumeIndex (INVALID_MOUNT_INDEX),
    m_HighVolumeIndex (INVALID_MOUNT_INDEX),
    m_pROMVolumeList (NULL),
    m_hRootNotifyVolume (NULL),
    m_PnPWaitIODelay (g_dwWaitIODelay),
    m_pFnNotification (NULL)

{
    ::InitializeCriticalSection (&m_cs);
    ::memset (m_Table, 0, sizeof (m_Table));
}

MountTable_t::~MountTable_t()
{
    // We don't really ever expect the global mount table to be destroyed.
    DEBUGCHK (0); 
    ::DeleteCriticalSection (&m_cs);
}

BOOL MountTable_t::IsValidFileName(const WCHAR* pFile, size_t FileChars)
{
    if (FileChars >= MAX_PATH) {
        return FALSE;
    }

    if (FileChars == 0) {
        return FALSE;
    }

    while (FileChars--) {
        if ((*pFile < 32) || (*pFile== '*') || (*pFile == '\\') || (*pFile == '/')
            || (*pFile == '?') || (*pFile == '>') || (*pFile == '<') || (*pFile == ':')
            || (*pFile == '"') || (*pFile == '|'))
            return FALSE;
        pFile ++;
    }

    return TRUE;
}

LRESULT MountTable_t::GetMountName (int Index,
    __out_ecount(MountNameChars) WCHAR* pMountName,
    size_t MountNameChars)
{
    if (!IsValidIndex (Index)) {
        return ERROR_INVALID_INDEX;
    }

    LRESULT lResult = ERROR_FILE_NOT_FOUND;

    LockTable ();

    if (m_Table[Index]) {

        if (MountNameChars > m_Table[Index]->NameChars) {

            // Copy the string to the output buffer.
            ::StringCchCopyN (pMountName, MountNameChars, 
                m_Table[Index]->MountName, m_Table[Index]->NameChars);

            lResult = ERROR_SUCCESS;
            
        } else {
            // The output buffer provided is not large enough
            // to contain the entire mount name.
            lResult = ERROR_INSUFFICIENT_BUFFER;
        }
    }

    UnlockTable ();

    return lResult;
}

BOOL MountTable_t::IsReservedName (const WCHAR* pPath, size_t PathChars)
{    
    // Skip the 1st slash
    if (L'\\' == *pPath) {
        pPath ++;
        PathChars --;
    }

    // First, compare with the system directory.
    if (ComparePaths (SYSDIRNAME, SYSDIRLEN, pPath, PathChars)) {
        // Path matches \Windows, so this name is reserved.
        return TRUE;
    }

    BOOL fReserved = FALSE;

    LockTable ();
    for (int i = 0; i <= m_HighVolumeIndex; i ++) {
        
        if (!m_Table[i]) {
            // No volume mounted at this index.
            continue;
        }
        
        if (ComparePaths (m_Table[i]->MountName, m_Table[i]->NameChars, 
                    pPath, PathChars)) {
            // Paths match, this name is reserved.
            fReserved = TRUE;
            break;
        }
    }
    UnlockTable ();

    return fReserved;
}

LRESULT MountTable_t::GetVolumeFromIndex (int MountIndex, HANDLE* phVolume,
    DWORD* pMountFlags)
{
    if (!IsValidIndex (MountIndex)) {
        return ERROR_INVALID_INDEX;
    }

    LRESULT lResult = ERROR_PATH_NOT_FOUND;
    
    LockTable ();

    if (m_Table[MountIndex] && m_Table[MountIndex]->hVolume) {
        
        *phVolume = m_Table[MountIndex]->hVolume;
        if (pMountFlags) {
            *pMountFlags = m_Table[MountIndex]->MountFlags;
        }
        lResult = ERROR_SUCCESS;
    }

    UnlockTable ();

    return lResult;
}

LRESULT MountTable_t::GetVolumeFromPath (const WCHAR* pPath, __out const WCHAR** ppFileName,
    HANDLE* phVolume, DWORD* pMountFlags)
{
    LRESULT lResult = ERROR_PATH_NOT_FOUND;
    static const WCHAR* ROOT_DIRECTORY = L"\\";

    // Paths passed to GetVolumeFromPath should already be canonical!
    DEBUGCHK (L'\\' == pPath[0]);

    LockTable ();

    // First, check for \\ prefix indicating that this is a UNC path.
    if ((L'\\' == pPath[0]) && 
        (L'\\' == pPath[1])) {

        // Handle UNC path.
        if (INVALID_MOUNT_INDEX == m_UNCVolumeIndex) {
            // For legacy reasons, set ERROR_NO_NETWORK when the UNC handler
            // (network redirector) is not present.
            lResult = ERROR_NO_NETWORK;

        } else {
            // UNC file system is present, preserve the entire UNC name 
            // including the \\ prefix.
            *ppFileName = pPath;
            *phVolume = m_Table[m_UNCVolumeIndex]->hVolume;
            if (pMountFlags) {
                *pMountFlags = m_Table[m_UNCVolumeIndex]->MountFlags;
            }
            lResult = ERROR_SUCCESS;
        }

    } else {

        // Not a UNC path, so parse the first token from the path string.
        
        const WCHAR* pRootTokenStart = pPath;
        
        // Path token starts after the first backslash.
        if (L'\\' == *pRootTokenStart) {
            pRootTokenStart ++;
        }

        // Path token ends at the second backslash or end of string.
        const WCHAR* pRootTokenEnd = pRootTokenStart;
        while ((L'\0' != *pRootTokenEnd) && (L'\\' != *pRootTokenEnd)) {
            pRootTokenEnd ++;
        }

        DWORD TokenChars = pRootTokenEnd - pRootTokenStart;

        if (TokenChars != 0) {

            // Enumerate all mounted volumes checking for a path match.
            for (int i = 0; i <= m_HighVolumeIndex; i ++) {

                if (!m_Table[i] || !m_Table[i]->hVolume) {
                    // No volume mounted at this index.
                    continue;
                }

                if (ComparePaths (m_Table[i]->MountName, m_Table[i]->NameChars,
                            pRootTokenStart, TokenChars)) {

                    // Paths match, found the proper volume. Return the
                    // remainder of the path string to the caller.
                    if (*pRootTokenEnd) {
                        *ppFileName = pRootTokenEnd;
                    } else {
                        // Caller only passed the mount name, so return
                        // backslash instead to prevent passing an empty
                        // string file name to a file system.
                        *ppFileName = ROOT_DIRECTORY;
                    }
                    *phVolume = m_Table[i]->hVolume;
                    if (pMountFlags) {
                        *pMountFlags = m_Table[i]->MountFlags;
                    }

                    lResult = ERROR_SUCCESS;
                    break;
                }
            }
        }

        if (ERROR_SUCCESS != lResult) {

            // Use the root file system, if present.
            if (INVALID_MOUNT_INDEX == m_RootVolumeIndex) {
                
                // No root file system volume is present.
                lResult = ERROR_PATH_NOT_FOUND;

            } else {

                // Defaults to the root file system. Return the original
                // path to the caller.
                *ppFileName = pPath;
                *phVolume = m_Table[m_RootVolumeIndex]->hVolume;
                if (pMountFlags) {
                    *pMountFlags = m_Table[m_RootVolumeIndex]->MountFlags;
                }
                lResult = ERROR_SUCCESS;
            }
        }
    }

    UnlockTable ();

    return lResult;
}

LRESULT MountTable_t::NotifyMountComplete (int Index, HANDLE hNotifyHandle)
{
    LRESULT lResult = ERROR_SUCCESS;
    
    WCHAR MountFolder[MAX_PATH];
    HANDLE hNotify = NULL;
    DWORD MountFlags = 0;

    LockTable ();
    
    if (!m_Table[Index] || !m_Table[Index]->hVolume) {

        // There is no volume registered at this index. Should never happen.
        DEBUGCHK (0);
        lResult = ERROR_FILE_NOT_FOUND;

    } else {

        MountFlags = m_Table[Index]->MountFlags;

        ::StringCchCopy (MountFolder, MAX_PATH, L"\\");
        ::StringCchCatN (MountFolder, MAX_PATH, m_Table[Index]->MountName,
            m_Table[Index]->NameChars);
        hNotify = m_hRootNotifyVolume;

        if (AFS_FLAG_ROOTFS & MountFlags) {
            // Store the root file system's notification handle for future
            // notifications.
            DEBUGCHK (!m_hRootNotifyVolume);
            m_hRootNotifyVolume = hNotifyHandle;
        }

    }

    UnlockTable ();

    if (hNotify && !(AFS_FLAG_HIDDEN & MountFlags)) {
        
        // Perform notification on the root notification handle that this 
        // volume has appeared.
        DEBUGCHK (ERROR_SUCCESS == lResult);

        if (m_pFnNotification) {

            // Perform call-back notification that the volume has been attached.
            FILECHANGEINFO NotifyInfo;
            NotifyInfo.cbSize = sizeof(NotifyInfo);
            NotifyInfo.uFlags = SHCNF_PATH | SHCNF_FLUSHNOWAIT;
            NotifyInfo.dwItem1 = (DWORD)MountFolder;
            NotifyInfo.wEventId = SHCNE_DRIVEADD;
            NotifyInfo.dwAttributes = FILE_ATTRIBUTE_DIRECTORY;
            if (!(AFS_FLAG_PERMANENT & MountFlags)) {
                NotifyInfo.dwAttributes |= FILE_ATTRIBUTE_TEMPORARY;
            }
            NotifyInfo.ftModified.dwLowDateTime = 0;
            NotifyInfo.ftModified.dwHighDateTime = 0;
            NotifyInfo.nFileSize = 0;

            __try {
                (m_pFnNotification) (&NotifyInfo);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        ::NotifyPathChange (hNotify, MountFolder, TRUE, FILE_ACTION_ADDED);
    }

    return lResult;
}


// Internal helper function for RegisterAFSEx and FSDMGR_RegisterVolume.
LRESULT MountTable_t::RegisterVolume (int Index, HANDLE hAPISet, DWORD VolumeContext, 
        HANDLE hOwnerProcess, DWORD MountFlags)
{
    if (!IsValidIndex (Index)) {
        return ERROR_INVALID_INDEX;
    }
    
    LRESULT lRet = ERROR_SUCCESS;
    HANDLE hVolume = NULL;    

    LockTable ();    

    if (!m_Table[Index]) {
        // No name registered at this index, don't allow a volume to be
        // registered here until a name is present.
        lRet = ERROR_FILE_NOT_FOUND;

    } else if (m_Table[Index]->hVolume) {
        // There is already a volume registered at this index.
        lRet = ERROR_ALREADY_EXISTS;

    } else if (m_Table[Index]->hOwnerProcess != hOwnerProcess) {
        // The name at this index was registered by a different process so
        // disallow the volume registration.
        lRet = ERROR_ACCESS_DENIED;
        
    } else {

        if (MountFlags != (VALID_MOUNT_FLAGS & MountFlags)) {
            // Mask-off any not supported MountFlags values.
            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!RegisterVolume: Invalid MountFlags ignored: 0x%x.\r\n",
                (MountFlags ^ (VALID_MOUNT_FLAGS & MountFlags))));
            MountFlags &= VALID_MOUNT_FLAGS;
        }
    
        if ((AFS_ROOTNUM_NETWORK == Index) &&
            (INVALID_MOUNT_INDEX == m_UNCVolumeIndex)) {
            // Any volume registering at the network index automatically
            // gets the network flag.
            MountFlags |= AFS_FLAG_NETWORK;
        }

        // There can be only one root volume mounted at a time.
        if ((AFS_FLAG_ROOTFS & MountFlags) && 
            (INVALID_MOUNT_INDEX != m_RootVolumeIndex)) {
            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!RegisterVolume: WARNING: AFS_FLAG_ROOTFS MountFlag ignored; " 
                L"a root file system is already mounted."));
            MountFlags &= ~AFS_FLAG_ROOTFS;         
        }

        if (AFS_FLAG_ROOTFS & MountFlags) {
            // The root file system cannot mount as ROM or be hidden; remove these
            // mount flags so they are ignored on the ROM file system.
            DEBUGMSG (ZONE_ERRORS && (AFS_FLAG_MOUNTROM & MountFlags), 
                (L"FSDMGR!RegisterVolume: WARNING: AFS_FLAG_MOUNTROM MountFlag ignored on root file system."));
            DEBUGMSG (ZONE_ERRORS && (AFS_FLAG_HIDDEN & MountFlags), 
                (L"FSDMGR!RegisterVolume: WARNING: AFS_FLAG_HIDDEN MountFlag ignored on root file system."));
            MountFlags &= ~(AFS_FLAG_MOUNTROM | AFS_FLAG_HIDDEN);
        }

        // There can be only one bootable volume mounted at a time.
        if ((AFS_FLAG_BOOTABLE & MountFlags) && 
            (INVALID_MOUNT_INDEX != m_BootVolumeIndex)) {
            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!RegisterVolume: WARNING: AFS_FLAG_BOOTABLE MountFlag ignored; "
                L"a boot file system is already mounted."));
            MountFlags &= ~AFS_FLAG_BOOTABLE;
        }

        // There can be only one network volume mounted at a time.
        if ((AFS_FLAG_NETWORK & MountFlags) &&
            (INVALID_MOUNT_INDEX != m_UNCVolumeIndex)) {
            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!RegisterVolume: WARNING: AFS_FLAG_NETWORK MountFlag ignored; "
                L"a Network file system is already mounted."));
            MountFlags &= ~AFS_FLAG_NETWORK;
        }

        if (AFS_FLAG_HIDEROM & MountFlags) {
            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!RegisterVolume: WARNING: AFS_FLAG_HIDEROM MountFlag ignored; "
                L"not supported."));
            MountFlags &= ~AFS_FLAG_HIDEROM;
        }

        // NOTENOTE: There is no limit on the number of AFS_FLAG_MOUNTROM volumes.
        
        // Create a handle to the volume using the provided API set and context.
        // This handle is owned by the kernel and will never be returned to the caller.

        hVolume = CreateAPIHandle(hAPISet, reinterpret_cast<void*> (VolumeContext));

        if (hVolume) {
        
            // Store the volume value at the specified index.

            m_Table[Index]->MountFlags = MountFlags;
            m_Table[Index]->hVolume = hVolume;
            m_Table[Index]->VolumeContext = VolumeContext;

            if (AFS_FLAG_ROOTFS & MountFlags) { 
                // This volume is the root file system.
                DEBUGCHK (INVALID_MOUNT_INDEX == m_RootVolumeIndex);
                m_RootVolumeIndex = Index;
                
                // The folder name of the root file system is the empty string.
                // Truncate the existing string.
                m_Table[Index]->NameChars = 0;
                m_Table[Index]->MountName[0] = L'\0';
            }

        } else {
        
            lRet = ::FsdGetLastError ();
        }
    }

    WCHAR MountFolder[MAX_PATH];
    HANDLE hNotify = NULL;

    if (ERROR_SUCCESS == lRet) {

        if (!(AFS_FLAG_HIDDEN & MountFlags) && m_hRootNotifyVolume) {

            // Broadcast on the root notification handle that this new mount
            // point has appeared. Delay the notification until we have left 
            // the table critical section.
            MountFolder[0] = L'\\';
            ::StringCchCatN (MountFolder, MAX_PATH, m_Table[Index]->MountName, 
                m_Table[Index]->NameChars);
            hNotify = m_hRootNotifyVolume;
        }

        if (AFS_FLAG_BOOTABLE & MountFlags) {
            // This volume is the boot file system.
            DEBUGCHK (INVALID_MOUNT_INDEX == m_BootVolumeIndex);
            m_BootVolumeIndex = Index;            
        }

        if (AFS_FLAG_NETWORK & MountFlags) {
            // This volume is the network file system.
            DEBUGCHK (INVALID_MOUNT_INDEX == m_UNCVolumeIndex);
            m_UNCVolumeIndex = Index;            
        }

        if (AFS_FLAG_MOUNTROM & MountFlags) {
            // This volume is a ROM file system.
            ROMVolumeListNode* pNode = new ROMVolumeListNode;
            if (pNode) {
                // Save the volume handle associated with this volume.
                pNode->hVolume = m_Table[Index]->hVolume;

                // Link this volume at the start of the list. These
                // entries will never be removed even if the volume is
                // later dismounted. We don't typically expect ROM 
                // volumes to ever be dismounted.
                pNode->pNext = m_pROMVolumeList;
                m_pROMVolumeList = pNode;
            }
        }        
        
        DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountTable_t::RegisterVolume: Registered volume at index %u (Name=\"%s\", MountFlags=0x%x)",
            Index, m_Table[Index]->MountName, MountFlags));
    }
    
    UnlockTable ();

    if (hVolume && (ERROR_SUCCESS != lRet)) {
#ifdef UNDER_CE
        ::CloseHandle (hVolume);
#else
        ::FSDMGR_PreClose (reinterpret_cast<MountedVolume_t*>(hVolume));
        ::FSDMGR_Close (reinterpret_cast<MountedVolume_t*>(hVolume));
#endif
        hVolume = NULL;
    }

    if (m_pFnNotification && hVolume) {
        // Register the notificaion callback function with the new volume.
        AFS_RegisterFileSystemFunction (hVolume, m_pFnNotification);
    }

    return lRet;
}

LRESULT MountTable_t::ReserveVolumeName (const WCHAR* pMountName, int Index, 
    HANDLE hOwnerProcess)
{
    if (!IsValidIndex (Index)) {
        // Bad index specified.
        return ERROR_INVALID_PARAMETER;
    }
    
    size_t MountNameChars;
    if (FAILED (::StringCchLength (pMountName, MAX_PATH, &MountNameChars)) ||
            !IsValidFileName (pMountName, MountNameChars)) {
        // Bad name specified.
        return ERROR_INVALID_PARAMETER;
    }

    // Does a file with this name already exist on the root file system?
    HANDLE hVolume;
    LRESULT lResult;
    DWORD Attributes = INVALID_FILE_ATTRIBUTES;
    lResult = GetVolumeFromIndex (m_RootVolumeIndex, &hVolume);
    if (ERROR_SUCCESS == lResult) {
        __try {
            Attributes = AFS_GetFileAttributesW (hVolume, pMountName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }
    if (INVALID_FILE_ATTRIBUTES != Attributes) {
        return ERROR_ALREADY_EXISTS;
    }

    // Alloc a new mount info structure up front. This will be freed later
    // if we fail.
    PREFAST_ASSUME(MountNameChars < MAX_PATH);
    DWORD MountInfoSize = sizeof (MountInfo) + (sizeof (WCHAR) * MountNameChars);
    MountInfo* pMountInfo = reinterpret_cast<MountInfo*> (new BYTE[MountInfoSize]);
    if (!pMountInfo) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pMountInfo->hVolume = NULL;
    pMountInfo->VolumeContext = 0;
    pMountInfo->MountFlags = 0;

    // Copy the supplied name into the structure.
    pMountInfo->NameChars = MountNameChars;
    VERIFY (SUCCEEDED (::StringCchCopyNW (pMountInfo->MountName, MountNameChars + 1, 
        pMountName, MountNameChars)));

    // Copy the owner process id into the structure so we can prevent other
    // processes from using this reserved slot and so we can keep other
    // processes from deregistering the volume.
    pMountInfo->hOwnerProcess = hOwnerProcess;

    LockTable ();

    if (!m_Table[Index]) {

        m_Table[Index] = pMountInfo;
        lResult = ERROR_SUCCESS;

    } else {
    
        lResult = ERROR_ALREADY_ASSIGNED;
    }

    UnlockTable ();

    if (ERROR_SUCCESS != lResult) {
        delete[] (BYTE*)pMountInfo;
    }

    return lResult;
}

// Internal helper function for RegisterAFSName and FSDMGR_RegisterVolume.
LRESULT MountTable_t::RegisterVolumeName (const WCHAR* pMountName, int* pIndex, 
        HANDLE hOwnerProcess)
{
    int ExistingIndex = INVALID_MOUNT_INDEX;

    size_t MountNameChars;
    if (FAILED (::StringCchLength (pMountName, MAX_PATH, &MountNameChars)) ||
            !IsValidFileName (pMountName, MountNameChars)) {
        // Bad name specified.
        *pIndex = INVALID_MOUNT_INDEX;
        return ERROR_INVALID_PARAMETER;
    }

    // Does a file with this name already exist on the root file system?
    HANDLE hVolume;
    LRESULT lResult;
    DWORD Attributes = INVALID_FILE_ATTRIBUTES;
    lResult = GetVolumeFromIndex (m_RootVolumeIndex, &hVolume);
    if (ERROR_SUCCESS == lResult) {
        __try {
            Attributes = AFS_GetFileAttributesW (hVolume, pMountName);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_EXCEPTION_IN_SERVICE);
        }
    }
    if (INVALID_FILE_ATTRIBUTES != Attributes) {
        *pIndex = INVALID_MOUNT_INDEX;
        return ERROR_ALREADY_EXISTS;
    }

    // Alloc a new mount info structure up front. This will be freed later
    // if we fail.
    DWORD MountInfoSize = 0;
    // MountInfoSize = sizeof (WCHAR) * MountNameChars;
    // MountInfoSize += sizeof (MountInfo);
    if (FAILED (DWordMult (sizeof(WCHAR), MountNameChars, &MountInfoSize))
        || FAILED (DWordAdd (sizeof(MountInfo), MountInfoSize, &MountInfoSize))) {
        return ERROR_ARITHMETIC_OVERFLOW;
    }
    
    MountInfo* pMountInfo = reinterpret_cast<MountInfo*> (new BYTE[MountInfoSize]);
    if (!pMountInfo) {
        *pIndex = INVALID_MOUNT_INDEX;
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    pMountInfo->hVolume = NULL;
    pMountInfo->VolumeContext = 0;
    pMountInfo->MountFlags = 0;

    // Copy the supplied name into the structure.
    pMountInfo->NameChars = MountNameChars;
    VERIFY (SUCCEEDED (::StringCchCopyNW (pMountInfo->MountName, MountNameChars + 1, 
        pMountName, MountNameChars)));

    // Copy the owner process id into the structure so we can prevent other
    // processes from using this reserved slot and so we can keep other
    // processes from deregistering the volume.
    pMountInfo->hOwnerProcess = hOwnerProcess;

    lResult = ERROR_NO_MORE_DEVICES;
    
    LockTable ();
    
    // Search all entries in the mount table to locate a free slot locate an
    // existing entry for this name if it already exists.
    int FirstFreeIndex = INVALID_MOUNT_INDEX;
    for (int index = 0; index < MAX_MOUNTED_VOLUMES; index ++) {
        
        MountInfo* pThisInfo = m_Table[index];
        if (!pThisInfo && (INVALID_MOUNT_INDEX == FirstFreeIndex) &&
            (index >= FIRST_USER_INDEX)) {
            // This is the first free table slot. We will use it if we find
            // no duplicate names.
            FirstFreeIndex = index;
        }

        // Check to see if this name already exists in the mount list. If
        // so, output the existing index but return ERROR_ALREADY_EXISTS 
        // for the error code.
        if (pThisInfo && ComparePaths (pThisInfo->MountName, pThisInfo->NameChars,
                pMountInfo->MountName, pMountInfo->NameChars)) {
            // Found a duplicate name, abort the search.
            FirstFreeIndex = INVALID_MOUNT_INDEX;
            lResult = ERROR_ALREADY_EXISTS;
            ExistingIndex = index;
            break;
        }
    }

    if (INVALID_MOUNT_INDEX != FirstFreeIndex) {

        // Found a free index and no duplicate name.        
        m_Table[FirstFreeIndex] = pMountInfo;
        if (FirstFreeIndex > m_HighVolumeIndex) {
            // Adjust the talbe high water mark.
            m_HighVolumeIndex = FirstFreeIndex;
        }
        
        // This name now exists at the free index we found during
        // the above loop.
        ExistingIndex = FirstFreeIndex;
        lResult = ERROR_SUCCESS;
    }

    UnlockTable ();

    if (INVALID_MOUNT_INDEX == FirstFreeIndex) {
        // Did not use the allocated mount info structure because
        // there were either no free slots or the name already
        // existed in the table.
        delete[] reinterpret_cast<BYTE*> (pMountInfo);
        pMountInfo = NULL;
    }

    DEBUGMSG (ZONE_INIT && (ERROR_SUCCESS == lResult), 
        (L"FSDMGR!MountTable_t::RegisterVolumeName: Registered \"%s\" at index %u",
        pMountName, ExistingIndex));

    *pIndex = ExistingIndex;
    return lResult;
}

LRESULT MountTable_t::DeregisterVolume (int Index, HANDLE hCallerProcess,
    BOOL fForce)
{
    if (!IsValidIndex (Index)) {
        return ERROR_INVALID_INDEX;
    }

    LRESULT lResult = ERROR_INVALID_INDEX;

    WCHAR MountFolder[MAX_PATH];
    HANDLE hNotify = NULL;
    HANDLE hVolume = NULL;
    DWORD MountFlags = 0;

    LockTable ();

    if (m_Table[Index] && m_Table[Index]->hVolume) {

        if (m_Table[Index]->hOwnerProcess != hCallerProcess) {

            // The caller is not the same process that registered this
            // volume to begin with, so it cannot deregister it.
            lResult = ERROR_ACCESS_DENIED;

        } else if (!fForce && 
            (AFS_FLAG_PERMANENT & m_Table[Index]->MountFlags)) {

            // Can never deregister a permanent volume once is has been
            // registered unless forced to do so by the owner process exiting.
            lResult = ERROR_ACCESS_DENIED;
            
        } else {

            MountFlags = m_Table[Index]->MountFlags;
            hVolume = m_Table[Index]->hVolume;

            if (AFS_FLAG_ROOTFS & MountFlags) {
                m_hRootNotifyVolume = NULL;
                m_RootVolumeIndex = INVALID_MOUNT_INDEX;
                
            } else if (!(AFS_FLAG_HIDDEN & MountFlags)) {
            
                // Broadcast on the root notification handle that this mount point
                // point has disappered. Delay the notification until we have left 
                // the table critical section.
                ::StringCchCopyN (MountFolder, MAX_PATH, m_Table[Index]->MountName, 
                    m_Table[Index]->NameChars);
                hNotify = m_hRootNotifyVolume;
            }

            if (AFS_FLAG_BOOTABLE & MountFlags) {
                m_BootVolumeIndex = INVALID_MOUNT_INDEX;
            }
            
            if (AFS_FLAG_NETWORK & MountFlags) {
                m_UNCVolumeIndex = INVALID_MOUNT_INDEX;
            }

            if (AFS_FLAG_MOUNTROM & MountFlags) {
                
                // NOTE: We don't actually remove this volume from the list, we
                // simply invalidate the node. It is possible someone references it
                // and needs the next pointer. We don't expect there to be ROM
                // file systems that are frequently mounted or unmounted, so this
                // shouldn't lead to any significant problems.
                //
                ROMVolumeListNode* pROM = GetROMVolumeList ();
                while (pROM) {
                    if (pROM->hVolume == hVolume) {
                        // Invalidate the handle on this node.
                        pROM->hVolume = INVALID_HANDLE_VALUE;
                        break; // This volume will only be in the list once.
                    }
                    pROM = pROM->pNext;
                }
            }

            DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountTable_t::DeregisterVolume: Deregistered volume at index %u (Name=\"%s\", MountFlags=0x%x)",
                Index, m_Table[Index]->MountName, MountFlags));

            // Remove this volume from the table.
            m_Table[Index]->hVolume = NULL;
            m_Table[Index]->MountFlags = 0;
            m_Table[Index]->VolumeContext = 0;
            lResult = ERROR_SUCCESS;
        }
    }

    UnlockTable ();

    if (hVolume) {

#ifdef UNDER_CE
        // Close the volume handle. This will call AFS_PreUnmount and AFS_Unmount AFS_Unmount.
        VERIFY (::CloseHandle (hVolume));
#else
        // Under NT we're not using true handles to we have to call the volume
        // mount and unmount functions directly.
        VERIFY (::FSDMGR_PreClose (reinterpret_cast<MountedVolume_t*> (hVolume)));
        VERIFY (::FSDMGR_Close (reinterpret_cast<MountedVolume_t*> (hVolume)));
#endif

    }

    if (hNotify) {
        // Perform file notification on the root notification handle that this 
        // volume's folder has disappeared.        
        DEBUGCHK (ERROR_SUCCESS == lResult);
        DEBUGCHK (!(AFS_FLAG_HIDDEN & MountFlags));

        if (m_pFnNotification) {

            // Perform call-back notification that the volume has been removed.
            FILECHANGEINFO NotifyInfo;
            NotifyInfo.cbSize = sizeof(NotifyInfo);
            NotifyInfo.uFlags = SHCNF_PATH | SHCNF_FLUSHNOWAIT;
            NotifyInfo.dwItem1 = (DWORD)MountFolder;
            NotifyInfo.wEventId = SHCNE_DRIVEREMOVED;

            __try {
                (m_pFnNotification) (&NotifyInfo);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
        
        ::NotifyPathChange (hNotify, MountFolder, TRUE, FILE_ACTION_REMOVED);
    }

    return lResult;
}

LRESULT MountTable_t::DeregisterVolumeName (int Index, HANDLE hCallerProcess)
{
    if (!IsValidIndex (Index)) {
        return ERROR_INVALID_INDEX;
    }

    LRESULT lResult = ERROR_INVALID_INDEX;

    LockTable ();

    if (m_Table[Index] && m_Table[Index]->hVolume) {
        
        // The volume must be deregistered first.
        lResult = ERROR_ACCESS_DENIED;

    } else if (m_Table[Index] && (m_Table[Index]->hOwnerProcess != hCallerProcess)) {

        // The caller process is not the same as the process that originally
        // registered this volume name, so don't allow it to deregister it.
        lResult = ERROR_ACCESS_DENIED;

    } else if (m_Table[Index]) {

        DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountTable_t::DeregisterVolumeName: Deregistered \"%s\" at index %u",
            m_Table[Index]->MountName, Index));
        
        delete[] reinterpret_cast<BYTE*> (m_Table[Index]);
        m_Table[Index] = NULL;

        if (Index == m_HighVolumeIndex) {

            // Drop the index high water mark down to the next highest
            // used index.
            while (!m_Table[m_HighVolumeIndex] && (m_HighVolumeIndex >= 0)) {
                m_HighVolumeIndex --;
            }            
        }
        
        lResult = ERROR_SUCCESS;
    }

    UnlockTable ();

    return lResult;
}

void MountTable_t::GetMountSettings(HKEY hKey, DWORD* pMountFlags)
{
    DWORD MountFlags = 0;
    
    if (::FsdGetRegistryValue (hKey, ::g_szMOUNT_FLAGS_STRING, &MountFlags)) {

        // If there is a "MountFlags" value in the registry, it overrides
        // all existing flags.
        *pMountFlags = MountFlags;
        
    } else {

        // If there is no "MountFlags" value, combine all additional flags
        // with the existing flags.
        MountFlags = *pMountFlags;
    }
    
    ::FsdLoadFlag (hKey, ::g_szMOUNT_FLAG_MOUNTASBOOT_STRING, &MountFlags, AFS_FLAG_BOOTABLE);
    ::FsdLoadFlag (hKey, ::g_szMOUNT_FLAG_MOUNTASROM_STRING, &MountFlags, AFS_FLAG_MOUNTROM);
    ::FsdLoadFlag (hKey, ::g_szMOUNT_FLAG_MOUNTHIDDEN_STRING, &MountFlags, AFS_FLAG_HIDDEN);
    ::FsdLoadFlag (hKey, ::g_szMOUNT_FLAG_MOUNTASROOT_STRING, &MountFlags, AFS_FLAG_ROOTFS);
    ::FsdLoadFlag (hKey, ::g_szMOUNT_FLAG_MOUNTSYSTEM_STRING, &MountFlags, AFS_FLAG_SYSTEM);
    ::FsdLoadFlag (hKey, ::g_szMOUNT_FLAG_MOUNTPERMANENT_STRING, &MountFlags, AFS_FLAG_PERMANENT);
    ::FsdLoadFlag (hKey, ::g_szMOUNT_FLAG_MOUNTASNETWORK_STRING, &MountFlags, AFS_FLAG_NETWORK);

    // Output the new mount flags. This is a combination between the old and new
    // flags because we started with MountFlags = *pMountFlags, above.
    *pMountFlags = MountFlags;
}

void MountTable_t::RegisterFileSystemNotificationFunction (SHELLFILECHANGEFUNC_t pFn)
{
    LockTable ();
    m_pFnNotification = pFn;
    UnlockTable ();

    // Update all currently mounted file systems.
    for (int i = 0; i <= m_HighVolumeIndex; i ++) {

        HANDLE hVolume;
        LRESULT lResult = GetVolumeFromIndex (i, &hVolume);
        if (ERROR_SUCCESS != lResult) {
            continue;
        }

        __try {
            AFS_RegisterFileSystemFunction (hVolume, pFn);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
}

LRESULT MountTable_t::InstallDeferredFilters()
{
    LRESULT lResult = NO_ERROR;

    // Update all currently mounted file systems.
    for (int i = 0; i <= m_HighVolumeIndex; i ++) {

        HANDLE hVolume = NULL;
        if(NO_ERROR != GetVolumeFromIndex (i, &hVolume)) {
            // No volume at this index.
            continue;
        }

        // Lock the volume handle against the FSDMGR volume API set. This call
        // will fail for non-FSD volumes like $device and $bus. We can only 
        // install filters on FSD volumes.
        MountedVolume_t* pVolume = NULL;
        HANDLE hLock = LockAPIHandle(hVolumeAPI, 
                            reinterpret_cast<HANDLE>(GetCurrentProcessId()),
                            hVolume, reinterpret_cast<LPVOID*>(&pVolume));
        if(!hLock) {
            // Not an FSDMGR volume, or the volume has gone away.
            continue;
        }

        __try {
            lResult = pVolume->InstallFilters(TRUE);
        } __finally {
            VERIFY(UnlockAPIHandle(hVolumeAPI, hLock));
        }

        if(NO_ERROR != lResult) {
            break;
        }
    }

    return lResult;
}

LRESULT MountTable_t::FinalizeBootSetup()
{
    LRESULT lResult = NO_ERROR;

    ASSERT(2 == m_CurrentBootPhase);

    // Install deferred filters on volumes mounted during boot phases 0 and 1.
    // Failure here is non-fatal.
    return InstallDeferredFilters();
}
