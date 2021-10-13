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

#include "tuxtimers.h"

#include "tuxRealTime.h"

#include <service.h>

////////////////////////////////////////////////////////////////////////////////
/***************  Constants and Defines Local to This File  *******************/

TCHAR * DaysOfWeek[] = {_T("Sun"),_T("Mon"),_T("Tues"),_T("Wed"),_T("Thurs"),
_T("Fri"),_T("Sat")};


// This structure contains various times used to test the Set and Get Real Time
// functions.
stRealTimeTests g_testTimes[] = {
    { {2005, 6,TUES,14,   20,27,30,200}, _T("Random time") },
    { {2004, 2,SUN,29,   19,27,30,200}, _T("Leap year") },
    { {2009,12,SAT,31,   23,59,59,999}, _T("Year end") },
    { {2009, 1,SAT, 1,       0, 0, 0, 0}, _T("Year begin") },
    { {2009, 6,TUES,14,    0, 0, 0, 0}, _T("Hour begin") },
    { {2009, 6,TUES,14,   23,59,59,999}, _T("Hour end") },
    { {2009, 6,TUES,14,   20, 0, 0, 0}, _T("Minute begin") },
    { {2009, 6,TUES,14,   10, 59,59,999}, _T("Minute end") },
    { {2009, 6,TUES,14,    9,27, 0, 0}, _T("Second begin") },
    { {2009, 6,TUES,14,    5,27,59,999}, _T("Second end") },
    { {2009, 6,TUES,14,    6,27,30,  0}, _T("Millisec begin") },
    { {2009, 6,TUES,14,   11,27,30,999}, _T("Millisec end") },
    { {1970, 1,THURS, 1,    0, 0, 0,  0}, _T("Random time") },
    { {2038, 1,TUES,19,    3,14, 8,  0}, _T("Random time") },
    { {2038, 1,WED,20,    3,14, 8,  0}, _T("Random time") },
    { {1904, 1,FRI, 1,    0, 0, 0,  0}, _T("Time epoch") },
    { {1858,11,WED,17,    0, 0, 0,  0}, _T("Time epoch") },

    { {1611,12,SAT,31,   11,11,11, 11}, _T("Random time") },
    { {1855, 6,THURS,14,   10,27,30,555}, _T("Random time") },
    { {2000, 6,WED,14,   10,27,30,200}, _T("Random time") },
    { {2006, 8,WED,30,   13,37,53,367}, _T("Random time") },
    { {2008, 4,WED,23,   17,29,49,890}, _T("Random time") },
    { {2010, 1,FRI, 1,    1, 1, 1,  1}, _T("Random time") },
    { {2256, 6,SAT,14,   10,27,30,200}, _T("Random time") },
    { {2890,12,SUN,31,   23,59,59,999}, _T("Random time") },
    { {3005, 6,FRI,14,   10,27,30,200}, _T("Random time") },

 // more between 1970 to 2070
    { {1968, 8,FRI,23,   13,23,53,367}, _T("Random time") },
    { {1972, 3,WED, 8,    3,17,20,659}, _T("Random time") },
    { {1993, 5,WED,12,    7, 2,41,234}, _T("Random time") },
    { {1999,12,FRI,31,   23,59,59,999}, _T("Random time") },
    { {2000, 1,SAT, 1,    0, 0, 0,  0}, _T("Random time") },
    { {2012, 8,TUES,14,   21, 9,33,987}, _T("Random time") },
    { {2029,12,MON, 3,   22,56,56,567}, _T("Random time") },
    { {2035, 9,WED,19,   17,33,17,111}, _T("Random time") },
    { {2047,11,TUES,26,   11,41,23,  4}, _T("Random time") },
    { {2068, 4,MON,30,   10,20,58, 34}, _T("Random time") },

 // more additional tests
    { {2005,12,SAT,31,   10,27,30,200}, _T("Random time") },
    { {2005, 1,SAT, 1,   10,27,30,200}, _T("Change date and not time") },
    { {2007, 1,MON, 1,   10,27,30,200}, _T("Change date and not time") },
    { {2007,12,MON,31,   10,27,30,200}, _T("Change date and not time") },
    { {2005, 1,TUES,11,   10,27,30,200}, _T("Change date and not time") },
    { {2026, 6,THURS,11,   10,27,30,200}, _T("Change date and not time") },
    { {2025,12,SUN,28,   10,27,30,200}, _T("Change date and not time") },
    { {2025, 1,WED, 1,   10,27,30,200}, _T("Change date and not time") },
    { {2005, 1,SAT, 1,   20,27,30,200}, _T("Change time and not date") },
    { {2005, 1,SAT, 1,   20,27,30,  2}, _T("Change time and not date") },
    { {2005, 1,SAT, 1,   23,59,59,999}, _T("Change time and not date") },
    { {2005, 1,SAT, 1,    0, 0, 0,  0}, _T("Change time and not date") },
    { {2005, 1,SAT, 1,   20,10,25,868}, _T("Change time and not date") },
    { {0, 0, 0, 0, 0, 0, 0, 0}, _T("") },  // marks end of list
};


// This structure contains various times used to test the rollover in case of
// seconds, minutes, hours, etc.
// To verify rollover, the given parameter should reach its max value and
// rollover to start from its least value again; and there should be an
// increment in its adjacent higher parameter. For ex: seconds should
// rollover from 59 to 0, with an increment in the minute value.

