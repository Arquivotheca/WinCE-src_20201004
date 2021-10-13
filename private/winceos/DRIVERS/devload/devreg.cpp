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
#include <windows.h>
#include <devload.h>

//
// Function to open the device key specified by the active key
//
// The caller is responsible for closing the returned HKEY
//
HKEY
OpenDeviceKey(
    LPCTSTR ActiveKey
    )
{
    TCHAR DevKey[256];
    HKEY hDevKey;
    HKEY hActive;
    DWORD ValType;
    DWORD ValLen;
    DWORD status;

    //
    // Get the device key from the active device registry key
    //
    status = RegOpenKeyEx(
                HKEY_LOCAL_MACHINE,
                ActiveKey,
                0,
                0,
                &hActive);
    if (status) {
        SetLastError(status);
        return NULL;
    }

    hDevKey = NULL;

    ValLen = sizeof(DevKey);
    status = RegQueryValueEx(       // Retrieve the device key
                hActive,
                DEVLOAD_DEVKEY_VALNAME,
                NULL,
                &ValType,
                (PUCHAR)DevKey,
                &ValLen);
    if (status != ERROR_SUCCESS) {
        goto odk_fail;
    }

    status = RegOpenKeyEx(          // Open the device key
                HKEY_LOCAL_MACHINE,
                DevKey,
                0,
                0,
                &hDevKey);
    if (status) {
        hDevKey = NULL;
    }

odk_fail:
    RegCloseKey(hActive);
    if(status != ERROR_SUCCESS) {
        SetLastError(status);
    }
    return hDevKey;
}   // OpenDeviceKey

