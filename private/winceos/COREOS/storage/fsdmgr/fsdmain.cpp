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

#ifdef UNDER_CE
BOOL WINAPI DllMain (HANDLE DllInstance, DWORD Reason, LPVOID /* Reserved */)
{
    switch(Reason) {

    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls ((HMODULE)DllInstance);
        DEBUGREGISTER ((HINSTANCE)DllInstance);
        DEBUGMSG (ZONE_INIT, (L"FSDMGR!DllMain: DLL_PROCESS_ATTACH\n"));
        return TRUE;

    case DLL_PROCESS_DETACH:
        DEBUGMSG (ZONE_INIT, (L"FSDMGR!DllMain: DLL_PROCESS_DETACH\n"));
        return TRUE;

    default:
        break;
    }
    return TRUE;
}

static LRESULT AutoLoadBlockDevice (const WCHAR* pName, HKEY hDeviceKey)
{ 
    WCHAR DriverPath[MAX_PATH];
    if (!FsdGetRegistryString (hDeviceKey, g_szFILE_SYSTEM_DRIVER_STRING, DriverPath, MAX_PATH)) {
        return ERROR_FILE_NOT_FOUND;
    }
    
    DEBUGMSG (ZONE_INIT, (L"FSDMGR!AutoLoadBlockDevice: Auto-loading block driver from \"%s\"",
        DriverPath));

    LRESULT lResult = ERROR_SUCCESS;
    if (!MountStore (pName, &BLOCK_DRIVER_GUID, DriverPath)) {
        lResult = FsdGetLastError ();
    }
    return lResult;
}

static LRESULT AutoLoadFileSystem (DWORD CurrentBootPhase, HKEY hRootKey, 
    const WCHAR* pRootKeyName, const WCHAR* pFileSystemName)
{
    HKEY hKeyFSD = NULL;
    LRESULT lResult;

    // Open the key for this file system under the specified root auto-load key.
    lResult = FsdRegOpenSubKey (hRootKey, pFileSystemName, &hKeyFSD);
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }

    // Detect whether or not this is the proper boot phase to load this driver.
    // If there is no boot phase specified in the registry, default to 1.
    DWORD BootPhase = 0;
    if (!FsdGetRegistryValue (hKeyFSD, g_szFSD_BOOTPHASE_STRING, &BootPhase)) {
        if (CurrentBootPhase != 0) {
            BootPhase = CurrentBootPhase;
        } else {
            BootPhase = 1;
        }   
        RegSetValueExW (hKeyFSD, g_szFSD_BOOTPHASE_STRING, 0, REG_DWORD, 
            (LPBYTE)&BootPhase, sizeof(DWORD));
    }

    if (CurrentBootPhase != BootPhase) {
        // The driver is not to be loaded during this boot phase.
        lResult = ERROR_NOT_READY;
        goto exit;
    }

    DEBUGMSG (ZONE_INIT, (L"FSDMGR!AutoLoadFileSystem: CurrentBootPhase=%u, RootKey=%s, FileSystem_t=%s\r\n",
        CurrentBootPhase, pRootKeyName, pFileSystemName));

    // Query the name of the FSD dll.
    WCHAR FSDName[MAX_PATH];
    if (FsdGetRegistryString(hKeyFSD, g_szFILE_SYSTEM_MODULE_STRING, FSDName, MAX_PATH)) {

        // Get the "MountAsXXX" and/or MountFlags settings for this FSD instance.
        DWORD MountFlags = 0;
        g_pMountTable->GetMountSettings(hKeyFSD, &MountFlags);

        // Get the name of a power manager activity event to associate with this FSD.
        WCHAR ActivityName[MAX_PATH];
        if (!FsdGetRegistryString(hKeyFSD, g_szACTIVITY_TIMER_STRING, ActivityName, MAX_PATH)) {
            VERIFY (SUCCEEDED (StringCchCopy (ActivityName, MAX_PATH, g_szDEFAULT_ACTIVITY_NAME)));
        }

        // Allocate a NullDisk_t object for this FSD.
        NullDisk_t* pDisk = new NullDisk_t (ActivityName);
        if (!pDisk) {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit;
        }

        // Use the storage path as the base root reg key.
        lResult = pDisk->AddRootRegKey (g_szSTORAGE_PATH);
        if (ERROR_SUCCESS != lResult) {
            delete pDisk;
            goto exit;
        }

        // The secondary registry key for this FSD is the root auto-load key.
        lResult = pDisk->AddRootRegKey (pRootKeyName);
        if (ERROR_SUCCESS != lResult) {
            delete pDisk;
            goto exit;
        }

        // The sub key for this FSD is the name of the file system.
        lResult = pDisk->SetRegSubKey (pFileSystemName);
        if (ERROR_SUCCESS != lResult) {
            delete pDisk;
            goto exit;
        }

        // Allocate a FileSystemDriver_t object.
        FileSystemDriver_t* pFileSystem = new FileSystemDriver_t (pDisk, FSDName);
        if (!pFileSystem) {
            delete pDisk;
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit;
        }

        // Finally, mount the file system
        lResult = MountFileSystemDriver (pDisk, pFileSystem, MountFlags, FALSE);
        if (ERROR_SUCCESS != lResult) {
            delete pFileSystem;
            delete pDisk;
            goto exit;
        }

    } else {

        // No "FSD" was specified in the registry, so try to load as an auto-load
        // block device instead.
        lResult = AutoLoadBlockDevice (pFileSystemName, hKeyFSD);
    }

