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

#include    <windows.h>
#include    <tchar.h>
#include    <excpt.h>

#include    <katoex.h>
#include    <usbdi.h>

#define  TEST_3347_RESOLVED    1

/* Debug Zones.
 */
#ifdef  DEBUG
#define DBG_ALWAYS              0x0000
#define DBG_INIT                0x0001
#define DBG_DEINIT              0x0002
#define DBG_OPEN                0x0004
#define DBG_CLOSE               0x0008
#define DBG_BANDWIDTH           0x0010
#define DBG_HUB                 0x0020
#define DBG_DPC                 0x0040
#define DBG_PCI                 0x0080
#define DBG_TEST                0x0100
#define DBG_IOCTL               0x0200
#define DBG_DEVICE_INFO         0x0400
#define DBG_CRITSECT            0x0800
#define DBG_ALLOC               0x1000
#define DBG_FUNCTION            0x2000
#define DBG_WARNING             0x4000
#define DBG_ERROR               0x8000
#endif

#define LOG_WARNING            3

typedef BOOL (* LPUSB_DEVICE_ATTACH)(   USB_HANDLE,
                                        LPCUSB_FUNCS,
                                        LPCUSB_INTERFACE,
                                        LPCWSTR,LPBOOL,
                                        LPCUSB_DRIVER_SETTINGS,
                                        DWORD );
typedef BOOL (* LPUSB_INSTALL_DRIVER)(  LPCWSTR );
typedef BOOL (* LPUSB_UNINSTALL_DRIVER)(VOID );

typedef struct dll_entry_points {
    HINSTANCE                           hInst;                      // DLL instance handle, error if NULL
    TCHAR                               szDllName[MAX_PATH];        // copy of the dll name
    DWORD                               dwCount;                    // function count
    FARPROC                             lpProcAddr[1];              // an array of function pointers
} DLL_ENTRY_POINTS, *LPDLL_ENTRY_POINTS;

typedef struct usbd_entry_points {
    HINSTANCE                           hInst;                      // USBMOUSE.DLL
    TCHAR                               szDllName[MAX_PATH];        // copy of the dll name
    DWORD                               dwCount;
    LPUSB_DEVICE_ATTACH                 lpDeviceAttach;
    LPUSB_INSTALL_DRIVER                lpInstallDriver;
    LPUSB_UNINSTALL_DRIVER              lpUnInstallDriver;
} USBD_DRIVER_ENTRY, *LPUSBD_DRIVER_ENTRY;

typedef struct usbd_driver_entry {
    HINSTANCE                           hInst;                      // USBD.DLL
    TCHAR                               szDllName[MAX_PATH];        // copy of the dll name
    DWORD                               dwCount;
    LPOPEN_CLIENT_REGISTRY_KEY          lpOpenClientRegistyKey;
    LPREGISTER_CLIENT_DRIVER_ID         lpRegisterClientDriverID;
    LPUN_REGISTER_CLIENT_DRIVER_ID      lpUnRegisterClientDriverID;
    LPREGISTER_CLIENT_SETTINGS          lpRegisterClientSettings;
    LPUN_REGISTER_CLIENT_SETTINGS       lpUnRegisterClientSettings;
#ifdef  TEST_3347_RESOLVED
    LPGET_USBD_VERSION                  lpGetUSBDVersion;
#endif
} USBD_ENTRY_POINTS, *LPUSBD_ENTRY_POINTS;


/*
*   Kato logging, global handle and short access functions
*/
#ifdef  __cplusplus
extern "C" {
#endif

extern  HKATO   g_hKato;
BOOL    WINAPI  LoadGlobalKato      ( void );
BOOL    WINAPI  CheckKatoInit		( void );
void    WINAPI  Log                 ( LPTSTR szFormat, ... );
void    WINAPI  LWarn               ( LPTSTR szFormat, ... );
void    WINAPI  LCondVerbose        ( BOOL fVerbose, LPTSTR szFormat, ...);
void    WINAPI  LogVerboseV         (BOOL fVerbose, LPTSTR szFormat, va_list pArgs) ;
void    WINAPI  Fail                ( LPTSTR szFormat, ... );
void    WINAPI  ClearFailWarnCount  (void);
void    WINAPI  ClearFailCount  (void);

DWORD	WINAPI	GetFailCount		(void);
DWORD	WINAPI	GetWarnCount		(void);

#ifdef  __cplusplus
}
#endif


/*
*   DLL load/unload functions.
*/
BOOL    WINAPI  LoadDllGetAddress   ( LPTSTR szDllName, 
                                      LPTSTR szDllFnText[], 
                                      LPDLL_ENTRY_POINTS lpDll 
                                    );
BOOL    WINAPI  UnloadDll           ( LPDLL_ENTRY_POINTS lpDll );
LPTSTR  WINAPI  SplitCommandLine    ( LPTSTR lpCmd );


/*
*   USB helper functions.
*/
LPTSTR  WINAPI  GetUSBErrorString   ( LPTSTR lpStr, DWORD dwError );
LPTSTR  WINAPI  USBGetErrorString   ( LPTSTR lpStr, DWORD dwError );
#define USBGetErrorString(s,e)      GetUSBErrorString(s,e)






