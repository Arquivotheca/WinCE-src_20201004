//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __KSCHED_H__
#define __KSCHED_H__

// Test function prototypes (TestProc's)
TESTPROCAPI TickTest       	(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SchedTest       	(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

//PRIVATE FUNCTIONS


void DumpSystemTime( LPSYSTEMTIME, LPTSTR);
INT ElapsedTime (SYSTEMTIME*, SYSTEMTIME*);
DWORD WINAPI ThreadProc (LPVOID);
BOOL ParseCmdLine (LPWSTR);
int rand_range (int, int);

//PRIVATE DATA

#define DEFAULT_NUM_THREADS		100
#define DEFAULT_SLEEP_RANGE		10
#define DEFAULT_HIGHEST_PRIO	160
#define	DEFAULT_LOWEST_PRIO		255
#define	DEFAULT_RUN_TIME		300000
#define MAX_NUM_ERRORS			20

#define EDBG_PRIO				100

WCHAR gszHelp [] = {
	TEXT("/e:num		maximum number of errors\n")
	TEXT("/h:num		highest prio thread\n")
	TEXT("/l:num		lowest prio thread\n")
	TEXT("/n:num		number of threads\n")
	TEXT("/r:num		sleep range\n")
	TEXT("/t:num		run time\n")
	TEXT("/q			quiet mode (no dbg messages)\n")
	TEXT("/c			continuous running\n")
};



// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 0x00010000

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   TEXT("Test the variable test scheduler"), 	0,   0,				0, NULL,
   TEXT( "Test GetTickCount ( )"),	1,   0,      BASE+	1, TickTest,  
   TEXT( "Test scheduler"),	2,   0,      BASE+	2, SchedTest,  
   NULL,                   			0,   0,				0, NULL  // marks end of list
};


#endif // __KSCHED_H__
