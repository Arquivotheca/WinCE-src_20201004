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

#pragma once

#include <windows.h>
#include <pm.h>
#include <nkintr.h>
#include <pkfuncs.h>
#include <devload.h>
#include <msgqueue.h>
#include <pmpolicy.h>
#include "pwrtstutil.h"
#include "pwrdrvr.h"

#define PM_PWRTESTLIB

#define MAX_STATE_NAMELEN               64
#define MAX_POWER_STATES                32
#define MAX_DEV_NAME_LEN 6  //Max. length of device legacy name

#define PWRMGR_STATE_KEY                PWRMGR_REG_KEY _T("\\State")
#define PWRMGR_TIMEOUTS_KEY             PWRMGR_REG_KEY _T("\\Timeouts")
#define PWRMGR_ACTIVITY_KEY             PWRMGR_REG_KEY _T("\\ActivityTimers")

#define USER_ACTIVITY                   _T("UserActivity")
#define USER_ACTIVITY_RESET_EVT         _T("PowerManager/ActivityTimer/") USER_ACTIVITY
#define USER_ACTIVITY_ACTIVE_EVT        _T("PowerManager/") USER_ACTIVITY _T("_Active")
#define USER_ACTIVITY_INACTIVE_EVT      _T("PowerManager/") USER_ACTIVITY _T("_Inactive")

#define SYSTEM_ACTIVITY                  _T("SystemActivity")
#define SYSTEM_ACTIVITY_RESET_EVT       _T("PowerManager/ActivityTimer/") SYSTEM_ACTIVITY
#define SYSTEM_ACTIVITY_ACTIVE_EVT      _T("PowerManager/") SYSTEM_ACTIVITY _T("_Active")
#define SYSTEM_ACTIVITY_INACTIVE_EVT    _T("PowerManager/") SYSTEM_ACTIVITY _T("_Inactive")

#define ACTIVITY_TIMER_RELOAD_EVT       _T("PowerManager/ReloadActivityTimeouts")

//Power State Names 
#define POWER_STATE_NAME_ON             _T("on")
#define POWER_STATE_NAME_SUSPEND        _T("suspend")
#define POWER_STATE_NAME_SCREENOFF      _T("screenoff")
#define POWER_STATE_NAME_USERIDLE       _T("useridle")
#define POWER_STATE_NAME_SYSTEMIDLE     _T("systemidle")
#define POWER_STATE_NAME_RESUMING       _T("resuming")
#define POWER_STATE_NAME_REBOOT         _T("reboot")
#define POWER_STATE_NAME_COLDREBOOT     _T("coldreboot")
#define POWER_STATE_NAME_BACKLIGHTOFF   _T("backlightoff")
#define POWER_STATE_NAME_UNATTENDED     _T("unattended")

#define SET_PWR_TESTNAME(x)  LPCTSTR pszPwrTest = (x)

typedef struct _IDLE_TIMEOUTS
{
        DWORD UserIdle;
            DWORD SystemBackLight;
            DWORD Suspend;
} IDLE_TIMEOUTS,*PIDLE_TIMEOUTS;


#define PWR_MSG_WAIT_TIME    5000
#define QUEUE_MAX_NAMELEN    200 



#define DEV_MSG_WAIT_TIME    5000
#define PNP_MSG_ENTRIES      20
#define PNP_MSG_NAMELEN      128
#define PNP_MSG_SIZE         (PNP_MSG_ENTRIES * (sizeof(DEVDETAIL) + (PNP_MSG_NAMELEN * sizeof(TCHAR))))

typedef struct _pmanaged_device_state
{
    WCHAR LegacyName[6];
    GUID guid ;
    POWER_CAPABILITIES pwrCaps;
    struct _pmanaged_device_state* next ; 
}PMANAGED_DEVICE_STATE,*PPMANAGED_DEVICE_STATE;


typedef struct _pmanaged_device_list
{
    GUID guid ;
    PPMANAGED_DEVICE_STATE pDeviceState;
    struct _pmanaged_device_list * next;
 }PMANAGED_DEVICE_LIST ,*PPMANAGED_DEVICE_LIST;

#define QUEUE_ENTRIES           3
#define QUEUE_MAX_MSG_SIZE      (sizeof(POWER_BCAST_MSG))
#define QUEUE_SIZE              (QUEUE_ENTRIES*QUEUE_MAX_MSG_SIZE)

BOOL DisableSystemSuspend(BOOL fDisable);
CEDEVICE_POWER_STATE RemapDx(CEDEVICE_POWER_STATE cps, UCHAR DeviceDx);
DWORD RemapSystemStateFlag(DWORD dwRequested, __out_ecount(cName) LPTSTR pszName, UINT cName);
LPTSTR StateFlag2String(DWORD dwStateFlag, __out_ecount(cStateFlag) LPTSTR pszStateFlag, UINT cStateFlag);

BOOL PMActivelyManagesPower(void);

BOOL GetIdleTimeouts(PIDLE_TIMEOUTS pIT,BOOL fPda);
BOOL SetIdleTimeouts(PIDLE_TIMEOUTS pIT,BOOL fPda);
VOID PrintIdleTimeouts(PIDLE_TIMEOUTS pIT,BOOL fPda);
BOOL SuspendTimeoutDisable(BOOL fPda ,DWORD dwTimeout ,BOOL fEnable );

BOOL SimulatePowerPolicy ( DWORD dwMsg);
BOOL IsPowerDeviceParent(PPMANAGED_DEVICE_STATE pDevicestate);

BOOL CheckCurrentPowerState(LPCTSTR pszState);
BOOL IsPowerStateSettable( LPTSTR  pszState);
BOOL WaitForUserActivity(DWORD dwTimeout);
BOOL WaitForUserInactivity(DWORD dwTimeout);
BOOL WaitForSystemActivity(DWORD dwTimeout);
BOOL WaitForSystemInactivity(DWORD dwTimeout);
BOOL AddWakesourceToActivityTimer(DWORD dwInterrupt, LPCTSTR pszActivityTimer);
BOOL RemoveWakesourceFromActivityTimer(DWORD dwInterrupt, LPCTSTR pszActivityTimer);
BOOL ActivityTimerReload(VOID);
void SetAutoWakeSource(DWORD dwWakeSource);
DWORD Util_SetSystemPowerState(LPCWSTR psState, DWORD StateFlags, DWORD Options);
BOOL  SuspendResumeSystem(LPCWSTR pwsSystemState , DWORD dwFlags ,DWORD dwOptions);
BOOL SetupRTCWakeupResource(WORD wSeconds);
VOID ReleaseRTCWakeupResource();
BOOL ConvertStringToGuid (LPCTSTR pszGuid, GUID *pGuid);
BOOL EnumPoweManagedDevices(VOID);
VOID FreeManagedDevices();
