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

MountedVolume_t::MountedVolume_t (MountTable_t* pParentTable, MountableDisk_t* pDisk, 
    FileSystem_t* pFileSystem, DWORD PnpWaitIoDelay, DWORD MountFlags) :
    m_pTable (pParentTable),
    m_pDisk (pDisk),
    m_pFilterChain (pFileSystem),   
    m_PnPWaitIODelay (PnpWaitIoDelay),
    m_MountFlags (MountFlags),
    m_hNotifyVolume (NULL),
    m_VolumeStatus (0),
    m_RefCount (0),
    m_MountIndex (INVALID_MOUNT_INDEX),
    m_hVolumeReadyEvent (NULL),
    m_hThreadExitEvent (NULL),
    m_hVolume (NULL)
    
{
    ::InitializeCriticalSection (&m_cs);   
}

MountedVolume_t::~MountedVolume_t ()
{    
    DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountedVolume_t::~MountedVolume_t: deleting volume (%p)", this));

    // The volume must always be detached before it is deleted.
    DEBUGCHK(INVALID_MOUNT_INDEX == m_MountIndex);

    if (m_hVolumeReadyEvent) {
        ::CloseHandle (m_hVolumeReadyEvent);
        m_hVolumeReadyEvent = NULL;
    }
    
    if (m_hThreadExitEvent) {
        ::CloseHandle (m_hThreadExitEvent);
        m_hThreadExitEvent = NULL;
    }

    // Disassociate this volume with the MountableDisk_t object.
    if (m_pDisk) {
        m_pDisk->DetachVolume ();
        m_pDisk = NULL;
    }
    
    ::DeleteCriticalSection (&m_cs);

    // At this point, we should have closed all open handles
    // so no more should remain on the handle list.
    DEBUGCHK (0 == m_HandleList.GetItemCount ());
}

LRESULT MountedVolume_t::Register (int MountIndex)
{
    if (INVALID_MOUNT_INDEX != m_MountIndex) {
        // Don't expect this to ever be called twice because this API is only
        // ever used internally.
        DEBUGCHK (0);
        return ERROR_ALREADY_EXISTS;
    }

    // Retrieve the handle for this volume. We need a local copy for 
    // locking when file/search handles are added to the volume. Also,
    // reload the mountflags associated with the volume in case they
    // were modified by the mount table due to conflict.
    LRESULT lResult = m_pTable->GetVolumeFromIndex (MountIndex, &m_hVolume, 
        &m_MountFlags);
    if (ERROR_SUCCESS != lResult) {
        DEBUGCHK (0);
        return lResult;
    }

    // Store the mount index locally.
    m_MountIndex = MountIndex;

    return ERROR_SUCCESS;
}

LRESULT MountedVolume_t::Attach ()
{
    if (INVALID_MOUNT_INDEX == m_MountIndex) {
        // The volume is not registered.
        DEBUGCHK (0);
        return ERROR_FILE_NOT_FOUND;
    }

    DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountedVolume_t::Attach: attaching volume (%p)\r\n", this));
    // Create the "volume ready" event. This event will be reset when
    // the volume is not available so that threads waiting for the volume
    // can wait for it to be signalled once the volume becomes ready again.
    DEBUGCHK (NULL == m_hVolumeReadyEvent);
    m_hVolumeReadyEvent = ::CreateEvent (NULL, TRUE, TRUE, NULL);
    if (!m_hVolumeReadyEvent) {
        return ::FsdGetLastError (ERROR_GEN_FAILURE);
    }

    // Create the "thread exit" event. This event will be reset when the
    // thread is being disabled or powered-off. The last thread to exit
    // the a volume in this situation will signal the event to trigger
    // disabling or power-off to complete.
    DEBUGCHK (NULL == m_hThreadExitEvent);
    m_hThreadExitEvent = ::CreateEvent (NULL, TRUE, TRUE, NULL);
    if (!m_hThreadExitEvent) {
        return ::FsdGetLastError (ERROR_GEN_FAILURE);
    }

    if (!(AFS_FLAG_HIDDEN & m_MountFlags)) {
        // Retrieve the name of this mounted volume from the parent table.
        WCHAR MountName[MAX_PATH];
        LRESULT lResult = m_pTable->GetMountName (m_MountIndex, MountName, MAX_PATH);
        if (ERROR_SUCCESS != lResult) {
            DEBUGCHK (0);
            return lResult;
        }
        // Create a volume notification handle if this volume is not hidden.
        DEBUGCHK (NULL == m_hNotifyVolume);
        m_hNotifyVolume = ::NotifyCreateVolume(MountName);
        DEBUGCHK (m_hNotifyVolume);
    }

    // Mark the volume as attached and available so threads may now enter.
    // A separate call to Enable is not required.
    AddReference (VOLUME_REFERENCE_ATTACHED | VOLUME_REFERENCE_AVAILABLE);

    return ERROR_SUCCESS;
}

