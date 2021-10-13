//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include "devmgrif.h"
#include "devzones.h"

typedef struct _RegActivationValues_tag {
    TCHAR DevDll[DEVDLL_LEN];       // required
    TCHAR Prefix[PREFIX_CHARS + 1]; // optional, must be of length 3 when present
    WCHAR BusPrefix[PREFIX_CHARS + 1];    // optional, must agree with Prefix if both are present
    DWORD Index;                    // optional
    DWORD Flags;                    // optional
    DWORD Context;                  // optional
    BOOL bOverrideContext;          // TRUE only if Context is valid
    DWORD PostInitCode;             // optional
    DWORD BusPostInitCode;          // optional
    BOOL bHasPostInitCode;          // TRUE only if PostInitCode is valid
    BOOL bHasBusPostInitCode;       // TRUE only if BusPostInitCode is valid.
} RegActivationValues_t, *pRegActivationValues_t;

typedef struct _BusDriverValues_tag {
    HANDLE hParent;
    WCHAR szBusName[MAXBUSNAME];
} BusDriverValues_t, *pBusDriverValues_t;

int v_NextDeviceNum;            // Used to create active device list

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

// Placeholder for entry point not exported by a driver.
static BOOL DevFileNotSupportedBool (void)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

// Placeholder for entry point not exported by a driver.
static DWORD DevFileNotSupportedDword (void)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return -1;
}

// This routine looks for a device driver entry point.  The "type" parameter
// is the driver's prefix or NULL; the lpszName parameter is the undecorated
// name of the entry point; and the hInst parameter is the DLL instance handle
// of the driver's DLL.
static PFNVOID GetDMProcAddr(LPCWSTR type, LPCWSTR lpszName, HINSTANCE hInst) {
    LPCWSTR s;
    WCHAR fullname[32];
    if (type != NULL && *type != L'\0') {
        memcpy(fullname,type,3*sizeof(WCHAR));
        fullname[3] = L'_';
        wcscpy(&fullname[4],lpszName);
        s = fullname;
    } else {
        s = lpszName;
    }
    return (PFNVOID)GetProcAddress(hInst, s);
}

// Look for a member of the candidate device list that matches this one.  Return a pointer
// to the matching entry or NULL if no match is found.  Make sure to hold g_devcs when 
// calling this routine.
static fscandidatedev_t *
FindCandidateDev(LPCWSTR pszName, LPCWSTR pszBusName)
{
    fscandidatedev_t *pdc;

    DEBUGCHK(pszName != NULL);
    DEBUGCHK(pszBusName != NULL);

    // look for a match
    for(pdc = (fscandidatedev_t *) g_CandidateDevs.Flink;
            pdc != (fscandidatedev_t *) &g_CandidateDevs;
            pdc = (fscandidatedev_t *) pdc->list.Flink) {
        if(pszName[0] != 0 && _wcsicmp(pdc->szDeviceName, pszName) == 0) {
            // found a match, get out
            break;
        }
        if(pszBusName[0] != 0 && _wcsicmp(pdc->szBusName, pszBusName) == 0) {
            // bus name conflict, get out
            break;
        }
    }

    // did we find a match?
    if(pdc == (fscandidatedev_t *) &g_CandidateDevs) {
        pdc = NULL;                 // no, return NULL
    }

    return pdc;
}


// Create a structure to keep track of device prefix/index combinations while we install
// the device and add it to the list of candidate devices.  Make sure to hold g_devcs 
// when this routine is called.  Return a pointer to the structure if successful or NULL
// if there's a problem.
static fscandidatedev_t *
CreateCandidateDev(LPCWSTR pszName, LPCWSTR pszBusName)
{
    fscandidatedev_t *pdc;

    DEBUGCHK(pszName != NULL);
    DEBUGCHK(pszBusName != NULL);
    
    EnterCriticalSection(&g_devcs);

    __try {
        pdc = FindCandidateDev(pszName, pszBusName);
        if(pdc != NULL) {
            pdc = NULL;
        } else {
            // allocate a structure
            pdc = LocalAlloc(0, sizeof(*pdc));
            if(pdc == NULL) {
                DEBUGMSG(ZONE_ACTIVE | ZONE_ERROR, 
                         (_T("DEVICE:CreateCandidateDev couldn't allocate candidate device structure\r\n")));
                SetLastError(ERROR_OUTOFMEMORY);
            } else {
                // init structure members
                memset(pdc, 0, sizeof(*pdc));
                wcsncpy(pdc->szDeviceName, pszName, ARRAYSIZE(pdc->szDeviceName));
                wcsncpy(pdc->szBusName, pszBusName, ARRAYSIZE(pdc->szBusName));

                // add this structure to the list
                InsertTailList(&g_CandidateDevs, (PLIST_ENTRY) pdc)
            }
        }
    }
    __finally {
        LeaveCriticalSection(&g_devcs);
    }

    return pdc;
}

// remove an entry from the list of candidate device structures and free its associated
// memory.  Make sure to hold g_devcs when calling this function.
static void
DeleteCandidateDev(fscandidatedev_t * pdc)
{
    EnterCriticalSection(&g_devcs);
    
    // sanity check
    if(pdc != NULL) {
        RemoveEntryList((PLIST_ENTRY) pdc);
        LocalFree(pdc);
    }

    LeaveCriticalSection(&g_devcs);
}

//
// FindDeviceByHandle - function to check if passed in device handle is actually a
// valid registered device.
//
// Return TRUE if it is a valid registered device, FALSE otherwise.
//
BOOL
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
            break;
        }
    }

    LeaveCriticalSection(&g_devcs);
    
    return fFound;
}