stRolloverTests g_rolloverTests[] = {
    { {2008, 6,THURS,16,  14,31,57,0}, _T("Checking rollover for Second"),
                        {2008, 6,THURS,16, 14,32, 2,0} },
    { {2009, 6,THURS,16,  14,59,57,0}, _T("Checking rollover for Minute"),
                        {2009, 6,THURS,16, 15, 0, 2,0} },
    { {2008, 6,THURS,16,  23,59,57,0}, _T("Checking rollover for Hour"),
                        {2008, 6,FRI,17, 0, 0, 2,0} },
    { {2009, 6,THURS,30,  23,59,57,0}, _T("Checking rollover for Day"),
                        {2009, 7,FRI, 1, 0, 0, 2,0} },
    { {2008,12,SAT,31,  23,59,57,0}, _T("Checking rollover for Month"),
                        {2009, 1,SUN, 1, 0, 0, 2,0} },

    // is leap year
    { {2000,2,MON,28,  23,59,57,0}, _T("Checking rollover for Leap Year (400 multiple)"),
                        {2000, 2,TUES, 29, 0, 0, 2,0} },
    { {2000,2,TUES,29,  23,59,57,0}, _T("Checking rollover for Leap Year (400 multiple)"),
                        {2000, 3,WED, 1, 0, 0, 2,0} },
    { {2400,2,MON,28,  23,59,57,0}, _T("Checking rollover for Leap Year (400 multiple)"),
                        {2400, 2,TUES, 29, 0, 0, 2,0} },
    { {2400,2,TUES,29,  23,59,57,0}, _T("Checking rollover for Leap Year (400 multiple)"),
                        {2400, 3,WED, 1, 0, 0, 2,0} },

    { {2004,2,SAT,28,  23,59,57,0}, _T("Checking rollover for Leap Year (4 multiple)"),
                        {2004, 2,SUN, 29, 0, 0, 2,0} },
    { {2004,2,SUN,29,  23,59,57,0}, _T("Checking rollover for Leap Year (4 multiple)"),
                        {2004, 3,MON, 1, 0, 0, 2,0} },

    { {2008,2,THURS,28,  23,59,57,0}, _T("Checking rollover for Leap Year (4 multiple)"),
                        {2008, 2,FRI, 29, 0, 0, 2,0} },
    { {2008,2,FRI,29,  23,59,57,0}, _T("Checking rollover for Leap Year (4 multiple)"),
                        {2008, 3,SAT, 1, 0, 0, 2,0} },

    { {2012,2,TUES,28,  23,59,57,0}, _T("Checking rollover for Leap Year (4 multiple)"),
                        {2012, 2,WED, 29, 0, 0, 2,0} },
    { {2012,2,WED,29,  23,59,57,0}, _T("Checking rollover for Leap Year (4 multiple)"),
                        {2012, 3,THURS, 1, 0, 0, 2,0} },

    { {2016,2,SUN,28,  23,59,57,0}, _T("Checking rollover for Leap Year (4 multiple)"),
                        {2016, 2,MON, 29, 0, 0, 2,0} },
    { {2016,2,MON,29,  23,59,57,0}, _T("Checking rollover for Leap Year (4 multiple)"),
                        {2016, 3,TUES, 1, 0, 0, 2,0} },

    // is NOT leap year
    { {2100,2,SUN,28,  23,59,57,0}, _T("Checking rollover for Leap Year (100 multiple)"),
                        {2100, 3,MON, 1, 0, 0, 2,0} },
    { {2200,2,FRI,28,  23,59,57,0}, _T("Checking rollover for Leap Year (100 multiple)"),
                        {2200, 3,SAT, 1, 0, 0, 2,0} },
    { {2003,2,FRI,28,  23,59,57,0}, _T("Checking rollover for Leap Year (non-leap)"),
                        {2003, 3,SAT, 1, 0, 0, 2,0} },
    { {2010,2,SUN,28,  23,59,57,0}, _T("Checking rollover for Leap Year (non-leap)"),
                        {2010, 3,MON, 1, 0, 0, 2,0} },

    { {2010,3,SUN,14,   1,59,57,0}, _T("Checking during expected DST update"),
                        {2010, 3,SUN,14, 2, 0, 2,0} },
    { {1999,12,FRI,31,  23,59,57,0}, _T("Checking rollover for Y2K"),
                        {2000, 1,SAT, 1, 0, 0, 2,0} },
    { {1904, 1,FRI, 1,   0, 0, 0,0}, _T("Checking rollover for time epoch"),
                        {1904, 1,FRI, 1, 0, 0, 5,0} },
    { {1858,11,WED,17,   0, 0, 0,0}, _T("Checking rollover for time epoch"),
                        {1858,11,WED,17, 0, 0, 5,0} },
    { {1970, 1,THURS, 1,   0, 0, 0,0}, _T("Checking rollover for time epoch"),
                        {1970, 1,THURS, 1,   0, 0, 5,0} },
    { {2038, 1,TUES, 19,  3,14, 7,0}, _T("Checking rollover for time epoch"),
                        {2038, 1,TUES, 19,  3,14,12,0} }
};


////////////////////////////////////////////////////////////////////////////////
/****************************Helper Functions *********************************/

// Compares the two given times to check if they are equal
BOOL IsEqual(LPSYSTEMTIME lpst1, LPSYSTEMTIME lpst2);

// Compares the two given times to check if the time After is within 10 sec of time Before
BOOL IsWithinTenSec(LPSYSTEMTIME lpstBefore, LPSYSTEMTIME lpstAfter );

// Tests the Get/Set time functions when valid(within the range) time is given
BOOL testValidTime (LPSYSTEMTIME lpst);

// Tests the Get/Set time functions when invalid(out of range) time is given
BOOL testInvalidTime (LPSYSTEMTIME lpst);


////////////////////////////////////////////////////////////////////////////////
/********************************* TEST PROCS *********************************/


/*******************************************************************************
 *
 *                             Range of years supported
 *
 ******************************************************************************/

/*
 * Finds the range of time supported by the OAL. It verifies that it is valid
 * and is at least 100 years.
 */

