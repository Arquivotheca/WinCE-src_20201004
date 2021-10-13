//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
// This module contains a set of Power Manager entry points that call out
// to pm.dll.  Linking with this version of the entry points introduces
// a linker dependency on pm.dll and enables OEMs to implement a Power
// Manager.
//

#include <windows.h>
#include "pmif.h"

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
        
/* Note that Power Manager may not be loaded, in which case we return ERROR_SERVICE_NOT_ACTIVE */
BOOL
IsPowerManagerReady(void)
{
    static BOOL fReady = FALSE;
    static HANDLE hevPMReady = NULL;

    // if the PM hasn't already started, look for it to set an event to notify us when it becomes available
    if(fReady == FALSE) {
        if(hevPMReady == NULL) {
            hevPMReady = OpenEvent(EVENT_ALL_ACCESS, FALSE, _T("SYSTEM/PowerManagerReady"));
        }
        if(hevPMReady != NULL) {
            if(WaitForSingleObject(hevPMReady, 0) == WAIT_OBJECT_0) {
                fReady = TRUE;
                CloseHandle(hevPMReady);
                hevPMReady = NULL;
            }
        }
    }

    return fReady;
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
    if (IsPowerManagerReady()) {
        __try {
            h = PmSetPowerRequirement(pvDevice,
                                      DeviceState,
                                      DeviceFlags,
                                      pvSystemState,
                                      StateFlags);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            RETAILMSG(1, (TEXT("PmSetPowerRequirement: EXCEPTION:0x%x\n"), GetExceptionCode()));
        }
    }
    else
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
    return h;
}


DWORD
PM_ReleasePowerRequirement(
    HANDLE h
    ) 
{
    DWORD dwErr;
    if (IsPowerManagerReady()) {
        __try {
            dwErr = PmReleasePowerRequirement(h, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = GetExceptionCode();
            RETAILMSG(1,(TEXT("PmReleasePowerRequirement: EXCEPTION:0x%x\n"), dwErr));
        }
    }
    else
        dwErr = ERROR_SERVICE_NOT_ACTIVE;
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
    if (IsPowerManagerReady()) {
        __try {
            dwErr = PmGetSystemPowerState(pBuffer, Length, pFlags);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = GetExceptionCode();
            RETAILMSG(1,(TEXT("PmGetSystemPowerState: EXCEPTION:0x%x\n"), dwErr));
        }
    }
    else
        dwErr = ERROR_SERVICE_NOT_ACTIVE;
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
    if (IsPowerManagerReady()) {
        __try {
            dwErr = PmSetSystemPowerState(pwsState, StateFlags, Options);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = GetExceptionCode();
            RETAILMSG(1,(TEXT("PmSetSystemPowerState: EXCEPTION:0x%x\n"), dwErr));
        }
    }
    else
        dwErr = ERROR_SERVICE_NOT_ACTIVE;
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
    if (IsPowerManagerReady()) {
        __try {
            dwErr = PmDevicePowerNotify(pvDevice, DeviceState, Flags);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = GetExceptionCode();
            RETAILMSG(1,(TEXT("PmDevicePowerNotify: EXCEPTION:0x%x\n"), dwErr));
        }
    }
    else
        dwErr = ERROR_SERVICE_NOT_ACTIVE;
    return dwErr;
}


HANDLE
PM_RequestPowerNotifications(
    HANDLE  hMsgQ,
    DWORD   Flags
    )
{   
    HANDLE h = NULL;
    if (IsPowerManagerReady()) {
        __try {
            h = PmRequestPowerNotifications(hMsgQ, Flags);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            RETAILMSG(1,(TEXT("PmRequestPowerNotifications: EXCEPTION:0x%x\n"), GetExceptionCode()));
        }
    }
    else
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
    return h;
}


DWORD
PM_StopPowerNotifications(
    HANDLE h
    )
{
    DWORD dwErr;
    if (IsPowerManagerReady()) {
        __try {
            dwErr = PmStopPowerNotifications(h);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = GetExceptionCode();    
            RETAILMSG(1,(TEXT("PmStopPowerNotifications: EXCEPTION:0x%x\n"), dwErr));
        }
    }
    else
        dwErr = ERROR_SERVICE_NOT_ACTIVE ;
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

    if (IsPowerManagerReady()) {
        __try {
            h = PmRegisterPowerRelationship(pvParent, pvChild, pCaps, Flags);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            RETAILMSG(1,(TEXT("PmRegisterPowerRelationship: EXCEPTION:0x%x\n"), GetExceptionCode()));
        }
    }
    else
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
    return h;
}


DWORD
PM_ReleasePowerRelationship(
    HANDLE h
    ) 
{
    DWORD dwErr;
    if (IsPowerManagerReady()) {
        __try {
            dwErr = PmReleasePowerRelationship(h);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = GetExceptionCode();
            RETAILMSG(1,(TEXT("PmReleasePowerRelationship: EXCEPTION:0x%x\n"), dwErr));
        }
    }
    else
        dwErr = ERROR_SERVICE_NOT_ACTIVE ;
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
    if (IsPowerManagerReady()) {
        __try {
            dwErr = PmSetDevicePower(pvDevice, 
                                     dwDeviceFlags, 
                                     dwState);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = GetExceptionCode();
            RETAILMSG(1,(TEXT("PmSetDevicePower: EXCEPTION:0x%x\n"), dwErr));
        }
    }
    else
        dwErr = ERROR_SERVICE_NOT_ACTIVE ;
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

    if (IsPowerManagerReady()) {
        __try {
            dwErr = PmGetDevicePower(pvDevice, 
                                     dwDeviceFlags,
                                     pdwState);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = GetExceptionCode();
            RETAILMSG(1,(TEXT("PmGetDevicePower: EXCEPTION:0x%x\n"), dwErr));
        }
    }
    else
        dwErr = ERROR_SERVICE_NOT_ACTIVE ;
    return dwErr;
}    

