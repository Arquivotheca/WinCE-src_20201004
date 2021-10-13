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
* tuxGetIdleTime.cpp
*/

/* this files contains the code used to test GetIdleTime */

/* common utils */
#include "..\..\..\common\commonUtils.h"

#include <windows.h>
#include <tchar.h>
#include <tux.h>

/* for busy sleep routine */
#include "sleepRoutines.h"

/* for gtc resolution calculations */
#include "timerConvert.h"

/* for command line macros */
#include "timersCmdLine.h"
/*
* The system should be at least this much idle during the test.
* If the system is not this much idle then the test fails.
*/
#define PERCENT_IDLE_THRESHOLD (0.95)

/*
* The low and high values bounding the sleep times. These are
* inclusive.
*/
#define SLEEP_BOUND_LOW_END (10)
#define SLEEP_BOUND_HIGH_END (1000)

/* Test length is ms */
#define IDLE_TEST_LENGTH_MS (2 * 60 * 1000)

/************ Definitions **************************************************/

/*
* get the run time for the idle test. If none is given then use
* the default passed in through the second param.
*
* return true on success and false otherwise.
*/
BOOL
handleIdleRunTimeParams (const TCHAR szArgString[], DWORD * pdwRunTimeMS);

/*
* get the idle threshold percent for the idle test. If none is given then use
* the default passed in through the second param.
*
* return true on success and false otherwise.
*/
BOOL
handleIdleThresholdParam (DOUBLE * pdoIdleThreshold);

/*
* return true if the test can continue and false otherwise.
*
* Note that this function also checks for bad implementations. In
* this case we want the test to continue, so we return true.
*/
BOOL
doesThisPlatSupportIdleTime();

/*
* Gets the resolution of the gtc.
*
* The resolution retrieval function returns the lowest calculated
* resolution and the range of the values calculated. We ignore the
* range. Other tests confirm this data and the user should be fixing
* those bugs before running this test.
*/
BOOL
getGTCResolution (DWORD * pdwRes);

/******************************************************************************
* *
* TESTS *
* *
******************************************************************************/

