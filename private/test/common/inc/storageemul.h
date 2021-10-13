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
#pragma once

#include <windows.h>

#define MAX_EMUL_STORES 5

typedef BOOL (WINAPI *PFNLogFunction)(const WCHAR * msg, ...);

#define SE_MOUNT_FORCE              1
#define SE_MOUNT_FAILIFPRESENT      2
#define SE_MOUNT_VALID (SE_MOUNT_FORCE | SE_MOUNT_FAILIFPRESENT)

class StorageEmulator
{
public:
    // CreateVolume will prepare the registry keys and such, but will not
    // activate the device.
    HRESULT CreateVolume(size_t cbSizeInMB, const TCHAR * tszTemplate = NULL, BOOL bReadOnly=FALSE);
    
    // MountVolume will update the mount folder in the partition profile and
    // then ActivateDevice, create the store and partition, and mount the partition
    // (formatting the partition if necessary)
    HRESULT MountVolume(const TCHAR * tszMountPoint, DWORD dwMountFlags = 0);

    // UnmountVolume will call DismountVolume, close the store and partition, and
    // deactivate the driver. This is needed in case the mount point changes.
    HRESULT UnmountVolume();

    // Cleanup will take care of Cleanup; if the semaphore has been created but
    // not acquired, set fReleaseSemaphore to false (this only happens in CreateVolume
    // if the semaphore fails to be acquired).
    HRESULT Cleanup(BOOL fReleaseSemaphore = TRUE);

    // Copy the backing file for the store into a separate location
    HRESULT SaveTemplate(const TCHAR * tszTemplate, BOOL fFailIfExists);

    // set the volume to be read only
    HRESULT SetVolumeReadOnly(BOOL bReadOnly);
    
    StorageEmulator();
    ~StorageEmulator();

    // If SetLogMethod is called, the new function will be used to log all errors.
    // Use this if the tool will be used in a test that outputs with Kato.
    // The default logger will just output to debug.
    HRESULT SetLogMethod(PFNLogFunction Logger);

    const TCHAR * GetDeviceName();

private:

    // Registry Helper functions
    HRESULT RegSetString(HKEY hRegKey, const TCHAR * tszName, const TCHAR * tszValue);
    HRESULT RegSetValue(HKEY hRegKey, const TCHAR * tszName, DWORD dwValue);
    HRESULT RegReadString(HKEY hRegKey, const TCHAR * tszName, TCHAR ** ptszValue);
    HRESULT RegReadValue(HKEY hRegKey, const TCHAR * tszName, DWORD * pdwValue);
    HRESULT DetermineStoreName();
    HRESULT FindDeviceName(const TCHAR * tszKeyName);
    HRESULT GetDrvRegKey(TCHAR * tszDrvRegKey, DWORD dwCchDest);
    HRESULT GetPartRegKey(TCHAR * tszPartRegKey, DWORD dwCchDest);
    HRESULT GrabIndexMutex();
    HRESULT ReleaseIndexMutex();
    HRESULT CleanupMountPoint(BOOL fCheckOnly);
    HRESULT CleanupDirectory(const TCHAR * tszDirectory);

    // The name to use in the reg keys for the driver and partition (MMStore_n)
    TCHAR m_tszStoreName[32];
    // The actual "DSKn:" name
    TCHAR m_tszDeviceName[6];
    // The location of the file the ramdisk will save to when unloaded (\\MMStore_n.ram,
    // or the template file if there is one).
    TCHAR m_tszStoreBacking[MAX_PATH + 1];

    // used for subsequent MountVolume call without a tszMountPoint
    TCHAR m_tszDefaultMountPoint[MAX_PATH+1];
    
    // The size of the store
    DWORD m_dwStoreSizeCb;

    // should the storage be read only
    BOOL m_bReadOnly;
    
    // Open registry keys for the driver and partition
    HKEY m_hDrvRegKey;
    HKEY m_hPartRegKey;

    // The device (returned from ActivateDevice and used in DeactivateDevice)
    HANDLE m_hDev;

    // The naming semaphore is used to track how many instances of the Storage Emulator
    // are open and also what number to use in the store name
    HANDLE m_hNamingSemaphore;

    // The index mutex is how we accurately determine the proper index to use in the profile
    // and such (so that settings made in this instance don't have any affect on settings
    // for other instances)
    HANDLE m_hIndexMutex;

    // The partition and store handles
    HANDLE m_hPartition;
    HANDLE m_hStore;

    // The logging function. This can be replaced through SetLogMethod.
    PFNLogFunction m_pfnLogger;
};
