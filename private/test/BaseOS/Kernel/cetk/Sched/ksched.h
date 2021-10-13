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

#ifndef __KSCHED_H__
#define __KSCHED_H__

// Test function prototypes (TestProc's)
TESTPROCAPI  TickTest            (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI  SchedTest           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// PRIVATE FUNCTIONS
void         DumpSystemTime      (LPSYSTEMTIME, LPTSTR);
long long    ElapsedTimeInms     (SYSTEMTIME*, SYSTEMTIME*);
DWORD WINAPI SleeperThread(LPVOID);
BOOL         ParseCmdLine        (LPWSTR);
int          rand_range          (int, int);

typedef struct _THREADINFO {
    HANDLE hThread;
    DWORD dwThreadId;
    DWORD dwRunCount;
    DWORD dwThreadPrio;
} THREADINFO, *PTHREADINFO;

typedef struct _SLEEPERRINFO {
    DWORD dwThreadNum;
    DWORD dwThreadPri;
    DWORD dwSleepTime;
    DWORD dwTimerTime;
} SLEEPERRINFO, *PSLEEPERRINFO;

// TEST MACROS
#define DEFAULT_MAX_ERRORS       (10)
#define DEFAULT_HIGHEST_PRIO     (1)
#define DEFAULT_LOWEST_PRIO      (CE_THREAD_PRIO_256_TIME_CRITICAL - 1)
#define DEFAULT_NUM_THREADS      (100)
#define DEFAULT_SLEEP_RANGE      (15)
#define DEFAULT_RUN_TIME         (5 * 60 * 1000)
#define DEFAULT_TOLERANCE_TIME   (2)
#define SLEEP_TIME               (2000)
#define GOEVT                    (TEXT("KSCHED_GO_EVENT"))

#define MAX(x, y) ( ((x) >  (y)) ? (x) : (y)  )
#define ABS(x)    ( ((x) >= 0)   ? (x) : -(x) )


// PRIVATE DATA
WCHAR gszHelp [] = {
    TEXT("/e:num        maximum number of errors (def: 10)\n")
    TEXT("/h:num        highest prio thread (def: 1)\n")
    TEXT("/l:num        lowest prio thread (def: 247)\n")
    TEXT("/n:num        number of threads (def: 100)\n")
    TEXT("/s:num        sleep range\n (def: 15)")
    TEXT("/r:num        run time in milliseconds (def: 300000)\n")
    TEXT("/t:num        tolerance time in milliseconds (def: 2)\n")
    TEXT("/c            continuous running (def: no)\n")
};



// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every
// test case within the entire team to be uniquely identified.

#define BASE 0x00010000

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   TEXT( "Test the variable tick scheduler" ),  0,   0,         0, NULL,
   TEXT( "Test GetTickCount ( )"            ),  1,   0,  BASE+  1, TickTest,
   TEXT( "Test scheduler"                   ),  2,   0,  BASE+  2, SchedTest,
   // marks end of list
   NULL,                                        0,   0,         0, NULL
};


#endif // __KSCHED_H__
