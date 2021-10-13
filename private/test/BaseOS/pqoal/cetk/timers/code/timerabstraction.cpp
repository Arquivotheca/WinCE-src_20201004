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
* timerAbstraction.cpp
*/

#include "timerAbstraction.h"
#include <Storemgr.h>
#include <math.h>

BOOL g_fQuickFail = FALSE;

BOOL g_bFatalErr = FALSE;

#ifdef UNDER_NT
/*
* NOTE: The below doesn't mean that this code currently supports NT.
* It does not.
*
* These are not implemented on nt, so stub them out so that this will
* compile.
*
* Test procs that rely on these will always fail out with an error.
*/
DWORD
CeGetThreadQuantum (HANDLE x)
{
    return (FALSE);
}

DWORD
CeSetThreadQuantum (HANDLE x, DWORD p)
{
    return (FALSE);
}
#endif


/*
* number of iterations needed to get the resolution to a good enough
* granularity. This is arbitrary. So far 10 has been working fine.
*
* This is different from test length for the resolution test. That is
* defined separately so that the user can change it as need be. We want
* to keep this independent because it is expected that the user has verified
* the resolution before running the tests that rely upon it. If they don't
* do this then all bets are off.
*/
#define GET_RESOLUTION_ITERATIONS 10


/******* Resolution Calculations **********************************************/

/* get one data point for the resolution calculation */
BOOL getResolutionOnce (sTimer * timer,
                        ULONGLONG * ullDiff, ULONGLONG *);

/* return the resolution of a clock in seconds */
/* fixme - move to header? */
BOOL getResolutionInS (sTimer * timer,
                       DWORD dwNumIterations,
                       double * pdoN, double * pdoRange,
                       BOOL bPrintSpew);

/* get the loosest resolution of three clocks */
BOOL getLoosestResolution (sTimer * timer1, sTimer * timer2, sTimer * timer3,
                           ULONGLONG * pullLoosestResolution1To2,
                           ULONGLONG * pullLoosestResolution1To3,
                           ULONGLONG * pullLoosestResolution2To3);

/* get the loosest resolution of two clocks */
BOOL getLoosestResolution (sTimer * timer1, sTimer * timer2,
                           ULONGLONG * pullLoosestResolution);

/* helper for getLoosestResolution */
BOOL calcLoosestResolution (double doRes1S, double doRes2S,
                            ULONGLONG * pullLoosest, double doConv);

/* prints the resolution data in histogram format */
void
printHistogram (const ULONGLONG * pullData, DWORD dwDataLength);

/******* Comparing clocks ***********************************************/

/*
* return values for getSamplesAllThree.
*
* Each comparison gets one bit. If it is set the test worked. If it
* isn't set the test didn't work. If all bits are not set (CLOCK_FALSE)
* then something else went wrong. CLOCK_TRUE is a simple way of checking
* that all tests passed.
*/
#define CLOCK_ONE_TO_TWO 0x1
#define CLOCK_ONE_TO_THREE 0x2
#define CLOCK_TWO_TO_THREE 0x4
#define CLOCK_TRUE 0x7
#define CLOCK_FALSE 0x0

/* macro to take the absolute value of a double */
#define DOUBLE_ABS(x) ((x) > 0.0 ? (x) : (-1.0 * (x)))

/*
* get samples for all three clocks
*
* return value is a bit field described by thte CLOCK_* macros
*/
DWORD
getSamplesAllThree (sTimer * timer1, sTimer * timer2, sTimer * timer3,
                    __out_ecount(1+dwNumSamples) ULONGLONG * pullTimer1Samples,
                    __out_ecount(1+dwNumSamples) ULONGLONG * pullTimer2Samples,
                    __out_ecount(1+dwNumSamples) ULONGLONG * pullTimer3Samples,
                    DWORD dwNumSamples,
                    DWORD dwSleepTimeMS,
                    sBounds * bound1To2, sBounds * bound1To3, sBounds * bound2To3,
                    BOOL (*fSleep)(DWORD dwTimeMS));

/* convert a sample from the relative format to the absolute format */
BOOL
convertOneSample (sTimer * timer1, sTimer * timer2, sTimer * timer3,
                  ULONGLONG ullS1Prev, ULONGLONG ullS2Prev, ULONGLONG ullS3Prev,
                  ULONGLONG ullS1, ULONGLONG ullS2, ULONGLONG ullS3,
                  ULONGLONG ullPrevS1Abs, ULONGLONG ullPrevS2Abs,
                  ULONGLONG ullPrevS3Abs,
                  ULONGLONG * ullS1Abs, ULONGLONG * ullS2Abs,
                  ULONGLONG * ullS3Abs,
                  double doConvMultByS2, double doConvMultByS3);

BOOL
convertOneSample (sTimer * timer1, sTimer * timer2,
                  ULONGLONG ullS1Prev, ULONGLONG ullS2Prev,
                  ULONGLONG ullS1, ULONGLONG ullS2,
                  ULONGLONG ullPrevS1Abs, ULONGLONG ullPrevS2Abs,
                  ULONGLONG * ullS1Abs, ULONGLONG * ullS2Abs,
                  double doConvMultByS2);

/* compare two sets of samples, printing them out */
BOOL
printSamplesForTwoClocks (sTimer * timer1, sTimer * timer2,
                          const ULONGLONG * pullTimer1Samples,
                          const ULONGLONG * pullTimer2Samples,
                          DWORD dwNumSamples,
                          sBounds * bound1To2);
void
printStatus (DWORD status, const TCHAR * szT1, const TCHAR * szT2);

/* check that bounds arguments are correct */
BOOL
boundsCheck (sBounds * b, double doConvToS);

/* calculate whether a given sample is within bounds */
BOOL
isSampleWithinBounds(sTimer * timer1, sTimer * timer2,
                     ULONGLONG ullSample1, ULONGLONG ullSample2,
                     double doFasterBound, double doSlowerBound,
                     ULONGLONG ullLoosestResBound,
                     BOOL bPrintDebuggingMessages = TRUE);

/* round a value to a multiple of a given edge */
BOOL roundDown (ULONGLONG ullVal, ULONGLONG ullEdge, ULONGLONG * pullAns);
BOOL specialRoundDown (ULONGLONG ullVal, ULONGLONG ullEdge, ULONGLONG * pullAns);

/*
* round a value to a multiple of a given edge. Handle values on the
* edge specially
*/
BOOL specialRoundUp (ULONGLONG ullVal, ULONGLONG ullEdge, ULONGLONG * pullAns);

/******* Sleep Routines ****************************************************/

/*
* busy sleep routines. With this all clock rely upon there own time for
* sleep. Alternative is the syscall Sleep, which relys on gtc no matter
* which clock you are testing.
*
* First routine sleeps for a given number of ms. Second sleeps for a time
* givin in the units of the specified clock.
*/
BOOL
abstractSleepMS (sTimer * timer, ULONGLONG ullSleepTimeMS);

BOOL
abstractSleep (sTimer * timer, ULONGLONG ullSleepTime);



/***************************************************************************
*
* Initialization
*
***************************************************************************/

/*
* Initialize a timer data structure. This involves setting the ullFreq and
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
* Resolution Calculations
*
***************************************************************************/

/*
* get the resolution, in ticks, of a given clock.
*
* timer is an initialized timer struct. dwNumIterations is the
* number of iterations to be done. It can't be zero. pullN and
* pullRange represent the smallest resolution and the largest
* resolution. largest - smallest = range. bPrintSpew tells whether
* the funtion should print debug spew on success. If you are using
* this for resolution testing enable this. If you are using this to
* garther the resolution for other tests then disable it.
*
* returns true on success and false otherwise. On false the return
* values are undefined.
*/
BOOL getResolution (sTimer * timer,
                    DWORD dwNumIterations,
                    ULONGLONG * pullN, ULONGLONG * pullRange,
                    BOOL bPrintSpew)
{
    if (!timer || (dwNumIterations == 0) || !pullN || !pullRange)
    {
        return FALSE;
    }

    BOOL returnVal = FALSE;

    DWORD dwNeededMem = 0;
    ULONGLONG * pullDiffs = NULL;
    ULONGLONG * pullEnds = NULL;

    if (DWordMult (sizeof (ULONGLONG), dwNumIterations, &dwNeededMem) != S_OK)
    {
        Error (_T("Overflow when calculating memory requirements."));
        goto cleanAndReturn;
    }

    /* track the diffs */
    pullDiffs = (ULONGLONG *) malloc (dwNeededMem);
    /* track the ending times */
    pullEnds = (ULONGLONG *) malloc (dwNeededMem);

    if (!pullDiffs || !pullEnds)
    {
        Error (_T("Could not allocate memory"));
        goto cleanAndReturn;
    }

    /* collect the samples */
    for (DWORD dwCurr = 0; dwCurr < dwNumIterations; dwCurr++)
    {
        if (!getResolutionOnce (timer, pullDiffs + dwCurr, pullEnds + dwCurr))
        {
            Error (_T("Couldn't get resolution for interation %u."),
                dwCurr);
            goto cleanAndReturn;
        }
    }

    /***** print data ******/

    /* first, print out the data, and find the minimum and maximum */

    /*
    * initialize the indexes and the values
    *
    * These values will be immediately overwritten the first time
    * through the loop. The zeros for indexes are just for completeness.
    */
    ULONGLONG ullSmallestSoFar = ULONGLONG_MAX;
    DWORD dwSmallestSoFarIndex = 0;
    ULONGLONG ullLargestSoFar = 0;
    DWORD dwLargestSoFarIndex = 0;

    /*
    * the first entry is always too much. Probably tlb misses and the like.
    * ignore it.
    */
    for (DWORD dwCurr = 1; dwCurr < dwNumIterations; dwCurr++)
    {
        if (bPrintSpew)
        {
            Info (_T("Iteration %5.u Resolution %8.I64u tick value %14.I64u"),
                dwCurr, pullDiffs[dwCurr], pullEnds[dwCurr]);
        }

        if (pullDiffs[dwCurr] < ullSmallestSoFar)
        {
            ullSmallestSoFar = pullDiffs[dwCurr];
            dwSmallestSoFarIndex = dwCurr;
        }

        if (pullDiffs[dwCurr] > ullLargestSoFar)
        {
            ullLargestSoFar = pullDiffs[dwCurr];
            dwLargestSoFarIndex = dwCurr;
        }
    }

    if (bPrintSpew)
    {
        Info (_T(""));
        Info (_T("First low encountered is %I64u at pos %u. ")
            _T("First high is %I64u at pos %u."),
            ullSmallestSoFar, dwSmallestSoFarIndex,
            ullLargestSoFar, dwLargestSoFarIndex);

        Info (_T(""));
        Info (_T("Histogram"));
        printHistogram (pullDiffs + 1, dwNumIterations - 1);
    }

    /*
    * at this point ullSmallest/largestSoFar have been loaded
    * with there respective value. Same for the index values.
    */
    *pullN = ullSmallestSoFar;
    *pullRange = ullLargestSoFar - ullSmallestSoFar;

    returnVal = TRUE;
cleanAndReturn:

    if(pullDiffs)
    {
        free (pullDiffs);
    }

    if(pullEnds)
    {
        free (pullEnds);
    }

    return (returnVal);
}

