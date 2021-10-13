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

///------------------------------------------------------------------------
/// module: DynamicDST.cpp
/// purpose: DynamicDST support. Need to handle the following 3 scenarios
///         1)  Update TZI information when system start up
///         2)  Update TZI information when local time across year boundary
///         3)  Update TZI information when user changes the content under HKLM\Time zones
/// 
///------------------------------------------------------------------------

#include "dst.h"

const TCHAR c_strLastUpdateYear[] = _T("LastUpdateYear");

//This function comes private\winceos\coreos\core\dll\time.c. Some additional verifications are added.
static BOOL IsValidTZISystemTime (const SYSTEMTIME *lpst)
{
    FILETIME ft;
    return !lpst->wMonth
        || (!lpst->wYear && 
            (lpst->wDayOfWeek <= 6) && 
            (lpst->wDay >= 1) && (lpst->wDay <= 5) && 
            (lpst->wHour < 24) &&
            (lpst->wMinute < 60) &&
            (lpst->wSecond < 60) && 
            (lpst->wMilliseconds < 1000))
        || SystemTimeToFileTime (lpst, &ft);
}

/// <summary>
///    Verify and return the TZI information for the specified year under specified
///    registry key
/// </summary>   
/// <param name="hDST">hKey for the dynamic DST registry</param>
/// <param name="dwYear">The year of TZI information</param>
/// <param name="pTZI">Buffer to return TZI information</param>
/// <returns>
///    If there is a valid TZI information for the specified year, it will return TRUE
///    otherwise FALSE.
///    
///    The TZI information is valid only when it meets all the following conditions:
///        1. The registry read returns ERROR_SUCCESS
///        2. The data format is REG_BINARY
///        3. The data size is the sizeof TZREG
/// </returns>
static BOOL GetTZIInfo(
    HKEY hDST, 
    DWORD dwYear, 
    TZREG *pTZI
)
{
    BOOL bRet = FALSE;
    
    DWORD   cbData = sizeof(TZREG);
    DWORD   dwType = 0;
    
#define MAX_YEAR_STR_LEN    32  //32 characters are OK for a dword year
    TCHAR   szYear[MAX_YEAR_STR_LEN];
    _itot_s(dwYear, szYear, 10);
    
    if (ERROR_SUCCESS != RegQueryValueEx(hDST, szYear, NULL, &dwType, 
            (LPBYTE)(pTZI), &cbData))
        goto Error;
        
    if (dwType != REG_BINARY || cbData != sizeof(TZREG))
        goto Error;

    if (!IsValidTZISystemTime(&pTZI->StandardDate))
        goto Error;
    
    if (!IsValidTZISystemTime(&pTZI->DaylightDate))
        goto Error;
        
    //If the TZI supports DST, the month value of both the DaylightDate and 
    //StandardDate must be NonZero
    if (pTZI->DaylightDate.wMonth == 0 && pTZI->StandardDate.wMonth != 0)
        goto Error;
    
    //If the TZI doesn't support DST, the month value of both the DaylightDate 
    //and StandardDate must be zero
    if (pTZI->DaylightDate.wMonth != 0 && pTZI->StandardDate.wMonth == 0)
        goto Error;

    bRet = TRUE;
        
Error:
    return bRet;
}