exit:
    if (hKeyFSD) {
        FsdRegCloseKey (hKeyFSD);
    }
    return lResult;
}

LRESULT AutoLoadFileSystems (DWORD CurrentBootPhase, DWORD LoadFlags)
{
    DEBUGMSG (ZONE_INIT || ZONE_VERBOSE, (L"FSDMGR!AutoLoadFileSystems: CurrentBootPhase=%u, LoadFlags=%x\r\n",
        CurrentBootPhase, LoadFlags));

    HKEY  hKeyAutoLoad;
    PFSDLOADLIST pFSDLoadList = NULL, pTemp=NULL;
    if (ERROR_SUCCESS == FsdRegOpenKey (g_szAUTOLOAD_PATH, &hKeyAutoLoad)) {
        pFSDLoadList = LoadFSDList (hKeyAutoLoad, LoadFlags);
        while (pFSDLoadList) {
            LRESULT lResult = AutoLoadFileSystem (CurrentBootPhase, hKeyAutoLoad, 
                g_szAUTOLOAD_PATH, pFSDLoadList->szName);
            UNREFERENCED_PARAMETER(lResult);
            DEBUGMSG (ZONE_ERRORS && (ERROR_SUCCESS != lResult) && (ERROR_NOT_READY != lResult),
                (L"FSDMGR!AutoLoadFileSystems: Unable to auto-load HKLM\\%s\\%s; error=%u",
                g_szAUTOLOAD_PATH, pFSDLoadList->szName, lResult));
            pTemp = pFSDLoadList;
            pFSDLoadList = pFSDLoadList->pNext;
            delete pTemp;
        }
        FsdRegCloseKey (hKeyAutoLoad);
    }
    return ERROR_SUCCESS;
}

