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
#include <tzreg.h>
#include <dstioctl.h>

#define NKGetTime                   WIN32_CALL(BOOL, NKGetTime, (LPSYSTEMTIME pst, DWORD dwType))
#define NKSetTime                   WIN32_CALL(BOOL, NKSetTime, (const SYSTEMTIME *pst, DWORD dwType, LONGLONG *pi64Delta))
#define NKGetTimeAsFileTime         WIN32_CALL(BOOL, NKGetTimeAsFileTime, (LPFILETIME pft, DWORD dwType))
#define NKFileTimeToSystemTime      WIN32_CALL(BOOL, FileTimeToSystemTime, (const FILETIME *lpft, LPSYSTEMTIME lpst))
#define NKSystemTimeToFileTime      WIN32_CALL(BOOL, SystemTimeToFileTime, (const SYSTEMTIME *lpst, LPFILETIME lpft))
#define NKGetRawTime                WIN32_CALL(BOOL, CeGetRawTime, (ULONGLONG *pi64Millsec))
DWORD WINAPI xxx_WaitForSingleObject(HANDLE hHandle,  DWORD dwMilliseconds);
#define WaitForSingleObject xxx_WaitForSingleObject

extern void WINAPI I_BatteryNotifyOfTimeChange(BOOL fForward, FILETIME *pftDelta);

#define MINUTE_TO_100NS                 600000000L

#define GetKeyHKLM(valname,keyname,lptype,data,lplen)   RegQueryValueExW(HKEY_LOCAL_MACHINE,valname,(LPDWORD)keyname,lptype,(LPBYTE)data,lplen)
#define OpenKeyHKLM(keyname,pkeyret)                    RegOpenKeyExW(HKEY_LOCAL_MACHINE,keyname,0, 0,pkeyret)

#define GetKey(keyin,valname,lptype,data,lplen)         RegQueryValueExW(keyin,valname,0,lptype,(LPBYTE)data,lplen)
#define OpenKey(keyin,keyname,pkeyret)                  RegOpenKeyExW(keyin,keyname,0, 0,pkeyret)

HANDLE g_hDSTService = INVALID_HANDLE_VALUE;

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
    GetCurrentFT(lpft);
}


