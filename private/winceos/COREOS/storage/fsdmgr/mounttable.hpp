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
#ifndef __MOUNTTABLE_HPP__
#define __MOUNTTABLE_HPP__

#include <windows.h>
#include <string.hxx>
#include <dbgapi.h>

class MountedVolume_t;
class MountableDisk_t;

// MountInfo 
//
// Structure to store information about a mounted volume. Allocate beyond the
// end of the structure to store the complete mount name.
// 
struct MountInfo {
    HANDLE hOwnerProcess;
    HANDLE hVolume;
    DWORD VolumeContext;
    DWORD MountFlags;
    DWORD NameChars;
    WCHAR MountName[1];
};

// ROMVolumeListNode
//
// An item describing a node in the list of mounted ROM volumes associated 
// with a volume table. The order of items in the list is the order in which
// the ROM volumes are searched. Every time a ROM volume mountes, it is 
// inserted at the front of the list.
//
struct ROMVolumeListNode {
   struct ROMVolumeListNode* pNext;
   HANDLE hVolume;
};

static const ULONG MAX_MOUNTED_VOLUMES = 256;
static const ULONG MAX_MOUNT_INDEX = MAX_MOUNTED_VOLUMES - 1;
static const ULONG INVALID_MOUNT_INDEX = (ULONG)-1;

// Index 0 is reserved for backwards compatibility (OID_FIRST_AFS), and index
// 1 is reserved for \Network.
static const ULONG FIRST_USER_INDEX = 2; 

// MountTable_t
//
// Class for managing a table of mount points. There is only ever one mount
// table associated with the storage manager.
//
class MountTable_t {

public:
    MountTable_t();
    ~MountTable_t();

    // NOTE: Any new AFS_FLAG_xxx values must be added to this bitmask.
    static const DWORD VALID_MOUNT_FLAGS = 
        AFS_FLAG_HIDDEN |
        AFS_FLAG_BOOTABLE |
        AFS_FLAG_ROOTFS |
        AFS_FLAG_HIDEROM | // Deprecated; has no effect.
        AFS_FLAG_MOUNTROM |
        AFS_FLAG_SYSTEM |
        AFS_FLAG_PERMANENT |
        AFS_FLAG_NETWORK |
        AFS_FLAG_KMODE;

    // Array access to the MountTable_t object. Returns a pointer to the
    // MountInfo structure in the internal table at the specified index.
    inline MountInfo* operator[] (DWORD Index)
    {
        if ((INVALID_MOUNT_INDEX == Index) ||
            (Index >= MAX_MOUNTED_VOLUMES)) {
            return NULL;
        }
        return m_Table[Index];
    }

    // Compare two paths.
    static inline BOOL ComparePaths (const WCHAR* pPath1, size_t Path1Chars,
            const WCHAR* pPath2, size_t Path2Chars)
    {
        if ((Path1Chars != Path2Chars) || 
            _wcsnicmp(pPath1, pPath2, Path1Chars)) {
            return FALSE;
        }
    
        return TRUE;
    }

    // Determine whether or not an index is within the valid range. Success
    // does not indicate that the index is actually occupied, but that it
    // is simply within the range of the table.
    static inline BOOL IsValidIndex (int Index) 
    {
        if (Index < 0 || Index > MAX_MOUNT_INDEX) {
            return FALSE;
        }
        return TRUE;
    }

    // Validate the characters in a file or directory name.
    static BOOL IsValidFileName (const WCHAR* pFile, size_t FileChars);

    BOOL IsReservedName (const WCHAR* pPath, size_t PathChars);

    // Retrieve the mount name associated with an index in the mount table,
    // if present.
    LRESULT GetMountName (int Index,
        __out_ecount(MountNameChars) WCHAR* pMountName, size_t MountNameChars);

    // Parse the first token from the path, and search for the token in the 
    // table. If the token is found, verify the volume is valid and return 
    // the volume HANDLE.
    LRESULT GetVolumeFromPath (const WCHAR* pPath, __out const WCHAR** ppFileName,
        HANDLE* phVolume, DWORD* pMountFlags);

    inline LRESULT GetVolumeFromPath (const WCHAR* pPath, __out const WCHAR** ppFileName,
        HANDLE* phVolume)
    {
        return GetVolumeFromPath (pPath, ppFileName, phVolume, NULL);
    }

    // Verify the volume at the specified index is valid and return the 
    // volume HANDLE.
    LRESULT GetVolumeFromIndex (int MountIndex, HANDLE* phVolume, 
        DWORD* pMountFlags = NULL);
    
