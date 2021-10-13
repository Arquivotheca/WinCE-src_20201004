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
 * tuxtimers.h
 */

#ifndef __TUXTIMERS_H__
#define __TUXTIMERS_H__

#include <windows.h>
#include <tchar.h>
#include <tux.h>


/*
 * This file includes TESTPROCs for the timer test functions.  Anything
 * needed by the tux function table must be included here.  This file
 * is included by the file with the tux function table.
 *
 * Please keep non-tux related stuff (stuff local to timers) out of
 * this file to keep from polluting the name space.
 */


/**************************************************************************
 * 
 *                           Timer Test Function Prototypes
 *
 **************************************************************************/

/*******  usage  **********************************************************/

TESTPROCAPI timerTestUsageMessage(
                  UINT uMsg, 
                  TPPARAM tpParam, 
                  LPFUNCTION_TABLE_ENTRY lpFTE);

/*******  getTickCount ****************************************************/

TESTPROCAPI getTickCountResolution(
                         UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI getTickCountBackwardsCheck(
                       UINT uMsg, 
                       TPPARAM tpParam, 
                       LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI getTickCountBackwardsCheckRandomSleep(
                       UINT uMsg, 
                       TPPARAM tpParam, 
                       LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI gtcPrintTime(
                  UINT uMsg, 
                  TPPARAM tpParam, 
                  LPFUNCTION_TABLE_ENTRY lpFTE);

/*******  High Performance  ***********************************************/

TESTPROCAPI hiPerfResolution(
                       UINT uMsg, 
                       TPPARAM tpParam, 
                       LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI hiPerfBackwardsCheck(
                       UINT uMsg, 
                       TPPARAM tpParam, 
                       LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI hiPerfPrintTime(
                  UINT uMsg, 
                  TPPARAM tpParam, 
                  LPFUNCTION_TABLE_ENTRY lpFTE);

/*******   System Time (RTC)  *********************************************/

TESTPROCAPI rtcBackwardsCheck(
                  UINT uMsg, 
                  TPPARAM tpParam, 
                  LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI rtcPrintTime(
                  UINT uMsg, 
                  TPPARAM tpParam, 
                  LPFUNCTION_TABLE_ENTRY lpFTE);

// Additional RTC tests
TESTPROCAPI testRealTimeRange(
                    UINT uMsg, 
                    TPPARAM tpParam, 
                    LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testGetAndSetRealTime(
                    UINT uMsg, 
                    TPPARAM tpParam, 
                    LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testTimeRollover(
                    UINT uMsg, 
                    TPPARAM tpParam, 
                    LPFUNCTION_TABLE_ENTRY lpFTE);

/* sets a user specified time */
TESTPROCAPI testSetUserSpecifiedRealTime(
                     UINT uMsg, 
                     TPPARAM tpParam, 
                     LPFUNCTION_TABLE_ENTRY lpFTE);



/*******  Compare timers  *************************************************/

#define DRIFT_BUSY_SLEEP 1
#define DRIFT_OS_SLEEP 2
#define DRIFT_OEMIDLE_PERIODIC 3
#define DRIFT_OEMIDLE_RANDOM 4

TESTPROCAPI compareAllThreeClocks(
                  UINT uMsg, 
                  TPPARAM tpParam, 
                  LPFUNCTION_TABLE_ENTRY lpFTE);

/*******  Wall Clock Drift Test ******************************************/

TESTPROCAPI wallClockDriftTestGTC (
                   UINT uMsg, 
                   TPPARAM tpParam, 
                   LPFUNCTION_TABLE_ENTRY lpFTE);

/* wrapper for the hi perf test */
TESTPROCAPI wallClockDriftTestHP (
                   UINT uMsg, 
                   TPPARAM tpParam, 
                   LPFUNCTION_TABLE_ENTRY lpFTE);

/* wrapper for the rtc test */
TESTPROCAPI wallClockDriftTestRTC (
                   UINT uMsg, 
                   TPPARAM tpParam, 
                   LPFUNCTION_TABLE_ENTRY lpFTE);


/**************************************************************************
 * 
 *                      GetIdleTime Test Function Prototypes
 *
 **************************************************************************/

TESTPROCAPI idleTime(
             UINT uMsg, 
             TPPARAM tpParam, 
             LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI idleTimeWhileBusy(
             UINT uMsg, 
             TPPARAM tpParam, 
             LPFUNCTION_TABLE_ENTRY lpFTE);

#endif // __TUXTIMERS_H__
