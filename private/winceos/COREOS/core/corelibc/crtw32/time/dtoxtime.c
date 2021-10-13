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
/***
*dtoxtime.c - convert OS local time to __time32_t
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines __loctotime32_t() - convert OS local time to internal format
*       (__time32_t).
*
*Revision History:
*       03-??-84  RLB   written
*       11-18-87  SKS   change tzset() to __tzset(), change source file name
*                       make _dtoxtime a near procedure
*       01-26-88  SKS   _dtoxtime is no longer a near procedure (for QC)
*       03-20-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and
*                       fixed the copyright. Also, cleaned up the formatting
*                       a bit.
*       10-04-90  GJF   New-style function declarator.
*       01-21-91  GJF   ANSI naming.
*       05-19-92  DJM   ifndef for POSIX build.
*       03-30-93  GJF   Revised. Old _dtoxtime is replaced by __gmtotime_t,
*                       which is more useful on Win32.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-06-93  GJF   Rewrote computation to avoid compiler warnings.
*       07-20-93  GJF   Replaced __gmtotime_t with function very similar to
*                       _dostotime_t() in 16-bit C 8.00. The reason for the
*                       change is that only local time values can be trusted
*                       on a Win32 platform. System time may be UTC (as
*                       documented), and is on NT, or may be the same as
*                       local time, as on Win32S and Win32C
*       02-10-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s.
*       09-25-95  GJF   Added DST flag to __loctotime_t's arguments. Also, 
*                       use _dstbias instead of assuming a DST bias of -3600.
*       02-07-98  GJF   Changes for Win64: replaced long type with time_t
*       10-19-98  GJF   Fill in tm_min and tm_sec before calling _isindst
*       05-17-99  PML   Remove all Macintosh support.
*       12-10-99  GB    Added support for years beyond 2099.
*       12-18-02  EAN   changed time_t to use a 64-bit int if available.
*                       VSWhidbey 6851
*       03-06-03  SSM   Assertions and validations
*       09-29-06  WL    Changed to return -1 for out-of-ranged inputs
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <time.h>
#include <ctime.h>
#include <internal.h>

/***
*__time32_t __loctotime32_t(yr, mo, dy, hr, mn, sc, dstflag) - converts OS local
*       time to internal time format (i.e., a __time32_t value)
*
*Purpose:
*       Converts a local time value, obtained in a broken down format from
*       the host OS, to __time32_t format (i.e., the number elapsed seconds since
*       01-01-70, 00:00:00, UTC).
*
*Entry:
*       int yr, mo, dy -    date
*       int hr, mn, sc -    time
*       int dstflag    -    1 if Daylight Time, 0 if Standard Time, -1 if
*                           not specified.
*
*Exit:
*       Returns calendar time value.
*
*Exceptions:
*
*******************************************************************************/