/// <summary>
///   Verify if the specified registry key contains valid dynamic DST information
///   
/// </summary>
/// <param name="hDST">The DST key need to be verified</param>
/// <param name="pdwFirstEntry">Buffer to return the value of FirstEntry</param>
/// <param name="pdwLastEntry">Buffer to return the value of LastEntry</param>
///   
/// <returns>
///   If the content under key hDST contains valid Dynamic DST information, it will 
///   return TRUE, the FirstEntry and LastEntry will also be put in pdwFirstEntry 
///   and pdwLastEntry
///   Otherwise it will return FALSE and content of pdwFirstEntry and pdwLastEntry
///   is undefined
///
///   The registry contains a valid Dynamic DST information only when it meets all 
///   the following requirements:
///       1. Registry read returns ERROR_SUCCESS when querying both FirstEntry 
///          and LastEntry
///       2. The type of FirstEntry and LastEntry is REG_DWORD
///       3. LastEntry >= FirstEntry
///       4. There are valid TZI information for both FirstEntry and LastEntry
/// </returns>
static BOOL VerifyDynamicDST(
    HKEY hDST, 
    LPDWORD pdwFirstEntry, 
    LPDWORD pdwLastEntry
)
{
    BOOL bRet = FALSE;

    DWORD cbData = sizeof(DWORD);
    DWORD dwType = 0;
    if (ERROR_SUCCESS != RegQueryValueEx(
                            hDST, 
                            cszFirstEntry, 
                            NULL, 
                            &dwType, 
                            (LPBYTE)(pdwFirstEntry), 
                            &cbData))
        goto Error;
        
    if (dwType != REG_DWORD)
        goto Error;

    cbData = sizeof(DWORD);
    if (ERROR_SUCCESS != RegQueryValueEx(
                            hDST, 
                            cszLastEntry, 
                            NULL, 
                            &dwType, 
                            (LPBYTE)(pdwLastEntry), 
                            &cbData))
        goto Error;

    if (dwType != REG_DWORD)
        goto Error;

    if (*pdwLastEntry <= *pdwFirstEntry)
        goto Error;

    TZREG tzi;
    if (!GetTZIInfo(hDST, *pdwFirstEntry, &tzi))
        goto Error;
        
    if (!GetTZIInfo(hDST, *pdwLastEntry, &tzi))
        goto Error;

    bRet = TRUE;

Error:
    return bRet;
}
/// <summary>
///   Update the TZI information for the specified timezone according to it's 
///   dynamic DST settings for current year.
///   
/// </summary>
///   <param name=" hTimezone:     The registry key of the timezone for which we need to 
///                       update
///   <param name=" dwCurrentYear: The year we want to update
///   
/// <returns>
///   none
/// </returns>
static void UpdateTZI(
    HKEY hTimezone, 
    DWORD dwCurrentYear
)
{
    HKEY hDST = NULL;
    if (ERROR_SUCCESS != RegOpenKeyEx(hTimezone, cszDynDST, 0, 0, &hDST))
        goto Error;
        
    DWORD dwFirstEntry = 0;
    DWORD dwLastEntry = 0;    
    if (!VerifyDynamicDST(hDST, &dwFirstEntry, &dwLastEntry))
        goto Error;

    TZREG tzi;
    
    DWORD dwDSTYear = dwFirstEntry;
    DWORD dwPrevDSTYear = dwDSTYear;

    while ((dwDSTYear <= dwCurrentYear) && (dwDSTYear <= dwLastEntry))
    {
        if (GetTZIInfo(hDST, dwDSTYear, &tzi))
        {
            dwPrevDSTYear = dwDSTYear;
        }
        dwDSTYear ++;
    }
    
    if (!GetTZIInfo(hDST, dwPrevDSTYear, &tzi))
    {
        RETAILMSG(1, (_T("[TIMESVC DST]Failed to get TZI information for year %d\n"), dwPrevDSTYear));
        goto Error;
    }

    if (ERROR_SUCCESS != RegSetValueEx(
                            hTimezone, 
                            cszTZI, 0, 
                            REG_BINARY, 
                            (LPBYTE)(&tzi), 
                            sizeof(TZREG)))
    {
        RETAILMSG(1, (_T("[TIMESVC DST]Failed to update TZI information\n")));
        goto Error;
    }
   
Error:
    if (hDST)
    {
        RegCloseKey(hDST);
    }
}


/// <summary>
///     Update the system timezone information if corresponded timezone is updated
///     
/// </summary>
///     <param name=" hTzRoot:     The registry key of HKLM\Time zones
///     
/// <returns>
///     none
///     
/// Comments:
///     If we update the system timezone information, it will signal the TZ_CHANGE 
///     event, which will finally get the original DST code to update the daylight 
///     saving time status
/// </returns>
static void UpdateCurrentTimezone(
    HKEY hTzRoot
)
{
    BOOL bRet;
    bRet = SetTimeZoneInformationByID(GetTimeZoneInformationID());
    ASSERT(bRet);
}



