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
 * timerAbstraction.h
 */

#ifndef __TIMER_ABSTRACTION_H
#define __TIMER_ABSTRACTION_H

#include <windows.h>
#include <assert.h>

#include "..\..\..\common\commonUtils.h"

/*
 * struct the provides an abstraction of the timers.  This allows the
 * function to deal with one type of timer.  The function pointer work
 * around type issues, since each clock uses a different type.  The
 * converter function above hide the necessary conventions from the
 * rest of the code.
 */
typedef struct _stimer
{
  const TCHAR * name;
  BOOL (*getTick)(ULONGLONG & ullTicks);
  BOOL (*getFreq)(ULONGLONG & ullFreq);
  ULONGLONG ullWrapsAt;
  ULONGLONG ullFreq;
  double doConversionToS;
} sTimer;


/*
 * Provides a way to easily pass the bounds of the drift test into the
 * function.  szName denotes which two clocks are being compared ("RTC
 * to GTC").  The bounds come next (faster must be numerically greater
 * than slower).  Loosest resolution is the loosest resolution between
 * these two clocks.  This isn't the precision (units), but the actual
 * resolution on the given system.
 */
typedef struct
{
  TCHAR * szName;
  double doFasterBound;
  double doSlowerBound;
  ULONGLONG ullLoosestRes;
} sBounds;


extern BOOL g_fQuickFail;

#define THREAD_PRIORITY_ZERO 0

#define THREAD_PRIORITY_BELOW_KITL 131

/***************************************************************************
 * 
 *                                  Initialization
 *
 ***************************************************************************/

/*
 * initalize the timer structure.  This must be called on each structure
 * before it can be used.
 * 
 * Returns TRUE on success and FALSE otherwise.
 */
BOOL
initTimerStruct (sTimer & timer);

/***************************************************************************
 * 
 *                          Get Latency and Resolution
 *
 ***************************************************************************/

BOOL
getResolution (sTimer * timer, 
            DWORD dwNumIterations,
            ULONGLONG * pullN, ULONGLONG * pullRange, 
        BOOL bPrintSpew);


/***************************************************************************
 * 
 *                                Comparing Clocks
 *
 ***************************************************************************/

/*
 * This function compares three clocks to each pair-wise.  Symantics
 * are the same as the two clock variety.
 *
 * All timers must be non-null and initialized.  The bounds structures
 * give the bounds for each pair-wise comparison.  szName,
 * doFasterbound, and doSlowerBound must be initialized.
 * ullLoosestRes is initialized by this function.
 *
 * Returns true on success and false on failure.
 */
BOOL
compareThreeClocks(sTimer * timer1, sTimer * timer2, sTimer * timer3,
           sBounds * bound1To2, 
           sBounds * bound1To3, 
           sBounds * bound2To3,
           DWORD dwSampleStepS,
           DWORD dwTestRunTimeS,
           BOOL (*fSleep)(DWORD dwTimeMS));

/***************************************************************************
 * 
 *                                Miscellaneous 
 *
 ***************************************************************************/

/*
 * goal is confirm that the timer doesn't go backwards.  We watch the
 * timer in a busy loop, checking it on each iteration.
 *
 * We say it goes backwards if the rollover is ever more that twice
 * the resolution.
 *
 * timer is the timer to work with, which can't be null.
 * ullIterations is the number of iterations to do.  ullRes is the
 * resolution of the clock.  ullPrintAtIt denotes how often to print
 * the time during the test.  This is useful for debugging.  The time
 * is printed every ullPrintAtIt iterations.  Zero denotes that no
 * time messages should be printed.
 *
 * If the user would like the function to sleep between timer reads,
 * he passes in a function pointer to a sleep function (taking a
 * dword), and then passes in the sleep time as the next parameter.
 * If the user would not like the function to sleep, meaning that it
 * just stays in a tight loop reading values, the user should pass in
 * NULL as the function pointer.  the dwSleepTime parameter is ignored
 * in the latter case.
 *
 * On error, messages are printed.  The test continues on error until
 * the allotted number of iterations has been completed.
 */
BOOL
doesClockGoBackwards (sTimer * timer,
              ULONGLONG ullIterations, 
              ULONGLONG ullRes, ULONGLONG ullPrintAtIt,
              BOOL (*fSleep)(DWORD dwTimeMS),
              DWORD dwSleepTime);

/*
 * This allows the caller to compare the given timer to a wall clock.
 * It records the starting time and then waits dwTestLengthS seconds, 
 * printing out messages now and then so that you know that it didn't
 * crash.  The messages get more frequent as the test nears the end,
 * at which time you can stop your watch and compare your result to
 * what the test think happened.  If they are close then you timers
 * aren't off by a lot.
 *
 * This test uses sleep.  If sleep is not acting correctly it could
 * cause this test to complain about possible overflow conditions.  It
 * won't cause it to report false negatives, since the time reported
 * at the end is taken from the clocks and has nothing to do with
 * sleep.  Sleep is just used to print out informational statements
 * are given times and to keep the processor free during this test.
 *
 * With a very noisy sleep, it is possible this could report a false
 * positive.  This is very very unlikely and never occur.  The timer
 * would have to be very close to the overflow boundaries such that
 * the overflow check didn't think that the variables would overflow
 * but sleep slept to long and caused this to happen.  There is no
 * good way to work around this.
 *
 * timer is a timer structure that provides the clock to test.
 * dwTestLengthS is the test length is seconds (approximate: the user
 * should use the debug output for the real calculated test length).
 * dwNumberOfStatusStatements is the number of statements that should
 * be printed if the test is longer than 10 minutes.  This is
 * approximate and gives the test an idea of how to space the printing
 * of information in the first part of the test.
 *
 * return FALSE on error and TRUE on success.
 */
BOOL
wallClockDriftTest(sTimer * timer, 
           DWORD dwTestLengthS, 
           DWORD dwNumberOfStatusStatements);

#endif /* TIMER_ABSTRACTION_H */
