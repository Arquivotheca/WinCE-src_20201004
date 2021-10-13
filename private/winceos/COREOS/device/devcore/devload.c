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
/* File:    devload.c
 *
 * Purpose: WinCE device manager initialization and built-in device management
 *
 */
#include <windows.h>
#include <pnp.h>
#include <devload.h>
#include <windev.h>
#include <fsioctl.h>
#include <errorrep.h>

#include "devmgrp.h"
#include <reflector.h>
#include "devmgrif.h"
#include "devzones.h"
#include "devinit.h"

VOID DeleteActiveKey( LPCWSTR ActivePath);

volatile LONG v_NextDeviceNum;            // Used to create active device list

#ifdef DEBUG
// This routine displays currently loaded device drivers.
static void
DebugDumpDevices(void)
{
    fsdev_t *lpTrav;
    
    DEBUGMSG(TRUE, (TEXT("DEVICE: Name   Load Order\r\n")));
    //
    // Display the list of devices in ascending load order.
    //
    EnterCriticalSection(&g_devcs);
    for (lpTrav = (fsdev_t *)g_DevChain.Flink;
         lpTrav != (fsdev_t *)&g_DevChain;
         lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
        if (lpTrav->pszDeviceName != NULL) {
            DEBUGCHK(lpTrav->pszDeviceName != NULL);
            DEBUGCHK(lpTrav->pszBusName != NULL);
            DEBUGMSG(TRUE, (TEXT("DEVICE: named device 0x%08x: '%s' (AKA '%s'), bus '%s'\r\n"), 
                lpTrav, 
                lpTrav->pszLegacyName != NULL ? lpTrav->pszLegacyName : L"<NoLegacyName>", 
                lpTrav->pszDeviceName, lpTrav->pszBusName));
        } else {
            DEBUGMSG(TRUE, (TEXT("DEVICE: unnamed device 0x%08x: lib %08x, bus '%s'\r\n"),
                                   lpTrav, lpTrav->hLib, 
                                   lpTrav->pszBusName != NULL ? lpTrav->pszBusName : L"<Unnamed>"));
        }
    }
    LeaveCriticalSection(&g_devcs);
}
#endif      // DEBUG



//
// FindDeviceByHandle - function to check if passed in device handle is actually a
// valid registered device.
//
// Return TRUE if it is a valid registered device, FALSE otherwise.
//
fsdev_t *
FindDeviceByHandle(
    fsdev_t * lpdev
    )
{
    fsdev_t * lpTrav;
    BOOL fFound = FALSE;

    EnterCriticalSection(&g_devcs);

    for (lpTrav = (fsdev_t *)g_DevChain.Flink;
         lpTrav != (fsdev_t *)&g_DevChain;
         lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
        if (lpTrav == lpdev) {
            fFound = TRUE;
            AddDeviceRef(lpdev);
            break;
        }
    }

    LeaveCriticalSection(&g_devcs);
    
    return (fFound? lpdev: NULL);
}

// look up an existing device based on its name
static fsdev_t *
FindDevice(fsdev_t * lpTrav, fsdev_t * lpEnd, LPCWSTR pszName, NameSpaceType nameType)
{
    DEBUGCHK(pszName != NULL);

    EnterCriticalSection (&g_devcs);
    for (;
         lpTrav != lpEnd;
         lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
        if(nameType == NtLegacy) {
            if(lpTrav->pszLegacyName != NULL && _wcsicmp(lpTrav->pszLegacyName, pszName) == 0) {
                break;
            }
        } else if(nameType == NtDevice) {
            if(lpTrav->pszDeviceName != NULL && _wcsicmp(lpTrav->pszDeviceName, pszName) == 0) {
                break;
            }
        } else if(nameType == NtBus) {
            if(lpTrav->pszBusName != NULL && _wcsicmp(lpTrav->pszBusName, pszName) == 0) {
                break;
            }
        }
    }

    // did we come back to the beginning?
    if(lpTrav == lpEnd) {
        // yes -- no match
        lpTrav = NULL;
    }
    else {
        AddDeviceRef(lpTrav);
    }
    LeaveCriticalSection (&g_devcs);

    return lpTrav;
}

// look up a candidate device based on its name
fsdev_t *
FindDeviceCandidateByName(LPCWSTR pszName, NameSpaceType nameType)
{
    return FindDevice((fsdev_t*)g_DevCandidateChain.Flink, (fsdev_t*)&g_DevCandidateChain, pszName, nameType);
}

// look up an existing device based on its name
fsdev_t *
FindDeviceByName(LPCWSTR pszName, NameSpaceType nameType)
{
    return FindDevice((fsdev_t*)g_DevChain.Flink, (fsdev_t*)&g_DevChain, pszName, nameType);
}
    