LRESULT MountedVolume_t::PopulateVolume ()
{
    if (!(AFS_FLAG_ROOTFS & m_MountFlags)) {
        // Nothing to populate on non-root volumes.
        return ERROR_SUCCESS;
    }

    DEBUGCHK (m_hVolume);

    // When the root file system is mounted, immediately create 
    // the system (\Windows) directory. These will silently fail
    // if the directories already exists.
    if (INVALID_FILE_ATTRIBUTES == AFS_GetFileAttributesW (m_hVolume, SYSDIRNAME)) {

        VERIFY (AFS_CreateDirectoryW (m_hVolume, SYSDIRNAME, NULL, NULL, 0));
    }

    if (INVALID_FILE_ATTRIBUTES == AFS_GetFileAttributesW (m_hVolume, L"\\Temp")) {

        VERIFY (AFS_CreateDirectoryW (m_hVolume, L"\\Temp", NULL, NULL, 0));
    }

    return ERROR_SUCCESS;
}

LRESULT MountedVolume_t::FinalizeMount ()
{
    if (INVALID_MOUNT_INDEX == m_MountIndex) {
        // An FSD has delayed calling FSDMGR_RegisterVolume.
        m_VolumeStatus |= VOL_DELAYED_FINALIZE;
        return ERROR_SUCCESS;
    }

    WCHAR MountName[MAX_PATH];
    LRESULT lResult = m_pTable->GetMountName (m_MountIndex, MountName, MAX_PATH);
    if (ERROR_SUCCESS != lResult) {
        DEBUGCHK (0);
        return lResult;
    }

    lResult = PopulateVolume ();
    if (ERROR_SUCCESS != lResult) {
        DEBUGCHK (0);
        return lResult;
    }

    // Notify the parent table that the mount has completed.
    m_pTable->NotifyMountComplete (m_MountIndex, m_hNotifyVolume);
    // ... nothing to do on failure here.

    // Perform final notifications indicating the file system is ready
    // for use.
    if (!(AFS_FLAG_HIDDEN & m_MountFlags)) {

        FILECHANGEINFO NotifyInfo = {0};
        NotifyInfo.cbSize = sizeof(FILECHANGEINFO);
        NotifyInfo.uFlags = SHCNF_PATH | SHCNF_FLUSHNOWAIT;
        NotifyInfo.dwItem1 = (DWORD)MountName;
        NotifyInfo.wEventId = SHCNE_DRIVEADD;
        NotifyInfo.dwAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_TEMPORARY;
        NotifyInfo.ftModified.dwLowDateTime = 0;
        NotifyInfo.ftModified.dwHighDateTime = 0;
        NotifyInfo.nFileSize = 0;

        ::FSDMGR_AdvertiseInterface (&FSD_MOUNT_GUID, MountName, TRUE);
    }

    if (AFS_FLAG_BOOTABLE & m_MountFlags) {
        ::FSDMGR_AdvertiseInterface (&BOOTFS_MOUNT_GUID, MountName, TRUE);
    }

    if (AFS_FLAG_MOUNTROM & m_MountFlags) {
        ::FSDMGR_AdvertiseInterface (&ROMFS_MOUNT_GUID, MountName, TRUE);
    }

    if (AFS_FLAG_ROOTFS & m_MountFlags) {
        ::FSDMGR_AdvertiseInterface (&ROOTFS_MOUNT_GUID, MountName, TRUE);
    }

    DEBUGCHK (m_pFilterChain);

    return ERROR_SUCCESS;
}

