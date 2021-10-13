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

#define _DEVMGRDATA_DEF

#include <windows.h>
#include <devload.h>
#include <console.h>
#include <errorrep.h>
#include <extfile.h>

#include "reflector.h"
#include "devmgrp.h"
#include "celogdev.h"
#include "devmgrif.h"
#include "pmif.h"
#include "iormif.h"
#include "devzones.h"
#include "fiomgr.h"

#ifndef SHIP_BUILD
//
// These defines must match the ZONE_* defines in devzones.h
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
#define DBG_BOOTSEQ     1024
#define DBG_PNP         2048
#define DBG_SERVICES    4096
#define DBG_IO          0x2000
#define DBG_FILE        0x4000

DBGPARAM dpCurSettings = {
    TEXT("DEVLOAD"), {
    TEXT("Errors"),TEXT("Warnings"),TEXT("Functions"),TEXT("Initialization"),
    TEXT("Built-In Devices"),TEXT("Undefined"),TEXT("Active Devices"),TEXT("Resource Manager"),
    TEXT("File System Drvr"),TEXT("Dying Devs"),TEXT("Boot Sequence"),TEXT("PnP"),
    TEXT("Services"),TEXT("IO"),TEXT("FILE"),TEXT("Undefined") },
    DBG_ERROR
};
#endif  // SHIP_BUILD

// global variables
CRITICAL_SECTION g_devcs;
LIST_ENTRY g_DevChain;
LIST_ENTRY g_DevCandidateChain;
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
    (PFNVOID)DM_CreateAsyncIoHandle,         // 38
    (PFNVOID)DM_SetIoProgress,              //39
    (PFNVOID)DM_CompleteAsyncIo,            //40
    (PFNVOID)DM_EnableDevice,               //41
    (PFNVOID)DM_DisableDevice,              //42
    (PFNVOID)DM_GetDeviceDriverStatus       //43
//xref ApiSetEnd
};

const PFNVOID ExFDevApiMethods[] = {
//xref ApiSetStart        
    (PFNVOID)DM_Unsupported,                // 0
    (PFNVOID)0,                             // 1
    (PFNVOID)DM_Unsupported,                // 2
    (PFNVOID)DM_Unsupported,                // 3
    (PFNVOID)0,                             // 4 - was CloseAllDeviceHandles
    (PFNVOID)0,                             // 5
    (PFNVOID)0,                             // 6
    (PFNVOID)0,                             // 7
    (PFNVOID)EX_DM_DeactivateDevice,        // 8
    (PFNVOID)0,                             // 9
    (PFNVOID)DM_GetDeviceByIndex,           // 10
    (PFNVOID)DM_CeResyncFilesys,            // 11
    (PFNVOID)EX_DM_ActivateDeviceEx,        // 12
    (PFNVOID)0,                             // 13 - moved to filesys - was RequestDeviceNotifications
    (PFNVOID)0,                             // 14 - moved to filesys - was StopDeviceNotifications
    (PFNVOID)0,                             // 15 - was GetDevicePathFromPnp
    (PFNVOID)IORM_ResourceCreateList,       // 16
    (PFNVOID)IORM_ResourceAdjust,           // 17
    (PFNVOID)EX_PM_GetSystemPowerState,     // 18
    (PFNVOID)EX_PM_SetSystemPowerState,     // 19
    (PFNVOID)EX_PM_SetPowerRequirement,     // 20
    (PFNVOID)EX_PM_ReleasePowerRequirement, // 21
    (PFNVOID)EX_PM_RequestPowerNotifications,// 22
    (PFNVOID)PM_StopPowerNotifications,     // 23
    (PFNVOID)0,                             // 24 - deprecated - VetoPowerNotification
    (PFNVOID)EX_PM_DevicePowerNotify,       // 25
    (PFNVOID)EX_PM_RegisterPowerRelationship,// 26
    (PFNVOID)PM_ReleasePowerRelationship,   // 27
    (PFNVOID)EX_PM_SetDevicePower,          // 28
    (PFNVOID)EX_PM_GetDevicePower,          // 29
    (PFNVOID)0,                             // 30 - moved to filesys - was AdvertiseInterface
    (PFNVOID)IORM_ResourceMarkAsShareable,  // 31
    (PFNVOID)IORM_ResourceDestroyList,      // 32
    (PFNVOID)IORM_ResourceRequestEx,        // 33
    (PFNVOID)EX_DM_GetDeviceInformationByDeviceHandle,  // 34
    (PFNVOID)EX_DM_EnumDeviceInterfaces,    // 35
    (PFNVOID)DM_REL_UDriverProcIoControl,   // 36
    (PFNVOID)EX_DM_EnumServices,            // 37 - EnumServices
    (PFNVOID)0,                             //    (PFNVOID)DM_CreateAsyncIoHandle       Internal Only
    (PFNVOID)EX_DM_SetIoProgress,           //39
    (PFNVOID)EX_DM_CompleteAsyncIo,         //40
    (PFNVOID)EX_DM_EnableDevice,            //41
    (PFNVOID)EX_DM_DisableDevice,           //42
    (PFNVOID)DM_GetDeviceDriverStatus       //43
//xref ApiSetEnd
};

#define NUM_FDEV_APIS (sizeof(FDevApiMethods)/sizeof(FDevApiMethods[0]))

