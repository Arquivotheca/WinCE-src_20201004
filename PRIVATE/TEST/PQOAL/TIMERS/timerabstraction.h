//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * timerAbstraction.h
 */

#ifndef __TIMER_ABSTRACTION_H
#define __TIMER_ABSTRACTION_H

#include <windows.h>
#include <assert.h>

#include "..\common\commonUtils.h"

/* for now we use the same numbers for each clock */



/* Sleep range.  Given in ms */
#define SLEEP_LOW_END_MS 100
#define SLEEP_HIGH_END_MS 5000

/*
 * approximately how long the inner loop should run.
 * Time given in seconds.  This must be a double value.
 */
#define INNER_LOOP_TIME_S 500.0

/*
 * If either clock has a real resolution worse than this value (for
 * example, it ticks each second and this value is set to 0.001 s),
 * the inner loop drift time is not checked.  We do this to avoid
 * issues with the precision of the real resolution.  Slow ticking clocks,
 * by their nature, round values, and these values introduce noise 
 * into the stern checks below.  The end result is false positives.
 *
 * If you change the SLEEP_*_END_MS constants this value might need 
 * to be changed as well.
 *
 * Currently set to catch all clocks that do better or equal to 10 ms
 * real resolution.  This should catch all clocks except for the RTC,
 * when it is ticking each second.
 */
#define INNER_LOOP_DONT_CHECK_THRESHOLD_S 0.010

/* 
 * The allowable drift for the inner loop.  This value is defined as
 * drift / total time.  The inner loop time is given in INNER_LOOP_TIME_S.
 *
 * This is independent of the DRIFT_OVERTIME_TOLERANCE_PERCENT so that
 * outliers can be caught if need be.  These outliers might be hid in
 * the average and stddev but will be caught by an absolute maximum
 * for the inner loop values.
 *
 * In general, this should be greater than or equal to the drift over
 * tolerance value.  If it isn't then it will be the limiting factor
 * on the drift over time.  In other words, the test will fail on this
 * value because at least some inner loop drift values will be larger
 * than it.  Use this value to catch outliers, not as the general
 * amount of drift allowed by the test.
 *
 * This value also is used to put an upper limit on each sleep call in the
 * inner loop.  If we are seeing that much on each call then something is
 * dreadfully wrong with the clocks.
 *
 * This should be set such that the resulting absolute value is greater than
 * the resolution of the least accurate clock on the system.
 *
 * For now, it is set to twice the value as the
 * DRIFT_OVERTIME_TOLERANCE_PERCENT number.  We haven't seen any
 * outliers that necessitate setting it tighter.  If these come up we
 * will revisit this value.
 */
#define INNER_LOOP_DRIFT_TOLERANCE_PERCENT 0.0004



/*
 * The allowable drift for the set of inner loop tests.  This value is
 * different from the INNER_LOOP_DRIFT_TOLERANCE, which is a hard and
 * fast limit on the drift seen in the inner loop.
 * DRIFT_OVERTIME_TOLERANCE_PERCENT refers to the average of all of
 * the inner loop runs.
 *
 * The value is calculated as the drift / total test length.  Using a 
 * percentage keeps it test length independent.
 *
 * If the tolerance set here equals INNER_LOOP_DRIFT_TOLERANCE, then
 * the tolerance check is redudant.  This constant is provided in case
 * the user wants to specify an absolute max on each value as well as
 * a max on the average.  Since the average will tend to smooth the data
 * only using it will tend to remove random noise that might be interesting.
 *
 * The value is given in decimal.  These values represent what
 * percentage drift is allowable (but not multiplied by 100).  1.0
 * represents 100%, which means that the clock drift is a value equal
 * to the length of the test.  This value can be above 1.0, but that
 * really doesn't make much sense.
 *
 * As an example, if a clock looses 0.54 s over an hour, the allowable drift
 * as calculated below is 0.54 s / (60 * 60) = 0.00015.
 * 
 * Currently, we use a drift of 0.75 s per hour, or a ratio of
 * 0.000208.  This comes from empirical observations.
 */
#define DRIFT_OVERTIME_TOLERANCE_PERCENT 0.000208

/*
 * number of times that the test calls the inner loop.  Each of these runs
 * is incorporated into the average and checked against the DRIFT_OVERTIME_* 
 * values
 */
#define OUTER_LOOP_ITERATIONS 30



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
 *                                Compare Two Clocks
 *
 ***************************************************************************/

/*
 * timer1 and timer2 must be initialized timer structures.
 *
 * This function returns TRUE if the tests are within bounds and FALSE
 * if they are not.  It prints out debug information concerning what 
 * it is doing.
 */
