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

#include <windows.h>
#include <tchar.h>
#include <tux.h>

#include "..\..\..\common\commonUtils.h"

#include "tuxCacheChurn.h"
#include "cacheChurn.h"
#include "breakForDebug.h"
#include "cacheRand.h"


/****** test procs ************************************************************/

TESTPROCAPI
PrintUsage(
           UINT uMsg,
           TPPARAM tpParam,
           LPFUNCTION_TABLE_ENTRY lpFTE)
{

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info (_T("This test allocates, reads, writes, and frees memory.  The goal is to"));
    Info (_T("stress the cache subsystem.  By inference, this also covers the VM memory"));
    Info (_T("hierarchy."));
    Info (_T(""));
    Info (_T("Command Line Options:"));
    Info (_T(""));
    Info (_T("  -runTime <time>   Run time (trailing 'm' means minutes, 'h' means hours, etc)."));
    Info (_T("                    Default: 10 minutes for each test."));
    Info (_T("  -cacheSize <num>  Cache size in bytes.  Default: L1 data cache size."));
    Info (_T("  -pageSize <num>   Page size in bytes.  Default: system reported size."));
    Info (_T("  -maxWorkingSet <num>   Specify the working set size (see below)."));
    Info (_T("                         Only applies to the maxWorkingSet test case."));
    Info (_T("  -caliThreshold <num>   Specify the calibration threhold in seconds. Default is 10 seconds."));
    Info (_T("  -breakOnFailure <num>  Break immediately on failure.  Default to no."));
    Info (_T(""));
    Info (_T("This test uses the notion of a working set.  For this test, this is denoted as the"));
    Info (_T("memory allocated by this test.  (note: A standard definition of working set includes"));
    Info (_T("the local variables, etc., associated with a program.  Those aren't included in this"));
    Info (_T("discussion.)  The working set size will increase or decrease, depending on whether"));
    Info (_T("the test behavior."));
    Info (_T(""));
    Info (_T("The working set size is determined by the test case.  It can range from the cache size"));
    Info (_T("to 16 times the cache size.  These sizes exercise different behaviors in the caches."));
    Info (_T(""));
    Info (_T("The cacheSize option overrides the value returned from CeGetCacheInfo (and by"));
    Info (_T("extention IOCTL_HAL_GET_CACHE_INFO) for the L1 Data cache size.  This value is then"));
    Info (_T("factored into the given test case to determine the working set."));
    Info (_T(""));
    Info (_T("The maxWorkingSet option works only with the test case bearing its name.  It allows"));
    Info (_T("the user to set a hard and fast maximum working set size.  This option overrides the"));
    Info (_T("system cache size and the cacheSize option when used with this specific test case."));
    Info (_T(""));
    Info (_T("The page size is read from the system.  It is currently 4KB on CE.  This test uses "));
    Info (_T("VirtualAlloc for all allocations.  VirtualAlloc always allocates in increments of the"));
    Info (_T("page size.  This test allocates chunks of memory based on the page size.  If you want "));
    Info (_T("to change these allocation sizes use the pageSize option.  This value can be anything"));
    Info (_T("other than zero.  In general, you should only use values that are multiples of the page"));
    Info (_T("size unless you have specific reasons not to."));
    Info (_T(""));
    Info (_T("The breakOnFailure option allows you to break immediately on a cache / memory related"));
    Info (_T("failure.  The goal is to stop the system as soon as possible, so that you don't loose"));
    Info (_T("state that might help you track down the failure.  Also, this provides a hook to other"));
    Info (_T("possible debugging mechanisms."));
    Info (_T(""));
    Info (_T("Please see the docs for more information."));

    return (TPR_PASS);
}