// Initialize the ROM file system driver. Because the registry is not available
// when the ROM file system loads (the ROM file system is needed to access ROM 
// registry hives), all values for this FSD are hard-coded.
static LRESULT InitializeROMFileSystem ()
{
    LRESULT lResult = ERROR_SUCCESS;
    
    DEBUGMSG (ZONE_INIT || ZONE_VERBOSE, (L"FSDMGR!InitializeROMFileSystem: File System=%s\r\n", g_szROMFS_STRING));
    
    // Allocate a NullDisk_t object for this FSD.
    NullDisk_t* pDisk = new NullDisk_t ();
    if (!pDisk) {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit;
    }

    // Root storage path.
    lResult = pDisk->AddRootRegKey (g_szSTORAGE_PATH);
    if (ERROR_SUCCESS != lResult) {
        delete pDisk;
        goto exit;
    }
    
    // Use the storage path as the base root reg key.
    lResult = pDisk->AddRootRegKey (g_szAUTOLOAD_PATH);
    if (ERROR_SUCCESS != lResult) {
        delete pDisk;
        goto exit;
    }
    
    // The secondary registry key for this FSD is the root auto-load key.
    lResult = pDisk->SetRegSubKey (g_szROMFS_STRING);
    if (ERROR_SUCCESS != lResult) {
        delete pDisk;
        goto exit;
    }
    
    // Allocate a FileSystemDriver_t object.
    FileSystemDriver_t* pFileSystem = new FileSystemDriver_t (pDisk, g_szROMFS_DLL_STRING);
    if (!pFileSystem) {
        delete pDisk;
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit;
    }
    
    // Finally, mount the file system.
    DWORD MountFlags = AFS_FLAG_MOUNTROM | AFS_FLAG_HIDDEN | AFS_FLAG_SYSTEM | AFS_FLAG_PERMANENT;
    lResult = MountFileSystemDriver (pDisk, pFileSystem, MountFlags, FALSE);
    if (ERROR_SUCCESS != lResult) {
        delete pFileSystem;
        delete pDisk;
        goto exit;
    }

exit:
    DEBUGMSG (ZONE_ERRORS && (lResult != ERROR_SUCCESS),
        (L"FSDMGR!InitializeROMFileSystem: FAILED; error=%u", lResult));
    
    return lResult;
}
#endif // UNDER_CE

LRESULT ProcessRebootFlags(MountableDisk_t* pDisk, FileSystemDriver_t* pFSD)
{
    LRESULT lResult = ERROR_SUCCESS;
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCurrentProcessId ());
    DWORD RebootFlags = 0;
    
    if (pFSD->FsIoControl (hProcess, FSCTL_GET_OR_SET_REBOOT_FLAGS, NULL, 0, &RebootFlags, sizeof(DWORD), NULL, NULL)) {

        if (RebootFlags & RebootFlagCleanVolume) {
            
            lResult = FSDMGR_CleanVolume(pDisk, NULL);
            if (lResult != ERROR_SUCCESS) {
                DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!ProcessRebootFlags: failed cleaning volume!"));
            }

            // Clear the CleanVolume flag.  
            //
            RebootFlags &= ~RebootFlagCleanVolume;
            pFSD->FsIoControl (hProcess, FSCTL_GET_OR_SET_REBOOT_FLAGS, &RebootFlags, sizeof(DWORD), NULL, 0, NULL, NULL);
        }       
    }

    return lResult;    
}

// was DeinitEx
LRESULT UnmountFileSystemDriver (MountableDisk_t* pDisk)
{
    MountedVolume_t* pVolume = pDisk->GetVolume ();
    int MountIndex = pVolume->GetMountIndex ();
    
    // Deregister the volume and its name.
    if (!DeregisterAFS (MountIndex) || 
        !DeregisterAFSName (MountIndex)) {
        
        return FsdGetLastError ();
    }

    // Wait for all threads to exit the volume so it will be completely
    // detached from this disk. We can't risk re-mounting the disk while
    // the volume is still attached due to lingering thread activity.
    pDisk->WaitForVolumeDetach (INFINITE);

    return ERROR_SUCCESS;
}