// look up an existing device based on its name
fsdev_t *
FindDeviceByName(LPCWSTR pszName, NameSpaceType nameType)
{
    fsdev_t *lpTrav;

    DEBUGCHK(pszName != NULL);

    EnterCriticalSection (&g_devcs);
    for (lpTrav = (fsdev_t *)g_DevChain.Flink;
         lpTrav != (fsdev_t *)&g_DevChain;
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
    if(lpTrav == (fsdev_t *) &g_DevChain) {
        // yes -- no match
        lpTrav = NULL;
    }
    LeaveCriticalSection (&g_devcs);

    return lpTrav;
}

static fscandidatedev_t *
CreateUniqueCandidateDev(WCHAR *pszPrefix, DWORD dwIndex, LPCWSTR pszBusName)
{
    fscandidatedev_t *lpcandidatedev = NULL;
    WCHAR szDeviceName[MAXDEVICENAME];
    BOOL fDuplicate = FALSE;

    DEBUGCHK(pszPrefix != NULL);
    DEBUGCHK(pszBusName != NULL);

    // format the device name
    if(pszPrefix[0] != 0) 
        StringCchPrintf(szDeviceName,MAXDEVICENAME,TEXT("%s%u"), pszPrefix, dwIndex);
    else
        szDeviceName[0] = 0;

    EnterCriticalSection(&g_devcs);

    // if the device has a standard name, check for conflicts
    if(pszPrefix[0] != 0) {
        DEBUGCHK(wcslen(pszPrefix) == PREFIX_CHARS);
        if(FindDeviceByName(szDeviceName, NtDevice) != NULL) {
            fDuplicate = TRUE;
        }
    }

    // if the device has a bus name, check for conflicts
    if(!fDuplicate) {
        if(pszBusName[0] != 0) {
            if(FindDeviceByName(pszBusName, NtBus) != NULL) {
                fDuplicate = TRUE;
            }
        }
    }

    // look for devices under creation if we're ok so far
    if(!fDuplicate) {
        // look for a conflict in the list of candidate devices (those
        // currently being created by another thread)
        // make sure nobody else uses this index while we are installing this device
        lpcandidatedev = CreateCandidateDev(szDeviceName, pszBusName);
    }
    
    LeaveCriticalSection(&g_devcs);

    return lpcandidatedev;
}

// This routine frees resources associated with a device driver.  These include
// unloading (or decrementing the use count of) the driver DLL and freeing the
// driver's descriptor structure.
void
DeleteDevice(fsdev_t *lpdev)
{
    DEBUGCHK(lpdev != NULL);
    DEBUGMSG(ZONE_ACTIVE, (_T("DeleteDevice: deleting device at 0x%08x\r\n"), lpdev));
    if(lpdev->hLib != NULL) {
        FreeLibrary(lpdev->hLib);
    }

    LocalFree(lpdev);
}

// This routine allocates a device driver structure in memory and initializes it.
// As part of this process, it loads the driver's DLL and obtains pointers to
// its entry points.  This routine returns a pointer to the new driver description
// structure, or NULL if there's an error.
static fsdev_t *
CreateDevice(
    LPCWSTR lpszPrefix,
    DWORD dwIndex,
    DWORD dwLegacyIndex,
    DWORD dwId,
    LPCWSTR lpszLib,
    DWORD dwFlags,
    LPCWSTR lpszBusPrefix,
    LPCWSTR lpszBusName,
    LPCWSTR lpszDeviceKey,
    HANDLE hParent
    )
{
    fsdev_t *lpdev;
    DWORD dwSize;
    DWORD dwStatus = ERROR_SUCCESS;
    WCHAR szDeviceName[MAXDEVICENAME];
    WCHAR szLegacyName[MAXDEVICENAME];

    DEBUGCHK(lpszPrefix != NULL);
    DEBUGCHK(lpszLib != NULL);
    DEBUGCHK(wcslen(lpszPrefix) <= 3);
    DEBUGCHK(dwLegacyIndex == dwIndex || (dwLegacyIndex == 0 && dwIndex == 10));
    DEBUGCHK(lpszBusName != NULL);

    // figure out how much memory to allocate
    dwSize = sizeof(*lpdev);

    // is the device named?
    if(lpszPrefix[0] == 0) {
        // unnamed device
        szDeviceName[0] = 0;
    } else {
        // named device, allocate room for its names
        StringCchPrintf(szDeviceName,MAXDEVICENAME,TEXT("%s%u"), lpszPrefix, dwIndex);
        if(dwLegacyIndex <= 9) {
            // allocate room for name and null
            wsprintf(szLegacyName, L"%s%u:", lpszPrefix, dwLegacyIndex);
            dwSize += (wcslen(szLegacyName) + 1) * sizeof(WCHAR);
        }

        // allocate room for name and null        
        dwSize += (wcslen(szDeviceName) + 1) * sizeof(WCHAR);
    }

    // If the bus driver didn't allocate a name the device may still support the 
    // bus name interface -- use its device name (if present) just in case.
    if(lpszBusName[0] == 0 && szDeviceName[0] != 0) {
        lpszBusName = szDeviceName;
    }
    
    // allocate room for the bus name
    if(lpszBusName[0] != 0) {
        dwSize += (wcslen(lpszBusName) + 1) * sizeof(WCHAR);
    }

    // make room to store the device key as well
    if(lpszDeviceKey != NULL) {
        dwSize += (wcslen(lpszDeviceKey) + 1) * sizeof(WCHAR);
    }
    
    // allocate the structure
    if (!(lpdev = LocalAlloc(0, dwSize))) {
        DEBUGMSG(ZONE_WARNING, (_T("DEVICE!CreateDevice: couldn't allocate device structure\r\n")));
        dwStatus = ERROR_OUTOFMEMORY;
    } else {
        LPWSTR psz = (LPWSTR) (((LPBYTE) lpdev) + sizeof(*lpdev));
        memset(lpdev, 0, dwSize);
        lpdev->dwId = dwId;
        lpdev->wFlags = 0;
        if(PSLGetCallerTrust() == OEM_CERTIFY_TRUST) {
            lpdev->wFlags |= DF_TRUSTED_LOADER;
            lpdev->hParent = hParent;
        }
        if (lpszPrefix[0] != 0) {
            if(dwLegacyIndex <= 9) {
                lpdev->pszLegacyName = psz;
                wcscpy(lpdev->pszLegacyName, szLegacyName);
                psz += wcslen(lpdev->pszLegacyName) + 1;
            }
            lpdev->pszDeviceName = psz;
            wcscpy(lpdev->pszDeviceName, szDeviceName);
            psz += wcslen(lpdev->pszDeviceName) + 1;
        }
        if(lpszBusName[0] != 0) {
            lpdev->pszBusName = psz;
            wcscpy(lpdev->pszBusName, lpszBusName);
            psz += wcslen(lpszBusName) + 1;
        }
        if(lpszDeviceKey != NULL) {
            lpdev->pszDeviceKey= psz;
            wcscpy(lpdev->pszDeviceKey, lpszDeviceKey);
            psz += wcslen(lpszDeviceKey) + 1;
        }
        DEBUGMSG(ZONE_ACTIVE, (_T("DEVICE!CreateDevice: loading driver DLL '%s'\r\n"), lpszLib));
        lpdev->hLib =
            (dwFlags & DEVFLAGS_LOADLIBRARY) ? LoadLibrary(lpszLib) : LoadDriver(lpszLib);
        if (!lpdev->hLib) {
            DEBUGMSG(ZONE_WARNING, (_T("DEVICE!CreateDevice: couldn't load '%s' -- error %d\r\n"), 
                lpszLib, GetLastError()));
            dwStatus = ERROR_FILE_NOT_FOUND;
        } else {
            LPCWSTR pEffType = NULL;
            if((dwFlags & DEVFLAGS_NAKEDENTRIES) == 0) {
                if(lpszPrefix[0] != 0) {
                    DEBUGCHK(lpszBusPrefix[0] == 0 || wcsicmp(lpszBusPrefix, lpszPrefix) == 0);
                    pEffType = lpszPrefix;      // use standard prefix decoration
                } else if(lpszBusPrefix[0] != 0 && lpdev->pszBusName != NULL) {
                    pEffType = lpszBusPrefix;   // no standard prefix, use bus prefix decoration
                } else {
                    if(lpdev->pszDeviceName != NULL) {
                        // device is expected to have a device or bus name, but we don't know
                        // how to look for its entry points
                        DEBUGMSG(ZONE_ACTIVE || ZONE_ERROR, 
                            (_T("DEVICE!CreateDevice: no entry point information for '%s' can't load '%s'\r\n"), 
                            lpszLib, lpdev->pszDeviceName));
                        dwStatus = ERROR_INVALID_FUNCTION;
                    }
                }
            }
            
            lpdev->fnInit = (pInitFn)GetDMProcAddr(pEffType,L"Init",lpdev->hLib);
            lpdev->fnPreDeinit = (pDeinitFn)GetDMProcAddr(pEffType,L"PreDeinit",lpdev->hLib);
            lpdev->fnDeinit = (pDeinitFn)GetDMProcAddr(pEffType,L"Deinit",lpdev->hLib);
            lpdev->fnOpen = (pOpenFn)GetDMProcAddr(pEffType,L"Open",lpdev->hLib);
            lpdev->fnPreClose = (pCloseFn)GetDMProcAddr(pEffType,L"PreClose",lpdev->hLib);
            lpdev->fnClose = (pCloseFn)GetDMProcAddr(pEffType,L"Close",lpdev->hLib);
            lpdev->fnRead = (pReadFn)GetDMProcAddr(pEffType,L"Read",lpdev->hLib);
            lpdev->fnWrite = (pWriteFn)GetDMProcAddr(pEffType,L"Write",lpdev->hLib);
            lpdev->fnSeek = (pSeekFn)GetDMProcAddr(pEffType,L"Seek",lpdev->hLib);
            lpdev->fnControl = (pControlFn)GetDMProcAddr(pEffType,L"IOControl",lpdev->hLib);
            lpdev->fnPowerup = (pPowerupFn)GetDMProcAddr(pEffType,L"PowerUp",lpdev->hLib);
            lpdev->fnPowerdn = (pPowerupFn)GetDMProcAddr(pEffType,L"PowerDown",lpdev->hLib);

            // Make sure that the driver has an init and deinit routine.  If it is named,
            // it must have open and close, plus at least one of the I/O routines (read, write
            // ioctl, and/or seek).  If a named driver has a pre-close routine, it must also 
            // have a pre-deinit routine.
            if (!(lpdev->fnInit && lpdev->fnDeinit) ||
                lpdev->pszDeviceName != NULL && (!lpdev->fnOpen || 
                             !lpdev->fnClose ||
                             (!lpdev->fnRead && !lpdev->fnWrite &&
                              !lpdev->fnSeek && !lpdev->fnControl) ||
                             (lpdev->fnPreClose && !lpdev->fnPreDeinit))) {
                DEBUGMSG(ZONE_WARNING, (_T("DEVICE!CreateDevice: illegal entry point combination in driver DLL '%s'\r\n"), 
                    lpszLib));
                dwStatus = ERROR_INVALID_FUNCTION;
            }

            if (!lpdev->fnOpen) lpdev->fnOpen = (pOpenFn) DevFileNotSupportedBool;
            if (!lpdev->fnClose) lpdev->fnClose = (pCloseFn) DevFileNotSupportedBool;
            if (!lpdev->fnControl) lpdev->fnControl = (pControlFn) DevFileNotSupportedBool;
            if (!lpdev->fnRead) lpdev->fnRead = (pReadFn) DevFileNotSupportedDword;
            if (!lpdev->fnWrite) lpdev->fnWrite = (pWriteFn) DevFileNotSupportedDword;
            if (!lpdev->fnSeek) lpdev->fnSeek = (pSeekFn) DevFileNotSupportedDword;
        }
    }

    // did everything go ok?
    if(dwStatus != ERROR_SUCCESS) {
        if(lpdev != NULL) {
            DeleteDevice(lpdev);
            lpdev = NULL;
        }
        SetLastError(dwStatus);
    }

    DEBUGMSG(ZONE_ACTIVE || (dwStatus != ERROR_SUCCESS && ZONE_WARNING), 
        (_T("CreateDevice: creation of type '%s', index %d, lib '%s' returning 0x%08x, error code %d\r\n"),
        lpszPrefix[0] != 0 ? lpszPrefix : _T("<unnamed>"), dwIndex, lpszLib, lpdev, dwStatus));

    return lpdev;
}
//
// This routine is security check for dwInfo passed in by either RegistryDevice or ActiveDevice
DWORD CheckLauchDeviceParam(DWORD dwInfo)
{
    if (CeGetCallerTrust() != OEM_CERTIFY_TRUST) { // Untrusted caller will do following.
        LPCTSTR lpActivePath = (LPCTSTR) dwInfo; // We assume it is Registry Path.
        if (lpActivePath) {
            HKEY hActiveKey;
            if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, lpActivePath, 0, 0, &hActiveKey) == ERROR_SUCCESS ) { // It is registry.
                // We need check Registry is in secure location.
                CE_REGISTRY_INFO regInfo;
                DWORD dwRet = ERROR_INVALID_PARAMETER;
                memset(&regInfo,0,sizeof(regInfo));
                regInfo.cbSize = sizeof(CE_REGISTRY_INFO);
                if (CeFsIoControl(NULL, FSCTL_GET_REGISTRY_INFO, &hActiveKey, sizeof(HKEY), &regInfo, sizeof(CE_REGISTRY_INFO), NULL, NULL)) { // Succeed
                    if (regInfo.dwFlags & CE_REG_INFO_FLAG_TRUST_PROTECTED) {
                        dwRet = ERROR_SUCCESS;
                    }
                }
                RegCloseKey( hActiveKey ); 
                return dwRet;
            }
        }
    }
    return ERROR_SUCCESS;  
}
    