TESTPROCAPI testCacheChurn(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* for easy handling of command line stuff */
    cTokenControl tokens;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    breakIfUserDesires();

    UINT uNumber = 0;
    /* set up random number generator to seed from tux */
    rand_s(&uNumber);
    setCacheRandSeed(uNumber);

    /* we ignore the command line for the cache size in this case */

    DWORD dwWorkingSet;
    DWORD dwPageSize;
    ULONGLONG ullRunTimeS;
    DWORD dwArraySize;
    DWORD dwCalicrationThreshold;

    /* handle break on failure flag if set */
    if (!handleBreakOnFailureFlag ())
    {
        goto cleanAndReturn;
    }

    if (!retrieveOptions(lpFTE, &dwWorkingSet, &dwPageSize, &ullRunTimeS, &dwCalicrationThreshold)) {
        Error (_T("Invalid command line parameters or system config"));
        goto cleanAndReturn;
    }

    /* figure out array sizes */
    if (dwWorkingSet % dwPageSize == 0)
    {
        /* when we devide we won't get a remainder */
        dwArraySize = dwWorkingSet / dwPageSize;

        Info (_T("Number of elements in the blocks array is %u."),
            dwArraySize);
    }
    else
    {
        dwArraySize = dwWorkingSet / dwPageSize + 1;

        Info (_T("Page size doesn't devide evenly into working set size."));
        Info (_T("Adding one, so that the resulting working set is larger."));
        Info (_T("Number of elements in the blocks array is %u."),
            dwArraySize);
    }

    Info (_T(""));

    if (runTestGivenWorkingSize(
                           dwArraySize,
                           dwPageSize,
                           ullRunTimeS,
                           dwCalicrationThreshold))
    {
        returnVal = TPR_PASS;
    }
    else
    {
        returnVal = TPR_FAIL;
    }


cleanAndReturn:

    return (returnVal);
}


/*
* run the test, given a working set size.
*
* the working set size passed in to this function does not
* need to be checked for sanity by the caller.  any value will
* work.  Note, though, that the array size will be adjusted for
* values that don't divide evenly.  This function alerts the user
* when it is doing this, and continues.
*
* returns true on success and false otherwise.
*/

BOOL
runTestGivenWorkingSize (
                  DWORD dwArraySize,
                  DWORD dwPageSize,
                  ULONGLONG ullRunTimeS,
                  DWORD dwCalibrationInMs)
{
    BOOL returnVal = FALSE;

    /* handle break on failure flag if set */
    if (!handleBreakOnFailureFlag ())
    {
        goto cleanAndReturn;
    }

    /* set up structures needed to run tests */
    sOverallStatistics stats;

    /* calibrate */

    Info (_T("Calibrating..."));
    Info (_T(""));

    ULONGLONG ullTestCalibrationRuns = 1;

    /*
    * these are ulonglong because we are using functions from the timing code,
    * which take ulonglong parameters
    */
    ULONGLONG ullDiff = 0;
    ULONGLONG ullTimeBegin, ullTimeEnd;

    for(;;)
    {
        ullTimeBegin = (ULONGLONG) GetTickCount ();

        /* run the test
        * don't print for calibration
        */
        if (!iterateBlocks (dwArraySize, dwPageSize,
            ullTestCalibrationRuns, &stats, 0, FALSE))
        {
            Error (_T("Test failed during calibration."));
            Error (_T("Calibration runs the given cache test for a short length"));
            Error (_T("of time to determine how long it takes.  Failure commonly"));
            Error (_T("means that the cache test failed.  Most likely, something"));
            Error (_T("is wrong with the cache routines or memory subsystem "));
            Error (_T("(either hardware or software) on the machine."));

            goto cleanAndReturn;
        }

        ullTimeEnd = (ULONGLONG) GetTickCount ();

        /* subtract, accounting for overflow */
        if (AminusB (ullDiff, ullTimeEnd, ullTimeBegin, DWORD_MAX) == FALSE)
        {
            /*
            * This should never happen.  If something goes wrong, then there is
            * a bug in the code.
            */
            IntErr (_T("AminusB failed in runCacheTest"));
            goto cleanAndReturn;
        }

        /* 1000 is completely arbitrary */
        if (ullDiff > (dwCalibrationInMs * 1000ULL))
        {
            Info (_T(""));
            Info (_T("Warning!  Calibration run time is quite large (%I64u ms)."),
                ullDiff);
            Info (_T("Warning!  GTC might have gone backwards unexpectedly."));
            Info (_T(""));
        }

        if (ullDiff < dwCalibrationInMs)
        {
            /* if within threshold double number of runs and try again */

            if (ULongLongMult (ullTestCalibrationRuns, 2ULL, &ullTestCalibrationRuns) != S_OK)
            {
                /*
                * we overflowed.  We could try to recover but it is much easier
                * to bail at this point.  If this happens the user either has
                * lightening speed caches or something is wrong with either
                * the timers or something else.
                *
                * This should be a very rare error.
                */
                Error (_T("Could not calibrate.  We overflowed the variable "));
                Error (_T("storing the number of calibration runs.  Either the "));
                Error (_T("cache routines are running very fast or the "));
                Error (_T("calibration threshold is set too high."));
                goto cleanAndReturn;
            }
        }
        else
        {
            break;
        }
    } /* while */

    /*
    * we now know how long one test will take.  We calculate how many
    * tests we need to do for the given test length.
    */

    Info (_T(""));
    Info (_T("Calibration results:"));
    Info (_T("%I64u iterations produced in %I64u mili-seconds."),
        ullTestCalibrationRuns, ullDiff);

    /*
    * convert test length into ms.
    */
    ULONGLONG ullRunTimeMs;

    if (ULongLongMult (ullRunTimeS, 1000ULL, &ullRunTimeMs) != S_OK)
    {
        Error (_T("Overflow converting run time to ms."));
        goto cleanAndReturn;
    }

    ULONGLONG ullTotalCalibrations = (ULONGLONG) ((double) ullTestCalibrationRuns / (double) ullDiff);

    ULONGLONG ullTotalIt;
    if (ULongLongMult (ullTotalCalibrations, ullRunTimeMs, &ullTotalIt) != S_OK)
    {
        Error (_T("Overflow total iterations."));
        goto cleanAndReturn;
    }

    double doRealisticTime;
    TCHAR * pszRealisticUnits;
    realisticTime (double (ullRunTimeS), doRealisticTime, &pszRealisticUnits);

    Info (_T(""));
    Info (_T("We are going to do %I64u total iterations."), ullTotalIt);
    Info (_T("This should closely correspond to a run time of %g %s."),
        doRealisticTime, pszRealisticUnits);
    Info (_T(""));

    /* figure out how often do we print messages */

    /* total iterations to be run for the actual test */
    ULONGLONG ullPrintMsgEvery = (ULONGLONG) (((double) ullTestCalibrationRuns / (double) ullDiff) *
        (double) DW_PRINT_MESSAGE_EVERY_N_MS);

    /* do division before cast because ms precision is not needed for this message */
    realisticTime (double (DW_PRINT_MESSAGE_EVERY_N_MS / 1000), doRealisticTime, &pszRealisticUnits);

    Info (_T("The code will print check point messages every %I64u iterations."), ullPrintMsgEvery);
    Info (_T("This should closely correspond to every %g %s."),
        doRealisticTime, pszRealisticUnits);
    Info (_T(""));

    /* run test*/

    if (!iterateBlocks (dwArraySize, dwPageSize,
        ullTotalIt, &stats, ullPrintMsgEvery, TRUE))
    {
        goto cleanAndReturn;
    }

    returnVal = TRUE;

cleanAndReturn:
    return (returnVal);

}

