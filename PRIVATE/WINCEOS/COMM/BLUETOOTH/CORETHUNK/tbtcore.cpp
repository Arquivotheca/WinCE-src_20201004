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

// To avoid including a billion header files
#define PBASEBAND_CONNECTION_DATA	PBASEBAND_CONNECTION

#include <objbase.h>
#include <initguid.h>
#include "bt_api.h"
#include "ws2bth.h"
#include "bt_ioctl.h"

static HANDLE g_hDev = INVALID_HANDLE_VALUE;

BOOL LoadDriver() 	
{
	HANDLE hTemp;
	
    if(INVALID_HANDLE_VALUE == g_hDev){
        hTemp = CreateFile(TEXT("BTD0:"),
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ|FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, 0, NULL);
        if(InterlockedCompareExchangePointer(&g_hDev,
                        hTemp,
                        INVALID_HANDLE_VALUE) != INVALID_HANDLE_VALUE){
            CloseHandle(hTemp);
        }
        return (INVALID_HANDLE_VALUE != g_hDev);
    }
   
	return TRUE;
}

HANDLE RequestBluetoothNotifications
(
DWORD dwClass,
HANDLE hMsgQ
) {
	HANDLE h = NULL;
		
	if (! LoadDriver()) {
		return NULL;
	}	
	
	BTAPICALL bc;
	memset(&bc, 0, sizeof(bc));
	bc.RequestBluetoothNotifications_p.dwClass = dwClass;	
	bc.RequestBluetoothNotifications_p.hMsgQ = hMsgQ;

	if (DeviceIoControl(g_hDev, BT_IOCTL_RequestBluetoothNotifications, &bc, sizeof(bc), NULL, NULL, NULL, NULL)) {
		h = bc.RequestBluetoothNotifications_p.hOut;
	}
	
	return h;
}

BOOL StopBluetoothNotifications
(
HANDLE h
) {		
	if (! LoadDriver()) {
		return FALSE;
	}
	
	BTAPICALL bc;
	memset (&bc, 0, sizeof(bc));
	bc.StopBluetoothNotifications_p.h = h;

	return DeviceIoControl (g_hDev, BT_IOCTL_StopBluetoothNotifications, &bc, sizeof(bc), NULL, NULL, NULL, NULL);
}