// This routine launches a device driver by threading it into the list of
// active devices and calling its Init() entry point.  It returns ERROR_SUCCESS
// if successful or a Win32 error code otherwise.
static DWORD
LaunchDevice(
    fsdev_t *lpdev, 
    DWORD dwInfo, 
    DWORD dwFlags,
    LPVOID lpvParam
    )
{
    DWORD retval = ERROR_SUCCESS;
    LPEXCEPTION_POINTERS pep;

    // record that this device is activating    
    lpdev->dwActivateId = GetCurrentThreadId();
    EnterCriticalSection(&g_devcs);
    InsertTailList(&g_ActivatingDevs, (PLIST_ENTRY) lpdev);
    LeaveCriticalSection(&g_devcs);

    retval = CheckLauchDeviceParam(dwInfo);
    if (retval!=ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR, (_T("DEVICE!LaunchDevice: CheckDWINFO (0x%x) return ERROR \r\n"), retval));
        goto errret;
    }
    
    // call the driver's init entry point
    __try {
        DEBUGMSG(ZONE_ACTIVE, (_T("DEVICE!LaunchDevice: calling Init() for device 0x%08x\r\n"), lpdev));
        lpdev->dwData = lpdev->fnInit(dwInfo,lpvParam);
        if (!(lpdev->dwData)) {
            retval = ERROR_OPEN_FAILED;
        }
    } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER ) {
        DEBUGMSG(ZONE_WARNING, (_T("DEVICE!LaunchDevice: exception in Init for device 0x%08x\r\n"), lpdev));
        retval = ERROR_INVALID_PARAMETER;
    }

    // device is done activating
    EnterCriticalSection(&g_devcs);
    RemoveEntryList((PLIST_ENTRY) lpdev);
    LeaveCriticalSection(&g_devcs);
    lpdev->dwActivateId = 0;
    
    if (retval != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE || ZONE_WARNING, (_T("DEVICE!LaunchDevice: Init() failed for device 0x%08x\r\n"), lpdev));
        goto errret;
    }

    DEBUGCHK(lpdev != NULL);

    // Are we supposed to unload this driver right away?  If so, the caller will 
    // free its resources.
    if ((dwFlags & DEVFLAGS_UNLOAD) == 0) {
        EnterCriticalSection(&g_devcs);
        __try {
            InsertTailList(&g_DevChain, (PLIST_ENTRY)lpdev);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            retval = ERROR_INVALID_PARAMETER;
        }
        LeaveCriticalSection(&g_devcs);

#ifdef DEBUG
        if (ZONE_ACTIVE) DebugDumpDevices();
#endif
    } else {
        // deinit the driver before deleting it
        __try {
            DEBUGMSG(ZONE_ACTIVE, (_T("DEVICE!LaunchDevice: calling DeInit(0x%08x) for device 0x%08x\r\n"), lpdev->dwData, lpdev));
            DEBUGCHK(lpdev->dwData != 0);
            // Note -- if a driver has DEVFLAGS_UNLOAD and yet allows applications
            // to open handles to it, we will potentially have a race condition
            // problem.  To really do this right, we would need to duplicate the
            // code in DeregisterDevice() or call it directly.
            if(lpdev->fnPreDeinit != NULL) {
                lpdev->fnPreDeinit(lpdev->dwData);
            }
            lpdev->fnDeinit(lpdev->dwData);
        } __except (pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
            DEBUGMSG(ZONE_WARNING, (_T("DEVICE!LaunchDevice: exception in DeInit for device 0x%08x\r\n"), lpdev));
        }
    }

