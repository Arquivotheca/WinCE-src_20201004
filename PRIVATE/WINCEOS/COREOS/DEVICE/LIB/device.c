//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* File:    device.c
 *
 * Purpose: WinCE device manager
 *
 */

//
// This module contains these functions:
//  FS_RegisterDevice
//  RegisterDeviceEx
//  DoFreeFSD
//  DoFreeDevice
//  WaitForFSDs
//  NotifyFSDs
//  IsValidDevice
//  FS_DeregisterDevice
//  FS_ActivateDevice
//  FS_ActivateDeviceEx
//  FindActiveByHandle
//  FS_DeactivateDevice
//  GetFSD
//  FormatDeviceName
//  LoadFSDThread
//  FS_LoadFSD
//  FS_LoadFSDEx
//  FS_CeResyncFilesys
//  FS_PowerAllDevices
//  FS_GetDeviceByIndex
//  FS_CreateDeviceHandle
//  FS_DevCloseFileHandle
//  FS_DevReadFile
//  FS_DevWriteFile
//  FS_DevSetFilePointer
//  FS_DevDeviceIoControl
//  FS_DevGetFileSize
//  FS_DevGetFileInformation
//  FS_DevFlushFileBuffers
//  FS_DevGetFileTime
//  FS_DevSetFileTime
//  FS_DevSetEndOfFile
//  FS_DevProcNotify
//  FS_CloseAllDeviceHandles
//  WinMain
//

#include <device.h>
#include <devload.h>
#include <devloadp.h>
#include <pwrmgr.h>
#include <console.h>

#ifdef TARGET_NT
#include <devemul.h>
#include "proxy.h"
#endif

#ifdef INSTRUM_DEV
#include <instrumd.h>
#else
#define ENTER_INSTRUM
#define EXIT_INSTRUM_INIT
#define EXIT_INSTRUM_DEINIT
#define EXIT_INSTRUM_POWERDOWN
#define EXIT_INSTRUM_POWERUP
#define EXIT_INSTRUM_OPEN
#define EXIT_INSTRUM_CLOSE
#define EXIT_INSTRUM_READ
#define EXIT_INSTRUM_WRITE
#define EXIT_INSTRUM_SEEK
#define EXIT_INSTRUM_IOCONTROL
#endif // INSTRUM_DEV

#include "celogdev.h"

/* Device driver implementation */

//      @doc    EXTERNAL OSDEVICE KERNEL

//      @module device.c - device driver support | Device driver support functions.

//      @topic WinCE Device Drivers | There are two types of device drivers.  The
//      first type are specialized drivers.  These are keyboard, mouse/touch screen,
//      and display drivers.  These drivers have specialized interfaces.  The other
//      type of driver is the stream based device driver model.  Drivers following the
//      stream based model will need to export several functions and be initialized
//      before they can be used.  They can be accessed through standard file APIs such
//      as OpenFile.  The driver dll must export a set of entrypoints of the form
//      ttt_api, where ttt is the device id passed in lpszType to RegisterDevice, and
//      api is a required or optional API.  The APIs required are: Init, Deinit, Open,
//      Close, Read, Write, Seek, and IOCtl.
//  Drivers which need interrupts can use kernel exported interrupt support 
//  routines. To understand the interrupt model and functions please look at
//  <l Interrupt Support.Kernel Interrupt Support>
//      @xref <f RegisterDevice> <f DeregisterDevice> 
//         <l Interrupt Support.Kernel Interrupt Support>

//  notes on handling of unloading of dlls while threads are in those dlls:
//  1> routines which take the device critical section before calling into a dll do not need to
//      adjust the reference count since the deregister call takes the critical section
//  2> routines which do not take the device critical section do the following:
//      a> check if the device is still valid [ if (fsodev->lpDev) ]
//      b> if so, increment the reference count, so dodec = 1, make call, decrement the reference count,
//          set dodec = 0
//      c> we assume we will not fault in the interlockedDecrement, since once we've succeeded in the
//          increment, we know we won't fault on the decrement since nobody will be LocalFree'ing the device
//      d> if we fault inside the actual function call, we'll do the decrement due to the dodec flag
//  3> We signal an event when we put someone on the DyingDevs list.  We can just poll from that point onwards
//      since (1) it's a rare case and (2) it's cheaper than always doing a SetEvent if you decrement the
//      reference count to 0.

CRITICAL_SECTION g_devcs;
LIST_ENTRY g_DevChain;
LIST_ENTRY g_DyingDevs;
LIST_ENTRY g_CandidateDevs;
fsopendev_t *g_lpOpenDevs;
fsopendev_t *g_lpDyingOpens;
HANDLE g_hDevApiHandle, g_hDevFileApiHandle, g_hCleanEvt, g_hCleanDoneEvt;

// Boot phase measure where we are in the boot process.
// Zero means we haven't started the device manager yet;
// One is used to find the registry;
// Two is used during initial device driver loading;
// Three is normal run mode.
DWORD g_BootPhase = 0;

//
// Devload.c
//
extern void InitDevices(void);
extern void DevloadInit(void);
extern void DevloadPostInit(void);
extern BOOL I_DeactivateDevice(HANDLE hDevice, LPWSTR ActivePath);
extern HANDLE StartOneDriver(LPCTSTR RegPath, DWORD LoadOrder, LPCREGINI Registry, DWORD cRegEntries, LPVOID lpvParam);
extern BOOL StartDeviceNotifyThread(void);

#ifndef TARGET_NT
const PFNVOID FDevApiMethods[] = {
//xref ApiSetStart        
    (PFNVOID)FS_DevProcNotify,				// 0
    (PFNVOID)0,								// 1
    (PFNVOID)FS_RegisterDevice,				// 2
    (PFNVOID)FS_DeregisterDevice,			// 3
    (PFNVOID)FS_CloseAllDeviceHandles,		// 4
    (PFNVOID)FS_CreateDeviceHandle,			// 5
    (PFNVOID)FS_LoadFSD,					// 6
    (PFNVOID)0,								// 7
    (PFNVOID)FS_DeactivateDevice,			// 8
    (PFNVOID)FS_LoadFSDEx,					// 9
    (PFNVOID)FS_GetDeviceByIndex,			// 10
    (PFNVOID)FS_CeResyncFilesys,			// 11
    (PFNVOID)FS_ActivateDeviceEx,			// 12
    (PFNVOID)FS_RequestDeviceNotifications,	// 13
    (PFNVOID)FS_StopDeviceNotifications,	// 14
    (PFNVOID)FS_GetDevicePathFromPnp,		// 15
    (PFNVOID)FS_ResourceCreateList,			// 16
    (PFNVOID)FS_ResourceAdjust,				// 17
    (PFNVOID)PM_GetSystemPowerState,		// 18
    (PFNVOID)PM_SetSystemPowerState,		// 19
    (PFNVOID)PM_SetPowerRequirement,		// 20
    (PFNVOID)PM_ReleasePowerRequirement,	// 21
    (PFNVOID)PM_RequestPowerNotifications,	// 22
    (PFNVOID)PM_StopPowerNotifications,		// 23
    (PFNVOID)0,		                                    // 24 - deprecated - VetoPowerNotification
    (PFNVOID)PM_DevicePowerNotify,			// 25
    (PFNVOID)PM_RegisterPowerRelationship,	// 26
    (PFNVOID)PM_ReleasePowerRelationship,	// 27
    (PFNVOID)PM_SetDevicePower,				// 28
    (PFNVOID)PM_GetDevicePower,				// 29
    (PFNVOID)FS_AdvertiseInterface,	            // 30
//xref ApiSetEnd
};