// was InitEx
LRESULT MountFileSystemDriver (MountableDisk_t* pDisk, FileSystemDriver_t* pFSD, DWORD MountFlags, BOOL fDoFormat)
{
    HANDLE hProcess = reinterpret_cast<HANDLE> (GetCurrentProcessId ());
    LRESULT lResult = pFSD->Load ();
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }

    MountedVolume_t* pVolume = new MountedVolume_t (g_pMountTable, pDisk, pFSD, g_dwWaitIODelay, MountFlags);
    if (!pVolume) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    
    // Associate the new volume with the specified MountableDisk_t object.
    pDisk->AttachVolume (pVolume);

    if (fDoFormat) {
        lResult = pFSD->Format ();
        // Format was requested, but the format failed. There's not much we can do here,
        // but we should probably continue with the mount and it might fail if the volume
        // is not formatted.
        if (lResult != ERROR_SUCCESS) {
            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!MountFileSystemDriver: failed formatting disk before mounting!"));
        }
    }

    // Mount the disk by calling the FSD's mount function. During this call, the FSD should
    // call FSDMGR_RegisterVolume to finalize the mount process.
    if (!pFSD->Mount ()) {
        lResult = ::FsdGetLastError ();
        DEBUGMSG ( ZONE_VERBOSE, (TEXT("FSDMGR!MountFileSystemDriver: failed FSD mounting 0x%x!"),lResult));


        //Filesystem Mount failed. Is it because of disk not available?
        if (!pDisk->IsAvailable ())
        {
            //return this status so that if disk is not available, the store wouldnt be preserved.
            DEBUGMSG ( ZONE_VERBOSE, (TEXT("FSDMGR!MountFileSystemDriver: failed FSD mounting  Disk Not available !")));
            lResult = ERROR_DEV_NOT_EXIST;
        }

        // In case FSDMGR_RegisterVolume was called but the FSD_MountDisk export failed
        // anyway, make sure the volume is detached.
        int MountIndex = pVolume->GetMountIndex ();
        if (g_pMountTable->IsValidIndex (MountIndex)) {
            //
            // Here we successfully registered the file system but failed to mount.
            // The volume will be deleted when we deregister the volume and the
            // reference count hits zero.  Kernel will call FSDMGR_Close.
            //
            g_pMountTable->DeregisterVolume (MountIndex, hProcess, TRUE);
            g_pMountTable->DeregisterVolumeName (MountIndex, hProcess);
            DEBUGMSG ( ZONE_VERBOSE, (TEXT("FSDMGR!MountFileSystemDriver: Deregistering Volume !")));
        }
        else 
        {
            //
            // Here we never registered the file system and we failed to mount.  We
            // simply need to delete the volume ourselves here.
            //
            pDisk->DetachVolume ();
            delete pVolume;
            pVolume = NULL;
            DEBUGMSG ( ZONE_VERBOSE, (TEXT("FSDMGR!MountFileSystemDriver: Detaching and deleting Volume !")));
        }
        return lResult;
    }

    ProcessRebootFlags(pDisk, pFSD);

    // At this point it is up to the FSD to call FSDMGR_RegisterVolume to complete the mount.
    // If this does not occur before we call FinalizeMount, we will set the m_fDelayFinalize
    // member to TRUE.

    lResult = pVolume->FinalizeMount ();
    if (ERROR_SUCCESS != lResult) {
        DEBUGMSG ( ZONE_VERBOSE, (TEXT("FSDMGR!MountFileSystemDriver: failed finalizing volume 0x%x!"),lResult));
        // Force cleanup due to failure.
        int MountIndex = pVolume->GetMountIndex ();
        DEBUGCHK (g_pMountTable->IsValidIndex (MountIndex));
        g_pMountTable->DeregisterVolume (MountIndex, hProcess, TRUE);
        g_pMountTable->DeregisterVolumeName (MountIndex, hProcess);
    }

    return lResult;
}

