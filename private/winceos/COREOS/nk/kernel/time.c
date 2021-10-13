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
 *
 * Module Name:
 *
 *        time.c
 *
 * Abstract:
 *
 *        This file implements time/alarm functions
 *
 * Revision History:
 *
 */

#include <kernel.h>
#include <tzinit.h>

#define WEEKDAY_OF_1601         1

#define FIRST_VALID_YEAR        1601        // first valid year 
#define TICKS_IN_A_DAY          86400000    // # of ticks in a day
#define TICKS_IN_A_YEAR         31536000000 // # of ticks in a year 
#define MS_TO_100NS             10000       // multiplier to convert time in ticks to time in 100ns
#define MINUTES_TO_MILLS        60000       // convert minutes to milliseconds
#define DAYS_IN_400_YEARS       146097      // # of days in 400 years
#define DAYS_IN_100_YEARS       36524       // # of days in 100 years
#define IsLeapYear(Y)           (!((Y)%4) && (((Y)%100) || !((Y)%400)))
#define NumberOfLeapYears(Y)    ((Y)/4 - (Y)/100 + (Y)/400)
#define ElapsedYearsToDays(Y)   ((Y)*365 + NumberOfLeapYears(Y))
#define MaxDaysInMonth(Y,M)     (IsLeapYear(Y) \
            ? (LeapYearDaysBeforeMonth[(M) + 1] - LeapYearDaysBeforeMonth[M]) \
            : (NormalYearDaysBeforeMonth[(M) + 1] - NormalYearDaysBeforeMonth[M]))

#define TZ_BIAS_IN_MILLS        ((LONG) KInfoTable[KINX_TIMEZONEBIAS] * MINUTES_TO_MILLS)

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
SYSTEMTIME gMaxSystemtime = { 30828,9,4,14,2,48,5,477 };

//
// soft RTC related
//
static ULONGLONG        gBaseUTCInTicks;            // Base system time of soft RTC
static ULONGLONG        gLastUTCInTickRead;         // Last time (system time) soft RTC is read
                                                    // to make sure RTC not going backward when sync with hardware
static DWORD            gTickCountAtBase;           // Tick Count corresponding to gBaseUTCInTicks
                                                    // i.e. Current System Time = Base System Time + time delta
                                                    //                          = gBaseUTCInTicks + (Current Tick Count - gTickCountAtBase)

static BOOL             gfUseSoftRTC;

//
// Raw Time related
//
#define gOfstRawTime    (g_pKData->i64RawOfst)
static ULONGLONG        ui64PrevRawTime;            // last "raw time" recorded, for rollover detection

//
// general
//
static DWORD dwYearRollover;


static BOOL UpdateAndSignalAlarm (void);

//
// convert ticks in system time to local time
//
__inline static ULONGLONG SystemTimeToLocalTime(ULONGLONG ui64Ticks)
{
    return ui64Ticks - TZ_BIAS_IN_MILLS;
}

//
// convert ticks in local time to system time
//
__inline static ULONGLONG LocalTimeToSystemTime(ULONGLONG ui64Ticks)
{
    return ui64Ticks + TZ_BIAS_IN_MILLS;
}

//
// convert days + milliseconds to milliseconds
//
static ULONGLONG DayAndFractionToTicks (ULONG ElapsedDays, ULONG Milliseconds)
{
    return (ULONGLONG) ElapsedDays * TICKS_IN_A_DAY + Milliseconds;
}

//
// convert millisecons to days + milleseconds
//
static void TicksToDaysAndFraction (ULONGLONG ui64Ticks, LPDWORD lpElapsedDays, LPDWORD lpMilliseconds)
{
    ULONGLONG uiTicksInFullDay;
    *lpElapsedDays   = (DWORD) (ui64Ticks / TICKS_IN_A_DAY);
    uiTicksInFullDay = (ULONGLONG) *lpElapsedDays * TICKS_IN_A_DAY;
    *lpMilliseconds  = (DWORD) (ui64Ticks - uiTicksInFullDay);
}