//  @func BOOL | I_CheckRegisterDeviceSafety(LPCWSTR lpszType, DWORD dwIndex,LPCWSTR lpszLib, PDWORD pdwFlags) 
// Check RegisterDevice is safe or not. 
// @parm pdwFlags is return as loading flag.
BOOL 
I_CheckRegisterDeviceSafety (LPCWSTR lpszType, DWORD dwIndex,LPCWSTR lpszLib, PDWORD pdwFlags)
{
    const LPCWSTR pszRegisteredDriversListPath = TEXT("Drivers\\RegisteredDevice");
    HKEY    Key;
    DWORD   dwFlags = DEVFLAGS_TRUSTEDCALLERONLY ;
    BOOL    fFoundEntry = FALSE;
    // Open key to be cleaned
    DWORD status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pszRegisteredDriversListPath, 0, 0, &Key);
    PREFAST_ASSERT(lpszLib!=NULL);
    
    if (status == ERROR_SUCCESS) {
        DWORD dwRegIndex = 0;
        BOOL  fCheckIndex = TRUE;
        
        while (!fFoundEntry){
            HKEY deviceKey;
            WCHAR DevName[DEVKEY_LEN];
            DWORD DevNameLength = _countof(DevName);
            status = RegEnumKeyEx(Key, dwRegIndex, DevName , &DevNameLength, NULL, NULL, NULL, NULL);
            if (status== ERROR_SUCCESS ) {
                DevName[DEVKEY_LEN-1] = 0;
                status = RegOpenKeyEx(Key,DevName,0,0,&deviceKey) ;
                if (status== ERROR_SUCCESS ) {
                    DWORD dwType;
                    // Read DLL Name.
                    DevNameLength = sizeof(DevName);
                    status = RegQueryValueEx(deviceKey,DEVLOAD_DLLNAME_VALNAME,NULL,&dwType, (PBYTE) DevName, &DevNameLength);
                    DevNameLength /= sizeof(WCHAR); // Convert back to charactors.
                    if (status == ERROR_SUCCESS && dwType == DEVLOAD_DLLNAME_VALTYPE && _wcsnicmp(lpszLib,DevName,DevNameLength)== 0 ) {
                        // Checking for the prefix
                        DevNameLength = sizeof(DevName);
                        status = RegQueryValueEx(deviceKey,DEVLOAD_PREFIX_VALNAME,NULL,&dwType, (PBYTE) DevName, &DevNameLength);
                        DevNameLength /= sizeof(WCHAR); // Convert back to charactors.
                        if (status == ERROR_SUCCESS && dwType == DEVLOAD_PREFIX_VALTYPE && _wcsnicmp(lpszType,DevName,DevNameLength)== 0 ) {
                            DWORD dwIndexInReg=MAXDWORD;
                            DWORD dwLength = sizeof(dwIndexInReg);
                            status = RegQueryValueEx(deviceKey,DEVLOAD_INDEX_VALNAME,NULL,&dwType, (PBYTE) &dwIndexInReg, &dwLength);
                            if (status == ERROR_SUCCESS && dwType == DEVLOAD_INDEX_VALTYPE && dwIndexInReg==dwIndex  || 
                                    !fCheckIndex && status != ERROR_SUCCESS ) {
                                // We found entry. Then We read dwFlags.
                                fFoundEntry = TRUE;
                                DevNameLength = sizeof(dwFlags);
                                status = RegQueryValueEx(deviceKey,DEVLOAD_FLAGS_VALNAME,NULL,&dwType, (PBYTE) &dwFlags, &DevNameLength);
                                if (!(status == ERROR_SUCCESS &&  dwType == DEVLOAD_FLAGS_VALTYPE) ) { // If fails we use default.
                                    dwFlags = DEVFLAGS_TRUSTEDCALLERONLY ;
                                }
                            }
                        }
                    }
                    RegCloseKey(deviceKey);
                }
                dwRegIndex ++ ;
            }
            else if (fCheckIndex) {
                fCheckIndex = FALSE;
                dwRegIndex = 0;
            }
            else
                break;
        };
        RegCloseKey(Key);;
    }
    
    DEBUGMSG(ZONE_WARNING && !fFoundEntry , (L"Can't find entry for (%s) in (%s) \r\n",lpszLib, pszRegisteredDriversListPath));
    if ((dwFlags & (DEVFLAGS_TRUSTEDCALLERONLY) )!= 0 ) { // Trusted Only
        ERRORMSG(1, (L"'%s' uses deprecated flag pszRegisteredDriversListPath", pszRegisteredDriversListPath));
    }
    
    if (pdwFlags!=NULL)
        *pdwFlags = dwFlags;
    return TRUE;
}
// Function to call a newly registered device's post initialization
// device I/O control function.
VOID
DevicePostInit(
    LPCTSTR DevName,
    DWORD  dwIoControlCode,
    HANDLE hActiveDevice,
    LPCWSTR RegPath
    )
{
    HANDLE hDev = INVALID_HANDLE_VALUE;
    HKEY hk = NULL;
    BOOL ret;
    POST_INIT_BUF PBuf;
    DWORD status;

    DEBUGCHK(DevName != NULL);
    DEBUGCHK(hActiveDevice != NULL);
    DEBUGCHK(RegPath != NULL);

    status = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        RegPath,
        0,
        0,
        &hk);
    if (status) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!DevicePostInit RegOpenKeyEx(%s) returned %d.\r\n"),
                  RegPath, status));
        goto done;
    }

    DEBUGMSG(ZONE_ACTIVE,
        (TEXT("DEVICE!DevicePostInit calling CreateFile(%s)\r\n"), DevName));
    hDev = CreateFile(
                DevName,
                GENERIC_READ|GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);
    if (hDev == INVALID_HANDLE_VALUE) {
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!DevicePostInit CreateFile(%s) failed %d\r\n"),
            DevName, GetLastError()));
        goto done;
    }

    DEBUGMSG(ZONE_ACTIVE,
        (TEXT("DEVICE!DevicePostInit calling DeviceIoControl(%s, %d)\r\n"),
         DevName, dwIoControlCode));
    PBuf.p_hDevice = hActiveDevice;
    PBuf.p_hDeviceKey = hk;
    ret = DeviceIoControl(
              hDev,
              dwIoControlCode,
              &PBuf,
              sizeof(PBuf),
              NULL,
              0,
              NULL,
              NULL);
    DEBUGMSG(ZONE_ACTIVE,
        (TEXT("DEVICE!DevicePostInit DeviceIoControl(%s, %d) "),
         DevName, dwIoControlCode));
    if (ret == TRUE) {
        DEBUGMSG(ZONE_ACTIVE, (TEXT("succeeded\r\n")));
    } else {
        DEBUGMSG(ZONE_ACTIVE, (TEXT("failed\r\n")));
    }

