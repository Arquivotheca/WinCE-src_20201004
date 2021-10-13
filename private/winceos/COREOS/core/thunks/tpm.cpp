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
#include <extfile.h>
#include <pm.h>
#include <windev.h>

/* Note that Power Manager may not be loaded, in which case we return ERROR_SERVICE_NOT_ACTIVE */
#define IsPowerManagerAPIReady() (WAIT_OBJECT_0 == WaitForAPIReady(SH_DEVMGR_APIS, 0))

extern "C"
DWORD
xxx_GetSystemPowerState(
    LPWSTR  pBuffer,
    DWORD   Length,
    PDWORD  pFlags
    )
{
    if (!IsPowerManagerAPIReady())
        return ERROR_SERVICE_NOT_ACTIVE;
    
    return GetSystemPowerState(pBuffer, Length, pFlags);
}


extern "C"
DWORD
xxx_SetSystemPowerState(
    LPCWSTR pwsState,
    DWORD   StateFlags,
    DWORD   Options
    )
{
    if (!IsPowerManagerAPIReady()) {
        //
        // if the Power Manager is not running but an app tries to suspend the system
        //
        if (!pwsState &&
            (POWER_STATE_SUSPEND & POWER_STATE(StateFlags)))
        {
            KernelIoControl(IOCTL_HAL_PRESUSPEND, NULL, 0, NULL, 0, NULL);
            FileSystemPowerFunction(FSNOTIFY_POWER_OFF);
            PowerOffSystem();
            Sleep(0);
            FileSystemPowerFunction(FSNOTIFY_POWER_ON);
            return ERROR_SUCCESS;
        } else
            return ERROR_SERVICE_NOT_ACTIVE;
    } else {
        return SetSystemPowerState(pwsState, StateFlags, Options);
    }
}


extern "C"
HANDLE
xxx_SetPowerRequirement(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState,
    ULONG                   DeviceFlags,
    PVOID                   pvSystemState,
    ULONG                   StateFlags    
    )
{
    if (!IsPowerManagerAPIReady()) {
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return NULL;
    }

    return SetPowerRequirement( pvDevice,
                                DeviceState,
                                DeviceFlags,
                                pvSystemState,
                                StateFlags);
}


extern "C"
DWORD
xxx_ReleasePowerRequirement(
    HANDLE hPowerReq
    ) 
{
    if (!IsPowerManagerAPIReady())
        return ERROR_SERVICE_NOT_ACTIVE;

    return ReleasePowerRequirement(hPowerReq);
}


extern "C"
HANDLE
xxx_RequestPowerNotifications(
    HANDLE  hMsgQ,
    DWORD   Flags
    )
{
    if (!IsPowerManagerAPIReady()) {
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return NULL;
    }

    return RequestPowerNotifications(hMsgQ, Flags);
}


extern "C"
DWORD
xxx_StopPowerNotifications(
    HANDLE h
    )
{
    if (!IsPowerManagerAPIReady()) {
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    return StopPowerNotifications(h);
}


extern "C"
DWORD
xxx_DevicePowerNotify(
    PVOID                   pvDevice,
    CEDEVICE_POWER_STATE    DeviceState, 
    DWORD                   Flags
    )
{
    if (!IsPowerManagerAPIReady())
        return ERROR_SERVICE_NOT_ACTIVE;

    return DevicePowerNotify(pvDevice, DeviceState, Flags);
}


extern "C"
HANDLE
xxx_RegisterPowerRelationship(
    PVOID pvParent, 
    PVOID pvChild,
    PPOWER_CAPABILITIES pCaps,    
    DWORD Flags
    )
{
    if (!IsPowerManagerAPIReady())
        return NULL;

    return RegisterPowerRelationship(pvParent, pvChild, pCaps, Flags);
}


extern "C"
DWORD
xxx_ReleasePowerRelationship(
    HANDLE h
    )
{
    if (!IsPowerManagerAPIReady()) {
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    return ReleasePowerRelationship(h);
}

extern "C"
DWORD
xxx_SetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    CEDEVICE_POWER_STATE dwState
    )
{
    if (!IsPowerManagerAPIReady()) {
        return ERROR_SERVICE_NOT_ACTIVE;
    }
    
    return SetDevicePower(pvDevice, dwDeviceFlags, dwState);
}

extern "C"
DWORD
xxx_GetDevicePower(
    PVOID pvDevice, 
    DWORD dwDeviceFlags, 
    PCEDEVICE_POWER_STATE pdwState
    )
{
    if (!IsPowerManagerAPIReady()) {
        return ERROR_SERVICE_NOT_ACTIVE;
    }
    
    return GetDevicePower(pvDevice, dwDeviceFlags, pdwState);
}

// This routine notifies the power policy manager of events it needs in order to
// implement the OEM's power policy.  The dwMessage parameter is one of the PPN_
// values (or an OEM-defined one).  The dwData parameter is a 32-bit value whose
// interpretation varies with the notification message.  A return value of TRUE
// indicates success.
extern "C"
BOOL
PowerPolicyNotify(DWORD dwMessage, DWORD dwData)
{
    static HANDLE hq = NULL;
    BOOL fOk = FALSE;
    struct PowerPolicyMessage_tag {
        DWORD       dwMessage;      // dwNotification from PowerPolicyNotify()
        DWORD       dwData;         // dwNotifyData from PowerPolicyNotify()
        HANDLE      hOwnerProcess;  // informational, filled in by caller process
    } ppm;
    LPCTSTR PMPOLICY_NOTIFICATION_QUEUE = _T("PowerManager/NotificationQueue");

    if (!IsPowerManagerAPIReady()) {
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return FALSE;
    }
    
    // Make sure the message queue is created.  Note all PM implementations
    // will support this message queue, in which case we return an error.
    if(hq == NULL) {
        MSGQUEUEOPTIONS mqo;

        // set up queue characteristics
        memset(&mqo, 0, sizeof(mqo));
        mqo.dwSize = sizeof(mqo);
        mqo.dwFlags = 0;
        mqo.dwMaxMessages = 10;
        mqo.cbMaxMessage = sizeof(mqo);
        mqo.bReadAccess = FALSE;

        // create the queue
        hq = CreateMsgQueue(PMPOLICY_NOTIFICATION_QUEUE, &mqo);
        if(GetLastError() != ERROR_ALREADY_EXISTS) {
            CloseMsgQueue(hq);
            hq = NULL;
            SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        }
    }

    // send the message
    if(hq != NULL) {
        // package the data
        ppm.dwMessage = dwMessage;
        ppm.dwData = dwData;
        ppm.hOwnerProcess = GetOwnerProcess();

        // send the message without blocking -- we don't want to hang
        // applications if the PM has a backlog of messages
        fOk = WriteMsgQueue(hq, &ppm, sizeof(ppm), 0, 0);        
    }

    return fOk;
}

