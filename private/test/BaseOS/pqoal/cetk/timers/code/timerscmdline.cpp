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
 * timersCmdLine.cpp
 */

/*
 * functions for processing the timer tests command lines
 */

#include "timersCmdLine.h"

#include "..\..\..\common\commonUtils.h"


/*
 * Prints out the usage message for the timer tests. It tells the user
 * what the tests do and also specifies the input that the user needs to
 * provide to run the tests.
 */

VOID
printTimerUsageMessage ()
{
  Info (_T("These tests aim to test the timers.  There are three different timers,"));
  Info (_T("the real-time clock or RTC, the getTickCount clock or GTC and the"));
  Info (_T("HiPerformance clock. The tests take different command line arguments."));
  Info (_T("Please see the documentation for more information."));
  Info (_T(""));
  Info (_T("Options are case insensitive. All <time> values are given in seconds,"));
  Info (_T("unless the following modifiers are used:"));
  Info (_T("  s => seconds  (12s = 12 seconds"));
  Info (_T("  m => minutes  (12m = 12 minutes)"));
  Info (_T("  h => hours    (12h = 12 hours)"));
  Info (_T("  d => days     (12d = 12 days)"));
  Info (_T(""));
  Info (_T(""));
  Info (_T("TESTS FOR ALL THREE CLOCKS:"));
  Info (_T(""));
  Info (_T("Resolution tests:"));
  Info (_T(""));
  Info (_T("  %s <seconds>          Maximum resolution allowed for the GTC."),
    ARG_STR_GTC_MAX_RES);
  Info (_T("                        Decimal values accepted as well."));
  Info (_T(""));
  Info (_T("Backwards checks:"));
  Info (_T(""));
  Info (_T("  %s <time>      Run time for test."),
                                              ARG_STR_BACKWARDS_CHECK_RUN_TIME);
  Info (_T(""));
  Info (_T("Drift comparison tests:"));
  Info (_T(""));
  Info (_T("  Global args (can't be special cased per test):"));
  Info (_T("    [ %s <time>] [%s <time>]"),
    ARG_STR_DRIFT_RUN_TIME, ARG_STR_DRIFT_SAMPLE_SIZE );
  Info (_T("    (These are not for the %s or %s test.)"),
    ARG_STR_TRACK_IDLE_PERIODIC_NAME, ARG_STR_TRACK_IDLE_RANDOM_NAME);
  Info (_T(""));
  Info (_T("  Per test args:"));
  Info (_T("    [<testname> [<bound> <num> <num>]]"));
  Info (_T("    <testName> is one of %s, %s, %s, %s"),
    ARG_STR_BUSY_SLEEP_NAME, ARG_STR_OS_SLEEP_NAME, ARG_STR_TRACK_IDLE_PERIODIC_NAME,
    ARG_STR_TRACK_IDLE_RANDOM_NAME);
  Info (_T("    <bound> is one of %s, %s, %s"),
             ARG_STR_DRIFT_GTC_RTC, ARG_STR_DRIFT_GTC_HI, ARG_STR_DRIFT_HI_RTC);
  Info (_T("    <num> gives the bounds for the given comparison."));
  Info (_T("    Note the lack of dashes preceeding these arguments."));
  Info (_T(""));
  Info (_T("Wall clock tests:"));
  Info (_T(""));
  Info (_T("  %s <time>              Run time for wall clock test."),
                                                  ARG_STR_WALL_CLOCK_RUN_TIME);
  Info (_T(""));
  Info (_T("GetIdleTime tests:"));
  Info (_T(""));
  Info (_T("  %s <time>            Run time for idle time tests."),
    ARG_STRING_IDLE_RUN_TIME);
  Info (_T("  %s <Idle threshold> Idle threshold(value between 0 and 1) for idle time tests."),
    ARG_STRING_IDLE_THRESHOLD);
  Info (_T(""));
  Info (_T(""));
  Info (_T("TESTS FOR REAL-TIME CLOCK:"));
  Info (_T(""));
  Info (_T("OEMGetRealTime and OEMSetRealTime tests:"));
  Info (_T(""));
  Info (_T("  %s <mm/dd/yyyy hh:mm:ss> DateAndTime value to test the RTC functions."),
                                                         ARG_STR_DATE_AND_TIME);
  Info (_T(""));
  Info (_T("  The time is in 24 hour format"));
  Info (_T("  Example: %s 06/25/2006 23:47:35"),ARG_STR_DATE_AND_TIME);
  Info (_T(""));
  Info (_T("Reentrance of OEMGetRealTime and OEMSetRealTime tests:"));
  Info (_T(""));
  Info (_T("  %s <number>      Number of threads for the reentrance test"),
                                                        ARG_STR_NUM_OF_THREADS);
  Info (_T("  Example: %s 25"),ARG_STR_NUM_OF_THREADS);
  Info (_T(""));
  Info (_T("  %s <time>      Run time for the reentrance test."),
                                                            ARG_STR_RUN_TIME);
  Info (_T(""));

}