    // Register a MountedVolume_t object in the table at the specified index.
    // Helper function for FSDMGR_RegisterVolume and FS_RegisterAFSEx.
    LRESULT RegisterVolume (int Index, HANDLE hAPISet, DWORD VolumeContext,
        HANDLE hOwnerProcess, DWORD MountFlags);

    // Allocate a MountInfo structure at the first free index in the table
    // and store the name and process id. Fails if a duplicate name already
    // exists in the table.
    LRESULT RegisterVolumeName (const WCHAR* pMountName, int* pIndex, 
            HANDLE hOwnerProcess);

    // Reserve a volume name at the specified index.
    LRESULT ReserveVolumeName (const WCHAR* pMountName, int Index, 
        HANDLE hOwnerProcess);
    
    // Disassociate the MountedVolume_t object and MountFlags value associated
    // with the specified table index. This does not destroy the volume, only
    // removes it from the table. Also call into any virtual file systems And
    // remove this volume from their list. Passing fForce=TRUE will force the
    // volume to be deregistered even if it is flagged as permanent.
    LRESULT DeregisterVolume (int Index, HANDLE hCallerProcess, 
        BOOL fForce = FALSE);

    // Delete the MountInfo structure at the specified table index. Fails if
    // there is still a MountedVolume_t object referenced at the index, so
    // DeregisterVolme must be called first.
    LRESULT DeregisterVolumeName (int Index, HANDLE hCallerProcess);

    // Notify that a volume mount has completed. Generate required file 
    // notifications as necessary.
    LRESULT NotifyMountComplete (int Index, HANDLE hNotifyHandle);

    // Determine the appropriate MountFlags for this volume based on the 
    // registry settings under hKey. 
    void GetMountSettings (HKEY hKey, DWORD* MountFlags);

    // Retrieve the head of the ROM volume list associated with this table.
    inline ROMVolumeListNode* GetROMVolumeList ()
    {
        return m_pROMVolumeList;
    }

    void RegisterFileSystemNotificationFunction (SHELLFILECHANGEFUNC_t pFn);
    inline SHELLFILECHANGEFUNC_t GetFileSystemNotificationFunction ()
    {
        return m_pFnNotification;
    }

    // Lock the table. Should be used externally when retrieving table entries
    // using the operator[] function to prevent the table from changing while
    // searching.
#if defined (DEBUG) && defined (UNDER_CE)
    inline void LockTable ()
    {
        if (!::TryEnterCriticalSection (&m_cs)) {
            DEBUGMSG (ZONE_VERBOSE, (L"FSDMGR!MountTable_t::LockTable: contention over table lock for table %p\r\n", this));
            ::EnterCriticalSection (&m_cs);
        }
    }
#else
    inline void LockTable ()
    {
        ::EnterCriticalSection (&m_cs);
    }
#endif // DEBUG && UNDER_CE

    // Unlock the table.
    inline void UnlockTable ()
    {
        ::LeaveCriticalSection (&m_cs);
    }

    inline int GetHighMountIndex ()
    {
        return m_HighVolumeIndex;
    }   

    inline int GetBootVolumeIndex ()
    {
        return m_BootVolumeIndex;
    }

    inline int GetRootVolumeIndex ()
    {
        return m_RootVolumeIndex;
    }

    inline int GetBootPhase() const 
    {
        return m_CurrentBootPhase;
    }

    inline void SetBootPhase (int NewBootPhase)
    {
        ASSERT (NewBootPhase == 0 || 
                NewBootPhase == 1 ||
                NewBootPhase == 2);

        ASSERT (NewBootPhase >= m_CurrentBootPhase);

        m_CurrentBootPhase = NewBootPhase;
    }
    
    LRESULT FinalizeBootSetup();

protected:

    LRESULT InstallDeferredFilters();

    int m_UNCVolumeIndex;
    int m_RootVolumeIndex;
    int m_BootVolumeIndex;
    int m_HighVolumeIndex;
    int m_CurrentBootPhase;

    ROMVolumeListNode* m_pROMVolumeList;

    // Notificaiton handle for the root volume.
    HANDLE m_hRootNotifyVolume;

    DWORD m_PnPWaitIODelay;

    CRITICAL_SECTION m_cs;
    MountInfo* m_Table[MAX_MOUNTED_VOLUMES];

    // Shell notification callback function pointer.
    SHELLFILECHANGEFUNC_t m_pFnNotification;
};

#endif // __MOUNTTABLE_HPP__