/*
* This test sleeps for random periods of time. After each sleep
* period the test confirms that the idle time has been updated
* correctly.
*/
TESTPROCAPI idleTime(
                     UINT uMsg,
                     TPPARAM tpParam,
                     LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* assume no error until proven otherwise */
    INT returnVal = TPR_FAIL;

    /*
    * only handle the run test message type
    */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    breakIfUserDesires();
    
    //Check for multiple processor system
    DWORD dwNumCPUs = CeGetTotalProcessors();
    DWORD dwOldAffinity = 0xffffffff;
    DWORD dwNewAffinity = 0xffffffff;
    DWORD dwAffinity = 1;
    BOOL bRestoreAff = TRUE;
    
    HANDLE hProc = GetCurrentProcess();
    Info (_T(""));
    Info (_T("Total Processors in System is %u"),dwNumCPUs);

    if (dwNumCPUs > 1)
    {
        // Set the process affinity to lock this test to one CPU
        if (!(CeGetProcessAffinity(hProc, &dwOldAffinity)))
        {
            Error (_T("Unable to get process affinity."));
            bRestoreAff = FALSE;
            goto cleanAndReturn; 
        }
        Info (_T("System has multiple CPUs. Locking test to a CPU."));
        Info (_T(""));
        if (!(CeSetProcessAffinity(hProc, dwAffinity)))
        {
            Error (_T("Unable to set process affinity."));
            bRestoreAff = FALSE;
            goto cleanAndReturn; 
        }
        
        // Verify Affinity 
        if (!(CeGetProcessAffinity(hProc, &dwNewAffinity)))
        {
            Error (_T("Unable to get process affinity."));
            goto cleanAndReturn; 
        }
        if(dwNewAffinity != dwAffinity)
        {
            Error (_T("Unable to lock test to a CPU."));
            goto cleanAndReturn;
        }
    }

    /******* Info **************************************************************/

    Info (_T("This test sleeps for random time periods. After each sleep cycle"));
    Info (_T("the test confirms that the idle time is within range of the elapsed"));
    Info (_T("time. If the system is noisy then this test will fail. Drivers"));
    Info (_T("and applications waking to service stuff will cause noise. Please"));
    Info (_T("run this test on an image that does not have these types of apps"));
    Info (_T("enabled."));
    Info (_T(""));

    /******* initialize vars ***************************************************/

    double doThresholdPercentIdle = PERCENT_IDLE_THRESHOLD;

    /* get idle threshold if given. */
    if (!handleIdleThresholdParam (&doThresholdPercentIdle))
    {
        goto cleanAndReturn;
    }

    DWORD dwThresholdResolution;

    Info (_T("Trying to get GTC resolution."));

    if (!getGTCResolution (&dwThresholdResolution))
    {
        goto cleanAndReturn;
    }

    if (DWordMult (dwThresholdResolution, 2, &dwThresholdResolution) != S_OK)
    {
        Error (_T("Overflowed when multiplying resolution by 2."));
        goto cleanAndReturn;
    }

    Info (_T(""));
    Info (_T("Using a resolution threshold of %u ms."), dwThresholdResolution);
    Info (_T(""));

    DWORD dwRandLowMS = SLEEP_BOUND_LOW_END;
    DWORD dwRandHighMS = SLEEP_BOUND_HIGH_END;

    DWORD dwTestLengthMS = IDLE_TEST_LENGTH_MS;

    /* get test run time if given. */
    if (!handleIdleRunTimeParams (ARG_STRING_IDLE_RUN_TIME, &dwTestLengthMS))
    {
        goto cleanAndReturn;
    }

    Info (_T("")); //blank line

    if (dwTestLengthMS == 0)
    {
        Error (_T("Test length cannot be zero."));
    }

    /******* Is getIdleTime supported? *******************************************/

    /* if get true continue with the test */
    /* this function sleeps, so do it before we figure out the test time */
    if (!doesThisPlatSupportIdleTime())
    {
        goto cleanAndReturn;
    }

    /******* rollover check on test time *****************************************/

    DWORD dwTestStartMS = GetTickCount();

    DWORD dwTestEnd = dwTestLengthMS + dwTestStartMS;

    if (dwTestEnd < MAX (dwTestLengthMS, dwTestStartMS))
    {
        Info (_T("GTC will overflow during this test."));
        Info (_T("Debug OS builds set the clock back 3 minutes to allow folks"));
        Info (_T("to easily test for overflow conditions in their code. We "));
        Info (_T("will wait for three minutes and try again."));
        Info (_T(""));

        Sleep (3 * 60 * 1000);

        dwTestStartMS = GetTickCount();

        dwTestEnd = dwTestLengthMS + dwTestStartMS;

        if (dwTestEnd < MAX (dwTestLengthMS, dwTestStartMS))
        {
            Error (_T("Waiting three minutes didn't solve the problem. The clock"));
            Error (_T("will still roll over. Test start is %u and test length is"),
                dwTestStartMS);
            Error (_T("%u. Exiting..."), dwTestLengthMS);
            goto cleanAndReturn;
        }

        Info (_T("Restarting the test..."));
        Info (_T(""));
    }

    /******* Variable Initialization ********************************************/

    /*
    * running total of the idle time and sleep time used by the sleep statements.
    * this the test time spent testing the system. It does not include the test
    * time used for processing the data.
    */
    DWORD dwRunningIdleTimeMS = 0;
    DWORD dwRunningSleepTimeMS = 0;

    /*
    * The wall clock test time. This is the total time for the test
    * (both idle and gtc).
    */
    DWORD dwOutsideStartingIdleTimeMS = GetIdleTime();
    DWORD dwOutsideStartingGTCTimeMS = GetTickCount();

    /* we run the entire test, even if it fails in the middle */
    BOOL bRet = TRUE;

    /******* Start Test *********************************************************/

    while (GetTickCount () < dwTestEnd)
    {
        DWORD dwSleepTime = getRandomInRange (dwRandLowMS, dwRandHighMS);

        /* get the data */
        DWORD dwSleepStartMS = GetTickCount();
        DWORD dwIdleStartMS = GetIdleTime();

        Sleep (dwSleepTime);

        DWORD dwIdleEndMS = GetIdleTime();
        DWORD dwSleepEndMS = GetTickCount();

        /* figure out how much time has elapsed, accounting for rollover */
        DWORD dwIdleTimeElapsed = AminusB (dwIdleEndMS, dwIdleStartMS);
        DWORD dwGTCElapsed = AminusB (dwSleepEndMS, dwSleepStartMS);

        /* update the running totals */
        dwRunningIdleTimeMS += dwIdleTimeElapsed;
        dwRunningSleepTimeMS += dwGTCElapsed;

        double doPercentIdle = (double) dwIdleTimeElapsed / (double) dwGTCElapsed;

        Info (_T("Sleep time: %u gtc elapsed: %u idle elapsed: %u - %u = %u")
            _T(" ratio %g"),
            dwSleepTime, dwGTCElapsed,
            dwIdleEndMS, dwIdleStartMS, dwIdleTimeElapsed, doPercentIdle);

        if (dwIdleTimeElapsed > dwGTCElapsed)
        {
            Error (_T("Idle time can't be longer than the actual time elapsed."));
            Error (_T("%u > %u"), dwIdleTimeElapsed, dwGTCElapsed);
            bRet = FALSE;
        }

        /*
        * threshold is triggered if we are less than the threshold and the gap between
        * values is greater than twice the resolution.
        *
        * The second case avoids issues seen for very small sleep times.
        */
        if ((doPercentIdle < doThresholdPercentIdle) &&
            (dwGTCElapsed - dwIdleTimeElapsed > dwThresholdResolution))
        {
            Error (_T("The machine should be idle and isn't. ")
                _T("Threshold is %g and value is %g"),
                doThresholdPercentIdle, doPercentIdle);
            bRet = FALSE;
        }

    }

    /* record ending test time */
    DWORD dwOutsideEndingIdleTimeMS = GetIdleTime();
    DWORD dwOutsideEndingGTCTimeMS = GetTickCount();

    DWORD dwOutsideIdleTimeElapsed =
        AminusB (dwOutsideEndingIdleTimeMS, dwOutsideStartingIdleTimeMS);
    DWORD dwOutsideGTCElapsed =
        AminusB (dwOutsideEndingGTCTimeMS, dwOutsideStartingGTCTimeMS);

    Info (_T(""));
    Info (_T("Test time (does not include processing): Sleep %u Idle: %u"),
        dwRunningSleepTimeMS, dwRunningIdleTimeMS);

    Info (_T("Total time (start to end): Sleep %u Idle: %u"),
        dwOutsideGTCElapsed, dwOutsideIdleTimeElapsed);
    Info (_T(""));

    if (dwOutsideGTCElapsed < dwRunningSleepTimeMS)
    {
        Error (_T("Total gtc time elapsed must be greater than test time.")
            _T(" %u < %u"),
            dwOutsideGTCElapsed, dwRunningSleepTimeMS);
        bRet = FALSE;
    }

    if (dwOutsideIdleTimeElapsed < dwRunningIdleTimeMS)
    {
        Error (_T("Total idle time elapsed must be greater than test idle time.")
            _T(" %u < %u"),
            dwOutsideIdleTimeElapsed, dwRunningIdleTimeMS);
        bRet = FALSE;
    }

    if (bRet)
    {
        Info (_T("Test Passed."));
    }
    else
    {
        Error (_T("Test Failed."));
        goto cleanAndReturn;
    }

    returnVal = TPR_PASS;


cleanAndReturn:
    
    if ((dwNumCPUs > 1) && (bRestoreAff))
    {
        // Return to old affinity
        Info (_T(""));
        Info (_T("Unlocking test from CPU"));
        if (!(CeSetProcessAffinity(hProc, dwOldAffinity)))
        {
            Error (_T("Unable to set process affinity."));
        }
        Info (_T(""));
    }

    return (returnVal);
}

