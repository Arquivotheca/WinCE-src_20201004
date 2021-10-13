//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* 
 * tuxtimers.cpp
 */

/*****  Headers  *************************************************************/

/* TESTPROCs for these functions */
#include "tuxtimers.h"

/* common utils */
#include "..\common\commonUtils.h"

/* implementation of timer tests */
#include "timerAbstraction.h"
#include "timerConvert.h"

/* statistics functions for data analysis */
#include "statistics.h"

/*****  Constants and Defines Local to This File  ****************************/

/* all doubles, defining maximum thresholds for these values */
#define GET_TICK_COUNT_MAX_RESOLUTION_AVE_S (0.002)
#define GET_TICK_COUNT_MAX_RESOLUTION_STDDEV_S \
              (GET_TICK_COUNT_MAX_RESOLUTION_AVE_S * 0.02) 
#define HI_PERF_MAX_RESOLUTION_AVE_S (0.002)
#define HI_PERF_MAX_RESOLUTION_STDDEV_S (HI_PERF_MAX_RESOLUTION_AVE_S * 0.02)
#define RTC_MAX_RESOLUTION_AVE_S (2.0)
#define RTC_MAX_RESOLUTION_STDDEV_S (RTC_MAX_RESOLUTION_AVE_S * 0.02)

/*
 * number of iterations to run the getTickCount latency and resolution
 * test.
 */
#define LATENCY_AND_RESOLUTION_MAX_ITERATIONS 300

/*
 * Time length of the backwards check tests.
 *
 * If the resolution calculations and tick timing is messed up then
 * you probably won't get the advertised backwards check time.
 *
 * These constants are considered 64 bits.
 *
 * Currently, all of these run for 10 minutes.
 */

#define GTC_BACKWARDS_CHECK_SECONDS (60 * 10)
#define QPC_BACKWARDS_CHECK_SECONDS (60 * 10)
#define RTC_BACKWARDS_CHECK_SECONDS (60 * 10)


/* length of the wall clock test, in seconds */
#define WALL_CLOCK_DEFAULT_TEST_LENGTH_S (3 * 60 * 60)

/* initialize a timer structure */
BOOL
initTimer (sTimer * newTimer, sTimer * existingTimer);

/* common code to init and run the latency/resolution test */
BOOL 
runLatencyAndResolution(sTimer * timer,
			double doThresholdAverage,
			double doThresholdStdDev,
			INT iNewPriority);

/* command line processing for backwards check tests */
BOOL
getBackwardsTestIterationsFromCmdLine (ULONGLONG * ullIter);
/*****************************************************************************
 *
 *                                GetTickCount Checks
 *
 *****************************************************************************/

/*
 * The function below calculates both the latency and the resolution of the 
 * SC_GetTickCount function in the OAL.  The latency is defined as the number
 * of GetTickCount system calls that can be made in one ms.  The resolution
 * is given in ms.
 */
TESTPROCAPI getTickCountLatencyAndResolution(
					     UINT uMsg, 
					     TPPARAM tpParam, 
					     LPFUNCTION_TABLE_ENTRY lpFTE) 