/*
 * pull the backwards checks params from the command line.
 *
 * dwRunTime can't be null.  On input it points to the default run
 * time in seconds.  On output it point to the user supplied runtime,
 * if this value was given and is correct.  If not it still holds the
 * default run time.
 *
 * returns true on success and false otherwise.  On false dwRunTime is
 * preserved.
 *
 * This prints a message telling the user what run time is being used
 * for the test.
 */
BOOL
handleBackwardsCheckParams (ULONGLONG * pullRunTime)
{
  BOOL returnVal = FALSE;

  if (!pullRunTime)
    {
      return (FALSE);
    }

  BOOL bUserSuppliedParams = FALSE;

  /*
   * if user doesn't specify the -c param to tux the cmd line global
   * var is null.  In this case don't even try to parse command line
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

      if (getOptionTimeUlonglong (&tokens, ARG_STR_BACKWARDS_CHECK_RUN_TIME,
                   pullRunTime))
    {
      bUserSuppliedParams = TRUE;
    }
    }

  /* convert to realistic time */
  double doTime;
  TCHAR * szUnits;
  realisticTime ((double) *pullRunTime, doTime, &szUnits);

  if (bUserSuppliedParams)
    {
      Info (_T("Using the user supplied test run time, which is %g %s."),
        doTime, szUnits);
    }
  else
    {
      /* use default */
      Info (_T("Using the default test run time, which is  %g %s"),
        doTime, szUnits);
    }

  returnVal = TRUE;
 cleanAndExit:

  return (TRUE);
}





typedef struct
{
  const TCHAR * szCmdName;
  sBounds * b;
  BOOL bUsingDefaults;
} sBoundsContainer;








BOOL
getBoundsForDriftTests (cTokenControl * tokens,
            const TCHAR * szTestName,
            sBoundsContainer * bounds,
            DWORD sBoundsLength);




/*
 * Returns FALSE if something goes wrong during parsing and
 * true otherwise.
 *
 * tokens is a non-null pointer to an initialized token class.
 * szTestName is the test name that starts off this string of
 * arguments.  What follows are the names of the arguments and the
 * corresponding bounds.  On input, the bounds are initialized to
 * their default values.  If command line parameters have been given
 * to override these then the specific bounds are reset.  If no
 * parameters are given then then bounds are not touched.
 *
 * On error the bounds are undefined.
 *
 * The parser is pretty good but will display odd behavior on odd
 * input.
 *
 * The general algorithm is:
 *
 * Move bounds into array of structures to make code cleaner.
 * Check for test name
 * Look for options of the form <optionName> <double> <double>
 * Convert what is found
 * Print out which values (default or user) are being used.
 *
 * In the bounds arrays, doFasterBound must always be numerically
 * greater.  This function preserves these symantics.
 */
