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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#define _DEVMGRDATA_DEF

#include <windows.h>
#include <devload.h>
#include <console.h>
#include <errorrep.h>

#include "reflector.h"
#include "devmgrp.h"
#include "celogdev.h"
#include "devmgrif.h"
#include "pmif.h"
#include "iormif.h"
#include "devzones.h"

#ifdef DEBUG
//
// These defines must match the ZONE_* defines in devmgr.h
//
#define DBG_ERROR      1
#define DBG_WARNING    2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_BUILTIN    16
#define DBG_ACTIVE     64
#define DBG_RESMGR     128
#define DBG_FSD        256
#define DBG_DYING      512
#define DBG_BOOTSEQ 1024
#define DBG_PNP         2048

DBGPARAM dpCurSettings = {
    TEXT("DEVLOAD"), {
    TEXT("Errors"),TEXT("Warnings"),TEXT("Functions"),TEXT("Initialization"),
    TEXT("Built-In Devices"),TEXT("Undefined"),TEXT("Active Devices"),TEXT("Resource Manager"),
    TEXT("File System Drvr"),TEXT("Dying Devs"),TEXT("Boot Sequence"),TEXT("PnP"),
    TEXT("Services"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined") },
    DBG_ERROR | DBG_WARNING | DBG_INIT
};
#endif  // DEBUG

// global variables
CRITICAL_SECTION g_devcs;
LIST_ENTRY g_DevChain;
LIST_ENTRY g_ActivatingDevs;
LIST_ENTRY g_DyingDevs;
LIST_ENTRY g_CandidateDevs;
fsopendev_t *g_lpOpenDevs;
fsopendev_t *g_lpDyingOpens;
HANDLE g_hDevApiHandle, g_hDevFileApiHandle, g_hCleanEvt;
BOOL g_bSystemInitd;

// Boot phase measure where we are in the boot process.
// Zero means we haven't started the device manager yet;
// One is used to find the registry;
// Two is used during initial device driver loading;
// Three is normal run mode.
DWORD g_BootPhase = 0;

const TCHAR s_DllName_ValName[] = DEVLOAD_DLLNAME_VALNAME;
const TCHAR s_ActiveKey[] = DEVLOAD_ACTIVE_KEY;
const TCHAR s_BuiltInKey[] = DEVLOAD_BUILT_IN_KEY;

VOID DeleteActiveKey( LPCWSTR ActivePath);

const PFNVOID FDevApiMethods[] = {
//xref ApiSetStart        
    (PFNVOID)DM_DevProcNotify,              // 0
    (PFNVOID)0,                             // 1
    (PFNVOID)DM_Unsupported,                // 2 DM_RegisterDevice
    (PFNVOID)DM_Unsupported,                // 3 DM_DeregisterDevice
    (PFNVOID)0,                             // 4 - was CloseAllDeviceHandles
    (PFNVOID)0,                             // 5 - was CreateDeviceHandle
    (PFNVOID)0,                             // 6
    (PFNVOID)0,                             // 7
    (PFNVOID)DM_DeactivateDevice,           // 8
    (PFNVOID)0,                             // 9
    (PFNVOID)DM_GetDeviceByIndex,           // 10
    (PFNVOID)DM_CeResyncFilesys,            // 11
    (PFNVOID)DM_ActivateDeviceEx,           // 12
    (PFNVOID)0,                             // 13 - moved to filesys - was RequestDeviceNotifications
    (PFNVOID)0,                             // 14 - moved to filesys - was StopDeviceNotifications
    (PFNVOID)0,                             // 15 - was GetDevicePathFromPnp
    (PFNVOID)IORM_ResourceCreateList,       // 16
    (PFNVOID)IORM_ResourceAdjust,           // 17
    (PFNVOID)PM_GetSystemPowerState,        // 18
    (PFNVOID)PM_SetSystemPowerState,        // 19
    (PFNVOID)PM_SetPowerRequirement,        // 20
    (PFNVOID)PM_ReleasePowerRequirement,    // 21
    (PFNVOID)PM_RequestPowerNotifications,  // 22
    (PFNVOID)PM_StopPowerNotifications,     // 23
    (PFNVOID)0,                             // 24 - deprecated - VetoPowerNotification
    (PFNVOID)PM_DevicePowerNotify,          // 25
    (PFNVOID)PM_RegisterPowerRelationship,  // 26
    (PFNVOID)PM_ReleasePowerRelationship,   // 27
    (PFNVOID)PM_SetDevicePower,             // 28
    (PFNVOID)PM_GetDevicePower,             // 29
    (PFNVOID)0,                             // 30 - moved to filesys - was AdvertiseInterface
    (PFNVOID)IORM_ResourceMarkAsShareable,  // 31
    (PFNVOID)IORM_ResourceDestroyList,      // 32
    (PFNVOID)IORM_ResourceRequestEx,        // 33
    (PFNVOID)DM_GetDeviceInformationByDeviceHandle,  // 34
    (PFNVOID)DM_EnumDeviceInterfaces,        // 35
    (PFNVOID)DM_REL_UDriverProcIoControl,    // 36
    (PFNVOID)DM_EnumServices,               // 37,
//xref ApiSetEnd
};

const PFNVOID ExFDevApiMethods[] = {
//xref ApiSetStart        
    (PFNVOID)DM_Unsupported,                // 0
    (PFNVOID)0,                             // 1
    (PFNVOID)DM_RegisterDevice,                // 2
    (PFNVOID)DM_DeregisterDevice,                // 3
    (PFNVOID)0,                             // 4 - was CloseAllDeviceHandles
    (PFNVOID)0,                             // 5
    (PFNVOID)0,                             // 6
    (PFNVOID)0,                             // 7
    (PFNVOID)DM_DeactivateDevice,           // 8
    (PFNVOID)0,                             // 9
    (PFNVOID)DM_GetDeviceByIndex,           // 10
    (PFNVOID)DM_CeResyncFilesys,            // 11
    (PFNVOID)EX_DM_ActivateDeviceEx,        // 12
    (PFNVOID)0,                             // 13 - moved to filesys - was RequestDeviceNotifications
    (PFNVOID)0,                             // 14 - moved to filesys - was StopDeviceNotifications
    (PFNVOID)0,                             // 15 - was GetDevicePathFromPnp
    (PFNVOID)IORM_ResourceCreateList,       // 16
    (PFNVOID)IORM_ResourceAdjust,           // 17
    (PFNVOID)PM_GetSystemPowerState,        // 18
    (PFNVOID)PM_SetSystemPowerState,        // 19
    (PFNVOID)EX_PM_SetPowerRequirement,     // 20
    (PFNVOID)EX_PM_ReleasePowerRequirement, // 21
    (PFNVOID)EX_PM_RequestPowerNotifications,// 22
    (PFNVOID)PM_StopPowerNotifications,     // 23
    (PFNVOID)0,                             // 24 - deprecated - VetoPowerNotification
    (PFNVOID)EX_PM_DevicePowerNotify,       // 25
    (PFNVOID)EX_PM_RegisterPowerRelationship,// 26
    (PFNVOID)PM_ReleasePowerRelationship,   // 27
    (PFNVOID)PM_SetDevicePower,             // 28
    (PFNVOID)PM_GetDevicePower,             // 29
    (PFNVOID)0,                             // 30 - moved to filesys - was AdvertiseInterface
    (PFNVOID)IORM_ResourceMarkAsShareable,  // 31
    (PFNVOID)IORM_ResourceDestroyList,      // 32
    (PFNVOID)IORM_ResourceRequestEx,        // 33
    (PFNVOID)EX_DM_GetDeviceInformationByDeviceHandle,  // 34
    (PFNVOID)EX_DM_EnumDeviceInterfaces,    // 35
    (PFNVOID)DM_REL_UDriverProcIoControl,   // 36
    (PFNVOID)EX_DM_EnumServices             // 37 - EnumServices
//xref ApiSetEnd
};

#define NUM_FDEV_APIS (sizeof(FDevApiMethods)/sizeof(FDevApiMethods[0]))

const ULONGLONG FDevApiSigs[NUM_FDEV_APIS] = {
    FNSIG3(DW, DW, DW),       // ProcNotify
    FNSIG0(),
    FNSIG4(PTR, DW, PTR, DW), // RegisterDevice
    FNSIG1(DW),               // DeregisterDevice
    FNSIG0(),                 // CloseAllDeviceHandles - deleted; was unused and a security risk
    FNSIG0(),                 // Obsolete..CreateDeviceHandle
    FNSIG0(),                 // unsupported and moved to thunk - was LoadFSD
    FNSIG0(),                 // ActivateDevice - obsolete; translated in coredll
    FNSIG1(DW),               // DeactivateDevice
    FNSIG0(),                 // unsupported and moved to thunk - was LoadFSDEx
    FNSIG2(DW, IO_PDW),       // GetDeviceByIndex
    FNSIG1(DW),               // CeResyncFilesys
    FNSIG4(I_WSTR, DW, DW, DW),// ActivateDeviceEx, Marshel lpReg in function.
    FNSIG0(),                 // moved to filesys - was RequestDeviceNotifications
    FNSIG0(),                 // moved to filesys - was StopDeviceNotifications
    FNSIG0(),                 // GetDevicePathFromPnp - obsolete; implemented in coredll
    FNSIG3(DW, DW, DW),       // ResourceCreateList
    FNSIG4(DW, DW, DW, DW),   // ResourceAdjust
    FNSIG3(O_PTR, DW, O_PDW), // GetSystemPowerState
    FNSIG3(I_WSTR, DW, DW),   // SetSystemPowerState
    FNSIG5(I_WSTR, DW, DW, I_WSTR, DW), // SetPowerRequirement
    FNSIG1(DW),               // ReleasePowerRequirement
    FNSIG2(DW, DW),           // RequestPowerNotifications
    FNSIG1(DW),               // StopPowerNotifications
    FNSIG0(),                 // deprecated - VetoPowerNotification
    FNSIG3(I_WSTR, DW, DW),   // DevicePowerNotify
    FNSIG4(I_WSTR, I_WSTR, IO_PDW, DW), // RegisterPowerRelationship
    FNSIG1(DW),               // ReleasePowerRelationship
    FNSIG3(I_WSTR, DW, DW),   // SetDevicePower
    FNSIG3(I_WSTR, DW, O_PDW),// GetDevicePower   
    FNSIG0(),                 // moved to filesys - was AdvertiseInterface
    FNSIG4(DW, DW, DW, DW),   // ResourceMarkAsShareable
    FNSIG1(DW),               // ResourceDestroyList
    FNSIG4(DW, DW, DW, DW),   // ResourceRequestEx
    FNSIG2(DW, IO_PDW),       // GetDeviceInformationByDeviceHandle
    FNSIG5(DW, DW, IO_PDW, O_PDW, O_PDW),  // EnumDeviceInterfaces
    FNSIG6(DW, I_PTR, DW, IO_PTR, DW, O_PDW), // REL_UserProcIoControl
    FNSIG1(IO_PDW),  // EnumServices
};

const PFNVOID DevFileApiMethods[] = {
    (PFNVOID)DM_DevCloseFileHandle,
    (PFNVOID)DM_DevPreCloseFileHandle,
    (PFNVOID)DM_DevReadFile,
    (PFNVOID)DM_DevWriteFile,
    (PFNVOID)DM_DevGetFileSize,
    (PFNVOID)DM_DevSetFilePointer,
    (PFNVOID)DM_DevGetDeviceInformationByFileHandle,
    (PFNVOID)DM_DevFlushFileBuffers,
    (PFNVOID)DM_DevGetFileTime,
    (PFNVOID)DM_DevSetFileTime,
    (PFNVOID)DM_DevSetEndOfFile,
    (PFNVOID)DM_DevDeviceIoControl,
};
const PFNVOID ExDevFileApiMethods[] = {
    (PFNVOID)DM_DevCloseFileHandle,
    (PFNVOID)DM_DevPreCloseFileHandle,
    (PFNVOID)DM_DevReadFile,
    (PFNVOID)DM_DevWriteFile,
    (PFNVOID)DM_DevGetFileSize,
    (PFNVOID)DM_DevSetFilePointer,
    (PFNVOID)EX_DM_DevGetDeviceInformationByFileHandle,
    (PFNVOID)DM_DevFlushFileBuffers,
    (PFNVOID)DM_DevGetFileTime,
    (PFNVOID)DM_DevSetFileTime,
    (PFNVOID)DM_DevSetEndOfFile,
    (PFNVOID)DM_DevDeviceIoControl,
};

#define NUM_FAPIS (sizeof(DevFileApiMethods)/sizeof(DevFileApiMethods[0]))

const ULONGLONG DevFileApiSigs[NUM_FAPIS] = {
    FNSIG1(DW),                 // CloseFileHandle
    FNSIG1(DW),                 // PreCloseFileHandle
    FNSIG5(DW,I_PTR,DW,O_PDW,IO_PDW),   // ReadFile
    FNSIG5(DW,O_PTR,DW,O_PDW,IO_PDW),   // WriteFile
    FNSIG2(DW,O_PDW),                   // GetFileSize
    FNSIG4(DW,DW,O_PDW,DW),             // SetFilePointer
    FNSIG2(DW,O_PDW),                   // GetDeviceInformationByFileHandle
    FNSIG1(DW),                         // FlushFileBuffers
    FNSIG4(DW,O_PDW,O_PDW,O_PDW),       // GetFileTime
    FNSIG4(DW,IO_PDW,IO_PDW,IO_PDW),    // SetFileTime
    FNSIG1(DW),                         // SetEndOfFile,
    FNSIG8(DW, DW, I_PTR, DW, IO_PTR, DW, O_PDW, IO_PDW), // DeviceIoControl
};

// This routine initializes low memory thresholds based on the amount
// of memory available when the device manager is loaded.
void InitOOMSettings()
{
    SYSTEM_INFO    SysInfo;
    GetSystemInfo (&SysInfo);

    // Calling this will set up the initial critical memory handlers and
    // enable memory scavenging in the kernel
    SetOOMEvent (NULL, 30, 15, 0x4000 / SysInfo.dwPageSize,
                 (0x2000 / SysInfo.dwPageSize) > 2 ?
                 (0x2000 / SysInfo.dwPageSize) : 2);    
}

//
// ProcessAutoDeregisterDevs - called from the main thread as part of periodic clean up.
//
// Auto deregister devices are devices that need to be deregistered in a context that will
// avoid a deadlock between the device CS and the loader CS.
// A device gets marked as auto-deregister when DeviceIoControl is called with
//
void ProcessAutoDeregisterDevs(void)
{
    BOOL bFreedOne;
    fsdev_t* lpdev;

    do {
        EnterCriticalSection(&g_devcs);
        bFreedOne = FALSE;
        // walking the chain of *active* devs, looking for ALL that want to deregister
        // don't care about refcount. That's handled by deregister & the dying-devs list
        DEBUGMSG(ZONE_DYING, (L"Device:ProcessAutoDeregisterDevs About to walk main list\r\n"));
        for (lpdev = (fsdev_t *)g_DevChain.Flink;
             lpdev != (fsdev_t *)&g_DevChain;
             lpdev = (fsdev_t *)(lpdev->list.Flink)) {
            if (lpdev->wFlags & DF_AUTO_DEREGISTER) {
                LeaveCriticalSection(&g_devcs);
                // don't remove from list. DeregisterDevice will do that
                DEBUGMSG(ZONE_DYING, (L"Device:ProcessAutoDeregisterDevs FOUND auto-deregister lpdev=%x\r\n", lpdev));
                DM_DeregisterDevice((HANDLE)lpdev);
                bFreedOne = TRUE;
                break;
                // break out of inner loop, because after letting go of critsec &
                // freeing lpdev, our loop traversal is not valid anymore. So start over
            }
        }
        if (lpdev == (fsdev_t *)&g_DevChain) {
            LeaveCriticalSection(&g_devcs);
            break;
        }
    } while (bFreedOne);
    // keep looping as long as we have something to do

}   // ProcessAutoDeregisterDevs

//
// ProcessDyingDevs - called from the main thread as part of periodic clean up.
//
// Dying devices are devices for which DeregisterDevice has been called while there are
// still open handles to the device. This can occur when a PC card storage device has been
// removed. A filesystem driver may be in the middle of mounting the device, so we can't free
// the device structure just yet; it is put on the g_DyingDevs list.
//
void ProcessDyingDevs(void)
{
    BOOL bFreedOne;
    fsdev_t* lpdev;
    LPEXCEPTION_POINTERS pep;

    DEBUGMSG(ZONE_DYING, (L"Device:ProcessDyingDevs About to walk dying list\r\n"));
    while (!IsListEmpty(&g_DyingDevs)) 
    {
        EnterCriticalSection(&g_devcs);
        bFreedOne = FALSE;
        // walk the chain of dying devs (already deregistered. just waiting to be unloaded)
        for (lpdev = (fsdev_t *)g_DyingDevs.Flink;
             lpdev != (fsdev_t *)&g_DyingDevs;
             lpdev = (fsdev_t *)lpdev->list.Flink) {
            if (!lpdev->dwRefCnt) {
                RemoveEntryList((PLIST_ENTRY)lpdev);
                LeaveCriticalSection(&g_devcs);
                DEBUGMSG(ZONE_DYING, (L"Device:ProcessDyingDevs FOUND dying dev lpdev=%x\r\n", lpdev));
                if(lpdev->fnPreDeinit != NULL) {
                    __try {
                        lpdev->fnDeinit(lpdev->dwData);
                    }
                    __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER ) {
                        DEBUGMSG(ZONE_WARNING, (_T("I_DeregisterDevice: exception in final deinit entry point\r\n")));
                    }
                }
                // determine the driver's active key path
                if (lpdev->dwId != ID_REGISTERED_DEVICE ) {
                    TCHAR szActivePath[REG_PATH_LEN];
                    HRESULT hRet = StringCchPrintf(szActivePath,_countof(szActivePath), L"%s\\%02u", s_ActiveKey, lpdev->dwId);
                    // delete the device's active key
                    if (SUCCEEDED(hRet))
                        DeleteActiveKey(szActivePath);
                    else
                        ASSERT(FALSE);
                }
                DeleteDevice(lpdev);
                bFreedOne = TRUE;
                break; 
                // break out of inner loop, because after letting go of critsec &
                // freeing lpdev, our loop traversal is not valid anymore. So start over
            }
        }
        if (bFreedOne == FALSE) {
            // Couldn't free anyone
            LeaveCriticalSection(&g_devcs);
            Sleep(5000);    // snooze, and try again...
        }
    }
}   // ProcessDyingDevs


//
// ProcessDyingOpens - called from the main thread as part of periodic clean up.
//
// Dying opens are orphaned device handles. A device open handle can be orphaned if
// CloseHandle is called by one thread while another thread using the same handle is
// blocked in a call to the device driver, for instance xxx_IoControl. When xxx_IoControl
// returns, DM_DevDeviceIoControl will access the open handle structure so it can't be freed
// yet; it is put on the g_lpDyingOpens list.
//
void ProcessDyingOpens(void)
{
    fsopendev_t *fsodev;
    fsopendev_t *fsodevprev;
    LPEXCEPTION_POINTERS pep;

    DEBUGMSG(ZONE_DYING, (L"Device:ProcessDyingOpens About to walk dying opens list\r\n"));
    EnterCriticalSection(&g_devcs);
    fsodevprev = fsodev = g_lpDyingOpens;
    while (fsodev) {
        if (fsodev->dwOpenRefCnt == 0) {
            fsdev_t *lpdev = fsodev->lpDev;
            if(lpdev != NULL && (lpdev->wFlags & DF_DYING)==0 && lpdev->fnPreClose != NULL) {
                LeaveCriticalSection(&g_devcs);
                __try {
                    lpdev->fnClose(fsodev->dwOpenData);
                }
                __except(pep = GetExceptionInformation(), ReportFault(pep,0), EXCEPTION_EXECUTE_HANDLER ) {
                    DEBUGMSG(ZONE_WARNING, (_T("ProcessDyingOpens: exception calling final close entry point\r\n")));
                }
                EnterCriticalSection(&g_devcs);
            }

            // Unlink and free unreferenced open
            if (fsodev == g_lpDyingOpens) {
                g_lpDyingOpens = fsodev->nextptr;
                LocalFree(fsodev);
                fsodev = fsodevprev = g_lpDyingOpens;
            } else {
                fsodevprev->nextptr = fsodev->nextptr;
                LocalFree(fsodev);
                fsodev = fsodevprev->nextptr;
            }
        } else {
            fsodevprev = fsodev;
            fsodev = fsodev->nextptr;
        }
    }
    LeaveCriticalSection(&g_devcs);
}   // ProcessDyingOpens


// Uses registry to call SignalStarted with the correct args.  Note that ideally
// device manager should NOT know this info is in the registry!
static void SignalStartedUsingReg()
{
#define MAX_APPSTART_KEYNAME 128
    HKEY   hKey;
    WCHAR  szName[MAX_APPSTART_KEYNAME];
    WCHAR  szVal[MAX_APPSTART_KEYNAME];
    LPWSTR lpszArg = NULL;
    DWORD  dwTemp, dwType, dwNameSize, dwValSize, i;

    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("init"), 0, NULL, 0, 0,
                       NULL, &hKey, &dwTemp) != ERROR_SUCCESS) {
        DEBUGCHK(0);
        return;
    }

    dwNameSize = MAX_APPSTART_KEYNAME;
    dwValSize = MAX_APPSTART_KEYNAME * sizeof(WCHAR);
    i = 0;
    while (RegEnumValue(hKey, i, szName, &dwNameSize, 0, &dwType,
                        (LPBYTE)szVal, &dwValSize) == ERROR_SUCCESS) {
        if ((dwType == REG_SZ) && !wcsncmp(szName, TEXT("Launch"), 6) // 6 for "launch"
            && !wcscmp(szVal, TEXT("device.dll"))) {
            lpszArg = szName + 6; // 6 to go past "launch"
            break;
        }
        
        dwNameSize = MAX_APPSTART_KEYNAME;
        dwValSize = MAX_APPSTART_KEYNAME * sizeof(WCHAR);
        i++;
    }

    RegCloseKey(hKey);

    if (lpszArg) {
        SignalStarted(_wtol(lpszArg));
    } else {
        // device.exe is not in the registry!
        DEBUGMSG(1, (TEXT("Device manager must be listed in HKLM\\init!\r\n")));
        DEBUGCHK(0);
    }
}

