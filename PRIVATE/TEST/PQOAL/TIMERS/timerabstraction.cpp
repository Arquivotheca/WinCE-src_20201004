//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * timerAbstraction.cpp
 */

#include "timerAbstraction.h"

/* 
 * This is only needed internally.  No need to export it to everyone
 * who includes timerAbstraction.h
 */
#include "statistics.h"

/*
 * number of iterations needed to get the resolution to a good enough 
 * granularity.  This is arbitrary.  So far 30 has been working fine.  The
 * resolution calculate function runs fast so we can make this larger if 
 * need be.
 *
 * This is different from test length for the resolution test.  That is 
 * defined separately so that the user can change it as need be.  We want
 * to keep this independent because it is expected that the user has verified
 * the resolution before running the tests that rely upon it.  If they don't
 * do this then all bets are off.
 */
#define GET_RESOLUTION_ITERATIONS 30

/***************************************************************************
 * 
 *                                  Initialization
 *
 ***************************************************************************/

/*
 * Initialize a timer data structure.  This involves setting the ullFreq and 
 * then calculation the multiplication factor to convert the timer values
 * into seconds.
 *
 * Returns TRUE on success and FALSE on error.
 */
BOOL
initTimerStruct (sTimer & timer)
{
  ULONGLONG ullFreq;

  if (((*(timer.getFreq))(ullFreq)) == FALSE)
    {
      return (FALSE);
    }

  timer.ullFreq = ullFreq;
  timer.doConversionToS = 1.0 / (double) ullFreq;
  
  return (TRUE);
}

/***************************************************************************
 * 
 *                                Compare Two Clocks
 *
 ***************************************************************************/

/*
 * Round a double value down to a given multiple.
 */
BOOL
roundDouble (double doNumber, double doThreshold, double * doAnswer)
{
  if (doThreshold == 0.0)
    {
      IntErr (_T("%S doThreshold == 0.0"), __FUNCTION__);
      return (FALSE);
    }

  ULONGLONG ullMult = (ULONGLONG) (doNumber / doThreshold);
  *doAnswer = ((double) ullMult) * doThreshold;

  return (TRUE);
}

/*
 * timer1 and timer2 must be initialized timer structures.
 *
 * This function returns TRUE if the tests are within bounds and FALSE
 * if they are not.  It prints out debug information concerning what 
 * it is doing.
 */