errret:
    return retval;
}

//      @func   HANDLE | RegisterDevice | Register a new device
//  @parm       LPCWSTR | lpszType | device id (SER, PAR, AUD, etc) - must be 3 characters long
//  @parm       DWORD | dwIndex | index between 0 and 9, ie: COM2 would be 2
//  @parm       LPCWSTR | lpszLib | dll containing driver code
//  @parm       DWORD | dwInfo | instance information
//      @rdesc  Returns a handle to a device, or 0 for failure.  This handle should
//                      only be passed to DeregisterDevice.
//      @comm   For stream based devices, the drivers will be .dll or .drv files.
//                      Each driver is initialized by
//                      a call to the RegisterDevice API (performed by the server process).
//                      The lpszLib parameter is what will be to open the device.  The
//                      lpszType is a three character string which is used to identify the 
//                      function entry points in the DLL, so that multiple devices can exist
//                      in one DLL.  The lpszLib is the name of the DLL which contains the
//                      entry points.  Finally, dwInfo is passed in to the init routine.  So,
//                      if there were two serial ports on a device, and comm.dll was the DLL
//                      implementing the serial driver, it would be likely that there would be
//                      the following init calls:<nl>
//                      <tab>h1 = RegisterDevice(L"COM", 1, L"comm.dll",0x3f8);<nl>
//                      <tab>h2 = RegisterDevice(L"COM", 2, L"comm.dll",0x378);<nl>
//                      Most likely, the server process will read this information out of a
//                      table or registry in order to initialize all devices at startup time.
//      @xref <f DeregisterDevice> <l Overview.WinCE Device Drivers>
//

