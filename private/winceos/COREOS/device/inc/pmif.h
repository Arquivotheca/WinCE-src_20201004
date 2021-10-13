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
/*++


Module Name:

    pmif.h

Abstract:

    Power Manager API

Revision History:

--*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <pmpriv.h>

// local function prototypes
BOOL WINAPI
PM_Init(
    VOID
    );

DWORD WINAPI
PM_GetSystemPowerState(
    LPWSTR  pBuffer,
    DWORD   Length,
    DWORD *const  pFlags
    );
DWORD WINAPI
EX_PM_GetSystemPowerState(
    __out_ecount(Length) LPWSTR  pBuffer,
    DWORD   Length,
    DWORD *const  pFlags
    );

DWORD WINAPI
PM_DevicePowerNotify(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState,
    DWORD                   Flags
    );
DWORD WINAPI
EX_PM_DevicePowerNotify(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState,
    DWORD                   Flags
    );

VOID WINAPI
PM_Notify(
    DWORD flags, 
    HPROCESS proc, 
    HTHREAD thread
    );

DWORD WINAPI
PM_SetSystemPowerState(
    LPCWSTR pwsState,
    DWORD   StateFlags,
    DWORD   Options
    );
DWORD WINAPI
EX_PM_SetSystemPowerState(
    LPCWSTR pwsState,
    DWORD   StateFlags,
    DWORD   Options
    );

HANDLE WINAPI
PM_SetPowerRequirement(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState,
    ULONG                   DeviceFlags,
    PVOID                   pvSystemState,
    ULONG                   StateFlags    
    );
HANDLE WINAPI
EX_PM_SetPowerRequirement(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState,
    ULONG                   DeviceFlags,
    PVOID                   pvSystemState,
    ULONG                   StateFlags    
    );

DWORD  WINAPI
PM_ReleasePowerRequirement(
    HANDLE hPowerReq
    );

DWORD  WINAPI
EX_PM_ReleasePowerRequirement(
    HANDLE hPowerReq
    );

HANDLE WINAPI
PM_RequestPowerNotifications(
    HANDLE  hMsgQ,    
    DWORD   Flags
    );

HANDLE WINAPI
EX_PM_RequestPowerNotifications(
    HANDLE  hMsgQ,    
    DWORD   Flags
    );

DWORD WINAPI
PM_StopPowerNotifications(
    HANDLE h
    );

DWORD WINAPI
PM_VetoPowerNotification(
    DWORD   Flags
    );

HANDLE WINAPI
PM_RegisterPowerRelationship(
    PVOID pvParent, 
    PVOID pvChild, 
    PPOWER_CAPABILITIES pCaps,
    DWORD Flags
    );
HANDLE WINAPI
EX_PM_RegisterPowerRelationship(
    PVOID pvParent, 
    PVOID pvChild, 
    PPOWER_CAPABILITIES pCaps,
    DWORD Flags
    );

DWORD WINAPI
PM_ReleasePowerRelationship(
    HANDLE h
    );

DWORD WINAPI
PM_SetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    CEDEVICE_POWER_STATE dwState
    );
DWORD WINAPI
EX_PM_SetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    CEDEVICE_POWER_STATE dwState
    );


DWORD WINAPI
PM_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags,
    PCEDEVICE_POWER_STATE pdwState    
    );

DWORD WINAPI
EX_PM_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags,
    PCEDEVICE_POWER_STATE pdwState    
    );

#ifdef __cplusplus
}
#endif  // __cplusplus

