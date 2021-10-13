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
#include <tchar.h>
#include <devload.h>
#include <winioctl.h>
#include <pm.h>

// Reg key name which contains the name of the memory mapped region which 
// points to POWER_DRIVER_CONFIG to use for this instance. The contents of 
// this region will be a POWER_DRIVER_CONFIG structure, allowing an 
// application to map the same region and reconfigure the driver.
//   
#define POWER_DRIVER_CONFIG_LOC     _T("ConfigLoc")
#define MAX_MAPNAME_LEN             32
#define MAX_EVNAME_LEN              32
#define MAX_DEVNAME_LEN             16

#define DRV_PREFIX                  _T("PDX")
#define DRV_NAME                    _T("PWRDRVR.DLL")
#define PDX_POWER_UP_EVENT_NAME     _T("PDX_POWER_UP_EVENT")

// The contents of this configuration structure determines the power
// handling behavior of the device driver.
//
typedef struct _POWER_DRIVER_CONFIG
{
    // Set this event, and the driver will request a power change to
    // the value in dxDesired.
    //
    TCHAR                   szEvRequest[MAX_EVNAME_LEN]; 

    // this event will be set by the driver to notify that a change has occurred
    //
    TCHAR                   szEvChange[MAX_EVNAME_LEN];
    
    // test application supplied settings
    //
    CRITICAL_SECTION        csDevice;
    POWER_CAPABILITIES      pwrCap;
    CEDEVICE_POWER_STATE    dxCurrent;
    CEDEVICE_POWER_STATE    dxDesired;

    // driver supplied settings
    //
    TCHAR                   szDeviceName[MAX_DEVNAME_LEN];

} POWER_DRIVER_CONFIG, *PPOWER_DRIVER_CONFIG;

// lock functions for the structure
//
#define LOCK(pd)            EnterCriticalSection(&(pd->pConfig->csDevice))
#define UNLOCK(pd)          LeaveCriticalSection(&(pd->pConfig->csDevice))