BOOL
getBoundsForDriftTests(cTokenControl * tokens,
               const TCHAR * szTestName,
               const TCHAR * szBound1Name,
               sBounds * bound1,
               const TCHAR * szBound2Name,
               sBounds * bound2,
               const TCHAR * szBound3Name,
               sBounds * bound3)
{
  BOOL returnVal = FALSE;

  /* sanity check on params */
  if (!tokens || !szTestName || !szBound1Name || !bound1 ||
      !szBound2Name || !bound2 || !szBound3Name || !bound3)
    {
      return (FALSE);
    }

  sBoundsContainer bounds[3];

  /*
   * load values into struct for easy processing.  This allows
   * searching via for loops as opposed to special casing.
   */
  bounds[0].szCmdName = szBound1Name;
  bounds[0].b = bound1;
  bounds[1].szCmdName = szBound2Name;
  bounds[1].b = bound2;
  bounds[2].szCmdName = szBound3Name;
  bounds[2].b = bound3;

  /* set all to default */
  for (DWORD dwPos = 0; dwPos < 3; dwPos++)
    {
      bounds[dwPos].bUsingDefaults = 1;
    }

  /*
   * use the generic version
   * This will loads the bounds variables above, and also
   * set the using defaults flags
   */
  if (!getBoundsForDriftTests (tokens, szTestName,
                  bounds, 3))
    {
      goto cleanAndReturn;
    }

  for (DWORD dwPos = 0; dwPos < 3; dwPos++)
    {
      if (bounds[dwPos].bUsingDefaults)
    {
      Info (_T("Using default bounds for %s: ratio range is %g to %g."),
        bounds[dwPos].b->szName,
        bounds[dwPos].b->doFasterBound,
        bounds[dwPos].b->doSlowerBound);
    }
      else
    {
      Info (_T("Using user's bounds for %s: ratio range is %g to %g."),
        bounds[dwPos].b->szName,
        bounds[dwPos].b->doFasterBound,
        bounds[dwPos].b->doSlowerBound);
    }
    }

  returnVal = TRUE;

 cleanAndReturn:

  return (returnVal);

}

BOOL
getBoundsForDriftTests (cTokenControl * tokens,
            const TCHAR * szTestName,
            sBoundsContainer * bounds,
            DWORD sBoundsLength)
{
  BOOL returnVal = FALSE;

  /* where we currently are in the token array */
  DWORD dwCurrIndex = 0;

  /*
   * find the os test name.  If we don't find it then using the
   * default values.
   *
   * Order matters in the expression below.  Don't want to call
   * tokens[] with out of range index.
   */
  while ((dwCurrIndex < tokens->listLength()) &&
     (_tcsicmp (szTestName, (*tokens)[dwCurrIndex]) != 0))
    {
      dwCurrIndex++;
    }

  if (dwCurrIndex >= tokens->listLength())
    {
      returnVal = TRUE;
      goto cleanAndReturn;
    }

  /*
   * dwCurrIndex should point to the szTestName parameter.  Move to
   * the next one.
   */
  dwCurrIndex++;

  DWORD dwNumOptionsProcessed = 0;

  /* while we have tokens and have not processed all possible options */
  while ((dwCurrIndex < tokens->listLength()) &&
     (dwNumOptionsProcessed < 3))
    {
      /* look for bound one param */
      DWORD dwGetOptionReturn = CMD_NOT_FOUND;

      for (DWORD dwPos = 0;
       (dwPos < sBoundsLength) && (dwGetOptionReturn == CMD_NOT_FOUND); dwPos++)
    {
      double doB1, doB2;

      dwGetOptionReturn =
        getOptionPlusTwo (tokens, dwCurrIndex,
                  bounds[dwPos].szCmdName, &doB1, &doB2);

      switch (dwGetOptionReturn)
        {
        case CMD_FOUND_IT:
          if (!bounds[dwPos].bUsingDefaults)
        {
          /* we have already seen this option */
          Error (_T("Already seen option %s on command line."),
             bounds[dwPos].szCmdName);
          goto cleanAndReturn;
        }

          bounds[dwPos].bUsingDefaults = FALSE;

          if (doB1 > doB2)
        {
          bounds[dwPos].b->doFasterBound = doB1;
          bounds[dwPos].b->doSlowerBound = doB2;
        }
          else
        {
          bounds[dwPos].b->doFasterBound = doB2;
          bounds[dwPos].b->doSlowerBound = doB1;
        }

          dwNumOptionsProcessed ++;

          break;
        case CMD_NOT_FOUND:
          /* do nothing */
          break;
        case CMD_ERROR:
          /* bail out */
          goto cleanAndReturn;
        default:
          IntErr (_T("Bad case statement."));
          goto cleanAndReturn;
        }
    }

      dwCurrIndex += 3;
    }


  returnVal = TRUE;

 cleanAndReturn:

  return (returnVal);

}


BOOL
getOptionsForDriftTests (cTokenControl * tokens, BOOL *quickFail)
{
  BOOL returnVal = TRUE;
  DWORD dwSizeIndex = 0;

  if (!tokens || !quickFail)
  {
      return (FALSE);
  }

  *quickFail = FALSE;
  if (findTokenI (tokens, L"-quickFail", &dwSizeIndex))
  {
     *quickFail = TRUE;
  }

  return returnVal;
}