/// <summary>
///     Update the TZI information for all time zones according to the dynamic DST 
///     settings for current year.
///     
/// </summary>
/// <param name="dwYear">For which year we are udpating the TZI information</param>
/// <param name="bForceUpdate"> 
///     If this flag is FALSE, we will check the registry value "LastUpdateYear"
///     under HKLM\Time zones to see if we have updated the information for this
///     year. If yes, we will not update the TZI information and return ERROR_ALREADY_EXISTS.
///     Otherwise, we will do update
///         
///     If this flag is set to TRUE, we will force an update and ignore the value 
///     of "LastUpdateYear"
///         
///     No matter TRUE or FALSE of this flag, we will update the value of 
///     "LastUpdateYear" after we finish the real update successfully
/// </param>
/// <returns>
///     If the Dynamic DST update is taken place, returns ERROR_SUCCESS.
///     If the TZI information is alreay up to date, returns ERROR_ALREADY_EXISTS.
///     If any error occurs, returns ERROR_GEN_FAILURE.
/// </returns>
int DynamicDSTUpdate(
    HKEY hTzRoot,
    DWORD dwYear,
    BOOL bForceUpdate
)
{
    DEBUGMSG(1, (_T("[TIMESVC DST]Updating TZI information for year %d\n"), dwYear));

    int iErr = ERROR_GEN_FAILURE;
    
    if (!bForceUpdate)
    {
        DWORD dwLastUpdateYear = 0;
        DWORD cbData = sizeof(DWORD);
        if ((ERROR_SUCCESS == RegQueryValueEx(
                                hTzRoot, 
                                c_strLastUpdateYear, 
                                NULL, 
                                NULL, 
                                (LPBYTE)(&dwLastUpdateYear), 
                                &cbData)) && 
            (dwYear == dwLastUpdateYear))
        {
            DEBUGMSG(1, (_T("[TIMESVC DST]Timezone update skipped for year %d\n"), dwYear));
            iErr = ERROR_ALREADY_EXISTS;
            goto Error;
        }
    }
    
    DWORD cchSubKeyLen = 0;
    if (ERROR_SUCCESS != RegQueryInfoKey(
                            hTzRoot, 
                            NULL, 
                            NULL, 
                            NULL, 
                            NULL, 
                            &cchSubKeyLen, 
                            NULL, 
                            NULL, 
                            NULL, 
                            NULL, 
                            NULL, 
                            NULL))
    {
        RETAILMSG(1, (_T("[TIMESVC DST]Failed to get registry information for HKLM\\Time zones")));
        goto Error;
    }
        
    //Add one more space for the terminating null character
    if (cchSubKeyLen < (INT_MAX - 1) / sizeof(TCHAR))
    {
        cchSubKeyLen ++;
    }
    else
    {
        goto Error;
    }
    
    TCHAR* pSubKey = new TCHAR[cchSubKeyLen];
    if (pSubKey != NULL)
    {
        DWORD dwIndex = 0;
        while (TRUE)
        {
            DWORD cchName = cchSubKeyLen;
            LONG lRet = RegEnumKeyEx(
                            hTzRoot, 
                            dwIndex, 
                            pSubKey, 
                            &cchName, 
                            NULL, 
                            NULL, 
                            NULL, 
                            NULL);
            if (lRet == ERROR_NO_MORE_ITEMS)
                break;

            dwIndex ++;
                
            //Since we are using the buffer with the maximum length of subkey 
            //names, we shouldn't get this Error
            ASSERT(lRet != ERROR_MORE_DATA);
            if (lRet != ERROR_SUCCESS)
                continue;
                
            HKEY hTimezone = NULL;
            if (ERROR_SUCCESS == RegOpenKeyEx(hTzRoot, pSubKey, 0, 0, &hTimezone))
            {
                UpdateTZI(hTimezone, dwYear);
                RegCloseKey(hTimezone);
            }
        }
        
        delete[] pSubKey;
    }

    //Once we upate tzi information for all timezones, the current system time 
    //zone may also be updated, we need let system know if there are changes 
    //to current system timezone
    UpdateCurrentTimezone(hTzRoot);

    //After we update the tzi information, set the update flag to this year to 
    //avoid unnecessary update in future
    if (ERROR_SUCCESS != RegSetValueEx(
                            hTzRoot, 
                            c_strLastUpdateYear, 
                            NULL, 
                            REG_DWORD, 
                            (LPBYTE)(&dwYear), 
                            sizeof(DWORD)))
    {
        RETAILMSG(1, (_T("[TIMESVC DST]Failed to update the value of LastUpdateYear to %d"), dwYear));
        goto Error;
    }
    
    iErr = ERROR_SUCCESS;
    
Error:
    return iErr;
}