DWORD BusyIdleTimePerProc(LPVOID lpParam)
{
    const DWORD *pdwTestEnd=(DWORD *)lpParam;
    DWORD dwRandLowMS = SLEEP_BOUND_LOW_END;
    DWORD dwRandHighMS = SLEEP_BOUND_HIGH_END;
    DWORD dwThreadAffinity=0;
    HTHREAD hThread=NULL;
    DWORD dwIdleEndMS=0;
    DWORD dwIdleStartMS=0;
    DWORD dwProcId=0;
    hThread=GetCurrentThread();

    if(!CeGetThreadAffinity( hThread, &dwThreadAffinity))
    {
        Error(_T("BusyIdleTimePerProc: Error While getting thread affinity"));
        return FALSE;
        //return error
    }
    //if its single proc platform, affinity returned is 0. Processor number is still 1.
    dwProcId=dwThreadAffinity?dwThreadAffinity:1;
    while (GetTickCount () < *pdwTestEnd)
    {
        DWORD dwSleepTime = getRandomInRange (dwRandLowMS, dwRandHighMS);

        /* get the data */
        if(!CeGetIdleTimeEx ( dwProcId, &dwIdleStartMS))
        {
            Error(_T("BusyIdleTimePerProc: Error While getting Idle time"));
            return FALSE;
            //error
        }

        BusyOsSleepFunction (dwSleepTime);

        if(!CeGetIdleTimeEx ( dwProcId, &dwIdleEndMS))
        {
            Error(_T("BusyIdleTimePerProc: Error While getting Idle time"));
            return FALSE;
            //error
        }

        Info (_T("Processor %u, Sleep time: %u idle start: %u idle end: %u"),
            dwProcId, dwSleepTime, dwIdleEndMS, dwIdleStartMS);

        if (dwIdleStartMS != dwIdleEndMS)
        {
            Error (_T(" Processor %u, Elapsed idle time should be zero. Instead, start is %u ")
                _T("and end is %u."), dwProcId, dwIdleStartMS, dwIdleEndMS);
            return FALSE;
        }
    }// end while (GetTickCount () < *pdwTestEnd)
    return TRUE;
}