LRESULT MountedVolume_t::Enter ()
{
    LONG RefCount;

    do
    {
        // Query the current reference count.
        RefCount = GetReferenceCount ();
        if (!(VOLUME_REFERENCE_ATTACHED & RefCount)) {
            // The volume is either not yet fully initialized/attached or it
            // has been detached. Either way, the path does not exist.
            return ERROR_PATH_NOT_FOUND;
        }

        if (!(VOLUME_REFERENCE_AVAILABLE & RefCount)) {
            // The volume is not available.
            return ERROR_DEVICE_NOT_AVAILABLE;
        }

        if (VOLUME_REFERENCE_POWERDOWN & RefCount) {
            // The volume is powered down.
            return ERROR_DEVICE_NOT_AVAILABLE;
        }

    } while (RefCount != ::InterlockedCompareExchange (&m_RefCount, 
                        (RefCount + VOLUME_REFERENCE_THREAD), 
                        RefCount));

    // Signal activity is occuring on the disk each time we successfully 
    // enter the volume.
    if (m_pDisk) {
        m_pDisk->SignalActivity ();
    }

    return ERROR_SUCCESS;
}

void MountedVolume_t::PowerUp ()
{        
    DEBUGMSG (ZONE_POWER, (L"FSDMGR!MountedVolume_t::PowerUp: powering up volume (%p)\r\n", this));

    DEBUGCHK (VOLUME_REFERENCE_POWERDOWN & m_RefCount);
    
    // Remove the power-down reference from the volume.
    LONG RefCount = RemoveReference (VOLUME_REFERENCE_POWERDOWN);

    if (VOLUME_REFERENCE_AVAILABLE & RefCount) {    
        // The volume is powered on and available, so set the volume-
        // ready event to release waiting threds.
        ::SetEvent (m_hVolumeReadyEvent);
    }
}

void MountedVolume_t::PowerDown ()
{            
    DEBUGMSG (ZONE_POWER, (L"FSDMGR!MountedVolume_t::PowerDown: powering down volume (%p)\r\n", this));

    DEBUGCHK (!(VOLUME_REFERENCE_POWERDOWN & m_RefCount));
    
    // Reset the power-up event. This event will be set again
    // when the volume is powered back on.
    ::ResetEvent (m_hVolumeReadyEvent);

    // Reset the thread exit event so we can wait for threads
    // to exit the volume, if any.
    ::ResetEvent (m_hThreadExitEvent);

    // Add the power-down reference to the volume.
    LONG RefCount = AddReference (VOLUME_REFERENCE_POWERDOWN);

    // Wait for all remaining threads to exit the volume before
    // returning from PowerDown.
    RefCount = WaitForThreads (RefCount);

    // Commit any open file handles to disk before continuing with 
    // the power off operation.
    CommitAllFiles ();
}

void MountedVolume_t::Disable ()
{
    DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountedVolume_t::Disable: disable volume (%p)\r\n", this));

    // Reset the volume ready event. This event will be set again
    // when the volume is powered back on.
    // Need to set event before changing reference count, because
    // it takes longer to reset an event than to change the logic
    ::ResetEvent (m_hVolumeReadyEvent);

    DEBUGCHK (VOLUME_REFERENCE_AVAILABLE & m_RefCount);
    LONG RefCount = RemoveReference (VOLUME_REFERENCE_AVAILABLE);
    UNREFERENCED_PARAMETER(RefCount);

    // The volume *must always* be disabled prior to being
    // detached. If this rule is adhered to, we guarantee the
    // volume is always attached at this point.
    DEBUGCHK (RefCount & VOLUME_REFERENCE_ATTACHED);
}