static BOOL DoSetTime (const SYSTEMTIME *pst, DWORD dwType)
{
    LARGE_INTEGER liDelta;
    BOOL fRet;

    if (g_hDSTService == INVALID_HANDLE_VALUE)
    {
        g_hDSTService = CreateFile(L"DST0:", GENERIC_READ|GENERIC_WRITE, 0, NULL, 0, 0, NULL);
    }
    if (g_hDSTService != INVALID_HANDLE_VALUE)
    {
        DSTSVC_TIME_CHANGE_INFORMATION TimeChangeInformation;
        TimeChangeInformation.stNew = *pst;
        TimeChangeInformation.dwType = dwType;
        DeviceIoControl(
                g_hDSTService,
                IOCTL_DSTSVC_NOTIFY_TIME_CHANGE,
                (LPVOID)&TimeChangeInformation,
                sizeof(DSTSVC_TIME_CHANGE_INFORMATION),
                NULL,
                0,
                NULL,
                NULL
                );
    }
    fRet = NKSetTime (pst, dwType, &liDelta.QuadPart);
    if (fRet) {
        BOOL fForward = (liDelta.QuadPart >= 0);
        if (!fForward) {
            liDelta.QuadPart = -liDelta.QuadPart;
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
    if( (ERROR_SUCCESS==OpenKeyHKLM(cszTimeZones, &hk1)) &&
        (ERROR_SUCCESS==GetKey(hk1, L"Default", &type, TimezoneName, &size1)) &&
        (ERROR_SUCCESS==OpenKey(hk1, TimezoneName, &hk2)) &&
        (ERROR_SUCCESS==GetKey(hk2, cszTZI, &type, &tzr, &size2)) )
    {
        // Read the value "TZI" and "Dlt" under HKLM\Time Zones\<time zone std name>
        size1 = sizeof(lpTzi->StandardName);
        GetKey(hk2,cszStd,&type,lpTzi->StandardName,&size1);
        lpTzi->StandardName[31] = 0;    // force NULL termination
        size1 = sizeof(lpTzi->DaylightName);
        GetKey(hk2,cszDlt,&type,lpTzi->DaylightName,&size1);
        lpTzi->DaylightName[31] = 0;    // force NULL termination
        lpTzi->Bias = tzr.Bias;
        lpTzi->StandardBias = tzr.StandardBias;
        lpTzi->DaylightBias = tzr.DaylightBias;
        lpTzi->StandardDate = tzr.StandardDate;
        lpTzi->DaylightDate = tzr.DaylightDate;
    }
    else
    {
        // Default to Pacific Time (US & Canada)
        lpTzi->Bias = 480;
        TZIStrCpyW(lpTzi->StandardName,L"Pacific Standard Time");   // MUST Be less than 32 chars
        lpTzi->StandardDate.wYear = 0;
        lpTzi->StandardDate.wMonth = 11;
        lpTzi->StandardDate.wDayOfWeek = 0;
        lpTzi->StandardDate.wDay = 1;
        lpTzi->StandardDate.wHour = 2;
        lpTzi->StandardDate.wMinute = 0;
        lpTzi->StandardDate.wSecond = 0;
        lpTzi->StandardDate.wMilliseconds = 0;
        lpTzi->StandardBias = 0;
        TZIStrCpyW(lpTzi->DaylightName,L"Pacific Daylight Time");   // MUST Be less than 32 chars
        lpTzi->DaylightDate.wYear = 0;
        lpTzi->DaylightDate.wMonth = 3;
        lpTzi->DaylightDate.wDayOfWeek = 0;
        lpTzi->DaylightDate.wDay = 3;
        lpTzi->DaylightDate.wHour = 2;
        lpTzi->DaylightDate.wMinute = 0;
        lpTzi->DaylightDate.wSecond = 0;
        lpTzi->DaylightDate.wMilliseconds = 0;
        lpTzi->DaylightBias = -60;
    }
    if(hk1) RegCloseKey(hk1);
    if(hk2) RegCloseKey(hk2);

}

static BOOL IsValidTZISystemTime (const SYSTEMTIME *lpst)
{
    FILETIME ft;
    return !lpst->wMonth
        || (!lpst->wYear && (lpst->wDayOfWeek <= 6) && (lpst->wDay >= 1) && (lpst->wDay <= 5))
        || NKSystemTimeToFileTime (lpst, &ft);
}

static BOOL IsTZISystemTimeSpecified (const SYSTEMTIME *lpst)
{
    // If StandardDate/DaylightDate not specified, the wMonth member in the SYSTEMTIME structure must be zero. 
    return lpst->wMonth != 0;
}

DWORD WINAPI GetTimeZoneInformation (LPTIME_ZONE_INFORMATION lpTzi)
{
    DWORD type;
    DWORD size = sizeof(TIME_ZONE_INFORMATION);
    
    // check for existing time-zone struct
    if (GetKeyHKLM(cszSystemTimezone,cszSystemTime,&type,(LPBYTE)lpTzi,&size))
    {
        // fallback to default
        GetDefaultTimeZoneInformation(lpTzi);
    }

    if (lpTzi->DaylightBias == 0 ||
        !IsTZISystemTimeSpecified(&lpTzi->StandardDate) ||
        !IsTZISystemTimeSpecified(&lpTzi->DaylightDate))
    {
        return TIME_ZONE_ID_UNKNOWN;
    }
    else if (lpTzi->Bias + lpTzi->StandardBias == UserKInfo[KINX_TIMEZONEBIAS])
    {
        return TIME_ZONE_ID_STANDARD;
    }
    else if (lpTzi->Bias + lpTzi->DaylightBias == UserKInfo[KINX_TIMEZONEBIAS])
    {
        return TIME_ZONE_ID_DAYLIGHT;
    }
    else
    {
        return TIME_ZONE_ID_INVALID; // current time zone doesn't support dst
    }
}

static BOOL Int_SetTimeZoneInformation(const TIME_ZONE_INFORMATION *lpTzi, DWORD dwID)
{
    DWORD dwDisp;
    HKEY  hKey=NULL;
    BOOL  fReturn = TRUE;
    DWORD dwStdBias = 0;
    DWORD dwDltBias = 0;

    if (!IsValidTZISystemTime(&lpTzi->StandardDate) ||
        !IsValidTZISystemTime(&lpTzi->DaylightDate))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        fReturn = FALSE;
        goto Error;
    }

    if (IsTZISystemTimeSpecified(&lpTzi->StandardDate) && IsTZISystemTimeSpecified(&lpTzi->DaylightDate))
    {
        // StandardBias and DaylightBias will only be valid if we have DaylightDate and StandardDate 
        dwStdBias = lpTzi->StandardBias;
        dwDltBias = lpTzi->DaylightBias;
    }

    if (SetTimeZoneBias(
            lpTzi->Bias + dwStdBias,
            lpTzi->Bias + dwDltBias))
    {
        if (ERROR_SUCCESS == RegCreateKeyExW(
                                HKEY_LOCAL_MACHINE,
                                cszSystemTime,
                                0,
                                NULL,
                                REG_OPTION_NON_VOLATILE,
                                KEY_ALL_ACCESS,
                                NULL,
                                &hKey,
                                &dwDisp) &&
            hKey)
        {
            if (ERROR_SUCCESS == RegSetValueExW(
                                    hKey,
                                    cszSystemTimezone,
                                    0,
                                    REG_BINARY,
                                    (LPBYTE)lpTzi,
                                    sizeof(TIME_ZONE_INFORMATION)))
            {
                if (INVALID_TIMEZONE_ID != dwID)
                {
                    fReturn = ERROR_SUCCESS == RegSetValueExW(
                                            hKey,
                                            cszTZID,
                                            0,
                                            REG_DWORD,
                                            (LPBYTE)&dwID,
                                            sizeof(dwID));
                }
                else
                {
                    LONG  lReturn = RegDeleteValue(hKey, cszTZID);
                    fReturn = (ERROR_SUCCESS == lReturn) || (ERROR_FILE_NOT_FOUND == lReturn);
                }
                
                if (fReturn)
                {
                    if (g_hDSTService == INVALID_HANDLE_VALUE)
                    {
                        g_hDSTService = CreateFile(L"DST0:", GENERIC_READ|GENERIC_WRITE, 0, NULL, 0, 0, NULL);
                    }
                    if (g_hDSTService != INVALID_HANDLE_VALUE)
                    {
                        DeviceIoControl(
                            g_hDSTService,
                            IOCTL_DSTSVC_NOTIFY_TZ_CHANGE,
                            NULL,
                            0,
                            NULL,
                            0,
                            NULL,
                            NULL
                            );
                    }
                    CeEventHasOccurred(NOTIFICATION_EVENT_TZ_CHANGE, NULL);
                    RegCloseKey(hKey);
                    goto Error;
                }
            }
            RegCloseKey(hKey);
        }
        SetLastError(ERROR_OUTOFMEMORY);
    }
    else
    {
        // last error already set in kernel
    }

    fReturn = FALSE;
    
Error:
    return fReturn;
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | SetTimeZoneInformation | Sets the current time-zone parameters.
    These parameters control translations from Coordinated Universal Time (UTC) to local time.
    @parm CONST TIME_ZONE_INFORMATION * | lptzi | address of time-zone settings

    @comm Follows the Win32 reference description without restrictions or modifications.
*/
BOOL WINAPI SetTimeZoneInformation (const TIME_ZONE_INFORMATION *lpTzi)
{
    DWORD dwMatchedID = INVALID_TIMEZONE_ID; // Default to INVALID_TIMEZONE_ID.
    HKEY  hKeyTimeZones = NULL;
    HKEY  hKeyTimeZoneEntry = NULL;
    DWORD dwIndex;
    DWORD dwID;
    DWORD dwSize;
    DWORD dwType;
    TCHAR szTimeZoneEntryName[64];
    LONG  lReturn;
    BOOL  fReturn = TRUE;

    // Open HKLM\Time Zones Registry Key
    lReturn = OpenKeyHKLM(cszTimeZones, &hKeyTimeZones);
    if(lReturn != ERROR_SUCCESS)
    {
        SetLastError(ERROR_GEN_FAILURE);
        fReturn = FALSE;
        goto Error;
    }

    // Enumerate Timezone Entries under HKLM\Time Zones
    // Save the ID only when all the field values are the same as the input
    // Will not fail if the registry information is not complete.
    dwIndex = 0;
    dwSize = sizeof(szTimeZoneEntryName);
    while (ERROR_SUCCESS == RegEnumKeyEx(
                                hKeyTimeZones,
                                dwIndex++,
                                szTimeZoneEntryName,
                                &dwSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL))
    {
        // Open Timezone Entry's Key
        if (ERROR_SUCCESS == OpenKey(
                                hKeyTimeZones,
                                szTimeZoneEntryName,
                                &hKeyTimeZoneEntry))
        {
            TZREG TzRegValue;
            WCHAR szStandardName[ 32 ];
            WCHAR szDaylightName[ 32 ];
            
            memset (&TzRegValue, 0, sizeof(TzRegValue));
            
            // Generate Timezone information from registry
            dwSize=sizeof(TzRegValue);
            if (ERROR_SUCCESS != GetKey(hKeyTimeZoneEntry, cszTZI, &dwType, (LPBYTE)&TzRegValue, &dwSize))
            {
                RegCloseKey(hKeyTimeZoneEntry);
                continue;
            }
            
            // Compare the time zone information
            if (TzRegValue.Bias != lpTzi->Bias ||
                TzRegValue.DaylightBias != lpTzi->DaylightBias ||
                TzRegValue.StandardBias != lpTzi->StandardBias ||
                memcmp(&TzRegValue.DaylightDate, &lpTzi->DaylightDate, sizeof(SYSTEMTIME)) != 0 ||
                memcmp(&TzRegValue.StandardDate, &lpTzi->StandardDate, sizeof(SYSTEMTIME)) != 0 )
            {
                RegCloseKey(hKeyTimeZoneEntry);
                continue;
            }
            
            // Get "Std" from registry
            dwSize=sizeof(szStandardName);
            if (ERROR_SUCCESS != GetKey(hKeyTimeZoneEntry, cszStd, &dwType, (LPBYTE)&szStandardName, &dwSize))
            {
                RegCloseKey(hKeyTimeZoneEntry);
                continue;
            }
            
            // Compare the Standard Name
            if (wcsncmp(szStandardName, lpTzi->StandardName, sizeof(szStandardName)/sizeof(WCHAR)) != 0)
            {
                RegCloseKey(hKeyTimeZoneEntry);
                continue;
            }
            
            // Get "Dlt" from registry
            dwSize=sizeof(szStandardName);
            if (ERROR_SUCCESS != GetKey(hKeyTimeZoneEntry, cszDlt, &dwType, (LPBYTE)&szDaylightName, &dwSize))
            {
                RegCloseKey(hKeyTimeZoneEntry);
                continue;
            }
            
            // Compare the Daylight Name
            if (wcsncmp(szDaylightName, lpTzi->DaylightName, sizeof(szDaylightName)/sizeof(WCHAR)) != 0)
            {
                RegCloseKey(hKeyTimeZoneEntry);
                continue;
            }
            
            // Get ID value 
            dwSize = sizeof(DWORD);
            if (ERROR_SUCCESS == GetKey(
                        hKeyTimeZoneEntry,
                        cszID,
                        &dwType,
                        &dwID,
                        &dwSize))
            {
                dwMatchedID = dwID;
                RegCloseKey(hKeyTimeZoneEntry);
                break;
            }
            
            RegCloseKey(hKeyTimeZoneEntry);
        }
        
        dwSize = sizeof(szTimeZoneEntryName);
    }
    
    RegCloseKey(hKeyTimeZones);
    
    fReturn = Int_SetTimeZoneInformation(lpTzi, dwMatchedID);

Error:
    return fReturn;
}

static BOOL SetValidTimeZoneEntry(HKEY hKeyTimeZoneEntry)
{
    TIME_ZONE_INFORMATION Tzi;
    TZREG TzRegValue;
    DWORD dwID;
    DWORD dwType;
    DWORD dwSize;
    BOOL  fReturn = TRUE;

    memset(&Tzi, 0, sizeof(Tzi));
    memset (&TzRegValue, 0, sizeof(TzRegValue));

    // Generate Timezone information from registry
    dwSize=sizeof(TzRegValue);
    if (ERROR_SUCCESS != GetKey(hKeyTimeZoneEntry, cszTZI, &dwType, (LPBYTE)&TzRegValue, &dwSize))
    {
        fReturn = FALSE;
        goto Error;
    }

    Tzi.Bias = TzRegValue.Bias;


    if (IsTZISystemTimeSpecified(&TzRegValue.StandardDate) && 
        IsTZISystemTimeSpecified(&TzRegValue.DaylightDate))
    {
        Tzi.StandardBias = TzRegValue.StandardBias;
        Tzi.DaylightBias = TzRegValue.DaylightBias;
    }
    else
    {
        //The daylight bias and standard bias will be ignored if no standard date or daylight date specified
        Tzi.StandardBias = 0;
        Tzi.DaylightBias = 0;
    }

    Tzi.StandardDate = TzRegValue.StandardDate;
    Tzi.DaylightDate = TzRegValue.DaylightDate;

    dwSize=sizeof(Tzi.DaylightName);
    if (ERROR_SUCCESS != GetKey(hKeyTimeZoneEntry, cszDlt, &dwType, (LPBYTE)&Tzi.DaylightName, &dwSize))
    {
        fReturn = FALSE;
        goto Error;
    }

    dwSize=sizeof(Tzi.StandardName);
    if (ERROR_SUCCESS != GetKey(hKeyTimeZoneEntry, cszStd, &dwType, (LPBYTE)&Tzi.StandardName, &dwSize))
    {
        fReturn = FALSE;
        goto Error;
    }

    dwSize=sizeof(dwID);
    if (ERROR_SUCCESS != GetKey(hKeyTimeZoneEntry, cszID, &dwType, (LPBYTE)&dwID, &dwSize))
    {
        fReturn = FALSE;
        goto Error;
    }
 
    fReturn = Int_SetTimeZoneInformation(&Tzi, dwID);
    
Error:
    return fReturn;
}

/*
    @doc BOTH EXTERNAL

    @func BOOL | SetTimeZoneInformationByID | Sets the current time-zone parameters using timezone ID.
    The parameter should be one of valid timezone ID stored in registry.
    And the ID prepresents the translations from Coordinated Universal Time (UTC) to local time.
    @parm UINT nID

    @comm 
*/
BOOL WINAPI SetTimeZoneInformationByID (UINT nID)
{
    HKEY  hKeyTimeZones = NULL;
    HKEY  hKeyTimeZoneEntry = NULL;
    DWORD dwIndex;
    DWORD dwID;
    DWORD dwSize;
    DWORD dwType;
    TCHAR szTimeZoneEntryName[64];
    LONG  lReturn;
    BOOL  fReturn = TRUE;

    // Open HKLM\Time Zones Registry Key
    lReturn = OpenKeyHKLM(cszTimeZones, &hKeyTimeZones);
    if(lReturn != ERROR_SUCCESS)
    {
        SetLastError(ERROR_GEN_FAILURE);
        fReturn = FALSE;
        goto Error;
    }

    // Enumerate Timezone Entries under HKLM\Time Zones
    dwIndex = 0;
    dwSize = sizeof(szTimeZoneEntryName);
    while (ERROR_SUCCESS == RegEnumKeyEx(
                                hKeyTimeZones,
                                dwIndex++,
                                szTimeZoneEntryName,
                                &dwSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL))
    {

        // Open Timezone Entry's Key
        if (ERROR_SUCCESS == OpenKey(
                                hKeyTimeZones,
                                szTimeZoneEntryName,
                                &hKeyTimeZoneEntry))
        {
            // Get ID value 
            dwSize = sizeof(DWORD);
            if (ERROR_SUCCESS == GetKey(
                        hKeyTimeZoneEntry,
                        cszID,
                        &dwType,
                        &dwID,
                        &dwSize))
            {
                // Is it specified ID?
                if ((UINT)dwID == nID) {
                    if (SetValidTimeZoneEntry(hKeyTimeZoneEntry))
                    {
                        RegCloseKey(hKeyTimeZoneEntry);
                        RegCloseKey(hKeyTimeZones);
                        goto Error;
                    }
                }
            }
            RegCloseKey(hKeyTimeZoneEntry);
        }
        dwSize = sizeof(szTimeZoneEntryName);
    }
    // Cannot find specified ID
    RegCloseKey(hKeyTimeZones);
    SetLastError(ERROR_INVALID_PARAMETER);
    fReturn = FALSE;

Error:
    return fReturn;
}

static BOOL ValidateTZID(UINT nID)
{
    HKEY  hKeyTimeZones = NULL;
    DWORD dwIndex;
    DWORD dwID;
    DWORD dwType;
    DWORD dwSize;
    TCHAR szTimeZoneEntryName[64];
    BOOL  fReturn = FALSE;

    // Open HKLM\Time Zones Registry Key
    if (ERROR_SUCCESS != OpenKeyHKLM(cszTimeZones, &hKeyTimeZones))
    {
        goto Error;
    }

    // Enumerate Timezone Entries under HKLM\Time Zones
    dwIndex = 0;
    dwSize = sizeof(szTimeZoneEntryName);
    while (ERROR_SUCCESS == RegEnumKeyEx(
                hKeyTimeZones,
                dwIndex++,
                szTimeZoneEntryName,
                &dwSize,
                NULL, NULL, NULL, NULL)) {
        HKEY hKeyTimeZoneEntry;
        // Open Timezone Entry's Key
        if (ERROR_SUCCESS == OpenKey(
                                hKeyTimeZones,
                                szTimeZoneEntryName,
                                &hKeyTimeZoneEntry))
        {
            // Get ID value 
            dwSize = sizeof(DWORD);
            if (ERROR_SUCCESS == GetKey(
                        hKeyTimeZoneEntry,
                        cszID,
                        &dwType,
                        &dwID,
                        &dwSize)) {
                // Is it specified ID?
                if ((UINT)dwID == nID) {
                    RegCloseKey(hKeyTimeZoneEntry);
                    fReturn = TRUE;
                    break;
                }
            }
            RegCloseKey(hKeyTimeZoneEntry);
        }
        dwSize = sizeof(szTimeZoneEntryName);
    }
    RegCloseKey(hKeyTimeZones);

Error:
    return fReturn;
}

#define TZID_PACIFIC  400  // (UTC-08:00) Pacific Time (US & Canada)

static DWORD GetDefaultTimeZoneInformationID(void)
{
    DWORD dwID;
    DWORD dwType;
    DWORD dwSize;
    HKEY hKey = 0;

    dwSize = sizeof(dwID);

    // see if we have a value "DefaultTZID" under HKLM\Time Zones
    if ((ERROR_SUCCESS != OpenKeyHKLM(cszTimeZones, &hKey)) ||
        (ERROR_SUCCESS != GetKey(hKey, szDefaultTZID, &dwType, &dwID, &dwSize)))
    {
        // If there is no DefaultTZID, let us set the return value of dwID as  
        // the 'ID' corresponding to the TimezoneName registry entry
        HKEY hKey1=0, hKey2=0;
        WCHAR TimezoneName[32];

        dwSize = sizeof(TimezoneName);

        if( !((ERROR_SUCCESS == OpenKeyHKLM(cszTimeZones, &hKey1)) &&
             (ERROR_SUCCESS == GetKey(hKey1, L"Default", &dwType, TimezoneName, &dwSize)) &&
             (ERROR_SUCCESS == OpenKey(hKey1, TimezoneName, &hKey2)) &&
             (ERROR_SUCCESS == GetKey(hKey2,cszID,&dwType,&dwID,&dwSize))))
        {
            // Default to Pacific time for now
            dwID = TZID_PACIFIC;
        }

        if(hKey1)
        {
            RegCloseKey(hKey1);
        } 

        if(hKey2)
        {
            RegCloseKey(hKey2);
        }
    }

    if(hKey)
    {
        RegCloseKey(hKey);
    }
    return dwID;
}


/*
    @doc BOTH EXTERNAL

    @func UINT | GetTimeZoneInformationID | Get current timezoen ID
    @parm void
    @comm 
*/
UINT WINAPI GetTimeZoneInformationID (void)
{
    HKEY hKeyTime = NULL;
    DWORD dwID;
    DWORD dwSize;
    DWORD dwType;

    dwSize = sizeof(dwID);

    // Check for existing Time key
    if(ERROR_SUCCESS == OpenKeyHKLM(cszSystemTime, &hKeyTime))
    {
        if (ERROR_SUCCESS == GetKey(
                                    hKeyTime,
                                    cszTZID,
                                    &dwType,
                                    &dwID,
                                    &dwSize))
        {
            // We have a value "TZID" under HKLM\Time
            // Validate the ID
            if(!ValidateTZID(dwID))
            {
                // Error if it's invalid
                dwID = INVALID_TIMEZONE_ID;
            }
        }
        else
        {
            // Error in reading TZID
            dwID = INVALID_TIMEZONE_ID;
        }
        RegCloseKey(hKeyTime);
    }
    else
    {
        // Time zone has not been set in any manner
        dwID = GetDefaultTimeZoneInformationID();
    }

    return dwID;
}


static BOOL CopyTimeZoneInformation(TIME_ZONE_INFORMATION *lpTZI, HKEY hKeyTimeZoneEntry)
{
    TZREG TzRegValue;
    DWORD dwSize;
    DWORD dwType;
    BOOL  fReturn = TRUE;

    // Generate Timezone information from registry
    dwSize = sizeof(TzRegValue);
    if (ERROR_SUCCESS != GetKey(
                            hKeyTimeZoneEntry,
                            cszTZI,
                            &dwType,
                            (LPBYTE)&TzRegValue, &dwSize))
    {
        fReturn = FALSE;
        goto Error;
    }

    lpTZI->Bias = TzRegValue.Bias;
    
    if (IsTZISystemTimeSpecified(&TzRegValue.StandardDate) && 
        IsTZISystemTimeSpecified(&TzRegValue.DaylightDate))
    {
        lpTZI->StandardBias = TzRegValue.StandardBias;
        lpTZI->DaylightBias = TzRegValue.DaylightBias;
    }
    else
    {
        //The daylight bias and standard bias will be ignored if no standard date or daylight date specified
        lpTZI->StandardBias = 0;
        lpTZI->DaylightBias = 0;
    }
    
    lpTZI->StandardDate = TzRegValue.StandardDate;
    lpTZI->DaylightDate = TzRegValue.DaylightDate;
    
    dwSize = sizeof(lpTZI->DaylightName);
    if (ERROR_SUCCESS != GetKey(
                            hKeyTimeZoneEntry,
                            cszDlt,
                            &dwType,
                            (LPBYTE)&lpTZI->DaylightName,
                            &dwSize))
    {
        fReturn = FALSE;
        goto Error;
    }

    dwSize = sizeof(lpTZI->StandardName);
    if (ERROR_SUCCESS != GetKey(
                            hKeyTimeZoneEntry,
                            cszStd,
                            &dwType,
                            (LPBYTE)&lpTZI->StandardName,
                            &dwSize))
    {
        fReturn = FALSE;
        goto Error;
    }

Error:
    return fReturn;
}

CRITICAL_SECTION g_csTZList;
DWORD g_dwNumTimeZoneEntries = 0;
TIME_ZONE_INFORMATION_WITH_ID *g_rgTimeZoneList = NULL;
HANDLE g_hTzRegChangeEvent = INVALID_HANDLE_VALUE;

void InvalidateCachedTimeZoneList(void)
{
    if (g_hTzRegChangeEvent != INVALID_HANDLE_VALUE)
    {
        CeFindCloseRegChange(g_hTzRegChangeEvent);
        g_hTzRegChangeEvent = INVALID_HANDLE_VALUE;
    }
    if (g_rgTimeZoneList)
    {
        LocalFree(g_rgTimeZoneList);
        g_rgTimeZoneList = NULL;
    }
    g_dwNumTimeZoneEntries = 0;
}

void InitializeTimeZoneList(void)
{
    InitializeCriticalSection(&g_csTZList);
}

void DeinitializeTimeZoneList(void)
{
    InvalidateCachedTimeZoneList();
    DeleteCriticalSection(&g_csTZList);
    if (g_hDSTService != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hDSTService);
        g_hDSTService = INVALID_HANDLE_VALUE;
    }
}


/*
    @doc BOTH EXTERNAL

    @func UINT | GetTimeZoneList | Get valid timezoen entry list
    @parm CONST TIME_ZONE_INFORMATION_WITH_ID * | rgTimeZoneList | address of time zone list (OUT)
    @parm CONST UINT | pTimeZoneList | number of entries in the list (IN)
    @comm 
*/
UINT GetTimeZoneList(TIME_ZONE_INFORMATION_WITH_ID *rgTimeZoneList, UINT cTimeZoneList)
{
    DWORD dwID;
    DWORD dwSize;
    DWORD dwType;
    DWORD dwNumEntries = 0, dwIndex;
    TIME_ZONE_INFORMATION_WITH_ID *rgLocalList = NULL;
    HKEY hKeyTimeZones = NULL;
    HKEY hKeyTimeZoneEntry = NULL;
    LONG lLastError = ERROR_SUCCESS;

    EnterCriticalSection(&g_csTZList);

    if ((g_hTzRegChangeEvent == INVALID_HANDLE_VALUE) ||
        (WAIT_OBJECT_0 == WaitForSingleObject(g_hTzRegChangeEvent, 0)))
    {
        // If notification has not been setup or registry has been updated, 
        InvalidateCachedTimeZoneList();
    }

    if (!g_dwNumTimeZoneEntries)
    {
        // Open HKLM\Time Zones Registry Key
        lLastError = OpenKeyHKLM(cszTimeZones, &hKeyTimeZones);
        if (lLastError != ERROR_SUCCESS)
        {
            lLastError = ERROR_GEN_FAILURE;
            goto Error;
        }

        // Setup notification event
        if (g_hTzRegChangeEvent == INVALID_HANDLE_VALUE)
        {
            g_hTzRegChangeEvent = CeFindFirstRegChange(hKeyTimeZones, TRUE, REG_NOTIFY_CHANGE_LAST_SET);
        }

        // Determine the number of subkeys under HKLM\Time Zones
        dwNumEntries = 0;
        lLastError = RegQueryInfoKey(
                    hKeyTimeZones,
                    NULL,
                    NULL,
                    NULL,
                    &dwNumEntries,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL);
        if ((lLastError != ERROR_SUCCESS) ||
            (dwNumEntries > UINT_MAX / sizeof(TIME_ZONE_INFORMATION_WITH_ID)))
        {
            lLastError = ERROR_GEN_FAILURE;
            goto Error;
        }

        // Allocate buffer to store ID list
        if (dwNumEntries)
        {
            rgLocalList = (TIME_ZONE_INFORMATION_WITH_ID *)LocalAlloc(LPTR, sizeof(TIME_ZONE_INFORMATION_WITH_ID)*dwNumEntries);
            if (rgLocalList == NULL)
            {
                lLastError = ERROR_NOT_ENOUGH_MEMORY;
                goto Error;
            }
        }
        else
        {
            lLastError = ERROR_NOT_FOUND;
        }

        // Copy Timezone information to the buffer
        for (dwIndex = 0; dwIndex < dwNumEntries; dwIndex++)
        {
            // Get subkey
            dwSize = sizeof(rgLocalList[dwIndex].ReferenceName);
            if (ERROR_SUCCESS != RegEnumKeyEx(
                                    hKeyTimeZones,
                                    dwIndex,
                                    rgLocalList[dwIndex].ReferenceName,
                                    &dwSize,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL))
            {
                lLastError = ERROR_GEN_FAILURE;
                goto Error;
            }

            // Open Timezone Entry's Key
            if (ERROR_SUCCESS != OpenKey(
                                    hKeyTimeZones,
                                    rgLocalList[dwIndex].ReferenceName,
                                    &hKeyTimeZoneEntry))
            {
                lLastError = ERROR_GEN_FAILURE;
                goto Error;
            }

            // Get ID value 
            dwSize = sizeof(DWORD);
            if (ERROR_SUCCESS != GetKey(
                                    hKeyTimeZoneEntry,
                                    cszID,
                                    &dwType,
                                    &dwID,
                                    &dwSize))
            {
                lLastError = ERROR_GEN_FAILURE;
                goto Error;
            }

            // Copy Time Zone Entry information
            rgLocalList[dwIndex].uId = dwID;
            if(!CopyTimeZoneInformation(&rgLocalList[dwIndex].tzi, hKeyTimeZoneEntry))
            {
                lLastError = ERROR_GEN_FAILURE;
                goto Error;
            }
            dwSize = sizeof(rgLocalList[dwIndex].DisplayName);
            if (ERROR_SUCCESS != GetKey(
                                    hKeyTimeZoneEntry,
                                    cszDisplay,
                                    &dwType,
                                    &rgLocalList[dwIndex].DisplayName,
                                    &dwSize))
            {
                lLastError = ERROR_GEN_FAILURE;
                goto Error;
            }

            RegCloseKey(hKeyTimeZoneEntry);
            hKeyTimeZoneEntry = NULL;
        }

        // Cache the time zone list
        g_dwNumTimeZoneEntries = dwNumEntries;
        g_rgTimeZoneList = rgLocalList;

        RegCloseKey(hKeyTimeZones);
        hKeyTimeZones = NULL;
    }
    else
    {
        // Use the cached time zone list
        dwNumEntries = g_dwNumTimeZoneEntries;
        rgLocalList = g_rgTimeZoneList;
    }

    // Copy output values
    __try
    {
        if (rgTimeZoneList)
        {
            if (cTimeZoneList >= dwNumEntries)
            {
                memcpy(
                    rgTimeZoneList,
                    rgLocalList,
                    dwNumEntries * sizeof(TIME_ZONE_INFORMATION_WITH_ID));
            }
            else
            {
                lLastError = ERROR_INSUFFICIENT_BUFFER;
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        lLastError = ERROR_INVALID_PARAMETER;
    }

Error:
    if (hKeyTimeZoneEntry)
    {
        RegCloseKey(hKeyTimeZoneEntry);
    }
    if (hKeyTimeZones)
    {
        RegCloseKey(hKeyTimeZones);
    }
    if ((lLastError != ERROR_SUCCESS) && (lLastError != ERROR_INSUFFICIENT_BUFFER))
    {
        dwNumEntries = 0;
    }
    SetLastError(lLastError);
    LeaveCriticalSection(&g_csTZList);
    return dwNumEntries;
}
