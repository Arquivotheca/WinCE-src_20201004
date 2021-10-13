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
    BOOL fOk;
    
    __try {
        fOk = PmInit();
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        RETAILMSG(1, (TEXT("PmInit: EXCEPTION:0x%x\r\n"), GetExceptionCode()));
        fOk = FALSE;
    }

    return fOk;
}
        
VOID
PM_Notify(
    DWORD    Flags,
    HPROCESS dwPid,
    HTHREAD  dwTid
    )
{
    __try {
        PmNotify(Flags, dwPid, dwTid);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        RETAILMSG(1,(TEXT("PmNotify: EXCEPTION:0x%x\n"), GetExceptionCode()));
    }
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
    HANDLE h = NULL;
    __try {
        h = PmSetPowerRequirement(pvDevice,
                                  DeviceState,
                                  DeviceFlags,
                                  pvSystemState,
                                  StateFlags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        RETAILMSG(1, (TEXT("PmSetPowerRequirement: EXCEPTION:0x%x\n"), GetExceptionCode()));
    }
    return h;
}


DWORD
PM_ReleasePowerRequirement(
	HANDLE h
    ) 
{
    DWORD dwErr;
    __try {
        dwErr = PmReleasePowerRequirement(h, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = GetExceptionCode();
        RETAILMSG(1,(TEXT("PmReleasePowerRequirement: EXCEPTION:0x%x\n"), dwErr));
    }
    return dwErr;
}


DWORD
PM_GetSystemPowerState(
    LPWSTR  pBuffer,
    DWORD   Length,
    PDWORD  pFlags
	)
{
    DWORD dwErr;
    // Check caller buffer access
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pBuffer = MapCallerPtr((LPVOID)pBuffer, Length*sizeof(WCHAR));
        pFlags = MapCallerPtr(pFlags, sizeof(DWORD));
    }

    __try {
        dwErr = PmGetSystemPowerState(pBuffer, Length, pFlags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = GetExceptionCode();
        RETAILMSG(1,(TEXT("PmGetSystemPowerState: EXCEPTION:0x%x\n"), dwErr));
    }
    return dwErr;
}


DWORD
PM_SetSystemPowerState(
	LPCWSTR pwsState,
	DWORD   StateFlags,
    DWORD   Options
	)
{
    DWORD dwErr;
    __try {
        dwErr = PmSetSystemPowerState(pwsState, StateFlags, Options);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = GetExceptionCode();
        RETAILMSG(1,(TEXT("PmSetSystemPowerState: EXCEPTION:0x%x\n"), dwErr));
    }
    return dwErr;
}


DWORD
PM_DevicePowerNotify(
	PVOID                   pvDevice,
	CEDEVICE_POWER_STATE    DeviceState, 
    DWORD                   Flags
    )
{
    DWORD dwErr;
    __try {
        dwErr = PmDevicePowerNotify(pvDevice, DeviceState, Flags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = GetExceptionCode();
        RETAILMSG(1,(TEXT("PmDevicePowerNotify: EXCEPTION:0x%x\n"), dwErr));
    }
    return dwErr;
}


HANDLE
PM_RequestPowerNotifications(
    HANDLE  hMsgQ,
    DWORD   Flags
    )
{   
    HANDLE h = NULL;
    __try {
        h = PmRequestPowerNotifications(hMsgQ, Flags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        RETAILMSG(1,(TEXT("PmRequestPowerNotifications: EXCEPTION:0x%x\n"), GetExceptionCode()));
    }
    return h;
}


DWORD
PM_StopPowerNotifications(
    HANDLE h
    )
{
    DWORD dwErr;
    __try {
        dwErr = PmStopPowerNotifications(h);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = GetExceptionCode();    
        RETAILMSG(1,(TEXT("PmStopPowerNotifications: EXCEPTION:0x%x\n"), dwErr));
    }
    return dwErr;
}


HANDLE
PM_RegisterPowerRelationship(
    PVOID pvParent, 
    PVOID pvChild, 
    PPOWER_CAPABILITIES pCaps,
    DWORD Flags
    )
{
    HANDLE h = NULL;
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pCaps = MapCallerPtr((LPVOID)pCaps, sizeof(POWER_CAPABILITIES));
    }
    __try {
        h = PmRegisterPowerRelationship(pvParent, pvChild, pCaps, Flags);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        RETAILMSG(1,(TEXT("PmRegisterPowerRelationship: EXCEPTION:0x%x\n"), GetExceptionCode()));
    }
    return h;
}


DWORD
PM_ReleasePowerRelationship(
	HANDLE h
    ) 
{
    DWORD dwErr;
    __try {
        dwErr = PmReleasePowerRelationship(h);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = GetExceptionCode();
        RETAILMSG(1,(TEXT("PmReleasePowerRelationship: EXCEPTION:0x%x\n"), dwErr));
    }
    return dwErr;
}

DWORD
PM_SetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    CEDEVICE_POWER_STATE dwState
    )
{    
    DWORD dwErr;
    __try {
        dwErr = PmSetDevicePower(pvDevice, 
                                 dwDeviceFlags, 
                                 dwState);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = GetExceptionCode();
        RETAILMSG(1,(TEXT("PmSetDevicePower: EXCEPTION:0x%x\n"), dwErr));
    }
    return dwErr;
}

DWORD
PM_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags,
    PCEDEVICE_POWER_STATE pdwState    
    )
{    
    DWORD dwErr;
    if (PSLGetCallerTrust() != OEM_CERTIFY_TRUST) {
        pdwState = MapCallerPtr((LPVOID)pdwState, sizeof(CEDEVICE_POWER_STATE));
    }
    __try {
        dwErr = PmGetDevicePower(pvDevice, 
                                 dwDeviceFlags,
                                 pdwState);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = GetExceptionCode();
        RETAILMSG(1,(TEXT("PmGetDevicePower: EXCEPTION:0x%x\n"), dwErr));
    }
    return dwErr;
}    

// EOF
