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
    UNREFERENCED_PARAMETER(bOff);
}

VOID
PM_Notify(
    DWORD    Flags,
    HPROCESS dwPid,
    HTHREAD  dwTid
    )
{
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(dwPid);
    UNREFERENCED_PARAMETER(dwTid);

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
    UNREFERENCED_PARAMETER(pvDevice);
    UNREFERENCED_PARAMETER(DeviceState);
    UNREFERENCED_PARAMETER(DeviceFlags);
    UNREFERENCED_PARAMETER(pvSystemState);
    UNREFERENCED_PARAMETER(StateFlags);

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
    UNREFERENCED_PARAMETER(pvDevice);
    UNREFERENCED_PARAMETER(DeviceState);
    UNREFERENCED_PARAMETER(DeviceFlags);
    UNREFERENCED_PARAMETER(pvSystemState);
    UNREFERENCED_PARAMETER(StateFlags);

    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}

DWORD
PM_ReleasePowerRequirement(
    HANDLE h
    ) 
{
    UNREFERENCED_PARAMETER(h);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
EX_PM_ReleasePowerRequirement(
    HANDLE h
    ) 
{
    UNREFERENCED_PARAMETER(h);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}


DWORD
PM_GetSystemPowerState(
    LPWSTR  pBuffer,
    DWORD   Length,
    DWORD *const  pFlags
    )
{
    UNREFERENCED_PARAMETER(pBuffer);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(pFlags);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
EX_PM_GetSystemPowerState(
    LPWSTR  pBuffer,
    DWORD   Length,
    DWORD *const  pFlags
    )
{
    UNREFERENCED_PARAMETER(pBuffer);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(pFlags);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
EX_PM_SetSystemPowerState(
    LPCWSTR pwsState,
    DWORD   StateFlags,
    DWORD   Options
    )
{
    UNREFERENCED_PARAMETER(pwsState);
    UNREFERENCED_PARAMETER(StateFlags);
    UNREFERENCED_PARAMETER(Options);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
PM_SetSystemPowerState(
    LPCWSTR pwsState,
    DWORD   StateFlags,
    DWORD   Options
    )
{
    UNREFERENCED_PARAMETER(pwsState);
    UNREFERENCED_PARAMETER(StateFlags);
    UNREFERENCED_PARAMETER(Options);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
PM_DevicePowerNotify(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState, 
    DWORD                   Flags
    )
{
    UNREFERENCED_PARAMETER(pvDevice);
    UNREFERENCED_PARAMETER(DeviceState);
    UNREFERENCED_PARAMETER(Flags);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}
DWORD
EX_PM_DevicePowerNotify(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState, 
    DWORD                   Flags
    )
{
    UNREFERENCED_PARAMETER(pvDevice);
    UNREFERENCED_PARAMETER(DeviceState);
    UNREFERENCED_PARAMETER(Flags);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}


HANDLE
PM_RequestPowerNotifications(
    HANDLE  hMsgQ,
    DWORD   Flags
    )
{   
    UNREFERENCED_PARAMETER(hMsgQ);
    UNREFERENCED_PARAMETER(Flags);

    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}

HANDLE
EX_PM_RequestPowerNotifications(
    HANDLE  hMsgQ,
    DWORD   Flags
    )
{   
    UNREFERENCED_PARAMETER(hMsgQ);
    UNREFERENCED_PARAMETER(Flags);

    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}


DWORD
PM_StopPowerNotifications(
    HANDLE h
    )
{
    UNREFERENCED_PARAMETER(h);

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
    UNREFERENCED_PARAMETER(pvParent);
    UNREFERENCED_PARAMETER(pvChild);
    UNREFERENCED_PARAMETER(pCaps);
    UNREFERENCED_PARAMETER(Flags);

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
    UNREFERENCED_PARAMETER(pvParent);
    UNREFERENCED_PARAMETER(pvChild);
    UNREFERENCED_PARAMETER(pCaps);
    UNREFERENCED_PARAMETER(Flags);

    SetLastError(ERROR_SERVICE_DOES_NOT_EXIST);
    return NULL;
}


DWORD
PM_ReleasePowerRelationship(
    HANDLE h
    ) 
{
    UNREFERENCED_PARAMETER(h);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
PM_SetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    CEDEVICE_POWER_STATE dwState
    )
{    
    UNREFERENCED_PARAMETER(pvDevice);
    UNREFERENCED_PARAMETER(dwDeviceFlags);
    UNREFERENCED_PARAMETER(dwState);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}
DWORD
EX_PM_SetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    CEDEVICE_POWER_STATE dwState
    )
{    
    UNREFERENCED_PARAMETER(pvDevice);
    UNREFERENCED_PARAMETER(dwDeviceFlags);
    UNREFERENCED_PARAMETER(dwState);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}

DWORD
PM_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags,
    PCEDEVICE_POWER_STATE pdwState    
    )
{    
    UNREFERENCED_PARAMETER(pvDevice);
    UNREFERENCED_PARAMETER(dwDeviceFlags);
    UNREFERENCED_PARAMETER(pdwState);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}    
DWORD
EX_PM_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags,
    PCEDEVICE_POWER_STATE pdwState    
    )
{    
    UNREFERENCED_PARAMETER(pvDevice);
    UNREFERENCED_PARAMETER(dwDeviceFlags);
    UNREFERENCED_PARAMETER(pdwState);

    return ERROR_SERVICE_DOES_NOT_EXIST;
}    