/*
 * note:  if the clock is going backwards we can catch this because the stddev
 * will go huge (assuming it isn't doing it all of the time.  The numbers will 
 * also be way to large (2^32).  This will raise flags in all but the most subtle
 * bugs
 */
BOOL
compareClock1ToClock2(sTimer * timer1, sTimer * timer2);

/*
 * one thing we can do here is confirm that the wanted drift values are within
 * the resolutions of the clocks.  We don't do that currently.
 * 
 * we assume that the user enters sane values.
 */
BOOL
initTimerConstants (sTimer * timer1,
		    sTimer * timer2,
		    sTimer *& timerHigherFreq,
		    sTimer *& timerLowerFreq,
		    double & doConversionLowerToHigherFreq,
		    double & doInnerAllowableDrift,
		    DWORD & dwInnerLoopIterations,
		    double & doLoosestResolution);

/*

 * timeHigherFreq is the timer with the higher freq.  timerLowerFreq
 * is the timer with the lower frequency.  Both timers can have the
 * same frequency.  
 *
 * doConversionLowerToHigherFreq is a conversion factor to convert
 * between the lower frequency clock and the higher frequency clock.
 * The factor fits the equation:
 *
 *    lowerFreq * doConversionLowerToHigherFreq = higherFreq
 *
 * doInnerAllowableDrift defines the maximum allowed drift for the
 * each iteration run by the loop in this function.  This function
 * calls compareClock1ToClock2InnerLoopOnce for dwNumberIterations.
 * doInnerAllowableDrift puts a maximum bound on these drift numbers.
 *
 * dwNumIterations provides the number of times that the 
 * compareClock1ToClock2InnerLoopOnce function is called.
 *
 * The return value for this function is FALSE on error and TRUE on
 * success.  doDriftInSeconds contains the total amount of drift seen
 * by this function in seconds and doTestLengthSeconds is the total
 * length of the test as seen by timerHigherFreq in seconds.  Note that,
 * if timerHigherFreq and timerLowerFreq drastically disagree, this 
 * value shouldn't be trusted.
 *
 * The driftInSeconds is calculated over the entire interval of the
 * test.  It isn't a sum of the drifts seen in the
 * compareClock1ToClock2InnerLoopOnce.  The drift numbers calculated
 * by compareClock1ToClock2InnerLoopOnce are not used in this function.
 */
BOOL
compareClock1ToClock2InnerLoop(
			       sTimer * timerHigherFreq,
			       sTimer * timerLowerFreq,
			       double doConversionLowerToHigherFreq,
			       double doInnerAllowableDrift,
			       DWORD dwNumIterations,
			       double & doDriftInSeconds,
			       double & doTestLengthSeconds,
			       double doLoosestResolution);

BOOL
compareHiPerfToGetTickCountOnce(sTimer * timerHigherFreq,
				sTimer * timerLowerFreq,
				double doConversionLowerToHigherFreq,
				double doInnerAllowableDrift,
				double doLoosestResolution);

/***************************************************************************
 * 
 *                          Get Latency and Resolution
 *
 ***************************************************************************/


BOOL getLatencyAndResolutionSmoothed (
				      sTimer * timer,
				      DWORD dwNumberOfIterations,
				      double & doGetTickCallsAve,
				      double & doGetTickCallsStdDev,
				      double & doResolutionAve,
				      double & doResolutionStdDev,
				      BOOL bPrintIterations);

/* 
 * a version that has the printing feature permanently turned off.  The only
 * printed information that will come from this function comes from error
 * messages on failure.  Success will produce no output.
 */
BOOL getLatencyAndResolutionSmoothed (
				      sTimer * timer,
				      DWORD dwNumberOfIterations,
				      double & doGetTickCallsAve,
				      double & doGetTickCallsStdDev,
				      double & doResolutionAve,
				      double & doResolutionStdDev);

/*
 * Calculates the resolution and the number of GetTickCount calls that
 * were made during this time.
 *
 * ullGetTickCalls returns the number of get tick calls made during
 * the time period given by ullResolution.  ullResolution is the
 * resolution of the timer clock.  The inbound values for both of
 * these parameters are ignored.
 *
 * This function returns TRUE on success and FALSE otherwise.  On
 * error, the value of dwGetTickCalls and dwResolution are undefined.
 */
BOOL getLatencyAndResolutionOnce (
				  sTimer * timer,
				  ULONGLONG & ullGetTickCalls,
				  double & doResolutionS);

/***************************************************************************
 * 
 *                                Miscellaneous 
 *
 ***************************************************************************/


/*
 * goal is to catch the timer on an interrupt firing and see if we can force
 * it backwards
 *
 * we say it goes backwards if the rollover is ever more that twice the resolution.
 */
BOOL
doesClockGoBackwards (sTimer * timer,
		      ULONGLONG ullIterations);



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