TESTPROCAPI testRealTimeRange(
              UINT uMsg,
              TPPARAM tpParam,
              LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;
    SYSTEMTIME begin = { 0 };
    SYSTEMTIME end = { 0 };
    ULONGLONG ullBegin = 0, ullEnd = 0;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    breakIfUserDesires();

    Info (_T(""));
    Info (_T("This test finds the range of time supported by the OAL."));
    Info (_T("It verifies that it is valid and is at least 100 years."));

    // Call function to find the range
    if(!findRange(&begin,&end))
    {
        Error (_T("Error finding the supported time range."));
        goto cleanAndExit;
    }


    // Check if the present date lies within the range???


    // Verify that the supported range is >= 100 years
    if(!SystemTimeToFileTime(&begin, (FILETIME*)(&ullBegin)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        goto cleanAndExit;
    }

    if(!SystemTimeToFileTime(&end, (FILETIME*)(&ullEnd)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        goto cleanAndExit;
    }

    ULONGLONG hundredYears = (100ull * 365ull * 24ull *60ull * 60ull * 1000ull *
                                                               1000ull * 10ull);

    if((ullEnd - ullBegin) < hundredYears)
    {
        Error (_T("The supported range is less than 100 years"));
        Error (_T(""));
        Error (_T("The test failed"));
        Error (_T(""));
        goto cleanAndExit;
    }

    returnVal = TPR_PASS;

cleanAndExit:

    Info (_T(""));
    if(returnVal == TPR_PASS)
    {
        Info (_T("The test passed."));
    }
    else
    {
        Error (_T("The test failed."));
    }
    Info (_T(""));

    return (returnVal);
}



/*******************************************************************************
 *
 *                             Set and Get RealTime
 *
 ******************************************************************************/

/*
 * This function tests the OEMSetRealTime and OEMGetRealTime functions. These
 * functions are accessed using the SetLocalTime and GetLocalTime functions.
 * The test sets various valid(within supported range) and invalid times(out of
 * range) and verifies that only the valid ones are set.
 * It also tests any user supplied date and time value. The get time function
 * is called after set time to check if the value is actually set. This also
 * verifies if get time is working correctly.
 *
 */

TESTPROCAPI testGetAndSetRealTime(
              UINT uMsg,
              TPPARAM tpParam,
              LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;
    INT rtCount = 0;
    SYSTEMTIME getTime = { 0 };
    SYSTEMTIME startRange = { 0 }, endRange = { 0 };
    SYSTEMTIME userTime = { 0 };
    ULONGLONG ullStartRange = 0, ullEndRange = 0, ullRTTime = 0;
    HANDLE hDST = NULL;

    TCHAR printBuffer[PRINT_BUFFER_SIZE] = { 0 };

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    breakIfUserDesires();

    // Get the current time. The RTC will be set back to this value after
    // the test is run.
    SYSTEMTIME currentTime = { 0 };
    GetLocalTime(&currentTime);


    Info (_T(""));
    Info (_T("This verifies the OEMSetRealTime and OEMGetRealTime functions"));
    Info (_T("in the OAL. These functions are accessed using SetLocalTime and"));
    Info (_T("GetLocalTime functions. Various valid times (within the supported"));
    Info (_T("time range) and invalid times (out of supported range) are set "));
    Info (_T("and verified that only the valid ones are set."));
    Info (_T("Also any user supplied date and time values are tested."));
    Info (_T("DST is disabled before this test and enabled again after the test is completed."));

    
    hDST = disableDST();

    if(NULL == hDST)
    {
        // Maybe the user did not include DST in the image.
        // Let the test continue to run instead of failing it
        Info (_T("WARNING:  Cannot disable DST."));
    }

    // First find the range of time supported by the real-time clock
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
        printTimeToBuffer(&startRange,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    if(!SystemTimeToFileTime(&endRange,(FILETIME*)(&ullEndRange)))
    {
        printTimeToBuffer(&endRange,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }


    //Testing GetLocalTime - get the time and set it again
    GetLocalTime(&getTime);
    if(!SetLocalTime(&getTime))
    {
       Error (_T("Test Failed - getting the time and setting it again is"));
       Error (_T("returning false. Either OEMGetRealTime returned an invalid"));
       Error (_T("value or OEMSetRealTime is not working correctly."));
       returnVal = TPR_FAIL;
    }


    // Iterate through all the values in the REALTIME_TESTS struct
    for(rtCount=0; g_testTimes[rtCount].time.wYear != 0;rtCount++)
    {
        Info (_T(" ")); //Blank line
        // Convert to file time to check if it is within the range
        if(!SystemTimeToFileTime(&g_testTimes[rtCount].time,
                                 (FILETIME*)(&ullRTTime)))
        {
            printTimeToBuffer(&g_testTimes[rtCount].time,printBuffer,PRINT_BUFFER_SIZE);
            Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
            returnVal = TPR_FAIL;
            goto cleanAndExit;
        }

        printTimeToBuffer(&g_testTimes[rtCount].time,printBuffer,PRINT_BUFFER_SIZE);
        Info (_T("OEMSetRealTime is given this time: %s"),printBuffer);
        Info (_T("Description : %s"), g_testTimes[rtCount].comment);

        // Check if it is a valid time, i.e., it falls within the range
        // Then set it and verify that it is set correctly by reading it again,
        // else we set it and verify that it is not set by reading it again.
        if((ullRTTime>=ullStartRange)&&(ullRTTime<=ullEndRange))
        {
            if(testValidTime(&g_testTimes[rtCount].time))
            {
                Info (_T("Time returned matches the time set. Test passed."));
            }
            else
            {
                Error (_T("Test failed."));
                returnVal = TPR_FAIL;
            }
        }
        else
        {
            if(testInvalidTime(&g_testTimes[rtCount].time))
            {
                Info (_T("The given time was not set. Test passed."));
            }
            else
            {
                Error (_T("Test failed."));
                returnVal = TPR_FAIL;
            }
        }
    }

    // Now get the date and time value supplied by the user

    if(parseUserDateTime(&userTime))
    {
        // Now test the parsed value from the command line
        // Convert to file time to check if it is within the range
        if(!SystemTimeToFileTime(&userTime,(FILETIME*)(&ullRTTime)))
        {
            printTimeToBuffer(&userTime,printBuffer,PRINT_BUFFER_SIZE);
            Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
            goto cleanAndExit;
        }

        Info (_T("OEMSetRealTime is given the following value supplied by the user:"));
        printSystemTime(&userTime);

        // Check if it is a valid time, i.e., it falls within the range
        // Then set it and verify that it is set correctly by reading it again,
        // else we set it and verify that it is not set by reading it again.
        if((ullRTTime>=ullStartRange)&&(ullRTTime<=ullEndRange))
        {
            if(testValidTime(&userTime))
            {
                Info (_T("This is a valid time and the test passed."));
            }
            else
            {
                Info (_T("This is a valid time and the test failed."));
                returnVal = TPR_FAIL;
            }
        }
        else
        {
            if(testInvalidTime(&userTime))
            {
                Info (_T("This is an invalid time and the test passed."));
            }
            else
            {
                Info (_T("This is an invalid time and the test failed."));
                returnVal = TPR_FAIL;
            }
        }
    }

cleanAndExit:

    // Set the RTC time back to what it was before the test was run.
    if(!SetLocalTime(&currentTime))
    {
        Error (_T("Could not set the time value back to what it was"));
        Error (_T("before this test was run."));
        returnVal = TPR_FAIL;
    }

    Info (_T(""));
    if(returnVal == TPR_PASS)
    {
        Info (_T("The test passed."));
    }
    else
    {
        Error (_T("The test failed."));
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

    Info (_T(""));

    return (returnVal);
}

/*******************************************************************************
 *
 *                             User specified time
 *
 ******************************************************************************/

/*
 * This function tests a user specified time
 *
 */

TESTPROCAPI testSetUserSpecifiedRealTime(
                     UINT uMsg,
                     TPPARAM tpParam,
                     LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;
    ULONGLONG ullRTTime = 0;

    TCHAR printBuffer[PRINT_BUFFER_SIZE] = { 0 };

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    breakIfUserDesires();

    // Get the current time. The RTC will be set back to this value after
    // the test is run.
    SYSTEMTIME currentTime;
    GetLocalTime(&currentTime);

    Info (_T(""));
    Info (_T("This test allows one to set the given time to the RTC.  Use this"));
    Info (_T("for debugging, etc.  It uses SetLocalTime, since this is a direct"));
    Info (_T("pass-through to OEMSetRealTime (unless you are using a softRTC)."));

    // Now get the date and time value supplied by the user
    SYSTEMTIME userTime = { 0 };
    SYSTEMTIME readTime = { 0 };

    if(parseUserDateTime(&userTime))
    {
        // Now test the parsed value from the command line
        // Convert to file time to check if it is within the range
        if(!SystemTimeToFileTime(&userTime,(FILETIME*)(&ullRTTime)))
        {
            printTimeToBuffer(&userTime,printBuffer,PRINT_BUFFER_SIZE);
            Error (_T("Error converting SystemTime to FileTime. The time is %s."), printBuffer);

            returnVal = TPR_FAIL;
            goto cleanAndExit;
        }

        /* ignore day of week because won't be set at this point */
        printTimeToBuffer(&userTime, printBuffer, PRINT_BUFFER_SIZE);
        Info (_T("Going to set the time to %s.  Using SetLocalTime to do this."), printBuffer);

        if (SetLocalTime (&userTime))
        {
            Info (_T("RTC call succeeded."));
        }
        else
        {
            Info (_T("RTC call failed."));
            returnVal = TPR_FAIL;
            goto cleanAndExit;
        }

        GetLocalTime (&readTime);
        /* don't ignore day of week */
        printTimeToBuffer(&readTime, printBuffer, PRINT_BUFFER_SIZE, FALSE);
        Info (_T("Calling GetLocalTime.  Time returned is %s."), printBuffer);

        if( userTime.wYear != readTime.wYear
            ||
            userTime.wMonth != readTime.wMonth
            ||
            userTime.wDay != readTime.wDay
            ||
            userTime.wHour != readTime.wHour
            ||
            ((readTime.wMinute - userTime.wMinute) % 60 > 1) ) 
        {
            Info( _T("WARNING: Set time doesn't match time read\r\n"));
            Info( _T("Please verify the input and output times for correctness\r\n"));
            goto cleanAndExit;
        }

    }
    else
    {
        Error (_T("Couldn't parse user input or no input present. Skip this test."));
        returnVal = TPR_SKIP;
        goto cleanAndExit;
    }

    returnVal = TPR_PASS;

cleanAndExit:

    // Set the RTC time back to what it was before the test was run.
    if(!SetLocalTime(&currentTime))
    {
        Error (_T("Could not set the time value back to what it was"));
        Error (_T("before this test was run."));
        returnVal = TPR_FAIL;
    }

    return (returnVal);
}


/*******************************************************************************
 *
 *                                              Print RTC Time
 *
 ******************************************************************************/

/*
 * This function prints out the time of the day.
 * Useful for debugging the system time.
 */

TESTPROCAPI rtcPrintTime(
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
    TCHAR printBuffer[PRINT_BUFFER_SIZE]  = { 0 };

    breakIfUserDesires();

    Info (_T("All values taken from GetLocalTime."));

    for (DWORD i = 0; i < 10; i++)
    {
        SYSTEMTIME sysTime;

        GetLocalTime (&sysTime);

        printTimeToBuffer(&sysTime, printBuffer, PRINT_BUFFER_SIZE, FALSE);
        Info (_T("Line %u   Time: %s"), i, printBuffer);

        Sleep (1000);
    }

    return (returnVal);
}

/*******************************************************************************
 *                             Test Valid Time
 ******************************************************************************/

// This function tests the set and get real time functions when a valid time is
// supplied
//
// Arguments:
//   Input: Time value that is to be tested
//   Output: None
//
// Return value:
//     TRUE if test passes, FALSE otherwise


BOOL testValidTime (LPSYSTEMTIME lpst)
{

    SYSTEMTIME getTime = { 0 };
    BOOL returnVal = FALSE;

    TCHAR printBuffer[PRINT_BUFFER_SIZE] = { 0 };


    // Check for null input pointer
    if(!lpst)
    {
        Error (_T("Error: Invalid pointer supplied for the Time argument"));
        goto cleanAndExit;
    }

    if(!SetLocalTime(lpst))
    {
        printTimeToBuffer(lpst,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("OEMSetRealTime did not set a valid time, %s "), printBuffer);
        Error (_T("that is within the supported range."));
        goto cleanAndExit;
    }

    // Now record time again after setting it. This is used to check if a
    // valid value is actaully set and to also verify that get time is
    // working correctly
    GetLocalTime(&getTime);


    // Now verify that the value was actually set and that get time
    // returned the correct value that was set
    if(!IsWithinTenSec(lpst, &getTime))
    {
        printTimeToBuffer(&getTime,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Test Failed - either OEMSetRealTime did not actually set a "));
        Error (_T("valid value or OEMGetRealTime is not functioning correctly."));
        Error (_T("OEMGetRealTime returned the following time after setting"));
        Error (_T("the time: %s"),printBuffer);
        Error (_T("The time read back is expected to be within 10 sec of the time set."));
        goto cleanAndExit;
    }

    returnVal = TRUE;

cleanAndExit:
    return (returnVal);
}

/*******************************************************************************
 *                             Test Invalid Time
 ******************************************************************************/

// This function tests the set and get time functions when an invalid time is
// supplied
//
// Arguments:
// ...Input: Time value that is to be tested
//    Output: None
//
// Return value:
//     TRUE if test passes, FALSE otherwise


BOOL testInvalidTime (LPSYSTEMTIME lpst)
{

    SYSTEMTIME getTime1 = { 0 }, getTime2 = { 0 };
    BOOL returnVal = FALSE;

    TCHAR printBuffer[PRINT_BUFFER_SIZE] = { 0 };

    // Check for null input pointer
    if(!lpst)
    {
        Error (_T("Error: Invalid pointer supplied for the Time argument"));
        goto cleanAndExit;
    }

    // First record the existing time before changing it, for later comparison
    GetLocalTime(&getTime1);

    if(SetLocalTime(lpst))
    {
        printTimeToBuffer(lpst,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("OEMSetRealTime accepted an invalid time, %s"));
        Error (_T("that is out of the supported range."));
        goto cleanAndExit;
    }

    // Now record time again after setting it. This is used to check if
    // the invalid value was actaully not set and to also verify that
    // get time is working correctly
    GetLocalTime(&getTime2);

    // Now verify that the value was actually not set and get time returned
    // the correct value that was got before the time was set
    if(!IsWithinTenSec(&getTime1,&getTime2))
    {
        Error (_T("Test Failed - either OEMSetRealTime set a bad value or"));
        Error (_T("OEMGetRealTime is not functioning correctly."));

        printTimeToBuffer(&getTime1,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Before trying to set an invalid time OEMGetRealTime returned"));
        Error (_T("the following time: %s"),printBuffer);

        printTimeToBuffer(&getTime2,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("After trying to set an invalid time OEMGetRealTime returned "));
        Error (_T("the following time: %s"),printBuffer);
        goto cleanAndExit;
    }

    returnVal = TPR_PASS;

cleanAndExit:
    return (returnVal);

}



/*******************************************************************************
 *
 *                                Rollover
 *
 ******************************************************************************/
/*
 * Verifies if rollover between years, months, days, hours, minutes and seconds
 * occurs correctly.
 * Various times are set and are read back after 5 seconds and verified if the
 * rollover is as expected.
 */

TESTPROCAPI testTimeRollover(
              UINT uMsg,
              TPPARAM tpParam,
              LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;
    INT roCount = 0;
    SYSTEMTIME getTime = { 0 };
    SYSTEMTIME startRange = { 0 }, endRange = { 0 };
    ULONGLONG ullStartRange = 0, ullEndRange = 0, ullROTime = 0;
    HANDLE hDST = NULL;

    TCHAR printBuffer[PRINT_BUFFER_SIZE] = { 0 };

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    breakIfUserDesires();

    // Get the current time. The RTC will be set back to this value after
    // the test is run.
    SYSTEMTIME currentTime = { 0 };
    GetLocalTime(&currentTime);


    Info (_T(""));
    Info (_T("This test verifies if rollover of seconds, minutes, hours,"));
    Info (_T("days and months occurs correctly."));
    Info (_T("Various times are set and are read back after 5 seconds and"));
    Info (_T("verified if the rollover is as expected."));
    Info (_T("DST is disabled before this test and enabled again after the test is completed."));
    
    hDST = disableDST();

    if(NULL == hDST)
    {
        // Maybe the user did not include DST in the image.
        // Let the test continue to run instead of failing it
        Info (_T("WARNING:  Cannot disable DST."));
    }

    // First find the range of time supported by the real-time clock
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
        printTimeToBuffer(&startRange,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    if(!SystemTimeToFileTime(&endRange,(FILETIME*)(&ullEndRange)))
    {
        printTimeToBuffer(&endRange,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

   // Iterate and set the values from the struct, sleep for 5 seconds and
   // check if rollover occurs correctly
    for(roCount=0; roCount<_countof(g_rolloverTests);roCount++)
    {
        // Convert to file time to check if it is within the range
        if(!SystemTimeToFileTime(&g_rolloverTests[roCount].time1,
                                                 (FILETIME*)(&ullROTime)))
        {
            printTimeToBuffer(&g_rolloverTests[roCount].time1,printBuffer,PRINT_BUFFER_SIZE);
            Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
            returnVal = TPR_FAIL;
            goto cleanAndExit;
        }

        if((ullROTime>=ullStartRange)&&
            (ullROTime<(ullEndRange-FIVE_SEC_IN_100ns_TICKS)))
        {
            Info (_T("")); // Blank line

            if (!SetLocalTime(&g_rolloverTests[roCount].time1))
            {
                printTimeToBuffer(&g_rolloverTests[roCount].time1,printBuffer,PRINT_BUFFER_SIZE);
                Error (_T("Error setting the time to %s"),printBuffer);
                returnVal = TPR_FAIL;
                goto cleanAndExit;
            }
            GetLocalTime(&getTime);

            Info (_T("Description : %s"), g_rolloverTests[roCount].comment);
            printTimeToBuffer(&g_rolloverTests[roCount].time1,printBuffer,PRINT_BUFFER_SIZE);
            Info (_T("OEMSetRealTime is given the time: %s"),printBuffer);
            printTimeToBuffer(&getTime,printBuffer,PRINT_BUFFER_SIZE);
            Info (_T("OEMGetRealTime returned the time: %s"),printBuffer);


            Info (_T("Now sleeping for 5 seconds ..."));
            Sleep (5000);

            // Read back the time now to check if rollover has occured correctly
            GetLocalTime(&getTime);
            printTimeToBuffer(&getTime,printBuffer,PRINT_BUFFER_SIZE);
            Info (_T("OEMGetRealTime has read back this time after 5 seconds: %s"),printBuffer);
            printTimeToBuffer(&g_rolloverTests[roCount].time2,printBuffer,PRINT_BUFFER_SIZE);
            Info (_T("The expected time after sleep is: %s"),printBuffer);

            if(!IsWithinTenSec(&g_rolloverTests[roCount].time2, &getTime))
            {
                Error (_T(""));
                Error (_T("Test Failed - Rollover did not occur correctly."));
                Error (_T("The test expects the time read back to be within 10 sec of the expected time."));
                Error (_T(""));
                returnVal = TPR_FAIL;
            }
            else
            {
                Info (_T(""));
                Info (_T("Test passed - Rollover occured correctly."));
                Info (_T(""));
            }
        }
    }

cleanAndExit:
    // Set the RTC time back to what it was before the test was run.
    if(!SetLocalTime(&currentTime))
    {
        Error (_T("Could not set the time value back to what it was"));
        Error (_T("before this test was run."));
        returnVal = TPR_FAIL;
    }

    Info (_T(""));
    if(returnVal == TPR_PASS)
    {
        Info (_T("The test passed."));
    }
    else
    {
        Error (_T("The test failed."));
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

    Info (_T(""));

    return (returnVal);
}


/*******************************************************************************
 *                              Print SystemTime
 ******************************************************************************/

// This function prints the real time that is in SYSTEMTIME format to the output
//
// Arguments:
//     Input: Time value that is to be printed
//     Output: None
//
// Return value:
//       TRUE if print successful, FALSE otherwise

/*
BOOL printSystemTime (LPSYSTEMTIME lpSysTime)
{
   // Check for null pointer in the input
   if(!lpSysTime)
   {
      Error (_T("Error: Invalid pointer supplied for the Time argument"));
      Error (_T("in the printSystemTime function."));
      return FALSE;
   }

   Info (_T("Date:%u-%u-%u  Time:%uh %um %us %ums \n"),lpSysTime->wMonth,
   lpSysTime->wDay, lpSysTime->wYear, lpSysTime->wHour, lpSysTime->wMinute,
   lpSysTime->wSecond, lpSysTime->wMilliseconds);

   return TRUE;
}
*/

/*******************************************************************************
 *                              Print SystemTime
 ******************************************************************************/

// This function prints the real time that is in SYSTEMTIME format to the output
//
// Arguments:
//      Input: Time value that is to be printed
//      Output: None
//
// Return value:
//     TRUE if print successful, FALSE otherwise

BOOL printSystemTime (LPSYSTEMTIME lpSysTime, BOOL bIgnoreDayOfWeek)
{
    // Check for null pointer in the input
    if(!lpSysTime)
    {
        Error (_T("Error: Invalid pointer supplied for the Time argument"));
        Error (_T("in the printSystemTime function."));
        return FALSE;
    }

    if (bIgnoreDayOfWeek)
    {
        Info (_T("%u-%u-%u %u:%u:%u"),lpSysTime->wMonth,
         lpSysTime->wDay, lpSysTime->wYear, lpSysTime->wHour, lpSysTime->wMinute,
         lpSysTime->wSecond);
    }
    else
    {
        const TCHAR * szDay = _T("ERR");

        if (lpSysTime->wDayOfWeek > 6)
        {
            Error (_T("Day of week is out of range. (%u)"), lpSysTime->wDayOfWeek);
        }
        else
        {
            szDay = DaysOfWeek[lpSysTime->wDayOfWeek];
        }

        Info (_T("%u-%u-%u %u:%u:%u %s"),lpSysTime->wMonth,
         lpSysTime->wDay, lpSysTime->wYear, lpSysTime->wHour, lpSysTime->wMinute,
         lpSysTime->wSecond, szDay);
    }


    return TRUE;
}

/*******************************************************************************
 *                         Print SystemTime to Buffer
 ******************************************************************************/
// This function prints the real time that is in SYSTEMTIME format to the buffer
//
// Arguments:
//     Input: Time value that is to be printed to the buffer
//      Output: None
//
// Return value:
//       TRUE if print successful, FALSE otherwise


BOOL printTimeToBuffer (LPSYSTEMTIME lpSysTime,__out_ecount(maxSize) TCHAR *ptcBuffer, size_t maxSize, BOOL bIgnoreDayOfWeek)
{
    INT j = 0;

    // Check for null pointer in the input
    if(!lpSysTime)
    {
        Error (_T("Error: Invalid pointer supplied for the Time argument in"));
        Error (_T("the printTimeToBuffer function."));
        return FALSE;
    }

    if(maxSize <= 1)
    {
        Error (_T("Buffer size insufficient to hold the date and time value. "));
        Error (_T("The size should be atleast 40 bytes."));
        return FALSE;
    }

    if (bIgnoreDayOfWeek)
    {
        j = _sntprintf_s(ptcBuffer, maxSize, maxSize-1, _T("%u-%u-%u %u:%u:%u"), lpSysTime->wMonth, lpSysTime->wDay, lpSysTime->wYear, lpSysTime->wHour, lpSysTime->wMinute, lpSysTime->wSecond);
    }
    else
    {
        const TCHAR * szDay = _T("ERR");
        if (lpSysTime->wDayOfWeek > 6)
        {
            Error (_T("Day of week is out of range. (%u)"), lpSysTime->wDayOfWeek);
        }
        else
        {
            szDay = DaysOfWeek[lpSysTime->wDayOfWeek];
        }

        j = _sntprintf_s(ptcBuffer, maxSize,maxSize-1,  _T("%u-%u-%u %u:%u:%u %s"), lpSysTime->wMonth, lpSysTime->wDay, lpSysTime->wYear, lpSysTime->wHour, lpSysTime->wMinute, lpSysTime->wSecond, szDay);
    }

    ptcBuffer[maxSize-1] = _T('\0'); // Append null char at the end of the string

    if(j<0)
    {
        Error (_T("Buffer size insufficient to hold the date and time value. "));
        Error (_T("The size should be atleast 40 bytes."));
        return FALSE;
    }
    return TRUE;
}


/*******************************************************************************
 *                                Is Equal
 ******************************************************************************/

// This function compares the two given times to check for equality.
// It returns true if they are within 1 second of each other
//
// Arguments:
//      Input: Time values that are to be compared
//      Output: None
//
// Return value:
//        TRUE if the values are equal, FALSE otherwise


BOOL IsEqual(LPSYSTEMTIME lpst1, LPSYSTEMTIME lpst2 )
{
    BOOL returnVal = FALSE;
    ULONGLONG ullTime1 = 0;
    ULONGLONG ullTime2 = 0;
    BOOL r1 = FALSE, r2 = FALSE;

    TCHAR printBuffer[PRINT_BUFFER_SIZE] = { 0 };

    // Check if the input pointers are valid
    if((!lpst1)||(!lpst2))
    {
        Error (_T("Error: Invalid pointer supplied for one of the time values."));
        goto cleanAndExit;
    }

    r1 = SystemTimeToFileTime( lpst1, (FILETIME*)&ullTime1);
    r2 = SystemTimeToFileTime( lpst2, (FILETIME*)&ullTime2);
    
    if(!r1 || !r2 )
    {
        Error (_T("Error converting SystemTime to FileTime in the IsEqual() function."));
        if(!r1)
        {
            Error (_T("Converting time value in the 1st parameter returned false."));
        }
        else
        {
            Error (_T("Converting time value in the 2nd parameter returned false."));
        }

        printTimeToBuffer(lpst1,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Time value in 1st parameter is:"));
        printTimeToBuffer(lpst2,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Time value in 2nd parameter is:"));
        goto cleanAndExit;
    }

    // If either of the values are within one second of max ULONGLONG FILETIME value,
    // then decrement both by one second since one second is added while comparing
    // them later which may result in overflow.
    if((ullTime1 > (ULONGLONG_MAX - ONE_SEC_IN_100ns_TICKS)) ||
            (ullTime2 > (ULONGLONG_MAX - ONE_SEC_IN_100ns_TICKS)))
    {
        ullTime1 -= ONE_SEC_IN_100ns_TICKS;
        ullTime2 -= ONE_SEC_IN_100ns_TICKS;
    }


    // Check if the times are within 1 sec of each other
    if(ullTime1<=ullTime2)
    {
        if((ullTime1 + ONE_SEC_IN_100ns_TICKS)>= ullTime2)
        {
            returnVal = TRUE;
        }
    }
    else
    {
        if((ullTime2 + ONE_SEC_IN_100ns_TICKS)>= ullTime1)
        {
            returnVal = TRUE;
        }
    }

cleanAndExit:
    return (returnVal);
}


/*******************************************************************************
 *                                Is Within 10 sec
 ******************************************************************************/

// This function compares the two given times to check if time elapsed is within 10 sec
// between lpstBefore and lpstAfter.
//
// Arguments:
//      Input: Time values that are to be compared
//      Output: None
//
// Return value:
//        TRUE if the values are equal, FALSE otherwise


BOOL IsWithinTenSec(LPSYSTEMTIME lpstBefore, LPSYSTEMTIME lpstAfter )
{
    BOOL returnVal = FALSE;
    ULONGLONG ullTimeBefore = 0;
    ULONGLONG ullTimeAfter = 0;
    BOOL r1 = FALSE, r2 = FALSE;

    TCHAR printBuffer[PRINT_BUFFER_SIZE] = { 0 };

    // Check if the input pointers are valid
    if((!lpstBefore)||(!lpstAfter))
    {
        Error (_T("Error: Invalid pointer supplied for one of the time values."));
        goto cleanAndExit;
    }

    // Disregard the millisec value since it is not set(ignored) when we set the time.
    lpstBefore->wMilliseconds   = 0;
    lpstAfter ->wMilliseconds   = 0;

    r1 = SystemTimeToFileTime(lpstBefore, (FILETIME*)(&ullTimeBefore));
    r2 = SystemTimeToFileTime(lpstAfter, (FILETIME*)(&ullTimeAfter));
    if(!r1 || !r2)
    {
        Error (_T("Error converting SystemTime to FileTime in the IsWithinTenSec() function."));
        if(!r1)
        {
            Error (_T("Converting time value in the 1st parameter returned false."));
        }
        else
        {
            Error (_T("Converting time value in the 2nd parameter returned false."));
        }

        printTimeToBuffer(lpstBefore,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Time value in 1st parameter is: %s"), printBuffer);
        printTimeToBuffer(lpstAfter,printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Time value in 2nd parameter is: %s"), printBuffer);
        goto cleanAndExit;
    }

    if(ullTimeAfter < ullTimeBefore)
    {
        // The time may either have gone backwards or may have overflowed
        // Since MaxTime difference allowed is 10 sec, add 10 sec to ullTimeBefore and
        // check if it is less than the original value. If it is less, then its an overflow
        // else time went backwards.

        ULONGLONG ullMaxTime = ullTimeBefore + TEN_SEC_IN_100ns_TICKS;

        if (ullMaxTime < ullTimeBefore)
        {
            // Time overflowed. Say so and return False.
            // There is no well defined way as to how the timer should behave on rollover
            Error (_T("The time got is less than the expected time."));
            printTimeToBuffer(lpstBefore,printBuffer,PRINT_BUFFER_SIZE);
            Error (_T("The time %s may have overflowed after it was set."), printBuffer);
        }
        // else time went backwards, return False
    }
    else if(ullTimeAfter <= (ullTimeBefore + TEN_SEC_IN_100ns_TICKS))
    {
        returnVal = TRUE;
    }

cleanAndExit:
    return (returnVal);
}

BOOL TimeTest(ULONGLONG* pTime)
{
    SYSTEMTIME SysTime = {0};

    if(!FileTimeToSystemTime((FILETIME*)pTime, &SysTime))
    {
        return FALSE;
    }

    return SetLocalTime(&SysTime);

}

#define BIN_SEARCH_MAX_ITER 500 //Binary search should be done way before this

// BOOL       bTruth - TRUE: search is used to find the start of the RTC range.
//                     FALSE: search is used to find the end of the RTC range.
// ULONGLONG* pStart - On input, the beginning of the range to scan
//                     On output, the result of the binarySearch.
// ULONGLONG  ullEnd - The end of the range to scan.
BOOL binarySearch(BOOL bTruth, ULONGLONG* pStart, ULONGLONG ullEnd)
{
    DWORD dwMaxIter = 0;
    if (ullEnd < *pStart)
    {
        return FALSE;
    }

    if(!TimeTest(pStart) && !TimeTest(&ullEnd))
    {
        return FALSE;
    }

    if(TimeTest(&ullEnd) != bTruth)   //Truth always needs to be the right
    {
        return FALSE;
    }

    ULONGLONG uGuess = 0;
    while((ullEnd - *pStart) > 2000)   //get it down to 0.2 ms.
    {
        uGuess = *pStart + (ullEnd - *pStart)/2;  // Order of operations prevents overflow.
        if(TimeTest(&uGuess) != bTruth)
        {
            *pStart = uGuess;
        }
        else
        {
            ullEnd = uGuess;
        }

        if((dwMaxIter++) > BIN_SEARCH_MAX_ITER) //we don't want an infinite loop, so we use this safeguard.
        {
            return FALSE;
        }
    }

    // On return, *pStart contains the result of the binary search.
    // Need to ensure that what's returned is in the RTC range.
    if ((0 != uGuess) && TimeTest(&uGuess))
    {
        *pStart = uGuess;
    }
    else if (bTruth && TimeTest(&ullEnd))
    {
        *pStart = ullEnd;
    }
    else if (!bTruth && TimeTest(pStart))
    {
        // nop
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}

/*******************************************************************************
 *                             Find Range
 ******************************************************************************/

// This function finds and prints out the supported time range
//
// Arguments:
//      Input: None
//      Output: The start and end times of the range
//
// Return value:
//       TRUE if range found, FALSE otherwise


BOOL findRange(SYSTEMTIME *pBeginTime, SYSTEMTIME *pEndTime)
{
    BOOL returnVal = FALSE;
    ULONGLONG ullTime = 0;
    ULONGLONG ullBeginTime = 0;
    ULONGLONG ullStart,ullEnd;

    TCHAR beginTimeBuffer[PRINT_BUFFER_SIZE] = { 0 };
    TCHAR endTimeBuffer[PRINT_BUFFER_SIZE] = { 0 };

    BOOL bPrintStart = TRUE, bPrintEnd = TRUE;

    Info (_T(""));
    Info (_T("Finding range supported by the OAL... "));
    Info (_T("Begin search at FILETIME 0 which is SYSTEMTIME 1-1-1601 0:0:0 and"));
    Info (_T("end the search at maximum 64-bit FILETIME value which is equal to"));
    Info (_T("SYSTEMTIME 9-14-30828 2:48:05.477"));
    Info (_T(""));

    // Get current RTC. We need to set RTC back to this value before returning
    SYSTEMTIME stCurrentTime = { 0 };
    GetLocalTime(&stCurrentTime);

    if(!SystemTimeToFileTime(&stCurrentTime, (FILETIME*)(&ullEnd)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        goto cleanAndExit;
    }

    ullStart = 0;
    // Look for the beginning of range starting from FileTime 0
    if(!binarySearch(TRUE, &ullStart, ullEnd))
    {
        Error (_T("Error failed to binary Search.  The test could locate the first valid time."));
        goto cleanAndExit;
    }
    
    ullBeginTime = ullStart;

    if(!SystemTimeToFileTime(&stCurrentTime, (FILETIME*)(&ullStart)))
    {
        Error (_T("Error converting SystemTime to FileTime."));
        goto cleanAndExit;
    }

   
    // Look for the end of range starting from FileTime MAX_FILETIME.
    ullEnd = MAX_FILETIME;

    if(!TimeTest(&ullEnd)) 
    {
        //If MAX_FILETIME not suported, we need to find the upper range value

        if(!binarySearch(FALSE, &ullStart, ullEnd))
        {
            Error (_T("Error failed to binary seach.  The test could not find the last valid time."));
            goto cleanAndExit;
        }
    }
    else
    {
        ullStart = MAX_FILETIME;
    }
    
    if(ullStart <= ullBeginTime)   //ullStart at this point contains the high end of the range. 
                                    //It Should not be less than the begin time.
    {
        Error (_T("Bad range.  High end is lower than low end"));
        goto cleanAndExit;

    }

    if(!FileTimeToSystemTime((FILETIME*)&ullBeginTime,pBeginTime))
    {
        Error (_T("Error converting FileTime %llu to SystemTime"),ullBeginTime);
        goto cleanAndExit;
    }

    if(!FileTimeToSystemTime((FILETIME*)&ullStart,pEndTime))
    {
        Error (_T("Error converting FileTime %llu to SystemTime"),ullStart);
        goto cleanAndExit;
    }

    if(!printTimeToBuffer(pBeginTime, beginTimeBuffer, PRINT_BUFFER_SIZE))
    {
        Error (_T("Error printing time to buffer."));
        bPrintStart = FALSE;
    }

    if(!printTimeToBuffer(pEndTime, endTimeBuffer, PRINT_BUFFER_SIZE))
    {
        Error (_T("Error printing time to buffer."));
        bPrintEnd = FALSE;
    }

    if(bPrintStart && bPrintEnd)
    {
        Info (_T("")); // Blank line
        Info (_T("The supported real-time range begins at %s and ends"),beginTimeBuffer);
        Info (_T("at %s inclusive."),endTimeBuffer);
        Info (_T("")); // Blank line
    }

    // The frequent setting of time is affecting the performance of the tests.
    // Sleep here for 1 minute to allow the system to settle down.
    Info (_T("Sleeping for 1 minute to allow the system to stabilize after time changes.") );
    Sleep(60000);

    returnVal = TRUE;

cleanAndExit:

    // Set the RTC time back to what it was before the test was run.
    if(!SetLocalTime(&stCurrentTime))
    {
        Error (_T("Could not set the time value back to what it was"));
        Error (_T("before this test was run."));
        returnVal = FALSE;
    }

    return (returnVal);
}


/*******************************************************************************
 *                Transition Points for the supported time range
 ******************************************************************************/
// This function finds the begin and end points of the time range supported by
// the OAL. Start with the minimum possible time (FILETIME 0) and keep
// incrementing the time and setting it until the set time function returns
// true; this is the start of the range. For the end time, start with the
// beginning of the range and set time again incrementing it until it returns
// false; this is the end of the range.
//
// Arguments:
//      Input: The time from where to begin searching and what to look for,
//             bBeginPoint is TRUE if looking for start of range and FALSE if
//             looking for end of range
//       Output: The start time or end time of the range
//
// Return value:
//        TRUE if found the start or end point of the range, FALSE otherwise


BOOL findTransitionPoints(ULONGLONG *pullTime, BOOL bBeginPoint,
                                             SYSTEMTIME *pstTransitionPoint)
{
    BOOL returnVal = FALSE;

    SYSTEMTIME setTime = { 0 };

    ULONGLONG timeInterval[] = {
        (10ull * 365ull * 24ull * 60ull * 60ull * 1000ull * 1000ull * 10ull),
                            // TEN_YEARS_IN_100ns_TICKS
        (365ull * 24ull * 60ull * 60ull * 1000ull * 1000ull * 10ull),
                            // ONE_YEAR_IN_100ns_TICKS
                (24ull * 60ull * 60ull * 1000ull * 1000ull * 10ull),
                                // ONE_DAY_IN_100ns_TICKS
        (60ull * 60ull * 1000ull * 1000ull * 10ull),
                            // ONE_HOUR_IN_100ns_TICKS
        (60ull * 1000ull * 1000ull * 10ull),
                            // ONE_MIN_IN_100ns_TICKS
        (1000ull * 1000ull * 10ull),
                            // ONE_SEC_IN_100ns_TICKS
        (10ull * 1000ull * 10ull),
                            // TEN_MSEC_IN_100ns_TICKS
        (1000ull * 10ull)};
                            // ONE_MSEC_IN_100ns_TICKS

    DWORD i = 0;
    ULONGLONG ullprevTime = 0;


    // Check for null pointer in the input
    if(!pullTime)
    {
        Error (_T("Error: Invalid pointer supplied for input time argument."));
        goto cleanAndExit;
    }


    for(i=0; i < _countof(timeInterval); i++)
    {
        do
        {
            if(!FileTimeToSystemTime((FILETIME *)pullTime, &setTime))
            {
                Error (_T("Error converting FileTime to SystemTime."));
                goto cleanAndExit;
            }

            ullprevTime = (*pullTime);

            *pullTime += timeInterval[i];
            if(*pullTime >= MAX_FILETIME) {
                *pullTime = MAX_FILETIME; // -0x2848800000; temp workaround
                if(!FileTimeToSystemTime( (FILETIME *)pullTime, pstTransitionPoint ) ) {
                    Error (_T("Error converting FileTime to SystemTime."));
                    goto cleanAndExit;
                }

                Info (_T("Using MAX_FILETIME 9-14-30828 3:48:05.477"));
                returnVal = TRUE;
                goto cleanAndExit;
            }

            // Check for overflow after addition
            if((*pullTime) < ullprevTime)
            {
                Info (_T("Reached 64 bit overflow value while finding the range."));

                // If looking for end of range and reached the 64 bit overflow,
                // it is possible that the platform may not have an end of range
                // which is perfectly good.
                // In this case, use the max. 64 bit FILETIME value as the end time.
                if(!bBeginPoint)
                {
                    // Get the max FILETIME value and assign the time to pstTransitionPoint.
                    ULONGLONG ullMaxFiletime = MAX_FILETIME;
                    if(!FileTimeToSystemTime((FILETIME *)(&ullMaxFiletime),pstTransitionPoint))
                    {
                        Error (_T("Error converting FileTime to SystemTime."));
                        goto cleanAndExit;
                    }

                    Info (_T("End of range may not exist. For testing purposes, we will use the"));
                    Info (_T("end time as 9-14-30828 2:48:05.477 which is MAX_FILETIME."));
                    Info (_T("")); // Blank line

                    returnVal = TRUE;
                }
                else
                {
                    Error (_T("Error finding the beginning of the range."));
                }
                goto cleanAndExit;
            }

        }while((!bBeginPoint) == (SetLocalTime(&setTime)));

        // At this point the *pullTime value is 2 intervals past the Transition
        // point (the Start/End points where the above while statement condition
        // is true). So, bring it back to the value where the while condition
        // holds true and continue

        (*pullTime) -= timeInterval[i];

        // do the following only if this is not the first iteration in which case
        // the pullTime is zero
        if(*pullTime)
        {
            (*pullTime) -= timeInterval[i];
        }

    }

    // If finding the Begin Range, increment the time by the timeInterval value
    // to reach the first point of time that is in the valid range
    if(bBeginPoint && (*pullTime))
    {
        (*pullTime) += timeInterval[--i];
    }

    if(!FileTimeToSystemTime((FILETIME *)(pullTime),&setTime))
    {
        Error (_T("Error converting FileTime to SystemTime."));
        goto cleanAndExit;
    }

    *pstTransitionPoint= setTime;

    returnVal = TRUE;

cleanAndExit:
    return (returnVal);
}


/*******************************************************************************
 *                Disable Daylight Saving Timezone
 ******************************************************************************/
// This function disables the DST
//
// Arguments:
//      Input: None
//      Output: None
//
// Return value:
//        Handle of DST0: if it is created successfully and disabled, NULL otherwise

HANDLE disableDST()
{
    HANDLE hDST = NULL;
    LPCTSTR lpszName = _T("DST0:");

    hDST = CreateFile(lpszName, GENERIC_WRITE | GENERIC_READ, 0,NULL,CREATE_ALWAYS,0,NULL);

    if(NULL == hDST || INVALID_HANDLE_VALUE == hDST)
    {
        Error (_T("Cannot CreateFile for DST0:.  GLE:  %d"), GetLastError());
        return NULL;
    }

    if(FALSE == DeviceIoControl(hDST, IOCTL_SERVICE_STOP, NULL, 0, NULL, 0, NULL, NULL))
    {
        Error (_T("Cannot send IOCTL to DST0:.  GLE:  %d"), GetLastError());
        CloseHandle(hDST);
        return NULL;
    }

    return hDST;
}

/*******************************************************************************
 *                Enable Daylight Saving Timezone
 ******************************************************************************/
// This function enables the DST
//
// Arguments:
//      Input: Handle of DST0:
//      Output: None
//
// Return value:
//        TRUE is DST is enabled successfully, FALSE otherwise

BOOL enableDST(HANDLE hDST)
{

    if(NULL == hDST || INVALID_HANDLE_VALUE == hDST)
    {
        return FALSE;
    }

    return(DeviceIoControl(hDST, IOCTL_SERVICE_START, NULL, 0, NULL, 0, NULL, NULL));

}







