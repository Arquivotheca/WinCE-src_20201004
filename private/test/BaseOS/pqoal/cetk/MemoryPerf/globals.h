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

// ***************************************************************************************************************
// A global struct is used for convinience to keep overall state and configuration information
// ***************************************************************************************************************

#pragma once

#include <windows.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <clparse.h>   
#include <MDPGPerf.h>
#include "PerfScenario.h"

//------------------------------------------------------------------------------
// A single global to share info between initialization, allocation, and the individual tests
//------------------------------------------------------------------------------
typedef struct {
    // pointers to buffers
    DWORD * pdwBase;
    DWORD * pdwTop;
    DWORD * pdw1MBBase;
    DWORD * pdw1MBTop;
    DWORD * pdwUncached;
    DWORD * pdwUncachedTop;
    // Remember which main tests have been run 
    bool bCachedMemoryTested;
    bool bUncachedMemoryTested;
    bool bDDrawMemoryTested;
    bool bReserved1;
    // cache info 
    int iL1LineSize;
    int iL1SizeBytes;
    int iL2LineSize;
    int iL2SizeBytes;
    CacheInfo tThisCacheInfo;
    bool bThisCacheInfo;
    bool bL1WriteAllocateOnMiss;
    bool bL2WriteAllocateOnMiss;
    bool bReserved2;
    int  iL1ReplacementPolicy;
    // CPU info
    int  iL2ReplacementPolicy;
    DWORD dwCPUMHz;
    int iLastCPULatency;
    DWORD dwCPULatencyMin;
    // Memory Performance Measurements
    int iTLBSize;
    DWORD dwPWLatency;
    DWORD dwL1Latency;
    DWORD dwL2Latency;
    DWORD dwMBLatency;
    DWORD dwSeqLatency;
    DWORD dwRASExtraLatency;
    DWORD dwL1WriteLatency;
    DWORD dwL2WriteLatency;
    DWORD dwMBWriteLatency;
    DWORD dwSeqWriteLatency;
    DWORD dwUCMBLatency;
    DWORD dwUCMBWriteLatency;
    DWORD dwDSMBLatency;
    DWORD dwDSMBWriteLatency;
    DWORD dwMBMinLatency;
    DWORD dwMBMaxLatency;
    DWORD dwMBMinWriteLatency;
    DWORD dwMBMaxWriteLatency;
    // command line options
    bool bSectionMapTest;
    bool bFillFirst;
    bool bUncachedTest;
    bool bVideoTest;
    bool bChartDetails;
    bool bChartOverview;
    bool bChartOverviewDetails;
    bool bWarmUpCPU;
    bool bWarnings;
    bool bConsole2;
    bool bExtraPrints;
    bool bNoL2;
    int giCEPriorityLevel;
    int giRescheduleSleep;
    // utility info
    int gbInitialized;  
    int giUseQPC;
    DWORD gdwCountFreqChanges;
    LARGE_INTEGER gliFreq;  
    LARGE_INTEGER gliOrigFreq;  
    double gdTickScale2us;
    double gdTotalTestLoopUS;
    int iSavedClockRate;
} tGlobal;

// --------------------------------------------------------------------
// Externs
//---------------------------------------------------------------------
extern tGlobal gtGlobal;
extern CKato*                            g_pKato;
extern SPS_SHELL_INFO*                   g_pShellInfo;
extern HINSTANCE  g_hInstance;

//-----------------------------------------------------------------------
//Global functions
//-----------------------------------------------------------------------
BOOL PerfReport(LPCTSTR pszPerfMarkerName, DWORD value);
BOOL InitPerfLog();

