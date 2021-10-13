//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
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
#include "pwrtestlib.h"

// Test Power Driver class
//

#define SZ_REG_KEY_FMT      L"Drivers\\BuiltIn\\PDXTEST%08X"
#define SZ_CONFIG_FMT       L"PDXCFG%08X"
#define SZ_EVCHANGE_FMT     L"PDXEVCHANGE%08X"
#define SZ_EVREQUEST_FMT    L"PDXEVREQUEST%08X"
#define SZ_STATE_FMT        L"%s_STATE_D%d"

#define DEF_NOTIF_TIME      1000 // 1 seconds
#define USER_ACTIVITY_TIME  60000 // 1 minute

// Struct that contains the details of each power state that is supported
typedef struct _POWER_STATE
{
    TCHAR   szStateName[MAX_PATH];
    DWORD dwDefault;
    DWORD dwFlags;
    BOOL     bSettable ; //Tells if the Power State can be Set by PM API 
} POWER_STATE, *PPOWER_STATE;

class CPDX 
{
public:
    CPDX();
    ~CPDX();

    BOOL StartTestDriver();
    BOOL StartTestDriver(UCHAR DeviceDx);
    BOOL StopTestDriver();
    BOOL StopDriver();


    BOOL SetDeviceCeiling(CEDEVICE_POWER_STATE cps);
    BOOL WaitForPowerChange(DWORD dwTimeout, CEDEVICE_POWER_STATE *pcps = NULL);
    CEDEVICE_POWER_STATE GetCurrentPowerState();
    BOOL SetDesiredPowerState(CEDEVICE_POWER_STATE cps);
    LPTSTR GetDeviceName(__out_ecount(uLen) LPTSTR pszDevName, DWORD uLen);
    LPTSTR GetPowerStateName(CEDEVICE_POWER_STATE cps, __out_ecount(uLen) LPTSTR pszDevName, DWORD uLen);

private:

    DWORD m_key;
    HANDLE m_hEvKillActivityThread;
    HANDLE m_hEvChange;
    HANDLE m_hEvRequest;
    HANDLE m_hCfgMap;
    HANDLE m_hDevice;
    PPOWER_DRIVER_CONFIG m_pConfig;
    TCHAR m_szRegKey[MAX_PATH];
    TCHAR m_szCfgName[MAX_MAPNAME_LEN];
    TCHAR m_szStateNames[PwrDeviceMaximum][MAX_STATE_NAMEL];

    BOOL CreateDriverState(CEDEVICE_POWER_STATE cps);
    BOOL DeleteDriverState(CEDEVICE_POWER_STATE cps);
    BOOL CreateDriverConfig(UCHAR DeviceDx);
    BOOL DeleteDriverConfig();
};

// Captures the details of supported power states obtained from 
// the Power Manager registry key
class CSystemPowerStates
{
public:
    CSystemPowerStates();
    ~CSystemPowerStates();

    PPOWER_STATE GetPowerState(UINT index);
    UINT GetPowerStateCount(VOID);

private:
    UINT m_count;
    POWER_STATE m_powerStateList[MAX_POWER_STATES];
};

class CPowerManagedDevices
{

  public: 
   CPowerManagedDevices();
   ~CPowerManagedDevices();
   PPMANAGED_DEVICE_STATE GetNextPMDevice();
   VOID ResetDeviceList();

 private:
    PPMANAGED_DEVICE_LIST m_pDeviceList;
    PPMANAGED_DEVICE_LIST m_pCurrentList;
       PPMANAGED_DEVICE_STATE m_pCurrentDevice;

};