done:
    if(hk != NULL) RegCloseKey(hk);
    if(hDev != INVALID_HANDLE_VALUE) CloseHandle(hDev);
}   // DevicePostInit


// This routine deletes a device driver's active key, presumably because the driver hasn't
// loaded successfully or has been unloaded.  If the key can't be removed, it deletes 
// values within the key to make sure that they won't somehow be recycled accidentally.
VOID
DeleteActiveKey(
    LPCWSTR ActivePath
    )
{
    DWORD status;

    DEBUGCHK(ActivePath != NULL);
    DEBUGCHK(ActivePath[0] != 0);

    status = RegDeleteKey(HKEY_LOCAL_MACHINE, ActivePath);
    if (status != ERROR_SUCCESS) {
        HKEY hActiveKey;
    
        DEBUGMSG(ZONE_ACTIVE|ZONE_WARNING,(TEXT("DEVICE!DeleteActiveKey: RegDeleteKey failed %d\n"), status));

        // Open active key
        status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, ActivePath, 0, KEY_WRITE, &hActiveKey);
        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
               (TEXT("DEVICE!DeleteActiveKey: RegOpenKeyEx(%s) returned %d\r\n"),
               ActivePath, status));
        } else {
            // Delete some values so we no longer mistake it for a valid active key.
            RegDeleteValue(hActiveKey, DEVLOAD_CLIENTINFO_VALNAME);
            RegDeleteValue(hActiveKey, DEVLOAD_HANDLE_VALNAME);
            RegDeleteValue(hActiveKey, DEVLOAD_DEVNAME_VALNAME);
            RegDeleteValue(hActiveKey, DEVLOAD_DEVKEY_VALNAME);
            RegDeleteValue(hActiveKey, DEVLOAD_PNPID_VALNAME);
            RegCloseKey(hActiveKey);
        }
    }
}   // DeleteActiveValues