const ULONGLONG FDevApiSigs[NUM_FDEV_APIS] = {
    FNSIG3(DW, DW, DW),       // ProcNotify
    FNSIG0(),
    FNSIG4(I_WSTR, DW, I_WSTR, DW), // RegisterDevice
    FNSIG1(DW),               // DeregisterDevice
    FNSIG0(),                 // CloseAllDeviceHandles - deleted; was unused and a security risk
    FNSIG0(),                 // Obsolete..CreateDeviceHandle
    FNSIG0(),                 // unsupported and moved to thunk - was LoadFSD
    FNSIG0(),                 // ActivateDevice - obsolete; translated in coredll
    FNSIG1(DW),               // DeactivateDevice
    FNSIG0(),                 // unsupported and moved to thunk - was LoadFSDEx
    FNSIG2(DW, IO_PDW),       // GetDeviceByIndex
    FNSIG1(DW),               // CeResyncFilesys
    // ActivateDeviceEx, 2nd parameter. Use I_PDW for a struct pointer. Marshalling is implemented by the API. 
    FNSIG4(I_WSTR, I_PDW, DW, DW),// ActivateDeviceEx, Marshel lpReg in function.
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
    FNSIG3(DW, O_PDW, O_PDW), // DM_CreateAsyncIoHandle
    FNSIG2(DW,DW),            // DM_SetIoProgress,              //39
    FNSIG3(DW,DW,DW),         // DM_CompleteAsyncIo             //40
    FNSIG2(DW,DW),            // DM_EnableDevice
    FNSIG2(DW,DW),            // DM_DisableDevice
    FNSIG2(DW,O_PDW)          // DM_GetDeviceDriverStatus
    
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
    (PFNVOID)DM_Unsupported, // ReadFileWithSeek
    (PFNVOID)DM_Unsupported, // WriteFileWithSeek
    (PFNVOID)DM_Unsupported, // LockFileEx
    (PFNVOID)DM_Unsupported, // UnlockFileEx
    (PFNVOID)DM_DevCancelIo, // CancelIo
    (PFNVOID)DM_DevCancelIoEx, // CancelIoEx 
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
    (PFNVOID)DM_Unsupported, // ReadFileWithSeek
    (PFNVOID)DM_Unsupported, // WriteFileWithSeek
    (PFNVOID)DM_Unsupported, // LockFileEx
    (PFNVOID)DM_Unsupported, // UnlockFileEx
    (PFNVOID)DM_DevCancelIo, // CancelIo
    (PFNVOID)DM_DevCancelIoEx, // CancelIoEx 
};

C_ASSERT(NUM_FILE_APIS == (sizeof(DevFileApiMethods)/sizeof(DevFileApiMethods[0])));

const ULONGLONG DevFileApiSigs[NUM_FILE_APIS] = {
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
    FNSIG0(),                           // ReadFileWithSeek
    FNSIG0(),                           // WriteFileWithSeek
    FNSIG0(),                           // LockFileEx
    FNSIG0(),                           // UnlockFileEx
    FNSIG1(DW),                         // CancelIo
    FNSIG3(DW,I_PTR,DW),                // CancelIoEx
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

// Uses registry to call SignalStarted with the correct args.  Note that ideally
// device manager should NOT know this info is in the registry!
static void SignalStartedUsingReg()
{
#define MAX_APPSTART_KEYNAME 128
    HKEY   hKey;
    WCHAR  szName[MAX_APPSTART_KEYNAME];
    WCHAR  szVal[MAX_APPSTART_KEYNAME];
    LPCWSTR lpszArg = NULL;
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
        DEBUGMSG(ZONE_BOOTSEQ, (TEXT("Device manager must be listed in HKLM\\init!\r\n")));
        DEBUGCHK(0);
    }
}

PVOID   CreateBackgroundIST();
VOID    DeleteBackgroundIST(PVOID);

// This routine initializes the Device Manager, Power Manager, and I/O Resource
// Manager APIs, launches device drivers, and processes device driver load/unload.
int WINAPI 
StartDeviceManager(HINSTANCE hInst, HINSTANCE hPrevInst, LPCWSTR lpCmdLine, int nCmShow)
{

    HINSTANCE hCeddkDll;
    HANDLE hEvent;
    PVOID   pBkThread;

    UNREFERENCED_PARAMETER(nCmShow);
    UNREFERENCED_PARAMETER(hPrevInst);
    UNREFERENCED_PARAMETER(hInst);

    if (WaitForAPIReady(SH_DEVMGR_APIS,0)==WAIT_OBJECT_0) {
        return 0;
    }
    
    DEBUGMSG(ZONE_BOOTSEQ, (TEXT("DEVICE: Starting boot phase 1\n")));

    // PHASE 1
    g_BootPhase = 1;
    InitOOMSettings();
    InitializeListHead(&g_DevChain);
    InitializeListHead(&g_DevCandidateChain);
    g_hCleanEvt = CreateEvent(NULL, FALSE, FALSE, NULL);    
#pragma prefast(suppress: 35002, "Param 1 is a pointer, but expecting a DW")
    g_hDevApiHandle = CreateAPISet("WFLD", NUM_FDEV_APIS, ExFDevApiMethods, FDevApiSigs);
    g_hDevFileApiHandle = CreateAPISet("W32D", NUM_FILE_APIS, ExDevFileApiMethods, DevFileApiSigs);
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

    // Initial Async IO
    CreateIoPacketHandle();
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
    
    pBkThread = CreateBackgroundIST();
        
    CELOG_DeviceFinished ();
    
    WaitForSingleObject(g_hCleanEvt, INFINITE);
    ASSERT(FALSE);
    
    if (pBkThread) {
        DeleteBackgroundIST(pBkThread);
    }
    
    DeiniUserProcMgr();
    DeleteIoPacketHandle();
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

