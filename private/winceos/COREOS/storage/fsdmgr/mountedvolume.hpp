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
#ifndef __MOUNTEDVOLUME_HPP__
#define __MOUNTEDVOLUME_HPP__

#include <dbgapi.h>

class MountTable_t;
class MountableDisk_t;
class FileSystem_t;
class FileSystemHandle_t;

#include "dlist.hpp"

// MountedVolume_t
// 
// Class describing a mounted file system volume and associated properties.
// 
class MountedVolume_t {

public:

    static const DWORD VOL_POWERDOWN        = 0x00000001;
    static const DWORD VOL_DETACHED         = 0x00000002;
    static const DWORD VOL_DEINIT           = 0x80000000;
    static const DWORD VOL_UNAVAILABLE      = (VOL_POWERDOWN | VOL_DETACHED);

    // A flag used to indicate that an FSD did not call FSDMGR_RegisterVolume
    // during its mount disk operation, so post mount notifications (FinalizeMount)
    // were not performed. If this flag is TRUE, then notifications should be
    // performed when FSDMGR_RegisterVolume is eventually called. RelFSD
    // behaves this when when the device has passive-KITL; FSDMGR_RegisterVolme
    // is not called until KITL starts.
    static const DWORD VOL_DELAYED_FINALIZE = 0x00000004;

    // Constructor will call pDisk->Associate (this)
    MountedVolume_t (MountTable_t* pParentTable, MountableDisk_t* pDisk,
        FileSystem_t* pFileSystem, DWORD PnpWaitIoDelay, DWORD MountFlags);
    ~MountedVolume_t ();

    // Register the volume.
    LRESULT Register (int MountIndex);

    // Initialize and attach the volume.
    LRESULT Attach ();

    // Enter the volume. If the device is not available, the reference
    // count is not incremented and the apprpriate error status is 
    // returned. Otherwise, a thread reference is added to the volume.
    LRESULT Enter ();

    // Add a thread reference to the volume. If the volume is temporarily 
    // unavailable due to removal or suspend, wait for the volume to become 
    // detached or available.
    inline LRESULT EnterWithWait ()
    {
        LRESULT VolumeStatus;
        do {
    
            VolumeStatus = Enter ();
    
            if (ERROR_DEVICE_NOT_AVAILABLE == VolumeStatus) {
                // Wait to give the volume a moment to become available.
                ::WaitForSingleObject (m_hVolumeReadyEvent, m_PnPWaitIODelay);
            }
    
        } while (ERROR_DEVICE_NOT_AVAILABLE == VolumeStatus);
        return VolumeStatus;
    }
    
    // Remove a thread reference from the volume. Triggers a volume detach if
    // the reference count goes to zero.
    inline LONG Exit ()
    {
        LRESULT RefCount = RemoveReference (VOLUME_REFERENCE_THREAD);
         
        if ((VOLUME_REFERENCE_ATTACHED & RefCount) &&
            !(VOLUME_REFERENCE_AVAILABLE & RefCount)  &&
            !(VOLUME_REFERENCE_THREAD_MASK & RefCount)) {
            // This is the last thread exiting a volume that is no longer
            // available, so trigger the exit event in case the power-down 
            // thread is waiting our exit.
            ::SetEvent (m_hThreadExitEvent);
        }
         
        if ((VOLUME_REFERENCE_POWERDOWN & RefCount) &&            
            !(VOLUME_REFERENCE_THREAD_MASK & RefCount)) {
            // This is the last thread exiting a volume that is being powered-
            // down, so trigger the exit event in case the power-down thread 
            // is waiting our exit.
            ::SetEvent (m_hThreadExitEvent);
        }
        
        return RefCount;
    }
    