/*
* Get the resolution of the given clock once.
*
* timer is an initialized timer structure. pullDiff returns the difference
* (resolution) calculated for this given run. pullEnd is the ending timer
* read on the clock after the resolution has been taken.
*
* return true on success and false otherwise.
*/
BOOL getResolutionOnce (
                        sTimer * timer,
                        ULONGLONG * pullDiff,
                        ULONGLONG * pullEnd)
{
    if (!timer || !pullDiff || !pullEnd)
    {
        return (FALSE);
    }

    /* be pessimistic */
    BOOL returnVal = FALSE;

    ULONGLONG ullStartTime = { 0 }, ullEndTime = { 0 };

    /*
    * handle to the current thread. This doesn't need to be
    * closed when processing is complete.
    */
    HTHREAD hThisThread = GetCurrentThread ();

    /*
    * return value from the getTick function. Have two so that we
    * can check it afterwards
    */
    BOOL bGetTickReturnVal, bRval1;

    /* get the quantum so that we can set this back */
    DWORD dwOldThreadQuantumMS = CeGetThreadQuantum (hThisThread);

    if (dwOldThreadQuantumMS == DWORD_MAX)
    {
        Error (_T("Couldn't get the thread quantum. GetLastError is %u."),
            GetLastError ());
        goto cleanAndReturn;
    }

    /* set the thread quantum to 0, meaning that we will never be swapped out */
    if (!CeSetThreadQuantum (hThisThread, 0))
    {
        Error (_T("Couldn't set the thread quantum. GetLastError is %u."),
            GetLastError ());
        goto cleanAndReturn;
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
    * put myself at the beginning of the OS tick. If we don't do this we
    * can get a hardware interrupt in the middle which will confuse the test
    */
    DWORD prevGTC = GetTickCount();
    while (GetTickCount() == prevGTC);

    /* get the timer to start at */
    if ((*timer->getTick)(ullEndTime) == FALSE)
    {
        Error (_T("Failed to get tick count for %s."),
            timer->name);

        goto cleanAndReturn;
    }

    /* find beginning of tick */

    bRval1 = (*timer->getTick)(ullStartTime);
    while( bRval1 && ullStartTime == ullEndTime )
    {
        bRval1 = (*timer->getTick)(ullStartTime);
    }

    if(!bRval1) {
        goto cleanAndReturn;
    }

    /* main timing loop */
    for(;;)
    {
        bGetTickReturnVal = (*timer->getTick)(ullEndTime);
        if(!bGetTickReturnVal ||
            (ullStartTime != ullEndTime))
        {
            break;
        }
    }

    /* save the ending time */
    *pullEnd = ullEndTime;

    if (!bGetTickReturnVal || !bRval1)
    {
        /* bad call to timer at some point */
        Error (_T("Failed to get tick for %s in main loop."), timer->name);

        goto cleanAndReturn;
    }

    /* find diff */
    if (!AminusB (*pullDiff, ullEndTime, ullStartTime, timer->ullWrapsAt))
    {
        goto cleanAndReturn;
    }

    returnVal = TRUE;

cleanAndReturn:

    if (dwOldThreadQuantumMS != DWORD_MAX)
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

/*
* return the resolution in seconds.
*
* Timer can't be null and must be initialized. dwNumIterations tells
* us how many data points to collect. pdoN and pdoRange are the
* return values. These give the low point and the range of the
* resolution numbers. These are in seconds. bPrintSpew is false if
* you want no info messages and true otherwise. Error messages are
* still printed on error.
*
* The return value isn't checked against any bounds to confirm that
* it is good.
*
* Return true on success and false otherwise.
*/
BOOL getResolutionInS (sTimer * timer,
                       DWORD dwNumIterations,
                       double * pdoN, double * pdoRange,
                       BOOL bPrintSpew)
{
    ULONGLONG ullN, ullRange;

    if (getResolution (timer, dwNumIterations, &ullN, &ullRange, bPrintSpew))
    {
        *pdoN = ((double) ullN) * timer->doConversionToS;
        *pdoRange = ((double) ullRange) * timer->doConversionToS;
        return (TRUE);
    }
    else
    {
        return (FALSE);
    }
}

/*
* This function calculates the loosest resolution for timer1 and
* timer2. This isn't the precision. This is the resolution that the
* clocks can actually do.
*
* The tests need to know with what accuracy they can trust the
* clocks. The clock might have quite precise units but often it
* won't tick with this precision. This function calculates the true
* real life resolution on the fly.
*
* timer1 and timer2 are initialized timer structs.
* pullLoosestResolution is a pointer to a ulonglong where the data is
* stored. It can't be null.
*
* This function returns true if it could obtain the resolution and
* false otherwise.
*/
BOOL
getLoosestResolution (sTimer * timer1, sTimer * timer2, sTimer * timer3,
                      ULONGLONG * pullLoosestResolution1To2,
                      ULONGLONG * pullLoosestResolution1To3,
                      ULONGLONG * pullLoosestResolution2To3)
{
    if (!timer1 || !timer2 || !pullLoosestResolution1To2)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    double doResT1S = 0, doResT2S = 0, doResT3S = 0, doResRangeT1 = 0, doResRangeT2 = 0, doResRangeT3 = 0;

    /* the resolution numbers from these functions have units of seconds */
    if (!getResolutionInS (timer1, GET_RESOLUTION_ITERATIONS,
        &doResT1S, &doResRangeT1, FALSE))
    {
        Error (_T("Could not get resolution of clock %s."),
            timer1->name);
        goto cleanAndReturn;
    }

    if (!getResolutionInS (timer2, GET_RESOLUTION_ITERATIONS,
        &doResT2S, &doResRangeT2, FALSE))
    {
        Error (_T("Could not get resolution of clock %s."),
            timer2->name);
        goto cleanAndReturn;
    }

    if (timer3 &&
        !getResolutionInS (timer3, GET_RESOLUTION_ITERATIONS,
        &doResT3S, &doResRangeT3, FALSE))
    {
        Error (_T("Could not get resolution of clock %s."),
            timer2->name);
        goto cleanAndReturn;
    }

    BOOL bRet = TRUE;

    /* convertion value is provided by the first clock */
    if (!calcLoosestResolution (doResT1S, doResT2S, pullLoosestResolution1To2,
        timer1->doConversionToS))
    {
        bRet = FALSE;
    }

    if (timer3 &&
        !calcLoosestResolution (doResT1S, doResT3S, pullLoosestResolution1To3,
        timer1->doConversionToS))
    {
        bRet = FALSE;
    }

    if (timer3 &&
        !calcLoosestResolution (doResT2S, doResT3S, pullLoosestResolution2To3,
        timer2->doConversionToS))
    {
        bRet = FALSE;
    }

    if (!bRet)
    {
        goto cleanAndReturn;
    }

    returnVal = TRUE;
cleanAndReturn:

    return (returnVal);

}

/*
* This function calculates the loosest resolution for timer1 and
* timer2. This isn't the precision. This is the resolution that the
* clocks can actually do.
*
* The tests need to know with what accuracy they can trust the
* clocks. The clock might have quite precise units but often it
* won't tick with this precision. This function calculates the true
* real life resolution on the fly.
*
* timer1 and timer2 are initialized timer structs.
* pullLoosestResolution is a pointer to a ulonglong where the data is
* stored. It can't be null.
*
* This function returns true if it could obtain the resolution and
* false otherwise.
*/
BOOL
getLoosestResolution (sTimer * timer1, sTimer * timer2,
                      ULONGLONG * pullLoosestResolution)
{
    return (getLoosestResolution (timer1, timer2, NULL,
        pullLoosestResolution, NULL, NULL));
}

/*
* Takes two doubles, representing resolutions in seconds. doConv
* provides a conversion value from seconds into pullLoosest. This
* function calculates which value is numerically greater and then
* converts it into the units of pullLoosest.
*
* It is a helper for getLoosestResolution.
*/
BOOL
calcLoosestResolution (double doRes1S, double doRes2S,
                       ULONGLONG * pullLoosest, double doConv)
{
    if (!pullLoosest || doConv == 0.0)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    double doLoosestResolution;

    /* find the max */
    if (doRes1S > doRes2S)
    {
        doLoosestResolution = doRes1S;
    }
    else
    {
        doLoosestResolution = doRes2S;
    }

    /* convert from seconds to the units for timer1 */
    double doLoosestResolutionT1units = doLoosestResolution / doConv;

    if ((doLoosestResolutionT1units < 0.0) ||
        (doLoosestResolutionT1units > (double) ULONGLONG_MAX))
    {
        Error (_T("Overflow when converting the loosest resolution."));
        goto cleanAndReturn;
    }

    /* convert into a ulonglong */
    *pullLoosest = (ULONGLONG) doLoosestResolutionT1units;

    /* if the loosest resolution is zero then something went wrong somewhere */
    if (*pullLoosest == 0)
    {
        Error (_T("The calculated resolution is zero. The clock must not be"));
        Error (_T("ticking, which shouldn't occur."));
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    returnVal = TRUE;
cleanAndReturn:

    return (returnVal);
}


/*
* Print the number of times values appear in an array.
* This is essentially a text histogram.
*
* pullData is the array. dwDataLength is the array length.
*
* The algorithm:
*
* Find the smallest value. This becomes the working value.
*
* Count how many instances of working value are in the array. Track
* the next largest value so that this can become the working value
* the next time. We are done when we have counted all of the
* elements.
*
* This is designed to be simple to code and to work well with
* data that has only a few values. This algorithm makes one trip
* through the array for each distinct value in the array.
*/
void
printHistogram (const ULONGLONG * pullData, DWORD dwDataLength)
{
    if (!pullData)
    {
        return;
    }

    DWORD dwNumAnalyzed = 0;
    ULONGLONG ullWorkingOn = ULONGLONG_MAX;
    ULONGLONG ullNextLeast = ULONGLONG_MAX;

    /* find least */
    for (DWORD dwCurr = 0; dwCurr < dwDataLength; dwCurr++)
    {
        if (pullData[dwCurr] < ullWorkingOn)
        {
            ullWorkingOn = pullData[dwCurr];
        }
    }

    /* workingOn has the smallest value in the array */
    while (dwNumAnalyzed < dwDataLength)
    {
        DWORD dwFoundForThisWorking = 0;

        for (DWORD dwCurr = 0; dwCurr < dwDataLength; dwCurr++)
        {
            /* we found a match */
            if (pullData[dwCurr] == ullWorkingOn)
            {
                dwFoundForThisWorking++;
                dwNumAnalyzed++;

                /* bail if we have found all elements */
                if (dwNumAnalyzed == dwDataLength)
                {
                    break;
                }
            }
            else
            {
                /* zero in on the next greatest value */
                if ((pullData[dwCurr] < ullNextLeast) &&
                    (pullData[dwCurr] > ullWorkingOn))
                {
                    ullNextLeast = pullData[dwCurr];
                }
            }
        }

        Info (_T("Value %6.I64u found %6.u times"),
            ullWorkingOn, dwFoundForThisWorking);

        /* set working on to the next value */
        if (ullNextLeast < ullWorkingOn)
        {
            IntErr (_T("ullNextLeast < ullWorkingOn %I64u < %I64u"),
                ullNextLeast, ullWorkingOn);
        }

        ullWorkingOn = ullNextLeast;
        ullNextLeast = ULONGLONG_MAX;
    }

    return;
}

/***************************************************************************
*
* Utility Functions for Wall Clock
*
***************************************************************************/

#define READ_MESSAGE_TIME_S 20

#define TEN_MINUTES_IN_S (10 * 60)
#define THIRTY_SECONDS_IN_S (30)
#define _100th_ns (0.0000001)
#define EPSILON (1.0)

enum clock_state {START_CLOCK, STOP_CLOCK};
enum time_interval_state{START, INTERVAL, STOP};


/*
 * This function will return true if the device's release directory is on the workstation
 */
BOOL relFRD()
{
    CE_VOLUME_INFO attrib = {0};

    BOOL fRet = CeGetVolumeInfo(L"\\Release", CeVolumeInfoLevelStandard, &attrib); // dwAttributes 0x0 dwFlags 0x10
    if (!fRet)
    {
        return FALSE;
    }
    else if (attrib.dwFlags == CE_VOLUME_FLAG_NETWORK)
    {
        return TRUE;
    }
    return FALSE;
}

/*
 * This function uses the desktops filesystem to track time.
 * This is used to verify that the device time is not drifting
 */
BOOL DeskTopClock(clock_state state, HANDLE *phFile, ULONGLONG total_time, sTimer *timer)
{
    DWORD dwRet, cb = dwRet = 0;
    BOOL fRet = FALSE;
    WCHAR fileName[MAX_PATH+1], path[MAX_PATH+1];
    FILETIME ftCreate = {0};
    FILETIME ftWrite = {0};
    FILETIME ftDiff = {0};
    ULARGE_INTEGER uiStart = {0};
    ULARGE_INTEGER uiDiff = {0};
    ULARGE_INTEGER uiEnd = {0};
    SYSTEMTIME stTotalTime = {0};
    double wallClockTime = 0.0;
    double doPrtTime = 0;
    _TCHAR * szPrtUnits = NULL;

    ASSERT(phFile);

    if (state == START_CLOCK)
    {
        *phFile = INVALID_HANDLE_VALUE;
        dwRet = GetModuleFileName(NULL, path, _countof(path));
        if (dwRet == 0)
        {
            Error (_T("Could not find scratch location: Error (%d)"), GetLastError());
            return FALSE;
        }

        WCHAR *p = wcsrchr(path, L'\\');
        *++p = L'\0';
        dwRet = GetTempFileName(path, L"BTS", 0, fileName);
        if (dwRet == 0)
        {
            Error (_T("Could not create temp file: Error (%d)"), GetLastError());
            return FALSE;
        }

        *phFile = CreateFile(fileName, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (*phFile == INVALID_HANDLE_VALUE)
        {
            Error (_T("Could not create file $s: Error (%d)"), fileName, GetLastError());
            return FALSE;
        }
    }
    else if (state == STOP_CLOCK)
    {
        fRet = WriteFile(*phFile, "hello world", sizeof("hello world"), &cb, NULL);
        if (fRet == FALSE)
        {
            Error (_T("Failed to update LastWritten attribute used to compare device clock to an trusted clock: Error (%d)"), GetLastError());
            return FALSE;
        }
        CloseHandle(*phFile);

        *phFile = CreateFile(fileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        fRet = GetFileTime(*phFile, &ftCreate, NULL, &ftWrite);
        if (fRet == FALSE)
        {
            Error (_T("Failed to update LastWritten attribute used to compare device clock to an trusted clock: Error (%d)"), GetLastError());
            return FALSE;
        }

        DeleteFile(fileName);
        CloseHandle(*phFile);

        uiStart.LowPart = ftCreate.dwLowDateTime;
        uiStart.HighPart = ftCreate.dwHighDateTime;

        uiEnd.LowPart = ftWrite.dwLowDateTime;
        uiEnd.HighPart = ftWrite.dwHighDateTime;

        uiDiff.QuadPart = uiEnd.QuadPart - uiStart.QuadPart;
        ftDiff.dwLowDateTime = uiDiff.LowPart;
        ftDiff.dwHighDateTime = uiDiff.HighPart;

        fRet = FileTimeToSystemTime(&ftDiff, &stTotalTime);
        if (fRet == FALSE)
        {
            Error (_T("Failed to convert FILETIME to SYSTEMTIME: Error (%d)"), GetLastError());
            return FALSE;
        }

        realisticTime ((double) total_time * timer->doConversionToS, doPrtTime, &szPrtUnits);
        Info (_T("The total time for this test run was %g %s."), doPrtTime, szPrtUnits);

        Info (_T("The desktop clock reported that the test took %d minutes and %d seconds."),
            stTotalTime.wMinute, stTotalTime.wSecond);

        // convert FILETIME (100ths of a nano-second) to whatever units the test is measuring in
        realisticTime (floor((double)uiDiff.QuadPart * _100th_ns), wallClockTime, &szPrtUnits);

        if ( fabs(wallClockTime - doPrtTime) > EPSILON)
            return FALSE;
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}


BOOL PrintTimingInfo(time_interval_state state, double sleep_period, DWORD test_length, sTimer *timer)
{
    /* for use with realisticTime */
    double doPrtTime;
    _TCHAR * szPrtUnits;

    if (state == START)
    {
        /* print out test directions */
        realisticTime (sleep_period, doPrtTime, &szPrtUnits);

        Info (_T("We will print status messages so that you will have some warning"));
        Info (_T("and can stop timing at the correct moment. These status messages"));
        Info (_T("will become more frequent as the test nears completion."));

        /*
        * the below isn't exactly correct (for 60 s case actually off by 30 seconds)
        * but it gets the point across. We want to give them a vague idea.
        */
        if (test_length > TEN_MINUTES_IN_S)
        {
            Info (_T("Your first messages will occur about every %g %s."),
                doPrtTime, szPrtUnits);
        }
        else if (test_length > THIRTY_SECONDS_IN_S)
        {
            Info (_T("Your first messages will occur about every minute."));
        }
        else
        {
            Info (_T("Your first message will occur every second."));
        }
        Info (_T(""));

        realisticTime (double (test_length), doPrtTime, &szPrtUnits);
        Info (_T("This test will run for %g %s."), doPrtTime, szPrtUnits);
        Info (_T(""));

        Info (_T("Waiting %u seconds for you to read this and to get ready..."),
            READ_MESSAGE_TIME_S);

        /* allow them to read the above and fumble for their stop watch */
        if (!abstractSleepMS (timer, READ_MESSAGE_TIME_S * 1000))
        {
            Error (_T("something is wrong with sleep for timer %s."),
                timer->name);
            return FALSE;
        }

        Info (_T("Start your clock in:"));

        if (!abstractSleepMS (timer, 1000))
        {
            Error (_T("something is wrong with sleep for timer %s."),
                timer->name);
            return FALSE;
        }

        for (DWORD dwC = 5; dwC != 0 ; dwC--)
        {
            Info (_T("%u"), dwC);

            if (!abstractSleepMS (timer, 1000))
            {
                Error (_T("something is wrong with sleep for timer %s."),
                    timer->name);
                return FALSE;
            }

        }

        Info (_T(""));
        Info (_T("****** Start your clock now!!!! ***********"));
        Info (_T(""));
    }
    else if (state == INTERVAL)
    {
        realisticTime (sleep_period, doPrtTime, &szPrtUnits);
        Info (_T("Current time is %I64u ticks. Time left in test is %I64u ")
            _T("ticks, which is %g %s."), doPrtTime, &szPrtUnits);
    }
    else if (state == STOP)
    {
        realisticTime (sleep_period, doPrtTime, &szPrtUnits);
        Info (_T(""));
        Info (_T("***** Stop your watch now! ******"));
        Info (_T(""));
        Info (_T("The total time for this test run was %g %s."), doPrtTime, szPrtUnits);

        Info (_T("Please confirm this result with what you timed the test to be."));
    }
    return TRUE;
}

/***************************************************************************
*
* Comparing Clocks
*
***************************************************************************/

/******** Three Clock Variety *********************************************/

/*
* See the header file for more info
*/
BOOL
compareThreeClocks(sTimer * timer1, sTimer * timer2, sTimer * timer3,
                   sBounds * bound1To2,
                   sBounds * bound1To3,
                   sBounds * bound2To3,
                   DWORD dwSampleStepS,
                   DWORD dwTestRunTimeS,
                   BOOL (*fSleep)(DWORD dwTimeMS))
{
    if (!timer1 || !timer2 || !timer3 || !bound1To2 || !bound1To3 || !bound2To3)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /* mem for the data samples */
    /* NULL means non allocated. non-null means allocated */

    ULONGLONG * pullTimer1Samples = NULL;
    ULONGLONG * pullTimer2Samples = NULL;
    ULONGLONG * pullTimer3Samples = NULL;

    /*
    * since this is a main entry point confirm with error messages the
    * other variables. This could be called by a user/other test
    * writer, as opposed to the other functions internally which assume
    * a lot more experience/understanding for there use.
    *
    * Do this after the initializations of the memory pointers above.
    */
    if (dwSampleStepS == 0)
    {
        Error (_T("Sample step size cannot be 0."));
        goto cleanAndReturn;
    }

    if (dwTestRunTimeS == 0)
    {
        Error (_T("Test run time cannot be 0."));
        goto cleanAndReturn;
    }

    /* print some info about the timers */
    Info (_T(""));
    Info (_T("Comparing three clocks: %s %s %s"),
        timer1->name, timer2->name, timer3->name);
    Info (_T("For %s, 1 tick equals %g s."),
        timer1->name, timer1->doConversionToS);
    Info (_T("For %s, 1 tick equals %g s."),
        timer2->name, timer2->doConversionToS);
    Info (_T("For %s, 1 tick equals %g s."),
        timer3->name, timer3->doConversionToS);

    Info (_T(""));
    Info (_T("Calculating clock resolutions. Please wait..."));
    Info (_T("Note: If the test spins forever at this point, it is quite"));
    Info (_T(" probable that one of the clocks isn't ticking."));
    Info (_T(""));

    if (!getLoosestResolution (timer1, timer2, timer3,
        &bound1To2->ullLoosestRes,
        &bound1To3->ullLoosestRes,
        &bound2To3->ullLoosestRes))
    {
        Error (_T("Couldn't calculate the loosest resolution of the clocks ")
            _T("being used for this test."));
        goto cleanAndReturn;
    }

    /* check in bound paramters and print data about the bounds */
    if (!boundsCheck (bound1To2, timer1->doConversionToS) ||
        !boundsCheck (bound1To3, timer1->doConversionToS) ||
        !boundsCheck (bound2To3, timer2->doConversionToS))
    {
        goto cleanAndReturn;
    }

    /******* figure out how many data samples will be taken */
    DWORD dwNumOfSamples = 0;

    /* force all samples to be the same length */
    if ((dwTestRunTimeS % dwSampleStepS) != 0)
    {
        Info (_T("Test run time is not a multiple of the sample step size"));
        Info (_T("Rounding up Test run time from %i"),dwTestRunTimeS);
        /* Round up dwTestRunTimeS to be a multiple of dwSampleStepS */
        dwTestRunTimeS = ((dwTestRunTimeS / dwSampleStepS) + 1) * dwSampleStepS;
        Info (_T("to %i"),dwTestRunTimeS);
    }

    dwNumOfSamples = dwTestRunTimeS / dwSampleStepS;

    if (dwNumOfSamples == 0)
    {
        Error (_T("Number of samples is zero."));
        goto cleanAndReturn;
    }

    DWORD dwMAllocBytes;

    /* To handl the boundary case where dwTestRunTimeS == dwSampleStepS */
    /* The buffer needs to be increased by one */
    /* calculate size separately to check for overflow */
    if (DWordMult(dwNumOfSamples+1, sizeof (*pullTimer1Samples),
        &dwMAllocBytes) != S_OK)
    {
        Error (_T("Overflow when calculating memory needs."));
        goto cleanAndReturn;
    }

    /* allocate the memory */
    pullTimer1Samples = (ULONGLONG *) malloc (dwMAllocBytes);
    pullTimer2Samples = (ULONGLONG *) malloc (dwMAllocBytes);
    pullTimer3Samples = (ULONGLONG *) malloc (dwMAllocBytes);

    if (!pullTimer1Samples || !pullTimer2Samples || !pullTimer3Samples)

    {
        Error (_T("Could not allocate memory for sample arrays."));
        Error (_T("getLastError is %u."), GetLastError());
        goto cleanAndReturn;
    }

    /*
    * calculate the wrap time and confirm that is isn't larger than the
    * sleep interval. This keeps the clocks from rolling over twice in
    * the sleep interval, which will cause errors in the drift
    * calculation.
    */
    ULONGLONG ullWrapTimeS =
        ULONGLONG (((double) timer1->ullWrapsAt) * timer1->doConversionToS);

    if (ullWrapTimeS < ULONGLONG (dwSampleStepS))
    {
        Error (_T("Step size is too large. It must be less than the time ")
            _T("it takes for both clocks to wrap around. In this case, ")
            _T("%s has a wrap time of %I64u s and and the step size is ")
            _T("set to %u s"), timer1->name, ullWrapTimeS, dwSampleStepS);
        goto cleanAndReturn;
    }

    ullWrapTimeS =
        ULONGLONG (((double) timer2->ullWrapsAt) * timer2->doConversionToS);

    if (ullWrapTimeS < ULONGLONG (dwSampleStepS))
    {
        Error (_T("Step size is too large. It must be less than the time ")
            _T("it takes for both clocks to wrap around. In this case, ")
            _T("%s has a wrap time of %I64u s and and the step size is ")
            _T("set to %u s"), timer2->name, ullWrapTimeS, dwSampleStepS);
        goto cleanAndReturn;
    }

    ullWrapTimeS =
        ULONGLONG (((double) timer3->ullWrapsAt) * timer3->doConversionToS);

    if (ullWrapTimeS < ULONGLONG (dwSampleStepS))
    {
        Error (_T("Step size is too large. It must be less than the time ")
            _T("it takes for both clocks to wrap around. In this case, ")
            _T("%s has a wrap time of %I64u s and and the step size is ")
            _T("set to %u s"), timer3->name, ullWrapTimeS, dwSampleStepS);
        goto cleanAndReturn;
    }

    DWORD dwSampleStepMS;

    if (DWordMult(dwSampleStepS, 1000,
        &dwSampleStepMS) != S_OK)
    {
        Error (_T("Overflow when converting sample step to ms."));
        goto cleanAndReturn;
    }

    /*
    * get the samples. This will calculate drift on the fly.
    *
    * Because of the added processing time the sample step should be
    * kept relatively long. On the order of a second should be fine.
    * Be careful if you reduce it less than this.
    */
    DWORD bGetSamplesSucceeded =
        (getSamplesAllThree (timer1, timer2, timer3,
        pullTimer1Samples, pullTimer2Samples, pullTimer3Samples,
        dwNumOfSamples, dwSampleStepMS,
        bound1To2, bound1To3, bound2To3, fSleep));


    /*Discontinue if an error occurred that can be fatal to the system*/
    if(g_bFatalErr == TRUE)
    {
        goto cleanAndReturn;
    }

    /************************** data analysis **************************/

    /*
    * the main loop does the main drift checks. At this point we do
    * more time consuming checks on the data. This keeps the main loop
    * clean, organized, and fast.
    */

    Info (_T(""));

    if (!printSamplesForTwoClocks (timer1, timer2,
        pullTimer1Samples, pullTimer2Samples,
        dwNumOfSamples, bound1To2))
    {
        bGetSamplesSucceeded = FALSE;
    }

    Info (_T(""));

    if (!printSamplesForTwoClocks (timer1, timer3,
        pullTimer1Samples, pullTimer3Samples,
        dwNumOfSamples, bound1To3))
    {
        bGetSamplesSucceeded = FALSE;
    }

    Info (_T(""));

    if (!printSamplesForTwoClocks (timer2, timer3,
        pullTimer2Samples, pullTimer3Samples,
        dwNumOfSamples, bound2To3))
    {
        bGetSamplesSucceeded = FALSE;
    }

    /* scoot these over so line up if printing an error */
    Info (_T(""));
    Info (_T(" Test Summary"));
    Info (_T(" ------------"));
    Info (_T(""));

    printStatus ((bGetSamplesSucceeded & CLOCK_ONE_TO_TWO),
        timer1->name, timer2->name);
    printStatus ((bGetSamplesSucceeded & CLOCK_ONE_TO_THREE),
        timer1->name, timer3->name);
    printStatus ((bGetSamplesSucceeded & CLOCK_TWO_TO_THREE),
        timer2->name, timer3->name);

    Info (_T(""));

    if (bGetSamplesSucceeded != CLOCK_TRUE)
    {
        goto cleanAndReturn;
    }

    /* All checks past this point assume that getting the samples succeeded. */
    returnVal = TRUE;

cleanAndReturn:

    if (pullTimer1Samples)
    {
        free (pullTimer1Samples);
    }

    if (pullTimer2Samples)
    {
        free (pullTimer2Samples);
    }

    if (pullTimer3Samples)
    {
        free (pullTimer3Samples);
    }

    return (returnVal);
}




/*
* Get a set of samples.
*
* All timers must be non-null and initialized. The arrays must be
* non-null and point to enough memory for the samples (dwNumSamples+1). 
* dwNumSamples is
* the number of samples and dwSleepTimeMS is the sleep time. The bounds
* values must be fully initialized (including ullLoosestRes).
*
* Return value is a bit field specifying which comparisons succeeded.
* If a bit is set the test succeeded. See the defines for CLOCK_*
* for the bit fields.
*/
DWORD
getSamplesAllThree (sTimer * timer1, sTimer * timer2, sTimer * timer3,
                    __out_ecount(1+dwNumSamples) ULONGLONG * pullTimer1Samples,
                    __out_ecount(1+dwNumSamples) ULONGLONG * pullTimer2Samples,
                    __out_ecount(1+dwNumSamples) ULONGLONG * pullTimer3Samples,
                    DWORD dwNumSamples,
                    DWORD dwSleepTimeMS,
                    sBounds * bound1To2, sBounds * bound1To3,
                    sBounds * bound2To3,
                    BOOL (*fSleep)(DWORD dwTimeMS))
{
    /* assume success until proven otherwise */
    /*
    * reversed from the standard method in this file so that we can track
    * errors in the drift calculation
    */
    DWORD returnVal = CLOCK_TRUE;

    if (!timer1 || !timer2 || !timer3 ||
        !pullTimer1Samples || !pullTimer2Samples || !pullTimer3Samples ||
        dwNumSamples == 0 || dwSleepTimeMS == 0 ||
        !bound1To2 || !bound1To3 || !bound2To3)
    {
        returnVal = CLOCK_FALSE;
        goto cleanAndReturn;
    }

    Info (_T(""));
    Info (_T("T1 refers to %s, T2 refers to %s, and T3 refers to %s."),
        timer1->name, timer2->name, timer3->name);
    Info (_T("The test will increase the thread priority for the duration ")
        _T("of reading the clocks. This is done in order to catch all ")
        _T("the clocks in the same instance."));
    Info (_T(""));

    ULONGLONG ullConvPrevS1 = 0;
    ULONGLONG ullConvPrevS2 = 0;
    ULONGLONG ullConvPrevS3 = 0;

    /*
    * Figure out the conversion factor between timer1 and timer2. We want
    * to convert the values to timer1's units. This should be the more
    * precise of the two clocks.
    */
    if (timer2->ullFreq == 0)
    {
        Error (_T("Frequency for %s is zero."), timer2->name);
        goto cleanAndReturn;
    }
    double doConvMultByT2 = (double) timer1->ullFreq / (double) timer2->ullFreq;

    if (timer3->ullFreq == 0)
    {
        Error (_T("Frequency for %s is zero."), timer3->name);
        goto cleanAndReturn;
    }
    double doConvMultByT3 = (double) timer1->ullFreq / (double) timer3->ullFreq;

    /* query the clocks to establish the beginning time */
    /* Increase the priority of the thread so we are not interrupted when reading the clocks*/
    DWORD dwOldPriority = CeGetThreadPriority(GetCurrentThread());

    if(dwOldPriority == THREAD_PRIORITY_ERROR_RETURN)
    {
        Error (_T("Could not get the current thread priority."));
        goto cleanAndReturn;
    }

    if(!CeSetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ZERO))
    {
        Error (_T("Could not set the current thread priority to %d"), THREAD_PRIORITY_ZERO);
        goto cleanAndReturn;
    }

    if (!(*timer1->getTick)(pullTimer1Samples[0]) ||
        !(*timer2->getTick)(pullTimer2Samples[0]) ||
        !(*timer3->getTick)(pullTimer3Samples[0]))
    {
        returnVal = CLOCK_FALSE;
    }

    /* Decrease the priority back to the original value */
    if(!CeSetThreadPriority(GetCurrentThread(),dwOldPriority))
    {
        Error (_T("Could not set the current thread priority back to its old value of %d"), dwOldPriority);
        g_bFatalErr = TRUE;
        goto cleanAndReturn;
    }

    if (returnVal == CLOCK_FALSE)
    {
        goto cleanAndReturn;
    }

    /* for completeness */
    Info (_T("Sample %4u T1: abs = %12I64u rel = %12I64u = %g s ")
        _T("T2: abs = %12I64u rel = %12I64u = %g s ")
        _T("T3: abs = %12I64u rel = %12I64u = %g s "),
        0,
        pullTimer1Samples[0],
        (ULONGLONG) 0, (double) 0.0,
        pullTimer2Samples[0],
        (ULONGLONG) 0, (double) 0.0,
        pullTimer3Samples[0],
        (ULONGLONG) 0, (double) 0.0);

    for (DWORD dwCurrSample = 1; dwCurrSample <= dwNumSamples; dwCurrSample++)
    {

        /*
        * sleep doesn't have to be perfectly accurate for this to work.
        * The clocks are being compared against each other, not the
        * sleep function.
        */
        //Sleep (dwSleepTimeMS);
        if (!(*fSleep)(dwSleepTimeMS))
        {
            Error (_T("Sleep function failed"));
            returnVal = CLOCK_FALSE;
            goto cleanAndReturn;
        }

        /* query the clocks to establish the beginning time */
        /* Increase the priority of the thread so we are not interrupted when reading the clocks */
        dwOldPriority = CeGetThreadPriority(GetCurrentThread());

        if(dwOldPriority == THREAD_PRIORITY_ERROR_RETURN)
        {
            Error (_T("Could not get the current thread priority."));
            goto cleanAndReturn;
        }

        if(!CeSetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ZERO))
        {
            Error (_T("Could not set the current thread priority to %d"), THREAD_PRIORITY_ZERO);
            goto cleanAndReturn;
        }

        if (!(*timer1->getTick)(pullTimer1Samples[dwCurrSample]) ||
            !(*timer2->getTick)(pullTimer2Samples[dwCurrSample]) ||
            !(*timer3->getTick)(pullTimer3Samples[dwCurrSample]))
        {
            returnVal = CLOCK_FALSE;
        }

        /* Decrease the priority back to the original value */
        if(!CeSetThreadPriority(GetCurrentThread(),dwOldPriority))
        {
            Error (_T("Could not set the current thread priority back to its old value of %d"), dwOldPriority);
            g_bFatalErr = TRUE;
            goto cleanAndReturn;
        }

        if (returnVal == CLOCK_FALSE)
        {
            goto cleanAndReturn;
        }

        ULONGLONG ullConvS1, ullConvS2, ullConvS3;

        /* convert to common units and absolute time */
        if (!convertOneSample(timer1, timer2, timer3,
            pullTimer1Samples[dwCurrSample - 1],
            pullTimer2Samples[dwCurrSample - 1],
            pullTimer3Samples[dwCurrSample - 1],
            pullTimer1Samples[dwCurrSample],
            pullTimer2Samples[dwCurrSample],
            pullTimer3Samples[dwCurrSample],
            ullConvPrevS1, ullConvPrevS2, ullConvPrevS3,
            &ullConvS1, &ullConvS2, &ullConvS3,
            doConvMultByT2, doConvMultByT3))
        {
            Error (_T("Couldn't convert sample %u."), dwCurrSample);
            returnVal = CLOCK_FALSE;
            goto cleanAndReturn;
        }

        Info (_T("Sample %4u T1: abs = %12I64u rel = %12I64u = %g s ")
            _T("T2: abs = %12I64u rel = %12I64u = %g s ")
            _T("T3: abs = %12I64u rel = %12I64u = %g s "),
            dwCurrSample,
            pullTimer1Samples[dwCurrSample],
            ullConvS1, ullConvS1 * timer1->doConversionToS,
            pullTimer2Samples[dwCurrSample],
            ullConvS2, ullConvS2 * timer2->doConversionToS,
            pullTimer3Samples[dwCurrSample],
            ullConvS3, ullConvS3 * timer3->doConversionToS);

        if (!isSampleWithinBounds (timer1, timer2, ullConvS1, ullConvS2,
            bound1To2->doFasterBound,
            bound1To2->doSlowerBound,
            bound1To2->ullLoosestRes))
        {
            /* to clear bit and against opposite */
            returnVal &= (~CLOCK_ONE_TO_TWO);
        }

        if (!isSampleWithinBounds (timer1, timer3, ullConvS1, ullConvS3,
            bound1To3->doFasterBound,
            bound1To3->doSlowerBound,
            bound1To3->ullLoosestRes))
        {
            returnVal &= (~CLOCK_ONE_TO_THREE);
        }

        if (!isSampleWithinBounds (timer2, timer3, ullConvS2, ullConvS3,
            bound2To3->doFasterBound,
            bound2To3->doSlowerBound,
            bound2To3->ullLoosestRes))
        {
            returnVal &= (~CLOCK_TWO_TO_THREE);
        }

        /* save these for the next loop around */
        ullConvPrevS1 = ullConvS1;
        ullConvPrevS2 = ullConvS2;
        ullConvPrevS3 = ullConvS3;

        if (g_fQuickFail && returnVal != CLOCK_TRUE)
        {
            break;
        }

    } /* for */

cleanAndReturn:

    return (returnVal);

}

/****** Common clock comparison functions ********************************/

/*
* convert a sample from the relative form to the absolute form.
*
* The relative values comes straight from the clocks. There are in
* the clocks units and have no zero relative to the start of the
* test. The absolute values are in the units of timer1 and have a
* zero based from the start of the test.
*
* This function is essentially a big equation. The one interesting
* bit of code involves how overflow is handled.
*
* This function assumes that the clocks go backwards only on
* overflow. In this case the function calculates how much time
* really did elapse and then uses this for the time between this
* sample and the previous sample. Therefore, the total time for this
* sample is the prev absolute sample plus this difference.
*
* For this reason this function takes many paraemeters. Essentially,
* it need to know the previous and current values for both relative
* values and the absolute values. It takes the data for both clocks
* and calculates the current absolute times.
*
* timer1 and timer2 must be initialized timer structs. All pointers
* must not be null. The calculated values are returned in ullS1Abs
* and ullS2Abs. These are in the units of the first clock.
*
* doConvMultByS2 does the conversion from the units of timer1 to
* timer2. The equation governing the conversion is:
*
* s1 = doConvMultByS2 * s2
*/
BOOL
convertOneSample (sTimer * timer1, sTimer * timer2,
                  ULONGLONG ullS1Prev, ULONGLONG ullS2Prev,
                  ULONGLONG ullS1, ULONGLONG ullS2,
                  ULONGLONG ullPrevS1Abs, ULONGLONG ullPrevS2Abs,
                  ULONGLONG * ullS1Abs, ULONGLONG * ullS2Abs,
                  double doConvMultByS2)
{
    if (!timer1 || !timer2 || !ullS1Abs || !ullS2Abs)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    ULONGLONG ullDiff;

    /* calculate the difference, taking into account rollover is need be */
    if (!AminusB (ullDiff, ullS1, ullS1Prev, timer1->ullWrapsAt))
    {
        Error (_T("AminusB failed in convertOneSample (first call)."));
        goto cleanAndReturn;
    }

    /*
    * add this difference onto the previous value to get the absolute value
    * for this data point
    */
    *ullS1Abs = ullPrevS1Abs + ullDiff;

    /* handle the samples for the second timer */
    if (!AminusB (ullDiff, ullS2, ullS2Prev, timer2->ullWrapsAt))
    {
        Error (_T("AminusB failed in convertOneSample (second call)."));
        goto cleanAndReturn;
    }

    *ullS2Abs = ullPrevS2Abs + ullDiff;

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}

BOOL
convertOneSample (sTimer * timer1, sTimer * timer2, sTimer * timer3,
                  ULONGLONG ullS1Prev, ULONGLONG ullS2Prev, ULONGLONG ullS3Prev,
                  ULONGLONG ullS1, ULONGLONG ullS2, ULONGLONG ullS3,
                  ULONGLONG ullPrevS1Abs, ULONGLONG ullPrevS2Abs,
                  ULONGLONG ullPrevS3Abs,
                  ULONGLONG * ullS1Abs, ULONGLONG * ullS2Abs,
                  ULONGLONG * ullS3Abs,
                  double doConvMultByS2, double doConvMultByS3)
{
    if (!timer1 || !timer2 || !timer3 || !ullS1Abs || !ullS2Abs || !ullS3Abs)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    ULONGLONG ullDiff;

    /* calculate the difference, taking into account rollover is need be */
    if (!AminusB (ullDiff, ullS1, ullS1Prev, timer1->ullWrapsAt))
    {
        Error (_T("AminusB failed in convertOneSample (3 args first call)"));
        goto cleanAndReturn;
    }

    /*
    * add this difference onto the previous value to get the absolute value
    * for this data point
    */
    *ullS1Abs = ullPrevS1Abs + ullDiff;

    /* handle the samples for the second timer */
    if (!AminusB (ullDiff, ullS2, ullS2Prev, timer2->ullWrapsAt))
    {
        Error (_T("AminusB failed in convertOneSample (3 args second call)"));
        goto cleanAndReturn;
    }

    *ullS2Abs = ullPrevS2Abs + ullDiff;

    /* handle the samples for the second timer */
    if (!AminusB (ullDiff, ullS3, ullS3Prev, timer3->ullWrapsAt))
    {
        Error (_T("AminusB failed in convertOneSample (3 args third call)"));
        goto cleanAndReturn;
    }

    *ullS3Abs = ullPrevS3Abs + ullDiff;

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}

/*
* Print out the samples for two clocks. This pretty prints the data
* in table format after a test run.
*
* All variables can't be null.
*
* Return true on success and false otherwise.
*/
BOOL
printSamplesForTwoClocks (sTimer * timer1, sTimer * timer2,
                          __in_ecount(dwNumSamples) const ULONGLONG * pullTimer1Samples,
                          __in_ecount(dwNumSamples) const ULONGLONG * pullTimer2Samples,
                          DWORD dwNumSamples,
                          sBounds * pBound1To2)
{
    if (!timer1 || !timer2 || !pullTimer1Samples || !pullTimer2Samples || !pBound1To2)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    Info (_T("Comparing %s to %s"), timer1->name, timer2->name);
    Info (_T(""));

    /*
    * Figure out the conversion factor between timer1 and timer2. We want
    * to convert the values to timer1's units. This should be the more
    * precise of the two clocks.
    */
    if (timer2->ullFreq == 0)
    {
        Error (_T("Frequency for %s is zero."), timer2->name);
        goto cleanAndReturn;
    }
    double doConvMultByT2 = (double) timer1->ullFreq / (double) timer2->ullFreq;

    ULONGLONG ullConvPrevS1 = 0, ullConvPrevS2 = 0;
    ULONGLONG ullConvS1, ullConvS2;

    for (DWORD dwCurrSample = 1; dwCurrSample < dwNumSamples; dwCurrSample++)
    {
        /* convert to common units and absolute time */
        if (!convertOneSample(timer1, timer2,
            pullTimer1Samples[dwCurrSample - 1],
            pullTimer2Samples[dwCurrSample - 1],
            pullTimer1Samples[dwCurrSample],
            pullTimer2Samples[dwCurrSample],
            ullConvPrevS1, ullConvPrevS2,
            &ullConvS1, &ullConvS2,
            doConvMultByT2))
        {
            Error (_T("Couldn't convert sample %u."), dwCurrSample);
            goto cleanAndReturn;
        }

        double doS1s, doS2s;

        /* both are in the units of the first sample */
        doS1s = ((double) ullConvS1) * timer1->doConversionToS;
        doS2s = ((double) ullConvS2) * timer2->doConversionToS;

        double doRatio;

        if (doS1s == 0.0)
        {
            doRatio = 0.0;
        }
        else
        {
            doRatio = (doS1s - doS2s) / doS1s;
        }

        /*
        * don't print debugging messages. These are printed above,
        * and we just need a yes or no answer so that we can flag
        * the bad values.
        */
        BOOL bSampleFailed =
            !isSampleWithinBounds (timer1, timer2, ullConvS1, ullConvS2,
            pBound1To2->doFasterBound,
            pBound1To2->doSlowerBound,
            pBound1To2->ullLoosestRes, FALSE);

        if (bSampleFailed)
        {
            Info (_T("Sample %4u T1 = %16.8f s T2 raw = %16.8f s ")
                _T("diff = %12.8f ratio = %11.8f ** out of range **"),
                dwCurrSample, doS1s, doS2s, doS1s - doS2s, doRatio);
        }
        else
        {
            Info (_T("Sample %4u T1 = %16.8f s T2 raw = %16.8f s ")
                _T("diff = %12.8f ratio = %11.8f"),
                dwCurrSample, doS1s, doS2s, doS1s - doS2s, doRatio);
        }

        /* save these for the next loop around */
        ullConvPrevS1 = ullConvS1;
        ullConvPrevS2 = ullConvS2;


    }

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}

/*
* prints out the status of the tests
*
* status is either 0 (false) or non-zero (true). szT1 and szT2 are the names
* of the timers. These can't be null.
*/
void
printStatus (DWORD status, const TCHAR * szT1, const TCHAR * szT2)
{
    if (!szT1 || !szT2)
    {
        return;
    }

    if (status)
    {
        Info (_T(" %s to %s succeeded"), szT1, szT2);
    }
    else
    {
        /* flag isn't set, so this check failed */
        Error (_T("%s to %s failed!"), szT1, szT2);
    }

    return;
}

/******* Are the timers in bounds? *****************************************/

/*
* Check the bounds structure for correctness, and print information
* about it.
*
* This is a helper for the three bound case.
*
* b can't be null. The double converts bounds to seconds.
*/
BOOL
boundsCheck (sBounds * b, double doConvToS)
{
    if (!b)
    {
        return (FALSE);
    }

    /* check bounds */
    if (b->doFasterBound < b->doSlowerBound)
    {
        Error (_T("The faster bound must be numerically greater than the slower "));
        Error (_T("bound. Comparing %s. faster bound = %g slower bound = %g"),
            b->szName, b->doFasterBound, b->doSlowerBound);
        return (FALSE);
    }

    /* for use with realistic time */
    double doRealisticTime;
    TCHAR * szRealisticUnits;

    realisticTime (double (b->ullLoosestRes) * doConvToS,
        doRealisticTime, &szRealisticUnits);

    Info (_T("Bounds ratios: %s Faster = %g Slower = %g ")
        _T("Loosest res = %I64u ticks = %g %s"),
        b->szName,
        b->doFasterBound, b->doSlowerBound,
        b->ullLoosestRes, doRealisticTime, szRealisticUnits);

    return (TRUE);
}

/*
* Calculate whether the samples are within the given bounds.
*
* timer1 and timer2 are initialized timers. ullSample1 and
* ullSample2 are the samples corresponding to the above timers.
* ullLoosestResBound is the resolution of the loosest clock being
* compared in the units of timer1.
*
* returns true if the values are within bounds and false for other errors.
*/
BOOL
isSampleWithinBounds(sTimer * timer1, sTimer * timer2,
                     ULONGLONG ullSample1, ULONGLONG ullSample2UnitsT2,
                     double doFasterBound, double doSlowerBound,
                     ULONGLONG ullLoosestResBound, BOOL bPrintDebuggingMessages)
{
    /* faster bound always has to be numerically greater than the slower bound */
    if (!timer1 || !timer2 || (doFasterBound < doSlowerBound))
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /* convert to units of timer1 */
    double doConvMultByT2 = (double) timer1->ullFreq / (double) timer2->ullFreq;
    ULONGLONG ullSample2 =
        (ULONGLONG) (((double) ullSample2UnitsT2) * doConvMultByT2);

    /*
    * scale the ratio against the first sample. This will give us a
    * bound relative to the sample size
    */
    ULONGLONG ullFasterBoundDiff =
        (ULONGLONG) (DOUBLE_ABS (doFasterBound) * (double) ullSample1);
    ULONGLONG ullSlowerBoundDiff =
        (ULONGLONG) (DOUBLE_ABS (doSlowerBound) * (double) ullSample1);

    ULONGLONG ullFasterBound;
    ULONGLONG ullSlowerBound;

    /*
    * Convert the scaled difference into the actual bound that we are
    * checking against. We can't have a negative number for the clock
    * time, so deal with these issues.
    */
    if (doFasterBound < 0.0)
    {
        if (ullSample1 < ullFasterBoundDiff)
        {
            ullFasterBound = 0;
        }
        else
        {
            ullFasterBound = ullSample1 - ullFasterBoundDiff;
        }
    }
    else
    {
        ullFasterBound = ullSample1 + ullFasterBoundDiff;

        /* handle overflow */
        if (ullFasterBound < MAX(ullSample1, ullFasterBoundDiff))
        {
            ullFasterBound = ULONGLONG_MAX;
        }
    }

    if (doSlowerBound < 0.0)
    {
        if (ullSample1 < ullSlowerBoundDiff)
        {
            ullSlowerBound = 0;
        }
        else
        {
            ullSlowerBound = ullSample1 - ullSlowerBoundDiff;
        }
    }
    else
    {
        ullSlowerBound = ullSample1 + ullSlowerBoundDiff;

        /* handle overflow */
        if (ullSlowerBound < MAX(ullSample1, ullSlowerBoundDiff))
        {
            ullSlowerBound = ULONGLONG_MAX;
        }
    }

    /*
    Info (_T("Before rounding slower = %I64u faster = %I64u"),
    ullSlowerBound, ullFasterBound);
    */

    /*
    * account for the resolution of the clock. We have to adjust our
    * window so that it falls on boundaries that conform with the
    * resolution of this clock. If we don't do this we could catch
    * ticks are just outside of the window because the sample was taken
    * in the middle of the tick. This widens the window to catch these
    * cases.
    */
    if (!specialRoundDown (ullSlowerBound, ullLoosestResBound, &ullSlowerBound))
    {
        if (bPrintDebuggingMessages)
        {
            Info (_T("Overflow/underflow when rounding slower bound."));
            Info (_T("Setting slower bound to zero."));
        }

        ullSlowerBound = 0;
    }

    if (!specialRoundUp (ullFasterBound, ullLoosestResBound, &ullFasterBound))
    {
        Error (_T("Overflow/underflow when rounding faster bound."));
        goto cleanAndReturn;
    }

    /* sanity check */
    if (ullFasterBound < ullSlowerBound)
    {
        IntErr (_T("ullFasterBound < ullSlowerBound %I64u < %I64u"),
            ullFasterBound, ullSlowerBound);
        goto cleanAndReturn;
    }

    if ((ullFasterBound < ullSample2) ||
        (ullSlowerBound > ullSample2))
    {
        /* sample 2 isn't in the expected window */
        if (bPrintDebuggingMessages)
        {
            Error (_T("Too much drift between %s and %s."),
                timer1->name, timer2->name);
            Error (_T(" %s is at %I64u ticks."),
                timer1->name, ullSample1);
            Error (_T(" %s should be between %I64u and %I64u ")
                _T("ticks inclusive. Instead, it is at %I64u. These numbers ")
                _T("are using %s's units."), timer2->name,
                ullSlowerBound, ullFasterBound, ullSample2, timer1->name);
        }
        goto cleanAndReturn;
    }

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}


/*
* rounds a value down to a multiple of the ullEdge value.
*
* ullVal is the value to be rounded. ullEdge defines the resolution
* to round to. The answer is put in pullAns (which can't be null).
*
* A ullVal that is a multiple of an edge is not modified by this
* function. In this case, ullVal == *pullAns.
*
* returns true on success and false on failure.
*/
BOOL roundDown (ULONGLONG ullVal, ULONGLONG ullEdge, ULONGLONG * pullAns)
{
    if ((ullEdge == 0) || (!pullAns))
    {
        return (FALSE);
    }

    *pullAns = ullVal - ullVal % ullEdge;

    return (TRUE);
}

/*
* rounds a value up to a multiple of the ullEdge value.
*
* ullVal is the value to be rounded. ullEdge defines the resolution
* to round to. The answer is put in pullAns (which can't be null).
*
* A ullVal that is a multiple of an edge is moved to the next edge
* value above that. This is why this is a special rounding function.
*
* returns true on success and false on failure.
*/
BOOL specialRoundDown (ULONGLONG ullVal, ULONGLONG ullEdge, ULONGLONG * pullAns)
{
    if ((ullEdge == 0) || (!pullAns))
    {
        return (FALSE);
    }

    if (!roundDown (ullVal, ullEdge, pullAns))
    {
        return (FALSE);
    }

    if (ULongLongSub (*pullAns, ullEdge, pullAns) != S_OK)
    {
        /* overflow */
        return (FALSE);
    }
    else
    {
        return (TRUE);
    }
}

/*
* rounds a value up to a multiple of the ullEdge value.
*
* ullVal is the value to be rounded. ullEdge defines the resolution
* to round to. The answer is put in pullAns (which can't be null).
*
* The value is rounded and then one ullEdge is added to it. This
* forms the upper bound in the test. If the value is on an edge to
* begin with, only one is added to it.
*
* returns true on success and false on failure.
*/
BOOL specialRoundUp (ULONGLONG ullVal, ULONGLONG ullEdge, ULONGLONG * pullAns)
{
    if ((ullEdge == 0) || (!pullAns))
    {
        return (FALSE);
    }

    if (!roundDown (ullVal, ullEdge, pullAns))
    {
        return (FALSE);
    }

    if (ULongLongAdd (*pullAns, ullEdge, pullAns) != S_OK)
    {
        /* overflow */
        return (FALSE);
    }

    if (ULongLongAdd (*pullAns, ullEdge, pullAns) != S_OK)
    {
        /* overflow */
        return (FALSE);
    }
    else
    {
        return (TRUE);
    }
}


/***************************************************************************
*
* Backwards Checks
*
***************************************************************************/

/*
* goal is confirm that the timer doesn't go backwards. We watch the
* timer in a busy loop, checking it on each iteration.
*
* We say it goes backwards if the rollover is ever more that twice
* the resolution.
*
* timer is the timer to work with, which can't be null.
* ullIterations is the number of iterations to do. ullRes is the
* resolution of the clock. ullPrintAtIt denotes how often to print
* the time during the test. This is useful for debugging. The time
* is printed every ullPrintAtIt iterations. Zero denotes that no
* time messages should be printed.
*
* If the user would like the function to sleep between timer reads,
* he passes in a function pointer to a sleep function (taking a
* dword), and then passes in the sleep time as the next parameter.
* If the user would not like the function to sleep, meaning that it
* just stays in a tight loop reading values, the user should pass in
* NULL as the function pointer. the dwSleepTime parameter is ignored
* in the latter case.
*
* On error, messages are printed. The test continues on error until
* the allotted number of iterations has been completed.
*/
BOOL
doesClockGoBackwards (sTimer * timer,
                      ULONGLONG ullIterations,
                      ULONGLONG ullRes, ULONGLONG ullPrintAtIt,
                      BOOL (*fSleep)(DWORD dwTimeMS),
                      DWORD dwSleepTime)
{
    if (!timer || (ullRes == 0))
    {
        return FALSE;
    }

    BOOL returnVal = TRUE;

    /* use this print special error message in this case */
    BOOL bClockJumpedForward = FALSE;

    /*
    * set the allowable difference to be twice the resolution. This is completely
    * arbitrary.
    */
    ULONGLONG ullAllowableDifference = 2 * ullRes;

    /* set the allowable foward jump to a day (arbitrary) */
    ULONGLONG ullAllowableForwardJump = (24ULL * 60ULL * 60ULL) * timer->ullFreq;

    ULONGLONG ullPrevTickCount;
    /* initialize to current time */
    if ((*timer->getTick)(ullPrevTickCount) == FALSE)
    {
        Error (_T("Failed to get tick count in doesClockGoBackwards for %s."),
            timer->name);

        returnVal = FALSE;
        goto cleanAndReturn;
    }

    ULONGLONG ullCurrTickCount;

    for (ULONGLONG ullCurrent = 0; ullCurrent < ullIterations; ullCurrent++)
    {
        if ((*timer->getTick)(ullCurrTickCount) == FALSE)
        {
            Error (_T("Failed to get tick count in doesClockGoBackwards for %s.")
                _T("Iteration is %I64u."),
                timer->name, ullCurrent);

            returnVal = FALSE;
            goto cleanAndReturn;
        }

        if (ullCurrTickCount < ullPrevTickCount)
        {
            /*
            * was it overflow? We need the difference, as if overflow
            * occurred.
            */

            ULONGLONG ullDifference;
            if (AminusB (ullDifference, ullCurrTickCount, ullPrevTickCount,
                timer->ullWrapsAt) == FALSE)
            {
                Error (_T("Failed on AminusB: %I64u - %I64u."),
                    ullCurrTickCount, ullPrevTickCount);
                returnVal = FALSE;
                goto cleanAndReturn;
            }

            /*
            * arbitrary: if larger than two times the resolution we have problems
            * less than this is probably rollover (or a really nasty bug).
            */
            if (ullDifference > ullAllowableDifference)
            {
                /* Curr_Prev is curr minus prev, other is vice versa */
                double doTimeCurr_Prev, doTimePrev_Curr;
                _TCHAR * pszUnitsCurr_Prev, * pszUnitsPrev_Curr;
                realisticTime (((double) (ullDifference)) * timer->doConversionToS,
                    doTimeCurr_Prev, &pszUnitsCurr_Prev);
                /*
                * prev is > curr, so result will be positive here (but
                * could be huge)
                */
                realisticTime (((double) (ullPrevTickCount - ullCurrTickCount)) *
                    timer->doConversionToS,
                    doTimePrev_Curr, &pszUnitsPrev_Curr);

                Error (_T("%s went backward. Previous value ")
                    _T("is %I64u and current value is %I64u. ")
                    _T("curr - prev (unsigned) is %I64u, which is %g %s. ")
                    _T("prev - curr is %I64u, which is %g %s."),
                    timer->name,
                    ullPrevTickCount, ullCurrTickCount,
                    ullDifference, doTimeCurr_Prev, pszUnitsCurr_Prev,
                    ullPrevTickCount - ullCurrTickCount,
                    doTimePrev_Curr, pszUnitsPrev_Curr);

                returnVal = FALSE;
            }
        }
        else if (ullCurrTickCount - ullPrevTickCount > ullAllowableForwardJump)
        {
            /*
            * adding this check to adjust for the monotonic clock.
            *
            * If the monotonic clock is enabled than any non-rollover
            * backward behavior on the RTC will appear as huge jumps in
            * time returned by GetSystemTime. The kernel, when running
            * with the monotonic clock, assumes that the RTC going
            * backwards always means rollover. If the clock goes back
            * due to a bug, the kernel will assume that this is due to
            * rollover and will (incorrectly) adjust accordingly.
            *
            * This adjustment will look like a jump of a huge number of
            * years (ie. 50 +). Without this test update these tests
            * will never fail on a platform running the monotonic
            * clock, since the clock will always be moving forwards.
            * The drift tests should catch this issue, but this test
            * provides more coverage for other scenarios and therefore
            * needs to account for it as well.
            *
            * we use the allowable difference since it has already been
            * calculated and is used above in the rollover check. The
            * rollover check has worked fine in the past, so I am
            * relatively sure that using this threshold won't cause
            * issues.
            *
            * look for dwYearsRTCRollover in the OEMGlobal struct and
            * the docs for more info.
            */
            bClockJumpedForward = TRUE;

            /* Curr_Prev is curr minus prev */
            double doTimeCurr_Prev, doAllowableForwardJump;
            _TCHAR * pszUnitsCurr_Prev, * pszUnitsAllowDiff;
            realisticTime (((double) (ullCurrTickCount - ullPrevTickCount)) * timer->doConversionToS,
                doTimeCurr_Prev, &pszUnitsCurr_Prev);

            realisticTime (((double) (ullAllowableForwardJump)) * timer->doConversionToS,
                doAllowableForwardJump, &pszUnitsAllowDiff);

            Error (_T("%s jumped forward too far. Previous value ")
                _T("is %I64u and current value is %I64u. Allowable jump is ")
                _T("%I64u, which is %g %s. ")
                _T("curr - prev is %I64u, which is %g %s."),
                timer->name,
                ullPrevTickCount, ullCurrTickCount,
                ullAllowableForwardJump, doAllowableForwardJump, pszUnitsAllowDiff,
                ullCurrTickCount - ullPrevTickCount,
                doTimeCurr_Prev, pszUnitsCurr_Prev);

            returnVal = FALSE;
        }

        /*
        * print out a message every so often
        * ullPrintAtIt == 0 denotes no time printing
        */
        if (ullPrintAtIt && (ullCurrent % ullPrintAtIt == 0))
        {
            /* make nice times when printing */
            double doRealisticTime;
            _TCHAR * pszRealisticUnits;
            realisticTime (((double) (ullCurrTickCount)) * timer->doConversionToS,
                doRealisticTime, &pszRealisticUnits);
            Info (_T(" Current time is %g %s."),
                doRealisticTime, pszRealisticUnits);
        }

        ullPrevTickCount = ullCurrTickCount;

        /* sleep for a given time, if the user desires */
        if (fSleep && !(*fSleep)(dwSleepTime))
        {
            Error (_T("Sleep function failed on sleeptime = %u"),
                dwSleepTime);
            returnVal = FALSE;
        }

    }

cleanAndReturn:

    if (bClockJumpedForward)
    {
        Error (_T(""));
        Error (_T("Clocked jumped forward during this test (see errors above). "));
        Error (_T(""));
        Error (_T("If dwYearsRTCRollover (OEMGlobal.h) is non-zero then most likely"));
        Error (_T("OEMGetRealTime jumped backwards. The kernel compensated because"));
        Error (_T("the monotonic clock code uses backwards to detect rollover. When"));
        Error (_T("debugging, focus on the OAL."));
        Error (_T("Also note that turning off interrupts for greater than the allowable"));
        Error (_T("jump time can also cause this test to fail. When broken into the "));
        Error (_T("debugger interrupts are off. Consider this fact if this test fails"));
        Error (_T("after letting the device sit for a long period of time (hours or days)"));
        Error (_T("while continuously broken into the debugger."));
    }

    return (returnVal);
}

/***************************************************************************
*
* Wall Clock Check
*
***************************************************************************/


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
    BOOL returnVal = FALSE;


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
        Error (_T("Test length overflowed. Either test is too long or timer freq"));
        Error (_T("is too high."));
        goto cleanAndReturn;
    }

    /* info message */
    Info (_T("This test allows you to compare clock %s with an external wall "),
        timer->name);
    Info (_T("clock. This allows you to verify that the clocks in the "));
    Info (_T("and OAL are not drifting drastically. It won't pick up slight"));
    Info (_T("variations (it is limited by how fast you can click a stop watch"));
    Info (_T("button), but will give a good idea of if your timers are grossly"));
    Info (_T("off. If the test can write to the desktop release directory, it will"));
    Info (_T("create a dummy directoy and write to it and obtain desktop time at "));
    Info (_T("the start and end of the test and compare against it."));
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

    /* get the beginning time for the timer being tested and the desktop clock*/
    ULONGLONG ullBeginTime = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    BOOL fFRDExists = relFRD();
    DWORD dwOldPriority = CeGetThreadPriority(GetCurrentThread());
    if(dwOldPriority == THREAD_PRIORITY_ERROR_RETURN)
    {
        Error (_T("Could not get the current thread priority."));
        goto cleanAndReturn;
    }

    /* Increase the priority of the thread so we are not interrupted between getting the
    desktop time and the timer in test*/
    /* We only do this if FRD exists, if not the user will time the test with a stop watch*/

    if(fFRDExists == TRUE)
    {
        if(!CeSetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_KITL))
        {
            Error (_T("Could not set the current thread priority to %d"), THREAD_PRIORITY_BELOW_KITL);
            goto cleanAndReturn;
        }

        //Get desktop time
        if( DeskTopClock(START_CLOCK, &hFile, 0, NULL) != FALSE)
        {
            //If successful get time from the timer in test
            if ((*timer->getTick)(ullBeginTime) == FALSE)
            {
                Error (_T("Failed to get beginning time for %s."),
                    timer->name);
                goto cleanAndReturn;
            }

        }
        else //set fFRDExists = False, so that we don't use desktop
        {
            Error (_T("Could not get desktop time. The test will need "));
            Error (_T("to be run manually using a stop watch. "));
            fFRDExists = FALSE;
        }

        // Set thread priority back to the original value
        if(!CeSetThreadPriority(GetCurrentThread(),dwOldPriority))
        {
            Error (_T("Could not set the current thread priority back to its old value of %d"), dwOldPriority);
            goto cleanAndReturn;
        }
    }

    if (fFRDExists == FALSE) //Run test manually using stop-watch
    {
        //Start stop watch
        PrintTimingInfo(START, (double)(ullSleepIteration * timer->doConversionToS), dwTestLengthS, timer);

        // Get time from the timer in test
        if ((*timer->getTick)(ullBeginTime) == FALSE)
        {
            Error (_T("Failed to get beginning time for %s."),
                timer->name);
            goto cleanAndReturn;
        }
    }

    ULONGLONG ullCurrTime = ullBeginTime;

    /* calculate the time that we want to stop */
    ULONGLONG ullWantedStopTime = ullBeginTime + ullTestLength;

    /*
    * overflow check
    * check the wraps at value for the timer and also check the timer value
    * itself. If the wraps at value is the ULONGLONG_MAX, the first check will
    * always be true but the second check will fail. (We actually got a hardware
    * overflow in this case.)
    */
    if ((ullWantedStopTime > timer->ullWrapsAt) ||
        (ullWantedStopTime <= MAX(ullBeginTime, ullTestLength)))
    {
        /*
        * we will get overflow during the test
        * Normally we can deal with this, but we can't guarantee that the
        * sleep times will be what we want them to be. The only way
        * to detect a bad sleep is to watch for current times that are
        * greater than the end time. This is normally an overflow check, though.
        *
        * There are ways to get around this, but for this test it is easiest to
        * have the user rerun it in this case.
        */
        Error (_T("The test would have overflowed a timing value for timer %s"),
            timer->name);
        Error (_T("Please rerun it when the timers are not about to overflow."));
        Error (_T("Please see the documentation for more info. This doesn't"));
        Error (_T("mean that the unit under test has a flaw."));
        goto cleanAndReturn;
    }

    ULONGLONG ullTimeLeft = ullTestLength;

    /*
    * while not within 10 minutes print a status report every sleep
    * iteration. This means that we will get only a maximum amount
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
            goto cleanAndReturn;
        }

        /* check for overflow in the next calculation */
        if (ullCurrTime >= ullWantedStopTime)
        {
            /*
            * something went wrong. The sleep statement must not have
            * been correct or the timers are doing funky things.
            */
            Error (_T("The current time overshot the stop time for the test."));
            Error (_T("This is quite odd, because it shouldn't have happened."));
            Error (_T("Something is most likely wrong with either sleep or the ")
                _T("timers."));
            goto cleanAndReturn;
        }

        /* recalculate time left */
        ullTimeLeft = ullWantedStopTime - ullCurrTime;

        if (fFRDExists == FALSE)
        {
            PrintTimingInfo(INTERVAL, (double) (ullTimeLeft * timer->doConversionToS), dwTestLengthS, timer);
        }
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
            goto cleanAndReturn;
        }

        /* check for overflow in the next calculation */
        if (ullCurrTime >= ullWantedStopTime)
        {
            /*
            * something went wrong. The sleep statement must not have
            * been correct or the timers are doing funky things.
            */
            Error (_T("The current time overshot the stop time for the test."));
            Error (_T("This is quite odd, because it shouldn't have happened."));
            Error (_T("Something is most likely wrong with either sleep or the ")
                _T("timers."));
            goto cleanAndReturn;
        }

        /* recalculate time left */
        ullTimeLeft = ullWantedStopTime - ullCurrTime;
        if (fFRDExists == FALSE)
        {
            PrintTimingInfo(INTERVAL, (double) (ullTimeLeft * timer->doConversionToS), dwTestLengthS, timer);
        }

    }

    /*
    * between 30 seconds and the test end print a message every second
    */
    for(;;)
    {
        /* sleep for normal sleep iteration */
        abstractSleep (timer, ullConstOneSecond);

        /* get time */
        if ((*timer->getTick)(ullCurrTime) == FALSE)
        {
            Error (_T("Failed to get tick count for %s."),
                timer->name);
            goto cleanAndReturn;
        }

        if (ullCurrTime >= ullWantedStopTime)
        {
            /* we are done */
            break;
        }

        /* recalculate time left. Can't overflow because of previous check */
        ullTimeLeft = ullWantedStopTime - ullCurrTime;

        if (fFRDExists == FALSE)
        {
            PrintTimingInfo(INTERVAL, (double) (ullTimeLeft * timer->doConversionToS), dwTestLengthS, timer);
        }
    }

    ULONGLONG ullTrueEndTime;

    /* we have slept for the given time. Now find the final time. */
    /* Increase the priority of the thread so we are not interrupted between getting the
    desktop time and the timer in test*/
    /* Do this only if FRD exists*/
    if(fFRDExists == TRUE)
    {
        dwOldPriority = CeGetThreadPriority(GetCurrentThread());

        if(dwOldPriority == THREAD_PRIORITY_ERROR_RETURN)
        {
            Error (_T("Could not get the current thread priority."));
            goto cleanAndReturn;
        }

        if(!CeSetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_KITL))
        {
            Error (_T("Could not set the current thread priority to %d"), THREAD_PRIORITY_BELOW_KITL);
            goto cleanAndReturn;
        }

    }

    // Get time from the timer in test
    if ((*timer->getTick)(ullTrueEndTime) == FALSE)
    {
        Error (_T("Failed to get tick count for %s."),
            timer->name);
        goto cleanAndReturn;
    }

    if (ullTrueEndTime < ullBeginTime)
    {
        /* we overflowed (which shouldn't happen) or something else
        * happened
        */
        Error (_T("The test end time is less than the test start time."));
        Error (_T("This would normally suggest overflow, except that this"));
        Error (_T("is designed not to overflow. Either Sleep is incorrect,"));
        Error (_T("the timers are incorrect, or the test is buggy."));
        goto cleanAndReturn;
    }

    /* Get desktop time and decrease the priority back to the original value */
    if (fFRDExists == TRUE)
    {
        if (DeskTopClock(STOP_CLOCK, &hFile, (ullTrueEndTime - ullBeginTime), timer) == FALSE)
        {
            Error (_T("Comparison with desktop time failed."));
        }
        else
        {
            /* Test passed*/
            returnVal = TRUE;
        }

        if(!CeSetThreadPriority(GetCurrentThread(),dwOldPriority))
        {
            Error (_T("Could not set the current thread priority back to its old value of %d"), dwOldPriority);
            goto cleanAndReturn;
        }
    }
    else
    {
        /* Ask user to stop the stop watch */
        PrintTimingInfo(STOP, (double) (ullTrueEndTime - ullBeginTime) * timer->doConversionToS, dwTestLengthS, timer);

        /*Test shows as pass in case of manual verification */
        returnVal = TRUE;
    }

