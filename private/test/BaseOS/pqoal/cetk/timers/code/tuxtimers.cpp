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
 * tuxtimers.cpp
 */

/*****  Headers  *************************************************************/

/* TESTPROCs for these functions */
#include "tuxtimers.h"

/* common utils */
#include "..\..\..\common\commonUtils.h"

/* implementation of timer tests */
#include "timerAbstraction.h"
#include "timerConvert.h"

/* command line parsing */
#include "timersCmdLine.h"

/* sleep routines for drift tests */
#include "sleepRoutines.h"

/*****  Constants and Defines Local to This File  ****************************/

/*****  Resoulution tests ******/
/*
 * these define how many interations the tests will run.  Each clock
 * has a different number of iterations since these influence the run
 * time.  The rtc is restricted for this reason.
 *
 * We always ignore the first data point (position 0), since this is
 * always too much.  An easy work around to always get 1000 data points
 * is to increase the array length.
 */
#define GTC_RES_RUN_INTERATIONS 1001

/* all ulonglongs */
#define GTC_RES_MAX_N 1
#define GTC_RES_MAX_RANGE 0


/****** Backwards checks *******/

/*
 * Time length of the backwards check tests.
 *
 * If the resolution calculations and tick timing is messed up then
 * you probably won't get the advertised backwards check time.
 *
 * These constants are considered 64 bits.
 *
 * Currently, all of these run for 3 hours.
 */
#define GTC_BCKWDS_CHECK_DEFAULT_S (60 * 60 * 3)
#define QPC_BCKWDS_CHECK_DEFAULT_S (60 * 60 * 3)
#define RTC_BCKWDS_CHECK_DEFAULT_S (60 * 60 * 3)

/*
 * how often to print a message telling what time it is.  This is useful on
 * very long runs.  It helps isolate how often the timer goes backward.
 *
 * This timing isn't exact.  It will vary with drift in the clock under test.
 *
 * Currently using 10 minutes.
 */
#define BACKWARDS_CHECK_PRINT_MESSAGE_EVERY_N_S (60 * 10)

/*
 * How long the test needs to calibrate.  We use the minimum possible because
 * the backwards checks shouldn't be noisy.  Noisy tests require longer periods
 * to get a good measurement.
 */
#define BACKWARDS_CHECK_CALIBRATION_THRESHOLD_S (10)

/*
 * How many iterations the calculating resolution code should run to determine a
 * clocks resolution.  15 seems to work fine.
 *
 * Increasing this will increase test run time for the rtc.
 */
#define BACKWARDS_CHECK_CALC_RESOLUTION_ITERATIONS 15

/*
 * Used to specify the maximum sleep time for the backwards check test
 * that sleeps between each read.  This the number of ms.  Value
 * should be exclusive.
 *
 * Don't set to zero.  Small values (ie. below 5 ms) might cause
 * undefined behavior.
 *
 * The test goal is to check behavior in OEM idle.
 */
#define BACKWARDS_CHECK_RANDOM_SLEEP_TIME_MS (1000)

/****** Drift tests *******/

/*
 * Thresholds for the drift tests
 *
 * If you are going to adjust the thresholds it is best to do it
 * through the command line arguments.
 */
#define HI_PERF_TO_GTC_DRIFT_FASTER_BOUND 0.0;
#define HI_PERF_TO_GTC_DRIFT_SLOWER_BOUND 0.0;

#define RTC_TO_GTC_DRIFT_FASTER_BOUND (0.00)
#define RTC_TO_GTC_DRIFT_SLOWER_BOUND (0.00)

#define HI_PERF_TO_RTC_DRIFT_FASTER_BOUND (0.0)
#define HI_PERF_TO_RTC_DRIFT_SLOWER_BOUND (0.0)

/*
 * The default step size and test run time.  The run time must be a
 * multiple of the step size.
 */
#define DRIFT_TEST_SAMPLE_STEP_S (2 * 60)
#define DRIFT_TEST_RUN_TIME_S (60 * 60 * 3)

/* track idle test params */
#define TRACK_IDLE_DEFAULT_SAMPLE_STEP_S (2*60)
#define TRACK_IDLE_DEFAULT_IND_RUN_TIME_S (10*60)

/****** Wallclock tests *******/

/* length of the wall clock test, in seconds */
#define WALL_CLOCK_DEFAULT_TEST_LENGTH_S (3 * 60 * 60)

/****** Functions ***********************************************************/

BOOL
runResolution(sTimer * timer,
          DWORD dwIterations,
          ULONGLONG * pullN, ULONGLONG * pullRange,
          int iNewPriority);

BOOL
runBackwardsCheck (sTimer * timer, ULONGLONG ullRunTimeS,
           BOOL (*fSleep)(DWORD dwTimeMS),
           DWORD dwSleepTime);