BOOL
handleBreakOnFailureFlag ()
{
    BOOL returnVal = FALSE;

    /* for easy handling of command line stuff */
    cTokenControl tokens;

    /*
    * global command line can't be null, tokenize must work, and option
    * must be present for us to parse it
    */
    if (g_szDllCmdLine && tokenize (g_szDllCmdLine, &tokens) &&
        isOptionPresent (&tokens, (ARG_STR_BREAK_ON_FAILURE)))
    {
        Info (_T("Per user request, break on failure is now true."));
        setBreakOnFailure (TRUE);
    }

    returnVal = TRUE;

    return (returnVal);
}

// see header for the details
BOOL retrieveOptions(
                LPFUNCTION_TABLE_ENTRY lpFTE,
                DWORD* pdwWorkingSet,
                DWORD* pdwPageSize,
                ULONGLONG* pullRunTimeS,
                DWORD* pdwCalibrationInMs)
{

    if (!pdwWorkingSet || !pdwPageSize || !pullRunTimeS)
    {
        Error (_T("Error on initializations."));
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    DWORD dwCacheSize;

    if (!getPageSize (pdwPageSize) ||
      !getRunTime (pullRunTimeS)   ||
      !getCalibrationThreshold(pdwCalibrationInMs))
    {
        goto cleanAndReturn;
    }

    /*
     * Here, if user doesn't choose the option "-maxWorkingSet", then try to get it
     * from the system
     */
    if (!getUserWorkingSet(pdwWorkingSet)) // no user option exists
    {
        /*
         * get the cache size from system, and then convert to working set
         */
        if (!getCacheSize (&dwCacheSize))
        {
            Error (_T("Failed to get cache size."));
            goto cleanAndReturn;
        }

        /* convert this cache size into the working set size of the test */
        if (DWordMult (dwCacheSize, lpFTE->dwUserData, pdwWorkingSet) != S_OK)
        {
            Error (_T("Overflowed when calculating working set size.")
                   _T("%u * %u is too big."), dwCacheSize, lpFTE->dwUserData);
            goto cleanAndReturn;
        }

        if (*pdwWorkingSet < (*pdwPageSize))
        {
            Error (_T("Working set size must be larger than page size.  ")
                   _T("%u < %u"), *pdwWorkingSet, *pdwPageSize);
            goto cleanAndReturn;
        }

        Info (_T("Non-user selected working set size is %u bytes."), *pdwWorkingSet);
    }

    returnVal = TRUE;

cleanAndReturn:
    return (returnVal);
}

BOOL
getUserWorkingSet (DWORD * pdwWorkingSet)
{
    if (!pdwWorkingSet)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /* for easy handling of command line stuff */
    cTokenControl tokens;

    DWORD dwWorkingSet;

    /*
    * global command line can't be null, tokenize must work, and option
    * must be present for us to parse it
    */
    if (g_szDllCmdLine && tokenize (g_szDllCmdLine, &tokens) &&
        isOptionPresent (&tokens, (ARG_STR_WORKING_SET)))
    {
        if (!getOptionDWord (&tokens, ARG_STR_WORKING_SET, &dwWorkingSet))
        {
            Error (_T("Couldn't read command line arg for %s."),
                   ARG_STR_WORKING_SET);
            goto cleanAndReturn;
        }

        Info (_T("User overrode working set size.  It is now %u bytes."),
            dwWorkingSet);
        Info (_T(""));

        *pdwWorkingSet = dwWorkingSet;
        returnVal = TRUE;
    }

cleanAndReturn:
    return (returnVal);
}



/*
* this code does not read the cache size from the user.  It
* will only pull from the system.
*
* if successful then returns true.  if not returns false.  successful
* in this case means that it got a value from the ioctl.  it doesn't
* error check this value.
*/
BOOL
getCacheSize (DWORD * pdwCacheSize)
{
    if (!pdwCacheSize)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /* for easy handling of command line stuff */
    cTokenControl tokens;

    DWORD dwCacheSize = 0;

    /*
    * global command line can't be null, tokenize must work, and option
    * must be present for us to parse it
    */
    if (g_szDllCmdLine && tokenize (g_szDllCmdLine, &tokens) &&
        isOptionPresent (&tokens, (ARG_STR_CACHE_SIZE)))
    {
        if (getOptionDWord (&tokens, ARG_STR_CACHE_SIZE, &dwCacheSize))
        {
        }
        else
        {
            Error (_T("Couldn't read command line arg for %s."),
                ARG_STR_CACHE_SIZE);
            goto cleanAndReturn;
        }

        Info (_T("User overrode cache size.  It is now %u bytes."),
            dwCacheSize);
        Info (_T(""));
    }
    else
    {
        /* struct to store cache information in */
        CacheInfo cacheInfo;

        if (!CeGetCacheInfo (sizeof (cacheInfo), &cacheInfo))
        {
            Error (_T("Call to CeGetCacheInfo.  Maybe IOCTL_HAL_GET_CACHE_INFO isn't implemented?"));
            Error (_T("GetLastError is %u."), GetLastError ());
            goto cleanAndReturn;
        }

        dwCacheSize = cacheInfo.dwL1DCacheSize;

        Info (_T("Using system reported L1 data cache size of %u bytes."),
            dwCacheSize);
        Info (_T(""));
    }

    /* check dwWorkingSize for sanity */
    if (dwCacheSize == 0)
    {
#ifdef x86
        // CeGetCacheInfo currently always returns 0 on x86
        // So assume a 4MB cache size
        Info (_T("CeGetCacheInfo reported a 0 byte cache, but assuming 4MB for L1 cache on this platform\r\n"));
        dwCacheSize = (4*1024*1024);
#else
        Error (_T("Working set size can't be zero."));
        goto cleanAndReturn;
#endif
    }

    *pdwCacheSize = dwCacheSize;
    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);

}