cleanAndReturn:

    return (returnVal);
}


/***************************************************************************
*
* Sleep Routines
*
***************************************************************************/

/*
* sleep for a given number of ms. This is a busy sleep that doesn't kick
* us from the proc automatically.
*
* return true on success and false otherwise.
*/
BOOL
abstractSleepMS (sTimer * timer, ULONGLONG ullSleepTimeMS)
{
    if (!timer || (timer->doConversionToS == 0))
    {
        return (FALSE);
    }

    /* convert sleep time into units of the given timer */
    return (abstractSleep (timer,
        (ULONGLONG) ((((double) ullSleepTimeMS) / 1000.0) /
        timer->doConversionToS)));
}

/*
* An abstraction of the system sleep function that supports the
* different units associated with each timer.
*
* timer is a timer structure and ullSleepTime is the time to sleep in
* the units given in the timer structure.
*/
BOOL
abstractSleep (sTimer * timer, ULONGLONG ullSleepTime)
{
    BOOL returnVal = FALSE;

    if (timer == NULL)
    {
        IntErr (_T("abstractSleep: timer == NULL"));
        return (FALSE);
    }

    ULONGLONG ullStartTime, ullTimeUntilWrap, ullStopTime, ullCurrTime;

    if ((*timer->getTick)(ullStartTime) == FALSE)
    {
        Error (_T("Couldn't get tick for %s"), timer->name);
        goto cleanAndReturn;
    }

    if (ullSleepTime > timer->ullWrapsAt)
    {
        Error (_T("Can't sleep for more than the time it takes for the timer ")
            _T("to roll over."));
        goto cleanAndReturn;
    }

    /* assume that curr time <= ullWrapsAt */
    ullTimeUntilWrap = timer->ullWrapsAt - ullStartTime;

    if (ullTimeUntilWrap < ullSleepTime)
    {
        /* timer would overflow */
        /* -1 accounts for the transition between wrapsAt and 0 */
        ullStopTime = ullSleepTime - ullTimeUntilWrap - 1;

        /* let clock roll over */
        while ((*timer->getTick)(ullCurrTime) && (ullCurrTime >= ullStartTime));

        /* wait for the stop time to come after roll over */
        while ((*timer->getTick)(ullCurrTime) && (ullCurrTime < ullStopTime));

    }
    else
    {
        ullStopTime = ullSleepTime + ullStartTime;
        while ((*timer->getTick)(ullCurrTime) &&
            ((ullCurrTime >= ullStartTime) && (ullCurrTime < ullStopTime)));
    }

    /*
    * if we bail out on get tick error (very very unlikely) then return
    * false. Else we bailed out when time expired so return true.
    */
    returnVal = !(ullCurrTime < ullStopTime);

cleanAndReturn:
    return (returnVal);

}