HANDLE
I_RegisterDeviceEx(
    LPCWSTR lpszType,
    DWORD dwIndex,
    LPCWSTR lpszLib,
    DWORD dwInfo
    )
{
    fsdev_t *lpdev = NULL;
    fscandidatedev_t *lpcandidatedev = NULL;
    DWORD retval = ERROR_SUCCESS;
    WCHAR szType[PREFIX_CHARS + 1];
    WCHAR szLib[DEVDLL_LEN + 1];
    LPCWSTR szBusName = L"";        // RegisterDevice doesn't provide a way to specify a bus name

    // validate parameters
    if(lpszType == NULL || lpszLib == NULL) {
        retval = ERROR_INVALID_PARAMETER;
        goto done;
    }
    
    if (dwIndex > 9) {
        retval = ERROR_INVALID_INDEX;
        goto done;
    }

    szType[ARRAYSIZE(szType) - 1] = 0 ;
    szLib[ARRAYSIZE(szLib) - 1] = 0 ;
    __try {
        wcsncpy(szType, lpszType, ARRAYSIZE(szType));
        wcsncpy(szLib, lpszLib, ARRAYSIZE(szLib));
        if(szType[ARRAYSIZE(szType) - 1] != 0 || szLib[ARRAYSIZE(szLib) - 1] != 0) {
            retval = ERROR_INVALID_PARAMETER;
        }
    } 
    __except(EXCEPTION_EXECUTE_HANDLER) {
        retval = ERROR_INVALID_PARAMETER;
    }
    if(retval != ERROR_SUCCESS) {
        goto done;
    }

    // allocate the device structure
    lpdev = CreateDevice(szType, dwIndex, dwIndex, ID_REGISTERED_DEVICE, szLib, DEVFLAGS_NONE, szType, szBusName, NULL, NULL);
    if (lpdev == NULL) {
        retval = GetLastError();
        goto done;
    }
    lpdev->wFlags |= DF_REGISTERED;      // launched with RegisterDevice()

    // is this a named stream device?
    if(lpdev->pszDeviceName != NULL) {
        // look for a conflict in the list of existing devices
        lpcandidatedev = CreateUniqueCandidateDev(szType, dwIndex, szBusName);

        // was there a name conflict?
        if(lpcandidatedev == NULL) {
            retval = ERROR_DEVICE_IN_USE;
            goto done;
        }
    }

    // Start the device if were we able to reserve this device name 
    // or it was unnamed
    if(lpdev->pszDeviceName == NULL || lpcandidatedev != NULL) {
        // start the driver
        retval = LaunchDevice(lpdev, dwInfo, DEVFLAGS_NONE, NULL);
    }

done:
    // release the candidate device name if necessary
    if(lpcandidatedev != NULL) {
        DeleteCandidateDev(lpcandidatedev);
    }
    
    // If we failed then let's clean up the module and data
    if (retval != ERROR_SUCCESS) {
        if (lpdev) {
            DeleteDevice(lpdev);
            lpdev = NULL;
        }
        SetLastError (retval);
    }

    return (HANDLE) lpdev;
}

// This routine unloads a driver that was loaded with I_RegisterDeviceEx().
// It returns TRUE if successful, or FALSE if there's a problem.  If there's
// a problem it calls SetLastError() to pass back additional information.
BOOL 
I_DeregisterDevice(HANDLE hDevice)
{
    fsdev_t *lpdev;
    fsopendev_t *lpopen;
    DWORD dwStatus = ERROR_NOT_FOUND;
    BOOL retval = FALSE;
    LPEXCEPTION_POINTERS pep;

    EnterCriticalSection(&g_devcs);
    __try {
        // validate the handle
        lpdev = (fsdev_t *)hDevice;
        if(FindDeviceByHandle(lpdev)) {
            // Signal an error if this device was not registered *or* if the
            // deactivating flag is not set.  This routine is invoked during
            // DeactivateDevice() processing, in which case it is valid for the
            // deactivating flag to be set.
            if((lpdev->wFlags & DF_REGISTERED) == 0 && (lpdev->wFlags & DF_DEACTIVATING) == 0) {
                DEBUGMSG(ZONE_WARNING, 
                    (_T("I_DeregisterDevice: rejecting attempt to DeregisterDevice() device 0x%08x loaded with ActivateDevice()\r\n"),
                    lpdev));
            } 
            else if((lpdev->wFlags & DF_TRUSTED_LOADER) != 0 && PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
                dwStatus = ERROR_ACCESS_DENIED;
            } else {
                dwStatus = ERROR_SUCCESS;
            }
        }
        if (dwStatus != ERROR_SUCCESS) {
            goto errret;
        }

        // make sure that open handles can't be used for further I/O
        lpopen = g_lpOpenDevs;      // check the active handles list
        while (lpopen) {
            if (lpopen->lpDev == lpdev) {
                lpopen->lpDev = NULL;
            }
            lpopen = lpopen->nextptr;
        }
        lpopen = g_lpDyingOpens;    // check the dying handles list
        while (lpopen) {
            if (lpopen->lpDev == lpdev) {
                lpopen->lpDev = NULL;
            }
            lpopen = lpopen->nextptr;
        }

        // remove from the list of active devices
        RemoveDeviceFromSearchList(lpdev);
        RemoveEntryList((PLIST_ENTRY)lpdev);

        LeaveCriticalSection(&g_devcs);
        
        // this flag is not examined by threads holding g_devcs so 
        // we don't need to modify it while holding the critical section
        // ourselves
        lpdev->wFlags |= DF_DYING;

        __try {
            if(lpdev->fnPreDeinit != NULL) {
                lpdev->fnPreDeinit(lpdev->dwData);
            } else {
                lpdev->fnDeinit(lpdev->dwData);
            }
        }
        __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER ) {
            DEBUGMSG(ZONE_WARNING, (_T("I_DeregisterDevice: exception in deinit entry point\r\n")));
        }
        
        if (!lpdev->dwRefCnt) {
            if(lpdev->fnPreDeinit != NULL) {
                __try {
                    lpdev->fnDeinit(lpdev->dwData);
                }
                __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG(ZONE_WARNING, (_T("I_DeregisterDevice: exception in final deinit entry point\r\n")));
                }
            }
            DeleteDevice(lpdev);
            EnterCriticalSection(&g_devcs);
        } else {
            EnterCriticalSection(&g_devcs);
            InsertTailList(&g_DyingDevs, (PLIST_ENTRY)lpdev);
            SetEvent(g_hCleanEvt);
        }
        retval = TRUE;
errret:
        ;
    } 
    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwStatus = ERROR_INVALID_PARAMETER;
    }
    LeaveCriticalSection(&g_devcs);

    if(!retval) {
        DEBUGCHK(dwStatus != ERROR_SUCCESS);
        SetLastError(dwStatus);
    }
    return retval;
}