/*
* get page size from system, and then change this side if the user
* specifies ART_STR_PAGE_SIZE.
*
* if all works return true.  if not, return false and print messages
* telling the user what he/she did wrong.
*/
BOOL
getPageSize (DWORD * pdwPageSize)
{
    if (!pdwPageSize)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /* for easy handling of command line stuff */
    cTokenControl tokens;

    SYSTEM_INFO sysInfo;

    /* get system page size */
    GetSystemInfo (&sysInfo);

    /* save this here and don't set to the output until
    * we know what the user wants to do
    */
    DWORD dwSysPageSize = sysInfo.dwPageSize;

    Info (_T("Page size as reported by the system is %u bytes."),
        dwSysPageSize);

    /* figure out if the user wants to override the page size */

    /*
    * global command line can't be null, tokenize must work, and option
    * must be present for us to parse it
    */
    if (g_szDllCmdLine && tokenize (g_szDllCmdLine, &tokens) &&
        isOptionPresent (&tokens, (ARG_STR_PAGE_SIZE)))
    {
        DWORD dwUserPageSize;

        if (getOptionDWord (&tokens, ARG_STR_PAGE_SIZE, &dwUserPageSize))
        {
        }
        else
        {
            Error (_T("Couldn't read command line arg for %s."),
                ARG_STR_PAGE_SIZE);
            goto cleanAndReturn;
        }

        Info (_T("User gave a page size value of %u bytes."),
            dwUserPageSize);
        Info (_T(""));

        if (dwUserPageSize % sizeof (DWORD) != 0)
        {
            Error (_T("Page size as specified for this test must be a multiple")
                _T("of %u (the size of a DWORD)."), sizeof(DWORD));
            goto cleanAndReturn;
        }

        if (dwUserPageSize % dwSysPageSize != 0)
        {
            Info (_T("User specified page size is not a multiple of"));
            Info (_T("the true page size on the system.  The test will"));
            Info (_T("continue, but realize that VirtualAlloc rounds"));
            Info (_T("all allocations up to page boundaries.  This "));
            Info (_T("non-standard page size is just reducing the amount"));
            Info (_T("of data that you read during the test."));
            Info (_T(""));
        }

        *pdwPageSize = dwUserPageSize;
    }
    else
    {
        *pdwPageSize = dwSysPageSize;
    }

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}


