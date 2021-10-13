//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * tuxFunctionTable.h
 */

#ifndef __TUX_FUNCTION_TABLE_H__
#define __TUX_FUNCTION_TABLE_H__

/*
 * This needs to include every header for each individual set of tests.
 * The function table will then be able to find all of the needed
 * TESTPROCs.
 */
#include "..\caches\tuxcaches.h"
#include "..\timers\tuxtimers.h"




/****** base to start the tests at.  The test numbers CAN'T overlap.  *******/
#define TIMER_BASE 100


#define CACHE_BASE 200

#define CACHE_BASE_CACHE_DISABLED (CACHE_BASE + 10)
#define CACHE_BASE_WRITE_THROUGH (CACHE_BASE + 40)
#define CACHE_BASE_WRITE_BACK (CACHE_BASE + 70)



static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
  /******  Timer tests ******/
  _T("GetTickCount Timer Tests"          ), 0, 0,         0, NULL,
  _T(   "Tick Count Latency/Resolution (high priority)"    ), 1, 1,  TIMER_BASE+ 2, getTickCountLatencyAndResolution,
  _T(   "Tick Count Backwards Check"     ), 1, 0,  TIMER_BASE+ 3, getTickCountBackwardsCheck,
  
  _T("High Performance Timer Tests"            ), 0, 0,         0, NULL,
  _T(   "High Performance Latency/Resolution (high priority)"  ), 1, 0,  TIMER_BASE+ 11, hiPerfLatencyAndResolution,
  _T(   "High Performance Backwards Check"     ), 1, 0,  TIMER_BASE+ 12, hiPerfBackwardsCheck,

  _T("System Time (RTC)"                       ), 0, 0,         0, NULL,
  _T(   "RTC Latency/Resolution (high priority)"               ), 1, 0,  TIMER_BASE+ 21, rtcLatencyAndResolution,
  _T(   "RTC Backwards Check"                  ), 1, 0,  TIMER_BASE+ 22, rtcBackwardsCheck,

  _T("Comparison Tests"                        ), 0, 0,         0, NULL,
  _T(   "Compare High Perf to GetTickCount"    ), 1, 0,  TIMER_BASE+ 31, compareHiPerfToGetTickCount,
  _T(   "Compare RTC to GetTickCount"          ), 1, 0,  TIMER_BASE+ 32, compareRTCToGetTickCount,
  _T(   "Compare High Perf to RTC"             ), 1, 0,  TIMER_BASE+ 33, compareHiPerfToRTC,

  _T("Miscellaneous Timer Tests"               ), 0, 0,         0, NULL,
  _T(   "Wall Clock Drift Test (manual)"       ), 1, 0,  TIMER_BASE+ 41, wallClockDriftTest,

  /******  Cache tests ******/
  _T("Cache Tests"                                ), 0, 0, 0, NULL,

  _T(  "Cache Tests Usage / Help"), 1, CT_IGNORED, CACHE_BASE + 0, cacheTestPrintUsage,
  _T(  "Print Cache Info"), 1, CT_IGNORED, CACHE_BASE + 1, printCacheInfo,

  /* 
   * The parameter tells the cache test how to set the test array size
   * and the cache mode.  See the cache test tux header tuxcaches.h
   * for more information.
   */
  _T(  "No caching"), 1, 0, 0, NULL,

  _T(  "Write Boundaries Read Everything (array = cache size)"),      2, CT_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 0, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 1, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = user defined)"),     2, CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 2, entryCacheTestWriteBoundariesReadEverything,

  _T(  "Write Everything Read Everything (array = cache size)"),         2, CT_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 3, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = twice cache size)"),    2, CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 4, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = user defined)"),    2, CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 5, entryCacheTestWriteEverythingReadEverything,

  _T(  "Verify Near Powers of Two (array = cache size)"                    ), 2, CT_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 6, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = twice cache size)"              ), 2, CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 7, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = user defined)"              ), 2, CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 8, entryCacheTestVaryPowersOfTwo,

  _T(  "Write Everything Read Everything Mixed Up (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 9, entryCacheTestWriteEverythingReadEverythingMixedUp,
  _T(  "Write Everything Read Everything Mixed Up (array = user defined)"   ), 2, CT_USER_DEFINED | CT_DISABLED, CACHE_BASE_CACHE_DISABLED + 10, entryCacheTestWriteEverythingReadEverythingMixedUp,

  _T(  "Write-through cache mode"), 1, 0, 0, NULL,

  _T(  "Write Boundaries Read Everything (array = cache size)"),      2, CT_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 0, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 1, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = user defined)"),     2, CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 2, entryCacheTestWriteBoundariesReadEverything,

  _T(  "Write Everything Read Everything (array = cache size)"),         2, CT_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 3, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = twice cache size)"),    2, CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 4, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = user defined)"),    2, CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 5, entryCacheTestWriteEverythingReadEverything,

  _T(  "Verify Near Powers of Two (array = cache size)"                    ), 2, CT_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 6, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = twice cache size)"              ), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 7, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = user defined)"              ), 2, CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 8, entryCacheTestVaryPowersOfTwo,

  _T(  "Write Everything Read Everything Mixed Up (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 9, entryCacheTestWriteEverythingReadEverythingMixedUp,
  _T(  "Write Everything Read Everything Mixed Up (array = user defined)"   ), 2, CT_USER_DEFINED | CT_WRITE_THROUGH, CACHE_BASE_WRITE_THROUGH + 10, entryCacheTestWriteEverythingReadEverythingMixedUp,

  _T(  "Write-back cache mode"), 1, 0, 0, NULL,

  _T(  "Write Boundaries Read Everything (array = cache size)"),      2, CT_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 0, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 1, entryCacheTestWriteBoundariesReadEverything,
  _T(  "Write Boundaries Read Everything (array = user defined)"),     2, CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 2, entryCacheTestWriteBoundariesReadEverything,

  _T(  "Write Everything Read Everything (array = cache size)"),         2, CT_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 3, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = twice cache size)"),    2, CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 4, entryCacheTestWriteEverythingReadEverything,
  _T(  "Write Everything Read Everything (array = user defined)"),    2, CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 5, entryCacheTestWriteEverythingReadEverything,

  _T(  "Verify Near Powers of Two (array = cache size)"                    ), 2, CT_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 6, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = twice cache size)"              ), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 7, entryCacheTestVaryPowersOfTwo,
  _T(  "Verify Near Powers of Two (array = user defined)"              ), 2, CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 8, entryCacheTestVaryPowersOfTwo,

  _T(  "Write Everything Read Everything Mixed Up (array = twice cache size)"), 2, CT_TWICE_CACHE_SIZE | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 9, entryCacheTestWriteEverythingReadEverythingMixedUp,
  _T(  "Write Everything Read Everything Mixed Up (array = user defined)"   ), 2, CT_USER_DEFINED | CT_WRITE_BACK, CACHE_BASE_WRITE_BACK + 10, entryCacheTestWriteEverythingReadEverythingMixedUp,



  
   NULL,                                       0,   0,              0, NULL  // marks end of list
};

#endif // __TUX_FUNCTION_TABLE_H__
