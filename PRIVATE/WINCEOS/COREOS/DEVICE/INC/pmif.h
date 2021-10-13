//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#endif	// __cplusplus

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
    PDWORD  pFlags
	);

DWORD WINAPI
PM_DevicePowerNotify(
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

HANDLE WINAPI
PM_SetPowerRequirement(
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

HANDLE WINAPI
PM_RequestPowerNotifications(
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
PM_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags,
    PCEDEVICE_POWER_STATE pdwState    
    );

#ifdef __cplusplus
}
#endif	// __cplusplus