    // Wait for all pending threads that have entered the volume to call Exit.
    // This call blocks until all those threads have exited.
    inline LONG WaitForThreads (LONG RefCount)
    {
        // Loop indefinitely while there are still threads that have
        // entered but not yet exited the volume.
        while (RefCount & VOLUME_REFERENCE_THREAD_MASK) {
            
            DEBUGMSG (1, (L"FSDMGR!MountedVolume_t::WaitForThreads: Waiting for %u threads to exit", 
                (RefCount & VOLUME_REFERENCE_THREAD_MASK) >> 3));
    
            // The volume must ALWAYS be attached while there
            // are still threads operating on it.
            DEBUGCHK (RefCount & VOLUME_REFERENCE_ATTACHED);
                
            // Wait for exiting threads to signal the m_hThreadExitEvent.
    
            //
            // NOTE: Threads ONLY signal this event when the volume is
            // marked as unavailable or for power-down. For good measure,
            // we use the wait i/o delay for a timeout here just in case
            // there's a scenario we're unaware of that could cause us
            // to sit here forever when no threads are in the volume. It 
            // shouldn't happen, but we don't want to deadlock due to a bug.
            //
            DWORD WaitResult = ::WaitForSingleObject (m_hThreadExitEvent, m_PnPWaitIODelay);        
            DEBUGMSG (ZONE_VERBOSE, (L"FSDMGR!MountedVolume_t::WaitForThreads: Wait returned result %u", WaitResult));
    
            //
            // NOTE: The event must be reset BEFORE checking the reference
            // count. If we reversed the order, there is a window of time
            // where a thread could exit after we've checked the ref count
            // but before we've reset the event and the event would not
            // get signalled (we'd end up timing out on the next wait).
            //
            ::ResetEvent (m_hThreadExitEvent);
            RefCount = GetReferenceCount ();

            if (WAIT_TIMEOUT == WaitResult) {
                //
                // If we're waiting on a thread that takes longer than the timeout
                // to complete, we'll break out of the loop and stop blocking. We
                // could potentially retry here, but if there is a deadlock we'll
                // hang the system and we don't want that so it is safer to just 
                // continue.
                //
                RETAILMSG (1, (L"FSDMGR!MountedVolume_t::WaitForThreads: WARNING: timed-out waiting for threads; "
                    L"check for deadlock or increase PnPWaitIODelay\r\n"));
                break;
            }
        }
        
        return RefCount;
    }

    // Create a file notification handle for an open file on the volume.
    inline HANDLE NotifyCreateFile (const WCHAR* pFilePath)
    {
        if (m_hNotifyVolume) {
            return ::NotifyCreateFile (m_hNotifyVolume, pFilePath);
        } else {
            return NULL;
        }
    }

    // Generate file notifications for a path-based change on the volume.
    inline void NotifyPathChange (const WCHAR* pPathName, BOOL fPath,
        DWORD Action)
    {
        if (m_hNotifyVolume) {
            ::NotifyPathChange (m_hNotifyVolume, pPathName, fPath, Action);
        }
    }

    // Generate file notifications for a file move or rename operation 
    // performed on the volume.
    inline void NotifyMoveFile (const WCHAR* pExisting, const WCHAR* pNew)
    {
        if (m_hNotifyVolume) {
            ::NotifyMoveFile (m_hNotifyVolume, pExisting, pNew);
        }
    }
    
    // Generate file notifications for a file or directory move or rename
    // operation performed on the volume.
    inline void NotifyMoveFileEx (const WCHAR* pExisting, const WCHAR* pNew, 
        BOOL fPath)
    {
        if (m_hNotifyVolume) {
            ::NotifyMoveFileEx (m_hNotifyVolume, pExisting, pNew, fPath);
        }
    }

    // Close a file notification handle and generate file notifications if
    // necessary.
    inline void NotifyCloseHandle (HANDLE h)
    {
        if (m_hNotifyVolume) {
            ::NotifyCloseHandle (m_hNotifyVolume, h);
        }
    }

    inline void NotifyHandleChange (HANDLE h, DWORD Flags) 
    {
        if (m_hNotifyVolume && h) {
            ::NotifyHandleChange (m_hNotifyVolume, h, Flags);
        }
    }