BOOL
getArgsForDriftTests (cTokenControl * tokens,
              const TCHAR * szSampleSizeName,
              const TCHAR * szRunTimeName,
              DWORD * pdwSampleSize,
              DWORD * pdwRunTime)
{
  BOOL returnVal = TRUE;

  if (!tokens || !szSampleSizeName || !szRunTimeName || !pdwSampleSize ||
      !pdwRunTime)
    {
      return (FALSE);
    }

  double doRealisticTime;
  _TCHAR * pszRealisticUnits;

  struct
  {
    const TCHAR * szCmdName;
    const TCHAR * szFriendlyName;
    DWORD * pdwVal;
  } vals[2];

  vals[0].szCmdName = szSampleSizeName;
  vals[0].pdwVal = pdwSampleSize;
  vals[0].szFriendlyName = _T("sample size");
  vals[1].szCmdName = szRunTimeName;
  vals[1].pdwVal = pdwRunTime;
  vals[1].szFriendlyName = _T("run time");

  for (DWORD dwPos = 0; dwPos < 2; dwPos++)
    {
      DWORD dwSizeIndex = 0;

      if (findTokenI (tokens, vals[dwPos].szCmdName, &dwSizeIndex))
    {
      /* make sure next token exists */
      if ((dwSizeIndex + 1 == 0) ||
          (dwSizeIndex + 1 >= tokens->listLength()))
        {
          Error (_T("Param %s has no argument."),
             vals[dwPos].szCmdName);
          returnVal = FALSE;
        }
      else
        {
          dwSizeIndex++;

          /* is on command line */
          ULONGLONG ullTimeS;

          if (!strToSeconds ((*tokens)[dwSizeIndex], &ullTimeS))
        {
          Error (_T("Error on param %s.  Couldn't convert %s to ")
             _T("seconds."),
             vals[dwPos].szCmdName, (*tokens)[dwSizeIndex]);
          returnVal = FALSE;
        }
          else
        {
          if (ullTimeS > (ULONGLONG) DWORD_MAX)
            {
              Error (_T("Sample size of %I64u is larger than a DWORD ")
                 _T("value."),
                 ullTimeS);
              returnVal = FALSE;
            }
          else
            {
              *(vals[dwPos].pdwVal) = (DWORD) ullTimeS;

              if (*(vals[dwPos].pdwVal) == 0)
            {
              Error (_T("Sample size can't be zero."));
              returnVal = FALSE;
            }
              else
            {
              realisticTime (double (*(vals[dwPos].pdwVal)),
                     doRealisticTime, &pszRealisticUnits);
              Info (_T("Using user supplied %s of %g %s."),
                vals[dwPos].szFriendlyName,
                doRealisticTime, pszRealisticUnits);
            }
            }
        }
        }
    }
      else
    {
      /* use default */
      realisticTime (double (*(vals[dwPos].pdwVal)),
             doRealisticTime, &pszRealisticUnits);
      Info (_T("User did not specify a %s.  Using %u s, ")
        _T("which is %g %s."),
        vals[dwPos].szFriendlyName,
        *(vals[dwPos].pdwVal), doRealisticTime, pszRealisticUnits);
    }

    }

  return (returnVal);
}



/*******************************************************************************
 *                        Parse user's date and time
 ******************************************************************************/

// This function parses the command line arguments and returns the
// user supplied date and time to test the Get and Set real-time functions.
//
// Arguments:
//  Input: None
//      Output: The time parsed from the command line
//
// Return value:
//      TRUE if time was parsed correctly, FALSE otherwise

