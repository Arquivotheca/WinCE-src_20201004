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

    pwrmgr.h

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
BOOL
PM_Init(
    VOID
    );

DWORD
PM_GetSystemPowerState(
    LPWSTR  pBuffer,
    DWORD   Length,
    PDWORD  pFlags
	);

DWORD
PM_DevicePowerNotify(
	PVOID                   pvDevice,
	CEDEVICE_POWER_STATE    DeviceState,
    DWORD                   Flags
    );

VOID
PM_Notify(
    DWORD flags, 
    HPROCESS proc, 
    HTHREAD thread
    );

DWORD
PM_SetSystemPowerState(
	LPCWSTR pwsState,
	DWORD   StateFlags,
    DWORD   Options
	);

HANDLE
PM_SetPowerRequirement(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState,
    ULONG                   DeviceFlags,
    PVOID                   pvSystemState,
    ULONG                   StateFlags    
	);

DWORD 
PM_ReleasePowerRequirement(
	HANDLE hPowerReq
    );

HANDLE
PM_RequestPowerNotifications(
    HANDLE  hMsgQ,    
    DWORD   Flags
    );

DWORD
PM_StopPowerNotifications(
    HANDLE h
    );

DWORD
PM_VetoPowerNotification(
    DWORD   Flags
    );

HANDLE
PM_RegisterPowerRelationship(
    PVOID pvParent, 
    PVOID pvChild, 
    PPOWER_CAPABILITIES pCaps,
    DWORD Flags
    );

DWORD
PM_ReleasePowerRelationship(
    HANDLE h
    );

DWORD
PM_SetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    CEDEVICE_POWER_STATE dwState
    );

DWORD
PM_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags,
    PCEDEVICE_POWER_STATE pdwState    
    );

#ifdef __cplusplus
}
#endif	// __cplusplus