// Function to call a newly registered device's post initialization
// device I/O control function.
static VOID
DevicePostInit(
    LPTSTR DevName,
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
static VOID
DeleteActiveKey(
    LPCWSTR ActivePath
    )
{
    DWORD status;

    DEBUGCHK(ActivePath != NULL);
    DEBUGCHK(ActivePath[0] != 0);

    // attempt to delete the active key completely
    status = RegDeleteKey(HKEY_LOCAL_MACHINE, ActivePath);
    if (status != ERROR_SUCCESS) {
        HKEY hActiveKey;
    
        DEBUGMSG(ZONE_ACTIVE|ZONE_WARNING,(TEXT("DEVICE!DeleteActiveKey: RegDeleteKey failed %d\n"), status));

        // Open active key
        status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, ActivePath, 0, 0, &hActiveKey);
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
            RegDeleteValue(hActiveKey, DEVLOAD_SOCKET_VALNAME);
            RegCloseKey(hActiveKey);
        }
    }
}   // DeleteActiveValues

// This routine reads mandatory and optional values that the caller of 
// ActivateDevice() may include in the device key.  It returns ERROR_SUCCESS
// if all required values are present, or a Win32 error code if there is a
// problem.
static DWORD
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
        0,
        &DevKey);
    if (status) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                 (TEXT("DEVICE!RegReadActivationValues RegOpenKeyEx(%s) returned %d.\r\n"),
                  RegPath, status));
        return status;
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
        RegCloseKey(DevKey);
        return status; // dll name is required
    }
    pav->DevDll[ARRAYSIZE(pav->DevDll) - 1] = 0;  // enforce null termination

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
        DEBUGMSG(ZONE_ACTIVE || ZONE_ERROR,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\Prefix) returned %d\r\n"),
                  RegPath, status));
        pav->Prefix[0] = 0;
    } else {
        if(pav->Prefix[ARRAYSIZE(pav->Prefix) - 1] != 0 
        || (wcslen(pav->Prefix) != PREFIX_CHARS && wcslen(pav->Prefix) != 0)) {
            DEBUGMSG(ZONE_ACTIVE,
                     (TEXT("DEVICE!RegReadActivationValues: ignoring invalid Prefix in (%s)\r\n"),
                      RegPath));
            pav->Prefix[0] = 0;
        }
    }
    pav->Prefix[ARRAYSIZE(pav->Prefix) - 1] = 0;  // enforce null termination

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
        DEBUGMSG(ZONE_ACTIVE || ZONE_ERROR,
                 (TEXT("DEVICE!RegReadActivationValues RegQueryValueEx(%s\\BusPrefix) returned %d\r\n"),
                  RegPath, status));
        pav->BusPrefix[0] = 0;
    } else {
        if(pav->BusPrefix[ARRAYSIZE(pav->BusPrefix) - 1] != 0 
        || (wcslen(pav->BusPrefix) != PREFIX_CHARS && wcslen(pav->BusPrefix) != 0)) {
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
    pav->BusPrefix[ARRAYSIZE(pav->BusPrefix) - 1] = 0;  // enforce null termination

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

    // if we got here with no errors, return success
    status = ERROR_SUCCESS;

done:
    // release resources
    DEBUGCHK(DevKey != NULL);
    RegCloseKey(DevKey);

    return status;
}

static DWORD
CreateActiveKey(
    LPWSTR pszActivePath,
    DWORD cchActivePath,
    LPDWORD pdwId,
    HKEY *phk
    )
{
    DWORD status;
    HKEY hkActive;

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
            DWORD Disposition;
            
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

static DWORD
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
            LPWSTR pszName = MapCallerPtr((LPVOID) lpReg[cReg].lpszVal, 256);   // check 256 bytes since we don't know the real length
            LPVOID pvData = MapCallerPtr(lpReg[cReg].pData, lpReg[cReg].dwLen); // really only need READ access
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

    // Registry path of the device driver (from HLM\Drivers\BuiltIn or HLM\Drivers\PCMCIA)
    if (pszRegPath != NULL) {    
        status = RegSetValueEx(
            hkActive,
            DEVLOAD_DEVKEY_VALNAME,
            0,
            DEVLOAD_DEVKEY_VALTYPE,
            (PBYTE)pszRegPath,
            sizeof(TCHAR)*(_tcslen(pszRegPath) + 1));
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
static DWORD
ReadBusDriverValues(HKEY hkActive, pBusDriverValues_t pbdv)
{
    DWORD dwStatus, dwSize, dwType;
    DEBUGCHK(hkActive != NULL);
    DEBUGCHK(pbdv != NULL);

    if(PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        // don't allow untrusted callers to specify bus parent parameters -- they
        // are not a bus driver!
        pbdv->hParent = NULL;
        pbdv->szBusName[0] = 0;
    } else {
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
            pbdv->szBusName[ARRAYSIZE(pbdv->szBusName) - 1] = 0;
        }
    }

    // got here with no fatal errors so return success
    dwStatus = ERROR_SUCCESS;
    return dwStatus;
}

static DWORD
FinalizeActiveKey(
    HKEY hkActive,
    const fsdev_t *lpdev
    )
{
    DWORD status;

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
// Function to RegisterDevice a device driver and add it to the active device list
// in HLM\Drivers\Active and then signal the system that a new device is available.
//
// Return the HANDLE from RegisterDevice.
//
HANDLE
I_ActivateDeviceEx(
    LPCTSTR   RegPath,
    LPCREGINI lpReg,
    DWORD     cReg,
    LPVOID    lpvParam
    )
{
    DWORD status, dwId, dwLegacyIndex;
    TCHAR ActivePath[REG_PATH_LEN];
    fsdev_t * lpdev = NULL;
    fscandidatedev_t * lpcandidatedev = NULL;
    RegActivationValues_t av;
    BusDriverValues_t bdv;
    BOOL fWantCandidate = FALSE;
    HKEY hkActive = NULL;
    
    DEBUGMSG(ZONE_ACTIVE,
        (TEXT("DEVICE!I_ActivateDeviceEx: starting HLM\\%s\r\n"), RegPath));
    
    // Read activation values from the device key
    status = RegReadActivationValues(RegPath, &av);
    if(status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
            (TEXT("DEVICE!I_ActivateDeviceEx: can't find all required activation values in '%s'\r\n"),
            RegPath));
        SetLastError(ERROR_BAD_CONFIGURATION);
        return NULL;;
    }
    
    // are we supposed to load the driver?
    if (av.Flags & DEVFLAGS_NOLOAD) {
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!I_ActivateDeviceEx: not loading %s with DEVFLAGS_NOLOAD\n"),
            RegPath));
        // not supposed to load the driver, return NULL with ERROR_SUCCESS
        SetLastError(ERROR_SUCCESS);
        return NULL;
    }
    if ((av.Flags & DEVFLAGS_BOOTPHASE_1) && (g_BootPhase > 1)) { // BOOTPHASE_1=loaded in phase 1 and never again
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!I_ActivateDeviceEx: skipping %s in BootPhase %d\n"),
            RegPath, g_BootPhase));
        // wrong boot phase for this driver, return NULL with ERROR_SUCCESS
        SetLastError(ERROR_SUCCESS);
        return NULL;
    }
    
    status = CreateActiveKey(ActivePath, ARRAYSIZE(ActivePath), &dwId, &hkActive);
    if(status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!I_ActivateDeviceEx: CreateActiveKey() failed %d\n"),
            status));
        DEBUGCHK(hkActive == NULL);
        goto done;
    }
    DEBUGCHK(hkActive != NULL);
    DEBUGCHK(ActivePath[0] != 0);

    // populate the active key with initial values
    status = InitializeActiveKey(hkActive, RegPath, lpReg, cReg);
    if(status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!I_ActivateDeviceEx: InitializeActiveKey() failed %d\n"),
            status));
        goto done;
    }

    // obtain bus driver parameters
    status = ReadBusDriverValues(hkActive, &bdv);
    if(status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!I_ActivateDeviceEx: ReadBusDriverValues() failed %d\n"),
            status));
        goto done;
    }

    // Is this a named device?
    dwLegacyIndex = av.Index;
    if(av.Prefix[0] == 0) {
        // Device is not accessible via the $device or legacy namespaces -- check 
        // support for the $bus namespace.  It needs to have a bus name provided
        // by the bus driver and a prefix that will allow us to get entry points to
        // create handles.
        if(bdv.szBusName[0]) {
            fWantCandidate = TRUE;
            lpcandidatedev = CreateUniqueCandidateDev(av.Prefix, av.Index, bdv.szBusName);
        }
    } else {
        // this is a named device, was an index specified?
        fWantCandidate = TRUE;
        if(av.Index != -1) {
            // yes, make sure there are no duplicates
            lpcandidatedev = CreateUniqueCandidateDev(av.Prefix, av.Index, bdv.szBusName);
        } else {
            // Create a new index and allocate a candidate device to reserve the 
            // name/index combination.  Start at index 1 and count up.  Index 10 will
            // appear as instance index 0 in the legacy namespace.
            SetLastError(ERROR_SUCCESS);
            for(av.Index = 1; av.Index < MAXDEVICEINDEX; av.Index++) {
                lpcandidatedev = CreateUniqueCandidateDev(av.Prefix, av.Index, bdv.szBusName);
                if(lpcandidatedev != NULL) {
                    if(av.Index == 10) {
                        dwLegacyIndex = 0;
                    } else {
                        dwLegacyIndex = av.Index;
                    }
                    break;
                } else {
                    // check if out of memory
                    if(GetLastError() == ERROR_OUTOFMEMORY) {
                        break;
                    }
                }
            }
        }
    }
    

    // were we looking for a uniquely named device?
    if(fWantCandidate) {
        // did we get a unique device candidate?
        if(lpcandidatedev == NULL) {
            status = ERROR_DEVICE_IN_USE;
            goto done;
        }
    }
    
    // allocate the device structure
    lpdev = CreateDevice(av.Prefix, av.Index, dwLegacyIndex, dwId, av.DevDll, av.Flags,
        av.BusPrefix, bdv.szBusName, RegPath, bdv.hParent);
    if (lpdev == NULL) {
        status = GetLastError();
        goto done;
    }

    // create the active key for this device and populate it
    status = FinalizeActiveKey(hkActive, lpdev);
    if(status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE,
            (TEXT("DEVICE!I_ActivateDeviceEx: FinalizeActiveKey() failed %d\n"),
            status));
        goto done;
    }

    // close the registry key before passing its path to the device driver
    RegCloseKey(hkActive);
    hkActive = NULL;
    
    // Default context is registry path
    if (av.bOverrideContext == FALSE) {
        av.Context = (DWORD)ActivePath;
    }
    
    // start the driver
    status = LaunchDevice(lpdev, av.Context, av.Flags, lpvParam);
    