BOOL parseUserDateTime (LPSYSTEMTIME pUserTime)
{
  cTokenControl tokens;

  BOOL returnVal = FALSE;

  if(!pUserTime)
  {
    goto cleanAndExit;
  }

  DWORD dwIndex1 =0;
  DWORD dwIndex2 = 0;

  const TCHAR *tok = NULL;
  const TCHAR *szDateSep = _T("/");
  const TCHAR *szTimeSep = _T(":");
  TCHAR * szNextTok = NULL;
  WORD tokArray[7] = {0, 0, 0, 0, 0, 0, 0};
  ULONGLONG ullVal;

  Info (_T("")); // Blank line

  if(!(g_szDllCmdLine && tokenize((TCHAR*)g_szDllCmdLine, &tokens)))
  {
    Error (_T("Couldn't parse the command line."));
    goto cleanAndExit;
  }

  if(!isOptionPresent(&tokens,(ARG_STR_DATE_AND_TIME)))
  {
    Info (_T("No user supplied date and time values."));
    Info (_T("You can specify the date and time using the following command line option:"));
    Info (_T("-dateAndTime mm/dd/yyyy hh:mm:ss"));
       Info (_T(""));
    goto cleanAndExit;
  }

  findTokenI(&tokens,ARG_STR_DATE_AND_TIME,&dwIndex1);

  // Check for correct number of arguments
  // Verify that there are exactly two arguments for the -dateAndTime option.
  // For this verify that the first two arguments following the option exist
  // and do not begin with -. Also verify that the argument following this is
  // either another option or the end of the command line.

  const TCHAR *str = NULL;
  DWORD checkIndex = ++dwIndex1; // First argument (date)

  // The two date and time arguments exist
  if((dwIndex1+2) > tokens.listLength())
  {
    Error (_T("Insufficient number of arguments for -dateAndTime option"));
    Error (_T("Should be -dateAndTime mm/dd/yyyy hh:mm:ss"));
    goto cleanAndExit;
  }

  // These two arguments are not options
  while(checkIndex <= (dwIndex1 + 1))
  {
    str = tokens[checkIndex];
    if(str[0] == '-')
    {
        Error (_T("Insufficient number of arguments for")
              _T("-dateAndTime option."));
        Error (_T("Should be -dateAndTime mm/dd/yyyy hh:mm:ss"));
        goto cleanAndExit;
    }
    checkIndex++;
  }

  // If another argument exists, it should be an other option
  if(checkIndex < tokens.listLength())
  {
    str = tokens[checkIndex];
    if(str[0] != '-')
    {
        Error (_T("Too many arguments for -dateAndTime option."));
        Error (_T("Should be -dateAndTime mm/dd/yyyy hh:mm:ss"));
        goto cleanAndExit;
    }
  }

  // Parse the date
  Info(_T("The user supplied date is: %s"),tokens[dwIndex1]);
  szNextTok = NULL;
  tok = _tcstok_s(tokens[dwIndex1], szDateSep, &szNextTok);

  while(tok)
  {
    if(dwIndex2 >= 3)
    {
        Error (_T("Incorrect(too many) arguments in date value."));
        Error (_T("Should be -dateAndTime mm/dd/yyyy hh:mm:ss"));
        goto cleanAndExit;
    }

    if(!strToULONGLONG(tok,&ullVal))
    {
        Error (_T("Error parsing the date."));
        goto cleanAndExit;
    }

    tokArray[dwIndex2] = (WORD)(ullVal);
    tok = _tcstok_s(NULL, szDateSep, &szNextTok);
    dwIndex2++;
  }

  if(dwIndex2<3)
  {
    Error (_T("Incorrect(fewer) arguments in date value."));
    Error (_T("Should be -dateAndTime mm/dd/yyyy hh:mm:ss"));
    goto cleanAndExit;
  }


  // Parse the time
  Info (_T("The user supplied time is: %s"),tokens[++dwIndex1]);
  szNextTok = NULL;
  tok = _tcstok_s(tokens[dwIndex1], szTimeSep, &szNextTok);

  while(tok)
  {
    if(dwIndex2 >= 6)
    {
        Error (_T("Incorrect(too many) arguments in time value."));
        Error (_T("Should be -dateAndTime mm/dd/yyyy hh:mm:ss"));
        goto cleanAndExit;
    }

    if(!strToULONGLONG(tok,&ullVal))
    {
        Error (_T("Error parsing the time."));
        goto cleanAndExit;
    }

    tokArray[dwIndex2] = (WORD)(ullVal);
    tok = _tcstok_s(NULL, szTimeSep, &szNextTok);
    dwIndex2++;
  }

  if(dwIndex2<6)
  {
    Error (_T("Incorrect(fewer) arguments in time value."));
    Error (_T("Should be -dateAndTime mm/dd/yyyy hh:mm:ss"));
    goto cleanAndExit;
  }

  pUserTime->wMonth = tokArray[0];
  pUserTime->wDay = tokArray[1];
  pUserTime->wYear = tokArray[2];
  pUserTime->wHour = tokArray[3];
  pUserTime->wMinute = tokArray[4];
  pUserTime->wSecond = tokArray[5];
  pUserTime->wMilliseconds = tokArray[6];

  returnVal = TRUE;

 cleanAndExit:
  return (returnVal);
}