//
// calculate # of years given the number of days
//
static ULONG ElapsedDaysToYears (ULONG ElapsedDays)
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
// test if a system time is valid
// MAX_FILETIME is 2:48:05.477 9/14/30828, so we'll use that as the max here as well
//
static BOOL IsValidSystemTime(const SYSTEMTIME *lpst) {
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
// get system time from hardware RTC in ticks from 1601/1/1, 00:00:00
//
static ULONGLONG GetUTCInTicks (void)
{
    ULONGLONG ui64CurrTime;
    SYSTEMTIME st;
    DEBUGCHK (OwnCS (&rtccs));

    // read RTC
    OEMGetRealTime (&st);
    DEBUGCHK(IsValidSystemTime(&st));

    // takes # of rollover year into account
    st.wYear += (WORD) dwYearRollover;

    ui64CurrTime = LocalTimeToSystemTime(NKSystemTimeToTicks(&st));

    if (((ui64CurrTime - gOfstRawTime + TICKS_IN_A_YEAR) < ui64PrevRawTime) 
        && g_pOemGlobal->dwYearsRTCRollover) {
       
        // RTC rollover
        RETAILMSG (1, (L"RTC Rollover detected (year = %d)\r\n", st.wYear));

        dwYearRollover  += g_pOemGlobal->dwYearsRTCRollover;
        st.wYear        += (WORD) g_pOemGlobal->dwYearsRTCRollover; // take rollover into account
        ui64CurrTime     = LocalTimeToSystemTime(NKSystemTimeToTicks(&st));

        DEBUGCHK ((ui64CurrTime - gOfstRawTime) >= ui64PrevRawTime);
    }

    // if ui64PrevRawTime is 0, RTC haven't being intialized yet.
    if (ui64PrevRawTime) {
        ui64PrevRawTime = (ui64CurrTime - gOfstRawTime);
    }
    return ui64CurrTime;
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
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
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
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
    }

    return fRet;
}

//
// get local/system time in ticks from 1601/1/1, 00:00:00
//
ULONGLONG GetTimeInTicks (DWORD dwType)
{
    ULONGLONG ui64Ticks;
    DEBUGCHK ((TM_LOCALTIME == dwType) || (TM_SYSTEMTIME == dwType));

    EnterCriticalSection (&rtccs);
    if (gfUseSoftRTC) {
        // NOTE: Tick count can wrap. The offset (OEMGetTickCount - gTickCountAtBase) must
        //       be calculated 1st before adding the base tick.
        ui64Ticks = gBaseUTCInTicks + (ULONG) (OEMGetTickCount () - gTickCountAtBase);

        // last time GetxxxTime is called. Should never set gTickCountAtBase to anything smaller than this
        // when refreshing RTC, or RTC can go backward.
        gLastUTCInTickRead = ui64Ticks;
    } else {
        // use a local copy so we won't except while holding RTC CS
        ui64Ticks = GetUTCInTicks ();
    }
    LeaveCriticalSection (&rtccs);
    if (TM_LOCALTIME == dwType) {
        ui64Ticks = SystemTimeToLocalTime(ui64Ticks);
    }

    return ui64Ticks;
}