// This routine initializes the Device Manager, Power Manager, and I/O Resource
// Manager APIs, launches device drivers, and processes device driver load/unload.
int WINAPI 
StartDeviceManager(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmShow)
{

    HINSTANCE hCeddkDll;
    HANDLE hEvent;

    if (WaitForAPIReady(SH_DEVMGR_APIS,0)==WAIT_OBJECT_0) {
        return 0;
    }
    
    DEBUGMSG(ZONE_BOOTSEQ, (TEXT("DEVICE: Starting boot phase 1\n")));

    // PHASE 1
    g_BootPhase = 1;
    InitOOMSettings();
    InitializeListHead(&g_DevChain);
    InitializeListHead(&g_ActivatingDevs);
    InitializeListHead(&g_DyingDevs);
    InitializeListHead(&g_CandidateDevs);
    g_hCleanEvt = CreateEvent(NULL, FALSE, FALSE, NULL);    
    g_hDevApiHandle = CreateAPISet("WFLD", NUM_FDEV_APIS, ExFDevApiMethods, FDevApiSigs);
    g_hDevFileApiHandle = CreateAPISet("W32D", NUM_FAPIS, ExDevFileApiMethods, DevFileApiSigs);
    RegisterDirectMethods(g_hDevFileApiHandle,DevFileApiMethods);
    RegisterAPISet(g_hDevFileApiHandle, HT_FILE | REGISTER_APISET_TYPE);
    InitializePnPNotifications();
    InitializeCriticalSection(&g_devcs);
    ResourceInitModule();
    ResourceInitFromRegistry(TEXT("Drivers\\Resources"));
    SetPowerOffHandler((FARPROC) DevMgrPowerOffHandler);
    RegisterDirectMethods(g_hDevApiHandle, FDevApiMethods );
    RegisterAPISet(g_hDevApiHandle, SH_DEVMGR_APIS);
    InitDeviceFilesystems();

    // Indicate that the device manager is up and running
    hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, _T("SYSTEM/DevMgrApiSetReady"));
    DEBUGCHK(hEvent != NULL);
    if (hEvent != NULL) {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }

    // Calibrate stall counter that is used for StallExecution
    hCeddkDll = LoadLibrary (TEXT("ceddk.dll"));
    if (NULL != hCeddkDll) {
        pCalibrateStallFn fnCalibrateStall = (pCalibrateStallFn)GetProcAddress(hCeddkDll, TEXT("CalibrateStallCounter"));
        if (!fnCalibrateStall) {
            DEBUGMSG(ZONE_BOOTSEQ,  (L"GetProcAddress failed on ceddk.dll\r\n"));
            FreeLibrary(hCeddkDll);       
        }
        else {
            fnCalibrateStall();
        }
    }

    // Call the power manager initialization entry point
    PM_Init();
    PM_SetSystemPowerState(NULL, POWER_STATE_ON, POWER_FORCE);

    // Initial User Mode Driver Handling.
    InitUserProcMgr();

    // See if we are going to have two boot phases
    hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, _T("SYSTEM/BootPhase1"));
    if (hEvent != NULL) {
        // Load phase 1 drivers from the boot registry
        DevloadInit();

        // Signal boot phase 1 complete
        SetEvent(hEvent);
        CloseHandle(hEvent);

        // Wait for phase 2 of the boot to begin
        hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("SYSTEM/BootPhase2"));
        DEBUGCHK(hEvent);
        if (hEvent) {
            DEBUGMSG(ZONE_BOOTSEQ, (TEXT("DEVICE: Started, waiting for boot phase 2\r\n")));
            WaitForSingleObject(hEvent, INFINITE);
            CloseHandle(hEvent);
        }

        // Load any new drivers from the persistent registry.  Since the 
        // registry may have changed, update the power state for any devices
        // that need it.
        DEBUGMSG(ZONE_BOOTSEQ, (TEXT("DEVICE: Second-phase driver load\r\n")));
        g_BootPhase = 2;
        PM_SetSystemPowerState(NULL, POWER_STATE_ON, POWER_FORCE);
        InitDevices(NULL);
        
        DEBUGMSG(ZONE_BOOTSEQ, (TEXT("DEVICE: Startup sequence complete\r\n")));
        SignalStartedUsingReg(); // SignalStarted call with the right args
    
    } else {
        DEBUGMSG(ZONE_BOOTSEQ, (TEXT("DEVICE: No boot registry - skipping to boot phase 2\n")));
        g_BootPhase = 2;
        DevloadInit();
        SignalStarted(lpCmdLine!=NULL?_wtol(lpCmdLine):20);
    }

    // Boot phase 3 isn't any different from phase 2; just marks that we got here.
    DEBUGMSG(ZONE_BOOTSEQ,
             (TEXT("DEVICE: Finished loading primary drivers - entering boot phase 3\n")));
    g_BootPhase = 3;
    
    CELOG_DeviceFinished ();
    
    while (1) {
        WaitForSingleObject(g_hCleanEvt, INFINITE);

        // check for auto-deregister devs first as they may end up queuing 
        // themselves on the dying devs list
        ProcessAutoDeregisterDevs();
        ProcessDyingDevs();
        ProcessDyingOpens();
    }
    DeiniUserProcMgr();
    return 1;    // should not ever be reached
}

