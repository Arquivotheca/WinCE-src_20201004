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
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    timeutil.c - time structures/functions shared between coredll and kernel
//

#include "kerncmn.h"

#define WEEKDAY_OF_1601         1

#define FIRST_VALID_YEAR        1601        // first valid year 
#define TICKS_IN_A_DAY          86400000    // # of ticks in a day
#define TICKS_IN_A_YEAR         31536000000 // # of ticks in a year 
#define MS_TO_100NS             10000       // multiplier to convert time in ticks to time in 100ns
#define MINUTES_TO_MILLS        60000       // convert minutes to milliseconds
#define DAYS_IN_400_YEARS       146097      // # of days in 400 years
#define IsLeapYear(Y)           (!((Y)%4) && (((Y)%100) || !((Y)%400)))
#define NumberOfLeapYears(Y)    ((Y)/4 - (Y)/100 + (Y)/400)
#define ElapsedYearsToDays(Y)   ((Y)*365 + NumberOfLeapYears(Y))
#define MaxDaysInMonth(Y,M)     (IsLeapYear(Y) \
            ? (LeapYearDaysBeforeMonth[(M) + 1] - LeapYearDaysBeforeMonth[M]) \
            : (NormalYearDaysBeforeMonth[(M) + 1] - NormalYearDaysBeforeMonth[M]))

const WORD LeapYearDaysBeforeMonth[13] = {
    0,                                 // January
    31,                                // February
    31+29,                             // March
    31+29+31,                          // April
    31+29+31+30,                       // May
    31+29+31+30+31,                    // June
    31+29+31+30+31+30,                 // July
    31+29+31+30+31+30+31,              // August
    31+29+31+30+31+30+31+31,           // September
    31+29+31+30+31+30+31+31+30,        // October
    31+29+31+30+31+30+31+31+30+31,     // November
    31+29+31+30+31+30+31+31+30+31+30,  // December
    31+29+31+30+31+30+31+31+30+31+30+31};

const WORD NormalYearDaysBeforeMonth[13] = {
    0,                                 // January
    31,                                // February
    31+28,                             // March
    31+28+31,                          // April
    31+28+31+30,                       // May
    31+28+31+30+31,                    // June
    31+28+31+30+31+30,                 // July
    31+28+31+30+31+30+31,              // August
    31+28+31+30+31+30+31+31,           // September
    31+28+31+30+31+30+31+31+30,        // October
    31+28+31+30+31+30+31+31+30+31,     // November
    31+28+31+30+31+30+31+31+30+31+30,  // December
    31+28+31+30+31+30+31+31+30+31+30+31};

const BYTE LeapYearDayToMonth[366] = {
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // January
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,        // February
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // March
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,     // April
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,  // May
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,     // June
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,  // July
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,  // August
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,     // September
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,  // October
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,     // November
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11}; // December

const BYTE NormalYearDayToMonth[365] = {
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // January
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,           // February
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // March
     3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,     // April
     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,  // May
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,     // June
     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,  // July
     7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,  // August
     8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,     // September
     9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,  // October
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,     // November
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11}; // December

//
// MAX_TIME_IN_TICKS - calculated FROM gMaxSystemtime
// (2:48:05.477 9/14/30828)
//
#define MAX_TIME_IN_TICKS  0x000346dc5d638865 

//
// MAX_SYSTEMTIME translated from MAX_FILETIME of 0x7FFFFFFFFFFFFFFF
// which is 2:48:05.477 9/14/30828
// If SYSTEMTIME layout changes this will need to be adjusted, but that's pretty unlikely
//
const SYSTEMTIME gMaxSystemtime = { 30828,9,4,14,2,48,5,477 };