// This routine reads mandatory and optional values that the caller of 
// ActivateDevice() may include in the device key.  It returns ERROR_SUCCESS
// if all required values are present, or a Win32 error code if there is a
// problem.
DWORD
RegReadActivationValues(LPCWSTR RegPath, pRegActivationValues_t pav)
{
    HKEY DevKey = NULL;
    DWORD status;
    DWORD ValLen;
    DWORD ValType;

    DEBUGCHK(RegPath != NULL);
    DEBUGCHK(pav != NULL);

    // Get the required (dll) and optional (prefix, index, flags, and context) values.
    status = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        RegPath,
        0,
        KEY_READ,
        &DevKey);
    if (status) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!RegReadActivationValues RegOpenKeyEx(%s) returned %d.\r\n"),
                  RegPath, status));
        goto done;
    }

    // Read DLL name
    ValLen = sizeof(pav->DevDll);
    status = RegQueryValueEx(
        DevKey,
        s_DllName_ValName,
        NULL,
        &ValType,
        (PUCHAR)pav->DevDll,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\DllName) returned %d\r\n"),
                  RegPath, status));
        goto done;
    }
    pav->DevDll[_countof(pav->DevDll) - 1] = 0;  // enforce null termination

    // Read Flags value, if it exists.
    ValLen = sizeof(pav->Flags);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_FLAGS_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)&pav->Flags,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\Flags) returned %d\r\n"),
                  RegPath, status));
        pav->Flags = DEVFLAGS_NONE;  // default is no flags set
    }

    // Read prefix value, if it exists.
    ValLen = sizeof(pav->Prefix);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_PREFIX_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)pav->Prefix,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE || ZONE_WARNING,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\Prefix) returned %d\r\n"),
                  RegPath, status));
        pav->Prefix[0] = 0;
    } else {
        size_t cchPrefix = 0;
        VERIFY(SUCCEEDED(StringCchLength(pav->Prefix, _countof(pav->Prefix), &cchPrefix)));
        if(pav->Prefix[_countof(pav->Prefix) - 1] != 0 
        || (cchPrefix != PREFIX_CHARS && wcslen(pav->Prefix) != 0)) {
            DEBUGMSG(ZONE_ACTIVE,
                     (TEXT("DEVICE!RegReadActivationValues: ignoring invalid Prefix in (%s)\r\n"),
                      RegPath));
            pav->Prefix[0] = 0;
        }
    }
    pav->Prefix[_countof(pav->Prefix) - 1] = 0;  // enforce null termination

    // Read bus prefix value, if it exists.
    ValLen = sizeof(pav->BusPrefix);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_BUSPREFIX_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)pav->BusPrefix,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE || ZONE_WARNING,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\BusPrefix) returned %d\r\n"),
                  RegPath, status));
        pav->BusPrefix[0] = 0;
    } else {
        size_t cchBusPrefix = 0;
        VERIFY(SUCCEEDED(StringCchLength(pav->BusPrefix, _countof(pav->BusPrefix), &cchBusPrefix)));
        if(pav->BusPrefix[_countof(pav->BusPrefix) - 1] != 0 
        || (cchBusPrefix != PREFIX_CHARS && wcslen(pav->BusPrefix) != 0)) {
            DEBUGMSG(ZONE_ACTIVE || ZONE_WARNING,
                     (TEXT("DEVICE!RegReadActivationValues: ignoring invalid BusPrefix in (%s)\r\n"),
                      RegPath));
            pav->BusPrefix[0] = 0;
        } else if(pav->Prefix[0] != 0 && wcsicmp(pav->Prefix, pav->BusPrefix) != 0) {
            DEBUGMSG(ZONE_ACTIVE || ZONE_ERROR,
                     (TEXT("DEVICE!RegReadActivationValues: prefix '%s' mismatch with bus prefix '%s' in (%s)\r\n"),
                      pav->Prefix, pav->BusPrefix, RegPath));
            status = ERROR_INVALID_PARAMETER;
            goto done;
        }
    }
    pav->BusPrefix[_countof(pav->BusPrefix) - 1] = 0;  // enforce null termination

    // Read the optional index and context values
    ValLen = sizeof(pav->Index);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_INDEX_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)&pav->Index,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\Index) returned %d\r\n"),
                  RegPath, status));
        pav->Index = (DWORD)-1;     // devload will find an index to use
    }

    // Read the Context value, if it exists.  If present, the Context overrides
    // the default behavior of passing the device a pointer to its activation
    // key.
    ValLen = sizeof(pav->Context);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_CONTEXT_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)&pav->Context,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\Context) returned %d\r\n"),
                  RegPath, status));
        pav->bOverrideContext = FALSE;    // context is pointer to active reg path string
    } else {
        pav->bOverrideContext = TRUE;
    }

    pav->bHasPostInitCode = FALSE;
    pav->bHasBusPostInitCode = FALSE;
    if (pav->Prefix[0] != 0) {
        // Determine whether this device wants a post init ioctl -- only
        // drivers which can be accessed via DeviceIoControl() can get these,
        // so we check for the presence of a prefix.
        ValLen = sizeof(pav->PostInitCode);
        status = RegQueryValueEx(
            DevKey,
            DEVLOAD_INITCODE_VALNAME,
            NULL,
            &ValType,
            (PUCHAR)&pav->PostInitCode,
            &ValLen);
        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE,
                     (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\InitCode) returned %d\r\n"),
                      RegPath, status));
        } else {
            pav->bHasPostInitCode = TRUE;
        }
    }
    // BusPostInitCode we get will be check later agains wheter this has bus name or not.
    ValLen = sizeof(pav->BusPostInitCode);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_BUSINITCODE_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)&pav->BusPostInitCode,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\BusInitCode) returned %d\r\n"),
                  RegPath, status));
    } else {
        pav->bHasBusPostInitCode = TRUE;
    }

    ValLen = sizeof(pav->AccountId);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_USERACCOUNTSID_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)&pav->AccountId,
        &ValLen);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\AccountId) returned %d\r\n"),
                  RegPath, status));
        pav->AccountId[0] = 0;
    } else {
        pav->AccountId[_countof(pav->AccountId)-1] = 0;
    }

    ValLen = sizeof(pav->Filter);
    status = RegQueryValueEx(
        DevKey,
        DEVLOAD_FILTER_VALNAME,
        NULL,
        &ValType,
        (PUCHAR)pav->Filter,
        &ValLen);
    if (status != ERROR_SUCCESS || (ValType!=REG_MULTI_SZ && ValType!=REG_SZ) ) {
        DEBUGMSG(ZONE_ACTIVE || ZONE_WARNING,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\Prefix) returned %d\r\n"),
                  RegPath, status));
        pav->Filter[0] = 0;
        pav->Filter[1] = 0;
    } else {
        pav->Filter[_countof(pav->Filter) - 1]=0;
        pav->Filter[_countof(pav->Filter) - 2]=0;
    }
    if (IsServicesRegKey(RegPath)) {
        // Legacy services may not have set this flag, so explicitly set it.
        pav->Flags |= DEVFLAGS_LOAD_AS_USERPROC;
        
        if (pav->bOverrideContext) {
            // Legacy services may set "Context", since in pre CE 6.0 days
            // this is how DWORD parameters were communicated.  This causes
            // all sorts of problems now that services.exe is effectively
            // hosted by udevice.exe, where Context has to do with registry key
            // and not DWORD.  This needs to be "ServiceContext" now.

            RETAILMSG(1,(L"SERVICES: Configuration error for %s.  Rename \"Context\" registry key to \"ServiceContext\"\r\n",RegPath));
            DEBUGCHK(0);

            if (pav->Context == 0) {
                // If Context=0 (%99 case), we can allow service to continue
                // to startup (because this is same as setting ServiceContext=0).
                // This helps with BC.
                pav->bOverrideContext = FALSE;
            }
            else {
                // Otherwise service will be badly broken - force it to be fixed.
                status = ERROR_INVALID_PARAMETER;
                goto done;
            }
        }
    }

    // if we got here with no errors, return success
    status = ERROR_SUCCESS;