TESTPROCAPI idleTimeWhileBusy(
                              UINT uMsg,
                              TPPARAM tpParam,
                              LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* assume no error until proven otherwise */
    INT returnVal = TPR_FAIL;

    /*
    * only handle the run test message type
    */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    breakIfUserDesires();

    /******* initialize vars ***************************************************/

    DWORD dwExitCode= (DWORD)-1;
    DWORD iProc=0;
    DWORD dwNumProcs= CeGetTotalProcessors();
    HTHREAD *hBusyIdleTimePerProcThread = NULL;
    DWORD dwTestLengthMS = IDLE_TEST_LENGTH_MS;

    /* get test run time if given. */
    if (!handleIdleRunTimeParams (ARG_STRING_IDLE_RUN_TIME, &dwTestLengthMS))
    {
        goto cleanAndReturn;
    }

    Info (_T("")); //blank line

    if (dwTestLengthMS == 0)
    {
        Error (_T("Test length cannot be zero."));
    }

    /******* Is getIdleTime supported? *******************************************/

    /* if get true continue with the test */
    /* this function sleeps, so do it before we figure out the test time */
    if (!doesThisPlatSupportIdleTime())
    {
        goto cleanAndReturn;
    }

    /******* rollover check ****************************************************/

    DWORD dwTestStartMS = GetTickCount();

    DWORD dwTestEnd = dwTestLengthMS + dwTestStartMS;

    if (dwTestEnd < MAX (dwTestLengthMS, dwTestStartMS))
    {
        Info (_T("GTC will overflow during this test."));
        Info (_T("Debug OS builds set the clock back 3 minutes to allow folks"));
        Info (_T("to easily test for overflow conditions in their code. We "));
        Info (_T("will wait for three minutes and try again."));
        Info (_T(""));

        Sleep (3 * 1000);

        dwTestStartMS = GetTickCount();

        dwTestEnd = dwTestLengthMS + dwTestStartMS;

        if (dwTestEnd < MAX (dwTestLengthMS, dwTestStartMS))
        {
            Error (_T("Waiting three minutes didn't solve the problem. The clock"));
            Error (_T("will still roll over. Test start is %u and test length is"),
                dwTestStartMS);
            Error (_T("%u. Exiting..."), dwTestLengthMS);
            goto cleanAndReturn;
        }

        Info (_T("Restarting the test..."));
        Info (_T(""));
    }


    /******* Variable Initialization ********************************************/

    /* we run the entire test, even if it fails in the middle */
    BOOL bRet = TRUE;

    /******* Start Test *********************************************************/

    hBusyIdleTimePerProcThread= new HTHREAD[dwNumProcs];

    if(!hBusyIdleTimePerProcThread) {
        Error(_T("Memory allocation failed @ %d. Is the System OOM?\r\n"), __LINE__);
        goto cleanAndReturn;
    }

    for (iProc=0; iProc<dwNumProcs;iProc++)
    {
        hBusyIdleTimePerProcThread[iProc] = NULL;
    }
    //for each processor core, create a thread and assign processor affinity
    //Then make it busy and measure idle time per core

    for (iProc=0;iProc<dwNumProcs;iProc++)
    {
        hBusyIdleTimePerProcThread[iProc]=CreateThread(NULL,NULL,BusyIdleTimePerProc,&dwTestEnd,CREATE_SUSPENDED,NULL);
        if(!CeSetThreadAffinity(hBusyIdleTimePerProcThread[iProc],iProc+1))
        {
            Error(_T("GetIdleTime: Error while setting affinity of Thread %u"),iProc);
        }
        else
        {
            Info(_T("Affinity set to Processor #%u, for thread #%u"), iProc+1, iProc);
        }
        ResumeThread(hBusyIdleTimePerProcThread[iProc]);
    }
    for (iProc=0;iProc<dwNumProcs;iProc++)
    {
        if(WaitForSingleObject(hBusyIdleTimePerProcThread[iProc], 2*dwTestLengthMS) != WAIT_OBJECT_0)
        {
            PREFAST_SUPPRESS( 6258, TEXT("Improper thread cleanup is acceptable\r\n"));
            TerminateThread(hBusyIdleTimePerProcThread[iProc], (DWORD)-1);
            Error (_T("GetIdleTime: Error while Waiting for thread #%u to exit."), iProc);
            Error (_T("This can happen if the runtime you have chosen is longer than test timeout."));
            bRet = FALSE;
        }
        else
        {
            GetExitCodeThread(hBusyIdleTimePerProcThread[iProc], &dwExitCode);
            if(dwExitCode != TRUE)
            {
                Error (_T("GetIdleTime: Thread #%u returned Error"), iProc);
                bRet = FALSE;
            }
        }
        CloseHandle(hBusyIdleTimePerProcThread[iProc]);
    }

    if (bRet)
    {
        Info (_T("Test Passed."));
    }
    else
    {
        Error (_T("Test Failed."));
        goto cleanAndReturn;
    }

    returnVal = TPR_PASS;

cleanAndReturn:
    if(hBusyIdleTimePerProcThread) delete [] hBusyIdleTimePerProcThread;
    return (returnVal);
}