//
// set local time in ticks
//      pui64Delta returns the time changed.
//      fBiasChange indicates whether this function is called to set new time (SetLocal/SystemTime)
//        or to set new time zone bias (SetTimeZoneInformation/SetDaylightTime).
//
static BOOL SetLocalTimeInTicks (const SYSTEMTIME* pst, ULONGLONG *pui64Delta, BOOL fBiasChange)
{
    BOOL      fRet;
    ULONGLONG ui64TickCurr = 0;
    SYSTEMTIME st = *pst;

    DEBUGCHK (OwnCS (&rtccs));

    // get the current RTC
    if (!fBiasChange)
    {
        ui64TickCurr = GetTimeInTicks(TM_SYSTEMTIME);
    }

    // update local time
    fRet = OEMSetRealTime (&st);

    if (fRet) {
        // reset years rollover
        dwYearRollover = 0;
    } else if (g_pOemGlobal->dwYearsRTCRollover) {
        WORD wYearOrig = st.wYear;
        DWORD dwOldYearRollover = dwYearRollover;
        dwYearRollover = 0;
        fRet = FALSE;

        // try the time with rollover
        while (!fRet && st.wYear >= g_pOemGlobal->dwYearsRTCRollover) {
            st.wYear -= (WORD) g_pOemGlobal->dwYearsRTCRollover;
            dwYearRollover += g_pOemGlobal->dwYearsRTCRollover;
            if (st.wYear + dwYearRollover != wYearOrig)
                break;
            fRet = OEMSetRealTime (&st);
        }

        if (!fRet) {
            dwYearRollover = dwOldYearRollover;
        }
    }
    
    if (fRet && !fBiasChange) {
        WORD wYear = st.wYear;
        
        // re-read rtc, in case hardware changed the time we set.
        VERIFY (OEMGetRealTime (&st));
        DEBUGCHK(IsValidSystemTime(&st));
        if (wYear > st.wYear) {
            // got preempted between Set/GetRealTime, and year rollover
            dwYearRollover += g_pOemGlobal->dwYearsRTCRollover;
        }

        // calculate new RTC in ticks and update soft RTC
        st.wYear        += (WORD) dwYearRollover;
        gBaseUTCInTicks = LocalTimeToSystemTime(NKSystemTimeToTicks(&st));
        gTickCountAtBase = OEMGetTickCount ();
        gLastUTCInTickRead = 0;    // reset "last time reading RTC", as time had changed

        // calculate/update delta and raw time offset
        *pui64Delta   = gBaseUTCInTicks - ui64TickCurr;
        gOfstRawTime += *pui64Delta;

    }

    return fRet;
}

//
// helper function to set a new time specified in ticks
//
BOOL SetTimeHelper (ULONGLONG ui64Ticks, DWORD dwType, LONGLONG *pi64Delta)
{
    BOOL fRet;
    SYSTEMTIME st;
    BOOL fBiasChange = FALSE;

    DEBUGCHK (OwnCS (&rtccs));
    DEBUGCHK ((TM_LOCALTIME == dwType) || (TM_SYSTEMTIME == dwType) || (TM_TIMEBIAS == dwType));
    
    if (TM_TIMEBIAS == dwType)
    {
        // For tz bias change, set fBiasChange flag, and convert system time to local time
        fBiasChange = TRUE;
        ui64Ticks = SystemTimeToLocalTime(ui64Ticks);
    }
    else if (TM_SYSTEMTIME == dwType)
    {
        // convert system time to local time
        ui64Ticks = SystemTimeToLocalTime(ui64Ticks);
    }
    
    fRet = NKTicksToSystemTime (ui64Ticks, &st);
    
    if (fRet) {
        
        ULONGLONG i64Delta = 0;

        fRet = SetLocalTimeInTicks (&st, &i64Delta, fBiasChange);
        
        if (fRet && !fBiasChange) {
            // refresh kernel alarm
            UpdateAndSignalAlarm ();
        }

        if (fRet && pi64Delta) {
            *pi64Delta = (LONGLONG) i64Delta;
        }
    }

    return fRet;
}
//
// refresh soft RTC, return the current (local) time in ticks since 1601/1/1, 00:00:00 to time
//
ULONGLONG NKRefreshRTC (void)
{
    EnterCriticalSection (&rtccs);
    gBaseUTCInTicks = GetUTCInTicks();
    
    if (gfUseSoftRTC) {
        gTickCountAtBase = OEMGetTickCount ();
        
        if (gBaseUTCInTicks < gLastUTCInTickRead)
        {
            // Soft RTC moving slower, catch it up by increasing the gap
            DEBUGCHK(gLastUTCInTickRead - gBaseUTCInTicks < 0xFFFFFFFF);
            gTickCountAtBase -= (DWORD) (gLastUTCInTickRead - gBaseUTCInTicks);
        }
    }
    LeaveCriticalSection (&rtccs);
    return SystemTimeToLocalTime(gBaseUTCInTicks);
}

//
// external interface to get time
//
BOOL NKGetTime (SYSTEMTIME *pst, DWORD dwType)
{
    ULONGLONG ui64Ticks = GetTimeInTicks (dwType);
    VERIFY (NKTicksToSystemTime (ui64Ticks, pst));
    return TRUE;
}