BOOL
compareClock1ToClock2(sTimer * timer1, sTimer * timer2)
{

  /*
   * assume no error until proven otherwise
   */
  BOOL returnVal = TRUE;

  /*
   * drift value and test length for a given inner loop run in seconds.
   * The test length is the getTickCount value (converted into seconds).
   * This is a good approximation of the test length, and if this value
   * varies greatly from the hi perf value then the other checks will
   * throw an error.  The user should realize that something is wrong
   * when the drift value is huge.
   */
  double doDriftInSeconds;	
  double doTestLengthSeconds;

  /*
   * store the values from the inner loop runs for later processing 
   */
  double doDriftValsSeconds[OUTER_LOOP_ITERATIONS];
  double doTestLengthValsSeconds[OUTER_LOOP_ITERATIONS];
  double doDriftPercentages[OUTER_LOOP_ITERATIONS];

  /*
   * The timer with the larger frequency is stored in timerHigherFreq.
   * If the timer frequencies are equal then this doesn't not matter.
   */
  sTimer * timerHigherFreq, * timerLowerFreq;

  /* 
   * values needed by the inner loop function.  These are constants converted 
   * into specific units.
   */
  double doConversionLowerToHigherFreq;
  double doInnerAllowDriftInHF;
  DWORD dwInnerLoopIterations;

  /*
   * The resolution of the least accurate clock
   */
  double doLoosestResolution;

  Info (_T("Initializing..."));

  /* initialize the values defined above */
  if (initTimerConstants (timer1, timer2, timerHigherFreq, timerLowerFreq,
			  doConversionLowerToHigherFreq, 
			  doInnerAllowDriftInHF,
			  dwInnerLoopIterations,
			  doLoosestResolution) == FALSE)
    {
      Error (_T("Could not initialize the timer constants."));
      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* print some debugging information */
  if (timerHigherFreq->ullFreq != timerLowerFreq->ullFreq)
    {
      Info (_T("Higher frequency timer is %s with a frequency of %I64u.  ")
	    _T("1 tick is %g s."),
	    timerHigherFreq->name, timerHigherFreq->ullFreq, 
	    timerHigherFreq->doConversionToS);
      Info (_T("Lower frequency timer is %s with a frequency of %I64u.  ")
	    _T("1 tick is %g s."),
	    timerLowerFreq->name, timerLowerFreq->ullFreq, 
	    timerLowerFreq->doConversionToS);
      Info (_T("Conversion factor from %s to %s is %g."),
	    timerLowerFreq->name, timerHigherFreq->name,
	    doConversionLowerToHigherFreq);
    }
  else
    {
      /* fequencies are equal */
      Info (_T("Both timers (%s and %s) have the same frequency of of %I64u.  ")
	 _T("1 tick is %g s."),
	 timerHigherFreq->name, timerLowerFreq->name,
	 timerHigherFreq->ullFreq, timerHigherFreq->doConversionToS);

      assert (doConversionLowerToHigherFreq == 1.0);
    }

  Info (_T("The worst resolution (not frequency) between these clock is %g s."),
	doLoosestResolution);

  /* number of times this function runs the inner loop.  This variable
   * determines the number of data points collected for the averages below.
   */
  DWORD dwNumIterations = OUTER_LOOP_ITERATIONS;

  Info (_T("The outer loop is going to run for %u iterations."),
	dwNumIterations);

  /* for each run of the inner loop */
  for (DWORD dwIterations = 0; dwIterations < dwNumIterations; dwIterations++)
    {
      /*
       * get the drift and test length for a run of the inner loop
       */
      if (compareClock1ToClock2InnerLoop(timerHigherFreq, timerLowerFreq,
					 doConversionLowerToHigherFreq, 
					 doInnerAllowDriftInHF,
					 dwInnerLoopIterations,
					 doDriftInSeconds, doTestLengthSeconds,
					 doLoosestResolution) == FALSE)
	{
	  Debug (_T("Inner loop failed.  Iteration is %u."), dwIterations);

	  returnVal = FALSE;
	  goto cleanAndExit;
	}
      
      /* round to the loosest resolution on the system */
      roundDouble (doDriftInSeconds, doLoosestResolution, &doDriftInSeconds);

      /*
       * make sure that this drift is within the given tolerances.  We
       * don't compare to error here because we only have one value
       * and, by definition, it can't have error.
       */
      if (doDriftInSeconds > 
	  (doInnerAllowDriftInHF * timerHigherFreq->doConversionToS))
	{
	  /* drift is too large */
	  Debug (_T("Drift for the inner loop is too large.  Iteration is %u.  ")
		 _T("Drift is %g s.  Test length is %g s."),
		 dwIterations, doDriftInSeconds, doTestLengthSeconds);

	  /* fail and bail out at this point */
	  returnVal = FALSE;
	  goto cleanAndExit;
	}

      /*
       * If we get to this point the drift for this inner loop run is
       * within tolerances
       */

      /* save the drift value and test length for later processing */
      doDriftValsSeconds[dwIterations] = doDriftInSeconds;
      doTestLengthValsSeconds[dwIterations] = doTestLengthSeconds;
      doDriftPercentages[dwIterations] = 
	doDriftInSeconds / doTestLengthSeconds;

#ifdef PRINT_DEBUG_VV
      /* some debug spew to see what it is doing */
      Info (_T("iter %u \tdrift = %.10g s \ttest length = %g s \t ")
	     _T("%% drift = %.10g"),
	     dwIterations, doDriftInSeconds, doTestLengthSeconds,
	     doDriftPercentages[dwIterations]);
#endif
    }

  /*
   * calculate the drift for the entire set of values.
   *
   * We check to make sure that the average drift is not above a given
   * threshold.  This will catch both drift that is accelarating
   * (assuming it accelerates fast enough to be within outside of the
   * threshold) and drift that is just too high period.
   *
   * It is possible to detect slowly increasing drift values, but if these are 
   * inside of the bounds specified above it is questionable whether we should
   * fail the test.  If these are deemed problematic then it is best to tighten
   * the bounds on the test.  As long as the values are within the tolerances
   * that we allow the test passes.
   */

  double doDriftAverage;
  double doDriftStddev;

  if (CalcStatisticsFromList(doDriftPercentages, dwNumIterations,
			     doDriftAverage, doDriftStddev) == FALSE)
    {
      /* fail */
      Debug (_T("CalcStatisticsFromList failed."));

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /*
   * check the average
   *
   * doLoosestResolution / (double) INNER_LOOP_TIME_S is the smallest % change
   * we can detect.  Normally this is less than DRIFT_OVERTIME_TOLERANCE_PERCENT.
   * If the clock is particularly granular (like rtc ticking at 1 sec), then
   * this will be greater than the allowable drift.  This signifies that we
   * don't have the resolution that we need, and this statement prevents false
   * positives in this case.
   */
  if ((doDriftAverage > DRIFT_OVERTIME_TOLERANCE_PERCENT) &&
      (doDriftAverage > (doLoosestResolution / (double) INNER_LOOP_TIME_S)))
    {
      Error (_T("The drift is outside of the allowable tolerance.  The ")
	     _T("drift is %g and the allowable drift is %g."),
	     doDriftAverage, DRIFT_OVERTIME_TOLERANCE_PERCENT);
      Error (_T("The smallest drift we can detect on this clock is %g percent."),
	     doLoosestResolution / (double) INNER_LOOP_TIME_S);
      
      returnVal = FALSE;
      goto cleanAndExit;
    }

  Debug (_T("The average drift is %g.  This is a percentage change overtime."),
	 doDriftAverage);

 cleanAndExit:
  return (returnVal);
}
  

/*
 * Initialize the constants needed for calculating whether the timers
 * are within the given tolerances.
 *
 *
 * we assume that the user enters sane values for the #defines above.
 * We do some error checking but don't do it exhaustively.
 *
 * timer1 and timer2 are the timers that we will be comparing.  If
 * this function executes sucessfully then the following return values
 * are set:
 *
 * timerHighFreq points to the timer with the larger freq.  This isn't
 * not necessarily the timer with the highest resolution.  It is the
 * timer with the higher precision.
 *
 * timerLowerFreq points to the timer with the smaller freq value.
 *
 * doConversionLowerToHigherFreq contains the value needed to convert
 * frequency values from the lower freq timer to the higher freq
 * timer.  The conversion is highFreq = doConvsionLowerToHigherFreq *
 * lowerFreq
 *
 * doInnerAllowableDrift is a maximum allowed drift for the inner loop
 * runs.  This is different from the allowable drift on the entire
 * data set (and entire test run).
 *
 * dwInnerLoopIterations is the number of inner loop iterations that
 * should be completed to approximately get the inner loop test length
 * that the user defined.  This is calculated from the average of the
 * possible sleep range and the length that the user would like the
 * inner loop to run.
 *
 * doLoosesetResolution is the resolution of the of the worst clock on
 * the system.  This differs from the timerHigherFreq/timerLowerFreq
 * ullFreq values because these represent _possible_ precisions; not
 * what the clocks can actually do.  The doLoosestResolution value is
 * calculated for the platform and returned.
 *
 * Return TRUE on success and FALSE otherwise.  On error this function
 * prints what went wrong.
 */
BOOL
initTimerConstants (sTimer * timer1, 
		    sTimer * timer2,
		    sTimer *& timerHigherFreq,
		    sTimer *& timerLowerFreq,
		    double & doConversionLowerToHigherFreq,
		    double & doInnerAllowableDrift,
		    DWORD & dwInnerLoopIterations,
		    double & doLoosestResolution)
{
  BOOL returnVal = TRUE;

  /******* initialize doLoosestResolution *********************************/
  
  /* find the resolution of both timers */
  double doT1GetTickCallsAve, doT1GetTickCallsStddev, 
    doT1ResAve, doT1ResStddev;
  double doT2GetTickCallsAve, doT2GetTickCallsStddev, 
    doT2ResAve, doT2ResStddev;
  
  if (getLatencyAndResolutionSmoothed(timer1, GET_RESOLUTION_ITERATIONS,
				      doT1GetTickCallsAve, 
				      doT1GetTickCallsStddev, doT1ResAve, 
				      doT1ResStddev) == FALSE)
    {
      Error (_T("Could not get resolution of clock %s."),
	     timer1->name);
      returnVal = FALSE;
      goto cleanAndExit;
    }

  if (getLatencyAndResolutionSmoothed(timer2, GET_RESOLUTION_ITERATIONS,
				      doT2GetTickCallsAve, 
				      doT2GetTickCallsStddev, doT2ResAve, 
				      doT2ResStddev) == FALSE)
    {
      Error (_T("Could not get resolution of clock %s."),
	     timer2->name);
      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* 
   * we only need the resolution numbers.  Ignore the stddev for now.  This 
   * will be checked in another test.
   */

  /* find the max */
  if (doT1ResAve > doT2ResAve)
    {
      doLoosestResolution = doT1ResAve;
    }
  else
    {
      doLoosestResolution = doT2ResAve;
    }

  /* if the loosest resolution is zero then something went wrong somewhere */
  if (doLoosestResolution == 0.0)
    {
      Error (_T("The calculated resolution is zero.  The clock must not be"));
      Error (_T("ticking, which shouldn't occur."));
      returnVal = FALSE;
      goto cleanAndExit;
    }
  
  /******* initialize timerHigherFreq and timerLowerFreq ******************/

  /*
   * figure out which clock has a higher frequency.
   *
   * This concerns possibly accuracy, not absolute accuracy.  Just because
   * a clock can report down to the ns doesn't mean that it actually has that
   * resolution in real life.
   *
   * This code is looking for the largest frequency number.
   */
  if (timer1->ullFreq  > timer2->ullFreq)
    {
      timerHigherFreq = timer1;
      timerLowerFreq = timer2;
    }
  else
    {
      timerHigherFreq = timer2;
      timerLowerFreq = timer1;
    }

  /******* initialize doInnerAllowableDrift **************************/

  /* convert the allowable drift into the units used by the 
   * higher freq clock */
  double doInnerAllowableDriftS = 
    INNER_LOOP_DRIFT_TOLERANCE_PERCENT * (double) INNER_LOOP_TIME_S;

  if (doInnerAllowableDriftS < doLoosestResolution)
    {
      Info (_T("Adjusting allowable drift for inner loop because it is"));
      Info (_T("inside the loosest resolution.  it is currently %g s"),
	    doInnerAllowableDriftS);
      doInnerAllowableDriftS = doLoosestResolution;
      Info (_T("it became %g s"), doLoosestResolution);

    }

  doInnerAllowableDrift = 
    doInnerAllowableDriftS * (double) timerHigherFreq->ullFreq;

  /******* initialize doConversionLowerToHigherFreq ***********************/

  /* 
   * compute the conversion factor to convert from the lower frequency clock
   * to the higher freq clock.  This is a multiplicative factor:
   *
   * higher freq = doConversionLowerToHigherFreq * lower freq
   */
  doConversionLowerToHigherFreq = (double) timerHigherFreq->ullFreq / 
    (double) timerLowerFreq->ullFreq;

  /******* initialize dwInnerLoopIterations *******************************/

  PREFAST_SUPPRESS (326, "correct");
  if (SLEEP_HIGH_END_MS <= SLEEP_LOW_END_MS)
    {
      /* error */
      Error (_T("Constants are incorrect."));
      Error (_T("SLEEP_HIGH_END_MS <= SLEEP_LOW_END_MS"));
      
      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* 
   * use this method to find the average since it provides no possibility of 
   * overflow.  If b > a, then ave = (b - a) / 2 + a
   * compute the value in ms, and then devide by 1000 to convert it to seconds
   */
  double dwAveSleepTimeS = 
    (((double) (SLEEP_HIGH_END_MS - SLEEP_LOW_END_MS)) / 2.0 + 
     (double) SLEEP_LOW_END_MS) / 1000.0;
  
  dwInnerLoopIterations = (DWORD) (INNER_LOOP_TIME_S / dwAveSleepTimeS);

 cleanAndExit:
  
  return (returnVal);
}



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
 * length of the test as seen by timerHigherFreq in seconds.  Note
 * that, if timerHigherFreq and timerLowerFreq drastically disagree,
 * this value shouldn't be trusted.
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
			       double doLoosestResolution)
{
  /* assume success until proven otherwise */
  BOOL returnVal = TRUE;

  ULONGLONG ullHigherBeginTime, ullHigherEndTime, 
    ullLowerBeginTime, ullLowerEndTime;

  /* query the clocks to establish the beginning time */
  if ((*timerHigherFreq->getTick)(ullHigherBeginTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timerHigherFreq->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* query the clocks to establish the beginning time */
  if ((*timerLowerFreq->getTick)(ullLowerBeginTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timerLowerFreq->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /*
   * run the individual tests, trying to force clock skew/drift to occur 
   */
  for (DWORD dwIterations = 0; dwIterations < dwNumIterations; dwIterations++)
    {
      if (compareHiPerfToGetTickCountOnce(timerHigherFreq, timerLowerFreq,
					  doConversionLowerToHigherFreq,
					  doInnerAllowableDrift,
					  doLoosestResolution) == FALSE)
	{
	  Debug (_T("compareClock1ToClock2Once failed on iteration %u"),
		 dwIterations);

	  returnVal = FALSE;
	  goto cleanAndExit;
	}
    }

  /* 
   * when we are done with the individual tests, make sure that the clock
   * hasn't drifted too much overall.
   */

  /* query the clocks to establish the ending time */
  if ((*timerHigherFreq->getTick)(ullHigherEndTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timerHigherFreq->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* query the clocks to establish the ending time */
  if ((*timerLowerFreq->getTick)(ullLowerEndTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timerLowerFreq->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }
  
  /* find differences, accounting for overflow */
  ULONGLONG ullTimerHigherDiff;
  if (AminusB (ullTimerHigherDiff, ullHigherEndTime, ullHigherBeginTime,
	       timerHigherFreq->ullWrapsAt) == FALSE)
    {
      Error (_T("Failed to find difference for higher freq clock.  Operation ")
	     _T("is %I64u - %I64u."),
	     ullHigherBeginTime, ullHigherEndTime);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  ULONGLONG ullTimerLowerDiff;
  if (AminusB (ullTimerLowerDiff, ullLowerEndTime, ullLowerBeginTime,
	       timerLowerFreq->ullWrapsAt) == FALSE)
    {
      Error (_T("Failed to find difference for higher freq clock.  Operation ")
	     _T("is %I64u - %I64u."),
	     ullLowerBeginTime, ullLowerEndTime);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /*
   * convert the differences into doubles.  We shouldn't loose a lot of 
   * accuracy between we don't need the less significant parts of the values.
   */
  double doHigherFreqDiff = (double) ullTimerHigherDiff;

  /* 
   * convert the lower freq clock to the high frequency units
   */
  double doLowerFreqDiff = 
    (double) ullTimerLowerDiff * doConversionLowerToHigherFreq;

  /* get the observed drift */
  if (doHigherFreqDiff < doLowerFreqDiff)
    {
      doDriftInSeconds = (doLowerFreqDiff - doHigherFreqDiff) *
	timerHigherFreq->doConversionToS;
    }
  else
    {
      doDriftInSeconds = (doHigherFreqDiff - doLowerFreqDiff) *
	timerHigherFreq->doConversionToS;
    }

  /*
   * calculate the test length in seconds.  If the two clocks vary greatly,
   * the caller which catch this as an error, so using only one clock for 
   * the test length isn't too much of a problem.
   */
  doTestLengthSeconds = doHigherFreqDiff * timerHigherFreq->doConversionToS;

 cleanAndExit:

  return (returnVal);

}


BOOL
compareHiPerfToGetTickCountOnce(sTimer * timerHigherFreq,
				sTimer * timerLowerFreq,
				double doConversionLowerToHigherFreq,
				double doInnerAllowableDrift,
				double doLoosestResolution)
{
  /* assume success until proven otherwise */
  BOOL returnVal = TRUE;

  /* invalid parameters */
  if (doConversionLowerToHigherFreq <= 0)
    { 
      returnVal = FALSE;
      goto cleanAndExit;
    }

  ULONGLONG ullHigherBeginTime, ullHigherEndTime, 
    ullLowerBeginTime, ullLowerEndTime;

  /* figure out how long to sleep */
  DWORD dwSleepTime = getRandomInRange (SLEEP_LOW_END_MS, SLEEP_HIGH_END_MS);

  /* query the clocks to establish the beginning time */
  if ((*timerHigherFreq->getTick)(ullHigherBeginTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timerHigherFreq->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* query the clocks to establish the beginning time */
  if ((*timerLowerFreq->getTick)(ullLowerBeginTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timerLowerFreq->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* 
   * sleep doesn't have to be accurate for this to work.  The clocks
   * are being compared against each other, not the sleep function.
   */
  Sleep (dwSleepTime);

  /* query the clocks to establish the ending time */
  if ((*timerHigherFreq->getTick)(ullHigherEndTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timerHigherFreq->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* query the clocks to establish the ending time */
  if ((*timerLowerFreq->getTick)(ullLowerEndTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timerLowerFreq->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* find differences, accounting for overflow */
  ULONGLONG ullTimerHigherDiff;
  if (AminusB (ullTimerHigherDiff, ullHigherEndTime, ullHigherBeginTime,
	       timerHigherFreq->ullWrapsAt) == FALSE)
    {
      Error (_T("Failed to find difference for higher freq clock.  Operation ")
	     _T("is %I64u - %I64u."),
	     ullHigherBeginTime, ullHigherEndTime);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  ULONGLONG ullTimerLowerDiff;
  if (AminusB (ullTimerLowerDiff, ullLowerEndTime, ullLowerBeginTime,
	       timerLowerFreq->ullWrapsAt) == FALSE)
    {
      Error (_T("Failed to find difference for higher freq clock.  Operation ")
	     _T("is %I64u - %I64u."),
	     ullLowerBeginTime, ullLowerEndTime);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /*
   * convert the differences into doubles.  We shouldn't loose a lot of 
   * accuracy between we don't need the less significant parts of the values.
   */
  double doHigherFreqDiff = (double) ullTimerHigherDiff;

  /* 
   * convert the lower freq clock to the high frequency units
   */
  double doLowerFreqDiff = 
    (double) ullTimerLowerDiff * doConversionLowerToHigherFreq;

  /* drift in higher freq clock */
  double doDrift;

  /* get the observed drift */
  if (doHigherFreqDiff < doLowerFreqDiff)
    {
      doDrift = (doLowerFreqDiff - doHigherFreqDiff);
    }
  else
    {
      doDrift = (doHigherFreqDiff - doLowerFreqDiff);
    }
  
#ifdef PRINT_DEBUG_VVV
  Info (_T("Innerloop: allowable drift is %g  actual drift is %g.")
	      _T("1 tick = %g s"), 
	      doInnerAllowableDrift, doDrift, timerHigherFreq->doConversionToS);
#endif
  /*
   * The second part confirms that we have enough real resolution in the
   * given clocks to remove noise from this check.  Since we are sleeping
   * at most 5 seconds (SLEEP_HIGH_END_MS) and clocks can have up to a 
   * resolution of 1 second the rounding errors will cause false positives
   * (especially with tight bounds (small doInnerAllowableDrift)).  So, 
   * we put a limit on how bad the clocks can be before we just disallow
   * this check.
   */
  if ((doDrift > doInnerAllowableDrift) && 
      (doLoosestResolution <= INNER_LOOP_DONT_CHECK_THRESHOLD_S))
    {
      /* drift is too large */
      /* doLowerFreq is in higher freq units, so use higher freq conv 
       * in last line */
      Debug (_T("Drift between %s and %s is too ")
	     _T("large.  %s returned %g s and %s ")
	     _T("returned %g s. "),
	     timerHigherFreq->name, timerLowerFreq->name,
	     timerHigherFreq->name, 
	     doHigherFreqDiff * timerHigherFreq->doConversionToS,
	     timerLowerFreq->name,
	     doLowerFreqDiff * timerHigherFreq->doConversionToS);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* 
   * if we made it here then the clocks are within acceptable drift
   * parameters.
   */

cleanAndExit:

  /* no cleaning to be done for this function */

  return (returnVal);
}




/***************************************************************************
 * 
 *                          Get Latency and Resolution
 *
 ***************************************************************************/

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
				      double & doResolutionStdDev)
{
  return (getLatencyAndResolutionSmoothed(timer,
					  dwNumberOfIterations,
					  doGetTickCallsAve,
					  doGetTickCallsStdDev,
					  doResolutionAve,
					  doResolutionStdDev,
					  FALSE));
}

/* 
 * doResolution is in seconds
 */
BOOL getLatencyAndResolutionSmoothed (
				      sTimer * timer,
				      DWORD dwNumberOfIterations,
				      double & doGetTickCallsAve,
				      double & doGetTickCallsStdDev,
				      double & doResolutionAve,
				      double & doResolutionStdDev,
				      BOOL bPrintIterations)
{
  /* be hopeful until proven otherwise */
  BOOL returnVal = TRUE;

  /*
   * variables to store the statistics for each run.  The statistics currently
   * aren't processed on the fly.
   *
   * If these are null they haven't been allocated.  If they aren't null the memory
   * has been allocated.  Null, is used to track error conditions in the code below.
   */
  ULONGLONG * calls = NULL;
  double * res = NULL;

  DWORD dwCallsBytes = dwNumberOfIterations * sizeof (*calls);
  DWORD dwResBytes = dwNumberOfIterations * sizeof (*res);

  if ((dwCallsBytes < MAX(dwNumberOfIterations, sizeof (*calls))) ||
      (dwResBytes < MAX(dwNumberOfIterations, sizeof (*res)))) 
    {
      /* overflow */
      Error (_T("Number of iterations is too large.  Memory allocation will ")
	     _T("overflow DWORD."));
      returnVal = FALSE;
      goto cleanAndReturn;
    }
	     
  /* 
   * get memory
   */
  calls = (ULONGLONG *) malloc (dwCallsBytes);
  res = (double *) malloc (dwResBytes);

  if ((calls == NULL) || (res == NULL))
    {
      returnVal = FALSE;
      goto cleanAndReturn;
    }

  /* loop through the given number of iterations */
  for (DWORD iterations = 0; iterations < dwNumberOfIterations; iterations++)
    {
      ULONGLONG dwNumOfCalls;
      double doResolution;

      if (getLatencyAndResolutionOnce (timer, dwNumOfCalls, doResolution) ==
	  FALSE)
	{
	  Debug (TEXT("ERROR: Something is wrong with ")
		 TEXT("getLatencyAndResolutionOnce: iteration ")
		 TEXT("is %u."), iterations);
	}

      calls[iterations] = dwNumOfCalls;
      res[iterations] = doResolution;
    }


  /* print out the data for each iteration. */
  if (bPrintIterations)
    {
      for (DWORD iterations = 0; iterations < dwNumberOfIterations; iterations++)
	{
	  Info (_T("INFO: For iteration %u it took %I64u calls to the get tick ")
		_T("function and yielded a resolution of %g s."),
		iterations, calls[iterations], res[iterations]);
	}
    }

  if (CalcStatisticsFromList (calls, dwNumberOfIterations, doGetTickCallsAve, 
			      doGetTickCallsStdDev) == FALSE)
    {
      Debug(_T("ERROR: Couldn't get stats for calls."));
      returnVal = FALSE;
      goto cleanAndReturn;
    }

  if (CalcStatisticsFromList (res, dwNumberOfIterations, doResolutionAve, 
			      doResolutionStdDev) == FALSE)
    {
      Debug(_T("ERROR: Couldn't get stats for resolutions."));
      returnVal = FALSE;
      goto cleanAndReturn;
    }

 cleanAndReturn:
  
  if (calls != NULL)
    free (calls);

  if (res != NULL)
    free (res);

  return (returnVal);
}


/*
 * Calculates the resolution and the number of GetTickCount calls that were
 * made during this time.  
 *
 * The first while loop forces the code to a point right after the timer has been
 * changed.  This means that we have the entire next period to make calculate the
 * number of get tick calls that can be made.  The next loop actually calcualates
 * the number of get tick calls and the resolution.
 */
BOOL getLatencyAndResolutionOnce (
				  sTimer * timer,
				  ULONGLONG & ullGetTickCalls,
				  double & doResolutionS)
{
  /* be hopeful until proven otherwise */
  BOOL returnVal = TRUE;

  ULONGLONG ullStartTime, ullEndTime;

  /*
   * track how many times getTickCount was called 
   * The first calls occurs before we are in the while loop (below), so start this
   * at one instead of zero
   */
  ullGetTickCalls = 1;

  /*
   * handle to the current thread.  This doesn't need to be 
   * closed when processing is complete.
   */
  HANDLE hThisThread = GetCurrentThread ();

  /* return value from the getTick function in timer */
  BOOL bGetTickReturnVal;

  /* get the quantum so that we can set this back */
  DWORD dwOldThreadQuantumMS = CeGetThreadQuantum (hThisThread);

  if (dwOldThreadQuantumMS == MAXDWORD)
    {
      Error (_T("Couldn't get the thread quantum.  GetLastError is %u."),
	     GetLastError ());
      returnVal = FALSE;
      goto cleanAndExit;
    }

  //  Info (_T("Setting the thread quantum to 0.  We will run until we block."));

  /* set the thread quantum to 0, meaning that we will never be swapped out */
  if (!CeSetThreadQuantum (hThisThread, 0))
    {
      Error (_T("Couldn't set the thread quantum.  GetLastError is %u."),
	     GetLastError ());
      returnVal = FALSE;
      goto cleanAndExit;
    }

  /*
   * At this point the quantum is set such that we will run until we
   * block.
   */

  /* 
   * put this process at the beginning of the quantum
   * (CeSetThreadQuantum takes affects on the next scheduling cycle).
   */
  Sleep (0);

  /* 
   * loop until the time changes, meaning that an interrupt has fired
   * and the timer has been serviced.  Quantums are generally at least
   * 25 ms, and since the timer should be updated every 1 ms this won't
   * introduce too much noise.
   */

  /* get the timer to start at */
  if ((*timer->getTick)(ullEndTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timer->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* loop until we get an error or the time changes */  
  while (((bGetTickReturnVal = (*timer->getTick)(ullStartTime)) == TRUE) &&
	 (ullStartTime == ullEndTime))
    {
    }
  
  if (bGetTickReturnVal == FALSE)
    {
      /* the loop above failed because of a bad call to get getTick */
      Error (_T("Failed to get tick count for %s in initialization loop."),
	     timer->name);

      returnVal = FALSE;
      goto cleanAndExit; 
    }

  /*
   * ullStartTime is initialized in the previous loop.
   * We should be at the beginning of the quantum.
   */

  /* 
   * loop until the time changes, meaning that an interrupt has fired
   * and the timer has been serviced, or we have seen an error on the
   * get tick call.
   */
  while (((bGetTickReturnVal = (*timer->getTick)(ullEndTime)) == TRUE) &&
	 (ullStartTime == ullEndTime))
    {
      ullGetTickCalls ++;
    }
  
  if (bGetTickReturnVal == FALSE)
    {
      /* the loop above failed because of a bad call to get getTick */
      Error (_T("Failed to get tick count for %s in main loop."),
	     timer->name);

      returnVal = FALSE;
      goto cleanAndExit; 
    }

  /*
   * the resolution is the value between these two clocks
   */
  ULONGLONG ullRes;
  
  if (AminusB (ullRes, ullEndTime, ullStartTime, timer->ullWrapsAt) == FALSE)
    {
      returnVal = FALSE;
      goto cleanAndExit;
    }

  doResolutionS = ((double) ullRes) * timer->doConversionToS;

 cleanAndExit:

  if (dwOldThreadQuantumMS != MAXDWORD)
    {
      /* we have to set it back */
      if (!CeSetThreadQuantum (hThisThread, dwOldThreadQuantumMS))
	{
	  Error (_T("Couldn't set the thread quantum back to the original value."));
	  Error (_T("GetLastError is %u."), GetLastError ());
	  returnVal = FALSE;
	}
    }

  return (returnVal);
}





/***************************************************************************
 * 
 *                          Backwards Checks
 *
 ***************************************************************************/

/*
 * How many iterations to run the resolution check for the backwards checks.
 */
#define BACKWARDS_LATENCY_AND_RESOLUTION_MAX_ITERATIONS 30


/*
 * goal is to catch the timer on an interrupt firing and see if we can force
 * it backwards.
 *
 * we say it goes backwards if the rollover is ever more that twice the resolution.
 *
 * timer is the timer to work with, which can't be null.  ullRunTimeS
 * is the run time in seconds.
 */
BOOL
doesClockGoBackwards (sTimer * timer,
		      ULONGLONG ullRunTimeS)
{

  /* be hopeful until proven otherwise */
  BOOL returnVal = TRUE;

  double doLoopsAve, doLoopsStdDev, doResolutionAve, doResolutionStdDev;
  
  /*
   * call the non-printing version of this function.  We only want error messages
   * to go the log.
   */
  if (getLatencyAndResolutionSmoothed (timer, 
				       BACKWARDS_LATENCY_AND_RESOLUTION_MAX_ITERATIONS,
				       doLoopsAve, doLoopsStdDev, 
				       doResolutionAve, doResolutionStdDev) == FALSE)
    {
      Error (_T("Could not get latency and resolution of %s."),
	     timer->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }      

  /*
   * for now, we assume that the user has confirmed that the clock isn't within
   * bounds for noise.  
   */
  Info (_T("Resolution of %s is %g +- %g s.  We can do roughly %g +- %g loops ")
	_T("in this time period."),
	timer->name, doResolutionAve, doResolutionStdDev, 
	doLoopsAve, doLoopsStdDev);

  /*
   * use the resolution and number of loops to figure out how many iterations
   * need to be done to run the given test run time.
   */
  ULONGLONG ullIterations =   
    (ULONGLONG)((double) ullRunTimeS * (doLoopsAve / doResolutionAve));

  Info (_T("This test should run for about %g seconds (%g minutes)."),
	(double) ullRunTimeS, (double) ullRunTimeS / 60.0);

  /*
   * set the allowable difference to be twice the resolution.  This is completely
   * arbitrary.
   */
  ULONGLONG ullAllowableDifference = 
    (ULONGLONG) ((2.0 * doResolutionAve) / timer->doConversionToS);

  /* zero is the smallest value, even with roll over */
  ULONGLONG ullPrevTickCount = 0;
  ULONGLONG ullCurrTickCount;

  for (ULONGLONG ullCurrent = 0; ullCurrent < ullIterations; ullCurrent++)
    {
      if ((*timer->getTick)(ullCurrTickCount) == FALSE)
	{
	  Error (_T("Failed to get tick count in doesClockGoBackwards for %s.")
		 _T("Iteration is %I64u."),
		 timer->name, ullCurrent);

	  returnVal = FALSE;
	  goto cleanAndExit;
	}

      if (ullCurrTickCount < ullPrevTickCount)
	{
	  /* was it overflow? */
	  ULONGLONG ullDifference;
	  if (AminusB (ullDifference, ullCurrTickCount, ullPrevTickCount, 
		       timer->ullWrapsAt) == FALSE)
	    {
	      Error (_T("Failed on AminusB: %I64u - %I64u."),
		     ullCurrTickCount, ullPrevTickCount);
	      returnVal = FALSE;
	      goto cleanAndExit;
	    }

	  /* 
	   * arbitrary: if larger than two times the resolution we have problems
	   * less than this is probably rollover (or a really nasty ).
	   */
	  if (ullDifference > ullAllowableDifference)
	    {
	      Debug (_T("%s went backward.  Previous value ")
		     _T("is %I64u and next value is %I64u. ")
		     _T("Permitted difference is %I64u."),
		     timer->name,
		     ullPrevTickCount, ullCurrTickCount, ullAllowableDifference);
	      
	      returnVal = FALSE;
	      
	    }
	  
	  /* at this point we assume that this was caused by timer rollover */
	}
      
      ullPrevTickCount = ullCurrTickCount;
    }

 cleanAndExit:

  return (returnVal);
}


/***************************************************************************
 * 
 *                          Wall Clock Check
 *
 ***************************************************************************/

#define READ_MESSAGE_TIME_S 20

#define TEN_MINUTES_IN_S (10 * 60)
#define THIRTY_SECONDS_IN_S (30)

/*
 * An abstraction of the system sleep function that supports the
 * different units associated with each timer.
 *
 * timer is a timer structure and ullSleepTime is the time to sleep in
 * the units given in the timer structure.
 */
void
abstractSleep (sTimer * timer, ULONGLONG ullSleepTime)
{
  if (timer == NULL)
    {
      IntErr (_T("abstractSleep: timer == NULL"));
      return;
    }

  Sleep ((DWORD)((double) ullSleepTime * (timer->doConversionToS * 1000.0)));
}

/*
 * Please see header for more info.
 */
BOOL
wallClockDriftTest(sTimer * timer, 
		   DWORD dwTestLengthS, 
		   DWORD dwNumberOfStatusStatements)
{
  if (timer == NULL)
    {
      IntErr (_T("wallClockDriftTest: timer == NULL"));
      return (FALSE);
    }

  if (dwTestLengthS == 0)
    {
      IntErr (_T("wallClockDriftTest: dwTestLengthS == 0"));
      return (FALSE);
    }

  if (dwNumberOfStatusStatements == 0)
    {
      IntErr (_T("wallClockDriftTest: dwNumberOfStatusStatements == 0"));
      return (FALSE);
    }


  /* assume true until proven otherwise */
  BOOL returnVal = TRUE;

  /* for use with realisticTime */
  double doPrtTime;
  _TCHAR * szPrtUnits;

  /* set up values for different time periods */
  ULONGLONG ullConstTenMinutes = timer->ullFreq * 60 * 10;
  ULONGLONG ullConstOneMinute = timer->ullFreq * 60;
  ULONGLONG ullConstThirtySeconds = timer->ullFreq * 30;
  ULONGLONG ullConstOneSecond = timer->ullFreq;

  /* convert the test length from seconds into the units in the timer */
  ULONGLONG ullTestLength = (ULONGLONG) dwTestLengthS * timer->ullFreq;

  if (ullTestLength < MAX(dwTestLengthS, timer->ullFreq))
    {
      /* overflow */
      Error (_T("Test length overflowed.  Either test is too long or timer freq"));
      Error (_T("is too high."));
      returnVal = FALSE;
      goto cleanAndReturn;
    }

  /* info message */
  Info (_T("This test allows you to compare clock %s with an external wall "),
	timer->name);
  Info (_T("clock.  This allows you to verify that the clocks in the "));
  Info (_T("and OAL are not drifting drastically.  It won't pick up slight"));
  Info (_T("variations (it is limited by how fast you can click a stop watch"));
  Info (_T("button), but will give a good idea of if your timers are grossly")
	_T("off."));
  Info (_T(""));

  /* don't print more than one statement each second. */
  if (dwTestLengthS < dwNumberOfStatusStatements)
    {
      dwNumberOfStatusStatements = dwTestLengthS;
    }

  /* 
   * find the iteration that will get us approximately the number of sleep
   * statements requested.
   */
  ULONGLONG ullSleepIterationS = 
    (ULONGLONG) ((double) dwTestLengthS / (double) dwNumberOfStatusStatements);

  /* 
   * don't want the messages outside of the 10 minute mark to come quicker than
   * the messages between 10 minutes and 30 seconds.
   */
  if (ullSleepIterationS < TEN_MINUTES_IN_S)
    {
      ullSleepIterationS = TEN_MINUTES_IN_S;
    }

  /* can't overflow since ullSleepIteration < dwTestLengthS */
  ULONGLONG ullSleepIteration = ullSleepIterationS * timer->ullFreq;

  /* print out test directions */
  realisticTime (double (ullSleepIteration) * timer->doConversionToS, 
		 doPrtTime, &szPrtUnits);
  Info (_T("We will print status messages so that you will have some warning"));
  Info (_T("and can stop timing at the correct moment.  These status messages"));
  Info (_T("will become more frequent as the test nears completion."));
  
  /* 
   * the below isn't exactly correct (for 60 s case actually off by 30 seconds)
   * but it gets the point across.  We want to give them a vague idea.
   */
  if (dwTestLengthS > TEN_MINUTES_IN_S)
    {
      Info (_T("Your first messages will occur about every %g %s."),
	    doPrtTime, szPrtUnits);
    }
  else if (dwTestLengthS > THIRTY_SECONDS_IN_S)
    {
      Info (_T("Your first messages will occur about every minute."));
    }
  else
    {
      Info (_T("Your first message will occur every second."));
    }
  Info (_T(""));

  realisticTime (double (dwTestLengthS), doPrtTime, &szPrtUnits);
  Info (_T("This test will run for %g %s."), doPrtTime, szPrtUnits);
  Info (_T(""));

  Info (_T("Waiting %u seconds for you to read this and to get ready..."), 
	READ_MESSAGE_TIME_S);

  /* allow them to read the above and fumble for their stop watch */
  Sleep (READ_MESSAGE_TIME_S * 1000);

  Info (_T("Start your clock in:"));

  Sleep (1000);

  for (DWORD dwC = 5; dwC != 0 ; dwC--)
    {
      Info (_T("%u"), dwC);
      Sleep (1000);
    }

  Info (_T(""));
  Info (_T("****** Start your clock now!!!! ***********"));
  Info (_T(""));

  ULONGLONG ullBeginTime;

  /* get the beginning time */
  if ((*timer->getTick)(ullBeginTime) == FALSE)
    {
      Error (_T("Failed to beginning time for %s."),
	     timer->name);
      returnVal = FALSE;
      goto cleanAndReturn;
    }

  ULONGLONG ullCurrTime = ullBeginTime;

  /* calculate the time that we want to stop */
  ULONGLONG ullWantedStopTime = ullBeginTime + ullTestLength;
  
  /*
   * overflow check
   * check the wraps at value for the timer and also check the timer value
   * itself.  If the wraps at value is the ULONGLONG_MAX, the first check will
   * always be true but the second check will fail.  (We actually got a hardware
   * overflow in this case.)
   */
  if ((ullWantedStopTime > timer->ullWrapsAt) ||
      (ullWantedStopTime <= MAX(ullBeginTime, ullTestLength)))
    {
      /* 
       * we will get overflow during the test
       * Normally we can deal with this, but we can't guarantee that the
       * sleep times will be what we want them to be.  The only way
       * to detect a bad sleep is to watch for current times that are
       * greater than the end time.  This is normally an overflow check, though.
       *
       * There are ways to get around this, but for this test it is easiest to 
       * have the user rerun it in this case.
       */
      Error (_T("The test would have overflowed a timing value for timer %s"),
	     timer->name);
      Error (_T("Please rerun it when the timers are not about to overflow."));
      Error (_T("Please see the documentation for more info.  This doesn't"));
      Error (_T("mean that the unit under test has a flaw."));
      returnVal = FALSE;
      goto cleanAndReturn;
    }

  ULONGLONG ullTimeLeft = ullTestLength;

  /* 
   * while not within 10 minutes print a status report every sleep
   * iteration.  This means that we will get only a maximum amount 
   * of output (for really long tests).
   */
  while (ullTimeLeft > ullConstTenMinutes)
    {
      if (ullTimeLeft - ullConstTenMinutes <= ullSleepIteration)
	{
	  /* sleep for the remaining time that will get us to ten minutes */
	  abstractSleep (timer, ullTimeLeft - ullConstTenMinutes);
	}
      else
	{
	  /* sleep for normal sleep iteration */
	  abstractSleep (timer, ullSleepIteration);
	}

      /* get time */
      if ((*timer->getTick)(ullCurrTime) == FALSE)
	{
	  Error (_T("Failed to get tick count for %s."),
		 timer->name);
	  returnVal = FALSE;
	  goto cleanAndReturn;
	}

      /* check for overflow in the next calculation */
      if (ullCurrTime >= ullWantedStopTime)
	{
	  /* 
	   * something went wrong.  The sleep statement must not have
	   * been correct or the timers are doing funky things.
	   */
	  Error (_T("The current time overshot the stop time for the test."));
	  Error (_T("This is quite odd, because it shouldn't have happened."));
	  Error (_T("Something is most likely wrong with either sleep or the ")
		 _T("timers."));
	  returnVal = FALSE;
	  goto cleanAndReturn;
	}

      /* recalculate time left */
      ullTimeLeft = ullWantedStopTime - ullCurrTime;

      realisticTime (double (ullTimeLeft) * timer->doConversionToS, 
		     doPrtTime, &szPrtUnits);
      Info (_T("Current time is %I64u ticks.  Time left in test is %I64u ")
	    _T("ticks, which is %g %s."), 
	    ullCurrTime, ullTimeLeft, doPrtTime, szPrtUnits);
    }

  /*
   * Between 10 minutes and 30 seconds print out messages each minute 
   */
  while (ullTimeLeft > ullConstThirtySeconds)
    {
      if (ullTimeLeft <= (ullConstOneMinute + ullConstThirtySeconds))
	{
	  /* sleep for the remaining time that will get us to 30 seconds */
	  abstractSleep (timer, ullTimeLeft - ullConstThirtySeconds);
	}
      else
	{
	  /* sleep for normal sleep iteration */
	  abstractSleep (timer, ullConstOneMinute);
	}

      /* get time */
      if ((*timer->getTick)(ullCurrTime) == FALSE)
	{
	  Error (_T("Failed to get tick count for %s."),
		 timer->name);
	  returnVal = FALSE;
	  goto cleanAndReturn;
	}

      /* check for overflow in the next calculation */
      if (ullCurrTime >= ullWantedStopTime)
	{
	  /* 
	   * something went wrong.  The sleep statement must not have
	   * been correct or the timers are doing funky things.
	   */
	  Error (_T("The current time overshot the stop time for the test."));
	  Error (_T("This is quite odd, because it shouldn't have happened."));
	  Error (_T("Something is most likely wrong with either sleep or the ")
		 _T("timers."));
	  returnVal = FALSE;
	  goto cleanAndReturn;
	}

      /* recalculate time left */
      ullTimeLeft = ullWantedStopTime - ullCurrTime;

      realisticTime (double (ullTimeLeft) * timer->doConversionToS, 
		     doPrtTime, &szPrtUnits);
      Info (_T("Current time is %I64u ticks.  Time left in test is %I64u ")
	    _T("ticks, which is %g %s."), 
	    ullCurrTime, ullTimeLeft, doPrtTime, szPrtUnits);
    }

  /* 
   * between 30 seconds and the test end print a message every second
   */
  while (1)
    {
      /* sleep for normal sleep iteration */
      abstractSleep (timer, ullConstOneSecond);

      /* get time */
      if ((*timer->getTick)(ullCurrTime) == FALSE)
	{
	  Error (_T("Failed to get tick count for %s."),
		 timer->name);
	  returnVal = FALSE;
	  goto cleanAndReturn;
	}

      if (ullCurrTime >= ullWantedStopTime)
	{
	  /* we are done */
	  break;
	}

      /* recalculate time left.  Can't overflow because of previous check */
      ullTimeLeft = ullWantedStopTime - ullCurrTime;

      realisticTime (double (ullTimeLeft) * timer->doConversionToS, 
		     doPrtTime, &szPrtUnits);
      Info (_T("Current time is %I64u ticks.  Time left in test is %I64u ")
	    _T("ticks, which is %g %s."), 
	    ullCurrTime, ullTimeLeft, doPrtTime, szPrtUnits);
    }

  ULONGLONG ullTrueEndTime;

  /* we have slept for the given time.  Now find the final time. */
  if ((*timer->getTick)(ullTrueEndTime) == FALSE)
    {
      Error (_T("Failed to get tick count for %s."),
	     timer->name);
      returnVal = FALSE;
      goto cleanAndReturn;
    }

  Info (_T(""));
  Info (_T("***** Stop your watch now! ******"));
  Info (_T(""));

  if (ullTrueEndTime < ullBeginTime)
    {
      /* 
       * we overflowed (which shouldn't happen) or something else 
       * happened
       */
      Error (_T("The test end time is less than the test start time."));
      Error (_T("This would normally suggest overflow, except that this"));
      Error (_T("is designed not to overflow.  Either Sleep is incorrect,"));
      Error (_T("the timers are incorrect, or the test is buggy."));
      returnVal = FALSE;
      goto cleanAndReturn;
    }

  realisticTime ((double) 
		 ((ullTrueEndTime - ullBeginTime) * timer->doConversionToS),
		 doPrtTime, &szPrtUnits);
  Info (_T("The total time for this test run was %g %s."), 
	doPrtTime, szPrtUnits);

  Info (_T("Please confirm this result with what you timed the test to be."));

    cleanAndReturn:
  
  return (returnVal);


}