/*******************************************************************************
 *                        Parse Thread Info
 ******************************************************************************/

// This function parses the command line arguments and returns the user
// supplied number of threads and the run time for the Reentrancy of Get and
// Set real-time test.
//
// Arguments:
//      Input: Pointer to number of threads and run time
//      Output: The number of threads and run time obtained from the
//              command line
//
// Return value:
//      TRUE if values are present and parsed correctly, FALSE otherwise


BOOL parseThreadInfo (DWORD *pNumThreads, DWORD *pRunTime)
{
  cTokenControl tokens;
  DWORD dwIndex;

  DWORD nThreads = 0,runTime = 0;

  BOOL returnVal = FALSE;

  double doRealisticTime;
  _TCHAR *pszRealisticUnits;

  Info (_T("")); // Blank line

  // Check for null pointer in the input
  if((!pNumThreads)||(!pRunTime))
  {
    Error (_T("Error: Invalid pointer supplied for the number of threads"));
    Error (_T("or the number of iterations."));
    goto cleanAndExit;
  }

  if(!(g_szDllCmdLine && tokenize((TCHAR*)g_szDllCmdLine, &tokens)))
  {
    Error (_T("Couldn't parse the command line."));
    Error (_T("Using the default value of %u threads"),DEFAULT_REAL_TIME_TEST_NUM_THREADS);
    Error (_T("and the default run time of %u seconds."),DEFAULT_REAL_TIME_TEST_RUN_TIME_SEC);
    goto cleanAndExit;
  }


  // Parse number of threads
  if(isOptionPresent(&tokens,(ARG_STR_NUM_OF_THREADS)))
  {
    findTokenI(&tokens,ARG_STR_NUM_OF_THREADS,&dwIndex);

    if(!strToULONGLONG(tokens[++dwIndex],(ULONGLONG*)(&nThreads)))
    {
        Error (_T("Error parsing the number of threads."));
        Error (_T("Using the default value of %u threads."),
                                           DEFAULT_REAL_TIME_TEST_NUM_THREADS);
    }
    else
    {
      if(!(nThreads >= 1))
      {
        Error (_T("The user supplied number of threads are %u"),nThreads);
        Error (_T("Acceptable values are greater than or equal to 1."));
        Error (_T("Using the default value of %u threads."),
                                           DEFAULT_REAL_TIME_TEST_NUM_THREADS);
      }
      else
      {
        Info (_T("The user supplied number of threads are %u"),nThreads);
        (*pNumThreads) = nThreads;
        returnVal = TRUE;
      }
    }
  }
  else
  {
    Info (_T("User did not supply number of threads, using default value of %u."),
                                            DEFAULT_REAL_TIME_TEST_NUM_THREADS);
  }

  realisticTime((double)(DEFAULT_REAL_TIME_TEST_RUN_TIME_SEC),doRealisticTime,&pszRealisticUnits);

  // Parse run time
  if(isOptionPresent(&tokens,(ARG_STR_RUN_TIME)))
  {
    findTokenI(&tokens,ARG_STR_RUN_TIME,&dwIndex);

    if(!strToSeconds(tokens[++dwIndex],(ULONGLONG*)(&runTime)))
    {
        Error (_T("Error parsing the run time."));
        Error (_T("Using the default value of %g %s."),doRealisticTime,pszRealisticUnits);
    }
    else
    {
      if(!(runTime >= 1))
      {
        Error (_T("The user supplied run time is %u seconds."),runTime);
        Error (_T("Acceptable values are greater than or equal to 1 second."));
        Error (_T("Using the default value of %g %s."),doRealisticTime,pszRealisticUnits);
      }
      else
      {
        realisticTime((double)runTime, doRealisticTime, &pszRealisticUnits);

        Info (_T("The test is going to run for the user supplied run time of %g %s"),
                                            doRealisticTime,pszRealisticUnits);
        (*pRunTime) = runTime;
        returnVal = TRUE;
      }
    }
  }
  else
  {
    Info (_T("User did not supply the run time, test will run for the default time of %g %s."),
                                                          doRealisticTime,pszRealisticUnits);
  }

 cleanAndExit:
  return (returnVal);
}
