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

#ifndef __KSCHED_H__
#define __KSCHED_H__

// Test function prototypes (TestProc's)
TESTPROCAPI TickTest        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SchedTest           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

//PRIVATE FUNCTIONS


void DumpSystemTime( LPSYSTEMTIME, LPTSTR);
INT ElapsedTime (SYSTEMTIME*, SYSTEMTIME*);
DWORD WINAPI ThreadProc (LPVOID);
BOOL ParseCmdLine (LPWSTR);
int rand_range (int, int);

//PRIVATE DATA

#define DEFAULT_NUM_THREADS     100
#define DEFAULT_SLEEP_RANGE     10
#define DEFAULT_HIGHEST_PRIO    160
#define DEFAULT_LOWEST_PRIO     255
#define DEFAULT_RUN_TIME        300000
#define MAX_NUM_ERRORS          20

#define EDBG_PRIO               100

WCHAR gszHelp [] = {
    TEXT("/e:num        maximum number of errors (def: 20)\n")
    TEXT("/h:num        highest prio thread (def: 160)\n")
    TEXT("/l:num        lowest prio thread (def: 255)\n")
    TEXT("/n:num        number of threads (def: 100)\n")
    TEXT("/r:num        sleep range\n (def: 10)")
    TEXT("/t:num        run time in milliseconds (def: 300000)\n")
    TEXT("/q            quiet mode (no dbg messages) (def: not quiet)\n")
    TEXT("/c            continuous running (def: no)\n")
};



// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 0x00010000

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   TEXT( "Test the variable tick scheduler" ),  0,   0,             0, NULL,
   TEXT( "Test GetTickCount ( )"            ),  1,   0,      BASE+  1, TickTest,  
   TEXT( "Test scheduler"                   ),  2,   0,      BASE+  2, SchedTest,  
   NULL,                                        0,   0,             0, NULL  // marks end of list
};


#endif // __KSCHED_H__
