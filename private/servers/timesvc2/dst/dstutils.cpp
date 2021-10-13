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
#include "dst.h"

// Determine first day of week [Sun-Sat] of the first day of the month.
static WORD GetFirstWeekDayOfMonth(WORD wYear, WORD wMonth) 
{
    SYSTEMTIME st;
    FILETIME   ft;

    ZeroMemory(&st, sizeof(SYSTEMTIME));
    st.wYear = wYear;
    st.wMonth =wMonth;
    st.wDay = 1;
    
    // Convert to FILETIME and then back to SYSTEMTIME to get this information.
    SystemTimeToFileTime(&st,&ft);
    FileTimeToSystemTime(&ft,&st);
    ASSERT((st.wDayOfWeek >= 0) && (st.wDayOfWeek <= 6));
    return st.wDayOfWeek;
}

// Convert a SYSTEMITIME in the day-in-month format (ultimatly from TIME_ZONE_INFORMATION) 
// into a real SYSTEMTIME for a specified year so that the system can process.
static BOOL DetermineDSTChangeDate(const SYSTEMTIME* pstTziDate, SYSTEMTIME* pstAbsoluteDate, WORD wYear)
{
    FILETIME ftTemp;

    memcpy(pstAbsoluteDate, pstTziDate, sizeof(SYSTEMTIME));
    if (pstTziDate->wYear == 0)
    {
        ASSERT((pstTziDate->wDay >= 1) && (pstTziDate->wDay <= 5));
        pstAbsoluteDate->wYear = wYear;

        // Day of week [Sun-Sat] the first day of week falls on
        WORD wFirstDayInMonth = GetFirstWeekDayOfMonth(wYear, pstTziDate->wMonth);
        WORD wFirstDayTzChange = 1 + pstTziDate->wDayOfWeek - wFirstDayInMonth;
        
        if (wFirstDayInMonth > pstTziDate->wDayOfWeek)
            wFirstDayTzChange += 7;
        
        pstAbsoluteDate->wDay = wFirstDayTzChange;
        
        for (int i = 1; i < pstTziDate->wDay; i ++)
        {
            // Increment the current week by one, and see if it's a legit date.
            // SystemTimeToFileTime has all the logic to determine if the SystemTime
            // is correct - if it's not in means we've "overshot" the intended date
            // and need to pull back on.  Consider for instance if the TZ Cutover
            // was set to be 5th Thursday in July 2006.  Eventually in this loop
            // we would get to July 34th, which is bogus, so we'd need to pull
            // back to July 27th.
            pstAbsoluteDate->wDay += 7;
            
            if (!SystemTimeToFileTime(pstAbsoluteDate, &ftTemp))
            {
                // We overshot, pull back a week
                pstAbsoluteDate->wDay -= 7;
                break;
            }
        }
    }
    
    return SystemTimeToFileTime(pstAbsoluteDate, &ftTemp);
}

/*++
Routine Description:
    determines if the current locale supports DST based on
    information in a TIME_ZONE_INFORMATION struct
    
Arguments:
    pointer to a TIME_ZONE_INFORMATION  struct
    
Return Value:
    TRUE if the locale supports DST, FALSE otherwise
    
--*/
BOOL DoesTzSupportDST(const TIME_ZONE_INFORMATION *ptzi)
{
    ASSERT(ptzi);

    // If the month value is zero then this locale does not change for DST
    // it should never be the case that we have a DST date but not a SDT date
    ASSERT(((0 != ptzi->StandardDate.wMonth) && (0 != ptzi->DaylightDate.wMonth)) ||
           ((0 == ptzi->StandardDate.wMonth) && (0 == ptzi->DaylightDate.wMonth)));

    if ((0 != ptzi->StandardDate.wMonth) && (0 != ptzi->DaylightDate.wMonth))
        return TRUE;
    else
        return FALSE;
}

BOOL GetStandardDate(WORD wYear, const TIME_ZONE_INFORMATION* pTZI, SYSTEMTIME* pstStandardDate, FILETIME* pftStandardDate)
{
    if (!DetermineDSTChangeDate(&pTZI->StandardDate, pstStandardDate, wYear))
        return FALSE;
    
    if (!SystemTimeToFileTime(pstStandardDate, pftStandardDate))
        return FALSE;
    
    //Normalize the pstStandarDate by FileTimeToSystemTime
    FileTimeToSystemTime(pftStandardDate, pstStandardDate);
    
    LONGLONG* pFileTime = (LONGLONG*)pftStandardDate;
    *pFileTime += MINUTES_TO_FILETIME(pTZI->Bias + pTZI->DaylightBias);
    
    //Make sure the calcuated file time is a valid file time.
    SYSTEMTIME stTemp;
    return FileTimeToSystemTime(pftStandardDate, &stTemp);
}

BOOL GetDaylightDate(WORD wYear, const TIME_ZONE_INFORMATION* pTZI, SYSTEMTIME* pstDaylightDate, FILETIME* pftDaylightDate)
{
    if (!DetermineDSTChangeDate(&pTZI->DaylightDate, pstDaylightDate, wYear))
        return FALSE;
    
    if (!SystemTimeToFileTime(pstDaylightDate, pftDaylightDate))
        return FALSE;
        
    //Normalize the pstDaylightDate by FileTimeToSystemTime
    FileTimeToSystemTime(pftDaylightDate, pstDaylightDate);

    LONGLONG* pFileTime = (LONGLONG*)pftDaylightDate;
    *pFileTime += MINUTES_TO_FILETIME(pTZI->Bias + pTZI->StandardBias);
    
    //Make sure the calcuated file time is a valid file time.
    SYSTEMTIME stTemp;
    return FileTimeToSystemTime(pftDaylightDate, &stTemp);
}