#define NUM_FDEV_APIS (sizeof(FDevApiMethods)/sizeof(FDevApiMethods[0]))

const DWORD FDevApiSigs[NUM_FDEV_APIS] = {
    FNSIG3(DW, DW, DW),       // ProcNotify
    FNSIG0(),
    FNSIG4(PTR, DW, PTR, DW), // RegisterDevice
    FNSIG1(DW),               // DeregisterDevice
    FNSIG1(DW),               // CloseAllDeviceHandles
    FNSIG4(PTR, DW, DW, DW),  // CreateDeviceHandle
    FNSIG2(PTR, PTR),         // LoadFSD
    FNSIG0(),                 // ActivateDevice - obsolete; translated in coredll
    FNSIG1(DW),               // DeactivateDevice
    FNSIG3(PTR, PTR, DW),     // LoadFSDEx
    FNSIG2(DW, PTR),          // GetDeviceByIndex
    FNSIG1(DW),               // CeResyncFilesys
    FNSIG4(PTR, PTR, DW, PTR),// ActivateDeviceEx
    FNSIG3(PTR, DW, DW),      // RequestDeviceNotifications
    FNSIG1(DW),               // StopDeviceNotifications
    FNSIG3(DW, PTR, DW),      // GetDevicePathFromPnp
    FNSIG3(DW, DW, DW),	      // ResourceCreateList
    FNSIG4(DW, DW, DW, DW),   // ResourceAdjust
    FNSIG3(PTR, DW, PTR),           // GetSystemPowerState
    FNSIG3(PTR, DW, DW),            // SetSystemPowerState
    FNSIG5(PTR, DW, DW, PTR, DW),   // SetPowerRequirement
    FNSIG1(DW),                     // ReleasePowerRequirement
    FNSIG2(DW, DW),                 // RequestPowerNotifications
    FNSIG1(DW),                     // StopPowerNotifications
    FNSIG0(),                     // deprecated - VetoPowerNotification
    FNSIG3(PTR, DW, DW),            // DevicePowerNotify
    FNSIG4(PTR, PTR, PTR, DW),      // RegisterPowerRelationship
    FNSIG1(DW),                     // ReleasePowerRelationship
    FNSIG3(PTR, DW, DW),           // SetDevicePower
    FNSIG3(PTR, DW, PTR),           // GetDevicePower   
    FNSIG3(PTR, PTR, DW),      // AdvertiseInterface
};

const PFNVOID DevFileApiMethods[] = {
    (PFNVOID)FS_DevCloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)FS_DevReadFile,
    (PFNVOID)FS_DevWriteFile,
    (PFNVOID)FS_DevGetFileSize,
    (PFNVOID)FS_DevSetFilePointer,
    (PFNVOID)FS_DevGetFileInformationByHandle,
    (PFNVOID)FS_DevFlushFileBuffers,
    (PFNVOID)FS_DevGetFileTime,
    (PFNVOID)FS_DevSetFileTime,
    (PFNVOID)FS_DevSetEndOfFile,
    (PFNVOID)FS_DevDeviceIoControl,
};

#define NUM_FAPIS (sizeof(DevFileApiMethods)/sizeof(DevFileApiMethods[0]))

const DWORD DevFileApiSigs[NUM_FAPIS] = {
    FNSIG1(DW),                 // CloseFileHandle
    FNSIG0(),
    FNSIG5(DW,PTR,DW,PTR,PTR),  // ReadFile
    FNSIG5(DW,PTR,DW,PTR,PTR),  // WriteFile
    FNSIG2(DW,PTR),             // GetFileSize
    FNSIG4(DW,DW,PTR,DW),       // SetFilePointer
    FNSIG2(DW,PTR),             // GetFileInformationByHandle
    FNSIG1(DW),                 // FlushFileBuffers
    FNSIG4(DW,PTR,PTR,PTR),     // GetFileTime
    FNSIG4(DW,PTR,PTR,PTR),     // SetFileTime
    FNSIG1(DW),                 // SetEndOfFile,
    FNSIG8(DW, DW, PTR, DW, PTR, DW, PTR, PTR), // DeviceIoControl
};
#endif // TARGET_NT

BOOL FS_AdvertiseInterface(LPCGUID devclass, LPCWSTR lpszName, BOOL fAdd)
{
    BOOL fOk = FALSE;
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        SetLastError(ERROR_ACCESS_DENIED);
    } else {
        fOk = UpdateAdvertisedInterface(devclass, lpszName, fAdd);
    }
    return fOk;
}

HANDLE FS_RequestDeviceNotifications (LPCGUID devclass, HANDLE hMsgQ, BOOL fAll)
{
    HANDLE h = RegisterDeviceNotifications(devclass, hMsgQ);

    if (h && fAll) {
        fsdev_t *lpTrav;

        // send notifications for all device interfaces
        EnterCriticalSection(&g_devcs);

        for (lpTrav = (fsdev_t *)g_DevChain.Flink;
             lpTrav != (fsdev_t *)&g_DevChain;
             lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
            PnpSendTo(h, lpTrav->dwId);
        }

        LeaveCriticalSection(&g_devcs);

        // also send notifications for interfaces set up with AdvertiseInterface()
        AdvertisementSendTo(h);
    }

    return h;
}

BOOL FS_StopDeviceNotifications (HANDLE h)
{
    if (!h || (h == INVALID_HANDLE_VALUE)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    UnregisterDeviceNotifications(h);
    return TRUE;
}

BOOL FS_GetDevicePathFromPnp (DWORD k, LPTSTR path, DWORD pathlen)
{
    TCHAR activekey[REG_PATH_LEN];
    wsprintf(activekey, TEXT("%s\\%02d"), s_ActiveKey, k);
    // Check caller buffer access
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        path = MapCallerPtr((LPVOID)path, pathlen*sizeof(TCHAR));
    }
    return
        RegQueryValueEx(HKEY_LOCAL_MACHINE, DEVLOAD_DEVKEY_VALNAME, (LPDWORD)activekey,
                        NULL, (LPBYTE)path, &pathlen) == 0;
}

