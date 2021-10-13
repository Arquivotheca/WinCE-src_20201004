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
DWORD *gdwInnerSleepIntervalListMs = NULL;
DWORD gdwNumTimes = 0;
DWORD gdwTimeIndex = 0;

void
setInnerSleepIntervalsMS (DWORD *dwTimeList, DWORD dwNumTimes)
{
    gdwInnerSleepIntervalListMs = dwTimeList;
    gdwNumTimes = dwNumTimes;
    gdwTimeIndex = 0;
}

BOOL LittleOsSleepFunction (DWORD dwMs)
{
    DWORD dwStart = GetTickCount();

    DWORD dwEnd = dwMs + dwStart;

    DWORD dwInnerSleepIntervalMs = dwMs;

    /* Use a different value from gdwInnerSleepIntervalListMs for each call */
    if (gdwInnerSleepIntervalListMs != NULL && gdwNumTimes > 0)
    {
        dwInnerSleepIntervalMs = gdwInnerSleepIntervalListMs[gdwTimeIndex % gdwNumTimes];
        gdwTimeIndex = (gdwTimeIndex + 1) % gdwNumTimes;
    }
    else
    {
        //Should never happen
        return FALSE;
    }

    if (dwEnd < MAX (dwMs, dwStart))
    {
        /* going to roll */
        while (GetTickCount () >= dwStart)
        {
            Sleep (dwInnerSleepIntervalMs);
        }
    }

    while (dwEnd > GetTickCount())
    {
        Sleep (dwInnerSleepIntervalMs);
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

    DWORD dwMaxMs = dwMs;

    /* Use a different value from gdwInnerSleepIntervalListMs for each call */
    if (gdwInnerSleepIntervalListMs != NULL && gdwNumTimes > 0)
    {
        /* A gaussian distribution will average to 1/2 the maximum value */
        dwMaxMs = 2 * gdwInnerSleepIntervalListMs[gdwTimeIndex % gdwNumTimes];
        gdwTimeIndex = (gdwTimeIndex + 1) % gdwNumTimes;
    }
    else
    {
        return FALSE;
    }

    if (dwEnd < MAX (dwMs, dwStart))
    {
        /* going to roll */
        while (GetTickCount () >= dwStart)
        {
            if (!WaitRandomTime (dwMaxMs))
            {
                return (FALSE);
            }
        }
    }

    while (dwEnd > GetTickCount())
    {
        if (!WaitRandomTime (dwMaxMs))
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
    UINT uiRand = 0;
    rand_s(&uiRand);

    Sleep ((DWORD)
        (((double) uiRand) / ((double) UINT_MAX) * dwMaxWait));
    return (TRUE);
}
