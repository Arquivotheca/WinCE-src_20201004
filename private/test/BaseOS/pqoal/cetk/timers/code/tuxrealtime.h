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

/*
 * tuxRealTime.h
*/

#ifndef __TUX_REAL_TIME_H
#define __TUX_REAL_TIME_H

////////////////////////////////////////////////////////////////////////////////
/**********************************  Headers  *********************************/

/* common utils */
#include "..\..\..\common\commonUtils.h"

/* command line parsing */
#include "timersCmdLine.h"

#include <windows.h>
#include <tchar.h>
#include <tux.h>

////////////////////////////////////////////////////////////////////////////////
/**************************  Constants and Defines *************************/


// days of week
extern TCHAR * DaysOfWeek[];

// mimicks dayOfWeek from SYSTEM_TIME
enum dayOfWeek {SUN = 0, MON, TUES, WED, THURS, FRI, SAT};


// This structure contains various times used to test the Set and Get Real Time 
// functions. The elements consist of time which is in SYSTEMTIME format
// and a comment. 
typedef struct REALTIME_TESTS {
        SYSTEMTIME time;
        TCHAR comment[50];
}stRealTimeTests;

extern stRealTimeTests g_testTimes[];


// This structure contains various times used to test the rollover in case of
// seconds, minutes, hours, etc. The elements consist of time in SYSTEMTIME 
// format which is to be set, a comment on the type of rollover and the time 
// that is to be present 5 seconds later. 
typedef struct ROLLOVER_TESTS {
    SYSTEMTIME time1;   // time to be set
    TCHAR comment[50];  // rollover description
    SYSTEMTIME time2;   // time expected after 5 sec
}stRolloverTests;


#define SIXTYFOUR_BIT_OVERFLOW 18446744073709551615  

#define TEN_SEC_IN_100ns_TICKS 100000000

#define FIVE_SEC_IN_100ns_TICKS 50000000

#define ONE_SEC_IN_100ns_TICKS 10000000

#define PRINT_BUFFER_SIZE 50

// per msdn doc for FileTimeToSystemTime
#define MAX_FILETIME    (0x7fffffffffffffff)


////////////////////////////////////////////////////////////////////////////////
/******************************* Functions ***********************************/

// Prints the system time
BOOL printSystemTime (LPSYSTEMTIME lpSysTime, BOOL bIgnoreDayOfWeek = TRUE );

// Prints the time to buffer
BOOL printTimeToBuffer (LPSYSTEMTIME lpSysTime,__out_ecount(maxSize) TCHAR *ptcBuffer,size_t maxSize,
            BOOL bIgnoreDayOfWeek = TRUE);

// Finds the range of valid times
BOOL findRange(SYSTEMTIME *pBeginTime, SYSTEMTIME *pEndTime);

// Finds the exact begin or end point of the supported range
BOOL findTransitionPoints(ULONGLONG *pullTime, BOOL bBeginPoint, 
                        SYSTEMTIME *stTransitionPoint);

// Disables DST
HANDLE disableDST();

// Enables DST
BOOL enableDST(HANDLE hDST);


#endif //__TUX_REAL_TIME_H