/* a helper function for this version of the drift tests */
BOOL
compareThreeClocksTrackIdle(sTimer * timer1, sTimer * timer2, sTimer * timer3,
                sBounds * bound1To2,
                sBounds * bound1To3,
                sBounds * bound2To3,
                DWORD dwSampleStepS,
                DWORD dwTestRunTimeS,
                BOOL (*fSleep)(DWORD dwTimeMS));

/*
 * abstract version of wall clock drift test.  Does the heavy lifting.
 * The entry points just call this with the correct timer.
 */
TESTPROCAPI wallClockDriftTest(
                   sTimer * timerToInitFrom,
                   UINT uMsg,
                   TPPARAM tpParam,
                   LPFUNCTION_TABLE_ENTRY lpFTE);

/*****************************************************************************
 *
 *                                Usage Message
 *
 *****************************************************************************/


TESTPROCAPI timerTestUsageMessage(
                  UINT uMsg,
                  TPPARAM tpParam,
                  LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    printTimerUsageMessage ();

    return (TPR_PASS);

}


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
TESTPROCAPI getTickCountResolution(
                         UINT uMsg,
                         TPPARAM tpParam,
                         LPFUNCTION_TABLE_ENTRY lpFTE)

{
    INT returnVal = TPR_FAIL;

    cTokenControl tokens;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    ULONGLONG ullMaxN_ms = { 0 };
    sTimer timer = { 0 };

    /***** initialize ullMaxN_ms */

    /*
     * global command line can't be null, tokenize must work, and option
     * must be present for us to parse it
     */
    if (g_szDllCmdLine && tokenize ((TCHAR *)g_szDllCmdLine, &tokens) &&
        isOptionPresent (&tokens, (ARG_STR_GTC_MAX_RES)))
    {
        double doMaxN;

        if (!getOptionDouble (&tokens, ARG_STR_GTC_MAX_RES, &doMaxN))
        {
            Error (_T("Couldn't read command line arg for %s."), ARG_STR_GTC_MAX_RES);
            goto cleanAndExit;
        }

        /* check for good value */
        if (doMaxN < 0.0)
        {
            Error (_T("Max resolution can't be negative."));
            goto cleanAndExit;
        }

        if ((doMaxN - ((DWORD)(doMaxN / 0.001)) * 0.001) != 0.0)
        {
            Info (_T("Warning: Upper bound of %g is not a multiple of ")
            _T("1 ms (0.001 s)"), doMaxN);
         }

        /* convert from double to ulonglong */
        ullMaxN_ms = (ULONGLONG) (doMaxN * 1000.0);
    }
    else
    {
        ullMaxN_ms = GTC_RES_MAX_N;
    }

    if (ullMaxN_ms == 0)
    {
        Error (_T("Using an upper bound of zero provides an impossible bound"));
        goto cleanAndExit;
    }

    Info (_T("The resolution has to be <= %I64u ticks."), ullMaxN_ms);

    /****** initialize ullMaxRange */

    ULONGLONG ullMaxRange = (ULONGLONG) GTC_RES_MAX_RANGE;

    Info (_T("The range has to be not greater than %I64u ticks."), ullMaxRange);

    /****** initialize timer */


    /* initialize timer struct */
    if (initTimer (&timer, &gGetTickCountTimer) == FALSE)
    {
        /* getTickCount should always be present */
        Error (_T("Could not initalize the GetTickCount timer structure."));

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

        goto cleanAndExit;
    }

    ULONGLONG ullN = { 0 }, ullRange = { 0 };

    if (!runResolution(&timer, GTC_RES_RUN_INTERATIONS, &ullN, &ullRange, 1))
    {
        Error (_T("Couldn't get resolution for timer %s"), timer.name);
        goto cleanAndExit;
    }

    if ((ullN > ullMaxN_ms) || (ullRange > ullMaxRange))
    {
        Error (_T("The resolution of %s is out of bounds."));
        Error (_T("It should be between [%I64u, %I64u] ticks but is instead ")
         _T("[%I64u, %I64u]"),
         ullMaxN_ms, ullMaxN_ms + ullMaxRange, ullN, ullN + ullRange);

        goto cleanAndExit;
    }

    returnVal = TPR_PASS;

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
    INT returnVal = TPR_FAIL;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    /* get tick count timer */
    sTimer timer;

    ULONGLONG ullRunTime = GTC_BCKWDS_CHECK_DEFAULT_S;

    /* get run time */
    if (!handleBackwardsCheckParams (&ullRunTime))
    {
        goto cleanAndExit;
    }

    /* initialize timer struct */
    if (initTimer (&timer, &gGetTickCountTimer) == FALSE)
    {
        /* getTickCount should always be present */
        Error (_T("Could not initalize the GetTickCount timer structure."));
        goto cleanAndExit;
    }

    /*
     * In this case we want a busy loop check.  Given no function pointer
     * for the sleep time tells the test routines to loop as fast as they
     * can.
     */
    if (runBackwardsCheck (&timer, ullRunTime, NULL, 0) == FALSE)
    {
        goto cleanAndExit;
    }
    
    returnVal = TPR_PASS;

cleanAndExit:

    return (returnVal);
}