{
  INT returnVal = TPR_PASS;

  /* only supporting executing the test */ 
  if (uMsg != TPM_EXECUTE) 
    { 
      returnVal =  TPR_NOT_HANDLED;
      goto cleanAndExit;
    }

  /* get tick count timer */
  sTimer timer;

  /* initialize timer struct */
  if (initTimer (&timer, &gGetTickCountTimer) == FALSE)
    {
      /* getTickCount should always be present */
      Error (_T("Could not initalize the GetTickCount timer structure."));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

  /* process incoming arguments */
  INT iNewPriority = lpFTE->dwUserData;

  if (!((iNewPriority == DONT_CHANGE_PRIORITY) ||
	((iNewPriority >= 0) && (iNewPriority < 256))))
    {
      Error (_T("Tux function table parameter is incorrect.")
	     _T(" It is 0x%x."),
	     lpFTE->dwUserData);

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

  if (runLatencyAndResolution(&timer, 
			      (double) GET_TICK_COUNT_MAX_RESOLUTION_AVE_S,
			      (double) GET_TICK_COUNT_MAX_RESOLUTION_STDDEV_S,
			      iNewPriority) != TRUE)
    {
      returnVal = TPR_FAIL;
    }

 cleanAndExit:
  return (returnVal);
}


/*****************************************************************************/

/*
 * goal is to catch the timer on an interrupt firing and see if we can force
 * it backwards
 */
TESTPROCAPI getTickCountBackwardsCheck(
				       UINT uMsg, 
				       TPPARAM tpParam, 
				       LPFUNCTION_TABLE_ENTRY lpFTE) 
{
  /* be hopeful until proven otherwise */
  INT returnVal = TPR_PASS;

  if (uMsg != TPM_EXECUTE) 
    { 
      return TPR_NOT_HANDLED;
    }

  /* get tick count timer */
  sTimer timer;
  
  ULONGLONG ullIter;
  
  /* either way, we will run the test */
  if (getBackwardsTestIterationsFromCmdLine (&ullIter) != TRUE)
    {
      ullIter = GTC_BACKWARDS_CHECK_SECONDS;

      /* use default */
      Info (_T("Using the default test length, which is %I64u s."),
	    ullIter);
    }
  else
    {
      Info (_T("Using the user supplied test length, which is %I64u s."), 
	    ullIter);
    }

  /* initialize timer struct */
  if (initTimer (&timer, &gGetTickCountTimer) == FALSE)
    {
      /* getTickCount should always be present */
      Error (_T("Could not initalize the GetTickCount timer structure."));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

  if (doesClockGoBackwards (&timer, ullIter) == FALSE)
    {
      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

 cleanAndExit:

  return (returnVal);
}


/*****************************************************************************
 *
 *                     Query Performance Counter Checks
 *
 *****************************************************************************/


TESTPROCAPI hiPerfLatencyAndResolution(
				       UINT uMsg, 
				       TPPARAM tpParam, 
				       LPFUNCTION_TABLE_ENTRY lpFTE) 
{
  INT returnVal = TPR_PASS;

  /* only supporting executing the test */ 
  if (uMsg != TPM_EXECUTE) 
    { 
      returnVal =  TPR_NOT_HANDLED;
      goto cleanAndExit;
    }

  /* hi perf timer */
  sTimer timer;

  /* initialize timer struct */
  if (initTimer (&timer, &gHiPerfTimer) == FALSE)
    {
      Error (_T("Could not initalize the hi performance timer structure."));
      Error (_T("Maybe hi performance timers are not supported?"));

      returnVal = TPR_SKIP;
      goto cleanAndExit;
    }

  /* run at priority of 1 */
  if (runLatencyAndResolution(&timer, 
			      (double) HI_PERF_MAX_RESOLUTION_AVE_S,
			      (double) HI_PERF_MAX_RESOLUTION_STDDEV_S,
			      1) != TRUE)
    {
      returnVal = TPR_FAIL;
    }

 cleanAndExit:
  
  return (returnVal);
}


/*
 * goal is to catch the timer on an interrupt firing and see if we can force
 * it backwards
 */
TESTPROCAPI hiPerfBackwardsCheck(
				 UINT uMsg, 
				 TPPARAM tpParam, 
				 LPFUNCTION_TABLE_ENTRY lpFTE) 
{
  /* be hopeful until proven otherwise */
  INT returnVal = TPR_PASS;

  if (uMsg != TPM_EXECUTE) 
    { 
      return TPR_NOT_HANDLED;
    }

  /* hi perf timer */
  sTimer timer;

  ULONGLONG ullIter;
  
  /* either way, we will run the test */
  if (getBackwardsTestIterationsFromCmdLine (&ullIter) != TRUE)
    {
      ullIter = QPC_BACKWARDS_CHECK_SECONDS;

      /* use default */
      Info (_T("Using the default test length, which is %I64u s."),
	    ullIter);
    }
  else
    {
      Info (_T("Using the user supplied test length, which is %I64u s."), 
	    ullIter);
    }
  
  /* initialize timer struct */
  if (initTimer (&timer, &gHiPerfTimer) == FALSE)
    {
      Error (_T("Could not initalize the hi performance timer structure."));
      Error (_T("Maybe hi performance timers are not supported?"));

      returnVal = TPR_SKIP;
      goto cleanAndExit;
    }

  if (doesClockGoBackwards (&timer, ullIter) == FALSE)
    {
      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

 cleanAndExit:

  return (returnVal);
}

/*****************************************************************************
 *
 *                     System Time (RTC) Checks
 *
 *****************************************************************************/

TESTPROCAPI rtcLatencyAndResolution(
				    UINT uMsg, 
				    TPPARAM tpParam, 
				    LPFUNCTION_TABLE_ENTRY lpFTE) 
{
  INT returnVal = TPR_PASS;

  /* only supporting executing the test */ 
  if (uMsg != TPM_EXECUTE) 
    { 
      returnVal =  TPR_NOT_HANDLED;
      goto cleanAndExit;
    }

  /* System time timer */
  sTimer timer;

  if (initTimer (&timer, &gTimeOfDayTimer) == FALSE)
    {
      Error (_T("Could not initalize the System Time (RTC) timer structure."));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }
  
  /* run at priority of 1 */
  if (runLatencyAndResolution(&timer, 
			      (double) RTC_MAX_RESOLUTION_AVE_S,
			      (double) RTC_MAX_RESOLUTION_STDDEV_S, 
			      1) != TRUE)
    {
      returnVal = TPR_FAIL;
    }

 cleanAndExit:
  
  return (returnVal);
}


/*
 * goal is to catch the timer on an interrupt firing and see if we can force
 * it backwards
 */
TESTPROCAPI rtcBackwardsCheck(
			      UINT uMsg, 
			      TPPARAM tpParam, 
			      LPFUNCTION_TABLE_ENTRY lpFTE) 
{
  if (uMsg != TPM_EXECUTE) 
    { 
      return TPR_NOT_HANDLED;
    }

  /* be hopeful until proven otherwise */
  INT returnVal = TPR_PASS;

  /* System time timer */
  sTimer timer;

  ULONGLONG ullIter;
  
  /* either way, we will run the test */
  if (getBackwardsTestIterationsFromCmdLine (&ullIter) != TRUE)
    {
      ullIter = RTC_BACKWARDS_CHECK_SECONDS;

      /* use default */
      Info (_T("Using the default test length, which is %I64u s."),
	    ullIter);
    }
  else
    {
      Info (_T("Using the user supplied test length, which is %I64u s."), 
	    ullIter);
    }

  if (initTimer (&timer, &gTimeOfDayTimer) == FALSE)
    {
      Error (_T("Could not initalize the System Time (RTC) timer structure."));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

  if (doesClockGoBackwards (&timer, ullIter) == FALSE)
    {
      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

 cleanAndExit:

  return (returnVal);
}

/*****************************************************************************
 *
 *                        Compare Clocks
 *
 *****************************************************************************/

TESTPROCAPI compareHiPerfToGetTickCount(
					UINT uMsg, 
					TPPARAM tpParam, 
					LPFUNCTION_TABLE_ENTRY lpFTE)
{
  /* assume no error until proven otherwise */
  INT returnVal = TPR_PASS;

  /*
   * only handle the run test message type
   */
  if (uMsg != TPM_EXECUTE) 
    { 
      returnVal = TPR_NOT_HANDLED;
      goto cleanAndExit;
    }
  
  sTimer getTickCount, hiPerf;

  if (initTimer (&getTickCount, &gGetTickCountTimer) == FALSE)
    {
      Error (_T("Could not initalize the GetTickCount timer structure."));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }
  
  if (initTimer (&hiPerf, &gHiPerfTimer) == FALSE)
    {
      Error (_T("Could not initalize the hi performance timer structure."));
      Error (_T("Maybe hi performance timers are not supported?"));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

  if (compareClock1ToClock2 (&getTickCount, &hiPerf) == FALSE)
    {
      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

 cleanAndExit:
  return (returnVal);

}


TESTPROCAPI compareRTCToGetTickCount(
					UINT uMsg, 
					TPPARAM tpParam, 
					LPFUNCTION_TABLE_ENTRY lpFTE)
{
  /* assume no error until proven otherwise */
  INT returnVal = TPR_PASS;

  /*
   * only handle the run test message type
   */
  if (uMsg != TPM_EXECUTE) 
    { 
      returnVal = TPR_NOT_HANDLED;
      goto cleanAndExit;
    }
  
  sTimer getTickCount, rtc;

  if (initTimer (&getTickCount, &gGetTickCountTimer) == FALSE)
    {
      Error (_T("Could not initalize the GetTickCount timer structure."));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

  if (initTimer (&rtc, &gTimeOfDayTimer) == FALSE)
    {
      Error (_T("Could not initalize the System Time (RTC) timer structure."));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

  if (compareClock1ToClock2 (&rtc, &getTickCount) == FALSE)
    {
      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

 cleanAndExit:
  return (returnVal);

}

TESTPROCAPI compareHiPerfToRTC(
					UINT uMsg, 
					TPPARAM tpParam, 
					LPFUNCTION_TABLE_ENTRY lpFTE)
{
  /* assume no error until proven otherwise */
  INT returnVal = TPR_PASS;

  /*
   * only handle the run test message type
   */
  if (uMsg != TPM_EXECUTE) 
    { 
      returnVal = TPR_NOT_HANDLED;
      goto cleanAndExit;
    }

  sTimer hiPerf, rtc;

  if (initTimer (&hiPerf, &gHiPerfTimer) == FALSE)
    {
      Error (_T("Could not initalize the hi performance timer structure."));
      Error (_T("Maybe hi performance timers are not supported?"));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

  if (initTimer (&rtc, &gTimeOfDayTimer) == FALSE)
    {
      Error (_T("Could not initalize the System Time (RTC) timer structure."));

      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }

  if (compareClock1ToClock2 (&hiPerf, &rtc) == FALSE)
    {
      returnVal = TPR_FAIL;
      goto cleanAndExit;
    }
  
 cleanAndExit:
  return (returnVal);

}

/*****************************************************************************
 *
 *                           Miscellaneous
 *
 *****************************************************************************/

/*
 * max number of status statements to print during the wall clock test.  This 
 * isn't followed exactly, but prevents thousands of lines of output.
 */
#define WALL_CLOCK_NUM_STATUS_STATEMENTS 100

TESTPROCAPI wallClockDriftTest(
			       UINT uMsg, 
			       TPPARAM tpParam, 
			       LPFUNCTION_TABLE_ENTRY lpFTE)
{
  INT iReturnVal = TPR_PASS;

  /* instantiate class for this command line */
  CClParse cmdLine(g_szDllCmdLine);

  /* only supporting executing the test */ 
  if (uMsg != TPM_EXECUTE) 
    { 
      iReturnVal =  TPR_NOT_HANDLED;
      goto cleanAndReturn;
    }

  /* get tick count timer */
  sTimer timer;

  /* initialize timer struct */
  if (initTimer (&timer, &gGetTickCountTimer) == FALSE)
    {
      /* getTickCount should always be present */
      Error (_T("Could not initalize the GetTickCount timer structure."));

      iReturnVal = TPR_FAIL;
      goto cleanAndReturn;
    }

  /* test length is seconds */
  DWORD dwTestLengthS;
  DWORD dwTestLengthMinutes;

  if (cmdLine.GetOptVal (_T("tt_wc"), &dwTestLengthMinutes) == TRUE)
    {
      if (dwTestLengthMinutes == 0)
	{
	  Error (_T("Can't run for zero minutes."));
	  iReturnVal = TPR_FAIL;
	  goto cleanAndReturn;
	}

      dwTestLengthS = dwTestLengthMinutes * 60;

      /* overflow check */
      if (dwTestLengthS <= dwTestLengthMinutes)
	{
	  Error (_T("overflow: test length is too large"));
	  iReturnVal = TPR_FAIL;
	  goto cleanAndReturn;
	}

      double doTime;
      TCHAR * szUnits;
      realisticTime ((double) dwTestLengthS, doTime, &szUnits);

      Info (_T("User specified test length of %g %s."), doTime, szUnits);
    }
  else
    {
      /* use default */
      dwTestLengthS = WALL_CLOCK_DEFAULT_TEST_LENGTH_S;

      double doTime;
      TCHAR * szUnits;
      realisticTime ((double) dwTestLengthS, doTime, &szUnits);

      Info (_T("No test length given on command line (or we couldn't parse it)"));
      Info (_T("Using default of %g %s."), doTime, szUnits);
    }

  if (wallClockDriftTest (&timer, dwTestLengthS, 
			  WALL_CLOCK_NUM_STATUS_STATEMENTS) == TRUE)
    {
      iReturnVal = TPR_PASS;
    }
  else
    {
      iReturnVal = TPR_FAIL;
    }

 cleanAndReturn:
  return (iReturnVal);
}





/*****************************************************************************
 *
 *                             Helpers
 *
 *****************************************************************************/

/*
 * initialize the given timer structure.  This copies the global
 * constant data from existingTimer into the structure and then calls
 * the initTimerStruct from the timer abstraction code to initialize
 * the rest.
 *
 * newTimer is the structure being initialized.  Constant data is
 * taken from existingTimer.  Neither of these can be NULL.
 *
 * Returns TRUE on success and FALSE otherwise.
 */
BOOL
initTimer (sTimer * newTimer, sTimer * existingTimer)
{
  BOOL returnVal = TRUE;

  if ((newTimer == NULL) || (existingTimer == NULL))
    {
      IntErr (_T("((newTimer == NULL) || (existingTimer == NULL))"));
      return FALSE;
    }

  /* 
   * The timer structures consist of function pointers to abstract
   * calls to different timers and data about how to convert the freq
   * of this timer to seconds.  Since of this info is defined on the
   * fly initialization occurs in a two stages.  The first involves
   * copying the global fixed data into the local structure.  The
   * second involves calling the init function on the structure.
   */

  /* copy the global fixed data into the local struct */
  memcpy (newTimer, existingTimer, sizeof (sTimer));

  /* initialize the on-the-fly stuff */
  return (initTimerStruct (*newTimer));
}

/*
 * Run the latency and resolution test.  This function sets the
 * priority if it needs to be changed, calls the main test code,
 * and checks the results against the thresholds passed as parameters
 * to this function.
 *
 * timer is the timer.  doThresholdAverage is the maximum allowed
 * resolution threshold.  doThresholdStdDev is the maximum allowed
 * threshold standard deviation.  iNewPriority is the priority to run
 * the test at.  DONT_CHANGE_PRIORITY is self explanatory.
 *
 * return FALSE on failure and TRUE on success.
 */
BOOL 
runLatencyAndResolution(sTimer * timer,
			double doThresholdAverage,
			double doThresholdStdDev,
			INT iNewPriority)
{ 
  if (!((iNewPriority == DONT_CHANGE_PRIORITY) ||
	((iNewPriority >= 0) && (iNewPriority < 256))))
    {
      IntErr (_T("runLatencyAndResolution: !((iNewPriority == ")
	      _T("DONT_CHANGE_PRIORITY) || (iNewPriority >= 0)")
	      _T("&& (iNewPriority < 256))))"));
      
      return (FALSE);
    }

  BOOL returnVal = TRUE;

  /* for now don't change it unless args tell us to */
  INT iOldPriority = DONT_CHANGE_PRIORITY;

  double doGetTickCallsAve, doGetTickCallsStdDev, 
    doResolutionAve, doResolutionStdDev;

  Info (_T("Precision of %s is %g s.  This is NOT the resolution."),
	timer->name, timer->doConversionToS);

  Info (_T("Starting to calculate the latency and resolution of the calls ")
	_T("used to get the tick data from %s."),
	timer->name);

  BOOL bPrintIterations = FALSE;
#ifdef PRINT_DEBUG_VV
  bPrintIterations = TRUE;
#endif

  /* set the thread priority if need be */
  if (handleThreadPriority (iNewPriority, &iOldPriority) == FALSE)
    {
      Error (_T("Could not set thread priority to %i."),
		 iNewPriority);

      returnVal = FALSE;
      goto cleanAndExit;
    }
  else if (iNewPriority != DONT_CHANGE_PRIORITY)
    {
      Info (_T("Thread priority is set to %i."),
	    iNewPriority);
    }
      
  /* find the latency and resolution */
  BOOL bFuncReturnVal = 
    getLatencyAndResolutionSmoothed (timer,
				     LATENCY_AND_RESOLUTION_MAX_ITERATIONS,
				     doGetTickCallsAve, 
				     doGetTickCallsStdDev, 
				     doResolutionAve,
				     doResolutionStdDev, bPrintIterations);

  /*
   * set the thread priority back
   * do this before we check the functions return value to be nice.
   *
   * yes, in this case we do potentially miss the fact that the resolution
   * might have failed, but we should never not be able to set the priority
   * back.  If we do we have big problems.
   */
  /* set the thread priority if need be */
  if (handleThreadPriority (iOldPriority, NULL) == FALSE)
    {
      Error (_T("Could not set thread priority back to %i."),
		 iOldPriority);

      returnVal = FALSE;
      goto cleanAndExit;
    }
  else if (iNewPriority != DONT_CHANGE_PRIORITY)
    {
      Info (_T("Thread priority set back to %i."),
	    iOldPriority);
    }

  if (bFuncReturnVal == FALSE)
    {
      Error (_T("Failed to get the smoothed latency and resolution for %s."),
	     timer->name);

      returnVal = FALSE;
      goto cleanAndExit;
    }

  if (doResolutionStdDev > doThresholdStdDev)
    {
      /* convert the times into nice, printing times and print them out */
      double doPrtTime1, doPrtTime2;
      TCHAR * szPrtUnits1, * szPrtUnits2;

      realisticTime (doThresholdStdDev, doPrtTime1, &szPrtUnits1);
      realisticTime (doResolutionStdDev, doPrtTime2, &szPrtUnits2);

      Error (_T("Standard deviation is outside of the allowable tolerance "));
      Error (_T("for %s.  The tolerance is %g %s and the standard deviation "),
	     timer->name, doPrtTime1, szPrtUnits1);
      Error (_T("is %g %s."), doPrtTime2, szPrtUnits2);
      
      returnVal = FALSE;
      goto cleanAndExit;
    }

  if (doResolutionAve > doThresholdAverage)
    {
      /* convert the times into nice, printing times and print them out */
      double doPrtTime1, doPrtTime2;
      TCHAR * szPrtUnits1, * szPrtUnits2;

      realisticTime (doThresholdAverage, doPrtTime1, &szPrtUnits1);
      realisticTime (doResolutionAve, doPrtTime2, &szPrtUnits2);

      Error (_T("Resolution is outside of the allowable tolerance for %s."),
	     timer->name);
      Error (_T("The tolerance is %g %s and the calculated value is %g %s."),
	     doPrtTime1, szPrtUnits1, doPrtTime2, szPrtUnits2);
      
      returnVal = FALSE;
      goto cleanAndExit;
    }

  /* print out stats for the test run */
  Info (_T("For the %s clock, it took %g +- %g calls which yielded ")
	_T("a resolution of %g +- %g s."),
	timer->name,
	doGetTickCallsAve, doGetTickCallsStdDev, 
	doResolutionAve, doResolutionStdDev);

 cleanAndExit:
  
  return (returnVal);
}

/*****************************************************************************
 *
 *                        Command Line Arg Processing
 *
 *****************************************************************************/

/* buffer length, including null term.  don't make this zero! */
#define MAX_COMMAND_LINE_ARG_LENGTH 100

/*
 * get the backward test iterations from the command line.  Put the value into
 * ullIter.
 *
 * return TRUE if we successfully got the value and false if either the option
 * wasn't present or we couldn't convert it.
 */
BOOL
getBackwardsTestIterationsFromCmdLine (ULONGLONG * ullIter)
{
  BOOL returnVal = FALSE;

  CClParse cmdLine(g_szDllCmdLine);

  /* buffer for string */
  TCHAR szBuf[MAX_COMMAND_LINE_ARG_LENGTH];

  if (cmdLine.GetOptString (_T("tt_it"), szBuf, MAX_COMMAND_LINE_ARG_LENGTH) 
      != TRUE)
    {
      /* wasn't on command line */
      goto cleanAndReturn;
    }

  /* force null term */
  szBuf[MAX_COMMAND_LINE_ARG_LENGTH - 1] = 0;

  if (strToULONGLONG (szBuf, ullIter) != TRUE)
    {
      Error (_T("Couldn't convert %s to a ulonglong"), szBuf);
      goto cleanAndReturn;
    }
  
  returnVal = TRUE;

 cleanAndReturn:
  return (returnVal);
}