void MountedVolume_t::Enable ()
{
    DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountedVolume_t::Enable: enable volume (%p)\r\n", this));
    DEBUGCHK (!(VOLUME_REFERENCE_AVAILABLE & m_RefCount));
    LONG RefCount = AddReference (VOLUME_REFERENCE_AVAILABLE);
    if (!(VOLUME_REFERENCE_POWERDOWN & RefCount)) {
        // The volume is available and not powered-down so set the
        // volume-ready event to release any waiting threads.
        ::SetEvent (m_hVolumeReadyEvent);
    }
}

LRESULT MountedVolume_t::Detach ()
{
    if (INVALID_MOUNT_INDEX == m_MountIndex) {
        // This volume is not currently attached.
        return ERROR_SUCCESS;
    }

    WCHAR MountName[MAX_PATH];
    LRESULT lResult = m_pTable->GetMountName (m_MountIndex, MountName, MAX_PATH);

    DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountedVolume_t::Detach: detaching volume \"%s\" (%p)", 
        MountName, this));

    m_MountIndex = INVALID_MOUNT_INDEX;
    m_VolumeStatus |= VOL_DETACHED;
    
    if (ERROR_SUCCESS == lResult) {
        
        // Perform device notifications indicating that this FSD has been removed.
        if (!(AFS_FLAG_HIDDEN & m_MountFlags)) {
            ::FSDMGR_AdvertiseInterface (&FSD_MOUNT_GUID, MountName, FALSE);
        }
        
        if (AFS_FLAG_BOOTABLE & m_MountFlags) {
            ::FSDMGR_AdvertiseInterface (&BOOTFS_MOUNT_GUID, MountName, FALSE);
        }

        if (AFS_FLAG_MOUNTROM & m_MountFlags) {
            ::FSDMGR_AdvertiseInterface (&ROMFS_MOUNT_GUID, MountName, FALSE);
        }
        
        if (AFS_FLAG_ROOTFS & m_MountFlags) {
            ::FSDMGR_AdvertiseInterface (&ROOTFS_MOUNT_GUID, MountName, FALSE);
        }
    }

    // When the final reference is gone, Destory will perform additional
    // volume cleanup.
    RemoveReference (VOLUME_REFERENCE_ATTACHED);

    return ERROR_SUCCESS;
}

LRESULT MountedVolume_t::MediaChangeEvent (
    STORAGE_MEDIA_CHANGE_EVENT_TYPE Event,
    STORAGE_MEDIA_ATTACH_RESULT* pResult)
{
    DWORD BytesReturned;
    FileSystem_t* pFileSystem = GetFileSystem ();

    // Pass the media change event to the file system. Do not enter the volume
    // because it is legal for media change events to occur while the volume is
    // disabled!
    if (!pFileSystem->FsIoControl (
        reinterpret_cast<HANDLE> (GetCurrentProcessId ()),
        FSCTL_STORAGE_MEDIA_CHANGE_EVENT,
        &Event,
        sizeof (STORAGE_MEDIA_CHANGE_EVENT_TYPE),
        pResult,
        pResult ? sizeof (STORAGE_MEDIA_ATTACH_RESULT) : 0,
        &BytesReturned,
        NULL))
    {
        return ::FsdGetLastError ();
    }
    return ERROR_SUCCESS;
}