/*
 * goal is catch the timer going backwards due to errors in the OEM
 * idle and associated sleep functions.  The test sleeps for given
 * intervals, checking the timer when it wakes up.
 */
TESTPROCAPI getTickCountBackwardsCheckRandomSleep(
                       UINT uMsg,
                       TPPARAM tpParam,
                       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* be hopeful until proven otherwise */
    INT returnVal = TPR_FAIL;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    /* get tick count timer */
    sTimer timer;

    ULONGLONG ullRunTime = GTC_BCKWDS_CHECK_DEFAULT_S;

    /* get run time */
    if (!handleBackwardsCheckParams (&ullRunTime))
    {
        goto cleanAndExit;
    }

    /* initialize timer struct */
    if (initTimer (&timer, &gGetTickCountTimer) == FALSE)
    {
        /* getTickCount should always be present */
        Error (_T("Could not initalize the GetTickCount timer structure."));
        goto cleanAndExit;
    }

    /* tell the test routine to use the wait for random time function */
    if (runBackwardsCheck (&timer, ullRunTime,
             WaitRandomTime,
             BACKWARDS_CHECK_RANDOM_SLEEP_TIME_MS) == FALSE)
    {
        goto cleanAndExit;
    }

    returnVal = TPR_PASS;

cleanAndExit:

    return (returnVal);
}

/*
 * print out the gtc.  Useful for debugging.
 */
TESTPROCAPI gtcPrintTime(
                  UINT uMsg,
                  TPPARAM tpParam,
                  LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwStepTime = 1000;
    DWORD dwNumOverFlow =1;
    DWORD dwGtc=0,dwOld=0;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    /* be hopeful until proven otherwise */
    INT returnVal = TPR_PASS;

    breakIfUserDesires();

    for (DWORD i = 0; i < 10; i++)
    {

        dwOld = dwGtc;
        dwGtc = GetTickCount();

        Info (_T("Line %u   GTC = %u ms = ~%g s"),
            i, dwGtc, (double) dwGtc / 1000.0);

        if(i>=0)  //Let's check to make sure at least 1000 milli seconds have gone by
        {
            if(dwGtc > dwOld)
            {
                if(dwGtc - dwOld < dwStepTime)
                {
                    returnVal = TPR_FAIL;
                    Error(_T("GetTickCount is out of sync with the sleep command.  Should step at least %u ms"),dwStepTime);
                    goto cleanAndExit;
                }
            }
            else  //investigate, overflow or error.
            {
                if( (dwGtc + (0xffffffff-dwOld) + 1) < dwStepTime)
                {
                    returnVal = TPR_FAIL;
                    Error(_T("GetTickCount is out of sync with the sleep command.  Should step at least %u ms"),dwStepTime);
                    goto cleanAndExit;
                }
                else if(dwNumOverFlow == 1)  //we let at most over flow occur
                {
                    dwNumOverFlow++;
                }
                else
                {
                    Error(_T("GetTickCount is ticking backwards"));
                    returnVal = TPR_FAIL;
                    goto cleanAndExit;
                }
            }
        }

        Sleep (dwStepTime);
    }
cleanAndExit:
    return (returnVal);
}


/*****************************************************************************
 *
 *                     Query Performance Counter Checks
 *
 *****************************************************************************/


