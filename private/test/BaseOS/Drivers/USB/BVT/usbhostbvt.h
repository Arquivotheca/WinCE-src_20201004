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
#ifndef     __USBHOSTBVT_H__
#define     __USBHOSTBVT_H__

#include    <windows.h>
#include    <tchar.h>
#include    <usbdi.h>



//******************************************************************************
//***** Defines, typedefs,...
//******************************************************************************
#define MAX_RECONNECT_RETRY 3

typedef struct dll_entry_points {
    HINSTANCE                           hInst;                      // DLL instance handle, error if NULL
    TCHAR                               szDllName[MAX_PATH];        // copy of the dll name
    DWORD                               dwCount;                    // function count
    FARPROC                             lpProcAddr[1];              // an array of function pointers
} DLL_ENTRY_POINTS, *LPDLL_ENTRY_POINTS;

typedef struct usbd_driver_entry {
    HINSTANCE                           hInst;                      // USBD.DLL
    TCHAR                               szDllName[MAX_PATH];        // copy of the dll name
    DWORD                               dwCount;
    LPOPEN_CLIENT_REGISTRY_KEY          lpOpenClientRegistryKey;
    LPREGISTER_CLIENT_DRIVER_ID         lpRegisterClientDriverID;
    LPUN_REGISTER_CLIENT_DRIVER_ID      lpUnRegisterClientDriverID;
    LPREGISTER_CLIENT_SETTINGS          lpRegisterClientSettings;
    LPUN_REGISTER_CLIENT_SETTINGS       lpUnRegisterClientSettings;
    LPGET_CLIENT_REGISTRY_PATH            lpGetClientRegistryPath;
    LPGET_USBD_VERSION                  lpGetUSBDVersion;
} USBD_ENTRY_POINTS, *LPUSBD_ENTRY_POINTS;

typedef struct __usb_dev {
    struct __usb_dev        *next;              // for chaining usb dev structs
    HANDLE                  hHeap;              // we make our own heap
    USB_HANDLE              hDevice;            // by USBAttach()
    LPCUSB_FUNCS            lpUsbFuncs;         // by USBAttach()
    LPCUSB_INTERFACE        lpInterface;        // by USBAttach()
    LPCWSTR                 szUniqueDriverId;   // by loader.cpp
    LPCUSB_DRIVER_SETTINGS  lpDriverSettings;   // by loader.cpp
    BOOL                    fAbort;             // device removed
    BOOL                    *pfAbort;           // use when not sure if copy
    LONG                    lRefCount;          // structure reference count
    LONG                    *plRefCount;        // use when not sure if copy
    LONG                    lCplRefCount;       // structure reference count cpl
    LONG                    *plCplRefCount;     // use when not sure if copy
    HANDLE                  hEvent;             // for USBNotify()
    HANDLE                  hThread;            // extra, user data
    DWORD                   dwThreadId;         // extra, user data
    DWORD                   dwData;             // extra, user data
    PVOID                   pvPtr;              // extra, user data
} USB_DEV, *LPUSB_DEV;

//******************************************************************************
//***** Global declarations
//*****************************************************************************
#define MAX_PATH 260
#define SUSPEND_RESUME_DISABLED 0x01
#define SUSPEND_RESUME_ENABLED   0x02

#define TESTDLL  "USBHostBVT.dll"
#define TESTCLIENT "USBHostBVT"
#define TEST_REG_PATH "\\Drivers\\USB\\LoadClients\\1118_37642\\Default\\Default\\USBHostBVT"
#define TEST_REG_NAME "DLL"
#define TEST_REG_VALUE "USBHostBVT.dll"
#define TEST_REG_PATH_CLIENT "\\Drivers\\USB\\ClientDrivers\\USBHostBVT"
#define TEST_REG_NAME_CLIENT "DLL"
#define TEST_REG_VALUE_CLIENT "USBHostBVT.dll"


static TCHAR *gcszDriverId= TEXT(TESTCLIENT);
static TCHAR *gcszDriverName=TEXT(TESTDLL);
static  LPUSB_DEV           glpUsbDev;
static  CRITICAL_SECTION    gcsUsbDev;
static  USBD_ENTRY_POINTS    USBDEntryPoints;
static  TCHAR               *USBDEntryPointsText[] = {
    { TEXT("OpenClientRegistryKey") },
    { TEXT("RegisterClientDriverID") },
    { TEXT("UnRegisterClientDriverID") },
    { TEXT("RegisterClientSettings") },
    { TEXT("UnRegisterClientSettings") },
    { TEXT("GetClientRegistryPath")},
    { TEXT("GetUSBDVersion") },
    NULL
    };
static BOOL        g_fUSBDLoaded=FALSE;
static BOOL        g_fUSBHostClientRegistered=FALSE;


//******************************************************************************
//***** Global function declarations
//******************************************************************************
BOOL	Util_LoadUSBD();
BOOL	Util_UnLoadUSBD();
void	Util_PrepareDriverSettings( LPUSB_DRIVER_SETTINGS lpDriverSettings, DWORD dwClass );
DWORD	Util_UnRegisterClient(TCHAR *pszDriverId, LPUSB_DRIVER_SETTINGS lpDriverSettings);
DWORD	Util_GetUSBDVersion();
DWORD	Util_SuspendAndResume();
DWORD	Util_RegistryPathTest();

DWORD	Test_GetUSBDVersion();						// Test# 100			
DWORD	Test_RegisterClient();						// Test# 101
DWORD	Test_RegistrySettingsforClient();			// Test# 102
DWORD	Test_OpenRegistryForClient();				// Test# 103
DWORD	Test_GetRegistryPath();						// Test# 104
DWORD	Test_UnRegisterClient();					// Test# 105
DWORD	Test_RegistrySettingsforUnloadedClient();	// Test# 106
DWORD	Test_OpenRegistryForUnloadedClient();		// Test# 107
DWORD	Test_GetRegistryPathForUnloadedClient();	// Test# 108
DWORD	Test_SuspendResumeGetVersion();				// Test# 200
DWORD	Test_SuspendResumeRegisteredClient();		// Test# 201
DWORD	Test_SuspendResumeUnRegisteredClient();		// Test# 202


//******************************************************************************
//***** Dll Load/Unload functions
//******************************************************************************
BOOL    WINAPI  LoadDllGetAddress   ( LPTSTR szDllName, 
                                      LPTSTR szDllFnText[], 
                                      LPDLL_ENTRY_POINTS lpDll 
                                    );
BOOL    WINAPI  UnloadDll           ( LPDLL_ENTRY_POINTS lpDll );
LPTSTR  WINAPI  SplitCommandLine    ( LPTSTR lpCmd );

#endif  // __USBHOSTBVT_H__