BOOL NKGetTimeAsFileTime (LPFILETIME pft, DWORD dwType)
{
    ULARGE_INTEGER uliTicks;
    uliTicks.QuadPart = GetTimeInTicks (dwType) * MS_TO_100NS;
    pft->dwHighDateTime = uliTicks.HighPart;
    pft->dwLowDateTime  = uliTicks.LowPart;
    return TRUE;
}

//
// external interface to set time
//
BOOL NKSetTime (const SYSTEMTIME *pst, DWORD dwType, LONGLONG *pi64Delta)
{
    BOOL fRet = FALSE;
    PTHREAD pth = pCurThread;
    DWORD dwErr = ERROR_INVALID_PARAMETER;

    fRet = IsValidSystemTime (pst);
    if (fRet) {
        ULONGLONG ui64Ticks;
        LONGLONG i64Delta;
        SYSTEMTIME st = *pst;
        st.wMilliseconds = 0;         // system time granularity is second.
        ui64Ticks = NKSystemTimeToTicks (&st);
        EnterCriticalSection (&rtccs);
        fRet = SetTimeHelper (ui64Ticks, dwType, &i64Delta);
        LeaveCriticalSection (&rtccs);
        *pi64Delta = i64Delta;
    }

    if (!fRet) {
        KSetLastError (pth, dwErr);
    }

    return fRet;
}

//
// external interface to get raw time
//
BOOL NKGetRawTime (ULONGLONG *pui64Ticks)
{
    ULONGLONG ui64Ticks;
    EnterCriticalSection (&rtccs);
    ui64Ticks = GetTimeInTicks(TM_SYSTEMTIME) - gOfstRawTime;
    LeaveCriticalSection (&rtccs);

    *pui64Ticks = ui64Ticks;
    return TRUE;
}

//------------------------------------------------------------------------------
// alarm related code
//------------------------------------------------------------------------------

static PHDATA     phdAlarmEvent;
static SYSTEMTIME CurAlarmTime;

void SignalKernelAlarm (void)
{
    DEBUGCHK (OwnCS (&rtccs));
    if (phdAlarmEvent) {
        EVNTModify (GetEventPtr (phdAlarmEvent), EVENT_SET);
        UnlockHandleData (phdAlarmEvent);
        phdAlarmEvent = NULL;
    }
}