void MountedVolume_t::CommitAllFiles ()
{
    for (FileSystemHandle_t* pHandle = m_HandleList.FirstItem ();
         pHandle != NULL;
         pHandle = m_HandleList.NextItem (pHandle)) {

        // Flush all files that are opened for writing.
        DWORD HandleFlags = pHandle->GetHandleFlags ();
        if ((FileSystemHandle_t::HDL_FILE & HandleFlags) &&
            (pHandle->CheckAccess (GENERIC_WRITE))) {
            FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
            DWORD HandleContext = pHandle->GetHandleContext ();
            pFileSystem->FlushFileBuffers (HandleContext);
        }
    }
}

void MountedVolume_t::CloseAllHandles ()
{
    LockVolume ();
    
    for (FileSystemHandle_t* pHandle = m_HandleList.FirstItem ();
         pHandle != NULL;
         pHandle = m_HandleList.NextItem (pHandle)) {

        FileSystem_t* pFileSystem = pHandle->GetOwnerFileSystem ();
        DWORD HandleContext = pHandle->GetHandleContext ();
        DWORD HandleFlags = pHandle->GetHandleFlags ();

        // NOTENOTE: This is the only time we ever call into and FSD
        // while holding a volume lock. We could avoid this by duplicating
        // the handle list before calling out, but there doesn't seem to
        // be much chance for deadlock so it ought not really matter.
        if (FileSystemHandle_t::HDL_FILE & HandleFlags) {

            // Close a file handle.
            pFileSystem->CloseFile (HandleContext);

        } else if (FileSystemHandle_t::HDL_SEARCH & HandleFlags) {

            // Close a search handle.
            pFileSystem->FindClose (HandleContext);

        } // else this must be a pseudo handle.

        // Mark the handle as invalid.
        pHandle->Invalidate ();
    }

    UnlockVolume ();
}

// Every filter gets a FileSystemFilterDriver_t object instance to manage the 
// filter driver itself and a NullDisk_t object to manage FSDMGR_ support 
// functions used by the filter.
LRESULT MountedVolume_t::InstallFilter (const WCHAR* RootKey, const WCHAR* SubKey, BOOL fSkipBootFilters)
{
    LRESULT lResult;

    DWORD dwFilterPhase;
    NullDisk_t* pDisk = NULL;
    HKEY hFilterKey = NULL;
    FileSystemFilterDriver_t* pFilter = NULL;
        
    // Allocate a new disk object.
    pDisk = new NullDisk_t ();
    if (!pDisk) {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit;
    }

    // Add the HKLM\System\StorageManager\Filters root key.
    WCHAR RegKey[MAX_PATH];
    ::StringCchPrintfW (RegKey, MAX_PATH, L"%s\\Filters", g_szSTORAGE_PATH);
    pDisk->AddRootRegKey (RegKey);

    if (0 != ::_wcsicmp (RegKey, RootKey)) {
        // Add the key that this filter is being loaded from if it is
        // not the same as the root key.
        pDisk->AddRootRegKey (RootKey);
    }

    // The reg sub key is the name of the filter subkey.
    pDisk->SetRegSubKey (SubKey);

    // Build the full reg key that we're loading this filter from.
    ::StringCchPrintfW (RegKey, MAX_PATH, L"%s\\%s", RootKey, SubKey);

    lResult = ::FsdRegOpenKey (RegKey, &hFilterKey);
    if (ERROR_SUCCESS != lResult) {
        goto exit;
    }

    if(fSkipBootFilters &&
       ::FsdGetRegistryValue(hFilterKey, g_szFSD_BOOTPHASE_STRING, &dwFilterPhase) &&
       (dwFilterPhase < 2)) {
        // Skip filters that are marked as bootphase 0 or 1.
        DEBUGCHK(m_pTable->GetBootPhase() > 1);
#ifdef DEBUG
        WCHAR szVolume[MAX_PATH] = L"<unknown>";
        VERIFY(ERROR_SUCCESS == m_pTable->GetMountName(m_MountIndex, szVolume, _countof(szVolume)));
        DEBUGMSG(ZONE_INIT, (L"FSDMGR!MountedVolume_t::InstallFilter: Skip filter \"%s\" already loaded on volume \"%s\"\r\n", 
                    RegKey, szVolume));
#endif // DEBUG

        lResult = ERROR_ALREADY_EXISTS;
        goto exit;
    }

    WCHAR DllName[MAX_PATH];

    if (!::FsdGetRegistryString (hFilterKey, g_szFILE_SYSTEM_MODULE_STRING, DllName, MAX_PATH)) {
        lResult = ERROR_FILE_NOT_FOUND;
        goto exit;
    }

    DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountedVolume_t::InstallFilter: Loading filter \"%s\" from key \"%s\"\r\n",
        DllName, RegKey));

    // Install the new filter driver.
    pFilter = new FileSystemFilterDriver_t (pDisk, DllName);
    if (!pFilter) {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit;
    }

    lResult = pFilter->Load ();
    if (ERROR_SUCCESS != lResult) {
        goto exit;
    }        

