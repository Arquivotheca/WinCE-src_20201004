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

Module Name:

    perfcomm.c

Abstract:

    CEPerf header file defining symbols used in comm OS tree.
    Performance Measurement Definitions.

Environment:

    Kernel mode driver

--*/


#ifdef CEPERF_ENABLE

#include <perfcomm.h>

#ifdef PERFCOMM_LIST

LPVOID pCePerfInternal;   // Instantiate required CePerf global
static HANDLE g_hCePerfSession = INVALID_HANDLE_VALUE;  // Handle to coredll session

CEPERF_BASIC_ITEM_DESCRIPTOR g_rgPerfItems[PERFCOMM_NUM_IDS] = {
    PERFCOMM_LIST
};

VOID PerfCommInit()
{
    CEPERF_SESSION_INFO info;
    int                 i; 

    RETAILMSG (1, (TEXT("PERFCOMM:: Registering session [%s] for [%d]  perf items.\r\n"),
        PERFCOMM_SESSION_NAME,
        PERFCOMM_NUM_IDS));

    for (i = 0 ; i < PERFCOMM_NUM_IDS ; i++)
    {
        RETAILMSG (1, (TEXT("PERFCOMM:: Perf item[%d] \t [%s]\r\n"),
        i,
        g_rgPerfItems[i].lpszItemName));
    }
        

    // BUGBUG should do CPU control in the shell or somewhere else instead of here

    CePerfControlCPU(CEPERF_CPU_ENABLE
                     | CEPERF_CPU_ICACHE_MISSES | CEPERF_CPU_ICACHE_HITS
                     | CEPERF_CPU_DCACHE_MISSES | CEPERF_CPU_DCACHE_HITS,
                     NULL, 0);

    // Session setup
    memset(&info, 0, sizeof(CEPERF_SESSION_INFO));
    info.wVersion = 1;
    info.dwStorageFlags = CEPERF_STORE_DEBUGOUT | CEPERF_STORE_TEXT;
    CePerfOpenSession(&g_hCePerfSession, PERFCOMM_SESSION_NAME,
              CEPERF_STATUS_RECORDING_ENABLED | CEPERF_STATUS_STORAGE_ENABLED | CEPERF_STATUS_NOT_THREAD_SAFE,
              &info);

    // Register Statistic and Duration items
    CePerfRegisterBulk(g_hCePerfSession, g_rgPerfItems, PERFCOMM_NUM_IDS, 0);
}

VOID PerfCommDeInit()
{
    // Write results to debugout
    CePerfFlushSession(g_hCePerfSession, NULL, CEPERF_FLUSH_DESCRIPTORS | CEPERF_FLUSH_DATA, 0);
    
    // Session cleanup (would also be cleaned up on process exit)
    CePerfDeregisterBulk(g_rgPerfItems, PERFCOMM_NUM_IDS);
    CePerfCloseSession(g_hCePerfSession);
}

#endif // PERFCOMM_LIST

#endif // CEPERF_ENABLE