// DLL entry point.
BOOL WINAPI
DllMain(
         HINSTANCE hDllHandle, 
         DWORD  dwReason, 
         LPVOID lpreserved
         ) 
{
    BOOL bRc = TRUE;
    
    UNREFERENCED_PARAMETER(hDllHandle);
    UNREFERENCED_PARAMETER(lpreserved);
    
    switch (dwReason) {
    case DLL_PROCESS_ATTACH: 
        {
            DEBUGREGISTER(hDllHandle);
            DEBUGMSG(ZONE_INIT,(_T("*** DLL_PROCESS_ATTACH - Current Process: 0x%x, ID: 0x%x ***\r\n"),
                GetCurrentProcess(), GetCurrentProcessId()));
            // is device manager already running?
            if(WaitForAPIReady(SH_DEVMGR_APIS,0) == WAIT_OBJECT_0 ) {
                // yes -- we only allow one instance, so fail this entry point
                bRc = FALSE;
            } else {
                DisableThreadLibraryCalls((HMODULE) hDllHandle);
            }
        } 
        break;
        
    case DLL_PROCESS_DETACH: 
        {
            DEBUGMSG(ZONE_INIT,(_T("*** DLL_PROCESS_DETACH - Current Process: 0x%x, ID: 0x%x ***\r\n"),
                GetCurrentProcess(), GetCurrentProcessId()));
        } 
        break;
        
    default:
        break;
    }
    
    return bRc;
}

