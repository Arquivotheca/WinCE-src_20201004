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


#define UNIT_OF_WORK_ITER       400 // value used in wmxuty

static SYS_MON  g_SysMon = {0};

// CPU Monitor algorithm courtesy of EvanG from the wmxuty object

// --------------------------------------------------------------------
static VOID DoUnitOfWork(LONG *pIdleCounter)
// --------------------------------------------------------------------
{
    int counter = 0;    
    
    for(int i = 0; i < UNIT_OF_WORK_ITER; i++)
        counter += i;

    InterlockedIncrement(pIdleCounter);
}

// --------------------------------------------------------------------
static DWORD WINAPI IdleWorkerThreadProc(LPVOID pData)
// --------------------------------------------------------------------
{
    PSYS_MON pSysMon = (PSYS_MON)pData;
    DWORD count = 0;

    // this is a very low prio thread
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);

    while(pSysMon->fCPUThread)
        DoUnitOfWork(&pSysMon->cCPUIdle);
    
    ExitThread(0);
    
    return 0;
}


// --------------------------------------------------------------------
static DWORD WINAPI CalibrationWorkerThreadProc(LPVOID pData)
// --------------------------------------------------------------------
{
    PSYS_MON pSysMon = (PSYS_MON)pData;
    DWORD count = 0;

    // this is a very low prio thread
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    while(pSysMon->fCPUThread)
        DoUnitOfWork(&pSysMon->cCPUIdle);
    
    ExitThread(0);
    
    return 0;
}



// --------------------------------------------------------------------
DWORD WINAPI SysWatcherThreadProc(LPVOID pData)
// --------------------------------------------------------------------
{
    PSYS_MON pSysMon = &g_SysMon;
    MEMORYSTATUS memStat;
    DWORD newCPUIdle = 0;

    // create an idle worker thread
    HANDLE hIdleThread = CreateThread(NULL, 0, IdleWorkerThreadProc, pSysMon, 0, NULL);
    
    // high prio thread
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    
    // terminate when sysMon.fCPUThread is signalled
    while(pSysMon->fCPUThread)
    {
        pSysMon->cCPUIdle = 0;
        Sleep(pSysMon->dwRefreshRate);
        newCPUIdle = pSysMon->cCPUIdle;
        // if we got an idle count value that is higher than the previous calibration
        // then use it for the new calibration value
        if(newCPUIdle > pSysMon->dwCalibMax){
            pSysMon->dwCalibMax = newCPUIdle;
        }
        // save cpu utilization % and reset the idle cpu counter
        pSysMon->cpuPercentUsed = 100 - (100 * ((float)(newCPUIdle) / (float)pSysMon->dwCalibMax));
        GlobalMemoryStatus(&memStat);
        pSysMon->memPercentUsed = 100 - (float)memStat.dwAvailPhys / (float)memStat.dwTotalPhys * 100;
    }

    WaitForSingleObject(hIdleThread, INFINITE);
    CloseHandle(hIdleThread);

    return 0;
}

// --------------------------------------------------------------------
DWORD CalibrateCPUMon(PSYS_MON pSysMon,DWORD dwCalibrateTimeMS, DWORD dwRefreshRateMS)
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
    SetThreadPriority(hCalibThread, THREAD_PRIORITY_TIME_CRITICAL);

    pSysMon->cCPUIdle=0;
    pSysMon->fCPUThread=TRUE;
    HANDLE hIdleThread = CreateThread(NULL, 0, CalibrationWorkerThreadProc, pSysMon, 0, NULL);
    Sleep(dwCalibrateTimeMS);
    lCalibMax=pSysMon->cCPUIdle;
    pSysMon->fCPUThread=FALSE;
    SetThreadPriority(hCalibThread, origPrio);

    WaitForSingleObject(hIdleThread, INFINITE);
    CloseHandle(hIdleThread);


    // return normalized calibration value
    return (int)(lCalibMax*(((float)dwRefreshRateMS)/dwCalibrateTimeMS));
}

// --------------------------------------------------------------------
float USBPerf_MarkMem()
// --------------------------------------------------------------------
{
    return g_SysMon.memPercentUsed;
}

// --------------------------------------------------------------------
float USBPerf_MarkCPU()
// --------------------------------------------------------------------
{
    return g_SysMon.cpuPercentUsed;
}

// --------------------------------------------------------------------
VOID USBPerf_StartSysMonitor(DWORD dwPollIntervalMS, DWORD dwCalibTimeMS)
// --------------------------------------------------------------------
{

    if(dwPollIntervalMS == 0 || dwCalibTimeMS == 0)
        return;
        
    PSYS_MON pSysMon = &g_SysMon;

    // stop if already running
    if(pSysMon->fCPUThread)
        USBPerf_StopSysMonitor();

    pSysMon->cCPUIdle = 0;
    pSysMon->dwRefreshRate = dwPollIntervalMS;
    if(pSysMon->fCalibrated == FALSE){
        pSysMon->dwCalibMax = CalibrateCPUMon(pSysMon,dwCalibTimeMS, dwPollIntervalMS);
        pSysMon->fCalibrated = TRUE;
    }

    pSysMon->fCPUThread = TRUE;
    pSysMon->hCPUThread = CreateThread(NULL, 0, SysWatcherThreadProc, NULL, 0, NULL);

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
        memset(pSysMon, 0, sizeof(SYS_MON));
    }
}