//
// test if a system time is valid
// MAX_FILETIME is 2:48:05.477 9/14/30828, so we'll use that as the max here as well
//
BOOL IsValidSystemTime(const SYSTEMTIME *lpst) {
    BOOL fLessThanMaxFiletime = 
            lpst->wYear < gMaxSystemtime.wYear
        || (lpst->wYear == gMaxSystemtime.wYear
                && lpst->wMonth < gMaxSystemtime.wMonth)
        || (lpst->wYear == gMaxSystemtime.wYear
                && lpst->wMonth == gMaxSystemtime.wMonth
                && lpst->wDay < gMaxSystemtime.wDay)
        || (lpst->wYear == gMaxSystemtime.wYear 
                && lpst->wMonth == gMaxSystemtime.wMonth
                && lpst->wDay == gMaxSystemtime.wDay
                && lpst->wHour < gMaxSystemtime.wHour)
        || (lpst->wYear == gMaxSystemtime.wYear
                && lpst->wMonth == gMaxSystemtime.wMonth
                && lpst->wDay == gMaxSystemtime.wDay
                && lpst->wHour == gMaxSystemtime.wHour
                && lpst->wMinute < gMaxSystemtime.wMinute)
        || (lpst->wYear == gMaxSystemtime.wYear
                && lpst->wMonth == gMaxSystemtime.wMonth
                && lpst->wDay == gMaxSystemtime.wDay
                && lpst->wHour == gMaxSystemtime.wHour
                && lpst->wMinute == gMaxSystemtime.wMinute
                && lpst->wSecond < gMaxSystemtime.wSecond)
        || (lpst->wYear == gMaxSystemtime.wYear
                && lpst->wMonth == gMaxSystemtime.wMonth
                && lpst->wDay == gMaxSystemtime.wDay
                && lpst->wHour == gMaxSystemtime.wHour
                && lpst->wMinute == gMaxSystemtime.wMinute
                && lpst->wSecond == gMaxSystemtime.wSecond
                && lpst->wMilliseconds <= gMaxSystemtime.wMilliseconds);

    return fLessThanMaxFiletime                 // MAX_FILETIME is also used as a systemtime max
        && (lpst->wYear >= FIRST_VALID_YEAR)    // at least year 1601
        && ((lpst->wMonth - 1) < 12)            // month between 1-12
        && (lpst->wDay >= 1)                    // day >=1
        && (lpst->wDay <= MaxDaysInMonth (lpst->wYear,lpst->wMonth-1))   // day < max day of month
        && (lpst->wHour < 24)                   // valid hour
        && (lpst->wMinute < 60)                 // valid minute
        && (lpst->wSecond < 60)                 // valid second
        && (lpst->wMilliseconds < 1000);        // valid milli-second
}

//
// convert days + milliseconds to milliseconds
//
__inline ULONGLONG DayAndFractionToTicks (ULONG ElapsedDays, ULONG Milliseconds)
{
    return (ULONGLONG) ElapsedDays * TICKS_IN_A_DAY + Milliseconds;
}

//
// convert time to # of ticks since 1601/1/1, 00:00:00
//
ULONGLONG NKSystemTimeToTicks (const SYSTEMTIME *lpst)
{
    ULONG ElapsedDays;
    ULONG ElapsedMilliseconds;

    ElapsedDays = ElapsedYearsToDays(lpst->wYear - FIRST_VALID_YEAR);
    ElapsedDays += (IsLeapYear(lpst->wYear) ?
        LeapYearDaysBeforeMonth[lpst->wMonth-1] :
        NormalYearDaysBeforeMonth[lpst->wMonth-1]);
    ElapsedDays += lpst->wDay - 1;
    ElapsedMilliseconds = (((lpst->wHour*60) + lpst->wMinute)*60 +
       lpst->wSecond)*1000 + lpst->wMilliseconds;

    return DayAndFractionToTicks (ElapsedDays, ElapsedMilliseconds);
}