#ifdef UNDER_CE
    // If we're currently in bootphase 0 or 1, leave a breadcrumb in the filter
    // registry key indicating the current bootphase. Use this information later
    // to determine whether or not a filter needs to be installed during the
    // bootphase 2 filter-only install pass.
    dwFilterPhase = m_pTable->GetBootPhase();
    if(dwFilterPhase < 2) {
        DEBUGCHK(!fSkipBootFilters);
        lResult = RegSetValueEx(hFilterKey, g_szFSD_BOOTPHASE_STRING, 0, REG_DWORD, (BYTE*)&dwFilterPhase, sizeof(dwFilterPhase));
        if(ERROR_SUCCESS != lResult) {
            // Failure writing to the boot registry is unexpected, but will cause
            // us to fail and unload the filter.
            DEBUGCHK(FALSE);
            goto exit;
        }
    }
#endif // UNDER_CE

    // Attach this volume to the NullDisk_t instance. Multiple disk
    // objects now refer to this volume object, but we really only 
    // count the disk associated with the file system driver.
    pDisk->AttachVolume (this);
    
    FileSystem_t* pFileSystemTmp = pFilter->Hook (m_pFilterChain);
    if (!pFileSystemTmp) {
        // Failed to hook the filter?
        lResult = ::FsdGetLastError (ERROR_GEN_FAILURE);
        goto exit;
    }
    
    // This filter is now at the top of the chain.
    m_pFilterChain = pFileSystemTmp;

    lResult = ERROR_SUCCESS;

exit:
    if (hFilterKey) {
        ::FsdRegCloseKey (hFilterKey);
    }
    
    if (ERROR_SUCCESS != lResult) {
        
        DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountedVolume_t::InstallFilter: failed loading filter \"%s\"\r\n", DllName));
        
        // Perform cleanup on failure.
        if (pFilter) {

            // The filter object will clean up the disk object on
            // our behalf during unload.
            pFilter->Unload ();
            delete pFilter;
            
        } else if (pDisk) {

            // Clean up the disk object.
            delete pDisk;
        }
    }
    return lResult;
}

LRESULT MountedVolume_t::InstallFilters (BOOL fSkipBootFilters)
{   
    FSDLOADLIST* pFilterList = NULL;
    LRESULT lResult;

    DEBUGCHK (m_pDisk);

    // The MountableDisk_t object is responsible for reading the registry,
    // so it builds the filter list based on its own known root keys.
    lResult = m_pDisk->BuildFilterList (&pFilterList);
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }       

    // Perform installation of all filters.
    while (pFilterList) {

        InstallFilter (pFilterList->szPath, pFilterList->szName, fSkipBootFilters);

        FSDLOADLIST* pTemp = pFilterList;
        pFilterList = pFilterList->pNext;
        delete pTemp;
    }

    return lResult;
}