static PFNVOID FS_GetProcAddr(LPCWSTR type, LPCWSTR lpszName, HINSTANCE hInst) {
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

static BOOL FS_NotSupportedBool (void)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

static DWORD FS_NotSupportedDword (void)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return -1;
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
FS_RegisterDevice(
    LPCWSTR lpszType,
    DWORD   dwIndex,
    LPCWSTR lpszLib,
    DWORD   dwInfo
    )
{
    return RegisterDeviceEx(
                lpszType,
                dwIndex,
                lpszLib,
                dwInfo,
                MAX_LOAD_ORDER+1,
                DEVFLAGS_NONE,
                0,
                NULL
                );
}

HANDLE
RegisterDeviceEx(
    LPCWSTR lpszType,
    DWORD dwIndex,
    LPCWSTR lpszLib,
    DWORD dwInfo,
    DWORD dwLoadOrder,
    DWORD dwFlags,
    DWORD dwId,
    LPVOID lpvParam
    )
{
    HANDLE hDev = 0;
    fsdev_t *lpdev = 0, *lpTrav;
    DWORD retval = 0;

    DEBUGMSG(ZONE_DYING, (TEXT("DEVICE: About to wait on CleanDoneEvt.\r\n")));

    // Need to wait for any filesystem devices to finish getting deregistered 
    // before we go ahead and register them again. This gets around problems
    // with storage card naming. 
    retval = WaitForSingleObject(g_hCleanDoneEvt, 5000);
    DEBUGMSG(ZONE_DYING, (TEXT("DEVICE: Got CleanDoneEvt.\r\n")));
    
    ASSERT(retval != WAIT_TIMEOUT);
    
    retval = ERROR_SUCCESS;    // Initialize for success

    if (dwIndex > 9) {
        retval = ERROR_INVALID_PARAMETER;
        goto errret;
    }
    if (!(lpdev = LocalAlloc(LPTR,sizeof(fsdev_t)))) {
        retval = ERROR_OUTOFMEMORY;
        goto errret;
    }
    
    __try {
        memset(lpdev, 0, sizeof(fsdev_t));
        if (lpszType != NULL)
            memcpy(lpdev->type,lpszType,sizeof(lpdev->type));
        lpdev->index = dwIndex;
        lpdev->hLib =
            (dwFlags & DEVFLAGS_LOADLIBRARY) ? LoadLibrary(lpszLib) : LoadDriver(lpszLib);
        if (!lpdev->hLib) {
            retval = ERROR_FILE_NOT_FOUND;
        } else {
            LPCWSTR pEffType = (dwFlags & DEVFLAGS_NAKEDENTRIES) ? NULL : lpdev->type;
            
            lpdev->fnInit = (pInitFn)FS_GetProcAddr(pEffType,L"Init",lpdev->hLib);
            lpdev->fnDeinit = (pDeinitFn)FS_GetProcAddr(pEffType,L"Deinit",lpdev->hLib);
            lpdev->fnOpen = (pOpenFn)FS_GetProcAddr(pEffType,L"Open",lpdev->hLib);
            lpdev->fnClose = (pCloseFn)FS_GetProcAddr(pEffType,L"Close",lpdev->hLib);
            lpdev->fnRead = (pReadFn)FS_GetProcAddr(pEffType,L"Read",lpdev->hLib);
            lpdev->fnWrite = (pWriteFn)FS_GetProcAddr(pEffType,L"Write",lpdev->hLib);
            lpdev->fnSeek = (pSeekFn)FS_GetProcAddr(pEffType,L"Seek",lpdev->hLib);
            lpdev->fnControl = (pControlFn)FS_GetProcAddr(pEffType,L"IOControl",lpdev->hLib);
            lpdev->fnPowerup = (pPowerupFn)FS_GetProcAddr(pEffType,L"PowerUp",lpdev->hLib);
            lpdev->fnPowerdn = (pPowerupFn)FS_GetProcAddr(pEffType,L"PowerDown",lpdev->hLib);

            if (!(lpdev->fnInit && lpdev->fnDeinit) ||
                lpszType && (!lpdev->fnOpen || !lpdev->fnClose ||
                             (!lpdev->fnRead && !lpdev->fnWrite &&
                              !lpdev->fnSeek && !lpdev->fnControl))) {
                retval = ERROR_INVALID_FUNCTION;
            }

            if (!lpdev->fnOpen) lpdev->fnOpen = (pOpenFn) FS_NotSupportedBool;
            if (!lpdev->fnClose) lpdev->fnClose = (pCloseFn) FS_NotSupportedBool;
            if (!lpdev->fnControl) lpdev->fnControl = (pControlFn) FS_NotSupportedBool;
            if (!lpdev->fnRead) lpdev->fnRead = (pReadFn) FS_NotSupportedDword;
            if (!lpdev->fnWrite) lpdev->fnWrite = (pWriteFn) FS_NotSupportedDword;
            if (!lpdev->fnSeek) lpdev->fnSeek = (pSeekFn) FS_NotSupportedDword;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        retval = ERROR_INVALID_PARAMETER;
    }

    if (retval) {
        goto errret;
    }

    // Non-stream devices can be loaded multiple times
    if (lpdev->type[0] != L'\0') {
        // Now enter the critical section to look for it in the device list
        EnterCriticalSection (&g_devcs);
        __try {
            //
            // check for uniqueness
            //
            for (lpTrav = (fsdev_t *)g_DevChain.Flink;
                 lpTrav != (fsdev_t *)&g_DevChain;
                 lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
                if (lpTrav->type[0] != L'\0') {
                    if (!memcmp(lpTrav->type,lpdev->type,sizeof(lpdev->type)) &&
                        (lpTrav->index == lpdev->index)) {
                        retval = ERROR_DEVICE_IN_USE;
                        break;
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            retval = ERROR_INVALID_PARAMETER;
        }
        LeaveCriticalSection(&g_devcs);
        if (retval) {
            goto errret;
        }
    }

    __try {
        ENTER_INSTRUM {
            lpdev->dwData = lpdev->fnInit(dwInfo,lpvParam);
        } EXIT_INSTRUM_INIT;

        if (!(lpdev->dwData)) {
            retval = ERROR_OPEN_FAILED;
        } else {
            // Sucess
            lpdev->PwrOn = TRUE;
            lpdev->dwLoadOrder = dwLoadOrder;
            lpdev->dwId = dwId;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        retval = ERROR_INVALID_PARAMETER;
    }
    if (retval) {
        goto errret;
    }
    if (dwFlags & DEVFLAGS_UNLOAD) {
        // This will cause us to return NULL while indicating SUCCESS.
        hDev = NULL;
        goto earlyret;
    }

    EnterCriticalSection(&g_devcs);
    __try {
        InsertTailList(&g_DevChain, (PLIST_ENTRY)lpdev);
        hDev = (HANDLE)lpdev;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        retval = ERROR_INVALID_PARAMETER;
    }
    LeaveCriticalSection(&g_devcs);

errret:
    
#ifdef DEBUG
    if (ZONE_ACTIVE) {
        if (hDev != NULL) {
            DEBUGMSG(ZONE_ACTIVE, (TEXT("DEVICE: Name   Load Order\r\n")));
            //
            // Display the list of devices in ascending load order.
            //
            EnterCriticalSection(&g_devcs);
            for (lpTrav = (fsdev_t *)g_DevChain.Flink;
                 lpTrav != (fsdev_t *)&g_DevChain;
                 lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
                if (*lpTrav->type)
                    DEBUGMSG(ZONE_ACTIVE, (TEXT("DEVICE: %c%c%c%d:  %d\r\n"),
                                           lpTrav->type[0], lpTrav->type[1], lpTrav->type[2],
                                           lpTrav->index, lpTrav->dwLoadOrder));
                else
                    DEBUGMSG(ZONE_ACTIVE, (TEXT("DEVICE: <anon %08x> %d:  %d\r\n"),
                                           lpTrav->hLib,
                                           lpTrav->index, lpTrav->dwLoadOrder));
            }
            LeaveCriticalSection(&g_devcs);
        }
    }
#endif

    // If we failed then let's clean up the module and data
    if (retval) {
earlyret:
        SetLastError (retval);
        if (lpdev) {
            if (lpdev->hLib) {
                FreeLibrary(lpdev->hLib);
            }
            LocalFree (lpdev);
        }
    }

    return hDev;
}

void DoFreeDevice(fsdev_t *lpdev)
{
    FreeLibrary(lpdev->hLib);
    LocalFree(lpdev);
}



//
// IsValidDevice - function to check if passed in device handle is actually a
// valid registered device.
//
// Return TRUE if it is a valid registered device, FALSE otherwise.
//
BOOL
IsValidDevice(
    fsdev_t * lpdev
    )
{
    fsdev_t * lpTrav;

    for (lpTrav = (fsdev_t *)g_DevChain.Flink;
         lpTrav != (fsdev_t *)&g_DevChain;
         lpTrav = (fsdev_t *)(lpTrav->list.Flink)) {
        if (lpTrav == lpdev) {
            return TRUE;
        }
    }
    return FALSE;
}


//      @func   BOOL | DeregisterDevice | Deregister a registered device
//  @parm       HANDLE | hDevice | handle to registered device, from RegisterDevice
//      @rdesc  Returns TRUE for success, FALSE for failure
//      @comm   DeregisterDevice can be used if a device is removed from the system or
//                      is being shut down.  An example would be:<nl>
//                      <tab>DeregisterDevice(h1);<nl>
//                      where h1 was returned from a call to RegisterDevice.
//      @xref <f RegisterDevice> <l Overview.WinCE Device Drivers>

BOOL FS_DeregisterDevice(HANDLE hDevice)
{
    fsdev_t *lpdev;
    fsopendev_t *lpopen;    
    BOOL retval = FALSE;

    ENTER_DEVICE_FUNCTION {
        lpdev = (fsdev_t *)hDevice;
        if (!IsValidDevice(lpdev)) {
            goto errret;
        }

        lpopen = g_lpOpenDevs;
        while (lpopen) {
            if (lpopen->lpDev == lpdev)
                lpopen->lpDev = 0;
            lpopen = lpopen->nextptr;
        }
        RemoveEntryList((PLIST_ENTRY)lpdev);

        LeaveCriticalSection(&g_devcs);
        ENTER_INSTRUM {
        lpdev->fnDeinit(lpdev->dwData);
        } EXIT_INSTRUM_DEINIT;
        
        lpdev->wFlags |= DEVFLAGS_DYING;
        if (!lpdev->dwRefCnt) {
            DoFreeDevice(lpdev);
            EnterCriticalSection(&g_devcs);
        } else {
            EnterCriticalSection(&g_devcs);
            InsertTailList(&g_DyingDevs, (PLIST_ENTRY)lpdev);
            DEBUGMSG(ZONE_DYING, (TEXT("DEVICE: About to Reset CleanDoneEvt.\r\n")));

            ResetEvent(g_hCleanDoneEvt);
            SetEvent(g_hCleanEvt);
        }
        retval = TRUE;
errret:
        ;
    } EXIT_DEVICE_FUNCTION;
    return retval;
}


//
//  @func   HANDLE | ActivateDevice | Register a device and add it to the active list under HKEY_LOCAL_MACHINE\Drivers\Active.
//  @parm   LPCWSTR | lpszDevKey | The driver's device key under HKEY_LOCAL_MACHINE\Drivers (for example "Drivers\PCMCIA\Modem")
//  @parm   DWORD | dwClientInfo | Instance information to be stored in the device's active key.
//  @rdesc  Returns a handle to a device, or 0 for failure.  This handle should
//          only be passed to DeactivateDevice
//  @comm   ActivateDevice will RegisterDevice the specified device and
//  create an active key in HKEY_LOCAL_MACHINE\Drivers\Active for the new device.
//  Also the device's creation is announced to the system via a
//  WM_DEVICECHANGE message and through the application notification system.
//
//  An example would be:<nl>
//  <tab>hnd = ActivateDevice(lpszDevKey, dwClientInfo);<nl>
//  where hnd is a HANDLE that can be used for DeactivateDevice<nl>
//  lpszDevKey is the registry path in HKEY_LOCAL_MACHINE to the device
//    driver's device key which has values that specify the driver name and
//    device prefix.<nl>
//  dwClientInfo is a DWORD value that will be stored in the device's active
//    key.<nl>
//  @xref <f DeactivateDevice> <f RegisterDevice> <f DeregisterDevice> <l Overview.WinCE Device Drivers>
//

HANDLE FS_ActivateDeviceEx(
    LPCTSTR lpszDevKey,
    LPCREGINI lpReg,
    DWORD cReg,
    LPVOID lpvParam
    )
{
    DEBUGMSG(1, (TEXT("DEVICE!ActivateDeviceEx(%s) entered\r\n"), lpszDevKey));
    CELOG_ActivateDevice (lpszDevKey);

    // lpReg will be validated inside StartOneDriver
    return StartOneDriver(
               lpszDevKey,
               MAX_LOAD_ORDER,
               lpReg, cReg, lpvParam);
}

#if 0 // This is now translated to ActivateDeviceEx by coredll prior to the thunk.
HANDLE FS_ActivateDevice(
    LPCTSTR lpszDevKey,
    DWORD dwClientInfo
    )
{
    REGINI reg;
    
    DEBUGMSG(1, (TEXT("DEVICE!ActivateDevice(%s, %d) entered\r\n"), lpszDevKey, dwClientInfo));

    // Add this to the registry to maintain compatibility with older API.
    reg.lpszVal = DEVLOAD_CLIENTINFO_VALNAME;
    reg.dwType = DEVLOAD_CLIENTINFO_VALTYPE;
    reg.pData = (LPBYTE) &dwClientInfo;
    reg.dwLen = sizeof(dwClientInfo);

    return StartOneDriver(
               lpszDevKey,
               MAX_LOAD_ORDER,
               &reg, 1);
}
#endif

//
// Function to search through HLM\Drivers\Active for a matching device handle.
// Return TRUE if the device is found. The device's active registry path will be
// copied to the ActivePath parameter.
// 
BOOL
FindActiveByHandle(
    HANDLE hDevice,
    LPTSTR ActivePath
    )
{
    DWORD Handle;
    DWORD RegEnum;
    DWORD status;
    DWORD ValLen;
    DWORD ValType;
    LPTSTR RegPathPtr;
    HKEY DevKey;
    HKEY ActiveKey;
    TCHAR DevNum[DEVNAME_LEN];

    _tcscpy(ActivePath, s_ActiveKey);
    status = RegOpenKeyEx(
                    HKEY_LOCAL_MACHINE,
                    ActivePath,
                    0,
                    0,
                    &ActiveKey);
    if (status != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
            (TEXT("DEVICE!FindActiveByHandle: RegOpenKeyEx(%s) returned %d\r\n"),
            ActivePath, status));
        return FALSE;
    }

    RegEnum = 0;
    while (1) {
        ValLen = sizeof(DevNum)/sizeof(TCHAR);
        status = RegEnumKeyEx(
                    ActiveKey,
                    RegEnum,
                    DevNum,
                    &ValLen,
                    NULL,
                    NULL,
                    NULL,
                    NULL);

        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                (TEXT("DEVICE!FindActiveByHandle: RegEnumKeyEx() returned %d\r\n"),
                status));
            RegCloseKey(ActiveKey);
            return FALSE;
        }

        status = RegOpenKeyEx(
                    ActiveKey,
                    DevNum,
                    0,
                    0,
                    &DevKey);
        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                (TEXT("DEVICE!FindActiveByHandle: RegOpenKeyEx(%s) returned %d\r\n"),
                DevNum, status));
            goto fad_next;
        }

        //
        // See if the handle value matches
        //
        ValLen = sizeof(Handle);
        status = RegQueryValueEx(
                    DevKey,
                    DEVLOAD_HANDLE_VALNAME,
                    NULL,
                    &ValType,
                    (PUCHAR)&Handle,
                    &ValLen);
        if (status != ERROR_SUCCESS) {
            DEBUGMSG(ZONE_ACTIVE|ZONE_ERROR,
                (TEXT("DEVICE!FindActiveByHandle: RegQueryValueEx(%s\\Handle) returned %d\r\n"),
                DevNum, status));
            //
            // We found an active device key with no handle value.
            //
            Handle = 0;
        }
        RegCloseKey(DevKey);

        if (Handle == (DWORD)hDevice) {
            //
            // Format the registry path
            //
            RegPathPtr = ActivePath + _tcslen(ActivePath);
            RegPathPtr[0] = (TCHAR)'\\';
            RegPathPtr++;
            _tcscpy(RegPathPtr, DevNum);
            RegCloseKey(ActiveKey);
            return TRUE;
        }

fad_next:
        RegEnum++;
    }   // while

}   // FindActiveByHandle


//
//  @func   BOOL | DeactivateDevice | Deregister a registered device and remove
//                                    it from the active list.
//  @parm   HANDLE | hDevice | handle to registered device, from ActivateDevice
//  @rdesc  Returns TRUE for success, FALSE for failure
//  @comm   DeactivateDevice will DeregisterDevice the specified device and
//  delete the device's active key from HKEY_LOCAL_MACHINE\Drivers\Active.
//  Also the device's removal is announced to the system via a
//  WM_DEVICECHANGE message and through the application notification system.
//          An example would be:<nl>
//                      <tab>DeactivateDevice(h1);<nl>
//                      where h1 was returned from a call to ActivateDevice.
//      @xref <f ActivateDevice> <f RegisterDevice> <f DeregisterDevice> <l Overview.WinCE Device Drivers>
//

BOOL FS_DeactivateDevice(HANDLE hDevice)
{
    TCHAR ActivePath[REG_PATH_LEN];

    if (FindActiveByHandle(hDevice, ActivePath)) {
        return I_DeactivateDevice(hDevice, ActivePath);
    }
    return FALSE;
}

#define DEVICE_NAME_SIZE 6  // i.e. "COM1:" (includes space for 0 terminator)

void FormatDeviceName(LPWSTR lpszName, fsdev_t * lpdev)
{
    memcpy(lpszName, lpdev->type, sizeof(lpdev->type));
    lpszName[sizeof(lpdev->type)/sizeof(WCHAR)+0] = (WCHAR)(L'0' + lpdev->index);
    lpszName[sizeof(lpdev->type)/sizeof(WCHAR)+1] = L':';
    lpszName[sizeof(lpdev->type)/sizeof(WCHAR)+2] = 0;
}

BOOL FS_LoadFSD(HANDLE hDevice, LPCWSTR lpFSDName)
{
	SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

BOOL FS_LoadFSDEx(HANDLE hDevice, LPCWSTR lpFSDName, DWORD dwFlag)
{
	SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}


//
//  @func   BOOL | CeResyncFilesys | Cause a file system driver to remount a device because of media change.
//  @parm   HANDLE | hDevice | handle to registered device from RegisterDevice
//  @rdesc  Returns TRUE for success, FALSE for failure
//  @comm   CeResyncFilesys is used by a device driver to indicate to its associated file
//                  system driver that a new volume/media has been inserted. The handle can be retrieved from
//                  the device's active key in the registry or from the p_hDevice field of the POST_INIT_BUF
//                  structure passed to the post-initialization IOCTL.
//                  
//
BOOL FS_CeResyncFilesys(HANDLE hDevice)
{
    fsdev_t *lpdev;
    WCHAR wsDev[DEVICE_NAME_SIZE];

    lpdev = (fsdev_t *)hDevice;
    if (!IsValidDevice(lpdev)) {
        DEBUGMSG(ZONE_ERROR,(L"DEVICE: CeResyncFilesys - invalid handle\n"));
        return FALSE;   // Invalid device handle
    }

    if ((GetFileAttributesW( L"\\StoreMgr") != -1) &&
        (GetFileAttributesW( L"\\StoreMgr") & FILE_ATTRIBUTE_TEMPORARY)) {
            WCHAR szFileName[MAX_PATH];
            FormatDeviceName(wsDev, lpdev);
            wcscpy( szFileName, L"\\StoreMgr\\");
            wcscat( szFileName, wsDev);
            return(MoveFile( szFileName, szFileName));
    }    
    
    return FALSE;
}


void FS_PowerAllDevices(BOOL bOff) {
    fsdev_t *lpdev;

    if (bOff) {
        //
        // Notify drivers of power off in reverse order from their LoadOrder
        //
        for (lpdev = (fsdev_t *)g_DevChain.Blink;
             lpdev != (fsdev_t *)&g_DevChain;
             lpdev = (fsdev_t *)lpdev->list.Blink) {
            if (lpdev->PwrOn) {
                if (lpdev->fnPowerdn) {
#ifdef DEBUG
                    if (ZONE_PNP) {
                        WCHAR strDeviceName[sizeof(lpdev->type)/sizeof(WCHAR)+4];
                        FormatDeviceName(strDeviceName,lpdev);
                        DEBUGMSG(ZONE_PNP,(TEXT("Calling \'%s\' PowerDown at 0x%x\r\n"),strDeviceName,lpdev->fnPowerdn));
                    }
#endif
                    ENTER_INSTRUM {
                    lpdev->fnPowerdn(lpdev->dwData);
                    } EXIT_INSTRUM_POWERDOWN;
                }
                lpdev->PwrOn = FALSE;
            }
        }
        DEBUGMSG(ZONE_PNP,(TEXT("Calling PmPowerHandler Off \r\n")));
        PmPowerHandler(bOff);
    } else {
        //
        // Notify drivers of power on according to their LoadOrder
        //
        DEBUGMSG(ZONE_PNP,(TEXT("Calling PmPowerHandler On \r\n")));
        PmPowerHandler(bOff);
        for (lpdev = (fsdev_t *)g_DevChain.Flink;
             lpdev != (fsdev_t *)&g_DevChain;
             lpdev = (fsdev_t *)lpdev->list.Flink) {
            if (!lpdev->PwrOn) {
                if (lpdev->fnPowerup) {
#ifdef DEBUG
                    if (ZONE_PNP) {
                        WCHAR strDeviceName[sizeof(lpdev->type)/sizeof(WCHAR)+4];
                        FormatDeviceName(strDeviceName,lpdev);
                        DEBUGMSG(ZONE_PNP,(TEXT("Calling \'%s\' PowerUp at 0x%x\r\n"),strDeviceName,lpdev->fnPowerup));
                    }
#endif
                    ENTER_INSTRUM {
                    lpdev->fnPowerup(lpdev->dwData);
                    } EXIT_INSTRUM_POWERUP;
                }
                lpdev->PwrOn = TRUE;
            }
        }
    }
}

BOOL FS_GetDeviceByIndex(DWORD dwIndex, LPWIN32_FIND_DATA lpFindFileData) {
    fsdev_t *lpdev;
    BOOL bRet = FALSE;
    // Check caller buffer access
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        lpFindFileData = MapCallerPtr((LPVOID)lpFindFileData, sizeof(WIN32_FIND_DATA));
    }

    ENTER_DEVICE_FUNCTION {
        lpdev = (fsdev_t *)g_DevChain.Flink;
        while (dwIndex && lpdev != (fsdev_t *)&g_DevChain) {
            dwIndex--;
            lpdev = (fsdev_t *)lpdev->list.Flink;
        }
        if (lpdev != (fsdev_t *)&g_DevChain) {
            lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            *(__int64 *)&lpFindFileData->ftCreationTime = 0;
            *(__int64 *)&lpFindFileData->ftLastAccessTime = 0;
            *(__int64 *)&lpFindFileData->ftLastWriteTime = 0;
            lpFindFileData->nFileSizeHigh = 0;
            lpFindFileData->nFileSizeLow = 0;
            lpFindFileData->dwOID = 0xffffffff;
            FormatDeviceName(lpFindFileData->cFileName, lpdev);
            bRet = TRUE;
        }
    } EXIT_DEVICE_FUNCTION;
    return bRet;
}

// assumes len == 5 and lpnew[4] == ':'
HANDLE FS_CreateDeviceHandle(LPCWSTR lpNew, DWORD dwAccess, DWORD dwShareMode, HPROCESS hProc)
{
    HANDLE hDev = INVALID_HANDLE_VALUE;
    DWORD dwErrCode = ERROR_DEV_NOT_EXIST;
    fsopendev_t *lpopendev;
    fsdev_t *lpdev;
    
    ENTER_DEVICE_FUNCTION {
        for (lpdev = (fsdev_t *)g_DevChain.Flink;
             lpdev != (fsdev_t *)&g_DevChain;
             lpdev = (fsdev_t *)lpdev->list.Flink) {
            if (!_wcsnicmp(lpNew,lpdev->type,
                     sizeof(lpdev->type)/sizeof(WCHAR))) {
                if ((DWORD)(lpNew[3]-L'0') == lpdev->index) {
                    if (!(lpopendev = LocalAlloc(LPTR,sizeof(fsopendev_t)))) {
                        dwErrCode = ERROR_OUTOFMEMORY;
                        goto errret;
                    }
                    lpopendev->lpDev = lpdev;
                    lpopendev->lpdwDevRefCnt = &lpdev->dwRefCnt;
                    LeaveCriticalSection(&g_devcs);
                    ENTER_INSTRUM {
                    lpopendev->dwOpenData = lpdev->fnOpen(lpdev->dwData,dwAccess,dwShareMode);
                    } EXIT_INSTRUM_OPEN;
                    EnterCriticalSection(&g_devcs);
                    if ((!IsValidDevice(lpdev)) || (!lpopendev->dwOpenData)) {
                        LocalFree(lpopendev);
                        // Don't set an error code, the driver should have done that.
                        goto errret;
                    }
                    lpopendev->hProc = hProc;
                    if (!(hDev = CreateAPIHandle(g_hDevFileApiHandle, lpopendev))) {
                        hDev = INVALID_HANDLE_VALUE;
                        dwErrCode = ERROR_OUTOFMEMORY;
                        LocalFree(lpopendev);
                        goto errret;
                    }
                    // OK, we managed to create the handle
                    dwErrCode = 0;
                    lpopendev->KHandle = hDev;
                    lpopendev->nextptr = g_lpOpenDevs;
                    g_lpOpenDevs = lpopendev;
                    break;
                }
            }
        }
errret:
        if( dwErrCode ) {
            SetLastError( dwErrCode );
            // This debugmsg occurs too frequently on a system without a sound device (eg, CEPC)
            DEBUGMSG(0, (TEXT("ERROR (device.c:FS_CreateDeviceHandle): %s, %d\r\n"), lpNew, dwErrCode));
        }
        
    } EXIT_DEVICE_FUNCTION;
    return hDev;
}

BOOL FS_DevCloseFileHandle(fsopendev_t *fsodev)
{
    BOOL retval = FALSE;
    fsopendev_t *fsTrav;
    fsdev_t *lpdev;

    ENTER_DEVICE_FUNCTION {
        if (g_lpOpenDevs == fsodev)
            g_lpOpenDevs = fsodev->nextptr;
        else {
            fsTrav = g_lpOpenDevs;
            while (fsTrav != NULL && fsTrav->nextptr != fsodev) {
                fsTrav = fsTrav->nextptr;
            }
            if (fsTrav != NULL) {
                fsTrav->nextptr = fsodev->nextptr;
            } else {
                DEBUGMSG(1, (TEXT("WARNING (device.c): fsodev (0x%X) not in DEVICE.EXE list\r\n"), fsodev));
            }
        }
        if (lpdev = fsodev->lpDev) {
            LeaveCriticalSection(&g_devcs);
            ENTER_INSTRUM {
            lpdev->fnClose(fsodev->dwOpenData);
            } EXIT_INSTRUM_CLOSE;
            EnterCriticalSection(&g_devcs);
        }
        if (fsodev->dwOpenRefCnt) {
            fsodev->nextptr = g_lpDyingOpens;
            g_lpDyingOpens = fsodev;
            SetEvent(g_hCleanEvt);
        } else {
            LocalFree(fsodev);
        }
        retval = TRUE;
    } EXIT_DEVICE_FUNCTION;
    return retval;
}

void OpenAddRef(fsopendev_t *fsodev)
{
    InterlockedIncrement(&fsodev->dwOpenRefCnt);
    InterlockedIncrement(fsodev->lpdwDevRefCnt);
}

void OpenDelRef(fsopendev_t *fsodev)
{
    InterlockedDecrement(&fsodev->dwOpenRefCnt);
    InterlockedDecrement(fsodev->lpdwDevRefCnt);
}

BOOL FS_DevReadFile(fsopendev_t *fsodev, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,
    LPOVERLAPPED lpOverlapped)
{
    BOOL retval = FALSE;
    DWORD dodec = 0;
    // Check caller buffer access
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        buffer = MapCallerPtr(buffer, nBytesToRead);
        lpNumBytesRead = MapCallerPtr(lpNumBytesRead, sizeof(DWORD));
    }
    __try {
        if (!fsodev->lpDev)
            SetLastError(ERROR_INVALID_HANDLE_STATE);
        else {
            OpenAddRef(fsodev);
            dodec = 1;
            if (fsodev->lpDev->wFlags & DEVFLAGS_DYING) { //Dying Device.
                *lpNumBytesRead = 0xffffffff;
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            else {
                ENTER_INSTRUM {
                    *lpNumBytesRead = fsodev->lpDev->fnRead(fsodev->dwOpenData,buffer,nBytesToRead);
                } EXIT_INSTRUM_READ;
            }
            OpenDelRef(fsodev);
            dodec = 0;
            if (*lpNumBytesRead == 0xffffffff) {
                retval = FALSE;
                *lpNumBytesRead = 0;
            } else
                retval = TRUE;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (dodec)
            OpenDelRef(fsodev);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return retval;
}

BOOL FS_DevWriteFile(fsopendev_t *fsodev, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,
    LPOVERLAPPED lpOverlapped)
{
    BOOL retval = FALSE;
    DWORD dodec = 0;
    // Check caller buffer access
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        buffer = MapCallerPtr((LPVOID)buffer, nBytesToWrite);
        lpNumBytesWritten = MapCallerPtr(lpNumBytesWritten, sizeof(DWORD));
    }

    __try {
        if (!fsodev->lpDev)
            SetLastError(ERROR_INVALID_HANDLE_STATE);
        else {
            OpenAddRef(fsodev);
            dodec = 1;
            ENTER_INSTRUM {
            if (fsodev->lpDev->wFlags & DEVFLAGS_DYING) { //Dying Device.
                *lpNumBytesWritten = 0xffffffff;
                retval = FALSE;
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            else {
                *lpNumBytesWritten = fsodev->lpDev->fnWrite(fsodev->dwOpenData,buffer,nBytesToWrite);
                } EXIT_INSTRUM_WRITE;
            };
            OpenDelRef(fsodev);
            dodec = 0;
            if (*lpNumBytesWritten == 0xffffffff)
                *lpNumBytesWritten = 0;
            else
                retval = TRUE;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (dodec)
            OpenDelRef(fsodev);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return retval;
}

DWORD FS_DevSetFilePointer(fsopendev_t *fsodev, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
    DWORD dwMoveMethod) {
    DWORD retval = 0xffffffff;
    DWORD dodec = 0;
    // Check caller buffer access
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        lpDistanceToMoveHigh = MapCallerPtr(lpDistanceToMoveHigh, sizeof(LONG));
    }
    __try {
        if (!fsodev->lpDev)
            SetLastError(ERROR_INVALID_HANDLE_STATE);
        else {
            OpenAddRef(fsodev);
            dodec = 1;
            if (fsodev->lpDev->wFlags & DEVFLAGS_DYING) { //Dying Device.
                retval = 0xffffffff;
                SetLastError(ERROR_DEVICE_REMOVED);
            }
            else {
                ENTER_INSTRUM {
                    retval = fsodev->lpDev->fnSeek(fsodev->dwOpenData,lDistanceToMove,dwMoveMethod);
                } EXIT_INSTRUM_SEEK;
            }
            OpenDelRef(fsodev);
            dodec = 0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (dodec)
            OpenDelRef(fsodev);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return retval;
}

BOOL FS_DevDeviceIoControl(fsopendev_t *fsodev, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    BOOL retval = FALSE;
    DWORD dodec = 0;
    // Check caller buffer access
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        lpInBuf = MapCallerPtr(lpInBuf, nInBufSize);
        lpOutBuf = MapCallerPtr(lpOutBuf, nOutBufSize);
        lpBytesReturned = MapCallerPtr(lpBytesReturned, sizeof(DWORD));
    }
    __try {
        if (!fsodev->lpDev)
            SetLastError(ERROR_INVALID_HANDLE_STATE);
        else {
            if(dwIoControlCode == IOCTL_DEVICE_AUTODEREGISTER) {
                // NOTE: This IOCTL is here to avoid a deadlock between (a) a DllMain calling
                // DeregisterDevice (and hence trying to acquire the device CS *after* the 
                // loader CS and (b) Various other paths in this file where we acquire the 
                // device CS *before* the Loader CS (by calling LoadLib, FreeLib etc). 
                // See WinCE#4165. Hence this code path must NOT acquire the device CS!!
                DEBUGMSG(ZONE_DYING, (L"FS_DevDeviceIoControl:IOCTL_DEVICE_AUTODEREGISTER. lpdev=%x\r\n", fsodev->lpDev));
                fsodev->lpDev->wFlags |= DEVFLAGS_AUTO_DEREGISTER;
                retval = TRUE;
                // signal the main device (dying-devs-handler) thread
                SetEvent(g_hCleanEvt);
            }
            else {
                OpenAddRef(fsodev);
                dodec = 1;
                if (fsodev->lpDev->wFlags & DEVFLAGS_DYING) { //Dying Device.
                    retval = FALSE;
                    SetLastError(ERROR_DEVICE_REMOVED);
                }
                else {
                    ENTER_INSTRUM {
                        retval = fsodev->lpDev->fnControl(fsodev->dwOpenData,dwIoControlCode,lpInBuf,nInBufSize,lpOutBuf,nOutBufSize,lpBytesReturned);
                    } EXIT_INSTRUM_IOCONTROL;
                };
                OpenDelRef(fsodev);
                dodec = 0;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (dodec)
            OpenDelRef(fsodev);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return retval;
}

DWORD FS_DevGetFileSize(fsopendev_t *fsodev, LPDWORD lpFileSizeHigh) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return 0xffffffff;
}

BOOL FS_DevGetFileInformationByHandle(fsopendev_t *fsodev, LPBY_HANDLE_FILE_INFORMATION lpFileInfo) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevFlushFileBuffers(fsopendev_t *fsodev) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevGetFileTime(fsopendev_t *fsodev, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevSetFileTime(fsopendev_t *fsodev, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

BOOL FS_DevSetEndOfFile(fsopendev_t *fsodev) {
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

static void DevPSLNotify (DWORD flags, HPROCESS proc, HTHREAD thread) {
    fsopendev_t *fsodev;
    int         icalls, i;
    HANDLE      h[20];
    HANDLE      *ph;

    DEVICE_PSL_NOTIFY pslPacket;

    EnterCriticalSection(&g_devcs);

    fsodev = g_lpOpenDevs;

    icalls = 0;

    while (fsodev) {
        if (fsodev->hProc == proc)
            ++icalls;
        fsodev = fsodev->nextptr;
    }

    if (icalls < sizeof(h) / sizeof(h[0]))
        ph = h;
    else {
        ph = (HANDLE *)LocalAlloc (LMEM_FIXED, icalls * sizeof(HANDLE));
        if (ph==NULL) { // Run out memory. Do as much as we can
            ph = h;
            icalls= sizeof(h) / sizeof(h[0]);
        };
    };

    i = 0;
    fsodev = g_lpOpenDevs;

    while (fsodev && i < icalls) {
        if (fsodev->hProc == proc)
            ph[i++] = fsodev->KHandle;
        fsodev = fsodev->nextptr;
    }

    LeaveCriticalSection (&g_devcs);

    pslPacket.dwSize  = sizeof(pslPacket);
    pslPacket.dwFlags = flags;
    pslPacket.hProc   = proc;
    pslPacket.hThread = thread;

    for (i = 0 ; i < icalls ; ++i)
        DeviceIoControl (ph[i], IOCTL_PSL_NOTIFY, (LPVOID)&pslPacket, sizeof(pslPacket), NULL, 0, NULL, NULL);

    if (ph != h)
        LocalFree (ph);
}

void FS_DevProcNotify(DWORD flags, HPROCESS proc, HTHREAD thread) {
    switch (flags) {
        case DLL_MEMORY_LOW:
            CompactAllHeaps ();
            return;
                
#ifndef TARGET_NT
        case DLL_PROCESS_DETACH:
            // interface may not be part of device.exe, make sure we clean up if necessary
            DeleteProcessAdvertisedInterfaces(flags, proc, thread);
            break;
        case DLL_PROCESS_EXITING:
            DevPSLNotify (flags, proc, thread);
            break;
        case DLL_SYSTEM_STARTED:
            //
            // Eventually we'll want to signal all drivers that the system has
            // initialized.  For now do what's necessary.
            // 
            DevloadPostInit();
            break;
#endif // TARGET_NT
    }

	// notify the power manager
	PM_Notify(flags, proc, thread);

    return;
}

void FS_CloseAllDeviceHandles(HPROCESS proc) {
#if TARGET_NT
    ENTER_DEVICE_FUNCTION {
#endif
    fsopendev_t *fsodev = g_lpOpenDevs;
    while (fsodev) {
        if (fsodev->hProc == proc) {
#if TARGET_NT
            FS_DevCloseFileHandle(fsodev);
#else
            CloseHandle(fsodev->KHandle);
#endif
            fsodev = g_lpOpenDevs;
        } else
            fsodev = fsodev->nextptr;
    }
#if TARGET_NT
    } EXIT_DEVICE_FUNCTION;
#endif
}

#ifndef TARGET_NT
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
// dwControlCode==IOCTL_DEVICE_AUTODEREGISTER.
// An example of an auto deregister device is the console device driver.
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
            if (lpdev->wFlags & DEVFLAGS_AUTO_DEREGISTER) {
                LeaveCriticalSection(&g_devcs);
                // don't remove from list. DeregisterDevice will do that
                DEBUGMSG(ZONE_DYING, (L"Device:ProcessAutoDeregisterDevs FOUND auto-deregister lpdev=%x\r\n", lpdev));
                FS_DeregisterDevice((HANDLE)lpdev);
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
                DoFreeDevice(lpdev);
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
// returns, FS_DevDeviceIoControl will access the open handle structure so it can't be freed
// yet; it is put on the g_lpDyingOpens list.
//
void ProcessDyingOpens(void)
{
    fsopendev_t *fsodev;
    fsopendev_t *fsodevprev;

    DEBUGMSG(ZONE_DYING, (L"Device:ProcessDyingOpens About to walk dying opens list\r\n"));
    EnterCriticalSection(&g_devcs);
    fsodevprev = fsodev = g_lpDyingOpens;
    while (fsodev) {
        if (fsodev->dwOpenRefCnt == 0) {
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
            && !wcscmp(szVal, TEXT("device.exe"))) {
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


extern DBGPARAM dpCurSettings;  // defined in devload.c

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmShow)
{

    HINSTANCE hCeddkDll;
    HANDLE hevBootPhase1;

    if (IsAPIReady(SH_DEVMGR_APIS)) {
        return 0;
    }
    
    DEBUGREGISTER(NULL);
    DEBUGMSG(ZONE_BOOTSEQ, (TEXT("DEVICE: Starting boot phase 1\n")));

    // PHASE 1
    g_BootPhase = 1;
    InitOOMSettings();
    InitializeListHead(&g_DevChain);
    InitializeListHead(&g_DyingDevs);
    InitializeListHead(&g_CandidateDevs);
    g_hCleanEvt = CreateEvent(0,0,0,0);
    g_hCleanDoneEvt = CreateEvent(0, 1, 1, 0); //manual reset, init state signalled
    g_hDevApiHandle = CreateAPISet("WFLD", NUM_FDEV_APIS, FDevApiMethods, FDevApiSigs);
    g_hDevFileApiHandle = CreateAPISet("W32D", NUM_FAPIS, DevFileApiMethods, DevFileApiSigs);
    RegisterAPISet(g_hDevFileApiHandle, HT_FILE | REGISTER_APISET_TYPE);
    InitializeDeviceNotifications();
    InitializeCriticalSection(&g_devcs);
    ResourceInitModule();
    ResourceInitFromRegistry(TEXT("Drivers\\Resources"));
    SetPowerOffHandler((FARPROC)FS_PowerAllDevices);
    RegisterAPISet(g_hDevApiHandle, SH_DEVMGR_APIS);
    StartDeviceNotifyThread();

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

    // See if we are going to have two boot phases
    hevBootPhase1 = OpenEvent(EVENT_ALL_ACCESS, FALSE, _T("SYSTEM/BootPhase1"));
    if (hevBootPhase1 != NULL) {
        HANDLE hEvent;

        // Load phase 1 drivers from the boot registry
        DevloadInit();

        // Signal boot phase 1 complete
        SetEvent(hevBootPhase1);
        CloseHandle(hevBootPhase1);

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
        InitDevices();
        
        DEBUGMSG(ZONE_BOOTSEQ, (TEXT("DEVICE: Startup sequence complete\r\n")));
        SignalStartedUsingReg(); // SignalStarted call with the right args
    
    } else {
        DEBUGMSG(ZONE_BOOTSEQ, (TEXT("DEVICE: No boot registry - skipping to boot phase 2\n")));
        g_BootPhase = 2;
        DevloadInit();
        SignalStarted(_wtol(lpCmdLine));
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

        DEBUGMSG(ZONE_DYING, (TEXT("DEVICE: Setting CleanDoneEvt in WinMain.\r\n")));

        SetEvent(g_hCleanDoneEvt);
    }
    
    return 1;    // should not ever be reached
}
#else 

BOOL
WINAPI
SetPriorityClass(
    HANDLE hProcess,
    DWORD dwPriorityClass
    );

int wmain(int argc, wchar_t *argv)
{
    SetPriorityClass(GetCurrentProcess(), 0x80 /*HIGH_PRIORITY_CLASS*/);
    InitializeCriticalSection(&g_devcs);
    g_hDevFileApiHandle = (HANDLE) 0x80002000;
    if (!proxy_init(8, 4096))
        return 0;
    DevloadInit();
    DevloadPostInit();
    wait_for_exit_event(); 
    return 1;
}
#endif // TARGET_NT
