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
 * tuxFunctionTable.cpp
 */

/* 
 * pull in the function defs for the timer tests
 */
#include "..\code\tuxtimers.h"

/****** base to start the tests at *******/
#define TIMER_BASE 1000

FUNCTION_TABLE_ENTRY g_lpFTE[] = {
  /* usage message */
  _T(   "Timer Test Usage Message"    ), 0, 1, 1, timerTestUsageMessage,

///////////////////////////////////////////////////////////////////////////////
  _T("GetTickCount Timer Tests"                    ), 0, 0, 1, NULL,
  _T(   "Resolution of GetTickCount Timer (high priority)"    ), 1, 1, TIMER_BASE + 10, getTickCountResolution,
  _T(   "GetTickCount Backwards Check - busy"        ), 1, 0,  TIMER_BASE + 20, getTickCountBackwardsCheck,
  _T(   "GetTickCount Backwards Check - random sleep"), 1, 0,  TIMER_BASE + 30, getTickCountBackwardsCheckRandomSleep,
  _T(   "Print output from GetTickCount"                       ), 1, 0,  TIMER_BASE + 50, gtcPrintTime,

///////////////////////////////////////////////////////////////////////////////
  _T("High Resolution Timer Tests"                    ), 0, 0, 2, NULL,
  _T(   "Resolution of the High Resolution Timer"  ), 1, 0, TIMER_BASE + 100 + 10, hiPerfResolution,
  _T(   "High Resolution Timer Backwards Check - busy"      ), 1, 0, TIMER_BASE + 100 + 20, hiPerfBackwardsCheck,
  _T(   "Print output from High Resolution Timer"                 ), 1, 0, TIMER_BASE + 100 + 50, hiPerfPrintTime,

///////////////////////////////////////////////////////////////////////////////
  _T("System Time (Real-Time Clock)"                               ), 0, 0, 3, NULL,
  _T(   "Real-Time Clock Backwards Check - busy"                   ), 1, 0, TIMER_BASE + 200 + 20, rtcBackwardsCheck,
  _T(   "Print output from Real-Time Clock"                        ), 1, 0, TIMER_BASE + 200 + 50, rtcPrintTime,
  _T(   "Real-Time clock range"                        ), 1, 0, TIMER_BASE + 200 + 60, testRealTimeRange,
  _T(   "OEMGetRealTime and OEMSetRealTime functions"  ), 1, 0, TIMER_BASE + 200 + 70, testGetAndSetRealTime,
  _T(   "Time rollover on Real-Time Clock"                              ), 1, 0, TIMER_BASE + 200 + 80, testTimeRollover,
  _T(   "Set Real-Time Clock"                        ), 1, 0, TIMER_BASE + 200 + 100, testSetUserSpecifiedRealTime,

///////////////////////////////////////////////////////////////////////////////
  _T("Comparison Tests"                    ), 0, 0, 4, NULL,
  _T(   "Compare All Three Timers - Busy Sleep"   ), 1, DRIFT_BUSY_SLEEP, TIMER_BASE + 2000 + 10, compareAllThreeClocks,
  _T(   "Compare All Three Timers - OS Sleep"     ), 1, DRIFT_OS_SLEEP, TIMER_BASE + 2000 + 20, compareAllThreeClocks,
  _T(   "Compare All Three Timers - Track OEMidle Periodic"), 1, DRIFT_OEMIDLE_PERIODIC, TIMER_BASE + 2000 + 30, compareAllThreeClocks,
  _T(   "Compare All Three Timers - Track OEMidle Random"), 1, DRIFT_OEMIDLE_RANDOM, TIMER_BASE + 2000 + 40, compareAllThreeClocks,

///////////////////////////////////////////////////////////////////////////////
  _T("Miscellaneous Timer Tests"                  ), 0, 0, 5, NULL,
  _T(   "Wall Clock Drift Test GTC (manual)"      ), 1, 0, TIMER_BASE + 5000 + 10, wallClockDriftTestGTC,
  _T(   "Wall Clock Drift Test Hi Perf (manual)"  ), 1, 0, TIMER_BASE + 5000 + 20, wallClockDriftTestHP,
  _T(   "Wall Clock Drift Test RTC (manual)"      ), 1, 0, TIMER_BASE + 5000 + 30, wallClockDriftTestRTC,

///////////////////////////////////////////////////////////////////////////////
  _T("GetIdleTime Tests"                  ), 0, 0, 6, NULL,
  _T(   "Idle Time While Sleeping"      ), 1, 0, TIMER_BASE + 6000 + 10, idleTime,
  _T(   "Idle Time While Busy"          ), 1, 0, TIMER_BASE + 6000 + 20, idleTimeWhileBusy,

  NULL, 0, 0, 0, NULL  // marks end of list
};