//
// calculate # of years given the number of days
//
__inline ULONG ElapsedDaysToYears (ULONG ElapsedDays)
{
    ULONG NumberOf400s;
    ULONG NumberOf100s;
    ULONG NumberOf4s;

    //  A 400 year time block is 146097 days
    NumberOf400s = ElapsedDays / DAYS_IN_400_YEARS;
    ElapsedDays -= NumberOf400s * DAYS_IN_400_YEARS;
    //  A 100 year time block is 36524 days
    //  The computation for the number of 100 year blocks is biased by 3/4 days per
    //  100 years to account for the extra leap day thrown in on the last year
    //  of each 400 year block.
    NumberOf100s = (ElapsedDays * 100 + 75) / 3652425;
    ElapsedDays -= NumberOf100s * 36524;
    //  A 4 year time block is 1461 days
    NumberOf4s = ElapsedDays / 1461;
    ElapsedDays -= NumberOf4s * 1461;
    return (NumberOf400s * 400) + (NumberOf100s * 100) +
           (NumberOf4s * 4) + (ElapsedDays * 100 + 75) / 36525;
}

//
// convert millisecons to days + milleseconds
//
__inline void TicksToDaysAndFraction (ULONGLONG ui64Ticks, LPDWORD lpElapsedDays, LPDWORD lpMilliseconds)
{
    ULONGLONG uiTicksInFullDay;
    *lpElapsedDays   = (DWORD) (ui64Ticks / TICKS_IN_A_DAY);
    uiTicksInFullDay = (ULONGLONG) *lpElapsedDays * TICKS_IN_A_DAY;
    *lpMilliseconds  = (DWORD) (ui64Ticks - uiTicksInFullDay);
}

//
// convert # of ticks since 1601/1/1, 00:00:00 to time
//
BOOL NKTicksToSystemTime (ULONGLONG ui64Ticks, SYSTEMTIME *lpst)
{
    BOOL fRet = (ui64Ticks <= MAX_TIME_IN_TICKS);

    if (fRet) {
        ULONG Days;
        ULONG Years;
        ULONG Minutes;
        ULONG Seconds;
        ULONG Milliseconds;

        TicksToDaysAndFraction (ui64Ticks, &Days, &Milliseconds);

        lpst->wDayOfWeek = (WORD)((Days + WEEKDAY_OF_1601) % 7);
        Years = ElapsedDaysToYears(Days);
        Days = Days - ElapsedYearsToDays(Years);
        if (IsLeapYear(Years + 1)) {
            lpst->wMonth = (WORD)(LeapYearDayToMonth[Days] + 1);
            Days = Days - LeapYearDaysBeforeMonth[lpst->wMonth-1];
        } else {
            lpst->wMonth = (WORD)(NormalYearDayToMonth[Days] + 1);
            Days = Days - NormalYearDaysBeforeMonth[lpst->wMonth-1];
        }
        Seconds = Milliseconds/1000;
        lpst->wMilliseconds = (WORD)(Milliseconds % 1000);
        Minutes = Seconds / 60;
        lpst->wSecond = (WORD)(Seconds % 60);
        lpst->wHour = (WORD)(Minutes / 60);
        lpst->wMinute = (WORD)(Minutes % 60);
        lpst->wYear = (WORD)(Years + 1601);
        lpst->wDay = (WORD)(Days + 1);
    }
    return fRet;
}

//
// convert system time to file time
//
BOOL NKSystemTimeToFileTime (const SYSTEMTIME *lpst, LPFILETIME lpft) 
{
    BOOL fRet = IsValidSystemTime(lpst);

    if (fRet) {
        ULARGE_INTEGER ui64_100ns;
        ui64_100ns.QuadPart = NKSystemTimeToTicks (lpst) * MS_TO_100NS;
        lpft->dwHighDateTime = ui64_100ns.HighPart;
        lpft->dwLowDateTime  = ui64_100ns.LowPart;
    } else {
        SetLastError (ERROR_INVALID_PARAMETER);
    }

    return fRet;
}

//
// convert file time to system time
//
BOOL NKFileTimeToSystemTime (const FILETIME *lpft, LPSYSTEMTIME lpst) 
{
    ULARGE_INTEGER ui64_100ns;
    BOOL           fRet;

    ui64_100ns.HighPart = lpft->dwHighDateTime;
    ui64_100ns.LowPart  = lpft->dwLowDateTime;

    fRet = NKTicksToSystemTime (ui64_100ns.QuadPart/MS_TO_100NS, lpst);
    if (!fRet) {
        SetLastError (ERROR_INVALID_PARAMETER);
    }

    return fRet;
}