FSDLOADLIST* LoadFSDList (HKEY hKey, DWORD LoadFlags, const WCHAR* pRegPath,
    FSDLOADLIST* pListTail, BOOL fReverse)
{
    WCHAR RegSubKey[MAX_PATH];
    DWORD SubKeyChars = MAX_PATH;
    FSDLOADLIST* pFSDLoadList = pListTail;

    for (DWORD Index = 0; (ERROR_SUCCESS == FsdRegEnumKey (hKey, Index, RegSubKey, &SubKeyChars));
         Index ++) {

        // Reset in/out buffer size.
        SubKeyChars = MAX_PATH;

        HKEY hSubKey = NULL;
        if (ERROR_SUCCESS != FsdRegOpenSubKey (hKey, RegSubKey, &hSubKey)) {
            continue;
        }

        DWORD LoadOrder;
        if (!FsdGetRegistryValue (hSubKey, g_szFSD_ORDER_STRING, &LoadOrder)) {
            LoadOrder = INFINITE;
        }

        DWORD ItemFlag;
        if (!FsdGetRegistryValue (hSubKey, g_szFSD_LOADFLAG_STRING, &ItemFlag)) {
            ItemFlag = 0;
        }

        if (!(ItemFlag & LOAD_FLAG_ASYNC) && !(ItemFlag & LOAD_FLAG_SYNC)) {
            // Default to asynchronous loading.
            ItemFlag |= LOAD_FLAG_ASYNC;
        }

        if (!(ItemFlag & LoadFlags)) {
            // The flags under this item do not match the specified load flags value
            // passed by the caller, so don't load this one.
            FsdRegCloseKey (hSubKey);
            hSubKey = NULL;
            continue;
        }

        // Search the existing load list, and insert a new item at the correct
        // position based on its order value.
        PFSDLOADLIST pTemp = pFSDLoadList;
        PFSDLOADLIST pPrev = NULL;
        while (pTemp) {            
            if  (fReverse) {
                if (pTemp->dwOrder < LoadOrder) {
                    break;
                }
            } else {    
                if (pTemp->dwOrder > LoadOrder) {
                    break;
                }   
            }   
            pPrev = pTemp;                  
            pTemp = pTemp->pNext;
        }

        // Allocate a new list node and insert it at the proper list location. 
        // It is the caller's responsibility to free all list nodes.
        pTemp = new FSDLOADLIST;
        if (pTemp) {
            if (pRegPath) {
                StringCbCopy (pTemp->szPath, sizeof (pTemp->szPath), pRegPath);
            }
            pTemp->dwOrder = LoadOrder;
            StringCbCopy (pTemp->szName, sizeof (pTemp->szName), RegSubKey);            
            if (pPrev) {
                pTemp->pNext = pPrev->pNext;
                pPrev->pNext = pTemp;
            } else {
                pTemp->pNext = pFSDLoadList;
                pFSDLoadList = pTemp;
            }
        }
        
        FsdRegCloseKey (hSubKey);
        hSubKey = NULL;
    }

    return pFSDLoadList;
}

EXTERN_C LRESULT STOREMGR_Initialize ()
{
    LRESULT lResult;

    DEBUGMSG (ZONE_INIT, (L"FSDMGR!STOREMGR_Initialize\r\n"));

    lResult = InitializeVirtualRoot ();
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }
    lResult = InitializeCanonicalizerAPI ();
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }    
    lResult = InitializeFileAPI ();
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }
    lResult = InitializeVolumeAPI ();
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }
    lResult = InitializeSearchPI ();
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }
    lResult = InitializeStoreAPI ();
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }
    lResult = InitializeBlockDeviceAPI ();
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }

#ifdef UNDER_CE    
    lResult = InitializeROMFileSystem ();
    if (ERROR_SUCCESS != lResult) {
        return lResult;
    }
#endif    

    return lResult;
}

#ifdef UNDER_CE

EXTERN_C LRESULT STOREMGR_StartBootPhase (DWORD BootPhase)
{
    static DWORD PrevBootPhase = (DWORD)-1;
    LRESULT lResult;

    DEBUGCHK (g_pMountTable); // Must have already initialized storage manager.

    DEBUGMSG (ZONE_INIT, (L"FSDMGR!STOREMGR_StartBootPhase BootPhase=%i (PrevBootPhase=%i)\r\n", 
        BootPhase, PrevBootPhase));

    g_pMountTable->SetBootPhase (BootPhase);
    if (2 == BootPhase) {
        g_pMountTable->FinalizeBootSetup ();
    }

    lResult = InitializeStorageManager (BootPhase);
    if (ERROR_SUCCESS != lResult) {
        DEBUGMSG(ZONE_ERRORS, (L"FSDMGR!STOREMGR_StartBootPhase: InitializeStorageManager failed; error=%u\r\n", lResult));
        return lResult;
    }
    
    lResult = AutoLoadFileSystems (BootPhase, LOAD_FLAG_SYNC);
    if (ERROR_SUCCESS != lResult) {
        DEBUGMSG(ZONE_ERRORS, (L"FSDMGR!STOREMGR_StartBootPhase: AutoLoadFileSystems failed; error=%u\r\n", lResult));
        return lResult;
    }

    // Save the last bootphase so we can detect skips.
    PrevBootPhase = BootPhase;

    return lResult;
}

