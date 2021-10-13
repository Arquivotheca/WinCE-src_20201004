//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
// 
// File:    typemapr.cpp
// 
// Contents:
//
//  implementation file 
//
//		ITypeMapper classes  implemenation
//	
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include <math.h>
#include "utctime.h"

const   long c_secondsInDay = 24 * 60 * 60 ;

static HRESULT DateToFixedDate(WORD wYear, const SYSTEMTIME * pVariableDate, SYSTEMTIME *pResultDate);
static BOOL IsBetweenDates(const SYSTEMTIME *pDate, const SYSTEMTIME *pStartDate, const SYSTEMTIME * pEndDate);
static BOOL isBefore(const SYSTEMTIME *pDate, const SYSTEMTIME * pmark);


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  HRESULT adjustTime(DOUBLE *pdblDate, long lSecondsToAdjust)
//
//  parameters:
//
//  description:
//      takes date and adds the +/- values to it
//          
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT adjustTime(DOUBLE *pdblDate, long lSecondsToAdjust)
{
    HRESULT hr=S_OK;
   	double      dblValue;

    double  dSeconds;
    long    lSeconds;
    double  dateToAdd = 1; 
    bool    fAvoidVBZero = false; 
    bool    fAvoidVBHistory = false; 
        
    dblValue = *pdblDate;
    
    // the timezone holds the diff to UTC in minutes
    if (dblValue < 0 && dblValue > -1) 
    {
        dblValue *= -1; 
    }
    if (dblValue < 0)
    {
        // addjust the bias to compensate for negative representations of dates/times before 1899
        fAvoidVBHistory = true;
        dblValue *= -1; 
        dateToAdd = -1;
    }

    
    dSeconds = (dblValue - floor(dblValue)) * c_secondsInDay; 
    lSeconds = (long) (floor(dSeconds + 0.5)); 

    lSeconds += (lSecondsToAdjust); 
    if (floor(dblValue) == 0)
    {
        fAvoidVBZero = true; 
        dblValue += 10; 
    }

    

    if (lSeconds >= c_secondsInDay)
    {
        // we need to add a day
        lSeconds -= c_secondsInDay;
        dblValue = floor(dblValue) + dateToAdd;
    }
    else if (lSeconds < 0)
    {
        // we need to substract a day
        lSeconds += c_secondsInDay;            
        dblValue = floor(dblValue) - dateToAdd;
    }
    else
    {
        dblValue = floor(dblValue);
    }

    
    dblValue += ( ((double) lSeconds) / c_secondsInDay); 

    if (fAvoidVBHistory)
    {
        // do the timewarp again.... go back in history
        dblValue *= -1; 
    }

    if (fAvoidVBZero)
    {
        // now this is tricky. We now have the correct time, but because (-0.9999 to 0.9999 is all the same day)
        // we might do the wrong calculation here. 
        if (dblValue - 10 > 0)
        {
    	    dblValue -= 10; 
        }
        else
        {
            // we would now do something like 9.8 - 10 = - 0.2 which is wrong. the result we want is 
            // - 1.8 (weird, isn't it :)).
            dblValue = floor(dblValue) - 10 - (dblValue - floor(dblValue)); 
        }
    }

    *pdblDate= dblValue; 
    return hr; 
    
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  HRESULT VariantTimeToUTCTime(DOUBLE dblVariantTime, DOUBLE *pdblUTCTime)
//
//  parameters:
//      takes a time in ole format and converts it to UTC using the current timezone information
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT VariantTimeToUTCTime(DOUBLE dblVariantTime, DOUBLE *pdblUTCTime)
{
    HRESULT hr=S_OK;
    long lSeconds;
   	TIME_ZONE_INFORMATION timeZone;
   	
    *pdblUTCTime = 0;
    CHK_BOOL(GetTimeZoneInformation(&timeZone) != TIME_ZONE_ID_INVALID, E_FAIL);

    if (timeZone.DaylightDate.wMonth)
    {
        // this timezone handles daylight saving time
        SYSTEMTIME tVariantDate;
        SYSTEMTIME tStartDate;
        SYSTEMTIME tEndDate;
        
        CHK_BOOL(::VariantTimeToSystemTime(dblVariantTime, &tVariantDate), E_FAIL);
        // calculate the start of daylight savings times
        CHK(DateToFixedDate(tVariantDate.wYear, &(timeZone.DaylightDate), &tStartDate));
        // calculate the end of daylight savings time
        CHK(DateToFixedDate(tVariantDate.wYear, &(timeZone.StandardDate), &tEndDate));

        // is the date passed in between the start and end date
        if (IsBetweenDates(&tVariantDate, &tStartDate, &tEndDate))
        {
            // adjust for daylight saving time
            lSeconds = (timeZone.Bias + timeZone.StandardBias + timeZone.DaylightBias) * 60; 
        }
        else
            lSeconds = (timeZone.Bias + timeZone.StandardBias) * 60;  
        
    }
    else
    {
        // no information about daylight savings time specified
        lSeconds = (timeZone.Bias * 60);  
    }

    
    *pdblUTCTime = dblVariantTime;
    CHK(adjustTime(pdblUTCTime, lSeconds));
    
Cleanup:
    return hr; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  HRESULT UTCTimeToVariantTime(DOUBLE dblVariantTime, DOUBLE *pdblUTCTime)
//
//  parameters:
//      takes a time in UTC format and converts it to local time using the current timezone information
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT UTCTimeToVariantTime(DOUBLE dblUTCTime, DOUBLE *pdblVariantTime)
{
    HRESULT hr=S_OK;
    long lSeconds;
   	TIME_ZONE_INFORMATION timeZone;
   	
    *pdblVariantTime = 0;
    CHK_BOOL(GetTimeZoneInformation(&timeZone) != TIME_ZONE_ID_INVALID, E_FAIL);

    if (timeZone.DaylightDate.wMonth)
    {
        SYSTEMTIME tUTCDate;
        SYSTEMTIME tStartDate;
        SYSTEMTIME tEndDate;
        
        CHK_BOOL(::VariantTimeToSystemTime(dblUTCTime, &tUTCDate), E_FAIL);
        CHK(DateToFixedDate(tUTCDate.wYear, &(timeZone.DaylightDate), &tStartDate));
        CHK(DateToFixedDate(tUTCDate.wYear, &(timeZone.StandardDate), &tEndDate));

        //tStartDate and tEndDate are in local time, we have to go to UTC time 
        DOUBLE dblScratch;

        // convert the starttime to UTC
        lSeconds = (timeZone.Bias + timeZone.StandardBias + timeZone.DaylightBias) * 60;   
        CHK_BOOL(::SystemTimeToVariantTime(&tStartDate, &dblScratch), E_FAIL);
        CHK(adjustTime(&dblScratch, lSeconds));
        CHK_BOOL(::VariantTimeToSystemTime(dblScratch, &tStartDate), E_FAIL);

        // convert the endtime to UTC
        lSeconds = (timeZone.Bias + timeZone.StandardBias + timeZone.DaylightBias) * 60; 
        CHK_BOOL(::SystemTimeToVariantTime(&tEndDate, &dblScratch), E_FAIL);
        CHK(adjustTime(&dblScratch, lSeconds));
        CHK_BOOL(::VariantTimeToSystemTime(dblScratch, &tEndDate), E_FAIL);


        if (IsBetweenDates(&tUTCDate, &tStartDate, &tEndDate))
        {
            // adjust for daylight saving time
            lSeconds = (timeZone.Bias + timeZone.StandardBias + timeZone.DaylightBias) * 60; 
        }
        else
            lSeconds = (timeZone.Bias + timeZone.StandardBias) * 60;  
    }
    else
    {
        // no information about daylight savings time specified
        lSeconds = (timeZone.Bias * 60);  
    }
        
    lSeconds *= -1;

    *pdblVariantTime = dblUTCTime;
    CHK(adjustTime(pdblVariantTime, lSeconds));
    
Cleanup:
    return hr; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  BOOL IsBetweenDates(const SYSTEMTIME *pDate, const SYSTEMTIME *pStartDate, const SYSTEMTIME * pEndDate)
//
//  parameters:
//      determines if pDate is in between StartDate and EndDate
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static BOOL IsBetweenDates(const SYSTEMTIME *pDate, const SYSTEMTIME *pStartDate, const SYSTEMTIME * pEndDate)
{
    if (isBefore(pDate, pStartDate))
        return FALSE;
    
    if ( isBefore(pDate, pEndDate))
    {
        // before the change into standard time;
        return TRUE;
    }
    
    return FALSE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  BOOL isBefore(SYSTEMTIME * ptime, SYSTEMTIME * pmark)
//
//  parameters:
//      returns true if ptimes is before pmark
//
//  description:
//          TRUE - before
//          FALSE - otherwise
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static BOOL isBefore(const SYSTEMTIME *pDate, const SYSTEMTIME * pmark)
{
    #define DCOMP(l,r)  { if ((l) < (r)) { return TRUE; } if ((l) > (r)) { return FALSE; } }
        
    // we have a specific date to compare against, we will ignore the year 
    DCOMP(pDate->wYear, pmark->wYear);
    DCOMP(pDate->wMonth, pmark->wMonth);
    DCOMP(pDate->wDay, pmark->wDay);
    DCOMP(pDate->wHour, pmark->wHour);
    DCOMP(pDate->wMinute, pmark->wMinute);
    DCOMP(pDate->wSecond, pmark->wSecond);
    DCOMP(pDate->wMilliseconds, pmark->wMilliseconds);

    return TRUE; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function:  BOOL DateToFixedDate(WORD wYear, SYSTEMTIME * pVariableDate, SYSTEMTIME *pResultDate)
//
//      pVariableDate can describe a variable date (like retrieved from TIME_ZONE_INFORMATION structure
//      for start and end dates of daylight saving time), or can be a fixed date.
//      It calculates a fixed date matching this input in the year wYear.
//
//  parameters:
//      wYear - the year the rollover date is calculated for
//      pVariableDate - describing the variable date
//      pResultDate - the date to use as the 
//  description:
//          S_OK - successfull calculation
//          E_FAIL - otherwise
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT DateToFixedDate(WORD wYear, const SYSTEMTIME * pVariableDate, SYSTEMTIME *pResultDate)
{
    HRESULT hr = S_OK;
    SYSTEMTIME scratch;
    DOUBLE dblDate;
    WORD wWeek;

    CHK_BOOL(pVariableDate && pResultDate, E_FAIL);

    // make scratch point to the first day of the month in pVariableDate
    memcpy(&scratch, pVariableDate, sizeof(SYSTEMTIME));

    // set it to the correct year;
    scratch.wYear = wYear;

    if (pVariableDate->wYear)
    {
        scratch.wDay = 1;
    }
    CHK_BOOL(::SystemTimeToVariantTime(&scratch, &dblDate), E_FAIL);
    CHK_BOOL(::VariantTimeToSystemTime(dblDate, &scratch), E_FAIL);
    if (pVariableDate->wYear)
    {
        // we are done, scratch contains the date in the new year
        memcpy(pResultDate, &scratch, sizeof(SYSTEMTIME));
        goto Cleanup;
    }

    // scratch is now on the first of the month, and wDayOfWeek is set correctly
    // now make the dayofweek in scratch match the value in pVariableDay
    while (pVariableDate->wDayOfWeek != scratch.wDayOfWeek)
    {
        scratch.wDay ++;
        scratch.wDayOfWeek = (scratch.wDayOfWeek + 1) % 7;
    }

    wWeek = 1;
    CHK_BOOL(::SystemTimeToVariantTime(&scratch, &dblDate), E_FAIL);
    // scratch now is set on the first day of the month, which matches pVariableDate->wDayOfWeek
    // dblDate is the double representation of scratch

    while (TRUE)
    {
        SYSTEMTIME work;
        
        if (pVariableDate->wDay == wWeek)
        {
            // we are all set, the correct date is in scratch
            memcpy(pResultDate, &scratch, sizeof(SYSTEMTIME));
            goto Cleanup;
        }

        dblDate += 7;
        wWeek++;
        CHK_BOOL (::VariantTimeToSystemTime(dblDate, &work), E_FAIL);

        if (work.wMonth != scratch.wMonth)
        {
            // Month changed, if we we are looking for the last week (5), we are done 
            if (pVariableDate->wDay == 5)
            {
                // the correct result is in scratch
                memcpy(pResultDate, &scratch, sizeof(SYSTEMTIME));
                goto Cleanup;
            }
            else
            {
                // we switch month without finding the week-count, i.e. the week-count could have
                // been 4 in the month of feb. 
                CHK(E_FAIL);
            }
        }
        memcpy(&scratch, &work, sizeof(SYSTEMTIME));
    }
    
Cleanup:
    return hr; 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// End Of File




