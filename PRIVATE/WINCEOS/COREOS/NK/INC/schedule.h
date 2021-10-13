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
#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include "kerncmn.h"

#define MAX_SUSPEND_COUNT 127

#define PRIORITY_LEVELS_HASHSIZE 32

#define PRIORITY_LEVELS_HASHSCALE (MAX_CE_PRIORITY_LEVELS/PRIORITY_LEVELS_HASHSIZE)

//
// MAX_TIMEOUT is 0x7FFF0000 to allow some 'reasonable' system tick value. 
// The value needs to be <= (0x80000000 - system_tick), and we're
// assuming that the system-tick will never be > 64K ms here.
//
#define MAX_TIMEOUT     0x7FFF0000

typedef struct {
    PTHREAD pRunnable;  /* List of runnable threads */
    PTHREAD pth;        /* Currently running thread */
    PTHREAD pHashThread[PRIORITY_LEVELS_HASHSIZE];
} RunList_t;

struct _PROXY {
    PPROXY pQPrev;         /* Previous proxy for this object queue, must be first for ReQueueByPriority */
    PPROXY pQNext;         /* Next proxy for this object queue */
    PPROXY pQUp;
    PPROXY pQDown;
    PPROXY pThLinkNext;    /* Next proxy for this thread */
    LPBYTE  pObject;        /* Pointer to object we're blocked on */
    BYTE    bType;          /* Type of object we're blocked on */
    BYTE    prio;           /* Current prio we're enqueued on */
    WORD    wCount;         /* Count matching thread's wCount */
    PTHREAD pTh;            /* Thread "owning" this proxy */
    DWORD   dwRetVal;       /* Return value if this is why we wake up */
};

struct _CLEANEVENT {
    PCLEANEVENT ceptr;
    LPVOID      base;
    DWORD       size;
    DWORD       op;
};

// DequeueProxy - remove a proxy from priority queue
void DequeueProxy (PPROXY pProx);

// system startup function
void SystemStartupFunc (ulong param);

// Wait functions

//
// DoWaitForObjects - wait directly on PHDATA, internal use only
//
DWORD DoWaitForObjects (DWORD  cObjects, PHDATA *phds, DWORD dwTimeout);

DWORD NKWaitForSingleObject (HANDLE h, DWORD dwTimeout);
DWORD NKWaitForMultipleObjects (DWORD cObjects, CONST HANDLE *lphObjects, BOOL fWaitAll, DWORD dwTimeout);

//
// like WaitForSingleObject, except handle is translated with kernel handle table
//
DWORD NKWaitForKernelObject (HANDLE h, DWORD dwTimeout);

// sleep function
void UB_Sleep(DWORD dwMilliseconds);

extern DWORD dwPrevReschedTime;
extern DWORD CurrTime;

// high part of the perf counter which is incremented by one every 
// time tick count wraps around; used in QueryPerformanceCounter.
extern DWORD g_PerfCounterHigh;


void NKSleepTillTick (void);
DWORD NKSetLowestScheduledPriority (DWORD maxprio);


//
// time/alarm related
//
void InitSoftRTC (void);

void NKSetKernelAlarm (HANDLE hAlarm, const SYSTEMTIME *lpst);
void NKRefreshKernelAlarm (void);
ULONGLONG NKRefreshRTC (void);
BOOL NKGetRawTime (ULONGLONG *puliTime);
BOOL NKGetTime (LPSYSTEMTIME pst, DWORD dwType);
BOOL NKSetTime (const SYSTEMTIME *pst, DWORD dwType, LONGLONG *pliDelta);
BOOL NKGetTimeAsFileTime (LPFILETIME pft, DWORD dwType);
BOOL NKFileTimeToSystemTime (const FILETIME *lpft, LPSYSTEMTIME lpst);
BOOL NKSystemTimeToFileTime (const SYSTEMTIME *lpst, LPFILETIME lpft);

BOOL NKTicksToSystemTime (ULONGLONG ui64Ticks, SYSTEMTIME *lpst);
ULONGLONG NKSystemTimeToTicks (const SYSTEMTIME * lpst);

DWORD OEMGetTickCount (void);
BOOL CallKitlIoctl (DWORD dwCode, LPVOID pInBuf, DWORD nInSize, LPVOID pOutBuf, DWORD nOutSize, LPDWORD pcbRet);

#endif