done:
    // release resources
    if (DevKey) {
        RegCloseKey(DevKey);
    }

    return status;
}

DWORD
CreateActiveKey(
    __out_ecount(cchActivePath) LPWSTR pszActivePath,
    DWORD cchActivePath,
    LPDWORD pdwId,
    HKEY *phk
    )
{
    DWORD status;
    HKEY hkActive = NULL;

    DEBUGCHK(pszActivePath != NULL);
    DEBUGCHK(cchActivePath != 0);
    DEBUGCHK(pdwId != NULL);
    DEBUGCHK(phk != NULL);

    do {
        HRESULT hr;

        // get a device ID to try out
        *pdwId = InterlockedIncrement(&v_NextDeviceNum) - 1;  // sub 1 to get the pre-increment value
        if(*pdwId == ID_REGISTERED_DEVICE) {
            *pdwId = InterlockedIncrement(&v_NextDeviceNum) - 1;  // sub 1 to get the pre-increment value
            DEBUGCHK(*pdwId != ID_REGISTERED_DEVICE);
        }

        // Format the key's active key path (HLM\Drivers\Active\nnnn)
        hr = StringCchPrintfW(pszActivePath, cchActivePath, L"%s\\%02u", s_ActiveKey, *pdwId);
        if(hr != S_OK) {
            status = ERROR_INSUFFICIENT_BUFFER;
        } else {
            DWORD Disposition = 0;
            // Create a new key in the active list
            status = RegCreateKeyEx(
                HKEY_LOCAL_MACHINE,
                pszActivePath,
                0,
                NULL,
                REG_OPTION_VOLATILE,
                0,
                NULL,
                &hkActive,     // HKEY result
                &Disposition);
                
            if (status != ERROR_SUCCESS) {
                DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                         (TEXT("DEVICE!CreateActiveKey: RegCreateKeyEx(%s) failed %d\r\n"),
                          pszActivePath, status));
                break;
            } 
            else if(Disposition != REG_CREATED_NEW_KEY) {
                DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                    (TEXT("Device!CreateActiveKey: key '%s' already exists!\r\n"), pszActivePath));
                RegCloseKey(hkActive);
                status = ERROR_ALREADY_EXISTS;
            }
        }
    } while(status == ERROR_ALREADY_EXISTS);

    if(status == ERROR_SUCCESS) {
        DEBUGCHK(hkActive != NULL);
        DEBUGCHK(pszActivePath[0] != 0);
        DEBUGMSG(ZONE_ACTIVE, (TEXT("DEVICE!CreateActiveKey: created HLM\\%s\r\n"), pszActivePath));
    } else {
        hkActive = NULL;
        pszActivePath[0] = 0;
    }
    *phk = hkActive;
    
    return status;
}

