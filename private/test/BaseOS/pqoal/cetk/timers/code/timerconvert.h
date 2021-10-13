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
 * timerConvert.h
 */

#ifndef __TIMER_CONVERT_H
#define __TIMER_CONVERT_H

#include <windows.h>
#include <assert.h>

#include "timerAbstraction.h"

extern sTimer gGetTickCountTimer;
extern sTimer gHiPerfTimer;
extern sTimer gTimeOfDayTimer;

/* initialize a timer structure */
BOOL
initTimer (sTimer * newTimer, sTimer * existingTimer);

/*
 * calls GetTickCount and converts the result into a ulonglong.
 *
 * returns FALSE on failure/ TRUE on success
 */
BOOL
GetTickCountFunc (ULONGLONG & ullTicks);

/*
 * returns the freq of getTickCount
 *
 * returns FALSE on failure/ TRUE on success
 */
BOOL
GetTickCountFreqFunc (ULONGLONG & ullFreq);

/*
 * calls QueryPerformanceCounter and converts the result into a
 * ulonglong. 
 *
 * returns FALSE on failure/ TRUE on success
 */
BOOL
HiPerfTicksFunc (ULONGLONG & ullTicks);

/*
 * returns QueryPerformanceFrequency (frequency of hi perf. clock)
 *
 * returns FALSE on failure/ TRUE on success
 */
BOOL
HiPerfFreqFunc (ULONGLONG & ullFreq);

/*
 * returns calls QueryPerformanceCounter and converts the result into a
 * ulonglong.
 *
 * returns FALSE on failure/ TRUE on success
 */
BOOL
RTCTicksFunc (ULONGLONG & ullTicks);

/*
 * returns frequency of time of day clock (rtc).  
 *
 * returns FALSE on failure/ TRUE on success
 */
BOOL
RTCFreqFunc (ULONGLONG & ullFreq);




#endif /* __TIMER_CONVERT_H */
