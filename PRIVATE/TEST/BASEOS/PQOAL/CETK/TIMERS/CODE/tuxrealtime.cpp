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


////////////////////////////////////////////////////////////////////////////////
/***************  Constants and Defines Local to This File  *******************/

TCHAR * DaysOfWeek[] = {_T("Sun"),
            _T("Mon"),
            _T("Tues"),
            _T("Wed"),
            _T("Thurs"),
            _T("Fri"),
            _T("Sat")};


// This structure contains various times used to test the Set and Get Real Time 
// functions. 
stRealTimeTests g_testTimes[] = {
    { {2005, 6,TUES,14,   20,27,30,200}, _T("Random time") },
    { {2004, 2,SUN,29,   19,27,30,200}, _T("Leap year") },
    { {2005,12,SAT,31,   23,59,59,999}, _T("Year end") },
    { {2005, 1,SAT, 1,       0, 0, 0, 0}, _T("Year begin") },
        { {2005, 6,TUES,14,    0, 0, 0, 0}, _T("Hour begin") }, 
        { {2005, 6,TUES,14,   23,59,59,999}, _T("Hour end") },        
        { {2005, 6,TUES,14,   20, 0, 0, 0}, _T("Minute begin") },
    { {2005, 6,TUES,14,   10, 59,59,999}, _T("Minute end") },
    { {2005, 6,TUES,14,    9,27, 0, 0}, _T("Second begin") },
    { {2005, 6,TUES,14,    5,27,59,999}, _T("Second end") },    
        { {2005, 6,TUES,14,    6,27,30,  0}, _T("Millisec begin") },
        { {2005, 6,TUES,14,   11,27,30,999}, _T("Millisec end") },
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
  { {2005, 6,THURS,16,  14,31,57,0}, _T("Checking rollover for Second"), 
                          {2005, 6,THURS,16, 14,32, 2,0} },
  { {2005, 6,THURS,16,  14,59,57,0}, _T("Checking rollover for Minute"),
                          {2005, 6,THURS,16, 15, 0, 2,0} },
  { {2005, 6,THURS,16,  23,59,57,0}, _T("Checking rollover for Hour"),
                          {2005, 6,FRI,17, 0, 0, 2,0} },
  { {2005, 6,THURS,30,  23,59,57,0}, _T("Checking rollover for Day"),
                          {2005, 7,FRI, 1, 0, 0, 2,0} },
  { {2005,12,SAT,31,  23,59,57,0}, _T("Checking rollover for Month"),
                          {2006, 1,SUN, 1, 0, 0, 2,0} },
  
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

// Creates an array of time values to be used with the reentrant test
BOOL createTimeArray(SYSTEMTIME *pSysTime, DWORD dwArrSize);

// Creates and starts the threads and verifies the return values from them
BOOL createAndExecuteThreads(threadInfo *pthreads, DWORD dwNumThreads,
                                                               BOOL bcalibrate);

// ThreadProc for the reentrant test
DWORD WINAPI threadSetAndGetTime(LPVOID pthreads);

// Finds the threads with matching date and time values
BOOL findDateTimeMatch (LPSYSTEMTIME lpSysTime, DWORD arrIndex, DWORD dwArrLen, 
                                                     LPSYSTEMTIME lpSysTimeArr);
    
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
  SYSTEMTIME begin;
  SYSTEMTIME end; 
  ULONGLONG ullBegin, ullEnd;

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
  INT rtCount;
  SYSTEMTIME getTime;
  SYSTEMTIME startRange, endRange;
  ULONGLONG ullStartRange, ullEndRange, ullRTTime;

  TCHAR printBuffer[PRINT_BUFFER_SIZE];

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
  Info (_T("This verifies the OEMSetRealTime and OEMGetRealTime functions"));
  Info (_T("in the OAL. These functions are accessed using SetLocalTime and"));
  Info (_T("GetLocalTime functions. Various valid times (within the supported")); 
  Info (_T("time range) and invalid times (out of supported range) are set "));
  Info (_T("and verified that only the valid ones are set.")); 
  Info (_T("Also any user supplied date and time values are tested."));

  
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
  SYSTEMTIME userTime;

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
  ULONGLONG ullRTTime;

  TCHAR printBuffer[PRINT_BUFFER_SIZE];

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
  SYSTEMTIME userTime;

  if(parseUserDateTime(&userTime))
    {  
      // Now test the parsed value from the command line
      // Convert to file time to check if it is within the range
      if(!SystemTimeToFileTime(&userTime,(FILETIME*)(&ullRTTime)))
      {          
      printTimeToBuffer(&userTime,printBuffer,PRINT_BUFFER_SIZE);
      Error (_T("Error converting SystemTime to FileTime. The time is %s."),
         printBuffer);
      goto cleanAndExit;
      }
 
      /* ignore day of week because won't be set at this point */
      printTimeToBuffer(&userTime, printBuffer, PRINT_BUFFER_SIZE);
      Info (_T("Going to set the time to %s.  Using SetLocalTime to do this."), 
        printBuffer);

      if (SetLocalTime (&userTime))
    {
      Info (_T("RTC call succeeded."));
    }
      else
    {
      Info (_T("RTC call failed."));
      goto cleanAndExit;
    }

      GetLocalTime (&userTime);
      /* don't ignore day of week */
      printTimeToBuffer(&userTime, printBuffer, PRINT_BUFFER_SIZE, FALSE);
      Info (_T("Calling GetLocalTime.  Time returned is %s."), printBuffer);
    }
  else
    {
      Error (_T("Couldn't parse user input or no input present.  Failing..."));
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
  TCHAR printBuffer[PRINT_BUFFER_SIZE];

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
//         Input: Time value that is to be tested
//      Output: None
//
// Return value:
//        TRUE if test passes, FALSE otherwise


BOOL testValidTime (LPSYSTEMTIME lpst)
{

    SYSTEMTIME getTime;
    BOOL returnVal = FALSE;

    TCHAR printBuffer[PRINT_BUFFER_SIZE];


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
//         Input: Time value that is to be tested
//      Output: None
//
// Return value:
//        TRUE if test passes, FALSE otherwise


BOOL testInvalidTime (LPSYSTEMTIME lpst)
{

    SYSTEMTIME getTime1, getTime2;
    BOOL returnVal = FALSE;

    TCHAR printBuffer[PRINT_BUFFER_SIZE];

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
       Error (_T("the following time:"),printBuffer);
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
  INT roCount;
  SYSTEMTIME getTime;
  SYSTEMTIME startRange, endRange;
  ULONGLONG ullStartRange, ullEndRange, ullROTime;

  TCHAR printBuffer[PRINT_BUFFER_SIZE];

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
  Info (_T("This test verifies if rollover of seconds, minutes, hours,"));
  Info (_T("days and months occurs correctly."));
  Info (_T("Various times are set and are read back after 5 seconds and"));
  Info (_T("verified if the rollover is as expected."));
  

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
  for(roCount=0; roCount<countof(g_rolloverTests);roCount++)
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
       Sleep (5000); //This function sleeps for the given number of milliseconds

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
  Info (_T(""));

  return (returnVal);
}



/*******************************************************************************
 *
 *                                Re-entrant
 *
 ******************************************************************************/

/*
 * Verifies that the real-time functions are re-entrant. The test creates a 
 * number of threads which set the time and then immediately read it back. The 
 * time read back should either be the time set by that thread or a time set by
 * another thread. If not, it means that the set time function or get time 
 * function is not reentrant.
 */

TESTPROCAPI testReentrant(
              UINT uMsg, 
              TPPARAM tpParam, 
              LPFUNCTION_TABLE_ENTRY lpFTE) 
{

  INT returnVal = TPR_FAIL; 
  LPSYSTEMTIME pSysTimeArray = NULL;
  DWORD *pthreadHasRunArr = NULL;
  pthreadInfo pthreadInfoArr = NULL;

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
  Info (_T("This test verifies that the real-time functions are re-entrant."));
  Info (_T("The test creates a number of threads which set the time and then"));
  Info (_T("immediately read it back. The time read back should be either "));
  Info (_T("the time set by that thread or a time set by another thread."));
  Info (_T("If not, it means that the OEMSetRealTime function or the "));
  Info (_T("OEMGetRealTime function or both are not reentrant."));


  // Get the number of threads and run time for the test. Use default values if
  // user supplied values are not available.
  DWORD dwNumThreads = DEFAULT_REAL_TIME_TEST_NUM_THREADS;
  DWORD dwRunTime = DEFAULT_REAL_TIME_TEST_RUN_TIME_SEC;
  DWORD dwNumIterations;

  // Get values from the command line
  parseThreadInfo(&dwNumThreads,&dwRunTime); 

  // Allocate memory to store the time values for the threads. The size of the 
  // array should be equal to the number of threads
  pSysTimeArray = (SYSTEMTIME *)malloc(dwNumThreads * sizeof(SYSTEMTIME));
  if(!pSysTimeArray)
  {
      Error (_T("Error allocating memory for array to hold time values for the threads."));
    goto cleanAndExit;
  }
  
  // Allocate memory to store the number of times the threads have run. The size
  // of the array should be equal to the number of threads
  pthreadHasRunArr = (DWORD *)malloc(dwNumThreads * sizeof(DWORD));
  if(!pthreadHasRunArr)
  {
      Error (_T("Error allocating memory for the array to keep count of the"));
    Error (_T("number of times each of the threads have run."));
    goto cleanAndExit;
  }


  // Now fill in the array with the time values for the threads. 
  if(!createTimeArray(pSysTimeArray, dwNumThreads))
  {
      Error (_T("Error creating array of time values for the threads."));
    goto cleanAndExit;
  }

  // Print out the values from the array
  Info (_T(""));
  Info (_T("The following are the time values that will be set by the threads."));
  Info (_T("Each thread will set one value from the list:"));

  TCHAR printBuffer[PRINT_BUFFER_SIZE];
  
  for(DWORD i = 0; i < dwNumThreads; i++)
  {
     printTimeToBuffer((pSysTimeArray + i),printBuffer,PRINT_BUFFER_SIZE);
     Info(_T("Thread %u will set -> %s"),i,printBuffer);
  }  
  Info (_T(""));


  // Now create the array of structs 
  pthreadInfoArr = (pthreadInfo)malloc(dwNumThreads * sizeof(threadInfo));
  if(!pthreadInfoArr)
  {
      Error (_T("Error allocating memory for the array to hold information"));
    Error (_T("for the threads."));
    goto cleanAndExit;
  }
  
  
  // Fill in the values in the array of structs with the individual thread info
  for(DWORD dwIndex = 0; dwIndex < dwNumThreads; dwIndex++)
  {
    pthreadInfoArr[dwIndex].dwThreadNum = dwIndex;
    pthreadInfoArr[dwIndex].dwThreads = dwNumThreads;
    pthreadInfoArr[dwIndex].pthreadTimes = pSysTimeArray;
    pthreadInfoArr[dwIndex].pthreadHasRun = pthreadHasRunArr;
  }  

  
  // Now calculate the number of iterations required for the given run time.
  // For this, run the test with different number of iterations until the time 
  // taken to run them exceeds the specified threshold. Now using these 
  // numIterations and the time taken to run them, calculate the numIterations 
  // for the required run time.

  Info (_T("Calibrating the number of iterations required for the given run time"));

  ULONGLONG ullCalibrationThreshold = REENTRANT_TEST_CALIBRATION_THERESHOLD_S;
  ULONGLONG ullTestCalibrationRuns = 1;   // start with one iteration

  ULONGLONG ullTimeBegin, ullTimeEnd, ullDiff = 0;

  // Get the number of iterations such that the time taken to run them exceeds
  // the threshold time
  while(1)
  {
     // Set the number of iterations for the threads to ullTestCalibrationRuns
     dwNumIterations = (DWORD)ullTestCalibrationRuns;
     for(dwIndex = 0; dwIndex < dwNumThreads; dwIndex++)
     {
        pthreadInfoArr[dwIndex].dwIterations = dwNumIterations;
     }
    
     // Get the start time from GTC
     if(!(ullTimeBegin = (ULONGLONG)GetTickCount()))
     {
        Error (_T("Couldn't get the start time from GTC"));
      goto cleanAndExit;
     }

     // Run the threads with dwNumIterations
     if(!createAndExecuteThreads(pthreadInfoArr, dwNumThreads, TRUE))
     {
    Error (_T("Test failed during calibration"));
       goto cleanAndExit;
     }
    
    // Now get the end time from GTC
    if(!(ullTimeEnd = (ULONGLONG)GetTickCount()))
    {
        Error (_T("Couldn't get end time from GTC"));
      goto cleanAndExit;
    }

    //Subtract, accounting for overflow
    if(!AminusB(ullDiff,ullTimeEnd,ullTimeBegin,ULONGLONG_MAX))
    {
        IntErr (_T("AminusB failed."));
      Error (_T("Could not calculate the time taken."));
      goto cleanAndExit;
    }    

    // Check if ullDiff time has crossed the threshold time, if yes we're done.
    // If not we double the number of Iterations and run again.
    if(ullDiff < (ullCalibrationThreshold * 1000))
    {
    ULONGLONG prev = ullTestCalibrationRuns;
    ullTestCalibrationRuns *= 2;

       if(prev >= ullTestCalibrationRuns)
       {  
          Error (_T("Could not calibrate"));
          goto cleanAndExit;
       }
    }
    else
    {
    Info (_T("Done calibrating."));
    break;
    }    
  } /*while*/
   

  // Now, number of iterations = (runTime in ms * ullTestCalibrationRuns)/ ullDiff
  double nIt = (double)((((double)dwRunTime * 1000) * (double)ullTestCalibrationRuns)
                                                               /(double)ullDiff);

  // Number of iterations for each run = total iterations/number of test runs
  dwNumIterations = (DWORD)(nIt/REPEAT_REENTRANT_TEST_TIMES);

  Info (_T("The number of iterations for each thread are %u"), dwNumIterations);
  
  // Now set the number of iterations for all threads to the calculated value
  for(dwIndex = 0; dwIndex < dwNumThreads; dwIndex++)
  {
    pthreadInfoArr[dwIndex].dwIterations = dwNumIterations;
  }


  // Now begin actual testing
  
  Info (_T(" ")); // Blank line
  Info (_T("Now begin testing ..."));

  Info (_T(" ")); // Blank line
  Info (_T("Create and execute threads. The threads set a time value and then"));
  Info (_T("read it back. The value read back must either be the value that was"));
  Info (_T("set by that thread or it may be a value set by another thread, "));
  Info (_T("i.e., it should belong to the list of time values being set by "));
  Info (_T("different threads. The test fails if this is not true."));
  Info (_T("The above is repeated %u times to catch failures more consistently.")
                                                  ,REPEAT_REENTRANT_TEST_TIMES);
  
  for(DWORD j = 0; j < REPEAT_REENTRANT_TEST_TIMES; j++)
  {
    Info (_T("")); // Blank line
    Info (_T("Test run %u:"),j);
    if(createAndExecuteThreads(pthreadInfoArr, dwNumThreads, FALSE))
    {
      returnVal = TPR_PASS;
    }
    else
    {
      returnVal = TPR_FAIL;
      break;
    }
  }

  // Now print the number of times the threads ran
  Info (_T("")); // Blank line
  Info (_T("Printing the number of iterations that the threads ran for, during each test run:"));
  Info (_T("")); // Blank line  

  for(i=0; i<dwNumThreads; i++)
  {
     Info (_T("Thread %u has run %u times"), i, pthreadHasRunArr[i]);
  }

 cleanAndExit:
     
  // Set the RTC time back to what it was before the test was run. 
  if(!SetLocalTime(&currentTime))
  {
       Error (_T("Could not set the time value back to what it was"));
       Error (_T("before this test was run."));
       returnVal = TPR_FAIL;
  }  

  if(pSysTimeArray)
  {
     free(pSysTimeArray);
  }
  if(pthreadHasRunArr)
  {
     free(pthreadHasRunArr);
  }
  if(pthreadInfoArr)
  {
     free(pthreadInfoArr);
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
  Info (_T(""));

  return (returnVal);
}


/*******************************************************************************
 *                  Create an array of random supported time values
 ******************************************************************************/

// This function creates an array of random time values that are within the
// supported range. These values will be used by the threads for the reentrant
// test. The size of the array is equal to the number of threads.
//
// Arguments: 
//         Input: Pointer to the array that holds the time values to be used by the
//             threads and size of the array which is equal to the number of 
//             threads. 
//      Output: None
//
// Return value:
//        TRUE if array created succesfully, FALSE otherwise

BOOL createTimeArray(SYSTEMTIME *pSysTime, DWORD dwArrSize)
{

  SYSTEMTIME startRange, endRange;
  ULONGLONG ullStartRange, ullEndRange, ullTestTime;

  TCHAR printBuffer[PRINT_BUFFER_SIZE];
  

  // First find the range of time supported by the real-time clock
  if(!findRange(&startRange, &endRange))
  {
      Error (_T("Error finding the supported time range."));
    return FALSE;
  }  
  
  // Convert both range start and end times to file times for easy
  // comparison
  if(!SystemTimeToFileTime(&startRange,(FILETIME*)(&ullStartRange)))
  {
      printTimeToBuffer(&startRange,printBuffer,PRINT_BUFFER_SIZE);
    Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
    return FALSE;
  }
    
  if(!SystemTimeToFileTime(&endRange,(FILETIME*)(&ullEndRange)))
  {
      printTimeToBuffer(&endRange,printBuffer,PRINT_BUFFER_SIZE);
    Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
    return FALSE;
  }

  
  // Now check the time values from the REALTIME_TESTS structure, if they fall 
  // within the range, copy them to the pSysTimes[] array
  DWORD dwArrIndex = 0;
  
  for(DWORD dwPos = 0; (g_testTimes[dwPos].time.wYear != 0) && (dwArrIndex < dwArrSize);
                                                                      dwPos++)
  {
       // Convert to file time to check if it is within the range
     if(!SystemTimeToFileTime(&g_testTimes[dwPos].time,(FILETIME*)(&ullTestTime)))
     {
       printTimeToBuffer(&g_testTimes[dwPos].time,printBuffer,PRINT_BUFFER_SIZE);
         Error (_T("Error converting SystemTime to FileTime. The time is %s"),printBuffer);
       return FALSE;
     }
     
     // If within the range, copy the value to the array of time values
     if((ullTestTime>=ullStartRange)&&(ullTestTime<=ullEndRange))
     {        
        pSysTime[dwArrIndex] = g_testTimes[dwPos].time;        
        dwArrIndex++;
     }
   } 
  

  // If dwArrLen is less than dwArrSize, i.e., there are not as many time values 
  // as required, add random time values that fall within the supported 
  // time range to the array
  
  for( ;dwArrIndex < dwArrSize; dwArrIndex++)
  {

    // Generate a time value that falls in the supported time range. For this 
    // get a random amount of time that falls between 0 and the total time in 
    // the range (this is got from the product of a random value between 0 and 1
    // and the total time in the range). Add this time to the start of the range
    // to get a random value that falls within the range.    
  
      double doRandNum = getRandDoubleZeroToOne();

    ULONGLONG ullRangeDiff = ullEndRange - ullStartRange;
    ULONGLONG ullRandTime;
    SYSTEMTIME stRandTime;
              
    doRandNum = doRandNum * (double)ullRangeDiff;
    ullRandTime = ullStartRange + (ULONGLONG)doRandNum;

        if(!((ullRandTime>=ullStartRange)&&(ullRandTime<=ullEndRange)))
        {
           Error (_T("Generated a random time out of the supported range"));
           return FALSE;
        }    

    if(!FileTimeToSystemTime((FILETIME*)(&ullRandTime),&stRandTime))
    {
      Error (_T("Error converting FileTime to SystemTime."));
      return FALSE;
    }
    
    pSysTime[dwArrIndex] = stRandTime;        
  }

  return TRUE;   
}



/*******************************************************************************
 *                    Create and execute threads
 ******************************************************************************/

// This function creates and executes the threads for the reentrant test. After
// the threads finish running, it checks for failures in the threads and returns.
// If this function is called during calibration we skip a few print statements
// to keep the ouput clean.
//
// Arguments: 
//         Input: Pointer to the threadInfo structure that contains all the
//             information about the threads.
//             Boolean value that indicates if the function is called during 
//             calibration of number of iterations or during the actual test.
//      Output: None
//
// Return value:
//        TRUE if threads created and executed successfully, FALSE otherwise

BOOL createAndExecuteThreads(pthreadInfo pthreads, DWORD dwNumThreads, 
                                                                BOOL bcalibrate)
{
  BOOL returnVal = TRUE;

  // Now create threads to set and get different times. Each thread sets the 
  // time to one value from the array

  LPHANDLE hThreads = NULL;
  DWORD dwThreadId = 0;
  DWORD dwThreadSize = 0; 

  DWORD threadQuantum = 1;  // units are milliseconds


  // Create threads...
 
  // Allocate memory to store all the thread info
  dwThreadSize = dwNumThreads * sizeof(HANDLE);

  hThreads = (LPHANDLE)malloc(dwThreadSize);

  if(!hThreads)
  {
      Error (_T("Error allocating memory for the threads"));
    returnVal = FALSE;
    goto cleanAndExit;
  } 

    
  // Create threads and set thread quantum. The threads being the same priority,
  // they are run in round robin with each running for the given quantum of 
  // time each time.

  // Print only if testing and not calibrating  
  if(!bcalibrate)
  {
    Info (_T("Now creating the threads and executing them ..."));
  }

  for(DWORD i=0; i<dwNumThreads; i++)
  {
  
    // Set the number of times the thread has run to 0
    pthreads[i].pthreadHasRun[i] = 0;
    
    // Create thread and have it ready to execute
      hThreads[i] = CreateThread(NULL,0,threadSetAndGetTime,(LPVOID)(&pthreads[i]),
                                                                  0,&dwThreadId);
    
    if(!hThreads[i])
    {
        Error(_T("Unable to create thread %u.  GetLastError is %u."),
                i, GetLastError());
        returnVal = FALSE;
    }

    // Set thread quantum
    if(!CeSetThreadQuantum(hThreads[i], threadQuantum))
    {
        Error (_T("Error setting thread quantum to %u"),threadQuantum);
        returnVal = FALSE;
    }

  }   

  
  for(i=0; i<dwNumThreads; i++)
  {
      if(!(WaitForSingleObject(hThreads[i],INFINITE) == WAIT_OBJECT_0))
      {
        Error (_T("Thread %u did not return"),i);
      returnVal = FALSE;
      }    
  }


  // Now check the return values of the threads for failures  
  if(!bcalibrate)
  {
    Info (_T("Now checking for failures ..."));
  }
  
  DWORD exitCode;  
  for(i=0; i<dwNumThreads; i++)
  {
    if(GetExitCodeThread(hThreads[i], &exitCode))
    {
       if(!exitCode)
       {
         Error (_T("Thread %u returned false."),i);
         returnVal = FALSE;
       }
    }
    else
    {
       Error (_T("Could not get the return value of the thread %u."),i);
       returnVal = FALSE;
    }
  }
  

  // Now close all handles to the threads
  for(i=0; i<dwNumThreads; i++)
  {
    if(!CloseHandle(hThreads[i]))
    {
       Error (_T("Error closing handle to thread %u."),i);
       returnVal = FALSE;
    }
  }

  // Now we are done checking  
  if(!bcalibrate)
  {
    if(returnVal)
    {
      Info (_T("Done. No failures found."));
    }
    else
    {
      Error (_T("Done. Found failures"));
    }
  }


 cleanAndExit:
   // Free the memory that was allocated to store the thread info
   if(hThreads)
   {
       free(hThreads);
   }
   
   return returnVal;    
}



/*******************************************************************************
 *                    Thread Proc for Set and Get time
 ******************************************************************************/

// This thread sets a given time and then immediately reads it back for the
// given number of iterations. The time value that is read back should be the
// value that was set by this thread or it should belong to the array of time
// values being set by the different threads.
//
// Arguments: 
//         Input: Pointer to the threadInfo structure that contains all the
//             information that the thread needs to execute
//      Output: None
//
// Return value:
//        TRUE if test passes, FALSE otherwise

  
DWORD WINAPI threadSetAndGetTime (LPVOID pthreads)
{
  SYSTEMTIME setTime, getTime;
  BOOL returnVal = TRUE;
  BOOL found = FALSE;

  TCHAR printBuffer[PRINT_BUFFER_SIZE];

  pthreadInfo pthisThread = (pthreadInfo)pthreads;
  
  DWORD dwIndex = pthisThread->dwThreadNum;
 
  setTime = pthisThread->pthreadTimes[dwIndex];
  DWORD dwNumIterations = pthisThread->dwIterations;
  DWORD dwNumThreads = pthisThread->dwThreads;

   
  for(DWORD it = 0; it < dwNumIterations; it++)
  {
    // Increment the number of times this thread is run
    (pthisThread->pthreadHasRun[dwIndex])++;

    // Now sleep random times for random duration between 0 and 
    // RANGE_FOR_RANDOM_SLEEP_MSEC. This is to introduce random swapping in 
    // and out of the threads so they are run in a very random order.

    // Create random times that fall within the RANGE_FOR_RANDOM_SLEEP_MSEC. 
    // This can be got from the product of a random value between 0 and 1
    // and the total time in the range.

      double doRand = getRandDoubleZeroToOne();

    double doRandNum = doRand * (double)RANGE_FOR_RANDOM_SLEEP_MSEC;
    
     // Sleep if RandNum is greater than half the RANGE_FOR_RANDOM_SLEEP_MSEC, 
     // this allows sleep for a random number of times.

    if(doRandNum < ((double)RANGE_FOR_RANDOM_SLEEP_MSEC/2.0))
    {
      Sleep((DWORD)doRandNum);
    }

    
    if(!SetLocalTime(&setTime))
    {
       printTimeToBuffer(&setTime,printBuffer,PRINT_BUFFER_SIZE);
       Error (_T("Thread %u: OEMSetRealTime did not set the following time: %s"),
                                                             dwIndex, printBuffer);
       returnVal = FALSE;
       goto cleanAndExit;
    }

    // Now record time again after setting it. This is used to check if a 
    // value from the structure was correctly set. It may either be a value
    // that was set in this thread or it may have been set by another thread
    // that interrupted this one. 
    GetLocalTime(&getTime);

    // Compare the time read back with the time that was set
    if(!IsWithinTenSec(&setTime, &getTime))
    {
       // Search if the returned value belongs to the array. If yes, fine, 
       // else test failed.         
       for(DWORD i = 0; i < dwNumThreads; i++)
       {
         if(IsWithinTenSec(&(pthisThread->pthreadTimes[i]), &getTime))
         {
               found = TRUE;
         }
       }
       if(!found)
       {  	     
	 Error (_T("Thread %u: Test Failed in this thread."),dwIndex);
	 printTimeToBuffer(&getTime,printBuffer,PRINT_BUFFER_SIZE);
	 Error (_T("Thread %u: OEMGetRealTime returned %s"),dwIndex, printBuffer);
	 Error (_T("The time returned is expected to be within 10 sec of the time set."));

         findDateTimeMatch(&getTime,dwIndex,dwNumThreads,pthisThread->pthreadTimes);
         returnVal = FALSE;
       }
    }
  }
  
cleanAndExit:
  return ((DWORD)returnVal);
}

/*******************************************************************************
 *                Find threads with matching date and time values
 ******************************************************************************/

// This function finds the threads that set the matching date and time values 
// given.
//
// Arguments: 
//         Input: lpSysTime is the time value that is to be matched, lpSysTimeArr
//             is a pointer to the array of time values that is to be searched
//             for the given value, dwIndex is the array index where it is most
//             probably found(this will be the thread number that failed) and 
//             dwArrLen is the length of the array
//      Output: None
//
// Return value:
//        FALSE if can't search, TRUE otherwise

BOOL findDateTimeMatch (LPSYSTEMTIME lpSysTime, DWORD dwIndex, DWORD dwArrLen, 
                                                      LPSYSTEMTIME lpSysTimeArr)
{
   SYSTEMTIME stThreadTime;
   BOOL foundDate = FALSE;
   BOOL foundTime = FALSE;

   TCHAR printBuffer[PRINT_BUFFER_SIZE];

   
   if((!lpSysTime)||(!lpSysTimeArr))
   {
        Error (_T("Invalid pointer/s supplied as input in findTimeMatch function."));
      return FALSE;
   }

   stThreadTime.wYear = lpSysTime->wYear;
   stThreadTime.wMonth = lpSysTime->wMonth;
   stThreadTime.wDay = lpSysTime->wDay;
   stThreadTime.wHour = lpSysTime->wHour;
   stThreadTime.wMinute = lpSysTime->wMinute;
   stThreadTime.wSecond = lpSysTime->wSecond;
   stThreadTime.wMilliseconds = lpSysTime->wMilliseconds;


   if((stThreadTime.wYear == lpSysTimeArr[dwIndex].wYear) &&
          (stThreadTime.wMonth == lpSysTimeArr[dwIndex].wMonth) &&
          (stThreadTime.wDay == lpSysTimeArr[dwIndex].wDay))
   {
      printTimeToBuffer(&lpSysTimeArr[dwIndex],printBuffer,PRINT_BUFFER_SIZE);
         Error (_T("Thread %u: The date belongs to thread %u, that sets this date and time: %s"),
                                                    dwIndex, dwIndex, printBuffer);
   }
   else
   {
        for(DWORD i = 0; i < dwArrLen; i++)
        {
          if((stThreadTime.wYear == lpSysTimeArr[i].wYear) &&
          (stThreadTime.wMonth == lpSysTimeArr[i].wMonth) &&
          (stThreadTime.wDay == lpSysTimeArr[i].wDay))
          {
             printTimeToBuffer(&lpSysTimeArr[i],printBuffer,PRINT_BUFFER_SIZE);
             Error (_T("Thread %u: The date belongs to thread %u, that sets this date and time: %s"),
                                                    dwIndex, i, printBuffer);
            foundDate = TRUE;
          }
        }
     if(!foundDate)
     {
        Error (_T("Thread %u: Did not find any thread that sets a matching date."),dwIndex);
     }
   }

   if((stThreadTime.wHour == lpSysTimeArr[dwIndex].wHour) &&
          (stThreadTime.wMinute == lpSysTimeArr[dwIndex].wMinute) &&
          (stThreadTime.wSecond == lpSysTimeArr[dwIndex].wSecond))
   {
     printTimeToBuffer(&lpSysTimeArr[dwIndex],printBuffer,PRINT_BUFFER_SIZE);
        Error (_T("Thread %u: The time belongs to thread %u, that sets this date and time: %s"),
                                                      dwIndex, dwIndex, printBuffer);
   }
   else
   {
        for(DWORD i = 0; i < dwArrLen; i++)
        {        
           if((stThreadTime.wHour == lpSysTimeArr[i].wHour) &&
              (stThreadTime.wMinute == lpSysTimeArr[i].wMinute) &&
              (stThreadTime.wSecond == lpSysTimeArr[i].wSecond))
           { 
             printTimeToBuffer(&lpSysTimeArr[i],printBuffer,PRINT_BUFFER_SIZE);
                 Error (_T("Thread %u: The time belongs to thread %u, that sets this date and time: %s"),
                                                         dwIndex, i, printBuffer);
          foundTime = TRUE;
           }
        }
     if(!foundTime)
     {
        Error (_T("Thread %u: Did not find any thread that sets a matching time."),dwIndex);
     }
   }
   
   return TRUE;
}

/*******************************************************************************
 *                              Print SystemTime
 ******************************************************************************/

// This function prints the real time that is in SYSTEMTIME format to the output 
//
// Arguments: 
//         Input: Time value that is to be printed
//      Output: None
//
// Return value:
//        TRUE if print successful, FALSE otherwise

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
//         Input: Time value that is to be printed
//      Output: None
//
// Return value:
//        TRUE if print successful, FALSE otherwise

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
       TCHAR * szDay = _T("ERR");
      
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
//         Input: Time value that is to be printed to the buffer
//      Output: None
//
// Return value:
//        TRUE if print successful, FALSE otherwise


BOOL printTimeToBuffer (LPSYSTEMTIME lpSysTime,TCHAR *ptcBuffer,size_t maxSize,
            BOOL bIgnoreDayOfWeek)
{
  INT j;

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
      j = _sntprintf(ptcBuffer, maxSize, 
              _T("%u-%u-%u %u:%u:%u"), 
      lpSysTime->wMonth, lpSysTime->wDay, lpSysTime->wYear, lpSysTime->wHour, 
      lpSysTime->wMinute, lpSysTime->wSecond);
    }
  else
    {
      TCHAR * szDay = _T("ERR");
      if (lpSysTime->wDayOfWeek > 6)
    {
      Error (_T("Day of week is out of range. (%u)"), lpSysTime->wDayOfWeek);
    }
      else
    {
      szDay = DaysOfWeek[lpSysTime->wDayOfWeek];
    }    

      j = _sntprintf(ptcBuffer, maxSize, 
              _T("%u-%u-%u %u:%u:%u %s"), 
      lpSysTime->wMonth, lpSysTime->wDay, lpSysTime->wYear, lpSysTime->wHour, 
      lpSysTime->wMinute, lpSysTime->wSecond, szDay);
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
//         Input: Time values that are to be compared
//      Output: None
//
// Return value:
//        TRUE if the values are equal, FALSE otherwise


BOOL IsEqual(LPSYSTEMTIME lpst1, LPSYSTEMTIME lpst2 )
{
  BOOL returnVal = FALSE;
  ULONGLONG ullTime1;
  ULONGLONG ullTime2; 
  BOOL r1, r2;

  TCHAR printBuffer[PRINT_BUFFER_SIZE];

  // Check if the input pointers are valid
  if((!lpst1)||(!lpst2))
  {
    Error (_T("Error: Invalid pointer supplied for one of the time values."));
    goto cleanAndExit;
  }
  
  if(!((r1 = SystemTimeToFileTime(lpst1, (FILETIME*)(&ullTime1))) && 
               (r2 = SystemTimeToFileTime(lpst2,(FILETIME*)(&ullTime2)))))
  {
      Error (_T("Error converting SystemTime to FileTime in the IsEqual() function."));
    if(!r1)
    {
        Error (_T("Converting time value in the 1st parameter returned false."));
    }
    else
        Error (_T("Converting time value in the 2nd parameter returned false."));

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
//         Input: Time values that are to be compared
//      Output: None
//
// Return value:
//        TRUE if the values are equal, FALSE otherwise


BOOL IsWithinTenSec(LPSYSTEMTIME lpstBefore, LPSYSTEMTIME lpstAfter )
{
  BOOL returnVal = FALSE;
  ULONGLONG ullTimeBefore;
  ULONGLONG ullTimeAfter; 
  BOOL r1, r2;

  TCHAR printBuffer[PRINT_BUFFER_SIZE];

  // Check if the input pointers are valid
  if((!lpstBefore)||(!lpstAfter))
  {
    Error (_T("Error: Invalid pointer supplied for one of the time values."));
    goto cleanAndExit;
  }  

  // Disregard the millisec value since it is not set(ignored) when we set the time.
  lpstBefore->wMilliseconds = 0;
  lpstAfter->wMilliseconds=0;
  
  if(!((r1 = SystemTimeToFileTime(lpstBefore, (FILETIME*)(&ullTimeBefore))) && 
               (r2 = SystemTimeToFileTime(lpstAfter,(FILETIME*)(&ullTimeAfter)))))
  {
      Error (_T("Error converting SystemTime to FileTime in the IsWithinTenSec() function."));
    if(!r1)
    {
        Error (_T("Converting time value in the 1st parameter returned false."));
    }
    else
        Error (_T("Converting time value in the 2nd parameter returned false."));

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





/*******************************************************************************
 *                             Find Range
 ******************************************************************************/

// This function finds and prints out the supported time range
//
// Arguments: 
//         Input: None
//      Output: The start and end times of the range
//
// Return value:
//        TRUE if range found, FALSE otherwise


BOOL findRange(SYSTEMTIME *pBeginTime, SYSTEMTIME *pEndTime)
{
  BOOL returnVal = FALSE;
  ULONGLONG ullTime;
  ULONGLONG ullBeginTime;

  TCHAR beginTimeBuffer[PRINT_BUFFER_SIZE];
  TCHAR endTimeBuffer[PRINT_BUFFER_SIZE];

  BOOL bPrintStart = TRUE, bPrintEnd = TRUE;

  Info (_T("")); 
  Info (_T("Finding range supported by the OAL... "));
  Info (_T("Begin search at FILETIME 0 which is SYSTEMTIME 1-1-1601 0:0:0 and"));
  Info (_T("end the search at maximum 64-bit FILETIME value which is equal to"));
  Info (_T("SYSTEMTIME 5-28-60056 5:36:10"));
  Info (_T("")); 

  // Get current RTC. We need to set RTC back to this value before returning
  SYSTEMTIME stCurrentTime;
  GetLocalTime(&stCurrentTime);


  // Look for the beginning of range starting from FileTime 0
  ullTime = 0; 
 
  if(!findTransitionPoints(&ullTime,TRUE,pBeginTime))
  {
      Error (_T("Error finding the beginning of the range."));
    goto cleanAndExit;
  }  
  
  if(!printTimeToBuffer(pBeginTime, beginTimeBuffer, PRINT_BUFFER_SIZE))
  {
      Error (_T("Error printing time to buffer."));
    bPrintStart = FALSE;
  }

  // Look for end of range starting from the begin time
  if(!SystemTimeToFileTime(pBeginTime, (FILETIME*)(&ullBeginTime)))
  {
    Error (_T("Error converting SystemTime to FileTime."));
    goto cleanAndExit;
  }

  if(!findTransitionPoints(&ullBeginTime,FALSE,pEndTime))
  {
      Error (_T("Error finding the end of the range."));
    goto cleanAndExit;
  }

  // The frequent setting of time is affecting the performance of the tests.
  // Sleep here for 3 minutes to allow the system to settle down.
  // 3 min = 3 x 60sec = 180 x 1000ms
  Sleep(180000);

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
//         Input: The time from where to begin searching and what to look for, 
//             bBeginPoint is TRUE if looking for start of range and FALSE if
//             looking for end of range
//      Output: The start time or end time of the range
//
// Return value:
//        TRUE if found the start or end point of the range, FALSE otherwise


BOOL findTransitionPoints(ULONGLONG *pullTime, BOOL bBeginPoint, 
                                             SYSTEMTIME *pstTransitionPoint)
{
    BOOL returnVal = FALSE;
    BOOL repeat = TRUE;

    SYSTEMTIME setTime;

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
                                                    
  DWORD i;
  ULONGLONG ullprevTime;
  

  // Check for null pointer in the input
  if(!pullTime)
  {
    Error (_T("Error: Invalid pointer supplied for input time argument."));
    goto cleanAndExit;
  }
  

  for(i=0; i < countof(timeInterval); i++)  
  {
      do
      {          
       if(!FileTimeToSystemTime((FILETIME *)(pullTime),&setTime))
       {
          Error (_T("Error converting FileTime to SystemTime."));
          goto cleanAndExit;
       }

           ullprevTime = (*pullTime);

           (*pullTime) += timeInterval[i];

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
               ULONGLONG ullMaxFiletime = ULONGLONG_MAX;
                   if(!FileTimeToSystemTime((FILETIME *)(&ullMaxFiletime),pstTransitionPoint))
                   {
                  Error (_T("Error converting FileTime to SystemTime."));
                  goto cleanAndExit;
                   }

                   Info (_T("End of range may not exist. For testing purposes, we will use the"));
               Info (_T("end time as 5-28-60056 5:36:10 which is the maximum 64-bit FILETIME value."));
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



