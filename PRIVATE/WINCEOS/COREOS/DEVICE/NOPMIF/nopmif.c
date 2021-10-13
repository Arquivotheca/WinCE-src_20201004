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

    pwrmgr.c

Abstract:

    Power Manager API

Revision History:

--*/

#include <windows.h>
#include "pwrmgr.h"

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


DWORD
PM_ReleasePowerRequirement(
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


HANDLE
PM_RequestPowerNotifications(
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

// EOF
