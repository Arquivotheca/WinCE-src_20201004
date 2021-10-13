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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#ifndef __CPUMON_H__
#define __CPUMON_H__

#include <windows.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

//Refresh rate of 2s (2000ms) is standard
#define STANDARD_REFRESH_RATE 2000

//Calibration time fo 2s (2000ms)
#define STANDARD_CALIBRATION_TIME 2000

//Name of shared memory
#define CPUMON_SHARED_MEM_NAME _T("CPU_Monitor_Shared_Memory")

//Size of shared memoy block
#define CPUMON_SHARED_MEM_SIZE 256

//Public APIs	
HRESULT CalibrateCPUMon();
HRESULT StartCPUMonitor();
HRESULT StopCPUMonitor();
float GetCurrentCPUUtilization();
	
//Private functions
void UnitOfWork();
long RestartIdleCount();
DWORD WINAPI UpdateMeasurementsThreadProc(void *arg);
DWORD WINAPI IdleWorkerThreadProc(void *arg);

#define CETKPERF_RUNNING_MUTEX_GUID _T("{AD14FE07-7549-4063-A082-F9BDCE7FBC74}")
#define CETKPERF_KITL_SERVICE_NAME_A    "cetkperf"

#define CETKPERF_KITL_WINDOW_SIZE 8 // same as EDBG_WINDOW_SIZE
#define CETKPERF_KITL_BUFFER_POOL_SIZE  (CETKPERF_KITL_WINDOW_SIZE*2*1520) // by amazing coincidence, same as EDBG_DFLT_BUFFER_POOL_SIZE

#ifdef __cplusplus
};
#endif /* __cplusplus */


#endif // __CPUMON_H__