DWORD
InitializeActiveKey(
    HKEY hkActive,
    LPCWSTR pszRegPath,
    LPCREGINI lpReg,
    DWORD cReg
    )
{
    DWORD status;
    
    DEBUGCHK(hkActive != NULL);
    
    // Install requested values in the device's active registry key.
    // (handle and device name are added after RegisterDevice())
    if (cReg != 0 && lpReg == NULL) {
        DEBUGMSG(ZONE_WARNING|ZONE_ERROR,
                 (TEXT("DEVICE!RegInitActiveKey: ignoring invalid registry initializer\n")));
        cReg = 0;  // cause all entries to be ignored
    }
    status = ERROR_SUCCESS;

    while (cReg-- && status == ERROR_SUCCESS) {
        __try {
            // validate nested pointers
            LPCWSTR pszName = (LPVOID) lpReg[cReg].lpszVal;//MapCallerPtr((LPVOID) lpReg[cReg].lpszVal, 256);    // check 256 bytes since we don't know the real length
            LPVOID pvData = lpReg[cReg].pData;//MapCallerPtr(lpReg[cReg].pData, lpReg[cReg].dwLen); // really only need READ access
            if(pszName == NULL || pvData == NULL) {
                status = ERROR_INVALID_ACCESS;
                break;
            } else {
                status = RegSetValueEx(
                    hkActive,
                    pszName,
                    0,
                    lpReg[cReg].dwType,
                    pvData,
                    lpReg[cReg].dwLen);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status = ERROR_INVALID_ACCESS;
        } 
        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                     (TEXT("DEVICE!RegInitActiveKey: RegSetValueEx(0x%08x) returned %d.\r\n"),
                      lpReg[cReg].lpszVal, status));
            goto done;
        }
    }

    // Registry path of the device driver (from HLM\Drivers\BuiltIn or HLM\Drivers\<bus>)
    if (pszRegPath != NULL) {    
        size_t cchRegPath = 0;
        VERIFY(SUCCEEDED(StringCchLength(pszRegPath, MAX_PATH, &cchRegPath)));
        status = RegSetValueEx(
            hkActive,
            DEVLOAD_DEVKEY_VALNAME,
            0,
            DEVLOAD_DEVKEY_VALTYPE,
            (PBYTE)pszRegPath,
            sizeof(TCHAR)*(cchRegPath + 1));
        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                     (TEXT("DEVICE!RegInitActiveKey: RegSetValueEx(%s) returned %d.\r\n"),
                      DEVLOAD_DEVKEY_VALNAME, status));
            goto done;
        }
    }

done:
    return status;
}

// this routine reads values written into the active key using the REGINI structures by the
// parent bus driver.
DWORD
ReadBusDriverValues(HKEY hkActive, pBusDriverValues_t pbdv)
{
    DWORD dwStatus = ERROR_SUCCESS, dwSize, dwType;
    DEBUGCHK(hkActive != NULL);
    DEBUGCHK(pbdv != NULL);

    // look for the parent bus driver
    dwSize = sizeof(pbdv->hParent);
    dwStatus = RegQueryValueEx(hkActive, DEVLOAD_BUSPARENT_VALNAME, NULL, &dwType, 
        (LPBYTE) &pbdv->hParent, &dwSize);
    if(dwStatus != ERROR_SUCCESS || dwType != DEVLOAD_BUSPARENT_VALTYPE) {
        pbdv->hParent = NULL;
    }

    // look for this device's name on the parent bus
    dwSize = sizeof(pbdv->szBusName);
    dwStatus = RegQueryValueEx(hkActive, DEVLOAD_BUSNAME_VALNAME, NULL, &dwType, 
        (LPBYTE) pbdv->szBusName, &dwSize);
    if(dwStatus != ERROR_SUCCESS || dwType != DEVLOAD_BUSNAME_VALTYPE) {
        pbdv->szBusName[0] = 0;
    } else {
        // enforce null termination
        pbdv->szBusName[_countof(pbdv->szBusName) - 1] = 0;
    }

    // got here with no fatal errors so return success
    dwStatus = ERROR_SUCCESS;
    
    return dwStatus;
}

