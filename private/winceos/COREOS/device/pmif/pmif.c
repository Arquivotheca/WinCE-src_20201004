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
            hevPMReady = OpenEvent(EVENT_MODIFY_STATE, FALSE, _T("SYSTEM/PowerManagerReady"));
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
    if (IsPowerManagerReady()) {
        __try {
            PmNotify(Flags, dwPid, dwTid);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            RETAILMSG(1,(TEXT("PmNotify: EXCEPTION:0x%x\n"), GetExceptionCode()));
        }
    }
    return;
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
    HANDLE h = NULL;
    TCHAR iDevice[MAX_PATH];
    TCHAR iSystemState[MAX_PATH];
    if (pvDevice)
        VERIFY(SUCCEEDED(StringCchCopy(iDevice,MAX_PATH,(LPCTSTR)pvDevice)));
    if (pvSystemState)
        VERIFY(SUCCEEDED(StringCchCopy(iSystemState,MAX_PATH,(LPCTSTR)pvSystemState)));
        
    if (IsPowerManagerReady()) {
        __try {
            h = PmSetPowerRequirement((PVOID)(pvDevice?iDevice:0),
                                      DeviceState,
                                      DeviceFlags,
                                      (PVOID)(pvSystemState?iSystemState:0),
                                      StateFlags);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            RETAILMSG(1, (TEXT("PmSetPowerRequirement: EXCEPTION:0x%x\n"), GetExceptionCode()));
        }
    }
    else
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
    return h;
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
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    h = EX_PM_SetPowerRequirement( pvDevice, DeviceState, DeviceFlags, pvSystemState, StateFlags );
    CeSetDirectCall(fKernel);
    return h;
}

DWORD
EX_PM_ReleasePowerRequirement(
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
PM_ReleasePowerRequirement(
    HANDLE h
    ) 
{
    DWORD dwErr;
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    dwErr = EX_PM_ReleasePowerRequirement(h);
    CeSetDirectCall(fKernel);
    return dwErr;
}
DWORD
PM_GetSystemPowerState(
    LPWSTR  pBuffer,
    DWORD   Length,
    DWORD *const  pFlags
    )
{
    DWORD dwErr;
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    dwErr = EX_PM_GetSystemPowerState(pBuffer,Length,pFlags );
    CeSetDirectCall(fKernel);
    return dwErr;
}
DWORD
EX_PM_GetSystemPowerState(
    __out_ecount(Length) LPWSTR  pBuffer,
    DWORD   Length,
    DWORD *const  pFlags
    )
{
    DWORD dwErr;
    DWORD dwFlags;
    TCHAR iBuffer[MAX_PATH] ;
    if (IsPowerManagerReady()) {
        __try {
            if (pBuffer!=NULL && Length!=0) {
                dwErr = PmGetSystemPowerState(iBuffer,MAX_PATH, &dwFlags);
                if (dwErr == ERROR_SUCCESS) {
                    if (StringCchCopy(pBuffer,min(Length,MAX_PATH),iBuffer) == STRSAFE_E_INSUFFICIENT_BUFFER) {
                        dwErr = ERROR_INSUFFICIENT_BUFFER;
                    }
                }
            }
            else {
                dwErr = PmGetSystemPowerState(NULL,0,&dwFlags);
            }
            if (pFlags) {
                *pFlags = dwFlags ;
            }
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
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    dwErr = EX_PM_SetSystemPowerState(pwsState,StateFlags,Options );
    CeSetDirectCall(fKernel);
    return dwErr;
}

DWORD
EX_PM_SetSystemPowerState(
    LPCWSTR pwsState,
    DWORD   StateFlags,
    DWORD   Options
    )
{
    DWORD dwErr;
    TCHAR iBuffer[MAX_PATH] ;
    if (pwsState!=NULL) {
        VERIFY(SUCCEEDED(StringCchCopy(iBuffer,MAX_PATH,pwsState)));
    }
    if (IsPowerManagerReady()) {
        __try {
            dwErr = PmSetSystemPowerState(pwsState!=NULL?iBuffer:NULL, StateFlags, Options);
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

DWORD
EX_PM_DevicePowerNotify(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState, 
    DWORD                   Flags
    )
{
    DWORD dwErr;
    if (IsPowerManagerReady()) {
        TCHAR iDevice[MAX_PATH];
        if (pvDevice)
            VERIFY(SUCCEEDED(StringCchCopy(iDevice,MAX_PATH,pvDevice)));
        __try {
            dwErr = PmDevicePowerNotify(pvDevice?iDevice:NULL, DeviceState, Flags);
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
EX_PM_RequestPowerNotifications(
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
HANDLE
PM_RequestPowerNotifications(
    HANDLE  hMsgQ,
    DWORD   Flags
    )
{   
    HANDLE h = NULL;
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    h = EX_PM_RequestPowerNotifications( hMsgQ, Flags );
    CeSetDirectCall(fKernel);
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

HANDLE
EX_PM_RegisterPowerRelationship
(
    PVOID pvParent, 
    PVOID pvChild, 
    PPOWER_CAPABILITIES pCaps,
    DWORD Flags
    )
{
    HANDLE h = NULL;

    if (IsPowerManagerReady()) {
        __try {
            POWER_CAPABILITIES powerCaps ;
            if (pCaps) {
                powerCaps = *pCaps;
            }
            h = PmRegisterPowerRelationship(pvParent, pvChild, pCaps!=NULL?&powerCaps:NULL, Flags);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            RETAILMSG(1,(TEXT("EX_PM_RegisterPowerRelationship: EXCEPTION:0x%x\n"), GetExceptionCode()));
            h = NULL;
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
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    dwErr = EX_PM_SetDevicePower(pvDevice,dwDeviceFlags,dwState );
    CeSetDirectCall(fKernel);
    return dwErr;
}
DWORD
EX_PM_SetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    CEDEVICE_POWER_STATE dwState
    )
{    
    DWORD dwErr;
    if (IsPowerManagerReady()) {
        TCHAR iDevice[MAX_PATH];
        if (pvDevice)
            VERIFY(SUCCEEDED(StringCchCopy(iDevice,MAX_PATH,pvDevice)));
        __try {
            dwErr = PmSetDevicePower(pvDevice?iDevice:NULL, 
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
    BOOL fKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
    dwErr = EX_PM_GetDevicePower(pvDevice,dwDeviceFlags,pdwState );
    CeSetDirectCall(fKernel);
    return dwErr;
}
DWORD
EX_PM_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags,
    PCEDEVICE_POWER_STATE pdwState    
    )
{    
    DWORD dwErr;

    if (IsPowerManagerReady()) {
        TCHAR iDevice[MAX_PATH];
        if (pvDevice)
            VERIFY(SUCCEEDED(StringCchCopy(iDevice,MAX_PATH,pvDevice)));
        __try {
            dwErr = PmGetDevicePower(pvDevice?iDevice:NULL, 
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

