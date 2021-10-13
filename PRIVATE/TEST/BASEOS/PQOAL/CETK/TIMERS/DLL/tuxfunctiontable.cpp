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


  _T("GetTickCount Timer Tests"                    ), 0, 0, 0, NULL,
  _T(   "Tick Count Resolution (high priority)"    ), 1, 1, TIMER_BASE + 10, getTickCountLatencyAndResolution,
  _T(   "Tick Count Backwards Check - busy"        ), 1, 0,  TIMER_BASE + 20, getTickCountBackwardsCheck,
  _T(   "Tick Count Backwards Check - random sleep"), 1, 0,  TIMER_BASE + 30, getTickCountBackwardsCheckRandomSleep,
  _T(   "Print GetTickCount"                       ), 1, 0,  TIMER_BASE + 50, gtcPrintTime,

  _T("High Performance Timer Tests"                    ), 0, 0, 0, NULL,
  _T(   "High Performance Resolution (high priority)"  ), 1, 0, TIMER_BASE + 100 + 10, hiPerfLatencyAndResolution,
  _T(   "High Performance Backwards Check - busy"      ), 1, 0, TIMER_BASE + 100 + 20, hiPerfBackwardsCheck,
  _T(   "Print High Performance Clock"                 ), 1, 0, TIMER_BASE + 100 + 50, hiPerfPrintTime,

  _T("System Time (RTC)"                               ), 0, 0, 0, NULL,
  _T(   "RTC Resolution (high priority)"               ), 1, 0, TIMER_BASE + 200 + 10, rtcLatencyAndResolution,
  _T(   "RTC Backwards Check - busy"                   ), 1, 0, TIMER_BASE + 200 + 20, rtcBackwardsCheck,
  _T(   "Print System Time\r\n"                        ), 1, 0, TIMER_BASE + 200 + 50, rtcPrintTime,

  _T(	"Real-Time clock range"                        ), 1, 0, TIMER_BASE + 200 + 60, testRealTimeRange,
  _T(	"OEMGetRealTime and OEMSetRealTime functions"  ), 1, 0, TIMER_BASE + 200 + 70, testGetAndSetRealTime,
  _T(   "Rollover of RTC"                              ), 1, 0, TIMER_BASE + 200 + 80, testTimeRollover,
  _T(   "Reentrance of OEMGetRealTime and OEMSetRealTime functions"), 1, 0, TIMER_BASE + 200 + 90, testReentrant,
  _T(   "Set System Time (RTC)"                        ), 1, 0, TIMER_BASE + 200 + 100, testSetUserSpecifiedRealTime,

  _T("Comparison Tests"                    ), 0, 0, 0, NULL,
  _T(   "Compare All Three - Busy Sleep"   ), 1, DRIFT_BUSY_SLEEP, TIMER_BASE + 2000 + 10, compareAllThreeClocks,
  _T(   "Compare All Three - OS Sleep"     ), 1, DRIFT_OS_SLEEP, TIMER_BASE + 2000 + 20, compareAllThreeClocks,
  _T(   "Compare All Three - Track OEMidle Periodic"), 1, DRIFT_OEMIDLE_PERIODIC, TIMER_BASE + 2000 + 30, compareAllThreeClocks,
  _T(   "Compare All Three - Track OEMidle Random"), 1, DRIFT_OEMIDLE_RANDOM, TIMER_BASE + 2000 + 40, compareAllThreeClocks,

  _T("Miscellaneous Timer Tests"                  ), 0, 0, 0, NULL,
  _T(   "Wall Clock Drift Test GTC (manual)"      ), 1, 0, TIMER_BASE + 5000 + 10, wallClockDriftTestGTC,
  _T(   "Wall Clock Drift Test Hi Perf (manual)"  ), 1, 0, TIMER_BASE + 5000 + 20, wallClockDriftTestHP,
  _T(   "Wall Clock Drift Test RTC (manual)"      ), 1, 0, TIMER_BASE + 5000 + 30, wallClockDriftTestRTC,

  _T("GetIdleTime Tests"                  ), 0, 0, 0, NULL,
  _T(   "Idle Time While Sleeping"      ), 1, 0, TIMER_BASE + 6000 + 10, idleTime,
  _T(   "Idle Time While Busy"          ), 1, 0, TIMER_BASE + 6000 + 20, idleTimeWhileBusy,

  NULL, 0, 0, 0, NULL  // marks end of list
};

