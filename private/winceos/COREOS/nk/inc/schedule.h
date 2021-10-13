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
#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include "kerncmn.h"

#define MAX_SUSPEND_COUNT 127

#define PRIORITY_LEVELS_HASHSCALE (MAX_CE_PRIORITY_LEVELS/PRIORITY_LEVELS_HASHSIZE)

//
// MAX_TIMEOUT is 0x7FFF0000 to allow some 'reasonable' system tick value. 
// The value needs to be <= (0x80000000 - system_tick), and we're
// assuming that the system-tick will never be > 64K ms here.
//
#define MAX_TIMEOUT     0x7FFF0000


extern TWO_D_QUEUE RunList;

struct _PROXY {
    // NOTE: The 1st 5 DWORD MUST LOOK EXACTLY LIKE TWO_D_NODE
    PPROXY      pQPrev;         // Previous node in 2-D queue
    PPROXY      pQNext;         // Next node in 2-D queue
    PPROXY      pQUp;           // Node above in 2-D queue
    PPROXY      pQDown;         // Node below in 2-D queue
    BYTE        prio;           // Current prio we're enqueued on (for thread - current priority)
    BYTE        bType;          // Type of object we're blocked on (various use for CS/mutex, indicate if the node is in a thread "owned list")
    WORD        wCount;         // Count matching thread's wCount (various use for CS/mutex, LockCount for mutex, signature for CS)
    PPROXY      pThLinkNext;    // Next proxy for this thread
    LPVOID      pObject;        // Pointer to object we're blocked on
    PTHREAD     pTh;            // Thread "owning" this proxy
    DWORD       dwRetVal;       // Return value if this is why we wake up
};

#define PROXY_OF_QNODE(qn)      ((PPROXY) (qn))
#define QNODE_OF_PROXY(prox)    ((PTWO_D_NODE) (prox))


// system startup function
void SystemStartupFunc (ulong param);

// structure to put thread to sleep
struct _SLEEPSTRUCT {
    PTHREAD pth;
    WORD    wCount2;
    WORD    wDirection;
};

// Wait functions
typedef BOOL (* PFN_WAITFUNC) (DWORD dwUserData);

struct _WAITSTRUCT {
    DWORD           cObjects;
    PHDATA          *phds;
    DWORD           dwTimeout;
    PPROXY          pProxyEnqueued;
    PPROXY          pProxyPending;
    PMUTEX          pMutex;
    BOOL            fEnqueue;
    PFN_WAITFUNC    pfnWait;
    DWORD           dwUserData;
    SLEEPSTRUCT     sleeper;
};

// create and lock an event. handle is closed automatically
PHDATA NKCreateAndLockEvent (BOOL fManualReset, BOOL fInitState);

//
// DoWaitForObjects - wait directly on PHDATA, internal use only
//
DWORD DoWaitForObjects (DWORD  cObjects, PHDATA *phds, DWORD dwTimeout);

BOOL NKWaitForMultipleObjects (DWORD cObjects, CONST HANDLE *lphObjects, BOOL fWaitAll, DWORD dwTimeout, LPDWORD lpRetVal);

//
// like WaitForSingleObject, except handle is translated with kernel handle table
//
DWORD NKWaitForKernelObject (HANDLE h, DWORD dwTimeout);

// sleep function
void UB_Sleep(DWORD dwMilliseconds);


// high part of the perf counter which is incremented by one every 
// time tick count wraps around; used in QueryPerformanceCounter.
extern volatile LONG g_PerfCounterHigh;


void NKSleepTillTick (void);
DWORD NKSetLowestScheduledPriority (DWORD maxprio);


//
// time/alarm related
//
void InitSoftRTC (void);

BOOL NKSetKernelAlarm (HANDLE hAlarm, const SYSTEMTIME *lpst);
void NKRefreshKernelAlarm (void);
ULONGLONG NKRefreshRTC (void);
BOOL NKGetRawTime (ULONGLONG *puliTime);
BOOL NKGetTime (LPSYSTEMTIME pst, DWORD dwType);
BOOL NKSetTime (const SYSTEMTIME *pst, DWORD dwType, LONGLONG *pliDelta);
BOOL NKGetTimeAsFileTime (LPFILETIME pft, DWORD dwType);
BOOL NKFileTimeToSystemTime (const FILETIME *lpft, LPSYSTEMTIME lpst);
BOOL NKSystemTimeToFileTime (const SYSTEMTIME *lpst, LPFILETIME lpft);
BOOL NKSetDaylightTime (DWORD dst, BOOL fAutoUpdate);
BOOL NKSetTimeZoneBias (DWORD dwBias, DWORD dwDaylightBias);

BOOL NKTicksToSystemTime (ULONGLONG ui64Ticks, SYSTEMTIME *lpst);
ULONGLONG NKSystemTimeToTicks (const SYSTEMTIME * lpst);

DWORD OEMGetTickCount (void);
BOOL CallKitlIoctl (DWORD dwCode, LPVOID pInBuf, DWORD nInSize, LPVOID pOutBuf, DWORD nOutSize, LPDWORD pcbRet);

#endif

