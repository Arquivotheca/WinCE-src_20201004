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

class MountedVolume_t;
class FileSystem_t;

#include "dlist.hpp"
#include "volumeapi.hpp"

// FileSystemHandle_t
//
// Simple open file or search handle object. Every open handle now has an
// associated FileSystem_t object. This allows handles to be routed around
// filters that have been installed higher in the chain after the handle
// was created.
// 
class FileSystemHandle_t : public DListNode_t {

public: 
    // Values used on the m_Flags member.
    static const DWORD HDL_FILE          = 0x00000001; // HT_FILE
    static const DWORD HDL_SEARCH        = 0x00000002; // HT_FIND 
    static const DWORD HDL_NO_LOCK_PAGES = 0x00000004; // Disable lock pages on read and write
    static const DWORD HDL_CONSOLE       = 0x20000000; // Created using \reg: or \con
    static const DWORD HDL_PSUEDO        = 0x40000000; // Created using VOL:
    static const DWORD HDL_INVALID       = 0x80000000; // Volume no longer exists
    
    FileSystemHandle_t (MountedVolume_t* pVolume, FileSystem_t* pFileSystem, DWORD Context, 
            DWORD Flags, DWORD AccessMode) :
        m_pVolume (pVolume),
        m_pFileSystem (pFileSystem),
        m_Context (Context),
        m_Flags (Flags),
        m_AccessMode (AccessMode),
        m_hVolumeLock (NULL),
        m_hNotify (NULL)
    {
        if (m_pVolume) {
            m_pVolume->AddHandle (this);
        }
    }

    ~FileSystemHandle_t ()
    {
        Invalidate ();
        
        if (m_pVolume) {
            m_pVolume->RemoveHandle (this);
            m_pVolume = NULL;
        }

        if (m_hVolumeLock) {
            // Remove the volume lock.
            VERIFY (::UnlockAPIHandle (hVolumeAPI, m_hVolumeLock));
            m_hVolumeLock = NULL;
        }
    }

    inline void NotifyCreateHandle (const WCHAR* pFilePath)
    {
        // Create a notification handle for this file handle.
        if (m_pVolume) {
            m_hNotify = m_pVolume->NotifyCreateFile (pFilePath);
        }
    }
    
    // Generate file notifications for a handle-based change. Only applies to
    // file handles.
    inline void NotifyHandleChange (DWORD Flags)
    {
        if (m_pVolume && m_hNotify) {
            m_pVolume->NotifyHandleChange (m_hNotify, Flags);
        }
    }

    inline void NotifyCloseHandle ()
    {
        if (m_pVolume && m_hNotify) {
            m_pVolume->NotifyCloseHandle (m_hNotify);
            m_hNotify = NULL;
        }
    }

    // Create a kernel handle for this object.
    HANDLE CreateHandle (HANDLE hAPISet, HANDLE hVolume)
    {   
        if (m_hVolumeLock) {
            // If there's a volume lock installed already, we've already created
            // a handle. This shouldn't ever be 
            DEBUGCHK (0);
            return NULL;
        }

        HANDLE hAPIHandle = NULL;

        // Install a lock on the volume handle so that the volume cannot
        // be destroyed while this handle exists. This lock will be removed
        // in the destructor.
        void* pVolumeObject;
        m_hVolumeLock = ::LockAPIHandle (hVolumeAPI, (HANDLE)GetCurrentProcessId (), 
            hVolume, &pVolumeObject);

        if (m_hVolumeLock) {

            // We expect that the lock was installed on the volume referenced
            // by this handle object, so the object returned by LockAPIHandle
            // should be the same.
            DEBUGCHK (pVolumeObject == reinterpret_cast<void*> (m_pVolume));
        
            // Create a handle for the specified API set. This handle is owned by
            // this process (kernel).        
            hAPIHandle = CreateAPIHandle (hAPISet, this);
            if (!hAPIHandle) {
                // Failed to create a new handle so remove the volume lock.
                VERIFY (::UnlockAPIHandle (hVolumeAPI, m_hVolumeLock));
                m_hVolumeLock = NULL;
            }
        }

        // Else: we failed to lock the volume, so don't allocate the handle.
        // This is likely caused by the volume being destroyed.

        return hAPIHandle;
    }