done:
    // close the active key if we have it open
    if(hkActive != NULL) {
        RegCloseKey(hkActive);
    }
    
    // if we've allocated a candidate device structure we should free it now.  The device
    // has either been instantiated or it hasn't.
    if(lpcandidatedev != NULL) {
        DeleteCandidateDev(lpcandidatedev);
    }

    // If we failed then let's clean up the module and data
    if (status == ERROR_SUCCESS) {
        DEBUGCHK(lpdev != NULL);
        DEBUGCHK(ActivePath[0] != 0);
        
        if((av.Flags & DEVFLAGS_UNLOAD) != 0) {
            // automatically unload the driver
            DeleteDevice(lpdev);
            DeleteActiveKey(ActivePath);
            SetLastError(ERROR_SUCCESS);
            lpdev = NULL;
        } else {
            // advertise the device's existence to the outside world
            PnpAdvertiseInterfaces(lpdev);
            // Only stream interfaces can use receive ioctls.
            if (av.bHasPostInitCode) {
                WCHAR szDeviceName[64];
                // Call the new device's IOCTL_DEVICE_INITIALIZED 
                DEBUGCHK(lpdev->pszDeviceName != NULL);
                DEBUGCHK(lpdev->pszDeviceName[0] != 0);
                wsprintf(szDeviceName, L"$device\\%s", lpdev->pszDeviceName);
                DevicePostInit(szDeviceName, av.PostInitCode, lpdev, RegPath );
            }
            if (av.bHasBusPostInitCode && bdv.szBusName[0]!=0 ) { // This has bus PostInitCode.
                WCHAR szDeviceName[64];
                wsprintf(szDeviceName, L"$bus\\%s", bdv.szBusName);
                DEBUGCHK(wcslen(bdv.szBusName)<64-6);
                DevicePostInit(szDeviceName, av.BusPostInitCode, lpdev, RegPath);
            }
        }
    } else {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
            (TEXT("DEVICE!I_ActivateDeviceEx: couldn't activate: prefix %s, index %d, dll %s, context 0x%x\r\n"),
            av.Prefix[0] != 0 ? av.Prefix : TEXT("<nil>"), 
            av.Index, av.DevDll, av.Context));
        
        // release resources
        if (lpdev) {
            DeleteDevice(lpdev);
            lpdev = NULL;
        }
        if(ActivePath[0] != 0) {
            DeleteActiveKey(ActivePath);
        }
        SetLastError (status);
    }
    
    return (HANDLE) lpdev;
}   // I_ActivateDeviceEx

