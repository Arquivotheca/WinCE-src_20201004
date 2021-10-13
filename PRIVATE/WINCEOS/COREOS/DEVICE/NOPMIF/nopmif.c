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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

//
// This module contains a version of the Power Manager entry points that
// does not call out to pm.dll.  This version of the PM entry points satisfies
// the linker without providing an implementation of the Power Manager 
// functions.
//

#include <windows.h>
#include "pmif.h"

BOOL
PM_Init(VOID)
{
    return TRUE;
}

VOID        
PmPowerHandler(BOOL bOff)
{
}

VOID
PM_Notify(
    DWORD    Flags,
    HPROCESS dwPid,
    HTHREAD  dwTid
    )
{
    return;
}


HANDLE
PM_SetPowerRequirement(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState,
    ULONG                   DeviceFlags,
    PVOID                   pvSystemState,
    ULONG                   StateFlags
    )
{
    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}

HANDLE
EX_PM_SetPowerRequirement(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState,
    ULONG                   DeviceFlags,
    PVOID                   pvSystemState,
    ULONG                   StateFlags
    )
{
    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}

DWORD
PM_ReleasePowerRequirement(
	HANDLE h
    ) 
{
    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
EX_PM_ReleasePowerRequirement(
    HANDLE h
    ) 
{
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


DWORD
PM_GetSystemPowerState(
    LPWSTR  pBuffer,
    DWORD   Length,
    PDWORD  pFlags
	)
{
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


DWORD
PM_SetSystemPowerState(
	LPCWSTR pwsState,
	DWORD   StateFlags,
    DWORD   Options
	)
{
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


DWORD
PM_DevicePowerNotify(
	PVOID                   pvDevice,
	CEDEVICE_POWER_STATE    DeviceState, 
    DWORD                   Flags
    )
{
    return ERROR_SERVICE_DOES_NOT_EXIST;
}
DWORD
EX_PM_DevicePowerNotify(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState, 
    DWORD                   Flags
    )
{
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


HANDLE
PM_RequestPowerNotifications(
    HANDLE  hMsgQ,
    DWORD   Flags
    )
{   
    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}

HANDLE
EX_PM_RequestPowerNotifications(
    HANDLE  hMsgQ,
    DWORD   Flags
    )
{   
    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}


DWORD
PM_StopPowerNotifications(
    HANDLE h
    )
{
    return ERROR_SERVICE_DOES_NOT_EXIST;
}


HANDLE
PM_RegisterPowerRelationship(
    PVOID pvParent, 
    PVOID pvChild, 
    PPOWER_CAPABILITIES pCaps,
    DWORD Flags
    )
{
    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}
HANDLE
EX_PM_RegisterPowerRelationship(
    PVOID pvParent, 
    PVOID pvChild, 
    PPOWER_CAPABILITIES pCaps,
    DWORD Flags
    )
{
    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}


DWORD
PM_ReleasePowerRelationship(
	HANDLE h
    ) 
{
    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
PM_SetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    CEDEVICE_POWER_STATE dwState
    )
{    
    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
PM_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags,
    PCEDEVICE_POWER_STATE pdwState    
    )
{    
    return ERROR_SERVICE_DOES_NOT_EXIST;
}    

