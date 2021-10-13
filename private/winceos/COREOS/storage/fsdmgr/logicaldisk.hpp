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
#ifndef __LOGICALDISK_HPP__
#define __LOGICALDISK_HPP__

class MountedVolume_t;
class FileSystem_t;

// LogicalDisk_t
//
// Interface/Abstract class describing a logical, sector-based, disk. A
// LogicalDisk_t object is analagous to a PDSK object in the Macallan storage
// manager. Both FSDs and Partition drivers will be passed a LogicalDisk_t object
// instead of a PDSK.
//
class LogicalDisk_t {

public:

    static const DWORD DSK_NO_IOCTLS    = 0x00000004;
    static const DWORD DSK_NEW_IOCTLS   = 0x00000008;
    static const DWORD DSK_READONLY     = 0x00000010;
    static const DWORD DSK_CLOSED       = 0x00000020;

    // Enumerated type used to indicate the type of an object that
    // inherits from LogicalDisk_t. Any child class should set the
    // m_ThisLogicalDiskType member of LogicalDisk_t to the appropriate
    // type in the costructor. This is helpful for debugging.
    enum LogicalDiskType_e {
        MountableDisk_Type,
        StoreDisk_Type
    };

    LogicalDisk_t () :
        m_pRootRegKeyList (NULL),
        m_pRegSubKey (NULL),
        m_DiskFlags (0)
    { ; }

    virtual ~LogicalDisk_t ();

    // Pure, virtual DiskIoControl function. Must be overloaded.
    virtual LRESULT DiskIoControl (DWORD IoControlCode, void* pInBuf,
            DWORD InBufBytes, void* pOutBuf, DWORD nOutBufBytes,
            DWORD* pBytesReturned, OVERLAPPED* pOverlapped) = 0;

    // Sets an activity timer event associated with the logical disk, if any.
    virtual void SignalActivity () = 0;

    // Query the name of the disk.
    virtual LRESULT GetName (WCHAR* pDiskName, DWORD NameChars) = 0;

    // Return a handle to the disk.
    virtual HANDLE GetDeviceHandle () = 0;

    LRESULT GetRegistryString (const WCHAR* ValueName, __out_ecount(ValueSize) WCHAR* pValue, DWORD ValueSize);
    LRESULT GetRegistryValue (const WCHAR* ValueName, DWORD* pValue);
    LRESULT GetRegistryFlag (const WCHAR* pValueName, DWORD* pFlag, DWORD SetBit);

    // Add a registry key at the head of m_pRootRegKeyList. Once a registry key
    // is added to the list, it cannot be removed. Keys are searched in the
    // reverse order that they are added.
    LRESULT AddRootRegKey (const WCHAR* RootKeyName);

    // Associate a root key with this object. This key will be appended to each
    // root key in the root key list when searching for registry settings.
    inline LRESULT SetRegSubKey (const WCHAR* SubKeyName, size_t MaxLen = MAX_PATH)
    {
        if (m_pRegSubKey && (0 == ::wcscmp (SubKeyName, m_pRegSubKey))) {
            // Trying to set it back to the same value, just succeed.
            return ERROR_SUCCESS;
        }

        WCHAR* pTemp = NULL;
        LRESULT lResult = ::FsdDuplicateString (&pTemp, SubKeyName, MaxLen);
        if (ERROR_SUCCESS == lResult) {
            // Delete any existing key and replace with the new one.
            if (m_pRegSubKey) {
                delete[] m_pRegSubKey;
            }

            m_pRegSubKey = pTemp;
        }

        return lResult;
    }

#ifdef UNDER_CE
    // Only for CE
    // Default implementation used by both Mountable Disk and Store Disk
    virtual BOOL DoIOControl(DWORD IoControlCode, void* pInBuf, DWORD InBufSize, 
                    void* pOutBuf, DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped )
    {
        //
        // This funciton could be invoked for two reasons:
        //
        // 1 ) A low level request in response to some action on the file system. For
        // examle, a WriteFile request is resulting in an IOCTL_DISK_WRITE operation.
        //
        // 2 ) An IOCTL request from user mode is being forwarded to the lower level
        // driver.
        //

        BOOL bRet = TRUE;
        DEBUGMSG ( ZONE_APIS, ( L"FSDMGR!FSDMGR_DiskIoControl: Default Route with %x \n", this ) );
        HANDLE hDisk = GetDeviceHandle ( );

        if ( ( pInBuf || pOutBuf ) &&
           ( !pInBuf || !::IsKModeAddr ( reinterpret_cast<DWORD> ( pInBuf ) ) ) &&
           ( !pOutBuf || !::IsKModeAddr ( reinterpret_cast<DWORD> ( pOutBuf ) ) ) ) {

            // All top level buffers exist in user VM. In this case, the IOCTL is
            // being forwarded, so continue the forwarding. The existing caller
            // process will be the caller process as viewed by the driver.

               bRet = ForwardDeviceIoControl ( hDisk, IoControlCode, pInBuf, InBufSize, pOutBuf, OutBufSize, pBytesReturned, pOverlapped );

        } else {

            // At least one of the top level buffers is in kernel VM, so perform
            // a normal DeviceIoControl. This process ( kernel ) will be the caller
            // process as viewed by the driver.

            bRet = DeviceIoControl ( hDisk, IoControlCode, pInBuf, InBufSize, pOutBuf, OutBufSize, pBytesReturned, pOverlapped );
        }

        return bRet;
    }
#endif

