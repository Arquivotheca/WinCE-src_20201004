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


////////////////////////////////////////////////////////////////////////////////
/**********************************  Headers  *********************************/

#include "tuxRTCAlarm.h"

/* common utils */
#include "..\..\..\common\commonUtils.h"

/* timer functions */
#include "..\code\tuxRealTime.h"
#include "..\code\timersCmdLine.h"


////////////////////////////////////////////////////////////////////////////////
/***************  Constants and Defines Local to This File  *******************/

// This will be the max alarm time we can test while testing Alarm durations
#define MAX_ALARM_TIME_SEC (30 * 60) //30 min

// Args for user specified alarm time tesitng
#define ARG_STR_DATE_AND_TIME (_T("-dateAndTime"))
#define ARG_STR_TIME_FOR_ALARM (_T("-fireAlarmInSec"))

//Buffer to get project name
#define MAX_STRING_NAME 255


////////////////////////////////////////////////////////////////////////////////
/****************************Helper Functions *********************************/

BOOL getRTCAlarmResolution(DWORD *pdwAlarmRes);

BOOL VerifyRTCAlarm(SYSTEMTIME stSetRTCTime, SYSTEMTIME stSetAlarmTime, DWORD dwTimeForAlarm, DWORD dwAlarmRes);

BOOL AddSubSecToFromSystemTime(SYSTEMTIME stTime, DWORD dwSec, BOOL bAdd, SYSTEMTIME *pstNewTime);

BOOL getUserSpecifiedTimeForAlarm (DWORD * pdwTimeForAlarm);

BOOL verifySFONEImage();


////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/

/*******************************************************************************
 *
 *                                Usage Message
 *
 ******************************************************************************/
/*
 * Prints out the usage message for the RTC Alarm tests. It tells the user
 * what the tests do and also specifies the input that the user needs to
 * provide to run the tests.
 */

TESTPROCAPI UsageMessage(
              UINT uMsg,
              TPPARAM tpParam,
              LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info (_T("The RTC Alarm tests should be run on a system that has no interference from"));
    Info (_T("any notification events. It is recommended to run on a minimum kernel image."));
    Info (_T(""));
    Info (_T("The aim of these tests is to test the RTC alarm. Tests check"));
    Info (_T("the alarm resolution, verify that alarm fires correctly after various durations"));
    Info (_T("and fires at all times that fall within RTC supported range."));
    Info (_T(""));
    Info (_T("To test user alarm times, test case 1003 --"));
    Info (_T("1. Use the command line option -dateAndTime <mm/dd/yyyy hh:mm:ss> to test"));
    Info (_T("alarm firing at that time"));
    Info (_T("2. Use the command line option -fireAlarmInSec <seconds> to test alarm firing"));
    Info (_T("after the given duration"));
    Info (_T("3. Use both the above options to set RTC to the given time and have the"));
    Info (_T("alarm fire after the given duration."));
    Info (_T(""));

    return (TPR_PASS);
}

/*****************************************************************************
 *
 *                             Verify RTC Alarm Resolution
 *
 *****************************************************************************/

/*
 * This test prints out the RTC alarm resolution.
 * The user needs to verify if the value is as expected.
 */

TESTPROCAPI
testAlarmResolution(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return  TPR_NOT_HANDLED;
    }
    /* only run if image is non-smartphone */
    if(verifySFONEImage())
    {
        Error (_T("This test should not be run on a smartphone image."));
        Error (_T("Test will now FAIL!"));
        return TPR_FAIL;
    }

    breakIfUserDesires();

    Info (_T("")); //blank line
    Info (_T("This test gets the RTC alarm resolution using IOCTL_KLIB_GETALARMRESOLUTION."));
    Info (_T("The user needs to visually verify if the value is as expected."));
    Info (_T(""));

    DWORD dwAlarmRes = 0;

    if (!getRTCAlarmResolution(&dwAlarmRes))
    {
        // Does this mean that the alarm is not implemented?
        Error (_T("Could not get the alarm resolution."));
    }
    else
    {
        Info (_T("The resolution of RTC Alarm is: %u seconds."), dwAlarmRes);
        returnVal = (dwAlarmRes && dwAlarmRes!=-1)? TPR_PASS : TPR_FAIL;
    }

    Info (_T(""));
    return (returnVal);

}


/*****************************************************************************
 *
 *                             Verify Alarm Fires After Given Duration
 *
 *****************************************************************************/

/*
 * This test verifies that that RTC alarm is able to fire correctly after
 * various time durations.
 */