// NOTE: The mount table must be locked before calling this function.
static void ResumeVolumes ()
{
    g_pMountTable->LockTable ();

    // Locate the boot volume index. The boot volume will be notified
    // separately from the other volumes.
    HANDLE hBootVolume = NULL;
    int BootVolumeIndex = g_pMountTable->GetBootVolumeIndex ();
    if (INVALID_MOUNT_INDEX != BootVolumeIndex) {
        g_pMountTable->GetVolumeFromIndex (BootVolumeIndex, &hBootVolume);
    }    

    // Power-on the boot volume before all other volumes.
    if (hBootVolume) {
        __try {
            AFS_NotifyMountedFS (hBootVolume, FSNOTIFY_POWER_ON);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }    

    int MaxIndex = g_pMountTable->GetHighMountIndex ();
    for (int i = 0; i <= MaxIndex; i++) {
        
        if (i == BootVolumeIndex) {
            // Skip the boot volume.
            continue;
        }

        HANDLE hVolume;
        LRESULT lResult = g_pMountTable->GetVolumeFromIndex (i, &hVolume);
        if (ERROR_SUCCESS == lResult) {
            // Notify the volume.
            __try {               
                AFS_NotifyMountedFS (hVolume, FSNOTIFY_POWER_ON);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }

    g_pMountTable->UnlockTable ();
}

// NOTE: The mount table must be locked before calling this function.
static void SuspendVolumes ()
{
    g_pMountTable->LockTable ();

    // Locate the boot volume index. The boot volume will be notified
    // separately from the other volumes.
    HANDLE hBootVolume = NULL;
    int BootVolumeIndex = g_pMountTable->GetBootVolumeIndex ();
    if (INVALID_MOUNT_INDEX != BootVolumeIndex) {
        g_pMountTable->GetVolumeFromIndex (BootVolumeIndex, &hBootVolume);
    }    

    int MaxIndex = g_pMountTable->GetHighMountIndex ();
    for (int i = 0; i <= MaxIndex; i++) {
        
        if (i == BootVolumeIndex) {
            // Skip the boot volume.
            continue;
        }

        HANDLE hVolume;
        LRESULT lResult = g_pMountTable->GetVolumeFromIndex (i, &hVolume);
        if (ERROR_SUCCESS == lResult) {
            // Notify the volume.
            __try {               
                AFS_NotifyMountedFS (hVolume, FSNOTIFY_POWER_OFF);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }

    // Power-off the boot volume after all other volumes.
    if (hBootVolume) {
        __try {
            AFS_NotifyMountedFS (hBootVolume, FSNOTIFY_POWER_OFF);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    g_pMountTable->UnlockTable ();
}

static void NotifyVolumes (DWORD Flags)
{
    g_pMountTable->LockTable ();

    int MaxIndex = g_pMountTable->GetHighMountIndex ();
    for (int i = 0; i <= MaxIndex; i++) {
        
        HANDLE hVolume;
        LRESULT lResult = g_pMountTable->GetVolumeFromIndex (i, &hVolume);
        if (ERROR_SUCCESS == lResult) {
            // Notify the volume.
            __try {               
                AFS_NotifyMountedFS (hVolume, Flags);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }
    }

    g_pMountTable->UnlockTable ();
}

EXTERN_C void STOREMGR_NotifyFileSystems (DWORD Flags)
{
    DEBUGMSG(ZONE_POWER, (L"FSDMGR!STOREMGR_NotifyFileSystems Flags=%x (%s)\r\n", Flags,
        (FSNOTIFY_POWER_OFF & Flags) ? L"FSNOTIFY_POWER_OFF" : L"FSNOTIFY_POWER_ON"));

    if (FSNOTIFY_POWER_OFF & Flags) {
        
        DEBUGCHK (!(FSNOTIFY_POWER_ON & Flags));        

        // NOTE: Always adhere to this ordering when acquiring both the storage
        // manager critical section and the mount table critical section. 
        LockStoreMgr ();

        SuspendVolumes ();
        SuspendStores ();

        g_pMountTable->LockTable ();

    } else if (FSNOTIFY_POWER_ON & Flags) {
    
        g_pMountTable->UnlockTable ();

        ResumeStores ();
        ResumeVolumes ();

        UnlockStoreMgr ();

    } else {
   
        NotifyVolumes (Flags);
    }
}

// AKA CeRegisterFileSystemNotification. The HWND parameter is interpreted as a
// SHELLFILECHANGEFUNC_t function pointer, which must be a PSL.
EXTERN_C BOOL STOREMGR_RegisterFileSystemFunction (SHELLFILECHANGEFUNC_t pFn)
{
    g_pMountTable->RegisterFileSystemNotificationFunction (pFn);
    return TRUE;
}

EXTERN_C void STOREMGR_ProcNotify (DWORD Flags, HPROCESS hProc, HTHREAD /* hThread */)
{
    switch (Flags) {

    case DLL_MEMORY_LOW:
        break;

    case DLL_PROCESS_DETACH:       
        FS_ProcessCleanupVolumes (hProc);
        break;
        
    case DLL_PROCESS_EXITING:
        break;

    default:
        break;
        
    }
}

// Get CEOIDINFOEX information for a mount point OID.
EXTERN_C BOOL STOREMGR_GetOidInfoEx (int MountIndex, CEOIDINFOEX* pOidInfoEx)
{
    LRESULT lResult;
    HANDLE hVolume;
    DWORD MountFlags;
    lResult = g_pMountTable->GetVolumeFromIndex (MountIndex, &hVolume, &MountFlags);    
    if (ERROR_SUCCESS != lResult) {
        SetLastError (lResult);
        return FALSE;
    }

    // Determine the oid for the root file system; it is the parent for all
    // mount points.
    int RootIndex = g_pMountTable->GetRootVolumeIndex ();
    CEOID OidParent = INVALID_OID;
    if (MountTable_t::IsValidIndex (RootIndex) &&
        (MountIndex != RootIndex)) {
        
        OidParent = (CEOID)((SYSTEM_MNTVOLUME << 28) | RootIndex);
    }

    __try {

        DEBUGCHK (pOidInfoEx->wVersion == CEOIDINFOEX_VERSION);
        pOidInfoEx->wObjType = OBJTYPE_DIRECTORY;
        pOidInfoEx->infDirectory.oidParent = OidParent;
        pOidInfoEx->infDirectory.dwAttributes = FILE_ATTRIBUTE_DIRECTORY;
        if (!(AFS_FLAG_PERMANENT & MountFlags)) {
            pOidInfoEx->infDirectory.dwAttributes |= FILE_ATTRIBUTE_TEMPORARY;
        }            
        if (AFS_FLAG_SYSTEM & MountFlags) {
            pOidInfoEx->infDirectory.dwAttributes |= FILE_ATTRIBUTE_SYSTEM;
        }
        if (AFS_FLAG_HIDDEN & MountFlags) {
            pOidInfoEx->infDirectory.dwAttributes |= FILE_ATTRIBUTE_HIDDEN;
        }

        // Copy the directory name.
        pOidInfoEx->infDirectory.szDirName[0] = L'\\';
        g_pMountTable->GetMountName (MountIndex, &pOidInfoEx->infDirectory.szDirName[1], 
            (sizeof (pOidInfoEx->infDirectory.szDirName) / sizeof (WCHAR)) - 1);

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        lResult = ERROR_INVALID_PARAMETER;
    }
    
    return (ERROR_SUCCESS == lResult);
}

#endif // UNDER_CE