    // Create a notification handle to use for detecting file notificaitons.
    // This is the handle that will be returned by FindFirstChangeNotification.
    inline HANDLE NotifyCreateEvent (HANDLE hProc, const WCHAR *pPath, 
        BOOL WatchSubTree, DWORD WatchFlags)
    {
        if (m_hNotifyVolume) {
            return ::NotifyCreateEvent (m_hNotifyVolume, hProc, pPath, 
                WatchSubTree, WatchFlags);
        } else {
            ::SetLastError (ERROR_PATH_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
    }

    // Retrieve the volume notification handle.
    inline HANDLE GetNotificationVolume ()
    {
        return m_hNotifyVolume;
    }

    // Remove the power-down reference from the volume.
    void PowerUp ();

    // Add the power-down reference to the volume and wait for all threads
    // currently in flight to exit before returning.
    void PowerDown ();

    // Disable access to the volume.
    void Disable ();

    // Enable access to the volume.
    void Enable ();

    // Call to unload/free the volume. This function simply decrements the 
    // reference count. When the count reaches zero, DetachVolume will be 
    // called. Once the handle count goes to zero the MountedVolume_t object will
    // be freed.
    LRESULT Detach ();

    // Process a media change event, such as attaching or detaching underlying
    // removable media. This function does not enter the volume as it is called
    // after the volume has been disabled.
    LRESULT MediaChangeEvent (
        STORAGE_MEDIA_CHANGE_EVENT_TYPE Event,
        STORAGE_MEDIA_ATTACH_RESULT* pResult);

    // Call FlushFileBuffers on all open file handles.
    void CommitAllFiles ();

    // Close all file and search handles and delete them. This function takes
    // the volume critical section.
    void CloseAllHandles ();

    // Install a single filter. Typically called by InstallFilters, but made
    // public in case there's another place we might want to use it.
    LRESULT InstallFilter (const WCHAR* RootKey, const WCHAR* SubKey, BOOL fSkipBootFilters);

    // Build a list of filters to associate with this volume and add each one
    // to the filter stack using FileSystemFilterDriver_t.Hook. When
    // fSkipBootFilters is TRUE, ignore any filters that are marked as bootphase
    // 0 or 1. If called during bootphase 0 or 1, filters will be marked as
    // such in the registry so they can be skipped later on.
    LRESULT InstallFilters (BOOL fSkipBootFilters = FALSE);

    // Perform final steps to finish mounting the file system.
    LRESULT FinalizeMount ();

    // Allocate a FileSystemHandle_t object and add it to the volume's list of open
    // file and search handles. Create a notification handle for the open handle 
    // using NotifyCreateFile. Associate the file handle object with a kernel
    // handle owned by the current process and return.
    HANDLE AllocFileHandle (DWORD HandleContext, DWORD HandleFlags,
        FileSystem_t* pFileSystem, const WCHAR* pFileName, DWORD AccessMode);

    // Allocate a FileSystemHandle_t object and add it to the volume's list of open
    // file and search handles. Associate the search handle object with a kernel
    // handle owned by the current process and return.
    HANDLE AllocSearchHandle (DWORD HandleContext, FileSystem_t* pFileSystem);

    // Return a pointer to our associated MountableDisk_t object. Required when
    // deregistering a registered file system in order to free the NullDisk_t 
    // object.
    inline MountableDisk_t* GetDisk () const
    {
        return m_pDisk;
    }

    HANDLE GetDiskHandle ();
    
    // Retrieve a pointer to the top file system in the filter chain.
    inline FileSystem_t* GetFileSystem () const
    {
        return m_pFilterChain;
    }

    // Retrieve the mount flags for this volume.
    inline DWORD GetMountFlags () const
    {
        return m_MountFlags;
    }

    // Retrieve the mount index for this volume. Will return INVALID_MOUNT_INDEX
    // if the volume is not mounted.
    inline  int GetMountIndex () const
    {
        return m_MountIndex;
    }
    
    inline BOOL DelayedFinalize () const
    {
        return (VOL_DELAYED_FINALIZE & m_VolumeStatus);
    }

    inline HANDLE GetVolumeHandle () const
    {
        return m_hVolume;
    }

    // Initialize the volume with default files and directories. Currently a
    // no-op for non-root volumes.
    LRESULT PopulateVolume ();

protected:

    // Reference count bits:
    //
    //      |..............................|.|.|
    // bit  31                                0
    //
    // Bit 0 is used for volume presence. If this bit is not set, the volume
    // should be considered dying and should not be entered by any new threads.
    static const LONG VOLUME_REFERENCE_ATTACHED     = 0x00000001;
    // 
    // Bit 1 is used for volume availability. If this bit is not set, the volume
    // is currently unavailable. Any thread trying to enter the volume should
    // wait until it becomes detached or available.
    static const LONG VOLUME_REFERENCE_AVAILABLE    = 0x00000002;
    // 
    // Bit 2 is used for volume power down status. If this bit is set, the volume
    // is currently in power down mode and should be treated as unavailable.
    static const LONG VOLUME_REFERENCE_POWERDOWN    = 0x00000004;
    // 
    // Bits 3 through 31 are used for thread references. When a thread enters 
    // the volume, it must increment the reference count stored in these bits. 
    // When the thread leaves the volume, it should decrement the reference 
    // count.
    static const LONG VOLUME_REFERENCE_THREAD       = 0x00000008;
    static const LONG VOLUME_REFERENCE_THREAD_MASK  = 0xFFFFFFF8;
    
    // Mark volume as detached, unhook filters, call FSD_UnmountDisk, 
    // dealloc MountableDisk_t object, dealloc FileSystem_t object. If there
    // are still open handles to this volume, the MountedVolume_t object
    // is not destroyed until the last open handle is closed.
    void Destroy ();

    // Remove a reference of the specified type from this volume. RefType can be
    // of type VOLUME_REFERENCE_THREAD, VOLUME_REFERENCE_AVAILABLE, or
    // VOLUME_REFERENCE_ATTACHED.
    inline LONG RemoveReference (LONG RefType)
    {
        LONG RefCount = 
            ::InterlockedExchangeAdd (&m_RefCount, -RefType) - RefType;

        //Dont consider power-down bit for teardown decision.
        if (0 == (RefCount & ~VOLUME_REFERENCE_POWERDOWN)) {
            //No other reference; so destroy (or teardown) the volume.
            Destroy ();
        }
        
        return RefCount;
    }

    // Add a reference of the specified type to this volume. RefType can be of
    // type VOLUME_REFERENCE_THREAD or VOLUME_REFERENCE_AVAILABLE. The 
    // VOLUME_REFERENCE_ATTACHED reference is only set once in the constructor.
    //
    // NOTE: VOLUME_REFERENCE_THREAD should typically be added using the Enter
    // or EnterWithWait functions
    inline LONG AddReference (LONG RefType)
    {
        return ::InterlockedExchangeAdd (&m_RefCount, RefType) + RefType;
    }

    inline LONG GetReferenceCount ()
    {
        return ::InterlockedExchangeAdd (&m_RefCount, 0);
    }

    inline void LockVolume ()
    {
        ::EnterCriticalSection (&m_cs);
    }

    inline void UnlockVolume ()
    {
        ::LeaveCriticalSection (&m_cs);
    }

    // Add a handle to the list of handles associated with this volume.
    friend class FileSystemHandle_t;
    inline void AddHandle (FileSystemHandle_t* pHandle)
    {
        LockVolume ();
        m_HandleList.AddItem (pHandle);
        UnlockVolume ();
    }

    // Deallocate an FileSystemHandle_t object and remove it from the volume's list
    // of open handles. If this is the last handle to be removed and the 
    // volume is detached, destroy the MountedVolume_t object. This function
    // takes the volume critical section.
    void RemoveHandle (FileSystemHandle_t* pHandle);


    // The parent mount table that owns this object.
    MountTable_t* m_pTable;

    // MountableDisk_t object associated with this volume.
    MountableDisk_t* m_pDisk;

    // Pointer to the topmost FileSystem_t object associated with this mounted
    // volume. If it is a FileSystemFilterDriver_t object, then it contains a
    // pointer to the next FileSystem_t object in the list and so on.
    FileSystem_t* m_pFilterChain;

    // Volume critical section used to protect the open handle list and other
    // volume structure.
    CRITICAL_SECTION m_cs;

    // List of open file and search handles associated with this volume.
    DList_t<FileSystemHandle_t> m_HandleList;

    // Contains "MountAsXXX" and "MountFlags" settings for the volume.
    DWORD m_MountFlags;

    // Root notification handle for the volume.
    HANDLE m_hNotifyVolume;

    // Volume status flags: VOL_POWERDOWN, VOL_DETACHED, VOL_DEINIT
    DWORD m_VolumeStatus;

    // Sleep value used when polling for an unavailable file system to become
    // either available or deinitialized.
    DWORD m_PnPWaitIODelay;

    // Reference count for this volume. Initially set to the value of 
    // VOLUME_REFERENCE_ATTACHED (1) | VOLUME_REFERENCE_AVAILABLE (2) in the 
    // constructor. Incremented by VOLUME_REFERENCE_THREAD (4) whenever a thread
    // enters the volume, and decremented back on thread exit. When the count
    // goes to zero, all references are exhaused and the volume is destroyed.
    LONG m_RefCount;

    // Index in the global mount table where this volume is registered. This
    // value is initialized to INVALID_MOUNT_INDEX.
    int m_MountIndex;
    
    // Store a local copy of the true handle to this volume. This is required
    // for handle-locking (LockAPIHandle) when file/search handles are added 
    // to the volume.
    HANDLE m_hVolume;

    // Manual reset event used to indicate that the volume is available and
    // not powered-down.
    HANDLE m_hVolumeReadyEvent;

    // Manual-reset event set when the last thread exiting a volume exits
    // and the volume is being powered-down.
    HANDLE m_hThreadExitEvent;
};

#endif // __MOUNTEDVOLUME_HPP__