//
// Function to DeregisterDevice a device, remove its active key, and announce
// the device's removal to the system.
//
BOOL
I_DeactivateDevice(
    HANDLE hDevice
    )
{
    TCHAR szActivePath[REG_PATH_LEN];
    fsdev_t *lpdev = (fsdev_t *) hDevice;
    DWORD dwStatus = ERROR_NOT_FOUND;

    // validate the device handle
    EnterCriticalSection(&g_devcs);
    if(FindDeviceByHandle(lpdev)) {
        // make sure the device was activated (not registered) and isn't deactivating
        if((lpdev->wFlags & DF_REGISTERED) == 0 && (lpdev->wFlags & DF_DEACTIVATING) == 0) {
            // Can the caller unload the device?  Check here even though I_DeregisterDevice()
            // also checks because by then we are already freeing resources.
            if((lpdev->wFlags & DF_TRUSTED_LOADER) != 0 && PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
                // driver loaded by a trusted process can't be unloaded by an untrusted one
                dwStatus = ERROR_ACCESS_DENIED;
            } else {
                lpdev->wFlags |= DF_DEACTIVATING;
                dwStatus = ERROR_SUCCESS;
            }
        }
    }
    LeaveCriticalSection(&g_devcs);

    // return false if this wasn't a valid device or if it's already
    // being deactivated
    if(dwStatus != ERROR_SUCCESS) {
        SetLastError(dwStatus);
        return FALSE;
    }

    // deregister any interfaces the device has associated with it
    PnpDeadvertiseInterfaces(lpdev);

    // determine the driver's active key path
    wsprintf(szActivePath, L"%s\\%02u", s_ActiveKey, lpdev->dwId);

    // unload the driver
    I_DeregisterDevice(lpdev);

    // delete the device's active key
    DeleteActiveKey(szActivePath);
    
    return TRUE;
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
    
    EnterCriticalSection(&g_devcs);
    
    // look for the device
    if(lpdev != NULL) {
        if(!FindDeviceByHandle(lpdev)) {
            DWORD dwCurrentThreadId = GetCurrentThreadId();
            fsdev_t * lpTrav;
            
            // Is it in the process of activating?  Only make the
            // device visible to the thread loading the driver.
            for (lpTrav = (fsdev_t *)g_ActivatingDevs.Flink;
                 lpTrav != (fsdev_t *)&g_ActivatingDevs;
                 lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
                if (lpTrav == lpdev && lpTrav->dwActivateId == dwCurrentThreadId) {
                    break;
                }
            }

            // did we find the device?
            if(lpTrav == (fsdev_t *)&g_ActivatingDevs) {
                dwStatus = ERROR_NOT_FOUND;
            }
        }
    }
    
    // if we found the device, extract the information we need from it
    if(dwStatus == ERROR_SUCCESS) {
        // load the caller's structure with information
        __try {
            HRESULT hr;
            pdi->hDevice = lpdev;
            pdi->hParentDevice = lpdev->hParent;
            if(pdi->hParentDevice != NULL) {
                // validate the parent device handle
                if(!FindDeviceByHandle(pdi->hParentDevice)) {
                    pdi->hParentDevice = NULL;
                }
            }
            if(lpdev->pszLegacyName != NULL) {
                DEBUGCHK(lpdev->pszLegacyName[0] != 0);
                hr = StringCchCopyW(pdi->szLegacyName, ARRAYSIZE(pdi->szLegacyName), lpdev->pszLegacyName);
            } else {
                pdi->szLegacyName[0] = 0;
            }
            if(lpdev->pszDeviceName) {
                DEBUGCHK(lpdev->pszDeviceName[0] != 0);
                hr = StringCchCopyW(pdi->szDeviceName, ARRAYSIZE(pdi->szDeviceName), pszDeviceRoot);
                hr = StringCchCatW(pdi->szDeviceName, ARRAYSIZE(pdi->szDeviceName), lpdev->pszDeviceName);
            } else {
                pdi->szDeviceName[0] = 0;
            }
            if(lpdev->pszBusName != NULL) {
                DEBUGCHK(lpdev->pszBusName[0] != 0);
                hr = StringCchCopyW(pdi->szBusName, ARRAYSIZE(pdi->szBusName), pszBusRoot);
                hr = StringCchCatW(pdi->szBusName, ARRAYSIZE(pdi->szBusName), lpdev->pszBusName);
            } else {
                pdi->szBusName[0] = 0;
            }
            if((lpdev->wFlags & DF_REGISTERED) != 0) {
                DEBUGCHK(lpdev->pszDeviceKey == NULL);
                pdi->szDeviceKey[0] = 0;
            } else {
                DEBUGCHK(lpdev->pszDeviceKey != NULL);
                wcsncpy(pdi->szDeviceKey, lpdev->pszDeviceKey, ARRAYSIZE(pdi->szDeviceKey) - 1);
                pdi->szDeviceKey[ARRAYSIZE(pdi->szDeviceKey) - 1] = 0;
            }
            pdi->dwSize = sizeof(*pdi);
            dwStatus = ERROR_SUCCESS;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            dwStatus = ERROR_INVALID_PARAMETER;
        }        
    }
    
    LeaveCriticalSection(&g_devcs);

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
                0,
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
    RootKeyPath[ARRAYSIZE(RootKeyPath) - 1] = 0;
                
    if (status != ERROR_SUCCESS) {
        // Root key value not found, thus root key is Drivers
        _tcscpy(RootKeyPath, DEVLOAD_DRIVERS_KEY);
    }

    // Close previous root key
    RegCloseKey(RootKey);
    DEBUGMSG(1,
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