    DWORD m_DiskFlags;

protected:

    struct RootRegKeyListItem {
        struct RootRegKeyListItem* pNext;
        WCHAR RootRegKeyName[1];
    };

    RootRegKeyListItem* m_pRootRegKeyList;
    WCHAR* m_pRegSubKey;

public:
    // Type of logical disk object.
    LogicalDiskType_e m_ThisLogicalDiskType;
};

// MountableDisk_t
//
// Abstract class describing a logical disk with an associated mounted volume.
// The MountableDisk_t class extends the LogicalDisk_t interface by providing
// association with a single MountedVolume_t object. MountableDisk_t is analagous
// to the PDSK structure passed to an FSD in FSD_MountDisk in the Macallan
// storage manager.
//
class MountableDisk_t : public LogicalDisk_t {

public:

    // Enumerated type used to indicate the type of an object that
    // inherits from MountableDisk_t. Any child class should set the
    // m_ThisMountableDiskType member of MountableDisk_t to the appropriate
    // type in the costructor. This is helpful for debugging.
    enum MountableDiskType_e {
        NullDisk_Type,
        PartitionDisk_Type
    };

    MountableDisk_t (BOOL fCreateDetachEvent) :
        m_pVolume (NULL),
        m_hVolumeDetachEvent (NULL)
    {
        m_ThisLogicalDiskType = MountableDisk_Type;
        if (fCreateDetachEvent) {
            m_hVolumeDetachEvent = ::CreateEvent (NULL, TRUE, TRUE, NULL);
        }

    }

    virtual ~MountableDisk_t ()
    {  
        if (m_hVolumeDetachEvent) {
            ::CloseHandle (m_hVolumeDetachEvent);
            m_hVolumeDetachEvent = NULL;
        }
    }

    // Pure, virtual DiskIoControl function. Must be overloaded.
    virtual LRESULT DiskIoControl (DWORD IoControlCode, void* pInBuf,
            DWORD InBufBytes, void* pOutBuf, DWORD nOutBufBytes,
            DWORD* pBytesReturned, OVERLAPPED* pOverlapped) = 0;

    // Pure, virtual function used to retrieve the store and partition
    // names associated with a mounted volume.
    virtual LRESULT GetVolumeInfo (CE_VOLUME_INFO* pInfo) = 0;

    // Associate a MountedVolume_t with this MountableDisk_t.
    inline void AttachVolume (MountedVolume_t* pVolume)
    {
        DEBUGCHK (NULL == m_pVolume);
        m_pVolume = pVolume;
        if (m_hVolumeDetachEvent) {
            ::ResetEvent (m_hVolumeDetachEvent);
        }
    }

    inline void DetachVolume ()
    {
        m_pVolume = NULL;
        if (m_hVolumeDetachEvent) {
            ::SetEvent (m_hVolumeDetachEvent);
        }
    }

    inline void WaitForVolumeDetach (DWORD Timeout)
    {
        if (m_hVolumeDetachEvent) {
            ::WaitForSingleObject (m_hVolumeDetachEvent, Timeout);
        }
    }

    // Return a pointer to the MountedVolume_t object associated with this
    // MountableDisk_t.
    inline MountedVolume_t* GetVolume ()
    {
        return m_pVolume;
    }

    LRESULT BuildFilterList (FSDLOADLIST** ppFilterList);

    virtual BOOL GetPartitionInfo(PARTINFO* /* pInfo */)
    {
        SetLastError( ERROR_NOT_SUPPORTED );
        return FALSE;
    }

    virtual BOOL GetStoreInfo(STOREINFO* /* pInfo */)
    {
        SetLastError( ERROR_NOT_SUPPORTED );
        return FALSE;
    }

    virtual LRESULT GetSecurityClassName(WCHAR* pSecurityClassName, DWORD NameChars) = 0;

    // Pure Virtual Function indicates whether the disk is available or not.
    virtual BOOL IsAvailable() = 0;


protected:

