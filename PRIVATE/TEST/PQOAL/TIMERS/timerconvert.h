//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
