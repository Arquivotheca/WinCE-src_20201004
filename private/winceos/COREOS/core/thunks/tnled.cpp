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
#include <nled.h>

static HANDLE ghNLed = INVALID_HANDLE_VALUE;

DWORD
IsNLedDriverReady(void)
{
    DWORD dwStatus = ERROR_SUCCESS;

    // do we have the NLED handle?  If not, we need to get the NLED driver's device name and
    // open a handle to it.  The GUID representing NLED drivers must match NLED_DRIVER_CLASS
    // in NLED.h.
    if(ghNLed == INVALID_HANDLE_VALUE) {
        // {CBB4F234-F35F-485b-A490-ADC7804A4EF3}
        const GUID gNLedClass = 
            { 0xcbb4f234, 0xf35f, 0x485b, { 0xa4, 0x90, 0xad, 0xc7, 0x80, 0x4a, 0x4e, 0xf3 } };
        DEVMGR_DEVICE_INFORMATION di;
        memset(&di, 0, sizeof(di));
        di.dwSize = sizeof(di);
        HANDLE hSrhHandle = FindFirstDevice(DeviceSearchByGuid, &gNLedClass, &di);
        if (hSrhHandle != INVALID_HANDLE_VALUE) {
            ghNLed = CreateFileW(di.szDeviceName, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            CloseHandle(hSrhHandle);
        }
    }
    // could not get NLED handle
    if(ghNLed == INVALID_HANDLE_VALUE) {
        dwStatus = ERROR_NOT_READY;
    }
    return dwStatus;
}

BOOL WINAPI
NLedGetDeviceInfo( UINT nInfoId, void *pOutput )
{
    DWORD dwStatus = IsNLedDriverReady();
    DWORD dwBytesReturned = 0, dwSize;
    BOOL fOk = FALSE;

    // if the NLED driver is not available, we mask this and report that there
    // are no LEDs available in the system
    if(dwStatus != ERROR_SUCCESS) {
        if (nInfoId == NLED_COUNT_INFO_ID && pOutput) {
            ((NLED_COUNT_INFO*) pOutput)->cLeds = 0;
            fOk = TRUE;
        }
        else {
            SetLastError(dwStatus);
        }
        goto cleanUp;
    }

    DEBUGCHK(ghNLed != INVALID_HANDLE_VALUE);

    switch(nInfoId) {
    case NLED_COUNT_INFO_ID:
        dwSize = sizeof(NLED_COUNT_INFO);
        break;
    case NLED_SUPPORTS_INFO_ID:
        dwSize = sizeof(NLED_SUPPORTS_INFO);
        break;
    case NLED_SETTINGS_INFO_ID:
        dwSize = sizeof(NLED_SETTINGS_INFO);
        break;
    case NLED_TYPE_INFO_ID:
        dwSize = sizeof(NLED_TYPE_INFO);
        break;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
        break;
    }


    // call the NLED driver
    fOk = DeviceIoControl(ghNLed, IOCTL_NLED_GETDEVICEINFO, &nInfoId, sizeof(nInfoId),
        pOutput, dwSize, &dwBytesReturned, NULL);

cleanUp:
    return fOk;
}

BOOL WINAPI 
NLedSetDevice( UINT nDeviceId, void *pInput )
{
    DWORD dwStatus = IsNLedDriverReady();
    BOOL fOk;

    if(dwStatus != ERROR_SUCCESS) {
        SetLastError(dwStatus);
        return FALSE;
    }

    DEBUGCHK(ghNLed != INVALID_HANDLE_VALUE);
    if (nDeviceId == NLED_SETTINGS_INFO_ID)  {
        // call the NLED driver -- the only supported nDeviceId is NLED_SETTINGS_INFO_ID
        fOk = DeviceIoControl(ghNLed, IOCTL_NLED_SETDEVICE, pInput, sizeof(NLED_SETTINGS_INFO),
            NULL, 0, NULL, NULL);
    }
    else {
        fOk = FALSE;
        SetLastError(ERROR_INVALID_PARAMETER);
    }

    return fOk;
}