/******************************************************************************
* *
* HELPERS *
* *
******************************************************************************/

/*
* get the run time for the idle test. If none is given then use
* the default passed in through the second param.
*
* return true on success and false otherwise.
*/
BOOL
handleIdleRunTimeParams (const TCHAR szArgString[], DWORD * pdwRunTimeMS)
{
    if (!pdwRunTimeMS)
    {
        return (FALSE);
    }

    BOOL bUserSuppliedParams = FALSE;

    ULONGLONG ullRunTimeS = (ULONGLONG)DWORD_MAX+1;

    /*
    * if user doesn't specify the -c param to tux the cmd line global
    * var is null. In this case don't even try to parse command line
    * args.
    */
    if (g_szDllCmdLine != NULL)
    {
        cTokenControl tokens;

        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndExit;
        }

        if (getOptionTimeUlonglong (&tokens, szArgString,
            &ullRunTimeS))
        {
            bUserSuppliedParams = TRUE;

            /* convert to ms */
            if (ULongLongMult (ullRunTimeS, 1000UL, &ullRunTimeS) != S_OK)
            {
                Error (_T("Overflow when converting seconds to ms."));
                goto cleanAndExit;
            }

            if (ullRunTimeS > (ULONGLONG) DWORD_MAX)
            {
                Error (_T("Run time is too large. %I64u > DWORD_MAX"),
                    ullRunTimeS);
                goto cleanAndExit;
            }
        }
    }

    if (bUserSuppliedParams)
    {
        *pdwRunTimeMS = (DWORD) ullRunTimeS;
    }

    /* convert to realistic time */
    double doTime;
    TCHAR * szUnits;
    realisticTime (((double) *pdwRunTimeMS) / 1000.0, doTime, &szUnits);

    if (bUserSuppliedParams)
    {
        Info (_T("Using the user supplied test run time, which is %g %s."),
            doTime, szUnits);
    }
    else
    {
        /* use default */
        Info (_T("Using the default test run time, which is %g %s"),
            doTime, szUnits);
    }

cleanAndExit:

    return (TRUE);
}

