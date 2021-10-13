//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include "celog.h"
#include "ceperf.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Convert a LARGE_INTEGER value from performance counter ticks to microseconds
__inline static VOID MDPGPerformanceCounterToMicroseconds(
    const LARGE_INTEGER* pliPerfCount,  // Performance counter ticks
    ULARGE_INTEGER*       pulMSCount     // Receives value in microseconds
    )
{
    __int64 iVal;
    static LARGE_INTEGER s_liPerfFreq = {0};

    if ((s_liPerfFreq.LowPart == 0) && (s_liPerfFreq.HighPart == 0)) 
    {
        if (!QueryPerformanceFrequency(&s_liPerfFreq)) 
        {
            pulMSCount->QuadPart = 0;
            return;
        }
    }

    iVal = (__int64)pliPerfCount->QuadPart;
    iVal *= 1000000;
    iVal /= s_liPerfFreq.QuadPart;

    pulMSCount->QuadPart = iVal;
}


__inline void MDPGLogTimeEx(LPCWSTR  KeyName, LPCWSTR pszEvent)
{
    LARGE_INTEGER liTime;
    DWORD dwMilliSec;
    HKEY hKey;
    LONG lResult;
    ULARGE_INTEGER uliTime = {0};
    DWORD  dwPerfOn;
    static DWORD s_dwRegPerfMask=0;
    static BOOL  s_fQueryPerfMask=FALSE;


    if (IsCeLogStatus(CELOGSTATUS_ENABLED_ANY)) 
    {
        CeLogData(TRUE, CELID_RAW_WCHAR, (PVOID) pszEvent, (WORD)(wcslen(pszEvent) + 1) *sizeof (WCHAR), 0, CELZONE_BOOT_TIME, 0, FALSE);
    }


    if( !s_fQueryPerfMask )
    {
       HKEY hKey=NULL;
       if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"init",0,KEY_READ,&hKey) == ERROR_SUCCESS )
       {
          DWORD dwType, cbData;
          cbData = sizeof(DWORD);
          RegQueryValueEx(hKey, L"WritePerfMark", NULL, &dwType, (LPBYTE)&s_dwRegPerfMask, &cbData);
          RegCloseKey(hKey);
       }
       s_fQueryPerfMask=TRUE; // This will prevent Query again and again 
    }

    dwPerfOn = s_dwRegPerfMask;
    
    
    // In registry marker "PerfMark " is 0 then don't Write , Just  return
    // This  helps only when we want to analyze  Task anatomy during boot. 
    if( !dwPerfOn )
    {
       return;
    }
    
    QueryPerformanceCounter(&liTime);
    MDPGPerformanceCounterToMicroseconds(&liTime, &uliTime);
    dwMilliSec = (DWORD)((__int64)uliTime.QuadPart/1000);


    lResult = RegCreateKeyEx(HKEY_CURRENT_USER,
                 KeyName,
                 0, NULL, REG_OPTION_VOLATILE, 
                 0, NULL, &hKey, NULL);

    if (lResult == ERROR_SUCCESS)
    {
       ASSERT(hKey != NULL);
       if(ERROR_SUCCESS != RegQueryValueEx(hKey, pszEvent, NULL, NULL, NULL, NULL))
       {
          RegSetValueEx(hKey, pszEvent, 0, REG_DWORD,
              (LPBYTE)&dwMilliSec, sizeof(DWORD));
       }
       RegCloseKey(hKey);
    }
    
}

__inline void CELOG_OemDependencyOnBoot(__format_string const WCHAR* szFormat, ...)
{
    if (IsCeLogZoneEnabled(CELZONE_BOOT_TIME)) 
    {
        va_list arglist;
        WCHAR   szTemp[MAX_PATH];
        size_t  cchLen;
        HRESULT hr;

        va_start(arglist, szFormat);
        hr = StringCchVPrintfW(szTemp, MAX_PATH, szFormat, arglist);
        if (SUCCEEDED(hr)) 
        {
            hr = StringCchLengthW(szTemp, MAX_PATH, &cchLen);
            if (SUCCEEDED(hr)) 
            {
                CeLogData(TRUE, CELID_RAW_WCHAR, szTemp, (WORD)((cchLen + 1) * sizeof(WCHAR)),
                          0, CELZONE_BOOT_TIME, 0, FALSE);
            }
        }   
    }
}