DWORD
FinalizeActiveKey(
    HKEY hkActive,
    const fsdev_t *lpdev
    )
{
    DWORD status = ERROR_ACCESS_DENIED;

    DEBUGCHK(hkActive != NULL);
    
    // is this a named device?
    if (lpdev->pszLegacyName != NULL) {
        // Write device name
        status = RegSetValueEx(hkActive,
                               DEVLOAD_DEVNAME_VALNAME,
                               0,
                               DEVLOAD_DEVNAME_VALTYPE,
                               (PBYTE)lpdev->pszLegacyName,
                               sizeof(TCHAR)*(_tcslen(lpdev->pszLegacyName) + 1));
        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                     (TEXT("DEVICE!FinalizeActiveKey: RegSetValueEx(%s) returned %d.\r\n"),
                      DEVLOAD_DEVNAME_VALNAME, status));
            goto done;
        }
    }

    // Add device handle
    status = RegSetValueEx(
        hkActive,
        DEVLOAD_HANDLE_VALNAME,
        0,
        DEVLOAD_HANDLE_VALTYPE,
        (PBYTE)&lpdev,
        sizeof(lpdev));
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!FinalizeActiveKey: RegSetValueEx(%s) returned %d.\r\n"),
                  DEVLOAD_HANDLE_VALNAME, status));
        goto done;
    }