BOOL
handleIdleThresholdParam (DOUBLE *pdoIdleThreshold)
{
    if (!pdoIdleThreshold)
    {
        return (FALSE);
    }

    BOOL bUserSuppliedParams = FALSE;

    DOUBLE doThreshold;

    /*
    * if user doesn't specify the -c param to tux the cmd line global
    * var is null. In this case don't even try to parse command line
    * args.
    */
    if (g_szDllCmdLine != NULL)
    {
        cTokenControl tokens;

        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndExit;
        }

        if (getOptionDouble(&tokens, ARG_STRING_IDLE_THRESHOLD, &doThreshold))
        {
            /*Check if the threshold is between 0 and 1*/
            if((doThreshold < 0) ||(doThreshold > 1))
            {
                Error (_T("User supplied value for Idle Threshold is invalid."));
                Error (_T("You need to choose a value between 0 and 1."));
            }
            else
            {
                /*Retrived the value successfully. Now set the value*/
                *pdoIdleThreshold = doThreshold;

                bUserSuppliedParams = TRUE;
            }
        }
    }

    if (bUserSuppliedParams)
    {
        Info (_T("Using the user supplied idle threshold, which is %g."),
            *pdoIdleThreshold);
    }
    else
    {
        /* use default */
        Info (_T("Using the default idle threshold, which is %g"),
            *pdoIdleThreshold);
    }

cleanAndExit:

    return (TRUE);
}


/*
* return true if the test can continue and false otherwise.
*
* This function checks if Idle time is implemented.
*/
BOOL
doesThisPlatSupportIdleTime()
{
    /* make sure that this is even supported */
    if (GetIdleTime () == DWORD_MAX)
    {
        Error (_T("GetIdleTime returned DWORD_MAX, which means that it isn't"));
        Error (_T("supported on this platform. The test is going to be aborted now."));
        return FALSE;
    }

    /*
    * now test if it is actually implemented. should not return zero and should increment when device is idle.
    */
    const DWORD dwSleepTime = 500;
    const DWORD dwMargin = 1;
    DWORD dwIdle1 = GetIdleTime();
    DWORD dwIdle2 = 0;
    DWORD dwIdleTimeElapsed = 0;

    /* repeat 3 times so we catch it if it is just returning random values*/
    for(DWORD dw = 0; dw < 3; dw++)
    {
        /* Sleep for 500ms */
        Sleep(dwSleepTime);

        /* Get Idle time */
        dwIdle2 = GetIdleTime();

        /* Calculate Idle time elapsed */
        dwIdleTimeElapsed = dwIdle2 - dwIdle1;

        /* Idle time elapsed should be greater than zero and less than the sleep time */
        if((dwIdleTimeElapsed == 0) || (dwIdleTimeElapsed > (dwSleepTime + dwMargin)))
        {
            Info (_T(""));
            Info (_T("Slept for %u ms and Idle time elapsed is being reported as"),dwSleepTime);
            Info (_T("%u ms. GetIdleTime is not working."), dwIdleTimeElapsed);
            Info (_T("Check that curridlelow, curridlehigh, and idleconv are being"));
            Info (_T("set correctly in the OAL."));
            Info (_T("The test is going to be aborted now."));
            Info (_T(""));
            return FALSE;
        }

        dwIdle1 = dwIdle2;
    }

    return TRUE;
}

/*
* Gets the resolution of the gtc.
*
* The resolution retrieval function returns the lowest calculated
* resolution and the range of the values calculated. We ignore the
* range. Other tests confirm this data and the user should be fixing
* those bugs before running this test.
*/
BOOL
getGTCResolution (DWORD * pdwRes)
{
    if (!pdwRes)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    ULONGLONG ullN, ullRange;

    /* init timer */
    sTimer timer;

    /* initialize timer struct */
    if (initTimer (&timer, &gGetTickCountTimer) == FALSE)
    {
        /* getTickCount should always be present */
        Error (_T("Could not initalize the GetTickCount timer structure."));

        goto cleanAndReturn;
    }

    /* use 10 iterations and don't print out status messages in the function */
    if (!getResolution (&timer, 10, &ullN, &ullRange, FALSE))
    {
        Error (_T("Couldn't get resolution of the GTC."));
        goto cleanAndReturn;
    }

    Info (_T("For the GTC, the lowest calculated resolution is %I64u. ")
        _T("The range is %I64u."), ullN, ullRange);

    /*
    * for now we will ignore the range part. This will be checked in another
    * test.
    */
    if (ullN > (ULONGLONG) DWORD_MAX)
    {
        Error (_T("Resolution is larger than DWORD_MAX."));
        goto cleanAndReturn;
    }

    if (ullN == 0)
    {
        Error (_T("Lowest calculated resolution can't be zero."));
        goto cleanAndReturn;
    }

    *pdwRes = (DWORD) ullN;

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}
