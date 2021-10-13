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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*
 * sleepRoutines.cpp
 */

#include "sleepRoutines.h"
#include "..\..\..\common\commonUtils.h"

BOOL OsSleepFunction (DWORD dwMs)
{
  Sleep (dwMs);
  return (TRUE);
}

BOOL BusyOsSleepFunction (DWORD dwMs)
{
  DWORD dwStart = GetTickCount();

  DWORD dwEnd = dwMs + dwStart;

  if (dwEnd < MAX (dwMs, dwStart))
    {
      /* going to roll */
      while (GetTickCount () >= dwStart);
    }

  while (dwEnd > GetTickCount());

  return (TRUE);
}

/* global for use with little sleep functions */
DWORD gdwInnerSleepIntervalMs = 0;

void
setInnerSleepIntervalMS (DWORD dwTime)
{
  gdwInnerSleepIntervalMs = dwTime;
}

BOOL LittleOsSleepFunction (DWORD dwMs)
{
  DWORD dwStart = GetTickCount();

  DWORD dwEnd = dwMs + dwStart;

  if (dwEnd < MAX (dwMs, dwStart))
    {
      /* going to roll */
      while (GetTickCount () >= dwStart)
	{
	  Sleep (gdwInnerSleepIntervalMs);
	}
    }

  while (dwEnd > GetTickCount())
    {
      Sleep (gdwInnerSleepIntervalMs);
    }

  return (TRUE);
}

/* 
 * Sleep for >= dwMs.  The sleep intervals are random, with the
 * average os sleep time as gdwInnerSleepIntervalMs.
 */
BOOL RandomOsSleepFunction (DWORD dwMs)
{
  DWORD dwStart = GetTickCount();

  DWORD dwEnd = dwMs + dwStart;

  /* with a gaussian distribution twice the average is the max */
  DWORD dwMax = gdwInnerSleepIntervalMs * 2;

  if (dwEnd < MAX (dwMs, dwStart))
    {
      /* going to roll */
      while (GetTickCount () >= dwStart)
	{
	  if (!WaitRandomTime (dwMax))
	    {
	      return (FALSE);
	    }
	}
    }

  while (dwEnd > GetTickCount())
    {
      if (!WaitRandomTime (dwMax))
	{
	  return (FALSE);
	}
    }

  return (TRUE);
}

/* 
 * Note that this violates the standard sleep semantics.  It will
 * always return before dwMaxWait has ended.  Hence, it is called
 * wait, not sleep.
 */
BOOL
WaitRandomTime (DWORD dwMaxWait)
{
  Sleep ((DWORD)
	 (((double) rand ()) / ((double) RAND_MAX) * dwMaxWait));
  return (TRUE);
}