    // The volume detach event is used to indicate that a volume is not
    // detached to the disk. The event is set when a volume is not attached,
    // and it is reset when a volume is attached.
    HANDLE m_hVolumeDetachEvent;

    // A reference to the mounted volume object, if any. NULL if no volume
    // is currently associated.
    MountedVolume_t* m_pVolume;

    // The type of mountable disk (PartitionDisk_t, NullDisk_t).
    MountableDiskType_e m_ThisMountableDiskType;
};

// NullDisk_t
//
// Class describing a NULL disk used in place of a real disk for auto-load and
// registered file systems that do not have backing sector-based media.
// Provides no IOCTL support. Analagous to the empty PDSK structure passed to
// an auto-load FSD in FSD_MountDisk in the Macallan storage manager.
//
class NullDisk_t : public MountableDisk_t {

public:

    NullDisk_t (const WCHAR* pActivityEventName = NULL) :
        MountableDisk_t (FALSE),
        m_hActivityEvent (NULL)
    {
        if (pActivityEventName && *pActivityEventName) {
            m_hActivityEvent = ::CreateEvent( NULL, FALSE, FALSE, pActivityEventName);
            DEBUGCHK (m_hActivityEvent);
        }
        m_ThisMountableDiskType = NullDisk_Type;
    }

    virtual ~NullDisk_t ()
    {
        DEBUGMSG (ZONE_INIT, (L"FSDMGR!NullDisk_t::~NullDisk_t: deleting disk (%p)", this));
        if (m_hActivityEvent) {
#ifdef UNDER_CE
            ::CloseHandle (m_hActivityEvent);
#else
            ::NotifyCloseEvent (m_hActivityEvent);
#endif
        }
    }

    virtual LRESULT GetVolumeInfo (CE_VOLUME_INFO* pInfo)
    {
        // NullDisk_t objects have no store or partition names.
        ::memset (pInfo->szStoreName, 0, sizeof (pInfo->szStoreName));
        ::memset (pInfo->szPartitionName, 0, sizeof (pInfo->szPartitionName));

        // NullDisk_t objects have no backing storage device, so
        // make sure this flag is not set.
        pInfo->dwAttributes &= ~CE_VOLUME_FLAG_STORE;

        return ERROR_SUCCESS;
    }

    // Since there is no disk associated with a NullDisk_t, IOCTLs are not
    // supported.
    virtual LRESULT DiskIoControl (DWORD /* IoControlCode */, void* /* pInBuf */,
            DWORD /* InBufBytes */, void* /* pOutBuf */, DWORD /* nOutBufBytes */,
            DWORD* /* pBytesReturned */, OVERLAPPED* /* pOverlapped */)
    {
        return ERROR_NOT_SUPPORTED;
    }

#ifdef UNDER_CE
    //Since there is no disk associated with a NullDisk_t, return FALSE
    virtual BOOL DoIOControl(DWORD IoControlCode, void* pInBuf, DWORD InBufSize, 
                    void* pOutBuf, DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped )
    {
        DEBUGMSG ( ZONE_APIS, ( L"FSDMGR!FSDMGR_DiskIoControl: Device IO Control is called on NullDisk %x Not supported \n", this ) );
        return FALSE;
    }
#endif

    // Signal activity on the logical disk.
    virtual void SignalActivity ()
    {
        if (m_hActivityEvent) {
            ::SetEvent (m_hActivityEvent);
        }
    }

    // The name of a NullDisk_t object is undefined, so always just return
    // the string "NullDisk_t."
    virtual LRESULT GetName (WCHAR* pDiskName, DWORD NameChars)
    {
        if (FAILED (::StringCchCopyW (pDiskName, NameChars, L"NullDisk"))) {
            return ERROR_INSUFFICIENT_BUFFER;
        } else {
            return ERROR_SUCCESS;
        }
    }

    // There is no real disk, so there cannot be a handle.
    virtual HANDLE GetDeviceHandle ()
    {
        return NULL;
    }

    // Return the security classification name for the disk. For null-disks,
    // use the registry sub-key, which will be the file system name.
    virtual LRESULT GetSecurityClassName(WCHAR* pSecurityClassName, DWORD NameChars)
    {
        if (FAILED (StringCchCopyW (pSecurityClassName, NameChars, m_pRegSubKey))) {
            return ERROR_INSUFFICIENT_BUFFER;
        } else {
            return ERROR_SUCCESS;
        }
    }

    // There is no real disk, so no need to check explicitly.
    inline virtual BOOL IsAvailable() 
    {
        return TRUE;
    }

protected:
    HANDLE m_hActivityEvent;
};

#endif // __LOGICALDISK_HPP__