static BOOL UpdateAndSignalAlarm (void)
{
    ULONGLONG ui64Ticks;
    SYSTEMTIME st;
    DEBUGCHK (OwnCS (&rtccs));

    ui64Ticks = NKRefreshRTC ();

    if (phdAlarmEvent) {

        ULONGLONG ui64CurAlarmTick = NKSystemTimeToTicks (&CurAlarmTime);

        if (ui64CurAlarmTick <= ui64Ticks + g_pOemGlobal->dwAlarmResolution) {
            // within alarm resolution time, fire the alarm
            SignalKernelAlarm ();
            return TRUE;
        } else {
            // pass a local copy of the alarm time; otherwise there is potential for oem code
            // to update the CurAlarmTime leading to failures downstream
            memcpy (&st, &CurAlarmTime, sizeof(st));

            // adjust for rollover if present
            if (g_pOemGlobal->dwYearsRTCRollover) {
                SYSTEMTIME stNow;
                NKGetTime (&stNow, TM_SYSTEMTIME);
                if ((st.wYear/g_pOemGlobal->dwYearsRTCRollover) == (stNow.wYear/g_pOemGlobal->dwYearsRTCRollover)) {
                    st.wYear -= (WORD) dwYearRollover;
                } else if ((DWORD)(st.wYear - stNow.wYear) < (g_pOemGlobal->dwYearsRTCRollover - 1)) {
                    st.wYear -= (WORD) dwYearRollover + (WORD) g_pOemGlobal->dwYearsRTCRollover;
                }
            }

            return OEMSetAlarmTime (&st);
        }
    }
    return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
NKRefreshKernelAlarm (void)
{
    DWORD dwErr = 0;
    PTHREAD pth = pCurThread;
    
    DEBUGMSG(ZONE_ENTRY,(L"NKRefreshKernelAlarm entry\r\n"));

    EnterCriticalSection(&rtccs);
    UpdateAndSignalAlarm ();
    LeaveCriticalSection(&rtccs);

    if (dwErr) {
        KSetLastError (pth, dwErr);
    }
    
    DEBUGMSG(ZONE_ENTRY,(L"NKRefreshKernelAlarm exit dwErr: 0x%8.8lx\r\n", dwErr));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
NKSetKernelAlarm(
    HANDLE hAlarm,
    const SYSTEMTIME *lpst
    )
{
    BOOL fRet = FALSE;
    SYSTEMTIME st = *lpst;  // make a local copy so we don't except holding rtc CS
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    PTHREAD pth = pCurThread;
    
    DEBUGMSG(ZONE_ENTRY,(L"NKSetKernelAlarm entry: %8.8lx %8.8lx\r\n",hAlarm,lpst));
    if (IsValidSystemTime (&st)) {
        st.wMilliseconds = 0;               // alarm resolution no less than 1 second.
        EnterCriticalSection(&rtccs);
        if (phdAlarmEvent) {
            UnlockHandleData (phdAlarmEvent);
        }
        phdAlarmEvent = LockHandleData (hAlarm, pActvProc);
        if (phdAlarmEvent) {
            memcpy (&CurAlarmTime, &st, sizeof(SYSTEMTIME));
            fRet = UpdateAndSignalAlarm ();
        }
        LeaveCriticalSection(&rtccs);
    }
    
    if (!fRet) {
        KSetLastError (pth, dwErr);
    }

    DEBUGMSG(ZONE_ENTRY,(L"NKSetKernelAlarm exit\r\n"));
    return fRet;
}

//------------------------------------------------------------------------------
// time-zone/DST related
//------------------------------------------------------------------------------
BOOL
NKSetDaylightTime(
    DWORD dst,
    BOOL fAutoUpdate
    )
{
    BIAS_STRUCT Bias = {dst, g_tzBias.NormalBias, g_tzBias.DaylightBias};
    BOOL fRet = FALSE;
    
    DEBUGMSG(ZONE_ENTRY,(L"NKSetDaylightTime entry: %8.8lx\r\n",dst));

    fRet = BiasChangeHelper (&Bias, TRUE, TRUE);
    
    DEBUGMSG(ZONE_ENTRY,(L"NKSetDaylightTime exit\r\n"));
    return fRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
NKSetTimeZoneBias(
    DWORD dwBias,
    DWORD dwDaylightBias
    )
{
    BIAS_STRUCT Bias = {g_tzBias.InDaylight, dwBias, dwDaylightBias};
    BOOL fRet = FALSE;
    
    DEBUGMSG(ZONE_ENTRY,(L"NKSetTimeZoneBias entry: %8.8lx %8.8lx\r\n",dwBias,dwDaylightBias));

    fRet = BiasChangeHelper (&Bias, TRUE, FALSE);

    DEBUGMSG(ZONE_ENTRY,(L"NKSetTimeZoneBias exit\r\n"));
    return fRet;
}

//------------------------------------------------------------------------------
// initialization
//------------------------------------------------------------------------------

void InitSoftRTC (void)
{
    DWORD type;
    DWORD size = sizeof (DWORD);
    BIAS_STRUCT Bias = {0};
    // Init the kernel timezone bias from bootvars if present
    size = sizeof(BIAS_STRUCT);
    if (NKRegQueryValueExW(HKEY_LOCAL_MACHINE, REGVAL_KTZBIAS, (LPDWORD)REGKEY_BOOTVARS,
                &type, (LPBYTE)&Bias, &size)) {
        // direct query failed, generate bias from timezone info
        InitKTzBiasFromTzInfo(&Bias);
    }
    BiasChangeHelper (&Bias, FALSE, FALSE);

    // Query "HKLM\Platform\SoftRTC" to see if we should use SoftRTC or not.
    // If the query fails, gfUseSoftRTC remains 0 and we'll use Hardware RTC.
    DEBUGCHK (SystemAPISets[SH_FILESYS_APIS]);
    NKRegQueryValueExW (HKEY_LOCAL_MACHINE, REGVAL_SOFTRTC,(LPDWORD) REGKEY_PLATFORM,
                &type, (LPBYTE)&gfUseSoftRTC, &size);
    ui64PrevRawTime = LocalTimeToSystemTime(NKRefreshRTC ());
}
