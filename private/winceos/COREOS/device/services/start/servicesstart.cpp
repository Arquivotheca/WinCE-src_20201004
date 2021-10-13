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
/* File:    servmain.c
 *
 * Purpose: WinCE service manager startup
 *
 */

//
// This process starts up servicesEnum.dll driver, which is responsible
// for starting all static services specified under HKLM\Services.
// The process exits as soon as the services have been initialized.
//
#include <windows.h>
#include <devload.h>

#define SERVICE_ROOT_PATH TEXT("Services")
// --------------------------------------------------------------------
INT WINAPI 
WinMain(
    HINSTANCE /*hInstance*/, 
    HINSTANCE /*hPrevInstance*/, 
    LPWSTR lpCmdLine,   
    INT /*nCmdShow*/)
{
    HKEY RootKey;
    DWORD status;
    DWORD ValType;
    DWORD ValLen;
    TCHAR BusName[DEVKEY_LEN];
    REGINI reg[1];
    DWORD  dwRegCount=0;
    RETAILMSG(0,(TEXT("Initializating services for Services.exe\r\n")));

    status = RegOpenKeyEx( HKEY_LOCAL_MACHINE, SERVICE_ROOT_PATH,0,0, &RootKey);
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

    // Someday we'll want to track the handle returned by ActivateDevice so that
    // we can potentially deactivate it later. But since this code refers only to
    // builtin (ie, static) devices, we can just throw away the key.
    if (dwRegCount) 
        ActivateDeviceEx(SERVICE_ROOT_PATH,reg,dwRegCount,NULL); 
    else
        ActivateDevice(SERVICE_ROOT_PATH, 0);  

    SignalStarted((lpCmdLine!=NULL?_wtol(lpCmdLine):60));

    RETAILMSG(0,(TEXT("Completed initializating services for Services.exe\r\n")));
    return 1;
}

