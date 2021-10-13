//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*
 * timerConvert.cpp
 */

#include "timerConvert.h"


/*
 * timer structure for the getTickCountClock
 */
sTimer gGetTickCountTimer = {_T("GetTickCount"),
			    GetTickCountFunc,
			    GetTickCountFreqFunc,
                            MAX_DWORD, 0, 0.0};

/*
 * timer structure for the hi performance (queryPerformance*) clock
 * functions.
 */ 
sTimer gHiPerfTimer = {_T("Hi Performance"),
		      HiPerfTicksFunc,
		      HiPerfFreqFunc,
                      ULONGLONG_MAX, 0, 0.0};

/* timer structure for the time of day (system) clock */
sTimer gTimeOfDayTimer = {_T("Time of day (RTC)"),
			 RTCTicksFunc,
			 RTCFreqFunc,
                         ULONGLONG_MAX, 0, 0.0};

/*
 * call getTickCount and convert to ulonglong
 */
BOOL
GetTickCountFunc (ULONGLONG & ullTicks)
{
  /* 
   * since getTickCount returns a dword, which is unsigned, this won't 
   * cause sign extension problems if the high bit is set.
   */
  ullTicks = (ULONGLONG) GetTickCount ();

  /* no error code, so always succeed */
  return TRUE;
}

/*
 * granularity of GetTickCount is always milli-seconds
 */
BOOL
GetTickCountFreqFunc (ULONGLONG & ullFreq)
{
  ullFreq = 1000;
  return TRUE;
}

/*
 * call query performance counter and make the value unsigned before returning
 */
BOOL
HiPerfTicksFunc (ULONGLONG & ullTicks)
{
  LARGE_INTEGER liPerfCount;
  
  if (QueryPerformanceCounter (&liPerfCount) == FALSE)
    {
      return (FALSE);
    }

  /*
   * LARGE_INTEGER is a signed value.  A negative time doesn't make
   * any sense, so return an error.
   */
  if (liPerfCount.QuadPart < 0)
    {
      return (FALSE);
    }
  
  /* 
   * cast from signed to unsigned
   */
  ullTicks = (ULONGLONG) liPerfCount.QuadPart;

  return TRUE;
}

/*
 * use the QueryPerformanceFrequency function to return the frequency of
 * the high performance clock.  The value is a LARGE_INTEGER, so cast to
 * unsigned and then return it.
 */
BOOL
HiPerfFreqFunc (ULONGLONG & ullFreq)
{
  LARGE_INTEGER liPerfFreq;
  
  if (QueryPerformanceFrequency (&liPerfFreq) == FALSE)
    {
      return (FALSE);
    }

  /*
   * LARGE_INTEGER is a signed value.  A negative time doesn't make
   * any sense, so return an error.
   */
  if (liPerfFreq.QuadPart < 0)
    {
      return (FALSE);
    }
  
  /* 
   * cast from signed to unsigned
   */
  ullFreq = (ULONGLONG) liPerfFreq.QuadPart;

  return TRUE;
}

/*
 * The system time is returned as a structure with the number of years, months,
 * days, etc.  We just want a ulong representing all of this information (in the
 * frequency specified by the RTCTickFreq function).  The FILETIME conversion
 * functions return a ulonglong with this info.  We then need to convert the 
 * filetime structure to a ulonglong.
 */
BOOL
RTCTicksFunc (ULONGLONG & ullTicks)
{
  SYSTEMTIME sysTime;
  FILETIME fileTime;

  /* returns no error value */
  GetSystemTime (&sysTime);

  /* even though this is boolean, docs say zero is error */
  if (SystemTimeToFileTime (&sysTime, &fileTime) == 0)
    {
      return (FALSE);
    }
  
  /*
   * assumes little endian 
   */
  assert (sizeof (ullTicks) == sizeof (FILETIME));
  memcpy (&ullTicks, &fileTime, sizeof (ullTicks));

  return (TRUE);
}


/*
 * from the docs:
 *
 * This structure is a 64-bit value representing the number of
 * 100-nanosecond intervals since January 1, 1601.
 *
 * so:
 * 
 * 100 ns = 0.1 us, which corresponds to a freq of 10,000,000 hz.
 * 
 * note that these are the units, not the resolution of the clock.
 */
BOOL
RTCFreqFunc (ULONGLONG & ullFreq)
{
  ullFreq = 10000000;

  return (TRUE);
}
