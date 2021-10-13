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
 *        Core DLL filetime / systemtime code
 *
 *
 * Module Name:
 *
 *        time.c
 *
 * Abstract:
 *
 *        This file implements Core DLL system and file time functions
 *
 * Revision History:
 *
 */
#ifdef KCOREDLL
#define WIN32_CALL(type, api, args) (*(type (*) args) g_ppfnWin32Calls[W32_ ## api])
#else
#define WIN32_CALL(type, api, args) IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)
#endif

#include <windows.h>
#include <memory.h>
#include <notify.h>
#include <kerncmn.h>

#define NKGetTime                   WIN32_CALL(BOOL, NKGetTime, (LPSYSTEMTIME pst, DWORD dwType))
#define NKSetTime                   WIN32_CALL(BOOL, NKSetTime, (const SYSTEMTIME *pst, DWORD dwType, LONGLONG *pi64Delta))
#define NKGetTimeAsFileTime         WIN32_CALL(BOOL, NKGetTimeAsFileTime, (LPFILETIME pft, DWORD dwType))
#define NKFileTimeToSystemTime      WIN32_CALL(BOOL, FileTimeToSystemTime, (const FILETIME *lpft, LPSYSTEMTIME lpst))
#define NKSystemTimeToFileTime      WIN32_CALL(BOOL, SystemTimeToFileTime, (const SYSTEMTIME *lpst, LPFILETIME lpft))
#define NKGetRawTime                WIN32_CALL(BOOL, CeGetRawTime, (ULONGLONG *pi64Millsec))

extern void WINAPI I_BatteryNotifyOfTimeChange(BOOL fForward, FILETIME *pftDelta);

#define MINUTE_TO_100NS                 600000000L

#define GetKeyHKLM(valname,keyname,lptype,data,lplen)   RegQueryValueExW(HKEY_LOCAL_MACHINE,valname,(LPDWORD)keyname,lptype,(LPBYTE)data,lplen)
#define OpenKeyHKLM(keyname,pkeyret)                    RegOpenKeyExW(HKEY_LOCAL_MACHINE,keyname,0, 0,pkeyret)

#define GetKey(keyin,valname,lptype,data,lplen)         RegQueryValueExW(keyin,valname,0,lptype,(LPBYTE)data,lplen)
#define OpenKey(keyin,keyname,pkeyret)                  RegOpenKeyExW(keyin,keyname,0, 0,pkeyret)