TESTPROCAPI
testAlarmFiresAfterGivenDuration(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;
    DWORD dwUpBound = 2, dwLoBound = 1;
    DWORD dwRand = 0;
    HANDLE hDST = NULL;
    SYSTEMTIME stSetRTCTime = { 0 }, stSetAlarmTime = { 0 };
    SYSTEMTIME startRange = { 0 }, endRange = { 0 };
    ULONGLONG ullStartRange = 0, ullEndRange = 0;
    ULONGLONG ullAlarmTime = 0, ullAlarmTimeMax = 0, ullRTCTime = 0;


    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    /* only run if image is non-smartphone */
    if(verifySFONEImage())
    {
        Error (_T("This test should not be run on a smartphone image."));
        Error (_T("Test will now FAIL!"));
        return TPR_FAIL;
    }

    breakIfUserDesires();

    Info (_T("")); // blank line
    Info (_T("This test verifies if the RTC Alarm is working and if the alarm is"));
    Info (_T("able to fire after various durations. Since random test durations are used,"));
    Info (_T("the test length will vary each time. The OAL function OEMSetAlarmTime"));
    Info (_T("will be accessed via the SetKernelAlarm API."));
    Info (_T("DST is disabled before this test and enabled again after the test is completed."));
    Info (_T(""));
    
    hDST = disableDST();

    if(NULL == hDST)
    {
        // Maybe the user did not include DST in the image.
        // Let the test continue to run instead of failing it
        Info (_T("WARNING:  Cannot disable DST."));
    }
    
    

    // First get the Alarm Resolution
    DWORD dwAlarmRes = 0;
    if (!getRTCAlarmResolution(&dwAlarmRes))
    {
        // Does this mean that the alarm is not implemented?
        Error (_T("Could not get the alarm resolution."));
        Error (_T("Cannot perform the check whether the alarm fired on time."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }
    else
    {
        Info (_T("The resolution of RTC Alarm is: %u seconds."), dwAlarmRes);
    }

    // Get the RTC supported range

    // First find the range of time supported by the real-time clock
    if(!findRange(&startRange, &endRange))
    {
        Error (_T("Error finding the supported time range."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    // Convert both range start and end times to file times for easy
    // comparison
    if(!SystemTimeToFileTime(&startRange,(FILETIME*)(&ullStartRange)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }
    if(!SystemTimeToFileTime(&endRange,(FILETIME*)(&ullEndRange)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    Info (_T("Now we will pick random time durations(in sec) between 0 and %u"),
                                                                                    MAX_ALARM_TIME_SEC);
    Info (_T("and verify that the alarm fires right after the specified durations."));
    Info (_T(""));

    // Check for zero alarm time
    // Get RTC, set alarm to the same time as RTC
    GetLocalTime(&stSetRTCTime);

    Info (_T("The alarm will be set to fire in 0 seconds."));
    // Now set the time and alarm and verify alarm fires
    if(!VerifyRTCAlarm(stSetRTCTime, stSetRTCTime, 0, dwAlarmRes))
    {
        Error (_T("Test failed. The alarm did not fire when set to fire in 0 seconds."));
        returnVal = TPR_FAIL;
    }

    while(dwUpBound < MAX_ALARM_TIME_SEC)
    {
        dwRand = getRandomInRange(dwLoBound, dwUpBound);
        Info (_T("The alarm will be set to fire in %u seconds."), dwRand);

        // Get RTC, add time for alarm to it to get the alarm time
        GetLocalTime(&stSetRTCTime);
        if(!AddSubSecToFromSystemTime(stSetRTCTime, dwRand, TRUE, &stSetAlarmTime))
        {
            Error (_T("Could not calculate the alarm time."));
            Error (_T("This alarm time can't be tested."));
            returnVal = TPR_FAIL;
            goto NEXT;
        }

        // Verify if the RTC time and AlarmTime + dwAlarmRes time fall within RTC supported range
        if(!SystemTimeToFileTime(&stSetAlarmTime,(FILETIME*)(&ullAlarmTime)))
        {
            Error (_T("Error converting SystemTime to FileTime."));
            Error (_T("This alarm time cannot be tested."));
            returnVal = TPR_FAIL;
            goto NEXT;
        }

        ullAlarmTimeMax = ullAlarmTime + ((dwRand + dwAlarmRes) * 1000 * 1000 * 10);
        if(ullAlarmTimeMax < ullAlarmTime)
        {
            Error (_T("Overflow occured when calculating the maximum alarm time."));
            Error (_T("This alarm time cannot be tested."));
            returnVal = TPR_FAIL;
            goto NEXT;
        }

        if(!SystemTimeToFileTime(&stSetRTCTime,(FILETIME*)(&ullRTCTime)))
        {
            Error (_T("Error converting SystemTime to FileTime."));
            Error (_T("This alarm time cannot be tested."));
            returnVal = TPR_FAIL;
            goto NEXT;
        }

        if((ullRTCTime >= ullStartRange)&&(ullAlarmTimeMax <= ullEndRange))
        {
            // This is a valid time
            // Now set the time and alarm and verify alarm fires
            if(!VerifyRTCAlarm(stSetRTCTime, stSetAlarmTime, dwRand, dwAlarmRes))
            {
                Error (_T("Test failed. The alarm did not fire after %u seconds."),dwRand);
                returnVal = TPR_FAIL;
                goto NEXT;
            }
        }
NEXT:
        dwLoBound = dwUpBound + 1;
        dwUpBound = dwLoBound * 2;
    }

cleanAndReturn:
  
    if(NULL != hDST) 
    {
        //Enable DST again
        if(FALSE == enableDST(hDST))
        {
            Error (_T("Cannot enable DST.  GLE = %d"), GetLastError());
            returnVal = TPR_FAIL;
        }
        
        CloseHandle(hDST);
    }
  
    return (returnVal);

}


/*****************************************************************************
 *
 *                             Verify Alarm Fires At Given Time
 *
 *****************************************************************************/

/*
 * This test verifies that that RTC alarm is able to fire correctly when set to
 * various times.
 */

TESTPROCAPI
testAlarmFiresAtGivenTime(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;
    SYSTEMTIME startRange = { 0 }, endRange = { 0 };
    ULONGLONG ullStartRange = { 0 }, ullEndRange = { 0 };
    HANDLE hDST = NULL;

   /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    /* only run if image is non-smartphone */
    if(verifySFONEImage())
    {
        Error (_T("This test should not be run on a smartphone image."));
        Error (_T("Test will now FAIL!"));
        return TPR_FAIL;
    }

    breakIfUserDesires();

    Info (_T("")); // blank line
    Info (_T("This test verifies if OEMSetAlarmTime works correctly and if the alarm fires"));
    Info (_T("at the time it is set. The OEMSetAlarmTime will be called via SetKernelAlarm API."));
    Info (_T("Only time values within the RTC supported range will be checked."));
    Info (_T("DST is disabled before this test and enabled again after the test is completed."));
    Info (_T(""));

    hDST = disableDST();

    if(NULL == hDST)
    {
        // Maybe the user did not include DST in the image.
        // Let the test continue to run instead of failing it
        Info (_T("WARNING:  Cannot disable DST."));
    }

    // Get RTC before test. We need to reset RTC to this value at the end of the test.
    SYSTEMTIME stCurrentTime;
    GetLocalTime(&stCurrentTime);

    // First get the Alarm Resolution
    DWORD dwAlarmRes = 0;
    if (!getRTCAlarmResolution(&dwAlarmRes))
    {
        // Does this mean that the alarm is not implemented?
        Error (_T("Could not get the alarm resolution."));
        Error (_T("Cannot perform the check whether the alarm fired on time."));
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }
    else
    {
        Info (_T("The resolution of RTC Alarm is: %u seconds."), dwAlarmRes);
    }

    // Get the RTC supported range

    if(!findRange(&startRange, &endRange))
    {
        Error (_T("Error finding the supported time range."));
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    // Convert both range start and end times to file times for easy
    // comparison
    if(!SystemTimeToFileTime(&startRange,(FILETIME*)(&ullStartRange)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    if(!SystemTimeToFileTime(&endRange,(FILETIME*)(&ullEndRange)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }


    // Iterate through all the values in the ALARMTIME_TESTS struct
    // For each, set alarm time to the test time and set RTC to the test time minus
    // 20sec(twice alarm resol)
    // so that we can wait for the alarm to fire in that time.
    // Check if both times are within RTC supported range, else, the alarm will never fire.

    INT rtCount;
    ULONGLONG ullSetRTCTime, ullSetAlarmTime, ullAlarmTimeMax;
    ULONGLONG ullAlarmResFileTimeUnits = dwAlarmRes * 1000 * 1000 * 10;
    SYSTEMTIME stSetRTCTime;

    for(rtCount=0; g_testTimes[rtCount].time.wYear != 0;rtCount++)
    {

        // Get the alarm time
        if(!SystemTimeToFileTime(&g_testTimes[rtCount].time,
                                 (FILETIME*)(&ullSetAlarmTime)))
        {
            Error (_T("Error converting SystemTime to FileTime."));
            Error (_T("This alarm time cannot be tested."));
            returnVal = TPR_FAIL;
            continue;
        }

        // Calculate the max time taken for the alarm to fire
        ullAlarmTimeMax = ullSetAlarmTime + ullAlarmResFileTimeUnits;
        if(ullAlarmTimeMax < ullSetAlarmTime)
        {
            Error (_T("Overflow occured when calculating the maximum alarm time."));
            Error (_T("This alarm time cannot be tested."));
            returnVal = TPR_FAIL;
            continue;
        }

        // Check if alarm time is within supported range
        if((ullSetAlarmTime >= ullStartRange)&&(ullAlarmTimeMax <= ullEndRange))
        {
            // Alarm time is valid.
            // Now get the time to which RTC will be set. Alarm Time minus twice alarm resolution
            if(!AddSubSecToFromSystemTime(g_testTimes[rtCount].time, (2 * dwAlarmRes), FALSE, &stSetRTCTime))
            {
                Error (_T("Could not calculate the time to which RTC must be set."));
                Error (_T("This alarm time can't be tested."));
                returnVal = TPR_FAIL;
                continue;
            }

            // Convert RTC time to FILETIME
            if(!SystemTimeToFileTime(&stSetRTCTime, (FILETIME*)(&ullSetRTCTime)))
            {
                Error (_T("Error converting SystemTime to FileTime."));
                Error (_T("This alarm time cannot be tested."));
                returnVal = TPR_FAIL;
                continue;
            }

            // Check if RTC time is within range
            if((ullSetRTCTime >= ullStartRange))
            {
                // This is also a valid time

                // Now set the time and alarm and verify alarm fires
                Info (_T(" ")); //Blank line
                Info (_T("Description of alarm time: %s"), g_testTimes[rtCount].comment);

                if(!VerifyRTCAlarm(stSetRTCTime, g_testTimes[rtCount].time, (2 * dwAlarmRes), dwAlarmRes))
                {
                    Error (_T("Test failed. The alarm did not fire at the set time."));
                    returnVal = TPR_FAIL;
                }
            }
        }
    }

cleanAndExit:

    Info (_T(""));

    // Set the RTC time back to what it was before the test was run.
    if(!SetLocalTime(&stCurrentTime))
    {
        Error (_T("Could not set the time value back to what it was"));
        Error (_T("before this test was run."));
        returnVal = TPR_FAIL;
    }

    if(NULL != hDST)
    {
        //Enable DST again
        if(FALSE == enableDST(hDST))
        {
            Error (_T("Cannot enable DST.  GLE = %d"), GetLastError());
            returnVal = TPR_FAIL;
        }
        CloseHandle(hDST);
    }

    return (returnVal);
}


/*****************************************************************************
 *
 *                             Verify User Given Alarm Time
 *
 *****************************************************************************/

/*
 * This test verifies user specified alarm time.
 */

TESTPROCAPI
testUserGivenAlarmTime(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;
    SYSTEMTIME stCurrentTime = { 0 };
    SYSTEMTIME stUserTime = { 0 };
    BOOL bUserTime = TRUE;
    HANDLE hDST = NULL;
    DWORD dwTimeForAlarm = 0;
    SYSTEMTIME startRange = { 0 }, endRange = { 0 };
    ULONGLONG ullStartRange = { 0 }, ullEndRange = { 0 };
    DWORD dwAlarmRes = 0;
    ULONGLONG ullSetRTCTime = 0, ullSetAlarmTime = 0, ullAlarmTimeMax = 0;
    ULONGLONG ullAlarmResFileTimeUnits = dwAlarmRes * 1000 * 1000 * 10;
    SYSTEMTIME stSetRTCTime = { 0 }, stSetAlarmTime = { 0 };


    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    /* only run if image is non-smartphone */
    if(verifySFONEImage())
    {
        Error (_T("This test should not be run on a smartphone image."));
        Error (_T("Test will now FAIL!"));
        return TPR_FAIL;
    }

    breakIfUserDesires();
    
    hDST = disableDST();
    if(NULL == hDST)
    {
        // Maybe the user did not include DST in the image.
        // Let the test continue to run instead of failing it
        Info (_T("WARNING:  Cannot disable DST."));
    }
    

    Info (_T("")); // blank line
    Info (_T("This test verifies if Alarm fires at user specified time."));
    Info (_T("The OEMSetAlarmTime will be called via SetKernelAlarm API."));
    Info (_T("Only time values within the RTC supported range will be checked."));
    Info (_T("DST is disabled before this test and enabled again after the test is completed."));
    Info (_T(""));

    // Get RTC before test. We need to reset RTC to this value at the end of the test.
    GetLocalTime(&stCurrentTime);

    // Get the user parameters
    if(!parseUserDateTime(&stUserTime))
    {
        Info (_T("We will use the current time if duration in which to fire the alarm is provided."));
        Info (_T(""));
        bUserTime = FALSE;
    }

    if(!getUserSpecifiedTimeForAlarm(&dwTimeForAlarm))
    {
        Info (_T("Could not get the duration in which to fire the alarm."));
        Info (_T("To specify duration use the command line: -fireAlarmInSec <seconds>"));
        Info (_T("We will use the default value of twice the alarm resolution for duration."));
        dwTimeForAlarm = 0;
    }

    if((!bUserTime) && (!dwTimeForAlarm))
    {
        Error (_T(""));
        Error (_T("User specified neither time nor duration for alarm. Skipping the test."));
        Error (_T("Set at least one of -dateAndTime <mm/dd/yyyy hh:mm:ss>"));
        Error (_T("or -fireAlarmInSec <sec> to test the alarm."));
        returnVal = TPR_SKIP;
        goto cleanAndReturn;
    }

    // First get the Alarm Resolution
    if (!getRTCAlarmResolution(&dwAlarmRes))
    {
        // Does this mean that the alarm is not implemented?
        Error (_T("Could not get the alarm resolution."));
        Error (_T("Cannot perform the check whether the alarm fired on time."));
        goto cleanAndReturn;
    }
    else
    {
        Info (_T("The resolution of RTC Alarm is: %u seconds."), dwAlarmRes);
    }

    // Get the RTC supported range

    if(!findRange(&startRange, &endRange))
    {
        Error (_T("Error finding the supported time range."));
        goto cleanAndReturn;
    }

    // Convert both range start and end times to file times for easy
    // comparison
    if(!SystemTimeToFileTime(&startRange,(FILETIME*)(&ullStartRange)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        goto cleanAndReturn;
    }

    if(!SystemTimeToFileTime(&endRange,(FILETIME*)(&ullEndRange)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        goto cleanAndReturn;
    }

    // If the user specifies only the test time, set RTC to user test time minus twice alarm resol
    // and set Alarm to the user specified time.
    // If the user only specifies the duration in which to fire alarm, set RTC to current time and
    // set Alarm to current time plus the user specified duration
    // If the user specifies both test time and duration to fire, then set RTC to user
    // specified time and set Alarm to user time plus duration.


    // Compute the RTC time and the Alarm time
    if(bUserTime)
    {
        if(dwTimeForAlarm)
        {
            stSetRTCTime = stUserTime;
            // stSetAlarmTime = stUserTime + dwTimeForAlarm;
            if(!AddSubSecToFromSystemTime(stUserTime, dwTimeForAlarm, TRUE, &stSetAlarmTime))
            {
                Error (_T("Could not calculate the alarm time. Exiting..."));
                Error (_T("Try again with a different alarm time."));
                goto cleanAndReturn;
            }
        }
        else
        {
            // stSetRTCTime = stUserTime - twiceAlarmRes;
            if(!AddSubSecToFromSystemTime(stUserTime, (2 * dwAlarmRes), FALSE, &stSetRTCTime))
            {
                Error (_T("Could not calculate the time to which RTC must be set. Exiting..."));
                Error (_T("Try again with a different alarm time."));
                goto cleanAndReturn;
            }
            stSetAlarmTime = stUserTime;
            dwTimeForAlarm = 2 * dwAlarmRes;
        }
    }
    else
    {
        stSetRTCTime = stCurrentTime;
        // stSetAlarmTime = stCurrentTime + dwTimeForAlarm;
        if(!AddSubSecToFromSystemTime(stCurrentTime, dwTimeForAlarm, TRUE, &stSetAlarmTime))
        {
            Error (_T("Could not calculate the alarm time. Exiting..."));
            Error (_T("Try again with a different alarm time."));
            goto cleanAndReturn;
        }
    }

    // Convert times to Filetimes
    if(!SystemTimeToFileTime(&stSetRTCTime, (FILETIME*)(&ullSetRTCTime)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        goto cleanAndReturn;
    }
    if(!SystemTimeToFileTime(&stSetAlarmTime, (FILETIME*)(&ullSetAlarmTime)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        goto cleanAndReturn;
    }

    // Calculate the max time taken for the alarm to fire
    ullAlarmTimeMax = ullSetAlarmTime + ullAlarmResFileTimeUnits;

    // Check if the RTC time and the alarm times are within supported range
    if((ullSetRTCTime >= ullStartRange)&&(ullAlarmTimeMax <= ullEndRange))
    {
        // Both times are valid.
        // Now set the time and alarm and verify alarm fires
        if(!VerifyRTCAlarm(stSetRTCTime, stSetAlarmTime, dwTimeForAlarm, dwAlarmRes))
        {
            Error (_T("Test failed. The alarm did not fire at the set time."));
            goto cleanAndReturn;
        }
    }
    else
    {
        Error (_T("Either the time to which the RTC has to be set or "));
        Error (_T("the time to which alarm has to be set plus alarm resolution"));
        Error (_T("or both of these are out of the RTC supported range."));
        Error (_T("Therefore, we cannot accurately test the alarm."));
        goto cleanAndReturn;
    }

    returnVal = TPR_PASS;


cleanAndReturn:

    Info (_T(""));

    // Set the RTC time back to what it was before the test was run.
    if(!SetLocalTime(&stCurrentTime))
    {
        Error (_T("Could not set the time value back to what it was"));
        Error (_T("before this test was run."));
        returnVal = TPR_FAIL;
    }
    
    if(NULL != hDST)
    {
        //Enable DST again
        if(FALSE == enableDST(hDST))
        {
            Error (_T("Cannot enable DST.  GLE = %d"), GetLastError());
            returnVal = TPR_FAIL;
        }
        CloseHandle(hDST);
    }
    

    return (returnVal);
}


/*******************************************************************************
 *                             Verify RTC alarm fires at given time
 ******************************************************************************/

// This function verifies if the RTC Alarm fires at the expected time
//
// Arguments:
//      Input: Time to set RTC to
//                    Alarm time
//                    Duration to wait for alarm
//                    Alarm resolution
//      Output: None
//
// Return value:
//      TRUE if successful, FALSE otherwise

BOOL VerifyRTCAlarm(SYSTEMTIME stSetRTCTime, SYSTEMTIME stSetAlarmTime, DWORD dwTimeForAlarm, DWORD dwAlarmRes)
{
    // Assume false until proven otherwise
    BOOL rVal = FALSE;
    BOOL bAlarmFired = FALSE;

    SYSTEMTIME stRTCBeforeTest = { 0 };
    HANDLE hEvent = NULL;

    TCHAR printBuffer[PRINT_BUFFER_SIZE] = { 0 };
    TCHAR printBuffer2[PRINT_BUFFER_SIZE] = { 0 };

    Info (_T("")); // blank line

    // We will have to change the value of RTC to test this alarm time.
    // Therefore, we will store the current RTC value and restore it at the end of the test.
    GetLocalTime(&stRTCBeforeTest);

    // Create an auto-reset event
    hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if(!hEvent)
    {
        Error (_T("Could not create the auto-reset event."));
        goto cleanAndReturn;
    }

    // Set RTC to the given value
    printTimeToBuffer(&stSetRTCTime,printBuffer,PRINT_BUFFER_SIZE);
    Info (_T("RTC will be set to this value = %s"), printBuffer);

    if(!SetLocalTime(&stSetRTCTime))
    {
        Error (_T("Could not set the RTC."));
        goto cleanAndReturn;
    }

    // Set RTC Alarm to this value
    printTimeToBuffer(&stSetAlarmTime,printBuffer,PRINT_BUFFER_SIZE);
    Info (_T("Alarm will be set to this value = %s"), printBuffer);

    // Call SetKernelAlarm API with the time you want the alarm to fire and the event
    if(!SetKernelAlarm( hEvent, &stSetAlarmTime))
    {
        Error (_T("SetKernelAlarm failed with GetLastError = %u"),GetLastError());
        goto cleanAndReturn;
    }

    // Wait for the event
    Info (_T("Alarm is set to fire in %u seconds.  Waiting..."), dwTimeForAlarm);

    // We will wait for the Alarm Time plus 10s + 2x the resolution.
    DWORD dwWaitTimeMS = (dwTimeForAlarm + 10) * 1000 + (dwAlarmRes*2);
    DWORD dwReason = WaitForSingleObject( hEvent, dwWaitTimeMS);
    switch (dwReason) {
        case WAIT_OBJECT_0:
            Info (_T("Alarm was fired SUCCESSFULLY!"));
            bAlarmFired = TRUE;
            break;
        case WAIT_TIMEOUT:
            Error (_T("Timeout waiting for alarm. Waited %dms"), dwWaitTimeMS);
            break;
        default:
            Error (_T("WaitForSingleObject() returned 0x%x"), dwReason);
            break;
    }

    // Get RTC again
    SYSTEMTIME stAfterAlarm;
    GetLocalTime( &stAfterAlarm );

    printTimeToBuffer(&stAfterAlarm,printBuffer,PRINT_BUFFER_SIZE);
    Info (_T("Current RTC Time = %s"), printBuffer);

    // If the alarm was fired, verify that it was fired within the bounds of RTC alarm resolution.
    if(bAlarmFired)
    {
        // If Alarm Time is <= RTC alarm resolution, should fire immediately at stSetRTCTime.
        // If Alarm Time is > RTC alarm resolution, can fire between stAlarmSetTime-10sec and stAlarmSetTime+10 sec.

        // Convert all to FILETIME units for computations
        ULONGLONG ullSetRTCTime = 0, ullSetAlarmTime = 0, ullAfterAlarm = 0;

        // Most platforms don't set the millisecond value when setting the RTC or the RTC alarm.

        // Set the millisecond values in stSetRTCTime and stSetAlarmTime to zero so that
        // we make the correct comparisons.
        stSetRTCTime.wMilliseconds = 0;
        stSetAlarmTime.wMilliseconds = 0;

        if(!SystemTimeToFileTime(&stSetRTCTime, (FILETIME*)(&ullSetRTCTime)))
        {
            Error (_T("Error converting SystemTime to FileTime."));
            goto cleanAndReturn;
        }
        if(!SystemTimeToFileTime(&stSetAlarmTime, (FILETIME*)(&ullSetAlarmTime)))
        {
            Error (_T("Error converting SystemTime to FileTime."));
            goto cleanAndReturn;
        }
        if(!SystemTimeToFileTime(&stAfterAlarm, (FILETIME*)(&ullAfterAlarm)))
        {
            Error (_T("Error converting SystemTime to FileTime."));
            goto cleanAndReturn;
        }

        // Determine the bounds using the RTC Alarm resolution
        ULONGLONG ullAlarmTimeMin = 0, ullAlarmTimeMax = 0;
        ULONGLONG ullAlarmResFileTimeUnits = dwAlarmRes * 1000 * 1000 * 10;

        // If within resolution, alarm fires immediately
        if(dwTimeForAlarm <= dwAlarmRes)
        {
            ullAlarmTimeMin = ullSetRTCTime;
            ullAlarmTimeMax = ullSetRTCTime + ullAlarmResFileTimeUnits;
        }
        else
        {
            ullAlarmTimeMin = ullSetAlarmTime - ullAlarmResFileTimeUnits;
            ullAlarmTimeMax = ullSetAlarmTime + ullAlarmResFileTimeUnits;
        }

        // Now verify if the time at which the alarm was fired is within the bounds.
        if((ullAfterAlarm < ullAlarmTimeMin) || (ullAfterAlarm > ullAlarmTimeMax))
        {
            Error (_T("The Alarm was not fired at the right time or within the bounds of RTC Alarm resolution."));

            SYSTEMTIME stAlarmTimeMin = { 0 }, stAlarmTimeMax = { 0 };
            if(!FileTimeToSystemTime((FILETIME*)(&ullAlarmTimeMin), &stAlarmTimeMin))
            {
                Error (_T("Error converting FileTime to SystemTime."));
                goto cleanAndReturn;
            }
            if(!FileTimeToSystemTime((FILETIME*)(&ullAlarmTimeMax), &stAlarmTimeMax))
            {
                Error (_T("Error converting FileTime to SystemTime."));
                goto cleanAndReturn;
            }

            printTimeToBuffer(&stAlarmTimeMin,printBuffer,PRINT_BUFFER_SIZE);
            printTimeToBuffer(&stAlarmTimeMax,printBuffer2,PRINT_BUFFER_SIZE);
            Error (_T("The alarm was expected to be fired in the bounds of:"));
            Error (_T("%s and %s"), printBuffer, printBuffer2);

            goto cleanAndReturn;
        }
    }

    rVal = bAlarmFired;

cleanAndReturn:

    Info (_T("")); // blank line

    // Set the RTC time back to what it was before the test was run.
    if(!SetLocalTime(&stRTCBeforeTest))
    {
        Error (_T("Could not set the time value back to what it was"));
        Error (_T("before this test was run."));
        rVal = FALSE;
    }

    if (hEvent)
    {
        CloseHandle( hEvent );
    }

    return rVal;
}

/*******************************************************************************
 *                             Add or Subtract Seconds from System time
 ******************************************************************************/

// This function adds or subtracts seconds from the system time.
//
// Arguments:
//      Input: System time from the seconds are to be added/subtracted
//                     Seconds to be added/subtracted
//                     Boolean variable to tell whether to add(TRUE) or subtract(FALSE)
//      Output: The resultant Sytem time after adding/subtracting seconds
//
// Return value:
//      TRUE if successful, FALSE otherwise

BOOL AddSubSecToFromSystemTime(SYSTEMTIME stTime, DWORD dwSec, BOOL bAdd, SYSTEMTIME *pstNewTime)
{
    BOOL returnVal = FALSE;

    //Check input
    if(!pstNewTime)
    {
        goto cleanAndReturn;
    }

    // Convert the time to Filetime for the computations.
    ULONGLONG ullTime = 0, ullNewTime = 0;
    if(!SystemTimeToFileTime(&stTime, (FILETIME*)(&ullTime)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        goto cleanAndReturn;
    }

    // Now add or subtract the seconds
    ULONGLONG ullSec = dwSec * 1000 * 1000 * 10;
    if(bAdd)
    {
        ullNewTime = ullTime + ullSec;
        // Check for overflow
        if(ullNewTime < ullTime)
        {
            Error (_T("Overflow occured while calculating time."));
            goto cleanAndReturn;
        }
    }
    else
    {
        ullNewTime = ullTime - ullSec;
        // Check for overflow
        if(ullNewTime > ullTime)
        {
            Error (_T("Overflow occured while calculating time."));
            goto cleanAndReturn;
        }
    }

    // Convert back to SYSTEMTIME
    if(!FileTimeToSystemTime((FILETIME*)(&ullNewTime), pstNewTime))
    {
        Error (_T("Error converting FileTime to SystemTime."));
        goto cleanAndReturn;
    }

    returnVal = TRUE;

cleanAndReturn:

    return returnVal;
}


/*******************************************************************************
 *                             Get RTC Alarm Resolution
 ******************************************************************************/
// This function gets the RTC Alarm Resolution by calling the Kernel IOCTL
//
// Arguments:
//      Input: None
//      Output: Alarm resolution
//
// Return value:
//      TRUE if resolution found, FALSE otherwise

BOOL getRTCAlarmResolution(DWORD *pdwAlarmRes)
{
    BOOL returnVal = FALSE;
    DWORD dwAlarmResol = 0;
    DWORD dwBytesRet = 0;


    if(!pdwAlarmRes)
    {
        goto cleanAndReturn;
    }

    if (!KernelLibIoControl((HANDLE)KMOD_CORE, IOCTL_KLIB_GETALARMRESOLUTION, NULL, 0, &dwAlarmResol, sizeof(dwAlarmResol), &dwBytesRet))
    {
        // Does this mean that the alarm is not implemented?
        Error (_T("Calling KernelLibIoControl with IOCTL_KLIB_GETALARMRESOLUTION returned FALSE."));
        goto cleanAndReturn;
    }

    if(!dwAlarmResol)
    {
        Error (_T("The returned alarm resolution is zero. Something wrong."));
        goto cleanAndReturn;
    }
    if(dwAlarmResol < MIN_NKALARMRESOLUTION_MSEC || dwAlarmResol > MAX_NKALARMRESOLUTION_MSEC ) 
    {
        Error (_T("The alarm resolution reported by IOCTL_KLIB_GETALARMRESOLUTION: %dms is outside of the expected resolution bounds"), dwAlarmResol );
        Error (_T("Expect to be within (%d, %d) [in ms]"), MIN_NKALARMRESOLUTION_MSEC, MAX_NKALARMRESOLUTION_MSEC );
        goto cleanAndReturn;
    }


    // The kernel always ignores millisec when setting the alarm. So, can we
    // assume that alarm resolution will never be less than a sec?
    *pdwAlarmRes = dwAlarmResol/1000;

    returnVal = TRUE;

cleanAndReturn:

    return returnVal;
}


/*******************************************************************************
 *                        Parse user's time for Alarm
 ******************************************************************************/

// This function parses the command line arguments and returns the
// user supplied duration to fire the alarm.
//
// Arguments:
//  Input: None
//      Output: The time parsed from the command line
//
// Return value:
//      TRUE if time was parsed correctly, FALSE otherwise

BOOL getUserSpecifiedTimeForAlarm (DWORD * pdwTimeForAlarm)
{
    cTokenControl tokens;

    BOOL returnVal = FALSE;

    // Check the input
    if (!pdwTimeForAlarm)
    {
        return FALSE;
    }

    DWORD dwUserTime = 0;

    // If no command line specified or if can't break command line into tokens, exit
    if(!(g_szDllCmdLine && tokenize((TCHAR*)g_szDllCmdLine, &tokens)))
    {
        Error (_T("Couldn't parse the command line."));
        goto cleanAndExit;
    }

    // Check if the option we are looking for is present and get it
    if (!getOptionDWord(&tokens, ARG_STR_TIME_FOR_ALARM, &dwUserTime))
    {
        goto cleanAndExit;
    }

    *pdwTimeForAlarm = dwUserTime;
    Info (_T("The user specified time for Alarm is %u seconds."), *pdwTimeForAlarm);

    returnVal = TRUE;

cleanAndExit:

    return returnVal;
}

/*******************************************************************************
 *                        Verify that the image is not a smartphone image
 ******************************************************************************/

// This function verifies that the image on which the test is being run is not
// a smartphone image
//
// Arguments:
//  Input: None
//  Output: boolean indicating whether non-smartphone image or otherwise
//
// Return value:
//      TRUE if image is smartphone, FALSE otherwise

BOOL verifySFONEImage()
{
    wchar_t projectName[MAX_STRING_NAME];
    int projectRet = SystemParametersInfo(
        SPI_GETPROJECTNAME, 
        MAX_STRING_NAME,
        (PVOID *)projectName,
        0
        );
     
    if(projectRet == 0)
    {
        Error (_T("Unable to get the project name. Error code is %d"), GetLastError());
        return TRUE;
    }

    _wcsupr_s(projectName, MAX_STRING_NAME);

    if(wcsnicmp(projectName,L"SMARTPHONE",MAX_STRING_NAME) == 0)
        return TRUE;
    else
        return FALSE;
}