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
#include "..\..\core\dll\timeutil.c"

#define TZ_BIAS_IN_MILLS        ((LONG) KInfoTable[KINX_TIMEZONEBIAS] * MINUTES_TO_MILLS)

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
            // to update the CurAlarmTime leading to failures downstream.
            // NOTE: CurAlarmTime is assumed to be in local time
            memcpy (&st, &CurAlarmTime, sizeof(st));

            // adjust for rollover if present
            if (g_pOemGlobal->dwYearsRTCRollover) {
                SYSTEMTIME stNow;
                NKGetTime (&stNow, TM_LOCALTIME);
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
// Set RTC alarm. lpst is assumed to be local time.
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
void InitTzBias (void)
{
    DWORD type, size;
    BIAS_STRUCT Bias = {0};

    // Init the kernel timezone bias from bootvars if present
    size = sizeof(BIAS_STRUCT);
    if (NKRegQueryValueExW(HKEY_LOCAL_MACHINE, REGVAL_KTZBIAS, (LPDWORD)REGKEY_BOOTVARS,
                &type, (LPBYTE)&Bias, &size)) {
        // direct query failed, generate bias from timezone info
        InitKTzBiasFromTzInfo(&Bias);
    }
    VERIFY (BiasChangeHelper (&Bias, FALSE, FALSE));
}

void InitSoftRTC (void)
{
    DWORD type = REG_DWORD;
    DWORD size = sizeof(DWORD);

    // Query "HKLM\Platform\SoftRTC" to see if we should use SoftRTC or not.
    // If the query fails, gfUseSoftRTC remains 0 and we'll use Hardware RTC.
    DEBUGCHK (SystemAPISets[SH_FILESYS_APIS]);
    NKRegQueryValueExW (HKEY_LOCAL_MACHINE, REGVAL_SOFTRTC,(LPDWORD) REGKEY_PLATFORM,
                &type, (LPBYTE)&gfUseSoftRTC, &size);
    NKDbgPrintfW (L"SoftRTC %s\r\n", gfUseSoftRTC ? L"enabled" : L"disabled");
    
    ui64PrevRawTime = LocalTimeToSystemTime(NKRefreshRTC ());
}