static BOOL OffsetFileTimeWithBias (const FILETIME *pftIn, LPFILETIME pftOut, long bias)
{
    LARGE_INTEGER i64;
    LONGLONG i64bias = bias;        // bias is in minutes
    i64bias *= MINUTE_TO_100NS;     // convert it to 100ns units (FILETIME unit)

    // off set time by bias
    i64.HighPart = pftIn->dwHighDateTime;
    i64.LowPart  = pftIn->dwLowDateTime;
    i64.QuadPart += i64bias;

    pftOut->dwHighDateTime = i64.HighPart;
    pftOut->dwLowDateTime  = i64.LowPart;

    return TRUE;
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | FileTimeToLocalFileTime | Converts a file time based on the
    Coordinated Universal Time (UTC) to a local file time.
    @parm CONST FILETIME * | lpFileTime | address of UTC file time to convert
    @parm LPFILETIME | lpLocalFileTime | address of converted file time

    @comm Follows the Win32 reference description without restrictions or modifications.

*/
BOOL WINAPI FileTimeToLocalFileTime(const FILETIME * lpft, LPFILETIME lpftLocal)
{
    return OffsetFileTimeWithBias (lpft, lpftLocal, -UserKInfo[KINX_TIMEZONEBIAS]);
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | LocalFileTimeToFileTime | Converts a local file time to a file time based
    on the Coordinated Universal Time (UTC).
    @parm CONST FILETIME * | lpftLocal | address of local file time to convert
    @parm LPFILETIME | lpft | address of converted file time

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL WINAPI LocalFileTimeToFileTime(const FILETIME *lpftLocal, LPFILETIME lpft)
{
    return OffsetFileTimeWithBias (lpftLocal, lpft, UserKInfo[KINX_TIMEZONEBIAS]);
}

void GetCurrentFT (LPFILETIME lpft)
{
    NKGetTimeAsFileTime (lpft, TM_SYSTEMTIME);
}

/*
    @doc BOTH EXTERNAL

    @func LONG | CompareFileTime | Compares two 64-bit file times.
    @parm CONST FILETIME * | lpFileTime1 | address of first file time
    @parm CONST FILETIME * | lpFileTime2 | address of second file time

    @devnote CHOOSE COMM TAG FOR ONLY ONE OF THE FOLLOWING:
    @comm Follows the Win32 reference description without restrictions or modifications.

*/
LONG WINAPI CompareFileTime(const FILETIME *lpft1, const FILETIME *lpft2) {
    if (lpft1->dwHighDateTime < lpft2->dwHighDateTime)
        return -1;
    else if (lpft1->dwHighDateTime > lpft2->dwHighDateTime)
        return 1;
    else if (lpft1->dwLowDateTime < lpft2->dwLowDateTime)
        return -1;
    else if (lpft1->dwLowDateTime > lpft2->dwLowDateTime)
        return 1;
    return 0;
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | SystemTimeToFileTime | Converts a system time to a file time.
    @parm CONST SYSTEMTIME * | lpst | address of system time to convert
    @parm LPFILETIME | lpft | address of buffer for converted file time

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL WINAPI SystemTimeToFileTime(const SYSTEMTIME *pst, LPFILETIME pft)
{
    return NKSystemTimeToFileTime (pst, pft);
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | FileTimeToSystemTime | Converts a 64-bit file time to system time format.
    @parm CONST FILETIME * | lpFileTime | pointer to file time to convert
    @parm LPSYSTEMTIME | lpSystemTime | pointer to structure to receive system time

    @comm Follows the Win32 reference description without restrictions or modifications.

*/
BOOL WINAPI FileTimeToSystemTime(const FILETIME *pft, LPSYSTEMTIME pst)
{
    return NKFileTimeToSystemTime (pft, pst);
}

/*
    @doc BOTH EXTERNAL

    @func VOID | GetSystemTime | Retrieves the current system date and time.
    The system time is expressed in Coordinated Universal Time (UTC).
    @parm LPSYSTEMTIME | lpst | address of system time structure

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
VOID WINAPI GetSystemTime (LPSYSTEMTIME lpSystemTime)
{
    NKGetTime (lpSystemTime, TM_SYSTEMTIME);
}


/*
    @doc BOTH EXTERNAL

    @func VOID | GetSystemTimeAsFileTime | Retrieves the current system date and time in FILETIME format.
    @parm LPFILETIME | lpft | address of FILETIME structure

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
VOID WINAPI GetSystemTimeAsFileTime (LPFILETIME lpft)
{
    NKGetTimeAsFileTime (lpft, TM_SYSTEMTIME);
}

#define DSTEVENT          _T("ShellDSTEvent")

static BOOL DoSetTime (const SYSTEMTIME *pst, DWORD dwType)
{
    LARGE_INTEGER liDelta;
    HANDLE hEvent = NULL;

    BOOL fRet = NKSetTime (pst, dwType, &liDelta.QuadPart);
    if (fRet) {
        BOOL fForward = (liDelta.QuadPart >= 0);
        if (!fForward) {
            liDelta.QuadPart = -liDelta.QuadPart;
        }

        // Workaround for problem with SetSystemTime when the new time
        // is passing the DST start/end time backwardly. In this case
        // the DST event will not be triggered automatically so only the
        // DST state and the bias is changed by DST service. Since the RTC
        // stores the local time so the system time after that will be
        // calcualted using the new bias which is not correct.
        // Solution: maunally set the DSTEVENT to trigger DST service 
        // local time calculation if the time is set backwardly.
        if (!fForward && TM_SYSTEMTIME == dwType)
        {
            hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, DSTEVENT);
            if (hEvent != INVALID_HANDLE_VALUE)
            {
                SetEvent(hEvent);
                CloseHandle(hEvent);
            }
        }

        I_BatteryNotifyOfTimeChange (fForward, (LPFILETIME) &liDelta);

        // if successful, broadcast a notification
        CeEventHasOccurred(NOTIFICATION_EVENT_TIME_CHANGE, NULL);
    }

    return fRet;
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | SetSystemTime | Sets the current system time and date. The system time
    is expressed in Coordinated Universal Time (UTC).
    @parm CONST SYSTEMTIME * | lpst | address of system time to set

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL WINAPI SetSystemTime (const SYSTEMTIME *lpSystemTime)
{
    return DoSetTime (lpSystemTime, TM_SYSTEMTIME);
}

/*
    @doc BOTH EXTERNAL

    @func VOID | GetLocalTime | Retrieves the current local date and time.
    @parm LPSYSTEMTIME |lpst | address of system time structure

    @comm Follows the Win32 reference description without restrictions or modifications.

*/
VOID WINAPI GetLocalTime (LPSYSTEMTIME lpSystemTime)
{
    NKGetTime (lpSystemTime, TM_LOCALTIME);
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | SetLocalTime | Sets the current local time and date.
    @parm CONST SYSTEMTIME * | lpst | address of local time to set

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL WINAPI SetLocalTime (const SYSTEMTIME *lpSystemTime)
{
    return DoSetTime (lpSystemTime, TM_LOCALTIME);
}

//
// Get monolithically increasing time, even with time change
//
BOOL CeGetRawTime (ULONGLONG *pui64Millsec)
{
    return NKGetRawTime (pui64Millsec);
}

// to retrieve RawTimeOffset
BOOL CeGetRawTimeOffset (LONGLONG *pi64Offset) 
{
    *pi64Offset = *((LONGLONG *) (PUserKData + RAWTIMEOFFSET_OFFSET));
    return TRUE;
}


/*
    @doc BOTH EXTERNAL

    @func DWORD | GetTimeZoneInformation | Retrieves the current time-zone parameters.
    These parameters control the translations between Coordinated Universal Time (UTC)
    and local time.
    @parm LPTIME_ZONE_INFORMATION | lptzi | address of time-zone settings

    @devnote CHOOSE COMM TAG FOR ONLY ONE OF THE FOLLOWING:
    @comm Follows the Win32 reference description with the following restrictions:
            Always return TIME_ZONE_ID_UNKNOWN;

*/
#define TZIStrCpyW(D,S) memcpy(D,S,sizeof(S))

typedef struct tagTZREG {
    LONG    Bias;
    LONG    StandardBias;
    LONG    DaylightBias;
    SYSTEMTIME StandardDate;
    SYSTEMTIME DaylightDate;
} TZREG;

const TCHAR cszTimeZones[] = L"Time Zones";

void GetDefaultTimeZoneInformation (LPTIME_ZONE_INFORMATION lpTzi)
{
    DWORD type, size1, size2;
    HKEY hk1=0, hk2=0;
    TZREG tzr;
    WCHAR TimezoneName[32];

    memset(lpTzi, 0, sizeof(TIME_ZONE_INFORMATION));
    memset(&tzr, 0, sizeof(tzr));
    size1 = sizeof(TimezoneName);
    size2 = sizeof(tzr);

    // see if we have a value "Default" under HKLM\Time Zones
    if( (ERROR_SUCCESS==OpenKeyHKLM(L"Time Zones", &hk1)) &&
        (ERROR_SUCCESS==GetKey(hk1, L"Default", &type, TimezoneName, &size1)) &&
        (ERROR_SUCCESS==OpenKey(hk1, TimezoneName, &hk2)) &&
        (ERROR_SUCCESS==GetKey(hk2, L"TZI", &type, &tzr, &size2)) )
    {
        // Read the value "TZI" and "Dlt" under HKLM\Time Zones\<time zone std name>
        size1 = sizeof(lpTzi->StandardName);
        GetKey(hk2,L"Std",&type,lpTzi->StandardName,&size1);
        lpTzi->StandardName[31] = 0;    // force NULL termination
        size1 = sizeof(lpTzi->DaylightName);
        GetKey(hk2,L"Dlt",&type,lpTzi->DaylightName,&size1);
        lpTzi->DaylightName[31] = 0;    // force NULL termination
        lpTzi->Bias = tzr.Bias;
        lpTzi->StandardBias = tzr.StandardBias;
        lpTzi->DaylightBias = tzr.DaylightBias;
        lpTzi->StandardDate = tzr.StandardDate;
        lpTzi->DaylightDate = tzr.DaylightDate;
    }
    else
    {
        // Default to Redmond, WA for now
        lpTzi->Bias = 480;
        TZIStrCpyW(lpTzi->StandardName,L"Pacific Standard Time");   // MUST Be less than 32 chars
        lpTzi->StandardDate.wYear = 0;
        lpTzi->StandardDate.wMonth = 10;
        lpTzi->StandardDate.wDayOfWeek = 0;
        lpTzi->StandardDate.wDay = 5;
        lpTzi->StandardDate.wHour = 2;
        lpTzi->StandardDate.wMinute = 0;
        lpTzi->StandardDate.wSecond = 0;
        lpTzi->StandardDate.wMilliseconds = 0;
        lpTzi->StandardBias = 0;
        TZIStrCpyW(lpTzi->DaylightName,L"Pacific Daylight Time");   // MUST Be less than 32 chars
        lpTzi->DaylightDate.wYear = 0;
        lpTzi->DaylightDate.wMonth = 4;
        lpTzi->DaylightDate.wDayOfWeek = 0;
        lpTzi->DaylightDate.wDay = 1;
        lpTzi->DaylightDate.wHour = 2;
        lpTzi->DaylightDate.wMinute = 0;
        lpTzi->DaylightDate.wSecond = 0;
        lpTzi->DaylightDate.wMilliseconds = 0;
        lpTzi->DaylightBias = -60;
    }
    if(hk1) RegCloseKey(hk1);
    if(hk2) RegCloseKey(hk2);

}

DWORD WINAPI GetTimeZoneInformation (LPTIME_ZONE_INFORMATION lpTzi)
{
    DWORD type;
    DWORD size = sizeof(TIME_ZONE_INFORMATION);
    // check for existing time-zone struct
    if (GetKeyHKLM(L"TimeZoneInformation",L"Time",&type,(LPBYTE)lpTzi,&size))
    {
        // fallback to default
        GetDefaultTimeZoneInformation(lpTzi);
    }
    if (lpTzi->Bias + lpTzi->StandardBias == UserKInfo[KINX_TIMEZONEBIAS])
        return TIME_ZONE_ID_STANDARD;
    else if (lpTzi->Bias + lpTzi->DaylightBias == UserKInfo[KINX_TIMEZONEBIAS])
        return TIME_ZONE_ID_DAYLIGHT;
    return TIME_ZONE_ID_UNKNOWN;
}

static BOOL IsValidTZISystemTime (const SYSTEMTIME *lpst)
{
    FILETIME ft;
    return !lpst->wMonth
        || (!lpst->wYear && (lpst->wDayOfWeek <= 6) && (lpst->wDay >= 1) && (lpst->wDay <= 5))
        || NKSystemTimeToFileTime (lpst, &ft);
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | SetTimeZoneInformation | Sets the current time-zone parameters.
    These parameters control translations from Coordinated Universal Time (UTC) to local time.
    @parm CONST TIME_ZONE_INFORMATION * | lptzi | address of time-zone settings

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL WINAPI SetTimeZoneInformation (const TIME_ZONE_INFORMATION *lpTzi) {
    DWORD dwDisp;
    HKEY  hKey=NULL;

    if (!IsValidTZISystemTime(&lpTzi->StandardDate) ||
        !IsValidTZISystemTime(&lpTzi->DaylightDate)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    SetTimeZoneBias(lpTzi->Bias+lpTzi->StandardBias,lpTzi->Bias+lpTzi->DaylightBias);

    if(ERROR_SUCCESS==RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Time", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisp) && hKey) {
        if(ERROR_SUCCESS==RegSetValueExW(hKey, L"TimeZoneInformation", 0, REG_BINARY, (LPBYTE)lpTzi,sizeof(TIME_ZONE_INFORMATION))) {
            CeEventHasOccurred(NOTIFICATION_EVENT_TZ_CHANGE, NULL);
            RegCloseKey(hKey);
            return TRUE;
        }
        RegCloseKey(hKey);
    }
    SetLastError(ERROR_OUTOFMEMORY);
    return FALSE;
}

