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
#ifndef __KSMPLIB_H__
#define __KSMPLIB_H__


#define KSMP_P1_EXE_NAME      _T("KSMP_P1.EXE")
#define KSMP_P1_EVENT_NAME    _T("KSMP_P1_EVT")
#define KSMP_P1_MSGQ_NAME     _T("KSMP_P1_MSGQ")

#define KSMP_P2_EXE_NAME      _T("KSMP_P2.EXE")
#define KSMP_P2_EVENT_NAME    _T("KSMP_P2_EVT")

// For SMP_DEPTH_SYNC_1
#define KSMP_SYNC1_EXE_NAME   _T("KSMP_SYNC1.EXE")
#define KSMP_SYNC1_EVT_PREFIX _T("KSMP_SYNC1_EVT")
#define KSMP_SYNC1_NUM_OF_PROCESS          8
#define KSMP_SYNC1_THREADS_PER_PROCESS     8

// For SMP_DEPTH_SCHL_1
#define KSMP_SCHL1_P1_EXE_NAME      _T("KSMP_SCHL1_P1.EXE")
#define KSMP_SCHL1_P2_EXE_NAME      _T("KSMP_SCHL1_P2.EXE")
#define KSMP_SCHL1_MSGQ_NAME        _T("KSMP_SCHL1_MSGQ")
#define KSMP_SCHL1_EVT_NAME         _T("KSMP_SCHL1_EVT")


#define NKD   NKDbgPrintfW


//Some constants
const DWORD FIVE_SECONDS_TIMEOUT = 5000; //in ms


const DWORD THREAD_PRIORITY_LOW =         10;
const DWORD THREAD_PRIORITY_MEDIUM =       5;
const DWORD THREAD_PRIORITY_HIGH =         2;

const DWORD ONE_PAGE = 4096; //in bytes

BOOL IsValidHandle(HANDLE h);
BOOL IsSMPCapable();
DWORD GenerateAffinity(BOOL fCanHaveNoAffinity);
DWORD GenerateUserAppThreadPriority();
BOOL CheckThreadsAffinity(PHANDLE phThds, DWORD dwNumOfThreads, DWORD dwExpectedThreadAffinity);
BOOL CheckThreadAffinity(HANDLE hThread, DWORD dwExpectedThreadAffinity);
BOOL CheckProcessAffinity(HANDLE hProcess, DWORD dwExpectedProcessAffinity);
BOOL CanPowerOffSecondaryProcessors();
BOOL DoWaitForProcess(PHANDLE pProcHnd, DWORD dwNumOfProcesses, DWORD dwExpectedExitCode);
BOOL DoWaitForThread(PHANDLE pThdHnd, DWORD dwNumOfThreads, DWORD dwExpectedExitCode);
BOOL InKernelMode();
VOID RestoreInitialCondition();

#endif // __KTBVT_H__