__time32_t __cdecl __loctotime32_t (
        int yr,         /* 0 based */
        int mo,         /* 1 based */
        int dy,         /* 1 based */
        int hr,
        int mn,
        int sc,
        int dstflag )
{
        int tmpdays;
        __time32_t tmptim;
        struct tm tb;
        int daylight = 0;
        long dstbias = 0;
        long timezone = 0;

        /*
         * Do a range check on the year and convert it to a delta
         * off of 1900.
         */

        _VALIDATE_RETURN_NOEXC( 
            ( ( ( yr - 1900 ) >= _BASE_YEAR ) && ( ( yr - 1900 ) <= _MAX_YEAR ) ),
            EINVAL, 
            ( ( __time32_t )( -1 ) ) 
        )
        yr -= 1900;

        _VALIDATE_RETURN_NOEXC( 
            ( ( mo >= 1 ) && ( mo <= 12 ) ),
            EINVAL, 
            ( ( __time32_t )( -1 ) ) 
        )
        _VALIDATE_RETURN_NOEXC( 
            ( ( hr >= 0 ) && ( hr <= 23 ) ),
            EINVAL, 
            ( ( __time32_t )( -1 ) ) 
        )
        _VALIDATE_RETURN_NOEXC( 
            ( ( mn >= 0 ) && ( mn <= 59 ) ),
            EINVAL, 
            ( ( __time32_t )( -1 ) ) 
        )
        _VALIDATE_RETURN_NOEXC( 
            ( ( sc >= 0 ) && ( sc <= 59 ) ),
            EINVAL, 
            ( ( __time32_t )( -1 ) ) 
        )
        _VALIDATE_RETURN_NOEXC( 
            ( ( dy >= 1 ) && ( 
                (
                    (_days[mo] - _days[mo - 1]) >= dy) ||		// Make sure that the # of days is in valid range for the month
                    (_IS_LEAP_YEAR(yr) && mo == 2 && dy <= 29)	// Special case for Feb in a leap year
                )
            ),
            EINVAL, 
            ( ( __time32_t )( -1 ) ) 
        )

        /*
         * Compute the number of elapsed days in the current year. Note the
         * test for a leap year would fail in the year 2100, if this was in
         * range (which it isn't).
         */
        tmpdays = dy + _days[mo - 1];
        if ( _IS_LEAP_YEAR(yr) && (mo > 2) )
            tmpdays++;

        /*
         * Compute the number of elapsed seconds since the Epoch. Note the
         * computation of elapsed leap years would break down after 2100
         * if such values were in range (fortunately, they aren't).
         */
        tmptim = /* 365 days for each year */
                 (((__time32_t)yr - _BASE_YEAR) * 365

                 /* one day for each elapsed leap year */
                 + (__time32_t)_ELAPSED_LEAP_YEARS(yr)

                 /* number of elapsed days in yr */
                 + tmpdays)

                 /* convert to hours and add in hr */
                 * 24 + hr;

        tmptim = /* convert to minutes and add in mn */
                 (tmptim * 60 + mn)

                 /* convert to seconds and add in sec */
                 * 60 + sc;

        /*
         * Account for time zone.
         */
        __tzset();

        _ERRCHECK(_get_daylight(&daylight));
        _ERRCHECK(_get_dstbias(&dstbias));
        _ERRCHECK(_get_timezone(&timezone));

        tmptim += timezone;

        /*
         * Fill in enough fields of tb for _isindst(), then call it to
         * determine DST.
         */
        tb.tm_yday = tmpdays;
        tb.tm_year = yr;
        tb.tm_mon  = mo - 1;
        tb.tm_hour = hr;
        tb.tm_min  = mn;
        tb.tm_sec  = sc;
        if ( (dstflag == 1) || ((dstflag == -1) && daylight && 
                                _isindst(&tb)) )
            tmptim += dstbias;
        return(tmptim);
}


#if 0

/*
 * THE FOLLOWING FUNCTION WAS DEFINED AND USED (IN PLACE OF THE ONE ABOVE)
 * FOR THE CUDA PRODUCT AND THE NT 1.0 SDK. IT WAS REPLACED (BY THE ONE
 * ABOVE) BECAUSE NON-NT WIN32 PLATFORMS MAY USE LOCAL TIME FOR SYSTEM TIME,
 * RATHER THAN UTC.
 */

/***
*time_t __gmtotime_t(yr, mo, dy, hr, mn, sc) - convert broken down time (UTC)
*   to time_t
*
*Purpose:
*       Converts a broken down UTC (GMT) time to time_t. This is similar to
*       _mkgmtime() except there is minimal overflow checking and no updating
*       of the input values (i.e., the fields of tm structure).
*
*Entry:
*       int yr, mo, dy -        date
*       int hr, mn, sc -        time
*
*Exit:
*       returns time_t value
*
*Exceptions:
*
*******************************************************************************/

time_t __cdecl __gmtotime_t (
        int yr,     /* 0 based */
        int mo,     /* 1 based */
        int dy,     /* 1 based */
        int hr,
        int mn,
        int sc
        )
{
        int tmpdays;
        long tmptim;

        /*
         * Do a quick range check on the year and convert it to a delta
         * off of 1900.
         */
        if ( ((long)(yr -= 1900) < _BASE_YEAR) || ((long)yr > _MAX_YEAR) )
                return (time_t)(-1);

        /*
         * Compute the number of elapsed days in the current year minus
         * one. Note the test for leap year and the would fail in the year 2100
         * if this was in range (which it isn't).
         */
        tmpdays = dy + _days[mo - 1];
        if ( !(yr & 3) && (mo > 2) )
                /*
                 * in a leap year, after Feb. add one day for elapsed
                 * Feb 29.
                 */
                tmpdays++;

        /*
         * Compute the number of elapsed seconds since the Epoch. Note the
         * computation of elapsed leap years would break down after 2100
         * if such values were in range (fortunately, they aren't).
         */
        tmptim = /* 365 days for each year */
                 (((long)yr - _BASE_YEAR) * 365L

                 /* one day for each elapsed leap year */
                 + (long)((yr - 1) >> 2) - _LEAP_YEAR_ADJUST

                 /* number of elapsed days in yr */
                 + tmpdays)

                 /* convert to hours and add in hr */
                 * 24L + hr;

        tmptim = /* convert to minutes and add in mn */
                 (tmptim * 60L + mn)

                 /* convert to seconds and add in sec */
                 * 60L + sc;

        return (tmptim >= 0) ? (time_t)tmptim : (time_t)(-1);
}

#endif

#endif  /* _POSIX_ */
