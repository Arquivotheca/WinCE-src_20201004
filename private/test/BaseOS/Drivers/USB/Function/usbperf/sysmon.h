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
/*++

    DISCLAIMER: The original writer is: Andrew Rogers

--*/


#pragma once

#include <windows.h>

// system monitor data structure
typedef volatile struct _SYS_MON
{
    LONG cCPUIdle;
    DWORD dwRefreshRate;
    DWORD dwCalibMax;
    DWORD dwStartTime;
    DWORD sysMonFlags;
    BOOL fCPUThread;
    BOOL fCalibrated;
    FLOAT cpuPercentUsed;
    FLOAT memPercentUsed;
    HANDLE hCPUThread;

} SYS_MON, *PSYS_MON;

float USBPerf_MarkMem();
float USBPerf_MarkCPU();
void USBPerf_ResetAvgs();
VOID USBPerf_StartSysMonitor(DWORD dwPollIntervalMS, DWORD dwCalibTimeMS);
VOID USBPerf_StopSysMonitor();