    // Validate the handle and call the m_pVolume->Enter function. If 
    // successful, return m_pVolume. Otherwise, return NULL.
    inline LRESULT EnterWithWait (MountedVolume_t** ppVolume)
    {
        // As long as there are open handles with a reference to a volume
        // that volume will exist.
        DEBUGCHK (m_pVolume);

        // Always return a volume pointer, even on failure.
        *ppVolume = m_pVolume;

        // Make sure the handle is invalid.
        if (!IsValid ()) {
            // This handle was already marked as invalid. This only occurs
            // after the volume is no longer available, so indicate that
            // the device has been removed.
            return ERROR_DEVICE_REMOVED;
        }

        LRESULT lResult = m_pVolume->EnterWithWait ();

        if(ERROR_PATH_NOT_FOUND == lResult) {

            // ERROR_PATH_NOT_FOUND does not make sense for handle-based calls,
            // so translate to a more appropriate error code. Since it is not
            // possible to have a handle to a file on a device that is not yet
            // initialized, we know it must instead be detached. The pager
            // expects this error code, ERROR_DEVICE_REMOVED, in order to stop
            // the retry loop on ReadFileWithSeek failure during page-in.

            lResult = ERROR_DEVICE_REMOVED;
        }

        return lResult;
    }

    inline DWORD GetHandleFlags ()
    {
        return m_Flags;
    }

    inline FileSystem_t* GetOwnerFileSystem ()
    {
        return m_pFileSystem;
    }

    inline DWORD GetHandleContext ()
    {
        return m_Context;
    }

    inline BOOL CheckAccess (DWORD AccessRequest)
    {
        // All requested access bits must be set for access check to return TRUE.
        return (AccessRequest == (AccessRequest & m_AccessMode)) ? TRUE : FALSE;
    }

    inline void Invalidate ()
    {
        // Mark the handle as invalid for tracking purposes.
        m_Flags |= HDL_INVALID;

        // Any associated file notification handle will be destroyed 
        // when the volume notification handle is destroyed or when
        // NotifyCloseHandle is called.
        m_hNotify = NULL;
    }

    inline BOOL IsValid ()
    {
        return !(HDL_INVALID & m_Flags);
    }

    inline void DisableLockPages()
    {
        m_Flags |= HDL_NO_LOCK_PAGES;
    }

    inline BOOL IsLockPagesDisabled()
    {
        return (m_Flags & HDL_NO_LOCK_PAGES);
    }

    // A pointer to the file system that created the handle context stored in
    // this object (m_Context). Note that this might not be the top file system
    // in MountedVolume_t object's filter chain because additional filters might
    // have been added at the front of the chain after this handle was created.
    FileSystem_t* m_pFileSystem;

    // Handle context returned by the FileSystem_t object's FSD_CreateFileW
    // export.
    const DWORD m_Context;

private:

    operator=(FileSystemHandle_t&);

    // Handle to a notification object associated with this handle. This 
    // value will be NULL for search handles which have no notification 
    // information.
    HANDLE m_hNotify;

    // A handle object holds a lock on the volume handle. This ensures that
    // the volume will not be destoryed until the lock is released in the
    // FileSystemHandle_t destructor.
    HANDLE m_hVolumeLock;

    // A pointer to the volume that owns this handle. 
    MountedVolume_t* m_pVolume;

    // Flags indicating properties about this handle (HDL_FILE, HDL_SEARCH,
    // HLD_PSUEDO, HDL_CONSOLE, HDL_INVALID).
    DWORD m_Flags;

    // File access mode: GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE
    // Search handles should be 0.
    const DWORD m_AccessMode;
};

