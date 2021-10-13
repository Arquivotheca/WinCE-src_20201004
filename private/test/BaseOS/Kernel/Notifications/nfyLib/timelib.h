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

#ifndef    _TIMELIB_H
#include "helpers.h"

#define    DumpST(st)                DebugDump(_T(#st) _T(" = %d/%d/%d - %d:%d:%d:%d (YYYY/MM/DD - hh:mm:ss:ms)\n"), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds)

#define MAX_DAY_SECS            2419200 // secs in 28 days
// #define RandInt(max, min)        ((INT)((Random() % (max - min)) + min))
#define RandInt(max, min)          ((INT)((rand() % ((INT)max - (INT)min + 1)) + (INT)min))
#define RAND_TIME_MAX_YEAR        2099 // max year allowed
#define RAND_TIME_MIN_YEAR        2000 // min year allowed
inline UINT SaveRandUint(const UINT max, const UINT min)
{
    UINT uiRand = 0;
    rand_s(&uiRand);
    return (uiRand % (max - min + 1)) + min;
}

#define LOOP_SLEEP                20

static const WORD c_wYear[]         = { 3, 1999, 2000, 2001 };
static const WORD c_wMonth[]        = { 2, 1, 12 };
static const WORD c_wDayOfWeek[]    = { 2, 0, 6 };
static const WORD c_wDay[]          = { 1, 1 };
static const WORD c_wHour[]         = { 2, 0, 23 };
static const WORD c_wMinute[]       = { 2, 0, 59 };
static const WORD c_wSecond[]       = { 2, 0, 59 };
static const WORD c_wMillisecond[]  = { 2, 0, 999 };

static const WORD *c_TimeBoundaries[] = {
    c_wYear, c_wMonth, c_wDayOfWeek, c_wHour, c_wMinute, c_wSecond, c_wMillisecond
};

#define TIMEBNDRY_LEN  sizeof( c_TimeBoundaries ) / sizeof( WORD * )


//
// functions for SYSTEMTIME arithmetic
//

BOOL AddSecondsToFileTime( LPFILETIME lpFileTime, DWORD dwSeconds );
BOOL SubtractSecondsFromFileTime( LPFILETIME lpFileTime, DWORD dwSeconds );
BOOL AddSecondsToSystemTime( LPSYSTEMTIME pstTime, DWORD dwSeconds );
BOOL SubtractSecondsFromSystemTime( LPSYSTEMTIME pstTime, DWORD dwSeconds );

BOOL PrintSystemTime( LPSYSTEMTIME pst );

DWORD SystemTimeDiff(SYSTEMTIME stTime1, SYSTEMTIME stTime2);
BOOL GetRandomBadTime(LPSYSTEMTIME pst);
BOOL GetRandomSystemTime(LPSYSTEMTIME pst, BOOL fLimit);
BOOL GetRandomBoundaryTime(LPSYSTEMTIME pst, BOOL fLimit);

#define    _TIMELIB_H
#endif