HANDLE MountedVolume_t::GetDiskHandle ()
{
    if (m_pDisk) {
        
        return m_pDisk->GetDeviceHandle ();

    } else {
    
        return INVALID_HANDLE_VALUE;
    }
}

HANDLE MountedVolume_t::AllocSearchHandle (DWORD HandleContext, 
    FileSystem_t* pFileSystem)
{
    HANDLE hRet = INVALID_HANDLE_VALUE;

    // The constructur will automatically add the object to our handle list.
    FileSystemHandle_t* pHandle = new FileSystemHandle_t (this, pFileSystem, 
        HandleContext, FileSystemHandle_t::HDL_SEARCH, 0);
    
    if (pHandle) {

        // Create a real handle to return to the caller.
        
        hRet = pHandle->CreateHandle (hSearchAPI, m_hVolume);

        if (!hRet) {
            delete pHandle;
            pHandle = NULL;
            hRet = INVALID_HANDLE_VALUE;
        }
    }

    return hRet;
}

HANDLE MountedVolume_t::AllocFileHandle (DWORD HandleContext, DWORD HandleFlags,
        FileSystem_t* pFileSystem, const WCHAR* pFileName, DWORD AccessMode)
{
    HANDLE hRet = INVALID_HANDLE_VALUE;

    // Should never be used to alloc a handle with the HDL_SEARCH type.
    DEBUGCHK (!(FileSystemHandle_t::HDL_SEARCH & HandleFlags));

    // The constructur will automatically add the object to our handle list.
    FileSystemHandle_t* pHandle = new FileSystemHandle_t (this, pFileSystem, 
        HandleContext, HandleFlags, AccessMode);
    
    if (pHandle) {

        // Create a real handle to return to the caller.
        
        hRet = pHandle->CreateHandle (hFileAPI, m_hVolume);

        if (hRet) {

            if (!(FileSystemHandle_t::HDL_PSUEDO & HandleFlags) &&
                !(FileSystemHandle_t::HDL_CONSOLE & HandleFlags)) {

                // Create a file notification handle for this open file.
                pHandle->NotifyCreateHandle (pFileName);
            }

        } else {
        
            delete pHandle;
            pHandle = NULL;
            hRet = INVALID_HANDLE_VALUE;
        }
    }

    return hRet;
}


void MountedVolume_t::RemoveHandle (FileSystemHandle_t* pHandle)
{
    LockVolume ();

    m_HandleList.RemoveItem (pHandle); 
    
    UnlockVolume ();
}

void MountedVolume_t::Destroy ()
{
    // At this point, no more threads can enter the volume.
    DEBUGCHK (VOL_DETACHED & m_VolumeStatus);
    
    DEBUGMSG (ZONE_INIT, (L"FSDMGR!MountedVolume_t::Destroy: destroying volume %p", this));
    
    // Free any threads that were waiting for the volume to come back.
    // Since it is now being destroyed, it will not be coming back.
    ::SetEvent (m_hVolumeReadyEvent);

    // Close all open handles.
    CloseAllHandles ();

    // Clean-up notification handle for the volume
    if (m_hNotifyVolume) {
        ::NotifyDeleteVolume(m_hNotifyVolume);
        m_hNotifyVolume = NULL;
    }

    // Clean-up the filter stack. Only the top level file system or filter
    // object needs to be deleted because each filter deletes the one below
    // it in the chain. FileSystemFilterDriver_t objects will also destroy
    // their NullDisk_t objects because nobody else references these.
    m_pFilterChain->Unload ();

    delete m_pFilterChain;
    m_pFilterChain = NULL;
    
    // Disassociate this volume with the MountableDisk_t object.
    if (m_pDisk) {
        m_pDisk->DetachVolume ();
        m_pDisk = NULL;
    }

    m_VolumeStatus |= VOL_DEINIT;

    // Once the last handle is closed, the final volume handle lock will
    // be removed and the kernel will call FSDMGR_Close to clean up this
    // object.
}


