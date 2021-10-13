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


#include "sysmon.h"


#define UNIT_OF_WORK_ITER       40 
#define CE_PRIO_UNDER_WATCHDOG  10  
static SYS_MON  g_SysMon = {0};

// CPU Monitor algorithm courtesy of EvanG from the wmxuty object



// --------------------------------------------------------------------
static VOID DoUnitOfWork(LONG *pIdleCounter)
// --------------------------------------------------------------------
{
    volatile DWORD i;

    for (i=0; i < UNIT_OF_WORK_ITER; i++);
    
    InterlockedIncrement(pIdleCounter);
}

// --------------------------------------------------------------------
static DWORD WINAPI IdleWorkerThreadProc(LPVOID pData)
// --------------------------------------------------------------------
{
    PSYS_MON pSysMon = (PSYS_MON)pData;

    // this is a very low prio thread
    CeSetThreadPriority(GetCurrentThread(), CE_THREAD_PRIO_256_IDLE);

    while(pSysMon->fCPUThread) {
        DoUnitOfWork((LONG *)&pSysMon->cCPUIdle);
    }

    ExitThread(0);

    return 0;
}


// --------------------------------------------------------------------
static DWORD WINAPI CalibrationWorkerThreadProc(LPVOID pData)
// --------------------------------------------------------------------
{
    PSYS_MON pSysMon = (PSYS_MON)pData;

    // this is a very high prio thread to get maximum accuracy on units of work count.
    CeSetThreadPriority(GetCurrentThread(), CE_PRIO_UNDER_WATCHDOG);

    while(pSysMon->fCPUThread)
        DoUnitOfWork((LONG *)&pSysMon->cCPUIdle);

    ExitThread(0);

    return 0;
}

// --------------------------------------------------------------------
DWORD CalibrateCPUMon(PSYS_MON pSysMon,DWORD dwCalibrateTimeMS)
// --------------------------------------------------------------------
{
    DWORD origPrio;
    LONG lCalibMax;
    HANDLE hCalibThread;

    // store old priority, bump to time critical
    // THE CPU MUST BE AT IDLE DURING CALIBRATION IN ORDER TO GET
    // RELIABLE CALIBRATION RESULTS
    hCalibThread = GetCurrentThread();
    origPrio = GetThreadPriority(hCalibThread);
    CeSetThreadPriority(hCalibThread, CE_PRIO_UNDER_WATCHDOG - 1); // As the control thread this needs to be just higher than the calibration thread in priority

    pSysMon->cCPUIdle=0;
    pSysMon->fCPUThread=TRUE;
    HANDLE hIdleThread = CreateThread(NULL, 0, CalibrationWorkerThreadProc, (void *)pSysMon, 0, NULL);
    CeSetThreadQuantum(hIdleThread, 0); // the calibration thread needs to run uninterrupted by any other thread that might have the same priority
    Sleep(dwCalibrateTimeMS);
    lCalibMax=pSysMon->cCPUIdle;
    pSysMon->fCPUThread=FALSE;
    SetThreadPriority(hCalibThread, origPrio);

    WaitForSingleObject(hIdleThread, INFINITE);
    CloseHandle(hIdleThread);

    // For verifying load readings on different CPU's.  
    RETAILMSG(0,(TEXT("Calibration data for %d ms, %d iterations, %d iterations per ms\r\n"), dwCalibrateTimeMS, lCalibMax, lCalibMax / dwCalibrateTimeMS));
    // return normalized calibration value
    return (DWORD)(lCalibMax/dwCalibrateTimeMS);
}

// --------------------------------------------------------------------
float USBPerf_MarkMem()
// --------------------------------------------------------------------
{
    MEMORYSTATUS memStat;
    
    GlobalMemoryStatus(&memStat);
    g_SysMon.memPercentUsed = 100 - (float)memStat.dwAvailPhys / (float)memStat.dwTotalPhys * 100;

    return g_SysMon.memPercentUsed;
}

// --------------------------------------------------------------------
float USBPerf_MarkCPU()
// --------------------------------------------------------------------
{
    LONG newCPUIdle = g_SysMon.cCPUIdle;
    DWORD dwElapsedMs = GetTickCount() - g_SysMon.dwStartTime;

    g_SysMon.cpuPercentUsed = 100 - (100 * ((float)(newCPUIdle) / ((float)g_SysMon.dwCalibMax * (float)dwElapsedMs)));

    // For verifying load readings on different CPU's.  
    RETAILMSG(0,(TEXT("SWT PCT %d, IDLE %d MS %d\r\n"), (DWORD)(g_SysMon.cpuPercentUsed), newCPUIdle, dwElapsedMs));

    return g_SysMon.cpuPercentUsed;
}

// --------------------------------------------------------------------
void USBPerf_ResetAvgs()
// --------------------------------------------------------------------
{
    RETAILMSG(0,(TEXT("Resetting Averages\r\n")));
    // forces averages to be reset
    g_SysMon.cCPUIdle = 0;
    g_SysMon.dwStartTime = GetTickCount();
}

// --------------------------------------------------------------------
VOID USBPerf_StartSysMonitor(DWORD dwPollIntervalMS, DWORD dwCalibTimeMS)
// --------------------------------------------------------------------
{

    if(dwCalibTimeMS == 0 || dwPollIntervalMS == 0)
        return;

    PSYS_MON pSysMon = &g_SysMon;

    // stop if already running
    if(pSysMon->fCPUThread)
        USBPerf_StopSysMonitor();

    
    pSysMon->cCPUIdle = 0;
    if(pSysMon->fCalibrated == FALSE){
        pSysMon->dwCalibMax = CalibrateCPUMon(pSysMon,dwCalibTimeMS);
        pSysMon->fCalibrated = TRUE;
    }

    pSysMon->dwStartTime = GetTickCount();
    pSysMon->cCPUIdle = 0;
    pSysMon->fCPUThread = TRUE;
    pSysMon->hCPUThread = CreateThread(NULL, 0, IdleWorkerThreadProc, (void *)pSysMon, 0, NULL);
    CeSetThreadQuantum(pSysMon->hCPUThread, 0); // the idle thread needs to run uninterrupted by any other thread that might have the same priority

}

// --------------------------------------------------------------------
VOID USBPerf_StopSysMonitor(VOID)
// --------------------------------------------------------------------
{
    PSYS_MON pSysMon = &g_SysMon;

    // nothing to stop if cpu thread is not running
    if(pSysMon->fCPUThread)
    {
        // signal cpu watcher thread to stop
        pSysMon->fCPUThread = FALSE;
        WaitForSingleObject(pSysMon->hCPUThread, INFINITE);
        CloseHandle(pSysMon->hCPUThread);
        memset((void *)pSysMon, 0, sizeof(SYS_MON));
    }
}