TESTPROCAPI hiPerfResolution(
                       UINT uMsg,
                       TPPARAM tpParam,
                       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndExit;
    }

    breakIfUserDesires();

    Info (_T("This test gets the High Resolution Timer (QueryPerformanceCounter) resolution."));
    Info (_T("The user needs to verify if the value printed out by this test is as expected."));
    Info (_T(""));

    LARGE_INTEGER liFrequency;

    if (!QueryPerformanceFrequency(&liFrequency))
    {
        // QPC is probably not implemented
        Error (_T("Tried calling QueryPerformanceFrequency() to get the resolution."));
        Error (_T("The function returned FALSE."));
        Error (_T("Check if the High Resolution Timer (QueryPerformanceCounter) is implemented in your BSP."));
        goto cleanAndExit;
    }

    if ((liFrequency.QuadPart < 0) || (liFrequency.QuadPart == 0))
    {
        Error (_T("Calling QueryPerformanceFrequency() returned an invalid value: %I64i"), liFrequency.QuadPart);
Error (_T("Please check the implementation of this function."));
        Error (_T(""));
        Error (_T("Could not retrieve the resolution of the High Resolution Timer."));
        goto cleanAndExit;
    }

    DOUBLE doResolution = 1.0/(DOUBLE)(liFrequency.QuadPart);

    Info (_T("The resolution of the High Resolution Timer is as follows:"));
    Info (_T("%g seconds = %g milliseconds = %g microseconds"),
                                doResolution, doResolution * 1000, doResolution * 1000000);

    returnVal = TPR_PASS;


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
    sTimer timer = { 0 };

    ULONGLONG ullRunTime = QPC_BCKWDS_CHECK_DEFAULT_S;

    /* get run time */
    if (!handleBackwardsCheckParams (&ullRunTime))
    {
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    /*Check to see if high performance timer is supported*/

    ULONGLONG ullFreq;

    if (((*(gHiPerfTimer.getFreq))(ullFreq)) == FALSE)
    {
        Error (_T("Hi performance timers are not supported, skipping test"));
        returnVal = TPR_SKIP;
        goto cleanAndExit;
    }

    /* initialize timer struct */
    if (initTimer (&timer, &gHiPerfTimer) == FALSE)
    {
        Error (_T("Could not initalize the hi performance timer structure."));
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    /* this test will not sleep in between clock reads */
    if (runBackwardsCheck (&timer, ullRunTime, NULL, 0) == FALSE)
    {
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

cleanAndExit:

    return (returnVal);
}


/*
 * print out the qpc.  Useful for debugging.
 */
TESTPROCAPI hiPerfPrintTime(
                  UINT uMsg,
                  TPPARAM tpParam,
                  LPFUNCTION_TABLE_ENTRY lpFTE)
{
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    BOOL returnVal = TRUE;

    breakIfUserDesires();

    /* get frequency */
    LARGE_INTEGER liPerfFreq = { 0 };

    if (QueryPerformanceFrequency (&liPerfFreq) == FALSE)
    {
        Error (_T("Couldn't get frequency of QPC."));
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    if ((liPerfFreq.QuadPart < 0) || (liPerfFreq.QuadPart == 0))
    {
        Error (_T("Frequency of %I64i is <= 0"), liPerfFreq.QuadPart);
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /*
     * cast from signed to unsigned
     */
    ULONGLONG ullFreq = (ULONGLONG) liPerfFreq.QuadPart;

    for (DWORD i = 0; i < 10; i++)
    {
        LARGE_INTEGER liPerfCount;

        if (QueryPerformanceCounter (&liPerfCount) == FALSE)
        {
            Error (_T("Couldn't get QPC."));
            returnVal = FALSE;
        }

        /*
         * LARGE_INTEGER is a signed value.  A negative time doesn't make
         * any sense, so return an error.
         */
        if (liPerfCount.QuadPart < 0)
        {
            Error (_T("QPC is negative.  %I64i < 0"), liPerfCount.QuadPart);
            returnVal = FALSE;
        }

        /*
         * cast from signed to unsigned
         */
        ULONGLONG ullTicks = (ULONGLONG) liPerfCount.QuadPart;

        Info (_T("Line %u   Ticks:  %I64u  =  ~%g s"), i,
        ullTicks, (double) ullTicks / (double) ullFreq);

        Sleep (1000);
    }

cleanAndReturn:

    if (returnVal)
    {
        return (TPR_PASS);
    }
    else
    {
        return (TPR_FAIL);
    }

}



/*****************************************************************************
 *
 *                     System Time (RTC) Checks
 *
 *****************************************************************************/


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
    sTimer timer = { 0 };

    ULONGLONG ullRunTime = RTC_BCKWDS_CHECK_DEFAULT_S;

    /* get run time */
    if (!handleBackwardsCheckParams (&ullRunTime))
    {
        goto cleanAndExit;
    }

    if (initTimer (&timer, &gTimeOfDayTimer) == FALSE)
    {
        Error (_T("Could not initalize the System Time (RTC) timer structure."));

        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    if (runBackwardsCheck (&timer, ullRunTime, NULL, 0) == FALSE)
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

TESTPROCAPI compareAllThreeClocks(
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
        return TPR_NOT_HANDLED;
    }

    breakIfUserDesires();

    /* initialize timers */
    sTimer getTickCount, hiPerf, rtc;

    sBounds boundGtcToRtc = { 0 };
    sBounds boundGtcToHiPerf = { 0 };
    sBounds boundHiPerfToRtc = { 0 };

    boundGtcToRtc.szName = _T("gtc to rtc");
    boundGtcToRtc.doFasterBound = 0.0;
    boundGtcToRtc.doSlowerBound = 0.0;
    boundGtcToHiPerf.szName = _T("gtc to qpc");
    boundGtcToHiPerf.doFasterBound = 0.0;
    boundGtcToHiPerf.doSlowerBound = 0.0;
    boundHiPerfToRtc.szName = _T("qpc to rtc");
    boundHiPerfToRtc.doFasterBound = 0.0;
    boundHiPerfToRtc.doSlowerBound = 0.0;

    DWORD dwSampleStepS = DRIFT_TEST_SAMPLE_STEP_S;
    DWORD dwTestRunTimeS = DRIFT_TEST_RUN_TIME_S;
    cTokenControl tokens;

    if (initTimer (&getTickCount, &gGetTickCountTimer) == FALSE)
    {
        Error (_T("Could not initalize the GetTickCount timer structure."));

        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }
    /*Check to see if high performance timer is supported*/

    ULONGLONG ullFreq;

    if (((*(gHiPerfTimer.getFreq))(ullFreq)) == FALSE)
    {
        Error (_T("Hi performance timers are not supported, skipping test"));
        returnVal = TPR_SKIP;
        goto cleanAndExit;
    }

    if (initTimer (&hiPerf, &gHiPerfTimer) == FALSE)
    {
        Error (_T("Could not initalize the hi performance timer structure."));
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    if (initTimer (&rtc, &gTimeOfDayTimer) == FALSE)
    {
        Error (_T("Could not initalize the System Time (RTC) timer structure."));

        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    BOOL (*fSleep)(DWORD dwTimeMS) = NULL;
    const TCHAR * szSleepFunctionName = NULL;

    switch (lpFTE->dwUserData)
    {
    case DRIFT_BUSY_SLEEP:
        fSleep = BusyOsSleepFunction;
        szSleepFunctionName = ARG_STR_BUSY_SLEEP_NAME;
        break;
    case DRIFT_OS_SLEEP:
        fSleep = OsSleepFunction;
        szSleepFunctionName = ARG_STR_OS_SLEEP_NAME;
        break;
    case DRIFT_OEMIDLE_PERIODIC:
        fSleep = LittleOsSleepFunction;
        szSleepFunctionName = ARG_STR_TRACK_IDLE_PERIODIC_NAME;
        break;
    case DRIFT_OEMIDLE_RANDOM:
        fSleep = RandomOsSleepFunction;
        szSleepFunctionName = ARG_STR_TRACK_IDLE_RANDOM_NAME;
        break;
    default:
        Error (_T("Wrong parameter for sleep type."));
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }


    /*
     * if user doesn't specify the -c param to tux the cmd line global
     * var is null.  In this case don't even try to parse command line
     * args.
     */
    if (g_szDllCmdLine != NULL)
    {

        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            returnVal = TPR_FAIL;
            goto cleanAndExit;
        }

        /*
         * run both command line arg processing functions and then bail
         * if either fails
         *
         * don't run this for the tracking oem idle case, since this case
         * will override the params anyhow.
         */
        if (!getArgsForDriftTests (&tokens,
                 ARG_STR_DRIFT_SAMPLE_SIZE,
                 ARG_STR_DRIFT_RUN_TIME,
                 &dwSampleStepS, &dwTestRunTimeS)&&
                 lpFTE->dwUserData != DRIFT_OEMIDLE_PERIODIC &&
                 lpFTE->dwUserData != DRIFT_OEMIDLE_RANDOM)
        {
            returnVal = TPR_FAIL;
        }

        if (!getBoundsForDriftTests (&tokens, szSleepFunctionName,
                   ARG_STR_DRIFT_GTC_RTC,
                   &boundGtcToRtc,
                   ARG_STR_DRIFT_GTC_HI,
                   &boundGtcToHiPerf,
                   ARG_STR_DRIFT_HI_RTC,
                   &boundHiPerfToRtc))
        {
            returnVal = TPR_FAIL;
        }

        g_fQuickFail = FALSE;
        getOptionsForDriftTests(&tokens, &g_fQuickFail); // use default behavior if this fails

        if (returnVal == TPR_FAIL)
        {
            goto cleanAndExit;
        }
    }

    /* use a special case driver function to run these tests */
    if (lpFTE->dwUserData == DRIFT_OEMIDLE_PERIODIC ||
        lpFTE->dwUserData == DRIFT_OEMIDLE_RANDOM)
    {
        if (!compareThreeClocksTrackIdle(&getTickCount, &hiPerf, &rtc,
                       &boundGtcToHiPerf, &boundGtcToRtc,
                       &boundHiPerfToRtc,
                       dwSampleStepS, dwTestRunTimeS, fSleep))
        {
            returnVal = TPR_FAIL;
            goto cleanAndExit;
        }
    }
    else
    {
        if (!compareThreeClocks(&getTickCount, &hiPerf, &rtc,
                  &boundGtcToHiPerf, &boundGtcToRtc,
                  &boundHiPerfToRtc,
                  dwSampleStepS, dwTestRunTimeS, fSleep))
        {
            returnVal = TPR_FAIL;
            goto cleanAndExit;
        }
    }

cleanAndExit:
    return (returnVal);

}

BOOL
compareThreeClocksTrackIdle(sTimer * timer1, sTimer * timer2, sTimer * timer3,
                sBounds * bound1To2,
                sBounds * bound1To3,
                sBounds * bound2To3,
                DWORD dwSampleStepS,
                DWORD dwTestRunTimeS,
                BOOL (*fSleep)(DWORD dwTimeMS))
{
    DWORD littleTimes[] = {5, 10, 20, 40, 60, 80, 100, 150,
             200, 250, 300, 350, 400, 450, 500, 550,
             600, 650, 700, 750, 800, 900,
             1000};
    /*, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000,
             10000, 11000, 12000, 13000, 14000, 15000, 16000, 17000,
             18000, 19000, 20000, 21000, 22000, 23000, 24000, 25000};
    */
    /*
    DWORD littleTimes[] = {
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101,
    102, 103, 104, 105, 106, 107, 108, 109, 110,
    111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128,
    129, 130, 131, 132, 133, 134, 135, 136, 137,
    138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150};
    */

    setInnerSleepIntervalsMS (littleTimes, _countof(littleTimes));

    return compareThreeClocks(timer1, timer2, timer3,
              bound1To2, bound1To3, bound2To3,
              dwSampleStepS, dwTestRunTimeS, fSleep);

}



/*****************************************************************************
 *
 *                           Wall Clock Tests
 *
 *****************************************************************************/

/*
 * max number of status statements to print during the wall clock test.  This
 * isn't followed exactly, but prevents thousands of lines of output.
 */
#define WALL_CLOCK_NUM_STATUS_STATEMENTS 100

/* wrapper for the gtc test */
TESTPROCAPI wallClockDriftTestGTC (
                   UINT uMsg,
                   TPPARAM tpParam,
                   LPFUNCTION_TABLE_ENTRY lpFTE)
{
    return (wallClockDriftTest(&gGetTickCountTimer, uMsg, tpParam, lpFTE));
}

/* wrapper for the hi perf test */
TESTPROCAPI wallClockDriftTestHP (
                   UINT uMsg,
                   TPPARAM tpParam,
                   LPFUNCTION_TABLE_ENTRY lpFTE)
{
    return (wallClockDriftTest(&gHiPerfTimer, uMsg, tpParam, lpFTE));
}

/* wrapper for the rtc test */
TESTPROCAPI wallClockDriftTestRTC (
                   UINT uMsg,
                   TPPARAM tpParam,
                   LPFUNCTION_TABLE_ENTRY lpFTE)
{
    return (wallClockDriftTest(&gTimeOfDayTimer, uMsg, tpParam, lpFTE));
}

/*
 * A standard version of the wall clock drift test.  The timerToInitFrom is a
 * global structure container the information for a given timer.  This will be
 * placed in readonly mem, so it can never be written to.  Doing so will cause
 * an exception.
 */
TESTPROCAPI wallClockDriftTest(sTimer * timerToInitFrom,
                   UINT uMsg,
                   TPPARAM tpParam,
                   LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT iReturnVal = TPR_PASS;

    cTokenControl tokens;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    sTimer timer = { 0 };
    /* initialize timer struct */
    if (initTimer (&timer, timerToInitFrom) == FALSE)
    {
        Error (_T("Could not initalize the %s timer structure."),
         timerToInitFrom->name);
        Error (_T("Maybe the timer doesn't exist?"));

        iReturnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    /* test length is seconds */
    DWORD dwTestLengthS = 0;
    ULONGLONG ullTestLengthS = 0;
    double doTime  = 0;
    TCHAR* szUnits = NULL;

    if (g_szDllCmdLine && tokenize ((TCHAR *)g_szDllCmdLine, &tokens) &&
        getOptionTimeUlonglong (&tokens, ARG_STR_WALL_CLOCK_RUN_TIME,
                  &ullTestLengthS))
    {
        dwTestLengthS = (DWORD) ullTestLengthS;
        if (dwTestLengthS == 0)
        {
            Error (_T("Can't run for zero minutes."));
            iReturnVal = TPR_FAIL;
            goto cleanAndReturn;
        }
        realisticTime ((double) dwTestLengthS, doTime, &szUnits);

        Info (_T("User specified test length of %g %s."), doTime, szUnits);
    }
    else
    {
        /* use default */
        dwTestLengthS = WALL_CLOCK_DEFAULT_TEST_LENGTH_S;
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
 */
BOOL
runResolution(sTimer * timer,
          DWORD dwIterations,
          ULONGLONG * pullN, ULONGLONG * pullRange,
          int iNewPriority)
{
    if (!timer || !pullN || !pullRange)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /* for use with realisticTime */
    {
    double doPrtTime;
    _TCHAR * szPrtUnits;
    realisticTime (timer->doConversionToS, doPrtTime, &szPrtUnits);

    Info (_T("1 tick of %s represents %g %s.  This is NOT the resolution."),
      timer->name, doPrtTime, szPrtUnits);
    }

    Info (_T("Calculating the resolution of timer %s."), timer->name);

    int iOldPriority;

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

    BOOL rVal = getResolution(timer, dwIterations, pullN, pullRange, TRUE);

    /* set the thread priority back before potentially bailing out on error */
    if (handleThreadPriority (iOldPriority, NULL) == FALSE)
    {
        Error (_T("Could not set thread priority back to %i."),
         iOldPriority);
        goto cleanAndExit;
    }
    else if (iNewPriority != DONT_CHANGE_PRIORITY)
    {
        Info (_T("Thread priority set back to %i."),
        iOldPriority);
    }

    if (!rVal)
    {
        Error (_T("Failed to get resolution for timer %s."), timer->name);
        goto cleanAndExit;
    }

    ULONGLONG ullUpper = *pullN + *pullRange;

    /* this should never happen because of getResolution */
    if (ullUpper < (MAX(*pullN, *pullRange)))
    {
        IntErr (_T("Overflow: *pullN + *pullRange"));
        goto cleanAndExit;
    }

    Info (_T(""));
    Info (_T("Resolution ranged from %I64u ticks to %I64u ticks, inclusive."),
    *pullN, ullUpper);

    {
    double doPrtTime1;
    _TCHAR * szPrtUnits1;
    double doPrtTime2;
    _TCHAR * szPrtUnits2;
    realisticTime (timer->doConversionToS * (double) *pullN,
           doPrtTime1, &szPrtUnits1);
    realisticTime (timer->doConversionToS * (double) ullUpper,
           doPrtTime2, &szPrtUnits2);

    Info (_T("This is [%g %s, %g %s]."),
      doPrtTime1, szPrtUnits1, doPrtTime2, szPrtUnits2);
    }

    returnVal = TRUE;

 cleanAndExit:

    return (returnVal);
}

/*
 * Runs the backwards check test.
 *
 * Timer can't be null.  run time is given in seconds and can't be zero.
 */
BOOL
runBackwardsCheck (sTimer * timer, ULONGLONG ullRunTimeS,
           BOOL (*fSleep)(DWORD dwTimeMS),
           DWORD dwSleepTimeMS)
{
    cTokenControl tokens;

    if (!timer || ullRunTimeS == 0)
    {
        return FALSE;
    }

    BOOL returnVal = FALSE;

    /* how often to print messages telling how long we have been running */
    DWORD dwPrintAtTimeS = BACKWARDS_CHECK_PRINT_MESSAGE_EVERY_N_S;

    /*
   * use the gtc to determine test length.
   */
    sTimer gtc = { 0 };
    //  if (initTimer (&gtc, &gTimeOfDayTimer) == FALSE)
    if (initTimer (&gtc, &gGetTickCountTimer) == FALSE)
    {
        Error (_T("Could not initalize the GTC timer structure."));
        goto cleanAndReturn;
    }

    /* find the resolution of the clock */
    ULONGLONG ullRes, ullResRange;

    if (!getResolution (timer, BACKWARDS_CHECK_CALC_RESOLUTION_ITERATIONS,
              &ullRes, &ullResRange, FALSE))
    {
        Error (_T("Couldn't calculate resolution for %s."), timer->name);

        goto cleanAndReturn;
    }

    /*
   * Distance (time) between clock reads.  This must be greater than the
   * largest possible allowable distance.
   */

    ULONGLONG ullResThreshold = 0;

    if (fSleep)
    {
        /*
       * if using a sleep function the threshold must be larger than the max
       * sleep time.  The OS sleep can return any time *after* the given
       * value.  The WaitRandomTime function (in this test code) waits for
       * a time smaller than the given time.  All things considered, a good
       * threshold is the sleep time doubled.  Arbitrary, but good enough to
       * work.
       */
        ullResThreshold = (ULONGLONG) (((double) dwSleepTimeMS / 1000.0 /
                      timer->doConversionToS) * 2.0);

        if (ullResThreshold == 0)
        {
            Error (_T("Resolution threshold is zero."));
            goto cleanAndReturn;
        }
    }
    else
    {
        /*
       * No sleep function supplied, so we will be reading the clock in a busy
       * loop.  Twice the resolution is a generous bound in this case.
       */
        ullResThreshold = ullRes * 2;
    }

    Info (_T("Resolution threshold is set to %I64u ticks = %g s."),
    ullResThreshold, (double) ullResThreshold * timer->doConversionToS);

    ULONGLONG ullPrintAt = 0;

    /**** Figure out how long one test iteration takes ************************/

    Info (_T("Calibrating..."));

    /* find the calibration threshold in the units of the gtc */
    ULONGLONG ullCalibrationThreshold;

    /* make nice times when printing */
    double doRealisticTime;
    _TCHAR * pszRealisticUnits;

    ULONGLONG ullTestCalibrationRuns = 1;


    if (!(g_szDllCmdLine && tokenize ((TCHAR *)g_szDllCmdLine, &tokens) &&
        getOptionTimeUlonglong (&tokens, ARG_STRING_CALIB_THRESHHOLD ,
                  &ullCalibrationThreshold)))
    {
        ullCalibrationThreshold =
            BACKWARDS_CHECK_CALIBRATION_THRESHOLD_S * gtc.ullFreq;
    }

    Info (_T("Calibrating threshold is set to %u ms"),(DWORD) ullCalibrationThreshold);

    /*
   * these are ulonglong because we are using functions from the timing code,
   * which take ulonglong parameters
   */
    ULONGLONG ullDiff = 0;
    ULONGLONG ullTimeBegin, ullTimeEnd;

    for(;;)
    {
        /* query gtc to establish start time of this run */
        if (!(*gtc.getTick)(ullTimeBegin))
        {
            Error (_T("Couldn't get start time from gtc."));
            goto cleanAndReturn;
        }

        /*
         * if we don't print messages during calibration the compiler removes the
         * statement that calculates whether we need to print messages.  This causes
         * the loop to run about 10 times faster for calibration than for a normal
         * run.  The test times are off in this case.
         *
         * Disabling different parts of the optimizer didn't help.  Easiest way is
         * to give it an insanely value for print
         */
        if (!doesClockGoBackwards (timer, ullTestCalibrationRuns,
                 ullResThreshold, 100000000,
                 fSleep, dwSleepTimeMS))
        {
            Error (_T("Test failed during calibration."));
            Error (_T("Calibration runs the test for short periods of time, so ")
            _T("something is wrong with the clock."));
            goto cleanAndReturn;
        }

        /* query gtc to establish end time of this run */
        if (!(*gtc.getTick)(ullTimeEnd))
        {
            Error (_T("Couldn't get end time from gtc."));
            goto cleanAndReturn;
        }

        /* subtract, accounting for overflow */
        if (!AminusB (ullDiff, ullTimeEnd, ullTimeBegin, ULONGLONG_MAX))
        {
        /*
         * This should never happen.  If something goes wrong, then there is
         * a bug in the code.
         */
            IntErr (_T("AminusB failed"));
            goto cleanAndReturn;
        }

        if (ullDiff < ullCalibrationThreshold)
        {
            /* if within threshold double number of runs and try again */
            ULONGLONG prev = ullTestCalibrationRuns;

            ullTestCalibrationRuns *= 2;

            if (prev >= ullTestCalibrationRuns)
            {
          /*
           * we overflowed.  We could try to recover but it is
           * much easier to bail at this point.  If this happens
           * the test routine is running very quickly or something
           * is wrong with either the timers or something else.
           *
           * This should be a very rare error.
           */
                Error (_T("Could not calibrate.  We overflowed the variable "));
                Error (_T("storing the number of calibration runs.  Either the "));
                Error (_T("test routines are running very fast or the calibration"));
                Error (_T("threshold is set too high.  Reduce the calibration"));
                Error (_T("time (in ms) by setting the -CalibThresh option."));
                Error (_T("The default value is 10000 ms."));

                goto cleanAndReturn;
            }
        }
        else
        {
            /* we are above the threshold, so we are done */
            break;
        }
    } /* while */

    /*
   * we now know how many test iterations can be done in the calibration threshold.
   */

    /* The time for the calibration run in seconds */
    double doDiffS = ((double) ullDiff) * gtc.doConversionToS;

    /* iterations = totalRunTime * it / diff */
    ULONGLONG ullIterations = (ULONGLONG)
    (((double) ullRunTimeS) * (((double) ullTestCalibrationRuns) / doDiffS));

    /* figure out at which times to print messages */
    ullPrintAt = (ULONGLONG)
    (((double) dwPrintAtTimeS) * (((double) ullTestCalibrationRuns) / doDiffS));

    if (ullIterations == 0)
    {
        Error (_T("Can't run for zero iterations.  Constants are incorrect."));
        goto cleanAndReturn;
    }

    Info (_T("We are going to do %I64u iterations for this test."), ullIterations);

    realisticTime ((double) ullRunTimeS,
         doRealisticTime, &pszRealisticUnits);
    Info (_T("It should take about %g %s."),
    doRealisticTime, pszRealisticUnits);

    Info (_T(""));
    Info (_T("Test starting now"));
    Info (_T(""));

    if (!doesClockGoBackwards (timer, ullIterations, ullResThreshold, ullPrintAt,
                 fSleep, dwSleepTimeMS))
    {
        Error (_T("Test failed!"));
        goto cleanAndReturn;
    }

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}