BOOL
getRunTime (ULONGLONG * pullRunTimeS)
{
    if (!pullRunTimeS)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /* for easy handling of command line stuff */
    cTokenControl tokens;

    ULONGLONG ullLocalRunTimeS = 0;

    const TCHAR * szOutputStringPrefix = NULL;

    /* figure out if user wants to override run time */

    /*
    * global command line can't be null, tokenize must work, and option
    * must be present for us to parse it
    */
    if (g_szDllCmdLine && tokenize (g_szDllCmdLine, &tokens) &&
        isOptionPresent (&tokens, (ARG_STR_RUN_TIME)))
    {
        if (getOptionTimeUlonglong (&tokens, ARG_STR_RUN_TIME, &ullLocalRunTimeS))
        {
        }
        else
        {
            Error (_T("Couldn't read command line arg for %s."),
                ARG_STR_RUN_TIME);
            goto cleanAndReturn;
        }

        szOutputStringPrefix = _T("User gave a run time value of");
    }
    else
    {
        /* use default */
        ullLocalRunTimeS = ULL_DEFAULT_RUN_TIME_S;

        szOutputStringPrefix = _T("Using default run time of");
    }

    /* print the time to for the user */
    /* use the values set above to get the message correct.*/

    /* for use with realistic time */
    double doRealisticTime;
    TCHAR * pszRealisticUnits;
    realisticTime (double (ullLocalRunTimeS), doRealisticTime, &pszRealisticUnits);
    Info (_T("%s %g %s."),
        szOutputStringPrefix, doRealisticTime, pszRealisticUnits);
    Info (_T(""));

    /* pass run time out to caller */
    *pullRunTimeS = ullLocalRunTimeS;

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}

BOOL
getCalibrationThreshold(DWORD *pdwCalibrationInMs)
{
    if (!pdwCalibrationInMs)
    {
        return (FALSE);
    }

    /* for easy handling of command line stuff */
    cTokenControl tokens;

    DWORD dwCalibrationInSec;

    /*
    * global command line can't be null, tokenize must work, and option
    * must be present for us to parse it
    */
    if (g_szDllCmdLine && tokenize (g_szDllCmdLine, &tokens) &&
        isOptionPresent (&tokens, (ARG_STR_CALI_THRESHOLD))  &&
        getOptionDWord (&tokens, ARG_STR_CALI_THRESHOLD, &dwCalibrationInSec))
    {
        Info (_T("User calibration threshold: %u seconds."), dwCalibrationInSec);
        Info (_T(""));
    }
    else
    {
        /* use the default value */
        dwCalibrationInSec = TEST_CALIBRATION_THRESHOLD_S;
        Info (_T("User calibration threshold: %u seconds."), dwCalibrationInSec);
        Info (_T(""));
    }

    *pdwCalibrationInMs = dwCalibrationInSec * 1000; // milliseconds

    return TRUE;
}
