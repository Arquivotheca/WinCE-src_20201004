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
/*
 * timersCmdLine.h
 */
#ifndef __TIMERS_CMD_LINE_H
#define __TIMERS_CMD_LINE_H

#include <windows.h>

/* gives declarations for tokens */
#include "..\..\..\common\cmd.h"

/* gives the declarations for bounds */
#include "timerAbstraction.h"

/* maximum allowed gtc value */
#define ARG_STR_GTC_MAX_RES (_T("-gtcMaxRes"))

/* run time for backwards checks */
#define ARG_STR_BACKWARDS_CHECK_RUN_TIME (_T("-backwardsRunTime"))

/* define default values for real-time tests */
#define DEFAULT_REAL_TIME_TEST_NUM_THREADS 50
#define DEFAULT_REAL_TIME_TEST_RUN_TIME_SEC 600      //600 sec = 10 min


/******* comparing clocks */

/* general */
#define ARG_STR_DRIFT_SAMPLE_SIZE (_T("-driftSampleSize"))
#define ARG_STR_DRIFT_RUN_TIME (_T("-driftRunTime"))

/* test names */
#define ARG_STR_BUSY_SLEEP_NAME (_T("-busySleep"))
#define ARG_STR_OS_SLEEP_NAME (_T("-osSleep"))
#define ARG_STR_TRACK_IDLE_PERIODIC_NAME (_T("-trackIdlePer"))
#define ARG_STR_TRACK_IDLE_RANDOM_NAME (_T("-trackIdleRan"))

/* drift bounds */
#define ARG_STR_DRIFT_GTC_RTC (_T("GtcToRtc"))
#define ARG_STR_DRIFT_GTC_HI (_T("GtcToHiPerf"))
#define ARG_STR_DRIFT_HI_RTC (_T("HiPerfToRtc"))

/* wall clock */
#define ARG_STR_WALL_CLOCK_RUN_TIME (_T("-wcRunTime"))

/******* real time tests */
/* args for real-time test */
#define ARG_STR_DATE_AND_TIME (_T("-dateAndTime"))
#define ARG_STR_NUM_OF_THREADS (_T("-numberOfThreads"))
#define ARG_STR_RUN_TIME (_T("-reentranceRunTime"))

/******* GetIdleTime tests */
#define ARG_STRING_IDLE_RUN_TIME (_T("-idleRunTime"))


/* print a usage message */
VOID
printTimerUsageMessage ();


/* get the bounds for the drift tests */
BOOL
getBoundsForDriftTests(cTokenControl * tokens, 
		       TCHAR * szTestName,
		       TCHAR * szBound1Name,
		       sBounds * bound1,
		       TCHAR * szBound2Name,
		       sBounds * bound2,
		       TCHAR * szBound3Name,
		       sBounds * bound3);

/* 
 * get the args for the drift tests.  the bounds ratios
 * are parsed separately.
 */
BOOL
getArgsForDriftTests (cTokenControl * tokens, 
		      TCHAR * szSampleSizeName,
		      TCHAR * szRunTimeName,
		      DWORD * pdwSampleSize,
		      DWORD * pdwRunTime);


/* get the args for the backwards check tests */
BOOL
handleBackwardsCheckParams (ULONGLONG * pullRunTime);


/* get args for the real-time tests */
// Parses the command line arguments and returns the user supplied date and time
BOOL parseUserDateTime (LPSYSTEMTIME pUserTime);

// Parses the command line arguments and returns the number of threads and
// iterations for the reentrant test
BOOL parseThreadInfo (DWORD *nThread, DWORD *nIterations);


#endif