done:
    return status;
}
//
// This routine looks up a device driver by its activation handle and fills in a
// data structure with information about it.  It returns ERROR_SUCCESS if successful
// or a Win32 error code if not.  The device information pointer is expected to have
// been validated by the caller.
//
DWORD 
I_GetDeviceInformation(fsdev_t *lpdev, PDEVMGR_DEVICE_INFORMATION pdi)
{
    DWORD dwStatus = ERROR_SUCCESS;
    LPCWSTR pszDeviceRoot = L"$device\\";
    LPCWSTR pszBusRoot = L"$bus\\";
    
    // sanity check parameters
    if(lpdev == NULL || pdi == NULL) {
        return ERROR_INVALID_PARAMETER;
    }
    
    __try {
        if(pdi->dwSize < sizeof(*pdi)) {
            dwStatus = ERROR_INVALID_PARAMETER;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwStatus = ERROR_INVALID_PARAMETER;
    }
    if(dwStatus != ERROR_SUCCESS) {
        return dwStatus;
    }
    
    
    // look for the device
    if( (lpdev = FindDeviceByHandle(lpdev))!=NULL) {
        // load the caller's structure with information
        __try {
            HRESULT hr;
            pdi->hDevice = lpdev;
            pdi->hParentDevice = lpdev->hParent;
            if(pdi->hParentDevice != NULL) {
                // validate the parent device handle
                fsdev_t * lpParent = FindDeviceByHandle(pdi->hParentDevice);
                if(lpParent) {
                    DeDeviceRef(lpParent);
                }
                else {
                    pdi->hParentDevice = NULL;
                }
            }
            if(lpdev->pszLegacyName != NULL) {
                DEBUGCHK(lpdev->pszLegacyName[0] != 0);
                hr = StringCchCopyW(pdi->szLegacyName, _countof(pdi->szLegacyName), lpdev->pszLegacyName);
            } else {
                pdi->szLegacyName[0] = 0;
            }
            if(lpdev->pszDeviceName) {
                DEBUGCHK(lpdev->pszDeviceName[0] != 0);
                hr = StringCchCopyW(pdi->szDeviceName, _countof(pdi->szDeviceName), pszDeviceRoot);
                hr = StringCchCatW(pdi->szDeviceName, _countof(pdi->szDeviceName), lpdev->pszDeviceName);
            } else {
                pdi->szDeviceName[0] = 0;
            }
            if(lpdev->pszBusName != NULL) {
                DEBUGCHK(lpdev->pszBusName[0] != 0);
                hr = StringCchCopyW(pdi->szBusName, _countof(pdi->szBusName), pszBusRoot);
                hr = StringCchCatW(pdi->szBusName, _countof(pdi->szBusName), lpdev->pszBusName);
            } else {
                pdi->szBusName[0] = 0;
            }
            if((lpdev->wFlags & DF_REGISTERED) != 0) {
                DEBUGCHK(lpdev->pszDeviceKey == NULL);
                pdi->szDeviceKey[0] = 0;
            } else {
                DEBUGCHK(lpdev->pszDeviceKey != NULL);
                StringCchCopy(pdi->szDeviceKey, _countof(pdi->szDeviceKey), lpdev->pszDeviceKey);
                pdi->szDeviceKey[_countof(pdi->szDeviceKey) - 1] = 0;
            }
            pdi->dwSize = sizeof(*pdi);
            dwStatus = ERROR_SUCCESS;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            dwStatus = ERROR_INVALID_PARAMETER;
        }        
        DeDeviceRef(lpdev);
    }
    else {
        dwStatus = ERROR_NOT_FOUND;
    }
    
    return dwStatus;
}

//
// Function to load device drivers for built-in devices.
//
VOID
InitDevices(LPCTSTR lpBusName )
{
    HKEY RootKey;
    DWORD status;
    DWORD ValType;
    DWORD ValLen;
    TCHAR RootKeyPath[REG_PATH_LEN];
    TCHAR BusName[DEVKEY_LEN];
    REGINI reg[1];
    DWORD  dwRegCount=0;

    //
    // Open HLM\Drivers key
    //
    status = RegOpenKeyEx(
                HKEY_LOCAL_MACHINE,
                DEVLOAD_DRIVERS_KEY,
                0,
                KEY_READ,
                &RootKey);
                
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ROOT|ZONE_ERROR,
            (TEXT("DEVICE!InitDevices RegOpenKeyEx(%s) returned %d.\r\n"),
            DEVLOAD_DRIVERS_KEY, status));
        return;
    }

    //
    // Look for root key value; if not found use current Root Key as default,
    // otherwise open new root key
    //
    ValLen = sizeof(RootKeyPath);
    status = RegQueryValueEx(
                RootKey,
                DEVLOAD_ROOTKEY_VALNAME,
                NULL,
                &ValType,
                (PUCHAR)RootKeyPath,
                &ValLen);
    RootKeyPath[_countof(RootKeyPath) - 1] = 0;
                
    if (status != ERROR_SUCCESS) {
        // Root key value not found, thus root key is Drivers
        StringCchCopy(RootKeyPath, _countof(RootKeyPath), DEVLOAD_DRIVERS_KEY);
    }

    // Close previous root key
    RegCloseKey(RootKey);
    DEBUGMSG(ZONE_ROOT,
        (L"DEVICE!InitDevices: Root Key is %s.\r\n",
        RootKeyPath));
    if (lpBusName!=NULL) {
        reg[0].lpszVal = DEVLOAD_BUSNAME_VALNAME;
        reg[0].dwType  = DEVLOAD_BUSNAME_VALTYPE;
        reg[0].pData   = (PBYTE) lpBusName ;
        reg[0].dwLen   = (_tcslen( lpBusName ) + 1) * sizeof(TCHAR);
        dwRegCount = 1;
    }
    else {
        status = RegOpenKeyEx( HKEY_LOCAL_MACHINE, RootKeyPath,0,0, &RootKey);
        if (status == ERROR_SUCCESS ) {
            ValLen = sizeof( BusName);
            status = RegQueryValueEx(
                        RootKey,
                        DEVLOAD_BUSNAME_VALNAME,
                        NULL,
                        &ValType,
                        (PUCHAR)BusName,
                        &ValLen);                
            if (status == ERROR_SUCCESS && ValType==DEVLOAD_BUSNAME_VALTYPE) {
                // We found Bus Name. This is new bus driver model. So we use new format.
                BusName[DEVKEY_LEN-1] = 0;
                reg[0].lpszVal = DEVLOAD_BUSNAME_VALNAME;
                reg[0].dwType  = DEVLOAD_BUSNAME_VALTYPE;
                reg[0].pData   = (PBYTE)BusName;
                reg[0].dwLen   =  ValLen;
                dwRegCount = 1;
            }
            // Close previous root key
            RegCloseKey(RootKey);        
        }
    }
    // Someday we'll want to track the handle returned by ActivateDevice so that
    // we can potentially deactivate it later. But since this code refers only to
    // builtin (ie, static) devices, we can just throw away the key.
    if (dwRegCount) 
        ActivateDeviceEx(RootKeyPath,reg,dwRegCount,NULL); 
    else
        ActivateDevice(RootKeyPath, 0);
}   // InitDevices


//
// Called from device.c after it initializes.
//
void DevloadInit(void)
{
    DEBUGMSG(ZONE_INIT, (TEXT("DEVICE!DevloadInit\r\n")));
#define PHASE_1_BUSNAME TEXT("BuiltInPhase1")
    //
    // Delete the HLM\Drivers\Active key since there are no active devices at
    // init time.
    //
    RegDeleteKey(HKEY_LOCAL_MACHINE, s_ActiveKey);

#ifdef DEBUG
    v_NextDeviceNum = 0xFFFFFFF0;   // expect wraparound
#else   // DEBUG
    v_NextDeviceNum = 1;
#endif  // DEBUG
    g_bSystemInitd = FALSE;
    InitDevices(PHASE_1_BUSNAME);
}


//
// Called from device.c after the system has initialized.
//
void DevloadPostInit(void)
{
    if (g_bSystemInitd) {
        DEBUGMSG(ZONE_INIT, (TEXT("DEVICE!DevloadPostInit: multiple DLL_SYSTEM_STARTED msgs!!!\r\n")));
        return;
    }
    g_bSystemInitd = TRUE;

    DEBUGMSG(ZONE_INIT, (TEXT("-DEVICE!DevloadPostInit\r\n")));
}