#ifdef UNDEF


#define MAX_FOUND 20

typedef struct
{
    ULONGLONG startTime;
    ULONGLONG endTime;
    DWORD it;
    ULONGLONG time;
}sResults;

BOOL
gtcInterruptLatency (sTimer * timer, DWORD dwNPlusOne)
{
    /* be hopeful until proven otherwise */
    BOOL returnVal = TRUE;

    ULONGLONG ullStartTime, ullEndTime;

    sResults results[MAX_FOUND];

    /*
    * handle to the current thread. This doesn't need to be
    * closed when processing is complete.
    */
    HANDLE hThisThread = GetCurrentThread ();

    /* return value from the getTick function in timer */
    BOOL bGetTickReturnVal;

    /* get the quantum so that we can set this back */
    DWORD dwOldThreadQuantumMS = CeGetThreadQuantum (hThisThread);

    if (dwOldThreadQuantumMS == DWORD_MAX)
    {
        Error (_T("Couldn't get the thread quantum. GetLastError is %u."),
            GetLastError ());
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /* set the thread quantum to 0, meaning that we will never be swapped out */
    if (!CeSetThreadQuantum (hThisThread, 0))
    {
        Error (_T("Couldn't set the thread quantum. GetLastError is %u."),
            GetLastError ());
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /*
    * At this point the quantum is set such that we will run until we
    * block.
    */

    DWORD dwNumFound = 0;
    BOOL firstTime = TRUE;

    while (dwNumFound < MAX_FOUND)
    {

        /*
        * put this process at the beginning of the quantum
        * (CeSetThreadQuantum takes affects on the next scheduling cycle).
        */
        // Sleep (0);

        DWORD it = 0;



        /*
        * put myself at the beginning of the OS tick. If we don't do this we
        * can get a hardware interrupt in the middle which will confuse the test
        */
        DWORD prevGTC = GetTickCount();
        while (GetTickCount() == prevGTC);


        while (1)
        {

            /*
            * loop until the time changes, meaning that an interrupt has fired
            * and the timer has been serviced. Quantums are generally at least
            * 25 ms, and since the timer should be updated every 1 ms this won't
            * introduce too much noise.
            */

            /* get the timer to start at */
            if ((*timer->getTick)(ullStartTime) == FALSE)
            {
                Error (_T("Failed to get tick count for %s."),
                    timer->name);

                returnVal = FALSE;
                goto cleanAndReturn;
            }

            /* loop until we get an error or the time changes */
            while (1)
            {
                if (((bGetTickReturnVal = (*timer->getTick)(ullEndTime)) != TRUE) ||
                    (ullStartTime != ullEndTime))
                {
                    break;
                }
            }

            if (bGetTickReturnVal == FALSE)
            {
                /* the loop above failed because of a bad call to get getTick */
                Error (_T("Failed to get tick count for %s in initialization loop."),
                    timer->name);

                returnVal = FALSE;
                goto cleanAndReturn;
            }

            /*
            * the resolution is the value between these two clocks
            */
            ULONGLONG ullRes;

            if (AminusB (ullRes, ullEndTime, ullStartTime, timer->ullWrapsAt) == FALSE)
            {
                returnVal = FALSE;
                goto cleanAndReturn;
            }

            if ((ullRes > (ULONGLONG) dwNPlusOne) && (!firstTime))
            {
                /* we found an interrupt */
                /* Info (_T("ullStartTime %I64u ullEndTime %I64u it %u time %I64u"),
                ullStartTime, ullEndTime, it, ullRes);
                */
                results[dwNumFound].startTime = ullStartTime;
                results[dwNumFound].endTime = ullEndTime;
                results[dwNumFound].it = it;
                results[dwNumFound].time = ullRes;

                dwNumFound++;
                break;

            }

            firstTime = FALSE;

            it++;

        } /* while */

    } /* while (dwNumFound... */


    /* print out results */
    for (DWORD curr = 0; curr < dwNumFound; curr++)
    {
        /* we found an interrupt */
        Info (_T("ullStartTime %I64u ullEndTime %I64u it %u time %I64u"),

            results[curr].startTime,
            results[curr].endTime,
            results[curr].it,
            results[curr].time);
    }

cleanAndReturn:

    if (dwOldThreadQuantumMS != DWORD_MAX)
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


#endif
